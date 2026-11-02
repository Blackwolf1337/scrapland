/*
 * Engine/bsp_q3.cpp of the engine3d: the Quake 3 BSP loader, see include/scrap/bspq3.h. Not used by the game (no
 * caller makes a CBspQ3).
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/bmp.h"
#include "scrap/bspq3.h"
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/graphic3d.h"
#include "scrap/trimesh.h"
#include "scrap/e3dworld.h"
#include "scrap/render.h"
#include "scrap/misc.h"

/* 1 in the exe: the light map materials keep the blend of CMaterial_Prepare */

/* ID3DXBaseMesh */

static void BspQ3_SetStage1(uint32_t colorOp, uint32_t alphaOp) {
	Rd_DevSetTextureStageState(g_pd3dDevice, 1, 1, colorOp);
	Rd_DevSetTextureStageState(g_pd3dDevice, 1, 4, alphaOp);
}

/* the map loaded: shaders, vertices, faces, light maps, then the meshes */
static void BspQ3Meshes_Free(BspQ3Meshes *self);
static void CBspQ3_dtor(CBspQ3 *self) {
	self->head = NULL;
	if (self->lump) {
		operator_delete_sized(self->lump, 0);
		self->lump = NULL;
	}
	BspQ3Meshes *lm = self->MeshList;
	if (lm) {
		BspQ3Meshes_Free(lm);
		operator_delete_sized(lm, sizeof(*lm));
		self->MeshList = NULL;
	}
	if (self->pMesh) {
		Rd_Release(self->pMesh);
		self->pMesh = NULL;
	}
	if (self->MeshD3D) {
		self->MeshD3D->vftable->Delete(&self->MeshD3D->base, 1);
		self->MeshD3D = NULL;
	}
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}

static void BspQ3Mesh_dtor(BspQ3Mesh *self);
static void BspQ3Meshes_Free(BspQ3Meshes *self) {
	for (int i = 0; i < self->n; i++) {
		BspQ3Mesh *m = self->m[i];
		if (m) {
			BspQ3Mesh_dtor(m);
			operator_delete_sized(m, sizeof(*m));
			self->m[i] = NULL;
		}
	}
}

static void BspQ3Mesh_dtor(BspQ3Mesh *self) {
	CHECK(!self->pVertVB);
	CHECK(!self->pVertIB);
	if (self->pTri) {
		operator_delete_sized(self->pTri, 6);
		self->pTri = NULL;
	}
	CVertList_Free(&self->vert);
}

static void CBspQ3_ReleaseD3D(CBspQ3 *self) {
	self->MeshD3D->vftable->ReleaseD3D(&self->MeshD3D->base, g_pd3dDevice);
	if (self->pMesh) {
		Rd_Release(self->pMesh);
		self->pMesh = NULL;
	}
	for (int i = 0; i < self->MeshList->n; i++) {
		BspQ3Mesh *m = self->MeshList->m[i];
		if (m->pVertVB) {
			Rd_Release(m->pVertVB);
			m->pVertVB = NULL;
		}
		if (m->pVertIB) {
			Rd_Release(m->pVertIB);
			m->pVertIB = NULL;
		}
	}
}

static void CBspQ3_InitD3D(CBspQ3 *self) {
	self->MeshD3D->vftable->InitD3D(&self->MeshD3D->base, g_pd3dDevice);
	for (int i = 0; i < self->MeshList->n; i++) {
		BspQ3Mesh *m = self->MeshList->m[i];
		CHECK(Render_CreateVB(&m->pVertVB, &m->vert, 0) == 0);
		CHECK(Render_CreateIB(&m->pVertIB, m->pTri, m->nTri, 0) == 0);
	}
}

/* D3DXATTRIBUTERANGE of pMesh, 0x14 bytes each */
static uint32_t g_BspQ3Attrib[640];

static void CBspQ3_Render(CBspQ3 *self) {
	CHECK(g_pd3dDevice);
	Render_SetWorldMatrix((&g_MatIdentity), 0);
	CHECK(self->MeshD3D);
	if (!g_KeyL) {
		for (int i = 0; i < self->MeshList->n; i++) {
			BspQ3Mesh *m = self->MeshList->m[i];
			BspQ3Face *face = m->face;
			CHECK(m->nTri == m->totTri);
			Render_SetMaterial(face->mat, 0);
			uint32_t lmi = (uint32_t)face->lmIndex;
			if (lmi < (uint32_t)self->nLMaps) {
				BspQ3LMap *lm = &self->lmaps[lmi];
				BspQ3_SetStage1(6, 3);
				Render_SetMap(1, lm->mat->map[MAP_DIFFUSE]);
			} else {
				BspQ3_SetStage1(1, 1);
			}
			if (m->pVertVB && g_UseVB) {
				Render_DrawVBIB(m->pVertVB, m->pVertIB, &m->vert, (uint32_t)m->nTri);
			} else {
				Render_DrawListIdx(&m->vert, &m->pTri[0][0], m->nTri, 0);
			}
			g_RenderTris += m->nTri;
			g_RenderVerts += m->vert.nVert;
		}
		BspQ3_SetStage1(1, 1);
		return;
	}
	CHECK(self->pMesh);
	uint32_t num;
	Rd_XMeshGetAttributeTable(self->pMesh, NULL, &num);
	CHECK(num <= BSPQ3_SIZE_ATTRIBTABLE);
	Rd_XMeshGetAttributeTable(self->pMesh, g_BspQ3Attrib, &num);
	const uint32_t *at = g_BspQ3Attrib;
	for (uint32_t i = 0; i < num; i++) {
		uint32_t id = *at;
		Render_SetMaterial((void *)(uintptr_t)id, 0);	/* the subset number as the material, as the original */
		Rd_XMeshDrawSubset(self->pMesh, id);
		at += 5;
	}
	g_RenderTris += Rd_XMeshGetNumFaces(self->pMesh);
	g_RenderVerts += Rd_XMeshGetNumVertices(self->pMesh);
}

/* a triangle of the corners (0x28 bytes each) into the mesh of face; the first one allocates the counted ones */
static CBspQ3 *g_BspQ3;

void BspQ3_ReleaseD3D(void) {
	if (g_BspQ3) {
		CBspQ3_ReleaseD3D(g_BspQ3);
	}
}

void BspQ3_InitD3D(void) {
	if (g_BspQ3) {
		CBspQ3_InitD3D(g_BspQ3);
	}
}

void BspQ3_Free(void) {
	CBspQ3 *b = g_BspQ3;
	if (b) {
		CBspQ3_dtor(b);
		operator_delete_sized(b, sizeof(*b));
		g_BspQ3 = NULL;
	}
}

void BspQ3_Create(void) {
}

void BspQ3_Render(void) {
	if (g_BspQ3) {
		CBspQ3_Render(g_BspQ3);
	}
}

/* the static initializer of a vertex array after g_BspQ3VertL that no code reads */
#define BSPQ3_VERTG 0x40

static Graphic3DVert g_BspQ3VertG[64];

void BspQ3_InitVertG(void) {
	for (int i = 0; i < BSPQ3_VERTG; i++) {
		Graphic3DVert_ctor(&g_BspQ3VertG[i]);
	}
}
