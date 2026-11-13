/*
 * The players of the world: the owner slots of the local players and the instance's players of the network, see
 * include/scrap/players.h. Not in the original.
 */
#include <float.h>

#include "scrap/alarm.h"
#include "scrap/game.h"
#include "scrap/instance.h"
#include "scrap/mathlib.h"
#include "scrap/players.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

static cEntity *g_NetPlayers[PLAYERS_MAX];	/* the instance's players, found every frame like cWorld.owners */

/* the game state of a player while it is not bound into the world */
typedef struct PlayerState {
	byte used;
	int money;
	cAlarm alarm;
	int8_t level[PLAYERS_LEVELS];
} PlayerState;

static PlayerState g_PlState[PLAYERS_MAX + 1];	/* + the world's own (PLAYERS_NONE) */
static int g_Bound = PLAYERS_NONE;
static int g_Turn;

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

cEntity *Players_Nearest(const vec3_t *pos) {
	cEntity *best = NULL;
	float bestD = FLT_MAX;

	if (!Players_Net()) {
		return cWorld_GetUsrEntity(g_pWorld, 0);
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = g_NetPlayers[i];
		if (e) {
			float d = Players_DistSq(e, pos);
			if (d < bestD) {
				bestD = d;
				best = e;
			}
		}
	}
	return best;
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
	g_Bound = i;
}

void Players_BindEntity(cEntity *e) {
	if (Players_Net()) {
		Players_Bind(Players_Of(e));
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
	return !Players_Net() || Instance_PvP() || attacker == victim || Players_Of(attacker) < 0 || Players_Of(victim) < 0;
}

cEntity *Players_NearestWanted(const vec3_t *pos) {
	cEntity *best = NULL;
	float bestD = FLT_MAX;

	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = g_NetPlayers[i];
		if (e && Players_Alarm(i)->on) {
			float d = Players_DistSq(e, pos);
			if (d < bestD) {
				bestD = d;
				best = e;
			}
		}
	}
	return best;
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
