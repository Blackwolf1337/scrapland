/*
 * The players of the world: the owner slots of the local players and the instance's players of the network, see
 * include/scrap/players.h. Not in the original.
 */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/alarm.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/faction.h"
#include "scrap/game.h"
#include "scrap/instance.h"
#include "scrap/mathlib.h"
#include "scrap/modvars.h"
#include "scrap/netaudit.h"
#include "scrap/netclass.h"
#include "scrap/places.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmodel.h"
#include "scrap/scriptmodes.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

/* the module of the player's own ship variable (Parking.PlayerShip, an overlay per player: include/scrap/modvars.h) */
#define PLAYERS_VAR_MODULE "Parking"

static cPlayer g_Pl[PLAYERS_MAX + 1];	/* + the world's own (PLAYERS_NONE) */
static cPlayer *g_Cur = &g_Pl[PLAYERS_NONE];
static cEntity *g_Runner;		/* the entity the world's context runs for (Players_Run), NULL */
static byte Players_Tracing(void);
static void Players_TraceLook(void);
static int g_Turn;				/* frames: the rule ANY takes the next player each frame */

#define g_Bound ((int)(g_Cur - g_Pl))
#define g_NetPlayer(i) (g_Pl[i].ent)

byte Players_Net(void) {
	return g_Instance;
}

cEntity *Players_Get(int i) {
	if (Players_Net()) {
		return i >= 0 && i < PLAYERS_MAX ? g_NetPlayer(i) : NULL;
	}
	return i >= 0 && i < 4 ? cWorld_GetUsrEntity(g_pWorld, i) : NULL;
}

static int Players_Slot(cEntity *e) {
	if (Entity_IsType(e, TYEN_CAR)) {
		return cVehicleEntity_GetOwnerSlot(e);
	}
	return Entity_IsType(e, TYEN_WALKCHAR) ? cWalkCharEntity_GetOwnerSlot(e) : -1;
}

int Players_Of(cEntity *e) {
	if (!e) {
		return -1;
	}
	int slot = Players_Slot(e);
	int i = Players_Net() ? slot - PLAYERS_NET_SLOT : slot;
	return Players_Get(i) == e ? i : -1;
}

byte Players_Is(cEntity *e) {
	if (!Players_Net()) {
		return e == cWorld_GetUsrEntity(g_pWorld, 0);
	}
	return Players_Of(e) >= 0;
}

/* the network audit of a read of "the player" in a player's context on an instance */
static void Players_AuditRead(const char *bound, const char *site) {
	if (Players_Net()) {
		NetAudit_Func(NA_PLAYER, bound, site);
	}
}

static int Players_ResolveAt(const char *site, byte copy);

/* the entity of a used player of the instance with one, else the first one, NULL */
static cEntity *Players_EntOf(int i) {
	if (i >= 0 && i < PLAYERS_MAX && g_Pl[i].used && g_NetPlayer(i)) {
		return g_NetPlayer(i);
	}
	for (i = 0; i < PLAYERS_MAX; i++) {
		if (g_NetPlayer(i)) {
			return g_NetPlayer(i);
		}
	}
	return NULL;
}

/* "the player": outside the instance player 0; the current player's entity; in the world's context the player of the
 * site's rule (Players_ResolveAt; the rule WORLD: none) */
static cEntity *Players_AnyOf(const char *site, const char *bound) {
	if (!Players_Net()) {
		return cWorld_GetUsrEntity(g_pWorld, 0);
	}
	if (g_Cur != &g_Pl[PLAYERS_NONE] && g_Cur->ent) {
		Players_AuditRead(bound, site);
		return g_Cur->ent;
	}
	int i = Players_ResolveAt(site, 0);
	return i == PLAYERS_NONE ? NULL : Players_EntOf(i);
}

cEntity *Players_AnyAt(const char *site) {
	return Players_AnyOf(site, "player.any.bound:");
}

