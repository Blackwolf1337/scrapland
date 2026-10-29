/*
 * Engine/grafico3d.cpp of the engine3d: CGraphic3D (class 0xa6000040), a sprite in the world: a square of width by
 * alto turned to the camera at the position of its node, drawn with additive alpha.
 */
#ifndef SCRAP_GRAPHIC3D_H
#define SCRAP_GRAPHIC3D_H

#include "scrap/mallad3d.h"
#include "scrap/sueloportal.h"

#define CGRAFICO3D_VFTABLE GAME_VFTABLE(CGraphic3D_vftable, 0x3e45b8)
#define g_ShowSprites GAME_GLOBAL_IN(int, GVar_R_SpriteSector, 16, 0x4e1dd0)	/* 1: visibility, 2: drawn, 3: with the name */

typedef struct CGraphic3D {
	void **vftable;			/* 0x00 OBJ3D_VF_* */
	uint32_t class;			/* 0x04 CLASS_OBJ3D_GRAPHIC3D */
	vec3_t pos;			/* 0x08 */
	float width;			/* 0x14 */
	float height;			/* 0x18 */
	CNode3D *node;			/* 0x1c counted */
	CMaterial *Mat;			/* 0x20 */
	CAnimMat *AnimMat;		/* 0x24 of an animated texture */
	SectorVis vis;			/* 0x28 */
} CGraphic3D;
SD_SIZE(CGraphic3D, 0x3c);

/* a vertex of the square (D3DFVF_XYZ | DIFFUSE | SPECULAR | TEX1) */
typedef struct Graphic3DVert {
	vec3_t pos;
	uint32_t diffuse;
	uint32_t specular;
	float u, v;
} Graphic3DVert;
SD_SIZE(Graphic3DVert, 0x1c);

CGraphic3D *__thiscall CGraphic3D_ctorTex(CGraphic3D *self, const char *tex, const vec3_t *pos, const float *size);
/* with the texture and its opacity map (CGraphic3D_SetTexture) */
CGraphic3D *__thiscall CGraphic3D_ctorTex2(CGraphic3D *self, const char *tex, const char *opac);
Graphic3DVert *__fastcall Graphic3DVert_ctor(Graphic3DVert *self);
CGraphic3D *__fastcall CGraphic3D_ctor(CGraphic3D *self);
void __fastcall CGraphic3D_dtor(CGraphic3D *self);
CGraphic3D *__thiscall CGraphic3D_Delete(CGraphic3D *self, byte flags);
byte __thiscall CGraphic3D_Read(CGraphic3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);
void __fastcall CGraphic3D_Draw(CGraphic3D *self);
void __thiscall CGraphic3D_Render(CGraphic3D *self, void *dev, CRenderNode *r);
byte __thiscall CGraphic3D_Write(CGraphic3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);
void __thiscall CGraphic3D_SetNode(CGraphic3D *self, CNode3D *node);
/* the texture with its opacity map (additive alpha without one) */
void __thiscall CGraphic3D_SetTexture(CGraphic3D *self, const char *tex, const char *opac);
/* the material of the textures, shared through g_MatList; alpha: the diffuse alpha, 0 is not transparent */
void __thiscall CGraphic3D_CreateMat(CGraphic3D *self, const char *tex, const char *opac, int src, int dst, byte alpha);
void __fastcall CGraphic3D_Init(CGraphic3D *self);

#endif
