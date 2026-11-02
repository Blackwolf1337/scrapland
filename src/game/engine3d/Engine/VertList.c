/*
 * Vertex lists of the engine3d, original source Engine/ListaVert.cpp (include/scrap/vertlist.h).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/vertlist.h"
#include "scrap/light.h"
#include "scrap/material.h"
#include "scrap/render.h"

#define VERTLIST_ID "LFVF"
#define VERTLIST_VER 1
#define FLT_MAX_F 3.4028234663852886e+38f
/* the 15 colors of the strips */
extern uint32_t g_BspColors[16];
#define g_StripColors ((&g_BspColors[1]))
#define STRIP_COLORS 15
#define TRI_USED 0x8000
#define TRI_FREE 0xffff

CVertList *CVertList_ctor(CVertList *self) {
	self->pVert = NULL;
	self->totVert = 0;
	self->frmt = 0;
	self->nVert = 0;
	self->scale = 0.0f;
	return self;
}

void CVertList_Free(CVertList *self) {
	if (self->pVert) {
		Mem_Free(self->pVert);
		self->pVert = NULL;
	}
	self->frmt = 0;
	self->totVert = 0;
	self->nVert = 0;
}

void CVertList_SetFormat(CVertList *self, int frmt) {
	CHECK(frmt >= VERT_NULL && frmt < VERT_TOTAL);
	if (self->pVert) {
		Mem_Free(self->pVert);
		self->pVert = NULL;
	}
	self->frmt = frmt;
	self->totVert = 0;
	self->nVert = 0;
}

void CVertList_Alloc(CVertList *self, int n, byte realloc) {
	CHECK(n > 0);
	CHECK(realloc || !self->pVert);
	self->nVert = n;
	if (self->pVert) {
		n = (n >> 2) + n;
	}
	self->totVert = n;
	self->pVert = Mem_Realloc(self->pVert, self->totVert * VertList_FormatSize(self->frmt));
}

void CVertList_Swap(CVertList *self, CVertList *other) {
	CVertList t = *other;

	other->pVert = self->pVert;
	other->nVert = self->nVert;
	other->totVert = self->totVert;
	other->frmt = self->frmt;
	self->nVert = t.nVert;
	self->totVert = t.totVert;
	self->pVert = t.pVert;
	self->frmt = t.frmt;
}

void CVertList_Read(CVertList *self, CFileIO *f) {
	CHECK(CFileIO_BeginBlock(f, VERTLIST_ID) == 0);
	CHECK(CFileIO_ReadInt(f) == VERTLIST_VER);
	CVertList_SetFormat(self, CFileIO_ReadInt(f));
	if (self->frmt) {
		uint32_t fvf = VertList_FormatFVF(self->frmt);
		CHECK((uint32_t)CFileIO_ReadInt(f) == fvf);
		int size = VertList_FormatSize(self->frmt);
		CHECK(CFileIO_ReadInt(f) == size);
		CVertList_Alloc(self, CFileIO_ReadInt(f), 0);
		CFileIO_Read(f, self->pVert, self->nVert * VertList_FormatSize(self->frmt));
	}
	CFileIO_Error(f, VERTLIST_ID);
	CHECK(!f->error);
}

void CVertList_Write(CVertList *self, CFileIO *f) {
	CFileIO_BeginBlockWrite(f, VERTLIST_ID);
	CFileIO_WriteInt(f, VERTLIST_VER);
	if (self->nVert) {
		CHECK(self->pVert);
		CHECK(self->frmt != VERT_NULL);
		CFileIO_WriteInt(f, self->frmt);
		CFileIO_WriteInt(f, VertList_FormatFVF(self->frmt));
		CFileIO_WriteInt(f, VertList_FormatSize(self->frmt));
		CFileIO_WriteInt(f, self->nVert);
		CFileIO_Write(f, self->pVert, self->nVert * VertList_FormatSize(self->frmt));
	} else {
		CFileIO_WriteInt(f, 0);
	}
	CFileIO_EndBlockWrite(f, VERTLIST_ID);
	CHECK(!f->error);
}

/* the vertex shader handles of the XBOX formats */
static uint32_t g_hVShader_Tex1_XBox;
static uint32_t g_hVShader_Tex1C_XBox;
static uint32_t g_hVShader_LTex1_XBox;
static uint32_t g_hVShader_LTex1N_XBox;
static uint32_t g_hVShader_LTex2_XBox;
static uint32_t g_hVShader_LTex2N_XBox;

