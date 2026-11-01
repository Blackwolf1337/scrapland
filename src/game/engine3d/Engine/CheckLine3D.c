/*
 * The segment queries against the collision triangles of the floor, original source Engine/checkline3d.cpp
 * (include/scrap/ground.h, CGround_Trace in include/scrap/light.h): walks through the cells of the quad grid that the
 * segment passes, with the triangles shared by cells tested once per query.
 */
#include "scrap/light.h"
#include "scrap/ground.h"

#define TRACE_FLT_MAX 3.4028234663852886e+38f

extern traceCell_t g_TraceCell;

byte Heights_CellStep(CHeights *alt, int *x, int *z, const vec3_t *a, const vec3_t *b, float *t) {
	float x1 = (float)(*x + 1) * alt->sizeX + alt->ext.min.x;
	float x0 = (float)*x * alt->sizeX + alt->ext.min.x;
	float z0 = (float)*z * alt->sizeZ + alt->ext.min.z;
	float z1 = (float)(*z + 1) * alt->sizeZ + alt->ext.min.z;
	float tm = 1.0f, u;
	int edge = 0;

	if (b->x > x1) {
		edge = 1;
		tm = (a->x - x1) / (a->x - b->x);
	} else if (x0 > b->x) {
		edge = 2;
		tm = (a->x - x0) / (a->x - b->x);
	}
	if (b->z > z1) {
		u = (a->z - z1) / (a->z - b->z);
		if (tm > u) {
			edge = 3;
			tm = u;
		}
	} else if (z0 > b->z) {
		u = (a->z - z0) / (a->z - b->z);
		if (tm > u) {
			edge = 4;
			tm = u;
		}
	}
	if (t) {
		*t = tm;
	}
	switch (edge) {
	case 1:
		(*x)++;
		return 1;
	case 2:
		(*x)--;
		return 1;
	case 3:
		(*z)++;
		return 1;
	case 4:
		(*z)--;
		return 1;
	}
	return 0;
}

byte Heights_TraceStep(CHeights *alt) {
	traceCell_t *tc = &g_TraceCell;
	float x0 = (float)tc->x * alt->sizeX + alt->ext.min.x;
	float x1 = (float)(tc->x + 1) * alt->sizeX + alt->ext.min.x;
	float z0 = (float)tc->z * alt->sizeZ + alt->ext.min.z;
	float z1 = (float)(tc->z + 1) * alt->sizeZ + alt->ext.min.z;
	float tm = 2.0f;
	int edge = 0;

	if (tc->to.x > x1) {
		edge = 1;
		tm = (tc->from.x - x1) * tc->invDX;
	} else if (x0 > tc->to.x) {
		edge = 2;
		tm = (tc->from.x - x0) * tc->invDX;
	}
	if (tc->to.z > z1) {
		if (tm > (tc->from.z - z1) * tc->invDZ) {
			tc->z++;
			return 1;
		}
	} else if (z0 > tc->to.z) {
		if (tm > (tc->from.z - z0) * tc->invDZ) {
			tc->z--;
			return 1;
		}
	}
	if (edge == 0) {
		return 0;
	}
	if (edge == 1) {
		tc->x++;
	} else {
		tc->x--;
	}
	return 1;
}

byte Ground_PointInTriIn(const vec3_t *p, const vec3_t *a, const vec3_t *b, const vec3_t *c) {
	vec3_t n = Ground_EdgeCross(p, a, b);
	vec3_t m = Ground_EdgeCross(p, b, c);

	if (0.0f > n.x * m.x + n.y * m.y + n.z * m.z) {
		return 0;
	}
	vec3_t k = Ground_EdgeCross(p, c, a);
	return !(0.0f > k.x * n.x + k.y * n.y + k.z * n.z);
}

static float Plane_Side(const plane_t *pl, const vec3_t *v) {
	return pl->n.y * v->y + pl->n.x * v->x + pl->n.z * v->z + pl->d;
}

static vec3_t Seg_Lerp(const vec3_t *a, const vec3_t *b, float t) {
	return (vec3_t){ (b->x - a->x) * t + a->x, (b->y - a->y) * t + a->y, (b->z - a->z) * t + a->z };
}

/* the quad has no triangles in the y range of the segment part from y0 to y1 */
static int Seg_SkipQuad(const CCollideQuad *q, float y0, float y1) {
	if (q->minY > y0 && q->minY > y1) {
		return 1;
	}
	if (y0 > q->maxY && y1 > q->maxY) {
		return 1;
	}
	return q->gapTop > y0 && q->gapTop > y1 && y0 > q->gapBottom && y1 > q->gapBottom;
}

