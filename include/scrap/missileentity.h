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
	void(*Wobble)(cEntity *self, float k);			/* 0xe0 Dumb */
	void(*RotStep)(cEntity *self, float dt);			/* 0xe4 Cloud */
	void(*Fall)(cEntity *self, float dt);			/* 0xe8 */
	void(*Swarm)(cEntity *self, float dt);			/* 0xec vf59 */
	void(*ScaleSwarmSound)(cEntity *self, float unused);		/* 0xf0 */
	void(*LifeTick)(cEntity *self);				/* 0xf4 */
	void(*UpdateVel)(cEntity *self);				/* 0xf8 */
	/* n bees less; with none left the missile hits. The second argument is not used, Tick passes its dt */
	byte(*Hit)(cEntity *self, int n, float unused);		/* 0xfc */
	void(*AddToSwarm)(cEntity *self, int n);			/* 0x100 */
	void(*SetSwarm)(cEntity *self, int n);			/* 0x104 */
	void(*Explode)(cEntity *self, const vec3_t *at);		/* 0x108 */
} cMissileEntityVtbl;
SD_OFFSET(cMissileEntityVtbl, Wobble, 0xe0);
SD_OFFSET(cMissileEntityVtbl, Swarm, 0xec);
SD_OFFSET(cMissileEntityVtbl, Explode, 0x108);
extern const cMissileEntityVtbl cMissileEntity_vftable;
#define CMISSILEENTITY_VFTABLE (&cMissileEntity_vftable.base.base.base)

static inline const cMissileEntityVtbl *Missile_Vtbl(cMissileEntity *e) {
	return (const cMissileEntityVtbl *)e->wl.obj.base.vftable;
}
SD_SIZE(cMissileEntity, 0x7e8);

/* the WithLife entities the missiles touched, by name (4 of 0x80) */
extern char g_MissileHits[4][0x80];

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

void cMissileEntity_dtor(cMissileEntity *self);
/* vf12: the model, its debris system of the bees has the same name */
void cMissileEntity_SetModel(cEntity *elem, const char *file, float sx, float sy, float sz, int flag);
/* vf15: the fire effects DumbMotor, GearMissile, Cloud and EMI (a debris element of the life time) */
void cMissileEntity_SetEmbeddedFX(cEntity *elem, const char *name);
/* vf23 (every slot): the flight, the turn to the target and the hits of the world and the entities */
void cMissileEntity_Tick(cEntity *base, float dt);
/* vf41, vf43: the parent or target and the bees in the network state */
void cMissileEntity_NetWrite(cEntity *elem, float dist2, void *msg, int full);
void cMissileEntity_NetRead(cEntity *base, void *msg, int isNew);
/* vf59: the bees fly around the missile within the radius */
void cMissileEntity_Swarm(cEntity *base, float dt);
/* vf64: n new bees */
void cMissileEntity_AddToSwarm(cEntity *base, int n);
/* vf66: the bees fly to at (beesGather) or die where they are */
void cMissileEntity_Explode(cEntity *elem, const vec3_t *at);
cEntity *cMissileEntity_Delete(cEntity *base, uint32_t flags);
void cMissileEntity_RotStep(cEntity *base, float dt);
void cMissileEntity_Touch(cEntity *base, cEntity *other, const vec3_t *point, const vec3_t *dir);
void cMissileEntity_LifeTick(cEntity *elem);
void cMissileEntity_Wobble(cEntity *base, float k);
void cMissileEntity_Fall(cEntity *base, float dt);
byte cMissileEntity_VarF(cEntity *base, const char *name, double *d, int set);
int cMissileEntity_GetSwarmCount(cMissileEntity *self);
const char *cMissileEntity_GetClassName(cEntity *base);
void cMissileEntity_UpdateOff(cEntity *base);
byte cMissileEntity_VarI(cEntity *base, const char *name, int *i, int set);
byte cMissileEntity_IsType(cEntity *base, const char *type);
/* n bees die (hidden swarm: only counted), or the missile explodes when there are not that many; 1 if it did */
byte cMissileEntity_Hit(cEntity *elem, int n, float unused);
void cMissileEntity_OnDeath(cEntity *base);
void cMissileEntity_SetActive(cEntity *base, byte on);
void cMissileEntity_SetAng(cEntity *base, const vec3_t *ang);
void cMissileEntity_SetBeesHit(cMissileEntity *self, byte v);
void cMissileEntity_SetHiddenSwarm(cMissileEntity *self, byte on);
void cMissileEntity_SetHideMain(cMissileEntity *self, byte on);
void cMissileEntity_SetHitType(cMissileEntity *self, int type);
void cMissileEntity_SetLifeTime(cMissileEntity *self, float t);
void cMissileEntity_SetType(cMissileEntity *self, int type);
void cMissileEntity_SetCount(cMissileEntity *self, int n);
void cMissileEntity_SetNoWorldHit(cMissileEntity *self, byte on);
void cMissileEntity_SetParent(cMissileEntity *self, const char *name);
void cMissileEntity_SetRadius(cMissileEntity *self, float r);
void cMissileEntity_SetRadiusGrow(cMissileEntity *self, float r);
void cMissileEntity_SetSpeed(cMissileEntity *self, float speed);
void cMissileEntity_SetSwDthTime(cMissileEntity *self, float t);
/* more bees are added, fewer go back to the debris system */
void cMissileEntity_SetSwarm(cEntity *base, int n);
void cMissileEntity_SetBeesGather(cMissileEntity *self, byte v);
void cMissileEntity_SetSwarmSpeed(cMissileEntity *self, float speed);
void cMissileEntity_SetTarget(cMissileEntity *self, const char *name);
void cMissileEntity_SetTopRadius(cMissileEntity *self, float r);
void cMissileEntity_SetTurnSpeed(cMissileEntity *self, float speed);
/* the velocity from the angles and the speed */
void cMissileEntity_UpdateVel(cEntity *base);
void cMissileEntity_SetVel(cEntity *base, const vec3_t *vel);
void cMissileEntity_Update(cEntity *base);
/* the volume of the first sound scaled with the bees */
void cMissileEntity_ScaleSwarmSound(cEntity *base, float unused);
byte cMissileEntity_VarS(cEntity *base, const char *name, char **s, int set);
byte cMissileEntity_VarV(cEntity *base, const char *name, double *x, double *y, double *z, int set);

#endif
