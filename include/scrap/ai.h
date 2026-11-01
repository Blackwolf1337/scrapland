/*
 * AI (original source: AI/): navigation graphs, traffic, A*, AI groups and the AI controllers of vehicles and
 * characters. The graph objects are made by SAI.IniAI with the engine's operator new; their globals keep the
 * names of the CHECK expressions in PyInterface/SAIPy.cpp.
 */
#ifndef SCRAP_AI_H
#define SCRAP_AI_H

#include <stdarg.h>

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

typedef struct cAIVehicleGroupVtbl {
	void *vf00;
	void(*InitRace)(cAIVehicleGroup *self);			/* 0x4 */
	void(*AddVehicle)(cAIVehicleGroup *self, cEntity *vehicle);	/* 0x8 */
	cEntity *(*GetVehicle)(cAIVehicleGroup *self, int i);	/* 0xc */
	int(*NumVehicles)(cAIVehicleGroup *self);			/* 0x10 */
	/* the time since the last change of state of a vehicle AI reached the limit */
	byte(*CanChange)(cAIVehicleGroup *self);			/* 0x14 */
	void(*ResetChange)(cAIVehicleGroup *self);			/* 0x18 the time to 0 */
	void(*AllowChange)(cAIVehicleGroup *self);			/* 0x1c the time to the limit */
} cAIVehicleGroupVtbl;
SD_OFFSET(cAIVehicleGroupVtbl, AllowChange, 0x1c);

