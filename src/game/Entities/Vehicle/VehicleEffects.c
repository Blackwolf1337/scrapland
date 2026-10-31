/*
 * cVehicleEffects, the effects a vehicle drives itself, see include/scrap/vehicleeffects.h.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/fxmisc.h"
#include "scrap/fxvehicle.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scrapcollision.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"

#define CGAMEPARTICLES_CTOR 0x21d1b0
#define CGAMEPARTICLES_DTOR 0x21d1c0
#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40

#define VEFF_RAND_SCALE 3.051850947599719e-05	/* 1 / 32767 (the double at rva 0x3a3d60) */

static inline cEntity *VE_Ent(cVehicleEntity *v) {
	return &v->wl.obj.base;
}

static inline CGameModel *VE_Model(cVehicleEntity *v) {
	return Veh_Vtbl(v)->GetModel(v);
}

/* the element of a new debris of the effect file with the life, made when the debris has none; noRadius: the
 * system first gets radius 0 */
static void *VE_Drop(const char *file, float life, byte noRadius, uint32_t size, void *(__thiscall *ctor)(void *)) {
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, file);
	if (noRadius) {
		sys->radiusSq = 0.0f;
	}
	cDebris *d = cDebrisSys_NewElement(sys);
	Debris_Life(d) = life;
	cDebrisSys_Drop(sys, d);
	if (ctor && !Debris_Element(d)) {
		void *mem = operator_new(size);
		cObjElementFX *el = mem ? ctor(mem) : NULL;
		Debris_Element(d) = el;
		el->vftable->Start(el, d);
	}
	return Debris_Element(d);
}

/* ---- the escape tubes and the speed trails ---- */

void __fastcall cVehicleTubes_Reset(cVehicleTubes *self) {
	self->count = 0;
	for (int i = 0; i < VEFF_TUBES; i++) {
		if (self->nodes[i] != -1) {
			CGameParticles_Release(&self->particles[i]);
		}
	}
}

void __fastcall cVehicleTrails_Clear(cVehicleTrails *self) {
	for (int i = 0; i < VEFF_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
}

void __fastcall cVehicleEffects_Reset(cVehicleEffects *self) {
	cVehicleTrails *t = self->trails;
	if (t) {
		t->count = 0;
		for (int i = 0; i < VEFF_TRAILS; i++) {
			CGameTrail_Clear(&t->trails[i]);
		}
	}
	if (self->tubes) {
		cVehicleTubes_Reset(self->tubes);
	}
	self->damageOn = 0;
	self->damageTime = 0.0f;
	self->emiOn = 0;
	self->emiTime = 0.0f;
	self->fireOn = 0;
	self->fireTime = 0.0f;
}

void __fastcall cVehicleEffects_ClearDamage(void *self) {
	cVehicleEffects *fx = self;
	fx->damageOn = 0;
	fx->damageTime = 0.0f;
}

void __fastcall cVehicleTubes_Init(cVehicleTubes *self) {
	char name[256];

	CHECK(self->owner);
	self->count = 0;
	CGameModel *model = VE_Model(self->owner);
	if (!model) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	if (!crt_strncmp(self->owner->typeName, "SLifeBoat", 9)) {
		self->kind = VEFF_TUBES_LIFEBOAT;
	} else if (!crt_strncmp(self->owner->typeName, "SMerc", 5)) {
		self->kind = VEFF_TUBES_MERC;
	} else {
		self->kind = VEFF_TUBES_NONE;
	}
	if (self->kind == VEFF_TUBES_NONE) {
		return;
	}
	for (int i = 0; i < VEFF_TUBES; i++) {
		Str_PrintfQuiet(name, 0xff, "DF_EscapeTube_%02d", i + 1);
		self->nodes[i] = CGameModel_FindRenderNodeWild(model, name);
		if (self->nodes[i] != -1) {
			self->count++;
			CGameParticles_Release(&self->particles[i]);
			CGameParticles_Attach(&self->particles[i], model, (uint32_t)self->nodes[i]);
		}
	}
	self->time = 0.0f;
}

void __fastcall cVehicleTrails_Init(cVehicleTrails *self) {
	char name[256];
	vec3_t pos;

	CHECK(self->owner);
	self->count = 0;
	CGameModel *model = VE_Model(self->owner);
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	int root = CGameModel_FindRenderNode(model, "DC_Root");
	CHECK(root > -1);
	self->root = *CGameModel_NodePos(model, &pos, (uint32_t)root, 0);
	for (int i = 0; i < VEFF_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 0x14);
		CGameTrail_SetFade(t, 1);
		CGameTrail_SetTexture(t, 2);
		CGameTrail_SetSmooth(t, 0);
		CGameTrail_SetWidth(t, 12.0f);
		CGameTrail_SetScale(t, 1.0f, 2.0f);
		CGameTrail_Clear(t);
		Str_PrintfQuiet(name, 0xff, "DF_SpeedTrail_%02d", i + 1);
		self->nodes[i] = CGameModel_FindRenderNodeWild(model, name);
		if (self->nodes[i] != -1) {
			self->count++;
		}
	}
	self->color = 0xffffff;
	self->fade = 0.0f;
	self->dist = 0.0f;
	self->time = 0.0f;
	self->segment = g_FXVehicleSpeedTrailDist;
}

/* the node of the trail in the world: its offset from DC_Root (raised by 20) turned and moved with the model */
static vec3_t *VE_TrailPoint(cVehicleTrails *self, CGameModel *model, int node, vec3_t *out) {
	vec3_t tmp, n, rot, mp;

	vec3_t *np = CGameModel_NodePos(model, &tmp, (uint32_t)node, 0);
	n.x = (np->x - self->root.x) + 0.0f;
	n.y = (np->y - self->root.y) + 0.0f;
	n.z = (np->z - self->root.z) + 20.0f;
	vec3_t *r = Vec3_Rotate(out, &n, CGameModel_GetRot(model, &rot));
	vec3_t *m = CGameModel_GetPos(model, &mp);
	out->x = r->x + m->x;
	out->y = r->y + m->y;
	out->z = r->z + m->z;
	return out;
}

/* rand / 32767 * range - sub in double, as the original */
static inline float VE_RandSub(double range, double sub) {
	return (float)((double)(crt_rand() & 0x7fff) * VEFF_RAND_SCALE * range - sub);
}

