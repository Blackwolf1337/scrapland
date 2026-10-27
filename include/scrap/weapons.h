/*
 * The vehicle weapons (original source: Entities/Vehicle/): WeaponManager.cpp, the weapons of a vehicle (cWeaponManager,
 * the struct cTargetLock of include/scrap/fxcountermeasures.h, at +0x5f0 of the vehicle), and the native functions of
 * the weapon scripts in Weaps/ATPC.cpp and Weaps/Devastator.cpp ("!ATPCFire" and so on, cPy_NativeFunc: the parameters
 * come in a memory file). The shared parts of the vehicle weapons are here; Inferno, Laser and Swarm are in
 * include/scrap/weaps2.h.
 */
#ifndef SCRAP_WEAPONS_H
#define SCRAP_WEAPONS_H

#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/filesys.h"
#include "scrap/fxweapons.h"
#include "scrap/missileentity.h"
#include "scrap/sound.h"
#include "scrap/world.h"

typedef cTargetLock cWeaponManager;

#define CWEAPONMANAGER_VFTABLE 0x3b664c
#define TYEN_VEHICLE "Car"			/* the string at rva 0x3a77b8 */

/* the fields of the vehicles (cVehicleEntity) and other entities the weapons use */
#define Vehicle_Sounds(e) ((cSndInterface *)((byte *)(e) + 0x144))	/* the weapon sounds by index */
#define Entity_Shake(e) (*(byte **)((byte *)(e) + 0xf0))	/* of a player: +0x220 strength, +0x224 angle */
#define Entity_Mass(e) (*(float *)((byte *)(e) + 0x230))
#define Entity_LifeI(e) (*(int *)((byte *)(e) + 0x238))			/* WithLife */
#define Entity_God(e) (*(byte *)((byte *)(e) + 0x23c))			/* no damage */
#define Vehicle_Attack(e) ((void *)((byte *)(e) + 0x298))
#define Entity_Radius(e) (*(float *)((byte *)(e) + 0x3a0))
#define Vehicle_NetSlot(e) (*(int *)((byte *)(e) + 0x410))		/* 0..3 on a net client */
#define Vehicle_Controls(e) ((byte *)(e) + 0x414)
#define Vehicle_Hit(e) (*(byte **)((byte *)(e) + 0x4fc))		/* its +0x64 is set to 2 by a hit */
#define Missile_Ammo(e) (*(int *)((byte *)(e) + 0x588))
#define Vehicle_EMITime(e) (*(float *)((byte *)(e) + 0x58c))
#define Vehicle_CloudTime(e) (*(float *)((byte *)(e) + 0x590))		/* set by cVehicleEntity_SetCloud */
#define Missile_F594(e) (*(float *)((byte *)(e) + 0x594))
#define Missile_Radius(e) (*(float *)((byte *)(e) + 0x5a4))		/* the EMI */
#define Vehicle_WeapMgr(e) ((cWeaponManager *)((byte *)(e) + 0x5f0))
#define Vehicle_CanFire(e) (*(byte *)((byte *)(e) + 0x644))
#define Vehicle_AutoAim(e) ((void *)((byte *)(e) + 0x6d0))
#define Vehicle_AimAng(e) ((float *)((byte *)(e) + 0x6dc))		/* x, y */
#define Vehicle_AimTarget(e) (*(cEntity **)((byte *)(e) + 0x6ec))
#define Vehicle_AimF6f4(e) (*(float *)((byte *)(e) + 0x6f4))
#define Vehicle_AimRange(e) (*(float *)((byte *)(e) + 0x6f8))
#define Vehicle_AimMode(e) (*(int *)((byte *)(e) + 0x718))		/* 1 Devastator, 4 ATPC, 0 off */
#define Vehicle_Effects(e) ((void *)((byte *)(e) + 0x7f0))		/* Entities/Vehicle/VehicleEffects.cpp */
#define Missile_OnHit(e) ((cPy_Function *)((byte *)(e) + 0x7d4))

/* the entity vtable slots of the weapons that entity.h and world.h do not name */
#define ENT_VF(e, off, type) ((type)(((void *const *)(e)->vftable)[(off) / 4]))
static inline vec3_t *Entity_GetAng(cEntity *e, vec3_t *out) {
	return ENT_VF(e, 0x80, vec3_t * (__thiscall *)(cEntity *, vec3_t *))(e, out);
}
static inline void Entity_SetVel(cEntity *e, const vec3_t *v) {
	ENT_VF(e, 0x8c, void(__thiscall *)(cEntity *, const vec3_t *))(e, v);
}
static inline byte Vehicle_IsEMIed(cEntity *e) {
	return ENT_VF(e, 0xdc, byte(__thiscall *)(cEntity *))(e);
}

