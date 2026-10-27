/*
 * AI/NodesWorld.cpp of the game: CNodesWorld, the 3D visibility graph of a level. The base class CNodesWorldSup
 * (rva 0xb80f0 and on, another source) loads the graph: the nodes with their links (0x1a8) and the points of
 * visibility in a grid of cells (0x18c), which the searches walk ring by ring around the cell of a position.
 * CNodesWorld adds the file names of the graph, the path and visibility queries on the points, the analysis of
 * the graph (AnalizeGraph, the "GRAFO" texts) and of the level (Analize), and the Global/DList.h instances the
 * generation of the graph uses.
 */
#ifndef SCRAP_NODESWORLD_H
#define SCRAP_NODESWORLD_H

#include "scrap/dlist.h"

typedef struct CNodesWorld CNodesWorld;
struct CColisTri;

/* ---- Global/DList.h instances ---- */

/* a node with one int or pointer (0xc bytes) */
typedef struct cNWIntNode {
	struct cNWIntNode *next;	/* 0x0 */
	struct cNWIntNode *prev;	/* 0x4 */
	void *data;			/* 0x8 */
} cNWIntNode;
SD_SIZE(cNWIntNode, 0xc);
typedef DLIST(cNWIntNode) cNWIntList;

/* a node with 12 bytes: a point, or a cell index with two pointers (0x14 bytes) */
typedef struct cNWVecNode {
	struct cNWVecNode *next;	/* 0x00 */
	struct cNWVecNode *prev;	/* 0x04 */
	vec3_t v;			/* 0x08 */
} cNWVecNode;
SD_SIZE(cNWVecNode, 0x14);
typedef DLIST(cNWVecNode) cNWVecList;

/* a segment of the debug view of the path tests (0x20 bytes) */
typedef struct cNWSegNode {
	struct cNWSegNode *next;	/* 0x00 */
	struct cNWSegNode *prev;	/* 0x04 */
	vec3_t a, b;			/* 0x08 */
} cNWSegNode;
SD_SIZE(cNWSegNode, 0x20);
typedef DLIST(cNWSegNode) cNWSegList;

/* a point of visibility of the generation, with the points it sees (0x44 bytes) */
typedef struct cNWPointNode {
	struct cNWPointNode *next;	/* 0x00 */
	struct cNWPointNode *prev;	/* 0x04 */
	vec3_t pos;			/* 0x08 */
	cNWIntList links;		/* 0x14 the pos of the other points */
	vec3_t boxA;			/* 0x2c */
	vec3_t boxB;			/* 0x38 */
} cNWPointNode;
SD_SIZE(cNWPointNode, 0x44);
typedef DLIST(cNWPointNode) cNWPointList;

/* a triangle with an inverted normal found by Analize (0x44 bytes, 15 words of data) */
typedef struct cNWTriNode {
	struct cNWTriNode *next;	/* 0x00 */
	struct cNWTriNode *prev;	/* 0x04 */
	float data[15];			/* 0x08 */
} cNWTriNode;
SD_SIZE(cNWTriNode, 0x44);
typedef DLIST(cNWTriNode) cNWTriList;

/* the box the generation fills (FLT_MAX first, -FLT_MAX after) */
typedef struct cNWBox {
	vec3_t a, b;
} cNWBox;

/* ---- the graph ---- */

/* a node of the loaded graph */
typedef struct cNWNode {
	vec3_t pos;			/* 0x00 */
	int numLinks;			/* 0x0c */
	struct cNWNode **links;		/* 0x10 */
} cNWNode;
SD_SIZE(cNWNode, 0x14);

/* the cells of the points of visibility (Global/GridElements.h with the pos pointers of the points) */
typedef struct cNWGrid cNWGrid;
typedef struct cNWGridVtbl {
	void *vf00_01[2];
	byte(__thiscall *GetCellAt)(cNWGrid *self, const vec3_t *p, cNWIntList **out);			/* 0x08 */
	byte(__thiscall *GetCell)(cNWGrid *self, const int *x, const int *y, const int *z, cNWIntList **out);	/* 0x0c */
	void *vf10;
	byte(__thiscall *CellCoords)(cNWGrid *self, const vec3_t *p, int *x, int *y, int *z);		/* 0x14 */
	void(__thiscall *GetDivs)(cNWGrid *self, int *nx, int *ny, int *nz);				/* 0x18 */
	vec3_t *(__thiscall *GetCellSize)(cNWGrid *self, vec3_t *out);					/* 0x1c */
} cNWGridVtbl;
SD_OFFSET(cNWGridVtbl, GetCellSize, 0x1c);

