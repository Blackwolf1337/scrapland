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
#include "scrap/grafico3d.h"
#include "scrap/mallatri.h"
#include "scrap/mundo.h"
#include "scrap/render.h"
#include "scrap/varios.h"

/* 1 in the exe: the light map materials keep the blend of CMaterial_Prepare */
#define g_BspQ3LMapNoBlend GAME_GLOBAL_IN(byte, group_432a7c, 1, 0x432a7d)

typedef CMallaD3D *(__thiscall *bspQ3MallaD3DDelete_t)(CMallaD3D *self, byte flags);
typedef int(__thiscall *bspQ3MallaD3DDev_t)(CMallaD3D *self, void *dev);
typedef CMallaTri *(__thiscall *bspQ3MallaTriDelete_t)(CMallaTri *self, byte flags);
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
	self->MallaD3D = NULL;
	self->pMesh = NULL;
	self->ListaMallas = NULL;
	Cad_Set(&self->name, name);
	void *f = crt_fopen(name, "rb");
	if (!f) {
		Err_Report(*crt_errno(), 1, " (%s)", name);
	}
	uint32_t tam = (uint32_t)crt_filelength(crt_fileno(f));
	self->lump = operator_new_array(tam);
	CHECK(crt_fread(self->lump, tam, 1, f) == 1);
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

BspQ3Mallas *__fastcall BspQ3Mallas_ctor(BspQ3Mallas *self) {
	memset(self->m, 0, sizeof(self->m));
	self->n = 0;
	return self;
}

BspQ3Malla *__fastcall BspQ3Malla_ctor(BspQ3Malla *self) {
	CListaVert_ctor(&self->vert);
	self->face = NULL;
	self->nTri = 0;
	self->totTri = 0;
	self->pTri = NULL;
	self->pVertVB = NULL;
	self->pVertIB = NULL;
	return self;
}

void *__fastcall BspQ3_Identidad(void *self) {
	return self;
}

void __fastcall CBspQ3_dtor(CBspQ3 *self) {
	self->head = NULL;
	if (self->lump) {
		operator_delete_sized(self->lump, 0);
		self->lump = NULL;
	}
	BspQ3Mallas *lm = self->ListaMallas;
	if (lm) {
		BspQ3Mallas_Free(lm);
		operator_delete_sized(lm, sizeof(*lm));
		self->ListaMallas = NULL;
	}
	if (self->pMesh) {
		COM_RELEASE(self->pMesh);
		self->pMesh = NULL;
	}
	if (self->MallaD3D) {
		((bspQ3MallaD3DDelete_t)self->MallaD3D->vftable[MALLAD3D_VF_DELETE])(self->MallaD3D, 1);
		self->MallaD3D = NULL;
	}
	if (self->name) {
		Cad_Set(&self->name, NULL);
	}
}

void __fastcall BspQ3Mallas_Free(BspQ3Mallas *self) {
	for (int i = 0; i < self->n; i++) {
		BspQ3Malla *m = self->m[i];
		if (m) {
			BspQ3Malla_dtor(m);
			operator_delete_sized(m, sizeof(*m));
			self->m[i] = NULL;
		}
	}
}

