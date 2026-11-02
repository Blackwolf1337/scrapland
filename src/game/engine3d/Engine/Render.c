/*
 * Engine/render.cpp of the engine3d, the decompiled part: see include/scrap/render.h.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/archio.h"
#include "scrap/camera.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/scene.h"
#include "scrap/graphic2d.h"
#include "scrap/vertlist.h"
#include "scrap/meshd3d.h"
#include "scrap/model.h"
#include "scrap/material.h"
#include "scrap/fog.h"
#include "scrap/node3d.h"
#include "scrap/texture.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"

static void Render_TSS(uint32_t stage, uint32_t type, uint32_t value) {
	void *dev = g_pd3dDevice;
	Rd_DevSetTextureStageState(dev, stage, type, value);
}

static void Render_State(uint32_t state, uint32_t value) {
	void *dev = g_pd3dDevice;
	Rd_DevSetRenderState(dev, state, value);
}

/* pixel shader i of g_PSNames when the device has it and it is not switched off in g_PSOff, else 0 */
static uint32_t Render_PSHandle(int i) {
	if (!g_PSOn || !g_PSHandles[i] || g_PSOff & 1u << i) {
		return 0;
	}
	return g_PSHandles[i];
}

static void Render_SetPS(uint32_t handle) {
	void *dev = g_pd3dDevice;
	Rd_DevSetPixelShader(dev, handle);
}

static void Render_PS(int i, void *arg, byte flags) {
	uint32_t ps = Render_PSHandle(PS_POST + i);
	if (!ps) {
		Render_PSFixed(arg, flags);
		return;
	}
	if (flags & 1) {
		Render_SetPS(ps);
	}
	if (flags & 2) {
		Render_SetPS(0);
	}
}

void Render_PS0(void *arg, byte flags) {
	Render_PS(0, arg, flags);
}

void Render_PS1(void *arg, byte flags) {
	Render_PS(1, arg, flags);
}

void Render_PS2(void *arg, byte flags) {
	Render_PS(2, arg, flags);
}

void Render_PS3(void *arg, byte flags) {
	Render_PS(3, arg, flags);
}

void Render_PS4(void *arg, byte flags) {
	Render_PS(4, arg, flags);
}

void Render_PS5(void *arg, byte flags) {
	Render_PS(5, arg, flags);
}

void Render_PS6(void *arg, byte flags) {
	Render_PS(6, arg, flags);
}

void Render_PS7(void *arg, byte flags) {
	Render_PS(7, arg, flags);
}

void Render_PS8(void *arg, byte flags) {
	Render_PS(8, arg, flags);
}

static void Stage_Color(uint32_t stage, uint32_t arg1, uint32_t arg2, uint32_t op) {
	Render_TSS(stage, D3DTSS_COLORARG1, arg1);
	Render_TSS(stage, D3DTSS_COLORARG2, arg2);
	Render_TSS(stage, D3DTSS_COLOROP, op);
}

static void Stage_Alpha(uint32_t stage, uint32_t arg1, uint32_t arg2, uint32_t op) {
	Render_TSS(stage, D3DTSS_ALPHAARG1, arg1);
	Render_TSS(stage, D3DTSS_ALPHAARG2, arg2);
	Render_TSS(stage, D3DTSS_ALPHAOP, op);
}

static uint32_t Op_Diffuse(void) {
	return g_DiffuseMod4x ? D3DTOP_MODULATE4X : D3DTOP_MODULATE;
}

static void Stage_Off(uint32_t stage) {
	Stage_Color(stage, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_DISABLE);
	Stage_Alpha(stage, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_DISABLE);
}

/* the stage 0 of the shader modes: the texture, its alpha */
static void Stage_Base(void) {
	Stage_Color(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
	Stage_Alpha(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
}

/* the reflection texture added by the alpha so far (R_EnvBlend 0) or blended by it; the alpha kept */
static void Stage_EnvBlend(uint32_t stage) {
	if (!g_EnvBlend) {
		Stage_Color(stage, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_MODULATEALPHA_ADDCOLOR);
	} else {
		Stage_Color(stage, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_BLENDCURRENTALPHA);
	}
	Stage_Alpha(stage, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_SELECTARG2);
}

/* the color so far added to (ADD), times the diffuse or the lightmap; the alpha kept */
static void Stage_Next(uint32_t stage, uint32_t arg1, uint32_t op) {
	Stage_Color(stage, arg1, D3DTA_CURRENT, op);
	Stage_Alpha(stage, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_SELECTARG2);
}

/* the stage 0 back to texture times diffuse */
static void Stage_End(void) {
	Stage_Color(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_MODULATE);
}

void Render_PSFixed(void *arg, byte flags) {
	(void)arg;
	if (flags & 1) {
		Stage_Color(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, Op_Diffuse());
	}
	if (flags & 2) {
		Stage_End();
	}
}

/* the shader modes (g_ShaderTab, g_ShaderTab2 with the lightmap): a pixel shader with its constants, the fixed
 * texture stages without one. Flags 1 begin, 4 the next material of the same mode, 2 end; the end takes the
 * textures of the stages beyond 0 away. */
#define LIGHTMAP(stage) Render_SetMap(stage, g_LightmapMat->map[0])

void Shader_DiffuseNoLit(CMaterial *mat, uint32_t flags) {
	(void)mat;
	if (flags & 1) {
		Stage_Color(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
	}
	if (flags & 2) {
		Stage_End();
	}
}

void Shader_DiffuseLightmap(CMaterial *mat, uint32_t flags) {
	(void)mat;
	if (flags & 1) {
		Stage_Base();
		Stage_Next(1, D3DTA_TEXTURE, g_LightmapOp);
	}
	if (flags & 5) {
		LIGHTMAP(1);
	}
	if (flags & 2) {
		Stage_End();
		Stage_Off(1);
		Render_SetMap(1, NULL);
	}
}

/* Envmap, MaskEnvmap: the reflection in stage 1 (added, or blended by R_EnvBlend), the diffuse and the lightmap
 * after it */
static void Shader_Env(CMaterial *mat, uint32_t flags, int ps, byte mask, byte lightmap) {
	uint32_t h = Render_PSHandle(ps);
	Mat_StageEnvMap(mat, flags, 1, 0.0f);
	if (!h) {
		if (flags & 1) {
			if (mask) {
				if (lightmap) {
					Stage_Base();
				} else {
					Stage_Alpha(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
				}
				Stage_EnvBlend(1);
				if (!lightmap) {
					Stage_Color(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
				}
			} else {
				Stage_Base();
				Stage_Next(1, D3DTA_TEXTURE, D3DTOP_ADD);
			}
			if (lightmap) {
				Stage_Next(2, D3DTA_TEXTURE, g_LightmapOp);
				Render_TSS(2, D3DTSS_TEXCOORDINDEX, 1);
			} else {
				Stage_Next(2, D3DTA_DIFFUSE, Op_Diffuse());
			}
		}
	} else if (flags & 1) {
		Render_SetPS(h);
		if (lightmap) {
			Render_TSS(2, D3DTSS_TEXCOORDINDEX, 1);
		}
	}
	if (flags & 5) {
		Render_SetMap(1, mat->map[MAP_ENVIRONMENT]);
		if (lightmap) {
			LIGHTMAP(2);
		}
	}
	if (!(flags & 2)) {
		return;
	}
	if (!h) {
		Stage_End();
		Stage_Off(1);
		Stage_Off(2);
	} else {
		Render_SetPS(0);
	}
	if (lightmap) {
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 2);
	}
	Render_SetMap(1, NULL);
	if (lightmap) {
		Render_SetMap(2, NULL);
	}
}

void Shader_Envmap(CMaterial *mat, uint32_t flags) {
	Shader_Env(mat, flags, PS_ENVMAP, 0, 0);
}

void Shader_EnvmapLightmap(CMaterial *mat, uint32_t flags) {
	Shader_Env(mat, flags, PS_ENVMAP_LM, 0, 1);
}

void Shader_MaskEnvmap(CMaterial *mat, uint32_t flags) {
	Shader_Env(mat, flags, PS_MASKENVMAP, 1, 0);
}

void Shader_MaskEnvmapLightmap(CMaterial *mat, uint32_t flags) {
	Shader_Env(mat, flags, PS_MASKENVMAP_LM, 1, 1);
}

/* MaskEnvBump: the bump map (map 3) in stage 1 moves the reflection of stage 2 */
static void Shader_EnvBump(CMaterial *mat, uint32_t flags, int ps, byte lightmap) {
	uint32_t h = Render_PSHandle(ps);
	Mat_StageEnvBump(mat, flags, 2, h ? 2 : 1);
	if (!h) {
		if (flags & 1) {
			Stage_Base();
			Stage_Next(1, D3DTA_TEXTURE, D3DTOP_BUMPENVMAP);
			Stage_EnvBlend(2);
			if (lightmap) {
				Stage_Next(3, D3DTA_TEXTURE, g_LightmapOp);
			} else {
				Stage_Next(3, D3DTA_DIFFUSE, Op_Diffuse());
			}
		}
	} else if (flags & 1) {
		Render_SetPS(h);
	}
	if (flags & 1) {
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
		if (lightmap) {
			Render_TSS(3, D3DTSS_TEXCOORDINDEX, 1);
		}
	}
	if (flags & 5) {
		Render_SetMap(1, mat->map[MAP_BUMP]);
		Render_SetMap(2, mat->map[MAP_ENVIRONMENT]);
		if (lightmap) {
			LIGHTMAP(3);
		}
	}
	if (!(flags & 2)) {
		return;
	}
	if (!h) {
		Stage_End();
		Stage_Off(1);
		Stage_Off(2);
		Stage_Off(3);
	} else {
		Render_SetPS(0);
	}
	Render_TSS(1, D3DTSS_TEXCOORDINDEX, 1);
	if (lightmap) {
		Render_TSS(3, D3DTSS_TEXCOORDINDEX, 3);
	}
	Render_SetMap(1, NULL);
	Render_SetMap(2, NULL);
	if (lightmap) {
		Render_SetMap(3, NULL);
	}
}

void Shader_MaskEnvBump(CMaterial *mat, uint32_t flags) {
	Shader_EnvBump(mat, flags, PS_MASKENVBUMP, 0);
}

void Shader_MaskEnvBumpLightmap(CMaterial *mat, uint32_t flags) {
	Shader_EnvBump(mat, flags, PS_MASKENVBUMP_LM, 1);
}

/* Glowmap: the glow map (map 4) added in the last stage; the pixel shaders take the alpha constant */
void Shader_Glowmap(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_GLOWMAP);
	if (flags & 1) {
		if (!h) {
			Stage_Color(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, Op_Diffuse());
			Stage_Alpha(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
			Stage_Next(1, D3DTA_TEXTURE, D3DTOP_ADD);
		} else {
			Render_SetPS(h);
		}
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		if (h) {
			Render_PSConstAlpha(mat, 0);
		}
		Render_SetMap(1, mat->map[MAP_GLOW]);
	}
	if (!(flags & 2)) {
		return;
	}
	if (!h) {
		Stage_End();
		Stage_Off(1);
	} else {
		Render_SetPS(0);
	}
	Render_TSS(1, D3DTSS_TEXCOORDINDEX, 1);
	Render_SetMap(1, NULL);
}

/* without the pixel shader the glow is left out */
void Shader_GlowmapLightmap(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_GLOWMAP_LM);
	if (!h) {
		Shader_DiffuseLightmap(mat, flags);
		return;
	}
	if (flags & 1) {
		Render_SetPS(h);
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		Render_PSConstAlpha(mat, 0);
		LIGHTMAP(1);
		Render_SetMap(2, mat->map[MAP_GLOW]);
	}
	if (flags & 2) {
		Render_SetPS(0);
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 2);
		Render_SetMap(1, NULL);
		Render_SetMap(2, NULL);
	}
}

/* the fixed stages of GlowmapMaskEnvmap: the reflection, the diffuse or the lightmap, the glow */
static void Stage_GlowEnv(byte lightmap) {
	Stage_Base();
	Stage_EnvBlend(1);
	if (lightmap) {
		Stage_Next(2, D3DTA_TEXTURE, g_LightmapOp);
	} else {
		Stage_Next(2, D3DTA_DIFFUSE, Op_Diffuse());
	}
	Stage_Next(3, D3DTA_TEXTURE, D3DTOP_ADD);
}

void Shader_GlowmapMaskEnvmap(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_GLOWMAPMASKENVMAP);
	Mat_StageEnvMap(mat, flags, 1, 0.0f);
	/* the glow in stage 3, in stage 2 with the pixel shader */
	uint32_t glow = h ? 2 : 3;
	if (flags & 1) {
		if (!h) {
			Stage_GlowEnv(0);
		} else {
			Render_SetPS(h);
		}
		Render_TSS(glow, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		if (h) {
			Render_PSConstAlpha(mat, 0);
		}
		Render_SetMap(1, mat->map[MAP_ENVIRONMENT]);
		Render_SetMap(glow, mat->map[MAP_GLOW]);
	}
	if (flags & 2) {
		if (!h) {
			Stage_End();
			Stage_Off(1);
			Stage_Off(2);
			Stage_Off(3);
		} else {
			Render_SetPS(0);
		}
		Render_TSS(glow, D3DTSS_TEXCOORDINDEX, glow);
		Render_SetMap(1, NULL);
		Render_SetMap(glow, NULL);
	}
}

void Shader_GlowmapMaskEnvmapLightmap(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_GLOWMAPMASKENVMAP_LM);
	Mat_StageEnvMap(mat, flags, 1, 0.0f);
	if (flags & 1) {
		if (!h) {
			Stage_GlowEnv(1);
		} else {
			Render_SetPS(h);
		}
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 1);
		Render_TSS(3, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		if (h) {
			Render_PSConstAlpha(mat, 0);
		}
		Render_SetMap(1, mat->map[MAP_ENVIRONMENT]);
		LIGHTMAP(2);
		Render_SetMap(3, mat->map[MAP_GLOW]);
	}
	if (!(flags & 2)) {
		return;
	}
	if (!h) {
		Stage_End();
		Stage_Off(1);
		Stage_Off(2);
		Stage_Off(3);
	} else {
		Render_SetPS(0);
	}
	Render_TSS(2, D3DTSS_TEXCOORDINDEX, 2);
	Render_TSS(3, D3DTSS_TEXCOORDINDEX, 3);
	Render_SetMap(1, NULL);
	Render_SetMap(2, NULL);
	Render_SetMap(3, NULL);
}

/* also the lightmap mode, without the lightmap */
void Shader_GlowmapMaskEnvBump(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_GLOWMAPMASKENVBUMP);
	Mat_StageEnvBump(mat, flags, 2, h ? 2 : 1);
	/* the glow in stage 4, in stage 3 with the pixel shader */
	uint32_t glow = h ? 3 : 4;
	if (flags & 1) {
		if (!h) {
			Stage_Base();
			Stage_Next(1, D3DTA_TEXTURE, D3DTOP_BUMPENVMAP);
			Stage_EnvBlend(2);
			Stage_Next(3, D3DTA_DIFFUSE, Op_Diffuse());
			Stage_Next(4, D3DTA_TEXTURE, D3DTOP_ADD);
		} else {
			Render_SetPS(h);
		}
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
		Render_TSS(glow, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		if (h) {
			Render_PSConstAlpha(mat, 0);
		}
		Render_SetMap(1, mat->map[MAP_BUMP]);
		Render_SetMap(2, mat->map[MAP_ENVIRONMENT]);
		Render_SetMap(glow, mat->map[MAP_GLOW]);
	}
	if (flags & 2) {
		if (!h) {
			Stage_End();
			Stage_Off(1);
			Stage_Off(2);
			Stage_Off(3);
			Stage_Off(4);
		} else {
			Render_SetPS(0);
		}
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 1);
		Render_TSS(glow, D3DTSS_TEXCOORDINDEX, glow);
		Render_SetMap(1, NULL);
		Render_SetMap(2, NULL);
		Render_SetMap(glow, NULL);
	}
}

