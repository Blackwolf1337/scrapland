/*
 * CSuelo of the engine3d (original source Engine/suelo.cpp): the floor of a scene. It keeps the scene meshes split
 * into a grid of floor nodes (nodos, CNodo3D) and the collision data (alturas): the collision meshes of the sectors
 * (CCollideMesh, triangles with their planes and a tested mark, engine3d/engine/suelo.h) and a grid of quads
 * (CCollideQuad, the triangles of one mesh that touch a cell, chained per cell). The collision data is read from or
 * saved to the .amc file next to the scene. The traces test a segment or a moving sphere against the triangles of
 * the cells it passes; every query has a number (gCollisionNComp) so that a triangle shared by cells is tested once.
 */
#ifndef SCRAP_SUELO_H
#define SCRAP_SUELO_H

#include "scrap/archio.h"
#include "scrap/mathlib.h"
#include "scrap/nodo3d.h"
#include "scrap/tabla.h"

#define MAX_COLLIDE_SECTORS 128
#define CNDX_MAX 0xffff
#define AMC_VER 100

/* CCollideTri.flags */
#define TRI_ALT_NO_COLISIONES 0x1	/* no collisions (also a whole mesh in CCollideMesh.flags) */
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
	box_t Extens;			/* 0x0c */
	int nVert;			/* 0x24 CTabla of vec3_t */
	vec3_t *pVert;			/* 0x28 */
	int nTri;			/* 0x2c CTabla of CCollideTri */
	CCollideTri *pTri;		/* 0x30 */
} CCollideMesh;
SD_SIZE(CCollideMesh, 0x34);
/* the tables freed, CHECK(!Refs) (an inline of suelo.h, rva 0x20eb70) */
void __fastcall CCollideMesh_dtor(CCollideMesh *self);

typedef struct CCollideQuad {
	float gapTop;			/* 0x00 the largest vertical gap without triangles */
	float gapBottom;		/* 0x04 */
	CCollideMesh *Mesh;		/* 0x08 */
	int nTri;			/* 0x0c CTabla of the triangle indices */
	uint16_t *pTri;			/* 0x10 */
	float maxY;			/* 0x14 */
	float minY;			/* 0x18 */
	uint32_t dwParam;		/* 0x1c the triangles counted while the quads are built */
	struct CCollideQuad *next;	/* 0x20 the quad of the next mesh in the same cell */
} CCollideQuad;
SD_SIZE(CCollideQuad, 0x24);

/* CSuelo.alturas, its own this in the quad functions */
typedef struct CAlturas {
	uint32_t MeshID;		/* 0x0000 the last mesh added at run time */
	CCollideMesh _CollideMesh1[MAX_COLLIDE_SECTORS];	/* 0x0004 per sector */
	CCollideMesh _CollideMesh2[MAX_COLLIDE_SECTORS];	/* 0x1a04 per sector, the no collision triangles */
	CCollideMesh CollideMesh1;	/* 0x3404 */
	CCollideMesh CollideMesh2;	/* 0x3438 */
	box_t ext;			/* 0x346c of the grid */
	vec3_t dim;			/* 0x3484 */
	int nCuads;			/* 0x3490 CTabla of CCollideQuad, nx * nz */
	CCollideQuad *cuads;		/* 0x3494 */
	int nx;				/* 0x3498 */
	int nz;				/* 0x349c */
	float sizeX;			/* 0x34a0 of a cell */
	float sizeZ;			/* 0x34a4 */
	float invX;			/* 0x34a8 cells per unit */
	float invZ;			/* 0x34ac */
	CCollideQuad *outSector;	/* 0x34b0 */
} CAlturas;
SD_OFFSET(CAlturas, CollideMesh1, 0x3404);
SD_OFFSET(CAlturas, ext, 0x346c);
SD_OFFSET(CAlturas, nCuads, 0x3490);
SD_OFFSET(CAlturas, outSector, 0x34b0);
SD_SIZE(CAlturas, 0x34b4);

