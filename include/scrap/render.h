/*
 * Engine/render.cpp of the engine3d: the renderer over the D3D 8 device (g_pd3dDevice): the frame state (matrices,
 * viewport, frustum, render pass), the material and world matrix of each draw, pixel shaders, dynamic vertex and
 * index buffers, clears and the debug primitives. Decompiled in parts (src/game/engine3d/Engine/Render.c); the rest is
 * still the exe's, the rva of each of those is given.
 */
#ifndef SCRAP_RENDER_H
#define SCRAP_RENDER_H

#include "scrap/gvar.h"
#include "scrap/cstring.h"
#include "scrap/d3d8.h"
#include "scrap/mathlib.h"
#include "scrap/table.h"

struct CMaterial;
typedef struct CMaterial CMaterial;
struct CVertList;

/* the D3D device of the engine3d, NULL without one */
extern void *g_pd3dDevice;
/* the identity matrix */
#define g_MatIdentity (GAME_ARRAY(const matrix_t, g_MatIdentity))
/* the triangles and vertices drawn in the frame (statistics) */
#define g_RenderTris GAME_GLOBAL_IN(int, g_RenderCounts, 0)
#define g_RenderVerts GAME_GLOBAL_IN(int, g_RenderCounts, 4)
/* nothing is drawn */
#define g_RenderOff GAME_GLOBAL_IN(byte, group_4a8ef8, 0)
/* only the meshes whose material has b38 are drawn */
#define g_RenderOnlyB38 GAME_GLOBAL_IN(byte, group_4a8ef8, 1)
/* the meshes are drawn from their vertex buffers */
#define g_UseVB GAME_GLOBAL_IN(byte, group_431bac, 0)
extern int g_RenderPass;
/* the D3DRS_ZENABLE value of the frame and whether the fog is on */
extern uint32_t g_ZEnable;
extern int g_FogActive;
/* the D3DRS_AMBIENT of the scenes */
extern uint32_t g_AmbientD3D;
/* the viewport: its size, and all of it (x, y, w, h, minZ, maxZ) */
#define g_ViewportW GAME_GLOBAL_IN(int, g_Viewport, 8)
#define g_ViewportH GAME_GLOBAL_IN(int, g_Viewport, 12)
extern uint32_t g_Viewport[6];
/* the planes of the view frustum (normal, d; outside where the distance is positive) */
#define MAX_PLANES_FRUSTUM 6
#define g_Frustum (GAME_ARRAY(const plane_t, g_Frustum))
/* the texture filters of the stages (Render_ResetState from the variables below): min, mag, mip, anisotropy */
extern uint32_t g_FilterMin;
extern uint32_t g_FilterMag;
extern uint32_t g_FilterMip;
extern uint32_t g_AnisotropyCur;
extern cGVar GVar_R_TexFilter;
#define g_TexFilter (GVar_R_TexFilter.value.i)		/* R_TexFilter 0 point, 1 linear, 2 anisotropic */
extern cGVar GVar_R_MipFilter;
#define g_MipFilter (GVar_R_MipFilter.value.i)		/* R_MipFilter 0 point, 1 linear */
#define g_Anisotropy GAME_GLOBAL_IN(uint32_t, GVar_R_Anisotropy, 16)	/* R_Anisotropy */
extern cGVar GVar_R_MipmapBias;
#define g_MipmapBias (GVar_R_MipmapBias.value.f)		/* R_MipmapBias */
extern float g_MipmapBias2;
#define g_MipmapsOn GAME_GLOBAL_IN(byte, group_4ad6f0, 2)
/* the address modes of the stages as Render_SetMap last set them */
extern byte g_StageMip[8];	/* the b8 (mipmaps) of the map */
extern byte g_StageTile[8];	/* the bc of the map: bit 0 wraps u, bit 1 v */
/* the texture of each stage and the frame it was set in */
extern void * g_StageTex[8];	/* [8] */
extern int g_StageTexFrame[8];	/* [8] */
#define g_TexChanges GAME_GLOBAL_IN(int, g_RenderCounts, 8)
#define g_VBIBTriLimit GAME_GLOBAL_IN(uint32_t, GVar_R_VBIBTriLimit, 16)	/* R_VBIBTriLimit */
/* the projection split into g_Tiles x g_Tiles parts (screenshots larger than the screen), one per frame */
extern int g_Tiles;
extern int g_TileState;
extern int g_TileX;
extern int g_TileY;
#define g_TileOn GAME_GLOBAL_IN(byte, group_4a8ef8, 3)
/* the D3DTOP of the lightmap stage */
extern uint32_t g_LightmapOp;
/* Render_SetMaterial: the last material, pass, frame and context (it is skipped for the same ones), the shader mode */
extern int g_MatPass;
extern int g_MatFrame;
extern CMaterial *g_MatCtx;
extern uint32_t g_ShaderCurrent;
#define g_MatChanges GAME_GLOBAL_IN(int, g_RenderCounts, 12)
/* RenderLightmapMat, the lightmap material of the draw (counted); with one the shader modes have 0x100 */
extern CMaterial *g_LightmapMat;
#define g_UseShaders GAME_GLOBAL_IN(byte, group_43240c, 1)
extern cGVar GVar_R_MaterialLog;
#define g_MaterialLog (GVar_R_MaterialLog.value.i)		/* R_MaterialLog */
#define g_DecalZBias GAME_GLOBAL_IN(uint32_t, GVar_R_DecalZBias, 16)	/* R_DecalZBias */
extern cGVar GVar_R_AlphaFog;
#define g_AlphaFog (GVar_R_AlphaFog.value.i)		/* R_AlphaFog 0, 1 (only add blending), 2 */
extern cGVar GVar_R_UseDefMaterial;
#define g_UseDefMaterial (GVar_R_UseDefMaterial.value.i)	/* R_UseDefMaterial 0, 1, 2 */
extern cGVar GVar_R_UseDefTexture;
#define g_UseDefTexture (GVar_R_UseDefTexture.value.i)	/* R_UseDefTexture 0..4 */
/* the default materials RenderIniD3D makes, in this order: white, checkerboard, grey, invisible, bump noise, ground */
extern CMaterial *g_MatWhite;
extern CMaterial *g_MatCheckerboard;
extern CMaterial *g_MatGray;
extern CMaterial *g_MatInvis;
extern CMaterial *g_MatBump;
extern CMaterial *g_MatGround;
/* the matrices of the frame: projection, view, the camera turn of the sprites, the world of the last draw; the eye */
extern matrix_t g_MatProj;
extern matrix_t g_MatView;
extern matrix_t g_MatBillboard;
extern matrix_t g_MatWorld;
extern vec3_t g_EyePos;
/* the material of a ModelFX in place of the ones of the meshes, NULL */
extern const CMaterial *g_MatForced;
/* the other overrides of ModelFX.cpp, cleared with it */
extern CMaterial *g_MatFX1;	/* in place of every material */
extern CMaterial *g_MatFX2;	/* in place of the lightmap material */
extern const CMaterial *g_MatFX3;	/* its colors for the D3D material */
/* bUsarHWEnvMap: the environment maps through the texture transform of the device (materials with two texture sets
 * get VERT_TEX1 instead of VERT_TEX2) */
