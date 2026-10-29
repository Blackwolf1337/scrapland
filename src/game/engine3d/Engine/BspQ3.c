/*
 * Engine/bsp_q3.cpp of the engine3d: the Quake 3 BSP loader, see include/scrap/bspq3.h. Not used by the game (no
 * caller makes a CBspQ3).
 */
#include <string.h>

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
#define g_BspQ3LMapNoBlend GAME_GLOBAL_IN(byte, group_432a7c, 1, 0x432a7d)

typedef CMeshD3D *(__thiscall *bspQ3MeshD3DDelete_t)(CMeshD3D *self, byte flags);
typedef int(__thiscall *bspQ3MeshD3DDev_t)(CMeshD3D *self, void *dev);
typedef CTriMesh *(__thiscall *bspQ3TriMeshDelete_t)(CTriMesh *self, byte flags);
/* ID3DXBaseMesh */
typedef int(__stdcall *bspQ3DrawSubset_t)(void *mesh, uint32_t attrib);
typedef uint32_t(__stdcall *bspQ3GetNum_t)(void *mesh);
typedef int(__stdcall *bspQ3GetAttributeTable_t)(void *mesh, void *table, uint32_t *size);

static void BspQ3_SetStage1(uint32_t colorOp, uint32_t alphaOp) {
	D3D_METHOD(g_pd3dDevice, d3dSetTextureStageState_t, D3DDEV_SETTEXTURESTAGESTATE)(g_pd3dDevice, 1, 1, colorOp);
	D3D_METHOD(g_pd3dDevice, d3dSetTextureStageState_t, D3DDEV_SETTEXTURESTAGESTATE)(g_pd3dDevice, 1, 4, alphaOp);
}

/* the start of the lump readers: the size of the lump, CHECK of a whole number of elements */
static int BspQ3_LumpLen(CBspQ3 *self, int id, int elem) {
	CHECK(self->lump);
	int len = self->head->lumps[id].len;
	CHECK(len % elem == 0);
	return len;
}

static void *BspQ3_LumpCopy(CBspQ3 *self, int id, int len) {
	void *p = operator_new_array((uint32_t)len);
	memcpy(p, self->lump + self->head->lumps[id].ofs, (size_t)len);
	return p;
}

CBspQ3 *__thiscall CBspQ3_ctor(CBspQ3 *self, const char *name) {
	self->name = NULL;
	memset(self, 0, sizeof(*self));
	self->MeshD3D = NULL;
	self->pMesh = NULL;
	self->MeshList = NULL;
	String_Set(&self->name, name);
	void *f = crt_fopen(name, "rb");
	if (!f) {
		Err_Report(*crt_errno(), 1, " (%s)", name);
	}
	uint32_t size = (uint32_t)crt_filelength(crt_fileno(f));
	self->lump = operator_new_array(size);
	CHECK(crt_fread(self->lump, size, 1, f) == 1);
	crt_fclose(f);
	self->head = (BspQ3Header *)self->lump;
	CHECK(self->head->id == BSPQ3_ID);
	CHECK(self->head->ver == BSPQ3_VER || self->head->ver == BSPQ3_VER_RCW);
	CBspQ3_ReadShaders(self);
	CBspQ3_ReadVerts(self);
	CBspQ3_ReadFaces(self);
	CBspQ3_ReadLMaps(self);
	if (self->lump) {
		operator_delete_sized(self->lump, 0);
		self->lump = NULL;
	}
	CBspQ3_Build(self);
	return self;
}

BspQ3Meshes *__fastcall BspQ3Meshes_ctor(BspQ3Meshes *self) {
	memset(self->m, 0, sizeof(self->m));
	self->n = 0;
	return self;
}

BspQ3Mesh *__fastcall BspQ3Mesh_ctor(BspQ3Mesh *self) {
	CVertList_ctor(&self->vert);
	self->face = NULL;
	self->nTri = 0;
	self->totTri = 0;
	self->pTri = NULL;
	self->pVertVB = NULL;
	self->pVertIB = NULL;
	return self;
}

void *__fastcall BspQ3_Identity(void *self) {
	return self;
}

void __fastcall CBspQ3_dtor(CBspQ3 *self) {
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
		COM_RELEASE(self->pMesh);
		self->pMesh = NULL;
	}
	if (self->MeshD3D) {
		((bspQ3MeshD3DDelete_t)self->MeshD3D->vftable[MESHD3D_VF_DELETE])(self->MeshD3D, 1);
		self->MeshD3D = NULL;
	}
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}

