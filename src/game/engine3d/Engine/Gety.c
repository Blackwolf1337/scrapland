/*
 * The height of the floor at a point, original source Engine/gety.cpp (include/scrap/suelo.h, sueloportal.h): the
 * vertical ray through the collision triangles of the grid cell of x, z.
 */
#include "scrap/scrap.h"
#include "scrap/suelo.h"
#include "scrap/sueloportal.h"

#define GETY_FLT_MAX 3.4028234663852886e+38f

static CCollideQuad *Gety_Cell(CSuelo *self, float x, float z) {
	CAlturas *alt = &self->alturas;

	return CAlturas_GetCell(alt, (uint32_t)(int)((x - alt->ext.min.x) * alt->invX),
		(uint32_t)(int)((z - alt->ext.min.z) * alt->invZ));
}

/* the y of the plane of t at x, z (n.y must not be 0) */
static float Gety_PlaneY(const CCollideTri *t, float x, float z) {
	return -((t->plane.n.z * z + t->plane.n.x * x + t->plane.d) / t->plane.n.y);
}

/* x, z inside the triangle seen from above */
static byte Gety_InTri(const CCollideMesh *mesh, const CCollideTri *t, float x, float z) {
	const vec3_t *v0 = &mesh->pVert[t->v[0]], *v1 = &mesh->pVert[t->v[1]], *v2 = &mesh->pVert[t->v[2]];

	byte s0 = (v1->x - v0->x) * (z - v0->z) > (v1->z - v0->z) * (x - v0->x);
	byte s1 = (v2->x - v1->x) * (z - v1->z) > (v2->z - v1->z) * (x - v1->x);
	if (s0 != s1) {
		return 0;
	}
	byte s2 = !((v0->x - v2->x) * (z - v2->z) > (v0->z - v2->z) * (x - v2->x));
	return s2 != s0;
}

/* the y range overlap of a quad (inline of suelo.h) */
static byte Gety_QuadInRange(const CCollideQuad *q, float minY, float maxY) {
	CHECK(minY <= maxY);
	return !(q->minY > maxY) && !(minY > q->maxY);
}

float __thiscall CSuelo_GetY(CSuelo *self, float x, float z, float maxY, byte all) {
	float best = -GETY_FLT_MAX;

	CSuelo_NewQuery(self);
	for (CCollideQuad *q = Gety_Cell(self, x, z); q; q = q->next) {
		if (!all && (q->Mesh->flags & TRI_ALT_NO_COLISIONES)) {
			continue;
		}
		if (!(maxY >= q->minY)) {
			continue;
		}
		CCollideMesh *mesh = q->Mesh;
		for (int i = 0; i < q->nTri; i++) {
			const CCollideTri *t = &mesh->pTri[q->pTri[i]];
			if (!all && (t->flags & TRI_ALT_NO_COLISIONES)) {
				continue;
			}
			if (!(t->plane.n.y != 0.0f)) {
				continue;
			}
			float y = Gety_PlaneY(t, x, z);
			if (maxY > y && y > best && Gety_InTri(mesh, t, x, z)) {
				best = y;
			}
		}
	}
	return best;
}

byte __thiscall CSuelo_GetYInfo(void *suelo, float x, float z, SueloRayo *info) {
	CSuelo *self = suelo;
	byte hit = 0;

	CHECK(info);
	CSuelo_NewQuery(self);
	uint32_t flags = info->flags;
	float minY = flags & GETY_INFO_MINY ? info->minY : -GETY_FLT_MAX;
	float maxY = flags & GETY_INFO_MAXY ? info->y : GETY_FLT_MAX;
	float minB = flags & GETY_INFO_MINB ? info->minB : -1.0f;
	float maxB = flags & GETY_INFO_MAXB ? info->maxB : 1.0f;
	uint32_t mask = ~(flags >> 8) & TRI_ALT_NO_COLISIONES;
	if (flags & GETY_INFO_SECTOR) {
		mask |= TRI_ALT_MESH;
	}
	float best = minY;
	for (CCollideQuad *q = Gety_Cell(self, x, z); q; q = q->next) {
		if (mask & q->Mesh->flags) {
			continue;
		}
		if (!Gety_QuadInRange(q, minY, maxY)) {
			continue;
		}
		for (int i = 0; i < q->nTri; i++) {
			const CCollideTri *t = &q->Mesh->pTri[q->pTri[i]];
			if (mask & t->flags) {
				continue;
			}
			if (flags & (GETY_INFO_MINB | GETY_INFO_MAXB)) {
				if (minB > t->plane.n.y || t->plane.n.y > maxB) {
					continue;
				}
			}
			if (!(t->plane.n.y != 0.0f)) {
				continue;
			}
			float y = Gety_PlaneY(t, x, z);
			if (y >= minY && maxY > y && y > best && Gety_InTri(q->Mesh, t, x, z)) {
				hit = 1;
				best = y;
				info->hitY = y;
				info->tri = (CCollideTri *)t;
				info->sector = q->Mesh->sector - 1;
			}
		}
	}
	return hit;
}

