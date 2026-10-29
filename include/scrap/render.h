/*
 * Engine/render.cpp of the engine3d: the renderer over the D3D 8 device (g_pd3dDevice): the frame state (matrices,
 * viewport, frustum, render pass), the material and world matrix of each draw, pixel shaders, dynamic vertex and
 * index buffers, clears and the debug primitives. Decompiled in parts (src/game/engine3d/Engine/Render.c); the rest is
 * still the exe's, the rva of each of those is given.
 */
#ifndef SCRAP_RENDER_H
#define SCRAP_RENDER_H

#include "scrap/cstring.h"
#include "scrap/d3d8.h"
#include "scrap/mathlib.h"
#include "scrap/tabla.h"

struct CMaterial;
typedef struct CMaterial CMaterial;
struct CListaVert;

/* the D3D device of the engine3d, NULL without one */
#define g_pd3dDevice GAME_GLOBAL(void *, g_pd3dDevice, 0x4ad6f4)
/* the identity matrix */
#define g_MatIdentity (GAME_ARRAY(const matrix_t, g_MatIdentity, 0x4ad6f8))
/* the triangles and vertices drawn in the frame (statistics) */
#define g_RenderTris GAME_GLOBAL_IN(int, g_RenderCounts, 0, 0x4a8f00)
#define g_RenderVerts GAME_GLOBAL_IN(int, g_RenderCounts, 4, 0x4a8f04)
/* nothing is drawn */
#define g_RenderOff GAME_GLOBAL_IN(byte, group_4a8ef8, 0, 0x4a8ef8)
/* only the meshes whose material has b38 are drawn */
#define g_RenderSoloB38 GAME_GLOBAL_IN(byte, group_4a8ef8, 1, 0x4a8ef9)
/* the meshes are drawn from their vertex buffers */
#define g_UsarVB GAME_GLOBAL_IN(byte, group_431bac, 0, 0x431bac)
#define g_RenderPass GAME_GLOBAL(int, g_RenderPass, 0x4afdb8)
/* the D3DRS_ZENABLE value of the frame and whether the fog is on */
#define g_ZEnable GAME_GLOBAL(uint32_t, g_ZEnable, 0x4ad740)
#define g_FogActive GAME_GLOBAL(int, g_FogActive, 0x4ad744)
/* the D3DRS_AMBIENT of the scenes */
#define g_AmbientD3D GAME_GLOBAL(uint32_t, g_AmbientD3D, 0x4ad748)
/* the viewport: its size, and all of it (x, y, w, h, minZ, maxZ) */
#define g_ViewportW GAME_GLOBAL_IN(int, g_Viewport, 8, 0x4ad7e8)
#define g_ViewportH GAME_GLOBAL_IN(int, g_Viewport, 12, 0x4ad7ec)
#define g_Viewport (GAME_ARRAY(uint32_t, g_Viewport, 0x4ad7e0))
/* the planes of the view frustum (normal, d; outside where the distance is positive) */
#define MAX_PLANOS_FRUSTUM 6
#define g_Frustum (GAME_ARRAY(const plane_t, g_Frustum, 0x4adbf0))
/* the texture filters of the stages (Render_ResetState from the variables below): min, mag, mip, anisotropy */
#define g_FiltroMin GAME_GLOBAL(uint32_t, g_FiltroMin, 0x431d7c)
#define g_FiltroMag GAME_GLOBAL(uint32_t, g_FiltroMag, 0x431d80)
#define g_FiltroMip GAME_GLOBAL(uint32_t, g_FiltroMip, 0x431d70)
#define g_Anisotropia GAME_GLOBAL(uint32_t, g_Anisotropia, 0x431d78)
#define g_TexFilter GAME_GLOBAL_IN(int, GVar_R_TexFilter, 16, 0x4add58)		/* R_TexFilter 0 point, 1 linear, 2 anisotropic */
#define g_MipFilter GAME_GLOBAL_IN(int, GVar_R_MipFilter, 16, 0x4ad8f8)		/* R_MipFilter 0 point, 1 linear */
#define g_Anisotropy GAME_GLOBAL_IN(uint32_t, GVar_R_Anisotropy, 16, 0x4adba0)	/* R_Anisotropy */
#define g_MipmapBias GAME_GLOBAL_IN(float, GVar_R_MipmapBias, 16, 0x4ada68)		/* R_MipmapBias */
#define g_MipmapBias2 GAME_GLOBAL(float, g_MipmapBias2, 0x4ad73c)
#define g_MipmapsOn GAME_GLOBAL_IN(byte, group_4ad6f0, 2, 0x4ad6f2)
#define MOTOR_MAXANISOTROPY(m) (*(uint32_t *)((m) + 0x2a448))
/* the address modes of the stages as Render_SetMapa last set them */
#define g_StageMip (GAME_ARRAY(byte, g_StageMip, 0x4addb4))	/* the b8 (mipmaps) of the map */
#define g_StageTile (GAME_ARRAY(byte, g_StageTile, 0x4addbc))	/* the bc of the map: bit 0 wraps u, bit 1 v */
/* the texture of each stage and the frame it was set in */
#define g_StageTex (GAME_ARRAY(void *, g_StageTex, 0x4ae594))	/* [8] */
#define g_StageTexFrame (GAME_ARRAY(int, g_StageTexFrame, 0x4ae5b4))	/* [8] */
#define g_TexChanges GAME_GLOBAL_IN(int, g_RenderCounts, 8, 0x4a8f08)
#define g_VBIBTriLimit GAME_GLOBAL_IN(uint32_t, GVar_R_VBIBTriLimit, 16, 0x4ad820)	/* R_VBIBTriLimit */
/* the projection split into g_Tiles x g_Tiles parts (screenshots larger than the screen), one per frame */
#define g_Tiles GAME_GLOBAL(int, g_Tiles, 0x432414)
#define g_TileState GAME_GLOBAL(int, g_TileState, 0x4ae5d4)
#define g_TileX GAME_GLOBAL(int, g_TileX, 0x4ae5d8)
#define g_TileY GAME_GLOBAL(int, g_TileY, 0x4ae5dc)
#define g_TileOn GAME_GLOBAL_IN(byte, group_4a8ef8, 3, 0x4a8efb)
/* the D3DTOP of the lightmap stage */
#define g_LightmapOp GAME_GLOBAL(uint32_t, g_LightmapOp, 0x432408)
/* Render_SetMaterial: the last material, pass, frame and context (it is skipped for the same ones), the shader mode */
#define g_MatPass GAME_GLOBAL(int, g_MatPass, 0x4ae588)
#define g_MatFrame GAME_GLOBAL(int, g_MatFrame, 0x4ae58c)
#define g_MatCtx GAME_GLOBAL(CMaterial *, g_MatCtx, 0x4ae584)
#define g_ShaderActual GAME_GLOBAL(uint32_t, g_ShaderActual, 0x4ae590)
#define g_MatChanges GAME_GLOBAL_IN(int, g_RenderCounts, 12, 0x4a8f0c)
/* RenderLightmapMat, the lightmap material of the draw (counted); with one the shader modes have 0x100 */
#define g_LightmapMat GAME_GLOBAL(CMaterial *, g_LightmapMat, 0x4addc4)
#define g_UsarShaders GAME_GLOBAL_IN(byte, group_43240c, 1, 0x43240d)
#define g_MaterialLog GAME_GLOBAL_IN(int, GVar_R_MaterialLog, 16, 0x4adbe8)		/* R_MaterialLog */
#define g_DecalZBias GAME_GLOBAL_IN(uint32_t, GVar_R_DecalZBias, 16, 0x4add88)	/* R_DecalZBias */
#define g_AlphaFog GAME_GLOBAL_IN(int, GVar_R_AlphaFog, 16, 0x4ada80)		/* R_AlphaFog 0, 1 (only add blending), 2 */
#define g_UseDefMaterial GAME_GLOBAL_IN(int, GVar_R_UseDefMaterial, 16, 0x4ad8e0)	/* R_UseDefMaterial 0, 1, 2 */
#define g_UseDefTexture GAME_GLOBAL_IN(int, GVar_R_UseDefTexture, 16, 0x4ad868)	/* R_UseDefTexture 0..4 */
/* the default materials RenderIniD3D makes, in this order: white, checkerboard, grey, invisible, bump noise, ground */
#define g_MatBlanco GAME_GLOBAL(CMaterial *, g_MatBlanco, 0x4ad6d8)
#define g_MatAjedrez GAME_GLOBAL(CMaterial *, g_MatAjedrez, 0x4ad6dc)
#define g_MatGris GAME_GLOBAL(CMaterial *, g_MatGris, 0x4ad6e0)
#define g_MatInvis GAME_GLOBAL(CMaterial *, g_MatInvis, 0x4ad6e4)
#define g_MatBump GAME_GLOBAL(CMaterial *, g_MatBump, 0x4ad6e8)
#define g_MatSuelo GAME_GLOBAL(CMaterial *, g_MatSuelo, 0x4ad6ec)
/* the matrices of the frame: projection, view, the camera turn of the sprites, the world of the last draw; the eye */
#define g_MatProj (GAME_GLOBAL(matrix_t, g_MatProj, 0x4ae3f8))
#define g_MatView (GAME_GLOBAL(matrix_t, g_MatView, 0x4ae4b8))
#define g_MatBillboard (GAME_GLOBAL(matrix_t, g_MatBillboard, 0x4ae4f8))
#define g_MatWorld (GAME_GLOBAL(matrix_t, g_MatWorld, 0x4ae478))
#define g_EyePos (GAME_GLOBAL(vec3_t, g_EyePos, 0x4adda8))
/* the material of a ModelFX in place of the ones of the meshes, NULL */
#define g_MatForzado GAME_GLOBAL(const CMaterial *, g_MatForzado, 0x4ad754)
/* the other overrides of ModelFX.cpp, cleared with it */
#define g_MatFX1 GAME_GLOBAL(CMaterial *, g_MatFX1, 0x4ad74c)	/* in place of every material */
#define g_MatFX2 GAME_GLOBAL(CMaterial *, g_MatFX2, 0x4ad750)	/* in place of the lightmap material */
#define g_MatFX3 GAME_GLOBAL(const CMaterial *, g_MatFX3, 0x4ad758)	/* its colors for the D3D material */
/* bUsarHWEnvMap: the environment maps through the texture transform of the device (materials with two texture sets
 * get VERT_TEX1 instead of VERT_TEX2) */
