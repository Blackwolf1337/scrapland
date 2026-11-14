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

#include "scrap/alarm.h"
#include "scrap/arena.h"
#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/model.h"
#include "scrap/node3d.h"
#include "scrap/netrep.h"
#include "scrap/places.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/arrowindicator.h"
#include "scrap/chartype.h"
#include "scrap/travel.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

#define REP_CONNS 1024				/* NET2_MAX_CONNS */
#define REP_HASH 4096				/* pointer table, twice the slots */
#define REP_NAME 64
#define REP_REWIND_MAX 0.5					/* s the lag compensation goes back at most */
#define REP_PARK_HEIGHT 1500.0f			/* units above the floor a ship left by its player is parked at most */
#define REP_SPAWNS 32					/* spawn points of a level kept */
#define REP_SPAWN_CROWD 2500.0f			/* units around a spawn point its players are counted in */
#define REP_SPOT_STEP 350.0f			/* units between the places tried around a spawn point */
#define REP_SPOT_TRIES 33				/* the point and four rings of eight */
#define REP_SPAWN_DROP 1500.0f			/* units below a spawn point its floor must be */
#define REP_SPAWN_FALL 2500.0f			/* a new character that falls further in its first seconds stood over a
										   shaft (an elevator's, which the instance does not make): that spawn
										   point is not used again */
#define REP_SPAWN_CHECK (4 * NETREP_TICK_HZ)
#define REP_CALL_DROP 3000.0f			/* and below the place of a called ship */
#define REP_CHAR_CLEAR 120.0f			/* units a character's place keeps from walls and others */
#define REP_SHIP_CLEAR 450.0f			/* and a called ship's */

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
	uint32_t shots, hits, boards, leaves, spawns, calls, deaths;
	cEntity *ships[PLAY_SHIPS_MAX];	/* the ships it called and still owns, oldest first (compared, read while
									   World_FindEntity(shipNames) is them) */
	char shipNames[PLAY_SHIPS_MAX][REP_NAME];
	int nships;
	int lives;						/* lives left (-lives), 0 with infinite lives */
	int rebuild;					/* the price of its own ship that died: the next call costs it (-rebuild) */
	byte dead;						/* died, no new character yet (the player message says so) */
	cEntity *corpse;				/* its dead character (compared, read while World_FindEntity(corpseName) is it) */
	char corpseName[REP_NAME];
	int spawnAt;					/* the spawn point of its character, -1 */
	uint64_t spawnTick;
	float spawnY;
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
	cEntity *shooter, *target;		/* compared only: read while World_FindEntity of the name is them */
	char shooterName[REP_NAME], targetName[REP_NAME];
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
	vec3_t spawn[REP_SPAWNS];		/* the players' spawn points and their yaws (radians) */
	float spawnYaw[REP_SPAWNS];
	byte spawnBad[REP_SPAWNS];		/* a character fell from there (REP_SPAWN_FALL) */
	int nspawns, spawnNext;
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

/* the sandbox options of the command line (Rep_ParseParam) */
static struct {
	int ships;						/* -ships: called ships a player owns at most */
	int respawnTicks;				/* -respawn: s from a death to the new character */
	int lives;						/* -lives: 0 infinite */
	byte rebuild;					/* -rebuild: a player pays for its own ship that died with its next call */
	int spawn;						/* -spawn: all at that spawn point, -1 spread */
} sand = { 1, PLAY_RESPAWN_SECS * NETREP_TICK_HZ, 0, 0, -1 };

byte Rep_ParseParam(const char *param, const char *ext) {
	int n = crt_atoi(ext);
	if (!crt_stricmp("ships", param)) {
		sand.ships = n < 1 ? 1 : n > PLAY_SHIPS_MAX ? PLAY_SHIPS_MAX : n;
		return 1;
	}
	if (!crt_stricmp("respawn", param)) {
		sand.respawnTicks = (n < 1 ? 1 : n > 60 ? 60 : n) * NETREP_TICK_HZ;
		return 1;
	}
	if (!crt_stricmp("lives", param)) {
		sand.lives = n < 0 ? 0 : n;
		return 1;
	}
	if (!crt_stricmp("rebuild", param)) {
		sand.rebuild = n != 0;
		return 1;
	}
	if (!crt_stricmp("spawn", param)) {
		sand.spawn = n < 0 ? -1 : n;
		return 1;
	}
	return 0;
}

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

int Rep_SlotOf(const cEntity *e) {
	return e ? Rep_Find(e) : -1;
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

/* how the watcher builds the proxy: S|model|pilot|motor 0..3|class|weapon slots, C|class or
 * M|model|MISSILE_WEAP_|MISSILE_FX_|level */
static void Rep_Look(cEntity *e, int kind, char *out, int size) {
	if (kind == NETREP_MISSILE) {
		const cMissileEntity *m = Missile(e);
		Str_PrintfQuiet(out, size, "M|%s|%d|%d|%d", Rep_ModelPath(e), m->weap, m->fx, m->level);
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
		/* a missile: its bees in life, the slot + 1 of the entity that fired it in view[0] */
		cEntity *parent = Missile(e)->parent[0] ? World_FindEntity(Missile(e)->parent) : NULL;
		int ps = parent ? Rep_Find(parent) : -1;
		int n = Missile(e)->swarmCount;
		s->life = (int16_t)(n > 32767 ? 32767 : n < 0 ? 0 : n);
		s->view[0] = (uint16_t)(ps + 1);
		return;
	}
	s->life = (int16_t)(w->life > 32767 ? 32767 : w->life < -32768 ? -32768 : w->life);
	s->flags |= Players_Of(e) >= 0 ? NETREP_FL_PLAYER : 0;
	if (kind == NETREP_SHIP) {
		cVehicleEntity *v = (cVehicleEntity *)e;
		s->ctrl[0] = NetRep_QCtrl(v->controls.acc);
		s->ctrl[1] = NetRep_QCtrl(v->controls.str);
		s->weapon = (uint8_t)v->weapons.curWeap;
		s->flags |= (v->controls.fire ? NETREP_FL_FIRE : 0) | Faction_Flags(e);
		return;
	}
	cWalkCharEntity *c = WChar(e);
	s->flags |= (c->falling ? NETREP_FL_FALLING : 0) | (c->walking ? NETREP_FL_WALKING : 0) | Faction_Flags(e);
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

static cEntity *Rep_Own(const RepPlayer *pl);

/* the players' spawn points: the "DM_Player_Spawn*" dummies, else "DM_Spawn*", else the origin */
static void Rep_FindSpawns(void) {
	static const char *const names[] = { "DM_Player_Spawn*", "DM_Spawn*" };
	char name[64];
	vec3_t p, ang;

	rep.haveSpawn = 1;
	rep.nspawns = Places_Spawns(rep.spawn, rep.spawnYaw, REP_SPAWNS);
	for (int k = 0; k < rep.nspawns; k++) {
		/* the yaw a dummy has (the spawn faces away from it, as below) */
		rep.spawnYaw[k] += M_PI_F;
		Con_Printf("instance: players spawn at %.0f %.0f %.0f (places)\n", (double)rep.spawn[k].x,
			(double)rep.spawn[k].y, (double)rep.spawn[k].z);
	}
	for (int i = 0; i < 2 && !rep.nspawns; i++) {
		DummySearch_Start(names[i], 1);
		while (rep.nspawns < REP_SPAWNS && DummySearch_NextEuler(name, sizeof(name), &p, &ang)) {
			rep.spawn[rep.nspawns] = p;
			rep.spawnYaw[rep.nspawns] = ang.y + M_PI_F;
			rep.nspawns++;
			Con_Printf("instance: players spawn at %s (%.0f %.0f %.0f)\n", name, (double)p.x, (double)p.y,
				(double)p.z);
		}
	}
	if (!rep.nspawns) {
		rep.spawn[0] = (vec3_t){ 0.0f, 0.0f, 0.0f };
		rep.spawnYaw[0] = 0.0f;
		rep.nspawns = 1;
	}
}

/* no ship or character (but skip) nearer to p than clear */
static byte Rep_Clear(const vec3_t *p, float clear, const cEntity *skip) {
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e == skip || e->deadNode.target || (!Entity_IsType(e, TYEN_CAR) && !Entity_IsType(e, TYEN_WALKCHAR))) {
			continue;
		}
		float dx = e->pos.x - p->x, dy = e->pos.y - p->y, dz = e->pos.z - p->z;
		float r = clear + WLife(e)->prim.radius;
		if (dx * dx + dy * dy + dz * dz < r * r) {
			return 0;
		}
	}
	return 1;
}

/* place k around at (0: at itself, then rings of eight REP_SPOT_STEP apart): on a floor at most drop below, clear of
 * walls and of others by clear (less than height), at height above the floor: 1 with *out */
static byte Rep_Spot(const vec3_t *at, int k, float drop, float clear, float height, const cEntity *skip,
	vec3_t *out) {
	vec3_t p = *at, hit;
	if (k > 0) {
		float r = REP_SPOT_STEP * (float)(1 + (k - 1) / 8);
		float a = (float)((k - 1) % 8) * (M_2PI_F / 8.0f) + (float)((k - 1) / 8) * 0.4f;
		p.x += r * sinf(a);
		p.z += r * cosf(a);
	}
	vec3_t top = { p.x, p.y + height, p.z }, down = { p.x, p.y - drop, p.z };
	/* the way from the point at to the place must be free too (no place behind a wall) */
	vec3_t from = { at->x, at->y + height, at->z };
	if ((k > 0 && Engine_TestLine(&from, &top, NULL, NULL)) || !Engine_TestLine(&top, &down, &hit, NULL)) {
		return 0;
	}
	vec3_t c = { hit.x, hit.y + height, hit.z };
	if (Engine_TestSphere(&c, &c, clear, NULL, NULL) || !Rep_Clear(&c, clear, skip)) {
		return 0;
	}
	*out = c;
	return 1;
}

/* the spawn point for a new character: the one with the fewest players near it (in turn among equals), and a
 * free place there; 0 when no spawn point has one */
static byte Rep_PickSpawn(vec3_t *pos, float *yaw, int *at) {
	int count[REP_SPAWNS];

	if (!rep.haveSpawn) {
		Rep_FindSpawns();
	}
	for (int s = 0; s < rep.nspawns; s++) {
		count[s] = 0;
		for (int i = 0; i < PLAYERS_MAX; i++) {
			cEntity *e = rep.pl[i].used ? Rep_Own(&rep.pl[i]) : NULL;
			float dx = e ? e->pos.x - rep.spawn[s].x : 0.0f, dz = e ? e->pos.z - rep.spawn[s].z : 0.0f;
			count[s] += e && dx * dx + dz * dz < REP_SPAWN_CROWD * REP_SPAWN_CROWD;
		}
	}
	for (int least = 0; least <= PLAYERS_MAX; least++) {
		for (int n = 0; n < rep.nspawns; n++) {
			int s = sand.spawn >= 0 ? sand.spawn % rep.nspawns : (rep.spawnNext + n) % rep.nspawns;
			if (count[s] != least && sand.spawn < 0) {
				continue;
			}
			for (int k = 0; k < REP_SPOT_TRIES; k++) {
				if (!rep.spawnBad[s] && Rep_Spot(&rep.spawn[s], k, REP_SPAWN_DROP, REP_CHAR_CLEAR, 200.0f, NULL, pos)) {
					*yaw = rep.spawnYaw[s];
					*at = s;
					rep.spawnNext = s + 1;
					return 1;
				}
			}
		}
	}
	return 0;
}

byte Rep_FreeSpot(const vec3_t *at, int first, vec3_t *out) {
	for (int k = first; k < REP_SPOT_TRIES; k++) {
		if (Rep_Spot(at, k, REP_SPAWN_DROP, REP_CHAR_CLEAR, 200.0f, NULL, out)) {
			return 1;
		}
	}
	return 0;
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

/* the dead character the player left, away */
static void Rep_DropCorpse(RepPlayer *pl) {
	cEntity *c = pl->corpse ? World_FindEntity(pl->corpseName) : NULL;
	if (c && c == pl->corpse && !c->deadNode.target) {
		cEntity_Del(c);
	}
	pl->corpse = NULL;
}

/* the player's character at pos (a spawn point when pos is NULL): its class of the faction or the state
 * (include/scrap/faction.h), Dtritus by default */
static cEntity *Rep_SpawnChar(int i, const vec3_t *pos, float yaw) {
	char name[REP_NAME];
	vec3_t p, ang;

	RepPlayer *pl = &rep.pl[i];
	pl->spawnAt = -1;
	Str_PrintfQuiet(name, sizeof(name), REP_PLAYER_PREFIX "%d_%u", i, ++pl->spawns);
	cEntity *e;
	if (Arena_On()) {
		/* the PvP modes: its garage ship at a free spawn point of the ships */
		if ((e = Arena_MakeShip(i, name)) == NULL) {
			return NULL;
		}
		p = e->pos;
	} else {
		if (pos) {
			p = *pos;
		} else if (Faction_SpawnPlace(i, &p, &yaw)) {
			/* a member that died in its office level: its jail dummy */
		} else if (!Rep_PickSpawn(&p, &yaw, &pl->spawnAt)) {
			p = rep.spawn[0];
			yaw = rep.spawnYaw[0];
		}
		if ((e = cWorld_CreateEntity(g_pWorld, name, p.x, p.y, p.z, TYEN_WALKCHAR)) == NULL) {
			return NULL;
		}
		WithLife_SetActCtrl(e, Faction_CharClass(i));
		Rep_SetVarS(e, "TgType", "Green");
		/* its death is the instance's (Rep_Death), not an NPC's of the scripts (CharAct: respawn from CharsNPC) */
		Rep_SetVarS(e, "OnDeath", "");
		ang = (vec3_t){ 0.0f, yaw, 0.0f };
		Obj_SetAng(e, &ang);
	}
	Rep_SetSlot(e, PLAYERS_NET_SLOT + i);
	Obj(e)->netStep = 1;
	Rep_DropCorpse(pl);
	Rep_SetOwn(pl, e);
	pl->lost = 0;
	pl->dead = 0;
	pl->spawnTick = rep.tick;
	pl->spawnY = p.y;
	Faction_Spawned(i, e);
	Coop_Spawned(e);
	return e;
}

/* the k-th ship player i owns while it is in the world, NULL */
static cEntity *Rep_OwnedShip(const RepPlayer *pl, int k) {
	cEntity *e = World_FindEntity(pl->shipNames[k]);
	return e && e == pl->ships[k] && !e->deadNode.target ? e : NULL;
}

/* the player that owns the ship (one it called), -1 */
static int Rep_ShipOwner(const cEntity *v) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		const RepPlayer *pl = &rep.pl[i];
		for (int k = 0; pl->used && k < pl->nships; k++) {
			if (pl->ships[k] == v && Rep_OwnedShip(pl, k) == v) {
				return i;
			}
		}
	}
	return -1;
}

