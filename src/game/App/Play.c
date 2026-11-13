/*
 * The client's own player: input, prediction and correction of its entity, its camera, see include/scrap/play.h.
 * Not in the original.
 *
 * The entity is stepped once per tick (NetPl_Step) with the input of that tick, the controls of the frames since the
 * tick before averaged. The drawn position runs one tick behind the stepped one: the slot of the entity is set to the
 * tick, so cObjEntity_Update draws between the position of the tick before and the new one; a correction adds an
 * offset that fades out (NetRepSmooth). The true predicted position is kept apart and put back before each step.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/arena.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/scorer.h"
#include "scrap/scrapcollision.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

#define PLAY_CAM "PlayCam"
#define PLAY_DEATH_CAM "PlayDeathCam"
#define PLAY_DEATH_DIST 1800.0f			/* units of the death camera from the place of the death */
#define PLAY_DEATH_HIGH 900.0f
#define PLAY_DEATH_TURN 0.35f			/* radians a second it goes round */
#define PLAY_NAME "Me%u"
#define PLAY_MAX_CATCHUP 8				/* ticks stepped in one frame at most; more ahead is a jump */
#define PLAY_BOT_NPC 2					/* the bot hunts ships of no player */
#define PLAY_BOT_TARGET 3				/* the bot takes a ship and stays in the air (the weapon tests) */
#define PLAY_BOT_WALK 4					/* the bot walks around and stays on foot (collisions of characters) */
#define PLAY_BOT_IDLE 5					/* the bot gives no input (the tests drive it from the instance) */
#define PLAY_BOT_USE 6					/* the bot stands and presses use every few seconds (doors) */
#define PLAY_BOT_FACE 7					/* the bot stands and turns to the nearest other player (screenshots) */
#define PLAY_BOT_FACE_SIDE 0.45f		/* radians it looks past it: the camera behind the own character is on the
										   line to the other one, which the own character would hide */
#define PLAY_ERRS 16384					/* errors kept for the percentiles of the status */

/* what the prediction collides with at the predicted tick (SCRAP_PLAYAHEAD=0; the players, the default; =all: the
 * characters near it too, which made the error of characters in a crowd larger: p95 5.6 to 9.0 with 32 bots) */
enum { PLAY_AHEAD_NONE, PLAY_AHEAD_PLAYERS, PLAY_AHEAD_ALL };

byte g_Play;
byte g_HeadlessPlay;

typedef struct PlayStats {
	uint32_t states, corrections, unknown;
	uint32_t n;							/* errors kept */
	float err[PLAY_ERRS];				/* prediction error of each server state */
	float corr[PLAY_ERRS];				/* the size of each correction (the true position jumped) */
	uint32_t nc;
	uint32_t replayed;					/* ticks run again */
} PlayStats;

static struct {
	int player;
	int slot, kind;						/* the own replication slot, -1 none */
	uint32_t slotTick;
	float phys[NETREP_PHYS_MAX];		/* the movement constants of the own entity */
	int nphys;
	cEntity *e;
	uint32_t made;
	uint32_t tick;						/* the last tick stepped, 0 none */
	NetRepPredHist hist;
	NetPlMotion motion[NETREP_PRED];	/* the full motion state after each tick, by tick % NETREP_PRED */
	NetPlAccum acc;
	NetRepInput input;					/* of the last tick */
	NetRepSmooth smooth;
	vec3_t truePos;						/* the predicted position; the entity stands at it plus the offset */
	vec3_t drawn;						/* where it was drawn at the end of the last tick */
	vec3_t lastServer;
	int pending;						/* ticks stepped since the last ack */
	byte catchup;						/* the new own entity steps up to the input tick with no input */
	uint32_t shownMs;
	double tickLocal;					/* local time of the last tick */
	cEntity *cam;
	byte started;
	byte dead;							/* the death camera looks at deathAt (the own entity died, no new one yet) */
	vec3_t deathAt;
	float deathYaw;
	float lastYaw;						/* of the own entity at its last tick */
	NetRepStatus status;				/* the newest status of the server */
	byte haveStatus;
	PlayStats st;
	FILE *log;
	byte bot;							/* 1, PLAY_BOT_NPC, PLAY_BOT_TARGET, PLAY_BOT_WALK, PLAY_BOT_IDLE, PLAY_BOT_USE */
	byte botCycle;
	byte botCm;							/* SCRAP_PLAYBOT_CM=1: also the countermeasure of the weapon now and then */
	byte ahead;							/* SCRAP_PLAYAHEAD: PLAY_AHEAD_ */
	double botNear;						/* s the bot aimed at its target from close by (the weapon turns) */
	double botStart;					/* local time the own entity of now came */
} play = { .slot = -1, .player = -1 };

