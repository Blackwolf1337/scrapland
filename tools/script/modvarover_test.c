/*
 * Unit test of the per player overlay of the module variables (src/game/Script/ModVarOver.c, stage 1.2 of the second
 * multiplayer design): which tags have an overlay, values and deletes per player, a player that has not written a
 * variable, clearing by module and by player. The native values are stubbed to ints (the overlay only copies and
 * frees them). ctest: modvarover_test.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/modvars.h"
#include "scrap/netclass.h"
#include "scrap/players.h"

static int g_Live;	/* values copied in and not freed yet */

void Native_SetNone(nativeVal_t *v) {
	memset(v, 0, sizeof(*v));
	v->type = NV_NONE;
}

void Native_SetInt(nativeVal_t *v, long i) {
	Native_SetNone(v);
	v->type = NV_INT;
	v->i = i;
}

void Native_Copy(nativeVal_t *out, const nativeVal_t *v) {
	*out = *v;
	if (v->type != NV_NONE) {
		g_Live++;
	}
}

void Native_Free(nativeVal_t *v) {
	if (v->type != NV_NONE) {
		g_Live--;
	}
	v->type = NV_NONE;
}

void *Mem_Realloc(void *p, uint32_t size) {
	return realloc(p, size);
}

void *Mem_Free(void *p) {
	free(p);
	return NULL;
}

static int g_Fails;

#define CHECK_EQ(what, got, want) \
	do { \
		long g_ = (long)(got), w_ = (long)(want); \
		if (g_ != w_) { \
			printf("FAIL %s: %ld, expected %ld\n", what, g_, w_); \
			g_Fails++; \
		} \
	} while (0)

/* the overlay state of pl for module.name, its int in *i */
static int Get(int pl, const char *module, const char *name, long *i) {
	const nativeVal_t *v;
	int st = ModVar_OverGet(pl, module, name, &v);
	*i = v ? v->i : -1;
	return st;
}

int main(void) {
	nativeVal_t v;
	long i;

	/* the tags: PLAYER always, PARTY only outside coop (in coop the party's value is the module's) */
	CHECK_EQ("PLAYER", ModVar_OverTagged(NC_PLAYER, 0), 1);
	CHECK_EQ("PLAYER coop", ModVar_OverTagged(NC_PLAYER, 1), 1);
	CHECK_EQ("PARTY", ModVar_OverTagged(NC_PARTY, 0), 1);
	CHECK_EQ("PARTY coop", ModVar_OverTagged(NC_PARTY, 1), 0);
	CHECK_EQ("WORLD", ModVar_OverTagged(NC_WORLD, 0), 0);
	CHECK_EQ("PLAYER_SHARED", ModVar_OverTagged(NC_PLAYER_SHARED, 0), 0);
	CHECK_EQ("PARTY_SHARED", ModVar_OverTagged(NC_PARTY_SHARED, 0), 0);
	CHECK_EQ("CLIENT", ModVar_OverTagged(NC_CLIENT, 0), 0);
	CHECK_EQ("absent", ModVar_OverTagged(NC_ABSENT, 0), 0);

	/* a player that has not written a variable reads the module's */
	CHECK_EQ("unwritten", Get(0, "Parking", "PlayerShip", &i), -1);

	/* two players keep their own values of the same variable */
	Native_SetInt(&v, 11);
	ModVar_OverSet(0, "Parking", "PlayerShip", &v);
	Native_SetInt(&v, 22);
	ModVar_OverSet(5, "Parking", "PlayerShip", &v);
	CHECK_EQ("player 0 state", Get(0, "Parking", "PlayerShip", &i), 1);
	CHECK_EQ("player 0 value", i, 11);
	CHECK_EQ("player 5 state", Get(5, "Parking", "PlayerShip", &i), 1);
	CHECK_EQ("player 5 value", i, 22);
	CHECK_EQ("player 1 unwritten", Get(1, "Parking", "PlayerShip", &i), -1);
	/* the same name in another module is another variable */
	CHECK_EQ("other module", Get(0, "Sputnik", "PlayerShip", &i), -1);

	/* a write replaces the value (the old one freed) */
	Native_SetInt(&v, 33);
	ModVar_OverSet(0, "Parking", "PlayerShip", &v);
	Get(0, "Parking", "PlayerShip", &i);
	CHECK_EQ("rewrite", i, 33);
	CHECK_EQ("count 0", ModVar_OverCount(0), 1);
	CHECK_EQ("live after rewrite", g_Live, 2);

	/* a delete is kept per player: the variable is gone for that player only */
	ModVar_OverDel(5, "Parking", "PlayerShip");
	CHECK_EQ("deleted", Get(5, "Parking", "PlayerShip", &i), 0);
	CHECK_EQ("other player keeps it", Get(0, "Parking", "PlayerShip", &i), 1);
	ModVar_OverDel(1, "Speech", "NPCName");
	CHECK_EQ("delete of an unwritten one", Get(1, "Speech", "NPCName", &i), 0);
	Native_SetInt(&v, 44);
	ModVar_OverSet(5, "Parking", "PlayerShip", &v);
	CHECK_EQ("set after delete", Get(5, "Parking", "PlayerShip", &i), 1);
	CHECK_EQ("set after delete value", i, 44);

	/* out of range players have no overlay */
	ModVar_OverSet(-1, "Parking", "PlayerShip", &v);
	ModVar_OverSet(PLAYERS_MAX, "Parking", "PlayerShip", &v);
	CHECK_EQ("player -1", Get(-1, "Parking", "PlayerShip", &i), -1);
	CHECK_EQ("player max", Get(PLAYERS_MAX, "Parking", "PlayerShip", &i), -1);

	/* the module made again: its variables go from every overlay, the others stay */
	Native_SetInt(&v, 55);
	ModVar_OverSet(0, "Speech", "NPCName", &v);
	ModVar_OverClearModule("Parking");
	CHECK_EQ("cleared module 0", Get(0, "Parking", "PlayerShip", &i), -1);
	CHECK_EQ("cleared module 5", Get(5, "Parking", "PlayerShip", &i), -1);
	CHECK_EQ("other module stays", Get(0, "Speech", "NPCName", &i), 1);
	CHECK_EQ("other module value", i, 55);

	/* a player that leaves or joins starts empty */
	for (int k = 0; k < 40; k++) {
		char *name = malloc(16);
		snprintf(name, 16, "v%d", k);
		Native_SetInt(&v, k);
		ModVar_OverSet(3, "RaceMaker", name, &v);
	}
	CHECK_EQ("grown", ModVar_OverCount(3), 40);
	Get(3, "RaceMaker", "v39", &i);
	CHECK_EQ("grown value", i, 39);
	ModVar_OverClearPlayer(3);
	CHECK_EQ("cleared player", ModVar_OverCount(3), 0);
	ModVar_OverClearPlayer(0);
	ModVar_OverClearPlayer(1);
	ModVar_OverClearPlayer(5);
	CHECK_EQ("nothing live", g_Live, 0);

	if (g_Fails) {
		printf("modvarover_test: %d failed\n", g_Fails);
		return 1;
	}
	printf("modvarover_test: all passed\n");
	return 0;
}