/* the segment crosses the plane of the triangle inside it: t of the crossing */
static int Seg_HitTri(const CCollideTri *tri, const CCollideMesh *mesh, const vec3_t *a, const vec3_t *b, float *t) {
	float da = Plane_Side(&tri->plane, a);
	float db = Plane_Side(&tri->plane, b);

	if ((0.0f > db) == (0.0f > da)) {
		return 0;
	}
	*t = da / (da - db);
	vec3_t p = Seg_Lerp(a, b, *t);
	return Ground_PointInTri(&p, &mesh->pVert[tri->v[0]], &mesh->pVert[tri->v[1]], &mesh->pVert[tri->v[2]]);
}

enum { SEG_TEST, SEG_POS, SEG_TRI };

/* the walk of TestSeg (the first hit), TraceSegPos and TraceSeg (the nearest hit); mode 0 skips the meshes and the
 * triangles without collisions, TraceSeg with mode 1 also the triangles without the 0x40 flag */
static byte Ground_SegWalk(CGround *self, const vec3_t *a, const vec3_t *b, int mode, int kind, vec3_t *hit,
	CGroundHit *info) {
	CHeights *alt = &self->heights;
	CCollideTri *bestTri = NULL;
	float best = 2.0f;

	CGround_NewQuery(self);
	int cx = (int)((a->x - alt->ext.min.x) * alt->invX);
	int cz = (int)((a->z - alt->ext.min.z) * alt->invZ);
	float endY = a->y;
	for (;;) {
		CCollideQuad *q = Heights_Cell(alt, cx, cz);
		if (!q) {
			return 0;
		}
		int nx = cx, nz = cz;
		float t;
		float startY = endY;
		byte stepped = Heights_CellStep(alt, &nx, &nz, a, b, &t);
		endY = stepped ? a->y + (b->y - a->y) * t : b->y;
		for (; q; q = q->next) {
			if (!mode && (q->Mesh->flags & TRI_ALT_NO_COLLISIONS)) {
				continue;
			}
			if (Seg_SkipQuad(q, startY, endY)) {
				continue;
			}
			for (int k = 0; k < q->nTri; k++) {
				CCollideTri *tri = &q->Mesh->pTri[q->pTri[k]];
				float th;

				if (!CCollideTri_Mark(tri)) {
					continue;
				}
				if (kind == SEG_TRI) {
					if ((tri->flags & TRI_ALT_NO_COLLISIONS) && mode == 0) {
						continue;
					}
					if (!(tri->flags & 0x40) && mode == 1) {
						continue;
					}
				} else if (!mode && (tri->flags & TRI_ALT_NO_COLLISIONS)) {
					continue;
				}
				if (!Seg_HitTri(tri, q->Mesh, a, b, &th)) {
					continue;
				}
				if (kind == SEG_TEST) {
					tri->flags |= TRI_ALT_HIT;
					return 1;
				}
				if (best > th) {
					if (info) {
						info->queryId = g_QuadTriCacheID;
						info->tri = k;
						info->quad = q;
					}
					best = th;
					bestTri = tri;
				}
			}
		}
		if (stepped) {
			cx = nx;
			cz = nz;
		}
		if (kind != SEG_TEST && 1.0f >= best && best >= 0.0f && t >= best) {
			bestTri->flags |= TRI_ALT_HIT;
			*hit = Seg_Lerp(a, b, best);
			return 1;
		}
		if (!stepped) {
			return 0;
		}
	}
}

byte CGround_TestSeg(CGround *self, const vec3_t *a, const vec3_t *b, int flags) {
	return Ground_SegWalk(self, a, b, (byte)flags, SEG_TEST, NULL, NULL);
}

byte CGround_TraceSegPos(CGround *self, const vec3_t *a, const vec3_t *b, vec3_t *hit, int flags) {
	return Ground_SegWalk(self, a, b, (byte)flags, SEG_POS, hit, NULL);
}

byte CGround_TraceSeg(CGround *self, const vec3_t *a, const vec3_t *b, vec3_t *hit, CGroundHit *tri,
	int flags) {
	return Ground_SegWalk(self, a, b, flags, SEG_TRI, hit, tri);
}

static int Float_SignDiffers(float a, float b) {
	union { float f; int32_t i; } ua = { a }, ub = { b };
	return (ub.i ^ ua.i) < 0;
}

