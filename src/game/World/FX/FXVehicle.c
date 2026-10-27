/*
 * World/FX/FXVehicle.cpp of the game: the effects of the vehicles, see include/scrap/fxvehicle.h.
 * Not here (too large): the Updates (vf01) of FiredEntity (rva 0x1eb8d0), Accelerator (0x1ed170), Damage
 * (0x1ed9c0), EMI (0x1f0aa0), Fire (0x1f1b90), Sputnik (0x1f3960), the ray setup of the Accelerator (0x1f48c0, not
 * called) and FX_ShipExplosion (0x1f4d50).
 */
#include "scrap/crt.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/fxvehicle.h"
#include "scrap/fxweapons.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/timer.h"
#include "scrap/world.h"

#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40

#define g_RandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */

/* the same helpers as the constructors of FXMisc.c (kept local, FXMisc.c has them static) */
static inline void FXV_SetVtbl(void *self, uint32_t vt) {
	*(void **)self = NULL;
	*(void **)self = Game_Ptr(vt);
}

static inline void FXV_TrailsCtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_ctor(t, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_CTOR), Game_Ptr(CGAMETRAIL_DTOR));
}

static inline void FXV_TrailsDtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_dtor(t, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_DTOR));
}

static inline void *FXV_Delete(void *self, uint32_t flags, uint32_t size) {
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

/* the node of name in model, CHECKed */
static int FXV_Node(CGameModel *model, const char *name) {
	int node = CGameModel_FindNodoR(model, name);
	CHECK(node > -1);
	return node;
}

/* Dummy01 of a loaded model, else the debris pointer itself stands for the node (as in the original) */
static int FXV_NodeOrDebris(cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	if (CGameModel_IsLoaded(model)) {
		return CGameModel_FindNodoR(model, "Dummy01");
	}
	return (int)(intptr_t)debris;
}

/* the model of the entity, CHECKed */
static CGameModel *FXV_Model(cEntity *e) {
	CGameModel *model = FX_Vtbl(e)->GetModel(e);
	CHECK(CGameModel_IsLoaded(model));
	return model;
}

/* the motor exhausts of the ship e that exist */
static void FXV_Exhausts(cEntity *e, int *num, FXVExhaust *ex) {
	*num = 0;
	for (int i = 0; i < FXV_MOTORS; i++) {
		if (FXVeh_MotorNode(FXVeh_Motor(e, i), 0) != -1) {
			ex[*num].motor = i;
			ex[*num].side = 0;
			(*num)++;
		}
		if (FXVeh_MotorNode(FXVeh_Motor(e, i), 1) != -1) {
			ex[*num].motor = i;
			ex[*num].side = 1;
			(*num)++;
		}
	}
}

/* ---- constructors ---- */

void *__thiscall cObjElementFXFiredDebris_ctor(void *self) {
	cObjElementFXFiredDebris *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXFIREDDEBRIS_VFTABLE);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	return s;
}

void *__thiscall cObjElementFXFiredEntity_ctor(void *self) {
	cObjElementFXFiredEntity *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXFIREDENTITY_VFTABLE);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	s->entity = NULL;
	return s;
}

void *__thiscall cObjElementFXSmokeDebris_ctor(void *self) {
	cObjElementFXSmokeDebris *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXSMOKEDEBRIS_VFTABLE);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	return s;
}

void *__thiscall cObjElementFXVehicleAccelerator_ctor(void *self) {
	cObjElementFXVehicleAccelerator *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXVEHICLEACCELERATOR_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXV_TrailsCtor(s->trails, FXVACCEL_TRAILS);
	s->on = 0;
	return s;
}

void *__thiscall cObjElementFXVehicleDamage_ctor(void *self) {
	cObjElementFXVehicleDamage *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXVEHICLEDAMAGE_VFTABLE);
	CGameParticles_ctor(&s->parts3);
	CGameParticles_ctor(&s->parts2);
	CGameParticles_ctor(&s->parts1);
	s->on = 0;
	s->entity = NULL;
	s->model = NULL;
	return s;
}

void *__thiscall cObjElementFXVehicleDeathTrail_ctor(void *self) {
	cObjElementFXDevastatorShoot *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXVEHICLEDEATHTRAIL_VFTABLE);
	CGameTrail_ctor(&s->trail);
	s->first = 1;
	s->on = 0;
	s->time = 0.0f;
	s->duration = 0.0f;
	return s;
}

void *__thiscall cObjElementFXVehicleEMI_ctor(void *self) {
	cObjElementFXVehicleEMI *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXVEHICLEEMI_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXV_TrailsCtor(s->trails, FXVEMI_TRAILS);
	s->on = 0;
	s->entity = NULL;
	s->fbc = 0;
	s->fb8 = 0;
	s->numTrails = FXVEMI_TRAILS;
	s->fc4 = 22.0f;
	s->fc8 = 0x1000;
	s->fcc = 0x40;
	return s;
}

void *__thiscall cObjElementFXVehicleFire_ctor(void *self) {
	cObjElementFXVehicleFire *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXVEHICLEFIRE_VFTABLE);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particlesOut);
	s->on = 0;
	s->entity = NULL;
	return s;
}

void *__thiscall cObjElementFXVehicleSpawn_ctor(void *self) {
	cObjElementFXVehicleSpawn *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXVEHICLESPAWN_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXV_TrailsCtor(s->trails, FXVSPAWN_TRAILS);
	s->on = 0;
	s->entity = NULL;
	s->time = 0.0f;
	s->stepTime = 0.0f;
	return s;
}

