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

#define MAX_PORTALS 32

typedef struct CZone {
	int id;				/* its index in g_Zones */
	int locked;			/* set while the culling is in it */
	int Frame3D;			/* g_FrameCount of the last frame it was seen */
	box_t rect;			/* the screen rectangle it is seen through */
	CNode3D *node;			/* the node of the sector, counted */
	CNode3D *nodes[2];		/* counted */
	int numNodes;
	CNode3D *portals[MAX_PORTALS];	/* the portal nodes, counted (their object is the portal) */
	struct CZone *zones[MAX_PORTALS];	/* the sector behind each portal */
	int numPortals;
	byte exterior;
	byte sky;			/* "(+sky_sector)": counted into the rectangle of all seen sectors */
	byte outside;		/* "(+out_sector)" */
	byte pad_13b;
} CZone;

/* the cached sector of a position (the vis member of objects that ask for their visibility) */
typedef struct SectorVis {
	uint32_t flags;			/* SECTORVIS_* */
	int sector;			/* -1 none, -2 not looked up */
	vec3_t pos;
} SectorVis;
#define SECTORVIS_VALID 1
#define SECTORVIS_CACHED 2		/* pos was the same as the last time */

extern cGVar GVar_R_UsePortals;
#define g_PortalsOn (GVar_R_UsePortals.value.i)	/* 0: everything is seen */

/* the sectors seen this frame (zones a pintar) and their screen rectangles */
#define MAX_PAINT_ZONES 0x80
typedef struct PaintZone {
	int zone;
	box_t rect;
	int times;			/* how many portals it was seen through */
} PaintZone;
extern PaintZone g_PaintZones[128];
extern int g_NumPaintZones;
/* the union of the rectangles of the seen sectors with sky */
#define g_PaintI148 g_PaintAll.zone
#define g_PaintRect g_PaintAll.rect
#define g_PaintNum g_PaintAll.times
extern PaintZone g_PaintAll;	/* the same three as one */

/* the sector of the cache drawn at pos for the debug output */
void SectorVis_Draw(SectorVis *self, const vec3_t *pos, byte visible, byte cached, const char *name);
/* whether the cached sector is seen; a new position only clears SECTORVIS_CACHED */
byte SectorVis_Check(SectorVis *self, const vec3_t *pos, const char *name);
/* the sector of pos (looked up again for a new position) is seen */
byte SectorVis_Update(SectorVis *self, const vec3_t *pos, const char *name);
/* the sphere is seen through the screen rectangle of its sector (always without the portal test) */
byte SectorVis_Sphere(SectorVis *self, const vec3_t *pos, const vec3_t *center, float radius,
	const char *name);
/* the sector reached from sector by the segment from a to b through the portals; front: only through portals
 * whose front side a is not on */
int Sector_Traverse(int sector, const vec3_t *a, const vec3_t *b, byte front);
/* the sectors seen from sector (the one of the camera) this frame */
void Portals_Cull(uint32_t sector);
/* the elements of g_PaintZones: the rectangle cleared */
PaintZone *PaintZone_ctor(PaintZone *self);
void PaintZone_dtor(PaintZone *self);
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
/* the sector named like node or one of its parents, -1 */
int Sector_OfNode(CNode3D *node);
int Sector_Find(const char *name);
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
	uint32_t flags;			/* GETY_INFO_ */
	float minY;
	float y;			/* the height it starts at */
	float minB;
	float maxB;
	float hitY;			/* out */
	struct CCollideTri *tri;	/* out, NULL for the top of a sector */
	int sector;			/* out, -1 none */
} GroundRay;
/* GETY_INFO_MAXY|GETY_INFO_SECTOR only; with g_RayVertSectors the top of the sector the ray starts in */
byte CGround_RayVert(void *ground, float x, float z, GroundRay *res);

#endif
