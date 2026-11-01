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

#include "scrap/ai.h"
#include "scrap/aivehiclecontrol.h"
#include "scrap/cstring.h"
#include "scrap/nodestraffic.h"
#include "scrap/pyfunction.h"
#include "scrap/slotcontroller.h"

#define CAIVEHICLE_VFTABLE GAME_VFTABLE(cAIVehicle_vftable, 0x3a8448)
#define CAIVEHICLE_SIZE 0x3b0
#define CAIVEHICLECONTROL_SIZE 0x2b8
#define CAIVEHICLESHOOT_SIZE 0x184

/* the external states (SAI.SetStateVehicle, the names of cAIVehicle_StateName) */
enum {
	AIV_FREEZE, AIV_GO_TO, AIV_FIGHTING, AIV_IN_ROUTE, AIV_TRAFFIC, AIV_GET_AWAY_STOP, AIV_RACE, AIV_CHASE,
	AIV_FIGHTING_FREEZE, AIV_TRAFFIC_INDOOR, AIV_FIGHTING_HOOK, AIV_FREEZE_OBJ_NOT_ACCESS, AIV_GET_AWAY,
	AIV_LOST_PATH, AIV_LOST_PATH_CHASE, AIV_FREEZE_LOST, AIV_FREEZE_CHANGE_STATE, AIV_STATE_NONE
};

/* the internal states of the move (cAIVehicle_InternStateName) */
enum {
	AIVI_STOPPED, AIVI_FINISHING_LINE, AIVI_FINISHING_LINE_WITH_SHOOT_CM, AIVI_FINISHING_LINE_DIRECT,
	AIVI_FINISHING_LINE_LIFE, AIVI_FINISHING_LINE_AMMO, AIVI_CHASE_SHOOTING, AIVI_CHASE_NOT_SHOOTING,
	AIVI_FOLLOW_THE_PATH, AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM, AIVI_ORIENT_SHOOTING, AIVI_STATE_INTERN_NONE
};

/* the events of the move (cAIVehicle_Event): 0 path lost, 1 target not reachable, 2 arrived, 3 target gone */


/* a state with its arguments: the one to take when the vehicle group lets it (pending) and the one kept by
 * cAIVehicle_SaveState to come back to after an event (saved) */
typedef struct cAIVehicleSaved {
	int state;			/* 0x00 */
	int internState;		/* 0x04 */
	vec3_t dest;			/* 0x08 */
	float speed;			/* 0x14 */
	byte boost;			/* 0x18 */
	char target[0x9c - 0x19];	/* 0x19 the name of the target entity, "" for none */
	vec3_t dest2;			/* 0x9c */
	float awayDist;			/* 0xa8 */
	byte changing;			/* 0xac */
	byte pad_ad[3];
	float accel;			/* 0xb0 */
	byte repos;			/* 0xb4 */
	byte pad_b5[3];
	float chaseMin;			/* 0xb8 */
	float chaseMax;			/* 0xbc */
	byte stopAtGoal;		/* 0xc0 */
	byte pad_c1[3];
	float rotA;			/* 0xc4 */
	float rotB;			/* 0xc8 */
} cAIVehicleSaved;
SD_OFFSET(cAIVehicleSaved, target, 0x19);
SD_OFFSET(cAIVehicleSaved, dest2, 0x9c);
SD_SIZE(cAIVehicleSaved, 0xcc);

