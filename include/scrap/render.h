/*
 * Engine/render.cpp of the engine3d: the renderer over the D3D 8 device (g_pd3dDevice): the frame state (matrices,
 * viewport, frustum, render pass), the material and world matrix of each draw, pixel shaders, dynamic vertex and
 * index buffers, clears and the debug primitives. Decompiled in parts (src/game/engine3d/Engine/Render.c); the rest is
 * still the exe's, the rva of each of those is given.
 */
#ifndef SCRAP_RENDER_H
#define SCRAP_RENDER_H

#include "scrap/mathlib.h"

struct CMaterial;
typedef struct CMaterial CMaterial;
struct CListaVert;

/* the D3D device of the engine3d, NULL without one */
#define g_pd3dDevice GAME_DATA(void *, 0x4ad6f4)
/* the identity matrix */
#define g_MatIdentity ((const matrix_t *)Game_Ptr(0x4ad6f8))
/* the triangles and vertices drawn in the frame (statistics) */
#define g_RenderTris GAME_DATA(int, 0x4a8f00)
#define g_RenderVerts GAME_DATA(int, 0x4a8f04)
/* nothing is drawn */
#define g_RenderOff GAME_DATA(byte, 0x4a8ef8)
#define g_RenderPass GAME_DATA(int, 0x4afdb8)
/* the D3DRS_ZENABLE value of the frame and whether the fog is on */
#define g_ZEnable GAME_DATA(uint32_t, 0x4ad740)
#define g_FogActive GAME_DATA(int, 0x4ad744)
/* the D3DRS_AMBIENT of the scenes */
#define g_AmbientD3D GAME_DATA(uint32_t, 0x4ad748)
/* the viewport: its size, and all of it (x, y, w, h, minZ, maxZ) */
#define g_ViewportW GAME_DATA(int, 0x4ad7e8)
#define g_ViewportH GAME_DATA(int, 0x4ad7ec)
#define g_Viewport ((uint32_t *)Game_Ptr(0x4ad7e0))
/* the planes of the view frustum (normal, d; outside where the distance is positive) */
#define MAX_PLANOS_FRUSTUM 6
#define g_Frustum ((const plane_t *)Game_Ptr(0x4adbf0))
/* the texture filters of the stages (Render_ResetState from the variables below): min, mag, mip, anisotropy */
#define g_FiltroMin GAME_DATA(uint32_t, 0x431d7c)
#define g_FiltroMag GAME_DATA(uint32_t, 0x431d80)
#define g_FiltroMip GAME_DATA(uint32_t, 0x431d70)
#define g_Anisotropia GAME_DATA(uint32_t, 0x431d78)
#define g_TexFilter GAME_DATA(int, 0x4add58)		/* R_TexFilter 0 point, 1 linear, 2 anisotropic */
#define g_MipFilter GAME_DATA(int, 0x4ad8f8)		/* R_MipFilter 0 point, 1 linear */
#define g_Anisotropy GAME_DATA(uint32_t, 0x4adba0)	/* R_Anisotropy */
#define g_MipmapBias GAME_DATA(float, 0x4ada68)		/* R_MipmapBias */
#define g_MipmapBias2 GAME_DATA(float, 0x4ad73c)
#define g_MipmapsOn GAME_DATA(byte, 0x4ad6f2)
#define MOTOR_MAXANISOTROPY(m) (*(uint32_t *)((m) + 0x2a448))
/* the address modes of the stages as Render_SetMapa last set them */
#define g_StageMip ((byte *)Game_Ptr(0x4addb4))	/* the b8 (mipmaps) of the map */
#define g_StageTile ((byte *)Game_Ptr(0x4addbc))	/* the bc of the map: bit 0 wraps u, bit 1 v */
/* the texture of each stage and the frame it was set in */
#define g_StageTex ((void **)Game_Ptr(0x4ae594))
#define g_StageTexFrame ((int *)Game_Ptr(0x4ae5b4))
#define g_TexChanges GAME_DATA(int, 0x4a8f08)
#define g_VBIBTriLimit GAME_DATA(uint32_t, 0x4ad820)	/* R_VBIBTriLimit */
/* the projection split into g_Tiles x g_Tiles parts (screenshots larger than the screen), one per frame */
#define g_Tiles GAME_DATA(int, 0x432414)
#define g_TileState GAME_DATA(int, 0x4ae5d4)
#define g_TileX GAME_DATA(int, 0x4ae5d8)
#define g_TileY GAME_DATA(int, 0x4ae5dc)
#define g_TileOn GAME_DATA(byte, 0x4a8efb)
/* the D3DTOP of the lightmap stage */
#define g_LightmapOp GAME_DATA(uint32_t, 0x432408)
/* Render_SetMaterial: the last material, pass, frame and context (it is skipped for the same ones), the shader mode */
#define g_MatPass GAME_DATA(int, 0x4ae588)
#define g_MatFrame GAME_DATA(int, 0x4ae58c)
#define g_MatCtx GAME_DATA(CMaterial *, 0x4ae584)
#define g_ShaderActual GAME_DATA(uint32_t, 0x4ae590)
#define g_MatChanges GAME_DATA(int, 0x4a8f0c)
/* RenderLightmapMat, the lightmap material of the draw (counted); with one the shader modes have 0x100 */
#define g_LightmapMat GAME_DATA(CMaterial *, 0x4addc4)
#define g_UsarShaders GAME_DATA(byte, 0x43240d)
#define g_MaterialLog GAME_DATA(int, 0x4adbe8)		/* R_MaterialLog */
#define g_DecalZBias GAME_DATA(uint32_t, 0x4add88)	/* R_DecalZBias */
#define g_AlphaFog GAME_DATA(int, 0x4ada80)		/* R_AlphaFog 0, 1 (only add blending), 2 */
#define g_UseDefMaterial GAME_DATA(int, 0x4ad8e0)	/* R_UseDefMaterial 0, 1, 2 */
#define g_UseDefTexture GAME_DATA(int, 0x4ad868)	/* R_UseDefTexture 0..4 */
/* the default materials RenderIniD3D makes (white first) */
#define g_MatBlanco GAME_DATA(CMaterial *, 0x4ad6d8)
#define g_MatDef2 GAME_DATA(CMaterial *, 0x4ad6dc)
#define g_MatDef3 GAME_DATA(CMaterial *, 0x4ad6e0)
#define g_MatDef4 GAME_DATA(CMaterial *, 0x4ad6e4)
/* the matrices of the frame: projection, view, the camera turn of the sprites, the world of the last draw; the eye */
#define g_MatProj (*(matrix_t *)Game_Ptr(0x4ae3f8))
#define g_MatView (*(matrix_t *)Game_Ptr(0x4ae4b8))
#define g_MatBillboard (*(matrix_t *)Game_Ptr(0x4ae4f8))
#define g_MatWorld (*(matrix_t *)Game_Ptr(0x4ae478))
#define g_EyePos (*(vec3_t *)Game_Ptr(0x4adda8))
/* the material of a ModelFX in place of the ones of the meshes, NULL */
#define g_MatForzado GAME_DATA(const CMaterial *, 0x4ad754)
/* the other overrides of ModelFX.cpp, cleared with it */
#define g_MatFX1 GAME_DATA(CMaterial *, 0x4ad74c)	/* in place of every material */
#define g_MatFX2 GAME_DATA(CMaterial *, 0x4ad750)	/* in place of the lightmap material */
#define g_MatFX3 GAME_DATA(const CMaterial *, 0x4ad758)	/* its colors for the D3D material */
/* bUsarHWEnvMap: the environment maps through the texture transform of the device (materials with two texture sets
 * get VERT_TEX1 instead of VERT_TEX2) */
