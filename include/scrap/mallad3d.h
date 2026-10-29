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
#include "scrap/listavert.h"
#include "scrap/luz.h"
#include "scrap/material.h"
#include "scrap/tabla.h"

#define CMALLAD3D_VFTABLE GAME_VFTABLE(CMeshD3D_vftable, 0x3e3584)

/* Build flags */
#define MESHD3D_ONESIDED 0x01		/* no back faces for two sided materials */
#define MESHD3D_VERTORIG 0x02		/* keep the CTriMesh index of each vertex */
#define MESHD3D_WEIGHT 0x04		/* b50: vertices with a weight (VERT_TEX1_B1) */
#define MESHD3D_WEIGHT2 0x08		/* b51: the same */
#define MESHD3D_POS 0x10		/* keep the positions and make the segments (CMeshD3D_CalcSegments) */
#define MESHD3D_TRIFLAGS 0x20		/* keep the triangle flags */

typedef struct CTriMesh CTriMesh;

typedef struct {
	float u, v;
} uv_t;
SD_SIZE(uv_t, 8);

/* a corner of a triangle as the D3D device draws it */
typedef struct CMeshVert {
	vec3_t pos;			/* 0x00 */
	vec3_t n;			/* 0x0c */
	uv_t uv;			/* 0x18 */
} CMeshVert;
SD_SIZE(CMeshVert, 0x20);

/* IObject3D, the slots of vftable */
enum {
	MESHD3D_VF_DELETE = 0,		/* (byte flags) */
	MESHD3D_VF_SETRENDER = 1,	/* (void *renderNode) */
	MESHD3D_VF_DRAW = 2,		/* (void *dev, void *ctx) */
	MESHD3D_VF_RESTORE = 3,	/* (void *dev): the buffers made */
	MESHD3D_VF_INVALIDATE = 4,	/* (void *dev): the buffers released */
	MESHD3D_VF_WRITE = 5,		/* (CFileIO *f, MeshD3D_FileCtx *ctx) */
	MESHD3D_VF_READ = 6		/* (CFileIO *f, MeshD3D_FileCtx *ctx) */
};

typedef struct CMeshD3D {
	void **vftable;			/* 0x00 MALLAD3D_VF_* */
	uint32_t class;			/* 0x04 CLASS_OBJ3D_MESHD3D */
	struct CMeshD3D *next;		/* 0x08 the next mesh of the list, owned */
	uint32_t nTri;			/* 0x0c */
	uint16_t (*pTri)[3];		/* 0x10 new[] */
	CVertList vert;		/* 0x14 */
	uint16_t *vertOrig;		/* 0x28 the CTriMesh vertex of each vertex (MESHD3D_VERTORIG) */
	int triBase;			/* 0x2c the first triangle of this mesh in the list (CalcSegmentos) */
	uint32_t nSeg;			/* 0x30 the triangles of the whole list (pSeg) */
	uint32_t n34;			/* 0x34 */
	uint32_t nPos;			/* 0x38 */
	void *pSeg;			/* 0x3c TriMeshPlane of each triangle of the list, the shadow volume (pTriSeg) */
	void *p40;			/* 0x40 8 bytes each */
	vec3_t *pPos;			/* 0x44 the CTriMesh positions (MESHD3D_POS), of the frame with a skin (pVertOrig) */
	uint32_t *triFlags;		/* 0x48 NULL for none */
	CMaterial *mat;			/* 0x4c */
	byte b50;			/* 0x50 MESHD3D_WEIGHT */
	byte b51;			/* 0x51 MESHD3D_WEIGHT2 */
	byte pad_52[2];
	box_t box;			/* 0x54 with the rest of the list */
	box_t ext;			/* 0x6c of the vertices */
	vec3_t center;			/* 0x84 of ext, through the node matrix with a skin */
	uint32_t nSkin;			/* 0x90 */
	byte *pSkin;			/* 0x94 16 bytes a vertex */
	byte skinRef;			/* 0x98 pSkin belongs to the first mesh of the list (or of the node) */
	byte pad_99[3];
	int BaseIndex;			/* 0x9c */
	int StartIndex;			/* 0xa0 the first triangle in pVertIB */
	int StripList;			/* 0xa4 the vertices of a strip without indices, 0 */
	int vbFrmt;			/* 0xa8 the vertex format of pVertVB */
	void *pVertVB;			/* 0xac IDirect3DVertexBuffer8 */
	void *pVertIB;			/* 0xb0 IDirect3DIndexBuffer8 */
	CNode3D *node;			/* 0xb4 referenced */
	void *ib8;			/* 0xb8 the render node of the last skinning (RenderEVA_DrawSkin) */
	int ibc;			/* 0xbc its frame */
	void *pc0;			/* 0xc0 COM objects released with the buffers */
	void *pc4;			/* 0xc4 */
} CMeshD3D;
SD_OFFSET(CMeshD3D, vert, 0x14);
SD_OFFSET(CMeshD3D, triFlags, 0x48);
SD_OFFSET(CMeshD3D, mat, 0x4c);
SD_OFFSET(CMeshD3D, box, 0x54);
SD_OFFSET(CMeshD3D, center, 0x84);
SD_OFFSET(CMeshD3D, skinRef, 0x98);
SD_OFFSET(CMeshD3D, node, 0xb4);
SD_SIZE(CMeshD3D, 0xc8);

