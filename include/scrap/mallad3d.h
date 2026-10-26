/*
 * CMallaD3D of the engine3d (original source Engine/MallaD3D.cpp, engine3d/engine/MallaD3D.h): a mesh for the D3D
 * device, a list of meshes of one material each with a vertex list (CListaVert), triangles of 16 bit indices and,
 * once restored, a vertex and an index buffer. A mesh is made from a CMallaTri (CMallaD3D_Build): its triangles are
 * grouped by material into at most MAX_D3D_OBJ temporary objects (MallaD3DObj) of up to g_MaxTriGrupo triangles,
 * their equal vertices welded, then each object becomes one mesh of the list.
 */
#ifndef SCRAP_MALLAD3D_H
#define SCRAP_MALLAD3D_H

#include "scrap/game.h"
#include "scrap/listavert.h"
#include "scrap/luz.h"
#include "scrap/material.h"
#include "scrap/tabla.h"

#define CLASE_OBJ3D_MALLAD3D 0xa1000002
#define CMALLAD3D_VFTABLE 0x3e3584

/* Build flags */
#define MALLAD3D_ONESIDED 0x01		/* no back faces for two sided materials */
#define MALLAD3D_VERTORIG 0x02		/* keep the CMallaTri index of each vertex */
#define MALLAD3D_WEIGHT 0x04		/* b50: vertices with a weight (VERT_TEX1_B1) */
#define MALLAD3D_WEIGHT2 0x08		/* b51: the same */
#define MALLAD3D_POS 0x10		/* keep the positions and make the segments (CMallaD3D_CalcSegmentos) */
#define MALLAD3D_TRIFLAGS 0x20		/* keep the triangle flags */

typedef struct CMallaTri CMallaTri;

typedef struct {
	float u, v;
} uv_t;
SD_SIZE(uv_t, 8);

/* a corner of a triangle as the D3D device draws it */
typedef struct CVertMalla {
	vec3_t pos;			/* 0x00 */
	vec3_t n;			/* 0x0c */
	uv_t uv;			/* 0x18 */
} CVertMalla;
SD_SIZE(CVertMalla, 0x20);

/* IObjeto3D, the slots of vftable */
enum {
	MALLAD3D_VF_DELETE = 0,		/* (byte flags) */
	MALLAD3D_VF_SETRENDER = 1,	/* (void *nodoR) */
	MALLAD3D_VF_DRAW = 2,		/* (void *dev, void *ctx) */
	MALLAD3D_VF_RESTORE = 3,	/* (void *dev): the buffers made */
	MALLAD3D_VF_INVALIDATE = 4,	/* (void *dev): the buffers released */
	MALLAD3D_VF_WRITE = 5,		/* (CArchIO *f, MallaD3D_FileCtx *ctx) */
	MALLAD3D_VF_READ = 6		/* (CArchIO *f, MallaD3D_FileCtx *ctx) */
};

typedef struct CMallaD3D {
	void **vftable;			/* 0x00 MALLAD3D_VF_* */
	uint32_t clase;			/* 0x04 CLASE_OBJ3D_MALLAD3D */
	struct CMallaD3D *next;		/* 0x08 the next mesh of the list, owned */
	uint32_t nTri;			/* 0x0c */
	uint16_t (*pTri)[3];		/* 0x10 new[] */
	CListaVert vert;		/* 0x14 */
	uint16_t *vertOrig;		/* 0x28 the CMallaTri vertex of each vertex (MALLAD3D_VERTORIG) */
	int triBase;			/* 0x2c the first triangle of this mesh in the list (CalcSegmentos) */
	uint32_t nSeg;			/* 0x30 */
	uint32_t n34;			/* 0x34 */
	uint32_t nPos;			/* 0x38 */
	void *pSeg;			/* 0x3c 16 bytes each (CalcSegmentos) */
	void *p40;			/* 0x40 8 bytes each */
	vec3_t *pPos;			/* 0x44 the CMallaTri positions (MALLAD3D_POS) */
	uint32_t *triFlags;		/* 0x48 NULL for none */
	CMaterial *mat;			/* 0x4c */
	byte b50;			/* 0x50 MALLAD3D_WEIGHT */
	byte b51;			/* 0x51 MALLAD3D_WEIGHT2 */
	byte pad_52[2];
	box_t box;			/* 0x54 with the rest of the list */
	box_t ext;			/* 0x6c of the vertices */
	vec3_t center;			/* 0x84 of ext, through the node matrix with a skin */
	uint32_t nSkin;			/* 0x90 */
	byte *pSkin;			/* 0x94 16 bytes a vertex */
	byte skinRef;			/* 0x98 pSkin belongs to the first mesh of the list (or of the node) */
	byte pad_99[3];
	int i9c;			/* 0x9c */
	int ia0;			/* 0xa0 */
	int ia4;			/* 0xa4 */
	int vbFrmt;			/* 0xa8 the vertex format of pVertVB */
	void *pVertVB;			/* 0xac IDirect3DVertexBuffer8 */
	void *pVertIB;			/* 0xb0 IDirect3DIndexBuffer8 */
	CNodo3D *nodo;			/* 0xb4 referenced */
	int ib8;			/* 0xb8 */
	int ibc;			/* 0xbc */
	void *pc0;			/* 0xc0 COM objects released with the buffers */
	void *pc4;			/* 0xc4 */
} CMallaD3D;
SD_OFFSET(CMallaD3D, vert, 0x14);
SD_OFFSET(CMallaD3D, triFlags, 0x48);
SD_OFFSET(CMallaD3D, mat, 0x4c);
SD_OFFSET(CMallaD3D, box, 0x54);
SD_OFFSET(CMallaD3D, center, 0x84);
SD_OFFSET(CMallaD3D, skinRef, 0x98);
SD_OFFSET(CMallaD3D, nodo, 0xb4);
SD_SIZE(CMallaD3D, 0xc8);

