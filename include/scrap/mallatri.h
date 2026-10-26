/*
 * CMallaTri of the engine3d (original source Engine/MallaTri.cpp): a triangle mesh in system memory with separate
 * vertex, texture vertex and triangle arrays that grow by doubling, an optional skin array (16 bytes a vertex) and
 * the D3D mesh made from it. It is filled from the vertex buffer meshes (CMallaD3D, engine3d/engine/MallaD3D.h) or
 * from other CMallaTri.
 */
#ifndef SCRAP_MALLATRI_H
#define SCRAP_MALLATRI_H

#include "scrap/game.h"
#include "scrap/listavert.h"
#include "scrap/mallad3d.h"

/* flexible vertex format of CVertMalla: D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 */
#define MALLATRI_FVF 0x112

/* one triangle: indices into pVert and pTVert, the normal at each corner */
typedef struct CTriMalla {
	int v[3];			/* 0x00 */
	int tv[3];			/* 0x0c */
	vec3_t n[3];			/* 0x18 */
	void *mat;			/* 0x3c material (Engine/render.cpp) */
	uint32_t flags;			/* 0x40 */
} CTriMalla;
SD_SIZE(CTriMalla, 0x44);

typedef struct CMallaTri {
	void *vftable;			/* 0x00 */
	uint32_t clase;			/* 0x04 CLASE_OBJ3D_MALLATRI */
	int nVert;			/* 0x08 */
	int nTVert;			/* 0x0c */
	int nTri;			/* 0x10 */
	int maxVert;			/* 0x14 allocated */
	int maxTVert;			/* 0x18 */
	int maxTri;			/* 0x1c */
	vec3_t *pVert;			/* 0x20 */
	uv_t *pTVert;			/* 0x24 */
	CTriMalla *pTri;		/* 0x28 */
	byte *pSkin;			/* 0x2c 16 bytes a vertex */
	CMallaD3D *d3d;			/* 0x30 deleted with the mesh */
	box_t box;			/* 0x34 of the vertices, CMallaTri_CalcBox */
} CMallaTri;
SD_SIZE(CMallaTri, 0x4c);

/* ListaVert.h, instanced in MallaTri.cpp: the normal and the texture coordinates of vertex i; a format without them
 * is a failed CHECK and zeros */
vec3_t *__thiscall CListaVert_GetNormal(CListaVert *self, vec3_t *out, uint32_t i);
uv_t *__thiscall CListaVert_GetUV(CListaVert *self, uv_t *out, uint32_t i);


CTriMalla *__fastcall CTriMalla_ctor(CTriMalla *self);

CMallaTri *__fastcall CMallaTri_ctor(CMallaTri *self);
CMallaTri *__thiscall CMallaTri_ctorCopy(CMallaTri *self, const CMallaTri *orig);
/* the meshes of src, with all its list */
CMallaTri *__thiscall CMallaTri_ctorD3D(CMallaTri *self, CMallaD3D *src, byte all);
void __fastcall CMallaTri_dtor(CMallaTri *self);
CMallaTri *__thiscall CMallaTri_Delete(CMallaTri *self, byte flags);
/* the members after the vftable cleared, except the box */
void __fastcall CMallaTri_Init(CMallaTri *self);
/* the arrays and the D3D mesh freed, everything 0 */
void __fastcall CMallaTri_Clear(CMallaTri *self);
/* the Read and Write slots of IObjeto3D: not supported, 0 */
byte __thiscall CMallaTri_NoFile(CMallaTri *self, void *f, void *ctx);
/* through the D3D mesh when there is one and g_MallaD3DOn, else each triangle with DrawPrimitiveUP */
void __thiscall CMallaTri_Draw(CMallaTri *self, void *dev, int unused);

