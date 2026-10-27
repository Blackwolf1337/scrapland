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
#include "scrap/cstring.h"
#include "scrap/nodestraffic.h"
#include "scrap/pyfunction.h"
#include "scrap/slotcontroller.h"

#define CAIVEHICLE_VFTABLE 0x3a8448
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

/* the move control and the shoot control (AI/IVehicleControl.cpp and its neighbours, not in C yet) */
typedef struct cAIObjVtbl {
	void *(__thiscall *Delete)(void *self, byte flags);	/* 0x00 */
} cAIObjVtbl;
typedef struct cAIVehicleControl {
	const cAIObjVtbl *vftable;
} cAIVehicleControl;
typedef struct cAIVehicleShoot {
	const cAIObjVtbl *vftable;
} cAIVehicleShoot;

/* a state with its arguments: the one to take when the vehicle group lets it (pending) and the one kept by
 * cAIVehicle_SaveState to come back to after an event (saved) */
typedef struct cAIVehicleSaved {
	int state;			/* 0x00 */
	int internState;		/* 0x04 */
	vec3_t dest;			/* 0x08 */
	float speed;			/* 0x14 */
	byte b18;			/* 0x18 */
	char target[0x9c - 0x19];	/* 0x19 the name of the target entity, "" for none */
	vec3_t dest2;			/* 0x9c */
	float f2f0;			/* 0xa8 */
	byte b20;			/* 0xac */
	byte pad_ad[3];
	float f30c;			/* 0xb0 */
	byte repos;			/* 0xb4 */
	byte pad_b5[3];
	float chaseMin;			/* 0xb8 */
	float chaseMax;			/* 0xbc */
	byte b32c;			/* 0xc0 */
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
	byte(__thiscall *Vf05)(cAIVehicle *self);						/* 0x14 */
	void (*Debugf)(cAIVehicle *self, const char *fmt, ...);				/* 0x18 */
	byte(__thiscall *VarI)(cAIVehicle *self, const char *name, int *i, byte set);	/* 0x1c */
	void *vf08_0c[5];
	void(__thiscall *OnEvent)(cAIVehicle *self, int ev);				/* 0x34 the debug text of the event */
	void *vf0e_15[8];
	void(__thiscall *Event)(cAIVehicle *self, int ev);				/* 0x58 vf22 */
	byte(__thiscall *IsMoving)(cAIVehicle *self);					/* 0x5c vf23 */
	byte(__thiscall *CanTarget)(cAIVehicle *self, cEntity *e);			/* 0x60 */
} cAIVehicleVtbl;
SD_OFFSET(cAIVehicleVtbl, Debugf, 0x18);
SD_OFFSET(cAIVehicleVtbl, OnEvent, 0x34);
SD_OFFSET(cAIVehicleVtbl, Event, 0x58);
SD_OFFSET(cAIVehicleVtbl, CanTarget, 0x60);

/* a cSlotController inside the AI (0x10 bytes, AIV_Slot) */
typedef struct cAIVSlot {
	byte raw[CSLOTCONTROLLER_SIZE];
} cAIVSlot;

