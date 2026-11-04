/*
 * AI/NodesWorld.cpp of the game: CNodesWorld, the 3D visibility graph of a level. The base class CNodesWorldSup
 * (and on, another source) loads the graph: the nodes with their links (0x1a8) and the points of
 * visibility (the nodes) in a grid of cells (0x18c), which the searches walk ring by ring around the cell of a
 * position.
 * CNodesWorld adds the file names of the graph, the path and visibility queries on the points, the analysis of
 * the graph (AnalizeGraph, the "GRAFO" texts) and of the level (Analyze), and the Global/DList.h instances the
 * generation of the graph uses.
 */
#ifndef SCRAP_NODESWORLD_H
#define SCRAP_NODESWORLD_H

#include "scrap/gvar.h"
#include "scrap/dlist.h"

typedef struct CNodesWorld CNodesWorld;
struct CCollisionTri;

/* ---- Global/DList.h instances ---- */

/* a node with one int or pointer */
typedef struct cNWIntNode {
	struct cNWIntNode *next;
	struct cNWIntNode *prev;
	void *data;
} cNWIntNode;
typedef DLIST(cNWIntNode) cNWIntList;

/* a node with 12 bytes: a point, or a cell index with two pointers */
typedef struct cNWVecNode {
	struct cNWVecNode *next;
	struct cNWVecNode *prev;
	vec3_t v;
} cNWVecNode;
typedef DLIST(cNWVecNode) cNWVecList;

/* a segment of the debug view of the path tests */
typedef struct cNWSegNode {
	struct cNWSegNode *next;
	struct cNWSegNode *prev;
	vec3_t a, b;
} cNWSegNode;
typedef DLIST(cNWSegNode) cNWSegList;

/* a point of visibility of the generation, with the points it sees */
typedef struct cNWPointNode {
	struct cNWPointNode *next;
	struct cNWPointNode *prev;
	vec3_t pos;
	cNWIntList links;		/* the pos of the other points */
	vec3_t boxA;
	vec3_t boxB;
} cNWPointNode;
typedef DLIST(cNWPointNode) cNWPointList;

/* a triangle with an inverted normal found by Analyze (15 words of data) */
typedef struct cNWTriNode {
	struct cNWTriNode *next;
	struct cNWTriNode *prev;
	float data[15];
} cNWTriNode;
typedef DLIST(cNWTriNode) cNWTriList;

/* a pair of points (the positions in their cNWPointNode) with the square of their distance */
typedef struct cNWPair {
	const vec3_t *a;
	const vec3_t *b;
	float dist;
} cNWPair;

typedef struct cNWPairNode {
	struct cNWPairNode *next;
	struct cNWPairNode *prev;
	cNWPair p;
} cNWPairNode;
typedef DLIST(cNWPairNode) cNWPairList;

/* the box the generation fills (FLT_MAX first, -FLT_MAX after) */
typedef struct cNWBox {
	vec3_t a, b;
} cNWBox;

/* ---- the graph ---- */

/* a node of the loaded graph */
typedef struct cNWNode {
	vec3_t pos;
	int numLinks;
	struct cNWNode **links;
} cNWNode;

/* the set of points of visibility seen from cells of the generation buffer (InfPointVis), shared by the cells with
 * the same set: refs counts them; the sets are kept in the hash lists sets[hash] (0x24 bytes) */
typedef struct cNWVis {
	int refs;
	int hash;			/* the list of sets it is in */
	struct cNWVisNode *node;	/* its node in that list */
	cNWIntList points;		/* the pos of the points, sorted by address */
} cNWVis;

typedef struct cNWVisNode {
	struct cNWVisNode *next;
	struct cNWVisNode *prev;
	cNWVis vis;
} cNWVisNode;
typedef DLIST(cNWVisNode) cNWVisList;

/* a cell of the generation buffer */
typedef struct cNWCell {
	int state;			/* 0, 3, 4, 5, 6: the cellLists of GetInfoNodes */
	cNWVis *vis;
} cNWCell;