typedef struct cAIVehicleVtbl {
	cAIVehicle *(__thiscall *Delete)(cAIVehicle *self, byte flags);			/* 0x00 */
	void(__thiscall *Update)(cAIVehicle *self, float dt);				/* 0x04 */
	void(__thiscall *UpdateOff)(cAIVehicle *self, float dt);				/* 0x08 */
	void(__thiscall *Draw)(cAIVehicle *self);						/* 0x0c */
	byte(__thiscall *GetChanged)(cAIVehicle *self);					/* 0x10 */
	byte(__thiscall *IsVisible)(cAIVehicle *self);					/* 0x14 */
	void (*Debugf)(cAIVehicle *self, const char *fmt, ...);				/* 0x18 */
	byte(__thiscall *VarI)(cAIVehicle *self, const char *name, int *i, byte set);	/* 0x1c */
	void *vf08_0c[5];
	void(__thiscall *OnEvent)(cAIVehicle *self, int ev);				/* 0x34 the debug text of the event */
	Nto1Node *(__thiscall *GetTargetNode)(cAIVehicle *self);			/* 0x38 &target */
	void *vf0f_11[3];
	cEntity *(__thiscall *GetTargetEnt)(cAIVehicle *self);				/* 0x48 the target */
	void *vf13_15[3];
	byte(__thiscall *Event)(cAIVehicle *self, int ev);				/* 0x58 vf22 */
	byte(__thiscall *IsMoving)(cAIVehicle *self);					/* 0x5c vf23 */
	byte(__thiscall *CanTarget)(cAIVehicle *self, cEntity *e);			/* 0x60 */
} cAIVehicleVtbl;
SD_OFFSET(cAIVehicleVtbl, Debugf, 0x18);
SD_OFFSET(cAIVehicleVtbl, OnEvent, 0x34);
SD_OFFSET(cAIVehicleVtbl, GetTargetNode, 0x38);
SD_OFFSET(cAIVehicleVtbl, Event, 0x58);
SD_OFFSET(cAIVehicleVtbl, CanTarget, 0x60);