byte Play_HeadlessDue(void) {
	static uint64_t next;
	uint64_t now = Plat_RealNs(), period = 1000000000ull / PLAY_HEADLESS_HZ;
	if (now < next) {
		Plat_WaitEvent((int)((next - now + 999999) / 1000000));
		return 0;
	}
	next = now - next > period ? now + period : next + period;
	return 1;
}

byte Play_ParseParam(const char *param, const char *ext) {
	(void)ext;
	return !crt_stricmp("play", param) && Watch_ParseParam("watch", ext);
}

int Play_OwnSlot(void) {
	return play.slot;
}

cEntity *Play_Entity(void) {
	return play.e;
}

byte Play_DeathCamOn(void) {
	return play.dead && !play.e;
}

/* ---- the entity and its camera ---- */

/* the death camera: a camera of its own around the place of the death, no HUD, until the next own entity */
static void Play_DeathCam(float dt) {
	cEntity *cam = World_FindEntity(PLAY_DEATH_CAM);
	int one = 1;

	if (!cam) {
		cam = cWorld_CreateEntity(g_pWorld, PLAY_DEATH_CAM, play.deathAt.x, play.deathAt.y + PLAY_DEATH_HIGH,
			play.deathAt.z, TYEN_FCAM);
		if (!cam) {
			return;
		}
		Entity_VarI(cam, "FreeCam", &one, 1);
	}
	if (dt <= 0.0f) {
		cWorld_SetCam(g_pWorld, 0, PLAY_DEATH_CAM);
		cScorer *sc = Scorer_Get(0);
		if (sc) {
			cScorer_Clear(sc);
		}
	}
	play.deathYaw += PLAY_DEATH_TURN * dt;
	/* behind the place for the entity's yaw (its yaw 0 looks to -z), nearer when a wall is between; the camera's
	 * yaw 0 looks to +z, a positive pitch looks down */
	vec3_t at = { play.deathAt.x, play.deathAt.y + 100.0f, play.deathAt.z }, hit;
	vec3_t p = { at.x + sinf(play.deathYaw) * PLAY_DEATH_DIST, at.y + PLAY_DEATH_HIGH,
		at.z + cosf(play.deathYaw) * PLAY_DEATH_DIST };
	float k = 1.0f;
	if (Engine_TestLine(&at, &p, &hit, NULL)) {
		float dx = hit.x - at.x, dz = hit.z - at.z;
		k = sqrtf(dx * dx + dz * dz) / PLAY_DEATH_DIST * 0.85f;
		p = (vec3_t){ at.x + (p.x - at.x) * k, at.y + (p.y - at.y) * k, at.z + (p.z - at.z) * k };
	}
	CGameCamera *c = &((cCamControl *)cam)->cam;
	c->pos = p;
	c->rot = (vec3_t){ atan2f(PLAY_DEATH_HIGH, PLAY_DEATH_DIST), play.deathYaw + M_PI_F, 0.0f };
}

static void Play_Camera(void) {
	cEntity *e = play.e;
	char *s;
	int zero = 0;

	if (!e) {
		return;
	}
	play.dead = 0;
	play.cam = World_FindEntity(PLAY_CAM);
	if (!play.cam) {
		play.cam = cWorld_CreateEntity(g_pWorld, PLAY_CAM, e->pos.x, e->pos.y + 500.0f, e->pos.z, TYEN_FCAM);
	}
	if (!play.cam) {
		return;
	}
	cWorld_SetCam(g_pWorld, 0, PLAY_CAM);
	s = e->name;
	Entity_VarS(play.cam, "MainTarget", &s, 1);
	Entity_VarS(play.cam, "Target", &s, 1);
	Entity_VarI(play.cam, "FreeCam", &zero, 1);
	/* the ship's camera switches views with its button; a character's has nothing to switch */
	s = play.kind == NETREP_SHIP ? "Vehicles.SwitchView" : "";
	Entity_VarS(play.cam, "OnSwitch", &s, 1);
	Input_SelectActionSetNamed(play.kind == NETREP_SHIP ? "Racer" : "Walk");
	/* the HUD of single player for the walker or the racer of view 0 */
	if (play.kind == NETREP_SHIP) {
		if (Mode_Import("RacerScorer")) {
			PORT_CALL(RacerScorer, Create, NULL, "i", 0L);
		}
	} else if (Mode_Import("CharScorer")) {
		PORT_CALL(CharScorer, Create, NULL, "i", 0L);
	}
}

void Play_OwnRemove(void) {
	if (play.e) {
		if (World_FindEntity(play.e->name) == play.e) {
			cEntity_Del(play.e);
		}
		play.e = NULL;
	}
	play.tick = 0;
}

