/*
 * The texture cache of the engine3d (original source Engine/Textura.cpp): CTexture, one entry per file name and
 * opacity map, kept in a single linked list with reference counts. An entry holds the file (a CBmp for BMP, TGA
 * and TEX, the raw file for PNG and DDS, loaded by D3DX) and its Direct3D 8 texture, which is created when it is
 * first used and released again when the device is lost or too many file textures are cached. The functions
 * return HRESULTs.
 */
#ifndef SCRAP_TEXTURE_H
#define SCRAP_TEXTURE_H

#include "scrap/table.h"
#include "scrap/gvar.h"
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
	uint16_t x, y;
	uint16_t cx, cy;
} MSprite;

typedef struct CMultiSprite {
	int width;			/* of the texture */
	int height;
	union {				/* a CTable: the casts (CTable *)&n hold on every platform */
		CTable t_n;
		struct {
			int n;				/* the sprites (CTable) */
			MSprite *p;
		};
	};
} CMultiSprite;

#define MST_VER 100

typedef struct CTexture {
	int Refs;
	struct CTexture *next;		/* g_TexList */
	char Name[TEX_MAX_NAME];
	char Opac[TEX_MAX_NAME];	/* the opacity map, "" for none */
	int width;
	int height;
	int kind;
	int bpp;
	uint32_t flags;			/* TEXF_* */
	int alpha;
	void *tex;			/* IDirect3DTexture8 */
	CBmp *bitmap;			/* BMP, TGA, TEX */
	void *PngData;			/* PNG, DDC: the file, the texture is created from it again */
	uint32_t PngSize;
	int frame;			/* the frame it was last used */
	byte mipmap;
	byte bump;			/* ".bump" */
	byte dudv;			/* ".dudv" */
	byte cube;			/* ".cube" */
	byte compressed;		/* DDS and its variants */
	byte failed;			/* never set in Textura.cpp */
	byte ddx;
	byte ddy;
	byte ddv;
	byte unused_245;
	byte pad_246[2];
	CMultiSprite multi;
} CTexture;

CMultiSprite *CMultiSprite_ctor(CMultiSprite *self);
void CMultiSprite_dtor(CMultiSprite *self);
/* the table from the .mst of the texture name (when not older), else built and saved; 0 if it could not be saved */
byte CMultiSprite_Build(CMultiSprite *self, const char *name);
/* the size of sprite i in pixels, the texture's for 0 or without sprites (then 0) */
byte CMultiSprite_GetSize(const CMultiSprite *self, uint32_t i, uint32_t *cx, uint32_t *cy);
/* the texture coordinates of sprite i, 0..1 for 0 or without sprites (then 0) */
byte CMultiSprite_GetUV(const CMultiSprite *self, uint32_t i, float *texMin, float *texMax);
byte CMultiSprite_Load(CMultiSprite *self, const char *name);
byte CMultiSprite_Save(const CMultiSprite *self, const char *name);

/* a new entry unless there is one; the file is checked, not loaded */
int Texture_Add(const char *name, const char *opac, int kind, uint32_t flags);
/* Lookup with a reference */
CTexture *Texture_Find(const char *name, const char *opac);
int Texture_Release(CTexture *t);
void Texture_Delete(CTexture *t);
int Texture_DeleteAll(void);
/* the device texture, created when missing (not for the raw files) */
void *Texture_GetD3D(CTexture *t, void *dev);
/* GetD3D for every entry, also the raw files; frees one of the device textures of those when too many are cached */
void *Texture_Load(CTexture *t, void *dev);
CMultiSprite *Texture_GetMultiSprite(CTexture *t, const char *name);
/* info[0] width, [1] alto, [2] flags, [3] alpha, [4] compressed */
int Texture_GetInfo(CTexture *t, int *info);
int Texture_ReleaseAllD3D(void);

const char *D3DFmt_Name(int fmt);

#endif
