/*
 * AI (original source: AI/): navigation graphs, traffic, A*, AI groups and the AI controllers of vehicles and
 * characters. The graph objects are made by SAI.IniAI with the engine's operator new; their globals keep the
 * names of the CHECK expressions in PyInterface/SAIPy.cpp.
 */
#ifndef SCRAP_AI_H
#define SCRAP_AI_H

#include <stdarg.h>

#include "scrap/gvar.h"
#include "scrap/entity.h"
#include "scrap/nodesworld.h"
#include "scrap/python.h"
#include "scrap/slotcontroller.h"

typedef struct CNodesWorld CNodesWorld;		/* RTTI CNodesWorld: visibility graph of the level */
typedef struct CNodesWorld2D CNodesWorld2D;
typedef struct cAIVehicleGroup cAIVehicleGroup;
typedef struct cAICharGroup cAICharGroup;
typedef struct cAIVehicle cAIVehicle;
typedef struct cAIChar cAIChar;

/* CNodesTraffic or CNodesTrafficInDoor */
typedef struct CNodesTraffic CNodesTraffic;	/* include/scrap/nodestraffic.h */

#define CAIENTITYGROUP_MAX 20
typedef struct cAIEntityGroup cAIEntityGroup;

typedef struct cAIEntityGroupVtbl {
	cAIEntityGroup *(*Delete)(cAIEntityGroup *self, byte flags);	/* 0x00 */
	void(*Clear)(cAIEntityGroup *self);				/* 0x04 no entities */
	byte(*Add)(cAIEntityGroup *self, cEntity *e);			/* 0x08 0 when full */
	cEntity *(*Get)(cAIEntityGroup *self, int i);			/* 0x0c */
	int(*Num)(cAIEntityGroup *self);				/* 0x10 */
	byte(*CanChange)(cAIEntityGroup *self);			/* 0x14 */
	void(*ResetChange)(cAIEntityGroup *self);			/* 0x18 the time to 0 */
	void(*AllowChange)(cAIEntityGroup *self);			/* 0x1c the time to the limit */
	void(*Update)(cAIEntityGroup *self, float dt);		/* 0x20 the time on */
} cAIEntityGroupVtbl;

/* calls through the slots of cAIEntityGroupVtbl */
static inline cAIEntityGroup *AIGroup_Delete(void *self, byte flags) {
	return VT(cAIEntityGroupVtbl, self)->Delete(self, flags);
}

static inline byte AIGroup_Add(void *self, cEntity *e) {
	return VT(cAIEntityGroupVtbl, self)->Add(self, e);
}

static inline void AIGroup_AllowChange(void *self) {
	VT(cAIEntityGroupVtbl, self)->AllowChange(self);
}

static inline byte AIGroup_CanChange(void *self) {
	return VT(cAIEntityGroupVtbl, self)->CanChange(self);
}

static inline void AIGroup_Clear(void *self) {
	VT(cAIEntityGroupVtbl, self)->Clear(self);
}

static inline void AIGroup_Update(void *self, float dt) {
	VT(cAIEntityGroupVtbl, self)->Update(self, dt);
}

static inline cEntity *AIGroup_Get(void *self, int i) {
	return VT(cAIEntityGroupVtbl, self)->Get(self, i);
}

static inline int AIGroup_Num(void *self) {
	return VT(cAIEntityGroupVtbl, self)->Num(self);
}

static inline void AIGroup_ResetChange(void *self) {
	VT(cAIEntityGroupVtbl, self)->ResetChange(self);
}

struct cAIEntityGroup {
	const cAIEntityGroupVtbl *vftable;	/* 0x00 */
	cEntity *ents[CAIENTITYGROUP_MAX];	/* 0x04 */
	int num;				/* 0x54 */
	byte unused_58;				/* 0x58 */
	byte pad_59[3];
	float changeTime;			/* 0x5c */
};

struct cAIVehicleGroup {
	cAIEntityGroup base;	/* 0x00 the vehicles; changeTime: the time since the last change of state */
	/* the vehicle AIs in the internal state STOPPED: all, the traffic by class (2, 1, 0) and IN_ROUTE */
	int numStopped;			/* 0x60 */
	int numStoppedHeavy;		/* 0x64 */
	int numStoppedMedium;		/* 0x68 */
	int numStoppedLight;		/* 0x6c */
	int numStoppedRoute;		/* 0x70 */
	vec3_t elevators[10];		/* 0x74 the elevator and metro doors of the level */
	int numElevators;		/* 0xec */
	float reposTime;		/* 0xf0 the world time of the last repositioning */
};

/* ---- cAI (AI/AI.cpp, src/game/AI/AI.c): the base of every AI (0x38 bytes), the entity, the time of the frame and
 * of the state, and the slot of the reactions ---- */

typedef struct cAI cAI;

/* the slots of cAI::vftable (Debugf is pure there) */
typedef struct cAIBaseVtbl {
	cAI *(*Delete)(cAI *self, byte flags);					/* 0x00 */
	void(*Update)(cAI *self, float dt);						/* 0x04 */
	void(*UpdateOff)(cAI *self, float dt);					/* 0x08 */
	void(*Draw)(cAI *self);							/* 0x0c */
	byte(*GetChanged)(cAI *self);						/* 0x10 */
	byte(*IsVisible)(cAI *self);							/* 0x14 */
	void (*Debugf)(cAI *self, const char *fmt, ...);					/* 0x18 text of the AI debug view */
	byte(*VarI)(cAI *self, const char *name, int *i, byte set);			/* 0x1c */
	byte(*VarF)(cAI *self, const char *name, double *d, byte set);		/* 0x20 */
	byte(*VarS)(cAI *self, const char *name, char **s, byte set);		/* 0x24 */
	byte(*VarV)(cAI *self, const char *name, double *x, double *y, double *z, byte set);	/* 0x28 */
	void(*Print)(cAI *self, const char *text);					/* 0x2c */
	void(*GetEventName)(cAI *self, int ev, char *out);				/* 0x30 */
	void(*OnEvent)(cAI *self, int ev);						/* 0x34 */
} cAIBaseVtbl;