/* the reflection through the pixel shader only, the material must be transparent */
void Shader_Glass(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_GLASS);
	if (!h) {
		Render_PSFixed(mat, (byte)flags);
		return;
	}
	Mat_StageEnvMap(mat, flags, 1, 0.0f);
	if (flags & 1) {
		Render_SetPS(h);
	}
	if (flags & 5) {
		if (!mat->params.transp) {
			CheckFailed("mat->params.dif_alpha", __FILE__, __TIMESTAMP__, __LINE__);
		}
		Render_SetMap(1, mat->map[MAP_ENVIRONMENT]);
	}
	if (flags & 2) {
		Render_SetPS(0);
		Render_SetMap(1, NULL);
	}
}

/* Glowmap with the scroll of map 0 and of the glow map */
static void Shader_ScrollGlow(CMaterial *mat, uint32_t flags, uint32_t glowStage) {
	if (mat && mat->map[MAP_ENVIRONMENT]) {
		CheckFailed("!mat || !mat->obt_mapa(MAPA_ENTORNO)", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Mat_StageScroll(mat, MAP_DIFFUSE, flags, 0, 1.0f);
	Mat_StageScroll(mat, MAP_GLOW, flags, glowStage, 1.0f);
}

void Shader_Scroll(CMaterial *mat, uint32_t flags) {
	Shader_ScrollGlow(mat, flags, 1);
	Shader_Glowmap(mat, flags);
}

void Shader_ScrollLightmap(CMaterial *mat, uint32_t flags) {
	Shader_ScrollGlow(mat, flags, 2);
	Shader_GlowmapLightmap(mat, flags);
}

/* the distance of p to frustum plane i */
static float Frustum_Dist(int i, const vec3_t *p) {
	const plane_t *pl = &g_Frustum[i];
	return pl->n.x * p->x + pl->n.y * p->y + pl->n.z * p->z + pl->d;
}

int Frustum_TestSeg(const vec3_t *a, const vec3_t *b) {
	float da[MAX_PLANES_FRUSTUM], db[MAX_PLANES_FRUSTUM];
	uint32_t outA = 0, outB = 0;

	for (int i = 0; i < MAX_PLANES_FRUSTUM; i++) {
		uint32_t bit = 1u << i;
		da[i] = Frustum_Dist(i, a);
		db[i] = Frustum_Dist(i, b);
		if (da[i] > 0.0f) {
			outA |= bit;
		}
		if (db[i] > 0.0f) {
			outB |= bit;
		}
		if (outA & bit & outB) {
			return i + 1;
		}
	}
	if (!outA || !outB) {
		return 0;
	}
	/* the crossings of the planes only one end is outside of */
	uint32_t cross = outA ^ outB;
	for (int i = 0; i < MAX_PLANES_FRUSTUM; i++) {
		uint32_t bit = 1u << i;
		if (!(bit & cross)) {
			continue;
		}
		float t = da[i] / (da[i] - db[i]);
		vec3_t p = { a->x + (b->x - a->x) * t, (b->y - a->y) * t + a->y, (b->z - a->z) * t + a->z };
		if (!Frustum_PlaneOutside(&p, bit)) {
			return 0;
		}
	}
	return 10;
}

int Frustum_PlaneOutside(const vec3_t *p, uint32_t skip) {
	for (int i = 0; i < MAX_PLANES_FRUSTUM; i++) {
		if (!(skip & 1u << i) && Frustum_Dist(i, p) > 0.0f) {
			return i + 1;
		}
	}
	return 0;
}

void Frustum_TestSpheres(const vec3_t *centers, const float *radius, int n, int *out) {
	for (int k = 0; k < n; k++) {
		out[k] = 0;
		for (int i = 0; i < MAX_PLANES_FRUSTUM; i++) {
			if (Frustum_Dist(i, &centers[k]) > radius[k]) {
				out[k] = i + 1;
				break;
			}
		}
	}
}

void CRenderIB_Reset(CRenderIB *self) {
	CHECK(!self->pVertIB);
	self->Total = 0;
	self->Used = 0;
}

void CRenderVB_Reset(CRenderVB *self) {
	CHECK(!self->pVertVB);
	self->FVF = 0;
	self->Size = 0;
	self->Used = 0;
	self->Total = 0;
}

/* n bytes of src at off of the buffer */
static void Buf_Write(void *buf, uint32_t off, const void *src, uint32_t n) {
	void *p;
	CHECK(Rd_BufLock(buf, off, n, &p, 0) >= 0);
	memcpy(p, src, n);
	Rd_BufUnlock(buf);
}

void CRenderIB_Add(CRenderIB *self, int numtri, const uint16_t *idx) {
	CHECK(self->pVertIB);
	CHECK(numtri > 0);
	CHECK(self->Used + numtri <= self->Total);
	Buf_Write(self->pVertIB, (uint32_t)self->Used * 6, idx, (uint32_t)numtri * 6);
	self->Used += numtri;
}

void CRenderVB_Add(CRenderVB *self, int numvert, const void *verts) {
	CHECK(self->pVertVB);
	CHECK(numvert > 0);
	CHECK(self->Used + numvert <= self->Total);
	Buf_Write(self->pVertVB, (uint32_t)(self->Used * self->Size), verts, (uint32_t)(self->Size * numvert));
	self->Used += numvert;
}

/* the buffer (AddRef) at 0 and its used count at 8 of both kinds */
static void *Buf_Get(void *const *buf, const int *used, int *out) {
	*out = *used;
	Rd_AddRef(*buf);
	return *buf;
}

void *CRenderIB_Get(CRenderIB *self, int *used) {
	return Buf_Get(&self->pVertIB, &self->Used, used);
}

void *CRenderVB_Get(CRenderVB *self, int *used) {
	return Buf_Get(&self->pVertVB, &self->Used, used);
}

void CRenderIB_Create(CRenderIB *self, uint32_t numtri) {
	CHECK(!self->pVertIB);
	CHECK(numtri < 64000);
	self->Total = (int)numtri;
	self->Used = 0;
	if (numtri) {
		void *dev = g_pd3dDevice;
		CHECK(Rd_DevCreateIndexBuffer(dev, numtri * 6, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
			D3DPOOL_DEFAULT, &self->pVertIB) >= 0);
	}
}

void CRenderVB_Create(CRenderVB *self, uint32_t numvert, int size, uint32_t fvf) {
	CHECK(!self->pVertVB);
	CHECK(numvert < 64000);
	self->Total = (int)numvert;
	self->Used = 0;
	self->Size = size;
	self->FVF = fvf;
	if (numvert) {
		void *dev = g_pd3dDevice;
		CHECK(Rd_DevCreateVertexBuffer(dev, numvert * (uint32_t)size, D3DUSAGE_WRITEONLY, fvf,
			D3DPOOL_DEFAULT, &self->pVertVB) >= 0);
	}
}

byte Render_Clear(const box_t *rect, uint32_t flags, uint32_t color, float z, uint32_t stencil) {
	E3D_FlushBatch();
	CHECK((flags & D3DCLEAR_STENCIL) == 0);
	if (g_RenderOff) {
		return 1;
	}
	if (flags & D3DCLEAR_ZBUFFER && Render_StencilBits()) {
		flags |= D3DCLEAR_STENCIL;
	}
	int r[4];
	if (rect) {
		Render_ClipRect(r, g_ViewportFull, rect);
	}
	void *dev = g_pd3dDevice;
	return Rd_DevClear(dev, rect != NULL, rect ? r : NULL, flags, color, z, stencil) == 0;
}

void Render_GetViewportSize(int *width, int *height) {
	CHECK(g_ViewportW && g_ViewportH);
	CHECK(width && height);
	*width = g_ViewportW;
	*height = g_ViewportH;
}

/* the time of the engine in ms as a float (unsigned through double) */
static float Time_Float(void) {
	return (float)(double)g_TimeMs;
}

static uint32_t F2U(float f) {
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	return u;
}

static void Render_SetTexTransform(uint32_t stage, const matrix_t *m) {
	void *dev = g_pd3dDevice;
	Rd_DevSetTransform(dev, D3DTS_TEXTURE0 + stage, m);
}

static void Render_PSConst(uint32_t reg, const float *c) {
	void *dev = g_pd3dDevice;
	Rd_DevSetPixelShaderConstant(dev, reg, c, 1);
}

void Render_PSConstAlpha(const CMaterial *mat, uint32_t reg) {
	static const float one[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
	float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if ((!mat->params.transp || mat->params.attrib & MAT_1000) && !g_MatForced) {
		memcpy(c, one, sizeof(c));
	}
	Render_PSConst(reg, c);
}

void Mat_StageEnvBump(CMaterial *mat, uint32_t flags, uint32_t stageEnv, uint32_t stageBump) {
	float bias = 0.0f;
	if (flags & 5) {
		const CMap *bump = mat->map[3];
		float s = bump->amount * g_EnvBumpScale;
		if (bump->compressed && g_EnvBumpBias) {
			bias = s * -0.5f;
			s = (float)((double)s + (double)s);
		}
		float rot = bump->uvRot[2] != 0.0f ? Time_Float() * 0.001f * bump->uvRot[2] : 0.0f;
		Mat_StageBumpMat(mat, flags, stageBump, 0.0f, s, rot);
	}
	if (flags & 2) {
		Mat_StageBumpMat(mat, flags, stageBump, 0.0f, 0.0f, 0.0f);
	}
	Mat_StageEnvMap(mat, flags, stageEnv, bias);
}

void Mat_StageEnvMap(CMaterial *mat, uint32_t flags, uint32_t stage, float bias) {
	CHECK(g_UseHWEnvMap);
	if (flags & 1) {
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		Render_TSS(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR);
	}
	if (flags & 5) {
		matrix_t m;
		Matrix_LoadIdentity(&m);
		const CMap *env = mat->map[2];
		if (g_EnvMapViewDep == 2) {
			m = g_MatBillboard;
			m.m[12] = 0.0f;
			m.m[13] = 0.0f;
			m.m[14] = 0.0f;
			vec3_t s = { env->uvScale[0] * g_EnvMapScale, -env->uvScale[1] * g_EnvMapScale, g_EnvMapScale };
			Matrix_PostScale(&m, &s);
		} else {
			m.m[0] = env->uvScale[0] * g_EnvMapScale;
			m.m[5] = -(env->uvScale[1] * g_EnvMapScale);
		}
		m.m[12] = -g_EnvMapOffset - env->uvOffset[0];
		m.m[13] = env->uvOffset[0] + g_EnvMapOffset;
		if (g_EnvMapViewDep == 1) {
			matrix_t v;
			void *dev = g_pd3dDevice;
			Rd_DevGetTransform(dev, D3DTS_VIEW, &v);
			vec3_t dir = { v.m[2], v.m[6], v.m[10] }, ang;
			Vec3_GetRotAng(&ang, &dir);
			m.m[12] -= (float)((double)ang.y * M_1_PI_D);
			m.m[13] -= (float)((double)ang.x * M_1_PI_D);
		}
		m.m[12] += bias;
		m.m[13] += bias;
		Render_SetTexTransform(stage, &m);
	}
	if (flags & 2) {
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		Render_TSS(stage, D3DTSS_TEXCOORDINDEX, stage);
	}
}

void Render_Shader(uint32_t mode, void *a, void *b) {
	CHECK((byte)mode < MAX_SHADER_TAB);
	const ShaderTab *sh = mode < 0x100 ? &g_ShaderTab[mode] : &g_ShaderTab2[mode & 0xff];
	CHECK(sh->Index == (byte)mode);
	CHECK(sh->fnRender);
	sh->fnRender(a, b);
}

/* a random value in lo..hi as rand() of the exe gives it */
static float Rand_Between(float lo, float hi) {
	return (float)((double)(crt_rand() & 0x7fff) * (1.0 / 32767.0) * (double)(hi - lo) + (double)lo);
}

static float Rand_Range(float r) {
	return Rand_Between(-r, r);
}

void Tex_StageJitter(void *unused, uint32_t flags, uint32_t stage, float ju, float jv, float scale) {
	(void)unused;
	if (flags & 1) {
		matrix_t m;
		Matrix_LoadIdentity(&m);
		m.m[0] = scale;
		m.m[5] = scale;
		m.m[8] = Rand_Range(ju);
		m.m[9] = Rand_Range(jv);
		Render_SetTexTransform(stage, &m);
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	}
	if (flags & 2) {
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	}
}

void Mat_StageScroll(CMaterial *mat, uint32_t mapid, uint32_t flags, uint32_t stage, float k) {
	if (flags & 5) {
		CHECK(mapid < MAP_TOTAL);
		const CMap *map = mat->map[mapid];
		CHECK(map);
		if (map->uvOffset[0] == 0.0f && map->uvOffset[1] == 0.0f && map->uvRot[2] == 0.0f) {
			Tex_StageTransform(mat, 2, stage, 0.0f, 0.0f, 1.0f, 0.0f);
		} else {
			Tex_StageTransform(mat, flags, stage, map->uvOffset[0], map->uvOffset[1], map->uvScale[0], map->uvRot[2] * k);
		}
	}
	if (flags & 2) {
		Tex_StageTransform(mat, flags, stage, 0.0f, 0.0f, 1.0f, 0.0f);
	}
}

void Tex_StageTransform(void *unused, uint32_t flags, uint32_t stage, float su, float sv, float scale, float rot) {
	(void)unused;
	if (flags & 5) {
		matrix_t m;
		if (rot != 0.0f) {
			Matrix_RotationZ(&m, Time_Float() * 0.001f * rot);
			m.m[0] *= scale;
			m.m[1] *= scale;
			m.m[4] *= scale;
			m.m[5] *= scale;
		} else {
			Matrix_LoadIdentity(&m);
			m.m[0] = scale;
			m.m[5] = scale;
		}
		float t = Time_Float();
		float u = t * su * 0.001f;
		float v = t * sv * 0.001f;
		m.m[8] = u - (float)(int)u;
		m.m[9] = v - (float)(int)v;
		Render_SetTexTransform(stage, &m);
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	}
	if (flags & 2) {
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	}
}

void Mat_StageBumpMat(CMaterial *mat, uint32_t flags, uint32_t stage, float k, float s, float rot) {
	if (!(flags & 5)) {
		return;
	}
	if (k != 0.0f) {
		s = mat->map[3]->amount * k + s;
	}
	float c = 1.0f, sn = 0.0f;
	if (rot != 0.0f) {
		Float_SinCos(rot, &sn, &c);
	}
	Render_TSS(stage, D3DTSS_BUMPENVMAT00, F2U(c * s));
	Render_TSS(stage, D3DTSS_BUMPENVMAT01, F2U(sn * s));
	Render_TSS(stage, D3DTSS_BUMPENVMAT10, F2U(-(sn * s)));
	Render_TSS(stage, D3DTSS_BUMPENVMAT11, F2U(c * s));
}

/* the shader modes with constants that move */
static void Shader_PSConstDiffuse(const CMaterial *mat) {
	float c[4];
	for (int i = 0; i < 4; i++) {
		c[i] = (float)mat->params.diffuse[i] * (1.0f / 255.0f);
	}
	Render_PSConst(0, c);
}

/* the end of the pixel shader of the modes with stages 1 and 2 */
static void Shader_End12(void) {
	Render_SetPS(0);
	Render_TSS(1, D3DTSS_TEXCOORDINDEX, 1);
	Render_TSS(2, D3DTSS_TEXCOORDINDEX, 2);
	Render_SetMap(1, NULL);
	Render_SetMap(2, NULL);
}

/* the cloud color: diffuse by R_CloudR, G, B and R_CloudA, all times emissive blue by R_CloudEmi */
static void Clouds_Color(const CMaterial *mat, float *c) {
	const byte *d = mat->params.diffuse;
	float e = (float)mat->params.emissive[2] * (1.0f / 255.0f) * g_CloudEmi;
	c[0] = (float)d[0] * (1.0f / 255.0f) * g_CloudR * e;
	c[1] = (float)d[1] * (1.0f / 255.0f) * g_CloudG * e;
	c[2] = (float)d[2] * (1.0f / 255.0f) * g_CloudB * e;
	c[3] = g_CloudA * e;
}

/* map 0 scrolled twice over itself (the second time as the glow map with the pixel shader), the fixed stages at
 * twice the speed and with the texture factor for the color */
void Shader_Clouds(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_CLOUDS);
	float c[4];
	if (!h) {
		Tex_StageTransform(mat, flags, 0, g_CloudVel1x * 2.0f, g_CloudVel1y * 2.0f, g_CloudScale1, 0.0f);
		if (flags & 1) {
			Clouds_Color(mat, c);
			c[3] *= 0.5f;
			Render_State(D3DRS_TEXTUREFACTOR, Color_FromFloat(c));
			Stage_Color(0, D3DTA_TEXTURE, D3DTA_TFACTOR, D3DTOP_MODULATE2X);
			Stage_Alpha(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
			Stage_Color(1, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_SELECTARG2);
			Stage_Alpha(1, D3DTA_TEXTURE, D3DTA_TFACTOR, D3DTOP_MODULATE);
			Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
		}
		if (flags & 5) {
			Render_SetMap(1, mat->map[MAP_DIFFUSE]);
		}
		if (flags & 2) {
			Render_State(D3DRS_TEXTUREFACTOR, 0xffffffff);
			Stage_End();
			Stage_Alpha(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
			Stage_Off(1);
			Render_TSS(1, D3DTSS_TEXCOORDINDEX, 1);
			Render_SetMap(1, NULL);
		}
		return;
	}
	Tex_StageTransform(mat, flags, 0, g_CloudVel1x, g_CloudVel1y, g_CloudScale1, 0.0f);
	Tex_StageTransform(mat, flags, 1, g_CloudVel2x, g_CloudVel2y, g_CloudScale2, 0.0f);
	if (flags & 1) {
		Render_SetPS(h);
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		Clouds_Color(mat, c);
		Render_PSConst(0, c);
		Render_SetMap(1, mat->map[MAP_DIFFUSE]);
		Render_SetMap(2, mat->map[MAP_GLOW]);
	}
	if (flags & 2) {
		Shader_End12();
	}
}

/* the glow and bump maps jittered, a flicker of 0.92..1 */
void Shader_Hologram(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_HOLOGRAM);
	if (!h) {
		Render_PSFixed(mat, (byte)flags);
		return;
	}
	Tex_StageJitter(mat, flags, 0, 0.0f, 0.0001f, 1.0f);
	Tex_StageTransform(mat, flags, 1, 0.0f, 0.05f, 1.0f, 0.0f);
	Tex_StageJitter(mat, flags, 2, 0.5f, 0.5f, 2.0f);
	if (flags & 1) {
		Render_SetPS(h);
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		float f = Rand_Between(0.92f, 1.0f);
		const float c[4] = { f, f, f, f };
		Render_PSConst(0, c);
		Render_SetMap(1, mat->map[MAP_GLOW]);
		Render_SetMap(2, mat->map[MAP_BUMP]);
	}
	if (flags & 2) {
		Shader_End12();
	}
}

/* the glow flickers (R_GlowFlickMod..1 squared, two slow waves of the time), moved by the bump noise map; Glowmap
 * without the pixel shader */
void Shader_GlowFlickLightmap(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_GLOWFLICK_LM);
	if (!h) {
		Shader_GlowmapLightmap(mat, flags);
		return;
	}
	Tex_StageTransform(mat, flags, 1, 0.01f, 0.1f, g_GlowFlickTile, g_GlowFlickRot);
	Mat_StageBumpMat(mat, flags, 3, 0.0f, g_GlowFlickBump, (float)(double)g_TimeMs * g_GlowFlickBRot);
	if (flags & 1) {
		Render_SetPS(h);
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 1);
		Render_TSS(3, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		float f = Rand_Between(g_GlowFlickMod, 1.0f);
		double t = (double)g_TimeMs;
		float a = (float)(cos(t * 0.0089) * 0.02 + 0.98);
		float b = (float)(cos(t * 0.0052) * 0.03 + 0.97);
		float v = a * b * (f * f);
		const float c[4] = { v, v, v, v };
		Render_PSConst(0, c);
		Render_SetMap(1, g_MatBump->map[MAP_DIFFUSE]);
		LIGHTMAP(2);
		Render_SetMap(3, mat->map[MAP_GLOW]);
	}
	if (flags & 2) {
		Render_SetPS(0);
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 1);
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 2);
		Render_TSS(3, D3DTSS_TEXCOORDINDEX, 3);
		Render_SetMap(1, NULL);
		Render_SetMap(2, NULL);
		Render_SetMap(3, NULL);
	}
}

