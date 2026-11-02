/*
 * CGround of the engine3d (original source Engine/suelo.cpp): the floor of a scene. It keeps the scene meshes split
 * into a grid of floor nodes (nodes, CNode3D) and the collision data (heights): the collision meshes of the sectors
 * (CCollideMesh, triangles with their planes and a tested mark, engine3d/engine/ground.h) and a grid of quads
 * (CCollideQuad, the triangles of one mesh that touch a cell, chained per cell). The collision data is read from or
 * saved to the .amc file next to the scene. The traces test a segment or a moving sphere against the triangles of
 * the cells it passes; every query has a number (gCollisionNComp) so that a triangle shared by cells is tested once.
 */
#ifndef SCRAP_GROUND_H
#define SCRAP_GROUND_H

#include "scrap/gvar.h"
#include "scrap/archio.h"
#include "scrap/mathlib.h"
#include "scrap/node3d.h"
#include "scrap/table.h"

#define MAX_COLLIDE_SECTORS 128
#define CNDX_MAX 0xffff
#define AMC_VER 100

/* CCollideTri.flags */
#define TRI_ALT_NO_COLLISIONS 0x1	/* no collisions (also a whole mesh in CCollideMesh.flags) */
#define TRI_ALT_INVALID 0x3		/* both bits: a degenerate or invalid triangle */
#define TRI_ALT_MESH 0x4		/* of a mesh added at run time */
#define TRI_ALT_TESTED 0x1000		/* debug marks of the last queries */
#define TRI_ALT_HIT 0x2000

typedef struct CCollideTri {
	uint32_t ncomp;			/* 0x00 the query that tested it last */
	plane_t plane;			/* 0x04 */
	uint16_t v[3];			/* 0x14 */
	uint16_t flags;			/* 0x1a */
} CCollideTri;
SD_SIZE(CCollideTri, 0x1c);

typedef struct CCollideMesh {
	int Refs;			/* 0x00 the quads that use it */
	uint16_t flags;			/* 0x04 */
	uint16_t sector;		/* 0x06 the sector + 1 */
	uint16_t MeshID;		/* 0x08 */
	byte Param;			/* 0x0a index in the .amc file, a mark while the quads are walked */
	byte outSector;			/* 0x0b of the outside sector: one quad for the whole mesh */
	box_t Ext;			/* 0x0c */
	union {				/* a CTable: the casts (CTable *)&nVert hold on every platform */
		CTable t_nVert;
		struct {
			int nVert;			/* 0x24 CTable of vec3_t */
			vec3_t *pVert;			/* 0x28 */
		};
	};
	union {				/* a CTable: the casts (CTable *)&nTri hold on every platform */
		CTable t_nTri;
		struct {
			int nTri;			/* 0x2c CTable of CCollideTri */
			CCollideTri *pTri;		/* 0x30 */
		};
	};
} CCollideMesh;
SD_SIZE(CCollideMesh, 0x34);
/* the size the files keep: the 32 bit one (the fields are read one by one) */
#define CCOLLIDEMESH_FILESIZE 0x34
/* the tables freed, CHECK(!Refs) (an inline of ground.h, rva 0x20eb70) */
void CCollideMesh_dtor(CCollideMesh *self);

typedef struct CCollideQuad {
	float gapTop;			/* 0x00 the largest vertical gap without triangles */
	float gapBottom;		/* 0x04 */
	CCollideMesh *Mesh;		/* 0x08 */
	union {				/* a CTable: the casts (CTable *)&nTri hold on every platform */
		CTable t_nTri;
		struct {
			int nTri;			/* 0x0c CTable of the triangle indices */
			uint16_t *pTri;			/* 0x10 */
		};
	};
	float maxY;			/* 0x14 */
	float minY;			/* 0x18 */
	uint32_t dwParam;		/* 0x1c the triangles counted while the quads are built */
	struct CCollideQuad *next;	/* 0x20 the quad of the next mesh in the same cell */
} CCollideQuad;
SD_SIZE(CCollideQuad, 0x24);

