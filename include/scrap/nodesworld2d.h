/*
 * AI/NodesWorld2D.cpp of the game: CNodesWorld2D, the graph of the walking characters of the indoor levels. It is a
 * CNodesWorldSup (include/scrap/nodesworldsup.h, the first 0x360 bytes of CNodesWorld) with a list of regions of
 * the generation. The points of visibility stand on the floor: a point fits when the three bodies of the table
 * g_NW2DBodies (a capsule each, set by Load) stand on the floor below it, and a path is walked in steps of 30
 * between such points. The graph files are <level>/AI/Path2DAI.pth and on.
 */
#ifndef SCRAP_NODESWORLD2D_H
#define SCRAP_NODESWORLD2D_H

#include "scrap/nodesworldsup.h"

/* a region of the generation: its points (0x1c bytes) */
typedef struct cNW2DRegion {
	int id;				/* 0x00 */
	cNWVecList points;		/* 0x04 */
} cNW2DRegion;
SD_SIZE(cNW2DRegion, 0x1c);

typedef struct cNW2DRegionNode {
	struct cNW2DRegionNode *next;	/* 0x00 */
	struct cNW2DRegionNode *prev;	/* 0x04 */
	cNW2DRegion r;			/* 0x08 */
} cNW2DRegionNode;
SD_SIZE(cNW2DRegionNode, 0x24);
typedef DLIST(cNW2DRegionNode) cNW2DRegionList;

struct CNodesWorld2D {
	byte sup[0x360];		/* 0x000 the CNodesWorldSup part, the layout of CNodesWorld */
	cNW2DRegionList regions;	/* 0x360 */
};
SD_SIZE(CNodesWorld2D, 0x378);

/* the CNodesWorldSup part */
#define NW2D_SUP(self) ((CNodesWorld *)(self))

#define CNODESWORLD2D_VFTABLE GAME_VFTABLE(CNodesWorld2D_vftable, 0x3abce8)

/* a body that walks the graph: a capsule of radius r - shrink between the heights h1 and h2 above a foot sphere of
 * radius foot */
typedef struct cNW2DBody {
	float r;			/* 0x0 */
	float h1;			/* 0x4 */
	float h2;			/* 0x8 */
	float foot;			/* 0xc */
} cNW2DBody;
SD_SIZE(cNW2DBody, 0x10);

#define g_NW2DBodies (&GAME_GLOBAL_IN(cNW2DBody, g_NW2DBodies, 0, 0x444d98))	/* 3 bodies */
#define g_NW2DDrawMode GAME_GLOBAL_IN(int, GVar_ShowNodes2DAI, 16, 0x444ddc)	/* what the debug view of the graph draws */
#define g_NW2DShowNode GAME_GLOBAL_IN(int, GVar_ShowInfoRegAI, 16, 0x444e54)	/* the node the debug view shows, -1 none */
#define g_NW2DGroundDist GAME_GLOBAL_IN(float, GVar_AltPerpFloorAI, 16, 0x444df4)	/* the length of the floor probes */
#define g_NW2DHeight GAME_GLOBAL_IN(float, GVar_AltMinTestWalkAI, 16, 0x444e24)	/* the height of the points over the floor */
#define g_NW2DDebugSegs GAME_GLOBAL_IN(int, GVar_ShowTestCanGoAI, 16, 0x444e3c)	/* the path tests keep their segments in the list they get */

/* ---- Global/DList.h instances ---- */

/* the destructor of a region: its points emptied and their free nodes deleted (fastcall on the region) */
void __fastcall NW2DRegion_dtor(cNW2DRegion *r);
void __thiscall NW2DRegionList_Add(cNW2DRegionList *list, const cNW2DRegion *r);
/* to the free list while it has room, else the points and the node deleted */
void __thiscall NW2DRegionList_Release(cNW2DRegionList *list, cNW2DRegionNode *node);
void __fastcall NW2DRegionList_Clear(cNW2DRegionList *list);
void __fastcall NW2DRegionList_FreePool(cNW2DRegionList *list);