/* the controls of a vehicle (+0x414): the selected weapon first, the fire buttons of this and the last frame */
#define Controls_Fire(c) ((c)[0x22])
#define Controls_FirePrev(c) ((c)[0x20])
#define Controls_CM(c) ((c)[0x23])			/* the ammo button (countermeasures) */
#define Controls_CMPrev(c) ((c)[0x21])
int __thiscall Controls_GetWeapon(byte *self);		/* rva 0xffb90 */
/* clears both fire buttons and +0x18 when the weapon changes (rva 0xffc40) */
void __thiscall Controls_SetWeapon(byte *self, int w);
/* the two force feedback motors: strength and fade time (rva 0xffc60, 0xffc90) */
void __thiscall Controls_RumbleA(byte *self, float strength, float time);
void __thiscall Controls_RumbleB(byte *self, float strength, float time);

/* the aim help of a vehicle (+0x6d0): the angle (its cosine at +4), the range */
void __thiscall cAutoAim_SetAngle(void *self, float ang);	/* rva 0xfe780 */
void __thiscall cAutoAim_SetRange(void *self, float range);	/* rva 0xfe7b0 */
void __thiscall cAutoAim_Update(void *self, cEntity *owner, float dt);	/* rva 0xfe1f0 */
/* the shots of the vehicle (+0x298): on the entity of that name or a point, n more; the attacker of the vehicle
 * by name; every frame after the weapons (rva 0xf9c10, 0xf9d00, 0xf9b60, 0xf9d90) */
void __thiscall cVehicleAttack_ShotAt(void *self, const char *name, int n);
void __thiscall cVehicleAttack_ShotAtPoint(void *self, const vec3_t *p, int n);
void __thiscall cVehicleAttack_HitBy(void *self, const char *name);
void __thiscall cVehicleAttack_Update(void *self);
/* the debris cloud on a vehicle for time (rva 0x10a150) */
void __thiscall cVehicleEntity_SetCloud(cEntity *self, float time, byte on);
/* the shot effect of the weapon slot (rva 0x103500) */
void __thiscall cVehicleEffects_Shoot(void *self, int weap, int slot);
/* World/FX/FXWeapons.cpp: the ATPC shot of the vehicle from origin to hit (rva 0x1fde50) */
void __thiscall cObjElementFXATPC_Shoot(cObjElementFXATPC *self, cEntity *v, const char *target, vec3_t hit,
	vec3_t origin);
/* a random float between lo and hi (rva 0x10e8f0) */
float Rand_FloatRange(float lo, float hi);
/* Weaps/Inferno.cpp: the shot of a net client (rva 0x120fa0) */
void Inferno_ClientFire(const char *name);

/* the AI of a vehicle takes the new weapon list (rva 0x8fd20) */
void __thiscall cAIVehicle_WeapListChanged(void *ai);

/* Sound/SndInterface.cpp */
void __thiscall cSndItem_UpdateAt(cSndItem *self, cEntity *e);
byte __thiscall cSndItem_Play(cSndItem *self, cEntity *e);
void __fastcall cSndItem_Restart(cSndItem *self);

/* the fields of the scorer items the weapons animate (the HUD of the weapon) */
#define ScorerItem_Angle(i) (*(float *)((byte *)(i) + 0xb4))
#define ScorerItem_SizeX(i) (*(float *)((byte *)(i) + 0xb8))
#define ScorerItem_SizeY(i) (*(float *)((byte *)(i) + 0xbc))
#define ScorerItem_Color(i) (*(uint32_t *)((byte *)(i) + 0xc0))
#define Scorer_Time(s) (*(float *)((byte *)(s) + 0x20))
#define Scorer_Delta(s) (*(float *)((byte *)(s) + 0x28))