/* CGround.heights, its own this in the quad functions */
typedef struct CHeights {
	uint32_t MeshID;		/* 0x0000 the last mesh added at run time */
	CCollideMesh _CollideMesh1[MAX_COLLIDE_SECTORS];	/* 0x0004 per sector */
	CCollideMesh _CollideMesh2[MAX_COLLIDE_SECTORS];	/* 0x1a04 per sector, the no collision triangles */
	CCollideMesh CollideMesh1;	/* 0x3404 */
	CCollideMesh CollideMesh2;	/* 0x3438 */
	box_t ext;			/* 0x346c of the grid */
	vec3_t dim;			/* 0x3484 */
	union {				/* a CTable: the casts (CTable *)&nQuads hold on every platform */
		CTable t_nQuads;
		struct {
			int nQuads;			/* 0x3490 CTable of CCollideQuad, nx * nz */
			CCollideQuad *quads;		/* 0x3494 */
		};
	};
	int nx;				/* 0x3498 */
	int nz;				/* 0x349c */
	float sizeX;			/* 0x34a0 of a cell */
	float sizeZ;			/* 0x34a4 */
	float invX;			/* 0x34a8 cells per unit */
	float invZ;			/* 0x34ac */
	CCollideQuad *outSector;	/* 0x34b0 */
} CHeights;
SD_OFFSET(CHeights, CollideMesh1, 0x3404);
SD_OFFSET(CHeights, ext, 0x346c);
SD_OFFSET(CHeights, nQuads, 0x3490);
SD_OFFSET(CHeights, outSector, 0x34b0);
SD_SIZE(CHeights, 0x34b4);

/* the triangle a segment hit (CGround_TraceSeg): quad->pTri[tri] in the mesh of the quad */
typedef struct CGroundHit {
	int queryId;		/* 0x0 g_QuadTriCacheID when it was found */
	int tri;			/* 0x4 */
	CCollideQuad *quad;	/* 0x8 */
} CGroundHit;

typedef struct CGround {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;	/* 0x00 */
			uint32_t class;			/* 0x04 CLASS_OBJ3D_GROUND */
		};
	};
	int traceStats[3];		/* 0x08 CGround_Trace: triangles tested, cache hits, cache misses */
	CGroundHit cache;		/* 0x14 the last hit of CGround_Trace; cleared whenever the meshes change */
	int nx;				/* 0x20 grid of the floor nodes */
	int nz;				/* 0x24 */
	union {				/* a CTable: the casts (CTable *)&nNodes hold on every platform */
		CTable t_nNodes;
		struct {
			int nNodes;			/* 0x28 CTable of CNode3D (0xfc bytes each) */
			CNode3D *nodes;			/* 0x2c */
		};
	};
	box_t Ext;			/* 0x30 */
	vec3_t Dim;			/* 0x48 */
	box_t CollideExt;		/* 0x54 */
	int CollideTri;			/* 0x6c the triangles that collide */
	CHeights heights;		/* 0x70 */
} CGround;
SD_OFFSET(CGround, Ext, 0x30);
SD_OFFSET(CGround, heights, 0x70);
SD_SIZE(CGround, 0x3524);

/* the quads of cell x z of the grid, NULL outside it (the accessor of table.h inlined) */
static inline CCollideQuad *Heights_Cell(CHeights *alt, uint32_t x, uint32_t z) {
	if (x >= (uint32_t)alt->nx || z >= (uint32_t)alt->nz) {
		return NULL;
	}
	uint32_t i = alt->nx * z + x;
	TABLE_CHECK(alt->quads, i, alt->nQuads);
	return &alt->quads[i];
}

/* rva 0x261530: the floor nodes freed (with the shadow maps they are not needed) */
void CGround_FreeNodes(CGround *self);
/* a vertex of the debug view of the collision meshes (FVF XYZ | DIFFUSE | TEX1) */
typedef struct CollideVert {
	vec3_t pos;			/* 0x00 */
	uint32_t color;			/* 0x0c */
	float u, v;			/* 0x10 */
} CollideVert;
SD_SIZE(CollideVert, 0x18);
/* the three vertices of a triangle, shaded by its normal with the texture projected along its main axis in the
 * solid view (ShowMapSolid) */
void Collide_TriVerts(CollideVert *out, const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color);
void CCollideMesh_DrawDebug(CCollideMesh *self, int nested);
/* the debug view of the collision (c_show, ShowMapQuads, AnalizeQuads) */
void CGround_DrawDebug(CGround *self);
/* rva 0x261580: in / |in| into out (the length through double, 1 / len rounded to float), a copy for length 0 */
vec3_t *Vec3_Normalize(vec3_t *out, const vec3_t *in);

extern float g_CollisionEpsilon;
extern cGVar GVar_MapCollisionBalance;
#define g_CollisionBalance (GVar_MapCollisionBalance.value.f)
extern int g_QuadTriCacheID;
extern uint32_t g_CollisionNComp;
#define g_CollisionTestsMax GAME_GLOBAL_IN(uint32_t, g_BspCacheStats, 8)
extern uint32_t g_CollisionTests;
extern uint32_t g_CollisionTestsTotal;
extern int g_CollisionQuadCount;