#define g_UsarHWEnvMap GAME_DATA(int, 0x431bdc)
/* the engine variables of the environment and bump maps (Gvars) */
#define g_EnvBumpBias GAME_DATA(int, 0x4adbb8)		/* R_EnvBumpBias, DXT1 bump textures */
#define g_EnvBumpScale GAME_DATA(float, 0x4ada08)	/* R_EnvBumpScale */
#define g_EnvMapOffset GAME_DATA(float, 0x4adac8)	/* R_EnvMapOffset */
#define g_EnvMapScale GAME_DATA(float, 0x4adb40)	/* R_EnvMapScale */
#define g_EnvMapViewDep GAME_DATA(int, 0x4adb58)	/* R_EnvMapViewDep 0, 1, 2 */
/* the render functions of the shader modes (two tables, the second for the modes with 0x100) */
#define MAX_SHADER_TAB 32
typedef void (*shaderRender_t)(void *a, void *b);
typedef struct ShaderTab {
	uint32_t Index;			/* 0x00 its own index */
	int i4[4];			/* 0x04 */
	shaderRender_t fnRender;	/* 0x14 */
} ShaderTab;
SD_SIZE(ShaderTab, 0x18);
#define g_ShaderTab ((const ShaderTab *)Game_Ptr(0x431e08))
#define g_ShaderTab2 ((const ShaderTab *)Game_Ptr(0x432108))
/* the pixel shaders: on (the device has them), their handles, the ones switched off (a bit each from 0x200000) */
#define MAX_PS 9
#define g_PSOn GAME_DATA(int, 0x4ada50)
#define g_PS (g_PSHandles + 21)
#define g_PSOff GAME_DATA(uint32_t, 0x4ada98)
/* all pixel shaders made from their files (the names end with NULL) */
#define g_PSNames ((const char *const *)Game_Ptr(0x431d88))
#define g_PSHandles ((uint32_t *)Game_Ptr(0x4ad760))
#define g_PShaderCount GAME_DATA(int, 0x4add70)	/* R_PShaderCount, the changes of the frame */
#define g_CullMode GAME_DATA(int, 0x4ad910)	/* R_CullMode, 1 two-sided */
/* the world matrix was given to the device */
#define g_WorldSet GAME_DATA(int, 0x432410)
/* the whole viewport (g_Viewport is the one of the current view) */
#define g_ViewportFull ((uint32_t *)Game_Ptr(0x4ad7f8))
#define g_D3DOn GAME_DATA(byte, 0x4ad6f1)
#define g_MatDefault ((void *)Game_Ptr(0x4adc98))
#define g_MatLinea ((void *)Game_Ptr(0x4ad918))	/* the material of the debug lines */
#define g_ShowSphereVis GAME_DATA(int, 0x4adbd0)	/* R_ShowSphereVis 0, 1, 2 */
#define g_VersionStr ((char *)Game_Ptr(0x4ae540))	/* 0x40 */
#define MOTOR_ZFORMAT(m) (*(uint32_t *)((m) + 0x2a4d0))
#define g_MatActual GAME_DATA(const CMaterial *, 0x4ae580)	/* the last one Render_SetMaterial set */
#define g_Caps ((void *)Game_Ptr(0x4ae1f8))	/* D3DCAPS8 of the device */
#define g_RenderI1F4 GAME_DATA(int, 0x4ae1f4)

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
/* the segment ab against the frustum: 0 seen, 1..6 both ends outside that plane, 10 not seen */
int Frustum_TestSeg(const vec3_t *a, const vec3_t *b);
/* the first plane (1..6) p is outside of, the planes in skip not tried; 0 inside */
int Frustum_PlaneOutside(const vec3_t *p, uint32_t skip);
/* for n spheres the first plane each one is outside of (as Frustum_PlaneOutside), 0 seen */
void Frustum_TestSpheres(const vec3_t *centers, const float *radius, int n, int *out);
void __fastcall CRenderIB_Reset(CRenderIB *self);
void __fastcall CRenderVB_Reset(CRenderVB *self);
void __thiscall CRenderIB_Add(CRenderIB *self, int numtri, const uint16_t *idx);
void __thiscall CRenderVB_Add(CRenderVB *self, int numvert, const void *verts);
/* the buffer (AddRef) and the used count */
void *__thiscall CRenderIB_Get(CRenderIB *self, int *used);
void *__thiscall CRenderVB_Get(CRenderVB *self, int *used);
void __thiscall CRenderIB_Create(CRenderIB *self, uint32_t numtri);
void __thiscall CRenderVB_Create(CRenderVB *self, uint32_t numvert, int size, uint32_t fvf);
/* the batches drawn first; rect (x0, y0, x1, y1 in the viewport) NULL for all; D3DCLEAR_ZBUFFER also clears the
 * stencil of a device that has one; whether the device cleared */
