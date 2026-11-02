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
#define SHADER_NAME 0x40

CMatId *CMatId_ctor(CMatId *self) {
	self->id = -1;
	self->gen = 0;
	return self;
}

void CMatId_dtor(CMatId *self) {
	(void)self;
}

uint32_t g_ShaderModeMask;

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

extern const char str_Fire[];
extern const char str_Noise[];
extern const char str_BmpGlowmapMaskEnvBumpPsh[];
extern const char str_BmpMaskEnvBumpPsh[];
extern const char str_BmpMaskEnvmapPsh[];
extern const char str_BmpGlowmapMaskEnvmapPsh[];
extern const char str_BmpCloudTestPsh[];
extern const char str_BmpEnvmapPsh[];
extern const char str_BmpGlowmapPsh[];
extern const char str_BmpHologramPsh[];
extern const char str_BmpElectricPsh[];
extern const char str_BmpFirePsh[];
extern const char str_BmpGlassPsh[];
extern const char str_BmpWavesPsh[];
extern const char str_BmpNoisePsh[];
extern const char str_BmpBloomFilterPsh[];
extern const char str_BmpBloomBlurPsh[];
extern const char str_BmpBloomTargetPsh[];
extern const char str_BmpMotionBlurAddPsh[];
extern const char str_BmpMotionBlurTargetPsh[];
extern const char str_BmpBlurTargetPsh[];
extern const char str_BmpDUDVFilterPsh[];
extern const char str_BmpDUDVTargetPsh[];
extern const char str_BmpRadialBlurTargetPsh[];
extern const char str_Diffuse[];
extern const char str_Envmap[];
extern const char str_MaskEnvmap[];
extern const char str_MaskEnvBump[];
extern const char str_Glowmap[];
extern const char str_GlowmapMaskEnvmap[];
extern const char str_GlowmapMaskEnvBump[];
extern const char str_Clouds[];
extern const char str_Hologram[];
extern const char str_GlowFlick[];
extern const char str_Electric[];
extern const char str_Glass[];
extern const char str_Waves[];
extern const char str_Scroll[];
extern const char str_BloomFilter[];
extern const char str_BloomBlur[];
extern const char str_BloomTarget[];
extern const char str_MotionBlurAdd[];
extern const char str_MotionBlurTarget[];
extern const char str_BlurTarget[];
extern const char str_DUDVFilter[];
extern const char str_DUDVTarget[];
extern const char str_RadialBlurTarget[];
extern const char str_DiffuseNoLit[];
ShaderTab g_ShaderTab[32] = {
	{ .Name = (void *)str_Diffuse, .flags = 257, .fnRender = (void *)Render_PSFixed },
	{
		.Index = 1,
		.Name = (void *)str_Envmap,
		.File = (void *)str_BmpEnvmapPsh,
		.flags = 257,
		.fnRender = (void *)Shader_Envmap,
	},
	{
		.Index = 2,
		.Name = (void *)str_MaskEnvmap,
		.File = (void *)str_BmpMaskEnvmapPsh,
		.flags = 256,
		.fnRender = (void *)Shader_MaskEnvmap,
	},
	{
		.Index = 3,
		.Name = (void *)str_MaskEnvBump,
		.File = (void *)str_BmpMaskEnvBumpPsh,
		.flags = 256,
		.fnRender = (void *)Shader_MaskEnvBump,
	},
	{
		.Index = 4,
		.Name = (void *)str_Glowmap,
		.File = (void *)str_BmpGlowmapPsh,
		.flags = 257,
		.fnRender = (void *)Shader_Glowmap,
	},
	{
		.Index = 5,
		.Name = (void *)str_GlowmapMaskEnvmap,
		.File = (void *)str_BmpGlowmapMaskEnvmapPsh,
		.flags = 256,
		.fnRender = (void *)Shader_GlowmapMaskEnvmap,
	},
	{
		.Index = 6,
		.Name = (void *)str_GlowmapMaskEnvBump,
		.File = (void *)str_BmpGlowmapMaskEnvBumpPsh,
		.flags = 256,
		.fnRender = (void *)Shader_GlowmapMaskEnvBump,
	},
	{
		.Index = 7,
		.Name = (void *)str_Clouds,
		.File = (void *)str_BmpCloudTestPsh,
		.flags = 257,
		.fnRender = (void *)Shader_Clouds,
	},
	{
		.Index = 8,
		.Name = (void *)str_Hologram,
		.File = (void *)str_BmpHologramPsh,
		.flags = 257,
		.fnRender = (void *)Shader_Hologram,
	},
	{ .Index = 9, .Name = (void *)str_GlowFlick, .flags = 257, .fnRender = (void *)Shader_Glowmap },
	{
		.Index = 10,
		.Name = (void *)str_Electric,
		.File = (void *)str_BmpElectricPsh,
		.flags = 257,
		.fnRender = (void *)Shader_Electric,
	},
	{
		.Index = 11,
		.Name = (void *)str_Fire,
		.File = (void *)str_BmpFirePsh,
		.flags = 257,
		.fnRender = (void *)Shader_Fire,
	},
	{
		.Index = 12,
		.Name = (void *)str_Glass,
		.File = (void *)str_BmpGlassPsh,
		.flags = 257,
		.fnRender = (void *)Shader_Glass,
	},
	{
		.Index = 13,
		.Name = (void *)str_Waves,
		.File = (void *)str_BmpWavesPsh,
		.flags = 257,
		.fnRender = (void *)Shader_Waves,
	},
	{ .Index = 14, .Name = (void *)str_Scroll, .flags = 257, .fnRender = (void *)Shader_Scroll },
	{
		.Index = 15,
		.Name = (void *)str_Noise,
		.File = (void *)str_BmpNoisePsh,
		.flags = 257,
		.fnRender = (void *)Shader_Noise,
	},
	{
		.Index = 16,
		.Name = (void *)str_BloomFilter,
		.File = (void *)str_BmpBloomFilterPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS0,
	},
	{
		.Index = 17,
		.Name = (void *)str_BloomBlur,
		.File = (void *)str_BmpBloomBlurPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS1,
	},
	{
		.Index = 18,
		.Name = (void *)str_BloomTarget,
		.File = (void *)str_BmpBloomTargetPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS2,
	},
	{
		.Index = 19,
		.Name = (void *)str_MotionBlurAdd,
		.File = (void *)str_BmpMotionBlurAddPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS3,
	},
	{
		.Index = 20,
		.Name = (void *)str_MotionBlurTarget,
		.File = (void *)str_BmpMotionBlurTargetPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS4,
	},
	{
		.Index = 21,
		.Name = (void *)str_BlurTarget,
		.File = (void *)str_BmpBlurTargetPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS5,
	},
	{
		.Index = 22,
		.Name = (void *)str_DUDVFilter,
		.File = (void *)str_BmpDUDVFilterPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS6,
	},
	{
		.Index = 23,
		.Name = (void *)str_DUDVTarget,
		.File = (void *)str_BmpDUDVTargetPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS7,
	},
	{
		.Index = 24,
		.Name = (void *)str_RadialBlurTarget,
		.File = (void *)str_BmpRadialBlurTargetPsh,
		.flags = 257,
		.fnRender = (void *)Render_PS8,
	},
	{ .Index = 25, .Name = (void *)str_DiffuseNoLit, .flags = 257, .fnRender = (void *)Shader_DiffuseNoLit },
};

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