void *__thiscall cObjElementFXVehicleSputnik_ctor(void *self) {
	cObjElementFXVehicleSputnik *s = self;
	FXV_SetVtbl(s, COBJELEMENTFXVEHICLESPUTNIK_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXV_TrailsCtor(s->trails, FXVSPUTNIK_TRAILS);
	s->on = 0;
	s->entity = NULL;
	s->fb8 = 0;
	s->fbc = 0;
	s->fc0 = 1;
	s->fc4 = 0.8f;
	return s;
}

/* ---- destructors ---- */

void __thiscall cObjElementFXFiredDebris_dtor(void *self) {
	cObjElementFXFiredDebris *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXFIREDDEBRIS_VFTABLE);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXFiredEntity_dtor(void *self) {
	cObjElementFXFiredEntity *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXFIREDENTITY_VFTABLE);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXSmokeDebris_dtor(void *self) {
	cObjElementFXSmokeDebris *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXSMOKEDEBRIS_VFTABLE);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXVehicleAccelerator_dtor(void *self) {
	cObjElementFXVehicleAccelerator *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXVEHICLEACCELERATOR_VFTABLE);
	FXV_TrailsDtor(s->trails, FXVACCEL_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXVehicleDamage_dtor(void *self) {
	cObjElementFXVehicleDamage *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXVEHICLEDAMAGE_VFTABLE);
	CGameParticles_Release(&s->parts1);
	CGameParticles_Release(&s->parts2);
	CGameParticles_Release(&s->parts3);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXVehicleDeathTrail_dtor(void *self) {
	cObjElementFXDevastatorShoot *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXVEHICLEDEATHTRAIL_VFTABLE);
	CGameTrail_dtor(&s->trail);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXVehicleEMI_dtor(void *self) {
	cObjElementFXVehicleEMI *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXVEHICLEEMI_VFTABLE);
	FXV_TrailsDtor(s->trails, FXVEMI_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXVehicleFire_dtor(void *self) {
	cObjElementFXVehicleFire *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXVEHICLEFIRE_VFTABLE);
	s->entity = NULL;
	s->f168 = 0;
	CGameParticles_Release(&s->particlesOut);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXVehicleSpawn_dtor(void *self) {
	cObjElementFXVehicleSpawn *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXVEHICLESPAWN_VFTABLE);
	FXV_TrailsDtor(s->trails, FXVSPAWN_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXVehicleSputnik_dtor(void *self) {
	cObjElementFXVehicleSputnik *s = self;
	*(void **)s = Game_Ptr(COBJELEMENTFXVEHICLESPUTNIK_VFTABLE);
	FXV_TrailsDtor(s->trails, FXVSPUTNIK_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

/* ---- deleting destructors (vf00) ---- */

void *__thiscall cObjElementFXFiredDebris_Delete(void *self, uint32_t flags) {
	cObjElementFXFiredDebris_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXFiredDebris));
}

void *__thiscall cObjElementFXFiredEntity_Delete(void *self, uint32_t flags) {
	cObjElementFXFiredEntity_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXFiredEntity));
}

void *__thiscall cObjElementFXSmokeDebris_Delete(void *self, uint32_t flags) {
	cObjElementFXSmokeDebris_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXSmokeDebris));
}

void *__thiscall cObjElementFXVehicleAccelerator_Delete(void *self, uint32_t flags) {
	cObjElementFXVehicleAccelerator_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXVehicleAccelerator));
}

void *__thiscall cObjElementFXVehicleDamage_Delete(void *self, uint32_t flags) {
	cObjElementFXVehicleDamage_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXVehicleDamage));
}

void *__thiscall cObjElementFXVehicleDeathTrail_Delete(void *self, uint32_t flags) {
	cObjElementFXVehicleDeathTrail_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXDevastatorShoot));
}

void *__thiscall cObjElementFXVehicleEMI_Delete(void *self, uint32_t flags) {
	cObjElementFXVehicleEMI_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXVehicleEMI));
}

void *__thiscall cObjElementFXVehicleFire_Delete(void *self, uint32_t flags) {
	cObjElementFXVehicleFire_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXVehicleFire));
}

void *__thiscall cObjElementFXVehicleSpawn_Delete(void *self, uint32_t flags) {
	cObjElementFXVehicleSpawn_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXVehicleSpawn));
}

void *__thiscall cObjElementFXVehicleSputnik_Delete(void *self, uint32_t flags) {
	cObjElementFXVehicleSputnik_dtor(self);
	return FXV_Delete(self, flags, sizeof(cObjElementFXVehicleSputnik));
}

/* ---- Update (vf01) ---- */

/* rand() in [0, 1] times range plus add, all in double */
static inline float FXV_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * g_RandScale * range + add);
}

static inline uint32_t *FXV_Ramp(uint32_t rva) {
	return (uint32_t *)Game_Ptr(rva);
}

/* one of the two ramps of 9 colors at rva */
static inline uint32_t *FXV_RandRamp(uint32_t rva) {
	return FXV_Ramp(rva + (uint32_t)(crt_rand() & 1) * 0x24);
}

