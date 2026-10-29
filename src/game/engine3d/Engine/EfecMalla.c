/*
 * The mesh view of the engine3d, original source Engine/efec_malla.cpp: Ctrl+M switches the models registered here
 * to a wireframe drawn with one material over them; with the stencil on, the screens are drawn first and the
 * wireframe only where they are not.
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/entrada.h"
#include "scrap/material.h"
#include "scrap/modelo.h"
#include "scrap/render.h"
#include "scrap/scrapworld.h"

#define MAX_SCREENS 10
#define MAX_MODELS 10
#define KEY_M 0x32
#define KEY_CTRL 0x1d

#define g_MeshEffectOn GAME_GLOBAL(int, g_MeshEffectOn, 0x4db6b0)
#define g_MeshEffectStencil GAME_GLOBAL(int, g_MeshEffectStencil, 0x4db6b4)
#define g_NumScreens GAME_GLOBAL(int, g_NumScreens, 0x4db6b8)
#define g_NumModels GAME_GLOBAL(int, g_NumModels, 0x4db6bc)
#define g_MeshEffectMat (GAME_ARRAY(CMaterial, g_MeshEffectMat, 0x4db6c0))	/* a static object of the file */
#define g_Screens (GAME_ARRAY(CModel *, g_Screens, 0x4db770))
#define g_Models (GAME_ARRAY(CModel *, g_Models, 0x4db798))
#define g_MeshEffectMatInit GAME_GLOBAL(byte, g_MeshEffectMatInit, 0x4db7c0)

void MeshEffect_AddScreen(CModel *m) {
	CHECK(g_NumScreens < MAX_SCREENS);
	g_Screens[g_NumScreens] = m;
	g_NumScreens++;
}

void MeshEffect_AddModel(CModel *m) {
	CHECK(g_NumModels < MAX_MODELS);
	g_Models[g_NumModels] = m;
	g_NumModels++;
}

/* the disabled models drawn once each */
static void MeshEffect_RenderList(CModel **list, int n) {
	for (int i = 0; i < n; i++) {
		CHECK(!list[i]->enable);
		CModel_Enable(list[i]);
		CModel_Render(list[i]);
		CModel_Disable(list[i]);
	}
}

void MeshEffect_RenderModels(void) {
	MeshEffect_RenderList(g_Models, g_NumModels);
}

void MeshEffect_RenderScreens(void) {
	MeshEffect_RenderList(g_Screens, g_NumScreens);
}

static void MeshEffect_SetRS(uint32_t state, uint32_t value) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, state, value);
}

void MeshEffect_Render(void) {
	if (g_KeyboardKeys[KEY_M] && g_KeyboardKeys[KEY_CTRL]) {
		g_KeyboardKeys[KEY_M] = 0;
		g_MeshEffectOn = !g_MeshEffectOn;
	}
	if (!g_MeshEffectOn) {
		MeshEffect_RenderModels();
		MeshEffect_RenderScreens();
		return;
	}
	if (!g_MeshEffectMatInit) {
		CMaterial *m = g_MeshEffectMat;
		m->params.w3e = 0;
		m->params.i40 = (g_MeshEffectStencil != 0) + 4;
		memset(m->params.ambient, 0, 4);
		memset(m->params.diffuse, 0, 4);
		const byte emissive[4] = { 0x10, 0x10, 0x30, 0 };
		memcpy(m->params.emissive, emissive, 4);
		m->params.transp = 1;
		m->params.srcBlend = 2;
		m->params.dstBlend = 2;
		g_MeshEffectMatInit = 1;
		CMaterial_Prepare(m, ENGINE_DEV(g_Engine));
	}
	E3D_FlushBatch();
	if (g_MeshEffectStencil) {
		/* the screens mark the stencil */
		MeshEffect_SetRS(D3DRS_STENCILENABLE, 1);
		MeshEffect_SetRS(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		MeshEffect_SetRS(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		MeshEffect_SetRS(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		MeshEffect_SetRS(D3DRS_STENCILPASS, D3DSTENCILOP_INCR);
		MeshEffect_SetRS(D3DRS_STENCILREF, 1);
		MeshEffect_SetRS(D3DRS_STENCILMASK, 0xffffffffu);
		MeshEffect_SetRS(D3DRS_STENCILWRITEMASK, 0xffffffffu);
		MeshEffect_RenderScreens();
		E3D_FlushBatch();
		MeshEffect_SetRS(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL);
		MeshEffect_SetRS(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		MeshEffect_SetRS(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		MeshEffect_SetRS(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
		MeshEffect_SetRS(D3DRS_STENCILREF, 1);
	}
	uint32_t fill;
	void *dev = g_pd3dDevice;
	g_MatFX1 = g_MeshEffectMat;
	D3D_METHOD(dev, d3dGetRenderState_t, D3DDEV_GETRENDERSTATE)(dev, D3DRS_FILLMODE, &fill);
	MeshEffect_SetRS(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	int stencil = g_ShadowsStencil;
	g_ShadowsStencil = 0;
	MeshEffect_RenderModels();
	E3D_FlushBatch();
	g_ShadowsStencil = stencil;
	g_MatFX1 = NULL;
	MeshEffect_SetRS(D3DRS_FILLMODE, fill);
	if (g_MeshEffectStencil) {
		MeshEffect_SetRS(D3DRS_STENCILENABLE, 0);
		MeshEffect_RenderModels();
		return;
	}
	MeshEffect_RenderScreens();
	E3D_FlushBatch();
	MeshEffect_RenderModels();
}
