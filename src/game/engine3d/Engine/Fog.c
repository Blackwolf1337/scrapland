/*
 * Engine/niebla.cpp of the engine3d: CFog, see include/scrap/fog.h.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/e3dworld.h"
#include "scrap/misc.h"
#include "scrap/staticinit.h"

#define g_FogColor (GVar_R_FogColor.value.i)	/* 0xRRGGBB forced, -1 none */
#define g_FogAbsDist (GVar_R_FogAbsDist.value.i)
#define g_FogDensity (GVar_R_FogDensity.value.f)	/* forced, 0 none */
static cGVar GVar_R_FogDensity;
static cGVar GVar_R_FogColor;
static cGVar GVar_R_FogAbsDist;
#define D3DRS_FOGCOLOR 34
#define D3DRS_FOGTABLEMODE 35
#define D3DRS_FOGSTART 36
#define D3DRS_FOGEND 37
#define D3DRS_FOGDENSITY 38
#define D3DRS_FOGVERTEXMODE 140
#define D3DFOG_LINEAR 3
#define D3DPRASTERCAPS_FOGVERTEX 0x80
#define D3DPRASTERCAPS_FOGTABLE 0x100
#define D3DPRASTERCAPS_WFOG 0x100000

CFog *CFog_ctor(CFog *self) {
	self->active = 0;
	self->start = 0.5f;
	self->end = 1.0f;
	self->density = 0.5f;
	self->font = FOG_COLOR_SCENE;
	self->table = 0;
	self->capsW = 0;
	self->capsTable = 0;
	self->capsVertex = 1;
	self->mode = D3DFOG_LINEAR;
	return self;
}

void CFog_dtor(CFog *self) {
	(void)self;
}

void CFog_InitD3D(CFog *self) {
	CHECK(g_Engine->device);
	uint32_t caps = g_Engine->caps.RasterCaps;
	self->capsTable = (caps & D3DPRASTERCAPS_FOGTABLE) != 0;
	self->capsW = (caps & D3DPRASTERCAPS_WFOG) != 0;
	self->capsVertex = (caps & D3DPRASTERCAPS_FOGVERTEX) != 0;
	E3D_LogDebug2((&g_FileLog), "Fog init: w = %d  tab = %d", self->capsW, self->capsTable);
}

static void Fog_State(void *dev, uint32_t state, uint32_t value) {
	Rd_DevSetRenderState(dev, state, value);
}

static uint32_t Float_Bits(float f) {
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	return u;
}

int g_FogActive;

void CFog_Apply(CFog *self) {
	void *dev = g_Engine->device;
	CHECK(dev);
	if (!g_FogEnable) {
		g_FogActive = 0;
		return;
	}
	if (g_FogColor != -1) {
		uint32_t c = (uint32_t)g_FogColor;
		self->color.rgba = (c >> 16 & 0xff) | (c >> 8 & 0xff) << 8 | (c & 0xff) << 16;
		self->color.intens = 1.0f;
		self->font = FOG_COLOR_OWN;
	}
	if (!self->active) {
		g_FogActive = 0;
		return;
	}
	byte table = self->table && self->capsTable;
	/* an unknown source passes what is on the stack in the original, 0 here */
	uint32_t color = 0;
	if (self->font == FOG_COLOR_SCENE) {
		color = Color_ToD3D(World_Get()->GroundScale ? &World_Get()->GroundScale->fog : &self->color);
	} else if (self->font == FOG_COLOR_OWN) {
		color = Color_ToD3D(&self->color);
	} else {
		CHECK(0);
	}
	Fog_State(dev, D3DRS_FOGCOLOR, color);
	float start, end;
	if (!self->capsW && table) {
		start = self->start;
		end = self->end;
	} else {
		const CCamera *cam = World_Get()->cam;
		float d = cam->zFar - cam->zNear;
		start = d * self->start + cam->zNear;
		end = d * self->end + cam->zNear;
	}
	if (g_FogAbsDist) {
		if (!self->capsW && table) {
			g_FogActive = 0;
			return;
		}
		start = self->start;
		end = self->end;
	}
	Fog_State(dev, D3DRS_FOGSTART, Float_Bits(start));
	Fog_State(dev, D3DRS_FOGEND, Float_Bits(end));
	float density = table && self->mode != D3DFOG_LINEAR ? self->density : 1.0f;
	if (g_FogDensity != 0.0f) {
		density = g_FogDensity;
	}
	Fog_State(dev, D3DRS_FOGDENSITY, Float_Bits(density));
	if (table) {
		Fog_State(dev, D3DRS_FOGVERTEXMODE, 0);
		Fog_State(dev, D3DRS_FOGTABLEMODE, (uint32_t)self->mode);
	} else {
		Fog_State(dev, D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
		Fog_State(dev, D3DRS_FOGTABLEMODE, 0);
	}
	g_FogActive = 1;
}

void CFog_SetColor(CFog *self, uint32_t rgba, float intens) {
	self->color.rgba = rgba;
	self->color.intens = intens;
	self->font = FOG_COLOR_OWN;
}

void CFog_SetRange(CFog *self, float start, float end) {
	self->start = start;
	self->end = end;
}

static cGVar GVar_R_FogDensity;
cGVar GVar_R_FogEnable;
static cGVar GVar_R_FogAbsDist;
static cGVar GVar_R_FogColor;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_FogEnable, &GVar_R_FogEnable, "R_FogEnable", 2, "Allow/disable fog (def. 1)", 1)
INIT_GVAR_INT(Init_GVar_R_FogColor, &GVar_R_FogColor, "R_FogColor", 0, "Force fog color (def. -1)", -1)
INIT_GVAR_FLOAT(Init_GVar_R_FogDensity, &GVar_R_FogDensity, "R_FogDensity", 0, "Force fog density (def. 0)", 0.0f)
INIT_GVAR_INT(Init_GVar_R_FogAbsDist, &GVar_R_FogAbsDist, "R_FogAbsDist", 0, "Fog range is in world coords (0/1)", 0)
