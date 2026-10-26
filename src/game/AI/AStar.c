/*
 * The containers of the A* search, original sources AI/AStar.cpp and the templates of Global/ (include/scrap/astar.h).
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/astar.h"
#include "scrap/console.h"
#include "scrap/hashtable.h"

#define CBINARYHEAP_VFTABLE 0x3a97c4

cBinaryHeap *__thiscall cBinaryHeap_ctor(cBinaryHeap *self, int capacity, binHeapLess_t less, binHeapEq_t equals,
	binHeapIndex_t setIndex) {
	self->vftable = Game_Ptr(CBINARYHEAP_VFTABLE);
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
	self->vftable = Game_Ptr(CBINARYHEAP_VFTABLE);
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
	int *ref = ((ElemAStar *)*elem)->heapRef;

	if (ref) {
		ref[0] = index;
		ref[1] = 1;
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

/* the table of the search object (its virtual 2) */
static HashTable *AStar_Table(void) {
	void **vt = *(void ***)g_AStar;
	return ((HashTable * (__thiscall *)(void *)) vt[2])(g_AStar);
}

void AStar_AddInfo(int key, const cAStarInfo *v) {
	HashTable *t = AStar_Table();
	CHECK(!t->locked);
	uint32_t slot = AStarTable_Slot(t, key);
	if (!AStarTable_Find(t, key, slot)) {
		AStarTable_Insert(t, key, slot, v);
	}
}

byte AStar_GetInfo(int key, cAStarItem **out) {
	HashTable *t = AStar_Table();
	cAStarItem *it = AStarTable_Find(t, key, AStarTable_Slot(t, key));

	if (!it) {
		return 0;
	}
	*out = it;
	return 1;
}