void __thiscall cVehicleTrails_Update(cVehicleTrails *self, float dt) {
	vec3_t c, p, w;

	CHECK(self->owner);
	CGameModel *model = VE_Model(self->owner);
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	vec3_t v = self->owner->localVel;
	float down = 0.0f;
	if (!(v.z > 0.0f)) {
		down = -v.z;
	}
	float f = Float_MinSS(1.0f, down * 0.5f / self->owner->maxSpeed);
	if (0.51f >= f && 0.51f >= self->fade) {
		if (self->dist > 0.0f) {
			int i;
			for (i = 0; i < VEFF_TRAILS; i++) {
				if (self->nodes[i] != -1 && CGameTrail_IsRender(&self->trails[i])) {
					break;
				}
			}
			if (i == VEFF_TRAILS) {
				self->dist = 0.0f;
			}
		}
		if (self->dist == 0.0f) {
			return;
		}
	}
	Float_Approach(&self->fade, f, dt);
	Veh_Vtbl(self->owner)->GetCenter(self->owner, &c);
	float seg = g_FXVehicleSpeedTrailDist;
	Float_Approach(&self->segment, Float_MaxSS(seg * 0.25f, seg - __builtin_fabsf(v.x * 0.5f)), dt * 1500.0f);
	float dx = c.x - self->last.x, dy = c.y - self->last.y, dz = c.z - self->last.z;
	float d2 = dy * dy + dx * dx + dz * dz;
	float dist;
	if (!(self->fade >= 0.55f)) {
		/* slow: the trails shrink every 0.05; the squared distance is kept (as in the original) */
		dist = d2;
		if (self->time >= 0.05f) {
			for (int i = 0; i < VEFF_TRAILS; i++) {
				if (self->nodes[i] != -1) {
					CGameTrail_Shrink(&self->trails[i]);
				}
			}
			self->time = 0.0f;
		}
	} else {
		dist = (float)__builtin_sqrt((double)d2);
		float fh = g_FXVehicleSpeedTrailFadeOutH;
		float a = ((fh - Float_MinSS(fh, __builtin_fabsf(v.x))) / fh) * ((self->fade - 0.55f) * 350.0f);
		uint32_t color = ((uint32_t)(int)(255.0f - a) << 24) | 0xffffff;
		if (dist > self->segment) {
			self->last = c;
			for (int i = 0; i < VEFF_TRAILS; i++) {
				if (self->nodes[i] == -1) {
					continue;
				}
				CGameTrail *t = &self->trails[i];
				VE_TrailPoint(self, model, self->nodes[i], &p);
				float width = g_FXVehicleSpeedTrailSize;
				CGameTrail_SetLast(t, NULL, &width, &color);
				float rz = VE_RandSub(10.0, 5.0);
				float ry = VE_RandSub(10.0, 5.0);
				float rx = VE_RandSub(10.0, 5.0);
				vec3_t q;
				q.x = rx + p.x;
				q.y = ry + p.y;
				q.z = rz + p.z;
				CGameTrail_Add(t, q, width * 0.2f, color);
			}
		} else {
			for (int i = 0; i < VEFF_TRAILS; i++) {
				if (self->nodes[i] == -1) {
					continue;
				}
				VE_TrailPoint(self, model, self->nodes[i], &w);
				float width = dist / self->segment * g_FXVehicleSpeedTrailSize * 0.8f + 0.2f;
				CGameTrail_SetLast(&self->trails[i], &w, &width, &color);
			}
		}
	}
	self->dist = dist;
	self->time = self->time + dt;
}

void __fastcall cVehicleEffects_Init(cVehicleEffects *self) {
	CHECK(self->owner);
	cVehicleEntity *v = self->owner;
	if (v->typeName[0] == 'T' || !strcmp(v->typeName, "SMosquito")) {
		cVehicleTrails *t = self->trails;
		if (t) {
			crt_eh_vector_dtor(t->trails, sizeof(CGameTrail), VEFF_TRAILS, GAME_FUNC(CGameTrail_dtor, CGAMETRAIL_DTOR));
			operator_delete_sized(t, sizeof(*t));
			self->trails = NULL;
		}
	} else {
		if (!self->trails) {
			cVehicleTrails *t = operator_new(sizeof(*t));
			if (t) {
				crt_eh_vector_ctor(t->trails, sizeof(CGameTrail), VEFF_TRAILS, GAME_FUNC(CGameTrail_ctor, CGAMETRAIL_CTOR),
					GAME_FUNC(CGameTrail_dtor, CGAMETRAIL_DTOR));
				t->owner = NULL;
			}
			self->trails = t;
		}
		cVehicleTrails_SetOwner(self->trails, VE_Ent(self->owner));
		cVehicleTrails_Init(self->trails);
	}
	if (!crt_strncmp(self->owner->typeName, "SLifeBoat", 9)) {
		if (!self->tubes) {
			cVehicleTubes *t = operator_new(sizeof(*t));
			if (t) {
				crt_eh_vector_ctor(t->particles, sizeof(CGameParticles), VEFF_TUBES,
					GAME_FUNC(CGameParticles_ctor, CGAMEPARTICLES_CTOR), GAME_FUNC(CGameParticles_Release, CGAMEPARTICLES_DTOR));
				t->owner = NULL;
			}
			self->tubes = t;
		}
		cVehicleTubes_SetOwner(self->tubes, VE_Ent(self->owner));
		cVehicleTubes_Init(self->tubes);
		return;
	}
	cVehicleTubes *t = self->tubes;
	if (t) {
		crt_eh_vector_dtor(t->particles, sizeof(CGameParticles), VEFF_TUBES, GAME_FUNC(CGameParticles_Release, CGAMEPARTICLES_DTOR));
		operator_delete_sized(t, sizeof(*t));
		self->tubes = NULL;
	}
}

void __thiscall cVehicleEffects_SetActive(cVehicleEffects *self, byte on) {
	if (!on && self->trails && self->trails->count > 0) {
		cVehicleTrails_Clear(self->trails);
	}
}

/* the CHECKs of the owner setters */
static void VE_CheckOwner(cEntity *e) {
	CHECK(e);
	CHECK(e->vftable->IsType(e, TYEN_VEHICLE));
}

void __thiscall cVehicleEffects_SetOwner(cVehicleEffects *self, cEntity *e) {
	VE_CheckOwner(e);
	self->owner = Veh(e);
	if (self->trails) {
		cVehicleTrails_SetOwner(self->trails, e);
	}
	if (self->tubes) {
		cVehicleTubes_SetOwner(self->tubes, e);
	}
}

void __thiscall cVehicleTubes_SetOwner(cVehicleTubes *self, cEntity *e) {
	VE_CheckOwner(e);
	self->owner = Veh(e);
}

void __thiscall cVehicleTrails_SetOwner(cVehicleTrails *self, cEntity *e) {
	VE_CheckOwner(e);
	self->owner = Veh(e);
}

/* ---- the debris effects ---- */