#define g_UsarEnvMap GAME_GLOBAL(int, g_UsarEnvMap, 0x431bd8)		/* video UsarEnvMap */
#define g_UsarHWEnvMap GAME_GLOBAL(int, g_UsarHWEnvMap, 0x431bdc)
#define g_UsarBumpMap GAME_GLOBAL(int, g_UsarBumpMap, 0x431be0)		/* video UsarBumpMap */
#define g_UsarGlowMap GAME_GLOBAL(int, g_UsarGlowMap, 0x431be4)		/* video UsarGlowMap */
/* the engine variables of the environment and bump maps (Gvars) */
#define g_EnvBumpBias GAME_GLOBAL_IN(int, GVar_R_EnvBumpBias, 16, 0x4adbb8)		/* R_EnvBumpBias, DXT1 bump textures */
#define g_EnvBumpScale GAME_GLOBAL_IN(float, GVar_R_EnvBumpScale, 16, 0x4ada08)	/* R_EnvBumpScale */
#define g_EnvMapOffset GAME_GLOBAL_IN(float, GVar_R_EnvMapOffset, 16, 0x4adac8)	/* R_EnvMapOffset */
#define g_EnvMapScale GAME_GLOBAL_IN(float, GVar_R_EnvMapScale, 16, 0x4adb40)	/* R_EnvMapScale */
#define g_EnvMapViewDep GAME_GLOBAL_IN(int, GVar_R_EnvMapViewDep, 16, 0x4adb58)	/* R_EnvMapViewDep 0, 1, 2 */
#define g_EnvBump GAME_GLOBAL_IN(int, g_GvEnvBump, 16, 0x4ad838)		/* R_EnvBump */
#define g_EnvMap GAME_GLOBAL_IN(int, g_GvEnvMap, 16, 0x4adda0)		/* R_EnvMap */
#define g_EnvBlend GAME_GLOBAL_IN(int, GVar_R_EnvBlend, 16, 0x4ad9d8)		/* R_EnvBlend */
/* the engine variables of the Clouds and GlowFlick shader modes (R_CloudR and so on) */
#define g_CloudR GAME_GLOBAL_IN(float, GVar_R_CloudR, 16, 0x4adb10)
#define g_CloudG GAME_GLOBAL_IN(float, GVar_R_CloudG, 16, 0x4ad9f0)
#define g_CloudB GAME_GLOBAL_IN(float, GVar_R_CloudB, 16, 0x4ad850)
#define g_CloudA GAME_GLOBAL_IN(float, GVar_R_CloudA, 16, 0x4adb88)
#define g_CloudEmi GAME_GLOBAL_IN(float, GVar_R_CloudEmi, 16, 0x4ad8b0)
#define g_CloudVel1x GAME_GLOBAL_IN(float, GVar_R_CloudVel1x, 16, 0x4adaf8)
#define g_CloudVel1y GAME_GLOBAL_IN(float, GVar_R_CloudVel1y, 16, 0x4adae0)
#define g_CloudScale1 GAME_GLOBAL_IN(float, GVar_R_CloudScale1, 16, 0x4ad880)
#define g_CloudVel2x GAME_GLOBAL_IN(float, GVar_R_CloudVel2x, 16, 0x4ada38)
#define g_CloudVel2y GAME_GLOBAL_IN(float, GVar_R_CloudVel2y, 16, 0x4adb28)
#define g_CloudScale2 GAME_GLOBAL_IN(float, GVar_R_CloudScale2, 16, 0x4adc78)
#define g_GlowFlickTile GAME_GLOBAL_IN(float, GVar_R_GlowFlickTile, 16, 0x4adb70)
#define g_GlowFlickRot GAME_GLOBAL_IN(float, GVar_R_GlowFlickRot, 16, 0x4adc90)
#define g_GlowFlickBump GAME_GLOBAL_IN(float, GVar_R_GlowFlickBump, 16, 0x4ada20)
#define g_GlowFlickBRot GAME_GLOBAL_IN(float, GVar_R_GlowFlickBRot, 16, 0x4adc60)
#define g_GlowFlickMod GAME_GLOBAL_IN(float, GVar_R_GlowFlickMod, 16, 0x4ad898)
/* the render functions of the shader modes (two tables, the second for the modes with 0x100) */
#define MAX_SHADER_TAB 32
typedef void (*shaderRender_t)(void *a, void *b);
typedef struct ShaderTab {
	uint32_t Index;			/* 0x00 its own index */
	const char *Nombre;		/* 0x04 */
	const char *Archivo;		/* 0x08 the .psh, NULL without a pixel shader */
	int i0c;			/* 0x0c */
	uint32_t flags;			/* 0x10 0x100, 0x101 */
	shaderRender_t fnRender;	/* 0x14 */
} ShaderTab;
SD_SIZE(ShaderTab, 0x18);
#define g_ShaderTab (GAME_ARRAY(const ShaderTab, g_ShaderTab, 0x431e08))	/* [32], ends with a NULL name */
#define g_ShaderTab2 (GAME_ARRAY(const ShaderTab, g_ShaderTab2, 0x432108))
/* the pixel shaders: on (the device has them), their handles, the ones switched off (a bit each from 0x200000) */
#define MAX_PS 9
#define g_PSOn GAME_GLOBAL_IN(int, GVar_R_PShaders, 16, 0x4ada50)
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
#define g_PSOff GAME_GLOBAL_IN(uint32_t, GVar_R_PSMask, 16, 0x4ada98)
/* all pixel shaders made from their files (the names end with NULL) */
#define g_PSNames (GAME_ARRAY(const char *const, g_PSNames, 0x431d88))
#define g_PSHandles (GAME_ARRAY(uint32_t, g_PSHandles, 0x4ad760))	/* [32], one per slot of g_PSNames */
#define g_PShaderCount GAME_GLOBAL_IN(int, GVar_R_PShaderCount, 16, 0x4add70)	/* R_PShaderCount, the changes of the frame */
#define g_CullMode GAME_GLOBAL_IN(int, GVar_R_CullMode, 16, 0x4ad910)	/* R_CullMode, 1 two-sided */
/* the world matrix was given to the device */
#define g_WorldSet GAME_GLOBAL(int, g_WorldSet, 0x432410)
/* the whole viewport (g_Viewport is the one of the current view) */
#define g_ViewportFull (GAME_ARRAY(uint32_t, g_ViewportFull, 0x4ad7f8))
#define g_D3DOn GAME_GLOBAL_IN(byte, group_4ad6f0, 1, 0x4ad6f1)
#define g_MatDefault ((void *)GAME_PTR(CMaterial_MatDefault, 0, 0x4adc98))
#define g_MatLinea ((void *)GAME_PTR(CMaterial_MatLinea, 0, 0x4ad918))	/* the material of the debug lines */
#define g_ShowSphereVis GAME_GLOBAL_IN(int, GVar_R_ShowSphereVis, 16, 0x4adbd0)	/* R_ShowSphereVis 0, 1, 2 */
#define g_VersionStr (GAME_ARRAY(char, g_VersionStr, 0x4ae540))	/* 0x40 */
#define MOTOR_ZFORMAT(m) (*(uint32_t *)((m) + 0x2a4d0))
#define g_MatActual GAME_GLOBAL(const CMaterial *, g_MatActual, 0x4ae580)
/* what CNodo3D_Render draws: the object classes (low 24 bits of the mask), the nodes whose flags & g_RenderFlagMask
 * is not 0 when g_RenderFlagNeg */
