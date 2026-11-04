/*
 * The containers of the A* search, original sources AI/AStar.cpp and the templates of Global/ (include/scrap/astar.h).
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/aicharcontrol.h"
#include "scrap/astar.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/hashtable.h"
#include "scrap/nodesworld2d.h"
#include "scrap/scrapmain.h"
#include "scrap/staticinit.h"

#define g_AStarDrawMode (GVar_ShowAStarAI.value.i)	/* 1 closed, 2 open, 3 both */
static cGVar GVar_ShowAStarAI;

static const CAStarVtbl CAStarSup_vftable;
#define CASTARSUP_VFTABLE (&CAStarSup_vftable)
static const CAStarVtbl CAStar_vftable;
#define CASTAR_VFTABLE (&CAStar_vftable)
static const CAStarVtbl CAStar2D_vftable;
#define CASTAR2D_VFTABLE (&CAStar2D_vftable)

static cBinaryHeap *cBinaryHeap_ctor(cBinaryHeap *self, int capacity, binHeapLess_t less, binHeapEq_t equals,
	binHeapIndex_t setIndex) {
	self->count = 0;
	self->capacity = 0;
	if (!less || !equals) {
		Con_FatalErrorf("ERROR: cBinaryHeap - funcCompare == NULL or funcEquals == NULL.\n");
	}
	self->equals = equals;
	self->setIndex = setIndex;
	self->less = less;
	self->elems = Mem_Alloc((capacity + 1) * sizeof(*self->elems));
	if (!self->elems) {
		Con_FatalErrorf("ERROR: cBinaryHeap - Insuficiente memoria Binary Heap.\n");
	}
	self->capacity = capacity;
	return self;
}

static void cBinaryHeap_dtor(cBinaryHeap *self) {
	Mem_Free(self->elems);
}

static void cBinaryHeap_Free(cBinaryHeap *self) {
	cBinaryHeap_dtor(self);
	operator_delete(self);
}

static void cBinaryHeap_Clear(cBinaryHeap *self) {
	self->count = 0;
}

static void BinHeap_Moved(cBinaryHeap *self, int i) {
	if (self->setIndex) {
		self->setIndex(&self->elems[i], i);
	}
}

static byte cBinaryHeap_SiftUp(cBinaryHeap *self, int i);
static byte cBinaryHeap_Push(cBinaryHeap *self, void *const *item) {
	if (self->capacity <= self->count) {
		return 0;
	}
	self->count++;
	self->elems[self->count] = *item;
	BinHeap_Moved(self, self->count);
	cBinaryHeap_SiftUp(self, self->count);
	return 1;
}

static byte cBinaryHeap_Top(cBinaryHeap *self, void ***out) {
	if (self->count < 1) {
		return 0;
	}
	*out = &self->elems[1];
	return 1;
}

static byte cBinaryHeap_SiftDown(cBinaryHeap *self, int i);
static byte cBinaryHeap_Pop(cBinaryHeap *self) {
	if (self->count < 1) {
		return 0;
	}
	if (self->count > 1) {
		self->elems[1] = self->elems[self->count];
		BinHeap_Moved(self, 1);
	}
	self->count--;
	cBinaryHeap_SiftDown(self, 1);
	return 1;
}

static byte cBinaryHeap_Get(cBinaryHeap *self, void ***out, int i) {
	if (i <= 0 || i > self->count) {
		return 0;
	}
	*out = &self->elems[i];
	return 1;
}

static void cBinaryHeap_Fix(cBinaryHeap *self, int i);

/* 4 bytes in the exe, too short for the hook jump: only for the C callers */
static int cBinaryHeap_Count(cBinaryHeap *self) {
	return self->count;
}

static void cBinaryHeap_Fix(cBinaryHeap *self, int i) {
	if (!cBinaryHeap_SiftUp(self, i)) {
		cBinaryHeap_SiftDown(self, i);
	}
}

static void BinHeap_Swap(cBinaryHeap *self, int a, int b) {
	void *t = self->elems[a];

	self->elems[a] = self->elems[b];
	BinHeap_Moved(self, a);
	self->elems[b] = t;
	BinHeap_Moved(self, b);
}

