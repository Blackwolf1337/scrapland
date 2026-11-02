/*
 * The BSP tree of the shadow casting triangles of the floor (BspLights of CBsp, the line checks of the light maps),
 * original source Engine/bsp2.cpp (include/scrap/bsp.h).
 */
#include <string.h>

#include "scrap/bsp.h"
#include "scrap/ground.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"

#define BSP2_EPS 1e-4f
#define BSP2_COLORS (g_BspColors)	/* 16 */

static Bsp2Tri *Bsp2_Tri(const Bsp2Node *node, uint32_t i) {
	TABLE_CHECK(node->tris.p, i, node->tris.n);
	return &((Bsp2Tri *)node->tris.p)[i];
}

static float Bsp2_Dist(const plane_t *pl, const vec3_t *v) {
	return pl->n.x * v->x + pl->n.y * v->y + pl->n.z * v->z + pl->d;
}

static void Bsp2Node_dtor(Bsp2Node *self);
void BspLights_dtor(void *self) {
	BspLights *l = self;

	if (l->root) {
		Bsp2Node *root = l->root;
		Bsp2Node_dtor(root);
		operator_delete_sized(root, sizeof(Bsp2Node));
		l->root = NULL;
	}
}

static void Bsp2Node_dtor(Bsp2Node *self) {
	if (self->front) {
		Bsp2Node *n = self->front;
		Bsp2Node_dtor(n);
		operator_delete_sized(n, sizeof(Bsp2Node));
		self->front = NULL;
	}
	if (self->back) {
		Bsp2Node *n = self->back;
		Bsp2Node_dtor(n);
		operator_delete_sized(n, sizeof(Bsp2Node));
		self->back = NULL;
	}
	Table_Free(&self->tris);
}

static int g_Bsp2NComp;

/* the segment a b blocked by a shadow triangle: the first hit into out (may be NULL) */
/* the statistics of the line checks */
static int g_Bsp2NTriCmp;

/* the eye of the debug drawing and the color index of the next leaf */
static vec3_t g_Bsp2Eye;
static int g_Bsp2Color;

static void Bsp2_Render(Bsp2Node *node);
void BspLights_Render(void *self) {
	BspLights *l = self;

	const vec3_t *eye = Render_GetEyePos();
	g_Bsp2Color = 15;
	g_Bsp2Eye = *eye;
	Bsp2_Render(l->root);
}

static int g_Bsp2Hits;
static int g_Bsp2Misses;

void Bsp2_Stats(int print) {
	if (print) {
		E3D_DebugPrintf("\ng_CheckLine3D_bsp2_ntricmp: %d   g_CheckLine3D_bsp2_ncomp: %d\n", g_Bsp2NTriCmp,
			g_Bsp2NComp);
		E3D_DebugPrintf("ult_aciertos: %d   ult_fallos: %d\n", g_Bsp2Hits, g_Bsp2Misses);
	}
}

/* the axis plane to split the leaf at, 0 for none */
/* the leaf below the root a point is in (the root when its child on that side is missing) */
static void Bsp2_Render(Bsp2Node *node) {
	while (node->back && node->front) {
		if (0.0f > Bsp2_Dist(&node->plane, &g_Bsp2Eye)) {
			Bsp2_Render(node->back);
			node = node->front;
		} else {
			Bsp2_Render(node->front);
			node = node->back;
		}
	}
	int n = node->tris.n;
	for (int i = 0; i < n; i++) {
		const Bsp2Tri *t = Bsp2_Tri(node, i);
		Debug_DrawTri(&t->v[0], &t->v[1], &t->v[2], BSP2_COLORS[g_Bsp2Color & 0xf]);
	}
	g_Bsp2Color++;
}