struct cNWGrid {
	const cNWGridVtbl *vftable;
};

typedef struct CNodesWorldVtbl {
	CNodesWorld *(__thiscall *Delete)(CNodesWorld *self, byte flags);	/* 0x00 */
	void *vf01_02[2];
	byte(__thiscall *LoadGraph)(CNodesWorld *self);			/* 0x0c CNodesWorld_LoadGraph */
	void *vf04_0a[7];
	const vec3_t *(__thiscall *GetVisPoint)(CNodesWorld *self, int i);	/* 0x2c */
	int(__thiscall *NumVisPoints)(CNodesWorld *self);			/* 0x30 */
	void *vf0d_0e[2];
	void(__thiscall *SavePath)(CNodesWorld *self);				/* 0x3c CNodesWorld_SavePath */
	void *vf10_12[3];
	/* the position of a node of the generation buffer (CHECK on the index) */
	vec3_t *(__thiscall *BufferPos)(CNodesWorld *self, vec3_t *out, int index);	/* 0x4c */
	void *vf14_22[15];
	void(__thiscall *FreeGraph)(CNodesWorld *self);			/* 0x8c */
	void *vf24;
	/* the node is one of the n nodes of list */
	byte(__thiscall *InList)(CNodesWorld *self, struct cNWNode *const *list, int n, const struct cNWNode *node);	/* 0x94 */
	void(__thiscall *ClearLists)(CNodesWorld *self);			/* 0x98 CNodesWorld_ClearLists */
} CNodesWorldVtbl;
SD_OFFSET(CNodesWorldVtbl, GetVisPoint, 0x2c);
SD_OFFSET(CNodesWorldVtbl, SavePath, 0x3c);
SD_OFFSET(CNodesWorldVtbl, BufferPos, 0x4c);
SD_OFFSET(CNodesWorldVtbl, FreeGraph, 0x8c);
SD_OFFSET(CNodesWorldVtbl, InList, 0x94);
SD_OFFSET(CNodesWorldVtbl, ClearLists, 0x98);

struct CNodesWorld {
	const CNodesWorldVtbl *vftable;	/* 0x000 */
	vec3_t halfNode;		/* 0x004 half of nodeSize */
	vec3_t axis[3];			/* 0x010 nodeSize along x, y and z */
	vec3_t nodeSize;		/* 0x034 vectInterNod */
	float f40;			/* 0x040 25000 outdoors, 1500 indoors */
	vec3_t f44;			/* 0x044 */
	float margin;			/* 0x050 the smallest component of nodeSize / 100 */
	float f54;			/* 0x054 */
	float linkDist;			/* 0x058 */
	float f5c;			/* 0x05c */
	float maxVisDist;		/* 0x060 the searches of points of visibility end at this distance */
	vec3_t nodeInit;		/* 0x064 PointNodeInit */
	byte created;			/* 0x070 */
	byte pad_71[3];
	vec3_t min;			/* 0x074 the box of the level */
	vec3_t max;			/* 0x080 */
	char name[0xff];		/* 0x08c the level, the folder of the graph files */
	byte nodeInitDummy;		/* 0x18b */
	cNWGrid *grid;			/* 0x18c the points of visibility by cell */
	cNWPointList points;		/* 0x190 the points of the generation */
	cNWNode *nodes;			/* 0x1a8 */
	int numNodes;			/* 0x1ac */
	byte pad_1b0[0x1f0 - 0x1b0];
	cNWVecList extraPoints;		/* 0x1f0 */
	byte pad_208[0x25c - 0x208];
	cNWVecList lists25c[2];		/* 0x25c the nodes of the debug view */
	cNWVecList lists28c[5];		/* 0x28c */
	byte pad_304[0x310 - 0x304];
	cNWVecList errCollide;		/* 0x310 AnalizeGraph: the links through the level, 3 points each */
	cNWVecList errUnlinked;		/* 0x328 the nodes without links */
	cNWVecList errOneWay;		/* 0x340 the links without the link back, 2 points each */
	int i358;			/* 0x358 */
	byte b35c;			/* 0x35c */
	byte pad_35d[3];
	/* CNodesWorld */
	byte analizing;			/* 0x360 Analize: Load analyzes the level instead of making the graph */
	byte pad_361[3];
	void *file;			/* 0x364 AnalisisMap.txt */
	byte open;			/* 0x368 the level is open */
	byte pad_369[0x3b0 - 0x369];
	byte b3b0;			/* 0x3b0 */
	byte pad_3b1[3];
	vec3_t analizeSize;		/* 0x3b4 */
	vec3_t hitFrom;			/* 0x3c0 */
	vec3_t hitTo;			/* 0x3cc */
	cNWTriList tris;		/* 0x3d8 */
};
SD_OFFSET(CNodesWorld, maxVisDist, 0x60);
SD_OFFSET(CNodesWorld, name, 0x8c);
SD_OFFSET(CNodesWorld, grid, 0x18c);
SD_OFFSET(CNodesWorld, numNodes, 0x1ac);
SD_OFFSET(CNodesWorld, extraPoints, 0x1f0);
SD_OFFSET(CNodesWorld, errCollide, 0x310);
SD_OFFSET(CNodesWorld, analizing, 0x360);
SD_OFFSET(CNodesWorld, hitFrom, 0x3c0);
SD_OFFSET(CNodesWorld, tris, 0x3d8);
SD_SIZE(CNodesWorld, 0x3f0);