/* calls through the slots of cAIBaseVtbl */
static inline cAI *AI_Delete(void *self, byte flags) {
	return VT(cAIBaseVtbl, self)->Delete(self, flags);
}

static inline byte AI_VarV(void *self, const char *name, double *x, double *y, double *z, byte set) {
	return VT(cAIBaseVtbl, self)->VarV(self, name, x, y, z, set);
}

static inline byte AI_VarS(void *self, const char *name, char **s, byte set) {
	return VT(cAIBaseVtbl, self)->VarS(self, name, s, set);
}

static inline byte AI_VarF(void *self, const char *name, double *d, byte set) {
	return VT(cAIBaseVtbl, self)->VarF(self, name, d, set);
}

static inline byte AI_VarI(void *self, const char *name, int *i, byte set) {
	return VT(cAIBaseVtbl, self)->VarI(self, name, i, set);
}

#define AI_Debugf(self, ...) (VT(cAIBaseVtbl, self)->Debugf((void *)(self), __VA_ARGS__))

static inline void AI_Draw(void *self) {
	VT(cAIBaseVtbl, self)->Draw(self);
}

static inline byte AI_GetChanged(void *self) {
	return VT(cAIBaseVtbl, self)->GetChanged(self);
}

static inline void AI_GetEventName(void *self, int ev, char *out) {
	VT(cAIBaseVtbl, self)->GetEventName(self, ev, out);
}

static inline byte AI_IsVisible(void *self) {
	return VT(cAIBaseVtbl, self)->IsVisible(self);
}

static inline void AI_OnEvent(void *self, int ev) {
	VT(cAIBaseVtbl, self)->OnEvent(self, ev);
}

static inline void AI_Update(void *self, float dt) {
	VT(cAIBaseVtbl, self)->Update(self, dt);
}

static inline void AI_UpdateOff(void *self, float dt) {
	VT(cAIBaseVtbl, self)->UpdateOff(self, dt);
}

/* the events of the AI (GetEventName) */
enum {
	AIEV_LOST_AGENT, AIEV_OBJECTIVE_NOT_FOUND, AIEV_OBJECTIVE_REACHED, AIEV_OBJECTIVE_DIED, AIEV_OBJECTIVE_NEAR,
	AIEV_OBJECTIVE_VIS, AIEV_ORIENT_REACHED, AIEV_DONT_GO, AIEV_PROBLEM_MOVE, AIEV_NO_PROBLEM, AIEV_COLLISION_AGENTS
};

/* ---- the states of the character AI (cAIChar) ---- */

/* the external states (SAI.SetStateChar, the names of cAIChar_StateName) */
enum {
	AIC_STATIC, AIC_IN_ROUTE, AIC_CHASE_WITH_ACTION, AIC_CHASE_WITHOUT_ACTION, AIC_GO_TO, AIC_GET_AWAY,
	AIC_PATROL_ZONE, AIC_CHASE_WITH_ACTION_BACK, AIC_ORIENT_DYNAMIC_OBJECTIVE, AIC_ORIENT_DYNAMIC_OBJECTIVE_WITH_ACTION,
	AIC_LOST_PATH, AIC_LOST_PATH_CHASE, AIC_STATIC_LOST, AIC_STATIC_OBJ_NOT_ACCESS, AIC_STATIC_CHANGE_STATE,
	AIC_STATE_NONE
};

/* the internal states of the move (cAIChar_InternStateName) */
enum {
	AICI_STOPPED, AICI_PAUSED_CONTROL, AICI_FINISHING_LINE, AICI_FINISHING_LINE_DIRECT, AICI_INT_CHASE_WITH_ACTION,
	AICI_INT_CHASE_WITHOUT_ACTION, AICI_STOPPED_ORIENT_STATIC, AICI_INT_CHASE_WITH_ACTION_BACK,
	AICI_STOPPED_ORIENT_DYNAMIC, AICI_STOPPED_ORIENT_DYNAMIC_WITH_ACTION, AICI_FOLLOW_THE_PATH, AICI_RELAX,
	AICI_TALK_TO, AICI_NONE
};

/* ---- the states of the vehicle AI (cAIVehicle) ---- */

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

struct cAI {
	const cAIBaseVtbl *vftable;	/* 0x00 */
	byte started;			/* 0x04 set by the first update */
	byte pad_5[3];
	cEntity *entity;		/* 0x08 */
	float dt;			/* 0x0c the time of the last update */
	cSlotController slotThink;	/* 0x10 0.5 s, the reactions (UpdateThink of cAIChar and cAIVehicle) */
	byte changing;			/* 0x20 a change of state runs */
	byte pad_21[3];
	int relaxCount;			/* 0x24 the relax actions a character played in its internal state */
	byte changed;			/* 0x28 GetChanged: the state changed in this frame, cleared by every update */
	byte pad_29[3];
	float stateTime;		/* 0x2c the time in the state */
	float waitTime;			/* 0x30 counts while seen is 0 */
	int seen;			/* 0x34 0 out of view, 1 seen, 2 not seen yet */
};

