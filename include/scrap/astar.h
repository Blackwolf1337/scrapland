/*
 * The containers of the A* path search (AI/AStar.cpp): cBinaryHeap<ElemAStar*>, the open list, and the hash table
 * of the searched nodes (Global/HashTable.h with int keys). The search itself is in the AStar classes.
 */
#ifndef SCRAP_ASTAR_H
#define SCRAP_ASTAR_H

#include "scrap/gvar.h"
#include "scrap/game.h"
#include "scrap/nodesworld.h"

/* the item of a point in the table of the searched nodes (by the point address) */
typedef struct cAStarInfo {
	int index;			/* 0x0 the place in the open heap (state 1) or in the closed list (state 0) */
	int state;			/* 0x4 0 closed, 1 open, 2 not reached */
	int elem;			/* 0x8 its element in the list of the search */
} cAStarInfo;

/* a node in the search */
typedef struct ElemAStar {
	const vec3_t *node;		/* 0x00 the point of visibility (a cNWNode), NULL for the start */
	struct ElemAStar *parent;	/* 0x04 */
	float f;			/* 0x08 g plus the estimate to the goal, what the heap sorts by */
	float g;			/* 0x0c the cost from the start */
	cAStarInfo *info;		/* 0x10 its item in the table, index and state set when it moves in the heap */
} ElemAStar;

/* the callbacks get pointers to the elements in the heap (cdecl) */
typedef byte (*binHeapLess_t)(void *const *a, void *const *b);
typedef byte (*binHeapEq_t)(void *const *a, const void *b);
typedef void (*binHeapIndex_t)(void **elem, int index);

