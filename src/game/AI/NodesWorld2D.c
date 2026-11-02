/*
 * The graph of the walking characters of the indoor levels, original source AI/NodesWorld2D.cpp with the instances
 * of Global/DList.h it uses (include/scrap/nodesworld2d.h). The base class is in NodesWorldSup.c.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/console.h"
#include "scrap/filepak.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/nodesworld2d.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/staticinit.h"


int crt_chmod(const char *path, int mode);

/* ---- Global/DList.h ---- */

void NW2DRegion_dtor(cNW2DRegion *r) {
	DListVec3_Clear(&r->points);
	DList_FreePool(&r->points, sizeof(cNWVecNode));
}

void NW2DRegionList_Add(cNW2DRegionList *list, const cNW2DRegion *r) {
	cNW2DRegionNode *node;

	if (list->freeNodes) {
		node = DList_NewNode(list, sizeof(*node));
	} else {
		list->iter = NULL;
		node = operator_new(sizeof(*node));
		if (node) {
			memset(&node->r.points, 0, sizeof(node->r.points));
		}
	}
	node->r = *r;
	DList_LinkFirst(list, node);
}

void NW2DRegionList_Release(cNW2DRegionList *list, cNW2DRegionNode *node) {
	if (list->numFree < list->maxFree) {
		node->next = list->freeNodes;
		list->numFree++;
		list->freeNodes = node;
		return;
	}
	if (node) {
		NW2DRegion_dtor(&node->r);
		operator_delete_sized(node, sizeof(*node));
	}
}

void NW2DRegionList_Clear(cNW2DRegionList *list) {
	while (list->head) {
		cNW2DRegionNode *p = list->head;
		list->iter = NULL;
		if (p == p->next) {
			NW2DRegionList_Release(list, p);
			list->head = NULL;
		} else {
			list->head = p->next;
			p->prev->next = p->next;
			p->next->prev = p->prev;
			NW2DRegionList_Release(list, p);
		}
		list->count--;
	}
}

void NW2DRegionList_FreePool(cNW2DRegionList *list) {
	while (list->freeNodes) {
		cNW2DRegionNode *p = list->freeNodes;
		list->freeNodes = p->next;
		NW2DRegion_dtor(&p->r);
		operator_delete_sized(p, sizeof(*p));
	}
	list->numFree = 0;
}

/* ---- CNodesWorld2D ---- */

CNodesWorld2D *CNodesWorld2D_ctor(void *mem) {
	CNodesWorld2D *self = mem;
	CNodesWorld *w = NW2D_SUP(self);

	CNodesWorldSup_ctor(w);
	w->sup.vftable = CNODESWORLD2D_VFTABLE;
	memset(&self->regions, 0, sizeof(self->regions));
	w->sup.maxVisDist = 5000.0f;
	w->sup.gridCell = 750.0f;
	w->sup.pairRange = 1500.0f;
	w->sup.linkDist = 1500.0f;
	w->sup.startVisDist = 1650.0f;
	return self;
}

void CNodesWorld2D_dtor(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);

	w->sup.vftable = CNODESWORLD2D_VFTABLE;
	CNodesWorldSup_FreeGraph(w);
	NW2DRegionList_Clear(&self->regions);
	NW2DRegionList_FreePool(&self->regions);
	CNodesWorldSup_dtor(w);
}