cAI *cAI_ctor(cAI *self, cEntity *e);
cAI *cAI_ctorDefault(cAI *self);
void cAI_dtor(cAI *self);
void cAI_Update(cAI *self, float dt);
void cAI_UpdateOff(cAI *self, float dt);
void cAI_Draw(cAI *self);
byte cAI_GetChanged(cAI *self);
/* the entity is in the sector of the camera of view 0 or seen from it through the portals */
byte cAI_IsVisible(cAI *self);
byte cAI_VarI(cAI *self, const char *name, int *i, byte set);
byte cAI_VarF(cAI *self, const char *name, double *d, byte set);
byte cAI_VarS(cAI *self, const char *name, char **s, byte set);
byte cAI_VarV(cAI *self, const char *name, double *x, double *y, double *z, byte set);
/* text as a format into the console */
void cAI_Print(cAI *self, const char *text);
/* the name of the AIEV_ event into out, nothing for the others */
void cAI_GetEventName(cAI *self, int ev, char *out);
void cAI_OnEvent(cAI *self, int ev);
byte cAI_EventChaseFalse(cAI *self, int ev);
byte cAI_EventChaseNoActionFalse(cAI *self, int ev);
byte cAI_EventTrue(cAI *self, int ev);
void cAI_ThinkChaseBackNop(cAI *self);
void cAI_ThinkChaseNoActionNop(cAI *self);
void cAI_ThinkChaseNop(cAI *self);
void cAI_ThinkGetAwayNop(cAI *self);
void cAI_ThinkGoToNop(cAI *self);
void cAI_UpdateActionNop(cAI *self);

/* ---- cAIMove (AI/AIMove.cpp, src/game/AI/AIMove.c): the base of cAIChar and cAIVehicle (0x90 bytes, a cAI): the
 * target link, the move of the state, the check of an AI that should move but does not ---- */
#define CAIMOVE_SIZE sizeof(cAIMove)

typedef struct cAIMove cAIMove;

/* cAIMove::vftable (25 slots, IsMoving is pure there) */
typedef struct cAIMoveVtbl {
	cAIBaseVtbl base;									/* 0x00 */
	Nto1Node *(*GetTarget)(cAI *self);					/* 0x38 the link to the target */
	byte(*CanJump)(cAI *self);						/* 0x3c flying */
	byte(*GetStopped)(cAI *self);						/* 0x40 */
	byte(*IsPaused)(cAI *self);						/* 0x44 */
	cEntity *(*GetTargetEnt)(cAI *self);					/* 0x48 */
	byte(*GetArrived)(cAI *self);						/* 0x4c */
	/* a random point within r of p the entity can reach (p itself after three tries), out returned */
	vec3_t *(*RandomPoint)(cAI *self, vec3_t *out, const vec3_t *p, float r);	/* 0x50 */
	/* moving without a velocity for 10 s: stopped and the event 7 */
	void(*CheckStopped)(cAI *self, float dt);				/* 0x54 */
	/* the move tells the AI (arrived, blocked, ..) */
	byte(*Event)(cAI *self, int ev);						/* 0x58 */
	byte(*IsMoving)(cAI *self);						/* 0x5c */
	byte(*CanTarget)(cAI *self, cEntity *e);					/* 0x60 */
} cAIMoveVtbl;

/* calls through the slots of cAIMoveVtbl */
static inline byte AIMove_CanJump(void *self) {
	return VT(cAIMoveVtbl, self)->CanJump(self);
}

static inline byte AIMove_CanTarget(void *self, cEntity *e) {
	return VT(cAIMoveVtbl, self)->CanTarget(self, e);
}

static inline void AIMove_CheckStopped(void *self, float dt) {
	VT(cAIMoveVtbl, self)->CheckStopped(self, dt);
}

static inline byte AIMove_Event(void *self, int ev) {
	return VT(cAIMoveVtbl, self)->Event(self, ev);
}

static inline byte AIMove_GetArrived(void *self) {
	return VT(cAIMoveVtbl, self)->GetArrived(self);
}

static inline Nto1Node *AIMove_GetTarget(void *self) {
	return VT(cAIMoveVtbl, self)->GetTarget(self);
}

static inline cEntity *AIMove_GetTargetEnt(void *self) {
	return VT(cAIMoveVtbl, self)->GetTargetEnt(self);
}

static inline byte AIMove_IsMoving(void *self) {
	return VT(cAIMoveVtbl, self)->IsMoving(self);
}

static inline byte AIMove_IsPaused(void *self) {
	return VT(cAIMoveVtbl, self)->IsPaused(self);
}

struct cAIMove {
	union {
		cAI base;			/* 0x00 */
		const cAIMoveVtbl *vftable;
	};
	Nto1Node target;		/* 0x38 the target entity at 0x40, data the AI */
	byte flying;			/* 0x48 the 3D control (CanFly, a Car) */
	byte pad_49[3];
	vec3_t dest;			/* 0x4c */
	float dist;			/* 0x58 the goal is reached that near (every move of a vehicle) */
	int moveMode;			/* 0x5c AICC_, the mode of cAIVehicleControl_SetMove for a vehicle */
	float speed;			/* 0x60 a character only */
	vec3_t dir;			/* 0x64 the direction of the orient and path moves */
	vec3_t center;			/* 0x70 PATROL_ZONE */
	float radius;			/* 0x7c PATROL_ZONE */
	byte pathA;			/* 0x80 the flags of the path move (cAICharControl routeA, routeB) */
	byte pathB;			/* 0x81 a vehicle: the path move stays flat */
	byte stopped;			/* 0x82 CheckStopped: no velocity for 10 s */
	byte inPath;			/* 0x83 FOLLOW_THE_PATH set it */
	byte paused;			/* 0x84 far from the camera: no update */
	byte arrived;			/* 0x85 GO_TO reached the end */
	byte repos;			/* 0x86 "Repos" */
	byte reposing;			/* 0x87 inside Repos */
	byte trafficPlaced;		/* 0x88 a vehicle in TRAFFIC with a kind of track: placed, repositioned like a
					 * visible one */
	byte pad_89[3];
	float stopTime;			/* 0x8c the time without a velocity */
};