static float Players_DistSq(cEntity *e, const vec3_t *pos) {
	float dx = e->pos.x - pos->x, dy = e->pos.y - pos->y, dz = e->pos.z - pos->z;
	return dx * dx + dy * dy + dz * dz;
}

/* the nearest player (only those whose alarm is on: wanted), NULL; its distance squared into *dist */
static cEntity *Players_NearestOf(const vec3_t *pos, byte wanted, float *dist) {
	cEntity *best = NULL;
	float bestD = FLT_MAX;

	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = g_NetPlayer(i);
		if (e && (!wanted || Players_Alarm(i)->on)) {
			float d = Players_DistSq(e, pos);
			if (d < bestD) {
				bestD = d;
				best = e;
			}
		}
	}
	*dist = bestD;
	return best;
}

cEntity *Players_Closest(const vec3_t *pos) {
	float d;
	return Players_Net() ? Players_NearestOf(pos, 0, &d) : cWorld_GetUsrEntity(g_pWorld, 0);
}

cEntity *Players_Nearest(const vec3_t *pos) {
	float d;

	if (!Players_Net()) {
		return cWorld_GetUsrEntity(g_pWorld, 0);
	}
	cEntity *wanted = Players_NearestOf(pos, 1, &d);
	return wanted && d < PLAYERS_WANTED_REACH * PLAYERS_WANTED_REACH ? wanted : Players_NearestOf(pos, 0, &d);
}

void Players_Forward(cEntity *e, vec3_t *dir) {
	float yaw = Entity_IsType(e, TYEN_WALKCHAR) ? WChar(e)->view.y : Obj(e)->ang.y;
	*dir = (vec3_t){ -sinf(yaw), 0.0f, -cosf(yaw) };
}

float Players_MinDistSq(const vec3_t *pos) {
	float best = FLT_MAX;

	if (Players_Net()) {
		for (int i = 0; i < PLAYERS_MAX; i++) {
			if (g_NetPlayer(i)) {
				float d = Players_DistSq(g_NetPlayer(i), pos);
				best = d < best ? d : best;
			}
		}
	}
	return best;
}

cEntity *Players_ScriptAt(int i, const char *site) {
	if (!Players_Net()) {
		return cWorld_GetUsrEntity(g_pWorld, i);
	}
	/* a coop instance's stand-in for the level's start without a member (include/scrap/coop.h) */
	cEntity *e = i == 0 ? Players_AnyOf(site, "player.read.bound:") : NULL;
	return i == 0 && !e ? Coop_StandInEntity() : e;
}

cEntity *Players_Local(void) {
	return cWorld_GetUsrEntity(g_pWorld, 0);
}

void Players_Clear(void) {
	for (int i = 0; i < 4; i++) {
		g_pWorld->owners[i] = NULL;
	}
	if (Players_Net()) {
		for (int i = 0; i < PLAYERS_MAX; i++) {
			g_NetPlayer(i) = NULL;
		}
		g_Turn++;
		if (Players_Tracing()) {
			Players_TraceLook();
		}
	}
}

void Players_SetOwner(int slot, cEntity *e) {
	if (slot >= 0 && slot < 4) {
		g_pWorld->owners[slot] = e;
	} else if (Players_Net() && slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		cPlayer *p = &g_Pl[slot - PLAYERS_NET_SLOT];
		p->ent = e;
	}
}

/* ---- the game state per player ---- */

/* player i is a used player of the instance */
static byte Players_Used(int i) {
	return Players_Net() && i >= 0 && i < PLAYERS_MAX && g_Pl[i].used;
}

