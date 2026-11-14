/*
 * The watcher: a game that shows the world of a headless instance, see include/scrap/replicate.h. Not in the original.
 *
 * Connect, hello, welcome (the level, loaded when it is not the one running), then the snapshots: each record gives
 * a sample of its proxy at the server time tick / NETREP_TICK_HZ + dt. Every frame the proxies are placed at the
 * server time now minus the interpolation delay (NetRepDelay: the snapshot gap and the measured jitter), between
 * the two samples around it (or ahead of the newest by its velocity for up to WATCH_EXTRAP s). The server time is the
 * local clock plus an offset that follows the ticks of the snapshots.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/arena.h"
#include "scrap/app.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/misc.h"
#include "scrap/netrep.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/coord.h"
#include "scrap/travel.h"
#include "scrap/scrap.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/vehicleattack.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weapons.h"
#include "scrap/weaps.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

#define WATCH_SAMPLES 8
#define WATCH_FOLLOW_JUMP 0.25		/* s: the drawn server time further off its target is taken at once */
#define WATCH_EXTRAP 0.25			/* s ahead of the newest sample at most */
#define WATCH_EXTRAP_TICKS 2			/* the sample must be from a snapshot at most this many ticks old */
#define WATCH_HOLD (1.5 / NETREP_SNAP_HZ)	/* s of motion before a sample at most */
#define WATCH_JUMP_SPEED 50000.0f		/* units per second, faster is a jump */
#define WATCH_ACK_MS 50				/* an ack at least this often */
#define WATCH_STATUS_MS 10000
#define WATCH_CAM_NAME "WatchCam"
#define WATCH_NAME "netrep%d_%u"
#define WATCH_CONNECT_MS 15000		/* the first frames after a level load can be slow */
#define WATCH_RETRIES 5
#define WATCH_AHEAD_RADIUS 25000.0f	/* units: the proxies this near move to the predicted tick (Watch_ProxiesAt) */
#define WATCH_MISSILE_LIFE 1e9f		/* s: a missile proxy ends when the instance's missile does, not by itself */

enum { WATCH_OFF, WATCH_CONNECTING, WATCH_HELLO, WATCH_LEVEL, WATCH_RUN, WATCH_GONE };

typedef struct WatchSample {
	double t;						/* server time, s */
	uint32_t tick;					/* of its snapshot */
	NetRepState st;
} WatchSample;

typedef struct WatchProxy {
	cEntity *e;
	int kind;
	int n;
	WatchSample s[WATCH_SAMPLES];	/* by time */
} WatchProxy;

byte g_Watch;

static void Watch_Metro(const uint8_t *data, int size);

static struct {
	char addr[64];
	uint16_t port;					/* of the instance, 0 -port */
	uint32_t rate;
	int phase;
	byte target;					/* a ticket's instance to go to (a world of a coordinator) */
	int ticketLen;
	uint8_t ticket[COORD_TICKET_MAX];
	Net2Host *host;
	NetRepView *view;
	char level[128];
	int welcome;					/* the flags of the welcome (REP_WELCOME_), 0 before it */
	float start[3];					/* the view point of the welcome */
	NetRepClock clock;
	NetRepDelay delay;				/* the interpolation delay */
	NetRepFollow follow;			/* the server time the proxies are drawn at, evenly */
	double rtt, rttVar;				/* s, of Net2 */
	float loss;						/* of Net2, 0..1 */
	double shown;					/* the server time the proxies are drawn at, s */
	uint64_t lastNs, lastAck, lastStatus;
	int retries;
	Net2Addr server;
	byte taken;						/* a message taken since the last ack */
	cEntity *cam;
	FILE *log;
	uint32_t msgs, notTaken, proxies, shots;
	uint32_t records;				/* things made of the instance's static records */
	uint32_t made;					/* proxies made: their names stay unique while a deleted one waits */
	uint64_t bytes;
	WatchProxy px[NETREP_MAX_ENTS];
} watch;

byte Watch_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("watch", param)) {
		Str_Copy(watch.addr, ext, sizeof(watch.addr));
		return 1;
	}
	if (!crt_stricmp("watchrate", param)) {
		watch.rate = (uint32_t)crt_atoi(ext);
		return 1;
	}
	return 0;
}

static double Watch_Now(void) {
	return (double)Plat_RealNs() / 1e9;
}

/* ---- the proxies ---- */

static void Watch_Free(int slot) {
	WatchProxy *p = &watch.px[slot];
	if (p->e) {
		cEntity_Del(p->e);
		watch.proxies--;
	}
	p->e = NULL;
	p->n = 0;
}

/* the model file or its compiled .sm3 (the loader takes either) */
static byte Watch_ModelExists(const char *path) {
	char alt[260];
	size_t n = strlen(path);

	if (File_Exists(path)) {
		return 1;
	}
	if (n < 4 || n >= sizeof(alt) || path[n - 4] != '.') {
		return 0;
	}
	memcpy(alt, path, n + 1);
	memcpy(alt + n - 3, "sm3", 3);
	return File_Exists(alt);
}

static float Watch_DistSq(const cEntity *a, const cEntity *b) {
	float dx = a->pos.x - b->pos.x, dy = a->pos.y - b->pos.y, dz = a->pos.z - b->pos.z;
	return dx * dx + dy * dy + dz * dz;
}

cEntity *Watch_SlotEntity(int slot) {
	if (slot < 0 || slot >= NETREP_MAX_ENTS) {
		return NULL;
	}
	return slot == Play_OwnSlot() ? Play_Entity() : watch.px[slot].e;
}

/* a missile proxy of M|model|weapon|effect|level: a Swarm's or an Inferno's as the network copy of the game makes it
 * (its bees, trails and engine sound; its hit effects), another one with its model and embedded effect; the bees of
 * life, fired by the entity of the slot view[0] - 1 */