struct cAIVehicleGroup {
	const cAIVehicleGroupVtbl *vftable;	/* 0x00 */
	byte pad_4[0x54 - 0x4];
	int numVehicles;		/* 0x54 */
	byte pad_58[0x5c - 0x58];
	float changeTime;		/* 0x5c the time since the last change of state */
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
SD_OFFSET(cAIVehicleGroup, numStoppedRoute, 0x70);
SD_OFFSET(cAIVehicleGroup, numElevators, 0xec);
SD_SIZE(cAIVehicleGroup, 0xf4);

/* ---- cAI (AI/AI.cpp, src/game/AI/AI.c): the base of every AI (0x38 bytes), the entity, the time of the frame and
 * of the state, and the slot of the reactions ---- */
#define CAI_VFTABLE GAME_VFTABLE(cAI_vftable)

typedef struct cAI cAI;

/* the slots of cAI::vftable (Debugf is pure there); variadic methods are __cdecl with self on the stack */
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
SD_OFFSET(cAIBaseVtbl, Debugf, 0x18);
SD_OFFSET(cAIBaseVtbl, OnEvent, 0x34);

/* the events of the AI (GetEventName) */
enum {
	AIEV_LOST_AGENT, AIEV_OBJECTIVE_NOT_FOUND, AIEV_OBJECTIVE_REACHED, AIEV_OBJECTIVE_DIED, AIEV_OBJECTIVE_NEAR,
	AIEV_OBJECTIVE_VIS, AIEV_ORIENT_REACHED, AIEV_DONT_GO, AIEV_PROBLEM_MOVE, AIEV_NO_PROBLEM, AIEV_COLLISION_AGENTS
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
SD_OFFSET(cAI, slotThink, 0x10);
SD_OFFSET(cAI, changed, 0x28);
SD_SIZE(cAI, 0x38);

/* the AI of an entity (cEntity::ai, GetAI) */
static inline const cAIBaseVtbl *AI_Vtbl(void *ai) {
	return ((cAI *)ai)->vftable;
}

static inline void AI_Debugf(void *ai, const char *text) {
	AI_Vtbl(ai)->Debugf(ai, text);
}

cAI *cAI_ctor(cAI *self, cEntity *e);
cAI *cAI_ctorDefault(cAI *self);
void cAI_dtor(cAI *self);
cAI *cAI_Delete(cAI *self, byte flags);
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

/* ---- cAIMove (AI/AIMove.cpp, src/game/AI/AIMove.c): the base of cAIChar and cAIVehicle (0x90 bytes, a cAI): the
 * target link, the move of the state, the check of an AI that should move but does not ---- */
#define CAIMOVE_VFTABLE GAME_VFTABLE(cAIMove_vftable)
#define CAIMOVE_SIZE 0x90

typedef struct cAIMove cAIMove;

/* cAIMove::vftable (25 slots, IsMoving is pure there) */
typedef struct cAIMoveVtbl {
	cAIBaseVtbl base;									/* 0x00 */
	Nto1Node *(*GetTarget)(cAIMove *self);					/* 0x38 the link to the target */
	byte(*CanJump)(cAIMove *self);						/* 0x3c flying */
	byte(*GetStopped)(cAIMove *self);						/* 0x40 */
	byte(*IsPaused)(cAIMove *self);						/* 0x44 */
	cEntity *(*GetTargetEnt)(cAIMove *self);					/* 0x48 */
	byte(*GetArrived)(cAIMove *self);						/* 0x4c */
	/* a random point within r of p the entity can reach (p itself after three tries), out returned */
	vec3_t *(*RandomPoint)(cAIMove *self, vec3_t *out, const vec3_t *p, float r);	/* 0x50 */
	/* moving without a velocity for 10 s: stopped and the event 7 */
	void(*CheckStopped)(cAIMove *self, float dt);				/* 0x54 */
	/* the move tells the AI (arrived, blocked, ..) */
	byte(*Event)(cAIMove *self, int ev);						/* 0x58 */
	byte(*IsMoving)(cAIMove *self);						/* 0x5c */
	byte(*CanTarget)(cAIMove *self, cEntity *e);					/* 0x60 */
} cAIMoveVtbl;
SD_OFFSET(cAIMoveVtbl, GetTarget, 0x38);
SD_OFFSET(cAIMoveVtbl, CanTarget, 0x60);

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
SD_OFFSET(cAIMove, target, 0x38);
SD_OFFSET(cAIMove, dest, 0x4c);
SD_OFFSET(cAIMove, center, 0x70);
SD_OFFSET(cAIMove, trafficPlaced, 0x88);
SD_OFFSET(cAIMove, stopTime, 0x8c);
SD_SIZE(cAIMove, CAIMOVE_SIZE);

cAIMove *cAIMove_ctor(cAIMove *self, cEntity *e);
cAIMove *cAIMove_ctorDefault(cAIMove *self);
void cAIMove_dtor(cAIMove *self);
cAIMove *cAIMove_Delete(cAIMove *self, byte flags);
void cAIMove_Draw(cAIMove *self);
void cAIMove_Update(cAIMove *self, float dt);
void cAIMove_UpdateOff(cAIMove *self, float dt);
byte cAIMove_VarF(cAIMove *self, const char *name, double *d, byte set);
byte cAIMove_VarI(cAIMove *self, const char *name, int *i, byte set);
byte cAIMove_VarS(cAIMove *self, const char *name, char **s, byte set);
byte cAIMove_VarV(cAIMove *self, const char *name, double *x, double *y, double *z, byte set);
void cAIMove_CheckStopped(cAIMove *self, float dt);
byte cAIMove_CanJump(cAIMove *self);
cEntity *cAIMove_GetTargetEnt(cAIMove *self);
Nto1Node *cAIMove_GetTarget(cAIMove *self);
byte cAIMove_GetStopped(cAIMove *self);
byte cAIMove_GetArrived(cAIMove *self);
byte cAIMove_IsPaused(cAIMove *self);
byte cAIMove_Event(cAIMove *self, int ev);
/* e there and active, else a warning */
byte cAIMove_CanTarget(cAIMove *self, cEntity *e);
vec3_t *cAIMove_RandomPoint(cAIMove *self, vec3_t *out, const vec3_t *p, float r);

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
SD_OFFSET(cAICharSaved, target, 0x1c);
SD_OFFSET(cAICharSaved, center, 0xac);
SD_SIZE(cAICharSaved, 0xc8);

typedef struct cAICharControl cAICharControl;

/* cAIChar::vftable (99 slots, rva 0x3a49b4); the variadic ones are __cdecl with self on the stack */
typedef struct cAICharVtbl {
	cAIChar *(*Delete)(cAIChar *self, byte flags);					/* 0x00 */
	void(*Update)(cAIChar *self, float dt);						/* 0x04 */
	void(*UpdateOff)(cAIChar *self, float dt);						/* 0x08 */
	void(*Draw)(cAIChar *self);								/* 0x0c */
	byte(*GetChanged)(cAIChar *self);							/* 0x10 */
	byte(*IsVisible)(cAIChar *self);						/* 0x14 */
	void (*Debugf)(cAIChar *self, const char *fmt, ...);						/* 0x18 */
	byte(*VarI)(cAIChar *self, const char *name, int *i, byte set);			/* 0x1c */
	byte(*VarF)(cAIChar *self, const char *name, double *d, byte set);			/* 0x20 */
	byte(*VarS)(cAIChar *self, const char *name, char **s, byte set);			/* 0x24 */
	byte(*VarV)(cAIChar *self, const char *name, double *x, double *y, double *z, byte set);	/* 0x28 */
	void *vf0b_0c[2];
	void(*OnEvent)(cAIChar *self, int ev);						/* 0x34 the debug text */
	Nto1Node *(*GetTarget)(cAIChar *self);						/* 0x38 the link to the target */
	byte(*CanJump)(cAIChar *self);							/* 0x3c flying */
	byte(*GetStopped)(cAIChar *self);							/* 0x40 */
	byte(*IsPaused)(cAIChar *self);							/* 0x44 */
	cEntity *(*GetTargetEnt)(cAIChar *self);						/* 0x48 */
	byte(*GetArrived)(cAIChar *self);							/* 0x4c */
	/* a random point within r of p the character can reach (p itself after three tries), out returned */
	vec3_t *(*RandomPoint)(cAIChar *self, vec3_t *out, const vec3_t *p, float r);	/* 0x50 */
	/* moving without a velocity for 10 s: stopped and the event 7 */
	void(*CheckStopped)(cAIChar *self, float dt);					/* 0x54 */
	/* the move tells the AI (arrived, blocked, ..): the reaction of the state, else EventDefault */
	byte(*Event)(cAIChar *self, int ev);							/* 0x58 */
	byte(*IsMoving)(cAIChar *self);							/* 0x5c */
	byte(*CanTarget)(cAIChar *self, cEntity *e);					/* 0x60 */
	void(*CheckParams)(cAIChar *self, int stupidPathfinding);				/* 0x64 */
	byte(*ChangeState)(cAIChar *self, int state, va_list ap);				/* 0x68 */
	/* SAI_CHAR_ state; the arguments depend on the state (see SAIPy.c) */
	byte (*SetState)(cAIChar *self, int state, ...);						/* 0x6c */
	byte(*RestoreState)(cAIChar *self, const cAICharSaved *s);				/* 0x70 */
	void(*SetInternStateV)(cAIChar *self, int state, va_list ap);			/* 0x74 */
	void (*SetInternState)(cAIChar *self, int state, ...);						/* 0x78 */
	int(*GetState)(cAIChar *self);							/* 0x7c */
	int(*GetInternState)(cAIChar *self);							/* 0x80 */
	void(*StateName)(cAIChar *self, int state, char *buf);				/* 0x84 */
	void(*GetStateName)(cAIChar *self, char *buf);					/* 0x88 */
	void(*InternStateName)(cAIChar *self, int state, char *buf);				/* 0x8c */
	void(*GetInternStateName)(cAIChar *self, char *buf);					/* 0x90 */
	byte(*GetDirect)(cAIChar *self);							/* 0x94 */
	byte(*GetDirect2)(cAIChar *self);							/* 0x98 */
	cAICharControl *(*GetControl)(cAIChar *self);					/* 0x9c */
	cAICharSaved *(*GetLast)(cAIChar *self);						/* 0xa0 */
	cAICharSaved *(*SaveTo)(cAIChar *self, cAICharSaved *s);				/* 0xa4 */
	byte(*SetRepos)(cAIChar *self, byte on, byte now);					/* 0xa8 */
	byte(*GetRepos)(cAIChar *self);							/* 0xac */
	byte(*IsTalking)(cAIChar *self);							/* 0xb0 */
	cEntity *(*GetCollide)(cAIChar *self);						/* 0xb4 */
	byte(*React)(cAIChar *self);								/* 0xb8 */
	byte(*CheckAction)(cAIChar *self);							/* 0xbc */
	byte(*GetChanged2)(cAIChar *self);							/* 0xc0 */
	byte(*CanTalk)(cAIChar *self);							/* 0xc4 */
	byte (*SetStateIntern)(cAIChar *self, int state, ...);						/* 0xc8 */
	byte(*RestoreStateIntern)(cAIChar *self, const cAICharSaved *s);			/* 0xcc */
	byte(*ChangeStateMore)(cAIChar *self, int state, int allowed, va_list ap);		/* 0xd0 */
	byte(*IsGroupState)(cAIChar *self, int state);					/* 0xd4 */
	byte(*Repos)(cAIChar *self, float minDist, float maxDist, byte flag);		/* 0xd8 */
	void(*AfterRepos)(cAIChar *self);							/* 0xdc */
	void(*SetDists)(cAIChar *self);							/* 0xe0 */
	void(*SetDists2)(cAIChar *self);							/* 0xe4 */
	void(*BeginChange)(cAIChar *self, int state);					/* 0xe8 */
	void(*SetPauseCheck)(cAIChar *self, byte on);					/* 0xec */
	void(*SaveState)(cAIChar *self, cAICharSaved *s);					/* 0xf0 */
	void(*RestoreSaved)(cAIChar *self, const cAICharSaved *s);				/* 0xf4 */
	void(*SetPaused)(cAIChar *self, byte on);						/* 0xf8 */
	void(*UpdateAction1)(cAIChar *self);							/* 0xfc */
	void(*UpdateAction0)(cAIChar *self);							/* 0x100 */
	void(*UpdateAction)(cAIChar *self);							/* 0x104 */
	void(*CheckActDist)(cAIChar *self);							/* 0x108 */
	void(*SetAction)(cAIChar *self, int action);						/* 0x10c */
	void(*UpdateMove)(cAIChar *self, float dt);						/* 0x110 */
	byte(*EventDefault)(cAIChar *self, int ev);						/* 0x114 */
	byte(*EventInRoute)(cAIChar *self, int ev);						/* 0x118 */
	byte(*EventPatrol)(cAIChar *self, int ev);						/* 0x11c */
	byte(*EventGoTo)(cAIChar *self, int ev);						/* 0x120 */
	byte(*EventGetAway)(cAIChar *self, int ev);						/* 0x124 */
	byte(*EventChase)(cAIChar *self, int ev);						/* 0x128 */
	byte(*EventChaseBack)(cAIChar *self, int ev);					/* 0x12c */
	byte(*EventChaseNoAction)(cAIChar *self, int ev);					/* 0x130 */
	byte(*EventLostPath)(cAIChar *self, int ev);						/* 0x134 */
	void(*Resume)(cAIChar *self);							/* 0x138 */
	void(*UpdateThink)(cAIChar *self);							/* 0x13c */
	void(*CheckPause)(cAIChar *self);							/* 0x140 */
	void(*CheckRepos)(cAIChar *self);							/* 0x144 */
	byte(*CanRepos)(cAIChar *self, int state);						/* 0x148 */
	void(*Think)(cAIChar *self);								/* 0x14c */
	void(*ThinkInRoute)(cAIChar *self);							/* 0x150 */
	void(*ThinkGetAway)(cAIChar *self);							/* 0x154 */
	void(*ThinkChase)(cAIChar *self);							/* 0x158 */
	void(*ThinkChaseBack)(cAIChar *self);						/* 0x15c */
	void(*ThinkChaseNoAction)(cAIChar *self);						/* 0x160 */
	void(*ThinkLostPath)(cAIChar *self);							/* 0x164 */
	void(*ThinkLostPathChase)(cAIChar *self);						/* 0x168 */
	void(*ReactLostPath)(cAIChar *self);							/* 0x16c */
	void(*ReactLostPathChase)(cAIChar *self);						/* 0x170 */
	void(*ReactPatrol)(cAIChar *self);							/* 0x174 */
	void(*ReactInRoute)(cAIChar *self);							/* 0x178 */
	void(*ThinkGoTo)(cAIChar *self);							/* 0x17c */
	void(*ThinkPatrol)(cAIChar *self);							/* 0x180 */
	void(*CheckTalk)(cAIChar *self);							/* 0x184 */
	byte(*Facing)(cAIChar *self, cEntity *a, cEntity *b);				/* 0x188 */
} cAICharVtbl;
SD_OFFSET(cAICharVtbl, Debugf, 0x18);
SD_OFFSET(cAICharVtbl, GetTarget, 0x38);
SD_OFFSET(cAICharVtbl, IsPaused, 0x44);
SD_OFFSET(cAICharVtbl, Event, 0x58);
SD_OFFSET(cAICharVtbl, SetState, 0x6c);
SD_OFFSET(cAICharVtbl, GetState, 0x7c);
SD_OFFSET(cAICharVtbl, GetDirect, 0x94);
SD_OFFSET(cAICharVtbl, SaveTo, 0xa4);
SD_OFFSET(cAICharVtbl, GetRepos, 0xac);
SD_OFFSET(cAICharVtbl, SetStateIntern, 0xc8);
SD_OFFSET(cAICharVtbl, SaveState, 0xf0);
SD_OFFSET(cAICharVtbl, EventDefault, 0x114);
SD_OFFSET(cAICharVtbl, Think, 0x14c);
SD_OFFSET(cAICharVtbl, Facing, 0x188);
SD_SIZE(cAICharVtbl, 99 * 4);

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
SD_OFFSET(cAIChar, pending, 0x90);
SD_OFFSET(cAIChar, slotAction, 0x158);
SD_OFFSET(cAIChar, ent, 0x198);
SD_OFFSET(cAIChar, pauses, 0x1c0);
SD_OFFSET(cAIChar, resume, 0x1c4);
SD_OFFSET(cAIChar, last, 0x28c);
SD_OFFSET(cAIChar, actTime, 0x354);
SD_OFFSET(cAIChar, lostPoint, 0x374);
SD_SIZE(cAIChar, 0x380);

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
SD_SIZE(cAIVehicleParams1, 0x2c);

typedef struct cAIVehicleParams2 {
	int stupidPathfinding;		/* 0x00 STUPID_PATHFINDING, must be 0 */
	int strafe;			/* 0x04 CONTROL_STRAFE, must be 2 */
	int brake;			/* 0x08 CONTROL_BRAKE 0..1 */
	int maxTimeStrafe;		/* 0x0c MAX_TIME_STRAFE 0..10 */
	float maxVelStrafe;		/* 0x10 MAX_VEL_STRAFE 0..1 */
	int aim;			/* 0x14 CONTROL_AIM 1..10 */
} cAIVehicleParams2;
SD_SIZE(cAIVehicleParams2, 0x18);

typedef struct cAIVehicleParams3 {
	int cadence;			/* 0x00 CADENCE_SHOOT 0..100 */
	int cadenceCM;			/* 0x04 CADENCE_SHOOT_CM 0..7 */
	float angAttack;		/* 0x08 ANG_ATTACK 0..pi */
	float counterattack;		/* 0x0c COUNTERATTACK 0..1 */
	int percCloud;			/* 0x10 USE_PERC_CLOUD 0..100 */
	int percSonic;			/* 0x14 USE_PERC_SONIC 0..100 */
	int percElectro;		/* 0x18 USE_PERC_ELECTRO 0..100 */
} cAIVehicleParams3;
SD_SIZE(cAIVehicleParams3, 0x1c);

#define g_IsMapOutDoor GAME_GLOBAL_IN(int, GVar_IsMapOutDoor, 16)	/* value of the engine variable IsMapOutDoor */
extern CNodesWorld *g_NodesWorld;
extern CNodesWorld2D *g_NodesWorld2D;
extern CNodesTraffic *g_NodesTrafficInDoor;
extern CNodesTraffic *g_NodesTraffic;
extern void *g_AStar;
extern void *g_AStar2D;
extern cAIVehicleGroup *g_VehicleGroup;
extern cAICharGroup *g_CharGroup;
#define g_TrafficState GAME_GLOBAL_IN(int, GVar_ShowTrafficAI, 16)	/* 3 after SAI.AnalizeTraffic */

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
#define g_AIChaseDefault1 GAME_GLOBAL_IN(float, GVar_MinDistChaseAI, 16)
#define g_AIChaseDefault2 GAME_GLOBAL_IN(float, GVar_MaxDistChaseAI, 16)

/* ---- cAIEntityGroup (AI/AIEntityGroup.cpp, src/game/AI/AIEntityGroup.c): the base of the vehicle and the character
 * groups, up to 20 entities (a race) and the time since an AI of the group took a group state; the next one may take
 * one when the time reached TimeBetwChangeStateAI ---- */
#define CAIENTITYGROUP_VFTABLE GAME_VFTABLE(cAIEntityGroup_vftable)
#define CAIENTITYGROUP_SIZE 0x60
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
SD_OFFSET(cAIEntityGroupVtbl, Update, 0x20);

struct cAIEntityGroup {
	const cAIEntityGroupVtbl *vftable;	/* 0x00 */
	cEntity *ents[CAIENTITYGROUP_MAX];	/* 0x04 */
	int num;				/* 0x54 */
	byte unused_58;				/* 0x58 */
	byte pad_59[3];
	float changeTime;			/* 0x5c */
};
SD_OFFSET(cAIEntityGroup, num, 0x54);
SD_OFFSET(cAIEntityGroup, changeTime, 0x5c);
SD_SIZE(cAIEntityGroup, CAIENTITYGROUP_SIZE);

#define g_TimeBetwChangeStateAI GAME_GLOBAL_IN(float, GVar_TimeBetwChangeStateAI, 16)	/* "TimeBetwChangeStateAI" */

/* ctor and dtor run on the memory of the subclasses */
void *cAIEntityGroup_ctor(void *self);
void cAIEntityGroup_dtor(void *self);
cAIEntityGroup *cAIEntityGroup_Delete(cAIEntityGroup *self, byte flags);
void cAIEntityGroup_Clear(cAIEntityGroup *self);
byte cAIEntityGroup_Add(cAIEntityGroup *self, cEntity *e);
cEntity *cAIEntityGroup_Get(cAIEntityGroup *self, int i);
int cAIEntityGroup_Num(cAIEntityGroup *self);
byte cAIEntityGroup_CanChange(cAIEntityGroup *self);
void cAIEntityGroup_ResetChange(cAIEntityGroup *self);
void cAIEntityGroup_AllowChange(cAIEntityGroup *self);
void cAIEntityGroup_Update(cAIEntityGroup *self, float dt);

/* ---- the AIs of the character classes (AI/AIBankDirector.cpp .. AISentinel.cpp, src/game/AI/AI<Class>.c): a cAIChar
 * with its own slots and its own move control (cAIControl<Class>, aicharcontrol.h). The constructors are declared in
 * walkcharentity.h. Most of them replace the action check (CheckActDist, the action button at the target) and the
 * distances of the states (SetDists); the ones that shoot take their parameters in SetDists2 ---- */
#define CAIBANKDIRECTOR_VFTABLE GAME_VFTABLE(cAIBankDirector_vftable)
#define CAIBANKMASTER_VFTABLE GAME_VFTABLE(cAIBankMaster_vftable)
#define CAIBISHOP_VFTABLE GAME_VFTABLE(cAIBishop_vftable)
#define CAIDESKTOP_VFTABLE GAME_VFTABLE(cAIDesktop_vftable)
#define CAIGEAR_VFTABLE GAME_VFTABLE(cAIGear_vftable)
#define CAIHUMAN_VFTABLE GAME_VFTABLE(cAIHuman_vftable)
#define CAIMAYOR_VFTABLE GAME_VFTABLE(cAIMayor_vftable)
#define CAIMESSENGER_VFTABLE GAME_VFTABLE(cAIMessenger_vftable)
#define CAINURSE_VFTABLE GAME_VFTABLE(cAINurse_vftable)
#define CAIPOLICE_VFTABLE GAME_VFTABLE(cAIPolice_vftable)
#define CAISENTINEL_VFTABLE GAME_VFTABLE(cAISentinel_vftable)

/* the director and the master (0x384 bytes) */
typedef struct cAIBank {
	cAIChar base;			/* 0x000 */
	int money;			/* 0x380 the money of the character at the demand of the attack (state 2) */
} cAIBank;
SD_SIZE(cAIBank, 0x384);
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
SD_SIZE(cAIBishop, 0x398);

/* the gear (0x390 bytes): the shot of the bishop without the time of the button */
typedef struct cAIGear {
	cAIChar base;			/* 0x000 */
	int targetLife;			/* 0x380 */
	byte blocked;			/* 0x384 */
	byte pad_385[3];
	int shootFactor;		/* 0x388 ShootGearAI */
	float shootAng;			/* 0x38c OffsetAngShootGearAI */
} cAIGear;
SD_SIZE(cAIGear, 0x390);

/* the messenger (0x390 bytes) */
typedef struct cAIMessenger {
	cAIChar base;			/* 0x000 */
	int shootFactor;		/* 0x380 ShootMessengerAI */
	float shootAng;			/* 0x384 OffsetAngShootMessengerAI */
	float shootTime;		/* 0x388 TimeShootMessengerAI */
	float shootDist;		/* 0x38c DistMaxShootMessengerAI */
} cAIMessenger;
SD_SIZE(cAIMessenger, 0x390);

/* the nurse (0x384 bytes) */
typedef struct cAINurse {
	cAIChar base;			/* 0x000 */
	byte fresh;			/* 0x380 set by the start of state 2: the action within actMax at once */
	byte pad_381[3];
} cAINurse;
SD_SIZE(cAINurse, 0x384);

/* AIBankDirector.c, AIBankMaster.c */
cAIBankDirector *cAIBankDirector_Delete(cAIBankDirector *self, byte flags);
cAIBankMaster *cAIBankMaster_Delete(cAIBankMaster *self, byte flags);
/* vf66 of the director and the master: the attack when the character got money or the control is idle */
void cAIBank_CheckAttack(cAIBank *self);
/* vf56 */
void cAIBankDirector_SetRanges(cAIBankDirector *self);
void cAIBankMaster_SetRanges(cAIBankMaster *self);
/* vf27 and vf50 of both: the state from outside and from inside (ATTACK, 2, becomes 7 with the money noted) */
byte cAIBank_SetState(cAIBank *self, int state, ...);
byte cAIBank_SetStateIntern(cAIBank *self, int state, ...);
/* AIBishop.c */
cAIChar *cAIBishop_Delete(cAIChar *self, byte flags);
/* vf66: the shot at the target in the cone of the view */
void cAIBishop_CheckAttack(cAIBishop *self);
void cAIBishop_SetRanges(cAIBishop *self);
/* vf57: the shot parameters for a target with or without an AI, then the ranges */
void cAIBishop_SetShoot(cAIBishop *self);
/* AIDesktop.c: the action button while it flees or patrols (the alarm on) without a strafe, but not in a fall or
 * the ActMore and ActUp actions */
void cAIDesktop_dtor(cAIDesktop *self);
cAIDesktop *cAIDesktop_Delete(cAIDesktop *self, byte flags);
void cAIDesktop_UpdateAction0(cAIDesktop *self);
/* AIGear.c */
cAIChar *cAIGear_Delete(cAIChar *self, byte flags);
/* vf65: blocked by a paused AI it ran into (not the gate keeper, Sebastian): the action button */
void cAIGear_UpdateAction(cAIGear *self);
void cAIGear_CheckActDist(cAIGear *self);
void cAIGear_SetDists(cAIGear *self);
void cAIGear_SetShoot(cAIGear *self);
/* the way to the target was not free (vf89 of the move control asks it) */
byte cAIGear_IsBlocked(cAIGear *self);
/* AIHuman.c */
cAIHuman *cAIHuman_Delete(cAIHuman *self, byte flags);
void cAIHuman_SetDists(cAIHuman *self);
/* AIMayor.c */
cAIMayor *cAIMayor_Delete(cAIMayor *self, byte flags);
void cAIMayor_CheckActDist(cAIMayor *self);
void cAIMayor_SetDists(cAIMayor *self);
/* AIMessenger.c */
cAIChar *cAIMessenger_Delete(cAIChar *self, byte flags);
void cAIMessenger_CheckActDist(cAIMessenger *self);
void cAIMessenger_SetDists(cAIMessenger *self);
void cAIMessenger_SetShoot(cAIMessenger *self);
/* AINurse.c */
cAIChar *cAINurse_Delete(cAIChar *self, byte flags);
/* the action at a dazed target */
void cAINurse_CheckActDist(cAINurse *self);
void cAINurse_SetDists(cAINurse *self);
/* vf27 and vf50: the state from outside and from inside, the start of state 2 marked */
byte cAINurse_SetState(cAINurse *self, int state, ...);
byte cAINurse_SetStateIntern(cAINurse *self, int state, ...);
/* AIPolice.c */
cAIPolice *cAIPolice_Delete(cAIPolice *self, byte flags);
void cAIPolice_CheckActDist(cAIPolice *self);
void cAIPolice_SetDists(cAIPolice *self);
/* AISentinel.c */
cAISentinel *cAISentinel_Delete(cAISentinel *self, byte flags);
void cAISentinel_CheckActDist(cAISentinel *self);
void cAISentinel_SetDists(cAISentinel *self);

/* cAIChar_dtor, cAIChar_SetDists (vf56) and cAIChar_ChangeState (vf26): aichar.h; the move controls:
 * aicharcontrol.h */

#define g_MaxHeightChaseCharAI GAME_GLOBAL_IN(float, GVar_MaxHeightChaseCharAI, 16)
#define g_BankMasterVacumDist GAME_GLOBAL_IN(float, GVar_BankMasterVacumDist, 16)
/* the shot of the bishop: the value against a target without an AI, the second one (the default) with one */
#define g_ShootBishopAI GAME_GLOBAL_IN(int, GVar_ShootBishopAI, 16)
#define g_ShootBishopAIDef GAME_GLOBAL_IN(int, GVar_ShootBishopAI, 20)
#define g_OffsetAngShootBishopAI GAME_GLOBAL_IN(float, GVar_OffsetAngShootBishopAI, 16)
#define g_OffsetAngShootBishopAIDef GAME_GLOBAL_IN(float, GVar_OffsetAngShootBishopAI, 20)
#define g_TimeShootBishopAI GAME_GLOBAL_IN(float, GVar_TimeShootBishopAI, 16)
#define g_TimeShootBishopAIDef GAME_GLOBAL_IN(float, GVar_TimeShootBishopAI, 20)
#define g_DistMaxShootBishopAI GAME_GLOBAL_IN(float, GVar_DistMaxShootBishopAI, 16)
#define g_DistMaxShootBishopAIDef GAME_GLOBAL_IN(float, GVar_DistMaxShootBishopAI, 20)
/* the same pairs of the gear */
#define g_ShootGearAI GAME_GLOBAL_IN(int, GVar_ShootGearAI, 16)
#define g_ShootGearAIDef GAME_GLOBAL_IN(int, GVar_ShootGearAI, 20)
#define g_OffsetAngShootGearAI GAME_GLOBAL_IN(float, GVar_OffsetAngShootGearAI, 16)
#define g_OffsetAngShootGearAIDef GAME_GLOBAL_IN(float, GVar_OffsetAngShootGearAI, 20)
#define g_ShootMessengerAI GAME_GLOBAL_IN(int, GVar_ShootMessengerAI, 16)
#define g_OffsetAngShootMessengerAI GAME_GLOBAL_IN(float, GVar_OffsetAngShootMessengerAI, 16)
#define g_TimeShootMessengerAI GAME_GLOBAL_IN(float, GVar_TimeShootMessengerAI, 16)
#define g_DistMaxShootMessengerAI GAME_GLOBAL_IN(float, GVar_DistMaxShootMessengerAI, 16)
#define g_DistActionNurseAI GAME_GLOBAL_IN(float, GVar_DistActionNurseAI, 16)
#define g_OffsetMoveNurseAI GAME_GLOBAL_IN(float, GVar_OffsetMoveNurseAI, 16)

#endif
