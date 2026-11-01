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
#define WFIELD(e, type, off) (*(type *)((byte *)(e) + (off)))

/* engine variables (Gvars, the value at +0x10 of the variable object) */
#define g_SwarmIDWeap GAME_GLOBAL_IN(int, GVar_SwarmIDWeap, 16)
#define g_InfernoIDWeap GAME_GLOBAL_IN(int, GVar_InfernoIDWeap, 16)
#define g_InfernoEngineSoundVol GAME_GLOBAL_IN(float, GVar_InfernoEngineSoundVol, 16)
#define g_InfernoRangeMin GAME_GLOBAL_IN(float, GVar_InfernoRangeMin, 16)
#define g_InfernoTimeDelay GAME_GLOBAL_IN(float, GVar_InfernoTimeDelay, 16)
#define g_InfernoAmmoCost GAME_GLOBAL_IN(int, GVar_InfernoAmmoCost, 16)
#define g_InfernoUpTopSpeed GAME_GLOBAL_IN(float, GVar_InfernoUpTopSpeed, 16)
#define g_InfernoIDAmmo GAME_GLOBAL_IN(int, GVar_InfernoIDAmmo, 16)
#define g_InfernoLifeTime GAME_GLOBAL_IN(float, GVar_InfernoLifeTime, 16)
#define g_InfernoEngineSoundAttIni GAME_GLOBAL_IN(float, GVar_InfernoEngineSoundAttIni, 16)
#define g_InfernoTurnSpeed GAME_GLOBAL_IN(float, GVar_InfernoTurnSpeed, 16)
#define g_InfernoMaxDamage GAME_GLOBAL_IN(float, GVar_InfernoMaxDamage, 16)
#define g_InfernoHitSoundVol GAME_GLOBAL_IN(float, GVar_InfernoHitSoundVol, 16)
#define g_InfernoHitSoundAttEndRadius GAME_GLOBAL_IN(float, GVar_InfernoHitSoundAttEndRadius, 16)
#define g_InfernoEngineSoundDoppler GAME_GLOBAL_IN(float, GVar_InfernoEngineSoundDoppler, 16)
#define g_InfernoBeesSpeed GAME_GLOBAL_IN(float, GVar_InfernoBeesSpeed, 16)
#define g_InfernoUpgFireFactor GAME_GLOBAL_IN(float, GVar_InfernoUpgFireFactor, 16)
#define g_InfernoInvUpgExtraDamage GAME_GLOBAL_IN(float, GVar_InfernoInvUpgExtraDamage, 16)
#define g_InfernoHitForce GAME_GLOBAL_IN(float, GVar_InfernoHitForce, 16)
#define g_InfernoRangeBay GAME_GLOBAL_IN(float, GVar_InfernoRangeBay, 16)
#define g_InfernoArmor GAME_GLOBAL_IN(int, GVar_InfernoArmor, 16)
#define g_InfernoUpgFireTime GAME_GLOBAL_IN(float, GVar_InfernoUpgFireTime, 16)
#define g_InfernoHitSound GAME_GLOBAL_IN(const char *, GVar_InfernoHitSound, 16)
#define g_InfernoMinDamage GAME_GLOBAL_IN(float, GVar_InfernoMinDamage, 16)
#define g_InfernoEngineSound GAME_GLOBAL_IN(const char *, GVar_InfernoEngineSound, 16)
#define g_InfernoNormSpeed GAME_GLOBAL_IN(float, GVar_InfernoNormSpeed, 16)
#define g_InfernoXplosionCall GAME_GLOBAL_IN(nativefunc_t, NativeFunc_InfernoXplosionCall, 4)	/* the function of the native "InfernoXplosionCall" */
#define g_InfernoMissileFile GAME_GLOBAL_IN(const char *, GVar_InfernoMissileFile, 16)
#define g_FXInfernoExplosionMaxDist GAME_GLOBAL_IN(float, GVar_FXInfernoExplosionMaxDist, 16)
#define g_InfernoDistLimit GAME_GLOBAL_IN(float, GVar_InfernoDistLimit, 16)
#define g_InfernoHitSoundAttIniRadius GAME_GLOBAL_IN(float, GVar_InfernoHitSoundAttIniRadius, 16)
#define g_InfernoEngineSoundAttEnd GAME_GLOBAL_IN(float, GVar_InfernoEngineSoundAttEnd, 16)
#define g_InfernoExplosionFile GAME_GLOBAL_IN(const char *, GVar_InfernoExplosionFile, 16)
#define g_LaserAmmoCost GAME_GLOBAL_IN(float, GVar_LaserAmmoCost, 16)
#define g_LaserRange GAME_GLOBAL_IN(float, GVar_LaserRange, 16)
#define g_LaserShootFile GAME_GLOBAL_IN(const char *, GVar_LaserShootFile, 16)
#define g_LaserHitForce GAME_GLOBAL_IN(float, GVar_LaserHitForce, 16)
#define g_LaserHitFile GAME_GLOBAL_IN(const char *, GVar_LaserHitFile, 16)
#define g_HookEMITime GAME_GLOBAL_IN(float, GVar_HookEMITime, 16)
#define g_LaserTimeDelay GAME_GLOBAL_IN(float, GVar_LaserTimeDelay, 16)
#define g_LaserRicSoundVol GAME_GLOBAL_IN(float, GVar_LaserRicSoundVol, 16)
#define g_LaserHookFile GAME_GLOBAL_IN(const char *, GVar_LaserHookFile, 16)
#define g_LaserDamage GAME_GLOBAL_IN(int, GVar_LaserDamage, 16)
#define g_HookAccel GAME_GLOBAL_IN(float, GVar_HookAccel, 16)
#define g_LaserDistLimit GAME_GLOBAL_IN(float, GVar_LaserDistLimit, 16)
#define g_LaserMinSpeed GAME_GLOBAL_IN(float, GVar_LaserMinSpeed, 16)
#define g_LaserRicSound GAME_GLOBAL_IN(const char *, GVar_LaserRicSound, 16)
#define g_SonicRechargeTime GAME_GLOBAL_IN(float, GVar_SonicRechargeTime, 16)
#define g_SonicArmor GAME_GLOBAL_IN(int, GVar_SonicArmor, 16)
#define g_FXSonicXpldeFile GAME_GLOBAL_IN(const char *, GVar_FXSonicXpldeFile, 16)
#define g_SonicLifeTime GAME_GLOBAL_IN(float, GVar_SonicLifeTime, 16)
#define g_SonicIDAmmo GAME_GLOBAL_IN(int, GVar_SonicIDAmmo, 16)
#define g_SonicAmmoNeed GAME_GLOBAL_IN(int, GVar_SonicAmmoNeed, 16)
#define g_CreateSonicFunc (GAME_ARRAY_IN(cPy_Function, GVar_CreateSonicFunc, 16))
#define g_SetUpSonicFunc (GAME_ARRAY_IN(cPy_Function, GVar_SetUpSonicFunc, 16))
#define g_SwarmHitSoundVol GAME_GLOBAL_IN(float, GVar_SwarmHitSoundVol, 16)
#define g_SwarmMissileFile GAME_GLOBAL_IN(const char *, GVar_SwarmMissileFile, 16)
#define g_SwarmUpTopSpeed GAME_GLOBAL_IN(float, GVar_SwarmUpTopSpeed, 16)
#define g_SwarmEngineSoundAttIni GAME_GLOBAL_IN(float, GVar_SwarmEngineSoundAttIni, 16)
#define g_SwarmBeeSoundAttEnd GAME_GLOBAL_IN(float, GVar_SwarmBeeSoundAttEnd, 16)
#define g_SwarmDeathTime GAME_GLOBAL_IN(float, GVar_SwarmDeathTime, 16)
#define g_SwarmBeeRadius GAME_GLOBAL_IN(float, GVar_SwarmBeeRadius, 16)
#define g_SwarmMinRadius GAME_GLOBAL_IN(float, GVar_SwarmMinRadius, 16)
#define g_SwarmBeesSpeed GAME_GLOBAL_IN(float, GVar_SwarmBeesSpeed, 16)
#define g_SwarmHitSound GAME_GLOBAL_IN(const char *, GVar_SwarmHitSound, 16)
#define g_SwarmHitSoundAttIni GAME_GLOBAL_IN(float, GVar_SwarmHitSoundAttIni, 16)
#define g_SwarmNormSpeed GAME_GLOBAL_IN(float, GVar_SwarmNormSpeed, 16)
#define g_SwarmLifeTime GAME_GLOBAL_IN(float, GVar_SwarmLifeTime, 16)
#define g_SwarmHitForce GAME_GLOBAL_IN(float, GVar_SwarmHitForce, 16)
#define g_SwarmTurnSpeedUPG GAME_GLOBAL_IN(float, GVar_SwarmTurnSpeedUPG, 16)
#define g_SwarmFastSpeed GAME_GLOBAL_IN(float, GVar_SwarmFastSpeed, 16)
#define g_SwarmTurnSpeed GAME_GLOBAL_IN(float, GVar_SwarmTurnSpeed, 16)
#define g_SwarmArmor GAME_GLOBAL_IN(int, GVar_SwarmArmor, 16)
#define g_SwarmEngineSound GAME_GLOBAL_IN(const char *, GVar_SwarmEngineSound, 16)
#define g_SwarmBeeSoundVol GAME_GLOBAL_IN(float, GVar_SwarmBeeSoundVol, 16)
#define g_SwarmIDAmmo GAME_GLOBAL_IN(int, GVar_SwarmIDAmmo, 16)
#define g_SwarmEngineSoundVol GAME_GLOBAL_IN(float, GVar_SwarmEngineSoundVol, 16)
#define g_SwarmTimeDelay GAME_GLOBAL_IN(float, GVar_SwarmTimeDelay, 16)
#define g_SwarmBeeSoundAttIni GAME_GLOBAL_IN(float, GVar_SwarmBeeSoundAttIni, 16)
#define g_SwarmHitSoundAttEnd GAME_GLOBAL_IN(float, GVar_SwarmHitSoundAttEnd, 16)
#define g_SwarmBeeSound GAME_GLOBAL_IN(const char *, GVar_SwarmBeeSound, 16)
#define g_SwarmNumber1 GAME_GLOBAL_IN(int, GVar_SwarmNumber1, 16)
#define g_SwarmEngineSoundAttEnd GAME_GLOBAL_IN(float, GVar_SwarmEngineSoundAttEnd, 16)
#define g_SwarmEngineSoundDoppler GAME_GLOBAL_IN(float, GVar_SwarmEngineSoundDoppler, 16)
#define g_SwarmAmmoCost1 GAME_GLOBAL_IN(int, GVar_SwarmAmmoCost1, 16)
#define g_SwarmAmmoCost2 GAME_GLOBAL_IN(int, GVar_SwarmAmmoCost2, 16)
#define g_SwarmAmmoCost3 GAME_GLOBAL_IN(int, GVar_SwarmAmmoCost3, 16)
#define g_SwarmAmmoCost4 GAME_GLOBAL_IN(int, GVar_SwarmAmmoCost4, 16)
extern int g_SwarmAmmoCosts[6];	/* [6] SwarmAmmoCost1..4 at the start, then 0 0 */
#define g_TeslaDownRangeTimePad GAME_GLOBAL_IN(float, GVar_TeslaDownRangeTimePad, 16)
#define g_TeslaMinAngRangePad GAME_GLOBAL_IN(float, GVar_TeslaMinAngRangePad, 16)
#define g_TeslaDownRangeTime GAME_GLOBAL_IN(float, GVar_TeslaDownRangeTime, 16)
#define g_TeslaMinAngRangeUpg GAME_GLOBAL_IN(float, GVar_TeslaMinAngRangeUpg, 16)
#define g_TeslaMinAngRangeUpgPad GAME_GLOBAL_IN(float, GVar_TeslaMinAngRangeUpgPad, 16)
#define g_TeslaMinAngRange GAME_GLOBAL_IN(float, GVar_TeslaMinAngRange, 16)
#define g_TeslaTrackForce GAME_GLOBAL_IN(float, GVar_TeslaTrackForce, 16)
#define g_TeslaShootFile GAME_GLOBAL_IN(const char *, GVar_TeslaShootFile, 16)
#define g_TeslaMaxAngRangePad GAME_GLOBAL_IN(float, GVar_TeslaMaxAngRangePad, 16)
#define g_TeslaUpRangeTime GAME_GLOBAL_IN(float, GVar_TeslaUpRangeTime, 16)
#define g_TeslaRange GAME_GLOBAL_IN(float, GVar_TeslaRange, 16)
#define g_TeslaHitForce GAME_GLOBAL_IN(float, GVar_TeslaHitForce, 16)
#define g_TeslaAmmoCost GAME_GLOBAL_IN(float, GVar_TeslaAmmoCost, 16)
#define g_TeslaDistLimit GAME_GLOBAL_IN(float, GVar_TeslaDistLimit, 16)
#define g_TeslaMaxAngRange GAME_GLOBAL_IN(float, GVar_TeslaMaxAngRange, 16)
#define g_VulcanIDWeap GAME_GLOBAL_IN(int, GVar_VulcanIDWeap, 16)
#define g_VulcanIDAmmo GAME_GLOBAL_IN(int, GVar_VulcanIDAmmo, 16)
#define g_FXVulcanHitDistance GAME_GLOBAL_IN(float, GVar_FXVulcanHitDistance, 16)
#define g_VulcanTimeDelay GAME_GLOBAL_IN(float, GVar_VulcanTimeDelay, 16)
#define g_VulcanRic2Sound GAME_GLOBAL_IN(const char *, GVar_VulcanRic2Sound, 16)
#define g_VulcanRicSoundAttIni GAME_GLOBAL_IN(float, GVar_VulcanRicSoundAttIni, 16)
#define g_VulcanDistLimitUPG GAME_GLOBAL_IN(float, GVar_VulcanDistLimitUPG, 16)
#define g_VulcanTimeDelayUPG GAME_GLOBAL_IN(float, GVar_VulcanTimeDelayUPG, 16)
#define g_VulcanHitForce GAME_GLOBAL_IN(float, GVar_VulcanHitForce, 16)
#define g_VulcanDamageEasy GAME_GLOBAL_IN(int, GVar_VulcanDamageEasy, 16)
#define g_VulcanDistLimit GAME_GLOBAL_IN(float, GVar_VulcanDistLimit, 16)
#define g_VulcanHitFile GAME_GLOBAL_IN(const char *, GVar_VulcanHitFile, 16)
#define g_VulcanRic1Sound GAME_GLOBAL_IN(const char *, GVar_VulcanRic1Sound, 16)
#define g_VulcanRic0Sound GAME_GLOBAL_IN(const char *, GVar_VulcanRic0Sound, 16)
#define g_VulcanAmmoCost GAME_GLOBAL_IN(float, GVar_VulcanAmmoCost, 16)
#define g_VulcanBulletFile GAME_GLOBAL_IN(const char *, GVar_VulcanBulletFile, 16)
#define g_VulcanDamage GAME_GLOBAL_IN(int, GVar_VulcanDamage, 16)
#define g_VulcanHitUseNormal GAME_GLOBAL_IN(int, GVar_VulcanHitUseNormal, 16)
#define g_VulcanRic0SoundVol GAME_GLOBAL_IN(float, GVar_VulcanRic0SoundVol, 16)
#define g_VulcanRic2SoundVol GAME_GLOBAL_IN(float, GVar_VulcanRic2SoundVol, 16)
#define g_VulcanRic1SoundVol GAME_GLOBAL_IN(float, GVar_VulcanRic1SoundVol, 16)
#define g_VulcanRange GAME_GLOBAL_IN(float, GVar_VulcanRange, 16)
#define g_VulcanRicSoundAttEnd GAME_GLOBAL_IN(float, GVar_VulcanRicSoundAttEnd, 16)