typedef struct CNodesWorldVtbl {
	void (*Free)(CNodesWorld *self);
	void(*GetBox)(CNodesWorld *self, vec3_t *min, vec3_t *max);
	byte(*LoadFiles)(CNodesWorld *self, const char *path);	/* CNodesWorldSup_LoadGraph */
	byte(*LoadGraph)(CNodesWorld *self);			/* CNodesWorld_LoadGraph */
	byte(*Create)(CNodesWorld *self);			/* LoadGraph, created on success */
	/* the graph file a node was loaded from, -1 for a node not in the array */
	int(*FileOf)(CNodesWorld *self, const struct cNWNode *node);
	/* the file of all n nodes, -1 if they differ or n < 1 */
	int(*CommonFile)(CNodesWorld *self, struct cNWNode *const *nodes, int n);
	/* the middle of the buffer cell of pos */
	vec3_t *(*SnapPos)(CNodesWorld *self, vec3_t *out, const vec3_t *pos);
	/* the linked nodes of node not at pos, at most max */
	void(*OtherLinks)(CNodesWorld *self, const struct cNWNode *node, const vec3_t *pos,
		struct cNWNode **list, int max, int *count);
	byte(*IsCreated)(CNodesWorld *self);
	/* nodeInit set to the last node, copied to out */
	vec3_t *(*LastNodePos)(CNodesWorld *self, vec3_t *out);
	const vec3_t *(*GetVisPoint)(CNodesWorld *self, int i);
	int(*NumVisPoints)(CNodesWorld *self);
	float(*GetMaxVisDist)(CNodesWorld *self);
	byte(*InGrid)(CNodesWorld *self, const vec3_t *pos);
	byte(*SavePath)(CNodesWorld *self);				/* CNodesWorld_SavePath */
	byte(*CheckBuffer)(CNodesWorld *self);
	void(*BuildGrid)(CNodesWorld *self);			/* the grid of the points */
	int(*BufferIndex)(CNodesWorld *self, const vec3_t *pos);
	/* the position of a node of the generation buffer (CHECK on the index) */
	vec3_t *(*BufferPos)(CNodesWorld *self, vec3_t *out, int index);
	void(*GetInfoNodes)(CNodesWorld *self);
	void(*AddVis)(CNodesWorld *self, const vec3_t *point, int index);
	int(*Hash)(CNodesWorld *self, int a, int b);
	void(*ReleaseVis)(CNodesWorld *self, int index);
	void(*LinkPoints)(CNodesWorld *self);
	void(*LinkBack)(CNodesWorld *self);
	/* the links of point have other */
	byte(*PointLinked)(CNodesWorld *self, const vec3_t *point, const vec3_t *other);
	void(*LinkPoint)(CNodesWorld *self, const vec3_t *point);
	byte(*Unreachable)(CNodesWorld *self, const vec3_t *from, const vec3_t *to, int index);
	int(*BoxCells)(CNodesWorld *self, const cNWBox *box);
	byte(*FindSorted)(CNodesWorld *self, cNWIntList *list, const void *key, cNWIntNode **at);
	byte(*HasPair)(CNodesWorld *self, void *list, const void *pair);
	void(*InsertPair)(CNodesWorld *self, void *list, const void *pair);
	byte(*VisHas)(CNodesWorld *self, const vec3_t *point, int index);
	byte(*Linked)(CNodesWorld *self, const vec3_t *a, const vec3_t *b);
	void(*FreeGraph)(CNodesWorld *self);
	void(*FreeTemp)(CNodesWorld *self);			/* the buffer and the sets of the generation */
	/* the node is one of the n nodes of list */
	byte(*InList)(CNodesWorld *self, struct cNWNode *const *list, int n, const struct cNWNode *node);
	void(*ClearLists)(CNodesWorld *self);			/* CNodesWorld_ClearLists */
	byte(*SaveInfPath)(CNodesWorld *self, const char *path);
} CNodesWorldVtbl;