#define g_RenderClaseMask GAME_GLOBAL(uint32_t, g_RenderClaseMask, 0x4ad568)
#define g_RenderFlagMask GAME_GLOBAL(uint32_t, g_RenderFlagMask, 0x4ad5fc)
#define g_RenderFlagNeg GAME_GLOBAL(byte, g_RenderFlagNeg, 0x4ad4ec)	/* the last one Render_SetMaterial set */
/* the D3DADAPTER_IDENTIFIER8 of the device, its texts "driver [description]  version" and the ids, and the device
 * of the last log of them */
#define g_Adapter (GAME_GLOBAL(D3DAdapterId, g_Adapter, 0x4addc8))
#define g_DriverStr (GAME_ARRAY(char, g_DriverStr, 0x4ae1f8))	/* 0x100 */
#define g_DeviceStr (GAME_ARRAY(char, g_DeviceStr, 0x4ae2f8))	/* 0x100 */
#define g_DriverName (GAME_ARRAY_IN(cString, GVar_R_LastDevice, 16, 0x4ad8c8))
#define g_DeviceLogged GAME_GLOBAL(int, g_DeviceLogged, 0x4ae538)
/* the identity scaled by 1/2048 */
#define g_MatBias (GAME_GLOBAL(matrix_t, g_MatBias, 0x4ae438))
#define g_TexNoPow2 GAME_GLOBAL_IN(byte, group_4ad738, 0, 0x4ad738)	/* D3DPTEXTURECAPS_NONPOW2CONDITIONAL */
#define g_TexPow2 GAME_GLOBAL_IN(byte, group_4ad738, 1, 0x4ad739)	/* D3DPTEXTURECAPS_POW2 */
#define g_TexRepeat GAME_GLOBAL_IN(byte, group_4ad6f0, 3, 0x4ad6f3)	/* D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE */
#define g_UsarShaders2 GAME_GLOBAL_IN(byte, group_43240c, 0, 0x43240c)
#define g_RenderI1F4 GAME_GLOBAL(int, g_RenderI1F4, 0x4ae1f4)

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
const char *Render_GetDriver(void);
/* the start of the D3D device: caps, the log of the device, base stage states, the pixel shaders from their
 * files, the device objects of the modules, the default materials */