/* 1 if the element moved */
static byte cBinaryHeap_SiftUp(cBinaryHeap *self, int i) {
	byte moved = 0;

	while (i > 1) {
		int parent = i / 2;
		if (!self->less(&self->elems[i], &self->elems[parent])) {
			break;
		}
		BinHeap_Swap(self, parent, i);
		moved = 1;
		i = parent;
	}
	return moved;
}

static byte cBinaryHeap_SiftDown(cBinaryHeap *self, int i) {
	byte moved = 0;

	for (;;) {
		int c = i * 2, best;
		if (c < self->count) {
			best = self->less(&self->elems[c], &self->elems[i]) ? c : i;
			if (self->less(&self->elems[c + 1], &self->elems[best])) {
				best = c + 1;
			}
		} else {
			if (self->count < c || !self->less(&self->elems[c], &self->elems[i])) {
				return moved;
			}
			best = c;
		}
		if (best == i) {
			return moved;
		}
		BinHeap_Swap(self, i, best);
		moved = 1;
		i = best;
	}
}

static byte AStar_ElemLess(void *const *a, void *const *b) {
	return ((const ElemAStar *)*a)->f < ((const ElemAStar *)*b)->f;
}

static byte AStar_ElemEq(void *const *a, const void *b) {
	return ((const ElemAStar *)*a)->node == ((const ElemAStar *)*(void *const *)b)->node;
}

static void AStar_ElemSetIndex(void **elem, int index) {
	cAStarInfo *info = ((ElemAStar *)*elem)->info;

	if (info) {
		info->index = index;
		info->state = 1;
	}
}

/* the number hash of HashTable.h: the nibbles from the low end */
static uint32_t AStarTable_Slot(HashTable *self, uintptr_t key) {
	uint32_t h = 0;

	CHECK(self->numSlots);
	for (; key; key >>= 4) {
		h = Hash_Step(h + 3, key & 0xf);
	}
	return h % self->numSlots;
}

static cAStarItem *AStarTable_Find(HashTable *self, intptr_t key, uint32_t slot) {
	for (cAStarItem *it = (cAStarItem *)self->slots[slot]; it; it = it->next) {
		if (it->key == key) {
			return it;
		}
	}
	return NULL;
}

static cAStarItem *AStarTable_Insert(HashTable *self, intptr_t key, uint32_t slot, const cAStarInfo *v) {
	CHECK(!self->locked);
	cAStarItem *it = HashItem_Alloc(sizeof(*it));
	if (it) {
		it->value = *v;
		it->next = NULL;
	}
	it->next = (cAStarItem *)self->slots[slot];
	it->key = key;
	self->slots[slot] = (HashNode *)it;
	return it;
}

/* the items freed, the slots kept */
static void AStarTable_Clear(HashTable *self) {
	CHECK(!self->locked);
	if (!self->slots) {
		return;
	}
	for (int i = 0; i < (int)self->numSlots; i++) {
		while (self->slots[i]) {
			cAStarItem *it = (cAStarItem *)self->slots[i];
			self->slots[i] = (HashNode *)it->next;
			HashItem_Free(it);
		}
	}
	memset(self->slots, 0, self->numSlots * sizeof(*self->slots));
}

static void AStarTable_dtor(HashTable *self) {
	AStarTable_Clear(self);
	operator_delete_array(self->slots);
	self->numSlots = 0;
	self->slots = NULL;
}

/* the item of key added unless it is there (the inline Add of the callers, without the warning) */
static void AStarTable_AddNew(HashTable *t, intptr_t key, const cAStarInfo *v) {
	CHECK(!t->locked);
	uint32_t slot = AStarTable_Slot(t, key);
	if (!AStarTable_Find(t, key, slot)) {
		AStarTable_Insert(t, key, slot, v);
	}
}

/* ---- the search ---- */

/* the graph: CNodesWorld or CNodesWorld2D, the vtable of CNodesWorldSup */
#define AS_NODES(self) ((CNodesWorld *)(self)->nodes)

