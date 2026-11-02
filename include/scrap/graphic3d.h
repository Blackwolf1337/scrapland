/*
 * Engine/grafico3d.cpp of the engine3d: CGraphic3D (class 0xa6000040), a sprite in the world: a square of width by
 * alto turned to the camera at the position of its node, drawn with additive alpha.
 */
#ifndef SCRAP_GRAPHIC3D_H
#define SCRAP_GRAPHIC3D_H

#include "scrap/gvar.h"
#include "scrap/meshd3d.h"
#include "scrap/groundportal.h"

extern const IObject3DVtbl CGraphic3D_vftable;
#define CGRAPHIC3D_VFTABLE (&CGraphic3D_vftable)
extern cGVar GVar_R_SpriteSector;
#define g_ShowSprites (GVar_R_SpriteSector.value.i)	/* 1: visibility, 2: drawn, 3: with the name */

typedef struct CGraphic3D {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;	/* 0x00 */
			uint32_t class;			/* 0x04 CLASS_OBJ3D_GRAPHIC3D */
		};
	};
	vec3_t pos;			/* 0x08 */
	float width;			/* 0x14 */
	float height;			/* 0x18 */
	CNode3D *node;			/* 0x1c counted */
	CMaterial *Mat;			/* 0x20 */
	CAnimMat *AnimMat;		/* 0x24 of an animated texture */
	SectorVis vis;			/* 0x28 */
} CGraphic3D;

/* a vertex of the square (D3DFVF_XYZ | DIFFUSE | SPECULAR | TEX1) */
typedef struct Graphic3DVert {
	vec3_t pos;
	uint32_t diffuse;
	uint32_t specular;
	float u, v;
} Graphic3DVert;

CGraphic3D *CGraphic3D_ctorTex(CGraphic3D *self, const char *tex, const vec3_t *pos, const float *size);
/* with the texture and its opacity map (CGraphic3D_SetTexture) */
CGraphic3D *CGraphic3D_ctorTex2(CGraphic3D *self, const char *tex, const char *opac);
Graphic3DVert *Graphic3DVert_ctor(Graphic3DVert *self);
CGraphic3D *CGraphic3D_ctor(CGraphic3D *self);
void CGraphic3D_dtor(CGraphic3D *self);
IObject3D *CGraphic3D_Delete(IObject3D *base, byte flags);
byte CGraphic3D_Read(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx);
void CGraphic3D_Draw(CGraphic3D *self);
void CGraphic3D_Render(IObject3D *base, void *dev, CRenderNode *r);
byte CGraphic3D_Write(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx);
void CGraphic3D_SetNode(CGraphic3D *self, CNode3D *node);
/* the texture with its opacity map (additive alpha without one) */
void CGraphic3D_SetTexture(CGraphic3D *self, const char *tex, const char *opac);
/* the material of the textures, shared through (&g_MatList); alpha: the diffuse alpha, 0 is not transparent */
void CGraphic3D_CreateMat(CGraphic3D *self, const char *tex, const char *opac, int src, int dst, byte alpha);
void CGraphic3D_Init(CGraphic3D *self);

#endif