void __thiscall cVehicleEffects_Start(cVehicleEffects *self, const char *name) {
	if (!strcmp(name, "VehicleDamage")) {
		if (!CGameModel_IsLoaded(VE_Model(self->owner))) {
			return;
		}
		cObjElementFXVehicleDamage *el = VE_Drop(g_FXVehicleDamageFile, 1.5f, 0, 0, NULL);
		if (el) {
			if (!el->on) {
				el->on = 1;
			}
			cObjElementFXVehicleDamage_SetEntity(el, VE_Ent(self->owner));
		}
		return;
	}
	if (!strcmp(name, "VehicleEMI")) {
		if (!CGameModel_IsLoaded(VE_Model(self->owner))) {
			return;
		}
		cObjElementFXVehicleEMI *el = VE_Drop(g_FXVehicleEMIFile, self->owner->emi, 1,
			sizeof(cObjElementFXVehicleEMI), cObjElementFXVehicleEMI_ctor);
		if (el) {
			if (!el->on) {
				el->on = 1;
			}
			cObjElementFXVehicleEMI_SetEntity(el, VE_Ent(self->owner));
		}
		return;
	}
	if (!crt_strncmp(name, "VehicleSputnik", 14)) {
		if (!CGameModel_IsLoaded(VE_Model(self->owner))) {
			return;
		}
		int mode = 0;
		float life = 0.0f, c4 = 0.0f;
		if (!strcmp(name, "VehicleSputnikMake")) {
			mode = 1;
			life = 3.15f;
			c4 = 2.34f;
		} else if (!strcmp(name, "VehicleSputnikSuck")) {
			mode = 2;
			life = 3.75f;
			c4 = 3.34f;
		}
		cObjElementFXVehicleSputnik *el = VE_Drop(g_FXVehicleSputnikFile, life, 0, 0, NULL);
		if (el) {
			if (!el->on) {
				el->on = 1;
			}
			cObjElementFXVehicleSputnik_SetEntity(el, VE_Ent(self->owner));
			cObjElementFXVehicleSputnik_SetC0(el, mode);
			cObjElementFXVehicleSputnik_SetC4(el, c4);
		}
		return;
	}
	if (!strcmp(name, "VehicleFire")) {
		if (!CGameModel_IsLoaded(VE_Model(self->owner))) {
			return;
		}
		cObjElementFXVehicleFire *el = VE_Drop(g_FXVehicleFireFile, 1.0f, 0, sizeof(cObjElementFXVehicleFire),
			cObjElementFXVehicleFire_ctor);
		if (el) {
			if (!el->on) {
				el->on = 1;
			}
			cObjElementFXVehicleFire_SetEntity(el, VE_Ent(self->owner));
		}
		return;
	}
	if (!strcmp(name, "VehicleSpawn")) {
		cObjElementFXVehicleSpawn *el = VE_Drop("models/gfx/test/SpawnRays.m3d", 2.0f, 1,
			sizeof(cObjElementFXVehicleSpawn), cObjElementFXVehicleSpawn_ctor);
		if (el) {
			if (!el->on) {
				el->on = 1;
			}
			cObjElementFXVehicleSpawn_SetEntity(el, VE_Ent(self->owner));
		}
	}
}

void __thiscall cVehicleEffects_UpdateState(cVehicleEffects *self, float dt) {
	vec3_t v, out, center, from, to, hit;

	CHECK(self->owner);
	cVehicleEntity *o = self->owner;
	int life = o->wl.life;
	int percent = life * 100 / o->maxLife;
	if (!self->damageOn && life > 0 && percent < g_FXSetOnFireLifePercent) {
		self->damageTime += dt;
		if (self->damageTime > 0.05f && CGameModel_IsLoaded(VE_Model(o))) {
			self->damageOn = 1;
			cVehicleEffects_Start(self, "VehicleDamage");
		}
	} else if (self->damageOn && percent > g_FXSetOnFireLifePercent) {
		self->damageOn = 0;
		self->damageTime = 0.0f;
	}
	if (!self->emiOn) {
		o = self->owner;
		if (o->emi > 0.5f) {
			self->emiTime = dt + self->emiTime;
			if (self->emiTime > 0.05f && CGameModel_IsLoaded(VE_Model(o))) {
				self->emiOn = 1;
				cVehicleEffects_Start(self, "VehicleEMI");
			}
		}
	} else if (self->owner->emi <= 0.0f) {
		self->emiOn = 0;
		self->emiTime = 0.0f;
	}
	o = self->owner;
	float burn = o->wl.fireRemain;
	if (!self->fireOn) {
		if (burn > 0.5f) {
			self->fireTime = dt + self->fireTime;
			if (self->fireTime > 0.05f && CGameModel_IsLoaded(VE_Model(o))) {
				self->fireOn = 1;
				cVehicleEffects_Start(self, "VehicleFire");
			}
		}
	} else if (0.0f > burn) {
		self->fireOn = 0;
		self->fireTime = 0.0f;
	}
	if (!g_WindFxSmoke || !self->owner->wl.obj.cam || g_FreeSpaceMagnet) {
		return;
	}
	float now = Timer_GetTime(&g_pWorld->timer);
	o = self->owner;
	v = o->localVel;
	float down = 0.0f;
	if (!(v.z > 0.0f)) {
		down = -v.z;
	}
	if (!(o->smoothThrust > 0.5f) || !(down > g_WindFXSmokeMinVel)) {
		return;
	}
	float last = g_WindFXSmokeLast;
	if (!(last > now) && !(now - last > g_WindFXSmokeTimeStep)) {
		return;
	}
	g_WindFXSmokeLast = now;
	if (!(0.1f > Float_Keep(now) - now)) {
		return;
	}
	float k = o->smoothSpeed / 28000.0f;
	float ax = o->smoothAng.x, ay = o->smoothAng.y;
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = -Veh_Vtbl(o)->GetRadius(o);
	vec3_t *r = Vec3_RotateXY(&out, &v, ax, ay);
	vec3_t *c = Veh_Vtbl(self->owner)->GetCenter(self->owner, &center);
	from.x = c->x + r->x;
	from.y = c->y + r->y;
	from.z = c->z + r->z;
	float rnd = (float)((double)(crt_rand() & 0x7fff) * VEFF_RAND_SCALE * 0.19999998807907104 + 0.75);
	v.x = 0.0f;
	v.y = -(g_WindFXSmokeMaxDist * k);
	v.z = -(rnd * g_WindFXSmokeMaxDist * k);
	Vec3_RotateXY(&out, &v, ax, ay);
	to.x = out.x + from.x;
	to.y = out.y + from.y;
	to.z = out.z + from.z;
	if (Engine_TestLine(&from, &to, &hit, NULL)) {
		cVehicleEffects_WindSmoke(&self->owner->effects, hit.x, hit.y, hit.z);
	}
}

/* ---- the parts before the range of the file: constructors, destructor, Update and the escape tubes ---- */

#define VEFF_TUBE_STEP 0.025f			/* between two emissions */
#define g_VehicleSmokeRamps (GAME_ARRAY(uint32_t, g_VehicleSmokeRamps, 0x428670))	/* 3 color ramps of 9 colors */

cVehicleEffects *__fastcall cVehicleEffects_ctor(cVehicleEffects *self) {
	self->owner = NULL;
	self->damageOn = 0;
	self->damageTime = 0.0f;
	self->emiOn = 0;
	self->emiTime = 0.0f;
	self->fireOn = 0;
	self->fireTime = 0.0f;
	self->trails = NULL;
	self->tubes = NULL;
	return self;
}