static cEntity *Watch_Missile(const char *name, char *const *part, int n, const NetRepState *st) {
	vec3_t pos = { NetRep_UPos(st->pos[0]), NetRep_UPos(st->pos[1]), NetRep_UPos(st->pos[2]) };
	vec3_t dir = { NetRep_UAng(st->ang[0]), NetRep_UAng(st->ang[1]), NetRep_UAng(st->ang[2]) };
	int weap = n > 2 ? crt_atoi(part[2]) : 0, fx = n > 3 ? crt_atoi(part[3]) : 0, level = n > 4 ? crt_atoi(part[4]) : 0;
	cEntity *parent = Watch_SlotEntity((int)st->view[0] - 1);
	cEntity *e;

	if (weap == MISSILE_WEAP_SWARM || weap == MISSILE_WEAP_INFERNO) {
		e = weap == MISSILE_WEAP_SWARM ? Swarm_NetCreate(name, pos, dir) : Inferno_NetCreate(name, pos, dir);
		if (!e) {
			return NULL;
		}
		Obj(e)->proxy = 1;
		cMissileEntity_SetParent(Missile(e), parent ? parent->name : "");
		if (weap == MISSILE_WEAP_SWARM) {
			Swarm_NetSetUp(name);
		} else {
			Inferno_NetSetUp(name);
		}
		Missile(e)->level = level;
		cMissileEntity_SetSwarm(Missile(e), st->life);
	} else {
		if (part[1][0] && !Watch_ModelExists(part[1])) {
			return NULL;
		}
		e = cWorld_CreateEntity(g_pWorld, name, pos.x, pos.y, pos.z, TYEN_MISSILE);
		if (!e) {
			return NULL;
		}
		Obj(e)->proxy = 1;
		Obj_SetAng(e, &dir);
		if (part[1][0]) {
			Entity_SetModel(e, part[1], 1.0f, 1.0f, 1.0f, 0);
		}
		if (fx > MISSILE_FX_NONE && fx < MISSILE_FXS) {
			Entity_SetEmbeddedFX(e, g_MissileFXNames[fx]);
		}
		cMissileEntity_SetParent(Missile(e), parent ? parent->name : "");
	}
	cMissileEntity_SetLifeTime(Missile(e), WATCH_MISSILE_LIFE);
	return e;
}

/* the instance's missile hit: bees lost (its life fell) or the end of it (it went off, or its slot went away while it
 * was on): its hit and explosion effects on the proxy, at its newest place */
static void Watch_MissileHit(WatchProxy *p, const NetRepState *st, byte end) {
	cEntity *e = p->e;
	if (!e || p->kind != NETREP_MISSILE || !Entity_IsType(e, TYEN_MISSILE) || !e->active) {
		return;
	}
	cMissileEntity *m = Missile(e);
	int lost = end ? m->swarmCount + 1 : m->swarmCount - st->life;
	if (lost <= 0) {
		return;
	}
	if (end && p->n) {
		const NetRepState *s = &p->s[p->n - 1].st;
		vec3_t at = { NetRep_UPos(s->pos[0]), NetRep_UPos(s->pos[1]), NetRep_UPos(s->pos[2]) };
		Entity_SetPos(e, &at);
	}
	cMissileEntity_Hit(m, lost, 0.0f);
}

/* S|model|pilot|motor 0..3|class|weapon slots, C|class or M|model|weapon|effect|level (Replicate.c) */
cEntity *Watch_Build(const char *name, int kind, const char *look, const NetRepState *st, byte proxy) {
	char buf[NETREP_LOOK_MAX + 1];
	char *part[9];
	int n = 0;

	Str_Copy(buf, look, sizeof(buf));
	for (char *p = buf; n < 9;) {
		part[n++] = p;
		p = strchr(p, '|');
		if (!p) {
			break;
		}
		*p++ = 0;
	}
	if (World_FindEntity(name)) {
		return NULL;
	}
	float x = NetRep_UPos(st->pos[0]), y = NetRep_UPos(st->pos[1]), z = NetRep_UPos(st->pos[2]);
	if (kind == NETREP_CHAR && n >= 2 && part[1][0]) {
		cEntity *e = cWorld_CreateEntity(g_pWorld, name, x, y, z, TYEN_WALKCHAR);
		if (e) {
			Obj(e)->proxy = proxy;
			/* a class the level does not load (a player's of the freeroam factions) */
			Faction_EnsureClass(part[1]);
			WithLife_SetActCtrl(e, part[1]);
			Con_Printf("watch: character %s (%s) at %.0f %.0f %.0f\n", name, part[1], (double)x, (double)y, (double)z);
		}
		return e;
	}
	if (kind == NETREP_MISSILE && n >= 2) {
		return Watch_Missile(name, part, n, st);
	}
	/* an item the instance made after the level's start (I|model): seen, not taken here */
	if (kind == NETREP_ITEM && n >= 2) {
		if (part[1][0] && !Watch_ModelExists(part[1])) {
			return NULL;
		}
		cEntity *e = cWorld_CreateEntity(g_pWorld, name, x, y, z, "item");
		if (e) {
			Obj(e)->proxy = proxy;
			if (part[1][0]) {
				Entity_SetModel(e, part[1], 1.0f, 1.0f, 1.0f, 0);
			}
		}
		return e;
	}
	if (kind != NETREP_SHIP || n < 8) {
		return NULL;
	}
	/* a model this game does not have would stop it in the loader */
	if (part[1][0] && !Watch_ModelExists(part[1])) {
		Con_Printf("watch: no model %s for %s\n", part[1], name);
		return NULL;
	}
	cEntity *e = cWorld_CreateEntity(g_pWorld, name, x, y, z, TYEN_CAR);
	if (!e) {
		return NULL;
	}
	Obj(e)->proxy = proxy;
	if (part[1][0]) {
		Entity_SetModel(e, part[1], 1.0f, 1.0f, 1.0f, 0);
	}
	if (part[7][0]) {
		WithLife_SetActCtrl(e, part[7]);
	}
	for (int i = 0; i < 4; i++) {
		char var[8];
		Str_PrintfQuiet(var, sizeof(var), "MEng%d", i);
		if (part[3 + i][0]) {
			Entity_VarS(e, var, &part[3 + i], 1);
		}
	}
	if (part[2][0]) {
		Entity_VarS(e, "MPil0", &part[2], 1);
	}
	/* the weapons mounted where the instance has them (the effects of the shots start at them) */
	if (n > 8 && part[8][0]) {
		Entity_VarS(e, "WeapList", &part[8], 1);
	}
	int one = 1;
	Entity_VarI(e, "isInit", &one, 1);
	return e;
}

static void Watch_Sample(WatchProxy *p, uint32_t tick, const NetRepState *st) {
	double t = (double)tick / NETREP_TICK_HZ + st->dt / 1000.0;
	int i = p->n;
	while (i > 0 && p->s[i - 1].t >= t) {
		if (p->s[i - 1].t == t) {
			p->s[i - 1].st = *st;
			p->s[i - 1].tick = tick;
			return;
		}
		i--;
	}
	if (p->n == WATCH_SAMPLES) {
		if (i == 0) {
			return;			/* older than all kept */
		}
		memmove(&p->s[0], &p->s[1], sizeof(p->s[0]) * (WATCH_SAMPLES - 1));
		p->n--;
		i--;
	}
	memmove(&p->s[i + 1], &p->s[i], sizeof(p->s[0]) * (size_t)(p->n - i));
	p->s[i].t = t;
	p->s[i].tick = tick;
	p->s[i].st = *st;
	p->n++;
}

