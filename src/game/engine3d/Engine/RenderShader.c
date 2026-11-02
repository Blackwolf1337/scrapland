/*
 * The shader modes of the materials, original source Engine/RenderShader.cpp (include/scrap/render.h,
 * include/scrap/material.h): a material gets the mode of its maps and flags (Diffuse, EnvMap, Glowmap and the masked
 * reflection modes, or the one its name asks for) and the bytes of the mode the renderer needs; it is registered
 * again when the features switched on (the shader mode mask) change.
 */
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/material.h"
#include "scrap/render.h"
#include "scrap/misc.h"

extern cGVar g_GvGlowMap;

#define g_GlowMap (*(int *)&g_GvGlowMap.value)		/* R_GlowMap */
extern uint32_t g_ShaderModeMask;
#define SHADER_NAME 0x40

CMatId *CMatId_ctor(CMatId *self) {
	self->id = -1;
	self->gen = 0;
	return self;
}

void CMatId_dtor(CMatId *self) {
	(void)self;
}

byte CMatId_Check(CMatId *self, CMaterial *mat) {
	if (self->id == -1) {
		CHECK(!mat->map[MAP_DIFFUSE]);
		E3D_LogWhite((&g_FileLog), "GetShaderMode: Deferred update: %s", mat->Name);
		CMatId_Register(self, mat);
		return 0;
	}
	if (self->gen != (int)g_ShaderModeMask) {
		self->id = -1;
		CMatId_Register(self, mat);
		return 0;
	}
	return 1;
}

int Shader_Find(const char *name) {
	for (int i = 0; g_ShaderTab[i].Name; i++) {
		if (!crt_stricmp(g_ShaderTab[i].Name, name)) {
			return i;
		}
	}
	Err_Printf(0, "Shader not found: %s", name);
	return 0;
}

void CMatId_Register(CMatId *self, CMaterial *mat) {
	char buf[SHADER_NAME];
	const char *name;

	CMap *bump = g_EnvBump ? mat->map[MAP_BUMP] : NULL;
	CMap *glow = g_GlowMap ? mat->map[MAP_GLOW] : NULL;
	byte alpha = mat->map[MAP_DIFFUSE] && mat->map[MAP_DIFFUSE]->hasAlpha;
	byte env = 0;
	byte mask = 0;
	if (g_EnvMap) {
		env = mat->params.reflection;
		mask = env && alpha;
	}
	if (glow) {
		if (mask) {
			name = bump ? "GlowmapMaskEnvBump" : "GlowmapMaskEnvmap";
		} else {
			name = "Glowmap";
		}
	} else if (mask) {
		name = bump ? "MaskEnvBump" : "MaskEnvmap";
	} else {
		name = env ? "EnvMap" : "Diffuse";
	}
	if (mat->params.transp && mat->params.srcBlend == 1 && mat->params.dstBlend == 3 && (mat->params.attrib & 0x800)) {
		name = "DiffuseNoLit";
	}
	int mode = Shader_Find(CMaterial_GetShaderName(mat, buf, sizeof(buf)) ? buf : name);
	CHECK(self->id == -1);
	self->id = mode;
	if (mat->params.reflection && !(mat->params.attrib & MAT_SHADER)) {
		alpha = 0;
	}
	if (!(mat->params.attrib & MAT_SHADER) && (glow || bump) && env && !mask) {
		E3D_LogInfo((&g_FileLog), "Invalid shader for material: %s", mat->Name);
	}
	CHECK(mode < MAX_SHADER_TAB);
	self->alphaTest = (byte)g_ShaderTab[mode].flags;
	self->blend = (byte)(g_ShaderTab[mode].flags >> 8);
	self->alpha = alpha;
	RenderGamma_LogShaderMask();
	self->gen = (int)g_ShaderModeMask;
}

void RenderGamma_LogShaderMask(void) {
	uint32_t m = (g_GlowMap ? 8 : 0) | (g_EnvBump ? 4 : 0) | (g_EnvMap ? 2 : 0) | (g_PSOn != 0);

	if (g_ShaderModeMask != m) {
		g_ShaderModeMask = m;
		E3D_LogWhite((&g_FileLog), "ShaderModeMask: %04X", m);
	}
}
