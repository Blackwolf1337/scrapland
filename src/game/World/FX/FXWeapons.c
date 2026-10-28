/*
 * World/FX/FXWeapons.cpp of the game: the effects of the weapon shots, see include/scrap/fxweapons.h.
 */
#include <string.h>

#include "scrap/fxvehicle.h"
#include "scrap/fxweapons.h"
#include "scrap/mathlib.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

#define g_RandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */
#define FX_PI 3.1415927f				/* 0x40490fdb */

/* rand() in [0, 1] times range, plus add (all in double) */
static inline float FXW_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * g_RandScale * range + add);
}

static inline float FXW_Delta(void) {
	return cTimer_GetDelta(&g_pWorld->timer);
}

/* the node of name in the model, CHECKed */
static int FXW_Node(CGameModel *model, const char *name) {
	int node = CGameModel_FindNodoR(model, name);
	CHECK(node > -1);
	return node;
}

/* the point 50 in front of the model (its z axis turned by its angles) */
static void FXW_Front(CGameModel *model, vec3_t *out, vec3_t *pos) {
	vec3_t rot, off, r;
	CGameModel_GetPos(model, pos);
	CGameModel_GetRot(model, &rot);
	off.x = 0.0f;
	off.y = 0.0f;
	off.z = 50.0f;
	const vec3_t *p = Vec3_Rotate(&r, &off, &rot);
	out->x = p->x + pos->x;
	out->y = p->y + pos->y;
	out->z = p->z + pos->z;
}

/* the smoke trails of the missiles: the end of the fade (0 ends the debris), the debris hidden when it starts */
static byte FXW_MissileAlive(cDebris *debris, cDebrisSys *sys, const float *time, float *fadeStart,
	float *lastShrink, const float *fadeTime) {
	if (cDebris_Update(debris, sys) && CGameModel_IsEnabled(Debris_Model(debris))) {
		return 1;
	}
	float t = *time;
	float fs = *fadeStart;
	if (0.0f > fs) {
		*fadeStart = t;
		*lastShrink = t;
		CGameModel_Disable(Debris_Model(debris));
		return 1;
	}
	if (t - fs >= *fadeTime) {
		return 0;
	}
	return 1;
}

/* ---- constructors ---- */

void *__thiscall cObjElementFXATPC_ctor(void *self) {
	cObjElementFXATPC *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXATPC_VFTABLE);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particles2);
	CGameTrail_ctor(&s->trail1c4);
	CGameTrail_ctor(&s->trail1c8);
	CGameTrail_ctor(&s->trail1d0);
	CGameTrail_ctor(&s->trail1d8);
	CGameTrail_ctor(&s->trail1e0);
	s->on = 0;
	s->shooter = NULL;
	*(uint32_t *)s->slotOn = 0;
	s->time = 0.0f;
	s->angle = 0.0f;
	return s;
}

void *__thiscall cObjElementFXDevastatorShoot_ctor(void *self) {
	cObjElementFXDevastatorShoot *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXDEVASTATORSHOOT_VFTABLE);
	CGameTrail_ctor(&s->trail);
	s->first = 1;
	s->on = 0;
	s->time = 0.0f;
	s->duration = 0.0f;
	return s;
}

void *__thiscall cObjElementFXInfernoExplosion_ctor(void *self) {
	cObjElementFXInfernoExplosion *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXINFERNOEXPLOSION_VFTABLE);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->cores);
	FXObj_TrailsCtor(s->trails, FXINFERNO_TRAILS);
	s->on = 0;
	s->time = 0.0f;
	s->stepTime = 0.0f;
	return s;
}

void *__thiscall cObjElementFXInfernoMissile_ctor(void *self) {
	cObjElementFXInfernoMissile *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXINFERNOMISSILE_VFTABLE);
	CGameTrail_ctor(&s->trail);
	CGameParticles_ctor(&s->particles);
	CGameLensFlare_ctor(&s->flare);
	s->on = 0;
	return s;
}

void *__thiscall cObjElementFXLaser_ctor(void *self) {
	cObjElementFXLaser *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXLASER_VFTABLE);
	CGameTrail_ctor(&s->trail);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	return s;
}

void *__thiscall cObjElementFXSwarmMissile_ctor(void *self) {
	cObjElementFXSwarmMissile *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXSWARMMISSILE_VFTABLE);
	CGameTrail_ctor(&s->trail);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	return s;
}

void *__thiscall cObjElementFXTesla_ctor(void *self) {
	cObjElementFXTesla *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXTESLA_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXObj_TrailsCtor(s->trails, FXTESLA_SLOTS * FXTESLA_TRAILS);
	s->on = 0;
	s->emitter = NULL;
	s->target = NULL;
	s->numPts = 10;
	s->name[0] = 0;
	s->shooting = 0;
	s->first = 1;
	s->time = 0.0f;
	s->fadeStart = -1.0f;
	s->followTime = g_FXTeslaFollowTime;
	return s;
}

/* ---- destructors ---- */