void Players_Join(int i, int money) {
	if (!Players_Net() || i < 0 || i >= PLAYERS_MAX) {
		return;
	}
	if (g_Bound == i) {
		Players_Enter(PLAYERS_NONE);
	}
	cPlayer *s = &g_Pl[i];
	/* the alarm of the level (its guaranteed characters, its lock) without the world's level and status */
	s->alarm = g_pWorld->alarm;
	s->alarm.value = 0.0f;
	s->alarm.on = 0;
	s->alarm.grow = -0.1f;
	s->alarm.sameChar = 0;
	s->alarm.tauntReady = 1;
	s->money = money;
	s->joined++;
	s->ship = NULL;
	s->shipName[0] = 0;
	for (int k = 0; k < PLAYERS_LEVELS; k++) {
		s->level[k] = -1;
	}
	s->input = NULL;
	s->camName[0] = 0;
	s->actionSet = -1;
	s->timeSpeed = 0.0f;
	s->used = 1;
	ModVar_OverClearPlayer(i);
}

void Players_Part(int i) {
	if (!Players_Net() || i < 0 || i >= PLAYERS_MAX) {
		return;
	}
	if (g_Bound == i) {
		Players_Enter(PLAYERS_NONE);
	}
	g_Pl[i].used = 0;
	ModVar_OverClearPlayer(i);
}

/* ---- the trace of the tests (SCRAP_PLAYER_TRACE=1 on an instance): each change of a player's money and alarm with
 * the context it happened in, each dispatch of a callback; "pltrace WRONG" when a player's context changed another
 * player's ---- */

static signed char g_Trace = -1;
static const char *g_TraceSite = "-";	/* where the current context was entered */
static int g_TraceMoney[PLAYERS_MAX];
static float g_TraceValue[PLAYERS_MAX];
static byte g_TraceOn[PLAYERS_MAX];
static byte g_TraceSeen[PLAYERS_MAX];

static byte Players_Tracing(void) {
	if (g_Trace < 0) {
		const char *v = getenv("SCRAP_PLAYER_TRACE");
		g_Trace = v && v[0] == '1';
	}
	return g_Trace && Players_Net();
}

/* the changes since the last look, made in the current context */
static void Players_TraceLook(void) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cPlayer *p = &g_Pl[i];
		if (!p->used) {
			g_TraceSeen[i] = 0;
			continue;
		}
		byte changed = g_TraceSeen[i] && (p->money != g_TraceMoney[i] || p->alarm.on != g_TraceOn[i] ||
			p->alarm.value != g_TraceValue[i]);
		if (changed) {
			char ctx[32];
			snprintf(ctx, sizeof(ctx), g_Cur == &g_Pl[PLAYERS_NONE] ? "the world" : "player %d", g_Bound);
			Con_Printf("pltrace player %d money %d -> %d alarm %.3f %d -> %.3f %d in %s (%s)\n", i, g_TraceMoney[i],
				p->money, (double)g_TraceValue[i], g_TraceOn[i], (double)p->alarm.value, p->alarm.on, ctx, g_TraceSite);
			if (g_Cur != &g_Pl[PLAYERS_NONE] && g_Cur != p) {
				Con_Printf("pltrace WRONG player %d changed in the context of player %d (%s)\n", i, g_Bound, g_TraceSite);
			}
		}
		g_TraceSeen[i] = 1;
		g_TraceMoney[i] = p->money;
		g_TraceValue[i] = p->alarm.value;
		g_TraceOn[i] = p->alarm.on;
	}
}

/* the frame loops, the steps of a player's own entity and the alarm's update (Alarm_SetOn) enter for every frame: no
 * callback lines for them (their changes have lines) */
static byte Players_TraceQuiet(const char *site) {
	return !strcmp(site, "cWorld_Update") || !strcmp(site, "Rep_PreFrame") || !strcmp(site, "Players_UpdateAlarms") ||
		!strcmp(site, "Players_SetGrow") || !strcmp(site, "AICtrl_Think") || !strcmp(site, "Alarm_SetOn");
}

static int Players_Switch(int i, const char *site) {
	int prev = g_Bound;
	if (!Players_Net()) {
		return prev;
	}
	cPlayer *n = &g_Pl[Players_Used(i) ? i : PLAYERS_NONE];
	if (Players_Tracing()) {
		Players_TraceLook();
		if (site && n != &g_Pl[PLAYERS_NONE] && !Players_TraceQuiet(site)) {
			Con_Printf("pltrace callback %s for player %d\n", site, (int)(n - g_Pl));
		}
		g_TraceSite = site ? site : "back";
	}
	g_Cur = n;
	return prev;
}

