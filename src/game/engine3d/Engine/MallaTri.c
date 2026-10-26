/*
 * Triangle meshes of the engine3d, original source Engine/MallaTri.cpp (include/scrap/mallatri.h), with the
 * CListaVert accessors of ListaVert.h it instances.
 */
#include <string.h>

#include "scrap/mallatri.h"

#define CMALLATRI_VFTABLE 0x3dbcf0
#define IOBJETO3D_VFTABLE 0x3d79dc
#define MALLATRI_SKIN 16
/* the box before the first vertex, rva 0x3a3a38 and 0x3a512c */
#define MALLATRI_FLT_MAX 3.4028234663852886e+38f

/* draw the meshes through their D3D mesh */
#define g_MallaD3DOn GAME_DATA(byte, 0x431bad)
/* statistics of the frame */
#define g_TrisDrawn GAME_DATA(int, 0x4a8f00)
#define g_VertsDrawn GAME_DATA(int, 0x4a8f04)

/* IDirect3DDevice8: SetVertexShader with an FVF, DrawPrimitiveUP */
#define D3DDEV8_SETVERTEXSHADER (0x130 / 4)
#define D3DDEV8_DRAWPRIMITIVEUP (0x120 / 4)
#define D3DPT_TRIANGLELIST 4
typedef int(__stdcall *d3dSetVertexShader_t)(void *dev, uint32_t fvf);
typedef int(__stdcall *d3dDrawPrimitiveUP_t)(void *dev, int type, uint32_t count, const void *data, uint32_t stride);
typedef void(__thiscall *mallaD3DDelete_t)(CMallaD3D *self, int flags);
typedef void(__thiscall *mallaD3DDraw_t)(CMallaD3D *self, void *dev, int arg);

/* the counts as the add functions take them: 0 without the array */
static int Malla_NumVert(const CMallaTri *m) {
	return m->pVert ? m->nVert : 0;
}

static int Malla_NumTVert(const CMallaTri *m) {
	return m->pTVert ? m->nTVert : 0;
}

static int Malla_NumTri(const CMallaTri *m) {
	return m->pTri ? m->nTri : 0;
}

CTriMalla *__fastcall CTriMalla_ctor(CTriMalla *self) {
	self->mat = NULL;
	self->flags = 0;
	return self;
}

/* new[] of count items; the triangles get their constructor */
static void *Malla_New(uint32_t count, uint32_t size, int tris) {
	byte *p = New_Array(count, size);
	if (p && tris) {
		for (uint32_t i = 0; i < count; i++) {
			CTriMalla_ctor((CTriMalla *)(p + i * size));
		}
	}
	return p;
}

/* *num = n; the array is made with n items, or with 2n when n is more than *max */
static void Malla_Resize(void **p, int *num, int *max, int n, uint32_t size, int tris) {
	void *old = *p;
	if (!old) {
		*max = n;
		*p = Malla_New(n, size, tris);
	} else if (n > *max) {
		*max = n * 2;
		void *np = Malla_New((uint32_t)n * 2, size, tris);
		*p = np;
		memcpy(np, old, *num * size);
		operator_delete_array(old);
	}
	*num = n;
}

static void *Malla_Free(void *p) {
	if (p) {
		operator_delete_array(p);
	}
	return NULL;
}