void __thiscall cObjElementFXATPC_dtor(void *self) {
	cObjElementFXATPC *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXATPC_VFTABLE);
	CGameTrail_dtor(&s->trail1e0);
	CGameTrail_dtor(&s->trail1d8);
	CGameTrail_dtor(&s->trail1d0);
	CGameTrail_dtor(&s->trail1c8);
	CGameTrail_dtor(&s->trail1c4);
	CGameParticles_Release(&s->particles2);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXDevastatorShoot_dtor(void *self) {
	cObjElementFXDevastatorShoot *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXDEVASTATORSHOOT_VFTABLE);
	CGameTrail_dtor(&s->trail);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXInfernoExplosion_dtor(void *self) {
	cObjElementFXInfernoExplosion *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXINFERNOEXPLOSION_VFTABLE);
	FXObj_TrailsDtor(s->trails, FXINFERNO_TRAILS);
	CGameParticles_Release(&s->cores);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXInfernoMissile_dtor(void *self) {
	cObjElementFXInfernoMissile *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXINFERNOMISSILE_VFTABLE);
	CGameLensFlare_dtor(&s->flare);
	CGameParticles_Release(&s->particles);
	CGameTrail_dtor(&s->trail);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXLaser_dtor(void *self) {
	cObjElementFXLaser *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXLASER_VFTABLE);
	CGameParticles_Release(&s->particles);
	CGameTrail_dtor(&s->trail);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXSwarmMissile_dtor(void *self) {
	cObjElementFXSwarmMissile *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXSWARMMISSILE_VFTABLE);
	CGameParticles_Release(&s->particles);
	CGameTrail_dtor(&s->trail);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXTesla_dtor(void *self) {
	cObjElementFXTesla *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXTESLA_VFTABLE);
	FXObj_TrailsDtor(s->trails, FXTESLA_SLOTS * FXTESLA_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

/* ---- deleting destructors (vf00) ---- */

void *__thiscall cObjElementFXATPC_Delete(void *self, uint32_t flags) {
	cObjElementFXATPC_dtor(self);
	return FXObj_Delete(self, flags, sizeof(cObjElementFXATPC));
}

void *__thiscall cObjElementFXDevastatorShoot_Delete(void *self, uint32_t flags) {
	cObjElementFXDevastatorShoot_dtor(self);
	return FXObj_Delete(self, flags, sizeof(cObjElementFXDevastatorShoot));
}

void *__thiscall cObjElementFXInfernoExplosion_Delete(void *self, uint32_t flags) {
	cObjElementFXInfernoExplosion_dtor(self);
	return FXObj_Delete(self, flags, sizeof(cObjElementFXInfernoExplosion));
}

void *__thiscall cObjElementFXInfernoMissile_Delete(void *self, uint32_t flags) {
	cObjElementFXInfernoMissile_dtor(self);
	return FXObj_Delete(self, flags, sizeof(cObjElementFXInfernoMissile));
}

void *__thiscall cObjElementFXLaser_Delete(void *self, uint32_t flags) {
	cObjElementFXLaser_dtor(self);
	return FXObj_Delete(self, flags, sizeof(cObjElementFXLaser));
}

void *__thiscall cObjElementFXSwarmMissile_Delete(void *self, uint32_t flags) {
	cObjElementFXSwarmMissile_dtor(self);
	return FXObj_Delete(self, flags, sizeof(cObjElementFXSwarmMissile));
}

void *__thiscall cObjElementFXTesla_Delete(void *self, uint32_t flags) {
	cObjElementFXTesla_dtor(self);
	return FXObj_Delete(self, flags, sizeof(cObjElementFXTesla));
}

/* ---- cObjElementFXATPC ---- */

#define g_ATPCShootSize GAME_DATA(float, 0x448c8c)
#define FXATPC_RAMP_CORE 0x4317a0	/* [-16..0], by the time */
#define FXATPC_RAMP_GLOW 0x431758	/* [2] */
#define FXATPC_RAMP_RING 0x43177c

static inline uint32_t FXW_Gray(int a) {
	uint32_t u = (uint32_t)a;
	return ((~u << 8 | u) << 8 | u) << 8 | u;
}

/* the smooth step (3 - 2x) x x */
static inline float FXW_Smooth(float x) {
	return (3.0f - (x + x)) * (x * x);
}

/* the point at angle a of a ring around the beam: (0, 1, 0) turned by a around z, then by the angles of the beam */
static void FXW_RingPoint(vec3_t *out, float a, const vec3_t *ang) {
	vec3_t up, rv, o;
	rv.x = 0.0f;
	rv.y = 0.0f;
	rv.z = a;
	up.x = 0.0f;
	up.y = 1.0f;
	up.z = 0.0f;
	Vec3_Rotate(out, Vec3_Rotate(&o, &up, &rv), ang);
}

/* the beam: the line of trail1c8 fades in, while the shot lasts the core (trail1c4), the turning rings (trail1d8,
 * trail1d0 and trail1e0 for the dudv pass) and the glow particles at both ends */
void __thiscall cObjElementFXATPC_Update(cObjElementFXATPC *self, cDebris *debris, float dt) {
	(void)dt;
	cEntity *e = self->shooter;
	if (e && (Entity_Removed(e) || !e->flag_4c)) {
		self->shooter = NULL;
	}
	float d = FXW_Delta();
	CGameModel *dm = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	self->time = self->time + d;
	CGameModel_SetSortZ(dm, -34.0f, 0.0f);
	vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
	CGameModel_SetPos(dm, &cam);
	int up = 0;
	if (self->shooter) {
		up = cWeaponManager_GetUpgrade(Vehicle_WeapMgr(self->shooter), g_ATPCIDWeap);
	}
	float u = self->time / g_ATPCTrailTime;
	float s1 = FXW_Smooth(u);
	vec3_t dir;
	float len;
	{
		float dz = self->to.z - self->from.z;
		float dx = self->to.x - self->from.x;
		float dy = self->to.y - self->from.y;
		len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (len == 0.0f) {
			dir.x = dx;
			dir.y = dy;
			dir.z = dz;
		} else {
			float inv = 1.0f / len;
			dir.x = dx * inv;
			dir.y = dy * inv;
			dir.z = dz * inv;
		}
	}
	float v = self->time / (g_ATPCTrailTime * 2.0f);
	float c = 0.0f > v ? 0.0f : Float_MinSS(1.0f, v);
	float fade = FXW_Smooth(FXW_Smooth(c));

	/* the line, one point per 1500 */
	int n = (int)(len / 1500.0f);
	if (n >= CGameTrail_GetMaxTrails(&self->trail1c8)) {
		n = CGameTrail_GetMaxTrails(&self->trail1c8) - 1;
	}
	if (n > 3) {
		float inv = 1.0f / (float)(n - 1);
		int a = 0x40 - (int)(fade * -191.0f);
		float sx = (self->to.x - self->from.x) * inv;
		float sy = (self->to.y - self->from.y) * inv;
		float sz = (self->to.z - self->from.z) * inv;
		CGameTrail_SetTransp(&self->trail1c8, (byte)a);
		CGameTrail_Reset(&self->trail1c8);
		float w = fade * 800.0f + 400.0f;
		float fn = (float)n;
		for (int i = 0; i < n; i++) {
			float fi = (float)i;
			vec3_t pt;
			pt.x = self->from.x + sx * fi;
			pt.y = sy * fi + self->from.y;
			pt.z = sz * fi + self->from.z;
			uint32_t color = (uint32_t)(-0x5c - (int)(fi / fn * 92.0f * fade)) << 24 | 0xffffff;
			if (i == 0 || i == n - 1) {
				color = 0xff000000;
			} else if (i == 1 || i == n - 2) {
				color = 0xc4404040;
			} else if (i == 2 || i == n - 3) {
				color = 0x80808080;
			}
			CGameTrail_Add(&self->trail1c8, pt, w, color);
		}
	}

	if (self->time > g_ATPCTrailTime) {
		if (CGameTrail_IsRender(&self->trail1c4)) {
			CGameTrail_Clear(&self->trail1c4);
			CGameTrail_Clear(&self->trail1d8);
			CGameTrail_Clear(&self->trail1d0);
			if (g_SceneDuDvRTarget) {
				CGameTrail_Clear(&self->trail1e0);
			}
			CHECK(p->SistPart);
			int np = CGameParticles_NumParticles(p);
			CGameParticles_KillRange(p, np - 9, np - 1);
		}
		return;
	}

	vec3_t tmp, ang, mid;
	ang = *Vec3_GetRotAng(&tmp, &dir);
	float w0 = (float)Game_Sin((double)Float_MaxSS(s1, 0.2f)) * self->width;

	/* the core */
	CGameTrail_Reset(&self->trail1c4);
	int a = (int)(s1 * 255.0f);
	CGameTrail_SetTransp(&self->trail1c4, (byte)a);
	float k16 = u * -16.0f;
	const uint32_t *ramp = (const uint32_t *)Game_Ptr(FXATPC_RAMP_CORE) - (int)k16;
	float rs2 = 0.75f;
	float w = w0 * 0.75f;
	CGameTrail_Add(&self->trail1c4, self->from, w, *ramp);
	CGameTrail_Add(&self->trail1c4, self->to, w, *ramp);
	w = w0 * 1.5f;
	CGameTrail_Add(&self->trail1c4, self->from, w, *ramp);
	CGameTrail_Add(&self->trail1c4, self->to, w, *ramp);

	/* the ring at the ends (trail1d8), oriented at the middle of the beam */
	mid.x = self->from.x + (self->to.x - self->from.x) * 0.5f;
	mid.y = self->from.y + (self->to.y - self->from.y) * 0.5f;
	mid.z = self->from.z + (self->to.z - self->from.z) * 0.5f;
	CGameTrail_Reset(&self->trail1d8);
	CGameTrail_SetOrient(&self->trail1d8, mid, 1);
	CGameTrail_SetUVScale(&self->trail1d8, len / 4000.0f, 0.0625f);
	float k = u * 16.0f;
	CGameTrail_SetUV(&self->trail1d8, k, (float)(int)k * 0.0625f);
	CGameTrail_SetTransp(&self->trail1d8, (byte)a);
	float r = (up == 10 ? 0.25f : 0.15f) * w0;
	float step = (float)(6.283185307179586 / (double)self->n1d4);
	float an = 0.0f;
	for (int i = 0; i < self->n1d4; i++) {
		vec3_t o, pt;
		FXW_RingPoint(&o, self->angle + an, &ang);
		pt.x = o.x * r + self->from.x;
		pt.y = self->from.y + o.y * r;
		pt.z = self->from.z + o.z * r;
		float rw = step * r * 6.0f;
		CGameTrail_Add(&self->trail1d8, pt, rw, (uint32_t)(int)(u * 255.0f) << 24 | 0xffffff);
		pt.x = o.x * r + self->to.x;
		pt.y = self->to.y + o.y * r;
		pt.z = self->to.z + o.z * r;
		CGameTrail_Add(&self->trail1d8, pt, rw, (uint32_t)(int)((1.0f - u) * 255.0f) << 24 | 0xffffff);
		an = an + step;
		if (!(6.2831855f > an)) {
			break;
		}
	}
	self->angle = d * 10.0f + self->angle;

	/* the rings turning the other way (trail1d0 and trail1e0) */
	CGameTrail_Reset(&self->trail1d0);
	CGameTrail_SetOrient(&self->trail1d0, mid, 1);
	CGameTrail_SetUVScale(&self->trail1d0, len / 1000.0f, 1.0f);
	CGameTrail_SetUV(&self->trail1d0, u * -32.0f, 1.0f);
	CGameTrail_SetTransp(&self->trail1d0, (byte)a);
	if (g_SceneDuDvRTarget) {
		CGameTrail_Reset(&self->trail1e0);
		CGameTrail_SetOrient(&self->trail1e0, mid, 1);
		CGameTrail_SetUVScale(&self->trail1e0, len / 2000.0f, 1.0f);
		CGameTrail_SetUV(&self->trail1e0, k16, 1.0f);
	}
	float r2 = self->width * s1 * (up == 10 ? 0.75f : 0.5f);
	double stepd = 6.283185307179586 / (double)self->n1cc;
	an = 0.0f;
	for (int i = 0; i < self->n1cc; i++) {
		vec3_t o, pt;
		FXW_RingPoint(&o, an - self->angle, &ang);
		pt.y = self->from.y + o.y * r2;
		pt.z = self->from.z + o.z * r2;
		pt.x = o.x * r2 + self->from.x;
		float rw = (float)stepd * r2 * 1.0225f;
		CGameTrail_Add(&self->trail1d0, pt, rw, *ramp);
		pt.y = self->to.y + o.y * r2;
		pt.z = self->to.z + o.z * r2;
		pt.x = o.x * r2 + self->to.x;
		CGameTrail_Add(&self->trail1d0, pt, rw, *ramp);
		if (g_SceneDuDvRTarget) {
			uint32_t gray = FXW_Gray(0x5c - (int)(s1 * 92.0f));
			pt.y = self->from.y + o.y * r2 * 3.0f;
			pt.z = self->from.z + o.z * r2 * 3.0f;
			pt.x = o.x * r2 * 3.0f + self->from.x;
			rw = r2 * 3.0f * (float)stepd * 1.0225f;
			CGameTrail_Add(&self->trail1e0, pt, rw, gray);
			pt.y = self->to.y + o.y * r2 * 3.0f;
			pt.z = self->to.z + o.z * r2 * 3.0f;
			pt.x = o.x * r2 * 3.0f + self->to.x;
			CGameTrail_Add(&self->trail1e0, pt, rw, gray);
		}
		an = an + (float)stepd;
		if (!(6.2831855f > an)) {
			break;
		}
	}

	/* the glow: 4 particles back from each end, smaller one by one */
	if (up != 10) {
		rs2 = 0.5f;
	}
	float size = (self->width + self->width) * s1 * rs2 + 100.0f;
	vec3_t n3;
	FX_Normalize(&n3, self->to.x - self->from.x, self->to.y - self->from.y, self->to.z - self->from.z);
	CHECK(p->SistPart);
	int np = CGameParticles_NumParticles(p);
	float sz10 = size / 10.0f;
	uint32_t alpha = (uint32_t)a << 24;
	uint32_t cg = 0xdc00;
	uint32_t cb = 0x6e;
	int idx = np - 1;
	for (int i = 0; i < 4; i++, idx--, cg -= 0x1e00, cb -= 10) {
		float fi = (float)i;
		CGameParticles_SetParticle(p, idx, self->to.x - n3.x * fi * 340.0f * s1, self->to.y - n3.y * fi * 340.0f * s1,
			self->to.z - n3.z * fi * 340.0f * s1, size - sz10 * fi, cg | cb | alpha | 0xff0000, 2, 0.0f);
	}
	cg = 0xdc00;
	cb = 0x6e;
	idx = np - 5;
	for (int i = 0; i < 4; i++, idx--, cg -= 0x1e00, cb -= 10) {
		float fi = (float)i;
		CGameParticles_SetParticle(p, idx, n3.x * fi * 340.0f * s1 + self->from.x, n3.y * fi * 340.0f * s1 + self->from.y,
			n3.z * fi * 340.0f * s1 + self->from.z, size - sz10 * fi, cg | cb | alpha | 0xff0000, 2, 0.0f);
	}
}

/* the particle of an emission at pos with no velocity: sizes, life, sprite and the rotation 0 */
static void FXW_Still(CGameParticles *p, const vec3_t *pos) {
	p->initPosMin = *pos;
	p->initPosMax = p->initPosMin;
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMax = p->initVelMin;
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = 0.0f;
	p->initAccMin.z = 0.0f;
	p->initAccMax = p->initAccMin;
}

static inline void FXW_SetSprite(CGameParticles *p, byte sprite) {
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
}

static inline void FXW_NoRot(CGameParticles *p) {
	p->rotIni = 0.0f;
	p->rotEnd = 0.0f;
	p->rotVar = 0.0f;
}

/* the shot of the vehicle v from origin to hit: the glow along the weapon slots, along the beam, at the hit and
 * sparks back towards v (target is not used) */
void __thiscall cObjElementFXATPC_Shoot(cObjElementFXATPC *self, cEntity *v, const char *target, vec3_t hit,
	vec3_t origin) {
	(void)target;
	CGameParticles *p = &self->particles;
	matrix_t mat;
	vec3_t pos;

	self->shooter = v;
	self->to = hit;
	self->from = origin;
	CHECK(self->shooter);
	CGameModel *model = FX_Vtbl(self->shooter)->GetModel(self->shooter);
	CHECK(CGameModel_IsLoaded(model));
	self->numSlots = 0;
	for (int i = 0; i < FXATPC_SLOTS; i++) {
		if (cTargetLock_HasSlot(Entity_TargetLock(self->shooter), g_ATPCIDWeap, i)) {
			const matrix_t *m = CGameModel_NodoMatrix(model, &mat,
				(uint32_t)Entity_WeapNodes(self->shooter)[i + g_ATPCIDAmmo * 4], 0, NULL, 0);
			self->numSlots++;
			self->slotOn[i] = 1;
			self->slotPos[i].x = m->m[12];
			self->slotPos[i].y = m->m[13];
			self->slotPos[i].z = m->m[14];
		}
	}
	self->width = (float)self->numSlots * g_ATPCShootSize * 0.2f + g_ATPCShootSize * 0.8f;
	CHECK(p->SistPart);

	/* from each slot to the origin */
	for (int i = 0; i < FXATPC_SLOTS; i++) {
		if (!self->slotOn[i]) {
			continue;
		}
		const vec3_t *sp = &self->slotPos[i];
		float dx = (self->from.x - sp->x) * 0.071428575f;
		float dy = (self->from.y - sp->y) * 0.071428575f;
		float dz = (self->from.z - sp->z) * 0.071428575f;
		for (int k = 0; k < 14; k++) {
			float fk = (float)k;
			pos.x = dx * fk + sp->x;
			pos.y = sp->y + dy * fk;
			pos.z = dz * fk + sp->z;
			FXW_Still(p, &pos);
			p->sizeIni = 450.0f;
			p->sizeEnd = 550.0f;
			p->sizeLife = 0.25f;
			p->lifeTime = g_ATPCTrailTime * 0.4f;
			p->birthRate = 0;
			FXW_SetSprite(p, 2);
			FXW_NoRot(p);
			CGameParticles_Apply(p);
			CGameParticles_SetColorRampOn(p, 1);
			CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXATPC_RAMP_RING));
			CGameParticles_EmitDt(p, 1, 0, g_ATPCTrailTime * 0.2f / 14.0f * (float)k);
		}
	}

	/* along the beam: 6 from 200 before the origin, 5 from 400 before the hit */
	vec3_t dir;
	FX_Normalize(&dir, self->to.x - self->from.x, self->to.y - self->from.y, self->to.z - self->from.z);
	float bx = dir.x * 200.0f;
	float by = dir.y * 200.0f;
	float bz = dir.z * 200.0f;
	for (int k = 0, c = 0, e = 0; c < 0x186; k++, c += 0x41, e += 0x7d) {
		float fk = (float)k;
		pos.x = dir.x * fk * 100.0f + (self->from.x - bx);
		pos.y = dir.y * fk * 100.0f + (self->from.y - by);
		pos.z = dir.z * fk * 100.0f + (self->from.z - bz);
		FXW_Still(p, &pos);
		p->lifeTime = g_ATPCTrailTime * 1.1f;
		p->sizeIni = self->width * 0.65f - (float)c;
		p->sizeLife = 1.0f;
		p->birthRate = 0;
		FXW_SetSprite(p, 5);
		FXW_NoRot(p);
		p->sizeEnd = self->width * 1.45f - (float)e;
		CGameParticles_Apply(p);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXATPC_RAMP_GLOW));
		CGameParticles_EmitN(p, 1, 0);
	}
	bx = dir.x * 400.0f;
	by = dir.y * 400.0f;
	bz = dir.z * 400.0f;
	for (int k = 0; k < 5; k++) {
		float fk = (float)k;
		pos.y = dir.y * fk * 200.0f + (self->to.y - by);
		pos.z = dir.z * fk * 200.0f + (self->to.z - bz);
		pos.x = dir.x * fk * 200.0f + (self->to.x - bx);
		FXW_Still(p, &pos);
		p->lifeTime = g_ATPCTrailTime * 1.1f;
		p->sizeIni = self->width * 0.6f;
		p->sizeLife = 0.5f;
		p->birthRate = 0;
		FXW_SetSprite(p, 5);
		FXW_NoRot(p);
		p->sizeEnd = (self->width + self->width) - (float)__builtin_fabs((double)((float)(k - 2) * 400.0f));
		CGameParticles_Apply(p);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXATPC_RAMP_RING));
		CGameParticles_EmitN(p, 1, 0);
	}

	/* the flash at the hit */
	FXW_Still(p, &self->to);
	p->sizeIni = 550.0f;
	p->sizeEnd = 2350.0f;
	p->sizeLife = 0.25f;
	p->lifeTime = g_ATPCTrailTime * 0.4f;
	p->birthRate = 0;
	FXW_SetSprite(p, 1);
	float r = FXW_Rand(6.2831854820251465, 0.0);
	p->rotVar = 0.0f;
	p->rotIni = r;
	p->rotEnd = r;
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXATPC_RAMP_GLOW));
	CGameParticles_EmitN(p, 1, 0);
	FXW_Still(p, &self->to);
	p->sizeIni = 550.0f;
	p->sizeEnd = 3250.0f;
	p->sizeLife = 0.2f;
	p->lifeTime = g_ATPCTrailTime * 0.5f;
	p->birthRate = 0;
	FXW_SetSprite(p, 5);
	r = FXW_Rand(6.2831854820251465, 0.0);
	p->rotIni = r;
	p->rotEnd = r;
	p->rotVar = FXW_Rand(0.7853981852531433, 0.0);
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXATPC_RAMP_RING));
	CGameParticles_EmitN(p, 1, 0);

	/* the sparks from the hit towards the shooter */
	vec3_t ctr, back;
	const vec3_t *cp = FX_Vtbl(self->shooter)->GetCenter(self->shooter, &ctr);
	FX_Normalize(&back, cp->x - self->to.x, cp->y - self->to.y, cp->z - self->to.z);
	CGameParticles_SetSparksOn(p, 1);
	for (int i = 0; i < 32; i++) {
		vec3_t rot, o;
		float a1 = FXW_Rand(1.5707963705062866, -0.7853981852531433);
		float a2 = FXW_Rand(1.5707963705062866, -0.7853981852531433);
		rot.x = FXW_Rand(1.5707963705062866, -0.7853981852531433);
		rot.y = a2;
		rot.z = a1;
		Vec3_Rotate(&o, &back, &rot);
		p->birthRate = 0;
		float sc = FXW_Rand(5.0, 10.0);
		p->initPosMin.y = o.y * sc + self->to.y;
		p->initPosMin.z = o.z * sc + self->to.z;
		p->initPosMin.x = self->to.x + o.x * sc;
		p->initPosMax = p->initPosMin;
		sc = FXW_Rand(3000.0, 2500.0);
		p->initVelMin.x = o.x * sc;
		p->initVelMin.y = o.y * sc;
		p->initVelMin.z = o.z * sc;
		p->initVelMax = p->initVelMin;
		sc = FXW_Rand(0.04999998211860657, 0.4000000059604645);
		p->initAccMin.x = -p->initVelMin.x * sc;
		p->initAccMin.y = -p->initVelMin.y * sc;
		p->initAccMin.z = -p->initVelMin.z * sc;
		p->initAccMax = p->initAccMin;
		p->lifeTime = FXW_Rand(0.10000002384185791, 0.44999998807907104) * g_ATPCTrailTime;
		sc = FXW_Rand(450.0, 150.0);
		p->sizeLife = 1.0f;
		FXW_NoRot(p);
		p->sizeIni = sc;
		p->sizeEnd = sc;
		byte spr = (byte)(crt_rand() % 3 + 0x41);
		p->spriteEnd = 0x44;
		p->sparkMaxSize = 1.0f;
		p->spriteIni = spr;
		p->sparkVelScale = 1.0f;
		CGameParticles_Apply(p);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXATPC_RAMP_GLOW + (uint32_t)(crt_rand() & 1) * 0x24));
		CGameParticles_EmitDt(p, 1, 0, FXW_Rand(0.30000001192092896, 0.0));
	}
	CGameParticles_SetSparksOn(p, 0);
}