extern int g_UseEnvMap;		/* video UsarEnvMap */
extern int g_UseHWEnvMap;
extern int g_UseBumpMap;		/* video UsarBumpMap */
extern int g_UseGlowMap;		/* video UsarGlowMap */
/* the engine variables of the environment and bump maps (Gvars) */
extern cGVar GVar_R_EnvBumpBias;
#define g_EnvBumpBias (GVar_R_EnvBumpBias.value.i)		/* R_EnvBumpBias, DXT1 bump textures */
extern cGVar GVar_R_EnvBumpScale;
#define g_EnvBumpScale (GVar_R_EnvBumpScale.value.f)	/* R_EnvBumpScale */
extern cGVar GVar_R_EnvMapOffset;
#define g_EnvMapOffset (GVar_R_EnvMapOffset.value.f)	/* R_EnvMapOffset */
extern cGVar GVar_R_EnvMapScale;
#define g_EnvMapScale (GVar_R_EnvMapScale.value.f)	/* R_EnvMapScale */
extern cGVar GVar_R_EnvMapViewDep;
#define g_EnvMapViewDep (GVar_R_EnvMapViewDep.value.i)	/* R_EnvMapViewDep 0, 1, 2 */
#define g_EnvBump GAME_GLOBAL_IN(int, g_GvEnvBump, 16)		/* R_EnvBump */
#define g_EnvMap GAME_GLOBAL_IN(int, g_GvEnvMap, 16)		/* R_EnvMap */
extern cGVar GVar_R_EnvBlend;
#define g_EnvBlend (GVar_R_EnvBlend.value.i)		/* R_EnvBlend */
/* the engine variables of the Clouds and GlowFlick shader modes (R_CloudR and so on) */
extern cGVar GVar_R_CloudR;
#define g_CloudR (GVar_R_CloudR.value.f)
extern cGVar GVar_R_CloudG;
#define g_CloudG (GVar_R_CloudG.value.f)
extern cGVar GVar_R_CloudB;
#define g_CloudB (GVar_R_CloudB.value.f)
extern cGVar GVar_R_CloudA;
#define g_CloudA (GVar_R_CloudA.value.f)
extern cGVar GVar_R_CloudEmi;
#define g_CloudEmi (GVar_R_CloudEmi.value.f)
extern cGVar GVar_R_CloudVel1x;
#define g_CloudVel1x (GVar_R_CloudVel1x.value.f)
extern cGVar GVar_R_CloudVel1y;
#define g_CloudVel1y (GVar_R_CloudVel1y.value.f)
extern cGVar GVar_R_CloudScale1;
#define g_CloudScale1 (GVar_R_CloudScale1.value.f)
extern cGVar GVar_R_CloudVel2x;
#define g_CloudVel2x (GVar_R_CloudVel2x.value.f)
extern cGVar GVar_R_CloudVel2y;
#define g_CloudVel2y (GVar_R_CloudVel2y.value.f)
extern cGVar GVar_R_CloudScale2;
#define g_CloudScale2 (GVar_R_CloudScale2.value.f)
extern cGVar GVar_R_GlowFlickTile;
#define g_GlowFlickTile (GVar_R_GlowFlickTile.value.f)
extern cGVar GVar_R_GlowFlickRot;
#define g_GlowFlickRot (GVar_R_GlowFlickRot.value.f)
extern cGVar GVar_R_GlowFlickBump;
#define g_GlowFlickBump (GVar_R_GlowFlickBump.value.f)
extern cGVar GVar_R_GlowFlickBRot;
#define g_GlowFlickBRot (GVar_R_GlowFlickBRot.value.f)
extern cGVar GVar_R_GlowFlickMod;
#define g_GlowFlickMod (GVar_R_GlowFlickMod.value.f)
/* the render functions of the shader modes (two tables, the second for the modes with 0x100) */
#define MAX_SHADER_TAB 32
typedef void (*shaderRender_t)(void *a, void *b);
typedef struct ShaderTab {
	uint32_t Index;			/* 0x00 its own index */
	const char *Name;		/* 0x04 */
	const char *File;		/* 0x08 the .psh, NULL without a pixel shader */
	int unused_0c;			/* 0x0c */
	uint32_t flags;			/* 0x10 0x100, 0x101 */
	shaderRender_t fnRender;	/* 0x14 */
} ShaderTab;
SD_SIZE(ShaderTab, 0x18);
#define g_ShaderTab (GAME_ARRAY(const ShaderTab, g_ShaderTab))	/* [32], ends with a NULL name */
#define g_ShaderTab2 (GAME_ARRAY(const ShaderTab, g_ShaderTab2))
/* the pixel shaders: on (the device has them), their handles, the ones switched off (a bit each from 0x200000) */
#define MAX_PS 9
extern cGVar GVar_R_PShaders;
#define g_PSOn (GVar_R_PShaders.value.i)
#define PS_POST 21				/* the first of the post effect shaders */
#define g_PS (g_PSHandles + PS_POST)
/* the pixel shaders of g_PSNames the shader modes use */
enum {
	PS_GLOWMAPMASKENVBUMP = 2,
	PS_MASKENVBUMP = 3,
	PS_MASKENVMAP = 4,
	PS_GLOWMAPMASKENVMAP = 5,
	PS_GLOWMAPMASKENVMAP_LM = 6,
	PS_MASKENVBUMP_LM = 7,
	PS_MASKENVMAP_LM = 8,
	PS_CLOUDS = 9,
	PS_ENVMAP = 10,
	PS_ENVMAP_LM = 11,
	PS_GLOWMAP = 12,
	PS_GLOWMAP_LM = 13,
	PS_HOLOGRAM = 14,
	PS_GLOWFLICK_LM = 15,
	PS_ELECTRIC = 16,
	PS_FIRE = 17,
	PS_GLASS = 18,
	PS_WAVES = 19,
	PS_NOISE = 20,
};
#define g_PSOff GAME_GLOBAL_IN(uint32_t, GVar_R_PSMask, 16)
/* all pixel shaders made from their files (the names end with NULL) */
#define g_PSNames (GAME_ARRAY(const char *const, g_PSNames))
extern uint32_t g_PSHandles[32];	/* [32], one per slot of g_PSNames */
extern cGVar GVar_R_PShaderCount;
#define g_PShaderCount (GVar_R_PShaderCount.value.i)	/* R_PShaderCount, the changes of the frame */
extern cGVar GVar_R_CullMode;
#define g_CullMode (GVar_R_CullMode.value.i)	/* R_CullMode, 1 two-sided */
/* the world matrix was given to the device */
extern int g_WorldSet;
/* the whole viewport (g_Viewport is the one of the current view) */
extern uint32_t g_ViewportFull[6];
#define g_D3DOn GAME_GLOBAL_IN(byte, group_4ad6f0, 1)
#define g_MatDefault ((void *)GAME_PTR(CMaterial_MatDefault, 0))
#define g_MatLine ((void *)GAME_PTR(CMaterial_MatLine, 0))	/* the material of the debug lines */
extern cGVar GVar_R_ShowSphereVis;
#define g_ShowSphereVis (GVar_R_ShowSphereVis.value.i)	/* R_ShowSphereVis 0, 1, 2 */
extern char g_VersionStr[64];	/* 0x40 */
extern const CMaterial *g_MatCurrent;
/* what CNode3D_Render draws: the object classes (low 24 bits of the mask), the nodes whose flags & g_RenderFlagMask
 * is not 0 when g_RenderFlagNeg */
