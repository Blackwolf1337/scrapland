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

/* the vtable slots of the ships and missiles the weapons call */
typedef struct WeapEntityVtbl {
	void *vf00_04[5];
	byte(__thiscall *IsType)(cEntity *self, const char *type);				/* 0x14 */
	vec3_t *(__thiscall *GetPos)(cEntity *self, vec3_t *out);				/* 0x18 */
	void *vf07;
	void(__thiscall *SetActive)(cEntity *self, int on);					/* 0x20 */
	void *vf09_0b[3];
	void(__thiscall *SetModel)(cEntity *self, const char *file, float sx, float sy, float sz, int flag); /* 0x30 */
	void(__thiscall *GetTargetAngle)(cEntity *self, float *ang, float dist, cEntity **target);	/* 0x34 */
	void(__thiscall *AimWeapon)(cEntity *self, float ax, float ay, float *dist, vec3_t *out, cEntity **target,
		int flags);									/* 0x38 */
	void *vf0f_15[7];
	void *(__thiscall *GetAI)(cEntity *self);						/* 0x58 */
	void *vf17_1c[6];
	vec3_t *(__thiscall *GetCenter)(cEntity *self, vec3_t *out);				/* 0x74 */
	vec3_t *(__thiscall *GetAngles)(cEntity *self, vec3_t *out);				/* 0x78 */
	void *vf1f;
	vec3_t *(__thiscall *GetOrient)(cEntity *self, vec3_t *out);				/* 0x80 */
	void(__thiscall *SetOrient)(cEntity *self, const vec3_t *ang);			/* 0x84 */
	vec3_t *(__thiscall *GetVel)(cEntity *self, vec3_t *out);				/* 0x88 */
	void(__thiscall *SetVel)(cEntity *self, const vec3_t *vel);				/* 0x8c */
	void *vf24_25[2];
	CGameModel *(__thiscall *GetModel)(cEntity *self);					/* 0x98 */
	void *vf27_32[12];
	void(__thiscall *SetMaxSpeed)(cEntity *self, float v);				/* 0xcc */
	void *vf34_3e[11];
	void(__thiscall *KillBees)(cEntity *self, int n, int flag);				/* 0xfc missiles */
	void(__thiscall *AddBees)(cEntity *self, int n);					/* 0x100 */
	void(__thiscall *SetBees)(cEntity *self, int n);					/* 0x104 */
} WeapEntityVtbl;
SD_OFFSET(WeapEntityVtbl, SetActive, 0x20);
SD_OFFSET(WeapEntityVtbl, SetModel, 0x30);
SD_OFFSET(WeapEntityVtbl, GetAI, 0x58);
SD_OFFSET(WeapEntityVtbl, GetCenter, 0x74);
SD_OFFSET(WeapEntityVtbl, GetOrient, 0x80);
SD_OFFSET(WeapEntityVtbl, SetVel, 0x8c);
SD_OFFSET(WeapEntityVtbl, GetModel, 0x98);
SD_OFFSET(WeapEntityVtbl, SetMaxSpeed, 0xcc);
SD_OFFSET(WeapEntityVtbl, KillBees, 0xfc);
SD_OFFSET(WeapEntityVtbl, SetBees, 0x104);

static inline const WeapEntityVtbl *Weap_Vtbl(cEntity *e) {
	return (const WeapEntityVtbl *)e->vftable;
}

#define WFIELD(e, type, off) (*(type *)((byte *)(e) + (off)))

/* the fields of the entities with life (ships, characters, missiles) */
#define Ent_Active(e) WFIELD(e, byte, 0x4c)
#define Ent_Cam(e) WFIELD(e, cEntity *, 0xf0)		/* the camera entity of a player character */
#define Ent_Mass(e) WFIELD(e, float, 0x230)
#define Ent_Life(e) WFIELD(e, int, 0x238)
#define Ent_God(e) WFIELD(e, byte, 0x23c)		/* the life does not change */
#define Ent_OnDeath(e) ((cPy_Function *)((byte *)(e) + 0x264))
#define Ent_OnDamage(e) ((cPy_Function *)((byte *)(e) + 0x278))
#define Ent_BurnTime(e) WFIELD(e, float, 0x28c)		/* the burn of the Inferno Napalm */
#define Ent_BurnRate(e) WFIELD(e, float, 0x290)
#define Ent_BurnLen(e) WFIELD(e, float, 0x294)
#define Ent_Radius(e) WFIELD(e, float, 0x3a0)
#define Ent_HitSounds(e) ((cSndInterface *)((byte *)(e) + 0x3a8))	/* the laser hit sounds of an entity with life */
#define Cam_ShakeTime(c) WFIELD(c, float, 0x220)
#define Cam_Shake(c) WFIELD(c, float, 0x224)

