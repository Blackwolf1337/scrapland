/*
 * The box queries against the collision triangles of the floor, original source Engine/checkbbox.cpp
 * (include/scrap/ground.h): axis aligned boxes against triangles, and the oriented boxes (a matrix and half sizes)
 * whose triangles are clipped by the six planes of the box.
 */
#include <string.h>

#include "scrap/ground.h"
#include "scrap/utild3d.h"

#define BBOX_FLT_MAX 3.4028234663852886e+38f
/* the original keeps 8 points on the stack; a triangle clipped by six planes can have 9 */
#define CLIP_MAX 16

/* the contact points and faces of the response of CGround_CheckBBox (colis->calc_respuesta), counted from 0 */
#define g_NTortas GAME_GLOBAL(int, g_NTortas)
#define g_NResponseFaces GAME_GLOBAL(int, g_NResponseFaces)
#define FACES_TORTAS 64
#define g_ResponseFaces (GAME_ARRAY(int[3], g_ResponseFaces))	/* [FACES_TORTAS] */

/* the box sides a point is outside of: 1 x > max, 2 x < min, 4 and 8 for y, 0x10 and 0x20 for z */
static byte Box_OutCode(const vec3_t *mn, const vec3_t *mx, const vec3_t *p) {
	byte c = p->x > mx->x ? 1 : 0;

	if (mn->x > p->x) {
		c |= 2;
	}
	if (p->y > mx->y) {
		c |= 4;
	}
	if (mn->y > p->y) {
		c |= 8;
	}
	if (p->z > mx->z) {
		c |= 0x10;
	}
	if (mn->z > p->z) {
		c |= 0x20;
	}
	return c;
}

static float Box_PlaneSide(const plane_t *pl, float x, float y, float z) {
	return pl->n.x * x + pl->n.y * y + pl->n.z * z + pl->d;
}

/* the diagonal from s to e of the box crosses the plane (sides ds and de) inside the triangle */
static int BoxTri_Diagonal(float ds, float de, const vec3_t *s, const vec3_t *e, const vec3_t *v) {
	if ((0.0f > ds) == (0.0f > de)) {
		return 0;
	}
	float t = ds / (ds - de);
	vec3_t p = { (e->x - s->x) * t + s->x, (e->y - s->y) * t + s->y, (e->z - s->z) * t + s->z };
	return Ground_PointInTri(&p, &v[0], &v[1], &v[2]);
}

byte BoxTri(const vec3_t *bmin, const vec3_t *bmax, const CCollideTri *tri, const vec3_t *verts) {
	vec3_t mn = *bmin, mx = *bmax;
	vec3_t v[3] = { verts[tri->v[0]], verts[tri->v[1]], verts[tri->v[2]] };
	byte code[3];
	int i, c;

	for (i = 0; i < 3; i++) {
		code[i] = Box_OutCode(&mn, &mx, &v[i]);
	}
	if (code[2] & code[0] & code[1]) {
		return 0;
	}
	for (i = 0; i < 3; i++) {
		if (!code[i]) {
			return 1;
		}
	}
	/* the edges where they cross a side plane of the box */
	float eps = g_CollisionEpsilon * 0.1f;
	vec3_t prev = v[2];
	byte prevCode = code[2];
	for (i = 0; i < 3; i++) {
		const vec3_t *cur = &v[i];
		byte cross = code[i] ^ prevCode;
		for (c = 0; c < 6; c++) {
			float num, den;
			if (!(cross & (1 << c))) {
				continue;
			}
			switch (c) {
			case 0:
				num = prev.x - mx.x;
				den = prev.x - cur->x;
				break;
			case 1:
				num = prev.x - mn.x;
				den = prev.x - cur->x;
				break;
			case 2:
				num = prev.y - mx.y;
				den = prev.y - cur->y;
				break;
			case 3:
				num = prev.y - mn.y;
				den = prev.y - cur->y;
				break;
			case 4:
				num = prev.z - mx.z;
				den = prev.z - cur->z;
				break;
			default:
				num = prev.z - mn.z;
				den = prev.z - cur->z;
				break;
			}
			float t = num / den;
			vec3_t p = { (cur->x - prev.x) * t + prev.x, (cur->y - prev.y) * t + prev.y,
				(cur->z - prev.z) * t + prev.z };
			if (!(p.x >= eps + mx.x) && !(mn.x - eps >= p.x) && !(p.y >= eps + mx.y) && !(mn.y - eps >= p.y) &&
				!(p.z >= eps + mx.z) && !(mn.z - eps >= p.z)) {
				return 1;
			}
		}
		prevCode = code[i];
		prev = *cur;
	}
	/* the plane of the triangle between the ends of the four diagonals of the box */
	const plane_t *pl = &tri->plane;
	float s000 = Box_PlaneSide(pl, mn.x, mn.y, mn.z);
	float s100 = Box_PlaneSide(pl, mx.x, mn.y, mn.z);
	float s110 = Box_PlaneSide(pl, mx.x, mx.y, mn.z);
	float s010 = Box_PlaneSide(pl, mn.x, mx.y, mn.z);
	float s001 = Box_PlaneSide(pl, mn.x, mn.y, mx.z);
	float s101 = Box_PlaneSide(pl, mx.x, mn.y, mx.z);
	float s111 = Box_PlaneSide(pl, mx.x, mx.y, mx.z);
	float s011 = Box_PlaneSide(pl, mn.x, mx.y, mx.z);
	vec3_t c100 = { mx.x, mn.y, mn.z }, c011 = { mn.x, mx.y, mx.z };
	vec3_t c110 = { mx.x, mx.y, mn.z }, c001 = { mn.x, mn.y, mx.z };
	vec3_t c010 = { mn.x, mx.y, mn.z }, c101 = { mx.x, mn.y, mx.z };
	return BoxTri_Diagonal(s000, s111, &mn, &mx, v) || BoxTri_Diagonal(s100, s011, &c100, &c011, v) ||
		BoxTri_Diagonal(s110, s001, &c110, &c001, v) || BoxTri_Diagonal(s010, s101, &c010, &c101, v);
}

