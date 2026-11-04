/*
 * Engine/bsp.cpp of the engine3d: the light maps of the floor ("mapa de sombras"). With [mapa_sombras] use the
 * triangles of the floor model are grouped into lists (BspTri, one per surface, linked by next) that get a light map
 * each: computed (shadows by tracing the floor, radiosity over a tree of regions per list, TXF caches of the result)
 * or read from the .EMI file of the map. The light maps are packed into pages (BspLMap) that become the second texture
 * of the lists, drawn by sector (BspZone) with their own vertex buffers. Off in the game: the global functions return
 * at once without use (g_ShadowMapUse, world.h).
 */
#ifndef SCRAP_BSP_H
#define SCRAP_BSP_H

#include "scrap/gvar.h"
#include "scrap/bmpflt.h"
#include "scrap/vertlist.h"
#include "scrap/light.h"
#include "scrap/material.h"
#include "scrap/e3dworld.h"
#include "scrap/render.h"
#include "scrap/table.h"
#include "scrap/e3dhashtable.h"

#define BSP_MAX_LISTS 80000
#define BSP_MAX_NOLMAP 20000
#define BSP_MAX_LIGHTS 512
#define BSP_MAX_TRILIST 1024

/* a link of the radiosity between two regions */
typedef struct BspLink {
	struct BspLink *next;
	struct BspRegion *region;
	float ff;			/* form factor */
} BspLink;

/* a node of the region tree of a list (Engine radiosity code, 100 bytes) */
typedef struct BspRegion {
	struct BspRegion *child[4];	/* ra rb rc rd */
	struct BspRegion *parent;
	byte interior;			/* any child */
	byte pad15[3];
	float area;
	vec3_t center;
	plane_t plane;
	uint16_t x, y;			/* texel of the light map */
	uint16_t lx, ly;		/* size in texels */
	float color[4];			/* emitted */
	float accum[4];			/* received */
	BspLink *links;
} BspRegion;

/* the names of the area lights of a list */
typedef struct BspName {
	struct BspName *next;
	char *name;			/* Cad */
	byte pad08[0x18];
} BspName;

/* a page of light maps; lmap2 is the page of the secondary maps */
typedef struct BspLMap {
	struct BspLMap *next;
	CBmp bmp;
	CBmp info;			/* the debug colors (.INFO) */
	CMaterial *Mat;
	int minX, minY;			/* the smallest map that did not fit, size + 1 when empty */
	int id;
	struct BspLMap *lmap2;
} BspLMap;

/* a light map texel of a list: the sample point and its weight (CTable of) */
typedef struct BspArea {
	vec3_t pos;
	float weight;
	uint16_t x, y;
	float glow;			/* the mean glow of the texel */
} BspArea;

/* a list: triangles of one surface with one light map */
typedef struct BspTri {
	struct BspTri *next;		/* the next triangle of the list */
	vec3_t v[3];
	vec3_t n[3];			/* vertex normals */
	float uv[3][2];
	float uvl[3][2];		/* in the light map */
	uint32_t color[3];
	int vid[3];			/* vertex ids */
	plane_t plane;
	int axis;			/* the largest axis of the normal */
	uint32_t flags;			/* BSPTRI_* */
	CMaterial *Mat;
	CBmpFlt2 light;			/* the light map */
	CBmpFlt2 light2;			/* the secondary one */
	CBmpFlt lightA;
	int lx, ly;			/* light map size */
	int ext[4];			/* x0 y0 x1 y1 */
	int ext2[4];
	CBmpFlt2 rad;			/* radiosity */
	CBmpFlt2 rad2;
	CBmp mask;			/* the list of each texel (color swapped) */
	CBmp mask2;
	int maskColor;			/* of the triangle in the debug mask (mask2) */
	int unused_130;
	BspName *names;
	char *name;			/* Cad */
	uint32_t radColor;		/* area light color, bytes */
	float radInt;
	float col144[4];
	byte radlight_ok;
	byte glowOn;			/* the glow map (lglow) is on: its average is not black */
	byte big;
	byte pad157;
	float glowSum[4];		/* r, g, b of the areas summed and the count + 1 */
	float listGlowSum[4];		/* the same of the whole list (in its first triangle) */
	float glowMax;			/* the largest channel of the glow at an area: the LOD */
	int glowAreas;			/* the areas summed */
	int listGlowAreas;		/* of the whole list */
	CTable areas[4];		/* BspArea, by level of detail */
	int numAreas;
	vec3_t center;
	float radio;
	float area;
	BspRegion *region;
	BspLMap *LMap;
	CNode3D *node;
	int zone;			/* sector, -1 none */
	struct BspTri *zone_next;	/* the next list of the sector */
	int numTri;
	uint16_t *pTri;			/* 3 indices a triangle */
	CVertList lv;
	CVertList lv2;
	CVertList lv3;
	void *pVertVB;			/* IDirect3DVertexBuffer8 */
	void *pVertIB;
	int baseVert;			/* of the vertex buffer */
	int startTri;
} BspTri;