static void Ev_Create(void *ctx, int slot, int kind, const char *look, uint32_t tick, const NetRepState *st) {
	char name[32];
	(void)ctx;
	Watch_Free(slot);
	if (slot == Play_OwnSlot()) {
		Play_OwnCreate(kind, look, tick, st);
		return;
	}
	WatchProxy *p = &watch.px[slot];
	Str_PrintfQuiet(name, sizeof(name), WATCH_NAME, slot, ++watch.made);
	p->e = Watch_Build(name, kind, look, st, 1);
	p->kind = kind;
	if (p->e) {
		watch.proxies++;
		Watch_Sample(p, tick, st);
	}
}

static void Ev_Update(void *ctx, int slot, uint32_t tick, const NetRepState *st) {
	(void)ctx;
	if (slot == Play_OwnSlot()) {
		Play_OwnUpdate(tick, st);
	} else if (watch.px[slot].e) {
		Watch_Sample(&watch.px[slot], tick, st);
	}
}

static void Ev_Remove(void *ctx, int slot) {
	(void)ctx;
	if (slot == Play_OwnSlot()) {
		Play_OwnRemove();
	}
	Watch_MissileHit(&watch.px[slot], NULL, 1);
	Watch_Free(slot);
}

/* the player message: the own slot; a proxy already made of it becomes the own entity */
static void Watch_Player(const uint8_t *data, int size) {
	int player, slot, kind, k, look, nphys;
	uint32_t tick;
	float phys[NETREP_PHYS_MAX];

	if (!NetRep_ReadPlayer(data, size, &player, &slot, &kind, &tick, phys, &nphys)) {
		return;
	}
	Play_SetOwn(player, slot, kind, tick, phys, nphys);
	if (slot == NETREP_NO_SLOT) {
		return;
	}
	const NetRepState *st = NetRepCli_State(watch.view, slot, &k, &look);
	if (st && watch.px[slot].e) {
		Watch_Free(slot);
		Play_OwnCreate(k, NetRepCli_LookString(watch.view, look), NetRepCli_Tick(watch.view), st);
	}
}

void Watch_Adopt(int slot, cEntity *e, int kind) {
	int off = -1;
	if (slot < 0 || slot >= NETREP_MAX_ENTS || !e) {
		return;
	}
	Watch_Free(slot);
	WatchProxy *p = &watch.px[slot];
	p->e = e;
	p->kind = kind;
	Obj(e)->proxy = 1;
	Obj(e)->netStep = 0;
	Entity_VarI(e, "UsrControl", &off, 1);
	watch.proxies++;
}

byte Watch_IsPlayer(cEntity *e) {
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		WatchProxy *p = &watch.px[s];
		if (p->e == e) {
			return p->n && (p->s[p->n - 1].st.flags & NETREP_FL_PLAYER);
		}
	}
	return 0;
}

static float Watch_LerpAng(uint16_t a, uint16_t b, float f) {
	return NetRep_UAng(a) + (float)(int16_t)(uint16_t)(b - a) / NETREP_ANG_SCALE * f;
}

/* the weapon of a ship proxy and the press and release of its fire (the sounds of the instant-hit weapons; the
 * others do things a proxy must not: their missiles come replicated) */
static void Watch_Weapon(cEntity *e, const NetRepState *st) {
	cVehicleEntity *v = Veh(e);
	int w = st->weapon < g_pWorld->weapons.count ? st->weapon : v->weapons.curWeap;
	byte fire = (st->flags & NETREP_FL_FIRE) != 0;

	if (w != v->weapons.curWeap) {
		if (v->controls.firePrev && NetPl_LocalWeapon(v->weapons.curWeap)) {
			cPy_Function_AddParamS(&TABLE_AT(g_pWorld->weapons, v->weapons.curWeap)->onRelease, e->name);
			cPy_Function_Execute(&TABLE_AT(g_pWorld->weapons, v->weapons.curWeap)->onRelease, 0);
		}
		v->controls.firePrev = 0;
		v->weapons.curWeap = w;
		Controls_SetWeapon(&v->controls, w);
	}
	if (fire != v->controls.firePrev && NetPl_LocalWeapon(w)) {
		cPy_Function *f = fire ? &TABLE_AT(g_pWorld->weapons, w)->onPress : &TABLE_AT(g_pWorld->weapons, w)->onRelease;
		cPy_Function_AddParamS(f, e->name);
		cPy_Function_Execute(f, 0);
	}
	v->controls.fire = v->controls.firePrev = fire;
}

/* the proxy at server time rt */
/* the samples around server time rt (*a == *b past the newest), the part f of the way from a to b, the seconds ext
 * ahead of the newest by its velocity */
static void Watch_Pick(const WatchProxy *p, double rt, const WatchSample **pa, const WatchSample **pb, float *pf,
	float *pext) {
	float f = 0.0f, ext = 0.0f;
	int i = p->n - 1;
	while (i > 0 && p->s[i].t > rt) {
		i--;
	}
	const WatchSample *a = &p->s[i];
	const WatchSample *b = i + 1 < p->n ? &p->s[i + 1] : a;
	if (b != a) {
		/* an entity that stood still (no velocity, so no records) starts to move a snapshot before b; a jump faster
		 * than WATCH_JUMP_SPEED (a respawn, a teleport) is not swept across */
		byte still = !a->st.vel[0] && !a->st.vel[1] && !a->st.vel[2];
		double ta = still && a->t < b->t - WATCH_HOLD ? b->t - WATCH_HOLD : a->t;
		float dx = NetRep_UPos(b->st.pos[0] - a->st.pos[0]), dy = NetRep_UPos(b->st.pos[1] - a->st.pos[1]);
		float dz = NetRep_UPos(b->st.pos[2] - a->st.pos[2]);
		if (dx * dx + dy * dy + dz * dz > WATCH_JUMP_SPEED * WATCH_JUMP_SPEED * (float)((b->t - a->t) * (b->t - a->t))) {
			ta = b->t;
		}
		f = rt <= ta ? 0.0f : (float)((rt - ta) / (b->t - ta));
	} else if (rt > a->t && a->tick + WATCH_EXTRAP_TICKS >= NetRepCli_Tick(watch.view)) {
		/* ahead of the newest sample only while it is from the newest snapshots: an entity without a record since
		 * then has not changed (or waits for the budget) */
		ext = (float)(rt - a->t > WATCH_EXTRAP ? WATCH_EXTRAP : rt - a->t);
	}
	*pa = a;
	*pb = b;
	*pf = f;
	*pext = ext;
}

