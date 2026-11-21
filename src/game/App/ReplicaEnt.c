/*
 * The replication v2 on the client (stage 3.2, -mp2), see include/scrap/repent.h: the create messages and journals of
 * the slots, the replicas made by the server's name and class, the class block on them, what the client made itself
 * deleted and counted. The snapshots, the clock and the placing stay the watcher's (Watch.c). Not in the original.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/initsteps.h"
#include "scrap/itementity.h"
#include "scrap/model.h"
#include "scrap/netaudit.h"
#include "scrap/netclass.h"
#include "scrap/netrep.h"
#include "scrap/netscene.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/pres.h"
#include "scrap/replicate.h"
#include "scrap/repent.h"
#include "scrap/uichan.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

#include "../Global/testchannel.h"

#define RC_JOURNAL 65536
#define RC_NAME 128					/* an entity name with its end (cEntity_ctor takes less than 0x7c) */
#define RC_CLASS 16
#define RC_LINKS 64					/* LinkModel values waiting for the entity they name */
#define RC_CLASS_WAIT_NS 5000000000ull	/* how long a replica waits for an action class the client lacks (a scene's,
									 * whose tables load when its record comes, include/scrap/netscene.h) */

typedef struct RcSlot {
	uint32_t gen;					/* of the create message held, 0 none */
	uint32_t madeGen;				/* of the replica made, 0 none */
	char cls[RC_CLASS];
	char name[RC_NAME];
	byte want;						/* the snapshots created the slot and did not remove it */
	byte made;
	uint64_t classWait;				/* Plat_RealNs when it began to wait for its action class, 0 */
} RcSlot;

static struct {
	RcSlot s[NETREP_MAX_ENTS];
	NetRepJournal *j;
	byte creating;					/* the replication makes an entity now: not counted */
	int applying;					/* a journal value goes into an entity now */
	uint32_t made, waits, fails, purged, messages, events, simCalls;
	int railDepth;					/* calls of the rail traffic's modules running */
	struct {
		int slot;					/* the slot + 1, 0: free */
		uint32_t gen;
		char value[NETREP_JSTR];
	} link[RC_LINKS];
	uint32_t linkWaits, linkLost;
} rc;

static NetRepJournal *ReplicaEnt_J(void) {
	if (!rc.j) {
		rc.j = NetRepJ_Create(NETREP_MAX_ENTS, RC_JOURNAL);
	}
	return rc.j;
}

/* the name without its digits, for the audit keys */
static void ReplicaEnt_Stem(const char *name, char *out, int size) {
	int n = 0;
	for (; *name && n < size - 1; name++) {
		if (*name < '0' || *name > '9') {
			out[n++] = *name;
		}
	}
	out[n] = 0;
}

static void ReplicaEnt_Count(const char *prefix, const char *type, const char *name) {
	char key[96], stem[64];
	ReplicaEnt_Stem(name, stem, sizeof(stem));
	Str_PrintfQuiet(key, sizeof(key), "%s:%s", type, stem);
	NetAudit_Name(NA_CREATE, prefix, key);
}

void ReplicaEnt_Created(const char *name, const char *type) {
	if (rc.creating || !crt_stricmp(type, TYEN_FCAM) || !crt_stricmp(type, TYEN_CAM) ||
		!crt_stricmp(type, TYEN_CAMPATH)) {
		return;
	}
	/* an interface of the instance shown here (Sputnik's hangar screen, the ship editor) makes its own hidden ships
	 * of the hangar, as single player's does: the interface's, not the world's */
	ReplicaEnt_Count(rc.railDepth ? "client.rail.create:" : Ui_ClientIn(0) ? "client.ui.create:" : "client.script.create:", type,
		name);
}

/* the modules of the rail traffic (the server's since stage 2.2: their calls on the client are counted apart) */
static byte ReplicaEnt_RailModule(const char *module) {
	return !strcmp(module, "Traffic") || !strcmp(module, "VehiclesNPC") || !strcmp(module, "InTraffic");
}