cVehicleTubes *__fastcall cVehicleTubes_ctor(cVehicleTubes *self) {
	crt_eh_vector_ctor(self->particles, sizeof(CGameParticles), VEFF_TUBES, GAME_FUNC(CGameParticles_ctor, CGAMEPARTICLES_CTOR),
		GAME_FUNC(CGameParticles_Release, CGAMEPARTICLES_DTOR));
	self->owner = NULL;
	return self;
}

cVehicleTrails *__fastcall cVehicleTrails_ctor(cVehicleTrails *self) {
	crt_eh_vector_ctor(self->trails, sizeof(CGameTrail), VEFF_TRAILS, GAME_FUNC(CGameTrail_ctor, CGAMETRAIL_CTOR),
		GAME_FUNC(CGameTrail_dtor, CGAMETRAIL_DTOR));
	self->owner = NULL;
	return self;
}

void __fastcall cVehicleEffects_dtor(cVehicleEffects *self) {
	cVehicleTrails *t = self->trails;
	if (t) {
		crt_eh_vector_dtor(t->trails, sizeof(CGameTrail), VEFF_TRAILS, GAME_FUNC(CGameTrail_dtor, CGAMETRAIL_DTOR));
		operator_delete_sized(t, sizeof(*t));
		self->trails = NULL;
	}
	cVehicleTubes *u = self->tubes;
	if (u) {
		crt_eh_vector_dtor(u->particles, sizeof(CGameParticles), VEFF_TUBES, GAME_FUNC(CGameParticles_Release, CGAMEPARTICLES_DTOR));
		operator_delete_sized(u, sizeof(*u));
		self->tubes = NULL;
	}
}

void __thiscall cVehicleEffects_Update(cVehicleEffects *self, float dt) {
	if (self->trails && self->trails->count > 0) {
		cVehicleTrails_Update(self->trails, dt);
	}
	if (self->tubes && self->tubes->count > 0) {
		cVehicleTubes_Update(self->tubes, dt);
	}
}

/* rand / 32767 * range + add in double, as the original */
static inline float VE_RandAdd(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * VEFF_RAND_SCALE * range + add);
}

/* the constants of the smoke of one kind of tube */
typedef struct VE_TubeSmoke {
	double velZ[2];			/* range, add of the velocity: z */
	double velXY[2];		/* x and y (minus) */
	double accX[2];			/* the acceleration x (minus) */
	double accY[2];			/* y (plus) */
	double accZ[2];			/* z, with the sign of accZSub */
	byte accZSub;			/* 1: add - rand * range (the SLifeBoat), else rand * range - add */
	double sizeEnd;			/* range of sizeEnd (plus 215) */
	double life[2];			/* range, add of the life time */
	double rotVar[2];		/* range, minus */
} VE_TubeSmoke;

static const VE_TubeSmoke VE_TubeSmokes[2] = {
	/* VEFF_TUBES_LIFEBOAT */
	{ { 500.0, 350.0 }, { 110.0, 55.0 }, { 50.0, 25.0 }, { 1150.0, 300.0 }, { 50.0, -15.0 }, 1, 20.0,
		{ 0x1.6666640000000p-2, 0x1.19999a0000000p-1 }, { 0x1.921fb60000000p-1, 0x1.921fb60000000p-2 } },
	/* VEFF_TUBES_MERC */
	{ { 150.0, 650.0 }, { 100.0, 50.0 }, { 40.0, 20.0 }, { 950.0, 400.0 }, { 40.0, 20.0 }, 0, 40.0,
		{ 0x1.9999980000000p-3, 0x1.cccccc0000000p-2 }, { 0x1.41b2f80000000p-1, 0x1.41b2f80000000p-2 } },
};

/* one puff of a tube: the velocity and acceleration (turned with the model), emitted when the system has room */
static void VE_TubePuff(cVehicleTubes *self, CGameParticles *p, const VE_TubeSmoke *k, float f, const vec3_t *rot) {
	vec3_t v, vel, a, acc;

	float vz = VE_RandAdd(k->velZ[0], k->velZ[1]);
	float vy = VE_RandSub(k->velXY[0], k->velXY[1]);
	float vx = VE_RandSub(k->velXY[0], k->velXY[1]);
	float m = f + f + 1.0f;
	v.x = vx * m;
	v.y = vy * m;
	v.z = vz * m;
	Vec3_Rotate(&vel, &v, rot);
	float az;
	if (k->accZSub) {
		az = (float)(k->accZ[1] - (double)(crt_rand() & 0x7fff) * VEFF_RAND_SCALE * k->accZ[0]);
	} else {
		az = VE_RandSub(k->accZ[0], k->accZ[1]);
	}
	float ay = VE_RandAdd(k->accY[0], k->accY[1]);
	float ax = VE_RandSub(k->accX[0], k->accX[1]);
	float m2 = 1.0f - f * 1.5f;
	a.x = ax * m2;
	a.y = ay * m2;
	a.z = az * m2;
	Vec3_Rotate(&acc, &a, rot);
	float active = (float)(CGameParticles_NumActive(p) / self->count);
	float total = (float)(CGameParticles_NumParticles(p) / self->count);
	if (!(total * (f + 0.45f) > active)) {
		return;
	}
	CGameParticles_SetColorRampOn(p, 1);
	p->birthRate = 0;
	p->initPosMin.x = 0.0f;
	p->initPosMin.y = 0.0f;
	p->initPosMin.z = 0.0f;
	p->initPosMax.x = 0.0f;
	p->initPosMax.y = p->initPosMin.y;
	p->initPosMax.z = p->initPosMin.z;
	p->initVelMin.y = vel.y;
	p->initVelMin.z = vel.z;
	p->initVelMin.x = vel.x;
	p->initVelMax.x = vel.x;
	p->initVelMax.y = p->initVelMin.y;
	p->initVelMax.z = p->initVelMin.z;
	p->initAccMin.y = acc.y;
	p->initAccMin.x = acc.x;
	p->initAccMin.z = acc.z;
	p->initAccMax.x = acc.x;
	p->initAccMax.y = p->initAccMin.y;
	p->initAccMax.z = p->initAccMin.z;
	p->sizeIni = VE_RandAdd(10.0, 35.0);
	p->sizeEnd = VE_RandAdd(k->sizeEnd, 215.0);
	float lf = f * 0.5f;
	float life = VE_RandAdd(k->life[0], k->life[1]);
	p->lifeTime = life * (1.0f - lf);
	float r = VE_RandAdd(0x1.921fb60000000p+2, 0.0);
	p->rotIni = r;
	p->rotEnd = r;
	p->rotVar = VE_RandSub(k->rotVar[0], k->rotVar[1]);
	p->sizeLife = 1.0f;
	byte s = (byte)((crt_rand() & 1) + 0x11);
	p->spriteIni = s;
	p->spriteEnd = s;
	CGameParticles_SetColorRamp(p, g_VehicleSmokeRamps + (crt_rand() % 3) * 9);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 0);
}

