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
#define FLT_MAX_F 3.4028234663852886e+38f	/* rva 0x3a3a38 */
/* the 15 colors of the strips (rva 0x432a34) */
#define g_StripColors (GAME_ARRAY_IN(const uint32_t, g_BspColors, 4, 0x432a34))
#define STRIP_COLORS 15
#define TRI_USED 0x8000
#define TRI_FREE 0xffff

CVertList *__fastcall CVertList_ctor(CVertList *self) {
	self->pVert = NULL;
	self->totVert = 0;
	self->frmt = 0;
	self->nVert = 0;
	self->scale = 0.0f;
	return self;
}

void __fastcall CVertList_Free(CVertList *self) {
	if (self->pVert) {
		Mem_Free(self->pVert);
		self->pVert = NULL;
	}
	self->frmt = 0;
	self->totVert = 0;
	self->nVert = 0;
}

void __thiscall CVertList_SetFormat(CVertList *self, int frmt) {
	CHECK(frmt >= VERT_NULL && frmt < VERT_TOTAL);
	if (self->pVert) {
		Mem_Free(self->pVert);
		self->pVert = NULL;
	}
	self->frmt = frmt;
	self->totVert = 0;
	self->nVert = 0;
}

void __thiscall CVertList_Alloc(CVertList *self, int n, byte realloc) {
	CHECK(n > 0);
	CHECK(realloc || !self->pVert);
	self->nVert = n;
	if (self->pVert) {
		n = (n >> 2) + n;
	}
	self->totVert = n;
	self->pVert = Mem_Realloc(self->pVert, self->totVert * VertList_FormatSize(self->frmt));
}

