/*
 * World/FX/FXVehicle.cpp of the game: the effects of the vehicles, objects (cObjElementFX, see fxcountermeasures.h)
 * on the debris of the debris systems: the debris fired out of a ship (FiredDebris, SmokeDebris), the smoke of an
 * entity hit by fire (FiredEntity), the damage, fire and EMI effects at the motors of a ship, the death trail, the
 * spawn and the Sputnik effects and the accelerator rays (Accelerator, not started anywhere in 1.6.1).
 * The vtable: Delete, Update (debris, dt), IsAlive (debris, sys), Start (debris), Reset (debris).
 * The motors of a ship: the four objects at vehicle + 0x728, each with two exhaust nodes (+0x38, +0x3c, -1 none);
 * the effects keep the list of the ones that exist as (motor, side) pairs.
 */
#ifndef SCRAP_FXVEHICLE_H
#define SCRAP_FXVEHICLE_H

#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/fxweapons.h"

#define COBJELEMENTFXVEHICLEDAMAGE_VFTABLE 0x3cd980
#define COBJELEMENTFXVEHICLEFIRE_VFTABLE 0x3cd998
#define COBJELEMENTFXFIREDENTITY_VFTABLE 0x3cd9b0
#define COBJELEMENTFXVEHICLEEMI_VFTABLE 0x3cd9c8
#define COBJELEMENTFXFIREDDEBRIS_VFTABLE 0x3cd9e0
#define COBJELEMENTFXSMOKEDEBRIS_VFTABLE 0x3cd9f8
#define COBJELEMENTFXVEHICLEDEATHTRAIL_VFTABLE 0x3cda10
#define COBJELEMENTFXVEHICLESPUTNIK_VFTABLE 0x3cda28
#define COBJELEMENTFXVEHICLESPAWN_VFTABLE 0x3cda40
#define COBJELEMENTFXVEHICLEACCELERATOR_VFTABLE 0x3cda58

#define FXV_MOTORS 4
#define FXV_EXHAUSTS (FXV_MOTORS * 2)
#define FXVACCEL_TRAILS 16
#define FXVEMI_TRAILS 16
#define FXVSPAWN_TRAILS 5
#define FXVSPUTNIK_TRAILS 13
#define FXVSPUTNIK_RANDS 195

/* the vehicle fields the effects read */
#define FXVeh_Motor(e, i) (((byte **)((byte *)(e) + 0x728))[i])
#define FXVeh_MotorNode(m, side) (*(int *)((m) + 0x38 + (side) * 4))
#define FXVeh_EmbeddedFX(e) (*(void **)((byte *)(e) + 0x224))
#define FXVeh_BurnTime(e) (*(float *)((byte *)(e) + 0x294))
#define FXVeh_EMITime(e) (*(uint32_t *)((byte *)(e) + 0x58c))	/* copied as bits */
#define FXVeh_MaxLife(e) (*(int *)((byte *)(e) + 0x640))		/* of a "Car" */
#define FXVeh_Effects(e) ((void *)((byte *)(e) + 0x7f0))		/* Entities/Vehicle/VehicleEffects.cpp */

/* the engine variables */
#define g_StdMaxLife GAME_DATA(int, 0x44ab34)			/* StdMaxLife */
#define g_FXSetOnFireLifePercent GAME_DATA(int, 0x447d6c)	/* FXSetOnFireLifePercent */

/* VehicleEffects.cpp (rva 0x101d30): the damage effect of the vehicle stopped (flag +4 and element +8 cleared) */
void __fastcall cVehicleEffects_ClearDamage(void *self);

/* a motor exhaust of the ship */
typedef struct FXVExhaust {
	int motor;			/* 0x0 FXVeh_Motor */
	int side;			/* 0x4 0 or 1 */
} FXVExhaust;
SD_SIZE(FXVExhaust, 8);