cAIMove *cAIMove_ctor(cAIMove *self, cEntity *e);
void cAIMove_dtor(cAIMove *self);
void cAIMove_Draw(cAI *elem);
void cAIMove_Update(cAI *elem, float dt);
void cAIMove_UpdateOff(cAI *elem, float dt);
byte cAIMove_VarF(cAI *elem, const char *name, double *d, byte set);
byte cAIMove_VarI(cAI *elem, const char *name, int *i, byte set);
byte cAIMove_VarS(cAI *elem, const char *name, char **s, byte set);
byte cAIMove_VarV(cAI *elem, const char *name, double *x, double *y, double *z, byte set);
void cAIMove_CheckStopped(cAI *elem, float dt);
byte cAIMove_CanJump(cAI *base);
cEntity *cAIMove_GetTargetEnt(cAI *base);
Nto1Node *cAIMove_GetTarget(cAI *base);
byte cAIMove_GetStopped(cAI *base);
byte cAIMove_GetArrived(cAI *base);
byte cAIMove_IsPaused(cAI *base);
/* e there and active, else a warning */
byte cAIMove_CanTarget(cAI *base, cEntity *e);
vec3_t *cAIMove_RandomPoint(cAI *elem, vec3_t *out, const vec3_t *p, float r);

/* ---- the AI of a walking character (AI/AIChar.cpp, src/game/AI/IChar.c; the states and the functions in
 * include/scrap/aichar.h) ---- */

/* a state with its arguments (cAIChar_SaveState): the one to take when the character group lets it (pending), the
 * one to come back to after a pause or an event (resume) and the one of the last change (last) */
typedef struct cAICharSaved {
	int state;			/* 0x00 AIC_ */
	int internState;		/* 0x04 AICI_ */
	vec3_t dest;			/* 0x08 */
	float dist;			/* 0x14 */
	float speed;			/* 0x18 */
	char target[0x80];		/* 0x1c the name of the target entity, "" for none */
	vec3_t dir;			/* 0x9c */
	byte pathA;			/* 0xa8 */
	byte pauses;			/* 0xa9 */
	byte pad_aa[2];
	vec3_t center;			/* 0xac */
	float radius;			/* 0xb8 */
	int action;			/* 0xbc */
	float stateTime;		/* 0xc0 */
	byte changing;			/* 0xc4 */
	byte changed;			/* 0xc5 */
	byte back;			/* 0xc6 */
	byte orient;			/* 0xc7 */
} cAICharSaved;

typedef struct cAICharControl cAICharControl;

/* cAIChar::vftable (99 slots) */
typedef struct cAICharVtbl {
	cAIMoveVtbl base;									/* 0x00 */
	void(*CheckParams)(cAI *self, int stupidPathfinding);				/* 0x64 */
	byte(*ChangeState)(cAI *self, int state, va_list ap);				/* 0x68 */
	/* SAI_CHAR_ state; the arguments depend on the state (see SAIPy.c) */
	byte (*SetState)(cAI *self, int state, ...);						/* 0x6c */
	byte(*RestoreState)(cAI *self, const cAICharSaved *s);				/* 0x70 */
	void(*SetInternStateV)(cAI *self, int state, va_list ap);			/* 0x74 */
	void (*SetInternState)(cAI *self, int state, ...);						/* 0x78 */
	int(*GetState)(cAI *self);							/* 0x7c */
	int(*GetInternState)(cAI *self);							/* 0x80 */
	void(*StateName)(cAI *self, int state, char *buf);				/* 0x84 */
	void(*GetStateName)(cAI *self, char *buf);					/* 0x88 */
	void(*InternStateName)(cAI *self, int state, char *buf);				/* 0x8c */
	void(*GetInternStateName)(cAI *self, char *buf);					/* 0x90 */
	byte(*GetDirect)(cAI *self);							/* 0x94 */
	byte(*GetDirect2)(cAI *self);							/* 0x98 */
	cAICharControl *(*GetControl)(cAI *self);					/* 0x9c */
	cAICharSaved *(*GetLast)(cAI *self);						/* 0xa0 */
	cAICharSaved *(*SaveTo)(cAI *self, cAICharSaved *s);				/* 0xa4 */
	byte(*SetRepos)(cAI *self, byte on, byte now);					/* 0xa8 */
	byte(*GetRepos)(cAI *self);							/* 0xac */
	byte(*IsTalking)(cAI *self);							/* 0xb0 */
	cEntity *(*GetCollide)(cAI *self);						/* 0xb4 */
	byte(*React)(cAI *self);								/* 0xb8 */
	byte(*CheckAction)(cAI *self);							/* 0xbc */
	byte(*GetChanged2)(cAI *self);							/* 0xc0 */
	byte(*CanTalk)(cAI *self);							/* 0xc4 */
	byte (*SetStateIntern)(cAI *self, int state, ...);						/* 0xc8 */
	byte(*RestoreStateIntern)(cAI *self, const cAICharSaved *s);			/* 0xcc */
	byte(*ChangeStateMore)(cAI *self, int state, int allowed, va_list ap);		/* 0xd0 */
	byte(*IsGroupState)(cAI *self, int state);					/* 0xd4 */
	byte(*Repos)(cAI *self, float minDist, float maxDist, byte flag);		/* 0xd8 */
	void(*AfterRepos)(cAI *self);							/* 0xdc */
	void(*SetDists)(cAI *self);							/* 0xe0 */
	void(*SetDists2)(cAI *self);							/* 0xe4 */
	void(*BeginChange)(cAI *self, int state);					/* 0xe8 */
	void(*SetPauseCheck)(cAI *self, byte on);					/* 0xec */
	void(*SaveState)(cAI *self, cAICharSaved *s);					/* 0xf0 */
	void(*RestoreSaved)(cAI *self, const cAICharSaved *s);				/* 0xf4 */
	void(*SetPaused)(cAI *self, byte on);						/* 0xf8 */
	void(*UpdateAction1)(cAI *self);							/* 0xfc */
	void(*UpdateAction0)(cAI *self);							/* 0x100 */
	void(*UpdateAction)(cAI *self);							/* 0x104 */
	void(*CheckActDist)(cAI *self);							/* 0x108 */
	void(*SetAction)(cAI *self, int action);						/* 0x10c */
	void(*UpdateMove)(cAI *self, float dt);						/* 0x110 */
	byte(*EventDefault)(cAI *self, int ev);						/* 0x114 */
	byte(*EventInRoute)(cAI *self, int ev);						/* 0x118 */
	byte(*EventPatrol)(cAI *self, int ev);						/* 0x11c */
	byte(*EventGoTo)(cAI *self, int ev);						/* 0x120 */
	byte(*EventGetAway)(cAI *self, int ev);						/* 0x124 */
	byte(*EventChase)(cAI *self, int ev);						/* 0x128 */
	byte(*EventChaseBack)(cAI *self, int ev);					/* 0x12c */
	byte(*EventChaseNoAction)(cAI *self, int ev);					/* 0x130 */
	byte(*EventLostPath)(cAI *self, int ev);						/* 0x134 */
	void(*Resume)(cAI *self);							/* 0x138 */
	void(*UpdateThink)(cAI *self);							/* 0x13c */
	void(*CheckPause)(cAI *self);							/* 0x140 */
	void(*CheckRepos)(cAI *self);							/* 0x144 */
	byte(*CanRepos)(cAI *self, int state);						/* 0x148 */
	void(*Think)(cAI *self);								/* 0x14c */
	void(*ThinkInRoute)(cAI *self);							/* 0x150 */
	void(*ThinkGetAway)(cAI *self);							/* 0x154 */
	void(*ThinkChase)(cAI *self);							/* 0x158 */
	void(*ThinkChaseBack)(cAI *self);						/* 0x15c */
	void(*ThinkChaseNoAction)(cAI *self);						/* 0x160 */
	void(*ThinkLostPath)(cAI *self);							/* 0x164 */
	void(*ThinkLostPathChase)(cAI *self);						/* 0x168 */
	void(*ReactLostPath)(cAI *self);							/* 0x16c */
	void(*ReactLostPathChase)(cAI *self);						/* 0x170 */
	void(*ReactPatrol)(cAI *self);							/* 0x174 */
	void(*ReactInRoute)(cAI *self);							/* 0x178 */
	void(*ThinkGoTo)(cAI *self);							/* 0x17c */
	void(*ThinkPatrol)(cAI *self);							/* 0x180 */
	void(*CheckTalk)(cAI *self);							/* 0x184 */
	byte(*Facing)(cAI *self, cEntity *a, cEntity *b);				/* 0x188 */
} cAICharVtbl;

