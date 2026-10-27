/*
 * Engine/stencil.cpp of the engine3d: the stencil shadows. While a frame is captured (g_StencilCapturar) the meshes
 * of the shadow casters add the silhouette segments and triangles of their volumes, in groups per light
 * (StencilBlock); the frame pass draws the volumes into the stencil buffer and then the scene again, unlit where the
 * stencil is set. SombrasStencil_Set (scrapmain.h), E3D_SetShadowLight (luz.h) and Stencil_Malla (render.h) are
 * declared with their users.
 */
#ifndef SCRAP_STENCIL_H
#define SCRAP_STENCIL_H

#include "scrap/luz.h"
#include "scrap/material.h"

#define STENCIL_MAX_GRUPOS 0x400

/* a group of volumes of one light */
typedef struct StencilBlock {
	CLuz *Light;			/* 0x00 */
	byte pad_4[0x70];
	int useg;			/* 0x74 the segments end here */
	byte pad_78[8];
} StencilBlock;
SD_SIZE(StencilBlock, 0x80);

#define g_StencilBlock GAME_DATA(StencilBlock *, 0x4afddc)	/* new[] of STENCIL_MAX_GRUPOS */
#define g_StencilGrupos GAME_DATA(int, 0x4afde0)
#define g_StencilSegmentos GAME_DATA(void *, 0x4afdcc)	/* new[] 0x30000 */
#define g_StencilSegs GAME_DATA(int, 0x4afdd0)
#define g_StencilTriangInv GAME_DATA(void *, 0x4afdd4)	/* new[] 0x6c000 */
#define g_StencilInv GAME_DATA(int, 0x4afdd8)
#define g_StencilCapturar GAME_DATA(int, 0x4afdc0)	/* bSombrasStencilCapturar */
#define g_StencilVB GAME_DATA(void *, 0x3902b0)	/* pVB_Stencil */
#define g_StencilIB GAME_DATA(void *, 0x3902b4)	/* pIB_Stencil, quads of 4 vertices */
#define g_StencilVB2 GAME_DATA(void *, 0x3902b8)
#define g_StencilVB3 GAME_DATA(void *, 0x3902bc)
#define g_StencilMat ((CMaterial *)Game_Ptr(0x3901d0))	/* the material of the volumes */

/* the light of the shadows (E3D_SetShadowLight) */
#define g_ShadowLight GAME_DATA(CLuz *, 0x4afdc4)
#define g_ShadowLightTipo GAME_DATA(int, 0x43257c)
#define g_ShadowLightPos GAME_DATA(vec3_t, 0x390030)	/* far away against the direction of a directional one */
#define g_ShadowLightDir GAME_DATA(vec3_t, 0x39006c)
#define g_ShadowRange GAME_DATA(float, 0x432580)
#define g_ShadowCos GAME_DATA(float, 0x4afdc8)	/* of half the cone angle */

/* engine variables */
#define g_ShowStencil GAME_DATA(int, 0x39004c)	/* R_ShowStencil, 2: the volumes drawn */
#define g_StencilMult GAME_DATA(float, 0x390028)	/* R_StencilMult */
#define g_StencilMultFrame GAME_DATA(float, 0x432578)
#define g_StencilRefWorld GAME_DATA(uint32_t, 0x390290)	/* R_StencilRefWorld */
#define g_StencilRefModels GAME_DATA(uint32_t, 0x390010)	/* R_StencilRefModels */
#define g_StencilSelf GAME_DATA(int, 0x3901b0)	/* R_StencilSelf */
#define g_StencilTog GAME_DATA(int, 0x390180)	/* R_StencilTog: switched once */

/* the filters of the scene drawing (render.cpp, also in Nodo3D.c) */
#define g_RenderClaseMask GAME_DATA(uint32_t, 0x4ad568)
#define g_RenderFlagMask GAME_DATA(uint32_t, 0x4ad5fc)
#define g_RenderFlagNeg GAME_DATA(byte, 0x4ad4ec)
#define g_FrameMs GAME_DATA(float, 0x4a8ed4)	/* the time of the last frame in ms */
#define g_KeyV GAME_DATA(byte, 0x4db2ef)	/* with X held: the shadows switched */

/* the lights of all groups on or off */
void StencilBlock_SetLuces(byte on);
/* a debug line, the world matrix kept */
void Stencil_DebugLine(const vec3_t *a, const vec3_t *b, uint32_t color);
/* the frame is captured (on), the multiplier of the frame taken */
void SombrasStencil_Capturar(byte on);
void Stencil_Free(void);
/* a CHECK, then the light position */
void Stencil_GetLightPos(vec3_t *out);
/* the switch from R_StencilShadows and the stencil bits, the buffers when there are stencil bits */
void Stencil_Init(void);
/* the frame pass of the shadows */
void SombrasStencil_Render(void);

/* still the exe's (src/imports.inc): the volumes of the segments and of the triangles, pass 0 front faces */
void Stencil_DrawSegs(int pass);
void Stencil_DrawTris(int pass);

#endif
