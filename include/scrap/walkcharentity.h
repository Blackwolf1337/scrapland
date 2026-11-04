/*
 * Entities/Char/WalkCharEntity.cpp of the game (src/game/Entities/Char/WalkCharEntity.c): the walking character
 * (cWalkCharEntity, "WalkChar", 0x574 bytes, a cWithLifeEntity). Every slot (Tick) it turns the view by its control
 * (cWalkCharControl at 0x4ec, the input of a player or the AI), plays the walk, run, strafe and relax actions of its
 * class (the cActCtrl at 0x234 with the cActClass at +8), moves by the velocity with the gravity and the collisions
 * of the map and the other entities, and keeps the entity it selects (the arrow of the player) and the one it
 * listens to. The inline virtual functions of WithLifeEntity.h and three of ObjEntity.h sit in the same part of the
 * exe. The constructor is declared in include/scrap/world.h, some of the functions in the headers of their callers
 * (chartype.h, ai.h, scoreritems.h).
 */
#ifndef SCRAP_WALKCHARENTITY_H
#define SCRAP_WALKCHARENTITY_H

#include "scrap/gvar.h"
#include "scrap/scrapcollision.h"
#include "scrap/walkcharcontrol.h"
#include "scrap/withlifeentity.h"

#define CWALKCHARENTITY_SIZE sizeof(cWalkCharEntity)

typedef struct cWalkCharEntity {
	cWithLifeEntity wl;
	/* cWalkCharEntity */
	Nto1Node collide;		/* "CollideEntity": the entity it ran into */
	float moved2;			/* the square of the distance of the last walk step */
	float headPitch;		/* of the camera node (or the render angles) */
	int camNode;			/* DC_Camera of the model */
	CSphereBox sphereBox;		/* the cache of the slides */
	int ownerSlot;			/* "UsrControl": the player slot, -1 for an AI */
	vec3_t view;			/* "ViewAng" */
	vec3_t viewVel;
	vec3_t slotView;		/* of the last slot */
	vec3_t renderView;		/* between two slots */
	float relaxTime;		/* "TimeToExtraRelax" */
	float floorDist;		/* "FloorDist" */
	float slideTime;		/* "SlidingAndFallingTime" */
	byte falling;			/* "isFalling" */
	byte walking;			/* "isWalking" */
	byte pad_466[2];
	float displAng;			/* "DisplAng": the walk direction against the view */
	float unused_46c;
	float lastMove;			/* the move of the walk while it stops */
	float lastDisplAng;
	float walkTime;			/* the stop is kept that long */
	float move;			/* 0..1 from the control */
	vec3_t fallVel;			/* the velocity of the fall */
	byte disableControl;		/* "DisableControl" */
	byte pad_48d[3];
	cAction *walkEnd;		/* the end action of the walk */
	float yModelDisp;		/* "YModelDisp" */
	float energyUsed;		/* 1 minus the weapon energy */
	cPy_Function onEnergyFull;	/* "OnEnergyBarFull" */
	float energyBarDecr;		/* "EnergyBarDecr": the energy an action needs */
	float energyTime;		/* "EnergyBarConstantDecr" */
	float hitFlash;			/* the seconds left of the flash of a hit */
	Nto1Node reposHead;		/* the point of the character group it was placed at (cAICGPoint) */
	int money;			/* "Money" */
	int moneyReward;		/* "MoneyReward" */
	byte viewLock;			/* the actions set the view angles */
	byte pad_4d5[3];
	float viewLockF;		/* the blend of the view lock */
	byte pushOn;			/* a push of an AI */
	byte pad_4dd[3];
	vec3_t push;
	cWalkCharControl control;
	Nto1Node listen;		/* "SelectedEntity": the one it listens to */
	byte listenOn;			/* "AutoSelect" */
	byte pad_531[3];
	Nto1Node victim;		/* "AutoSelectedEntity": the selection of the arrow */
	cPy_Function outSelect;		/* "OutSelectFunc" */
	cPy_Function abortSelect;	/* "AbortSelectFunc" */
	float abortTime;		/* the selection is kept that long without a change */
	int abortOpt;			/* the option of the arrow at the last slot */
} cWalkCharEntity;