void __fastcall CMallaTri_Init(CMallaTri *self) {
	self->clase = CLASE_OBJ3D_MALLATRI;
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

CMallaTri *__fastcall CMallaTri_ctor(CMallaTri *self) {
	self->vftable = Game_Ptr(CMALLATRI_VFTABLE);
	memset(&self->box, 0, sizeof(self->box));
	CMallaTri_Init(self);
	return self;
}

CMallaTri *__thiscall CMallaTri_ctorCopy(CMallaTri *self, const CMallaTri *orig) {
	CMallaTri_ctor(self);
	CMallaTri_Add(self, orig, 0, 0);
	return self;
}

CMallaTri *__thiscall CMallaTri_ctorD3D(CMallaTri *self, CMallaD3D *src, byte all) {
	CMallaTri_ctor(self);
	CMallaTri_AddD3D(self, src, all);
	return self;
}

void __fastcall CMallaTri_Clear(CMallaTri *self) {
	self->pVert = Malla_Free(self->pVert);
	self->pTVert = Malla_Free(self->pTVert);
	self->pTri = Malla_Free(self->pTri);
	if (self->d3d) {
		((mallaD3DDelete_t)self->d3d->vftable[0])(self->d3d, 1);
		self->d3d = NULL;
	}
	self->pSkin = Malla_Free(self->pSkin);
	self->nTri = 0;
	self->nTVert = 0;
	self->nVert = 0;
	self->maxTri = 0;
	self->maxTVert = 0;
	self->maxVert = 0;
	memset(&self->box, 0, sizeof(self->box));
}

void __fastcall CMallaTri_dtor(CMallaTri *self) {
	self->vftable = Game_Ptr(CMALLATRI_VFTABLE);
	CMallaTri_Clear(self);
	self->vftable = Game_Ptr(IOBJETO3D_VFTABLE);
}

CMallaTri *__thiscall CMallaTri_Delete(CMallaTri *self, byte flags) {
	CMallaTri_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte __thiscall CMallaTri_NoFile(CMallaTri *self, void *f, void *ctx) {
	(void)self;
	(void)f;
	(void)ctx;
	return 0;
}

void __thiscall CMallaTri_SetNumVert(CMallaTri *self, int n, byte realloc) {
	CHECK(n > 0);
	CHECK(realloc || !self->pVert);
	Malla_Resize((void **)&self->pVert, &self->nVert, &self->maxVert, n, sizeof(vec3_t), 0);
}

void __thiscall CMallaTri_SetNumTVert(CMallaTri *self, int n, byte realloc) {
	CHECK(n >= 0);
	CHECK(realloc || !self->pTVert);
	if (!n) {
		self->pTVert = Malla_Free(self->pTVert);
		return;
	}
	Malla_Resize((void **)&self->pTVert, &self->nTVert, &self->maxTVert, n, sizeof(uv_t), 0);
}

void __thiscall CMallaTri_SetNumTri(CMallaTri *self, int n, byte realloc) {
	CHECK(n > 0);
	CHECK(realloc || !self->pTri);
	Malla_Resize((void **)&self->pTri, &self->nTri, &self->maxTri, n, sizeof(CTriMalla), 1);
}

int __fastcall CMallaTri_AddVert(CMallaTri *self) {
	int n = Malla_NumVert(self);
	CMallaTri_SetNumVert(self, n + 1, 1);
	return n;
}

int __fastcall CMallaTri_AddTVert(CMallaTri *self) {
	int n = Malla_NumTVert(self);
	CMallaTri_SetNumTVert(self, n + 1, 1);
	return n;
}

int __fastcall CMallaTri_AddTri(CMallaTri *self) {
	int n = Malla_NumTri(self);
	CMallaTri_SetNumTri(self, n + 1, 1);
	return n;
}

void *__fastcall CMallaTri_AllocSkin(CMallaTri *self) {
	CHECK(self->pSkin == NULL);
	CHECK(self->nVert > 0);
	self->pSkin = New_Array(self->nVert, MALLATRI_SKIN);
	return self->pSkin;
}

void *__thiscall CMallaTri_GetSkin(CMallaTri *self, uint32_t i) {
	CHECK(i < (uint32_t)self->nVert);
	CHECK(self->pSkin);
	return self->pSkin + i * MALLATRI_SKIN;
}

void __thiscall CMallaTri_SetVert(CMallaTri *self, uint32_t i, const vec3_t *v) {
	CHECK(i < (uint32_t)self->nVert);
	CHECK(self->pVert);
	self->pVert[i] = *v;
}

vec3_t *__thiscall CMallaTri_GetVert(CMallaTri *self, vec3_t *out, uint32_t i) {
	CHECK(i < (uint32_t)self->nVert);
	CHECK(self->pVert);
	*out = self->pVert[i];
	return out;
}

void __thiscall CMallaTri_SetTVert(CMallaTri *self, uint32_t i, const uv_t *uv) {
	CHECK(i < (uint32_t)self->nTVert);
	CHECK(self->pTVert);
	self->pTVert[i] = *uv;
}

CTriMalla *__thiscall CMallaTri_GetTri(CMallaTri *self, uint32_t i) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	return &self->pTri[i];
}

void __thiscall CMallaTri_SetTriVerts(CMallaTri *self, uint32_t i, int a, int b, int c) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].v[0] = a;
	self->pTri[i].v[1] = b;
	self->pTri[i].v[2] = c;
}

void __thiscall CMallaTri_SetTriTVerts(CMallaTri *self, uint32_t i, int a, int b, int c) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].tv[0] = a;
	self->pTri[i].tv[1] = b;
	self->pTri[i].tv[2] = c;
}

void __thiscall CMallaTri_SetTriMat(CMallaTri *self, uint32_t i, void *mat) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].mat = mat;
}