/* the k-th ship off the player's list; the newest left is its own ship of the scripts (Parking.PlayerShip) */
static void Rep_Disown(int i, int k) {
	RepPlayer *pl = &rep.pl[i];
	memmove(&pl->ships[k], &pl->ships[k + 1], sizeof(pl->ships[0]) * (size_t)(pl->nships - k - 1));
	memmove(&pl->shipNames[k], &pl->shipNames[k + 1], sizeof(pl->shipNames[0]) * (size_t)(pl->nships - k - 1));
	pl->nships--;
	Players_SetShip(i, pl->nships ? Rep_OwnedShip(pl, pl->nships - 1) : NULL);
}

/* a parked ship no one sits in that player i may take: no other player owns it */
static byte Rep_IsParked(cEntity *e, int i) {
	if (!Entity_IsType(e, TYEN_CAR) || e->deadNode.target || Obj(e)->proxy || strncmp(e->name, "Parked", 6) ||
		cVehicleEntity_GetOwnerSlot(e) != -1 || WLife(e)->life <= 0) {
		return 0;
	}
	int owner = Rep_ShipOwner(e);
	return owner < 0 || owner == i;
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

/* the price of the ship's model (ShipMaker.GetShipPrice), 0 */
static int Rep_ShipPrice(cEntity *v) {
	nativeVal_t out, n;
	int price = 0;
	const char *model = Rep_VarS(v, "Model");

	Native_SetNone(&out);
	Native_SetNone(&n);
	if (!model[0] || !Mode_Import("ShipMaker") || !PORT_CALL(ShipMaker, GetShipPrice, &out, "s", model) ||
		!Native_ToInt(&out, &n) || !Native_ArgInt(&n, &price)) {
		Native_ErrClear();
		price = 0;
	}
	Native_Free(&out);
	Native_Free(&n);
	return price;
}

/* the sandbox: a player on foot with no parked ship near calls one beside it (Parking.CreateParkedShip): it owns it;
 * past the -ships limit its oldest goes (or stays to the player that sits in it, owned no more); the ship of a death
 * is paid first (-rebuild) */
static cEntity *Rep_CallShip(int i, cEntity *ch) {
	RepPlayer *pl = &rep.pl[i];
	nativeVal_t c[3], pos, ang, out;
	cEntity *ship = NULL;
	vec3_t at, p;

	/* the money is the bound player's (Rep_PreFrame binds it); the coop story calls no ship indoors */
	if ((pl->rebuild > 0 && g_pWorld->money < pl->rebuild) || Coop_NoShipCall()) {
		return NULL;
	}
	float yaw = WChar(ch)->view.y;
	/* in front of the character (yaw 0 looks to -z), else a free place around that */
	at = (vec3_t){ ch->pos.x - sinf(yaw) * 900.0f, ch->pos.y, ch->pos.z - cosf(yaw) * 900.0f };
	int k = 0;
	while (k < REP_SPOT_TRIES && !Rep_Spot(&at, k, REP_CALL_DROP, REP_SHIP_CLEAR, 600.0f, ch, &p)) {
		k++;
	}
	if (k == REP_SPOT_TRIES) {
		return NULL;
	}
	while (pl->nships >= sand.ships) {
		cEntity *old = Rep_OwnedShip(pl, 0);
		if (old && Rep_IsParked(old, i)) {
			cEntity_Del(old);
		}
		Rep_Disown(i, 0);
	}
	/* the player's own ship when it has one (include/scrap/travel.h), else a new one */
	if ((ship = Faction_CallShip(i, &p, yaw)) == NULL && (ship = Travel_OwnShip(i, &p, yaw)) == NULL) {
		Native_SetFloat(&c[0], p.x);
		Native_SetFloat(&c[1], p.y);
		Native_SetFloat(&c[2], p.z);
		Mode_Tuple3(&pos, &c[0], &c[1], &c[2]);
		Native_SetFloat(&ang, yaw);
		Native_SetNone(&out);
		if (PORT_CALL(Parking, CreateParkedShip, &out, "vv", &pos, &ang) && out.type == NV_ENT) {
			ship = Native_Ent(&out);
		} else {
			Native_ErrPrint();
		}
		Native_Free(&out);
		Native_Free(&pos);
	}
	if (ship) {
		pl->calls++;
		if (pl->rebuild > 0) {
			g_pWorld->money -= pl->rebuild;
			Con_Printf("instance: player %d paid %d for its ship\n", i, pl->rebuild);
			pl->rebuild = 0;
		}
		pl->ships[pl->nships] = ship;
		Str_Copy(pl->shipNames[pl->nships], ship->name, REP_NAME);
		pl->nships++;
		Players_SetShip(i, ship);
		Con_Printf("instance: player %d called %s\n", i, ship->name);
	}
	return ship && Rep_IsParked(ship, i) ? ship : NULL;
}

/* the switch: into the nearest parked ship (a new one when none is near), out of the ship */
static void Rep_Switch(int i, cEntity *e) {
	RepPlayer *pl = &rep.pl[i];
	char name[REP_NAME];

	if (NetPl_Kind(e) == NETREP_CHAR) {
		cEntity *best = NULL;
		float bestD = PLAY_BOARD_RADIUS * PLAY_BOARD_RADIUS;
		for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
			if (!Rep_IsParked(v, i)) {
				continue;
			}
			float dx = v->pos.x - e->pos.x, dy = v->pos.y - e->pos.y, dz = v->pos.z - e->pos.z;
			float d = dx * dx + dy * dy + dz * dz;
			if (d < bestD) {
				bestD = d;
				best = v;
			}
		}
		if (!best && !(best = Rep_CallShip(i, e))) {
			return;
		}
		Str_Copy(name, best->name, sizeof(name));
		Rep_SetVarS(best, "MPil0", Faction_Pilot(i));
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
		Travel_Boarded(i, best);
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
	Native_ErrPrint();
	Rep_SpawnChar(i, &p, yaw);
	NetRepInBuf_Restart(&pl->in);
	pl->told = -2;
	pl->leaves++;
	Con_Printf("instance: player %d out of %s\n", i, name);
}

/* the ship stands near enough above the floor for a parking box under it (Parking.SetShipAsParked) */
static byte Rep_NearFloor(cEntity *v) {
	vec3_t down = { v->pos.x, v->pos.y - REP_PARK_HEIGHT, v->pos.z }, hit;
	return Engine_TestLine(&v->pos, &down, &hit, NULL) != 0;
}

/* the player's entity gone: a character away, a ship parked when it is near the floor, else left empty (a dead one
 * left to the game: its wreck); the scripts run for that player */
static void Rep_DropOwn(int i) {
	RepPlayer *pl = &rep.pl[i];
	cEntity *e = Rep_Own(pl);

	Rep_SetOwn(pl, NULL);
	if (!e) {
		return;
	}
	Players_Bind(i);
	Obj(e)->netStep = 0;
	if (NetPl_Kind(e) == NETREP_SHIP) {
		Rep_SetSlot(e, -1);
		if (WLife(e)->life > 0 && Arena_On()) {
			cEntity_Del(e);
		} else if (WLife(e)->life > 0 && Rep_NearFloor(e)) {
			PORT_CALL(Parking, SetShipAsParked, NULL, "s", e->name);
			Native_ErrPrint();
		}
	} else {
		Rep_SetSlot(e, -1);
		if (WLife(e)->life > 0) {
			cEntity_Del(e);
		} else {
			/* a dead character lies there until the player's next one */
			pl->corpse = e;
			Str_Copy(pl->corpseName, e->name, sizeof(pl->corpseName));
		}
	}
	Players_BindTurn();
}

/* the game's cost of a death (MissionsFuncs.SinglePlayerDeath and Vehicles.OnDeath of single player): the alarm
 * off, a life less (-lives; at none left the player starts again: single player's jail is another level), its own
 * ship lost (-rebuild: the next call costs its price) */
static void Rep_Death(int i, cEntity *e) {
	RepPlayer *pl = &rep.pl[i];

	pl->deaths++;
	pl->dead = 1;
	Players_Bind(i);
	Alarm_Set(&g_pWorld->alarm, 0.0f);
	Faction_Died(i);
	Arena_Died(i, e);
	if (sand.rebuild && NetPl_Kind(e) == NETREP_SHIP && Rep_ShipOwner(e) == i) {
		pl->rebuild = Rep_ShipPrice(e);
	}
	if (sand.lives && --pl->lives <= 0) {
		pl->lives = sand.lives;
		g_pWorld->money = Instance_StartMoney();
		pl->rebuild = 0;
		Con_Printf("instance: player %d has no lives left: starts again\n", i);
	}
	Con_Printf("instance: player %d died in %s (%d lives left, ship cost %d)\n", i, e->name, sand.lives ? pl->lives : -1,
		pl->rebuild);
	Players_BindTurn();
}

/* the use button of a character at a door (an elevator, a metro station, a teleporter): the game's use of its arrow
 * runs (the step of the input) in place of the switch into a ship */
static byte Rep_UsesDoor(cEntity *e) {
	if (NetPl_Kind(e) != NETREP_CHAR) {
		return 0;
	}
	cEntity *t = WChar_Victim(e);
	if (!t || !Entity_IsType(t, TYEN_WITH_LIFE) || !WLife(t)->arrow.canUse) {
		return 0;
	}
	const char *f = cPy_Function_Name(&WLife(t)->arrow.onUse);
	return !strcmp(f, "UseElevator") || !strcmp(f, "OnUse") || Coop_UseAny(t);
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
		pl->lives = sand.lives;
		NetRepInBuf_Init(&pl->in);
		/* the ticket of a coordinator's world: the player's state and its entrance (include/scrap/travel.h) */
		TravelSpawn sp;
		if (!Travel_Admit(conn, i, &sp)) {
			return -1;
		}
		Players_Join(i, sp.money);
		if (Arena_On()) {
			/* the PvP modes: no ship before its join and its first spawn (Arena_SpawnDue) */
			pl->lost = rep.tick ? rep.tick : 1;
			Arena_Joined(i);
		} else if (!Rep_SpawnChar(i, sp.have ? &sp.pos : NULL, sp.yaw)) {
			Players_Part(i);
			Travel_Left(i);
			return -1;
		}
		pl->used = 1;
		rep.players++;
		Con_Printf("instance: player %d on connection %d (%s)\n", i, conn, pl->ownName);
		Travel_Joined(i);
		Coop_Joined(i);
		return i;
	}
	return -1;
}

