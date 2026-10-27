/*
 * The 3D visibility graph of a level, original source AI/NodesWorld.cpp with the instances of Global/DList.h it
 * uses (include/scrap/nodesworld.h). The base class CNodesWorldSup is another source.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/console.h"
#include "scrap/filepak.h"
#include "scrap/mathlib.h"
#include "scrap/fxmisc.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"

#define NW_EMPTY ((const char *)Game_Ptr(0x3a3228))	/* the "" of the exe */

int crt_chmod(const char *path, int mode);	/* rva 0x345287 */

/* ---- Global/DList.h ---- */

void __fastcall NWIntList_Clear(cNWIntList *list) {
	DList_Clear(list, sizeof(cNWIntNode));
}

void __fastcall NWVecList_Clear(cNWVecList *list) {
	DList_Clear(list, sizeof(cNWVecNode));
}

void __fastcall NWTriList_Clear(cNWTriList *list) {
	DList_Clear(list, sizeof(cNWTriNode));
}

void __fastcall NWIntList_Free(cNWIntList *list) {
	NWIntList_Clear(list);
	DList_FreePool(list, sizeof(cNWIntNode));
}

void __fastcall NWVecList_Free(cNWVecList *list) {
	NWVecList_Clear(list);
	DList_FreePool(list, sizeof(cNWVecNode));
}

void __fastcall NWIntList_FreeAt0c(void *owner) {
	NWIntList_Free((cNWIntList *)((byte *)owner + 0xc));
}

void __thiscall NWIntList_Add(cNWIntList *list, void *const *data) {
	cNWIntNode *node = DList_NewNode(list, sizeof(*node));
	node->data = *data;
	DList_LinkFirst(list, node);
}

void __thiscall NWVecList_Add(cNWVecList *list, const vec3_t *v) {
	cNWVecNode *node = DList_NewNode(list, sizeof(*node));
	node->v = *v;
	DList_LinkFirst(list, node);
}

void __thiscall NWTriList_Add(cNWTriList *list, const float *data) {
	cNWTriNode *node = DList_NewNode(list, sizeof(*node));
	memcpy(node->data, data, sizeof(node->data));
	DList_LinkFirst(list, node);
}

cNWPointNode *__fastcall NWPointList_NewNode(cNWPointList *list) {
	cNWPointNode *node = list->freeNodes;

	if (node) {
		list->numFree--;
		list->freeNodes = node->next;
		return node;
	}
	node = operator_new(sizeof(*node));
	if (!node) {
		return NULL;
	}
	memset(&node->links, 0, sizeof(*node) - offsetof(cNWPointNode, links));
	return node;
}

void __thiscall NWPointList_Add(cNWPointList *list, const void *data) {
	list->iter = NULL;
	cNWPointNode *node = NWPointList_NewNode(list);
	memcpy(&node->pos, data, sizeof(*node) - offsetof(cNWPointNode, pos));
	DList_LinkFirst(list, node);
}

void __thiscall NWIntList_AddBefore(cNWIntList *list, void *const *data) {
	cNWIntNode *head = list->head;

	if (list->iter == head) {
		NWIntList_Add(list, data);
		return;
	}
	CHECK(head != NULL);
	CHECK(list->iter != NULL);
	list->head = list->iter;
	NWIntList_Add(list, data);
	list->head = head;
}

void __thiscall NWSegList_Append(cNWSegList *list, const vec3_t *seg) {
	cNWSegNode *node = DList_NewNode(list, sizeof(*node));

	node->a = seg[0];
	node->b = seg[1];
	if (!list->head) {
		node->next = node;
		node->prev = node;
		list->count++;
		list->head = node;
		return;
	}
	node->prev = list->head->prev;
	node->next = list->head;
	list->head->prev->next = node;
	list->head->prev = node;
	list->count++;
}

void __fastcall NWIntList_RemoveHead(cNWIntList *list) {
	DList_RemoveHead(list, sizeof(cNWIntNode));
}

void __fastcall NWVecList_RemoveHead(cNWVecList *list) {
	DList_RemoveHead(list, sizeof(cNWVecNode));
}

void __thiscall NWPointList_Release(cNWPointList *list, cNWPointNode *node) {
	if (list->numFree < list->maxFree) {
		node->next = list->freeNodes;
		list->numFree++;
		list->freeNodes = node;
		return;
	}
	if (node) {
		NWIntList_Free(&node->links);
		operator_delete_sized(node, sizeof(*node));
	}
}

void __fastcall NWPointList_Clear(cNWPointList *list) {
	while (list->head) {
		cNWPointNode *p = list->head;
		list->iter = NULL;
		if (p == p->next) {
			NWPointList_Release(list, p);
			list->head = NULL;
		} else {
			list->head = p->next;
			p->prev->next = p->next;
			p->next->prev = p->prev;
			NWPointList_Release(list, p);
		}
		list->count--;
	}
}

void __fastcall NWBox_Init(cNWBox *box) {
	box->a.x = 3.4028234663852886e+38f;
	box->a.y = 3.4028234663852886e+38f;
	box->a.z = 3.4028234663852886e+38f;
	box->b.x = -3.4028234663852886e+38f;
	box->b.y = -3.4028234663852886e+38f;
	box->b.z = -3.4028234663852886e+38f;
}

/* ---- CNodesWorld ---- */

CNodesWorld *__thiscall CNodesWorld_ctor(void *mem) {
	CNodesWorld *self = mem;

	CNodesWorldSup_ctor(self);
	self->vftable = Game_Ptr(CNODESWORLD_VFTABLE);
	memset(&self->tris, 0, sizeof(self->tris));
	self->file = NULL;
	self->analizing = 0;
	if (g_IsMapOutDoor) {
		g_NWNodeRadius = 751.0f;
		g_NWF60 = 4000.0f;
		self->maxVisDist = 200000.0f;
		self->f54 = 12500.0f;
		self->f40 = 25000.0f;
		self->linkDist = 50000.0f;
		self->f5c = 27500.0f;
		return self;
	}
	g_NWNodeRadius = 131.0f;
	g_NWF60 = 130.0f;
	self->maxVisDist = 10000.0f;
	self->f54 = 750.0f;
	self->f40 = 1500.0f;
	self->linkDist = 1500.0f;
	self->f5c = 1650.0f;
	return self;
}

