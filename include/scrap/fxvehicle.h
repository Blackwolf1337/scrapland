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

#include "scrap/gvar.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/fxweapons.h"

extern const cObjElementFXVtbl cObjElementFXVehicleDamage_vftable;
#define COBJELEMENTFXVEHICLEDAMAGE_VFTABLE (&cObjElementFXVehicleDamage_vftable)
extern const cObjElementFXVtbl cObjElementFXVehicleFire_vftable;
#define COBJELEMENTFXVEHICLEFIRE_VFTABLE (&cObjElementFXVehicleFire_vftable)
extern const cObjElementFXVtbl cObjElementFXFiredEntity_vftable;
#define COBJELEMENTFXFIREDENTITY_VFTABLE (&cObjElementFXFiredEntity_vftable)
extern const cObjElementFXVtbl cObjElementFXVehicleEMI_vftable;
#define COBJELEMENTFXVEHICLEEMI_VFTABLE (&cObjElementFXVehicleEMI_vftable)
extern const cObjElementFXVtbl cObjElementFXFiredDebris_vftable;
#define COBJELEMENTFXFIREDDEBRIS_VFTABLE (&cObjElementFXFiredDebris_vftable)
extern const cObjElementFXVtbl cObjElementFXSmokeDebris_vftable;
#define COBJELEMENTFXSMOKEDEBRIS_VFTABLE (&cObjElementFXSmokeDebris_vftable)
extern const cObjElementFXVtbl cObjElementFXVehicleDeathTrail_vftable;
#define COBJELEMENTFXVEHICLEDEATHTRAIL_VFTABLE (&cObjElementFXVehicleDeathTrail_vftable)
extern const cObjElementFXVtbl cObjElementFXVehicleSputnik_vftable;
#define COBJELEMENTFXVEHICLESPUTNIK_VFTABLE (&cObjElementFXVehicleSputnik_vftable)
extern const cObjElementFXVtbl cObjElementFXVehicleSpawn_vftable;
#define COBJELEMENTFXVEHICLESPAWN_VFTABLE (&cObjElementFXVehicleSpawn_vftable)
extern const cObjElementFXVtbl cObjElementFXVehicleAccelerator_vftable;
#define COBJELEMENTFXVEHICLEACCELERATOR_VFTABLE (&cObjElementFXVehicleAccelerator_vftable)

#define FXV_MOTORS 4
#define FXV_EXHAUSTS (FXV_MOTORS * 2)
#define FXVACCEL_TRAILS 16
#define FXVEMI_TRAILS 16
#define FXVSPAWN_TRAILS 5
#define FXVSPUTNIK_TRAILS 13
#define FXVSPUTNIK_RANDS 195

/* the vehicle fields the effects read */
#define FXVeh_Motor(e, i) (Veh(e)->motors.motor[i])
#define FXVeh_MotorNode(m, side) ((m)->node[side])
#define FXVeh_EmbeddedFX(e) ((void *)Obj(e)->fx)
#define FXVeh_Effects(e) ((void *)&Veh(e)->effects)		/* Entities/Vehicle/VehicleEffects.cpp */

/* the engine variables */
extern cGVar GVar_StdMaxLife;
#define g_StdMaxLife (GVar_StdMaxLife.value.i)			/* StdMaxLife */
extern cGVar GVar_FXSetOnFireLifePercent;
#define g_FXSetOnFireLifePercent (GVar_FXSetOnFireLifePercent.value.i)	/* FXSetOnFireLifePercent */

/* VehicleEffects.cpp: the damage effect of the vehicle stopped (flag +4 and element +8 cleared) */
void cVehicleEffects_ClearDamage(void *self);

/* a motor exhaust of the ship */
typedef struct FXVExhaust {
	int motor;			/* 0x0 FXVeh_Motor */
	int side;			/* 0x4 0 or 1 */
} FXVExhaust;

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

typedef cObjElementFXFiredDebris cObjElementFXSmokeDebris;

/* the trail of a dying vehicle: the Devastator shot with 0x1e trail points (0x14 bytes) */
typedef cObjElementFXDevastatorShoot cObjElementFXVehicleDeathTrail;

