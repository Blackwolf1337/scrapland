/*
 * Engine/grafico2d.cpp of the engine3d: CGrafico2D, a textured screen rectangle (or a polygon of up to 64 vertices
 * in its space) with its own material, drawn as pretransformed vertices. Positions and sizes are pixels, or parts of
 * the 2D screen (Grafico2D_SetPantalla) with the flags; x is scaled by SpriteScaleWidth around the middle of the
 * viewport.
 */
#ifndef SCRAP_GRAFICO2D_H
#define SCRAP_GRAFICO2D_H

#include "scrap/material.h"

#define MAX_VERT_2D 64

/* the flags of CGrafico2D_ctorTex and CGrafico2D_CreateMat */
enum {
	GRAF2D_PANTALLA = 0x001,	/* the position is a part of the screen */
	GRAF2D_TAM_PANTALLA = 0x002,	/* the size too */
	GRAF2D_MAPA_BA = 0x004,		/* the map gets ba */
	GRAF2D_MAT_3F = 0x008,		/* the material gets w3e high byte 1 */
	GRAF2D_MAT_I40 = 0x010,		/* i40 of the material 2, else 8 */
	GRAF2D_MAPA_BC = 0x020,		/* bc of the map kept */
	GRAF2D_MULTISPRITE = 0x040,	/* the frames of a multi sprite map are made */
	GRAF2D_ATRIB_20 = 0x080,	/* atrib 0x60, else 0x40 */
	GRAF2D_PROPIO = 0x100,		/* the material is its own, not shared with an equal one of the list */
};

typedef struct CGrafico2D {
	float x, y;			/* 0x00 */
	float alignX, alignY;		/* 0x08 the part of the size left of and above the position */
	float escala[2];		/* 0x10 1 */
	float escala2[2];		/* 0x18 1, multiplied with escala */
	byte pantalla;			/* 0x20 GRAF2D_PANTALLA */
	byte tamPantalla;		/* 0x21 GRAF2D_TAM_PANTALLA */
	byte centrado;			/* 0x22 aligned at the middle */
	byte sinTextura;		/* 0x23 the size is 1x1 instead of the size of the texture */
	byte propio;			/* 0x24 GRAF2D_PROPIO */
	byte rot;			/* 0x25 0..3 the corners of the texture turned, odd ones swap width and height */
	byte pad_26[2];
	float u0, v0, u1, v1;		/* 0x28 the part of the texture */
	uint32_t color;			/* 0x38 0xffffff */
	CMaterial *Mat;			/* 0x3c */
	CAnimMat *AnimMat;		/* 0x40 the own copy of the animation of the map */
} CGrafico2D;
SD_SIZE(CGrafico2D, 0x44);

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
#define g_Pantalla2DW GAME_DATA(int, 0x432598)
#define g_Pantalla2DH GAME_DATA(int, 0x43259c)
#define g_Pantalla2DInvW GAME_DATA(double, 0x4325a0)
#define g_Pantalla2DInvH GAME_DATA(double, 0x4325a8)
#define g_SpriteScaleWidth GAME_DATA(float, 0x4b1d0c)
#define g_Verts2D ((Vert2D *)Game_Ptr(0x4b1d18))	/* MAX_VERT_2D, the polygon of the last draw */

CGrafico2D *__thiscall CGrafico2D_ctorTex(CGrafico2D *self, const char *tex, const char *opac, uint32_t flags);
CGrafico2D *__fastcall CGrafico2D_ctor(CGrafico2D *self);
void __fastcall CGrafico2D_dtor(CGrafico2D *self);
void __fastcall CGrafico2D_Render(CGrafico2D *self);
/* tipo 0: the rectangle; else a primitive (4 list, 5 strip, 6 fan) of nVert positions in the space of the
 * rectangle with uv (NULL: from the screen position) and colors (NULL: the color) */
void __thiscall CGrafico2D_Draw(CGrafico2D *self, const float *pos, const float *uv, const uint32_t *colors,
	uint32_t nVert, int tipo);
void Grafico2D_SetPantalla(int w, int h);
void __thiscall CGrafico2D_SetTexture(CGrafico2D *self, const char *tex, const char *opac, uint32_t flags);
void __thiscall CGrafico2D_CreateMat(CGrafico2D *self, const char *tex, const char *opac, int src, int dst, byte alpha,
	uint32_t flags);
void __fastcall CGrafico2D_Init(CGrafico2D *self);
/* the primitives of nVert vertices (4: triangle list, 5: strip, 6: fan) */
uint32_t Grafico2D_NumPrims(int tipo, uint32_t nVert);
void Grafico2D_GetPantalla(int *w, int *h);

#endif
