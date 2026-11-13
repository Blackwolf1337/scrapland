/*
 * The replication of the instance's world to its watchers and players, see include/scrap/replicate.h,
 * include/scrap/play.h and include/scrap/netrep.h. Not in the original.
 *
 * Every snapshot walks the world's entities: the ships and characters that are not deleted get the slot of the last
 * snapshot back (found by pointer and name, with the same look), else a free one. The sample time of an object is
 * the end of its current slot (cObjEntity.slot): its render position reaches pos there (cObjEntity_Update lerps from
 * slotPos to pos over the slot), so a watcher that draws the samples at their times draws what this world draws.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/model.h"
#include "scrap/node3d.h"
#include "scrap/netrep.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scriptnative.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

#define REP_CONNS 1024				/* NET2_MAX_CONNS */
#define REP_HASH 4096				/* pointer table, twice the slots */
#define REP_NAME 64
#define REP_RESPAWN_TICKS (3 * NETREP_TICK_HZ)	/* a player without an entity gets a new one after this */
#define REP_REWIND_MAX 0.5					/* s the lag compensation goes back at most */
#define REP_CHAR_CLASS "Dtritus"

typedef struct RepEnt {
	cEntity *e;						/* NULL: the slot has no entity of this world */
	char name[REP_NAME];
	int look;
	uint32_t gen;					/* entities the slot had (SCRAP_REPLOG) */
} RepEnt;

typedef struct RepConn {
	NetRepClient *c;
	int player;						/* its player, -1 for a watcher */
	NetRepClientStats last;			/* at the last status line */
} RepConn;

typedef struct RepPlayer {
	byte used;
	int conn;
	cEntity *own;					/* the character or ship it was given (maybe freed: compared, read only
									   while World_FindEntity(ownName) is it) */
	char ownName[REP_NAME];
	int told, toldKind;				/* the replication slot and kind of it the client was told, -1 */
	NetRepInBuf in;
	uint32_t shown[NETREP_INBUF];	/* by tick: the server time (ms) the client showed the others at */
	uint32_t shownNow;				/* of the input of this tick */
	uint16_t buttons;				/* of the input of the last tick */
	uint64_t lost;					/* the tick its entity went missing, 0 */
	uint32_t shots, hits, boards, leaves, spawns;
	uint32_t stale;					/* inputs for an entity it no longer has */
	NetRepStatus status;			/* the last status sent */
	uint32_t statusSnap;			/* the snapshot it went with */
	double rewindSum;
	uint32_t rewindN;
	NetRepInBuf last;				/* the counters at the last status line */
} RepPlayer;

typedef struct RepRewind {
	cEntity *e;
	vec3_t pos;
} RepRewind;

typedef struct RepShot {
	cEntity *shooter, *target;
	uint8_t weapon, count;
	vec3_t from, to;
} RepShot;

#define REP_SHOTS 512				/* hits of the weapons kept between two snapshots */

static struct {
	NetRepServer *srv;
	RepEnt ent[NETREP_MAX_ENTS];
	int16_t hash[REP_HASH];			/* slot by pointer, -1 empty */
	RepConn conn[REP_CONNS];
	RepPlayer pl[PLAYERS_MAX];
	int watchers, players;
	uint64_t tick;					/* of the frame run or running */
	byte haveSpawn;
	vec3_t spawn;					/* the players' spawn point and its yaw (radians) */
	float spawnYaw;
	RepRewind rewind[NETREP_MAX_ENTS];
	int rewound;
	RepShot shots[REP_SHOTS];
	int nshots;
	uint32_t shotsLost;				/* more than REP_SHOTS between two snapshots */
	NetRepShot wire[REP_SHOTS];		/* the shots of the snapshot with their slots */
	float wirePos[REP_SHOTS][3];	/* where the shooter is */
	uint32_t snapTick;				/* snapshots so far: tick * SNAP_HZ / TICK_HZ */
	uint64_t gatherNs, buildNs, gatherMax, buildMax;
	uint32_t snaps;
	FILE *log;
	byte logTried;
	uint8_t msgs[NETREP_MAX_MSGS][NETREP_MSG_MAX];
} rep;

/* ---- the entities ---- */

static uint32_t Rep_PtrHash(const void *p) {
	uintptr_t v = (uintptr_t)p;
	v ^= v >> 17;
	v *= (uintptr_t)0x9e3779b97f4a7c15ull;
	return (uint32_t)(v >> 7) & (REP_HASH - 1);
}

static void Rep_HashBuild(void) {
	for (int i = 0; i < REP_HASH; i++) {
		rep.hash[i] = -1;
	}
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		if (rep.ent[s].e) {
			uint32_t h = Rep_PtrHash(rep.ent[s].e);
			while (rep.hash[h] >= 0) {
				h = (h + 1) & (REP_HASH - 1);
			}
			rep.hash[h] = (int16_t)s;
		}
	}
}

/* the slot of the entity of the last snapshot (same pointer and name), -1 */
static int Rep_Find(const cEntity *e) {
	if (!rep.srv) {
		return -1;
	}
	for (uint32_t h = Rep_PtrHash(e);; h = (h + 1) & (REP_HASH - 1)) {
		int s = rep.hash[h];
		if (s < 0) {
			return -1;
		}
		if (rep.ent[s].e == e && !strncmp(rep.ent[s].name, e->name, REP_NAME - 1)) {
			return s;
		}
	}
}