void Play_SetOwn(int player, int slot, int kind, uint32_t tick, const float *phys, int nphys) {
	play.player = player;
	if (slot == NETREP_NO_SLOT) {
		slot = -1;
	}
	memcpy(play.phys, phys, (size_t)nphys * sizeof(float));
	play.nphys = nphys;
	if (slot == play.slot) {
		if (play.e) {
			NetPl_SetPhys(play.e, play.phys, play.nphys);
		}
		return;
	}
	/* after a death the death camera looks at the place until the new entity comes (the own slot may be gone
	 * already: the look of a wreck is another); the entity of the slot before stays as its proxy */
	if (slot < 0 && kind == NETREP_DEAD && play.started && !play.dead) {
		char *none = "";
		play.dead = 1;
		play.deathAt = play.truePos;
		play.deathYaw = play.lastYaw;
		if (play.cam) {
			Entity_VarS(play.cam, "OnSwitch", &none, 1);
		}
		Play_DeathCam(0.0f);
		Con_Printf("play: died at %.0f %.0f %.0f\n", (double)play.deathAt.x, (double)play.deathAt.y,
			(double)play.deathAt.z);
	}
	if (play.e && play.slot >= 0 && World_FindEntity(play.e->name) == play.e) {
		play.e->pos = play.truePos;
		Watch_Adopt(play.slot, play.e, play.kind);
		play.e = NULL;
	}
	Play_OwnRemove();
	play.slot = slot;
	play.kind = kind;
	play.slotTick = tick;
	Con_Printf("play: player %d, slot %d, kind %d from tick %u\n", player, slot, kind, tick);
}

/* the ammo of the status into the own ship */
static void Play_ApplyAmmo(void) {
	cEntity *e = play.e;
	if (!play.haveStatus || !e || NetPl_Kind(e) != NETREP_SHIP || !Veh(e)->weapons.ammo) {
		return;
	}
	int n = play.status.nammo < g_pWorld->ammo.count ? play.status.nammo : g_pWorld->ammo.count;
	for (int k = 0; k < n; k++) {
		Veh(e)->weapons.ammo[k] = play.status.ammo[k];
	}
}

void Play_OwnStatus(const NetRepStatus *st) {
	if (play.haveStatus && (int32_t)(st->tick - play.status.tick) < 0) {
		return;			/* an older one that came late */
	}
	play.status = *st;
	play.haveStatus = 1;
	g_pWorld->money = st->money;
	/* the level held where the server has it: the client's alarm does not grow or fall by itself */
	g_pWorld->alarm.value = st->alarm / 255.0f;
	g_pWorld->alarm.on = (st->flags & NETREP_ST_ALARM) != 0;
	g_pWorld->alarm.grow = 0.0f;
	Play_ApplyAmmo();
}

/* the state of a record into the entity: position, velocity, angles, the view of a character */
static void Play_SetState(cEntity *e, const NetRepState *st) {
	vec3_t v;
	for (int k = 0; k < 3; k++) {
		(&v.x)[k] = NetRep_UPos(st->pos[k]);
	}
	e->pos = v;
	Obj(e)->vel = (vec3_t){ (float)st->vel[0], (float)st->vel[1], (float)st->vel[2] };
	Obj(e)->ang = (vec3_t){ NetRep_UAng(st->ang[0]), NetRep_UAng(st->ang[1]), NetRep_UAng(st->ang[2]) };
	WLife(e)->life = st->life;
	if (NetPl_Kind(e) == NETREP_CHAR) {
		cWalkCharEntity *w = WChar(e);
		w->view = (vec3_t){ NetRep_UAng(st->view[0]), NetRep_UAng(st->view[1]), 0.0f };
		w->falling = (st->flags & NETREP_FL_FALLING) != 0;
		w->walking = (st->flags & NETREP_FL_WALKING) != 0;
	}
}

void Play_OwnCreate(int kind, const char *look, uint32_t tick, const NetRepState *st) {
	char name[32];
	int zero = 0;

	Play_OwnRemove();
	Str_PrintfQuiet(name, sizeof(name), PLAY_NAME, ++play.made);
	play.e = Watch_Build(name, kind, look, st, 0);
	if (!play.e) {
		return;
	}
	cEntity *e = play.e;
	play.kind = kind;
	Entity_VarI(e, "UsrControl", &zero, 1);
	Obj(e)->netStep = 1;
	NetPl_SetPhys(e, play.phys, play.nphys);
	Play_SetState(e, st);
	Obj(e)->slotPos = e->pos;
	Obj(e)->renderPos = e->pos;
	play.truePos = e->pos;
	play.drawn = e->pos;
	NetRepPredHist_Init(&play.hist);
	NetRepSmooth_Init(&play.smooth, PLAY_SMOOTH_HALF, PLAY_SMOOTH_SNAP);
	memset(&play.acc, 0, sizeof(play.acc));
	memset(&play.input, 0, sizeof(play.input));
	if (kind == NETREP_SHIP) {
		play.acc.weapon = play.input.weapon = (uint8_t)Veh(e)->controls.weapon;
		Play_ApplyAmmo();
	}
	/* the ticks from the record's on: the server ran them with no input since the switch (NetRepInBuf_Restart) */
	play.tick = tick;
	play.catchup = 1;
	play.botStart = 0.0;
	Con_Printf("play: own %s %s at %.0f %.0f %.0f (tick %u)\n", kind == NETREP_SHIP ? "ship" : "character", name,
		(double)e->pos.x, (double)e->pos.y, (double)e->pos.z, tick);
	if (play.started) {
		Play_Camera();
	}
}

