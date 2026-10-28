/*
 * Engine/bsp.cpp of the engine3d: the light maps of the floor ("mapa de sombras"). With [mapa_sombras] usar the
 * triangles of the floor model are grouped into lists (BspTri, one per surface, linked by sig) that get a light map
 * each: computed (shadows by tracing the floor, radiosity over a tree of regions per list, TXF caches of the result)
 * or read from the .EMI file of the map. The light maps are packed into pages (BspLMap) that become the second texture
 * of the lists, drawn by sector (BspZona) with their own vertex buffers. Off in the game: the global functions return
 * at once without usar (g_MapaSombrasUsar, mundo.h).
 */
#ifndef SCRAP_BSP_H
#define SCRAP_BSP_H

#include "scrap/bmpflt.h"
#include "scrap/listavert.h"
#include "scrap/luz.h"
#include "scrap/material.h"
#include "scrap/mundo.h"
#include "scrap/render.h"
#include "scrap/tabla.h"
#include "scrap/tablahash.h"

#define BSP_MAX_LISTAS 80000
#define BSP_MAX_NOLMAP 20000
#define BSP_MAX_LUCES 512
#define BSP_MAX_LISTATRI 1024

/* a link of the radiosity between two regions */
typedef struct BspEnlace {
	struct BspEnlace *sig;		/* 0x0 */
	struct BspRegion *region;	/* 0x4 */
	float ff;			/* 0x8 form factor */
} BspEnlace;

/* a node of the region tree of a list (Engine radiosity code, 100 bytes) */
typedef struct BspRegion {
	struct BspRegion *hijo[4];	/* 0x00 ra rb rc rd */
	struct BspRegion *padre;	/* 0x10 */
	byte interior;			/* 0x14 any child */
	byte pad15[3];
	float area;			/* 0x18 */
	vec3_t centro;			/* 0x1c */
	plane_t plano;			/* 0x28 */
	uint16_t x, y;			/* 0x38 texel of the light map */
	uint16_t lx, ly;		/* 0x3c size in texels */
	float color[4];			/* 0x40 emitted */
	float acum[4];			/* 0x50 received */
	BspEnlace *enlaces;		/* 0x60 */
} BspRegion;
SD_OFFSET(BspRegion, centro, 0x1c);
SD_OFFSET(BspRegion, acum, 0x50);
SD_SIZE(BspRegion, 0x64);

/* the names of the area lights of a list */
typedef struct BspNombre {
	struct BspNombre *sig;		/* 0x00 */
	char *nombre;			/* 0x04 Cad */
	byte pad08[0x18];
} BspNombre;
SD_SIZE(BspNombre, 0x20);

/* a page of light maps; lmap2 is the page of the secondary maps */
typedef struct BspLMap {
	struct BspLMap *sig;		/* 0x00 */
	CBmp bmp;			/* 0x04 */
	CBmp info;			/* 0x10 the debug colors (.INFO) */
	CMaterial *Mat;			/* 0x1c */
	int minX, minY;			/* 0x20 the smallest map that did not fit, size + 1 when empty */
	int id;				/* 0x28 */
	struct BspLMap *lmap2;		/* 0x2c */
} BspLMap;
SD_SIZE(BspLMap, 0x30);

/* a light map texel of a list: the sample point and its weight (CTabla of 0x18 bytes) */
typedef struct BspArea {
	vec3_t pos;			/* 0x00 */
	float peso;			/* 0x0c */
	uint16_t x, y;			/* 0x10 */
	float f14;			/* 0x14 */
} BspArea;
SD_SIZE(BspArea, 0x18);

