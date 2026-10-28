/*
 * Lights of the engine3d, original source Engine/luz.cpp (include/scrap/luz.h).
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fuentes.h"
#include "scrap/luz.h"
#include "scrap/material.h"
#include "scrap/mundo.h"
#include "scrap/prof.h"
#include "scrap/render.h"
#include "scrap/sueloportal.h"
#include "scrap/utild3d.h"

#define CLUZ_VFTABLE GAME_VFTABLE(CLuz_vftable, 0x3d79fc)
#define IOBJETO3D_VFTABLE GAME_VFTABLE(IObjeto3D_vftable, 0x3d79dc)
#define LUZ_INV255 (1.0f / 255.0f)
/* "no limit" for the range of the device light and the start of the nearest search, a float constant of the exe */
#define LUZ_RANGO_INF GAME_GLOBAL(const float, LUZ_RANGO_INF, 0x3d7be0)
#define PROF_LUCES 0x1d		/* the profiler section of Luz_Select */

#define g_Luces (GAME_ARRAY(CLuz *, g_Luces, 0x4ac438))	/* MAX_LUCES, the active lights */
#define g_nLuces GAME_GLOBAL(int, g_nLuces, 0x4ac434)
#define g_LucesD3D GAME_GLOBAL(int, g_LucesD3D, 0x4ac430)	/* with act_D3D */
#define g_LucesTotal GAME_GLOBAL(int, g_LucesTotal, 0x4ac42c)
#define g_LuzIndD3D GAME_GLOBAL(uint32_t, g_LuzIndD3D, 0x4ac428)	/* the next light gets index 4 + this */

/* the engine variables of the lights (Gvars) */
#define g_LightEnaD3D GAME_GLOBAL_IN(int, GVar_R_LightEnaD3D, 16, 0x4ac228)	/* R_LightEnaD3D: 0 always, 1 on changes, from 2 never */
#define g_LightAmbient GAME_GLOBAL_IN(float, GVar_R_LightAmbient, 16, 0x4ac240)
#define g_LightMult GAME_GLOBAL_IN(float, GVar_R_LightMult, 16, 0x4ac258)
#define g_SpotlightClamp GAME_GLOBAL_IN(float, GVar_R_SpotlightClamp, 16, 0x4ac270)
#define g_LightDesat GAME_GLOBAL_IN(float, GVar_R_LightDesat, 16, 0x4ac2a0)
#define g_MonitorAmbient GAME_GLOBAL_IN(uint32_t, GVar_MonitorAmbient, 16, 0x4ac2b8)
#define g_LightTrace GAME_GLOBAL_IN(int, GVar_R_LightTrace, 16, 0x4ac2d0)
#define g_SpotlightAdjust GAME_GLOBAL_IN(int, GVar_R_SpotlightAdjust, 16, 0x4ac2e8)
#define g_LightAtten GAME_GLOBAL_IN(float, GVar_R_LightAtten, 16, 0x4ac300)
#define g_LightGamma GAME_GLOBAL_IN(float, GVar_R_LightGamma, 16, 0x4ac318)
#define g_SpotlightMult GAME_GLOBAL_IN(float, GVar_R_SpotlightMult, 16, 0x4ac330)
#define g_LightSortI GAME_GLOBAL_IN(float, GVar_R_LightSortI, 16, 0x4ac348)
#define g_LightAtten2 GAME_GLOBAL_IN(int, GVar_R_LightAtten2, 16, 0x4ac360)
#define g_SpotlightAng GAME_GLOBAL_IN(int, GVar_R_SpotlightAng, 16, 0x4ac378)
#define g_SpotlightAtten GAME_GLOBAL_IN(float, GVar_R_SpotlightAtten, 16, 0x4ac390)
#define g_LightMult2 GAME_GLOBAL_IN(float, GVar_R_LightMult2, 16, 0x4ac3a8)
#define g_LightTarget GAME_GLOBAL_IN(int, GVar_R_LightTarget, 16, 0x4ac3c0)
#define g_LightDefer GAME_GLOBAL_IN(int, GVar_R_LightDefer, 16, 0x4ac3d8)
#define g_LightSector GAME_GLOBAL_IN(int, GVar_R_LightSector, 16, 0x4ac3f0)
#define g_LightSort GAME_GLOBAL_IN(int, GVar_R_LightSort, 16, 0x4ac408)
#define g_LightMultName GAME_GLOBAL_IN(const char *, GVar_R_LightMultName, 16, 0x4ac420)