void __thiscall cObjElementFXATPC_Start(cObjElementFXATPC *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXW_Node(model, "Dummy01"));
	CGameTrail_SetMaxTrails(&self->trail1c4, 5);
	CGameTrail_SetType(&self->trail1c4, 3);
	CGameTrail_SetTexture(&self->trail1c4, 0);
	CGameTrail_SetMaxTrails(&self->trail1c8, 0x10);
	CGameTrail_SetType(&self->trail1c8, 0);
	CGameTrail_SetTexture(&self->trail1c8, 1);
	self->n1d4 = 8;
	CGameTrail_SetMaxTrails(&self->trail1d8, 0x11);
	CGameTrail_SetType(&self->trail1d8, 3);
	CGameTrail_SetTexture(&self->trail1d8, 6);
	self->n1cc = 0xc;
	CGameTrail_SetMaxTrails(&self->trail1d0, 0x19);
	CGameTrail_SetType(&self->trail1d0, 3);
	CGameTrail_SetTexture(&self->trail1d0, 7);
	CGameTrail_SetMaxTrails(&self->trail1e0, (byte)(self->n1cc * 2 + 1));
	CGameTrail_SetType(&self->trail1e0, 3);
	CGameTrail_SetTexture(&self->trail1e0, 0x10);
}

void __thiscall cObjElementFXATPC_Reset(cObjElementFXATPC *self, cDebris *debris) {
	(void)debris;
	*(uint32_t *)self->slotOn = 0;
	CGameTrail_Clear(&self->trail1c4);
	CGameTrail_Clear(&self->trail1d8);
	CGameTrail_Clear(&self->trail1d0);
	CGameTrail_Clear(&self->trail1c8);
	CGameTrail_Clear(&self->trail1e0);
	self->time = 0.0f;
	self->angle = 0.0f;
}

/* ---- cObjElementFXDevastatorShoot ---- */

void __thiscall cObjElementFXDevastatorShoot_Update(cObjElementFXDevastatorShoot *self, cDebris *debris, float dt) {
	(void)debris;
	(void)dt;
	float t = self->time + FXW_Delta();
	self->time = t;
	if (!self->first && t > self->duration) {
		return;
	}
	int a = 0;
	if (!self->first) {
		a = (int)(t / self->duration * 255.0f);
		if (a > 0xff) {
			CGameTrail_SetTransp(&self->trail, 0xff);
			self->first = 0;
			return;
		}
		if (a < 0) {
			a = 0;
		}
	}
	CGameTrail_SetTransp(&self->trail, (byte)a);
	self->first = 0;
}

byte __thiscall cObjElementFXDevastatorShoot_IsAlive(cObjElementFXDevastatorShoot *self, cDebris *debris,
	cDebrisSys *sys) {
	(void)debris;
	(void)sys;
	if (!self->first && self->time >= self->duration) {
		CGameTrail_Clear(&self->trail);
		return 0;
	}
	return 1;
}

void __thiscall cObjElementFXDevastatorShoot_Start(cObjElementFXDevastatorShoot *self, cDebris *debris) {
	(void)debris;
	CGameTrail *t = &self->trail;
	CGameTrail_SetMaxTrails(t, 6);
	CGameTrail_SetTexture(t, 3);
	CGameTrail_SetFade(t, 0);
	CGameTrail_SetWidth(t, 200.0f);
	CGameTrail_SetPremul(t, 1);
}

void __thiscall cObjElementFXDevastatorShoot_Reset(cObjElementFXDevastatorShoot *self, cDebris *debris) {
	(void)debris;
	self->time = 0.0f;
	CGameTrail_Clear(&self->trail);
	CGameTrail_SetTransp(&self->trail, 0xff);
	self->first = 1;
}

/* ---- cObjElementFXInfernoExplosion ---- */


void __thiscall cObjElementFXInfernoExplosion_Update(cObjElementFXInfernoExplosion *self, cDebris *debris,
	float dt) {
	(void)debris;
	(void)dt;
	float d = FXW_Delta();
	float step = self->stepTime + d;
	float time = self->time + d;
	self->time = time;
	self->stepTime = step;
	if (0.01f > step) {
		return;
	}

	/* the rays fade out over the fade time before the end */
	float u = time - ((g_FXInfernoExplosionTime - 0.5f) - g_FXInfernoExplosionFadeTime);
	float c;
	if (0.0f > u) {
		c = 0.0f;
	} else {
		c = Float_MinSS(1.0f, u);
	}
	float s = (3.0f - (c + c)) * (c * c);
	int a = (int)((3.0f - (s + s)) * (s * s) * 255.0f);
	for (int i = 0; i < FXINFERNO_TRAILS; i++) {
		CGameTrail_Reset(&self->trails[i]);
		CGameTrail_SetTransp(&self->trails[i], (byte)a);
	}

	int inTrail = 0;
	int trail = 0;
	for (int i = 0; i < self->numRays; i++) {
		float len = self->rayLen[i];
		if (len > g_FXInfernoRayMinSize && trail < FXINFERNO_TRAILS) {
			const vec3_t *b = &self->rayBase[i];
			const vec3_t *dir = &self->rayDir[i];
			vec3_t end;
			end.x = b->x + dir->x * len;
			end.y = b->y + dir->y * len;
			end.z = b->z + dir->z * len;
			float w = len * g_FXInfernoRayRadius;
			CGameTrail_Add(&self->trails[trail], *b, w * 0.8f, 0x20f5aa19);
			CGameTrail_Add(&self->trails[trail], end, w, 0xffe12d0a);
			inTrail++;
			if (inTrail >= FXINFERNO_TRAILS) {
				trail++;
				inTrail = 0;
			}
		}
		float speed = self->raySpeed[i];
		self->rayLen[i] = self->stepTime * speed + self->rayLen[i];
		self->raySpeed[i] = self->rayAcc[i] * self->stepTime + speed;
		float max = self->rayMax[i];
		if (self->rayLen[i] > max && 3.4028235e38f > max) {
			self->rayLen[i] = max;
			self->raySpeed[i] = 0.0f;
			self->rayAcc[i] = 0.0f;
		}
	}

	if (g_SceneDuDvRTarget) {
		/* the core: grows, then fades out after 0.6 */
		float v = (self->time * 1.1f - 0.25f) / g_FXInfernoExplosionTime;
		if (v > 1.0f) {
			c = 1.0f;
		} else {
			c = Float_MaxSS(0.0f, v);
		}
		float q = c * 1.6666666f;
		s = q * q * (2.0f - q);
		a = (int)(s * s * (2.0f - s) * 255.0f);
		if (c > 0.6f) {
			q = (c - 0.6f) * 2.5f;
			s = q * q * (2.0f - q);
			a = 0xff - (int)(s * s * (s - 2.0f) * -255.0f);
		}
		float r = 1.0f - c;
		CGameParticles_SetParticle(&self->cores, 0, self->pos.x, self->pos.y, self->pos.z,
			r * r * (2.0f - r) * (g_FXInfernoCoreSize * 60.0f), FXW_Gray(a), 1, FX_PI);

		/* the flash */
		v = 1.0f - self->time / g_FXInfernoExplosionTime;
		if (v > 1.0f) {
			c = 1.0f;
		} else {
			c = Float_MaxSS(0.0f, v);
		}
		r = 1.0f - c;
		s = c * c * (2.0f - c);
		a = (int)((3.0f - (s + s)) * (s * s) * 255.0f);
		float size = (3.0f - (r + r)) * (r * r) * (g_FXInfernoCoreSize * 80.0f) * 1.5f;
		CGameParticles_SetParticle(&self->cores, 1, self->pos.x, self->pos.y, self->pos.z, size, FXW_Gray(a), 1,
			0.0f);
	}
	self->stepTime = 0.0f;
}