/* the state of the module */
extern char g_InfernoHitName[128];	/* 0x80, the entity of the sphere callback */
extern cEntity *g_InfernoOwner;	/* the ship of the exploding missile, NULL if none */
extern float g_InfernoRange;
extern cEntity *g_InfernoMissile;
#define g_InfernoHudTarget (GAME_ARRAY(char[0x80], g_InfernoHudTarget))	/* per player: the target shown */
extern float g_InfernoHudTime[4];	/* per player */
extern vec3_t g_InfernoPos;
extern vec3_t g_SonicHitPos;
#define g_SwarmHudTarget (GAME_ARRAY(char[0x80], g_SwarmHudTarget))
extern float g_SwarmHudTime[4];
#define g_InfernoSphereFunc (GAME_ARRAY_IN(cPy_Function, g_ChTypeSphereFunc, 0))
extern int * g_SwarmSlotData[4];	/* per slot: a pointer to the int AddBees takes */

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
void Inferno_NetCreateNative(cAbstractFile *f);	/* "InfernoCreate" */
void Inferno_HudNative(cAbstractFile *f);	/* "InfernoFX" */
void Inferno_FireNative(cAbstractFile *f);	/* "InfernoFire" */
void Inferno_HitNative(cAbstractFile *f);	/* "InfernoHit" */
cEntity *Inferno_Hit(const char *name);
void Inferno_OnDamageNative(cAbstractFile *f);
void Inferno_OnDeathNative(cAbstractFile *f);
void Inferno_AimOffNative(cAbstractFile *f);	/* "InfernoOnEnd" */
void Inferno_SlowNative(cAbstractFile *f);	/* "InfernoOnObjSlot" */
void Inferno_AimOnNative(cAbstractFile *f);	/* "InfernoOnStart" */
void Inferno_AimMode3Native(cAbstractFile *f);	/* "InfernoPress" */
void Inferno_AimMode2Native(cAbstractFile *f);	/* "InfernoRelease" */
void Inferno_ServerHitNative(cAbstractFile *f);	/* "InfernoServerHit" */
void Inferno_Explode(const char *name);
void Inferno_NetSetUpNative(cAbstractFile *f);	/* "InfernoSetUp" */
void Inferno_SphereHit(cAbstractFile *f);	/* "InfernoXplosionCall" */