/* the triangle not tested yet in this query: marked as tested (counted in g_CollisionTests) */
static inline int CCollideTri_Mark(CCollideTri *tri) {
	if (tri->ncomp == g_CollisionNComp) {
		return 0;
	}
	tri->ncomp = g_CollisionNComp;
	tri->flags |= TRI_ALT_TESTED;
	g_CollisionTests++;
	return 1;
}

/* engine3d/engine/table.h: count, sizeof(T) and the elements */
void Table_Write2(CFileIO *f, CTable *t);
void Table_Write1C(CFileIO *f, CTable *t);
void Table_WriteC(CFileIO *f, CTable *t);
/* elements with a vector deleting destructor (vftable[0]) or plain new[]; the table empty after */
void Table_FreeObjs(CTable *t);

/* the vector deleting destructor of CNode3D (0xfc bytes) as the floor nodes use it */
void *CNode3D_VecDelete(void *self, uint32_t flags);

CCollideMesh *CCollideMesh_ctor(CCollideMesh *self);
/* the plane of triangle i (unit normal, unchanged when it has length 0) */
plane_t *CCollideMesh_TriPlane(CCollideMesh *self, plane_t *out, uint32_t i);
/* a copy of triangle i with a new plane, ncomp 0 */
void CCollideMesh_TriWithPlane(CCollideMesh *self, CCollideTri *out, int i);
/* triangle i and its corners, each output may be NULL */
void CCollideMesh_GetTri(CCollideMesh *self, int i, CCollideTri *tri, vec3_t *v0, vec3_t *v1, vec3_t *v2);
/* the "CMSH" block; the mesh is entered in the load list by its Param */
void CCollideMesh_Read(CCollideMesh *self, CFileIO *f);
void CCollideMesh_Write(CCollideMesh *self, CFileIO *f);

CCollideQuad *CCollideQuad_ctor(CCollideQuad *self);
/* the next quads freed, the mesh released */
void CCollideQuad_dtor(CCollideQuad *self);
CCollideQuad *CCollideQuad_Delete(CCollideQuad *self, uint32_t flags);
/* the highest and the lowest corner of a triangle of the mesh */
float CCollideQuad_TriMaxY(CCollideQuad *self, const CCollideTri *tri);
float CCollideQuad_TriMinY(CCollideQuad *self, const CCollideTri *tri);
/* the index (in pTri) of the colliding triangle with the lowest lowest corner below maxY, -1 for none */
int CCollideQuad_LowestTri(CCollideQuad *self);
/* the "QUAD" block and the next ones */
void CCollideQuad_Read(CCollideQuad *self, CFileIO *f);
void CCollideQuad_Write(CCollideQuad *self, CFileIO *f);
/* the largest vertical gap between the triangles into gapTop and gapBottom */
void CCollideQuad_CalcGap(CCollideQuad *self);

/* the quads of cell x z, NULL outside the grid (table.h) */
CCollideQuad *CHeights_GetCell(CHeights *self, uint32_t x, uint32_t z);
CHeights *CHeights_ctor(CHeights *self);
void CHeights_dtor(CHeights *self);
/* the Param of all meshes of the quads */
void CHeights_SetParam(CHeights *self, byte param);
/* the quad functions below take the CHeights without using it */
/* minY and maxY from the triangles (widened by the epsilon), then the gap */
void CCollideQuad_CalcExt(CHeights *self, CCollideQuad *q);
/* all triangles of the mesh */
void CCollideQuad_AddAllTris(CHeights *self, CCollideQuad *q);
/* the quad of mesh (with that MeshID) in the chain of quad, a new one at the end */
CCollideQuad *CCollideQuad_Find(CHeights *self, CCollideQuad *quad, uint32_t id, CCollideMesh *mesh);
/* the grid over box for about numTri triangles (and the balance) per cell */
void CHeights_QuadInit(CHeights *self, int unused, const box_t *box, int numTri);
/* the triangles of mesh from initTri (0) into the quads of the cells they touch */
void CHeights_QuadAddMesh(CHeights *self, CCollideMesh *mesh, int initTri);

