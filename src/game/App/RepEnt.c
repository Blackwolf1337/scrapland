/*
 * The replication v2 on the instance (stage 3.2, -mp2), see include/scrap/repent.h: the slots of every entity, the
 * class block, the entity variable journal and its messages; the declared state and the test verb "mirror" of both
 * sides. Not in the original.
 *
 * A slot belongs to an entity from its first replicated variable set or its first snapshot to its destruction
 * (cEntity.netSlot holds slot + 1; the destructor frees it, so no later entity at the same address takes its journal).
 * A freed slot is taken again only when no client holds it any more (NetRepSrv_SlotFree).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/ai.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/itementity.h"
#include "scrap/model.h"
#include "scrap/netaudit.h"
#include "scrap/netclass.h"
#include "scrap/netrep.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/pres.h"
#include "scrap/repent.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

#include "../Global/testchannel.h"

#define RE_CONNS 1024				/* NET2_MAX_CONNS */
#define RE_JOURNAL 65536			/* journal entries of all entities */
#define RE_EVENTS 1024				/* events between two snapshots */
#define RE_MSG 8192					/* a reliable message is sent when it is this long */
#define RE_REC (NET2_MAX_MESSAGE - 16)	/* the longest record */
#define RE_RESYNC 32				/* entities whose journal numbers are read back per snapshot */

byte g_Mp2;

typedef struct ReRec {
	cEntity *e;						/* NULL: free */
	uint32_t gen;
	int look;						/* the look id of the last snapshot, -1 */
	byte dirty;						/* journal entries changed since the last flush */
} ReRec;

typedef struct ReEvent {
	uint16_t slot;
	uint32_t gen;
	NetRepJVal v;
} ReEvent;

static struct {
	ReRec rec[NETREP_MAX_ENTS];
	int cursor;
	uint32_t gen;
	NetRepJournal *j;
	NetRepServer *srv;				/* of the last snapshot */
	uint16_t dirty[NETREP_MAX_ENTS];
	int ndirty;
	ReEvent ev[RE_EVENTS];
	int nev;
	uint32_t *jsent[RE_CONNS];		/* per connection and slot: the generation whose create message it got */
	uint32_t lostSlots, lostLooks, lostEvents, longNames, cutTexts, bigRecords;
	uint32_t creates, updates, events;	/* records sent */
	uint64_t bytes;
	uint8_t msg[RE_MSG + RE_REC + 16];
	uint8_t rec1[RE_REC];
} re;

/* the tap of the inline Entity_Var functions (include/scrap/entity.h) */
void (*g_EntVarTap)(cEntity *e, const char *name, int type, const void *value);

static void RepEnt_Tap(cEntity *e, const char *name, int type, const void *value);
static byte RepEnt_ReadBack(cEntity *e, const NetRepJVal *j, NetRepJVal *out);

byte RepEnt_ParseParam(const char *param, const char *ext) {
	(void)ext;
	if (crt_stricmp("mp2", param)) {
		return 0;
	}
	g_Mp2 = 1;
	Pres_Init();
	if (g_Instance && !re.j) {
		re.j = NetRepJ_Create(NETREP_MAX_ENTS, RE_JOURNAL);
		g_EntVarTap = RepEnt_Tap;
		for (int s = 0; s < NETREP_MAX_ENTS; s++) {
			re.rec[s].look = -1;
		}
	}
	return 1;
}

/* ---- the declared state (both sides) ---- */

static byte RepEnt_Camera(cEntity *e) {
	const char *cls = Entity_GetClassName(e);
	return !strcmp(cls, TYEN_FCAM) || !strcmp(cls, TYEN_CAM) || !strcmp(cls, TYEN_CAMPATH);
}

int RepEnt_Kind(cEntity *e) {
	if (e->deadNode.target || !e->name || RepEnt_Camera(e)) {
		return 0;
	}
	if (Entity_IsType(e, TYEN_CAR)) {
		return NETREP_SHIP;
	}
	if (Entity_IsType(e, TYEN_WALKCHAR)) {
		return NETREP_CHAR;
	}
	if (Entity_IsType(e, TYEN_MISSILE)) {
		return NETREP_MISSILE;
	}
	if (Entity_IsType(e, "item")) {
		return NETREP_ITEM;
	}
	if (Entity_IsType(e, TYEN_WITH_LIFE)) {
		return REPENT_LIFE;
	}
	return Entity_IsType(e, TYEN_OBJECT) ? REPENT_OBJECT : REPENT_BASE;
}