void __fastcall BspQ3Meshes_Free(BspQ3Meshes *self) {
	for (int i = 0; i < self->n; i++) {
		BspQ3Mesh *m = self->m[i];
		if (m) {
			BspQ3Mesh_dtor(m);
			operator_delete_sized(m, sizeof(*m));
			self->m[i] = NULL;
		}
	}
}

void __fastcall BspQ3Mesh_dtor(BspQ3Mesh *self) {
	CHECK(!self->pVertVB);
	CHECK(!self->pVertIB);
	if (self->pTri) {
		operator_delete_sized(self->pTri, 6);
		self->pTri = NULL;
	}
	CVertList_Free(&self->vert);
}

long double Vec3_DotX87(const float *a, const float *b) {
	long double r;

	__asm__("flds 4(%1)\n\t"
		"fmuls 4(%2)\n\t"
		"flds (%1)\n\t"
		"fmuls (%2)\n\t"
		"faddp\n\t"
		"flds 8(%1)\n\t"
		"fmuls 8(%2)\n\t"
		"faddp"
		: "=t"(r)
		: "r"(a), "r"(b));
	return r;
}

void __fastcall CBspQ3_ReleaseD3D(CBspQ3 *self) {
	((bspQ3MeshD3DDev_t)self->MeshD3D->vftable[MESHD3D_VF_INVALIDATE])(self->MeshD3D, g_pd3dDevice);
	if (self->pMesh) {
		COM_RELEASE(self->pMesh);
		self->pMesh = NULL;
	}
	for (int i = 0; i < self->MeshList->n; i++) {
		BspQ3Mesh *m = self->MeshList->m[i];
		if (m->pVertVB) {
			COM_RELEASE(m->pVertVB);
			m->pVertVB = NULL;
		}
		if (m->pVertIB) {
			COM_RELEASE(m->pVertIB);
			m->pVertIB = NULL;
		}
	}
}

void __fastcall CBspQ3_InitD3D(CBspQ3 *self) {
	((bspQ3MeshD3DDev_t)self->MeshD3D->vftable[MESHD3D_VF_RESTORE])(self->MeshD3D, g_pd3dDevice);
	for (int i = 0; i < self->MeshList->n; i++) {
		BspQ3Mesh *m = self->MeshList->m[i];
		CHECK(Render_CreateVB(&m->pVertVB, &m->vert, 0) == 0);
		CHECK(Render_CreateIB(&m->pVertIB, m->pTri, m->nTri, 0) == 0);
	}
}

