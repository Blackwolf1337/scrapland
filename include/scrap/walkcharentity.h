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

#include "scrap/scrapcollision.h"
#include "scrap/walkcharcontrol.h"
#include "scrap/withlifeentity.h"

#define CWALKCHARENTITY_VFTABLE GAME_VFTABLE(cWalkCharEntity_vftable, 0x3b03bc)
#define CWALKCHARENTITY_SIZE 0x574

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
SD_OFFSET(cWalkCharEntity, collide, 0x3e8);
SD_OFFSET(cWalkCharEntity, sphereBox, 0x404);
SD_OFFSET(cWalkCharEntity, ownerSlot, 0x424);
SD_OFFSET(cWalkCharEntity, renderView, 0x44c);
SD_OFFSET(cWalkCharEntity, falling, 0x464);
SD_OFFSET(cWalkCharEntity, fallVel, 0x480);
SD_OFFSET(cWalkCharEntity, onEnergyFull, 0x49c);
SD_OFFSET(cWalkCharEntity, reposHead, 0x4bc);
SD_OFFSET(cWalkCharEntity, viewLock, 0x4d4);
SD_OFFSET(cWalkCharEntity, control, 0x4ec);
SD_OFFSET(cWalkCharEntity, listen, 0x520);
SD_OFFSET(cWalkCharEntity, victim, 0x534);
SD_OFFSET(cWalkCharEntity, abortSelect, 0x558);
SD_SIZE(cWalkCharEntity, CWALKCHARENTITY_SIZE);

static inline cWalkCharEntity *WChar(const void *e) {
	return (cWalkCharEntity *)e;
}

static inline cActCtrl *WChar_Ctrl(const void *e) {
	return WChar(e)->wl.actCtrl;
}

/* the slots of the character vtable after the ones of cWithLifeEntity */
typedef struct cWalkCharVtbl {
	cWithLifeEntityVtbl base;
	void(__thiscall *EnableAI)(cEntity *self, byte on, int *arg);			/* 0xe0 vf56 */
	void(__thiscall *UpdateSelect)(cEntity *self, float dt);			/* 0xe4 vf57 */
	void(__thiscall *Turn)(cEntity *self, float dt);				/* 0xe8 vf58 */
	void(__thiscall *Idle)(cEntity *self, float dt);				/* 0xec vf59 */
	void(__thiscall *Fall)(cEntity *self, float dt);				/* 0xf0 vf60 */
	void(__thiscall *Accel)(cEntity *self, float dt);				/* 0xf4 vf61 */
	void(__thiscall *WalkAnim)(cEntity *self, float dt);				/* 0xf8 vf62 */
	void(__thiscall *ActionTick)(cEntity *self, float dt);				/* 0xfc vf63 */
	void(__thiscall *Fly)(cEntity *self, float dt);					/* 0x100 vf64 */
	void(__thiscall *FlySound)(cEntity *self, float dt);				/* 0x104 vf65 */
	void(__thiscall *WalkSound)(cEntity *self, float dt);				/* 0x108 vf66 */
	void(__thiscall *SetViewAng)(cEntity *self, const vec3_t *ang);			/* 0x10c vf67 */
	void(__thiscall *SetYModelDisp)(cEntity *self, float y);			/* 0x110 vf68 */
} cWalkCharVtbl;
SD_OFFSET(cWalkCharVtbl, EnableAI, 0xe0);
SD_OFFSET(cWalkCharVtbl, SetYModelDisp, 0x110);

static inline const cWalkCharVtbl *WChar_Vtbl(const cEntity *e) {
	return (const cWalkCharVtbl *)e->vftable;
}

/* engine variables (Gvars) */
#define g_CharUsrPushSpeed GAME_GLOBAL_IN(float, GVar_CharUsrPushSpeed, 16, 0x44729c)		/* the push of an AI the player runs into */
#define g_TimeToAbortSelectChar GAME_GLOBAL_IN(float, GVar_TimeToAbortSelectChar, 16, 0x4472fc)
#define g_AbortSelectDistance GAME_GLOBAL_IN(float, GVar_AbortSelectDistance, 16, 0x447224)
#define g_CharProcessReduction GAME_GLOBAL_IN(float, GVar_CharProcessReduction, 16, 0x447254)	/* nearer to a camera the AI slots are faster */
#define g_SndLoopCharNPCMul GAME_GLOBAL_IN(float, GVar_SndLoopCharNPCMul, 16, 0x447284)
#define g_CharGrav GAME_GLOBAL_IN(float, GVar_CharGrav, 16, 0x4472cc)
#define g_ArrowDeactivationRadius GAME_GLOBAL_IN(float, GVar_ArrowDeactivationRadius, 16, 0x447928)
#define g_ActiveExtrapolation GAME_GLOBAL_IN(int, GVar_ActiveExtrapolation, 16, 0x447a08)		/* the view is extrapolated by its velocity */