void __thiscall CMallaTri_SetTriNormals(CMallaTri *self, uint32_t i, const vec3_t *a, const vec3_t *b, const vec3_t *c) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].n[0] = *a;
	self->pTri[i].n[1] = *b;
	self->pTri[i].n[2] = *c;
}

void __thiscall CMallaTri_SetTriFlags(CMallaTri *self, uint32_t i, uint32_t flags) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i].flags = flags;
}

void __thiscall CMallaTri_RemoveTri(CMallaTri *self, uint32_t i) {
	CHECK(i < (uint32_t)self->nTri);
	CHECK(self->pTri);
	self->pTri[i] = self->pTri[self->nTri - 1];
	self->nTri--;
}

void __thiscall CMallaTri_GetTriVert(CMallaTri *self, CVertMalla *out, uint32_t tri, uint32_t vert) {
	CHECK(tri < (uint32_t)self->nTri);
	CHECK(vert < 3);
	CHECK(self->pTri && self->pVert);
	const CTriMalla *t = &self->pTri[tri];
	uv_t uv = { 0.0f, 0.0f };
	if (self->pTVert) {
		uv = self->pTVert[t->tv[vert]];
	}
	out->pos = self->pVert[t->v[vert]];
	out->n = t->n[vert];
	out->uv = uv;
}

void __thiscall CMallaTri_AddTriVerts(CMallaTri *self, const CVertMalla *a, const CVertMalla *b, const CVertMalla *c,
	void *mat, byte tvert, uint32_t flags) {
	int nv = Malla_NumVert(self);
	int ntv = Malla_NumTVert(self);
	int nt = Malla_NumTri(self);

	if (tvert) {
		CHECK(self->pTVert || !self->pTri);
		CMallaTri_SetNumTVert(self, ntv + 3, 1);
	} else {
		CHECK(!self->pTVert);
	}
	CMallaTri_SetNumVert(self, nv + 3, 1);
	CMallaTri_SetNumTri(self, nt + 1, 1);
	vec3_t pos = a->pos;
	CMallaTri_SetVert(self, nv, &pos);
	pos = b->pos;
	CMallaTri_SetVert(self, nv + 1, &pos);
	pos = c->pos;
	CMallaTri_SetVert(self, nv + 2, &pos);
	if (self->pTVert) {
		uv_t uv = a->uv;
		CMallaTri_SetTVert(self, ntv, &uv);
		uv = b->uv;
		CMallaTri_SetTVert(self, ntv + 1, &uv);
		uv = c->uv;
		CMallaTri_SetTVert(self, ntv + 2, &uv);
	}
	vec3_t n[3] = { a->n, b->n, c->n };
	CMallaTri_SetTriVerts(self, nt, nv, nv + 1, nv + 2);
	CMallaTri_SetTriTVerts(self, nt, ntv, ntv + 1, ntv + 2);
	CMallaTri_SetTriMat(self, nt, mat);
	CMallaTri_SetTriNormals(self, nt, &n[0], &n[1], &n[2]);
	CMallaTri_SetTriFlags(self, nt, flags);
}

void __fastcall CMallaTri_CreateTVert(CMallaTri *self) {
	if (!self->pTri || self->pTVert) {
		return;
	}
	CMallaTri_SetNumTVert(self, 1, 0);
	uv_t uv = { 0.0f, 0.0f };
	CMallaTri_SetTVert(self, 0, &uv);
	for (int i = 0; i < self->nTri; i++) {
		self->pTri[i].tv[0] = 0;
		self->pTri[i].tv[1] = 0;
		self->pTri[i].tv[2] = 0;
	}
}

/* the triangle accessor of MallaD3D.h */
static const uint16_t *MallaD3D_Tri(const CMallaD3D *m, uint32_t i) {
	CHECK(i < m->nTri);
	CHECK(m->pTri);
	return m->pTri[i];
}