/* calls through the slots of cAICharVtbl */
static inline void AIChar_AfterRepos(void *self) {
	VT(cAICharVtbl, self)->AfterRepos(self);
}

static inline void AIChar_BeginChange(void *self, int state) {
	VT(cAICharVtbl, self)->BeginChange(self, state);
}

static inline byte AIChar_CanRepos(void *self, int state) {
	return VT(cAICharVtbl, self)->CanRepos(self, state);
}

static inline byte AIChar_CanTalk(void *self) {
	return VT(cAICharVtbl, self)->CanTalk(self);
}

static inline byte AIChar_ChangeState(void *self, int state, va_list ap) {
	return VT(cAICharVtbl, self)->ChangeState(self, state, ap);
}

static inline byte AIChar_ChangeStateMore(void *self, int state, int allowed, va_list ap) {
	return VT(cAICharVtbl, self)->ChangeStateMore(self, state, allowed, ap);
}

static inline void AIChar_CheckActDist(void *self) {
	VT(cAICharVtbl, self)->CheckActDist(self);
}

static inline byte AIChar_CheckAction(void *self) {
	return VT(cAICharVtbl, self)->CheckAction(self);
}

static inline void AIChar_CheckPause(void *self) {
	VT(cAICharVtbl, self)->CheckPause(self);
}

static inline void AIChar_CheckRepos(void *self) {
	VT(cAICharVtbl, self)->CheckRepos(self);
}

static inline void AIChar_CheckTalk(void *self) {
	VT(cAICharVtbl, self)->CheckTalk(self);
}

static inline byte AIChar_EventDefault(void *self, int ev) {
	return VT(cAICharVtbl, self)->EventDefault(self, ev);
}

static inline byte AIChar_GetChanged2(void *self) {
	return VT(cAICharVtbl, self)->GetChanged2(self);
}

static inline cEntity *AIChar_GetCollide(void *self) {
	return VT(cAICharVtbl, self)->GetCollide(self);
}

static inline byte AIChar_GetDirect(void *self) {
	return VT(cAICharVtbl, self)->GetDirect(self);
}

static inline byte AIChar_GetDirect2(void *self) {
	return VT(cAICharVtbl, self)->GetDirect2(self);
}

static inline int AIChar_GetInternState(void *self) {
	return VT(cAICharVtbl, self)->GetInternState(self);
}

static inline void AIChar_GetInternStateName(void *self, char *buf) {
	VT(cAICharVtbl, self)->GetInternStateName(self, buf);
}

static inline byte AIChar_GetRepos(void *self) {
	return VT(cAICharVtbl, self)->GetRepos(self);
}

static inline int AIChar_GetState(void *self) {
	return VT(cAICharVtbl, self)->GetState(self);
}

static inline void AIChar_GetStateName(void *self, char *buf) {
	VT(cAICharVtbl, self)->GetStateName(self, buf);
}

static inline void AIChar_InternStateName(void *self, int state, char *buf) {
	VT(cAICharVtbl, self)->InternStateName(self, state, buf);
}

static inline byte AIChar_IsGroupState(void *self, int state) {
	return VT(cAICharVtbl, self)->IsGroupState(self, state);
}

static inline byte AIChar_IsTalking(void *self) {
	return VT(cAICharVtbl, self)->IsTalking(self);
}

static inline byte AIChar_React(void *self) {
	return VT(cAICharVtbl, self)->React(self);
}

static inline void AIChar_ReactInRoute(void *self) {
	VT(cAICharVtbl, self)->ReactInRoute(self);
}

