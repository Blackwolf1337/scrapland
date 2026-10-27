/*
 * AI (original source: AI/): navigation graphs, traffic, A*, AI groups and the AI controllers of vehicles and
 * characters. The graph objects are made by SAI.IniAI with the engine's operator new; their globals keep the
 * names of the CHECK expressions in PyInterface/SAIPy.cpp.
 */
#ifndef SCRAP_AI_H
#define SCRAP_AI_H

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
	void(__thiscall *Analize)(cNodesTraffic *self);		/* 0x40 */
} cNodesTrafficVtbl;
SD_OFFSET(cNodesTrafficVtbl, Init, 0x1c);
SD_OFFSET(cNodesTrafficVtbl, Analize, 0x40);

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

typedef struct cAICharVtbl {
	cAIVtbl base;
	void *vf07_1a[20];
	/* SAI_CHAR_ state; the arguments depend on the state (see SAIPy.c) */
	byte (*SetState)(cAIChar *self, int state, ...);	/* 0x6c */
	void *vf1c_1e[3];
	int(__thiscall *GetState)(cAIChar *self);	/* 0x7c */
} cAICharVtbl;
SD_OFFSET(cAICharVtbl, SetState, 0x6c);
SD_OFFSET(cAICharVtbl, GetState, 0x7c);

struct cAIChar {
	const cAICharVtbl *vftable;
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

#define g_IsMapOutDoor GAME_DATA(int, 0x4677e8)	/* value of the engine variable IsMapOutDoor */
#define g_NodesWorld GAME_DATA(CNodesWorld *, 0x444d34)
#define g_NodesWorld2D GAME_DATA(CNodesWorld2D *, 0x444dc8)
#define g_NodesTrafficInDoor GAME_DATA(cNodesTraffic *, 0x444d2c)
#define g_NodesTraffic GAME_DATA(cNodesTraffic *, 0x444c98)
#define g_AStar GAME_DATA(void *, 0x444c78)
#define g_AStar2D GAME_DATA(void *, 0x444c7c)
#define g_VehicleGroup GAME_DATA(cAIVehicleGroup *, 0x444b84)
#define g_CharGroup GAME_DATA(cAICharGroup *, 0x4445b8)
#define g_TrafficState GAME_DATA(int, 0x444d0c)	/* 3 after SAI.AnalizeTraffic */

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
void __thiscall CNodesWorld_Analize(CNodesWorld *nodes, const vec3_t *size);
void __thiscall CNodesWorld_AnalizeGraph(CNodesWorld *nodes);
byte __thiscall CNodesWorld_GetRacePoint(CNodesWorld *nodes, vec3_t *out, const vec3_t *initial, float minDist,
	float maxDist, int arg);
void __thiscall CNodesWorld2D_Load(CNodesWorld2D *nodes, int arg, const char *name, const vec3_t *pos);
void __thiscall CNodesWorld2D_AnalizeGraph(CNodesWorld2D *nodes);

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
#define g_AIChaseDefault1 GAME_DATA(float, 0x444948)
#define g_AIChaseDefault2 GAME_DATA(float, 0x444828)

#endif