/* the fields of the ships */
#define Veh_HitInfo(v) ((void *)((byte *)(v) + 0x298))	/* the last entity or point the ship hit */
#define Veh_Kick(v) ((void *)((byte *)(v) + 0x414))	/* the recoil of the shots */
#define Veh_Speed(v) WFIELD(v, float, 0x46c)
#define Veh_PushTime(v) WFIELD(v, float, 0x470)	/* the push of the Tesla upgrade 5 against the flight */
#define Veh_Push(v) WFIELD(v, vec3_t, 0x474)
#define Veh_AI(v) WFIELD(v, byte *, 0x4fc)		/* its int at +0x64 is set to 2 by a hit */
#define Veh_EMITime(v) WFIELD(v, float, 0x58c)	/* the electromagnetic interference of the Hook */
#define Veh_Weapons(v) ((cTargetLock *)((byte *)(v) + 0x5f0))
#define Veh_CurWeapon(v) WFIELD(v, int, 0x5fc)
#define Veh_FireTime(v) WFIELD(v, float, 0x600)	/* +0x10 of the weapon block: negative while it reloads */
#define Veh_FireTime2(v) WFIELD(v, float, 0x604)	/* +0x14, the Hook and the Sonic */
#define Veh_WeapNodes(v) WFIELD(v, uint32_t *, 0x60c)	/* render node per ammo kind and slot: [ammo * 4 + slot] */
#define Veh_Player(v) WFIELD(v, int, 0x410)		/* 0..3 for the ship of a local player */
#define Veh_Aim(v) ((void *)((byte *)(v) + 0x6d0))
#define Veh_AimLimit(v) WFIELD(v, float, 0x6d8)	/* +0x8 of the aim, "AngLimit" */
#define Veh_AimAng(v) WFIELD(v, float, 0x6dc)		/* the aim angles x, y */
#define Veh_AimAngY(v) WFIELD(v, float, 0x6e0)
#define Veh_TargetLink(v) ((Nto1Node *)((byte *)(v) + 0x6e4))	/* the target entity of the ship */
#define Veh_AimCone(v) WFIELD(v, float, 0x6f4)	/* +0x24 of the aim */
#define Veh_AimDist(v) WFIELD(v, float, 0x6f8)	/* +0x28 of the aim */
#define Veh_AimMode(v) WFIELD(v, int, 0x718)
#define Veh_Effects(v) ((void *)((byte *)(v) + 0x7f0))	/* VehicleEffects.cpp: the shoot effects */
#define Veh_TeslaAng(v) WFIELD(v, float, 0x6fc)
#define Veh_TeslaMinAng(v) WFIELD(v, float, 0x700)
#define Veh_TeslaAngSpeed(v) WFIELD(v, float, 0x704)
#define Veh_Seat(v) WFIELD(v, int, 0x614)

/* the fields of the missiles (cMissileEntity) */
#define Mis_Sounds(m) ((cSndInterface *)((byte *)(m) + 0x144))
#define Mis_Size(m) ((void *)((byte *)(m) + 0x39c))	/* Primitive.cpp: the collision size */
#define Mis_Speed(m) WFIELD(m, float, 0x3f4)
#define Mis_Owner(m) ((char *)((byte *)(m) + 0x3fc))	/* the name of the ship that shot it */
#define Mis_Bees(m) WFIELD(m, int, 0x588)
#define Mis_Debris(m) WFIELD(m, cDebris *, 0x590)	/* the effect debris of the bees, linked at +0x50 */
#define Mis_Radius(m) WFIELD(m, float, 0x5a8)
#define Mis_Type(m) WFIELD(m, int, 0x7b8)		/* Inferno: 0 Uranio, 1 Hidrogeno, 2 Napalm; Swarm upgrade */
#define Mis_B7bd(m) WFIELD(m, byte, 0x7bd)
#define Mis_OnBeeHit(m) ((cPy_Function *)((byte *)(m) + 0x7c0))
#define Mis_OnHit(m) ((cPy_Function *)((byte *)(m) + 0x7d4))

