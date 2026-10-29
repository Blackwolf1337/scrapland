/*
 * Engine/ListaVert.cpp of the engine3d: the vertex list of a mesh, an array of vertices in one of the D3D
 * formats (VERT_*). The fields of a vertex (LVERT_*) are found through CVertList_GetField. The XBOX formats keep
 * the positions as shorts, the normals packed into 11:11:10 bits and the texture coordinates as shorts. The file
 * also builds triangle strips (a triangle is 3 ushorts, bit 15 marks the ones in use) and measures the hits of a
 * 16 entry vertex cache.
 */
#ifndef SCRAP_VERTLIST_H
#define SCRAP_VERTLIST_H

#include "scrap/archio.h"
#include "scrap/mathlib.h"

/* the formats; the size and the D3D FVF of each are VertList_FormatSize and VertList_FormatFVF */
enum {
	VERT_NULL = 0,
	VERT_TEX1 = 1,			/* 0x20: pos, normal, tex */
	VERT_TEX2 = 2,			/* 0x28: pos, normal, 2 tex */
	VERT_LTEX1 = 3,			/* 0x1c: pos, diffuse, specular, tex */
	VERT_TEX1_B1 = 4,		/* 0x24: pos, weight, normal, tex */
	VERT_LTEX2N = 5,		/* 0x2c: pos, normal, diffuse, 2 tex */
	VERT_LTEX3 = 6,			/* 0x34: pos, normal, diffuse, 3 tex */
	VERT_LTEX1N = 7,		/* 0x24: pos, normal, diffuse, tex */
	VERT_TLTEX1 = 8,		/* 0x20: transformed pos and rhw, diffuse, specular, tex */
	VERT_LTEX2 = 10,		/* 0x20: pos, diffuse, 2 tex */
	VERT_POS_XBOX = 11,		/* 4: the position packed like a normal, divided by esc */
	VERT_TEX1_XBOX = 12,		/* 0x18 */
	VERT_TEX1C_XBOX = 13,		/* 0x1c */
	VERT_LTEX1_XBOX = 14,		/* 0x12 */
	VERT_LTEX1N_XBOX = 15,		/* 0x16 */
	VERT_LTEX2_XBOX = 16,		/* 0x16 */
	VERT_LTEX2N_XBOX = 17,		/* 0x1a */
	VERT_TOTAL = 18
};

/* the fields of a vertex */
enum {
	LVERT_POS = 0,
	LVERT_NORMAL = 2,
	LVERT_DIFFUSE = 3,
	LVERT_SPECULAR = 4,
	LVERT_TEX2D_SET1 = 5,		/* up to LVERT_TEX2D_SET8 = 12 */
	LVERT_POS_SHORT = 13,		/* 3 shorts: pos / scale */
	LVERT_NORMAL_PACKED = 14,	/* VertList_PackNormal */
	LVERT_TEX_SHORT = 15,		/* 2 shorts: tex * scale */
	LVERT_TEX_SHORT2 = 16		/* 2 shorts: tex * 32767 */
};

typedef struct CVertList {
	byte *pVert;			/* 0x0 Mem_Realloc */
	int nVert;			/* 0x4 */
	int totVert;			/* 0x8 allocated */
	int frmt;			/* 0xc VERT_* */
	float scale;			/* 0x10 divides the positions of VERT_POS_XBOX */
} CVertList;
SD_SIZE(CVertList, 0x14);

/* the vertex shader handles of the XBOX formats */
#define g_hVShader_Tex1_XBox GAME_GLOBAL(uint32_t, g_hVShader_Tex1_XBox, 0x4ad630)
#define g_hVShader_Tex1C_XBox GAME_GLOBAL(uint32_t, g_hVShader_Tex1C_XBox, 0x4ad634)
#define g_hVShader_LTex1_XBox GAME_GLOBAL(uint32_t, g_hVShader_LTex1_XBox, 0x4ad638)
#define g_hVShader_LTex1N_XBox GAME_GLOBAL(uint32_t, g_hVShader_LTex1N_XBox, 0x4ad63c)
#define g_hVShader_LTex2_XBox GAME_GLOBAL(uint32_t, g_hVShader_LTex2_XBox, 0x4ad640)
#define g_hVShader_LTex2N_XBox GAME_GLOBAL(uint32_t, g_hVShader_LTex2N_XBox, 0x4ad644)

