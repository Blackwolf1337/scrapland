/*
 * AI (original source: AI/): navigation graphs, traffic, A*, AI groups and the AI controllers of vehicles and
 * characters. The graph objects are made by SAI.IniAI with the engine's operator new; their globals keep the
 * names of the CHECK expressions in PyInterface/SAIPy.cpp.
 */
#ifndef SCRAP_AI_H
#define SCRAP_AI_H

#include "scrap/entity.h"
#include "scrap/python.h"

typedef struct CNodesWorld CNodesWorld;		/* RTTI CNodesWorld: visibility graph of the level */
typedef struct CNodesWorld2D CNodesWorld2D;
typedef struct cAIVehicleGroup cAIVehicleGroup;
typedef struct cAICharGroup cAICharGroup;
typedef struct cAIVehicle cAIVehicle;
typedef struct cAIChar cAIChar;

typedef struct CNodesWorldVtbl {
	void *vf00_0a[11];
	const vec3_t *(__thiscall *GetVisPoint)(CNodesWorld *self, int i);	/* 0x2c */
	int(__thiscall *NumVisPoints)(CNodesWorld *self);			/* 0x30 */
} CNodesWorldVtbl;
SD_OFFSET(CNodesWorldVtbl, GetVisPoint, 0x2c);

struct CNodesWorld {
	const CNodesWorldVtbl *vftable;
};

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
} cAIVehicleGroupVtbl;

struct cAIVehicleGroup {
	const cAIVehicleGroupVtbl *vftable;
};

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

/* the three parameter blocks of SAI.EnableAIVehicle; field meaning not known yet, named by type and order */
typedef struct cAIVehicleParams1 {
	float f0, f1, f2;
	int i3, i4, i5, i6, i7, i8, i9, i10;
} cAIVehicleParams1;
SD_SIZE(cAIVehicleParams1, 0x2c);

typedef struct cAIVehicleParams2 {
	int i0, i1, i2, i3;
	float f4;
	int i5;
} cAIVehicleParams2;
SD_SIZE(cAIVehicleParams2, 0x18);

typedef struct cAIVehicleParams3 {
	int i0, i1;
	float f2, f3;
	int i4, i5, i6;
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

/* < 0 when no player controls the entity (assumed from SAI.EnableAI*) */
int __thiscall cVehicleEntity_GetOwnerSlot(cEntity *e);
int __thiscall cWalkCharEntity_GetOwnerSlot(cEntity *e);

/* the state of the vehicle AI, arguments by state (see SAIPy.c); 0 if it was not taken (rva 0x8dfb0) */
byte cAIVehicle_SetState(cAIVehicle *ai, int state, ...);

/* default values of SAI.SetStateVehicle CHASE (state 7) */
#define g_AIChaseDefault1 GAME_DATA(float, 0x444948)
#define g_AIChaseDefault2 GAME_DATA(float, 0x444828)

#endif
