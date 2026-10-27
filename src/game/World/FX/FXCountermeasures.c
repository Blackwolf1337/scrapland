/*
 * FXCountermeasures.cpp of the game: the effects of the hook laser and the countermeasures, see
 * include/scrap/fxcountermeasures.h.
 */
#include "scrap/crt.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/missileentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"
#include "scrap/world.h"

#define g_RandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */
#define FX_PI 3.1415927f				/* rva 0x3a3a30 */
#define FX_PI_3 1.0471976f				/* rva 0x3b084c */

/* the model of a debris is its first member */
#define Debris_Model(d) ((CGameModel *)(d))

/* cEntity slot 0x78: the angles of the entity into out */
static inline vec3_t *Entity_GetRot(cEntity *e, vec3_t *out) {
	return ((vec3_t * (__thiscall *)(cEntity *, vec3_t *)) WEnt_Vtbl(e)->vf1e_21[0])(e, out);
}

static inline CGameModel *Entity_Model(cEntity *e) {
	return WEnt_Vtbl(e)->GetObjData(e);
}

/* rand() in [0, 1] times range, plus add (all in double) */
static inline float Fx_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * g_RandScale * range + add);
}

vec3_t *__thiscall cWorld_GetCamPos(cWorld *self, vec3_t *out) {
	*out = self->camPos[self->curCam];
	return out;
}

/* ---- cObjElementFXHook ---- */

void __fastcall cObjElementFXHook_StopTrails(cObjElementFXHook *self) {
	if (CGameTrail_IsRender(&self->trail[0])) {
		CGameParticles_KillAll(&self->particles);
		CGameTrail_Clear(&self->trail[0]);
		CGameTrail_Clear(&self->trail[1]);
	}
}

/* the hook from the weapon node of the emitter to the target, the points between on a line */
void __thiscall cObjElementFXHook_StartTarget(cObjElementFXHook *self, cEntity *emitter, cEntity *target, int slot) {
	matrix_t mat;
	vec3_t rot, r, off;
	const vec3_t *end;

	self->withTarget = 1;
	self->emitter = emitter;
	self->target = target;
	CHECK(self->emitter);
	CGameModel *model = Entity_Model(self->emitter);
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	self->slot = slot;
	self->active = 1;
	self->fade = 0.0f;
	const matrix_t *m = CGameModel_NodoMatrix(model, &mat, Entity_WeapNodes(self->emitter)[slot + g_LaserIDAmmo * 4],
		0, NULL, 0);
	off.x = 0.0f;
	off.y = 0.0f;
	off.z = -500.0f;
	self->points[0].x = m->m[12];
	self->points[0].y = m->m[13];
	self->points[0].z = m->m[14];
	Vec3_Rotate(&r, &off, Entity_GetRot(self->emitter, &rot));
	self->points[1].x = r.x + self->points[0].x;
	self->points[1].y = self->points[0].y + r.y;
	self->points[1].z = self->points[0].z + r.z;
	end = NULL;
	if (self->target && self->target->flag_4c && Entity_IsType(self->target, TYEN_WITH_LIFE)) {
		end = WEnt_Vtbl(self->target)->GetCenter(self->target, &r);
	} else if (self->active && cTargetLock_GetPoint(Entity_TargetLock(self->emitter))) {
		end = cTargetLock_GetPoint(Entity_TargetLock(self->emitter));
	}
	if (end) {
		self->points[self->numPoints] = *end;
	}
	int n = self->numPoints;
	float inv = 1.0f / (float)(n + 1);
	float dx = (self->points[n].x - self->points[0].x) * inv;
	float dy = (self->points[n].y - self->points[0].y) * inv;
	float dz = (self->points[n].z - self->points[0].z) * inv;
	if (n >= 2) {
		for (int i = 2; i <= self->numPoints; i++) {
			float f = (float)i;
			self->points[i].x = self->points[0].x + dx * f;
			self->points[i].y = self->points[0].y + dy * f;
			self->points[i].z = self->points[0].z + dz * f;
		}
	}
	CGameTrail_Clear(&self->trail[0]);
	CGameTrail_Clear(&self->trail[1]);
	CHECK(self->particles.SistPart);
}

/* the direction from a to b, the inline normalize of the original */
static inline vec3_t FXHook_Dir(const vec3_t *a, const vec3_t *b) {
	return FX_Unit((vec3_t){ b->x - a->x, b->y - a->y, b->z - a->z });
}

static inline uint32_t FXHook_Alpha(int v) {
	return (uint32_t)(v > 0xff ? 0xff : v) << 24;
}

/* a glow at p - d * k */
static void FXHook_Glow(cObjElementFXHook *self, int i, const vec3_t *p, const vec3_t *d, float k, float size, int alpha,
	uint32_t rgb) {
	float x = p->x - d->x * k, y = p->y - d->y * k, z = p->z - d->z * k;

	CGameParticles_SetParticle(&self->particles, i, x, y, z, g_FXHookSize * size, FXHook_Alpha(alpha) | rgb, 1, 0.0f);
}

/* a glow at p + d * k */
static void FXHook_GlowOut(cObjElementFXHook *self, int i, const vec3_t *p, const vec3_t *d, float k, float size,
	int alpha, uint32_t rgb) {
	float x = d->x * k + p->x, y = p->y + d->y * k, z = p->z + d->z * k;

	CGameParticles_SetParticle(&self->particles, i, x, y, z, g_FXHookSize * size, FXHook_Alpha(alpha) | rgb, 1, 0.0f);
}