/* calls through the slots of CNodesWorldVtbl */
static inline void NodesWorld_Free(void *self) {
	VT(CNodesWorldVtbl, self)->Free(self);
}

static inline void NodesWorld_AddVis(void *self, const vec3_t *point, int index) {
	VT(CNodesWorldVtbl, self)->AddVis(self, point, index);
}

static inline int NodesWorld_BoxCells(void *self, const cNWBox *box) {
	return VT(CNodesWorldVtbl, self)->BoxCells(self, box);
}

static inline int NodesWorld_BufferIndex(void *self, const vec3_t *pos) {
	return VT(CNodesWorldVtbl, self)->BufferIndex(self, pos);
}

static inline vec3_t *NodesWorld_BufferPos(void *self, vec3_t *out, int index) {
	return VT(CNodesWorldVtbl, self)->BufferPos(self, out, index);
}

static inline void NodesWorld_BuildGrid(void *self) {
	VT(CNodesWorldVtbl, self)->BuildGrid(self);
}

static inline void NodesWorld_ClearLists(void *self) {
	VT(CNodesWorldVtbl, self)->ClearLists(self);
}

static inline byte NodesWorld_Create(void *self) {
	return VT(CNodesWorldVtbl, self)->Create(self);
}

static inline int NodesWorld_FileOf(void *self, const struct cNWNode *node) {
	return VT(CNodesWorldVtbl, self)->FileOf(self, node);
}

static inline byte NodesWorld_FindSorted(void *self, cNWIntList *list, const void *key, cNWIntNode **at) {
	return VT(CNodesWorldVtbl, self)->FindSorted(self, list, key, at);
}

static inline void NodesWorld_FreeGraph(void *self) {
	VT(CNodesWorldVtbl, self)->FreeGraph(self);
}

static inline void NodesWorld_FreeTemp(void *self) {
	VT(CNodesWorldVtbl, self)->FreeTemp(self);
}

static inline void NodesWorld_GetBox(void *self, vec3_t *min, vec3_t *max) {
	VT(CNodesWorldVtbl, self)->GetBox(self, min, max);
}

static inline void NodesWorld_GetInfoNodes(void *self) {
	VT(CNodesWorldVtbl, self)->GetInfoNodes(self);
}

static inline const vec3_t *NodesWorld_GetVisPoint(void *self, int i) {
	return VT(CNodesWorldVtbl, self)->GetVisPoint(self, i);
}

static inline int NodesWorld_Hash(void *self, int a, int b) {
	return VT(CNodesWorldVtbl, self)->Hash(self, a, b);
}

static inline byte NodesWorld_InGrid(void *self, const vec3_t *pos) {
	return VT(CNodesWorldVtbl, self)->InGrid(self, pos);
}

static inline byte NodesWorld_InList(void *self, struct cNWNode *const *list, int n, const struct cNWNode *node) {
	return VT(CNodesWorldVtbl, self)->InList(self, list, n, node);
}

static inline void NodesWorld_InsertPair(void *self, void *list, const void *pair) {
	VT(CNodesWorldVtbl, self)->InsertPair(self, list, pair);
}

static inline byte NodesWorld_IsCreated(void *self) {
	return VT(CNodesWorldVtbl, self)->IsCreated(self);
}

static inline void NodesWorld_LinkBack(void *self) {
	VT(CNodesWorldVtbl, self)->LinkBack(self);
}

static inline void NodesWorld_LinkPoint(void *self, const vec3_t *point) {
	VT(CNodesWorldVtbl, self)->LinkPoint(self, point);
}

static inline void NodesWorld_LinkPoints(void *self) {
	VT(CNodesWorldVtbl, self)->LinkPoints(self);
}

static inline byte NodesWorld_Linked(void *self, const vec3_t *a, const vec3_t *b) {
	return VT(CNodesWorldVtbl, self)->Linked(self, a, b);
}

static inline byte NodesWorld_LoadGraph(void *self) {
	return VT(CNodesWorldVtbl, self)->LoadGraph(self);
}