void __fastcall BspQ3Malla_dtor(BspQ3Malla *self) {
	CHECK(!self->pVertVB);
	CHECK(!self->pVertIB);
	if (self->pTri) {
		operator_delete_sized(self->pTri, 6);
		self->pTri = NULL;
	}
	CListaVert_Free(&self->vert);
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
	((bspQ3MallaD3DDev_t)self->MallaD3D->vftable[MALLAD3D_VF_INVALIDATE])(self->MallaD3D, g_pd3dDevice);
	if (self->pMesh) {
		COM_RELEASE(self->pMesh);
		self->pMesh = NULL;
	}
	for (int i = 0; i < self->ListaMallas->n; i++) {
		BspQ3Malla *m = self->ListaMallas->m[i];
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
	((bspQ3MallaD3DDev_t)self->MallaD3D->vftable[MALLAD3D_VF_RESTORE])(self->MallaD3D, g_pd3dDevice);
	for (int i = 0; i < self->ListaMallas->n; i++) {
		BspQ3Malla *m = self->ListaMallas->m[i];
		CHECK(Render_CreateVB(&m->pVertVB, &m->vert, 0) == 0);
		CHECK(Render_CreateIB(&m->pVertIB, m->pTri, m->nTri, 0) == 0);
	}
}

void __fastcall CBspQ3_ReadFaces(CBspQ3 *self) {
	char dir[MAX_RUTA], buf[MAX_RUTA];
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
	Ruta_Part(dir, self->name, RUTA_DRIVE | RUTA_DIR);
	for (int i = 0; i < self->nFaces; i++) {
		BspQ3Face *face = &self->faces[i];
		face->mat = NULL;
		if (face->type != BSPQ3_POLYGON) {
			continue;
		}
		Ruta_Merge(buf, dir, face->shader->name);
		size_t len = strlen(buf);
		Str_Copy(buf + len, ".tga", (int)(sizeof(buf) - len));
		CMaterial_ctor(&tmp);
		Cad_Set(&tmp.Nombre, self->faces[i].shader->name);
		CMaterial_SetMap(&tmp, MAPA_DIFFUSE, buf);
		tmp.params.b39 = 0;
		CMaterial *mat = CListaMat_Find(g_ListaMat, &tmp);
		if (!mat) {
			mat = operator_new(sizeof(CMaterial));
			mat = mat ? CMaterial_ctor(mat) : NULL;
			CMaterial_Assign(mat, &tmp);
			CMaterial_Prepare(mat, g_MotorDev);
			CListaMat_Add(g_ListaMat, mat);
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
	char path[MAX_RUTA];

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
		lm->alto = BSPQ3_LMAP_SIZE;
		lm->ancho = BSPQ3_LMAP_SIZE;
		lm->i00 = -1;
		Ruta_Part(path, self->name, RUTA_DRIVE | RUTA_DIR | RUTA_NAME);
		size_t len = strlen(path);
		Bprintf(path + len, (int)(sizeof(path) - len), "_lmap%04d.bmp", i);
		if (crt_access(path, 0)) {
			CBmp bmp;
			bmp.img = NULL;
			bmp.ancho = BSPQ3_LMAP_SIZE;
			bmp.alto = BSPQ3_LMAP_SIZE;
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
		Cad_Printf(&mat->Nombre, "Lightmap Mat %04d", i);
		CMaterial_SetMap(mat, MAPA_DIFFUSE, path);
		mat->params.b39 = 0;
		mat->mapa[MAPA_DIFFUSE]->lightmap = 1;
		mat->params.w3e &= 0xff;
		CMaterial_Prepare(mat, g_MotorDev);
		CListaMat_Add(g_ListaMat, mat);
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
	CHECK(self->MallaD3D);
	if (!g_KeyL) {
		for (int i = 0; i < self->ListaMallas->n; i++) {
			BspQ3Malla *m = self->ListaMallas->m[i];
			BspQ3Face *face = m->face;
			CHECK(m->nTri == m->totTri);
			Render_SetMaterial(face->mat, 0);
			uint32_t lmi = (uint32_t)face->lmIndex;
			if (lmi < (uint32_t)self->nLMaps) {
				BspQ3LMap *lm = &self->lmaps[lmi];
				BspQ3_SetStage1(6, 3);
				Render_SetMapa(1, lm->mat->mapa[MAPA_DIFFUSE]);
			} else {
				BspQ3_SetStage1(1, 1);
			}
			if (m->pVertVB && g_UsarVB) {
				Render_DrawVBIB(m->pVertVB, m->pVertIB, &m->vert, (uint32_t)m->nTri);
			} else {
				Render_DrawListaIdx(&m->vert, &m->pTri[0][0], m->nTri, 0);
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
	CHECK(num <= BSPQ3_TAM_ATTRIBTABLE);
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

void __thiscall BspQ3Mallas_AddTri(BspQ3Mallas *self, const void *a, const void *b, const void *c, BspQ3Face *face) {
	BspQ3Malla *m = BspQ3Mallas_Find(self, face);
	CHECK(m);
	if (!m->totTri) {
		CHECK(m->nTri);
		m->pTri = New_Array((uint32_t)m->nTri, 6);
		CListaVert_SetFormat(&m->vert, VERT_TEX2);
		CListaVert_Alloc(&m->vert, m->nTri * 3, 0);
		m->totTri = m->nTri;
		m->nTri = 0;
	}
	CHECK(m->nTri < m->totTri);
	CListaVert_SetTex2(&m->vert, (uint32_t)(m->nTri * 3), a);
	CListaVert_SetTex2(&m->vert, (uint32_t)(m->nTri * 3 + 1), b);
	CListaVert_SetTex2(&m->vert, (uint32_t)(m->nTri * 3 + 2), c);
	uint16_t t = (uint16_t)m->nTri;
	m->pTri[m->nTri][0] = (uint16_t)(t + t * 2);
	m->pTri[m->nTri][1] = (uint16_t)(t + 1 + t * 2);
	m->pTri[m->nTri][2] = (uint16_t)(t + (uint16_t)(t * 2 + 2));
	m->nTri++;
}

BspQ3Malla *__thiscall BspQ3Mallas_Find(BspQ3Mallas *self, BspQ3Face *face) {
	CHECK(face);
	for (int i = 0; i < self->n; i++) {
		BspQ3Malla *m = self->m[i];
		if (face->mat == m->face->mat && face->lmIndex == m->face->lmIndex) {
			return m;
		}
	}
	return NULL;
}

void __thiscall BspQ3Mallas_Count(BspQ3Mallas *self, BspQ3Face *face, int ntri) {
	BspQ3Malla *m = BspQ3Mallas_Find(self, face);
	if (!m) {
		CHECK(self->n < BSPQ3_MAX_MALLAS);
		m = operator_new(sizeof(BspQ3Malla));
		m = m ? BspQ3Malla_ctor(m) : NULL;
		m->face = face;
		self->m[self->n] = m;
		self->n++;
	}
	m->nTri += ntri;
}

void __fastcall CBspQ3_Build(CBspQ3 *self) {
	CHECK(!self->MallaD3D);
	CHECK(!self->ListaMallas);
	BspQ3Mallas *lm = operator_new(sizeof(BspQ3Mallas));
	self->ListaMallas = lm ? BspQ3Mallas_ctor(lm) : NULL;
	for (int i = 0; i < self->nFaces; i++) {
		BspQ3Face *f = &self->faces[i];
		if (f->type == BSPQ3_POLYGON && f->nVerts >= 3) {
			BspQ3Mallas_Count(self->ListaMallas, f, f->nVerts - 2);
		}
	}
	CMallaTri *mt = operator_new(sizeof(CMallaTri));
	mt = mt ? CMallaTri_ctor(mt) : NULL;
	for (int i = 0; i < self->nFaces; i++) {
		BspQ3Face *f = &self->faces[i];
		if (f->type != BSPQ3_POLYGON || f->nVerts < 3) {
			continue;
		}
		const BspQ3Vert *v = f->verts;
		CHECK(f->nVerts < BSPQ3_MAX_VERT);
		for (int k = 0; k < f->nVerts; k++, v++) {
			CVertMalla *d = &g_BspQ3Vert[k];
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
			CMallaTri_AddTriVerts(mt, &g_BspQ3Vert[0], &g_BspQ3Vert[k - 1], &g_BspQ3Vert[k], f->mat, 1, 0);
			BspQ3Mallas_AddTri(self->ListaMallas, g_BspQ3VertL, g_BspQ3VertL + (k - 1) * 0x28,
				g_BspQ3VertL + k * 0x28, f);
		}
	}
	CMallaD3D *md = operator_new(sizeof(CMallaD3D));
	self->MallaD3D = md ? CMallaD3D_ctorMalla(md, mt, 0, NULL) : NULL;
	if (mt) {
		((bspQ3MallaTriDelete_t)((void **)mt->vftable)[0])(mt, 1);
	}
	for (int i = 0; i < self->ListaMallas->n; i++) {
		BspQ3Malla *m = self->ListaMallas->m[i];
		CListaVert_Optimize(&m->vert, m->pTri, m->nTri);
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

void BspQ3_Crear(void) {
}

void BspQ3_Render(void) {
	if (g_BspQ3) {
		CBspQ3_Render(g_BspQ3);
	}
}

/* the static initializer (rva 0x05f770) of a vertex array after g_BspQ3VertL that no code reads */
#define BSPQ3_VERTG 0x40
#define g_BspQ3VertG (GAME_ARRAY(Grafico3DVert, g_BspQ3VertG, 0x4e54f8))	/* [BSPQ3_VERTG] */

void BspQ3_InitVertG(void) {
	for (int i = 0; i < BSPQ3_VERTG; i++) {
		Grafico3DVert_ctor(&g_BspQ3VertG[i]);
	}
}
