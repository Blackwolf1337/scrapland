/*
 * AI/NodesWorldSup.cpp of the game: CNodesWorldSup, the base class of the graphs CNodesWorld and CNodesWorld2D
 * (the layout is the first 0x360 bytes of CNodesWorld in include/scrap/nodesworld.h). It loads the graph files
 * (<path>.pth, <path>1.pth ...), builds the grid of the points of visibility (Global/GridElements.h with the node
 * pointers of the points) and keeps the tools of the generation: the buffer of cells with the sets of points seen
 * from them (shared and reference counted, in hash lists), the links between the generated points and the sorted
 * Global/DList.h lists of the searches.
 */
#ifndef SCRAP_NODESWORLDSUP_H
#define SCRAP_NODESWORLDSUP_H

#include "scrap/nodestraffic.h"

/* the cNWPointNode of a point pos (the data of the point lists and links) */
#define NW_POINT(v) ((cNWPointNode *)((byte *)(v) - offsetof(cNWPointNode, pos)))

#define NWGRID_VFTABLE 0x3ac354
#define CNODESWORLDSUP_VFTABLE 0x3ac264

/* ---- Global/DList.h instances ---- */

/* the free nodes of the point list deleted with their links */
void __fastcall NWPointList_FreePool(cNWPointList *list);
void __thiscall NWVisList_Add(cNWVisList *list, const cNWVis *vis);
/* to the free list while it has room, else the points and the node deleted */
void __thiscall NWVisList_Release(cNWVisList *list, cNWVisNode *node);
void __fastcall NWVisList_RemoveHead(cNWVisList *list);
void __fastcall NWVisList_RemoveIter(cNWVisList *list);
void __fastcall NWVisList_Clear(cNWVisList *list);
/* in front of the iterator, the head kept (Add when the iterator is the head) */
void __thiscall NWVecList_AddBefore(cNWVecList *list, const vec3_t *v);
/* at the end, head stays */
void __thiscall NWVecList_Append(cNWVecList *list, const vec3_t *v);
void __thiscall NWIntList_Append(cNWIntList *list, void *const *data);
/* v added in front of head, the iterator kept */
void NWIntList_AddKeepIter(cNWIntList *list, const void *v);

/* ---- Global/GridElements.h with the node pointers of the points (cells of cNWIntList) ---- */

cGridElements *__thiscall NWGrid_ctor(cGridElements *self, const vec3_t *max, const vec3_t *min, int nx, int ny,
	int nz);
void __fastcall NWGrid_dtor(cGridElements *self);
cGridElements *__thiscall NWGrid_Delete(cGridElements *self, byte flags);
void __thiscall NWGrid_Free(cGridElements *self);
byte __thiscall NWGrid_Add(cGridElements *self, const vec3_t *p, void *const *data);

/* ---- CNodesWorldSup (thiscall on the CNodesWorld layout; vtable slots in CNodesWorldVtbl) ---- */

/* CNodesWorldSup_ctor, _dtor, _FreeGraph and _LoadGraph: nodesworld.h */
CNodesWorld *__thiscall CNodesWorldSup_Delete(CNodesWorld *self, byte flags);
void __thiscall CNodesWorldSup_GetBox(CNodesWorld *self, vec3_t *min, vec3_t *max);
byte __thiscall CNodesWorldSup_Create(CNodesWorld *self);
int __thiscall CNodesWorldSup_FileOf(CNodesWorld *self, const cNWNode *node);
int __thiscall CNodesWorldSup_CommonFile(CNodesWorld *self, cNWNode *const *nodes, int n);
vec3_t *__thiscall CNodesWorldSup_SnapPos(CNodesWorld *self, vec3_t *out, const vec3_t *pos);
void __thiscall CNodesWorldSup_OtherLinks(CNodesWorld *self, const cNWNode *node, const vec3_t *pos, cNWNode **list,
	int max, int *count);