/* ---- CNodesWorld2D ---- */

/* CNodesWorld2D_ctor: ai.h */
void __fastcall CNodesWorld2D_dtor(CNodesWorld2D *self);
CNodesWorld2D *__thiscall CNodesWorld2D_Delete(CNodesWorld2D *self, byte flags);
/* <name>/AI/Path2DAI of the base class */
byte __thiscall CNodesWorld2D_LoadGraph(CNodesWorld2D *self);
/* the floor points of the points and their links into <name>/AI/Path2DAI.pth (writes a file) */
byte __thiscall CNodesWorld2D_SavePath(CNodesWorld2D *self);
/* <name>/AI/InfPath2DAI.pth of the base class (writes a file) */
byte __thiscall CNodesWorld2D_SaveInfPath(CNodesWorld2D *self);
void __thiscall CNodesWorld2D_ClearLists(CNodesWorld2D *self);
/* the floor below p: at p, again at p + 0, then 15 higher, each g_NW2DGroundDist down; 1 with hit set (the probes
 * the callers inline, also AI/AICharControl2D.cpp) */
int NW2D_Probe(const vec3_t *p, vec3_t *hit);
/* the floor below p (three probes of g_NW2DGroundDist: at p, again, 15 higher) raised by height: 1; else hit = p,
 * 0; self not used */
byte __thiscall CNodesWorld2D_Ground(CNodesWorld2D *self, const vec3_t *p, vec3_t *hit, float height);
/* the floor below p raised by height sees b; self not used */
byte __thiscall CNodesWorld2D_GroundSees(CNodesWorld2D *self, const vec3_t *p, const vec3_t *b, float height);
/* the floor points of a and b see each other; the last three arguments not used */
byte __thiscall CNodesWorld2D_GroundLine(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, int u0, int u1, int u2);
/* the body at x y z stands on the floor down below it: out its position on the floor; self not used */
byte __thiscall CNodesWorld2D_FitAt(CNodesWorld2D *self, float x, float y, float z, vec3_t *out, float down, float foot,
	float r, float h2, float h1);
/* the body walks from a towards b in steps of 30 on the floor (climbs at most up); self not used */
byte __thiscall CNodesWorld2D_Walk(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float up, float foot,
	float r, float h2, float h1);
/* the body fits at a and b and walks both ways */
byte __thiscall CNodesWorld2D_WalkFit(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float up, float foot,
	float r, float h2, float h1);
/* the three bodies (radius less shrink) fit at a and b and walk both ways */
byte __thiscall CNodesWorld2D_TestWalk(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float shrink);
/* the body of radius r at the foot height of a walks straight towards b (y kept), or what it hits leaves room 20
 * (at most half the way) before it for a sphere of r on the floor there */
byte __thiscall CNodesWorld2D_TestStep(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float r, float h);
/* the floor points of a and b (raised by max(h - r, g_NW2DHeight)) see each other, the segments at radius r around
 * the path at height h are free (the side, up, and the two between), and TestStep both ways; r2 and h2 not used,
 * debug as in CNodesWorld_TestPath */
byte __thiscall CNodesWorld2D_TestPath(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float r, float r2,
	float h, float h2, cNWSegList *debug);
/* the corner a b c at b free on the floor: the segments between the sides of a b and of b c at radius r and height h,
 * turned up and down (not flat: the other side); r2 and h2 not used, debug as in CNodesWorld_TestPath */
byte __thiscall CNodesWorld2D_TestCorner(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, const vec3_t *c,
	float r, float r2, float h, float h2, byte flat, cNWSegList *debug);
/* the generation of the graph (NWSup_Generate): the bodies walk at the start */
byte __thiscall CNodesWorld2D_Generate(CNodesWorld2D *self, const vec3_t *init, const vec3_t *min, const vec3_t *max);
/* the debug view of g_NW2DDrawMode, then the node g_NW2DShowNode with the cells of its region */
void __fastcall CNodesWorld2D_Draw(CNodesWorld2D *self);
/* not called in the exe: Unreachable over the links whose point the bodies walk from to the center of the cell */
byte __thiscall CNodesWorld2D_UnreachableFree(CNodesWorld2D *self, const vec3_t *from, const vec3_t *to, int index);
/* the cell to link the pair of points through (NWSup_FindLinkBox, NWSup_FindLinkCell); Free: the cells of tmp1c8
 * whose center sees both points on the floor and walks to them */
