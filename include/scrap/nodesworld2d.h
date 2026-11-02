/*
 * AI/NodesWorld2D.cpp of the game: CNodesWorld2D, the graph of the walking characters of the indoor levels. It is a
 * CNodesWorldSup (include/scrap/nodesworldsup.h, the first 0x360 bytes of CNodesWorld) with a list of regions of
 * the generation. The points of visibility stand on the floor: a point fits when the three bodies of the table
 * g_NW2DBodies (a capsule each, set by Load) stand on the floor below it, and a path is walked in steps of 30
 * between such points. The graph files are <level>/AI/Path2DAI.pth and on.
 */
#ifndef SCRAP_NODESWORLD2D_H
#define SCRAP_NODESWORLD2D_H

#include "scrap/gvar.h"
#include "scrap/nodesworldsup.h"

/* a region of the generation: its points (0x1c bytes) */
typedef struct cNW2DRegion {
	int id;				/* 0x00 */
	cNWVecList points;		/* 0x04 */
} cNW2DRegion;

typedef struct cNW2DRegionNode {
	struct cNW2DRegionNode *next;	/* 0x00 */
	struct cNW2DRegionNode *prev;	/* 0x04 */
	cNW2DRegion r;			/* 0x08 */
} cNW2DRegionNode;
typedef DLIST(cNW2DRegionNode) cNW2DRegionList;

struct CNodesWorld2D {
	CNodesWorldSup sup;		/* 0x000 the code uses the CNodesWorld layout (NW2D_SUP) */
	cNW2DRegionList regions;	/* 0x360 */
};

/* the CNodesWorldSup part */
#define NW2D_SUP(self) ((CNodesWorld *)(self))

/* a body that walks the graph: a capsule of radius r - shrink between the heights h1 and h2 above a foot sphere of
 * radius foot */
typedef struct cNW2DBody {
	float r;			/* 0x0 */
	float h1;			/* 0x4 */
	float h2;			/* 0x8 */
	float foot;			/* 0xc */
} cNW2DBody;

extern cGVar GVar_ShowNodes2DAI;
#define g_NW2DDrawMode (GVar_ShowNodes2DAI.value.i)	/* what the debug view of the graph draws */
extern cGVar GVar_AltMinTestWalkAI;
#define g_NW2DHeight (GVar_AltMinTestWalkAI.value.f)	/* the height of the points over the floor */
extern cGVar GVar_ShowTestCanGoAI;
#define g_NW2DDebugSegs (GVar_ShowTestCanGoAI.value.i)	/* the path tests keep their segments in the list they get */

/* ---- Global/DList.h instances ---- */

void NW2DRegionList_Add(cNW2DRegionList *list, const cNW2DRegion *r);

/* ---- CNodesWorld2D ---- */

/* the floor below p: at p, again at p + 0, then 15 higher, each g_NW2DGroundDist down; 1 with hit set (the probes
 * the callers inline, also AI/AICharControl2D.cpp) */
int NW2D_Probe(const vec3_t *p, vec3_t *hit);
/* the floor points of a and b (raised by max(h - r, g_NW2DHeight)) see each other, the segments at radius r around
 * the path at height h are free (the side, up, and the two between), and TestStep both ways; r2 and h2 not used,
 * debug as in CNodesWorld_TestPath */
byte CNodesWorld2D_TestPath(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, float r, float r2,
	float h, float h2, cNWSegList *debug);
/* the corner a b c at b free on the floor: the segments between the sides of a b and of b c at radius r and height h,
 * turned up and down (not flat: the other side); r2 and h2 not used, debug as in CNodesWorld_TestPath */
byte CNodesWorld2D_TestCorner(CNodesWorld2D *self, const vec3_t *a, const vec3_t *b, const vec3_t *c,
	float r, float r2, float h, float h2, byte flat, cNWSegList *debug);
/* the debug view of g_NW2DDrawMode, then the node g_NW2DShowNode with the cells of its region */
void CNodesWorld2D_Draw(CNodesWorld2D *self);
void CNodesWorld2D_GetPointsVis(CNodesWorld2D *self, const vec3_t *pos, const vec3_t **list, int max,
	int *count, float r, float h1, float h2, float distMin, float distMax, byte soft, int mode);
/* a point of visibility at most 2500 away that TestPath sees */
byte CNodesWorld2D_HasPointVis(CNodesWorld2D *self, const vec3_t *pos, float r, float h1, float h2);
/* the nearest point of visibility the segment sees, else the nearest one */
void CNodesWorld2D_NearestPointVis(CNodesWorld2D *self, const vec3_t *pos, vec3_t *out);
/* the path of CNodesWorld_BuildPath, the angles on the floor (without y) */
byte CNodesWorld2D_BuildPath(CNodesWorld2D *self, cNWVecList *path, int depth, const vec3_t *dir,
	const vec3_t *start, byte direct, cNWNode **node, float r, float h1, float h2, byte flat);
/* CNodesWorld2D_Load and CNodesWorld2D_AnalyzeGraph: ai.h */

#endif
