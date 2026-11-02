/*
 * The vehicle weapons (original source: Entities/Vehicle/): WeaponManager.cpp, the weapons of a vehicle (cWeaponManager,
 * the struct cTargetLock of include/scrap/fxcountermeasures.h, at +0x5f0 of the vehicle), and the native functions of
 * the weapon scripts in Weaps/ATPC.cpp and Weaps/Devastator.cpp ("!ATPCFire" and so on, cPy_NativeFunc: the parameters
 * come in a memory file). The shared parts of the vehicle weapons are here; Inferno, Laser and Swarm are in
 * include/scrap/weaps2.h.
 */
#ifndef SCRAP_WEAPONS_H
#define SCRAP_WEAPONS_H

#include "scrap/gvar.h"
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
extern cGVar GVar_LaserIDWeap;
#define g_LaserIDWeap (GVar_LaserIDWeap.value.i)
extern cGVar GVar_ATPCIDWeap;
#define g_ATPCIDWeap (GVar_ATPCIDWeap.value.i)
extern cGVar GVar_DevastatorIDWeap;
#define g_DevastatorIDWeap (GVar_DevastatorIDWeap.value.i)
extern cGVar GVar_ATPCIDAmmo;
#define g_ATPCIDAmmo (GVar_ATPCIDAmmo.value.i)
extern cGVar GVar_DevastatorIDAmmo;
#define g_DevastatorIDAmmo (GVar_DevastatorIDAmmo.value.i)
extern cGVar GVar_ATPCDelay;
#define g_ATPCDelay (GVar_ATPCDelay.value.f)
extern cGVar GVar_ATPCUpgradeDelay;
#define g_ATPCUpgradeDelay (GVar_ATPCUpgradeDelay.value.f)
#define g_HookPlugFunc (((cPy_Function *)&GAME_GVARFUNC(GVar_HookPlugFunc)->func))
#define g_RestoreUpgradeFunc (((cPy_Function *)&GAME_GVARFUNC(GVar_RestoreUpgradeFunc)->func))
extern char g_WeapListBuf[512];		/* 0x200 bytes, GetWeapList */
#define g_WeaponNeedOrder (GAME_ARRAY(const int[6], g_WeaponNeedOrder))	/* three orders of six weapons */
extern cGVar GVar_VehicleCenterPadLock;
#define g_VehicleCenterPadLock (GVar_VehicleCenterPadLock.value.f)
#define g_CannonScorerColorRed (*(uint32_t *)&GAME_GVAR(GVar_CannonScorerColorRed)->value)
#define g_CannonScorerColorGreen (*(uint32_t *)&GAME_GVAR(GVar_CannonScorerColorGreen)->value)
#define g_CannonScorerColorBlue (*(uint32_t *)&GAME_GVAR(GVar_CannonScorerColorBlue)->value)

/* the vehicle of that name, CHECKed (the start of every native function of a weapon) */
static inline cEntity *Weap_GetVehicle(const char *name) {
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_VEHICLE));
	return v;
}