typedef int(__stdcall *d3dSetLight_t)(void *dev, uint32_t idx, const D3DLight *l);
typedef int(__stdcall *d3dLightEnable_t)(void *dev, uint32_t idx, int enable);

static void D3D_SetLight(uint32_t idx, const D3DLight *l) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetLight_t, D3DDEV_SETLIGHT)(dev, idx, l);
}

static void D3D_LightEnable(uint32_t idx, int enable) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dLightEnable_t, D3DDEV_LIGHTENABLE)(dev, idx, enable);
}

static void D3D_SetRenderState(uint32_t state, uint32_t value) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, state, value);
}

typedef void(__thiscall *nodoDelete_t)(CNodo3D *self, int flags);
typedef void(__thiscall *luzUpdate_t)(CLuz *self, int arg);

static void Luz_Release(CNodo3D **slot) {
	if (*slot) {
		CNodo3D *old = *slot;
		old->Refs--;
		if (!old->Refs && old) {
			((nodoDelete_t)old->vftable[0])(old, 1);
		}
		*slot = NULL;
	}
}

static void Luz_Link(CNodo3D **slot, CNodo3D *n) {
	if (n) {
		n->Refs++;
	}
	Luz_Release(slot);
	*slot = n;
}

/* the enable of the device light through the cache (inlined in several places of the original) */
static void Luz_EnableD3D(CLuz *l, byte enable) {
	if (g_LightEnaD3D <= 1 && (enable != l->ena_D3D || g_LightEnaD3D == 0)) {
		CHECK(l->act_D3D || !enable);
		l->ena_D3D = enable;
		D3D_LightEnable(l->indD3D, enable);
	}
}

static void Luz_Add(CLuz *self) {
	self->activa = 1;
	self->ind_lista = g_nLuces;
	CHECK(g_nLuces < MAX_LUCES);
	int n = g_nLuces;
	g_Luces[n] = self;
	g_nLuces = n + 1;
	CLuz_UpdateD3D(self, 0);
}

static void Luz_Remove(CLuz *self) {
	CHECK(self->ind_lista < MAX_LUCES);
	int i = self->ind_lista;
	CLuz *last = g_Luces[g_nLuces - 1];
	g_nLuces--;
	g_Luces[i] = last;
	last->ind_lista = i;
	self->activa = 0;
	self->ind_lista = -1;
	CLuz_UpdateD3D(self, 0);
}

/* the color bytes times the intensity */
static color4_t Luz_Color(const CLuz *self) {
	float k = self->intensidad * LUZ_INV255;
	color4_t c = { (float)self->color[0] * k, (float)self->color[1] * k, (float)self->color[2] * k,
		(float)self->color[3] * k };
	return c;
}

static float Luz_Dist2(const CLuz *l, const vec3_t *p) {
	float dx = l->d3d.Position.x - p->x;
	float dy = l->d3d.Position.y - p->y;
	float dz = l->d3d.Position.z - p->z;
	return dx * dx + dy * dy + dz * dz;
}

static float Luz_DegToRad(float deg) {
	return (float)((double)deg * M_PI_D / DEG_PER_PI);
}