/* the AI of the character classes (AI/, rva 0x066120..0x086100); operator new of the size, then the constructor */
void *__thiscall cAISentinel_ctor(void *mem, cEntity *e, int *arg);	/* 0x380 bytes */
void *__thiscall cAIGear_ctor(void *mem, cEntity *e, int *arg);		/* 0x390 */
void *__thiscall cAINurse_ctor(void *mem, cEntity *e, int *arg);		/* 0x384 */
void *__thiscall cAIBankDirector_ctor(void *mem, cEntity *e, int *arg);	/* 0x384 */
void *__thiscall cAIMayor_ctor(void *mem, cEntity *e, int *arg);		/* 0x380 */
void *__thiscall cAIPolice_ctor(void *mem, cEntity *e, int *arg);		/* 0x380 */
void *__thiscall cAIBishop_ctor(void *mem, cEntity *e, int *arg);		/* 0x398 */
void *__thiscall cAIMessenger_ctor(void *mem, cEntity *e, int *arg);	/* 0x390 */
void *__thiscall cAIBankMaster_ctor(void *mem, cEntity *e, int *arg);	/* 0x384 */
void *__thiscall cAIDesktop_ctor(void *mem, cEntity *e, int *arg);		/* 0x380 */
void *__thiscall cAIHuman_ctor(void *mem, cEntity *e, int *arg);		/* 0x380 */
void *__thiscall cAIChar_ctor(void *mem, cEntity *e, int *arg);		/* 0x380, every other class */

/* engine side */
/* the name of the default loop action of the controller, "" without one (rva 0x62c80) */
const char *__thiscall cActCtrl_GetDefLoopName(void *ctrl);
void __thiscall cWithLifeEntity_SetEmbeddedFX(cEntity *self, const char *fxType);	/* rva 0x12c7f0 */

/* WalkCharEntity.c (the functions declared in chartype.h, ai.h and scoreritems.h too) */
void __fastcall cWalkCharEntity_dtor(cEntity *self);
cEntity *__thiscall cWalkCharEntity_Delete(cEntity *self, uint32_t flags);
/* vf63: the press and release callbacks of the class, the talk with the victim or listener, the energy over dt */
void __thiscall cWalkCharEntity_ActionTick(cEntity *self, float dt);
/* vf24 (UpdateModel): the model at the render position and angles, the head turned to the view (blended by the view
 * lock) */
void __thiscall cWalkCharEntity_UpdateModel(cEntity *self);
/* the view lock of the class (and the blend towards it in dt) */
void __thiscall cWalkCharEntity_UpdateViewLock(cEntity *self, float dt);
/* the energy is full and the victim a WalkChar and no action of priority runs; with checkClass its class must
 * allow it */
byte __thiscall cWalkCharEntity_CanPossess(cEntity *self, byte checkClass);
/* the link node at 0x3e8 unlinked */
void __fastcall cWalkCharEntity_UnlinkCollide(cEntity *self);

/* vf11: e (with life) pushes the character out of its collision sphere, in x and z unless it falls or flies */
void __thiscall cWalkCharEntity_Touch(cEntity *self, cEntity *e, const vec3_t *point, const vec3_t *normal);
/* the walk of a slot: the view, the actions, the velocity, the slides along the map and the other entities */
void __thiscall cWalkCharEntity_Walk(cEntity *self, float dt);
/* vf58: the view turned by the control (clamped up and down) */
void __thiscall cWalkCharEntity_Turn(cEntity *self, float dt);
/* the walk action loop with init (or loop at once) unless one of them runs; end is the action of the stop */
void __thiscall cWalkCharEntity_PlayWalk(cEntity *self, const char *init, const char *loop, const char *end);
/* the flash of the model after a hit (life below the maximum) */
void __thiscall cWalkCharEntity_UpdateHitFlash(cEntity *self, float dt);
/* vf56: the AI of the class made (on, unless there is one) or deleted */
void __thiscall cWalkCharEntity_EnableAI(cEntity *self, byte on, int *arg);
byte __thiscall cWalkCharEntity_VarF(cEntity *self, const char *name, double *d, int set);
/* vf60: the gravity, the steps and the floor */
void __thiscall cWalkCharEntity_Fall(cEntity *self, float dt);
/* vf64: the flight of a class that can fly */
void __thiscall cWalkCharEntity_Fly(cEntity *self, float dt);
void *__thiscall cWithLifeEntity_GetActCtrl(cEntity *self);
float __thiscall cWalkCharEntity_GetEnergyFrac(cEntity *e);
vec3_t *__thiscall cWalkCharEntity_GetRenderAng(cEntity *self, vec3_t *out);
vec3_t *__thiscall cWalkCharEntity_GetSndAng(cEntity *self, vec3_t *out);
/* "RelaxAlarm" while the alarm is on for a class with alarm actions, else "Relax" */
const char *__fastcall cWalkCharEntity_RelaxName(cEntity *self);
void __thiscall cWalkCharEntity_GetCamera(cEntity *self, vec3_t *pos, vec3_t *ang);
const char *__thiscall cWalkCharEntity_GetClassName(cEntity *self);
int __thiscall cWalkCharEntity_GetOwnerSlot(cEntity *e);
float __thiscall cWalkCharEntity_GetRadius(cEntity *self);
int __thiscall cWalkCharEntity_GetBatteries(cEntity *e);
byte __thiscall cWalkCharEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte __thiscall cWalkCharEntity_IsType(cEntity *self, const char *type);
/* WithLifeEntity.h vf09 and vf10: the tests of the collision sphere */
byte __thiscall cWithLifeEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit);
byte __thiscall cWithLifeEntity_TestSphere(cEntity *self, cEntity *exclude, const vec3_t *center, float radius,
	const vec3_t *prev);