CGround *CGround_ctor(CGround *self);
void CGround_dtor(CGround *self);
IObject3D *CGround_Delete(IObject3D *base, byte flags);
void CGround_Nop(CGround *self, void *arg);
void CGround_NodesVf3(IObject3D *base, void *arg);
void CGround_NodesVf4(IObject3D *base, void *arg);
/* the "SUEL" block: 1 */
byte CGround_Read(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx);
byte CGround_Write(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx);
/* the .amc file of the scene name: 0 loaded, 1 older than the scene, -1 failed */
int CGround_LoadCollision(CGround *self, const char *name);
/* 0 written, -1 failed */
int CGround_SaveCollision(CGround *self, const char *name);
/* a new query number: the tested marks of all triangles cleared when it wraps */
void CGround_NewQuery(CGround *self);
/* the sphere of radius r at b against the floor: the contact that is most a floor (normal nearest to the
 * vertical among the nearest ones); out the position pushed out along it (a for a wall), hit the contact, n the
 * direction from it to b. The y range of the quads tested is that of a to b */
byte CGround_CollideSphere(CGround *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n);
/* the same with the nearest contact; without outputs the first contact ends it */
byte CGround_SphereContact(CGround *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n);
/* a sphere of radius r moving from a to b: the contact nearest to a (out and hit the same point); without outputs the first contact ends it */
byte CGround_SweepSphere(CGround *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n);
struct CTriMesh;

/* Engine/gety.cpp (Gety.c): the highest triangle point below maxY at x, z, -FLT_MAX for none; all: also the
 * triangles and meshes without collisions */
float CGround_GetY(CGround *self, float x, float z, float maxY, byte all);
/* the same between minY and maxY, of the triangles whose normal has a y of 0.6 or more */
float CGround_GetYRange(CGround *self, float x, float z, float maxY, float minY, byte all);
/* the highest point below maxY of all triangles of the cell into out (may be NULL); 0 for none */
byte CGround_GetYBelow(CGround *self, float x, float z, float maxY, float *out);

/* the triangles of tm added to mesh (a mesh added at run time: outID and outIni get its MeshID and first triangle),
 * then into the quads */
void CGround_AddMesh(CGround *self, CCollideMesh *mesh, struct CTriMesh *tm, uint32_t *outID, int *outIni);
/* the quads of mesh taken out of the cells (not the first quad of a cell) */
void CGround_RemoveMesh(CGround *self, CCollideMesh *mesh, int a2, int a3);
/* collisions of a mesh added at run time on or off */
void CGround_SetMeshCollide(CGround *self, CCollideMesh *mesh, byte on, int MeshID, int IniTri);
void CGround_CalcGaps(CGround *self);
/* the triangles of the floor nodes in sector - 1 (all for -1) into mesh: mode 1 the colliding ones, mode 2 the
 * others (the mesh then gets TRI_ALT_NO_COLLISIONS) */
void CGround_BuildSectorMesh(CGround *self, CCollideMesh *mesh, int mode, uint32_t sector);
/* the grid and the meshes of all sectors from the floor nodes; empty cells get CollideMesh1 */
void CGround_CreateQuadCollision(CGround *self);

/* the floor of a scene (CScene): from its .amc file when the scene was loaded with one, else the triangles of its
 * meshes into a grid of floor nodes (one CTriMesh each) and the collision data from the .amc file or built and
 * saved */
void CGround_Build(CGround *self, CScene *scn);
/* the floor nodes to vertex buffer meshes */
void CGround_NodesToD3D(CGround *self, int fvf);
/* the vertex buffer meshes of the floor nodes */
void CGround_Draw(IObject3D *base, void *dev, CRenderNode *unused);
/* a debug box of half size r around c */
void Ground_DrawBox(const vec3_t *c, float r, uint32_t color);
/* the collision debug drawing of the floor of gMundo */
void Ground_DrawDebug(void);

/* (p - a) x (b - a): the side of p to the edge a b, in the operation order of suelo.cpp and checkline3d.cpp */
static inline vec3_t Ground_EdgeCross(const vec3_t *p, const vec3_t *a, const vec3_t *b) {
	float dx = p->x - a->x, dy = p->y - a->y, dz = p->z - a->z;
	float ex = b->x - a->x, ey = b->y - a->y, ez = b->z - a->z;
	return (vec3_t){ dy * ez - dz * ey, dz * ex - dx * ez, dx * ey - dy * ex };
}

