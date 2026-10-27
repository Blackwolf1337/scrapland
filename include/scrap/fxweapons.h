/*
 * World/FX/FXWeapons.cpp of the game: the effects of the weapon shots, objects (cObjElementFX) on the debris of the
 * debris systems like the ones of include/scrap/fxcountermeasures.h. The debris system makes them by effect kind
 * (Scrap.SetDebrisSys): 32 SwarmMissile, 33 Tesla, 34 ATPC, 41 Laser, 43 InfernoMissile, 44 InfernoExplosion,
 * 46 DevastatorShoot. The vtable: Delete, Update (debris, dt), IsAlive (debris, sys), Start (debris), Reset (debris).
 */
#ifndef SCRAP_FXWEAPONS_H
#define SCRAP_FXWEAPONS_H

#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"

#define COBJELEMENTFXSWARMMISSILE_VFTABLE 0x3cdc50
#define COBJELEMENTFXLASER_VFTABLE 0x3cdc68
#define COBJELEMENTFXDEVASTATORSHOOT_VFTABLE 0x3cdc80
#define COBJELEMENTFXTESLA_VFTABLE 0x3cdc98
#define COBJELEMENTFXATPC_VFTABLE 0x3cdcb0
#define COBJELEMENTFXINFERNOMISSILE_VFTABLE 0x3cdcc8
#define COBJELEMENTFXINFERNOEXPLOSION_VFTABLE 0x3cdce0

#define FXINFERNO_RAYS 224
#define FXINFERNO_TRAILS 15		/* 15 rays per trail */
#define FXTESLA_SLOTS 4
#define FXTESLA_TRAILS 4		/* per slot */

/* ScrapEngLensFlares.cpp: the holder of a lens flare (one pointer); the setters write the flare object */
void __thiscall CGameLensFlare_SetOn(void *self, byte on);			/* rva 0x232f30 */
void __thiscall CGameLensFlare_SetIntensity(void *self, float v);		/* rva 0x232f80, +0x14 */
void __thiscall CGameLensFlare_SetF30(void *self, float v);			/* rva 0x232fa0, +0x30 */
void __thiscall CGameLensFlare_SetPos(void *self, vec3_t pos);		/* rva 0x232fc0, +0x08 */
void __thiscall CGameLensFlare_SetF18(void *self, float v);			/* rva 0x232ff0, +0x18 */
void __thiscall CGameLensFlare_SetI1c(void *self, int v);			/* rva 0x233010, +0x1c */

/* the weapon block of a ship at 0x5f0 (the one of Entity_TargetLock): the ammo of kind i (0 out of range), whether
 * slot (0..4) of weapon kind w is mounted (rva 0x11a6c0, 0x11a9f0) */
float __thiscall cTargetLock_GetAmmo(cTargetLock *self, int ammo);
byte __thiscall cTargetLock_HasSlot(cTargetLock *self, int weap, int slot);
#define Entity_B436(e) (*(byte *)((byte *)(e) + 0x436))
#define Entity_B636(e) (*(byte *)((byte *)(e) + 0x636))
#define Entity_CurWeapon(e) (*(int *)((byte *)(e) + 0x5fc))	/* the selected weapon kind */

/* the engine variables */
#define g_TeslaIDWeap GAME_DATA(int, 0x448884)
#define g_TeslaIDAmmo GAME_DATA(int, 0x44a828)
#define g_FXTeslaFadeOutTime GAME_DATA(float, 0x44a6e8)
#define g_FXTeslaFollowTime GAME_DATA(float, 0x44a700)
#define g_FXLaserBeamSize GAME_DATA(float, 0x449c6c)
#define g_FXLaserBeamHeight GAME_DATA(float, 0x449e6c)
#define g_FXInfernoCoreSize GAME_DATA(float, 0x449850)
#define g_FXInfernoRayRadius GAME_DATA(float, 0x449880)
#define g_FXInfernoExplosionTime GAME_DATA(float, 0x449898)
#define g_FXInfernoExplosionFadeTime GAME_DATA(float, 0x449a18)
#define g_FXInfernoRayMinSize GAME_DATA(float, 0x449b60)

