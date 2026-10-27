/*
 * The stencil shadows of the engine3d, original source Engine/stencil.cpp (include/scrap/stencil.h): the buffers,
 * the shadow light, the switch, and the frame pass that draws the shadow volumes into the stencil buffer and the
 * scene again where it is set. The volumes themselves (Stencil_Malla, the segment and triangle passes) are still
 * the exe's.
 */
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/fuentes.h"
#include "scrap/luz.h"
#include "scrap/material.h"
#include "scrap/modelo.h"
#include "scrap/prof.h"
#include "scrap/render.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/stencil.h"
#include "scrap/utild3d.h"
#include "scrap/bsp.h"
#include "scrap/varios.h"

vec3_t *Vec3_Normalize(vec3_t *out, const vec3_t *in);

#define LUZ_DIRECCIONAL 0x138a

/* the D3DRS_* states of the pass */
#define RS_ZWRITEENABLE 14
#define RS_SHADEMODE 9
#define RS_SRCBLEND 19
#define RS_DESTBLEND 20
#define RS_CULLMODE 22
#define RS_ALPHABLENDENABLE 27
#define RS_STENCILENABLE 52
#define RS_STENCILFAIL 53
#define RS_STENCILZFAIL 54
#define RS_STENCILPASS 55
#define RS_STENCILFUNC 56
#define RS_STENCILREF 57
#define RS_STENCILMASK 58
#define RS_STENCILWRITEMASK 59
#define RS_COLORWRITEENABLE 168

static void Stencil_RS(uint32_t state, uint32_t value) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, state, value);
}

void StencilBlock_SetLuces(byte on) {
	for (int i = 0; i < g_StencilGrupos; i++) {
		CHECK(g_StencilBlock[i].Light);
		CLuz_SetActiva(g_StencilBlock[i].Light, on);
	}
}

void Stencil_DebugLine(const vec3_t *a, const vec3_t *b, uint32_t color) {
	matrix_t world;
	void *dev = g_pd3dDevice;

	D3D_METHOD(dev, d3dGetTransform_t, D3DDEV_GETTRANSFORM)(dev, D3DTS_WORLD, &world);
	Debug_DrawLine(a, b, color);
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_WORLD, &world);
}

void SombrasStencil_Set(byte on) {
	byte ok;

	if (!Render_StencilBits()) {
		Con_Warnf("%s", (const char *)Game_Ptr(0x3dc684));	/* "No stencil bits\n" */
		ok = 0;
	} else if (Render_StencilBits() < 4) {
		Con_Warnf("%s", (const char *)Game_Ptr(0x3dc698));	/* "Not enought stencil bits\n" */
		ok = 0;
	} else {
		ok = 1;
	}
	byte v = ok ? on : 0;
	E3D_LogDebug(g_FileLog, "Stencil shadows %s", v ? "ON" : "OFF");
	g_SombrasStencil = v;
	g_StencilCapturar = 0;
	g_StencilRender = NULL;
	g_RenderClaseMask = 0;
	g_RenderFlagMask = 0;
	g_RenderFlagNeg = 0;
	g_RenderPass = 0;
	g_StencilSegs = 0;
	g_StencilInv = 0;
	g_StencilGrupos = 0;
}

void SombrasStencil_Capturar(byte on) {
	if (!g_RenderOff && g_SombrasStencil) {
		g_StencilMultFrame = g_StencilMult;
		g_StencilCapturar = on != 0;
	}
}

void E3D_SetShadowLight(CLuz *l, int unused) {
	vec3_t d, n;

	(void)unused;
	g_ShadowLight = NULL;
	if (!g_SombrasStencil || !g_StencilCapturar || !l) {
		return;
	}
	g_ShadowLightTipo = l->tipo;
	g_ShadowLight = l;
	if (l->tipo == LUZ_DIRECCIONAL) {
		/* far away against the direction */
		d = l->d3d.Direction;
		const vec3_t *u = Vec3_Normalize(&n, &d);
		g_ShadowLightPos = (vec3_t){ -u->x * 2000000.0f, -u->y * 2000000.0f, -u->z * 2000000.0f };
		d = l->d3d.Direction;
		u = Vec3_Normalize(&n, &d);
		g_ShadowRange = 2500000.0f;
		g_ShadowCos = 0.0f;
		g_ShadowLightDir = *u;
		return;
	}
	g_ShadowLightPos = l->d3d.Position;
	d = l->d3d.Direction;
	g_ShadowLightDir = *Vec3_Normalize(&n, &d);
	g_ShadowRange = l->rango == 0.0f ? 1000.0f : l->rango * 1.1f;
	g_ShadowCos = (float)Game_Cos((double)l->phi * 0.5 * 3.141592653589793 / 180.0);
}