void __thiscall cObjElementFXInfernoExplosion_Start(cObjElementFXInfernoExplosion *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXW_Node(model, "Dummy01"));
	CGameParticles_Attach(&self->cores, model, FXW_Node(model, "Dummy02(+dudv_pass)"));
	for (int i = 0; i < FXINFERNO_TRAILS; i++) {
		CGameTrail_SetMaxTrails(&self->trails[i], 0x1f);
		CGameTrail_SetTexture(&self->trails[i], 0);
		CGameTrail_SetType(&self->trails[i], 3);
	}
	self->numRays = 0;
}

void __thiscall cObjElementFXInfernoExplosion_Reset(cObjElementFXInfernoExplosion *self, cDebris *debris) {
	(void)debris;
	self->time = 0.0f;
	self->stepTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
	CGameParticles_KillAll(&self->cores);
	for (int i = 0; i < FXINFERNO_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	self->numRays = 0;
}

/* a random direction: the unit vector (1, 1, 1) turned by three random angles (0..2 pi, the first rand is z) */
static void FXW_RandDir(vec3_t *out) {
	static const vec3_t one = { 1.0f, 1.0f, 1.0f };
	vec3_t rot;

	rot.z = FXW_Rand((double)M_2PI_F, 0.0);
	rot.y = FXW_Rand((double)M_2PI_F, 0.0);
	rot.x = FXW_Rand((double)M_2PI_F, 0.0);
	Vec3_Rotate(out, &one, &rot);
}

/* the rays (up to the floor within range) and the core particles of an explosion at x y z; bees widens it */
void __thiscall cObjElementFXInfernoExplosion_Explode(cObjElementFXInfernoExplosion *self, float x, float y, float z,
	float range, int bees, byte flag) {
	static const vec3_t one = { 1.0f, 1.0f, 1.0f };
	CGameParticles *p = &self->particles;
	float k, ax, ay;
	int i, c;

	self->pos.x = x;
	self->pos.y = y;
	self->pos.z = z;
	self->bees = bees;
	self->flag = flag;
	self->range = range;
	k = ((float)bees * g_InfernoRangeBay + g_InfernoRangeMin) / g_InfernoRangeMin;
	self->numRays = 0;
	k = k * 0.3f;
	for (i = 0; i < FXINFERNO_RAYS; i++) {
		vec3_t dir, base, to, hit;
		float lo, hi, t;
		int n;

		if (self->numRays >= FXINFERNO_RAYS) {
			break;
		}
		FXW_RandDir(&dir);
		FXW_RandDir(&base);
		hi = g_FXInfernoCoreSize * 0.25f;
		lo = g_FXInfernoCoreSize * 0.05f;
		t = FXW_Rand((double)(hi - lo), (double)lo);
		n = self->numRays;
		self->rayBase[n].x = base.x * t * k + self->pos.x;
		self->rayBase[n].y = self->pos.y + base.y * t * k;
		self->rayBase[n].z = self->pos.z + base.z * t * k;
		self->rayDir[n] = dir;
		self->rayLen[n] = FXW_Rand((double)(g_FXInfernoRayInitSizeMax - g_FXInfernoRayInitSizeMin),
			(double)g_FXInfernoRayInitSizeMin);
		self->raySpeed[n] = FXW_Rand((double)(g_FXInfernoRaySpeedMax - g_FXInfernoRaySpeedMin),
			(double)g_FXInfernoRaySpeedMin) * k;
		self->rayAcc[n] = -(FXW_Rand((double)(g_FXInfernoRayAccMax - g_FXInfernoRayAccMin),
			(double)g_FXInfernoRayAccMin) * k);
		self->rayMax[n] = 3.4028235e38f;
		to.x = self->range * self->rayDir[n].x + self->rayBase[n].x;
		to.y = self->rayBase[n].y + self->rayDir[n].y * self->range;
		to.z = self->rayBase[n].z + self->rayDir[n].z * self->range;
		if (Engine_TestLine(&self->rayBase[n], &to, &hit, NULL)) {
			float dy = hit.y - self->rayBase[n].y, dx = hit.x - self->rayBase[n].x, dz = hit.z - self->rayBase[n].z;

			self->rayMax[n] = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			self->rayMax[n] = self->rayMax[n] - self->rayMax[n] * g_FXInfernoRayRadius;
		}
		if (self->rayMax[n] >= g_FXInfernoRayMinSize) {
			self->numRays++;
		}
	}
	/* three big particles around the center */
	p->birthRate = 0;
	for (c = 3; c; c--) {
		vec3_t dir;
		float b1, b2, b3, lo, hi, t;

		FXW_RandDir(&dir);
		b1 = FXW_Rand(30.0, -15.0);
		b2 = FXW_Rand(30.0, -15.0);
		b3 = FXW_Rand(30.0, -15.0) * k;
		b2 = b2 * k;
		b1 = b1 * k;
		hi = g_FXInfernoCoreSize * 0.5f;
		lo = g_FXInfernoCoreSize * 0.1f;
		t = FXW_Rand((double)(hi - lo), (double)lo);
		p->initPosMin.x = dir.x * t + self->pos.x + b3;
		p->initPosMin.y = dir.y * t + self->pos.y + b2;
		p->initPosMin.z = dir.z * t + self->pos.z + b1;
		p->initPosMax = p->initPosMin;
		p->initVelMin.x = 0.0f;
		p->initVelMin.y = 0.0f;
		p->initVelMin.z = 0.0f;
		p->initVelMax = p->initVelMin;
		p->initAccMin.x = 0.0f;
		p->initAccMin.y = 0.0f;
		p->initAccMin.z = 0.0f;
		p->initAccMax = p->initAccMin;
		p->rotIni = 0.0f;
		p->rotEnd = 0.0f;
		p->rotVar = 0.0f;
		t = FXW_Rand(0x1.999998p-4, 0x1.99999ap-2) * g_FXInfernoExplosionTime;
		p->sizeLife = 0.25f;
		p->spriteIni = 2;
		p->spriteEnd = 2;
		p->lifeTime = t;
		p->sizeIni = FXW_Rand(0x1.99999p-4, 0x1.99999ap-1) * g_FXInfernoCoreSize * k;
		p->sizeEnd = FXW_Rand(0.5, 7.5) * g_FXInfernoCoreSize * k;
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, g_FXInfernoRamp);
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);
	}
	/* a shell of particles every 45 degrees, moving out */
	p->birthRate = 0;
	p->rotIni = 0.0f;
	p->rotEnd = 0.0f;
	p->rotVar = 0.0f;
	{
		float t = FXW_Rand(0x1.99999p-4, 0x1.333334p-1) * g_FXInfernoExplosionTime;

		p->sizeLife = 0.2f;
		p->spriteIni = 2;
		p->spriteEnd = 2;
		p->lifeTime = t;
	}
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, g_FXInfernoRamp);
	for (ax = 0.0f; M_2PI_D > (double)ax; ax = ax + 0.7853982f) {
		for (ay = 0.0f; M_2PI_D > (double)ay; ay = ay + 0.7853982f) {
			vec3_t rot = { ax, ay, 0.0f }, d;
			float b1, b2, b3;

			Vec3_Rotate(&d, &one, &rot);
			b1 = FXW_Rand(30.0, -15.0);
			b2 = FXW_Rand(30.0, -15.0);
			b3 = FXW_Rand(30.0, -15.0) * k;
			b2 = b2 * k;
			b1 = b1 * k;
			p->initPosMin.y = self->pos.y + d.y * g_FXInfernoCoreSize * 0.5f + b2;
			p->initPosMin.x = d.x * g_FXInfernoCoreSize * 0.5f + self->pos.x + b3;
			p->initPosMin.z = self->pos.z + d.z * g_FXInfernoCoreSize * 0.5f + b1;
			p->initPosMax = p->initPosMin;
			p->initVelMin.x = d.x * g_FXInfernoCoreSize * 0.5f * k;
			p->initVelMin.y = d.y * g_FXInfernoCoreSize * 0.5f * k;
			p->initVelMin.z = d.z * g_FXInfernoCoreSize * 0.5f * k;
			p->initVelMax = p->initVelMin;
			p->initAccMin.x = 0.0f;
			p->initAccMin.y = 0.0f;
			p->initAccMin.z = 0.0f;
			p->initAccMax = p->initAccMin;
			p->sizeIni = FXW_Rand(0x1.99999p-4, 0x1.99999ap-1) * g_FXInfernoCoreSize * k;
			p->sizeEnd = FXW_Rand(0.5, 2.5) * g_FXInfernoCoreSize * k;
			CGameParticles_Apply(p);
			CGameParticles_EmitDt(p, 1, 1, 0.05f);
		}
	}
}

/* ---- cObjElementFXInfernoMissile ---- */

void __thiscall cObjElementFXInfernoMissile_Update(cObjElementFXInfernoMissile *self, cDebris *debris, float dt) {
	cDebris_Move(debris, dt);
	float d = FXW_Delta();
	float time = self->time + d;
	self->time = time;
	float fs = self->fadeStart;
	if (fs > 0.0f) {
		/* the trail fades out */
		int a = (int)((time - fs) / self->fadeTime * 255.0f);
		if (a > 0xff) {
			a = 0xff;
		}
		CGameTrail_SetTransp(&self->trail, (byte)a);
		if (self->flareOn) {
			CGameLensFlare_SetIntensity(&self->flare,
				(1.0f - (self->time - self->fadeStart) / self->fadeTime) * 0.2f);
			CGameLensFlare_SetOn(&self->flare, 1);
		}
		time = self->time;
		if (time - self->lastShrink > 0.1f) {
			self->lastShrink = time;
			CGameTrail_Shrink(&self->trail);
		}
		return;
	}

	CGameModel *model = Debris_Model(debris);
	vec3_t pos, p;
	FXW_Front(model, &p, &pos);
	if (self->flareOn) {
		CGameLensFlare_SetPos(&self->flare, pos);
		CGameLensFlare_SetIntensity(&self->flare, 0.2f);
		CGameLensFlare_SetOn(&self->flare, 1);
	}
	float dy = p.y - self->last.y;
	float dx = p.x - self->last.x;
	float dz = p.z - self->last.z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (!(dist > self->minDist) && CGameTrail_IsRender(&self->trail)) {
		CGameTrail_SetLastPosColor(&self->trail, p, 0x80ffdc5a);
		return;
	}
	self->last = p;
	CGameTrail_Add(&self->trail, p, FXW_Rand(10.0, 160.0), 0x80ffdc5a);
}

byte __thiscall cObjElementFXInfernoMissile_IsAlive(cObjElementFXInfernoMissile *self, cDebris *debris,
	cDebrisSys *sys) {
	return FXW_MissileAlive(debris, sys, &self->time, &self->fadeStart, &self->lastShrink, &self->fadeTime);
}

void __thiscall cObjElementFXInfernoMissile_Start(cObjElementFXInfernoMissile *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXW_Node(model, "Dummy01"));
	CHECK(self->particles.SistPart);
	CGameParticles_SetParticle(&self->particles, 0, 0.0f, 0.0f, 0.0f, 6000.0f, 0xc0ff2805, 6, 0.0f);
	CGameParticles_SetParticle(&self->particles, 1, 0.0f, 0.0f, 0.0f, 2650.0f, 0x40ffbe32, 5, 0.0f);
	CGameParticles_SetParticle(&self->particles, 2, 0.0f, 0.0f, 0.0f, 450.0f, 0x10fff050, 1, 0.0f);
	CGameParticles_SetParticle(&self->particles, 3, 0.0f, 0.0f, 0.0f, 250.0f, 0x08fff580, 1, 0.0f);
	CGameTrail *t = &self->trail;
	CGameTrail_SetMaxTrails(t, 0x1e);
	CGameTrail_SetFade(t, 1);
	CGameTrail_SetSmooth(t, 1);
	CGameTrail_SetTexture(t, 0);
	CGameTrail_SetColor(t, 0x3cfafafa);
	CGameTrail_SetWidth(t, 65.0f);
	CGameTrail_SetScale(t, 1.0f, 8.0f);
	CGameTrail_SetType(t, 2);
	CGameTrail_SetPremul(t, 1);
	self->minDist = 700.0f;
	self->fadeTime = 0.75f;
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->lastShrink = 0.0f;
	self->flareOn = 0;
	CGameLensFlare_SetF30(&self->flare, 0.2f);
	CGameLensFlare_SetI1c(&self->flare, 2);
}

