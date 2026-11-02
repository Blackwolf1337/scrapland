/*
 * Entities/Vehicle/Weaps/Inferno.cpp, Laser.cpp and Swarm.cpp of the game: the vehicle weapons Inferno (a missile
 * of bees that explodes in a sphere), Laser (shots and the Hook) and Swarm (a swarm of bee missiles), with the Sonic
 * mine and the Tesla handlers that sit in the same code. Most functions are C callbacks of the weapon scripts
 * (cPy_Function natives): they read their parameters from the memory file of the call (names with ReadString,
 * numbers with Read), find the entities by name and work on them. The missiles are cMissileEntity ("Missile"), the
 * ships cVehicleEntity ("Car"). The weapon block of a ship at +0x5f0 is the one of include/scrap/fxweapons.h
 * (cTargetLock: ammo, weapon slots, levels, the lock); WeaponManager.cpp owns it.
 */
#ifndef SCRAP_WEAPS2_H
#define SCRAP_WEAPS2_H

#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/doors.h"
#include "scrap/fxweapons.h"
#include "scrap/missileentity.h"
#include "scrap/net.h"
#include "scrap/primitive.h"
#include "scrap/scorer.h"
#include "scrap/scrapcollision.h"
#include "scrap/sound.h"
#include "scrap/weapons.h"
#include "scrap/weaps.h"
#include "scrap/world.h"

#define TYEN_MISSILE "Missile"

/* the fields of the entities with life (ships, characters, missiles) */
#define Ent_Active(e) (((cEntity *)(e))->active)
#define Cam_ShakeTime(c) (((cFollowCamEntity *)(c))->quakeTime)
#define Cam_Shake(c) (((cFollowCamEntity *)(c))->quakeFactor)

/* the fields of the ships */
#define Veh_HitInfo(v) ((void *)&WLife(v)->attack)	/* the last entity or point the ship hit */
#define Veh_Player(v) (Veh(v)->slot)			/* 0..3 for the ship of a local player */
#define Veh_Aim(v) ((void *)&Veh(v)->autoAim)
#define Veh_Effects(v) ((void *)&Veh(v)->effects)	/* VehicleEffects.cpp: the shoot effects */

/* the fields of the missiles (cMissileEntity) */
#define Mis_Size(m) ((void *)&Missile(m)->wl.prim)	/* Primitive.cpp: the collision size */

/* a field of an effect element of the debris by offset (Vulcan and Laser), until the elements have a struct */

/* engine variables (Gvars, the value at +0x10 of the variable object) */
extern cGVar GVar_SwarmIDWeap;
#define g_SwarmIDWeap (GVar_SwarmIDWeap.value.i)
extern cGVar GVar_InfernoIDWeap;
#define g_InfernoIDWeap (GVar_InfernoIDWeap.value.i)
extern cGVar GVar_InfernoRangeMin;
#define g_InfernoRangeMin (GVar_InfernoRangeMin.value.f)
extern cGVar GVar_InfernoAmmoCost;
#define g_InfernoAmmoCost (GVar_InfernoAmmoCost.value.i)
extern cGVar GVar_InfernoIDAmmo;
#define g_InfernoIDAmmo (GVar_InfernoIDAmmo.value.i)
extern cGVar GVar_InfernoRangeBay;
#define g_InfernoRangeBay (GVar_InfernoRangeBay.value.f)
extern cGVar GVar_InfernoDistLimit;
#define g_InfernoDistLimit (GVar_InfernoDistLimit.value.f)
extern cGVar GVar_LaserRange;
#define g_LaserRange (GVar_LaserRange.value.f)
extern cGVar GVar_LaserDistLimit;
#define g_LaserDistLimit (GVar_LaserDistLimit.value.f)
extern cGVar GVar_SwarmIDAmmo;
#define g_SwarmIDAmmo (GVar_SwarmIDAmmo.value.i)
extern cGVar GVar_TeslaDownRangeTimePad;
#define g_TeslaDownRangeTimePad (GVar_TeslaDownRangeTimePad.value.f)
extern cGVar GVar_TeslaDownRangeTime;
#define g_TeslaDownRangeTime (GVar_TeslaDownRangeTime.value.f)
extern cGVar GVar_TeslaMinAngRange;
#define g_TeslaMinAngRange (GVar_TeslaMinAngRange.value.f)
extern cGVar GVar_TeslaRange;
#define g_TeslaRange (GVar_TeslaRange.value.f)
extern cGVar GVar_TeslaDistLimit;
#define g_TeslaDistLimit (GVar_TeslaDistLimit.value.f)
extern cGVar GVar_VulcanIDWeap;
#define g_VulcanIDWeap (GVar_VulcanIDWeap.value.i)
extern cGVar GVar_VulcanIDAmmo;
#define g_VulcanIDAmmo (GVar_VulcanIDAmmo.value.i)
extern cGVar GVar_VulcanDistLimitUPG;
#define g_VulcanDistLimitUPG (GVar_VulcanDistLimitUPG.value.f)
extern cGVar GVar_VulcanDistLimit;
#define g_VulcanDistLimit (GVar_VulcanDistLimit.value.f)
extern cGVar GVar_VulcanRange;
#define g_VulcanRange (GVar_VulcanRange.value.f)