/* a list: triangles of one surface with one light map (0x224 bytes) */
typedef struct BspTri {
	struct BspTri *sig;		/* 0x000 the next triangle of the list */
	vec3_t v[3];			/* 0x004 */
	vec3_t n[3];			/* 0x028 vertex normals */
	float uv[3][2];			/* 0x04c */
	float uvl[3][2];		/* 0x064 in the light map */
	uint32_t color[3];		/* 0x07c */
	int vid[3];			/* 0x088 vertex ids */
	plane_t plano;			/* 0x094 */
	int eje;			/* 0x0a4 the largest axis of the normal */
	uint32_t flags;			/* 0x0a8 BSPTRI_* */
	CMaterial *Mat;			/* 0x0ac */
	CBmpFlt2 luz;			/* 0x0b0 the light map */
	CBmpFlt2 luz2;			/* 0x0bc the secondary one */
	CBmpFlt luzA;			/* 0x0c8 */
	int lx, ly;			/* 0x0d4 light map size */
	int ext[4];			/* 0x0dc x0 y0 x1 y1 */
	int ext2[4];			/* 0x0ec */
	CBmpFlt2 rad;			/* 0x0fc radiosity */
	CBmpFlt2 rad2;			/* 0x108 */
	CBmp mask;			/* 0x114 the list of each texel (color swapped) */
	CBmp mask2;			/* 0x120 */
	int i12c;			/* 0x12c */
	int i130;			/* 0x130 */
	BspNombre *nombres;		/* 0x134 */
	char *nombre;			/* 0x138 Cad */
	uint32_t radColor;		/* 0x13c area light color, bytes */
	float radInt;			/* 0x140 */
	float col144[4];		/* 0x144 */
	byte radluz_ok;			/* 0x154 */
	byte b155;			/* 0x155 */
	byte grande;			/* 0x156 */
	byte pad157;
	float f158[4];			/* 0x158 */
	float f168[4];			/* 0x168 */
	float f178;			/* 0x178 */
	int i17c;			/* 0x17c */
	int i180;			/* 0x180 */
	CTabla areas[4];		/* 0x184 BspArea, by level of detail */
	int numAreas;			/* 0x1a4 */
	vec3_t centro;			/* 0x1a8 */
	float radio;			/* 0x1b4 */
	float area;			/* 0x1b8 */
	BspRegion *region;		/* 0x1bc */
	BspLMap *LMap;			/* 0x1c0 */
	CNodo3D *nodo;			/* 0x1c4 */
	int zona;			/* 0x1c8 sector, -1 none */
	struct BspTri *zona_sig;	/* 0x1cc the next list of the sector */
	int numTri;			/* 0x1d0 */
	uint16_t *pTri;			/* 0x1d4 3 indices a triangle */
	CListaVert lv;			/* 0x1d8 */
	CListaVert lv2;			/* 0x1ec */
	CListaVert lv3;			/* 0x200 */
	void *pVertVB;			/* 0x214 IDirect3DVertexBuffer8 */
	void *pVertIB;			/* 0x218 */
	int baseVert;			/* 0x21c of the vertex buffer */
	int startTri;			/* 0x220 */
} BspTri;
SD_OFFSET(BspTri, plano, 0x94);
SD_OFFSET(BspTri, Mat, 0xac);
SD_OFFSET(BspTri, mask, 0x114);
SD_OFFSET(BspTri, nombre, 0x138);
SD_OFFSET(BspTri, f178, 0x178);
SD_OFFSET(BspTri, areas, 0x184);
SD_OFFSET(BspTri, region, 0x1bc);
SD_OFFSET(BspTri, lv, 0x1d8);
SD_OFFSET(BspTri, pVertVB, 0x214);
SD_SIZE(BspTri, 0x224);

#define BSPTRI_NOLMAP 0x1		/* flags: no light map, the vertex colors */

/* the vertex and index buffers of the lists without light map (0x2c bytes) */
typedef struct BspNodo {
	struct BspNodo *sig;		/* 0x00 */
	int i04, i08, i0c;		/* 0x04 */
	uint16_t *pTri;			/* 0x10 */
	CListaVert lv;			/* 0x14 */
	void *pVertVB;			/* 0x28 */
} BspNodo;
SD_SIZE(BspNodo, 0x2c);

/* a sector: its lists (through zona_sig) and the vertex buffers by vertex format */
typedef struct BspZona {
	BspTri *tris;			/* 0x00 */
	int i04;			/* 0x04 */
	CRenderVB vb[8];		/* 0x08 */
	CRenderIB ib;			/* 0xa8 */
} BspZona;
SD_SIZE(BspZona, 0xb4);