/* the fields of a scorer item the weapons draw */
#define Item_Rot(i) WFIELD(i, float, 0xb4)
#define Item_SizeX(i) WFIELD(i, float, 0xb8)
#define Item_SizeY(i) WFIELD(i, float, 0xbc)
#define Item_Color(i) WFIELD(i, uint32_t, 0xc0)

/* engine variables (Gvars, the value at +0x10 of the variable object) */
#define g_SwarmIDWeap GAME_GLOBAL_IN(int, GVar_SwarmIDWeap, 16, 0x448800)
#define g_InfernoIDWeap GAME_GLOBAL_IN(int, GVar_InfernoIDWeap, 16, 0x44886c)
#define g_InfernoEngineSoundVol GAME_GLOBAL_IN(float, GVar_InfernoEngineSoundVol, 16, 0x449740)
#define g_InfernoRangeMin GAME_GLOBAL_IN(float, GVar_InfernoRangeMin, 16, 0x449768)
#define g_InfernoTimeDelay GAME_GLOBAL_IN(float, GVar_InfernoTimeDelay, 16, 0x449780)
#define g_InfernoAmmoCost GAME_GLOBAL_IN(int, GVar_InfernoAmmoCost, 16, 0x449798)
#define g_InfernoUpTopSpeed GAME_GLOBAL_IN(float, GVar_InfernoUpTopSpeed, 16, 0x4497d8)
#define g_InfernoIDAmmo GAME_GLOBAL_IN(int, GVar_InfernoIDAmmo, 16, 0x4497f0)
#define g_InfernoLifeTime GAME_GLOBAL_IN(float, GVar_InfernoLifeTime, 16, 0x449808)
#define g_InfernoEngineSoundAttIni GAME_GLOBAL_IN(float, GVar_InfernoEngineSoundAttIni, 16, 0x449820)
#define g_InfernoTurnSpeed GAME_GLOBAL_IN(float, GVar_InfernoTurnSpeed, 16, 0x449838)
#define g_InfernoMaxDamage GAME_GLOBAL_IN(float, GVar_InfernoMaxDamage, 16, 0x449868)
#define g_InfernoHitSoundVol GAME_GLOBAL_IN(float, GVar_InfernoHitSoundVol, 16, 0x4498c0)
#define g_InfernoHitSoundAttEndRadius GAME_GLOBAL_IN(float, GVar_InfernoHitSoundAttEndRadius, 16, 0x4498e8)
#define g_InfernoEngineSoundDoppler GAME_GLOBAL_IN(float, GVar_InfernoEngineSoundDoppler, 16, 0x449928)
#define g_InfernoBeesSpeed GAME_GLOBAL_IN(float, GVar_InfernoBeesSpeed, 16, 0x449940)
#define g_InfernoUpgFireFactor GAME_GLOBAL_IN(float, GVar_InfernoUpgFireFactor, 16, 0x449958)
#define g_InfernoInvUpgExtraDamage GAME_GLOBAL_IN(float, GVar_InfernoInvUpgExtraDamage, 16, 0x449970)
#define g_InfernoHitForce GAME_GLOBAL_IN(float, GVar_InfernoHitForce, 16, 0x4499a0)
#define g_InfernoRangeBay GAME_GLOBAL_IN(float, GVar_InfernoRangeBay, 16, 0x4499c8)
#define g_InfernoArmor GAME_GLOBAL_IN(int, GVar_InfernoArmor, 16, 0x4499f0)
#define g_InfernoUpgFireTime GAME_GLOBAL_IN(float, GVar_InfernoUpgFireTime, 16, 0x449a30)
#define g_InfernoHitSound GAME_GLOBAL_IN(const char *, GVar_InfernoHitSound, 16, 0x449a48)
#define g_InfernoMinDamage GAME_GLOBAL_IN(float, GVar_InfernoMinDamage, 16, 0x449a78)
#define g_InfernoEngineSound GAME_GLOBAL_IN(const char *, GVar_InfernoEngineSound, 16, 0x449ac8)
#define g_InfernoNormSpeed GAME_GLOBAL_IN(float, GVar_InfernoNormSpeed, 16, 0x449ae0)
#define g_InfernoXplosionCall GAME_GLOBAL_IN(nativefunc_t, NativeFunc_InfernoXplosionCall, 4, 0x449aec)	/* the function of the native "InfernoXplosionCall" */
#define g_InfernoMissileFile GAME_GLOBAL_IN(const char *, GVar_InfernoMissileFile, 16, 0x449b08)
#define g_FXInfernoExplosionMaxDist GAME_GLOBAL_IN(float, GVar_FXInfernoExplosionMaxDist, 16, 0x449b20)
#define g_InfernoDistLimit GAME_GLOBAL_IN(float, GVar_InfernoDistLimit, 16, 0x449b78)
#define g_InfernoHitSoundAttIniRadius GAME_GLOBAL_IN(float, GVar_InfernoHitSoundAttIniRadius, 16, 0x449ba8)
#define g_InfernoEngineSoundAttEnd GAME_GLOBAL_IN(float, GVar_InfernoEngineSoundAttEnd, 16, 0x449bcc)
#define g_InfernoExplosionFile GAME_GLOBAL_IN(const char *, GVar_InfernoExplosionFile, 16, 0x449be4)
#define g_LaserAmmoCost GAME_GLOBAL_IN(float, GVar_LaserAmmoCost, 16, 0x449bfc)
#define g_LaserRange GAME_GLOBAL_IN(float, GVar_LaserRange, 16, 0x449c44)
#define g_LaserShootFile GAME_GLOBAL_IN(const char *, GVar_LaserShootFile, 16, 0x449c84)
#define g_LaserHitForce GAME_GLOBAL_IN(float, GVar_LaserHitForce, 16, 0x449c9c)
#define g_LaserHitFile GAME_GLOBAL_IN(const char *, GVar_LaserHitFile, 16, 0x449cb4)
#define g_HookEMITime GAME_GLOBAL_IN(float, GVar_HookEMITime, 16, 0x449cdc)
#define g_LaserTimeDelay GAME_GLOBAL_IN(float, GVar_LaserTimeDelay, 16, 0x449cf4)
#define g_LaserRicSoundVol GAME_GLOBAL_IN(float, GVar_LaserRicSoundVol, 16, 0x449d4c)
#define g_LaserHookFile GAME_GLOBAL_IN(const char *, GVar_LaserHookFile, 16, 0x449d7c)
#define g_LaserDamage GAME_GLOBAL_IN(int, GVar_LaserDamage, 16, 0x449d94)
#define g_HookAccel GAME_GLOBAL_IN(float, GVar_HookAccel, 16, 0x449dd4)
#define g_LaserDistLimit GAME_GLOBAL_IN(float, GVar_LaserDistLimit, 16, 0x449dec)
#define g_LaserMinSpeed GAME_GLOBAL_IN(float, GVar_LaserMinSpeed, 16, 0x449e04)
#define g_LaserRicSound GAME_GLOBAL_IN(const char *, GVar_LaserRicSound, 16, 0x449e84)
#define g_SonicRechargeTime GAME_GLOBAL_IN(float, GVar_SonicRechargeTime, 16, 0x449ec4)
#define g_SonicArmor GAME_GLOBAL_IN(int, GVar_SonicArmor, 16, 0x449edc)
#define g_FXSonicXpldeFile GAME_GLOBAL_IN(const char *, GVar_FXSonicXpldeFile, 16, 0x449ef4)
#define g_SonicLifeTime GAME_GLOBAL_IN(float, GVar_SonicLifeTime, 16, 0x449f0c)
#define g_SonicIDAmmo GAME_GLOBAL_IN(int, GVar_SonicIDAmmo, 16, 0x449f24)
#define g_SonicAmmoNeed GAME_GLOBAL_IN(int, GVar_SonicAmmoNeed, 16, 0x449f4c)
#define g_CreateSonicFunc (GAME_ARRAY_IN(cPy_Function, GVar_CreateSonicFunc, 16, 0x449f64))
#define g_SetUpSonicFunc (GAME_ARRAY_IN(cPy_Function, GVar_SetUpSonicFunc, 16, 0x449f88))
#define g_SwarmHitSoundVol GAME_GLOBAL_IN(float, GVar_SwarmHitSoundVol, 16, 0x44a1c8)
#define g_SwarmMissileFile GAME_GLOBAL_IN(const char *, GVar_SwarmMissileFile, 16, 0x44a208)
#define g_SwarmUpTopSpeed GAME_GLOBAL_IN(float, GVar_SwarmUpTopSpeed, 16, 0x44a258)
#define g_SwarmEngineSoundAttIni GAME_GLOBAL_IN(float, GVar_SwarmEngineSoundAttIni, 16, 0x44a270)
#define g_SwarmBeeSoundAttEnd GAME_GLOBAL_IN(float, GVar_SwarmBeeSoundAttEnd, 16, 0x44a288)
#define g_SwarmDeathTime GAME_GLOBAL_IN(float, GVar_SwarmDeathTime, 16, 0x44a2a0)
#define g_SwarmBeeRadius GAME_GLOBAL_IN(float, GVar_SwarmBeeRadius, 16, 0x44a2d0)
#define g_SwarmMinRadius GAME_GLOBAL_IN(float, GVar_SwarmMinRadius, 16, 0x44a2e8)
#define g_SwarmBeesSpeed GAME_GLOBAL_IN(float, GVar_SwarmBeesSpeed, 16, 0x44a318)
#define g_SwarmHitSound GAME_GLOBAL_IN(const char *, GVar_SwarmHitSound, 16, 0x44a330)
#define g_SwarmHitSoundAttIni GAME_GLOBAL_IN(float, GVar_SwarmHitSoundAttIni, 16, 0x44a370)
#define g_SwarmNormSpeed GAME_GLOBAL_IN(float, GVar_SwarmNormSpeed, 16, 0x44a388)
#define g_SwarmLifeTime GAME_GLOBAL_IN(float, GVar_SwarmLifeTime, 16, 0x44a400)
#define g_SwarmHitForce GAME_GLOBAL_IN(float, GVar_SwarmHitForce, 16, 0x44a418)
#define g_SwarmTurnSpeedUPG GAME_GLOBAL_IN(float, GVar_SwarmTurnSpeedUPG, 16, 0x44a430)
#define g_SwarmFastSpeed GAME_GLOBAL_IN(float, GVar_SwarmFastSpeed, 16, 0x44a448)
#define g_SwarmTurnSpeed GAME_GLOBAL_IN(float, GVar_SwarmTurnSpeed, 16, 0x44a460)
#define g_SwarmArmor GAME_GLOBAL_IN(int, GVar_SwarmArmor, 16, 0x44a490)
#define g_SwarmEngineSound GAME_GLOBAL_IN(const char *, GVar_SwarmEngineSound, 16, 0x44a4c0)
#define g_SwarmBeeSoundVol GAME_GLOBAL_IN(float, GVar_SwarmBeeSoundVol, 16, 0x44a4d8)
#define g_SwarmIDAmmo GAME_GLOBAL_IN(int, GVar_SwarmIDAmmo, 16, 0x44a4f0)
#define g_SwarmEngineSoundVol GAME_GLOBAL_IN(float, GVar_SwarmEngineSoundVol, 16, 0x44a508)
#define g_SwarmTimeDelay GAME_GLOBAL_IN(float, GVar_SwarmTimeDelay, 16, 0x44a550)
#define g_SwarmBeeSoundAttIni GAME_GLOBAL_IN(float, GVar_SwarmBeeSoundAttIni, 16, 0x44a568)
#define g_SwarmHitSoundAttEnd GAME_GLOBAL_IN(float, GVar_SwarmHitSoundAttEnd, 16, 0x44a590)
#define g_SwarmBeeSound GAME_GLOBAL_IN(const char *, GVar_SwarmBeeSound, 16, 0x44a5a8)
#define g_SwarmNumber1 GAME_GLOBAL_IN(int, GVar_SwarmNumber1, 16, 0x44a5f8)
#define g_SwarmEngineSoundAttEnd GAME_GLOBAL_IN(float, GVar_SwarmEngineSoundAttEnd, 16, 0x44a638)
#define g_SwarmEngineSoundDoppler GAME_GLOBAL_IN(float, GVar_SwarmEngineSoundDoppler, 16, 0x44a650)
#define g_SwarmAmmoCost1 GAME_GLOBAL_IN(int, GVar_SwarmAmmoCost1, 16, 0x44a3a0)
#define g_SwarmAmmoCost2 GAME_GLOBAL_IN(int, GVar_SwarmAmmoCost2, 16, 0x44a300)
#define g_SwarmAmmoCost3 GAME_GLOBAL_IN(int, GVar_SwarmAmmoCost3, 16, 0x44a478)
#define g_SwarmAmmoCost4 GAME_GLOBAL_IN(int, GVar_SwarmAmmoCost4, 16, 0x44a4a8)
#define g_SwarmAmmoCosts (GAME_ARRAY(int, g_SwarmAmmoCosts, 0x44a658))	/* [6] SwarmAmmoCost1..4 at the start, then 0 0 */
#define g_TeslaDownRangeTimePad GAME_GLOBAL_IN(float, GVar_TeslaDownRangeTimePad, 16, 0x44a680)
#define g_TeslaMinAngRangePad GAME_GLOBAL_IN(float, GVar_TeslaMinAngRangePad, 16, 0x44a730)
#define g_TeslaDownRangeTime GAME_GLOBAL_IN(float, GVar_TeslaDownRangeTime, 16, 0x44a748)
#define g_TeslaMinAngRangeUpg GAME_GLOBAL_IN(float, GVar_TeslaMinAngRangeUpg, 16, 0x44a778)
#define g_TeslaMinAngRangeUpgPad GAME_GLOBAL_IN(float, GVar_TeslaMinAngRangeUpgPad, 16, 0x44a790)
#define g_TeslaMinAngRange GAME_GLOBAL_IN(float, GVar_TeslaMinAngRange, 16, 0x44a810)
#define g_TeslaTrackForce GAME_GLOBAL_IN(float, GVar_TeslaTrackForce, 16, 0x44a698)
#define g_TeslaShootFile GAME_GLOBAL_IN(const char *, GVar_TeslaShootFile, 16, 0x44a6c0)
#define g_TeslaMaxAngRangePad GAME_GLOBAL_IN(float, GVar_TeslaMaxAngRangePad, 16, 0x44a718)
#define g_TeslaUpRangeTime GAME_GLOBAL_IN(float, GVar_TeslaUpRangeTime, 16, 0x44a7b8)
#define g_TeslaRange GAME_GLOBAL_IN(float, GVar_TeslaRange, 16, 0x44a7d0)
#define g_TeslaHitForce GAME_GLOBAL_IN(float, GVar_TeslaHitForce, 16, 0x44a7f8)
#define g_TeslaAmmoCost GAME_GLOBAL_IN(float, GVar_TeslaAmmoCost, 16, 0x44a858)
#define g_TeslaDistLimit GAME_GLOBAL_IN(float, GVar_TeslaDistLimit, 16, 0x44a870)
#define g_TeslaMaxAngRange GAME_GLOBAL_IN(float, GVar_TeslaMaxAngRange, 16, 0x44a888)
#define g_VulcanIDWeap GAME_GLOBAL_IN(int, GVar_VulcanIDWeap, 16, 0x448818)
#define g_VulcanIDAmmo GAME_GLOBAL_IN(int, GVar_VulcanIDAmmo, 16, 0x44a8b0)
#define g_FXVulcanHitDistance GAME_GLOBAL_IN(float, GVar_FXVulcanHitDistance, 16, 0x44a8c8)
#define g_VulcanTimeDelay GAME_GLOBAL_IN(float, GVar_VulcanTimeDelay, 16, 0x44a900)
#define g_VulcanRic2Sound GAME_GLOBAL_IN(const char *, GVar_VulcanRic2Sound, 16, 0x44a918)
#define g_VulcanRicSoundAttIni GAME_GLOBAL_IN(float, GVar_VulcanRicSoundAttIni, 16, 0x44a930)
#define g_VulcanDistLimitUPG GAME_GLOBAL_IN(float, GVar_VulcanDistLimitUPG, 16, 0x44a948)
#define g_VulcanTimeDelayUPG GAME_GLOBAL_IN(float, GVar_VulcanTimeDelayUPG, 16, 0x44a960)
#define g_VulcanHitForce GAME_GLOBAL_IN(float, GVar_VulcanHitForce, 16, 0x44a978)
#define g_VulcanDamageEasy GAME_GLOBAL_IN(int, GVar_VulcanDamageEasy, 16, 0x44a9b0)
#define g_VulcanDistLimit GAME_GLOBAL_IN(float, GVar_VulcanDistLimit, 16, 0x44a9c8)
#define g_VulcanHitFile GAME_GLOBAL_IN(const char *, GVar_VulcanHitFile, 16, 0x44a9e0)
#define g_VulcanRic1Sound GAME_GLOBAL_IN(const char *, GVar_VulcanRic1Sound, 16, 0x44a9f8)
#define g_VulcanRic0Sound GAME_GLOBAL_IN(const char *, GVar_VulcanRic0Sound, 16, 0x44aa10)
#define g_VulcanAmmoCost GAME_GLOBAL_IN(float, GVar_VulcanAmmoCost, 16, 0x44aa28)
#define g_VulcanBulletFile GAME_GLOBAL_IN(const char *, GVar_VulcanBulletFile, 16, 0x44aa40)
#define g_VulcanDamage GAME_GLOBAL_IN(int, GVar_VulcanDamage, 16, 0x44aa68)
#define g_VulcanHitUseNormal GAME_GLOBAL_IN(int, GVar_VulcanHitUseNormal, 16, 0x44aa80)
#define g_VulcanRic0SoundVol GAME_GLOBAL_IN(float, GVar_VulcanRic0SoundVol, 16, 0x44aa98)
#define g_VulcanRic2SoundVol GAME_GLOBAL_IN(float, GVar_VulcanRic2SoundVol, 16, 0x44aab0)
#define g_VulcanRic1SoundVol GAME_GLOBAL_IN(float, GVar_VulcanRic1SoundVol, 16, 0x44aac8)
#define g_VulcanRange GAME_GLOBAL_IN(float, GVar_VulcanRange, 16, 0x44aae0)
#define g_VulcanRicSoundAttEnd GAME_GLOBAL_IN(float, GVar_VulcanRicSoundAttEnd, 16, 0x44aaf8)