void __fastcall CBspQ3_ReadFaces(CBspQ3 *self) {
	char dir[MAX_FILEPATH], buf[MAX_FILEPATH];
	CMaterial tmp;

	int size = BspQ3_LumpLen(self, BSPQ3_LUMP_FACES, sizeof(BspQ3FileFace));
	int n = size / (int)sizeof(BspQ3FileFace);
	BspQ3FileFace *ff = BspQ3_LumpCopy(self, BSPQ3_LUMP_FACES, size);
	self->nFaces = n;
	self->nPatches = 0;
	self->faces = New_Array((uint32_t)n, sizeof(BspQ3Face));
	self->faceRefs = New_Array((uint32_t)self->nFaces, sizeof(BspQ3FaceRef));
	for (int i = 0; i < self->nFaces; i++) {
		const BspQ3FileFace *f = &ff[i];
		BspQ3Face *out = &self->faces[i];
		switch (f->type) {
		case BSPQ3_POLYGON:
			out->shader = &self->shaders[f->shader];
			out->type = f->type;
			out->verts = &self->verts[f->vert];
			out->nVerts = f->nVerts;
			out->nMeshVerts = f->nMeshVerts;
			out->lmIndex = f->lmIndex;
			{
				float d = f->normal.y * f->lmOrigin.y + f->lmOrigin.x * f->normal.x;
				d = d + f->lmOrigin.z * f->normal.z;
				out->dist = (int)d;
			}
			out->normal = f->normal;
			out->origin = f->lmOrigin;
			self->faceRefs[i].face = out;
			break;
		case BSPQ3_MESH:
			out->type = f->type;
			out->verts = &self->verts[f->vert];
			out->nVerts = f->nVerts;
			out->nMeshVerts = f->nMeshVerts;
			out->lmIndex = f->lmIndex;
			self->faceRefs[i].face = out;
			break;
		case BSPQ3_PATCH:
			out->type = f->type;
			out->verts = &self->verts[f->vert];
			out->nVerts = f->nVerts;
			out->nMeshVerts = f->nMeshVerts;
			out->lmIndex = f->lmIndex;
			out->dist = self->nPatches++;
			out->lmVecs[0] = f->lmVecs[0];
			out->lmVecs[1] = f->lmVecs[1];
			self->faceRefs[i].face = out;
			break;
		default:
			self->faceRefs[i].face = NULL;
			out->type = BSPQ3_OTHER;
			out->nVerts = 0;
			break;
		}
	}
	Path_Part(dir, self->name, PATH_DRIVE | PATH_DIR);
	for (int i = 0; i < self->nFaces; i++) {
		BspQ3Face *face = &self->faces[i];
		face->mat = NULL;
		if (face->type != BSPQ3_POLYGON) {
			continue;
		}
		Path_Merge(buf, dir, face->shader->name);
		size_t len = strlen(buf);
		Str_Copy(buf + len, ".tga", (int)(sizeof(buf) - len));
		CMaterial_ctor(&tmp);
		String_Set(&tmp.Name, self->faces[i].shader->name);
		CMaterial_SetMap(&tmp, MAP_DIFFUSE, buf);
		tmp.params.b39 = 0;
		CMaterial *mat = CMatList_Find(g_MatList, &tmp);
		if (!mat) {
			mat = operator_new(sizeof(CMaterial));
			mat = mat ? CMaterial_ctor(mat) : NULL;
			CMaterial_Assign(mat, &tmp);
			CMaterial_Prepare(mat, g_EngineDev);
			CMatList_Add(g_MatList, mat);
		}
		self->faces[i].mat = mat;
		if (self->faces[i].shader->flags & 0x20) {
			self->faces[i].lmIndex = -1;
		}
		CMaterial_dtor(&tmp);
	}
	E3D_DebugPrintf("\nQ3BSP: faces: %d  patches: %d", self->nFaces, self->nPatches);
	if (ff) {
		operator_delete_sized(ff, sizeof(BspQ3FileFace));
	}
}

void __fastcall CBspQ3_ReadLMaps(CBspQ3 *self) {
	char path[MAX_FILEPATH];

	CHECK(self->lump);
	int size = self->head->lumps[BSPQ3_LUMP_LIGHTMAPS].len;
	byte *data = BspQ3_LumpCopy(self, BSPQ3_LUMP_LIGHTMAPS, size);
	self->nLMaps = size / (BSPQ3_LMAP_SIZE * BSPQ3_LMAP_SIZE * 3);
	self->lmaps = New_Array((uint32_t)self->nLMaps, sizeof(BspQ3LMap));
	byte *src = data;
	for (int i = 0; i < self->nLMaps; i++) {
		BspQ3LMap *lm = &self->lmaps[i];
		lm->data = src;
		lm->bpp = 3;
		lm->height = BSPQ3_LMAP_SIZE;
		lm->width = BSPQ3_LMAP_SIZE;
		lm->i00 = -1;
		Path_Part(path, self->name, PATH_DRIVE | PATH_DIR | PATH_NAME);
		size_t len = strlen(path);
		Bprintf(path + len, (int)(sizeof(path) - len), "_lmap%04d.bmp", i);
		if (crt_access(path, 0)) {
			CBmp bmp;
			bmp.img = NULL;
			bmp.width = BSPQ3_LMAP_SIZE;
			bmp.height = BSPQ3_LMAP_SIZE;
			bmp.img = New_Array(BSPQ3_LMAP_SIZE * BSPQ3_LMAP_SIZE, 4);
			CHECK(bmp.img);
			byte *d = bmp.img;
			const byte *s = self->lmaps[i].data;
			for (int k = BSPQ3_LMAP_SIZE * BSPQ3_LMAP_SIZE; k; k--) {
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
				d += 4;
				s += 3;
			}
			CBmp_Save(&bmp, path, 0);
			Img_Free(&bmp, 4);
		}
		CMaterial *mat = operator_new(sizeof(CMaterial));
		mat = mat ? CMaterial_ctor(mat) : NULL;
		String_Printf(&mat->Name, "Lightmap Mat %04d", i);
		CMaterial_SetMap(mat, MAP_DIFFUSE, path);
		mat->params.b39 = 0;
		mat->map[MAP_DIFFUSE]->lightmap = 1;
		mat->params.w3e &= 0xff;
		CMaterial_Prepare(mat, g_EngineDev);
		CMatList_Add(g_MatList, mat);
		if (!g_BspQ3LMapNoBlend) {
			mat->params.srcBlend = 9;
			mat->params.dstBlend = 3;
			mat->params.transp = 1;
		}
		src += BSPQ3_LMAP_SIZE * BSPQ3_LMAP_SIZE * 3;
		self->lmaps[i].mat = mat;
		self->lmaps[i].data = NULL;
	}
	E3D_DebugPrintf("\nQ3BSP: lightmaps: %d", self->nLMaps);
	if (data) {
		operator_delete_sized(data, 1);
	}
}

