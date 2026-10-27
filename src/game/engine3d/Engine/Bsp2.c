/*
 * The BSP tree of the shadow casting triangles of the floor (BspLuces of CBsp, the line checks of the light maps),
 * original source Engine/bsp2.cpp (include/scrap/bsp.h).
 */
#include <string.h>

#include "scrap/bsp.h"
#include "scrap/suelo.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"

#define BSP2_EPS 1e-4f
#define BSP2_COLORS ((const uint32_t *)Game_Ptr(0x432a30))	/* 16 */

static uint32_t Bsp2_Bits(float f) {
	uint32_t u;

	memcpy(&u, &f, sizeof(u));
	return u;
}

static Bsp2Tri *Bsp2_Tri(const Bsp2Nodo *node, uint32_t i) {
	TABLA_CHECK(node->tris.p, i, node->tris.n);
	return &((Bsp2Tri *)node->tris.p)[i];
}

static float Bsp2_Dist(const plane_t *pl, const vec3_t *v) {
	return pl->n.x * v->x + pl->n.y * v->y + pl->n.z * v->z + pl->d;
}

/* -1 behind, 0 on, 1 in front of the plane */
static int Bsp2_Side(float d) {
	if (d > BSP2_EPS) {
		return 1;
	}
	return -BSP2_EPS > d ? -1 : 0;
}

Bsp2Nodo *__fastcall Bsp2Nodo_ctor(Bsp2Nodo *self) {
	self->tris.p = NULL;
	self->tris.n = 0;
	self->front = NULL;
	self->back = NULL;
	return self;
}

void __fastcall BspLuces_dtor(void *self) {
	BspLuces *l = self;

	if (l->root) {
		Bsp2Nodo *root = l->root;
		Bsp2Nodo_dtor(root);
		operator_delete_sized(root, sizeof(Bsp2Nodo));
		l->root = NULL;
	}
}

void __fastcall Bsp2Nodo_dtor(Bsp2Nodo *self) {
	if (self->front) {
		Bsp2Nodo *n = self->front;
		Bsp2Nodo_dtor(n);
		operator_delete_sized(n, sizeof(Bsp2Nodo));
		self->front = NULL;
	}
	if (self->back) {
		Bsp2Nodo *n = self->back;
		Bsp2Nodo_dtor(n);
		operator_delete_sized(n, sizeof(Bsp2Nodo));
		self->back = NULL;
	}
	Tabla_Free(&self->tris);
}

int __thiscall BspLuces_CheckLine(BspLuces *self, const vec3_t *a, const vec3_t *b, vec3_t *out) {
	CHECK(++g_Bsp2NComp);
	return Bsp2_CheckLine(a, b, out, self->root) == 1;
}

int Bsp2_CheckLine(const vec3_t *a, const vec3_t *b, vec3_t *out, Bsp2Nodo *node) {
	if (node->front && node->back) {
		float da = Bsp2_Dist(&node->plano, a);
		float db = Bsp2_Dist(&node->plano, b);
		if ((int32_t)(Bsp2_Bits(db) ^ Bsp2_Bits(da)) >= 0) {
			return Bsp2_CheckLine(a, b, out, 0.0f > da ? node->back : node->front);
		}
		/* the halves on both sides, each a bit past the plane */
		float t = da / (da - db);
		vec3_t d = { b->x - a->x, b->y - a->y, b->z - a->z };
		vec3_t m = { d.x * t + a->x, d.y * t + a->y, d.z * t + a->z };
		float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
		vec3_t u = d;
		if ((double)len != 0.0) {
			float k = (float)(1.0 / (double)len);
			u.x = d.x * k;
			u.y = d.y * k;
			u.z = d.z * k;
		}
		u.x = u.x * BSP2_EPS;
		u.y = u.y * BSP2_EPS;
		u.z = u.z * BSP2_EPS;
		vec3_t p1 = { m.x - u.x, m.y - u.y, m.z - u.z };
		int r = Bsp2_CheckLine(a, &p1, out, 0.0f > da ? node->back : node->front);
		if (r != -1) {
			return r;
		}
		vec3_t p2 = { u.x + m.x, u.y + m.y, u.z + m.z };
		return Bsp2_CheckLine(&p2, b, out, 0.0f > da ? node->front : node->back);
	}
	int n = node->tris.n;
	float best = 2.0f;
	for (int i = 0; i < n; i++) {
		const Bsp2Tri *t = Bsp2_Tri(node, i);
		const CMaterial *mat = t->Mat;
		if (!(mat->params.atrib & MAT_SOMBRA) && (mat->params.transp || (mat->mapa[1] && !mat->params.b3b))) {
			continue;
		}
		g_Bsp2NTriCmp++;
		float da = Bsp2_Dist(&t->plano, a);
		float db = Bsp2_Dist(&t->plano, b);
		if ((int32_t)(Bsp2_Bits(db) ^ Bsp2_Bits(da)) < 0) {
			float k = da / (da - db);
			vec3_t p = { (b->x - a->x) * k + a->x, (b->y - a->y) * k + a->y, (b->z - a->z) * k + a->z };
			if (Suelo_PointInTri(&p, &t->v[0], &t->v[1], &t->v[2])) {
				if (!out) {
					return 1;
				}
				best = Float_MinSS(k, best);
			}
		}
		if (1.0f >= best && best >= 0.0f) {
			if (out) {
				out->x = (b->x - a->x) * best + a->x;
				out->y = (b->y - a->y) * best + a->y;
				out->z = (b->z - a->z) * best + a->z;
			}
			return 1;
		}
	}
	return -1;
}

