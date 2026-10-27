/*
 * cMissileEntity ("Missile"): the shots of the weapons, a cWithLifeEntity (1000 bytes) that flies with its speed
 * along its angles ("Linear", "Dumb", "Cloud" or "Fall") until its life time ran out or it hit something. A
 * swarm missile carries bees, debris of the system swarmSys that fly around it. src/game/Entities/MissileEntity.c;
 * the original code follows the items, its source file has no CHECK. Not decompiled yet: the update of the
 * flight (vf23), the swarm functions vf59, vf64 and vf66 (Explode), the network vf41/vf43 and SetFX/SetModel.
 */
#ifndef SCRAP_MISSILEENTITY_H
#define SCRAP_MISSILEENTITY_H

#include "scrap/objentity.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapmodel.h"
#include "scrap/timer.h"
#include "scrap/world.h"

#define CMISSILEENTITY_VFTABLE 0x3b2b24
#define CMISSILEENTITY_SIZE 0x7e8

#define MISSILE_HIT_NONE 0
#define MISSILE_HIT_HIT 1			/* OnHit when it hits */
#define MISSILE_HIT_INOUT 2			/* OnHit for the entities it went through when it ends */
#define MISSILE_HIT_INSIDE 3

typedef struct cMissileEntity {
	cEntity base;			/* 0x000 */
	byte pad_7c[0xac - 0x7c];
	vec3_t ang;			/* 0x0ac */
	CGameModel model;		/* 0x0b8 */
	vec3_t vel;			/* 0x0d4 */
	vec3_t rotVel;			/* 0x0e0 */
	byte pad_ec[0x110 - 0xec];
	cTimer timer;			/* 0x110 the time of the entity */
	byte pad_138[0x140 - 0x138];
	byte *obj140;			/* 0x140 the floats at 0x4 and 0x8 are set to the one at 0xc */
	byte weapons[0x238 - 0x144];	/* 0x144 PtrArray of the weapons (and more) */
	int life;			/* 0x238 */
	byte pad_23c[0x28c - 0x23c];
	float f28c;			/* 0x28c -1 */
	byte pad_290[0x3e8 - 0x290];
	byte hideMain;			/* 0x3e8 the model hidden */
	byte pad_3e9[3];
	float swDthTime;		/* 0x3ec */
	float lifeTime;			/* 0x3f0 left */
	float speed;			/* 0x3f4 */
	float turnSpeed;		/* 0x3f8 rad, "TurnSpeed" in degrees */
	char parent[0x80];		/* 0x3fc the entity that fired it */
	char target[0x80];		/* 0x47c */
	char swarmSys[0x80];		/* 0x4fc the debris system of the bees */
	int type;			/* 0x57c Linear 0, Dumb 1, Cloud 2, Fall 3 */
	int i580;			/* 0x580 */
	byte flag584;			/* 0x584 */
	byte flag585;			/* 0x585 */
	byte pad_586[2];
	int swarmCount;			/* 0x588 bees */
	byte hiddenSwarm;		/* 0x58c */
	byte pad_58d[3];
	struct cDebris *swarm;		/* 0x590 the bees, linked at 0x50 */
	float swarmSpeed;		/* 0x594 */
	vec3_t v598;			/* 0x598 */
	float radius;			/* 0x5a4 */
	float topRadius;		/* 0x5a8 */
	float radiusGrow;		/* 0x5ac */
	int netSwarmCount;		/* 0x5b0 the count of a network copy */
	char hits[4][0x80];		/* 0x5b4 the entities it went through (InOut) */
	int hitType;			/* 0x7b4 MISSILE_HIT_ */
	int hideMainVar;		/* 0x7b8 the variable "HideMain" */
	byte noWorldHit;		/* 0x7bc */
	byte fallOut;			/* 0x7bd "FallOutAtmosphere" */
	byte pad_7be[2];
	cPy_Function onBeeDeath;	/* 0x7c0 (missile, x, y, z) */
	cPy_Function onHit;		/* 0x7d4 (missile, parent, target or the entity it went through, ...) */
} cMissileEntity;
SD_OFFSET(cMissileEntity, ang, 0xac);
SD_OFFSET(cMissileEntity, timer, 0x110);
SD_OFFSET(cMissileEntity, life, 0x238);
SD_OFFSET(cMissileEntity, hideMain, 0x3e8);
SD_OFFSET(cMissileEntity, parent, 0x3fc);
SD_OFFSET(cMissileEntity, type, 0x57c);
SD_OFFSET(cMissileEntity, swarmCount, 0x588);
SD_OFFSET(cMissileEntity, hits, 0x5b4);
SD_OFFSET(cMissileEntity, hitType, 0x7b4);
SD_OFFSET(cMissileEntity, onHit, 0x7d4);
SD_SIZE(cMissileEntity, CMISSILEENTITY_SIZE);

/* the WithLife entities the missiles touched, by name (4 of 0x80) */
#define g_MissileHits ((char (*)[0x80])Game_Ptr(0x4476b8))

