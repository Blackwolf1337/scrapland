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

#define MAX_PANTALLAS 10
#define MAX_MODELOS 10
#define TECLA_M 0x32
#define TECLA_CTRL 0x1d

#define g_EfecMallaOn GAME_DATA(int, 0x4db6b0)
#define g_EfecMallaStencil GAME_DATA(int, 0x4db6b4)
#define g_NumPantallas GAME_DATA(int, 0x4db6b8)
#define g_NumModelos GAME_DATA(int, 0x4db6bc)
#define g_EfecMallaMat ((CMaterial *)Game_Ptr(0x4db6c0))	/* a static object of the file */
#define g_Pantallas ((CModelo **)Game_Ptr(0x4db770))
#define g_Modelos ((CModelo **)Game_Ptr(0x4db798))
#define g_EfecMallaMatInic GAME_DATA(byte, 0x4db7c0)

void EfecMalla_AddPantalla(CModelo *m) {
	CHECK(g_NumPantallas < MAX_PANTALLAS);
	g_Pantallas[g_NumPantallas] = m;
	g_NumPantallas++;
}

void EfecMalla_AddModelo(CModelo *m) {
	CHECK(g_NumModelos < MAX_MODELOS);
	g_Modelos[g_NumModelos] = m;
	g_NumModelos++;
}

/* the disabled models drawn once each */
static void EfecMalla_RenderList(CModelo **list, int n) {
	for (int i = 0; i < n; i++) {
		CHECK(!list[i]->enable);
		CModelo_Enable(list[i]);
		CModelo_Render(list[i]);
		CModelo_Disable(list[i]);
	}
}

void EfecMalla_RenderModelos(void) {
	EfecMalla_RenderList(g_Modelos, g_NumModelos);
}

void EfecMalla_RenderPantallas(void) {
	EfecMalla_RenderList(g_Pantallas, g_NumPantallas);
}

static void EfecMalla_SetRS(uint32_t state, uint32_t value) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, state, value);
}

void EfecMalla_Render(void) {
	if (g_TecladoKeys[TECLA_M] && g_TecladoKeys[TECLA_CTRL]) {
		g_TecladoKeys[TECLA_M] = 0;
		g_EfecMallaOn = !g_EfecMallaOn;
	}
	if (!g_EfecMallaOn) {
		EfecMalla_RenderModelos();
		EfecMalla_RenderPantallas();
		return;
	}
	if (!g_EfecMallaMatInic) {
		CMaterial *m = g_EfecMallaMat;
		m->params.w3e = 0;
		m->params.i40 = (g_EfecMallaStencil != 0) + 4;
		memset(m->params.ambient, 0, 4);
		memset(m->params.diffuse, 0, 4);
		const byte emissive[4] = { 0x10, 0x10, 0x30, 0 };
		memcpy(m->params.emissive, emissive, 4);
		m->params.transp = 1;
		m->params.srcBlend = 2;
		m->params.dstBlend = 2;
		g_EfecMallaMatInic = 1;
		CMaterial_Prepare(m, MOTOR_DEV(g_Motor));
	}
	E3D_FlushBatch();
	if (g_EfecMallaStencil) {
		/* the screens mark the stencil */
		EfecMalla_SetRS(D3DRS_STENCILENABLE, 1);
		EfecMalla_SetRS(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		EfecMalla_SetRS(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		EfecMalla_SetRS(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		EfecMalla_SetRS(D3DRS_STENCILPASS, D3DSTENCILOP_INCR);
		EfecMalla_SetRS(D3DRS_STENCILREF, 1);
		EfecMalla_SetRS(D3DRS_STENCILMASK, 0xffffffffu);
		EfecMalla_SetRS(D3DRS_STENCILWRITEMASK, 0xffffffffu);
		EfecMalla_RenderPantallas();
		E3D_FlushBatch();
		EfecMalla_SetRS(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL);
		EfecMalla_SetRS(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		EfecMalla_SetRS(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		EfecMalla_SetRS(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
		EfecMalla_SetRS(D3DRS_STENCILREF, 1);
	}
	uint32_t fill;
	void *dev = g_pd3dDevice;
	g_MatFX1 = g_EfecMallaMat;
	D3D_METHOD(dev, d3dGetRenderState_t, D3DDEV_GETRENDERSTATE)(dev, D3DRS_FILLMODE, &fill);
	EfecMalla_SetRS(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	int stencil = g_SombrasStencil;
	g_SombrasStencil = 0;
	EfecMalla_RenderModelos();
	E3D_FlushBatch();
	g_SombrasStencil = stencil;
	g_MatFX1 = NULL;
	EfecMalla_SetRS(D3DRS_FILLMODE, fill);
	if (g_EfecMallaStencil) {
		EfecMalla_SetRS(D3DRS_STENCILENABLE, 0);
		EfecMalla_RenderModelos();
		return;
	}
	EfecMalla_RenderPantallas();
	E3D_FlushBatch();
	EfecMalla_RenderModelos();
}
