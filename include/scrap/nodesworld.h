/*
 * AI/NodesWorld.cpp of the game: CNodesWorld, the 3D visibility graph of a level. The base class CNodesWorldSup
 * (rva 0xb80f0 and on, another source) loads the graph: the nodes with their links (0x1a8) and the points of
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

/* a triangle with an inverted normal found by Analyze (0x44 bytes, 15 words of data) */
typedef struct cNWTriNode {
	struct cNWTriNode *next;	/* 0x00 */
	struct cNWTriNode *prev;	/* 0x04 */
	float data[15];			/* 0x08 */
} cNWTriNode;
SD_SIZE(cNWTriNode, 0x44);
typedef DLIST(cNWTriNode) cNWTriList;

/* a pair of points (the positions in their cNWPointNode) with the square of their distance */
typedef struct cNWPair {
	const vec3_t *a;		/* 0x0 */
	const vec3_t *b;		/* 0x4 */
	float dist;			/* 0x8 */
} cNWPair;
SD_SIZE(cNWPair, 0xc);

typedef struct cNWPairNode {
	struct cNWPairNode *next;	/* 0x00 */
	struct cNWPairNode *prev;	/* 0x04 */
	cNWPair p;			/* 0x08 */
} cNWPairNode;
SD_SIZE(cNWPairNode, 0x14);
typedef DLIST(cNWPairNode) cNWPairList;

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
	cNWGrid *(*Delete)(cNWGrid *self, byte flags);						/* 0x00 */
	byte(*Add)(cNWGrid *self, const vec3_t *p, void *const *data);				/* 0x04 */
	byte(*GetCellAt)(cNWGrid *self, const vec3_t *p, cNWIntList **out);			/* 0x08 */
	byte(*GetCell)(cNWGrid *self, const int *x, const int *y, const int *z, cNWIntList **out);	/* 0x0c */
	byte(*Draw)(cNWGrid *self, uint32_t color);						/* 0x10 */
	byte(*CellCoords)(cNWGrid *self, const vec3_t *p, int *x, int *y, int *z);		/* 0x14 */
	void(*GetDivs)(cNWGrid *self, int *nx, int *ny, int *nz);				/* 0x18 */
	vec3_t *(*GetCellSize)(cNWGrid *self, vec3_t *out);					/* 0x1c */
	byte(*IsInside)(cNWGrid *self, const vec3_t *p);						/* 0x20 */
} cNWGridVtbl;
SD_OFFSET(cNWGridVtbl, GetCellSize, 0x1c);

struct cNWGrid {
	const cNWGridVtbl *vftable;
};

/* the set of points of visibility seen from cells of the generation buffer (InfPointVis), shared by the cells with
 * the same set: refs counts them; the sets are kept in the hash lists sets[hash] (0x24 bytes) */
typedef struct cNWVis {
	int refs;			/* 0x00 */
	int hash;			/* 0x04 the list of sets it is in */
	struct cNWVisNode *node;	/* 0x08 its node in that list */
	cNWIntList points;		/* 0x0c the pos of the points, sorted by address */
} cNWVis;
SD_SIZE(cNWVis, 0x24);

typedef struct cNWVisNode {
	struct cNWVisNode *next;	/* 0x00 */
	struct cNWVisNode *prev;	/* 0x04 */
	cNWVis vis;			/* 0x08 */
} cNWVisNode;
SD_SIZE(cNWVisNode, 0x2c);
typedef DLIST(cNWVisNode) cNWVisList;

/* a cell of the generation buffer */
typedef struct cNWCell {
	int state;			/* 0x0 0, 3, 4, 5, 6: the cellLists of GetInfoNodes */
	cNWVis *vis;			/* 0x4 */
} cNWCell;
SD_SIZE(cNWCell, 0x8);

