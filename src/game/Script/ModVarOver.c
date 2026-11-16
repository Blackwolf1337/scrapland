/*
 * The per player overlay of the module variables (stage 1.2 of the second multiplayer design, include/scrap/modvars.h
 * "per player"). Not in the original.
 *
 * One list per player of the instance: module name, variable name, a value or a delete. ModVars.c asks here first for
 * a variable whose netclass tag is PLAYER (or PARTY outside coop) while a player's context is current; a variable the
 * player has not written reads the module's value. The lists are short (the variables one player wrote), so a search
 * by name is enough. Kept apart from ModVars.c so the unit test (tools/script/modvarover_test.c) links it alone.
 */
#include <string.h>

#include "scrap/global.h"
#include "scrap/modvars.h"
#include "scrap/netclass.h"
#include "scrap/players.h"

typedef struct {
	const char *module, *name;	/* static texts of the registry (g_ModVarModules, the table or the overflow list) */
	byte has;			/* 0: deleted for this player */
	nativeVal_t v;
} mvoVar_t;

typedef struct {
	mvoVar_t *v;
	int n, cap;
} mvoList_t;

static mvoList_t mvo_pl[PLAYERS_MAX];

static mvoVar_t *MVO_Find(int pl, const char *module, const char *name) {
	if (pl < 0 || pl >= PLAYERS_MAX) {
		return NULL;
	}
	mvoList_t *l = &mvo_pl[pl];
	for (int i = 0; i < l->n; i++) {
		if (!strcmp(l->v[i].name, name) && !strcmp(l->v[i].module, module)) {
			return &l->v[i];
		}
	}
	return NULL;
}

static mvoVar_t *MVO_Add(int pl, const char *module, const char *name) {
	mvoVar_t *x = MVO_Find(pl, module, name);
	if (x || pl < 0 || pl >= PLAYERS_MAX) {
		return x;
	}
	mvoList_t *l = &mvo_pl[pl];
	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 8;
		l->v = Mem_Realloc(l->v, (uint32_t)(sizeof(mvoVar_t) * (size_t)l->cap));
	}
	x = &l->v[l->n++];
	x->module = module;
	x->name = name;
	x->has = 0;
	Native_SetNone(&x->v);
	return x;
}

static void MVO_Remove(mvoList_t *l, int i) {
	Native_Free(&l->v[i].v);
	l->v[i] = l->v[--l->n];
}

int ModVar_OverGet(int pl, const char *module, const char *name, const nativeVal_t **v) {
	mvoVar_t *x = MVO_Find(pl, module, name);
	*v = x && x->has ? &x->v : NULL;
	return x ? x->has : -1;
}

void ModVar_OverSet(int pl, const char *module, const char *name, const nativeVal_t *v) {
	mvoVar_t *x = MVO_Add(pl, module, name);
	if (x) {
		Native_Free(&x->v);
		Native_Copy(&x->v, v);
		x->has = 1;
	}
}

void ModVar_OverDel(int pl, const char *module, const char *name) {
	mvoVar_t *x = MVO_Add(pl, module, name);
	if (x) {
		Native_Free(&x->v);
		Native_SetNone(&x->v);
		x->has = 0;
	}
}

void ModVar_OverClearModule(const char *module) {
	for (int pl = 0; pl < PLAYERS_MAX; pl++) {
		mvoList_t *l = &mvo_pl[pl];
		for (int i = l->n - 1; i >= 0; i--) {
			if (!strcmp(l->v[i].module, module)) {
				MVO_Remove(l, i);
			}
		}
	}
}

void ModVar_OverClearPlayer(int pl) {
	if (pl < 0 || pl >= PLAYERS_MAX) {
		return;
	}
	mvoList_t *l = &mvo_pl[pl];
	while (l->n) {
		MVO_Remove(l, l->n - 1);
	}
	l->v = Mem_Free(l->v);
	l->cap = 0;
}

int ModVar_OverCount(int pl) {
	return pl >= 0 && pl < PLAYERS_MAX ? mvo_pl[pl].n : 0;
}

byte ModVar_OverTagged(int tag, byte coop) {
	return tag == NC_PLAYER || (tag == NC_PARTY && !coop);
}
