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

#define CMISSILEENTITY_SIZE sizeof(cMissileEntity)

#define MISSILE_HIT_NONE 0
#define MISSILE_HIT_HIT 1			/* OnHit when it hits */
#define MISSILE_HIT_INOUT 2			/* OnHit for the entities it went through when it ends */
#define MISSILE_HIT_INSIDE 3

typedef struct cMissileEntity {
	cWithLifeEntity wl;
	/* cMissileEntity */
	byte hideMain;			/* the model hidden */
	byte pad_3e9[3];
	float swDthTime;
	float lifeTime;			/* left */
	float speed;
	float turnSpeed;		/* rad, "TurnSpeed" in degrees */
	char parent[0x80];		/* the entity that fired it */
	char target[0x80];
	char swarmSys[0x80];		/* the debris system of the bees */
	int type;			/* Linear 0, Dumb 1, Cloud 2, Fall 3 */
	float beeTime;			/* the bees get a random push every 0.05 s of it */
	byte beesGather;		/* 1: on Explode the bees fly into the point; Swarm, Inferno and Gear clear it */
	byte beesHit;		/* 1: the bees count in the hits (swarmCount + 1) and die without OnBeeDeath */
	byte pad_586[2];
	int swarmCount;			/* bees */
	byte hiddenSwarm;
	byte pad_58d[3];
	struct cDebris *swarm;		/* the bees, linked at 0x50 */
	float swarmSpeed;
	vec3_t lastPos;			/* the view position of the last Swarm, 0 before the first */
	float radius;
	float topRadius;
	float radiusGrow;
	int netSwarmCount;		/* the count of a network copy */
	char hits[4][0x80];		/* the entities it went through (InOut) */
	int hitType;			/* MISSILE_HIT_ */
	int level;			/* "HideMain": the upgrade level (Inferno: 0 Uranio, 1 Hidrogeno, 2 Napalm; Swarm) */
	byte noWorldHit;
	byte fallOut;			/* "FallOutAtmosphere" */
	byte pad_7be[2];
	cPy_Function onBeeDeath;	/* (missile, x, y, z) */
	cPy_Function onHit;		/* (missile, parent, target or the entity it went through, ...) */
} cMissileEntity;

static inline cMissileEntity *Missile(const void *e) {
	return (cMissileEntity *)e;
}

static inline cEntity *Missile_Ent(cMissileEntity *e) {
	return &e->wl.obj.base;
}

/* the base class (src/game/Entities/WithLifeEntity.c) */
void cWithLifeEntity_dtor(cEntity *self);
byte cWithLifeEntity_VarF(cEntity *self, const char *name, double *d, int set);
void cWithLifeEntity_UpdateOff(cEntity *self);
byte cWithLifeEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte cWithLifeEntity_IsType(cEntity *self, const char *type);
/* the OnDeath functions of the entity and of its list */
void cWithLifeEntity_OnDeath(cEntity *self);
void cWithLifeEntity_SetActive(cEntity *self, byte on);
void cWithLifeEntity_Update(cEntity *self);
byte cWithLifeEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte cWithLifeEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
/* a debris element: the move of dt, and back to the free ones of its system */
void cDebris_Step(struct cDebris *self, float dt);
void cDebrisSys_Release(struct cDebrisSys *sys, struct cDebris *d);

int cMissileEntity_GetSwarmCount(cMissileEntity *self);
void cMissileEntity_SetBeesHit(cMissileEntity *self, byte v);
void cMissileEntity_SetHiddenSwarm(cMissileEntity *self, byte on);
void cMissileEntity_SetHideMain(cMissileEntity *self, byte on);
void cMissileEntity_SetLifeTime(cMissileEntity *self, float t);
void cMissileEntity_SetType(cMissileEntity *self, int type);
void cMissileEntity_SetCount(cMissileEntity *self, int n);
void cMissileEntity_SetNoWorldHit(cMissileEntity *self, byte on);
void cMissileEntity_SetParent(cMissileEntity *self, const char *name);
void cMissileEntity_SetRadius(cMissileEntity *self, float r);
void cMissileEntity_SetRadiusGrow(cMissileEntity *self, float r);
void cMissileEntity_SetSpeed(cMissileEntity *self, float speed);
void cMissileEntity_SetSwDthTime(cMissileEntity *self, float t);
void cMissileEntity_SetBeesGather(cMissileEntity *self, byte v);
void cMissileEntity_SetSwarmSpeed(cMissileEntity *self, float speed);
void cMissileEntity_SetTarget(cMissileEntity *self, const char *name);
void cMissileEntity_SetTopRadius(cMissileEntity *self, float r);
void cMissileEntity_SetTurnSpeed(cMissileEntity *self, float speed);
void cMissileEntity_AddToSwarm(cMissileEntity *self, int n);
void cMissileEntity_SetSwarm(cMissileEntity *self, int n);
byte cMissileEntity_Hit(cMissileEntity *self, int n, float unused);

#endif
