/*
 * World/FX/FXVehicle.cpp of the game: the effects of the vehicles, see include/scrap/fxvehicle.h.
 */
#include "scrap/gvar.h"
#include "scrap/crt.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/fx.h"
#include "scrap/fxvehicle.h"
#include "scrap/fxweapons.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define COBJELEMENTFXVEHICLEDAMAGE_VFTABLE (&cObjElementFXVehicleDamage_vftable)
#define COBJELEMENTFXVEHICLEFIRE_VFTABLE (&cObjElementFXVehicleFire_vftable)
#define COBJELEMENTFXFIREDENTITY_VFTABLE (&cObjElementFXFiredEntity_vftable)
#define COBJELEMENTFXVEHICLEEMI_VFTABLE (&cObjElementFXVehicleEMI_vftable)
#define COBJELEMENTFXFIREDDEBRIS_VFTABLE (&cObjElementFXFiredDebris_vftable)
#define COBJELEMENTFXSMOKEDEBRIS_VFTABLE (&cObjElementFXSmokeDebris_vftable)
#define COBJELEMENTFXVEHICLEDEATHTRAIL_VFTABLE (&cObjElementFXVehicleDeathTrail_vftable)
#define COBJELEMENTFXVEHICLESPUTNIK_VFTABLE (&cObjElementFXVehicleSputnik_vftable)
#define COBJELEMENTFXVEHICLESPAWN_VFTABLE (&cObjElementFXVehicleSpawn_vftable)
#define COBJELEMENTFXVEHICLEACCELERATOR_VFTABLE (&cObjElementFXVehicleAccelerator_vftable)
#define g_FXFiredDebrisFile (GVar_FXFiredDebrisFile.value.s)
#define g_FXShipExplosionFile (GVar_FXShipExplosionFile.value.s)
static const cObjElementFXVtbl cObjElementFXVehicleSputnik_vftable;
static const cObjElementFXVtbl cObjElementFXVehicleSpawn_vftable;
static const cObjElementFXVtbl cObjElementFXVehicleFire_vftable;
static const cObjElementFXVtbl cObjElementFXVehicleEMI_vftable;
static const cObjElementFXVtbl cObjElementFXVehicleDeathTrail_vftable;
static const cObjElementFXVtbl cObjElementFXVehicleDamage_vftable;
static const cObjElementFXVtbl cObjElementFXVehicleAccelerator_vftable;
static const cObjElementFXVtbl cObjElementFXSmokeDebris_vftable;
static const cObjElementFXVtbl cObjElementFXFiredEntity_vftable;
static const cObjElementFXVtbl cObjElementFXFiredDebris_vftable;
static cGVar GVar_FXShipExplosionFile;
static cGVar GVar_FXFiredDebrisFile;

/* the ship of an effect */
#define FXV_Veh(e) ((const cVehicleEntity *)(e))

/* the node of name in model, CHECKed */
static int FXV_Node(CGameModel *model, const char *name) {
	int node = CGameModel_FindRenderNode(model, name);
	CHECK(node > -1);
	return node;
}

/* Dummy01 of a loaded model, else the debris pointer itself stands for the node (as in the original) */
static int FXV_NodeOrDebris(cDebris *debris) {
	CGameModel *model = Debris_Model(debris);
	if (CGameModel_IsLoaded(model)) {
		return CGameModel_FindRenderNode(model, "Dummy01");
	}
	return (int)(intptr_t)debris;
}

/* the model of the entity, CHECKed */
static CGameModel *FXV_Model(cEntity *e) {
	CGameModel *model = Obj_GetModel(e);
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

void *cObjElementFXFiredDebris_ctor(void *self) {
	cObjElementFXFiredDebris *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXFIREDDEBRIS_VFTABLE);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	return s;
}

void *cObjElementFXFiredEntity_ctor(void *self) {
	cObjElementFXFiredEntity *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXFIREDENTITY_VFTABLE);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	s->entity = NULL;
	return s;
}

void *cObjElementFXSmokeDebris_ctor(void *self) {
	cObjElementFXSmokeDebris *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXSMOKEDEBRIS_VFTABLE);
	CGameParticles_ctor(&s->particles);
	s->on = 0;
	return s;
}

void *cObjElementFXVehicleAccelerator_ctor(void *self) {
	cObjElementFXVehicleAccelerator *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEACCELERATOR_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXObj_TrailsCtor(s->trails, FXVACCEL_TRAILS);
	s->on = 0;
	return s;
}

void *cObjElementFXVehicleDamage_ctor(void *self) {
	cObjElementFXVehicleDamage *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEDAMAGE_VFTABLE);
	CGameParticles_ctor(&s->parts3);
	CGameParticles_ctor(&s->parts2);
	CGameParticles_ctor(&s->parts1);
	s->on = 0;
	s->entity = NULL;
	s->model = NULL;
	return s;
}

cObjElementFXVehicleDeathTrail *cObjElementFXVehicleDeathTrail_ctor(cObjElementFXVehicleDeathTrail *self) {
	cObjElementFXDevastatorShoot *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEDEATHTRAIL_VFTABLE);
	CGameTrail_ctor(&s->trail);
	s->first = 1;
	s->on = 0;
	s->time = 0.0f;
	s->duration = 0.0f;
	return s;
}

void *cObjElementFXVehicleEMI_ctor(void *self) {
	cObjElementFXVehicleEMI *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEEMI_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXObj_TrailsCtor(s->trails, FXVEMI_TRAILS);
	s->on = 0;
	s->entity = NULL;
	s->time = 0.0f;
	s->stepTime = 0.0f;
	s->numTrails = FXVEMI_TRAILS;
	s->offset = 22.0f;
	s->numVerts = 0x1000;
	s->maxPts = 0x40;
	return s;
}

void *cObjElementFXVehicleFire_ctor(void *self) {
	cObjElementFXVehicleFire *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEFIRE_VFTABLE);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particlesOut);
	s->on = 0;
	s->entity = NULL;
	return s;
}

void *cObjElementFXVehicleSpawn_ctor(void *self) {
	cObjElementFXVehicleSpawn *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLESPAWN_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXObj_TrailsCtor(s->trails, FXVSPAWN_TRAILS);
	s->on = 0;
	s->entity = NULL;
	s->time = 0.0f;
	s->stepTime = 0.0f;
	return s;
}

void *cObjElementFXVehicleSputnik_ctor(void *self) {
	cObjElementFXVehicleSputnik *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLESPUTNIK_VFTABLE);
	CGameParticles_ctor(&s->particles);
	FXObj_TrailsCtor(s->trails, FXVSPUTNIK_TRAILS);
	s->on = 0;
	s->entity = NULL;
	s->time = 0.0f;
	s->time2 = 0.0f;
	s->phase = 1;
	s->duration = 0.8f;
	return s;
}

/* ---- destructors ---- */

static void cObjElementFXFiredDebris_dtor(void *self) {
	cObjElementFXFiredDebris *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXFIREDDEBRIS_VFTABLE);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXFiredEntity_dtor(void *self) {
	cObjElementFXFiredEntity *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXFIREDENTITY_VFTABLE);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXSmokeDebris_dtor(void *self) {
	cObjElementFXSmokeDebris *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXSMOKEDEBRIS_VFTABLE);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXVehicleAccelerator_dtor(void *self) {
	cObjElementFXVehicleAccelerator *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEACCELERATOR_VFTABLE);
	FXObj_TrailsDtor(s->trails, FXVACCEL_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXVehicleDamage_dtor(void *self) {
	cObjElementFXVehicleDamage *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEDAMAGE_VFTABLE);
	CGameParticles_Release(&s->parts1);
	CGameParticles_Release(&s->parts2);
	CGameParticles_Release(&s->parts3);
	cObjElement_dtor(s);
}

static void cObjElementFXVehicleDeathTrail_dtor(cObjElementFXVehicleDeathTrail *self) {
	cObjElementFXDevastatorShoot *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEDEATHTRAIL_VFTABLE);
	CGameTrail_dtor(&s->trail);
	cObjElement_dtor(s);
}

static void cObjElementFXVehicleEMI_dtor(void *self) {
	cObjElementFXVehicleEMI *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEEMI_VFTABLE);
	FXObj_TrailsDtor(s->trails, FXVEMI_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXVehicleFire_dtor(void *self) {
	cObjElementFXVehicleFire *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLEFIRE_VFTABLE);
	s->entity = NULL;
	s->time = 0.0f;
	CGameParticles_Release(&s->particlesOut);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXVehicleSpawn_dtor(void *self) {
	cObjElementFXVehicleSpawn *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLESPAWN_VFTABLE);
	FXObj_TrailsDtor(s->trails, FXVSPAWN_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXVehicleSputnik_dtor(void *self) {
	cObjElementFXVehicleSputnik *s = self;
	FXObj_SetVtbl(s, COBJELEMENTFXVEHICLESPUTNIK_VFTABLE);
	FXObj_TrailsDtor(s->trails, FXVSPUTNIK_TRAILS);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

/* ---- deleting destructors (vf00) ---- */

static void cObjElementFXFiredDebris_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXFiredDebris_dtor(self);
	operator_delete(self);
}

static void cObjElementFXFiredEntity_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXFiredEntity_dtor(self);
	operator_delete(self);
}

static void cObjElementFXSmokeDebris_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXSmokeDebris_dtor(self);
	operator_delete(self);
}

static void cObjElementFXVehicleAccelerator_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXVehicleAccelerator_dtor(self);
	operator_delete(self);
}

static void cObjElementFXVehicleDamage_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXVehicleDamage_dtor(self);
	operator_delete(self);
}

static void cObjElementFXVehicleDeathTrail_Free(cObjElementFX *base) {
	cObjElementFXVehicleDeathTrail *self = (cObjElementFXVehicleDeathTrail *)base;
	cObjElementFXVehicleDeathTrail_dtor(self);
	operator_delete(self);
}

static void cObjElementFXVehicleEMI_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXVehicleEMI_dtor(self);
	operator_delete(self);
}

static void cObjElementFXVehicleFire_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXVehicleFire_dtor(self);
	operator_delete(self);
}

static void cObjElementFXVehicleSpawn_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXVehicleSpawn_dtor(self);
	operator_delete(self);
}

static void cObjElementFXVehicleSputnik_Free(cObjElementFX *base) {
	void *self = (void *)base;
	cObjElementFXVehicleSputnik_dtor(self);
	operator_delete(self);
}

/* ---- Update (vf01) ---- */

/* rand() in [0, 1] times range plus add, all in double */
static inline float FXV_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * range + add);
}

static inline uint32_t *FXV_Ramp(uint32_t *ramp) {
	return ramp;
}

/* one of the two ramps of 9 colors at ramps */
static inline uint32_t *FXV_RandRamp(uint32_t *ramps) {
	return FXV_Ramp(ramps + 9 * (crt_rand() & 1));
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


static uint32_t cObjElementFXFiredDebris_UpdateRamps[18] = {
	0xff565656, 0xe2fff696, 0xc4e68211, 0xc4ab5e0d, 0xc49c3a08, 0xc46a2617, 0xe640190e, 0xff000000, 0xff000000,
	0xff565656, 0xaaffec82, 0xa0ef8228, 0xaa9a260f, 0xbb7c1f0c, 0xcc5e170a, 0xe1400f05, 0xff000000, 0xff000000,
};

static uint32_t cObjElementFXFiredDebris_UpdateRamps2[18] = {
	0xff000000, 0x5a0f0f0f, 0x40141414, 0x401e1e1e, 0x5a191919, 0x800f0f0f, 0xc0050505, 0xff000000, 0xff000000,
	0xff000000, 0x5a141414, 0x5a282828, 0x46323232, 0x503c3c3c, 0x802d2d2d, 0xc00f0f0f, 0xff000000, 0xff000000,
};

/* Update */
static void cObjElementFXFiredDebris_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXFiredDebris *self = (cObjElementFXFiredDebris *)base;
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
	float f = debris->life - dt;
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
	if (!CGameModel_IsVisible(model) || !(debris->life > 0.4f) || !(d > 0.01f)) {
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
		p->lifeTime = Float_MinSS(debris->life, t);
		p->rotIni = FXV_Rand(6.2831854820251465, 0.0);
		p->rotVar = 0.0f;
		p->rotEnd = FXV_Rand(1.5707963705062866, -0.7853981852531433) + p->rotIni;
		p->sizeLife = FXV_Rand(0.10000002384185791, 0.8999999761581421);
		if (crt_rand() & 1) {
			FXV_SetSprite(p, (byte)(crt_rand() % 2 + 0x11));
		} else {
			FXV_SetSprite(p, (byte)(crt_rand() % 4 + 0x21));
		}
		CGameParticles_SetColorRamp(p, FXV_RandRamp(cObjElementFXFiredDebris_UpdateRamps2));
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);

		FXV_Box(p, x, y, z, self->spread / 300.0f, self->spread / 100.0f);
		r = FXV_Rand(0.5, -0.25);
		p->sizeIni = FXV_Rand(0.09999996423721313, 0.800000011920929) * (r * s1) + s1;
		r = FXV_Rand(0.5, -0.25);
		p->sizeEnd = FXV_Rand(0.09999996423721313, 0.800000011920929) * (r * s2) + s2;
		t = FXV_Rand(0.30000001192092896, 0.550000011920929) * life;
		p->lifeTime = Float_MinSS(debris->life, t);
		p->rotIni = FXV_Rand(6.2831854820251465, 0.0);
		p->rotVar = 0.0f;
		p->rotEnd = FXV_Rand(1.5707963705062866, -0.7853981852531433) + p->rotIni;
		p->sizeLife = FXV_Rand(0.10000002384185791, 0.8999999761581421);
		FXV_FireSprites(p);
		CGameParticles_SetColorRamp(p, FXV_RandRamp(cObjElementFXFiredDebris_UpdateRamps2));
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);

		FXV_Box(p, x, y, z, self->spread / 300.0f, self->spread / 100.0f);
		r = FXV_Rand(0.5, -0.25);
		p->sizeIni = FXV_Rand(0.09999996423721313, 0.6000000238418579) * (r * s1 + s1);
		r = FXV_Rand(0.5, -0.25);
		p->sizeEnd = FXV_Rand(0.09999996423721313, 0.6000000238418579) * (r * s2 + s2);
		t = FXV_Rand(0.04999998211860657, 0.30000001192092896) * life;
		p->lifeTime = Float_MinSS(debris->life, t);
		p->rotIni = FXV_Rand(6.2831854820251465, 0.0);
		p->rotVar = 0.0f;
		p->rotEnd = FXV_Rand(3.1415927410125732, -1.5707963705062866) + p->rotIni;
		p->sizeLife = FXV_Rand(0.09999999403953552, 0.3499999940395355);
		FXV_FireSprites(p);
		CGameParticles_SetColorRamp(p, FXV_RandRamp(cObjElementFXFiredDebris_UpdateRamps));
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, self->time * 0.25f);

		x = x - dx;
		y = y - dy;
		z = z - dz;
	}
}

static uint32_t cObjElementFXSmokeDebris_UpdateRamp[9] = { 0xffffffff, 0xc4ffc680, 0xc4ffc680, 0xc4dea460, 0xc4c48040, 0xc4804020, 0xe6402010, 0xff000000, 0xff000000 };

static uint32_t cObjElementFXSmokeDebris_UpdateTable[19] = {
	0xff000000, 0x80202020, 0x80414141, 0x8a4b4b4b, 0xa4464646, 0xc02d2d2d, 0xe4181818, 0xff000000, 0xff000000,
	0xff000000, 0x80373737, 0x804b4b4b, 0x80555555, 0xa4505050, 0xc0373737, 0xe4202020, 0xff000000, 0xff000000,
};

static void cObjElementFXSmokeDebris_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXSmokeDebris *self = (cObjElementFXSmokeDebris *)base;
	CGameModel *model = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	vec3_t pos;

	cDebris_Move(debris, dt);
	self->time = cTimer_GetDelta(&g_pWorld->timer) + self->time;
	if (1.0f > debris->life - dt) {
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
			if (debris->life - dt > 1.15f) {
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
				CGameParticles_SetColorRamp(p, FXV_RandRamp(cObjElementFXSmokeDebris_UpdateTable));
				CGameParticles_Apply(p);
				CGameParticles_EmitDt(p, 1, 1, 0.04f);
			}
			FXV_Box(p, x, y, z, self->spread, self->spread);
			r = FXV_Rand(0.5, -0.25);
			p->sizeIni = FXV_Rand(0.050000011920928955, 0.25) * (r * s1 + s1);
			r = FXV_Rand(0.5, -0.25);
			p->sizeEnd = FXV_Rand(0.050000011920928955, 0.25) * (r * s2 + s2);
			p->lifeTime = Float_MinSS(debris->life, FXV_Rand(0.050000011920928955, 0.25));
			r = FXV_Rand(6.2831854820251465, 0.0);
			p->rotIni = r;
			p->rotEnd = r;
			p->rotVar = FXV_Rand(0.5235987901687622, -0.2617993950843811);
			p->sizeLife = FXV_Rand(0.09999999403953552, 0.3499999940395355);
			FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
			CGameParticles_SetColorRamp(p, FXV_Ramp(cObjElementFXSmokeDebris_UpdateRamp));
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
	if (e && (Entity_Removed(e) || !e->active)) {
		*entity = NULL;
	}
}

/* the rays grow out of the vertices of the ship while it appears */
static void cObjElementFXVehicleSpawn_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXVehicleSpawn *self = (cObjElementFXVehicleSpawn *)base;
	(void)dt;
	FXV_CheckEntity(&self->entity);
	cEntity *e = self->entity;
	if (!e || !e->active) {
		return;
	}
	CGameModel *em = Obj_GetModel(e);
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
	Obj_SetEffect(self->entity, MODEL_EFFECT_FADE_LIT);
	Obj_SetEffectParam(self->entity, 0, 1.0f - c);
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
			FX_Normalize(&self->rayDir[i], b->x - pos.x, b->y - pos.y, b->z - pos.z);
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
		FX_Normalize(&self->rayDir[n], b->x - pos.x, b->y - pos.y, b->z - pos.z);
		self->rayLen[self->numRays] = FXV_Rand(0.5, 0.30000001192092896);
		self->raySpeed[self->numRays] = FXV_Rand(230.0, 185.0);
		double r = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
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

