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
	struct BspLink *next;		/* 0x0 */
	struct BspRegion *region;	/* 0x4 */
	float ff;			/* 0x8 form factor */
} BspLink;

/* a node of the region tree of a list (Engine radiosity code, 100 bytes) */
typedef struct BspRegion {
	struct BspRegion *child[4];	/* 0x00 ra rb rc rd */
	struct BspRegion *parent;	/* 0x10 */
	byte interior;			/* 0x14 any child */
	byte pad15[3];
	float area;			/* 0x18 */
	vec3_t center;			/* 0x1c */
	plane_t plane;			/* 0x28 */
	uint16_t x, y;			/* 0x38 texel of the light map */
	uint16_t lx, ly;		/* 0x3c size in texels */
	float color[4];			/* 0x40 emitted */
	float accum[4];			/* 0x50 received */
	BspLink *links;		/* 0x60 */
} BspRegion;
SD_OFFSET(BspRegion, center, 0x1c);
SD_OFFSET(BspRegion, accum, 0x50);
SD_SIZE(BspRegion, 0x64);

/* the names of the area lights of a list */
typedef struct BspName {
	struct BspName *next;		/* 0x00 */
	char *name;			/* 0x04 Cad */
	byte pad08[0x18];
} BspName;
SD_SIZE(BspName, 0x20);

/* a page of light maps; lmap2 is the page of the secondary maps */
typedef struct BspLMap {
	struct BspLMap *next;		/* 0x00 */
	CBmp bmp;			/* 0x04 */
	CBmp info;			/* 0x10 the debug colors (.INFO) */
	CMaterial *Mat;			/* 0x1c */
	int minX, minY;			/* 0x20 the smallest map that did not fit, size + 1 when empty */
	int id;				/* 0x28 */
	struct BspLMap *lmap2;		/* 0x2c */
} BspLMap;
SD_SIZE(BspLMap, 0x30);

/* a light map texel of a list: the sample point and its weight (CTable of 0x18 bytes) */
typedef struct BspArea {
	vec3_t pos;			/* 0x00 */
	float weight;			/* 0x0c */
	uint16_t x, y;			/* 0x10 */
	float glow;			/* 0x14 the mean glow of the texel */
} BspArea;
SD_SIZE(BspArea, 0x18);

