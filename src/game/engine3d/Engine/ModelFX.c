/*
 * Engine/ModelFX.cpp of the engine3d: the render effects of a model (CModel f10c, its parameter the float in f110),
 * drawn in the normal passes as several draws of the scene with forced materials. See the ModelFX part of
 * include/scrap/model.h.
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/global.h"
#include "scrap/graphic2d.h"
#include "scrap/material.h"
#include "scrap/model.h"
#include "scrap/flare.h"
#include "scrap/render.h"

/* the parameter of the effect, 0..1 */
static float ModelFX_Param(const CModel *m) {
	float f;
	memcpy(&f, &m->f110, sizeof(f));
	return f;
}

/* 0..255 of a float times 256 */
static int Byte_Clamp(int v) {
	return v <= 0 ? 0 : v < 0xff ? v : 0xff;
}

static int ModelFX_Alpha(const CModel *m) {
	return Byte_Clamp((int)(ModelFX_Param(m) * 256.0f));
}

/* 0..3 times step, one in 8 times; 0 else */
static int Rand_Flicker(int shift) {
	return (crt_rand() & 7) == 0 ? (crt_rand() & 3) << shift : 0;
}

/* a random number 0..1 of the exe's rand times k, through double */
static double Rand_Unit(double k) {
	return (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0) * k;
}

static void Scene_Draw(const CModel *m, NodeAnimCtx *ctx) {
	CScene_Render(m->scene, ctx);
}

/* the blend states of the shared effect material */
static void FXMat_Set(int src, int dst, byte lighting, int zFunc, byte zWrite) {
	g_FXMat.params.srcBlend = src;
	g_FXMat.params.dstBlend = dst;
	g_FXMat.params.lighting = lighting;
	g_FXMat.params.transp = 1;
	g_FXMat.params.zFunc = zFunc;
	g_FXMat.params.fog = 0;
	g_FXMat.params.zWrite = zWrite;
}

static void FXMat_Colors(uint32_t ambient, uint32_t diffuse, uint32_t emissive) {
	memcpy(g_FXMat.params.ambient, &ambient, 4);
	memcpy(g_FXMat.params.diffuse, &diffuse, 4);
	memcpy(g_FXMat.params.emissive, &emissive, 4);
}

/* the gray of the static noise: 0x20 plus a flicker */
static uint32_t FX_Gray(void) {
	byte e = (byte)(Rand_Flicker(4) + 0x20);
	return (uint32_t)e | (uint32_t)e << 8 | (uint32_t)e << 16;
}

/* the model drawn with the material forced into every draw */
static void ModelFX_Forced(const CModel *m, NodeAnimCtx *ctx, int zFunc) {
	Render_ClearForced();
	g_MatFX1 = &g_FXMat;
	FXMat_Set(1, 2, 0, zFunc, 1);
	Scene_Draw(m, ctx);
}

/* one in 64 frames the scene is stretched along x for a moment (the picture jumps) */
static void ModelFX_Jump(const CModel *m, NodeAnimCtx *ctx) {
	if (crt_rand() & 0x3f) {
		return;
	}
	matrix_t *w = &ctx->m;
	float x = w->m[12], y = w->m[13], z = w->m[14];
	w->m[12] = 0.0f;
	w->m[13] = 0.0f;
	w->m[14] = 0.0f;
	vec3_t s;
	s.x = (float)(Rand_Unit((double)0.6f) + (double)0.9f);
	s.y = 0.9f;
	s.z = 1.0f;
	Matrix_PostScale(w, &s);
	w->m[12] = x;
	w->m[13] = y;
	w->m[14] = z;
	CScene_Update(m->scene, ctx);
}