extern uint32_t g_RenderClassMask;
extern uint32_t g_RenderFlagMask;
extern byte g_RenderFlagNeg;	/* the last one Render_SetMaterial set */
/* the D3DADAPTER_IDENTIFIER8 of the device, its texts "driver [description]  version" and the ids, and the device
 * of the last log of them */
extern D3DAdapterId g_Adapter;
extern char g_DriverStr[256];	/* 0x100 */
extern char g_DeviceStr[256];	/* 0x100 */
#define g_DriverName (GAME_ARRAY_IN(cString, GVar_R_LastDevice, 16))
extern int g_DeviceLogged;
/* the identity scaled by 1/2048 */
extern matrix_t g_MatBias;
#define g_TexNoPow2 GAME_GLOBAL_IN(byte, group_4ad738, 0)	/* D3DPTEXTURECAPS_NONPOW2CONDITIONAL */
#define g_TexPow2 GAME_GLOBAL_IN(byte, group_4ad738, 1)	/* D3DPTEXTURECAPS_POW2 */
#define g_TexRepeat GAME_GLOBAL_IN(byte, group_4ad6f0, 3)	/* D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE */
#define g_UseShaders2 GAME_GLOBAL_IN(byte, group_43240c, 0)
extern int g_RenderI1F4;

/* a dynamic index buffer of triangles (16 bit indices) */
typedef struct CRenderIB {
	void *pVertIB;			/* 0x0 IDirect3DIndexBuffer8 */
	int Total;			/* 0x4 triangles */
	int Used;			/* 0x8 */
} CRenderIB;
SD_SIZE(CRenderIB, 0xc);