uint32_t VertList_FormatFVF(int frmt) {
	switch (frmt) {
	case VERT_TEX1:
		return 0x112;
	case VERT_TEX2:
		return 0x212;
	case VERT_LTEX1:
		return 0x1c2;
	case VERT_TEX1_B1:
		return 0x116;
	case VERT_LTEX2N:
		return 0x252;
	case VERT_LTEX3:
		return 0x352;
	case VERT_LTEX1N:
		return 0x152;
	case VERT_TLTEX1:
		return 0x1c4;
	case VERT_LTEX2:
		return 0x242;
	/* the XBOX formats name their vertex shader */
	case VERT_TEX1_XBOX:
		CHECK(g_hVShader_Tex1_XBox);
		return g_hVShader_Tex1_XBox;
	case VERT_TEX1C_XBOX:
		CHECK(g_hVShader_Tex1C_XBox);
		return g_hVShader_Tex1C_XBox;
	case VERT_LTEX1_XBOX:
		CHECK(g_hVShader_LTex1_XBox);
		return g_hVShader_LTex1_XBox;
	case VERT_LTEX1N_XBOX:
		CHECK(g_hVShader_LTex1N_XBox);
		return g_hVShader_LTex1N_XBox;
	case VERT_LTEX2_XBOX:
		CHECK(g_hVShader_LTex2_XBox);
		return g_hVShader_LTex2_XBox;
	case VERT_LTEX2N_XBOX:
		CHECK(g_hVShader_LTex2N_XBox);
		return g_hVShader_LTex2N_XBox;
	}
	CHECK(0);
	return 0;
}

int VertList_FormatSize(int frmt) {
	switch (frmt) {
	case VERT_TEX1:
	case VERT_TLTEX1:
	case VERT_LTEX2:
		return 0x20;
	case VERT_TEX2:
		return 0x28;
	case VERT_LTEX1:
	case VERT_TEX1C_XBOX:
		return 0x1c;
	case VERT_TEX1_B1:
	case VERT_LTEX1N:
		return 0x24;
	case VERT_LTEX2N:
		return 0x2c;
	case VERT_LTEX3:
		return 0x34;
	case VERT_POS_XBOX:
		return 4;
	case VERT_TEX1_XBOX:
		return 0x18;
	case VERT_LTEX1_XBOX:
		return 0x12;
	case VERT_LTEX1N_XBOX:
	case VERT_LTEX2_XBOX:
		return 0x16;
	case VERT_LTEX2N_XBOX:
		return 0x1a;
	}
	CHECK(0);
	return 0;
}

const char *VertList_FormatName(int frmt) {
	switch (frmt) {
	case VERT_TEX1:
		return "TEX1";
	case VERT_TEX2:
		return "TEX2";
	case VERT_LTEX1:
		return "LTEX1";
	case VERT_TEX1_B1:
		return "TEX1B1";
	case VERT_LTEX2N:
		return "LTEX2N";
	case VERT_LTEX3:
		return "LTEX3";
	case VERT_LTEX1N:
		return "LTEX1N";
	case VERT_TLTEX1:
		return "TLTEX1";
	case VERT_LTEX2:
		return "LTEX2";
	}
	CHECK(0);
	return NULL;
}

uint32_t CVertList_FVF(CVertList *self) {
	return VertList_FormatFVF(self->frmt);
}

int CVertList_Size(CVertList *self) {
	return VertList_FormatSize(self->frmt);
}

const char *CVertList_FormatName(CVertList *self) {
	return VertList_FormatName(self->frmt);
}

vec3_t *VertList_UnpackNormal(vec3_t *out, uint32_t packed) {
	out->x = (float)((int32_t)(packed << 21) >> 21) / 1023.0f;
	out->y = (float)((int32_t)(packed << 10) >> 21) / 1023.0f;
	out->z = (float)((int32_t)packed >> 22) / 511.0f;
	return out;
}

/* the field getters of one vertex */
static void *VertTex1_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_NORMAL:
		return v + 0xc;
	case LVERT_TEX2D_SET1:
		return v + 0x18;
	}
	return NULL;
}

static void *VertTex2_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_NORMAL:
		return v + 0xc;
	case LVERT_TEX2D_SET1:
		return v + 0x18;
	case LVERT_TEX2D_SET1 + 1:
		return v + 0x20;
	}
	return NULL;
}