/* the map (gBSP, 0x61aa8 bytes) */
typedef struct CBsp {
	char *nombre;			/* 0x00000 Cad, the scene path */
	CModelo *modelo;		/* 0x00004 the map model */
	BspTri *listas[BSP_MAX_LISTAS];	/* 0x00008 */
	int num_listas;			/* 0x4e208 */
	BspTri *nolmap[BSP_MAX_NOLMAP];	/* 0x4e20c */
	int num_nolmap;			/* 0x61a8c */
	BspNodo *nodos;			/* 0x61a90 */
	void *luces;			/* 0x61a94 4 bytes, the stencil shadows */
	CTabla zonas;			/* 0x61a98 BspZona, zonas[0] for zona -1 */
	CMaterial *matFX;		/* 0x61aa0 */
	CMaterial *matWire;		/* 0x61aa4 the wire frame material */
} CBsp;
SD_OFFSET(CBsp, num_listas, 0x4e208);
SD_OFFSET(CBsp, num_nolmap, 0x61a8c);
SD_OFFSET(CBsp, zonas, 0x61a98);
SD_SIZE(CBsp, 0x61aa8);

/* the TXF cache of the computed light maps: one file a list, "<map>_TXF/<name>.<ext>.<n>.%05d.TXF" */
typedef struct BspTxf {
	byte on;			/* 0x000 */
	byte pad01[3];
	void *FileTXF;			/* 0x004 FILE of the exe, the lock while writing */
	char base[0x104];		/* 0x008 */
	char nombre[0x104];		/* 0x10c */
	char equipo[0x20];		/* 0x210 COMPUTERNAME */
	int written;			/* 0x230 */
	int skipped;			/* 0x234 */
	int locked;			/* 0x238 */
	int writeRetries;		/* 0x23c */
	int readRetries;		/* 0x240 */
	int deleted;			/* 0x244 */
	int rebuild;			/* 0x248 */
	uint32_t inicio;		/* 0x24c timeGetTime */
} BspTxf;
SD_OFFSET(BspTxf, equipo, 0x210);
SD_SIZE(BspTxf, 0x250);

/* a light of the compiler, taken from the lights of the scene (0x118 bytes) */
typedef struct BspLuz {
	vec3_t pos;			/* 0x00 */
	vec3_t dir;			/* 0x0c unit, (2 -1 1) for the omni lights */
	byte puntual;			/* 0x18 not a directional light */
	byte sombra;			/* 0x19 */
	byte stencil;			/* 0x1a the shadows of the node too (NODO_SOMBRA) */
	byte noLmap;			/* 0x1b node flag 27 */
	float rad;			/* 0x1c */
	D3DLight d3d;			/* 0x20 its position and direction from pos and dir */
	float f88;			/* 0x88 */
	float v8c[3];			/* 0x8c */
	char nombre[0x80];		/* 0x98 of the node */
} BspLuz;
SD_OFFSET(BspLuz, d3d, 0x20);
SD_OFFSET(BspLuz, nombre, 0x98);
SD_SIZE(BspLuz, 0x118);