static int32_t RepEnt_Ms(float t) {
	float v = t * 1000.0f;
	return !(v > -2.0e9f) ? -2000000000 : v > 2.0e9f ? 2000000000 : (int32_t)lrintf(v);
}

/* the four layers of an action controller */
static void RepEnt_Layers(const cActCtrl *ctrl, NetRepState *s) {
	static const int at[2][2] = { { REPX_ACT2, REPX_ACT2T }, { REPX_ACT3, REPX_ACT3T } };
	for (int l = 0; l < 4; l++) {
		const cActLayer *ly = &ctrl->layers[l];
		uint16_t a = ly->on && ly->action ? Rep_ActIndex(ctrl->cls, ly->action) : 0;
		uint32_t t = a ? (uint32_t)(ly->time > 0.0f ? RepEnt_Ms(ly->time) : 0) : 0;
		if (l < NETREP_LAYERS) {
			s->act[l] = a;
			s->actTime[l] = t;
		} else {
			s->x[at[l - 2][0]] = a;
			s->x[at[l - 2][1]] = (int32_t)t;
		}
	}
	s->x[REPX_WALK] = RepEnt_Ms(ctrl->walkSpeed);
}

void RepEnt_State(cEntity *e, int kind, NetRepState *s) {
	if (kind >= NETREP_SHIP && kind <= NETREP_ITEM) {
		Rep_StateOf(e, kind, s);
	} else {
		memset(s, 0, sizeof(*s));
		s->pos[0] = NetRep_QPos(e->pos.x);
		s->pos[1] = NetRep_QPos(e->pos.y);
		s->pos[2] = NetRep_QPos(e->pos.z);
		s->flags = e->active ? NETREP_FL_ACTIVE : 0;
		if (kind == REPENT_BASE) {
			return;
		}
		cObjEntity *o = Obj(e);
		for (int k = 0; k < 3; k++) {
			s->vel[k] = NetRep_QVel((&o->vel.x)[k]);
			s->ang[k] = NetRep_QAng((&o->ang.x)[k]);
		}
		float dt = Rep_SampleDtOf(e) * 1000.0f;
		s->dt = (uint16_t)(dt > 65535.0f ? 65535.0f : dt + 0.5f);
		if (kind == REPENT_LIFE) {
			int life = WLife(e)->life;
			s->life = (int16_t)(life > 32767 ? 32767 : life < -32768 ? -32768 : life);
		}
	}
	if (kind == REPENT_BASE) {
		return;
	}
	cObjEntity *o = Obj(e);
	s->x[REPX_RADAR] = o->radarShow;
	if (CGameModel_IsLoaded(&o->model)) {
		uint32_t p = 0;
		s->x[REPX_FX] = (int32_t)CGameModel_GetEffect(&o->model, NULL);
		CGameModel_GetEffectParam(&o->model, 0, &p);
		s->x[REPX_FXP] = (int32_t)p;
	}
	s->x[REPX_MOVE] = (o->move.active ? 1 << 30 : 0) | (RepEnt_Ms(o->move.time > 0.0f ? o->move.time : 0.0f) &
		0x3fffffff);
	s->x[REPX_MOVET] = RepEnt_Ms(o->move.moveTime);
	if (kind == NETREP_ITEM) {
		const cItemEntity *it = (const cItemEntity *)e;
		s->x[REPX_ITEM] = it->respawn > 0.0f ? RepEnt_Ms(it->respawn) : 0;
		s->x[REPX_BLINK] = it->blink;
		return;
	}
	if (kind == REPENT_OBJECT) {
		return;
	}
	cWithLifeEntity *w = WLife(e);
	if (w->actCtrl && w->actCtrl->cls) {
		RepEnt_Layers(w->actCtrl, s);
	}
	if (kind == NETREP_CHAR) {
		s->x[REPX_HEAD] = WChar(e)->viewLock;
	}
	s->x[REPX_PREVLIFE] = w->prevLife;
	s->x[REPX_BURN] = RepEnt_Ms(w->fireRemain > 0.0f ? w->fireRemain : 0.0f);
	s->x[REPX_ARROW] = (w->arrow.canUse ? 1 : 0) | (w->arrow.canView ? 2 : 0) | (w->arrow.canMission ? 4 : 0) |
		((w->arrow.coneStatus & 3) << 3) | (w->arrow.tgType << 8);
	if (kind == NETREP_SHIP) {
		cVehicleEntity *v = Veh(e);
		s->x[REPX_MAXLIFE] = v->maxLife;
		s->x[REPX_BOOST] = RepEnt_Ms(v->boost);
		s->x[REPX_CLOUD] = (RepEnt_Ms(v->cloud) & 0x3fffffff) | (v->cloudOn ? 1 << 30 : 0);
		s->x[REPX_EMI] = RepEnt_Ms(v->emi);
		s->x[REPX_FIREW] = v->controls.fire && v->weapons.curWeap >= 0 && v->weapons.curWeap < 30 ?
			1 << v->weapons.curWeap : 0;
		s->x[REPX_WEAPSND] = v->weapons.weapSnd + 1;
		/* the Hook firing (its sound and the effect of a shot at nothing are the replica's, Hook_NetPresent; a
		 * replica keeps the bit in its countermeasure key, which only the Advance half reads) */
		if (g_Instance ? Hook_Firing(e) : v->controls.cm) {
			s->x[REPX_FIREW] |= 1 << 30;
		}
		/* the weapons' lock (the hook plug of HookPlugFunc): its effect and sound are the replica's */
		if (v->weapons.on) {
			s->x[REPX_FIREW] = (int32_t)((uint32_t)s->x[REPX_FIREW] | 0x80000000u);
		}
	} else if (kind == NETREP_MISSILE) {
		cMissileEntity *m = Missile(e);
		cEntity *t = m->target[0] ? World_FindEntity(m->target) : NULL;
		s->x[REPX_TARGET] = t ? (g_Instance ? RepEnt_Slot(t) : ReplicaEnt_SlotOf(t)) + 1 : 0;
		s->x[REPX_LEVEL] = m->level;
	}
}

