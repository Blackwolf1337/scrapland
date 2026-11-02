/*
 * Engine/SueloPortal.cpp of the engine3d: the sectors (zones) of a floor, joined by portals, and which of them are
 * seen in a frame. A sector is the node it was made from, its portals (objects of class CLASE_OBJ3D_A7000100) and
 * the sector behind each portal: the sectors, their lookups, the cached sector of an object (SectorVis), the portal
 * culling and the viewport narrowed to a seen sector.
 */
#ifndef SCRAP_GROUNDPORTAL_H
#define SCRAP_GROUNDPORTAL_H

#include "scrap/gvar.h"
#include "scrap/node3d.h"

extern cGVar g_FastGetYSectorVar;

#define MAX_PORTALS 32

typedef struct CZone {
	int id;				/* 0x000 its index in g_Zones */
	int locked;			/* 0x004 set while the culling is in it */
	int Frame3D;			/* 0x008 g_FrameCount of the last frame it was seen */
	box_t rect;			/* 0x00c the screen rectangle it is seen through */
	CNode3D *node;			/* 0x024 the node of the sector, counted */
	CNode3D *nodes[2];		/* 0x028 counted */
	int numNodes;			/* 0x030 */
	CNode3D *portals[MAX_PORTALS];	/* 0x034 the portal nodes, counted (their object is the portal) */
	struct CZone *zones[MAX_PORTALS];	/* 0x0b4 the sector behind each portal */
	int numPortals;		/* 0x134 */
	byte exterior;			/* 0x138 */
	byte sky;			/* 0x139 "(+sky_sector)": counted into the rectangle of all seen sectors */
	byte outside;		/* 0x13a "(+out_sector)" */
	byte pad_13b;
} CZone;
SD_SIZE(CZone, 0x13c);
SD_OFFSET(CZone, numPortals, 0x134);

/* the cached sector of a position (the vis member of objects that ask for their visibility) */
typedef struct SectorVis {
	uint32_t flags;			/* 0x0 SECTORVIS_* */
	int sector;			/* 0x4 -1 none, -2 not looked up */
	vec3_t pos;			/* 0x8 */
} SectorVis;
#define SECTORVIS_VALID 1
#define SECTORVIS_CACHED 2		/* pos was the same as the last time */

extern CZone * g_Zones[132];
#define g_NumZones (*(int *)&((void * *)g_Zones)[128])
extern cGVar GVar_R_PortalPos;
#define g_SectorCache (GVar_R_PortalPos.value.i)	/* the cached sector is used when the position is the same */
extern cGVar GVar_R_UsePortals;
#define g_PortalsOn (GVar_R_UsePortals.value.i)	/* 0: everything is seen */
extern cGVar GVar_R_PortalExt;
#define g_PortalTest (GVar_R_PortalExt.value.i)	/* the screen rectangle test; 2: drawn */
extern cGVar GVar_R_PortalNorm;
#define g_PortalFront (GVar_R_PortalNorm.value.i)	/* portals the camera is in front of beyond the near distance are skipped */
extern cGVar GVar_R_PortalEnd;
#define g_PortalExterior (GVar_R_PortalEnd.value.i)	/* exterior sectors do not look into interior ones */
extern cGVar GVar_R_PortalDist;
#define g_PortalNear (GVar_R_PortalDist.value.f)	/* the near distance of the portals, times zNear / 256 * 128 */
#define g_PortalUnion (*(int *)&((void * *)g_Zones)[129])	/* the rectangle of all seen sectors with sky is kept */

/* the sectors seen this frame (zones a pintar) and their screen rectangles */
#define MAX_PAINT_ZONES 0x80
typedef struct PaintZone {
	int zone;			/* 0x00 */
	box_t rect;			/* 0x04 */
	int times;			/* 0x1c how many portals it was seen through */
} PaintZone;
SD_SIZE(PaintZone, 0x20);
extern PaintZone g_PaintZones[128];
extern int g_NumPaintZones;
/* the union of the rectangles of the seen sectors with sky */
#define g_PaintI148 GAME_GLOBAL_IN(int, g_PaintAll, 0)
#define g_PaintRect (GAME_GLOBAL_IN(box_t, g_PaintAll, 4))
#define g_PaintNum GAME_GLOBAL_IN(int, g_PaintAll, 28)
extern PaintZone g_PaintAll;	/* the same three as one */


