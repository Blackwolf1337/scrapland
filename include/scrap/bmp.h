/*
 * CBmp of the engine3d (original sources Engine/bmp.cpp, engine3d/engine/bmp.h): an image of RGBA bytes, one
 * uint32_t a pixel, R in the low byte. The float images of include/scrap/bmpflt.h have the same header, so the
 * helpers here serve all of them with the size of a pixel.
 */
#ifndef SCRAP_BMP_H
#define SCRAP_BMP_H

#include "scrap/game.h"

typedef struct CImage {
	int width;
	int height;
	void *img;			/* rows of pixels */
} CImage;
typedef CImage CBmp;

/* the pixel at x, y with CHECKs */
void *Img_Pixel(const CImage *b, uint32_t x, uint32_t y, int bytes);
/* the originals free the pixels with the sized delete of one pixel */
void Img_Free(CImage *b);
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

uint32_t *CBmp_Pixel(const CBmp *self, uint32_t x, uint32_t y);
void CBmp_Fill(CBmp *self, uint32_t color);
/* orig at (ox, oy) size w x h into (x, y), only the channels in mask (1 r, 2 g, 4 b, 8 a) */
void CBmp_Copy(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, uint32_t mask);
/* the average of the channels in in goes to the channels in out */
void CBmp_MixChannels(CBmp *self, uint32_t in, uint32_t out);
/* c = (orig * a + c * b) / 256, the weights limited to +-2^20 */
void CBmp_Blend(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, int a, int b);
/* bilinear at u, v in 0..1 */
uint32_t *CBmp_Sample(const CBmp *self, uint32_t *out, float u, float v);
/* 3x3 filters of the inner pixels */
void CBmp_Sharpen(CBmp *self);
/* orig scaled to the size of self: a copy, halved by 2x2 averages, or by averaging boxes; only the channels in mask */
void CBmp_Resample(CBmp *self, const CBmp *orig, uint32_t mask);

/* the image files: 0, -1 with errno and, for a bad file, a line in the file log. Loading reads through the packs,
 * saving writes a file below the game directory. BMP: 8 (palette), 24 or 32 bits, saved with 24; TEX: the pixels as
 * they are; TGA: 24 or 32 bits, RLE or not, saved RLE */
#define IMG_OVERWRITE 1		/* save: replace an existing file */
#define IMG_ALPHA 2		/* TGA save: 32 bits */
#define IMG_INDEX_ALPHA 0x100	/* BMP load: the palette index as alpha */
/* by the extension: .tex, .tga, else BMP */
int CBmp_Load(CBmp *self, const char *name, uint32_t flags);
int CBmp_Save(CBmp *self, const char *name, uint32_t flags);

/* packed by #pragma pack, which Ghidra's C parser knows too (docs/ghidra.md) */
#pragma pack(push, 1)
typedef struct {
	uint16_t type;			/* "BM" */
	uint32_t size;
	uint32_t reserved;
	uint32_t offBits;
} bmpFileCab_t;
#pragma pack(pop)

typedef struct {
	uint32_t size;
	int32_t width;
	int32_t height;			/* bottom up */
	uint16_t planes;
	uint16_t bits;
	uint32_t compression;
	uint32_t sizeImage;
	int32_t xPels;
	int32_t yPels;
	uint32_t clrUsed;
	uint32_t clrImportant;
} bmpInfoCab_t;

typedef struct {
	uint32_t magic;			/* "TEX\x1a" */
	int32_t version;		/* 1 */
	int32_t width;
	int32_t height;
	uint32_t pad[4];
} texCab_t;

#pragma pack(push, 1)
typedef struct {
	byte idLength;
	byte cmapType;
	byte type;			/* 2 true color, 10 RLE */
	byte cmap[5];
	uint16_t x, y;
	uint16_t width, height;
	byte bits;
	byte desc;			/* 0x20 top down */
} tgaCab_t;
#pragma pack(pop)

#endif