/* Laser.c */
void Hook_Aim(cEntity *v);
void Hook_Unlock(cEntity *v);
void Hook_AimNative(cAbstractFile *f);	/* "HookActivate" */
void Hook_OffNative(cAbstractFile *f);	/* "HookDeActivate" */
void Hook_UpdateNative(cAbstractFile *f);	/* "HookFire" */
void Hook_StartNative(cAbstractFile *f);	/* "HookPlugFX" */
byte Laser_Shoot(cEntity *v);
void Laser_CreateNative(cAbstractFile *f);	/* "LaserCreate" */
void Laser_HudNative(cAbstractFile *f);	/* "LaserFX" */
void Laser_FireNative(cAbstractFile *f);	/* "LaserFire" */
void Laser_Push(cEntity *v, cEntity *e, int n);
void Laser_AimOnNative(cAbstractFile *f);	/* "LaserPress" */
void Laser_AimOffNative(cAbstractFile *f);	/* "LaserRelease" */
void Hook_StopSound(cEntity *v);
void Hook_StartSound(cEntity *v);
byte Vehicle_IsLocalPlayer(cEntity *v);
void Sonic_FireNative(cAbstractFile *f);	/* "SonicFire" */
void Sonic_ExplodeNative(cAbstractFile *f);	/* "SonicXplde" */