/* Electric and Fire: map 0 and two layers of the glow and bump maps scrolled, the bump turned; the diffuse color as
 * the constant */
static void Shader_Burn(CMaterial *mat, uint32_t flags, uint32_t h, float bumpScale) {
	Mat_StageBumpMat(mat, flags, 1, 0.0f, bumpScale, 0.0f);
	if (flags & 1) {
		Render_SetPS(h);
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
		Render_TSS(2, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		Render_SetMap(1, mat->map[MAP_GLOW]);
		Render_SetMap(2, mat->map[MAP_BUMP]);
		Shader_PSConstDiffuse(mat);
	}
	if (flags & 2) {
		Shader_End12();
	}
}

/* while the world runs, the third layer jumps at random half of the frames */
void Shader_Electric(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_ELECTRIC);
	if (!h) {
		Render_PSFixed(mat, (byte)flags);
		return;
	}
	Tex_StageTransform(mat, flags, 0, -0.1f, 0.26f, 1.2f, 0.0f);
	Tex_StageTransform(mat, flags, 1, 0.08f, -0.06f, 5.4f, 0.0f);
	if (World_TimeScale() > 0.0f && crt_rand() & 1) {
		float scale = (float)(crt_rand() % 10) / 500.0f + 4.0f;
		Tex_StageTransform(mat, flags, 2, 0.65f, -0.38f, scale, 0.0f);
	} else {
		Tex_StageTransform(mat, flags, 2, -0.98f, 0.55f, 4.0f, 0.0f);
	}
	Shader_Burn(mat, flags, h, 0.8f);
}