void Play_Start(void) {
	const char *path = getenv("SCRAP_PLAYLOG");
	play.started = 1;
	if (path && *path && !play.log) {
		play.log = fopen(path, "w");
		if (play.log) {
			fputs("tick,kind,err,corr,replayed,sx,sy,sz,px,py,pz\n", play.log);
		}
	}
	const char *bot = getenv("SCRAP_PLAYBOT");
	play.bot = !bot || !*bot || *bot == '0' ? 0 : !crt_stricmp(bot, "npc") ? PLAY_BOT_NPC :
		!crt_stricmp(bot, "target") ? PLAY_BOT_TARGET : !crt_stricmp(bot, "walk") ? PLAY_BOT_WALK :
		!crt_stricmp(bot, "idle") ? PLAY_BOT_IDLE : !crt_stricmp(bot, "use") ? PLAY_BOT_USE : !crt_stricmp(bot, "face") ? PLAY_BOT_FACE : 1;
	const char *ahead = getenv("SCRAP_PLAYAHEAD");
	play.ahead = !ahead || !*ahead ? PLAY_AHEAD_PLAYERS : *ahead == '0' ? PLAY_AHEAD_NONE : *ahead == 'a' ?
		PLAY_AHEAD_ALL : PLAY_AHEAD_PLAYERS;
	const char *cycle = getenv("SCRAP_PLAYBOT_WEAPONS");
	play.botCycle = cycle && *cycle && *cycle != '0';
	const char *cm = getenv("SCRAP_PLAYBOT_CM");
	play.botCm = cm && *cm == '1';
	/* the missile weapons fire as the client of the game's network mode: the launch effects, no missile */
	if (g_SwarmIDWeap >= 0 && g_SwarmIDWeap < g_pWorld->weapons.count) {
		cPy_Function_SetFunc(&TABLE_AT(g_pWorld->weapons, g_SwarmIDWeap)->onFire, "!SwarmClientFire");
	}
	if (g_InfernoIDWeap >= 0 && g_InfernoIDWeap < g_pWorld->weapons.count) {
		cPy_Function_SetFunc(&TABLE_AT(g_pWorld->weapons, g_InfernoIDWeap)->onFire, "!InfernoClientFire");
	}
	Play_Camera();
}

/* ---- prediction ---- */

static void Play_Record(uint32_t tick) {
	NetRepPred p;
	cEntity *e = play.e;

	memset(&p, 0, sizeof(p));
	p.tick = tick;
	p.pos[0] = e->pos.x;
	p.pos[1] = e->pos.y;
	p.pos[2] = e->pos.z;
	p.vel[0] = Obj(e)->vel.x;
	p.vel[1] = Obj(e)->vel.y;
	p.vel[2] = Obj(e)->vel.z;
	const vec3_t *a = NetPl_Kind(e) == NETREP_CHAR ? &WChar(e)->view : &Obj(e)->ang;
	p.ang[0] = a->x;
	p.ang[1] = a->y;
	p.ang[2] = a->z;
	NetRepPredHist_Put(&play.hist, &p, &play.input);
	NetPl_Save(e, &play.motion[tick % NETREP_PRED]);
}

/* the entity drawn from where it was to the true position plus the offset over the next tick */
static void Play_Draw(void) {
	cEntity *e = play.e;
	cObjEntity *o = Obj(e);
	vec3_t d = { play.truePos.x + play.smooth.off[0], play.truePos.y + play.smooth.off[1],
		play.truePos.z + play.smooth.off[2] };
	o->slotPos = play.drawn;
	e->pos = d;
	play.drawn = d;
	o->slot->interval = 1.0f / NETREP_TICK_HZ;
	o->slot->time = o->slot->interval;
}