/* the emission box (+-box) around x y z and the velocities (+-vel) of the next particles */
static void FXV_Box(CGameParticles *p, float x, float y, float z, float box, float vel) {
	p->initPosMin.x = x - box;
	p->initPosMin.y = y - box;
	p->initPosMin.z = z - box;
	p->initPosMax.x = x + box;
	p->initPosMax.y = y + box;
	p->initPosMax.z = z + box;
	float v = 0.0f - vel;
	p->initVelMin.x = v;
	p->initVelMin.y = v;
	p->initVelMin.z = v;
	v = vel + 0.0f;
	p->initVelMax.x = v;
	p->initVelMax.y = v;
	p->initVelMax.z = v;
}

/* the number of emissions along a way n times the minimum distance, 1 at least */
static inline int FXV_Steps(float n) {
	int i = (int)n;
	return i > 0 ? i : 1;
}

static inline void FXV_SetSprite(CGameParticles *p, byte sprite) {
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
}

/* the fire sprites 0x11..0x12, else the sprites 0x21 (start) and 0x25 (end) */
static void FXV_FireSprites(CGameParticles *p) {
	if (crt_rand() & 1) {
		FXV_SetSprite(p, (byte)(crt_rand() % 2 + 0x11));
	} else {
		p->spriteIni = 0x21;
		p->spriteEnd = 0x25;
	}
}

#define FXV_RAMP_FIRED 0x431130		/* [2] */
#define FXV_RAMP_FIREDSMOKE 0x4310e8	/* [2] */
#define FXV_RAMP_SMOKE 0x43119c		/* [2] */
#define FXV_RAMP_SMOKE2 0x431178

void __thiscall cObjElementFXFiredDebris_Update(cObjElementFXFiredDebris *self, cDebris *debris, float dt) {
	CGameModel *model = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	vec3_t pos;

	cDebris_Move(debris, dt);
	self->time = cTimer_GetDelta(&g_pWorld->timer) + self->time;
	CHECK(CGameModel_IsLoaded(model));
	CGameModel_GetPos(model, &pos);
	if (self->first) {
		self->first = 0;
		self->last = pos;
	}
	p->birthRate = 0;
	CGameParticles_SetColorRampOn(p, 1);

	/* the glow at the debris fades in its last half second */
	float f = debris->speed - dt;
	f = f + f;
	int a = (int)((1.0f - Float_MinSS(1.0f, f)) * 255.0f);
	int a0 = a + 0xa4;
	if (a0 > 0xff) {
		a0 = 0xff;
	}
	CGameParticles_SetParticle(p, 0, pos.x, pos.y, pos.z, self->scale * 0.45f, (uint32_t)a0 << 24 | 0xffe632, 2,
		0.0f);
	int a1 = a + 0xdc;
	if (a1 > 0xff) {
		a1 = 0xff;
	}
	CGameParticles_SetParticle(p, 1, pos.x, pos.y, pos.z, self->scale * 2.7f, (uint32_t)a1 << 24 | 0xff780a, 5,
		0.0f);

	float dy = pos.y - self->last.y;
	float dx = pos.x - self->last.x;
	float dz = pos.z - self->last.z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (!CGameModel_IsVisible(model) || !(debris->speed > 0.4f) || !(d > 0.01f)) {
		return;
	}
	float minDist = self->minDist;
	if (!(minDist > 0.01f)) {
		return;
	}
	if (!(d > minDist) && !(self->time > self->interval)) {
		return;
	}

	/* the fire and smoke along the way since the last emission */
	float n = d / minDist;
	self->time = 0.0f;
	self->last = pos;
	float inv = 1.0f / n;
	float life = Float_MinSS(1.0f, minDist / d * 2.5f);
	dx = dx * inv;
	dy = dy * inv;
	dz = dz * inv;
	float s1 = self->scale * FXV_Rand(0.09999999403953552, 0.30000001192092896);
	float s2 = self->scale * FXV_Rand(0.5499999523162842, 1.25);
	float x = pos.x;
	float y = pos.y;
	float z = pos.z;
	for (int i = 0; i < FXV_Steps(n); i++) {
		float r, t;

		FXV_Box(p, x, y, z, self->spread / 300.0f, self->spread / 100.0f);
		p->sizeIni = FXV_Rand(0.5, -0.25) * s1 + s1;
		p->sizeEnd = FXV_Rand(0.5, -0.25) * s2 + s2;
		t = FXV_Rand(0.30000001192092896, 0.6499999761581421) * life;
		p->lifeTime = Float_MinSS(debris->speed, t);
		p->rotIni = FXV_Rand(6.2831854820251465, 0.0);
		p->rotVar = 0.0f;
		p->rotEnd = FXV_Rand(1.5707963705062866, -0.7853981852531433) + p->rotIni;
		p->sizeLife = FXV_Rand(0.10000002384185791, 0.8999999761581421);
		if (crt_rand() & 1) {
			FXV_SetSprite(p, (byte)(crt_rand() % 2 + 0x11));
		} else {
			FXV_SetSprite(p, (byte)(crt_rand() % 4 + 0x21));
		}
		CGameParticles_SetColorRamp(p, FXV_RandRamp(FXV_RAMP_FIRED));
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);

		FXV_Box(p, x, y, z, self->spread / 300.0f, self->spread / 100.0f);
		r = FXV_Rand(0.5, -0.25);
		p->sizeIni = FXV_Rand(0.09999996423721313, 0.800000011920929) * (r * s1) + s1;
		r = FXV_Rand(0.5, -0.25);
		p->sizeEnd = FXV_Rand(0.09999996423721313, 0.800000011920929) * (r * s2) + s2;
		t = FXV_Rand(0.30000001192092896, 0.550000011920929) * life;
		p->lifeTime = Float_MinSS(debris->speed, t);
		p->rotIni = FXV_Rand(6.2831854820251465, 0.0);
		p->rotVar = 0.0f;
		p->rotEnd = FXV_Rand(1.5707963705062866, -0.7853981852531433) + p->rotIni;
		p->sizeLife = FXV_Rand(0.10000002384185791, 0.8999999761581421);
		FXV_FireSprites(p);
		CGameParticles_SetColorRamp(p, FXV_RandRamp(FXV_RAMP_FIRED));
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);

		FXV_Box(p, x, y, z, self->spread / 300.0f, self->spread / 100.0f);
		r = FXV_Rand(0.5, -0.25);
		p->sizeIni = FXV_Rand(0.09999996423721313, 0.6000000238418579) * (r * s1 + s1);
		r = FXV_Rand(0.5, -0.25);
		p->sizeEnd = FXV_Rand(0.09999996423721313, 0.6000000238418579) * (r * s2 + s2);
		t = FXV_Rand(0.04999998211860657, 0.30000001192092896) * life;
		p->lifeTime = Float_MinSS(debris->speed, t);
		p->rotIni = FXV_Rand(6.2831854820251465, 0.0);
		p->rotVar = 0.0f;
		p->rotEnd = FXV_Rand(3.1415927410125732, -1.5707963705062866) + p->rotIni;
		p->sizeLife = FXV_Rand(0.09999999403953552, 0.3499999940395355);
		FXV_FireSprites(p);
		CGameParticles_SetColorRamp(p, FXV_RandRamp(FXV_RAMP_FIREDSMOKE));
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, self->time * 0.25f);

		x = x - dx;
		y = y - dy;
		z = z - dz;
	}
}