/* Swarm.c */
void Swarm_OnBeeHitNative(cAbstractFile *f);
void Swarm_BeeDeath(cEntity *sw, float x, float y, float z, byte sound);
void Swarm_ShootNative(cAbstractFile *f);	/* "SwarmClientFire" */
void Swarm_Shoot(const char *name);
void Swarm_NetCreateNative(cAbstractFile *f);	/* "SwarmCreate" */
void Swarm_HudNative(cAbstractFile *f);	/* "SwarmFX" */
void Swarm_FireNative(cAbstractFile *f);	/* "SwarmFire" */
void Swarm_HitFXNative(cAbstractFile *f);	/* "SwarmHit" */
cEntity *Swarm_HitFX(const char *name, const char *ship, const char *target);
void Swarm_OnDamageNative(cAbstractFile *f);
void Swarm_OnDeathNative(cAbstractFile *f);
void Swarm_AimOffNative(cAbstractFile *f);	/* "SwarmOnEnd" */
void Swarm_AimOnNative(cAbstractFile *f);	/* "SwarmOnStart" */
void Swarm_AimMode3Native(cAbstractFile *f);	/* "SwarmPress" */
void Swarm_AimMode2Native(cAbstractFile *f);	/* "SwarmRelease" */
void Swarm_ServerHitNative(cAbstractFile *f);	/* "SwarmServerHit" */
void Swarm_Hit(const char *name, const char *ship, const char *target);
void Swarm_NetSetUpNative(cAbstractFile *f);	/* "SwarmSetUp" */
byte Tesla_Shoot(cEntity *v);

/* Tesla.c */
void Tesla_CreateNative(cAbstractFile *f);	/* "TeslaCreate" */
void Tesla_HudNative(cAbstractFile *f);		/* "TeslaFX" */
void Tesla_FireNative(cAbstractFile *f);	/* "TeslaFire" */
void Tesla_Push(cEntity *v, cEntity *e, int n);
void Tesla_AimOnNative(cAbstractFile *f);	/* "TeslaPress" */
void Tesla_AimOffNative(cAbstractFile *f);	/* "TeslaRelease" */
void Tesla_AimOn(cEntity *v);

/* Vulcan.c */
void Vulcan_Shoot(cEntity *v);
void Vulcan_CreateNative(cAbstractFile *f);	/* "VulcanCreate" */
void Vulcan_HudNative(cAbstractFile *f);	/* "VulcanFX" */
void Vulcan_FireNative(cAbstractFile *f);	/* "VulcanFire" */
void Vulcan_Push(cEntity *v, cEntity *e, int n);
void Vulcan_AimOnNative(cAbstractFile *f);	/* "VulcanPress" */
void Vulcan_AimOffNative(cAbstractFile *f);	/* "VulcanRelease" */
void Vulcan_AimOn(cEntity *v);

#endif