int Players_EnterAt(int i, const char *site) {
	return Players_Switch(i, site);
}

void Players_Leave(int prev) {
	Players_Switch(prev, NULL);
}

int Players_EnterEntityAt(cEntity *e, const char *site) {
	int i = Players_Of(e);
	return Players_Switch(i >= 0 ? i : Players_ShipOwner(e), site);
}

int Players_EnterOwnerAt(cEntity *e, const char *site) {
	int i = e ? Players_Slot(e) - PLAYERS_NET_SLOT : -1;
	return Players_Switch(i >= 0 && i < PLAYERS_MAX ? i : Players_ShipOwner(e), site);
}

int Players_EnterJoinedAt(int i, uint16_t joined, const char *site) {
	return Players_Switch(Players_Used(i) && g_Pl[i].joined == joined ? i : PLAYERS_NONE, site);
}

cEntity *Players_Run(cEntity *e) {
	cEntity *prev = g_Runner;
	g_Runner = e;
	return prev;
}

int Players_Current(void) {
	return g_Bound;
}

int Players_ActingAt(const char *site) {
	if (!Players_Net() || g_Cur != &g_Pl[PLAYERS_NONE]) {
		return g_Bound;
	}
	return Players_ResolveAt(site, 0);
}

/* ---- the resolver of "the player" in the world's context ---- */

/* the rules of the C sites, found once by the address of the site's name (__func__) */
#define PLAYERS_RULES 1024
static struct {
	const char *site;
	int8_t tag;
} g_Rules[PLAYERS_RULES];

static netTag_t Players_RuleOf(const char *site, byte copy) {
	if (copy) {
		return NetClass_Find(NCT_PLAYERREAD, site);
	}
	unsigned h = (unsigned)(((uintptr_t)site >> 3) * 2654435761u) % PLAYERS_RULES;
	for (int k = 0; k < PLAYERS_RULES; k++, h = (h + 1) % PLAYERS_RULES) {
		if (g_Rules[h].site == site) {
			return (netTag_t)g_Rules[h].tag;
		}
		if (!g_Rules[h].site) {
			g_Rules[h].site = site;
			g_Rules[h].tag = (int8_t)NetClass_Find(NCT_PLAYERREAD, site);
			return (netTag_t)g_Rules[h].tag;
		}
	}
	return NetClass_Find(NCT_PLAYERREAD, site);
}

/* some used player with an entity, the next one each frame (code that polls "the player" every frame sees all of them
 * in turn), PLAYERS_NONE */
static int Players_First(void) {
	for (int k = 0; k < PLAYERS_MAX; k++) {
		int i = (g_Turn + k) % PLAYERS_MAX;
		if (g_Pl[i].used && g_NetPlayer(i)) {
			return i;
		}
	}
	return PLAYERS_NONE;
}

/* the player nearest to the entity the world's context runs for (Players_Nearest: a wanted one first), without one as
 * the rule ANY */
static int Players_NearRunner(void) {
	int i = g_Runner ? Players_Of(Players_Nearest(&g_Runner->pos)) : -1;
	return i >= 0 ? i : Players_First();
}

/* the player a read of "the player" at site (a C function, or Module.variable with copy) gets in the world's context
 * of the instance, by the site's row of tools/script/netclass/playerread.tsv: NEAREST (to the entity that runs), ANY
 * (some player, the next one each frame), PARTY (the coop party's acting member: its mission's, else the nearest; outside coop NEAREST),
 * WORLD (no player: the world's own money, alarm and module variables). A site without a row gets PARTY in coop and
 * NEAREST elsewhere and is counted as player.world.norule (the gate of stage 1.3: none on the city levels). */