byte __thiscall CNodesWorldSup_IsCreated(CNodesWorld *self);
vec3_t *__thiscall CNodesWorldSup_LastNodePos(CNodesWorld *self, vec3_t *out);
const vec3_t *__thiscall CNodesWorldSup_GetVisPoint(CNodesWorld *self, int i);
int __thiscall CNodesWorldSup_NumVisPoints(CNodesWorld *self);
float __thiscall CNodesWorldSup_GetMaxVisDist(CNodesWorld *self);
byte __thiscall CNodesWorldSup_InGrid(CNodesWorld *self, const vec3_t *pos);
/* every buffer index back from its position; a fatal error if one differs */
byte __thiscall CNodesWorldSup_CheckBuffer(CNodesWorld *self);
/* the grid of the points over the box of the level (outdoors up to DM_MapTop + 5000), cells of f54 */
void __thiscall CNodesWorldSup_BuildGrid(CNodesWorld *self);
int __thiscall CNodesWorldSup_BufferIndex(CNodesWorld *self, const vec3_t *pos);
vec3_t *__thiscall CNodesWorldSup_BufferPos(CNodesWorld *self, vec3_t *out, int index);
/* the positions of the buffer cells into lists28c by state; the warning when no cell is outside (state 6) */
void __thiscall CNodesWorldSup_GetInfoNodes(CNodesWorld *self);
/* the point added to the set of the cell: an equal set of the hash list is shared, else the set is changed (one
 * user) or copied */
void __thiscall CNodesWorldSup_AddVis(CNodesWorld *self, const vec3_t *point, int index);
int __thiscall CNodesWorldSup_Hash(CNodesWorld *self, int a, int b);
/* one user less of the set of the cell; the last one frees it */
void __thiscall CNodesWorldSup_ReleaseVis(CNodesWorld *self, int index);
/* each point linked to the points of the set of its cell, then LinkBack; numPointLinks the pairs */
void __thiscall CNodesWorldSup_LinkPoints(CNodesWorld *self);
/* the links made both ways */
void __thiscall CNodesWorldSup_LinkBack(CNodesWorld *self);
byte __thiscall CNodesWorldSup_PointLinked(CNodesWorld *self, const vec3_t *point, const vec3_t *other);
/* the point linked both ways to the points of the set of its cell */
void __thiscall CNodesWorldSup_LinkPoint(CNodesWorld *self, const vec3_t *point);
/* to is not reached from from over links whose points are in the set of the cell index */
byte __thiscall CNodesWorldSup_Unreachable(CNodesWorld *self, const vec3_t *from, const vec3_t *to, int index);
int __thiscall CNodesWorldSup_BoxCells(CNodesWorld *self, const cNWBox *box);
/* key in the list sorted by address: 1; else 0 and at the node to insert in front of */
byte __thiscall CNodesWorldSup_FindSorted(CNodesWorld *self, cNWIntList *list, const void *key, cNWIntNode **at);
byte __thiscall CNodesWorldSup_HasPair(CNodesWorld *self, cNWPairList *list, const cNWPair *pair);
/* the pair (a, b, dist) into the list sorted by distance, not twice */
void __thiscall CNodesWorldSup_InsertPair(CNodesWorld *self, cNWPairList *list, const cNWPair *pair);
byte __thiscall CNodesWorldSup_VisHas(CNodesWorld *self, const vec3_t *point, int index);
byte __thiscall CNodesWorldSup_Linked(CNodesWorld *self, const vec3_t *a, const vec3_t *b);
void __thiscall CNodesWorldSup_FreeTemp(CNodesWorld *self);
byte __thiscall CNodesWorldSup_InList(CNodesWorld *self, cNWNode *const *list, int n, const cNWNode *node);

/* ---- the cells that link two points of the generation (CNodesWorld and CNodesWorld2D) ---- */

/* the distance of the generation: the squares of y, x, then z summed */
float NWSup_Dist(const vec3_t *c, const vec3_t *p);
/* the distances from c to the two points summed, flat: on the floor plane */
float NWSup_PairDist(const vec3_t *c, const vec3_t *const *pair, byte flat);
/* c nearer than f40 to both points */
byte NWSup_Near(const CNodesWorld *self, const vec3_t *c, const vec3_t *const *pair);
/* the best cell {index, a, b} into the first debug list when drawMode is on */
void NWSup_LinkFound(CNodesWorld *self, const int *best, int drawMode);
/* the nearest cell of the box (2D: also not state 0) whose set has both points that do not reach each other over
 * its links; out its index */