void __fastcall CBspQ3_ReadShaders(CBspQ3 *self) {
	int size = BspQ3_LumpLen(self, BSPQ3_LUMP_SHADERS, sizeof(BspQ3Shader));
	int n = size / (int)sizeof(BspQ3Shader);
	self->shaders = BspQ3_LumpCopy(self, BSPQ3_LUMP_SHADERS, size);
	self->nShaders = n;
	E3D_DebugPrintf("\nQ3BSP: shaders: %d", n);
}

void __fastcall CBspQ3_ReadVerts(CBspQ3 *self) {
	int size = BspQ3_LumpLen(self, BSPQ3_LUMP_VERTICES, sizeof(BspQ3Vert));
	int n = size / (int)sizeof(BspQ3Vert);
	self->verts = BspQ3_LumpCopy(self, BSPQ3_LUMP_VERTICES, size);
	self->nVerts = n;
	E3D_DebugPrintf("\nQ3BSP: vertices: %d", n);
}

int __thiscall CBspQ3_ReadLump(CBspQ3 *self, int id, void **out, int elem) {
	CHECK(self->lump);
	CHECK(id >= 0 && id < BSPQ3_NUM_LUMPS);
	int size = self->head->lumps[id].len;
	int n = size / elem;
	CHECK(size % elem == 0);
	*out = BspQ3_LumpCopy(self, id, size);
	return n;
}

void __fastcall CBspQ3_Render(CBspQ3 *self) {
	CHECK(g_pd3dDevice);
	Render_SetWorldMatrix(g_MatIdentity, 0);
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
	D3D_METHOD(self->pMesh, bspQ3GetAttributeTable_t, 0x48)(self->pMesh, NULL, &num);
	CHECK(num <= BSPQ3_SIZE_ATTRIBTABLE);
	D3D_METHOD(self->pMesh, bspQ3GetAttributeTable_t, 0x48)(self->pMesh, g_BspQ3Attrib, &num);
	const uint32_t *at = g_BspQ3Attrib;
	for (uint32_t i = 0; i < num; i++) {
		uint32_t id = *at;
		Render_SetMaterial((void *)id, 0);
		D3D_METHOD(self->pMesh, bspQ3DrawSubset_t, 0xc)(self->pMesh, id);
		at += 5;
	}
	g_RenderTris += D3D_METHOD(self->pMesh, bspQ3GetNum_t, 0x10)(self->pMesh);
	g_RenderVerts += D3D_METHOD(self->pMesh, bspQ3GetNum_t, 0x14)(self->pMesh);
}

void __thiscall BspQ3Meshes_AddTri(BspQ3Meshes *self, const void *a, const void *b, const void *c, BspQ3Face *face) {
	BspQ3Mesh *m = BspQ3Meshes_Find(self, face);
	CHECK(m);
	if (!m->totTri) {
		CHECK(m->nTri);
		m->pTri = New_Array((uint32_t)m->nTri, 6);
		CVertList_SetFormat(&m->vert, VERT_TEX2);
		CVertList_Alloc(&m->vert, m->nTri * 3, 0);
		m->totTri = m->nTri;
		m->nTri = 0;
	}
	CHECK(m->nTri < m->totTri);
	CVertList_SetTex2(&m->vert, (uint32_t)(m->nTri * 3), a);
	CVertList_SetTex2(&m->vert, (uint32_t)(m->nTri * 3 + 1), b);
	CVertList_SetTex2(&m->vert, (uint32_t)(m->nTri * 3 + 2), c);
	uint16_t t = (uint16_t)m->nTri;
	m->pTri[m->nTri][0] = (uint16_t)(t + t * 2);
	m->pTri[m->nTri][1] = (uint16_t)(t + 1 + t * 2);
	m->pTri[m->nTri][2] = (uint16_t)(t + (uint16_t)(t * 2 + 2));
	m->nTri++;
}

