/*
 * NetRep: the interest grid and the priority of the records (include/scrap/netrep.h).
 */
#include <math.h>
#include <stdlib.h>

#include "scrap/netrep.h"

static uint32_t Grid_Hash(int32_t cx, int32_t cz) {
	uint32_t h = (uint32_t)cx * 0x9e3779b1u ^ (uint32_t)cz * 0x85ebca77u;
	return (h ^ (h >> 15)) & (NETREP_GRID_BUCKETS - 1);
}

static int32_t Grid_Cell(float v, float cell) {
	float c = floorf(v / cell);
	if (!(c >= -1.0e9f)) {
		return -1000000000;
	}
	return c > 1.0e9f ? 1000000000 : (int32_t)c;
}

void NetRepGrid_Clear(NetRepGrid *g, float cell) {
	g->cell = cell > 1.0f ? cell : 1.0f;
	for (int i = 0; i < NETREP_GRID_BUCKETS; i++) {
		g->head[i] = -1;
	}
}

void NetRepGrid_Add(NetRepGrid *g, int slot, float x, float z) {
	if (slot < 0 || slot >= NETREP_MAX_ENTS) {
		return;
	}
	int32_t cx = Grid_Cell(x, g->cell), cz = Grid_Cell(z, g->cell);
	uint32_t b = Grid_Hash(cx, cz);
	g->cx[slot] = cx;
	g->cz[slot] = cz;
	g->next[slot] = g->head[b];
	g->head[b] = (int16_t)slot;
}

int NetRepGrid_Query(const NetRepGrid *g, float x, float z, float radius, uint16_t *out, int max) {
	int32_t x0 = Grid_Cell(x - radius, g->cell), x1 = Grid_Cell(x + radius, g->cell);
	int32_t z0 = Grid_Cell(z - radius, g->cell), z1 = Grid_Cell(z + radius, g->cell);
	int n = 0;

	/* more cells than buckets: every bucket once, with the cell range as the test */
	if ((int64_t)(x1 - x0 + 1) * (z1 - z0 + 1) > NETREP_GRID_BUCKETS) {
		for (int b = 0; b < NETREP_GRID_BUCKETS; b++) {
			for (int s = g->head[b]; s >= 0; s = g->next[s]) {
				if (g->cx[s] >= x0 && g->cx[s] <= x1 && g->cz[s] >= z0 && g->cz[s] <= z1 && n < max) {
					out[n++] = (uint16_t)s;
				}
			}
		}
		return n;
	}
	for (int32_t cz = z0; cz <= z1; cz++) {
		for (int32_t cx = x0; cx <= x1; cx++) {
			for (int s = g->head[Grid_Hash(cx, cz)]; s >= 0; s = g->next[s]) {
				if (g->cx[s] == cx && g->cz[s] == cz && n < max) {
					out[n++] = (uint16_t)s;
				}
			}
		}
	}
	return n;
}

float NetRep_Priority(float d, float radius, float err, int op) {
	float k = d / (radius * 0.25f);
	float w = 1.0f / (1.0f + k * k);
	if (w < 0.02f) {
		w = 0.02f;
	}
	switch (op) {
	case NETREP_OP_REMOVE:
		return 2.0f;
	case NETREP_OP_CREATE:
		return 0.5f + 3.0f * w;
	default: {
		float e = err / 200.0f;
		return w * (1.0f + (e > 10.0f ? 10.0f : e));
	}
	}
}

static int Cand_Cmp(const void *a, const void *b) {
	const NetRepCand *x = a, *y = b;
	if (x->prio != y->prio) {
		return x->prio > y->prio ? -1 : 1;
	}
	return (int)x->slot - (int)y->slot;
}

void NetRep_SortCands(NetRepCand *c, int n) {
	qsort(c, (size_t)n, sizeof(*c), Cand_Cmp);
}