void __thiscall cObjElementFXSmokeDebris_Update(cObjElementFXSmokeDebris *self, cDebris *debris, float dt) {
	CGameModel *model = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	vec3_t pos;

	cDebris_Move(debris, dt);
	self->time = cTimer_GetDelta(&g_pWorld->timer) + self->time;
	if (1.0f > debris->speed - dt) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	CGameModel_GetPos(model, &pos);
	if (self->interval > self->time) {
		return;
	}
	self->time = 0.0f;
	if (self->first) {
		self->first = 0;
		self->last = pos;
	}
	if (CGameModel_IsVisible(model)) {
		p->birthRate = 0;
		CGameParticles_SetColorRampOn(p, 1);
		float dy = pos.y - self->last.y;
		float dx = pos.x - self->last.x;
		float dz = pos.z - self->last.z;
		float n = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) / self->minDist;
		float inv = 1.0f / n;
		dx = dx * inv;
		dy = dy * inv;
		dz = dz * inv;
		float s1 = FXV_Rand(0.09999999403953552, 0.4000000059604645) * self->scale;
		float s2 = FXV_Rand(0.050000011920928955, 0.949999988079071) * self->scale;
		float x = pos.x;
		float y = pos.y;
		float z = pos.z;
		for (int i = 0; i < FXV_Steps(n); i++) {
			float r;

			/* the dark smoke while the debris has more than 1.15 to live */
			if (debris->speed - dt > 1.15f) {
				FXV_Box(p, x, y, z, self->spread, self->spread);
				p->sizeIni = FXV_Rand(0.5, -0.25) * s1 + s1;
				p->sizeEnd = FXV_Rand(0.5, -0.25) * s2 + s2;
				p->lifeTime = FXV_Rand(0.20000004768371582, 0.6499999761581421);
				r = FXV_Rand(6.2831854820251465, 0.0);
				p->rotIni = r;
				p->rotEnd = r;
				p->rotVar = FXV_Rand(0.5235987901687622, -0.2617993950843811);
				p->sizeLife = FXV_Rand(0.10000002384185791, 0.8999999761581421);
				FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x11));
				CGameParticles_SetColorRamp(p, FXV_RandRamp(FXV_RAMP_SMOKE));
				CGameParticles_Apply(p);
				CGameParticles_EmitDt(p, 1, 1, 0.04f);
			}
			FXV_Box(p, x, y, z, self->spread, self->spread);
			r = FXV_Rand(0.5, -0.25);
			p->sizeIni = FXV_Rand(0.050000011920928955, 0.25) * (r * s1 + s1);
			r = FXV_Rand(0.5, -0.25);
			p->sizeEnd = FXV_Rand(0.050000011920928955, 0.25) * (r * s2 + s2);
			p->lifeTime = Float_MinSS(debris->speed, FXV_Rand(0.050000011920928955, 0.25));
			r = FXV_Rand(6.2831854820251465, 0.0);
			p->rotIni = r;
			p->rotEnd = r;
			p->rotVar = FXV_Rand(0.5235987901687622, -0.2617993950843811);
			p->sizeLife = FXV_Rand(0.09999999403953552, 0.3499999940395355);
			FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
			CGameParticles_SetColorRamp(p, FXV_Ramp(FXV_RAMP_SMOKE2));
			CGameParticles_Apply(p);
			CGameParticles_EmitDt(p, 1, 1, 0.0f);
			x = x - dx;
			y = y - dy;
			z = z - dz;
		}
	}
	self->last = pos;
}

/* the entity of an effect let go when it is deleted or off */
static inline void FXV_CheckEntity(cEntity **entity) {
	cEntity *e = *entity;
	if (e && (Entity_Removed(e) || !e->flag_4c)) {
		*entity = NULL;
	}
}

