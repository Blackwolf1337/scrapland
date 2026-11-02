/*
 * Lights of the engine3d, original source Engine/luz.cpp (include/scrap/light.h).
 */
#include <string.h>

#include "scrap/meshd3d.h"
#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fonts.h"
#include "scrap/light.h"
#include "scrap/material.h"
#include "scrap/e3dworld.h"
#include "scrap/prof.h"
#include "scrap/render.h"
#include "scrap/groundportal.h"
#include "scrap/utild3d.h"

extern const IObject3DVtbl CLight_vftable;
#define CLUZ_VFTABLE (&CLight_vftable)
#define LIGHT_INV255 (1.0f / 255.0f)
/* "no limit" for the range of the device light and the start of the nearest search, a float constant of the exe */
extern const float LIGHT_RANGE_INF;

extern CLight * g_Lights[1024];	/* MAX_LIGHTS, the active lights */
extern int g_nLights;
extern int g_LightsD3D;	/* with act_D3D */
extern int g_LightsTotal;
extern uint32_t g_LightIndexD3D;	/* the next light gets index 4 + this */

/* the engine variables of the lights (Gvars) */
extern cGVar GVar_R_LightEnaD3D;
#define g_LightEnaD3D (GVar_R_LightEnaD3D.value.i)	/* R_LightEnaD3D: 0 always, 1 on changes, from 2 never */
extern cGVar GVar_R_LightAmbient;
#define g_LightAmbient (GVar_R_LightAmbient.value.f)
extern cGVar GVar_R_LightMult;
#define g_LightMult (GVar_R_LightMult.value.f)
extern cGVar GVar_R_SpotlightClamp;
#define g_SpotlightClamp (GVar_R_SpotlightClamp.value.f)
extern cGVar GVar_R_LightDesat;
#define g_LightDesat (GVar_R_LightDesat.value.f)
#define g_MonitorAmbient (*(uint32_t *)&GAME_GVAR(GVar_MonitorAmbient)->value)
extern cGVar GVar_R_LightTrace;
#define g_LightTrace (GVar_R_LightTrace.value.i)
extern cGVar GVar_R_SpotlightAdjust;
#define g_SpotlightAdjust (GVar_R_SpotlightAdjust.value.i)
extern cGVar GVar_R_LightAtten;
#define g_LightAtten (GVar_R_LightAtten.value.f)
extern cGVar GVar_R_LightGamma;
#define g_LightGamma (GVar_R_LightGamma.value.f)
extern cGVar GVar_R_SpotlightMult;
#define g_SpotlightMult (GVar_R_SpotlightMult.value.f)
extern cGVar GVar_R_LightSortI;
#define g_LightSortI (GVar_R_LightSortI.value.f)
extern cGVar GVar_R_LightAtten2;
#define g_LightAtten2 (GVar_R_LightAtten2.value.i)
extern cGVar GVar_R_SpotlightAng;
#define g_SpotlightAng (GVar_R_SpotlightAng.value.i)
extern cGVar GVar_R_SpotlightAtten;
#define g_SpotlightAtten (GVar_R_SpotlightAtten.value.f)
extern cGVar GVar_R_LightMult2;
#define g_LightMult2 (GVar_R_LightMult2.value.f)
extern cGVar GVar_R_LightTarget;
#define g_LightTarget (GVar_R_LightTarget.value.i)
extern cGVar GVar_R_LightDefer;
#define g_LightDefer (GVar_R_LightDefer.value.i)
extern cGVar GVar_R_LightSector;
#define g_LightSector (GVar_R_LightSector.value.i)
extern cGVar GVar_R_LightSort;
#define g_LightSort (GVar_R_LightSort.value.i)
extern cGVar GVar_R_LightMultName;
#define g_LightMultName (GVar_R_LightMultName.value.s)


static void D3D_SetLight(uint32_t idx, const D3DLight *l) {
	void *dev = g_pd3dDevice;
	Rd_DevSetLight(dev, idx, l);
}

static void D3D_LightEnable(uint32_t idx, int enable) {
	void *dev = g_pd3dDevice;
	Rd_DevLightEnable(dev, idx, enable);
}

