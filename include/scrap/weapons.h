/*
 * The vehicle weapons (original source: Entities/Vehicle/): WeaponManager.cpp, the weapons of a vehicle (cWeaponManager,
 * the struct cTargetLock of include/scrap/fxcountermeasures.h, at +0x5f0 of the vehicle), and the native functions of
 * the weapon scripts in Weaps/ATPC.cpp and Weaps/Devastator.cpp ("!ATPCFire" and so on, cPy_NativeFunc: the parameters
 * come in a memory file). The shared parts of the vehicle weapons are here; Inferno, Laser and Swarm are in
 * include/scrap/weaps2.h.
 */
#ifndef SCRAP_WEAPONS_H
#define SCRAP_WEAPONS_H

#include "scrap/autoaim.h"
#include "scrap/camcontrol.h"
#include "scrap/controls.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/filesys.h"
#include "scrap/fxweapons.h"
#include "scrap/missileentity.h"
#include "scrap/scoreritems.h"
#include "scrap/sound.h"
#include "scrap/vehicleattack.h"
#include "scrap/vehicleentity.h"
#include "scrap/world.h"

typedef cTargetLock cWeaponManager;

#define CWEAPONMANAGER_VFTABLE GAME_VFTABLE(cWeaponManager_vftable)
#define TYEN_VEHICLE "Car"			/* the string at rva 0x3a77b8 */

/* the fields of the vehicles (cVehicleEntity) and other entities the weapons use */
#define Entity_FollowCam(e) ((cFollowCamEntity *)Obj(e)->cam)	/* of a player, NULL else */
#define Vehicle_AimTarget(e) ((cEntity *)Veh(e)->autoAim.target.target)
#define Vehicle_Effects(e) ((void *)&Veh(e)->effects)	/* Entities/Vehicle/VehicleEffects.cpp */


/* the shot effect of the weapon slot */
void cVehicleEffects_Shoot(void *self, int weap, int slot);
/* World/FX/FXWeapons.cpp: the ATPC shot of the vehicle from origin to hit */
void cObjElementFXATPC_Shoot(cObjElementFXATPC *self, cEntity *v, const char *target, vec3_t hit,
	vec3_t origin);
/* a random float between lo and hi */
float Rand_FloatRange(float lo, float hi);

/* the AI of a vehicle takes the new weapon list */
void cAIVehicle_WeapListChanged(void *ai);

/* Sound/SndInterface.cpp */
void cSndItem_UpdateAt(cSndItem *self, cEntity *e);
void cSndItem_Restart(cSndItem *self);

/* the crosshair item of the HUD the weapons animate (a cCannonScorer, found by its name) */

/* the engine variables */
#define g_LaserIDWeap GAME_GLOBAL_IN(int, GVar_LaserIDWeap, 16)
#define g_ATPCIDWeap GAME_GLOBAL_IN(int, GVar_ATPCIDWeap, 16)
#define g_DevastatorIDWeap GAME_GLOBAL_IN(int, GVar_DevastatorIDWeap, 16)
#define g_ATPCIDAmmo GAME_GLOBAL_IN(int, GVar_ATPCIDAmmo, 16)
#define g_DevastatorIDAmmo GAME_GLOBAL_IN(int, GVar_DevastatorIDAmmo, 16)
#define g_ATPCDelay GAME_GLOBAL_IN(float, GVar_ATPCDelay, 16)
#define g_ATPCUpgradeDelay GAME_GLOBAL_IN(float, GVar_ATPCUpgradeDelay, 16)
#define g_HookPlugFunc (GAME_ARRAY_IN(cPy_Function, GVar_HookPlugFunc, 16))
#define g_RestoreUpgradeFunc (GAME_ARRAY_IN(cPy_Function, GVar_RestoreUpgradeFunc, 16))
#define g_WeapListBuf (GAME_ARRAY(char, g_WeapListBuf))		/* 0x200 bytes, GetWeapList */
#define g_WeaponNeedOrder (GAME_ARRAY(const int[6], g_WeaponNeedOrder))	/* three orders of six weapons */
#define g_VehicleCenterPadLock GAME_GLOBAL_IN(float, GVar_VehicleCenterPadLock, 16)
#define g_CannonScorerColorRed GAME_GLOBAL_IN(uint32_t, GVar_CannonScorerColorRed, 16)
#define g_CannonScorerColorGreen GAME_GLOBAL_IN(uint32_t, GVar_CannonScorerColorGreen, 16)
#define g_CannonScorerColorBlue GAME_GLOBAL_IN(uint32_t, GVar_CannonScorerColorBlue, 16)

/* the vehicle of that name, CHECKed (the start of every native function of a weapon) */
static inline cEntity *Weap_GetVehicle(const char *name) {
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_VEHICLE));
	return v;
}