/* ObjEntity.h vf41 and vf43 (nothing) and vf42 (1) */
void __thiscall cObjEntity_Vf41(cEntity *self, int a, int b, int c);
byte __thiscall cObjEntity_Vf42(cEntity *self, int a);
void __thiscall cObjEntity_Vf43(cEntity *self, int a, int b);
/* vf23 */
void __thiscall cWalkCharEntity_Tick(cEntity *self, float dt);
/* vf48: the listener unlinked */
void __thiscall cWalkCharEntity_UnlinkListen(cEntity *self);
/* vf59: the body turned to the view, the relax and turn actions, the view to the listener */
void __thiscall cWalkCharEntity_Idle(cEntity *self, float dt);
/* vf57: the selection of the player and the callbacks of a lost one */
void __thiscall cWalkCharEntity_UpdateSelect(cEntity *self, float dt);
void __thiscall cWalkCharEntity_SetActive(cEntity *self, byte on);
void __thiscall cWalkCharEntity_SetAng(cEntity *self, const vec3_t *ang);
void __thiscall cWalkCharEntity_SetEnergy(cEntity *self, float energy);
/* the used energy, at most 1 */
void __thiscall cWalkCharEntity_SetEnergyUsed(cEntity *self, float used);
void __thiscall cWalkCharEntity_SetEmbeddedFX(cEntity *self, const char *fxType);
void __thiscall cWalkCharEntity_SetFalling(cEntity *self, byte on);
/* WithLifeEntity.h vf51 */
void __thiscall cWithLifeEntity_SetMass(cEntity *self, float mass);
/* vf12: also the floor distance and the camera node of the model */
void __thiscall cWalkCharEntity_SetModel(cEntity *self, const char *file, float sx, float sy, float sz, int flag);
/* an AI slows its slots down unless it is seen or near a camera */
void __thiscall cWalkCharEntity_UpdateSlotRate(cEntity *self, float dt);
/* the push of an AI, once a slot: 1 if it was taken */
byte __thiscall cWalkCharEntity_SetPush(cEntity *self, const vec3_t *push);
void __thiscall cWalkCharEntity_ApplyPush(cEntity *self, float dt);
/* "UsrControl": the player slot (-1 the AI) */
void __thiscall cWalkCharEntity_SetUsrControl(cEntity *self, int slot);
void __thiscall cWalkCharEntity_SetVel(cEntity *self, const vec3_t *vel);
void __thiscall cWalkCharEntity_SetViewAng(cEntity *self, const vec3_t *ang);
void __thiscall cWalkCharEntity_SetViewFrom(cEntity *self, const vec3_t *pos);
void __thiscall cWalkCharEntity_SetYModelDisp(cEntity *self, float y);
/* vf61: the walk acceleration of the control, the friction and the speed limit */
void __thiscall cWalkCharEntity_Accel(cEntity *self, float dt);
void __thiscall cWalkCharEntity_Update(cEntity *self);
/* vf65 and vf66: the pitch and volume of the loop sound 0 by the speed */
void __thiscall cWalkCharEntity_FlySound(cEntity *self, float dt);
void __thiscall cWalkCharEntity_WalkSound(cEntity *self, float dt);
byte __thiscall cWalkCharEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte __thiscall cWalkCharEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
/* vf62: the walk, run, strafe and back actions by the control and the velocity they give */
void __thiscall cWalkCharEntity_WalkAnim(cEntity *self, float dt);
/* the action of layer 0 is Run, Walk or Sprint...; with Back...; StrRight or StrLeft... */
byte __fastcall cWalkCharEntity_IsMoveAnim(cEntity *self);
byte __fastcall cWalkCharEntity_IsBackOrMoveAnim(cEntity *self);
byte __fastcall cWalkCharEntity_IsStrafeAnim(cEntity *self);
/* the class has alarm actions and the alarm is on */
byte __fastcall cWalkCharEntity_IsAlarm(cEntity *self);

#endif