static inline cWalkCharEntity *WChar(const void *e) {
	return (cWalkCharEntity *)e;
}

static inline cActCtrl *WChar_Ctrl(const void *e) {
	return WChar(e)->wl.actCtrl;
}

extern cGVar GVar_CharProcessReduction;
#define g_CharProcessReduction (GVar_CharProcessReduction.value.f)	/* nearer to a camera the AI slots are faster */
extern cGVar GVar_ArrowDeactivationRadius;
#define g_ArrowDeactivationRadius (GVar_ArrowDeactivationRadius.value.f)
extern cGVar GVar_ActiveExtrapolation;
#define g_ActiveExtrapolation (GVar_ActiveExtrapolation.value.i)		/* the view is extrapolated by its velocity */

/* the AI of the character classes (AI/); operator new of the size, then the constructor */
void *cAISentinel_ctor(void *mem, cEntity *e, int *arg);	/* bytes */
void *cAIGear_ctor(void *mem, cEntity *e, int *arg);
void *cAINurse_ctor(void *mem, cEntity *e, int *arg);
void *cAIBankDirector_ctor(void *mem, cEntity *e, int *arg);
void *cAIMayor_ctor(void *mem, cEntity *e, int *arg);
void *cAIPolice_ctor(void *mem, cEntity *e, int *arg);
void *cAIBishop_ctor(void *mem, cEntity *e, int *arg);
void *cAIMessenger_ctor(void *mem, cEntity *e, int *arg);
void *cAIBankMaster_ctor(void *mem, cEntity *e, int *arg);
void *cAIDesktop_ctor(void *mem, cEntity *e, int *arg);
void *cAIHuman_ctor(void *mem, cEntity *e, int *arg);
void *cAIChar_ctor(void *mem, cEntity *e, int *arg);		/* 0x380, every other class */

/* engine side */
void cWithLifeEntity_SetEmbeddedFX(cEntity *self, const char *fxType);

/* the energy is full and the victim a WalkChar and no action of priority runs; with checkClass its class must
 * allow it */
byte cWalkCharEntity_CanPossess(cEntity *self, byte checkClass);

cActCtrl *cWithLifeEntity_GetActCtrl(cEntity *self);
/* the fraction of the weapon energy above the full batteries */
float cWalkCharEntity_GetEnergyFrac(cEntity *e);
/* < 0 when no player controls the entity (assumed from SAI.EnableAI*) */
int cWalkCharEntity_GetOwnerSlot(cEntity *e);
/* the count of the full batteries */
int cWalkCharEntity_GetBatteries(cEntity *e);
/* WithLifeEntity.h and: the tests of the collision sphere */
byte cWithLifeEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit);
byte cWithLifeEntity_TestSphere(cEntity *self, cEntity *exclude, const vec3_t *center, float radius,
	const vec3_t *prev);
/* the weapon energy of a character, 0 or more */
void cWalkCharEntity_SetEnergy(cEntity *self, float energy);
/* the falling state of a character (0x464), with what goes with it */
void cWalkCharEntity_SetFalling(cEntity *self, byte on);
/* WithLifeEntity.h */
void cWithLifeEntity_SetMass(cEntity *self, float mass);
/* the view of a character set to the direction from pos to it */
void cWalkCharEntity_SetViewFrom(cEntity *self, const vec3_t *pos);
void cWalkCharEntity_SetYModelDisp(cWalkCharEntity *w, float y);
void cWalkCharEntity_SetViewAng(cWalkCharEntity *w, const vec3_t *ang);
void cWalkCharEntity_EnableAI(cEntity *self, byte on, int *arg);

#endif