CNodesWorld *CNodesWorld2D_Delete(CNodesWorld *base, byte flags) {
	CNodesWorld2D *self = (CNodesWorld2D *)base;
	CNodesWorld2D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

byte CNodesWorld2D_LoadGraph(CNodesWorld *base) {
	CNodesWorld2D *self = (CNodesWorld2D *)base;
	char path[0x100];

	Con_Printf("Loading Graph 2D AI...\n");
	NW_LevelPath(path, NW2D_SUP(self), "/AI/Path2DAI");
	return CNodesWorldSup_LoadGraph(NW2D_SUP(self), path) != 0;
}

byte CNodesWorld2D_SaveInfPath(CNodesWorld2D *self) {
	char path[0x100];

	NW_LevelPath(path, NW2D_SUP(self), "/AI/InfPath2DAI.pth");
	if (!CNodesWorldSup_SaveInfPath(NW2D_SUP(self), path)) {
		CHECK(!"CNodesWorld2D_SavePath_File_InfPathAI_Not_Open");
		return 0;
	}
	return 1;
}

/* the floor point of p into the file; a fatal error without floor */
static void NW2D_WriteGround(CNodesWorld2D *self, cFilePack *f, const vec3_t *p) {
	vec3_t g;

	if (!CNodesWorld2D_Ground(self, p, &g, g_NW2DHeight)) {
		Con_FatalErrorf("ERROR AI - PROBLEMA PUNTO INVALIDO GRAFO 2D: %f %f %f\n", (double)p->x, (double)p->y,
			(double)p->z);
	}
	cFilePack_Write(&f->file.base, &g, sizeof(g));
}

byte CNodesWorld2D_SavePath(CNodesWorld *base) {
	CNodesWorld2D *self = (CNodesWorld2D *)base;
	CNodesWorld *w = NW2D_SUP(self);
	char path[0x100];
	cFilePack f;
	byte ok;

	Con_Printf("Saving Path AI...\n");
	NW_LevelPath(path, w, "/AI/Path2DAI.pth");
	crt_chmod(path, 0x180);
	cFilePack_ctor(&f);
	if (!cFilePack_Open(&f.file.base, path, 6)) {
		Con_Bluef("ERROR AI: File Path AI no abierto.\n");
		CHECK(!"CNodesWorld2D_SavePath_File_Path_Not_Open");
		ok = 0;
	} else {
		int n = w->sup.points.count;
		cFilePack_Write(&f.file.base, &n, 4);
		cNWPointNode *p = w->sup.points.iter = w->sup.points.head;
		while (p) {
			NW2D_WriteGround(self, &f, &p->pos);
			p = w->sup.points.iter = w->sup.points.iter->next;
			if (p == w->sup.points.head) {
				break;
			}
		}
		p = w->sup.points.iter = w->sup.points.head;
		while (p) {
			int links = p->links.count;
			cFilePack_Write(&f.file.base, &links, 4);
			cNWIntNode *l = p->links.iter = p->links.head;
			while (l) {
				NW2D_WriteGround(self, &f, l->data);
				l = p->links.iter = p->links.iter->next;
				if (l == p->links.head) {
					break;
				}
			}
			p = w->sup.points.iter = w->sup.points.iter->next;
			if (p == w->sup.points.head) {
				break;
			}
		}
		if (!cFilePack_Close(&f.file.base)) {
			Con_Bluef("ERROR AI: File Path AI no cerrado.\n");
			CHECK(!"CNodesWorld2D_SavePath_File_Path_Not_Closed");
			ok = 0;
		} else {
			ok = 1;
		}
	}
	cFilePack_dtor(&f);
	return ok;
}

void CNodesWorld2D_ClearLists(CNodesWorld *base) {
	CNodesWorld2D *self = (CNodesWorld2D *)base;
	CNodesWorld *w = NW2D_SUP(self);

	DListVec3_Clear(&w->sup.extraPoints);
	NWPointList_Clear(&w->sup.points);
	NWVecList_Clear(&w->sup.foundNodes[0]);
	NWVecList_Clear(&w->sup.foundNodes[1]);
	for (int i = 0; i < 5; i++) {
		DListVec3_Clear(&w->sup.cellLists[i]);
	}
	if (!g_NW2DDrawMode) {
		return;
	}
	cNW2DRegionNode *r = self->regions.iter = self->regions.head;
	while (r) {
		DListVec3_Clear(&r->r.points);
		r = self->regions.iter = self->regions.iter->next;
		if (r == self->regions.head) {
			break;
		}
	}
	NW2DRegionList_Clear(&self->regions);
}

/* the floor below p: at p, again at p + 0, then 15 higher, each g_NW2DGroundDist down; 1 with hit set */
int NW2D_Probe(const vec3_t *p, vec3_t *hit) {
	float d = g_NW2DGroundDist;
	vec3_t a, q;

	q.x = p->x - 0.0f;
	q.y = p->y - d;
	q.z = p->z - 0.0f;
	if (Engine_TestLine(p, &q, hit, NULL)) {
		return 1;
	}
	a.x = p->x + 0.0f;
	a.y = p->y + 0.0f;
	a.z = p->z + 0.0f;
	q.x = a.x - 0.0f;
	q.y = a.y - d;
	q.z = a.z - 0.0f;
	if (Engine_TestLine(&a, &q, hit, NULL)) {
		return 1;
	}
	a.x = p->x + 15.0f;
	a.y = p->y + 15.0f;
	a.z = p->z + 15.0f;
	q.x = a.x - 0.0f;
	q.y = a.y - d;
	q.z = a.z - 0.0f;
	return Engine_TestLine(&a, &q, hit, NULL) != 0;
}

byte CNodesWorld2D_Ground(CNodesWorld2D *self, const vec3_t *p, vec3_t *hit, float height) {
	(void)self;
	if (!NW2D_Probe(p, hit)) {
		*hit = *p;
		return 0;
	}
	hit->y = hit->y + height;
	return 1;
}

byte CNodesWorld2D_GroundSees(CNodesWorld2D *self, const vec3_t *p, const vec3_t *b, float height) {
	vec3_t hit;

	(void)self;
	if (!NW2D_Probe(p, &hit)) {
		return 0;
	}
	hit.y = hit.y + height;
	return Engine_TestLine(&hit, b, NULL, NULL) == 0;
}

byte CNodesWorld2D_GroundLine(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, int u0, int u1,
	int u2) {
	vec3_t ga, gb;

	(void)u0;
	(void)u1;
	(void)u2;
	if (!CNodesWorld2D_Ground(self, a, &ga, g_NW2DHeight) || !CNodesWorld2D_Ground(self, b, &gb, g_NW2DHeight)) {
		return 0;
	}
	return Engine_TestLine(&ga, &gb, NULL, NULL) == 0;
}

/* the foot of radius foot down from p onto the floor, then the capsule of radius r from h1 to h2 above it free;
 * out the top of the capsule. 0 without floor or with the capsule blocked */
static int NW2D_FitDown(const vec3_t *p, vec3_t *out, float down, float foot, float r, float h2, float h1) {
	vec3_t q, o, s, t;

	q.x = p->x - 0.0f;
	q.y = p->y - down;
	q.z = p->z - 0.0f;
	if (!Engine_TestSphere(p, &q, foot, out, NULL)) {
		return 0;
	}
	o.x = out->x - 0.0f;
	o.y = out->y - foot;
	o.z = out->z - 0.0f;
	s.x = o.x + 0.0f;
	s.y = o.y + h1;
	s.z = o.z + 0.0f;
	t.x = s.x;
	t.y = o.y + h2;
	t.z = s.z;
	byte hit = Engine_TestSphere(&s, &t, r, NULL, NULL);
	*out = t;
	return !hit;
}

byte CNodesWorld2D_FitAt(CNodesWorld2D *self, float x, float y, float z, vec3_t *out, float down, float foot,
	float r, float h2, float h1) {
	vec3_t p = { x, y, z };

	(void)self;
	return NW2D_FitDown(&p, out, down, foot, r, h2, h1);
}

byte CNodesWorld2D_Walk(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float up, float foot,
	float r, float h2, float h1) {
	vec3_t c = *a, dir, next, hit, d;

	(void)self;
	NW_Normalize(&dir, b->x - a->x, 0.0f, b->z - a->z);
	/* the box of a and b on the floor the steps stay in */
	float minX = a->x, maxX, minZ = a->z, maxZ;
	if (b->x > minX) {
		maxX = b->x;
	} else {
		maxX = minX;
		minX = b->x;
	}
	if (b->z > minZ) {
		maxZ = b->z;
	} else {
		maxZ = minZ;
		minZ = b->z;
	}
	byte last = 0, stop = 0;
	for (;;) {
		if (stop) {
			return 1;
		}
		next.x = c.x + dir.x * 30.0f;
		next.y = dir.y * 30.0f + c.y;
		next.z = c.z + dir.z * 30.0f;
		if ((dir.x == 0.0f && dir.z == 0.0f) || !(next.x >= minX) || !(maxX >= next.x) || !(next.z >= minZ) ||
			!(maxZ >= next.z)) {
			/* the last step ends at b, at the height of the walk */
			last = 1;
			next.x = b->x;
			next.y = c.y;
			next.z = b->z;
			if (dir.x == 0.0f && dir.z == 0.0f) {
				stop = 1;
			}
		}
		byte blocked = 0;
		if (Engine_TestSphere(&c, &next, r, &hit, NULL)) {
			next = hit;
			blocked = 1;
			last = 0;
		}
		vec3_t n2 = next;
		if (!NW2D_FitDown(&n2, &d, up, foot, r, h2, h1)) {
			return 0;
		}
		/* blocked without moving on */
		if (blocked && 0.5 > __builtin_fabs((double)(c.x - d.x)) && 0.5 > __builtin_fabs((double)(c.y - d.y)) &&
			0.5 > __builtin_fabs((double)(c.z - d.z))) {
			return 0;
		}
		c = d;
		if (last) {
			return 1;
		}
	}
}

byte CNodesWorld2D_WalkFit(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float up, float foot,
	float r, float h2, float h1) {
	vec3_t ha, hb;

	if (!NW2D_FitDown(a, &ha, 500.0f, foot, r, h2, h1)) {
		return 0;
	}
	if (!CNodesWorld2D_FitAt(self, b->x, b->y, b->z, &hb, 500.0f, foot, r, h2, h1)) {
		return 0;
	}
	if (!CNodesWorld2D_Walk(self, &ha, &hb, up, foot, r, h2, h1)) {
		return 0;
	}
	return CNodesWorld2D_Walk(self, &hb, &ha, up, foot, r, h2, h1);
}

byte CNodesWorld2D_TestStep(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float r, float h) {
	vec3_t s = *a, f, q, hit, p, g;

	s.y = s.y - r;
	float len = NW_Normalize(&f, b->x - s.x, 0.0f, b->z - s.z);
	q.x = f.x * len + s.x;
	q.y = f.y * len + s.y;
	q.z = f.z * len + s.z;
	if (!Engine_TestLine(&s, &q, &hit, NULL)) {
		return 1;
	}
	float d = (float)__builtin_sqrt((double)((s.y - hit.y) * (s.y - hit.y) + (s.x - hit.x) * (s.x - hit.x) +
		(s.z - hit.z) * (s.z - hit.z)));
	float back = 20.0f > d ? d * 0.5f : 20.0f;
	p.x = hit.x - f.x * back;
	p.y = hit.y - f.y * back;
	p.z = hit.z - f.z * back;
	if (!CNodesWorld2D_Ground(self, &p, &g, h)) {
		return 0;
	}
	return Engine_TestSphere(&g, &g, r, NULL, NULL) == 0;
}

static void NW2D_Neg(vec3_t *out, const vec3_t *v) {
	out->x = -v->x;
	out->y = -v->y;
	out->z = -v->z;
}

/* the sides u and -u of the path from a along dl at radius r free */
static int NW2D_PathSides(const vec3_t *a, const vec3_t *u, float r, const vec3_t *dl, cNWSegList *debug) {
	vec3_t nu;

	if (!NW_PathSide(a, u, r, dl, debug)) {
		return 0;
	}
	NW2D_Neg(&nu, u);
	return NW_PathSide(a, &nu, r, dl, debug);
}

byte CNodesWorld2D_TestPath(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float r, float r2,
	float h, float h2, cNWSegList *debug) {
	static const vec3_t up = { 0.0f, 1.0f, 0.0f };
	vec3_t ha, hb, A, B, d, n, nn, u, v, dl;

	(void)r2;
	(void)h2;
	if (!NW2D_Probe(a, &ha)) {
		return 0;
	}
	A.x = ha.x;
	A.y = ha.y + h;
	A.z = ha.z;
	float off = Float_MaxSS(h - r, g_NW2DHeight);
	ha.y = ha.y + off;
	if (!NW2D_Probe(b, &hb)) {
		return 0;
	}
	B.x = hb.x;
	B.y = hb.y + h;
	B.z = hb.z;
	hb.y = hb.y + off;
	if (Engine_TestLine(&ha, &hb, NULL, NULL)) {
		return 0;
	}
	NW_DebugSeg(debug, &ha, &hb);
	float len = NW_Normalize(&d, hb.x - ha.x, hb.y - ha.y, hb.z - ha.z);
	/* the side of the path: d x (0 1 0), the products with 0 kept; a vertical path is free */
	float sx = d.y * 0.0f - d.z, sy = d.z * 0.0f - d.x * 0.0f, sz = d.x - d.y * 0.0f;
	if (sx == 0.0f && sy == 0.0f && sz == 0.0f) {
		return 1;
	}
	NW_Normalize(&n, sx, sy, sz);
	dl.x = d.x * len;
	dl.y = d.y * len;
	dl.z = d.z * len;
	if (!NW2D_PathSides(&A, &n, r, &dl, debug)) {
		return 0;
	}
	NW2D_Neg(&nn, &n);
	if (!NW_PathSide(&A, &up, r, &dl, debug)) {
		return 0;
	}
	v.x = nn.x + 0.0f;
	v.y = nn.y + 1.0f;
	v.z = nn.z + 0.0f;
	Vec3_NormalizeTo(&u, &v);
	if (!NW2D_PathSides(&A, &u, r, &dl, debug)) {
		return 0;
	}
	v.x = nn.x - 0.0f;
	v.y = nn.y - 1.0f;
	v.z = nn.z - 0.0f;
	Vec3_NormalizeTo(&u, &v);
	if (!NW2D_PathSides(&A, &u, r, &dl, debug)) {
		return 0;
	}
	if (!CNodesWorld2D_TestStep(self, &A, &B, r, h)) {
		return 0;
	}
	return CNodesWorld2D_TestStep(self, &B, &A, r, h) != 0;
}

/* the side of the direction d: d x (0 1 0), the products with 0 kept; 0 for a vertical d */
static int NW2D_Side(vec3_t *out, const vec3_t *d) {
	out->x = d->y * 0.0f - d->z;
	out->y = d->z * 0.0f - d->x * 0.0f;
	out->z = d->x - d->y * 0.0f;
	return out->x != 0.0f || out->y != 0.0f || out->z != 0.0f;
}

/* the corner at b between the sides u1 and u0 free: the sides, then the sides turned up and down */
static int NW2D_CornerSides(const vec3_t *b, const vec3_t *u1, const vec3_t *u0, float r, cNWSegList *debug) {
	vec3_t v, s1, s0;

	if (!NW_CornerSide(b, u1, u0, r, debug)) {
		return 0;
	}
	v.x = u1->x + 0.0f;
	v.y = u1->y + 1.0f;
	v.z = u1->z + 0.0f;
	Vec3_NormalizeTo(&s1, &v);
	v.x = u0->x + 0.0f;
	v.y = u0->y + 1.0f;
	v.z = u0->z + 0.0f;
	Vec3_NormalizeTo(&s0, &v);
	if (!NW_CornerSide(b, &s1, &s0, r, debug)) {
		return 0;
	}
	v.x = u1->x - 0.0f;
	v.y = u1->y - 1.0f;
	v.z = u1->z - 0.0f;
	Vec3_NormalizeTo(&s1, &v);
	v.x = u0->x - 0.0f;
	v.y = u0->y - 1.0f;
	v.z = u0->z - 0.0f;
	Vec3_NormalizeTo(&s0, &v);
	return NW_CornerSide(b, &s1, &s0, r, debug);
}

byte CNodesWorld2D_TestCorner(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, const vec3_t *c,
	float r, float r2, float h, float h2, byte flat, cNWSegList *debug) {
	vec3_t ha, hb, hc, B, d1, d0, n, n1, n0;

	(void)self;
	(void)r2;
	(void)h2;
	if (!NW2D_Probe(b, &hb)) {
		return 0;
	}
	B.x = hb.x;
	B.y = hb.y + h;
	B.z = hb.z;
	float off = Float_MaxSS(h - r, g_NW2DHeight);
	hb.y = hb.y + off;
	if (!NW2D_Probe(c, &hc)) {
		return 0;
	}
	hc.y = hc.y + off;
	if (!NW2D_Probe(a, &ha)) {
		return 0;
	}
	ha.y = ha.y + off;
	NW_Normalize(&d1, hc.x - hb.x, hc.y - hb.y, hc.z - hb.z);
	NW_Normalize(&d0, hb.x - ha.x, hb.y - ha.y, hb.z - ha.z);
	/* a vertical side is free */
	if (!NW2D_Side(&n, &d1)) {
		return 1;
	}
	Vec3_NormalizeTo(&n1, &n);
	if (!NW2D_Side(&n, &d0)) {
		return 1;
	}
	Vec3_NormalizeTo(&n0, &n);
	if (!flat) {
		NW2D_Neg(&n1, &n1);
		NW2D_Neg(&n0, &n0);
	}
	return NW2D_CornerSides(&B, &n1, &n0, r, debug);
}

cNW2DBody g_NW2DBodies[3];

byte CNodesWorld2D_TestWalk(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float shrink) {
	for (int i = 0; i < 3; i++) {
		const cNW2DBody *body = &g_NW2DBodies[i];
		float r = body->r - shrink;
		float foot = body->foot, h2 = body->h2, h1 = body->h1;
		vec3_t ha, hb;

		if (!NW2D_FitDown(a, &ha, 500.0f, foot, r, h2, h1)) {
			return 0;
		}
		if (!NW2D_FitDown(b, &hb, 500.0f, foot, r, h2, h1)) {
			return 0;
		}
		if (!CNodesWorld2D_Walk(self, &ha, &hb, 442.891998f, foot, r, h2, h1)) {
			return 0;
		}
		if (!CNodesWorld2D_Walk(self, &hb, &ha, 442.891998f, foot, r, h2, h1)) {
			return 0;
		}
	}
	return 1;
}

void CNodesWorld2D_AnalyzeGraph(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);
	byte bad;

	g_NWAnalyzeOn = 1;
	g_NW2DDrawMode = 9;
	DListVec3_Clear(&w->sup.errCollide);
	bad = 0;
	for (int i = 0; i < w->sup.numNodes; i++) {
		for (int k = 0; k < w->sup.nodes[i].numLinks; k++) {
			const cNWNode *n = &w->sup.nodes[i];
			float h = g_NW2DHeight;
			float off = 221.445999f - h;
			vec3_t a = n->pos, b = n->links[k]->pos, ga, gb;
			a.y = off + a.y;
			b.y = off + b.y;
			if (!CNodesWorld2D_Ground(self, &a, &ga, h) || !CNodesWorld2D_Ground(self, &b, &gb, g_NW2DHeight) ||
				Engine_TestLine(&ga, &gb, NULL, NULL) || !CNodesWorld2D_TestWalk(self, &a, &b, 1.0f)) {
				NWVecList_Append(&w->sup.errCollide, &w->sup.nodes[i].pos);
				NWVecList_Append(&w->sup.errCollide, &w->sup.nodes[i].links[k]->pos);
				bad = 1;
			}
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. LINKS GRAFO COLISION ESC.: %d\n", w->sup.errCollide.count / 4);
		Con_Bluef("GRAFO ERRONEO!\n");
	} else {
		Con_Printf("\nGRAFO NO COLISIONA CON GEOMETRIA\n");
	}
	DListVec3_Clear(&w->sup.errUnlinked);
	bad = 0;
	for (int i = 0; i < w->sup.numNodes; i++) {
		if (!w->sup.nodes[i].numLinks) {
			NWVecList_Append(&w->sup.errUnlinked, &w->sup.nodes[i].pos);
			bad = 1;
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. NODOS NO CONECTADOS: %d\n", w->sup.errUnlinked.count);
		Con_Bluef("GRAFO ERRONEO!\n");
	} else {
		Con_Printf("\nGRAFO CON NODOS CONECTADOS\n");
	}
	DListVec3_Clear(&w->sup.errOneWay);
	bad = 0;
	for (int i = 0; i < w->sup.numNodes; i++) {
		for (int k = 0; k < w->sup.nodes[i].numLinks; k++) {
			const cNWNode *to = w->sup.nodes[i].links[k];
			int j = 0;
			while (j < to->numLinks && to->links[j] != &w->sup.nodes[i]) {
				j++;
			}
			if (j < to->numLinks) {
				continue;
			}
			NWVecList_Append(&w->sup.errOneWay, &w->sup.nodes[i].pos);
			NWVecList_Append(&w->sup.errOneWay, &w->sup.nodes[i].links[k]->pos);
			bad = 1;
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. LINKS NO CONMUTATIVOS: %d\n", w->sup.errOneWay.count / 2);
		Con_Bluef("GRAFO ERRONEO!\n");
		Con_Bluef("%s", str_Empty);
		return;
	}
	Con_Printf("\nGRAFO CON LINKS CONMUTATIVOS\n");
	Con_Bluef("%s", str_Empty);
}

void CNodesWorld2D_GetPairs(CNodesWorld2D *self, cNWPairList *list) {
	CNodesWorld *w = NW2D_SUP(self);

	Con_Printf("Getting Pairs Points Visibility No Linked...\n");
	cNWPointNode *saved = w->sup.points.iter;
	cNWPointNode *pn = w->sup.points.iter = w->sup.points.head;
	while (pn) {
		const vec3_t *p = &pn->pos;
		cNWPointNode *cur = w->sup.points.iter;
		cNWPointNode *qn = w->sup.points.iter = cur->next;
		while (qn != w->sup.points.head) {
			const vec3_t *q = &qn->pos;
			float dx = p->x - q->x, dy = p->y - q->y, dz = p->z - q->z;
			float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			if (w->sup.pairRange + w->sup.pairRange > d && !w->sup.vftable->Linked(w, p, q)) {
				cNWPair pair;
				float ex = p->x - q->x, ez = p->z - q->z;
				pair.a = p;
				pair.b = q;
				pair.dist = ez * ez + ex * ex;
				w->sup.vftable->InsertPair(w, list, &pair);
			}
			qn = w->sup.points.iter = w->sup.points.iter->next;
		}
		w->sup.points.iter = cur;
		pn = w->sup.points.iter = cur->next;
		if (pn == w->sup.points.head) {
			break;
		}
	}
	w->sup.points.iter = saved;
	Con_Printf("Num Pairs Points Visibility No Linked: %ld\n", (long)list->count);
}

void CNodesWorld2D_LinkExtra(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);

	Con_Printf("Linking Points of Visibility Extra...\n");
	cNWVecNode *esaved = w->sup.extraPoints.iter;
	cNWVecNode *en = w->sup.extraPoints.iter = w->sup.extraPoints.head;
	while (en) {
		/* the data of a cNWPointNode: the position, the links and the box zeroed */
		cNWPointNode tmp;
		memset(&tmp, 0, sizeof(tmp));
		tmp.pos = en->v;
		NWPointList_Add(&w->sup.points, &tmp.pos);
		cNWPointNode *saved = w->sup.points.iter;
		cNWPointNode *pn = w->sup.points.iter = w->sup.points.head;
		const vec3_t *p = pn ? &pn->pos : NULL;
		cNWPointNode *qn = w->sup.points.iter = w->sup.points.iter->next;
		while (qn != w->sup.points.head) {
			const vec3_t *q = &qn->pos;
			vec3_t gp, gq;
			float dx = p->x - q->x, dy = p->y - q->y, dz = p->z - q->z;
			float d = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
			if (w->sup.linkDist > d && CNodesWorld2D_Ground(self, p, &gp, g_NW2DHeight) &&
				CNodesWorld2D_Ground(self, q, &gq, g_NW2DHeight) && !Engine_TestLine(&gp, &gq, NULL, NULL) &&
				CNodesWorld2D_TestWalk(self, p, q, 0.0f)) {
				NWIntList_Add(&NW_POINT(p)->links, (void *const *)&q);
				NWIntList_AddKeepIter(&NW_POINT(q)->links, p);
				w->sup.numPointLinks++;
			}
			qn = w->sup.points.iter = w->sup.points.iter->next;
		}
		w->sup.points.iter = saved;
		en = w->sup.extraPoints.iter = w->sup.extraPoints.iter->next;
		if (en == w->sup.extraPoints.head) {
			en = NULL;
		}
		NWIntList_Free(&tmp.links);
	}
	w->sup.extraPoints.iter = esaved;
}

void CNodesWorld2D_AddExtraPoints(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);
	char buf[20];
	vec3_t p;
	float ang[3];
	byte flag;

	DummySearch_Start("DM_PointGraph2DAI*", 1);
	while (DummySearch_Next(buf, sizeof(buf), &p, ang, &flag)) {
		if (!CNodesWorld2D_TestWalk(self, &p, &p, 0.0f)) {
			Con_FatalErrorf("Punto Extra Grafo 2D Erroneo: %f %f %f\n", (double)p.x, (double)p.y, (double)p.z);
		}
		NWVecList_Add(&w->sup.extraPoints, &p);
	}
}

float CNodesWorld2D_NearestVisDist(CNodesWorld2D *self, const vec3_t *pos) {
	CNodesWorld *w = NW2D_SUP(self);
	float best = 3.4028234663852886e+38f;
	int i = w->sup.vftable->BufferIndex(w, pos);
	cNWVis *vis = w->sup.buffer[i].vis;

	if (!vis) {
		return best;
	}
	cNWIntNode *saved = vis->points.iter;
	cNWIntNode *it = vis->points.iter = vis->points.head;
	while (it) {
		const vec3_t *q = it->data;
		float dz = q->z - pos->z, dx = q->x - pos->x;
		float d = (float)__builtin_sqrt((double)(dz * dz + dx * dx));
		best = Float_MinSS(d, best);
		vis = w->sup.buffer[i].vis;
		it = vis->points.iter = vis->points.iter->next;
		if (it == vis->points.head) {
			break;
		}
	}
	w->sup.buffer[i].vis->points.iter = saved;
	return best;
}

void CNodesWorld2D_CollectCell(CNodesWorld2D *self, const vec3_t *pos, cNWIntList *cell,
	const vec3_t **list, int max, int *count, float r, float h1, float h2, float distMin, float distMax, int mode,
	byte useMax) {
	CNodesWorld *w = NW2D_SUP(self);

	if (!cell) {
		return;
	}
	cell->iter = cell->head;
	if (!cell->head) {
		return;
	}
	cNWIntNode *it = cell->head;
	while (*count < max) {
		vec3_t q = *(const vec3_t *)it->data;
		float dx = pos->x - q.x, dy = pos->y - q.y, dz = pos->z - q.z;
		float d2 = dy * dy + dx * dx + dz * dz;
		if (w->sup.maxVisDist * w->sup.maxVisDist > d2 && d2 > distMin * distMin && (!useMax || distMax * distMax > d2)) {
			int add;
			if (mode == 0) {
				add = CNodesWorld2D_TestPath(self, pos, &q, r, r, h1, h2, NULL);
			} else if (mode == 1) {
				add = CNodesWorld2D_GroundSees(self, &q, pos, h1);
			} else if (mode == 2 && !g_IsMapOutDoor) {
				int f = Floor_Get(&g_pWorld->floor, pos->y);
				add = f == Floor_Get(&g_pWorld->floor, q.y);
			} else {
				add = 1;
			}
			if (add) {
				list[*count] = it->data;
				(*count)++;
			}
		}
		it = cell->iter = cell->iter->next;
		if (it == cell->head) {
			return;
		}
	}
}

/* the search state of SearchRings that the cells need */
typedef struct nw2dRings {
	CNodesWorld2D *self;
	const vec3_t *pos;
	const vec3_t **list;
	int max;
	int *count;
	float r, h1, h2, distMin, distMax;
	byte soft;
	int mode;
} nw2dRings;

/* one cell of a ring: its points collected; 1 when the search has enough */
static int NW2DRings_Cell(const nw2dRings *s, const int *x, const int *y, const int *z, float e) {
	CNodesWorld *w = NW2D_SUP(s->self);
	cNWIntList *cell;

	w->sup.grid->vftable->GetCell(w->sup.grid, x, y, z, &cell);
	CNodesWorld2D_CollectCell(s->self, s->pos, cell, s->list, s->max, s->count, s->r, s->h1, s->h2, s->distMin,
		s->distMax, s->mode, s->soft == 0);
	return *s->count >= s->max || (*s->count > 0 && s->soft && e >= s->distMax);
}

/* a side of the ring box moved out one cell: 0 the side stops, 1 it grows, 2 the search ends */
static int NW2DRings_Grow(const nw2dRings *s, int inGrid, float e) {
	const CNodesWorld *w = NW2D_SUP(s->self);

	if (!inGrid || !(w->sup.maxVisDist > e) || (!s->soft && !(s->distMax > e))) {
		return 0;
	}
	if (*s->count > 0 && s->soft && e >= s->distMax) {
		return 2;
	}
	return 1;
}

void CNodesWorld2D_SearchRings(CNodesWorld2D *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float r, float h1, float h2, float distMin, float distMax, byte soft, int mode) {
	CNodesWorld *w = NW2D_SUP(self);
	nw2dRings s = { self, pos, list, max, count, r, h1, h2, distMin, distMax, soft, mode };
	int c[3], n[3], st[3], x, y, z;
	vec3_t cs, dir;
	byte stopXhi = 0, stopXlo = 0, stopYhi = 0, stopYlo = 0, stopZhi = 0, stopZlo = 0;

	CHECK(distMin <= distMax);
	w->sup.grid->vftable->CellCoords(w->sup.grid, pos, &c[0], &c[1], &c[2]);
	w->sup.grid->vftable->GetDivs(w->sup.grid, &n[0], &n[1], &n[2]);
	w->sup.grid->vftable->GetCellSize(w->sup.grid, &cs);
	NW_Normalize(&dir, (pos->x + 1.0f) - pos->x, (pos->y + 1.0f) - pos->y, (pos->z + 1.0f) - pos->z);
	/* the start box: from the cell of pos to the cell of the point distMin along the diagonal */
	if (distMin > 0.0f) {
		st[0] = (int)(((pos->x + dir.x * distMin) - w->sup.min.x) / cs.x);
		st[1] = (int)(((pos->y + dir.y * distMin) - w->sup.min.y) / cs.y);
		st[2] = (int)(((pos->z + dir.z * distMin) - w->sup.min.z) / cs.z);
	} else {
		st[0] = c[0];
		st[1] = c[1];
		st[2] = c[2];
	}
	int dx = st[0] - c[0];
	if (dx <= 0) {
		dx = 1;
	}
	int dy = st[1] - c[1];
	if (dy <= 0) {
		dy = 1;
	}
	int dz = st[2] - c[2];
	if (dz <= 0) {
		dz = 1;
	}
	int hiX = c[0] - 1 + dx;
	if (hiX >= n[0]) {
		hiX = n[0] - 1;
	}
	int loX = c[0] - dx + 1;
	if (loX < 0) {
		loX = 0;
	}
	int hiY = c[1] - 1 + dy;
	if (hiY >= n[1]) {
		hiY = n[1] - 1;
	}
	int loY = c[1] - dy + 1;
	if (loY < 0) {
		loY = 0;
	}
	int hiZ = c[2] - 1 + dz;
	if (hiZ >= n[2]) {
		hiZ = n[2] - 1;
	}
	int loZ = c[2] - dz + 1;
	if (loZ < 0) {
		loZ = 0;
	}
#define NW2DRINGS_STEP(stop, cond, e, op) \
	if (!(stop)) { \
		int g = NW2DRings_Grow(&s, (cond), (e)); \
		if (g == 2) { \
			return; \
		} \
		if (g) { \
			op; \
		} else { \
			(stop) = 1; \
		} \
	}
#define NW2DRINGS_CELL(px, py, pz, e) \
	if (NW2DRings_Cell(&s, (px), (py), (pz), (e))) { \
		return; \
	}
	for (int k = 0;; k++) {
		float fk = (float)k;
		float ex = fk * cs.x, ey = fk * cs.y, ez = fk * cs.z;

		NW2DRINGS_STEP(stopXhi, hiX + 1 < n[0], ex, hiX++)
		NW2DRINGS_STEP(stopXlo, loX - 1 >= 0, ex, loX--)
		NW2DRINGS_STEP(stopYhi, hiY + 1 < n[1], ey, hiY++)
		NW2DRINGS_STEP(stopYlo, loY - 1 >= 0, ey, loY--)
		NW2DRINGS_STEP(stopZhi, hiZ + 1 < n[2], ez, hiZ++)
		NW2DRINGS_STEP(stopZlo, loZ - 1 >= 0, ez, loZ--)
		if (stopXhi && stopXlo && stopYhi && stopYlo && stopZhi && stopZlo) {
			return;
		}
		/* the new faces of the box: x over all y z, y without the x faces, z without the x and y faces */
		if (!stopXhi) {
			for (y = loY; y <= hiY; y++) {
				for (z = loZ; z <= hiZ; z++) {
					NW2DRINGS_CELL(&hiX, &y, &z, ex)
				}
			}
		}
		if (!stopXlo) {
			for (y = loY; y <= hiY; y++) {
				for (z = loZ; z <= hiZ; z++) {
					NW2DRINGS_CELL(&loX, &y, &z, ex)
				}
			}
		}
		int xs = loX + !stopXlo, xe = hiX - !stopXhi;
		if (!stopYhi) {
			for (x = xs; x <= xe; x++) {
				for (z = loZ; z <= hiZ; z++) {
					NW2DRINGS_CELL(&x, &hiY, &z, ey)
				}
			}
		}
		if (!stopYlo) {
			for (x = xs; x <= xe; x++) {
				for (z = loZ; z <= hiZ; z++) {
					NW2DRINGS_CELL(&x, &loY, &z, ey)
				}
			}
		}
		int ys = loY + !stopYlo, ye = hiY - !stopYhi;
		if (!stopZhi) {
			for (x = xs; x <= xe; x++) {
				for (y = ys; y <= ye; y++) {
					NW2DRINGS_CELL(&x, &y, &hiZ, ez)
				}
			}
		}
		if (!stopZlo) {
			for (x = xs; x <= xe; x++) {
				for (y = ys; y <= ye; y++) {
					NW2DRINGS_CELL(&x, &y, &loZ, ez)
				}
			}
		}
	}
#undef NW2DRINGS_STEP
#undef NW2DRINGS_CELL
}

void CNodesWorld2D_GetPointsVis(CNodesWorld2D *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float r, float h1, float h2, float distMin, float distMax, byte soft, int mode) {
	CNodesWorld *w = NW2D_SUP(self);
	vec3_t tmp;

	CHECK(distMin <= distMax);
	*count = 0;
	const vec3_t *cs = w->sup.grid->vftable->GetCellSize(w->sup.grid, &tmp);
	if (cs->x * cs->x + cs->y * cs->y + cs->z * cs->z >= distMin * distMin) {
		cNWIntList *cell;
		w->sup.grid->vftable->GetCellAt(w->sup.grid, pos, &cell);
		CNodesWorld2D_CollectCell(self, pos, cell, list, max, count, r, h1, h2, distMin, distMax, mode, soft == 0);
	}
	if (*count < max) {
		CNodesWorld2D_SearchRings(self, pos, list, max, count, r, h1, h2, distMin, distMax, soft, mode);
	}
}

byte CNodesWorld2D_HasPointVis(CNodesWorld2D *self, const vec3_t *pos, float r, float h1, float h2) {
	const vec3_t *point;
	int count;

	CNodesWorld2D_GetPointsVis(self, pos, &point, 1, &count, r, h1, h2, 0.0f, 2500.0f, 1, 0);
	return count > 0;
}

void CNodesWorld2D_NearestPointVis(CNodesWorld2D *self, const vec3_t *pos, vec3_t *out) {
	CNodesWorld *w = NW2D_SUP(self);
	const vec3_t *list[7];
	int count;

	CNodesWorld2D_GetPointsVis(self, pos, list, 7, &count, 0.0f, 0.0f, 0.0f, 0.0f, w->sup.maxVisDist, 1, 2);
	CHECK(count > 0);
	/* the original leaves both answers unset without a point */
	vec3_t seen = { 0.0f, 0.0f, 0.0f }, any = seen;
	float bestSeen = 3.4028234663852886e+38f, bestAny = 3.4028234663852886e+38f;
	byte found = 0;
	for (int i = 0; i < count; i++) {
		const vec3_t *v = list[i];
		float dx = pos->x - v->x, dy = pos->y - v->y, dz = pos->z - v->z;
		float d2 = dy * dy + dx * dx + dz * dz;
		/* the square of a squared distance, as in the original */
		if (bestSeen * bestSeen > d2 && !Engine_TestLine(v, pos, NULL, NULL)) {
			seen = *v;
			bestSeen = d2;
			found = 1;
		}
		if (bestAny > d2 && !found) {
			any = *v;
			bestAny = d2;
		}
	}
	*out = found ? seen : any;
}

/* the y of the direction dropped when it is not about vertical (|x| or |z| over 0.1) */
static void NW2D_FlatDir(vec3_t *d) {
	if (__builtin_fabs((double)d->x) > (double)0.1f || __builtin_fabs((double)d->z) > (double)0.1f) {
		d->y = 0.0f;
	}
}

byte CNodesWorld2D_BuildPath(CNodesWorld2D *self, cNWVecList *path, int depth, const vec3_t *dir,
	const vec3_t *start, byte direct, cNWNode **node, float r, float h1, float h2, byte flat) {
	CNodesWorld *w = NW2D_SUP(self);
	cNWNode *cand[7], *list[20], *cur;
	/* the original keeps the count of the search, its loop index and the chosen node in one stack slot; the node
	 * chosen last stays when an iteration chooses none */
	int slot = 0;
	cNWNode *chosen = NULL;
	int ncand;
	vec3_t s, d, e, p;

	CHECK(depth <= 20);
	DListVec3_Clear(path);
	s = *start;
	if (direct && CNodesWorld2D_TestPath(self, &s, &(*node)->pos, r, r, h1, h2, NULL)) {
		cur = *node;
		cand[0] = cur;
		ncand = 1;
	} else {
		CNodesWorld2D_GetPointsVis(self, &s, (const vec3_t **)cand, 7, &slot, r, h1, h2, 0.0f, w->sup.startVisDist, 1, 0);
		ncand = slot;
		if (!ncand) {
			return 0;
		}
		cur = cand[0];
	}
	double best = (double)3.4028234663852886e+38f;
	d = *dir;
	NW2D_FlatDir(&d);
	if (ncand > 1) {
		/* the pointer itself when no angle is below FLT_MAX, as in the original */
		cur = (cNWNode *)node;
		e.y = 0.0f;
		for (slot = 0; slot < ncand; slot++) {
			cNWNode *v = cand[slot];
			e.x = v->pos.x - s.x;
			e.z = v->pos.z - s.z;
			double ang = Vec3_Angle(&d, &e);
			if (0.7853981633974483 > ang) {
				cur = v;
				break;
			}
			if (best > ang) {
				cur = v;
				best = ang;
			}
		}
	}
	list[0] = cur;
	p = cur->pos;
	DListVec3_Add(path, &p);
	int n = 1;
	for (int i = 1; i < depth; i++) {
		int numLinks = cur->numLinks;
		byte inList = 1, near = 0;

		best = (double)3.4028234663852886e+38f;
		if (numLinks == 1) {
			n = 1;
			list[0] = cur;
		}
		if (numLinks <= 0) {
			break;
		}
		for (int j = 0; j < numLinks; j++) {
			if (w->sup.vftable->InList(w, list, n, cur->links[j])) {
				continue;
			}
			inList = 0;
			cNWNode *v = cur->links[j];
			const cNWNode *prev = list[n - 1];
			e.y = 0.0f;
			e.x = v->pos.x - prev->pos.x;
			e.z = v->pos.z - prev->pos.z;
			double ang = Vec3_Angle(&d, &e);
			if (0.7853981633974483 > ang) {
				chosen = v;
				near = 1;
				break;
			}
			if (best > ang) {
				chosen = v;
				best = ang;
			}
		}
		if (!near && inList) {
			break;
		}
		cNWNode *next = chosen;
		p = next->pos;
		NWVecList_Add(path, &p);
		if (flat) {
			const cNWNode *prev = list[n - 1];
			d.x = p.x - prev->pos.x;
			d.y = p.y - prev->pos.y;
			d.z = p.z - prev->pos.z;
			NW2D_FlatDir(&d);
		}
		list[n] = next;
		cur = next;
		n++;
	}
	*node = cur;
	return 1;
}

/* the box of the nodes of the generation: the size of a node along each axis and its half */
static void NW2D_SetNodeSize(CNodesWorld *w) {
	w->sup.axis[0].x = w->sup.nodeSize.x;
	w->sup.axis[0].y = 0.0f;
	w->sup.axis[0].z = 0.0f;
	w->sup.axis[1].y = w->sup.nodeSize.y;
	w->sup.axis[1].x = 0.0f;
	w->sup.axis[1].z = 0.0f;
	w->sup.axis[2].z = w->sup.nodeSize.z;
	w->sup.axis[2].x = 0.0f;
	w->sup.axis[2].y = 0.0f;
	w->sup.halfNode.y = w->sup.nodeSize.y * 0.5f;
	w->sup.halfNode.z = w->sup.nodeSize.z * 0.5f;
	w->sup.halfNode.x = w->sup.nodeSize.x * 0.5f;
}

void CNodesWorld2D_Load(CNodesWorld2D *self, int arg, const char *name, const vec3_t *pos) {
	static const cNW2DBody bodies[3] = {
		{ 155.0f, 221.445999f, 189.220001f, 40.0f },
		{ 35.0f, 52.0480003f, 52.0480003f, 12.0f },
		{ 70.0f, 88.8450012f, 88.8450012f, 20.0f },
	};
	CNodesWorld *w = NW2D_SUP(self);
	box_t box;

	Con_Printf("\nCreating Nodes...\n");
	Con_Bluef("%s", str_Empty);
	memcpy(g_NW2DBodies, bodies, sizeof(bodies));
	w->sup.nodeSize = *pos;
	NW2D_SetNodeSize(w);
	w->sup.nodeTenth.z = w->sup.nodeSize.z * 0.1f;
	w->sup.nodeTenth.x = w->sup.nodeSize.x * 0.1f;
	w->sup.nodeTenth.y = w->sup.nodeSize.y * 0.1f;
	w->sup.margin = Float_MinSS(w->sup.nodeSize.z, Float_MinSS(w->sup.nodeSize.y, w->sup.nodeSize.x)) / 100.0f;
	World_GetCollideBox(&box);
	w->sup.min = box.min;
	w->sup.max = box.max;
	if (*name) {
		size_t n = strnlen(name, sizeof(w->sup.name) - 1);
		memcpy(w->sup.name, name, n);
		w->sup.name[n] = 0;
	}
	w->sup.vftable->FreeGraph(w);
	if (arg < 0) {
		if (arg != -1) {
			goto end;
		}
		if (!w->sup.vftable->Create(w)) {
			Con_Warnf("NODES 2D AI LEVEL NOT LOADED.\n\n");
			w->sup.nodeSize.x = 155.0f;
			w->sup.nodeSize.y = 155.0f;
			w->sup.nodeSize.z = 155.0f;
			NW2D_SetNodeSize(w);
			goto end;
		}
		w->sup.vftable->BuildGrid(w);
		Con_Printf("NODES 2D AI LEVEL LOADED.\n\n");
		Con_Bluef("%s", str_Empty);
		goto end;
	}
	if (!(Float_MinSS(pos->z, Float_MinSS(pos->y, pos->x)) > 10.0f) ||
		!(10000.0f > Float_MaxSS(pos->z, Float_MaxSS(pos->y, pos->x)))) {
		CHECK(!"(MinComponent(vectInterNod) > 10.0f) && (MaxComponent(vectInterNod) < 10000.0f)");
	}
	w->sup.created = 0;
	if (arg == 0) {
		char buf[20];
		float ang[3];
		byte flag;

		DummySearch_Start("DM_NodeIniAI", 0);
		if (!DummySearch_Next(buf, sizeof(buf), &w->sup.nodeInit, ang, &flag)) {
			Con_Bluef("ERROR AI: Dummy DM_NodeInitAI no encontrado.\n");
			CHECK(!"CNodesWorld2D_IniNodesWorld_DM_NodeInitAI_Not_Found");
		}
		w->sup.nodeInitDummy = 1;
	} else {
		NW_CamPos(&w->sup.nodeInit.x, &w->sup.nodeInit.y, &w->sup.nodeInit.z);
		w->sup.nodeInitDummy = 0;
	}
	CHECK(w->sup.nodeInit.x >= w->sup.min.x && w->sup.max.x >= w->sup.nodeInit.x && w->sup.nodeInit.y >= w->sup.min.y &&
		w->sup.max.y >= w->sup.nodeInit.y && w->sup.nodeInit.z >= w->sup.min.z && w->sup.max.z >= w->sup.nodeInit.z);
	if (arg <= 0) {
		w->sup.min.x = (w->sup.min.x - w->sup.nodeSize.x) - w->sup.margin;
		w->sup.min.y = (w->sup.min.y - w->sup.nodeSize.y) - w->sup.margin;
		w->sup.min.z = (w->sup.min.z - w->sup.nodeSize.z) - w->sup.margin;
		w->sup.max.x = (w->sup.nodeSize.x + w->sup.max.x) + w->sup.margin;
		w->sup.max.y = (w->sup.max.y + w->sup.nodeSize.y) + w->sup.margin;
		w->sup.max.z = (w->sup.max.z + w->sup.nodeSize.z) + w->sup.margin;
	} else {
		float fa = (float)arg;
		float ex = ((w->sup.halfNode.x + w->sup.halfNode.x) * fa + w->sup.halfNode.x) + w->sup.margin;
		float ey = (w->sup.halfNode.y + (w->sup.halfNode.y + w->sup.halfNode.y) * fa) + w->sup.margin;
		float ez = (w->sup.halfNode.z + (w->sup.halfNode.z + w->sup.halfNode.z) * fa) + w->sup.margin;
		w->sup.min.z = w->sup.nodeInit.z - ez;
		w->sup.min.x = w->sup.nodeInit.x - ex;
		w->sup.min.y = w->sup.nodeInit.y - ey;
		w->sup.max.y = w->sup.nodeInit.y + ey;
		w->sup.max.x = ex + w->sup.nodeInit.x;
		w->sup.max.z = w->sup.nodeInit.z + ez;
	}
	if (arg == 0) {
		CNodesWorld2D_AddExtraPoints(self);
	}
	byte made = CNodesWorld2D_Generate(self, &w->sup.nodeInit, &w->sup.min, &w->sup.max);
	Con_Printf("\nNUM. POINTS OF VISIBILITY GENERATED 2D: %d\n", w->sup.points.count - w->sup.extraPoints.count);
	Con_Printf("NUM. POINTS OF VISIBILITY EXTRA MAP 2D: %d\n", w->sup.extraPoints.count);
	Con_Printf("NUM. POINTS OF VISIBILITY TOTAL 2D: %d\n", w->sup.points.count);
	Con_Printf("NUM. LINKS BETWEEN POINTS VIS. 2D: %d\n", w->sup.numPointLinks);
	Con_Bluef("%s", str_Empty);
	if (made) {
		CNodesWorld2D_SaveInfPath(self);
		w->sup.vftable->FreeTemp(w);
	}
	if (!w->sup.vftable->Create(w)) {
		Con_Bluef("NODES 2D AI LEVEL NOT LOADED.\n\n");
		goto end;
	}
	if (arg == 0) {
		w->sup.vftable->BuildGrid(w);
	}
	w->sup.created = 1;
	Con_Printf("NODES 2D AI LEVEL LOADED.\n");
	Con_Bluef("%s", str_Empty);
end:
	Con_Bluef("%s", str_Empty);
}

/* ---- the generation of the graph ---- */

byte CNodesWorld2D_FindLinkBox(CNodesWorld2D *self, const vec3_t *const *pair, const cNWBox *box,
	int *out) {
	return NWSup_FindLinkBox(NW2D_SUP(self), pair, box, out, 1);
}

byte CNodesWorld2D_FindLinkCell(CNodesWorld2D *self, const vec3_t *const *pair, int *out) {
	return NWSup_FindLinkCell(NW2D_SUP(self), pair, out, 1);
}

/* the floor points of a and b raised by g_NW2DHeight see each other */
static int NW2D_GroundsSee(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b) {
	vec3_t ga, gb;

	return CNodesWorld2D_Ground(self, a, &ga, g_NW2DHeight) && CNodesWorld2D_Ground(self, b, &gb, g_NW2DHeight) &&
		!Engine_TestLine(&ga, &gb, NULL, NULL);
}

byte CNodesWorld2D_FindLinkFree(CNodesWorld2D *self, const vec3_t *const *pair, int *out) {
	CNodesWorld *w = NW2D_SUP(self);
	int best[3];
	float bestDist = 3.4028234663852886e+38f;
	byte found = 0;
	const int *cells = w->sup.reachedCells;

	for (int k = 0; k < w->sup.numReachedCells; k++) {
		vec3_t c, c2;
		int idx = cells[k];
		if (w->sup.buffer[idx].state == 5) {
			continue;
		}
		w->sup.vftable->BufferPos(w, &c, idx);
		if (!NWSup_Near(w, &c, pair) || !NW2D_GroundsSee(self, &c, pair[0]) || !NW2D_GroundsSee(self, &c, pair[1]) ||
			!CNodesWorld2D_TestWalk(self, &c, pair[0], 0.0f) || !CNodesWorld2D_TestWalk(self, &c, pair[1], 0.0f) ||
			!w->sup.vftable->Unreachable(w, pair[0], pair[1], idx)) {
			continue;
		}
		w->sup.vftable->BufferPos(w, &c2, idx);
		float d = NWSup_PairDist(&c2, pair, 1);
		if (bestDist > d) {
			bestDist = d;
			found = 1;
			best[0] = idx;
			*out = idx;
			best[1] = (int)(uintptr_t)pair[0];
			best[2] = (int)(uintptr_t)pair[1];
		}
	}
	if (found) {
		NWSup_LinkFound(w, best, g_NW2DDrawMode);
	}
	return found;
}

static int NW2D_InitFree(CNodesWorld *w, const vec3_t *n, const vec3_t *c) {
	CNodesWorld2D *self = (CNodesWorld2D *)w;

	return NW2D_GroundsSee(self, n, c) && CNodesWorld2D_TestWalk(self, n, c, 0.0f);
}

static float NW2D_InitDist(CNodesWorld *w, const vec3_t *n) {
	return CNodesWorld2D_NearestVisDist((CNodesWorld2D *)w, n);
}

byte CNodesWorld2D_NextInit(CNodesWorld2D *self, vec3_t *out) {
	return NWSup_NextInit(NW2D_SUP(self), out, NW2D_InitFree, NW2D_InitDist);
}

static void NW2D_FloodStep(CNodesWorld *w, cNWVecList *queue, const vec3_t *c, const vec3_t *n) {
	CNodesWorld2D *self = (CNodesWorld2D *)w;

	if (NW2D_GroundsSee(self, c, n) && CNodesWorld2D_TestWalk(self, c, n, 0.0f)) {
		w->sup.buffer[w->sup.vftable->BufferIndex(w, n)].state = 0;
		DListVec3_Add(queue, n);
	}
}

byte CNodesWorld2D_IniBuffer(CNodesWorld2D *self, const vec3_t *init, const vec3_t *min,
	const vec3_t *max) {
	return NWSup_IniBuffer(NW2D_SUP(self), init, min, max, NW2D_FloodStep, 0);
}

/* the cell n joins the region of the point at start: nearer than f40 on the floor plane, the floor points see each
 * other and the bodies walk there */
static int NW2D_RegionReach(CNodesWorld *w, const vec3_t *start, const vec3_t *n) {
	CNodesWorld2D *self = (CNodesWorld2D *)w;
	float dz = start->z - n->z, dx = start->x - n->x;
	float d = (float)__builtin_sqrt((double)(dz * dz + dx * dx));

	return !(d >= w->sup.pairRange) && NW2D_GroundsSee(self, start, n) && CNodesWorld2D_TestWalk(self, start, n, 0.0f);
}

void CNodesWorld2D_CreateRegion(CNodesWorld2D *self, const vec3_t *start) {
	NWSup_CreateRegion(NW2D_SUP(self), start, NW2D_RegionReach, 1);
}

/* the steps of the generation of CNodesWorld2D */
static void NW2D_OpPairs(CNodesWorld *w, cNWPairList *list) {
	CNodesWorld2D_GetPairs((CNodesWorld2D *)w, list);
}

static byte NW2D_OpFindBox(CNodesWorld *w, const vec3_t *const *pair, const cNWBox *box, int *out) {
	return CNodesWorld2D_FindLinkBox((CNodesWorld2D *)w, pair, box, out);
}

static byte NW2D_OpFindCell(CNodesWorld *w, const vec3_t *const *pair, int *out) {
	return CNodesWorld2D_FindLinkCell((CNodesWorld2D *)w, pair, out);
}

static void NW2D_OpRegion(CNodesWorld *w, const vec3_t *start) {
	CNodesWorld2D_CreateRegion((CNodesWorld2D *)w, start);
}

static int NW2D_OpInitOk(CNodesWorld *w, const vec3_t *p) {
	return CNodesWorld2D_TestWalk((CNodesWorld2D *)w, p, p, 0.0f);
}

static byte NW2D_OpIniBuffer(CNodesWorld *w, const vec3_t *init, const vec3_t *min, const vec3_t *max) {
	return CNodesWorld2D_IniBuffer((CNodesWorld2D *)w, init, min, max);
}

static byte NW2D_OpNextInit(CNodesWorld *w, vec3_t *out) {
	return CNodesWorld2D_NextInit((CNodesWorld2D *)w, out);
}

static void NW2D_OpInterRegion(CNodesWorld *w) {
	CNodesWorld2D_InterRegion((CNodesWorld2D *)w);
}

static void NW2D_OpLinkExtra(CNodesWorld *w) {
	CNodesWorld2D_LinkExtra((CNodesWorld2D *)w);
}

static const nwGenOps nw2d_ops = { NW2D_OpPairs, NW2D_OpFindBox, NW2D_OpFindCell, NW2D_OpRegion, NW2D_OpInitOk,
	NW2D_OpIniBuffer, NW2D_OpNextInit, NW2D_OpInterRegion, NW2D_OpLinkExtra, 1 };

void CNodesWorld2D_InterRegion(CNodesWorld2D *self) {
	NWSup_InterRegion(NW2D_SUP(self), &nw2d_ops);
}

byte CNodesWorld2D_Generate(CNodesWorld2D *self, const vec3_t *init, const vec3_t *min, const vec3_t *max) {
	return NWSup_Generate(NW2D_SUP(self), init, min, max, &nw2d_ops);
}

void CNodesWorld2D_Draw(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);
	int mode = g_NW2DDrawMode;

	switch (mode) {
	case 1:
	case 7:
	case 8:
		g_FontTerminal8->color = 0xffffff;
		g_FontTerminal8->align = 4;
		for (int i = 0; i < w->sup.numNodes; i++) {
			DebugUtils_DrawSphere(&w->sup.nodes[i].pos, 25.0f, 0xff00ff, 2, 2);
			if (g_NW2DDrawMode == 8) {
				NWSup_DrawNodeText(&w->sup.nodes[i].pos, i);
			}
			for (int k = 0; k < w->sup.nodes[i].numLinks; k++) {
				DebugUtils_DrawLine(&w->sup.nodes[i].pos, &w->sup.nodes[i].links[k]->pos, 0xff00ff);
			}
			if (g_NW2DDrawMode == 7) {
				vec3_t low = w->sup.nodes[i].pos;
				low.y = low.y - 1000.0f;
				DebugUtils_DrawLine(&w->sup.nodes[i].pos, &low, 0xff00);
			}
		}
		NWSup_DrawFound(w, &w->sup.foundNodes[0], 25.0f, 0xff00);
		NWSup_DrawFound(w, &w->sup.foundNodes[1], 25.0f, 0xff);
		NWSup_DrawExtra(w, 25.0f, 0xffff00);
		if (g_NW2DDrawMode == 8 && w->sup.grid) {
			w->sup.grid->vftable->Draw(w->sup.grid, 0xff00);
		}
		break;
	case 2:
		NWSup_DrawCells(w, &w->sup.cellLists[1], 0xffffff, "ListNodesVis: %d\n");
		break;
	case 3:
		NWSup_DrawCells(w, &w->sup.cellLists[2], 0xff, "ListNodesNotVis: %d\n");
		break;
	case 4:
		NWSup_DrawCells(w, &w->sup.cellLists[0], 0xff0000, "ListNodesMap: %d\n");
		break;
	case 5:
		NWSup_DrawCells(w, &w->sup.cellLists[3], 0xff00, "ListNodesFree: %d\n");
		break;
	case 6:
		NWSup_DrawCells(w, &w->sup.cellLists[4], 0, "ListNodesOutMap: %d\n");
		break;
	case 9:
	case 10: {
		float r = mode == 9 ? 500.0f : 155.0f;
		NWSup_DrawCollide(w, r, 0);
		NWSup_DrawErrors(w, r);
		break;
	}
	}
	/* the node g_NW2DShowNode with its number and the cells of its region */
	int s = g_NW2DShowNode;
	if (s < 0 || s >= w->sup.numNodes) {
		return;
	}
	g_FontTerminal8->color = 0xffffff;
	g_FontTerminal8->align = 4;
	DebugUtils_DrawSphere(&w->sup.nodes[g_NW2DShowNode].pos, 25.0f, 0xff0000, 2, 2);
	NWSup_DrawNodeText(&w->sup.nodes[g_NW2DShowNode].pos, g_NW2DShowNode);
	cNW2DRegionNode *rn = self->regions.iter = self->regions.head;
	while (rn) {
		if (rn->r.id == w->sup.numNodes - g_NW2DShowNode - 1) {
			NWSup_DrawCells(w, &rn->r.points, 0xffffff, NULL);
			Con_Printf("Num. Nodos Visibles: %d\n", rn->r.points.count);
			return;
		}
		rn = self->regions.iter = self->regions.iter->next;
		if (rn == self->regions.head) {
			return;
		}
	}
}