void __fastcall CNodesWorld_dtor(CNodesWorld *self) {
	self->vftable = Game_Ptr(CNODESWORLD_VFTABLE);
	CNodesWorldSup_FreeGraph(self);
	NWTriList_Clear(&self->tris);
	DList_FreePool(&self->tris, sizeof(cNWTriNode));
	CNodesWorldSup_dtor(self);
}

CNodesWorld *__thiscall CNodesWorld_Delete(CNodesWorld *self, byte flags) {
	CNodesWorld_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void NW_LevelPath(char *path, const CNodesWorld *self, const char *file) {
	size_t n = strnlen(self->name, sizeof(self->name));
	size_t f = strlen(file);

	if (n + f > 0xff) {
		f = 0xff - n;
	}
	memcpy(path, self->name, n);
	memcpy(path + n, file, f);
	path[n + f] = 0;
}

byte __thiscall CNodesWorld_LoadGraph(CNodesWorld *self) {
	char path[0x100];

	Con_Printf("Loading Graph 3D AI...\n");
	NW_LevelPath(path, self, "/AI/PathAI");
	return CNodesWorldSup_LoadGraph(self, path) != 0;
}

byte __thiscall CNodesWorld_SaveInfPath(CNodesWorld *self) {
	char path[0x100];

	NW_LevelPath(path, self, "/AI/InfPathAI.pth");
	if (!CNodesWorldSup_SaveInfPath(self, path)) {
		CHECK(!"CNodesWorld_SavePath_File_InfPathAI_Not_Open");
		return 0;
	}
	return 1;
}

byte __thiscall CNodesWorld_SavePath(CNodesWorld *self) {
	char path[0x100];
	cFilePack f;
	byte ok;

	Con_Printf("Saving Path AI...\n");
	NW_LevelPath(path, self, "/AI/PathAI.pth");
	crt_chmod(path, 0x180);
	cFilePack_ctor(&f);
	if (!cFilePack_Open(&f, path, 6)) {
		Con_Bluef("ERROR AI: File Path AI no abierto.\n");
		CHECK(!"CNodesWorld_SavePath_File_Path_Not_Open");
		ok = 0;
	} else {
		int n = self->points.count;
		cFilePack_Write(&f, &n, 4);
		cNWPointNode *p = self->points.iter = self->points.head;
		while (p) {
			cFilePack_Write(&f, &p->pos, sizeof(p->pos));
			p = self->points.iter = self->points.iter->next;
			if (p == self->points.head) {
				break;
			}
		}
		p = self->points.iter = self->points.head;
		while (p) {
			int links = p->links.count;
			cFilePack_Write(&f, &links, 4);
			cNWIntNode *l = p->links.iter = p->links.head;
			while (l) {
				cFilePack_Write(&f, l->data, sizeof(vec3_t));
				l = p->links.iter = p->links.iter->next;
				if (l == p->links.head) {
					break;
				}
			}
			p = self->points.iter = self->points.iter->next;
			if (p == self->points.head) {
				break;
			}
		}
		if (!cFilePack_Close(&f)) {
			Con_Bluef("ERROR AI: File Path AI no cerrado.\n");
			CHECK(!"CNodesWorld_SavePath_File_Path_Not_Closed");
			ok = 0;
		} else {
			ok = 1;
		}
	}
	cFilePack_dtor(&f);
	return ok;
}

void __thiscall CNodesWorld_ClearLists(CNodesWorld *self) {
	DListVec3_Clear(&self->extraPoints);
	NWPointList_Clear(&self->points);
	NWVecList_Clear(&self->lists25c[0]);
	NWVecList_Clear(&self->lists25c[1]);
	for (int i = 0; i < 5; i++) {
		DListVec3_Clear(&self->lists28c[i]);
	}
}

float NW_Normalize(vec3_t *out, float x, float y, float z) {
	float len = (float)__builtin_sqrt((double)(y * y + x * x + z * z));

	if (len == 0.0f) {
		out->x = x;
		out->y = y;
		out->z = z;
	} else {
		float inv = 1.0f / len;
		out->x = x * inv;
		out->y = y * inv;
		out->z = z * inv;
	}
	return len;
}

void NW_DebugSeg(cNWSegList *debug, const vec3_t *a, const vec3_t *b) {
	if (g_NWDebugSegs && debug) {
		vec3_t seg[2] = { *a, *b };
		NWSegList_Append(debug, seg);
	}
}

int NW_PathSide(const vec3_t *a, const vec3_t *side, float r, const vec3_t *dl, cNWSegList *debug) {
	vec3_t p, q;

	p.x = side->x * r + a->x;
	p.y = side->y * r + a->y;
	p.z = side->z * r + a->z;
	q.x = dl->x + p.x;
	q.y = dl->y + p.y;
	q.z = dl->z + p.z;
	if (Engine_TestLine(&p, &q, NULL, NULL)) {
		return 0;
	}
	NW_DebugSeg(debug, &p, &q);
	return 1;
}

static void NW_Neg(vec3_t *out, const vec3_t *v) {
	out->x = -v->x;
	out->y = -v->y;
	out->z = -v->z;
}

/* the unit normal of dir x (0 1 0), (1 0 0) for a vertical dir; the products with 0 kept */
static void NW_SideNormal(vec3_t *out, const vec3_t *dir) {
	vec3_t p;

	p.x = dir->y * 0.0f - dir->z;
	p.y = dir->z * 0.0f - dir->x * 0.0f;
	p.z = dir->x - dir->y * 0.0f;
	if (p.x == 0.0f && p.y == 0.0f && p.z == 0.0f) {
		p.x = 1.0f;
		p.y = 0.0f;
		p.z = 0.0f;
	}
	NW_Normalize(out, p.x, p.y, p.z);
}

/* d x v */
static void NW_Cross(vec3_t *out, const vec3_t *d, const vec3_t *v) {
	out->x = v->z * d->y - v->y * d->z;
	out->y = v->x * d->z - v->z * d->x;
	out->z = v->y * d->x - v->x * d->y;
}

byte __thiscall CNodesWorld_TestPath(CNodesWorld *self, const vec3_t *a, const vec3_t *b, float r, float r2,
	cNWSegList *debug, int mode) {
	vec3_t dir, n, nn, c, m, mm, s, ns, t, nt, dl;

	(void)self;
	(void)r2;
	if (Engine_TestLine(a, b, NULL, NULL)) {
		return 0;
	}
	NW_DebugSeg(debug, a, b);
	float len = NW_Normalize(&dir, b->x - a->x, b->y - a->y, b->z - a->z);
	NW_SideNormal(&n, &dir);
	dl.x = dir.x * len;
	dl.y = dir.y * len;
	dl.z = dir.z * len;
	if (!NW_PathSide(a, &n, r, &dl, debug)) {
		return 0;
	}
	NW_Neg(&nn, &n);
	if (!NW_PathSide(a, &nn, r, &dl, debug)) {
		return 0;
	}
	c.x = nn.z * dir.y - nn.y * dir.z;
	c.y = nn.x * dir.z - nn.z * dir.x;
	c.z = nn.y * dir.x - nn.x * dir.y;
	m = *Vec3_NormalizeTo(&n, &c);
	if (!NW_PathSide(a, &m, r, &dl, debug)) {
		return 0;
	}
	NW_Neg(&mm, &m);
	if (mode != 1 && !NW_PathSide(a, &mm, r, &dl, debug)) {
		return 0;
	}
	c.x = mm.x + nn.x;
	c.y = mm.y + nn.y;
	c.z = mm.z + nn.z;
	Vec3_NormalizeTo(&s, &c);
	if (!NW_PathSide(a, &s, r, &dl, debug)) {
		return 0;
	}
	NW_Neg(&ns, &s);
	if (!NW_PathSide(a, &ns, r, &dl, debug)) {
		return 0;
	}
	c.x = nn.x - mm.x;
	c.y = nn.y - mm.y;
	c.z = nn.z - mm.z;
	t = *Vec3_NormalizeTo(&s, &c);
	if (!NW_PathSide(a, &t, r, &dl, debug)) {
		return 0;
	}
	NW_Neg(&nt, &t);
	if (!NW_PathSide(a, &nt, r, &dl, debug)) {
		return 0;
	}
	return 1;
}

int NW_CornerSide(const vec3_t *b, const vec3_t *u, const vec3_t *v, float r, cNWSegList *debug) {
	vec3_t p, q;

	p.x = u->x * r + b->x;
	p.y = u->y * r + b->y;
	p.z = u->z * r + b->z;
	q.x = v->x * r + b->x;
	q.y = v->y * r + b->y;
	q.z = v->z * r + b->z;
	if (Engine_TestLine(&p, &q, NULL, NULL)) {
		return 0;
	}
	NW_DebugSeg(debug, &p, &q);
	return 1;
}

byte __thiscall CNodesWorld_TestCorner(CNodesWorld *self, const vec3_t *a, const vec3_t *b, const vec3_t *c, float r,
	float r2, byte flat, cNWSegList *debug) {
	vec3_t d1, d0, n1, n0, nn1, nn0, v, m1, m0, s1, s0, t1, t0;

	(void)self;
	(void)r2;
	NW_Normalize(&d1, c->x - b->x, c->y - b->y, c->z - b->z);
	NW_Normalize(&d0, b->x - a->x, b->y - a->y, b->z - a->z);
	NW_SideNormal(&n1, &d1);
	NW_SideNormal(&n0, &d0);
	if (!flat) {
		NW_Neg(&nn1, &n1);
		NW_Neg(&nn0, &n0);
		if (!NW_CornerSide(b, &nn1, &nn0, r, debug)) {
			return 0;
		}
		NW_Cross(&v, &d1, &nn1);
		m1 = *Vec3_NormalizeTo(&s1, &v);
		NW_Cross(&v, &d0, &nn0);
		m0 = *Vec3_NormalizeTo(&s1, &v);
		v.x = m1.x + nn1.x;
		v.y = m1.y + nn1.y;
		v.z = m1.z + nn1.z;
		Vec3_NormalizeTo(&s1, &v);
		v.x = m0.x + nn0.x;
		v.y = m0.y + nn0.y;
		v.z = m0.z + nn0.z;
		Vec3_NormalizeTo(&s0, &v);
		if (!NW_CornerSide(b, &s1, &s0, r, debug)) {
			return 0;
		}
		v.x = nn1.x - m1.x;
		v.y = nn1.y - m1.y;
		v.z = nn1.z - m1.z;
		t1 = *Vec3_NormalizeTo(&s1, &v);
		v.x = nn0.x - m0.x;
		v.y = nn0.y - m0.y;
		v.z = nn0.z - m0.z;
		t0 = *Vec3_NormalizeTo(&s0, &v);
		return NW_CornerSide(b, &t1, &t0, r, debug);
	}
	if (!NW_CornerSide(b, &n1, &n0, r, debug)) {
		return 0;
	}
	NW_Cross(&v, &d1, &n1);
	NW_Normalize(&m1, v.x, v.y, v.z);
	NW_Cross(&v, &d0, &n0);
	NW_Normalize(&m0, v.x, v.y, v.z);
	NW_Normalize(&s1, m1.x + n1.x, m1.y + n1.y, m1.z + n1.z);
	NW_Normalize(&s0, m0.x + n0.x, m0.y + n0.y, m0.z + n0.z);
	if (!NW_CornerSide(b, &s1, &s0, r, debug)) {
		return 0;
	}
	v.x = n1.x - m1.x;
	v.y = n1.y - m1.y;
	v.z = n1.z - m1.z;
	t1 = *Vec3_NormalizeTo(&s1, &v);
	v.x = n0.x - m0.x;
	v.y = n0.y - m0.y;
	v.z = n0.z - m0.z;
	t0 = *Vec3_NormalizeTo(&s0, &v);
	return NW_CornerSide(b, &t1, &t0, r, debug);
}

byte __thiscall CNodesWorld_BufferPosSeen(CNodesWorld *self, const vec3_t *from, int index) {
	vec3_t p;

	return Engine_TestLine(from, self->vftable->BufferPos(self, &p, index), NULL, NULL) == 0;
}

byte __thiscall CNodesWorld_TestSurface(CNodesWorld *self, const vec3_t *a, const vec3_t *b, vec3_t *hit,
	CColisTri *tri) {
	vec3_t n, v;

	if (!Engine_TestLine(a, b, hit, tri)) {
		float dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
		if (g_IsMapOutDoor && dx == 0.0f && dz == 0.0f && dy > 0.0f) {
			return 0;
		}
		*hit = *b;
		tri->hit.x = 3.4028234663852886e+38f;
		return 1;
	}
	if (tri->noColisiones & 1) {
		return 0;
	}
	float ax = a->x;
	float dx = b->x - ax, dy = b->y - a->y, dz = b->z - a->z;
	NW_Normalize(&n, tri->plane.n.x, tri->plane.n.y, tri->plane.n.z);
	NW_Normalize(&v, -dx, -dy, -dz);
	if (!(v.x * n.x + v.y * n.y + v.z * n.z <= -0.1f)) {
		return 0;
	}
	if (!self->b3b0) {
		self->hitFrom.x = ax;
		self->hitFrom.y = a->y;
		self->hitFrom.z = a->z;
		self->hitTo = *hit;
	}
	return 1;
}

byte __thiscall CNodesWorld_TestSurfaceAround(CNodesWorld *self, const vec3_t *pos, vec3_t *hit, CColisTri *tri) {
	float fx = (float)__builtin_fabs((double)(self->max.x - self->min.x));
	float fy = (float)__builtin_fabs((double)(self->max.y - self->min.y));
	float fz = (float)__builtin_fabs((double)(self->max.z - self->min.z));
	float l = Float_MaxSS(fz, Float_MaxSS(fy, fx));
	float nl = -l;
	vec3_t p;

	p.x = pos->x + 0.0f;
	p.y = pos->y + l;
	p.z = pos->z + 0.0f;
	if (CNodesWorld_TestSurface(self, pos, &p, hit, tri)) {
		return 1;
	}
	p.x = pos->x + 0.0f;
	p.y = nl + pos->y;
	p.z = pos->z + 0.0f;
	if (CNodesWorld_TestSurface(self, pos, &p, hit, tri)) {
		return 1;
	}
	p.x = pos->x + l;
	p.y = pos->y + 0.0f;
	p.z = pos->z + 0.0f;
	if (CNodesWorld_TestSurface(self, pos, &p, hit, tri)) {
		return 1;
	}
	p.x = pos->x + nl;
	p.y = pos->y + 0.0f;
	p.z = pos->z + 0.0f;
	if (CNodesWorld_TestSurface(self, pos, &p, hit, tri)) {
		return 1;
	}
	p.x = pos->x + 0.0f;
	p.y = pos->y + 0.0f;
	p.z = pos->z + l;
	if (CNodesWorld_TestSurface(self, pos, &p, hit, tri)) {
		return 1;
	}
	p.x = pos->x + 0.0f;
	p.y = pos->y + 0.0f;
	p.z = nl + pos->z;
	return CNodesWorld_TestSurface(self, pos, &p, hit, tri) != 0;
}

void __thiscall CNodesWorld_CollectCell(CNodesWorld *self, const vec3_t *pos, cNWIntList *cell, const vec3_t **list,
	int max, int *count, float radius, float distMin, float distMax, int mode, byte useMax) {
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
		if (self->maxVisDist * self->maxVisDist > d2 && d2 > distMin * distMin &&
			(!useMax || distMax * distMax > d2)) {
			int add;
			if (mode == 0) {
				add = CNodesWorld_TestPath(self, pos, &q, radius, radius, NULL, 0);
			} else if (mode == 1) {
				add = !Engine_TestLine(pos, &q, NULL, NULL);
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

/* one cell of a ring: its points collected; 1 when the search has enough */
static int NWRings_Cell(CNodesWorld *self, const int *x, const int *y, const int *z, const vec3_t *pos,
	const vec3_t **list, int max, int *count, float radius, float distMin, float distMax, byte soft, int mode,
	float e) {
	cNWIntList *cell;

	self->grid->vftable->GetCell(self->grid, x, y, z, &cell);
	CNodesWorld_CollectCell(self, pos, cell, list, max, count, radius, distMin, distMax, mode, soft == 0);
	return *count >= max || (*count > 0 && soft && e >= distMax);
}

/* a side of the ring box moved out one cell: 0 the side stops, 1 it grows, 2 the search ends */
static int NWRings_Grow(const CNodesWorld *self, int inGrid, float e, float distMax, byte soft, int count) {
	if (!inGrid || !(self->maxVisDist > e) || (!soft && !(distMax > e))) {
		return 0;
	}
	if (count > 0 && soft && e >= distMax) {
		return 2;
	}
	return 1;
}

void __thiscall CNodesWorld_SearchRings(CNodesWorld *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
	float radius, float distMin, float distMax, byte soft, int mode) {
	int c[3], n[3], st[3], x, y, z;
	vec3_t cs, dir;
	byte stopXhi = 0, stopXlo = 0, stopYhi = 0, stopYlo = 0, stopZhi = 0, stopZlo = 0;

	CHECK(distMin <= distMax);
	self->grid->vftable->CellCoords(self->grid, pos, &c[0], &c[1], &c[2]);
	self->grid->vftable->GetDivs(self->grid, &n[0], &n[1], &n[2]);
	self->grid->vftable->GetCellSize(self->grid, &cs);
	NW_Normalize(&dir, (pos->x + 1.0f) - pos->x, (pos->y + 1.0f) - pos->y, (pos->z + 1.0f) - pos->z);
	/* the start box: from the cell of pos to the cell of the point distMin along the diagonal */
	if (distMin > 0.0f) {
		st[0] = (int)(((pos->x + dir.x * distMin) - self->min.x) / cs.x);
		st[1] = (int)(((pos->y + dir.y * distMin) - self->min.y) / cs.y);
		st[2] = (int)(((pos->z + dir.z * distMin) - self->min.z) / cs.z);
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
#define NWRINGS_STEP(stop, cond, e, op) \
	if (!(stop)) { \
		int g = NWRings_Grow(self, (cond), (e), distMax, soft, *count); \
		if (g == 2) { \
			return; \
		} \
		if (g) { \
			op; \
		} else { \
			(stop) = 1; \
		} \
	}
#define NWRINGS_CELL(px, py, pz, e) \
	if (NWRings_Cell(self, (px), (py), (pz), pos, list, max, count, radius, distMin, distMax, soft, mode, (e))) { \
		return; \
	}
	for (int r = 0;; r++) {
		float fr = (float)r;
		float ex = fr * cs.x, ey = fr * cs.y, ez = fr * cs.z;

		NWRINGS_STEP(stopXhi, hiX + 1 < n[0], ex, hiX++)
		NWRINGS_STEP(stopXlo, loX - 1 >= 0, ex, loX--)
		NWRINGS_STEP(stopYhi, hiY + 1 < n[1], ey, hiY++)
		NWRINGS_STEP(stopYlo, loY - 1 >= 0, ey, loY--)
		NWRINGS_STEP(stopZhi, hiZ + 1 < n[2], ez, hiZ++)
		NWRINGS_STEP(stopZlo, loZ - 1 >= 0, ez, loZ--)
		if (stopXhi && stopXlo && stopYhi && stopYlo && stopZhi && stopZlo) {
			return;
		}
		/* the new faces of the box: x over all y z, y without the x faces, z without the x and y faces */
		if (!stopXhi) {
			for (y = loY; y <= hiY; y++) {
				for (z = loZ; z <= hiZ; z++) {
					NWRINGS_CELL(&hiX, &y, &z, ex)
				}
			}
		}
		if (!stopXlo) {
			for (y = loY; y <= hiY; y++) {
				for (z = loZ; z <= hiZ; z++) {
					NWRINGS_CELL(&loX, &y, &z, ex)
				}
			}
		}
		int xs = loX + !stopXlo, xe = hiX - !stopXhi;
		if (!stopYhi) {
			for (x = xs; x <= xe; x++) {
				for (z = loZ; z <= hiZ; z++) {
					NWRINGS_CELL(&x, &hiY, &z, ey)
				}
			}
		}
		if (!stopYlo) {
			for (x = xs; x <= xe; x++) {
				for (z = loZ; z <= hiZ; z++) {
					NWRINGS_CELL(&x, &loY, &z, ey)
				}
			}
		}
		int ys = loY + !stopYlo, ye = hiY - !stopYhi;
		if (!stopZhi) {
			for (x = xs; x <= xe; x++) {
				for (y = ys; y <= ye; y++) {
					NWRINGS_CELL(&x, &y, &hiZ, ez)
				}
			}
		}
		if (!stopZlo) {
			for (x = xs; x <= xe; x++) {
				for (y = ys; y <= ye; y++) {
					NWRINGS_CELL(&x, &y, &loZ, ez)
				}
			}
		}
	}
#undef NWRINGS_STEP
#undef NWRINGS_CELL
}

void __thiscall CNodesWorld_GetPointsVis(CNodesWorld *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float radius, float distMin, float distMax, byte soft, int mode) {
	vec3_t tmp;

	CHECK(distMin <= distMax);
	*count = 0;
	const vec3_t *cs = self->grid->vftable->GetCellSize(self->grid, &tmp);
	if (cs->x * cs->x + cs->y * cs->y + cs->z * cs->z >= distMin * distMin) {
		cNWIntList *cell;
		self->grid->vftable->GetCellAt(self->grid, pos, &cell);
		CNodesWorld_CollectCell(self, pos, cell, list, max, count, radius, distMin, distMax, mode, soft == 0);
	}
	if (*count < max) {
		CNodesWorld_SearchRings(self, pos, list, max, count, radius, distMin, distMax, soft, mode);
	}
}

byte __thiscall CNodesWorld_HasPointVis(CNodesWorld *self, const vec3_t *pos, float radius) {
	const vec3_t *point;
	int count;

	CNodesWorld_GetPointsVis(self, pos, &point, 1, &count, radius, 0.0f, g_IsMapOutDoor ? 25000.0f : 2500.0f, 0, 0);
	return count > 0;
}

void NW_CamPos(float *x, float *y, float *z) {
	typedef void(__thiscall * getCamPos_t)(cEntity * e, float *x, float *y, float *z);
	cEntity *e = World_FindEntity(cWorld_GetCamName(g_pWorld, 0));

	((getCamPos_t)((void *const *)e->vftable)[0x5c / 4])(e, x, y, z);
}

byte __thiscall CNodesWorld_PointSeen(CNodesWorld *self, const vec3_t *pos) {
	vec3_t cam;

	(void)self;
	int sector = Sector_At(pos);
	if (sector < 0) {
		return 1;
	}
	NW_CamPos(&cam.x, &cam.y, &cam.z);
	int camSector = Video_GetStatus();
	int posSector = Sector_At(&cam);
	if (camSector >= 0 && posSector >= 0 && sector != camSector && sector != posSector) {
		int s = Collision_Sector(sector, pos, &cam, 1);
		if (s != camSector) {
			return s == posSector;
		}
	}
	return 1;
}

byte __thiscall CNodesWorld_GetRacePoint(CNodesWorld *self, vec3_t *out, const vec3_t *initial, float minDist,
	float maxDist, int arg) {
	const vec3_t *list[7];
	int count;

	(void)arg;
	CHECK(minDist <= maxDist);
	if (!self->numNodes) {
		return 0;
	}
	CNodesWorld_GetPointsVis(self, initial, list, 7, &count, 0.0f, minDist, maxDist, 0, 2);
	if (!count) {
		return 0;
	}
	*out = *list[crt_rand() % count];
	return 1;
}

byte __thiscall CNodesWorld_GetRacePointDir(CNodesWorld *self, vec3_t *out, const vec3_t *initial, float minDist,
	float maxDist, int arg, const vec3_t *dir, float maxAngle) {
	const vec3_t *list[15];
	int count;

	(void)arg;
	CHECK(self->numNodes > 0);
	CHECK(minDist <= maxDist);
	CNodesWorld_GetPointsVis(self, initial, list, 15, &count, 0.0f, minDist, maxDist, 0, 2);
	if (!count) {
		return 0;
	}
	int i = crt_rand() % count;
	int up = crt_rand() % 2;
	for (int k = 0; k < count; k++) {
		const vec3_t *v = list[i];
		vec3_t d = { v->x - initial->x, v->y - initial->y, v->z - initial->z };
		if ((double)maxAngle > Vec3_Angle(dir, &d)) {
			*out = *list[i];
			return 1;
		}
		if (up) {
			i = (i + 1) % count;
		} else if (!i) {
			i = count - 1;
		} else {
			i--;
		}
	}
	return 0;
}

/* the y of the direction dropped when it is not about vertical (|x| or |z| over 0.1) */
static void NW_FlatDir(vec3_t *d) {
	if (__builtin_fabs((double)d->x) > (double)0.1f || __builtin_fabs((double)d->z) > (double)0.1f) {
		d->y = 0.0f;
	}
}

byte __thiscall CNodesWorld_BuildPath(CNodesWorld *self, cNWVecList *path, int depth, const vec3_t *dir,
	const vec3_t *start, byte direct, cNWNode **node, float radius, byte flat) {
	cNWNode *cand[7], *list[20], *cur;
	/* one stack slot of the original: the count of the search, its loop index, then the chosen node */
	int slot = 0;
	int ncand;
	vec3_t s, d, e, p;

	CHECK(depth <= 20);
	DListVec3_Clear(path);
	s = *start;
	if (direct && CNodesWorld_TestPath(self, &s, &(*node)->pos, radius, radius, NULL, 0)) {
		cur = *node;
		cand[0] = cur;
		ncand = 1;
	} else {
		CNodesWorld_GetPointsVis(self, &s, (const vec3_t **)cand, 7, &slot, radius, 0.0f, self->f5c, 1, 0);
		ncand = slot;
		if (!ncand) {
			return 0;
		}
		cur = cand[0];
	}
	double best = (double)3.4028234663852886e+38f;
	d = *dir;
	NW_FlatDir(&d);
	if (ncand > 1) {
		/* the pointer itself when no angle is below FLT_MAX, as in the original */
		cur = (cNWNode *)node;
		for (slot = 0; slot < ncand; slot++) {
			cNWNode *v = cand[slot];
			e.x = v->pos.x - s.x;
			e.y = v->pos.y - s.y;
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
			if (self->vftable->InList(self, list, n, cur->links[j])) {
				continue;
			}
			inList = 0;
			cNWNode *v = cur->links[j];
			const cNWNode *prev = list[n - 1];
			e.x = v->pos.x - prev->pos.x;
			e.y = v->pos.y - prev->pos.y;
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
			NW_FlatDir(&d);
		}
		list[n] = next;
		cur = next;
		n++;
	}
	*node = cur;
	return 1;
}

void __thiscall CNodesWorld_NearestPointVis(CNodesWorld *self, const vec3_t *pos, vec3_t *out) {
	const vec3_t *list[7];
	int count = 0;
	vec3_t tmp;
	float maxd = self->maxVisDist;

	CHECK(maxd >= 0.0f);
	const vec3_t *cs = self->grid->vftable->GetCellSize(self->grid, &tmp);
	int full = 0;
	if (cs->x * cs->x + cs->y * cs->y + cs->z * cs->z >= 0.0f) {
		cNWIntList *cell;
		self->grid->vftable->GetCellAt(self->grid, pos, &cell);
		CNodesWorld_CollectCell(self, pos, cell, list, 7, &count, 0.0f, 0.0f, maxd, 2, 0);
		full = count >= 7;
	}
	if (!full) {
		CNodesWorld_SearchRings(self, pos, list, 7, &count, 0.0f, 0.0f, maxd, 1, 2);
	}
	CHECK(count > 0);
	/* the cell size left in tmp is the answer without any point */
	vec3_t seen = tmp, any = tmp;
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

/* a random linked node of the node: the unit direction to it, the distance */
static float NW_RandomLink(const CNodesWorld *self, int node, vec3_t *dir) {
	const cNWNode *n = &self->nodes[node];
	vec3_t to = n->links[crt_rand() % n->numLinks]->pos;

	return NW_Normalize(dir, to.x - n->pos.x, to.y - n->pos.y, to.z - n->pos.z);
}

/* rand() / 32767 of len, in doubles */
static float NW_RandomPart(float len) {
	int r = crt_rand() & 0x7fff;

	return (float)((double)r * 0x1.0002000400080p-15 * (double)len + 0.0);
}

void __thiscall CNodesWorld_RandomNearNode(CNodesWorld *self, int node, float maxDist, vec3_t *out) {
	vec3_t dir;
	float len = NW_RandomLink(self, node, &dir);
	float t = NW_RandomPart(Float_MinSS(len, maxDist));
	const vec3_t *p = &self->nodes[node].pos;

	out->x = dir.x * t + p->x;
	out->y = p->y + dir.y * t;
	out->z = p->z + dir.z * t;
}

void __thiscall CNodesWorld_RandomAheadNode(CNodesWorld *self, int node, const vec3_t *pos, float distMin,
	float distMax, float maxDist, const vec3_t *dir, float maxAngle, vec3_t *out) {
	vec3_t d, p, e;
	float t = NW_RandomPart(NW_RandomLink(self, node, &d));
	const vec3_t *np = &self->nodes[node].pos;

	p.x = d.x * t + np->x;
	p.y = np->y + d.y * t;
	p.z = np->z + d.z * t;
	e.x = p.x - pos->x;
	e.y = p.y - pos->y;
	e.z = p.z - pos->z;
	float dist = (float)__builtin_sqrt((double)(e.y * e.y + e.x * e.x + e.z * e.z));
	if (dist > distMin && distMax > dist && (double)maxAngle > Vec3_Angle(dir, &e)) {
		*out = p;
		return;
	}
	CNodesWorld_RandomNearNode(self, node, maxDist, out);
}

/* the axes and the half of nodeSize */
static void NW_SetNodeSize(CNodesWorld *self) {
	self->axis[0].x = self->nodeSize.x;
	self->axis[0].y = 0.0f;
	self->axis[0].z = 0.0f;
	self->axis[1].y = self->nodeSize.y;
	self->axis[1].x = 0.0f;
	self->axis[1].z = 0.0f;
	self->axis[2].z = self->nodeSize.z;
	self->axis[2].x = 0.0f;
	self->axis[2].y = 0.0f;
	self->halfNode.x = self->nodeSize.x * 0.5f;
	self->halfNode.y = self->nodeSize.y * 0.5f;
	self->halfNode.z = self->nodeSize.z * 0.5f;
}

/* the top of the level outdoors: the dummy DM_Atmosphere */
static void NW_MapTop(vec3_t *top) {
	char buf[20];
	float ang[3];
	byte flag;

	DummySearch_Start("DM_Atmosphere", 0);
	if (!DummySearch_Next(buf, sizeof(buf), top, ang, &flag)) {
		Con_Bluef("ERROR AI: Dummy DM_MapTop no encontrado.\n");
		CHECK(!"CNodesWorld_IniNodesWorld_DM_MapTop_Not_Found");
	}
}

void __thiscall CNodesWorld_Load(CNodesWorld *self, int arg, const char *name, const vec3_t *pos) {
	box_t box;
	vec3_t top;

	Con_Printf("\nCreating Nodes...\n");
	Con_Bluef("%s", NW_EMPTY);
	self->nodeSize = *pos;
	NW_SetNodeSize(self);
	self->f44.x = self->nodeSize.x * 0.1f;
	self->f44.y = self->nodeSize.y * 0.1f;
	self->f44.z = self->nodeSize.z * 0.1f;
	self->margin = Float_MinSS(self->nodeSize.z, Float_MinSS(self->nodeSize.y, self->nodeSize.x)) / 100.0f;
	World_GetCollideBox(&box);
	self->min = box.min;
	self->max = box.max;
	if (*name) {
		size_t n = strnlen(name, sizeof(self->name) - 1);
		memcpy(self->name, name, n);
		self->name[n] = 0;
	}
	self->vftable->FreeGraph(self);
	if (arg < 0) {
		if (arg != -1) {
			goto end;
		}
		if (!self->vftable->Create(self)) {
			Con_Warnf("NODES 3D AI LEVEL NOT LOADED.\n\n");
			float sz = g_IsMapOutDoor ? 1400.0f : 130.0f;
			self->nodeSize.x = sz;
			self->nodeSize.y = sz;
			self->nodeSize.z = sz;
			NW_SetNodeSize(self);
			goto end;
		}
		self->min.x -= self->margin;
		self->min.y -= self->margin;
		self->min.z -= self->margin;
		self->max.x += self->margin;
		self->max.y += self->margin;
		self->max.z += self->margin;
		if (g_IsMapOutDoor) {
			NW_MapTop(&top);
			if (self->max.y > top.y) {
				self->max.y = top.y;
			}
		}
		self->vftable->BuildGrid(self);
		Con_Printf("NODES 3D AI LEVEL LOADED.\n\n");
		Con_Bluef("%s", NW_EMPTY);
		goto end;
	}
	float mn = Float_MinSS(pos->z, Float_MinSS(pos->y, pos->x));
	if (g_IsMapOutDoor) {
		if (!(mn > 100.0f) || !(100000.0f > Float_MaxSS(pos->z, Float_MaxSS(pos->y, pos->x)))) {
			CHECK(!"(MinComponent(vectInterNod) > 100.0f) && (MaxComponent(vectInterNod) < 100000.0f)");
		}
	} else if (!(mn > 10.0f) || !(10000.0f > Float_MaxSS(pos->z, Float_MaxSS(pos->y, pos->x)))) {
		CHECK(!"(MinComponent(vectInterNod) > 10.0f) && (MaxComponent(vectInterNod) < 10000.0f)");
	}
	self->created = 0;
	if (arg == 0) {
		char buf[20];
		float ang[3];
		byte flag;

		DummySearch_Start("DM_NodeIniAI", 0);
		if (!DummySearch_Next(buf, sizeof(buf), &self->nodeInit, ang, &flag)) {
			Con_Bluef("ERROR AI: Dummy DM_NodeInitAI no encontrado.\n");
			CHECK(!"CNodesWorld_IniNodesWorld_DM_NodeInitAI_Not_Found");
		}
		self->nodeInitDummy = 1;
	} else {
		NW_CamPos(&self->nodeInit.x, &self->nodeInit.y, &self->nodeInit.z);
		self->nodeInitDummy = 0;
	}
	CHECK(self->nodeInit.x >= self->min.x && self->max.x >= self->nodeInit.x && self->nodeInit.y >= self->min.y &&
		self->max.y >= self->nodeInit.y && self->nodeInit.z >= self->min.z && self->max.z >= self->nodeInit.z);
	if (arg <= 0) {
		self->min.x = (self->min.x - self->nodeSize.x) - self->margin;
		self->min.y = (self->min.y - self->nodeSize.y) - self->margin;
		self->min.z = (self->min.z - self->nodeSize.z) - self->margin;
		self->max.x = (self->nodeSize.x + self->max.x) + self->margin;
		self->max.y = (self->max.y + self->nodeSize.y) + self->margin;
		self->max.z = (self->max.z + self->nodeSize.z) + self->margin;
	} else {
		float fa = (float)arg;
		float ex = ((self->halfNode.x + self->halfNode.x) * fa + self->halfNode.x) + self->margin;
		float ey = (self->halfNode.y + (self->halfNode.y + self->halfNode.y) * fa) + self->margin;
		float ez = (self->halfNode.z + (self->halfNode.z + self->halfNode.z) * fa) + self->margin;
		self->min.z = self->nodeInit.z - ez;
		self->min.x = self->nodeInit.x - ex;
		self->min.y = self->nodeInit.y - ey;
		self->max.y = self->nodeInit.y + ey;
		self->max.x = ex + self->nodeInit.x;
		self->max.z = self->nodeInit.z + ez;
	}
	if (g_IsMapOutDoor) {
		NW_MapTop(&top);
		if (arg == 0 || self->max.y > top.y) {
			self->max.y = top.y;
		}
	}
	if (!self->analizing) {
		Con_Printf("Mem. used: %d bytes\n", g_MemStats->bytes);
		if (arg == 0) {
			CNodesWorld_AddExtraPoints(self);
		}
		byte made = CNodesWorld_Generate(self, &self->nodeInit, &self->min, &self->max);
		Con_Printf("\nNUM. POINTS OF VISIBILITY GENERATED: %d\n", self->points.count - self->extraPoints.count);
		Con_Printf("NUM. POINTS OF VISIBILITY EXTRA MAP: %d\n", self->extraPoints.count);
		Con_Printf("NUM. POINTS OF VISIBILITY TOTAL: %d\n", self->points.count);
		Con_Printf("NUM. LINKS BETWEEN POINTS VIS.: %d\n", self->numPointLinks);
		Con_Bluef("%s", NW_EMPTY);
		if (made) {
			CNodesWorld_SaveInfPath(self);
		}
		self->vftable->FreeTemp(self);
		if (self->vftable->Create(self)) {
			if (arg == 0) {
				self->vftable->BuildGrid(self);
			}
			self->created = 1;
			Con_Printf("Mem. used: %d bytes\n", g_MemStats->bytes);
			Con_Printf("NODES 3D AI LEVEL LOADED.\n");
			Con_Bluef("%s", NW_EMPTY);
			goto end;
		}
	} else {
		CNodesWorld_AnalizeMap(self, &self->nodeInit, &self->min, &self->max);
		self->vftable->FreeTemp(self);
		if (self->vftable->Create(self)) {
			self->vftable->BuildGrid(self);
			self->created = 1;
			goto end;
		}
	}
	Con_Bluef("NODES 3D AI LEVEL NOT LOADED.\n\n");
end:
	Con_Bluef("%s", NW_EMPTY);
}

void __thiscall CNodesWorld_Analize(CNodesWorld *self, const vec3_t *size) {
	g_NWAnalizeOn = 1;
	g_NWDrawMode = 7;
	self->analizing = 1;
	self->analizeSize = *size;
	CNodesWorld_Load(self, 0, NW_EMPTY, size);
	self->analizing = 0;
	if (!self->open) {
		Con_Printf("\nESCENARIO CERRADO\n");
	} else {
		Con_Bluef("ESCENARIO ABIERTO!\n");
	}
	if (self->tris.count > 0) {
		Con_Bluef("\nNUM. TRIANG. NORMAL INVERTIDA: %d\n", self->tris.count);
	} else {
		Con_Printf("\nNUM. TRIANG. NORMAL INVERTIDA: %d\n", self->tris.count);
	}
	if (!self->errUnlinked.count) {
		Con_Printf("\nSECTORES CORRECTOS\n");
	} else {
		Con_Bluef("\nNUM. PUNTOS EN SECTORES INCORRECTOS: %d\n", self->errUnlinked.count);
	}
	Con_Bluef("%s", NW_EMPTY);
}

void __thiscall CNodesWorld_AnalizeGraph(CNodesWorld *self) {
	vec3_t hit;
	byte bad;

	g_NWAnalizeOn = 1;
	g_NWDrawMode = 9;
	DListVec3_Clear(&self->errCollide);
	bad = 0;
	for (int i = 0; i < self->numNodes; i++) {
		for (int k = 0; k < self->nodes[i].numLinks; k++) {
			const cNWNode *n = &self->nodes[i];
			if (Engine_TestSphere(&n->pos, &n->links[k]->pos, g_NWNodeRadius - 1.0f, &hit, NULL)) {
				NWVecList_Add(&self->errCollide, &self->nodes[i].pos);
				NWVecList_Add(&self->errCollide, &self->nodes[i].links[k]->pos);
				NWVecList_Add(&self->errCollide, &hit);
				bad = 1;
			}
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. PUNTOS GRAFO COLISION ESC.: %d\n", self->errCollide.count / 3);
		Con_Bluef("GRAFO ERRONEO!\n");
	} else {
		Con_Printf("\nGRAFO NO COLISIONA CON GEOMETRIA\n");
	}
	DListVec3_Clear(&self->errUnlinked);
	bad = 0;
	for (int i = 0; i < self->numNodes; i++) {
		if (!self->nodes[i].numLinks) {
			NWVecList_Add(&self->errUnlinked, &self->nodes[i].pos);
			bad = 1;
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. NODOS NO CONECTADOS: %d\n", self->errUnlinked.count);
		Con_Bluef("GRAFO ERRONEO!\n");
	} else {
		Con_Printf("\nGRAFO CON NODOS CONECTADOS\n");
	}
	DListVec3_Clear(&self->errOneWay);
	bad = 0;
	for (int i = 0; i < self->numNodes; i++) {
		for (int k = 0; k < self->nodes[i].numLinks; k++) {
			const cNWNode *to = self->nodes[i].links[k];
			int j = 0;
			while (j < to->numLinks && to->links[j] != &self->nodes[i]) {
				j++;
			}
			if (j < to->numLinks) {
				continue;
			}
			NWVecList_Add(&self->errOneWay, &self->nodes[i].pos);
			NWVecList_Add(&self->errOneWay, &self->nodes[i].links[k]->pos);
			bad = 1;
		}
	}
	if (bad) {
		Con_Bluef("\n\nNUM. LINKS NO CONMUTATIVOS: %d\n", self->errOneWay.count / 2);
		Con_Bluef("GRAFO ERRONEO!\n");
		Con_Bluef("%s", NW_EMPTY);
		return;
	}
	Con_Printf("\nGRAFO CON LINKS CONMUTATIVOS\n");
	Con_Bluef("%s", NW_EMPTY);
}
