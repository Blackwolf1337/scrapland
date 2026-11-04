/*
 * Engine/flare.cpp of the engine3d: the lens flares, see include/scrap/flare.h.
 */
#include "scrap/rdev.h"
#include "scrap/e3dutil.h"
#include "scrap/flare.h"
#include "scrap/light.h"
#include "scrap/e3dworld.h"
#include "scrap/render.h"
#include "scrap/particlesystem.h"
#include "scrap/crt.h"
#include "scrap/text.h"
#include "scrap/staticinit.h"

#define CFLARE_VFTABLE (&CFlare_vftable)
#define g_SkipFlares (GVar_R_SkipFlares.value.i)		/* R_SkipFlares */
#define g_ShowFlaresInfo (GVar_R_ShowFlaresInfo.value.i)	/* R_ShowFlaresInfo */
static cGVar GVar_R_SkipFlares;
static cGVar GVar_R_ShowFlaresInfo;
static const IObject3DVtbl CFlare_vftable;

CFlare *CFlare_ctor(CFlare *self) {
	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->node = NULL;
	self->vftable = CFLARE_VFTABLE;
	self->class = CLASS_OBJ3D_FLARE;
	self->scale[0] = g_FlareSize;
	self->scale[1] = g_FlareSize;
	self->color = 0xffffff;
	self->visible = 1;
	self->visTrace = 0;
	self->intens = 1.0f;
	self->lastTime = 0;
	self->nextTrace = 0;
	self->graphic = NULL;
	self->transp = 1.0f;
	return self;
}

static void CFlare_FreeGraphic(CFlare *self) {
	if (self->graphic) {
		CGraphic2D_dtor(self->graphic);
		operator_delete(self->graphic);
		self->graphic = NULL;
	}
}

static void CFlare_dtor(CFlare *self) {
	CFlare_FreeGraphic(self);
	CObjBase3D_dtor((CObjBase3D *)self);
}

static void CFlare_Free(IObject3D *base) {
	CFlare *self = (CFlare *)base;
	CFlare_dtor(self);
	operator_delete(self);
}

void CFlare_Draw(CFlare *self) {
	self->graphic->color = self->color;
	self->graphic->scale[0] = self->scale[0];
	self->graphic->scale[1] = self->scale[1];
	CGraphic2D_Render(self->graphic);
}

/* the fade step for the time step dt: in when shown, out (slower below 0.2) when not */
static float CFlare_Fade(const CFlare *self, float dt) {
	float t = self->transp;
	if (self->visible) {
		return (self->visTrace ? 1.0f : g_FlareFadeIn) * dt + t;
	}
	float k = self->visTrace ? 1.0f : g_FlareFadeOut;
	if ((double)t >= 0.2) {
		return (1.0f - k * dt) * t;
	}
	return t - k * dt * 0.2f;
}

static void CFlare_Render(IObject3D *base, void *dev, CRenderNode *r) {
	CFlare *self = (CFlare *)base;
	CHECK(self->graphic);
	CHECK(self->node);
	if (g_Overdraw || g_SkipFlares) {
		return;
	}
	float now = g_TimeMsF;
	float dt = (now - (float)(double)self->lastTime) * 0.001f;
	self->lastTime = (uint32_t)now;
	const float *m = self->node->m.m;
	vec3_t p = { m[12], m[13], m[14] };
	if ((float)(double)(self->nextTrace + 50) < g_TimeMsF) {
		CGround *ground = World_Get()->ObjGround;
		self->visTrace = !ground || !CGround_Trace(ground, &p, Render_GetEyePos(), NULL, 0.0f, NULL);
		uint32_t rnd = (uint32_t)crt_rand() & 7;
		self->nextTrace = (uint32_t)g_TimeMsF + rnd;
	}
	self->visible = self->visTrace;
	const matrix_t *proj = Render_GetProj();
	matrix_t vp;
	matrix_t mvp = *Matrix_MulD3D(&vp, Render_GetView(), proj);
	uint32_t view[6];
	Rd_DevGetViewport(dev, view);
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
	self->graphic->x = (w * s.x + x) * 0.5f + w * 0.5f;
	self->graphic->y = h * 0.5f - (h * s.y + y) * 0.5f;
	byte alpha = self->graphic->Mat->params.diffuse[3];
	if (g_FlareAttenDist) {
		const vec3_t *eye = Render_GetEyePos();
		float dx = self->pos.x - eye->x, dy = self->pos.y - eye->y, dz = self->pos.z - eye->z;
		float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (d > g_FlareAttenDistEnd) {
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
	if (g_FlareShowInfo || g_ShowFlaresInfo) {
		Text_PrintfRel(0, 0x10, "transp: %f  '%s'", (double)t, self->node->name);
	}
	int a = (int)(self->transp * (float)alpha);
	a = a <= 0 ? 0 : a < 0xff ? a : 0xff;
	if (self->graphic->Mat->params.srcBlend == 2) {
		self->color = (uint32_t)a * 0x01010101u;
	} else {
		self->color = (uint32_t)a << 24 | 0xffffff;
	}
	ParticleSystem_Draw((CParticleSystem *)self, r);
}

void CFlare_SetTexture(CFlare *self, const char *tex, const char *opac, int src, int dst, byte alpha) {
	CFlare_FreeGraphic(self);
	CGraphic2D *g = operator_new(sizeof(CGraphic2D));
	self->graphic = g ? CGraphic2D_ctor(g) : NULL;
	CGraphic2D_CreateMat(self->graphic, tex, opac, src, dst, alpha, 0);
	self->graphic->centered = 1;
	self->graphic->noTexture = 1;
	self->graphic->screenSize = 1;
}

/* the vtable of CFlare */
static const IObject3DVtbl CFlare_vftable = {
	.Free = CFlare_Free,
	.Update = IObject3D_UpdateNop,
	.Render = CFlare_Render,
	.InitD3D = IObject3D_InitD3DNop,
	.ReleaseD3D = IObject3D_ReleaseD3DNop,
	.Write = IObject3D_WriteFalse,
	.Read = IObject3D_ReadFalse,
};

static cGVar GVar_R_ShowFlaresInfo;
static cGVar GVar_R_SkipFlares;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_SkipFlares, &GVar_R_SkipFlares, "R_SkipFlares", 0, "Disable flares rendering (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_ShowFlaresInfo, &GVar_R_ShowFlaresInfo, "R_ShowFlaresInfo", 0, "Show flares debug info (0/1)", 0)