void __thiscall CMallaTri_AddD3D(CMallaTri *self, CMallaD3D *src, byte all) {
	for (; src; src = src->next) {
		int nv = Malla_NumVert(self);
		int ntv = Malla_NumTVert(self);
		int nt = Malla_NumTri(self);

		for (int i = 0; i < src->vert.nVert; i++) {
			int iv = CMallaTri_AddVert(self);
			int itv = CMallaTri_AddTVert(self);
			CHECK(iv == nv + i);
			CHECK(itv == ntv + i);
			vec3_t pos;
			uv_t uv;
			CListaVert_GetPos(&src->vert, &pos, i);
			CListaVert_GetUV(&src->vert, &uv, i);
			CMallaTri_SetVert(self, iv, &pos);
			CMallaTri_SetTVert(self, itv, &uv);
		}
		for (uint32_t i = 0; (int)i < (int)src->nTri; i++) {
			int it = CMallaTri_AddTri(self);
			CHECK(it == nt + (int)i);
			const uint16_t *idx = MallaD3D_Tri(src, i);
			vec3_t n[3];
			CListaVert_GetNormal(&src->vert, &n[0], idx[0]);
			CListaVert_GetNormal(&src->vert, &n[1], idx[1]);
			CListaVert_GetNormal(&src->vert, &n[2], idx[2]);
			CMallaTri_SetTriVerts(self, it, idx[0] + nv, idx[1] + nv, idx[2] + nv);
			CMallaTri_SetTriTVerts(self, it, idx[0] + ntv, idx[1] + ntv, idx[2] + ntv);
			CMallaTri_SetTriMat(self, it, src->mat);
			CMallaTri_SetTriNormals(self, it, &n[0], &n[1], &n[2]);
			CMallaTri_SetTriFlags(self, it, src->triFlags ? src->triFlags[i] : 0);
		}
		if (!all) {
			break;
		}
	}
}

void __thiscall CMallaTri_Add(CMallaTri *self, const CMallaTri *orig, uint32_t mask, byte want) {
	int nv = Malla_NumVert(self);
	int ntv = Malla_NumTVert(self);
	int nt = Malla_NumTri(self);

	if (!orig->pVert && !orig->pTri) {
		return;
	}
	CHECK(orig->pVert && orig->pTri);
	if (self->pTri || self->pVert) {
		CHECK(self->pTri && self->pVert);
		if (orig->pTVert && !self->pTVert) {
			CMallaTri_CreateTVert(self);
		}
	}
	if (orig->nVert && orig->pVert) {
		CMallaTri_SetNumVert(self, nv + orig->nVert, 1);
		memcpy(self->pVert + nv, orig->pVert, orig->nVert * sizeof(vec3_t));
	}
	if (orig->nTVert && orig->pTVert) {
		CMallaTri_SetNumTVert(self, orig->nTVert + ntv, 1);
		memcpy(self->pTVert + ntv, orig->pTVert, orig->nTVert * sizeof(uv_t));
	} else if (self->pTVert) {
		CMallaTri_SetNumTVert(self, ntv + 1, 1);
		uv_t uv = { 0.0f, 0.0f };
		CMallaTri_SetTVert(self, ntv, &uv);
	}
	if (orig->nTri && orig->pTri) {
		for (int i = 0; i < orig->nTri; i++) {
			const CTriMalla *t = CMallaTri_GetTri((CMallaTri *)orig, i);
			if (((t->flags & mask) == 0) == (int)want) {
				continue;
			}
			int it = CMallaTri_AddTri(self);
			CHECK(it <= nt + i);
			CMallaTri_SetTriVerts(self, it, t->v[0] + nv, t->v[1] + nv, t->v[2] + nv);
			CMallaTri_SetTriMat(self, it, t->mat);
			CMallaTri_SetTriNormals(self, it, &t->n[0], &t->n[1], &t->n[2]);
			CMallaTri_SetTriFlags(self, it, t->flags);
			if (orig->pTVert) {
				CMallaTri_SetTriTVerts(self, it, t->tv[0] + ntv, t->tv[1] + ntv, t->tv[2] + ntv);
			} else {
				CMallaTri_SetTriTVerts(self, it, ntv, ntv, ntv);
			}
		}
	}
	if (!self->pSkin && !orig->pSkin) {
		return;
	}
	CHECK((self->pSkin && orig->pSkin) || (nv == 0));
	CHECK(self->nVert == nv + orig->nVert);
	byte *skin = New_Array(self->nVert, MALLATRI_SKIN);
	/* one of the skins may be missing when there were no vertices: no copy of 0 bytes from NULL */
	if (nv) {
		memcpy(skin, self->pSkin, nv * MALLATRI_SKIN);
	}
	if (orig->nVert) {
		memcpy(skin + nv * MALLATRI_SKIN, orig->pSkin, orig->nVert * MALLATRI_SKIN);
	}
	self->pSkin = Malla_Free(self->pSkin);
	self->pSkin = skin;
}