/* a list: triangles of one surface with one light map (0x224 bytes) */
typedef struct BspTri {
	struct BspTri *next;		/* 0x000 the next triangle of the list */
	vec3_t v[3];			/* 0x004 */
	vec3_t n[3];			/* 0x028 vertex normals */
	float uv[3][2];			/* 0x04c */
	float uvl[3][2];		/* 0x064 in the light map */
	uint32_t color[3];		/* 0x07c */
	int vid[3];			/* 0x088 vertex ids */
	plane_t plane;			/* 0x094 */
	int axis;			/* 0x0a4 the largest axis of the normal */
	uint32_t flags;			/* 0x0a8 BSPTRI_* */
	CMaterial *Mat;			/* 0x0ac */
	CBmpFlt2 light;			/* 0x0b0 the light map */
	CBmpFlt2 light2;			/* 0x0bc the secondary one */
	CBmpFlt lightA;			/* 0x0c8 */
	int lx, ly;			/* 0x0d4 light map size */
	int ext[4];			/* 0x0dc x0 y0 x1 y1 */
	int ext2[4];			/* 0x0ec */
	CBmpFlt2 rad;			/* 0x0fc radiosity */
	CBmpFlt2 rad2;			/* 0x108 */
	CBmp mask;			/* 0x114 the list of each texel (color swapped) */
	CBmp mask2;			/* 0x120 */
	int maskColor;			/* 0x12c of the triangle in the debug mask (mask2) */
	int unused_130;			/* 0x130 */
	BspName *names;		/* 0x134 */
	char *name;			/* 0x138 Cad */
	uint32_t radColor;		/* 0x13c area light color, bytes */
	float radInt;			/* 0x140 */
	float col144[4];		/* 0x144 */
	byte radlight_ok;			/* 0x154 */
	byte glowOn;			/* 0x155 the glow map (lglow) is on: its average is not black */
	byte big;			/* 0x156 */
	byte pad157;
	float glowSum[4];		/* 0x158 r, g, b of the areas summed and the count + 1 */
	float listGlowSum[4];		/* 0x168 the same of the whole list (in its first triangle) */
	float glowMax;			/* 0x178 the largest channel of the glow at an area: the LOD */
	int glowAreas;			/* 0x17c the areas summed */
	int listGlowAreas;		/* 0x180 of the whole list */
	CTable areas[4];		/* 0x184 BspArea, by level of detail */
	int numAreas;			/* 0x1a4 */
	vec3_t center;			/* 0x1a8 */
	float radio;			/* 0x1b4 */
	float area;			/* 0x1b8 */
	BspRegion *region;		/* 0x1bc */
	BspLMap *LMap;			/* 0x1c0 */
	CNode3D *node;			/* 0x1c4 */
	int zone;			/* 0x1c8 sector, -1 none */
	struct BspTri *zone_next;	/* 0x1cc the next list of the sector */
	int numTri;			/* 0x1d0 */
	uint16_t *pTri;			/* 0x1d4 3 indices a triangle */
	CVertList lv;			/* 0x1d8 */
	CVertList lv2;			/* 0x1ec */
	CVertList lv3;			/* 0x200 */
	void *pVertVB;			/* 0x214 IDirect3DVertexBuffer8 */
	void *pVertIB;			/* 0x218 */
	int baseVert;			/* 0x21c of the vertex buffer */
	int startTri;			/* 0x220 */
} BspTri;
SD_OFFSET(BspTri, plane, 0x94);
SD_OFFSET(BspTri, Mat, 0xac);
SD_OFFSET(BspTri, mask, 0x114);
SD_OFFSET(BspTri, name, 0x138);
SD_OFFSET(BspTri, glowMax, 0x178);
SD_OFFSET(BspTri, areas, 0x184);
SD_OFFSET(BspTri, region, 0x1bc);
SD_OFFSET(BspTri, lv, 0x1d8);
SD_OFFSET(BspTri, pVertVB, 0x214);
SD_SIZE(BspTri, 0x224);

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

/* the vertex and index buffers of the lists without light map (0x2c bytes) */
typedef struct BspNode {
	struct BspNode *next;		/* 0x00 */
	int i04, i08, i0c;		/* 0x04 */
	uint16_t *pTri;			/* 0x10 */
	CVertList lv;			/* 0x14 */
	void *pVertVB;			/* 0x28 */
} BspNode;
SD_SIZE(BspNode, 0x2c);

/* a sector: its lists (through zone_next) and the vertex buffers by vertex format */
typedef struct BspZone {
	BspTri *tris;			/* 0x00 */
	int unused_04;			/* 0x04 */
	CRenderVB vb[8];		/* 0x08 */
	CRenderIB ib;			/* 0xa8 */
} BspZone;
SD_SIZE(BspZone, 0xb4);

/* the map (gBSP, 0x61aa8 bytes) */
typedef struct CBsp {
	char *name;			/* 0x00000 Cad, the scene path */
	CModel *model;		/* 0x00004 the map model */
	BspTri *lists[BSP_MAX_LISTS];	/* 0x00008 */
	int num_lists;			/* 0x4e208 */
	BspTri *nolmap[BSP_MAX_NOLMAP];	/* 0x4e20c */
	int num_nolmap;			/* 0x61a8c */
	BspNode *nodes;			/* 0x61a90 */
	void *lights;			/* 0x61a94 4 bytes, the stencil shadows */
	CTable zones;			/* 0x61a98 BspZone, zones[0] for zone -1 */
	CMaterial *matFX;		/* 0x61aa0 */
	CMaterial *matWire;		/* 0x61aa4 the wire frame material */
} CBsp;
SD_OFFSET(CBsp, num_lists, 0x4e208);
SD_OFFSET(CBsp, num_nolmap, 0x61a8c);
SD_OFFSET(CBsp, zones, 0x61a98);
SD_SIZE(CBsp, 0x61aa8);

