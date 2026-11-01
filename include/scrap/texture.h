/*
 * The texture cache of the engine3d (original source Engine/Textura.cpp): CTexture, one entry per file name and
 * opacity map, kept in a single linked list with reference counts. An entry holds the file (a CBmp for BMP, TGA
 * and TEX, the raw file for PNG and DDS, loaded by D3DX) and its Direct3D 8 texture, which is created when it is
 * first used and released again when the device is lost or too many file textures are cached. The functions
 * return HRESULTs.
 */
#ifndef SCRAP_TEXTURE_H
#define SCRAP_TEXTURE_H

#include "scrap/bmp.h"

#define TEX_MAX_NAME 0x104

#define TEX_E_INVALIDARG 0x80070057
#define TEX_E_OUTOFMEMORY 0x8007000e
#define TEX_E_FAIL 0x80004005
#define TEX_E_NOTFOUND 0x88760866	/* D3DERR_NOTFOUND */
#define TEX_E_NOTAVAILABLE 0x8876086a	/* D3DERR_NOTAVAILABLE */
#define TEX_E_INVALIDCALL 0x8876086c	/* D3DERR_INVALIDCALL */

/* CTexture.flags (the flags of Texture_Add) */
#define TEXF_NOMIPMAP 0x20
#define TEXF_FIXEDBPP 0x40		/* g_TexFixedBPP, no mipmaps, no DDS */
#define TEXF_NODDS 0x80
#define TEXF_DDS 0x100			/* SaveDDS may write a DDS version */

/* Engine/MultiSprite.cpp (src/game/engine3d/Engine/MultiSprite.c): the sprites of a texture, rectangles in pixels
 * numbered by the alpha of the &N.index.bmp beside it (sprite 0: the whole texture), kept in a .mst file */
typedef struct MSprite {
	uint16_t x, y;			/* 0x0 */
	uint16_t cx, cy;		/* 0x4 */
} MSprite;
SD_SIZE(MSprite, 0x8);

typedef struct CMultiSprite {
	int width;			/* 0x0 of the texture */
	int height;			/* 0x4 */
	int n;				/* 0x8 the sprites (CTable) */
	MSprite *p;			/* 0xc */
} CMultiSprite;
SD_SIZE(CMultiSprite, 0x10);

#define MST_VER 100
#define g_BuildMST GAME_GLOBAL_IN(int, GVar_BuildMST, 16)	/* BuildMST: load the .mst files */

typedef struct CTexture {
	int Refs;			/* 0x000 */
	struct CTexture *next;		/* 0x004 g_TexList */
	char Name[TEX_MAX_NAME];	/* 0x008 */
	char Opac[TEX_MAX_NAME];	/* 0x10c the opacity map, "" for none */
	int width;			/* 0x210 */
	int height;			/* 0x214 */
	int kind;			/* 0x218 */
	int bpp;			/* 0x21c */
	uint32_t flags;			/* 0x220 TEXF_* */
	int alpha;			/* 0x224 */
	void *tex;			/* 0x228 IDirect3DTexture8 */
	CBmp *bitmap;			/* 0x22c BMP, TGA, TEX */
	void *PngData;			/* 0x230 PNG, DDC: the file, the texture is created from it again */
	uint32_t PngSize;		/* 0x234 */
	int frame;			/* 0x238 the frame it was last used */
	byte mipmap;			/* 0x23c */
	byte bump;			/* 0x23d ".bump" */
	byte dudv;			/* 0x23e ".dudv" */
	byte cube;			/* 0x23f ".cube" */
	byte compressed;		/* 0x240 DDS and its variants */
	byte failed;			/* 0x241 never set in Textura.cpp */
	byte ddx;			/* 0x242 */
	byte ddy;			/* 0x243 */
	byte ddv;			/* 0x244 */
	byte unused_245;			/* 0x245 */
	byte pad_246[2];
	CMultiSprite multi;		/* 0x248 */
} CTexture;
SD_SIZE(CTexture, 0x258);
SD_OFFSET(CTexture, width, 0x210);
SD_OFFSET(CTexture, tex, 0x228);
SD_OFFSET(CTexture, mipmap, 0x23c);
SD_OFFSET(CTexture, multi, 0x248);

#define g_TexList GAME_GLOBAL(CTexture *, g_TexList)