void __thiscall cObjElementFXInfernoMissile_Reset(cObjElementFXInfernoMissile *self, cDebris *debris) {
	(void)debris;
	CGameTrail_Clear(&self->trail);
	CGameTrail_SetTransp(&self->trail, 0);
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->lastShrink = 0.0f;
	self->flareOn = 0;
	CGameLensFlare_SetOn(&self->flare, 0);
}

void __thiscall cObjElementFXInfernoMissile_SetFlare(cObjElementFXInfernoMissile *self, byte on) {
	self->flareOn = on;
}

/* ---- cObjElementFXLaser ---- */

void __thiscall cObjElementFXLaser_Update(cObjElementFXLaser *self, cDebris *debris, float dt) {
	(void)dt;
	CGameModel *model = Debris_Model(debris);
	vec3_t *pos = &self->pos;
	float d = FXW_Delta();
	self->time = self->time + d;
	CGameModel_SetPos(model, pos);
	float dy = pos->y - self->start.y;
	float dx = pos->x - self->start.x;
	float dz = pos->z - self->start.z;
	self->dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	byte vis = CGameModel_IsVisible(model);
	if (!vis) {
		if (self->drawn) {
			CGameTrail_Clear(&self->trail);
			CGameParticles_KillAll(&self->particles);
			self->drawn = 0;
		}
		float vx = self->vel.x * d;
		float vy = self->vel.y * d;
		float vz = self->vel.z * d;
		pos->x = pos->x + vx;
		pos->y = pos->y + vy;
		pos->z = pos->z + vz;
		return;
	}
	if (self->dist > self->len) {
		return;
	}

	dy = pos->y - self->end.y;
	dx = pos->x - self->end.x;
	dz = pos->z - self->end.z;
	float rest = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	self->rest = rest;
	/* the beam is g_FXLaserBeamSize long, shorter (and fading) at the end */
	float size = g_FXLaserBeamSize;
	float len;
	int a;
	if (size > rest) {
		len = rest;
		a = (int)(255.0f - rest / size * 255.0f);
	} else {
		len = size;
		if (size > self->dist) {
			a = (int)(255.0f - size / size * 255.0f);
		} else {
			a = 0;
		}
	}
	CGameTrail_SetTransp(&self->trail, (byte)a);
	vec3_t tip;
	float tx = len * self->dir.x;
	float ty = self->dir.y * len;
	float tz = self->dir.z * len;
	tip.x = pos->x + tx;
	tip.y = pos->y + ty;
	tip.z = pos->z + tz;
	CGameTrail_Reset(&self->trail);
	CGameTrail_Add(&self->trail, *pos, g_FXLaserBeamHeight, 0x40ffc814);
	CGameTrail_Add(&self->trail, tip, g_FXLaserBeamHeight, 0x40ffc814);
	int n = (int)(len / (g_FXLaserBeamSize / (float)CGameParticles_NumParticles(&self->particles)));
	float step = len / (float)(n - 1);
	CGameParticles_KillRange(&self->particles, n, CGameParticles_NumParticles(&self->particles));
	for (int i = 0; i < n; i++) {
		float f = (float)i;
		float px = self->dir.x * step * f;
		float py = self->dir.y * step * f;
		float pz = self->dir.z * step * f;
		CGameParticles_SetParticle(&self->particles, i, pos->x + px, py + pos->y, pz + pos->z,
			g_FXLaserBeamHeight * 2.25f, 0x5cff5514, 0, 0.0f);
	}
	float vx = self->vel.x * d;
	float vy = self->vel.y * d;
	float vz = self->vel.z * d;
	pos->x = vx + pos->x;
	pos->y = pos->y + vy;
	pos->z = pos->z + vz;
	self->drawn = vis;
}

byte __thiscall cObjElementFXLaser_IsAlive(cObjElementFXLaser *self, cDebris *debris, cDebrisSys *sys) {
	(void)debris;
	(void)sys;
	return !(self->dist > self->len);
}

void __thiscall cObjElementFXLaser_Start(cObjElementFXLaser *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	if (CGameModel_IsLoaded(model)) {
		int node = CGameModel_FindNodoR(model, "Dummy01");
		if (node != -1) {
			CGameParticles_Attach(&self->particles, model, node);
		}
	}
	CHECK(self->particles.SistPart);
	CGameTrail_SetMaxTrails(&self->trail, 3);
	CGameTrail_SetTexture(&self->trail, 0);
	CGameTrail_SetType(&self->trail, 2);
	self->time = 0.0f;
	self->drawn = 0;
}

void __thiscall cObjElementFXLaser_Reset(cObjElementFXLaser *self, cDebris *debris) {
	(void)debris;
	CGameTrail_Clear(&self->trail);
	CGameTrail_SetTransp(&self->trail, 0);
	CGameParticles_KillAll(&self->particles);
	self->time = 0.0f;
	self->drawn = 0;
}

