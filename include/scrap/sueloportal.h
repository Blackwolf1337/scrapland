/*
 * Engine/SueloPortal.cpp of the engine3d: the sectors (zonas) of a floor, joined by portals, and which of them are
 * seen in a frame. A sector is the node it was made from, its portals (objects of class CLASE_OBJ3D_A7000100) and
 * the sector behind each portal: the sectors, their lookups, the cached sector of an object (SectorVis), the portal
 * culling and the viewport narrowed to a seen sector.
 */
#ifndef SCRAP_SUELOPORTAL_H
#define SCRAP_SUELOPORTAL_H

#include "scrap/nodo3d.h"

#define MAX_PORTALES 32

typedef struct CZona {
	int id;				/* 0x000 its index in g_Zonas */
	int bloq;			/* 0x004 set while the culling is in it */
	int Frame3D;			/* 0x008 g_FrameCount of the last frame it was seen */
	box_t rect;			/* 0x00c the screen rectangle it is seen through */
	CNodo3D *nodo;			/* 0x024 the node of the sector, counted */
	CNodo3D *nodos[2];		/* 0x028 counted */
	int numNodos;			/* 0x030 */
	CNodo3D *portales[MAX_PORTALES];	/* 0x034 the portal nodes, counted (their object is the portal) */
	struct CZona *zonas[MAX_PORTALES];	/* 0x0b4 the sector behind each portal */
	int numPortales;		/* 0x134 */
	byte exterior;			/* 0x138 */
	byte b139;			/* 0x139 counted into the rectangle of all seen sectors */
	byte b13a;			/* 0x13a */
	byte pad_13b;
} CZona;
SD_SIZE(CZona, 0x13c);
SD_OFFSET(CZona, numPortales, 0x134);

/* the cached sector of a position (the vis member of objects that ask for their visibility) */
typedef struct SectorVis {
	uint32_t flags;			/* 0x0 SECTORVIS_* */
	int sector;			/* 0x4 -1 none, -2 not looked up */
	vec3_t pos;			/* 0x8 */
} SectorVis;
#define SECTORVIS_VALID 1
#define SECTORVIS_CACHED 2		/* pos was the same as the last time */

#define g_Zonas ((CZona **)Game_Ptr(0x4afed8))
#define g_NumZonas GAME_DATA(int, 0x4b00d8)
#define g_SectorCache GAME_DATA(int, 0x4b0128)	/* the cached sector is used when the position is the same */
#define g_PortalesOn GAME_DATA(int, 0x4e1e00)	/* 0: everything is seen */
#define g_PortalTest GAME_DATA(int, 0x4b1178)	/* the screen rectangle test; 2: drawn */
#define g_PortalFront GAME_DATA(int, 0x4b0110)	/* portals the camera is in front of beyond the near distance are skipped */
#define g_PortalExterior GAME_DATA(int, 0x4b1190)	/* exterior sectors do not look into interior ones */
#define g_PortalNear GAME_DATA(float, 0x4b0140)	/* the near distance of the portals, times zNear / 256 * 128 */
#define g_PortalUnion GAME_DATA(int, 0x4b00dc)	/* the rectangle of all seen sectors with b139 is kept */

/* the sectors seen this frame (zonas a pintar) and their screen rectangles */
#define MAX_ZONAS_PINTAR 0x80
typedef struct ZonaPintar {
	int zona;			/* 0x00 */
	box_t rect;			/* 0x04 */
	int veces;			/* 0x1c how many portals it was seen through */
} ZonaPintar;
SD_SIZE(ZonaPintar, 0x20);
#define g_ZonasPintar ((ZonaPintar *)Game_Ptr(0x4b0168))
#define g_NumZonasPintar GAME_DATA(int, 0x4afed0)
/* the union of the rectangles of the seen sectors with b139 */
#define g_PintarI148 GAME_DATA(int, 0x4b0148)
#define g_PintarRect (*(box_t *)Game_Ptr(0x4b014c))
#define g_PintarNum GAME_DATA(int, 0x4b0164)
#define g_PintarTodas ((ZonaPintar *)Game_Ptr(0x4b0148))	/* the same three as one */