static inline void AIChar_ReactLostPath(void *self) {
	VT(cAICharVtbl, self)->ReactLostPath(self);
}

static inline void AIChar_ReactLostPathChase(void *self) {
	VT(cAICharVtbl, self)->ReactLostPathChase(self);
}

static inline void AIChar_ReactPatrol(void *self) {
	VT(cAICharVtbl, self)->ReactPatrol(self);
}

static inline byte AIChar_Repos(void *self, float minDist, float maxDist, byte flag) {
	return VT(cAICharVtbl, self)->Repos(self, minDist, maxDist, flag);
}

static inline void AIChar_RestoreSaved(void *self, const cAICharSaved *s) {
	VT(cAICharVtbl, self)->RestoreSaved(self, s);
}

static inline byte AIChar_RestoreState(void *self, const cAICharSaved *s) {
	return VT(cAICharVtbl, self)->RestoreState(self, s);
}

static inline byte AIChar_RestoreStateIntern(void *self, const cAICharSaved *s) {
	return VT(cAICharVtbl, self)->RestoreStateIntern(self, s);
}

static inline void AIChar_Resume(void *self) {
	VT(cAICharVtbl, self)->Resume(self);
}

static inline void AIChar_SaveState(void *self, cAICharSaved *s) {
	VT(cAICharVtbl, self)->SaveState(self, s);
}

static inline cAICharSaved *AIChar_SaveTo(void *self, cAICharSaved *s) {
	return VT(cAICharVtbl, self)->SaveTo(self, s);
}

static inline void AIChar_SetAction(void *self, int action) {
	VT(cAICharVtbl, self)->SetAction(self, action);
}

static inline void AIChar_SetDists(void *self) {
	VT(cAICharVtbl, self)->SetDists(self);
}

static inline void AIChar_SetDists2(void *self) {
	VT(cAICharVtbl, self)->SetDists2(self);
}

#define AIChar_SetInternState(self, ...) (VT(cAICharVtbl, self)->SetInternState((void *)(self), __VA_ARGS__))

static inline void AIChar_SetInternStateV(void *self, int state, va_list ap) {
	VT(cAICharVtbl, self)->SetInternStateV(self, state, ap);
}

static inline void AIChar_SetPauseCheck(void *self, byte on) {
	VT(cAICharVtbl, self)->SetPauseCheck(self, on);
}

static inline void AIChar_SetPaused(void *self, byte on) {
	VT(cAICharVtbl, self)->SetPaused(self, on);
}

static inline byte AIChar_SetRepos(void *self, byte on, byte now) {
	return VT(cAICharVtbl, self)->SetRepos(self, on, now);
}

#define AIChar_SetState(self, ...) (VT(cAICharVtbl, self)->SetState((void *)(self), __VA_ARGS__))

#define AIChar_SetStateIntern(self, ...) (VT(cAICharVtbl, self)->SetStateIntern((void *)(self), __VA_ARGS__))
static inline void AIChar_StateName(void *self, int state, char *buf) {
	VT(cAICharVtbl, self)->StateName(self, state, buf);
}

static inline void AIChar_Think(void *self) {
	VT(cAICharVtbl, self)->Think(self);
}

static inline void AIChar_ThinkChase(void *self) {
	VT(cAICharVtbl, self)->ThinkChase(self);
}

static inline void AIChar_ThinkChaseBack(void *self) {
	VT(cAICharVtbl, self)->ThinkChaseBack(self);
}

static inline void AIChar_ThinkChaseNoAction(void *self) {
	VT(cAICharVtbl, self)->ThinkChaseNoAction(self);
}

static inline void AIChar_ThinkGetAway(void *self) {
	VT(cAICharVtbl, self)->ThinkGetAway(self);
}

static inline void AIChar_ThinkGoTo(void *self) {
	VT(cAICharVtbl, self)->ThinkGoTo(self);
}

static inline void AIChar_ThinkInRoute(void *self) {
	VT(cAICharVtbl, self)->ThinkInRoute(self);
}

static inline void AIChar_ThinkLostPath(void *self) {
	VT(cAICharVtbl, self)->ThinkLostPath(self);
}

static inline void AIChar_ThinkLostPathChase(void *self) {
	VT(cAICharVtbl, self)->ThinkLostPathChase(self);
}

static inline void AIChar_ThinkPatrol(void *self) {
	VT(cAICharVtbl, self)->ThinkPatrol(self);
}

static inline void AIChar_UpdateAction(void *self) {
	VT(cAICharVtbl, self)->UpdateAction(self);
}

static inline void AIChar_UpdateAction0(void *self) {
	VT(cAICharVtbl, self)->UpdateAction0(self);
}

static inline void AIChar_UpdateAction1(void *self) {
	VT(cAICharVtbl, self)->UpdateAction1(self);
}

static inline void AIChar_UpdateMove(void *self, float dt) {
	VT(cAICharVtbl, self)->UpdateMove(self, dt);
}

static inline void AIChar_UpdateThink(void *self) {
	VT(cAICharVtbl, self)->UpdateThink(self);
}