void __fastcall BspLuces_Render(void *self) {
	BspLuces *l = self;

	const vec3_t *eye = Render_GetEyePos();
	g_Bsp2Color = 15;
	g_Bsp2Eye = *eye;
	Bsp2_Render(l->root);
}

void Bsp2_AddTri(byte countOnly, Bsp2Nodo *node, uint32_t *nFront, uint32_t *nBack, int lado, const Bsp2Tri *src,
	const vec3_t *v0, const vec3_t *v1, const vec3_t *v2) {
	CHECK(lado == 1 || lado == -1);
	if (countOnly) {
		if (lado > 0) {
			(*nFront)++;
		} else {
			(*nBack)++;
		}
		return;
	}
	Bsp2Tri t;
	t.Mat = src->Mat;
	t.plano = src->plano;
	t.v[0] = *v0;
	t.v[1] = *v1;
	t.v[2] = *v2;
	if (lado > 0) {
		*Bsp2_Tri(node->front, *nFront) = t;
		(*nFront)++;
	} else {
		*Bsp2_Tri(node->back, *nBack) = t;
		(*nBack)++;
	}
}

void Bsp2_Stats(int print) {
	if (print) {
		E3D_DebugPrintf("\ng_CheckLine3D_bsp2_ntricmp: %d   g_CheckLine3D_bsp2_ncomp: %d\n", g_Bsp2NTriCmp,
			g_Bsp2NComp);
		E3D_DebugPrintf("ult_aciertos: %d   ult_fallos: %d\n", g_Bsp2Hits, g_Bsp2Misses);
	}
}

byte __thiscall Bsp2_ChoosePlane(BspLuces *self, Bsp2Nodo *node, plane_t *dest) {
	(void)self;
	CHECK(node);
	CHECK(dest);
	vec3_t vmin = { 3.4028234663852886e+38f, 3.4028234663852886e+38f, 3.4028234663852886e+38f };
	vec3_t vmax = { -3.4028234663852886e+38f, -3.4028234663852886e+38f, -3.4028234663852886e+38f };
	int n = node->tris.n;
	for (int i = 0; i < n; i++) {
		Bsp2_Tri(node, i);
		Bsp2_Tri(node, i);
		const Bsp2Tri *t = Bsp2_Tri(node, i);
		for (int k = 0; k < 3; k++) {
			vmin.x = Float_MinSS(vmin.x, t->v[k].x);
			vmin.y = Float_MinSS(vmin.y, t->v[k].y);
			vmin.z = Float_MinSS(vmin.z, t->v[k].z);
			vmax.x = Float_MaxSS(vmax.x, t->v[k].x);
			vmax.y = Float_MaxSS(vmax.y, t->v[k].y);
			vmax.z = Float_MaxSS(vmax.z, t->v[k].z);
		}
	}
	/* the planes through the middle of the box along the axes */
	plane_t pl[3] = {
		{ { 1.0f, 0.0f, 0.0f }, (vmax.x + vmin.x) * -0.5f },
		{ { 0.0f, 1.0f, 0.0f }, (vmax.y + vmin.y) * -0.5f },
		{ { 0.0f, 0.0f, 1.0f }, (vmax.z + vmin.z) * -0.5f },
	};
	int front[3], back[3];
	for (int k = 0; k < 3; k++) {
		Bsp2_Split(1, node, &pl[k], &front[k], &back[k]);
	}
	float ex = vmax.x - vmin.x, ey = vmax.y - vmin.y, ez = vmax.z - vmin.z;
	int axis = 2;
	if (ex > ey) {
		if (ex > ez) {
			axis = 0;
		}
	} else if (ey > ez) {
		axis = 1;
	}
	for (int i = 0; i < 3; i++) {
		if (front[axis] > 5 && back[axis] > 5) {
			*dest = pl[axis];
			return 1;
		}
		axis = (axis + 1) % 3;
	}
	return 0;
}