CZona *__fastcall CZona_ctor(CZona *self);
void __fastcall CZona_dtor(CZona *self);
/* the sector of the cache drawn at pos for the debug output */
void __thiscall SectorVis_Draw(SectorVis *self, const vec3_t *pos, byte visible, byte cached, const char *name);
/* whether the cached sector is seen; a new position only clears SECTORVIS_CACHED */
byte __thiscall SectorVis_Check(SectorVis *self, const vec3_t *pos, const char *name);
/* the sector of pos (looked up again for a new position) is seen */
byte __thiscall SectorVis_Update(SectorVis *self, const vec3_t *pos, const char *name);
/* the screen position (-1..1) of p and its w; 0 outside the view (out then the direction to p on the border) */
byte Portal_Project(const vec3_t *p, float *out, float *w);
/* the sphere is seen through the screen rectangle of its sector (always without the portal test) */
byte __thiscall SectorVis_Sphere(SectorVis *self, const vec3_t *pos, const vec3_t *center, float radius,
	const char *name);
/* the sector reached from sector by the segment from a to b through the portals; front: only through portals
 * whose front side a is not on */
int Sector_Recorrer(int sector, const vec3_t *a, const vec3_t *b, byte front);
/* sector seen through rect, and the sectors behind its portals through their part of it */
void Zona_Pintar(uint32_t zona, const box_t *rect);
/* the sectors seen from sector (the one of the camera) this frame */
void Portales_Cull(uint32_t sector);
/* the elements of g_ZonasPintar: the rectangle cleared */
ZonaPintar *__fastcall ZonaPintar_ctor(ZonaPintar *self);
void __fastcall ZonaPintar_dtor(ZonaPintar *self);
/* the debug drawing of the seen sectors (gvRenderShowPortals 2, 3 also their rectangles as text) */
void Portales_DebugDraw(void);
/* the sectors (the nodes the portals join) and their portals from the floor scene */
void Sectores_Crear(CEscena *esc);
/* the projection narrowed to the screen rectangle of zp (NULL: back to the whole view); 0 when
 * nothing of it is seen */
byte Portales_SetView(const ZonaPintar *zp);
/* the sector under pos into *sector; whether it is seen (always without portals); name is not used */
byte Sector_FindPos(const vec3_t *pos, int *sector, const char *name);
const char *Sector_GetName(int sector);
byte Sector_IsExterior(int sector);
/* -1 is not */
byte Sector_IsOutside(int sector);
byte Sector_Get139(int sector);
byte Sector_IsVisible(int sector);
/* the sector with nodo among its nodes (main: as its node) */
CZona *Zona_FindNodo(CNodo3D *nodo, byte main);
/* the sector named like nodo or one of its parents, -1 */
int Sector_OfNode(CNodo3D *nodo);
int Sector_Find(const char *name);
CZona *Sector_Get(uint32_t sector);
int Sector_Num(void);
/* the sector with nodo among its nodes, -1 */
int Sector_OfZonaNodo(CNodo3D *nodo);
void Sectores_Free(void);

/* the query of Engine/gety.cpp (src/game/engine3d/Engine/Gety.c): the highest floor point below y at x, z */
#define GETY_INFO_MINY 0x1		/* minY given, else -FLT_MAX */
#define GETY_INFO_MAXY 0x2		/* y given, else FLT_MAX */
#define GETY_INFO_MINB 0x4		/* the y of the normal at least minB, else -1 */
#define GETY_INFO_MAXB 0x8		/* at most maxB, else 1 */
#define GETY_INFO_NOCOL 0x100		/* also the triangles without collisions */
#define GETY_INFO_SECTOR 0x200		/* not the meshes added at run time */
typedef struct SueloRayo {
	uint32_t flags;			/* 0x00 GETY_INFO_ */
	float minY;			/* 0x04 */
	float y;			/* 0x08 the height it starts at */
	float minB;			/* 0x0c */
	float maxB;			/* 0x10 */
	float hitY;			/* 0x14 out */
	struct CCollideTri *tri;	/* 0x18 out, NULL for the top of a sector */
	int sector;			/* 0x1c out, -1 none */
} SueloRayo;
SD_SIZE(SueloRayo, 0x20);
/* GETY_INFO_MAXY|GETY_INFO_SECTOR only; with g_RayoVertSectors the top of the sector the ray starts in */
byte __thiscall CSuelo_RayoVert(void *suelo, float x, float z, SueloRayo *res);
/* the highest triangle point between minY and y with the flags of info; 1 when found */
byte __thiscall CSuelo_GetYInfo(void *suelo, float x, float z, SueloRayo *info);
#define g_RayoVertSectors GAME_DATA(int, 0x4e6e10)

#endif