/* the engine variables */
#define g_LaserIDWeap GAME_DATA(int, 0x4487d0)
#define g_ATPCIDWeap GAME_DATA(int, 0x4487e8)
#define g_DevastatorIDWeap GAME_DATA(int, 0x448830)
#define g_ATPCIDAmmo GAME_DATA(int, 0x448e4c)
#define g_DevastatorIDAmmo GAME_DATA(int, 0x449108)
#define g_ATPCDelay GAME_DATA(float, 0x448d34)
#define g_ATPCUpgradeDelay GAME_DATA(float, 0x448be4)
#define g_HookPlugFunc ((cPy_Function *)Game_Ptr(0x448848))
#define g_RestoreUpgradeFunc ((cPy_Function *)Game_Ptr(0x4487ac))
#define g_WeapListBuf ((char *)Game_Ptr(0x448890))		/* 0x200 bytes, GetWeapList */
#define g_WeaponNeedOrder ((const int (*)[6])Game_Ptr(0x428840))	/* three orders of six weapons */
#define g_VehicleCenterPadLock GAME_DATA(float, 0x447ba4)
#define g_CannonScorerColorRed GAME_DATA(uint32_t, 0x45d88c)
#define g_CannonScorerColorGreen GAME_DATA(uint32_t, 0x45d8bc)
#define g_CannonScorerColorBlue GAME_DATA(uint32_t, 0x45d8a4)

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
#define g_WeapRandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */
static inline double Weap_Rand01(void) {
	return (double)(crt_rand() & 0x7fff) * g_WeapRandScale;
}

/* the cannon color of the HUD items (alpha 0xbf) */
static inline uint32_t Weap_CannonColor(void) {
	return ((g_CannonScorerColorRed | 0xffffbf00u) << 8 | g_CannonScorerColorGreen) << 8 | g_CannonScorerColorBlue;
}

/* WeaponManager.cpp */
int __thiscall cWeaponManager_HasMinAmmo(cWeaponManager *self, uint32_t w);
byte __thiscall cWeaponManager_VarF(cWeaponManager *self, const char *name, double *v, byte set);
int cWeaponManager_FixWeapon(int w);
int __thiscall cWeaponManager_NumAmmoSlots(cWeaponManager *self, int ammo);
int __thiscall cWeaponManager_NumSlots(cWeaponManager *self, int w, byte warn);
float __thiscall cWeaponManager_GetWeapAmmo(cWeaponManager *self, uint32_t w);
uint32_t __thiscall cWeaponManager_GetSlots(cWeaponManager *self, int w);
char *__thiscall cWeaponManager_GetWeapList(cWeaponManager *self);
int __thiscall cWeaponManager_GetUpgrade(cWeaponManager *self, int w);
float __thiscall cWeaponManager_GetUAmmo(cWeaponManager *self, int w);
void __thiscall cWeaponManager_Init(cWeaponManager *self, cEntity *owner);
byte __thiscall cWeaponManager_VarI(cWeaponManager *self, const char *name, int *v, byte set);
int __thiscall cWeaponManager_NextWeap(cWeaponManager *self, uint32_t w);
int __thiscall cWeaponManager_PrevWeap(cWeaponManager *self, uint32_t w);
float __thiscall cWeaponManager_SetAmmo(cWeaponManager *self, int ammo, float v);
void __thiscall cWeaponManager_LockEntity(cWeaponManager *self, const char *name);
void __thiscall cWeaponManager_LockPoint(cWeaponManager *self, const vec3_t *p);
void __thiscall cWeaponManager_SetFlag46(cWeaponManager *self, byte v);
uint32_t __thiscall cWeaponManager_SetSlots(cWeaponManager *self, int w, int mask);
void __thiscall cWeaponManager_SetWeapList(cWeaponManager *self, const char *list);
byte __thiscall cWeaponManager_SetSlot(cWeaponManager *self, int w, int slot, byte on);
byte __thiscall cWeaponManager_SetUpgrade(cWeaponManager *self, int w, int upgrade);
byte __thiscall cWeaponManager_SetUAmmo(cWeaponManager *self, int w, float v);
void __thiscall cWeaponManager_SetWeapSnd(cWeaponManager *self, int snd);
void __thiscall cWeaponManager_Update(cWeaponManager *self, float dt);
void __thiscall cWeaponManager_NextWeapSnd(cWeaponManager *self, int unused);
byte __thiscall cWeaponManager_VarS(cWeaponManager *self, const char *name, char **v, byte set);
byte __thiscall cWeaponManager_VarV(cWeaponManager *self, const char *name, double *x, double *y, double *z, byte set);
void __thiscall cWeaponManager_PlayStartSnd(cWeaponManager *self);
int __thiscall cWeaponManager_WeaponNeed(cWeaponManager *self);