void Shader_Fire(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_FIRE);
	if (!h) {
		Render_PSFixed(mat, (byte)flags);
		return;
	}
	Tex_StageTransform(mat, flags, 0, -0.5f, 0.8f, 4.0f, 0.0f);
	Tex_StageTransform(mat, flags, 1, 0.02f, -0.03f, 6.0f, 0.0f);
	float scale = (float)(crt_rand() % 10) / 100.0f + 4.0f;
	Tex_StageTransform(mat, flags, 2, 0.65f, -0.38f, scale, 0.0f);
	Shader_Burn(mat, flags, h, 0.4f);
}

/* map 0 scrolled both ways by its own scroll under the reflection of the bump map; the material must be
 * transparent. Without the pixel shader only map 0 */
void Shader_Waves(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_WAVES);
	if (!h) {
		if (flags & 1) {
			Stage_End();
			Stage_Alpha(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
		}
		if (flags & 2) {
			Stage_End();
			Stage_Alpha(0, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_SELECTARG1);
		}
		return;
	}
	Mat_StageEnvBump(mat, flags, 2, 2);
	if (flags & 1) {
		Render_SetPS(h);
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 0);
		Render_TSS(3, D3DTSS_TEXCOORDINDEX, 0);
	}
	if (flags & 5) {
		if (!mat->params.transp) {
			CheckFailed("mat->params.dif_alpha", __FILE__, __TIMESTAMP__, __LINE__);
		}
		const CMap *m = mat->map[MAP_DIFFUSE];
		Tex_StageTransform(mat, flags, 0, m->uvOffset[0], m->uvOffset[1], m->uvScale[0], m->uvRot[2]);
		Tex_StageTransform(mat, flags, 1, -m->uvOffset[0], -m->uvOffset[1], m->uvScale[0], m->uvRot[2]);
		Render_SetMap(1, mat->map[MAP_BUMP]);
		Render_SetMap(2, mat->map[MAP_ENVIRONMENT]);
		Render_SetMap(3, mat->map[MAP_DIFFUSE]);
	}
	if (flags & 2) {
		Tex_StageTransform(mat, flags, 0, 0.0f, 0.0f, 1.0f, 0.0f);
		Tex_StageTransform(mat, flags, 1, 0.0f, 0.0f, 1.0f, 0.0f);
		Render_SetPS(0);
		Render_TSS(1, D3DTSS_TEXCOORDINDEX, 1);
		Render_TSS(3, D3DTSS_TEXCOORDINDEX, 3);
		Render_SetMap(1, NULL);
		Render_SetMap(2, NULL);
		Render_SetMap(3, NULL);
	}
}

/* map 0 jittered, the diffuse color as the constant */
void Shader_Noise(CMaterial *mat, uint32_t flags) {
	uint32_t h = Render_PSHandle(PS_NOISE);
	if (!h) {
		Render_PSFixed(mat, (byte)flags);
		return;
	}
	Tex_StageJitter(mat, flags, 0, 2.0f, 2.0f, 14.0f);
	if (flags & 1) {
		Render_SetPS(h);
	}
	if (flags & 5) {
		Shader_PSConstDiffuse(mat);
	}
	if (flags & 2) {
		Render_SetPS(0);
	}
}

void Render_D3DMaterial(const CMaterial *mat, const float *colors) {
	float m[17];
	const float *p = colors;
	if (g_MatForced || g_DiffuseMod4x) {
		float k = !g_MatForced || g_DiffuseMod4x ? 0.25f : 1.0f;
		for (int i = 0; i < 16; i++) {
			m[i] = i == 3 || (i >= 8 && i < 12) ? colors[i] : colors[i] * k;
		}
		m[16] = colors[16];
		if (g_MatForced) {
			float a = mat->params.transp ? colors[3] : 1.0f;
			m[3] = (float)g_MatForced->params.diffuse[3] / 255.0f * a;
		}
		p = m;
	}
	void *dev = g_pd3dDevice;
	Rd_DevSetMaterial(dev, p);
}

const matrix_t *Render_GetProj(void) {
	return &g_MatProj;
}

const matrix_t *Render_GetView(void) {
	return &g_MatView;
}

const matrix_t *Render_GetBillboard(void) {
	return &g_MatBillboard;
}

const vec3_t *Render_GetEyePos(void) {
	return &g_EyePos;
}

static void Render_SetTexture(uint32_t stage, void *tex) {
	void *dev = g_pd3dDevice;
	Rd_DevSetTexture(dev, stage, tex);
}

void Render_DrawTexRect(const box_t *rect, uint32_t cLeft, uint32_t cRight, void *tex) {
	if (g_RenderOff) {
		return;
	}
	void *saved = g_pd3dDevice;
	g_pd3dDevice = g_Engine->device;
	Render_ResetState();
	Render_SetTexture(0, tex);
	Render_TSS(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	Render_TSS(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	uint32_t op;
	void *dev = g_pd3dDevice;
	Rd_DevGetTextureStageState(dev, 0, D3DTSS_COLOROP, &op);
	Render_TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	Render_TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	Render_TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	uint32_t vp[6];
	dev = g_pd3dDevice;
	Rd_DevGetViewport(dev, vp);
	int r[4];
	Render_ClipRect(r, vp, rect);
	float x0 = (float)r[0] - 0.5f, y0 = (float)r[1] - 0.5f, x1 = (float)r[2] - 0.5f, y1 = (float)r[3] - 0.5f;
	const Vert2D v[4] = {
		{ x0, y0, 0.0f, 1.0f, cLeft, 0, 0.0f, 0.0f },
		{ x1, y0, 0.0f, 1.0f, cRight, 0, 1.0f, 0.0f },
		{ x0, y1, 0.0f, 1.0f, cLeft, 0, 0.0f, 1.0f },
		{ x1, y1, 0.0f, 1.0f, cRight, 0, 1.0f, 1.0f },
	};
	dev = g_pd3dDevice;
	Rd_DevSetVertexShader(dev, D3DFVF_2D);
	dev = g_pd3dDevice;
	Rd_DevDrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vert2D));
	Render_TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	Render_TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	Render_TSS(0, D3DTSS_COLOROP, op);
	Render_ResetState();
	Render_SetTexture(0, NULL);
	g_pd3dDevice = saved;
}