#define BSPTRI_NOLMAP 0x1		/* flags: no light map, the vertex colors */

/* the corners of tri into box (minss and maxss in the order of the exe) */
static inline void Box_AddTri(box_t *box, const BspTri *tri) {
	for (int i = 0; i < 3; i++) {
		const vec3_t *p = &tri->v[i];
		box->min.x = Float_MinSS(box->min.x, p->x);
		box->min.y = Float_MinSS(box->min.y, p->y);
		box->min.z = Float_MinSS(box->min.z, p->z);
		box->max.x = Float_MaxSS(box->max.x, p->x);
		box->max.y = Float_MaxSS(box->max.y, p->y);
		box->max.z = Float_MaxSS(box->max.z, p->z);
	}
}

static inline byte Bsp_Vec3Equal(const vec3_t *a, const vec3_t *b) {
	return a->x == b->x && a->y == b->y && a->z == b->z;
}

/* the mask colors keep the list in R G B with R and B swapped */
static inline uint32_t Color_SwapRB(uint32_t c) {
	return (c & 0xff000000) | (c & 0xff) << 16 | (c & 0xff00) | (c >> 16 & 0xff);
}

/* the vertex and index buffers of the lists without light map */
typedef struct BspNode {
	struct BspNode *next;
	int i04, i08, i0c;
	uint16_t *pTri;
	CVertList lv;
	void *pVertVB;
} BspNode;

/* a sector: its lists (through zone_next) and the vertex buffers by vertex format */
typedef struct BspZone {
	BspTri *tris;
	int unused_04;
	CRenderVB vb[8];
	CRenderIB ib;
} BspZone;

/* the map (gBSP) */
typedef struct CBsp {
	char *name;			/* Cad, the scene path */
	CModel *model;		/* the map model */
	BspTri *lists[BSP_MAX_LISTS];
	int num_lists;
	BspTri *nolmap[BSP_MAX_NOLMAP];
	int num_nolmap;
	BspNode *nodes;
	void *lights;			/* 4 bytes, the stencil shadows */
	CTable zones;			/* BspZone, zones[0] for zone -1 */
	CMaterial *matFX;
	CMaterial *matWire;		/* the wire frame material */
} CBsp;

/* the TXF cache of the computed light maps: one file a list, "<map>_TXF/<name>.<ext>.<n>.%05d.TXF" */
typedef struct BspTxf {
	byte on;
	byte pad01[3];
	void *FileTXF;			/* FILE of the exe, the lock while writing */
	char base[0x104];
	char name[0x104];
	char team[0x20];		/* COMPUTERNAME */
	int written;
	int skipped;
	int locked;
	int writeRetries;
	int readRetries;
	int deleted;
	int rebuild;
	uint32_t start;		/* timeGetTime */
} BspTxf;

/* a light of the compiler, taken from the lights of the scene */
typedef struct BspLight {
	vec3_t pos;
	vec3_t dir;			/* unit, (2 -1 1) for the omni lights */
	byte point;			/* not a directional light */
	byte shadow;
	byte stencil;			/* the shadows of the node too (NODE_SHADOW) */
	byte noLmap;			/* node flag 27 */
	float rad;
	IllumLight illum;			/* the position and direction of its D3D light from pos and dir */
	CGroundHit cache;		/* the last hit of the shadow traces (CGround_Trace) */
	char name[0x80];		/* of the node */
} BspLight;