/* a dynamic vertex buffer */
typedef struct CRenderVB {
	void *pVertVB;			/* 0x00 IDirect3DVertexBuffer8 */
	int Total;			/* 0x04 vertices */
	int Used;			/* 0x08 */
	int Size;			/* 0x0c of a vertex */
	uint32_t FVF;			/* 0x10 */
} CRenderVB;
SD_SIZE(CRenderVB, 0x14);

/* pixel shader i on (flags 1) or off (flags 2), the fixed texture stages (Render_PSFixed) without it; arg is not
 * used */
void Render_PS0(void *arg, byte flags);
void Render_PS1(void *arg, byte flags);
void Render_PS2(void *arg, byte flags);
void Render_PS3(void *arg, byte flags);
void Render_PS4(void *arg, byte flags);
void Render_PS5(void *arg, byte flags);
void Render_PS6(void *arg, byte flags);
void Render_PS7(void *arg, byte flags);
void Render_PS8(void *arg, byte flags);
/* stage 0 texture times diffuse (four times with DiffuseMod4x when on) */
void Render_PSFixed(void *arg, byte flags);
/* the render functions of the shader modes, g_ShaderTab and the lightmap ones of g_ShaderTab2 (flags: 1 begin, 4
 * the next material of the mode, 2 end) */
