/*
 * Engine/portal.cpp of the engine3d: CPortal (CLASE_OBJ3D_A7000100), a rectangle of width by alto in the xz plane of
 * its node that joins two sectors (the nodes with i48 == idZone). Its corners and plane in the world are cached.
 */
#ifndef SCRAP_PORTAL_H
#define SCRAP_PORTAL_H

#include "scrap/gvar.h"
#include "scrap/meshd3d.h"

extern cGVar GVar_R_ShowPortals;
#define g_ShowPortals (GVar_R_ShowPortals.value.i)	/* gvRenderShowPortals */

typedef struct CPortal {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;
			uint32_t class;			/* CLASS_OBJ3D_PORTAL */
		};
	};
	CNode3D *node;			/* CObjBase3D_SetNode */
	float width;
	float height;
	CNode3D *zones[2];		/* the sector nodes, counted */
	int idZone[2];
	byte calculated;			/* the cache below is valid */
	byte pad_25[3];
	vec3_t cache_vert[4];
	plane_t cache_plane;
} CPortal;

CPortal *CPortal_ctor(CPortal *self);
/* the sector nodes from the scene of the node, when none is set yet */
void CPortal_LinkZones(CPortal *self);
/* the signed distance of p to the plane */
float CPortal_PlaneDist(CPortal *self, const vec3_t *p);
/* the distance of p to the rectangle */
float CPortal_Dist(CPortal *self, const vec3_t *p);
/* the segment from a to b goes through the rectangle; the point into out */
byte CPortal_SegCross(CPortal *self, const vec3_t *a, const vec3_t *b, vec3_t *out);
void CPortal_SetZone(CPortal *self, uint32_t i, CNode3D *zone);
/* the pixel of the screen coordinate r + 1 or 1 - r (0..2) in a viewport side of n pixels from o */
static inline int Portal_Pixel(float r, uint32_t n, uint32_t o) {
	return (int)(r * (float)(double)n * 0.5f + (float)(double)o);
}
/* the debug drawing of the rectangle (R_ShowPortals) */
void CPortal_DrawDebug(CPortal *self, uint32_t color);
/* the screen box (-1..1, z 0..1) the portal covers, clipped at the near plane; 0 when it is all behind or beyond the
 * far plane */
byte CPortal_ClipRect(CPortal *self, box_t *rect);

/* other modules (src/imports.inc) */
/* Debug_Line: e3dutil.h */

#endif