typedef struct CNodesWorldVtbl {
	CNodesWorld *(*Delete)(CNodesWorld *self, byte flags);	/* 0x00 */
	void(*GetBox)(CNodesWorld *self, vec3_t *min, vec3_t *max);	/* 0x04 */
	byte(*LoadFiles)(CNodesWorld *self, const char *path);	/* 0x08 CNodesWorldSup_LoadGraph */
	byte(*LoadGraph)(CNodesWorld *self);			/* 0x0c CNodesWorld_LoadGraph */
	byte(*Create)(CNodesWorld *self);			/* 0x10 LoadGraph, created on success */
	/* the graph file a node was loaded from, -1 for a node not in the array */
	int(*FileOf)(CNodesWorld *self, const struct cNWNode *node);	/* 0x14 */
	/* the file of all n nodes, -1 if they differ or n < 1 */
	int(*CommonFile)(CNodesWorld *self, struct cNWNode *const *nodes, int n);	/* 0x18 */
	/* the middle of the buffer cell of pos */
	vec3_t *(*SnapPos)(CNodesWorld *self, vec3_t *out, const vec3_t *pos);	/* 0x1c */
	/* the linked nodes of node not at pos, at most max */
	void(*OtherLinks)(CNodesWorld *self, const struct cNWNode *node, const vec3_t *pos,
		struct cNWNode **list, int max, int *count);		/* 0x20 */
	byte(*IsCreated)(CNodesWorld *self);			/* 0x24 */
	/* nodeInit set to the last node, copied to out */
	vec3_t *(*LastNodePos)(CNodesWorld *self, vec3_t *out);	/* 0x28 */
	const vec3_t *(*GetVisPoint)(CNodesWorld *self, int i);	/* 0x2c */
	int(*NumVisPoints)(CNodesWorld *self);			/* 0x30 */
	float(*GetMaxVisDist)(CNodesWorld *self);			/* 0x34 */
	byte(*InGrid)(CNodesWorld *self, const vec3_t *pos);	/* 0x38 */
	byte(*SavePath)(CNodesWorld *self);				/* 0x3c CNodesWorld_SavePath */
	byte(*CheckBuffer)(CNodesWorld *self);			/* 0x40 */
	void(*BuildGrid)(CNodesWorld *self);			/* 0x44 the grid of the points */
	int(*BufferIndex)(CNodesWorld *self, const vec3_t *pos);	/* 0x48 */
	/* the position of a node of the generation buffer (CHECK on the index) */
	vec3_t *(*BufferPos)(CNodesWorld *self, vec3_t *out, int index);	/* 0x4c */
	void(*GetInfoNodes)(CNodesWorld *self);			/* 0x50 */
	void(*AddVis)(CNodesWorld *self, const vec3_t *point, int index);	/* 0x54 */
	int(*Hash)(CNodesWorld *self, int a, int b);		/* 0x58 */
	void(*ReleaseVis)(CNodesWorld *self, int index);		/* 0x5c */
	void(*LinkPoints)(CNodesWorld *self);			/* 0x60 */
	void(*LinkBack)(CNodesWorld *self);			/* 0x64 */
	/* the links of point have other */
	byte(*PointLinked)(CNodesWorld *self, const vec3_t *point, const vec3_t *other);	/* 0x68 */
	void(*LinkPoint)(CNodesWorld *self, const vec3_t *point);	/* 0x6c */
	byte(*Unreachable)(CNodesWorld *self, const vec3_t *from, const vec3_t *to, int index);	/* 0x70 */
	int(*BoxCells)(CNodesWorld *self, const cNWBox *box);	/* 0x74 */
	byte(*FindSorted)(CNodesWorld *self, cNWIntList *list, const void *key, cNWIntNode **at);	/* 0x78 */
	byte(*HasPair)(CNodesWorld *self, void *list, const void *pair);	/* 0x7c */
	void(*InsertPair)(CNodesWorld *self, void *list, const void *pair);	/* 0x80 */
	byte(*VisHas)(CNodesWorld *self, const vec3_t *point, int index);	/* 0x84 */
	byte(*Linked)(CNodesWorld *self, const vec3_t *a, const vec3_t *b);	/* 0x88 */
	void(*FreeGraph)(CNodesWorld *self);			/* 0x8c */
	void(*FreeTemp)(CNodesWorld *self);			/* 0x90 the buffer and the sets of the generation */
	/* the node is one of the n nodes of list */
	byte(*InList)(CNodesWorld *self, struct cNWNode *const *list, int n, const struct cNWNode *node);	/* 0x94 */
	void(*ClearLists)(CNodesWorld *self);			/* 0x98 CNodesWorld_ClearLists */
	byte(*SaveInfPath)(CNodesWorld *self, const char *path);	/* 0x9c */
} CNodesWorldVtbl;
SD_OFFSET(CNodesWorldVtbl, GetVisPoint, 0x2c);
SD_OFFSET(CNodesWorldVtbl, SavePath, 0x3c);
SD_OFFSET(CNodesWorldVtbl, BufferPos, 0x4c);
SD_OFFSET(CNodesWorldVtbl, Create, 0x10);
SD_OFFSET(CNodesWorldVtbl, BuildGrid, 0x44);
SD_OFFSET(CNodesWorldVtbl, Unreachable, 0x70);
SD_OFFSET(CNodesWorldVtbl, FreeGraph, 0x8c);
SD_OFFSET(CNodesWorldVtbl, FreeTemp, 0x90);
SD_OFFSET(CNodesWorldVtbl, InList, 0x94);
SD_OFFSET(CNodesWorldVtbl, ClearLists, 0x98);
SD_OFFSET(CNodesWorldVtbl, SaveInfPath, 0x9c);