static void Play_Step(uint32_t tick, byte none) {
	cEntity *e = play.e;
	vec3_t view0 = NetPl_Kind(e) == NETREP_CHAR ? WChar(e)->view : Obj(e)->ang;
	vec3_t ang0 = Obj(e)->ang;

	if (!none) {
		NetPl_Input(&play.acc, &play.input);
	}
	/* asleep (include/scrap/faction.h): no input, as the server steps it */
	if (none || Faction_OwnAsleep()) {
		uint8_t weapon = play.input.weapon;
		memset(&play.input, 0, sizeof(play.input));
		play.input.weapon = weapon;
	}
	play.input.tick = tick;
	play.input.slot = (uint16_t)play.slot;
	e->pos = play.truePos;
	if (play.ahead != PLAY_AHEAD_NONE) {
		Watch_ProxiesAt((double)tick / NETREP_TICK_HZ, play.ahead == PLAY_AHEAD_PLAYERS);
	}
	NetPl_Step(e, &play.input, 1, 0);
	Watch_ProxiesBack();
	play.truePos = e->pos;
	play.lastYaw = NetPl_Kind(e) == NETREP_CHAR ? WChar(e)->view.y : Obj(e)->ang.y;
	Play_Record(tick);
	play.tick = tick;
	play.pending++;
	NetRepSmooth_Step(&play.smooth, 1.0f / NETREP_TICK_HZ);
	/* the view and angles drawn from the tick before */
	Obj(e)->slotAng = ang0;
	if (NetPl_Kind(e) == NETREP_CHAR) {
		WChar(e)->slotView = view0;
	}
	Play_Draw();
}

/* the nearest proxy within 40000 units: a player's (player) or of no player, of a kind (0 any), standing still
 * (still 1) or moving (still 2) */
static cEntity *Play_BotNearest(cEntity *e, int kind, byte player, byte still) {
	cEntity *best = NULL;
	float bestD = 40000.0f * 40000.0f;
	for (cEntity *o = cWorld_GetFirstEntity(g_pWorld); o; o = cEntity_Next(o)) {
		if (o == e || !Obj(o)->proxy || o->deadNode.target || Watch_IsPlayer(o) != player ||
			(kind && NetPl_Kind(o) != kind) ||
			(still && (still == 1) == (Obj(o)->vel.x != 0.0f || Obj(o)->vel.z != 0.0f))) {
			continue;
		}
		float dx = o->pos.x - e->pos.x, dy = o->pos.y - e->pos.y, dz = o->pos.z - e->pos.z;
		float d = dx * dx + dy * dy + dz * dz;
		if (d < bestD) {
			bestD = d;
			best = o;
		}
	}
	return best;
}

/* the weapon of the bot: the Vulcan, or with SCRAP_PLAYBOT_WEAPONS=1 (or a list of names) each weapon for 6 s of
 * aiming in turn */
static int Play_BotWeapon(void) {
	static const char *const names[] = { "Vulcan", "Laser", "Devastator", "Tesla", "ATPC", "Swarm", "Inferno" };
	const int w[] = { g_VulcanIDWeap, g_LaserIDWeap, g_DevastatorIDWeap, g_TeslaIDWeap, g_ATPCIDWeap, g_SwarmIDWeap,
		g_InfernoIDWeap };
	int pick[7], n = 0;
	if (!play.botCycle) {
		return g_VulcanIDWeap;
	}
	/* SCRAP_PLAYBOT_WEAPONS=1 all of them, else the names in it */
	const char *list = getenv("SCRAP_PLAYBOT_WEAPONS");
	for (int i = 0; i < 7; i++) {
		if (!strcmp(list, "1") || strstr(list, names[i])) {
			pick[n++] = w[i];
		}
	}
	return n ? pick[(int)(play.botNear / 6.0) % n] : g_VulcanIDWeap;
}

/* the bot of the tests: walks, takes a parked ship, flies, fires at the nearest other player, lands and gets out,
 * and so on */