byte __thiscall CNodesWorld2D_FindLinkBox(CNodesWorld2D *self, const vec3_t *const *pair, const cNWBox *box,
	int *out);
byte __thiscall CNodesWorld2D_FindLinkCell(CNodesWorld2D *self, const vec3_t *const *pair, int *out);
byte __thiscall CNodesWorld2D_FindLinkFree(CNodesWorld2D *self, const vec3_t *const *pair, int *out);
/* the next start of the generation (NWSup_NextInit), the floor points see each other and the bodies walk */
byte __thiscall CNodesWorld2D_NextInit(CNodesWorld2D *self, vec3_t *out);
/* the buffer of the box (NWSup_IniBuffer): the cells the bodies walk to on the floor */
byte __thiscall CNodesWorld2D_IniBuffer(CNodesWorld2D *self, const vec3_t *init, const vec3_t *min,
	const vec3_t *max);
/* the region of the new point of visibility (NWSup_CreateRegion): the cells the bodies walk to */
void __thiscall CNodesWorld2D_CreateRegion(CNodesWorld2D *self, const vec3_t *start);
/* the points of visibility between the regions (NWSup_InterRegion) */
void __fastcall CNodesWorld2D_InterRegion(CNodesWorld2D *self);
/* the pairs of points nearer than 2 * f40 that are not linked, into list sorted by the distance on the floor */
void __thiscall CNodesWorld2D_GetPairs(CNodesWorld2D *self, cNWPairList *list);
/* the extra points into the points, linked to the points they walk to */
void __thiscall CNodesWorld2D_LinkExtra(CNodesWorld2D *self);
/* the dummies DM_PointGraph2DAI* into extraPoints (a fatal error for one that does not fit) */
void __thiscall CNodesWorld2D_AddExtraPoints(CNodesWorld2D *self);
/* the floor distance from pos to the nearest point of the set of its buffer cell, FLT_MAX without one */
float __thiscall CNodesWorld2D_NearestVisDist(CNodesWorld2D *self, const vec3_t *pos);
/* the points of visibility of the cell between distMin and the visibility distance (and distMax with useMax):
 * mode 0 TestPath, 1 GroundSees, 2 indoors on the same floor */
void __thiscall CNodesWorld2D_CollectCell(CNodesWorld2D *self, const vec3_t *pos, cNWIntList *cell,
	const vec3_t **list, int max, int *count, float r, float h1, float h2, float distMin, float distMax, int mode,
	byte useMax);
void __thiscall CNodesWorld2D_SearchRings(CNodesWorld2D *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float r, float h1, float h2, float distMin, float distMax, byte soft, int mode);
void __thiscall CNodesWorld2D_GetPointsVis(CNodesWorld2D *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float r, float h1, float h2, float distMin, float distMax, byte soft, int mode);
/* a point of visibility at most 2500 away that TestPath sees */
byte __thiscall CNodesWorld2D_HasPointVis(CNodesWorld2D *self, const vec3_t *pos, float r, float h1, float h2);
/* the nearest point of visibility the segment sees, else the nearest one */
void __thiscall CNodesWorld2D_NearestPointVis(CNodesWorld2D *self, const vec3_t *pos, vec3_t *out);
/* the path of CNodesWorld_BuildPath, the angles on the floor (without y) */
byte __thiscall CNodesWorld2D_BuildPath(CNodesWorld2D *self, cNWVecList *path, int depth, const vec3_t *dir,
	const vec3_t *start, byte direct, cNWNode **node, float r, float h1, float h2, byte flat);
/* CNodesWorld2D_Load and CNodesWorld2D_AnalyzeGraph: ai.h */

#endif