/* the scan lines over the screen and the noise texture g at random places */
static void ModelFX_Overlay(CGraphic2D *g) {
	CGraphic2D *lines = &g_FXGraphicStatic;
	lines->x = 0.0f;
	float t = g_TimeMsF * 0.0001f;
	lines->y = (t - (float)(int)t) * 544.0f - 64.0f;
	lines->color = (uint32_t)(-0x80 - (crt_rand() & 0x3f)) << 24 | 0xd2d2ff;
	lines->scale[0] = 640.0f;
	lines->scale[1] = 64.0f - (float)(Rand_Unit(16.0) + 0.0);
	lines->u0 = 0.0f;
	lines->v0 = 0.0f;
	lines->u1 = 1.0f;
	lines->v1 = 1.0f;
	CGraphic2D_Render(lines);
	g->x = 0.0f;
	g->y = 0.0f;
	g->scale[0] = 80.0f;
	g->scale[1] = 60.0f;
	g->color = 0x80ffffff;
	float a = (float)(Rand_Unit(8.0) + 0.0);
	float b = (float)(Rand_Unit(8.0) + 0.0);
	g->u0 = b;
	g->v0 = a;
	g->u1 = b + 8.0f;
	g->v1 = a + 8.0f;
	CGraphic2D_Render(g);
	Render_Clear(NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
}

void ModelFX_Static(CModel *m, NodeAnimCtx *ctx) {
	ModelFX_Jump(m, ctx);
	Render_ClearForced();
	FXMat_Colors(0x802020, 0x801010, 0);
	g_MatFX1 = &g_FXMat;
	FXMat_Set(2, 2, 1, 8, 0);
	g_FXMat.params.diffuse[3] = (byte)Rand_Flicker(3);
	uint32_t e = FX_Gray();
	memcpy(g_FXMat.params.emissive, &e, 4);
	CMaterial_ScaleColors(&g_FXMat, 0.5f);
	Scene_Draw(m, ctx);
	Render_Clear(NULL, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
	ModelFX_Forced(m, ctx, 8);
	Render_ClearForced();
	ModelFX_Overlay(&g_FXGraphicNoise2);
}

/* not used: ModelFX_Static in orange over a gray pass */
void ModelFX_Static2(CModel *m, NodeAnimCtx *ctx) {
	ModelFX_Jump(m, ctx);
	ModelFX_Forced(m, ctx, 4);
	Render_ClearForced();
	FXMat_Colors(0x808080, 0xff5040, 0);
	g_MatFX1 = &g_FXMat;
	FXMat_Set(6, 2, 1, 4, 1);
	g_FXMat.params.diffuse[3] = (byte)Rand_Flicker(4);
	uint32_t e = FX_Gray();
	memcpy(g_FXMat.params.emissive, &e, 4);
	CMaterial_ScaleColors(&g_FXMat, 1.0f);
	Scene_Draw(m, ctx);
	Render_Clear(NULL, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
	ModelFX_Forced(m, ctx, 8);
	Render_ClearForced();
	ModelFX_Overlay(&g_FXGraphicNoise1);
}

void ModelFX_Base(CModel *m, NodeAnimCtx *ctx) {
	ModelFX_Forced(m, ctx, 4);
}

/* the model again with the forced material, the classes masked */
static void ModelFX_Again(const CModel *m, NodeAnimCtx *ctx) {
	Scene_Draw(m, ctx);
	Render_ClearForced();
	g_RenderClassMask = 0xa700ff1f;
	Scene_Draw(m, ctx);
}

/* fading out: below 0.5 drawn as it is, lit models fade through the forced material */
static void ModelFX_FadeOut(CModel *m, NodeAnimCtx *ctx, byte base) {
	float v = ModelFX_Param(m);
	if (v >= 1.0f) {
		return;
	}
	if (!m->dynLight) {
		if (0.5f > v) {
			Scene_Draw(m, ctx);
		}
		return;
	}
	if (base) {
		ModelFX_Base(m, ctx);
	}
	Render_ClearForced();
	g_MatForced = &g_FXMat;
	g_FXMat.params.srcBlend = 5;
	g_FXMat.params.dstBlend = 6;
	g_FXMat.params.diffuse[3] = (byte)Byte_Clamp((int)((1.0 - (double)v) * 256.0));
	CMaterial_ScaleColors(&g_FXMat, 1.0f);
	ModelFX_Again(m, ctx);
}

void ModelFX_FadeLit(CModel *m, NodeAnimCtx *ctx) {
	ModelFX_FadeOut(m, ctx, 1);
}

void ModelFX_Fade(CModel *m, NodeAnimCtx *ctx) {
	ModelFX_FadeOut(m, ctx, 0);
}

void ModelFX_Wire(CModel *m, NodeAnimCtx *ctx) {
	uint32_t fill;
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dGetRenderState_t, D3DDEV_GETRENDERSTATE)(dev, D3DRS_FILLMODE, &fill);
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FILLMODE, 2);
	ModelFX_Static(m, ctx);
	Render_ClearForced();
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FILLMODE, fill);
}