static void Play_Bot(double now) {
	cEntity *e = play.e;
	double t;

	if (!play.botStart) {
		play.botStart = now;
	}
	t = now - play.botStart;
	memset(&play.acc, 0, sizeof(play.acc));
	play.acc.time = 1.0f;
	if (play.bot == PLAY_BOT_IDLE) {
		return;
	}
	if (play.bot == PLAY_BOT_FACE) {
		cEntity *o = play.kind == NETREP_CHAR ? Play_BotNearest(e, 0, 1, 0) : NULL;
		if (o) {
			float dyaw = remainderf(atan2f(-(o->pos.x - e->pos.x), -(o->pos.z - e->pos.z)) + PLAY_BOT_FACE_SIDE -
				WChar(e)->view.y, M_2PI_F);
			play.acc.rot[0] = dyaw > 0.5f ? 1.0f : dyaw < -0.5f ? -1.0f : dyaw * 2.0f;
		}
		return;
	}
	if (play.bot == PLAY_BOT_USE) {
		play.acc.buttons = play.kind == NETREP_CHAR && t > 10.0 && fmod(t, 3.0) < 0.2 ? NETREP_IN_ACT3 | NETREP_IN_SWITCH : 0;
		return;
	}
	if (play.kind == NETREP_CHAR) {
		/* a while around, then to the nearest ship that stands still and no player has, and into it */
		play.acc.move[0] = t < 6.0 ? 1.0f : 0.5f;
		play.acc.rot[0] = (float)sin(t * 0.7) * 0.6f;
		if (t < 8.0 || play.bot == PLAY_BOT_WALK) {
			return;
		}
		cEntity *ship = Play_BotNearest(e, NETREP_SHIP, 0, 1);
		/* no free ship near after a while: the switch calls one (the sandbox) */
		if ((!ship || t > 20.0) && fmod(t, 2.0) < 0.2) {
			play.acc.buttons = NETREP_IN_ACT3 | NETREP_IN_SWITCH;
			return;
		}
		if (ship) {
			float dx = ship->pos.x - e->pos.x, dz = ship->pos.z - e->pos.z;
			float dyaw = remainderf(atan2f(-dx, -dz) - WChar(e)->view.y, M_2PI_F);
			play.acc.rot[0] = dyaw > 0.5f ? 1.0f : dyaw < -0.5f ? -1.0f : dyaw * 2.0f;
			play.acc.move[0] = fabsf(dyaw) < 0.6f ? 1.0f : 0.2f;
			if (dx * dx + dz * dz < PLAY_BOARD_RADIUS * PLAY_BOARD_RADIUS * 0.5f && fmod(t, 2.0) < 0.2) {
				play.acc.buttons = NETREP_IN_ACT3 | NETREP_IN_SWITCH;
			}
		}
		return;
	}
	/* the target of the weapon tests climbs a little and stays there */
	if (play.bot == PLAY_BOT_TARGET) {
		play.acc.move[2] = t < 4.0 ? 0.6f : 0.0f;
		play.acc.move[0] = Veh(e)->speed > 300.0f ? -1.0f : 0.0f;
		return;
	}
	/* after a while in the ship: slow down and get out (not while it tries all weapons, nor in the PvP modes) */
	if (t > 45.0 && !play.botCycle && !Arena_ClientOn()) {
		play.acc.move[0] = Veh(e)->speed > 500.0f ? -1.0f : 0.0f;
		if (Veh(e)->speed < PLAY_LEAVE_SPEED * 0.8f && fmod(t, 2.0) < 0.2) {
			play.acc.buttons = NETREP_IN_SWITCH;
		}
		return;
	}
	/* in the ship: fly up and around, turn to the nearest other player (SCRAP_PLAYBOT=npc: ship of no player) and
	 * fire when it is ahead */
	cEntity *best = Play_BotNearest(e, play.bot == PLAY_BOT_NPC ? NETREP_SHIP : 0, play.bot != PLAY_BOT_NPC,
		play.bot == PLAY_BOT_NPC ? 2 : 0);
	float bestD = best ? (best->pos.x - e->pos.x) * (best->pos.x - e->pos.x) + (best->pos.y - e->pos.y) *
		(best->pos.y - e->pos.y) + (best->pos.z - e->pos.z) * (best->pos.z - e->pos.z) : 0.0f;
	play.acc.move[0] = 0.6f;
	play.acc.move[2] = t < 6.0 ? 0.5f : 0.0f;
	/* the PvP modes: to the flag, the mail bag or its zone (include/scrap/arena.h); fire at a player ahead */
	float goal[3];
	if (Arena_BotGoal(goal)) {
		float gx = goal[0] - e->pos.x, gy = goal[1] + 600.0f - e->pos.y, gz = goal[2] - e->pos.z;
		float gyaw = remainderf(atan2f(-gx, -gz) - Obj(e)->ang.y, M_2PI_F);
		play.acc.rot[0] = gyaw > 0.6f ? 1.0f : gyaw < -0.6f ? -1.0f : gyaw / 0.6f;
		play.acc.move[2] = gy > 500.0f ? 0.8f : gy < -500.0f ? -0.8f : gy / 625.0f;
		play.acc.move[0] = fabsf(gyaw) < 0.5f ? 1.0f : 0.3f;
		if (best) {
			float byaw = remainderf(atan2f(-(best->pos.x - e->pos.x), -(best->pos.z - e->pos.z)) - Obj(e)->ang.y, M_2PI_F);
			play.acc.buttons = fabsf(byaw) < 0.15f && bestD < 8000.0f * 8000.0f ? NETREP_IN_ACT : 0;
		}
		play.acc.weapon = (uint8_t)Play_BotWeapon();
		return;
	}
	if (!best) {
		play.acc.rot[0] = 0.4f;
		return;
	}
	/* yaw 0 looks to -z (forward = (-sin yaw, 0, -cos yaw)) */
	float dx = best->pos.x - e->pos.x, dy = best->pos.y - e->pos.y, dz = best->pos.z - e->pos.z;
	float want = atan2f(-dx, -dz);
	float dyaw = remainderf(want - Obj(e)->ang.y, M_2PI_F);
	float pitch = atan2f(dy, sqrtf(dx * dx + dz * dz));
	play.acc.rot[0] = dyaw > 0.6f ? 1.0f : dyaw < -0.6f ? -1.0f : dyaw / 0.6f;
	play.acc.rot[1] = pitch > 0.3f ? 0.6f : pitch < -0.3f ? -0.6f : pitch * 2.0f;
	play.acc.move[0] = bestD > 8000.0f * 8000.0f ? 0.8f : 0.1f;
	/* trying the weapons: stops about 4000 units away and fires in bursts of 1.5 s (the missiles fly at a release) */
	if (play.botCycle) {
		/* level with the target (the narrow aim cones of some weapons) */
		play.acc.move[2] = dy > 500.0f ? 0.8f : dy < -500.0f ? -0.8f : dy / 625.0f;
		if (bestD < 6000.0f * 6000.0f) {
			play.acc.move[0] = Veh(e)->speed > 200.0f ? -1.0f : 0.0f;
		}
	}
	if (fabsf(dyaw) < 0.15f && fmod(t, play.botCycle ? 2.0 : 3.0) < (play.botCycle ? 1.5 : 2.0)) {
		play.acc.buttons = NETREP_IN_ACT;
	}
	if (play.botCm && fabsf(dyaw) < 0.3f && fmod(t, 5.0) >= 3.5) {
		play.acc.buttons = NETREP_IN_ACT2;
	}
	if (fabsf(dyaw) < 0.3f && bestD < 8000.0f * 8000.0f) {
		play.botNear += 1.0 / NETREP_TICK_HZ;
	}
	play.acc.weapon = (uint8_t)Play_BotWeapon();
}