#define g_Bsp GAME_DATA(CBsp *, 0x4b2b38)
#define g_BspLMaps GAME_DATA(BspLMap *, 0x4b2b3c)	/* the pages */
#define g_BspTriTmp GAME_DATA(BspTri *, 0x4b2b40)
#define g_pBSP_VB GAME_DATA(void *, 0x4d5d70)
#define g_BspNumLuces GAME_DATA(int, 0x4b2b44)
#define g_BspLuces ((BspLuz *)Game_Ptr(0x4b2c20))	/* [BSP_MAX_LUCES] */
#define g_LmapInfo GAME_DATA(int, 0x4b2b5c)		/* the .INFO maps */
#define g_LmapInfo2 GAME_DATA(int, 0x432990)
#define g_TxfMode GAME_DATA(int, 0x4b2b74)		/* the TXF loading of an .EMI (2 deletes) */
#define g_R_ForceLMap GAME_DATA(int, 0x4d5c78)	/* gvar R_ForceLMap, -1 */
#define g_LmapSecond GAME_DATA(int, 0x4b2b80)		/* the secondary maps (stencil pass 2) */
#define g_RenderMap GAME_DATA(int, 0x4d5ca8)		/* gvar RenderMap */
#define g_LightmapPackMode GAME_DATA(int, 0x4b2bf4)	/* gvar */
#define g_LightmapDxtPadding GAME_DATA(int, 0x4d5c30)	/* gvar */
#define g_LightmapLimitUV GAME_DATA(int, 0x4d5d68)	/* gvar */
#define g_MapDrawBySector GAME_DATA(int, 0x4d5c60)	/* gvar */
#define g_MapDrawSector GAME_DATA(int, 0x4b2bdc)	/* gvar, -2 all */
#define g_MapRenderMap GAME_DATA(const char *, 0x4b2c0c)	/* gvar, a wildcard of map names */
#define g_MapRenderMat GAME_DATA(const char *, 0x4d5c90)	/* gvar, a wildcard of material names */
#define g_MapSortByShader GAME_DATA(int, 0x4d5cd8)	/* gvar */
#define g_RenderSectors GAME_DATA(int, 0x4b2bc4)	/* gvar */
#define g_TXF_Helper GAME_DATA(int, 0x4b2b94)		/* gvar */
#define g_ShowInfoOnSectorError GAME_DATA(int, 0x4d5c48)	/* gvar */
#define g_Gamma2x GAME_DATA(int, 0x4db878)		/* the gamma2x of the video settings */
/* the keys that switch the wire frame: F8 (cleared) with X held */
#define g_KeyF8 GAME_DATA(byte, 0x4db302)
#define g_KeyX GAME_DATA(byte, 0x4db2ed)
#define g_StencilRender GAME_DATA(void *, 0x4afdbc)	/* the floor drawn again by the stencil shadows */
#define g_BspTris GAME_DATA(int, 0x4b2b78)		/* drawn this frame */
#define g_BspVerts GAME_DATA(int, 0x4b2b7c)
#define g_ListaTri ((BspTri **)Game_Ptr(0x4d5d90))	/* [BSP_MAX_LISTATRI] the lists of a pass to sort */
/* the sector of the eye: found this frame, the sector, the frames without one, the eye of the last frame */
#define g_BspSecOk GAME_DATA(byte, 0x4d5d88)
#define g_BspSecVisto GAME_DATA(int, 0x4b2b34)
#define g_BspSecErrores GAME_DATA(int, 0x4d5d8c)
#define g_BspOjoAnt GAME_DATA(vec3_t, 0x4b2c14)
#define g_BspLodLevel GAME_DATA(float, 0x4b2b64)
/* the distances of the level of detail of the areas (rva 0x432974..0x432988), plain and for large lists */
#define g_BspLodDist ((const float *)Game_Ptr(0x432974))
#define g_BspLodCount ((int *)Game_Ptr(0x4b2b48))
#define g_BspColores ((const uint32_t *)Game_Ptr(0x432a30))	/* 16 debug colors */
/* the globals of m3d.ini ([mapa_sombras], read in ScrapEngMain.c, the same definitions) */
#define g_MapaSombrasMultitext GAME_DATA(int, 0x4b2b00)
#define g_MapaSombrasEscala GAME_DATA(float, 0x432950)
#define g_MapaSombrasResol GAME_DATA(float, 0x432954)
#define g_MapaSombrasTexX GAME_DATA(int, 0x432958)
#define g_MapaSombrasTexY GAME_DATA(int, 0x43295c)
#define g_MapaSombrasRadiosidad GAME_DATA(int, 0x4b2b10)
#define g_MapaSombrasModulate4x GAME_DATA(int, 0x4b2b1c)
#define g_MapaSombrasUsarTxf GAME_DATA(int, 0x4b2b24)

/* engine3d/engine/bmp*.h instanced here */
void __fastcall CBmpFlt2_dtor(CBmpFlt2 *self);
void __fastcall CBmpFlt_dtor(CBmpFlt *self);
void __thiscall CBmp_Assign(CBmp *self, const CBmp *orig);
void __thiscall CBmpFlt2_Assign(CBmpFlt2 *self, const CBmpFlt2 *orig);
/* new pixels filled with color */
void __thiscall CBmp_CreateFill(CBmp *self, int ancho, int alto, uint32_t color);
void __thiscall CBmpFlt2_CreateFill(CBmpFlt2 *self, int ancho, int alto, const float *color);
void __thiscall CBmpFlt_CreateFill(CBmpFlt *self, int ancho, int alto, const float *color);
/* orig turned by 90 degrees (the size swapped) */
void __thiscall CBmp_Rotate(CBmp *self, const CBmp *orig);
void __thiscall CBmpFlt2_Rotate(CBmpFlt2 *self, const CBmpFlt2 *orig);