void ReplicaEnt_ModCall(const char *module, const char *func) {
	char level[32], key[96];
	const char *path = g_pApp && g_pApp->levelPath ? g_pApp->levelPath : "";
	const char *slash = strrchr(path, '/');
	byte rail = ReplicaEnt_RailModule(module);
	int n = 0;

	for (const char *p = slash ? slash + 1 : path; *p && n < (int)sizeof(level) - 1; p++) {
		level[n++] = (char)(*p >= 'A' && *p <= 'Z' ? *p - 'A' + 'a' : *p);
	}
	level[n] = 0;
	if (NetClass_ModuleTag(module, func, level) == NC_SIM) {
		Str_PrintfQuiet(key, sizeof(key), "%s.%s", module, func);
		if (rail || rc.railDepth) {
			NetAudit_Name(NA_CREATE, "client.rail.call:", key);
		} else if (Ui_ClientIn(0)) {
			/* a helper of the simulation an interface of the instance calls here for its own screen and ships */
			NetAudit_Name(NA_CREATE, "client.ui.call:", key);
		} else {
			NetAudit_Name(NA_CREATE, "client.sim.call:", key);
			rc.simCalls++;
		}
	}
	if (rail) {
		rc.railDepth++;
	}
}

void ReplicaEnt_ModLeave(const char *module) {
	if (rc.railDepth && ReplicaEnt_RailModule(module)) {
		rc.railDepth--;
	}
}

static void ReplicaEnt_Apply(cEntity *e, const NetRepJVal *v);

/* the replicas linked to the server's entity of slot own (their LinkModel names it) linked again: the own entity made
 * from that slot is another entity of the client (a ship the player boarded: the plate on it), or a new replica of it
 * (the ship parked again) */
static void ReplicaEnt_Relink(int own) {
	size_t n = strlen(rc.s[own].name);
	for (int slot = 0; n && rc.j && slot < NETREP_MAX_ENTS; slot++) {
		cEntity *e = slot != own && rc.s[slot].made ? Watch_SlotEntity(slot) : NULL;
		byte linked = 0;
		for (int k = e ? NetRepJ_First(rc.j, slot) : -1; k >= 0; k = NetRepJ_Next(rc.j, k)) {
			const NetRepJVal *v = NetRepJ_Get(rc.j, k);
			if (v->type == NETREP_JS && !strcmp(v->name, "LinkModel") && !strncmp(v->u.s, rc.s[own].name, n) &&
				v->u.s[n] == ',') {
				ReplicaEnt_Apply(e, v);
				linked = 1;
			}
		}
		/* the link's weight as the server's: without its entity between the replica's deletion and the own entity
		 * it went to 0 (cModMove_Update), the plate would grow in again */
		for (int k = linked ? NetRepJ_First(rc.j, slot) : -1; k >= 0; k = NetRepJ_Next(rc.j, k)) {
			const NetRepJVal *v = NetRepJ_Get(rc.j, k);
			if (!strcmp(v->name, "OffMul")) {
				ReplicaEnt_Apply(e, v);
			}
		}
	}
}

void ReplicaEnt_OwnCreate(int slot, int kind, const char *look, uint32_t tick, const NetRepState *st) {
	rc.creating = 1;
	Play_OwnCreate(kind, look, tick, st);
	ReplicaEnt_Journal(slot, Play_Entity());
	ReplicaEnt_Relink(slot);
	rc.creating = 0;
}

byte ReplicaEnt_Busy(void) {
	return rc.creating || rc.applying;
}

int ReplicaEnt_SlotOf(const cEntity *e) {
	return e && e->netSlot ? e->netSlot - 1 : -1;
}

const char *ReplicaEnt_Text(const cEntity *e, const char *name) {
	int slot = ReplicaEnt_SlotOf(e);
	int k = slot >= 0 && rc.j ? NetRepJ_Find(rc.j, slot, name) : -1;
	const NetRepJVal *v = k >= 0 ? NetRepJ_Get(rc.j, k) : NULL;
	return v && v->type == NETREP_JS ? v->u.s : NULL;
}

/* LinkModel "object,node" names another entity (the CHECK of cModMove_Link ends the game without it): the value
 * with the client's name of it (the own entity has its own name) in out; 0 when the client does not have it yet */
static byte ReplicaEnt_LinkValue(const char *value, char *out, int size) {
	char obj[RC_NAME];
	const char *comma = strchr(value, ',');
	int n = comma ? (int)(comma - value) : (int)strlen(value);
	if (!n || n >= (int)sizeof(obj)) {
		Str_Copy(out, value, size);
		return 1;
	}
	memcpy(obj, value, (size_t)n);
	obj[n] = 0;
	cEntity *t = ReplicaEnt_Find(obj);
	if (!t) {
		return 0;
	}
	Str_PrintfQuiet(out, size, "%s%s", t->name, comma ? comma : "");
	return 1;
}