void __thiscall CVertList_Swap(CVertList *self, CVertList *other) {
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

void __thiscall CVertList_Read(CVertList *self, CFileIO *f) {
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

void __thiscall CVertList_Write(CVertList *self, CFileIO *f) {
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

uint32_t __fastcall CVertList_FVF(CVertList *self) {
	return VertList_FormatFVF(self->frmt);
}

int __fastcall CVertList_Size(CVertList *self) {
	return VertList_FormatSize(self->frmt);
}

const char *__fastcall CVertList_FormatName(CVertList *self) {
	return VertList_FormatName(self->frmt);
}

/* cdq, xor, sub: INT_MIN stays negative */
static int VertList_Abs(int v) {
	uint32_t s = (uint32_t)(v >> 31);
	return (int)(((uint32_t)v ^ s) - s);
}

uint32_t VertList_PackNormal(const vec3_t *n) {
	int sx = (int)((double)n->x * 1023.0);
	int sy = (int)((double)n->y * 1023.0);
	int sz = (int)((double)n->z * 511.0);

	CHECK(VertList_Abs(sx) < 1024);
	CHECK(VertList_Abs(sy) < 1024);
	CHECK(VertList_Abs(sz) < 512);
	return ((((uint32_t)sy & 0x7ff) | (uint32_t)sz << 11) << 11) | ((uint32_t)sx & 0x7ff);
}

vec3_t *VertList_UnpackNormal(vec3_t *out, uint32_t packed) {
	out->x = (float)((int32_t)(packed << 21) >> 21) / 1023.0f;
	out->y = (float)((int32_t)(packed << 10) >> 21) / 1023.0f;
	out->z = (float)((int32_t)packed >> 22) / 511.0f;
	return out;
}

/* the field getters of one vertex */
void *__thiscall VertTex1_Field(byte *v, int f) {
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

void *__thiscall VertTex2_Field(byte *v, int f) {
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

void *__thiscall VertLTex1_Field(byte *v, int f) {
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

void *__thiscall VertLTex2N_Field(byte *v, int f) {
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

void *__thiscall VertLTex3_Field(byte *v, int f) {
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

void *__thiscall VertLTex1N_Field(byte *v, int f) {
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
void *__thiscall VertTLTex1_Field(byte *v, int f) {
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

void *__thiscall VertLTex2_Field(byte *v, int f) {
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

void *__thiscall VertTex1XBox_Field(byte *v, int f) {
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

void *__thiscall VertTex1CXBox_Field(byte *v, int f) {
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

void *__thiscall VertLTex1XBox_Field(byte *v, int f) {
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

void *__thiscall VertLTex1NXBox_Field(byte *v, int f) {
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

void *__thiscall VertLTex2XBox_Field(byte *v, int f) {
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

void *__thiscall VertLTex2NXBox_Field(byte *v, int f) {
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

byte __thiscall CVertList_Optimize(CVertList *self, void *tris, int nTri) {
	(void)self;
	(void)tris;
	(void)nTri;
	return 0;
}

byte *__thiscall CVertList_Tex1(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_TEX1, 0x20);
}

byte *__thiscall CVertList_Tex2(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_TEX2, 0x28);
}

byte *__thiscall CVertList_LTex1(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX1, 0x1c);
}

byte *__thiscall CVertList_Tex1B1(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_TEX1_B1, 0x24);
}

byte *__thiscall CVertList_LTex2N(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX2N, 0x2c);
}

byte *__thiscall CVertList_LTex3(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX3, 0x34);
}

byte *__thiscall CVertList_LTex1N(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX1N, 0x24);
}

byte *__thiscall CVertList_TLTex1(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_TLTEX1, 0x20);
}

byte *__thiscall CVertList_LTex2(CVertList *self, uint32_t i) {
	return VertList_Vert(self, i, VERT_LTEX2, 0x20);
}

void __thiscall CVertList_SetTex1(CVertList *self, uint32_t i, const void *v) {
	memcpy(VertList_Vert(self, i, VERT_TEX1, 0x20), v, 0x20);
}

void __thiscall CVertList_SetTex2(CVertList *self, uint32_t i, const void *v) {
	memcpy(VertList_Vert(self, i, VERT_TEX2, 0x28), v, 0x28);
}

void __thiscall CVertList_SetLTex1(CVertList *self, uint32_t i, const void *v) {
	memcpy(VertList_Vert(self, i, VERT_LTEX1, 0x1c), v, 0x1c);
}

void __thiscall CVertList_SetTex1B1(CVertList *self, uint32_t i, const void *v) {
	memcpy(VertList_Vert(self, i, VERT_TEX1_B1, 0x24), v, 0x24);
}

void *__thiscall CVertList_GetTex1(CVertList *self, void *out, uint32_t i) {
	switch (self->frmt) {
	case VERT_TEX1:
		memcpy(out, CVertList_Tex1(self, i), 0x20);
		return out;
	case VERT_TEX2:
		memcpy(out, CVertList_Tex2(self, i), 0x20);
		return out;
	case VERT_TEX1_B1: {
		/* without the weight at 0xc */
		const byte *v = CVertList_Tex1B1(self, i);
		memcpy(out, v, 0xc);
		memcpy((byte *)out + 0xc, v + 0x10, 0x14);
		return out;
	}
	}
	CHECK(0);
	CHECK(0);
	return out;
}

void *__thiscall CVertList_GetField(CVertList *self, int f, uint32_t i) {
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

void __thiscall CVertList_SetColor(CVertList *self, uint32_t and, uint32_t or) {
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

static float VertList_BoxLow(const box_t *b) {
	return Float_MinSS(b->min.z, Float_MinSS(b->min.y, b->min.x));
}

static float VertList_BoxHigh(const box_t *b) {
	return Float_MaxSS(b->max.z, Float_MaxSS(b->max.y, b->max.x));
}

void __thiscall CVertList_GetExtents(CVertList *self, box_t *ext, byte reset) {
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

void __thiscall CVertList_Transform(CVertList *self, const matrix_t *m, byte normalize) {
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

/* cvttss2si to a short, saturated */
static int16_t VertList_Short(float f) {
	int i = (int)f;

	if (i <= -0x8000) {
		return -0x8000;
	}
	if (i >= 0x7fff) {
		return 0x7fff;
	}
	return (int16_t)i;
}

/* a list of n vertices in frmt to fill, kept apart from self until the end */
static void VertList_NewTmp(CVertList *tmp, int frmt, int n) {
	tmp->pVert = NULL;
	tmp->scale = 0.0f;
	tmp->nVert = 0;
	tmp->totVert = 0;
	tmp->frmt = frmt;
	CHECK(n > 0);
	tmp->nVert = n;
	tmp->totVert = n;
	tmp->pVert = Mem_Realloc(NULL, VertList_FormatSize(frmt) * n);
}

/* the vertices of tmp into self, the old ones freed */
static void VertList_TakeTmp(CVertList *self, CVertList *tmp, int n, int frmt) {
	byte *old = self->pVert;

	self->pVert = tmp->pVert;
	self->nVert = n;
	self->totVert = n;
	self->frmt = frmt;
	if (old) {
		Mem_Free(old);
	}
}

static void VertList_WarnPos(const box_t *pos, float posScale) {
	if ((double)posScale * -32768.0 > (double)VertList_BoxLow(pos) ||
		VertList_BoxHigh(pos) > posScale * 32768.0f) {
		Con_Warnf("Vertex out of range x%.1f\n", (double)posScale);
	}
}

static void VertList_WarnTex(const box_t *tex, float texScale, const char *fmt) {
	if (-32768.0f / texScale > VertList_BoxLow(tex) || VertList_BoxHigh(tex) > 32768.0f / texScale) {
		Con_Warnf(fmt, (double)(32768.0f / texScale));
	}
}

void __thiscall CVertList_Pack(CVertList *self, int frmt, float posScale, float texScale) {
	CVertList tmp;
	box_t pos, tex;

	CHECK(frmt >= VERT_NULL && frmt < VERT_TOTAL);
	int n = self->nVert;
	VertList_NewTmp(&tmp, frmt, n);
	VertList_BoxInit(&pos);
	VertList_BoxInit(&tex);
	for (int i = 0; i < self->nVert; i++) {
		vec3_t nrm = { 0.0f, 0.0f, 0.0f };
		float uv[2] = { 0.0f, 0.0f };
		uint32_t color = 0;

		vec3_t p = *(const vec3_t *)CVertList_GetField(self, LVERT_POS, i);
		const vec3_t *pn = CVertList_GetField(self, LVERT_NORMAL, i);
		if (pn) {
			nrm = *pn;
		}
		const uint32_t *pc = CVertList_GetField(self, LVERT_DIFFUSE, i);
		if (pc) {
			color = *pc;
		}
		const float *pt = CVertList_GetField(self, LVERT_TEX2D_SET1, i);
		if (pt) {
			memcpy(uv, pt, sizeof(uv));
		}
		VertList_BoxAdd(&pos, p.x, p.y, p.z);
		VertList_BoxAdd(&tex, uv[0], uv[1], 0.0f);

		if (frmt == VERT_POS_XBOX) {
			vec3_t s;
			s.x = p.x / self->scale;
			s.y = p.y / self->scale;
			s.z = p.z / self->scale;
			((uint32_t *)tmp.pVert)[i] = VertList_PackNormal(&s);
		} else {
			*(vec3_t *)CVertList_GetField(&tmp, LVERT_POS, i) = p;
		}
		uint32_t *dn = CVertList_GetField(&tmp, LVERT_NORMAL_PACKED, i);
		if (dn) {
			*dn = VertList_PackNormal(&nrm);
		}
		uint32_t *dc = CVertList_GetField(&tmp, LVERT_DIFFUSE, i);
		if (dc) {
			*dc = color;
		}
		int16_t *ds = CVertList_GetField(&tmp, LVERT_TEX_SHORT, i);
		if (ds) {
			ds[0] = VertList_Short(uv[0] * texScale);
			ds[1] = VertList_Short(uv[1] * texScale);
		}
		float *dt = CVertList_GetField(&tmp, LVERT_TEX2D_SET1, i);
		if (dt) {
			memcpy(dt, uv, sizeof(uv));
		}
	}
	VertList_WarnPos(&pos, posScale);
	VertList_WarnTex(&tex, texScale, "Texcoord out of range %.1f\n");
	VertList_TakeTmp(self, &tmp, n, frmt);
}

void __thiscall CVertList_PackTex1(CVertList *self, float posScale, float texScale) {
	int frmt = VERT_NULL;

	if (self->frmt == VERT_TEX1) {
		frmt = VERT_TEX1_XBOX;
	} else if (self->frmt == VERT_LTEX1N) {
		frmt = VERT_TEX1C_XBOX;
	}
	CVertList_Pack(self, frmt, posScale, texScale);
}

void __thiscall CVertList_ToXBox(CVertList *self, float posScale, float texScale, byte tex2) {
	CVertList tmp;
	box_t pos, tex, tex1;

	byte hasNormal = CVertList_GetField(self, LVERT_NORMAL, 0) != NULL;
	byte hasTex1 = CVertList_GetField(self, LVERT_TEX2D_SET1 + 1, 0) != NULL;
	int frmt = hasNormal + VERT_LTEX1_XBOX;
	if (hasTex1 && tex2) {
		frmt = hasNormal + VERT_LTEX2_XBOX;
	}
	int n = self->nVert;
	VertList_NewTmp(&tmp, frmt, n);
	VertList_BoxInit(&pos);
	VertList_BoxInit(&tex);
	VertList_BoxInit(&tex1);
	for (int i = 0; i < self->nVert; i++) {
		vec3_t nrm = { 0.0f, 0.0f, 0.0f };
		float uv[2] = { 0.0f, 0.0f };
		float uv1[2] = { 0.0f, 0.0f };
		uint32_t color = 0;

		vec3_t p = *(const vec3_t *)CVertList_GetField(self, LVERT_POS, i);
		const vec3_t *pn = CVertList_GetField(self, LVERT_NORMAL, i);
		if (pn) {
			nrm = *pn;
		}
		const uint32_t *pc = CVertList_GetField(self, LVERT_DIFFUSE, i);
		if (pc) {
			color = *pc;
		}
		const float *pt = CVertList_GetField(self, LVERT_TEX2D_SET1, i);
		if (pt) {
			memcpy(uv, pt, sizeof(uv));
		}
		const float *pt1 = CVertList_GetField(self, LVERT_TEX2D_SET1 + 1, i);
		if (pt1) {
			memcpy(uv1, pt1, sizeof(uv1));
		}
		VertList_BoxAdd(&pos, p.x, p.y, p.z);
		VertList_BoxAdd(&tex, uv[0], uv[1], 0.0f);
		VertList_BoxAdd(&tex1, uv1[0], uv1[1], 0.0f);

		int16_t *dp = CVertList_GetField(&tmp, LVERT_POS_SHORT, i);
		dp[0] = VertList_Short(p.x / posScale);
		dp[1] = VertList_Short(p.y / posScale);
		dp[2] = VertList_Short(p.z / posScale);
		uint32_t *dn = CVertList_GetField(&tmp, LVERT_NORMAL_PACKED, i);
		if (dn) {
			*dn = VertList_PackNormal(&nrm);
		}
		uint32_t *dc = CVertList_GetField(&tmp, LVERT_DIFFUSE, i);
		if (dc) {
			*dc = color;
		}
		int16_t *ds = CVertList_GetField(&tmp, LVERT_TEX_SHORT, i);
		if (ds) {
			ds[0] = VertList_Short(uv[0] * texScale);
			ds[1] = VertList_Short(uv[1] * texScale);
		}
		float *dt = CVertList_GetField(&tmp, LVERT_TEX2D_SET1, i);
		if (dt) {
			memcpy(dt, uv, sizeof(uv));
		}
		int16_t *ds1 = CVertList_GetField(&tmp, LVERT_TEX_SHORT2, i);
		if (ds1) {
			ds1[0] = VertList_Short(uv1[0] * 32767.0f);
			ds1[1] = VertList_Short(uv1[1] * 32767.0f);
		}
		float *dt1 = CVertList_GetField(&tmp, LVERT_TEX2D_SET1 + 1, i);
		if (dt1) {
			memcpy(dt1, uv1, sizeof(uv1));
		}
	}
	VertList_WarnPos(&pos, posScale);
	VertList_WarnTex(&tex, texScale, "Texcoord_1 out of range %.1f\n");
	if (-1.0f > VertList_BoxLow(&tex1) || VertList_BoxHigh(&tex1) > 1.0f) {
		Con_Warnf("Texcoord_2 out of range -1..1\n");
	}
	VertList_TakeTmp(self, &tmp, n, frmt);
}

void __thiscall CVertList_FromXBox(CVertList *self, float posScale, float texScale) {
	byte *out = Mem_Alloc(self->nVert * 0x24);
	const byte *src = self->pVert;

	CHECK(self->frmt == VERT_LTEX1N_XBOX);
	for (int i = 0; i < self->nVert; i++, src += 0x16) {
		float *d = (float *)(out + i * 0x24);
		int16_t s[3];
		uint32_t nrm;
		float uv[2];

		memcpy(s, src, sizeof(s));
		memcpy(&nrm, src + 6, sizeof(nrm));
		memcpy(uv, src + 0xe, sizeof(uv));
		d[0] = (float)s[0] * posScale;
		d[1] = (float)s[1] * posScale;
		d[2] = (float)s[2] * posScale;
		memcpy(&d[6], src + 10, 4);
		d[7] = uv[0] / texScale;
		d[8] = uv[1] / texScale;
		VertList_UnpackNormal((vec3_t *)&d[3], nrm);
	}
	Mem_Free(self->pVert);
	self->pVert = out;
	self->totVert = self->nVert;
	self->frmt = VERT_LTEX1N;
}

/* the triangle strips: a triangle is in use while bit 15 of its corners is set, taken when all are 0xffff */
void Tri_Mark(uint16_t *tri) {
	CHECK(((tri[0] | tri[1] | tri[2]) & TRI_USED) == 0);
	tri[0] |= TRI_USED;
	tri[1] |= TRI_USED;
	tri[2] |= TRI_USED;
}

void Tri_Unmark(uint16_t *tri) {
	CHECK(((tri[0] & tri[1] & tri[2]) & TRI_USED) != 0);
	tri[0] &= ~TRI_USED;
	tri[1] &= ~TRI_USED;
	tri[2] &= ~TRI_USED;
}

void Tri_Rotate(uint16_t *tri, int n) {
	n %= 3;
	if (n <= 0) {
		return;
	}
	uint16_t a = tri[1], b = tri[2];
	do {
		uint16_t c = tri[0];
		tri[0] = a;
		a = b;
		b = c;
	} while (--n > 0);
	tri[1] = a;
	tri[2] = b;
}

static void Tri_Take(uint16_t *tri) {
	tri[0] = TRI_FREE;
	tri[1] = TRI_FREE;
	tri[2] = TRI_FREE;
}

int Strip_Next(const uint16_t *tris, int nTri, uint32_t a, uint32_t b, int depth, int *side) {
	uint16_t e0 = (uint16_t)(a & 0x7fff);
	uint16_t e1 = (uint16_t)(b & 0x7fff);

	/* odd triangles of a strip run the other way round */
	if (depth & 1) {
		uint16_t t = e0;
		e0 = e1;
		e1 = t;
	}
	for (int t = 0; t < nTri; t++, tris += 3) {
		if (tris[0] >= TRI_USED || tris[0] == TRI_FREE) {
			continue;
		}
		if (tris[0] == e0 && tris[1] == e1) {
			*side = 2;
			return t;
		}
		if (tris[1] == e0 && tris[2] == e1) {
			*side = 0;
			return t;
		}
		if (tris[2] == e0 && tris[0] == e1) {
			*side = 1;
			return t;
		}
	}
	return -1;
}

int Strip_Length(int t, uint16_t *tris, int nTri, int depth, uint32_t a, uint32_t b) {
	uint16_t *tri = &tris[t * 3];
	int side;
	int len = 1;

	Tri_Mark(tri);
	int ts = Strip_Next(tris, nTri, a, b, depth + 1, &side);
	CHECK(ts != t);
	if (ts >= 0) {
		len = Strip_Length(ts, tris, nTri, depth + 1, b, tris[ts * 3 + side]) + 1;
	}
	Tri_Unmark(tri);
	return len;
}

int Strip_FindFirst(uint16_t *tris, int nTri, int depth) {
	int best = 0, bestTri = 0, bestEdge = 0;

	for (int t = 0; t < nTri; t++) {
		const uint16_t *tri = &tris[t * 3];
		if (tri[0] == TRI_FREE) {
			continue;
		}
		int l0 = Strip_Length(t, tris, nTri, depth, tri[1], tri[2]);
		int l1 = Strip_Length(t, tris, nTri, depth, tri[2], tri[0]);
		int l2 = Strip_Length(t, tris, nTri, depth, tri[0], tri[1]);
		if (l0 > best) {
			best = l0;
			bestTri = t;
			bestEdge = 0;
		}
		if (l1 > best) {
			best = l1;
			bestTri = t;
			bestEdge = 1;
		}
		if (l2 > best) {
			best = l2;
			bestTri = t;
			bestEdge = 2;
		}
	}
	if (best) {
		Tri_Rotate(&tris[bestTri * 3], bestEdge);
		return bestTri;
	}
	for (int t = 0; t < nTri; t++) {
		CHECK(tris[t * 3] == TRI_FREE);
	}
	return -1;
}

int Strip_FindNew(uint16_t *tris, int nTri, int depth) {
	for (int t = 0; t < nTri; t++) {
		uint16_t *tri = &tris[t * 3];
		int side;
		if (tri[0] == TRI_FREE) {
			continue;
		}
		Tri_Mark(tri);
		int ta = Strip_Next(tris, nTri, tri[1], tri[2], depth + 1, &side);
		CHECK(ta != t);
		if (ta >= 0) {
			Tri_Unmark(tri);
			return t;
		}
		int tb = Strip_Next(tris, nTri, tri[2], tri[0], depth + 1, &side);
		CHECK(tb != t);
		if (tb >= 0) {
			Tri_Rotate(tri, 1);
			Tri_Unmark(tri);
			return t;
		}
		int tc = Strip_Next(tris, nTri, tri[0], tri[1], depth + 1, &side);
		CHECK(tc != t);
		if (tc >= 0) {
			Tri_Rotate(tri, 2);
			Tri_Unmark(tri);
			return t;
		}
		Tri_Unmark(tri);
	}
	for (int t = 0; t < nTri; t++) {
		if (tris[t * 3] != TRI_FREE) {
			return t;
		}
	}
	return -1;
}

byte VertList_Strip(CVertList *out, const CVertList *in, const uint16_t *tris, int nTri) {
	CHECK(nTri > 0);
	CHECK(in->nVert >= 3);
	CHECK(in->nVert <= 0x7fff);
	uint16_t *sTri = New_Array(nTri, 6);
	memcpy(sTri, tris, nTri * 6);
	uint16_t *strip = New_Array(nTri * 8, 2);

	int t = Strip_FindFirst(sTri, nTri, 0);
	strip[0] = sTri[t * 3];
	strip[1] = sTri[t * 3 + 1];
	strip[2] = sTri[t * 3 + 2];
	Tri_Take(&sTri[t * 3]);
	int n = 3;
	for (int k = nTri - 1; k > 0; k--) {
		int side;
		uint16_t v;
		t = Strip_Next(sTri, nTri, strip[n - 2], strip[n - 1], n, &side);
		if (t < 0) {
			/* a new strip, joined by degenerate triangles */
			t = Strip_FindFirst(sTri, nTri, n);
			CHECK(t >= 0);
			const uint16_t *tri = &sTri[t * 3];
			strip[n] = strip[n - 1];
			strip[n + 1] = tri[0];
			strip[n + 2] = tri[0];
			n += 3;
			if (n & 1) {
				strip[n++] = tri[1];
				v = tri[2];
			} else {
				strip[n++] = tri[2];
				v = tri[1];
			}
		} else {
			v = sTri[t * 3 + side];
		}
		strip[n++] = v;
		Tri_Take(&sTri[t * 3]);
	}
	CHECK(n < nTri * 8);
	CHECK(Strip_FindNew(sTri, nTri, 0) < 0);

	CVertList_SetFormat(out, in->frmt);
	CVertList_Alloc(out, n, 0);
	int size = VertList_FormatSize(out->frmt);
	byte *dst = out->pVert;
	for (int i = 0; i < n; i++, dst += size) {
		CHECK(strip[i] < in->nVert);
		memcpy(dst, in->pVert + strip[i] * size, size);
	}
	/* the color changes at each degenerate triangle */
	int color = 0;
	for (int i = 0; i < n; i++) {
		if (i + 1 < n && strip[i] == strip[i + 1]) {
			continue;
		}
		if (i > 0 && strip[i] == strip[i - 1]) {
			color++;
		}
		uint32_t *c = CVertList_GetField(out, LVERT_DIFFUSE, i);
		if (c) {
			*c = g_StripColors[color % STRIP_COLORS];
		}
	}
	if (sTri) {
		operator_delete_sized(sTri, 6);
	}
	if (strip) {
		operator_delete_sized(strip, 2);
	}
	E3D_LogDebug(g_FileLog, "Strips Verts: %d -> %d", in->nVert, out->nVert);
	return 1;
}

int VertCache_Find(int v) {
	for (int i = 0; i < VCACHE_SIZE; i++) {
		if (g_VCacheVert[i] == v) {
			return i;
		}
	}
	return -1;
}

int VertCache_Oldest(void) {
	int best = 1000000, slot = 0;

	for (int i = 0; i < VCACHE_SIZE; i++) {
		if (g_VCacheVert[i] < 0) {
			return i;
		}
		if (g_VCacheAge[i] < best) {
			best = g_VCacheAge[i];
			slot = i;
		}
	}
	return slot;
}

void VertCache_Add(int v) {
	int slot = VertCache_Find(v);

	if (slot < 0) {
		g_VCacheMisses++;
		slot = VertCache_Oldest();
	} else {
		g_VCacheHits++;
	}
	int count = g_VCacheCount;
	g_VCacheAge[slot] = count;
	count++;
	g_VCacheVert[slot] = v;
	g_VCacheCount = count;
	CHECK(count < 65536);
}

/* the cost of a vertex for the cache: its age in it, else 1000000 plus its number */
static uint32_t VertCache_Cost(int v) {
	int slot = VertCache_Find(v);
	return slot >= 0 ? (uint32_t)g_VCacheAge[slot] : 1000000u + (uint32_t)v;
}

int VertCache_Compare(const void *a, const void *b) {
	const uint16_t *ta = a, *tb = b;
	uint32_t ca = 0, cb = 0;

	for (int k = 0; k < 3; k++) {
		ca += VertCache_Cost(ta[k]);
		cb += VertCache_Cost(tb[k]);
	}
	return (int)(ca - cb);
}

void VertCache_Reset(void) {
	memset(g_VCacheVert, -1, VCACHE_SIZE * sizeof(int));
	memset(g_VCacheAge, -1, VCACHE_SIZE * sizeof(int));
	g_VCacheCount = 0;
	g_VCacheHits = 0;
	g_VCacheMisses = 0;
}

byte VertCache_Optimize(uint16_t *tris, int nTri) {
	VertCache_Reset();
	for (int i = 0; i < nTri; i++) {
		VertCache_Add(tris[i * 3]);
		VertCache_Add(tris[i * 3 + 1]);
		VertCache_Add(tris[i * 3 + 2]);
	}
	int hits = g_VCacheHits, misses = g_VCacheMisses;
	VertCache_Reset();
	for (int i = 0; i < nTri; i++, tris += 3) {
		crt_qsort(tris, nTri - i, 6, VertCache_Compare);
		VertCache_Add(tris[0]);
		VertCache_Add(tris[1]);
		VertCache_Add(tris[2]);
	}
	E3D_LogDebug(g_FileLog, " prev. hit: %d  miss: %d", hits, misses);
	E3D_LogDebug(g_FileLog, " opt.  hit: %d  miss: %d", g_VCacheHits, g_VCacheMisses);
	return 1;
}

void __thiscall CVertList_EnvMapUV(CVertList *self, const matrix_t *world, const matrix_t *view,
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

static void VertList_RampUV(CVertList *self, int stride, int off, const matrix_t *r) {
	float a = r->m[2], b = r->m[6], c = r->m[10];

	for (int i = 0; i < self->nVert; i++) {
		float *v = (float *)(self->pVert + i * stride);
		v[off / 4 + 1] = 0.5f;
		v[off / 4] = ((v[4] * b + v[3] * a + v[5] * c) * 0.95f + 1.0f) * 0.5f;
	}
}

void __thiscall CVertList_LightRampUV(CVertList *self, const matrix_t *world) {
	matrix_t rx, ry, rot, r;
	D3DLight light;
	int on;

	CHECK(self->pVert);
	on = 0;
	void *dev = g_Engine->device;
	if (D3D_METHOD(dev, d3dGetLightEnable_t, D3DDEV_GETLIGHTENABLE)(dev, 0, &on) || !on) {
		return;
	}
	dev = g_Engine->device;
	D3D_METHOD(dev, d3dGetLight_t, D3DDEV_GETLIGHT)(dev, 0, &light);
	const vec3_t *d = &light.Direction;
	double len = __builtin_sqrt((double)(d->z * d->z + d->x * d->x));
	Matrix_RotationX(&rx, -(float)Game_Atan2((double)d->y, len));
	Matrix_RotationY(&ry, Game_Atan2F((double)d->x, (double)d->z));
	rot = *Matrix_MulD3D(&r, &ry, &rx);
	Matrix_MulTo(&r, world, &rot);
	if (self->frmt == VERT_TEX1) {
		VertList_RampUV(self, 0x20, 0x18, &r);
	} else if (self->frmt == VERT_TEX2) {
		VertList_RampUV(self, 0x28, 0x20, &r);
	}
}

/* the D3D resources of the vertex lists: nothing to make or free (the buffers belong to the meshes) */
void VertList_ReleaseD3D(void) {
}

void VertList_InitD3D(void) {
}