/* the position of the proxy at server time rt */
static void Watch_PosAt(const WatchProxy *p, double rt, vec3_t *pos) {
	const WatchSample *a, *b;
	float f, ext;
	Watch_Pick(p, rt, &a, &b, &f, &ext);
	for (int k = 0; k < 3; k++) {
		float pa = NetRep_UPos(a->st.pos[k]), v = (float)a->st.vel[k] + (float)(b->st.vel[k] - a->st.vel[k]) * f;
		(&pos->x)[k] = pa + (NetRep_UPos(b->st.pos[k]) - pa) * f + v * ext;
	}
}

/* the proxies (the players' and the other characters near the own entity, or only the players' with players) where
 * they are at
 * server time t (the own prediction of that tick collides with them there, not where they are drawn), and back to
 * the drawn places */
static vec3_t g_ProxiesDrawn[NETREP_MAX_ENTS];
static byte g_ProxiesMoved[NETREP_MAX_ENTS];
static byte g_AnyMoved;

void Watch_ProxiesAt(double t, byte players) {
	const cEntity *own = Play_Entity();
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		WatchProxy *p = &watch.px[s];
		if (!p->e || !p->n || p->kind == NETREP_MISSILE) {
			continue;
		}
		/* the ships of no player stay: the traffic follows its rails, its path ahead is a worse guess than where it
		 * is drawn (A/B in the M4b handback); the other characters only on request (players 0) */
		byte player = (p->s[p->n - 1].st.flags & NETREP_FL_PLAYER) != 0;
		if ((players || p->kind == NETREP_SHIP || p->kind == NETREP_ITEM) && !player) {
			continue;
		}
		if (!player && own && !g_ProxiesMoved[s] && Watch_DistSq(p->e, own) > WATCH_AHEAD_RADIUS * WATCH_AHEAD_RADIUS) {
			continue;
		}
		vec3_t pos;
		if (!g_ProxiesMoved[s]) {
			g_ProxiesDrawn[s] = p->e->pos;
			g_ProxiesMoved[s] = 1;
		}
		Watch_PosAt(p, t, &pos);
		Entity_SetPos(p->e, &pos);
		g_AnyMoved = 1;
	}
}

void Watch_ProxiesBack(void) {
	if (!g_AnyMoved) {
		return;
	}
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		if (g_ProxiesMoved[s]) {
			if (watch.px[s].e) {
				Entity_SetPos(watch.px[s].e, &g_ProxiesDrawn[s]);
			}
			g_ProxiesMoved[s] = 0;
		}
	}
	g_AnyMoved = 0;
}

/* the proxy at server time rt */
static void Watch_Place(int slot, WatchProxy *p, double rt, float dt) {
	cEntity *e = p->e;
	cWithLifeEntity *w = WLife(e);
	cObjEntity *o = Obj(e);
	vec3_t pos, ang, vel, rot = { 0.0f, 0.0f, 0.0f };
	float rates[NETREP_LAYERS], f, ext;
	uint16_t act[NETREP_LAYERS];
	const WatchSample *a, *b;

	Watch_Pick(p, rt, &a, &b, &f, &ext);
	const NetRepState *sa = &a->st, *sb = &b->st;
	if (p->kind == NETREP_ITEM) {
		vec3_t at = { NetRep_UPos(sb->pos[0]), NetRep_UPos(sb->pos[1]), NetRep_UPos(sb->pos[2]) };
		vec3_t an = { NetRep_UAng(sb->ang[0]), NetRep_UAng(sb->ang[1]), NetRep_UAng(sb->ang[2]) };
		if (at.x != e->pos.x || at.y != e->pos.y || at.z != e->pos.z) {
			Entity_SetPos(e, &at);
			Obj_SetAng(e, &an);
		}
		if (((sa->flags & NETREP_FL_ACTIVE) != 0) != e->active) {
			Entity_SetActive(e, (sa->flags & NETREP_FL_ACTIVE) != 0);
		}
		return;
	}
	float *pv = &pos.x, *av = &ang.x, *vv = &vel.x, *rv = &rot.x;
	for (int k = 0; k < 3; k++) {
		float pa = NetRep_UPos(sa->pos[k]);
		vv[k] = (float)sa->vel[k] + (float)(sb->vel[k] - sa->vel[k]) * f;
		pv[k] = pa + (NetRep_UPos(sb->pos[k]) - pa) * f + vv[k] * ext;
		av[k] = Watch_LerpAng(sa->ang[k], sb->ang[k], f);
		if (b != a) {
			rv[k] = (float)(int16_t)(uint16_t)(sb->ang[k] - sa->ang[k]) / NETREP_ANG_SCALE / (float)(b->t - a->t);
		}
	}
	Entity_SetPos(e, &pos);
	o->slotPos = pos;
	o->renderPos = pos;
	o->ang = ang;
	o->slotAng = ang;
	o->renderAng = ang;
	o->vel = vel;
	o->rotVel = rot;
	byte on = (sa->flags & NETREP_FL_ACTIVE) != 0;
	if (p->kind == NETREP_MISSILE) {
		/* the newest sample tells the hits at once (the effects of a hit wait for no delay) */
		const NetRepState *sn = &p->s[p->n - 1].st;
		Watch_MissileHit(p, sn, !(sn->flags & NETREP_FL_ACTIVE));
		on = on && e->active;
	}
	if (on != e->active && p->kind == NETREP_SHIP && !g_pWorld->weapons.count) {
		/* a ship of an indoor level (no weapons there): the vehicle's stop of its weapons would read the empty
		 * weapon table */
		cWithLifeEntity_SetActive(e, on);
	} else if (on != e->active) {
		Entity_SetActive(e, on);
	}
	if (p->kind == NETREP_MISSILE) {
		/* a missile proxy moves with the samples; its bees follow it in its update */
	} else if (p->kind == NETREP_SHIP) {
		w->life = sa->life;
		cVehicleEntity *v = (cVehicleEntity *)e;
		/* the proxy knows no maximum: the highest life seen */
		if (v->maxLife < w->life) {
			v->maxLife = w->life;
		}
		v->controls.acc = sa->ctrl[0] / 127.0f;
		v->controls.str = sa->ctrl[1] / 127.0f;
		v->speed = sqrtf(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
		Watch_Weapon(e, sa);
		Faction_EntityFlags(e, p->s[p->n - 1].st.flags);
	} else {
		cWalkCharEntity *c = WChar(e);
		w->life = sa->life;
		c->view.x = Watch_LerpAng(sa->view[0], sb->view[0], f);
		c->view.y = Watch_LerpAng(sa->view[1], sb->view[1], f);
		c->view.z = 0.0f;
		c->slotView = c->view;
		c->renderView = c->view;
		c->viewVel = (vec3_t){ 0.0f, 0.0f, 0.0f };
		c->falling = (sa->flags & NETREP_FL_FALLING) != 0;
		c->walking = (sa->flags & NETREP_FL_WALKING) != 0;
		/* the action of the sample before; its rate from the time it went on between the two samples */
		for (int l = 0; l < NETREP_LAYERS; l++) {
			act[l] = sa->act[l];
			float ta = sa->actTime[l] / 1000.0f, tb = sb->actTime[l] / 1000.0f;
			rates[l] = b != a && sb->act[l] == sa->act[l] && tb > ta ? (tb - ta) / (float)(b->t - a->t) : 0.0f;
			rates[l] = rates[l] > 4.0f ? 4.0f : rates[l];
		}
		if (w->actCtrl && w->actCtrl->cls) {
			cActCtrl_NetDrive(w->actCtrl, act, rates, NETREP_LAYERS, dt);
		}
	}
	if (watch.log && rt >= p->s[0].t) {
		fprintf(watch.log, "%.4f,%d,%.2f,%.2f,%.2f\n", rt, slot, (double)pos.x, (double)pos.y, (double)pos.z);
	}
}

byte Watch_ArenaWorld(void) {
	return (watch.welcome & REP_WELCOME_ARENA) != 0;
}

void Watch_RailKill(void) {
	uint8_t m = REP_MSG_RAILKILL;
	Watch_SendReliable(&m, 1);
}

/* characters and ships the level scripts made: the instance has the real ones (not the client's own rail traffic) */
static void Watch_Purge(void) {
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e->deadNode.target || (!Entity_IsType(e, TYEN_CAR) && !Entity_IsType(e, TYEN_WALKCHAR)) ||
			e == Play_Entity() || Rep_LocalAmbient(e)) {
			continue;
		}
		if (!Obj(e)->proxy) {
			cEntity_Del(e);
		}
	}
}