/* CAStarSup, the base class: the table, the ends zeroed, maxDist */
static CAStarSup *CAStarSup_ctor(CAStarSup *self) {
	self->vftable = CASTARSUP_VFTABLE;
	HashTable *t = operator_new(sizeof(HashTable));
	if (t) {
		t->numSlots = 0x3f1;
		t->slots = New_Array(0x3f1, sizeof(HashNode *));
		memset(t->slots, 0, 0x3f1 * sizeof(HashNode *));
		t->locked = 0;
	}
	self->table = t;
	if (!t) {
		Con_FatalErrorf("ERROR AI: AStar - Error al crear Tabla Hash.\n");
	}
	self->goal.y = 0.0f;
	self->goal.z = 0.0f;
	self->goal.x = 0.0f;
	self->start.x = 0.0f;
	self->start.y = self->goal.y;
	self->start.z = self->goal.z;
	self->radius = 0.0f;
	self->numClosed = 0;
	self->maxDist = g_NodesWorld->sup.pairRange * 1.25f;
	return self;
}

static void CAStarSup_dtor(CAStarSup *self) {
	HashTable *t = self->table;

	self->vftable = CASTARSUP_VFTABLE;
	if (t) {
		AStarTable_dtor(t);
		operator_delete(t);
	}
}

static void CAStarSup_Free(CAStarSup *self) {
	CAStarSup_dtor(self);
	operator_delete(self);
}

static void CAStarSup_Draw(CAStarSup *self, uint32_t color) {
	cBinaryHeap *open = self->open;

	(void)color;
	if (g_AStarDrawMode == 1 || g_AStarDrawMode == 3) {
		for (int i = 0; i < self->numClosed; i++) {
			const vec3_t *p = self->closed[i]->node;
			if (p) {
				DebugUtils_DrawSphere(p, 50.0f, 0xff, 2, 2);
			}
		}
	}
	if (g_AStarDrawMode == 2 || g_AStarDrawMode == 3) {
		int n = cBinaryHeap_Count(open);
		for (int i = 1; i <= n; i++) {
			void **e = NULL;
			cBinaryHeap_Get(open, &e, i);
			const vec3_t *p = ((ElemAStar *)*e)->node;
			if (p) {
				DebugUtils_DrawSphere(p, 50.0f, 0xff00ff, 2, 2);
			}
		}
	}
	DebugUtils_DrawSphere(&self->start, 50.0f, 0xff00, 2, 2);
	DebugUtils_DrawSphere(&self->goal, 50.0f, 0xff0000, 2, 2);
}

static HashTable *CAStarSup_GetTable(CAStarSup *self) {
	return self->table;
}

/* the A* from start to goal over the points of visibility; path: goal first, the start last. ok 0 when the lists are
 * full or the start has no points, found 0 when the goal has no point or the graph has no way */