/* the state of the module */
#define g_InfernoHitName (GAME_ARRAY(char, g_InfernoHitName, 0x449480))	/* 0x80, the entity of the sphere callback */
#define g_InfernoOwner GAME_GLOBAL(cEntity *, g_InfernoOwner, 0x449500)	/* the ship of the exploding missile, NULL if none */
#define g_InfernoRange GAME_GLOBAL(float, g_InfernoRange, 0x449504)
#define g_InfernoMissile GAME_GLOBAL(cEntity *, g_InfernoMissile, 0x449508)
#define g_InfernoHudTarget (GAME_ARRAY(char[0x80], g_InfernoHudTarget, 0x449510))	/* per player: the target shown */
#define g_InfernoHudTime (GAME_ARRAY(float, g_InfernoHudTime, 0x449710))	/* per player */
#define g_InfernoPos GAME_GLOBAL(vec3_t, g_InfernoPos, 0x449bb0)
#define g_SonicHitPos GAME_GLOBAL(vec3_t, g_SonicHitPos, 0x449f9c)
#define g_SwarmHudTarget (GAME_ARRAY(char[0x80], g_SwarmHudTarget, 0x449fa8))
#define g_SwarmHudTime (GAME_ARRAY(float, g_SwarmHudTime, 0x44a1a8))
#define g_InfernoSphereFunc (GAME_ARRAY_IN(cPy_Function, g_ChTypeSphereFunc, 0, 0x46679c))
#define g_SwarmSlotData (GAME_ARRAY(int *, g_SwarmSlotData, 0x428918))	/* per slot: a pointer to the int AddBees takes */