/* the vertex cache: vertex and age of each entry (-1 empty), the next age, hits and misses */
#define g_VCacheVert (GAME_ARRAY(int, g_VCacheVert, 0x4ad648))
#define g_VCacheAge (GAME_ARRAY(int, g_VCacheAge, 0x4ad688))
#define g_VCacheCount GAME_GLOBAL_IN(int, g_VCacheAge, 64, 0x4ad6c8)
#define g_VCacheHits GAME_GLOBAL_IN(int, g_VCacheAge, 68, 0x4ad6cc)
#define g_VCacheMisses GAME_GLOBAL_IN(int, g_VCacheAge, 72, 0x4ad6d0)
#define VCACHE_SIZE 16

CVertList *__fastcall CVertList_ctor(CVertList *self);
/* also the destructor (two copies in the exe) */
void __fastcall CVertList_Free(CVertList *self);
/* empty, in the format */
void __thiscall CVertList_SetFormat(CVertList *self, int frmt);
/* n vertices; with pVert set (realloc) 25% more room */
void __thiscall CVertList_Alloc(CVertList *self, int n, byte realloc);
/* an empty stub, returns 0 */
byte __thiscall CVertList_Optimize(CVertList *self, void *tris, int nTri);
/* the coordinates itex of the reflection map in software: with map->bb 1 the reflected view direction (sphere
 * map), else the normal through world * view; then offset f10 and scale f18 of the map */
struct CMap;
void __thiscall CVertList_EnvMapUV(CVertList *self, const matrix_t *world, const matrix_t *view,
	const struct CMap *map, int itex);
/* VERT_TEX1 and VERT_TEX2 (second set): u from the angle between the normal through world and light 0 of the
 * device, v 0.5 (a ramp texture); nothing while light 0 is off */
void __thiscall CVertList_LightRampUV(CVertList *self, const matrix_t *world);
/* the first 4 fields */
void __thiscall CVertList_Swap(CVertList *self, CVertList *other);
/* the "LFVF" block */
void __thiscall CVertList_Read(CVertList *self, CFileIO *f);
void __thiscall CVertList_Write(CVertList *self, CFileIO *f);

uint32_t VertList_FormatFVF(int frmt);
int VertList_FormatSize(int frmt);
const char *VertList_FormatName(int frmt);
uint32_t __fastcall CVertList_FVF(CVertList *self);
int __fastcall CVertList_Size(CVertList *self);
const char *__fastcall CVertList_FormatName(CVertList *self);
/* a unit vector into 11:11:10 bits (x low) and back */
uint32_t VertList_PackNormal(const vec3_t *n);
vec3_t *VertList_UnpackNormal(vec3_t *out, uint32_t packed);

/* field f of vertex i, NULL if the format has none */
void *__thiscall CVertList_GetField(CVertList *self, int f, uint32_t i);
/* the field of one vertex of each format */
void *__thiscall VertTex1_Field(byte *v, int f);
void *__thiscall VertTex2_Field(byte *v, int f);
void *__thiscall VertLTex1_Field(byte *v, int f);
void *__thiscall VertLTex2N_Field(byte *v, int f);
void *__thiscall VertLTex3_Field(byte *v, int f);
void *__thiscall VertLTex1N_Field(byte *v, int f);
void *__thiscall VertTLTex1_Field(byte *v, int f);
void *__thiscall VertLTex2_Field(byte *v, int f);
void *__thiscall VertTex1XBox_Field(byte *v, int f);
void *__thiscall VertTex1CXBox_Field(byte *v, int f);
void *__thiscall VertLTex1XBox_Field(byte *v, int f);
void *__thiscall VertLTex1NXBox_Field(byte *v, int f);
void *__thiscall VertLTex2XBox_Field(byte *v, int f);
void *__thiscall VertLTex2NXBox_Field(byte *v, int f);

