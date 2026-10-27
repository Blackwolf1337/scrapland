/*
 * FXCountermeasures.cpp of the game: the effects of the hook laser and the countermeasures, see
 * include/scrap/fxcountermeasures.h.
 */
#include "scrap/crt.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
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
	self->bc = 0;
	self->c0 = 0;
	CGameParticles_KillAll(&self->particles);
	cObjElementFXDebrisCloud_Shuffle(self);
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
	s->bc = 0;
	s->c0 = 0;
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