void Stencil_Free(void) {
	g_ShadowLight = NULL;
	if (g_StencilBlock) {
		operator_delete_sized(g_StencilBlock, sizeof(StencilBlock));
		g_StencilBlock = NULL;
	}
	if (g_StencilSegmentos) {
		operator_delete_sized(g_StencilSegmentos, 0xc);
		g_StencilSegmentos = NULL;
	}
	if (g_StencilTriangInv) {
		operator_delete_sized(g_StencilTriangInv, 0xc);
		g_StencilTriangInv = NULL;
	}
	if (g_StencilVB3) {
		COM_RELEASE(g_StencilVB3);
		g_StencilVB3 = NULL;
	}
	if (g_StencilVB2) {
		COM_RELEASE(g_StencilVB2);
		g_StencilVB2 = NULL;
	}
	if (g_StencilVB) {
		COM_RELEASE(g_StencilVB);
		g_StencilVB = NULL;
	}
	if (g_StencilIB) {
		COM_RELEASE(g_StencilIB);
		g_StencilIB = NULL;
	}
}

void Stencil_GetLightPos(vec3_t *out) {
	CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
	*out = g_ShadowLightPos;
}

void Stencil_Init(void) {
	const char *warn = NULL;
	byte ok, on;

	if (!Render_StencilBits()) {
		warn = Game_Ptr(0x3dc684);	/* "No stencil bits\n" */
	} else if (Render_StencilBits() < 4) {
		warn = Game_Ptr(0x3dc698);	/* "Not enought stencil bits\n" */
	}
	if (warn) {
		Con_Warnf("%s", warn);
		g_StencilShadows = -1;
		ok = 0;
		on = 0;
	} else {
		ok = 1;
		if (g_StencilShadows < 0) {
			g_StencilShadows = 1;
			on = 1;
		} else {
			on = g_StencilShadows > 0;
		}
	}
	SombrasStencil_Set(on);
	if (!ok) {
		return;
	}
	CHECK(!g_StencilBlock);
	g_StencilBlock = operator_new_array(STENCIL_MAX_GRUPOS * sizeof(StencilBlock));
	CHECK(!g_StencilSegmentos);
	g_StencilSegmentos = operator_new_array(0x30000);
	CHECK(!g_StencilTriangInv);
	g_StencilTriangInv = operator_new_array(0x6c000);
	/* two triangles per quad of 4 vertices */
	uint16_t *idx = operator_new_array(0x18000);
	for (int i = 0; i < 0x2000; i++) {
		uint16_t v = (uint16_t)(i * 4);
		idx[i * 6] = v;
		idx[i * 6 + 1] = v + 1;
		idx[i * 6 + 2] = v + 2;
		idx[i * 6 + 3] = v + 1;
		idx[i * 6 + 4] = v + 3;
		idx[i * 6 + 5] = v + 2;
	}
	Render_CreateIB(&g_StencilIB, idx, 0x4000, 0);
	if (idx) {
		operator_delete_sized(idx, 2);
	}
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dCreateVB_t, D3DDEV_CREATEVB)(dev, 0x60000, 0x208, 2, 0, &g_StencilVB);
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dCreateVB_t, D3DDEV_CREATEVB)(dev, 0x6c000, 0x208, 2, 0, &g_StencilVB2);
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dCreateVB_t, D3DDEV_CREATEVB)(dev, 0x6c000, 0x208, 2, 0, &g_StencilVB3);
}

/* the x87 division of the original (fdivr QWORD) in the precision the game set, stored as a double */
static double X87_DivToDouble(double num, float den) {
	long double r;

	__asm__("fdivrl %2" : "=t"(r) : "0"((long double)den), "m"(num));
	return (double)r;
}

/* the ticks of a profiler section averaged over 20 frames, in seconds */
static double Stencil_TscTime(int section) {
	double t = Int64_ToDouble(Prof_Avg(section, 0x14));
	return X87_DivToDouble(t, Tsc_CpuFreq(0));
}

/* the scene drawn: unlit where the stencil is set, then lit where it is not, the floor through g_StencilRender */
static void Stencil_DrawScene(void) {
	if (g_StencilRender) {
		g_RenderPass = 2;
		StencilBlock_SetLuces(0);
		((void (*)(void))g_StencilRender)();
		if (g_StencilSelf) {
			E3D_FlushBatch();
			Stencil_RS(RS_STENCILREF, g_StencilRefModels);
			Modelos_Render();
		}
		E3D_FlushBatch();
		StencilBlock_SetLuces(1);
		Stencil_RS(RS_STENCILENABLE, 0);
		g_RenderPass = 3;
		g_RenderFlagMask = 0x40;
		g_RenderFlagNeg = 1;
		((void (*)(void))g_StencilRender)();
		Modelos_Render();
		E3D_FlushBatch();
		g_RenderFlagMask = 0x40;
		g_RenderFlagNeg = 0;
		g_RenderPass = 1;
		g_StencilRender = NULL;
		return;
	}
	StencilBlock_SetLuces(0);
	Modelos_Render();
	E3D_FlushBatch();
	StencilBlock_SetLuces(1);
	Stencil_RS(RS_STENCILENABLE, 0);
	g_RenderFlagMask = 0x40;
	g_RenderFlagNeg = 1;
	Modelos_Render();
	E3D_FlushBatch();
	g_RenderFlagMask = 0x40;
	g_RenderFlagNeg = 0;
	g_RenderClaseMask = 0x58ffff1f;
	Modelos_Render();
	E3D_FlushBatch();
}