extern BspLMap *g_BspLMaps;	/* the pages */
extern BspTri *g_BspTriTmp;
extern BspLight g_BspLights[512];	/* [BSP_MAX_LIGHTS] */
extern int g_TxfMode;		/* the TXF loading of an .EMI (2 deletes) */
extern cGVar GVar_TXF_Helper;
#define g_TXF_Helper (GVar_TXF_Helper.value.i)		/* gvar */
extern int g_Gamma2x;		/* the gamma2x of the video settings */
/* the keys that switch the wire frame: F8 (cleared) with X held */
#define g_KeyF8 g_KeyboardKeys[50]
#define g_KeyX g_KeyboardKeys[29]
extern void *g_StencilRender;	/* the floor drawn again by the stencil shadows */
extern int g_BspSectorSeen;
extern float g_BspLodLevel;
/* the distances of the level of detail of the areas, plain and for large lists */
extern float g_BspLodDist[6];
extern int g_BspLodCount[4];
extern uint32_t g_BspColors[16];	/* [16], debug colors */
/* the globals of m3d.ini ([mapa_sombras], read in ScrapEngMain.c, the same definitions) */
extern int g_ShadowMapMultitext;
extern float g_ShadowMapScale;
extern float g_ShadowMapRes;
extern int g_ShadowMapTexX;
extern int g_ShadowMapTexY;
extern int g_ShadowMapRadiosity;
extern int g_ShadowMapModulate4x;
extern int g_ShadowMapUseTxf;
extern int g_ShadowMapSoft;
extern float g_ShadowMapAtten;
extern int g_ShadowMapIllumGlobal;
extern int g_ShadowMapNoLightAmb;
extern float g_ShadowMapGamma;
extern int g_ShadowMapBounces;
extern float g_ShadowMapCoefRad;
/* the radiosity */
extern int g_BspAreasTotal[4];	/* the areas of all lists by level */

void CBmpFlt2_Assign(CBmpFlt2 *self, const CBmpFlt2 *orig);
void CBmpFlt2_CreateFill(CBmpFlt2 *self, int width, int height, const float *color);

/* the TXF cache */
BspTxf *BspTxf_ctor(BspTxf *self, const char *map, const char *name, int n, int on);
void BspTxf_dtor(BspTxf *self);
byte BspTxf_Check(BspTxf *self, int i, byte def, byte nl);
void BspTxf_Save(BspTxf *self, CBmpFlt2 *bmp);
void BspTxf_Load(BspTxf *self, BspTri **lists, int n, int which);

/* the lists */
BspTri *BspTri_ctor(BspTri *self);
void BspTri_dtor(BspTri *self);
box_t *BspTri_ListBox(box_t *out, const BspTri *list);
box_t *BspTri_AddBox(box_t *out, const box_t *in, const BspTri *tri);
void BspTri_CalcPlanes(BspTri *list);
int BspTri_Side(const BspTri *tri);
/* p on the plane of the triangle: the normal and the uv (light map uv with lmap) at p */
void BspTri_Interp(const BspTri *tri, const vec3_t *p, vec3_t *n, float *uv, byte lmap);
int BspTri_Zone(CNode3D *node);
const char *BspTri_ZoneName(const BspTri *tri);
int BspTri_Count(const BspTri *list);

/* the texel (u, v) of a light map at scale on the plane, the coordinate of axis from the plane */
void Bsp_TexelToWorld(vec3_t *out, float u, float v, int axis, float scale, const plane_t *pl);
/* the color as RGB bytes scaled to the largest channel and that channel */
void Bsp_ColorIntens(uint32_t *out, const float *rgb);

/* the light maps of the lists (with the ones without) into new pages by material and sector, the pages saved as
 * "<scene>.%04d.LMAP1.tga" (LMAP2 the secondary ones, .INFO the debug colors) and their materials made from them */
void CBsp_GroupMaps(CBsp *self);