/* the base class (AI/NodesWorldSup.cpp, abstract): the first 0x360 bytes of CNodesWorld and CNodesWorld2D. The code
 * of both works on the CNodesWorld layout, which repeats these members */
typedef struct CNodesWorldSup {
	const CNodesWorldVtbl *vftable;	/* 0x000 */
	vec3_t halfNode;		/* 0x004 half of nodeSize */
	vec3_t axis[3];			/* 0x010 nodeSize along x, y and z */
	vec3_t nodeSize;		/* 0x034 vectInterNod */
	float pairRange;		/* 0x040 25000 outdoors, 1500 indoors: a link is tried up to twice it */
	vec3_t nodeTenth;		/* 0x044 nodeSize / 10 */
	float margin;			/* 0x050 the smallest component of nodeSize / 100 */
	float gridCell;			/* 0x054 the cell of the grid of the level, 12500 outdoors, 750 indoors */
	float linkDist;			/* 0x058 */
	float startVisDist;		/* 0x05c the search of the visible points a path starts at, 27500 or 1650 */
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
	cNWCell *buffer;		/* 0x1b0 the cells of the generation, nx * ny * nz (Mem_Alloc) */
	int nx, ny, nz;			/* 0x1b4 */
	int dimBuffer;			/* 0x1c0 */
	int numPointLinks;		/* 0x1c4 the links of the generated points */
	void *reachedCells;		/* 0x1c8 the indices of the cells the region reached (IniBuffer), FindLinkFree; Mem_Alloc */
	int numReachedCells;			/* 0x1cc */
	cNWIntList edgeCells;		/* 0x1d0 the cells the region did not reach, NWSup_NextInit starts next to them */
	cNWVisList *sets;		/* 0x1e8 numSets hash lists of the sets of points (Mem_Alloc) */
	int numSets;			/* 0x1ec */
	cNWVecList extraPoints;		/* 0x1f0 */
	cNWNode *fileStart[20];		/* 0x208 the first node of each graph file */
	int numFiles;			/* 0x258 */
	cNWVecList foundNodes[2];		/* 0x25c the nodes of the debug view */
	cNWVecList cellLists[5];		/* 0x28c the buffer cells by state (GetInfoNodes) */
	vec3_t openPos;			/* 0x304 the cell where the region of the generation left the box */
	cNWVecList errCollide;		/* 0x310 AnalizeGraph: the links through the level, 3 points each */
	cNWVecList errUnlinked;		/* 0x328 the nodes without links */
	cNWVecList errOneWay;		/* 0x340 the links without the link back, 2 points each */
	int debugHits;			/* 0x358 the nodes the debug test found blocked */
	byte unused_35c;			/* 0x35c */
	byte pad_35d[3];
} CNodesWorldSup;
SD_OFFSET(CNodesWorldSup, maxVisDist, 0x60);
SD_OFFSET(CNodesWorldSup, name, 0x8c);
SD_OFFSET(CNodesWorldSup, grid, 0x18c);
SD_OFFSET(CNodesWorldSup, buffer, 0x1b0);
SD_OFFSET(CNodesWorldSup, sets, 0x1e8);
SD_OFFSET(CNodesWorldSup, numFiles, 0x258);
SD_OFFSET(CNodesWorldSup, errCollide, 0x310);
SD_SIZE(CNodesWorldSup, 0x360);