void __thiscall cObjElementFXLaser_Shoot(cObjElementFXLaser *self, float x0, float y0, float z0, float x1, float y1,
	float z1, float time) {
	self->start.x = x0;
	self->start.y = y0;
	self->start.z = z0;
	self->end.x = x1;
	self->end.y = y1;
	self->end.z = z1;
	self->duration = time;
	float dx = self->end.x - self->start.x;
	float dy = self->end.y - self->start.y;
	float dz = self->end.z - self->start.z;
	self->delta.x = dx;
	self->delta.y = dy;
	self->delta.z = dz;
	self->len = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
	dx = self->delta.x;
	dy = self->delta.y;
	dz = self->delta.z;
	float l = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
	vec3_t dir;
	if (l == 0.0f) {
		dir = self->delta;
	} else {
		float inv = 1.0f / l;
		dir.x = self->delta.x * inv;
		dir.y = self->delta.y * inv;
		dir.z = self->delta.z * inv;
	}
	float invt = 1.0f / time;
	self->dir = dir;
	self->vel.x = self->delta.x * invt;
	self->vel.z = self->delta.z * invt;
	self->vel.y = self->delta.y * invt;
	self->pos = self->start;
	dx = self->pos.x - self->end.x;
	dy = self->pos.y - self->end.y;
	dz = self->pos.z - self->end.z;
	self->dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

/* ---- cObjElementFXSwarmMissile ---- */

void __thiscall cObjElementFXSwarmMissile_Update(cObjElementFXSwarmMissile *self, cDebris *debris, float dt) {
	cDebris_Move(debris, dt);
	if (self->node == -1) {
		return;
	}
	self->time = self->time + FXW_Delta();
	vec3_t pos, p;
	FXW_Front(Debris_Model(debris), &p, &pos);
	float fs = self->fadeStart;
	if (fs > 0.0f) {
		int a = (int)((self->time - fs) / self->fadeTime * 255.0f);
		if (a > 0xff) {
			a = 0xff;
		}
		CGameTrail_SetTransp(&self->trail, (byte)a);
		CGameTrail_SetLastPos(&self->trail, p);
		float time = self->time;
		if (time - self->lastShrink > 0.1f) {
			self->lastShrink = time;
			CGameTrail_Shrink(&self->trail);
		}
		return;
	}

	/* the color goes from orange to white with the time */
	float s = self->time * 255.0f;
	int r = (int)(s + 255.0f);
	if (r > 0xff) {
		r = 0xff;
	}
	int g = (int)(s + 200.0f);
	int b = (int)(s + 90.0f);
	if (g > 0xff) {
		g = 0xff;
	}
	if (b > 0xff) {
		b = 0xff;
	}
	uint32_t color = (uint32_t)((r | 0xc000) >> 2);
	color = color << 8 | (uint32_t)(g >> 2);
	color = color << 8 | (uint32_t)(b >> 2);
	float dx = p.x - self->last.x;
	float dy = p.y - self->last.y;
	float dz = p.z - self->last.z;
	float dist2 = dy * dy + dx * dx + dz * dz;
	if (!(dist2 > self->minDist2) && CGameTrail_IsRender(&self->trail)) {
		CGameTrail_SetLastPosColor(&self->trail, p, color);
		return;
	}
	self->last = p;
	CGameTrail_Add(&self->trail, p, FXW_Rand(10.0, 70.0), color);
}

byte __thiscall cObjElementFXSwarmMissile_IsAlive(cObjElementFXSwarmMissile *self, cDebris *debris, cDebrisSys *sys) {
	return FXW_MissileAlive(debris, sys, &self->time, &self->fadeStart, &self->lastShrink, &self->fadeTime);
}

void __thiscall cObjElementFXSwarmMissile_Start(cObjElementFXSwarmMissile *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	self->node = -1;
	if (CGameModel_IsLoaded(model)) {
		self->node = CGameModel_FindNodoR(model, "DF_MissileMotor");
	}
	if (self->node != -1) {
		CGameParticles_Attach(p, model, self->node);
	}
	CHECK(p->SistPart);
	CGameParticles_SetParticle(p, 0, 0.0f, 25.0f, 0.0f, FXW_Rand(250.0, 725.0), 0x80ff5514, 5, 0.0f);
	CGameParticles_SetParticle(p, 1, 0.0f, 40.0f, 0.0f, FXW_Rand(25.0, 150.0), 0x50ffc33c, 2, 0.0f);
	CGameParticles_SetParticle(p, 2, 0.0f, 120.0f, 0.0f, FXW_Rand(25.0, 125.0), 0x6effa528, 2, 0.0f);
	CGameParticles_SetParticle(p, 3, 0.0f, 190.0f, 0.0f, FXW_Rand(20.0, 92.0), 0x8cff7d14, 2, 0.0f);
	CGameTrail *t = &self->trail;
	CGameTrail_SetMaxTrails(t, 0x1e);
	CGameTrail_SetFade(t, 1);
	CGameTrail_SetSmooth(t, 1);
	CGameTrail_SetTexture(t, 2);
	CGameTrail_SetColor(t, 0x3cfafafa);
	CGameTrail_SetWidth(t, 65.0f);
	CGameTrail_SetScale(t, 1.0f, 8.0f);
	self->minDist2 = 490000.0f;
	self->fadeTime = 0.6f;
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->lastShrink = 0.0f;
}

void __thiscall cObjElementFXSwarmMissile_Reset(cObjElementFXSwarmMissile *self, cDebris *debris) {
	(void)debris;
	CGameTrail_Clear(&self->trail);
	CGameTrail_SetTransp(&self->trail, 0);
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->lastShrink = 0.0f;
}

/* ---- cObjElementFXTesla ---- */

#define g_TeslaRange GAME_DATA(float, 0x44a7d0)
#define g_FXTeslaStepTime GAME_DATA(float, 0x44a760)
#define g_FXTeslaRandRadius GAME_DATA(float, 0x44a840)
#define FXTESLA_RAMP_SPARK 0x431930		/* [2] */
#define Entity_TeslaAng(e) ((float *)((byte *)(e) + 0x6dc))	/* the aim angles x and y */
#define Entity_HitInfo(e) ((cVehicleAttack *)((byte *)(e) + 0x298))
#define Entity_F3a0(e) (*(float *)((byte *)(e) + 0x3a0))

/* the end moved back from the target towards the emitter by 0.2..0.5 of r along n */
static void FXW_TeslaBack(vec3_t *tpos, const vec3_t *n, float r) {
	float s = FXW_Rand(0.30000001192092896, 0.20000000298023224);
	tpos->x = s * (n->x * r) + tpos->x;
	tpos->y = s * (n->y * r) + tpos->y;
	tpos->z = s * (n->z * r) + tpos->z;
}

/* the ray of each mounted slot from its weapon node to the target (or the aim point), drawn as four trails through
 * the points that move to new random offsets every FXTeslaStepTime, the glow at both ends */
void __thiscall cObjElementFXTesla_Update(cObjElementFXTesla *self, cDebris *debris, float dt) {
	(void)dt;
	cEntity *e = self->emitter;
	if (e && (Entity_Removed(e) || !e->flag_4c)) {
		cWeaponManager_SetFlag46(Vehicle_WeapMgr(e), 0);
		self->emitter = NULL;
	}
	if (!self->emitter) {
		return;
	}
	CGameModel *em = FX_Vtbl(self->emitter)->GetModel(self->emitter);
	CGameModel *dm = Debris_Model(debris);
	vec3_t tmp, tmp2;
	CHECK(CGameModel_IsLoaded(em));
	float d = FXW_Delta();
	e = self->emitter;
	self->time = d + self->time;
	if (!Entity_B636(e)) {
		self->shooting = 0;
	}
	CGameModel_SetSortZ(dm, -33.0f, 0.0f);
	vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
	CGameModel_SetPos(dm, &cam);
	int alpha = 0;
	if (self->fadeStart >= 0.0f) {
		int a = (int)((self->time - self->fadeStart) / g_FXTeslaFadeOutTime * 255.0f);
		alpha = a >= 0 ? a : 0;
		if (alpha > 0xff) {
			alpha = 0xff;
		}
	}

	/* the target of the hit info of the emitter */
	if (*cHitInfo_GetName(Entity_HitInfo(self->emitter))) {
		if (strcmp(self->name, cHitInfo_GetName(Entity_HitInfo(self->emitter))) != 0) {
			Str_Copy(self->name, cHitInfo_GetName(Entity_HitInfo(self->emitter)), sizeof(self->name));
			HashNode *node = NULL;
			if (self->name[0]) {
				HashTable *ht = &g_pWorld->entities;
				node = HashTable_Find(ht, self->name, HashTable_Hash(ht, self->name));
			}
			self->target = node ? node->value : NULL;
			self->followTime = g_FXTeslaFollowTime * 5.0f;
		}
	} else if (self->target) {
		self->target = NULL;
		self->name[0] = 0;
		self->followTime = g_FXTeslaFollowTime * 5.0f;
	}
	cEntity *t = self->target;
	if (t && (Entity_Removed(t) || !t->flag_4c)) {
		self->target = NULL;
	}

	/* the end of the ray: before the target, else the aim of the emitter */
	vec3_t *tp = &self->tpos;
	if (self->target) {
		*tp = *FX_Vtbl(self->target)->GetCenter(self->target, &tmp);
		float r = Entity_F3a0(self->target);
		const vec3_t *ec = FX_Vtbl(self->emitter)->GetCenter(self->emitter, &tmp);
		vec3_t n;
		FX_Normalize(&n, ec->x - tp->x, ec->y - tp->y, ec->z - tp->z);
		FXW_TeslaBack(tp, &n, r);
	} else {
		vec3_t ang;
		cEntity *hit = NULL;
		e = self->emitter;
		ang.x = Entity_TeslaAng(e)[0];
		ang.y = Entity_TeslaAng(e)[1];
		ang.z = 0.0f;
		float range = g_TeslaRange;
		e->vftable->GetTargetAngle(e, &ang.x, range, &hit);
		if (hit && hit->vftable->IsType(hit, TYEN_CAR)) {
			*tp = *FX_Vtbl(hit)->GetCenter(hit, &tmp);
			float r = Entity_F3a0(hit);
			const vec3_t *ec = FX_Vtbl(self->emitter)->GetCenter(self->emitter, &tmp);
			vec3_t dv, n;
			dv.x = ec->x - tp->x;
			dv.y = ec->y - tp->y;
			dv.z = ec->z - tp->z;
			Vec3_NormalizeTo(&n, &dv);
			float s = FXW_Rand(0.30000001192092896, 0.20000000298023224);
			tp->x = s * (n.x * r) + tp->x;
			tp->y = tp->y + s * (n.y * r);
			tp->z = tp->z + s * (n.z * r);
		} else {
			e = self->emitter;
			e->vftable->AimWeapon(e, ang.x, ang.y, &range, tp, &hit, 8);
		}
		float s = FXW_Rand(25.0, 0.0);
		vec3_t rd;
		const vec3_t *r = FX_RandomDir(&rd);
		tp->x = tp->x + s * r->x;
		tp->y = tp->y + r->y * s;
		tp->z = tp->z + r->z * s;
	}
	Float_Approach(&self->followTime, g_FXTeslaFollowTime, d * 0.25f);

	for (int i = 0, base = 2; i < FXTESLA_SLOTS; i++, base += 6) {
		if (!self->slotOn[i]) {
			continue;
		}
		CGameTrail *tr = &self->trails[i * FXTESLA_TRAILS];
		vec3_t *offs = self->offs[i];
		vec3_t *pts = self->pts[i];
		vec3_t P[GAMETRAIL_MAX_BEZIER + 2], coefs[GAMETRAIL_MAX_BEZIER], X[2], node, v, o;
		matrix_t mat;

		/* the line from the weapon node of the slot to the end */
		const matrix_t *m = CGameModel_NodoMatrix(em, &mat,
			(uint32_t)Entity_WeapNodes(self->emitter)[i + g_TeslaIDAmmo * 4], 0, NULL, 0);
		v.x = 0.0f;
		v.y = 0.0f;
		v.z = -500.0f;
		node.x = m->m[12];
		node.y = m->m[13];
		node.z = m->m[14];
		P[1] = node;
		const vec3_t *r = Vec3_Rotate(&o, &v, FX_Vtbl(self->emitter)->GetAngles(self->emitter, &tmp2));
		int n = self->numPts;
		P[2].z = r->z + node.z;
		P[2].x = node.x + r->x;
		P[2].y = r->y + node.y;
		float inv = 1.0f / (float)(n - 1);
		P[n] = *tp;
		float sx = inv * (P[n].x - P[1].x);
		float sy = inv * (P[n].y - P[1].y);
		float sz = inv * (P[n].z - P[1].z);
		for (int k = 1; k < n - 1; k++) {
			float fk = (float)k;
			P[k + 1].y = P[1].y + fk * sy;
			P[k + 1].x = P[1].x + fk * sx;
			P[k + 1].z = P[1].z + fk * sz;
		}

		/* new random offsets every step */
		float st = self->stepTime[i] + d;
		self->stepTime[i] = st;
		if (self->first || st >= g_FXTeslaStepTime) {
			self->stepTime[i] = 0.0f;
			for (int k = 0; k < self->numPts; k++) {
				offs[k + FXTESLA_PTS] = offs[k];
			}
			offs[0].x = 0.0f;
			offs[0].y = 0.0f;
			offs[0].z = 0.0f;
			vec3_t *last = &offs[self->numPts - 1];
			last->x = 0.0f;
			last->y = 0.0f;
			last->z = 0.0f;
			for (int k = 1; k < self->numPts - 1; k++) {
				float amp = g_FXTeslaRandRadius;
				float s = (float)((double)(crt_rand() & 0x7fff) * g_RandScale * (double)amp + 0.0);
				vec3_t rd;
				const vec3_t *rr = FX_RandomDir(&rd);
				offs[k].x = rr->x * s;
				offs[k].y = rr->y * s;
				offs[k].z = rr->z * s;
			}
			if (self->first) {
				for (int k = 0; k < self->numPts; k++) {
					pts[k].z = P[k + 1].z + offs[k].z;
					pts[k].x = offs[k].x + P[k + 1].x;
					pts[k].y = offs[k].y + P[k + 1].y;
				}
				for (int k = 0; k < self->numPts; k++) {
					offs[k + FXTESLA_PTS] = offs[k];
				}
			} else if (0.0f > self->fadeStart) {
				/* sparks at the end */
				CGameParticles *p = &self->particles;
				for (int k = 0; k < 4; k++) {
					vec3_t rd;
					FX_RandomDir(&rd);
					CGameParticles_SetSparksOn(p, 1);
					p->birthRate = 0;
					float s = FXW_Rand(5.0, 10.0);
					p->initPosMin.y = tp->y + s * rd.y;
					p->initPosMin.x = s * rd.x + tp->x;
					p->initPosMin.z = tp->z + s * rd.z;
					p->initPosMax = p->initPosMin;
					s = FXW_Rand(1000.0, 2500.0);
					p->initVelMin.x = s * rd.x;
					p->initVelMin.y = s * rd.y;
					p->initVelMin.z = s * rd.z;
					p->initVelMax = p->initVelMin;
					p->initAccMin.x = 0.0f;
					p->initAccMin.y = -9800.0f;
					p->initAccMin.z = 0.0f;
					p->initAccMax = p->initAccMin;
					p->lifeTime = FXW_Rand(0.06000000238418579, 0.2199999988079071);
					s = FXW_Rand(100.0, 200.0);
					FXW_NoRot(p);
					p->spriteIni = 0x41;
					p->spriteEnd = 0x45;
					p->sparkMaxSize = 1.1f;
					p->sizeIni = s;
					p->sizeEnd = s;
					CGameParticles_Apply(p);
					CGameParticles_SetColorRampOn(p, 1);
					CGameParticles_SetColorRamp(p,
						(uint32_t *)Game_Ptr(FXTESLA_RAMP_SPARK + (uint32_t)(crt_rand() % 2) * 0x24));
					CGameParticles_EmitDt(p, 1, 0, FXW_Rand(0.05000000074505806, 0.0));
					CGameParticles_SetSparksOn(p, 0);
				}
			}
		}

		/* the points follow their offsets */
		e = self->emitter;
		if (e && e->flag_4c) {
			pts[0] = P[1];
			pts[self->numPts - 1] = P[self->numPts];
			float k1 = Float_MinSS(1.0f, 1.0f / g_FXTeslaFollowTime * d);
			float k2 = self->stepTime[i] / g_FXTeslaStepTime;
			float u = d * 10.0f + self->uvOfs[i];
			self->uvOfs[i] = u;
			if (u > 1.0f) {
				if (u >= 2.0f) {
					self->uvOfs[i] = 0.0f;
				} else {
					self->uvOfs[i] = u - 1.0f;
				}
			}
			CGameTrail_SetUV(&tr[1], 1.0f - self->uvOfs[i], 0.0f);
			float w = 1.0f - k2;
			for (int k = 1; k < self->numPts - 1; k++) {
				vec3_t *c = &pts[k];
				const vec3_t *q = &P[k + 1];
				float d1x = c->x - q->x;
				float d1y = c->y - q->y;
				float d1z = c->z - q->z;
				float ax = offs[k].x - d1x;
				float ay = offs[k].y - d1y;
				float az = offs[k].z - d1z;
				float bx = offs[k + FXTESLA_PTS].x - d1x;
				float by = offs[k + FXTESLA_PTS].y - d1y;
				float bz = offs[k + FXTESLA_PTS].z - d1z;
				c->x = c->x + k2 * ax * k1;
				c->y = c->y + k2 * ay * k1;
				c->z = c->z + k2 * az * k1;
				c->x = c->x + bx * w * k1;
				c->y = c->y + by * w * k1;
				c->z = c->z + bz * w * k1;
			}
		}

		/* the trails along the curve, the more segments the longer */
		float ddx = pts[0].x - pts[self->numPts - 1].x;
		float ddy = pts[0].y - pts[self->numPts - 1].y;
		float ddz = pts[0].z - pts[self->numPts - 1].z;
		int mx = CGameTrail_GetMaxTrails(tr);
		float len = (float)__builtin_sqrt((double)(ddy * ddy + ddx * ddx + ddz * ddz));
		int segs = (int)((float)mx * len / g_TeslaRange);
		if (segs < 4) {
			segs = 4;
		} else if (segs > CGameTrail_GetMaxTrails(tr)) {
			segs = CGameTrail_GetMaxTrails(tr);
		}
		for (int j = 0; j < FXTESLA_TRAILS; j++) {
			CGameTrail_SetSegments(&tr[j], (byte)(segs - 1));
			CGameTrail_SetTransp(&tr[j], (byte)alpha);
		}
		CGameTrail_BezierCoefs(tr, self->numPts, pts, coefs);
		uint32_t gb = self->target ? 0x40 : 0;
		if (segs - 1 > 0) {
			float last = (float)(segs - 2);
			uint32_t ring = ((gb >> 2) + 0x20) << 16 | (0x60 - gb) | 0x28002000;
			uint32_t c255 = 0xff - gb;
			vec3_t prev;
			for (int j = 0; j < segs - 1; j++) {
				vec3_t q;
				CGameTrail_BezierPoint(tr, self->numPts, coefs, &q, (float)j / last);
				if (j == 0) {
					prev = q;
				}
				float px = prev.x - q.x;
				float py = prev.y - q.y;
				float pz = prev.z - q.z;
				float dist = (float)__builtin_sqrt((double)(py * py + px * px + pz * pz));
				int inner = j > 0 && j < segs - 2;
				if (inner) {
					float lo = dist * 0.05f;
					float range = dist * 0.09f - lo;
					for (int k = 0; k < 2; k++) {
						vec3_t rd;
						float s = (float)((double)(crt_rand() & 0x7fff) * g_RandScale * (double)range + (double)lo);
						const vec3_t *rr = FX_RandomDir(&rd);
						X[k].x = s * rr->x + q.x;
						X[k].y = rr->y * s + q.y;
						X[k].z = rr->z * s + q.z;
					}
				} else {
					X[0] = q;
					X[1] = q;
				}
				prev = q;
				float wv = inner ? 1.0f : 0.5f;
				int c1 = crt_rand() % 10;
				uint32_t cr = (uint32_t)((int)gb + 0x37 + c1) << 16;
				int c2 = crt_rand() % 10;
				uint32_t color = (uint32_t)(c2 + 0x37) << 8 | c255 | cr | 0x40000000;
				CGameTrail_Add(&tr[0], q, wv * 375.0f, color);
				int c3 = crt_rand() % 18;
				uint32_t cb = (uint32_t)(c3 - (int)gb + 0x6e);
				int c4 = crt_rand() % 8;
				uint32_t cg = (uint32_t)((int)gb + 0x22 + c4) << 16 | cb;
				int c5 = crt_rand() % 8;
				CGameTrail_Add(&tr[1], q, 150.0f, (uint32_t)(c5 + 0x22) << 8 | cg | 0x18000000);
				for (int k = 0; k < 2; k++) {
					float wr = FXW_Rand(16.0, 48.0);
					CGameTrail_Add(&tr[2 + k], X[k], wr, ring);
				}
			}
		}

		/* the glow at the end and at the weapon node */
		vec3_t n2;
		FX_Normalize(&n2, tp->x - node.x, tp->y - node.y, tp->z - node.z);
		CGameParticles *p = &self->particles;
		uint32_t A = (uint32_t)alpha << 24;
		float gs = FXW_Rand(100.0, 350.0);
		CGameParticles_SetParticle(p, base - 2, tp->x, tp->y, tp->z, gs, A | 0xa4a4ff, 1, 0.0f);
		gs = FXW_Rand(200.0, 650.0);
		CGameParticles_SetParticle(p, base - 1, tp->x, tp->y, tp->z, gs, A | 0x5c5cff, 2, 0.0f);
		int a20 = alpha + 0x20;
		int a = a20 > 0xff ? 0xff : a20;
		gs = FXW_Rand(100.0, 550.0);
		CGameParticles_SetParticle(p, base, tp->x - n2.x * 80.0f, tp->y - n2.y * 80.0f, tp->z - n2.z * 80.0f, gs,
			(uint32_t)a << 24 | 0x5c5cc0, 2, 0.0f);
		a = alpha + 0x5c;
		if (a > 0xff) {
			a = 0xff;
		}
		gs = FXW_Rand(100.0, 550.0);
		CGameParticles_SetParticle(p, base + 1, tp->x - n2.x * 180.0f, tp->y - n2.y * 180.0f, tp->z - n2.z * 180.0f,
			gs, (uint32_t)a << 24 | 0x4040c0, 2, 0.0f);
		a = a20 > 0xff ? 0xff : a20;
		gs = FXW_Rand(100.0, 700.0);
		CGameParticles_SetParticle(p, base + 2, node.x, node.y, node.z, gs, (uint32_t)a << 24 | 0x4040ff, 2, 0.0f);
		gs = FXW_Rand(40.0, 320.0);
		CGameParticles_SetParticle(p, base + 3, node.x, node.y, node.z, gs, A | 0xc4c4ff, 1, 0.0f);
	}
	if (self->first) {
		self->first = 0;
	}
}

byte __thiscall cObjElementFXTesla_IsAlive(cObjElementFXTesla *self, cDebris *debris, cDebrisSys *sys) {
	(void)debris;
	(void)sys;
	cEntity *e = self->emitter;
	if (!e) {
		return 0;
	}
	/* the fade starts when the emitter stops shooting the Tesla (or has no ammo left) */
	if (!self->shooting || !Entity_B636(e) || !Entity_B436(e) || Entity_CurWeapon(e) != g_TeslaIDWeap
		|| 0.0f >= cTargetLock_GetAmmo(Entity_TargetLock(e), g_TeslaIDAmmo)) {
		self->shooting = 0;
		if (0.0f > self->fadeStart) {
			self->fadeStart = self->time;
		}
	}
	if (self->fadeStart >= 0.0f && self->time - self->fadeStart > g_FXTeslaFadeOutTime) {
		return 0;
	}
	return 1;
}

void __thiscall cObjElementFXTesla_Start(cObjElementFXTesla *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	/* the original keeps the debris pointer as the node when the model has none */
	int node = (int)(intptr_t)debris;
	if (CGameModel_IsLoaded(model)) {
		node = CGameModel_FindNodoR(model, "Dummy01");
	}
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CHECK(self->particles.SistPart);
	for (int i = 0; i < FXTESLA_SLOTS; i++) {
		self->stepTime[i] = 0.0f;
		self->uvOfs[i] = 0.0f;
		for (int j = 0; j < FXTESLA_TRAILS; j++) {
			CGameTrail *t = &self->trails[i * FXTESLA_TRAILS + j];
			CGameTrail_SetMaxTrails(t, 0x1e);
			CGameTrail_SetSmooth(t, 1);
			CGameTrail_SetTexture(t, j ? 5 : 0);
			CGameTrail_SetPremul(t, j != 0);
			CGameTrail_SetULen(t, 1.0f, 0.0f);
		}
	}
}

void __thiscall cObjElementFXTesla_Reset(cObjElementFXTesla *self, cDebris *debris) {
	(void)debris;
	self->time = 0.0f;
	self->fadeStart = -1.0f;
	self->followTime = g_FXTeslaFollowTime;
	self->name[0] = 0;
	for (int i = 0; i < FXTESLA_SLOTS; i++) {
		self->stepTime[i] = 0.0f;
		self->uvOfs[i] = 0.0f;
		if (self->slotOn[i]) {
			self->slotOn[i] = 0;
			for (int j = 0; j < FXTESLA_TRAILS; j++) {
				CGameTrail_Clear(&self->trails[i * FXTESLA_TRAILS + j]);
			}
		}
	}
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXTesla_SetEmitter(cObjElementFXTesla *self, cEntity *e) {
	self->emitter = e;
	self->target = NULL;
	self->name[0] = 0;
	self->shooting = 1;
	self->first = 1;
	CHECK(e);
	CHECK(CGameModel_IsLoaded(FX_Vtbl(self->emitter)->GetModel(self->emitter)));
	self->numSlots = 0;
	for (int i = 0; i < FXTESLA_SLOTS; i++) {
		if (cTargetLock_HasSlot(Entity_TargetLock(self->emitter), g_TeslaIDWeap, i)) {
			self->numSlots++;
			self->slotOn[i] = 1;
		} else {
			self->slotOn[i] = 0;
		}
	}
}

/* ---- the Swarm emitters ---- */

#define g_SwarmBeeExplosionFile GAME_DATA(const char *, 0x44a5c0)
#define g_SwarmExplosionFile GAME_DATA(const char *, 0x44a520)
#define FXSWARM_RAMP_BEE 0x431978
#define FXSWARM_RAMP_FLASH 0x431a2c		/* [2] */
#define FXSWARM_RAMP_SMOKE 0x4319c0		/* [3] */
#define FXSWARM_RAMP_SPARK 0x431a50		/* [2] */
#define FXSWARM_RAMP_HIT 0x431810		/* [2] */
#define FXSWARM_RAMP_HITSPARK 0x431858		/* [2] */

/* the particles of the element (a cObjElementFXSistPart) of a debris, switched on; NULL without element */
static CGameParticles *FXW_SistPart(cDebris *debris) {
	cObjElementFXSistPart *el = Debris_Element(debris);
	if (!el) {
		return NULL;
	}
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	return &el->particles;
}

/* an emission at pos with the velocity vel and no acceleration */
static void FXW_Moving(CGameParticles *p, const vec3_t *pos, const vec3_t *vel) {
	p->initPosMin = *pos;
	p->initPosMax = p->initPosMin;
	p->initVelMin = *vel;
	p->initVelMax = p->initVelMin;
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = 0.0f;
	p->initAccMin.z = 0.0f;
	p->initAccMax = p->initAccMin;
}

void FXSwarm_BeeDeath(float x, float y, float z, float vx, float vy, float vz, float size, float time, int n) {
	CHECK(size >= 0.01f);
	CHECK(time >= 0.01f);
	CHECK(n >= 1);
	if (!FX_IsVisible(x, y, z, size * 1500.0f, 250000.0f, NULL, 1)) {
		return;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_SwarmBeeExplosionFile);
	cDebris *debris = cDebrisSys_GetIdle(sys);
	CGameModel_SetSortZ(Debris_Model(debris), -0.5f, 0.0f);
	Debris_Life(debris) = time + time;
	if (!cDebrisSys_IsIdle(sys, debris)) {
		cDebrisSys_Drop(sys, debris);
	}
	CGameParticles *p = FXW_SistPart(debris);
	if (!p) {
		return;
	}
	vec3_t pos = { x, y, z };
	vec3_t vel = { vx, vy, vz };
	float r;

	/* the bee */
	CGameParticles_SetColorRampOn(p, 1);
	p->birthRate = 0;
	FXW_Moving(p, &pos, &vel);
	p->lifeTime = FXW_Rand(0.09999996423721313, 0.800000011920929) * time;
	p->sizeLife = FXW_Rand(0.040000006556510925, 0.23999999463558197);
	p->sizeIni = FXW_Rand(150.0, 350.0) * size;
	p->sizeEnd = FXW_Rand(500.0, 8000.0) * size;
	r = FXW_Rand(6.2831854820251465, 0.0);
	p->rotIni = r;
	p->rotEnd = r;
	p->rotVar = FXW_Rand(1.5707963705062866, -0.7853981852531433);
	CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXSWARM_RAMP_BEE));
	FXW_SetSprite(p, 0x72);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);

	/* the flash */
	FXW_Moving(p, &pos, &vel);
	p->sizeIni = FXW_Rand(50.0, 350.0) * size;
	p->sizeEnd = FXW_Rand(500.0, 4500.0) * size;
	p->lifeTime = FXW_Rand(0.30000007152557373, 0.8999999761581421) * time;
	r = FXW_Rand(6.2831854820251465, 0.0);
	p->rotIni = r;
	p->rotEnd = r;
	p->rotVar = FXW_Rand(0.5235987901687622, -0.2617993950843811);
	r = FXW_Rand(0.07999998331069946, 0.18000000715255737);
	p->spriteIni = 0x01;
	p->spriteEnd = 0x21;
	p->sizeLife = r;
	CGameParticles_Apply(p);
	CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXSWARM_RAMP_FLASH + (uint32_t)(crt_rand() & 1) * 0x24));
	CGameParticles_EmitN(p, 1, 1);

	/* n pieces around a circle: smoke and sparks */
	float base = FXW_Rand(6.2831854820251465, 0.0);
	float step = (float)(6.283185307179586 / (double)n);
	float a = 0.0f;
	do {
		vec3_t rv, v, o;
		rv.x = a + base;
		rv.y = rv.x;
		rv.z = rv.x;
		float r1 = FXW_Rand(350.0, 600.0);
		float r2 = FXW_Rand(350.0, 600.0);
		v.x = FXW_Rand(350.0, 600.0) * size;
		v.y = r2 * size;
		v.z = r1 * size;
		Vec3_Rotate(&o, &v, &rv);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetSparksOn(p, 0);
		p->initPosMin.x = o.x * 0.7f + x;
		p->initPosMin.y = o.y * 0.7f + y;
		p->initPosMin.z = o.z * 0.7f + z;
		p->initPosMax = p->initPosMin;
		float s1 = FXW_Rand(160.0, -80.0);
		float s2 = FXW_Rand(400.0, 550.0);
		r = FXW_Rand(160.0, -80.0);
		p->initVelMin.y = s2 * size + vy;
		p->initVelMin.z = s1 * size + vz;
		p->initVelMin.x = r * size + vx;
		p->initVelMax = p->initVelMin;
		p->initAccMin.x = 0.0f;
		p->initAccMin.y = -80.0f;
		p->initAccMin.z = 0.0f;
		p->initAccMax = p->initAccMin;
		p->sizeIni = FXW_Rand(500.0, 1500.0) * size;
		p->sizeEnd = FXW_Rand(500.0, 4000.0) * size;
		p->sizeLife = 1.0f;
		p->lifeTime = FXW_Rand(0.09999996423721313, 0.800000011920929) * time;
		r = FXW_Rand(6.2831854820251465, 0.0);
		p->rotIni = r;
		p->rotEnd = r;
		p->rotVar = FXW_Rand(1.0471975803375244, -0.5235987901687622);
		FXW_SetSprite(p, (byte)((crt_rand() & 1) + 0x7b));
		CGameParticles_Apply(p);
		CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXSWARM_RAMP_SMOKE + (uint32_t)(crt_rand() % 3) * 0x24));
		float t = FXW_Rand(0.10000000149011612, 0.0);
		CGameParticles_EmitDt(p, 1, 1, FXW_Rand(0.25, 0.10000000149011612) * time + t);

		p->initPosMin.z = o.z + z;
		p->initPosMin.y = o.y + y;
		p->initPosMin.x = o.x + x;
		p->initPosMax = p->initPosMin;
		p->initVelMin.x = vx - (p->initPosMin.x - x) * 0.1f;
		p->initVelMin.y = vy - (p->initPosMin.y - y) * 0.1f;
		p->initVelMin.z = vz - (p->initPosMin.z - z) * 0.1f;
		p->initVelMax = p->initVelMin;
		p->initAccMin.x = 0.0f;
		p->initAccMin.y = 0.0f;
		p->initAccMin.z = 0.0f;
		p->initAccMax = p->initAccMin;
		p->sizeIni = FXW_Rand(100.0, 600.0) * size;
		p->sizeEnd = FXW_Rand(500.0, 2500.0) * size;
		p->lifeTime = FXW_Rand(0.09999996423721313, 0.800000011920929) * time;
		r = FXW_Rand(6.2831854820251465, 0.0);
		p->rotVar = 0.0f;
		p->rotIni = r;
		p->rotEnd = r;
		p->sizeLife = FXW_Rand(0.17000000178813934, 0.10999999940395355);
		/* the cases fall through: the sprites of case 2 stay, the rand calls differ */
		switch (crt_rand() % 2) {
		case 0:
			p->spriteEnd = 0x41;
			p->spriteIni = (byte)(crt_rand() % 6 + 0x31);
			/* fall through */
		case 1:
			p->spriteEnd = 0x51;
			p->spriteIni = (byte)(crt_rand() % 6 + 0x41);
			/* fall through */
		case 2:
			p->spriteEnd = 0x61;
			p->spriteIni = (byte)(crt_rand() % 6 + 0x51);
			break;
		}
		CGameParticles_Apply(p);
		CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXSWARM_RAMP_SPARK + (uint32_t)(crt_rand() & 1) * 0x24));
		CGameParticles_EmitDt(p, 1, 1, FXW_Rand(0.10000000149011612, 0.0));
		a = step + a;
	} while (6.283185307179586 > (double)a);
}

