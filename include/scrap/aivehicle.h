/*
 * AI/AIVehicle.cpp of the game: cAIVehicle, the AI of a vehicle (0x3b0 bytes, a cAIMove). It owns the move control
 * (cAIVehicleControl, AI/IVehicleControl.cpp) and the shoot control (cAIVehicleShoot) and runs the state machine
 * of SAI.SetStateVehicle: the external state (FREEZE, GO_TO, FIGHTING, ... STATE_NONE) and the internal state of the
 * move (STOPPED, FINISHING_LINE*, CHASE_*, FOLLOW_THE_PATH*, ORIENT_SHOOTING). The think slot reacts to the target
 * and the items (life and ammo) around, the graph slot repositions the AI near the camera (traffic, routes). A
 * change of state the vehicle group does not let through yet waits in "pending". src/game/AI/IVehicle.c; the
 * parameter blocks of SAI.EnableAIVehicle are in include/scrap/ai.h.
 */
#ifndef SCRAP_AIVEHICLE_H
#define SCRAP_AIVEHICLE_H

#include <stdarg.h>

#include "scrap/gvar.h"
#include "scrap/ai.h"
#include "scrap/aivehiclecontrol.h"
#include "scrap/cstring.h"
#include "scrap/nodestraffic.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"
#include "scrap/slotcontroller.h"

#define CAIVEHICLE_SIZE sizeof(cAIVehicle)
#define CAIVEHICLECONTROL_SIZE sizeof(cAIVehicleControl)
#define CAIVEHICLESHOOT_SIZE sizeof(cAIVehicleShoot)

/* a state with its arguments: the one to take when the vehicle group lets it (pending) and the one kept by
 * cAIVehicle_SaveState to come back to after an event (saved) */
typedef struct cAIVehicleSaved {
	int state;
	int internState;
	vec3_t dest;
	float dist;
	byte boost;
	char target[0x9c - 0x19];	/* the name of the target entity, "" for none */
	vec3_t dir;
	float awayDist;
	byte changing;
	byte pad_ad[3];
	float accel;
	byte repos;
	byte pad_b5[3];
	float chaseMin;
	float chaseMax;
	byte stopAtGoal;
	byte pad_c1[3];
	float rotA;
	float rotB;
} cAIVehicleSaved;

struct cAIVehicle {
	union {
		cAIMove base;
		const cAIMoveVtbl *vftable;
	};
	cAIVehicleSaved pending;	/* state AIV_STATE_NONE for none */
	byte(*onArrive)(cAIVehicle *self);	/* GO_TO arrived: 1 if it took care of it */
	cSlotController slotAttack;
	cSlotController slotGraph;	/* the repositioning */
	cSlotController slotItems;
	cEntity *vehicle;
	cAIVehicleControl *control;
	cAIVehicleShoot *shoot;
	int state;			/* AIV_ */
	int prevState;
	int internState;		/* AIVI_ */
	byte boost;			/* the boost of the moves to a point and along the path */
	byte pad_1a9[3];
	int lifeMark;			/* the life of the vehicle at the last damage check */
	float damageTime;
	cAIVehicleParams1 params;	/* vBehav */
	cAIVehicleParams1 paramsInit;
	cAIVehicleSaved saved;
	int track;
	int node;
	byte reposBehind;		/* the next repos is behind the camera */
	byte pad_2e1[3];
	vec3_t trackOfs;
	float awayDist;			/* GET_AWAY_STOP: the half size of the box around the target to leave */
	float maxDistRepos;
	float minDistRepos;
	float maxDistReposNotVis;
	float minDistReposNotVis;
	float rotA;
	float rotB;
	float accel;			/* the acceleration of the move to a point */
	float chaseMin;
	float chaseMax;
	byte unused_318;
	byte pad_319[3];
	int trackType;			/* 0..4, 5 none */
	vec3_t trackPos;
	byte stopAtGoal;		/* the move to a point stops there */
	byte pad_32d[3];
	float targetTime;		/* the time the target is held (the hook, the lost path) */
	int hookSide;			/* the side of the next hook point, 0..2 */
	int hookMode;
	int hookSaved;
	byte goingForLife;		/* on the way to a life item */
	byte pad_341[3];
	cItemDesc itemLife;		/* the nearest life item */
	cEntity *itemLifeEnt;
	int itemLifeMark;
	byte goingForAmmo;		/* on the way to an ammo item */
	byte pad_355[3];
	cItemDesc itemAmmo;		/* the nearest ammo item */
	cItemDesc itemAmmoK[3];		/* the nearest of each kind */
	float itemAmmoLevel;		/* the ammo of the kind of the item when it was found */
	float itemAmmoStart;		/* the ammo when the AI went for the item */
	cEntity *itemAmmoEnt[3];
	cEntity *itemAmmoBest;
	float itemAmmoDistK[3];
	int unused_39c[3];
	byte unused_3a8;
	byte unused_3a9[2];
	byte pad_3ab;
	int ammoSearch;			/* the nearest ammo first (0) or the farthest (1), 3 none */
};

static inline cEntity *AIV_Target(const cAIVehicle *ai) {
	return ai->base.target.target;
}

