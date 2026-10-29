/*
 * Triangle meshes of the engine3d, original source Engine/MallaTri.cpp (include/scrap/trimesh.h), with the
 * CVertList accessors of ListaVert.h it instances.
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/trimesh.h"

#define CTRIMESH_VFTABLE GAME_VFTABLE(CTriMesh_vftable, 0x3dbcf0)
#define IOBJECT3D_VFTABLE GAME_VFTABLE(IObject3D_vftable, 0x3d79dc)
#define TRIMESH_SKIN 16
/* the box before the first vertex, rva 0x3a3a38 and 0x3a512c */
#define TRIMESH_FLT_MAX 3.4028234663852886e+38f

/* draw the meshes through their D3D mesh */
#define g_MeshD3DOn GAME_GLOBAL_IN(byte, group_431bac, 1, 0x431bad)
/* statistics of the frame */

typedef void(__thiscall *meshD3DDelete_t)(CMeshD3D *self, int flags);
typedef void(__thiscall *meshD3DDraw_t)(CMeshD3D *self, void *dev, int arg);

/* the counts as the add functions take them: 0 without the array */
static int Mesh_NumVert(const CTriMesh *m) {
	return m->pVert ? m->nVert : 0;
}

static int Mesh_NumTVert(const CTriMesh *m) {
	return m->pTVert ? m->nTVert : 0;
}

static int Mesh_NumTri(const CTriMesh *m) {
	return m->pTri ? m->nTri : 0;
}

CMeshTri *__fastcall CMeshTri_ctor(CMeshTri *self) {
	self->mat = NULL;
	self->flags = 0;
	return self;
}

/* new[] of count items; the triangles get their constructor */
static void *Mesh_New(uint32_t count, uint32_t size, int tris) {
	byte *p = New_Array(count, size);
	if (p && tris) {
		for (uint32_t i = 0; i < count; i++) {
			CMeshTri_ctor((CMeshTri *)(p + i * size));
		}
	}
	return p;
}

/* *num = n; the array is made with n items, or with 2n when n is more than *max */
static void Mesh_Resize(void **p, int *num, int *max, int n, uint32_t size, int tris) {
	void *old = *p;
	if (!old) {
		*max = n;
		*p = Mesh_New(n, size, tris);
	} else if (n > *max) {
		*max = n * 2;
		void *np = Mesh_New((uint32_t)n * 2, size, tris);
		*p = np;
		memcpy(np, old, *num * size);
		operator_delete_array(old);
	}
	*num = n;
}

static void *Mesh_Free(void *p) {
	if (p) {
		operator_delete_array(p);
	}
	return NULL;
}

void __fastcall CTriMesh_Init(CTriMesh *self) {
	self->class = CLASS_OBJ3D_TRIMESH;
	self->nTri = 0;
	self->nTVert = 0;
	self->nVert = 0;
	self->maxTri = 0;
	self->maxTVert = 0;
	self->maxVert = 0;
	self->pVert = NULL;
	self->pTVert = NULL;
	self->pTri = NULL;
	self->d3d = NULL;
	self->pSkin = NULL;
}

CTriMesh *__fastcall CTriMesh_ctor(CTriMesh *self) {
	self->vftable = CTRIMESH_VFTABLE;
	memset(&self->box, 0, sizeof(self->box));
	CTriMesh_Init(self);
	return self;
}

CTriMesh *__thiscall CTriMesh_ctorCopy(CTriMesh *self, const CTriMesh *orig) {
	CTriMesh_ctor(self);
	CTriMesh_Add(self, orig, 0, 0);
	return self;
}

CTriMesh *__thiscall CTriMesh_ctorD3D(CTriMesh *self, CMeshD3D *src, byte all) {
	CTriMesh_ctor(self);
	CTriMesh_AddD3D(self, src, all);
	return self;
}

void __fastcall CTriMesh_Clear(CTriMesh *self) {
	self->pVert = Mesh_Free(self->pVert);
	self->pTVert = Mesh_Free(self->pTVert);
	self->pTri = Mesh_Free(self->pTri);
	if (self->d3d) {
		((meshD3DDelete_t)self->d3d->vftable[0])(self->d3d, 1);
		self->d3d = NULL;
	}
	self->pSkin = Mesh_Free(self->pSkin);
	self->nTri = 0;
	self->nTVert = 0;
	self->nVert = 0;
	self->maxTri = 0;
	self->maxTVert = 0;
	self->maxVert = 0;
	memset(&self->box, 0, sizeof(self->box));
}