typedef struct CSuelo {
	void *vftable;			/* 0x00 */
	uint32_t clase;			/* 0x04 CLASE_OBJ3D_SUELO */
	int traceStats[3];		/* 0x08 CSuelo_Trace: triangles tested, cache hits, cache misses */
	int cache[3];			/* 0x14 a CSueloHit, the last hit of CSuelo_Trace; cleared whenever the meshes change */
	int nx;				/* 0x20 grid of the floor nodes */
	int nz;				/* 0x24 */
	int nNodos;			/* 0x28 CTabla of CNodo3D (0xfc bytes each) */
	CNodo3D *nodos;			/* 0x2c */
	box_t Extens;			/* 0x30 */
	vec3_t Dim;			/* 0x48 */
	box_t CollideExt;		/* 0x54 */
	int CollideTri;			/* 0x6c the triangles that collide */
	CAlturas alturas;		/* 0x70 */
} CSuelo;
SD_OFFSET(CSuelo, Extens, 0x30);
SD_OFFSET(CSuelo, alturas, 0x70);
SD_SIZE(CSuelo, 0x3524);

/* the quads of cell x z of the grid, NULL outside it (the accessor of tabla.h inlined) */
static inline CCollideQuad *Alturas_Cell(CAlturas *alt, uint32_t x, uint32_t z) {
	if (x >= (uint32_t)alt->nx || z >= (uint32_t)alt->nz) {
		return NULL;
	}
	uint32_t i = alt->nx * z + x;
	TABLA_CHECK(alt->cuads, i, alt->nCuads);
	return &alt->cuads[i];
}

/* rva 0x261530: the floor nodes freed (with the shadow maps they are not needed) */
void __fastcall CSuelo_FreeNodos(CSuelo *self);
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
void __fastcall CSuelo_DrawDebug(CSuelo *self);
/* rva 0x261580: in / |in| into out (the length through double, 1 / len rounded to float), a copy for length 0 */
vec3_t *Vec3_Normalize(vec3_t *out, const vec3_t *in);

#define g_CollisionEpsilon GAME_GLOBAL(float, g_CollisionEpsilon, 0x43241c)
#define g_CollisionBalance GAME_GLOBAL_IN(float, GVar_MapCollisionBalance, 16, 0x4af868)
#define g_QuadTriCacheID GAME_GLOBAL(int, g_QuadTriCacheID, 0x4af7d8)
#define g_CollisionNComp GAME_GLOBAL(uint32_t, g_CollisionNComp, 0x4af7dc)
#define g_CollisionTestsMax GAME_GLOBAL(uint32_t, g_CollisionTestsMax, 0x4af7e8)
#define g_CollisionTests GAME_GLOBAL(uint32_t, g_CollisionTests, 0x4af7ec)
#define g_CollisionTestsTotal GAME_GLOBAL(uint32_t, g_CollisionTestsTotal, 0x4af7f0)
#define g_CollisionQuadCount GAME_GLOBAL(int, g_CollisionQuadCount, 0x4af7f4)

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

/* engine3d/engine/tabla.h: count, sizeof(T) and the elements */
void Tabla_Write2(CArchIO *f, CTabla *t);
void Tabla_Write1C(CArchIO *f, CTabla *t);
void Tabla_WriteC(CArchIO *f, CTabla *t);
/* elements with a vector deleting destructor (vftable[0]) or plain new[]; the table empty after */
void __fastcall Tabla_FreeObjs(CTabla *t);

/* the vector deleting destructor of CNodo3D (0xfc bytes) as the floor nodes use it */
void *__thiscall CNodo3D_VecDelete(void *self, uint32_t flags);

CCollideMesh *__fastcall CCollideMesh_ctor(CCollideMesh *self);
/* the plane of triangle i (unit normal, unchanged when it has length 0) */
plane_t *__thiscall CCollideMesh_TriPlane(CCollideMesh *self, plane_t *out, uint32_t i);
/* a copy of triangle i with a new plane, ncomp 0 */
void __thiscall CCollideMesh_TriWithPlane(CCollideMesh *self, CCollideTri *out, int i);
/* triangle i and its corners, each output may be NULL */
void __thiscall CCollideMesh_GetTri(CCollideMesh *self, int i, CCollideTri *tri, vec3_t *v0, vec3_t *v1, vec3_t *v2);
/* the "CMSH" block; the mesh is entered in the load list by its Param */
void __thiscall CCollideMesh_Read(CCollideMesh *self, CArchIO *f);
void __thiscall CCollideMesh_Write(CCollideMesh *self, CArchIO *f);