static int Players_ResolveAt(const char *site, byte copy) {
	netTag_t tag = Players_RuleOf(site, copy);
	const char *key;
	int i;

	if (tag == NC_WORLD) {
		key = "player.world.world:";
		i = PLAYERS_NONE;
	} else if (tag == NC_ANY) {
		key = "player.world.any:";
		i = Players_First();
	} else {
		key = tag == NC_NEAREST ? "player.world.nearest:" : tag == NC_PARTY ? "player.world.party:" :
			"player.world.norule:";
		i = (tag == NC_PARTY || tag != NC_NEAREST) && Coop_On() ? Coop_MissionPlayer() : -1;
		if (i < 0 || !g_Pl[i].used) {
			i = Players_NearRunner();
		}
	}
	if (copy) {
		NetAudit_Name(NA_PLAYER, key, site);
	} else {
		NetAudit_Func(NA_PLAYER, key, site);
	}
	return i;
}

cPlayer *Players_Cur(void) {
	return g_Cur;
}

cPlayer *Players_Ctx(int i) {
	if (i == PLAYERS_NONE) {
		return &g_Pl[PLAYERS_NONE];
	}
	return Players_Used(i) ? &g_Pl[i] : NULL;
}

int Players_VarOwnerAt(const char *module, const char *name) {
	if (!Players_Net()) {
		return -1;
	}
	if (g_Cur != &g_Pl[PLAYERS_NONE]) {
		return g_Bound;
	}
	char key[128];
	snprintf(key, sizeof(key), "%s.%s", module, name);
	int i = Players_ResolveAt(key, 1);
	return i == PLAYERS_NONE ? -1 : i;
}

/* the context money and alarm of a read at site: the current one, in the world's context of the instance the player
 * of the site's rule */
static cPlayer *Players_CtxAt(const char *site) {
	if (g_Cur != &g_Pl[PLAYERS_NONE] || !Players_Net()) {
		return g_Cur;
	}
	return &g_Pl[Players_ResolveAt(site, 0)];
}

int *Players_CurMoneyAt(const char *site) {
	cPlayer *p = Players_CtxAt(site);
	return p == &g_Pl[PLAYERS_NONE] ? &g_pWorld->money : &p->money;
}

cAlarm *Players_CurAlarmAt(const char *site) {
	cPlayer *p = Players_CtxAt(site);
	return p == &g_Pl[PLAYERS_NONE] ? &g_pWorld->alarm : &p->alarm;
}

int Players_OfAlarm(const cAlarm *a) {
	for (int i = 0; Players_Net() && i < PLAYERS_MAX; i++) {
		if (a == &g_Pl[i].alarm) {
			return i;
		}
	}
	return PLAYERS_NONE;
}

void Players_NoteCam(const char *name) {
	if (Players_Net() && g_Cur != &g_Pl[PLAYERS_NONE]) {
		Str_Copy(g_Cur->camName, name, sizeof(g_Cur->camName));
	}
}

void Players_NoteActionSet(int set) {
	if (Players_Net() && g_Cur != &g_Pl[PLAYERS_NONE]) {
		g_Cur->actionSet = set;
	}
}

void Players_SetShip(int i, cEntity *ship) {
	if (!Players_Used(i)) {
		return;
	}
	cPlayer *s = &g_Pl[i];
	s->ship = ship;
	Str_Copy(s->shipName, ship ? ship->name : "", sizeof(s->shipName));
	int was = Players_Enter(i);
	if (ship) {
		Mode_PutHandle(PLAYERS_VAR_MODULE, "PlayerShip", ship);
	} else {
		Mode_ModNone(PLAYERS_VAR_MODULE, "PlayerShip");
	}
	Native_ErrClear();
	Players_Leave(was);
}

cEntity *Players_Ship(int i) {
	if (!Players_Used(i) || !g_Pl[i].ship) {
		return NULL;
	}
	cPlayer *s = &g_Pl[i];
	cEntity *e = World_FindEntity(s->shipName);
	return e == s->ship && !e->deadNode.target ? e : NULL;
}