static byte CAStarSup_Search(CAStarSup *self, cNWVecList *path, int stupid, byte *ok, byte *found, int *state) {
	cBinaryHeap *open;
	const vec3_t *goalPt;
	int n, file;

	*ok = 1;
	*found = 1;
	*state = 0;
	DListVec3_Clear(path);
	AStar_Reset(self);
	if ((self->start.x == self->goal.x && self->start.y == self->goal.y && self->start.z == self->goal.z)
		|| AStar_TestPath(self, &self->goal, &self->start)) {
		DListVec3_Add(path, &self->goal);
		DListVec3_Add(path, &self->start);
		return 1;
	}
	n = 0;
	AStar_PointsVis(self, &self->goal, &goalPt, 1, &n);
	if (n == 0) {
		goto lost;
	}
	file = AStar_FileOf(self, goalPt);
	vec3_t startCopy = self->start;
	ElemAStar first;
	first.node = NULL;
	first.parent = NULL;
	first.g = 0.0f;
	first.f = AStar_Heuristic(self, &self->start);
	first.info = NULL;
	if (!AStar_AddElem(self, &first)) {
		goto fail;
	}
	ElemAStar *top = &self->list[self->numList - 1];
	open = self->open;
	if (!cBinaryHeap_Push(open, (void *const *)&top)) {
		goto fail;
	}
	self->numList--;
	while (cBinaryHeap_Count(self->open) > 0) {
		const vec3_t *buf[50];
		vec3_t curPos, parentPos;
		void **tp = NULL;
		int count, taken;

		open = self->open;
		cBinaryHeap_Top(open, &tp);
		ElemAStar *cur = *tp;
		curPos = cur->node ? *cur->node : self->start;
		if (cur->parent) {
			parentPos = *cur->parent->node;
		} else {
			parentPos.x = -3.40282347e+38f;
			parentPos.y = -3.40282347e+38f;
			parentPos.z = -3.40282347e+38f;
		}
		cBinaryHeap_Pop(open);
		if (AStar_TestPath(self, &self->goal, &curPos)) {
			if (cur->node) {
				AStar_CloseAdd(self, &cur);
				AStar_BuildPath(self, path, cur);
			}
			DListVec3_Add(path, &self->start);
			if ((byte)Vec3_Differ(&self->start.x, &startCopy.x)) {
				DListVec3_Add(path, &startCopy);
			}
			return 1;
		}
		count = 0;
		if (!cur->node) {
			AStar_PointsVisFile(self, &self->start, buf, 50, &count, state, file);
			if (count == 0) {
				goto fail;
			}
		} else {
			AStar_OtherLinks(self, cur->node, &parentPos, buf, 50, &count);
		}
		AStar_CloseAdd(self, &cur);
		taken = 0;
		for (int i = 0; i < count; i++) {
			ElemAStar *el;
			cAStarItem *info;
			byte isOpen, isClosed;

			if (stupid) {
				int r = crt_rand();
				if (taken >= 1 && r % 10 < stupid) {
					continue;
				}
			}
			const vec3_t *nb = buf[i];
			taken++;
			float g = AStar_Dist(self, &curPos, nb) + cur->g;
			isOpen = 0;
			isClosed = 0;
			el = NULL;
			if (AStar_GetElem(self, nb, &el, &info)) {
				if (info->value.state == 1) {
					isOpen = 1;
					if (g >= el->g) {
						continue;
					}
				} else if (info->value.state == 0) {
					isClosed = 1;
					if (g >= el->g) {
						continue;
					}
				}
			}
			ElemAStar *parent = cur;
			if (curPos.x == self->start.x && curPos.y == self->start.y && curPos.z == self->start.z) {
				parent = NULL;
			}
			float h = AStar_Heuristic(self, nb);
			el->parent = parent;
			el->f = h + g;
			el->g = g;
			open = self->open;
			if (isOpen) {
				cBinaryHeap_Fix(open, info->value.index);
			} else {
				if (isClosed) {
					AStar_CloseRemove(self, info->value.index);
				}
				open = self->open;
				if (!cBinaryHeap_Push(open, (void *const *)&el)) {
					goto fail;
				}
			}
		}
	}
lost:
	*found = 0;
	return 0;
fail:
	*ok = 0;
	return 0;
}

static void CAStarSup_BuildPath(CAStarSup *self, cNWVecList *path, const ElemAStar *elem) {
	(void)self;
	DListVec3_Add(path, elem->node);
	for (const ElemAStar *p = elem->parent; p; p = p->parent) {
		const vec3_t *v = p->node;
		cNWVecNode *node = DList_NewNode(path, sizeof(*node));
		node->v = *v;
		DList_LinkFirst(path, node);
	}
}