/* rand() in [0, 1] (double) */
static inline double Weap_Rand01(void) {
	return (double)(crt_rand() & 0x7fff) * RAND_SCALE;
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
extern char g_ATPCShooter[128];		/* 0x80 bytes, the vehicle of the last shot */
extern char g_ATPCVictim[128];		/* 0x80 bytes */
extern float g_ATPCNumSlots;		/* the slots of the last shot */
extern cGVar GVar_ATPCAmmoCost;
#define g_ATPCAmmoCost (GVar_ATPCAmmoCost.value.f)
extern cGVar GVar_ATPCExtraDamage;
#define g_ATPCExtraDamage (GVar_ATPCExtraDamage.value.i)
extern cGVar GVar_ATPCExplosionRadius;
#define g_ATPCExplosionRadius (GVar_ATPCExplosionRadius.value.f)
extern cGVar GVar_ATPCMainDamage;
#define g_ATPCMainDamage (GVar_ATPCMainDamage.value.i)
extern cGVar GVar_ATPCHitForce;
#define g_ATPCHitForce (GVar_ATPCHitForce.value.f)
extern cGVar GVar_ATPCRange;
#define g_ATPCRange (GVar_ATPCRange.value.f)
extern cGVar GVar_ATPCConvDist;
#define g_ATPCConvDist (GVar_ATPCConvDist.value.f)
extern cGVar GVar_ATPCCapsuleSoundAttEnd;
#define g_ATPCCapsuleSoundAttEnd (GVar_ATPCCapsuleSoundAttEnd.value.f)
extern cGVar GVar_ATPCCapsuleSoundAttIni;
#define g_ATPCCapsuleSoundAttIni (GVar_ATPCCapsuleSoundAttIni.value.f)
extern cGVar GVar_ATPCCapsuleSound;
#define g_ATPCCapsuleSound (GVar_ATPCCapsuleSound.value.s)
extern cGVar GVar_ATPCCapsuleSoundRadius;
#define g_ATPCCapsuleSoundRadius (GVar_ATPCCapsuleSoundRadius.value.f)
extern cGVar GVar_ATPCTrailTime;
#define g_ATPCTrailTime (GVar_ATPCTrailTime.value.f)
extern cGVar GVar_ATPCCapsuleExtendedSound;
#define g_ATPCCapsuleExtendedSound (GVar_ATPCCapsuleExtendedSound.value.s)
extern cGVar GVar_ATPCHitSound;
#define g_ATPCHitSound (GVar_ATPCHitSound.value.s)
extern cGVar GVar_ATPCHitSoundAttEnd;
#define g_ATPCHitSoundAttEnd (GVar_ATPCHitSoundAttEnd.value.f)
extern cGVar GVar_ATPCHitSoundAttIni;
#define g_ATPCHitSoundAttIni (GVar_ATPCHitSoundAttIni.value.f)
extern cGVar GVar_ATPCHitSoundVol;
#define g_ATPCHitSoundVol (GVar_ATPCHitSoundVol.value.f)
extern cGVar GVar_ATPCCapsuleSoundVol;
#define g_ATPCCapsuleSoundVol (GVar_ATPCCapsuleSoundVol.value.f)
extern cGVar GVar_ATPCShootFile;
#define g_ATPCShootFile (GVar_ATPCShootFile.value.s)
extern cGVar GVar_ATPCCapsuleExtendedSoundAttIni;
#define g_ATPCCapsuleExtendedSoundAttIni (GVar_ATPCCapsuleExtendedSoundAttIni.value.f)
extern cGVar GVar_ATPCCapsuleExtendedSoundAttEnd;
#define g_ATPCCapsuleExtendedSoundAttEnd (GVar_ATPCCapsuleExtendedSoundAttEnd.value.f)
extern cGVar GVar_ATPCCapsuleExtendedSoundVol;
#define g_ATPCCapsuleExtendedSoundVol (GVar_ATPCCapsuleExtendedSoundVol.value.f)
#define g_CreateCloudFunc (((cPy_Function *)&GAME_GVARFUNC(GVar_CreateCloudFunc)->func))
extern cGVar GVar_CloudLifeTime;
#define g_CloudLifeTime (GVar_CloudLifeTime.value.f)
extern cGVar GVar_CloudAmmoNeed;
#define g_CloudAmmoNeed (GVar_CloudAmmoNeed.value.i)
extern cGVar GVar_CloudIDAmmo;
#define g_CloudIDAmmo (GVar_CloudIDAmmo.value.i)
extern cGVar GVar_CloudRechargeTime;
#define g_CloudRechargeTime (GVar_CloudRechargeTime.value.f)
#define g_SetUpCloudFunc (((cPy_Function *)&GAME_GVARFUNC(GVar_SetUpCloudFunc)->func))

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
extern char g_DevShooter[128];		/* 0x80 bytes */
extern char g_DevVictim[128];		/* 0x80 bytes */
extern cGVar GVar_DevastatorRechargeTime;
#define g_DevastatorRechargeTime (GVar_DevastatorRechargeTime.value.f)
extern cGVar GVar_DevastatorExplosionRadius;
#define g_DevastatorExplosionRadius (GVar_DevastatorExplosionRadius.value.f)
extern cGVar GVar_DevastatorExtraDamage;
#define g_DevastatorExtraDamage (GVar_DevastatorExtraDamage.value.f)
extern cGVar GVar_DevastatorHitForce;
#define g_DevastatorHitForce (GVar_DevastatorHitForce.value.f)
extern cGVar GVar_DevastatorMinDamage;
#define g_DevastatorMinDamage (GVar_DevastatorMinDamage.value.f)
extern cGVar GVar_DevastatorSpreadAngle;
#define g_DevastatorSpreadAngle (GVar_DevastatorSpreadAngle.value.f)
extern cGVar GVar_DevastatorRange;
#define g_DevastatorRange (GVar_DevastatorRange.value.f)
extern cGVar GVar_DevastatorDistLimit;
#define g_DevastatorDistLimit (GVar_DevastatorDistLimit.value.f)
#define g_DevCenter (GAME_ARRAY(vec3_t, g_DevCenter))		/* the explosion */
extern cGVar GVar_DevastatorAmmoCost;
#define g_DevastatorAmmoCost (GVar_DevastatorAmmoCost.value.f)
extern cGVar GVar_DevastatorBulletFile;
#define g_DevastatorBulletFile (GVar_DevastatorBulletFile.value.s)
extern cGVar GVar_DevastatorHitFile;
#define g_DevastatorHitFile (GVar_DevastatorHitFile.value.s)
extern cGVar GVar_DevastatorShootFile;
#define g_DevastatorShootFile (GVar_DevastatorShootFile.value.s)
extern cGVar GVar_DevastatorSparksFile;
#define g_DevastatorSparksFile (GVar_DevastatorSparksFile.value.s)
extern cGVar GVar_DevastatorBoomSound;
#define g_DevastatorBoomSound (GVar_DevastatorBoomSound.value.s)
extern cGVar GVar_DevastatorBoomSoundAttEnd;
#define g_DevastatorBoomSoundAttEnd (GVar_DevastatorBoomSoundAttEnd.value.f)
extern cGVar GVar_DevastatorBoomSoundAttIni;
#define g_DevastatorBoomSoundAttIni (GVar_DevastatorBoomSoundAttIni.value.f)
extern cGVar GVar_DevastatorBoomSoundVol;
#define g_DevastatorBoomSoundVol (GVar_DevastatorBoomSoundVol.value.f)
extern cGVar GVar_FXDevastatorDistance;
#define g_FXDevastatorDistance (GVar_FXDevastatorDistance.value.f)
extern cGVar GVar_FXDevastatorRadius;
#define g_FXDevastatorRadius (GVar_FXDevastatorRadius.value.f)
extern cGVar GVar_FXDevastatorScale;
#define g_FXDevastatorScale (GVar_FXDevastatorScale.value.f)
#define g_DevXplosionNative (GAME_GLOBAL(cPy_NativeFunc, NativeFunc_DevastatorXplosionCall).func)	/* the function of "!DevastatorXplosionCall" */
extern uint32_t g_DevColorRamps[36];	/* three color ramps of 9 words */
#define g_CreateEMIFunc (((cPy_Function *)&GAME_GVARFUNC(GVar_CreateEMIFunc)->func))
extern cGVar GVar_EMIRechargeTime;
#define g_EMIRechargeTime (GVar_EMIRechargeTime.value.f)
extern cGVar GVar_EMIAmmoNeed;
#define g_EMIAmmoNeed (GVar_EMIAmmoNeed.value.i)
extern cGVar GVar_EMILowRadiusFactor;
#define g_EMILowRadiusFactor (GVar_EMILowRadiusFactor.value.f)
#define g_SetUpEMIFunc (((cPy_Function *)&GAME_GVARFUNC(GVar_SetUpEMIFunc)->func))
extern cGVar GVar_EMILifeTime;
#define g_EMILifeTime (GVar_EMILifeTime.value.f)
extern cGVar GVar_EMIIDAmmo;
#define g_EMIIDAmmo (GVar_EMIIDAmmo.value.i)
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