/* the smoke of an entity on fire (0xd4 bytes) */
typedef struct cObjElementFXFiredEntity {
	cObjElementFX base;
	cEntity *entity;		/* 0x04 */
	byte posValid;		/* 0x08 pos holds the position of the last frame */
	byte pad_09[3];
	float radius;			/* 0x0c of its model */
	vec3_t pos;			/* 0x10 of its model */
	CGameParticles particles;	/* 0x1c on Dummy01 */
	float time;			/* 0xcc */
	byte on;			/* 0xd0 */
	byte pad_d1[3];
} cObjElementFXFiredEntity;

/* the two inner control points of a ray of the accelerator, offsets that move at random */
typedef struct FXVAccelCtrl {
	vec3_t a;			/* 0x00 */
	vec3_t b;			/* 0x0c */
} FXVAccelCtrl;

/* the rays of the accelerator: Bezier curves from a vertex of the ship (0x4c0 bytes) */
typedef struct cObjElementFXVehicleAccelerator {
	cObjElementFX base;
	cEntity *owner;			/* 0x004 */
	void *accelerator;		/* 0x008 */
	CGameParticles particles;	/* 0x00c on Dummy01, two per ray */
	float fade[FXVACCEL_TRAILS];	/* 0x0bc the time the ray has left */
	float delay[FXVACCEL_TRAILS];	/* 0x0fc before the ray starts */
	CGameTrail trails[FXVACCEL_TRAILS];	/* 0x13c */
	float width[FXVACCEL_TRAILS];	/* 0x17c */
	vec3_t from[FXVACCEL_TRAILS];	/* 0x1bc in the space of the ship */
	FXVAccelCtrl ctrl[FXVACCEL_TRAILS];	/* 0x27c */
	vec3_t to[FXVACCEL_TRAILS];	/* 0x3fc in the space of the ship */
	byte on;			/* 0x4bc */
	byte pad_4bd[3];
} cObjElementFXVehicleAccelerator;

/* the damage of a ship: smoke and sparks at the motors while the life is low (0x2d0 bytes) */
typedef struct cObjElementFXVehicleDamage {
	cObjElementFX base;
	cEntity *entity;		/* 0x004 */
	byte active;			/* 0x008 the life is checked */
	byte pad_09[3];
	int life;			/* 0x00c */
	int lifePct;			/* 0x010 */
	CGameModel *model;		/* 0x014 the model of the debris */
	byte seen;			/* 0x018 the ship was seen in the last Update, pos is valid */
	byte pad_19[3];
	CGameParticles parts3;		/* 0x01c on Dummy03: the smoke at the vertices */
	CGameParticles parts2;		/* 0x0cc on Dummy02: debris, sparks and flames */
	float level;			/* 0x17c follows lifePct / FXSetOnFireLifePercent: 0 burning, 1 out */
	float emitTime;			/* 0x180 since the last emission at the vertices */
	CGameParticles parts1;		/* 0x184 on Dummy01: the trail of smoke */
	vec3_t pos;			/* 0x234 of the ship model at the last puff of the trail */
	byte first;			/* 0x240 */
	byte pad_241[3];
	float trailTime;		/* 0x244 since the last puff of the trail */
	int numExhausts;		/* 0x248 */
	FXVExhaust exhausts[FXV_EXHAUSTS];	/* 0x24c */
	byte pad_28c[0x2cc - 0x28c];
	byte on;			/* 0x2cc */
	byte pad_2cd[3];
} cObjElementFXVehicleDamage;

/* the electromagnetic interference of a ship: rays that jump along the vertices of the ship or of one of its motors,
 * drawn as Bezier trails through their last points (0x82e4 bytes) */