/* the context of Read and Write: the materials of the model (a CTabla of CMaterial *) and the node */
typedef struct MallaD3D_FileCtx {
	CTabla *mats;			/* 0x0 */
	CNodo3D *nodo;			/* 0x4 */
} MallaD3D_FileCtx;

/* the corners of the temporary objects: a corner of the CMallaTri with its vertex and bone */
typedef struct MallaD3DVert {
	CVertMalla v;			/* 0x00 */
	uint16_t index;			/* 0x20 vertex of the CMallaTri */
	byte bone;			/* 0x22 byte 1 of its skin */
	byte pad;			/* 0x23 0 */
} MallaD3DVert;
SD_SIZE(MallaD3DVert, 0x24);

typedef struct __attribute__((packed)) MallaD3DTri {
	uint32_t flags;			/* 0x0 */
	uint16_t v[3];			/* 0x4 */
} MallaD3DTri;
SD_SIZE(MallaD3DTri, 10);

/* a material group of the CMallaTri, the vertices and triangles still separate (Mem_Alloc) */
typedef struct MallaD3DObj {
	CMaterial *mat;			/* 0x00 */
	int nVert;			/* 0x04 */
	int nTri;			/* 0x08 */
	MallaD3DVert *pVert;		/* 0x0c */
	MallaD3DTri *pTri;		/* 0x10 */
} MallaD3DObj;
SD_SIZE(MallaD3DObj, 0x14);

/* a triangle of the CMallaTri while the mesh is made */
typedef struct MallaD3DTriTmp {
	MallaD3DVert v[3];		/* 0x00 */
	CMaterial *mat;			/* 0x6c */
	int grupo;			/* 0x70 */
	uint32_t flags;			/* 0x74 */
} MallaD3DTriTmp;
SD_SIZE(MallaD3DTriTmp, 0x78);

/* a material group in the cListaDin of CMallaD3D_Build */
typedef struct MallaD3DGrupo {
	CMaterial *mat;			/* 0x0 */
	int nTri;			/* 0x4 */
	int idx;			/* 0x8 the index before sorting, then the MallaD3DObj */
	int used;			/* 0xc triangles placed */
} MallaD3DGrupo;
SD_SIZE(MallaD3DGrupo, 0x10);

#define MAX_D3D_OBJ 0x200
#define g_D3DObj ((MallaD3DObj *)Game_Ptr(0x4dc128))
#define g_NumD3DObj GAME_DATA(int, 0x4de928)
/* the vertices MallaD3D_CompareVertIdx sorts */
#define g_WeldVert GAME_DATA(MallaD3DVert *, 0x4de92c)
/* the triangles of a group, 0x800 */
#define g_MaxTriGrupo GAME_DATA(int, 0x432a70)
/* materials with two texture sets get VERT_TEX1 instead of VERT_TEX2 (1) */
#define g_MallaD3DTex1 GAME_DATA(int, 0x431bdc)

/* the deleting destructor */
CMallaD3D *__thiscall CMallaD3D_Delete(CMallaD3D *self, byte flags);
CMallaD3D *__fastcall CMallaD3D_ctor(CMallaD3D *self);
/* made from malla (MALLAD3D_* flags), the object of nodo when there is one */
CMallaD3D *__thiscall CMallaD3D_ctorMalla(CMallaD3D *self, CMallaTri *malla, uint32_t flags, CNodo3D *nodo);
void __fastcall CMallaD3D_dtor(CMallaD3D *self);
/* the members after the vftable cleared, except vert, box and ext */
void __fastcall CMallaD3D_Init(CMallaD3D *self);
void __thiscall CMallaD3D_Build(CMallaD3D *self, CMallaTri *malla, uint32_t flags);