/* the x87 code of the original: a float minus (divided by) a float, rounded by the x87 control word, then to float */
static inline float Weap_X87Sub(float a, float b) {
	long double r;
	__asm__("fsubs %2" : "=t"(r) : "0"((long double)a), "m"(b));
	return (float)r;
}
static inline float Weap_X87Div(float a, float b) {
	long double r;
	__asm__("fdivs %2" : "=t"(r) : "0"((long double)a), "m"(b));
	return (float)r;
}

/* rand() in [0, 1] (double) */
#define g_WeapRandScale GAME_GLOBAL(double, g_FxRandScale)	/* 1/32767 */
static inline double Weap_Rand01(void) {
	return (double)(crt_rand() & 0x7fff) * g_WeapRandScale;
}

/* the cannon color of the HUD items (alpha 0xbf) */
static inline uint32_t Weap_CannonColor(void) {
	return ((g_CannonScorerColorRed | 0xffffbf00u) << 8 | g_CannonScorerColorGreen) << 8 | g_CannonScorerColorBlue;
}

/* WeaponManager.cpp */
int cWeaponManager_HasMinAmmo(cWeaponManager *self, uint32_t w);
byte cWeaponManager_VarF(cWeaponManager *self, const char *name, double *v, byte set);
int cWeaponManager_FixWeapon(int w);
int cWeaponManager_NumAmmoSlots(cWeaponManager *self, int ammo);
int cWeaponManager_NumSlots(cWeaponManager *self, int w, byte warn);
float cWeaponManager_GetWeapAmmo(cWeaponManager *self, uint32_t w);
uint32_t cWeaponManager_GetSlots(cWeaponManager *self, int w);
char *cWeaponManager_GetWeapList(cWeaponManager *self);
int cWeaponManager_GetUpgrade(cWeaponManager *self, int w);
float cWeaponManager_GetUAmmo(cWeaponManager *self, int w);
void cWeaponManager_Init(cWeaponManager *self, cEntity *owner);
byte cWeaponManager_VarI(cWeaponManager *self, const char *name, int *v, byte set);
int cWeaponManager_NextWeap(cWeaponManager *self, uint32_t w);
int cWeaponManager_PrevWeap(cWeaponManager *self, uint32_t w);
float cWeaponManager_SetAmmo(cWeaponManager *self, int ammo, float v);
void cWeaponManager_LockEntity(cWeaponManager *self, const char *name);
void cWeaponManager_LockPoint(cWeaponManager *self, const vec3_t *p);
void cWeaponManager_SetTeslaFiring(cWeaponManager *self, byte v);
uint32_t cWeaponManager_SetSlots(cWeaponManager *self, int w, int mask);
void cWeaponManager_SetWeapList(cWeaponManager *self, const char *list);
byte cWeaponManager_SetSlot(cWeaponManager *self, int w, int slot, byte on);
byte cWeaponManager_SetUpgrade(cWeaponManager *self, int w, int upgrade);
byte cWeaponManager_SetUAmmo(cWeaponManager *self, int w, float v);
void cWeaponManager_SetWeapSnd(cWeaponManager *self, int snd);
void cWeaponManager_Update(cWeaponManager *self, float dt);
void cWeaponManager_NextWeapSnd(cWeaponManager *self, int unused);
byte cWeaponManager_VarS(cWeaponManager *self, const char *name, char **v, byte set);
byte cWeaponManager_VarV(cWeaponManager *self, const char *name, double *x, double *y, double *z, byte set);
void cWeaponManager_PlayStartSnd(cWeaponManager *self);
int cWeaponManager_WeaponNeed(cWeaponManager *self);

