/*
 * Engine/flare.cpp of the engine3d: the lens flares, see include/scrap/flare.h.
 */
#include "scrap/e3dutil.h"
#include "scrap/flare.h"
#include "scrap/luz.h"
#include "scrap/mundo.h"
#include "scrap/render.h"
#include "scrap/sistpart.h"
#include "scrap/crt.h"
#include "scrap/texto.h"

byte Floats11_Equal(const float *a, const float *b) {
	static const byte order[11] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 6 };
	for (int i = 0; i < 11; i++) {
		if (a[order[i]] != b[order[i]]) {
			return 0;
		}
	}
	return 1;
}

CFlare *__fastcall CFlare_ctor(CFlare *self) {
	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->nodo = NULL;
	self->vftable = CFLARE_VFTABLE;
	self->clase = CLASE_OBJ3D_FLARE;
	self->escala[0] = g_FlareTam;
	self->escala[1] = g_FlareTam;
	self->color = 0xffffff;
	self->visible = 1;
	self->visTrace = 0;
	self->intens = 1.0f;
	self->lastTime = 0;
	self->nextTrace = 0;
	self->grf = NULL;
	self->transp = 1.0f;
	return self;
}

static void CFlare_FreeGrf(CFlare *self) {
	if (self->grf) {
		CGrafico2D_dtor(self->grf);
		operator_delete_sized(self->grf, sizeof(CGrafico2D));
		self->grf = NULL;
	}
}

void __fastcall CFlare_dtor(CFlare *self) {
	self->vftable = CFLARE_VFTABLE;
	CFlare_FreeGrf(self);
	CObjBase3D_dtor((IObjeto3D *)self);
}

CFlare *__thiscall CFlare_Delete(CFlare *self, byte flags) {
	CFlare_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CFlare));
	}
	return self;
}

void __fastcall CFlare_Draw(CFlare *self) {
	self->grf->color = self->color;
	self->grf->escala[0] = self->escala[0];
	self->grf->escala[1] = self->escala[1];
	CGrafico2D_Render(self->grf);
}

/* the fade step for the time step dt: in when shown, out (slower below 0.2) when not */
static float CFlare_Fade(const CFlare *self, float dt) {
	float t = self->transp;
	if (self->visible) {
		return (self->visTrace ? 1.0f : g_FlareFundidoEnt) * dt + t;
	}
	float k = self->visTrace ? 1.0f : g_FlareFundidoSal;
	if ((double)t >= 0.2) {
		return (1.0f - k * dt) * t;
	}
	return t - k * dt * 0.2f;
}

void __thiscall CFlare_Render(CFlare *self, void *dev, CNodoR *r) {
	CHECK(self->grf);
	CHECK(self->nodo);
	if (g_Sobredib || g_SkipFlares) {
		return;
	}
	float now = g_TimeMsF;
	float dt = (now - (float)(double)self->lastTime) * 0.001f;
	self->lastTime = (uint32_t)now;
	const float *m = self->nodo->m.m;
	vec3_t p = { m[12], m[13], m[14] };
	if ((float)(double)(self->nextTrace + 50) < g_TimeMsF) {
		CSuelo *suelo = Mundo_Get()->ObjSuelo;
		self->visTrace = !suelo || !CSuelo_Trace(suelo, &p, Render_GetEyePos(), NULL, 0.0f, NULL);
		uint32_t rnd = (uint32_t)crt_rand() & 7;
		self->nextTrace = (uint32_t)g_TimeMsF + rnd;
	}
	self->visible = self->visTrace;
	const matrix_t *proj = Render_GetProj();
	matrix_t vp;
	matrix_t mvp = *Matrix_MulD3D(&vp, Render_GetView(), proj);
	uint32_t view[6];
	D3D_METHOD(dev, d3dGetViewport_t, D3DDEV_GETVIEWPORT)(dev, view);
	vec3_t s;
	if (Vec3_TransformH(&s, &p, &mvp) || 0.02 > (double)s.z || s.z > 1.0f) {
		self->visible = 0;
		return;
	}
	if (__builtin_fabs((double)s.x) > 1.0) {
		self->visible = 0;
	}
	if (__builtin_fabs((double)s.y) > 1.0) {
		self->visible = 0;
	}
	float w = (float)(double)view[2], x = (float)(double)view[0];
	float h = (float)(double)view[3], y = (float)(double)view[1];
	self->grf->x = (w * s.x + x) * 0.5f + w * 0.5f;
	self->grf->y = h * 0.5f - (h * s.y + y) * 0.5f;
	byte alpha = self->grf->Mat->params.diffuse[3];
	if (g_FlareAtenDist) {
		const vec3_t *eye = Render_GetEyePos();
		float dx = self->pos.x - eye->x, dy = self->pos.y - eye->y, dz = self->pos.z - eye->z;
		float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (d > g_FlareAtenDistFin) {
			self->visible = 0;
		}
	}
	self->transp = CFlare_Fade(self, dt);
	float t = self->intens * self->transp;
	self->transp = t;
	if (!(0.0f >= t)) {
		t = 1.0f < t ? 1.0f : t;
	} else {
		t = 0.0f;
	}
	self->transp = t;
	if (0.01 >= (double)t) {
		return;
	}
	if (g_FlareVerInfo || g_ShowFlaresInfo) {
		Texto_PrintfRel(0, 0x10, "transp: %f  '%s'", (double)t, self->nodo->name);
	}
	int a = (int)(self->transp * (float)alpha);
	a = a <= 0 ? 0 : a < 0xff ? a : 0xff;
	if (self->grf->Mat->params.srcBlend == 2) {
		self->color = (uint32_t)a * 0x01010101u;
	} else {
		self->color = (uint32_t)a << 24 | 0xffffff;
	}
	SistPart_Draw((CSistPart *)self, r);
}

void __thiscall CFlare_SetTexture(CFlare *self, const char *tex, const char *opac, int src, int dst, byte alpha) {
	CFlare_FreeGrf(self);
	CGrafico2D *g = operator_new(sizeof(CGrafico2D));
	self->grf = g ? CGrafico2D_ctor(g) : NULL;
	CGrafico2D_CreateMat(self->grf, tex, opac, src, dst, alpha, 0);
	self->grf->centrado = 1;
	self->grf->sinTextura = 1;
	self->grf->tamPantalla = 1;
}

void __thiscall CFlare_SetTextureAdd(CFlare *self, const char *tex, const char *opac) {
	CFlare_SetTexture(self, tex, opac, 5, 6, 0xff);
}