static int Rep_Kind(cEntity *e) {
	if (e->deadNode.target) {
		return 0;
	}
	if (Entity_IsType(e, TYEN_CAR)) {
		return NETREP_SHIP;
	}
	if (Entity_IsType(e, TYEN_MISSILE)) {
		return NETREP_MISSILE;
	}
	return Entity_IsType(e, TYEN_WALKCHAR) ? NETREP_CHAR : 0;
}

/* the path the model of an object was loaded from (CGameModel_Name is only its file name), "" */
static const char *Rep_ModelPath(cEntity *e) {
	CGameModel *m = &Obj(e)->model;
	const char *path = CGameModel_IsLoaded(m) && m->Model->scene ? m->Model->scene->name : "";
	return path ? path : "";
}

static const char *Rep_VarS(cEntity *e, const char *name) {
	char *s = NULL;
	return Entity_VarS(e, name, &s, 0) && s ? s : "";
}

/* how the watcher builds the proxy: S|model|pilot|motor 0..3|class|weapon slots, C|class or M|model */
static void Rep_Look(cEntity *e, int kind, char *out, int size) {
	if (kind == NETREP_MISSILE) {
		Str_PrintfQuiet(out, size, "M|%s", Rep_ModelPath(e));
		return;
	}
	cWithLifeEntity *w = WLife(e);
	const char *cls = w->actCtrl && w->actCtrl->cls ? w->actCtrl->cls->name : "";

	if (kind == NETREP_CHAR) {
		Str_PrintfQuiet(out, size, "C|%s", cls);
		return;
	}
	Str_PrintfQuiet(out, size, "S|%s|%s|%s|%s|%s|%s|%s|%s", Rep_ModelPath(e),
		Rep_VarS(e, "MPil0"), Rep_VarS(e, "MEng0"), Rep_VarS(e, "MEng1"), Rep_VarS(e, "MEng2"), Rep_VarS(e, "MEng3"),
		cls, cWeaponManager_GetWeapList(&Veh(e)->weapons));
}

/* the index + 1 of the action in its class, 0 none */
static uint16_t Rep_ActIndex(const cActClass *cls, const cAction *a) {
	uint16_t i = 1;
	for (const cAction *x = cls->first; x; x = x->next, i++) {
		if (x == a) {
			return i;
		}
	}
	return 0;
}

static float Rep_SampleDt(cEntity *e) {
	cSlotController *sc = Obj(e)->slot;
	/* a player's entity is stepped at the tick (NetPl_Step) */
	if (Obj(e)->netStep) {
		return 0.0f;
	}
	float t = sc ? sc->time : 0.0f;
	return t < 0.0f ? 0.0f : t;
}

static void Rep_State(cEntity *e, int kind, NetRepState *s) {
	cWithLifeEntity *w = WLife(e);
	cObjEntity *o = Obj(e);

	memset(s, 0, sizeof(*s));
	s->pos[0] = NetRep_QPos(e->pos.x);
	s->pos[1] = NetRep_QPos(e->pos.y);
	s->pos[2] = NetRep_QPos(e->pos.z);
	s->vel[0] = NetRep_QVel(o->vel.x);
	s->vel[1] = NetRep_QVel(o->vel.y);
	s->vel[2] = NetRep_QVel(o->vel.z);
	s->ang[0] = NetRep_QAng(o->ang.x);
	s->ang[1] = NetRep_QAng(o->ang.y);
	s->ang[2] = NetRep_QAng(o->ang.z);
	float dt = Rep_SampleDt(e) * 1000.0f;
	s->dt = (uint16_t)(dt > 65535.0f ? 65535.0f : dt + 0.5f);
	s->flags = e->active ? NETREP_FL_ACTIVE : 0;
	if (kind == NETREP_MISSILE) {
		return;
	}
	s->life = (int16_t)(w->life > 32767 ? 32767 : w->life < -32768 ? -32768 : w->life);
	s->flags |= Players_Of(e) >= 0 ? NETREP_FL_PLAYER : 0;
	if (kind == NETREP_SHIP) {
		cVehicleEntity *v = (cVehicleEntity *)e;
		s->ctrl[0] = NetRep_QCtrl(v->controls.acc);
		s->ctrl[1] = NetRep_QCtrl(v->controls.str);
		s->weapon = (uint8_t)v->weapons.curWeap;
		s->flags |= v->controls.fire ? NETREP_FL_FIRE : 0;
		return;
	}
	cWalkCharEntity *c = WChar(e);
	s->flags |= (c->falling ? NETREP_FL_FALLING : 0) | (c->walking ? NETREP_FL_WALKING : 0);
	s->view[0] = NetRep_QAng(c->view.x);
	s->view[1] = NetRep_QAng(c->view.y);
	cActCtrl *ctrl = w->actCtrl;
	if (ctrl && ctrl->cls) {
		for (int l = 0; l < NETREP_LAYERS; l++) {
			const cActLayer *ly = &ctrl->layers[l];
			if (ly->on && ly->action) {
				s->act[l] = Rep_ActIndex(ctrl->cls, ly->action);
				float t = ly->time * 1000.0f;
				s->actTime[l] = t > 0.0f ? (uint32_t)(t + 0.5f) : 0;
			}
		}
	}
}