BspQ3Mesh *__thiscall BspQ3Meshes_Find(BspQ3Meshes *self, BspQ3Face *face) {
	CHECK(face);
	for (int i = 0; i < self->n; i++) {
		BspQ3Mesh *m = self->m[i];
		if (face->mat == m->face->mat && face->lmIndex == m->face->lmIndex) {
			return m;
		}
	}
	return NULL;
}

void __thiscall BspQ3Meshes_Count(BspQ3Meshes *self, BspQ3Face *face, int ntri) {
	BspQ3Mesh *m = BspQ3Meshes_Find(self, face);
	if (!m) {
		CHECK(self->n < BSPQ3_MAX_MESHES);
		m = operator_new(sizeof(BspQ3Mesh));
		m = m ? BspQ3Mesh_ctor(m) : NULL;
		m->face = face;
		self->m[self->n] = m;
		self->n++;
	}
	m->nTri += ntri;
}

void __fastcall CBspQ3_Build(CBspQ3 *self) {
	CHECK(!self->MeshD3D);
	CHECK(!self->MeshList);
	BspQ3Meshes *lm = operator_new(sizeof(BspQ3Meshes));
	self->MeshList = lm ? BspQ3Meshes_ctor(lm) : NULL;
	for (int i = 0; i < self->nFaces; i++) {
		BspQ3Face *f = &self->faces[i];
		if (f->type == BSPQ3_POLYGON && f->nVerts >= 3) {
			BspQ3Meshes_Count(self->MeshList, f, f->nVerts - 2);
		}
	}
	CTriMesh *mt = operator_new(sizeof(CTriMesh));
	mt = mt ? CTriMesh_ctor(mt) : NULL;
	for (int i = 0; i < self->nFaces; i++) {
		BspQ3Face *f = &self->faces[i];
		if (f->type != BSPQ3_POLYGON || f->nVerts < 3) {
			continue;
		}
		const BspQ3Vert *v = f->verts;
		CHECK(f->nVerts < BSPQ3_MAX_VERT);
		for (int k = 0; k < f->nVerts; k++, v++) {
			CMeshVert *d = &g_BspQ3Vert[k];
			float *l = (float *)(g_BspQ3VertL + k * 0x28);
			/* z up to y up */
			d->pos.x = v->pos.x;
			d->pos.y = v->pos.z;
			d->pos.z = v->pos.y;
			d->n.x = v->n.x;
			d->n.y = v->n.z;
			d->n.z = v->n.y;
			d->uv.u = v->tc[0];
			d->uv.v = v->tc[1];
			l[0] = v->pos.x;
			l[1] = v->pos.z;
			l[2] = v->pos.y;
			l[3] = v->n.x;
			l[4] = v->n.z;
			l[5] = v->n.y;
			l[6] = v->tc[0];
			l[7] = v->tc[1];
			l[8] = v->lm[0];
			l[9] = v->lm[1];
		}
		for (int k = 2; k < f->nVerts; k++) {
			CTriMesh_AddTriVerts(mt, &g_BspQ3Vert[0], &g_BspQ3Vert[k - 1], &g_BspQ3Vert[k], f->mat, 1, 0);
			BspQ3Meshes_AddTri(self->MeshList, g_BspQ3VertL, g_BspQ3VertL + (k - 1) * 0x28,
				g_BspQ3VertL + k * 0x28, f);
		}
	}
	CMeshD3D *md = operator_new(sizeof(CMeshD3D));
	self->MeshD3D = md ? CMeshD3D_ctorMesh(md, mt, 0, NULL) : NULL;
	if (mt) {
		((bspQ3TriMeshDelete_t)((void **)mt->vftable)[0])(mt, 1);
	}
	for (int i = 0; i < self->MeshList->n; i++) {
		BspQ3Mesh *m = self->MeshList->m[i];
		CVertList_Optimize(&m->vert, m->pTri, m->nTri);
	}
}

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

/* the static initializer (rva 0x05f770) of a vertex array after g_BspQ3VertL that no code reads */
#define BSPQ3_VERTG 0x40
#define g_BspQ3VertG (GAME_ARRAY(Graphic3DVert, g_BspQ3VertG, 0x4e54f8))	/* [BSPQ3_VERTG] */

void BspQ3_InitVertG(void) {
	for (int i = 0; i < BSPQ3_VERTG; i++) {
		Graphic3DVert_ctor(&g_BspQ3VertG[i]);
	}
}