/* the counts set; a larger one than allocated doubles the array (realloc: else CHECK there is none yet) */
void __thiscall CMallaTri_SetNumVert(CMallaTri *self, int n, byte realloc);
/* 0 frees the texture vertices (the count stays) */
void __thiscall CMallaTri_SetNumTVert(CMallaTri *self, int n, byte realloc);
void __thiscall CMallaTri_SetNumTri(CMallaTri *self, int n, byte realloc);
/* one more: the index of the new one */
int __fastcall CMallaTri_AddVert(CMallaTri *self);
int __fastcall CMallaTri_AddTVert(CMallaTri *self);
int __fastcall CMallaTri_AddTri(CMallaTri *self);
void *__fastcall CMallaTri_AllocSkin(CMallaTri *self);
void *__thiscall CMallaTri_GetSkin(CMallaTri *self, uint32_t i);

void __thiscall CMallaTri_SetVert(CMallaTri *self, uint32_t i, const vec3_t *v);
vec3_t *__thiscall CMallaTri_GetVert(CMallaTri *self, vec3_t *out, uint32_t i);
void __thiscall CMallaTri_SetTVert(CMallaTri *self, uint32_t i, const uv_t *uv);
CTriMalla *__thiscall CMallaTri_GetTri(CMallaTri *self, uint32_t i);
/* the object as a CMallaTri, the objeto3d.h CHECK and NULL for another class */
static inline CMallaTri *Obj3D_MallaTri(IObjeto3D *o) {
	if (o->clase != CLASE_OBJ3D_MALLATRI) {
		CheckFailed("devNULL", "objeto3d.h", __TIMESTAMP__, __LINE__);
		return NULL;
	}
	return (CMallaTri *)o;
}
void __thiscall CMallaTri_SetTriVerts(CMallaTri *self, uint32_t i, int a, int b, int c);
void __thiscall CMallaTri_SetTriTVerts(CMallaTri *self, uint32_t i, int a, int b, int c);
void __thiscall CMallaTri_SetTriNormals(CMallaTri *self, uint32_t i, const vec3_t *a, const vec3_t *b, const vec3_t *c);
void __thiscall CMallaTri_SetTriMat(CMallaTri *self, uint32_t i, void *mat);
void __thiscall CMallaTri_SetTriFlags(CMallaTri *self, uint32_t i, uint32_t flags);
/* the last triangle moved to i */
void __thiscall CMallaTri_RemoveTri(CMallaTri *self, uint32_t i);
/* corner vert of triangle tri; uv 0 without texture vertices */
void __thiscall CMallaTri_GetTriVert(CMallaTri *self, CVertMalla *out, uint32_t tri, uint32_t vert);
/* a triangle of new vertices (and texture vertices if tvert) */
void __thiscall CMallaTri_AddTriVerts(CMallaTri *self, const CVertMalla *a, const CVertMalla *b, const CVertMalla *c,
	void *mat, byte tvert, uint32_t flags);
/* texture vertex 0 at (0, 0) for all triangles, when there are triangles but no texture vertices */
void __fastcall CMallaTri_CreateTVert(CMallaTri *self);

/* the vertices and triangles of src (and of its list if all) added */
void __thiscall CMallaTri_AddD3D(CMallaTri *self, CMallaD3D *src, byte all);
/* the triangles of orig with ((flags & mask) == 0) != want added, all its vertices and the skins */
void __thiscall CMallaTri_Add(CMallaTri *self, const CMallaTri *orig, uint32_t mask, byte want);

/* n made unit length through 1.0 / length in double; a zero length leaves it */
void Malla_Normalize(vec3_t *n);

/* box recomputed from the vertices, then merged into out (copied if first) */
void __thiscall CMallaTri_CalcBox(CMallaTri *self, box_t *out, byte first);
void __thiscall CMallaTri_GetBox(CMallaTri *self, box_t *out, byte recalc);
/* vertices through m, the normals turned and normalized */
void __thiscall CMallaTri_Transform(CMallaTri *self, const matrix_t *m);

#endif