/* the engine variables of the vehicle AI */
extern cGVar GVar_MaxDistReposAI;
#define g_MaxDistReposAI (GVar_MaxDistReposAI.value.f)
extern cGVar GVar_MinDistReposAI;
#define g_MinDistReposAI (GVar_MinDistReposAI.value.f)
extern cGVar GVar_HookRange;
#define g_HookRange (GVar_HookRange.value.f)

/* the boost flag of the vehicle from CONTROL_AIM (after the parameters changed) */
void cAIVehicle_ApplyParams(cAIVehicle *self);
byte cAIVehicle_Think(cAIVehicle *self);
cAIVehicleControl *cAIVehicle_GetControl(cAIVehicle *self);
byte cAIVehicle_GetChanged(cAIVehicle *self);
void cAIVehicle_GetInternStateName(cAIVehicle *self, char *buf);
/* the names of the state and its sub state for the debug texts */
void cAIVehicle_GetStateName(cAIVehicle *self, char *buf);
cAIVehicleParams1 *cAIVehicle_GetParamsInit(cAIVehicle *self);
cAIVehicleParams2 *cAIVehicle_GetParams2Init(cAIVehicle *self);
cAIVehicleParams3 *cAIVehicle_GetParams3Init(cAIVehicle *self);
/* a flag of the attack part of the AI (+0x194, its +0x174) */
byte cAIVehicle_ControlFlag(cAIVehicle *self);
byte cAIVehicle_InSight(cAIVehicle *self);

/* ---- cAIVehicleControl and cAIVehicleShoot (src/game/AI/IVehicleControl.c, IVehicleShootControl.c; the rest of
 * them in aivehiclecontrol.h) ---- */
cAIVehicleControl *cAIVehicleControl_ctor(void *mem, cEntity *v, cAIVehicle *ai, void *params);
void cAIVehicleControl_ApplyParams(cAIVehicleControl *self);
void cAIVehicleControl_Update(cAIVehicleControl *self, float dt);
void cAIVehicleControl_Draw(cAIVehicleControl *self);
cAIVehicleParams2 *cAIVehicleControl_GetParams(cAIVehicleControl *self);
cAIVehicleParams2 *cAIVehicleControl_GetParamsInit(cAIVehicleControl *self);
byte cAIVehicleControl_GetFlag(cAIVehicleControl *self);
byte cAIVehicleControl_InSight(cAIVehicleControl *self);
void cAIVehicleControl_SetInertia(cAIVehicleControl *self, int on);
void cAIVehicleControl_SetRotStatic(cAIVehicleControl *self, float a, float b);
/* the last point (0xc4) and the point of the target (0x98, -FLT_MAX each after ClearDest) */
vec3_t *cAIVehicleControl_GetGoal(cAIVehicleControl *self, vec3_t *out);
vec3_t *cAIVehicleControl_GetDest(cAIVehicleControl *self, vec3_t *out);
void cAIVehicleControl_ClearDest(cAIVehicleControl *self);
/* the controls of the vehicle let go */
void cAIVehicleControl_Stop(cAIVehicleControl *self);
/* the move of the control (mode 0 stop, 1 to a point, 2 direct, 3 chase, 6 orient, 7 path; the arguments by mode,
 * see cAIVehicle_SetInternState); 0 if it was refused */
byte cAIVehicleControl_SetMove(cAIVehicleControl *self, int mode, ...);
/* the byte at 0xb0: 1 in LOST_PATH, 0 at every change of state */
void cAIVehicleControl_SetDirect(cAIVehicleControl *self, byte on);
cAIVehicleShoot *cAIVehicleShoot_ctor(void *mem, cEntity *v, cAIVehicle *ai, void *params);
void cAIVehicleShoot_Update(cAIVehicleShoot *self, float dt);
void cAIVehicleShoot_Update2(cAIVehicleShoot *self, float dt);
void cAIVehicleShoot_Draw(cAIVehicleShoot *self);
cAIVehicleParams3 *cAIVehicleShoot_GetParams(cAIVehicleShoot *self);
cAIVehicleParams3 *cAIVehicleShoot_GetParamsInit(cAIVehicleShoot *self);
void cAIVehicleShoot_WeapListChanged(cAIVehicleShoot *self);
/* the vehicle has the hook (byte at 0x61) */
byte cAIVehicleShoot_HasHook(cAIVehicleShoot *self);
/* the ammo of the weapons in use above factor of the maximum */
byte cAIVehicleShoot_HasAmmo(cAIVehicleShoot *self, float factor);
/* the vehicle has a weapon for the ammo kind (the ones of the factor below a limit or above) */
byte cAIVehicleShoot_UsesAmmo(cAIVehicleShoot *self, int kind, float factor);
byte cAIVehicleShoot_TargetInSight(cAIVehicleShoot *self, int arg);
byte cAIVehicleShoot_SetMode(cAIVehicleShoot *self, int a, int b, byte c);
/* the fields at 0x124, 0x128 (2 in a race at the finishing line, assumed) and 0x178 */
void cAIVehicleShoot_GetInfo(cAIVehicleShoot *self, int *a, int *kind, byte *flag);

/* ---- Global/Nto1Links.h: the out of line copies the AI calls ---- */

/* ---- cAIVehicleGroup ---- */
/* an elevator of the group within dist of pos */
byte cAIVehicleGroup_ElevatorNear(cAIVehicleGroup *self, const vec3_t *pos, float dist);

#endif