void SombrasStencil_Render(void) {
	if (g_RenderOff) {
		return;
	}
	if (g_SombrasStencil) {
		g_StencilCapturar = 0;
		g_StencilMultFrame = g_StencilMult;
	}
	if (g_KeyV && g_KeyX) {
		g_KeyV = 0;
		SombrasStencil_Set(g_SombrasStencil == 0);
	}
	if (g_StencilTog) {
		g_StencilTog = 0;
		SombrasStencil_Set(g_SombrasStencil == 0);
	}
	if (!g_SombrasStencil) {
		return;
	}
	Render_SetWorldMatrix(g_MatIdentity, 0);
	g_StencilMat->params.b39 = 0;
	g_StencilMat->params.i40 = (g_ShowStencil != 2) + 4;
	Render_SetMaterial(g_StencilMat, 0);
	/* the volumes: front faces count up, back faces down */
	Stencil_RS(RS_ZWRITEENABLE, 0);
	Stencil_RS(RS_STENCILENABLE, g_ShowStencil != 2);
	Stencil_RS(RS_SHADEMODE, 1);
	Stencil_RS(RS_STENCILFUNC, 8);
	Stencil_RS(RS_STENCILZFAIL, 1);
	Stencil_RS(RS_STENCILFAIL, 1);
	Stencil_RS(RS_STENCILREF, 1);
	Stencil_RS(RS_STENCILMASK, 0xffffffff);
	Stencil_RS(RS_STENCILWRITEMASK, 0xffffffff);
	Stencil_RS(RS_STENCILPASS, 8);
	Stencil_RS(RS_COLORWRITEENABLE, g_ShowStencil == 2 ? 0xf : 0);
	Stencil_RS(RS_ALPHABLENDENABLE, 1);
	Stencil_RS(RS_SRCBLEND, (g_ShowStencil == 2) + 1);
	Stencil_RS(RS_DESTBLEND, 2);
	Stencil_RS(RS_CULLMODE, 3);
	Stencil_DrawSegs(0);
	Stencil_DrawTris(0);
	Stencil_RS(RS_CULLMODE, 2);
	Stencil_RS(RS_STENCILPASS, 7);
	Stencil_DrawSegs(1);
	Stencil_DrawTris(1);
	Stencil_RS(RS_SHADEMODE, 2);
	Stencil_RS(RS_CULLMODE, 3);
	Stencil_RS(RS_ZWRITEENABLE, 1);
	Stencil_RS(RS_STENCILENABLE, 0);
	Stencil_RS(RS_ALPHABLENDENABLE, 0);
	Stencil_RS(RS_COLORWRITEENABLE, 0xf);
	Render_SetCull(3);
	Stencil_RS(RS_STENCILENABLE, 1);
	Stencil_RS(RS_STENCILREF, g_StencilRefWorld);
	Stencil_RS(RS_STENCILFUNC, 4);
	Stencil_RS(RS_STENCILPASS, 2);
	CHECK(!g_StencilCapturar);
	Stencil_DrawScene();
	g_RenderClaseMask = 0xa70000e0;
	Stencil_RS(RS_STENCILENABLE, 0);
	CHECK(!g_StencilCapturar);
	if (g_ProfMode == 2) {
		Font_PrintfXY(g_FntComic14, 10.0f, 120.0f, "grupos: %d", g_StencilGrupos);
		Font_PrintfXY(g_FntComic14, 10.0f, 140.0f, "segs: %d", g_StencilSegs);
		Font_PrintfXY(g_FntComic14, 10.0f, 160.0f, "inv: %d", g_StencilInv);
	}
	if (g_ShowStencil) {
		double t5 = Stencil_TscTime(5);
		double t6 = Stencil_TscTime(6);
		Font_PrintfXY(g_FntTerm8, 10.0f, 180.0f, "tsc(5): %f ms  %f%%", t5 * 1000.0, t5 / (double)g_FrameMs * 100.0);
		Font_PrintfXY(g_FntTerm8, 10.0f, 190.0f, "tsc(6): %f ms  %f%%", t6 * 1000.0, t6 / (double)g_FrameMs * 100.0);
	}
	g_StencilSegs = 0;
	g_StencilInv = 0;
	g_StencilGrupos = 0;
	g_ShadowLight = NULL;
}