/* the snapshot: the states of the entities into the server, slots for new ones */
static void Rep_Gather(uint64_t tick) {
	char look[NETREP_LOOK_MAX + 1];
	static byte seen[NETREP_MAX_ENTS];
	NetRepState st;

	NetRepSrv_Begin(rep.srv, (uint32_t)tick);
	memset(seen, 0, sizeof(seen));
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		int kind = Rep_Kind(e);
		if (!kind) {
			continue;
		}
		Rep_Look(e, kind, look, sizeof(look));
		int known = NetRepSrv_NumLooks(rep.srv);
		int id = NetRepSrv_Look(rep.srv, look);
		if (id < 0) {
			continue;
		}
		if (id >= known) {
			Con_Printf("replication: look %d %s\n", id, look);
		}
		int s = Rep_Find(e);
		if (s >= 0 && (seen[s] || rep.ent[s].look != id)) {
			s = -1;			/* rebuilt with another look: a new entity for the watchers */
		}
		if (s < 0) {
			s = NetRepSrv_FreeSlot(rep.srv);
			if (s < 0) {
				continue;
			}
			rep.ent[s].e = e;
			Str_Copy(rep.ent[s].name, e->name, REP_NAME);
			rep.ent[s].look = id;
			rep.ent[s].gen++;
		}
		seen[s] = 1;
		Rep_State(e, kind, &st);
		NetRepSrv_Set(rep.srv, s, kind, id, &st);
	}
	NetRepSrv_End(rep.srv);
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		if (!seen[s]) {
			rep.ent[s].e = NULL;
		}
	}
	Rep_HashBuild();
}

/* SCRAP_REPLOG: the sample of each replicated entity of the world now */
static void Rep_Log(uint64_t tick) {
	if (!rep.logTried) {
		rep.logTried = 1;
		const char *path = getenv("SCRAP_REPLOG");
		if (path && *path) {
			rep.log = fopen(path, "w");
		}
		if (rep.log) {
			fputs("time,slot,x,y,z,gen\n", rep.log);
		}
	}
	if (!rep.log) {
		return;
	}
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		int s = Rep_Kind(e) ? Rep_Find(e) : -1;
		if (s >= 0) {
			double t = (double)tick / INSTANCE_TICK_HZ + Rep_SampleDt(e);
			fprintf(rep.log, "%.4f,%d,%.2f,%.2f,%.2f,%u\n", t, s, (double)e->pos.x, (double)e->pos.y, (double)e->pos.z,
				rep.ent[s].gen);
		}
	}
}

/* ---- the players ---- */

/* the players' spawn point: the first "DM_Player_Spawn*" dummy, else "DM_Spawn*", else the origin */
static void Rep_FindSpawn(void) {
	static const char *const names[] = { "DM_Player_Spawn*", "DM_Spawn*" };
	char name[64];
	vec3_t ang;

	rep.haveSpawn = 1;
	for (int i = 0; i < 2; i++) {
		DummySearch_Start(names[i], 1);
		if (DummySearch_NextEuler(name, sizeof(name), &rep.spawn, &ang)) {
			rep.spawnYaw = ang.y + M_PI_F;
			Con_Printf("instance: players spawn at %s (%.0f %.0f %.0f)\n", name, (double)rep.spawn.x,
				(double)rep.spawn.y, (double)rep.spawn.z);
			return;
		}
	}
	rep.spawn = (vec3_t){ 0.0f, 0.0f, 0.0f };
	rep.spawnYaw = 0.0f;
}

static void Rep_SetSlot(cEntity *e, int slot) {
	Entity_VarI(e, "UsrControl", &slot, 1);
}

static void Rep_SetVarS(cEntity *e, const char *var, const char *value) {
	char *v = (char *)value;
	Entity_VarS(e, var, &v, 1);
}

static void Rep_SetOwn(RepPlayer *pl, cEntity *e) {
	pl->own = e;
	Str_Copy(pl->ownName, e ? e->name : "", sizeof(pl->ownName));
}

/* the entity the player was given while it is still in the world, else NULL */
static cEntity *Rep_Own(const RepPlayer *pl) {
	cEntity *e = pl->own && pl->ownName[0] ? World_FindEntity(pl->ownName) : NULL;
	return e && e == pl->own && !e->deadNode.target ? e : NULL;
}

/* the player's Dtritus at pos (a ring around the spawn point when pos is NULL) */
static cEntity *Rep_SpawnChar(int i, const vec3_t *pos, float yaw) {
	char name[REP_NAME];
	vec3_t p, ang;

	if (!rep.haveSpawn) {
		Rep_FindSpawn();
	}
	if (pos) {
		p = *pos;
	} else {
		float a = (float)i * (M_2PI_F / 8.0f);
		float r = PLAY_SPAWN_RING * (float)(1 + i / 8);
		p = (vec3_t){ rep.spawn.x + r * sinf(a), rep.spawn.y, rep.spawn.z + r * cosf(a) };
		yaw = rep.spawnYaw;
	}
	RepPlayer *pl = &rep.pl[i];
	Str_PrintfQuiet(name, sizeof(name), "NetPlayer%d_%u", i, ++pl->spawns);
	cEntity *e = cWorld_CreateEntity(g_pWorld, name, p.x, p.y, p.z, TYEN_WALKCHAR);
	if (!e) {
		return NULL;
	}
	WithLife_SetActCtrl(e, REP_CHAR_CLASS);
	Rep_SetSlot(e, PLAYERS_NET_SLOT + i);
	Rep_SetVarS(e, "TgType", "Green");
	ang = (vec3_t){ 0.0f, yaw, 0.0f };
	Obj_SetAng(e, &ang);
	Obj(e)->netStep = 1;
	Rep_SetOwn(pl, e);
	pl->lost = 0;
	return e;
}