CLuz *__fastcall CLuz_ctor(CLuz *self) {
	static const byte white[4] = { 0xff, 0xff, 0xff, 0 };

	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->dir = (vec3_t){ 0.0f, -1.0f, 0.0f };
	memcpy(self->color, white, sizeof(white));
	self->intensidad = 1.0f;
	self->indD3D = g_LuzIndD3D + 4;
	g_LuzIndD3D++;
	self->vftable = CLUZ_VFTABLE;
	self->clase = CLASE_OBJ3D_LUZ;
	self->tipo = LUZ_DIREC;
	self->mostrar = 0;
	self->sombra = 0;
	self->ena_D3D = 0;
	self->activa = 0;
	self->f30 = 0.0f;
	self->rango = 0.0f;
	self->theta = 43.0f;
	self->phi = 45.0f;
	self->mult = 1.0f;
	self->f44 = 1.0f;
	self->peso = 0.0f;
	self->nodo = NULL;
	self->nodoObjetivo = NULL;
	self->act_D3D = 0;
	self->sector = -1;
	CLuz_FillD3D(self, &self->d3d);
	if (!self->activa) {
		Luz_Add(self);
	}
	g_LucesTotal++;
	return self;
}

void __fastcall CLuz_dtor(CLuz *self) {
	self->vftable = CLUZ_VFTABLE;
	Luz_Release(&self->nodo);
	self->nodo = NULL;
	Luz_Release(&self->nodoObjetivo);
	self->nodoObjetivo = NULL;
	if (self->activa) {
		Luz_Remove(self);
	}
	CHECK(g_LucesTotal > 0);
	if (--g_LucesTotal == 0) {
		g_LuzIndD3D = 0;
		E3D_LogDebug(g_FileLog, "Global light index reset");
	}
	self->vftable = IOBJETO3D_VFTABLE;
}