/* the TXF cache of the computed light maps: one file a list, "<map>_TXF/<name>.<ext>.<n>.%05d.TXF" */
typedef struct BspTxf {
	byte on;			/* 0x000 */
	byte pad01[3];
	void *FileTXF;			/* 0x004 FILE of the exe, the lock while writing */
	char base[0x104];		/* 0x008 */
	char name[0x104];		/* 0x10c */
	char team[0x20];		/* 0x210 COMPUTERNAME */
	int written;			/* 0x230 */
	int skipped;			/* 0x234 */
	int locked;			/* 0x238 */
	int writeRetries;		/* 0x23c */
	int readRetries;		/* 0x240 */
	int deleted;			/* 0x244 */
	int rebuild;			/* 0x248 */
	uint32_t start;		/* 0x24c timeGetTime */
} BspTxf;
SD_OFFSET(BspTxf, team, 0x210);
SD_SIZE(BspTxf, 0x250);

/* a light of the compiler, taken from the lights of the scene (0x118 bytes) */
typedef struct BspLight {
	vec3_t pos;			/* 0x00 */
	vec3_t dir;			/* 0x0c unit, (2 -1 1) for the omni lights */
	byte point;			/* 0x18 not a directional light */
	byte shadow;			/* 0x19 */
	byte stencil;			/* 0x1a the shadows of the node too (NODE_SHADOW) */
	byte noLmap;			/* 0x1b node flag 27 */
	float rad;			/* 0x1c */
	IllumLight illum;			/* 0x20 the position and direction of its D3D light from pos and dir */
	CGroundHit cache;		/* 0x8c the last hit of the shadow traces (CGround_Trace) */
	char name[0x80];		/* 0x98 of the node */
} BspLight;
SD_OFFSET(BspLight, illum, 0x20);
SD_OFFSET(BspLight, name, 0x98);
SD_SIZE(BspLight, 0x118);