void Render_IniD3D(void);
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
/* a mesh with its material (and the second pass of its second texture set), the skinned and stencil shadow ones
 * through their modules; r is the render node, NULL */
struct CNodoR;
int Render_MallaD3D(struct CMallaD3D *m, struct CNodoR *r);
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
/* the planes of the frustum of view and proj (UpdateVisibilityPlanes) */
void Frustum_Calc(const matrix_t *view, const matrix_t *proj);

/* Engine/RenderEVA.cpp (src/game/engine3d/Engine/RenderEVA.c): the meshes with a vertex animation (EVA) or a skin,
 * moved in software into a dynamic vertex buffer of 0x1000 vertices (VERT_TEX1), their shadow positions into the
 * main mesh of the node; and the particles, quads in a dynamic vertex buffer of 0x800 vertices drawn through a fixed
 * index buffer of 0x200 quads */
#define MAX_VERT_VB 0x1000
#define VB_PARTICLES 0x200
#define g_EVAVB GAME_GLOBAL(void *, g_EVAVB, 0x4dea50)		/* IDirect3DVertexBuffer8, D3DFVF 0x112 */
#define g_EVAVBUsed GAME_GLOBAL(int, g_EVAVBUsed, 0x4deb74)
#define g_PartVB GAME_GLOBAL(void *, g_PartVB, 0x4deb78)		/* D3DFVF 0x1c2 (Grafico3DVert) */
#define g_PartIB GAME_GLOBAL(void *, g_PartIB, 0x4deb7c)
#define g_PartVBUsed GAME_GLOBAL(int, g_PartVBUsed, 0x4deb80)
#define g_PartTri GAME_GLOBAL(uint16_t *, g_PartTri, 0x4deb84)	/* pTriParticles, new[] */
#define g_PartNumTri GAME_GLOBAL(int, g_PartNumTri, 0x4deb88)
/* the particles drawn in the frame (statistics) */
#define g_RenderParts GAME_GLOBAL_IN(int, g_RenderCounts, 24, 0x4a8f18)
/* the engine variables */
#define g_MaxSparkSize GAME_GLOBAL_IN(float, GVar_R_MaxSparkSize, 16, 0x4dea94)	/* R_MaxSparkSize */
#define g_MinSparkDist GAME_GLOBAL_IN(float, GVar_R_MinSparkDist, 16, 0x4deadc)	/* R_MinSparkDist */
#define g_ParticlesAlphaDist GAME_GLOBAL_IN(int, GVar_R_ParticlesAlphaDist, 16, 0x4deb24)	/* R_ParticlesAlphaDist */
#define g_ParticlesDraw GAME_GLOBAL_IN(int, GVar_R_ParticlesDraw, 16, 0x4deaf4)	/* R_ParticlesDraw */
#define g_ParticlesLock GAME_GLOBAL_IN(int, GVar_R_ParticlesLock, 16, 0x4deb54)	/* R_ParticlesLock: 1 always discard, 2 the size too */
#define g_ParticlesOne GAME_GLOBAL_IN(int, GVar_R_ParticlesOne, 16, 0x4dea64)		/* R_ParticlesOne */
#define g_ParticlesZBias GAME_GLOBAL_IN(int, GVar_R_ParticlesZBias, 16, 0x4deb0c)	/* R_ParticlesZBias */
#define g_ShowEVA GAME_GLOBAL_IN(int, GVar_R_ShowEVA, 16, 0x4deac4)		/* R_ShowEVA */
#define g_ShowSkin GAME_GLOBAL_IN(int, GVar_R_ShowSkin, 16, 0x4deaac)		/* R_ShowSkin */
#define g_SkinNodeSpace GAME_GLOBAL_IN(int, GVar_R_SkinNodeSpace, 16, 0x4deb6c)	/* R_SkinNodeSpace */
#define g_UseEVA GAME_GLOBAL_IN(int, GVar_R_UseEVA, 16, 0x4deb3c)		/* R_UseEVA */
#define g_UseSkin GAME_GLOBAL_IN(int, GVar_R_UseSkin, 16, 0x4dea7c)		/* R_UseSkin */

