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

typedef struct CNodesWorld CNodesWorld;		/* RTTI CNodesWorld: visibility graph of the level */
typedef struct CNodesWorld2D CNodesWorld2D;
typedef struct cAIVehicleGroup cAIVehicleGroup;
typedef struct cAICharGroup cAICharGroup;
typedef struct cAIVehicle cAIVehicle;
typedef struct cAIChar cAIChar;

/* CNodesTraffic or CNodesTrafficInDoor */
typedef struct cNodesTraffic cNodesTraffic;
typedef struct cNodesTrafficVtbl {
	void *vf00_06[7];
	void(__thiscall *Init)(cNodesTraffic *self);		/* 0x1c */
	void *vf08_0f[8];
	void(__thiscall *Analyze)(cNodesTraffic *self);		/* 0x40 */
} cNodesTrafficVtbl;
SD_OFFSET(cNodesTrafficVtbl, Init, 0x1c);
SD_OFFSET(cNodesTrafficVtbl, Analyze, 0x40);

struct cNodesTraffic {
	const cNodesTrafficVtbl *vftable;
};

typedef struct cAIVehicleGroupVtbl {
	void *vf00;
	void(__thiscall *InitRace)(cAIVehicleGroup *self);			/* 0x4 */
	void(__thiscall *AddVehicle)(cAIVehicleGroup *self, cEntity *vehicle);	/* 0x8 */
	cEntity *(__thiscall *GetVehicle)(cAIVehicleGroup *self, int i);	/* 0xc */
	int(__thiscall *NumVehicles)(cAIVehicleGroup *self);			/* 0x10 */
	/* the time since the last change of state of a vehicle AI reached the limit */
	byte(__thiscall *CanChange)(cAIVehicleGroup *self);			/* 0x14 */
	void(__thiscall *ResetChange)(cAIVehicleGroup *self);			/* 0x18 the time to 0 */
	void(__thiscall *AllowChange)(cAIVehicleGroup *self);			/* 0x1c the time to the limit */
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

/* the first slots of every AI (cAIVehicle, cAIChar); variadic methods are __cdecl with self on the stack */
typedef struct cAIVtbl {
	void *vf00_05[6];
	void (*Debugf)(void *self, const char *fmt, ...);	/* 0x18 text of the AI debug view */
} cAIVtbl;
SD_OFFSET(cAIVtbl, Debugf, 0x18);

static inline void AI_Debugf(void *ai, const char *text) {
	(*(const cAIVtbl **)ai)->Debugf(ai, text);
}

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
	cAIChar *(__thiscall *Delete)(cAIChar *self, byte flags);					/* 0x00 */
	void(__thiscall *Update)(cAIChar *self, float dt);						/* 0x04 */
	void(__thiscall *UpdateOff)(cAIChar *self, float dt);						/* 0x08 */
	void(__thiscall *Draw)(cAIChar *self);								/* 0x0c */
	byte(__thiscall *GetChanged)(cAIChar *self);							/* 0x10 */
	byte(__thiscall *InView)(cAIChar *self);							/* 0x14 */
	void (*Debugf)(cAIChar *self, const char *fmt, ...);						/* 0x18 */
	byte(__thiscall *VarI)(cAIChar *self, const char *name, int *i, byte set);			/* 0x1c */
	byte(__thiscall *VarF)(cAIChar *self, const char *name, double *d, byte set);			/* 0x20 */
	byte(__thiscall *VarS)(cAIChar *self, const char *name, char **s, byte set);			/* 0x24 */
	byte(__thiscall *VarV)(cAIChar *self, const char *name, double *x, double *y, double *z, byte set);	/* 0x28 */
	void *vf0b_0c[2];
	void(__thiscall *OnEvent)(cAIChar *self, int ev);						/* 0x34 the debug text */
	Nto1Node *(__thiscall *GetTarget)(cAIChar *self);						/* 0x38 the link to the target */
	byte(__thiscall *CanJump)(cAIChar *self);							/* 0x3c byte 0x48 of the AI */
	byte(__thiscall *GetStopped)(cAIChar *self);							/* 0x40 */
	byte(__thiscall *IsPaused)(cAIChar *self);							/* 0x44 */
	cEntity *(__thiscall *GetTargetEnt)(cAIChar *self);						/* 0x48 */
	byte(__thiscall *GetArrived)(cAIChar *self);							/* 0x4c */
	/* a random point within r of p the character can reach (p itself after three tries), out returned */
	vec3_t *(__thiscall *RandomPoint)(cAIChar *self, vec3_t *out, const vec3_t *p, float r);	/* 0x50 */
	/* moving without a velocity for 10 s: stopped and the event 7 */
	void(__thiscall *CheckStopped)(cAIChar *self, float dt);					/* 0x54 */
	/* the move tells the AI (arrived, blocked, ..): the reaction of the state, else EventDefault */
	byte(__thiscall *Event)(cAIChar *self, int ev);							/* 0x58 */
	byte(__thiscall *IsMoving)(cAIChar *self);							/* 0x5c */
	byte(__thiscall *CanTarget)(cAIChar *self, cEntity *e);					/* 0x60 */
	void(__thiscall *CheckParams)(cAIChar *self, int stupidPathfinding);				/* 0x64 */
	byte(__thiscall *ChangeState)(cAIChar *self, int state, va_list ap);				/* 0x68 */
	/* SAI_CHAR_ state; the arguments depend on the state (see SAIPy.c) */
	byte (*SetState)(cAIChar *self, int state, ...);						/* 0x6c */
	byte(__thiscall *RestoreState)(cAIChar *self, const cAICharSaved *s);				/* 0x70 */
	void(__thiscall *SetInternStateV)(cAIChar *self, int state, va_list ap);			/* 0x74 */
	void (*SetInternState)(cAIChar *self, int state, ...);						/* 0x78 */
	int(__thiscall *GetState)(cAIChar *self);							/* 0x7c */
	int(__thiscall *GetInternState)(cAIChar *self);							/* 0x80 */
	void(__thiscall *StateName)(cAIChar *self, int state, char *buf);				/* 0x84 */
	void(__thiscall *GetStateName)(cAIChar *self, char *buf);					/* 0x88 */
	void(__thiscall *InternStateName)(cAIChar *self, int state, char *buf);				/* 0x8c */
	void(__thiscall *GetInternStateName)(cAIChar *self, char *buf);					/* 0x90 */
	byte(__thiscall *GetDirect)(cAIChar *self);							/* 0x94 */
	byte(__thiscall *GetDirect2)(cAIChar *self);							/* 0x98 */
	cAICharControl *(__thiscall *GetControl)(cAIChar *self);					/* 0x9c */
	cAICharSaved *(__thiscall *GetLast)(cAIChar *self);						/* 0xa0 */
	cAICharSaved *(__thiscall *SaveTo)(cAIChar *self, cAICharSaved *s);				/* 0xa4 */
	byte(__thiscall *SetRepos)(cAIChar *self, byte on, byte now);					/* 0xa8 */
	byte(__thiscall *GetRepos)(cAIChar *self);							/* 0xac */
	byte(__thiscall *IsTalking)(cAIChar *self);							/* 0xb0 */
	cEntity *(__thiscall *GetCollide)(cAIChar *self);						/* 0xb4 */
	byte(__thiscall *React)(cAIChar *self);								/* 0xb8 */
	byte(__thiscall *CheckAction)(cAIChar *self);							/* 0xbc */
	byte(__thiscall *GetChanged2)(cAIChar *self);							/* 0xc0 */
	byte(__thiscall *CanTalk)(cAIChar *self);							/* 0xc4 */
	byte (*SetStateIntern)(cAIChar *self, int state, ...);						/* 0xc8 */
	byte(__thiscall *RestoreStateIntern)(cAIChar *self, const cAICharSaved *s);			/* 0xcc */
	byte(__thiscall *ChangeStateMore)(cAIChar *self, int state, int allowed, va_list ap);		/* 0xd0 */
	byte(__thiscall *IsGroupState)(cAIChar *self, int state);					/* 0xd4 */
	byte(__thiscall *Repos)(cAIChar *self, float minDist, float maxDist, byte flag);		/* 0xd8 */
	void(__thiscall *AfterRepos)(cAIChar *self);							/* 0xdc */
	void(__thiscall *SetDists)(cAIChar *self);							/* 0xe0 */
	void(__thiscall *SetDists2)(cAIChar *self);							/* 0xe4 */
	void(__thiscall *BeginChange)(cAIChar *self, int state);					/* 0xe8 */
	void(__thiscall *SetPauseCheck)(cAIChar *self, byte on);					/* 0xec */
	void(__thiscall *SaveState)(cAIChar *self, cAICharSaved *s);					/* 0xf0 */
	void(__thiscall *RestoreSaved)(cAIChar *self, const cAICharSaved *s);				/* 0xf4 */
	void(__thiscall *SetPaused)(cAIChar *self, byte on);						/* 0xf8 */
	void(__thiscall *UpdateAction1)(cAIChar *self);							/* 0xfc */
	void(__thiscall *UpdateAction0)(cAIChar *self);							/* 0x100 */
	void(__thiscall *UpdateAction)(cAIChar *self);							/* 0x104 */
	void(__thiscall *CheckActDist)(cAIChar *self);							/* 0x108 */
	void(__thiscall *SetAction)(cAIChar *self, int action);						/* 0x10c */
	void(__thiscall *UpdateMove)(cAIChar *self, float dt);						/* 0x110 */
	byte(__thiscall *EventDefault)(cAIChar *self, int ev);						/* 0x114 */
	byte(__thiscall *EventInRoute)(cAIChar *self, int ev);						/* 0x118 */
	byte(__thiscall *EventPatrol)(cAIChar *self, int ev);						/* 0x11c */
	byte(__thiscall *EventGoTo)(cAIChar *self, int ev);						/* 0x120 */
	byte(__thiscall *EventGetAway)(cAIChar *self, int ev);						/* 0x124 */
	byte(__thiscall *EventChase)(cAIChar *self, int ev);						/* 0x128 */
	byte(__thiscall *EventChaseBack)(cAIChar *self, int ev);					/* 0x12c */
	byte(__thiscall *EventChaseNoAction)(cAIChar *self, int ev);					/* 0x130 */
	byte(__thiscall *EventLostPath)(cAIChar *self, int ev);						/* 0x134 */
	void(__thiscall *Resume)(cAIChar *self);							/* 0x138 */
	void(__thiscall *UpdateThink)(cAIChar *self);							/* 0x13c */
	void(__thiscall *CheckPause)(cAIChar *self);							/* 0x140 */
	void(__thiscall *CheckRepos)(cAIChar *self);							/* 0x144 */
	byte(__thiscall *CanRepos)(cAIChar *self, int state);						/* 0x148 */
	void(__thiscall *Think)(cAIChar *self);								/* 0x14c */
	void(__thiscall *ThinkInRoute)(cAIChar *self);							/* 0x150 */
	void(__thiscall *ThinkGetAway)(cAIChar *self);							/* 0x154 */
	void(__thiscall *ThinkChase)(cAIChar *self);							/* 0x158 */
	void(__thiscall *ThinkChaseBack)(cAIChar *self);						/* 0x15c */
	void(__thiscall *ThinkChaseNoAction)(cAIChar *self);						/* 0x160 */
	void(__thiscall *ThinkLostPath)(cAIChar *self);							/* 0x164 */
	void(__thiscall *ThinkLostPathChase)(cAIChar *self);						/* 0x168 */
	void(__thiscall *ReactLostPath)(cAIChar *self);							/* 0x16c */
	void(__thiscall *ReactLostPathChase)(cAIChar *self);						/* 0x170 */
	void(__thiscall *ReactPatrol)(cAIChar *self);							/* 0x174 */
	void(__thiscall *ReactInRoute)(cAIChar *self);							/* 0x178 */
	void(__thiscall *ThinkGoTo)(cAIChar *self);							/* 0x17c */
	void(__thiscall *ThinkPatrol)(cAIChar *self);							/* 0x180 */
	void(__thiscall *CheckTalk)(cAIChar *self);							/* 0x184 */
	byte(__thiscall *Facing)(cAIChar *self, cEntity *a, cEntity *b);				/* 0x188 */
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

/* 0x380 bytes; the first 0x90 are cAIMove (AI/AIMove.cpp) with its base cAI */
struct cAIChar {
	const cAICharVtbl *vftable;	/* 0x000 */
	byte pad_4[4];
	cEntity *owner;			/* 0x008 of cAI */
	float dt;			/* 0x00c the time of the frame */
	byte slotThink[0x10];		/* 0x010 the think slot of cAI (a cSlotController) */
	byte changing;			/* 0x020 a change of state runs */
	byte pad_21[3];
	int relaxCount;			/* 0x024 the relax actions played in the internal state */
	byte changed;			/* 0x028 the internal state changed in this frame */
	byte pad_29;
	byte b2a;			/* 0x02a */
	byte pad_2b;
	float stateTime;		/* 0x02c */
	byte pad_30[8];
	Nto1Node target;		/* 0x038 the target entity at 0x40, data the AI */
	byte flying;			/* 0x048 the 3D control (CanFly) */
	byte pad_49[3];
	vec3_t dest;			/* 0x04c */
	float dist;			/* 0x058 the goal is reached that near */
	int moveMode;			/* 0x05c AICC_ */
	float speed;			/* 0x060 */
	vec3_t dir;			/* 0x064 the direction of the orient and path moves */
	vec3_t center;			/* 0x070 PATROL_ZONE */
	float radius;			/* 0x07c PATROL_ZONE */
	byte pathA;			/* 0x080 */
	byte pathB;			/* 0x081 */
	byte stopped;			/* 0x082 CheckStopped: no velocity for 10 s */
	byte inPath;			/* 0x083 FOLLOW_THE_PATH set it */
	byte paused;			/* 0x084 far from the camera: no update */
	byte arrived;			/* 0x085 GO_TO reached the end */
	byte repos;			/* 0x086 "Repos" */
	byte reposing;			/* 0x087 inside Repos */
	byte b88;			/* 0x088 */
	byte pad_89[3];
	float stopTime;			/* 0x08c the time without a velocity */
	cAICharSaved pending;		/* 0x090 state AIC_STATE_NONE for none */
	byte slotAction[0x10];		/* 0x158 0.2 s, the distance check of the action */
	byte slotPause[0x10];		/* 0x168 1 s */
	byte slotRepos[0x10];		/* 0x178 1 s */
	byte slotLost[0x10];		/* 0x188 0.2 s, LOST_PATH looks for the graph */
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
SD_OFFSET(cAIChar, slotThink, 0x10);
SD_OFFSET(cAIChar, target, 0x38);
SD_OFFSET(cAIChar, center, 0x70);
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

#define g_IsMapOutDoor GAME_GLOBAL_IN(int, GVar_IsMapOutDoor, 16, 0x4677e8)	/* value of the engine variable IsMapOutDoor */
#define g_NodesWorld GAME_GLOBAL(CNodesWorld *, g_NodesWorld, 0x444d34)
#define g_NodesWorld2D GAME_GLOBAL(CNodesWorld2D *, g_NodesWorld2D, 0x444dc8)
#define g_NodesTrafficInDoor GAME_GLOBAL(cNodesTraffic *, g_NodesTrafficInDoor, 0x444d2c)
#define g_NodesTraffic GAME_GLOBAL(cNodesTraffic *, g_NodesTraffic, 0x444c98)
#define g_AStar GAME_GLOBAL(void *, g_AStar, 0x444c78)
#define g_AStar2D GAME_GLOBAL(void *, g_AStar2D, 0x444c7c)
#define g_VehicleGroup GAME_GLOBAL(cAIVehicleGroup *, g_VehicleGroup, 0x444b84)
#define g_CharGroup GAME_GLOBAL(cAICharGroup *, g_CharGroup, 0x4445b8)
#define g_TrafficState GAME_GLOBAL_IN(int, GVar_ShowTrafficAI, 16, 0x444d0c)	/* 3 after SAI.AnalizeTraffic */

/* constructors (thiscall on the memory from operator_new) */
CNodesWorld *__thiscall CNodesWorld_ctor(void *mem);
CNodesWorld2D *__thiscall CNodesWorld2D_ctor(void *mem);
cNodesTraffic *__thiscall CNodesTrafficInDoor_ctor(void *mem);
cNodesTraffic *__thiscall CNodesTraffic_ctor(void *mem);
void *__thiscall CAStar_ctor(void *mem, CNodesWorld *nodes);
void *__thiscall CAStar2D_ctor(void *mem, CNodesWorld2D *nodes);
cAIVehicleGroup *__thiscall cAIVehicleGroup_ctor(void *mem);
cAICharGroup *__thiscall cAICharGroup_ctor(void *mem);

void __thiscall CNodesWorld_Load(CNodesWorld *nodes, int arg, const char *name, const vec3_t *pos);
void __thiscall CNodesWorld_Analyze(CNodesWorld *nodes, const vec3_t *size);
void __thiscall CNodesWorld_AnalyzeGraph(CNodesWorld *nodes);
byte __thiscall CNodesWorld_GetRacePoint(CNodesWorld *nodes, vec3_t *out, const vec3_t *initial, float minDist,
	float maxDist, int arg);
void __thiscall CNodesWorld2D_Load(CNodesWorld2D *nodes, int arg, const char *name, const vec3_t *pos);
void __thiscall CNodesWorld2D_AnalyzeGraph(CNodesWorld2D *nodes);

byte __thiscall cAICharGroup_GetReposPos(cAICharGroup *group, const vec3_t *pos, const char *name, vec3_t *out,
	cEntity *target);

int __thiscall cAIVehicle_GetState(cAIVehicle *ai);
void __thiscall cAIVehicle_Setup(cAIVehicle *ai, cAIVehicleParams1 *p1, cAIVehicleParams2 *p2,
	cAIVehicleParams3 *p3);
cAIVehicleParams1 *__thiscall cAIVehicle_GetParams1(cAIVehicle *ai);
cAIVehicleParams2 *__thiscall cAIVehicle_GetParams2(cAIVehicle *ai);
cAIVehicleParams3 *__thiscall cAIVehicle_GetParams3(cAIVehicle *ai);
cEntity *__thiscall cAIVehicle_GetNearestItemLife(cAIVehicle *ai);
void __thiscall cAIVehicle_SetRotStatic(cAIVehicle *ai, float a, float b);
void __thiscall cAIVehicle_SetInertia(cAIVehicle *ai, int on);

/* the names of the state and its sub state for the debug texts (0x100 bytes) */
void __thiscall cAIVehicle_GetStateName(cAIVehicle *ai, char *out);
void __thiscall cAIVehicle_GetInternStateName(cAIVehicle *ai, char *out);

/* < 0 when no player controls the entity (assumed from SAI.EnableAI*) */
int __thiscall cVehicleEntity_GetOwnerSlot(cEntity *e);
int __thiscall cWalkCharEntity_GetOwnerSlot(cEntity *e);

/* the state of the vehicle AI, arguments by state (see SAIPy.c); 0 if it was not taken (rva 0x8dfb0) */
byte cAIVehicle_SetState(cAIVehicle *ai, int state, ...);

/* default values of SAI.SetStateVehicle CHASE (state 7) */
#define g_AIChaseDefault1 GAME_GLOBAL_IN(float, GVar_MinDistChaseAI, 16, 0x444948)
#define g_AIChaseDefault2 GAME_GLOBAL_IN(float, GVar_MaxDistChaseAI, 16, 0x444828)

/* ---- cAI (AI/AI.cpp, src/game/AI/AI.c): the base of every AI (0x38 bytes), the entity, the time of the frame and
 * of the state, and the slot of the reactions ---- */
#define CAI_VFTABLE GAME_VFTABLE(cAI_vftable, 0x3a3f18)

typedef struct cAI cAI;

/* the slots of cAI::vftable (Debugf is pure there) */
typedef struct cAIBaseVtbl {
	cAI *(__thiscall *Delete)(cAI *self, byte flags);					/* 0x00 */
	void(__thiscall *Update)(cAI *self, float dt);						/* 0x04 */
	void(__thiscall *UpdateOff)(cAI *self, float dt);					/* 0x08 */
	void(__thiscall *Draw)(cAI *self);							/* 0x0c */
	byte(__thiscall *GetChanged)(cAI *self);						/* 0x10 */
	byte(__thiscall *IsVisible)(cAI *self);							/* 0x14 */
	void (*Debugf)(cAI *self, const char *fmt, ...);					/* 0x18 */
	byte(__thiscall *VarI)(cAI *self, const char *name, int *i, byte set);			/* 0x1c */
	byte(__thiscall *VarF)(cAI *self, const char *name, double *d, byte set);		/* 0x20 */
	byte(__thiscall *VarS)(cAI *self, const char *name, char **s, byte set);		/* 0x24 */
	byte(__thiscall *VarV)(cAI *self, const char *name, double *x, double *y, double *z, byte set);	/* 0x28 */
	void(__thiscall *Print)(cAI *self, const char *text);					/* 0x2c */
	void(__thiscall *GetEventName)(cAI *self, int ev, char *out);				/* 0x30 */
	void(__thiscall *OnEvent)(cAI *self, int ev);						/* 0x34 */
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
	byte slot[0x10];		/* 0x10 cSlotController of 0.5 s (slotcontroller.h) */
	byte changing;			/* 0x20 */
	byte pad_21[3];
	int i24;			/* 0x24 */
	byte changed;			/* 0x28 GetChanged, cleared by every update */
	byte pad_29[3];
	float stateTime;		/* 0x2c the time in the state */
	float waitTime;			/* 0x30 counts while i34 is 0 */
	int i34;			/* 0x34 */
};
SD_OFFSET(cAI, changed, 0x28);
SD_SIZE(cAI, 0x38);

cAI *__thiscall cAI_ctor(cAI *self, cEntity *e);
cAI *__fastcall cAI_ctorDefault(cAI *self);
void __fastcall cAI_dtor(cAI *self);
cAI *__thiscall cAI_Delete(cAI *self, byte flags);
void __thiscall cAI_Update(cAI *self, float dt);
void __thiscall cAI_UpdateOff(cAI *self, float dt);
void __thiscall cAI_Draw(cAI *self);
byte __thiscall cAI_GetChanged(cAI *self);
/* the entity is in the sector of the camera of view 0 or seen from it through the portals */
byte __thiscall cAI_IsVisible(cAI *self);
byte __thiscall cAI_VarI(cAI *self, const char *name, int *i, byte set);
byte __thiscall cAI_VarF(cAI *self, const char *name, double *d, byte set);
byte __thiscall cAI_VarS(cAI *self, const char *name, char **s, byte set);
byte __thiscall cAI_VarV(cAI *self, const char *name, double *x, double *y, double *z, byte set);
/* text as a format into the console */
void __thiscall cAI_Print(cAI *self, const char *text);
/* the name of the AIEV_ event into out, nothing for the others */
void __thiscall cAI_GetEventName(cAI *self, int ev, char *out);
void __thiscall cAI_OnEvent(cAI *self, int ev);

/* ---- cAIMove (AI/AIMove.cpp, src/game/AI/AIMove.c): the base of cAIChar and cAIVehicle (the first 0x90 bytes of
 * cAIChar: the target link, the move of the state, the check of a character that does not move); the prototypes in
 * aivehicle.h ---- */
#define CAIMOVE_VFTABLE GAME_VFTABLE(cAIMove_vftable, 0x3a7754)
#define CAIMOVE_SIZE 0x90

/* ---- cAIEntityGroup (AI/AIEntityGroup.cpp, src/game/AI/AIEntityGroup.c): the base of the vehicle and the character
 * groups, up to 20 entities (a race) and the time since an AI of the group took a group state; the next one may take
 * one when the time reached TimeBetwChangeStateAI ---- */
#define CAIENTITYGROUP_VFTABLE GAME_VFTABLE(cAIEntityGroup_vftable, 0x3a6f18)
#define CAIENTITYGROUP_SIZE 0x60
#define CAIENTITYGROUP_MAX 20

typedef struct cAIEntityGroup cAIEntityGroup;

typedef struct cAIEntityGroupVtbl {
	cAIEntityGroup *(__thiscall *Delete)(cAIEntityGroup *self, byte flags);	/* 0x00 */
	void(__thiscall *Clear)(cAIEntityGroup *self);				/* 0x04 no entities */
	byte(__thiscall *Add)(cAIEntityGroup *self, cEntity *e);			/* 0x08 0 when full */
	cEntity *(__thiscall *Get)(cAIEntityGroup *self, int i);			/* 0x0c */
	int(__thiscall *Num)(cAIEntityGroup *self);				/* 0x10 */
	byte(__thiscall *CanChange)(cAIEntityGroup *self);			/* 0x14 */
	void(__thiscall *ResetChange)(cAIEntityGroup *self);			/* 0x18 the time to 0 */
	void(__thiscall *AllowChange)(cAIEntityGroup *self);			/* 0x1c the time to the limit */
	void(__thiscall *Update)(cAIEntityGroup *self, float dt);		/* 0x20 the time on */
} cAIEntityGroupVtbl;
SD_OFFSET(cAIEntityGroupVtbl, Update, 0x20);

struct cAIEntityGroup {
	const cAIEntityGroupVtbl *vftable;	/* 0x00 */
	cEntity *ents[CAIENTITYGROUP_MAX];	/* 0x04 */
	int num;				/* 0x54 */
	byte b58;				/* 0x58 */
	byte pad_59[3];
	float changeTime;			/* 0x5c */
};
SD_OFFSET(cAIEntityGroup, num, 0x54);
SD_OFFSET(cAIEntityGroup, changeTime, 0x5c);
SD_SIZE(cAIEntityGroup, CAIENTITYGROUP_SIZE);

#define g_TimeBetwChangeStateAI GAME_GLOBAL_IN(float, GVar_TimeBetwChangeStateAI, 16, 0x444698)	/* "TimeBetwChangeStateAI" */

/* ctor and dtor are fastcall on the memory of the subclasses */
void *__fastcall cAIEntityGroup_ctor(void *self);
void __fastcall cAIEntityGroup_dtor(void *self);
cAIEntityGroup *__thiscall cAIEntityGroup_Delete(cAIEntityGroup *self, byte flags);
void __thiscall cAIEntityGroup_Clear(cAIEntityGroup *self);
byte __thiscall cAIEntityGroup_Add(cAIEntityGroup *self, cEntity *e);
cEntity *__thiscall cAIEntityGroup_Get(cAIEntityGroup *self, int i);
int __thiscall cAIEntityGroup_Num(cAIEntityGroup *self);
byte __thiscall cAIEntityGroup_CanChange(cAIEntityGroup *self);
void __thiscall cAIEntityGroup_ResetChange(cAIEntityGroup *self);
void __thiscall cAIEntityGroup_AllowChange(cAIEntityGroup *self);
void __thiscall cAIEntityGroup_Update(cAIEntityGroup *self, float dt);

/* ---- the AIs of the character classes (AI/AIBankDirector.cpp .. AISentinel.cpp, src/game/AI/AI<Class>.c): a cAIChar
 * with its own slots and its own move control (cAIControl<Class>, aicharcontrol.h). The constructors are declared in
 * walkcharentity.h. Most of them replace the action check (CheckActDist, the action button at the target) and the
 * distances of the states (SetDists); the ones that shoot take their parameters in SetDists2 ---- */
#define CAIBANKDIRECTOR_VFTABLE GAME_VFTABLE(cAIBankDirector_vftable, 0x3a40a8)
#define CAIBANKMASTER_VFTABLE GAME_VFTABLE(cAIBankMaster_vftable, 0x3a42b8)
#define CAIBISHOP_VFTABLE GAME_VFTABLE(cAIBishop_vftable, 0x3a452c)
#define CAIDESKTOP_VFTABLE GAME_VFTABLE(cAIDesktop_vftable, 0x3a6d14)
#define CAIGEAR_VFTABLE GAME_VFTABLE(cAIGear_vftable, 0x3a6fdc)
#define CAIHUMAN_VFTABLE GAME_VFTABLE(cAIHuman_vftable, 0x3a7180)
#define CAIMAYOR_VFTABLE GAME_VFTABLE(cAIMayor_vftable, 0x3a7310)
#define CAIMESSENGER_VFTABLE GAME_VFTABLE(cAIMessenger_vftable, 0x3a7580)
#define CAINURSE_VFTABLE GAME_VFTABLE(cAINurse_vftable, 0x3a7864)
#define CAIPOLICE_VFTABLE GAME_VFTABLE(cAIPolice_vftable, 0x3a7a14)
#define CAISENTINEL_VFTABLE GAME_VFTABLE(cAISentinel_vftable, 0x3a7ba8)

/* the director and the master (0x384 bytes) */
typedef struct cAIBank {
	cAIChar base;			/* 0x000 */
	int money;			/* 0x380 the money of the character at the demand of the attack (state 2) */
} cAIBank;
SD_SIZE(cAIBank, 0x384);

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
void __fastcall cAIBankDirector_dtor(cAIChar *self);
cAIChar *__thiscall cAIBankDirector_Delete(cAIChar *self, byte flags);
void __fastcall cAIBankMaster_dtor(cAIChar *self);
cAIChar *__thiscall cAIBankMaster_Delete(cAIChar *self, byte flags);
/* vf66 of the director and the master: the attack when the character got money or the control is idle */
void __thiscall cAIBank_CheckAttack(cAIBank *self);
/* vf56 */
void __thiscall cAIBankDirector_SetRanges(cAIBank *self);
void __thiscall cAIBankMaster_SetRanges(cAIBank *self);
/* vf27 and vf50 of both: the state from outside and from inside (ATTACK, 2, becomes 7 with the money noted) */
byte cAIBank_SetState(cAIBank *self, int state, ...);
byte cAIBank_SetStateIntern(cAIBank *self, int state, ...);
/* AIBishop.c */
void __fastcall cAIBishop_dtor(cAIChar *self);
cAIChar *__thiscall cAIBishop_Delete(cAIChar *self, byte flags);
/* vf66: the shot at the target in the cone of the view */
void __thiscall cAIBishop_CheckAttack(cAIBishop *self);
void __thiscall cAIBishop_SetRanges(cAIBishop *self);
/* vf57: the shot parameters for a target with or without an AI, then the ranges */
void __thiscall cAIBishop_SetShoot(cAIBishop *self);
/* AIDesktop.c: the action button while it flees or patrols (the alarm on) without a strafe, but not in a fall or
 * the ActMore and ActUp actions */
void __fastcall cAIDesktop_dtor(cAIChar *self);
cAIChar *__thiscall cAIDesktop_Delete(cAIChar *self, byte flags);
void __thiscall cAIDesktop_UpdateAction0(cAIChar *self);
/* AIGear.c */
void __fastcall cAIGear_dtor(cAIChar *self);
cAIChar *__thiscall cAIGear_Delete(cAIChar *self, byte flags);
/* vf65: blocked by a paused AI it ran into (not the gate keeper, Sebastian): the action button */
void __thiscall cAIGear_UpdateAction(cAIGear *self);
void __thiscall cAIGear_CheckActDist(cAIGear *self);
void __thiscall cAIGear_SetDists(cAIGear *self);
void __thiscall cAIGear_SetShoot(cAIGear *self);
/* the way to the target was not free (vf89 of the move control asks it) */
byte __fastcall cAIGear_IsBlocked(cAIGear *self);
/* AIHuman.c */
void __fastcall cAIHuman_dtor(cAIChar *self);
cAIChar *__thiscall cAIHuman_Delete(cAIChar *self, byte flags);
void __thiscall cAIHuman_SetDists(cAIChar *self);
/* AIMayor.c */
void __fastcall cAIMayor_dtor(cAIChar *self);
cAIChar *__thiscall cAIMayor_Delete(cAIChar *self, byte flags);
void __thiscall cAIMayor_CheckActDist(cAIChar *self);
void __thiscall cAIMayor_SetDists(cAIChar *self);
/* AIMessenger.c */
void __fastcall cAIMessenger_dtor(cAIChar *self);
cAIChar *__thiscall cAIMessenger_Delete(cAIChar *self, byte flags);
void __thiscall cAIMessenger_CheckActDist(cAIMessenger *self);
void __thiscall cAIMessenger_SetDists(cAIMessenger *self);
void __thiscall cAIMessenger_SetShoot(cAIMessenger *self);
/* AINurse.c */
void __fastcall cAINurse_dtor(cAIChar *self);
cAIChar *__thiscall cAINurse_Delete(cAIChar *self, byte flags);
/* the action at a dazed target */
void __thiscall cAINurse_CheckActDist(cAINurse *self);
void __thiscall cAINurse_SetDists(cAINurse *self);
/* vf27 and vf50: the state from outside and from inside, the start of state 2 marked */
byte cAINurse_SetState(cAINurse *self, int state, ...);
byte cAINurse_SetStateIntern(cAINurse *self, int state, ...);
/* AIPolice.c */
void __fastcall cAIPolice_dtor(cAIChar *self);
cAIChar *__thiscall cAIPolice_Delete(cAIChar *self, byte flags);
void __thiscall cAIPolice_CheckActDist(cAIChar *self);
void __thiscall cAIPolice_SetDists(cAIChar *self);
/* AISentinel.c */
void __fastcall cAISentinel_dtor(cAIChar *self);
cAIChar *__thiscall cAISentinel_Delete(cAIChar *self, byte flags);
void __thiscall cAISentinel_CheckActDist(cAIChar *self);
void __thiscall cAISentinel_SetDists(cAIChar *self);

/* cAIChar_dtor, cAIChar_SetDists (vf56) and cAIChar_ChangeState (vf26): aichar.h; the move controls:
 * aicharcontrol.h */

#define g_MaxHeightChaseCharAI GAME_GLOBAL_IN(float, GVar_MaxHeightChaseCharAI, 16, 0x4442e8)
#define g_BankMasterVacumDist GAME_GLOBAL_IN(float, GVar_BankMasterVacumDist, 16, 0x4460dc)
/* the shot of the bishop: the value against a target without an AI, the second one (the default) with one */
#define g_ShootBishopAI GAME_GLOBAL_IN(int, GVar_ShootBishopAI, 16, 0x4441bc)
#define g_ShootBishopAIDef GAME_GLOBAL_IN(int, GVar_ShootBishopAI, 20, 0x4441c0)
#define g_OffsetAngShootBishopAI GAME_GLOBAL_IN(float, GVar_OffsetAngShootBishopAI, 16, 0x4441d4)
#define g_OffsetAngShootBishopAIDef GAME_GLOBAL_IN(float, GVar_OffsetAngShootBishopAI, 20, 0x4441d8)
#define g_TimeShootBishopAI GAME_GLOBAL_IN(float, GVar_TimeShootBishopAI, 16, 0x444204)
#define g_TimeShootBishopAIDef GAME_GLOBAL_IN(float, GVar_TimeShootBishopAI, 20, 0x444208)
#define g_DistMaxShootBishopAI GAME_GLOBAL_IN(float, GVar_DistMaxShootBishopAI, 16, 0x4441ec)
#define g_DistMaxShootBishopAIDef GAME_GLOBAL_IN(float, GVar_DistMaxShootBishopAI, 20, 0x4441f0)
/* the same pairs of the gear */
#define g_ShootGearAI GAME_GLOBAL_IN(int, GVar_ShootGearAI, 16, 0x4446b0)
#define g_ShootGearAIDef GAME_GLOBAL_IN(int, GVar_ShootGearAI, 20, 0x4446b4)
#define g_OffsetAngShootGearAI GAME_GLOBAL_IN(float, GVar_OffsetAngShootGearAI, 16, 0x4446c8)
#define g_OffsetAngShootGearAIDef GAME_GLOBAL_IN(float, GVar_OffsetAngShootGearAI, 20, 0x4446cc)
#define g_ShootMessengerAI GAME_GLOBAL_IN(int, GVar_ShootMessengerAI, 16, 0x4446f8)
#define g_OffsetAngShootMessengerAI GAME_GLOBAL_IN(float, GVar_OffsetAngShootMessengerAI, 16, 0x4446e0)
#define g_TimeShootMessengerAI GAME_GLOBAL_IN(float, GVar_TimeShootMessengerAI, 16, 0x444710)
#define g_DistMaxShootMessengerAI GAME_GLOBAL_IN(float, GVar_DistMaxShootMessengerAI, 16, 0x444728)
#define g_DistActionNurseAI GAME_GLOBAL_IN(float, GVar_DistActionNurseAI, 16, 0x44475c)
#define g_OffsetMoveNurseAI GAME_GLOBAL_IN(float, GVar_OffsetMoveNurseAI, 16, 0x444774)

#endif