/* the ATPC shot (0x1e8 bytes, constructor rva 0x1f9690) */
typedef struct cObjElementFXATPC {
	cObjElementFX base;
	int f04;			/* 0x004 */
	byte pad_08[0x20 - 0x08];
	CGameParticles particles;	/* 0x020 on Dummy01 */
	CGameParticles particles2;	/* 0x0d0 */
	int f180;			/* 0x180 */
	byte pad_184[0x18c - 0x184];
	int f18c;			/* 0x18c */
	byte pad_190[0x1c4 - 0x190];
	CGameTrail trail1c4;		/* 0x1c4 */
	CGameTrail trail1c8;		/* 0x1c8 */
	int n1cc;			/* 0x1cc 12 */
	CGameTrail trail1d0;		/* 0x1d0 */
	int n1d4;			/* 0x1d4 8 */
	CGameTrail trail1d8;		/* 0x1d8 */
	int f1dc;			/* 0x1dc */
	CGameTrail trail1e0;		/* 0x1e0 2 * n1cc + 1 points */
	byte on;			/* 0x1e4 */
	byte pad_1e5[3];
} cObjElementFXATPC;
SD_OFFSET(cObjElementFXATPC, particles2, 0xd0);
SD_OFFSET(cObjElementFXATPC, trail1c4, 0x1c4);
SD_OFFSET(cObjElementFXATPC, on, 0x1e4);
SD_SIZE(cObjElementFXATPC, 0x1e8);

/* the Devastator shot: a trail that fades in (0x14 bytes, constructor rva 0x1f9770) */
typedef struct cObjElementFXDevastatorShoot {
	cObjElementFX base;
	CGameTrail trail;		/* 0x04 */
	float duration;			/* 0x08 */
	float time;			/* 0x0c */
	byte first;			/* 0x10 set by Reset, the first Update keeps the trail transparent */
	byte on;			/* 0x11 */
	byte pad_12[2];
} cObjElementFXDevastatorShoot;
SD_OFFSET(cObjElementFXDevastatorShoot, first, 0x10);
SD_SIZE(cObjElementFXDevastatorShoot, 0x14);

/* the Inferno explosion: rays out of the center drawn as trails and the core particles (0x24c8 bytes, constructor
 * rva 0x1f97e0) */
typedef struct cObjElementFXInfernoExplosion {
	cObjElementFX base;
	CGameParticles particles;	/* 0x0004 on Dummy01 */
	CGameParticles cores;		/* 0x00b4 on Dummy02(+dudv_pass), 2 particles */
	float time;			/* 0x0164 */
	float stepTime;			/* 0x0168 since the last step (steps of 0.01 at least) */
	vec3_t pos;			/* 0x016c */
	byte pad_178[0x184 - 0x178];
	vec3_t rayBase[FXINFERNO_RAYS];	/* 0x0184 */
	vec3_t rayDir[FXINFERNO_RAYS];	/* 0x0c04 */
	float rayLen[FXINFERNO_RAYS];	/* 0x1684 */
	float raySpeed[FXINFERNO_RAYS];	/* 0x1a04 */
	float rayAcc[FXINFERNO_RAYS];	/* 0x1d84 */
	float rayMax[FXINFERNO_RAYS];	/* 0x2104 */
	CGameTrail trails[FXINFERNO_TRAILS];	/* 0x2484 */
	int numRays;			/* 0x24c0 */
	byte on;			/* 0x24c4 */
	byte pad_24c5[3];
} cObjElementFXInfernoExplosion;
SD_OFFSET(cObjElementFXInfernoExplosion, cores, 0xb4);
SD_OFFSET(cObjElementFXInfernoExplosion, rayBase, 0x184);
SD_OFFSET(cObjElementFXInfernoExplosion, rayDir, 0xc04);
SD_OFFSET(cObjElementFXInfernoExplosion, rayLen, 0x1684);
SD_OFFSET(cObjElementFXInfernoExplosion, rayMax, 0x2104);
SD_OFFSET(cObjElementFXInfernoExplosion, trails, 0x2484);
SD_OFFSET(cObjElementFXInfernoExplosion, numRays, 0x24c0);
SD_SIZE(cObjElementFXInfernoExplosion, 0x24c8);