/* the link to q counts when the bodies walk from q to the center of the cell on the floor */
static int NW2D_LinkFree(CNodesWorld *w, const vec3_t *q, int index) {
	CNodesWorld2D *self = (CNodesWorld2D *)w;
	vec3_t c;

	w->sup.vftable->BufferPos(w, &c, index);
	return NW2D_GroundsSee(self, q, &c) && CNodesWorld2D_TestWalk(self, q, &c, 0.0f);
}

byte CNodesWorld2D_UnreachableFree(CNodesWorld2D *self, const vec3_t *from, const vec3_t *to, int index) {
	return NWSup_UnreachableBy(NW2D_SUP(self), from, to, index, NW2D_LinkFree);
}

/* the vtable of CNodesWorld2D */
const CNodesWorldVtbl CNodesWorld2D_vftable = {
	.Delete = CNodesWorld2D_Delete,
	.GetBox = CNodesWorldSup_GetBox,
	.LoadFiles = CNodesWorldSup_LoadGraph,
	.LoadGraph = CNodesWorld2D_LoadGraph,
	.Create = CNodesWorldSup_Create,
	.FileOf = CNodesWorldSup_FileOf,
	.CommonFile = CNodesWorldSup_CommonFile,
	.SnapPos = CNodesWorldSup_SnapPos,
	.OtherLinks = CNodesWorldSup_OtherLinks,
	.IsCreated = CNodesWorldSup_IsCreated,
	.LastNodePos = CNodesWorldSup_LastNodePos,
	.GetVisPoint = CNodesWorldSup_GetVisPoint,
	.NumVisPoints = CNodesWorldSup_NumVisPoints,
	.GetMaxVisDist = CNodesWorldSup_GetMaxVisDist,
	.InGrid = CNodesWorldSup_InGrid,
	.SavePath = CNodesWorld2D_SavePath,
	.CheckBuffer = CNodesWorldSup_CheckBuffer,
	.BuildGrid = CNodesWorldSup_BuildGrid,
	.BufferIndex = CNodesWorldSup_BufferIndex,
	.BufferPos = CNodesWorldSup_BufferPos,
	.GetInfoNodes = CNodesWorldSup_GetInfoNodes,
	.AddVis = CNodesWorldSup_AddVis,
	.Hash = CNodesWorldSup_Hash,
	.ReleaseVis = CNodesWorldSup_ReleaseVis,
	.LinkPoints = CNodesWorldSup_LinkPoints,
	.LinkBack = CNodesWorldSup_LinkBack,
	.PointLinked = CNodesWorldSup_PointLinked,
	.LinkPoint = CNodesWorldSup_LinkPoint,
	.Unreachable = CNodesWorldSup_Unreachable,
	.BoxCells = CNodesWorldSup_BoxCells,
	.FindSorted = CNodesWorldSup_FindSorted,
	.HasPair = CNodesWorldSup_HasPair,
	.InsertPair = CNodesWorldSup_InsertPair,
	.VisHas = CNodesWorldSup_VisHas,
	.Linked = CNodesWorldSup_Linked,
	.FreeGraph = CNodesWorldSup_FreeGraph,
	.FreeTemp = CNodesWorldSup_FreeTemp,
	.InList = CNodesWorldSup_InList,
	.ClearLists = CNodesWorld2D_ClearLists,
	.SaveInfPath = CNodesWorldSup_SaveInfPath,
};

cGVar GVar_ShowNodes2DAI;

cGVar GVar_AltPerpFloorAI;

cGVar GVar_AltMinTestWalkAI;

cGVar GVar_ShowInfoRegAI;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_AltMinTestWalkAI, &GVar_AltMinTestWalkAI, "AltMinTestWalkAI", 4, "Alt. min. to test walk.\n", 25.0f)
INIT_GVAR_INT(Init_GVar_ShowNodes2DAI, &GVar_ShowNodes2DAI, "ShowNodes2DAI", 4, "0 = No Show, 1 = Show Regions, 2 = Show Portals, 3 = Show Link's Portals, 4 = Show Portals + Links.", 0)
INIT_GVAR_FLOAT(Init_GVar_AltPerpFloorAI, &GVar_AltPerpFloorAI, "AltPerpFloorAI", 4, "Alt. máx. to get point perp.\n", 20000.0f)
INIT_GVAR_INT(Init_GVar_ShowInfoRegAI, &GVar_ShowInfoRegAI, "ShowInfoRegAI", 4, "Show info region when creation graph.\n", -1)