/* ---- the slots (instance) ---- */

int RepEnt_Slot(const cEntity *e) {
	return e && e->netSlot ? e->netSlot - 1 : -1;
}

/* the entity's slot, a new one for an entity without */
static int RepEnt_SlotFor(cEntity *e) {
	if (e->netSlot) {
		return e->netSlot - 1;
	}
	for (int k = 0; k < NETREP_MAX_ENTS; k++) {
		int s = (re.cursor + k) % NETREP_MAX_ENTS;
		if (re.rec[s].e || (re.srv && !NetRepSrv_SlotFree(re.srv, s))) {
			continue;
		}
		re.cursor = (s + 1) % NETREP_MAX_ENTS;
		re.rec[s].e = e;
		re.rec[s].gen = ++re.gen;
		re.rec[s].look = -1;
		re.rec[s].dirty = 0;
		e->netSlot = (uint16_t)(s + 1);
		return s;
	}
	re.lostSlots++;
	return -1;
}

static void RepEnt_Free(int s) {
	NetRepJ_Clear(re.j, s);
	re.rec[s].e->netSlot = 0;
	re.rec[s].e = NULL;
	re.rec[s].dirty = 0;
}

/* the destructor of an entity with a slot (Entity.c) */
void RepEnt_Gone(cEntity *e) {
	int s = RepEnt_Slot(e);
	if (s >= 0 && re.rec[s].e == e) {
		RepEnt_Free(s);
	}
}

static void RepEnt_MarkDirty(int s) {
	if (!re.rec[s].dirty) {
		re.rec[s].dirty = 1;
		re.dirty[re.ndirty++] = (uint16_t)s;
	}
}

/* the variables the base state or the class block carry every snapshot, and the identity: not journaled */
static byte RepEnt_Carried(const char *name) {
	static const char *const names[] = { "Pos", "Ang", "Vel", "Life", "Active", "Name", "UsrControl", "isFalling",
		"isWalking", "SpeedKmH", "ViewAng", "ViewPos" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (!strcmp(name, names[i])) {
			return 1;
		}
	}
	return 0;
}