static void Rep_Leave(int i) {
	RepPlayer *pl = &rep.pl[i];
	Con_Printf("instance: player %d left (%u inputs, %u late, %u missed, %u shots, %u hits)\n", i, pl->in.got,
		pl->in.late, pl->in.missed, pl->shots, pl->hits);
	Travel_Left(i);
	Faction_Left(i);
	Arena_Left(i);
	Rep_DropOwn(i);
	Rep_DropCorpse(pl);
	/* its called ships stay in the world, owned by no one */
	while (pl->nships) {
		Rep_Disown(i, 0);
	}
	Players_Part(i);
	pl->used = 0;
	rep.players--;
}

void Rep_TestSwitch(int i, byte call) {
	cEntity *e = i >= 0 && i < PLAYERS_MAX && rep.pl[i].used ? Rep_Own(&rep.pl[i]) : NULL;
	if (!e) {
		Con_Printf("instance: no player %d\n", i);
		return;
	}
	Players_Bind(i);
	if (!call) {
		Rep_Switch(i, e);
	} else if (NetPl_Kind(e) == NETREP_CHAR) {
		Rep_CallShip(i, e);
	}
	Players_BindTurn();
}

int Rep_PlayerConn(int i) {
	return i >= 0 && i < PLAYERS_MAX && rep.pl[i].used ? rep.pl[i].conn : -1;
}

