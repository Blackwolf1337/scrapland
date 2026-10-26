/*
 * Engine/niebla.cpp of the engine3d: CNiebla, see include/scrap/niebla.h.
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/mundo.h"
#include "scrap/varios.h"

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

CNiebla *__fastcall CNiebla_ctor(CNiebla *self) {
	self->activa = 0;
	self->inicio = 0.5f;
	self->fin = 1.0f;
	self->densidad = 0.5f;
	self->fuente = NIEBLA_COLOR_ESCENA;
	self->tabla = 0;
	self->capsW = 0;
	self->capsTabla = 0;
	self->capsVertex = 1;
	self->modo = D3DFOG_LINEAR;
	return self;
}

void __fastcall CNiebla_dtor(CNiebla *self) {
	(void)self;
}

void __fastcall CNiebla_InitD3D(CNiebla *self) {
	CHECK(g_MotorDev);
	uint32_t caps = MOTOR_RASTERCAPS(g_Motor);
	self->capsTabla = (caps & D3DPRASTERCAPS_FOGTABLE) != 0;
	self->capsW = (caps & D3DPRASTERCAPS_WFOG) != 0;
	self->capsVertex = (caps & D3DPRASTERCAPS_FOGVERTEX) != 0;
	E3D_LogDebug2(g_FileLog, "Fog init: w = %d  tab = %d", self->capsW, self->capsTabla);
}

static void Niebla_State(void *dev, uint32_t state, uint32_t value) {
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, state, value);
}

static uint32_t Float_Bits(float f) {
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	return u;
}

void __fastcall CNiebla_Apply(CNiebla *self) {
	void *dev = g_MotorDev;
	CHECK(dev);
	if (!g_FogEnable) {
		g_FogActive = 0;
		return;
	}
	if (g_FogColor != -1) {
		uint32_t c = (uint32_t)g_FogColor;
		self->color.rgba = (c >> 16 & 0xff) | (c >> 8 & 0xff) << 8 | (c & 0xff) << 16;
		self->color.intens = 1.0f;
		self->fuente = NIEBLA_COLOR_PROPIO;
	}
	if (!self->activa) {
		g_FogActive = 0;
		return;
	}
	byte tabla = self->tabla && self->capsTabla;
	/* an unknown source passes what is on the stack in the original, 0 here */
	uint32_t color = 0;
	if (self->fuente == NIEBLA_COLOR_ESCENA) {
		color = Color_ToD3D(Mundo_Get()->EscSuelo ? &Mundo_Get()->EscSuelo->niebla : &self->color);
	} else if (self->fuente == NIEBLA_COLOR_PROPIO) {
		color = Color_ToD3D(&self->color);
	} else {
		CHECK(0);
	}
	Niebla_State(dev, D3DRS_FOGCOLOR, color);
	float inicio, fin;
	if (!self->capsW && tabla) {
		inicio = self->inicio;
		fin = self->fin;
	} else {
		const CCamara *cam = Mundo_Get()->cam;
		float d = cam->zFar - cam->zNear;
		inicio = d * self->inicio + cam->zNear;
		fin = d * self->fin + cam->zNear;
	}
	if (g_FogAbsDist) {
		if (!self->capsW && tabla) {
			g_FogActive = 0;
			return;
		}
		inicio = self->inicio;
		fin = self->fin;
	}
	Niebla_State(dev, D3DRS_FOGSTART, Float_Bits(inicio));
	Niebla_State(dev, D3DRS_FOGEND, Float_Bits(fin));
	float densidad = tabla && self->modo != D3DFOG_LINEAR ? self->densidad : 1.0f;
	if (g_FogDensity != 0.0f) {
		densidad = g_FogDensity;
	}
	Niebla_State(dev, D3DRS_FOGDENSITY, Float_Bits(densidad));
	if (tabla) {
		Niebla_State(dev, D3DRS_FOGVERTEXMODE, 0);
		Niebla_State(dev, D3DRS_FOGTABLEMODE, (uint32_t)self->modo);
	} else {
		Niebla_State(dev, D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
		Niebla_State(dev, D3DRS_FOGTABLEMODE, 0);
	}
	g_FogActive = 1;
}

void __thiscall CNiebla_SetColor(CNiebla *self, uint32_t rgba, float intens) {
	self->color.rgba = rgba;
	self->color.intens = intens;
	self->fuente = NIEBLA_COLOR_PROPIO;
}

void __thiscall CNiebla_SetRango(CNiebla *self, float inicio, float fin) {
	self->inicio = inicio;
	self->fin = fin;
}