struct cAIVehicle {
	const cAIVehicleVtbl *vftable;	/* 0x000 */
	byte pad_4[0xc - 0x4];
	float dt;			/* 0x00c the time of the frame */
	cAIVSlot slotThink;		/* 0x010 of cAIMove: the reactions */
	byte changing;			/* 0x020 a change of state runs */
	byte pad_21[0x28 - 0x21];
	byte changed;			/* 0x028 the state changed in this frame */
	byte pad_29[0x2c - 0x29];
	float stateTime;		/* 0x02c */
	float waitTime;			/* 0x030 of cAI: the time out of view */
	int seen;			/* 0x034 of cAI: 0 out of view, 1 seen, 2 not seen yet */
	Nto1Node target;		/* 0x038 the target entity at 0x40 */
	byte pad_48[0x4c - 0x48];
	vec3_t dest;			/* 0x04c */
	float speed;			/* 0x058 */
	int moveMode;			/* 0x05c */
	byte pad_60[0x64 - 0x60];
	vec3_t dest2;			/* 0x064 */
	byte pad_70[0x81 - 0x70];
	byte flat;			/* 0x081 the path move stays flat */
	byte pad_82[0x86 - 0x82];
	byte repos;			/* 0x086 "Repos" */
	byte pad_87;
	byte trafficPlaced;		/* 0x088 TRAFFIC with a kind of track: placed, repositioned like a visible one */
	byte pad_89[0x90 - 0x89];
	cAIVehicleSaved pending;	/* 0x090 state AIV_STATE_NONE for none */
	byte(*onArrive)(cAIVehicle *self);	/* 0x15c GO_TO arrived: 1 if it took care of it */
	cAIVSlot slotAttack;		/* 0x160 */
	cAIVSlot slotGraph;		/* 0x170 the repositioning */
	cAIVSlot slotItems;		/* 0x180 */
	cEntity *vehicle;		/* 0x190 */
	cAIVehicleControl *control;	/* 0x194 */
	cAIVehicleShoot *shoot;		/* 0x198 */
	int state;			/* 0x19c AIV_ */
	int prevState;			/* 0x1a0 */
	int internState;		/* 0x1a4 AIVI_ */
	byte boost;			/* 0x1a8 the boost of the moves to a point and along the path */
	byte pad_1a9[3];
	int lifeMark;			/* 0x1ac the life of the vehicle at the last damage check */
	float damageTime;		/* 0x1b0 */
	cAIVehicleParams1 params;	/* 0x1b4 vBehav */
	cAIVehicleParams1 paramsInit;	/* 0x1e0 */
	cAIVehicleSaved saved;		/* 0x20c */
	int track;			/* 0x2d8 */
	int node;			/* 0x2dc */
	byte reposBehind;		/* 0x2e0 the next repos is behind the camera */
	byte pad_2e1[3];
	vec3_t trackOfs;		/* 0x2e4 */
	float awayDist;			/* 0x2f0 GET_AWAY_STOP: the half size of the box around the target to leave */
	float maxDistRepos;		/* 0x2f4 */
	float minDistRepos;		/* 0x2f8 */
	float maxDistReposNotVis;	/* 0x2fc */
	float minDistReposNotVis;	/* 0x300 */
	float rotA;			/* 0x304 */
	float rotB;			/* 0x308 */
	float accel;			/* 0x30c the acceleration of the move to a point */
	float chaseMin;			/* 0x310 */
	float chaseMax;			/* 0x314 */
	byte unused_318;			/* 0x318 */
	byte pad_319[3];
	int trackType;			/* 0x31c 0..4, 5 none */
	vec3_t trackPos;		/* 0x320 */
	byte stopAtGoal;		/* 0x32c the move to a point stops there */
	byte pad_32d[3];
	float targetTime;		/* 0x330 the time the target is held (the hook, the lost path) */
	int hookSide;			/* 0x334 the side of the next hook point, 0..2 */
	int hookMode;			/* 0x338 */
	int hookSaved;			/* 0x33c */
	byte goingForLife;		/* 0x340 on the way to a life item */
	byte pad_341[3];
	cItemDesc itemLife;		/* 0x344 the nearest life item */
	cEntity *itemLifeEnt;		/* 0x34c */
	int itemLifeMark;		/* 0x350 */
	byte goingForAmmo;		/* 0x354 on the way to an ammo item */
	byte pad_355[3];
	cItemDesc itemAmmo;		/* 0x358 the nearest ammo item */
	cItemDesc itemAmmoK[3];		/* 0x360 the nearest of each kind */
	float itemAmmoLevel;		/* 0x378 the ammo of the kind of the item when it was found */
	float itemAmmoStart;		/* 0x37c the ammo when the AI went for the item */
	cEntity *itemAmmoEnt[3];	/* 0x380 */
	cEntity *itemAmmoBest;		/* 0x38c */
	float itemAmmoDistK[3];		/* 0x390 */
	int unused_39c[3];			/* 0x39c */
	byte unused_3a8;			/* 0x3a8 */
	byte unused_3a9[2];			/* 0x3a9 */
	byte pad_3ab;
	int ammoSearch;			/* 0x3ac the nearest ammo first (0) or the farthest (1), 3 none */
};
SD_OFFSET(cAIVehicle, slotThink, 0x10);
SD_OFFSET(cAIVehicle, target, 0x38);
SD_OFFSET(cAIVehicle, repos, 0x86);
SD_OFFSET(cAIVehicle, pending, 0x90);
SD_OFFSET(cAIVehicle, onArrive, 0x15c);
SD_OFFSET(cAIVehicle, vehicle, 0x190);
SD_OFFSET(cAIVehicle, params, 0x1b4);
SD_OFFSET(cAIVehicle, saved, 0x20c);
SD_OFFSET(cAIVehicle, track, 0x2d8);
SD_OFFSET(cAIVehicle, maxDistRepos, 0x2f4);
SD_OFFSET(cAIVehicle, trackType, 0x31c);
SD_OFFSET(cAIVehicle, itemLife, 0x344);
SD_OFFSET(cAIVehicle, itemAmmo, 0x358);
SD_OFFSET(cAIVehicle, itemAmmoBest, 0x38c);
SD_OFFSET(cAIVehicle, unused_3a8, 0x3a8);
SD_SIZE(cAIVehicle, CAIVEHICLE_SIZE);