static cGVar GVar_FXFiredEntitySmokeStep;

/* ---- FiredEntity ---- */

#define g_FXFiredEntitySmokeStep (GVar_FXFiredEntitySmokeStep.value.f)

/* (0, 0, z) turned by rot */
static vec3_t *FXV_TurnZ(vec3_t *out, float z, const vec3_t *rot) {
	vec3_t v;
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = z;
	return Vec3_Rotate(out, &v, rot);
}

/* the smoke and fire behind a burning entity, one emission per step of the way since the last one */
static uint32_t cObjElementFXFiredEntity_UpdateRamps[18] = {
	0xfffff696, 0xa4fff08c, 0xafe6b411, 0xb9ab720d, 0xc89c4408, 0xd76a2603, 0xe6401400, 0xff000000, 0xff000000,
	0xffffe65a, 0xa4ffe150, 0xafef8228, 0xb99a440f, 0xc87c1f0c, 0xd75e170a, 0xe1400f05, 0xff000000, 0xff000000,
};

static uint32_t cObjElementFXFiredEntity_UpdateTable[74] = {
	0xff000000, 0x40141414, 0x40141414, 0x40141414, 0x60141414, 0x800a0a0a, 0xc0050505, 0xff000000, 0xff000000,
	0xff000000, 0x40191919, 0x40191919, 0x40191919, 0x60191919, 0x800d0d0d, 0xc0060606, 0xff000000, 0xff000000,
	0xff000000, 0x401e1e1e, 0x401e1e1e, 0x401e1e1e, 0x601e1e1e, 0x800f0f0f, 0xc0070707, 0xff000000, 0xff000000,
	0xff000000, 0x40232323, 0x40232323, 0x40232323, 0x60232323, 0x80111111, 0xc0080808, 0xff000000, 0xff000000,
	0xff000000, 0x40282828, 0x40282828, 0x40282828, 0x60282828, 0x80141414, 0xc00a0a0a, 0xff000000, 0xff000000, 0,
	0xff47e2e2, 0x403bb9ff, 0x103ea4ff, 0x104296ff, 0x204787ff, 0x804a7eff, 0xc04165db, 0xff000000, 0xff000000,
	0xff8080ff, 0xa48080ff, 0x808080ff, 0x808080ff, 0x808080ff, 0xa48080ff, 0xc08080ff, 0xff8080ff, 0xff8080ff,
	0xff4040ff, 0xa44040ff, 0x804040ff, 0x804040ff, 0x804040ff, 0xa44040ff, 0xc04040ff, 0xff4040ff, 0xff4040ff,
};

static void cObjElementFXFiredEntity_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXFiredEntity *self = (cObjElementFXFiredEntity *)base;
	(void)dt;
	FXV_CheckEntity(&self->entity);
	float d = cTimer_GetDelta(&g_pWorld->timer);
	cEntity *e = self->entity;
	if (!e || !e->active || !CGameModel_IsLoaded(Obj_GetModel(e))
		|| !CGameModel_IsVisible(Obj_GetModel(self->entity))) {
		self->posValid = 0;
		return;
	}
	CGameModel *em = Obj_GetModel(self->entity);
	CGameModel *model = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	vec3_t pos, rot, dv, n, dir, o;

	CHECK(CGameModel_IsLoaded(em));
	CGameModel_GetPos(em, &pos);
	CGameModel_GetRot(em, &rot);
	CGameModel_SetPos(model, &pos);
	CGameModel_SetRot(model, &rot);
	CGameModel_SetSortZ(model, -1024.0f, 1.0f);
	float size = self->radius * FXV_Rand(0.5999999046325684, 1.2000000476837158);
	float scale = FXV_Rand(0.10000002384185791, 0.699999988079071);
	if (size > 350.0f) {
		scale = (size - 350.0f) / 900.0f + scale;
	}
	if (!self->posValid) {
		self->pos = pos;
	}
	float time = self->time + d;
	self->posValid = 1;
	self->time = time;
	dv.x = self->pos.x - pos.x;
	dv.y = self->pos.y - pos.y;
	dv.z = self->pos.z - pos.z;
	float d2 = dv.y * dv.y + dv.x * dv.x + dv.z * dv.z;
	float step = g_FXFiredEntitySmokeStep;
	if (step * step > d2 && 0.035f >= time) {
		return;
	}
	float dist = (float)__builtin_sqrt((double)d2);
	Vec3_NormalizeTo(&n, &dv);
	/* standing still: straight back from the entity */
	if (time > 0.035f && 5.0f > dist) {
		FXV_TurnZ(&dir, 1.0f, &rot);
	} else {
		dir = n;
	}
	byte still = 0;
	if (self->time > 0.035f) {
		still = 1;
		dist = g_FXFiredEntitySmokeStep;
	}
	self->time = 0.0f;
	p->birthRate = 0;
	CGameParticles_SetColorRampOn(p, 1);

	float k = 0.0f;
	do {
		float kd = k * dist;
		vec3_t start;
		start.x = dir.x * kd + pos.x;
		start.y = dir.y * kd + pos.y;
		start.z = dir.z * kd + pos.z;

		/* the smoke */
		for (float j = k; g_FXFiredEntitySmokeStep / dist > j;
			j = g_FXFiredEntitySmokeStep * 0.33f / dist + j) {
			vec3_t rd, at;
			float sj = (j + k) * dist;
			at.x = dir.x * sj + pos.x;
			at.y = dir.y * sj + pos.y;
			at.z = dir.z * sj + pos.z;
			FX_RandomDir(&rd);
			double r = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
			if (still) {
				float sc = (float)(r * 4.0 + 1.0);
				float rx = rd.x * sc;
				float ry = rd.y * sc;
				float rz = rd.z * sc;
				float lo = size * -0.5f;
				FXV_TurnZ(&o, (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * (double)(size * 0.5f - lo)
					+ (double)lo), &rot);
				p->initPosMin.x = pos.x + o.x + rx;
				p->initPosMin.y = o.y + pos.y + ry;
				p->initPosMin.z = o.z + pos.z + rz;
				p->initPosMax = p->initPosMin;
				float v = FXV_Rand(520.0, 440.0);
				FXV_TurnZ(&o, 1.0f, &rot);
				p->initVelMin.x = v * o.x;
				p->initVelMin.y = o.y * v;
				p->initVelMin.z = o.z * v;
				p->initVelMax = p->initVelMin;
				float a = FXV_Rand(0.1499999761581421, 0.30000001192092896);
				p->initAccMin.x = a * -p->initVelMin.x;
				p->initAccMin.y = a * -p->initVelMin.y;
				p->initAccMin.z = a * -p->initVelMin.z;
			} else {
				float sc = (float)(r * 15.0 + 10.0);
				float rx = sc * rd.x;
				float ry = sc * rd.y;
				float rz = sc * rd.z;
				FXV_TurnZ(&o, size * -0.25f, &rot);
				p->initPosMin.x = o.x + at.x + rx;
				p->initPosMin.y = o.y + at.y + ry;
				p->initPosMin.z = o.z + at.z + rz;
				p->initPosMax = p->initPosMin;
				p->initVelMin.x = 0.0f;
				p->initVelMin.y = 0.0f;
				p->initVelMin.z = 0.0f;
				p->initVelMax = p->initVelMin;
				p->initAccMin.x = 0.0f;
				p->initAccMin.y = 0.0f;
				p->initAccMin.z = 0.0f;
			}
			p->initAccMax = p->initAccMin;
			p->sizeIni = FXV_Rand(50.0, 150.0) * scale;
			p->sizeEnd = FXV_Rand(50.0, 500.0) * scale;
			p->lifeTime = FXV_Rand(0.050000011920928955, 0.25);
			float ri = FXV_Rand(6.2831854820251465, 0.0);
			p->rotIni = ri;
			p->rotEnd = ri;
			p->rotVar = FXV_Rand(1.5707963705062866, -0.7853981852531433);
			p->sizeLife = FXV_Rand(0.19999998807907104, 0.25);
			switch (crt_rand() % 3) {
			case 0:
				FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x19));
				break;
			case 1:
				FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
				break;
			case 2:
				FXV_SetSprite(p, 0x37);
				break;
			}
			CGameParticles_SetColorRamp(p, FXV_RandRamp(cObjElementFXFiredEntity_UpdateRamps));
			CGameParticles_Apply(p);
			CGameParticles_EmitN(p, 1, 1);
		}

		/* the fire */
		vec3_t rd;
		FXV_TurnZ(&o, size, &rot);
		p->initPosMin.x = start.x + o.x;
		p->initPosMin.y = o.y + start.y;
		p->initPosMin.z = o.z + start.z;
		p->initPosMax = p->initPosMin;
		FX_RandomDir(&rd);
		float v = FXV_Rand(25.0, 65.0);
		p->initVelMin.x = v * rd.x;
		p->initVelMin.y = v * rd.y;
		p->initVelMin.z = v * rd.z;
		v = FXV_Rand(40.0, 225.0);
		p->initVelMax.x = v * rd.x;
		p->initVelMax.y = v * rd.y;
		p->initVelMax.z = v * rd.z;
		if (still) {
			v = FXV_Rand(290.0, 150.0);
			FXV_TurnZ(&o, 1.0f, &rot);
			p->initVelMin.x = v * o.x + p->initVelMin.x;
			p->initVelMin.y = p->initVelMin.y + o.y * v;
			p->initVelMin.z = p->initVelMin.z + o.z * v;
			v = FXV_Rand(290.0, 150.0);
			FXV_TurnZ(&o, 1.0f, &rot);
			p->initVelMax.x = o.x * v + p->initVelMax.x;
			p->initVelMax.y = o.y * v + p->initVelMax.y;
			p->initVelMax.z = o.z * v + p->initVelMax.z;
		}
		p->initAccMin.x = 0.0f;
		p->initAccMin.y = 350.0f;
		p->initAccMin.z = 0.0f;
		p->initAccMax.x = 0.0f;
		p->initAccMax.y = 850.0f;
		p->initAccMax.z = 0.0f;
		p->sizeIni = FXV_Rand(50.0, 700.0) * scale;
		p->sizeEnd = FXV_Rand(100.0, 1250.0) * scale;
		p->lifeTime = FXV_Rand(0.10000002384185791, 1.149999976158142);
		float ri = FXV_Rand(6.2831854820251465, 0.0);
		p->rotIni = ri;
		p->rotEnd = ri;
		p->sizeLife = 1.0f;
		p->rotVar = FXV_Rand(0.7853981852531433, -0.39269909262657166);
		if (crt_rand() % 5 == 0) {
			FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
		} else {
			FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x11));
		}
		CGameParticles_SetColorRamp(p, FXV_Ramp(cObjElementFXFiredEntity_UpdateTable + 9 * (crt_rand() % 5)));
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);
		k = k + g_FXFiredEntitySmokeStep / dist;
	} while (1.0f > k);
	self->pos = pos;
}

/* the model of exhaust ex of the ship e: the motor holds one per side */
static inline CGameModel *FXV_ExhaustModel(cEntity *e, const FXVExhaust *ex) {
	return &FXVeh_Motor(e, ex->motor)->model[ex->side];
}

/* ---- EMI ---- */

/* the marks of the vertices of ray i cleared */
static void FXV_EMIClearMarks(cObjElementFXVehicleEMI *self, int i) {
	for (int j = 0; j < self->numVerts >> 4; j++) {
		self->marks[i][j] = 0;
	}
}

/* the trails and their marks back to the start */
static void FXV_EMIClear(cObjElementFXVehicleEMI *self, int i) {
	self->part[i] = -1;
	self->count[i] = 0;
	self->tail[i] = 0;
	self->head[i] = 0;
	FXV_EMIClearMarks(self, i);
}

/* point idx of the flat array of all rays (the original reads the one before a ray too) */
static inline vec3_t *FXV_EMIPt(cObjElementFXVehicleEMI *self, int idx) {
	return &self->pts[0][0] + idx;
}

static inline int *FXV_EMIVidx(cObjElementFXVehicleEMI *self, int idx) {
	return &self->vidx[0][0] + idx;
}

static inline uint16_t *FXV_EMIMark(cObjElementFXVehicleEMI *self, int i, int vi) {
	return (uint16_t *)((byte *)self->marks[i] + (vi >> 4) * 2);
}

/* the rays jump from vertex to vertex, each step to the nearest free one more than 16 away from the last point */
static void cObjElementFXVehicleEMI_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXVehicleEMI *self = (cObjElementFXVehicleEMI *)base;
	cEntity *e = self->entity;
	if (e && (Entity_Removed(e) || !e->active)) {
		Obj_SetEffect(e, MODEL_EFFECT_NONE);
		self->entity = NULL;
	}
	e = self->entity;
	if (!e || !e->active) {
		return;
	}
	CGameModel *model = Obj_GetModel(e);
	CGameModel *dm = Debris_Model(debris);
	CGameParticles *ps = &self->particles;
	vec3_t pos, rot, tmp;
	CHECK(CGameModel_IsLoaded(model));
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	CGameModel_SetPos(dm, &pos);
	CGameModel_SetRot(dm, &rot);
	CGameModel_SetSortZ(dm, -36.0f, 0.0f);
	float d = cTimer_GetDelta(&g_pWorld->timer);
	self->time = self->time + d;
	self->stepTime = self->stepTime + d;
	Obj_SetEffect(self->entity, MODEL_EFFECT_ELECTRIC);
	float v = self->time;
	if (v > 0.5f) {
		v = debris->life - dt;
		if (v > 0.5f) {
			v = 1.0f;
		} else {
			v = v + v;
		}
	} else {
		v = v + v;
	}
	Obj_SetEffectParam(self->entity, 0, v);

	if (!FXV_Veh(self->entity)->wl.obj.cam) {
		if (!CGameTrail_IsRender(&self->trails[0]) && !CGameTrail_IsRender(&self->trails[1])) {
			return;
		}
		for (int i = 0; i < self->numTrails; i++) {
			CGameTrail_Clear(&self->trails[i]);
		}
		CGameParticles_KillAll(ps);
		return;
	}
	if (!CGameModel_IsVisible(model)) {
		for (int i = 0; i < self->numTrails; i++) {
			CGameTrail_Clear(&self->trails[i]);
		}
		CGameParticles_KillAll(ps);
		return;
	}

	if (self->time > 0.05f) {
		int partIdx = 1;
		for (int i = 0; i < self->numTrails; i++, partIdx += 3) {
			CGameTrail *tr = &self->trails[i];
			if (!(self->rayTime[i] > 0.0f)) {
				if (!(debris->life >= 0.6f)) {
					continue;
				}
				/* a new ray from a random vertex of the ship, later also of a motor */
				vec3_t vtx, dv, r, n;
				int vi;
				self->part[i] = -1;
				self->rayTime[i] = FXV_Rand(0.19999998807907104, 0.800000011920929);
				if (i > 8 && self->numExhausts > 0) {
					int k = crt_rand() % self->numExhausts;
					self->part[i] = k;
					model = FXV_ExhaustModel(self->entity, &self->exhausts[k]);
					CHECK(CGameModel_IsLoaded(model));
					pos = *CGameModel_GetPos(model, &tmp);
					rot = *CGameModel_GetRot(model, &tmp);
				}
				CGameModel_Vertices(model, &vtx, NULL, NULL, 1, -1, &vi, 0xffffffffu, NULL);
				dv.x = vtx.x - pos.x;
				dv.y = vtx.y - pos.y;
				dv.z = vtx.z - pos.z;
				Vec3_RotateInv(&r, &dv, &rot);
				dv.x = vtx.x - pos.x;
				dv.y = vtx.y - pos.y;
				dv.z = vtx.z - pos.z;
				Vec3_NormalizeTo(&n, &dv);
				float o = self->offset;
				self->pts[i][0].x = r.x + n.x * o;
				self->pts[i][0].y = r.y + n.y * o;
				self->pts[i][0].z = r.z + n.z * o;
				self->vidx[i][0] = vi;
				self->count[i] = 0;
				self->tail[i] = 0;
				self->head[i] = 0;
				FXV_EMIClearMarks(self, i);
				continue;
			}

			float t = self->rayTime[i] - d;
			self->rayTime[i] = t;
			if (t > 0.0f && self->count[i] > 0) {
				int k = self->part[i];
				if (k != -1) {
					model = FXV_ExhaustModel(self->entity, &self->exhausts[k]);
					CHECK(CGameModel_IsLoaded(model));
					pos = *CGameModel_GetPos(model, &tmp);
					rot = *CGameModel_GetRot(model, &tmp);
				}
				int a = 0xff - (int)(Float_MinSS(1.0f, self->rayTime[i] * 4.0f) * 255.0f);
				CGameTrail_Reset(tr);
				CGameTrail_SetTransp(tr, (byte)a);

				/* the last (up to 6) points of the ring as the control points */
				vec3_t ctrl[GAMETRAIL_MAX_BEZIER], coefs[GAMETRAIL_MAX_BEZIER];
				int n = self->count[i] < 6 ? self->count[i] : 6;
				int j = self->tail[i] - n;
				if (j < 0) {
					j += self->maxPts;
				}
				for (int m = 0; m < n; m++) {
					vec3_t o;
					Vec3_Rotate(&o, FXV_EMIPt(self, i * FXVEMI_POINTS + j), &rot);
					j++;
					ctrl[m].x = pos.x + o.x;
					ctrl[m].y = o.y + pos.y;
					ctrl[m].z = o.z + pos.z;
					if (j >= self->maxPts) {
						j = 0;
					}
				}
				CGameTrail_BezierCoefs(tr, n, ctrl, coefs);
				int nm = CGameTrail_GetMaxTrails(tr);
				if (nm - 1 > 0) {
					float last = (float)(nm - 2);
					for (int m = 0; m < nm - 1; m++) {
						vec3_t p, rd;
						CGameTrail_BezierPoint(tr, n, coefs, &p, (float)m / last);
						float w = FXV_Rand(12.0, 24.0);
						float sc = FXV_Rand(1.5, 0.5);
						FX_RandomDir(&rd);
						p.x = p.x + sc * rd.x;
						p.z = p.z + rd.z * sc;
						p.y = p.y + rd.y * sc;
						CGameTrail_Add(tr, p, w, 0x088080ff);
					}
				}
				/* the glow at the head of the ray */
				vec3_t q = ctrl[n - 1];
				int a1 = a + 0xe6;
				if (a1 > 0xff) {
					a1 = 0xff;
				}
				CGameParticles_SetParticle(ps, partIdx - 1, q.x, q.y, q.z, 800.0f, (uint32_t)a1 << 24 | 0x2020ff, 5,
					0.0f);
				a1 = a + 0x40;
				if (a1 > 0xff) {
					a1 = 0xff;
				}
				CGameParticles_SetParticle(ps, partIdx, q.x, q.y, q.z, 150.0f, (uint32_t)a1 << 24 | 0x4040ff, 1, 0.0f);
				a1 = a + 0x10;
				if (a1 > 0xff) {
					a1 = 0xff;
				}
				CGameParticles_SetParticle(ps, partIdx + 1, q.x, q.y, q.z, 64.0f, (uint32_t)a1 << 24 | 0x8080ff, 1,
					0.0f);
			}
			if (!(self->rayTime[i] > 0.0f)) {
				CGameTrail_Clear(tr);
				CGameParticles_KillRange(ps, partIdx - 1, partIdx + 1);
			}
			if (!(self->rayTime[i] > 0.0f) || !(self->stepTime > 0.02f)) {
				continue;
			}

			/* the step: the ring moves on, the vertex that drops out is free again */
			if (self->count[i] < self->maxPts) {
				self->count[i]++;
			} else {
				int vi = *FXV_EMIVidx(self, i * FXVEMI_POINTS + self->head[i]);
				if (vi >= 0 && vi < self->numVerts) {
					*FXV_EMIMark(self, i, vi) &= (uint16_t)~(1u << (vi & 0xf));
				}
				int h = self->head[i] + 1;
				self->head[i] = h < self->maxPts ? h : 0;
			}
			int tl = self->tail[i] + 1;
			self->tail[i] = tl < self->maxPts ? tl : 0;
			int best = -1;
			vec3_t prev = *FXV_EMIPt(self, i * FXVEMI_POINTS + self->tail[i] - 1);
			vec3_t verts[FXVEMI_POINTS], c, sn;
			int vids[FXVEMI_POINTS];
			CGameModel_Vertices(model, verts, NULL, NULL, FXVEMI_POINTS, -1, vids, 0xffffffffu, NULL);
			float bestDist = 3.4028235e38f;
			Float_SinCos(-rot.x, &sn.x, &c.x);
			Float_SinCos(-rot.y, &sn.y, &c.y);
			Float_SinCos(-rot.z, &sn.z, &c.z);
			for (int m = 0; m < FXVEMI_POINTS; m++) {
				int vi = vids[m];
				if (vi > self->numVerts || (*FXV_EMIMark(self, i, vi) & (1 << (vi & 0xf)))) {
					continue;
				}
				vec3_t dv, r, n;
				dv.x = verts[m].x - pos.x;
				dv.y = verts[m].y - pos.y;
				dv.z = verts[m].z - pos.z;
				Vec3_RotateSC(&r, &dv, &c, &sn);
				dv.x = verts[m].x - pos.x;
				dv.y = verts[m].y - pos.y;
				dv.z = verts[m].z - pos.z;
				Vec3_NormalizeTo(&n, &dv);
				float o = self->offset;
				r.x = r.x + n.x * o;
				r.y = r.y + n.y * o;
				r.z = r.z + n.z * o;
				float dx = r.x - prev.x;
				float dy = r.y - prev.y;
				float dz = r.z - prev.z;
				float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
				if (best == -1 || (dist > 16.0f && bestDist >= dist)) {
					best = m;
					bestDist = dist;
					*FXV_EMIVidx(self, i * FXVEMI_POINTS + self->tail[i]) = vids[m];
					*FXV_EMIPt(self, i * FXVEMI_POINTS + self->tail[i]) = r;
				}
			}
			if (best >= 0) {
				int vi = *FXV_EMIVidx(self, i * FXVEMI_POINTS + self->tail[i]);
				*FXV_EMIMark(self, i, vi) |= (uint16_t)(1u << (vi & 0xf));
				const vec3_t *p = FXV_EMIPt(self, i * FXVEMI_POINTS + self->tail[i]);
				FX_Normalize(&self->dir, prev.x - p->x, prev.y - p->y, prev.z - p->z);
			}
		}
	}
	if (self->stepTime > 0.02f) {
		self->stepTime = 0.0f;
	}
}