void __thiscall cVehicleTubes_Update(cVehicleTubes *self, float dt) {
	vec3_t pos, rot;

	CHECK(self->owner);
	CHECK(self->owner->wl.obj.fx);
	CGameModel *model = VE_Model(self->owner);
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	if (!CGameModel_IsVisible(model) || !self->owner->wl.obj.base.flag_4c) {
		return;
	}
	uint32_t fx = CGameModel_GetEffect(&self->owner->wl.obj.model, NULL);
	if (fx) {
		return;
	}
	float t = self->time + dt;
	self->time = t;
	if (VEFF_TUBE_STEP > t) {
		return;
	}
	self->time = 0.0f;
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	vec3_t v = self->owner->localVel;
	float down = 0.0f;
	if (!(v.z > 0.0f)) {
		down = -v.z;
	}
	float f = Float_MinSS(1.0f, down * 0.5f / self->owner->maxSpeed);
	for (int i = 0; i < VEFF_TUBES; i++) {
		if (self->nodes[i] == -1 || (uint32_t)self->kind > VEFF_TUBES_MERC) {
			continue;
		}
		VE_TubePuff(self, &self->particles[i], &VE_TubeSmokes[self->kind], f, &rot);
	}
}

/* ---- the wind smoke ---- */

#define g_WindSmokeRamps (GAME_ARRAY(uint32_t, g_WindSmokeRamps, 0x428798))	/* 2 color ramps of 9 colors */

/* rand / 32767 * range + add with a float range, as the original */
static inline float VE_RandAddF(float range, float add) {
	return (float)((double)(crt_rand() & 0x7fff) * VEFF_RAND_SCALE * (double)range + (double)add);
}

/* -1 or 1 */
static inline float VE_RandSign(void) {
	return (float)((crt_rand() & 1) * 2 - 1);
}

void __thiscall cVehicleEffects_WindSmoke(cVehicleEffects *self, float x, float y, float z) {
	vec3_t vel, cam, tmp, n, unit, side, ang, dir;

	Obj_Vtbl(VE_Ent(self->owner))->GetVel(VE_Ent(self->owner), &vel);
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_WindFxSmokeFile);
	cDebris *d = cDebrisSys_GetIdle(sys);
	cam = g_pWorld->camPos[g_pWorld->curCam];
	CGameModel_SetPos(Debris_Model(d), &cam);
	d->speed = 1.0f;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	}
	cObjElementFXParticleSystem *el = Debris_Element(d);
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	CGameParticles *p = &el->particles;
	p->birthRate = 0;
	const vec3_t *c = Veh_Vtbl(self->owner)->GetCenter(self->owner, &tmp);
	float dy = y - c->y;
	float dx = x - c->x;
	float dz = z - c->z;
	float q = dy * dy + dx * dx + dz * dz;
	float md = g_WindFXSmokeMaxDist;
	if (q > md * md) {
		return;
	}
	float radius = Veh_Vtbl(self->owner)->GetRadius(self->owner);
	float len = (float)__builtin_sqrt((double)q);
	if (len == 0.0f) {
		n.x = dx;
		n.y = dy;
		n.z = dz;
	} else {
		float inv = 1.0f / len;
		n.x = dx * inv;
		n.y = dy * inv;
		n.z = dz * inv;
	}
	/* the smoke starts a bit before the hit, spread sideways along the x axis of the ship */
	float back = VE_RandAdd(15.0, 40.0);
	unit.x = 1.0f;
	unit.y = 0.0f;
	unit.z = 0.0f;
	float bx = x - back * n.x;
	float by = y - back * n.y;
	float bz = z - back * n.z;
	Vec3_Rotate(&side, &unit, Veh_Vtbl(self->owner)->GetAngles(self->owner, &ang));
	float scale = radius / g_WindFXSmokeSizeDiv;
	float posLo = radius * 0x1.333334p-3f;
	float posRange = radius * 0x1.333334p+0f - posLo;
	float velLo = radius * 0x1.666666p-1f;
	float velRange = radius * 0x1.ccccccp+0f - velLo;
	for (int i = 3; i; i--) {
		float s = VE_RandAddF(posRange, posLo);
		float sg = VE_RandSign();
		float sx = side.x * sg;
		float sy = side.y * sg;
		float sz = side.z * sg;
		float ox = s * sx;
		float oy = s * sy;
		float oz = s * sz;
		float t = VE_RandAdd(50.0, 15.0);
		const vec3_t *u = FX_RandomDir(&dir);
		p->initPosMin.x = t * u->x + bx + ox;
		p->initPosMin.y = u->y * t + by + oy;
		p->initPosMin.z = u->z * t + bz + oz;
		p->initPosMax = p->initPosMin;
		float s2 = VE_RandAddF(velRange, velLo);
		float sg2 = VE_RandSign();
		float s2x = side.x * sg2;
		float s2z = side.z * sg2;
		float s2y = side.y * sg2;
		float o2x = s2 * s2x;
		float o2y = s2 * s2y;
		float o2z = s2 * s2z;
		float w = VE_RandAdd(270.0, 80.0);
		float vz = vel.z * 0.75f;
		float vx = vel.x * 0.75f;
		float vy = vel.y * 0.75f;
		p->initVelMin.x = vx - w * n.x + o2x;
		p->initVelMin.y = vy - w * n.y + o2y;
		p->initVelMin.z = vz - w * n.z + o2z;
		p->initVelMax = p->initVelMin;
		p->initAccMin.x = p->initVelMin.x * 0.25f;
		p->initAccMin.y = p->initVelMin.y * 0.25f;
		p->initAccMin.z = p->initVelMin.z * 0.25f;
		p->initAccMax = p->initAccMin;
		p->sizeIni = VE_RandAdd(150.0, 550.0) * scale;
		p->sizeEnd = VE_RandAdd(250.0, 1300.0) * scale;
		p->lifeTime = VE_RandAdd(0x1.9999980000000p-3, 0x1.cccccc0000000p-2);
		p->sizeLife = VE_RandAdd(0x1.99999c0000000p-3, 0x1.6666660000000p-2);
		float r = VE_RandAdd(0x1.921fb60000000p+2, 0.0);
		p->rotIni = r;
		p->rotEnd = r;
		p->rotVar = VE_RandSub(0x1.921fb60000000p-1, 0x1.921fb60000000p-2);
		byte sp = (byte)((crt_rand() & 1) + 0x11);
		p->spriteIni = sp;
		p->spriteEnd = sp;
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, g_WindSmokeRamps + (crt_rand() & 1) * 9);
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 0);
	}
}

/* ---- the shoot effects (rva 0x103500): the particles of m_FollowSistPart2 of the embedded FX at the weapon ---- */

/* the color ramps of the shots (9 colors each) */
#define VS_RAMP_FIRE (GAME_ARRAY(uint32_t, VS_RAMP_FIRE, 0x428704))	/* [2 * 9], 2 ramps */
#define VS_RAMP_FIRE2 (GAME_ARRAY_IN(uint32_t, VS_RAMP_FIRE, 36, 0x428728))
#define VS_RAMP_SMOKE (GAME_ARRAY(uint32_t, VS_RAMP_SMOKE, 0x4286e0))
#define VS_RAMP_ATPC (GAME_ARRAY(uint32_t, VS_RAMP_ATPC, 0x42874c))