/* point p in the triangle a b c (on the side of its normal), also on its edges */
byte Ground_PointInTri(const vec3_t *p, const vec3_t *a, const vec3_t *b, const vec3_t *c);
/* distance of p to the segment a b, the nearest point into out (may be NULL) */
float Ground_DistPointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
float Ground_Dist2PointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
/* the parameter t at which the segment a b comes within r of the plane; 0 if it stays farther away on one side */
byte Ground_SegPlane(const vec3_t *a, const vec3_t *b, float r, const plane_t *pl, float *t);
/* the first t in [0, 1] at which the segment p q comes within r of the edge a b (or of its end a); 0 for none */
byte Ground_SegEdge(const vec3_t *a, const vec3_t *b, const vec3_t *p, const vec3_t *q, float r, float *t);
/* the point of the triangle nearest to the sphere center c, if it is within r */
byte Ground_SphereTri(const vec3_t *c, float r, const CCollideTri *tri, const CCollideMesh *mesh, vec3_t *res);
/* the query of checkbbox.cpp ("colis"): the box min/max (use_minmax 1) or min2/max2 (2), moved by pos */
typedef struct CCollisionBBox {
	byte pad_0[0x18];
	vec3_t pos;			/* 0x18 */
	vec3_t min;			/* 0x24 */
	vec3_t max;			/* 0x30 */
	vec3_t min2;			/* 0x3c */
	vec3_t max2;			/* 0x48 */
	int use_minmax;		/* 0x54 */
	byte pad_58[0x24];
	int calc_response;		/* 0x7c must be 0 */
} CCollisionBBox;
SD_OFFSET(CCollisionBBox, use_minmax, 0x54);
SD_SIZE(CCollisionBBox, 0x80);

/* the queries against the triangles of the floor (suelo.cpp and checkbbox.cpp, src/imports.inc); the last
 * arguments are 0 in all callers */
/* rva 0x26ed30: 1 if the segment from a to b hits a triangle */
byte CGround_TestSeg(CGround *self, const vec3_t *a, const vec3_t *b, int flags);
/* rva 0x26df80 and 0x26f120: also the first hit point, the hit triangle */
/* 1 if the segment from-to hits the floor; res must be NULL; threshold > 0: not when from lies less than it behind the
 * plane; cache: the last hit to try first (NULL: the one of the floor) */
byte CGround_Trace(CGround *self, const vec3_t *from, const vec3_t *to, vec3_t *res, float threshold, CGroundHit *cache);
byte CGround_TraceSegPos(CGround *self, const vec3_t *a, const vec3_t *b, vec3_t *hit, int flags);
byte CGround_TraceSeg(CGround *self, const vec3_t *a, const vec3_t *b, vec3_t *hit, CGroundHit *tri,
	int flags);
/* rva 0x26fb10: 1 if a triangle is in the box */
byte CGround_CheckBBox(CGround *self, CCollisionBBox *collision);

/* checkbbox.cpp */
/* the triangle (its verts in v) touches the box */
byte BoxTri(const vec3_t *bmin, const vec3_t *bmax, const CCollideTri *tri, const vec3_t *v);
/* the convex polygon of count points clipped to the side n.p >= d (a little more, by the collision epsilon) */
void Poly_ClipPlane(const vec3_t *n, float d, int *count, vec3_t *pts);
/* the triangle tri clipped by the box of matrix m and half sizes half: 1 and the center of what is left in it */
int Box_ClipTri(const matrix_t *m, const float *half, const vec3_t *tri, vec3_t *center);
/* how deep p is in that box (the smallest distance to its six sides, < 0 outside) */
float Box_PointDepth(const matrix_t *m, const float *half, const vec3_t *p);
/* the box of matrix m and half sizes half against the floor (drawn as a debug box): the triangle nearest to its
 * center, outPos the center of its part in the box and outN its normal; without outputs the first one ends it */
byte CGround_CollideOBB(CGround *self, const matrix_t *m, const vec3_t *half, vec3_t *outPos, vec3_t *outN);

/* checkline3d.cpp */
/* the cell walk of a segment from a to b through the grid: the cell x z it leaves through first (the next one into x
 * and z, the parameter of the exit into t); 0 in the cell of b (t 1) */
byte Heights_CellStep(CHeights *alt, int *x, int *z, const vec3_t *a, const vec3_t *b, float *t);
/* the same for CGround_Trace on its walk state (g_TraceCell) */
byte Heights_TraceStep(CHeights *alt);
/* p in the triangle a b c (on the side of its normal), also on its edges */
byte Ground_PointInTriIn(const vec3_t *p, const vec3_t *a, const vec3_t *b, const vec3_t *c);

/* the position where a sphere of radius r moving from a to b first touches the triangle */
byte Ground_SweepTri(const vec3_t *a, const vec3_t *b, float r, const CCollideTri *tri, const CCollideMesh *mesh,
	vec3_t *res);

/* the walk state of CGround_Trace */
typedef struct {
	float invDX;			/* 0x00 1 / (from.x - to.x) */
	float invDZ;			/* 0x04 */
	int x;				/* 0x08 the cell */
	int z;				/* 0x0c */
	vec3_t from;			/* 0x10 */
	vec3_t to;			/* 0x1c */
} traceCell_t;

#endif
