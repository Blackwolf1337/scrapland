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
/* rva 0x261580: in / |in| into out (the length through double, 1 / len rounded to float), a copy for length 0 */
vec3_t *Vec3_Normalize(vec3_t *out, const vec3_t *in);

extern float g_CollisionEpsilon;
extern int g_QuadTriCacheID;
extern uint32_t g_CollisionNComp;
extern uint32_t g_BspCacheStats[3];	/* the hits, the misses and the most tests */
#define g_CollisionTestsMax g_BspCacheStats[2]
extern uint32_t g_CollisionTests;

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

/* the vector deleting destructor of CNode3D (0xfc bytes) as the floor nodes use it */
void CNode3D_Free(ObjRef *base);

CCollideQuad *CCollideQuad_ctor(CCollideQuad *self);
/* the next quads freed, the mesh released */
void CCollideQuad_dtor(CCollideQuad *self);

/* the quads of cell x z, NULL outside the grid (table.h) */
CCollideQuad *CHeights_GetCell(CHeights *self, uint32_t x, uint32_t z);
/* the quad functions below take the CHeights without using it */

CGround *CGround_ctor(CGround *self);
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

/* the triangles of tm added to mesh (a mesh added at run time: outID and outIni get its MeshID and first triangle),
 * then into the quads */
void CGround_AddMesh(CGround *self, CCollideMesh *mesh, struct CTriMesh *tm, uint32_t *outID, int *outIni);
/* the quads of mesh taken out of the cells (not the first quad of a cell) */
void CGround_RemoveMesh(CGround *self, CCollideMesh *mesh, int a2, int a3);
/* collisions of a mesh added at run time on or off */
void CGround_SetMeshCollide(CGround *self, CCollideMesh *mesh, byte on, int MeshID, int IniTri);

/* the floor of a scene (CScene): from its .amc file when the scene was loaded with one, else the triangles of its
 * meshes into a grid of floor nodes (one CTriMesh each) and the collision data from the .amc file or built and
 * saved */
void CGround_Build(CGround *self, CScene *scn);
/* the floor nodes to vertex buffer meshes */
void CGround_NodesToD3D(CGround *self, int fvf);
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
float Ground_Dist2PointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
/* the query of checkbbox.cpp ("colis"): the box min/max (use_minmax 1) or min2/max2 (2), moved by pos */
typedef struct CCollisionBBox {
	vec3_t pos;			/* 0x18 */
	vec3_t min;			/* 0x24 */
	vec3_t max;			/* 0x30 */
	vec3_t min2;			/* 0x3c */
	vec3_t max2;			/* 0x48 */
	int use_minmax;		/* 0x54 */
	int calc_response;		/* 0x7c must be 0 */
} CCollisionBBox;

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

/* checkline3d.cpp */

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
