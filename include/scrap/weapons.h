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

extern cGVar GVar_CannonScorerColorBlue;
extern cGVar GVar_CannonScorerColorGreen;
extern cGVar GVar_CannonScorerColorRed;
extern cGVarFunction GVar_RestoreUpgradeFunc;

typedef cTargetLock cWeaponManager;

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
#define g_RestoreUpgradeFunc (((cPy_Function *)&GVar_RestoreUpgradeFunc.func))
extern cGVar GVar_VehicleCenterPadLock;
#define g_VehicleCenterPadLock (GVar_VehicleCenterPadLock.value.f)
#define g_CannonScorerColorRed (GVar_CannonScorerColorRed.value.u)
#define g_CannonScorerColorGreen (GVar_CannonScorerColorGreen.value.u)
#define g_CannonScorerColorBlue (GVar_CannonScorerColorBlue.value.u)

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
void cWeaponManager_SetWeapList(cWeaponManager *self, const char *list);
byte cWeaponManager_SetUpgrade(cWeaponManager *self, int w, int upgrade);
byte cWeaponManager_SetUAmmo(cWeaponManager *self, int w, float v);
void cWeaponManager_SetWeapSnd(cWeaponManager *self, int snd);
void cWeaponManager_Update(cWeaponManager *self, float dt);
void cWeaponManager_NextWeapSnd(cWeaponManager *self, int unused);
byte cWeaponManager_VarS(cWeaponManager *self, const char *name, char **v, byte set);
byte cWeaponManager_VarV(cWeaponManager *self, const char *name, double *x, double *y, double *z, byte set);

extern cGVar GVar_ATPCRange;
#define g_ATPCRange (GVar_ATPCRange.value.f)
extern cGVar GVar_ATPCTrailTime;
#define g_ATPCTrailTime (GVar_ATPCTrailTime.value.f)

/* a sound at pos with its attenuation and volume */
void ATPC_PlaySound(const char *name, const vec3_t *pos, float attIni, float attEnd, float vol);

extern cGVar GVar_DevastatorRange;
#define g_DevastatorRange (GVar_DevastatorRange.value.f)
extern cGVar GVar_DevastatorDistLimit;
#define g_DevastatorDistLimit (GVar_DevastatorDistLimit.value.f)
extern cGVar GVar_EMILowRadiusFactor;
#define g_EMILowRadiusFactor (GVar_EMILowRadiusFactor.value.f)
/* WeaponManager.cpp before rva 0x11a0f0: the fire stamp delayed by t (to -t when it is beyond t) */
void cWeaponManager_DelayFire(cWeaponManager *self, float t);
cWeaponManager *cWeaponManager_ctor(cWeaponManager *self);
void cWeaponManager_dtor(cWeaponManager *self);
void cWeaponManager_ClearAmmo(cWeaponManager *self);
void cWeaponManager_ClearWeaps(cWeaponManager *self);

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