static float CAStarSup_Heuristic(CAStarSup *self, const vec3_t *p) {
	float dx = self->goal.x - p->x;
	float dy = self->goal.y - p->y;
	float dz = self->goal.z - p->z;

	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

static float CAStarSup_Dist(CAStarSup *self, const vec3_t *a, const vec3_t *b) {
	float dx = b->x - a->x;
	float dy = b->y - a->y;
	float dz = b->z - a->z;

	(void)self;
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

static byte CAStarSup_GetElem(CAStarSup *self, const vec3_t *node, ElemAStar **out, cAStarItem **info) {
	*out = NULL;
	byte r = AStar_TableGet(self, node, info);
	if (!r) {
		return r;
	}
	cAStarItem *it = *info;
	if (it->value.state == 1) {
		void **e = NULL;
		cBinaryHeap_Get(self->open, &e, it->value.index);
		*out = *e;
		return 1;
	}
	if (it->value.state == 0) {
		*out = self->closed[it->value.index];
		return 1;
	}
	*out = &self->list[it->value.elem];
	return 1;
}

static byte CAStarSup_AddElem(CAStarSup *self, const ElemAStar *e) {
	if (self->numList >= self->capList) {
		return 0;
	}
	self->list[self->numList] = *e;
	self->numList++;
	return 1;
}

static byte CAStarSup_CloseAdd(CAStarSup *self, ElemAStar *const *e) {
	if (self->numClosed >= self->capClosed) {
		return 0;
	}
	self->closed[self->numClosed] = *e;
	self->numClosed++;
	cAStarInfo *info = (*e)->info;
	if (info) {
		info->state = 0;
		info->index = self->numClosed - 1;
	}
	return 1;
}

static byte CAStarSup_CloseRemove(CAStarSup *self, int i) {
	if (i >= self->numClosed) {
		return 0;
	}
	self->numClosed--;
	if (self->numClosed != i) {
		self->closed[i] = self->closed[self->numClosed];
		self->closed[i]->info->index = i;
	}
	return 1;
}

static byte CAStarSup_TableGet(CAStarSup *self, const vec3_t *node, cAStarItem **out) {
	HashTable *t = self->table;
	cAStarItem *it = AStarTable_Find(t, (intptr_t)node, AStarTable_Slot(t, (uintptr_t)node));

	if (!it) {
		return 0;
	}
	*out = it;
	return 1;
}

static void CAStarSup_Reset(CAStarSup *self) {
	cBinaryHeap *open;

	for (int i = 0; i < self->numClosed; i++) {
		cAStarInfo *info = self->closed[i]->info;
		if (info) {
			info->state = 2;
		}
	}
	open = self->open;
	int n = cBinaryHeap_Count(open);
	for (int i = 1; i <= n; i++) {
		void **e = NULL;
		open = self->open;
		cBinaryHeap_Get(open, &e, i);
		cAStarInfo *info = ((ElemAStar *)*e)->info;
		if (info) {
			info->state = 2;
		}
	}
	self->numClosed = 0;
	open = self->open;
	cBinaryHeap_Clear(open);
}

/* ---- CAStar and CAStar2D ---- */

static inline CAStar *AStar_Common(CAStar2D *self) {
	return (CAStar *)self;
}

/* the lists of the search, one place per point of visibility */
static void CAStar_InitList(CAStar *self);
static void AStar_Alloc(CAStar *self) {
	CNodesWorld *nw = AS_NODES(self);
	int n = NodesWorld_NumVisPoints(nw);

	self->base.capList = n + 1;
	self->base.list = Mem_Alloc((n + 1) * sizeof(ElemAStar));
	if (!self->base.list) {
		Con_FatalErrorf("ERROR AI: AStar - Error al crear ListAStar.\n");
	}
	cBinaryHeap *h = operator_new(sizeof(cBinaryHeap));
	if (h) {
		nw = AS_NODES(self);
		h = cBinaryHeap_ctor(h, NodesWorld_NumVisPoints(nw), AStar_ElemLess, AStar_ElemEq, AStar_ElemSetIndex);
	}
	self->base.open = h;
	if (!h) {
		Con_FatalErrorf("ERROR AI: AStar - Error al crear lista Open.\n");
	}
	nw = AS_NODES(self);
	n = NodesWorld_NumVisPoints(nw);
	self->base.capClosed = n + 1;
	self->base.closed = Mem_Alloc((n + 1) * sizeof(*self->base.closed));
	if (!self->base.closed) {
		Con_FatalErrorf("ERROR AI: AStar - Error al crear lista Closed.\n");
	}
	CAStar_InitList(self);
}

void *CAStar_ctor(void *mem, CNodesWorld *nodes) {
	CAStar *self = mem;

	CAStarSup_ctor(&self->base);
	self->base.vftable = CASTAR_VFTABLE;
	self->nodes = NULL;
	if (!nodes) {
		Con_FatalErrorf("ERROR AI: Grafo pathfinding inexistente.\n");
	}
	self->nodes = nodes;
	AStar_Alloc(self);
	return self;
}

void *CAStar2D_ctor(void *mem, CNodesWorld2D *nodes) {
	CAStar2D *self = mem;

	CAStarSup_ctor(&self->base);
	self->base.vftable = CASTAR2D_VFTABLE;
	self->nodes = NULL;
	if (!nodes) {
		Con_FatalErrorf("ERROR AI: Grafo pathfinding inexistente.\n");
	}
	self->nodes = nodes;
	self->h2 = 0.0f;
	self->h1 = 0.0f;
	AStar_Alloc(AStar_Common(self));
	return self;
}

static void AStar_FreeLists(CAStarSup *self, const CAStarVtbl *vftable) {
	void *list = self->list;

	self->vftable = vftable;
	Mem_Free(list);
	if (self->open) {
		cBinaryHeap_Free(self->open);
	}
	Mem_Free(self->closed);
	CAStarSup_dtor(self);
}

static void CAStar_Free(CAStarSup *elem) {
	CAStar *self = (CAStar *)elem;
	AStar_FreeLists(&self->base, CASTAR_VFTABLE);
	operator_delete(self);
}

static void CAStar2D_Free(CAStarSup *elem) {
	CAStar2D *self = (CAStar2D *)elem;
	AStar_FreeLists(&self->base, CASTAR2D_VFTABLE);
	operator_delete(self);
}

/* the element and the table item of every point of visibility (both classes) */
static void CAStar_InitList(CAStar *self) {
	CNodesWorld *nw;

	AStarTable_Clear(self->base.table);
	self->base.numList = 0;
	nw = AS_NODES(self);
	for (int i = 0; i < NodesWorld_NumVisPoints(nw); i++) {
		ElemAStar e;
		cAStarInfo info;
		cAStarItem *it;

		e.node = NodesWorld_GetVisPoint(nw, i);
		e.parent = NULL;
		e.f = -1.0f;
		e.g = -1.0f;
		e.info = NULL;
		AStar_AddElem(self, &e);
		info.index = -1;
		info.state = 2;
		info.elem = self->base.numList - 1;
		AStarTable_AddNew(self->base.table, (intptr_t)e.node, &info);
		AStar_TableGet(self, e.node, &it);
		self->base.list[self->base.numList - 1].info = &it->value;
		nw = AS_NODES(self);
	}
}

void CAStar_DrawDebug(CAStar *self) {
	if (g_AStarDrawMode) {
		CAStarSup_Draw(&self->base, 0xff);
	}
}

void CAStar2D_DrawDebug(CAStar *self) {
	if (g_AStarDrawMode) {
		CAStarSup_Draw(&self->base, 0xff00ff);
	}
}

static byte CAStar_TestPath(CAStarSup *elem, const vec3_t *a, const vec3_t *b) {
	CAStar *self = (CAStar *)elem;
	return CNodesWorld_TestPath(self->nodes, a, b, self->base.radius, self->base.radius, NULL, 0);
}

static byte CAStar2D_TestPath(CAStarSup *elem, const vec3_t *a, const vec3_t *b) {
	CAStar2D *self = (CAStar2D *)elem;
	return CNodesWorld2D_TestPath(self->nodes, a, b, self->base.radius, self->base.radius, self->h1, self->h2,
		NULL);
}

static void CAStar_NearestStart(CAStarSup *elem) {
	CAStar *self = (CAStar *)elem;
	vec3_t p = self->base.start;

	CNodesWorld_NearestPointVis(self->nodes, &p, &self->base.start);
}

static void CAStar2D_NearestStart(CAStarSup *elem) {
	CAStar2D *self = (CAStar2D *)elem;
	vec3_t p = self->base.start;

	CNodesWorld2D_NearestPointVis(self->nodes, &p, &self->base.start);
}

/* the slots that go on to the graph (both classes) */
static int CAStar_FileOf(CAStarSup *base, const vec3_t *node) {
	CAStar *self = (CAStar *)base;
	CNodesWorld *nw = AS_NODES(self);

	return NodesWorld_FileOf(nw, (const cNWNode *)node);
}

static void CAStar_OtherLinks(CAStarSup *base, const vec3_t *node, const vec3_t *pos, const vec3_t **list, int max, int *count) {
	CAStar *self = (CAStar *)base;
	CNodesWorld *nw = AS_NODES(self);

	NodesWorld_OtherLinks(nw, (const cNWNode *)node, pos, (cNWNode **)list, max, count);
}

/* the points of visibility of the graph with the radius (and the heights) of the search */
static void AStar_Points(CAStar *self, byte flat, const vec3_t *pos, const vec3_t **list, int max, int *count,
	byte soft, int mode) {
	if (flat) {
		const CAStar2D *s2 = (const CAStar2D *)self;

		CNodesWorld2D_GetPointsVis(s2->nodes, pos, list, max, count, self->base.radius, s2->h1, s2->h2, 0.0f,
			self->base.maxDist, soft, mode);
	} else {
		CNodesWorld_GetPointsVis(self->nodes, pos, list, max, count, self->base.radius, 0.0f,
			self->base.maxDist, soft, mode);
	}
}

static void CAStar_PointsVis(CAStarSup *base, const vec3_t *pos, const vec3_t **list, int max, int *count) {
	CAStar *self = (CAStar *)base;
	AStar_Points(self, 0, pos, list, max, count, 0, 0);
}

static void CAStar2D_PointsVis(CAStarSup *base, const vec3_t *pos, const vec3_t **list, int max, int *count) {
	CAStar2D *self = (CAStar2D *)base;
	AStar_Points(AStar_Common(self), 1, pos, list, max, count, 0, 0);
}

/* the first point of the search in the graph file: the points of visibility, the one the segment sees, else the
 * nearest one of the floor (2D: the distance on the floor plane) */
static void AStar_PointsVisFileFlat(CAStar *self, byte flat, const vec3_t *pos, const vec3_t **list, int max, int *count,
	int *state, int file) {
	const vec3_t *buf[20];
	const vec3_t *best;
	float bestDist;
	int n;

	CHECK(max > 0);
	*state = 0;
	AStar_Points(self, flat, pos, list, max < 7 ? max : 7, count, 0, 0);
	if (*count > 0 && CAStar_FileOf(&self->base, *list) != file) {
		*count = 0;
		return;
	}
	if (*count != 0) {
		return;
	}
	*state = 1;
	AStar_Points(self, flat, pos, list, 1, count, 0, 1);
	if (*count > 0 && CAStar_FileOf(&self->base, *list) != file) {
		*count = 0;
		return;
	}
	if (*count != 0) {
		return;
	}
	*state = 2;
	vec3_t p = *pos;
	bestDist = 3.40282347e+38f;
	n = 0;
	AStar_Points(self, flat, pos, buf, max < 20 ? max : 20, &n, 1, 2);
	best = NULL;
	for (int i = 0; i < n; i++) {
		if (CAStar_FileOf(&self->base, buf[i]) != file) {
			continue;
		}
		const vec3_t *q = buf[i];
		float d;
		if (flat) {
			float dx = p.x - q->x;
			float dz = p.z - q->z;
			d = dx * dx + 0.0f + dz * dz;
		} else {
			float dx = p.x - q->x;
			float dy = p.y - q->y;
			float dz = p.z - q->z;
			d = dy * dy + dx * dx + dz * dz;
		}
		if (d < bestDist) {
			bestDist = d;
			best = q;
		}
	}
	if (best) {
		*list = best;
		*count = 1;
	}
}

static void CAStar_PointsVisFile(CAStarSup *base, const vec3_t *pos, const vec3_t **list, int max, int *count, int *state, int file) {
	CAStar *self = (CAStar *)base;
	AStar_PointsVisFileFlat(self, 0, pos, list, max, count, state, file);
}

static void CAStar2D_PointsVisFile(CAStarSup *base, const vec3_t *pos, const vec3_t **list, int max, int *count, int *state, int file) {
	CAStar2D *self = (CAStar2D *)base;
	AStar_PointsVisFileFlat(AStar_Common(self), 1, pos, list, max, count, state, file);
}

byte CAStar_FindPath(void *astar, cNWVecList *path, const vec3_t *to, const vec3_t *from, float radius,
	float dist, int stupid, byte *startOk, byte *goalOk, int *state) {
	CAStar *self = astar;
	CNodesWorld *nw = AS_NODES(self);

	(void)dist;
	CHECK(NodesWorld_IsCreated(nw));
	self->base.radius = radius;
	self->base.start = *to;
	self->base.goal = *from;
	return AStar_Search(self, path, stupid, goalOk, startOk, state);
}

byte CAStar2D_FindPath(void *astar, cNWVecList *path, const vec3_t *dest, const vec3_t *pos, float r,
	float dist, float h1, float h2, int soft, byte *noPath, byte *reach, int *res) {
	CAStar2D *self = astar;
	CNodesWorld *nw = AS_NODES(self);

	(void)dist;
	CHECK(NodesWorld_IsCreated(nw));
	self->base.radius = r;
	self->h1 = h1;
	self->h2 = h2;
	self->base.start = *dest;
	self->base.goal = *pos;
	return AStar_Search(self, path, soft, reach, noPath, res);
}

void CAStar_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count, float r) {
	CAStar *self = astar;

	CNodesWorld_GetPointsVis(self->nodes, pos, list, max, count, r, 0.0f, self->base.maxDist, 0, 0);
}

void CAStar2D_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count,
	float r, float h1, float h2) {
	CAStar2D *self = astar;

	CNodesWorld2D_GetPointsVis(self->nodes, pos, list, max, count, r, h1, h2, 0.0f, self->base.maxDist, 0, 0);
}