/* the crossing of the plane counts: with a threshold, not when from lies less than it behind the plane */
static int Trace_Crosses(float da, byte useThreshold, float threshold) {
	return !useThreshold || -threshold > da || da >= 0.0f;
}

byte CGround_Trace(void *ground, const vec3_t *from, const vec3_t *to, void *res, float threshold, void *cache) {
	CGround *self = ground;
	CGroundHit *c = cache;
	vec3_t *out = res;

	CHECK(!out);
	byte useThreshold = threshold > 0.0f;
	CGround_NewQuery(self);
	if (!c) {
		c = (CGroundHit *)self->cache;
	}
	if (c->quad && c->queryId == g_QuadTriCacheID && !out) {
		CCollideMesh *mesh = c->quad->Mesh;
		CCollideTri *tri = &mesh->pTri[c->quad->pTri[c->tri]];
		float da = Plane_Side(&tri->plane, from);
		float db = Plane_Side(&tri->plane, to);
		if (Float_SignDiffers(da, db) && Trace_Crosses(da, useThreshold, threshold)) {
			vec3_t p = Seg_Lerp(from, to, da / (da - db));
			if (Ground_PointInTriIn(&p, &mesh->pVert[tri->v[0]], &mesh->pVert[tri->v[1]], &mesh->pVert[tri->v[2]])) {
				self->traceStats[1]++;
				return 1;
			}
		}
		self->traceStats[2]++;
		c->queryId = 0;
		c->tri = 0;
		c->quad = NULL;
	}

	CHeights *alt = &self->heights;
	traceCell_t *tc = &g_TraceCell;
	float best = 2.0f;
	int cx = (int)((from->x - alt->ext.min.x) * alt->invX);
	int cz = (int)((from->z - alt->ext.min.z) * alt->invZ);
	if ((uint32_t)cx >= (uint32_t)alt->nx || (uint32_t)cz >= (uint32_t)alt->nz) {
		/* from outside the grid: the walk starts in the nearest cell when to is inside */
		if ((uint32_t)(int)((to->x - alt->ext.min.x) * alt->invX) >= (uint32_t)alt->nx) {
			return 0;
		}
		if ((uint32_t)(int)((to->z - alt->ext.min.z) * alt->invZ) >= (uint32_t)alt->nz) {
			return 0;
		}
		if (cx <= 0) {
			cx = 0;
		} else if (cx >= alt->nx - 1) {
			cx = alt->nx - 1;
		}
		if (cz <= 0) {
			cz = 0;
		} else if (cz >= alt->nz - 1) {
			cz = alt->nz - 1;
		}
	}
	tc->from = *from;
	tc->to = *to;
	tc->x = cx;
	tc->z = cz;
	float d = from->x - to->x;
	tc->invDX = d == 0.0f ? TRACE_FLT_MAX : 1.0f / d;
	d = from->z - to->z;
	tc->invDZ = d == 0.0f ? TRACE_FLT_MAX : 1.0f / d;
	for (;;) {
		CCollideQuad *q = Heights_Cell(alt, tc->x, tc->z);
		if (!q) {
			return 0;
		}
		for (; q; q = q->next) {
			if (q->Mesh->flags & 2) {
				continue;
			}
			for (int k = 0; k < q->nTri; k++) {
				CCollideMesh *mesh = q->Mesh;
				CCollideTri *tri = &mesh->pTri[q->pTri[k]];

				if (!CCollideTri_Mark(tri)) {
					continue;
				}
				if (tri->flags & 0x22) {
					continue;
				}
				self->traceStats[0]++;
				float da = Plane_Side(&tri->plane, from);
				float db = Plane_Side(&tri->plane, to);
				if (!Float_SignDiffers(da, db) || !Trace_Crosses(da, useThreshold, threshold)) {
					continue;
				}
				float t = da / (da - db);
				vec3_t p = Seg_Lerp(from, to, t);
				if (!Ground_PointInTriIn(&p, &mesh->pVert[tri->v[0]], &mesh->pVert[tri->v[1]],
					&mesh->pVert[tri->v[2]])) {
					continue;
				}
				if (!out) {
					c->queryId = g_QuadTriCacheID;
					c->tri = k;
					c->quad = q;
					return 1;
				}
				if (best > t) {
					best = t;
					c->tri = k;
					c->queryId = g_QuadTriCacheID;
					c->quad = q;
				}
			}
		}
		if (1.0f >= best && best >= 0.0f) {
			break;
		}
		if (!Heights_TraceStep(alt)) {
			return 0;
		}
	}
	if (out) {
		*out = Seg_Lerp(from, to, best);
	}
	return 1;
}