#define FXVEMI_POINTS 64
#define FXVEMI_MARKS 0x200
typedef struct cObjElementFXVehicleEMI {
	cObjElementFX base;
	cEntity *entity;		/* 0x0004 */
	CGameParticles particles;	/* 0x0008 on Dummy01, three per ray */
	float stepTime;			/* 0x00b8 a step of the rays every 0.02 */
	float time;			/* 0x00bc */
	int numTrails;			/* 0x00c0 16 */
	float offset;			/* 0x00c4 22, the points out along the normal */
	int numVerts;			/* 0x00c8 0x1000, the vertices the marks hold */
	int maxPts;			/* 0x00cc 0x40 */
	vec3_t dir;			/* 0x00d0 of the last step */
	int numExhausts;		/* 0x00dc */
	FXVExhaust exhausts[FXV_EXHAUSTS];	/* 0x00e0 */
	byte pad_120[0x160 - 0x120];
	int part[FXVEMI_TRAILS];	/* 0x0160 the exhaust of the ray, -1 the whole ship */
	float rayTime[FXVEMI_TRAILS];	/* 0x01a0 */
	vec3_t pts[FXVEMI_TRAILS][FXVEMI_POINTS];	/* 0x01e0 a ring, in the space of the model */
	int vidx[FXVEMI_TRAILS][FXVEMI_POINTS];		/* 0x31e0 the vertex of each point */
	int head[FXVEMI_TRAILS];	/* 0x41e0 */
	int tail[FXVEMI_TRAILS];	/* 0x4220 */
	int count[FXVEMI_TRAILS];	/* 0x4260 */
	uint16_t marks[FXVEMI_TRAILS][FXVEMI_MARKS];	/* 0x42a0 a bit per vertex in the ring */
	CGameTrail trails[FXVEMI_TRAILS];	/* 0x82a0 */
	byte on;			/* 0x82e0 */
	byte pad_82e1[3];
} cObjElementFXVehicleEMI;

/* the fire at the motors of a ship (0x1fc bytes) */
typedef struct cObjElementFXVehicleFire {
	cObjElementFX base;
	cEntity *entity;		/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	CGameParticles particlesOut;	/* 0x0b8 on Dummy02, the smoke */
	float time;			/* 0x168 */
	float emitTime;			/* 0x16c since the last emission */
	byte pad_170[4];
	int numExhausts;		/* 0x174 */
	FXVExhaust exhausts[FXV_EXHAUSTS];	/* 0x178 */
	byte pad_1b8[0x1f8 - 0x1b8];
	byte on;			/* 0x1f8 */
	byte pad_1f9[3];
} cObjElementFXVehicleFire;

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

/* the Sputnik of a ship: rays from the Sputnik to points of the ship drawn as trails, a glow at the Sputnik
 * (0x1720 bytes) */
typedef struct cObjElementFXVehicleSputnik {
	cObjElementFX base;
	cEntity *entity;		/* 0x0004 */
	CGameParticles particles;	/* 0x0008 on Dummy01 */
	float time;			/* 0x00b8 */
	float time2;			/* 0x00bc */
	int phase;			/* 0x00c0 1 */
	float duration;			/* 0x00c4 0.8 */
	byte active;			/* 0x00c8 the active of the entity at the start, a change ends the effect */
	byte pad_c9[3];
	int numExhausts;		/* 0x00cc */
	FXVExhaust exhausts[FXV_EXHAUSTS];	/* 0x00d0 */
	byte pad_110[0x190 - 0x110];
	int sputnikNode;		/* 0x0190 "Bip Spootnick MASTER" of the entity "Sputnik", -1 none */
	CGameTrail trails[FXVSPUTNIK_TRAILS];	/* 0x0194 15 rays per trail */
	vec3_t rayPos[FXVSPUTNIK_RANDS];	/* 0x01c8 the point of the ship */
	float rayLife[FXVSPUTNIK_RANDS];	/* 0x0aec -1 */
	float rayLifeMax[FXVSPUTNIK_RANDS];	/* 0x0df8 */
	float rayDelay[FXVSPUTNIK_RANDS];	/* 0x1104 0.15..0.55 before the next ray */
	float rayWidth[FXVSPUTNIK_RANDS];	/* 0x1410 */
	byte on;			/* 0x171c */
	byte pad_171d[3];
} cObjElementFXVehicleSputnik;