static byte Rep_IsParked(cEntity *e) {
	return Entity_IsType(e, TYEN_CAR) && !e->deadNode.target && !Obj(e)->proxy && !strncmp(e->name, "Parked", 6) &&
		cVehicleEntity_GetOwnerSlot(e) == -1;
}

/* the wall and the view radius of a ship swapped for flying or parked (Parking.SetShipRadius) */
static void Rep_ShipRadius(cEntity *v, byte parked) {
	double wr, vr;
	if (Entity_VarF(v, "WRadius", &wr, 0) && Entity_VarF(v, "VRadius", &vr, 0) && (parked ? wr > vr : wr < vr)) {
		Entity_VarF(v, "WRadius", &vr, 1);
		Entity_VarF(v, "VRadius", &wr, 1);
	}
}

/* the sandbox has no shop yet: a taken ship gets every weapon (slot A of those it lacks) and full ammo */
static void Rep_Loadout(cEntity *ship) {
	char list[256];
	int pos = 0;
	cWeaponManager *wm = &Veh(ship)->weapons;

	list[0] = 0;
	for (int w = 0; w < g_pWorld->weapons.count && pos < (int)sizeof(list) - 12; w++) {
		uint32_t slots = cWeaponManager_GetSlots(wm, w);
		Str_PrintfQuiet(list + pos, (int)sizeof(list) - pos, "%s%u", w ? "," : "", slots ? slots : 1u);
		pos += (int)strlen(list + pos);
	}
	char *l = list;
	cWeaponManager_VarS(wm, "WeapList", &l, 1);
	for (int k = 0; k < g_pWorld->ammo.count; k++) {
		cWeaponManager_SetAmmo(wm, k, TABLE_AT(g_pWorld->ammo, k)->max);
	}
}

/* the switch: into the nearest parked ship, out of the ship */
static void Rep_Switch(int i, cEntity *e) {
	RepPlayer *pl = &rep.pl[i];
	char name[REP_NAME];

	if (NetPl_Kind(e) == NETREP_CHAR) {
		cEntity *best = NULL;
		float bestD = PLAY_BOARD_RADIUS * PLAY_BOARD_RADIUS;
		for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
			if (!Rep_IsParked(v)) {
				continue;
			}
			float dx = v->pos.x - e->pos.x, dy = v->pos.y - e->pos.y, dz = v->pos.z - e->pos.z;
			float d = dx * dx + dy * dy + dz * dz;
			if (d < bestD) {
				bestD = d;
				best = v;
			}
		}
		if (!best) {
			return;
		}
		Str_Copy(name, best->name, sizeof(name));
		Rep_SetVarS(best, "MPil0", "P_" REP_CHAR_CLASS);
		PORT_CALL(ShipMaker, SetUpChars, NULL, "s", name);
		int zero = 0;
		Entity_VarI(best, "ArrowCanUse", &zero, 1);
		Rep_SetVarS(best, "UseFunc", "");
		Rep_SetVarS(best, "TgType", "Green");
		Rep_ShipRadius(best, 0);
		/* the parked ship is held at its place by a move that ended (Parking.SetShipAsParked): off */
		double off = 0.0;
		Entity_VarF(best, "MoveTime", &off, 1);
		/* it starts from rest, as the client's new copy of it does */
		cVehicleEntity *v = Veh(best);
		v->thrust = v->turnSpeedX = v->turnSpeedY = v->roll = v->speed = v->boost = v->pushTime = 0.0f;
		Obj(best)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
		Obj(best)->rotVel = (vec3_t){ 0.0f, 0.0f, 0.0f };
		Rep_Loadout(best);
		Rep_SetSlot(best, PLAYERS_NET_SLOT + i);
		Entity_SetActive(best, 1);
		Obj(best)->netStep = 1;
		cEntity_Del(e);
		Rep_SetOwn(pl, best);
		NetRepInBuf_Restart(&pl->in);
		pl->told = -2;
		pl->boards++;
		Con_Printf("instance: player %d into %s\n", i, name);
		return;
	}
	if (NetPl_Kind(e) != NETREP_SHIP || Veh(e)->speed > PLAY_LEAVE_SPEED) {
		return;
	}
	/* out to the right of the ship, on the floor below it; the ship stays parked */
	float yaw = Obj(e)->ang.y;
	vec3_t p = { e->pos.x + cosf(yaw) * 600.0f, e->pos.y + 200.0f, e->pos.z - sinf(yaw) * 600.0f };
	vec3_t down = { p.x, p.y - 100000.0f, p.z }, floor;
	if (Engine_TestLine(&p, &down, &floor, NULL)) {
		p.y = floor.y + 150.0f;
	}
	vec3_t zero = { 0.0f, 0.0f, 0.0f };
	Str_Copy(name, e->name, sizeof(name));
	Obj(e)->netStep = 0;
	Rep_SetSlot(e, -1);
	Obj_SetVel(e, &zero);
	PORT_CALL(Parking, SetShipAsParked, NULL, "s", name);
	Rep_SpawnChar(i, &p, yaw);
	NetRepInBuf_Restart(&pl->in);
	pl->told = -2;
	pl->leaves++;
	Con_Printf("instance: player %d out of %s\n", i, name);
}

