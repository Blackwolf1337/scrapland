/*
 * The players of the world: the owner slots of the local players and the instance's players of the network, see
 * include/scrap/players.h. Not in the original.
 */
#include <float.h>

#include "scrap/alarm.h"
#include "scrap/faction.h"
#include "scrap/game.h"
#include "scrap/instance.h"
#include "scrap/mathlib.h"
#include "scrap/modvars.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/scrap.h"
#include "scrap/scriptmodes.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

static cEntity *g_NetPlayers[PLAYERS_MAX];	/* the instance's players, found every frame like cWorld.owners */

/* the variables of the scripts' module PLAYERS_VAR_MODULE that mean "the player's" (the player's own ship) */
#define PLAYERS_VAR_MODULE "Parking"
static const char *const g_PlVars[] = { "PlayerShip", "LocalShip" };
#define PLAYERS_VARS ((int)(sizeof(g_PlVars) / sizeof(g_PlVars[0])))

/* the game state of a player while it is not bound into the world */
typedef struct PlayerState {
	byte used;
	uint16_t joined;				/* joins of the slot so far */
	int money;
	cAlarm alarm;
	int8_t level[PLAYERS_LEVELS];
	nativeVal_t vars[PLAYERS_VARS];	/* of g_PlVars */
	cEntity *ship;					/* its own ship (Players_SetShip): compared, read while World_FindEntity(shipName)
									   is it */
	char shipName[64];
} PlayerState;

static PlayerState g_PlState[PLAYERS_MAX + 1];	/* + the world's own (PLAYERS_NONE) */
static int g_Bound = PLAYERS_NONE;
static int g_Turn;
static modVarRef_t g_VarRef;		/* the module of g_PlVars, found once a frame (the turn) */
static int g_VarRefTurn = -1;
static byte g_VarRefOk;

byte Players_Net(void) {
	return g_Instance;
}

