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
	cWithLifeEntity wl;		/* 0x000 */
	/* cWalkCharEntity */
	Nto1Node collide;		/* 0x3e8 "CollideEntity": the entity it ran into */
	float moved2;			/* 0x3f8 the square of the distance of the last walk step */
	float headPitch;		/* 0x3fc of the camera node (or the render angles) */
	int camNode;			/* 0x400 DC_Camera of the model */
	CSphereBox sphereBox;		/* 0x404 the cache of the slides */
	int ownerSlot;			/* 0x424 "UsrControl": the player slot, -1 for an AI */
	vec3_t view;			/* 0x428 "ViewAng" */
	vec3_t viewVel;			/* 0x434 */
	vec3_t slotView;		/* 0x440 of the last slot */
	vec3_t renderView;		/* 0x44c between two slots */
	float relaxTime;		/* 0x458 "TimeToExtraRelax" */
	float floorDist;		/* 0x45c "FloorDist" */
	float slideTime;		/* 0x460 "SlidingAndFallingTime" */
	byte falling;			/* 0x464 "isFalling" */
	byte walking;			/* 0x465 "isWalking" */
	byte pad_466[2];
	float displAng;			/* 0x468 "DisplAng": the walk direction against the view */
	float unused_46c;			/* 0x46c */
	float lastMove;			/* 0x470 the move of the walk while it stops */
	float lastDisplAng;		/* 0x474 */
	float walkTime;			/* 0x478 the stop is kept that long */
	float move;			/* 0x47c 0..1 from the control */
	vec3_t fallVel;			/* 0x480 the velocity of the fall */
	byte disableControl;		/* 0x48c "DisableControl" */
	byte pad_48d[3];
	cAction *walkEnd;		/* 0x490 the end action of the walk */
	float yModelDisp;		/* 0x494 "YModelDisp" */
	float energyUsed;		/* 0x498 1 minus the weapon energy */
	cPy_Function onEnergyFull;	/* 0x49c "OnEnergyBarFull" */
	float energyBarDecr;		/* 0x4b0 "EnergyBarDecr": the energy an action needs */
	float energyTime;		/* 0x4b4 "EnergyBarConstantDecr" */
	float hitFlash;			/* 0x4b8 the seconds left of the flash of a hit */
	Nto1Node reposHead;		/* 0x4bc the point of the character group it was placed at (cAICGPoint) */
	int money;			/* 0x4cc "Money" */
	int moneyReward;		/* 0x4d0 "MoneyReward" */
	byte viewLock;			/* 0x4d4 the actions set the view angles */
	byte pad_4d5[3];
	float viewLockF;		/* 0x4d8 the blend of the view lock */
	byte pushOn;			/* 0x4dc a push of an AI */
	byte pad_4dd[3];
	vec3_t push;			/* 0x4e0 */
	cWalkCharControl control;	/* 0x4ec */
	Nto1Node listen;		/* 0x520 "SelectedEntity": the one it listens to */
	byte listenOn;			/* 0x530 "AutoSelect" */
	byte pad_531[3];
	Nto1Node victim;		/* 0x534 "AutoSelectedEntity": the selection of the arrow */
	cPy_Function outSelect;		/* 0x544 "OutSelectFunc" */
	cPy_Function abortSelect;	/* 0x558 "AbortSelectFunc" */
	float abortTime;		/* 0x56c the selection is kept that long without a change */
	int abortOpt;			/* 0x570 the option of the arrow at the last slot */
} cWalkCharEntity;

static inline cWalkCharEntity *WChar(const void *e) {
	return (cWalkCharEntity *)e;
}

static inline cActCtrl *WChar_Ctrl(const void *e) {
	return WChar(e)->wl.actCtrl;
}

