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
#include "scrap/scrapworld.h"

#define NW2D_EMPTY ((const char *)Game_Ptr(0x3a3228))	/* the "" of the exe */

int crt_chmod(const char *path, int mode);	/* rva 0x345287 */

/* ---- Global/DList.h ---- */

void __fastcall NW2DRegion_dtor(cNW2DRegion *r) {
	DListVec3_Clear(&r->points);
	DList_FreePool(&r->points, sizeof(cNWVecNode));
}

void __thiscall NW2DRegionList_Add(cNW2DRegionList *list, const cNW2DRegion *r) {
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

void __thiscall NW2DRegionList_Release(cNW2DRegionList *list, cNW2DRegionNode *node) {
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

void __fastcall NW2DRegionList_Clear(cNW2DRegionList *list) {
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

void __fastcall NW2DRegionList_FreePool(cNW2DRegionList *list) {
	while (list->freeNodes) {
		cNW2DRegionNode *p = list->freeNodes;
		list->freeNodes = p->next;
		NW2DRegion_dtor(&p->r);
		operator_delete_sized(p, sizeof(*p));
	}
	list->numFree = 0;
}

/* ---- CNodesWorld2D ---- */

CNodesWorld2D *__thiscall CNodesWorld2D_ctor(void *mem) {
	CNodesWorld2D *self = mem;
	CNodesWorld *w = NW2D_SUP(self);

	CNodesWorldSup_ctor(w);
	w->vftable = Game_Ptr(CNODESWORLD2D_VFTABLE);
	memset(&self->regions, 0, sizeof(self->regions));
	w->maxVisDist = 5000.0f;
	w->f54 = 750.0f;
	w->f40 = 1500.0f;
	w->linkDist = 1500.0f;
	w->f5c = 1650.0f;
	return self;
}

void __fastcall CNodesWorld2D_dtor(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);

	w->vftable = Game_Ptr(CNODESWORLD2D_VFTABLE);
	CNodesWorldSup_FreeGraph(w);
	NW2DRegionList_Clear(&self->regions);
	NW2DRegionList_FreePool(&self->regions);
	CNodesWorldSup_dtor(w);
}

CNodesWorld2D *__thiscall CNodesWorld2D_Delete(CNodesWorld2D *self, byte flags) {
	CNodesWorld2D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte __thiscall CNodesWorld2D_LoadGraph(CNodesWorld2D *self) {
	char path[0x100];

	Con_Printf("Loading Graph 2D AI...\n");
	NW_LevelPath(path, NW2D_SUP(self), "/AI/Path2DAI");
	return CNodesWorldSup_LoadGraph(NW2D_SUP(self), path) != 0;
}

byte __thiscall CNodesWorld2D_SaveInfPath(CNodesWorld2D *self) {
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
	cFilePack_Write(f, &g, sizeof(g));
}

byte __thiscall CNodesWorld2D_SavePath(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);
	char path[0x100];
	cFilePack f;
	byte ok;

	Con_Printf("Saving Path AI...\n");
	NW_LevelPath(path, w, "/AI/Path2DAI.pth");
	crt_chmod(path, 0x180);
	cFilePack_ctor(&f);
	if (!cFilePack_Open(&f, path, 6)) {
		Con_Bluef("ERROR AI: File Path AI no abierto.\n");
		CHECK(!"CNodesWorld2D_SavePath_File_Path_Not_Open");
		ok = 0;
	} else {
		int n = w->points.count;
		cFilePack_Write(&f, &n, 4);
		cNWPointNode *p = w->points.iter = w->points.head;
		while (p) {
			NW2D_WriteGround(self, &f, &p->pos);
			p = w->points.iter = w->points.iter->next;
			if (p == w->points.head) {
				break;
			}
		}
		p = w->points.iter = w->points.head;
		while (p) {
			int links = p->links.count;
			cFilePack_Write(&f, &links, 4);
			cNWIntNode *l = p->links.iter = p->links.head;
			while (l) {
				NW2D_WriteGround(self, &f, l->data);
				l = p->links.iter = p->links.iter->next;
				if (l == p->links.head) {
					break;
				}
			}
			p = w->points.iter = w->points.iter->next;
			if (p == w->points.head) {
				break;
			}
		}
		if (!cFilePack_Close(&f)) {
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

void __thiscall CNodesWorld2D_ClearLists(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);

	DListVec3_Clear(&w->extraPoints);
	NWPointList_Clear(&w->points);
	NWVecList_Clear(&w->lists25c[0]);
	NWVecList_Clear(&w->lists25c[1]);
	for (int i = 0; i < 5; i++) {
		DListVec3_Clear(&w->lists28c[i]);
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
static int NW2D_Probe(const vec3_t *p, vec3_t *hit) {
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

byte __thiscall CNodesWorld2D_Ground(CNodesWorld2D *self, const vec3_t *p, vec3_t *hit, float height) {
	(void)self;
	if (!NW2D_Probe(p, hit)) {
		*hit = *p;
		return 0;
	}
	hit->y = hit->y + height;
	return 1;
}

byte __thiscall CNodesWorld2D_GroundSees(CNodesWorld2D *self, const vec3_t *p, const vec3_t *b, float height) {
	vec3_t hit;

	(void)self;
	if (!NW2D_Probe(p, &hit)) {
		return 0;
	}
	hit.y = hit.y + height;
	return Engine_TestLine(&hit, b, NULL, NULL) == 0;
}

byte __thiscall CNodesWorld2D_GroundLine(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, int u0, int u1,
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

byte __thiscall CNodesWorld2D_FitAt(CNodesWorld2D *self, float x, float y, float z, vec3_t *out, float down, float foot,
	float r, float h2, float h1) {
	vec3_t p = { x, y, z };

	(void)self;
	return NW2D_FitDown(&p, out, down, foot, r, h2, h1);
}

byte __thiscall CNodesWorld2D_Walk(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float up, float foot,
	float r, float h2, float h1) {
	vec3_t c = *a, dir, next, hit, d;
	float dx = b->x - a->x;
	float dz = b->z - a->z;
	float len = (float)__builtin_sqrt((double)((dx * dx + 0.0f) + dz * dz));

	(void)self;
	if (len == 0.0f) {
		dir.x = dx;
		dir.y = 0.0f;
		dir.z = dz;
	} else {
		float inv = 1.0f / len;
		dir.x = dx * inv;
		dir.y = inv * 0.0f;
		dir.z = dz * inv;
	}
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

byte __thiscall CNodesWorld2D_WalkFit(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float up, float foot,
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

byte __thiscall CNodesWorld2D_TestWalk(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float shrink) {
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

void __thiscall CNodesWorld2D_AnalizeGraph(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);
	byte bad;

	g_NWAnalizeOn = 1;
	g_NW2DDrawMode = 9;
	DListVec3_Clear(&w->errCollide);
	bad = 0;
	for (int i = 0; i < w->numNodes; i++) {
		for (int k = 0; k < w->nodes[i].numLinks; k++) {
			const cNWNode *n = &w->nodes[i];
			float h = g_NW2DHeight;
			float off = 221.445999f - h;
			vec3_t a = n->pos, b = n->links[k]->pos, ga, gb;
			a.y = off + a.y;
			b.y = off + b.y;
			if (!CNodesWorld2D_Ground(self, &a, &ga, h) || !CNodesWorld2D_Ground(self, &b, &gb, g_NW2DHeight) ||
				Engine_TestLine(&ga, &gb, NULL, NULL) || !CNodesWorld2D_TestWalk(self, &a, &b, 1.0f)) {
				NWVecList_Append(&w->errCollide, &w->nodes[i].pos);
				NWVecList_Append(&w->errCollide, &w->nodes[i].links[k]->pos);
				bad = 1;
			}
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. LINKS GRAFO COLISION ESC.: %d\n", w->errCollide.count / 4);
		Con_Bluef("GRAFO ERRONEO!\n");
	} else {
		Con_Printf("\nGRAFO NO COLISIONA CON GEOMETRIA\n");
	}
	DListVec3_Clear(&w->errUnlinked);
	bad = 0;
	for (int i = 0; i < w->numNodes; i++) {
		if (!w->nodes[i].numLinks) {
			NWVecList_Append(&w->errUnlinked, &w->nodes[i].pos);
			bad = 1;
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. NODOS NO CONECTADOS: %d\n", w->errUnlinked.count);
		Con_Bluef("GRAFO ERRONEO!\n");
	} else {
		Con_Printf("\nGRAFO CON NODOS CONECTADOS\n");
	}
	DListVec3_Clear(&w->errOneWay);
	bad = 0;
	for (int i = 0; i < w->numNodes; i++) {
		for (int k = 0; k < w->nodes[i].numLinks; k++) {
			const cNWNode *to = w->nodes[i].links[k];
			int j = 0;
			while (j < to->numLinks && to->links[j] != &w->nodes[i]) {
				j++;
			}
			if (j < to->numLinks) {
				continue;
			}
			NWVecList_Append(&w->errOneWay, &w->nodes[i].pos);
			NWVecList_Append(&w->errOneWay, &w->nodes[i].links[k]->pos);
			bad = 1;
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. LINKS NO CONMUTATIVOS: %d\n", w->errOneWay.count / 2);
		Con_Bluef("GRAFO ERRONEO!\n");
		Con_Bluef("%s", NW2D_EMPTY);
		return;
	}
	Con_Printf("\nGRAFO CON LINKS CONMUTATIVOS\n");
	Con_Bluef("%s", NW2D_EMPTY);
}

void __thiscall CNodesWorld2D_GetPairs(CNodesWorld2D *self, cNWPairList *list) {
	CNodesWorld *w = NW2D_SUP(self);

	Con_Printf("Getting Pairs Points Visibility No Linked...\n");
	cNWPointNode *saved = w->points.iter;
	cNWPointNode *pn = w->points.iter = w->points.head;
	while (pn) {
		const vec3_t *p = &pn->pos;
		cNWPointNode *cur = w->points.iter;
		cNWPointNode *qn = w->points.iter = cur->next;
		while (qn != w->points.head) {
			const vec3_t *q = &qn->pos;
			float dx = p->x - q->x, dy = p->y - q->y, dz = p->z - q->z;
			float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			if (w->f40 + w->f40 > d && !w->vftable->Linked(w, p, q)) {
				cNWPair pair;
				float ex = p->x - q->x, ez = p->z - q->z;
				pair.a = (int)(uintptr_t)p;
				pair.b = (int)(uintptr_t)q;
				pair.dist = ez * ez + ex * ex;
				w->vftable->InsertPair(w, list, &pair);
			}
			qn = w->points.iter = w->points.iter->next;
		}
		w->points.iter = cur;
		pn = w->points.iter = cur->next;
		if (pn == w->points.head) {
			break;
		}
	}
	w->points.iter = saved;
	Con_Printf("Num Pairs Points Visibility No Linked: %ld\n", (long)list->count);
}

void __thiscall CNodesWorld2D_LinkExtra(CNodesWorld2D *self) {
	CNodesWorld *w = NW2D_SUP(self);

	Con_Printf("Linking Points of Visibility Extra...\n");
	cNWVecNode *esaved = w->extraPoints.iter;
	cNWVecNode *en = w->extraPoints.iter = w->extraPoints.head;
	while (en) {
		/* the data of a cNWPointNode: the position, the links and the box zeroed */
		cNWPointNode tmp;
		memset(&tmp, 0, sizeof(tmp));
		tmp.pos = en->v;
		NWPointList_Add(&w->points, &tmp.pos);
		cNWPointNode *saved = w->points.iter;
		cNWPointNode *pn = w->points.iter = w->points.head;
		const vec3_t *p = pn ? &pn->pos : NULL;
		cNWPointNode *qn = w->points.iter = w->points.iter->next;
		while (qn != w->points.head) {
			const vec3_t *q = &qn->pos;
			vec3_t gp, gq;
			float dx = p->x - q->x, dy = p->y - q->y, dz = p->z - q->z;
			float d = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
			if (w->linkDist > d && CNodesWorld2D_Ground(self, p, &gp, g_NW2DHeight) &&
				CNodesWorld2D_Ground(self, q, &gq, g_NW2DHeight) && !Engine_TestLine(&gp, &gq, NULL, NULL) &&
				CNodesWorld2D_TestWalk(self, p, q, 0.0f)) {
				NWIntList_Add(&NW_POINT(p)->links, (void *const *)&q);
				NWIntList_AddKeepIter(&NW_POINT(q)->links, p);
				w->numPointLinks++;
			}
			qn = w->points.iter = w->points.iter->next;
		}
		w->points.iter = saved;
		en = w->extraPoints.iter = w->extraPoints.iter->next;
		if (en == w->extraPoints.head) {
			en = NULL;
		}
		NWIntList_Free(&tmp.links);
	}
	w->extraPoints.iter = esaved;
}

void __thiscall CNodesWorld2D_AddExtraPoints(CNodesWorld2D *self) {
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
		NWVecList_Add(&w->extraPoints, &p);
	}
}

float __thiscall CNodesWorld2D_NearestVisDist(CNodesWorld2D *self, const vec3_t *pos) {
	CNodesWorld *w = NW2D_SUP(self);
	float best = 3.4028234663852886e+38f;
	int i = w->vftable->BufferIndex(w, pos);
	cNWVis *vis = w->buffer[i].vis;

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
		vis = w->buffer[i].vis;
		it = vis->points.iter = vis->points.iter->next;
		if (it == vis->points.head) {
			break;
		}
	}
	w->buffer[i].vis->points.iter = saved;
	return best;
}

void __thiscall CNodesWorld2D_CollectCell(CNodesWorld2D *self, const vec3_t *pos, cNWIntList *cell,
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
		if (w->maxVisDist * w->maxVisDist > d2 && d2 > distMin * distMin && (!useMax || distMax * distMax > d2)) {
			int add;
			if (mode == 0) {
				add = CNodesWorld2D_TestPath(self, pos, &q, r, r, h1, h2, 0);
			} else if (mode == 1) {
				add = CNodesWorld2D_GroundSees(self, &q, pos, h1);
			} else if (mode == 2 && !g_IsMapOutDoor) {
				int f = Floor_Get(g_pWorld->floor, pos->y);
				add = f == Floor_Get(g_pWorld->floor, q.y);
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

	w->grid->vftable->GetCell(w->grid, x, y, z, &cell);
	CNodesWorld2D_CollectCell(s->self, s->pos, cell, s->list, s->max, s->count, s->r, s->h1, s->h2, s->distMin,
		s->distMax, s->mode, s->soft == 0);
	return *s->count >= s->max || (*s->count > 0 && s->soft && e >= s->distMax);
}

/* a side of the ring box moved out one cell: 0 the side stops, 1 it grows, 2 the search ends */
static int NW2DRings_Grow(const nw2dRings *s, int inGrid, float e) {
	const CNodesWorld *w = NW2D_SUP(s->self);

	if (!inGrid || !(w->maxVisDist > e) || (!s->soft && !(s->distMax > e))) {
		return 0;
	}
	if (*s->count > 0 && s->soft && e >= s->distMax) {
		return 2;
	}
	return 1;
}

void __thiscall CNodesWorld2D_SearchRings(CNodesWorld2D *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float r, float h1, float h2, float distMin, float distMax, byte soft, int mode) {
	CNodesWorld *w = NW2D_SUP(self);
	nw2dRings s = { self, pos, list, max, count, r, h1, h2, distMin, distMax, soft, mode };
	int c[3], n[3], st[3], x, y, z;
	vec3_t cs, dir;
	byte stopXhi = 0, stopXlo = 0, stopYhi = 0, stopYlo = 0, stopZhi = 0, stopZlo = 0;

	CHECK(distMin <= distMax);
	w->grid->vftable->CellCoords(w->grid, pos, &c[0], &c[1], &c[2]);
	w->grid->vftable->GetDivs(w->grid, &n[0], &n[1], &n[2]);
	w->grid->vftable->GetCellSize(w->grid, &cs);
	{
		float dx = (pos->x + 1.0f) - pos->x, dy = (pos->y + 1.0f) - pos->y, dz = (pos->z + 1.0f) - pos->z;
		float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (len == 0.0f) {
			dir.x = dx;
			dir.y = dy;
			dir.z = dz;
		} else {
			float inv = 1.0f / len;
			dir.x = dx * inv;
			dir.y = dy * inv;
			dir.z = dz * inv;
		}
	}
	/* the start box: from the cell of pos to the cell of the point distMin along the diagonal */
	if (distMin > 0.0f) {
		st[0] = (int)(((pos->x + dir.x * distMin) - w->min.x) / cs.x);
		st[1] = (int)(((pos->y + dir.y * distMin) - w->min.y) / cs.y);
		st[2] = (int)(((pos->z + dir.z * distMin) - w->min.z) / cs.z);
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

void __thiscall CNodesWorld2D_GetPointsVis(CNodesWorld2D *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float r, float h1, float h2, float distMin, float distMax, byte soft, int mode) {
	CNodesWorld *w = NW2D_SUP(self);
	vec3_t tmp;

	CHECK(distMin <= distMax);
	*count = 0;
	const vec3_t *cs = w->grid->vftable->GetCellSize(w->grid, &tmp);
	if (cs->x * cs->x + cs->y * cs->y + cs->z * cs->z >= distMin * distMin) {
		cNWIntList *cell;
		w->grid->vftable->GetCellAt(w->grid, pos, &cell);
		CNodesWorld2D_CollectCell(self, pos, cell, list, max, count, r, h1, h2, distMin, distMax, mode, soft == 0);
	}
	if (*count < max) {
		CNodesWorld2D_SearchRings(self, pos, list, max, count, r, h1, h2, distMin, distMax, soft, mode);
	}
}

byte __thiscall CNodesWorld2D_HasPointVis(CNodesWorld2D *self, const vec3_t *pos, float r, float h1, float h2) {
	const vec3_t *point;
	int count;

	CNodesWorld2D_GetPointsVis(self, pos, &point, 1, &count, r, h1, h2, 0.0f, 2500.0f, 1, 0);
	return count > 0;
}

void __thiscall CNodesWorld2D_NearestPointVis(CNodesWorld2D *self, const vec3_t *pos, vec3_t *out) {
	CNodesWorld *w = NW2D_SUP(self);
	const vec3_t *list[7];
	int count;

	CNodesWorld2D_GetPointsVis(self, pos, list, 7, &count, 0.0f, 0.0f, 0.0f, 0.0f, w->maxVisDist, 1, 2);
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

byte __thiscall CNodesWorld2D_BuildPath(CNodesWorld2D *self, cNWVecList *path, int depth, const vec3_t *dir,
	const vec3_t *start, byte direct, cNWNode **node, float r, float h1, float h2, byte flat) {
	CNodesWorld *w = NW2D_SUP(self);
	cNWNode *cand[7], *list[20], *cur;
	/* one stack slot of the original: the count of the search, its loop index, then the chosen node */
	int slot = 0;
	int ncand;
	vec3_t s, d, e, p;

	CHECK(depth <= 20);
	DListVec3_Clear(path);
	s = *start;
	if (direct && CNodesWorld2D_TestPath(self, &s, &(*node)->pos, r, r, h1, h2, 0)) {
		cur = *node;
		cand[0] = cur;
		ncand = 1;
	} else {
		CNodesWorld2D_GetPointsVis(self, &s, (const vec3_t **)cand, 7, &slot, r, h1, h2, 0.0f, w->f5c, 1, 0);
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
			if (w->vftable->InList(w, list, n, cur->links[j])) {
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
				slot = (int)(intptr_t)v;
				near = 1;
				break;
			}
			if (best > ang) {
				slot = (int)(intptr_t)v;
				best = ang;
			}
		}
		if (!near && inList) {
			break;
		}
		cNWNode *next = (cNWNode *)(intptr_t)slot;
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
	w->axis[0].x = w->nodeSize.x;
	w->axis[0].y = 0.0f;
	w->axis[0].z = 0.0f;
	w->axis[1].y = w->nodeSize.y;
	w->axis[1].x = 0.0f;
	w->axis[1].z = 0.0f;
	w->axis[2].z = w->nodeSize.z;
	w->axis[2].x = 0.0f;
	w->axis[2].y = 0.0f;
	w->halfNode.y = w->nodeSize.y * 0.5f;
	w->halfNode.z = w->nodeSize.z * 0.5f;
	w->halfNode.x = w->nodeSize.x * 0.5f;
}

void __thiscall CNodesWorld2D_Load(CNodesWorld2D *self, int arg, const char *name, const vec3_t *pos) {
	static const cNW2DBody bodies[3] = {
		{ 155.0f, 221.445999f, 189.220001f, 40.0f },
		{ 35.0f, 52.0480003f, 52.0480003f, 12.0f },
		{ 70.0f, 88.8450012f, 88.8450012f, 20.0f },
	};
	CNodesWorld *w = NW2D_SUP(self);
	box_t box;

	Con_Printf("\nCreating Nodes...\n");
	Con_Bluef("%s", NW2D_EMPTY);
	memcpy(g_NW2DBodies, bodies, sizeof(bodies));
	w->nodeSize = *pos;
	NW2D_SetNodeSize(w);
	w->f44.z = w->nodeSize.z * 0.1f;
	w->f44.x = w->nodeSize.x * 0.1f;
	w->f44.y = w->nodeSize.y * 0.1f;
	w->margin = Float_MinSS(w->nodeSize.z, Float_MinSS(w->nodeSize.y, w->nodeSize.x)) / 100.0f;
	World_GetCollideBox(&box);
	w->min = box.min;
	w->max = box.max;
	if (*name) {
		size_t n = strnlen(name, sizeof(w->name) - 1);
		memcpy(w->name, name, n);
		w->name[n] = 0;
	}
	w->vftable->FreeGraph(w);
	if (arg < 0) {
		if (arg != -1) {
			goto end;
		}
		if (!w->vftable->Create(w)) {
			Con_Warnf("NODES 2D AI LEVEL NOT LOADED.\n\n");
			w->nodeSize.x = 155.0f;
			w->nodeSize.y = 155.0f;
			w->nodeSize.z = 155.0f;
			NW2D_SetNodeSize(w);
			goto end;
		}
		w->vftable->BuildGrid(w);
		Con_Printf("NODES 2D AI LEVEL LOADED.\n\n");
		Con_Bluef("%s", NW2D_EMPTY);
		goto end;
	}
	if (!(Float_MinSS(pos->z, Float_MinSS(pos->y, pos->x)) > 10.0f) ||
		!(10000.0f > Float_MaxSS(pos->z, Float_MaxSS(pos->y, pos->x)))) {
		CHECK(!"(MinComponent(vectInterNod) > 10.0f) && (MaxComponent(vectInterNod) < 10000.0f)");
	}
	w->created = 0;
	if (arg == 0) {
		char buf[20];
		float ang[3];
		byte flag;

		DummySearch_Start("DM_NodeIniAI", 0);
		if (!DummySearch_Next(buf, sizeof(buf), &w->nodeInit, ang, &flag)) {
			Con_Bluef("ERROR AI: Dummy DM_NodeInitAI no encontrado.\n");
			CHECK(!"CNodesWorld2D_IniNodesWorld_DM_NodeInitAI_Not_Found");
		}
		w->nodeInitDummy = 1;
	} else {
		NW_CamPos(&w->nodeInit.x, &w->nodeInit.y, &w->nodeInit.z);
		w->nodeInitDummy = 0;
	}
	CHECK(w->nodeInit.x >= w->min.x && w->max.x >= w->nodeInit.x && w->nodeInit.y >= w->min.y &&
		w->max.y >= w->nodeInit.y && w->nodeInit.z >= w->min.z && w->max.z >= w->nodeInit.z);
	if (arg <= 0) {
		w->min.x = (w->min.x - w->nodeSize.x) - w->margin;
		w->min.y = (w->min.y - w->nodeSize.y) - w->margin;
		w->min.z = (w->min.z - w->nodeSize.z) - w->margin;
		w->max.x = (w->nodeSize.x + w->max.x) + w->margin;
		w->max.y = (w->max.y + w->nodeSize.y) + w->margin;
		w->max.z = (w->max.z + w->nodeSize.z) + w->margin;
	} else {
		float fa = (float)arg;
		float ex = ((w->halfNode.x + w->halfNode.x) * fa + w->halfNode.x) + w->margin;
		float ey = (w->halfNode.y + (w->halfNode.y + w->halfNode.y) * fa) + w->margin;
		float ez = (w->halfNode.z + (w->halfNode.z + w->halfNode.z) * fa) + w->margin;
		w->min.z = w->nodeInit.z - ez;
		w->min.x = w->nodeInit.x - ex;
		w->min.y = w->nodeInit.y - ey;
		w->max.y = w->nodeInit.y + ey;
		w->max.x = ex + w->nodeInit.x;
		w->max.z = w->nodeInit.z + ez;
	}
	if (arg == 0) {
		CNodesWorld2D_AddExtraPoints(self);
	}
	byte made = CNodesWorld2D_Generate(self, &w->nodeInit, &w->min, &w->max);
	Con_Printf("\nNUM. POINTS OF VISIBILITY GENERATED 2D: %d\n", w->points.count - w->extraPoints.count);
	Con_Printf("NUM. POINTS OF VISIBILITY EXTRA MAP 2D: %d\n", w->extraPoints.count);
	Con_Printf("NUM. POINTS OF VISIBILITY TOTAL 2D: %d\n", w->points.count);
	Con_Printf("NUM. LINKS BETWEEN POINTS VIS. 2D: %d\n", w->numPointLinks);
	Con_Bluef("%s", NW2D_EMPTY);
	if (made) {
		CNodesWorld2D_SaveInfPath(self);
		w->vftable->FreeTemp(w);
	}
	if (!w->vftable->Create(w)) {
		Con_Bluef("NODES 2D AI LEVEL NOT LOADED.\n\n");
		goto end;
	}
	if (arg == 0) {
		w->vftable->BuildGrid(w);
	}
	w->created = 1;
	Con_Printf("NODES 2D AI LEVEL LOADED.\n");
	Con_Bluef("%s", NW2D_EMPTY);
end:
	Con_Bluef("%s", NW2D_EMPTY);
}