/* ---- Fire ---- */


/* (Rand_FloatRange(-0.35, 0.35) * s + s) * r, r drawn first */
static inline float FXV_FireSize(float s, float r) {
	float f = Rand_FloatRange(-0.35f, 0.35f);
	return (f * s + s) * r;
}

/* the flames along the ship (16 of its vertices and 8 of a motor) and the smoke behind a fast AI ship */
static uint32_t FXV_FIRE_RAMPS[27] = {
	0xff3b00, 0xff4300, 0xff5200, 0xff6000, 0x20ff7300, 0x80ff8300, 0xc4804100, 0xff000000, 0, 0x80ffffff, 0xffff94,
	0xf3f750, 0xcda736, 0x20a44a18, 0x80871005, 0xc4800000, 0xff000000, 0, 0xff000000, 0x80824c11, 0x40ef6e20,
	0x20ff7d32, 0x40b48250, 0x80877732, 0xc45e4022, 0xff000000,
};

static uint32_t cObjElementFXVehicleFire_UpdateTable[19] = {
	0xff3c3c3c, 0xc0333333, 0x802c2c2c, 0x40232323, 0x401a1a1a, 0x80101010, 0xc0070707, 0xff000000, 0xff000000,
	0xff464646, 0x80373737, 0x60282828, 0x40323232, 0x603c3c3c, 0x802d2d2d, 0xc00f0f0f, 0xff000000, 0xff000000,
};

static void cObjElementFXVehicleFire_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXVehicleFire *self = (cObjElementFXVehicleFire *)base;
	cEntity *e = self->entity;
	if (e && (Entity_Removed(e) || !e->active)) {
		Obj_SetEffect(e, MODEL_EFFECT_NONE);
		self->entity = NULL;
	}
	float d = cTimer_GetDelta(&g_pWorld->timer);
	e = self->entity;
	self->time = self->time + d;
	self->emitTime = self->emitTime + d;
	if (!e || !e->active || !CGameModel_IsLoaded(Obj_GetModel(e))
		|| !CGameModel_IsVisible(Obj_GetModel(self->entity))) {
		return;
	}
	CGameModel *model = Obj_GetModel(self->entity);
	CGameModel *dm = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	vec3_t pos, rot;
	CHECK(CGameModel_IsLoaded(model));
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	CGameModel_SetPos(dm, &pos);
	CGameModel_SetRot(dm, &rot);
	CGameModel_SetSortZ(dm, -49.0f, 0.0f);
	Obj_SetEffect(self->entity, MODEL_EFFECT_FIRE);
	float v = self->time;
	if (v > 0.2f) {
		v = debris->life - dt;
		if (v > 0.2f) {
			v = 1.0f;
		} else {
			v = v * 5.0f;
		}
	} else {
		v = v * 5.0f;
	}
	Obj_SetEffectParam(self->entity, 0, v);
	if (!(debris->life > 0.0f) || !(self->emitTime > 0.04f) || !(FXV_Veh(self->entity)->wl.fireRemain > 0.5f)) {
		return;
	}

	const cVehicleEntity *veh = FXV_Veh(self->entity);
	float life = (1.0f - veh->wl.fireAtt * 0.25f) * 0.75f;
	float rad = Obj_GetRadius(self->entity);
	float s1 = FXV_Rand(0.20000000298023224, 0.20000000298023224) * rad;
	float s2 = FXV_Rand(0.25, 0.8500000238418579) * rad;
	vec3_t lv = FXV_Veh(self->entity)->localVel;
	float vz = 0.0f;
	if (!(lv.z > 0.0f)) {
		vz = lv.z * 0.5f;
	}
	float vx = -(lv.x * 0.25f) * 0.55f;
	float vy = -(lv.y * 0.25f) * 0.55f;
	vz = -vz * 0.55f;
	/* 16 vertices of the ship, 8 of a motor (left as they are on the stack without one) */
	vec3_t verts[24];
	CGameModel_Vertices(model, verts, NULL, NULL, 16, -1, NULL, 0xffffffffu, NULL);
	if (self->numExhausts > 0 && (crt_rand() & 1)) {
		int k = crt_rand() % self->numExhausts;
		CGameModel *mm = FXV_ExhaustModel(self->entity, &self->exhausts[k]);
		CHECK(CGameModel_IsLoaded(mm));
		CGameModel_Vertices(mm, &verts[16], NULL, NULL, 8, -1, NULL, 0xffffffffu, NULL);
	}
	vec3_t c, sn;
	Float_SinCos(-rot.x, &sn.x, &c.x);
	Float_SinCos(-rot.y, &sn.y, &c.y);
	Float_SinCos(-rot.z, &sn.z, &c.z);
	p->birthRate = 0;
	CGameParticles_SetColorRampOn(p, 1);

	for (int i = 0; i < 24; i++) {
		vec3_t dv, r, n;
		dv.x = verts[i].x - pos.x;
		dv.y = verts[i].y - pos.y;
		dv.z = verts[i].z - pos.z;
		Vec3_RotateSC(&r, &dv, &c, &sn);
		Vec3_NormalizeTo(&n, &r);
		float t;
		switch (crt_rand() % 3) {
		case 0:
			p->initPosMin.x = r.x + n.x * 5.0f;
			p->initPosMin.z = r.z + n.z * 5.0f;
			p->initPosMin.y = r.y + n.y * 5.0f;
			p->initPosMax = p->initPosMin;
			p->initVelMin.x = vx;
			p->initVelMin.y = vy;
			p->initVelMin.z = vz;
			p->initVelMax = p->initVelMin;
			t = FXV_Rand(0.09999996423721313, 0.6000000238418579);
			p->initAccMin.x = -p->initVelMin.x * t;
			p->initAccMin.y = -p->initVelMin.y * t;
			p->initAccMin.z = -p->initVelMin.z * t;
			p->initAccMax = p->initAccMin;
			p->sizeIni = FXV_FireSize(s1, FXV_Rand(0.10000002384185791, 0.5));
			p->sizeEnd = FXV_FireSize(s2, FXV_Rand(0.10000002384185791, 0.699999988079071));
			t = FXV_Rand(0.19999998807907104, 0.25) * life;
			p->lifeTime = Float_MinSS(debris->life, t);
			t = FXV_Rand(6.2831854820251465, 0.0);
			p->rotIni = t;
			p->rotEnd = t;
			p->rotVar = FXV_Rand(1.0471975803375244, -0.5235987901687622);
			FXV_SetSprite(p, 0x13);
			p->sizeLife = FXV_Rand(0.10000002384185791, 0.5);
			CGameParticles_SetColorRamp(p, FXV_Ramp(FXV_FIRE_RAMPS + 9 * (crt_rand() % 3)));
			CGameParticles_Apply(p);
			CGameParticles_EmitDt(p, 1, 1, self->emitTime * 0.25f);
			break;
		case 1:
			p->initPosMin.z = n.z * 3.0f + r.z;
			p->initPosMin.x = n.x * 3.0f + r.x;
			p->initPosMin.y = n.y * 3.0f + r.y;
			p->initPosMax = p->initPosMin;
			p->initVelMin.x = vx;
			p->initVelMin.y = vy;
			p->initVelMin.z = vz;
			p->initVelMax = p->initVelMin;
			t = FXV_Rand(0.09999996423721313, 0.6000000238418579);
			p->initAccMin.x = t * -p->initVelMin.x;
			p->initAccMin.y = t * -p->initVelMin.y;
			p->initAccMin.z = t * -p->initVelMin.z;
			p->initAccMax = p->initAccMin;
			p->sizeIni = FXV_FireSize(s1, FXV_Rand(0.10000002384185791, 0.5));
			p->sizeEnd = FXV_FireSize(s2, FXV_Rand(0.10000002384185791, 0.699999988079071));
			t = FXV_Rand(0.19999998807907104, 0.25) * life;
			p->lifeTime = Float_MinSS(debris->life, t);
			t = FXV_Rand(6.2831854820251465, 0.0);
			p->rotIni = t;
			p->rotEnd = t;
			p->rotVar = FXV_Rand(1.0471975803375244, -0.5235987901687622);
			p->sizeLife = FXV_Rand(0.10000002384185791, 0.5);
			FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
			CGameParticles_SetColorRamp(p, FXV_Ramp(FXV_FIRE_RAMPS + 9 * (crt_rand() % 2)));
			CGameParticles_Apply(p);
			CGameParticles_EmitDt(p, 1, 1, self->emitTime * 0.25f);
			break;
		case 2:
			p->initPosMin = r;
			p->initPosMax = p->initPosMin;
			p->initVelMin.x = vx;
			p->initVelMin.y = vy;
			p->initVelMin.z = vz;
			p->initVelMax = p->initVelMin;
			t = FXV_Rand(0.10000002384185791, 0.5);
			p->initAccMin.x = t * -p->initVelMin.x;
			p->initAccMin.y = t * -p->initVelMin.y;
			p->initAccMin.z = t * -p->initVelMin.z;
			p->initAccMax = p->initAccMin;
			p->sizeIni = FXV_FireSize(s1, FXV_Rand(0.10000000894069672, 0.20000000298023224));
			p->sizeEnd = FXV_FireSize(s2, FXV_Rand(0.19999998807907104, 0.699999988079071));
			t = FXV_Rand(0.19999998807907104, 0.30000001192092896) * life;
			p->lifeTime = Float_MinSS(debris->life, t);
			t = FXV_Rand(1.5707963705062866, -0.7853981852531433);
			p->rotIni = t;
			p->rotEnd = t;
			p->rotVar = FXV_Rand(1.0471975803375244, -0.5235987901687622);
			p->sizeLife = 1.0f;
			FXV_SetSprite(p, 0x37);
			CGameParticles_SetColorRamp(p, FXV_Ramp((&FXV_FIRE_RAMPS[9]) + 9 * (crt_rand() % 2)));
			CGameParticles_Apply(p);
			CGameParticles_EmitDt(p, 1, 1, self->emitTime * 0.5f);
			break;
		}
	}

	/* the smoke behind an AI ship faster than 250 */
	veh = FXV_Veh(self->entity);
	if (!veh->wl.obj.cam && veh->speed > 250.0f) {
		CGameParticles *po = &self->particlesOut;
		float t;
		po->birthRate = 0;
		CGameParticles_SetColorRampOn(po, 1);
		po->initPosMin = verts[crt_rand() % 8];
		po->initPosMax = po->initPosMin;
		po->initVelMin.x = 0.0f;
		po->initVelMin.y = 0.0f;
		po->initVelMin.z = 0.0f;
		po->initVelMax = po->initVelMin;
		po->initAccMin.x = 0.0f;
		po->initAccMin.y = 0.0f;
		po->initAccMin.z = 0.0f;
		po->initAccMax = po->initAccMin;
		po->sizeIni = FXV_FireSize(s1, FXV_Rand(0.5, 1.0));
		po->sizeEnd = FXV_FireSize(s2, FXV_Rand(1.2000000476837158, 2.200000047683716));
		t = FXV_Rand(0.09999999403953552, 0.25);
		po->lifeTime = Float_MinSS(debris->life, t);
		t = FXV_Rand(6.2831854820251465, 0.0);
		po->rotIni = t;
		po->rotEnd = t;
		po->sizeLife = 1.0f;
		po->rotVar = FXV_Rand(0.5235987901687622, -0.2617993950843811);
		if (crt_rand() & 1) {
			FXV_SetSprite(po, (byte)((crt_rand() & 1) + 0x11));
		} else {
			FXV_SetSprite(po, (byte)(crt_rand() % 4 + 0x21));
		}
		CGameParticles_SetColorRamp(po, FXV_Ramp(FXV_FIRE_RAMPS + 9 * (crt_rand() % 3)));
		CGameParticles_Apply(po);
		CGameParticles_EmitN(po, 1, 1);

		po->initPosMin = verts[crt_rand() % 8];
		po->initPosMax = po->initPosMin;
		t = FXV_Rand(160.0, 125.0);
		po->initVelMin.x = 0.0f;
		po->initVelMin.z = 0.0f;
		po->initVelMin.y = t;
		po->initVelMax = po->initVelMin;
		t = FXV_Rand(100.0, 550.0);
		po->initAccMin.x = 0.0f;
		po->initAccMin.z = 0.0f;
		po->initAccMin.y = t;
		po->initAccMax = po->initAccMin;
		po->sizeIni = FXV_FireSize(s1, FXV_Rand(0.7999999523162842, 2.0));
		po->sizeEnd = FXV_FireSize(s2, FXV_Rand(1.2000000476837158, 3.700000047683716));
		t = FXV_Rand(0.20999997854232788, 0.7400000095367432);
		po->lifeTime = Float_MinSS(debris->life, t);
		t = FXV_Rand(6.2831854820251465, 0.0);
		po->rotIni = t;
		po->rotEnd = t;
		po->sizeLife = 1.0f;
		po->rotVar = FXV_Rand(0.5235987901687622, -0.2617993950843811);
		if (crt_rand() & 1) {
			byte spr = (byte)((crt_rand() & 1) + 0x21);
			po->spriteEnd = 0x24;
			po->spriteIni = spr;
		} else {
			FXV_SetSprite(po, (byte)((crt_rand() & 1) + 0x11));
		}
		CGameParticles_SetColorRamp(po, FXV_RandRamp(cObjElementFXVehicleFire_UpdateTable));
		CGameParticles_Apply(po);
		CGameParticles_EmitDt(po, 1, 1, Rand_FloatRange(0.0f, self->emitTime / 3.0f));
	}
	self->emitTime = 0.0f;
}

static cGVar GVar_FXVehicleDamageSmokeStep;

/* ---- Damage ---- */

#define g_FXVehicleDamageSmokeStep (GVar_FXVehicleDamageSmokeStep.value.f)	/* FXVehicleDamageSmokeStep */
#define FXV_DAMAGE_VERTS 12		/* of the ship, then 6 of a motor */
#define FXV_PI2 6.2831854820251465