/* the player's entity gone: a character away, a ship parked (a dead one left to the game: its wreck) */
static void Rep_DropOwn(int i) {
	RepPlayer *pl = &rep.pl[i];
	cEntity *e = Rep_Own(pl);

	Rep_SetOwn(pl, NULL);
	if (!e) {
		return;
	}
	Obj(e)->netStep = 0;
	if (NetPl_Kind(e) == NETREP_SHIP) {
		Rep_SetSlot(e, -1);
		if (WLife(e)->life > 0) {
			PORT_CALL(Parking, SetShipAsParked, NULL, "s", e->name);
		}
	} else {
		Rep_SetSlot(e, -1);
		cEntity_Del(e);
	}
}

static int Rep_Join(int conn) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		RepPlayer *pl = &rep.pl[i];
		if (pl->used) {
			continue;
		}
		uint32_t spawns = pl->spawns;	/* the names stay unique while a deleted entity waits */
		memset(pl, 0, sizeof(*pl));
		pl->conn = conn;
		pl->told = -1;
		pl->spawns = spawns;
		NetRepInBuf_Init(&pl->in);
		Players_Join(i, Instance_StartMoney());
		if (!Rep_SpawnChar(i, NULL, 0.0f)) {
			Players_Part(i);
			return -1;
		}
		pl->used = 1;
		rep.players++;
		Con_Printf("instance: player %d on connection %d (%s)\n", i, conn, pl->ownName);
		return i;
	}
	return -1;
}

static void Rep_Leave(int i) {
	RepPlayer *pl = &rep.pl[i];
	Con_Printf("instance: player %d left (%u inputs, %u late, %u missed, %u shots, %u hits)\n", i, pl->in.got,
		pl->in.late, pl->in.missed, pl->shots, pl->hits);
	Rep_DropOwn(i);
	Players_Part(i);
	pl->used = 0;
	rep.players--;
}

void Rep_PreFrame(uint64_t tick) {
	NetRepInput in;

	rep.tick = tick;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		RepPlayer *pl = &rep.pl[i];
		if (!pl->used) {
			continue;
		}
		cEntity *e = Players_Get(i);
		if (e && e == pl->own && WLife(e)->life <= 0 && !pl->lost) {
			/* dead: the game's death runs on (the effects, the wreck) without the player */
			Con_Printf("instance: player %d died in %s\n", i, e->name);
			Rep_DropOwn(i);
			e = NULL;
		}
		if (e != pl->own || !e) {
			/* dead or deleted by the game: a new Dtritus after a while */
			if (!pl->lost) {
				pl->lost = tick;
			}
			if (tick - pl->lost >= REP_RESPAWN_TICKS) {
				Rep_DropOwn(i);
				Rep_SpawnChar(i, NULL, 0.0f);
				NetRepInBuf_Restart(&pl->in);
				pl->told = -2;
			}
			continue;
		}
		pl->lost = 0;
		/* the money, the alarm and the scripts' player of this player while its input runs */
		Players_Bind(i);
		NetRepInBuf_Take(&pl->in, (uint32_t)tick, &in);
		pl->shownNow = pl->shown[tick % NETREP_INBUF];
		if ((in.buttons & NETREP_IN_SWITCH) && !(pl->buttons & NETREP_IN_SWITCH)) {
			pl->buttons = in.buttons;
			Rep_Switch(i, e);
			continue;
		}
		pl->buttons = in.buttons;
		NetPl_Step(e, &in, 0, 0);
	}
	Players_BindTurn();
}

/* the lag compensation: the other entities at the server time the shooter's client showed them */
void Rep_RewindBegin(cEntity *v) {
	vec3_t p;

	rep.rewound = 0;
	int i = g_Instance && rep.srv ? Players_Of(v) : -1;
	if (i < 0 || !rep.pl[i].used) {
		return;
	}
	RepPlayer *pl = &rep.pl[i];
	double now = (double)rep.tick / NETREP_TICK_HZ;
	double t = pl->shownNow / 1000.0;
	t = t < now - REP_REWIND_MAX ? now - REP_REWIND_MAX : t > now ? now : t;
	pl->shots++;
	pl->rewindSum += now - t;
	pl->rewindN++;
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		cEntity *e = rep.ent[s].e;
		if (!e || e == v || e->deadNode.target || !NetRepSrv_PosAt(rep.srv, s, t, &p.x)) {
			continue;
		}
		rep.rewind[rep.rewound].e = e;
		rep.rewind[rep.rewound].pos = e->pos;
		rep.rewound++;
		e->pos = p;
	}
}

void Rep_RewindEnd(cEntity *v) {
	if (!rep.rewound || Players_Of(v) < 0) {
		return;
	}
	for (int k = 0; k < rep.rewound; k++) {
		rep.rewind[k].e->pos = rep.rewind[k].pos;
	}
	rep.rewound = 0;
}

byte Rep_Hurt(cEntity *attacker, cEntity *victim, const char *weapon) {
	if (!g_Instance) {
		return 1;
	}
	if (!Players_MayHurt(attacker, victim)) {
		return 0;
	}
	int i = Players_Of(attacker), j = Players_Of(victim);
	if (i < 0 || !rep.pl[i].used || attacker == victim) {
		return 1;
	}
	RepPlayer *pl = &rep.pl[i];
	pl->hits++;
	Con_Printf("instance: hit by player %d (%s) with %s on %s%s%d, life %d, shown %.0f ms back\n", i, attacker->name,
		weapon, victim->name, j >= 0 ? " = player " : " ", j, WLife(victim)->life,
		(double)rep.tick * 1000.0 / NETREP_TICK_HZ - pl->shownNow);
	return 1;
}

