/*
 * CBmp of the engine3d (original sources Engine/bmp.cpp, engine3d/engine/bmp.h): an image of RGBA bytes, one
 * uint32_t a pixel, R in the low byte. The float images of include/scrap/bmpflt.h have the same header, so the
 * helpers here serve all of them with the size of a pixel.
 */
#ifndef SCRAP_BMP_H
#define SCRAP_BMP_H

#include "scrap/game.h"

typedef struct CImagen {
	int ancho;			/* 0x0 */
	int alto;			/* 0x4 */
	void *img;			/* 0x8 rows of pixels */
} CImagen;
SD_SIZE(CImagen, 0xc);
typedef CImagen CBmp;

/* the pixel at x, y with CHECKs */
void *Img_Pixel(const CImagen *b, uint32_t x, uint32_t y, int bytes);
/* the originals free the pixels with the sized delete of one pixel */
void Img_Free(CImagen *b, int bytes);
/* new pixels, not cleared */
void Img_Create(CImagen *b, int ancho, int alto, int bytes);
/* the last 4 characters of name are ext, ignoring case (all of a shorter name) */
int Img_IsExt(const char *name, const char *ext);
/* w and h cut to both images; 0 if nothing is left */
int Img_Clip(const CImagen *self, int x, int y, const CImagen *orig, int ox, int oy, int *w, int *h);
/* the first source row or column of destination i in a box filter */
int Img_BoxStart(int i, float scale);
/* the start of the Resample methods: a missing image gets the size of orig, the same size with all channels is a
 * copy (then 1) */
int Img_ResampleCopy(CImagen *self, const CImagen *orig, uint32_t mask, int bytes);

/* 0xAARRGGBB colors: a + (b - a) * t in steps of 1/256 */
uint32_t Color_Lerp(uint32_t a, uint32_t b, float t);

uint32_t *__thiscall CBmp_Pixel(const CBmp *self, uint32_t x, uint32_t y);
void __thiscall CBmp_Create(CBmp *self, int ancho, int alto);
/* the constructor with a size (new pixels) and the deleting destructor, copies in Engine/Textura.cpp */
CBmp *__thiscall CBmp_ctor(CBmp *self, int ancho, int alto);
CBmp *__thiscall CBmp_Delete(CBmp *self, byte flags);
void __thiscall CBmp_Fill(CBmp *self, uint32_t color);
/* orig at (ox, oy) size w x h into (x, y), only the channels in mask (1 r, 2 g, 4 b, 8 a) */
void __thiscall CBmp_Copy(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, uint32_t mask);
/* 1 a horizontal 5 pixel blur of RGB, 2 a fire step (random bottom row, rising average), 3 RGB times 3/4 */
void __thiscall CBmp_Effect(CBmp *self, int kind);
/* c = b * (1 - exp(-c * a / b)) on all channels, rounded */
void __thiscall CBmp_Exposure(CBmp *self, float a, float b);
/* c = b * pow(c / b, 1 / g), rounded, nothing for g 1 */
void __thiscall CBmp_Gamma(CBmp *self, float g, float b);
/* the average of the channels in in goes to the channels in out */
void __thiscall CBmp_MixChannels(CBmp *self, uint32_t in, uint32_t out);
/* c = (orig * a + c * b) / 256, the weights limited to +-2^20 */
void __thiscall CBmp_Blend(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, int a, int b);
/* bilinear at u, v in 0..1 */
uint32_t *__thiscall CBmp_Sample(const CBmp *self, uint32_t *out, float u, float v);
/* 3x3 filters of the inner pixels */
void __fastcall CBmp_Sharpen(CBmp *self);
void __fastcall CBmp_Blur(CBmp *self);
/* orig scaled to the size of self: a copy, halved by 2x2 averages, or by averaging boxes; only the channels in mask */
void __thiscall CBmp_Resample(CBmp *self, const CBmp *orig, uint32_t mask);

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
int __thiscall CBmp_Load(CBmp *self, const char *name, uint32_t flags);
int __thiscall CBmp_Save(CBmp *self, const char *name, uint32_t flags);

#endif
