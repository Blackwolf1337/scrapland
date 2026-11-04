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
	int motor;			/* FXVeh_Motor */
	int side;			/* 0 or 1 */
} FXVExhaust;

/* the debris fired out of a ship and its smoke: particles emitted along the way of the debris */
typedef struct cObjElementFXFiredDebris {
	cObjElementFX base;
	CGameParticles particles;	/* on Dummy01 */
	float time;			/* since the last emission */
	float interval;
	byte first;			/* the next Update takes the position as the last one */
	byte pad_bd[3];
	vec3_t last;			/* the position of the last emission */
	float minDist;			/* between two emissions */
	float scale;
	float spread;
	byte on;
	byte pad_d9[3];
} cObjElementFXFiredDebris;

typedef cObjElementFXFiredDebris cObjElementFXSmokeDebris;

/* the trail of a dying vehicle: the Devastator shot with 0x1e trail points */
typedef cObjElementFXDevastatorShoot cObjElementFXVehicleDeathTrail;

/* the smoke of an entity on fire */
typedef struct cObjElementFXFiredEntity {
	cObjElementFX base;
	cEntity *entity;
	byte posValid;		/* pos holds the position of the last frame */
	byte pad_09[3];
	float radius;			/* of its model */
	vec3_t pos;			/* of its model */
	CGameParticles particles;	/* on Dummy01 */
	float time;
	byte on;
	byte pad_d1[3];
} cObjElementFXFiredEntity;

/* the two inner control points of a ray of the accelerator, offsets that move at random */
typedef struct FXVAccelCtrl {
	vec3_t a;
	vec3_t b;
} FXVAccelCtrl;

/* the rays of the accelerator: Bezier curves from a vertex of the ship */
typedef struct cObjElementFXVehicleAccelerator {
	cObjElementFX base;
	cEntity *owner;
	void *accelerator;
	CGameParticles particles;	/* on Dummy01, two per ray */
	float fade[FXVACCEL_TRAILS];	/* the time the ray has left */
	float delay[FXVACCEL_TRAILS];	/* before the ray starts */
	CGameTrail trails[FXVACCEL_TRAILS];
	float width[FXVACCEL_TRAILS];
	vec3_t from[FXVACCEL_TRAILS];	/* in the space of the ship */
	FXVAccelCtrl ctrl[FXVACCEL_TRAILS];
	vec3_t to[FXVACCEL_TRAILS];	/* in the space of the ship */
	byte on;
	byte pad_4bd[3];
} cObjElementFXVehicleAccelerator;

/* the damage of a ship: smoke and sparks at the motors while the life is low */
typedef struct cObjElementFXVehicleDamage {
	cObjElementFX base;
	cEntity *entity;
	byte active;			/* the life is checked */
	byte pad_09[3];
	int life;
	int lifePct;
	CGameModel *model;		/* the model of the debris */
	byte seen;			/* the ship was seen in the last Update, pos is valid */
	byte pad_19[3];
	CGameParticles parts3;		/* on Dummy03: the smoke at the vertices */
	CGameParticles parts2;		/* on Dummy02: debris, sparks and flames */
	float level;			/* follows lifePct / FXSetOnFireLifePercent: 0 burning, 1 out */
	float emitTime;			/* since the last emission at the vertices */
	CGameParticles parts1;		/* on Dummy01: the trail of smoke */
	vec3_t pos;			/* of the ship model at the last puff of the trail */
	byte first;
	byte pad_241[3];
	float trailTime;		/* since the last puff of the trail */
	int numExhausts;
	FXVExhaust exhausts[FXV_EXHAUSTS];
	byte on;
	byte pad_2cd[3];
} cObjElementFXVehicleDamage;

/* the electromagnetic interference of a ship: rays that jump along the vertices of the ship or of one of its motors,
 * drawn as Bezier trails through their last points (0x82e4 bytes) */
#define FXVEMI_POINTS 64
#define FXVEMI_MARKS 0x200
typedef struct cObjElementFXVehicleEMI {
	cObjElementFX base;
	cEntity *entity;
	CGameParticles particles;	/* on Dummy01, three per ray */
	float stepTime;			/* a step of the rays every 0.02 */
	float time;
	int numTrails;			/* 16 */
	float offset;			/* 22, the points out along the normal */
	int numVerts;			/* 0x1000, the vertices the marks hold */
	int maxPts;			/* 0x40 */
	vec3_t dir;			/* of the last step */
	int numExhausts;
	FXVExhaust exhausts[FXV_EXHAUSTS];
	int part[FXVEMI_TRAILS];	/* the exhaust of the ray, -1 the whole ship */
	float rayTime[FXVEMI_TRAILS];
	vec3_t pts[FXVEMI_TRAILS][FXVEMI_POINTS];	/* a ring, in the space of the model */
	int vidx[FXVEMI_TRAILS][FXVEMI_POINTS];		/* the vertex of each point */
	int head[FXVEMI_TRAILS];
	int tail[FXVEMI_TRAILS];
	int count[FXVEMI_TRAILS];
	uint16_t marks[FXVEMI_TRAILS][FXVEMI_MARKS];	/* a bit per vertex in the ring */
	CGameTrail trails[FXVEMI_TRAILS];
	byte on;
	byte pad_82e1[3];
} cObjElementFXVehicleEMI;