/* the money, the alarm and the ammo of the player for its HUD, when they changed or a second after the last */
static void Rep_SendStatus(Net2Host *host, int i, cEntity *e) {
	RepPlayer *pl = &rep.pl[i];
	NetRepStatus st;
	uint8_t buf[16 + 2 * NETREP_AMMO_MAX];

	memset(&st, 0, sizeof(st));
	st.tick = (uint32_t)rep.tick;
	st.money = Players_Money(i);
	const cAlarm *a = Players_Alarm(i);
	float v = a->value < 0.0f ? 0.0f : a->value > 1.0f ? 1.0f : a->value;
	st.alarm = (uint8_t)(v * 255.0f + 0.5f);
	st.flags = a->on ? NETREP_ST_ALARM : 0;
	if (e && NetPl_Kind(e) == NETREP_SHIP && Veh(e)->weapons.ammo) {
		int n = g_pWorld->ammo.count < NETREP_AMMO_MAX ? g_pWorld->ammo.count : NETREP_AMMO_MAX;
		for (int k = 0; k < n; k++) {
			float x = Veh(e)->weapons.ammo[k];
			st.ammo[k] = (uint16_t)(x <= 0.0f ? 0.0f : x >= 65535.0f ? 65535.0f : x + 0.5f);
		}
		st.nammo = (uint8_t)n;
	}
	if (!NetRep_StatusDiffers(&st, &pl->status) && rep.snapTick - pl->statusSnap < NETREP_SNAP_HZ && pl->statusSnap) {
		return;
	}
	int n = NetRep_WriteStatus(buf, sizeof(buf), &st);
	if (n > 0) {
		Net2_Send(host, pl->conn, REP_CH_UNRELIABLE, buf, n);
		pl->status = st;
		pl->statusSnap = rep.snapTick ? rep.snapTick : 1;
	}
}

/* the players' entities in their slots after a snapshot: a slot that changed goes to the client */
static void Rep_TellSlots(Net2Host *host) {
	uint8_t buf[16 + 4 * NETREP_PHYS_MAX];
	float phys[NETREP_PHYS_MAX];

	for (int i = 0; i < PLAYERS_MAX; i++) {
		RepPlayer *pl = &rep.pl[i];
		if (!pl->used || !rep.conn[pl->conn].c) {
			continue;
		}
		cEntity *e = pl->own && Players_Get(i) == pl->own ? pl->own : NULL;
		int slot = e ? Rep_Find(e) : -1;
		int kind = e ? NetPl_Kind(e) : 0;
		if (e) {
			NetRepSrv_SetView(rep.conn[pl->conn].c, e->pos.x, e->pos.y, e->pos.z, 0.0f);
		}
		Rep_SendStatus(host, i, e);
		if (slot == pl->told) {
			continue;
		}
		pl->told = slot;
		pl->toldKind = kind;
		NetRepSrv_SetOwn(rep.conn[pl->conn].c, slot);
		int np = e ? NetPl_GetPhys(e, phys) : 0;
		int n = NetRep_WritePlayer(buf, sizeof(buf), i, slot < 0 ? NETREP_NO_SLOT : slot, kind, (uint32_t)rep.tick, phys,
			np);
		if (n > 0) {
			Net2_Send(host, pl->conn, REP_CH_RELIABLE, buf, n);
		}
	}
}

/* ---- the shots ---- */

void Rep_Shot(cEntity *shooter, int weapon, int count, const vec3_t *from, const vec3_t *to, cEntity *target) {
	if (!rep.watchers || count <= 0) {
		return;
	}
	if (rep.nshots >= REP_SHOTS) {
		rep.shotsLost++;
		return;
	}
	RepShot *x = &rep.shots[rep.nshots++];
	x->shooter = shooter;
	x->target = target;
	x->weapon = (uint8_t)weapon;
	x->count = (uint8_t)(count > 255 ? 255 : count);
	x->from = *from;
	x->to = *to;
}

/* the shots since the last snapshot to each client that sees the shooter; a player does not get the shots of its
 * own ship that its client shows itself (NetPl_LocalWeapon) */