/* the constructors (return self), destructors and deleting destructors */
void *cObjElementFXFiredDebris_ctor(void *self);
void *cObjElementFXFiredEntity_ctor(void *self);
void *cObjElementFXSmokeDebris_ctor(void *self);
void *cObjElementFXVehicleAccelerator_ctor(void *self);
void *cObjElementFXVehicleDamage_ctor(void *self);
cObjElementFXVehicleDeathTrail *cObjElementFXVehicleDeathTrail_ctor(cObjElementFXVehicleDeathTrail *self);
void *cObjElementFXVehicleEMI_ctor(void *self);
void *cObjElementFXVehicleFire_ctor(void *self);
void *cObjElementFXVehicleSpawn_ctor(void *self);
void *cObjElementFXVehicleSputnik_ctor(void *self);
void cObjElementFXFiredDebris_dtor(void *self);
void cObjElementFXFiredEntity_dtor(void *self);
void cObjElementFXSmokeDebris_dtor(void *self);
void cObjElementFXVehicleAccelerator_dtor(void *self);
void cObjElementFXVehicleDamage_dtor(void *self);
void cObjElementFXVehicleDeathTrail_dtor(cObjElementFXVehicleDeathTrail *self);
void cObjElementFXVehicleEMI_dtor(void *self);
void cObjElementFXVehicleFire_dtor(void *self);
void cObjElementFXVehicleSpawn_dtor(void *self);
void cObjElementFXVehicleSputnik_dtor(void *self);
cObjElementFX *cObjElementFXFiredDebris_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXFiredEntity_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXSmokeDebris_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXVehicleAccelerator_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXVehicleDamage_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXVehicleDeathTrail_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXVehicleEMI_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXVehicleFire_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXVehicleSpawn_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXVehicleSputnik_Delete(cObjElementFX *base, byte flags);

/* Update */
void cObjElementFXFiredDebris_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXFiredEntity_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXVehicleAccelerator_Update(cObjElementFX *base, cDebris *debris, float dt);
/* while the ship is seen: debris, sparks, smoke and flames at its vertices by the damage level, the trail of smoke */
void cObjElementFXVehicleDamage_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXVehicleEMI_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXVehicleFire_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXVehicleSputnik_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXSmokeDebris_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXVehicleSpawn_Update(cObjElementFX *base, cDebris *debris, float dt);

/* IsAlive; the ones of FiredDebris, SmokeDebris, Accelerator and Spawn are FXMisc_Alive, the DeathTrail is the
 * code of cObjElementFXDevastatorShoot (Update, IsAlive, Reset) */
byte cObjElementFXFiredEntity_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
byte cObjElementFXVehicleDamage_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
byte cObjElementFXVehicleEMI_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
byte cObjElementFXVehicleFire_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
byte cObjElementFXVehicleSputnik_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);

void cObjElementFXFiredDebris_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXFiredEntity_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXSmokeDebris_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleAccelerator_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleDamage_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleDeathTrail_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleEMI_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleFire_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleSpawn_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleSputnik_Start(cObjElementFX *base, cDebris *debris);

void cObjElementFXFiredDebris_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXFiredEntity_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXSmokeDebris_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleAccelerator_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleDamage_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleEMI_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleFire_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleSpawn_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXVehicleSputnik_Reset(cObjElementFX *base, cDebris *debris);

/* the setters after the start by the vehicle (Entities/Vehicle) */
void cObjElementFXFiredEntity_SetEntity(cObjElementFXFiredEntity *self, cEntity *e);
void cObjElementFXVehicleDamage_SetEntity(cObjElementFXVehicleDamage *self, cEntity *e);
void cObjElementFXVehicleEMI_SetEntity(cObjElementFXVehicleEMI *self, cEntity *e);
void cObjElementFXVehicleFire_SetEntity(cObjElementFXVehicleFire *self, cEntity *e);
void cObjElementFXVehicleSputnik_SetEntity(cObjElementFXVehicleSputnik *self, cEntity *e);
void cObjElementFXVehicleSputnik_SetC0(cObjElementFXVehicleSputnik *self, int v);
void cObjElementFXVehicleSputnik_SetC4(cObjElementFXVehicleSputnik *self, float v);
void cObjElementFXVehicleSpawn_SetEntity(cObjElementFXVehicleSpawn *self, cEntity *e);
void cObjElementFXVehicleAccelerator_SetOwner(cObjElementFXVehicleAccelerator *self, cEntity *e);
void cObjElementFXVehicleAccelerator_SetAccelerator(cObjElementFXVehicleAccelerator *self, void *a);
/* random rays: start delay, time, width and the points of the curves (rva 0x1f48c0, not called in 1.6.1) */
void cObjElementFXVehicleAccelerator_InitRays(cObjElementFXVehicleAccelerator *self);
/* FXVehicle_RenderAttached (rva 0x1f9050, the damage effect drawn with its ship) is in embeddedfxvehicle.h */

#endif