Bsp2Nodo *__thiscall BspLuces_FindLeaf(BspLuces *self, const vec3_t *p) {
	Bsp2Nodo *root = self->root;
	Bsp2Nodo *n = 0.0f > Bsp2_Dist(&root->plano, p) ? root->back : root->front;

	return n ? Bsp2_FindLeaf(p, n) : root;
}

Bsp2Nodo *Bsp2_FindLeaf(const vec3_t *p, Bsp2Nodo *node) {
	for (;;) {
		Bsp2Nodo *n = 0.0f > Bsp2_Dist(&node->plano, p) ? node->back : node->front;
		if (!n) {
			return node;
		}
		node = n;
	}
}

int Bsp2_Count(const Bsp2Nodo *node) {
	if (!node) {
		return 0;
	}
	int n = node->tris.n;
	if (node->front) {
		n += Bsp2_Count(node->front);
	}
	if (node->back) {
		n += Bsp2_Count(node->back);
	}
	return n;
}

Bsp2Tri *Bsp2Tri_Copy(Bsp2Tri *dest, const Bsp2Tri *src) {
	*dest = *src;
	return dest;
}

void __thiscall Bsp2_Build(BspLuces *self, Bsp2Nodo *node, int depth) {
	plane_t pl;

	for (;;) {
		E3D_DebugPrintf("\n%*snodo: %d", depth, "", node->tris.n);
		if (!Bsp2_ChoosePlane(self, node, &pl)) {
			return;
		}
		Bsp2_Split(0, node, &pl, NULL, NULL);
		depth++;
		Bsp2_Build(self, node->front, depth);
		node = node->back;
	}
}

void Bsp2_Split(byte countOnly, Bsp2Nodo *node, const plane_t *pl, int *outFront, int *outBack) {
	int n = node->tris.n;

	if (!countOnly) {
		node->plano = *pl;
		Bsp2Nodo *f = operator_new(sizeof(Bsp2Nodo));
		node->front = f ? Bsp2Nodo_ctor(f) : NULL;
		Bsp2Nodo *b = operator_new(sizeof(Bsp2Nodo));
		node->back = b ? Bsp2Nodo_ctor(b) : NULL;
		Tabla_Alloc38(&node->front->tris, n * 2);
		Tabla_Alloc38(&node->back->tris, n * 2);
	}
	uint32_t nf = 0, nb = 0;
	for (int i = 0; i < n; i++) {
		Bsp2Tri *t = Bsp2_Tri(node, i);
		int s0 = Bsp2_Side(Bsp2_Dist(pl, &t->v[0]));
		int s1 = Bsp2_Side(Bsp2_Dist(pl, &t->v[1]));
		int s2 = Bsp2_Side(Bsp2_Dist(pl, &t->v[2]));
		byte front, back;
		if (!s0 && !s1 && !s2) {
			/* in the plane: on the side its normal faces */
			if (t->plano.n.x * pl->n.x + t->plano.n.y * pl->n.y + t->plano.n.z * pl->n.z > 0.0f) {
				Bsp2_AddTri(countOnly, node, &nf, &nb, 1, t, &t->v[0], &t->v[1], &t->v[2]);
				continue;
			}
			front = 0;
			back = 1;
		} else if (s0 >= 0 && s1 >= 0 && s2 >= 0) {
			front = 1;
			back = 0;
		} else {
			/* the triangles that cross the plane go to both sides whole */
			front = s0 > 0 || s1 > 0 || s2 > 0;
			back = 1;
		}
		if (front) {
			if (!countOnly) {
				*Bsp2_Tri(node->front, nf) = *t;
			}
			nf++;
		}
		if (back) {
			if (!countOnly) {
				*Bsp2_Tri(node->back, nb) = *t;
			}
			nb++;
		}
	}
	if (!countOnly) {
		Tabla_Free(&node->tris);
		Tabla_Resize38(&node->front->tris, nf);
		Tabla_Resize38(&node->back->tris, nb);
	}
	if (outFront) {
		*outFront = nf;
	}
	if (outBack) {
		*outBack = nb;
	}
}

void Bsp2_Render(Bsp2Nodo *node) {
	while (node->back && node->front) {
		if (0.0f > Bsp2_Dist(&node->plano, &g_Bsp2Eye)) {
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