/* 0x380 bytes */
struct cAIChar {
	union {
		cAIMove base;			/* 0x000 */
		const cAICharVtbl *vftable;
	};
	cAICharSaved pending;		/* 0x090 state AIC_STATE_NONE for none */
	cSlotController slotAction;		/* 0x158 0.2 s, the distance check of the action */
	cSlotController slotPause;		/* 0x168 1 s */
	cSlotController slotRepos;		/* 0x178 1 s */
	cSlotController slotLost;		/* 0x188 0.2 s, LOST_PATH looks for the graph */
	cEntity *ent;			/* 0x198 the character (cWalkCharEntity) */
	cAICharControl *control;	/* 0x19c the move control (2D or 3D) */
	int state;			/* 0x1a0 AIC_ */
	int internState;		/* 0x1a4 AICI_ */
	int action;			/* 0x1a8 1 while the action button is used on the target */
	struct cWalkCharControl *ctrl;	/* 0x1ac the control of the character (0x4ec of it) */
	float actMin;			/* 0x1b0 the action between these distances to the target */
	float actMax;			/* 0x1b4 */
	float chaseNear;		/* 0x1b8 */
	float chaseFar;			/* 0x1bc */
	byte pauses;			/* 0x1c0 IN_ROUTE and PATROL_ZONE stop now and then */
	byte back;			/* 0x1c1 */
	byte pauseCheck;		/* 0x1c2 */
	byte pad_1c3;
	cAICharSaved resume;		/* 0x1c4 kept before a pause or an event */
	cAICharSaved last;		/* 0x28c the state when the last change began */
	float actTime;			/* 0x354 the action button held */
	float standTime;		/* 0x358 the target did not move */
	byte canTalk;			/* 0x35c */
	byte talking;			/* 0x35d */
	byte talkTurn;			/* 0x35e */
	byte pad_35f;
	int numTalks;			/* 0x360 */
	int talkRun;			/* 0x364 the talk actions in a row */
	int lastTalk;			/* 0x368 0..2, Talk0..Talk2 */
	float routeTime;		/* 0x36c */
	byte orient;			/* 0x370 */
	byte pad_371[3];
	vec3_t lostPoint;		/* 0x374 -FLT_MAX for none */
};

/* the three parameter blocks of SAI.EnableAIVehicle, named by the CHECKs of cAIVehicle_CheckParams1..3 */
typedef struct cAIVehicleParams1 {
	float damageGetAway;		/* 0x00 FACTOR_DAMAGE_GET_AWAY 0..1 */
	float damageItemLife;		/* 0x04 FACTOR_DAMAGE_ITEM_LIFE 0..1 */
	float itemAmmo;			/* 0x08 FACTOR_ITEM_AMMO 0..1 */
	int boostGetAway;		/* 0x0c CONTROL_BOOST_GET_AWAY 0..1 */
	int boostItemLife;		/* 0x10 CONTROL_BOOST_ITEM_LIFE 0..1 */
	int boostItemAmmo;		/* 0x14 CONTROL_BOOST_ITEM_AMMO 0..1 */
	int hookRace;			/* 0x18 CONTROL_HOOK_RACE */
	int hookMap;			/* 0x1c FACTOR_HOOK_MAP 0..10 */
	int strafeEvasion;		/* 0x20 CONTROL_STRAFE_EVASION 0..1 */
	int percHook;			/* 0x24 USE_PERC_HOOK 0..100 */
	int percHookGetAway;		/* 0x28 USE_PERC_HOOK_GET_AWAY 0..100 */
} cAIVehicleParams1;

typedef struct cAIVehicleParams2 {
	int stupidPathfinding;		/* 0x00 STUPID_PATHFINDING, must be 0 */
	int strafe;			/* 0x04 CONTROL_STRAFE, must be 2 */
	int brake;			/* 0x08 CONTROL_BRAKE 0..1 */
	int maxTimeStrafe;		/* 0x0c MAX_TIME_STRAFE 0..10 */
	float maxVelStrafe;		/* 0x10 MAX_VEL_STRAFE 0..1 */
	int aim;			/* 0x14 CONTROL_AIM 1..10 */
} cAIVehicleParams2;

typedef struct cAIVehicleParams3 {
	int cadence;			/* 0x00 CADENCE_SHOOT 0..100 */
	int cadenceCM;			/* 0x04 CADENCE_SHOOT_CM 0..7 */
	float angAttack;		/* 0x08 ANG_ATTACK 0..pi */
	float counterattack;		/* 0x0c COUNTERATTACK 0..1 */
	int percCloud;			/* 0x10 USE_PERC_CLOUD 0..100 */
	int percSonic;			/* 0x14 USE_PERC_SONIC 0..100 */
	int percElectro;		/* 0x18 USE_PERC_ELECTRO 0..100 */
} cAIVehicleParams3;

extern cGVar GVar_IsMapOutDoor;
#define g_IsMapOutDoor (GVar_IsMapOutDoor.value.i)	/* value of the engine variable IsMapOutDoor */
extern CNodesWorld *g_NodesWorld;
extern CNodesWorld2D *g_NodesWorld2D;
extern CNodesTraffic *g_NodesTrafficInDoor;
extern CNodesTraffic *g_NodesTraffic;
extern void *g_AStar;
extern void *g_AStar2D;
extern cAIVehicleGroup *g_VehicleGroup;
extern cAICharGroup *g_CharGroup;
extern cGVar GVar_ShowTrafficAI;
#define g_TrafficState (GVar_ShowTrafficAI.value.i)	/* 3 after SAI.AnalizeTraffic */

/* constructors (on the memory from operator_new) */
CNodesWorld *CNodesWorld_ctor(void *mem);
CNodesWorld2D *CNodesWorld2D_ctor(void *mem);
CNodesTraffic *CNodesTrafficInDoor_ctor(void *mem);
CNodesTraffic *CNodesTraffic_ctor(void *mem);
void *CAStar_ctor(void *mem, CNodesWorld *nodes);
void *CAStar2D_ctor(void *mem, CNodesWorld2D *nodes);
cAIVehicleGroup *cAIVehicleGroup_ctor(void *mem);
cAICharGroup *cAICharGroup_ctor(void *mem);

void CNodesWorld_Load(CNodesWorld *nodes, int arg, const char *name, const vec3_t *pos);
void CNodesWorld_Analyze(CNodesWorld *nodes, const vec3_t *size);
void CNodesWorld_AnalyzeGraph(CNodesWorld *nodes);
byte CNodesWorld_GetRacePoint(CNodesWorld *nodes, vec3_t *out, const vec3_t *initial, float minDist,
	float maxDist, int arg);