/* v normalized into out (v itself for the length 0) */
static void FXV_Normalize(vec3_t *out, float x, float y, float z) {
	float len = (float)__builtin_sqrt((double)(y * y + x * x + z * z));
	if (len == 0.0f) {
		out->x = x;
		out->y = y;
		out->z = z;
		return;
	}
	float inv = 1.0f / len;
	out->x = x * inv;
	out->y = y * inv;
	out->z = z * inv;
}

/* the rays grow out of the vertices of the ship while it appears */
void __thiscall cObjElementFXVehicleSpawn_Update(cObjElementFXVehicleSpawn *self, cDebris *debris, float dt) {
	(void)dt;
	FXV_CheckEntity(&self->entity);
	cEntity *e = self->entity;
	if (!e || !e->flag_4c) {
		return;
	}
	CGameModel *em = FX_Vtbl(e)->GetModel(e);
	CHECK(CGameModel_IsLoaded(em));
	vec3_t pos, rot;
	CGameModel_GetPos(em, &pos);
	CGameModel_GetRot(em, &rot);
	CGameModel *model = Debris_Model(debris);
	CGameModel_SetPos(model, &pos);
	CGameModel_SetRot(model, &rot);
	CGameModel_SetSortZ(model, 0.0f, 0.0f);

	float d = cTimer_GetDelta(&g_pWorld->timer);
	float t = d + self->time;
	self->time = t;
	self->stepTime = d + self->stepTime;
	float u = t - 0.4f;
	float c;
	if (0.0f > u) {
		c = 0.0f;
	} else {
		c = Float_MinSS(1.0f, u);
	}
	float s = (3.0f - (c + c)) * (c * c);
	int a = (int)((3.0f - (s + s)) * (s * s) * 255.0f);
	FX_Vtbl(self->entity)->SetFX(self->entity, 2);
	FX_Vtbl(self->entity)->SetFXParam(self->entity, 0, 1.0f - c);
	for (int i = 0; i < FXVSPAWN_TRAILS; i++) {
		CGameTrail_Reset(&self->trails[i]);
		CGameTrail_SetTransp(&self->trails[i], (byte)a);
	}

	/* 15 rays per trail */
	int inTrail = 0;
	int trail = 0;
	int i;
	for (i = 0; i < self->numRays; i++) {
		if (self->rayLen[i] > 0.25f && trail < FXVSPAWN_TRAILS) {
			vec3_t *b = &self->rayBase[i];
			CGameModel_Vertices(em, b, NULL, NULL, 1, self->rayVertex[i], NULL, 0xffffffffu, NULL);
			FXV_Normalize(&self->rayDir[i], b->x - pos.x, b->y - pos.y, b->z - pos.z);
			*b = pos;
			const vec3_t *dir = &self->rayDir[i];
			float len = self->rayLen[i];
			vec3_t end;
			end.x = len * dir->x + pos.x;
			end.y = dir->y * len + pos.y;
			end.z = dir->z * len + pos.z;
			float w = len * 10.0f;
			CGameTrail_Add(&self->trails[trail], *b, w * 0.8f, 0x2019aaf5);
			CGameTrail_Add(&self->trails[trail], end, w, 0xff0a2de1);
			inTrail++;
			if (inTrail >= 15) {
				inTrail = 0;
				trail++;
			}
		}
		float speed = self->raySpeed[i];
		self->rayLen[i] = speed * self->stepTime + self->rayLen[i];
		self->raySpeed[i] = self->rayAcc[i] * self->stepTime + speed;
		float max = self->rayMax[i];
		if (self->rayLen[i] > max && 3.4028235e38f > max) {
			self->rayLen[i] = max;
			self->raySpeed[i] = 0.0f;
			self->rayAcc[i] = 0.0f;
		}
	}

	/* one new ray per frame from a random vertex */
	int n = self->numRays;
	if (n < FXVSPAWN_RAYS) {
		vec3_t *b = &self->rayBase[n];
		CGameModel_Vertices(em, b, NULL, NULL, 1, -1, &self->rayVertex[n], 0xffffffffu, NULL);
		n = self->numRays;
		b = &self->rayBase[n];
		FXV_Normalize(&self->rayDir[n], b->x - pos.x, b->y - pos.y, b->z - pos.z);
		self->rayLen[self->numRays] = FXV_Rand(0.5, 0.30000001192092896);
		self->raySpeed[self->numRays] = FXV_Rand(230.0, 185.0);
		double r = (double)(crt_rand() & 0x7fff) * g_RandScale;
		self->rayAcc[self->numRays] = -(float)(r + r + 1.5);
		b = &self->rayBase[self->numRays];
		float dx = b->x - pos.x;
		float dy = b->y - pos.y;
		float dz = b->z - pos.z;
		float m = FXV_Rand(0.6000000238418579, 1.600000023841858);
		self->rayMax[self->numRays] = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * m;
		/* the loop index, the number of rays when the loop ran to the end */
		self->rayBase[i] = pos;
		self->numRays++;
	}
	self->stepTime = 0.0f;
}

/* ---- IsAlive (vf02) ---- */

byte __thiscall cObjElementFXFiredEntity_IsAlive(cObjElementFXFiredEntity *self, cDebris *debris, cDebrisSys *sys) {
	byte r = cDebris_Update(debris, sys);
	if (!r) {
		return r;
	}
	/* the smoke of a gone entity ends within 1.5 */
	cEntity *e = self->entity;
	if (!e || Entity_Removed(e) || !e->flag_4c) {
		if (debris->speed > 1.5f) {
			debris->speed = 1.5f;
		}
	}
	return 1;
}