const char str_Fire[] = "Fire";

const char str_Noise[] = "Noise";

const char str_Diffuse[] = "Diffuse";

const char str_Envmap[] = "Envmap";

const char str_MaskEnvmap[] = "MaskEnvmap";

const char str_MaskEnvBump[] = "MaskEnvBump";

const char str_Glowmap[] = "Glowmap";

const char str_GlowmapMaskEnvmap[] = "GlowmapMaskEnvmap";

const char str_GlowmapMaskEnvBump[] = "GlowmapMaskEnvBump";

const char str_Clouds[] = "Clouds";

const char str_Hologram[] = "Hologram";

const char str_GlowFlick[] = "GlowFlick";

const char str_Electric[] = "Electric";

const char str_Glass[] = "Glass";

const char str_Waves[] = "Waves";

const char str_Scroll[] = "Scroll";

const char str_BloomFilter[] = "BloomFilter";

const char str_BloomBlur[] = "BloomBlur";

const char str_BloomTarget[] = "BloomTarget";

const char str_MotionBlurAdd[] = "MotionBlurAdd";

const char str_MotionBlurTarget[] = "MotionBlurTarget";

const char str_BlurTarget[] = "BlurTarget";

const char str_DUDVFilter[] = "DUDVFilter";

const char str_DUDVTarget[] = "DUDVTarget";

const char str_RadialBlurTarget[] = "RadialBlurTarget";

const char str_DiffuseNoLit[] = "DiffuseNoLit";