/* Weaps/ATPC.cpp: the shot of the ATPC (a charge per mounted slot), its HUD item and the debris cloud */
#define g_ATPCShooter ((char *)Game_Ptr(0x448a90))		/* 0x80 bytes, the vehicle of the last shot */
#define g_ATPCVictim ((char *)Game_Ptr(0x448b10))		/* 0x80 bytes */
#define g_ATPCNumSlots GAME_DATA(float, 0x448b90)		/* the slots of the last shot */
#define g_ATPCAmmoCost GAME_DATA(float, 0x448c2c)
#define g_ATPCExtraDamage GAME_DATA(int, 0x448c44)
#define g_ATPCExplosionRadius GAME_DATA(float, 0x448c74)
#define g_ATPCMainDamage GAME_DATA(int, 0x448cd4)
#define g_ATPCHitForce GAME_DATA(float, 0x448d6c)
#define g_ATPCRange GAME_DATA(float, 0x448dac)
#define g_ATPCConvDist GAME_DATA(float, 0x448dd4)
#define g_ATPCCapsuleSoundAttEnd GAME_DATA(float, 0x448ba4)
#define g_ATPCCapsuleSoundAttIni GAME_DATA(float, 0x448bbc)
#define g_ATPCCapsuleSound GAME_DATA(const char *, 0x448bfc)
#define g_ATPCCapsuleSoundRadius GAME_DATA(float, 0x448c14)
#define g_ATPCTrailTime GAME_DATA(float, 0x448c5c)
#define g_ATPCCapsuleExtendedSound GAME_DATA(const char *, 0x448ca4)
#define g_ATPCHitSound GAME_DATA(const char *, 0x448cbc)
#define g_ATPCHitSoundAttEnd GAME_DATA(float, 0x448cec)
#define g_ATPCHitSoundAttIni GAME_DATA(float, 0x448d04)
#define g_ATPCHitSoundVol GAME_DATA(float, 0x448d1c)
#define g_ATPCCapsuleSoundVol GAME_DATA(float, 0x448d94)
#define g_ATPCShootFile GAME_DATA(const char *, 0x448dec)
#define g_ATPCCapsuleExtendedSoundAttIni GAME_DATA(float, 0x448e04)
#define g_ATPCCapsuleExtendedSoundAttEnd GAME_DATA(float, 0x448e1c)
#define g_ATPCCapsuleExtendedSoundVol GAME_DATA(float, 0x448e34)
#define g_CreateCloudFunc ((cPy_Function *)Game_Ptr(0x448e9c))
#define g_CloudLifeTime GAME_DATA(float, 0x448ec0)
#define g_CloudAmmoNeed GAME_DATA(int, 0x448ed8)
#define g_CloudIDAmmo GAME_DATA(int, 0x448ef0)
#define g_CloudRechargeTime GAME_DATA(float, 0x448f08)
#define g_SetUpCloudFunc ((cPy_Function *)Game_Ptr(0x448f20))

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
#define g_DevShooter ((char *)Game_Ptr(0x448f48))		/* 0x80 bytes */
#define g_DevVictim ((char *)Game_Ptr(0x448fc8))		/* 0x80 bytes */
#define g_DevastatorRechargeTime GAME_DATA(float, 0x449088)
#define g_DevastatorExplosionRadius GAME_DATA(float, 0x4490b0)
#define g_DevastatorExtraDamage GAME_DATA(float, 0x4490c8)
#define g_DevastatorHitForce GAME_DATA(float, 0x4490f0)
#define g_DevastatorMinDamage GAME_DATA(float, 0x449160)
#define g_DevastatorSpreadAngle GAME_DATA(float, 0x449178)
#define g_DevastatorRange GAME_DATA(float, 0x4491a8)
#define g_DevastatorDistLimit GAME_DATA(float, 0x449210)
#define g_DevCenter ((vec3_t *)Game_Ptr(0x449258))		/* the explosion */
#define g_DevastatorAmmoCost GAME_DATA(float, 0x44928c)
#define g_CreateEMIFunc ((cPy_Function *)Game_Ptr(0x4492bc))
#define g_EMIRechargeTime GAME_DATA(float, 0x4492f8)
#define g_EMIAmmoNeed GAME_DATA(int, 0x449328)
#define g_EMILowRadiusFactor GAME_DATA(float, 0x449340)
#define g_SetUpEMIFunc ((cPy_Function *)Game_Ptr(0x4493b0))
#define g_EMILifeTime GAME_DATA(float, 0x449444)
#define g_EMIIDAmmo GAME_DATA(int, 0x44945c)
/* WeaponManager.cpp before rva 0x11a0f0: the fire stamp delayed by t (to -t when it is beyond t) */
void __thiscall cWeaponManager_DelayFire(cWeaponManager *self, float t);

void Devastator_Start(cEntity *v);
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
	cSndItem *it = PtrArray_Get(Vehicle_Sounds(e), i);
	if (it) {
		it->volume = it->vols[0];
		it->pitch = it->pitches[0];
		it->playing = 1;
		cSndItem_UpdateAt(it, e);
		cSndItem_Restart(it);
	}
}

#endif
