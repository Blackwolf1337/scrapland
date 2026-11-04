/*
 * Entities/WithLifeEntity.cpp of the game (src/game/Entities/WithLifeEntity.c): cWithLifeEntity ("WithLife", 1000
 * bytes), the base class of the ships, the characters and the missiles. A cObjEntity with the life and the fire of the
 * Inferno Napalm, the action controller of its class (cActCtrl, "ActCtrl"), the attacks it took (cVehicleAttack,
 * "Hit_" variables), the arrow of the player selection, the collision sphere and two sound sets ("sdd" for the hits,
 * "sdl" for the life sounds, which fade by their time). The OnDeath and OnDamage functions of the entity and of its
 * entity list run when the life went down (CheckLife, every slot). The inline functions of WithLifeEntity.h (the
 * sphere tests / the mass GetActCtrl) are in src/game/Entities/Char/WalkCharEntity.c, the other
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
	cObjEntity obj;
	float burnAcc;			/* the fire takes a life point each time it falls under 0 */
	int prevLife;			/* "PrevLife", the life of the last CheckLife */
	float mass;			/* "Mass" */
	cActCtrl *actCtrl;		/* "ActCtrl", NULL without a class */
	int life;			/* "Life" */
	byte god;			/* "Invulnerable" */
	byte pad_23d[3];
	Nto1Node burner;		/* "Pyromaniac": who set it on fire, data the entity */
	cString usrName;		/* "Descriptor" */
	Nto1Node targets;		/* head of the links to the entity (the AI targets), target the entity */
	cPy_Function onDeath;		/* "OnDeath" (name, attacker) */
	cPy_Function onDamage;		/* "OnDamage" (name, attacker, damage) */
	float fireAtt;			/* "FireAtenuationFactor", -1 for no fire; a vehicle sets its smooth speed / 16800 */
	float fireDelay;		/* "FireDelayTime": the time per life point */
	float fireRemain;		/* "FireRemainTime" */
	cVehicleAttack attack;		/* "Hit_" */
	cArrowIndicator arrow;
	cPrimitive prim;		/* the collision sphere */
	cSndInterface hitSounds;	/* "sdd" */
	cSndInterface lifeSounds;	/* "sdl", 4 is the fire */
	cString headModel;		/* "HeadModel" */
	int headAnim;			/* "HeadAnimIndex" */
} cWithLifeEntity;

/* cWithLifeEntity::vftable continues the one of cObjEntity; characters, vehicles and missiles continue it */
typedef struct cWithLifeEntityVtbl {
	cObjEntityVtbl base;
	void(*CheckLife)(cEntity *self);
	/* nothing; a character unlinks its listener (the arrow calls it after a new use or view function) */
	void(*UnlinkListen)(cEntity *self);
	void(*OnDeath)(cEntity *self);
	void(*OnDamage)(cEntity *self, int damage);
	void(*SetMass)(cEntity *self, float mass);
	cAction *(*GetAction)(cEntity *self, const char *name);
	cActCtrl *(*GetActCtrl)(cEntity *self);
	void(*SetActCtrl)(cEntity *self, const char *cls);
	byte(*IsEMIed)(cEntity *self);
} cWithLifeEntityVtbl;

/* calls through the slots of cWithLifeEntityVtbl */
static inline void WithLife_CheckLife(void *self) {
	VT(cWithLifeEntityVtbl, self)->CheckLife(self);
}

static inline cActCtrl *WithLife_GetActCtrl(void *self) {
	return VT(cWithLifeEntityVtbl, self)->GetActCtrl(self);
}

static inline cAction *WithLife_GetAction(void *self, const char *name) {
	return VT(cWithLifeEntityVtbl, self)->GetAction(self, name);
}

static inline byte WithLife_IsEMIed(void *self) {
	return VT(cWithLifeEntityVtbl, self)->IsEMIed(self);
}

static inline void WithLife_OnDamage(void *self, int damage) {
	VT(cWithLifeEntityVtbl, self)->OnDamage(self, damage);
}

static inline void WithLife_OnDeath(void *self) {
	VT(cWithLifeEntityVtbl, self)->OnDeath(self);
}

static inline void WithLife_SetActCtrl(void *self, const char *cls) {
	VT(cWithLifeEntityVtbl, self)->SetActCtrl(self, cls);
}

static inline void WithLife_SetMass(void *self, float mass) {
	VT(cWithLifeEntityVtbl, self)->SetMass(self, mass);
}

static inline void WithLife_UnlinkListen(void *self) {
	VT(cWithLifeEntityVtbl, self)->UnlinkListen(self);
}

static inline cWithLifeEntity *WLife(const void *e) {
	return (cWithLifeEntity *)e;
}

/* the variable slots of cActCtrl::vftable */

void cWithLifeEntity_SetPos(cEntity *self, const vec3_t *pos);
/* the PretransformChan of the action class */
int cWithLifeEntity_GetNodeSlot(cEntity *self);
/* the position of the sounds, moved by the SoundDisp of the action class */
vec3_t *cWithLifeEntity_GetSndPos(cEntity *self, vec3_t *out);
float cWithLifeEntity_GetRadius(cEntity *self);
/* (every slot): OnGeneralDamage and OnDamage when the life went down, OnDeath at 0 */
void cWithLifeEntity_CheckLife(cEntity *self);
/* the OnDamage functions of the entity and of its entity list */
void cWithLifeEntity_OnDamage(cEntity *self, int damage);
/* the action of that name of the class, NULL without one */
cAction *cWithLifeEntity_GetAction(cEntity *self, const char *name);
/* the action controller of that class (NULL or "": none); the OnCreate of the class runs for a new one */
void cWithLifeEntity_SetActCtrl(cEntity *self, const char *cls);
/* 0 */
byte cWithLifeEntity_IsEMIed(cEntity *self);
/* nothing (the characters unlink their listeners here) */
void cWithLifeEntity_Vf48(cEntity *self);

#endif