/* a LinkModel of slot that names an entity the client lacks: applied by ReplicaEnt_Links once it is there */
static void ReplicaEnt_LinkWait(int slot, const char *value) {
	int free = -1;
	for (int k = 0; k < RC_LINKS; k++) {
		if (rc.link[k].slot == slot + 1) {
			free = k;
			break;
		}
		if (!rc.link[k].slot && free < 0) {
			free = k;
		}
	}
	if (free < 0) {
		rc.linkLost++;
		return;
	}
	rc.link[free].slot = slot + 1;
	rc.link[free].gen = rc.s[slot].madeGen ? rc.s[slot].madeGen : rc.s[slot].gen;
	Str_Copy(rc.link[free].value, value, sizeof(rc.link[free].value));
	rc.linkWaits++;
}

/* each frame: the waiting links whose entity came */
static void ReplicaEnt_Links(void) {
	for (int k = 0; k < RC_LINKS; k++) {
		int slot = rc.link[k].slot - 1;
		if (slot < 0) {
			continue;
		}
		cEntity *e = rc.s[slot].made && rc.s[slot].madeGen == rc.link[k].gen ? Watch_SlotEntity(slot) : NULL;
		if (!e && rc.s[slot].gen != rc.link[k].gen) {
			rc.link[k].slot = 0;	/* another entity in the slot now */
			continue;
		}
		char buf[NETREP_JSTR];
		if (!e || !ReplicaEnt_LinkValue(rc.link[k].value, buf, sizeof(buf))) {
			continue;
		}
		NetRepJVal v;
		memset(&v, 0, sizeof(v));
		Str_Copy(v.name, "LinkModel", sizeof(v.name));
		v.type = NETREP_JS;
		Str_Copy(v.u.s, rc.link[k].value, sizeof(v.u.s));
		rc.link[k].slot = 0;
		ReplicaEnt_Apply(e, &v);
	}
}

/* the variables of the client's own entity its prediction runs (stage 3.5): the rollback state brings the server's at
 * the tick they were set (a journal entry would set them again when it comes: a boost of the client started again).
 * Vel stays: a velocity the scripts set again and again (the brake of a parking box, Parking.OnPark) keeps the
 * prediction nearer to the server when it comes than without it (fewer corrections in the stage 3.5 tests). The cloud
 * and EMI counters (MotorDCnt, EMICounter) come with the rollback state: a late entry set the own cloud back */
static byte ReplicaEnt_Predicted(const char *name) {
	static const char *const names[] = { "Pos", "Ang", "ViewAng", "isBoost", "Life", "Energy", "MoveTime", "MovePos",
		"MoveAng", "MotorDCnt", "EMICounter" };
	for (size_t k = 0; k < sizeof(names) / sizeof(names[0]); k++) {
		if (!strcmp(name, names[k])) {
			return 1;
		}
	}
	return 0;
}

/* a journal value into the entity through its Var functions (an entry "name#n" of the values that add up is the
 * variable name) */