/* ---- the camera ---- */

static void Watch_SetView(const float *pos, float yaw, float pitch) {
	CGameCamera *c = &((cCamControl *)watch.cam)->cam;
	c->pos = (vec3_t){ pos[0], pos[1], pos[2] };
	c->rot = (vec3_t){ pitch * M_PI_F / 180.0f, yaw * M_PI_F / 180.0f, 0.0f };
}

static void Watch_MakeCam(void) {
	int one = 1;
	float p[3] = { watch.start[0], watch.start[1] + 800.0f, watch.start[2] };

	watch.cam = World_FindEntity(WATCH_CAM_NAME);
	if (!watch.cam) {
		watch.cam = cWorld_CreateEntity(g_pWorld, WATCH_CAM_NAME, p[0], p[1], p[2], TYEN_FCAM);
	}
	if (!watch.cam) {
		return;
	}
	cWorld_SetCam(g_pWorld, 0, WATCH_CAM_NAME);
	Entity_VarI(watch.cam, "FreeCam", &one, 1);
	Watch_SetView(p, 0.0f, -10.0f);
}

/* ---- the connection ---- */

static void Watch_Start(void) {
	Net2Config cfg;
	Net2Addr *addr = &watch.server;
	int err = 0;

	watch.phase = WATCH_GONE;
	NetRepClock_Init(&watch.clock);
	NetRepDelay_Init(&watch.delay);
	NetRepFollow_Init(&watch.follow, WATCH_FOLLOW_JUMP);
	if (!Net2_ParseAddr(watch.addr, watch.port ? watch.port : Instance_Port(), addr)) {
		Con_Errorf("watch: '%s' is not a numeric address\n", watch.addr);
		return;
	}
	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = INSTANCE_APP_VERSION;
	cfg.ipv4 = addr->family == NET2_AF_IPV4;
	cfg.ipv6 = addr->family == NET2_AF_IPV6;
	cfg.connectTimeout = WATCH_CONNECT_MS;
	watch.host = Net2_Create(&cfg, &err);
	watch.view = NetRepCli_Create();
	if (watch.host) {
		Instance_TestLink(watch.host, "watch");
	}
	if (!watch.host || !watch.view || Net2_Connect(watch.host, addr, Plat_RealNs() / 1000000) != NET2_OK) {
		Con_Errorf("watch: no Net2 client (error %d)\n", err);
		return;
	}
	const char *path = getenv("SCRAP_REPLOG");
	if (path && *path) {
		watch.log = fopen(path, "w");
		if (watch.log) {
			fputs("time,slot,x,y,z\n", watch.log);
		}
	}
	watch.phase = WATCH_CONNECTING;
	Con_Printf("watch: connecting to %s\n", watch.addr);
}

static void Watch_Welcome(const uint8_t *data, int size) {
	uint32_t tick;

	if (!NetRep_ReadWelcome(data, size, &tick, watch.start, watch.level, sizeof(watch.level), &watch.welcome)) {
		Con_Errorf("watch: bad welcome\n");
		return;
	}
	NetRepClock_Sample(&watch.clock, tick, Watch_Now(), watch.rtt);
	watch.phase = WATCH_LEVEL;
	Con_Printf("watch: level %s, tick %u\n", watch.level, tick);
	if (!g_pApp->levelPath || crt_stricmp(g_pApp->levelPath, watch.level)) {
		CWinApp_LoadLevel(g_pApp, watch.level);
	}
}

static void Watch_Snapshot(const uint8_t *data, int size) {
	NetRepEvents ev = { NULL, Ev_Create, Ev_Update, Ev_Remove };

	watch.msgs++;
	watch.bytes += (uint64_t)size;
	if (!NetRepCli_Message(watch.view, data, size, &ev)) {
		watch.notTaken++;
		return;
	}
	watch.taken = 1;
	double now = Watch_Now();
	NetRepDelay_Sample(&watch.delay, &watch.clock, NetRepCli_Tick(watch.view), now, watch.rtt);
	NetRepClock_Sample(&watch.clock, NetRepCli_Tick(watch.view), now, watch.rtt);
}

