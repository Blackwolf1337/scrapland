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
#define FACES_TORTAS 64

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

static int g_NTortas;
static int g_NResponseFaces;

/* with colis->calc_respuesta (a fatal CHECK) the original also collects the contact points and faces of the box
 * into a response; that code can not run and is left out */
byte CGround_CheckBBox(CGround *self, CCollisionBBox *collision) {
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

static int g_ResponseFaces[64][3];

/* the static initializer: the faces of the response cleared */
void CheckBBox_InitResponseFaces(void) {
	for (int i = 0; i < FACES_TORTAS; i++) {
		g_ResponseFaces[i][0] = 0;
		g_ResponseFaces[i][1] = 0;
		g_ResponseFaces[i][2] = 0;
	}
}