void Shader_DiffuseNoLit(CMaterial *mat, uint32_t flags);
void Shader_DiffuseLightmap(CMaterial *mat, uint32_t flags);
void Shader_Envmap(CMaterial *mat, uint32_t flags);
void Shader_EnvmapLightmap(CMaterial *mat, uint32_t flags);
void Shader_MaskEnvmap(CMaterial *mat, uint32_t flags);
void Shader_MaskEnvmapLightmap(CMaterial *mat, uint32_t flags);
void Shader_MaskEnvBump(CMaterial *mat, uint32_t flags);
void Shader_MaskEnvBumpLightmap(CMaterial *mat, uint32_t flags);
void Shader_Glowmap(CMaterial *mat, uint32_t flags);		/* also GlowFlick */
void Shader_GlowmapLightmap(CMaterial *mat, uint32_t flags);
void Shader_GlowmapMaskEnvmap(CMaterial *mat, uint32_t flags);
void Shader_GlowmapMaskEnvmapLightmap(CMaterial *mat, uint32_t flags);
void Shader_GlowmapMaskEnvBump(CMaterial *mat, uint32_t flags);
void Shader_Glass(CMaterial *mat, uint32_t flags);
void Shader_Scroll(CMaterial *mat, uint32_t flags);
void Shader_ScrollLightmap(CMaterial *mat, uint32_t flags);
void Shader_Clouds(CMaterial *mat, uint32_t flags);
void Shader_Hologram(CMaterial *mat, uint32_t flags);
void Shader_GlowFlickLightmap(CMaterial *mat, uint32_t flags);
void Shader_Electric(CMaterial *mat, uint32_t flags);
void Shader_Fire(CMaterial *mat, uint32_t flags);
void Shader_Waves(CMaterial *mat, uint32_t flags);
void Shader_Noise(CMaterial *mat, uint32_t flags);
/* the segment ab against the frustum: 0 seen, 1..6 both ends outside that plane, 10 not seen */
int Frustum_TestSeg(const vec3_t *a, const vec3_t *b);
/* the first plane (1..6) p is outside of, the planes in skip not tried; 0 inside */
int Frustum_PlaneOutside(const vec3_t *p, uint32_t skip);
/* for n spheres the first plane each one is outside of (as Frustum_PlaneOutside), 0 seen */
void Frustum_TestSpheres(const vec3_t *centers, const float *radius, int n, int *out);
void CRenderIB_Reset(CRenderIB *self);
void CRenderVB_Reset(CRenderVB *self);
void CRenderIB_Add(CRenderIB *self, int numtri, const uint16_t *idx);
void CRenderVB_Add(CRenderVB *self, int numvert, const void *verts);
/* the buffer (AddRef) and the used count */
void *CRenderIB_Get(CRenderIB *self, int *used);
void *CRenderVB_Get(CRenderVB *self, int *used);
void CRenderIB_Create(CRenderIB *self, uint32_t numtri);
void CRenderVB_Create(CRenderVB *self, uint32_t numvert, int size, uint32_t fvf);
/* the batches drawn first; rect (x0, y0, x1, y1 in the viewport) NULL for all; D3DCLEAR_ZBUFFER also clears the
 * stencil of a device that has one; whether the device cleared */
byte Render_Clear(const box_t *rect, uint32_t flags, uint32_t color, float z, uint32_t stencil);
void Render_GetViewportSize(int *width, int *height);
/* a textured rectangle of the viewport, its left side in cLeft, its right side in cRight */
void Render_DrawTexRect(const box_t *rect, uint32_t cLeft, uint32_t cRight, void *tex);
/* the end of the D3D device: textures and pixel shaders off, the shaders deleted, the device objects of the modules
 * released */
void Render_EndD3D(void);
/* the end of a view: the batches drawn, the default material, the whole viewport */
void Render_EndView(void);
/* the batches drawn, the materials of ModelFX cleared */
void Render_ClearForced(void);
const char *Render_GetDriver(void);
/* the start of the D3D device: caps, the log of the device, base stage states, the pixel shaders from their
 * files, the device objects of the modules, the default materials */
void Render_IniD3D(void);
/* the start of a frame: the device of the application, the whole viewport, the base states */
void Render_BeginFrame(void);
/* the triangles of idx over the vertices of v (counted with stats) */
int Render_DrawListIdx(const struct CVertList *v, const uint16_t *idx, int ntri, byte stats);
int Render_SetWorldD3D(const matrix_t *m);
void Render_WorldDirty(void);
/* the world transform of the device (with defer only remembered); 0 */
int Render_SetWorldMatrix(const matrix_t *m, byte defer);
/* the world matrix given to the device once (HRESULT) */
int Render_SetWorld(void);
/* the base texture stage states and the filters of the variables */
void Render_ResetState(void);
/* the device states and texture of a material for pass 0 (the material) or 1 (its second texture set added); NULL
 * with pass -1 forgets the last material; HRESULT */
int Render_SetMaterial(void *mat, int pass);
/* the projection of the tile of this frame (state 2), the next tile after one was drawn; 0 without tiles */
byte Render_ProjTile(const matrix_t *proj);
int Render_SetProj(const matrix_t *m);
/* D3DCULL_CCW is none with R_CullMode */
void Render_SetCull(uint32_t mode);
/* the lightmap material (counted) and its modulation (2 2x, 4 4x) */
void Render_SetLightmapMat(CMaterial *mat, int mode);
/* the texture of map (NULL none) on stage: its frame, mipmaps, wrap and anisotropy */
struct CMap;
int Render_SetMap(uint32_t stage, struct CMap *map);
/* ntri triangles from startTri of ib over numVerts vertices of vb in format frmt from baseVert */
int Render_DrawVBIBFmt(void *vb, void *ib, int frmt, uint32_t numVerts, uint32_t ntri, uint32_t baseVert,
	uint32_t startTri);
struct CMeshD3D;
int Render_DrawMeshD3D(const struct CMeshD3D *m);
int Render_DrawVBIB(void *vb, void *ib, const struct CVertList *v, uint32_t ntri);
/* the view matrix, its inverse as the billboard matrix and the eye */
int Render_SetView(const matrix_t *m);
/* a mesh with its material (and the second pass of its second texture set), the skinned and stencil shadow ones
 * through their modules; r is the render node, NULL */