void ModelFX_Glow(CModel *m, NodeAnimCtx *ctx) {
	int a = ModelFX_Alpha(m);
	Scene_Draw(m, ctx);
	Render_ClearForced();
	FXMat_Colors(0, 0xff00, 0xffffff);
	g_MatFX1 = &g_FXMat;
	FXMat_Set(5, 3, 1, 4, 0);
	g_FXMat.params.diffuse[3] = (byte)a;
	CMaterial_ScaleColors(&g_FXMat, 1.0f);
	Render_ClearForced();
	g_RenderClassMask = 0xa700ff1f;
	Scene_Draw(m, ctx);
}

/* the model, then again with the shader material mat at the gray of the parameter */
static void ModelFX_Shader(CModel *m, NodeAnimCtx *ctx, CMaterial *mat) {
	Scene_Draw(m, ctx);
	Render_ClearForced();
	CHECK(mat);
	g_MatFX1 = mat;
	byte a = (byte)ModelFX_Alpha(m);
	mat->params.diffuse[0] = a;
	mat->params.diffuse[1] = a;
	mat->params.diffuse[2] = a;
	mat->params.diffuse[3] = a;
	ModelFX_Again(m, ctx);
}

void ModelFX_Electric(CModel *m, NodeAnimCtx *ctx) {
	ModelFX_Shader(m, ctx, g_MatElectric);
}

void ModelFX_Fire(CModel *m, NodeAnimCtx *ctx) {
	ModelFX_Shader(m, ctx, g_MatFire);
}

void ModelFX_Noise(CModel *m, NodeAnimCtx *ctx) {
	int base = (int)(ModelFX_Param(m) * 256.0f);
	int a;
	if (Rand_Flicker(4) + base <= 0) {
		a = 0;
	} else if (Rand_Flicker(4) + base >= 0xff) {
		a = 0xff;
	} else {
		a = base + Rand_Flicker(4);
	}
	CMaterial *mat = g_MatNoise;
	CHECK(mat);
	ModelFX_Forced(m, ctx, 4);
	Render_ClearForced();
	g_MatForced = mat;
	mat->params.srcBlend = 5;
	mat->params.dstBlend = 6;
	mat->params.diffuse[3] = (byte)Byte_Clamp(0xff - a);
	CMaterial_ScaleColors(mat, 1.0f);
	Scene_Draw(m, ctx);
	Render_ClearForced();
	g_MatFX1 = mat;
	float v = ModelFX_Param(m);
	float f = v > 0.5f ? 0.5f - (v - 0.5f) : v;
	f = f + f;
	mat->params.diffuse[0] = (byte)(int)((float)(crt_rand() % 16 + 0x5c) * f);
	mat->params.diffuse[1] = (byte)(int)((float)(crt_rand() % 16 + 0x5c) * f);
	int c = crt_rand() % 32;
	mat->params.srcBlend = 2;
	mat->params.dstBlend = 2;
	mat->params.diffuse[2] = (byte)(int)((float)(c + 0xc0) * f);
	mat->params.diffuse[3] = (byte)(int)(Float_MinSS(1.0f, f * 3.0f) * 255.0f);
	ModelFX_Again(m, ctx);
}