byte NWSup_FindLinkBox(CNodesWorld *self, const vec3_t *const *pair, const cNWBox *box, int *out, byte is2D);
/* the same over the cells of tmp1c8 */
byte NWSup_FindLinkCell(CNodesWorld *self, const vec3_t *const *pair, int *out, byte is2D);
/* the next start of the generation: the free neighbour cell (state 4) of a cell of list1d0 in state 5 whose point
 * of visibility is the nearest one, isFree tests the way from the neighbour to the cell */
typedef int (*nwInitFree_t)(CNodesWorld *self, const vec3_t *n, const vec3_t *c);
typedef float (*nwInitDist_t)(CNodesWorld *self, const vec3_t *n);
byte NWSup_NextInit(CNodesWorld *self, vec3_t *out, nwInitFree_t isFree, nwInitDist_t dist);
/* the buffer of the generation for the box min max (nx ny nz, state 6), the cells reached from the cell of init
 * over the neighbours along the axes into state 0 when step lets them (step also sets the state of the others),
 * then tmp1c8 with the reached cells and the hash lists of the sets; 3D: open (and 0) when the region reaches the
 * side of the box with nodeInitDummy (the top only indoors), the analysis of the level */
typedef void (*nwFloodStep_t)(CNodesWorld *self, cNWVecList *queue, const vec3_t *c, const vec3_t *n);
byte NWSup_IniBuffer(CNodesWorld *self, const vec3_t *init, const vec3_t *min, const vec3_t *max, nwFloodStep_t step,
	byte is3D);
/* the region of the new point (the head of points) grown from start over the cells reach lets in: the cells into the
 * set of the point (AddVis) and its box, then 4, the border 5; 2D: the cells into a new debug region when the view
 * is on */
typedef int (*nwRegionReach_t)(CNodesWorld *self, const vec3_t *start, const vec3_t *n);
void NWSup_CreateRegion(CNodesWorld *self, const vec3_t *start, nwRegionReach_t reach, byte is2D);
/* the steps of the generation that differ between CNodesWorld and CNodesWorld2D */
typedef struct nwGenOps {
	void (*getPairs)(CNodesWorld *self, cNWPairList *list);
	byte (*findBox)(CNodesWorld *self, const vec3_t *const *pair, const cNWBox *box, int *out);
	byte (*findCell)(CNodesWorld *self, const vec3_t *const *pair, int *out);
	void (*createRegion)(CNodesWorld *self, const vec3_t *start);
	/* the start cell is free */
	int (*initOk)(CNodesWorld *self, const vec3_t *p);
	byte (*iniBuffer)(CNodesWorld *self, const vec3_t *init, const vec3_t *min, const vec3_t *max);
	byte (*nextInit)(CNodesWorld *self, vec3_t *out);
	void (*interRegion)(CNodesWorld *self);
	void (*linkExtra)(CNodesWorld *self);
	byte is2D;
} nwGenOps;
/* "hh:mm:ss" of the seconds from t0 to t1 into the console with fmt */
void NWSup_PrintTime(const char *fmt, int32_t t1, int32_t t0);
/* the pairs of points not linked: a new point of visibility with its region in the cell that links them, until no
 * pair has such a cell */
void NWSup_InterRegion(CNodesWorld *self, const nwGenOps *ops);
/* the graph of the box from init: the regions of the points of visibility from start to start (NextInit), their
 * links, the points between the regions and the extra points; the start snapped to its cell must be inside the box
 * and free (a fatal error else) */
byte NWSup_Generate(CNodesWorld *self, const vec3_t *init, const vec3_t *min, const vec3_t *max, const nwGenOps *ops);
/* the debug view: the number of a node over it, the found cells (index, a, b) as spheres with the lines to a and
 * b, the cells of a list as boxes (and their count with fmt), the extra points, the errors of AnalizeGraph (links
 * through the level: triples with the radius r on the third, 2D pairs; the nodes without links, one way links) */
void NWSup_DrawNodeText(const vec3_t *n, int i);
void NWSup_DrawFound(CNodesWorld *self, cNWVecList *list, float r, uint32_t color);
void NWSup_DrawCells(CNodesWorld *self, cNWVecList *list, uint32_t color, const char *fmt);
void NWSup_DrawExtra(CNodesWorld *self, float r, uint32_t color);
void NWSup_DrawCollide(CNodesWorld *self, float r, byte triples);
void NWSup_DrawErrors(CNodesWorld *self, float r);

#endif