/* the debris fired out of a ship and its smoke: particles emitted along the way of the debris (0xdc bytes) */
typedef struct cObjElementFXFiredDebris {
	cObjElementFX base;
	CGameParticles particles;	/* 0x04 on Dummy01 */
	float time;			/* 0xb4 since the last emission */
	float interval;			/* 0xb8 */
	byte first;			/* 0xbc the next Update takes the position as the last one */
	byte pad_bd[3];
	vec3_t last;			/* 0xc0 the position of the last emission */
	float minDist;			/* 0xcc between two emissions */
	float scale;			/* 0xd0 */
	float spread;			/* 0xd4 */
	byte on;			/* 0xd8 */
	byte pad_d9[3];
} cObjElementFXFiredDebris;
SD_OFFSET(cObjElementFXFiredDebris, time, 0xb4);
SD_OFFSET(cObjElementFXFiredDebris, last, 0xc0);
SD_OFFSET(cObjElementFXFiredDebris, on, 0xd8);
SD_SIZE(cObjElementFXFiredDebris, 0xdc);

typedef cObjElementFXFiredDebris cObjElementFXSmokeDebris;

/* the smoke of an entity on fire (0xd4 bytes) */
typedef struct cObjElementFXFiredEntity {
	cObjElementFX base;
	cEntity *entity;		/* 0x04 */
	byte b08;			/* 0x08 */
	byte pad_09[3];
	float radius;			/* 0x0c of its model */
	vec3_t pos;			/* 0x10 of its model */
	CGameParticles particles;	/* 0x1c on Dummy01 */
	float time;			/* 0xcc */
	byte on;			/* 0xd0 */
	byte pad_d1[3];
} cObjElementFXFiredEntity;
SD_OFFSET(cObjElementFXFiredEntity, particles, 0x1c);
SD_OFFSET(cObjElementFXFiredEntity, on, 0xd0);
SD_SIZE(cObjElementFXFiredEntity, 0xd4);

/* the rays of the accelerator (0x4c0 bytes) */
typedef struct cObjElementFXVehicleAccelerator {
	cObjElementFX base;
	cEntity *owner;			/* 0x004 */
	void *accelerator;		/* 0x008 */
	CGameParticles particles;	/* 0x00c on Dummy01 */
	float fade[FXVACCEL_TRAILS];	/* 0x0bc */
	float delay[FXVACCEL_TRAILS];	/* 0x0fc */
	CGameTrail trails[FXVACCEL_TRAILS];	/* 0x13c */
	byte pad_17c[0x4bc - 0x17c];
	byte on;			/* 0x4bc */
	byte pad_4bd[3];
} cObjElementFXVehicleAccelerator;
SD_OFFSET(cObjElementFXVehicleAccelerator, delay, 0xfc);
SD_OFFSET(cObjElementFXVehicleAccelerator, trails, 0x13c);
SD_SIZE(cObjElementFXVehicleAccelerator, 0x4c0);

/* the damage of a ship: smoke and sparks at the motors while the life is low (0x2d0 bytes) */
typedef struct cObjElementFXVehicleDamage {
	cObjElementFX base;
	cEntity *entity;		/* 0x004 */
	byte active;			/* 0x008 the life is checked */
	byte pad_09[3];
	int life;			/* 0x00c */
	int lifePct;			/* 0x010 */
	CGameModel *model;		/* 0x014 the model of the debris */
	byte b18;			/* 0x018 */
	byte pad_19[3];
	CGameParticles parts3;		/* 0x01c on Dummy03 */
	CGameParticles parts2;		/* 0x0cc on Dummy02 */
	int f17c;			/* 0x17c */
	int f180;			/* 0x180 */
	CGameParticles parts1;		/* 0x184 on Dummy01 */
	vec3_t pos;			/* 0x234 of the ship model */
	byte b240;			/* 0x240 */
	byte pad_241[3];
	int f244;			/* 0x244 */
	int numExhausts;		/* 0x248 */
	FXVExhaust exhausts[FXV_EXHAUSTS];	/* 0x24c */
	byte pad_28c[0x2cc - 0x28c];
	byte on;			/* 0x2cc */
	byte pad_2cd[3];
} cObjElementFXVehicleDamage;
SD_OFFSET(cObjElementFXVehicleDamage, parts3, 0x1c);
SD_OFFSET(cObjElementFXVehicleDamage, parts2, 0xcc);
SD_OFFSET(cObjElementFXVehicleDamage, parts1, 0x184);
SD_OFFSET(cObjElementFXVehicleDamage, pos, 0x234);
SD_OFFSET(cObjElementFXVehicleDamage, exhausts, 0x24c);
SD_OFFSET(cObjElementFXVehicleDamage, on, 0x2cc);
SD_SIZE(cObjElementFXVehicleDamage, 0x2d0);