static void Rep_SendShots(Net2Host *host) {
	uint8_t buf[NETREP_MSG_MAX];
	NetRepShot out[NETREP_SHOTS_MAX];
	int n = 0;

	for (int i = 0; i < rep.nshots; i++) {
		RepShot *x = &rep.shots[i];
		int slot = Rep_Find(x->shooter);
		if (slot < 0) {
			continue;
		}
		NetRepShot *w = &rep.wire[n];
		int t = x->target ? Rep_Find(x->target) : -1;
		w->slot = (uint16_t)slot;
		w->weapon = x->weapon;
		w->count = x->count;
		w->target = t < 0 ? NETREP_NO_SLOT : (uint16_t)t;
		for (int k = 0; k < 3; k++) {
			w->pos[k] = NetRep_QPos((&x->to.x)[k]);
			w->from[k] = NetRep_QPos((&x->from.x)[k]);
			rep.wirePos[n][k] = (&x->shooter->pos.x)[k];
		}
		n++;
	}
	rep.nshots = 0;
	if (!n) {
		return;
	}
	for (int c = 0; c < REP_CONNS; c++) {
		if (!rep.conn[c].c) {
			continue;
		}
		float view[3];
		float r = NetRepSrv_GetView(rep.conn[c].c, view) * NETREP_KEEP;
		int pi = rep.conn[c].player;
		int own = pi >= 0 ? rep.pl[pi].told : -1;
		int m = 0;
		for (int i = 0; i < n; i++) {
			const NetRepShot *w = &rep.wire[i];
			float dx = rep.wirePos[i][0] - view[0], dy = rep.wirePos[i][1] - view[1], dz = rep.wirePos[i][2] - view[2];
			if (dx * dx + dy * dy + dz * dz > r * r || (w->slot == own && NetPl_LocalWeapon(w->weapon))) {
				continue;
			}
			out[m++] = *w;
			if (m == NETREP_SHOTS_MAX) {
				int size = NetRep_WriteShots(buf, sizeof(buf), (uint32_t)rep.tick, out, m);
				if (size > 0) {
					Net2_Send(host, c, REP_CH_UNRELIABLE, buf, size);
				}
				m = 0;
			}
		}
		if (m) {
			int size = NetRep_WriteShots(buf, sizeof(buf), (uint32_t)rep.tick, out, m);
			if (size > 0) {
				Net2_Send(host, c, REP_CH_UNRELIABLE, buf, size);
			}
		}
	}
}

/* ---- the watchers ---- */

void Rep_Connect(Net2Host *host, int conn) {
	(void)host;
	(void)conn;
}

void Rep_Disconnect(int conn) {
	if (conn < 0 || conn >= REP_CONNS || !rep.conn[conn].c) {
		return;
	}
	if (rep.conn[conn].player >= 0) {
		Rep_Leave(rep.conn[conn].player);
	}
	NetRepSrv_RemoveClient(rep.srv, rep.conn[conn].c);
	rep.conn[conn].c = NULL;
	rep.watchers--;
}

byte Rep_IsWatcher(int conn) {
	return conn >= 0 && conn < REP_CONNS && rep.conn[conn].c;
}

static void Rep_Hello(Net2Host *host, int conn, uint32_t rate, int flags) {
	uint8_t buf[300];
	float view[3] = { 0.0f, 0.0f, 0.0f };

	if (!rep.srv) {
		rep.srv = NetRepSrv_Create();
		Rep_HashBuild();
	}
	if (!rep.srv) {
		return;
	}
	if (!rep.conn[conn].c) {
		rep.conn[conn].c = NetRepSrv_AddClient(rep.srv, rate ? rate : NETREP_RATE_DEFAULT);
		if (!rep.conn[conn].c) {
			return;
		}
		memset(&rep.conn[conn].last, 0, sizeof(rep.conn[conn].last));
		rep.conn[conn].player = (flags & NETREP_HELLO_PLAY) ? Rep_Join(conn) : -1;
		rep.watchers++;
	}
	int i = rep.conn[conn].player;
	cEntity *p = i >= 0 ? Rep_Own(&rep.pl[i]) : Players_Any();
	if (p) {
		view[0] = p->pos.x;
		view[1] = p->pos.y;
		view[2] = p->pos.z;
	}
	NetRepSrv_SetView(rep.conn[conn].c, view[0], view[1], view[2], 0.0f);
	int n = NetRep_WriteWelcome(buf, sizeof(buf), (uint32_t)(Instance_Time() * INSTANCE_TICK_HZ + 0.5), view,
		g_pApp->levelPath ? g_pApp->levelPath : "");
	if (n > 0) {
		Net2_Send(host, conn, REP_CH_RELIABLE, buf, n);
	}
	Con_Printf("instance: %s %d, rate %u bytes/s\n", i >= 0 ? "player" : "watcher", conn, rate);
}

/* the inputs of a player's ack into its buffer, with the server time its client showed; an input made for another
 * entity than the one the client was told last (before a switch reached it) is dropped */
static void Rep_Inputs(RepPlayer *pl, const NetRepInput *in, int n, uint32_t shownMs) {
	for (int k = 0; k < n; k++) {
		if (in[k].slot != pl->told) {
			pl->stale++;
			continue;
		}
		if (NetRepInBuf_Put(&pl->in, &in[k], (uint32_t)rep.tick)) {
			uint32_t back = (in[0].tick - in[k].tick) * 1000u / NETREP_TICK_HZ;
			pl->shown[in[k].tick % NETREP_INBUF] = shownMs > back ? shownMs - back : 0;
		}
	}
}

byte Rep_Message(Net2Host *host, int conn, int channel, const uint8_t *data, int size) {
	uint32_t rate, bits, shown;
	uint16_t newest;
	float view[3];
	int flags, n;
	NetRepInput in[NETREP_INPUT_REDUNDANCY];

	if (conn < 0 || conn >= REP_CONNS || size < 1) {
		return 0;
	}
	if (channel == REP_CH_RELIABLE && data[0] == NETREP_MSG_HELLO) {
		if (NetRep_ReadHello(data, size, &rate, &flags)) {
			Rep_Hello(host, conn, rate, flags);
		} else {
			Net2_Disconnect(host, conn, NET2_DR_VERSION);
		}
		return 1;
	}
	if (!rep.conn[conn].c) {
		return 0;
	}
	if (data[0] == NETREP_MSG_ACK &&
		NetRep_ReadAckInput(data, size, &newest, &bits, view, in, NETREP_INPUT_REDUNDANCY, &n, &shown)) {
		NetRepSrv_Ack(rep.srv, rep.conn[conn].c, newest, bits);
		int i = rep.conn[conn].player;
		if (i < 0) {
			NetRepSrv_SetView(rep.conn[conn].c, view[0], view[1], view[2], 0.0f);
		} else {
			Rep_Inputs(&rep.pl[i], in, n, shown);
		}
	}
	return 1;
}