/* the sparks debris at the hook end against dir (the normal of the wall when the end is near one) */
static void FXHook_Sparks(cObjElementFXHook *self, const vec3_t *end, vec3_t dir) {
	vec3_t hit, n;
	CColisTri tri;

	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_LaserHookSparksFile);
	cDebris *d = cDebrisSys_GetIdle(sys);
	vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
	CGameModel_SetPos(Debris_Model(d), &cam);
	Debris_Life(d) = 1.0f;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	}
	cObjElementFXSparks *sp = Debris_Element(d);
	if (!sp) {
		return;
	}
	if (!sp->on) {
		sp->on = 1;
	}
	vec3_t to = { end->x + dir.x * 50.0f, end->y + dir.y * 50.0f, end->z + dir.z * 50.0f };
	if (Engine_TestLine(end, &to, &hit, &tri)) {
		dir = *Vec3_NormalizeTo(&n, &tri.plane.n);
	}
	int cnt = 1 - (int)(self->sparkTime / -0.04f);
	if (cnt <= 0) {
		return;
	}
	uint32_t g = (uint32_t)(crt_rand() % 64 + 0x80) << 8;
	uint32_t r = (uint32_t)(crt_rand() % 64 + 0x80) << 16;
	cObjElementFXSparks_Emit(sp, cnt, end->x, end->y, end->z, dir.x, dir.y, dir.z, FX_PI, FX_PI, FX_PI, 0.0f, 0.0f,
		0.0f, 5500.0f, 6500.0f, 85.0f, 95.0f, 1.0f, 0.8f, 0.2f, 0.24f, 0.08f, 0.1f, 0.0f, self->sparkTime,
		r | g | 0x40000ff, 200.0f, 800.0f, 5.0f, 6.0f, 1);
}