/* the vtable of CAStar */
static const CAStarVtbl CAStar_vftable = {
	.Free = CAStar_Free,
	.Draw = CAStarSup_Draw,
	.GetTable = CAStarSup_GetTable,
	.TestPath = CAStar_TestPath,
	.NearestStart = CAStar_NearestStart,
	.PointsVis = CAStar_PointsVis,
	.PointsVisFile = CAStar_PointsVisFile,
	.OtherLinks = CAStar_OtherLinks,
	.Search = CAStarSup_Search,
	.BuildPath = CAStarSup_BuildPath,
	.Heuristic = CAStarSup_Heuristic,
	.Dist = CAStarSup_Dist,
	.GetElem = CAStarSup_GetElem,
	.FileOf = CAStar_FileOf,
	.AddElem = CAStarSup_AddElem,
	.CloseAdd = CAStarSup_CloseAdd,
	.CloseRemove = CAStarSup_CloseRemove,
	.TableGet = CAStarSup_TableGet,
	.Reset = CAStarSup_Reset,
};

/* the vtable of CAStar2D */
static const CAStarVtbl CAStar2D_vftable = {
	.Free = CAStar2D_Free,
	.Draw = CAStarSup_Draw,
	.GetTable = CAStarSup_GetTable,
	.TestPath = CAStar2D_TestPath,
	.NearestStart = CAStar2D_NearestStart,
	.PointsVis = CAStar2D_PointsVis,
	.PointsVisFile = CAStar2D_PointsVisFile,
	.OtherLinks = CAStar_OtherLinks,
	.Search = CAStarSup_Search,
	.BuildPath = CAStarSup_BuildPath,
	.Heuristic = CAStarSup_Heuristic,
	.Dist = CAStarSup_Dist,
	.GetElem = CAStarSup_GetElem,
	.FileOf = CAStar_FileOf,
	.AddElem = CAStarSup_AddElem,
	.CloseAdd = CAStarSup_CloseAdd,
	.CloseRemove = CAStarSup_CloseRemove,
	.TableGet = CAStarSup_TableGet,
	.Reset = CAStarSup_Reset,
};