static inline uint32_t *FXV_RampN(uint32_t *ramps, int n) {
	return FXV_Ramp(ramps + 9 * (crt_rand() % n));
}

/* the speeds of the next particles and their slowdown by a random factor */
static void FXV_VelBrake(CGameParticles *p, const vec3_t *vel, double range, double add) {
	p->initVelMin = *vel;
	p->initVelMax = p->initVelMin;
	float g = FXV_Rand(range, add);
	p->initAccMin.x = -p->initVelMin.x * g;
	p->initAccMin.y = -p->initVelMin.y * g;
	p->initAccMin.z = -p->initVelMin.z * g;
	p->initAccMax = p->initAccMin;
}

static uint32_t FXV_DamageDebrisRamp[9] = { 0x80dcdcff, 0x84d2d2ff, 0x8c8282e6, 0x965e5eab, 0xa444444c, 0xc00f0f38, 0xde050519, 0xff000000, 0xff000000 };

/* a still piece of debris at r, lifeTime scaled by k, its rotation at the end turned by up to +-90 degrees */
static void FXV_DamageDebris(CGameParticles *p, const vec3_t *r, double iniRange, double iniAdd, double endRange,
	double endAdd, double lifeRange, double lifeAdd, float k, byte sprite) {
	const vec3_t zero = { 0.0f, 0.0f, 0.0f };

	p->initPosMin = *r;
	p->initPosMax = p->initPosMin;
	FXV_VelBrake(p, &zero, 0.10000002384185791, 0.5);
	p->sizeIni = FXV_Rand(iniRange, iniAdd);
	p->sizeEnd = FXV_Rand(endRange, endAdd);
	p->lifeTime = FXV_Rand(lifeRange, lifeAdd) * 0.75f * k;
	p->rotIni = FXV_Rand(FXV_PI2, 0.0);
	p->rotVar = 0.0f;
	p->rotEnd = FXV_Rand(3.1415927410125732, -1.5707963705062866) + p->rotIni;
	FXV_SetSprite(p, sprite);
	p->sizeLife = FXV_Rand(0.039999991655349731, 0.2199999988079071);
	CGameParticles_SetColorRamp(p, FXV_Ramp(FXV_DamageDebrisRamp));
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 0);
}

static uint32_t FXV_DamageDebrisTable[46] = {
	0xff000000, 0x40141414, 0x40141414, 0x40141414, 0x60141414, 0x800a0a0a, 0xc0050505, 0xff000000, 0xff000000,
	0xff000000, 0x40191919, 0x40191919, 0x40191919, 0x60191919, 0x800d0d0d, 0xc0060606, 0xff000000, 0xff000000,
	0xff000000, 0x401e1e1e, 0x401e1e1e, 0x401e1e1e, 0x601e1e1e, 0x800f0f0f, 0xc0070707, 0xff000000, 0xff000000,
	0xff000000, 0x40232323, 0x40232323, 0x40232323, 0x60232323, 0x80111111, 0xc0080808, 0xff000000, 0xff000000,
	0xff000000, 0x40282828, 0x40282828, 0x40282828, 0x60282828, 0x80141414, 0xc00a0a0a, 0xff000000, 0xff000000,
};

/* the smoke at a vertex r of the ship moving with vel; c3, c4 the sizes */
static void FXV_DamageSmoke(CGameParticles *p, const vec3_t *r, const vec3_t *vel, float c3, float c4,
	double lifeRange, double lifeAdd, double rotRange, double rotAdd) {
	p->initPosMin = *r;
	p->initPosMax = p->initPosMin;
	FXV_VelBrake(p, vel, 0.10000002384185791, 0.5);
	p->sizeIni = FXV_Rand(0.69999998807907104, -0.34999999403953552) * c3 + c3;
	p->sizeEnd = FXV_Rand(0.69999998807907104, -0.34999999403953552) * c4 + c4;
	p->lifeTime = FXV_Rand(lifeRange, lifeAdd) * 0.75f;
	float t = FXV_Rand(FXV_PI2, 0.0);
	p->rotIni = t;
	p->rotEnd = t;
	p->rotVar = FXV_Rand(rotRange, rotAdd);
	p->sizeLife = FXV_Rand(0.10000002384185791, 0.89999997615814209);
	FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x11));
	CGameParticles_SetColorRamp(p, FXV_RampN(FXV_DamageDebrisTable, 5));
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
}

/* the size of a flame: (r1 * c + c) times a second random factor */
static inline float FXV_FlameSize(float c, double range, double add) {
	float r1 = FXV_Rand(0.69999998807907104, -0.34999999403953552);
	return FXV_Rand(range, add) * (r1 * c + c);
}

static uint32_t FXV_FLAME_RAMPS[27] = {
	0xff3b00, 0xff4300, 0xff5200, 0xff6000, 0x20ff7300, 0x80ff8300, 0xc4804100, 0xff000000, 0xff000000, 0x80ffffff,
	0xffff94, 0xf3f750, 0xcda736, 0x20a44a18, 0x80871005, 0xc4800000, 0xff000000, 0xff000000, 0xff000000,
	0x80824c11, 0x40ef6e20, 0x20ff7d32, 0x40b48250, 0x80877732, 0xc45e4022, 0xff000000, 0xff000000,
};

/* a flame at the vertex r of the ship (n its direction from the ship) moving with vel; c3, c4 the sizes */
static void FXV_DamageFlame(cObjElementFXVehicleDamage *self, cDebris *debris, const vec3_t *r, const vec3_t *n,
	const vec3_t *vel, float c3, float c4) {
	CGameParticles *p = &self->parts2;
	float t;

	switch (crt_rand() % 3) {
	case 0:
		p->initPosMin.z = n->z * 5.0f + r->z;
		p->initPosMin.x = n->x * 5.0f + r->x;
		p->initPosMin.y = n->y * 5.0f + r->y;
		p->initPosMax = p->initPosMin;
		FXV_VelBrake(p, vel, 0.099999964237213135, 0.60000002384185791);
		p->sizeIni = FXV_FlameSize(c3, 0.10000002384185791, 0.5);
		p->sizeEnd = FXV_FlameSize(c4, 0.099999964237213135, 0.60000002384185791);
		t = FXV_Rand(0.19999998807907104, 0.25) * 0.75f;
		p->lifeTime = Float_MinSS(debris->life, t);
		t = FXV_Rand(FXV_PI2, 0.0);
		p->rotIni = t;
		p->rotEnd = t;
		p->rotVar = FXV_Rand(1.0471975803375244, -0.5235987901687622);
		FXV_SetSprite(p, 0x13);
		p->sizeLife = FXV_Rand(0.10000002384185791, 0.5);
		CGameParticles_SetColorRamp(p, FXV_RampN(FXV_FLAME_RAMPS, 3));
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, self->emitTime * 0.25f);
		break;
	case 1:
		p->initPosMin.z = n->z * 3.0f + r->z;
		p->initPosMin.x = n->x * 3.0f + r->x;
		p->initPosMin.y = n->y * 3.0f + r->y;
		p->initPosMax = p->initPosMin;
		FXV_VelBrake(p, vel, 0.099999964237213135, 0.60000002384185791);
		p->sizeIni = FXV_FlameSize(c3, 0.10000002384185791, 0.5);
		p->sizeEnd = FXV_FlameSize(c4, 0.099999964237213135, 0.60000002384185791);
		t = FXV_Rand(0.19999998807907104, 0.25) * 0.75f;
		p->lifeTime = Float_MinSS(debris->life, t);
		t = FXV_Rand(FXV_PI2, 0.0);
		p->rotIni = t;
		p->rotEnd = t;
		p->rotVar = FXV_Rand(1.0471975803375244, -0.5235987901687622);
		p->sizeLife = FXV_Rand(0.10000002384185791, 0.5);
		FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
		CGameParticles_SetColorRamp(p, FXV_RampN(FXV_FLAME_RAMPS, 2));
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, self->emitTime * 0.25f);
		break;
	case 2:
		p->initPosMin = *r;
		p->initPosMax = p->initPosMin;
		FXV_VelBrake(p, vel, 0.10000002384185791, 0.5);
		p->sizeIni = FXV_FlameSize(c3, 0.10000000894069672, 0.20000000298023224);
		p->sizeEnd = FXV_FlameSize(c4, 0.19999998807907104, 0.60000002384185791);
		t = FXV_Rand(0.19999998807907104, 0.30000001192092896) * 0.75f;
		p->lifeTime = Float_MinSS(debris->life, t);
		t = FXV_Rand(1.5707963705062866, -0.7853981852531433);
		p->rotIni = t;
		p->rotEnd = t;
		p->sizeLife = 1.0f;
		FXV_SetSprite(p, 0x37);
		p->rotVar = FXV_Rand(1.0471975803375244, -0.5235987901687622);
		CGameParticles_SetColorRamp(p, FXV_RampN((&FXV_FLAME_RAMPS[9]), 2));
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, self->emitTime * 0.5f);
		break;
	}
}

static uint32_t cObjElementFXVehicleDamage_UpdateRamps[27] = {
	0xfffff696, 0xa4fff08c, 0xafe6b411, 0xb9ab720d, 0xc89c4408, 0xd76a2603, 0xe6401400, 0xff000000, 0xff000000,
	0xffffe65a, 0xa4ffe150, 0xafef8228, 0xb99a440f, 0xc87c1f0c, 0xd75e170a, 0xe1400f05, 0xff000000, 0xff000000,
	0xffffa032, 0xa4ff7832, 0xafef3c19, 0xb99a230f, 0xc87c0f0a, 0xd75e0a05, 0xe1400502, 0xff000000, 0xff000000,
};

/* a puff of the trail of smoke at pt: around the position of the ship (far) or behind it */
static void FXV_TrailPuff(CGameParticles *p, const vec3_t *pt, const vec3_t *pos, const vec3_t *rot, float rad,
	float sc, byte far) {
	vec3_t d, o, r, v;
	const vec3_t z = { 0.0f, 0.0f, 1.0f };

	FX_RandomDir(&d);
	if (far) {
		float s = FXV_Rand(4.0, 1.0);
		o.x = d.x * s;
		o.y = d.y * s;
		o.z = d.z * s;
		float lo = rad * -0.5f;
		float hi = rad * 0.5f;
		v.x = 0.0f;
		v.y = 0.0f;
		v.z = FXV_Rand((double)(hi - lo), (double)lo);
		Vec3_Rotate(&r, &v, rot);
		p->initPosMin.x = pos->x + r.x + o.x;
		p->initPosMin.y = r.y + pos->y + o.y;
		p->initPosMin.z = r.z + pos->z + o.z;
		p->initPosMax = p->initPosMin;
		float vs = FXV_Rand(520.0, 440.0);
		Vec3_Rotate(&r, &z, rot);
		p->initVelMin.x = r.x * vs;
		p->initVelMin.y = r.y * vs;
		p->initVelMin.z = r.z * vs;
		p->initVelMax = p->initVelMin;
		float g = FXV_Rand(0.14999997615814209, 0.30000001192092896);
		p->initAccMin.x = -p->initVelMin.x * g;
		p->initAccMin.y = -p->initVelMin.y * g;
		p->initAccMin.z = -p->initVelMin.z * g;
	} else {
		float s = FXV_Rand(15.0, 10.0);
		o.x = d.x * s;
		o.y = d.y * s;
		o.z = d.z * s;
		v.x = 0.0f;
		v.y = 0.0f;
		v.z = rad * -0.25f;
		Vec3_Rotate(&r, &v, rot);
		p->initPosMin.x = pt->x + r.x + o.x;
		p->initPosMin.y = r.y + pt->y + o.y;
		p->initPosMin.z = r.z + pt->z + o.z;
		p->initPosMax = p->initPosMin;
		p->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		p->initVelMax = p->initVelMin;
		p->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	}
	p->initAccMax = p->initAccMin;
	p->sizeIni = FXV_Rand(50.0, 250.0) * sc;
	p->sizeEnd = FXV_Rand(50.0, 500.0) * sc;
	p->lifeTime = FXV_Rand(0.050000011920928955, 0.25);
	float t = FXV_Rand(FXV_PI2, 0.0);
	p->rotIni = t;
	p->rotEnd = t;
	p->rotVar = FXV_Rand(1.5707963705062866, -0.78539818525314331);
	p->sizeLife = FXV_Rand(0.099999994039535522, 0.30000001192092896);
	switch (crt_rand() % 3) {
	case 0:
		FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x19));
		break;
	case 1:
		FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
		break;
	case 2:
		FXV_SetSprite(p, 0x37);
		break;
	}
	CGameParticles_SetColorRamp(p, FXV_RandRamp(cObjElementFXVehicleDamage_UpdateRamps));
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
}

/* the big puff of the trail at p0 pushed out by rad, rising */
static void FXV_TrailBigPuff(CGameParticles *p, const vec3_t *p0, const vec3_t *rot, float rad, byte far) {
	vec3_t d, r;
	const vec3_t z = { 0.0f, 0.0f, 1.0f };
	vec3_t v = { 0.0f, 0.0f, rad };

	Vec3_Rotate(&r, &v, rot);
	p->initPosMin.x = p0->x + r.x;
	p->initPosMin.y = r.y + p0->y;
	p->initPosMin.z = r.z + p0->z;
	p->initPosMax = p->initPosMin;
	FX_RandomDir(&d);
	float s = FXV_Rand(25.0, 65.0);
	p->initVelMin.x = d.x * s;
	p->initVelMin.y = d.y * s;
	p->initVelMin.z = d.z * s;
	s = FXV_Rand(40.0, 225.0);
	p->initVelMax.x = d.x * s;
	p->initVelMax.y = d.y * s;
	p->initVelMax.z = d.z * s;
	if (far) {
		s = FXV_Rand(290.0, 150.0);
		Vec3_Rotate(&r, &z, rot);
		p->initVelMin.x = r.x * s + p->initVelMin.x;
		p->initVelMin.y = r.y * s + p->initVelMin.y;
		p->initVelMin.z = r.z * s + p->initVelMin.z;
		s = FXV_Rand(290.0, 150.0);
		Vec3_Rotate(&r, &z, rot);
		p->initVelMax.x = s * r.x + p->initVelMax.x;
		p->initVelMax.y = s * r.y + p->initVelMax.y;
		p->initVelMax.z = s * r.z + p->initVelMax.z;
	}
	p->initAccMin = (vec3_t){ 0.0f, 350.0f, 0.0f };
	p->initAccMax = (vec3_t){ 0.0f, 850.0f, 0.0f };
	p->sizeIni = FXV_Rand(50.0, 700.0);
	p->sizeEnd = FXV_Rand(100.0, 1250.0);
	p->lifeTime = FXV_Rand(0.10000002384185791, 1.1499999761581421);
	float t = FXV_Rand(FXV_PI2, 0.0);
	p->rotIni = t;
	p->rotEnd = t;
	p->sizeLife = 1.0f;
	p->rotVar = FXV_Rand(0.78539818525314331, -0.39269909262657166);
	if (crt_rand() % 5 == 0) {
		FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x1c));
	} else {
		FXV_SetSprite(p, (byte)((crt_rand() & 1) + 0x11));
	}
	CGameParticles_SetColorRamp(p, FXV_RampN(FXV_DamageDebrisTable, 5));
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
}

/* the damage at the vertices: debris and sparks at one vertex now and then, smoke at four, flames along the ship
 * while the level is low */