void __thiscall cObjElementFXHook_Update(cObjElementFXHook *self, cDebris *debris, float dt) {
	matrix_t mat;
	vec3_t tmp, rot, r, pt, coefs[FXHOOK_POINTS + 2];
	CGameModel *model = NULL;

	(void)dt;
	cEntity *e = self->emitter;
	if (e && (Entity_Removed(e) || !e->flag_4c)) {
		self->emitter = NULL;
	}
	e = self->target;
	if (e && (Entity_Removed(e) || !e->flag_4c)) {
		self->target = NULL;
	}
	float delta = cTimer_GetDelta(&g_pWorld->timer);
	self->sparkTime = self->sparkTime + delta;
	e = self->emitter;
	if (e && e->flag_4c) {
		CGameModel_SetPos(Debris_Model(debris), FX_Vtbl(e)->GetCenter(e, &tmp));
		CGameModel_SetSortZ(Debris_Model(debris), -5.0f, 1.0f);
	}
	e = self->emitter;

	if (!self->withTarget) {
		/* the line without target */
		if (!e || !e->flag_4c) {
			return;
		}
		model = Entity_Model(e);
		CHECK(CGameModel_IsLoaded(model));
		if (!model) {
			cObjElementFXHook_StopTrails(self);
			return;
		}
		if (!CGameModel_IsVisible(model)) {
			const vec3_t *c = FX_Vtbl(self->emitter)->GetCenter(self->emitter, &rot);
			const vec3_t *cam = cWorld_GetCamPos(g_pWorld, &tmp);
			float dy = cam->y - c->y, dx = cam->x - c->x, dz = cam->z - c->z;
			if (!(g_HookRange * g_HookRange > dy * dy + dx * dx + dz * dz)) {
				cObjElementFXHook_StopTrails(self);
				return;
			}
		}
		const matrix_t *m = CGameModel_NodoMatrix(model, &mat,
			Entity_WeapNodes(self->emitter)[self->slot + g_LaserIDAmmo * 4], 0, NULL, 0);
		vec3_t off = { 0.0f, 0.0f, -g_HookRange };
		self->points[0].x = m->m[12];
		self->points[0].y = m->m[13];
		self->points[0].z = m->m[14];
		int n = CGameTrail_GetMaxTrails(&self->trail[0]) - 1;
		const vec3_t *v = Vec3_Rotate(&r, &off, FX_Vtbl(self->emitter)->GetAngles(self->emitter, &rot));
		float inv = 1.0f / (float)n;
		float sx = inv * v->x, sy = v->y * inv, sz = v->z * inv;
		CGameTrail_Reset(&self->trail[0]);
		CGameTrail_SetTransp(&self->trail[0], 0);
		for (int i = 0; i < CGameTrail_GetMaxTrails(&self->trail[0]) - 1; i++) {
			CGameTrail_Add(&self->trail[0], self->points[0], g_FXHookSize * 2.0f, 0x64640a0a);
			self->points[0].x = sx + self->points[0].x;
			self->points[0].y = sy + self->points[0].y;
			self->points[0].z = sz + self->points[0].z;
		}
		return;
	}

	/* with target: the points between are pulled to the line by the tensor */
	if (e && e->flag_4c) {
		model = Entity_Model(e);
		CHECK(CGameModel_IsLoaded(model));
		const matrix_t *m = CGameModel_NodoMatrix(model, &mat,
			Entity_WeapNodes(self->emitter)[self->slot + g_LaserIDAmmo * 4], 0, NULL, 0);
		vec3_t off = { 0.0f, 0.0f, -500.0f };
		self->points[0].x = m->m[12];
		self->points[0].y = m->m[13];
		self->points[0].z = m->m[14];
		const vec3_t *v = Vec3_Rotate(&r, &off, FX_Vtbl(self->emitter)->GetAngles(self->emitter, &rot));
		self->points[1].x = v->x + self->points[0].x;
		self->points[1].y = self->points[0].y + v->y;
		self->points[1].z = self->points[0].z + v->z;
		const vec3_t *end = NULL;
		e = self->target;
		if (e && e->flag_4c && Entity_IsType(e, TYEN_WITH_LIFE)) {
			end = FX_Vtbl(self->target)->GetCenter(self->target, &tmp);
		} else if (self->active && cTargetLock_GetPoint(Entity_TargetLock(self->emitter))) {
			end = cTargetLock_GetPoint(Entity_TargetLock(self->emitter));
		}
		if (end) {
			self->points[self->numPoints] = *end;
		}
		int n = self->numPoints;
		float inv = 1.0f / (float)(n + 1);
		float dz = (self->points[n].z - self->points[0].z) * inv;
		float dx = (self->points[n].x - self->points[0].x) * inv;
		float dy = (self->points[n].y - self->points[0].y) * inv;
		if (n >= 2) {
			for (int i = 2; i <= self->numPoints; i++) {
				float f = (float)i;
				vec3_t *p = &self->points[i];
				float tx = self->points[0].x + dx * f;
				float ty = self->points[0].y + dy * f;
				float tz = self->points[0].z + dz * f;
				float ex = (p->x - tx) * g_FXHookTensor * delta;
				float ey = (p->y - ty) * g_FXHookTensor * delta;
				float ez = (p->z - tz) * g_FXHookTensor * delta;
				p->x = p->x - ex;
				p->y = p->y - ey;
				p->z = p->z - ez;
			}
		}
	}
	if (!self->active) {
		self->fade = self->fade + delta;
	}
	if (!model) {
		goto off;
	}
	if (!CGameModel_IsVisible(model)) {
		vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
		const vec3_t *c = FX_Vtbl(self->emitter)->GetCenter(self->emitter, &tmp);
		float dy = cam.y - c->y, dx = cam.x - c->x, dz = cam.z - c->z;
		if (!(g_HookRange * g_HookRange > dy * dy + dx * dx + dz * dz)) {
			goto off;
		}
	}

	/* the curve through the points, fading out with the fade time */
	int alpha = (int)((self->fade / g_FXHookFadeTime) * 255.0f);
	if (alpha < 0) {
		alpha = 0;
	} else if (alpha > 0xff) {
		alpha = 0xff;
	}
	int n2 = self->numPoints + 2;
	CGameTrail_BezierCoefs(&self->trail[0], n2, self->points, coefs);
	CGameTrail_SetTransp(&self->trail[0], (byte)alpha);
	CGameTrail_SetTransp(&self->trail[1], (byte)alpha);
	int cnt = CGameTrail_GetMaxTrails(&self->trail[0]) - 1;
	for (int i = 0; i < cnt; i++) {
		int m = CGameTrail_GetMaxTrails(&self->trail[0]) - 2;
		CGameTrail_BezierPoint(&self->trail[0], n2, coefs, &pt, (float)i / (float)m);
		CGameTrail_Add(&self->trail[0], pt, g_FXHookSize, 0x22020ff);
		CGameTrail_Add(&self->trail[1], pt, g_FXHookSize * 3.0f, 0x404040c0);
	}
	CHECK(self->particles.SistPart);
	CGameParticles_SetSparksOn(&self->particles, 0);
	CGameParticles_SetColorRampOn(&self->particles, 0);
	uint32_t a24 = (uint32_t)alpha << 24;
	for (int i = 0; i < 5; i++) {
		CGameTrail_BezierPoint(&self->trail[0], n2, coefs, &pt, (float)i * 0.2f + self->flash * 0.2f);
		CGameParticles_SetParticle(&self->particles, i, pt.x, pt.y, pt.z, g_FXHookSize * 2.5f, a24 | 0x2020c0, 1, 0.0f);
	}
	float fl = self->flash - delta * 2.0f;
	self->flash = fl;
	if (0.0f > fl) {
		self->flash = 1.0f;
	}

	/* the end: sparks for a player without target, glows back along the last piece */
	int np = self->numPoints;
	vec3_t end = self->points[np];
	vec3_t nd = FXHook_Dir(&self->points[np - 1], &self->points[np]);
	e = self->emitter;
	if (e && e->flag_4c && Entity_PlayerCam(e) && !self->target && 0.0f >= self->fade && self->sparkTime > 0.04f) {
		FXHook_Sparks(self, &end, nd);
		self->sparkTime = 0.0f;
	}
	FXHook_Glow(self, 5, &end, &nd, 275.0f, 10.0f, alpha + 0xdc, 0x5a5aff);
	FXHook_Glow(self, 6, &end, &nd, 250.0f, 15.0f, alpha + 0xc0, 0x6464ff);
	FXHook_Glow(self, 7, &end, &nd, 200.0f, 20.0f, alpha + 0xa4, 0x6e6eff);
	FXHook_Glow(self, 8, &end, &nd, 100.0f, 25.0f, alpha + 0x80, 0x8080ff);
	FXHook_Glow(self, 9, &end, &nd, 70.0f, 15.0f, alpha + 0x40, 0x2020ff);
	FXHook_Glow(self, 10, &end, &nd, 40.0f, 10.0f, alpha, 0x4040ff);
	CGameParticles_SetParticle(&self->particles, 11, end.x, end.y, end.z, g_FXHookSize * 4.0f, a24 | 0x8080ff, 1, 0.0f);
	/* the start: glows out along the first piece */
	const vec3_t *p0 = &self->points[0];
	vec3_t n0 = FXHook_Dir(&self->points[0], &self->points[1]);
	CGameParticles_SetParticle(&self->particles, 12, p0->x, p0->y, p0->z, g_FXHookSize * 6.0f,
		FXHook_Alpha(alpha + 0x80) | 0x4040ff, 1, 0.0f);
	FXHook_GlowOut(self, 13, p0, &n0, 25.0f, 5.0f, alpha + 0xa4, 0x6e6eff);
	FXHook_GlowOut(self, 14, p0, &n0, 40.0f, 4.0f, alpha + 0xc0, 0x5c5cff);
	FXHook_GlowOut(self, 15, p0, &n0, 50.0f, 2.0f, alpha + 0xdc, 0x4040ff);
	return;

off:
	if (CGameTrail_IsRender(&self->trail[0])) {
		CGameParticles_KillAll(&self->particles);
		CGameTrail_Clear(&self->trail[0]);
		CGameTrail_Clear(&self->trail[1]);
	}
}