struct CRenderNode;
int Render_MeshD3D(struct CMeshD3D *m, struct CRenderNode *r);
int Render_GetWorld(matrix_t *out);
/* the alpha constant of the pixel shaders in reg: 1 1 1 0, 0 for transparent materials (not MAT_1000) and with a
 * forced material */
void Render_PSConstAlpha(const CMaterial *mat, uint32_t reg);
/* the texture stages of the reflection map (map 2) and the bump map before it (map 3, its scale and turn) */
void Mat_StageEnvBump(CMaterial *mat, uint32_t flags, uint32_t stageEnv, uint32_t stageBump);
/* the camera space reflection coordinates of map 2 scaled by R_EnvMapScale (R_EnvMapViewDep) */
void Mat_StageEnvMap(CMaterial *mat, uint32_t flags, uint32_t stage, float bias);
/* the render function of shader mode mode */
void Render_Shader(uint32_t mode, void *a, void *b);
/* texture coordinates of stage moved by a random part of +-ju, +-jv and scaled */
void Tex_StageJitter(void *unused, uint32_t flags, uint32_t stage, float ju, float jv, float scale);
/* the scroll (f10) and turn (v20[2] * k) of map mapid */
void Mat_StageScroll(CMaterial *mat, uint32_t mapid, uint32_t flags, uint32_t stage, float k);
/* texture coordinates of stage scrolled by su, sv per second, scaled and turned by rot per second */
void Tex_StageTransform(void *unused, uint32_t flags, uint32_t stage, float su, float sv, float scale, float rot);
/* the bump env matrix of stage: scale s (plus the f2c of map 3 times k) turned by rot */
void Mat_StageBumpMat(CMaterial *mat, uint32_t flags, uint32_t stage, float k, float s, float rot);
/* the D3DMATERIAL8 of colors (diffuse, ambient, specular, emissive, power): with DiffuseMod4x the colors but the
 * specular a fourth, with a forced material its alpha */
void Render_D3DMaterial(const CMaterial *mat, const float *colors);

const matrix_t *Render_GetProj(void);
const matrix_t *Render_GetView(void);
const matrix_t *Render_GetBillboard(void);
const vec3_t *Render_GetEyePos(void);

/* a line in the current world matrix (the line material); an error without a device */
int Render_Line(const vec3_t *a, const vec3_t *b, uint32_t color);
/* a line in pixels of the viewport (pretransformed, the line material); an error without a device */
int Render_Line2D(float x0, float y0, float x1, float y1, uint32_t color);
/* the viewport size, CHECK that it is set */
int Render_GetViewportW(void);
int Render_GetViewportH(void);
/* the pixels of rect (0..1 of the viewport vp) as a D3DRECT, clamped; 0 (a point) when nothing is left */
byte Render_ClipRect(int *out, const uint32_t *vp, const box_t *rect);
/* the stencil bits of the depth buffer */
int Render_StencilBits(void);
/* the vertex buffer of a vertex list and the index buffer of triangles, in system memory with dynamic (HRESULT) */
int Render_CreateVB(void **vb, struct CVertList *vert, byte dynamic);
int Render_CreateIB(void **ib, const void *tris, int nTri, byte dynamic);
/* the batches waiting for the view drawn */
void E3D_FlushBatch(void);
/* the box in the space of m is in the view frustum (as the sphere around it, scale times its radius); with
 * R_ShowSphereVis drawn */
byte Box_IsVisible(const matrix_t *m, const box_t *box, const char *name, float scale);
byte Frustum_SegVisible(const vec3_t *a, const vec3_t *b);
/* arg is not used */
byte Sph_IsVisible(const vec3_t *center, float radius, int arg);
/* "major.minor" of a version word, a static buffer */
const char *Render_VersionStr(uint32_t v);
/* the view and projection of the camera, the frustum of them */
struct CCamera;
void Render_SetCamera(struct CCamera *cam);
void Render_NoIndexedBlend(void *dev);
/* the viewport narrowed to rect and the projection to it (with proj); NULL the whole view again; 1 */
byte Render_SetViewRect(const box_t *rect, byte proj);
/* the screen position (pixels) of a world position; 0, -1 when it is not on the screen */
int Render_ToScreen(const vec3_t *pos, float *xy);
/* the planes of the frustum of view and proj (UpdateVisibilityPlanes) */
void Frustum_Calc(const matrix_t *view, const matrix_t *proj);

/* Engine/RenderEVA.cpp (src/game/engine3d/Engine/RenderEVA.c): the meshes with a vertex animation (EVA) or a skin,
 * moved in software into a dynamic vertex buffer of 0x1000 vertices (VERT_TEX1), their shadow positions into the
 * main mesh of the node; and the particles, quads in a dynamic vertex buffer of 0x800 vertices drawn through a fixed
 * index buffer of 0x200 quads */