void __fastcall CTriMesh_dtor(CTriMesh *self) {
	self->vftable = CTRIMESH_VFTABLE;
	CTriMesh_Clear(self);
	self->vftable = IOBJECT3D_VFTABLE;
}

CTriMesh *__thiscall CTriMesh_Delete(CTriMesh *self, byte flags) {
	CTriMesh_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte __thiscall CTriMesh_NoFile(CTriMesh *self, void *f, void *ctx) {
	(void)self;
	(void)f;
	(void)ctx;
	return 0;
}

void __thiscall CTriMesh_SetNumVert(CTriMesh *self, int n, byte realloc) {
	CHECK(n > 0);
	CHECK(realloc || !self->pVert);
	Mesh_Resize((void **)&self->pVert, &self->nVert, &self->maxVert, n, sizeof(vec3_t), 0);
}

void __thiscall CTriMesh_SetNumTVert(CTriMesh *self, int n, byte realloc) {
	CHECK(n >= 0);
	CHECK(realloc || !self->pTVert);
	if (!n) {
		self->pTVert = Mesh_Free(self->pTVert);
		return;
	}
	Mesh_Resize((void **)&self->pTVert, &self->nTVert, &self->maxTVert, n, sizeof(uv_t), 0);
}

void __thiscall CTriMesh_SetNumTri(CTriMesh *self, int n, byte realloc) {
	CHECK(n > 0);
	CHECK(realloc || !self->pTri);
	Mesh_Resize((void **)&self->pTri, &self->nTri, &self->maxTri, n, sizeof(CMeshTri), 1);
}

int __fastcall CTriMesh_AddVert(CTriMesh *self) {
	int n = Mesh_NumVert(self);
	CTriMesh_SetNumVert(self, n + 1, 1);
	return n;
}

int __fastcall CTriMesh_AddTVert(CTriMesh *self) {
	int n = Mesh_NumTVert(self);
	CTriMesh_SetNumTVert(self, n + 1, 1);
	return n;
}

int __fastcall CTriMesh_AddTri(CTriMesh *self) {
	int n = Mesh_NumTri(self);
	CTriMesh_SetNumTri(self, n + 1, 1);
	return n;
}

void *__fastcall CTriMesh_AllocSkin(CTriMesh *self) {
	CHECK(self->pSkin == NULL);
	CHECK(self->nVert > 0);
	self->pSkin = New_Array(self->nVert, TRIMESH_SKIN);
	return self->pSkin;
}

void *__thiscall CTriMesh_GetSkin(CTriMesh *self, uint32_t i) {
	CHECK(i < (uint32_t)self->nVert);
	CHECK(self->pSkin);
	return self->pSkin + i * TRIMESH_SKIN;
}

void __thiscall CTriMesh_SetVert(CTriMesh *self, uint32_t i, const vec3_t *v) {
	CHECK(i < (uint32_t)self->nVert);
	CHECK(self->pVert);
	self->pVert[i] = *v;
}

vec3_t *__thiscall CTriMesh_GetVert(CTriMesh *self, vec3_t *out, uint32_t i) {
	CHECK(i < (uint32_t)self->nVert);
	CHECK(self->pVert);
	*out = self->pVert[i];
	return out;
}

void __thiscall CTriMesh_SetTVert(CTriMesh *self, uint32_t i, const uv_t *uv) {
	CHECK(i < (uint32_t)self->nTVert);
	CHECK(self->pTVert);
	self->pTVert[i] = *uv;
}

CMeshTri *__thiscall CTriMesh_GetTri(CTriMesh *self, uint32_t i) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	return &self->pTri[i];
}

void __thiscall CTriMesh_SetTriVerts(CTriMesh *self, uint32_t i, int a, int b, int c) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].v[0] = a;
	self->pTri[i].v[1] = b;
	self->pTri[i].v[2] = c;
}

void __thiscall CTriMesh_SetTriTVerts(CTriMesh *self, uint32_t i, int a, int b, int c) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].tv[0] = a;
	self->pTri[i].tv[1] = b;
	self->pTri[i].tv[2] = c;
}