/* the fire at the motors of a ship */
typedef struct cObjElementFXVehicleFire {
	cObjElementFX base;
	cEntity *entity;
	CGameParticles particles;	/* on Dummy01 */
	CGameParticles particlesOut;	/* on Dummy02, the smoke */
	float time;
	float emitTime;			/* since the last emission */
	byte pad_170[4];
	int numExhausts;
	FXVExhaust exhausts[FXV_EXHAUSTS];
	byte on;
	byte pad_1f9[3];
} cObjElementFXVehicleFire;

/* the spawn of a ship: rays from vertices of its model, drawn as trails */
#define FXVSPAWN_RAYS 75
typedef struct cObjElementFXVehicleSpawn {
	cObjElementFX base;
	cEntity *entity;
	CGameParticles particles;	/* on Dummy01 */
	float stepTime;			/* of the frame */
	float time;
	vec3_t rayBase[FXVSPAWN_RAYS];
	vec3_t rayDir[FXVSPAWN_RAYS];
	float rayLen[FXVSPAWN_RAYS];
	float raySpeed[FXVSPAWN_RAYS];
	float rayAcc[FXVSPAWN_RAYS];
	float rayMax[FXVSPAWN_RAYS];
	CGameTrail trails[FXVSPAWN_TRAILS];
	int rayVertex[FXVSPAWN_RAYS];	/* the vertex of the model */
	int numRays;
	byte on;
	byte pad_dd5[3];
} cObjElementFXVehicleSpawn;

/* the Sputnik of a ship: rays from the Sputnik to points of the ship drawn as trails, a glow at the Sputnik
 * (0x1720 bytes) */
typedef struct cObjElementFXVehicleSputnik {
	cObjElementFX base;
	cEntity *entity;
	CGameParticles particles;	/* on Dummy01 */
	float time;
	float time2;
	int phase;			/* 1 */
	float duration;			/* 0.8 */
	byte active;			/* the active of the entity at the start, a change ends the effect */
	byte pad_c9[3];
	int numExhausts;
	FXVExhaust exhausts[FXV_EXHAUSTS];
	int sputnikNode;		/* "Bip Spootnick MASTER" of the entity "Sputnik", -1 none */
	CGameTrail trails[FXVSPUTNIK_TRAILS];	/* 15 rays per trail */
	vec3_t rayPos[FXVSPUTNIK_RANDS];	/* the point of the ship */
	float rayLife[FXVSPUTNIK_RANDS];	/* -1 */
	float rayLifeMax[FXVSPUTNIK_RANDS];
	float rayDelay[FXVSPUTNIK_RANDS];	/* 0.15..0.55 before the next ray */
	float rayWidth[FXVSPUTNIK_RANDS];
	byte on;
	byte pad_171d[3];
} cObjElementFXVehicleSputnik;

/* the constructors (return self), destructors and the Free slots */
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

/* the setters after the start by the vehicle (Entities/Vehicle) */
void cObjElementFXFiredEntity_SetEntity(cObjElementFXFiredEntity *self, cEntity *e);
void cObjElementFXVehicleDamage_SetEntity(cObjElementFXVehicleDamage *self, cEntity *e);
void cObjElementFXVehicleEMI_SetEntity(cObjElementFXVehicleEMI *self, cEntity *e);
void cObjElementFXVehicleFire_SetEntity(cObjElementFXVehicleFire *self, cEntity *e);
void cObjElementFXVehicleSputnik_SetEntity(cObjElementFXVehicleSputnik *self, cEntity *e);
void cObjElementFXVehicleSputnik_SetC0(cObjElementFXVehicleSputnik *self, int v);
void cObjElementFXVehicleSputnik_SetC4(cObjElementFXVehicleSputnik *self, float v);
void cObjElementFXVehicleSpawn_SetEntity(cObjElementFXVehicleSpawn *self, cEntity *e);
/* FXVehicle_RenderAttached (the damage effect drawn with its ship) is in embeddedfxvehicle.h */

#endif