/* the electromagnetic interference of a ship: trails along the motors (0x82e4 bytes) */
typedef struct cObjElementFXVehicleEMI {
	cObjElementFX base;
	cEntity *entity;		/* 0x0004 */
	CGameParticles particles;	/* 0x0008 on Dummy01 */
	int fb8;			/* 0x00b8 */
	int fbc;			/* 0x00bc */
	int numTrails;			/* 0x00c0 16 */
	float fc4;			/* 0x00c4 22 */
	int fc8;			/* 0x00c8 0x1000, 16 per entry of marks */
	int fcc;			/* 0x00cc 0x40 */
	byte pad_d0[0xdc - 0xd0];
	int numExhausts;		/* 0x00dc */
	FXVExhaust exhausts[FXV_EXHAUSTS];	/* 0x00e0 */
	byte pad_120[0x160 - 0x120];
	int f160[FXVEMI_TRAILS];	/* 0x0160 -1 */
	int f1a0[FXVEMI_TRAILS];	/* 0x01a0 */
	byte pad_1e0[0x41e0 - 0x1e0];
	int f41e0[FXVEMI_TRAILS];	/* 0x41e0 */
	int f4220[FXVEMI_TRAILS];	/* 0x4220 */
	int f4260[FXVEMI_TRAILS];	/* 0x4260 */
	uint16_t marks[FXVEMI_TRAILS][0x200];	/* 0x42a0 */
	CGameTrail trails[FXVEMI_TRAILS];	/* 0x82a0 */
	byte on;			/* 0x82e0 */
	byte pad_82e1[3];
} cObjElementFXVehicleEMI;
SD_OFFSET(cObjElementFXVehicleEMI, numExhausts, 0xdc);
SD_OFFSET(cObjElementFXVehicleEMI, f160, 0x160);
SD_OFFSET(cObjElementFXVehicleEMI, f41e0, 0x41e0);
SD_OFFSET(cObjElementFXVehicleEMI, marks, 0x42a0);
SD_OFFSET(cObjElementFXVehicleEMI, trails, 0x82a0);
SD_SIZE(cObjElementFXVehicleEMI, 0x82e4);

/* the fire at the motors of a ship (0x1fc bytes) */
typedef struct cObjElementFXVehicleFire {
	cObjElementFX base;
	cEntity *entity;		/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	CGameParticles particlesOut;	/* 0x0b8 on Dummy02 */
	int f168;			/* 0x168 */
	int f16c;			/* 0x16c */
	byte pad_170[4];
	int numExhausts;		/* 0x174 */
	FXVExhaust exhausts[FXV_EXHAUSTS];	/* 0x178 */
	byte pad_1b8[0x1f8 - 0x1b8];
	byte on;			/* 0x1f8 */
	byte pad_1f9[3];
} cObjElementFXVehicleFire;
SD_OFFSET(cObjElementFXVehicleFire, particlesOut, 0xb8);
SD_OFFSET(cObjElementFXVehicleFire, numExhausts, 0x174);
SD_OFFSET(cObjElementFXVehicleFire, on, 0x1f8);
SD_SIZE(cObjElementFXVehicleFire, 0x1fc);

