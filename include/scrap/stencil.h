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

#include "scrap/gvar.h"
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

extern float g_StencilMultFrame;

#define g_KeyV g_KeyboardKeys[31]	/* with X held: the shadows switched */

/* the frame is captured (on), the multiplier of the frame taken */
void ShadowsStencil_Capture(byte on);
void Stencil_Free(void);
/* the switch from R_StencilShadows and the stencil bits, the buffers when there are stencil bits */
void Stencil_Init(void);
/* the frame pass of the shadows */
void ShadowsStencil_Render(void);

#endif
