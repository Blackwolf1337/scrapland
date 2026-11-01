/*
 * Engine/portal.cpp of the engine3d: CPortal (CLASE_OBJ3D_A7000100), a rectangle of width by alto in the xz plane of
 * its node that joins two sectors (the nodes with i48 == idZone). Its corners and plane in the world are cached.
 */
#ifndef SCRAP_PORTAL_H
#define SCRAP_PORTAL_H

#include "scrap/gvar.h"
#include "scrap/meshd3d.h"

#define CPORTAL_VFTABLE GAME_VFTABLE(CPortal_vftable)
extern cGVar GVar_R_ShowPortals;
#define g_ShowPortals (GVar_R_ShowPortals.value.i)	/* gvRenderShowPortals */

typedef struct CPortal {
	void **vftable;			/* 0x00 OBJ3D_VF_* */
	uint32_t class;			/* 0x04 CLASS_OBJ3D_PORTAL */
	CNode3D *node;			/* 0x08 CObjBase3D_SetNode */
	float width;			/* 0x0c */
	float height;			/* 0x10 */
	CNode3D *zones[2];		/* 0x14 the sector nodes, counted */
	int idZone[2];			/* 0x1c */
	byte calculated;			/* 0x24 the cache below is valid */
	byte pad_25[3];
	vec3_t cache_vert[4];		/* 0x28 */
	plane_t cache_plane;		/* 0x58 */
} CPortal;
SD_SIZE(CPortal, 0x68);
SD_OFFSET(CPortal, cache_plane, 0x58);

CPortal *CPortal_ctor(CPortal *self);
void CPortal_dtor(CPortal *self);
CPortal *CPortal_Delete(CPortal *self, byte flags);
/* the sector nodes from the scene of the node, when none is set yet */
void CPortal_LinkZones(CPortal *self);
byte CPortal_Read(CPortal *self, CFileIO *f, MeshD3D_FileCtx *ctx);
byte CPortal_Write(CPortal *self, CFileIO *f, MeshD3D_FileCtx *ctx);
void CPortal_Render(CPortal *self, void *dev, CRenderNode *r);
/* the signed distance of p to the plane */
float CPortal_PlaneDist(CPortal *self, const vec3_t *p);
/* the distance of p to the rectangle */
float CPortal_Dist(CPortal *self, const vec3_t *p);
/* the segment from a to b goes through the rectangle; the point into out */
byte CPortal_SegCross(CPortal *self, const vec3_t *a, const vec3_t *b, vec3_t *out);
void CPortal_SetZone(CPortal *self, uint32_t i, CNode3D *zone);
/* m: the matrix of the node, passed but not used */
void CPortal_UpdateCache(CPortal *self, const matrix_t *m);
/* the pixel of the screen coordinate r + 1 or 1 - r (0..2) in a viewport side of n pixels from o */
static inline int Portal_Pixel(float r, uint32_t n, uint32_t o) {
	return (int)(r * (float)(double)n * 0.5f + (float)(double)o);
}
/* the debug drawing of the rectangle (R_ShowPortals) */
void CPortal_DrawDebug(CPortal *self, uint32_t color);
/* the screen box (-1..1, z 0..1) the portal covers, clipped at the near plane; 0 when it is all behind or beyond the
 * far plane */
byte CPortal_ClipRect(CPortal *self, box_t *rect);
/* a screen rectangle (-1..1) drawn in the viewport vp (x, y, w, h) for the debug output */
void Portal_DrawRect(const int *vp, const float *rect, uint32_t color);

/* other modules (src/imports.inc) */
/* Debug_Line: e3dutil.h */

#endif