static void FXV_DamageVertices(cObjElementFXVehicleDamage *self, cDebris *debris, CGameModel *model,
	const vec3_t *pos, const vec3_t *c, const vec3_t *sn) {
	cEntity *e = self->entity;
	CGameParticles *p2 = &self->parts2;
	CGameParticles *p3 = &self->parts3;
	vec3_t verts[FXV_DAMAGE_VERTS + 6], dv, r, n, v, vel;

	float a = FXV_Rand(200.0, 1400.0);
	float b = FXV_Rand(50.0, 250.0);
	float c3 = FXV_Rand(0.069999992847442627, 0.15000000596046448) * b;
	float c4 = FXV_Rand(0.5, 0.85000002384185791) * b;
	vec3_t lv = FXV_Veh(e)->localVel;
	float vz = 0.0f;
	if (!(lv.z > 0.0f)) {
		vz = lv.z * 0.5f;
	}
	vel.x = 0.0f - lv.x * 0.25f * 0.15f;
	vel.y = -25.0f - lv.y * 0.25f * 0.15f;
	vel.z = a * 0.25f - vz * 0.15f;
	CGameModel_Vertices(model, verts, NULL, NULL, FXV_DAMAGE_VERTS, -1, NULL, 0xffffffffu, NULL);
	if (self->numExhausts > 0 && (crt_rand() & 1)) {
		int k = crt_rand() % self->numExhausts;
		CGameModel *mm = FXV_ExhaustModel(self->entity, &self->exhausts[k]);
		CHECK(CGameModel_IsLoaded(mm));
		CGameModel_Vertices(mm, &verts[FXV_DAMAGE_VERTS], NULL, NULL, 6, -1, NULL, 0xffffffffu, NULL);
	}

	/* debris and sparks at the vertex 4, the more often the lower the level */
	p2->birthRate = 0;
	CGameParticles_SetColorRampOn(p2, 1);
	int m = (int)(self->level + self->level);
	if (crt_rand() % (m < 1 ? 1 : m) == 0) {
		dv.x = verts[4].x - pos->x;
		dv.y = verts[4].y - pos->y;
		dv.z = verts[4].z - pos->z;
		Vec3_RotateSC(&r, &dv, c, sn);
		dv.x = verts[4].x - pos->x;
		dv.y = verts[4].y - pos->y;
		dv.z = verts[4].z - pos->z;
		Vec3_NormalizeTo(&n, &dv);
		float k = FXV_Rand(0.59999996423721313, 0.69999998807907104);
		FXV_DamageDebris(p2, &r, 10.0, 30.0, 60.0, 120.0, 0.080000013113021851, 0.31999999284744263, k, 2);
		FXV_DamageDebris(p2, &r, 10.0, 10.0, 30.0, 285.0, 0.039999991655349731, 0.2800000011920929, k, 5);
		CGameParticles_SetSparksOn(p2, 1);
		for (int i = 0; i < crt_rand() % 8 + 6; i++) {
			float b1 = FXV_Rand(1.0471975803375244, -0.5235987901687622);
			float b2 = FXV_Rand(1.0471975803375244, -0.5235987901687622);
			float b3 = FXV_Rand(1.0471975803375244, -0.5235987901687622);
			vec3_t rot = { b3, b2, b1 };
			Vec3_Rotate(&v, &n, &rot);
			p2->initPosMin = r;
			p2->initPosMax = p2->initPosMin;
			float vs = FXV_Rand(1300.0, 950.0);
			p2->initVelMin.x = v.x * vs;
			p2->initVelMin.y = v.y * vs;
			p2->initVelMin.z = v.z * vs;
			p2->initVelMax = p2->initVelMin;
			float g = FXV_Rand(0.14999997615814209, 0.30000001192092896);
			p2->initAccMin.x = -p2->initVelMin.x * g + 0.0f;
			p2->initAccMin.y = -p2->initVelMin.y * g - 2500.0f;
			p2->initAccMin.z = -p2->initVelMin.z * g + 0.0f;
			p2->initAccMax = p2->initAccMin;
			float size = FXV_Rand(15.0, 45.0);
			p2->sizeIni = size;
			p2->sizeEnd = size;
			float lt = FXV_Rand(0.19999998807907104, 0.30000001192092896);
			p2->rotIni = 0.0f;
			p2->rotEnd = 0.0f;
			p2->rotVar = 0.0f;
			p2->sizeLife = 1.0f;
			p2->spriteIni = 0x41;
			p2->spriteEnd = 0x45;
			p2->lifeTime = lt * 0.75f * k;
			CGameParticles_SetColorRamp(p2, FXV_Ramp(FXV_DamageDebrisRamp));
			p2->sparkMaxSize = 1.0f;
			CGameParticles_Apply(p2);
			CGameParticles_EmitDt(p2, 1, 0, FXV_Rand(0.10000000149011612, 0.0));
		}
		CGameParticles_SetSparksOn(p2, 0);
	}

	/* smoke at the first four vertices */
	p3->birthRate = 0;
	CGameParticles_SetColorRampOn(p3, 1);
	for (int i = 0; i < 4; i++) {
		dv.x = verts[i].x - pos->x;
		dv.y = verts[i].y - pos->y;
		dv.z = verts[i].z - pos->z;
		Vec3_RotateSC(&r, &dv, c, sn);
		if (0.99f > self->level) {
			FXV_DamageSmoke(p3, &r, &vel, c3, c4, 0.40000000596046448, 0.40000000596046448, 1.5707963705062866,
				-0.78539818525314331);
		}
		if (0.65f > self->level) {
			FXV_DamageSmoke(p3, &r, &vel, c3, c4, 0.39999997615814209, 0.30000001192092896, 1.0471975803375244,
				-0.5235987901687622);
		}
	}

	/* flames along the ship, the more the lower the level */
	p2->birthRate = 0;
	CGameParticles_SetColorRampOn(p2, 1);
	float lv0 = self->level;
	if (!(lv0 < 0.0f) && 0.45f > lv0 && 18.0f - lv0 * 25.0f > 0.0f) {
		for (int j = 0; 18.0f - self->level * 25.0f > (float)j; j++) {
			dv.x = verts[j].x - pos->x;
			dv.y = verts[j].y - pos->y;
			dv.z = verts[j].z - pos->z;
			Vec3_RotateSC(&r, &dv, c, sn);
			n = FX_Unit(r);
			FXV_DamageFlame(self, debris, &r, &n, &vel, c3, c4);
		}
	}
	self->emitTime = 0.0f;
}

/* the trail of smoke behind the moving ship (pos, its angles rot, radius rad) */
static void FXV_DamageTrail(cObjElementFXVehicleDamage *self, const vec3_t *pos, const vec3_t *rot, float t,
	float rad) {
	CGameParticles *p1 = &self->parts1;
	vec3_t tmp;

	if (!self->seen) {
		self->pos = *pos;
	}
	float tt = t + self->trailTime;
	self->seen = 1;
	self->trailTime = tt;
	vec3_t d = { self->pos.x - pos->x, self->pos.y - pos->y, self->pos.z - pos->z };
	float d2 = d.y * d.y + d.x * d.x + d.z * d.z;
	float step = g_FXVehicleDamageSmokeStep;
	if (step * step > d2 && 0.035f >= tt) {
		return;
	}
	float len = (float)__builtin_sqrt((double)d2);
	vec3_t nd = d;
	if (len != 0.0f) {
		float k = 1.0f / len;
		nd.x = d.x * k;
		nd.y = d.y * k;
		nd.z = d.z * k;
	}
	vec3_t dir;
	if (tt > 0.035f && 5.0f > len) {
		const vec3_t z = { 0.0f, 0.0f, 1.0f };
		dir = *Vec3_Rotate(&tmp, &z, rot);
	} else {
		dir = nd;
	}
	byte far = 0;
	if (self->trailTime > 0.035f) {
		far = 1;
		len = g_FXVehicleDamageSmokeStep;
	}
	float sc = 1.0f;
	if (rad > 350.0f) {
		sc = (rad - 350.0f) / 900.0f + 1.0f;
	}
	self->trailTime = 0.0f;
	p1->birthRate = 0;
	CGameParticles_SetColorRampOn(p1, 1);
	float u = 0.0f;
	do {
		float k0 = u * len;
		vec3_t p0 = { dir.x * k0 + pos->x, dir.y * k0 + pos->y, dir.z * k0 + pos->z };
		if (self->first || 0.45f > self->level) {
			float f = u;
			if (g_FXVehicleDamageSmokeStep / len > u) {
				do {
					float k = (f + u) * len;
					vec3_t pt = { dir.x * k + pos->x, dir.y * k + pos->y, dir.z * k + pos->z };
					FXV_TrailPuff(p1, &pt, pos, rot, rad, sc, far);
					f = g_FXVehicleDamageSmokeStep * 0.33f / len + f;
				} while (g_FXVehicleDamageSmokeStep / len > f);
			}
		}
		FXV_TrailBigPuff(p1, &p0, rot, rad, far);
		u = g_FXVehicleDamageSmokeStep / len + u;
	} while (1.0f > u);
	self->first = 0;
	self->pos = *pos;
}

/* while the ship is seen: debris, sparks, smoke and flames at its vertices by the damage level, the trail of smoke */
static void cObjElementFXVehicleDamage_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXVehicleDamage *self = (cObjElementFXVehicleDamage *)base;
	CGameModel *dm = Debris_Model(debris);
	vec3_t pos, rot, c, sn;

	(void)dt;
	cEntity *e = self->entity;
	if (e && (Entity_Removed(e) || !e->active)) {
		if (FXVeh_EmbeddedFX(e)) {
			cEmbeddedFXVehicle_SetAttached(FXVeh_EmbeddedFX(e), NULL);
		}
		self->entity = NULL;
	}
	float t = cTimer_GetDelta(&g_pWorld->timer);
	e = self->entity;
	if (!e || !e->active || !CGameModel_IsLoaded(Obj_GetModel(e))
		|| !CGameModel_IsVisible(Obj_GetModel(self->entity))) {
		self->seen = 0;
		return;
	}
	CGameModel *model = Obj_GetModel(self->entity);
	CHECK(CGameModel_IsLoaded(model));
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	CGameModel_SetPos(dm, &pos);
	CGameModel_SetRot(dm, &rot);
	CGameModel_SetSortZ(dm, -CGameModel_Radius(model), 1.0f);
	if (!self->active) {
		return;
	}
	e = self->entity;
	float rad = Obj_GetRadius(e);
	const cVehicleEntity *veh = FXV_Veh(self->entity);
	cEntity *cam = veh->wl.obj.cam;
	Float_Approach(&self->level, (float)self->lifePct / (float)g_FXSetOnFireLifePercent, t);
	CHECK(FXV_Veh(self->entity)->wl.obj.fx);
	cEmbeddedFXVehicle *efx = Veh_FX(FXV_Veh(self->entity));
	byte followed = efx->followed;
	if (followed && Veh(self->entity)->wl.life > 0) {
		cEmbeddedFXVehicle_SetAttached(efx, self);
	}
	Float_SinCos(-rot.x, &sn.x, &c.x);
	Float_SinCos(-rot.y, &sn.y, &c.y);
	Float_SinCos(-rot.z, &sn.z, &c.z);

	/* the smoke and sparks drawn once with the followed ship (FXVehicle_RenderAttached) */
	byte vis;
	if (cam) {
		vis = !followed;
	} else if (followed) {
		vis = 0;
	} else if (!g_SplitScreenGame) {
		goto trail;
	} else {
		vis = 1;
	}
	CGameParticles_SetVisible(&self->parts3, vis);
	CGameParticles_SetVisible(&self->parts2, vis);
	self->seen = 1;
	float et = t + self->emitTime;
	self->emitTime = et;
	if (et > 0.04f) {
		self->first = 0;
		FXV_DamageVertices(self, debris, model, &pos, &c, &sn);
	}

trail:
	if (cam && !g_SplitScreenGame) {
		return;
	}
	FXV_DamageTrail(self, &pos, &rot, t, rad);
}

static cGVar GVar_ShipExplosionMaxDist;

/* ---- FX_ShipExplosion ---- */

#define g_ShipExplosionMaxDist (GVar_ShipExplosionMaxDist.value.f)	/* ShipExplosionMaxDist */
#define FXV_PI_2 1.5707964f

/* the speeds of the next particles: no own speed, no acceleration, the speed of the ship added */
static void FXV_ShipSpeed(CGameParticles *p, const vec3_t *vel) {
	p->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initVelMax = p->initVelMin;
	p->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initAccMax = p->initAccMin;
	p->initVelMin.x = p->initVelMin.x + vel->x;
	p->initVelMin.y = vel->y + p->initVelMin.y;
	p->initVelMin.z = vel->z + p->initVelMin.z;
	p->initVelMax.x = p->initVelMax.x + vel->x;
	p->initVelMax.y = p->initVelMax.y + vel->y;
	p->initVelMax.z = p->initVelMax.z + vel->z;
}

/* the speeds set: braked by g, the speed of the ship added */
static void FXV_ShipBrake(CGameParticles *p, const vec3_t *vel, double range, double add) {
	p->initVelMax = p->initVelMin;
	float g = FXV_Rand(range, add);
	p->initAccMin.x = g * -p->initVelMin.x;
	p->initAccMin.y = g * -p->initVelMin.y;
	p->initAccMin.z = g * -p->initVelMin.z;
	p->initAccMax = p->initAccMin;
	p->initVelMin.x = p->initVelMin.x + vel->x;
	p->initVelMin.y = p->initVelMin.y + vel->y;
	p->initVelMin.z = p->initVelMin.z + vel->z;
	p->initVelMax.x = p->initVelMax.x + vel->x;
	p->initVelMax.y = p->initVelMax.y + vel->y;
	p->initVelMax.z = p->initVelMax.z + vel->z;
}

static void FXV_RandRot(CGameParticles *p) {
	float t = FXV_Rand(FXV_PI2, 0.0);
	p->rotIni = t;
	p->rotEnd = t;
}

/* a unit direction: (1, 1, 1) turned by two random angles (x the second) */
static vec3_t FXV_FlameDir(void) {
	vec3_t t;
	const vec3_t one = { 1.0f, 1.0f, 1.0f };

	float a1 = FXV_Rand(FXV_PI2, 0.0);
	float a2 = FXV_Rand(FXV_PI2, 0.0);
	vec3_t rot = { a2, a1, 0.0f };
	return FX_Unit(*Vec3_Rotate(&t, &one, &rot));
}

/* n debris of sys thrown out of pos: life, position and speed random, the smoke behind it */
static void FXV_ShipDebris(cDebrisSys *sys, int n, const vec3_t *pos, float scale, float time, double lifeRange,
	double lifeAdd, double velRange, double velAdd, float minDist, double spreadRange, double spreadAdd,
	double scaleRange, double scaleAdd) {
	vec3_t dir;

	for (int k = n; k; k--) {
		cDebris *d = cDebrisSys_NewElement(sys);
		d->life = FXV_Rand(lifeRange, lifeAdd) * time;
		cDebrisSys_Drop(sys, d);
		FX_RandomDir(&dir);
		float s = FXV_Rand(115.0, 120.0);
		d->drop[0].x = s * dir.x * scale + pos->x;
		d->drop[0].y = s * dir.y * scale + pos->y;
		d->drop[0].z = s * dir.z * scale + pos->z;
		s = FXV_Rand(velRange, velAdd);
		d->drop[2].x = s * dir.x * scale;
		d->drop[2].y = s * dir.y * scale;
		d->drop[2].z = s * dir.z * scale;
		cObjElementFXFiredDebris *el = (cObjElementFXFiredDebris *)d->fx;
		if (el) {
			if (!el->on) {
				el->on = 1;
			}
			el->minDist = scale * minDist;
			el->spread = FXV_Rand(spreadRange, spreadAdd) * scale;
			el->scale = FXV_Rand(scaleRange, scaleAdd) * scale;
		}
	}
}

static uint32_t FX_ShipExplosionRamps[18] = {
	0xff000000, 0x80ffffff, 0x40ffffff, 0x40ffffff, 0x40ffffff, 0x80c4c4c4, 0xc4808080, 0xff000000, 0xff000000,
	0xff000000, 0x80ffe8e8, 0x40ffe8e8, 0x40ffe8e8, 0x40ffe8e8, 0x80c4a0a0, 0xc4806464, 0xff000000, 0xff000000,
};

static uint32_t FXV_XFIRE_RAMPS[27] = {
	0xff000000, 0x400f0f0f, 0x30141414, 0x30232323, 0x401e1e1e, 0x80141414, 0xc80f0f0f, 0xff000000, 0xff000000,
	0xff000000, 0x5a191919, 0x40232323, 0x40323232, 0x5a3c3c3c, 0xa8323232, 0xc8141414, 0xff000000, 0xff000000,
	0xff000000, 0x5a1e1e1e, 0x402d2d2d, 0x403c3c3c, 0x5a464646, 0xa83c3c3c, 0xc8141414, 0xff000000, 0xff000000,
};

/* the flames: 8 puffs each along a curved way out of the explosion, the ways apart from the last one */
static void FXV_ShipFlames(CGameParticles *p, int numFlames, const vec3_t *pos, const vec3_t *vel, float scale,
	float time) {
	vec3_t prev = { 0.0f, 0.0f, 0.0f };

	for (int f = 0; f < numFlames; f++) {
		vec3_t d1 = FXV_FlameDir();
		vec3_t d2 = FXV_FlameDir();
		float dist = FXV_Rand(45.0, 80.0);
		float size = FXV_Rand(150.0, 650.0);
		if (f > 0) {
			float dy = prev.y - d1.y;
			float dx = prev.x - d1.x;
			float dz = prev.z - d1.z;
			while (0.75f > (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz))) {
				d1 = FXV_FlameDir();
				dy = prev.y - d1.y;
				dx = prev.x - d1.x;
				dz = prev.z - d1.z;
			}
		}
		prev = d1;
		for (int k = 0; k < 8; k++) {
			size = Float_MaxSS(125.0f, size - FXV_Rand(40.0, 60.0));
			dist = FXV_Rand(0.079999998211860657, 0.23999999463558197) * size + dist;
			float s = FXV_Rand(0.05000000074505806, 0.0);
			d1 = FX_Unit((vec3_t){ d2.x * s + d1.x, d2.y * s + d1.y, d2.z * s + d1.z });
			s = FXV_Rand(125.0, 275.0);
			float vx = d1.x * s;
			float vy = d1.y * s;
			float vz = d1.z * s;
			float o1 = FXV_Rand(30.0, -15.0);
			float o2 = FXV_Rand(30.0, -15.0);
			float o3 = FXV_Rand(30.0, -15.0);
			float wx = d1.x * dist;
			float wy = d1.y * dist;
			float wz = d1.z * dist;
			p->initPosMin.x = wx * scale + pos->x + o3 * scale;
			p->initPosMin.y = wy * scale + pos->y + o2 * scale;
			p->initPosMin.z = wz * scale + pos->z + o1 * scale;
			p->initPosMax = p->initPosMin;
			vx = vx * scale;
			vy = vy * scale;
			vz = vz * scale;
			p->initVelMin = (vec3_t){ vx, vy, vz };
			FXV_ShipBrake(p, vel, 0.10000002384185791, 0.69999998807907104);
			p->sizeIni = FXV_Rand(0.10000000894069672, 0.20000000298023224) * size * scale;
			p->sizeEnd = FXV_Rand(0.099999964237213135, 0.80000001192092896) * size * scale;
			p->lifeTime = FXV_Rand(0.079999983310699463, 0.72000002861022949) * time;
			FXV_RandRot(p);
			p->sizeLife = 1.0f;
			FXV_SetSprite(p, 0x79);
			p->rotVar = FXV_Rand(1.5707963705062866, -0.78539818525314331);
			CGameParticles_Apply(p);
			CGameParticles_SetColorRamp(p, FXV_RampN(FXV_XFIRE_RAMPS, 3));
			float et = (float)k * 0.02f * time;
			CGameParticles_EmitDt(p, 1, 1, FXV_Rand(0.019999995827674866, 0.18000000715255737) + et);

			o1 = FXV_Rand(20.0, -10.0);
			o2 = FXV_Rand(20.0, -10.0);
			o3 = FXV_Rand(20.0, -10.0);
			p->initPosMin.y = wy * scale + pos->y + o2 * scale;
			p->initPosMin.z = wz * scale + pos->z + o1 * scale;
			p->initPosMin.x = wx * scale + pos->x + o3 * scale;
			p->initPosMax = p->initPosMin;
			p->initVelMin = (vec3_t){ vx, vy, vz };
			FXV_ShipBrake(p, vel, 0.14999997615814209, 0.75);
			p->sizeIni = FXV_Rand(0.10000000894069672, 0.20000000298023224) * size * scale;
			p->sizeEnd = size * scale;
			p->lifeTime = FXV_Rand(0.099999964237213135, 0.60000002384185791) * time;
			FXV_RandRot(p);
			p->rotVar = FXV_Rand(1.0471975803375244, -0.5235987901687622);
			p->sizeLife = FXV_Rand(0.070000007748603821, 0.10999999940395355);
			p->spriteEnd = 0x21;
			p->spriteIni = (byte)((byte)(int)FXV_Rand(6.0, 0.0) + 1 + k);
			CGameParticles_Apply(p);
			CGameParticles_SetColorRamp(p, FXV_RandRamp(FX_ShipExplosionRamps));
			CGameParticles_EmitDt(p, 1, 1, et);
		}
	}
}