/* the spawn of a ship: rays from vertices of its model, drawn as trails (0xdd8 bytes) */
#define FXVSPAWN_RAYS 75
typedef struct cObjElementFXVehicleSpawn {
	cObjElementFX base;
	cEntity *entity;		/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	float stepTime;			/* 0x0b8 of the frame */
	float time;			/* 0x0bc */
	byte pad_c0[0xd8 - 0xc0];
	vec3_t rayBase[FXVSPAWN_RAYS];	/* 0x0d8 */
	vec3_t rayDir[FXVSPAWN_RAYS];	/* 0x45c */
	float rayLen[FXVSPAWN_RAYS];	/* 0x7e0 */
	float raySpeed[FXVSPAWN_RAYS];	/* 0x90c */
	float rayAcc[FXVSPAWN_RAYS];	/* 0xa38 */
	float rayMax[FXVSPAWN_RAYS];	/* 0xb64 */
	CGameTrail trails[FXVSPAWN_TRAILS];	/* 0xc90 */
	int rayVertex[FXVSPAWN_RAYS];	/* 0xca4 the vertex of the model */
	int numRays;			/* 0xdd0 */
	byte on;			/* 0xdd4 */
	byte pad_dd5[3];
} cObjElementFXVehicleSpawn;
SD_OFFSET(cObjElementFXVehicleSpawn, rayDir, 0x45c);
SD_OFFSET(cObjElementFXVehicleSpawn, rayMax, 0xb64);
SD_OFFSET(cObjElementFXVehicleSpawn, trails, 0xc90);
SD_OFFSET(cObjElementFXVehicleSpawn, numRays, 0xdd0);
SD_SIZE(cObjElementFXVehicleSpawn, 0xdd8);

/* the Sputnik of a ship (0x1720 bytes) */
typedef struct cObjElementFXVehicleSputnik {
	cObjElementFX base;
	cEntity *entity;		/* 0x0004 */
	CGameParticles particles;	/* 0x0008 on Dummy01 */
	int fb8;			/* 0x00b8 */
	int fbc;			/* 0x00bc */
	int fc0;			/* 0x00c0 1 */
	float fc4;			/* 0x00c4 0.8 */
	byte active;			/* 0x00c8 the flag_4c of the entity at the start, a change ends the effect */
	byte pad_c9[3];
	int numExhausts;		/* 0x00cc */
	FXVExhaust exhausts[FXV_EXHAUSTS];	/* 0x00d0 */
	byte pad_110[0x190 - 0x110];
	int sputnikNode;		/* 0x0190 "Bip Spootnick MASTER" of the entity "Sputnik", -1 none */
	CGameTrail trails[FXVSPUTNIK_TRAILS];	/* 0x0194 */
	byte pad_1c8[0xaec - 0x1c8];
	float faec[FXVSPUTNIK_RANDS];	/* 0x0aec -1 */
	byte pad_df8[0x1104 - 0xdf8];
	float rands[FXVSPUTNIK_RANDS];	/* 0x1104 0.15..0.55 */
	byte pad_1410[0x171c - 0x1410];
	byte on;			/* 0x171c */
	byte pad_171d[3];
} cObjElementFXVehicleSputnik;
SD_OFFSET(cObjElementFXVehicleSputnik, active, 0xc8);
SD_OFFSET(cObjElementFXVehicleSputnik, sputnikNode, 0x190);
SD_OFFSET(cObjElementFXVehicleSputnik, trails, 0x194);
SD_OFFSET(cObjElementFXVehicleSputnik, faec, 0xaec);
SD_OFFSET(cObjElementFXVehicleSputnik, rands, 0x1104);
SD_OFFSET(cObjElementFXVehicleSputnik, on, 0x171c);
SD_SIZE(cObjElementFXVehicleSputnik, 0x1720);

