/*
 * cMissileEntity ("Missile"): the shots of the weapons, a cWithLifeEntity (1000 bytes) that flies with its speed
 * along its angles ("Linear", "Dumb", "Cloud" or "Fall") until its life time ran out or it hit something. A
 * swarm missile carries bees, debris of the system swarmSys that fly around it. src/game/Entities/MissileEntity.c;
 * the original code follows the items, its source file has no CHECK.
 */
#ifndef SCRAP_MISSILEENTITY_H
#define SCRAP_MISSILEENTITY_H

#include "scrap/objentity.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapmodel.h"
#include "scrap/timer.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"

#define CMISSILEENTITY_VFTABLE GAME_VFTABLE(cMissileEntity_vftable, 0x3b2b24)
#define CMISSILEENTITY_SIZE 0x7e8

#define MISSILE_HIT_NONE 0
#define MISSILE_HIT_HIT 1			/* OnHit when it hits */
#define MISSILE_HIT_INOUT 2			/* OnHit for the entities it went through when it ends */
#define MISSILE_HIT_INSIDE 3

typedef struct cMissileEntity {
	cWithLifeEntity wl;		/* 0x000 */
	/* cMissileEntity */
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
	float beeTime;			/* 0x580 the bees get a random push every 0.05 s of it */
	byte beesGather;		/* 0x584 1: on Explode the bees fly into the point; Swarm, Inferno and Gear clear it */
	byte beesHit;		/* 0x585 1: the bees count in the hits (swarmCount + 1) and die without OnBeeDeath */
	byte pad_586[2];
	int swarmCount;			/* 0x588 bees */
	byte hiddenSwarm;		/* 0x58c */
	byte pad_58d[3];
	struct cDebris *swarm;		/* 0x590 the bees, linked at 0x50 */
	float swarmSpeed;		/* 0x594 */
	vec3_t lastPos;			/* 0x598 the view position of the last Swarm, 0 before the first */
	float radius;			/* 0x5a4 */
	float topRadius;		/* 0x5a8 */
	float radiusGrow;		/* 0x5ac */
	int netSwarmCount;		/* 0x5b0 the count of a network copy */
	char hits[4][0x80];		/* 0x5b4 the entities it went through (InOut) */
	int hitType;			/* 0x7b4 MISSILE_HIT_ */
	int level;			/* 0x7b8 "HideMain": the upgrade level (Inferno: 0 Uranio, 1 Hidrogeno, 2 Napalm; Swarm) */
	byte noWorldHit;		/* 0x7bc */
	byte fallOut;			/* 0x7bd "FallOutAtmosphere" */
	byte pad_7be[2];
	cPy_Function onBeeDeath;	/* 0x7c0 (missile, x, y, z) */
	cPy_Function onHit;		/* 0x7d4 (missile, parent, target or the entity it went through, ...) */
} cMissileEntity;
SD_OFFSET(cMissileEntity, hideMain, 0x3e8);
SD_OFFSET(cMissileEntity, parent, 0x3fc);
SD_OFFSET(cMissileEntity, type, 0x57c);
SD_OFFSET(cMissileEntity, swarmCount, 0x588);
SD_OFFSET(cMissileEntity, hits, 0x5b4);
SD_OFFSET(cMissileEntity, hitType, 0x7b4);
SD_OFFSET(cMissileEntity, onHit, 0x7d4);

static inline cMissileEntity *Missile(const void *e) {
	return (cMissileEntity *)e;
}

static inline cEntity *Missile_Ent(cMissileEntity *e) {
	return &e->wl.obj.base;
}