CLuz *__thiscall CLuz_Delete(CLuz *self, byte flags) {
	CLuz_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __fastcall IObjeto3D_dtor(IObjeto3D *self) {
	self->vftable = IOBJETO3D_VFTABLE;
}

IObjeto3D *__thiscall IObjeto3D_Delete(IObjeto3D *self, byte flags) {
	self->vftable = IOBJETO3D_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte __thiscall IObjeto3D_Read(IObjeto3D *self, CArchIO *f, void *ctx) {
	(void)self;
	(void)f;
	(void)ctx;
	return 0;
}

byte __thiscall IObjeto3D_Write(IObjeto3D *self, CArchIO *f, void *ctx) {
	(void)self;
	(void)f;
	(void)ctx;
	return 0;
}

int Luz_Select(const vec3_t *pos, int sector, const vec3_t *center, float radius, CLuz **out, int maxOn) {
	CHECK(g_pd3dDevice);
	Prof_Begin(PROF_LUCES);
	CHECK(radius > 0.0f);
	int n = 0;
	CLuz *dirSombra = NULL, *sombra = NULL;
	float best = LUZ_RANGO_INF;
	for (int i = 0; i < g_nLuces; i++) {
		CLuz *l = g_Luces[i];
		if (!l->act_D3D) {
			continue;
		}
		CHECK(l->activa);
		CHECK(l->ind_lista == i);
		if (l->sector == sector || l->sector < 0 || sector < 0 || !g_LightSector) {
			byte ok = 1;
			if (l->tipo != LUZ_DIREC && l->rango != 0.0f) {
				ok = l->rango * l->rango > Luz_Dist2(l, pos);
			}
			if (ok && l->tipo == LUZ_FOCO && g_SpotlightAng) {
				vec3_t p = l->d3d.Position, d = l->d3d.Direction;
				ok = Sph_InCone(center, radius, &p, &d, Luz_DegToRad(l->phi));
			}
			if (ok && n < g_LightSort) {
				if (g_LightTrace && l->tipo != LUZ_DIREC) {
					vec3_t p = l->d3d.Position;
					CHECK(g_Mundo && g_Mundo->ObjSuelo);
					ok = !CSuelo_Trace(g_Mundo->ObjSuelo, pos, &p, NULL, 0.0f, NULL);
				}
				if (ok) {
					CHECK(out && n < MAX_LUCES_OBJ);
					out[n++] = l;
					l->peso = 0.0f;
					if (l->tipo != LUZ_DIREC) {
						if (0.0f > g_LightSortI) {
							l->peso = -(l->intensidad / (l->d3d.Attenuation2 * Luz_Dist2(l, pos) + 1.0f));
						} else {
							float d = (float)__builtin_sqrt((double)Luz_Dist2(l, pos));
							l->peso = d;
							l->peso = (1.0f / (l->intensidad * g_LightSortI + 1.0f)) * d;
						}
					}
					if (l->sombra && (l->nodo->flags & NODO_SOMBRA)) {
						if (l->tipo == LUZ_DIREC) {
							dirSombra = l;
						} else {
							float d2 = Luz_Dist2(l, pos);
							if (best >= d2) {
								sombra = l;
								best = d2;
							}
						}
					}
					continue;
				}
			}
		}
		Luz_EnableD3D(l, 0);
	}
	E3D_SetShadowLight(sombra ? sombra : dirSombra, 0);
	crt_qsort(out, n, sizeof(*out), Luz_Compare);
	for (int i = 0; i < n; i++) {
		CLuz *l = out[i];
		CHECK(l->act_D3D);
		CHECK(l->activa);
		Luz_EnableD3D(l, i < maxOn);
	}
	Prof_End(PROF_LUCES);
	return n;
}

int Luz_SetDefault(byte on) {
	/* the directions of the three lights of EnableMonitorLights */
	static const uint32_t dirBits[3][3] = {
		{ 0xbf15e68a, 0xbe42517e, 0x3f49c1c6 },
		{ 0x3f7ffea0, 0xbbcd737b, 0x3aad5005 },
		{ 0xb99a05ae, 0xbe807faa, 0xbf77ce74 },
	};
	/* their colors, constants of the exe */
	static const uint32_t colorRva[3] = { 0x3d7c10, 0x3d7c00, 0x3d7bf0 };

	E3D_FlushBatch();
	E3D_SetShadowLight(NULL, 0);
	if (on) {
		D3D_SetRenderState(D3DRS_AMBIENT, g_MonitorAmbient);
		Luz_EnableAll(0);
		for (uint32_t i = 0; i < 3; i++) {
			color4_t c;
			vec3_t d;
			memcpy(&c, Game_Ptr(colorRva[i]), sizeof(c));
			memcpy(&d, dirBits[i], sizeof(d));
			Luz_SetDirLight(i, &c, &d);
		}
		for (uint32_t i = 0; i < 3; i++) {
			D3D_LightEnable(i, 1);
		}
		return 0;
	}
	D3D_SetRenderState(D3DRS_AMBIENT, g_AmbientD3D);
	for (uint32_t i = 0; i < 3; i++) {
		D3D_LightEnable(i, 0);
	}
	Luz_EnableAll(1);
	return 0;
}

void __thiscall CLuz_Hide(CLuz *self, int arg) {
	(void)arg;
	CLuz_UpdateD3D(self, 1);
}

void __thiscall CLuz_Reset(CLuz *self, int arg) {
	(void)arg;
	CLuz_FillD3D(self, &self->d3d);
	CLuz_UpdateD3D(self, 0);
	((luzUpdate_t)((void **)self->vftable)[1])(self, 0);
}

/* R_ShowLights and the material of the debug drawing, a static of CLuz_Render */
#define g_ShowLights GAME_GLOBAL_IN(int, GVar_R_ShowLights, 16, 0x4ac288)
#define g_LuzMat (GAME_GLOBAL(CMaterial, g_LuzMat, 0x4ad438))
#define g_LuzMatGuard (GAME_ARRAY(int, g_LuzMatGuard, 0x4ad4e8))

/* the atexit destructor of it (rva 0x38ed20) */
void Luz_FiniMat(void) {
	CMaterial_dtor(&g_LuzMat);
}

#define D3DFVF_XYZ_DIFFUSE 0x42

typedef struct LuzVert {
	vec3_t pos;
	uint32_t color;
} LuzVert;

/* the virtual Render (dev, r not used): a light drawn only with mostrar or R_ShowLights: a corner of the diffuse
 * color sized by the distance to the eye; from 2 the texts, from 3 the range sphere, the axes and the cone of a
 * spot light */
void __thiscall CLuz_Render(CLuz *self, void *dev, CNodoR *r) {
	LuzVert v[5];
	matrix_t m;
	vec3_t p, a, b;
	const vec3_t *eye;
	const char *tipo;
	uint32_t color;
	float s, len, k, rad;

	(void)r;
	if (!self->mostrar && !g_ShowLights) {
		return;
	}
	if (*g_LuzMatGuard > crt_InitEpoch()) {
		crt_Init_thread_header(g_LuzMatGuard);
		if (*g_LuzMatGuard == -1) {
			CMaterial_ctor(&g_LuzMat);
			crt_atexit(Luz_FiniMat);
			crt_Init_thread_footer(g_LuzMatGuard);
		}
	}
	Render_SetWorld();
	g_LuzMat.params.b39 = 0;
	Render_SetMaterial(&g_LuzMat, 0);
	color = Color_FromFloat(&self->d3d.Diffuse.r);
	Render_GetWorld(&m);
	p.x = m.m[12];
	p.y = m.m[13];
	p.z = m.m[14];
	eye = Render_GetEyePos();
	s = (float)__builtin_sqrt((double)((eye->y - p.y) * (eye->y - p.y) + (eye->x - p.x) * (eye->x - p.x) +
		(eye->z - p.z) * (eye->z - p.z))) * 0.01f;
	memset(v, 0, sizeof(v));
	v[1].pos.x = s;
	v[2].pos.y = s;
	v[3].pos.z = s;
	v[4].pos.x = s;
	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;
	v[4].color = color;
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, D3DFVF_XYZ_DIFFUSE);
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 3, v, sizeof(v[0]));
	g_RenderTris += 3;
	g_RenderVerts += 5;
	if (g_ShowLights > 1) {
		tipo = Luz_TipoStr(self->tipo);
		a = self->d3d.Position;
		Debug_Text3D(g_FntTerm8, 0.0f, 0.0f, &a, 0xffffff, "%s [%s,%d] [%f]", self->nodo->name, tipo, self->sector,
			(double)self->rango);
		a = self->d3d.Position;
		Debug_Text3D(g_FntTerm8, 0.0f, 12.0f, &a, color, "%.1f %.1f %.1f", (double)self->d3d.Diffuse.r,
			(double)self->d3d.Diffuse.g, (double)self->d3d.Diffuse.b);
	}
	if (g_ShowLights > 2) {
		a.x = 0.0f;
		a.y = 0.0f;
		a.z = 0.0f;
		Debug_DrawSphereM(&m, &a, self->rango, g_DebugColors[self->ind_lista & 0xf], 10, 10);
	}
	if (self->rango != 0.0f) {
		rad = self->rango;
		if (self->tipo == LUZ_FOCO) {
			rad = rad * 0.1f;
		}
		a.x = 0.0f;
		a.y = 0.0f;
		a.z = 0.0f;
		Debug_DrawAxes(&m, &a, rad);
	}
	if (self->tipo == LUZ_FOCO) {
		b = self->d3d.Direction;
		len = (float)__builtin_sqrt((double)(b.y * b.y + b.x * b.x + b.z * b.z));
		if ((double)len != 0.0) {
			k = (float)(1.0 / (double)len);
			b.x = self->d3d.Direction.x * k;
			b.y = self->d3d.Direction.y * k;
			b.z = self->d3d.Direction.z * k;
		}
		b.x = self->d3d.Position.x + b.x * self->rango;
		b.y = self->d3d.Position.y + b.y * self->rango;
		b.z = self->d3d.Position.z + b.z * self->rango;
		a = self->d3d.Position;
		Debug_DrawLine(&a, &b, 0xffff00);
		Debug_DrawConeM(&m, self->rango, Luz_DegToRad(self->phi));
	}
}