/* the constructors (thiscall, return self), destructors and deleting destructors */
void *__thiscall cObjElementFXFiredDebris_ctor(void *self);
void *__thiscall cObjElementFXFiredEntity_ctor(void *self);
void *__thiscall cObjElementFXSmokeDebris_ctor(void *self);
void *__thiscall cObjElementFXVehicleAccelerator_ctor(void *self);
void *__thiscall cObjElementFXVehicleDamage_ctor(void *self);
void *__thiscall cObjElementFXVehicleDeathTrail_ctor(void *self);
void *__thiscall cObjElementFXVehicleEMI_ctor(void *self);
void *__thiscall cObjElementFXVehicleFire_ctor(void *self);
void *__thiscall cObjElementFXVehicleSpawn_ctor(void *self);
void *__thiscall cObjElementFXVehicleSputnik_ctor(void *self);
void __thiscall cObjElementFXFiredDebris_dtor(void *self);
void __thiscall cObjElementFXFiredEntity_dtor(void *self);
void __thiscall cObjElementFXSmokeDebris_dtor(void *self);
void __thiscall cObjElementFXVehicleAccelerator_dtor(void *self);
void __thiscall cObjElementFXVehicleDamage_dtor(void *self);
void __thiscall cObjElementFXVehicleDeathTrail_dtor(void *self);
void __thiscall cObjElementFXVehicleEMI_dtor(void *self);
void __thiscall cObjElementFXVehicleFire_dtor(void *self);
void __thiscall cObjElementFXVehicleSpawn_dtor(void *self);
void __thiscall cObjElementFXVehicleSputnik_dtor(void *self);
void *__thiscall cObjElementFXFiredDebris_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXFiredEntity_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXSmokeDebris_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXVehicleAccelerator_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXVehicleDamage_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXVehicleDeathTrail_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXVehicleEMI_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXVehicleFire_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXVehicleSpawn_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXVehicleSputnik_Delete(void *self, uint32_t flags);

/* Update */
void __thiscall cObjElementFXFiredDebris_Update(cObjElementFXFiredDebris *self, cDebris *debris, float dt);
void __thiscall cObjElementFXSmokeDebris_Update(cObjElementFXSmokeDebris *self, cDebris *debris, float dt);
void __thiscall cObjElementFXVehicleSpawn_Update(cObjElementFXVehicleSpawn *self, cDebris *debris, float dt);

/* IsAlive; the ones of FiredDebris, SmokeDebris, Accelerator and Spawn are FXMisc_Alive, the DeathTrail is the
 * code of cObjElementFXDevastatorShoot (Update, IsAlive, Reset) */
byte __thiscall cObjElementFXFiredEntity_IsAlive(cObjElementFXFiredEntity *self, cDebris *debris, cDebrisSys *sys);
byte __thiscall cObjElementFXVehicleDamage_IsAlive(cObjElementFXVehicleDamage *self, cDebris *debris,
	cDebrisSys *sys);
byte __thiscall cObjElementFXVehicleEMI_IsAlive(cObjElementFXVehicleEMI *self, cDebris *debris, cDebrisSys *sys);
byte __thiscall cObjElementFXVehicleFire_IsAlive(cObjElementFXVehicleFire *self, cDebris *debris, cDebrisSys *sys);
byte __thiscall cObjElementFXVehicleSputnik_IsAlive(cObjElementFXVehicleSputnik *self, cDebris *debris,
	cDebrisSys *sys);