/* the puffs on a sphere around the explosion: 4 x 4 directions, each with up to four kinds by chance */
static uint32_t FX_ShipExplosionRamp[9] = { 0xff000000, 0xaafeee90, 0x82ef7d38, 0x96bc4110, 0xb97a2a0a, 0xd23f1805, 0xeb0e0801, 0xff000000, 0xff000000 };

static uint32_t FX_ShipExplosionRamps2[18] = {
	0xff000000, 0xa4feee90, 0x60efa438, 0x40bc6410, 0x607a500a, 0x803f2805, 0xc40e0801, 0xff000000, 0xff000000,
	0xff565656, 0xaaffec82, 0xa0ef8228, 0xaa9a260f, 0xbb7c1f0c, 0xcc5e1706, 0xe1320a01, 0xff000000, 0xff000000,
};

static void FXV_ShipSphere(CGameParticles *p, const vec3_t *pos, const vec3_t *vel, float scale, float time) {
	const vec3_t one = { 1.0f, 1.0f, 1.0f };
	vec3_t dir;

	float a = 0.0f;
	do {
		float y0 = FXV_Rand(2.0943951606750488, -1.0471975803375244);
		float y1 = FXV_Rand(2.0943951606750488, -1.0471975803375244);
		float y2 = FXV_Rand(2.0943951606750488, -1.0471975803375244);
		float b = 0.0f;
		float rz = a + y2;
		float rx = y0 + 0.0f + 0.0f;
		do {
			float j1 = FXV_Rand(0.69999998807907104, -0.34999999403953552);
			float j2 = FXV_Rand(0.69999998807907104, -0.34999999403953552);
			vec3_t rot = { rz + j2, j1 + (y1 + b), rx };
			Vec3_Rotate(&dir, &one, &rot);
			if (crt_rand() & 1) {
				float o1 = FXV_Rand(100.0, -50.0);
				float o2 = FXV_Rand(100.0, -50.0);
				float o3 = FXV_Rand(100.0, -50.0);
				p->initPosMin.y = dir.y * 125.0f * scale + pos->y + o2 * scale;
				p->initPosMin.x = dir.x * 125.0f * scale + pos->x + o3 * scale;
				p->initPosMin.z = dir.z * 125.0f * scale + pos->z + o1 * scale;
				p->initPosMax = p->initPosMin;
				FXV_ShipSpeed(p, vel);
				p->sizeIni = FXV_Rand(100.0, 250.0) * scale;
				p->sizeEnd = FXV_Rand(100.0, 650.0) * scale;
				p->lifeTime = FXV_Rand(0.10000002384185791, 0.5) * time;
				FXV_RandRot(p);
				p->sizeLife = 1.0f;
				p->rotVar = FXV_Rand(0.44879895448684692, -0.22439947724342346);
				switch (crt_rand() % 3) {
				case 0:
					p->spriteIni = 0x79;
					break;
				case 1:
					p->spriteIni = 0x7a;
					break;
				case 2:
					p->spriteIni = 0x7c;
					break;
				}
				p->spriteEnd = p->spriteIni;
				CGameParticles_Apply(p);
				CGameParticles_SetColorRamp(p, FXV_RampN(FXV_XFIRE_RAMPS, 3));
				CGameParticles_EmitDt(p, 1, 1, FXV_Rand(0.19999998807907104, 0.25));
			}
			if (crt_rand() & 1) {
				float o1 = FXV_Rand(90.0, -45.0);
				float o2 = FXV_Rand(90.0, -45.0);
				float o3 = FXV_Rand(90.0, -45.0) * scale;
				o2 = o2 * scale;
				o1 = o1 * scale;
				float s = FXV_Rand(40.0, 80.0);
				p->initPosMin.x = s * dir.x * scale + pos->x + o3;
				p->initPosMin.y = s * dir.y * scale + pos->y + o2;
				p->initPosMin.z = s * dir.z * scale + pos->z + o1;
				p->initPosMax = p->initPosMin;
				FXV_ShipSpeed(p, vel);
				p->sizeIni = FXV_Rand(100.0, 50.0) * scale;
				p->sizeEnd = FXV_Rand(150.0, 500.0) * scale;
				p->lifeTime = FXV_Rand(0.049999982118606567, 0.40000000596046448) * time;
				FXV_RandRot(p);
				p->rotVar = FXV_Rand(0.5235987901687622, -0.2617993950843811);
				p->sizeLife = FXV_Rand(0.15000000596046448, 0.25);
				int r = crt_rand() % 6;
				p->spriteIni = (byte)(r + 0x21);
				p->spriteEnd = (byte)(r + 0x21 + 0xa);
				CGameParticles_Apply(p);
				CGameParticles_SetColorRamp(p, FXV_Ramp(FX_ShipExplosionRamps2));
				CGameParticles_EmitDt(p, 1, 1, FXV_Rand(0.25, 0.05000000074505806));
			}
			if (crt_rand() & 1) {
				float o1 = FXV_Rand(70.0, -35.0);
				float o2 = FXV_Rand(70.0, -35.0);
				float o3 = FXV_Rand(70.0, -35.0) * scale;
				o2 = o2 * scale;
				o1 = o1 * scale;
				float s = FXV_Rand(50.0, 55.0);
				p->initPosMin.x = s * dir.x * scale + pos->x + o3;
				p->initPosMin.y = s * dir.y * scale + pos->y + o2;
				p->initPosMin.z = s * dir.z * scale + pos->z + o1;
				p->initPosMax = p->initPosMin;
				float k = FXV_Rand(0.5, 2.1500000953674316);
				float inv = 1.0f / time;
				p->initVelMin.x = inv * (k * (p->initPosMin.x - pos->x));
				p->initVelMin.z = inv * (k * (p->initPosMin.z - pos->z));
				p->initVelMin.y = inv * (k * (p->initPosMin.y - pos->y));
				FXV_ShipBrake(p, vel, 0.049999997019767761, 0.15000000596046448);
				p->sizeIni = FXV_Rand(60.0, 115.0) * scale;
				p->sizeEnd = FXV_Rand(150.0, 500.0) * scale;
				p->rotIni = 0.0f;
				p->rotEnd = 0.0f;
				p->rotVar = 0.0f;
				p->lifeTime = FXV_Rand(0.10000002384185791, 0.5) * time;
				FXV_SetSprite(p, 0x75);
				p->sparkMaxSize = 1.0f;
				p->sizeLife = FXV_Rand(0.099999994039535522, 0.25);
				CGameParticles_Apply(p);
				CGameParticles_SetColorRamp(p, FXV_Ramp(FX_ShipExplosionRamp));
				CGameParticles_SetSparksOn(p, 1);
				CGameParticles_EmitDt(p, 1, 1, FXV_Rand(0.15000000596046448, 0.0));
				CGameParticles_SetSparksOn(p, 0);
			}
			if (crt_rand() & 1) {
				float o1 = FXV_Rand(80.0, -40.0);
				float o2 = FXV_Rand(80.0, -40.0);
				float o3 = FXV_Rand(80.0, -40.0);
				p->initPosMin.y = dir.y * 100.0f * scale + pos->y + o2 * scale;
				p->initPosMin.x = dir.x * 100.0f * scale + pos->x + o3 * scale;
				p->initPosMin.z = dir.z * 100.0f * scale + pos->z + o1 * scale;
				p->initPosMax = p->initPosMin;
				float k = FXV_Rand(0.5, 2.9500000476837158);
				float inv = 1.0f / time;
				p->initVelMin.x = (p->initPosMin.x - pos->x) * k * inv;
				p->initVelMin.y = (p->initPosMin.y - pos->y) * k * inv;
				p->initVelMin.z = (p->initPosMin.z - pos->z) * k * inv;
				FXV_ShipBrake(p, vel, 0.20000004768371582, 1.0499999523162842);
				p->sizeIni = FXV_Rand(100.0, 50.0) * scale;
				p->sizeEnd = FXV_Rand(200.0, 750.0) * scale;
				p->lifeTime = FXV_Rand(0.099999964237213135, 0.60000002384185791) * time;
				FXV_RandRot(p);
				p->rotVar = FXV_Rand(1.5707963705062866, -0.78539818525314331);
				p->sizeLife = FXV_Rand(0.070000007748603821, 0.10999999940395355);
				p->spriteEnd = 0x21;
				p->spriteIni = (byte)(crt_rand() % 4 + 1);
				CGameParticles_Apply(p);
				CGameParticles_SetColorRamp(p, FXV_RandRamp(FX_ShipExplosionRamps));
				CGameParticles_EmitDt(p, 1, 1, FXV_Rand(0.30000001192092896, 0.0));
			}
			b = b + FXV_PI_2;
		} while (6.2831853071795862 > (double)b);
		a = a + FXV_PI_2;
	} while (6.2831853071795862 > (double)a);
}

static uint32_t FX_ShipExplosionTable[28] = {
	0xff000000, 0x80ffffff, 0x40ffffff, 0x40ffffff, 0x40ffffff, 0x80c4c4c4, 0xc4808080, 0xff000000, 0xff000000,
	0xff000000, 0x80ffc0c0, 0x40ffc0c0, 0x40ffc0c0, 0x40ffc0c0, 0x80c48c8c, 0xc4805050, 0xff000000, 0xff000000,
	0xff000000, 0x80ffe8c0, 0x40ffe8c0, 0x40ffe8c0, 0x40ffe8c0, 0x80c4a58c, 0xc4806450, 0xff000000, 0xff000000,
};

void FX_ShipExplosion(vec3_t pos, vec3_t vel, float scale, float time, int numFlames, int collision) {
	vec3_t from, to, hit, tmp;
	CCollisionTri tri;

	CHECK(scale >= 0.01f);
	CHECK(time >= 0.01f);
	CHECK(numFlames >= 0);
	if (!FX_IsVisible(pos.x, pos.y, pos.z, scale * 2000.0f, g_ShipExplosionMaxDist, NULL, 1)) {
		return;
	}
	if (collision) {
		/* pushed out of the wall it is in: the first axis on which a line through it hits */
		float up = scale * 500.0f;
		float down = scale * -500.0f;
		vec3_t n = { 0.0f, 0.0f, 0.0f };
		to = (vec3_t){ pos.x + 0.0f, pos.y + up, pos.z + 0.0f };
		from = (vec3_t){ pos.x + 0.0f, down + pos.y, pos.z + 0.0f };
		byte hitOk = Engine_TestLine(&from, &to, &hit, &tri);
		if (!hitOk) {
			to = (vec3_t){ pos.x + 0.0f, pos.y + 0.0f, up + pos.z };
			from = (vec3_t){ pos.x + 0.0f, pos.y + 0.0f, down + pos.z };
			hitOk = Engine_TestLine(&from, &to, &hit, &tri);
		}
		if (!hitOk) {
			to = (vec3_t){ up + pos.x, pos.y + 0.0f, pos.z + 0.0f };
			from = (vec3_t){ down + pos.x, pos.y + 0.0f, pos.z + 0.0f };
			hitOk = Engine_TestLine(&from, &to, &hit, &tri);
		}
		if (hitOk) {
			n = tri.plane.n;
		}
		pos.x = n.x * 750.0f * scale + pos.x;
		pos.y = n.y * 750.0f * scale + pos.y;
		pos.z = n.z * 750.0f * scale + pos.z;
	}

	/* debris with smoke, then debris on fire (only with the smoke ones) */
	int flag = numFlames > 0;
	int n1 = crt_rand() % 2 * numFlames + flag;
	if (n1 > 0) {
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXSmokeDebrisFile);
		FXV_ShipDebris(sys, n1, &pos, scale, time, 0.55000001192092896, 0.80000001192092896, 1650.0, 700.0, 25.0f,
			8.0, 24.0, 25.0, 150.0);
	}
	int n2 = crt_rand() % 2 * numFlames + flag;
	if (n1 > 0) {
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXFiredDebrisFile);
		if (n2 > 0) {
			FXV_ShipDebris(sys, n2, &pos, scale, time, 0.69999992847442627, 0.60000002384185791, 1200.0, 950.0,
				50.0f, 4.0, 28.0, 30.0, 130.0);
		}
	}

	/* the explosion itself: a debris of its own system, its particles */
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXShipExplosionFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	const vec3_t *r = FX_RandomDir(&tmp);
	vec3_t at = { r->x + pos.x, r->y + pos.y, r->z + pos.z };
	CGameModel_SetPos(Debris_Model(d), &at);
	d->life = time * 1.5f;
	cDebrisSys_Drop(sys, d);
	cObjElementFXParticleSystem *el = (cObjElementFXParticleSystem *)d->fx;
	if (!el) {
		return;
	}
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	CGameParticles *p = &el->particles;
	CGameParticles_SetColorRampOn(p, 1);
	p->birthRate = 0;
	p->initPosMin = pos;
	p->initPosMax = p->initPosMin;
	p->initVelMin = vel;
	p->initVelMax = p->initVelMin;
	p->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initAccMax = p->initAccMin;
	p->lifeTime = FXV_Rand(0.04999995231628418, 0.60000002384185791) * time;
	p->sizeLife = FXV_Rand(0.019999995827674866, 0.20000000298023224);
	p->sizeIni = FXV_Rand(50.0, 350.0) * scale;
	p->sizeEnd = FXV_Rand(500.0, 1700.0) * scale;
	float t = FXV_Rand(FXV_PI2, 0.0);
	p->rotVar = 0.39269909f;
	p->rotIni = t;
	p->rotEnd = t;
	CGameParticles_SetColorRamp(p, FXV_RampN(FX_ShipExplosionTable, 3));
	FXV_SetSprite(p, 0x72);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);

	FXV_ShipFlames(p, numFlames, &pos, &vel, scale, time);

	/* four times a flash and a puff around the explosion */
	for (int i = 0; i < 4; i++) {
		vec3_t dir;
		FX_RandomDir(&dir);
		float s = FXV_Rand(50.0, 75.0);
		p->initPosMin.y = s * dir.y * scale + pos.y;
		p->initPosMin.x = s * dir.x * scale + pos.x;
		p->initPosMin.z = s * dir.z * scale + pos.z;
		p->initPosMax = p->initPosMin;
		FXV_ShipSpeed(p, &vel);
		p->sizeIni = FXV_Rand(50.0, 50.0) * scale;
		p->sizeEnd = FXV_Rand(145.0, 805.0) * scale;
		p->lifeTime = FXV_Rand(0.029999971389770508, 0.93000000715255737) * time;
		FXV_RandRot(p);
		p->rotVar = FXV_Rand(3.1415927410125732, -1.5707963705062866);
		p->spriteIni = 1;
		p->spriteEnd = 0x21;
		p->sizeLife = FXV_Rand(0.070000000298023224, 0.10000000149011612);
		CGameParticles_Apply(p);
		CGameParticles_SetColorRamp(p, FXV_Ramp(FX_ShipExplosionRamps));
		CGameParticles_EmitDt(p, 1, 1, FXV_Rand(0.10000000149011612, 0.0) * time);

		dir = *FX_RandomDir(&tmp);
		s = FXV_Rand(50.0, 145.0);
		p->initPosMin.y = s * dir.y * scale + pos.y;
		p->initPosMin.x = s * dir.x * scale + pos.x;
		p->initPosMin.z = s * dir.z * scale + pos.z;
		p->initPosMax = p->initPosMin;
		FXV_ShipSpeed(p, &vel);
		p->sizeIni = FXV_Rand(100.0, 250.0) * scale;
		p->sizeEnd = FXV_Rand(100.0, 850.0) * scale;
		p->lifeTime = FXV_Rand(0.050000011920928955, 0.80000001192092896) * time;
		FXV_RandRot(p);
		p->sizeLife = 1.0f;
		p->rotVar = FXV_Rand(1.0471975803375244, -0.5235987901687622);
		FXV_SetSprite(p, (byte)(crt_rand() % 4 + 0x79));
		CGameParticles_Apply(p);
		CGameParticles_SetColorRamp(p, FXV_RampN((&FXV_XFIRE_RAMPS[9]), 2));
		CGameParticles_EmitDt(p, 1, 1, FXV_Rand(0.11999998986721039, 0.20000000298023224) * time);
	}

	FXV_ShipSphere(p, &pos, &vel, scale, time);

	/* most times a second ball */
	if (crt_rand() % 4) {
		p->initPosMin = pos;
		p->initPosMax = p->initPosMin;
		p->initVelMin = vel;
		p->initVelMax = p->initVelMin;
		p->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		p->initAccMax = p->initAccMin;
		p->lifeTime = FXV_Rand(0.040000021457672119, 0.25999999046325684) * time;
		p->sizeLife = FXV_Rand(0.040000006556510925, 0.14000000059604645);
		p->sizeIni = FXV_Rand(100.0, 600.0) * scale;
		p->sizeEnd = FXV_Rand(200.0, 1450.0) * scale;
		t = FXV_Rand(FXV_PI2, 0.0);
		p->rotIni = t;
		p->rotEnd = t;
		p->rotVar = 0.39269909f;
		CGameParticles_SetColorRamp(p, FXV_RampN(FX_ShipExplosionTable, 3));
		FXV_SetSprite(p, 0x72);
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);
	}
}

/* ---- Sputnik ---- */

/* rand() % n of the original (the sign kept) */
static inline int FXV_RandMod(int n) {
	return crt_rand() % n;
}

/* the phase 1 lets the ship appear (FX 8, parameter 1 - c), phase 2 vanish; the rays run from the Sputnik to random
 * points of the ship while 0 < c < 1 */