void CNodesWorld2D_Load(CNodesWorld2D *nodes, int arg, const char *name, const vec3_t *pos);
void CNodesWorld2D_AnalyzeGraph(CNodesWorld2D *nodes);

byte cAICharGroup_GetReposPos(cAICharGroup *group, const vec3_t *pos, const char *name, vec3_t *out,
	cEntity *target);

int cAIVehicle_GetState(cAIVehicle *ai);
void cAIVehicle_Setup(cAIVehicle *ai, cAIVehicleParams1 *p1, cAIVehicleParams2 *p2,
	cAIVehicleParams3 *p3);
cAIVehicleParams1 *cAIVehicle_GetParams1(cAIVehicle *ai);
cAIVehicleParams2 *cAIVehicle_GetParams2(cAIVehicle *ai);
cAIVehicleParams3 *cAIVehicle_GetParams3(cAIVehicle *ai);
cEntity *cAIVehicle_GetNearestItemLife(cAIVehicle *ai);
void cAIVehicle_SetRotStatic(cAIVehicle *ai, float a, float b);
void cAIVehicle_SetInertia(cAIVehicle *ai, int on);

/* the state of the vehicle AI, arguments by state (see SAIPy.c); 0 if it was not taken */
byte cAIVehicle_SetState(cAIVehicle *ai, int state, ...);

/* default values of SAI.SetStateVehicle CHASE (state 7) */
extern cGVar GVar_MinDistChaseAI;
#define g_AIChaseDefault1 (GVar_MinDistChaseAI.value.f)
extern cGVar GVar_MaxDistChaseAI;
#define g_AIChaseDefault2 (GVar_MaxDistChaseAI.value.f)

/* ---- cAIEntityGroup (AI/AIEntityGroup.cpp, src/game/AI/AIEntityGroup.c): the base of the vehicle and the character
 * groups, up to 20 entities (a race) and the time since an AI of the group took a group state; the next one may take
 * one when the time reached TimeBetwChangeStateAI ---- */
#define CAIENTITYGROUP_SIZE sizeof(cAIEntityGroup)

/* ctor and dtor run on the memory of the subclasses */
void *cAIEntityGroup_ctor(void *self);
void cAIEntityGroup_dtor(void *self);
void cAIEntityGroup_Clear(cAIEntityGroup *self);
byte cAIEntityGroup_Add(cAIEntityGroup *self, cEntity *e);
cEntity *cAIEntityGroup_Get(cAIEntityGroup *self, int i);
int cAIEntityGroup_Num(cAIEntityGroup *self);
byte cAIEntityGroup_CanChange(cAIEntityGroup *self);
void cAIEntityGroup_ResetChange(cAIEntityGroup *self);
void cAIEntityGroup_AllowChange(cAIEntityGroup *self);
void cAIEntityGroup_Update(cAIEntityGroup *self, float dt);

/* the director and the master (0x384 bytes) */
typedef struct cAIBank {
	cAIChar base;			/* 0x000 */
	int money;			/* 0x380 the money of the character at the demand of the attack (state 2) */
} cAIBank;
typedef cAIBank cAIBankDirector;
typedef cAIBank cAIBankMaster;

/* the classes that add no members (0x380 bytes) */
typedef cAIChar cAIDesktop;
typedef cAIChar cAIHuman;
typedef cAIChar cAIMayor;
typedef cAIChar cAIPolice;
typedef cAIChar cAISentinel;

/* the bishop (0x398 bytes) */
typedef struct cAIBishop {
	cAIChar base;			/* 0x000 */
	int targetLife;			/* 0x380 the life of the target at the last check */
	byte blocked;			/* 0x384 the way to the target is not free */
	byte pad_385[3];
	int shootFactor;		/* 0x388 ShootBishopAI: one shot in so many */
	float shootAng;			/* 0x38c OffsetAngShootBishopAI */
	float shootTime;		/* 0x390 TimeShootBishopAI */
	float shootDist;		/* 0x394 DistMaxShootBishopAI */
} cAIBishop;

/* the gear (0x390 bytes): the shot of the bishop without the time of the button */
typedef struct cAIGear {
	cAIChar base;			/* 0x000 */
	int targetLife;			/* 0x380 */
	byte blocked;			/* 0x384 */
	byte pad_385[3];
	int shootFactor;		/* 0x388 ShootGearAI */
	float shootAng;			/* 0x38c OffsetAngShootGearAI */
} cAIGear;

/* the messenger (0x390 bytes) */
typedef struct cAIMessenger {
	cAIChar base;			/* 0x000 */
	int shootFactor;		/* 0x380 ShootMessengerAI */
	float shootAng;			/* 0x384 OffsetAngShootMessengerAI */
	float shootTime;		/* 0x388 TimeShootMessengerAI */
	float shootDist;		/* 0x38c DistMaxShootMessengerAI */
} cAIMessenger;

/* the nurse (0x384 bytes) */
typedef struct cAINurse {
	cAIChar base;			/* 0x000 */
	byte fresh;			/* 0x380 set by the start of state 2: the action within actMax at once */
	byte pad_381[3];
} cAINurse;

/* vf66 of the director and the master: the attack when the character got money or the control is idle */
void cAIBank_CheckAttack(cAI *elem);
/* vf27 and vf50 of both: the state from outside and from inside (ATTACK, 2, becomes 7 with the money noted) */
byte cAIBank_SetState(cAI *elem, int state, ...);
byte cAIBank_SetStateIntern(cAI *elem, int state, ...);
/* the way to the target was not free (vf89 of the move control asks it) */
byte cAIGear_IsBlocked(cAIGear *self);

/* cAIChar_dtor, cAIChar_SetDists (vf56) and cAIChar_ChangeState (vf26): aichar.h; the move controls:
 * aicharcontrol.h */

extern cGVar GVar_BankMasterVacumDist;
#define g_BankMasterVacumDist (GVar_BankMasterVacumDist.value.f)

#endif
