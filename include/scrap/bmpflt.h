/*
 * CBmpFlt and CBmpFlt2 of the engine3d (original sources Engine/BmpFlt.cpp, Engine/BmpFlt2.cpp and the headers
 * bmpflt.h, bmpflt2.h): images of float pixels, RGBA in CBmpFlt and RGB in CBmpFlt2, as the light maps are made.
 * TXF is their file format: a TXF_header header (version 1 RGBA, 2 RGB), the pixels, then extra bytes of the caller.
 */
#ifndef SCRAP_BMPFLT_H
#define SCRAP_BMPFLT_H

#include "scrap/bmp.h"

/* img: 4 floats a pixel, 3 in a CBmpFlt2 */
typedef CImage CBmpFlt;
typedef CImage CBmpFlt2;

#define TXF_MAGIC 0x1a465854	/* "TXF\x1a" */

typedef struct TXF_header {
	int magic;			/* 0x00 */
	int version;			/* 0x04 */
	int width;			/* 0x08 */
	int height;			/* 0x0c */
	int extra;			/* 0x10 bytes after the pixels */
	int pad[3];			/* 0x14 */
} TXF_header;

/* the header of each class */
#define TXF_CAB 0x4e734c
#define TXF2_CAB 0x4e736c
extern TXF_header g_TXFCab;
extern TXF_header g_TXFCab2;

/* TXF_Save: an existing file is only replaced with this flag */
#define TXF_OVERWRITE 1

/* 0, -1 with the error in the file log; flags are not used */
int TXF_Load(const char *name, CBmpFlt *dest, int flags, void *extra, int nbytes);
void CBmpFlt_Fill(CBmpFlt *self, const float *color);
/* only ".txf" files, else -1 */
int CBmpFlt_Save(CBmpFlt *self, const char *name, byte flags, const void *extra, int nbytes);
int CBmpFlt_Load(CBmpFlt *self, const char *name, int flags, void *extra, int nbytes);
/* c = b * (1 - exp(-c * a / b)) on all channels */
void CBmpFlt_Exposure(CBmpFlt *self, float a, float b);
/* new pixels, not cleared */
void CBmpFlt_Create(CBmpFlt *self, int width, int height);
/* orig at (ox, oy) size w x h into (x, y), weights a and b in 1/256: c = orig * a + c * b */
void CBmpFlt_Blend(CBmpFlt *self, int x, int y, const CBmpFlt *orig, int ox, int oy, int w, int h, int a,
	int b);
/* orig scaled to the size of self by averaging boxes, only the channels in mask (1 r, 2 g, 4 b, 8 a) */
void CBmpFlt_Resample(CBmpFlt *self, const CBmpFlt *orig, uint32_t mask);
/* bmpflt.h: the pixel at x, y with CHECKs */
float *CBmpFlt_Pixel(const CBmpFlt *self, uint32_t x, uint32_t y);

/* the same for RGB */
int TXF2_Load(const char *name, CBmpFlt2 *dest, int flags, void *extra, int nbytes);
void CBmpFlt2_Fill(CBmpFlt2 *self, const float *color);
/* all channels times k, nothing for 1 */
void CBmpFlt2_Scale(CBmpFlt2 *self, float k);
int CBmpFlt2_Save(CBmpFlt2 *self, const char *name, byte flags, const void *extra, int nbytes);
void CBmpFlt2_Exposure(CBmpFlt2 *self, float a, float b);
/* c = b * pow(c / b, 1 / g) with negative channels as 0, nothing for g 1 */
void CBmpFlt2_Gamma(CBmpFlt2 *self, float g, float b);
void CBmpFlt2_Create(CBmpFlt2 *self, int width, int height);
int CBmpFlt2_Load(CBmpFlt2 *self, const char *name, int flags, void *extra, int nbytes);
void CBmpFlt2_Blend(CBmpFlt2 *self, int x, int y, const CBmpFlt2 *orig, int ox, int oy, int w, int h,
	int a, int b);
/* only a copy of the same size with mask 0xf, anything else is a failed CHECK */
void CBmpFlt2_Resample(CBmpFlt2 *self, const CBmpFlt2 *orig, uint32_t mask);
float *CBmpFlt2_Pixel(const CBmpFlt2 *self, uint32_t x, uint32_t y);

#endif
