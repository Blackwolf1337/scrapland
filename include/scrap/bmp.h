/*
 * CBmp of the engine3d (original sources Engine/bmp.cpp, engine3d/engine/bmp.h): an image of RGBA bytes, one
 * uint32_t a pixel, R in the low byte. The float images of include/scrap/bmpflt.h have the same header, so the
 * helpers here serve all of them with the size of a pixel.
 */
#ifndef SCRAP_BMP_H
#define SCRAP_BMP_H

#include "scrap/game.h"

typedef struct CImage {
	int width;			/* 0x0 */
	int height;			/* 0x4 */
	void *img;			/* 0x8 rows of pixels */
} CImage;
typedef CImage CBmp;

/* the pixel at x, y with CHECKs */
void *Img_Pixel(const CImage *b, uint32_t x, uint32_t y, int bytes);
/* the originals free the pixels with the sized delete of one pixel */
void Img_Free(CImage *b, int bytes);
/* new pixels, not cleared */
void Img_Create(CImage *b, int width, int height, int bytes);
/* the last 4 characters of name are ext, ignoring case (all of a shorter name) */
int Img_IsExt(const char *name, const char *ext);
/* w and h cut to both images; 0 if nothing is left */
int Img_Clip(const CImage *self, int x, int y, const CImage *orig, int ox, int oy, int *w, int *h);
/* the first source row or column of destination i in a box filter */
int Img_BoxStart(int i, float scale);
/* the start of the Resample methods: a missing image gets the size of orig, the same size with all channels is a
 * copy (then 1) */
int Img_ResampleCopy(CImage *self, const CImage *orig, uint32_t mask, int bytes);

/* 0xAARRGGBB colors: a + (b - a) * t in steps of 1/256 */
uint32_t Color_Lerp(uint32_t a, uint32_t b, float t);

uint32_t *CBmp_Pixel(const CBmp *self, uint32_t x, uint32_t y);
void CBmp_Create(CBmp *self, int width, int height);
/* the constructor with a size (new pixels) and the deleting destructor, copies in Engine/Textura.cpp */
CBmp *CBmp_ctor(CBmp *self, int width, int height);
CBmp *CBmp_Delete(CBmp *self, byte flags);
/* the size 0, the pixels freed (also CBackground2D, Engine/fondo2d.cpp) */
void CBmp_dtor(CBmp *self);
void CBmp_Fill(CBmp *self, uint32_t color);
/* orig at (ox, oy) size w x h into (x, y), only the channels in mask (1 r, 2 g, 4 b, 8 a) */
void CBmp_Copy(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, uint32_t mask);
/* 1 a horizontal 5 pixel blur of RGB, 2 a fire step (random bottom row, rising average), 3 RGB times 3/4 */
void CBmp_Effect(CBmp *self, int kind);
/* c = b * (1 - exp(-c * a / b)) on all channels, rounded */
void CBmp_Exposure(CBmp *self, float a, float b);
/* c = b * pow(c / b, 1 / g), rounded, nothing for g 1 */
void CBmp_Gamma(CBmp *self, float g, float b);
/* the average of the channels in in goes to the channels in out */
void CBmp_MixChannels(CBmp *self, uint32_t in, uint32_t out);
/* c = (orig * a + c * b) / 256, the weights limited to +-2^20 */
void CBmp_Blend(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, int a, int b);
/* bilinear at u, v in 0..1 */
uint32_t *CBmp_Sample(const CBmp *self, uint32_t *out, float u, float v);
/* 3x3 filters of the inner pixels */
void CBmp_Sharpen(CBmp *self);
void CBmp_Blur(CBmp *self);
/* orig scaled to the size of self: a copy, halved by 2x2 averages, or by averaging boxes; only the channels in mask */
void CBmp_Resample(CBmp *self, const CBmp *orig, uint32_t mask);

/* the image files: 0, -1 with errno and, for a bad file, a line in the file log. Loading reads through the packs,
 * saving writes a file below the game directory. BMP: 8 (palette), 24 or 32 bits, saved with 24; TEX: the pixels as
 * they are; TGA: 24 or 32 bits, RLE or not, saved RLE */
#define IMG_OVERWRITE 1		/* save: replace an existing file */
#define IMG_ALPHA 2		/* TGA save: 32 bits */
#define IMG_INDEX_ALPHA 0x100	/* BMP load: the palette index as alpha */
int BMP_Load(const char *name, CBmp *dest, uint32_t flags);
int TEX_Load(const char *name, CBmp *dest, uint32_t flags);
int TGA_Load(const char *name, CBmp *dest, uint32_t flags);
int BMP_Save(const char *name, const CBmp *orig, uint32_t flags);
int TEX_Save(const char *name, const CBmp *orig, uint32_t flags);
int TGA_Save(const char *name, const CBmp *orig, uint32_t flags);
/* by the extension: .tex, .tga, else BMP */
int CBmp_Load(CBmp *self, const char *name, uint32_t flags);
int CBmp_Save(CBmp *self, const char *name, uint32_t flags);

/* packed by #pragma pack, which Ghidra's C parser knows too (docs/ghidra.md) */
#pragma pack(push, 1)
typedef struct {
	uint16_t type;			/* 0x0 "BM" */
	uint32_t size;			/* 0x2 */
	uint32_t reserved;		/* 0x6 */
	uint32_t offBits;		/* 0xa */
} bmpFileCab_t;
#pragma pack(pop)

typedef struct {
	uint32_t size;			/* 0x00 */
	int32_t width;			/* 0x04 */
	int32_t height;			/* 0x08 bottom up */
	uint16_t planes;		/* 0x0c */
	uint16_t bits;			/* 0x0e */
	uint32_t compression;		/* 0x10 */
	uint32_t sizeImage;		/* 0x14 */
	int32_t xPels;			/* 0x18 */
	int32_t yPels;			/* 0x1c */
	uint32_t clrUsed;		/* 0x20 */
	uint32_t clrImportant;		/* 0x24 */
} bmpInfoCab_t;

typedef struct {
	uint32_t magic;			/* 0x00 "TEX\x1a" */
	int32_t version;		/* 0x04 1 */
	int32_t width;			/* 0x08 */
	int32_t height;			/* 0x0c */
	uint32_t pad[4];		/* 0x10 */
} texCab_t;

#pragma pack(push, 1)
typedef struct {
	byte idLength;			/* 0x00 */
	byte cmapType;			/* 0x01 */
	byte type;			/* 0x02 2 true color, 10 RLE */
	byte cmap[5];			/* 0x03 */
	uint16_t x, y;			/* 0x08 */
	uint16_t width, height;		/* 0x0c */
	byte bits;			/* 0x10 */
	byte desc;			/* 0x11 0x20 top down */
} tgaCab_t;
#pragma pack(pop)

#endif