/* the base classes (not decompiled yet) */
void __fastcall cWithLifeEntity_dtor(cEntity *self);
byte __thiscall cWithLifeEntity_VarF(cEntity *self, const char *name, double *d, int set);
void __thiscall cWithLifeEntity_UpdateOff(cEntity *self);
byte __thiscall cWithLifeEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte __thiscall cWithLifeEntity_IsType(cEntity *self, const char *type);
/* the OnDeath functions of the entity and of its list */
void __thiscall cWithLifeEntity_OnDeath(cEntity *self);
void __thiscall cWithLifeEntity_SetActive(cEntity *self, byte on);
void __thiscall cWithLifeEntity_Update(cEntity *self);
byte __thiscall cWithLifeEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte __thiscall cWithLifeEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
/* a debris element: the move of dt, and back to the free ones of its system */
void __thiscall cDebris_Step(struct cDebris *self, float dt);
void __thiscall cDebrisSys_Release(struct cDebrisSys *sys, struct cDebris *d);

void __fastcall cMissileEntity_dtor(cMissileEntity *self);
cMissileEntity *__thiscall cMissileEntity_Delete(cMissileEntity *self, uint32_t flags);
void __thiscall cMissileEntity_RotStep(cMissileEntity *self, float dt);
void __thiscall cMissileEntity_Touch(cMissileEntity *self, cEntity *other, const vec3_t *point, const vec3_t *dir);
void __thiscall cMissileEntity_LifeTick(cMissileEntity *self);
void __thiscall cMissileEntity_Wobble(cMissileEntity *self, float k);
void __thiscall cMissileEntity_Fall(cMissileEntity *self, float dt);
byte __thiscall cMissileEntity_VarF(cMissileEntity *self, const char *name, double *d, int set);
int __fastcall cMissileEntity_GetSwarmCount(cMissileEntity *self);
const char *__thiscall cMissileEntity_GetClassName(cMissileEntity *self);
void __thiscall cMissileEntity_UpdateOff(cMissileEntity *self);
byte __thiscall cMissileEntity_VarI(cMissileEntity *self, const char *name, int *i, int set);
byte __thiscall cMissileEntity_IsType(cMissileEntity *self, const char *type);
/* n bees die (hidden swarm: only counted), or the missile explodes when there are not that many; 1 if it did */
byte __thiscall cMissileEntity_Hit(cMissileEntity *self, int n, int unused);
void __thiscall cMissileEntity_OnDeath(cMissileEntity *self);
void __thiscall cMissileEntity_SetActive(cMissileEntity *self, byte on);
void __thiscall cMissileEntity_SetAng(cMissileEntity *self, const vec3_t *ang);
void __thiscall cMissileEntity_SetFlag585(cMissileEntity *self, byte v);
void __thiscall cMissileEntity_SetHiddenSwarm(cMissileEntity *self, byte on);
void __thiscall cMissileEntity_SetHideMain(cMissileEntity *self, byte on);
void __thiscall cMissileEntity_SetHitType(cMissileEntity *self, int type);
void __thiscall cMissileEntity_SetLifeTime(cMissileEntity *self, float t);
void __thiscall cMissileEntity_SetType(cMissileEntity *self, int type);
void __thiscall cMissileEntity_SetCount(cMissileEntity *self, int n);
void __thiscall cMissileEntity_SetNoWorldHit(cMissileEntity *self, byte on);
void __thiscall cMissileEntity_SetParent(cMissileEntity *self, const char *name);
void __thiscall cMissileEntity_SetRadius(cMissileEntity *self, float r);
void __thiscall cMissileEntity_SetRadiusGrow(cMissileEntity *self, float r);
void __thiscall cMissileEntity_SetSpeed(cMissileEntity *self, float speed);
void __thiscall cMissileEntity_SetSwDthTime(cMissileEntity *self, float t);
/* more bees are added, fewer go back to the debris system */
void __thiscall cMissileEntity_SetSwarm(cMissileEntity *self, int n);
void __thiscall cMissileEntity_SetFlag584(cMissileEntity *self, byte v);
void __thiscall cMissileEntity_SetSwarmSpeed(cMissileEntity *self, float speed);
void __thiscall cMissileEntity_SetTarget(cMissileEntity *self, const char *name);
void __thiscall cMissileEntity_SetTopRadius(cMissileEntity *self, float r);
void __thiscall cMissileEntity_SetTurnSpeed(cMissileEntity *self, float speed);
/* the velocity from the angles and the speed */
void __thiscall cMissileEntity_UpdateVel(cMissileEntity *self);
void __thiscall cMissileEntity_SetVel(cMissileEntity *self, const vec3_t *vel);
void __thiscall cMissileEntity_Update(cMissileEntity *self);
/* the values of the first weapon scaled with the bees */
void __thiscall cMissileEntity_ScaleWeapon(cMissileEntity *self, int unused);
byte __thiscall cMissileEntity_VarS(cMissileEntity *self, const char *name, char **s, int set);
byte __thiscall cMissileEntity_VarV(cMissileEntity *self, const char *name, double *x, double *y, double *z, int set);

#endif
