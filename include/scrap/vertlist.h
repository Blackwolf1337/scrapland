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

#define g_VCacheCount g_VCacheAge[16]
#define g_VCacheHits g_VCacheAge[17]
#define g_VCacheMisses g_VCacheAge[18]
#define VCACHE_SIZE 16

CVertList *CVertList_ctor(CVertList *self);
/* also the destructor (two copies in the exe) */
void CVertList_Free(CVertList *self);
/* empty, in the format */
void CVertList_SetFormat(CVertList *self, int frmt);
/* n vertices; with pVert set (realloc) 25% more room */
void CVertList_Alloc(CVertList *self, int n, byte realloc);
/* an empty stub, returns 0 */
byte CVertList_Optimize(CVertList *self, void *tris, int nTri);
/* the coordinates itex of the reflection map in software: with map->bb 1 the reflected view direction (sphere
 * map), else the normal through world * view; then offset f10 and scale f18 of the map */
struct CMap;
void CVertList_EnvMapUV(CVertList *self, const matrix_t *world, const matrix_t *view,
	const struct CMap *map, int itex);
/* the first 4 fields */
void CVertList_Swap(CVertList *self, CVertList *other);
/* the "LFVF" block */
void CVertList_Read(CVertList *self, CFileIO *f);
void CVertList_Write(CVertList *self, CFileIO *f);

uint32_t VertList_FormatFVF(int frmt);
int VertList_FormatSize(int frmt);
const char *VertList_FormatName(int frmt);
uint32_t CVertList_FVF(CVertList *self);
int CVertList_Size(CVertList *self);
const char *CVertList_FormatName(CVertList *self);
vec3_t *VertList_UnpackNormal(vec3_t *out, uint32_t packed);

/* field f of vertex i, NULL if the format has none */
void *CVertList_GetField(CVertList *self, int f, uint32_t i);

/* vertex i of the format, with checks */
byte *CVertList_Tex1(CVertList *self, uint32_t i);
byte *CVertList_Tex2(CVertList *self, uint32_t i);
byte *CVertList_LTex1(CVertList *self, uint32_t i);
byte *CVertList_Tex1B1(CVertList *self, uint32_t i);
byte *CVertList_LTex2N(CVertList *self, uint32_t i);
byte *CVertList_LTex3(CVertList *self, uint32_t i);
byte *CVertList_LTex1N(CVertList *self, uint32_t i);
byte *CVertList_LTex2(CVertList *self, uint32_t i);
void CVertList_SetTex1(CVertList *self, uint32_t i, const void *v);
void CVertList_SetTex2(CVertList *self, uint32_t i, const void *v);
void CVertList_SetTex1B1(CVertList *self, uint32_t i, const void *v);
/* the position of vertex i (rva 0x212570, in TriMesh.c) */
vec3_t *CVertList_GetPos(CVertList *self, vec3_t *out, uint32_t i);

/* the diffuse colors: c & and | or */
void CVertList_SetColor(CVertList *self, uint32_t and, uint32_t or);
/* the box of the positions, from the empty box if reset */
void CVertList_GetExtents(CVertList *self, box_t *ext, byte reset);
/* positions and normals (TEX1, TEX2, TEX1_B1) by m, the normals made unit length if normalize */
void CVertList_Transform(CVertList *self, const matrix_t *m, byte normalize);

#endif