/* the exe functions of other modules */
void __thiscall cWithLifeEntity_SetBurner(cEntity *e, cEntity *attacker);
void __thiscall cTargetLock_SetHookDelay(cTargetLock *self, float t);
/* SndInterface.c */
byte __thiscall cSndItem_Play(cSndItem *self, cEntity *e);
void __thiscall cSndInterface_Resize(cSndInterface *self, int n);
void __thiscall cSndInterface_SetSound(cSndInterface *self, int i, const char *name);
/* Functions.c */
byte __thiscall cPy_Function_SetNative(cPy_Function *self, nativefunc_t func);
/* FXWeapons.cpp: the emitters of the explosion and the bee effects */
void __thiscall cObjElementFXInfernoExplosion_Explode(cObjElementFXInfernoExplosion *self, float x, float y, float z,
	float range, int bees, byte flag);
void FXSwarm_BeeDeath(float x, float y, float z, float vx, float vy, float vz, float size, float time, int n);
void FXSwarm_Hit(float x, float y, float z, float vx, float vy, float vz, float size, float time, float dx, float dy,
	float dz);

#define WEAP_NATIVE(rva) ((nativefunc_t)Game_Ptr(rva))	/* a native of the exe (its C version behind the hook) */
#define WEAP_PI 3.141592653589793			/* rva 0x3a3700 */
#define Ent_Sounds(e) ((cSndInterface *)((byte *)(e) + 0x144))

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
	cSndItem *s = PtrArray_Get(Ent_Sounds(e), i);
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
	Veh_AimMode(v) = mode;
	return v;
}