static void *VertLTex1_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_DIFFUSE:
		return v + 0xc;
	case LVERT_SPECULAR:
		return v + 0x10;
	case LVERT_TEX2D_SET1:
		return v + 0x14;
	}
	return NULL;
}

static void *VertLTex2N_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_NORMAL:
		return v + 0xc;
	case LVERT_DIFFUSE:
		return v + 0x18;
	case LVERT_TEX2D_SET1:
		return v + 0x1c;
	case LVERT_TEX2D_SET1 + 1:
		return v + 0x24;
	}
	return NULL;
}

static void *VertLTex3_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_NORMAL:
		return v + 0xc;
	case LVERT_DIFFUSE:
		return v + 0x18;
	case LVERT_TEX2D_SET1:
		return v + 0x1c;
	case LVERT_TEX2D_SET1 + 1:
		return v + 0x24;
	case LVERT_TEX2D_SET1 + 2:
		return v + 0x2c;
	}
	return NULL;
}

static void *VertLTex1N_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_NORMAL:
		return v + 0xc;
	case LVERT_DIFFUSE:
		return v + 0x18;
	case LVERT_TEX2D_SET1:
		return v + 0x1c;
	}
	return NULL;
}

/* no LVERT_POS: the transformed position has 4 floats */
static void *VertTLTex1_Field(byte *v, int f) {
	switch (f) {
	case LVERT_DIFFUSE:
		return v + 0x10;
	case LVERT_SPECULAR:
		return v + 0x14;
	case LVERT_TEX2D_SET1:
		return v + 0x18;
	}
	return NULL;
}

static void *VertLTex2_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_DIFFUSE:
		return v + 0xc;
	case LVERT_TEX2D_SET1:
		return v + 0x10;
	case LVERT_TEX2D_SET1 + 1:
		return v + 0x18;
	}
	return NULL;
}

static void *VertTex1XBox_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_TEX2D_SET1:
		return v + 0x10;
	case LVERT_NORMAL_PACKED:
		return v + 0xc;
	}
	return NULL;
}

static void *VertTex1CXBox_Field(byte *v, int f) {
	switch (f) {
	case LVERT_POS:
		return v;
	case LVERT_DIFFUSE:
		return v + 0x10;
	case LVERT_TEX2D_SET1:
		return v + 0x14;
	case LVERT_NORMAL_PACKED:
		return v + 0xc;
	}
	return NULL;
}

static void *VertLTex1XBox_Field(byte *v, int f) {
	switch (f) {
	case LVERT_DIFFUSE:
		return v + 6;
	case LVERT_TEX2D_SET1:
		return v + 10;
	case LVERT_POS_SHORT:
		return v;
	}
	return NULL;
}

static void *VertLTex1NXBox_Field(byte *v, int f) {
	switch (f) {
	case LVERT_DIFFUSE:
		return v + 10;
	case LVERT_TEX2D_SET1:
		return v + 0xe;
	case LVERT_POS_SHORT:
		return v;
	case LVERT_NORMAL_PACKED:
		return v + 6;
	}
	return NULL;
}

static void *VertLTex2XBox_Field(byte *v, int f) {
	switch (f) {
	case LVERT_DIFFUSE:
		return v + 6;
	case LVERT_TEX2D_SET1:
		return v + 10;
	case LVERT_POS_SHORT:
		return v;
	case LVERT_TEX_SHORT2:
		return v + 0x12;
	}
	return NULL;
}

static void *VertLTex2NXBox_Field(byte *v, int f) {
	switch (f) {
	case LVERT_DIFFUSE:
		return v + 10;
	case LVERT_TEX2D_SET1:
		return v + 0xe;
	case LVERT_POS_SHORT:
		return v;
	case LVERT_NORMAL_PACKED:
		return v + 6;
	case LVERT_TEX_SHORT2:
		return v + 0x16;
	}
	return NULL;
}

/* vertex i of a list that must be in frmt */
static byte *VertList_Vert(CVertList *self, uint32_t i, int frmt, int size) {
	CHECK(self->pVert && i < (uint32_t)self->totVert);
	CHECK(self->frmt == frmt);
	return self->pVert + i * size;
}

byte CVertList_Optimize(CVertList *self, void *tris, int nTri) {
	(void)self;
	(void)tris;
	(void)nTri;
	return 0;
}

byte *CVertList_Tex1(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_TEX1, 0x20);
}

byte *CVertList_Tex2(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_TEX2, 0x28);
}

byte *CVertList_LTex1(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX1, 0x1c);
}