struct CParticula;
struct Grafico3DVert;
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
int RenderEVA_DrawSkinNodo(struct CMallaD3D *m, struct CNodoR *r, void *skin);
/* the mesh with its skin over the render nodes of the model; HRESULT */
int RenderEVA_DrawSkin(struct CMallaD3D *m, struct CNodoR *r);
/* the four corners of a particle: a square of its size turned by rot facing the camera (bill: the billboard
 * matrix in the space of the particles, view: the view matrix of it), sparks stretched along their screen motion;
 * alpha (below 1) scales the inverted alpha of the color, zbias moves it to the camera */
void Particula_Quad(struct CParticula *p, const matrix_t *bill, const matrix_t *view, struct Grafico3DVert *out,
	float alpha, float zbias);

/* Engine/RenderMallaD3D.cpp, RenderMallaD3D_transp.cpp and RenderObjBase3D.cpp
 * (src/game/engine3d/Engine/RenderMallaD3D.c): the batches of a view. Opaque meshes (R_MeshCache) are sorted by
 * texture, material and skin, transparent ones and the particle systems and flares far to near, each drawn with the
 * world matrix it was queued with. Up to 0x400 of each, a full batch is drawn at once. */
#define MAX_RENDER_BATCH 0x400
typedef struct RenderMallaEntry {
	struct CMallaD3D *malla;	/* 0x00 */
	matrix_t world;			/* 0x04 */
	uint32_t tex;			/* 0x44 the diffuse texture >> 1, 0 without */
	int list;			/* 0x48 the batch count when the list of the mesh was queued */
	struct CNodoR *r;		/* 0x4c */
} RenderMallaEntry;
SD_SIZE(RenderMallaEntry, 0x50);
typedef struct RenderTranspEntry {
	struct CMallaD3D *malla;	/* 0x00 */
	matrix_t world;			/* 0x04 */
	int group;			/* 0x44 g_TranspGroup, 0 for the identity */
	struct CNodoR *r;		/* 0x48 */
	float dist2;			/* 0x4c to the eye, squared */
} RenderTranspEntry;
SD_SIZE(RenderTranspEntry, 0x50);
typedef struct RenderObjEntry {
	struct IObjeto3D *obj;		/* 0x00 a CSistPart or a CFlare */
	matrix_t world;			/* 0x04 */
	struct CNodoR *r;		/* 0x44 */
	float dist2;			/* 0x48 */
} RenderObjEntry;
SD_SIZE(RenderObjEntry, 0x4c);
#define g_TabMalla (GAME_GLOBAL(CTabla, g_TabMalla, 0x4deb90))		/* TabMallaD3D: RenderMallaEntry */
#define g_ListaMalla (GAME_ARRAY(RenderMallaEntry *, g_ListaMalla, 0x4debb0))
#define g_UsadosMalla GAME_GLOBAL_IN(int, g_ListaMalla, 4096, 0x4dfbb0)
#define g_TabTransp (GAME_GLOBAL(CTabla, g_TabTransp, 0x4dfbb8))		/* RenderTranspEntry */
#define g_ListaTransp (GAME_ARRAY(RenderTranspEntry *, g_ListaTransp, 0x4dfbf0))
#define g_UsadosTransp GAME_GLOBAL_IN(int, g_ListaTransp, 4096, 0x4e0bf0)
#define g_TranspGroup GAME_GLOBAL(int, g_TranspGroup, 0x432a78)		/* counted up for each mesh list, 1 after a flush */
#define g_TabObj (GAME_GLOBAL(CTabla, g_TabObj, 0x4e0bf8))		/* TabObjBase3D: RenderObjEntry */
#define g_ListaObj (GAME_ARRAY(RenderObjEntry *, g_ListaObj, 0x4e0c00))
#define g_UsadosObj GAME_GLOBAL_IN(int, g_ListaObj, 4096, 0x4e1c00)
#define g_MeshCache GAME_GLOBAL_IN(int, GVar_R_MeshCache, 16, 0x4deba8)		/* R_MeshCache, 10 logs the batches */
#define g_ShowTranspBox GAME_GLOBAL_IN(int, GVar_R_ShowTranspBox, 16, 0x4dfbe8)	/* R_ShowTranspBox */
#define g_TranspZBias GAME_GLOBAL_IN(float, GVar_R_TranspZBias, 16, 0x4dfbd0)	/* R_TranspZBias */

/* the batches drawn; E_FAIL without a device, else 0 */
int RenderMallaD3D_Flush(void);
int RenderMallaD3D_Flush2(void);
int RenderObjBase3D_Flush(void);
/* a transparent mesh queued (cont: the same list as the one before, else a new group); HRESULT */
int RenderMallaD3D_AddTransp(struct CMallaD3D *m, const matrix_t *world, byte cont, struct CNodoR *r);
/* the qsort orders: texture, material, then the skinned ones last; the transparent ones and the objects far first */
int RenderMalla_Compare(const void *a, const void *b);
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
void Stencil_Malla(struct CListaVert *v, const void *tris, uint32_t nTri, struct CMallaD3D *m, struct CNodoR *r);
/* CListaVert_EnvMapUV: listavert.h */
void ListaVert_InitD3D(void);			/* rva 0x2404f0, empty */
void RenderTarget_InitD3D(void);		/* rva 0x220d20 */
void ModelFX_InitD3D(void);			/* ModelFX.c, modelo.h */
void ListaVert_ReleaseD3D(void);		/* rva 0x2404e0 (ListaVert.cpp), empty */
void RenderTarget_ReleaseD3D(void);		/* rva 0x220c60 (RenderTarget.cpp) */

#endif