void __thiscall CTriMesh_SetTriMat(CTriMesh *self, uint32_t i, void *mat) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].mat = mat;
}

void __thiscall CTriMesh_SetTriNormals(CTriMesh *self, uint32_t i, const vec3_t *a, const vec3_t *b, const vec3_t *c) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].n[0] = *a;
	self->pTri[i].n[1] = *b;
	self->pTri[i].n[2] = *c;
}

void __thiscall CTriMesh_SetTriFlags(CTriMesh *self, uint32_t i, uint32_t flags) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].flags = flags;
}

void __thiscall CTriMesh_RemoveTri(CTriMesh *self, uint32_t i) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i] = self->pTri[self->nTri - 1];
	self->nTri--;
}

void __thiscall CTriMesh_GetTriVert(CTriMesh *self, CMeshVert *out, uint32_t tri, uint32_t vert) {
	CHECK(tri < (uint32_t)self->nTri);
	CHECK(vert < 3);
	CHECK(self->pTri && self->pVert);
	const CMeshTri *t = &self->pTri[tri];
	uv_t uv = { 0.0f, 0.0f };
	if (self->pTVert) {
		uv = self->pTVert[t->tv[vert]];
	}
	out->pos = self->pVert[t->v[vert]];
	out->n = t->n[vert];
	out->uv = uv;
}

void __thiscall CTriMesh_AddTriVerts(CTriMesh *self, const CMeshVert *a, const CMeshVert *b, const CMeshVert *c,
	void *mat, byte tvert, uint32_t flags) {
	int nv = Mesh_NumVert(self);
	int ntv = Mesh_NumTVert(self);
	int nt = Mesh_NumTri(self);

	if (tvert) {
		CHECK(self->pTVert || !self->pTri);
		CTriMesh_SetNumTVert(self, ntv + 3, 1);
	} else {
		CHECK(!self->pTVert);
	}
	CTriMesh_SetNumVert(self, nv + 3, 1);
	CTriMesh_SetNumTri(self, nt + 1, 1);
	vec3_t pos = a->pos;
	CTriMesh_SetVert(self, nv, &pos);
	pos = b->pos;
	CTriMesh_SetVert(self, nv + 1, &pos);
	pos = c->pos;
	CTriMesh_SetVert(self, nv + 2, &pos);
	if (self->pTVert) {
		uv_t uv = a->uv;
		CTriMesh_SetTVert(self, ntv, &uv);
		uv = b->uv;
		CTriMesh_SetTVert(self, ntv + 1, &uv);
		uv = c->uv;
		CTriMesh_SetTVert(self, ntv + 2, &uv);
	}
	vec3_t n[3] = { a->n, b->n, c->n };
	CTriMesh_SetTriVerts(self, nt, nv, nv + 1, nv + 2);
	CTriMesh_SetTriTVerts(self, nt, ntv, ntv + 1, ntv + 2);
	CTriMesh_SetTriMat(self, nt, mat);
	CTriMesh_SetTriNormals(self, nt, &n[0], &n[1], &n[2]);
	CTriMesh_SetTriFlags(self, nt, flags);
}

void __fastcall CTriMesh_CreateTVert(CTriMesh *self) {
	if (!self->pTri || self->pTVert) {
		return;
	}
	CTriMesh_SetNumTVert(self, 1, 0);
	uv_t uv = { 0.0f, 0.0f };
	CTriMesh_SetTVert(self, 0, &uv);
	for (int i = 0; i < self->nTri; i++) {
		self->pTri[i].tv[0] = 0;
		self->pTri[i].tv[1] = 0;
		self->pTri[i].tv[2] = 0;
	}
}

/* the triangle accessor of MeshD3D.h */
static const uint16_t *MeshD3D_Tri(const CMeshD3D *m, uint32_t i) {
	CHECK(i < m->nTri);
	CHECK(m->pTri);
	return m->pTri[i];
}