/* the static records of the instance (REP_MSG_STATICS): the things its level's start made that this game's start did
 * not, made here to be seen (no physics, no uses: the instance has them) */
static void Watch_Records(const uint8_t *data, int size) {
	NetRepReader r;
	char name[REP_RECORD_NAME], cls[32], model[256], act[64];

	NetRepReader_Init(&r, data, size);
	NetRep_GetU8(&r);
	int n = (int)NetRep_GetU8(&r);
	for (int i = 0; i < n && !r.bad; i++) {
		NetRep_GetStr(&r, name, sizeof(name));
		NetRep_GetStr(&r, cls, sizeof(cls));
		NetRep_GetStr(&r, model, sizeof(model));
		NetRep_GetStr(&r, act, sizeof(act));
		vec3_t pos, ang;
		pos.x = NetRep_UPos(NetRep_GetSVar(&r));
		pos.y = NetRep_UPos(NetRep_GetSVar(&r));
		pos.z = NetRep_UPos(NetRep_GetSVar(&r));
		ang.x = NetRep_UAng((uint16_t)NetRep_GetU16(&r));
		ang.y = NetRep_UAng((uint16_t)NetRep_GetU16(&r));
		ang.z = NetRep_UAng((uint16_t)NetRep_GetU16(&r));
		int on = (int)NetRep_GetU8(&r);
		if (r.bad || !name[0] || World_FindEntity(name)) {
			continue;
		}
		const char *type = !strcmp(cls, "item") ? "item" : !strcmp(cls, "Obj") ? "Obj" :
			!strcmp(cls, TYEN_WITH_LIFE) ? TYEN_WITH_LIFE : NULL;
		if (!type || (model[0] && !Watch_ModelExists(model))) {
			continue;
		}
		cEntity *e = cWorld_CreateEntity(g_pWorld, name, pos.x, pos.y, pos.z, type);
		if (!e) {
			continue;
		}
		Obj(e)->proxy = 1;
		if (act[0] && Entity_IsType(e, TYEN_WITH_LIFE)) {
			WithLife_SetActCtrl(e, act);
		}
		if (model[0] && !CGameModel_IsLoaded(&Obj(e)->model)) {
			Entity_SetModel(e, model, 1.0f, 1.0f, 1.0f, 0);
		}
		Obj_SetAng(e, &ang);
		Entity_SetActive(e, on == REP_STATIC_ON);
		watch.records++;
	}
}

/* their changes (REP_MSG_STATICSET): on, off, gone */
static void Watch_RecordSet(const uint8_t *data, int size) {
	NetRepReader r;
	char name[REP_RECORD_NAME];

	NetRepReader_Init(&r, data, size);
	NetRep_GetU8(&r);
	int n = (int)NetRep_GetU8(&r);
	for (int i = 0; i < n && !r.bad; i++) {
		NetRep_GetStr(&r, name, sizeof(name));
		int st = (int)NetRep_GetU8(&r);
		cEntity *e = r.bad ? NULL : World_FindEntity(name);
		if (!e || Entity_IsType(e, TYEN_CAR) || Entity_IsType(e, TYEN_WALKCHAR)) {
			continue;
		}
		if (st == REP_STATIC_GONE) {
			cEntity_Del(e);
		} else if (e->active != (st == REP_STATIC_ON)) {
			Entity_SetActive(e, st == REP_STATIC_ON);
		}
	}
}

/* the shots of the instance's ships: their effects on the proxies (the own ship's of a NetPl_LocalWeapon do not
 * come, the client showed them itself) */
static void Watch_Shots(const uint8_t *data, int size) {
	NetRepShot shots[NETREP_SHOTS_MAX];
	uint32_t tick;
	int n;

	if (!NetRep_ReadShots(data, size, &tick, shots, NETREP_SHOTS_MAX, &n)) {
		return;
	}
	for (int i = 0; i < n; i++) {
		const NetRepShot *x = &shots[i];
		cEntity *e = x->slot == Play_OwnSlot() ? Play_Entity() : watch.px[x->slot].e;
		if (!e || !Entity_IsType(e, TYEN_VEHICLE)) {
			continue;
		}
		cEntity *t = NULL;
		if (x->target != NETREP_NO_SLOT && x->target < NETREP_MAX_ENTS) {
			t = x->target == Play_OwnSlot() ? Play_Entity() : watch.px[x->target].e;
		}
		vec3_t from = { NetRep_UPos(x->from[0]), NetRep_UPos(x->from[1]), NetRep_UPos(x->from[2]) };
		vec3_t to = { NetRep_UPos(x->pos[0]), NetRep_UPos(x->pos[1]), NetRep_UPos(x->pos[2]) };
		/* the push of a hit on the own entity is in the server's states already (a correction brings it) */
		cEntity *own = t && t == Play_Entity() ? t : NULL;
		vec3_t vel = own ? Obj(own)->vel : to, rot = own ? Obj(own)->rotVel : to;
		cVehicleAttack_Replay(&WLife(e)->attack, x->weapon, x->count, &from, &to, t);
		if (own) {
			Obj(own)->vel = vel;
			Obj(own)->rotVel = rot;
		}
		watch.shots++;
	}
}

/* the station map of a metro door the own player used: the map of single player (its pick goes to the instance:
 * Watch_MetroPick), or for the tests the first station of the list SCRAP_PLAYBOT_METRO that it offers */
static void Watch_Metro(const uint8_t *data, int size) {
	char door[COORD_LEVEL + 1], station[COORD_LEVEL + 1];
	NetRepReader r;
	const char *want = getenv("SCRAP_PLAYBOT_METRO");

	NetRepReader_Init(&r, data + 1, size - 1);
	NetRep_GetStr(&r, door, sizeof(door));
	int n = (int)NetRep_GetU8(&r);
	char offered[512] = ",";
	int len = 1;
	for (int k = 0; k < n && !r.bad; k++) {
		NetRep_GetStr(&r, station, sizeof(station));
		len += Str_PrintfQuiet(offered + len, (int)sizeof(offered) - len, "%s,", station);
		len = len < (int)sizeof(offered) ? len : (int)sizeof(offered) - 1;
	}
	if (r.bad || !Play_Entity()) {
		return;
	}
	Con_Printf("watch: metro map of %s (%d stations)\n", door, n);
	if (want && *want) {
		/* the first of the comma list that the map offers */
		char list[256], key[COORD_LEVEL + 3];
		Str_Copy(list, want, sizeof(list));
		for (char *tok = strtok(list, ","); tok; tok = strtok(NULL, ",")) {
			Str_PrintfQuiet(key, sizeof(key), ",%s,", tok);
			if (strstr(offered, key)) {
				Watch_MetroPick(tok);
				break;
			}
		}
		return;
	}
	if (!g_HeadlessPlay && !PORT_CALL(Metro, OnUse, NULL, "ssss", Play_Entity()->name, door, "Travel", "")) {
		Native_ErrPrint();
	}
}