/* a spark of the hit at P along the direction o: position, velocity and their slowing down */
static void FXW_HitSpark(CGameParticles *p, const vec3_t *pos, const vec3_t *o, float size, double prange,
	double padd, double vrange, double vadd) {
	float s = FXW_Rand(prange, padd);
	p->initPosMin.x = pos->x + o->x * s * size;
	p->initPosMin.y = pos->y + o->y * s * size;
	p->initPosMin.z = pos->z + o->z * s * size;
	p->initPosMax = p->initPosMin;
	s = FXW_Rand(vrange, vadd);
	p->initVelMin.x = o->x * s * size;
	p->initVelMin.y = o->y * s * size;
	p->initVelMin.z = o->z * s * size;
	p->initVelMax = p->initVelMin;
	s = FXW_Rand(0.10000002384185791, 0.5);
	p->initAccMin.x = -p->initVelMin.x * s;
	p->initAccMin.y = -p->initVelMin.y * s;
	p->initAccMin.z = -p->initVelMin.z * s;
	p->initAccMax = p->initAccMin;
}

/* the end of the sizes, life and rotation of a spark of the hit and its emission */
static void FXW_HitSparkEmit(CGameParticles *p, float size, double irange, double iadd, double erange, double eadd,
	double lrange, double ladd, double trange, double tadd) {
	p->sizeIni = FXW_Rand(irange, iadd) * size;
	p->sizeEnd = FXW_Rand(erange, eadd) * size;
	p->sizeLife = 1.0f;
	p->lifeTime = FXW_Rand(lrange, ladd);
	float r = FXW_Rand(6.2831854820251465, 0.0);
	p->rotVar = 0.2617994f;
	FXW_SetSprite(p, 0x12);
	p->rotIni = r;
	p->rotEnd = r;
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXSWARM_RAMP_HITSPARK + (uint32_t)(crt_rand() & 1) * 0x24));
	CGameParticles_EmitDt(p, 1, 1, FXW_Rand(trange, tadd));
}