static inline cEntity *AIV_Target(const cAIVehicle *ai) {
	return ai->target.target;
}

/* the engine variables of the vehicle AI */
#define g_MaxDistReposAI GAME_GLOBAL_IN(float, GVar_MaxDistReposAI, 16, 0x444858)
#define g_MinDistReposAI GAME_GLOBAL_IN(float, GVar_MinDistReposAI, 16, 0x444900)
#define g_MaxDistReposNotVisAI GAME_GLOBAL_IN(float, GVar_MaxDistReposNotVisAI, 16, 0x44478c)
#define g_MinDistReposNotVisAI GAME_GLOBAL_IN(float, GVar_MinDistReposNotVisAI, 16, 0x4447a4)
#define g_TimeDamageAI GAME_GLOBAL_IN(float, GVar_TimeDamageAI, 16, 0x444990)
#define g_HookRange GAME_GLOBAL_IN(float, GVar_HookRange, 16, 0x449d24)
#define g_OnObjectiveNullAI (&GAME_GLOBAL_IN(cPy_Function, GVar_OnObjectiveNullAI, 16, 0x4447d4))
#define g_OnItemTakenAI (&GAME_GLOBAL_IN(cPy_Function, GVar_OnItemTakenAI, 16, 0x4449c0))

/* ---- AI/AIVehicle.cpp (src/game/AI/IVehicle.c); more in ai.h, nodestraffic.h, embeddedfxvehicle.h, weapons.h,
 * vehicletraffic.h and vehicleentity.h (the constructor) ---- */