extern CBsp *g_Bsp;
extern BspLMap *g_BspLMaps;	/* the pages */
extern BspTri *g_BspTriTmp;
extern void *g_pBSP_VB;
extern int g_BspNumLights;
extern BspLight g_BspLights[512];	/* [BSP_MAX_LIGHTS] */
extern int g_LmapInfo;		/* the .INFO maps */
extern int g_LmapInfo2;
extern int g_TxfMode;		/* the TXF loading of an .EMI (2 deletes) */
extern cGVar GVar_R_ForceLMap;
#define g_R_ForceLMap (GVar_R_ForceLMap.value.i)	/* gvar R_ForceLMap, -1 */
extern int g_LmapSecond;		/* the secondary maps (stencil pass 2) */
extern cGVar GVar_RenderMap;
#define g_RenderMap (GVar_RenderMap.value.i)		/* gvar RenderMap */
extern cGVar GVar_LightmapPackMode;
#define g_LightmapPackMode (GVar_LightmapPackMode.value.i)	/* gvar */
extern cGVar GVar_LightmapDxtPadding;
#define g_LightmapDxtPadding (GVar_LightmapDxtPadding.value.i)	/* gvar */
extern cGVar GVar_LightmapLimitUV;
#define g_LightmapLimitUV (GVar_LightmapLimitUV.value.i)	/* gvar */
extern cGVar GVar_MapDrawBySector;
#define g_MapDrawBySector (GVar_MapDrawBySector.value.i)	/* gvar */
extern cGVar GVar_MapDrawSector;
#define g_MapDrawSector (GVar_MapDrawSector.value.i)	/* gvar, -2 all */
extern cGVar GVar_MapRenderMap;
#define g_MapRenderMap (GVar_MapRenderMap.value.s)	/* gvar, a wildcard of map names */
extern cGVar GVar_MapRenderMat;
#define g_MapRenderMat (GVar_MapRenderMat.value.s)	/* gvar, a wildcard of material names */
extern cGVar GVar_MapSortByShader;
#define g_MapSortByShader (GVar_MapSortByShader.value.i)	/* gvar */
extern cGVar GVar_RenderSectors;
#define g_RenderSectors (GVar_RenderSectors.value.i)	/* gvar */
extern cGVar GVar_TXF_Helper;
#define g_TXF_Helper (GVar_TXF_Helper.value.i)		/* gvar */
extern cGVar GVar_ShowInfoOnSectorError;
#define g_ShowInfoOnSectorError (GVar_ShowInfoOnSectorError.value.i)	/* gvar */
extern int g_Gamma2x;		/* the gamma2x of the video settings */
/* the keys that switch the wire frame: F8 (cleared) with X held */
#define g_KeyF8 GAME_GLOBAL_IN(byte, g_KeyboardKeys, 50)
#define g_KeyX GAME_GLOBAL_IN(byte, g_KeyboardKeys, 29)
extern void *g_StencilRender;	/* the floor drawn again by the stencil shadows */
extern int g_BspTris;		/* drawn this frame */
extern int g_BspVerts;
extern BspTri * g_TriList[1024];	/* [BSP_MAX_TRILIST] the lists of a pass to sort */
/* the sector of the eye: found this frame, the sector, the frames without one, the eye of the last frame */
extern byte g_BspSectorOk;
extern int g_BspSectorSeen;
extern int g_BspSectorErrors;
extern vec3_t g_BspEyePrev;
extern float g_BspLodLevel;
/* the distances of the level of detail of the areas, plain and for large lists */
extern float g_BspLodDist[7];
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
extern float g_BspRadMinD2;	/* the smallest squared distance of a form factor */
extern cGVar GVar_RadSkipBySectors;
#define g_RadSkipBySectors (GVar_RadSkipBySectors.value.i)	/* gvar: only the lists of the same sector */
extern cGVar GVar_RadiosityPortals;
#define g_RadiosityPortals (GVar_RadiosityPortals.value.i)	/* gvar: other sectors only through the portals */
extern cGVar GVar_RadiosityMinDist;
#define g_RadiosityMinDist (GVar_RadiosityMinDist.value.f)	/* gvar: g_BspRadMinD2 in texels */
/* the compiler */
extern int g_LmapOption58;		/* a switch of the dialog (0x40d): only the nodes with flag 4 */
extern cGVar GVar_TXF_CheckFiles;
#define g_TxfCheckFiles (GVar_TXF_CheckFiles.value.i)	/* gvar TXF_CheckFiles: the TXF mode asked for */

/* engine3d/engine/bmp*.h instanced here */
void CBmpFlt2_dtor(CBmpFlt2 *self);
void CBmpFlt_dtor(CBmpFlt *self);
void CBmp_Assign(CBmp *self, const CBmp *orig);
void CBmpFlt2_Assign(CBmpFlt2 *self, const CBmpFlt2 *orig);
/* new pixels filled with color */
void CBmp_CreateFill(CBmp *self, int width, int height, uint32_t color);
void CBmpFlt2_CreateFill(CBmpFlt2 *self, int width, int height, const float *color);
void CBmpFlt_CreateFill(CBmpFlt *self, int width, int height, const float *color);
/* orig turned by 90 degrees (the size swapped) */
void CBmp_Rotate(CBmp *self, const CBmp *orig);
void CBmpFlt2_Rotate(CBmpFlt2 *self, const CBmpFlt2 *orig);

/* the TXF cache */
BspTxf *BspTxf_ctor(BspTxf *self, const char *map, const char *name, int n, int on);
void BspTxf_dtor(BspTxf *self);
void BspTxf_SetName(BspTxf *self, int i);
void BspTxf_Info(BspTxf *self);
byte BspTxf_Check(BspTxf *self, int i, byte def, byte nl);
void BspTxf_Save(BspTxf *self, CBmpFlt2 *bmp);
void BspTxf_Load(BspTxf *self, BspTri **lists, int n, int which);

