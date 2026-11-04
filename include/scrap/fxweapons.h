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
void CGameLensFlare_SetIntensity(void *self, float v);		/* +0x14 */
void CGameLensFlare_SetTraceInterval(void *self, float v);			/* +0x30 */
void CGameLensFlare_SetPos(void *self, vec3_t pos);		/* +0x08 */
void CGameLensFlare_SetScale(void *self, float v);			/* +0x18 */
void CGameLensFlare_SetType(void *self, int v);			/* +0x1c */
void CGameLensFlare_SetRelative(void *self, byte v);			/* +0x2c */

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

/* the ATPC shot: the beam from the shooter to the hit, its rings and glow (constructor) */
#define FXATPC_SLOTS 4
typedef struct cObjElementFXATPC {
	cObjElementFX base;
	cEntity *shooter;
	vec3_t from;			/* the origin of the shot */
	vec3_t to;			/* the hit */
	CGameParticles particles;	/* on Dummy01 */
	CGameParticles particles2;
	float time;			/* since the shot */
	int numSlots;			/* the mounted slots of the shooter */
	byte slotOn[FXATPC_SLOTS];
	vec3_t slotPos[FXATPC_SLOTS];	/* the weapon nodes of the slots */
	float width;			/* by the slots and ATPCShootSize */
	CGameTrail core;
	CGameTrail line;
	int ringPoints;			/* 12, the points of each ring */
	CGameTrail rings;
	int endRingPoints;			/* 8 */
	CGameTrail endRing;
	float angle;			/* of the rings */
	CGameTrail ringsDuDv;		/* 2 * ringPoints + 1 points, the dudv pass */
	byte on;
	byte pad_1e5[3];
} cObjElementFXATPC;

/* the Devastator shot: a trail that fades in (constructor) */
typedef struct cObjElementFXDevastatorShoot {
	cObjElementFX base;
	CGameTrail trail;
	float duration;
	float time;
	byte first;			/* set by Reset, the first Update keeps the trail transparent */
	byte on;
	byte pad_12[2];
} cObjElementFXDevastatorShoot;

/* the Inferno explosion: rays out of the center drawn as trails and the core particles (0x24c8 bytes, constructor
 * ) */
typedef struct cObjElementFXInfernoExplosion {
	cObjElementFX base;
	CGameParticles particles;	/* on Dummy01 */
	CGameParticles cores;		/* on Dummy02(+dudv_pass), 2 particles */
	float time;
	float stepTime;			/* since the last step (steps of 0.01 at least) */
	vec3_t pos;
	float range;			/* the length of the rays tested against the floor */
	int bees;			/* the bays of the Inferno, a bigger explosion */
	byte flag;
	byte pad_181[3];
	vec3_t rayBase[FXINFERNO_RAYS];
	vec3_t rayDir[FXINFERNO_RAYS];
	float rayLen[FXINFERNO_RAYS];
	float raySpeed[FXINFERNO_RAYS];
	float rayAcc[FXINFERNO_RAYS];
	float rayMax[FXINFERNO_RAYS];
	CGameTrail trails[FXINFERNO_TRAILS];
	int numRays;
	byte on;
	byte pad_24c5[3];
} cObjElementFXInfernoExplosion;

/* the Inferno missile: its smoke trail, particles and a lens flare (constructor) */
typedef struct cObjElementFXInfernoMissile {
	cObjElementFX base;
	CGameTrail trail;
	CGameParticles particles;	/* on Dummy01 */
	vec3_t last;			/* the last trail point */
	float minDist;			/* between two trail points */
	float fadeTime;
	float time;
	float fadeStart;		/* -1 while flying */
	float lastShrink;
	byte flareOn;
	byte pad_d9[3];
	void *flare;			/* CGameLensFlare */
	byte on;
	byte pad_e1[3];
} cObjElementFXInfernoMissile;

/* the laser shot: a beam from start to end that moves along (constructor) */
typedef struct cObjElementFXLaser {
	cObjElementFX base;
	CGameTrail trail;
	CGameParticles particles;	/* on Dummy01 */
	float time;
	vec3_t start;
	vec3_t end;
	float duration;
	vec3_t delta;			/* end - start */
	float len;
	vec3_t dir;
	vec3_t pos;			/* the head */
	float dist;			/* of the head from start (from end in Laser_Start) */
	float rest;			/* of the head from end */
	vec3_t vel;
	byte drawn;
	byte on;
	byte pad_122[2];
} cObjElementFXLaser;

/* the Swarm missile: its trail and the motor particles (constructor) */
typedef struct cObjElementFXSwarmMissile {
	cObjElementFX base;
	int node;			/* DF_MissileMotor, -1 for none */
	CGameTrail trail;
	CGameParticles particles;
	vec3_t last;
	float minDist2;			/* squared */
	float fadeTime;
	float time;
	float fadeStart;		/* -1 while flying */
	float lastShrink;
	byte on;
	byte pad_dd[3];
} cObjElementFXSwarmMissile;

/* the Tesla ray from the weapon slots of the emitter to the target: per slot a Bezier curve through numPts points
 * that wander by random offsets (constructor) */
#define FXTESLA_PTS 10
typedef struct cObjElementFXTesla {
	cObjElementFX base;
	cEntity *emitter;
	char name[0x80];		/* the target of the hit info of the emitter */
	cEntity *target;
	vec3_t tpos;			/* the end of the ray */
	int numSlots;
	byte slotOn[FXTESLA_SLOTS];
	CGameParticles particles;	/* on Dummy01 */
	int numPts;			/* 10 */
	vec3_t offs[FXTESLA_SLOTS][2 * FXTESLA_PTS];	/* the offsets of the points, then the ones before */
	vec3_t pts[FXTESLA_SLOTS][FXTESLA_PTS];	/* the control points */
	CGameTrail trails[FXTESLA_SLOTS * FXTESLA_TRAILS];
	float uvOfs[FXTESLA_SLOTS];
	float time;
	float fadeStart;		/* -1 while shooting */
	byte shooting;
	byte first;			/* the next Update takes the offsets as they are */
	byte pad_74e[2];
	float stepTime[FXTESLA_SLOTS];	/* since the offsets changed */
	float followTime;
	byte on;
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

/* the constructors (..), destructors and the Free slots */
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