/* the Inferno missile: its smoke trail, particles and a lens flare (0xe4 bytes, constructor rva 0x1f9880) */
typedef struct cObjElementFXInfernoMissile {
	cObjElementFX base;
	CGameTrail trail;		/* 0x04 */
	CGameParticles particles;	/* 0x08 on Dummy01 */
	vec3_t last;			/* 0xb8 the last trail point */
	float minDist;			/* 0xc4 between two trail points */
	float fadeTime;			/* 0xc8 */
	float time;			/* 0xcc */
	float fadeStart;		/* 0xd0 -1 while flying */
	float lastShrink;		/* 0xd4 */
	byte flareOn;			/* 0xd8 */
	byte pad_d9[3];
	void *flare;			/* 0xdc CGameLensFlare */
	byte on;			/* 0xe0 */
	byte pad_e1[3];
} cObjElementFXInfernoMissile;
SD_OFFSET(cObjElementFXInfernoMissile, last, 0xb8);
SD_OFFSET(cObjElementFXInfernoMissile, flare, 0xdc);
SD_SIZE(cObjElementFXInfernoMissile, 0xe4);

/* the laser shot: a beam from start to end that moves along (0x124 bytes, constructor rva 0x1f9900) */
typedef struct cObjElementFXLaser {
	cObjElementFX base;
	CGameTrail trail;		/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	byte pad_b8[0xc4 - 0xb8];
	float time;			/* 0x0c4 */
	vec3_t start;			/* 0x0c8 */
	vec3_t end;			/* 0x0d4 */
	float duration;			/* 0x0e0 */
	vec3_t delta;			/* 0x0e4 end - start */
	float len;			/* 0x0f0 */
	vec3_t dir;			/* 0x0f4 */
	vec3_t pos;			/* 0x100 the head */
	float dist;			/* 0x10c of the head from start (from end in Laser_Start) */
	float rest;			/* 0x110 of the head from end */
	vec3_t vel;			/* 0x114 */
	byte drawn;			/* 0x120 */
	byte on;			/* 0x121 */
	byte pad_122[2];
} cObjElementFXLaser;
SD_OFFSET(cObjElementFXLaser, time, 0xc4);
SD_OFFSET(cObjElementFXLaser, pos, 0x100);
SD_OFFSET(cObjElementFXLaser, drawn, 0x120);
SD_SIZE(cObjElementFXLaser, 0x124);

/* the Swarm missile: its trail and the motor particles (0xe0 bytes, constructor rva 0x1f9970) */
typedef struct cObjElementFXSwarmMissile {
	cObjElementFX base;
	int node;			/* 0x04 DF_MissileMotor, -1 for none */
	CGameTrail trail;		/* 0x08 */
	CGameParticles particles;	/* 0x0c */
	vec3_t last;			/* 0xbc */
	float minDist2;			/* 0xc8 squared */
	float fadeTime;			/* 0xcc */
	float time;			/* 0xd0 */
	float fadeStart;		/* 0xd4 -1 while flying */
	float lastShrink;		/* 0xd8 */
	byte on;			/* 0xdc */
	byte pad_dd[3];
} cObjElementFXSwarmMissile;
SD_OFFSET(cObjElementFXSwarmMissile, last, 0xbc);
SD_OFFSET(cObjElementFXSwarmMissile, lastShrink, 0xd8);
SD_SIZE(cObjElementFXSwarmMissile, 0xe0);

/* the Tesla ray from the weapon slots of the emitter (0x768 bytes, constructor rva 0x1f99e0) */
typedef struct cObjElementFXTesla {
	cObjElementFX base;
	cEntity *emitter;		/* 0x004 */
	byte b8;			/* 0x008 */
	byte pad_09[0x88 - 0x09];
	int f88;			/* 0x088 */
	byte pad_8c[0x98 - 0x8c];
	int numSlots;			/* 0x098 */
	byte slotOn[FXTESLA_SLOTS];	/* 0x09c */
	CGameParticles particles;	/* 0x0a0 on Dummy01 */
	int f150;			/* 0x150 10 */
	byte pad_154[0x6f4 - 0x154];
	CGameTrail trails[FXTESLA_SLOTS * FXTESLA_TRAILS];	/* 0x6f4 */
	int a734[FXTESLA_SLOTS];	/* 0x734 */
	float time;			/* 0x744 */
	float fadeStart;		/* 0x748 -1 while shooting */
	byte shooting;			/* 0x74c */
	byte b74d;			/* 0x74d */
	byte pad_74e[2];
	int a750[FXTESLA_SLOTS];	/* 0x750 */
	float followTime;		/* 0x760 */
	byte on;			/* 0x764 */
	byte pad_765[3];
} cObjElementFXTesla;
SD_OFFSET(cObjElementFXTesla, numSlots, 0x98);
SD_OFFSET(cObjElementFXTesla, particles, 0xa0);
SD_OFFSET(cObjElementFXTesla, trails, 0x6f4);
SD_OFFSET(cObjElementFXTesla, time, 0x744);
SD_OFFSET(cObjElementFXTesla, shooting, 0x74c);
SD_OFFSET(cObjElementFXTesla, a750, 0x750);
SD_OFFSET(cObjElementFXTesla, followTime, 0x760);
SD_SIZE(cObjElementFXTesla, 0x768);