/* the vtable of CAStarSup */
static const CAStarVtbl CAStarSup_vftable = {
	.Free = CAStarSup_Free,
	.Draw = CAStarSup_Draw,
	.GetTable = CAStarSup_GetTable,
	.TestPath = (void *)crt_purecall,
	.NearestStart = (void *)crt_purecall,
	.PointsVis = (void *)crt_purecall,
	.PointsVisFile = (void *)crt_purecall,
	.OtherLinks = (void *)crt_purecall,
	.Search = CAStarSup_Search,
	.BuildPath = CAStarSup_BuildPath,
	.Heuristic = CAStarSup_Heuristic,
	.Dist = CAStarSup_Dist,
	.GetElem = CAStarSup_GetElem,
	.FileOf = (void *)crt_purecall,
	.AddElem = CAStarSup_AddElem,
	.CloseAdd = CAStarSup_CloseAdd,
	.CloseRemove = CAStarSup_CloseRemove,
	.TableGet = CAStarSup_TableGet,
	.Reset = CAStarSup_Reset,
};

/* the vtable of cBinaryHeap_ElemAStar */

static cGVar GVar_ShowAStarAI;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_ShowAStarAI, &GVar_ShowAStarAI, "ShowAStarAI", 4, "Show info AStar.\n", 0)
