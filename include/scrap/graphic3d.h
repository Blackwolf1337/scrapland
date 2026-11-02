/*
 * Engine/grafico3d.cpp of the engine3d: CGraphic3D (class 0xa6000040), a sprite in the world: a square of width by
 * alto turned to the camera at the position of its node, drawn with additive alpha.
 */
#ifndef SCRAP_GRAPHIC3D_H
#define SCRAP_GRAPHIC3D_H

#include "scrap/gvar.h"
#include "scrap/meshd3d.h"
#include "scrap/groundportal.h"

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

Graphic3DVert *Graphic3DVert_ctor(Graphic3DVert *self);
CGraphic3D *CGraphic3D_ctor(CGraphic3D *self);
void CGraphic3D_SetNode(CGraphic3D *self, CNode3D *node);
/* the material of the textures, shared through (&g_MatList); alpha: the diffuse alpha, 0 is not transparent */
void CGraphic3D_CreateMat(CGraphic3D *self, const char *tex, const char *opac, int src, int dst, byte alpha);

#endif