void Bsp_TexelBox(const box_t *b, int axis, float scale, int *x0, int *y0, int *x1, int *y1);
/* the boxes cover the same texels, or both fit a light map */
byte Bsp_SameSize(const box_t *a, int axis, float scale, const box_t *b);
/* the extents and the size of the light map; notify: a message for too large ones */
void BspTri_SetExt(BspTri *tri, int x0, int y0, int x1, int y1, byte notify);
byte BspTri_VertEqual(const BspTri *act, uint32_t nv, const BspTri *tri);
CTable *BspTri_GetAreas(BspTri *tri, float dist, byte big);
int BspTri_CmpZone(const void *a, const void *b);
vec3_t *BspTri_Sample(vec3_t *out, int x, int y, int axis, float scale, const BspTri *tri, const BspTri *list,
	float *weight);
void Bsp_MatInfo(CMaterial *m);
void CBsp_AreasGlow(CBsp *self);
void CBsp_AreasMax(CBsp *self);
void CBsp_AdjustGamma(CBsp *self);
void CBsp_InitAreas(CBsp *self);
void BspTri_CalcGlow(BspTri *list);
float Bsp_Occlusion(const vec3_t *p, const vec3_t *n, float r, int samples, const vec3_t *light, CGround *ground);
void CBsp_SaveIni(CBsp *self);
void Bsp_EndMessage(CBsp *self, int secs);

BspZone *BspZone_ctor(BspZone *self);
void BspZone_dtor(BspZone *self);
void CBsp_RemoveLights(CBsp *self);

void Bsp_RenderTransp(void);
void Bsp_RenderPortals(void);

/* the map compiler dialog */

/* the map compiler (BspCalc.c) */

/* the light maps computed: the lists of the meshes of the scene, their light, the pages and the .EMI */
void CBsp_Calculate(CBsp *self);

/* the float edits of the dialogs (src/game/engine3d/Engine/Misc.c): set returns 1, get 1 for a number */

/*
 * Engine/bsp2.cpp (Bsp2.c): the BSP tree of the triangles that cast shadows (CBsp.lights), split along the axis
 * planes through the middle of their box while both sides keep more than 5 triangles.
 */
typedef struct Bsp2Tri {
	vec3_t v[3];
	plane_t plane;
	CMaterial *Mat;
} Bsp2Tri;

typedef struct Bsp2Node {
	plane_t plane;
	struct Bsp2Node *front;		/* the side of the normal */
	struct Bsp2Node *back;
	CTable tris;			/* Bsp2Tri, of the leaves */
} Bsp2Node;

typedef struct BspLights {
	Bsp2Node *root;
} BspLights;

void BspLights_dtor(void *self);
/* the leaves as debug triangles, from the far side of the eye */
void BspLights_Render(void *self);
void Bsp2_Stats(int print);

/*
 * Engine/radiosidad.cpp (Radiosity.c): the regions and links of the radiosity, and the area lights of the
 * materials: a material named "(+area_light: name)" (MAT_AREALIGHT, older ones "(+name)") emits the light of the
 * section name of the .rad files of the map ("rgbi", "aten", "falloff").
 */
typedef struct RadAreaLight {
	struct RadAreaLight *next;
	char *name;			/* String_Set */
	byte rgbi[4];			/* r g b, from the diffuse color of the material */
	float intensity;
	float unused_10;
	float atten[2];
	float falloff;
} RadAreaLight;

typedef struct CRadiosity {
	CIniFile ini;			/* the .rad files */
	CHashTable *table;		/* the area lights by material */
	RadAreaLight *list;
} CRadiosity;

/* the color and the plane of parent */
BspRegion *BspRegion_ctor(BspRegion *self, int x, int y, int lx, int ly, BspRegion *parent);
void BspRegion_dtor(BspRegion *self);
/* the .rad files: name.rad and lights.rad of dir (may be NULL), then the global one */
CRadiosity *CRadiosity_ctor(CRadiosity *self, const char *dir);
void CRadiosity_dtor(CRadiosity *self);
/* CRadiosity_ReadMat with the warnings of a missing or old definition */
byte CRadiosity_ReadMatWarn(CRadiosity *self, CMaterial *Mat, RadAreaLight *dest);
/* the normals and their count */
vec3_t *Rad_Normals(uint32_t *num);

#endif
