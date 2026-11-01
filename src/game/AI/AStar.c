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

#define CBINARYHEAP_VFTABLE GAME_VFTABLE(cBinaryHeap_ElemAStar_vftable)
#define CASTARSUP_VFTABLE GAME_VFTABLE(CAStarSup_vftable)
#define CASTAR_VFTABLE GAME_VFTABLE(CAStar_vftable)
#define CASTAR2D_VFTABLE GAME_VFTABLE(CAStar2D_vftable)

cBinaryHeap *__thiscall cBinaryHeap_ctor(cBinaryHeap *self, int capacity, binHeapLess_t less, binHeapEq_t equals,
	binHeapIndex_t setIndex) {
	self->vftable = CBINARYHEAP_VFTABLE;
	self->count = 0;
	self->capacity = 0;
	if (!less || !equals) {
		Con_FatalErrorf("ERROR: cBinaryHeap - funcCompare == NULL or funcEquals == NULL.\n");
	}
	self->equals = equals;
	self->setIndex = setIndex;
	self->less = less;
	self->elems = Mem_Alloc(capacity * 4 + 4);
	if (!self->elems) {
		Con_FatalErrorf("ERROR: cBinaryHeap - Insuficiente memoria Binary Heap.\n");
	}
	self->capacity = capacity;
	return self;
}

void __fastcall cBinaryHeap_dtor(cBinaryHeap *self) {
	self->vftable = CBINARYHEAP_VFTABLE;
	Mem_Free(self->elems);
}

