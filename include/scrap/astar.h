/*
 * The containers of the A* path search (AI/AStar.cpp): cBinaryHeap<ElemAStar*>, the open list, and the hash table
 * of the searched nodes (Global/HashTable.h with int keys). The search itself is in the AStar classes.
 */
#ifndef SCRAP_ASTAR_H
#define SCRAP_ASTAR_H

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
SD_SIZE(ElemAStar, 0x14);

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
/* the only instance */
typedef cBinaryHeap cBinaryHeap_ElemAStarPtr;

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

/* the table of the searched nodes, by point */
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

/* ---- the search: CAStarSup with CAStar (CNodesWorld) and CAStar2D (CNodesWorld2D) ---- */

typedef struct CAStarSup CAStarSup;
typedef struct CAStarVtbl {
	CAStarSup *(__thiscall *Delete)(CAStarSup *self, byte flags);					/* 0x00 */
	/* the spheres of the closed and open nodes (g_AStarDrawMode) and of the ends; color not used */
	void(__thiscall *Draw)(CAStarSup *self, uint32_t color);					/* 0x04 */
	HashTable *(__thiscall *GetTable)(CAStarSup *self);					/* 0x08 */
	byte(__thiscall *TestPath)(CAStarSup *self, const vec3_t *a, const vec3_t *b);		/* 0x0c */
	/* the start moved to the nearest point of visibility */
	void(__thiscall *NearestStart)(CAStarSup *self);						/* 0x10 */
	void(__thiscall *PointsVis)(CAStarSup *self, const vec3_t *pos, const vec3_t **list, int max, int *count);	/* 0x14 */
	/* the points of visibility of pos in the graph file file: *state 0 the normal search, 1 the segment only, 2
	 * the nearest of the floor */
	void(__thiscall *PointsVisFile)(CAStarSup *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
		int *state, int file);								/* 0x18 */
	void(__thiscall *OtherLinks)(CAStarSup *self, const vec3_t *node, const vec3_t *pos, const vec3_t **list,
		int max, int *count);								/* 0x1c */
	byte(__thiscall *Search)(CAStarSup *self, cNWVecList *path, int stupid, byte *ok, byte *found, int *state);	/* 0x20 */
	/* the points of elem and its parents in front of path */
	void(__thiscall *BuildPath)(CAStarSup *self, cNWVecList *path, const ElemAStar *elem);	/* 0x24 */
	float(__thiscall *Heuristic)(CAStarSup *self, const vec3_t *p);				/* 0x28 */
	float(__thiscall *Dist)(CAStarSup *self, const vec3_t *a, const vec3_t *b);		/* 0x2c */
	byte(__thiscall *GetElem)(CAStarSup *self, const vec3_t *node, ElemAStar **out, cAStarItem **info);	/* 0x30 */
	int(__thiscall *FileOf)(CAStarSup *self, const vec3_t *node);				/* 0x34 */
	byte(__thiscall *AddElem)(CAStarSup *self, const ElemAStar *e);				/* 0x38 */
	byte(__thiscall *CloseAdd)(CAStarSup *self, ElemAStar *const *e);				/* 0x3c */
	byte(__thiscall *CloseRemove)(CAStarSup *self, int i);					/* 0x40 */
	byte(__thiscall *TableGet)(CAStarSup *self, const vec3_t *node, cAStarItem **out);		/* 0x44 */
	/* the states of the closed and open nodes to 2, the lists emptied */
	void(__thiscall *Reset)(CAStarSup *self);							/* 0x48 */
} CAStarVtbl;
SD_OFFSET(CAStarVtbl, Reset, 0x48);

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
SD_OFFSET(CAStarSup, radius, 0x40);
SD_SIZE(CAStarSup, 0x44);		/* CAStarSup_Delete */

/* the search on CNodesWorld (0x48 bytes, SAIPy.c) */
typedef struct CAStar {
	CAStarSup base;			/* 0x00 */
	CNodesWorld *nodes;		/* 0x44 */
} CAStar;
SD_SIZE(CAStar, 0x48);

/* the search on CNodesWorld2D (0x50 bytes, SAIPy.c) with the heights of the body for the tests on the floor. The
 * graph is at 0x44 as in CAStar, the code shared by both classes takes it as a CAStar */