/* the plane of a triangle in pSeg: n.p + d = 0 */
typedef struct TriMeshPlane {
	float d;			/* 0x0 */
	vec3_t n;			/* 0x4 */
} TriMeshPlane;
SD_SIZE(TriMeshPlane, 0x10);

/* the context of Read and Write: the materials of the model (a CTable of CMaterial *) and the node */
typedef struct MeshD3D_FileCtx {
	CTable *mats;			/* 0x0 */
	CNode3D *node;			/* 0x4 */
} MeshD3D_FileCtx;

/* the corners of the temporary objects: a corner of the CTriMesh with its vertex and bone */
typedef struct MeshD3DVert {
	CMeshVert v;			/* 0x00 */
	uint16_t index;			/* 0x20 vertex of the CTriMesh */
	byte bone;			/* 0x22 byte 1 of its skin */
	byte pad;			/* 0x23 0 */
} MeshD3DVert;
SD_SIZE(MeshD3DVert, 0x24);

typedef struct __attribute__((packed)) MeshD3DTri {
	uint32_t flags;			/* 0x0 */
	uint16_t v[3];			/* 0x4 */
} MeshD3DTri;
SD_SIZE(MeshD3DTri, 10);

/* a material group of the CTriMesh, the vertices and triangles still separate (Mem_Alloc) */
typedef struct MeshD3DObj {
	CMaterial *mat;			/* 0x00 */
	int nVert;			/* 0x04 */
	int nTri;			/* 0x08 */
	MeshD3DVert *pVert;		/* 0x0c */
	MeshD3DTri *pTri;		/* 0x10 */
} MeshD3DObj;
SD_SIZE(MeshD3DObj, 0x14);

/* a triangle of the CTriMesh while the mesh is made */
typedef struct MeshD3DTriTmp {
	MeshD3DVert v[3];		/* 0x00 */
	CMaterial *mat;			/* 0x6c */
	int group;			/* 0x70 */
	uint32_t flags;			/* 0x74 */
} MeshD3DTriTmp;
SD_SIZE(MeshD3DTriTmp, 0x78);

/* a material group in the cDynList of CMeshD3D_Build */
typedef struct MeshD3DGroup {
	CMaterial *mat;			/* 0x0 */
	int nTri;			/* 0x4 */
	int idx;			/* 0x8 the index before sorting, then the MeshD3DObj */
	int used;			/* 0xc triangles placed */
} MeshD3DGroup;
SD_SIZE(MeshD3DGroup, 0x10);

#define MAX_D3D_OBJ 0x200
#define g_D3DObj (GAME_ARRAY(MeshD3DObj, g_D3DObj, 0x4dc128))
#define g_NumD3DObj GAME_GLOBAL(int, g_NumD3DObj, 0x4de928)
/* the vertices MeshD3D_CompareVertIdx sorts */
#define g_WeldVert GAME_GLOBAL(MeshD3DVert *, g_WeldVert, 0x4de92c)
/* the triangles of a group, 0x800 */
#define g_MaxTriGroup GAME_GLOBAL(int, g_MaxTriGroup, 0x432a70)

/* the deleting destructor */
CMeshD3D *__thiscall CMeshD3D_Delete(CMeshD3D *self, byte flags);
CMeshD3D *__fastcall CMeshD3D_ctor(CMeshD3D *self);
/* made from mesh (MALLAD3D_* flags), the object of node when there is one */
CMeshD3D *__thiscall CMeshD3D_ctorMesh(CMeshD3D *self, CTriMesh *mesh, uint32_t flags, CNode3D *node);
void __fastcall CMeshD3D_dtor(CMeshD3D *self);
/* the members after the vftable cleared, except vert, box and ext */
void __fastcall CMeshD3D_Init(CMeshD3D *self);
void __thiscall CMeshD3D_Build(CMeshD3D *self, CTriMesh *mesh, uint32_t flags);