#define g_InfernoSphereFunc ((&g_ChTypeSphereFunc))

/* the exe functions of other modules */
void cWithLifeEntity_SetBurner(cEntity *e, cEntity *attacker);
void cTargetLock_SetHookDelay(cTargetLock *self, float t);
/* SndInterface.c */
void cSndInterface_Resize(cSndInterface *self, int n);
void cSndInterface_SetSound(cSndInterface *self, int i, const char *name);
/* FXWeapons.cpp: the emitters of the explosion and the bee effects */
void cObjElementFXInfernoExplosion_Explode(cObjElementFXInfernoExplosion *self, float x, float y, float z,
	float range, int bees, byte flag);
void FXSwarm_BeeDeath(float x, float y, float z, float vx, float vy, float vz, float size, float time, int n);
void FXSwarm_Hit(float x, float y, float z, float vx, float vy, float vz, float size, float time, float dx, float dy,
	float dz);

#define WEAP_PI 3.141592653589793

/* the shared code of the weapons, inlined in the original */
static inline cEntity *Weap_Vehicle(const char *name) {
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_VEHICLE));
	return v;
}

static inline cEntity *Weap_Missile(const char *name) {
	cEntity *m = World_FindEntity(name);
	CHECK(m);
	CHECK(Entity_IsType(m, TYEN_MISSILE));
	return m;
}

/* the ship of that name, NULL if there is none */
static inline cEntity *Weap_FindVehicle(const char *name) {
	cEntity *v = World_FindEntity(name);
	return v && Entity_IsType(v, TYEN_VEHICLE) ? v : NULL;
}

/* World.h: the entity of player slot i (the CHECK when it is set) */
static inline cEntity *Weap_Owner(cWorld *w, int i) {
	cEntity *e = w->owners[i];
	if (e) {
		CHECK(Entity_IsType(e, TYEN_WITH_LIFE));
	}
	return w->owners[i];
}

/* sound i of the entity from its first volume and pitch */
static inline void Weap_PlaySound(cEntity *e, int i) {
	cSndItem *s = PtrArray_Get(&Obj(e)->snd, i);
	if (s) {
		s->volume = s->vols[0];
		s->pitch = s->pitches[0];
		cSndItem_Play(s, e);
	}
}

static inline void Weap_ReadVec(cAbstractFile *f, vec3_t *v) {
	f->vftable->Read(f, &v->x, 4);
	f->vftable->Read(f, &v->y, 4);
	f->vftable->Read(f, &v->z, 4);
}