static inline int NodesWorld_NumVisPoints(void *self) {
	return VT(CNodesWorldVtbl, self)->NumVisPoints(self);
}

static inline void NodesWorld_OtherLinks(void *self, const struct cNWNode *node, const vec3_t *pos, struct cNWNode **list, int max, int *count) {
	VT(CNodesWorldVtbl, self)->OtherLinks(self, node, pos, list, max, count);
}

static inline byte NodesWorld_PointLinked(void *self, const vec3_t *point, const vec3_t *other) {
	return VT(CNodesWorldVtbl, self)->PointLinked(self, point, other);
}

static inline void NodesWorld_ReleaseVis(void *self, int index) {
	VT(CNodesWorldVtbl, self)->ReleaseVis(self, index);
}

static inline byte NodesWorld_SavePath(void *self) {
	return VT(CNodesWorldVtbl, self)->SavePath(self);
}

static inline vec3_t *NodesWorld_SnapPos(void *self, vec3_t *out, const vec3_t *pos) {
	return VT(CNodesWorldVtbl, self)->SnapPos(self, out, pos);
}

static inline byte NodesWorld_Unreachable(void *self, const vec3_t *from, const vec3_t *to, int index) {
	return VT(CNodesWorldVtbl, self)->Unreachable(self, from, to, index);
}

static inline byte NodesWorld_VisHas(void *self, const vec3_t *point, int index) {
	return VT(CNodesWorldVtbl, self)->VisHas(self, point, index);
}

/* the base class (AI/NodesWorldSup.cpp, abstract): the first 0x360 bytes of CNodesWorld and CNodesWorld2D. The code
 * of both works on the CNodesWorld layout, which repeats these members */
typedef struct CNodesWorldSup {
	const CNodesWorldVtbl *vftable;
	vec3_t halfNode;		/* half of nodeSize */
	vec3_t axis[3];			/* nodeSize along x, y and z */
	vec3_t nodeSize;		/* vectInterNod */
	float pairRange;		/* 25000 outdoors, 1500 indoors: a link is tried up to twice it */
	vec3_t nodeTenth;		/* nodeSize / 10 */
	float margin;			/* the smallest component of nodeSize / 100 */
	float gridCell;			/* the cell of the grid of the level, 12500 outdoors, 750 indoors */
	float linkDist;
	float startVisDist;		/* the search of the visible points a path starts at, 27500 or 1650 */
	float maxVisDist;		/* the searches of points of visibility end at this distance */
	vec3_t nodeInit;		/* PointNodeInit */
	byte created;
	byte pad_71[3];
	vec3_t min;			/* the box of the level */
	vec3_t max;
	char name[0xff];		/* the level, the folder of the graph files */
	byte nodeInitDummy;
	struct cGridElements *grid;	/* the points of visibility by cell (cGridElements_InfPointVisPtr) */
	cNWPointList points;		/* the points of the generation */
	cNWNode *nodes;
	int numNodes;
	cNWCell *buffer;		/* the cells of the generation, nx * ny * nz (Mem_Alloc) */
	int nx, ny, nz;
	int dimBuffer;
	int numPointLinks;		/* the links of the generated points */
	void *reachedCells;		/* the indices of the cells the region reached (IniBuffer), FindLinkFree; Mem_Alloc */
	int numReachedCells;
	cNWIntList edgeCells;		/* the cells the region did not reach, NWSup_NextInit starts next to them */
	cNWVisList *sets;		/* numSets hash lists of the sets of points (Mem_Alloc) */
	int numSets;
	cNWVecList extraPoints;
	cNWNode *fileStart[20];		/* the first node of each graph file */
	int numFiles;
	cNWVecList foundNodes[2];		/* the nodes of the debug view */
	cNWVecList cellLists[5];		/* the buffer cells by state (GetInfoNodes) */
	vec3_t openPos;			/* the cell where the region of the generation left the box */
	cNWVecList errCollide;		/* AnalizeGraph: the links through the level, 3 points each */
	cNWVecList errUnlinked;		/* the nodes without links */
	cNWVecList errOneWay;		/* the links without the link back, 2 points each */
	int debugHits;			/* the nodes the debug test found blocked */
	byte unused_35c;
	byte pad_35d[3];
} CNodesWorldSup;