typedef struct CAStar2D {
	CAStarSup base;			/* 0x00 */
	struct CNodesWorld2D *nodes;	/* 0x44 */
	float h1;			/* 0x48 */
	float h2;			/* 0x4c */
} CAStar2D;
SD_OFFSET(CAStar2D, nodes, 0x44);
SD_OFFSET(CAStar2D, h1, 0x48);
SD_SIZE(CAStar2D, 0x50);

#define g_AStarDrawMode GAME_GLOBAL_IN(int, GVar_ShowAStarAI, 16, 0x444c90)	/* 1 closed, 2 open, 3 both */

/* the table of 1009 slots of the search (Global/HashTable.h with int keys) */
HashTable *__thiscall AStarTable_ctor(HashTable *self, int size);

/* CAStarSup, the base class: the table, the ends zeroed, maxDist */
CAStarSup *__fastcall CAStarSup_ctor(CAStarSup *self);
void __fastcall CAStarSup_dtor(CAStarSup *self);
CAStarSup *__thiscall CAStarSup_Delete(CAStarSup *self, byte flags);
void __thiscall CAStarSup_Draw(CAStarSup *self, uint32_t color);
HashTable *__thiscall CAStarSup_GetTable(CAStarSup *self);
/* the A* from start to goal over the points of visibility; path: goal first, the start last. ok 0 when the lists are
 * full or the start has no points, found 0 when the goal has no point or the graph has no way */
byte __thiscall CAStarSup_Search(CAStarSup *self, cNWVecList *path, int stupid, byte *ok, byte *found, int *state);
void __thiscall CAStarSup_BuildPath(CAStarSup *self, cNWVecList *path, const ElemAStar *elem);
float __thiscall CAStarSup_Heuristic(CAStarSup *self, const vec3_t *p);
float __thiscall CAStarSup_Dist(CAStarSup *self, const vec3_t *a, const vec3_t *b);
byte __thiscall CAStarSup_GetElem(CAStarSup *self, const vec3_t *node, ElemAStar **out, cAStarItem **info);
byte __thiscall CAStarSup_AddElem(CAStarSup *self, const ElemAStar *e);
byte __thiscall CAStarSup_CloseAdd(CAStarSup *self, ElemAStar *const *e);
byte __thiscall CAStarSup_CloseRemove(CAStarSup *self, int i);
byte __thiscall CAStarSup_TableGet(CAStarSup *self, const vec3_t *node, cAStarItem **out);
void __thiscall CAStarSup_Reset(CAStarSup *self);

/* CAStar and CAStar2D: CAStar_ctor and CAStar2D_ctor are in ai.h */
void __fastcall CAStar_dtor(CAStar *self);
CAStar *__thiscall CAStar_Delete(CAStar *self, byte flags);
void __fastcall CAStar2D_dtor(CAStar2D *self);
CAStar2D *__thiscall CAStar2D_Delete(CAStar2D *self, byte flags);
/* the element and the table item of every point of visibility (both classes) */
void __fastcall CAStar_InitList(CAStar *self);
/* Draw when g_AStarDrawMode is on */
void __fastcall CAStar_DrawDebug(CAStar *self);
void __fastcall CAStar2D_DrawDebug(CAStar *self);
byte __thiscall CAStar_TestPath(CAStar *self, const vec3_t *a, const vec3_t *b);
byte __thiscall CAStar2D_TestPath(CAStar2D *self, const vec3_t *a, const vec3_t *b);
void __thiscall CAStar_NearestStart(CAStar *self);
void __thiscall CAStar2D_NearestStart(CAStar2D *self);
void __thiscall CAStar_PointsVis(CAStar *self, const vec3_t *pos, const vec3_t **list, int max, int *count);
void __thiscall CAStar2D_PointsVis(CAStar2D *self, const vec3_t *pos, const vec3_t **list, int max, int *count);
void __thiscall CAStar_PointsVisFile(CAStar *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
	int *state, int file);
void __thiscall CAStar2D_PointsVisFile(CAStar2D *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
	int *state, int file);
/* the slots that go on to the graph (both classes) */
int __thiscall CAStar_FileOf(CAStar *self, const vec3_t *node);
void __thiscall CAStar_OtherLinks(CAStar *self, const vec3_t *node, const vec3_t *pos, const vec3_t **list, int max,
	int *count);
/* CAStar_FindPath and CAStar_GetPointsVis: nodesworld.h; CAStar2D_FindPath and CAStar2D_GetPointsVis:
 * aicharcontrol.h */

#endif