struct CNodesWorld {
	CNodesWorldSup sup;		/* 0x000 */
	/* CNodesWorld */
	byte analyzing;			/* 0x360 Analyze: Load analyzes the level instead of making the graph */
	byte pad_361[3];
	void *file;			/* 0x364 AnalisisMap.txt */
	byte open;			/* 0x368 the level is open */
	byte pad_369[3];
	byte firstTri[0x44];		/* 0x36c the first triangle the analysis found (a CCollisionTri) */
	byte firstSaved;		/* 0x3b0 the first bad triangle and hit are kept */
	byte pad_3b1[3];
	vec3_t analyzeSize;		/* 0x3b4 */
	vec3_t hitFrom;			/* 0x3c0 */
	vec3_t hitTo;			/* 0x3cc */
	cNWTriList tris;		/* 0x3d8 */
};
SD_OFFSET(CNodesWorld, sup.maxVisDist, 0x60);
SD_OFFSET(CNodesWorld, sup.name, 0x8c);
SD_OFFSET(CNodesWorld, sup.grid, 0x18c);
SD_OFFSET(CNodesWorld, sup.numNodes, 0x1ac);
SD_OFFSET(CNodesWorld, sup.buffer, 0x1b0);
SD_OFFSET(CNodesWorld, sup.sets, 0x1e8);
SD_OFFSET(CNodesWorld, sup.extraPoints, 0x1f0);
SD_OFFSET(CNodesWorld, sup.numFiles, 0x258);
SD_OFFSET(CNodesWorld, sup.openPos, 0x304);
SD_OFFSET(CNodesWorld, sup.errCollide, 0x310);
SD_OFFSET(CNodesWorld, analyzing, 0x360);
SD_OFFSET(CNodesWorld, hitFrom, 0x3c0);
SD_OFFSET(CNodesWorld, tris, 0x3d8);
SD_SIZE(CNodesWorld, 0x3f0);

extern const CNodesWorldVtbl CNodesWorld_vftable;
#define CNODESWORLD_VFTABLE (&CNodesWorld_vftable)

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
void NWIntList_FreeAt0c(void *owner);
void NWIntList_Add(cNWIntList *list, void *const *data);
void NWVecList_Add(cNWVecList *list, const vec3_t *v);
void NWTriList_Add(cNWTriList *list, const float *data);
/* data: the 0x3c bytes from pos on */
void NWPointList_Add(cNWPointList *list, const void *data);
/* in front of the iterator, the head kept (Add when the iterator is the head) */
void NWIntList_AddBefore(cNWIntList *list, void *const *data);
/* at the end, head stays */
void NWSegList_Append(cNWSegList *list, const vec3_t *seg);
void NWIntList_Clear(cNWIntList *list);
void NWVecList_Clear(cNWVecList *list);
void NWTriList_Clear(cNWTriList *list);
void NWPointList_Clear(cNWPointList *list);
void NWIntList_RemoveHead(cNWIntList *list);
void NWVecList_RemoveHead(cNWVecList *list);
/* to the free list while it has room, else the links and the node deleted */
void NWPointList_Release(cNWPointList *list, cNWPointNode *node);
/* a node from the free list, else a new one with the links and box zeroed */
cNWPointNode *NWPointList_NewNode(cNWPointList *list);
void NWBox_Init(cNWBox *box);