/* the events a replica gets from its state already (its actions, its moves, its forces): not sent; ForceDefLoop is
 * journaled (RepEnt_Tap) */
static byte RepEnt_StateEvent(const char *name) {
	static const char *const names[] = { "Action", "ForceAction", "StopSecondary", "MoveTime",
		"MovePos", "MoveAng", "MoveViewAng", "MoveFade", "ForceVel", "ForceAll", "ActionPress", "UsePress" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (!strcmp(name, names[i])) {
			return 1;
		}
	}
	return 0;
}

static void RepEnt_Value(NetRepJVal *v, const char *name, int type, const void *value) {
	memset(v, 0, sizeof(*v));
	Str_Copy(v->name, name, sizeof(v->name));
	switch (type) {
	case 'i':
		v->type = NETREP_JI;
		v->u.i = *(const int *)value;
		break;
	case 'f':
		v->type = NETREP_JF;
		v->u.f = *(const double *)value;
		break;
	case 'v':
		v->type = NETREP_JV;
		memcpy(v->u.v, value, sizeof(v->u.v));
		break;
	default: {
		const char *s = *(char *const *)value;
		v->type = NETREP_JS;
		if (s && strlen(s) >= sizeof(v->u.s)) {
			re.cutTexts++;
		}
		Str_Copy(v->u.s, s ? s : "", sizeof(v->u.s));
		break;
	}
	}
}

static void RepEnt_Tap(cEntity *e, const char *name, int type, const void *value) {
	NetRepJVal v;
	if (!g_Instance || e->deadNode.target || RepEnt_Camera(e)) {
		return;
	}
	/* a player's entity the scripts move or start or end a scripted move of: its client takes its next rollback state
	 * as the hand-over of a flow (include/scrap/play.h) */
	if ((!strcmp(name, "Pos") || !strcmp(name, "Ang") || !strcmp(name, "ViewAng") || !strcmp(name, "MoveTime")) &&
		Entity_IsType(e, TYEN_WITH_LIFE) && Obj(e)->netStep) {
		Rep_OwnMoved(e);
	}
	netTag_t tag = NetClass_Find(NCT_ENTVAR, name);
	if (tag != NC_STATE && tag != NC_EVENT) {
		if (tag == NC_ABSENT || tag == NC_UNTAGGED) {
			NetAudit_Name(NA_CREATE, "repent.untagged:", name);
		}
		return;
	}
	if (strlen(name) >= NETREP_JNAME) {
		re.longNames++;
		return;
	}
	if (tag == NC_STATE ? RepEnt_Carried(name) : RepEnt_StateEvent(name)) {
		return;
	}
	/* the default loop a script forces is state the actions do not carry (a replica's next loop, the DefLoop it reads
	 * back): kept in the journal like a set */
	if (!strcmp(name, "ForceDefLoop")) {
		tag = NC_STATE;
	}
	int s = RepEnt_SlotFor(e);
	if (s < 0) {
		return;
	}
	RepEnt_Value(&v, name, type, value);
	/* the variables whose sets add up (an arrow option, a pilot animation): each its own entry, by the option's
	 * number or the animation (the client applies the name before '#') */
	if (!strcmp(name, "AddArrOpt") && Entity_IsType(e, TYEN_WITH_LIFE)) {
		Str_PrintfQuiet(v.name, sizeof(v.name), "AddArrOpt#%d", WLife(e)->arrow.nOpt);
	} else if (!strcmp(name, "AnmP0") && type == 'i') {
		Str_PrintfQuiet(v.name, sizeof(v.name), "AnmP0#%d", v.u.i);
	}
	if (tag == NC_STATE && v.type != NETREP_JS && strcmp(name, "ArrnOpt")) {
		/* a number that reads back as it was set: the journal follows its later values (RepEnt_Resync) */
		NetRepJVal r;
		v.mark = RepEnt_ReadBack(e, &v, &r) && NetRepJ_Equal(&v, &r);
	}
	if (tag == NC_EVENT) {
		if (re.nev == RE_EVENTS) {
			re.lostEvents++;
			return;
		}
		ReEvent *x = &re.ev[re.nev++];
		x->slot = (uint16_t)s;
		x->gen = re.rec[s].gen;
		x->v = v;
		return;
	}
	if (NetRepJ_Set(re.j, s, &v) > 0) {
		RepEnt_MarkDirty(s);
	}
}