void __thiscall CMallaTri_CalcBox(CMallaTri *self, box_t *out, byte first) {
	CHECK(self->pVert);
	box_t *b = &self->box;
	b->min.x = b->min.y = b->min.z = MALLATRI_FLT_MAX;
	b->max.x = b->max.y = b->max.z = -MALLATRI_FLT_MAX;
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

void __thiscall CMallaTri_GetBox(CMallaTri *self, box_t *out, byte recalc) {
	if (recalc) {
		CMallaTri_CalcBox(self, out, 1);
		return;
	}
	*out = self->box;
}

/* normalized through 1.0 / length in double; a zero length leaves it */
static void Malla_Normalize(vec3_t *n) {
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

void __thiscall CMallaTri_Transform(CMallaTri *self, const matrix_t *m) {
	if (!self->pVert && !self->pTri) {
		return;
	}
	CHECK(self->pVert && self->pTri);
	for (int i = 0; i < self->nVert; i++) {
		Vec3_TransformH(&self->pVert[i], &self->pVert[i], m);
	}
	for (int i = 0; i < self->nTri; i++) {
		CTriMalla *t = &self->pTri[i];
		for (int k = 0; k < 3; k++) {
			Vec3_TransformDir0(&t->n[k], &t->n[k], m);
		}
		for (int k = 0; k < 3; k++) {
			Malla_Normalize(&t->n[k]);
		}
	}
}

void __thiscall CMallaTri_Draw(CMallaTri *self, void *dev, int unused) {
	(void)unused;
	/* the meshes draw through their D3D mesh, this path is not meant to run */
	CHECK(0);
	if (self->d3d && g_MallaD3DOn) {
		((mallaD3DDraw_t)self->d3d->vftable[2])(self->d3d, dev, 0);
		return;
	}
	if (!self->pTri || !self->pVert) {
		return;
	}
	Render_SetWorld();
	int textured = self->pTVert != NULL;
	for (int i = 0; i < self->nTri; i++) {
		const CTriMalla *t = &self->pTri[i];
		CVertMalla v[3];
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
		void **vt = *(void ***)dev;
		((d3dSetVertexShader_t)vt[D3DDEV8_SETVERTEXSHADER])(dev, MALLATRI_FVF);
		vt = *(void ***)dev;
		((d3dDrawPrimitiveUP_t)vt[D3DDEV8_DRAWPRIMITIVEUP])(dev, D3DPT_TRIANGLELIST, 1, v, sizeof(CVertMalla));
	}
	g_TrisDrawn += self->nTri;
	g_VertsDrawn += self->nTri * 3;
}

/* ListaVert.h */
static void *Lista_Copy(void *out, const byte *v, int off, uint32_t size) {
	memcpy(out, v + off, size);
	return out;
}

vec3_t *__thiscall CListaVert_GetNormal(CListaVert *self, vec3_t *out, uint32_t i) {
	switch (self->frmt) {
	case VERT_TEX1:
		return Lista_Copy(out, CListaVert_Tex1(self, i), 0xc, sizeof(*out));
	case VERT_TEX2:
		return Lista_Copy(out, CListaVert_Tex2(self, i), 0xc, sizeof(*out));
	case VERT_TEX1_B1:
		return Lista_Copy(out, CListaVert_Tex1B1(self, i), 0x10, sizeof(*out));
	case VERT_LTEX1N:
		return Lista_Copy(out, CListaVert_LTex1N(self, i), 0xc, sizeof(*out));
	default:
		CHECK(0);
		out->z = 0.0f;
		out->y = 0.0f;
		out->x = 0.0f;
		return out;
	}
}

uv_t *__thiscall CListaVert_GetUV(CListaVert *self, uv_t *out, uint32_t i) {
	switch (self->frmt) {
	case VERT_TEX1:
		return Lista_Copy(out, CListaVert_Tex1(self, i), 0x18, sizeof(*out));
	case VERT_TEX2:
		return Lista_Copy(out, CListaVert_Tex2(self, i), 0x18, sizeof(*out));
	case VERT_LTEX1:
		return Lista_Copy(out, CListaVert_LTex1(self, i), 0x14, sizeof(*out));
	case VERT_TEX1_B1:
		return Lista_Copy(out, CListaVert_Tex1B1(self, i), 0x1c, sizeof(*out));
	case VERT_LTEX3:
		return Lista_Copy(out, CListaVert_LTex3(self, i), 0x1c, sizeof(*out));
	case VERT_LTEX1N:
		return Lista_Copy(out, CListaVert_LTex1N(self, i), 0x1c, sizeof(*out));
	case VERT_LTEX2:
		return Lista_Copy(out, CListaVert_LTex2(self, i), 0x10, sizeof(*out));
	default:
		CHECK(0);
		out->v = 0.0f;
		out->u = 0.0f;
		return out;
	}
}