/* the hook without target: only the line out of the weapon node */
void __thiscall cObjElementFXHook_StartPoint(cObjElementFXHook *self, cEntity *emitter, int slot) {
	self->withTarget = 0;
	self->emitter = emitter;
	CHECK(self->emitter);
	CGameModel *model = Entity_Model(self->emitter);
	if (CGameModel_IsLoaded(model)) {
		CHECK(CGameModel_IsLoaded(model));
		self->active = 1;
		self->fade = 0.0f;
		self->slot = slot;
		CGameTrail_Clear(&self->trail[0]);
	}
}

byte __thiscall cObjElementFXHook_IsAlive(cObjElementFXHook *self, cDebris *debris, cDebrisSys *sys) {
	(void)debris;
	(void)sys;
	if (self->withTarget) {
		if (self->emitter && (cTargetLock_GetPoint(Entity_TargetLock(self->emitter))
				|| cTargetLock_GetEntity(Entity_TargetLock(self->emitter)))
			&& !(self->fade > 0.0f)) {
			return 1;
		}
		self->active = 0;
		return !(self->fade >= g_FXHookFadeTime);
	}
	if (self->emitter && !cTargetLock_GetPoint(Entity_TargetLock(self->emitter))
		&& !cTargetLock_GetEntity(Entity_TargetLock(self->emitter))) {
		if (!self->emitter) {
			return 1;
		}
		if (PtrArray_Get(Entity_Weapons(self->emitter), WEAPON_LASER)
			&& Weapon_HookOn(PtrArray_Get(Entity_Weapons(self->emitter), WEAPON_LASER))) {
			return 1;
		}
	}
	self->active = 0;
	return 0;
}

void __thiscall cObjElementFXHook_Start(cObjElementFXHook *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	/* the original keeps the debris pointer as the node when the model has none */
	int node = (int)(intptr_t)debris;
	if (CGameModel_IsLoaded(model)) {
		node = CGameModel_FindNodoR(model, "Dummy01");
	}
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CGameTrail_SetMaxTrails(&self->trail[0], 0x1e);
	CGameTrail_SetColor(&self->trail[0], 0xffff0000);
	CGameTrail_SetMaxTrails(&self->trail[1], 0x1e);
	CGameTrail_SetColor(&self->trail[1], 0xffff0000);
	self->sparkTime = 0.0f;
}

void __thiscall cObjElementFXHook_Reset(cObjElementFXHook *self, cDebris *debris) {
	(void)debris;
	CGameParticles_KillAll(&self->particles);
	CGameTrail_Clear(&self->trail[0]);
	CGameTrail_Clear(&self->trail[1]);
	self->sparkTime = 0.0f;
}

/* ---- cObjElementFXEMI ---- */

byte __thiscall cObjElementFXEMI_IsAlive(cObjElementFXEMI *self, cDebris *debris, cDebrisSys *sys) {
	if (!self->entity) {
		return 0;
	}
	return cDebris_Update(debris, sys);
}

void __thiscall cObjElementFXEMI_Start(cObjElementFXEMI *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	self->node = -1;
	if (CGameModel_IsLoaded(model)) {
		self->node = CGameModel_FindNodoR(model, "Dummy01");
	}
	if (self->node != -1) {
		CGameParticles_Attach(&self->particles, model, self->node);
	}
	CHECK(self->particles.SistPart);
	for (int i = 0; i < FXEMI_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 0x10);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetColor(t, 0xc0c0ff);
		CGameTrail_SetTexture(t, (~i & 1) | 8);
		CGameTrail_SetPremul(t, 1);
		CGameTrail_SetUVScale(t, 0.25f, 1.0f);
		self->c74[i] = 0.5f;
		self->trailTime[i] = -1.0f;
		self->c54[i] = 0.0f;
	}
	self->d18 = 0;
}

void __thiscall cObjElementFXEMI_Reset(cObjElementFXEMI *self, cDebris *debris) {
	(void)debris;
	self->entity = NULL;
	self->bc = 0;
	self->c4 = 0;
	for (int i = 0; i < FXEMI_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
		self->trailTime[i] = -1.0f;
		self->c54[i] = 0.0f;
		/* Nto1Links.h line 0x4e */
		if (!self->links[i].data) {
			CheckFailed("Node", __FILE__, __TIMESTAMP__, __LINE__);
		}
		Nto1Node_Unlink(&self->links[i]);
	}
	CGameParticles_KillAll(&self->particles);
	self->d18 = 0;
}

void __thiscall cObjElementFXEMI_SetEntity(cObjElementFXEMI *self, cEntity *e) {
	self->entity = e;
}

/* ---- cObjElementFXSonicExplosion ---- */