CZone *CZone_ctor(CZone *self);
void CZone_dtor(CZone *self);
/* the sector of the cache drawn at pos for the debug output */
void SectorVis_Draw(SectorVis *self, const vec3_t *pos, byte visible, byte cached, const char *name);
/* whether the cached sector is seen; a new position only clears SECTORVIS_CACHED */
byte SectorVis_Check(SectorVis *self, const vec3_t *pos, const char *name);
/* the sector of pos (looked up again for a new position) is seen */
byte SectorVis_Update(SectorVis *self, const vec3_t *pos, const char *name);
/* the screen position (-1..1) of p and its w; 0 outside the view (out then the direction to p on the border) */
byte Portal_Project(const vec3_t *p, float *out, float *w);
/* the sphere is seen through the screen rectangle of its sector (always without the portal test) */
byte SectorVis_Sphere(SectorVis *self, const vec3_t *pos, const vec3_t *center, float radius,
	const char *name);
/* the sector reached from sector by the segment from a to b through the portals; front: only through portals
 * whose front side a is not on */
int Sector_Traverse(int sector, const vec3_t *a, const vec3_t *b, byte front);
/* sector seen through rect, and the sectors behind its portals through their part of it */
void Zone_Paint(uint32_t zone, const box_t *rect);
/* the sectors seen from sector (the one of the camera) this frame */
void Portals_Cull(uint32_t sector);
/* the elements of g_PaintZones: the rectangle cleared */
PaintZone *PaintZone_ctor(PaintZone *self);
void PaintZone_dtor(PaintZone *self);
/* the debug drawing of the seen sectors (gvRenderShowPortals 2, 3 also their rectangles as text) */
void Portals_DebugDraw(void);
/* the sectors (the nodes the portals join) and their portals from the floor scene */
void Sectors_Create(CScene *scn);
/* the projection narrowed to the screen rectangle of zp (NULL: back to the whole view); 0 when
 * nothing of it is seen */
byte Portals_SetView(const PaintZone *zp);
/* the sector under pos into *sector; whether it is seen (always without portals); name is not used */
byte Sector_FindPos(const vec3_t *pos, int *sector, const char *name);
const char *Sector_GetName(int sector);
/* -1 is not */
byte Sector_IsOutside(int sector);
byte Sector_IsVisible(int sector);
/* the sector with node among its nodes (main: as its node) */
CZone *Zone_FindNode(CNode3D *node, byte main);
/* the sector named like node or one of its parents, -1 */
int Sector_OfNode(CNode3D *node);
int Sector_Find(const char *name);
CZone *Sector_Get(uint32_t sector);
int Sector_Num(void);
/* the sector with node among its nodes, -1 */
void Sectors_FreeAll(void);

/* the query of Engine/gety.cpp (src/game/engine3d/Engine/Gety.c): the highest floor point below y at x, z */
#define GETY_INFO_MINY 0x1		/* minY given, else -FLT_MAX */
#define GETY_INFO_MAXY 0x2		/* y given, else FLT_MAX */
#define GETY_INFO_MINB 0x4		/* the y of the normal at least minB, else -1 */
#define GETY_INFO_MAXB 0x8		/* at most maxB, else 1 */
#define GETY_INFO_NOCOL 0x100		/* also the triangles without collisions */
#define GETY_INFO_SECTOR 0x200		/* not the meshes added at run time */
typedef struct GroundRay {
	uint32_t flags;			/* 0x00 GETY_INFO_ */
	float minY;			/* 0x04 */
	float y;			/* 0x08 the height it starts at */
	float minB;			/* 0x0c */
	float maxB;			/* 0x10 */
	float hitY;			/* 0x14 out */
	struct CCollideTri *tri;	/* 0x18 out, NULL for the top of a sector */
	int sector;			/* 0x1c out, -1 none */
} GroundRay;
SD_SIZE(GroundRay, 0x20);
/* GETY_INFO_MAXY|GETY_INFO_SECTOR only; with g_RayVertSectors the top of the sector the ray starts in */
byte CGround_RayVert(void *ground, float x, float z, GroundRay *res);
/* the highest triangle point between minY and y with the flags of info; 1 when found */
byte CGround_GetYInfo(void *ground, float x, float z, GroundRay *info);
#define g_RayVertSectors (*(int *)&g_FastGetYSectorVar.value)

#endif