void Luz_SetDirLight(uint32_t idx, const color4_t *color, const vec3_t *dir) {
	D3DLight l;

	memset(&l, 0, sizeof(l));
	l.Diffuse = *color;
	l.Specular = *color;
	l.Ambient = (color4_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	l.Type = D3DLIGHT_DIRECTIONAL;
	l.Direction = *dir;
	D3D_SetLight(idx, &l);
}

void __thiscall CLuz_EnableD3D(CLuz *self, byte enable) {
	Luz_EnableD3D(self, enable);
}

/* the members after the version as the file has them */
static const struct {
	uint8_t off, size;
} luzFields[] = {
	{ offsetof(CLuz, sector), 4 }, { offsetof(CLuz, tipo), 4 }, { offsetof(CLuz, sombra), 1 },
	{ offsetof(CLuz, pos), 12 }, { offsetof(CLuz, dir), 12 }, { offsetof(CLuz, color), 8 },
	{ offsetof(CLuz, f30), 4 }, { offsetof(CLuz, rango), 4 }, { offsetof(CLuz, theta), 4 },
	{ offsetof(CLuz, phi), 4 }, { offsetof(CLuz, mult), 4 }, { offsetof(CLuz, f44), 4 },
};

byte __thiscall CLuz_Read(CLuz *self, CArchIO *f, void *ctx) {
	Luz_Link(&self->nodo, *(CNodo3D **)((byte *)ctx + 4));
	CArchIO_BeginBlock(f, "LUZ");
	CHECK(CArchIO_ReadInt(f) == 1);
	CHECK(self->sector == -1);
	CHECK(!self->ena_D3D);
	for (size_t i = 0; i < sizeof(luzFields) / sizeof(luzFields[0]); i++) {
		CArchIO_Read(f, (byte *)self + luzFields[i].off, luzFields[i].size);
	}
	CLuz_SetActiva(self, CArchIO_ReadBool(f));
	CArchIO_Error(f, "LUZ");
	return 1;
}

void Luz_List(void) {
	E3D_DebugPrintf("\nLuces:");
	for (int i = 0; i < g_nLuces; i++) {
		CLuz *l = g_Luces[i];
		const char *name = l->nodo ? l->nodo->name : "(null)";
		E3D_DebugPrintf("\n  '%s'  tipo: %s  activa: %s  act_d3d: %s", name, Luz_TipoStr(l->tipo),
			l->activa ? "SI" : "NO", l->act_D3D ? "SI" : "NO");
		CHECK(!l->activa || l->ind_lista == i);
	}
	E3D_DebugPrintf("\n  %d luces (%d activas, %d act_d3d)", g_LucesTotal, g_nLuces, g_LucesD3D);
}

byte __thiscall CLuz_Write(CLuz *self, CArchIO *f, void *ctx) {
	CHECK(self->nodo == *(CNodo3D **)((byte *)ctx + 4));
	CArchIO_BeginBlockWrite(f, "LUZ");
	CArchIO_WriteInt(f, 1);
	CHECK(!self->ena_D3D);
	Luz_Release(&self->nodoObjetivo);
	self->nodoObjetivo = NULL;
	for (size_t i = 0; i < sizeof(luzFields) / sizeof(luzFields[0]); i++) {
		CArchIO_Write(f, (byte *)self + luzFields[i].off, luzFields[i].size);
	}
	CArchIO_WriteInt(f, self->activa);
	CArchIO_EndBlockWrite(f, "LUZ");
	return 1;
}

void __thiscall CLuz_Update(CLuz *self, int arg) {
	(void)arg;
	if (!self->activa) {
		return;
	}
	D3DLight *l = &self->d3d;
	CLuz_FillD3D(self, l);
	CNodo3D *n = self->nodo;
	if (n) {
		float x = n->m.m[12], y = n->m.m[13], z = n->m.m[14];
		l->Position = (vec3_t){ x, y, z };
		if (l->Type != D3DLIGHT_POINT) {
			CNodo3D *t = self->nodoObjetivo;
			if (t && g_LightTarget) {
				l->Direction.x = t->m.m[12] - x;
				l->Direction.y = t->m.m[13] - y;
				l->Direction.z = t->m.m[14] - z;
			} else {
				vec3_t d;
				Vec3_TransformDir0(&d, &self->dir, &n->m);
				l->Direction = d;
			}
		}
		vec3_t c;
		float k;
		color4_t col;
		if (CNodo_LightColor(self->nodo, &c, &k)) {
			col = (color4_t){ c.x * k, c.y * k, c.z * k, 1.0f };
		} else {
			col = Luz_Color(self);
		}
		l->Specular = col;
		l->Diffuse = col;
	}
	CLuz_AdjustD3D(self, l);
	if (!self->activa) {
		return;
	}
	byte vis = 1;
	if (self->sector >= 0 && g_LightSector) {
		vis = Sector_IsVisible(self->sector);
	}
	if (vis && g_pd3dDevice && l->Type != D3DLIGHT_DIRECTIONAL && LUZ_RANGO_INF > l->Range) {
		vec3_t p = l->Position;
		vis = Sph_IsVisible(&p, l->Range, 0);
	}
	CLuz_UpdateD3D(self, !vis);
}

void Luz_EnableAll(byte enable) {
	CHECK(g_pd3dDevice);
	if (g_LightDefer) {
		enable = 0;
	}
	for (int i = 0; i < g_nLuces; i++) {
		CLuz *l = g_Luces[i];
		CHECK(l->activa);
		CHECK(l->ind_lista == i);
		Luz_EnableD3D(l, enable ? l->act_D3D : 0);
	}
}

void __thiscall CLuz_AdjustD3D(CLuz *self, D3DLight *l) {
	byte spot = l->Type == D3DLIGHT_SPOT && g_SpotlightAdjust;

	if (g_LightAtten2 && self->rango != 0.0f) {
		float k = 1.0f / self->intensidad;
		l->Diffuse.r = l->Diffuse.r * k;
		l->Diffuse.g = l->Diffuse.g * k;
		l->Diffuse.b = l->Diffuse.b * k;
		l->Diffuse.a = l->Diffuse.a * k;
		l->Attenuation0 = l->Attenuation0 / (self->mult * g_LightMult2 * self->intensidad);
	} else {
		float m = spot ? g_SpotlightMult : g_LightMult;
		if (*g_LightMultName && !Wild_MatchLower(g_LightMultName, self->nodo->name)) {
			m = 1.0f;
		}
		l->Diffuse.r = l->Diffuse.r * m * self->mult;
		l->Diffuse.g = l->Diffuse.g * m * self->mult;
		l->Diffuse.b = l->Diffuse.b * m * self->mult;
		l->Diffuse.a = l->Diffuse.a * m * self->mult;
		float a = spot ? g_SpotlightAtten : g_LightAtten;
		l->Attenuation2 = self->rango != 0.0f ? a / (self->rango * self->rango) : 0.0f;
	}
	if (g_LightGamma != 1.0f) {
		double e = (double)(1.0f / g_LightGamma);
		l->Diffuse.r = (float)Game_Pow((double)l->Diffuse.r, e);
		l->Diffuse.g = (float)Game_Pow((double)l->Diffuse.g, e);
		l->Diffuse.b = (float)Game_Pow((double)l->Diffuse.b, e);
	}
	float desat = g_LightDesat;
	if (desat != 0.0f) {
		float s = 1.0f - desat;
		float r = l->Diffuse.r, g = l->Diffuse.g, b = l->Diffuse.b;
		float lum = (g * 0.59f + r * 0.3f + b * 0.11f) * desat;
		l->Diffuse.r = s * r + lum;
		l->Diffuse.b = s * b + lum;
		l->Diffuse.g = s * g + lum;
	}
	float clamp = g_SpotlightClamp;
	if (spot && clamp != 0.0f) {
		float r = l->Diffuse.r, g = l->Diffuse.g, b = l->Diffuse.b;
		float m = g > r ? g : r;
		m = b > m ? b : m;
		if (m > clamp) {
			float k = clamp / m;
			r = k * r;
			g = k * g;
			b = k * b;
		}
		l->Diffuse.r = r;
		l->Diffuse.g = g;
		l->Diffuse.b = b;
	}
	l->Specular = l->Diffuse;
	float amb = g_LightAmbient;
	l->Ambient = (color4_t){ amb, amb, amb, 1.0f };
}

void __thiscall CLuz_SetNodo(CLuz *self, CNodo3D *nodo) {
	Luz_Link(&self->nodo, nodo);
}

void __thiscall CLuz_SetNodoObjetivo(CLuz *self, CNodo3D *nodo) {
	Luz_Link(&self->nodoObjetivo, nodo);
}

void __thiscall CLuz_SetActiva(CLuz *self, byte activa) {
	if (activa) {
		if (!self->activa) {
			Luz_Add(self);
		}
	} else if (self->activa) {
		Luz_Remove(self);
	}
}

int Luz_Compare(const void *a, const void *b) {
	const CLuz *la = *(CLuz *const *)a, *lb = *(CLuz *const *)b;
	return lb->peso > la->peso ? -1 : 1;
}

void __thiscall CLuz_FillD3D(CLuz *self, D3DLight *l) {
	switch (self->tipo) {
	case LUZ_OMNI:
		l->Type = D3DLIGHT_POINT;
		break;
	case LUZ_FOCO:
		l->Type = D3DLIGHT_SPOT;
		break;
	case LUZ_DIREC:
		l->Type = D3DLIGHT_DIRECTIONAL;
		break;
	default:
		CHECK(0);
	}
	color4_t c = Luz_Color(self);
	l->Diffuse = c;
	l->Specular = c;
	l->Ambient = (color4_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	l->Position = self->pos;
	l->Direction = self->dir;
	l->Range = self->rango == 0.0f ? LUZ_RANGO_INF : self->rango;
	l->Falloff = 1.0f;
	l->Attenuation0 = 1.0f;
	l->Attenuation1 = 0.0f;
	l->Attenuation2 = self->rango == 0.0f ? 0.0f : g_LightAtten / (self->rango * self->rango);
	l->Theta = Luz_DegToRad(self->theta);
	l->Phi = Luz_DegToRad(self->phi);
}

int Luz_NumD3D(void) {
	return g_LucesD3D;
}

int Luz_NumActivas(void) {
	return g_nLuces;
}

int Luz_NumTotal(void) {
	return g_LucesTotal;
}

void __thiscall CLuz_UpdateD3D(CLuz *self, byte hide) {
	if (!g_pd3dDevice) {
		return;
	}
	byte act = hide ? 0 : self->activa;
	if (act != self->act_D3D) {
		self->act_D3D = act;
		g_LucesD3D += act ? 1 : -1;
		if (!g_LightDefer) {
			CLuz_EnableD3D(self, act);
		} else {
			Luz_EnableD3D(self, 0);
		}
	}
	if (self->act_D3D) {
		D3D_SetLight(self->indD3D, &self->d3d);
	}
}

box_t *__fastcall Box_Zero(box_t *self) {
	self->min = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->max = (vec3_t){ 0.0f, 0.0f, 0.0f };
	return self;
}