CCollideQuad *__fastcall CCollideQuad_ctor(CCollideQuad *self);
/* the next quads freed, the mesh released */
void __fastcall CCollideQuad_dtor(CCollideQuad *self);
CCollideQuad *__thiscall CCollideQuad_Delete(CCollideQuad *self, uint32_t flags);
/* the highest and the lowest corner of a triangle of the mesh */
float __thiscall CCollideQuad_TriMaxY(CCollideQuad *self, const CCollideTri *tri);
float __thiscall CCollideQuad_TriMinY(CCollideQuad *self, const CCollideTri *tri);
/* the index (in pTri) of the colliding triangle with the lowest lowest corner below maxY, -1 for none */
int __fastcall CCollideQuad_LowestTri(CCollideQuad *self);
/* the "QUAD" block and the next ones */
void __thiscall CCollideQuad_Read(CCollideQuad *self, CArchIO *f);
void __thiscall CCollideQuad_Write(CCollideQuad *self, CArchIO *f);
/* the largest vertical gap between the triangles into gapTop and gapBottom */
void __fastcall CCollideQuad_CalcGap(CCollideQuad *self);

/* the quads of cell x z, NULL outside the grid (tabla.h) */
CCollideQuad *__thiscall CAlturas_GetCell(CAlturas *self, uint32_t x, uint32_t z);
CAlturas *__fastcall CAlturas_ctor(CAlturas *self);
void __fastcall CAlturas_dtor(CAlturas *self);
/* the Param of all meshes of the quads */
void __thiscall CAlturas_SetParam(CAlturas *self, byte param);
/* the quad functions below take the CAlturas without using it */
/* minY and maxY from the triangles (widened by the epsilon), then the gap */
void __thiscall CCollideQuad_CalcExtens(CAlturas *self, CCollideQuad *q);
/* all triangles of the mesh */
void __thiscall CCollideQuad_AddAllTris(CAlturas *self, CCollideQuad *q);
/* the quad of mesh (with that MeshID) in the chain of cuad, a new one at the end */
CCollideQuad *__thiscall CCollideQuad_Find(CAlturas *self, CCollideQuad *cuad, uint32_t id, CCollideMesh *mesh);
/* the grid over box for about numTri triangles (and the balance) per cell */
void __thiscall CAlturas_QuadInit(CAlturas *self, int unused, const box_t *box, int numTri);
/* the triangles of mesh from initTri (0) into the quads of the cells they touch */
void __thiscall CAlturas_QuadAddMesh(CAlturas *self, CCollideMesh *mesh, int initTri);

CSuelo *__fastcall CSuelo_ctor(CSuelo *self);
void __fastcall CSuelo_dtor(CSuelo *self);
CSuelo *__thiscall CSuelo_Delete(CSuelo *self, byte flags);
void __thiscall CSuelo_Nop(CSuelo *self, void *arg);
void __thiscall CSuelo_NodosVf3(CSuelo *self, void *arg);
void __thiscall CSuelo_NodosVf4(CSuelo *self, void *arg);
/* the "SUEL" block: 1 */
byte __thiscall CSuelo_Read(CSuelo *self, CArchIO *f, void *ctx);
byte __thiscall CSuelo_Write(CSuelo *self, CArchIO *f, void *ctx);
/* the .amc file of the scene name: 0 loaded, 1 older than the scene, -1 failed */
int __thiscall CSuelo_LoadCollision(CSuelo *self, const char *name);
/* 0 written, -1 failed */
int __thiscall CSuelo_SaveCollision(CSuelo *self, const char *name);
/* a new query number: the tested marks of all triangles cleared when it wraps */
void __fastcall CSuelo_NewQuery(CSuelo *self);
/* the sphere of radius r at b against the floor: the contact that is most a floor (normal nearest to the
 * vertical among the nearest ones); out the position pushed out along it (a for a wall), hit the contact, n the
 * direction from it to b. The y range of the quads tested is that of a to b */