void __thiscall cObjElementFXSonicExplosion_Update(cObjElementFXSonicExplosion *self, cDebris *debris, float dt) {
	(void)debris;
	(void)dt;
	float t = cTimer_GetDelta(&g_pWorld->timer);
	self->time += t;
	self->time2 += t;
	if (!g_SceneDuDvRTarget) {
		return;
	}
	float h = t * 0.5f;
	for (int i = 0; i < FXSONIC_RINGS; i++) {
		float v = self->ringVel[i];
		self->ringSize[i] = v * t + self->ringSize[i];
		self->ringVel[i] = self->ringAcc[i] * t + v;
		self->ringFade[i] = h + self->ringFade[i];
	}
	for (int i = 0; i < FXSONIC_RINGS; i++) {
		int c = (int)((1.0f - self->ringFade[i]) * 255.0f);
		if (c > 0xff) {
			c = 0xff;
		} else if (c < 0) {
			c = 0;
		}
		int sprite;
		float rot;
		if (i == 0) {
			rot = FX_PI;
			sprite = 1;
		} else {
			rot = 0.0f;
			sprite = (i < 2) + 3;
		}
		uint32_t u = (uint32_t)c;
		uint32_t color = ((~u << 8 | u) << 8 | u) << 8 | u;
		CGameParticles_SetParticle(&self->rings, i, self->pos.x, self->pos.y, self->pos.z, self->ringSize[i], color,
			sprite, rot);
	}
}

byte __thiscall cObjElementFXSonicExplosion_IsAlive(cObjElementFXSonicExplosion *self, cDebris *debris,
	cDebrisSys *sys) {
	(void)self;
	return cDebris_Update(debris, sys);
}

void __thiscall cObjElementFXSonicExplosion_Start(cObjElementFXSonicExplosion *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	int node = CGameModel_FindNodoR(model, "Dummy01");
	CHECK(node > -1);
	CGameParticles_Attach(&self->particles, model, node);
	node = CGameModel_FindNodoR(model, "Dummy02(+dudv_pass)");
	CHECK(node > -1);
	CGameParticles_Attach(&self->rings, model, node);
}

void __thiscall cObjElementFXSonicExplosion_Reset(cObjElementFXSonicExplosion *self, cDebris *debris) {
	(void)debris;
	self->time = 0.0f;
	self->time2 = 0.0f;
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXSonicExplosion_Explode(cObjElementFXSonicExplosion *self, float x, float y, float z) {
	static const float mult[FXSONIC_RINGS] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f };
	static const float fade[FXSONIC_RINGS] = { 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f };
	CGameParticles *p = &self->particles;

	self->pos.x = x;
	self->pos.y = y;
	self->pos.z = z;
	self->scale = 1.0f;
	for (int i = 0; i < FXSONIC_RINGS; i++) {
		self->ringSize[i] = g_FXSonicRadius * mult[i] * 0.075f;
		self->ringVel[i] = g_FXSonicRadius * mult[i] * 0.4f;
		self->ring1dc[i] = 0.0f;
		self->ringAcc[i] = -g_FXSonicRadius;
		self->ringFade[i] = fade[i];
	}

	/* the burst: 7 particles that grow */
	CGameParticles_SetSparksOn(p, 0);
	CGameParticles_SetColorRampOn(p, 0);
	p->birthRate = 0;
	p->initPosMin.x = self->pos.x - 650.0f;
	p->initPosMin.y = self->pos.y - 650.0f;
	p->initPosMin.z = self->pos.z - 650.0f;
	p->initPosMax.x = self->pos.x + 650.0f;
	p->initPosMax.y = self->pos.y + 650.0f;
	p->initPosMax.z = self->pos.z + 650.0f;
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMax.x = 0.0f;
	p->initVelMax.y = 0.0f;
	p->initVelMax.z = 0.0f;
	p->colorIni = 0x10dedeff;
	p->colorEnd = 0xff8080ff;
	p->lifeTime = 0.5f;
	p->sizeLife = 0.4f;
	p->spriteIni = 5;
	p->spriteEnd = 5;
	for (int i = 1; i < 8; i++) {
		int r = crt_rand() & 0x7fff;
		p->rotVar = 0.0f;
		float f = (float)((double)r * g_RandScale * 6.2831854820251465 + 0.0);
		p->rotIni = f;
		p->rotEnd = f;
		p->sizeIni = (g_FXSonicRadius / 10.0f) * (float)i;
		p->sizeEnd = g_FXSonicRadius * 0.25f * (float)i;
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, (float)i * 0.02f);
	}

	/* the flash */
	CGameParticles_SetColorRampOn(p, 1);
	p->birthRate = 0;
	p->initPosMin = self->pos;
	p->initPosMax = self->pos;
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMax.x = 0.0f;
	p->initVelMax.y = 0.0f;
	p->initVelMax.z = 0.0f;
	p->sizeIni = 0.0f;
	p->sizeEnd = g_FXSonicRadius;
	p->colorIni = 0x8b4b4ff;
	p->colorEnd = 0xff5050ff;
	p->lifeTime = 0.5f;
	p->sizeLife = 1.0f;
	p->rotIni = 0.0f;
	p->rotEnd = 0.0f;
	p->rotVar = 0.0f;
	p->spriteIni = 0x38;
	p->spriteEnd = 0x38;
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
	CGameParticles_SetColorRampOn(p, 1);
	p->birthRate = 0;

	/* sparks and smoke on a sphere, every 60 degrees */
	float b = 0.0f;
	do {
		float a = 0.0f;
		do {
			vec3_t ang, one, d;
			ang.x = b;
			ang.y = a;
			ang.z = 0.0f;
			one.x = 1.0f;
			one.y = 1.0f;
			one.z = 1.0f;
			Vec3_Rotate(&d, &one, &ang);
			float r1 = Fx_Rand(30.0, -15.0);
			float r2 = Fx_Rand(30.0, -15.0);
			float r3 = Fx_Rand(30.0, -15.0);
			p->initPosMin.x = r3 + (self->pos.x + d.x * 30.0f);
			p->initPosMin.y = r2 + (self->pos.y + d.y * 30.0f);
			p->initPosMin.z = r1 + (self->pos.z + d.z * 30.0f);
			p->initPosMax = p->initPosMin;
			float k = g_FXSonicRadius / 60.0f;
			p->initVelMin.x = (p->initPosMin.x - self->pos.x) * k;
			p->initVelMin.y = (p->initPosMin.y - self->pos.y) * k;
			p->initVelMin.z = (p->initPosMin.z - self->pos.z) * k;
			p->initVelMax = p->initVelMin;
			p->initAccMin.x = d.x * 100.0f;
			p->initAccMin.y = d.y * 100.0f;
			p->initAccMin.z = d.z * 100.0f;
			p->initAccMax.x = d.x * 1250.0f;
			p->initAccMax.y = d.y * 1250.0f;
			p->initAccMax.z = d.z * 1250.0f;
			CGameParticles_SetSparksOn(p, 1);
			p->rotIni = 0.0f;
			p->rotEnd = 0.0f;
			p->rotVar = 0.0f;
			p->sizeIni = 500.0f;
			p->sizeEnd = 500.0f;
			p->sizeLife = 1.0f;
			p->spriteIni = 0x47;
			p->spriteEnd = 0x47;
			p->lifeTime = Fx_Rand(0.10000002384185791, 0.5);
			CGameParticles_Apply(p);
			CGameParticles_EmitN(p, 1, 1);
			CGameParticles_SetSparksOn(p, 0);
			p->rotIni = 0.0f;
			p->rotEnd = 0.0f;
			p->rotVar = 0.0f;
			p->sizeIni = 1750.0f;
			p->sizeEnd = 2250.0f;
			p->sizeLife = 1.0f;
			p->spriteIni = 2;
			p->spriteEnd = 2;
			p->lifeTime = Fx_Rand(0.09999999403953552, 0.3499999940395355);
			CGameParticles_Apply(p);
			CGameParticles_EmitN(p, 1, 1);
			a += FX_PI_3;
		} while (M_2PI_D > (double)a);
		b += FX_PI_3;
	} while (M_2PI_D > (double)b);
}