int Rep_ConnPlayer(int conn) {
	return conn >= 0 && conn < REP_CONNS && rep.conn[conn].c ? rep.conn[conn].player : -1;
}

void Rep_Respawn(int i) {
	RepPlayer *pl = &rep.pl[i];
	if (i < 0 || i >= PLAYERS_MAX || !pl->used) {
		return;
	}
	Rep_DropOwn(i);
	Rep_SpawnChar(i, NULL, 0.0f);
	NetRepInBuf_Restart(&pl->in);
	pl->told = -2;
}

void Rep_Placed(int i) {
	if (i >= 0 && i < PLAYERS_MAX) {
		rep.pl[i].spawnAt = -1;
	}
}

void Rep_Remove(int i) {
	RepPlayer *pl = i >= 0 && i < PLAYERS_MAX && rep.pl[i].used ? &rep.pl[i] : NULL;
	if (!pl) {
		return;
	}
	Rep_DropOwn(i);
	pl->lost = rep.tick ? rep.tick : 1;
	pl->dead = 0;
	NetRepInBuf_Restart(&pl->in);
	pl->told = -2;
}

void Rep_Rebody(int i, const vec3_t *pos, float yaw) {
	RepPlayer *pl = i >= 0 && i < PLAYERS_MAX && rep.pl[i].used ? &rep.pl[i] : NULL;
	cEntity *e = pl ? Rep_Own(pl) : NULL;
	/* a dead player only at a place (a revive) */
	if (!pl || (!e && (!pos || !pl->dead)) || (e && !pos && NetPl_Kind(e) != NETREP_CHAR)) {
		return;
	}
	vec3_t p = pos ? *pos : e->pos;
	if (!pos) {
		yaw = WChar(e)->view.y;
	}
	Rep_DropOwn(i);
	Rep_SpawnChar(i, &p, yaw);
	NetRepInBuf_Restart(&pl->in);
	pl->told = -2;
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
			Rep_Death(i, e);
			Rep_DropOwn(i);
			e = NULL;
		}
		if (!e && Coop_Holds(i)) {
			continue;		/* hidden by a scene of the coop story or in its ship editor (include/scrap/coop.h) */
		}
		if (e != pl->own || !e) {
			/* dead or deleted by the game: a new Dtritus after a while */
			if (!pl->lost) {
				pl->lost = tick;
			}
			if (Arena_On() ? Arena_SpawnDue(i, tick - pl->lost) :
				tick - pl->lost >= (uint64_t)sand.respawnTicks + Faction_DownedTicks(i)) {
				Rep_DropOwn(i);
				Rep_SpawnChar(i, NULL, 0.0f);
				NetRepInBuf_Restart(&pl->in);
				pl->told = -2;
			}
			continue;
		}
		pl->lost = 0;
		if (pl->spawnAt >= 0 && tick - pl->spawnTick < REP_SPAWN_CHECK && e->pos.y < pl->spawnY - REP_SPAWN_FALL) {
			Con_Printf("instance: player %d fell from spawn point %d: not used again\n", i, pl->spawnAt);
			rep.spawnBad[pl->spawnAt] = 1;
			Rep_DropOwn(i);
			Rep_SpawnChar(i, NULL, 0.0f);
			NetRepInBuf_Restart(&pl->in);
			pl->told = -2;
			continue;
		}
		/* the money, the alarm and the scripts' player of this player while its input runs */
		Players_Bind(i);
		NetRepInBuf_Take(&pl->in, (uint32_t)tick, &in);
		pl->shownNow = pl->shown[tick % NETREP_INBUF];
		Faction_Input(i, e, &in);
		Places_Input(i, e, &in);
		if ((in.buttons & NETREP_IN_SWITCH) && !(pl->buttons & NETREP_IN_SWITCH) && !Arena_On()) {
			if (!Rep_UsesDoor(e)) {
				pl->buttons = in.buttons;
				Rep_Switch(i, e);
				continue;
			}
			Coop_BeforeUse(i, WChar(e)->listenOn && WChar_Listen(e) ? WChar_Listen(e) : WChar_Victim(e));
		}
		pl->buttons = in.buttons;
		NetPl_Step(e, &in, 0, 0);
	}
	Players_BindTurn();
}