static inline cSlotController *AIV_Slot(cAIVSlot *s) {
	return (cSlotController *)s->raw;
}

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
	float f30;			/* 0x030 */
	int i34;			/* 0x034 */
	Nto1Node target;		/* 0x038 the target entity at 0x40 */
	byte pad_48[0x4c - 0x48];
	vec3_t dest;			/* 0x04c */
	float speed;			/* 0x058 */
	int moveMode;			/* 0x05c */
	byte pad_60[0x64 - 0x60];
	vec3_t dest2;			/* 0x064 */
	byte pad_70[0x86 - 0x70];
	byte repos;			/* 0x086 "Repos" */
	byte pad_87[0x90 - 0x87];
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
	byte b1a8;			/* 0x1a8 */
	byte pad_1a9[3];
	int lifeMark;			/* 0x1ac the life of the vehicle at the last damage check */
	float damageTime;		/* 0x1b0 */
	cAIVehicleParams1 params;	/* 0x1b4 vBehav */
	cAIVehicleParams1 paramsInit;	/* 0x1e0 */
	cAIVehicleSaved saved;		/* 0x20c */
	int track;			/* 0x2d8 */
	int node;			/* 0x2dc */
	byte b2e0;			/* 0x2e0 */
	byte pad_2e1[3];
	vec3_t trackOfs;		/* 0x2e4 */
	float f2f0;			/* 0x2f0 */
	float maxDistRepos;		/* 0x2f4 */
	float minDistRepos;		/* 0x2f8 */
	float maxDistReposNotVis;	/* 0x2fc */
	float minDistReposNotVis;	/* 0x300 */
	float rotA;			/* 0x304 */
	float rotB;			/* 0x308 */
	float f30c;			/* 0x30c */
	float chaseMin;			/* 0x310 */
	float chaseMax;			/* 0x314 */
	byte b318;			/* 0x318 */
	byte pad_319[3];
	int trackType;			/* 0x31c 0..4, 5 none */
	vec3_t trackPos;		/* 0x320 */
	byte b32c;			/* 0x32c */
	byte pad_32d[3];
	float f330;			/* 0x330 */
	int i334;			/* 0x334 */
	int hookMode;			/* 0x338 */
	int hookSaved;			/* 0x33c */
	byte b340;			/* 0x340 */
	byte pad_341[3];
	cItemDesc itemLife;		/* 0x344 the nearest life item */
	cEntity *itemLifeEnt;		/* 0x34c */
	int itemLifeMark;		/* 0x350 */
	byte b354;			/* 0x354 */
	byte pad_355[3];
	cItemDesc itemAmmo;		/* 0x358 the nearest ammo item */
	cItemDesc itemAmmoK[3];		/* 0x360 the nearest of each kind */
	float itemAmmoDist;		/* 0x378 */
	float f37c;			/* 0x37c */
	cEntity *itemAmmoEnt[3];	/* 0x380 */
	cEntity *itemAmmoBest;		/* 0x38c */
	float itemAmmoDistK[3];		/* 0x390 */
	int i39c[3];			/* 0x39c */
	byte b3a8;			/* 0x3a8 */
	byte b3a9[2];			/* 0x3a9 */
	byte pad_3ab;
	int i3ac;			/* 0x3ac */
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
SD_OFFSET(cAIVehicle, b3a8, 0x3a8);
SD_SIZE(cAIVehicle, CAIVEHICLE_SIZE);

static inline cEntity *AIV_Target(const cAIVehicle *ai) {
	return ai->target.target;
}

/* the engine variables of the vehicle AI */
#define g_MaxDistReposAI GAME_DATA(float, 0x444858)
#define g_MinDistReposAI GAME_DATA(float, 0x444900)
#define g_MaxDistReposNotVisAI GAME_DATA(float, 0x44478c)
#define g_MinDistReposNotVisAI GAME_DATA(float, 0x4447a4)
#define g_TimeDamageAI GAME_DATA(float, 0x444990)
#define g_HookRange GAME_DATA(float, 0x449d24)
#define g_OnObjectiveNullAI (&GAME_DATA(cPy_Function, 0x4447d4))
#define g_OnItemTakenAI (&GAME_DATA(cPy_Function, 0x4449c0))

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
/* the state an event of the state ev counts in: FIGHTING for GET_AWAY */
int __thiscall cAIVehicle_GetStateFor(cAIVehicle *self, int ev);
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
byte __thiscall cAIVehicle_ControlFlag2(cAIVehicle *self);
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
void __thiscall cAIVehicle_Event(cAIVehicle *self, int ev);
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