#define g_FXVulcanFireScale GAME_GLOBAL_IN(float, GVar_FXVulcanFireScale, 16, 0x447d9c)
#define g_FXVulcanFireTime GAME_GLOBAL_IN(float, GVar_FXVulcanFireTime, 16, 0x447d84)
#define g_FXDevastatorFireScale GAME_GLOBAL_IN(float, GVar_FXDevastatorFireScale, 16, 0x447db4)
#define g_FXDevastatorFireTime GAME_GLOBAL_IN(float, GVar_FXDevastatorFireTime, 16, 0x447c64)

#define VS_PI_2 0x1.921fb6p+0f
#define VS_2PI 0x1.921fb60000000p+2

/* the weapon node of the slot relative to DC_Root of the model */
static void VS_NodePos(cVehicleEffects *self, CGameModel *model, int ammo, int slot, vec3_t *d) {
	vec3_t np;
	int node = self->owner->weapons.nodes[ammo][slot];

	CHECK(node > -1);
	CGameModel_NodePos(model, &np, (uint32_t)node, 0);
	vec3_t root = Veh_FX(self->owner)->rootPos;
	d->x = np.x - root.x;
	d->y = np.y - root.y;
	d->z = np.z - root.z;
}

static inline void VS_Pos(CGameParticles *p, float x, float y, float z) {
	p->initPosMin.x = x;
	p->initPosMin.y = y;
	p->initPosMin.z = z;
	p->initPosMax = p->initPosMin;
}

static inline void VS_NoVel(CGameParticles *p) {
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMax.x = 0.0f;
	p->initVelMax.y = 0.0f;
	p->initVelMax.z = 0.0f;
}

static inline void VS_Sprites(CGameParticles *p, byte s) {
	p->spriteIni = s;
	p->spriteEnd = s;
}

static inline byte VS_Sprite4(void) {
	return (byte)(crt_rand() % 4 + 0x21);
}

/* a random start angle and the rotation speed of the fire sprites */
static inline void VS_RotRand(CGameParticles *p) {
	float r = VE_RandAdd(VS_2PI, 0.0);
	p->rotIni = r;
	p->rotEnd = r;
	p->rotVar = VE_RandSub(0x1.921fb60000000p-1, 0x1.921fb60000000p-2);
}

/* the position spread around the node (x, y +- xy, z down by a random) */
static inline void VS_Spread(CGameParticles *p, const vec3_t *d, float xy, double r0, double a0, double r1, double a1) {
	float z0 = -VE_RandAdd(r0, a0);
	p->initPosMin.x = d->x - xy;
	p->initPosMin.y = d->y - xy;
	p->initPosMin.z = d->z + z0;
	float z1 = -VE_RandAdd(r1, a1);
	p->initPosMax.x = d->x + xy;
	p->initPosMax.y = d->y + xy;
	p->initPosMax.z = d->z + z1;
}

/* the first part of the Vulcan and the Devastator: the flash, then the smoke spread */
static void VS_FlashEnd(CGameParticles *p, int n) {
	byte sp = VS_Sprite4();
	p->colorIni = 0x80808080;
	VS_Sprites(p, sp);
	p->colorEnd = 0xff000000;
	CGameParticles_Apply(p);
	CGameParticles_EmitDt(p, n, 1, VE_RandAdd(0x1.eb851e0000000p-6, 0x1.47ae140000000p-7) + 0x1.eb851ep-6f);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetSparksOn(p, 1);
	CGameParticles_SetColorRamp(p, VS_RAMP_SMOKE);
}

/* the two trail sprites of a shot (0x1a, then 0x0d), one particle each over 0.01..0.02 s */
static void VS_Trails(CGameParticles *p) {
	VS_Sprites(p, 0x1a);
	CGameParticles_Apply(p);
	CGameParticles_EmitDt(p, 1, 1, VE_RandAdd(0x1.47ae140000000p-7, 0x1.47ae140000000p-7));
	VS_Sprites(p, 0x0d);
	CGameParticles_Apply(p);
	CGameParticles_EmitDt(p, 1, 1, VE_RandAdd(0x1.47ae140000000p-7, 0x1.47ae140000000p-7));
}

/* the sparks around the barrel: 4 directions in the plane, each a particle with its own velocity */
typedef struct VS_Sparks {
	double vel[2];			/* the speed along the direction */
	double velZ[2];			/* the z of the velocity (minus) */
	double size[2];
	byte colors;			/* the Devastator sets its colors for each */
} VS_Sparks;

static void VS_Ring(CGameParticles *p, const vec3_t *d, float scale, const VS_Sparks *k, float time) {
	vec3_t unit, rot, dir;
	float dx = d->x + 0.0f;
	float dy = d->y + 0.0f;
	float ang = 0.0f;

	do {
		float a = VE_RandAdd(0x1.78861c0000000p+0, 0x1.99999a0000000p-4) + ang;
		rot.x = 0.0f;
		rot.y = a;
		rot.z = 0.0f;
		unit.x = 1.0f;
		unit.y = 0.0f;
		unit.z = 0.0f;
		Vec3_Rotate(&dir, &unit, &rot);
		float ox = dir.x * 40.0f * scale;
		float oy = dir.y * 40.0f * scale;
		float oz = dir.z * 40.0f * scale;
		float jz = -VE_RandAdd(10.0, 70.0);
		p->initPosMin.x = ox + dx;
		p->initPosMin.y = oy + dy;
		p->initPosMin.z = oz + (d->z + jz);
		p->initPosMax = p->initPosMin;
		float m = VE_RandAdd(k->vel[0], k->vel[1]);
		float vx = dir.x * m * scale;
		float vy = dir.y * m * scale;
		float vz = dir.z * m * scale;
		float wz = -VE_RandAdd(k->velZ[0], k->velZ[1]);
		p->initVelMin.x = vx + 0.0f;
		p->initVelMin.y = vy + 0.0f;
		p->initVelMin.z = wz + vz;
		m = VE_RandAdd(k->vel[0], k->vel[1]);
		vx = dir.x * m * scale;
		vy = dir.y * m * scale;
		vz = dir.z * m * scale;
		wz = -VE_RandAdd(k->velZ[0], k->velZ[1]);
		p->initVelMax.x = vx + 0.0f;
		p->initVelMax.y = vy + 0.0f;
		p->initVelMax.z = wz + vz;
		float s = VE_RandAdd(k->size[0], k->size[1]) * scale;
		p->sizeIni = s;
		p->sizeEnd = s;
		p->lifeTime = time;
		p->sizeLife = 0x1.99999ap-4f;
		p->rotIni = 0.0f;
		p->rotEnd = 0.0f;
		p->rotVar = 0.0f;
		byte sp = VS_Sprite4();
		if (k->colors) {
			p->colorIni = 0x32ff8c32;
		}
		VS_Sprites(p, sp);
		if (k->colors) {
			p->colorEnd = 0xff323232;
		}
		CGameParticles_Apply(p);
		CGameParticles_EmitN(p, 1, 1);
		ang = ang + VS_PI_2;
	} while (6.283185307179586 > (double)ang);
}