byte *CVertList_Tex1B1(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_TEX1_B1, 0x24);
}

byte *CVertList_LTex2N(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX2N, 0x2c);
}

byte *CVertList_LTex3(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX3, 0x34);
}

byte *CVertList_LTex1N(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX1N, 0x24);
}

static byte *CVertList_TLTex1(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_TLTEX1, 0x20);
}

byte *CVertList_LTex2(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX2, 0x20);
}

void CVertList_SetTex1(CVertList *self, uint32_t i, const void *v) {
	memcpy(VertList_Vert(self, i, VERT_TEX1, 0x20), v, 0x20);
}

void CVertList_SetTex2(CVertList *self, uint32_t i, const void *v) {
	memcpy(VertList_Vert(self, i, VERT_TEX2, 0x28), v, 0x28);
}

void CVertList_SetTex1B1(CVertList *self, uint32_t i, const void *v) {
	memcpy(VertList_Vert(self, i, VERT_TEX1_B1, 0x24), v, 0x24);
}

void *CVertList_GetField(CVertList *self, int f, uint32_t i) {
	switch (self->frmt) {
	case VERT_TEX1:
		return VertTex1_Field(CVertList_Tex1(self, i), f);
	case VERT_TEX2:
		return VertTex2_Field(CVertList_Tex2(self, i), f);
	case VERT_LTEX1:
		return VertLTex1_Field(CVertList_LTex1(self, i), f);
	case VERT_LTEX2N:
		return VertLTex2N_Field(CVertList_LTex2N(self, i), f);
	case VERT_LTEX3:
		return VertLTex3_Field(CVertList_LTex3(self, i), f);
	case VERT_LTEX1N:
		return VertLTex1N_Field(CVertList_LTex1N(self, i), f);
	case VERT_TLTEX1:
		return VertTLTex1_Field(CVertList_TLTex1(self, i), f);
	case VERT_LTEX2:
		return VertLTex2_Field(CVertList_LTex2(self, i), f);
	}
	CHECK(self->pVert && i < (uint32_t)self->totVert);
	switch (self->frmt) {
	case VERT_POS_XBOX:
		return NULL;
	case VERT_TEX1_XBOX:
		return VertTex1XBox_Field(self->pVert + i * 0x18, f);
	case VERT_TEX1C_XBOX:
		return VertTex1CXBox_Field(self->pVert + i * 0x1c, f);
	case VERT_LTEX1_XBOX:
		return VertLTex1XBox_Field(self->pVert + i * 0x12, f);
	case VERT_LTEX1N_XBOX:
		return VertLTex1NXBox_Field(self->pVert + i * 0x16, f);
	case VERT_LTEX2_XBOX:
		return VertLTex2XBox_Field(self->pVert + i * 0x16, f);
	case VERT_LTEX2N_XBOX:
		return VertLTex2NXBox_Field(self->pVert + i * 0x1a, f);
	}
	CHECK(0);
	return NULL;
}

void CVertList_SetColor(CVertList *self, uint32_t and, uint32_t or) {
	uint32_t *c = CVertList_GetField(self, LVERT_DIFFUSE, 0);
	int size = VertList_FormatSize(self->frmt);

	CHECK(c);
	for (int i = 0; i < self->nVert; i++) {
		*c = (*c & and) | or;
		c = (uint32_t *)((byte *)c + size);
	}
}

static void VertList_BoxInit(box_t *b) {
	b->min.x = FLT_MAX_F;
	b->min.y = FLT_MAX_F;
	b->min.z = FLT_MAX_F;
	b->max.x = -FLT_MAX_F;
	b->max.y = -FLT_MAX_F;
	b->max.z = -FLT_MAX_F;
}

static void VertList_BoxAdd(box_t *b, float x, float y, float z) {
	b->min.x = Float_MinSS(b->min.x, x);
	b->min.y = Float_MinSS(b->min.y, y);
	b->min.z = Float_MinSS(b->min.z, z);
	b->max.x = Float_MaxSS(b->max.x, x);
	b->max.y = Float_MaxSS(b->max.y, y);
	b->max.z = Float_MaxSS(b->max.z, z);
}

void CVertList_GetExtents(CVertList *self, box_t *ext, byte reset) {
	CHECK(self->pVert);
	CHECK(ext);
	if (reset) {
		VertList_BoxInit(ext);
	}
	for (int i = 0; i < self->nVert; i++) {
		vec3_t p;
		CVertList_GetPos(self, &p, i);
		VertList_BoxAdd(ext, p.x, p.y, p.z);
	}
}

