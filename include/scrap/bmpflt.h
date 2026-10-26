/*
 * CBmpFlt of the engine3d (original sources Engine/BmpFlt.cpp, engine3d/engine/bmpflt.h): an image of float RGBA
 * pixels, as the light maps are made. TXF is its file format: a TXF_cab header, the pixels, then extra bytes of
 * the caller.
 */
#ifndef SCRAP_BMPFLT_H
#define SCRAP_BMPFLT_H

#include "scrap/game.h"

typedef struct CBmpFlt {
	int ancho;			/* 0x0 */
	int alto;			/* 0x4 */
	float *img;			/* 0x8 4 floats a pixel, rows */
} CBmpFlt;
SD_SIZE(CBmpFlt, 0xc);

#define TXF_MAGIC 0x1a465854	/* "TXF\x1a" */
#define TXF_VERSION 1

typedef struct TXF_cab {
	int magic;			/* 0x00 */
	int version;			/* 0x04 */
	int ancho;			/* 0x08 */
	int alto;			/* 0x0c */
	int extra;			/* 0x10 bytes after the pixels */
	int pad[3];			/* 0x14 */
} TXF_cab;
SD_SIZE(TXF_cab, 0x20);

#define g_TXFCab (&GAME_DATA(TXF_cab, 0x4e734c))

/* TXF_Save: an existing file is only replaced with this flag */
#define TXF_OVERWRITE 1

/* 0, -1 with the error in the file log; flags are not used */
int TXF_Load(const char *name, CBmpFlt *dest, int flags, void *extra, int nbytes);
int TXF_Save(const char *name, const CBmpFlt *orig, byte flags, const void *extra, int nbytes);
void __thiscall CBmpFlt_Fill(CBmpFlt *self, const float *color);
/* only ".txf" files, else -1 */
int __thiscall CBmpFlt_Save(CBmpFlt *self, const char *name, byte flags, const void *extra, int nbytes);
int __thiscall CBmpFlt_Load(CBmpFlt *self, const char *name, int flags, void *extra, int nbytes);
/* c = b * (1 - exp(-c * a / b)) on all channels */
void __thiscall CBmpFlt_Exposure(CBmpFlt *self, float a, float b);
/* new pixels, not cleared */
void __thiscall CBmpFlt_Create(CBmpFlt *self, int ancho, int alto);
/* orig at (ox, oy) size w x h into (x, y), weights a and b in 1/256: c = orig * a + c * b */
void __thiscall CBmpFlt_Blend(CBmpFlt *self, int x, int y, const CBmpFlt *orig, int ox, int oy, int w, int h, int a,
	int b);
/* orig scaled to the size of self by averaging boxes, only the channels in mask (1 r, 2 g, 4 b, 8 a) */
void __thiscall CBmpFlt_Resample(CBmpFlt *self, const CBmpFlt *orig, uint32_t mask);
/* bmpflt.h: the pixel at x, y with CHECKs */
float *__thiscall CBmpFlt_Pixel(const CBmpFlt *self, uint32_t x, uint32_t y);

#endif