void __thiscall CTriMesh_AddD3D(CTriMesh *self, CMeshD3D *src, byte all) {
	for (; src; src = src->next) {
		int nv = Mesh_NumVert(self);
		int ntv = Mesh_NumTVert(self);
		int nt = Mesh_NumTri(self);

		for (int i = 0; i < src->vert.nVert; i++) {
			int iv = CTriMesh_AddVert(self);
			int itv = CTriMesh_AddTVert(self);
			CHECK(iv == nv + i);
			CHECK(itv == ntv + i);
			vec3_t pos;
			uv_t uv;
			CVertList_GetPos(&src->vert, &pos, i);
			CVertList_GetUV(&src->vert, &uv, i);
			CTriMesh_SetVert(self, iv, &pos);
			CTriMesh_SetTVert(self, itv, &uv);
		}
		for (uint32_t i = 0; (int)i < (int)src->nTri; i++) {
			int it = CTriMesh_AddTri(self);
			CHECK(it == nt + (int)i);
			const uint16_t *idx = MeshD3D_Tri(src, i);
			vec3_t n[3];
			CVertList_GetNormal(&src->vert, &n[0], idx[0]);
			CVertList_GetNormal(&src->vert, &n[1], idx[1]);
			CVertList_GetNormal(&src->vert, &n[2], idx[2]);
			CTriMesh_SetTriVerts(self, it, idx[0] + nv, idx[1] + nv, idx[2] + nv);
			CTriMesh_SetTriTVerts(self, it, idx[0] + ntv, idx[1] + ntv, idx[2] + ntv);
			CTriMesh_SetTriMat(self, it, src->mat);
			CTriMesh_SetTriNormals(self, it, &n[0], &n[1], &n[2]);
			CTriMesh_SetTriFlags(self, it, src->triFlags ? src->triFlags[i] : 0);
		}
		if (!all) {
			break;
		}
	}
}

void __thiscall CTriMesh_Add(CTriMesh *self, const CTriMesh *orig, uint32_t mask, byte want) {
	int nv = Mesh_NumVert(self);
	int ntv = Mesh_NumTVert(self);
	int nt = Mesh_NumTri(self);

	if (!orig->pVert && !orig->pTri) {
		return;
	}
	CHECK(orig->pVert && orig->pTri);
	if (self->pTri || self->pVert) {
		CHECK(self->pTri && self->pVert);
		if (orig->pTVert && !self->pTVert) {
			CTriMesh_CreateTVert(self);
		}
	}
	if (orig->nVert && orig->pVert) {
		CTriMesh_SetNumVert(self, nv + orig->nVert, 1);
		memcpy(self->pVert + nv, orig->pVert, orig->nVert * sizeof(vec3_t));
	}
	if (orig->nTVert && orig->pTVert) {
		CTriMesh_SetNumTVert(self, orig->nTVert + ntv, 1);
		memcpy(self->pTVert + ntv, orig->pTVert, orig->nTVert * sizeof(uv_t));
	} else if (self->pTVert) {
		CTriMesh_SetNumTVert(self, ntv + 1, 1);
		uv_t uv = { 0.0f, 0.0f };
		CTriMesh_SetTVert(self, ntv, &uv);
	}
	if (orig->nTri && orig->pTri) {
		for (int i = 0; i < orig->nTri; i++) {
			const CMeshTri *t = CTriMesh_GetTri((CTriMesh *)orig, i);
			if (((t->flags & mask) == 0) == (int)want) {
				continue;
			}
			int it = CTriMesh_AddTri(self);
			CHECK(it <= nt + i);
			CTriMesh_SetTriVerts(self, it, t->v[0] + nv, t->v[1] + nv, t->v[2] + nv);
			CTriMesh_SetTriMat(self, it, t->mat);
			CTriMesh_SetTriNormals(self, it, &t->n[0], &t->n[1], &t->n[2]);
			CTriMesh_SetTriFlags(self, it, t->flags);
			if (orig->pTVert) {
				CTriMesh_SetTriTVerts(self, it, t->tv[0] + ntv, t->tv[1] + ntv, t->tv[2] + ntv);
			} else {
				CTriMesh_SetTriTVerts(self, it, ntv, ntv, ntv);
			}
		}
	}
	if (!self->pSkin && !orig->pSkin) {
		return;
	}
	CHECK((self->pSkin && orig->pSkin) || (nv == 0));
	CHECK(self->nVert == nv + orig->nVert);
	byte *skin = New_Array(self->nVert, TRIMESH_SKIN);
	/* one of the skins may be missing when there were no vertices: no copy of 0 bytes from NULL */
	if (nv) {
		memcpy(skin, self->pSkin, nv * TRIMESH_SKIN);
	}
	if (orig->nVert) {
		memcpy(skin + nv * TRIMESH_SKIN, orig->pSkin, orig->nVert * TRIMESH_SKIN);
	}
	self->pSkin = Mesh_Free(self->pSkin);
	self->pSkin = skin;
}