byte Render_Clear(const box_t *rect, uint32_t flags, uint32_t color, float z, uint32_t stencil);
void Render_GetViewportSize(int *ancho, int *alto);
/* a textured rectangle of the viewport, its left side in cLeft, its right side in cRight */
void Render_DrawTexRect(const box_t *rect, uint32_t cLeft, uint32_t cRight, void *tex);
/* the end of the D3D device: textures and pixel shaders off, the shaders deleted, the device objects of the modules
 * released */
void Render_FinD3D(void);
/* the end of a view: the batches drawn, the default material, the whole viewport */
void Render_EndView(void);
/* the batches drawn, the materials of ModelFX cleared */
void Render_ClearForced(void);
const void *Render_GetCaps(void);
/* the start of a frame: the device of the application, the whole viewport, the base states */
void Render_BeginFrame(void);
/* the triangles of idx over the vertices of v (counted with stats) */
int Render_DrawListaIdx(const struct CListaVert *v, const uint16_t *idx, int ntri, byte stats);
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
/* the texture of mapa (NULL none) on stage: its frame, mipmaps, wrap and anisotropy */
struct CMapa;
int Render_SetMapa(uint32_t stage, struct CMapa *mapa);
/* ntri triangles from startTri of ib over numVerts vertices of vb in format frmt from baseVert */
int Render_DrawVBIBFmt(void *vb, void *ib, int frmt, uint32_t numVerts, uint32_t ntri, uint32_t baseVert,
	uint32_t startTri);