/* the smoke after the shot: sprites 9 at the node (50 lower), no velocity, a random start angle, one particle */
static void VS_Smoke(CGameParticles *p, const vec3_t *d, float sizeLife, double lifeR, double lifeA, double iniR,
	double iniA, double endR, double endA) {
	VS_Pos(p, d->x + 0.0f, d->y + 0.0f, d->z - 50.0f);
	VS_NoVel(p);
	float life = VE_RandAdd(lifeR, lifeA);
	p->sizeLife = sizeLife;
	p->lifeTime = life;
	p->sizeIni = VE_RandAdd(iniR, iniA);
	float e = VE_RandAdd(endR, endA);
	VS_Sprites(p, 9);
	p->sizeEnd = e;
}

/* the end of the effects: no rotation speed, a random start angle, one particle */
static void VS_Emit(CGameParticles *p) {
	float r = VE_RandAdd(VS_2PI, 0.0);
	p->rotVar = 0.0f;
	p->rotIni = r;
	p->rotEnd = r;
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
}

static const VS_Sparks VS_VulcanSparks = { { 20.0, 60.0 }, { 10.0, 50.0 }, { 20.0, 100.0 }, 0 };
static const VS_Sparks VS_DevastatorSparks = { { 20.0, 80.0 }, { 50.0, 100.0 }, { 30.0, 110.0 }, 1 };

static void VS_Vulcan(cVehicleEffects *self, CGameModel *model, CGameParticles *p, int slot) {
	vec3_t d;

	VS_NodePos(self, model, g_VulcanIDAmmo, slot, &d);
	p->sparkVelScale = 1.0f;
	p->sparkMaxSize = 4.0f;
	CGameParticles_SetSparksOn(p, 0);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, VS_RAMP_FIRE);
	p->birthRate = 0;
	VS_Pos(p, d.x, d.y, d.z);
	/* the flash */
	float z = -VE_RandAdd(200.0, 600.0);
	float s = g_FXVulcanFireScale;
	float v = s * 50.0f;
	p->initVelMin.x = v;
	p->initVelMin.y = v;
	p->initVelMin.z = s * z;
	p->initVelMax = p->initVelMin;
	p->sizeIni = VE_RandAdd(15.0, 10.0) * g_FXVulcanFireScale;
	p->sizeEnd = VE_RandAdd(35.0, 135.0) * g_FXVulcanFireScale;
	float t = g_FXVulcanFireTime * 1.5f;
	p->sizeLife = 0.75f;
	p->lifeTime = t;
	float r = VE_RandAdd(VS_2PI, 0.0);
	p->rotVar = 0x1.921fb6p-1f;
	p->rotIni = r;
	p->rotEnd = r;
	VS_FlashEnd(p, 1);
	VS_Spread(p, &d, 20.0f, 20.0, 40.0, 20.0, 40.0);
	s = g_FXVulcanFireScale;
	float z0 = s * 0.0f;
	p->initVelMin.z = s * -400.0f;
	p->initVelMin.x = z0;
	p->initVelMin.y = z0;
	s = g_FXVulcanFireScale;
	z0 = s * 0.0f;
	p->initVelMax.z = s * -600.0f;
	p->initVelMax.x = z0;
	p->initVelMax.y = z0;
	float sz = VE_RandAdd(10.0, 60.0) * g_FXVulcanFireScale;
	p->sizeIni = sz;
	p->sizeEnd = sz;
	p->lifeTime = g_FXVulcanFireTime;
	p->sizeLife = 1.0f;
	p->rotIni = 0.0f;
	p->rotEnd = 0.0f;
	p->rotVar = 0.0f;
	VS_Trails(p);
	VS_Ring(p, &d, g_FXVulcanFireScale, &VS_VulcanSparks, g_FXVulcanFireTime);
	VS_Smoke(p, &d, 0x1.99999ap-4f, 0x1.47ae100000000p-6, 0x1.99999a0000000p-4, 50.0, 150.0, 100.0, 350.0);
	VS_Emit(p);
}

static void VS_Devastator(cVehicleEffects *self, CGameModel *model, CGameParticles *p, int slot) {
	vec3_t d;

	VS_NodePos(self, model, g_DevastatorIDAmmo, slot, &d);
	p->sparkVelScale = 1.0f;
	p->sparkMaxSize = 4.0f;
	CGameParticles_SetSparksOn(p, 0);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, VS_RAMP_FIRE + (crt_rand() & 1) * 9);
	p->birthRate = 0;
	VS_Pos(p, d.x, d.y, d.z);
	float a = VE_RandAdd(100.0, 200.0);
	float b = VE_RandSub(50.0, 25.0);
	float c = VE_RandSub(50.0, 25.0);
	float s = g_FXDevastatorFireScale;
	p->initVelMin.x = c * s;
	p->initVelMin.y = b * s;
	p->initVelMin.z = -a * s;
	p->initVelMax = p->initVelMin;
	p->sizeIni = g_FXDevastatorFireScale * VE_RandAdd(10.0, 55.0);
	p->sizeEnd = g_FXDevastatorFireScale * VE_RandAdd(40.0, 180.0);
	float t = g_FXDevastatorFireTime * 0x1.4cccccp+0f;
	p->sizeLife = 1.0f;
	p->lifeTime = t;
	float r = VE_RandAdd(VS_2PI, 0.0);
	p->rotVar = 0x1.921fb6p-1f;
	p->rotIni = r;
	p->rotEnd = r;
	VS_FlashEnd(p, 2);
	VS_Spread(p, &d, 20.0f, 20.0, 40.0, 20.0, 40.0);
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = g_FXDevastatorFireScale * -400.0f;
	p->initVelMax.x = 0.0f;
	p->initVelMax.y = 0.0f;
	p->initVelMax.z = g_FXDevastatorFireScale * -600.0f;
	p->sizeIni = g_FXDevastatorFireScale * VE_RandAdd(20.0, 60.0);
	p->sizeEnd = g_FXDevastatorFireScale * VE_RandAdd(20.0, 80.0);
	p->lifeTime = g_FXDevastatorFireTime;
	p->sizeLife = 0.5f;
	p->rotIni = 0.0f;
	p->rotEnd = 0.0f;
	p->rotVar = 0.0f;
	p->colorIni = 0x32ff8c32;
	p->colorEnd = 0xff323232;
	VS_Trails(p);
	VS_Ring(p, &d, g_FXDevastatorFireScale, &VS_DevastatorSparks, g_FXDevastatorFireTime);
	VS_Smoke(p, &d, 0x1.99999ap-4f, 0x1.9999980000000p-5, 0x1.3333340000000p-3, 50.0, 150.0, 100.0, 450.0);
	VS_Emit(p);
}