byte Watch_MetroPick(const char *station) {
	uint8_t buf[COORD_LEVEL + 8];
	NetRepBuf w;
	if (!watch.host || watch.phase != WATCH_RUN) {
		return 0;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_PICK);
	NetRep_PutStr(&w, station);
	Net2_Send(watch.host, 0, REP_CH_RELIABLE, buf, w.size);
	Con_Printf("watch: metro to %s\n", station);
	return 1;
}

byte Watch_SendReliable(const uint8_t *data, int size) {
	if (!watch.host || watch.phase != WATCH_RUN) {
		return 0;
	}
	Net2_Send(watch.host, 0, REP_CH_RELIABLE, data, size);
	return 1;
}

/* the connection to the instance and everything of it gone (a travel to another instance) */
static void Watch_Reset(void) {
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		Watch_Free(s);
	}
	Play_Reset();
	Faction_ClientReset();
	Coop_ClientReset();
	Arena_ClientReset();
	if (watch.host) {
		Net2_Destroy(watch.host);
		watch.host = NULL;
	}
	NetRepCli_Destroy(watch.view);
	watch.view = NULL;
	watch.cam = NULL;
	watch.welcome = 0;
	watch.lastNs = watch.lastAck = 0;
	watch.retries = 0;
	watch.taken = 0;
	watch.phase = WATCH_OFF;
}

void Watch_Stop(void) {
	Watch_Reset();
	watch.target = 0;
	watch.ticketLen = 0;
	watch.port = 0;
	Net2_Wipe(watch.ticket, sizeof(watch.ticket));
}

byte Watch_Alive(void) {
	return (watch.phase != WATCH_OFF && watch.phase != WATCH_GONE) || (watch.phase == WATCH_OFF && watch.target);
}

void Watch_Go(const char *addr, uint16_t port, const uint8_t *ticket, int size, const char *level) {
	char path[COORD_LEVEL + 8];
	if (size < 0 || size > (int)sizeof(watch.ticket)) {
		return;
	}
	if (watch.phase != WATCH_OFF) {
		Watch_Reset();
	}
	Str_Copy(watch.addr, addr, sizeof(watch.addr));
	watch.port = port;
	memcpy(watch.ticket, ticket, (size_t)size);
	watch.ticketLen = size;
	watch.target = 1;
	/* the loading screen at once (the welcome finds the level loading or loaded) */
	Str_PrintfQuiet(path, sizeof(path), "Levels/%s", level);
	if (!g_pApp->levelPath || crt_stricmp(g_pApp->levelPath, path)) {
		CWinApp_LoadLevel(g_pApp, path);
	}
	Con_Printf("watch: to %s on %s port %u\n", level, addr, port);
}

void Watch_Idle(void) {
	if (watch.host) {
		uint64_t ms = Plat_RealNs() / 1000000;
		Net2_Poll(watch.host, ms);
		Net2_Flush(watch.host, ms);
	}
}

byte Watch_ClockReady(void) {
	return watch.phase == WATCH_RUN && watch.clock.have;
}

double Watch_InputTime(double now, double rtt, double lead) {
	return NetRepClock_Server(&watch.clock, now) + rtt * 0.5 + lead;
}

double Watch_ShownTime(void) {
	return watch.shown;
}