void __thiscall CTriMesh_CalcBox(CTriMesh *self, box_t *out, byte first) {
	CHECK(self->pVert);
	box_t *b = &self->box;
	b->min.x = b->min.y = b->min.z = TRIMESH_FLT_MAX;
	b->max.x = b->max.y = b->max.z = -TRIMESH_FLT_MAX;
	for (int i = 0; i < self->nVert; i++) {
		const vec3_t *v = &self->pVert[i];
		/* minss and maxss: the vertex when unordered */
		b->min.x = b->min.x < v->x ? b->min.x : v->x;
		b->min.y = b->min.y < v->y ? b->min.y : v->y;
		b->min.z = b->min.z < v->z ? b->min.z : v->z;
		b->max.x = b->max.x > v->x ? b->max.x : v->x;
		b->max.y = b->max.y > v->y ? b->max.y : v->y;
		b->max.z = b->max.z > v->z ? b->max.z : v->z;
	}
	/* an identical copy of Box_Merge, rva 0x2b8870 */
	Box_Merge(out, b, first);
}

void __thiscall CTriMesh_GetBox(CTriMesh *self, box_t *out, byte recalc) {
	if (recalc) {
		CTriMesh_CalcBox(self, out, 1);
		return;
	}
	*out = self->box;
}

void Mesh_Normalize(vec3_t *n) {
	float x = n->x, y = n->y, z = n->z;
	float len = (float)__builtin_sqrt((double)(x * x + y * y + z * z));
	if ((double)len == 0.0) {
		return;
	}
	float k = (float)(1.0 / (double)len);
	n->x = x * k;
	n->y = y * k;
	n->z = z * k;
}

void __thiscall CTriMesh_Transform(CTriMesh *self, const matrix_t *m) {
	if (!self->pVert && !self->pTri) {
		return;
	}
	CHECK(self->pVert && self->pTri);
	for (int i = 0; i < self->nVert; i++) {
		Vec3_TransformH(&self->pVert[i], &self->pVert[i], m);
	}
	for (int i = 0; i < self->nTri; i++) {
		CMeshTri *t = &self->pTri[i];
		for (int k = 0; k < 3; k++) {
			Vec3_TransformDir0(&t->n[k], &t->n[k], m);
		}
		for (int k = 0; k < 3; k++) {
			Mesh_Normalize(&t->n[k]);
		}
	}
}

void __thiscall CTriMesh_Draw(CTriMesh *self, void *dev, int unused) {
	(void)unused;
	/* the meshes draw through their D3D mesh, this path is not meant to run */
	CHECK(0);
	if (self->d3d && g_MeshD3DOn) {
		((meshD3DDraw_t)self->d3d->vftable[2])(self->d3d, dev, 0);
		return;
	}
	if (!self->pTri || !self->pVert) {
		return;
	}
	Render_SetWorld();
	int textured = self->pTVert != NULL;
	for (int i = 0; i < self->nTri; i++) {
		const CMeshTri *t = &self->pTri[i];
		CMeshVert v[3];
		Render_SetMaterial(t->mat, 0);
		for (int k = 0; k < 3; k++) {
			v[k].pos = self->pVert[t->v[k]];
			v[k].n = t->n[k];
			if (textured) {
				v[k].uv = self->pTVert[t->tv[k]];
			} else {
				v[k].uv.u = 0.0f;
				v[k].uv.v = 0.0f;
			}
		}
		D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, TRIMESH_FVF);
		D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLELIST, 1, v, sizeof(CMeshVert));
	}
	g_RenderTris += self->nTri;
	g_RenderVerts += self->nTri * 3;
}

/* ListaVert.h */
static void *List_Copy(void *out, const byte *v, int off, uint32_t size) {
	memcpy(out, v + off, size);
	return out;
}