void __thiscall ModelFX_Render(CModel *self, NodeAnimCtx *ctx) {
	if (!g_PSOn && self->f10c >= 6 && self->f10c <= 8) {
		/* the shader effects need the pixel shaders */
		Scene_Draw(self, ctx);
		return;
	}
	int pass = g_RenderPass;
	if (pass != 0 && pass != 3 && pass != 0xd) {
		if (self->f10c == 6 || self->f10c == 7) {
			Scene_Draw(self, ctx);
		}
		return;
	}
	Render_ClearForced();
	uint32_t class = g_RenderClassMask, flags = g_RenderFlagMask;
	byte neg = g_RenderFlagNeg;
	pass = g_RenderPass;
	g_RenderFlagMask = 0;
	g_RenderFlagNeg = 0;
	g_RenderClassMask = 0xa70000e0;
	g_RenderPass = 0;
	switch (self->f10c) {
	case 1:
		ModelFX_Static(self, ctx);
		break;
	case 2:
		ModelFX_FadeLit(self, ctx);
		break;
	case 3:
		ModelFX_Fade(self, ctx);
		break;
	case 4:
		ModelFX_Wire(self, ctx);
		break;
	case 5:
		ModelFX_Glow(self, ctx);
		break;
	case 6:
		ModelFX_Electric(self, ctx);
		break;
	case 7:
		ModelFX_Fire(self, ctx);
		break;
	case 8:
		ModelFX_Noise(self, ctx);
		break;
	default:
		CHECK(0);
		break;
	}
	Render_ClearForced();
	g_RenderFlagMask = flags;
	g_RenderClassMask = class;
	g_RenderFlagNeg = neg;
	g_RenderPass = pass;
}

/* a shader material of the effects: additive, transparent, from the bump, glow and mask maps */
static CMaterial *FXMat_Shader(const char *name, const char *bump, const char *glow, const char *mask) {
	CMaterial m;
	CMaterial_ctor(&m);
	String_Set(&m.Name, name);
	m.params.srcBlend = 2;
	m.params.attrib |= MAT_SHADER;
	m.params.dstBlend = 2;
	m.params.lighting = 0;
	m.params.transp = 1;
	m.params.zFunc = 4;
	m.params.fog = 0;
	m.params.zWrite = 0;
	CMaterial_SetMap(&m, MAP_DIFFUSE, bump);
	CMaterial_SetMap(&m, MAP_GLOW, glow);
	CMaterial_SetMap(&m, MAP_BUMP, mask);
	CMaterial_Prepare(&m, g_pd3dDevice);
	CMaterial *r = Mat_New(&m, 1);
	CMaterial_dtor(&m);
	return r;
}

/* a 2D overlay of the whole screen */
static void FXGraphic_Create(CGraphic2D *g, const char *tex, int src, int dst, uint32_t flags) {
	g->screen = 1;
	g->screenSize = 1;
	g->noTexture = 1;
	CGraphic2D_CreateMat(g, tex, NULL, src, dst, 1, flags);
}

void ModelFX_InitD3D(void) {
	FXGraphic_Create(&g_FXGraphicStatic, "bmp/noise_static.alpha.tga", 5, 6, 0x10);
	FXGraphic_Create(&g_FXGraphicNoise1, "bmp/noise.tga", 1, 3, 0x30);
	FXGraphic_Create(&g_FXGraphicNoise2, "bmp/noise2.tga", 2, 2, 0x30);
	g_MatElectric = FXMat_Shader("fx_Electric (+shader:Electric)", "bmp/Electric.Bump.bmp", "bmp/Electric.bmp",
		"bmp/Electric.Mask.bmp");
	g_MatFire = FXMat_Shader("fx_Fire (+shader:Fire)", "bmp/Fire.Bump.bmp", "bmp/Fire.bmp", "bmp/Fire.Mask.bmp");
	CMaterial m;
	CMaterial_ctor(&m);
	String_Set(&m.Name, "fx_Noise (+shader:Noise)");
	m.params.attrib |= MAT_SHADER;
	m.params.srcBlend = 5;
	m.params.dstBlend = 6;
	m.params.lighting = 0;
	m.params.transp = 1;
	m.params.zFunc = 4;
	m.params.fog = 0;
	m.params.zWrite = 0;
	CMaterial_SetMap(&m, MAP_DIFFUSE, "bmp/noise2.tga");
	m.map[MAP_DIFFUSE]->filter = 0;
	CMaterial_Prepare(&m, g_pd3dDevice);
	g_MatNoise = Mat_New(&m, 1);
	CMaterial_dtor(&m);
}