/* the TXF cache */
BspTxf *__thiscall BspTxf_ctor(BspTxf *self, const char *mapa, const char *name, int n, int on);
void __fastcall BspTxf_dtor(BspTxf *self);
void __thiscall BspTxf_SetName(BspTxf *self, int i);
void __fastcall BspTxf_Info(BspTxf *self);
byte __thiscall BspTxf_Check(BspTxf *self, int i, byte def, byte nl);
void __thiscall BspTxf_Guardar(BspTxf *self, CBmpFlt2 *bmp);
void __thiscall BspTxf_Cargar(BspTxf *self, BspTri **listas, int n, int cual);

/* the lists */
BspTri *__fastcall BspTri_ctor(BspTri *self);
void __fastcall BspTri_dtor(BspTri *self);
void BspTri_CalcCentro(BspTri *tri);
box_t *BspTri_ListBox(box_t *out, const BspTri *list);
box_t *BspTri_AddBox(box_t *out, const box_t *in, const BspTri *tri);
void BspTri_CalcPlanos(BspTri *list);
int BspTri_Lado(const BspTri *tri);
/* p on the plane of the triangle: the normal and the uv (light map uv with lmap) at p */
void BspTri_Interp(const BspTri *tri, const vec3_t *p, vec3_t *n, float *uv, byte lmap);
void BspTri_GetExt(const BspTri *tri, int *x0, int *y0, int *x1, int *y1);
void BspTri_GetExt2(const BspTri *tri, int *x0, int *y0, int *x1, int *y1);
/* the texel x, y of a map at (x0, y0) belongs to the list of color (mask colors are R and B swapped) */
byte BspTri_EsTexel(int x0, int y0, int x, int y, uint32_t color, const BspTri *tri);
int BspTri_Zona(CNodo3D *nodo);
const char *BspTri_ZonaName(const BspTri *tri);
int BspTri_Count(const BspTri *list);
/* the qsort order of zona-sorted lists */
int BspTri_CmpId(const void *a, const void *b);

/* the texel (u, v) of a light map at scale on the plane, the coordinate of axis from the plane */
void Bsp_TexelToWorld(vec3_t *out, float u, float v, int axis, float scale, const plane_t *pl);
void Bsp_WorldToTexel(float *out, const vec3_t *v, int axis, float scale);
/* point i of n of the Hammersley set in [0, 1) x [0, 1) */
float *Bsp_Hammersley(float *out, uint32_t i, uint32_t n);
/* the color as RGB bytes scaled to the largest channel and that channel */
void Bsp_ColorIntens(uint32_t *out, const float *rgb);

/* the light map pages */
void __fastcall BspLMap_dtor(BspLMap *self);
void __thiscall BspLMap_Init(BspLMap *self, int ancho, int alto);
CMaterial *BspLMap_GetMat(const BspLMap *self);

/* a free place of w x h in the page (rounded up to the padding, maybe turned); 0 when full */
byte BspLMap_Buscar(const CBmp *bmp, int w, int h, int *rx, int *ry);
void BspLMap_Copiar(CBmp *dst, int dx, int dy, const CBmpFlt2 *orig, const CBmp *mask, uint32_t ox, uint32_t oy,
	int w, int h);
/* the light maps of list into the page lm (and its secondary page), the light map uv moved there */
byte BspLMap_Insertar(BspTri *list, BspLMap *lm, int w, int h);