static void cObjElementFXVehicleSputnik_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXVehicleSputnik *self = (cObjElementFXVehicleSputnik *)base;
	(void)dt;
	cEntity *e = self->entity;
	if (e && (Entity_Removed(e) || !e->active)) {
		Obj_SetEffect(e, MODEL_EFFECT_NONE);
		Obj_SetEffectParam(self->entity, 0, 0.0f);
		self->entity = NULL;
	}
	e = self->entity;
	if (!e || !e->active) {
		return;
	}
	CGameModel *em = Obj_GetModel(e);
	CGameModel *model = Debris_Model(debris);
	vec3_t pos, rot;
	CHECK(CGameModel_IsLoaded(em));
	CGameModel_GetPos(em, &pos);
	CGameModel_GetRot(em, &rot);
	CGameModel_SetPos(model, &pos);
	CGameModel_SetRot(model, &rot);
	CGameModel_SetSortZ(model, -39.0f, 0.0f);
	float d = cTimer_GetDelta(&g_pWorld->timer);
	float t = self->time + d;
	self->time = t;
	self->time2 = self->time2 + d;

	/* c: the progress of the phase, c2 the same with the first 0.1 faster */
	int phase = self->phase;
	float x = 0.0f;
	if (phase == 1) {
		x = (t - 0.8f) / (self->duration - 0.8f);
	} else if (phase == 2) {
		x = (t - 0.8f) / (self->duration - 1.6f);
	}
	float c = Float_MaxSS(0.0f, x);
	x = 0.0f;
	if (phase == 1) {
		if (0.9f > t) {
			x = (t - 0.8f) * 10.0f;
		} else {
			x = (t - 0.8f) / (self->duration - 0.8f);
		}
	} else if (phase == 2) {
		if (0.9f > t) {
			x = (t - 0.8f) * 10.0f;
		} else {
			x = (t - 0.8f) / (self->duration - 1.6f);
		}
	}
	float c2 = Float_MaxSS(0.0f, x);
	CHECK(phase > 0);
	if (c >= 0.0f && 1.0f >= c) {
		Obj_SetEffect(self->entity, MODEL_EFFECT_NOISE);
		if (self->phase == 1) {
			Obj_SetEffectParam(self->entity, 0, 1.0f - c);
		} else {
			Obj_SetEffectParam(self->entity, 0, c);
		}
	} else if (self->phase == 1) {
		float v = c - 1.05f;
		if (1.05f > c) {
			v = v * 20.0f;
		} else {
			v = 1.0f - (v + v);
		}
		Obj_SetEffect(self->entity, MODEL_EFFECT_ELECTRIC);
		Obj_SetEffectParam(self->entity, 0, v * 0.5f);
	} else if (self->phase == 2 && c2 > 1.0f) {
		Entity_SetActive(self->entity, 0);
	}

	HashTable *ht = &g_pWorld->entities;
	HashNode *node = HashTable_Find(ht, "Sputnik", HashTable_Hash(ht, "Sputnik"));
	if (!node || !node->value || self->sputnikNode == -1) {
		return;
	}
	cEntity *sp = node->value;
	CGameModel *spm = Obj_GetModel(sp);
	matrix_t mat;
	CHECK(CGameModel_IsLoaded(spm));
	const matrix_t *m = CGameModel_NodeMatrix(spm, &mat, (uint32_t)self->sputnikNode, 1, NULL, 0);
	vec3_t spos;
	spos.x = m->m[12];
	spos.y = m->m[13];
	spos.z = m->m[14];
	CGameParticles *ps = &self->particles;
	int np = CGameParticles_NumParticles(ps);
	/* the model the rays start on: the ship, after a while one of its motors */
	CGameModel *src = em;

	if (c > 0.0f && 1.0f > c) {
		for (int i = 0; i < FXVSPUTNIK_TRAILS; i++) {
			CGameTrail *tr = &self->trails[i];
			CGameTrail_Reset(tr);
			float u = FXV_Rand(1.0, 0.0);
			float v = FXV_Rand(1.0, 0.0);
			CGameTrail_SetUV(tr, v, u);
			int a;
			if (0.5f > c2) {
				a = 0;
			} else {
				a = (int)(((c2 - 0.5f) + (c2 - 0.5f)) * 255.0f);
			}
			CGameTrail_SetTransp(tr, (byte)a);
		}
		int trail = 0;
		int inTrail = 0;
		for (int i = 0; i < FXVSPUTNIK_RANDS; i++) {
			float life = self->rayLife[i] - d;
			self->rayLife[i] = life;
			if (life > 0.0f) {
				vec3_t *rp = &self->rayPos[i];
				vec3_t dv, n, off, o, tgt, p;
				double r = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
				dv.x = rp->x - spos.x;
				dv.z = rp->z - spos.z;
				float w = (float)(r + r - 1.0) + self->rayWidth[i];
				dv.y = rp->y - spos.y;
				FX_Normalize(&n, dv.x, dv.y, dv.z);
				float s = FXV_Rand(48.0, 8.0);
				off.x = -10.0f;
				off.y = -20.0f;
				off.z = 0.0f;
				float nx = n.x * s;
				float ny = n.y * s;
				float nz = n.z * s;
				Vec3_Rotate(&o, &off, CGameModel_GetRot(spm, &rot));
				tgt.x = spos.x + o.x + nx;
				tgt.y = o.y + spos.y + ny;
				tgt.z = o.z + spos.z + nz;
				p = *rp;
				float f = self->rayLife[i] / self->rayLifeMax[i] + FXV_Rand(0.4000000059604645, -0.20000000298023224);
				float af = 0.0f >= f ? 0.0f : Float_MinSS(1.0f, f);
				if (!FXV_RandMod(16)) {
					goto next;
				}
				CGameTrail *tr = &self->trails[trail];
				uint32_t cg, cb;
				float w2;
				int r0 = crt_rand();
				if (trail < 2) {
					cg = (uint32_t)(r0 % 16 + 0x18) << 8;
					cg |= (uint32_t)(FXV_RandMod(16) + 0x18) << 16;
					cb = (uint32_t)(FXV_RandMod(32) + 0x80);
					CGameTrail_Add(tr, tgt, w, cb | (uint32_t)(-1 - (int)(af * 128.0f)) << 24 | cg);
					cg = (uint32_t)(FXV_RandMod(16) + 0x18) << 8;
					cg = (uint32_t)(FXV_RandMod(16) + 0x18) << 16 | cg;
					cb = (uint32_t)(FXV_RandMod(32) + 0x80);
					w2 = w;
				} else {
					cg = (uint32_t)(r0 % 16 + 0x20) << 8;
					cg |= (uint32_t)(FXV_RandMod(16) + 0x20) << 16;
					cb = (uint32_t)(FXV_RandMod(32) + 0x40);
					CGameTrail_Add(tr, tgt, w, cb | (uint32_t)(-1 - (int)(af * 128.0f)) << 24 | cg);
					float s2 = FXV_Rand(5.0, 30.0);
					cg = (uint32_t)(FXV_RandMod(16) + 0x20) << 8;
					cg = (uint32_t)(FXV_RandMod(16) + 0x20) << 16 | cg;
					cb = (uint32_t)(FXV_RandMod(32) + 0x40);
					w2 = s2 * w;
				}
				CGameTrail_Add(tr, p, w2, cb | cg | 0xff000000u);
				if (i & 1) {
					float ss = FXV_Rand(0.10000002384185791, 1.0);
					uint32_t color = (uint32_t)(-1 - (int)((c2 - 1.0f) * af * -32.0f)) << 24 | 0x2020ff;
					float rad = Obj_GetRadius(self->entity);
					CGameParticles_SetParticle(ps, i, p.x, p.y, p.z, rad * ss, color, 5, 0.0f);
				}
			} else {
				CGameParticles_Kill(ps, i);
				float rd = self->rayDelay[i] - d;
				self->rayDelay[i] = rd;
				if (!(rd > 0.0f)) {
					/* a new ray */
					vec3_t dir;
					self->rayDelay[i] = FXV_Rand(0.10000000894069672, 0.05000000074505806);
					float l = FXV_Rand(0.20000000298023224, 0.05000000074505806);
					self->rayLifeMax[i] = l;
					self->rayLife[i] = l;
					self->rayWidth[i] = FXV_Rand(3.5, 4.0);
					if (!FXV_RandMod(12) && self->numExhausts > 0) {
						int k = self->numExhausts > 1 ? FXV_RandMod(self->numExhausts) : 0;
						src = FXV_ExhaustModel(self->entity, &self->exhausts[k]);
					}
					CGameModel_Vertices(src, &self->rayPos[i], NULL, NULL, 1, -1, NULL, 0xffffffffu, NULL);
					float s = FXV_Rand(8.0, 0.0);
					FX_RandomDir(&dir);
					self->rayPos[i].x = s * dir.x + self->rayPos[i].x;
					self->rayPos[i].y = dir.y * s + self->rayPos[i].y;
					self->rayPos[i].z = dir.z * s + self->rayPos[i].z;
				}
			}
		next:
			inTrail++;
			if (inTrail >= 15) {
				inTrail = 0;
				trail++;
			}
		}
	} else {
		for (int i = 0; i < FXVSPUTNIK_TRAILS; i++) {
			CGameTrail_Clear(&self->trails[i]);
		}
		for (int i = 0; i < FXVSPUTNIK_RANDS; i++) {
			CGameParticles_Kill(ps, i);
		}
	}

	/* the glow at the Sputnik: the last 8 particles */
	int idx = np - 1;
	for (int k = 0; k < 8; k++) {
		if (c > 0.0f && 1.0f > c) {
			vec3_t dir, off, o;
			float s = FXV_Rand(10.0, 2.0);
			FX_RandomDir(&dir);
			float rx = dir.x * s;
			float ry = dir.y * s;
			float rz = dir.z * s;
			off.x = -10.0f;
			off.y = -19.0f;
			off.z = 0.0f;
			Vec3_Rotate(&o, &off, CGameModel_GetRot(spm, &rot));
			float gx = spos.x + o.x + rx;
			float gy = o.y + spos.y + ry;
			float gz = o.z + spos.z + rz;
			float size = FXV_Rand(16.0, 32.0);
			uint32_t color = (uint32_t)(-1 - (int)((c2 - 1.0f) * -64.0f)) << 24 | 0x4040ff;
			CGameParticles_SetParticle(ps, idx, gx, gy, gz, size, color, 5, 0.0f);
		} else {
			CGameParticles_Kill(ps, idx);
		}
		idx--;
	}
}

/* ---- Accelerator ---- */

/* the rays grow from the ship along their curves and fade out */
static void cObjElementFXVehicleAccelerator_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXVehicleAccelerator *self = (cObjElementFXVehicleAccelerator *)base;
	FXV_CheckEntity(&self->owner);
	if (!self->owner || !self->owner->active) {
		return;
	}
	CGameModel_SetSortZ(Debris_Model(debris), -47.0f, 0.0f);
	cEntity *e = self->owner;
	if (!e || !Entity_IsType(e, TYEN_CAR) || !FXV_Veh(e)->wl.obj.cam) {
		if (CGameParticles_IsVisible(&self->particles)) {
			for (int i = 0; i < FXVACCEL_TRAILS; i++) {
				self->fade[i] = 0.0f;
				CGameTrail_Clear(&self->trails[i]);
			}
			CGameParticles_KillAll(&self->particles);
			CGameParticles_SetVisible(&self->particles, 0);
		}
		return;
	}
	CGameModel *model = Obj_GetModel(e);
	CHECK(model && CGameModel_IsLoaded(model));

	for (int i = 0; i < FXVACCEL_TRAILS; i++) {
		if (self->delay[i] > 0.0f) {
			self->delay[i] = self->delay[i] - dt;
			continue;
		}
		if (!(self->fade[i] > 0.0f)) {
			continue;
		}
		float f = self->fade[i] - dt;
		self->fade[i] = f;
		if (f > 0.0f) {
			CGameTrail *t = &self->trails[i];
			FXVAccelCtrl *c = &self->ctrl[i];
			vec3_t rd, rot, p0, p3, pos, coefs[4], pts[4];

			int a = 0xff - (int)(Float_MinSS(1.0f, f / 0.06f) * 255.0f);
			CGameTrail_Reset(t);
			CGameTrail_SetTransp(t, (byte)a);
			/* the inner control points wander */
			FX_RandomDir(&rd);
			c->a.x = c->a.x + rd.x * dt * 20.0f;
			c->a.y = c->a.y + rd.y * dt * 20.0f;
			c->a.z = c->a.z + rd.z * dt * 20.0f;
			FX_RandomDir(&rd);
			c->b.x = c->b.x + rd.x * dt * 20.0f;
			c->b.y = c->b.y + rd.y * dt * 20.0f;
			c->b.z = c->b.z + rd.z * dt * 20.0f;
			Vec3_Rotate(&p0, &self->from[i], CGameModel_GetRot(model, &rot));
			Vec3_Rotate(&p3, &self->to[i], CGameModel_GetRot(model, &rot));
			CGameModel_GetPos(model, &pos);
			pts[0].x = p0.x + pos.x;
			pts[0].y = pos.y + p0.y;
			pts[0].z = pos.z + p0.z;
			CGameModel_GetPos(model, &pos);
			pts[3].x = p3.x + pos.x;
			pts[3].y = pos.y + p3.y;
			pts[3].z = pos.z + p3.z;
			float dz = (pts[3].z - pts[0].z) * 0.33333334f;
			float dx = (pts[3].x - pts[0].x) * 0.33333334f;
			float dy = (pts[3].y - pts[0].y) * 0.33333334f;
			pts[1].z = c->a.z + (pts[0].z + dz);
			pts[1].x = pts[0].x + dx + c->a.x;
			pts[1].y = pts[0].y + dy + c->a.y;
			pts[2].x = pts[0].x + dx * 2.0f + c->b.x;
			pts[2].z = pts[0].z + dz * 2.0f + c->b.z;
			pts[2].y = pts[0].y + dy * 2.0f + c->b.y;
			CGameTrail_BezierCoefs(t, 4, pts, coefs);

			int n = CGameTrail_GetMaxTrails(t);
			for (int j = 0; j < n - 1; j++) {
				vec3_t p;
				CGameTrail_BezierPoint(t, 4, coefs, &p, (float)j / (float)(n - 2));
				if (j > 0 && j < n - 2) {
					float r = FXV_Rand(5.800000190734863, 7.800000190734863);
					FX_RandomDir(&rd);
					p.x = p.x + rd.x * r;
					p.y = p.y + rd.y * r;
					p.z = p.z + rd.z * r;
				}
				float w = FXV_Rand(20.0, 55.0);
				int c1 = crt_rand() % 3;
				int c2 = crt_rand() % 3;
				int fade = (int)((float)j / (float)n * -192.0f);
				uint32_t color = (uint32_t)(c2 + 0x24) << 16 | (uint32_t)(0x10 - fade) << 24
					| (uint32_t)(c1 + 0x24) << 8 | 0x5a;
				CGameTrail_Add(t, p, self->width[i] * w, color);
			}

			/* the glow at the start of the ray */
			CGameParticles *ps = &self->particles;
			CGameParticles_SetVisible(ps, 1);
			int a1 = a + 0x20;
			if (a1 > 0xff) {
				a1 = 0xff;
			}
			CGameParticles_SetParticle(ps, i * 2, pts[0].x, pts[0].y, pts[0].z, self->width[i] * 164.0f,
				(uint32_t)a1 << 24 | 0xc0c0ff, 2, 0.0f);
			int a2 = a + 0xc0;
			if (a2 > 0xff) {
				a2 = 0xff;
			}
			float r = FXV_Rand(6.2831854820251465, 0.0);
			CGameParticles_SetParticle(ps, i * 2 + 1, pts[0].x, pts[0].y, pts[0].z, self->width[i] * 850.0f,
				(uint32_t)a2 << 24 | 0x3030ff, 5, r);
		}
		if (!(self->fade[i] > 0.0f)) {
			CGameTrail_Clear(&self->trails[i]);
			CGameParticles_KillRange(&self->particles, i * 2, i * 2 + 2);
		}
	}
}

/* ---- IsAlive (vf02) ---- */

/* IsAlive; the ones of FiredDebris, SmokeDebris, Accelerator and Spawn are FXMisc_Alive, the DeathTrail is the
 * code of cObjElementFXDevastatorShoot (Update, IsAlive, Reset) */
static byte cObjElementFXFiredEntity_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys) {
	cObjElementFXFiredEntity *self = (cObjElementFXFiredEntity *)base;
	byte r = cDebris_Update(debris, sys);
	if (!r) {
		return r;
	}
	/* the smoke of a gone entity ends within 1.5 */
	cEntity *e = self->entity;
	if (!e || Entity_Removed(e) || !e->active) {
		if (debris->life > 1.5f) {
			debris->life = 1.5f;
		}
	}
	return 1;
}

static byte cObjElementFXVehicleDamage_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys) {
	cObjElementFXVehicleDamage *self = (cObjElementFXVehicleDamage *)base;
	cEntity *e;
	if (self->active && (e = self->entity) != NULL && e->active) {
		self->life = 0;
		self->lifePct = 100;
		int maxLife = g_StdMaxLife;
		if (Entity_IsType(e, TYEN_WITH_LIFE)) {
			self->life = Veh(self->entity)->wl.life;
		}
		if (Entity_IsType(self->entity, TYEN_CAR)) {
			maxLife = Veh(self->entity)->maxLife;
		}
		int life = self->life;
		int pct = life * 100 / maxLife;
		self->lifePct = pct;
		/* the effect lives on while the life is low */
		if (life <= 0 || pct >= g_FXSetOnFireLifePercent) {
			cVehicleEffects_ClearDamage(FXVeh_Effects(self->entity));
			self->active = 0;
		} else {
			debris->life = 1.5f;
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

static byte cObjElementFXVehicleEMI_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys) {
	cObjElementFXVehicleEMI *self = (cObjElementFXVehicleEMI *)base;
	if (!self->entity) {
		return 0;
	}
	/* the debris lives as long as the interference */
	debris->life = Veh(self->entity)->emi;
	if (cDebris_Update(debris, sys)) {
		return 1;
	}
	cEntity *e = self->entity;
	if (e && e->active) {
		Obj_SetEffect(e, MODEL_EFFECT_NONE);
	}
	return 0;
}

static byte cObjElementFXVehicleFire_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys) {
	cObjElementFXVehicleFire *self = (cObjElementFXVehicleFire *)base;
	cEntity *e = self->entity;
	if (!e) {
		return 0;
	}
	float t = WLife(e)->fireRemain;
	if (!(0.0f >= t)) {
		debris->life = t;
		if (cDebris_Update(debris, sys)) {
			return 1;
		}
		e = self->entity;
		if (!e) {
			return 0;
		}
	}
	if (e->active) {
		Obj_SetEffect(e, MODEL_EFFECT_NONE);
	}
	return 0;
}

static byte cObjElementFXVehicleSputnik_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys) {
	cObjElementFXVehicleSputnik *self = (cObjElementFXVehicleSputnik *)base;
	if (!self->entity) {
		return 0;
	}
	byte changed = self->entity->active != self->active;
	if (cDebris_Update(debris, sys) && !changed) {
		return 1;
	}
	Obj_SetEffect(self->entity, MODEL_EFFECT_NONE);
	Obj_SetEffectParam(self->entity, 0, 0.0f);
	return 0;
}