static void ReplicaEnt_Apply(cEntity *e, const NetRepJVal *v) {
	char name[NETREP_JNAME];
	rc.applying++;
	if (!strcmp(v->name, REPENT_J_FX) && v->type == NETREP_JS) {
		/* the embedded effect, played as the server's (g_PresIn); a replica made now takes the state only: the
		 * named effect of a character is a one-shot of the moment it was set */
		g_PresIn++;
		if (rc.creating && Entity_IsType(e, TYEN_WALKCHAR)) {
			cWithLifeEntity_SetEmbeddedFX(e, v->u.s);
		} else {
			Entity_SetEmbeddedFX(e, v->u.s);
		}
		g_PresIn--;
		rc.applying--;
		return;
	}
	if (!strcmp(v->name, REPENT_J_OWNMODEL)) {
		/* read where it applies (ArenaClient.c) */
		rc.applying--;
		return;
	}
	Str_Copy(name, v->name, sizeof(name));
	char *hash = strchr(name, '#');
	if (hash) {
		*hash = 0;
	}
	if (e == Play_Entity() && ReplicaEnt_Predicted(name)) {
		NetAudit_Name(NA_PREDICT, "pred.journal:", name);
		rc.applying--;
		return;
	}
	char file[0x200];
	if (v->type == NETREP_JI && !strcmp(name, "AnmP0") && Entity_IsType(e, TYEN_CAR) &&
		cVehicleEntity_PilotAnimFile(Veh(e), v->u.i, file, sizeof(file)) && !Watch_ModelExists(file)) {
		/* an animation of a pilot this replica shows another model for (the missing pilot's stand-in model): the
		 * file is not there, its load would end the client */
		NetAudit_Name(NA_CREATE, "client.skip:", file);
		rc.applying--;
		return;
	}
	switch (v->type) {
	case NETREP_JI: {
		int i = v->u.i;
		Entity_VarI(e, name, &i, 1);
		break;
	}
	case NETREP_JF: {
		double d = v->u.f;
		Entity_VarF(e, name, &d, 1);
		break;
	}
	case NETREP_JV: {
		double x = v->u.v[0], y = v->u.v[1], z = v->u.v[2];
		Entity_VarV(e, name, &x, &y, &z, 1);
		break;
	}
	case NETREP_JS: {
		char buf[NETREP_JSTR];
		char *s = buf;
		if (!strcmp(name, "LinkModel")) {
			if (!ReplicaEnt_LinkValue(v->u.s, buf, sizeof(buf))) {
				ReplicaEnt_LinkWait(ReplicaEnt_SlotOf(e) >= 0 ? ReplicaEnt_SlotOf(e) : Play_OwnSlot(), v->u.s);
				break;
			}
			byte linked = Entity_IsType(e, TYEN_OBJECT) && Obj(e)->mod.link.target;
			if (buf[0] == ',' && Entity_IsType(e, TYEN_OBJECT) && !linked) {
				/* the unlink of a link the replica does not have (it waited for its entity) */
				break;
			}
			if (buf[0] != ',' && linked) {
				/* a new link of a linked replica: the scripts' unlink before it is not in the journal (the editor's
				 * end links the platform's ship again, include/scrap/uichan.h) */
				char *un = ",";
				Entity_VarS(e, name, &un, 1);
			}
		} else if (!strcmp(name, "ActCtrl") && rc.creating && Entity_IsType(e, TYEN_WITH_LIFE) && WLife(e)->actCtrl &&
			!strcmp(WLife(e)->actCtrl->cls->name, v->u.s)) {
			/* the replica was made with that class: a second one would make the model's embedded effect again (a
			 * scene's effect, FXScene_Create, played twice: twice the steam of Yakuzzi) */
			break;
		} else {
			Str_Copy(buf, v->u.s, sizeof(buf));
		}
		Entity_VarS(e, name, &s, 1);
		break;
	}
	}
	rc.applying--;
}

void ReplicaEnt_Journal(int slot, cEntity *e) {
	if (!e || slot < 0 || slot >= NETREP_MAX_ENTS || !rc.j) {
		return;
	}
	for (int k = NetRepJ_First(rc.j, slot); k >= 0; k = NetRepJ_Next(rc.j, k)) {
		ReplicaEnt_Apply(e, NetRepJ_Get(rc.j, k));
	}
}

/* the replica of a slot made, the own entity too */
static cEntity *ReplicaEnt_Entity(int slot) {
	return rc.s[slot].made ? Watch_SlotEntity(slot) : NULL;
}

void ReplicaEnt_Message(const uint8_t *data, int size) {
	NetRepReader r;
	NetRepJVal v;
	NetRepJournal *j = ReplicaEnt_J();

	if (!j || size < 2) {
		return;
	}
	rc.messages++;
	int kind = data[1];
	NetRepReader_Init(&r, data + 2, size - 2);
	while (r.pos < r.size && !r.bad) {
		uint32_t slot = NetRep_GetVar(&r), gen = NetRep_GetVar(&r);
		if (r.bad || slot >= NETREP_MAX_ENTS) {
			break;
		}
		RcSlot *s = &rc.s[slot];
		if (kind == REPENT_K_DROP) {
			/* the entity is gone on the server: its journal too (its replica, if any, goes with the snapshots) */
			if (gen == s->gen) {
				NetRepJ_Clear(j, (int)slot);
			}
			continue;
		}
		if (kind == REPENT_K_EVENT) {
			if (!NetRep_GetJVal(&r, &v)) {
				break;
			}
			cEntity *e = gen == s->madeGen ? ReplicaEnt_Entity((int)slot) : NULL;
			if (e) {
				ReplicaEnt_Apply(e, &v);
				rc.events++;
			}
			continue;
		}
		if (kind == REPENT_K_CREATE) {
			char cls[RC_CLASS], name[RC_NAME];
			NetRep_GetStr(&r, cls, sizeof(cls));
			NetRep_GetStr(&r, name, sizeof(name));
			if (gen != s->gen) {
				NetRepJ_Clear(j, (int)slot);
				s->gen = gen;
			}
			Str_Copy(s->cls, cls, sizeof(s->cls));
			Str_Copy(s->name, name, sizeof(s->name));
		} else if (kind != REPENT_K_VARS) {
			break;
		}
		uint32_t n = NetRep_GetVar(&r);
		cEntity *e = gen == s->gen && gen == s->madeGen ? ReplicaEnt_Entity((int)slot) : NULL;
		for (uint32_t i = 0; i < n && !r.bad; i++) {
			if (!NetRep_GetJVal(&r, &v)) {
				break;
			}
			if (gen != s->gen) {
				continue;		/* the changes of an entity this client no longer has the create message of */
			}
			NetRepJ_Set(j, (int)slot, &v);
			if (e) {
				ReplicaEnt_Apply(e, &v);
			}
		}
	}
}