/* the slots of the character vtable after the ones of cWithLifeEntity */
typedef struct cWalkCharVtbl {
	cWithLifeEntityVtbl base;
	void(*EnableAI)(cEntity *self, byte on, int *arg);			/* 0xe0 vf56 */
	void(*UpdateSelect)(cEntity *self, float dt);			/* 0xe4 vf57 */
	void(*Turn)(cEntity *self, float dt);				/* 0xe8 vf58 */
	void(*Idle)(cEntity *self, float dt);				/* 0xec vf59 */
	void(*Fall)(cEntity *self, float dt);				/* 0xf0 vf60 */
	void(*Accel)(cEntity *self, float dt);				/* 0xf4 vf61 */
	void(*WalkAnim)(cEntity *self, float dt);				/* 0xf8 vf62 */
	void(*ActionTick)(cEntity *self, float dt);				/* 0xfc vf63 */
	void(*Fly)(cEntity *self, float dt);					/* 0x100 vf64 */
	void(*FlySound)(cEntity *self, float dt);				/* 0x104 vf65 */
	void(*WalkSound)(cEntity *self, float dt);				/* 0x108 vf66 */
	void(*SetViewAng)(cEntity *self, const vec3_t *ang);			/* 0x10c vf67 */
	void(*SetYModelDisp)(cEntity *self, float y);			/* 0x110 vf68 */
} cWalkCharVtbl;

static inline const cWalkCharVtbl *WChar_Vtbl(const cEntity *e) {
	return (const cWalkCharVtbl *)e->vftable;
}

extern cGVar GVar_CharProcessReduction;
#define g_CharProcessReduction (GVar_CharProcessReduction.value.f)	/* nearer to a camera the AI slots are faster */
extern cGVar GVar_ArrowDeactivationRadius;
#define g_ArrowDeactivationRadius (GVar_ArrowDeactivationRadius.value.f)
extern cGVar GVar_ActiveExtrapolation;
#define g_ActiveExtrapolation (GVar_ActiveExtrapolation.value.i)		/* the view is extrapolated by its velocity */

/* the AI of the character classes (AI/, rva 0x066120..0x086100); operator new of the size, then the constructor */
void *cAISentinel_ctor(void *mem, cEntity *e, int *arg);	/* 0x380 bytes */
void *cAIGear_ctor(void *mem, cEntity *e, int *arg);		/* 0x390 */
void *cAINurse_ctor(void *mem, cEntity *e, int *arg);		/* 0x384 */
void *cAIBankDirector_ctor(void *mem, cEntity *e, int *arg);	/* 0x384 */
void *cAIMayor_ctor(void *mem, cEntity *e, int *arg);		/* 0x380 */
void *cAIPolice_ctor(void *mem, cEntity *e, int *arg);		/* 0x380 */
void *cAIBishop_ctor(void *mem, cEntity *e, int *arg);		/* 0x398 */
void *cAIMessenger_ctor(void *mem, cEntity *e, int *arg);	/* 0x390 */
void *cAIBankMaster_ctor(void *mem, cEntity *e, int *arg);	/* 0x384 */
void *cAIDesktop_ctor(void *mem, cEntity *e, int *arg);		/* 0x380 */
void *cAIHuman_ctor(void *mem, cEntity *e, int *arg);		/* 0x380 */
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
/* WithLifeEntity.h vf09 and vf10: the tests of the collision sphere */
byte cWithLifeEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit);
byte cWithLifeEntity_TestSphere(cEntity *self, cEntity *exclude, const vec3_t *center, float radius,
	const vec3_t *prev);
/* the weapon energy of a character, 0 or more */
void cWalkCharEntity_SetEnergy(cEntity *self, float energy);
/* the falling state of a character (0x464), with what goes with it */
void cWalkCharEntity_SetFalling(cEntity *self, byte on);
/* WithLifeEntity.h vf51 */
void cWithLifeEntity_SetMass(cEntity *self, float mass);
/* the view of a character set to the direction from pos to it */
void cWalkCharEntity_SetViewFrom(cEntity *self, const vec3_t *pos);

#endif