/* ---- the parts of AIVehicle.cpp not in C yet ---- */
/* the reactions of the think slot, by state */
void __thiscall cAIVehicle_ReactFighting(cAIVehicle *self);		/* rva 0x86d90 */
void __thiscall cAIVehicle_ReactHook(cAIVehicle *self);		/* rva 0x86eb0 */
void __thiscall cAIVehicle_ReactGetAway(cAIVehicle *self);		/* rva 0x870e0 */
void __thiscall cAIVehicle_ReactGetAwayStop(cAIVehicle *self);	/* rva 0x87260 */
void __thiscall cAIVehicle_ReactLostPath(cAIVehicle *self);		/* rva 0x87580 */
void __thiscall cAIVehicle_ReactLostPathChase(cAIVehicle *self);	/* rva 0x87600 */
/* the moves of the states */
void __thiscall cAIVehicle_ThinkChase(cAIVehicle *self);		/* rva 0x87850 */
void __thiscall cAIVehicle_ThinkFighting(cAIVehicle *self);		/* rva 0x87a00 */
void __thiscall cAIVehicle_ThinkGetAway(cAIVehicle *self);		/* rva 0x87bf0 */
void __thiscall cAIVehicle_ThinkGetAwayStop(cAIVehicle *self);	/* rva 0x87d20 */
void __thiscall cAIVehicle_ThinkGoTo(cAIVehicle *self);		/* rva 0x87e60 */
void __thiscall cAIVehicle_ThinkLostPath(cAIVehicle *self);		/* rva 0x87f80 */
void __thiscall cAIVehicle_ThinkLostPathChase(cAIVehicle *self);	/* rva 0x88070 */
void __thiscall cAIVehicle_ThinkRace(cAIVehicle *self);		/* rva 0x88160 */
/* the items the states go for */
void __thiscall cAIVehicle_ItemsChase(cAIVehicle *self);		/* rva 0x885d0 */
void __thiscall cAIVehicle_ItemsFighting(cAIVehicle *self);		/* rva 0x88790 */
void __thiscall cAIVehicle_ItemsGetAway(cAIVehicle *self);		/* rva 0x88ae0 */
void __thiscall cAIVehicle_ItemsGetAwayStop(cAIVehicle *self);	/* rva 0x88c20 */
void __thiscall cAIVehicle_ItemsInRoute(cAIVehicle *self);		/* rva 0x88d70 */
void __thiscall cAIVehicle_FightingAfterItem(cAIVehicle *self);	/* rva 0x86ad0 */
/* the repositioning near the camera */
byte __thiscall cAIVehicle_NeedRepos(cAIVehicle *self);		/* rva 0x868f0 */
byte __thiscall cAIVehicle_Repos(cAIVehicle *self);			/* rva 0x89b00 */
void __thiscall cAIVehicle_GraphTraffic(cAIVehicle *self);		/* rva 0x89070 */
void __thiscall cAIVehicle_GraphTrafficInDoor(cAIVehicle *self);	/* rva 0x89630 */
/* the point of the track node for the vehicle (out returned); Random with a random offset kept in trackOfs */
vec3_t *__thiscall cAIVehicle_TrackPoint(cAIVehicle *self, vec3_t *out, const vec3_t *p, int track, int node);
vec3_t *__thiscall cAIVehicle_TrackPointRandom(cAIVehicle *self, vec3_t *out, const vec3_t *p, int track,
	int node);
/* the internal state and its move (the arguments by state) */
void cAIVehicle_SetInternState(cAIVehicle *self, int state, ...);			/* rva 0x8e2d0 */
/* the external state from SetState: 0 if the arguments did not fit */
byte __thiscall cAIVehicle_ChangeState(cAIVehicle *self, int state, va_list args);	/* rva 0x8ea60 */

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

/* ---- cAIVehicleControl and cAIVehicleShoot (AI/IVehicleControl.cpp ..) ---- */
cAIVehicleControl *__thiscall cAIVehicleControl_ctor(void *mem, cEntity *v, cAIVehicle *ai, void *params);
void __thiscall cAIVehicleControl_ApplyParams(cAIVehicleControl *self);
void __thiscall cAIVehicleControl_Update(cAIVehicleControl *self, float dt);
void __thiscall cAIVehicleControl_Draw(cAIVehicleControl *self);
cAIVehicleParams2 *__thiscall cAIVehicleControl_GetParams(cAIVehicleControl *self);
cAIVehicleParams2 *__thiscall cAIVehicleControl_GetParamsInit(cAIVehicleControl *self);
byte __thiscall cAIVehicleControl_GetFlag(cAIVehicleControl *self);
byte __thiscall cAIVehicleControl_GetFlag2(cAIVehicleControl *self);
void __thiscall cAIVehicleControl_SetInertia(cAIVehicleControl *self, int on);
void __thiscall cAIVehicleControl_SetRotStatic(cAIVehicleControl *self, float a, float b);
/* the last point (0xc4) and the point of the target (0x98, -FLT_MAX each after ClearDest) */
vec3_t *__thiscall cAIVehicleControl_GetGoal(cAIVehicleControl *self, vec3_t *out);
vec3_t *__thiscall cAIVehicleControl_GetDest(cAIVehicleControl *self, vec3_t *out);
void __thiscall cAIVehicleControl_ClearDest(cAIVehicleControl *self);
/* the controls of the vehicle let go */
void __thiscall cAIVehicleControl_Stop(cAIVehicleControl *self);
cAIVehicleShoot *__thiscall cAIVehicleShoot_ctor(void *mem, cEntity *v, cAIVehicle *ai, void *params);
void __thiscall cAIVehicleShoot_Update(cAIVehicleShoot *self, float dt);
void __thiscall cAIVehicleShoot_Update2(cAIVehicleShoot *self, float dt);
void __thiscall cAIVehicleShoot_Draw(cAIVehicleShoot *self);
cAIVehicleParams3 *__thiscall cAIVehicleShoot_GetParams(cAIVehicleShoot *self);
cAIVehicleParams3 *__thiscall cAIVehicleShoot_GetParamsInit(cAIVehicleShoot *self);
void __thiscall cAIVehicleShoot_WeapListChanged(cAIVehicleShoot *self);

#endif