void ReplicaEnt_Want(int slot) {
	if (slot >= 0 && slot < NETREP_MAX_ENTS) {
		rc.s[slot].want = 1;
		rc.s[slot].made = 0;
		rc.s[slot].madeGen = 0;
	}
}

void ReplicaEnt_Unwant(int slot) {
	if (slot >= 0 && slot < NETREP_MAX_ENTS) {
		rc.s[slot].want = 0;
		rc.s[slot].made = 0;
		rc.s[slot].madeGen = 0;
	}
}

void ReplicaEnt_Update(int slot, const NetRepState *st) {
	RcSlot *s = &rc.s[slot];
	if (s->made && (uint32_t)st->x[REPX_GEN] != s->madeGen) {
		/* another entity of the server in the slot now (built otherwise, renamed): made again */
		Watch_FreeSlot(slot);
		ReplicaEnt_Want(slot);
	}
}

/* the action class of a look O|model|action class is one the client does not have (yet) */
static byte ReplicaEnt_ClassMissing(const char *look) {
	const char *p = strchr(look, '|');
	p = p ? strchr(p + 1, '|') : NULL;
	return p && p[1] && g_pActClassList && !cActClassList_Get(g_pActClassList, p + 1);
}

/* a replica of an object, a thing with life or a plain entity: O|model|action class */
static cEntity *ReplicaEnt_BuildOther(RcSlot *s, int kind, const char *look, const NetRepState *st) {
	char buf[NETREP_LOOK_MAX + 1];
	char *model = "", *act = "";

	Str_Copy(buf, look, sizeof(buf));
	char *p = strchr(buf, '|');
	if (p) {
		model = p + 1;
		p = strchr(model, '|');
		if (p) {
			*p = 0;
			act = p + 1;
		}
	}
	if (model[0] && !Watch_ModelExists(model)) {
		return NULL;
	}
	cEntity *e = cWorld_CreateEntity(g_pWorld, s->name, NetRep_UPos(st->pos[0]), NetRep_UPos(st->pos[1]),
		NetRep_UPos(st->pos[2]), s->cls);
	if (!e || kind == REPENT_BASE) {
		return e;
	}
	Obj(e)->replica = 1;
	if (model[0]) {
		Entity_SetModel(e, model, 1.0f, 1.0f, 1.0f, 0);
	}
	if (act[0] && Entity_IsType(e, TYEN_WITH_LIFE)) {
		WithLife_SetActCtrl(e, act);
	}
	vec3_t ang = { NetRep_UAng(st->ang[0]), NetRep_UAng(st->ang[1]), NetRep_UAng(st->ang[2]) };
	Obj_SetAng(e, &ang);
	return e;
}

/* the entities the client made itself away: not the replicas, the own entity, the cameras (the rail traffic is the
 * server's, include/scrap/viewers.h) */
static void ReplicaEnt_Purge(void) {
	cEntity *own = Play_Entity();
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e->deadNode.target || e->netSlot || e == own || !e->name) {
			continue;
		}
		const char *cls = Entity_GetClassName(e);
		if (!strcmp(cls, TYEN_FCAM) || !strcmp(cls, TYEN_CAM) || !strcmp(cls, TYEN_CAMPATH) ||
			(Entity_IsType(e, TYEN_OBJECT) && Obj(e)->replica)) {
			continue;
		}
		ReplicaEnt_Count("client.purge:", cls, e->name);
		cEntity_Del(e);
		rc.purged++;
	}
}

/* the look of a first replication kind for a replica: a ship without its pilot (the journal sets it in the order of
 * the server's sets, its animations after it) */
static const char *ReplicaEnt_Look(int kind, const char *look, char *out, int size) {
	if (kind != NETREP_SHIP) {
		return look;
	}
	const char *p = strchr(look, '|');
	p = p ? strchr(p + 1, '|') : NULL;
	const char *q = p ? strchr(p + 1, '|') : NULL;
	if (!q) {
		return look;
	}
	Str_PrintfQuiet(out, size, "%.*s%s", (int)(p + 1 - look), look, q);
	return out;
}