cBinaryHeap *__thiscall cBinaryHeap_Delete(cBinaryHeap *self, byte flags) {
	cBinaryHeap_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __thiscall cBinaryHeap_Clear(cBinaryHeap *self) {
	self->count = 0;
}

static void BinHeap_Moved(cBinaryHeap *self, int i) {
	if (self->setIndex) {
		self->setIndex(&self->elems[i], i);
	}
}

byte __thiscall cBinaryHeap_Push(cBinaryHeap *self, void *const *item) {
	if (self->capacity <= self->count) {
		return 0;
	}
	self->count++;
	self->elems[self->count] = *item;
	BinHeap_Moved(self, self->count);
	self->vftable->SiftUp(self, self->count);
	return 1;
}

byte __thiscall cBinaryHeap_Top(cBinaryHeap *self, void ***out) {
	if (self->count < 1) {
		return 0;
	}
	*out = &self->elems[1];
	return 1;
}

byte __thiscall cBinaryHeap_Pop(cBinaryHeap *self) {
	if (self->count < 1) {
		return 0;
	}
	if (self->count > 1) {
		self->elems[1] = self->elems[self->count];
		BinHeap_Moved(self, 1);
	}
	self->count--;
	self->vftable->SiftDown(self, 1);
	return 1;
}

byte __thiscall cBinaryHeap_Get(cBinaryHeap *self, void ***out, int i) {
	if (i <= 0 || i > self->count) {
		return 0;
	}
	*out = &self->elems[i];
	return 1;
}

byte __thiscall cBinaryHeap_Find(cBinaryHeap *self, const void *item, int *index, void ***out) {
	for (int i = 1; i <= self->count; i++) {
		if (self->equals(&self->elems[i], item)) {
			*out = &self->elems[i];
			*index = i;
			return 1;
		}
	}
	return 0;
}

byte __thiscall cBinaryHeap_Set(cBinaryHeap *self, void *const *item, int i) {
	if (i <= 0 || i > self->count) {
		return 0;
	}
	self->elems[i] = *item;
	BinHeap_Moved(self, i);
	self->vftable->Fix(self, i);
	return 1;
}

int __thiscall cBinaryHeap_Count(cBinaryHeap *self) {
	return self->count;
}

void __thiscall cBinaryHeap_Fix(cBinaryHeap *self, int i) {
	if (!self->vftable->SiftUp(self, i)) {
		self->vftable->SiftDown(self, i);
	}
}

static void BinHeap_Swap(cBinaryHeap *self, int a, int b) {
	void *t = self->elems[a];

	self->elems[a] = self->elems[b];
	BinHeap_Moved(self, a);
	self->elems[b] = t;
	BinHeap_Moved(self, b);
}

byte __thiscall cBinaryHeap_SiftUp(cBinaryHeap *self, int i) {
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

byte __thiscall cBinaryHeap_SiftDown(cBinaryHeap *self, int i) {
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

byte AStar_ElemLess(void *const *a, void *const *b) {
	return ((const ElemAStar *)*a)->f < ((const ElemAStar *)*b)->f;
}

byte AStar_ElemEq(void *const *a, const void *b) {
	return ((const ElemAStar *)*a)->node == ((const ElemAStar *)*(void *const *)b)->node;
}

void AStar_ElemSetIndex(void **elem, int index) {
	cAStarInfo *info = ((ElemAStar *)*elem)->info;

	if (info) {
		info->index = index;
		info->state = 1;
	}
}

uint32_t __thiscall AStarTable_Slot(HashTable *self, uint32_t key) {
	uint32_t h = 0;

	CHECK(self->numSlots);
	for (; key; key >>= 4) {
		h = Hash_Step(h + 3, key & 0xf);
	}
	return h % self->numSlots;
}

cAStarItem *__thiscall AStarTable_Find(HashTable *self, int key, uint32_t slot) {
	for (cAStarItem *it = (cAStarItem *)self->slots[slot]; it; it = it->next) {
		if (it->key == key) {
			return it;
		}
	}
	return NULL;
}

cAStarItem *__thiscall AStarTable_Insert(HashTable *self, int key, uint32_t slot, const cAStarInfo *v) {
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

byte __thiscall AStarTable_Add(HashTable *self, int key, const cAStarInfo *v, byte warn) {
	CHECK(!self->locked);
	uint32_t slot = AStarTable_Slot(self, key);
	if (AStarTable_Find(self, key, slot)) {
		if (warn) {
			Con_Warnf("CHashTable2: Symbol already defined: '%08X'\n", key);
		}
		return 0;
	}
	AStarTable_Insert(self, key, slot, v);
	return 1;
}

byte __thiscall AStarTable_Get(HashTable *self, int key, cAStarItem **out) {
	cAStarItem *it = AStarTable_Find(self, key, AStarTable_Slot(self, key));

	if (!it) {
		return 0;
	}
	*out = it;
	return 1;
}

void __fastcall AStarTable_Clear(HashTable *self) {
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
	memset(self->slots, 0, self->numSlots * 4);
}

void __fastcall AStarTable_dtor(HashTable *self) {
	AStarTable_Clear(self);
	operator_delete_array(self->slots);
	self->numSlots = 0;
	self->slots = NULL;
}

/* the item of key added unless it is there (the inline Add of the callers, without the warning) */
static void AStarTable_AddNew(HashTable *t, int key, const cAStarInfo *v) {
	CHECK(!t->locked);
	uint32_t slot = AStarTable_Slot(t, key);
	if (!AStarTable_Find(t, key, slot)) {
		AStarTable_Insert(t, key, slot, v);
	}
}

/* ---- the search ---- */

/* the graph: CNodesWorld or CNodesWorld2D, the vtable of CNodesWorldSup */
#define AS_NODES(self) ((CNodesWorld *)(self)->nodes)

HashTable *__thiscall AStarTable_ctor(HashTable *self, int size) {
	CHECK(size > 0);
	self->numSlots = size;
	self->slots = New_Array(size, 4);
	memset(self->slots, 0, size * 4);
	self->locked = 0;
	return self;
}

CAStarSup *__fastcall CAStarSup_ctor(CAStarSup *self) {
	self->vftable = CASTARSUP_VFTABLE;
	HashTable *t = operator_new(0xc);
	if (t) {
		t->numSlots = 0x3f1;
		t->slots = New_Array(0x3f1, 4);
		memset(t->slots, 0, 0x3f1 * 4);
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

void __fastcall CAStarSup_dtor(CAStarSup *self) {
	HashTable *t = self->table;

	self->vftable = CASTARSUP_VFTABLE;
	if (t) {
		AStarTable_dtor(t);
		operator_delete_sized(t, 0xc);
	}
}

CAStarSup *__thiscall CAStarSup_Delete(CAStarSup *self, byte flags) {
	CAStarSup_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, 0x44);
	}
	return self;
}

void __thiscall CAStarSup_Draw(CAStarSup *self, uint32_t color) {
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
		int n = open->vftable->Count(open);
		for (int i = 1; i <= n; i++) {
			void **e;
			open->vftable->Get(open, &e, i);
			const vec3_t *p = ((ElemAStar *)*e)->node;
			if (p) {
				DebugUtils_DrawSphere(p, 50.0f, 0xff00ff, 2, 2);
			}
		}
	}
	DebugUtils_DrawSphere(&self->start, 50.0f, 0xff00, 2, 2);
	DebugUtils_DrawSphere(&self->goal, 50.0f, 0xff0000, 2, 2);
}

HashTable *__thiscall CAStarSup_GetTable(CAStarSup *self) {
	return self->table;
}

byte __thiscall CAStarSup_Search(CAStarSup *self, cNWVecList *path, int stupid, byte *ok, byte *found, int *state) {
	const CAStarVtbl *vt = self->vftable;
	cBinaryHeap *open;
	const vec3_t *goalPt;
	int n, file;

	*ok = 1;
	*found = 1;
	*state = 0;
	DListVec3_Clear(path);
	vt->Reset(self);
	if ((self->start.x == self->goal.x && self->start.y == self->goal.y && self->start.z == self->goal.z)
		|| vt->TestPath(self, &self->goal, &self->start)) {
		DListVec3_Add(path, &self->goal);
		DListVec3_Add(path, &self->start);
		return 1;
	}
	n = 0;
	vt->PointsVis(self, &self->goal, &goalPt, 1, &n);
	if (n == 0) {
		goto lost;
	}
	file = vt->FileOf(self, goalPt);
	vec3_t startCopy = self->start;
	ElemAStar first;
	first.node = NULL;
	first.parent = NULL;
	first.g = 0.0f;
	first.f = vt->Heuristic(self, &self->start);
	first.info = NULL;
	if (!vt->AddElem(self, &first)) {
		goto fail;
	}
	ElemAStar *top = &self->list[self->numList - 1];
	open = self->open;
	if (!open->vftable->Push(open, (void *const *)&top)) {
		goto fail;
	}
	self->numList--;
	while (self->open->vftable->Count(self->open) > 0) {
		const vec3_t *buf[50];
		vec3_t curPos, parentPos;
		void **tp;
		int count, taken;

		open = self->open;
		open->vftable->Top(open, &tp);
		ElemAStar *cur = *tp;
		curPos = cur->node ? *cur->node : self->start;
		if (cur->parent) {
			parentPos = *cur->parent->node;
		} else {
			parentPos.x = -3.40282347e+38f;
			parentPos.y = -3.40282347e+38f;
			parentPos.z = -3.40282347e+38f;
		}
		open->vftable->Pop(open);
		if (vt->TestPath(self, &self->goal, &curPos)) {
			if (cur->node) {
				vt->CloseAdd(self, &cur);
				vt->BuildPath(self, path, cur);
			}
			DListVec3_Add(path, &self->start);
			if ((byte)Vec3_Differ(&self->start.x, &startCopy.x)) {
				DListVec3_Add(path, &startCopy);
			}
			return 1;
		}
		count = 0;
		if (!cur->node) {
			vt->PointsVisFile(self, &self->start, buf, 50, &count, state, file);
			if (count == 0) {
				goto fail;
			}
		} else {
			vt->OtherLinks(self, cur->node, &parentPos, buf, 50, &count);
		}
		vt->CloseAdd(self, &cur);
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
			float g = vt->Dist(self, &curPos, nb) + cur->g;
			isOpen = 0;
			isClosed = 0;
			el = NULL;
			if (vt->GetElem(self, nb, &el, &info)) {
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
			float h = vt->Heuristic(self, nb);
			el->parent = parent;
			el->f = h + g;
			el->g = g;
			open = self->open;
			if (isOpen) {
				open->vftable->Fix(open, info->value.index);
			} else {
				if (isClosed) {
					vt->CloseRemove(self, info->value.index);
				}
				open = self->open;
				if (!open->vftable->Push(open, (void *const *)&el)) {
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

void __thiscall CAStarSup_BuildPath(CAStarSup *self, cNWVecList *path, const ElemAStar *elem) {
	(void)self;
	DListVec3_Add(path, elem->node);
	for (const ElemAStar *p = elem->parent; p; p = p->parent) {
		const vec3_t *v = p->node;
		cNWVecNode *node = DList_NewNode(path, sizeof(*node));
		node->v = *v;
		DList_LinkFirst(path, node);
	}
}

float __thiscall CAStarSup_Heuristic(CAStarSup *self, const vec3_t *p) {
	float dx = self->goal.x - p->x;
	float dy = self->goal.y - p->y;
	float dz = self->goal.z - p->z;

	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

float __thiscall CAStarSup_Dist(CAStarSup *self, const vec3_t *a, const vec3_t *b) {
	float dx = b->x - a->x;
	float dy = b->y - a->y;
	float dz = b->z - a->z;

	(void)self;
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

byte __thiscall CAStarSup_GetElem(CAStarSup *self, const vec3_t *node, ElemAStar **out, cAStarItem **info) {
	*out = NULL;
	byte r = self->vftable->TableGet(self, node, info);
	if (!r) {
		return r;
	}
	cAStarItem *it = *info;
	if (it->value.state == 1) {
		void **e;
		self->open->vftable->Get(self->open, &e, it->value.index);
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

byte __thiscall CAStarSup_AddElem(CAStarSup *self, const ElemAStar *e) {
	if (self->numList >= self->capList) {
		return 0;
	}
	self->list[self->numList] = *e;
	self->numList++;
	return 1;
}

byte __thiscall CAStarSup_CloseAdd(CAStarSup *self, ElemAStar *const *e) {
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

byte __thiscall CAStarSup_CloseRemove(CAStarSup *self, int i) {
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

byte __thiscall CAStarSup_TableGet(CAStarSup *self, const vec3_t *node, cAStarItem **out) {
	HashTable *t = self->table;
	cAStarItem *it = AStarTable_Find(t, (int)node, AStarTable_Slot(t, (uint32_t)node));

	if (!it) {
		return 0;
	}
	*out = it;
	return 1;
}

void __thiscall CAStarSup_Reset(CAStarSup *self) {
	cBinaryHeap *open;

	for (int i = 0; i < self->numClosed; i++) {
		cAStarInfo *info = self->closed[i]->info;
		if (info) {
			info->state = 2;
		}
	}
	open = self->open;
	int n = open->vftable->Count(open);
	for (int i = 1; i <= n; i++) {
		void **e;
		open = self->open;
		open->vftable->Get(open, &e, i);
		cAStarInfo *info = ((ElemAStar *)*e)->info;
		if (info) {
			info->state = 2;
		}
	}
	self->numClosed = 0;
	open = self->open;
	open->vftable->Clear(open);
}

/* ---- CAStar and CAStar2D ---- */

static inline CAStar *AStar_Common(CAStar2D *self) {
	return (CAStar *)self;
}

/* the lists of the search, one place per point of visibility */
static void AStar_Alloc(CAStar *self) {
	CNodesWorld *nw = AS_NODES(self);
	int n = nw->sup.vftable->NumVisPoints(nw);

	self->base.capList = n + 1;
	self->base.list = Mem_Alloc((n + 1) * sizeof(ElemAStar));
	if (!self->base.list) {
		Con_FatalErrorf("ERROR AI: AStar - Error al crear ListAStar.\n");
	}
	cBinaryHeap *h = operator_new(sizeof(cBinaryHeap));
	if (h) {
		nw = AS_NODES(self);
		h = cBinaryHeap_ctor(h, nw->sup.vftable->NumVisPoints(nw), AStar_ElemLess, AStar_ElemEq, AStar_ElemSetIndex);
	}
	self->base.open = h;
	if (!h) {
		Con_FatalErrorf("ERROR AI: AStar - Error al crear lista Open.\n");
	}
	nw = AS_NODES(self);
	n = nw->sup.vftable->NumVisPoints(nw);
	self->base.capClosed = n + 1;
	self->base.closed = Mem_Alloc((n + 1) * 4);
	if (!self->base.closed) {
		Con_FatalErrorf("ERROR AI: AStar - Error al crear lista Closed.\n");
	}
	CAStar_InitList(self);
}

void *__thiscall CAStar_ctor(void *mem, CNodesWorld *nodes) {
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

void *__thiscall CAStar2D_ctor(void *mem, CNodesWorld2D *nodes) {
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

static void AStar_Free(CAStarSup *self, void *vftable) {
	void *list = self->list;

	self->vftable = vftable;
	Mem_Free(list);
	if (self->open) {
		self->open->vftable->Delete(self->open, 1);
	}
	Mem_Free(self->closed);
	CAStarSup_dtor(self);
}

void __fastcall CAStar_dtor(CAStar *self) {
	AStar_Free(&self->base, CASTAR_VFTABLE);
}

CAStar *__thiscall CAStar_Delete(CAStar *self, byte flags) {
	AStar_Free(&self->base, CASTAR_VFTABLE);
	if (flags & 1) {
		operator_delete_sized(self, 0x48);
	}
	return self;
}

void __fastcall CAStar2D_dtor(CAStar2D *self) {
	AStar_Free(&self->base, CASTAR2D_VFTABLE);
}

CAStar2D *__thiscall CAStar2D_Delete(CAStar2D *self, byte flags) {
	AStar_Free(&self->base, CASTAR2D_VFTABLE);
	if (flags & 1) {
		operator_delete_sized(self, 0x50);
	}
	return self;
}

void __fastcall CAStar_InitList(CAStar *self) {
	CNodesWorld *nw;

	AStarTable_Clear(self->base.table);
	self->base.numList = 0;
	nw = AS_NODES(self);
	for (int i = 0; i < nw->sup.vftable->NumVisPoints(nw); i++) {
		ElemAStar e;
		cAStarInfo info;
		cAStarItem *it;

		e.node = nw->sup.vftable->GetVisPoint(nw, i);
		e.parent = NULL;
		e.f = -1.0f;
		e.g = -1.0f;
		e.info = NULL;
		self->base.vftable->AddElem(&self->base, &e);
		info.index = -1;
		info.state = 2;
		info.elem = self->base.numList - 1;
		AStarTable_AddNew(self->base.table, (int)e.node, &info);
		self->base.vftable->TableGet(&self->base, e.node, &it);
		self->base.list[self->base.numList - 1].info = &it->value;
		nw = AS_NODES(self);
	}
}

void __fastcall CAStar_DrawDebug(CAStar *self) {
	if (g_AStarDrawMode) {
		CAStarSup_Draw(&self->base, 0xff);
	}
}

void __fastcall CAStar2D_DrawDebug(CAStar *self) {
	if (g_AStarDrawMode) {
		CAStarSup_Draw(&self->base, 0xff00ff);
	}
}

byte __thiscall CAStar_TestPath(CAStar *self, const vec3_t *a, const vec3_t *b) {
	return CNodesWorld_TestPath(self->nodes, a, b, self->base.radius, self->base.radius, NULL, 0);
}

byte __thiscall CAStar2D_TestPath(CAStar2D *self, const vec3_t *a, const vec3_t *b) {
	return CNodesWorld2D_TestPath(self->nodes, a, b, self->base.radius, self->base.radius, self->h1, self->h2,
		NULL);
}

void __thiscall CAStar_NearestStart(CAStar *self) {
	vec3_t p = self->base.start;

	CNodesWorld_NearestPointVis(self->nodes, &p, &self->base.start);
}

void __thiscall CAStar2D_NearestStart(CAStar2D *self) {
	vec3_t p = self->base.start;

	CNodesWorld2D_NearestPointVis(self->nodes, &p, &self->base.start);
}

int __thiscall CAStar_FileOf(CAStar *self, const vec3_t *node) {
	CNodesWorld *nw = AS_NODES(self);

	return nw->sup.vftable->FileOf(nw, (const cNWNode *)node);
}

void __thiscall CAStar_OtherLinks(CAStar *self, const vec3_t *node, const vec3_t *pos, const vec3_t **list, int max,
	int *count) {
	CNodesWorld *nw = AS_NODES(self);

	nw->sup.vftable->OtherLinks(nw, (const cNWNode *)node, pos, (cNWNode **)list, max, count);
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

void __thiscall CAStar_PointsVis(CAStar *self, const vec3_t *pos, const vec3_t **list, int max, int *count) {
	AStar_Points(self, 0, pos, list, max, count, 0, 0);
}

void __thiscall CAStar2D_PointsVis(CAStar2D *self, const vec3_t *pos, const vec3_t **list, int max, int *count) {
	AStar_Points(AStar_Common(self), 1, pos, list, max, count, 0, 0);
}

/* the first point of the search in the graph file: the points of visibility, the one the segment sees, else the
 * nearest one of the floor (2D: the distance on the floor plane) */
static void AStar_PointsVisFile(CAStar *self, byte flat, const vec3_t *pos, const vec3_t **list, int max, int *count,
	int *state, int file) {
	const vec3_t *buf[20];
	const vec3_t *best;
	float bestDist;
	int n;

	CHECK(max > 0);
	*state = 0;
	AStar_Points(self, flat, pos, list, max < 7 ? max : 7, count, 0, 0);
	if (*count > 0 && CAStar_FileOf(self, *list) != file) {
		*count = 0;
		return;
	}
	if (*count != 0) {
		return;
	}
	*state = 1;
	AStar_Points(self, flat, pos, list, 1, count, 0, 1);
	if (*count > 0 && CAStar_FileOf(self, *list) != file) {
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
		if (CAStar_FileOf(self, buf[i]) != file) {
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

void __thiscall CAStar_PointsVisFile(CAStar *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
	int *state, int file) {
	AStar_PointsVisFile(self, 0, pos, list, max, count, state, file);
}

void __thiscall CAStar2D_PointsVisFile(CAStar2D *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
	int *state, int file) {
	AStar_PointsVisFile(AStar_Common(self), 1, pos, list, max, count, state, file);
}

byte __thiscall CAStar_FindPath(void *astar, cNWVecList *path, const vec3_t *to, const vec3_t *from, float radius,
	float dist, int stupid, byte *startOk, byte *goalOk, int *state) {
	CAStar *self = astar;
	CNodesWorld *nw = AS_NODES(self);

	(void)dist;
	CHECK(nw->sup.vftable->IsCreated(nw));
	self->base.radius = radius;
	self->base.start = *to;
	self->base.goal = *from;
	return self->base.vftable->Search(&self->base, path, stupid, goalOk, startOk, state);
}

byte __thiscall CAStar2D_FindPath(void *astar, cNWVecList *path, const vec3_t *dest, const vec3_t *pos, float r,
	float dist, float h1, float h2, int soft, byte *noPath, byte *reach, int *res) {
	CAStar2D *self = astar;
	CNodesWorld *nw = AS_NODES(self);

	(void)dist;
	CHECK(nw->sup.vftable->IsCreated(nw));
	self->base.radius = r;
	self->h1 = h1;
	self->h2 = h2;
	self->base.start = *dest;
	self->base.goal = *pos;
	return self->base.vftable->Search(&self->base, path, soft, reach, noPath, res);
}

void __thiscall CAStar_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count, float r) {
	CAStar *self = astar;

	CNodesWorld_GetPointsVis(self->nodes, pos, list, max, count, r, 0.0f, self->base.maxDist, 0, 0);
}

void __thiscall CAStar2D_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count,
	float r, float h1, float h2) {
	CAStar2D *self = astar;

	CNodesWorld2D_GetPointsVis(self->nodes, pos, list, max, count, r, h1, h2, 0.0f, self->base.maxDist, 0, 0);
}