static void D3D_SetRenderState(uint32_t state, uint32_t value) {
	void *dev = g_pd3dDevice;
	Rd_DevSetRenderState(dev, state, value);
}


static void Light_Release(CNode3D **slot) {
	if (*slot) {
		CNode3D *old = *slot;
		old->Refs--;
		if (!old->Refs && old) {
			old->vftable->Delete(&old->ref, 1);
		}
		*slot = NULL;
	}
}

static void Light_Link(CNode3D **slot, CNode3D *n) {
	if (n) {
		n->Refs++;
	}
	Light_Release(slot);
	*slot = n;
}

/* the enable of the device light through the cache (inlined in several places of the original) */
static void Light_EnableD3D(CLight *l, byte enable) {
	if (g_LightEnaD3D <= 1 && (enable != l->ena_D3D || g_LightEnaD3D == 0)) {
		CHECK(l->act_D3D || !enable);
		l->ena_D3D = enable;
		D3D_LightEnable(l->indexD3D, enable);
	}
}

static void Light_Add(CLight *self) {
	self->active = 1;
	self->index_list = g_nLights;
	CHECK(g_nLights < MAX_LIGHTS);
	int n = g_nLights;
	g_Lights[n] = self;
	g_nLights = n + 1;
	CLight_UpdateD3D(self, 0);
}

static void Light_Remove(CLight *self) {
	CHECK(self->index_list < MAX_LIGHTS);
	int i = self->index_list;
	CLight *last = g_Lights[g_nLights - 1];
	g_nLights--;
	g_Lights[i] = last;
	last->index_list = i;
	self->active = 0;
	self->index_list = -1;
	CLight_UpdateD3D(self, 0);
}

/* the color bytes times the intensity */
static color4_t Light_Color(const CLight *self) {
	float k = self->intensity * LIGHT_INV255;
	color4_t c = { (float)self->color[0] * k, (float)self->color[1] * k, (float)self->color[2] * k,
		(float)self->color[3] * k };
	return c;
}

static float Light_Dist2(const CLight *l, const vec3_t *p) {
	float dx = l->d3d.Position.x - p->x;
	float dy = l->d3d.Position.y - p->y;
	float dz = l->d3d.Position.z - p->z;
	return dx * dx + dy * dy + dz * dz;
}

static float Light_DegToRad(float deg) {
	return (float)((double)deg * M_PI_D / DEG_PER_PI);
}

CLight *CLight_ctor(CLight *self) {
	static const byte white[4] = { 0xff, 0xff, 0xff, 0 };

	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->dir = (vec3_t){ 0.0f, -1.0f, 0.0f };
	memcpy(self->color, white, sizeof(white));
	self->intensity = 1.0f;
	self->indexD3D = g_LightIndexD3D + 4;
	g_LightIndexD3D++;
	self->vftable = CLUZ_VFTABLE;
	self->class = CLASS_OBJ3D_LIGHT;
	self->type = LIGHT_DIR;
	self->show = 0;
	self->shadow = 0;
	self->ena_D3D = 0;
	self->active = 0;
	self->rangeStart = 0.0f;
	self->range = 0.0f;
	self->theta = 43.0f;
	self->phi = 45.0f;
	self->mult = 1.0f;
	self->radCoef = 1.0f;
	self->weight = 0.0f;
	self->node = NULL;
	self->nodeTarget = NULL;
	self->act_D3D = 0;
	self->sector = -1;
	CLight_FillD3D(self, &self->d3d);
	if (!self->active) {
		Light_Add(self);
	}
	g_LightsTotal++;
	return self;
}

void CLight_dtor(CLight *self) {
	self->vftable = CLUZ_VFTABLE;
	Light_Release(&self->node);
	self->node = NULL;
	Light_Release(&self->nodeTarget);
	self->nodeTarget = NULL;
	if (self->active) {
		Light_Remove(self);
	}
	CHECK(g_LightsTotal > 0);
	if (--g_LightsTotal == 0) {
		g_LightIndexD3D = 0;
		E3D_LogDebug(g_FileLog, "Global light index reset");
	}
	self->vftable = IOBJECT3D_VFTABLE;
}

