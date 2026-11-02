/*
 * Engine/LensFlare.cpp of the engine3d: CLensFlare, the lens flare of a point (the sun, lights of the game effects):
 * a row of sprites of BMP/LensFlare.bmp along the line from the screen position of the point through the center of
 * the screen, in four patterns (type). It fades towards the edges of the screen and, with a trace interval, in and
 * out by whether the floor hides the point. Up to MAX_LENSFLARE_RENDER flares that are on are in the render list.
 */
#ifndef SCRAP_LENSFLARE_H
#define SCRAP_LENSFLARE_H

#include "scrap/gvar.h"
#include "scrap/ground.h"
#include "scrap/material.h"
#include "scrap/mathlib.h"

#define MAX_LENSFLARE_RENDER 8

typedef struct CLensFlare {
	byte on;			/* 0x00 */
	byte pad_1[3];
	int RenderIndex;		/* 0x04 in gLensFlareRender, -1 */
	vec3_t pos;			/* 0x08 */
	float intens;			/* 0x14 1, the alpha of the sprites */
	float scale;			/* 0x18 1, the size of the sprites */
	int type;			/* 0x1c the pattern 0..3 */
	float margin;			/* 0x20 0.2, the part beyond the screen edges in which it fades out */
	float sx, sy;			/* 0x24 the screen position of the last render (-1..1) */
	byte relative;			/* 0x2c pos relative to the eye */
	byte pad_2d[3];
	float traceInterval;		/* 0x30 seconds between the traces against the floor, 0: none */
	float traceTimer;		/* 0x34 */
	float transp;			/* 0x38 1, the fade of the traces 0..1 */
	byte visible;			/* 0x3c the floor did not hide it at the last trace */
	byte pad_3d[3];
	CGroundHit hit;			/* 0x40 the hit cache of the trace */
	uint32_t lastTime;		/* 0x4c g_TimeMsF of the last render */
} CLensFlare;
SD_SIZE(CLensFlare, 0x50);

/* a vertex of the sprites (D3DFVF_XYZ | DIFFUSE | SPECULAR | TEX1) */
#define D3DFVF_LENSFLARE 0x1c2
typedef struct VertLensFlare {
	float x, y, z;			/* 0x00 */
	uint32_t color;			/* 0x0c */
	uint32_t specular;		/* 0x10 */
	float u, v;			/* 0x14 */
} VertLensFlare;
SD_SIZE(VertLensFlare, 0x1c);

extern CMaterial *g_LensFlareMat;
extern cGVar GVar_R_LensFlare;
#define g_LensFlares (GVar_R_LensFlare.value.i)		/* the engine variable that draws them */
extern CLensFlare * g_LensFlareRender[8];	/* MAX_LENSFLARE_RENDER */
extern int g_nLensFlareRender;
extern VertLensFlare g_LensFlareVerts[4];	/* 4 */

CLensFlare *CLensFlare_ctor(CLensFlare *self);
void CLensFlare_dtor(CLensFlare *self);
/* into or out of the render list by on */
void LensFlare_Update(CLensFlare *lf);
void CLensFlare_SetOn(CLensFlare *self, byte on);
void LensFlare_Set(CLensFlare *lf, byte on);
/* all off, the list empty; 0 */
int LensFlare_ClearAll(void);
int LensFlare_Release(void);
/* the material of the sprites; 0 */
int LensFlare_Init(void);
int LensFlare_InitList(void);
/* the flares of the list with the view and projection of the device, in screen space */
void LensFlare_RenderAll(void);
void CLensFlare_Render(CLensFlare *self, matrix_t view, matrix_t proj);
/* a sprite at dist along the line through the center (0 the point, 1 the center), size and color */
void CLensFlare_DrawSprite(CLensFlare *self, float dist, uint32_t sprite, float size, float r, float g,
	float b, float a);
void CLensFlare_SetIntens(CLensFlare *self, float v);
void CLensFlare_SetTraceInterval(CLensFlare *self, float v);
void CLensFlare_SetMargin(CLensFlare *self, float v);
void CLensFlare_SetPos(CLensFlare *self, vec3_t pos);
void CLensFlare_SetScale(CLensFlare *self, float v);
void CLensFlare_SetType(CLensFlare *self, int type);
void CLensFlare_SetRelative(CLensFlare *self, byte relative);

#endif
