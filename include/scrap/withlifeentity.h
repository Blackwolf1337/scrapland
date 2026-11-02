/*
 * Entities/WithLifeEntity.cpp of the game (src/game/Entities/WithLifeEntity.c): cWithLifeEntity ("WithLife", 1000
 * bytes), the base class of the ships, the characters and the missiles. A cObjEntity with the life and the fire of the
 * Inferno Napalm, the action controller of its class (cActCtrl, "ActCtrl"), the attacks it took (cVehicleAttack,
 * "Hit_" variables), the arrow of the player selection, the collision sphere and two sound sets ("sdd" for the hits,
 * "sdl" for the life sounds, which fade by their time). The OnDeath and OnDamage functions of the entity and of its
 * entity list run when the life went down (CheckLife, every slot). The inline functions of WithLifeEntity.h (the
 * sphere tests vf09/vf10, the mass vf51, GetActCtrl vf53) are in src/game/Entities/Char/WalkCharEntity.c, the other
 * prototypes of the class in the headers of their callers (world.h, missileentity.h, vehicleentity.h, net.h,
 * walkcharentity.h, weaps2.h).
 */
#ifndef SCRAP_WITHLIFEENTITY_H
#define SCRAP_WITHLIFEENTITY_H

#include "scrap/actions.h"
#include "scrap/arrowindicator.h"
#include "scrap/cstring.h"
#include "scrap/objentity.h"
#include "scrap/primitive.h"
#include "scrap/pyfunction.h"
#include "scrap/sound.h"
#include "scrap/vehicleattack.h"

#define CWITHLIFEENTITY_SIZE sizeof(cWithLifeEntity)

typedef struct cWithLifeEntity {
	cObjEntity obj;			/* 0x000 */
	float burnAcc;			/* 0x228 the fire takes a life point each time it falls under 0 */
	int prevLife;			/* 0x22c "PrevLife", the life of the last CheckLife */
	float mass;			/* 0x230 "Mass" */
	cActCtrl *actCtrl;		/* 0x234 "ActCtrl", NULL without a class */
	int life;			/* 0x238 "Life" */
	byte god;			/* 0x23c "Invulnerable" */
	byte pad_23d[3];
	Nto1Node burner;		/* 0x240 "Pyromaniac": who set it on fire, data the entity */
	cString usrName;		/* 0x250 "Descriptor" */
	Nto1Node targets;		/* 0x254 head of the links to the entity (the AI targets), target the entity */
	cPy_Function onDeath;		/* 0x264 "OnDeath" (name, attacker) */
	cPy_Function onDamage;		/* 0x278 "OnDamage" (name, attacker, damage) */
	float fireAtt;			/* 0x28c "FireAtenuationFactor", -1 for no fire; a vehicle sets its smooth speed / 16800 */
	float fireDelay;		/* 0x290 "FireDelayTime": the time per life point */
	float fireRemain;		/* 0x294 "FireRemainTime" */
	cVehicleAttack attack;		/* 0x298 "Hit_" */
	cArrowIndicator arrow;		/* 0x314 */
	cPrimitive prim;		/* 0x39c the collision sphere */
	cSndInterface hitSounds;	/* 0x3a8 "sdd" */
	cSndInterface lifeSounds;	/* 0x3c4 "sdl", 4 is the fire */
	cString headModel;		/* 0x3e0 "HeadModel" */
	int headAnim;			/* 0x3e4 "HeadAnimIndex" */
} cWithLifeEntity;

/* cWithLifeEntity::vftable continues the one of cObjEntity; characters, vehicles and missiles continue it */
typedef struct cWithLifeEntityVtbl {
	cObjEntityVtbl base;
	void(*CheckLife)(cEntity *self);						/* 0xbc vf47 */
	/* nothing; a character unlinks its listener (the arrow calls it after a new use or view function) */
	void(*UnlinkListen)(cEntity *self);						/* 0xc0 vf48 */
	void(*OnDeath)(cEntity *self);						/* 0xc4 vf49 */
	void(*OnDamage)(cEntity *self, int damage);				/* 0xc8 vf50 */
	void(*SetMass)(cEntity *self, float mass);					/* 0xcc vf51 */
	cAction *(*GetAction)(cEntity *self, const char *name);			/* 0xd0 vf52 */
	cActCtrl *(*GetActCtrl)(cEntity *self);					/* 0xd4 vf53 */
	void(*SetActCtrl)(cEntity *self, const char *cls);				/* 0xd8 vf54 */
	byte(*IsEMIed)(cEntity *self);						/* 0xdc vf55 */
} cWithLifeEntityVtbl;

static inline cWithLifeEntity *WLife(const void *e) {
	return (cWithLifeEntity *)e;
}

static inline const cWithLifeEntityVtbl *WithLife_Vtbl(const cEntity *e) {
	return (const cWithLifeEntityVtbl *)e->vftable;
}

/* the variable slots of cActCtrl::vftable */

static inline const cActCtrlVarVtbl *ActCtrl_Vtbl(const cActCtrl *c) {
	return (const cActCtrlVarVtbl *)c->vftable;
}

void cWithLifeEntity_SetPos(cEntity *self, const vec3_t *pos);
/* vf25: the PretransformChan of the action class */
int cWithLifeEntity_GetNodeSlot(cEntity *self);
/* vf31: the position of the sounds, moved by the SoundDisp of the action class */
vec3_t *cWithLifeEntity_GetSndPos(cEntity *self, vec3_t *out);
float cWithLifeEntity_GetRadius(cEntity *self);
/* vf47 (every slot): OnGeneralDamage and OnDamage when the life went down, OnDeath at 0 */
void cWithLifeEntity_CheckLife(cEntity *self);
/* vf50: the OnDamage functions of the entity and of its entity list */
void cWithLifeEntity_OnDamage(cEntity *self, int damage);
/* vf52: the action of that name of the class, NULL without one */
cAction *cWithLifeEntity_GetAction(cEntity *self, const char *name);
/* vf54: the action controller of that class (NULL or "": none); the OnCreate of the class runs for a new one */
void cWithLifeEntity_SetActCtrl(cEntity *self, const char *cls);
/* vf55: 0 */
byte cWithLifeEntity_IsEMIed(cEntity *self);
/* vf48: nothing (the characters unlink their listeners here) */
void cWithLifeEntity_Vf48(cEntity *self);

#endif
