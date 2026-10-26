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
#define g_Viewport ((const uint32_t *)Game_Ptr(0x4ad7e0))
/* the planes of the view frustum (normal, d; outside where the distance is positive) */
#define MAX_PLANOS_FRUSTUM 6
#define g_Frustum ((const plane_t *)Game_Ptr(0x4adbf0))
/* the matrices of the frame: projection, view, the camera turn of the sprites, the world of the last draw; the eye */
#define g_MatProj (*(matrix_t *)Game_Ptr(0x4ae3f8))
#define g_MatView (*(matrix_t *)Game_Ptr(0x4ae4b8))
#define g_MatBillboard (*(matrix_t *)Game_Ptr(0x4ae4f8))
#define g_MatWorld (*(matrix_t *)Game_Ptr(0x4ae478))
#define g_EyePos (*(vec3_t *)Game_Ptr(0x4adda8))
/* the material of a ModelFX in place of the ones of the meshes, NULL */
#define g_MatForzado GAME_DATA(const CMaterial *, 0x4ad754)
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
#define g_PS ((const uint32_t *)Game_Ptr(0x4ad7b4))
#define g_PSOff GAME_DATA(uint32_t, 0x4ada98)

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
byte Render_Clear(const int *rect, uint32_t flags, uint32_t color, float z, uint32_t stencil);
void Render_GetViewportSize(int *ancho, int *alto);
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

/* still the exe's */
/* rva 0x24bac0: the world transform of the device (with defer only remembered); 0 */
int Render_SetWorldMatrix(const matrix_t *m, byte defer);
/* rva 0x24bc00: the world matrix set once (HRESULT) */
int Render_SetWorld(void);
/* rva 0x24bf30: the material for a pass, -1 with no material */
int Render_SetMaterial(void *mat, int pass);
/* rva 0x24b960: a line in the current world matrix; an error without a device */
int Render_Line(const vec3_t *a, const vec3_t *b, uint32_t color);
/* rva 0x24cf00: the screen position (pixels) of a world position; 0, -1 when it is not on the screen */
int Render_ToScreen(const vec3_t *pos, float *xy);
/* rva 0x24e650, 0x24e620: the viewport size, CHECK that it is set */
int Render_GetViewportW(void);
int Render_GetViewportH(void);
/* rva 0x24e220: rect of the viewport vp as a D3DRECT of the device */
void Render_ClipRect(int *out, const uint32_t *vp, const int *rect);
/* rva 0x24e6f0: the depth buffer has a stencil */
int Render_HasStencil(void);
/* rva 0x24d460, 0x24d380: the vertex buffer of a vertex list and the index buffer of triangles (HRESULT) */
struct CListaVert;
int Render_CreateVB(void **vb, struct CListaVert *vert, int flags);
int Render_CreateIB(void **ib, const void *tris, uint32_t nTri, int flags);

/* other modules (src/imports.inc) */
void RenderMallaD3D_Flush(void);		/* rva 0x2b6af0 (RenderMallaD3D.cpp) */
void RenderMallaD3D_Flush2(void);		/* rva 0x2b6e60 */
void RenderObjBase3D_Flush(void);		/* rva 0x2b7260 (RenderObjBase3D.cpp) */

#endif
