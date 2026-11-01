/*
 * Engine/grafico2d.cpp of the engine3d: CGraphic2D, a textured screen rectangle (or a polygon of up to 64 vertices
 * in its space) with its own material, drawn as pretransformed vertices. Positions and sizes are pixels, or parts of
 * the 2D screen (Graphic2D_SetScreen) with the flags; x is scaled by SpriteScaleWidth around the middle of the
 * viewport.
 */
#ifndef SCRAP_GRAPHIC2D_H
#define SCRAP_GRAPHIC2D_H

#include "scrap/material.h"

#define MAX_VERT_2D 64

/* the flags of CGraphic2D_ctorTex and CGraphic2D_CreateMat */
enum {
	GRAPHIC2D_SCREEN = 0x001,	/* the position is a part of the screen */
	GRAPHIC2D_SCREEN_SIZE = 0x002,	/* the size too */
	GRAPHIC2D_MAP_BA = 0x004,		/* the map gets ba */
	GRAPHIC2D_MAT_3F = 0x008,		/* the material writes z (zWrite) */
	GRAPHIC2D_MAT_I40 = 0x010,		/* i40 of the material 2, else 8 */
	GRAPHIC2D_MAP_BC = 0x020,		/* bc of the map kept */
	GRAPHIC2D_MULTISPRITE = 0x040,	/* the frames of a multi sprite map are made */
	GRAPHIC2D_ATTRIB_20 = 0x080,	/* attrib 0x60, else 0x40 */
	GRAPHIC2D_OWN = 0x100,		/* the material is its own, not shared with an equal one of the list */
};

typedef struct CGraphic2D {
	float x, y;			/* 0x00 */
	float alignX, alignY;		/* 0x08 the part of the size left of and above the position */
	float scale[2];		/* 0x10 1 */
	float scale2[2];		/* 0x18 1, multiplied with scale */
	byte screen;			/* 0x20 GRAPHIC2D_SCREEN */
	byte screenSize;		/* 0x21 GRAPHIC2D_SCREEN_SIZE */
	byte centered;			/* 0x22 aligned at the middle */
	byte noTexture;		/* 0x23 the size is 1x1 instead of the size of the texture */
	byte own;			/* 0x24 GRAPHIC2D_OWN */
	byte rot;			/* 0x25 0..3 the corners of the texture turned, odd ones swap width and height */
	byte pad_26[2];
	float u0, v0, u1, v1;		/* 0x28 the part of the texture */
	uint32_t color;			/* 0x38 0xffffff */
	CMaterial *Mat;			/* 0x3c */
	CAnimMat *AnimMat;		/* 0x40 the own copy of the animation of the map */
} CGraphic2D;
SD_SIZE(CGraphic2D, 0x44);

/* a pretransformed vertex (D3DFVF_XYZRHW | DIFFUSE | SPECULAR | TEX1) */
#define D3DFVF_2D 0x1c4
typedef struct Vert2D {
	float x, y, z, rhw;		/* 0x00 */
	uint32_t color;			/* 0x10 */
	uint32_t specular;		/* 0x14 */
	float u, v;			/* 0x18 */
} Vert2D;
SD_SIZE(Vert2D, 0x20);

/* the 2D screen size and its inverses */
extern int g_Screen2DW;
extern int g_Screen2DH;
extern double g_Screen2DInvW;
extern double g_Screen2DInvH;
#define g_SpriteScaleWidth GAME_GLOBAL_IN(float, GVar_SpriteScaleWidth, 16)
#define g_Verts2D (GAME_ARRAY(Vert2D, g_Verts2D))	/* MAX_VERT_2D, the polygon of the last draw */

CGraphic2D *CGraphic2D_ctorTex(CGraphic2D *self, const char *tex, const char *opac, uint32_t flags);
CGraphic2D *CGraphic2D_ctor(CGraphic2D *self);
void CGraphic2D_dtor(CGraphic2D *self);
void CGraphic2D_Render(CGraphic2D *self);
/* type 0: the rectangle; else a primitive (4 list, 5 strip, 6 fan) of nVert positions in the space of the
 * rectangle with uv (NULL: from the screen position) and colors (NULL: the color) */
void CGraphic2D_Draw(CGraphic2D *self, const float *pos, const float *uv, const uint32_t *colors,
	uint32_t nVert, int type);
void Graphic2D_SetScreen(int w, int h);
void CGraphic2D_SetTexture(CGraphic2D *self, const char *tex, const char *opac, uint32_t flags);
void CGraphic2D_CreateMat(CGraphic2D *self, const char *tex, const char *opac, int src, int dst, byte alpha,
	uint32_t flags);
void CGraphic2D_Init(CGraphic2D *self);
/* the primitives of nVert vertices (4: triangle list, 5: strip, 6: fan) */
uint32_t Graphic2D_NumPrims(int type, uint32_t nVert);
void Graphic2D_GetScreen(int *w, int *h);

#endif