/* the lists */
BspTri *BspTri_ctor(BspTri *self);
void BspTri_dtor(BspTri *self);
void BspTri_CalcCenter(BspTri *tri);
box_t *BspTri_ListBox(box_t *out, const BspTri *list);
box_t *BspTri_AddBox(box_t *out, const box_t *in, const BspTri *tri);
void BspTri_CalcPlanes(BspTri *list);
int BspTri_Side(const BspTri *tri);
/* p on the plane of the triangle: the normal and the uv (light map uv with lmap) at p */
void BspTri_Interp(const BspTri *tri, const vec3_t *p, vec3_t *n, float *uv, byte lmap);
void BspTri_GetExt(const BspTri *tri, int *x0, int *y0, int *x1, int *y1);
void BspTri_GetExt2(const BspTri *tri, int *x0, int *y0, int *x1, int *y1);
/* the texel x, y of a map at (x0, y0) belongs to the list of color (mask colors are R and B swapped) */
byte BspTri_IsTexel(int x0, int y0, int x, int y, uint32_t color, const BspTri *tri);
int BspTri_Zone(CNode3D *node);
const char *BspTri_ZoneName(const BspTri *tri);
int BspTri_Count(const BspTri *list);
/* the qsort order of zone-sorted lists */
int BspTri_CmpId(const void *a, const void *b);

/* the texel (u, v) of a light map at scale on the plane, the coordinate of axis from the plane */
void Bsp_TexelToWorld(vec3_t *out, float u, float v, int axis, float scale, const plane_t *pl);
void Bsp_WorldToTexel(float *out, const vec3_t *v, int axis, float scale);
/* point i of n of the Hammersley set in [0, 1) x [0, 1) */
float *Bsp_Hammersley(float *out, uint32_t i, uint32_t n);
/* the color as RGB bytes scaled to the largest channel and that channel */
void Bsp_ColorIntens(uint32_t *out, const float *rgb);

/* the light map pages */
void BspLMap_dtor(BspLMap *self);
void BspLMap_Init(BspLMap *self, int width, int height);
CMaterial *BspLMap_GetMat(const BspLMap *self);

/* a free place of w x h in the page (rounded up to the padding, maybe turned); 0 when full */
byte BspLMap_Find(const CBmp *bmp, int w, int h, int *rx, int *ry);
void BspLMap_Copy(CBmp *dst, int dx, int dy, const CBmpFlt2 *orig, const CBmp *mask, uint32_t ox, uint32_t oy,
	int w, int h);
/* the light maps of list into the page lm (and its secondary page), the light map uv moved there */
byte BspLMap_Insert(BspTri *list, BspLMap *lm, int w, int h);
/* the light maps of the lists (with the ones without) into new pages by material and sector, the pages saved as
 * "<scene>.%04d.LMAP1.tga" (LMAP2 the secondary ones, .INFO the debug colors) and their materials made from them */
void CBsp_GroupMaps(CBsp *self);