byte __thiscall CSuelo_RayoVert(void *suelo, float x, float z, SueloRayo *info) {
	CSuelo *self = suelo;
	byte hit = 0;

	CHECK(info);
	CHECK(info->flags == (GETY_INFO_MAXY | GETY_INFO_SECTOR));
	CSuelo_NewQuery(self);
	float minY = info->flags & GETY_INFO_MINY ? info->minY : -GETY_FLT_MAX;
	float maxY = info->flags & GETY_INFO_MAXY ? info->y : GETY_FLT_MAX;
	CCollideQuad *q = Gety_Cell(self, x, z);
	if (!q) {
		q = self->alturas.outSector;
		if (!q) {
			return 0;
		}
		CHECK(!q || q->Mesh);
	} else if (g_RayoVertSectors) {
		/* the sector meshes of the cell: the one whose y range holds maxY when no other overlaps it */
		CCollideQuad *sel = NULL, *above = NULL;
		byte alone = 1;
		float low = minY;
		for (CCollideQuad *c = q; c; c = c->next) {
			if (c->Mesh->flags & (TRI_ALT_NO_COLISIONES | TRI_ALT_MESH)) {
				continue;
			}
			CHECK(low <= maxY);
			if (c->minY > maxY) {
				above = c;
				continue;
			}
			if (low > c->maxY) {
				continue;
			}
			if (!sel || c->minY > sel->maxY) {
				alone = 1;
				sel = c;
			} else {
				alone = 0;
				if (c->maxY > sel->maxY) {
					sel = c;
				}
			}
			low = sel->minY;
		}
		if (sel && alone) {
			info->hitY = maxY;
			info->tri = NULL;
			info->sector = sel->Mesh->sector - 1;
			if (maxY > sel->maxY && above) {
				info->hitY = sel->maxY;
			}
			return 1;
		}
	}
	for (; q; q = q->next) {
		if (q->Mesh->flags & (TRI_ALT_NO_COLISIONES | TRI_ALT_MESH)) {
			continue;
		}
		if (!Gety_QuadInRange(q, minY, maxY)) {
			continue;
		}
		for (int i = 0; i < q->nTri; i++) {
			CCollideTri *t = &q->Mesh->pTri[q->pTri[i]];
			if (t->flags & (TRI_ALT_NO_COLISIONES | TRI_ALT_MESH)) {
				continue;
			}
			if (t->ncomp == g_CollisionNComp) {
				continue;
			}
			t->ncomp = g_CollisionNComp;
			t->flags |= TRI_ALT_TESTED;
			g_CollisionTests++;
			if (!(t->plane.n.y != 0.0f)) {
				continue;
			}
			float y = Gety_PlaneY(t, x, z);
			if (y > minY && maxY > y && Gety_InTri(q->Mesh, t, x, z)) {
				hit = 1;
				minY = y;
				info->hitY = y;
				info->tri = t;
				info->sector = q->Mesh->sector - 1;
			}
		}
	}
	return hit;
}

float __thiscall CSuelo_GetYRange(CSuelo *self, float x, float z, float maxY, float minY, byte all) {
	float best = -GETY_FLT_MAX;

	CSuelo_NewQuery(self);
	for (CCollideQuad *q = Gety_Cell(self, x, z); q; q = q->next) {
		if (!all && (q->Mesh->flags & TRI_ALT_NO_COLISIONES)) {
			continue;
		}
		if (!Gety_QuadInRange(q, minY, maxY)) {
			continue;
		}
		CCollideMesh *mesh = q->Mesh;
		for (int i = 0; i < q->nTri; i++) {
			const CCollideTri *t = &mesh->pTri[q->pTri[i]];
			if (0.6f > t->plane.n.y) {
				continue;
			}
			if (!all && (t->flags & TRI_ALT_NO_COLISIONES)) {
				continue;
			}
			if (!(t->plane.n.y != 0.0f)) {
				continue;
			}
			float y = Gety_PlaneY(t, x, z);
			if (y > minY && maxY > y && y > best && Gety_InTri(mesh, t, x, z)) {
				best = y;
			}
		}
	}
	return best;
}

byte __thiscall CSuelo_GetYBelow(CSuelo *self, float x, float z, float maxY, float *out) {
	CCollideQuad *q = Gety_Cell(self, x, z);

	if (!q) {
		return 0;
	}
	CSuelo_NewQuery(self);
	float best = -GETY_FLT_MAX;
	for (; q; q = q->next) {
		CCollideMesh *mesh = q->Mesh;
		for (int i = 0; i < q->nTri; i++) {
			const CCollideTri *t = &mesh->pTri[q->pTri[i]];
			if (!(t->plane.n.y != 0.0f)) {
				continue;
			}
			float y = Gety_PlaneY(t, x, z);
			if (maxY > y && y > best && Gety_InTri(mesh, t, x, z)) {
				best = y;
			}
		}
	}
	if (best != -GETY_FLT_MAX) {
		if (out) {
			*out = best;
		}
		return 1;
	}
	return 0;
}

/* the static initializer (rva 0x05f7a0): "FastGetYSector", "Fast GetYSector (0/1)", on */
#define g_FastGetYSectorVar (GAME_ARRAY(cGVar, g_FastGetYSectorVar, 0x4e6e00))

void Gety_InitFastGetYSector(void) {
	GVar_InitInt(g_FastGetYSectorVar, GAME_STR(str_3e6efc, 0x3e6efc), 0, GAME_STR(str_3e6ee4, 0x3e6ee4), 1);
}
