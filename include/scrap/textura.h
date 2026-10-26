/*
 * The texture cache of the engine3d (original source Engine/Textura.cpp): CTextura, one entry per file name and
 * opacity map, kept in a single linked list with reference counts. An entry holds the file (a CBmp for BMP, TGA
 * and TEX, the raw file for PNG and DDS, loaded by D3DX) and its Direct3D 8 texture, which is created when it is
 * first used and released again when the device is lost or too many file textures are cached. The functions
 * return HRESULTs.
 */
#ifndef SCRAP_TEXTURA_H
#define SCRAP_TEXTURA_H

#include "scrap/bmp.h"

#define TEX_MAX_NOMBRE 0x104

#define TEX_E_INVALIDARG 0x80070057
#define TEX_E_OUTOFMEMORY 0x8007000e
#define TEX_E_FAIL 0x80004005
#define TEX_E_NOTFOUND 0x88760866	/* D3DERR_NOTFOUND */
#define TEX_E_NOTAVAILABLE 0x8876086a	/* D3DERR_NOTAVAILABLE */
#define TEX_E_INVALIDCALL 0x8876086c	/* D3DERR_INVALIDCALL */

/* CTextura.flags (the flags of Textura_Add) */
#define TEXF_NOMIPMAP 0x20
#define TEXF_FIXEDBPP 0x40		/* g_TexFixedBPP, no mipmaps, no DDS */
#define TEXF_NODDS 0x80
#define TEXF_DDS 0x100			/* SaveDDS may write a DDS version */

/* the table of the sprites of a texture (rva 0x29cf80.., not decompiled) */
typedef struct CMultiSprite {
	int i0;				/* 0x0 */
	int i4;				/* 0x4 */
	void *tabla;			/* 0x8 set once built */
	void *datos;			/* 0xc */
} CMultiSprite;
SD_SIZE(CMultiSprite, 0x10);

typedef struct CTextura {
	int Refs;			/* 0x000 */
	struct CTextura *sig;		/* 0x004 g_TexList */
	char Nombre[TEX_MAX_NOMBRE];	/* 0x008 */
	char Opac[TEX_MAX_NOMBRE];	/* 0x10c the opacity map, "" for none */
	int ancho;			/* 0x210 */
	int alto;			/* 0x214 */
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
	byte b245;			/* 0x245 */
	byte pad_246[2];
	CMultiSprite multi;		/* 0x248 */
} CTextura;
SD_SIZE(CTextura, 0x258);
SD_OFFSET(CTextura, ancho, 0x210);
SD_OFFSET(CTextura, tex, 0x228);
SD_OFFSET(CTextura, mipmap, 0x23c);
SD_OFFSET(CTextura, multi, 0x248);

#define g_TexList GAME_DATA(CTextura *, 0x4b2610)

CMultiSprite *__thiscall CMultiSprite_ctor(CMultiSprite *self);
void __thiscall CMultiSprite_dtor(CMultiSprite *self);
byte __thiscall CMultiSprite_Build(CMultiSprite *self, const char *name);

CTextura *__thiscall CTextura_ctor(CTextura *self, const char *name, const char *opac, int kind, uint32_t flags);
void __fastcall CTextura_dtor(CTextura *self);
/* the file: its kind from the name, a CBmp (pixels only with load) or the raw file */
int __thiscall CTextura_LoadFile(CTextura *self, byte load);
/* the device texture, its old one released */
int __thiscall CTextura_CreateD3D(CTextura *self, void *dev);
/* src scaled into mipmap level and the levels below */
int __thiscall CTextura_UploadLevel(CTextura *self, CBmp *src, int level);
/* the developer tool: orig as a DDS file next to the texture, through bmp/nvdxt.exe and bmp/stitch.exe */
int __thiscall CTextura_SaveDDS(CTextura *self, CBmp *orig, int level, byte keepExt);

/* the entry of name with opacity map opac (NULL or "" for none) */
CTextura *Textura_Lookup(const char *name, const char *opac);
/* a new entry unless there is one; the file is checked, not loaded */
int Textura_Add(const char *name, const char *opac, int kind, uint32_t flags);
/* Lookup with a reference */
CTextura *Textura_Find(const char *name, const char *opac);
/* the references now, -1 for NULL */
int Textura_AddRef(CTextura *t);
int Textura_Release(CTextura *t);
void Textura_Delete(CTextura *t);
int Textura_Remove(const char *name, const char *opac);
int Textura_DeleteAll(void);
/* the device texture, created when missing (not for the raw files) */
void *Textura_GetD3D(CTextura *t, void *dev);
void *Textura_GetD3DByName(const char *name, const char *opac);
/* GetD3D for every entry, also the raw files; frees one of the device textures of those when too many are cached */
void *Textura_Load(CTextura *t, void *dev);
/* the least recently used raw file texture when more than g_CachedTex have a device texture */
CTextura *Textura_FindPurge(void);
CMultiSprite *Textura_GetMultiSprite(CTextura *t, const char *name);
/* info[0] ancho, [1] alto, [2] flags, [3] alpha, [4] compressed */
int Textura_GetInfo(CTextura *t, int *info);
int Textura_ReleaseD3D(const char *name, const char *opac);
int Textura_ReleaseAllD3D(void);
int Textura_Recreate(const char *name, const char *opac, void *dev);
int Textura_RecreateAll(void *dev);

/* the D3DFORMAT for a texture file */
int Tex_Format(int bpp, byte alpha, byte palette, byte bump, byte compressed, byte unused);
const char *D3DFmt_Name(int fmt);
byte Tex_IsPower2(int n);
/* name ends in ext before its last extension ("a.alpha.tga") */
byte Ruta_HasSubExt(const char *name, const char *ext);
void Ruta_ToBackslash(char *ruta);
/* R and G to signed offsets around 0 (V8U8 style) */
void Bmp_ToBump(CBmp *bmp);
/* R and G to the DUDV pair, the smaller part in A */
void Bmp_ToDudv(CBmp *bmp);
/* the average color: 8 x 1 pixels, 4 darker, 3 as it is, 1 brighter */
void Bmp_ToRamp(CBmp *bmp);
/* bmp into a locked A8R8G8B8 IDirect3DSurface8 of the same size */
int Surface_FromBmp(void *surf, const CBmp *bmp);

#endif