void Render_EndD3D(void) {
	E3D_DebugPrintf("\n> RenderFinD3D()");
	g_D3DOn = 0;
	CHECK(!g_ViewportW);
	void *dev = g_Engine->device;
	if (dev) {
		for (uint32_t i = 0; i < 8; i++) {
			Rd_DevSetTexture(dev, i, NULL);
		}
		Rd_DevSetPixelShader(dev, 0);
	}
	for (int i = 0; g_PSNames[i]; i++) {
		if (g_PSHandles[i]) {
			if (dev) {
				Rd_DevDeletePixelShader(dev, g_PSHandles[i]);
			}
			g_PSHandles[i] = 0;
		}
	}
	RenderLock_ReleaseD3D();
	VertList_ReleaseD3D();
	RenderEVA_ReleaseD3D();
	RenderTarget_ReleaseD3D();
	if (dev) {
		RenderGamma_ReleaseD3D();
	}
	g_pd3dDevice = NULL;
	E3D_DebugPrintf("\n< RenderFinD3D()");
}

void Render_EndView(void) {
	E3D_FlushBatch();
	Render_SetMaterial(g_MatDefault, 0);
	void *dev = g_pd3dDevice;
	Rd_DevSetViewport(dev, g_ViewportFull);
	memset(g_Viewport, 0, 6 * sizeof(uint32_t));
}

void Render_ClearForced(void) {
	E3D_FlushBatch();
	g_MatFX1 = NULL;
	g_MatFX2 = NULL;
	g_MatForced = NULL;
	g_MatFX3 = NULL;
	g_MatCurrent = NULL;
}

const char *Render_GetDriver(void) {
	return g_DriverStr;
}

static void Render_LogVersion(const char *fmt, uint32_t ver) {
	char *buf = GAME_ARRAY(char, g_VersionStr);
	Bprintf(buf, 0x40, "%d.%d", ver >> 8 & 0xff, ver & 0xff);
	E3D_LogPrint(g_FileLog, fmt, buf);
}

static void Render_LogDevice(const D3DCaps8 *caps, const d3dSurfaceDesc_t *mode, const d3dSurfaceDesc_t *zbuf) {
	E3D_LogPrint(g_FileLog, "%s", g_Engine->deviceStats);
	E3D_LogPrint(g_FileLog, "Driver: %s  WHQL: %d", g_DriverStr, g_Adapter.WHQLLevel);
	E3D_LogPrint(g_FileLog, "%s", g_DeviceStr);
	E3D_LogPrint(g_FileLog, "Mode: %dx%d %s", mode->Width, mode->Height, D3DFmt_Name(mode->Format));
	E3D_LogPrint(g_FileLog, "ZBuf: %dx%d %s", zbuf->Width, zbuf->Height, D3DFmt_Name(zbuf->Format));
	E3D_LogPrint(g_FileLog, "MaxTexture: %dx%d", caps->MaxTextureWidth, caps->MaxTextureHeight);
	E3D_LogPrint(g_FileLog, "MaxAnisotropy: %d", caps->MaxAnisotropy);
	E3D_LogPrint(g_FileLog, "MaxTextureBlendStages: %d", caps->MaxTextureBlendStages);
	E3D_LogPrint(g_FileLog, "MaxSimultaneousTextures: %d", caps->MaxSimultaneousTextures);
	E3D_LogPrint(g_FileLog, "MaxActiveLights: %d", caps->MaxActiveLights);
	E3D_LogPrint(g_FileLog, "MaxStreams: %d", caps->MaxStreams);
	Render_LogVersion("VertexShaderVersion: %s", caps->VertexShaderVersion);
	Render_LogVersion("PixelShaderVersion: %s", caps->PixelShaderVersion);
}

static void Render_LoadPS(int i) {
	const char *name = g_PSNames[i];
	void *data, *code = NULL, *errors = NULL;
	uint32_t size;

	FileIO_ReadAll(name, &data, &size);
	int hr = D3DX_AssembleShader(data, size, 0, NULL, &code, &errors);
	if (errors || hr < 0) {
		const char *msg = errors ? Rd_XBufGetBufferPointer(errors)
					 : "Could not open file";
		Err_Printf(0, "Pixel Shader Errors (%s):\n%s", name, msg);
	} else {
		void *dev = g_pd3dDevice;
		const void *p = Rd_XBufGetBufferPointer(code);
		if (Rd_DevCreatePixelShader(dev, name, p, &g_PSHandles[i]) < 0) {
			g_PSHandles[i] = 0;
			E3D_LogVerbose(g_FileLog, "WARN: CreatePixelShader failed: %s", name);
		}
	}
	Ptr_Delete(&data);
	if (code) {
		Rd_Release(code);
	}
	if (errors) {
		Rd_Release(errors);
	}
}

/* the identity and the bias matrix */
static void Render_SetMatrices(void) {
	Matrix_LoadIdentity((matrix_t *)g_MatIdentity);
	Matrix_LoadIdentity(&g_MatBias);
	g_MatBias.m[0] = 1.0f / 2048;
	g_MatBias.m[5] = 1.0f / 2048;
	g_MatBias.m[10] = 1.0f / 2048;
}

/* the static initializer: the shaders on with the hardware environment maps, and the two matrices */
void Render_InitMatrices(void) {
	if (g_UseHWEnvMap) {
		g_UseShaders = 1;
	}
	Render_SetMatrices();
}