/* the slots of the vftable */
void __thiscall CMallaD3D_SetRender(CMallaD3D *self, void *nodoR);
int __thiscall CMallaD3D_Draw(CMallaD3D *self, void *dev, void *ctx);
int __thiscall CMallaD3D_Restore(CMallaD3D *self, void *dev);
void __thiscall CMallaD3D_Invalidate(CMallaD3D *self, void *dev);
byte __thiscall CMallaD3D_Write(CMallaD3D *self, CArchIO *f, MallaD3D_FileCtx *ctx);
byte __thiscall CMallaD3D_Read(CMallaD3D *self, CArchIO *f, MallaD3D_FileCtx *ctx);

/* the buffers of this mesh, 0 or E_FAIL without vertices or triangles */
int __fastcall CMallaD3D_CreateVB(CMallaD3D *self);
/* the node of all meshes of the list, referenced */
void __thiscall CMallaD3D_SetNodo(CMallaD3D *self, CNodo3D *nodo);
void __thiscall CMallaD3D_AllocTri(CMallaD3D *self, int n);
void __thiscall CMallaD3D_SetTri(CMallaD3D *self, uint32_t i, uint16_t a, uint16_t b, uint16_t c);
/* the unit normal of triangle i (unnormalized when its length is 0) */
vec3_t *__thiscall CMallaD3D_TriNormal(CMallaD3D *self, vec3_t *out, uint32_t i);
/* ext and center */
void __fastcall CMallaD3D_CalcCenter(CMallaD3D *self);
/* ext, center and box (of the list), merged into out (copied if first) */
void __thiscall CMallaD3D_CalcBox(CMallaD3D *self, box_t *out, byte first);
void __thiscall CMallaD3D_GetBox(CMallaD3D *self, box_t *out, byte recalc);
/* CHECKs of vertices and triangles, of the list and of this mesh (0) */
void __fastcall CMallaD3D_CheckList(CMallaD3D *self);
int __fastcall CMallaD3D_Check(CMallaD3D *self);

/* the temporary objects */
int MallaD3D_NewObj(void);
void MallaD3DObj_AllocVert(MallaD3DObj *obj, int n);
void MallaD3DObj_AllocTri(MallaD3DObj *obj, int n);
void MallaD3DObj_SetVert(MallaD3DObj *obj, int i, const MallaD3DVert *v);
void MallaD3DObj_SetTri(MallaD3DObj *obj, int i, const MallaD3DTri *t);
void MallaD3DObj_SetMat(MallaD3DObj *obj, CMaterial *mat);
void MallaD3DObj_Free(MallaD3DObj *obj);
/* equal vertices made one; the vertices removed */
int MallaD3DObj_Weld(MallaD3DObj *obj);
/* triangle i of m, flipped (corners 1 and 2 swapped, the normals turned) unless keep */
void MallaD3D_FillTri(MallaD3DTriTmp *t, CMallaTri *m, uint32_t i, byte keep);
/* whether t can go into g: the same material and room */
int MallaD3D_GrupoFits(const MallaD3DGrupo *g, const MallaD3DTriTmp *t);
/* the plane of the corners and their center */
void MallaD3D_TriPlane(const MallaD3DTriTmp *t, vec3_t *center, plane_t *plane);

/* qsort orders: vertices by bone, then bytes; vertex indices into g_WeldVert; groups by the name of map 0, the
 * material and the index; triangles by the side of their plane, then the distance of their center */
int MallaD3D_CompareVert(const void *a, const void *b);
int MallaD3D_CompareVertIdx(const void *a, const void *b);
int MallaD3D_CompareGrupo(const void *a, const void *b);
int MallaD3D_CompareTri(const void *a, const void *b);

/* Engine/render.cpp: the vertex buffer of a vertex list and the index buffer of triangles (HRESULT) */
int Render_CreateVB(void **vb, CListaVert *vert, int flags);
int Render_CreateIB(void **ib, const void *tris, uint32_t nTri, int flags);
/* the drawing of a mesh list (HRESULT) */
int MallaD3D_Render(CMallaD3D *self, void *ctx);
/* the plane through three points; 0 for a degenerate triangle (calc_plano) */
byte Plane_Calc(plane_t *plane, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3);
/* Engine/CalcSegmentos.cpp: the segments of the list from pPos */
void __thiscall CMallaD3D_CalcSegmentos(CMallaD3D *self, byte all);

#endif