void ReplicaEnt_Frame(void) {
	int kind;
	const char *look;
	uint32_t tick;
	char lk[NETREP_LOOK_MAX + 1];

	/* a level whose start runs the client's steps only (include/scrap/initsteps.h) makes nothing to delete */
	/* nor while an interface of the instance is shown: the ships of the hangar screen are its own
	 * (include/scrap/uichan.h) */
	if (!InitSteps_Client() && !Ui_ClientIn(0)) {
		ReplicaEnt_Purge();
	}
	ReplicaEnt_Links();
	for (int slot = 0; slot < NETREP_MAX_ENTS; slot++) {
		RcSlot *s = &rc.s[slot];
		if (!s->want || s->made || !s->gen) {
			continue;
		}
		const NetRepState *st = Watch_SlotState(slot, &kind, &look, &tick);
		if (!st || (uint32_t)st->x[REPX_GEN] != s->gen) {
			continue;
		}
		cEntity *e;
		rc.creating = 1;
		if (slot == Play_OwnSlot()) {
			Play_OwnCreate(kind, look, tick, st);
			e = Play_Entity();
			ReplicaEnt_Journal(slot, e);
			ReplicaEnt_Relink(slot);
		} else {
			cEntity *taken = World_FindEntity(s->name);
			if (!taken && kind > NETREP_ITEM && ReplicaEnt_ClassMissing(look)) {
				uint64_t now = Plat_RealNs();
				if (!s->classWait) {
					s->classWait = now;
				}
				if (now - s->classWait < RC_CLASS_WAIT_NS) {
					rc.creating = 0;
					rc.waits++;
					continue;
				}
			}
			s->classWait = 0;
			if (taken) {
				/* the name is the client's own entity's (deleted above, free after the frame) or a replica's
				 * that is not removed yet: next frame */
				rc.creating = 0;
				rc.waits++;
				continue;
			}
			e = kind <= NETREP_ITEM ? Watch_Build(s->name, kind, ReplicaEnt_Look(kind, look, lk, sizeof(lk)), st, 1) :
				ReplicaEnt_BuildOther(s, kind, look, st);
			if (e) {
				e->netSlot = (uint16_t)(slot + 1);
				ReplicaEnt_Journal(slot, e);
				Watch_Attach(slot, e, kind, tick, st);
				/* the replicas linked to this slot's entity link to the new replica (the plate when the own ship
				 * becomes a parked replica again) */
				ReplicaEnt_Relink(slot);
			}
		}
		rc.creating = 0;
		s->made = 1;
		s->madeGen = s->gen;
		if (e) {
			rc.made++;
		} else {
			rc.fails++;
			ReplicaEnt_Count("client.fail:", s->cls, s->name);
		}
	}
}

/* ms of the class block as seconds */
static float ReplicaEnt_Sec(int32_t ms) {
	return (float)ms / 1000.0f;
}

static const int g_LayerX[2][2] = { { REPX_ACT2, REPX_ACT2T }, { REPX_ACT3, REPX_ACT3T } };

/* the action (index + 1) and its time (s) of layer l in a state */
static uint32_t ReplicaEnt_Act(const NetRepState *s, int l) {
	return l < NETREP_LAYERS ? s->act[l] : (uint32_t)s->x[g_LayerX[l - NETREP_LAYERS][0]];
}

static float ReplicaEnt_ActTime(const NetRepState *s, int l) {
	return (float)(l < NETREP_LAYERS ? s->actTime[l] : (uint32_t)s->x[g_LayerX[l - NETREP_LAYERS][1]]) / 1000.0f;
}

float ReplicaEnt_LayerTarget(const NetRepState *a, const NetRepState *b, float f, int l, uint32_t *act) {
	uint32_t ia = ReplicaEnt_Act(a, l), ib = ReplicaEnt_Act(b, l);
	float ta = ReplicaEnt_ActTime(a, l), tb = ReplicaEnt_ActTime(b, l);
	*act = ia;
	return ia && ia == ib && tb >= ta ? ta + (tb - ta) * f : ia ? ta : -1.0f;
}

/* a ship's layers: the actions of its parts the class shows from the ship's state (cActCtrl_Present) */
static void ReplicaEnt_ShipLayers(cActCtrl *ctrl, const NetRepState *s) {
	for (int l = 0; l < 4; l++) {
		uint32_t ia = ReplicaEnt_Act(s, l);
		cAction *act = ia ? ctrl->cls->first : NULL;
		for (uint32_t i = 1; act && i < ia; i++) {
			act = act->next;
		}
		if (act == ctrl->layers[l].action && (act != NULL) == (ctrl->layers[l].on != 0)) {
			continue;
		}
		if (act && act->chanel == l) {
			cActCtrl_PlayAction(ctrl, act, 1, 0, 1.0f);
		} else if (!act && l > 0 && ctrl->layers[l].on) {
			cActCtrl_StopChan(ctrl, l);
		}
	}
}