#define MAX_VERT_VB 0x1000
#define VB_PARTICLES 0x200
extern void *g_EVAVB;		/* IDirect3DVertexBuffer8, D3DFVF 0x112 */
extern int g_EVAVBUsed;
extern void *g_PartVB;		/* D3DFVF 0x1c2 (Graphic3DVert) */
extern void *g_PartIB;
extern int g_PartVBUsed;
extern uint16_t *g_PartTri;	/* pTriParticles, new[] */
extern int g_PartNumTri;
/* the particles drawn in the frame (statistics) */
#define g_RenderParts GAME_GLOBAL_IN(int, g_RenderCounts, 24)
/* the engine variables */
extern cGVar GVar_R_MaxSparkSize;
#define g_MaxSparkSize (GVar_R_MaxSparkSize.value.f)	/* R_MaxSparkSize */
extern cGVar GVar_R_MinSparkDist;
#define g_MinSparkDist (GVar_R_MinSparkDist.value.f)	/* R_MinSparkDist */
extern cGVar GVar_R_ParticlesAlphaDist;
#define g_ParticlesAlphaDist (GVar_R_ParticlesAlphaDist.value.i)	/* R_ParticlesAlphaDist */
extern cGVar GVar_R_ParticlesDraw;
#define g_ParticlesDraw (GVar_R_ParticlesDraw.value.i)	/* R_ParticlesDraw */
extern cGVar GVar_R_ParticlesLock;
#define g_ParticlesLock (GVar_R_ParticlesLock.value.i)	/* R_ParticlesLock: 1 always discard, 2 the size too */
extern cGVar GVar_R_ParticlesOne;
#define g_ParticlesOne (GVar_R_ParticlesOne.value.i)		/* R_ParticlesOne */
extern cGVar GVar_R_ParticlesZBias;
#define g_ParticlesZBias (GVar_R_ParticlesZBias.value.i)	/* R_ParticlesZBias */
extern cGVar GVar_R_ShowEVA;
#define g_ShowEVA (GVar_R_ShowEVA.value.i)		/* R_ShowEVA */
extern cGVar GVar_R_ShowSkin;
#define g_ShowSkin (GVar_R_ShowSkin.value.i)		/* R_ShowSkin */
extern cGVar GVar_R_SkinNodeSpace;
#define g_SkinNodeSpace (GVar_R_SkinNodeSpace.value.i)	/* R_SkinNodeSpace */
extern cGVar GVar_R_UseEVA;
#define g_UseEVA (GVar_R_UseEVA.value.i)		/* R_UseEVA */
extern cGVar GVar_R_UseSkin;
#define g_UseSkin (GVar_R_UseSkin.value.i)		/* R_UseSkin */

struct CParticle;
struct Graphic3DVert;
/* both vertex buffers and the index buffer of the particles made (EVA_crear_VB); HRESULT */
int RenderEVA_InitD3D(void);
int RenderEVA_CreateParticles(void);
/* the buffers of the particles released; 0 */
int RenderEVA_FreeParticles(void);
int RenderEVA_ReleaseD3D(void);
/* nvert vertices of the particle (skin) buffer locked after the used ones, from the start (discarded) when they do
 * not fit; HRESULT */
int RenderEVA_LockParticles(int nvert, void **data);
int RenderEVA_LockSkin(int nvert, void **data);
/* the mesh with the vertex animation skin (a CEVA) at the blend of the animation slot of r; HRESULT */
int RenderEVA_DrawSkinNode(struct CMeshD3D *m, struct CRenderNode *r, void *skin);
/* the mesh with its skin over the render nodes of the model; HRESULT */
int RenderEVA_DrawSkin(struct CMeshD3D *m, struct CRenderNode *r);
/* the four corners of a particle: a square of its size turned by rot facing the camera (bill: the billboard
 * matrix in the space of the particles, view: the view matrix of it), sparks stretched along their screen motion;
 * alpha (below 1) scales the inverted alpha of the color, zbias moves it to the camera */
void Particle_Quad(struct CParticle *p, const matrix_t *bill, const matrix_t *view, struct Graphic3DVert *out,
	float alpha, float zbias);

/* Engine/RenderMallaD3D.cpp, RenderMallaD3D_transp.cpp and RenderObjBase3D.cpp
 * (src/game/engine3d/Engine/RenderMeshD3D.c): the batches of a view. Opaque meshes (R_MeshCache) are sorted by
 * texture, material and skin, transparent ones and the particle systems and flares far to near, each drawn with the
 * world matrix it was queued with. Up to 0x400 of each, a full batch is drawn at once. */