/* ---- Start (vf03) ---- */

static void cObjElementFXFiredDebris_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXFiredDebris *self = (cObjElementFXFiredDebris *)base;
	CGameModel *model = Debris_Model(debris);
	int node = CGameModel_FindRenderNode(model, "Dummy01");
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CHECK(self->particles.ParticleSystem);
	self->first = 1;
	self->interval = 0.1f;
	self->time = 0.0f;
	self->minDist = 100.0f;
	self->last.x = 0.0f;
	self->last.y = 0.0f;
	self->last.z = 0.0f;
}

static void cObjElementFXFiredEntity_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXFiredEntity *self = (cObjElementFXFiredEntity *)base;
	CGameModel *model = Debris_Model(debris);
	CGameParticles_Attach(&self->particles, model, FXV_Node(model, "Dummy01"));
	self->time = 0.0f;
	self->posValid = 0;
}

static void cObjElementFXSmokeDebris_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXSmokeDebris *self = (cObjElementFXSmokeDebris *)base;
	CGameModel *model = Debris_Model(debris);
	int node = CGameModel_FindRenderNode(model, "Dummy01");
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CHECK(self->particles.ParticleSystem);
	self->first = 1;
	self->interval = 0.03f;
	self->minDist = 50.0f;
	self->time = 0.0f;
	self->last.x = 0.0f;
	self->last.y = 0.0f;
	self->last.z = 0.0f;
}

static void cObjElementFXVehicleAccelerator_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleAccelerator *self = (cObjElementFXVehicleAccelerator *)base;
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXV_Node(model, "Dummy01"));
	CHECK(self->particles.ParticleSystem);
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

static void cObjElementFXVehicleDamage_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleDamage *self = (cObjElementFXVehicleDamage *)base;
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	self->model = model;
	CGameParticles_Attach(&self->parts1, model, FXV_Node(model, "Dummy01"));
	CGameParticles_Attach(&self->parts3, model, FXV_Node(model, "Dummy03"));
	CGameParticles_Attach(&self->parts2, model, FXV_Node(model, "Dummy02"));
	self->numExhausts = 0;
	self->active = 1;
	self->emitTime = 0.0f;
	self->trailTime = 0.0f;
	self->first = 1;
	self->seen = 0;
	self->level = 0.0f;
}

static void cObjElementFXVehicleDeathTrail_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleDeathTrail *self = (cObjElementFXVehicleDeathTrail *)base;
	(void)debris;
	CGameTrail_SetMaxTrails(&self->trail, 0x1e);
}

static void cObjElementFXVehicleEMI_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleEMI *self = (cObjElementFXVehicleEMI *)base;
	int node = FXV_NodeOrDebris(debris);
	if (node != -1) {
		CGameParticles_Attach(&self->particles, Debris_Model(debris), node);
	}
	for (int i = 0; i < self->numTrails; i++) {
		CGameTrail *t = &self->trails[i];
		self->rayTime[i] = 0.0f;
		CGameTrail_SetMaxTrails(t, 0x12);
		CGameTrail_SetSmooth(t, 0);
		CGameTrail_SetColor(t, 0xc0c0ff);
		CGameTrail_SetTexture(t, 0);
		CGameTrail_SetScale(t, 1.0f, 0.6f);
		CGameTrail_SetFade(t, 1);
		FXV_EMIClear(self, i);
	}
}

static void cObjElementFXVehicleFire_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleFire *self = (cObjElementFXVehicleFire *)base;
	CGameModel *model = Debris_Model(debris);
	int node = CGameModel_FindRenderNode(model, "Dummy01");
	if (node != -1) {
		CGameParticles_Attach(&self->particles, model, node);
	}
	CHECK(self->particles.ParticleSystem);
	node = CGameModel_FindRenderNode(model, "Dummy02");
	if (node != -1) {
		CGameParticles_Attach(&self->particlesOut, model, node);
	}
	CHECK(self->particlesOut.ParticleSystem);
	self->emitTime = 0.0f;
	self->numExhausts = 0;
}

static void cObjElementFXVehicleSpawn_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleSpawn *self = (cObjElementFXVehicleSpawn *)base;
	int node = FXV_NodeOrDebris(debris);
	if (node != -1) {
		CGameParticles_Attach(&self->particles, Debris_Model(debris), node);
	}
	for (int i = 0; i < FXVSPAWN_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 0x1f);
		CGameTrail_SetTexture(t, 0);
		CGameTrail_SetType(t, TRAIL_QUADS);
	}
	self->numRays = 0;
}

static void cObjElementFXVehicleSputnik_Start(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleSputnik *self = (cObjElementFXVehicleSputnik *)base;
	CGameModel *model = Debris_Model(debris);
	CHECK(CGameModel_IsLoaded(model));
	CGameParticles_Attach(&self->particles, model, FXV_Node(model, "Dummy01"));
	CHECK(self->particles.ParticleSystem);
	for (int i = 0; i < FXVSPUTNIK_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetTexture(t, i < 2 ? 0 : 0xb);
		CGameTrail_SetMaxTrails(t, 0x1f);
		CGameTrail_SetType(t, TRAIL_QUADS);
		CGameTrail_SetUVScale(t, 4.0f, 1.0f);
	}
}

/* ---- Reset (vf04) ---- */

static void cObjElementFXFiredDebris_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXFiredDebris *self = (cObjElementFXFiredDebris *)base;
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

static void cObjElementFXFiredEntity_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXFiredEntity *self = (cObjElementFXFiredEntity *)base;
	(void)debris;
	self->entity = NULL;
	self->time = 0.0f;
	self->posValid = 0;
}

static void cObjElementFXSmokeDebris_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXSmokeDebris *self = (cObjElementFXSmokeDebris *)base;
	(void)debris;
	self->first = 1;
	self->time = 0.0f;
	self->last.x = 0.0f;
	self->last.y = 0.0f;
	self->last.z = 0.0f;
	CGameParticles_KillAll(&self->particles);
}

static void cObjElementFXVehicleAccelerator_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleAccelerator *self = (cObjElementFXVehicleAccelerator *)base;
	(void)debris;
	for (int i = 0; i < FXVACCEL_TRAILS; i++) {
		self->fade[i] = 0.0f;
		self->delay[i] = 0.0f;
		CGameTrail_Clear(&self->trails[i]);
	}
}

static void cObjElementFXVehicleDamage_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleDamage *self = (cObjElementFXVehicleDamage *)base;
	(void)debris;
	self->entity = NULL;
	self->numExhausts = 0;
	self->active = 1;
	self->emitTime = 0.0f;
	self->trailTime = 0.0f;
	self->first = 1;
	self->seen = 0;
	self->level = 0.0f;
}

static void cObjElementFXVehicleEMI_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleEMI *self = (cObjElementFXVehicleEMI *)base;
	(void)debris;
	self->time = 0.0f;
	self->stepTime = 0.0f;
	for (int i = 0; i < self->numTrails; i++) {
		self->rayTime[i] = 0.0f;
		CGameTrail_Clear(&self->trails[i]);
		FXV_EMIClear(self, i);
	}
	CGameParticles_KillAll(&self->particles);
}

static void cObjElementFXVehicleFire_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleFire *self = (cObjElementFXVehicleFire *)base;
	(void)debris;
	self->entity = NULL;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	self->numExhausts = 0;
}

static void cObjElementFXVehicleSpawn_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleSpawn *self = (cObjElementFXVehicleSpawn *)base;
	(void)debris;
	self->time = 0.0f;
	self->stepTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
	for (int i = 0; i < FXVSPAWN_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	self->numRays = 0;
}

static void cObjElementFXVehicleSputnik_Reset(cObjElementFX *base, cDebris *debris) {
	cObjElementFXVehicleSputnik *self = (cObjElementFXVehicleSputnik *)base;
	(void)debris;
	self->time = 0.0f;
	self->time2 = 0.0f;
	CGameParticles_KillAll(&self->particles);
	for (int i = 0; i < FXVSPUTNIK_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	for (int i = 0; i < FXVSPUTNIK_RANDS; i++) {
		self->rayLife[i] = -1.0f;
		self->rayDelay[i] = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * (double)0.4f + (double)0.15f);
	}
}

/* ---- the setters ---- */

void cObjElementFXFiredEntity_SetEntity(cObjElementFXFiredEntity *self, cEntity *e) {
	vec3_t tmp;
	self->entity = e;
	CGameModel *model = FXV_Model(e);
	self->pos = *CGameModel_GetPos(model, &tmp);
	self->radius = CGameModel_Radius(model);
}

void cObjElementFXVehicleDamage_SetEntity(cObjElementFXVehicleDamage *self, cEntity *e) {
	vec3_t tmp;
	self->entity = e;
	CGameModel *model = FXV_Model(e);
	self->pos = *CGameModel_GetPos(model, &tmp);
	FXV_Exhausts(self->entity, &self->numExhausts, self->exhausts);
}

void cObjElementFXVehicleEMI_SetEntity(cObjElementFXVehicleEMI *self, cEntity *e) {
	self->entity = e;
	FXV_Model(e);
	FXV_Exhausts(self->entity, &self->numExhausts, self->exhausts);
}

void cObjElementFXVehicleFire_SetEntity(cObjElementFXVehicleFire *self, cEntity *e) {
	self->entity = e;
	FXV_Model(e);
	FXV_Exhausts(self->entity, &self->numExhausts, self->exhausts);
}

void cObjElementFXVehicleSputnik_SetEntity(cObjElementFXVehicleSputnik *self, cEntity *e) {
	vec3_t pos, rot;
	self->entity = e;
	CGameModel *model = FXV_Model(e);
	/* both results are not used */
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	self->active = self->entity->active;
	FXV_Exhausts(self->entity, &self->numExhausts, self->exhausts);
	self->sputnikNode = -1;
	HashTable *t = &g_pWorld->entities;
	HashNode *node = HashTable_Find(t, "Sputnik", HashTable_Hash(t, "Sputnik"));
	if (node && node->value) {
		cEntity *sp = node->value;
		self->sputnikNode = CGameModel_FindRenderNode(FXV_Model(sp), "Bip Spootnick MASTER");
	}
}

void cObjElementFXVehicleSputnik_SetC0(cObjElementFXVehicleSputnik *self, int v) {
	self->phase = v;
}

void cObjElementFXVehicleSputnik_SetC4(cObjElementFXVehicleSputnik *self, float v) {
	self->duration = v;
}

void cObjElementFXVehicleSpawn_SetEntity(cObjElementFXVehicleSpawn *self, cEntity *e) {
	self->entity = e;
	FXV_Model(e);
}

/* the damage effect drawn once with the ship it follows: only its smoke and sparks, the fire (parts1) hidden */
void FXVehicle_RenderAttached(void *element) {
	if (g_SplitScreenGame) {
		return;
	}
	cObjElementFXVehicleDamage *self = element;
	if (!self->entity || !self->model || !self->parts3.ParticleSystem || !self->parts2.ParticleSystem
		|| !self->parts1.ParticleSystem) {
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

/* the vtable of cObjElementFXVehicleDamage */
static const cObjElementFXVtbl cObjElementFXVehicleDamage_vftable = {
	.Free = cObjElementFXVehicleDamage_Free,
	.Update = cObjElementFXVehicleDamage_Update,
	.IsAlive = cObjElementFXVehicleDamage_IsAlive,
	.Start = cObjElementFXVehicleDamage_Start,
	.Reset = cObjElementFXVehicleDamage_Reset,
};

/* the vtable of cObjElementFXVehicleFire */
static const cObjElementFXVtbl cObjElementFXVehicleFire_vftable = {
	.Free = cObjElementFXVehicleFire_Free,
	.Update = cObjElementFXVehicleFire_Update,
	.IsAlive = cObjElementFXVehicleFire_IsAlive,
	.Start = cObjElementFXVehicleFire_Start,
	.Reset = cObjElementFXVehicleFire_Reset,
};

/* the vtable of cObjElementFXFiredEntity */
static const cObjElementFXVtbl cObjElementFXFiredEntity_vftable = {
	.Free = cObjElementFXFiredEntity_Free,
	.Update = cObjElementFXFiredEntity_Update,
	.IsAlive = cObjElementFXFiredEntity_IsAlive,
	.Start = cObjElementFXFiredEntity_Start,
	.Reset = cObjElementFXFiredEntity_Reset,
};

/* the vtable of cObjElementFXVehicleEMI */
static const cObjElementFXVtbl cObjElementFXVehicleEMI_vftable = {
	.Free = cObjElementFXVehicleEMI_Free,
	.Update = cObjElementFXVehicleEMI_Update,
	.IsAlive = cObjElementFXVehicleEMI_IsAlive,
	.Start = cObjElementFXVehicleEMI_Start,
	.Reset = cObjElementFXVehicleEMI_Reset,
};

/* the vtable of cObjElementFXFiredDebris */
static const cObjElementFXVtbl cObjElementFXFiredDebris_vftable = {
	.Free = cObjElementFXFiredDebris_Free,
	.Update = cObjElementFXFiredDebris_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXFiredDebris_Start,
	.Reset = cObjElementFXFiredDebris_Reset,
};

/* the vtable of cObjElementFXSmokeDebris */
static const cObjElementFXVtbl cObjElementFXSmokeDebris_vftable = {
	.Free = cObjElementFXSmokeDebris_Free,
	.Update = cObjElementFXSmokeDebris_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSmokeDebris_Start,
	.Reset = cObjElementFXSmokeDebris_Reset,
};

/* the vtable of cObjElementFXVehicleDeathTrail */
static const cObjElementFXVtbl cObjElementFXVehicleDeathTrail_vftable = {
	.Free = cObjElementFXVehicleDeathTrail_Free,
	.Update = cObjElementFXDevastatorShoot_Update,
	.IsAlive = cObjElementFXDevastatorShoot_IsAlive,
	.Start = cObjElementFXVehicleDeathTrail_Start,
	.Reset = cObjElementFXDevastatorShoot_Reset,
};

/* the vtable of cObjElementFXVehicleSputnik */
static const cObjElementFXVtbl cObjElementFXVehicleSputnik_vftable = {
	.Free = cObjElementFXVehicleSputnik_Free,
	.Update = cObjElementFXVehicleSputnik_Update,
	.IsAlive = cObjElementFXVehicleSputnik_IsAlive,
	.Start = cObjElementFXVehicleSputnik_Start,
	.Reset = cObjElementFXVehicleSputnik_Reset,
};

/* the vtable of cObjElementFXVehicleSpawn */
static const cObjElementFXVtbl cObjElementFXVehicleSpawn_vftable = {
	.Free = cObjElementFXVehicleSpawn_Free,
	.Update = cObjElementFXVehicleSpawn_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXVehicleSpawn_Start,
	.Reset = cObjElementFXVehicleSpawn_Reset,
};

/* the vtable of cObjElementFXVehicleAccelerator */
static const cObjElementFXVtbl cObjElementFXVehicleAccelerator_vftable = {
	.Free = cObjElementFXVehicleAccelerator_Free,
	.Update = cObjElementFXVehicleAccelerator_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXVehicleAccelerator_Start,
	.Reset = cObjElementFXVehicleAccelerator_Reset,
};

static cGVar GVar_FXShipExplosionFile;
static cGVar GVar_FXFiredDebrisFile;
cGVar GVar_FXSmokeDebrisFile;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_FXVehicleDamageSmokeStep, &GVar_FXVehicleDamageSmokeStep, "FXVehicleDamageSmokeStep", 0, "Vehicle Damage Smoke Effect Step", 350.0f)
INIT_GVAR_FLOAT(Init_GVar_FXFiredEntitySmokeStep, &GVar_FXFiredEntitySmokeStep, "FXFiredEntitySmokeStep", 0, "Fired Entity Smoke Effect Step", 1000.0f)
FINI_GVAR_STRING(Fini_GVar_FXFiredDebrisFile, &GVar_FXFiredDebrisFile)
INIT_GVAR_STRING(Init_GVar_FXFiredDebrisFile, Fini_GVar_FXFiredDebrisFile, &GVar_FXFiredDebrisFile, "FXFiredDebrisFile", 0, "Default Model for Fired Debris Effect", "Models/GFX/FiredDebris.M3D")
FINI_GVAR_STRING(Fini_GVar_FXSmokeDebrisFile, &GVar_FXSmokeDebrisFile)
INIT_GVAR_STRING(Init_GVar_FXSmokeDebrisFile, Fini_GVar_FXSmokeDebrisFile, &GVar_FXSmokeDebrisFile, "FXSmokeDebrisFile", 0, "Default Model for Smoke Debris Effect", "Models/GFX/SmokeDebris.M3D")
FINI_GVAR_STRING(Fini_GVar_FXShipExplosionFile, &GVar_FXShipExplosionFile)
INIT_GVAR_STRING(Init_GVar_FXShipExplosionFile, Fini_GVar_FXShipExplosionFile, &GVar_FXShipExplosionFile, "FXShipExplosionFile", 0, "Default Model for Ship Explosion", "Models/GFX/ShipExplosion.M3D")
INIT_GVAR_FLOAT(Init_GVar_ShipExplosionMaxDist, &GVar_ShipExplosionMaxDist, "ShipExplosionMaxDist", 0, "Maximun Distance to show the Ship Explosion Effect", 250000.0f)