struct CMallaD3D;
int Render_DrawMallaD3D(const struct CMallaD3D *m);
int Render_DrawVBIB(void *vb, void *ib, const struct CListaVert *v, uint32_t ntri);
/* the view matrix, its inverse as the billboard matrix and the eye */
int Render_SetView(const matrix_t *m);
int Render_GetWorld(matrix_t *out);
/* the alpha constant of the pixel shaders in reg: 1 1 1 0, 0 for transparent materials (not MAT_1000) and with a
 * forced material */
void Render_PSConstAlpha(const CMaterial *mat, uint32_t reg);
/* the texture stages of the reflection map (mapa 2) and the bump map before it (mapa 3, its scale and turn) */
void Mat_StageEnvBump(CMaterial *mat, uint32_t flags, uint32_t stageEnv, uint32_t stageBump);
/* the camera space reflection coordinates of mapa 2 scaled by R_EnvMapScale (R_EnvMapViewDep) */
void Mat_StageEnvMap(CMaterial *mat, uint32_t flags, uint32_t stage, float bias);
/* the render function of shader mode modo */
void Render_Shader(uint32_t modo, void *a, void *b);
/* texture coordinates of stage moved by a random part of +-ju, +-jv and scaled */
void Tex_StageJitter(void *unused, uint32_t flags, uint32_t stage, float ju, float jv, float scale);
/* the scroll (f10) and turn (v20[2] * k) of map idmapa */
void Mat_StageScroll(CMaterial *mat, uint32_t idmapa, uint32_t flags, uint32_t stage, float k);
/* texture coordinates of stage scrolled by su, sv per second, scaled and turned by rot per second */
void Tex_StageTransform(void *unused, uint32_t flags, uint32_t stage, float su, float sv, float scale, float rot);
/* the bump env matrix of stage: scale s (plus the f2c of mapa 3 times k) turned by rot */
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
/* the viewport size, CHECK that it is set */
int Render_GetViewportW(void);
int Render_GetViewportH(void);
/* the pixels of rect (0..1 of the viewport vp) as a D3DRECT, clamped; 0 (a point) when nothing is left */
byte Render_ClipRect(int *out, const uint32_t *vp, const box_t *rect);
/* the stencil bits of the depth buffer */
int Render_StencilBits(void);
/* the vertex buffer of a vertex list and the index buffer of triangles, in system memory with dynamic (HRESULT) */
int Render_CreateVB(void **vb, struct CListaVert *vert, byte dynamic);
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
struct CCamara;
void Render_SetCamara(struct CCamara *cam);
void Render_NoIndexedBlend(void *dev);
/* the viewport narrowed to rect and the projection to it (with proj); NULL the whole view again; 1 */
byte Render_SetViewRect(const box_t *rect, byte proj);
/* the screen position (pixels) of a world position; 0, -1 when it is not on the screen */
int Render_ToScreen(const vec3_t *pos, float *xy);

/* still the exe's */
/* rva 0x24d640: the planes of the frustum of view and proj */
void Frustum_Calc(const matrix_t *view, const matrix_t *proj);

/* other modules (src/imports.inc) */
void RenderLock_ReleaseD3D(void);		/* rva 0x2a4860 (RenderLock.cpp) */
void ListaVert_ReleaseD3D(void);		/* rva 0x2404e0 (ListaVert.cpp), empty */
void RenderEVA_ReleaseD3D(void);		/* rva 0x2b4550 (RenderEVA.cpp) */
void RenderTarget_ReleaseD3D(void);		/* rva 0x220c60 (RenderTarget.cpp) */
void RenderGamma_ReleaseD3D(void);		/* rva 0x2a7980 (RenderGamma.cpp) */
void RenderGamma_Begin(void);			/* rva 0x2a7520 */
void RenderGamma_LogShaderMask(void);		/* rva 0x2a7cd0, "ShaderModeMask" */
void RenderMallaD3D_Flush(void);		/* rva 0x2b6af0 (RenderMallaD3D.cpp) */
void RenderMallaD3D_Flush2(void);		/* rva 0x2b6e60 */
void RenderObjBase3D_Flush(void);		/* rva 0x2b7260 (RenderObjBase3D.cpp) */

#endif
