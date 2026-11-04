/*
 * World/FX/FXWeapons.cpp of the game: the effects of the weapon shots, objects (cObjElementFX) on the debris of the
 * debris systems like the ones of include/scrap/fxcountermeasures.h. The debris system makes them by effect kind
 * (Scrap.SetDebrisSys): 32 SwarmMissile, 33 Tesla, 34 ATPC, 41 Laser, 43 InfernoMissile, 44 InfernoExplosion,
 * 46 DevastatorShoot. The vtable: Delete, Update (debris, dt), IsAlive (debris, sys), Start (debris), Reset (debris).
 */
#ifndef SCRAP_FXWEAPONS_H
#define SCRAP_FXWEAPONS_H

#include "scrap/gvar.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/global.h"

#define FXINFERNO_RAYS 224
#define FXINFERNO_TRAILS 15		/* 15 rays per trail */
#define FXTESLA_SLOTS 4
#define FXTESLA_TRAILS 4		/* per slot */

/* ScrapEngLensFlares.cpp: the holder of a lens flare (one pointer); the setters write the flare object */
void CGameLensFlare_SetOn(void *self, byte on);
void CGameLensFlare_SetIntensity(void *self, float v);		/* rva 0x232f80, +0x14 */
void CGameLensFlare_SetTraceInterval(void *self, float v);			/* rva 0x232fa0, +0x30 */
void CGameLensFlare_SetPos(void *self, vec3_t pos);		/* rva 0x232fc0, +0x08 */
void CGameLensFlare_SetScale(void *self, float v);			/* rva 0x232ff0, +0x18 */
void CGameLensFlare_SetType(void *self, int v);			/* rva 0x233010, +0x1c */
void CGameLensFlare_SetRelative(void *self, byte v);			/* rva 0x233020, +0x2c */

/* the lens flare object of the engine glue: CLensFlare of lensflare.h */

/* the weapon block of a ship at 0x5f0 (the one of Entity_TargetLock): the ammo of kind i (0 out of range), whether
 * slot (0..4) of weapon kind w is mounted */
float cTargetLock_GetAmmo(cTargetLock *self, int ammo);
byte cTargetLock_HasSlot(cTargetLock *self, int weap, int slot);

/* the engine variables */
extern cGVar GVar_TeslaIDWeap;
#define g_TeslaIDWeap (GVar_TeslaIDWeap.value.i)
extern cGVar GVar_TeslaIDAmmo;
#define g_TeslaIDAmmo (GVar_TeslaIDAmmo.value.i)
extern cGVar GVar_FXInfernoExplosionTime;
#define g_FXInfernoExplosionTime (GVar_FXInfernoExplosionTime.value.f)

/* the ATPC shot: the beam from the shooter to the hit, its rings and glow (0x1e8 bytes, constructor rva 0x1f9690) */
#define FXATPC_SLOTS 4
typedef struct cObjElementFXATPC {
	cObjElementFX base;
	cEntity *shooter;		/* 0x004 */
	vec3_t from;			/* 0x008 the origin of the shot */
	vec3_t to;			/* 0x014 the hit */
	CGameParticles particles;	/* 0x020 on Dummy01 */
	CGameParticles particles2;	/* 0x0d0 */
	float time;			/* 0x180 since the shot */
	int numSlots;			/* 0x188 the mounted slots of the shooter */
	byte slotOn[FXATPC_SLOTS];	/* 0x18c */
	vec3_t slotPos[FXATPC_SLOTS];	/* 0x190 the weapon nodes of the slots */
	float width;			/* 0x1c0 by the slots and ATPCShootSize */
	CGameTrail core;		/* 0x1c4 */
	CGameTrail line;		/* 0x1c8 */
	int ringPoints;			/* 0x1cc 12, the points of each ring */
	CGameTrail rings;		/* 0x1d0 */
	int endRingPoints;			/* 0x1d4 8 */
	CGameTrail endRing;		/* 0x1d8 */
	float angle;			/* 0x1dc of the rings */
	CGameTrail ringsDuDv;		/* 0x1e0 2 * ringPoints + 1 points, the dudv pass */
	byte on;			/* 0x1e4 */
	byte pad_1e5[3];
} cObjElementFXATPC;

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

/* the Inferno explosion: rays out of the center drawn as trails and the core particles (0x24c8 bytes, constructor
 * rva 0x1f97e0) */
