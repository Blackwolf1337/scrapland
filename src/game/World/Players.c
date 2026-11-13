/*
 * The players of the world: the owner slots of the local players and the instance's players of the network, see
 * include/scrap/players.h. Not in the original.
 */
#include <float.h>

#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/players.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

static cEntity *g_NetPlayers[PLAYERS_MAX];	/* the instance's players, found every frame like cWorld.owners */

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
	}
}

void Players_SetOwner(int slot, cEntity *e) {
	if (slot >= 0 && slot < 4) {
		g_pWorld->owners[slot] = e;
	} else if (Players_Net() && slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		g_NetPlayers[slot - PLAYERS_NET_SLOT] = e;
	}
}