/* the point where the edge from pts[prev] to pts[i] crosses the plane */
static vec3_t Clip_Edge(const vec3_t *pts, const float *dist, int i, int prev) {
	float t = dist[i] / (dist[i] - dist[prev]);
	return (vec3_t){ pts[i].x + (pts[prev].x - pts[i].x) * t, pts[i].y + (pts[prev].y - pts[i].y) * t,
		pts[i].z + (pts[prev].z - pts[i].z) * t };
}

void Poly_ClipPlane(const vec3_t *n, float d, int *count, vec3_t *pts) {
	vec3_t out[CLIP_MAX];
	float dist[CLIP_MAX];
	int num = *count, first = -1, nIn = 0, nOut = 0, k = 0, i;

	if (num > 0) {
		float bias = (float)__builtin_fabs((double)d) * (g_CollisionEpsilon * 1e-5f);
		for (i = 0; i < num; i++) {
			dist[i] = pts[i].x * n->x + pts[i].y * n->y + pts[i].z * n->z - d + bias;
			if (dist[i] >= 0.0f) {
				nIn++;
				if (first < 0) {
					first = i;
				}
			} else {
				nOut++;
			}
		}
	}
	if (nIn <= 0) {
		*count = 0;
		return;
	}
	if (num == 2) {
		if (nOut > 0) {
			int o = first == 0;
			pts[o] = Clip_Edge(pts, dist, first, o);
		}
		return;
	}
	if (nOut <= 0) {
		*count = num;
		return;
	}
	if (first > 0) {
		/* in from first - 1 to first, the points inside, out after them */
		out[k++] = Clip_Edge(pts, dist, first, first - 1);
		for (i = first; i < num && dist[i] >= 0.0f; i++) {
			out[k++] = pts[i];
		}
		out[k++] = i >= num ? Clip_Edge(pts, dist, 0, num - 1) : Clip_Edge(pts, dist, i, i - 1);
	} else {
		/* the points inside from 0, out, the ones outside, in, the rest inside */
		for (i = 0; i < num && dist[i] >= 0.0f; i++) {
			out[k++] = pts[i];
		}
		out[k++] = Clip_Edge(pts, dist, i, i - 1);
		while (i < num && 0.0f > dist[i]) {
			i++;
		}
		if (i >= num) {
			out[k++] = Clip_Edge(pts, dist, 0, num - 1);
		} else {
			out[k++] = Clip_Edge(pts, dist, i, i - 1);
			for (; i < num && dist[i] >= 0.0f; i++) {
				out[k++] = pts[i];
			}
		}
	}
	memcpy(pts, out, k * sizeof(vec3_t));
	*count = k;
}