/* ship name: the aim mode of the ship */
static inline cEntity *Weap_SetAimMode(cAbstractFile *f, int mode) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_Vehicle(name);
	Veh(v)->autoAim.status = mode;
	return v;
}

/* the debris drop (position, angles, velocity) at a weapon node of the ship */
static inline void Weap_DropAtNode(cEntity *v, uint32_t node, matrix_t *mtx, vec3_t *ang, vec3_t *vel) {
	const matrix_t *mm = CGameModel_NodeMatrix(Obj_Vtbl(v)->GetModel(v), mtx, node, 0, NULL, 0);
	float x = mm->m[12];
	float y = mm->m[13];
	float z = mm->m[14];
	Obj_Vtbl(v)->GetViewAng(v, ang);
	Obj_Vtbl(v)->GetVel(v, vel);
	g_DebrisDrop[0].x = x;
	g_DebrisDrop[0].y = y;
	g_DebrisDrop[0].z = z;
	g_DebrisDrop[1] = *ang;
	g_DebrisDrop[2] = *vel;
}

/* the engine sound of a missile: one sound, looped */
static inline void Weap_EngineSound(cEntity *m, const char *file, float vol, float attIni, float attEnd, float doppler) {
	cSndInterface_Resize(&Obj(m)->snd, 1);
	cSndInterface_SetSound(&Obj(m)->snd, 0, file);
	cSndItem *s = PtrArray_Get(&Obj(m)->snd, 0);
	if (s) {
		s->vols[0] = vol;
		s->sound.desc.param1 = attIni;
		s->sound.desc.param2 = attEnd;
		s->sound.desc.param3 = doppler;
		s->sound.desc.loop = 1;
	}
}

void Weap_UpdateHud(cAbstractFile *f, char (*targets)[0x80], float *times, int levelRot, int levelSize);

/* the length of (x, y, z), summed like the original */
static inline float Weap_Len(float x, float y, float z) {
	return (float)__builtin_sqrt((double)(y * y + x * x + z * z));
}

/* (x, y, z) normalized into out unless its length is 0 */
static inline void Weap_Norm(vec3_t *out, float x, float y, float z) {
	float len = Weap_Len(x, y, z);
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

/* the push of a shot of n bays with force on e (an entity with life that walks not), away from the ship v */
static inline void Weap_PushAway(cEntity *v, cEntity *e, int n, float force) {
	vec3_t a, b, d, nd, ov, vel;

	float mass = WLife(e)->mass;
	const vec3_t *pv = v->vftable->GetPos(v, &a);
	const vec3_t *pe = e->vftable->GetPos(e, &b);
	d.x = pe->x - pv->x;
	d.y = pe->y - pv->y;
	d.z = pe->z - pv->z;
	float k = (float)n * force / mass;
	const vec3_t *dir = Vec3_NormalizeTo(&nd, &d);
	float px = dir->x * k;
	float py = dir->y * k;
	float pz = dir->z * k;
	const vec3_t *cur = Obj_Vtbl(e)->GetVel(e, &ov);
	vel.y = cur->y + py;
	vel.z = cur->z + pz;
	vel.x = cur->x + px;
	Obj_Vtbl(e)->SetVel(e, &vel);
}

/* the HUD item of a weapon that does not fire turns back to the angle 0 by d */
static inline void Weap_HudSpinDown(cCannonScorer *item, float d) {
	float a = NormAng(item->angle);
	item->angle = a;
	float s = a + d;
	if (0.0f > s) {
		item->angle = s;
		return;
	}
	float t = a - d;
	item->angle = t > 0.0f ? t : 0.0f;
}

/* Inferno.c; the natives with their names in the registry of C functions ("!name" for cPy_Function_SetFunc),
 * the missile callbacks without a name are set by the fire natives */
void Inferno_Shoot(const char *name);

byte Tesla_Shoot(cEntity *v);

#endif
