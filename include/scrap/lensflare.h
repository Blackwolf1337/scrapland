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
	byte on;
	byte pad_1[3];
	int RenderIndex;		/* in gLensFlareRender, -1 */
	vec3_t pos;
	float intens;			/* 1, the alpha of the sprites */
	float scale;			/* 1, the size of the sprites */
	int type;			/* the pattern 0..3 */
	float margin;			/* 0.2, the part beyond the screen edges in which it fades out */
	float sx, sy;			/* the screen position of the last render (-1..1) */
	byte relative;			/* pos relative to the eye */
	byte pad_2d[3];
	float traceInterval;		/* seconds between the traces against the floor, 0: none */
	float traceTimer;
	float transp;			/* 1, the fade of the traces 0..1 */
	byte visible;			/* the floor did not hide it at the last trace */
	byte pad_3d[3];
	CGroundHit hit;			/* the hit cache of the trace */
	uint32_t lastTime;		/* g_TimeMsF of the last render */
} CLensFlare;

/* a vertex of the sprites (D3DFVF_XYZ | DIFFUSE | SPECULAR | TEX1) */
#define D3DFVF_LENSFLARE 0x1c2
typedef struct VertLensFlare {
	float x, y, z;
	uint32_t color;
	uint32_t specular;
	float u, v;
} VertLensFlare;

CLensFlare *CLensFlare_ctor(CLensFlare *self);
void CLensFlare_dtor(CLensFlare *self);
void CLensFlare_SetOn(CLensFlare *self, byte on);
/* all off, the list empty; 0 */
int LensFlare_ClearAll(void);
int LensFlare_Release(void);
/* the material of the sprites; 0 */
int LensFlare_Init(void);
int LensFlare_InitList(void);
/* the flares of the list with the view and projection of the device, in screen space */
void LensFlare_RenderAll(void);
void CLensFlare_SetIntens(CLensFlare *self, float v);
void CLensFlare_SetTraceInterval(CLensFlare *self, float v);
void CLensFlare_SetPos(CLensFlare *self, vec3_t pos);
void CLensFlare_SetScale(CLensFlare *self, float v);
void CLensFlare_SetType(CLensFlare *self, int type);
void CLensFlare_SetRelative(CLensFlare *self, byte relative);

#endif