/* ---- CNodesWorld ---- */

/* the generation of the graph */
/* the dummies DM_PointGraph3DAI* into extraPoints (a fatal error for one in the level) */
void CNodesWorld_AddExtraPoints(CNodesWorld *self);
/* the pairs of points nearer than 2 * f40 that are not linked, into list sorted by the distance */
void CNodesWorld_GetPairs(CNodesWorld *self, cNWPairList *list);
/* the extra points into the points, linked to the points free of the level within linkDist */
void CNodesWorld_LinkExtra(CNodesWorld *self);
/* the distance from pos to the nearest point of the set of its buffer cell, FLT_MAX without one */
float CNodesWorld_NearestVisDist(CNodesWorld *self, const vec3_t *pos);
/* the cell to link the pair of points through: NWSup_FindLinkBox, NWSup_FindLinkCell; Free: the cells of reachedCells whose
 * center sees both points free of the level (the distance on the floor plane) */
byte CNodesWorld_FindLinkBox(CNodesWorld *self, const vec3_t *const *pair, const cNWBox *box, int *out);
byte CNodesWorld_FindLinkCell(CNodesWorld *self, const vec3_t *const *pair, int *out);
byte CNodesWorld_FindLinkFree(CNodesWorld *self, const vec3_t *const *pair, int *out);
/* the next start of the generation (NWSup_NextInit), the way to the cell free of the level */
byte CNodesWorld_NextInit(CNodesWorld *self, vec3_t *out);
/* the buffer of the box min max: the cells reached from init free of the level (NWSup_IniBuffer); with the
 * analysis also the cells outside the sectors and the triangles around them (LogTri) */
byte CNodesWorld_IniBuffer(CNodesWorld *self, const vec3_t *init, const vec3_t *min, const vec3_t *max);
/* the triangle tri hit around p into tris and the file of the analysis (not twice, at most 1000); hit is changed
 * into meters with y and z swapped */
void CNodesWorld_LogTri(CNodesWorld *self, const vec3_t *p, vec3_t *hit, const struct CCollisionTri *tri);
/* the region of the new point of visibility (NWSup_CreateRegion): the cells nearer than f40 that a sphere reaches */
void CNodesWorld_CreateRegion(CNodesWorld *self, const vec3_t *start);
/* the points of visibility between the regions (NWSup_InterRegion) */
void CNodesWorld_InterRegion(CNodesWorld *self);
/* NWSup_Generate: the start free of the level */
byte CNodesWorld_Generate(CNodesWorld *self, const vec3_t *init, const vec3_t *min, const vec3_t *max);
/* the debug view of g_NWDrawMode */
void CNodesWorld_Draw(CNodesWorld *self);
/* not called in the exe: the link test of a and b as spheres and a line (red with a hit, counted in i358), the box
 * of the cell at p (red in the level) */
void CNodesWorld_DrawLinkTest(CNodesWorld *self, const vec3_t *a, const vec3_t *b);
void CNodesWorld_DrawCell(CNodesWorld *self, const vec3_t *p);
/* not called in the exe: Unreachable over the links whose point the sphere leaves free to the center of the cell */
byte CNodesWorld_UnreachableFree(CNodesWorld *self, const vec3_t *from, const vec3_t *to, int index);
/* the analysis of the level into <name>/AI/AnalisisMap.txt (writes a file): the buffer from init with the triangles
 * around its cells, whether the region is open; 0 for a start in the level or outside the box */