void RepEnt_Note(cEntity *e, const char *name, const char *text) {
	NetRepJVal v;
	if (!re.j || !e || e->deadNode.target || RepEnt_Camera(e)) {
		return;
	}
	int s = RepEnt_SlotFor(e);
	if (s < 0) {
		return;
	}
	RepEnt_Value(&v, name, 's', &text);
	if (NetRepJ_Set(re.j, s, &v) > 0) {
		RepEnt_MarkDirty(s);
	}
}

/* the look string of an entity of a v2 kind (O|model|action class) */
static void RepEnt_Look(cEntity *e, int kind, char *out, int size) {
	if (kind >= NETREP_SHIP && kind <= NETREP_ITEM) {
		Rep_LookOf(e, kind, out, size);
		return;
	}
	const char *model = "", *act = "";
	if (kind != REPENT_BASE) {
		CGameModel *m = &Obj(e)->model;
		model = CGameModel_IsLoaded(m) && m->Model->scene && m->Model->scene->name ? m->Model->scene->name : "";
	}
	if (kind == REPENT_LIFE && WLife(e)->actCtrl && WLife(e)->actCtrl->cls) {
		act = WLife(e)->actCtrl->cls->name;
	}
	Str_PrintfQuiet(out, size, "O|%s|%s", model, act);
}

/* the numbers of the journals that the simulation changed without a set (a character's money given away): the values
 * the entities read back now, RE_RESYNC entities per snapshot in turn; only the numbers that read back as they were
 * set (NetRepJVal.mark; not ArrnOpt, which only takes 0, nor texts: a model's path reads back as its name) */
static void RepEnt_Resync(void) {
	static int cursor;
	NetRepJVal r;
	for (int n = 0, k = 0; n < RE_RESYNC && k < NETREP_MAX_ENTS; k++) {
		int s = cursor;
		cursor = (cursor + 1) % NETREP_MAX_ENTS;
		cEntity *e = re.rec[s].e;
		if (!e || e->deadNode.target) {
			continue;
		}
		n++;
		for (int i = NetRepJ_First(re.j, s); i >= 0;) {
			const NetRepJVal *v = NetRepJ_Get(re.j, i);
			int next = NetRepJ_Next(re.j, i);
			if (v->mark && RepEnt_ReadBack(e, v, &r) && !NetRepJ_Equal(v, &r) && NetRepJ_Set(re.j, s, &r) > 0) {
				RepEnt_MarkDirty(s);
			}
			i = next;
		}
	}
}

/* the journal of slot from to slot to */
static void RepEnt_Move(int from, int to) {
	for (int k = NetRepJ_First(re.j, from); k >= 0; k = NetRepJ_Next(re.j, k)) {
		NetRepJ_Set(re.j, to, NetRepJ_Get(re.j, k));
	}
}

void RepEnt_Gather(NetRepServer *srv, uint32_t tick) {
	char look[NETREP_LOOK_MAX + 1];
	NetRepState st;

	re.srv = srv;
	RepEnt_Resync();
	NetRepSrv_Begin(srv, tick);
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		int kind = RepEnt_Kind(e);
		if (!kind) {
			continue;
		}
		int s = RepEnt_SlotFor(e);
		if (s < 0) {
			continue;
		}
		RepEnt_Look(e, kind, look, sizeof(look));
		int id = NetRepSrv_Look(srv, look);
		if (id < 0) {
			re.lostLooks++;
			continue;
		}
		if (re.rec[s].look >= 0 && re.rec[s].look != id) {
			/* built otherwise now (another model, action class): a new entity for the clients, with its journal */
			e->netSlot = 0;
			int n = RepEnt_SlotFor(e);
			if (n < 0) {
				e->netSlot = (uint16_t)(s + 1);
				continue;
			}
			RepEnt_Move(s, n);
			/* the create message of the new slot carries all of it: no change to send */
			NetRepJ_Clean(re.j, n);
			NetRepJ_Clear(re.j, s);
			re.rec[s].e = NULL;
			re.rec[s].dirty = 0;
			s = n;
		}
		re.rec[s].look = id;
		RepEnt_State(e, kind, &st);
		st.x[REPX_GEN] = (int32_t)re.rec[s].gen;
		/* the level scope: the things of the level, and on an indoor map its ships too (the few traffic ships
		 * behind the windows, seen from farther than the interest radius) */
		int flags = kind >= NETREP_ITEM || (kind == NETREP_SHIP && !g_IsMapOutDoor) ? NETREP_SET_LEVEL : 0;
		/* a thing without life of a player's activity (a race's checkpoint, count down, camera) is its alone */
		int owner = Entity_IsType(e, TYEN_WITH_LIFE) ? -1 : Players_OfOwned(e);
		if (owner >= 0) {
			flags |= NETREP_SET_ONLY(owner + 1);
		}
		NetRepSrv_SetEx(srv, s, kind, id, &st, flags);
	}
	NetRepSrv_End(srv);
}