byte __thiscall CSuelo_CollideSphere(CSuelo *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n);
/* the same with the nearest contact; without outputs the first contact ends it */
byte __thiscall CSuelo_SphereContact(CSuelo *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n);
/* a sphere of radius r moving from a to b: the contact nearest to a (out and hit the same point); without outputs the first contact ends it */
byte __thiscall CSuelo_SweepSphere(CSuelo *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n);
struct CMallaTri;

/* Engine/gety.cpp (Gety.c): the highest triangle point below maxY at x, z, -FLT_MAX for none; all: also the
 * triangles and meshes without collisions */
float __thiscall CSuelo_GetY(CSuelo *self, float x, float z, float maxY, byte all);
/* the same between minY and maxY, of the triangles whose normal has a y of 0.6 or more */
float __thiscall CSuelo_GetYRange(CSuelo *self, float x, float z, float maxY, float minY, byte all);
/* the highest point below maxY of all triangles of the cell into out (may be NULL); 0 for none */
byte __thiscall CSuelo_GetYBelow(CSuelo *self, float x, float z, float maxY, float *out);

/* the triangles of tm added to mesh (a mesh added at run time: outID and outIni get its MeshID and first triangle),
 * then into the quads */
void __thiscall CSuelo_AddMesh(CSuelo *self, CCollideMesh *mesh, struct CMallaTri *tm, uint32_t *outID, int *outIni);
/* the quads of mesh taken out of the cells (not the first quad of a cell) */
void __thiscall CSuelo_RemoveMesh(CSuelo *self, CCollideMesh *mesh, int a2, int a3);
/* collisions of a mesh added at run time on or off */
void __thiscall CSuelo_SetMeshCollide(CSuelo *self, CCollideMesh *mesh, byte on, int MeshID, int IniTri);
void __fastcall CSuelo_CalcGaps(CSuelo *self);
/* the triangles of the floor nodes in sector - 1 (all for -1) into mesh: mode 1 the colliding ones, mode 2 the
 * others (the mesh then gets TRI_ALT_NO_COLISIONES) */
void __thiscall CSuelo_BuildSectorMesh(CSuelo *self, CCollideMesh *mesh, int mode, uint32_t sector);
/* the grid and the meshes of all sectors from the floor nodes; empty cells get CollideMesh1 */
void __fastcall CSuelo_CreateQuadCollision(CSuelo *self);

/* the floor of a scene (CEscena): from its .amc file when the scene was loaded with one, else the triangles of its
 * meshes into a grid of floor nodes (one CMallaTri each) and the collision data from the .amc file or built and
 * saved */
void __thiscall CSuelo_Build(CSuelo *self, CEscena *esc);
/* the floor nodes to vertex buffer meshes */
void __thiscall CSuelo_NodosToD3D(CSuelo *self, int fvf);
/* the vertex buffer meshes of the floor nodes */
void __thiscall CSuelo_Draw(CSuelo *self, void *dev, int unused);
/* a debug box of half size r around c */
void Suelo_DrawBox(const vec3_t *c, float r, uint32_t color);
/* the collision debug drawing of the floor of gMundo */
void Suelo_DrawDebug(void);

/* (p - a) x (b - a): the side of p to the edge a b, in the operation order of suelo.cpp and checkline3d.cpp */
static inline vec3_t Suelo_EdgeCross(const vec3_t *p, const vec3_t *a, const vec3_t *b) {
	float dx = p->x - a->x, dy = p->y - a->y, dz = p->z - a->z;
	float ex = b->x - a->x, ey = b->y - a->y, ez = b->z - a->z;
	return (vec3_t){ dy * ez - dz * ey, dz * ex - dx * ez, dx * ey - dy * ex };
}