#define CNODESWORLD_VFTABLE 0x3aabb8

/* the globals of the module */
#define g_NWDebugSegs GAME_DATA(int, 0x444e3c)		/* the path tests keep their segments in the list they get */
#define g_NWNodeRadius GAME_DATA(float, 0x444d30)	/* 751 outdoors, 131 indoors */
#define g_NWF60 GAME_DATA(float, 0x444d60)
#define g_NWAnalizeOn GAME_DATA(int, 0x44418c)
#define g_NWDrawMode GAME_DATA(int, 0x444d48)		/* what the debug view of the graph draws */

/* the base class (another source) */
CNodesWorld *__thiscall CNodesWorldSup_ctor(CNodesWorld *self);
void __fastcall CNodesWorldSup_dtor(CNodesWorld *self);
/* vtable slot 0x8c, called directly by the destructor */
void __thiscall CNodesWorldSup_FreeGraph(CNodesWorld *self);
byte __thiscall CNodesWorldSup_LoadGraph(CNodesWorld *self, const char *path);
byte __thiscall CNodesWorldSup_SaveInfPath(CNodesWorld *self, const char *path);
/* the Global/DList.h instance of the points of another source (0x14 nodes) */
void __fastcall DListVec3_Clear(cNWVecList *list);
void __thiscall DListVec3_Add(cNWVecList *list, const vec3_t *v);

/* the DList.h instances */
void __fastcall NWIntList_Free(cNWIntList *list);
void __fastcall NWVecList_Free(cNWVecList *list);
/* the list of a struct at +0xc */
void __fastcall NWIntList_FreeAt0c(void *owner);
void __thiscall NWIntList_Add(cNWIntList *list, void *const *data);
void __thiscall NWVecList_Add(cNWVecList *list, const vec3_t *v);
void __thiscall NWTriList_Add(cNWTriList *list, const float *data);
/* data: the 0x3c bytes from pos on */
void __thiscall NWPointList_Add(cNWPointList *list, const void *data);
/* in front of the iterator (at the end without one) */
void __thiscall NWIntList_AddBefore(cNWIntList *list, void *const *data);
/* at the end, head stays */
void __thiscall NWSegList_Append(cNWSegList *list, const vec3_t *seg);
void __fastcall NWIntList_Clear(cNWIntList *list);
void __fastcall NWVecList_Clear(cNWVecList *list);
void __fastcall NWTriList_Clear(cNWTriList *list);
void __fastcall NWPointList_Clear(cNWPointList *list);
void __fastcall NWIntList_RemoveHead(cNWIntList *list);
void __fastcall NWVecList_RemoveHead(cNWVecList *list);
/* to the free list while it has room, else the links and the node deleted */
void __thiscall NWPointList_Release(cNWPointList *list, cNWPointNode *node);
/* a node from the free list, else a new one with the links and box zeroed */
cNWPointNode *__fastcall NWPointList_NewNode(cNWPointList *list);
void __fastcall NWBox_Init(cNWBox *box);

/* ---- CNodesWorld ---- */

/* CNodesWorld_ctor: ai.h */
void __fastcall CNodesWorld_dtor(CNodesWorld *self);
CNodesWorld *__thiscall CNodesWorld_Delete(CNodesWorld *self, byte flags);
/* the graph file <name>/AI/PathAI of the base class */
byte __thiscall CNodesWorld_LoadGraph(CNodesWorld *self);
/* <name>/AI/InfPathAI.pth of the base class (writes a file) */
byte __thiscall CNodesWorld_SaveInfPath(CNodesWorld *self);
void __thiscall CNodesWorld_ClearLists(CNodesWorld *self);
/* the segment a b free of the level, also the 4 (mode 1: 3) segments around it at radius r; self not used */
byte __thiscall CNodesWorld_TestPath(CNodesWorld *self, const vec3_t *a, const vec3_t *b, float r, float r2,
	cNWSegList *debug, int mode);