/* ---- the messages (instance) ---- */

static uint32_t *RepEnt_Sent0(int conn) {
	if (conn < 0 || conn >= RE_CONNS) {
		return NULL;
	}
	if (!re.jsent[conn]) {
		re.jsent[conn] = calloc(NETREP_MAX_ENTS, sizeof(uint32_t));
	}
	return re.jsent[conn];
}

void RepEnt_Drop(int conn) {
	if (conn >= 0 && conn < RE_CONNS) {
		free(re.jsent[conn]);
		re.jsent[conn] = NULL;
	}
}

/* a message of records: started with the kind, sent when long */
typedef struct ReMsg {
	Net2Host *host;
	int conn;
	int kind;
	int size;
} ReMsg;

static void RepEnt_MsgFlush(ReMsg *m) {
	if (m->size > 2) {
		Net2_Send(m->host, m->conn, REP_CH_RELIABLE, re.msg, m->size);
		re.bytes += (uint64_t)m->size;
	}
	re.msg[0] = REPENT_MSG;
	re.msg[1] = (uint8_t)m->kind;
	m->size = 2;
}

static void RepEnt_MsgAdd(ReMsg *m, const uint8_t *rec, int n) {
	if (m->size + n > RE_MSG) {
		RepEnt_MsgFlush(m);
	}
	memcpy(re.msg + m->size, rec, (size_t)n);
	m->size += n;
}

static int RepEnt_JournalRec(int s, byte create, byte dirtyOnly) {
	NetRepBuf w;
	NetRepBuf_Init(&w, re.rec1, sizeof(re.rec1));
	NetRep_PutVar(&w, (uint32_t)s);
	NetRep_PutVar(&w, re.rec[s].gen);
	if (create) {
		NetRep_PutStr(&w, Entity_GetClassName(re.rec[s].e));
		NetRep_PutStr(&w, re.rec[s].e->name);
	}
	int n = 0;
	for (int k = NetRepJ_First(re.j, s); k >= 0; k = NetRepJ_Next(re.j, k)) {
		n += !dirtyOnly || NetRepJ_IsDirty(re.j, k);
	}
	NetRep_PutVar(&w, (uint32_t)n);
	for (int k = NetRepJ_First(re.j, s); k >= 0; k = NetRepJ_Next(re.j, k)) {
		if (!dirtyOnly || NetRepJ_IsDirty(re.j, k)) {
			NetRep_PutJVal(&w, NetRepJ_Get(re.j, k));
		}
	}
	if (w.bad) {
		re.bigRecords++;
		return 0;
	}
	return w.size;
}