void ReplicaEnt_Present(cEntity *e, int kind, const NetRepState *a, const NetRepState *b, float span, float f,
	float dt) {
	if (kind == REPENT_BASE) {
		return;
	}
	cObjEntity *o = Obj(e);
	o->radarShow = a->x[REPX_RADAR];
	if (CGameModel_IsLoaded(&o->model)) {
		uint32_t fx = (uint32_t)a->x[REPX_FX], p = 0;
		CGameModel_GetEffectParam(&o->model, 0, &p);
		if (CGameModel_GetEffect(&o->model, NULL) != fx) {
			Obj_SetEffect(e, fx);
		}
		if (p != (uint32_t)a->x[REPX_FXP]) {
			float f;
			uint32_t bits = (uint32_t)a->x[REPX_FXP];
			memcpy(&f, &bits, sizeof(f));
			Obj_SetEffectParam(e, 0, f);
		}
	}
	o->move.active = (a->x[REPX_MOVE] >> 30) & 1;
	o->move.time = ReplicaEnt_Sec(a->x[REPX_MOVE] & 0x3fffffff);
	o->move.moveTime = ReplicaEnt_Sec(a->x[REPX_MOVET]);
	if (kind == NETREP_ITEM || kind == REPENT_OBJECT || kind == REPENT_LIFE) {
		/* a thing of the level that turned or moved without a velocity (Watch_Place moves an item only) */
		vec3_t ang = { NetRep_UAng(a->ang[0]), NetRep_UAng(a->ang[1]), NetRep_UAng(a->ang[2]) };
		if (NetRep_QAng(o->ang.x) != a->ang[0] || NetRep_QAng(o->ang.y) != a->ang[1] ||
			NetRep_QAng(o->ang.z) != a->ang[2]) {
			Obj_SetAng(e, &ang);
		}
	}
	if (kind == NETREP_ITEM) {
		cItemEntity *it = (cItemEntity *)e;
		byte was = it->respawn > 0.0f;
		it->respawn = ReplicaEnt_Sec(a->x[REPX_ITEM]);
		it->blink = (byte)a->x[REPX_BLINK];
		/* the model hidden while the item waits to come back, as cItemEntity_SetRespawn hides it (the parking box
		 * of a boarded ship, rTime 0.5); a take's or a return's fade effect comes as its record before that */
		if (was != (it->respawn > 0.0f) && CGameModel_IsLoaded(&o->model)) {
			if (it->respawn > 0.0f) {
				CGameModel_Disable(&o->model);
			} else {
				CGameModel_Enable(&o->model);
			}
		}
		return;
	}
	if (kind == REPENT_OBJECT) {
		return;
	}
	cWithLifeEntity *w = WLife(e);
	if (kind == REPENT_LIFE) {
		w->life = a->life;
	}
	w->prevLife = a->x[REPX_PREVLIFE];
	w->fireRemain = ReplicaEnt_Sec(a->x[REPX_BURN]);
	w->arrow.canUse = a->x[REPX_ARROW] & 1;
	w->arrow.canView = (a->x[REPX_ARROW] >> 1) & 1;
	w->arrow.canMission = (a->x[REPX_ARROW] >> 2) & 1;
	w->arrow.coneStatus = (a->x[REPX_ARROW] >> 3) & 3;
	w->arrow.tgType = a->x[REPX_ARROW] >> 8;
	if (w->actCtrl && w->actCtrl->cls && w->actCtrl->cls->dynShip) {
		ReplicaEnt_ShipLayers(w->actCtrl, a);
	} else if (w->actCtrl && w->actCtrl->cls && (kind == NETREP_CHAR || kind == REPENT_LIFE)) {
		/* the layers of characters and things with life at the network's times */
		uint16_t act[4];
		float rates[4], target[4];
		for (int l = 0; l < 4; l++) {
			uint32_t ia;
			target[l] = ReplicaEnt_LayerTarget(a, b, f, l, &ia);
			uint32_t ib = ReplicaEnt_Act(b, l);
			float ta = ReplicaEnt_ActTime(a, l), tb = ReplicaEnt_ActTime(b, l);
			act[l] = (uint16_t)ia;
			/* the rate of the server between the samples (0 for one sample: no record since, so its time stands),
			 * not known (-1) when the action changed or went round between them */
			rates[l] = ia == ib && tb >= ta ? (span > 0.0f ? (tb - ta) / span : 0.0f) : -1.0f;
			rates[l] = rates[l] > 4.0f ? 4.0f : rates[l];
		}
		w->actCtrl->walkSpeed = ReplicaEnt_Sec(a->x[REPX_WALK]);
		if (NetScene_Exact(e)) {
			/* the scene entity and its actors stand at the server's times of the moment drawn: the camera, the
			 * actors and the scene's own events at the server's scene time, from its first frame on */
			cActCtrl_NetExact(w->actCtrl, act, target, 4, NetScene_Filter(e));
		} else {
			cActCtrl_NetDrive(w->actCtrl, act, rates, target, 4, dt, NetScene_Filter(e));
		}
	}
	if (kind == NETREP_CHAR) {
		/* the head turns with the view by this lock (WCE_ViewLockBlend, Present) */
		WChar(e)->viewLock = (byte)a->x[REPX_HEAD];
	}
	if (kind == NETREP_SHIP) {
		cVehicleEntity *v = Veh(e);
		v->maxLife = a->x[REPX_MAXLIFE];
		v->boost = ReplicaEnt_Sec(a->x[REPX_BOOST]);
		v->cloud = ReplicaEnt_Sec(a->x[REPX_CLOUD] & 0x3fffffff);
		v->cloudOn = (a->x[REPX_CLOUD] >> 30) & 1;
		v->emi = ReplicaEnt_Sec(a->x[REPX_EMI]);
		cWeaponManager_NetPlug(&v->weapons, e, ((uint32_t)a->x[REPX_FIREW] >> 31) != 0);
		cWeaponManager_NetWeapSnd(&v->weapons, a->x[REPX_WEAPSND] - 1);
		v->controls.cm = (a->x[REPX_FIREW] >> 30) & 1;
		Hook_NetPresent(e, v->controls.cm, dt);
	} else if (kind == NETREP_MISSILE) {
		Missile(e)->level = a->x[REPX_LEVEL];
	}
}