struct CNodesWorld {
	CNodesWorldSup sup;
	/* CNodesWorld */
	byte analyzing;			/* Analyze: Load analyzes the level instead of making the graph */
	byte pad_361[3];
	void *file;			/* AnalisisMap.txt */
	byte open;			/* the level is open */
	byte pad_369[3];
	byte firstTri[0x44];		/* the first triangle the analysis found (a CCollisionTri) */
	byte firstSaved;		/* the first bad triangle and hit are kept */
	byte pad_3b1[3];
	vec3_t analyzeSize;
	vec3_t hitFrom;
	vec3_t hitTo;
	cNWTriList tris;
};

/* the globals of the module */
extern cGVar GVar_ShowTestCanGoAI;
#define g_NWDebugSegs (GVar_ShowTestCanGoAI.value.i)		/* the path tests keep their segments in the list they get */
extern float g_NWNodeRadius;	/* 751 outdoors, 131 indoors */
extern cGVar GVar_RADIUS_OBJECTIVE_IN_ROUTE;
#define g_NWF60 (GVar_RADIUS_OBJECTIVE_IN_ROUTE.value.f)
extern cGVar GVar_ShowAI;
#define g_NWAnalyzeOn (GVar_ShowAI.value.i)
extern cGVar GVar_ShowNodesAI;
#define g_NWDrawMode (GVar_ShowNodesAI.value.i)		/* what the debug view of the graph draws */

/* the base class (another source) */
CNodesWorld *CNodesWorldSup_ctor(CNodesWorld *self);
void CNodesWorldSup_dtor(CNodesWorld *self);
/* vtable slot 0x8c, called directly by the destructor */
void CNodesWorldSup_FreeGraph(CNodesWorld *self);
byte CNodesWorldSup_LoadGraph(CNodesWorld *self, const char *path);
byte CNodesWorldSup_SaveInfPath(CNodesWorld *self, const char *path);
/* the Global/DList.h instance of the points of another source (0x14 nodes) */
void DListVec3_Clear(cNWVecList *list);
void DListVec3_Add(cNWVecList *list, const vec3_t *v);

/* the DList.h instances */
void NWIntList_Free(cNWIntList *list);
void NWVecList_Free(cNWVecList *list);
/* the list of a struct at +0xc */
void NWIntList_Add(cNWIntList *list, void *const *data);
void NWVecList_Add(cNWVecList *list, const vec3_t *v);
/* data: the from pos on */
void NWPointList_Add(cNWPointList *list, const void *data);
/* in front of the iterator, the head kept (Add when the iterator is the head) */
void NWIntList_AddBefore(cNWIntList *list, void *const *data);
void NWIntList_Clear(cNWIntList *list);
void NWVecList_Clear(cNWVecList *list);
void NWPointList_Clear(cNWPointList *list);
void NWIntList_RemoveHead(cNWIntList *list);
void NWVecList_RemoveHead(cNWVecList *list);
void NWBox_Init(cNWBox *box);

/* ---- CNodesWorld ---- */

/* the generation of the graph */
/* the triangle tri hit around p into tris and the file of the analysis (not twice, at most 1000); hit is changed
 * into meters with y and z swapped */
void CNodesWorld_LogTri(CNodesWorld *self, const vec3_t *p, vec3_t *hit, const struct CCollisionTri *tri);
/* the debug view of g_NWDrawMode */
void CNodesWorld_Draw(CNodesWorld *self);