/* Weaps/ATPC.cpp: the shot of the ATPC (a charge per mounted slot), its HUD item and the debris cloud */
#define g_ATPCShooter (GAME_ARRAY(char, g_ATPCShooter))		/* 0x80 bytes, the vehicle of the last shot */
#define g_ATPCVictim (GAME_ARRAY(char, g_ATPCVictim))		/* 0x80 bytes */
#define g_ATPCNumSlots GAME_GLOBAL(float, g_ATPCNumSlots)		/* the slots of the last shot */
#define g_ATPCAmmoCost GAME_GLOBAL_IN(float, GVar_ATPCAmmoCost, 16)
#define g_ATPCExtraDamage GAME_GLOBAL_IN(int, GVar_ATPCExtraDamage, 16)
#define g_ATPCExplosionRadius GAME_GLOBAL_IN(float, GVar_ATPCExplosionRadius, 16)
#define g_ATPCMainDamage GAME_GLOBAL_IN(int, GVar_ATPCMainDamage, 16)
#define g_ATPCHitForce GAME_GLOBAL_IN(float, GVar_ATPCHitForce, 16)
#define g_ATPCRange GAME_GLOBAL_IN(float, GVar_ATPCRange, 16)
#define g_ATPCConvDist GAME_GLOBAL_IN(float, GVar_ATPCConvDist, 16)
#define g_ATPCCapsuleSoundAttEnd GAME_GLOBAL_IN(float, GVar_ATPCCapsuleSoundAttEnd, 16)
#define g_ATPCCapsuleSoundAttIni GAME_GLOBAL_IN(float, GVar_ATPCCapsuleSoundAttIni, 16)
#define g_ATPCCapsuleSound GAME_GLOBAL_IN(const char *, GVar_ATPCCapsuleSound, 16)
#define g_ATPCCapsuleSoundRadius GAME_GLOBAL_IN(float, GVar_ATPCCapsuleSoundRadius, 16)
#define g_ATPCTrailTime GAME_GLOBAL_IN(float, GVar_ATPCTrailTime, 16)
#define g_ATPCCapsuleExtendedSound GAME_GLOBAL_IN(const char *, GVar_ATPCCapsuleExtendedSound, 16)
#define g_ATPCHitSound GAME_GLOBAL_IN(const char *, GVar_ATPCHitSound, 16)
#define g_ATPCHitSoundAttEnd GAME_GLOBAL_IN(float, GVar_ATPCHitSoundAttEnd, 16)
#define g_ATPCHitSoundAttIni GAME_GLOBAL_IN(float, GVar_ATPCHitSoundAttIni, 16)
#define g_ATPCHitSoundVol GAME_GLOBAL_IN(float, GVar_ATPCHitSoundVol, 16)
#define g_ATPCCapsuleSoundVol GAME_GLOBAL_IN(float, GVar_ATPCCapsuleSoundVol, 16)
#define g_ATPCShootFile GAME_GLOBAL_IN(const char *, GVar_ATPCShootFile, 16)
#define g_ATPCCapsuleExtendedSoundAttIni GAME_GLOBAL_IN(float, GVar_ATPCCapsuleExtendedSoundAttIni, 16)
#define g_ATPCCapsuleExtendedSoundAttEnd GAME_GLOBAL_IN(float, GVar_ATPCCapsuleExtendedSoundAttEnd, 16)
#define g_ATPCCapsuleExtendedSoundVol GAME_GLOBAL_IN(float, GVar_ATPCCapsuleExtendedSoundVol, 16)
#define g_CreateCloudFunc (GAME_ARRAY_IN(cPy_Function, GVar_CreateCloudFunc, 16))
#define g_CloudLifeTime GAME_GLOBAL_IN(float, GVar_CloudLifeTime, 16)
#define g_CloudAmmoNeed GAME_GLOBAL_IN(int, GVar_CloudAmmoNeed, 16)
#define g_CloudIDAmmo GAME_GLOBAL_IN(int, GVar_CloudIDAmmo, 16)
#define g_CloudRechargeTime GAME_GLOBAL_IN(float, GVar_CloudRechargeTime, 16)
#define g_SetUpCloudFunc (GAME_ARRAY_IN(cPy_Function, GVar_SetUpCloudFunc, 16))

/* a sound at pos with its attenuation and volume */
void ATPC_PlaySound(const char *name, const vec3_t *pos, float attIni, float attEnd, float vol);
void ATPC_SetRecharge(cEntity *v);
void ATPC_Create(cAbstractFile *params);
void ATPC_FX(cAbstractFile *params);
void ATPC_Fire(cAbstractFile *params);
void ATPC_OnEnd(cAbstractFile *params);
void ATPC_OnStart(cAbstractFile *params);
void ATPC_XplosionCall(cAbstractFile *params);
void Cloud_Fire(cAbstractFile *params);
void Cloud_OnHit(cAbstractFile *params);