/* the light map computation helpers */
float *Bsp_DirSphere(float *out, float z, float u);
void Bsp_TexelBox(const box_t *b, int axis, float scale, int *x0, int *y0, int *x1, int *y1);
/* the boxes cover the same texels, or both fit a light map */
byte Bsp_SameSize(const box_t *a, int axis, float scale, const box_t *b);
/* the extents and the size of the light map; notify: a message for too large ones */
void BspTri_SetExt(BspTri *tri, int x0, int y0, int x1, int y1, byte notify);
byte BspTri_TexelOf(const BspTri *tri, const vec3_t *p, int *x, int *y);
byte BspTri_VertEqual(const BspTri *act, uint32_t nv, const BspTri *tri);
float *Bsp_SampleTex(float *out, float u, float v, int axis, float scale, const BspTri *tri, const CBmp *tex);
float *Bsp_SampleTexAA(float *out, int x, int y, int axis, float scale, const BspTri *tri, const CBmp *tex, int n);
CTable *BspTri_GetAreas(BspTri *tri, float dist, byte big);
float Bsp_FormFactor(const vec3_t *pa, const vec3_t *na, const vec3_t *pb, const vec3_t *nb, float area);
float BspRegion_FormFactor(const BspRegion *a, const BspRegion *b);
void BspRegion_Accumulate(BspRegion *r, const float *add);
void BspTri_SumTexel(const BspTri *tri, uint32_t x, uint32_t y, float *acc, int *n);
float *BspTri_Average(float *out, const BspTri *tri, int x, int y);
void BspTri_AreasLod(CTable *dst, const CTable *src, int w, int h, int dim);
BspRegion *BspRegion_New(int x, int y, int lx, int ly, BspRegion *parent, const CTable *areas);
void BspRegion_Split(BspRegion *r, const CTable *areas);
float *BspRegion_Color(float *out, BspRegion *r, BspTri *tri, uint32_t color);
void BspTri_Rotate(BspTri *list);
int BspTri_CmpMat(const void *a, const void *b);
int BspTri_CmpZone(const void *a, const void *b);
vec3_t *BspTri_Sample(vec3_t *out, int x, int y, int axis, float scale, const BspTri *tri, const BspTri *list,
	float *weight);
void Bsp_MatInfo(CMaterial *m);
int BspRegion_Debug(BspRegion *r);
void BspLMap_CreateMat(BspLMap *self, const char *name, byte level);
void BspLMap_Save(char *copy, CBmp *bmp, const char *path, int n, const char *ext);
void CBsp_AreasGlow(CBsp *self);
void CBsp_AreasMax(CBsp *self);
void CBsp_IllumRegions(CBsp *self, int n);
void CBsp_AdjustGamma(CBsp *self);
int BspRegion_Link(CBsp *self, BspRegion *a, BspRegion *b, int link, float threshold);
void CBsp_InitAreas(CBsp *self);
void BspTri_CalcGlow(BspTri *list);
float Bsp_Occlusion(const vec3_t *p, const vec3_t *n, float r, int samples, const vec3_t *light, CGround *ground);
void CBmpFlt_FillGaps(CBmpFlt *img, const CBmp *mask);
void CBsp_SaveIni(CBsp *self);
void Bsp_EndMessage(CBsp *self, int secs);

/* the rest */
void BspNode_dtor(BspNode *self);
void BspName_dtor(BspName *self);
BspZone *BspZone_ctor(BspZone *self);
void BspZone_dtor(BspZone *self);
BspZone *BspZone_Delete(BspZone *self, uint32_t flags);
void BspVB_Unwind(CRenderVB *self);
void Table_FreeB4(CTable *t);
CRenderVB *BspZone_GetVB(BspZone *self, int frmt);
void CBsp_dtor(CBsp *self);
void Bsp_MarkNode(CNode3D *node, byte release);
void CBsp_CreateZones(CBsp *self);
void CBsp_ColorSinLmap(CBsp *self);
void CBsp_RemoveLights(CBsp *self);

/* drawing */
void BspTri_Draw(BspTri *tri);
void CBsp_DrawTri(CBsp *self, BspTri *tri, byte pass);
void CBsp_DrawZone(CBsp *self, int sector, byte pass);
void CBsp_DrawPass(CBsp *self, byte pass);
void CBsp_DrawWire(CBsp *self);
void CBsp_DrawFX(CBsp *self);
void CBsp_Render(CBsp *self);
void Bsp_PortalInfo(byte info);
void CBsp_ReleaseD3D(CBsp *self);
/* the gates of mundo.cpp are in world.h; these are called by the renderer */
void Bsp_RenderStencil(void);
void Bsp_RenderTransp(void);
void Bsp_RenderPortals(void);

/* the map made from the floor: read from the .EMI (without calcular) or computed */
CBsp *CBsp_ctor(CBsp *self, const char *name, const vec3_t *scale);
/* the map compiler dialog */