byte __thiscall cObjElementFXVehicleDamage_IsAlive(cObjElementFXVehicleDamage *self, cDebris *debris,
	cDebrisSys *sys) {
	cEntity *e;
	if (self->active && (e = self->entity) != NULL && e->flag_4c) {
		self->life = 0;
		self->lifePct = 100;
		int maxLife = g_StdMaxLife;
		if (e->vftable->IsType(e, TYEN_WITH_LIFE)) {
			self->life = Vehicle_Life(self->entity);
		}
		if (self->entity->vftable->IsType(self->entity, TYEN_CAR)) {
			maxLife = FXVeh_MaxLife(self->entity);
		}
		int life = self->life;
		int pct = life * 100 / maxLife;
		self->lifePct = pct;
		/* the effect lives on while the life is low */
		if (life <= 0 || pct >= g_FXSetOnFireLifePercent) {
			cVehicleEffects_ClearDamage(FXVeh_Effects(self->entity));
			self->active = 0;
		} else {
			debris->speed = 1.5f;
		}
	}
	if (cDebris_Update(debris, sys)) {
		return 1;
	}
	if (self->entity && FXVeh_EmbeddedFX(self->entity)) {
		cEmbeddedFXVehicle_SetAttached(FXVeh_EmbeddedFX(self->entity), NULL);
	}
	self->entity = NULL;
	return 0;
}

byte __thiscall cObjElementFXVehicleEMI_IsAlive(cObjElementFXVehicleEMI *self, cDebris *debris, cDebrisSys *sys) {
	if (!self->entity) {
		return 0;
	}
	/* the debris lives as long as the interference */
	*(uint32_t *)&debris->speed = FXVeh_EMITime(self->entity);
	if (cDebris_Update(debris, sys)) {
		return 1;
	}
	cEntity *e = self->entity;
	if (e && e->flag_4c) {
		FX_Vtbl(e)->SetFX(e, 0);
	}
	return 0;
}

byte __thiscall cObjElementFXVehicleFire_IsAlive(cObjElementFXVehicleFire *self, cDebris *debris, cDebrisSys *sys) {
	cEntity *e = self->entity;
	if (!e) {
		return 0;
	}
	float t = FXVeh_BurnTime(e);
	if (!(0.0f >= t)) {
		debris->speed = t;
		if (cDebris_Update(debris, sys)) {
			return 1;
		}
		e = self->entity;
		if (!e) {
			return 0;
		}
	}
	if (e->flag_4c) {
		FX_Vtbl(e)->SetFX(e, 0);
	}
	return 0;
}

byte __thiscall cObjElementFXVehicleSputnik_IsAlive(cObjElementFXVehicleSputnik *self, cDebris *debris,
	cDebrisSys *sys) {
	if (!self->entity) {
		return 0;
	}
	byte changed = self->entity->flag_4c != self->active;
	if (cDebris_Update(debris, sys) && !changed) {
		return 1;
	}
	FX_Vtbl(self->entity)->SetFX(self->entity, 0);
	FX_Vtbl(self->entity)->SetFXParam(self->entity, 0, 0.0f);
	return 0;
}

/* ---- Start (vf03) ---- */

void __thiscall cObjElementFXFiredDebris_Start(cObjElementFXFiredDebris *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	int node = CGameModel_FindNodoR(model, "Dummy01");
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CHECK(self->particles.SistPart);
	self->first = 1;
	self->interval = 0.1f;
	self->time = 0.0f;
	self->minDist = 100.0f;
	self->last.x = 0.0f;
	self->last.y = 0.0f;
	self->last.z = 0.0f;
}

void __thiscall cObjElementFXFiredEntity_Start(cObjElementFXFiredEntity *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CGameParticles_Attach(&self->particles, model, FXV_Node(model, "Dummy01"));
	self->time = 0.0f;
	self->b08 = 0;
}

void __thiscall cObjElementFXSmokeDebris_Start(cObjElementFXSmokeDebris *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	int node = CGameModel_FindNodoR(model, "Dummy01");
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CHECK(self->particles.SistPart);
	self->first = 1;
	self->interval = 0.03f;
	self->minDist = 50.0f;
	self->time = 0.0f;
	self->last.x = 0.0f;
	self->last.y = 0.0f;
	self->last.z = 0.0f;
}

void __thiscall cObjElementFXVehicleAccelerator_Start(cObjElementFXVehicleAccelerator *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXV_Node(model, "Dummy01"));
	CHECK(self->particles.SistPart);
	for (int i = 0; i < FXVACCEL_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		self->fade[i] = 0.0f;
		self->delay[i] = 0.0f;
		CGameTrail_SetMaxTrails(t, 0xe);
		CGameTrail_SetSmooth(t, 0);
		CGameTrail_SetTexture(t, (i & 1) ? 8 : 9);
		CGameTrail_SetPremul(t, 1);
		CGameTrail_SetUVScale(t, 0.25f, 1.0f);
	}
}

void __thiscall cObjElementFXVehicleDamage_Start(cObjElementFXVehicleDamage *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	self->model = model;
	CGameParticles_Attach(&self->parts1, model, FXV_Node(model, "Dummy01"));
	CGameParticles_Attach(&self->parts3, model, FXV_Node(model, "Dummy03"));
	CGameParticles_Attach(&self->parts2, model, FXV_Node(model, "Dummy02"));
	self->numExhausts = 0;
	self->active = 1;
	self->f180 = 0;
	self->f244 = 0;
	self->b240 = 1;
	self->b18 = 0;
	self->f17c = 0;
}