byte Rep_Command(const char *line) {
	int i, life;
	if (sscanf(line, "life %d %d", &i, &life) != 2) {
		return 0;
	}
	cEntity *e = i >= 0 && i < PLAYERS_MAX && rep.pl[i].used ? Rep_Own(&rep.pl[i]) : NULL;
	if (e) {
		WLife(e)->life = life;
	}
	Con_Printf("instance: life of player %d (%s) %d\n", i, e ? e->name : "-", life);
	return 1;
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
		/* the entities of the last snapshot: one may be gone since (World_FindEntity of its name is not it) */
		cEntity *e = rep.ent[s].e;
		if (!e || e == v || World_FindEntity(rep.ent[s].name) != e || e->deadNode.target ||
			!NetRepSrv_PosAt(rep.srv, s, t, &p.x)) {
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
		int kind = e ? NetPl_Kind(e) : pl->dead ? NETREP_DEAD : 0;
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
	Str_Copy(x->shooterName, shooter->name, REP_NAME);
	Str_Copy(x->targetName, target ? target->name : "", REP_NAME);
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
		/* the entities may be gone since their shot */
		int slot = World_FindEntity(x->shooterName) == x->shooter ? Rep_Find(x->shooter) : -1;
		if (slot < 0) {
			continue;
		}
		NetRepShot *w = &rep.wire[n];
		int t = x->target && World_FindEntity(x->targetName) == x->target ? Rep_Find(x->target) : -1;
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
	NetRepSrv_SetView(rep.conn[conn].c, view[0], view[1], view[2], Arena_On() ? ARENA_INTEREST : 0.0f);
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