/* vertex i of the format, with checks */
byte *__thiscall CVertList_Tex1(CVertList *self, uint32_t i);
byte *__thiscall CVertList_Tex2(CVertList *self, uint32_t i);
byte *__thiscall CVertList_LTex1(CVertList *self, uint32_t i);
byte *__thiscall CVertList_Tex1B1(CVertList *self, uint32_t i);
byte *__thiscall CVertList_LTex2N(CVertList *self, uint32_t i);
byte *__thiscall CVertList_LTex3(CVertList *self, uint32_t i);
byte *__thiscall CVertList_LTex1N(CVertList *self, uint32_t i);
byte *__thiscall CVertList_TLTex1(CVertList *self, uint32_t i);
byte *__thiscall CVertList_LTex2(CVertList *self, uint32_t i);
void __thiscall CVertList_SetTex1(CVertList *self, uint32_t i, const void *v);
void __thiscall CVertList_SetTex2(CVertList *self, uint32_t i, const void *v);
void __thiscall CVertList_SetLTex1(CVertList *self, uint32_t i, const void *v);
void __thiscall CVertList_SetTex1B1(CVertList *self, uint32_t i, const void *v);
/* vertex i of a TEX1, TEX2 or TEX1_B1 list as a TEX1 vertex (0x20 bytes) */
void *__thiscall CVertList_GetTex1(CVertList *self, void *out, uint32_t i);
/* the position of vertex i (rva 0x212570, in MallaTri.c) */
vec3_t *__thiscall CVertList_GetPos(CVertList *self, vec3_t *out, uint32_t i);

/* the diffuse colors: c & and | or */
void __thiscall CVertList_SetColor(CVertList *self, uint32_t and, uint32_t or);
/* the box of the positions, from the empty box if reset */
void __thiscall CVertList_GetExtents(CVertList *self, box_t *ext, byte reset);
/* positions and normals (TEX1, TEX2, TEX1_B1) by m, the normals made unit length if normalize */
void __thiscall CVertList_Transform(CVertList *self, const matrix_t *m, byte normalize);
/* into frmt (TEX1_XBOX, TEX1C_XBOX or POS_XBOX) with the texture coordinates times texScale; warns about
 * positions outside +-32768 * posScale and texture coordinates outside +-32768 / texScale */
void __thiscall CVertList_Pack(CVertList *self, int frmt, float posScale, float texScale);
/* TEX1 to TEX1_XBOX, LTEX1N to TEX1C_XBOX, others to VERT_NULL (fails) */
void __thiscall CVertList_PackTex1(CVertList *self, float posScale, float texScale);
/* into LTEX1_XBOX or LTEX1N_XBOX (with normals), LTEX2(N)_XBOX with a second set of texture coordinates and tex2 */
void __thiscall CVertList_ToXBox(CVertList *self, float posScale, float texScale, byte tex2);
/* LTEX1N_XBOX back to LTEX1N */
void __thiscall CVertList_FromXBox(CVertList *self, float posScale, float texScale);

/* out: the vertices of in along triangle strips of the triangles, the strips colored; 1 */
byte VertList_Strip(CVertList *out, const CVertList *in, const uint16_t *tris, int nTri);
/* the triangle that starts the longest strip, rotated so the strip starts with its first edge; -1 if none is left */
int Strip_FindFirst(uint16_t *tris, int nTri, int depth);
/* the length of the strip from triangle t over the edge a b */
int Strip_Length(int t, uint16_t *tris, int nTri, int depth, uint32_t a, uint32_t b);
/* a triangle left that has a neighbour (rotated to it), else the first one left, -1 if none */
int Strip_FindNew(uint16_t *tris, int nTri, int depth);
/* the free triangle with the edge a b (b a for odd depth), *side the corner after it */
int Strip_Next(const uint16_t *tris, int nTri, uint32_t a, uint32_t b, int depth, int *side);
void Tri_Mark(uint16_t *tri);
void Tri_Unmark(uint16_t *tri);
/* the corners turned n times */
void Tri_Rotate(uint16_t *tri, int n);

/* the triangles sorted for the vertex cache, the hits before and after logged; 1 */
byte VertCache_Optimize(uint16_t *tris, int nTri);
void VertCache_Reset(void);
void VertCache_Add(int v);
/* an empty entry or the oldest */
int VertCache_Oldest(void);
int VertCache_Find(int v);
/* the qsort compare of VertCache_Optimize: the triangles with their vertices in the cache first */
int VertCache_Compare(const void *a, const void *b);

#endif
