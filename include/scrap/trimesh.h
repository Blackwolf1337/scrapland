/*
 * CTriMesh of the engine3d (original source Engine/MallaTri.cpp): a triangle mesh in system memory with separate
 * vertex, texture vertex and triangle arrays that grow by doubling, an optional skin array (16 bytes a vertex) and
 * the D3D mesh made from it. It is filled from the vertex buffer meshes (CMeshD3D, engine3d/engine/MeshD3D.h) or
 * from other CTriMesh.
 */
#ifndef SCRAP_TRIMESH_H
#define SCRAP_TRIMESH_H

#include "scrap/game.h"
#include "scrap/vertlist.h"
#include "scrap/meshd3d.h"

/* flexible vertex format of CMeshVert: D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 */
#define TRIMESH_FVF 0x112

/* one triangle: indices into pVert and pTVert, the normal at each corner */
typedef struct CMeshTri {
	int v[3];			/* 0x00 */
	int tv[3];			/* 0x0c */
	vec3_t n[3];			/* 0x18 */
	void *mat;			/* 0x3c material (Engine/render.cpp) */
	uint32_t flags;			/* 0x40 */
} CMeshTri;
SD_SIZE(CMeshTri, 0x44);

typedef struct CTriMesh {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;	/* 0x00 */
			uint32_t class;			/* 0x04 CLASS_OBJ3D_TRIMESH */
		};
	};
	int nVert;			/* 0x08 */
	int nTVert;			/* 0x0c */
	int nTri;			/* 0x10 */
	int maxVert;			/* 0x14 allocated */
	int maxTVert;			/* 0x18 */
	int maxTri;			/* 0x1c */
	vec3_t *pVert;			/* 0x20 */
	uv_t *pTVert;			/* 0x24 */
	CMeshTri *pTri;		/* 0x28 */
	byte *pSkin;			/* 0x2c 16 bytes a vertex */
	CMeshD3D *d3d;			/* 0x30 deleted with the mesh */
	box_t box;			/* 0x34 of the vertices, CTriMesh_CalcBox */
} CTriMesh;
SD_SIZE(CTriMesh, 0x4c);

/* ListaVert.h, instanced in MallaTri.cpp: the normal and the texture coordinates of vertex i; a format without them
 * is a failed CHECK and zeros */
vec3_t *CVertList_GetNormal(CVertList *self, vec3_t *out, uint32_t i);
uv_t *CVertList_GetUV(CVertList *self, uv_t *out, uint32_t i);


CMeshTri *CMeshTri_ctor(CMeshTri *self);

CTriMesh *CTriMesh_ctor(CTriMesh *self);
CTriMesh *CTriMesh_ctorCopy(CTriMesh *self, const CTriMesh *orig);
/* the meshes of src, with all its list */
CTriMesh *CTriMesh_ctorD3D(CTriMesh *self, CMeshD3D *src, byte all);
void CTriMesh_dtor(CTriMesh *self);
IObject3D *CTriMesh_Delete(IObject3D *base, byte flags);
/* the members after the vftable cleared, except the box */
void CTriMesh_Init(CTriMesh *self);
/* the arrays and the D3D mesh freed, everything 0 */
void CTriMesh_Clear(CTriMesh *self);
/* the Read and Write slots of IObject3D: not supported, 0 */
byte CTriMesh_NoFile(CTriMesh *self, void *f, void *ctx);
/* through the D3D mesh when there is one and g_MeshD3DOn, else each triangle with DrawPrimitiveUP */
void CTriMesh_Draw(IObject3D *elem, void *dev, CRenderNode *unused);

/* the counts set; a larger one than allocated doubles the array (realloc: else CHECK there is none yet) */
void CTriMesh_SetNumVert(CTriMesh *self, int n, byte realloc);
/* 0 frees the texture vertices (the count stays) */
void CTriMesh_SetNumTVert(CTriMesh *self, int n, byte realloc);
void CTriMesh_SetNumTri(CTriMesh *self, int n, byte realloc);
/* one more: the index of the new one */
int CTriMesh_AddVert(CTriMesh *self);
int CTriMesh_AddTVert(CTriMesh *self);
int CTriMesh_AddTri(CTriMesh *self);
void *CTriMesh_AllocSkin(CTriMesh *self);
void *CTriMesh_GetSkin(CTriMesh *self, uint32_t i);

void CTriMesh_SetVert(CTriMesh *self, uint32_t i, const vec3_t *v);
vec3_t *CTriMesh_GetVert(CTriMesh *self, vec3_t *out, uint32_t i);
void CTriMesh_SetTVert(CTriMesh *self, uint32_t i, const uv_t *uv);
CMeshTri *CTriMesh_GetTri(CTriMesh *self, uint32_t i);
/* the object as a CTriMesh, the objeto3d.h CHECK and NULL for another class */
static inline CTriMesh *Obj3D_TriMesh(IObject3D *o) {
	if (o->class != CLASS_OBJ3D_TRIMESH) {
		CheckFailed("devNULL", "objeto3d.h", __TIMESTAMP__, __LINE__);
		return NULL;
	}
	return (CTriMesh *)o;
}
void CTriMesh_SetTriVerts(CTriMesh *self, uint32_t i, int a, int b, int c);
void CTriMesh_SetTriTVerts(CTriMesh *self, uint32_t i, int a, int b, int c);
void CTriMesh_SetTriNormals(CTriMesh *self, uint32_t i, const vec3_t *a, const vec3_t *b, const vec3_t *c);
void CTriMesh_SetTriMat(CTriMesh *self, uint32_t i, void *mat);
void CTriMesh_SetTriFlags(CTriMesh *self, uint32_t i, uint32_t flags);
/* the last triangle moved to i */
void CTriMesh_RemoveTri(CTriMesh *self, uint32_t i);
/* corner vert of triangle tri; uv 0 without texture vertices */
void CTriMesh_GetTriVert(CTriMesh *self, CMeshVert *out, uint32_t tri, uint32_t vert);
/* a triangle of new vertices (and texture vertices if tvert) */
void CTriMesh_AddTriVerts(CTriMesh *self, const CMeshVert *a, const CMeshVert *b, const CMeshVert *c,
	void *mat, byte tvert, uint32_t flags);
/* texture vertex 0 at (0, 0) for all triangles, when there are triangles but no texture vertices */
void CTriMesh_CreateTVert(CTriMesh *self);

/* the vertices and triangles of src (and of its list if all) added */
void CTriMesh_AddD3D(CTriMesh *self, CMeshD3D *src, byte all);
/* the triangles of orig with ((flags & mask) == 0) != want added, all its vertices and the skins */
void CTriMesh_Add(CTriMesh *self, const CTriMesh *orig, uint32_t mask, byte want);

/* n made unit length through 1.0 / length in double; a zero length leaves it */
void Mesh_Normalize(vec3_t *n);

/* box recomputed from the vertices, then merged into out (copied if first) */
void CTriMesh_CalcBox(CTriMesh *self, box_t *out, byte first);
void CTriMesh_GetBox(CTriMesh *self, box_t *out, byte recalc);
/* vertices through m, the normals turned and normalized */
void CTriMesh_Transform(CTriMesh *self, const matrix_t *m);

#endif