/* the .EMI file: 0 when read, -1 without the file */
int CBsp_ReadEMI(CBsp *self);
/* the triangles by sector and by material into the log, at debug level 3 */
void CBsp_Prepare(CBsp *self);
/* the vertex and index buffers of the sectors and lists, the materials of the debug views */
void CBsp_InitD3D(CBsp *self);

/* the map compiler (BspCalc.c) */
/* the active lights of the scene into g_BspLights, all of them switched off; the ambient of [mapa_sombras] */
void Bsp_TakeLights(CScene *scn);
/* the texels of the triangles of list: the coverage into the low byte of cob (0xff when the texel is inside, else the
 * weight of its samples) where it is larger, the triangle into the masks; the texel box of each into ext2 */
void BspTri_Mask(BspTri *list, CBmp *cob);
/* the lists of the same material, page and sector joined (below 4096 triangles), sorted, their vertex lists */
void CBsp_CreateBuffers(CBsp *self);
/* the light maps light, light2 and rad2 of each list from the lights (shadows by tracing the floor), with the TXF caches */
void CBsp_DirectLight(CBsp *self);
/* the .EMI file written: 0, -1 on a write error */
int CBsp_SaveEMI(CBsp *self);
/* the global light of the radiosity (ilum_global, rebotes bounces) added to the light maps */
void CBsp_RadGlobal(CBsp *self);
/* the light of the area lights and glow maps (radiosidad) into rad */
void CBsp_RadAreas(CBsp *self);

/* the light maps computed: the lists of the meshes of the scene, their light, the pages and the .EMI */
void CBsp_Calculate(CBsp *self);

/* the float edits of the dialogs (src/game/engine3d/Engine/Misc.c): set returns 1, get 1 for a number */

/*
 * Engine/bsp2.cpp (Bsp2.c): the BSP tree of the triangles that cast shadows (CBsp.lights), split along the axis
 * planes through the middle of their box while both sides keep more than 5 triangles.
 */
typedef struct Bsp2Tri {
	vec3_t v[3];			/* 0x00 */
	plane_t plane;			/* 0x24 */
	CMaterial *Mat;			/* 0x34 */
} Bsp2Tri;
SD_SIZE(Bsp2Tri, 0x38);

typedef struct Bsp2Node {
	plane_t plane;			/* 0x00 */
	struct Bsp2Node *front;		/* 0x10 the side of the normal */
	struct Bsp2Node *back;		/* 0x14 */
	CTable tris;			/* 0x18 Bsp2Tri, of the leaves */
} Bsp2Node;
SD_SIZE(Bsp2Node, 0x20);

typedef struct BspLights {
	Bsp2Node *root;			/* 0x0 */
} BspLights;

/* the statistics of the line checks */
extern int g_Bsp2NTriCmp;
extern int g_Bsp2NComp;
extern int g_Bsp2Hits;
extern int g_Bsp2Misses;
/* the eye of the debug drawing and the color index of the next leaf */
extern vec3_t g_Bsp2Eye;
extern int g_Bsp2Color;

Bsp2Node *Bsp2Node_ctor(Bsp2Node *self);
void Bsp2Node_dtor(Bsp2Node *self);
void BspLights_dtor(void *self);
/* the segment a b blocked by a shadow triangle: the first hit into out (may be NULL) */
int BspLights_CheckLine(BspLights *self, const vec3_t *a, const vec3_t *b, vec3_t *out);
/* 1 blocked, -1 free */
int Bsp2_CheckLine(const vec3_t *a, const vec3_t *b, vec3_t *out, Bsp2Node *node);
/* the leaves as debug triangles, from the far side of the eye */
void BspLights_Render(void *self);
void Bsp2_Render(Bsp2Node *node);
/* a triangle of src with the corners v0 v1 v2 into the front (side 1) or the back child, or only counted */
void Bsp2_AddTri(byte countOnly, Bsp2Node *node, uint32_t *nFront, uint32_t *nBack, int side, const Bsp2Tri *src,
	const vec3_t *v0, const vec3_t *v1, const vec3_t *v2);
