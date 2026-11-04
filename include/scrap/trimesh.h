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
	int v[3];
	int tv[3];
	vec3_t n[3];
	void *mat;			/* material (Engine/render.cpp) */
	uint32_t flags;
} CMeshTri;

typedef struct CTriMesh {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;
			uint32_t class;			/* CLASS_OBJ3D_TRIMESH */
		};
	};
	int nVert;
	int nTVert;
	int nTri;
	int maxVert;			/* allocated */
	int maxTVert;
	int maxTri;
	vec3_t *pVert;
	uv_t *pTVert;
	CMeshTri *pTri;
	byte *pSkin;			/* 16 bytes a vertex */
	CMeshD3D *d3d;			/* deleted with the mesh */
	box_t box;			/* of the vertices, CTriMesh_CalcBox */
} CTriMesh;

/* ListaVert.h, instanced in MallaTri.cpp: the normal and the texture coordinates of vertex i; a format without them
 * is a failed CHECK and zeros */
vec3_t *CVertList_GetNormal(CVertList *self, vec3_t *out, uint32_t i);
uv_t *CVertList_GetUV(CVertList *self, uv_t *out, uint32_t i);

CTriMesh *CTriMesh_ctor(CTriMesh *self);
CTriMesh *CTriMesh_ctorCopy(CTriMesh *self, const CTriMesh *orig);
void CTriMesh_dtor(CTriMesh *self);
/* the arrays and the D3D mesh freed, everything 0 */
void CTriMesh_Clear(CTriMesh *self);

/* the counts set; a larger one than allocated doubles the array (realloc: else CHECK there is none yet) */
void CTriMesh_SetNumVert(CTriMesh *self, int n, byte realloc);
/* 0 frees the texture vertices (the count stays) */
void CTriMesh_SetNumTVert(CTriMesh *self, int n, byte realloc);
void CTriMesh_SetNumTri(CTriMesh *self, int n, byte realloc);
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
/* corner vert of triangle tri; uv 0 without texture vertices */
void CTriMesh_GetTriVert(CTriMesh *self, CMeshVert *out, uint32_t tri, uint32_t vert);
/* a triangle of new vertices (and texture vertices if tvert) */
void CTriMesh_AddTriVerts(CTriMesh *self, const CMeshVert *a, const CMeshVert *b, const CMeshVert *c,
	void *mat, byte tvert, uint32_t flags);

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