void RepEnt_Flush(Net2Host *host, int conns) {
	ReMsg m = { host, 0, REPENT_K_VARS, 0 };

	if (!re.j) {
		return;
	}
	for (int c = 0; c < conns && c < RE_CONNS; c++) {
		uint32_t *sent = re.jsent[c];
		if (!sent) {
			continue;
		}
		m.conn = c;
		m.kind = REPENT_K_VARS;
		RepEnt_MsgFlush(&m);
		for (int i = 0; i < re.ndirty; i++) {
			int s = re.dirty[i];
			if (!re.rec[s].e || !re.rec[s].dirty || sent[s] != re.rec[s].gen) {
				continue;
			}
			int n = RepEnt_JournalRec(s, 0, 1);
			if (n) {
				RepEnt_MsgAdd(&m, re.rec1, n);
				re.updates++;
			}
		}
		RepEnt_MsgFlush(&m);
		m.kind = REPENT_K_EVENT;
		RepEnt_MsgFlush(&m);
		for (int i = 0; i < re.nev; i++) {
			const ReEvent *x = &re.ev[i];
			if (sent[x->slot] != x->gen) {
				continue;
			}
			NetRepBuf w;
			NetRepBuf_Init(&w, re.rec1, sizeof(re.rec1));
			NetRep_PutVar(&w, x->slot);
			NetRep_PutVar(&w, x->gen);
			NetRep_PutJVal(&w, &x->v);
			RepEnt_MsgAdd(&m, re.rec1, w.size);
			re.events++;
		}
		RepEnt_MsgFlush(&m);
	}
	for (int i = 0; i < re.ndirty; i++) {
		int s = re.dirty[i];
		if (re.rec[s].dirty) {
			NetRepJ_Clean(re.j, s);
			re.rec[s].dirty = 0;
		}
	}
	re.ndirty = 0;
	re.nev = 0;
}

void RepEnt_Sent(Net2Host *host, int conn, NetRepClient *c) {
	const uint16_t *slots;
	ReMsg m = { host, conn, REPENT_K_CREATE, 0 };

	int n = NetRepSrv_Created(c, &slots);
	uint32_t *sent = n ? RepEnt_Sent0(conn) : NULL;
	if (!sent) {
		return;
	}
	RepEnt_MsgFlush(&m);
	for (int i = 0; i < n; i++) {
		int s = slots[i];
		if (!re.rec[s].e || sent[s] == re.rec[s].gen) {
			continue;
		}
		int size = RepEnt_JournalRec(s, 1, 0);
		if (size) {
			RepEnt_MsgAdd(&m, re.rec1, size);
			sent[s] = re.rec[s].gen;
			re.creates++;
		}
	}
	RepEnt_MsgFlush(&m);
}

void RepEnt_Status(void) {
	if (!re.j) {
		return;
	}
	Con_Printf("repent: %d journal entries (%u lost), %u creates %u updates %u events sent, %llu bytes, %u slots %u looks "
		"%u events lost, %u long names, %u cut texts, %u big records\n", NetRepJ_Used(re.j), NetRepJ_Lost(re.j),
		re.creates, re.updates, re.events, (unsigned long long)re.bytes, re.lostSlots, re.lostLooks, re.lostEvents,
		re.longNames, re.cutTexts, re.bigRecords);
}

/* ---- the mirror (both sides) ---- */

static void RepEnt_JsonStr(tcBuf *b, const char *s) {
	TC_Add(b, "\"", 1);
	for (; *s; s++) {
		uint8_t c = (uint8_t)*s;
		if (c == '"' || c == '\\') {
			TC_Addf(b, "\\%c", c);
		} else if (c < 0x20 || c > 0x7e) {
			TC_Addf(b, "\\u%04x", c);
		} else {
			TC_Add(b, s, 1);
		}
	}
	TC_Add(b, "\"", 1);
}

static void RepEnt_JsonVal(tcBuf *b, const NetRepJVal *v) {
	switch (v->type) {
	case NETREP_JI:
		TC_Addf(b, "%d", v->u.i);
		break;
	case NETREP_JF:
		TC_Addf(b, "%.9g", v->u.f);
		break;
	case NETREP_JV:
		TC_Addf(b, "[%.9g,%.9g,%.9g]", v->u.v[0], v->u.v[1], v->u.v[2]);
		break;
	default:
		RepEnt_JsonStr(b, v->u.s);
		break;
	}
}