CMultiSprite *CMultiSprite_ctor(CMultiSprite *self);
void CMultiSprite_dtor(CMultiSprite *self);
/* the table from the .mst of the texture name (when not older), else built and saved; 0 if it could not be saved */
byte CMultiSprite_Build(CMultiSprite *self, const char *name);
/* the table from the &U&D&N.index.bmp of the texture name */
byte CMultiSprite_BuildTable(CMultiSprite *self, const char *name);
/* the size of sprite i in pixels, the texture's for 0 or without sprites (then 0) */
byte CMultiSprite_GetSize(const CMultiSprite *self, uint32_t i, uint32_t *cx, uint32_t *cy);
/* the texture coordinates of sprite i, 0..1 for 0 or without sprites (then 0) */
byte CMultiSprite_GetUV(const CMultiSprite *self, uint32_t i, float *texMin, float *texMax);
byte CMultiSprite_Load(CMultiSprite *self, const char *name);
byte CMultiSprite_Save(const CMultiSprite *self, const char *name);

CTexture *CTexture_ctor(CTexture *self, const char *name, const char *opac, int kind, uint32_t flags);
void CTexture_dtor(CTexture *self);
/* the file: its kind from the name, a CBmp (pixels only with load) or the raw file */
int CTexture_LoadFile(CTexture *self, byte load);
/* the device texture, its old one released */
int CTexture_CreateD3D(CTexture *self, void *dev);
/* src scaled into mipmap level and the levels below */
int CTexture_UploadLevel(CTexture *self, CBmp *src, int level);
/* the developer tool: orig as a DDS file next to the texture, through bmp/nvdxt.exe and bmp/stitch.exe */
int CTexture_SaveDDS(CTexture *self, CBmp *orig, int level, byte keepExt);

/* the entry of name with opacity map opac (NULL or "" for none) */
CTexture *Texture_Lookup(const char *name, const char *opac);
/* a new entry unless there is one; the file is checked, not loaded */
int Texture_Add(const char *name, const char *opac, int kind, uint32_t flags);
/* Lookup with a reference */
CTexture *Texture_Find(const char *name, const char *opac);
/* the references now, -1 for NULL */
int Texture_AddRef(CTexture *t);
int Texture_Release(CTexture *t);
void Texture_Delete(CTexture *t);
int Texture_Remove(const char *name, const char *opac);
int Texture_DeleteAll(void);
/* the device texture, created when missing (not for the raw files) */
void *Texture_GetD3D(CTexture *t, void *dev);
void *Texture_GetD3DByName(const char *name, const char *opac);
/* GetD3D for every entry, also the raw files; frees one of the device textures of those when too many are cached */
void *Texture_Load(CTexture *t, void *dev);
/* the least recently used raw file texture when more than g_CachedTex have a device texture */
CTexture *Texture_FindPurge(void);
CMultiSprite *Texture_GetMultiSprite(CTexture *t, const char *name);
/* info[0] width, [1] alto, [2] flags, [3] alpha, [4] compressed */
int Texture_GetInfo(CTexture *t, int *info);
int Texture_ReleaseD3D(const char *name, const char *opac);
int Texture_ReleaseAllD3D(void);
int Texture_Recreate(const char *name, const char *opac, void *dev);
int Texture_RecreateAll(void *dev);

/* the D3DFORMAT for a texture file */
int Tex_Format(int bpp, byte alpha, byte palette, byte bump, byte compressed, byte unused);
const char *D3DFmt_Name(int fmt);
byte Tex_IsPower2(int n);
/* name ends in ext before its last extension ("a.alpha.tga") */
byte Path_HasSubExt(const char *name, const char *ext);
void Path_ToBackslash(char *path);
/* R and G to signed offsets around 0 (V8U8 style) */
void Bmp_ToBump(CBmp *bmp);
/* R and G to the DUDV pair, the smaller part in A */
void Bmp_ToDudv(CBmp *bmp);
/* the average color: 8 x 1 pixels, 4 darker, 3 as it is, 1 brighter */
void Bmp_ToRamp(CBmp *bmp);
/* bmp into a locked A8R8G8B8 IDirect3DSurface8 of the same size */
int Surface_FromBmp(void *surf, const CBmp *bmp);

#endif