void Play_PreFrame(double now, double rtt, double rttVar, float loss) {
	if (play.dead && !play.e) {
		Play_DeathCam((float)(now - play.tickLocal));
		play.tickLocal = now;
	}
	if (!play.e || !Watch_ClockReady()) {
		return;
	}
	uint32_t target = Watch_InputTick(now, rtt, PLAY_LEAD + rttVar + (loss > 0.01f ? PLAY_LOSS_LEAD : 0.0));
	if (!play.tick || target > play.tick + PLAY_MAX_CATCHUP * 4 || target + PLAY_MAX_CATCHUP * 4 < play.tick) {
		play.tick = target ? target - 1 : 0;
	}
	for (int n = 0; play.tick < target && (n < PLAY_MAX_CATCHUP || play.catchup); n++) {
		if (play.bot) {
			Play_Bot(now);
		}
		Play_Step(play.tick + 1, play.catchup && play.tick + 1 < target);
		play.tickLocal = now;
	}
	play.catchup = 0;
	play.shownMs = (uint32_t)(Watch_ShownTime() * 1000.0);
}

void Play_PostFrame(float dt) {
	if (play.e && !play.bot) {
		NetPl_Accumulate(play.e, &play.acc, dt);
	}
	Faction_ClientFrame();
	Coop_ClientFrame();
	Arena_ClientFrame();
}

int Play_TakeInputs(NetRepInput *in, int *n, uint32_t *shownMs) {
	if (!play.e || !play.pending || !play.tick) {
		return 0;
	}
	*n = 0;
	for (uint32_t t = play.tick; *n < NETREP_INPUT_REDUNDANCY && t > 0; t--) {
		const NetRepInput *x = NetRepPredHist_Input(&play.hist, t);
		if (!x) {
			break;
		}
		in[(*n)++] = *x;
	}
	*shownMs = play.shownMs;
	play.pending = 0;
	return *n > 0;
}

/* the weapon keys and the times of the next shots of a ship, kept across a replay: the replay fires nothing and
 * gives no presses or releases (WeaponManager.c sees no change of the keys) */
typedef struct PlayWeapKeep {
	byte fire, firePrev, cm, cmPrev;
	float fireStamp, cmStamp;
} PlayWeapKeep;

static void Play_KeepWeapons(cEntity *e, PlayWeapKeep *k, byte restore) {
	if (NetPl_Kind(e) != NETREP_SHIP) {
		return;
	}
	cVehicleControls *c = &Veh(e)->controls;
	cWeaponManager *w = &Veh(e)->weapons;
	if (restore) {
		c->fire = k->fire;
		c->firePrev = k->firePrev;
		c->cm = k->cm;
		c->cmPrev = k->cmPrev;
		w->fireStamp = k->fireStamp;
		w->cmStamp = k->cmStamp;
		return;
	}
	k->fire = c->fire;
	k->firePrev = c->firePrev;
	k->cm = c->cm;
	k->cmPrev = c->cmPrev;
	k->fireStamp = w->fireStamp;
	k->cmStamp = w->cmStamp;
	/* no key down and none before during the replay */
	c->firePrev = c->cmPrev = 0;
}