/* ---- cObjElementFXDebrisCloud ---- */

static void cObjElementFXDebrisCloud_Shuffle(cObjElementFXDebrisCloud *self) {
	for (int i = 0; i < FXDEBRISCLOUD_RANDS; i++) {
		double d = (double)(crt_rand() & 0x7fff) * g_RandScale;
		self->rands[i] = (float)(d + d + 0.0);
	}
}

byte __thiscall cObjElementFXDebrisCloud_IsAlive(cObjElementFXDebrisCloud *self, cDebris *debris, cDebrisSys *sys) {
	(void)self;
	return cDebris_Update(debris, sys);
}

void __thiscall cObjElementFXDebrisCloud_Start(cObjElementFXDebrisCloud *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	self->node = -1;
	if (CGameModel_IsLoaded(model)) {
		self->node = CGameModel_FindNodoR(model, "Dummy01");
	}
	if (self->node != -1) {
		CGameParticles_Attach(&self->particles, model, self->node);
	}
	CHECK(self->particles.SistPart);
	cObjElementFXDebrisCloud_Shuffle(self);
}

void __thiscall cObjElementFXDebrisCloud_Reset(cObjElementFXDebrisCloud *self, cDebris *debris) {
	(void)debris;
	self->entity = NULL;
	self->emitT = 0.0f;
	self->time = 0.0f;
	CGameParticles_KillAll(&self->particles);
	cObjElementFXDebrisCloud_Shuffle(self);
}

#define FXDEBRISCLOUD_RAMPS 0x4308f8		/* 3 color ramps of 9 colors */
#define FX_PI_4 0.785398185f			/* rva 0x3b0840 */
#define FX_2PI 6.28318548f			/* rva 0x3a3a34 */

/* (3 - 2f) f f */
static inline float FXCM_Smooth(float f) {
	return (3.0f - (f + f)) * (f * f);
}

/* a puff of debris from the center out along a random direction (scale: a random factor rounded to tenths) */
static void FXDebrisCloud_Puff(cObjElementFXDebrisCloud *self, float size, double kRange, double kAdd, double rotRange,
	double rotAdd, double lifeRange, double lifeAdd, float sizeLo, float sizeHi, double endRange, double endAdd,
	int sprite) {
	CGameParticles *p = &self->particles;
	vec3_t tmp;

	const vec3_t *d = FX_RandomDir(&tmp);
	float dx = d->x * size, dy = d->y * size, dz = d->z * size;
	float k = (float)(int)Fx_Rand(kRange, kAdd) / 10.0f;
	p->initPosMin.x = k * dx;
	p->initPosMin.y = k * dy;
	p->initPosMin.z = k * dz;
	p->initPosMax = p->initPosMin;
	p->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initVelMax = p->initVelMin;
	p->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initAccMax = p->initAccMin;
	float r = Fx_Rand(6.2831854820251465, 0.0);
	p->rotIni = r;
	p->rotEnd = r;
	p->rotVar = Fx_Rand(rotRange, rotAdd);
	p->lifeTime = Fx_Rand(lifeRange, lifeAdd);
	float lo = sizeLo * size;
	p->sizeIni = Fx_Rand((double)(sizeHi * size - lo), (double)lo);
	float e = Fx_Rand(endRange, endAdd);
	p->sizeLife = 1.0f;
	if (sprite) {
		p->spriteIni = (byte)sprite;
		p->spriteEnd = (byte)sprite;
	} else {
		int b = crt_rand() & 1;
		if (b == 0) {
			p->spriteIni = p->spriteEnd = 0x13;
		} else if (b == 1) {
			p->spriteIni = p->spriteEnd = 0x2c;
		}
	}
	p->sizeEnd = p->sizeIni * e;
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(FXDEBRISCLOUD_RAMPS) + (crt_rand() % 3) * 9);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 0);
}