cEntity *ReplicaEnt_Find(const char *name) {
	/* the own slot's name is the own entity first: the replica of that name (the parked ship the player boards) is
	 * deleted after the frame, a link to it now would go with it (the plate on the boarded ship) */
	int own = Play_OwnSlot();
	if (own >= 0 && own < NETREP_MAX_ENTS && !strcmp(rc.s[own].name, name) && Play_Entity()) {
		return Play_Entity();
	}
	cEntity *e = World_FindEntity(name);
	return e ? e : Play_Wreck(name);
}

const char *ReplicaEnt_SlotName(int slot) {
	return slot >= 0 && slot < NETREP_MAX_ENTS ? rc.s[slot].name : "";
}

void ReplicaEnt_Reset(void) {
	NetScene_ClientReset();
	if (rc.j) {
		for (int s = 0; s < NETREP_MAX_ENTS; s++) {
			NetRepJ_Clear(rc.j, s);
		}
	}
	memset(rc.s, 0, sizeof(rc.s));
	memset(rc.link, 0, sizeof(rc.link));
}

void ReplicaEnt_Mirror(tcBuf *b) {
	float view[3] = { 0.0f, 0.0f, 0.0f };
	cEntity *own = Play_Entity();
	if (own) {
		view[0] = own->pos.x;
		view[1] = own->pos.y;
		view[2] = own->pos.z;
	}
	TC_Addf(b, "{\"side\":\"client\",\"view\":[%.0f,%.0f,%.0f],\"own\":%d,\"tag\":%d,\"made\":%u,\"waits\":%u,\"fails\":%u,"
		"\"purged\":%u,\"simcalls\":%u,\"linkwaits\":%u,\"linklost\":%u,\"jused\":%d,\"jlost\":%u,\"ents\":[",
		(double)view[0], (double)view[1], (double)view[2], Play_OwnSlot(), Play_Player() + 1, rc.made, rc.waits, rc.fails,
		rc.purged, rc.simCalls, rc.linkWaits, rc.linkLost, rc.j ? NetRepJ_Used(rc.j) : 0, rc.j ? NetRepJ_Lost(rc.j) : 0u);
	int n = 0;
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		cEntity *e = rc.s[s].made && s != Play_OwnSlot() ? Watch_SlotEntity(s) : NULL;
		if (!e || e->deadNode.target) {
			continue;
		}
		RepEnt_MirrorEnt(b, n++, s, rc.s[s].madeGen, e, rc.j, s);
	}
	TC_Add(b, "]}\n", -1);
}