/* the light map computation helpers */
float *Bsp_DirEsfera(float *out, float z, float u);
void Bsp_TexelBox(const box_t *b, int axis, float scale, int *x0, int *y0, int *x1, int *y1);
/* the boxes cover the same texels, or both fit a light map */
byte Bsp_MismoTam(const box_t *a, int axis, float scale, const box_t *b);
/* the extents and the size of the light map; avisar: a message for too large ones */
void BspTri_SetExt(BspTri *tri, int x0, int y0, int x1, int y1, byte avisar);
byte __thiscall BspTri_TexelOf(const BspTri *tri, const vec3_t *p, int *x, int *y);
byte BspTri_VertIgual(const BspTri *act, uint32_t nv, const BspTri *tri);
float *Bsp_SampleTex(float *out, float u, float v, int axis, float scale, const BspTri *tri, const CBmp *tex);
float *Bsp_SampleTexAA(float *out, int x, int y, int axis, float scale, const BspTri *tri, const CBmp *tex, int n);
CTabla *BspTri_GetAreas(BspTri *tri, float dist, byte grande);
float Bsp_FormFactor(const vec3_t *pa, const vec3_t *na, const vec3_t *pb, const vec3_t *nb, float area);
float BspRegion_FormFactor(const BspRegion *a, const BspRegion *b);
void BspRegion_Acumular(BspRegion *r, const float *add);
void BspTri_SumTexel(const BspTri *tri, uint32_t x, uint32_t y, float *acc, int *n);
float *BspTri_Media(float *out, const BspTri *tri, int x, int y);
void BspTri_AreasLod(CTabla *dst, const CTabla *src, int w, int h, int dim);
BspRegion *BspRegion_New(int x, int y, int lx, int ly, BspRegion *padre, const CTabla *areas);
void BspRegion_Dividir(BspRegion *r, const CTabla *areas);
float *BspRegion_Color(float *out, BspRegion *r, BspTri *tri, uint32_t color);
void BspTri_Rotar(BspTri *list);
int BspTri_CmpMat(const void *a, const void *b);
int BspTri_CmpZona(const void *a, const void *b);
vec3_t *BspTri_Muestra(vec3_t *out, int x, int y, int axis, float scale, const BspTri *tri, const BspTri *list,
	float *peso);
void Bsp_MatInfo(CMaterial *m);
int BspRegion_Debug(BspRegion *r);
void __thiscall BspLMap_CrearMat(BspLMap *self, const char *name, byte nivel);
void BspLMap_Guardar(char *copia, CBmp *bmp, const char *ruta, int n, const char *ext);
void __fastcall CBsp_AreasGlow(CBsp *self);
void __fastcall CBsp_AreasMax(CBsp *self);
void __thiscall CBsp_IlumRegiones(CBsp *self, int n);
void __fastcall CBsp_AjustarGamma(CBsp *self);
int __thiscall BspRegion_Enlazar(CBsp *self, BspRegion *a, BspRegion *b, int enlace, float umbral);
void __fastcall CBsp_IniciarRegiones(CBsp *self);
void __fastcall CBsp_IniciarAreas(CBsp *self);
void BspTri_CalcGlow(BspTri *list);
float Bsp_Oclusion(const vec3_t *p, const vec3_t *n, float r, int samples, const vec3_t *luz, CSuelo *suelo);
void CBmpFlt_Rellenar(CBmpFlt *img, const CBmp *mask);
void __fastcall CBsp_GuardarIni(CBsp *self);
void __thiscall Bsp_FinMensaje(CBsp *self, int secs);

/* the rest */
void __fastcall BspNodo_dtor(BspNodo *self);
void __fastcall BspNombre_dtor(BspNombre *self);
BspZona *__fastcall BspZona_ctor(BspZona *self);
void __fastcall BspZona_dtor(BspZona *self);
BspZona *__thiscall BspZona_Delete(BspZona *self, uint32_t flags);
void __fastcall BspVB_Unwind(CRenderVB *self);
void __fastcall Tabla_FreeB4(CTabla *t);
CRenderVB *__thiscall BspZona_GetVB(BspZona *self, int frmt);
void __fastcall CBsp_dtor(CBsp *self);
void Bsp_NodoMarcar(CNodo3D *nodo, byte liberar);
void __fastcall CBsp_CrearZonas(CBsp *self);
void __fastcall CBsp_ColorSinLmap(CBsp *self);
void __fastcall CBsp_QuitarLuces(CBsp *self);

/* drawing */
void __stdcall BspTri_Draw(BspTri *tri);
void __thiscall CBsp_DrawTri(CBsp *self, BspTri *tri, byte pass);
void __thiscall CBsp_DrawZona(CBsp *self, int sector, byte pass);
void __thiscall CBsp_DrawPass(CBsp *self, byte pass);
void __fastcall CBsp_DrawWire(CBsp *self);
void __fastcall CBsp_DrawFX(CBsp *self);
void __fastcall CBsp_Render(CBsp *self);
void Bsp_PortalInfo(byte info);
void __fastcall CBsp_ReleaseD3D(CBsp *self);
/* the gates of mundo.cpp are in mundo.h; these are called by the renderer */
void Bsp_RenderStencil(void);
void Bsp_RenderTransp(void);
void Bsp_RenderPortales(void);