void Render_IniD3D(void) {
	static const struct {
		const char *name, *file;
	} defMats[6] = {
		{ "Material_RGB_White", "bmp/rgb_white.tga" },
		{ "Material_RGB_CheckBrd", "bmp/rgb_chkbrd.tga" },
		{ "Material_RGB_Grey", "bmp/rgb_grey.tga" },
		{ "Material_RGB_Invis", NULL },
		{ "Material_RGB_BumpNoise", "bmp/rgb_noise.bump.tga" },
		{ "Material_RGB_Ground", "bmp/rgb_ground.tga" },
	};
	D3DCaps8 caps;
	d3dSurfaceDesc_t mode, zbuf;

	E3D_DebugPrintf("\n> RenderIniD3D()");
	if (!g_Engine || !g_Engine->device) {
		CheckFailed("g_pD3DApp && g_pD3DApp->m_pd3dDevice", __FILE__, __TIMESTAMP__, __LINE__);
	}
	CD3DApplication *motor = g_Engine;
	void *dev = motor->device;
	g_pd3dDevice = dev;
	memcpy(&caps, &motor->caps, sizeof(caps));
	((CMaterial *)g_MatLine)->params.lighting = 0;
	((CMaterial *)g_MatDefault)->params.lighting = 0;
	memcpy(&g_Adapter, &motor->adapters[motor->adapter].id, sizeof(g_Adapter));
	const uint16_t *ver = g_Adapter.DriverVersion;
	Bprintf(g_DriverStr, 0x100, "%s [%s]  %d.%02d.%02d.%04d", g_Adapter.Description, g_Adapter.Driver, ver[3],
		ver[2], ver[1], ver[0]);
	Bprintf(g_DeviceStr, 0x100, "VendorId: %04X   DeviceId: %04X   SubSysId: %04X   Revision: %d",
		g_Adapter.VendorId, g_Adapter.DeviceId, g_Adapter.SubSysId, g_Adapter.Revision);
	memcpy(&mode, &motor->backBufferDesc, sizeof(mode));
	memcpy(&zbuf, &motor->depthBufferDesc, sizeof(zbuf));
	if (g_DeviceLogged != motor->devId) {
		g_DeviceLogged = motor->devId;
		Render_LogDevice(&caps, &mode, &zbuf);
	}
	Rd_DevGetViewport(dev, g_ViewportFull);
	g_UseShaders2 = 0;
	g_UseShaders = 1;
	Render_SetMatrices();
	uint32_t tc = caps.TextureCaps;
	g_MipmapsOn = (tc & D3DPTEXTURECAPS_MIPMAP) != 0;
	g_TexRepeat = (tc & D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE) != 0;
	g_TexNoPow2 = (tc & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) != 0;
	g_TexPow2 = (tc & D3DPTEXTURECAPS_POW2) != 0;
	RenderGamma_Begin();
	Render_TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	Render_TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	Render_TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	Render_TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	Render_TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	Render_TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	for (uint32_t i = 1; i < 8; i++) {
		Render_TSS(i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		Render_TSS(i, D3DTSS_COLORARG2, D3DTA_CURRENT);
		Render_TSS(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
		Render_TSS(i, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		Render_TSS(i, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
		Render_TSS(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	}
	if (caps.PixelShaderVersion < D3DPS_VERSION(1, 1)) {
		E3D_LogBlue(g_FileLog, "The graphics device does not support Pixel Shader 1.1");
		g_PSOn = 0;
		g_EnvBump = 0;
		g_EnvMap = 0;
		g_FogEnable = 0;
	}
	for (int i = 0; g_PSNames[i]; i++) {
		g_PSHandles[i] = 0;
		if (g_PSOn) {
			Render_LoadPS(i);
		}
	}
	RenderLock_InitD3D();
	VertList_InitD3D();
	if (RenderEVA_InitD3D() != 0) {
		CheckFailed("EVA_crear_VB() == S_OK", __FILE__, __TIMESTAMP__, __LINE__);
	}
	RenderTarget_InitD3D();
	ModelFX_InitD3D();
	/* the six globals one after the other in the original, separate C objects since stage 3c2 */
	CMaterial **slots[6] = { &g_MatWhite, &g_MatCheckerboard, &g_MatGray, &g_MatInvis, &g_MatBump, &g_MatGround };
	for (int i = 0; i < 6; i++) {
		CMaterial m;
		CMaterial_ctor(&m);
		String_Set(&m.Name, defMats[i].name);
		if (defMats[i].file) {
			CMaterial_SetMap(&m, 0, defMats[i].file);
		} else {
			m.params.zFunc = 1;
		}
		m.params.lighting = 0;
		*slots[i] = Mat_New(&m, 1);
		CMaterial_dtor(&m);
	}
	cString_Set(g_DriverName, g_DriverStr);
	if (g_FrameCount) {
		g_FrameCount++;
	}
	g_D3DOn = 1;
	E3D_LogPrint(g_FileLog, "Render device initialized");
	E3D_DebugPrintf("\n< RenderIniD3D()");
}

void Render_BeginFrame(void) {
	g_pd3dDevice = g_Engine->device;
	CHECK(g_pd3dDevice);
	g_RenderI1F4 = 0;
	RenderGamma_Begin();
	memcpy(g_Viewport, g_ViewportFull, 6 * sizeof(uint32_t));
	void *dev = g_pd3dDevice;
	Rd_DevSetViewport(dev, g_Viewport);
	Render_ResetState();
	Render_State(D3DRS_AMBIENT, g_AmbientD3D);
	Render_State(D3DRS_FOGENABLE, 0);
	Render_State(D3DRS_CULLMODE, g_CullMode ? D3DCULL_NONE : D3DCULL_CCW);
	g_PShaderCount = 0;
	RenderGamma_LogShaderMask();
}

int Render_DrawListIdx(const CVertList *v, const uint16_t *idx, int ntri, byte stats) {
	void *dev = g_Engine->device;
	if (!dev) {
		return E_FAIL;
	}
	if (stats) {
		g_RenderTris += ntri;
		g_RenderVerts += v->nVert;
	}
	Rd_DevSetVertexShader(dev, CVertList_FVF((CVertList *)v));
	return Rd_DevDrawIndexedPrimitiveUP(dev, D3DPT_TRIANGLELIST, 0,
		(uint32_t)v->nVert, (uint32_t)ntri, idx, D3DFMT_INDEX16, v->pVert, (uint32_t)CVertList_Size((CVertList *)v));
}

static int Render_WorldToDevice(const matrix_t *m) {
	void *dev = g_pd3dDevice;
	return Rd_DevSetTransform(dev, D3DTS_WORLD, m);
}

int Render_SetWorldMatrix(const matrix_t *m, byte defer) {
	g_WorldSet = 0;
	g_MatWorld = *m;
	if (!defer && g_pd3dDevice) {
		Render_WorldToDevice(&g_MatWorld);
		g_WorldSet = 1;
	}
	return 0;
}

int Render_SetWorldD3D(const matrix_t *m) {
	g_WorldSet = 0;
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	Render_WorldToDevice(m);
	return 0;
}

void Render_WorldDirty(void) {
	g_WorldSet = 0;
}

int Render_GetWorld(matrix_t *out) {
	if (out) {
		*out = g_MatWorld;
	}
	return 0;
}

int Render_SetWorld(void) {
	if (!g_WorldSet) {
		if (!g_pd3dDevice) {
			return E_FAIL;
		}
		Render_WorldToDevice(&g_MatWorld);
		g_WorldSet = 1;
	}
	return 0;
}

void Render_ResetState(void) {
	uint32_t filter = g_TexFilter == 2 ? 3 : (uint32_t)(g_TexFilter != 0) + 1;
	g_FilterMin = filter;
	g_FilterMag = filter;
	g_FilterMip = 2 - (uint32_t)(g_MipFilter != 1);
	uint32_t aniso = g_Anisotropy;
	if (g_Engine->caps.MaxAnisotropy < aniso) {
		aniso = g_Engine->caps.MaxAnisotropy;
	}
	g_AnisotropyCur = aniso;
	for (uint32_t i = 0; i < 8; i++) {
		Render_SetTexture(i, NULL);
		Render_TSS(i, D3DTSS_TEXCOORDINDEX, i);
		Render_TSS(i, D3DTSS_MINFILTER, g_FilterMin);
		Render_TSS(i, D3DTSS_MAGFILTER, g_FilterMag);
		Render_TSS(i, D3DTSS_MIPFILTER, g_MipmapsOn ? g_FilterMip : 0);
		Render_TSS(i, D3DTSS_MAXANISOTROPY, g_AnisotropyCur);
		Render_TSS(i, D3DTSS_MIPMAPLODBIAS, F2U(g_MipmapBias + g_MipmapBias2));
		Render_TSS(i, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		Render_TSS(i, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		g_StageMip[i] = 1;
		g_StageTile[i] = 3;
	}
}

/* the shader mode of the material through its id (pass 0 only); the alpha test, the blending and its value from
 * the bytes 4, 5, 6 of the id */
static void Mat_Shader(const CMaterial *mat, byte frameChanged, byte *alphaTest, byte *blend, byte *alpha) {
	CMatId *id;
	CMaterial_GetId((CMaterial *)mat, &id);
	const byte *b = (const byte *)id;
	*alphaTest = b[4];
	*blend = b[5];
	*alpha = b[6];
	uint32_t mode = (uint32_t)id->id;
	if (g_LightmapMat) {
		mode += 0x100;
	}
	if (g_ShaderCurrent == mode && !frameChanged) {
		Render_Shader(mode, (void *)mat, (void *)4);
	} else {
		g_PShaderCount++;
		Render_Shader(g_ShaderCurrent, NULL, (void *)2);
		Render_Shader(mode, (void *)mat, (void *)1);
		g_ShaderCurrent = mode;
	}
}

/* the blending of pass 0 */
static void Mat_Blend(void *dev, const CMaterial *mat, byte shAlphaTest, byte shBlend, byte shAlpha) {
	byte alpha = shAlphaTest ? shAlpha : 0;
	byte blend = shBlend ? mat->params.transp : 0;
	int src = mat->params.srcBlend, dst = mat->params.dstBlend;
	byte at = mat->params.attrib >> 5 & 1;
	byte alphaTest = at ? 1 : alpha;
	byte blending = at ? 0 : blend;
	if (g_MatForced) {
		src = g_MatForced->params.srcBlend;
		if (!blending) {
			dst = g_MatForced->params.dstBlend;
		}
		alphaTest = 0;
		blending = 1;
	}
	if (alphaTest && !(src == 2 && dst == 2)) {
		Rd_DevSetRenderState(dev, D3DRS_ALPHATESTENABLE, 1);
		Rd_DevSetRenderState(dev, D3DRS_ALPHAREF, at ? 0x80 : 1);
		Rd_DevSetRenderState(dev, D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
	} else {
		Rd_DevSetRenderState(dev, D3DRS_ALPHATESTENABLE, 0);
	}
	if (mat->params.attrib & 0x400) {
		alphaTest = 0;
	}
	if (!alphaTest && !blending) {
		Rd_DevSetRenderState(dev, D3DRS_ALPHABLENDENABLE, 0);
		Rd_DevSetRenderState(dev, D3DRS_FOGENABLE,
			mat->params.fog ? (uint32_t)g_FogActive : 0);
		Rd_DevSetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xf);
		if (g_PSOn) {
			Rd_DevSetTextureStageState(dev, 0, D3DTSS_ALPHAOP,
				D3DTOP_SELECTARG2);
		}
		return;
	}
	Rd_DevSetRenderState(dev, D3DRS_ALPHABLENDENABLE, at ^ 1);
	Rd_DevSetRenderState(dev, D3DRS_SRCBLEND, (uint32_t)src);
	Rd_DevSetRenderState(dev, D3DRS_DESTBLEND, (uint32_t)dst);
	Rd_DevSetTextureStageState(dev, 0, D3DTSS_ALPHAOP,
		!blending ? D3DTOP_SELECTARG1 : alphaTest ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
	byte fog = mat->params.fog && g_FogActive && g_AlphaFog && (g_AlphaFog != 1 || (src == 5 && dst == 6));
	Rd_DevSetRenderState(dev, D3DRS_FOGENABLE, fog);
	Rd_DevSetRenderState(dev, D3DRS_COLORWRITEENABLE,
		src == 2 && dst == 2 ? 0xf : 7);
}

/* pass 1: the second texture set added */
static void Mat_Pass1(void *dev) {
	Rd_DevSetRenderState(dev, D3DRS_ALPHATESTENABLE, 0);
	Rd_DevSetRenderState(dev, D3DRS_ALPHABLENDENABLE, 1);
	Rd_DevSetRenderState(dev, D3DRS_SRCBLEND, 2);
	Rd_DevSetRenderState(dev, D3DRS_DESTBLEND, 2);
}

int Render_SetMaterial(void *m, int pass) {
	if (!m) {
		CHECK(pass == -1);
		g_MatCurrent = NULL;
		return 0;
	}
	const CMaterial *mat = g_MatFX1 ? g_MatFX1 : m;
	if (g_UseDefMaterial && !(mat->params.attrib & 0x840)) {
		mat = g_UseDefMaterial == 1 ? g_MatWhite : g_MatInvis;
	}
	void *dev = g_Engine->device;
	if (!dev) {
		return E_FAIL;
	}
	if (g_MatCurrent != mat || g_MatPass != pass || g_MatFrame != g_FrameCount || g_LightmapMat != g_MatCtx) {
		byte frameChanged = g_MatFrame != g_FrameCount;
		g_MatCurrent = mat;
		g_MatPass = pass;
		g_MatFrame = g_FrameCount;
		g_MatCtx = g_LightmapMat;
		if (g_MaterialLog) {
			const char *ctx = g_LightmapMat ? *(const char **)((byte *)g_LightmapMat + 4) : GAME_STR(str_Empty);
			E3D_LogWhite(g_FileLog, "MatLog: %d %d %d [%s] [%s]", g_FrameCount, g_MatChanges,
				CMaterial_GetId((CMaterial *)mat, NULL), mat->Name, ctx);
		}
		Rd_DevSetRenderState(dev, D3DRS_ZWRITEENABLE, mat->params.zWrite);
		Rd_DevSetRenderState(dev, D3DRS_ZFUNC, (uint32_t)mat->params.zFunc);
		Render_State(D3DRS_ZBIAS, mat->params.attrib & 0x800 ? g_DecalZBias : 0);
		uint32_t light = 0;
		if (mat->params.lighting) {
			Render_D3DMaterial(mat, g_MatFX3 ? g_MatFX3->colors : mat->colors);
			light = 1;
		}
		Rd_DevSetRenderState(dev, D3DRS_LIGHTING, light);
		byte shAlphaTest = 1, shBlend = 1, shAlpha = 0;
		if (g_UseShaders) {
			CHECK(!pass);
			Mat_Shader(mat, frameChanged, &shAlphaTest, &shBlend, &shAlpha);
		} else if (mat->params.reflection && pass == 1) {
			Rd_DevSetTextureStageState(dev, 0, D3DTSS_TEXCOORDINDEX, 1);
		} else {
			Rd_DevSetTextureStageState(dev, 0, D3DTSS_TEXCOORDINDEX, 0);
		}
		if (!pass) {
			Mat_Blend(dev, mat, shAlphaTest, shBlend, shAlpha);
		} else if (pass == 1) {
			Mat_Pass1(dev);
		}
		g_MatChanges++;
	}
	if (pass) {
		if (pass == 1) {
			Render_SetMap(0, mat->map[2]);
		}
		return 0;
	}
	/* R_UseDefTexture: the white texture (1: only without one) */
	CMap *map = mat->map[0];
	switch (g_UseDefTexture) {
	case 1:
		if (!map) {
			map = g_MatWhite->map[0];
		}
		break;
	case 2:
		if (!(mat->params.attrib & 0x40)) {
			map = g_MatWhite->map[0];
		}
		break;
	case 3:
		if (!(mat->params.attrib & 0x40)) {
			map = g_MatCheckerboard->map[0];
		}
		break;
	case 4:
		if (!(mat->params.attrib & 0x40)) {
			map = g_MatGray->map[0];
		}
		break;
	}
	Render_SetMap(0, map);
	return 0;
}

/* the part x0..x1, y0..y1 (-0.5..0.5, y up) of the view through proj */
static void Proj_Sub(matrix_t *out, const matrix_t *proj, float x0, float x1, float y0, float y1) {
	matrix_t m = *proj;
	m.m[0] = proj->m[0] / (x1 - x0);
	m.m[5] = proj->m[5] / (y0 - y1);
	m.m[8] = (x1 + x0) / (x0 - x1);
	m.m[9] = (y1 + y0) / (y1 - y0);
	*out = m;
}

byte Render_ProjTile(const matrix_t *proj) {
	byte on = g_TileOn;
	if (g_TileState < 2) {
		if (!g_TileState && !on) {
			return 0;
		}
		g_TileState = 1;
		if (on) {
			return 0;
		}
		g_TileOn = on = 1;
	}
	int n = g_Tiles;
	g_TileState = 2;
	if (!on) {
		if (++g_TileX == n) {
			g_TileX = 0;
			if (++g_TileY == n) {
				g_TileY = 0;
				g_TileState = 0;
				return 0;
			}
		}
		g_TileOn = 1;
	}
	float fn = (float)n;
	float x0 = (float)g_TileX / fn - 0.5f;
	float x1 = (float)(g_TileX + 1) / fn - 0.5f;
	float y0 = Float_Keep(-((float)g_TileY / fn - 0.5f));
	float y1 = Float_Keep(-((float)(g_TileY + 1) / fn - 0.5f));
	Proj_Sub(&g_MatProj, proj, x0, x1, y0, y1);
	CHECK(g_pd3dDevice);
	void *dev = g_pd3dDevice;
	Rd_DevSetTransform(dev, D3DTS_PROJECTION, &g_MatProj);
	return 1;
}

int Render_SetProj(const matrix_t *m) {
	g_MatProj = *m;
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	void *dev = g_pd3dDevice;
	Rd_DevSetTransform(dev, D3DTS_PROJECTION, &g_MatProj);
	return 0;
}

void Render_SetCull(uint32_t mode) {
	if (g_CullMode && mode == D3DCULL_CCW) {
		mode = D3DCULL_NONE;
	}
	Render_State(D3DRS_CULLMODE, mode);
}

void Render_SetLightmapMat(CMaterial *mat, int mode) {
	if (mat && g_MatFX2) {
		mat = g_MatFX2;
	}
	CHECK(!g_LightmapMat || !mat);
	if (mat) {
		mat->Refs++;
	}
	if (g_LightmapMat) {
		g_LightmapMat->Refs--;
	}
	g_LightmapOp = D3DTOP_MODULATE;
	g_LightmapMat = mat;
	if (mode == 2) {
		g_LightmapOp = D3DTOP_MODULATE2X;
	} else if (mode == 4) {
		g_LightmapOp = D3DTOP_MODULATE4X;
	}
}

typedef void(*fxTexUpdate_t)(void *fx, CMap *map);
typedef void *(*fxTexGet_t)(void *fx);

int Render_SetMap(uint32_t stage, CMap *map) {
	CHECK(g_pd3dDevice);
	CHECK(stage < 8);
	void *tex = NULL;
	if (map) {
		if (map->AnimMat) {
			uint32_t f = (uint32_t)CAnimMat_GetFrame(map->AnimMat);
			map->cur = f < (uint32_t)map->frames ? &map->TextAni[f] : &map->staticTex;
		}
		if (map->FXTexture) {
			void *fx = map->FXTexture;
			((fxTexUpdate_t)(*(void ***)fx)[1])(fx, map);
			fx = map->FXTexture;
			tex = ((fxTexGet_t)(*(void ***)fx)[2])(fx);
		}
		if (map->filter != g_StageMip[stage]) {
			g_StageMip[stage] = map->filter;
			Render_TSS(stage, D3DTSS_MIPFILTER, map->filter && g_MipmapsOn ? g_FilterMip : 0);
		}
		if (map->tile != g_StageTile[stage]) {
			g_StageTile[stage] = map->tile;
			Render_TSS(stage, D3DTSS_ADDRESSU, (~((uint32_t)map->tile * 2) & 2) | 1);
			Render_TSS(stage, D3DTSS_ADDRESSV, (~(uint32_t)map->tile & 2) | 1);
		}
		Render_TSS(stage, D3DTSS_MAXANISOTROPY, map->lightmap ? 1 : g_AnisotropyCur);
		if (!tex) {
			tex = CMap_GetTexture(map, 1);
		}
	}
	if (g_StageTex[stage] != tex || g_StageTexFrame[stage] != g_FrameCount) {
		g_StageTexFrame[stage] = g_FrameCount;
		g_StageTex[stage] = tex;
		Render_SetTexture(stage, tex);
		if (tex) {
			g_TexChanges++;
		}
	}
	return 0;
}

/* the vertex format, the stream and the indices of a draw */
static void Render_Stream(void *dev, void *vb, uint32_t fvf, int size, void *ib, uint32_t baseVert) {
	Rd_DevSetVertexShader(dev, fvf);
	Rd_DevSetStreamSource(dev, 0, vb, (uint32_t)size);
	if (ib) {
		Rd_DevSetIndices(dev, ib, baseVert);
	}
}

int Render_DrawVBIBFmt(void *vb, void *ib, int frmt, uint32_t numVerts, uint32_t ntri, uint32_t baseVert,
	uint32_t startTri) {
	void *dev = g_pd3dDevice;
	if (!dev) {
		return E_FAIL;
	}
	Render_Stream(dev, vb, VertList_FormatFVF(frmt), VertList_FormatSize(frmt), ib, baseVert);
	Rd_DevDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, numVerts,
		startTri * 3, ntri);
	return 0;
}

int Render_DrawMeshD3D(const CMeshD3D *m) {
	void *dev = g_pd3dDevice;
	if (!dev) {
		return E_FAIL;
	}
	if (m->StripList) {
		CHECK(m->StripList > 2);
		CHECK(m->pVertVB && !m->pVertIB);
		CHECK(!m->BaseIndex && !m->StartIndex);
		Render_Stream(dev, m->pVertVB, VertList_FormatFVF(m->vbFrmt), VertList_FormatSize(m->vbFrmt), NULL, 0);
		Rd_DevDrawPrimitive(dev, D3DPT_TRIANGLESTRIP, 0,
			(uint32_t)m->StripList - 2);
		return 0;
	}
	CHECK(m->pVertVB && m->pVertIB);
	uint32_t ntri = m->nTri;
	if (g_VBIBTriLimit < ntri) {
		ntri = g_VBIBTriLimit;
	}
	Render_Stream(dev, m->pVertVB, VertList_FormatFVF(m->vbFrmt), VertList_FormatSize(m->vbFrmt), m->pVertIB,
		(uint32_t)m->BaseIndex);
	Rd_DevDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0,
		(uint32_t)m->vert.nVert, (uint32_t)m->StartIndex * 3, ntri);
	return 0;
}

int Render_DrawVBIB(void *vb, void *ib, const CVertList *v, uint32_t ntri) {
	void *dev = g_Engine->device;
	if (!dev) {
		return E_FAIL;
	}
	Render_Stream(dev, vb, CVertList_FVF((CVertList *)v), CVertList_Size((CVertList *)v), ib, 0);
	Rd_DevDrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0,
		(uint32_t)v->nVert, 0, ntri);
	return 0;
}

int Render_SetView(const matrix_t *m) {
	matrix_t inv;
	g_MatView = *m;
	CHECK(Matrix_Invert(&inv, &g_MatView) == 0);
	g_MatBillboard = inv;
	g_EyePos = (vec3_t){ inv.m[12], inv.m[13], inv.m[14] };
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	void *dev = g_pd3dDevice;
	Rd_DevSetTransform(dev, D3DTS_VIEW, &g_MatView);
	return 0;
}

void E3D_FlushBatch(void) {
	if (g_ViewportW) {
		RenderMeshD3D_Flush();
		RenderMeshD3D_Flush2();
		RenderObjBase3D_Flush();
	}
}

byte Box_IsVisible(const matrix_t *m, const box_t *box, const char *name, float scale) {
	box_t wb;
	Box_Transform(&wb, box, m);
	vec3_t c = { (wb.min.x + wb.max.x) * 0.5f, (wb.max.y + wb.min.y) * 0.5f, (wb.max.z + wb.min.z) * 0.5f };
	float dx = wb.max.x - wb.min.x, dy = wb.max.y - wb.min.y, dz = wb.max.z - wb.min.z;
	float r = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f * scale;
	if (!(r > 0.0f)) {
		vec3_t pos = { m->m[12], m->m[13], m->m[14] };
		/* the center x where the radius is meant, as the original */
		E3D_LogBlue(g_FileLog, "ERROR: Invalid radius: %f  Model: %s  Pos: %s", (double)c.x, name, Vec3_Str(&pos, 0, 6));
		CHECK(r > 0.0f);
	}
	int out;
	Frustum_TestSpheres(&c, &r, 1, &out);
	if (out) {
		return 0;
	}
	if (g_ShowSphereVis) {
		Debug_DrawBoxM(m, &box->min, &box->max, 0xffffff);
		Debug_DrawBox(&wb.min, &wb.max, 0xffff00);
		Debug_DrawSphere(&c, r, 0xff00, 10, -1);
	}
	return 1;
}

byte Frustum_SegVisible(const vec3_t *a, const vec3_t *b) {
	return Frustum_TestSeg(a, b) == 0;
}

byte Sph_IsVisible(const vec3_t *center, float radius, int arg) {
	(void)arg;
	vec3_t c = *center;
	CHECK(!(radius <= 0.0f));
	int out;
	Frustum_TestSpheres(&c, &radius, 1, &out);
	if (out) {
		return 0;
	}
	if (g_ShowSphereVis > 1) {
		Debug_DrawSphere(&c, radius, 0xff, 10, -1);
	}
	return 1;
}

/* n bytes of src into a new buffer (a vertex or index buffer, the same methods) */
static int Buf_Fill(void *buf, const void *src, uint32_t lockSize, uint32_t n) {
	void *p;
	if (Rd_BufLock(buf, 0, lockSize, &p, 0) < 0) {
		return E_FAIL;
	}
	memcpy(p, src, n);
	Rd_BufUnlock(buf);
	return 0;
}

int Render_CreateIB(void **ib, const void *tris, int nTri, byte dynamic) {
	CHECK(ib);
	CHECK(tris);
	CHECK(nTri > 0);
	uint32_t n = (uint32_t)nTri * 6;
	void *dev = g_Engine->device;
	if (Rd_DevCreateIndexBuffer(dev, n, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
		dynamic ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT, ib) < 0) {
		return E_FAIL;
	}
	return Buf_Fill(*ib, tris, n, n);
}

int Render_CreateVB(void **vb, CVertList *vert, byte dynamic) {
	CHECK(vb);
	CHECK(vert);
	CHECK(vert->nVert > 0);
	int n = vert->nVert;
	void *dev = g_Engine->device;
	uint32_t fvf = CVertList_FVF(vert);
	if (Rd_DevCreateVertexBuffer(dev, (uint32_t)(CVertList_Size(vert) * n), D3DUSAGE_WRITEONLY,
		fvf, dynamic ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT, vb) < 0) {
		return E_FAIL;
	}
	return Buf_Fill(*vb, vert->pVert, 0, (uint32_t)(CVertList_Size(vert) * vert->nVert));
}

const char *Render_VersionStr(uint32_t v) {
	Bprintf(g_VersionStr, 0x40, "%d.%d", v >> 8 & 0xff, v & 0xff);
	return g_VersionStr;
}

void Render_SetCamera(CCamera *cam) {
	matrix_t m;
	Render_SetView(CCamera_GetView(cam, &m));
	g_MatProj = *CCamera_GetProj(cam, &m);
	void *dev = g_pd3dDevice;
	if (dev) {
		Rd_DevSetTransform(dev, D3DTS_PROJECTION, &g_MatProj);
	}
	Frustum_Calc(&g_MatView, &g_MatProj);
}

byte Render_ClipRect(int *out, const uint32_t *vp, const box_t *rect) {
	CHECK(out);
	int w = (int)vp[2], h = (int)vp[3];
	float fw = (float)(double)vp[2], fx = (float)(double)vp[0];
	float fh = (float)(double)vp[3], fy = (float)(double)vp[1];
	int x0 = (int)((double)(fw * rect->min.x + fx) + 0.5);
	int y0 = (int)((double)(rect->min.y * fh + fy) + 0.5);
	int x1 = (int)((double)(rect->max.x * fw + fx) + 0.5);
	int y1 = (int)((double)(rect->max.y * fh + fy) + 0.5);
	x0 = x0 > 0 ? (x0 >= w ? w : x0) : 0;
	y0 = y0 > 0 ? (y0 >= h ? h : y0) : 0;
	x1 = x1 > 0 ? (x1 >= w ? w : x1) : 0;
	y1 = y1 > 0 ? (y1 >= h ? h : y1) : 0;
	if (x1 > x0 && y1 > y0) {
		out[0] = x0;
		out[1] = y0;
		out[2] = x1;
		out[3] = y1;
		return 1;
	}
	out[2] = x0;
	out[0] = x0;
	out[1] = y0;
	out[3] = y0;
	return 0;
}

void Render_NoIndexedBlend(void *dev) {
	Rd_DevSetRenderState(dev, D3DRS_INDEXEDVERTEXBLENDENABLE, 0);
}

byte Render_SetViewRect(const box_t *rect, byte proj) {
	E3D_FlushBatch();
	void *dev = g_pd3dDevice;
	if (!rect) {
		matrix_t m = g_MatProj;
		Rd_DevSetTransform(dev, D3DTS_PROJECTION, &m);
		dev = g_pd3dDevice;
		Rd_DevSetViewport(dev, g_Viewport);
		return 1;
	}
	uint32_t vp[6];
	memcpy(vp, g_Viewport, sizeof(vp));
	int w = (int)vp[2], h = (int)vp[3];
	int r[4];
	Render_ClipRect(r, vp, rect);
	vp[2] = (uint32_t)(r[2] - r[0]);
	vp[3] = (uint32_t)(r[3] - r[1]);
	vp[0] = (uint32_t)r[0];
	vp[1] = (uint32_t)r[1];
	dev = g_pd3dDevice;
	Rd_DevSetViewport(dev, vp);
	if (proj) {
		float fw = (float)w, fh = (float)h;
		matrix_t m;
		Proj_Sub(&m, &g_MatProj, (float)r[0] / fw - 0.5f, (float)r[2] / fw - 0.5f,
			Float_Keep(-((float)r[1] / fh - 0.5f)), Float_Keep(-((float)r[3] / fh - 0.5f)));
		dev = g_pd3dDevice;
		Rd_DevSetTransform(dev, D3DTS_PROJECTION, &m);
	}
	return 1;
}

int Render_GetViewportH(void) {
	CHECK(g_ViewportH);
	return g_ViewportH;
}

int Render_GetViewportW(void) {
	CHECK(g_ViewportW);
	return g_ViewportW;
}

int Render_StencilBits(void) {
	switch (g_Engine->depthBufferDesc.Format) {
	case D3DFMT_D15S1:
		return 1;
	case D3DFMT_D24S8:
		return 8;
	case D3DFMT_D24X4S4:
		return 4;
	default:
		return 0;
	}
}

int Render_Line(const vec3_t *a, const vec3_t *b, uint32_t color) {
	if (!g_Engine->device) {
		return E_FAIL;
	}
	if (g_DiffuseMod4x) {
		color = (color >> 2 & 0x3f3f3f) | (color & 0xff000000);
	}
	const struct {
		vec3_t p;
		uint32_t color;
	} v[2] = { { *a, color }, { *b, color } };
	Render_SetMaterial(g_MatLine, 0);
	void *dev = g_pd3dDevice;
	Rd_DevSetVertexShader(dev, D3DFVF_XYZ | D3DFVF_DIFFUSE);
	dev = g_pd3dDevice;
	Rd_DevDrawPrimitiveUP(dev, D3DPT_LINELIST, 1, v, sizeof(v[0]));
	return 0;
}

int Render_ToScreen(const vec3_t *pos, float *xy) {
	CHECK(g_pd3dDevice);
	CHECK(xy);
	matrix_t vp, m;
	m = *Matrix_MulD3D(&vp, &g_MatView, &g_MatProj);
	vec3_t p;
	if (Vec3_TransformH(&p, pos, &m)) {
		return -1;
	}
	if (0.02 > (double)p.z || p.z > 1.0f || __builtin_fabs((double)p.x) > 1.0 || __builtin_fabs((double)p.y) > 1.0) {
		return -1;
	}
	float w = (float)(double)g_Viewport[2], x = (float)(double)g_Viewport[0];
	xy[0] = (w * p.x + x) * 0.5f + w * 0.5f;
	float h = (float)(double)g_Viewport[3], y = (float)(double)g_Viewport[1];
	xy[1] = h * 0.5f - (h * p.y + y) * 0.5f;
	return 0;
}

/* the plane through a, b, c: normal (b - a) x (c - a), normalized unless it is 0 */
static void Plane_From3(plane_t *out, const vec3_t *a, const vec3_t *b, const vec3_t *c) {
	vec3_t u = { b->x - a->x, b->y - a->y, b->z - a->z };
	vec3_t v = { c->x - a->x, c->y - a->y, c->z - a->z };
	vec3_t n = { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };
	float len = (float)__builtin_sqrt((double)(n.y * n.y + n.x * n.x + n.z * n.z));
	if ((double)len != 0.0) {
		float k = (float)(1.0 / (double)len);
		n = (vec3_t){ n.x * k, n.y * k, n.z * k };
	}
	out->n = n;
	out->d = -(n.y * a->y + n.x * a->x + n.z * a->z);
}

void Frustum_Calc(const matrix_t *view, const matrix_t *proj) {
	/* the corners of the clip space: near (z 0) then far, each from top left on */
	vec3_t c[8] = {
		{ -1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f },
		{ -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { -1.0f, -1.0f, 1.0f },
	};
	static const byte planes[MAX_PLANES_FRUSTUM][3] = {
		{ 4, 6, 5 }, { 0, 1, 2 }, { 0, 3, 4 }, { 1, 5, 2 }, { 0, 4, 1 }, { 2, 6, 3 },
	};
	matrix_t tmp, m, inv;
	m = *Matrix_MulD3D(&tmp, view, proj);
	if (!D3DX_MatrixInverse(&inv, NULL, &m)) {
		E3D_LogDebug(g_FileLog, "  ERROR: UpdateVisibilityPlanes(): Could not calculate the inverse matrix");
		return;
	}
	for (int i = 0; i < 8; i++) {
		Vec3_TransformH(&c[i], &c[i], &inv);
	}
	plane_t *fr = (plane_t *)g_Frustum;
	for (int i = 0; i < MAX_PLANES_FRUSTUM; i++) {
		Plane_From3(&fr[i], &c[planes[i][0]], &c[planes[i][1]], &c[planes[i][2]]);
	}
}

int Render_MeshD3D(CMeshD3D *m, CRenderNode *r) {
	CHECK(m);
	/* the shadow pass of a model being drawn with stencil shadows */
	if (r && ((CModel *)r->owner)->visible == 2) {
		CHECK(g_RenderPass == 1 || g_RenderPass == 11);
		if (m->pSkin) {
			RenderEVA_DrawSkin(m, r);
		} else if (r->skin) {
			RenderEVA_DrawSkinNode(m, r, r->skin);
		} else {
			Stencil_Mesh(&m->vert, m->pTri, m->nTri, m, r);
		}
		return 0;
	}
	if (g_RenderOnlyB38 && m->mat && !m->mat->params.twoSided) {
		return 0;
	}
	void *dev = g_Engine->device;
	if (!dev) {
		return E_FAIL;
	}
	byte pass1 = 0, envSoft = 0;
	if (m->mat && m->mat->params.reflection) {
		pass1 = 1;
		if (!g_UseHWEnvMap) {
			matrix_t view, world;
			Rd_DevGetTransform(dev, D3DTS_VIEW, &view);
			Rd_DevGetTransform(dev, D3DTS_WORLD, &world);
			CVertList_EnvMapUV(&m->vert, &world, &view, m->mat->map[2], 1);
			envSoft = 1;
		}
	}
	Render_SetMaterial(m->mat, 0);
	CHECK(!m->weight);
	if (m->pSkin) {
		RenderEVA_DrawSkin(m, r);
	} else if (r && r->skin) {
		RenderEVA_DrawSkinNode(m, r, r->skin);
	} else if (m->weight2) {
		CHECK(0);
	} else {
		if (m->pVertVB && g_UseVB && !envSoft) {
			Render_DrawMeshD3D(m);
		} else {
			CHECK(0);
			Render_DrawListIdx(&m->vert, (const uint16_t *)m->pTri, (int)m->nTri, 0);
		}
		Stencil_Mesh(&m->vert, m->pTri, m->nTri, m, r);
	}
	if (pass1 && !g_UseShaders) {
		Render_SetMaterial(m->mat, 1);
		Render_DrawListIdx(&m->vert, (const uint16_t *)m->pTri, (int)m->nTri, 0);
	}
	g_RenderTris += (int)m->nTri;
	g_RenderVerts += m->vert.nVert;
	return 0;
}