typedef struct cBinaryHeap cBinaryHeap;
typedef struct cBinaryHeapVtbl {
	cBinaryHeap *(*Delete)(cBinaryHeap *self, byte flags);
	void(*Clear)(cBinaryHeap *self);
	byte(*Push)(cBinaryHeap *self, void *const *item);
	byte(*Top)(cBinaryHeap *self, void ***out);
	byte(*Pop)(cBinaryHeap *self);
	byte(*Get)(cBinaryHeap *self, void ***out, int i);
	byte(*Find)(cBinaryHeap *self, const void *item, int *index, void ***out);
	byte(*Set)(cBinaryHeap *self, void *const *item, int i);
	int(*Count)(cBinaryHeap *self);
	void(*Fix)(cBinaryHeap *self, int i);
	byte(*SiftUp)(cBinaryHeap *self, int i);
	byte(*SiftDown)(cBinaryHeap *self, int i);
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
/* the only instance */
typedef cBinaryHeap cBinaryHeap_ElemAStarPtr;

cBinaryHeap *cBinaryHeap_ctor(cBinaryHeap *self, int capacity, binHeapLess_t less, binHeapEq_t equals,
	binHeapIndex_t setIndex);
void cBinaryHeap_dtor(cBinaryHeap *self);
cBinaryHeap *cBinaryHeap_Delete(cBinaryHeap *self, byte flags);
void cBinaryHeap_Clear(cBinaryHeap *self);
byte cBinaryHeap_Push(cBinaryHeap *self, void *const *item);
byte cBinaryHeap_Top(cBinaryHeap *self, void ***out);
byte cBinaryHeap_Pop(cBinaryHeap *self);
byte cBinaryHeap_Get(cBinaryHeap *self, void ***out, int i);
byte cBinaryHeap_Find(cBinaryHeap *self, const void *item, int *index, void ***out);
byte cBinaryHeap_Set(cBinaryHeap *self, void *const *item, int i);
/* 4 bytes in the exe, too short for the hook jump: only for the C callers */
int cBinaryHeap_Count(cBinaryHeap *self);
void cBinaryHeap_Fix(cBinaryHeap *self, int i);
/* 1 if the element moved */
byte cBinaryHeap_SiftUp(cBinaryHeap *self, int i);
byte cBinaryHeap_SiftDown(cBinaryHeap *self, int i);

byte AStar_ElemLess(void *const *a, void *const *b);
byte AStar_ElemEq(void *const *a, const void *b);
void AStar_ElemSetIndex(void **elem, int index);

/* the table of the searched nodes, by point */
typedef struct cAStarItem {
	cAStarInfo value;		/* 0x00 */
	intptr_t key;			/* 0x0c the node */
	struct cAStarItem *next;	/* 0x10 */
} cAStarItem;

/* the number hash of HashTable.h: the nibbles from the low end */
uint32_t AStarTable_Slot(HashTable *self, uintptr_t key);
cAStarItem *AStarTable_Find(HashTable *self, intptr_t key, uint32_t slot);
cAStarItem *AStarTable_Insert(HashTable *self, intptr_t key, uint32_t slot, const cAStarInfo *v);
byte AStarTable_Add(HashTable *self, intptr_t key, const cAStarInfo *v, byte warn);
byte AStarTable_Get(HashTable *self, intptr_t key, cAStarItem **out);
/* the items freed, the slots kept */
void AStarTable_Clear(HashTable *self);
void AStarTable_dtor(HashTable *self);

/* ---- the search: CAStarSup with CAStar (CNodesWorld) and CAStar2D (CNodesWorld2D) ---- */

typedef struct CAStarSup CAStarSup;
typedef struct CAStarVtbl {
	CAStarSup *(*Delete)(CAStarSup *self, byte flags);					/* 0x00 */
	/* the spheres of the closed and open nodes (g_AStarDrawMode) and of the ends; color not used */
	void(*Draw)(CAStarSup *self, uint32_t color);					/* 0x04 */
	HashTable *(*GetTable)(CAStarSup *self);					/* 0x08 */
	byte(*TestPath)(CAStarSup *self, const vec3_t *a, const vec3_t *b);		/* 0x0c */
	/* the start moved to the nearest point of visibility */
	void(*NearestStart)(CAStarSup *self);						/* 0x10 */
	void(*PointsVis)(CAStarSup *self, const vec3_t *pos, const vec3_t **list, int max, int *count);	/* 0x14 */
	/* the points of visibility of pos in the graph file file: *state 0 the normal search, 1 the segment only, 2
	 * the nearest of the floor */
	void(*PointsVisFile)(CAStarSup *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
		int *state, int file);								/* 0x18 */
	void(*OtherLinks)(CAStarSup *self, const vec3_t *node, const vec3_t *pos, const vec3_t **list,
		int max, int *count);								/* 0x1c */
	byte(*Search)(CAStarSup *self, cNWVecList *path, int stupid, byte *ok, byte *found, int *state);	/* 0x20 */
	/* the points of elem and its parents in front of path */
	void(*BuildPath)(CAStarSup *self, cNWVecList *path, const ElemAStar *elem);	/* 0x24 */
	float(*Heuristic)(CAStarSup *self, const vec3_t *p);				/* 0x28 */
	float(*Dist)(CAStarSup *self, const vec3_t *a, const vec3_t *b);		/* 0x2c */
	byte(*GetElem)(CAStarSup *self, const vec3_t *node, ElemAStar **out, cAStarItem **info);	/* 0x30 */
	int(*FileOf)(CAStarSup *self, const vec3_t *node);				/* 0x34 */
	byte(*AddElem)(CAStarSup *self, const ElemAStar *e);				/* 0x38 */
	byte(*CloseAdd)(CAStarSup *self, ElemAStar *const *e);				/* 0x3c */
	byte(*CloseRemove)(CAStarSup *self, int i);					/* 0x40 */
	byte(*TableGet)(CAStarSup *self, const vec3_t *node, cAStarItem **out);		/* 0x44 */
	/* the states of the closed and open nodes to 2, the lists emptied */
	void(*Reset)(CAStarSup *self);							/* 0x48 */
} CAStarVtbl;

struct CAStarSup {
	const CAStarVtbl *vftable;	/* 0x00 */
	float maxDist;			/* 0x04 1.25 * f40 of g_NodesWorld: the distance of the point searches */
	HashTable *table;		/* 0x08 1009 slots, the item of each point of visibility */
	ElemAStar *list;		/* 0x0c an element per point of visibility (Mem_Alloc) */
	int numList;			/* 0x10 */
	int capList;			/* 0x14 */
	cBinaryHeap *open;		/* 0x18 */
	ElemAStar **closed;		/* 0x1c (Mem_Alloc) */
	int numClosed;			/* 0x20 */
	int capClosed;			/* 0x24 */
	vec3_t start;			/* 0x28 */
	vec3_t goal;			/* 0x34 */
	float radius;			/* 0x40 */
};

/* the search on CNodesWorld (0x48 bytes, SAIPy.c) */
typedef struct CAStar {
	CAStarSup base;			/* 0x00 */
	CNodesWorld *nodes;		/* 0x44 */
} CAStar;

/* the search on CNodesWorld2D (0x50 bytes, SAIPy.c) with the heights of the body for the tests on the floor. The
 * graph is at 0x44 as in CAStar, the code shared by both classes takes it as a CAStar */
typedef struct CAStar2D {
	CAStarSup base;			/* 0x00 */
	struct CNodesWorld2D *nodes;	/* 0x44 */
	float h1;			/* 0x48 */
	float h2;			/* 0x4c */
} CAStar2D;

extern cGVar GVar_ShowAStarAI;
#define g_AStarDrawMode (GVar_ShowAStarAI.value.i)	/* 1 closed, 2 open, 3 both */

/* the table of 1009 slots of the search (Global/HashTable.h with int keys) */
HashTable *AStarTable_ctor(HashTable *self, int size);

/* CAStarSup, the base class: the table, the ends zeroed, maxDist */
CAStarSup *CAStarSup_ctor(CAStarSup *self);
void CAStarSup_dtor(CAStarSup *self);
CAStarSup *CAStarSup_Delete(CAStarSup *self, byte flags);
void CAStarSup_Draw(CAStarSup *self, uint32_t color);
HashTable *CAStarSup_GetTable(CAStarSup *self);
/* the A* from start to goal over the points of visibility; path: goal first, the start last. ok 0 when the lists are
 * full or the start has no points, found 0 when the goal has no point or the graph has no way */
byte CAStarSup_Search(CAStarSup *self, cNWVecList *path, int stupid, byte *ok, byte *found, int *state);
void CAStarSup_BuildPath(CAStarSup *self, cNWVecList *path, const ElemAStar *elem);
float CAStarSup_Heuristic(CAStarSup *self, const vec3_t *p);
float CAStarSup_Dist(CAStarSup *self, const vec3_t *a, const vec3_t *b);
byte CAStarSup_GetElem(CAStarSup *self, const vec3_t *node, ElemAStar **out, cAStarItem **info);
byte CAStarSup_AddElem(CAStarSup *self, const ElemAStar *e);
byte CAStarSup_CloseAdd(CAStarSup *self, ElemAStar *const *e);
byte CAStarSup_CloseRemove(CAStarSup *self, int i);
byte CAStarSup_TableGet(CAStarSup *self, const vec3_t *node, cAStarItem **out);
void CAStarSup_Reset(CAStarSup *self);

/* CAStar and CAStar2D: CAStar_ctor and CAStar2D_ctor are in ai.h */
void CAStar_dtor(CAStar *self);
CAStarSup *CAStar_Delete(CAStarSup *elem, byte flags);
void CAStar2D_dtor(CAStar2D *self);
CAStarSup *CAStar2D_Delete(CAStarSup *elem, byte flags);
/* the element and the table item of every point of visibility (both classes) */
void CAStar_InitList(CAStar *self);
/* Draw when g_AStarDrawMode is on */
void CAStar_DrawDebug(CAStar *self);
void CAStar2D_DrawDebug(CAStar *self);
byte CAStar_TestPath(CAStarSup *elem, const vec3_t *a, const vec3_t *b);
byte CAStar2D_TestPath(CAStarSup *elem, const vec3_t *a, const vec3_t *b);
void CAStar_NearestStart(CAStarSup *elem);
void CAStar2D_NearestStart(CAStarSup *elem);
void CAStar_PointsVis(CAStarSup *base, const vec3_t *pos, const vec3_t **list, int max, int *count);
void CAStar2D_PointsVis(CAStarSup *base, const vec3_t *pos, const vec3_t **list, int max, int *count);
void CAStar_PointsVisFile(CAStarSup *base, const vec3_t *pos, const vec3_t **list, int max, int *count, int *state, int file);
void CAStar2D_PointsVisFile(CAStarSup *base, const vec3_t *pos, const vec3_t **list, int max, int *count, int *state, int file);
/* the slots that go on to the graph (both classes) */
int CAStar_FileOf(CAStarSup *base, const vec3_t *node);
void CAStar_OtherLinks(CAStarSup *base, const vec3_t *node, const vec3_t *pos, const vec3_t **list, int max, int *count);
/* CAStar_FindPath and CAStar_GetPointsVis: nodesworld.h; CAStar2D_FindPath and CAStar2D_GetPointsVis:
 * aicharcontrol.h */

#endif