void __thiscall cObjElementFXVehicleDeathTrail_Start(cObjElementFXDevastatorShoot *self, cDebris *debris) {
	(void)debris;
	CGameTrail_SetMaxTrails(&self->trail, 0x1e);
}

/* the trails and their marks back to the start */
static void FXV_EMIClear(cObjElementFXVehicleEMI *self, int i) {
	self->f160[i] = -1;
	self->f4260[i] = 0;
	self->f4220[i] = 0;
	self->f41e0[i] = 0;
	for (int j = 0; j < self->fc8 >> 4; j++) {
		self->marks[i][j] = 0;
	}
}

void __thiscall cObjElementFXVehicleEMI_Start(cObjElementFXVehicleEMI *self, cDebris *debris) {
	int node = FXV_NodeOrDebris(debris);
	if (node != -1) {
		CGameParticles_Attach(&self->particles, Debris_Model(debris), node);
	}
	for (int i = 0; i < self->numTrails; i++) {
		CGameTrail *t = &self->trails[i];
		self->f1a0[i] = 0;
		CGameTrail_SetMaxTrails(t, 0x12);
		CGameTrail_SetSmooth(t, 0);
		CGameTrail_SetColor(t, 0xc0c0ff);
		CGameTrail_SetTexture(t, 0);
		CGameTrail_SetScale(t, 1.0f, 0.6f);
		CGameTrail_SetFade(t, 1);
		FXV_EMIClear(self, i);
	}
}

void __thiscall cObjElementFXVehicleFire_Start(cObjElementFXVehicleFire *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	int node = CGameModel_FindNodoR(model, "Dummy01");
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CHECK(self->particles.SistPart);
	node = CGameModel_FindNodoR(model, "Dummy02");
	if (node != -1) {
		CGameParticles_Attach(&self->particlesOut, model, node);
	}
	CHECK(self->particlesOut.SistPart);
	self->f16c = 0;
	self->numExhausts = 0;
}

void __thiscall cObjElementFXVehicleSpawn_Start(cObjElementFXVehicleSpawn *self, cDebris *debris) {
	int node = FXV_NodeOrDebris(debris);
	if (node != -1) {
		CGameParticles_Attach(&self->particles, Debris_Model(debris), node);
	}
	for (int i = 0; i < FXVSPAWN_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 0x1f);
		CGameTrail_SetTexture(t, 0);
		CGameTrail_SetType(t, 3);
	}
	self->numRays = 0;
}

void __thiscall cObjElementFXVehicleSputnik_Start(cObjElementFXVehicleSputnik *self, cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXV_Node(model, "Dummy01"));
	CHECK(self->particles.SistPart);
	for (int i = 0; i < FXVSPUTNIK_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetTexture(t, i < 2 ? 0 : 0xb);
		CGameTrail_SetMaxTrails(t, 0x1f);
		CGameTrail_SetType(t, 3);
		CGameTrail_SetUVScale(t, 4.0f, 1.0f);
	}
}

/* ---- Reset (vf04) ---- */