/* the map made from the floor: read from the .EMI (without calcular) or computed */
CBsp *__thiscall CBsp_ctor(CBsp *self, const char *nombre, const vec3_t *escala);
/* the map compiler dialog */
void Bsp_DlgEnable(void *hwnd);
intptr_t __stdcall Bsp_DlgProc(void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);

/* the .EMI file: 0 when read, -1 without the file */
int __fastcall CBsp_LeerEMI(CBsp *self);
/* the triangles by sector and by material into the log, at debug level 3 */
void __fastcall CBsp_Preparar(CBsp *self);
/* the vertex and index buffers of the sectors and lists, the materials of the debug views */
void __fastcall CBsp_InitD3D(CBsp *self);

/* the map compiler (BspCalc.c) */
/* the active lights of the scene into g_BspLuces, all of them switched off; the ambient of [mapa_sombras] */
void Bsp_TomarLuces(CEscena *esc);

/* exe functions of this file still the exe's */
void __fastcall CBsp_Calcular(CBsp *self);		/* rva 0x28a860, the light maps computed */

/* the float edits of the dialogs (src/game/engine3d/Engine/Varios.c): set returns 1, get 1 for a number */
byte Dlg_SetFloat(void *hwnd, int id, float v, int prec);
byte Dlg_GetFloat(void *hwnd, int id, float *out);

/*
 * Engine/bsp2.cpp (Bsp2.c): the BSP tree of the triangles that cast shadows (CBsp.luces), split along the axis
 * planes through the middle of their box while both sides keep more than 5 triangles.
 */
typedef struct Bsp2Tri {
	vec3_t v[3];			/* 0x00 */
	plane_t plano;			/* 0x24 */
	CMaterial *Mat;			/* 0x34 */
} Bsp2Tri;
SD_SIZE(Bsp2Tri, 0x38);

typedef struct Bsp2Nodo {
	plane_t plano;			/* 0x00 */
	struct Bsp2Nodo *front;		/* 0x10 the side of the normal */
	struct Bsp2Nodo *back;		/* 0x14 */
	CTabla tris;			/* 0x18 Bsp2Tri, of the leaves */
} Bsp2Nodo;
SD_SIZE(Bsp2Nodo, 0x20);

typedef struct BspLuces {
	Bsp2Nodo *root;			/* 0x0 */
} BspLuces;

/* the statistics of the line checks */
#define g_Bsp2NTriCmp GAME_DATA(int, 0x4e738c)
#define g_Bsp2NComp GAME_DATA(int, 0x4e7390)
#define g_Bsp2Hits GAME_DATA(int, 0x4e7394)
#define g_Bsp2Misses GAME_DATA(int, 0x4e7398)
/* the eye of the debug drawing and the color index of the next leaf */
#define g_Bsp2Eye GAME_DATA(vec3_t, 0x4e739c)
#define g_Bsp2Color GAME_DATA(int, 0x4e73a8)

Bsp2Nodo *__fastcall Bsp2Nodo_ctor(Bsp2Nodo *self);
void __fastcall Bsp2Nodo_dtor(Bsp2Nodo *self);
void __fastcall BspLuces_dtor(void *self);
/* the segment a b blocked by a shadow triangle: the first hit into out (may be NULL) */
int __thiscall BspLuces_CheckLine(BspLuces *self, const vec3_t *a, const vec3_t *b, vec3_t *out);
/* 1 blocked, -1 free */
int Bsp2_CheckLine(const vec3_t *a, const vec3_t *b, vec3_t *out, Bsp2Nodo *node);
/* the leaves as debug triangles, from the far side of the eye */
void __fastcall BspLuces_Render(void *self);
void Bsp2_Render(Bsp2Nodo *node);
/* a triangle of src with the corners v0 v1 v2 into the front (lado 1) or the back child, or only counted */
void Bsp2_AddTri(byte countOnly, Bsp2Nodo *node, uint32_t *nFront, uint32_t *nBack, int lado, const Bsp2Tri *src,
	const vec3_t *v0, const vec3_t *v1, const vec3_t *v2);