void Watch_PreFrame(void) {
	Net2Event ev;
	uint64_t ns = Plat_RealNs();

	if (watch.phase == WATCH_OFF) {
		if (Join_Active() && !watch.target) {
			return;			/* the coordinator's ticket first */
		}
		Watch_Start();
	}
	if (!watch.host) {
		return;
	}
	Net2_Poll(watch.host, ns / 1000000);
	while (Net2_Receive(watch.host, &ev)) {
		switch (ev.type) {
		case NET2_EV_CONNECT: {
			uint8_t buf[16];
			/* the coordinator's ticket before the hello */
			if (watch.ticketLen) {
				static uint8_t tk[COORD_TICKET_MAX + 3];
				tk[0] = COORD_MSG_ITICKET;
				tk[1] = (uint8_t)watch.ticketLen;
				tk[2] = (uint8_t)(watch.ticketLen >> 8);
				memcpy(tk + 3, watch.ticket, (size_t)watch.ticketLen);
				Net2_Send(watch.host, 0, REP_CH_RELIABLE, tk, watch.ticketLen + 3);
			}
			int n = NetRep_WriteHello(buf, sizeof(buf), watch.rate ? watch.rate : NETREP_RATE_DEFAULT,
				g_Play ? NETREP_HELLO_PLAY : 0);
			Net2_Send(watch.host, 0, REP_CH_RELIABLE, buf, n);
			watch.phase = WATCH_HELLO;
			Con_Printf("watch: connected\n");
			break;
		}
		case NET2_EV_DISCONNECT:
			Con_Printf("watch: disconnected (%s)\n", Net2_ReasonString(ev.reason));
			if (watch.phase == WATCH_CONNECTING && watch.retries++ < WATCH_RETRIES &&
				Net2_Connect(watch.host, &watch.server, ns / 1000000) == NET2_OK) {
				Con_Printf("watch: connecting again\n");
				break;
			}
			watch.phase = WATCH_GONE;
			break;
		case NET2_EV_MESSAGE:
			if (ev.size < 1) {
				break;
			}
			if (ev.channel == REP_CH_RELIABLE && ev.data[0] == NETREP_MSG_WELCOME) {
				Watch_Welcome(ev.data, ev.size);
			} else if (ev.channel == REP_CH_RELIABLE && ev.data[0] == NETREP_MSG_PLAYER) {
				Watch_Player(ev.data, ev.size);
			} else if (ev.channel == REP_CH_RELIABLE && ev.data[0] == COORD_MSG_METRO) {
				Watch_Metro(ev.data, ev.size);
			} else if (ev.channel == REP_CH_RELIABLE && ev.data[0] == FACTION_MSG_STATUS) {
				Faction_ClientMessage(ev.data, ev.size);
			} else if (ev.channel == REP_CH_RELIABLE && ev.data[0] == COOP_MSG) {
				Coop_ClientMessage(ev.data, ev.size);
			} else if (ev.channel == REP_CH_RELIABLE && ev.data[0] == ARENA_MSG) {
				Arena_ClientMessage(ev.data, ev.size);
			} else if (ev.channel == REP_CH_RELIABLE && ev.data[0] == REP_MSG_STATICS && watch.phase == WATCH_RUN) {
				Watch_Records(ev.data, ev.size);
			} else if (ev.channel == REP_CH_RELIABLE && ev.data[0] == REP_MSG_STATICSET && watch.phase == WATCH_RUN) {
				Watch_RecordSet(ev.data, ev.size);
			} else if (ev.channel == REP_CH_UNRELIABLE && ev.data[0] == NETREP_MSG_SNAP && watch.phase == WATCH_RUN) {
				Watch_Snapshot(ev.data, ev.size);
			} else if (ev.channel == REP_CH_UNRELIABLE && ev.data[0] == NETREP_MSG_SHOTS && watch.phase == WATCH_RUN) {
				Watch_Shots(ev.data, ev.size);
			} else if (ev.channel == REP_CH_UNRELIABLE && ev.data[0] == NETREP_MSG_STATUS && watch.phase == WATCH_RUN) {
				NetRepStatus st;
				if (NetRep_ReadStatus(ev.data, ev.size, &st)) {
					Play_OwnStatus(&st);
				}
			}
			break;
		}
	}
	if (watch.phase == WATCH_LEVEL && !g_pApp->loadPending && g_pApp->levelPath &&
		!crt_stricmp(g_pApp->levelPath, watch.level)) {
		watch.phase = WATCH_RUN;
		if (g_Play) {
			Play_Start();
		} else {
			Watch_MakeCam();
		}
		Con_Printf("watch: level running\n");
	}
	if (watch.phase != WATCH_RUN) {
		watch.lastNs = ns;
		return;
	}
	Watch_Purge();
	float dt = watch.lastNs ? (float)((double)(ns - watch.lastNs) / 1e9) : 0.0f;
	watch.lastNs = ns;
	/* the proxies at the time of the newest samples (half a round trip ago) minus the interpolation delay */
	double now = (double)ns / 1e9;
	double delay = NetRepDelay_Step(&watch.delay, dt, watch.loss);
	double rt = NetRepFollow_Step(&watch.follow, NetRepClock_Server(&watch.clock, now) - watch.rtt * 0.5 - delay, dt);
	watch.shown = rt;
	if (g_Play) {
		Play_PreFrame(now, watch.rtt, watch.rttVar, watch.loss);
	}
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		if (watch.px[s].e && watch.px[s].n) {
			Watch_Place(s, &watch.px[s], rt, dt);
		}
	}
	if (ns / 1000000 - watch.lastStatus >= WATCH_STATUS_MS) {
		const vec3_t *c = watch.cam ? &((cCamControl *)watch.cam)->cam.pos : NULL;
		Con_Printf("watch: %u proxies, %u records, %u messages %llu bytes, %u not taken, %u shots, server time %.2f, "
			"delay %.0f ms (jitter %.1f ms), rtt %.0f ms, view %.0f %.0f %.0f\n", watch.proxies, watch.records, watch.msgs,
			(unsigned long long)watch.bytes, watch.notTaken, watch.shots, rt, watch.delay.delay * 1000.0,
			watch.delay.dev * 1000.0, watch.rtt * 1000.0, c ? (double)c->x : 0.0, c ? (double)c->y : 0.0,
			c ? (double)c->z : 0.0);
		Play_Status();
		watch.lastStatus = ns / 1000000;
	}
}

void Watch_PostFrame(void) {
	uint64_t ns = Plat_RealNs(), ms = ns / 1000000;
	uint16_t newest;
	uint32_t bits, shown;
	uint8_t buf[128];
	NetRepInput in[NETREP_INPUT_REDUNDANCY];
	Net2Stats st;
	int n;

	if (!watch.host) {
		return;
	}
	if (Net2_GetStats(watch.host, 0, &st) && st.rtt) {
		watch.rtt = st.rtt / 1000.0;
		watch.rttVar = st.rttVar / 1000.0;
		watch.loss = st.loss;
	}
	if (g_Play && watch.phase == WATCH_RUN) {
		Play_PostFrame(watch.lastNs ? (float)((double)(ns - watch.lastNs) / 1e9) : 0.0f);
	}
	/* a player's inputs once per tick, with the acks */
	if (g_Play && watch.phase == WATCH_RUN && Play_TakeInputs(in, &n, &shown)) {
		float v[3] = { 0.0f, 0.0f, 0.0f };
		if (!NetRepCli_Ack(watch.view, &newest, &bits)) {
			newest = 0;
			bits = 0;
		}
		int size = NetRep_WriteAckInput(buf, sizeof(buf), newest, bits, v, in, n, shown);
		if (size > 0) {
			Net2_Send(watch.host, 0, REP_CH_UNRELIABLE, buf, size);
		}
		watch.taken = 0;
		watch.lastAck = ms;
	} else if (watch.phase == WATCH_RUN && (watch.taken || ms - watch.lastAck >= WATCH_ACK_MS) &&
		NetRepCli_Ack(watch.view, &newest, &bits)) {
		float v[3] = { 0.0f, 0.0f, 0.0f };
		if (watch.cam) {
			const vec3_t *p = &((cCamControl *)watch.cam)->cam.pos;
			v[0] = p->x;
			v[1] = p->y;
			v[2] = p->z;
		}
		int size = NetRep_WriteAck(buf, sizeof(buf), newest, bits, v);
		Net2_Send(watch.host, 0, REP_CH_UNRELIABLE, buf, size);
		watch.taken = 0;
		watch.lastAck = ms;
	}
	Net2_Flush(watch.host, ms);
}

void Watch_Close(void) {
	if (g_Play) {
		Play_Close();
	}
	if (watch.host) {
		Net2_Destroy(watch.host);
		watch.host = NULL;
	}
	NetRepCli_Destroy(watch.view);
	watch.view = NULL;
	if (watch.log) {
		fclose(watch.log);
		watch.log = NULL;
	}
}
