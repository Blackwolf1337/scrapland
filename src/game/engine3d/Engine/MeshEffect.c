/*
 * The mesh view of the engine3d, original source Engine/efec_malla.cpp: Ctrl+M switches the models registered here
 * to a wireframe drawn with one material over them; with the stencil on, the screens are drawn first and the
 * wireframe only where they are not.
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/e3dinput.h"
#include "scrap/material.h"
#include "scrap/model.h"
#include "scrap/render.h"
#include "scrap/scrapworld.h"

#define MAX_SCREENS 10
#define MAX_MODELS 10
#define KEY_M 0x32
#define KEY_CTRL 0x1d

extern int g_MeshEffectOn;
extern int g_MeshEffectStencil;
extern int g_NumScreens;
extern int g_NumModels;
#define g_MeshEffectMat (GAME_ARRAY(CMaterial, g_MeshEffectMat))	/* a static object of the file */
extern CModel * g_Screens[10];
extern CModel * g_Models[10];
extern byte g_MeshEffectMatInit;

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
		m->params.fog = 0;
		m->params.zWrite = 0;
		m->params.zFunc = (g_MeshEffectStencil != 0) + 4;
		memset(m->params.ambient, 0, 4);
		memset(m->params.diffuse, 0, 4);
		const byte emissive[4] = { 0x10, 0x10, 0x30, 0 };
		memcpy(m->params.emissive, emissive, 4);
		m->params.transp = 1;
		m->params.srcBlend = 2;
		m->params.dstBlend = 2;
		g_MeshEffectMatInit = 1;
		CMaterial_Prepare(m, g_Engine->device);
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