/* the axes of the box: the rows of its rotation, in the order the original makes them */
static void Box_Axes(const matrix_t *m, vec3_t ax[3]) {
	vec3_t u = { 1.0f, 0.0f, 0.0f };

	Vec3_TransformDir0(&ax[0], &u, m);
	u = (vec3_t){ 0.0f, 1.0f, 0.0f };
	Vec3_TransformDir0(&ax[1], &u, m);
	u = (vec3_t){ 0.0f, 0.0f, 1.0f };
	Vec3_TransformDir0(&ax[2], &u, m);
}

static vec3_t Vec3_Times(const vec3_t *v, float s) {
	return (vec3_t){ v->x * s, v->y * s, v->z * s };
}

int Box_ClipTri(const matrix_t *m, const float *half, const vec3_t *tri, vec3_t *center) {
	vec3_t pos = { m->m[12], m->m[13], m->m[14] };
	vec3_t ax[3], poly[CLIP_MAX];
	int count = 3, side, i;

	Box_Axes(m, ax);
	poly[0] = tri[0];
	poly[1] = tri[1];
	poly[2] = tri[2];
	for (side = -1; side <= 1; side += 2) {
		float s = (float)side;
		for (i = 0; i < 3; i++) {
			vec3_t n = { ax[i].x * s, ax[i].y * s, s * ax[i].z };
			Poly_ClipPlane(&n, n.y * pos.y + n.x * pos.x + n.z * pos.z - half[i], &count, poly);
		}
	}
	if (count) {
		float sx = 0.0f, sy = 0.0f, sz = 0.0f;
		for (i = 0; i < count; i++) {
			sx += poly[i].x;
			sy += poly[i].y;
			sz += poly[i].z;
		}
		float fn = (float)count;
		center->x = sx / fn;
		center->y = sy / fn;
		center->z = sz / fn;
	}
	return count > 0;
}

float Box_PointDepth(const matrix_t *m, const float *half, const vec3_t *p) {
	vec3_t pos = { m->m[12], m->m[13], m->m[14] };
	vec3_t ax[3];
	float best = BBOX_FLT_MAX;

	Box_Axes(m, ax);
	for (int side = -1; side <= 1; side += 2) {
		float s = (float)side;
		for (int i = 0; i < 3; i++) {
			vec3_t n = Vec3_Times(&ax[i], s);
			float v = (p->x * n.x + p->y * n.y + p->z * n.z) - (n.y * pos.y + n.x * pos.x + n.z * pos.z - half[i]);
			if (!(v > best)) {
				best = v;
			}
		}
	}
	return best;
}

static int Int_Max0(int v) {
	return v > 0 ? v : 0;
}

static int Int_MinLast(int v, int n) {
	return v >= n - 1 ? n - 1 : v;
}

/* the quad has no triangles between loY and hiY */
static int BBox_SkipQuad(const CCollideQuad *q, float loY, float hiY) {
	return q->minY > hiY || loY > q->maxY || (q->gapTop > hiY && loY > q->gapBottom);
}