byte CNodesWorld_AnalyzeMap(CNodesWorld *self, const vec3_t *init, const vec3_t *min, const vec3_t *max);

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
/* CNodesWorld_ctor: ai.h */
void CNodesWorld_dtor(CNodesWorld *self);
CNodesWorld *CNodesWorld_Delete(CNodesWorld *self, byte flags);
/* the graph file <name>/AI/PathAI of the base class */
byte CNodesWorld_LoadGraph(CNodesWorld *self);
/* the points and their links into <name>/AI/PathAI.pth (writes a file) */
byte CNodesWorld_SavePath(CNodesWorld *self);
/* <name>/AI/InfPathAI.pth of the base class (writes a file) */
byte CNodesWorld_SaveInfPath(CNodesWorld *self);
void CNodesWorld_ClearLists(CNodesWorld *self);
/* the segment a b free of the level, also the 4 (mode 1: 3) segments around it at radius r; self not used */
byte CNodesWorld_TestPath(CNodesWorld *self, const vec3_t *a, const vec3_t *b, float r, float r2,
	cNWSegList *debug, int mode);
/* the corner a b c at b free of the level: the segments between the sides of a b and of b c at radius r (3 pairs,
 * flat: the other side of the pairs); self not used */
byte CNodesWorld_TestCorner(CNodesWorld *self, const vec3_t *a, const vec3_t *b, const vec3_t *c, float r,
	float r2, byte flat, cNWSegList *debug);
/* the analysis of a segment of the level: 1 when it is free (hit = b, the hit of tri FLT_MAX; outdoors 0 straight
 * up) or it hits a solid triangle facing it (hitFrom and hitTo kept unless b3b0) */
byte CNodesWorld_TestSurface(CNodesWorld *self, const vec3_t *a, const vec3_t *b, vec3_t *hit,
	struct CCollisionTri *tri);
/* TestSurface from pos along the 6 axis directions as far as the largest side of the level box */
byte CNodesWorld_TestSurfaceAround(CNodesWorld *self, const vec3_t *pos, vec3_t *hit, struct CCollisionTri *tri);
/* the points of visibility of the cell between distMin and the visibility distance (and distMax with useMax):
 * mode 0 TestPath with radius, 1 the segment, 2 indoors on the same floor */
void CNodesWorld_CollectCell(CNodesWorld *self, const vec3_t *pos, cNWIntList *cell, const vec3_t **list,
	int max, int *count, float radius, float distMin, float distMax, int mode, byte useMax);
/* the cells ring by ring around the cell of pos; soft goes on past distMax until one is found */
void CNodesWorld_SearchRings(CNodesWorld *self, const vec3_t *pos, const vec3_t **list, int max, int *count,
	float radius, float distMin, float distMax, byte soft, int mode);
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
/* a random point between the node and a random linked node, at most maxDist from the node */
void CNodesWorld_RandomNearNode(CNodesWorld *self, int node, float maxDist, vec3_t *out);
/* such a point (without maxDist) between distMin and distMax from pos in the cone of maxAngle around dir, else
 * RandomNearNode with maxDist */
void CNodesWorld_RandomAheadNode(CNodesWorld *self, int node, const vec3_t *pos, float distMin,
	float distMax, float maxDist, const vec3_t *dir, float maxAngle, vec3_t *out);
/* CNodesWorld_GetRacePoint (a random point of visibility on the floor of initial between minDist and maxDist),
 * CNodesWorld_Analyze (Load in the analysis mode, writes AnalisisMap.txt) and CNodesWorld_AnalyzeGraph: ai.h */

/* ---- AI/AStar.cpp (another source, not in C): a path over the visibility graph from from to to; startOk and
 * goalOk 0 when the end has no point of visibility ---- */
byte CAStar_FindPath(void *astar, cNWVecList *path, const vec3_t *to, const vec3_t *from, float radius,
	float dist, int stupid, byte *startOk, byte *goalOk, int *state);
void CAStar_GetPointsVis(void *astar, const vec3_t *pos, const vec3_t **list, int max, int *count, float r);

#endif