/* Weaps/Devastator.cpp: the Devastator and the EMI */
#define g_DevShooter (GAME_ARRAY(char, g_DevShooter))		/* 0x80 bytes */
#define g_DevVictim (GAME_ARRAY(char, g_DevVictim))		/* 0x80 bytes */
#define g_DevastatorRechargeTime GAME_GLOBAL_IN(float, GVar_DevastatorRechargeTime, 16)
#define g_DevastatorExplosionRadius GAME_GLOBAL_IN(float, GVar_DevastatorExplosionRadius, 16)
#define g_DevastatorExtraDamage GAME_GLOBAL_IN(float, GVar_DevastatorExtraDamage, 16)
#define g_DevastatorHitForce GAME_GLOBAL_IN(float, GVar_DevastatorHitForce, 16)
#define g_DevastatorMinDamage GAME_GLOBAL_IN(float, GVar_DevastatorMinDamage, 16)
#define g_DevastatorSpreadAngle GAME_GLOBAL_IN(float, GVar_DevastatorSpreadAngle, 16)
#define g_DevastatorRange GAME_GLOBAL_IN(float, GVar_DevastatorRange, 16)
#define g_DevastatorDistLimit GAME_GLOBAL_IN(float, GVar_DevastatorDistLimit, 16)
#define g_DevCenter (GAME_ARRAY(vec3_t, g_DevCenter))		/* the explosion */
#define g_DevastatorAmmoCost GAME_GLOBAL_IN(float, GVar_DevastatorAmmoCost, 16)
#define g_DevastatorBulletFile GAME_GLOBAL_IN(const char *, GVar_DevastatorBulletFile, 16)
#define g_DevastatorHitFile GAME_GLOBAL_IN(const char *, GVar_DevastatorHitFile, 16)
#define g_DevastatorShootFile GAME_GLOBAL_IN(const char *, GVar_DevastatorShootFile, 16)
#define g_DevastatorSparksFile GAME_GLOBAL_IN(const char *, GVar_DevastatorSparksFile, 16)
#define g_DevastatorBoomSound GAME_GLOBAL_IN(const char *, GVar_DevastatorBoomSound, 16)
#define g_DevastatorBoomSoundAttEnd GAME_GLOBAL_IN(float, GVar_DevastatorBoomSoundAttEnd, 16)
#define g_DevastatorBoomSoundAttIni GAME_GLOBAL_IN(float, GVar_DevastatorBoomSoundAttIni, 16)
#define g_DevastatorBoomSoundVol GAME_GLOBAL_IN(float, GVar_DevastatorBoomSoundVol, 16)
#define g_FXDevastatorDistance GAME_GLOBAL_IN(float, GVar_FXDevastatorDistance, 16)
#define g_FXDevastatorRadius GAME_GLOBAL_IN(float, GVar_FXDevastatorRadius, 16)
#define g_FXDevastatorScale GAME_GLOBAL_IN(float, GVar_FXDevastatorScale, 16)
#define g_DevXplosionNative GAME_GLOBAL_IN(nativefunc_t, NativeFunc_DevastatorXplosionCall, 4)	/* the function of "!DevastatorXplosionCall" */
#define g_DevColorRamps (GAME_ARRAY(uint32_t, g_DevColorRamps))	/* three color ramps of 9 words */
#define g_CreateEMIFunc (GAME_ARRAY_IN(cPy_Function, GVar_CreateEMIFunc, 16))
#define g_EMIRechargeTime GAME_GLOBAL_IN(float, GVar_EMIRechargeTime, 16)
#define g_EMIAmmoNeed GAME_GLOBAL_IN(int, GVar_EMIAmmoNeed, 16)
#define g_EMILowRadiusFactor GAME_GLOBAL_IN(float, GVar_EMILowRadiusFactor, 16)
#define g_SetUpEMIFunc (GAME_ARRAY_IN(cPy_Function, GVar_SetUpEMIFunc, 16))
#define g_EMILifeTime GAME_GLOBAL_IN(float, GVar_EMILifeTime, 16)
#define g_EMIIDAmmo GAME_GLOBAL_IN(int, GVar_EMIIDAmmo, 16)
/* WeaponManager.cpp before rva 0x11a0f0: the fire stamp delayed by t (to -t when it is beyond t) */
void cWeaponManager_DelayFire(cWeaponManager *self, float t);
#define CWEAPONMANAGER_VFTABLE GAME_VFTABLE(cWeaponManager_vftable)
cWeaponManager *cWeaponManager_ctor(cWeaponManager *self);
void cWeaponManager_dtor(cWeaponManager *self);
cWeaponManager *cWeaponManager_Delete(cWeaponManager *self, byte flags);
/* the item i of the world weapons table, int3 out of range */
cWeapInfo *WeapTable_At(void *table, uint32_t i);
void cWeaponManager_ClearAmmo(cWeaponManager *self);
void cWeaponManager_ClearWeaps(cWeaponManager *self);

void Devastator_Start(cEntity *v);
void Devastator_Create(cAbstractFile *params);
void Devastator_FX(cAbstractFile *params);
void Devastator_Fire(cAbstractFile *params);
void Devastator_Press(cAbstractFile *params);
void Devastator_Release(cAbstractFile *params);
void Devastator_SetUp(cEntity *v);
void Devastator_XplosionCall(cAbstractFile *params);
void EMI_Fire(cAbstractFile *params);
void EMI_OnHit(cAbstractFile *params);
void Inferno_ClientFireNet(cAbstractFile *params);

/* the start sound of the weapon sound i of the vehicle (inlined in the original) */
static inline void Vehicle_StartSnd(cEntity *e, int i) {
	cSndItem *it = PtrArray_Get(&Veh(e)->wl.obj.snd, i);
	if (it) {
		it->volume = it->vols[0];
		it->pitch = it->pitches[0];
		it->playing = 1;
		cSndItem_UpdateAt(it, e);
		cSndItem_Restart(it);
	}
}

#endif