byte __thiscall CGround_CollideOBB(CGround *self, const matrix_t *m, const vec3_t *half, vec3_t *outPos, vec3_t *outN) {
	CHeights *alt = &self->heights;
	vec3_t pos = { m->m[12], m->m[13], m->m[14] };
	float r = (float)__builtin_sqrt((double)(half->x * half->x + half->y * half->y + half->z * half->z));
	vec3_t neg = { -half->x, -half->y, -half->z };
	vec3_t bestPos, bestN, c;
	CCollideTri *bestTri = NULL;
	float bestDist;

	Debug_DrawBoxM(m, &neg, half, 0xff00);
	int x0 = (int)(((pos.x - r) - alt->ext.min.x) * alt->invX);
	int z0 = (int)(((pos.z - r) - alt->ext.min.z) * alt->invZ);
	int x1 = (int)(((pos.x + r) - alt->ext.min.x) * alt->invX);
	int z1 = (int)(((pos.z + r) - alt->ext.min.z) * alt->invZ);
	float loY = pos.y - r, hiY = pos.y + r;
	x0 = Int_Max0(x0);
	z0 = Int_Max0(z0);
	x1 = Int_MinLast(x1, alt->nx);
	z1 = Int_MinLast(z1, alt->nz);
	if (x1 < x0) {
		return 0;
	}
	CGround_NewQuery(self);
	bestDist = BBOX_FLT_MAX;
	for (int z = z0; z <= z1; z++) {
		for (int x = x0; x <= x1; x++) {
			CCollideQuad *q = Heights_Cell(alt, x, z);
			CHECK(q);
			for (; q; q = q->next) {
				if (q->Mesh->flags & TRI_ALT_NO_COLLISIONS) {
					continue;
				}
				CHECK(loY <= hiY);
				if (BBox_SkipQuad(q, loY, hiY)) {
					continue;
				}
				for (int k = 0; k < q->nTri; k++) {
					CCollideMesh *mesh = q->Mesh;
					CCollideTri *tri = &mesh->pTri[q->pTri[k]];
					if ((tri->flags & TRI_ALT_NO_COLLISIONS) || !CCollideTri_Mark(tri)) {
						continue;
					}
					vec3_t v[3] = { mesh->pVert[tri->v[0]], mesh->pVert[tri->v[1]], mesh->pVert[tri->v[2]] };
					if (!Box_ClipTri(m, &half->x, v, &c)) {
						continue;
					}
					float dy = c.y - pos.y, dx = c.x - pos.x, dz = c.z - pos.z;
					float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
					if (bestDist > d) {
						bestDist = d;
						bestPos = c;
						bestN = tri->plane.n;
						bestTri = tri;
					}
					if (!outPos && !outN) {
						bestTri->flags |= TRI_ALT_HIT;
						return 1;
					}
				}
			}
		}
	}
	if (!bestTri) {
		return 0;
	}
	bestTri->flags |= TRI_ALT_HIT;
	if (outPos) {
		*outPos = bestPos;
	}
	if (outN) {
		*outN = bestN;
	}
	return 1;
}

/* with colis->calc_respuesta (a fatal CHECK) the original also collects the contact points and faces of the box
 * into a response; that code can not run and is left out */
byte __thiscall CGround_CheckBBox(CGround *self, CCollisionBBox *collision) {
	CHeights *alt = &self->heights;

	CHECK(collision);
	g_NTortas = 0;
	g_NResponseFaces = 0;
	CHECK((uint32_t)(collision->use_minmax - 1) < 2);
	const vec3_t *bmn = collision->use_minmax == 1 ? &collision->min : &collision->min2;
	const vec3_t *bmx = collision->use_minmax == 1 ? &collision->max : &collision->max2;
	vec3_t lo = { bmn->x + collision->pos.x, collision->pos.y + bmn->y, collision->pos.z + bmn->z };
	vec3_t hi = { bmx->x + collision->pos.x, collision->pos.y + bmx->y, collision->pos.z + bmx->z };
	int x0 = Int_Max0((int)((lo.x - alt->ext.min.x) * alt->invX));
	int z0 = Int_Max0((int)((lo.z - alt->ext.min.z) * alt->invZ));
	int x1 = Int_MinLast((int)((hi.x - alt->ext.min.x) * alt->invX), alt->nx);
	int z1 = Int_MinLast((int)((hi.z - alt->ext.min.z) * alt->invZ), alt->nz);
	if (x1 < x0) {
		return 0;
	}
	CGround_NewQuery(self);
	CHECK(!collision->calc_response);
	for (int z = z0; z <= z1; z++) {
		for (int x = x0; x <= x1; x++) {
			CCollideQuad *q = Heights_Cell(alt, x, z);
			CHECK(q);
			for (; q; q = q->next) {
				if (q->Mesh->flags & TRI_ALT_NO_COLLISIONS) {
					continue;
				}
				CHECK(lo.y <= hi.y);
				if (BBox_SkipQuad(q, lo.y, hi.y)) {
					continue;
				}
				for (int k = 0; k < q->nTri; k++) {
					CCollideTri *tri = &q->Mesh->pTri[q->pTri[k]];
					if (!CCollideTri_Mark(tri) || (tri->flags & TRI_ALT_NO_COLLISIONS)) {
						continue;
					}
					if (BoxTri(&lo, &hi, tri, q->Mesh->pVert)) {
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

/* the static initializer: the faces of the response cleared */
void CheckBBox_InitResponseFaces(void) {
	for (int i = 0; i < FACES_TORTAS; i++) {
		g_ResponseFaces[i][0] = 0;
		g_ResponseFaces[i][1] = 0;
		g_ResponseFaces[i][2] = 0;
	}
}