/* the name of the level with file after it, in a buffer of 0x100 like the original */
void NW_LevelPath(char *path, const CNodesWorld *self, const char *file);
/* the position of the camera entity of view 0 (vtable slot 0x5c of the Cam) */
void NW_CamPos(float *x, float *y, float *z);
/* x y z made a unit vector (left as it is for the length 0); the length. The original sums the squares x or y
 * first, the same sum. */
float NW_Normalize(vec3_t *out, float x, float y, float z);
/* the segment a b into the debug list of the path tests when g_NWDebugSegs is on */
void NW_DebugSeg(cNWSegList *debug, const vec3_t *a, const vec3_t *b);
/* the segment from a + side * r along dl; 1 if it is free */
int NW_PathSide(const vec3_t *a, const vec3_t *side, float r, const vec3_t *dl, cNWSegList *debug);
/* the segment between b + u * r and b + v * r free of the level */
int NW_CornerSide(const vec3_t *b, const vec3_t *u, const vec3_t *v, float r, cNWSegList *debug);
/* the segment a b free of the level, also the 4 (mode 1: 3) segments around it at radius r; self not used */
byte CNodesWorld_TestPath(CNodesWorld *self, const vec3_t *a, const vec3_t *b, float r, float r2,
	cNWSegList *debug, int mode);
/* the corner a b c at b free of the level: the segments between the sides of a b and of b c at radius r (3 pairs,
 * flat: the other side of the pairs); self not used */
byte CNodesWorld_TestCorner(CNodesWorld *self, const vec3_t *a, const vec3_t *b, const vec3_t *c, float r,
	float r2, byte flat, cNWSegList *debug);
/* TestSurface from pos along the 6 axis directions as far as the largest side of the level box */
byte CNodesWorld_TestSurfaceAround(CNodesWorld *self, const vec3_t *pos, vec3_t *hit, struct CCollisionTri *tri);
/* the points of visibility around pos: the cell of pos, then the rings */
void CNodesWorld_GetPointsVis(CNodesWorld *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float radius, float distMin, float distMax, byte soft, int mode);
/* a path of up to depth nodes from start (at most 20) into path, starting at *node when TestPath sees it (direct),
 * else at the point of visibility in the direction dir; each next node the linked one not in the path closest to
 * dir (taken at once under 45 degrees), flat: dir follows the path, without y once it turns away from y; the last
 * node into *node */
byte CNodesWorld_BuildPath(CNodesWorld *self, cNWVecList *path, int depth, const vec3_t *dir,
	const vec3_t *start, byte direct, cNWNode **node, float radius, byte flat);
/* a point of visibility at most 2500 (outdoors 25000) away that TestPath sees with radius */
byte CNodesWorld_HasPointVis(CNodesWorld *self, const vec3_t *pos, float radius);
/* the point is in a sector the camera sees */
byte CNodesWorld_PointSeen(CNodesWorld *self, const vec3_t *pos);
/* a random point of visibility on the floor of initial between minDist and maxDist in the cone of maxAngle
 * around dir: from a random one on, in a random direction; arg not used */
byte CNodesWorld_GetRacePointDir(CNodesWorld *self, vec3_t *out, const vec3_t *initial, float minDist,
	float maxDist, int arg, const vec3_t *dir, float maxAngle);
/* the nearest point of visibility the segment sees, else the nearest one */
void CNodesWorld_NearestPointVis(CNodesWorld *self, const vec3_t *pos, vec3_t *out);
/* CNodesWorld_GetRacePoint (a random point of visibility on the floor of initial between minDist and maxDist),
 * CNodesWorld_Analyze (Load in the analysis mode, writes AnalisisMap.txt) and CNodesWorld_AnalyzeGraph: ai.h */

/* ---- AI/AStar.cpp (another source, not in C): a path over the visibility graph from from to to; startOk and
 * goalOk 0 when the end has no point of visibility ---- */
byte CAStar_FindPath(void *astar, cNWVecList *path, const vec3_t *to, const vec3_t *from, float radius,
	float dist, int stupid, byte *startOk, byte *goalOk, int *state);
void CAStar_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count, float r);

#endif