void Rep_Tick(Net2Host *host, uint64_t tick) {
	int sizes[NETREP_MAX_MSGS];

	Rep_Log(tick);
	uint32_t due = (uint32_t)(tick * NETREP_SNAP_HZ / NETREP_TICK_HZ);
	if (!rep.watchers || due == rep.snapTick) {
		rep.snapTick = due;
		return;
	}
	rep.snapTick = due;
	uint64_t t0 = Plat_RealNs();
	Rep_Gather(tick);
	Rep_TellSlots(host);
	Rep_SendShots(host);
	uint64_t t1 = Plat_RealNs();
	for (int i = 0; i < REP_CONNS; i++) {
		if (!rep.conn[i].c) {
			continue;
		}
		int n = NetRepSrv_Build(rep.srv, rep.conn[i].c, rep.msgs, sizes, NETREP_MAX_MSGS);
		for (int m = 0; m < n; m++) {
			Net2_Send(host, i, REP_CH_UNRELIABLE, rep.msgs[m], sizes[m]);
		}
	}
	uint64_t t2 = Plat_RealNs();
	rep.gatherNs += t1 - t0;
	rep.buildNs += t2 - t1;
	rep.gatherMax = t1 - t0 > rep.gatherMax ? t1 - t0 : rep.gatherMax;
	rep.buildMax = t2 - t1 > rep.buildMax ? t2 - t1 : rep.buildMax;
	rep.snaps++;
}

static void Rep_PlayerStatus(int i) {
	RepPlayer *pl = &rep.pl[i];
	NetRepInBuf *b = &pl->in, *l = &pl->last;
	uint32_t got = b->got - l->got, leadN = b->leadN - l->leadN;
	cEntity *e = Rep_Own(pl);
	const cAlarm *a = Players_Alarm(i);
	Con_Printf("  player %d (%s): %u inputs, %u late, %u again, %u missed, %u stale, lead avg %.2f ticks, %u shots %u "
		"hits, rewind avg %.0f ms, %u boards %u leaves, money %d, alarm %.2f%s, at %.0f %.0f %.0f\n", i,
		e ? e->name : "-", got, b->late - l->late, b->dup - l->dup, b->missed - l->missed, pl->stale,
		leadN ? (double)(b->leadSum - l->leadSum) / leadN : 0.0, pl->shots, pl->hits,
		pl->rewindN ? pl->rewindSum * 1000.0 / pl->rewindN : 0.0, pl->boards, pl->leaves, Players_Money(i),
		(double)a->value, a->on ? " on" : "", e ? (double)e->pos.x : 0.0, e ? (double)e->pos.y : 0.0,
		e ? (double)e->pos.z : 0.0);
	*l = *b;
}

void Rep_Status(double secs) {
	if (!rep.srv || !rep.snaps) {
		return;
	}
	Con_Printf("replication: %d watchers (%d players), %d entities, %u snapshots, gather ms avg %.3f max %.3f, build "
		"ms avg %.3f max %.3f\n", rep.watchers, rep.players, NetRepSrv_Live(rep.srv), rep.snaps,
		(double)rep.gatherNs / rep.snaps / 1e6, (double)rep.gatherMax / 1e6, (double)rep.buildNs / rep.snaps / 1e6,
		(double)rep.buildMax / 1e6);
	rep.gatherNs = rep.buildNs = rep.gatherMax = rep.buildMax = 0;
	rep.snaps = 0;
	for (int i = 0; i < REP_CONNS; i++) {
		if (!rep.conn[i].c) {
			continue;
		}
		const NetRepClientStats *st = NetRepSrv_Stats(rep.conn[i].c);
		NetRepClientStats *l = &rep.conn[i].last;
		uint32_t snaps = st->snaps - l->snaps;
		uint32_t msgs = st->msgs - l->msgs;
		double bytes = (double)(st->bytes - l->bytes);
		Con_Printf("  watcher %d: %.0f bytes/s, %u snapshots, %.1f bytes and %.1f records a snapshot (max %u bytes, %u "
			"records), %u messages, %u creates %u removes %u fulls, %u waited, %u acked, interest %u\n", i,
			secs > 0.0 ? bytes / secs : 0.0, snaps, snaps ? bytes / snaps : 0.0,
			snaps ? (double)(st->records - l->records) / snaps : 0.0, st->maxSnapBytes, st->maxRecords, msgs,
			st->creates - l->creates, st->removes - l->removes, st->fulls - l->fulls, st->skipped - l->skipped,
			st->acked - l->acked, st->interest);
		*l = *st;
		if (rep.conn[i].player >= 0) {
			Rep_PlayerStatus(rep.conn[i].player);
		}
	}
}

void Rep_Close(void) {
	NetRepSrv_Destroy(rep.srv);
	rep.srv = NULL;
	if (rep.log) {
		fclose(rep.log);
		rep.log = NULL;
	}
}