cEntity *Players_Get(int i) {
	if (Players_Net()) {
		return i >= 0 && i < PLAYERS_MAX ? g_NetPlayers[i] : NULL;
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

cEntity *Players_Any(void) {
	if (!Players_Net()) {
		return cWorld_GetUsrEntity(g_pWorld, 0);
	}
	if (g_Bound != PLAYERS_NONE && g_NetPlayers[g_Bound]) {
		return g_NetPlayers[g_Bound];
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (g_NetPlayers[i]) {
			return g_NetPlayers[i];
		}
	}
	return NULL;
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
		cEntity *e = g_NetPlayers[i];
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

float Players_MinDistSq(const vec3_t *pos) {
	float best = FLT_MAX;

	if (Players_Net()) {
		for (int i = 0; i < PLAYERS_MAX; i++) {
			if (g_NetPlayers[i]) {
				float d = Players_DistSq(g_NetPlayers[i], pos);
				best = d < best ? d : best;
			}
		}
	}
	return best;
}

cEntity *Players_Script(int i) {
	if (!Players_Net()) {
		return cWorld_GetUsrEntity(g_pWorld, i);
	}
	return i == 0 ? Players_Any() : NULL;
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
			g_NetPlayers[i] = NULL;
		}
		g_Turn++;
	}
}

void Players_SetOwner(int slot, cEntity *e) {
	if (slot >= 0 && slot < 4) {
		g_pWorld->owners[slot] = e;
	} else if (Players_Net() && slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		g_NetPlayers[slot - PLAYERS_NET_SLOT] = e;
	}
}

/* ---- the game state per player ---- */

void Players_Join(int i, int money) {
	if (!Players_Net() || i < 0 || i >= PLAYERS_MAX) {
		return;
	}
	if (g_Bound == i) {
		Players_Bind(PLAYERS_NONE);
	}
	PlayerState *s = &g_PlState[i];
	/* the alarm of the level (its guaranteed characters, its lock) without the world's level and status */
	s->alarm = g_Bound == PLAYERS_NONE ? g_pWorld->alarm : g_PlState[PLAYERS_NONE].alarm;
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
	s->used = 1;
}

void Players_Part(int i) {
	if (!Players_Net() || i < 0 || i >= PLAYERS_MAX) {
		return;
	}
	if (g_Bound == i) {
		Players_Bind(PLAYERS_NONE);
	}
	g_PlState[i].used = 0;
	for (int k = 0; k < PLAYERS_VARS; k++) {
		Native_Free(&g_PlState[i].vars[k]);
	}
}

/* the variables of g_PlVars of player o into its state, those of player n into the module */
static void Players_SwapVars(PlayerState *o, PlayerState *n) {
	if (g_VarRefTurn != g_Turn) {
		g_VarRefTurn = g_Turn;
		g_VarRefOk = ModVar_Named(PLAYERS_VAR_MODULE, &g_VarRef);
	}
	if (!g_VarRefOk) {
		return;
	}
	for (int k = 0; k < PLAYERS_VARS; k++) {
		nativeVal_t cur;
		if (!ModVar_GetVal(&g_VarRef, g_PlVars[k], &cur)) {
			continue;
		}
		if (!Native_Equal(&cur, &n->vars[k])) {
			ModVar_SetVal(&g_VarRef, g_PlVars[k], &n->vars[k]);
		}
		Native_Free(&o->vars[k]);
		o->vars[k] = cur;
	}
}

void Players_Bind(int i) {
	if (!Players_Net()) {
		return;
	}
	if (i < 0 || i >= PLAYERS_MAX || !g_PlState[i].used) {
		i = PLAYERS_NONE;
	}
	if (i == g_Bound) {
		return;
	}
	PlayerState *o = &g_PlState[g_Bound], *n = &g_PlState[i];
	o->money = g_pWorld->money;
	o->alarm = g_pWorld->alarm;
	g_pWorld->money = n->money;
	g_pWorld->alarm = n->alarm;
	Players_SwapVars(o, n);
	g_Bound = i;
}

void Players_SetShip(int i, cEntity *ship) {
	if (!Players_Net() || i < 0 || i >= PLAYERS_MAX || !g_PlState[i].used) {
		return;
	}
	int was = g_Bound;
	PlayerState *s = &g_PlState[i];
	s->ship = ship;
	Str_Copy(s->shipName, ship ? ship->name : "", sizeof(s->shipName));
	Players_Bind(i);
	if (ship) {
		Mode_PutHandle(PLAYERS_VAR_MODULE, "PlayerShip", ship);
	} else {
		Mode_ModNone(PLAYERS_VAR_MODULE, "PlayerShip");
	}
	Native_ErrClear();
	Players_Bind(was);
}

cEntity *Players_Ship(int i) {
	if (!Players_Net() || i < 0 || i >= PLAYERS_MAX || !g_PlState[i].used || !g_PlState[i].ship) {
		return NULL;
	}
	PlayerState *s = &g_PlState[i];
	cEntity *e = World_FindEntity(s->shipName);
	return e == s->ship && !e->deadNode.target ? e : NULL;
}

int Players_ShipOwner(cEntity *e) {
	for (int i = 0; e && Players_Net() && i < PLAYERS_MAX; i++) {
		if (g_PlState[i].ship == e && Players_Ship(i) == e) {
			return i;
		}
	}
	return -1;
}

uint16_t Players_Joined(int i) {
	return Players_Net() && i >= 0 && i < PLAYERS_MAX ? g_PlState[i].joined : 0;
}

void Players_BindJoined(int i, uint16_t joined) {
	if (Players_Net() && i >= 0 && i < PLAYERS_MAX && g_PlState[i].used && g_PlState[i].joined == joined) {
		Players_Bind(i);
	}
}

void Players_BindEntity(cEntity *e) {
	if (Players_Net()) {
		int i = Players_Of(e);
		Players_Bind(i >= 0 ? i : Players_ShipOwner(e));
	}
}

void Players_BindNearest(const vec3_t *pos) {
	if (Players_Net()) {
		Players_Bind(Players_Of(Players_Nearest(pos)));
	}
}

void Players_BindTurn(void) {
	if (!Players_Net()) {
		return;
	}
	for (int k = 0; k < PLAYERS_MAX; k++) {
		int i = (g_Turn + k) % PLAYERS_MAX;
		if (g_PlState[i].used && g_NetPlayers[i]) {
			Players_Bind(i);
			return;
		}
	}
	Players_Bind(PLAYERS_NONE);
}

int Players_Bound(void) {
	return g_Bound;
}

int Players_Money(int i) {
	if (!Players_Net() || i == g_Bound || i < 0 || i >= PLAYERS_MAX) {
		return g_pWorld->money;
	}
	return g_PlState[i].money;
}

const cAlarm *Players_Alarm(int i) {
	if (!Players_Net() || i == g_Bound || i < 0 || i >= PLAYERS_MAX) {
		return &g_pWorld->alarm;
	}
	return &g_PlState[i].alarm;
}

byte Players_MayHurt(cEntity *attacker, cEntity *victim) {
	if (!Players_Net() || attacker == victim || Players_Of(attacker) < 0 || Players_Of(victim) < 0) {
		return 1;
	}
	/* the freeroam factions first (sanctuaries, relations, the law), else the server's -pvp */
	int rule = Faction_PvP(attacker, victim);
	return rule > 0 || (rule < 0 && Instance_PvP());
}

cEntity *Players_NearestWanted(const vec3_t *pos) {
	float d;
	return Players_NearestOf(pos, 1, &d);
}

void Players_LevelReset(int source) {
	if (Players_Net()) {
		for (int i = 0; i <= PLAYERS_MAX; i++) {
			g_PlState[i].level[source] = -1;
		}
	}
}

void Players_LevelRaise(int source, int level) {
	if (Players_Net() && g_PlState[g_Bound].level[source] < level) {
		g_PlState[g_Bound].level[source] = (int8_t)level;
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
		if (g_PlState[i].used && g_NetPlayers[i]) {
			Players_Bind(i);
			Alarm_Update(&g_pWorld->alarm, dt);
		}
	}
	Players_BindTurn();
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
		if (!g_PlState[i].used) {
			continue;
		}
		int l = -1;
		for (int k = 0; k < PLAYERS_LEVELS; k++) {
			l = g_PlState[i].level[k] > l ? g_PlState[i].level[k] : l;
		}
		Players_Bind(i);
		Alarm_SetGrow(&g_pWorld->alarm, l);
	}
	Players_BindTurn();
}