static void VS_Swarm(cVehicleEffects *self, CGameModel *model, CGameParticles *p, int slot) {
	vec3_t d;

	VS_NodePos(self, model, g_SwarmIDAmmo, slot, &d);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetSparksOn(p, 0);
	p->birthRate = 0;
	for (int i = 0; i < 4; i++) {
		CGameParticles_SetColorRamp(p, VS_RAMP_FIRE + (crt_rand() & 1) * 9);
		VS_Pos(p, d.x, d.y, d.z);
		float a = VE_RandAdd(150.0, 250.0);
		float b = VE_RandSub(60.0, 30.0);
		float c = VE_RandSub(60.0, 30.0);
		p->initVelMin.z = -a;
		p->initVelMin.x = c;
		p->initVelMin.y = b;
		p->initVelMax = p->initVelMin;
		p->sizeIni = VE_RandAdd(15.0, 150.0);
		p->sizeEnd = VE_RandAdd(30.0, 260.0);
		float t = VE_RandAdd(0x1.9999980000000p-4, 0x1.6666660000000p-2);
		p->sizeLife = 1.0f;
		p->lifeTime = t;
		VS_RotRand(p);
		byte sp;
		if (crt_rand() & 1) {
			sp = (byte)(crt_rand() % 2 + 0x21);
			p->spriteEnd = 0x24;
		} else {
			sp = (byte)((crt_rand() & 1) + 0x11);
			p->spriteEnd = sp;
		}
		p->spriteIni = sp;
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, VE_RandAdd(0x1.47ae140000000p-7, 0x1.47ae140000000p-7) + (float)i * 0x1.47ae14p-5f);
	}
	CGameParticles_SetColorRamp(p, VS_RAMP_SMOKE);
	VS_Smoke(p, &d, 0.25f, 0x1.47ae200000000p-6, 0x1.1eb8520000000p-2, 50.0, 200.0, 50.0, 750.0);
	VS_Emit(p);
}

static void VS_ATPC(cVehicleEffects *self, CGameModel *model, CGameParticles *p, int slot) {
	vec3_t d;

	VS_NodePos(self, model, g_ATPCIDAmmo, slot, &d);
	CGameParticles_SetSparksOn(p, 0);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, VS_RAMP_ATPC);
	float x = d.x + 0.0f;
	float y = d.y + 0.0f;
	float z = d.z + 0.0f;
	p->birthRate = 0;
	VS_Pos(p, x, y, z);
	VS_NoVel(p);
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = 0.0f;
	p->initAccMin.z = 0.0f;
	p->initAccMax.x = 0.0f;
	p->initAccMax.y = 0.0f;
	p->initAccMax.z = 0.0f;
	p->sizeIni = VE_RandAdd(25.0, 50.0);
	float e = VE_RandAdd(105.0, 445.0);
	p->sizeLife = 0.25f;
	p->sizeEnd = e;
	p->lifeTime = VE_RandAdd(0x1.eb85200000000p-6, 0x1.c28f5c0000000p-3);
	float r = VE_RandAdd(VS_2PI, 0.0);
	p->rotVar = 0.0f;
	VS_Sprites(p, 9);
	p->rotIni = r;
	p->rotEnd = r;
	CGameParticles_Apply(p);
	CGameParticles_EmitDt(p, 1, 1, VE_RandAdd(0x1.47ae140000000p-7, 0x1.47ae140000000p-7));
	p->sizeIni = VE_RandAdd(50.0, 150.0);
	e = VE_RandAdd(100.0, 650.0);
	p->sizeLife = 0x1.99999ap-3f;
	p->sizeEnd = e;
	p->lifeTime = VE_RandAdd(0x1.47ae100000000p-6, 0x1.99999a0000000p-3);
	VS_Sprites(p, 5);
	VS_Emit(p);
}

static void VS_Inferno(cVehicleEffects *self, CGameModel *model, CGameParticles *p, int slot) {
	vec3_t d;

	VS_NodePos(self, model, g_InfernoIDAmmo, slot, &d);
	p->sparkVelScale = 1.0f;
	p->sparkMaxSize = 4.0f;
	CGameParticles_SetSparksOn(p, 0);
	CGameParticles_SetColorRampOn(p, 1);
	p->birthRate = 0;
	VS_Spread(p, &d, 40.0f, 10.0, 20.0, 50.0, 100.0);
	float z = -VE_RandAdd(100.0, 250.0);
	p->initVelMin.x = -65.0f;
	p->initVelMin.y = -65.0f;
	p->initVelMin.z = z;
	z = -VE_RandAdd(50.0, 400.0);
	p->initVelMax.x = 65.0f;
	p->initVelMax.y = 65.0f;
	p->initVelMax.z = z;
	p->sizeIni = VE_RandAdd(50.0, 100.0);
	p->sizeEnd = VE_RandAdd(50.0, 250.0);
	float t = VE_RandAdd(0x1.3333340000000p-3, 0x1.99999a0000000p-2);
	p->sizeLife = 1.0f;
	p->lifeTime = t;
	CGameParticles_SetColorRamp(p, VS_RAMP_FIRE);
	for (int i = 0; i < 2; i++) {
		if (i) {
			CGameParticles_SetColorRamp(p, VS_RAMP_FIRE2);
		}
		VS_RotRand(p);
		VS_Sprites(p, (byte)((crt_rand() & 1) + 0x11));
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 2, 1, VE_RandAdd(0x1.9999980000000p-5, 0x1.47ae140000000p-7));
	}
	VS_Smoke(p, &d, 0x1.333334p-3f, 0x1.9999900000000p-5, 0x1.99999a0000000p-2, 50.0, 150.0, 100.0, 800.0);
	VS_Emit(p);
}

/* the prototype of include/scrap/weapons.h takes the effects as void * */
void __thiscall cVehicleEffects_Shoot(void *effects, int weap, int slot) {
	cVehicleEffects *self = effects;

	CHECK(self->owner);
	CHECK(self->owner->wl.obj.fx);
	CGameModel *model = VE_Model(self->owner);
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	if (!CGameModel_IsVisible(model) && !Veh_FX(self->owner)->followed) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	cEmbeddedFXVehicle *fx = Veh_FX(self->owner);
	CHECK(fx->follow2.ParticleSystem);
	CGameParticles *p = &fx->follow2;
	if (!p->ParticleSystem || weap == g_LaserIDWeap) {
		return;
	}
	if (weap == g_VulcanIDWeap) {
		VS_Vulcan(self, model, p, slot);
	} else if (weap == g_DevastatorIDWeap) {
		VS_Devastator(self, model, p, slot);
	} else if (weap == g_SwarmIDWeap) {
		VS_Swarm(self, model, p, slot);
	} else if (weap == g_ATPCIDWeap) {
		VS_ATPC(self, model, p, slot);
	} else if (weap == g_InfernoIDWeap) {
		VS_Inferno(self, model, p, slot);
	}
}