void __thiscall cObjElementFXDebrisCloud_Update(cObjElementFXDebrisCloud *self, cDebris *debris, float dt) {
	CGameModel *model = Debris_Model(debris);
	vec3_t pos, rot, up, ang;
	float f;

	(void)dt;
	cEntity *e = self->entity;
	if (e && (Entity_Removed(e) || !e->flag_4c)) {
		self->entity = NULL;
	}
	if (!self->entity) {
		return;
	}
	float delta = cTimer_GetDelta(&g_pWorld->timer);
	self->time = self->time + delta;
	self->emitT = self->emitT + delta;
	e = self->entity;
	if (e && e->flag_4c) {
		FX_Vtbl(e)->GetCenter(e, &pos);
		FX_Vtbl(self->entity)->GetAngles(self->entity, &rot);
		CGameModel_SetPos(model, &pos);
		CGameModel_SetRot(model, &rot);
	}
	byte vis = CGameModel_IsVisible(model);
	if (!vis) {
		if (self->time > self->emitT && 0.1f > self->emitT) {
			return;
		}
		self->emitT = 0.0f;
	}
	cMissileEntity *mis = (cMissileEntity *)self->entity;
	float size = mis->topRadius;
	float life = mis->lifeTime;
	/* the cloud grows over the first 0.9 s and shrinks in the last second of the missile */
	float c = 1.0f;
	if (FX_InRange(self->time, 0.0f, 0.9f, &c)) {
		c = (3.0f - c * 2.0f) * (c * c);
		size = size * (c * 0.7f + 0.3f);
	} else if (1.0f > life) {
		c = FXCM_Smooth(life);
		size = (c * 0.2f + 0.8f) * size;
	}
	self->particles.birthRate = 0;
	int n = CGameParticles_NumParticles(&self->particles);
	float psize = size * 0.85f;
	int k = 0;
	float a = FX_PI_4;
	do {
		float a10 = a * 10.0f;
		float b = 0.0f;
		do {
			float v = Fx_Rand(0.39999997615814209, 0.60000002384185791) * delta + self->rands[k];
			self->rands[k] = v;
			if (v > 2.0f) {
				self->rands[k] = 0.0f;
				f = self->rands[k];
			} else if (1.0f > v) {
				f = self->rands[k];
			} else {
				f = 2.0f - v;
			}
			up = (vec3_t){ 0.0f, 1.0f, 0.0f };
			float s = FXCM_Smooth(f);
			ang.x = s * 0.05f + a;
			ang.y = 0.0f;
			ang.z = b - s * 0.07f;
			Vec3_Rotate(&pos, &up, &ang);
			float shrink = 1.0f - s * 0.28f;
			float px = pos.x * size * 0.8f * shrink;
			float py = pos.y * size * 0.8f * shrink;
			float pz = pos.z * size * 0.8f * shrink;
			float r = b * 10.0f + a10;
			if (k & 1) {
				r = s * 0.2f + r;
			} else {
				r = r - s * 0.2f;
			}
			r = NormAng(r);
			float q = (float)(int)(c * 10.0f) / 10.0f;
			uint32_t gray = (uint32_t)(int)(q * 43.0f);
			int alpha = 0x96 - (int)((q - 1.0f) * 105.0f);
			if (alpha > 0xff) {
				alpha = 0xff;
			}
			uint32_t color = ((((uint32_t)alpha << 8 | gray) << 8 | gray) << 8) | gray;
			CGameParticles_SetParticle(&self->particles, n - 1 - k, px, py, pz, psize, color, 0x11, r);
			k++;
			b = b + FX_PI_4;
		} while (FX_2PI > b);
		a = a + FX_PI_4;
	} while (FX_2PI > a);
	if (!vis) {
		return;
	}
	if (!(life > 1.35f && self->emitT > 0.07f) && !(self->emitT >= self->time)) {
		return;
	}
	self->emitT = 0.0f;
	FXDebrisCloud_Puff(self, size, 2.6999998092651367, 1.5, 0.62831854820251465, -0.31415927410125732,
		0.19999992847442627, 1.6000000238418579, 0.9f, 1.0f, 0.10000002384185791, 1.1499999761581421, 0);
	FXDebrisCloud_Puff(self, size, 2.0, 2.5, 0.89759790897369385, -0.44879895448684692, 0.20000004768371582,
		1.3999999761581421, 1.15f, 1.2f, 0.04999995231628418, 1.25, 0x33);
}

void __thiscall cObjElementFXDebrisCloud_SetEntity(cObjElementFXDebrisCloud *self, cEntity *e) {
	self->entity = e;
}

void __thiscall cObjElementFXDebrisCloud_Warm(cObjElementFXDebrisCloud *self, cDebris *debris) {
	for (int i = 0; i < 4; i++) {
		self->base.vftable->Update(&self->base, debris, 0.0f);
	}
}