/* cMissileEntity::vftable: the slots of the missile after the ones of cWithLifeEntity */
typedef struct cMissileEntityVtbl {
	cWithLifeEntityVtbl base;
	void(__thiscall *Wobble)(cMissileEntity *self, float k);			/* 0xe0 Dumb */
	void(__thiscall *RotStep)(cMissileEntity *self, float dt);			/* 0xe4 Cloud */
	void(__thiscall *Fall)(cMissileEntity *self, float dt);			/* 0xe8 */
	void(__thiscall *Swarm)(cMissileEntity *self, float dt);			/* 0xec vf59 */
	void(__thiscall *ScaleWeapon)(cMissileEntity *self, float unused);		/* 0xf0 */
	void(__thiscall *LifeTick)(cMissileEntity *self);				/* 0xf4 */
	void(__thiscall *UpdateVel)(cMissileEntity *self);				/* 0xf8 */
	/* n bees less; with none left the missile hits. The second argument is not used, Tick passes its dt */
	byte(__thiscall *Hit)(cMissileEntity *self, int n, float unused);		/* 0xfc */
	void(__thiscall *AddToSwarm)(cMissileEntity *self, int n);			/* 0x100 */
	void(__thiscall *SetSwarm)(cMissileEntity *self, int n);			/* 0x104 */
	void(__thiscall *Explode)(cMissileEntity *self, const vec3_t *at);		/* 0x108 */
} cMissileEntityVtbl;
SD_OFFSET(cMissileEntityVtbl, Wobble, 0xe0);
SD_OFFSET(cMissileEntityVtbl, Swarm, 0xec);
SD_OFFSET(cMissileEntityVtbl, Explode, 0x108);

static inline const cMissileEntityVtbl *Missile_Vtbl(cMissileEntity *e) {
	return (const cMissileEntityVtbl *)e->wl.obj.base.vftable;
}
SD_SIZE(cMissileEntity, CMISSILEENTITY_SIZE);

/* the WithLife entities the missiles touched, by name (4 of 0x80) */
#define g_MissileHits (GAME_ARRAY(char[0x80], g_MissileHits, 0x4476b8))

/* the base class (src/game/Entities/WithLifeEntity.c) */
void __fastcall cWithLifeEntity_dtor(cEntity *self);
byte __thiscall cWithLifeEntity_VarF(cEntity *self, const char *name, double *d, int set);
void __thiscall cWithLifeEntity_UpdateOff(cEntity *self);
byte __thiscall cWithLifeEntity_VarI(cEntity *self, const char *name, int *i, int set);
int __thiscall cWithLifeEntity_IsType(cEntity *self, const char *type);
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
/* vf12: the model, its debris system of the bees has the same name */
void __thiscall cMissileEntity_SetModel(cMissileEntity *self, const char *file, float sx, float sy, float sz, int flag);
/* vf15: the fire effects DumbMotor, GearMissile, Cloud and EMI (a debris element of the life time) */
void __thiscall cMissileEntity_SetEmbeddedFX(cMissileEntity *self, const char *name);
/* vf23 (every slot): the flight, the turn to the target and the hits of the world and the entities */
void __thiscall cMissileEntity_Tick(cMissileEntity *self, float dt);
/* vf41, vf43: the parent or target and the bees in the network state */
void __thiscall cMissileEntity_NetWrite(cMissileEntity *self, float dist2, void *msg, int full);
void __thiscall cMissileEntity_NetRead(cMissileEntity *self, void *msg, int isNew);
/* vf59: the bees fly around the missile within the radius */
void __thiscall cMissileEntity_Swarm(cMissileEntity *self, float dt);
/* vf64: n new bees */
void __thiscall cMissileEntity_AddToSwarm(cMissileEntity *self, int n);
/* vf66: the bees fly to at (beesGather) or die where they are */
void __thiscall cMissileEntity_Explode(cMissileEntity *self, const vec3_t *at);
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
/* the volume of the first sound scaled with the bees */
void __thiscall cMissileEntity_ScaleWeapon(cMissileEntity *self, int unused);
byte __thiscall cMissileEntity_VarS(cMissileEntity *self, const char *name, char **s, int set);
byte __thiscall cMissileEntity_VarV(cMissileEntity *self, const char *name, double *x, double *y, double *z, int set);

#endif
