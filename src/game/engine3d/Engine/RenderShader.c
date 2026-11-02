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

SD_SYM(str_3d9104);
SD_SYM(Render_PSFixed);
SD_SYM(str_3d910c);
SD_SYM(str_3d8f68);
SD_SYM(Shader_Envmap);
SD_SYM(str_3d9114);
SD_SYM(str_3d8ec8);
SD_SYM(Shader_MaskEnvmap);
SD_SYM(str_3d9120);
SD_SYM(str_3d8eb4);
SD_SYM(Shader_MaskEnvBump);
SD_SYM(str_3d912c);
SD_SYM(str_3d8f90);
SD_SYM(Shader_Glowmap);
SD_SYM(str_3d9134);
SD_SYM(str_3d8edc);
SD_SYM(Shader_GlowmapMaskEnvmap);
SD_SYM(str_3d9148);
SD_SYM(str_3d8e98);
SD_SYM(Shader_GlowmapMaskEnvBump);
SD_SYM(str_3d915c);
SD_SYM(str_3d8f54);
SD_SYM(Shader_Clouds);
SD_SYM(str_3d9164);
SD_SYM(str_3d8fb8);
SD_SYM(Shader_Hologram);
SD_SYM(str_3d9170);
SD_SYM(str_3d917c);
SD_SYM(str_3d8fe8);
SD_SYM(Shader_Electric);
SD_SYM(str_3bbe9c);
SD_SYM(str_3d8ffc);
SD_SYM(Shader_Fire);
SD_SYM(str_3d9188);
SD_SYM(str_3d900c);
SD_SYM(Shader_Glass);
SD_SYM(str_3d9190);
SD_SYM(str_3d901c);
SD_SYM(Shader_Waves);
SD_SYM(str_3d9198);
SD_SYM(Shader_Scroll);
SD_SYM(str_3c2ddc);
SD_SYM(str_3d902c);
SD_SYM(Shader_Noise);
SD_SYM(str_3d91a0);
SD_SYM(str_3d903c);
SD_SYM(Render_PS0);
SD_SYM(str_3d91ac);
SD_SYM(str_3d9050);
SD_SYM(Render_PS1);
SD_SYM(str_3d91b8);
SD_SYM(str_3d9064);
SD_SYM(Render_PS2);
SD_SYM(str_3d91c4);
SD_SYM(str_3d9078);
SD_SYM(Render_PS3);
SD_SYM(str_3d91d4);
SD_SYM(str_3d9090);
SD_SYM(Render_PS4);
SD_SYM(str_3d91e8);
SD_SYM(str_3d90ac);
SD_SYM(Render_PS5);
SD_SYM(str_3d91f4);
SD_SYM(str_3d90c0);
SD_SYM(Render_PS6);
SD_SYM(str_3d9200);
SD_SYM(str_3d90d4);
SD_SYM(Render_PS7);
SD_SYM(str_3d920c);
SD_SYM(str_3d90e8);
SD_SYM(Render_PS8);
SD_SYM(str_3d9220);
SD_SYM(Shader_DiffuseNoLit);
ShaderTab g_ShaderTab[32] = {
	{ .Name = (void *)sd_str_3d9104, .flags = 257, .fnRender = (void *)sd_Render_PSFixed },
	{
		.Index = 1,
		.Name = (void *)sd_str_3d910c,
		.File = (void *)sd_str_3d8f68,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Envmap,
	},
	{
		.Index = 2,
		.Name = (void *)sd_str_3d9114,
		.File = (void *)sd_str_3d8ec8,
		.flags = 256,
		.fnRender = (void *)sd_Shader_MaskEnvmap,
	},
	{
		.Index = 3,
		.Name = (void *)sd_str_3d9120,
		.File = (void *)sd_str_3d8eb4,
		.flags = 256,
		.fnRender = (void *)sd_Shader_MaskEnvBump,
	},
	{
		.Index = 4,
		.Name = (void *)sd_str_3d912c,
		.File = (void *)sd_str_3d8f90,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Glowmap,
	},
	{
		.Index = 5,
		.Name = (void *)sd_str_3d9134,
		.File = (void *)sd_str_3d8edc,
		.flags = 256,
		.fnRender = (void *)sd_Shader_GlowmapMaskEnvmap,
	},
	{
		.Index = 6,
		.Name = (void *)sd_str_3d9148,
		.File = (void *)sd_str_3d8e98,
		.flags = 256,
		.fnRender = (void *)sd_Shader_GlowmapMaskEnvBump,
	},
	{
		.Index = 7,
		.Name = (void *)sd_str_3d915c,
		.File = (void *)sd_str_3d8f54,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Clouds,
	},
	{
		.Index = 8,
		.Name = (void *)sd_str_3d9164,
		.File = (void *)sd_str_3d8fb8,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Hologram,
	},
	{ .Index = 9, .Name = (void *)sd_str_3d9170, .flags = 257, .fnRender = (void *)sd_Shader_Glowmap },
	{
		.Index = 10,
		.Name = (void *)sd_str_3d917c,
		.File = (void *)sd_str_3d8fe8,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Electric,
	},
	{
		.Index = 11,
		.Name = (void *)sd_str_3bbe9c,
		.File = (void *)sd_str_3d8ffc,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Fire,
	},
	{
		.Index = 12,
		.Name = (void *)sd_str_3d9188,
		.File = (void *)sd_str_3d900c,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Glass,
	},
	{
		.Index = 13,
		.Name = (void *)sd_str_3d9190,
		.File = (void *)sd_str_3d901c,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Waves,
	},
	{ .Index = 14, .Name = (void *)sd_str_3d9198, .flags = 257, .fnRender = (void *)sd_Shader_Scroll },
	{
		.Index = 15,
		.Name = (void *)sd_str_3c2ddc,
		.File = (void *)sd_str_3d902c,
		.flags = 257,
		.fnRender = (void *)sd_Shader_Noise,
	},
	{
		.Index = 16,
		.Name = (void *)sd_str_3d91a0,
		.File = (void *)sd_str_3d903c,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS0,
	},
	{
		.Index = 17,
		.Name = (void *)sd_str_3d91ac,
		.File = (void *)sd_str_3d9050,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS1,
	},
	{
		.Index = 18,
		.Name = (void *)sd_str_3d91b8,
		.File = (void *)sd_str_3d9064,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS2,
	},
	{
		.Index = 19,
		.Name = (void *)sd_str_3d91c4,
		.File = (void *)sd_str_3d9078,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS3,
	},
	{
		.Index = 20,
		.Name = (void *)sd_str_3d91d4,
		.File = (void *)sd_str_3d9090,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS4,
	},
	{
		.Index = 21,
		.Name = (void *)sd_str_3d91e8,
		.File = (void *)sd_str_3d90ac,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS5,
	},
	{
		.Index = 22,
		.Name = (void *)sd_str_3d91f4,
		.File = (void *)sd_str_3d90c0,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS6,
	},
	{
		.Index = 23,
		.Name = (void *)sd_str_3d9200,
		.File = (void *)sd_str_3d90d4,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS7,
	},
	{
		.Index = 24,
		.Name = (void *)sd_str_3d920c,
		.File = (void *)sd_str_3d90e8,
		.flags = 257,
		.fnRender = (void *)sd_Render_PS8,
	},
	{ .Index = 25, .Name = (void *)sd_str_3d9220, .flags = 257, .fnRender = (void *)sd_Shader_DiffuseNoLit },
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

const char str_3bbe9c[] = "Fire";

const char str_3c2ddc[] = "Noise";

const char str_3d9104[] = "Diffuse";

const char str_3d910c[] = "Envmap";

const char str_3d9114[] = "MaskEnvmap";

const char str_3d9120[] = "MaskEnvBump";

const char str_3d912c[] = "Glowmap";

const char str_3d9134[] = "GlowmapMaskEnvmap";

const char str_3d9148[] = "GlowmapMaskEnvBump";

const char str_3d915c[] = "Clouds";

const char str_3d9164[] = "Hologram";

const char str_3d9170[] = "GlowFlick";

const char str_3d917c[] = "Electric";

const char str_3d9188[] = "Glass";

const char str_3d9190[] = "Waves";

const char str_3d9198[] = "Scroll";

const char str_3d91a0[] = "BloomFilter";

const char str_3d91ac[] = "BloomBlur";

const char str_3d91b8[] = "BloomTarget";

const char str_3d91c4[] = "MotionBlurAdd";

const char str_3d91d4[] = "MotionBlurTarget";

const char str_3d91e8[] = "BlurTarget";

const char str_3d91f4[] = "DUDVFilter";

const char str_3d9200[] = "DUDVTarget";

const char str_3d920c[] = "RadialBlurTarget";

const char str_3d9220[] = "DiffuseNoLit";

