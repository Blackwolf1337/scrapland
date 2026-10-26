/*
 * Engine/grafico3d.cpp of the engine3d: CGrafico3D (class 0xa6000040), a sprite in the world: a square of ancho by
 * alto turned to the camera at the position of its node, drawn with additive alpha.
 */
#ifndef SCRAP_GRAFICO3D_H
#define SCRAP_GRAFICO3D_H

#include "scrap/mallad3d.h"
#include "scrap/sueloportal.h"

#define CGRAFICO3D_VFTABLE 0x3e45b8
#define g_ShowSprites GAME_DATA(int, 0x4e1dd0)	/* 1: visibility, 2: drawn, 3: with the name */

typedef struct CGrafico3D {
	void **vftable;			/* 0x00 OBJ3D_VF_* */
	uint32_t clase;			/* 0x04 CLASE_OBJ3D_GRAFICO3D */
	vec3_t pos;			/* 0x08 */
	float ancho;			/* 0x14 */
	float alto;			/* 0x18 */
	CNodo3D *nodo;			/* 0x1c counted */
	CMaterial *Mat;			/* 0x20 */
	CAnimMat *AnimMat;		/* 0x24 of an animated texture */
	SectorVis vis;			/* 0x28 */
} CGrafico3D;
SD_SIZE(CGrafico3D, 0x3c);

/* a vertex of the square (D3DFVF_XYZ | DIFFUSE | SPECULAR | TEX1) */
typedef struct Grafico3DVert {
	vec3_t pos;
	uint32_t diffuse;
	uint32_t specular;
	float u, v;
} Grafico3DVert;
SD_SIZE(Grafico3DVert, 0x1c);

CGrafico3D *__thiscall CGrafico3D_ctorTex(CGrafico3D *self, const char *tex, const vec3_t *pos, const float *size);
CGrafico3D *__fastcall CGrafico3D_ctor(CGrafico3D *self);
void __fastcall CGrafico3D_dtor(CGrafico3D *self);
CGrafico3D *__thiscall CGrafico3D_Delete(CGrafico3D *self, byte flags);
byte __thiscall CGrafico3D_Read(CGrafico3D *self, CArchIO *f, MallaD3D_FileCtx *ctx);
void __fastcall CGrafico3D_Draw(CGrafico3D *self);
void __thiscall CGrafico3D_Render(CGrafico3D *self, void *dev, CNodoR *r);
byte __thiscall CGrafico3D_Write(CGrafico3D *self, CArchIO *f, MallaD3D_FileCtx *ctx);
void __thiscall CGrafico3D_SetNodo(CGrafico3D *self, CNodo3D *nodo);
/* the texture with its opacity map (additive alpha without one) */
void __thiscall CGrafico3D_SetTexture(CGrafico3D *self, const char *tex, const char *opac);
/* the material of the textures, shared through g_ListaMat; alpha: the diffuse alpha, 0 is not transparent */
void __thiscall CGrafico3D_CreateMat(CGrafico3D *self, const char *tex, const char *opac, int src, int dst, byte alpha);
void __fastcall CGrafico3D_Init(CGrafico3D *self);

#endif