void Bsp2_Stats(int print);
/* the axis plane to split the leaf at, 0 for none */
byte Bsp2_ChoosePlane(BspLights *self, Bsp2Node *node, plane_t *dest);
/* the leaf below the root a point is in (the root when its child on that side is missing) */
Bsp2Node *BspLights_FindLeaf(BspLights *self, const vec3_t *p);
Bsp2Node *Bsp2_FindLeaf(const vec3_t *p, Bsp2Node *node);
/* the triangles of the tree */
int Bsp2_Count(const Bsp2Node *node);
Bsp2Tri *Bsp2Tri_Copy(Bsp2Tri *dest, const Bsp2Tri *src);
/* the leaf split until no plane is found, with a debug line a node */
void Bsp2_Build(BspLights *self, Bsp2Node *node, int depth);
/* the triangles of the leaf by pl into two new children (countOnly: only the counts); crossing ones go to both */
void Bsp2_Split(byte countOnly, Bsp2Node *node, const plane_t *pl, int *outFront, int *outBack);

/*
 * Engine/radiosidad.cpp (Radiosity.c): the regions and links of the radiosity, and the area lights of the
 * materials: a material named "(+area_light: name)" (MAT_AREALIGHT, older ones "(+name)") emits the light of the
 * section name of the .rad files of the map ("rgbi", "aten", "falloff").
 */
typedef struct RadAreaLight {
	struct RadAreaLight *next;		/* 0x00 */
	char *name;			/* 0x04 String_Set */
	byte rgbi[4];			/* 0x08 r g b, from the diffuse color of the material */
	float intensity;		/* 0x0c */
	float unused_10;			/* 0x10 */
	float atten[2];			/* 0x14 */
	float falloff;			/* 0x1c */
} RadAreaLight;
SD_SIZE(RadAreaLight, 0x20);

typedef struct CRadiosity {
	CIniFile ini;			/* 0x0 the .rad files */
	CHashTable *table;		/* 0x4 the area lights by material */
	RadAreaLight *list;		/* 0x8 */
} CRadiosity;
SD_SIZE(CRadiosity, 0xc);

/* the links alive */
extern int g_BspNumLinks;
/* the unit normals of bmp\normales.rad, read once */
extern CTable g_RadNormals;

BspLink *BspLink_ctor(BspLink *self, BspLink *next, BspRegion *region, float ff);
/* the next links deleted */
void BspLink_dtor(BspLink *self);
BspLink *BspLink_Delete(BspLink *self, byte flags);
RadAreaLight *RadAreaLight_ctor(RadAreaLight *self);
RadAreaLight *RadAreaLight_Delete(RadAreaLight *self, byte flags);
/* the color and the plane of parent */
BspRegion *BspRegion_ctor(BspRegion *self, int x, int y, int lx, int ly, BspRegion *parent);
void BspRegion_dtor(BspRegion *self);
/* the .rad files: name.rad and lights.rad of dir (may be NULL), then the global one */
CRadiosity *CRadiosity_ctor(CRadiosity *self, const char *dir);
void CRadiosity_dtor(CRadiosity *self);
/* the area light of Mat, read when it is not in the table yet (NULL for none) */
RadAreaLight *CRadiosity_Get(CRadiosity *self, CMaterial *Mat);
RadAreaLight *CRadiosity_Add(CRadiosity *self, CMaterial *Mat);
RadAreaLight *CRadiosity_Find(CRadiosity *self, CMaterial *Mat);
/* the section of the area light named in the material name into dest (may be NULL): 1 found */
byte CRadiosity_ReadMat(CRadiosity *self, CMaterial *Mat, RadAreaLight *dest);
/* rgbi and intensity of the area light of Mat: 1 found */
byte CRadiosity_GetColor(CRadiosity *self, CMaterial *Mat, void *dest);
/* CRadiosity_ReadMat with the warnings of a missing or old definition */
byte CRadiosity_ReadMatWarn(CRadiosity *self, CMaterial *Mat, RadAreaLight *dest);
/* the normals and their count */
vec3_t *Rad_Normals(uint32_t *num);

#endif