void CVertList_Transform(CVertList *self, const matrix_t *m, byte normalize) {
	byte *v = NULL;
	int nOff = 0;

	CHECK(self->pVert);
	switch (self->frmt) {
	case VERT_TEX1:
	case VERT_TEX2:
		v = self->pVert;
		nOff = 0xc;
		break;
	case VERT_TEX1_B1:
		v = self->pVert;
		nOff = 0x10;
		break;
	default:
		CHECK(0);
		break;
	}
	int size = VertList_FormatSize(self->frmt);
	for (int i = 0; i < self->nVert; i++, v += size) {
		vec3_t *p = (vec3_t *)v;
		vec3_t *n = (vec3_t *)(v + nOff);
		Vec3_TransformH(p, p, m);
		Vec3_TransformDir0(n, n, m);
		if (!normalize) {
			continue;
		}
		float x = n->x, y = n->y, z = n->z;
		float len = (float)__builtin_sqrt((double)(x * x + y * y + z * z));
		if ((double)len != 0.0) {
			float k = (float)(1.0 / (double)len);
			x = x * k;
			y = y * k;
			z = z * k;
		}
		n->x = x;
		n->y = y;
		n->z = z;
	}
}

void CVertList_EnvMapUV(CVertList *self, const matrix_t *world, const matrix_t *view,
	const CMap *map, int itex) {
	matrix_t wv, inv;

	CHECK(self->pVert);
	CHECK(map);
	Matrix_MulTo(&wv, world, view);
	CHECK(Matrix_Invert(&inv, view) == 0);
	float u0 = wv.m[0], u1 = wv.m[4], u2 = wv.m[8];
	float v0 = wv.m[1], v1 = wv.m[5], v2 = wv.m[9];
	float offU = map->uvOffset[0], offV = map->uvOffset[1];
	float scaleU = map->uvScale[0], scaleV = map->uvScale[1];
	CHECK(itex >= 0);
	CHECK(LVERT_TEX2D_SET1 + itex <= LVERT_TEX2D_SET1 + 7);
	byte *pv = CVertList_GetField(self, LVERT_POS, 0);
	byte *pn = CVertList_GetField(self, LVERT_NORMAL, 0);
	byte *pt = CVertList_GetField(self, LVERT_TEX2D_SET1 + itex, 0);
	int stride = VertList_FormatSize(self->frmt);
	CHECK(pv && pn && pt);
	if (map->env2 == 1) {
		for (int i = 0; i < self->nVert; i++) {
			vec3_t p, n, r;
			Vec3_TransformH(&p, (const vec3_t *)pv, world);
			/* the direction to the camera, normalized inline */
			float ex = inv.m[12] - p.x;
			float ey = inv.m[13] - p.y;
			float ez = inv.m[14] - p.z;
			float len = (float)__builtin_sqrt((double)(ey * ey + ex * ex + ez * ez));
			if ((double)len != 0.0) {
				float k = (float)(1.0 / (double)len);
				ex = ex * k;
				ey = ey * k;
				ez = ez * k;
			}
			Vec3_TransformDir0(&n, (const vec3_t *)pn, world);
			float d = n.y * ey + n.x * ex + n.z * ez;
			d = d + d;
			r.x = n.x * d - ex;
			r.y = n.y * d - ey;
			r.z = n.z * d - ez;
			Vec3_TransformDir0(&r, &r, view);
			((float *)pt)[0] = ((r.x + 1.0f) * 0.5f + offU) * scaleU;
			((float *)pt)[1] = ((r.y + 1.0f) * 0.5f + offV) * scaleV;
			pv += stride;
			pn += stride;
			pt += stride;
		}
		return;
	}
	for (int i = 0; i < self->nVert; i++) {
		const float *nv = (const float *)pn;
		float x = nv[0], y = nv[1], z = nv[2];
		((float *)pt)[0] = ((y * u1 + x * u0 + z * u2 + 1.0f) * 0.5f + offU) * scaleU;
		((float *)pt)[1] = ((1.0f - (y * v1 + x * v0 + z * v2)) * 0.5f + offV) * scaleV;
		pn += stride;
		pt += stride;
	}
}

/* the D3D resources of the vertex lists: nothing to make or free (the buffers belong to the meshes) */
void VertList_ReleaseD3D(void) {
}

void VertList_InitD3D(void) {
}
