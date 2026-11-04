/*
 * CMeshD3D of the engine3d (original source Engine/MallaD3D.cpp, engine3d/engine/MeshD3D.h): a mesh for the D3D
 * device, a list of meshes of one material each with a vertex list (CVertList), triangles of 16 bit indices and,
 * once restored, a vertex and an index buffer. A mesh is made from a CTriMesh (CMeshD3D_Build): its triangles are
 * grouped by material into at most MAX_D3D_OBJ temporary objects (MeshD3DObj) of up to g_MaxTriGroup triangles,
 * their equal vertices welded, then each object becomes one mesh of the list.
 */
#ifndef SCRAP_MESHD3D_H
#define SCRAP_MESHD3D_H

#include "scrap/game.h"
#include "scrap/vertlist.h"
#include "scrap/light.h"
#include "scrap/material.h"
#include "scrap/table.h"

/* Build flags */
#define MESHD3D_ONESIDED 0x01		/* no back faces for two sided materials */
#define MESHD3D_VERTORIG 0x02		/* keep the CTriMesh index of each vertex */
#define MESHD3D_WEIGHT 0x04		/* weight: vertices with a weight (VERT_TEX1_B1) */
#define MESHD3D_WEIGHT2 0x08		/* weight2: the same */
#define MESHD3D_POS 0x10		/* keep the positions and make the segments (CMeshD3D_CalcSegments) */
#define MESHD3D_TRIFLAGS 0x20		/* keep the triangle flags */

typedef struct CTriMesh CTriMesh;

typedef struct {
	float u, v;
} uv_t;

/* a corner of a triangle as the D3D device draws it */
typedef struct CMeshVert {
	vec3_t pos;
	vec3_t n;
	uv_t uv;
} CMeshVert;

/* the 3 vertex indices of a triangle. A member points to it as tri16_t *, not as uint16_t (*)[3]: Ghidra's C parser
 * drops the pointer of a pointer to an array (docs/ghidra.md). */
typedef uint16_t tri16_t[3];

typedef struct CMeshD3D {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;
			uint32_t class;			/* CLASS_OBJ3D_MESHD3D */
		};
	};
	struct CMeshD3D *next;		/* the next mesh of the list, owned */
	uint32_t nTri;
	tri16_t *pTri;			/* new[] */
	CVertList vert;
	uint16_t *vertOrig;		/* the CTriMesh vertex of each vertex (MESHD3D_VERTORIG) */
	int triBase;			/* the first triangle of this mesh in the list (CalcSegmentos) */
	uint32_t nSeg;			/* the triangles of the whole list (pSeg) */
	uint32_t numSegs;		/* of segs, the closing one too */
	uint32_t nPos;
	void *pSeg;			/* TriMeshPlane of each triangle of the list, the shadow volume (pTriSeg) */
	void *segs;			/* MeshSeg (8 bytes): the edges of the shadow volume, an end marker last */
	vec3_t *pPos;			/* the CTriMesh positions (MESHD3D_POS), of the frame with a skin (pVertOrig) */
	uint32_t *triFlags;		/* NULL for none */
	CMaterial *mat;
	byte weight;			/* MESHD3D_WEIGHT */
	byte weight2;			/* MESHD3D_WEIGHT2 */
	byte pad_52[2];
	box_t box;			/* with the rest of the list */
	box_t ext;			/* of the vertices */
	vec3_t center;			/* of ext, through the node matrix with a skin */
	uint32_t nSkin;
	byte *pSkin;			/* 16 bytes a vertex */
	byte skinRef;			/* pSkin belongs to the first mesh of the list (or of the node) */
	byte pad_99[3];
	int BaseIndex;
	int StartIndex;			/* the first triangle in pVertIB */
	int StripList;			/* the vertices of a strip without indices, 0 */
	int vbFrmt;			/* the vertex format of pVertVB */
	void *pVertVB;			/* IDirect3DVertexBuffer8 */
	void *pVertIB;			/* IDirect3DIndexBuffer8 */
	CNode3D *node;			/* referenced */
	void *skinNode;			/* the render node of the last skinning (RenderEVA_DrawSkin) */
	int skinFrame;			/* its frame */
	void *pc0;			/* COM objects released with the buffers */
	void *pc4;
} CMeshD3D;

/* the plane of a triangle in pSeg: n.p + d = 0 */
typedef struct TriMeshPlane {
	float d;
	vec3_t n;
} TriMeshPlane;

/* the context of Read and Write: the materials of the model (a CTable of CMaterial *) and the node */
typedef struct MeshD3D_FileCtx {
	CTable *mats;
	CNode3D *node;
} MeshD3D_FileCtx;

/* the corners of the temporary objects: a corner of the CTriMesh with its vertex and bone */
typedef struct MeshD3DVert {
	CMeshVert v;
	uint16_t index;			/* vertex of the CTriMesh */
	byte bone;			/* byte 1 of its skin */
	byte pad;			/* 0 */
} MeshD3DVert;

#pragma pack(push, 1)
typedef struct MeshD3DTri {
	uint32_t flags;
	uint16_t v[3];
} MeshD3DTri;
#pragma pack(pop)

/* a material group of the CTriMesh, the vertices and triangles still separate (Mem_Alloc) */
typedef struct MeshD3DObj {
	CMaterial *mat;
	int nVert;
	int nTri;
	MeshD3DVert *pVert;
	MeshD3DTri *pTri;
} MeshD3DObj;

/* a triangle of the CTriMesh while the mesh is made */
typedef struct MeshD3DTriTmp {
	MeshD3DVert v[3];
	CMaterial *mat;
	int group;
	uint32_t flags;
} MeshD3DTriTmp;

/* a material group in the cDynList of CMeshD3D_Build */
typedef struct MeshD3DGroup {
	CMaterial *mat;
	int nTri;
	int idx;			/* the index before sorting, then the MeshD3DObj */
	int used;			/* triangles placed */
} MeshD3DGroup;

#define MAX_D3D_OBJ 0x200

CMeshD3D *CMeshD3D_ctor(CMeshD3D *self);
/* made from mesh (MALLAD3D_* flags), the object of node when there is one */
CMeshD3D *CMeshD3D_ctorMesh(CMeshD3D *self, CTriMesh *mesh, uint32_t flags, CNode3D *node);

/* the unit normal of triangle i (unnormalized when its length is 0) */
vec3_t *CMeshD3D_TriNormal(CMeshD3D *self, vec3_t *out, uint32_t i);
void CMeshD3D_GetBox(CMeshD3D *self, box_t *out, byte recalc);

/* the drawing of a mesh list (HRESULT) */
int MeshD3D_Render(CMeshD3D *self, void *ctx);
/* the plane through three points; 0 for a degenerate triangle (calc_plano) */
byte Plane_Calc(plane_t *plane, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3);
/* Illum_VertList (the light of a CLight on the vertices of a list, Engine/RenderCalcIlum.cpp): light.h */
/* Engine/CalcSegmentos.cpp: the segments of the list from pPos */
void CMeshD3D_CalcSegments(CMeshD3D *self, byte all);

#endif