void __fastcall cAIVehicle_dtor(cAIVehicle *self);
cAIVehicle *__thiscall cAIVehicle_Delete(cAIVehicle *self, byte flags);
/* the boost flag of the vehicle from CONTROL_AIM (after the parameters changed) */
void __thiscall cAIVehicle_ApplyParams(cAIVehicle *self);
void __thiscall cAIVehicle_React(cAIVehicle *self);
byte __thiscall cAIVehicle_Think(cAIVehicle *self);
void __thiscall cAIVehicle_ThinkHook(cAIVehicle *self);
void __thiscall cAIVehicle_CheckItems(cAIVehicle *self);
void __thiscall cAIVehicle_UpdateGraph(cAIVehicle *self);
void __thiscall cAIVehicle_ReposIfFar(cAIVehicle *self);
void __thiscall cAIVehicle_UpdateRoute(cAIVehicle *self);
void __thiscall cAIVehicle_UpdateThink(cAIVehicle *self);
byte __thiscall cAIVehicle_CheckTarget(cAIVehicle *self);
void __thiscall cAIVehicle_Draw(cAIVehicle *self);
void __thiscall cAIVehicle_Update(cAIVehicle *self, float dt);
void __thiscall cAIVehicle_UpdateOff(cAIVehicle *self, float dt);
byte __thiscall cAIVehicle_VarF(cAIVehicle *self, const char *name, double *d, byte set);
byte __thiscall cAIVehicle_VarI(cAIVehicle *self, const char *name, int *i, byte set);
byte __thiscall cAIVehicle_VarS(cAIVehicle *self, const char *name, char **s, byte set);
byte __thiscall cAIVehicle_VarV(cAIVehicle *self, const char *name, double *x, double *y, double *z, byte set);
cAIVehicleControl *__thiscall cAIVehicle_GetControl(cAIVehicle *self);
byte __thiscall cAIVehicle_GetChanged(cAIVehicle *self);
int __thiscall cAIVehicle_GetPrevState(cAIVehicle *self);
/* FIGHTING when asked with GET_AWAY, else the state of the AI */
int __thiscall cAIVehicle_GetStateFor(cAIVehicle *self, int state);
/* the name of the state into buf; buf unchanged for a state out of the range */
void __stdcall cAIVehicle_InternStateName(int state, char *buf);
void __thiscall cAIVehicle_GetInternStateName(cAIVehicle *self, char *buf);
void __stdcall cAIVehicle_StateName(int state, char *buf);
void __thiscall cAIVehicle_GetStateName(cAIVehicle *self, char *buf);
cAIVehicleParams1 *__thiscall cAIVehicle_GetParamsInit(cAIVehicle *self);
cAIVehicleParams2 *__thiscall cAIVehicle_GetParams2Init(cAIVehicle *self);
cAIVehicleParams3 *__thiscall cAIVehicle_GetParams3Init(cAIVehicle *self);
/* the vehicle closer than 15000 to p */
byte __thiscall cAIVehicle_IsNear(cAIVehicle *self, const vec3_t *p);
byte __thiscall cAIVehicle_ControlFlag(cAIVehicle *self);
byte __thiscall cAIVehicle_InSight(cAIVehicle *self);
/* e within HookRange, not touching and at most 45 degrees off the nose; CanHook also in sight */
byte __thiscall cAIVehicle_InHookRange(cAIVehicle *self, cEntity *e);
byte __thiscall cAIVehicle_CanHook(cAIVehicle *self, cEntity *e);
byte __thiscall cAIVehicle_InHookRangePos(cAIVehicle *self, const vec3_t *p);
/* a change to the state asks the vehicle group (not FREEZE*, LOST_PATH*, TRAFFIC*) */
byte __stdcall cAIVehicle_IsGroupState(int state);
void __thiscall cAIVehicle_CheckParams1(cAIVehicle *self, const cAIVehicleParams1 *p);
void __thiscall cAIVehicle_CheckParams2(cAIVehicle *self, const cAIVehicleParams2 *p);
void __thiscall cAIVehicle_CheckParams3(cAIVehicle *self, const cAIVehicleParams3 *p);
byte __thiscall cAIVehicle_IsMoving(cAIVehicle *self);
void cAIVehicle_Debugf(cAIVehicle *self, const char *fmt, ...);
byte __thiscall cAIVehicle_Event(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventChase(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventDefault(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventFighting(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventGetAway(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventGetAwayStop(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventGoTo(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventInRoute(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventLostPath(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventTraffic(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_EventTrafficInDoor(cAIVehicle *self, int ev);
byte __thiscall cAIVehicle_PlaceOnTrackStart(cAIVehicle *self, int track);
void __thiscall cAIVehicle_SaveState(cAIVehicle *self, cAIVehicleSaved *s);
/* the point where the segment a b crosses the plane: 1, out set; 0 if both ends are on one side */
int SegmentPlane_Hit(vec3_t *out, const vec3_t *a, const vec3_t *b, const plane_t *pl);
/* the turn speeds (rotA, rotB) for a track speed */
void __thiscall cAIVehicle_SetRotSpeed(cAIVehicle *self, float speed);
/* a kind of traffic track for the class of the vehicle, at random among the kinds of the graph; 5 none */
void __thiscall cAIVehicle_PickTrackType(cAIVehicle *self);
byte __thiscall cAIVehicle_RestoreState(cAIVehicle *self, const cAIVehicleSaved *s);
/* the change of state from inside the AI (the group lets the next one through at once) */
byte cAIVehicle_SetStateIntern(cAIVehicle *self, int state, ...);
byte __thiscall cAIVehicle_RestoreStateIntern(cAIVehicle *self, const cAIVehicleSaved *s);
/* the nearest item entity of the life list or of the ammo kind that is there and not exclude */
cEntity *__thiscall cAIVehicle_NearestLife(cAIVehicle *self, float *dist, cItemDesc *out, cEntity *exclude);
cEntity *__thiscall cAIVehicle_NearestAmmo(cAIVehicle *self, int kind, float *dist, cItemDesc *out,
	cEntity *exclude);
void __thiscall cAIVehicle_FindLife(cAIVehicle *self, cEntity *exclude);
int __thiscall cAIVehicle_GetTrackType(cAIVehicle *self);
/* the reactions of the think slot, by state */
void __thiscall cAIVehicle_ReactFighting(cAIVehicle *self);
void __thiscall cAIVehicle_ReactHook(cAIVehicle *self);
void __thiscall cAIVehicle_ReactGetAway(cAIVehicle *self);
void __thiscall cAIVehicle_ReactGetAwayStop(cAIVehicle *self);
/* LOST_PATH and LOST_PATH_CHASE (two copies): FREEZE_LOST when the place of the vehicle is free after 15 s */
void __thiscall cAIVehicle_ReactLostPath(cAIVehicle *self);
/* the hook of the states that move: aimed or let go on the hook slot */
void __thiscall cAIVehicle_ThinkChase(cAIVehicle *self);
void __thiscall cAIVehicle_ThinkFighting(cAIVehicle *self);
void __thiscall cAIVehicle_ThinkGetAway(cAIVehicle *self);
void __thiscall cAIVehicle_ThinkGetAwayStop(cAIVehicle *self);
void __thiscall cAIVehicle_ThinkGoTo(cAIVehicle *self);
/* LOST_PATH and LOST_PATH_CHASE (two copies): back to the kept state once the vehicle is in the graph again */
void __thiscall cAIVehicle_ThinkLostPath(cAIVehicle *self);
/* the hook point: tried when the vehicle moves, let go near the goal or out of range; 1 while it is set */
byte __thiscall cAIVehicle_TryHook(cAIVehicle *self, int a, int b);
/* the life items (and the ammo items for FIGHTING) the states go for */
void __thiscall cAIVehicle_ItemsChase(cAIVehicle *self);
void __thiscall cAIVehicle_ItemsFighting(cAIVehicle *self);
void __thiscall cAIVehicle_ItemsGetAway(cAIVehicle *self);
void __thiscall cAIVehicle_ItemsGetAwayStop(cAIVehicle *self);
void __thiscall cAIVehicle_ItemsInRoute(cAIVehicle *self);
void __thiscall cAIVehicle_FightingAfterItem(cAIVehicle *self);
/* the nearest ammo item of a kind the shoot control uses (kinds 0, 2, 1); itemAmmoLevel the ammo of that kind */
cEntity *__thiscall cAIVehicle_NearestAmmoAny(cAIVehicle *self, float *dist, cItemDesc *out, cEntity *exclude,
	float factor);
/* itemAmmoBest: the nearest ammo item, with a random factor first (0 or FLT_MAX), the other one when none */
void __thiscall cAIVehicle_FindAmmo(cAIVehicle *self, cEntity *exclude);
void __thiscall cAIVehicle_GraphTrafficInDoor(cAIVehicle *self);
void __thiscall cAIVehicle_ThinkRace(cAIVehicle *self);
/* the internal state (AIVI_) and its move; the arguments by state, floats as double, bytes as int:
 * STOPPED, ORIENT_SHOOTING: none
 * FINISHING_LINE: vec3_t dest, speed, boost, a, b, accel, stopAtGoal
 * FINISHING_LINE_WITH_SHOOT_CM, _LIFE, _AMMO: vec3_t dest, speed (WITH_SHOOT_CM also boost)
 * FINISHING_LINE_DIRECT: vec3_t dest, speed, boost, rotA, rotB
 * CHASE_SHOOTING: chaseMin, chaseMax; CHASE_NOT_SHOOTING: chaseMin, rotB
 * FOLLOW_THE_PATH: vec3_t dir, speed, boost, flat, repos, a, b; FOLLOW_THE_PATH_WITH_SHOOT_CM: vec3_t dir, speed,
 * boost, flat */
void cAIVehicle_SetInternState(cAIVehicle *self, int state, ...);
/* the external state from SetState: 0 if the arguments did not fit */
byte __thiscall cAIVehicle_ChangeState(cAIVehicle *self, int state, va_list args);
byte __thiscall cAIVehicle_ChangeStateMore(cAIVehicle *self, int state, byte allowed, va_list args);
/* the vehicle too far from the camera for its view (the repos distances reset to the engine variables) */
byte __thiscall cAIVehicle_NeedRepos(cAIVehicle *self);

/* ---- the repositioning and the track points ---- */
/* the repositioning near the camera */
byte __thiscall cAIVehicle_Repos(cAIVehicle *self);
void __thiscall cAIVehicle_GraphTraffic(cAIVehicle *self);
/* a line from the vehicle to a point at HookRange beside it hits the scene; the hit point in p */
byte __thiscall cAIVehicle_TestHookPoint(cAIVehicle *self, vec3_t *p);
/* the point of the track node for the vehicle (out returned); Random with a random offset kept in trackOfs */
vec3_t *__thiscall cAIVehicle_TrackPoint(cAIVehicle *self, vec3_t *out, const vec3_t *p, int track, int node);
vec3_t *__thiscall cAIVehicle_TrackPointRandom(cAIVehicle *self, vec3_t *out, const vec3_t *p, int track,
	int node);
/* a point of a traffic track near the camera for the vehicle (in view between minVis and maxVis, else between
 * minNotVis and maxNotVis, around the point); track and node set; 0 for none */
byte __thiscall cAIVehicle_ReposTraffic(cAIVehicle *self, vec3_t *out, float minVis, float maxVis, float minNotVis,
	float maxNotVis, float around);

/* ---- cAIMove (AI/AIMove.cpp), the base class ---- */
void *__thiscall cAIMove_ctor(void *self, cEntity *e);
void __fastcall cAIMove_dtor(void *self);
void __thiscall cAIMove_Draw(void *self);
void __thiscall cAIMove_Update(void *self, float dt);
void __thiscall cAIMove_UpdateOff(void *self, float dt);
byte __thiscall cAIMove_VarF(void *self, const char *name, double *d, byte set);
byte __thiscall cAIMove_VarI(void *self, const char *name, int *i, byte set);
byte __thiscall cAIMove_VarS(void *self, const char *name, char **s, byte set);
byte __thiscall cAIMove_VarV(void *self, const char *name, double *x, double *y, double *z, byte set);
/* the rest of AI/AIMove.cpp (src/game/AI/AIMove.c; the slots of cAIChar, include/scrap/ai.h) */
void *__fastcall cAIMove_ctorDefault(void *self);
void *__thiscall cAIMove_Delete(void *self, byte flags);
void __thiscall cAIMove_CheckStopped(void *self, float dt);
byte __thiscall cAIMove_CanJump(void *self);
cEntity *__thiscall cAIMove_GetTargetEnt(void *self);
Nto1Node *__thiscall cAIMove_GetTarget(void *self);
byte __thiscall cAIMove_GetStopped(void *self);
byte __thiscall cAIMove_GetArrived(void *self);
byte __thiscall cAIMove_IsPaused(void *self);
byte __thiscall cAIMove_Event(void *self, int ev);
/* e there and active, else a warning */
byte __thiscall cAIMove_CanTarget(void *self, cEntity *e);
vec3_t *__thiscall cAIMove_RandomPoint(void *self, vec3_t *out, const vec3_t *p, float r);

/* ---- cAIVehicleControl and cAIVehicleShoot (src/game/AI/IVehicleControl.c, IVehicleShootControl.c; the rest of
 * them in aivehiclecontrol.h) ---- */
cAIVehicleControl *__thiscall cAIVehicleControl_ctor(void *mem, cEntity *v, cAIVehicle *ai, void *params);
void __thiscall cAIVehicleControl_ApplyParams(cAIVehicleControl *self);
void __thiscall cAIVehicleControl_Update(cAIVehicleControl *self, float dt);
void __thiscall cAIVehicleControl_Draw(cAIVehicleControl *self);
cAIVehicleParams2 *__thiscall cAIVehicleControl_GetParams(cAIVehicleControl *self);
cAIVehicleParams2 *__thiscall cAIVehicleControl_GetParamsInit(cAIVehicleControl *self);
byte __thiscall cAIVehicleControl_GetFlag(cAIVehicleControl *self);
byte __thiscall cAIVehicleControl_InSight(cAIVehicleControl *self);
void __thiscall cAIVehicleControl_SetInertia(cAIVehicleControl *self, int on);
void __thiscall cAIVehicleControl_SetRotStatic(cAIVehicleControl *self, float a, float b);
/* the last point (0xc4) and the point of the target (0x98, -FLT_MAX each after ClearDest) */
vec3_t *__thiscall cAIVehicleControl_GetGoal(cAIVehicleControl *self, vec3_t *out);
vec3_t *__thiscall cAIVehicleControl_GetDest(cAIVehicleControl *self, vec3_t *out);
void __thiscall cAIVehicleControl_ClearDest(cAIVehicleControl *self);
/* the controls of the vehicle let go */
void __thiscall cAIVehicleControl_Stop(cAIVehicleControl *self);
/* the move of the control (mode 0 stop, 1 to a point, 2 direct, 3 chase, 6 orient, 7 path; the arguments by mode,
 * see cAIVehicle_SetInternState); 0 if it was refused */
byte cAIVehicleControl_SetMove(cAIVehicleControl *self, int mode, ...);
/* the byte at 0xb0: 1 in LOST_PATH, 0 at every change of state */
void __thiscall cAIVehicleControl_SetDirect(cAIVehicleControl *self, byte on);
cAIVehicleShoot *__thiscall cAIVehicleShoot_ctor(void *mem, cEntity *v, cAIVehicle *ai, void *params);
void __thiscall cAIVehicleShoot_Update(cAIVehicleShoot *self, float dt);
void __thiscall cAIVehicleShoot_Update2(cAIVehicleShoot *self, float dt);
void __thiscall cAIVehicleShoot_Draw(cAIVehicleShoot *self);
cAIVehicleParams3 *__thiscall cAIVehicleShoot_GetParams(cAIVehicleShoot *self);
cAIVehicleParams3 *__thiscall cAIVehicleShoot_GetParamsInit(cAIVehicleShoot *self);
void __thiscall cAIVehicleShoot_WeapListChanged(cAIVehicleShoot *self);
/* the vehicle has the hook (byte at 0x61) */
byte __thiscall cAIVehicleShoot_HasHook(cAIVehicleShoot *self);
/* the ammo of the weapons in use above factor of the maximum */
byte __thiscall cAIVehicleShoot_HasAmmo(cAIVehicleShoot *self, float factor);
/* the vehicle has a weapon for the ammo kind (the ones of the factor below a limit or above) */
byte __thiscall cAIVehicleShoot_UsesAmmo(cAIVehicleShoot *self, int kind, float factor);
byte __thiscall cAIVehicleShoot_TargetInSight(cAIVehicleShoot *self, int arg);
byte __thiscall cAIVehicleShoot_SetMode(cAIVehicleShoot *self, int a, int b, byte c);
/* the fields at 0x124, 0x128 (2 in a race at the finishing line, assumed) and 0x178 */
void __thiscall cAIVehicleShoot_GetInfo(cAIVehicleShoot *self, int *a, int *kind, byte *flag);

/* ---- Global/Nto1Links.h: the out of line copies the AI calls ---- */
/* node out of its ring (CHECK: Node, its data set) */
void __fastcall Nto1Node_UnlinkChecked(Nto1Node *node);

/* ---- cAIVehicleGroup ---- */
/* an elevator of the group within dist of pos */
byte __thiscall cAIVehicleGroup_ElevatorNear(cAIVehicleGroup *self, const vec3_t *pos, float dist);

#endif