/* point p in the triangle a b c (on the side of its normal), also on its edges */
byte Suelo_PointInTri(const vec3_t *p, const vec3_t *a, const vec3_t *b, const vec3_t *c);
/* distance of p to the segment a b, the nearest point into out (may be NULL) */
float Suelo_DistPointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
float Suelo_Dist2PointSeg(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
/* the parameter t at which the segment a b comes within r of the plane; 0 if it stays farther away on one side */
byte Suelo_SegPlane(const vec3_t *a, const vec3_t *b, float r, const plane_t *pl, float *t);
/* the first t in [0, 1] at which the segment p q comes within r of the edge a b (or of its end a); 0 for none */
byte Suelo_SegEdge(const vec3_t *a, const vec3_t *b, const vec3_t *p, const vec3_t *q, float r, float *t);
/* the point of the triangle nearest to the sphere center c, if it is within r */
byte Suelo_SphereTri(const vec3_t *c, float r, const CCollideTri *tri, const CCollideMesh *mesh, vec3_t *res);
/* the triangle a segment hit (CSuelo_TraceSeg): quad->pTri[tri] in the mesh of the quad */
typedef struct CSueloHit {
	int i0;
	int tri;
	CCollideQuad *quad;
} CSueloHit;

/* the query of checkbbox.cpp ("colis"): the box min/max (usar_minmax 1) or min2/max2 (2), moved by pos */
typedef struct CColisBBox {
	byte pad_0[0x18];
	vec3_t pos;			/* 0x18 */
	vec3_t min;			/* 0x24 */
	vec3_t max;			/* 0x30 */
	vec3_t min2;			/* 0x3c */
	vec3_t max2;			/* 0x48 */
	int usar_minmax;		/* 0x54 */
	byte pad_58[0x24];
	int calc_respuesta;		/* 0x7c must be 0 */
} CColisBBox;
SD_OFFSET(CColisBBox, usar_minmax, 0x54);
SD_SIZE(CColisBBox, 0x80);

/* the queries against the triangles of the floor (suelo.cpp and checkbbox.cpp, src/imports.inc); the last
 * arguments are 0 in all callers */
/* rva 0x26ed30: 1 if the segment from a to b hits a triangle */
byte __thiscall CSuelo_TestSeg(CSuelo *self, const vec3_t *a, const vec3_t *b, int flags);
/* rva 0x26df80 and 0x26f120: also the first hit point, the hit triangle */
byte __thiscall CSuelo_TraceSegPos(CSuelo *self, const vec3_t *a, const vec3_t *b, vec3_t *hit, int flags);
byte __thiscall CSuelo_TraceSeg(CSuelo *self, const vec3_t *a, const vec3_t *b, vec3_t *hit, CSueloHit *tri,
	int flags);
/* rva 0x26fb10: 1 if a triangle is in the box */
byte __thiscall CSuelo_CheckBBox(CSuelo *self, CColisBBox *colis);

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
byte __thiscall CSuelo_CollideOBB(CSuelo *self, const matrix_t *m, const vec3_t *half, vec3_t *outPos, vec3_t *outN);

/* checkline3d.cpp */
/* the cell walk of a segment from a to b through the grid: the cell x z it leaves through first (the next one into x
 * and z, the parameter of the exit into t); 0 in the cell of b (t 1) */
byte Alturas_CellStep(CAlturas *alt, int *x, int *z, const vec3_t *a, const vec3_t *b, float *t);
/* the same for CSuelo_Trace on its walk state (g_TraceCell) */
byte Alturas_TraceStep(CAlturas *alt);
/* p in the triangle a b c (on the side of its normal), also on its edges */
byte Suelo_PointInTriIn(const vec3_t *p, const vec3_t *a, const vec3_t *b, const vec3_t *c);

/* the position where a sphere of radius r moving from a to b first touches the triangle */
byte Suelo_SweepTri(const vec3_t *a, const vec3_t *b, float r, const CCollideTri *tri, const CCollideMesh *mesh,
	vec3_t *res);

/* the walk state of CSuelo_Trace */
typedef struct {
	float invDX;			/* 0x00 1 / (from.x - to.x) */
	float invDZ;			/* 0x04 */
	int x;				/* 0x08 the cell */
	int z;				/* 0x0c */
	vec3_t from;			/* 0x10 */
	vec3_t to;			/* 0x1c */
} traceCell_t;

#endif
