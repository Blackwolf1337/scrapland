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
			const IObject3DVtbl *vftable;
			uint32_t class;			/* CLASS_OBJ3D_GRAPHIC3D */
		};
	};
	vec3_t pos;
	float width;
	float height;
	CNode3D *node;			/* counted */
	CMaterial *Mat;
	CAnimMat *AnimMat;		/* of an animated texture */
	SectorVis vis;
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