void __thiscall cObjElementFXFiredDebris_Start(cObjElementFXFiredDebris *self, cDebris *debris);
void __thiscall cObjElementFXFiredEntity_Start(cObjElementFXFiredEntity *self, cDebris *debris);
void __thiscall cObjElementFXSmokeDebris_Start(cObjElementFXSmokeDebris *self, cDebris *debris);
void __thiscall cObjElementFXVehicleAccelerator_Start(cObjElementFXVehicleAccelerator *self, cDebris *debris);
void __thiscall cObjElementFXVehicleDamage_Start(cObjElementFXVehicleDamage *self, cDebris *debris);
void __thiscall cObjElementFXVehicleDeathTrail_Start(cObjElementFXDevastatorShoot *self, cDebris *debris);
void __thiscall cObjElementFXVehicleEMI_Start(cObjElementFXVehicleEMI *self, cDebris *debris);
void __thiscall cObjElementFXVehicleFire_Start(cObjElementFXVehicleFire *self, cDebris *debris);
void __thiscall cObjElementFXVehicleSpawn_Start(cObjElementFXVehicleSpawn *self, cDebris *debris);
void __thiscall cObjElementFXVehicleSputnik_Start(cObjElementFXVehicleSputnik *self, cDebris *debris);

void __thiscall cObjElementFXFiredDebris_Reset(cObjElementFXFiredDebris *self, cDebris *debris);
void __thiscall cObjElementFXFiredEntity_Reset(cObjElementFXFiredEntity *self, cDebris *debris);
void __thiscall cObjElementFXSmokeDebris_Reset(cObjElementFXSmokeDebris *self, cDebris *debris);
void __thiscall cObjElementFXVehicleAccelerator_Reset(cObjElementFXVehicleAccelerator *self, cDebris *debris);
void __thiscall cObjElementFXVehicleDamage_Reset(cObjElementFXVehicleDamage *self, cDebris *debris);
void __thiscall cObjElementFXVehicleEMI_Reset(cObjElementFXVehicleEMI *self, cDebris *debris);
void __thiscall cObjElementFXVehicleFire_Reset(cObjElementFXVehicleFire *self, cDebris *debris);
void __thiscall cObjElementFXVehicleSpawn_Reset(cObjElementFXVehicleSpawn *self, cDebris *debris);
void __thiscall cObjElementFXVehicleSputnik_Reset(cObjElementFXVehicleSputnik *self, cDebris *debris);

/* the setters after the start by the vehicle (Entities/Vehicle) */
void __thiscall cObjElementFXFiredEntity_SetEntity(cObjElementFXFiredEntity *self, cEntity *e);
void __thiscall cObjElementFXVehicleDamage_SetEntity(cObjElementFXVehicleDamage *self, cEntity *e);
void __thiscall cObjElementFXVehicleEMI_SetEntity(cObjElementFXVehicleEMI *self, cEntity *e);
void __thiscall cObjElementFXVehicleFire_SetEntity(cObjElementFXVehicleFire *self, cEntity *e);
void __thiscall cObjElementFXVehicleSputnik_SetEntity(cObjElementFXVehicleSputnik *self, cEntity *e);
void __thiscall cObjElementFXVehicleSputnik_SetC0(cObjElementFXVehicleSputnik *self, int v);
void __thiscall cObjElementFXVehicleSputnik_SetC4(cObjElementFXVehicleSputnik *self, float v);
void __thiscall cObjElementFXVehicleSpawn_SetEntity(cObjElementFXVehicleSpawn *self, cEntity *e);
void __thiscall cObjElementFXVehicleAccelerator_SetOwner(cObjElementFXVehicleAccelerator *self, cEntity *e);
void __thiscall cObjElementFXVehicleAccelerator_SetAccelerator(cObjElementFXVehicleAccelerator *self, void *a);
/* FXVehicle_RenderAttached (rva 0x1f9050, the damage effect drawn with its ship) is in embeddedfxvehicle.h */

/* World/FX/FXScene.cpp, the end of it in this range: the two names (31 characters at most) and the float at
 * 0xfc of cObjElementFXSceneDecontamination */
void __thiscall cObjElementFXSceneDecontamination_SetNames(void *self, const char *a, const char *b);
void __thiscall cObjElementFXSceneDecontamination_SetFC(void *self, float v);

#endif