/* the hit of a swarm at x y z (moving with v, the shot along d): smoke debris flying off, the bees, the explosion
 * moved out from the wall next to it and its sparks */
void FXSwarm_Hit(float x, float y, float z, float vx, float vy, float vz, float size, float time, float dx, float dy,
	float dz) {
	CHECK(size >= 0.01f);
	CHECK(time >= 0.01f);
	if (!FX_IsVisible(x, y, z, size * 1500.0f, 250000.0f, NULL, 1)) {
		return;
	}
	vec3_t dv = { dx, dy, dz };
	vec3_t n, tmp;
	Vec3_NormalizeTo(&n, &dv);
	vec3_t ang = *Vec3_GetRotAng(&tmp, &n);

	/* the smoke debris */
	int k = crt_rand() % 3 + 2;
	if (k > 0) {
		ang.x = -ang.x;
		ang.y = -ang.y;
		ang.z = -ang.z;
		do {
			cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXSmokeDebrisFile);
			cDebris *deb = cDebrisSys_NewElement(sys);
			vec3_t v, o;
			deb->speed = FXW_Rand(0.30000007152557373, 1.649999976158142) * time;
			cDebrisSys_Drop(sys, deb);
			float a1 = FXW_Rand(3.1415927410125732, -1.5707963705062866);
			float a2 = FXW_Rand(3.1415927410125732, -1.5707963705062866);
			v.x = FXW_Rand(3.1415927410125732, -1.5707963705062866);
			v.y = a2;
			v.z = a1;
			Vec3_Rotate(&o, &v, &ang);
			float s = FXW_Rand(115.0, 120.0);
			deb->drop[0].x = x + o.x * s * size;
			deb->drop[0].y = y + o.y * s * size;
			deb->drop[0].z = z + o.z * s * size;
			s = FXW_Rand(500.0, 4350.0);
			deb->drop[2].x = s * o.x * size;
			deb->drop[2].y = s * o.y * size;
			deb->drop[2].z = s * o.z * size;
			cObjElementFXSmokeDebris *el = Debris_Element(deb);
			if (el) {
				if (!el->on) {
					el->on = 1;
				}
				el->minDist = size * 100.0f;
				el->spread = FXW_Rand(32.0, 96.0) * size;
				el->scale = FXW_Rand(25.0, 600.0) * size;
			}
		} while (--k);
	}

	/* the explosion moves out from a wall within 500 in y, z or x */
	vec3_t pos = { x, y, z };
	vec3_t from, to, hit;
	CColisTri tri;
	float off = size * 500.0f;
	float offn = size * -500.0f;
	from.x = x + 0.0f;
	from.y = offn + y;
	from.z = z + 0.0f;
	to.x = from.x;
	to.y = off + y;
	to.z = from.z;
	byte hitWall = Engine_TestLine(&from, &to, &hit, &tri);
	if (!hitWall) {
		from.x = pos.x + 0.0f;
		from.y = pos.y + 0.0f;
		from.z = offn + pos.z;
		to.x = from.x;
		to.y = from.y;
		to.z = off + pos.z;
		hitWall = Engine_TestLine(&from, &to, &hit, &tri);
		if (!hitWall) {
			float py = from.y;
			from.x = offn + pos.x;
			from.y = py;
			from.z = pos.z + 0.0f;
			to.x = off + pos.x;
			to.y = py;
			to.z = from.z;
			hitWall = Engine_TestLine(&from, &to, &hit, &tri);
		}
	}
	vec3_t wn = { 0.0f, 0.0f, 0.0f };
	if (hitWall) {
		wn = tri.plane.n;
	}
	y = wn.y * 500.0f * size + y;
	z = wn.z * 500.0f * size + z;
	x = x + wn.x * 500.0f * size;
	float t = FXW_Rand(0.04999995231628418, 0.6000000238418579);
	float bs = FXW_Rand(0.039999961853027344, 0.5400000214576721) * size;
	FXSwarm_BeeDeath(x, y, z, vx, vy, vz, bs, t, 4);

	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_SwarmExplosionFile);
	cDebris *deb = cDebrisSys_NewElement(sys);
	vec3_t rd;
	const vec3_t *r = FX_RandomDir(&rd);
	vec3_t dp;
	dp.x = x + r->x;
	dp.y = r->y + y;
	dp.z = r->z + z;
	CGameModel_SetPos(Debris_Model(deb), &dp);
	CGameModel_SetSortZ(Debris_Model(deb), -0.4f, 0.0f);
	deb->speed = time + time;
	cDebrisSys_Drop(sys, deb);
	CGameParticles *p = FXW_SistPart(deb);
	if (!p) {
		return;
	}
	pos.x = x;
	pos.y = y;
	pos.z = z;
	vec3_t vel = { vx, vy, vz };
	CGameParticles_SetColorRampOn(p, 1);
	p->birthRate = 0;
	FXW_Moving(p, &pos, &vel);
	p->lifeTime = FXW_Rand(0.09999996423721313, 0.800000011920929) * time;
	p->sizeLife = FXW_Rand(0.040000006556510925, 0.23999999463558197);
	p->sizeIni = FXW_Rand(150.0, 350.0) * size;
	p->sizeEnd = FXW_Rand(500.0, 7000.0) * size;
	float rr = FXW_Rand(6.2831854820251465, 0.0);
	p->rotIni = rr;
	p->rotEnd = rr;
	p->rotVar = FXW_Rand(1.5707963705062866, -0.7853981852531433);
	CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXSWARM_RAMP_HIT + (uint32_t)(crt_rand() & 1) * 0x24));
	FXW_SetSprite(p, 5);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);

	/* 16 sparks in random directions */
	for (int i = 0; i < 16; i++) {
		FX_RandomDir(&rd);
		FXW_HitSpark(p, &pos, &rd, size, 160.0, 110.0, 130.0, 120.0);
		FXW_HitSparkEmit(p, size, 40.0, 325.0, 85.0, 700.0, 0.3999999761581421, 0.8999999761581421,
			0.05000000074505806, 0.0);
	}
	/* 64 around the sphere, every 45 degrees and a bit at random */
	float b = 0.0f;
	do {
		float a = 0.0f;
		do {
			vec3_t rot1, rot2, one, o;
			rot1.x = 0.0f;
			rot1.z = 0.0f;
			rot1.y = FXW_Rand(1.0, -0.5) + a;
			rot2.x = 0.0f;
			rot2.y = 0.0f;
			one.x = 1.0f;
			one.y = 0.0f;
			one.z = 0.0f;
			rot2.z = FXW_Rand(1.0, -0.5) + b;
			Vec3_Rotate(&o, Vec3_Rotate(&tmp, &one, &rot2), &rot1);
			FXW_HitSpark(p, &pos, &o, size, 210.0, 80.0, 250.0, 650.0);
			FXW_HitSparkEmit(p, size, 30.0, 285.0, 75.0, 750.0, 0.40000003576278687, 0.800000011920929,
				0.15000000596046448, 0.05000000074505806);
			a = a + 0.7853982f;
		} while (6.2831855f > a);
		b = b + 0.7853982f;
	} while (6.2831855f > b);
}