void __thiscall cObjElementFXATPC_Start(cObjElementFXATPC *self, cDebris *debris);
void __thiscall cObjElementFXATPC_Reset(cObjElementFXATPC *self, cDebris *debris);

void __thiscall cObjElementFXDevastatorShoot_Update(cObjElementFXDevastatorShoot *self, cDebris *debris, float dt);
byte __thiscall cObjElementFXDevastatorShoot_IsAlive(cObjElementFXDevastatorShoot *self, cDebris *debris,
	cDebrisSys *sys);
void __thiscall cObjElementFXDevastatorShoot_Start(cObjElementFXDevastatorShoot *self, cDebris *debris);
void __thiscall cObjElementFXDevastatorShoot_Reset(cObjElementFXDevastatorShoot *self, cDebris *debris);

void __thiscall cObjElementFXInfernoExplosion_Update(cObjElementFXInfernoExplosion *self, cDebris *debris, float dt);
void __thiscall cObjElementFXInfernoExplosion_Start(cObjElementFXInfernoExplosion *self, cDebris *debris);
void __thiscall cObjElementFXInfernoExplosion_Reset(cObjElementFXInfernoExplosion *self, cDebris *debris);

void __thiscall cObjElementFXInfernoMissile_Update(cObjElementFXInfernoMissile *self, cDebris *debris, float dt);
byte __thiscall cObjElementFXInfernoMissile_IsAlive(cObjElementFXInfernoMissile *self, cDebris *debris,
	cDebrisSys *sys);
void __thiscall cObjElementFXInfernoMissile_Start(cObjElementFXInfernoMissile *self, cDebris *debris);
void __thiscall cObjElementFXInfernoMissile_Reset(cObjElementFXInfernoMissile *self, cDebris *debris);
void __thiscall cObjElementFXInfernoMissile_SetFlare(cObjElementFXInfernoMissile *self, byte on);

void __thiscall cObjElementFXLaser_Update(cObjElementFXLaser *self, cDebris *debris, float dt);
byte __thiscall cObjElementFXLaser_IsAlive(cObjElementFXLaser *self, cDebris *debris, cDebrisSys *sys);
void __thiscall cObjElementFXLaser_Start(cObjElementFXLaser *self, cDebris *debris);
void __thiscall cObjElementFXLaser_Reset(cObjElementFXLaser *self, cDebris *debris);
/* the beam from (x0 y0 z0) to (x1 y1 z1) in time */
void __thiscall cObjElementFXLaser_Shoot(cObjElementFXLaser *self, float x0, float y0, float z0, float x1, float y1,
	float z1, float time);

void __thiscall cObjElementFXSwarmMissile_Update(cObjElementFXSwarmMissile *self, cDebris *debris, float dt);
byte __thiscall cObjElementFXSwarmMissile_IsAlive(cObjElementFXSwarmMissile *self, cDebris *debris, cDebrisSys *sys);
void __thiscall cObjElementFXSwarmMissile_Start(cObjElementFXSwarmMissile *self, cDebris *debris);
void __thiscall cObjElementFXSwarmMissile_Reset(cObjElementFXSwarmMissile *self, cDebris *debris);

byte __thiscall cObjElementFXTesla_IsAlive(cObjElementFXTesla *self, cDebris *debris, cDebrisSys *sys);
void __thiscall cObjElementFXTesla_Start(cObjElementFXTesla *self, cDebris *debris);
void __thiscall cObjElementFXTesla_Reset(cObjElementFXTesla *self, cDebris *debris);
/* the ship that shoots and its mounted Tesla slots */
void __thiscall cObjElementFXTesla_SetEmitter(cObjElementFXTesla *self, cEntity *e);

#endif