#define MAX_RENDER_BATCH 0x400
typedef struct RenderMeshEntry {
	struct CMeshD3D *mesh;	/* 0x00 */
	matrix_t world;			/* 0x04 */
	uint32_t tex;			/* 0x44 the diffuse texture >> 1, 0 without */
	int list;			/* 0x48 the batch count when the list of the mesh was queued */
	struct CRenderNode *r;		/* 0x4c */
} RenderMeshEntry;
SD_SIZE(RenderMeshEntry, 0x50);
typedef struct RenderTranspEntry {
	struct CMeshD3D *mesh;	/* 0x00 */
	matrix_t world;			/* 0x04 */
	int group;			/* 0x44 g_TranspGroup, 0 for the identity */
	struct CRenderNode *r;		/* 0x48 */
	float dist2;			/* 0x4c to the eye, squared */
} RenderTranspEntry;
SD_SIZE(RenderTranspEntry, 0x50);
typedef struct RenderObjEntry {
	struct IObject3D *obj;		/* 0x00 a CParticleSystem or a CFlare */
	matrix_t world;			/* 0x04 */
	struct CRenderNode *r;		/* 0x44 */
	float dist2;			/* 0x48 */
} RenderObjEntry;
SD_SIZE(RenderObjEntry, 0x4c);
extern CTable g_TabMesh;		/* TabMallaD3D: RenderMeshEntry */
extern RenderMeshEntry * g_MeshList[1026];
#define g_UsedMesh GAME_GLOBAL_IN(int, g_MeshList, 4096)
extern CTable g_TabTransp;		/* RenderTranspEntry */
extern RenderTranspEntry * g_TranspList[1026];
#define g_UsedTransp GAME_GLOBAL_IN(int, g_TranspList, 4096)
extern int g_TranspGroup;		/* counted up for each mesh list, 1 after a flush */
extern CTable g_TabObj;		/* TabObjBase3D: RenderObjEntry */
extern RenderObjEntry * g_ObjList[1026];
#define g_UsedObj GAME_GLOBAL_IN(int, g_ObjList, 4096)
extern cGVar GVar_R_MeshCache;
#define g_MeshCache (GVar_R_MeshCache.value.i)		/* R_MeshCache, 10 logs the batches */
extern cGVar GVar_R_ShowTranspBox;
#define g_ShowTranspBox (GVar_R_ShowTranspBox.value.i)	/* R_ShowTranspBox */
extern cGVar GVar_R_TranspZBias;
#define g_TranspZBias (GVar_R_TranspZBias.value.f)	/* R_TranspZBias */

/* the batches drawn; E_FAIL without a device, else 0 */
int RenderMeshD3D_Flush(void);
int RenderMeshD3D_Flush2(void);
int RenderObjBase3D_Flush(void);
/* a transparent mesh queued (cont: the same list as the one before, else a new group); HRESULT */
int RenderMeshD3D_AddTransp(struct CMeshD3D *m, const matrix_t *world, byte cont, struct CRenderNode *r);
/* the qsort orders: texture, material, then the skinned ones last; the transparent ones and the objects far first */
int RenderMesh_Compare(const void *a, const void *b);
int RenderTransp_Compare(const void *a, const void *b);
int RenderObj_Compare(const void *a, const void *b);

/* Engine/RenderLock.cpp (src/game/engine3d/Engine/RenderLock.c) */
void RenderLock_ReleaseD3D(void);
void RenderLock_InitD3D(void);
/* the lock of the frame of the mode RenderLock */
void RenderLock_Frame(void);
void RenderLock_CopyBack(void);
void RenderLock_DrawMem(void);
void RenderLock_Clear(void);

/* Engine/RenderGamma.cpp (src/game/engine3d/Engine/RenderGamma.c): the gamma ramp of the device from the engine
 * variables, set again when one changed */
void RenderGamma_ReleaseD3D(void);
void RenderGamma_Begin(void);
/* Engine/RenderShader.cpp: the shader modes of the materials */
void RenderGamma_LogShaderMask(void);		/* the mask of the shader features on, logged when it changes */
int Shader_Find(const char *name);		/* the index in g_ShaderTab, 0 and an error when not found */

/* still the exe's */


/* other modules (src/imports.inc) */
/* rva 0x267530 (stencil.cpp): the shadow volume of the triangles of a mesh */
void Stencil_Mesh(struct CVertList *v, const void *tris, uint32_t nTri, struct CMeshD3D *m, struct CRenderNode *r);
/* CVertList_EnvMapUV: vertlist.h */
void VertList_InitD3D(void);			/* rva 0x2404f0, empty */
void RenderTarget_InitD3D(void);
void VertList_ReleaseD3D(void);		/* rva 0x2404e0 (ListaVert.cpp), empty */
void RenderTarget_ReleaseD3D(void);		/* rva 0x220c60 (RenderTarget.cpp) */

#endif