/* the corner a b c at b free of the level: the segments between the sides of a b and of b c at radius r (3 pairs,
 * flat: the other side of the pairs); self not used */
byte __thiscall CNodesWorld_TestCorner(CNodesWorld *self, const vec3_t *a, const vec3_t *b, const vec3_t *c, float r,
	float r2, byte flat, cNWSegList *debug);
/* the node of the generation buffer seen from the point */
byte __thiscall CNodesWorld_BufferPosSeen(CNodesWorld *self, const vec3_t *from, int index);
/* the analysis of a segment of the level: 1 when it is free (hit = b, the hit of tri FLT_MAX; outdoors 0 straight
 * up) or it hits a solid triangle facing it (hitFrom and hitTo kept unless b3b0) */
byte __thiscall CNodesWorld_TestSurface(CNodesWorld *self, const vec3_t *a, const vec3_t *b, vec3_t *hit,
	struct CColisTri *tri);
/* TestSurface from pos along the 6 axis directions as far as the largest side of the level box */
byte __thiscall CNodesWorld_TestSurfaceAround(CNodesWorld *self, const vec3_t *pos, vec3_t *hit, struct CColisTri *tri);
/* the points of visibility of the cell between distMin and the visibility distance (and distMax with useMax):
 * mode 0 TestPath with radius, 1 the segment, 2 indoors on the same floor */
void __thiscall CNodesWorld_CollectCell(CNodesWorld *self, const vec3_t *pos, cNWIntList *cell, const vec3_t **list,
	int max, int *count, float radius, float distMin, float distMax, int mode, byte useMax);
/* the cells ring by ring around the cell of pos; soft goes on past distMax until one is found */
void __thiscall CNodesWorld_SearchRings(CNodesWorld *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
	float radius, float distMin, float distMax, byte soft, int mode);
/* the points of visibility around pos: the cell of pos, then the rings */
void __thiscall CNodesWorld_GetPointsVis(CNodesWorld *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float radius, float distMin, float distMax, byte soft, int mode);
/* a path of up to depth nodes from start (at most 20) into path, starting at *node when TestPath sees it (direct),
 * else at the point of visibility in the direction dir; each next node the linked one not in the path closest to
 * dir (taken at once under 45 degrees), flat: dir follows the path, without y once it turns away from y; the last
 * node into *node */
byte __thiscall CNodesWorld_BuildPath(CNodesWorld *self, cNWVecList *path, int depth, const vec3_t *dir,
	const vec3_t *start, byte direct, cNWNode **node, float radius, byte flat);
/* a point of visibility at most 2500 (outdoors 25000) away that TestPath sees with radius */
byte __thiscall CNodesWorld_HasPointVis(CNodesWorld *self, const vec3_t *pos, float radius);
/* the point is in a sector the camera sees */
byte __thiscall CNodesWorld_PointSeen(CNodesWorld *self, const vec3_t *pos);
/* such a point in the cone of maxAngle around dir, from a random one on in a random direction */
byte __thiscall CNodesWorld_GetRacePointDir(CNodesWorld *self, vec3_t *out, const vec3_t *initial, float minDist,
	float maxDist, int arg, const vec3_t *dir, float maxAngle);
/* the nearest point of visibility the segment sees, else the nearest one */
void __thiscall CNodesWorld_NearestPointVis(CNodesWorld *self, const vec3_t *pos, vec3_t *out);
/* a random point between the node and a random linked node, at most maxDist from the node */
void __thiscall CNodesWorld_RandomNearNode(CNodesWorld *self, int node, float maxDist, vec3_t *out);
/* such a point (without maxDist) between distMin and distMax from pos in the cone of maxAngle around dir, else
 * RandomNearNode with maxDist */
void __thiscall CNodesWorld_RandomAheadNode(CNodesWorld *self, int node, const vec3_t *pos, float distMin,
	float distMax, float maxDist, const vec3_t *dir, float maxAngle, vec3_t *out);
/* CNodesWorld_GetRacePoint (a random point of visibility on the floor of initial between minDist and maxDist),
 * CNodesWorld_Analize (Load in the analysis mode, writes AnalisisMap.txt) and CNodesWorld_AnalizeGraph: ai.h */

#endif