/* a server state of the own entity: the error of the prediction of its tick, and the correction */
void Play_OwnUpdate(uint32_t tick, const NetRepState *st) {
	cEntity *e = play.e;
	float sp[3];

	if (!e) {
		return;
	}
	for (int k = 0; k < 3; k++) {
		sp[k] = NetRep_UPos(st->pos[k]);
	}
	play.lastServer = (vec3_t){ sp[0], sp[1], sp[2] };
	WLife(e)->life = st->life;
	Faction_EntityFlags(e, st->flags);
	float err = NetRepPredHist_Error(&play.hist, tick, sp);
	play.st.states++;
	if (err < 0.0f) {
		play.st.unknown++;
		return;
	}
	play.st.err[play.st.n++ % PLAY_ERRS] = err;
	float corr = 0.0f;
	uint32_t replayed = 0;
	if (err > PLAY_CORRECT_MIN) {
		vec3_t before = play.truePos;
		/* the state of that tick with the server's position, velocity and angles, then the inputs after it */
		PlayWeapKeep keep = { 0 };
		Play_KeepWeapons(e, &keep, 0);
		NetPl_Restore(e, &play.motion[tick % NETREP_PRED]);
		Play_SetState(e, st);
		NetPl_Save(e, &play.motion[tick % NETREP_PRED]);
		for (uint32_t t = tick + 1; t <= play.tick; t++) {
			const NetRepInput *in = NetRepPredHist_Input(&play.hist, t);
			if (!in) {
				break;
			}
			NetRepInput x = *in;
			if (play.ahead != PLAY_AHEAD_NONE) {
				Watch_ProxiesAt((double)t / NETREP_TICK_HZ, play.ahead == PLAY_AHEAD_PLAYERS);
			}
			NetPl_Step(e, &x, 1, 1);
			Watch_ProxiesBack();
			play.input = x;
			Play_Record(t);
			replayed++;
		}
		Play_KeepWeapons(e, &keep, 1);
		play.input.tick = play.tick;
		play.truePos = e->pos;
		NetRepSmooth_Correct(&play.smooth, &before.x, &play.truePos.x);
		float dx = before.x - play.truePos.x, dy = before.y - play.truePos.y, dz = before.z - play.truePos.z;
		corr = sqrtf(dx * dx + dy * dy + dz * dz);
		play.st.corr[play.st.nc++ % PLAY_ERRS] = corr;
		play.st.corrections++;
		play.st.replayed += replayed;
		e->pos = (vec3_t){ play.truePos.x + play.smooth.off[0], play.truePos.y + play.smooth.off[1],
			play.truePos.z + play.smooth.off[2] };
	}
	if (play.log) {
		const NetRepPred *p = NetRepPredHist_Get(&play.hist, tick);
		fprintf(play.log, "%u,%d,%.2f,%.2f,%u,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n", tick, play.kind, (double)err,
			(double)corr, replayed, (double)sp[0], (double)sp[1], (double)sp[2], p ? (double)p->pos[0] : 0.0,
			p ? (double)p->pos[1] : 0.0, p ? (double)p->pos[2] : 0.0);
	}
}

static int Play_CmpF(const void *a, const void *b) {
	float x = *(const float *)a, y = *(const float *)b;
	return x < y ? -1 : x > y;
}

/* the percentiles 50, 95, 99 and the maximum of the first n of v */
static void Play_Pct(const float *v, uint32_t n, float *out) {
	static float tmp[PLAY_ERRS];
	n = n > PLAY_ERRS ? PLAY_ERRS : n;
	memcpy(tmp, v, n * sizeof(float));
	qsort(tmp, n, sizeof(float), Play_CmpF);
	out[0] = n ? tmp[n / 2] : 0.0f;
	out[1] = n ? tmp[n * 95 / 100] : 0.0f;
	out[2] = n ? tmp[n * 99 / 100] : 0.0f;
	out[3] = n ? tmp[n - 1] : 0.0f;
}

void Play_Status(void) {
	float e[4], c[4];
	if (play.slot < 0) {
		return;
	}
	Play_Pct(play.st.err, play.st.n, e);
	Play_Pct(play.st.corr, play.st.nc, c);
	Con_Printf("play: slot %d kind %d tick %u, %u states (%u without a prediction), error p50 %.2f p95 %.2f p99 %.2f "
		"max %.1f, %u corrections p50 %.2f p95 %.2f p99 %.2f max %.1f, %u ticks run again\n", play.slot, play.kind,
		play.tick, play.st.states, play.st.unknown, (double)e[0], (double)e[1], (double)e[2], (double)e[3],
		play.st.corrections, (double)c[0], (double)c[1], (double)c[2], (double)c[3], play.st.replayed);
}

void Play_Reset(void) {
	Play_OwnRemove();
	play.slot = -1;
	play.player = -1;
	play.started = 0;
	play.haveStatus = 0;
	play.pending = 0;
	play.cam = NULL;
}

void Play_Close(void) {
	Play_Status();
	if (play.log) {
		fclose(play.log);
		play.log = NULL;
	}
}