int Players_ShipOwner(cEntity *e) {
	for (int i = 0; e && Players_Net() && i < PLAYERS_MAX; i++) {
		if (g_Pl[i].ship == e && Players_Ship(i) == e) {
			return i;
		}
	}
	return -1;
}

uint16_t Players_Joined(int i) {
	return Players_Net() && i >= 0 && i < PLAYERS_MAX ? g_Pl[i].joined : 0;
}

int Players_Money(int i) {
	return Players_Used(i) ? g_Pl[i].money : *Players_CurMoney();
}

void Players_SetMoney(int i, int money) {
	if (Players_Used(i)) {
		g_Pl[i].money = money;
	} else {
		*Players_CurMoney() = money;
	}
}

const cAlarm *Players_Alarm(int i) {
	return Players_Used(i) ? &g_Pl[i].alarm : Players_CurAlarm();
}

byte Players_MayHurt(cEntity *attacker, cEntity *victim) {
	if (!Players_Net() || attacker == victim || Players_Of(attacker) < 0 || Players_Of(victim) < 0) {
		return 1;
	}
	/* the members of the coop story's party never hurt each other (the single player's one player); else the
	 * freeroam factions first (sanctuaries, relations, the law), else the server's -pvp */
	if (Coop_On()) {
		return 0;
	}
	int rule = Faction_PvP(attacker, victim);
	return rule > 0 || (rule < 0 && Instance_PvP());
}

cEntity *Players_NearestWanted(const vec3_t *pos) {
	float d;
	return Players_NearestOf(pos, 1, &d);
}

byte Players_SomeWanted(void) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (g_NetPlayer(i) && Players_Alarm(i)->on) {
			return 1;
		}
	}
	return 0;
}

void Players_LevelReset(int source) {
	if (Players_Net()) {
		for (int i = 0; i <= PLAYERS_MAX; i++) {
			g_Pl[i].level[source] = -1;
		}
	}
}

void Players_LevelRaiseAt(int source, int level, const char *site) {
	cPlayer *p = Players_Net() ? Players_CtxAt(site) : NULL;
	if (p && p->level[source] < level) {
		p->level[source] = (int8_t)level;
	}
}

void Players_UpdateAlarms(float dt) {
	/* a player's client shows the alarm of the server's status message (Play_OwnStatus): it does not move here */
	if (g_Play) {
		return;
	}
	if (!Players_Net()) {
		Alarm_Update(&g_pWorld->alarm, dt);
		return;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (g_Pl[i].used && g_NetPlayer(i)) {
			int prev = Players_Enter(i);
			Alarm_Update(&g_Pl[i].alarm, dt);
			Players_Leave(prev);
		}
	}
}

void Players_SetTimeSpeed(float speed) {
	if (!Players_Net()) {
		Timer_SetSpeed(&g_pWorld->timer, speed);
		return;
	}
	cEntity *e = g_Cur != &g_Pl[PLAYERS_NONE] ? g_Cur->ent : NULL;
	if (!e || speed <= 0.01f || !Entity_IsType(e, TYEN_OBJECT)) {
		return;
	}
	cTimer *t = &Obj(e)->timer;
	float was = g_Cur->timeSpeed > 0.01f ? g_Cur->timeSpeed : 1.0f;
	Timer_SetSpeed(t, cTimer_GetSpeed(t) * was / speed);
	g_Cur->timeSpeed = speed;
}

void Players_SetGrow(int level) {
	if (g_Play) {
		return;
	}
	if (!Players_Net()) {
		Alarm_SetGrow(&g_pWorld->alarm, level);
		return;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (!g_Pl[i].used) {
			continue;
		}
		int l = -1;
		for (int k = 0; k < PLAYERS_LEVELS; k++) {
			l = g_Pl[i].level[k] > l ? g_Pl[i].level[k] : l;
		}
		int prev = Players_Enter(i);
		Alarm_SetGrow(&g_Pl[i].alarm, l);
		Players_Leave(prev);
	}
}