IObject3D *CLight_Delete(IObject3D *base, byte flags) {
	CLight *self = (CLight *)base;
	CLight_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

void IObject3D_dtor(IObject3D *self) {
	self->vftable = IOBJECT3D_VFTABLE;
}

IObject3D *IObject3D_Delete(IObject3D *self, byte flags) {
	self->vftable = IOBJECT3D_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte IObject3D_Read(IObject3D *self, CFileIO *f, void *ctx) {
	(void)self;
	(void)f;
	(void)ctx;
	return 0;
}

byte IObject3D_Write(IObject3D *self, CFileIO *f, void *ctx) {
	(void)self;
	(void)f;
	(void)ctx;
	return 0;
}

int Light_Select(const vec3_t *pos, int sector, const vec3_t *center, float radius, CLight **out, int maxOn) {
	CHECK(g_pd3dDevice);
	CHECK(radius > 0.0f);
	int n = 0;
	CLight *dirShadow = NULL, *shadow = NULL;
	float best = LIGHT_RANGE_INF;
	for (int i = 0; i < g_nLights; i++) {
		CLight *l = g_Lights[i];
		if (!l->act_D3D) {
			continue;
		}
		CHECK(l->active);
		CHECK(l->index_list == i);
		if (l->sector == sector || l->sector < 0 || sector < 0 || !g_LightSector) {
			byte ok = 1;
			if (l->type != LIGHT_DIR && l->range != 0.0f) {
				ok = l->range * l->range > Light_Dist2(l, pos);
			}
			if (ok && l->type == LIGHT_SPOT && g_SpotlightAng) {
				vec3_t p = l->d3d.Position, d = l->d3d.Direction;
				ok = Sph_InCone(center, radius, &p, &d, Light_DegToRad(l->phi));
			}
			if (ok && n < g_LightSort) {
				if (g_LightTrace && l->type != LIGHT_DIR) {
					vec3_t p = l->d3d.Position;
					CHECK(g_World && g_World->ObjGround);
					ok = !CGround_Trace(g_World->ObjGround, pos, &p, NULL, 0.0f, NULL);
				}
				if (ok) {
					CHECK(out && n < MAX_LIGHTS_OBJ);
					out[n++] = l;
					l->weight = 0.0f;
					if (l->type != LIGHT_DIR) {
						if (0.0f > g_LightSortI) {
							l->weight = -(l->intensity / (l->d3d.Attenuation2 * Light_Dist2(l, pos) + 1.0f));
						} else {
							float d = (float)__builtin_sqrt((double)Light_Dist2(l, pos));
							l->weight = d;
							l->weight = (1.0f / (l->intensity * g_LightSortI + 1.0f)) * d;
						}
					}
					if (l->shadow && (l->node->flags & NODE_SHADOW)) {
						if (l->type == LIGHT_DIR) {
							dirShadow = l;
						} else {
							float d2 = Light_Dist2(l, pos);
							if (best >= d2) {
								shadow = l;
								best = d2;
							}
						}
					}
					continue;
				}
			}
		}
		Light_EnableD3D(l, 0);
	}
	E3D_SetShadowLight(shadow ? shadow : dirShadow, 0);
	crt_qsort(out, n, sizeof(*out), Light_Compare);
	for (int i = 0; i < n; i++) {
		CLight *l = out[i];
		CHECK(l->act_D3D);
		CHECK(l->active);
		Light_EnableD3D(l, i < maxOn);
	}
	return n;
}

int Light_SetDefault(byte on) {
	/* the directions of the three lights of EnableMonitorLights */
	static const uint32_t dirBits[3][3] = {
		{ 0xbf15e68a, 0xbe42517e, 0x3f49c1c6 },
		{ 0x3f7ffea0, 0xbbcd737b, 0x3aad5005 },
		{ 0xb99a05ae, 0xbe807faa, 0xbf77ce74 },
	};
	/* their colors, constants of the exe */
	const color4_t *colors[3] = { GAME_ARRAY(color4_t, colorRva_3d7c10), GAME_ARRAY(color4_t, colorRva_3d7c00),
		GAME_ARRAY(color4_t, colorRva_3d7bf0) };

	E3D_FlushBatch();
	E3D_SetShadowLight(NULL, 0);
	if (on) {
		D3D_SetRenderState(D3DRS_AMBIENT, g_MonitorAmbient);
		Light_EnableAll(0);
		for (uint32_t i = 0; i < 3; i++) {
			color4_t c;
			vec3_t d;
			memcpy(&c, colors[i], sizeof(c));
			memcpy(&d, dirBits[i], sizeof(d));
			Light_SetDirLight(i, &c, &d);
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
	Light_EnableAll(1);
	return 0;
}

void CLight_Hide(IObject3D *base, void *arg) {
	CLight *self = (CLight *)base;
	(void)arg;
	CLight_UpdateD3D(self, 1);
}

void CLight_Reset(IObject3D *elem, void *arg) {
	CLight *self = (CLight *)elem;
	(void)arg;
	CLight_FillD3D(self, &self->d3d);
	CLight_UpdateD3D(self, 0);
	self->vftable->Update(&self->base, NULL);
}

/* R_ShowLights and the material of the debug drawing, a static of CLight_Render */
extern cGVar GVar_R_ShowLights;
#define g_ShowLights (GVar_R_ShowLights.value.i)
extern CMaterial g_LightMat;
#define g_LightMatGuard (GAME_ARRAY(int, g_LightMatGuard))

/* the atexit destructor of it */
void Light_FiniMat(void) {
	CMaterial_dtor(&g_LightMat);
}

#define D3DFVF_XYZ_DIFFUSE 0x42

typedef struct LightVert {
	vec3_t pos;
	uint32_t color;
} LightVert;

/* the virtual Render (dev, r not used): a light drawn only with show or R_ShowLights: a corner of the diffuse
 * color sized by the distance to the eye; from 2 the texts, from 3 the range sphere, the axes and the cone of a
 * spot light */
void CLight_Render(IObject3D *base, void *dev, CRenderNode *r) {
	CLight *self = (CLight *)base;
	LightVert v[5];
	matrix_t m;
	vec3_t p, a, b;
	const vec3_t *eye;
	const char *type;
	uint32_t color;
	float s, len, k, rad;

	(void)r;
	if (!self->show && !g_ShowLights) {
		return;
	}
	if (*g_LightMatGuard > crt_InitEpoch()) {
		crt_Init_thread_header(g_LightMatGuard);
		if (*g_LightMatGuard == -1) {
			CMaterial_ctor(&g_LightMat);
			crt_atexit(Light_FiniMat);
			crt_Init_thread_footer(g_LightMatGuard);
		}
	}
	Render_SetWorld();
	g_LightMat.params.lighting = 0;
	Render_SetMaterial(&g_LightMat, 0);
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
	Rd_DevSetVertexShader(dev, D3DFVF_XYZ_DIFFUSE);
	Rd_DevDrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 3, v, sizeof(v[0]));
	g_RenderTris += 3;
	g_RenderVerts += 5;
	if (g_ShowLights > 1) {
		type = Light_TypeStr(self->type);
		a = self->d3d.Position;
		Debug_Text3D(g_FntTerm8, 0.0f, 0.0f, &a, 0xffffff, "%s [%s,%d] [%f]", self->node->name, type, self->sector,
			(double)self->range);
		a = self->d3d.Position;
		Debug_Text3D(g_FntTerm8, 0.0f, 12.0f, &a, color, "%.1f %.1f %.1f", (double)self->d3d.Diffuse.r,
			(double)self->d3d.Diffuse.g, (double)self->d3d.Diffuse.b);
	}
	if (g_ShowLights > 2) {
		a.x = 0.0f;
		a.y = 0.0f;
		a.z = 0.0f;
		Debug_DrawSphereM(&m, &a, self->range, g_DebugColors[self->index_list & 0xf], 10, 10);
	}
	if (self->range != 0.0f) {
		rad = self->range;
		if (self->type == LIGHT_SPOT) {
			rad = rad * 0.1f;
		}
		a.x = 0.0f;
		a.y = 0.0f;
		a.z = 0.0f;
		Debug_DrawAxes(&m, &a, rad);
	}
	if (self->type == LIGHT_SPOT) {
		b = self->d3d.Direction;
		len = (float)__builtin_sqrt((double)(b.y * b.y + b.x * b.x + b.z * b.z));
		if ((double)len != 0.0) {
			k = (float)(1.0 / (double)len);
			b.x = self->d3d.Direction.x * k;
			b.y = self->d3d.Direction.y * k;
			b.z = self->d3d.Direction.z * k;
		}
		b.x = self->d3d.Position.x + b.x * self->range;
		b.y = self->d3d.Position.y + b.y * self->range;
		b.z = self->d3d.Position.z + b.z * self->range;
		a = self->d3d.Position;
		Debug_DrawLine(&a, &b, 0xffff00);
		Debug_DrawConeM(&m, self->range, Light_DegToRad(self->phi));
	}
}

void Light_SetDirLight(uint32_t idx, const color4_t *color, const vec3_t *dir) {
	D3DLight l;

	memset(&l, 0, sizeof(l));
	l.Diffuse = *color;
	l.Specular = *color;
	l.Ambient = (color4_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	l.Type = D3DLIGHT_DIRECTIONAL;
	l.Direction = *dir;
	D3D_SetLight(idx, &l);
}

void CLight_EnableD3D(CLight *self, byte enable) {
	Light_EnableD3D(self, enable);
}

/* the members after the version as the file has them */
static const struct {
	uint8_t off, size;
} lightFields[] = {
	{ offsetof(CLight, sector), 4 }, { offsetof(CLight, type), 4 }, { offsetof(CLight, shadow), 1 },
	{ offsetof(CLight, pos), 12 }, { offsetof(CLight, dir), 12 }, { offsetof(CLight, color), 8 },
	{ offsetof(CLight, rangeStart), 4 }, { offsetof(CLight, range), 4 }, { offsetof(CLight, theta), 4 },
	{ offsetof(CLight, phi), 4 }, { offsetof(CLight, mult), 4 }, { offsetof(CLight, radCoef), 4 },
};

byte CLight_Read(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CLight *self = (CLight *)base;
	Light_Link(&self->node, ((MeshD3D_FileCtx *)ctx)->node);
	CFileIO_BeginBlock(f, "LUZ");
	CHECK(CFileIO_ReadInt(f) == 1);
	CHECK(self->sector == -1);
	CHECK(!self->ena_D3D);
	for (size_t i = 0; i < sizeof(lightFields) / sizeof(lightFields[0]); i++) {
		CFileIO_Read(f, (byte *)self + lightFields[i].off, lightFields[i].size);
	}
	CLight_SetActive(self, CFileIO_ReadBool(f));
	CFileIO_Error(f, "LUZ");
	return 1;
}

void Light_List(void) {
	E3D_DebugPrintf("\nLuces:");
	for (int i = 0; i < g_nLights; i++) {
		CLight *l = g_Lights[i];
		const char *name = l->node ? l->node->name : "(null)";
		E3D_DebugPrintf("\n  '%s'  tipo: %s  activa: %s  act_d3d: %s", name, Light_TypeStr(l->type),
			l->active ? "SI" : "NO", l->act_D3D ? "SI" : "NO");
		CHECK(!l->active || l->index_list == i);
	}
	E3D_DebugPrintf("\n  %d luces (%d activas, %d act_d3d)", g_LightsTotal, g_nLights, g_LightsD3D);
}

byte CLight_Write(IObject3D *base, CFileIO *f, MeshD3D_FileCtx *ctx) {
	CLight *self = (CLight *)base;
	CHECK(self->node == ((MeshD3D_FileCtx *)ctx)->node);
	CFileIO_BeginBlockWrite(f, "LUZ");
	CFileIO_WriteInt(f, 1);
	CHECK(!self->ena_D3D);
	Light_Release(&self->nodeTarget);
	self->nodeTarget = NULL;
	for (size_t i = 0; i < sizeof(lightFields) / sizeof(lightFields[0]); i++) {
		CFileIO_Write(f, (byte *)self + lightFields[i].off, lightFields[i].size);
	}
	CFileIO_WriteInt(f, self->active);
	CFileIO_EndBlockWrite(f, "LUZ");
	return 1;
}

void CLight_Update(IObject3D *base, CRenderNode *arg) {
	CLight *self = (CLight *)base;
	(void)arg;
	if (!self->active) {
		return;
	}
	D3DLight *l = &self->d3d;
	CLight_FillD3D(self, l);
	CNode3D *n = self->node;
	if (n) {
		float x = n->m.m[12], y = n->m.m[13], z = n->m.m[14];
		l->Position = (vec3_t){ x, y, z };
		if (l->Type != D3DLIGHT_POINT) {
			CNode3D *t = self->nodeTarget;
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
		if (CNode_LightColor(self->node, &c, &k)) {
			col = (color4_t){ c.x * k, c.y * k, c.z * k, 1.0f };
		} else {
			col = Light_Color(self);
		}
		l->Specular = col;
		l->Diffuse = col;
	}
	CLight_AdjustD3D(self, l);
	if (!self->active) {
		return;
	}
	byte vis = 1;
	if (self->sector >= 0 && g_LightSector) {
		vis = Sector_IsVisible(self->sector);
	}
	if (vis && g_pd3dDevice && l->Type != D3DLIGHT_DIRECTIONAL && LIGHT_RANGE_INF > l->Range) {
		vec3_t p = l->Position;
		vis = Sph_IsVisible(&p, l->Range, 0);
	}
	CLight_UpdateD3D(self, !vis);
}

void Light_EnableAll(byte enable) {
	CHECK(g_pd3dDevice);
	if (g_LightDefer) {
		enable = 0;
	}
	for (int i = 0; i < g_nLights; i++) {
		CLight *l = g_Lights[i];
		CHECK(l->active);
		CHECK(l->index_list == i);
		Light_EnableD3D(l, enable ? l->act_D3D : 0);
	}
}

void CLight_AdjustD3D(CLight *self, D3DLight *l) {
	byte spot = l->Type == D3DLIGHT_SPOT && g_SpotlightAdjust;

	if (g_LightAtten2 && self->range != 0.0f) {
		float k = 1.0f / self->intensity;
		l->Diffuse.r = l->Diffuse.r * k;
		l->Diffuse.g = l->Diffuse.g * k;
		l->Diffuse.b = l->Diffuse.b * k;
		l->Diffuse.a = l->Diffuse.a * k;
		l->Attenuation0 = l->Attenuation0 / (self->mult * g_LightMult2 * self->intensity);
	} else {
		float m = spot ? g_SpotlightMult : g_LightMult;
		if (*g_LightMultName && !Wild_MatchLower(g_LightMultName, self->node->name)) {
			m = 1.0f;
		}
		l->Diffuse.r = l->Diffuse.r * m * self->mult;
		l->Diffuse.g = l->Diffuse.g * m * self->mult;
		l->Diffuse.b = l->Diffuse.b * m * self->mult;
		l->Diffuse.a = l->Diffuse.a * m * self->mult;
		float a = spot ? g_SpotlightAtten : g_LightAtten;
		l->Attenuation2 = self->range != 0.0f ? a / (self->range * self->range) : 0.0f;
	}
	if (g_LightGamma != 1.0f) {
		double e = (double)(1.0f / g_LightGamma);
		l->Diffuse.r = (float)pow((double)l->Diffuse.r, e);
		l->Diffuse.g = (float)pow((double)l->Diffuse.g, e);
		l->Diffuse.b = (float)pow((double)l->Diffuse.b, e);
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

void CLight_SetNode(CLight *self, CNode3D *node) {
	Light_Link(&self->node, node);
}

void CLight_SetNodeTarget(CLight *self, CNode3D *node) {
	Light_Link(&self->nodeTarget, node);
}

void CLight_SetActive(CLight *self, byte active) {
	if (active) {
		if (!self->active) {
			Light_Add(self);
		}
	} else if (self->active) {
		Light_Remove(self);
	}
}

int Light_Compare(const void *a, const void *b) {
	const CLight *la = *(CLight *const *)a, *lb = *(CLight *const *)b;
	return lb->weight > la->weight ? -1 : 1;
}

void CLight_FillD3D(CLight *self, D3DLight *l) {
	switch (self->type) {
	case LIGHT_OMNI:
		l->Type = D3DLIGHT_POINT;
		break;
	case LIGHT_SPOT:
		l->Type = D3DLIGHT_SPOT;
		break;
	case LIGHT_DIR:
		l->Type = D3DLIGHT_DIRECTIONAL;
		break;
	default:
		CHECK(0);
	}
	color4_t c = Light_Color(self);
	l->Diffuse = c;
	l->Specular = c;
	l->Ambient = (color4_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	l->Position = self->pos;
	l->Direction = self->dir;
	l->Range = self->range == 0.0f ? LIGHT_RANGE_INF : self->range;
	l->Falloff = 1.0f;
	l->Attenuation0 = 1.0f;
	l->Attenuation1 = 0.0f;
	l->Attenuation2 = self->range == 0.0f ? 0.0f : g_LightAtten / (self->range * self->range);
	l->Theta = Light_DegToRad(self->theta);
	l->Phi = Light_DegToRad(self->phi);
}

int Light_NumD3D(void) {
	return g_LightsD3D;
}

int Light_NumActive(void) {
	return g_nLights;
}

int Light_NumTotal(void) {
	return g_LightsTotal;
}

void CLight_UpdateD3D(CLight *self, byte hide) {
	if (!g_pd3dDevice) {
		return;
	}
	byte act = hide ? 0 : self->active;
	if (act != self->act_D3D) {
		self->act_D3D = act;
		g_LightsD3D += act ? 1 : -1;
		if (!g_LightDefer) {
			CLight_EnableD3D(self, act);
		} else {
			Light_EnableD3D(self, 0);
		}
	}
	if (self->act_D3D) {
		D3D_SetLight(self->indexD3D, &self->d3d);
	}
}

box_t *Box_Zero(box_t *self) {
	self->min = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->max = (vec3_t){ 0.0f, 0.0f, 0.0f };
	return self;
}

/* the slot InitD3D of a class that does nothing there */
void IObject3D_InitD3DNop(IObject3D *self, void *dev) {
	(void)self;
	(void)dev;
}

/* the slot Read of a class that does only return 0 */
byte IObject3D_ReadFalse(IObject3D *self, CFileIO *f, MeshD3D_FileCtx *ctx) {
	(void)self;
	(void)f;
	(void)ctx;
	return 0;
}

/* the slot ReleaseD3D of a class that does nothing there */
void IObject3D_ReleaseD3DNop(IObject3D *self, void *dev) {
	(void)self;
	(void)dev;
}

/* the slot Render of a class that does nothing there */
void IObject3D_RenderNop(IObject3D *self, void *dev, CRenderNode *r) {
	(void)self;
	(void)dev;
	(void)r;
}

/* the slot Update of a class that does nothing there */
void IObject3D_UpdateNop(IObject3D *self, CRenderNode *r) {
	(void)self;
	(void)r;
}

/* the slot Write of a class that does only return 0 */
byte IObject3D_WriteFalse(IObject3D *self, CFileIO *f, MeshD3D_FileCtx *ctx) {
	(void)self;
	(void)f;
	(void)ctx;
	return 0;
}

/* the vtable of CLight */
const IObject3DVtbl CLight_vftable = {
	.Delete = CLight_Delete,
	.Update = CLight_Update,
	.Render = CLight_Render,
	.InitD3D = CLight_Reset,
	.ReleaseD3D = CLight_Hide,
	.Write = CLight_Write,
	.Read = CLight_Read,
};