/* the debris drop (position, angles, velocity) at a weapon node of the ship */
static inline void Weap_DropAtNode(cEntity *v, uint32_t node, matrix_t *mtx, vec3_t *ang, vec3_t *vel) {
	const matrix_t *mm = CGameModel_NodoMatrix(Weap_Vtbl(v)->GetModel(v), mtx, node, 0, NULL, 0);
	float x = mm->m[12];
	float y = mm->m[13];
	float z = mm->m[14];
	Weap_Vtbl(v)->GetAngles(v, ang);
	Weap_Vtbl(v)->GetVel(v, vel);
	g_DebrisDrop[0].x = x;
	g_DebrisDrop[0].y = y;
	g_DebrisDrop[0].z = z;
	g_DebrisDrop[1] = *ang;
	g_DebrisDrop[2] = *vel;
}

/* the engine sound of a missile: one sound, looped */
static inline void Weap_EngineSound(cEntity *m, const char *file, float vol, float attIni, float attEnd, float doppler) {
	cSndInterface_Resize(Ent_Sounds(m), 1);
	cSndInterface_SetSound(Ent_Sounds(m), 0, file);
	cSndItem *s = PtrArray_Get(Ent_Sounds(m), 0);
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

	float mass = Ent_Mass(e);
	const vec3_t *pv = Weap_Vtbl(v)->GetPos(v, &a);
	const vec3_t *pe = Weap_Vtbl(e)->GetPos(e, &b);
	d.x = pe->x - pv->x;
	d.y = pe->y - pv->y;
	d.z = pe->z - pv->z;
	float k = (float)n * force / mass;
	const vec3_t *dir = Vec3_NormalizeTo(&nd, &d);
	float px = dir->x * k;
	float py = dir->y * k;
	float pz = dir->z * k;
	const vec3_t *cur = Weap_Vtbl(e)->GetVel(e, &ov);
	vel.y = cur->y + py;
	vel.z = cur->z + pz;
	vel.x = cur->x + px;
	Weap_Vtbl(e)->SetVel(e, &vel);
}

/* the HUD item of a weapon that does not fire turns back to the angle 0 by d */
static inline void Weap_HudSpinDown(cScorerItem *item, float d) {
	float a = NormAng(Item_Rot(item));
	Item_Rot(item) = a;
	float s = a + d;
	if (0.0f > s) {
		Item_Rot(item) = s;
		return;
	}
	float t = a - d;
	Item_Rot(item) = t > 0.0f ? t : 0.0f;
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
byte __fastcall Vehicle_IsLocalPlayer(cEntity *v);
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
