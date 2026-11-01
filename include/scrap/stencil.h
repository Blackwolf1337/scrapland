/*
 * Engine/stencil.cpp of the engine3d: the stencil shadows. While a frame is captured (g_StencilCapture) the meshes
 * of the shadow casters add the silhouette segments and triangles of their volumes, in groups per light
 * (StencilBlock); the frame pass draws the volumes into the stencil buffer and then the scene again, unlit where the
 * stencil is set. ShadowsStencil_Set (scrapmain.h), E3D_SetShadowLight (light.h) and Stencil_Mesh (render.h: the
 * silhouette and the cap triangles of a mesh list facing away from the light into a new group) are declared with
 * their users.
 */
#ifndef SCRAP_STENCIL_H
#define SCRAP_STENCIL_H

#include "scrap/e3dutil.h"
#include "scrap/light.h"
#include "scrap/material.h"
#include "scrap/render.h"

#define STENCIL_MAX_GROUPS 0x400

/* a group of volumes of one light */
typedef struct StencilBlock {
	CLight *Light;			/* 0x00 */
	vec3_t lightPos;		/* 0x04 g_ShadowLightPos */
	float range;			/* 0x10 g_ShadowRange */
	float cosCone;			/* 0x14 g_ShadowCos */
	byte directional;		/* 0x18 */
	byte spot;			/* 0x19 */
	byte pad_1a[2];
	vec3_t lightPosObj;		/* 0x1c in the space of the mesh */
	vec3_t lightDirObj;		/* 0x28 */
	matrix_t world;			/* 0x34 moved by R_StencilViewDisp along the view */
	int useg;			/* 0x74 the segments end here */
	int unused_78;			/* 0x78 */
	int utri;			/* 0x7c the cap triangles end here */
} StencilBlock;
SD_SIZE(StencilBlock, 0x80);

#define g_StencilBlock GAME_GLOBAL(StencilBlock *, g_StencilBlock)	/* new[] of STENCIL_MAX_GROUPS */
#define g_StencilGroups GAME_GLOBAL(int, g_StencilGroups)
#define g_StencilSegments GAME_GLOBAL(vec3_t *, g_StencilSegments)	/* new[] 0x30000: two points a segment */
#define g_StencilSegs GAME_GLOBAL(int, g_StencilSegs)
#define g_StencilTriInv GAME_GLOBAL(vec3_t *, g_StencilTriInv)	/* new[] 0x6c000: three points a triangle */
#define g_StencilInv GAME_GLOBAL(int, g_StencilInv)
#define g_StencilCapture GAME_GLOBAL(int, g_StencilCapture)	/* bSombrasStencilCapturar */
#define g_StencilVB GAME_GLOBAL(void *, g_StencilVB)	/* pVB_Stencil */
#define g_StencilIB GAME_GLOBAL(void *, g_StencilIB)	/* pIB_Stencil, quads of 4 vertices */
#define g_StencilVB2 GAME_GLOBAL(void *, g_StencilVB2)
#define g_StencilVB3 GAME_GLOBAL(void *, g_StencilVB3)
#define g_StencilMat (GAME_ARRAY(CMaterial, g_StencilMat))	/* the material of the volumes */

/* the light of the shadows (E3D_SetShadowLight) */
#define g_ShadowLight GAME_GLOBAL(CLight *, g_ShadowLight)
#define g_ShadowLightType GAME_GLOBAL(int, g_ShadowLightType)
#define g_ShadowLightPos GAME_GLOBAL(vec3_t, g_ShadowLightPos)	/* far away against the direction of a directional one */
#define g_ShadowLightDir GAME_GLOBAL(vec3_t, g_ShadowLightDir)
#define g_ShadowRange GAME_GLOBAL(float, g_ShadowRange)
#define g_ShadowCos GAME_GLOBAL(float, g_ShadowCos)	/* of half the cone angle */

/* engine variables */
#define g_ShowStencil GAME_GLOBAL_IN(int, GVar_R_ShowStencil, 16)	/* R_ShowStencil, 2: the volumes drawn */
#define g_StencilMult GAME_GLOBAL_IN(float, GVar_R_StencilMult, 16)	/* R_StencilMult */
#define g_StencilMultFrame GAME_GLOBAL(float, g_StencilMultFrame)
#define g_StencilRefWorld GAME_GLOBAL_IN(uint32_t, GVar_R_StencilRefWorld, 16)	/* R_StencilRefWorld */
#define g_StencilRefModels GAME_GLOBAL_IN(uint32_t, GVar_R_StencilRefModels, 16)	/* R_StencilRefModels */
#define g_StencilSelf GAME_GLOBAL_IN(int, GVar_R_StencilSelf, 16)	/* R_StencilSelf */
#define g_StencilTog GAME_GLOBAL_IN(int, GVar_R_StencilTog, 16)	/* R_StencilTog: switched once */

#define g_KeyV GAME_GLOBAL_IN(byte, g_KeyboardKeys, 31)	/* with X held: the shadows switched */

/* the lights of all groups on or off */
void StencilBlock_SetLights(byte on);
/* a debug line, the world matrix kept */
void Stencil_DebugLine(const vec3_t *a, const vec3_t *b, uint32_t color);
/* the frame is captured (on), the multiplier of the frame taken */
void ShadowsStencil_Capture(byte on);
void Stencil_Free(void);
/* the switch from R_StencilShadows and the stencil bits, the buffers when there are stencil bits */
void Stencil_Init(void);
/* the frame pass of the shadows */
void ShadowsStencil_Render(void);

/* the volumes of the segments and of the triangles, pass 0 (the front faces) fills the vertex buffers */
void Stencil_DrawSegs(int pass);
void Stencil_DrawTris(int pass);

#endif