typedef struct cObjElementFXInfernoExplosion {
	cObjElementFX base;
	CGameParticles particles;	/* 0x0004 on Dummy01 */
	CGameParticles cores;		/* 0x00b4 on Dummy02(+dudv_pass), 2 particles */
	float time;			/* 0x0164 */
	float stepTime;			/* 0x0168 since the last step (steps of 0.01 at least) */
	vec3_t pos;			/* 0x016c */
	float range;			/* 0x0178 the length of the rays tested against the floor */
	int bees;			/* 0x017c the bays of the Inferno, a bigger explosion */
	byte flag;			/* 0x0180 */
	byte pad_181[3];
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

/* the laser shot: a beam from start to end that moves along (0x124 bytes, constructor rva 0x1f9900) */
typedef struct cObjElementFXLaser {
	cObjElementFX base;
	CGameTrail trail;		/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
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

/* the Tesla ray from the weapon slots of the emitter to the target: per slot a Bezier curve through numPts points
 * that wander by random offsets (0x768 bytes, constructor rva 0x1f99e0) */
#define FXTESLA_PTS 10
typedef struct cObjElementFXTesla {
	cObjElementFX base;
	cEntity *emitter;		/* 0x004 */
	char name[0x80];		/* 0x008 the target of the hit info of the emitter */
	cEntity *target;		/* 0x088 */
	vec3_t tpos;			/* 0x08c the end of the ray */
	int numSlots;			/* 0x098 */
	byte slotOn[FXTESLA_SLOTS];	/* 0x09c */
	CGameParticles particles;	/* 0x0a0 on Dummy01 */
	int numPts;			/* 0x150 10 */
	vec3_t offs[FXTESLA_SLOTS][2 * FXTESLA_PTS];	/* 0x154 the offsets of the points, then the ones before */
	vec3_t pts[FXTESLA_SLOTS][FXTESLA_PTS];	/* 0x514 the control points */
	CGameTrail trails[FXTESLA_SLOTS * FXTESLA_TRAILS];	/* 0x6f4 */
	float uvOfs[FXTESLA_SLOTS];	/* 0x734 */
	float time;			/* 0x744 */
	float fadeStart;		/* 0x748 -1 while shooting */
	byte shooting;			/* 0x74c */
	byte first;			/* 0x74d the next Update takes the offsets as they are */
	byte pad_74e[2];
	float stepTime[FXTESLA_SLOTS];	/* 0x750 since the offsets changed */
	float followTime;		/* 0x760 */
	byte on;			/* 0x764 */
	byte pad_765[3];
} cObjElementFXTesla;

/* the helpers of the constructors and destructors of the effects (also FXVehicle.c) */

/* the start of every constructor: the base class leaves 0, then the vtable of the class */
static inline void FXObj_TrailsCtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_ctor(t, sizeof(CGameTrail), n, (void *)CGameTrail_ctor, (void *)CGameTrail_dtor);
}

static inline void FXObj_TrailsDtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_dtor(t, sizeof(CGameTrail), n, (void *)CGameTrail_dtor);
}

/* (x, y, z) normalized into out, the length summed y first (the vector itself for the length 0) */
static inline void FX_Normalize(vec3_t *out, float x, float y, float z) {
	float len = (float)__builtin_sqrt((double)(y * y + x * x + z * z));
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

/* the constructors (rva 0x1f9690..), destructors and deleting destructors (vf00) */
void *cObjElementFXATPC_ctor(void *self);
void *cObjElementFXDevastatorShoot_ctor(void *self);
void *cObjElementFXInfernoExplosion_ctor(void *self);
void *cObjElementFXInfernoMissile_ctor(void *self);
void *cObjElementFXLaser_ctor(void *self);
void *cObjElementFXSwarmMissile_ctor(void *self);
void *cObjElementFXTesla_ctor(void *self);

void cObjElementFXDevastatorShoot_Update(cObjElementFX *base, cDebris *debris, float dt);
byte cObjElementFXDevastatorShoot_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
void cObjElementFXDevastatorShoot_Reset(cObjElementFX *base, cDebris *debris);

void cObjElementFXInfernoMissile_SetFlare(cObjElementFXInfernoMissile *self, byte on);

/* the beam from (x0 y0 z0) to (x1 y1 z1) in time */
void cObjElementFXLaser_Shoot(cObjElementFXLaser *self, float x0, float y0, float z0, float x1, float y1,
	float z1, float time);

/* the ship that shoots and its mounted Tesla slots */
void cObjElementFXTesla_SetEmitter(cObjElementFXTesla *self, cEntity *e);

#endif