void __thiscall cObjElementFXFiredDebris_Reset(cObjElementFXFiredDebris *self, cDebris *debris) {
	(void)debris;
	self->first = 1;
	self->interval = 0.09f;
	self->time = 0.0f;
	self->minDist = 100.0f;
	self->last.x = 0.0f;
	self->last.y = 0.0f;
	self->last.z = 0.0f;
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXFiredEntity_Reset(cObjElementFXFiredEntity *self, cDebris *debris) {
	(void)debris;
	self->entity = NULL;
	self->time = 0.0f;
	self->b08 = 0;
}

void __thiscall cObjElementFXSmokeDebris_Reset(cObjElementFXSmokeDebris *self, cDebris *debris) {
	(void)debris;
	self->first = 1;
	self->time = 0.0f;
	self->last.x = 0.0f;
	self->last.y = 0.0f;
	self->last.z = 0.0f;
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXVehicleAccelerator_Reset(cObjElementFXVehicleAccelerator *self, cDebris *debris) {
	(void)debris;
	for (int i = 0; i < FXVACCEL_TRAILS; i++) {
		self->fade[i] = 0.0f;
		self->delay[i] = 0.0f;
		CGameTrail_Clear(&self->trails[i]);
	}
}

void __thiscall cObjElementFXVehicleDamage_Reset(cObjElementFXVehicleDamage *self, cDebris *debris) {
	(void)debris;
	self->entity = NULL;
	self->numExhausts = 0;
	self->active = 1;
	self->f180 = 0;
	self->f244 = 0;
	self->b240 = 1;
	self->b18 = 0;
	self->f17c = 0;
}

void __thiscall cObjElementFXVehicleEMI_Reset(cObjElementFXVehicleEMI *self, cDebris *debris) {
	(void)debris;
	self->fbc = 0;
	self->fb8 = 0;
	for (int i = 0; i < self->numTrails; i++) {
		self->f1a0[i] = 0;
		CGameTrail_Clear(&self->trails[i]);
		FXV_EMIClear(self, i);
	}
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXVehicleFire_Reset(cObjElementFXVehicleFire *self, cDebris *debris) {
	(void)debris;
	self->entity = NULL;
	self->f168 = 0;
	self->f16c = 0;
	self->numExhausts = 0;
}

void __thiscall cObjElementFXVehicleSpawn_Reset(cObjElementFXVehicleSpawn *self, cDebris *debris) {
	(void)debris;
	self->time = 0.0f;
	self->stepTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
	for (int i = 0; i < FXVSPAWN_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	self->numRays = 0;
}

void __thiscall cObjElementFXVehicleSputnik_Reset(cObjElementFXVehicleSputnik *self, cDebris *debris) {
	(void)debris;
	self->fb8 = 0;
	self->fbc = 0;
	CGameParticles_KillAll(&self->particles);
	for (int i = 0; i < FXVSPUTNIK_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	for (int i = 0; i < FXVSPUTNIK_RANDS; i++) {
		self->faec[i] = -1.0f;
		self->rands[i] = (float)((double)(crt_rand() & 0x7fff) * g_RandScale * (double)0.4f + (double)0.15f);
	}
}

/* ---- the setters ---- */

void __thiscall cObjElementFXFiredEntity_SetEntity(cObjElementFXFiredEntity *self, cEntity *e) {
	vec3_t tmp;
	self->entity = e;
	CGameModel *model = FXV_Model(e);
	self->pos = *CGameModel_GetPos(model, &tmp);
	self->radius = CGameModel_Radius(model);
}

void __thiscall cObjElementFXVehicleDamage_SetEntity(cObjElementFXVehicleDamage *self, cEntity *e) {
	vec3_t tmp;
	self->entity = e;
	CGameModel *model = FXV_Model(e);
	self->pos = *CGameModel_GetPos(model, &tmp);
	FXV_Exhausts(self->entity, &self->numExhausts, self->exhausts);
}

void __thiscall cObjElementFXVehicleEMI_SetEntity(cObjElementFXVehicleEMI *self, cEntity *e) {
	self->entity = e;
	FXV_Model(e);
	FXV_Exhausts(self->entity, &self->numExhausts, self->exhausts);
}

void __thiscall cObjElementFXVehicleFire_SetEntity(cObjElementFXVehicleFire *self, cEntity *e) {
	self->entity = e;
	FXV_Model(e);
	FXV_Exhausts(self->entity, &self->numExhausts, self->exhausts);
}

void __thiscall cObjElementFXVehicleSputnik_SetEntity(cObjElementFXVehicleSputnik *self, cEntity *e) {
	vec3_t pos, rot;
	self->entity = e;
	CGameModel *model = FXV_Model(e);
	/* both results are not used */
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	self->active = self->entity->flag_4c;
	FXV_Exhausts(self->entity, &self->numExhausts, self->exhausts);
	self->sputnikNode = -1;
	HashTable *t = &g_pWorld->entities;
	HashNode *node = HashTable_Find(t, "Sputnik", HashTable_Hash(t, "Sputnik"));
	if (node && node->value) {
		cEntity *sp = node->value;
		self->sputnikNode = CGameModel_FindNodoR(FXV_Model(sp), "Bip Spootnick MASTER");
	}
}

void __thiscall cObjElementFXVehicleSputnik_SetC0(cObjElementFXVehicleSputnik *self, int v) {
	self->fc0 = v;
}

void __thiscall cObjElementFXVehicleSputnik_SetC4(cObjElementFXVehicleSputnik *self, float v) {
	self->fc4 = v;
}

void __thiscall cObjElementFXVehicleSpawn_SetEntity(cObjElementFXVehicleSpawn *self, cEntity *e) {
	self->entity = e;
	FXV_Model(e);
}

void __thiscall cObjElementFXVehicleAccelerator_SetOwner(cObjElementFXVehicleAccelerator *self, cEntity *e) {
	self->owner = e;
	CHECK(e && FX_Vtbl(e)->GetModel(e) && CGameModel_IsLoaded(FX_Vtbl(self->owner)->GetModel(self->owner)));
}

void __thiscall cObjElementFXVehicleAccelerator_SetAccelerator(cObjElementFXVehicleAccelerator *self, void *a) {
	self->accelerator = a;
	CHECK(a);
}

/* the damage effect drawn once with the ship it follows: only its smoke and sparks, the fire (parts1) hidden */
void __fastcall FXVehicle_RenderAttached(void *element) {
	if (g_SplitScreenGame) {
		return;
	}
	cObjElementFXVehicleDamage *self = element;
	if (!self->entity || !self->model || !self->parts3.SistPart || !self->parts2.SistPart
		|| !self->parts1.SistPart) {
		return;
	}
	CGameParticles_SetVisible(&self->parts3, 1);
	CGameParticles_SetVisible(&self->parts2, 1);
	CGameParticles_SetVisible(&self->parts1, 0);
	CHECK(CGameModel_IsLoaded(self->model));
	CGameModel_RenderNow(self->model, 0);
	CGameParticles_SetVisible(&self->parts3, 0);
	CGameParticles_SetVisible(&self->parts2, 0);
	CGameParticles_SetVisible(&self->parts1, 1);
}

/* ---- World/FX/FXScene.cpp ---- */

void __thiscall cObjElementFXSceneDecontamination_SetNames(void *self, const char *a, const char *b) {
	Str_Copy((char *)self + 0xbc, a, 0x20);
	Str_Copy((char *)self + 0xdc, b, 0x20);
}

void __thiscall cObjElementFXSceneDecontamination_SetFC(void *self, float v) {
	*(float *)((byte *)self + 0xfc) = v;
}