void Bsp2_Stats(int print);
/* the axis plane to split the leaf at, 0 for none */
byte __thiscall Bsp2_ChoosePlane(BspLuces *self, Bsp2Nodo *node, plane_t *dest);
/* the leaf below the root a point is in (the root when its child on that side is missing) */
Bsp2Nodo *__thiscall BspLuces_FindLeaf(BspLuces *self, const vec3_t *p);
Bsp2Nodo *Bsp2_FindLeaf(const vec3_t *p, Bsp2Nodo *node);
/* the triangles of the tree */
int Bsp2_Count(const Bsp2Nodo *node);
Bsp2Tri *Bsp2Tri_Copy(Bsp2Tri *dest, const Bsp2Tri *src);
/* the leaf split until no plane is found, with a debug line a node */
void __thiscall Bsp2_Build(BspLuces *self, Bsp2Nodo *node, int depth);
/* the triangles of the leaf by pl into two new children (countOnly: only the counts); crossing ones go to both */
void Bsp2_Split(byte countOnly, Bsp2Nodo *node, const plane_t *pl, int *outFront, int *outBack);

/*
 * Engine/radiosidad.cpp (Radiosidad.c): the regions and links of the radiosity, and the area lights of the
 * materials: a material named "(+area_light: name)" (MAT_AREALIGHT, older ones "(+name)") emits the light of the
 * section name of the .rad files of the map ("rgbi", "aten", "falloff").
 */
typedef struct RadAreaLuz {
	struct RadAreaLuz *sig;		/* 0x00 */
	char *nombre;			/* 0x04 Cad_Set */
	byte rgbi[4];			/* 0x08 r g b, from the diffuse color of the material */
	float intensidad;		/* 0x0c */
	float f10;			/* 0x10 */
	float aten[2];			/* 0x14 */
	float falloff;			/* 0x1c */
} RadAreaLuz;
SD_SIZE(RadAreaLuz, 0x20);

typedef struct CRadiosidad {
	CArchINI ini;			/* 0x0 the .rad files */
	CTablaHash *tabla;		/* 0x4 the area lights by material */
	RadAreaLuz *lista;		/* 0x8 */
} CRadiosidad;
SD_SIZE(CRadiosidad, 0xc);

/* the links alive */
#define g_BspNumEnlaces GAME_DATA(int, 0x4e73ac)
/* the unit normals of bmp\normales.rad, read once */
#define g_RadNormales GAME_DATA(CTabla, 0x4e73b0)

BspEnlace *__thiscall BspEnlace_ctor(BspEnlace *self, BspEnlace *sig, BspRegion *region, float ff);
/* the next links deleted */
void __fastcall BspEnlace_dtor(BspEnlace *self);
BspEnlace *__thiscall BspEnlace_Delete(BspEnlace *self, byte flags);
RadAreaLuz *__fastcall RadAreaLuz_ctor(RadAreaLuz *self);
RadAreaLuz *__thiscall RadAreaLuz_Delete(RadAreaLuz *self, byte flags);
/* the color and the plane of padre */
BspRegion *__thiscall BspRegion_ctor(BspRegion *self, int x, int y, int lx, int ly, BspRegion *padre);
void __fastcall BspRegion_dtor(BspRegion *self);
/* the .rad files: name.rad and luces.rad of dir (may be NULL), then the global one */
CRadiosidad *__thiscall CRadiosidad_ctor(CRadiosidad *self, const char *dir);
void __fastcall CRadiosidad_dtor(CRadiosidad *self);
/* the area light of Mat, read when it is not in the table yet (NULL for none) */
RadAreaLuz *__thiscall CRadiosidad_Get(CRadiosidad *self, CMaterial *Mat);
RadAreaLuz *__thiscall CRadiosidad_Add(CRadiosidad *self, CMaterial *Mat);
RadAreaLuz *__thiscall CRadiosidad_Find(CRadiosidad *self, CMaterial *Mat);
/* the section of the area light named in the material name into dest (may be NULL): 1 found */
byte __thiscall CRadiosidad_ReadMat(CRadiosidad *self, CMaterial *Mat, RadAreaLuz *dest);
/* rgbi and intensidad of the area light of Mat: 1 found */
byte __thiscall CRadiosidad_GetColor(CRadiosidad *self, CMaterial *Mat, void *dest);
/* CRadiosidad_ReadMat with the warnings of a missing or old definition */
byte __thiscall CRadiosidad_ReadMatWarn(CRadiosidad *self, CMaterial *Mat, RadAreaLuz *dest);
/* the normals and their count */
vec3_t *Rad_Normales(uint32_t *num);

#endif