/* ---- the constructors and destructors ---- */

#define FXCM_TRAIL_CTOR 0x21ddb0
#define FXCM_TRAIL_DTOR 0x21de40
#define FXCM_NODE_CTOR 0x1c9610
#define FXCM_NODE_DTOR 0x0d1190		/* the same code as Nto1Node_dtor */

static inline void FXCM_SetVtbl(void *self, uint32_t vt) {
	*(void **)self = NULL;
	*(void **)self = Game_Ptr(vt);
}

Nto1Node *__thiscall FXCountermeasures_NodeCtor(Nto1Node *self) {
	self->target = NULL;
	self->data = NULL;
	self->next = self;
	self->prev = self;
	return self;
}

void *__thiscall cObjElementFXDebrisCloud_ctor(void *self) {
	cObjElementFXDebrisCloud *s = self;

	FXCM_SetVtbl(s, COBJELEMENTFXDEBRISCLOUD_VFTABLE);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	s->entity = NULL;
	s->emitT = 0.0f;
	s->time = 0.0f;
	return s;
}

void *__thiscall cObjElementFXEMI_ctor(void *self) {
	cObjElementFXEMI *s = self;

	FXCM_SetVtbl(s, COBJELEMENTFXEMI_VFTABLE);
	CGameParticles_ctor(&s->particles);
	crt_eh_vector_ctor(s->trails, sizeof(CGameTrail), FXEMI_TRAILS, Game_Ptr(FXCM_TRAIL_CTOR),
		Game_Ptr(FXCM_TRAIL_DTOR));
	crt_eh_vector_ctor(s->links, sizeof(Nto1Node), FXEMI_TRAILS, Game_Ptr(FXCM_NODE_CTOR), Game_Ptr(FXCM_NODE_DTOR));
	s->on = 0;
	s->entity = NULL;
	s->bc = 0;
	s->c0 = 0.6f;
	s->c4 = 0;
	s->d14 = 6;
	for (int i = 0; i < FXEMI_TRAILS; i++) {
		s->links[i].data = s;
	}
	return s;
}

void *__thiscall cObjElementFXHook_ctor(void *self) {
	cObjElementFXHook *s = self;

	FXCM_SetVtbl(s, COBJELEMENTFXHOOK_VFTABLE);
	CGameTrail_ctor(&s->trail[0]);
	CGameTrail_ctor(&s->trail[1]);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	s->emitter = NULL;
	s->target = NULL;
	s->numPoints = 3;
	s->flash = 0.0f;
	return s;
}

void *__thiscall cObjElementFXSonicExplosion_ctor(void *self) {
	cObjElementFXSonicExplosion *s = self;

	FXCM_SetVtbl(s, COBJELEMENTFXSONICEXPLOSION_VFTABLE);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->rings);
	s->on = 0;
	s->time = 0.0f;
	s->time2 = 0.0f;
	return s;
}

void __thiscall cObjElementFXDebrisCloud_dtor(void *self) {
	cObjElementFXDebrisCloud *s = self;

	*(void **)s = Game_Ptr(COBJELEMENTFXDEBRISCLOUD_VFTABLE);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXEMI_dtor(void *self) {
	cObjElementFXEMI *s = self;

	*(void **)s = Game_Ptr(COBJELEMENTFXEMI_VFTABLE);
	crt_eh_vector_dtor(s->links, sizeof(Nto1Node), FXEMI_TRAILS, Game_Ptr(FXCM_NODE_DTOR));
	crt_eh_vector_dtor(s->trails, sizeof(CGameTrail), FXEMI_TRAILS, Game_Ptr(FXCM_TRAIL_DTOR));
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXHook_dtor(void *self) {
	cObjElementFXHook *s = self;

	*(void **)s = Game_Ptr(COBJELEMENTFXHOOK_VFTABLE);
	CGameParticles_Release(&s->particles);
	CGameTrail_dtor(&s->trail[1]);
	CGameTrail_dtor(&s->trail[0]);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXSonicExplosion_dtor(void *self) {
	cObjElementFXSonicExplosion *s = self;

	*(void **)s = Game_Ptr(COBJELEMENTFXSONICEXPLOSION_VFTABLE);
	CGameParticles_Release(&s->rings);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

/* the deleting destructors (vtable slot 0, rva 0x1c99d0..0x1c9b3e) */
static inline cObjElementFX *FXCM_Delete(void *self, byte flags, uint32_t size) {
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

cObjElementFX *__thiscall cObjElementFXDebrisCloud_Delete(cObjElementFX *self, byte flags) {
	cObjElementFXDebrisCloud_dtor(self);
	return FXCM_Delete(self, flags, sizeof(cObjElementFXDebrisCloud));
}

cObjElementFX *__thiscall cObjElementFXEMI_Delete(cObjElementFX *self, byte flags) {
	cObjElementFXEMI_dtor(self);
	return FXCM_Delete(self, flags, sizeof(cObjElementFXEMI));
}

cObjElementFX *__thiscall cObjElementFXHook_Delete(cObjElementFX *self, byte flags) {
	cObjElementFXHook_dtor(self);
	return FXCM_Delete(self, flags, sizeof(cObjElementFXHook));
}

cObjElementFX *__thiscall cObjElementFXSonicExplosion_Delete(cObjElementFX *self, byte flags) {
	cObjElementFXSonicExplosion_dtor(self);
	return FXCM_Delete(self, flags, sizeof(cObjElementFXSonicExplosion));
}