/* the slots of the vftable */
void __thiscall CMeshD3D_SetRender(CMeshD3D *self, void *renderNode);
int __thiscall CMeshD3D_Draw(CMeshD3D *self, void *dev, void *ctx);
int __thiscall CMeshD3D_Restore(CMeshD3D *self, void *dev);
void __thiscall CMeshD3D_Invalidate(CMeshD3D *self, void *dev);
byte __thiscall CMeshD3D_Write(CMeshD3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);
byte __thiscall CMeshD3D_Read(CMeshD3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);

/* the buffers of this mesh, 0 or E_FAIL without vertices or triangles */
int __fastcall CMeshD3D_CreateVB(CMeshD3D *self);
/* the node of all meshes of the list, referenced */
void __thiscall CMeshD3D_SetNode(CMeshD3D *self, CNode3D *node);
void __thiscall CMeshD3D_AllocTri(CMeshD3D *self, int n);
void __thiscall CMeshD3D_SetTri(CMeshD3D *self, uint32_t i, uint16_t a, uint16_t b, uint16_t c);
/* the unit normal of triangle i (unnormalized when its length is 0) */
vec3_t *__thiscall CMeshD3D_TriNormal(CMeshD3D *self, vec3_t *out, uint32_t i);
/* ext and center */
void __fastcall CMeshD3D_CalcCenter(CMeshD3D *self);
/* ext, center and box (of the list), merged into out (copied if first) */
void __thiscall CMeshD3D_CalcBox(CMeshD3D *self, box_t *out, byte first);
void __thiscall CMeshD3D_GetBox(CMeshD3D *self, box_t *out, byte recalc);
/* CHECKs of vertices and triangles, of the list and of this mesh (0) */
void __fastcall CMeshD3D_CheckList(CMeshD3D *self);
int __fastcall CMeshD3D_Check(CMeshD3D *self);

/* the temporary objects */
int MeshD3D_NewObj(void);
void MeshD3DObj_AllocVert(MeshD3DObj *obj, int n);
void MeshD3DObj_AllocTri(MeshD3DObj *obj, int n);
void MeshD3DObj_SetVert(MeshD3DObj *obj, int i, const MeshD3DVert *v);
void MeshD3DObj_SetTri(MeshD3DObj *obj, int i, const MeshD3DTri *t);
void MeshD3DObj_SetMat(MeshD3DObj *obj, CMaterial *mat);
void MeshD3DObj_Free(MeshD3DObj *obj);
/* equal vertices made one; the vertices removed */
int MeshD3DObj_Weld(MeshD3DObj *obj);
/* triangle i of m, flipped (corners 1 and 2 swapped, the normals turned) unless keep */
void MeshD3D_FillTri(MeshD3DTriTmp *t, CTriMesh *m, uint32_t i, byte keep);
/* whether t can go into g: the same material and room */
int MeshD3D_GroupFits(const MeshD3DGroup *g, const MeshD3DTriTmp *t);
/* the plane of the corners and their center */
void MeshD3D_TriPlane(const MeshD3DTriTmp *t, vec3_t *center, plane_t *plane);

/* qsort orders: vertices by bone, then bytes; vertex indices into g_WeldVert; groups by the name of map 0, the
 * material and the index; triangles by the side of their plane, then the distance of their center */
int MeshD3D_CompareVert(const void *a, const void *b);
int MeshD3D_CompareVertIdx(const void *a, const void *b);
int MeshD3D_CompareGroup(const void *a, const void *b);
int MeshD3D_CompareTri(const void *a, const void *b);

/* the drawing of a mesh list (HRESULT) */
int MeshD3D_Render(CMeshD3D *self, void *ctx);
/* the plane through three points; 0 for a degenerate triangle (calc_plano) */
byte Plane_Calc(plane_t *plane, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3);
/* Illum_VertList (the light of a CLight on the vertices of a list, Engine/RenderCalcIlum.cpp): light.h */
/* Engine/CalcSegmentos.cpp: the segments of the list from pPos */
void __thiscall CMeshD3D_CalcSegments(CMeshD3D *self, byte all);

#endif