/* the value a variable reads back from the entity (as the type of the journal's value): 0 when it reads none */
static byte RepEnt_ReadBack(cEntity *e, const NetRepJVal *j, NetRepJVal *out) {
	*out = *j;
	switch (j->type) {
	case NETREP_JI: {
		int i = 0;
		if (!Entity_VarI(e, j->name, &i, 0)) {
			return 0;
		}
		out->u.i = i;
		return 1;
	}
	case NETREP_JF: {
		double d = 0.0;
		if (!Entity_VarF(e, j->name, &d, 0)) {
			return 0;
		}
		out->u.f = d;
		return 1;
	}
	case NETREP_JV:
		return Entity_VarV(e, j->name, &out->u.v[0], &out->u.v[1], &out->u.v[2], 0);
	default: {
		char *s = NULL;
		if (!Entity_VarS(e, j->name, &s, 0)) {
			return 0;
		}
		Str_Copy(out->u.s, s ? s : "", sizeof(out->u.s));
		return 1;
	}
	}
}

/* one entity of the mirror: its declared state, the journal of j's owner, read back */
void RepEnt_MirrorEnt(tcBuf *b, int n, int slot, uint32_t gen, cEntity *e, const NetRepJournal *j, int owner) {
	NetRepState st;
	int kind = RepEnt_Kind(e);
	RepEnt_State(e, kind, &st);
	st.x[REPX_GEN] = (int32_t)gen;
	TC_Add(b, n ? ",{" : "{", -1);
	TC_Addf(b, "\"slot\":%d,\"gen\":%u,\"kind\":%d,\"cls\":", slot, gen, kind);
	RepEnt_JsonStr(b, Entity_GetClassName(e));
	TC_Add(b, ",\"name\":", -1);
	RepEnt_JsonStr(b, e->name);
	TC_Addf(b, ",\"replica\":%d", Entity_IsType(e, TYEN_OBJECT) ? Obj(e)->replica : 0);
	/* the tag of the clients a private thing goes to (Players_OfOwned), 0 every client */
	TC_Addf(b, ",\"only\":%d", g_Instance && !Entity_IsType(e, TYEN_WITH_LIFE) ? Players_OfOwned(e) + 1 : 0);
	TC_Addf(b, ",\"pos\":[%d,%d,%d],\"vel\":[%d,%d,%d],\"ang\":[%u,%u,%u],\"view\":[%u,%u],\"life\":%d,\"flags\":%u,"
		"\"ctrl\":[%d,%d],\"act\":[%u,%u],\"actTime\":[%u,%u],\"weapon\":%u,\"x\":[", st.pos[0], st.pos[1], st.pos[2],
		st.vel[0], st.vel[1], st.vel[2], st.ang[0], st.ang[1], st.ang[2], st.view[0], st.view[1], st.life, st.flags,
		st.ctrl[0], st.ctrl[1], st.act[0], st.act[1], st.actTime[0], st.actTime[1], st.weapon);
	for (int i = 0; i < NETREP_X; i++) {
		TC_Addf(b, "%s%d", i ? "," : "", st.x[i]);
	}
	TC_Add(b, "],\"j\":[", -1);
	int k = 0;
	for (int i = j ? NetRepJ_First(j, owner) : -1; i >= 0; i = NetRepJ_Next(j, i), k++) {
		const NetRepJVal *v = NetRepJ_Get(j, i);
		NetRepJVal r;
		TC_Add(b, k ? ",[" : "[", -1);
		RepEnt_JsonStr(b, v->name);
		TC_Add(b, ",", 1);
		RepEnt_JsonVal(b, v);
		TC_Add(b, ",", 1);
		if (RepEnt_ReadBack(e, v, &r)) {
			RepEnt_JsonVal(b, &r);
		} else {
			TC_Add(b, "null", -1);
		}
		TC_Add(b, "]", 1);
	}
	TC_Add(b, "]}", -1);
}

void RepEnt_Mirror(tcBuf *b, const char *arg) {
	(void)arg;
	if (!g_Mp2) {
		TC_Add(b, "error: not -mp2\n", -1);
		return;
	}
	if (!g_Instance) {
		ReplicaEnt_Mirror(b);
		return;
	}
	TC_Add(b, "{\"side\":\"server\",\"ents\":[", -1);
	int n = 0;
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		cEntity *e = re.rec[s].e;
		if (!e || !RepEnt_Kind(e)) {
			continue;
		}
		RepEnt_MirrorEnt(b, n++, s, re.rec[s].gen, e, re.j, s);
	}
	TC_Add(b, "]}\n", -1);
}
