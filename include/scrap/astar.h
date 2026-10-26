/*
 * The containers of the A* path search (AI/AStar.cpp): cBinaryHeap<ElemAStar*>, the open list, and the hash table
 * of the searched nodes (Global/HashTable.h with int keys). The search itself is in the AStar classes.
 */
#ifndef SCRAP_ASTAR_H
#define SCRAP_ASTAR_H

#include "scrap/game.h"

/* a node in the search */
typedef struct ElemAStar {
	int node;			/* 0x00 what the equal callback compares */
	int pad_4;
	float f;			/* 0x08 the cost the heap sorts by */
	int pad_c;
	int *heapRef;			/* 0x10 index and flag set when it moves in the heap */
} ElemAStar;

/* the callbacks get pointers to the elements in the heap (cdecl) */
typedef byte (*binHeapLess_t)(void *const *a, void *const *b);
typedef byte (*binHeapEq_t)(void *const *a, const void *b);
typedef void (*binHeapIndex_t)(void **elem, int index);

typedef struct cBinaryHeap cBinaryHeap;
typedef struct cBinaryHeapVtbl {
	cBinaryHeap *(__thiscall *Delete)(cBinaryHeap *self, byte flags);
	void(__thiscall *Clear)(cBinaryHeap *self);
	byte(__thiscall *Push)(cBinaryHeap *self, void *const *item);
	byte(__thiscall *Top)(cBinaryHeap *self, void ***out);
	byte(__thiscall *Pop)(cBinaryHeap *self);
	byte(__thiscall *Get)(cBinaryHeap *self, void ***out, int i);
	byte(__thiscall *Find)(cBinaryHeap *self, const void *item, int *index, void ***out);
	byte(__thiscall *Set)(cBinaryHeap *self, void *const *item, int i);
	int(__thiscall *Count)(cBinaryHeap *self);
	void(__thiscall *Fix)(cBinaryHeap *self, int i);
	byte(__thiscall *SiftUp)(cBinaryHeap *self, int i);
	byte(__thiscall *SiftDown)(cBinaryHeap *self, int i);
} cBinaryHeapVtbl;

struct cBinaryHeap {
	const cBinaryHeapVtbl *vftable;	/* 0x00 */
	void **elems;			/* 0x04 elems[1..count] */
	int count;			/* 0x08 */
	int capacity;			/* 0x0c */
	binHeapLess_t less;		/* 0x10 a goes before b */
	binHeapEq_t equals;		/* 0x14 */
	binHeapIndex_t setIndex;	/* 0x18 may be NULL */
};
SD_SIZE(cBinaryHeap, 0x1c);

cBinaryHeap *__thiscall cBinaryHeap_ctor(cBinaryHeap *self, int capacity, binHeapLess_t less, binHeapEq_t equals,
	binHeapIndex_t setIndex);
void __fastcall cBinaryHeap_dtor(cBinaryHeap *self);
cBinaryHeap *__thiscall cBinaryHeap_Delete(cBinaryHeap *self, byte flags);
void __thiscall cBinaryHeap_Clear(cBinaryHeap *self);
byte __thiscall cBinaryHeap_Push(cBinaryHeap *self, void *const *item);
byte __thiscall cBinaryHeap_Top(cBinaryHeap *self, void ***out);
byte __thiscall cBinaryHeap_Pop(cBinaryHeap *self);
byte __thiscall cBinaryHeap_Get(cBinaryHeap *self, void ***out, int i);
byte __thiscall cBinaryHeap_Find(cBinaryHeap *self, const void *item, int *index, void ***out);
byte __thiscall cBinaryHeap_Set(cBinaryHeap *self, void *const *item, int i);
/* 4 bytes in the exe, too short for the hook jump: only for the C callers */
int __thiscall cBinaryHeap_Count(cBinaryHeap *self);
void __thiscall cBinaryHeap_Fix(cBinaryHeap *self, int i);
/* 1 if the element moved */
byte __thiscall cBinaryHeap_SiftUp(cBinaryHeap *self, int i);
byte __thiscall cBinaryHeap_SiftDown(cBinaryHeap *self, int i);

byte AStar_ElemLess(void *const *a, void *const *b);
byte AStar_ElemEq(void *const *a, const void *b);
void AStar_ElemSetIndex(void **elem, int index);

/* the table of the searched nodes, by node number */
typedef struct cAStarInfo {
	int i0, i1, i2;
} cAStarInfo;

typedef struct cAStarItem {
	cAStarInfo value;		/* 0x00 */
	int key;			/* 0x0c */
	struct cAStarItem *next;	/* 0x10 */
} cAStarItem;
SD_SIZE(cAStarItem, 0x14);

/* the number hash of HashTable.h: the nibbles from the low end */
uint32_t __thiscall AStarTable_Slot(HashTable *self, uint32_t key);
cAStarItem *__thiscall AStarTable_Find(HashTable *self, int key, uint32_t slot);
cAStarItem *__thiscall AStarTable_Insert(HashTable *self, int key, uint32_t slot, const cAStarInfo *v);
byte __thiscall AStarTable_Add(HashTable *self, int key, const cAStarInfo *v, byte warn);
byte __thiscall AStarTable_Get(HashTable *self, int key, cAStarItem **out);
/* the items freed, the slots kept */
void __fastcall AStarTable_Clear(HashTable *self);
void __fastcall AStarTable_dtor(HashTable *self);
/* the same on the table of g_AStar */
void AStar_AddInfo(int key, const cAStarInfo *v);
byte AStar_GetInfo(int key, cAStarItem **out);

#endif