vec3_t *__thiscall CVertList_GetNormal(CVertList *self, vec3_t *out, uint32_t i) {
	switch (self->frmt) {
	case VERT_TEX1:
		return List_Copy(out, CVertList_Tex1(self, i), 0xc, sizeof(*out));
	case VERT_TEX2:
		return List_Copy(out, CVertList_Tex2(self, i), 0xc, sizeof(*out));
	case VERT_TEX1_B1:
		return List_Copy(out, CVertList_Tex1B1(self, i), 0x10, sizeof(*out));
	case VERT_LTEX1N:
		return List_Copy(out, CVertList_LTex1N(self, i), 0xc, sizeof(*out));
	default:
		CHECK(0);
		out->z = 0.0f;
		out->y = 0.0f;
		out->x = 0.0f;
		return out;
	}
}

/* an inline of ListaVert.h, compiled into ScrapEngCollision.cpp */
vec3_t *__thiscall CVertList_GetPos(CVertList *self, vec3_t *out, uint32_t i) {
	switch (self->frmt) {
	case VERT_TEX1:
		return List_Copy(out, CVertList_Tex1(self, i), 0, sizeof(*out));
	case VERT_TEX2:
		return List_Copy(out, CVertList_Tex2(self, i), 0, sizeof(*out));
	case VERT_LTEX1:
		return List_Copy(out, CVertList_LTex1(self, i), 0, sizeof(*out));
	case VERT_TEX1_B1:
		return List_Copy(out, CVertList_Tex1B1(self, i), 0, sizeof(*out));
	case VERT_LTEX2N:
		return List_Copy(out, CVertList_LTex2N(self, i), 0, sizeof(*out));
	case VERT_LTEX3:
		return List_Copy(out, CVertList_LTex3(self, i), 0, sizeof(*out));
	case VERT_LTEX1N:
		return List_Copy(out, CVertList_LTex1N(self, i), 0, sizeof(*out));
	case VERT_LTEX2:
		return List_Copy(out, CVertList_LTex2(self, i), 0, sizeof(*out));
	default:
		break;
	}
	if (!self->pVert || (uint32_t)self->totVert <= i) {
		CheckFailed("pVert && (dword)i<(dword)totVert", "engine/ListaVert.h", __TIMESTAMP__, __LINE__);
	}
	switch (self->frmt) {
	case VERT_POS_XBOX: {
		float scale = self->scale;
		vec3_t n;
		VertList_UnpackNormal(&n, ((uint32_t *)self->pVert)[i]);
		out->x = n.x * scale;
		out->y = n.y * scale;
		out->z = n.z * scale;
		return out;
	}
	case VERT_TEX1_XBOX:
		return List_Copy(out, self->pVert + i * 0x18, 0, sizeof(*out));
	case VERT_TEX1C_XBOX:
		return List_Copy(out, self->pVert + i * 0x1c, 0, sizeof(*out));
	default:
		CheckFailed("0", "engine/ListaVert.h", __TIMESTAMP__, __LINE__);
		out->z = 0.0f;
		out->y = 0.0f;
		out->x = 0.0f;
		return out;
	}
}

uv_t *__thiscall CVertList_GetUV(CVertList *self, uv_t *out, uint32_t i) {
	switch (self->frmt) {
	case VERT_TEX1:
		return List_Copy(out, CVertList_Tex1(self, i), 0x18, sizeof(*out));
	case VERT_TEX2:
		return List_Copy(out, CVertList_Tex2(self, i), 0x18, sizeof(*out));
	case VERT_LTEX1:
		return List_Copy(out, CVertList_LTex1(self, i), 0x14, sizeof(*out));
	case VERT_TEX1_B1:
		return List_Copy(out, CVertList_Tex1B1(self, i), 0x1c, sizeof(*out));
	case VERT_LTEX3:
		return List_Copy(out, CVertList_LTex3(self, i), 0x1c, sizeof(*out));
	case VERT_LTEX1N:
		return List_Copy(out, CVertList_LTex1N(self, i), 0x1c, sizeof(*out));
	case VERT_LTEX2:
		return List_Copy(out, CVertList_LTex2(self, i), 0x10, sizeof(*out));
	default:
		CHECK(0);
		out->v = 0.0f;
		out->u = 0.0f;
		return out;
	}
}
