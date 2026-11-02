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

extern cGVarFunction GVar_CreateSonicFunc;
extern cGVarFunction GVar_SetUpSonicFunc;

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
extern cGVar GVar_InfernoEngineSoundVol;
#define g_InfernoEngineSoundVol (GVar_InfernoEngineSoundVol.value.f)
extern cGVar GVar_InfernoRangeMin;
#define g_InfernoRangeMin (GVar_InfernoRangeMin.value.f)
extern cGVar GVar_InfernoTimeDelay;
#define g_InfernoTimeDelay (GVar_InfernoTimeDelay.value.f)
extern cGVar GVar_InfernoAmmoCost;
#define g_InfernoAmmoCost (GVar_InfernoAmmoCost.value.i)
extern cGVar GVar_InfernoUpTopSpeed;
#define g_InfernoUpTopSpeed (GVar_InfernoUpTopSpeed.value.f)
extern cGVar GVar_InfernoIDAmmo;
#define g_InfernoIDAmmo (GVar_InfernoIDAmmo.value.i)
extern cGVar GVar_InfernoLifeTime;
#define g_InfernoLifeTime (GVar_InfernoLifeTime.value.f)
extern cGVar GVar_InfernoEngineSoundAttIni;
#define g_InfernoEngineSoundAttIni (GVar_InfernoEngineSoundAttIni.value.f)
extern cGVar GVar_InfernoTurnSpeed;
#define g_InfernoTurnSpeed (GVar_InfernoTurnSpeed.value.f)
extern cGVar GVar_InfernoMaxDamage;
#define g_InfernoMaxDamage (GVar_InfernoMaxDamage.value.f)
extern cGVar GVar_InfernoHitSoundVol;
#define g_InfernoHitSoundVol (GVar_InfernoHitSoundVol.value.f)
extern cGVar GVar_InfernoHitSoundAttEndRadius;
#define g_InfernoHitSoundAttEndRadius (GVar_InfernoHitSoundAttEndRadius.value.f)
extern cGVar GVar_InfernoEngineSoundDoppler;
#define g_InfernoEngineSoundDoppler (GVar_InfernoEngineSoundDoppler.value.f)
extern cGVar GVar_InfernoBeesSpeed;
#define g_InfernoBeesSpeed (GVar_InfernoBeesSpeed.value.f)
extern cGVar GVar_InfernoUpgFireFactor;
#define g_InfernoUpgFireFactor (GVar_InfernoUpgFireFactor.value.f)
extern cGVar GVar_InfernoInvUpgExtraDamage;
#define g_InfernoInvUpgExtraDamage (GVar_InfernoInvUpgExtraDamage.value.f)
extern cGVar GVar_InfernoHitForce;
#define g_InfernoHitForce (GVar_InfernoHitForce.value.f)
extern cGVar GVar_InfernoRangeBay;
#define g_InfernoRangeBay (GVar_InfernoRangeBay.value.f)
extern cGVar GVar_InfernoArmor;
#define g_InfernoArmor (GVar_InfernoArmor.value.i)
extern cGVar GVar_InfernoUpgFireTime;
#define g_InfernoUpgFireTime (GVar_InfernoUpgFireTime.value.f)
extern cGVar GVar_InfernoHitSound;
#define g_InfernoHitSound (GVar_InfernoHitSound.value.s)
extern cGVar GVar_InfernoMinDamage;
#define g_InfernoMinDamage (GVar_InfernoMinDamage.value.f)
extern cGVar GVar_InfernoEngineSound;
#define g_InfernoEngineSound (GVar_InfernoEngineSound.value.s)
extern cGVar GVar_InfernoNormSpeed;
#define g_InfernoNormSpeed (GVar_InfernoNormSpeed.value.f)
#define g_InfernoXplosionCall (GAME_GLOBAL(cPy_NativeFunc, NativeFunc_InfernoXplosionCall).func)	/* the function of the native "InfernoXplosionCall" */
extern cGVar GVar_InfernoMissileFile;
#define g_InfernoMissileFile (GVar_InfernoMissileFile.value.s)
extern cGVar GVar_FXInfernoExplosionMaxDist;
#define g_FXInfernoExplosionMaxDist (GVar_FXInfernoExplosionMaxDist.value.f)
extern cGVar GVar_InfernoDistLimit;
#define g_InfernoDistLimit (GVar_InfernoDistLimit.value.f)
extern cGVar GVar_InfernoHitSoundAttIniRadius;
#define g_InfernoHitSoundAttIniRadius (GVar_InfernoHitSoundAttIniRadius.value.f)
extern cGVar GVar_InfernoEngineSoundAttEnd;
#define g_InfernoEngineSoundAttEnd (GVar_InfernoEngineSoundAttEnd.value.f)
extern cGVar GVar_InfernoExplosionFile;
#define g_InfernoExplosionFile (GVar_InfernoExplosionFile.value.s)
extern cGVar GVar_LaserAmmoCost;
#define g_LaserAmmoCost (GVar_LaserAmmoCost.value.f)
extern cGVar GVar_LaserRange;
#define g_LaserRange (GVar_LaserRange.value.f)
extern cGVar GVar_LaserShootFile;
#define g_LaserShootFile (GVar_LaserShootFile.value.s)
extern cGVar GVar_LaserHitForce;
#define g_LaserHitForce (GVar_LaserHitForce.value.f)
extern cGVar GVar_LaserHitFile;
#define g_LaserHitFile (GVar_LaserHitFile.value.s)
extern cGVar GVar_HookEMITime;
#define g_HookEMITime (GVar_HookEMITime.value.f)
extern cGVar GVar_LaserTimeDelay;
#define g_LaserTimeDelay (GVar_LaserTimeDelay.value.f)
extern cGVar GVar_LaserRicSoundVol;
#define g_LaserRicSoundVol (GVar_LaserRicSoundVol.value.f)
extern cGVar GVar_LaserHookFile;
#define g_LaserHookFile (GVar_LaserHookFile.value.s)
extern cGVar GVar_LaserDamage;
#define g_LaserDamage (GVar_LaserDamage.value.i)
extern cGVar GVar_HookAccel;
#define g_HookAccel (GVar_HookAccel.value.f)
extern cGVar GVar_LaserDistLimit;
#define g_LaserDistLimit (GVar_LaserDistLimit.value.f)
extern cGVar GVar_LaserMinSpeed;
#define g_LaserMinSpeed (GVar_LaserMinSpeed.value.f)
extern cGVar GVar_LaserRicSound;
#define g_LaserRicSound (GVar_LaserRicSound.value.s)
extern cGVar GVar_SonicRechargeTime;
#define g_SonicRechargeTime (GVar_SonicRechargeTime.value.f)
extern cGVar GVar_SonicArmor;
#define g_SonicArmor (GVar_SonicArmor.value.i)
extern cGVar GVar_FXSonicXpldeFile;
#define g_FXSonicXpldeFile (GVar_FXSonicXpldeFile.value.s)
extern cGVar GVar_SonicLifeTime;
#define g_SonicLifeTime (GVar_SonicLifeTime.value.f)
extern cGVar GVar_SonicIDAmmo;
#define g_SonicIDAmmo (GVar_SonicIDAmmo.value.i)
extern cGVar GVar_SonicAmmoNeed;
#define g_SonicAmmoNeed (GVar_SonicAmmoNeed.value.i)
#define g_CreateSonicFunc (((cPy_Function *)&GVar_CreateSonicFunc.func))
#define g_SetUpSonicFunc (((cPy_Function *)&GVar_SetUpSonicFunc.func))
extern cGVar GVar_SwarmHitSoundVol;
#define g_SwarmHitSoundVol (GVar_SwarmHitSoundVol.value.f)
extern cGVar GVar_SwarmMissileFile;
#define g_SwarmMissileFile (GVar_SwarmMissileFile.value.s)
extern cGVar GVar_SwarmUpTopSpeed;
#define g_SwarmUpTopSpeed (GVar_SwarmUpTopSpeed.value.f)
extern cGVar GVar_SwarmEngineSoundAttIni;
#define g_SwarmEngineSoundAttIni (GVar_SwarmEngineSoundAttIni.value.f)
extern cGVar GVar_SwarmBeeSoundAttEnd;
#define g_SwarmBeeSoundAttEnd (GVar_SwarmBeeSoundAttEnd.value.f)
extern cGVar GVar_SwarmDeathTime;
#define g_SwarmDeathTime (GVar_SwarmDeathTime.value.f)
extern cGVar GVar_SwarmBeeRadius;
#define g_SwarmBeeRadius (GVar_SwarmBeeRadius.value.f)
extern cGVar GVar_SwarmMinRadius;
#define g_SwarmMinRadius (GVar_SwarmMinRadius.value.f)
extern cGVar GVar_SwarmBeesSpeed;
#define g_SwarmBeesSpeed (GVar_SwarmBeesSpeed.value.f)
extern cGVar GVar_SwarmHitSound;
#define g_SwarmHitSound (GVar_SwarmHitSound.value.s)
extern cGVar GVar_SwarmHitSoundAttIni;
#define g_SwarmHitSoundAttIni (GVar_SwarmHitSoundAttIni.value.f)
extern cGVar GVar_SwarmNormSpeed;
#define g_SwarmNormSpeed (GVar_SwarmNormSpeed.value.f)
extern cGVar GVar_SwarmLifeTime;
#define g_SwarmLifeTime (GVar_SwarmLifeTime.value.f)
extern cGVar GVar_SwarmHitForce;
#define g_SwarmHitForce (GVar_SwarmHitForce.value.f)
extern cGVar GVar_SwarmTurnSpeedUPG;
#define g_SwarmTurnSpeedUPG (GVar_SwarmTurnSpeedUPG.value.f)
extern cGVar GVar_SwarmFastSpeed;
#define g_SwarmFastSpeed (GVar_SwarmFastSpeed.value.f)
extern cGVar GVar_SwarmTurnSpeed;
#define g_SwarmTurnSpeed (GVar_SwarmTurnSpeed.value.f)
extern cGVar GVar_SwarmArmor;
#define g_SwarmArmor (GVar_SwarmArmor.value.i)
extern cGVar GVar_SwarmEngineSound;
#define g_SwarmEngineSound (GVar_SwarmEngineSound.value.s)
extern cGVar GVar_SwarmBeeSoundVol;
#define g_SwarmBeeSoundVol (GVar_SwarmBeeSoundVol.value.f)
extern cGVar GVar_SwarmIDAmmo;
#define g_SwarmIDAmmo (GVar_SwarmIDAmmo.value.i)
extern cGVar GVar_SwarmEngineSoundVol;
#define g_SwarmEngineSoundVol (GVar_SwarmEngineSoundVol.value.f)
extern cGVar GVar_SwarmTimeDelay;
#define g_SwarmTimeDelay (GVar_SwarmTimeDelay.value.f)
extern cGVar GVar_SwarmBeeSoundAttIni;
#define g_SwarmBeeSoundAttIni (GVar_SwarmBeeSoundAttIni.value.f)
extern cGVar GVar_SwarmHitSoundAttEnd;
#define g_SwarmHitSoundAttEnd (GVar_SwarmHitSoundAttEnd.value.f)
extern cGVar GVar_SwarmBeeSound;
#define g_SwarmBeeSound (GVar_SwarmBeeSound.value.s)
extern cGVar GVar_SwarmNumber1;
#define g_SwarmNumber1 (GVar_SwarmNumber1.value.i)
extern cGVar GVar_SwarmEngineSoundAttEnd;
#define g_SwarmEngineSoundAttEnd (GVar_SwarmEngineSoundAttEnd.value.f)
extern cGVar GVar_SwarmEngineSoundDoppler;
#define g_SwarmEngineSoundDoppler (GVar_SwarmEngineSoundDoppler.value.f)
extern cGVar GVar_SwarmAmmoCost1;
#define g_SwarmAmmoCost1 (GVar_SwarmAmmoCost1.value.i)
extern cGVar GVar_SwarmAmmoCost2;
#define g_SwarmAmmoCost2 (GVar_SwarmAmmoCost2.value.i)
extern cGVar GVar_SwarmAmmoCost3;
#define g_SwarmAmmoCost3 (GVar_SwarmAmmoCost3.value.i)
extern cGVar GVar_SwarmAmmoCost4;
#define g_SwarmAmmoCost4 (GVar_SwarmAmmoCost4.value.i)
extern int g_SwarmAmmoCosts[6];	/* [6] SwarmAmmoCost1..4 at the start, then 0 0 */
extern cGVar GVar_TeslaDownRangeTimePad;
#define g_TeslaDownRangeTimePad (GVar_TeslaDownRangeTimePad.value.f)
extern cGVar GVar_TeslaMinAngRangePad;
#define g_TeslaMinAngRangePad (GVar_TeslaMinAngRangePad.value.f)
extern cGVar GVar_TeslaDownRangeTime;
#define g_TeslaDownRangeTime (GVar_TeslaDownRangeTime.value.f)
extern cGVar GVar_TeslaMinAngRangeUpg;
#define g_TeslaMinAngRangeUpg (GVar_TeslaMinAngRangeUpg.value.f)
extern cGVar GVar_TeslaMinAngRangeUpgPad;
#define g_TeslaMinAngRangeUpgPad (GVar_TeslaMinAngRangeUpgPad.value.f)
extern cGVar GVar_TeslaMinAngRange;
#define g_TeslaMinAngRange (GVar_TeslaMinAngRange.value.f)
extern cGVar GVar_TeslaTrackForce;
#define g_TeslaTrackForce (GVar_TeslaTrackForce.value.f)
extern cGVar GVar_TeslaShootFile;
#define g_TeslaShootFile (GVar_TeslaShootFile.value.s)
extern cGVar GVar_TeslaMaxAngRangePad;
#define g_TeslaMaxAngRangePad (GVar_TeslaMaxAngRangePad.value.f)
extern cGVar GVar_TeslaUpRangeTime;
#define g_TeslaUpRangeTime (GVar_TeslaUpRangeTime.value.f)
extern cGVar GVar_TeslaRange;
#define g_TeslaRange (GVar_TeslaRange.value.f)
extern cGVar GVar_TeslaHitForce;
#define g_TeslaHitForce (GVar_TeslaHitForce.value.f)
extern cGVar GVar_TeslaAmmoCost;
#define g_TeslaAmmoCost (GVar_TeslaAmmoCost.value.f)
extern cGVar GVar_TeslaDistLimit;
#define g_TeslaDistLimit (GVar_TeslaDistLimit.value.f)
extern cGVar GVar_TeslaMaxAngRange;
#define g_TeslaMaxAngRange (GVar_TeslaMaxAngRange.value.f)
extern cGVar GVar_VulcanIDWeap;
#define g_VulcanIDWeap (GVar_VulcanIDWeap.value.i)
extern cGVar GVar_VulcanIDAmmo;
#define g_VulcanIDAmmo (GVar_VulcanIDAmmo.value.i)
extern cGVar GVar_FXVulcanHitDistance;
#define g_FXVulcanHitDistance (GVar_FXVulcanHitDistance.value.f)
extern cGVar GVar_VulcanTimeDelay;
#define g_VulcanTimeDelay (GVar_VulcanTimeDelay.value.f)
extern cGVar GVar_VulcanRic2Sound;
#define g_VulcanRic2Sound (GVar_VulcanRic2Sound.value.s)
extern cGVar GVar_VulcanRicSoundAttIni;
#define g_VulcanRicSoundAttIni (GVar_VulcanRicSoundAttIni.value.f)
extern cGVar GVar_VulcanDistLimitUPG;
#define g_VulcanDistLimitUPG (GVar_VulcanDistLimitUPG.value.f)
extern cGVar GVar_VulcanTimeDelayUPG;
#define g_VulcanTimeDelayUPG (GVar_VulcanTimeDelayUPG.value.f)
extern cGVar GVar_VulcanHitForce;
#define g_VulcanHitForce (GVar_VulcanHitForce.value.f)
extern cGVar GVar_VulcanDamageEasy;
#define g_VulcanDamageEasy (GVar_VulcanDamageEasy.value.i)
extern cGVar GVar_VulcanDistLimit;
#define g_VulcanDistLimit (GVar_VulcanDistLimit.value.f)
extern cGVar GVar_VulcanHitFile;
#define g_VulcanHitFile (GVar_VulcanHitFile.value.s)
extern cGVar GVar_VulcanRic1Sound;
#define g_VulcanRic1Sound (GVar_VulcanRic1Sound.value.s)
extern cGVar GVar_VulcanRic0Sound;
#define g_VulcanRic0Sound (GVar_VulcanRic0Sound.value.s)
extern cGVar GVar_VulcanAmmoCost;
#define g_VulcanAmmoCost (GVar_VulcanAmmoCost.value.f)
extern cGVar GVar_VulcanBulletFile;
#define g_VulcanBulletFile (GVar_VulcanBulletFile.value.s)
extern cGVar GVar_VulcanDamage;
#define g_VulcanDamage (GVar_VulcanDamage.value.i)
extern cGVar GVar_VulcanHitUseNormal;
#define g_VulcanHitUseNormal (GVar_VulcanHitUseNormal.value.i)
extern cGVar GVar_VulcanRic0SoundVol;
#define g_VulcanRic0SoundVol (GVar_VulcanRic0SoundVol.value.f)
extern cGVar GVar_VulcanRic2SoundVol;
#define g_VulcanRic2SoundVol (GVar_VulcanRic2SoundVol.value.f)
extern cGVar GVar_VulcanRic1SoundVol;
#define g_VulcanRic1SoundVol (GVar_VulcanRic1SoundVol.value.f)
extern cGVar GVar_VulcanRange;
#define g_VulcanRange (GVar_VulcanRange.value.f)
extern cGVar GVar_VulcanRicSoundAttEnd;
#define g_VulcanRicSoundAttEnd (GVar_VulcanRicSoundAttEnd.value.f)

/* the state of the module */
extern char g_InfernoHitName[128];	/* 0x80, the entity of the sphere callback */
extern cEntity *g_InfernoOwner;	/* the ship of the exploding missile, NULL if none */
extern float g_InfernoRange;
extern cEntity *g_InfernoMissile;
extern char g_InfernoHudTarget[4][0x80];	/* per player: the target shown */
extern float g_InfernoHudTime[4];	/* per player */
extern vec3_t g_InfernoPos;
extern vec3_t g_SonicHitPos;
extern char g_SwarmHudTarget[4][0x80];
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
