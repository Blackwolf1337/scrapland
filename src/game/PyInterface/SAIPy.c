/*
 * Python module SAI (function table SAI_methods): AI setup and control of vehicles and characters.
 * Original source: PyInterface/SAIPy.cpp, with the state handlers behind SetStateVehicle/SetStateChar (static
 * functions there).
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/astar.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/nodestraffic.h"
#include "scrap/nodesworld2d.h"
#include "scrap/scriptapi.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"

/* new T, NULL if the memory is not there (the original wraps the constructor in an EH frame for the delete) */
#define NEW_OBJ(type, ctor, ...) ({ void *mem_ = operator_new(sizeof(type)); mem_ ? ctor(mem_, ##__VA_ARGS__) : NULL; })

/* an entity that is a Car or WalkChar with active set, else NULL */
static cEntity *FindAIEntity(const char *name, const char *type) {
	cEntity *e = World_FindEntity(name);
	if (!e || !e->active || !Entity_IsType(e, type)) {
		return NULL;
	}
	return e;
}

CNodesTraffic *g_NodesTraffic;
CNodesWorld *g_NodesWorld;

/* IniAI(levelname, IsMapOutDoor) : creates the navigation graphs, traffic, A* and AI groups of the level */
API_FN(SAI, IniAI) {
	const char *name;
	if (!Mode_Parse(a, n, "si", &name, &g_IsMapOutDoor)) {
		return 0;
	}
	g_NodesWorld = NEW_OBJ(CNodesWorld, CNodesWorld_ctor);
	CHECK(g_NodesWorld);
	vec3_t zero = { 0.0f, 0.0f, 0.0f };
	CNodesWorld_Load(g_NodesWorld, -1, name, &zero);
	CNodesTraffic *traffic;
	if (!g_IsMapOutDoor) {
		g_NodesWorld2D = NEW_OBJ(CNodesWorld2D, CNodesWorld2D_ctor);
		CHECK(g_NodesWorld2D);
		CNodesWorld2D_Load(g_NodesWorld2D, -1, name, &zero);
	}
	if (!g_IsMapOutDoor) { /* read again: loading the 2D graph may change it */
		g_NodesTrafficInDoor = NEW_OBJ(CNodesTrafficInDoor, CNodesTrafficInDoor_ctor);
		CHECK(g_NodesTrafficInDoor);
		traffic = g_NodesTrafficInDoor;
	}
	else {
		g_NodesTraffic = NEW_OBJ(CNodesTraffic, CNodesTraffic_ctor);
		CHECK(g_NodesTraffic);
		traffic = g_NodesTraffic;
	}
	Traffic_Init(traffic);
	g_AStar = NEW_OBJ(CAStar, CAStar_ctor, g_NodesWorld);
	CHECK(g_AStar);
	if (!g_IsMapOutDoor) {
		g_AStar2D = NEW_OBJ(CAStar2D, CAStar2D_ctor, g_NodesWorld2D);
		CHECK(g_AStar2D);
	}
	g_VehicleGroup = NEW_OBJ(cAIVehicleGroup, cAIVehicleGroup_ctor);
	CHECK(g_VehicleGroup);
	if (!g_IsMapOutDoor) {
		g_CharGroup = NEW_OBJ(cAICharGroup, cAICharGroup_ctor);
		CHECK(g_CharGroup);
	}
	return 1;
}

/* BuildGraph(arg, x, y, z) */
API_FN(SAI, BuildGraph) {
	int arg;
	float x, y, z;
	if (!Mode_Parse(a, n, "ifff", &arg, &x, &y, &z)) {
		return 0;
	}
	vec3_t pos = { x, y, z };
	CNodesWorld_Load(g_NodesWorld, arg, "", &pos);
	return 1;
}

/* AnalizeMap(size) : analyses the level with a cube of that size; makes the graph if there is none */
API_FN(SAI, AnalyzeMap) {
	float size;
	if (!Mode_Parse(a, n, "f", &size)) {
		return 0;
	}
	vec3_t cube = { size, size, size };
	if (!g_NodesWorld) {
		g_NodesWorld = NEW_OBJ(CNodesWorld, CNodesWorld_ctor);
		CHECK(g_NodesWorld);
	}
	CNodesWorld_Analyze(g_NodesWorld, &cube);
	return 1;
}

/* AnalizeGraph() : Analiza características grafo. No argument check. */
API_FN(SAI, AnalyzeGraph) {
	CNodesWorld_AnalyzeGraph(g_NodesWorld);
	return 1;
}

/* AnalizeGraph2D() : No argument check. */
API_FN(SAI, AnalyzeGraph2D) {
	CNodesWorld2D_AnalyzeGraph(g_NodesWorld2D);
	return 1;
}

/* AnalizeTraffic() : traffic graph of the level (indoor or outdoor) */
API_FN(SAI, AnalyzeTraffic) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	CNodesTraffic *traffic = g_IsMapOutDoor ? g_NodesTraffic : g_NodesTrafficInDoor;
	Traffic_Analyze(traffic);
	g_TrafficState = 3;
	return 1;
}

/* BuildGraph2D(arg, x, y, z) : indoor levels only */
API_FN(SAI, BuildGraph2D) {
	int arg;
	float x, y, z;
	CHECK(!g_IsMapOutDoor);
	if (!Mode_Parse(a, n, "ifff", &arg, &x, &y, &z)) {
		return 0;
	}
	vec3_t pos = { x, y, z };
	CNodesWorld2D_Load(g_NodesWorld2D, arg, "", &pos);
	return 1;
}

/*
 * State handlers of SetStateVehicle/SetStateChar. Each parses (state, entity[, target], ...), checks the
 * entities, writes "CAMBIO de ESTADO desde PYTHON" into the AI debug view and sets the state; 1 if the AI took
 * it, else the error report below and None. The original passes the entity name uninitialized to the report when
 * parsing failed before it; it is NULL here.
 */
enum {
	SSERR_PARAMS = 1,
	SSERR_TARGET_NULL = 3,
	SSERR_TARGET_INACTIVE = 4,
	SSERR_STATE = 5,
	SSERR_NO_AI = 6,
	SSERR_ENTITY_NULL = 8,
	SSERR_ENTITY_TYPE = 9,
};

/*
 * the report in the console (Spanish texts of the original): 1 (None), or 0 when an error is set. differs from the
 * original: after a failed argument parse (the only step of a handler that sets an error) it raises the parse's
 * TypeError; the original returns None with the error left set, which surfaced at a later line.
 */
static byte SetState_Error(const char *entity, const char *state, int err, const char *target) {
	static const char *const texts[] = {
		[1] = "PARAMETROS INCORRECTOS\n",
		[2] = "CAMBIO DE ESTADO EN TODAS LAS ENTIDADES\n",
		[5] = "ERROR CAMBIO DE ESTADO\n",
		[6] = "ENTIDAD SIN IA\n",
		[7] = "ENTIDAD INACTIVA\n",
		[8] = "ENTIDAD NULL\n",
		[9] = "ASIGNACION A ENTIDAD CON TIPO INCORRECTO\n",
	};

	Con_Bluef("\nERROR: SetState(%s) - Entidad: %s\n", state, entity);
	if (err == SSERR_TARGET_NULL) {
		Con_Bluef("OBJETIVO: %s - NULL\n", target);
	} else if (err == SSERR_TARGET_INACTIVE) {
		Con_Bluef("OBJETIVO: %s - INACTIVO\n", target);
	} else if (err >= 1 && err <= 9 && texts[err]) {
		Con_Bluef("%s", texts[err]);
	}
	Con_Bluef("\n**************************************************************************************************"
		"\n\n");
	return !Native_Failed();
}

static int CheckAIEntity(cEntity *e, const char *type) {
	if (!e) {
		return SSERR_ENTITY_NULL;
	}
	if (!Entity_IsType(e, type)) {
		return SSERR_ENTITY_TYPE;
	}
	if (e->active && !Entity_GetAI(e)) {
		return SSERR_NO_AI;
	}
	return 0;
}

static int SetState_CheckChar(cEntity *e) {
	return CheckAIEntity(e, TYEN_WALKCHAR);
}

static int SetState_CheckVehicle(cEntity *e) {
	return CheckAIEntity(e, TYEN_CAR);
}

static int SetState_CheckTarget(cEntity *e) {
	if (!e) {
		return SSERR_TARGET_NULL;
	}
	return e->active ? 0 : SSERR_TARGET_INACTIVE;
}

/* the AI of the entity after the check, NULL with *err set */
static void *StateAI(const char *name, int (*check)(cEntity *), int *err) {
	cEntity *e = World_FindEntity(name);
	*err = check(e);
	if (*err) {
		return NULL;
	}
	void *ai = Entity_GetAI(e);
	AI_Debugf(ai, "CAMBIO de ESTADO desde PYTHON");
	return ai;
}

static byte StateResult(byte ok, const char *name, const char *state, nativeVal_t *out) {
	if (ok) {
		return Api_Build(out, "i", 1);
	}
	return SetState_Error(name, state, SSERR_STATE, NULL);
}

/* state(int), entity: vehicle states without arguments */
static byte VehicleStateSimple(const nativeVal_t *a, int n, nativeVal_t *out, const char *state) {
	int st, err = SSERR_PARAMS;
	const char *name = NULL;

	if (Mode_Parse(a, n, "is", &st, &name)) {
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st), name, state, out);
		}
	}
	return SetState_Error(name, state, err, NULL);
}

/* state(int), entity, target: the target is checked first; NULL with the result of the report in *res */
static cEntity *StateTarget(const char *name, const char *target, const char *state, byte *res) {
	cEntity *t = World_FindEntity(target);
	int err = SetState_CheckTarget(t);
	if (err) {
		*res = SetState_Error(name, state, err, target);
		return NULL;
	}
	return t;
}

/* SAI_VEHICLE_FREEZE: (state, vehicle) */
static byte SetStateVehicle0(const nativeVal_t *a, int n, nativeVal_t *out) {
	return VehicleStateSimple(a, n, out, "FREEZE");
}

/* SAI_VEHICLE_GO_TO: (state, vehicle, x, y, z, f[, flag, f, f, f, flag]); flags count as on only when 1 */
static byte SetStateVehicle1(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err = SSERR_PARAMS, i5 = 0, i9 = 0;
	const char *name = NULL;
	vec3_t pos;
	float f4, f6 = 1.0f, f7 = 20.0f, f8 = 20.0f;

	if (Mode_Parse(a, n, "isffff|ifffi", &st, &name, &pos.x, &pos.y, &pos.z, &f4, &i5, &f6, &f7, &f8, &i9)) {
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, pos, (double)f4, i5 == 1, (double)f6, (double)f7,
				(double)f8, i9 == 1), name, "GO_TO", out);
		}
	}
	return SetState_Error(name, "GO_TO", err, NULL);
}

/* (state, vehicle, target) */
static byte VehicleStateTarget(const nativeVal_t *a, int n, nativeVal_t *out, const char *state) {
	int st, err = SSERR_PARAMS;
	const char *name = NULL, *target;

	if (Mode_Parse(a, n, "iss", &st, &name, &target)) {
		byte res;
		cEntity *t = StateTarget(name, target, state, &res);
		if (!t) {
			return res;
		}
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, t), name, state, out);
		}
	}
	return SetState_Error(name, state, err, NULL);
}

/* SAI_VEHICLE_FIGHTING: (state, vehicle, target) */
static byte SetStateVehicle2(const nativeVal_t *a, int n, nativeVal_t *out) {
	return VehicleStateTarget(a, n, out, "FIGHTING");
}

/* SAI_VEHICLE_IN_ROUTE: (state, vehicle[, flag, f, f]) */
static byte SetStateVehicle3(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err = SSERR_PARAMS, flag = 1;
	const char *name = NULL;
	float f1 = 5.0f, f2 = 2.0f;

	if (Mode_Parse(a, n, "is|iff", &st, &name, &flag, &f1, &f2)) {
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, flag == 1, (double)f1, (double)f2), name, "IN_ROUTE",
				out);
		}
	}
	return SetState_Error(name, "IN_ROUTE", err, NULL);
}

/* SAI_VEHICLE_TRAFFIC: (state, vehicle) */
static byte SetStateVehicle4(const nativeVal_t *a, int n, nativeVal_t *out) {
	return VehicleStateSimple(a, n, out, "TRAFFIC");
}

/* SAI_VEHICLE_GET_AWAY_STOP: (state, vehicle, target, f); every error but the target one reports PARAMETROS */
static byte SetStateVehicle5(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err;
	const char *name = NULL, *target;
	float f;

	if (Mode_Parse(a, n, "issf", &st, &name, &target, &f)) {
		byte res;
		cEntity *t = StateTarget(name, target, "GET_AWAY_STOP", &res);
		if (!t) {
			return res;
		}
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai && cAIVehicle_SetState(ai, st, t, (double)f)) {
			return Api_Build(out, "i", 1);
		}
	}
	return SetState_Error(name, "GET_AWAY_STOP", SSERR_PARAMS, NULL);
}

/* SAI_VEHICLE_RACE: (state, vehicle, x, y, z, f[, flag, flag]) */
static byte SetStateVehicle6(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err = SSERR_PARAMS, i1 = 1, i2 = 0;
	const char *name = NULL;
	vec3_t pos;
	float f;

	if (Mode_Parse(a, n, "isffff|ii", &st, &name, &pos.x, &pos.y, &pos.z, &f, &i1, &i2)) {
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, pos, (double)f, i1 == 1, i2 == 1), name, "RACE", out);
		}
	}
	return SetState_Error(name, "RACE", err, NULL);
}

/* SAI_VEHICLE_CHASE: (state, vehicle, target[, f, f]), defaults from the engine */
static byte SetStateVehicle7(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err = SSERR_PARAMS;
	const char *name = NULL, *target;
	float f1 = g_AIChaseDefault1, f2 = g_AIChaseDefault2;

	if (Mode_Parse(a, n, "iss|ff", &st, &name, &target, &f1, &f2)) {
		byte res;
		cEntity *t = StateTarget(name, target, "CHASE", &res);
		if (!t) {
			return res;
		}
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, t, (double)f1, (double)f2), name, "CHASE", out);
		}
	}
	return SetState_Error(name, "CHASE", err, NULL);
}

/* SAI_VEHICLE_FIGHTING_FREEZE: (state, vehicle, target) */
static byte SetStateVehicle8(const nativeVal_t *a, int n, nativeVal_t *out) {
	return VehicleStateTarget(a, n, out, "FIGHTING_FREEZE");
}

/* SAI_VEHICLE_TRAFFIC_INDOOR: (state, vehicle) */
static byte SetStateVehicle9(const nativeVal_t *a, int n, nativeVal_t *out) {
	return VehicleStateSimple(a, n, out, "TRAFFIC_INDOOR");
}

/* SAI_CHAR_STATIC: (state, char) */
static byte SetStateChar0(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err = SSERR_PARAMS;
	const char *name = NULL;

	if (Mode_Parse(a, n, "is", &st, &name)) {
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(AIChar_SetState(ai, st), name, "STATIC_CHAR", out);
		}
	}
	return SetState_Error(name, "STATIC_CHAR", err, NULL);
}

/* SAI_CHAR_IN_ROUTE: (state, char, f[, flag]) */
static byte SetStateChar1(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err = SSERR_PARAMS, flag = 1;
	const char *name = NULL;
	float f;

	if (Mode_Parse(a, n, "isf|i", &st, &name, &f, &flag)) {
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(AIChar_SetState(ai, st, (double)f, flag == 1), name, "IN_ROUTE_CHAR", out);
		}
	}
	return SetState_Error(name, "IN_ROUTE_CHAR", err, NULL);
}

/* (state, char, target, f) */
static byte CharStateTargetF(const nativeVal_t *a, int n, nativeVal_t *out, const char *state) {
	int st, err = SSERR_PARAMS;
	const char *name = NULL, *target;
	float f;

	if (Mode_Parse(a, n, "issf", &st, &name, &target, &f)) {
		byte res;
		cEntity *t = StateTarget(name, target, state, &res);
		if (!t) {
			return res;
		}
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(AIChar_SetState(ai, st, t, (double)f), name, state, out);
		}
	}
	return SetState_Error(name, state, err, NULL);
}

/* (state, char, target, flag) */
static byte CharStateTargetI(const nativeVal_t *a, int n, nativeVal_t *out, const char *state) {
	int st, err = SSERR_PARAMS, flag;
	const char *name = NULL, *target;

	if (Mode_Parse(a, n, "issi", &st, &name, &target, &flag)) {
		byte res;
		cEntity *t = StateTarget(name, target, state, &res);
		if (!t) {
			return res;
		}
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(AIChar_SetState(ai, st, t, flag == 1), name, state, out);
		}
	}
	return SetState_Error(name, state, err, NULL);
}

/* SAI_CHAR_CHASE_WITH_ACTION: (state, char, target, f) */
static byte SetStateChar2(const nativeVal_t *a, int n, nativeVal_t *out) {
	return CharStateTargetF(a, n, out, "CHASE_WITH_ACTION_CHAR");
}

/* SAI_CHAR_CHASE_WITHOUT_ACTION: (state, char, target, f) */
static byte SetStateChar3(const nativeVal_t *a, int n, nativeVal_t *out) {
	return CharStateTargetF(a, n, out, "CHASE_WITHOUT_ACTION_CHAR");
}

/* SAI_CHAR_GO_TO: (state, char, x, y, z, x, y, z, f, f) */
static byte SetStateChar4(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err = SSERR_PARAMS;
	const char *name = NULL;
	vec3_t p1, p2;
	float f1, f2;

	if (Mode_Parse(a, n, "isffffffff", &st, &name, &p1.x, &p1.y, &p1.z, &p2.x, &p2.y, &p2.z, &f1, &f2)) {
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(AIChar_SetState(ai, st, p1, p2, (double)f1, (double)f2), name, "GO_TO_CHAR", out);
		}
	}
	return SetState_Error(name, "GO_TO_CHAR", err, NULL);
}

/* SAI_CHAR_GET_AWAY: (state, char, target, f) */
static byte SetStateChar5(const nativeVal_t *a, int n, nativeVal_t *out) {
	return CharStateTargetF(a, n, out, "GET_AWAY_CHAR");
}

/* SAI_CHAR_PATROL_ZONE: (state, char, x, y, z, f, f[, flag]) */
static byte SetStateChar6(const nativeVal_t *a, int n, nativeVal_t *out) {
	int st, err = SSERR_PARAMS, flag = 1;
	const char *name = NULL;
	vec3_t pos;
	float f1, f2;

	if (Mode_Parse(a, n, "isfffff|i", &st, &name, &pos.x, &pos.y, &pos.z, &f1, &f2, &flag)) {
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(AIChar_SetState(ai, st, pos, (double)f1, (double)f2, flag == 1), name,
				"PATROL_ZONE_CHAR", out);
		}
	}
	return SetState_Error(name, "PATROL_ZONE_CHAR", err, NULL);
}

/* SAI_CHAR_CHASE_WITH_ACTION_BACK: (state, char, target, f) */
static byte SetStateChar7(const nativeVal_t *a, int n, nativeVal_t *out) {
	return CharStateTargetF(a, n, out, "CHASE_WITH_ACTION_BACK_CHAR");
}

/* SAI_CHAR_ORIENT_DINAMIC_OBJECTIVE: (state, char, target, flag) */
static byte SetStateChar8(const nativeVal_t *a, int n, nativeVal_t *out) {
	return CharStateTargetI(a, n, out, "ORIENT_DINAMIC_OBJECTIVE_CHAR");
}

/* SAI_CHAR_ORIENT_DINAMIC_OBJECTIVE_WITH_ACTION: (state, char, target, flag) */
static byte SetStateChar9(const nativeVal_t *a, int n, nativeVal_t *out) {
	return CharStateTargetI(a, n, out, "ORIENT_DINAMIC_OBJECTIVE_WITH_ACTION_CHAR");
}

/*
 * SetStateVehicle(state, ...) / SetStateChar(state, ...) : the handler for the state number. The first argument is
 * read the way PyInt_AsLong(PyTuple_GetItem(args, 0)) read it: none gives the TypeError of a NULL item (that of
 * None), and only the low 32 bits of the number count. differs from the original: an empty tuple or a state that is no number
 * raises the error of the read; the original returns None with the error left set.
 */
static const portEntry_t vehicleStates[10] = {
	SetStateVehicle0, SetStateVehicle1, SetStateVehicle2, SetStateVehicle3, SetStateVehicle4,
	SetStateVehicle5, SetStateVehicle6, SetStateVehicle7, SetStateVehicle8, SetStateVehicle9,
};
static const portEntry_t charStates[10] = {
	SetStateChar0, SetStateChar1, SetStateChar2, SetStateChar3, SetStateChar4,
	SetStateChar5, SetStateChar6, SetStateChar7, SetStateChar8, SetStateChar9,
};

static byte SetState(const nativeVal_t *a, int n, nativeVal_t *out, const portEntry_t *handlers) {
	nativeVal_t none;
	int st;
	Native_SetNone(&none);
	if (!Mode_ArgInt(n ? &a[0] : &none, &st)) {
		return 0;
	}
	uint32_t state = (uint32_t)st;
	return state > 9 || handlers[state](a, n, out);
}

API_FN(SAI, SetStateVehicle) {
	return SetState(a, n, out, vehicleStates);
}

API_FN(SAI, SetStateChar) {
	return SetState(a, n, out, charStates);
}

/*
 * GetStateVehicle(name) : state number of the vehicle AI, None without AI. differs from the original: a parse error
 * raises its TypeError (the original returns None with the error left set, which surfaced at a later line).
 */
API_FN(SAI, GetStateVehicle) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	cEntity *e = World_FindEntity(name);
	cAIVehicle *ai = e ? Entity_GetAI(e) : NULL;
	return !ai || Api_Build(out, "i", cAIVehicle_GetState(ai));
}

/*
 * GetStateChar(name) : state number of the character AI, None without AI. differs from the original: a parse error
 * raises its TypeError (the original returns None with the error left set, which surfaced at a later line).
 */
API_FN(SAI, GetStateChar) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	cEntity *e = World_FindEntity(name);
	cAIChar *ai = e ? Entity_GetAI(e) : NULL;
	return !ai || Api_Build(out, "i", AIChar_GetState(ai));
}

/*
 * EnableAIVehicle(name, enable, ...) : 25 more values in three parameter blocks. As compiled the format has 27
 * conversions for 26 pointers, so a 27th argument writes through an undefined pointer; here it goes into a dummy.
 * Failure returns NULL without a Python exception. Up to 27 arguments, more than the fast path takes (PORT_ARGS): a
 * call of 12 or more goes through the adapter (Mode_SelfEntry passes them all)
 */
API_FN(SAI, EnableAIVehicle) {
	const char *name;
	int enable, extra;
	cAIVehicleParams1 p1 = { .boostItemLife = 1, .boostItemAmmo = 1, .itemAmmo = 0.25f, .hookRace = 0, .hookMap = 0,
		.strafeEvasion = 0, .percHook = 0, .percHookGetAway = 0 };
	cAIVehicleParams2 p2;
	cAIVehicleParams3 p3 = { .cadenceCM = 3, .counterattack = 1.0f, .percCloud = 35, .percSonic = 50,
		.percElectro = 15 };

	if (!Mode_Parse(a, n, "sifiiiiifiiff|iiffiiiiiiiiii", &name, &enable, &p1.damageGetAway, &p2.strafe,
			&p2.brake, &p1.boostGetAway, &p2.stupidPathfinding, &p2.maxTimeStrafe, &p2.maxVelStrafe, &p2.aim,
			&p3.cadence, &p3.angAttack, &p1.damageItemLife, &p1.boostItemLife, &p1.boostItemAmmo, &p1.itemAmmo,
			&p3.counterattack, &p1.hookRace, &p1.hookMap, &p1.strafeEvasion, &p3.cadenceCM, &p3.percSonic,
			&p3.percCloud, &p3.percElectro, &p1.percHook, &p1.percHookGetAway, &extra)) {
		return 0;
	}
	cEntity *e = FindAIEntity(name, TYEN_CAR);
	if (!e || cVehicleEntity_GetOwnerSlot(e) >= 0) {
		return Native_NullResult();
	}
	byte on = enable != 0;
	cAIVehicle *ai = Entity_GetAI(e);
	if (on && ai) {
		cAIVehicle_Setup(ai, &p1, &p2, &p3);
		*cAIVehicle_GetParams1(ai) = p1;
		*cAIVehicle_GetParams2(ai) = p2;
		*cAIVehicle_GetParams3(ai) = p3;
	}
	else {
		cVehicleEntity_EnableAI(Veh(e), on, &p1, &p2, &p3);
	}
	Native_SetInt(out, 1);
	return 1;
}

/* EnableAIChar(name, enable, arg) : the name must match the entity exactly (the table lookup ignores case) */
API_FN(SAI, EnableAIChar) {
	const char *name;
	int enable, arg;
	if (!Mode_Parse(a, n, "sii", &name, &enable, &arg)) {
		return 0;
	}
	cEntity *e = FindAIEntity(name, TYEN_WALKCHAR);
	if (!e || cWalkCharEntity_GetOwnerSlot(e) >= 0 || strcmp(name, e->name)) {
		return 0;
	}
	cWalkCharEntity_EnableAI(e, enable != 0, &arg);
	return 1;
}

/*
 * GetNextRacePoint(initialPoint, minDist, maxDist) : next race point; maxDist grows by 1000 until one is found
 * (outdoors only one try). Past 1e8 the CHECK fails. differs from the original: a parse error raises its TypeError;
 * the original returns None with the error left set, and OutMap's spawn searches took the None as no point and went
 * on to their fallback position (the end of their for loop cleared the error).
 */
byte AI_NextRacePoint(const vec3_t *initial, float minDist, float maxDist, vec3_t *out) {
	int outdoor = g_IsMapOutDoor == 1;
	byte found;
	for (;;) {
		found = CNodesWorld_GetRacePoint(g_NodesWorld, out, initial, minDist, maxDist, 0);
		maxDist += 1000.0f;
		if (found || outdoor || !(1e8f >= maxDist)) {
			break;
		}
	}
	CHECK(maxDist <= 1e8f && "Se ha superado la distancia máxima sin haber encontrado un punto");
	return !outdoor || found;
}

API_FN(SAI, GetNextRacePoint) {
	vec3_t initial, p;
	float minDist, maxDist;
	if (!Mode_Parse(a, n, "(fff)ff", &initial.x, &initial.y, &initial.z, &minDist, &maxDist)) {
		return 0;
	}
	return !AI_NextRacePoint(&initial, minDist, maxDist, &p) ||
		Api_Build(out, "fff", (double)p.x, (double)p.y, (double)p.z);
}

/*
 * GetNearestItemLife(name) : name of the nearest life item for the AI of the entity, else None. differs from the
 * original: a parse error raises its TypeError; the original returns None with the error left set, and its caller
 * (Mission_StupidMerc) hands the None to Scrap.GetEntity, whose parse raised that error on the next line.
 */
API_FN(SAI, GetNearestItemLife) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	cEntity *e = World_FindEntity(name);
	cAIVehicle *ai = e ? Entity_GetAI(e) : NULL;
	cEntity *item = ai ? cAIVehicle_GetNearestItemLife(ai) : NULL;
	return !item || Api_Build(out, "s", item->name);
}

/* GetRandomVisibilityPoint() : a random point of the visibility graph (CRT rand), None if there is none */
API_FN(SAI, GetRandomVisibilityPoint) {
	int count = NodesWorld_NumVisPoints(g_NodesWorld);
	if (count <= 0) {
		return 1;
	}
	vec3_t p;
	memcpy(&p, NodesWorld_GetVisPoint(g_NodesWorld, crt_rand() % count), sizeof(p));
	return Api_Build(out, "(fff)", (double)p.x, (double)p.y, (double)p.z);
}

/*
 * GetReposCharPos(pos, name[, targetName]) : free position for a character, None if it fails. differs from the
 * original: a parse error raises its TypeError (the original returns None with the error left set, which surfaced at
 * a later line).
 */
API_FN(SAI, GetReposCharPos) {
	vec3_t pos, p;
	const char *name, *targetName = NULL;
	if (!Mode_Parse(a, n, "(fff)s|s", &pos.x, &pos.y, &pos.z, &name, &targetName)) {
		return 0;
	}
	cEntity *target = NULL;
	if (targetName && *targetName) {
		target = World_FindEntity(targetName);
		if (target && !Entity_IsType(target, TYEN_WALKCHAR)) {
			target = NULL;
		}
	}
	return !cAICharGroup_GetReposPos(g_CharGroup, &pos, name, &p, target) ||
		Api_Build(out, "fff", (double)p.x, (double)p.y, (double)p.z);
}

/* InitVehicleRace() : No argument check. */
API_FN(SAI, InitVehicleRace) {
	AIGroup_Clear(g_VehicleGroup);
	return 1;
}

/*
 * AddVehicleRace(name) : adds a vehicle to the race group. differs from the original: a parse error raises its
 * TypeError (the original returns None with the error left set, which surfaced at a later line).
 */
API_FN(SAI, AddVehicleRace) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	cEntity *e = FindAIEntity(name, TYEN_CAR);
	if (e) {
		AIGroup_Add(g_VehicleGroup, e);
	}
	return 1;
}

/*
 * SetRotStaticObj(name, a, b). differs from the original: a parse error raises its TypeError (the original returns
 * None with the error left set, which surfaced at a later line).
 */
API_FN(SAI, SetRotStaticObj) {
	const char *name;
	float f1, f2;
	if (!Mode_Parse(a, n, "sff", &name, &f1, &f2)) {
		return 0;
	}
	cEntity *e = FindAIEntity(name, TYEN_CAR);
	cAIVehicle *ai = e ? Entity_GetAI(e) : NULL;
	if (ai) {
		cAIVehicle_SetRotStatic(ai, f1, f2);
	}
	return 1;
}

/*
 * SetInertia(name, on) : on only for the value 1. differs from the original: a parse error raises its TypeError (the
 * original returns None with the error left set, which surfaced at a later line).
 */
API_FN(SAI, SetInertia) {
	const char *name;
	int on;
	if (!Mode_Parse(a, n, "si", &name, &on)) {
		return 0;
	}
	cEntity *e = FindAIEntity(name, TYEN_CAR);
	cAIVehicle *ai = e ? Entity_GetAI(e) : NULL;
	if (ai) {
		cAIVehicle_SetInertia(ai, on == 1);
	}
	return 1;
}

void *g_AppPyOne;

/*
 * GetOD() : the object (the 1 of the level, a folded stub in the original). differs from the original: the result is
 * a new value; the original returned the object without a new reference, so each call took one from it.
 */
API_FN(SAI, GetOD) {
	if (!g_AppPyOne) {
		return 0;
	}
	Native_Arg((PyObject *)g_AppPyOne, out);
	return 1;
}

static char str_PyDoc_SAI_AddVehicleRace[];
static char str_PyDoc_SAI_AnalyzeGraph[];
static char str_PyDoc_SAI_AnalyzeGraph2D[];
static char str_PyDoc_SAI_AnalyzeMap[];
static char str_PyDoc_SAI_AnalyzeTraffic[56];
static char str_PyDoc_SAI_BuildGraph[];
static char str_PyDoc_SAI_BuildGraph2D[];
static char str_PyDoc_SAI_EnableAIChar[];
static char str_PyDoc_SAI_EnableAIVehicle[];
static char str_PyDoc_SAI_GetNearestItemLife[104];
static char str_PyDoc_SAI_GetNextRacePoint[256];
static char str_PyDoc_SAI_GetRandomVisibilityPoint[];
static char str_PyDoc_SAI_GetReposCharPos[];
static char str_PyDoc_SAI_GetStateChar[];
static char str_PyDoc_SAI_GetStateVehicle[];
static char str_PyDoc_SAI_IniAI[];
static char str_PyDoc_SAI_InitVehicleRace[60];
static char str_PyDoc_SAI_SetInertia[72];
static char str_PyDoc_SAI_SetRotStaticObj[];
static char str_PyDoc_SAI_SetStateChar[];
static char str_PyDoc_SAI_SetStateVehicle[872];
static const portFn_t SAI_methods[] = {
	PORT_FND(AnalizeMap, SAI_AnalyzeMap, str_PyDoc_SAI_AnalyzeMap),
	PORT_FND(AnalizeGraph, SAI_AnalyzeGraph, str_PyDoc_SAI_AnalyzeGraph),
	PORT_FND(AnalizeGraph2D, SAI_AnalyzeGraph2D, str_PyDoc_SAI_AnalyzeGraph2D),
	PORT_FND(AnalizeTraffic, SAI_AnalyzeTraffic, str_PyDoc_SAI_AnalyzeTraffic),
	PORT_FND(BuildGraph, SAI_BuildGraph, str_PyDoc_SAI_BuildGraph),
	PORT_FND(BuildGraph2D, SAI_BuildGraph2D, str_PyDoc_SAI_BuildGraph2D),
	PORT_FND(SetStateVehicle, SAI_SetStateVehicle, str_PyDoc_SAI_SetStateVehicle),
	PORT_FND(SetStateChar, SAI_SetStateChar, str_PyDoc_SAI_SetStateChar),
	PORT_FND(GetStateVehicle, SAI_GetStateVehicle, str_PyDoc_SAI_GetStateVehicle),
	PORT_FND(GetStateChar, SAI_GetStateChar, str_PyDoc_SAI_GetStateChar),
	PORT_FND(EnableAIVehicle, SAI_EnableAIVehicle, str_PyDoc_SAI_EnableAIVehicle),
	PORT_FND(EnableAIChar, SAI_EnableAIChar, str_PyDoc_SAI_EnableAIChar),
	PORT_FND(IniAI, SAI_IniAI, str_PyDoc_SAI_IniAI),
	PORT_FND(GetNextRacePoint, SAI_GetNextRacePoint, str_PyDoc_SAI_GetNextRacePoint),
	PORT_FND(GetNearestItemLife, SAI_GetNearestItemLife, str_PyDoc_SAI_GetNearestItemLife),
	PORT_FND(GetRandomVisibilityPoint, SAI_GetRandomVisibilityPoint, str_PyDoc_SAI_GetRandomVisibilityPoint),
	PORT_FND(GetReposCharPos, SAI_GetReposCharPos, str_PyDoc_SAI_GetReposCharPos),
	PORT_FND(InitVehicleRace, SAI_InitVehicleRace, str_PyDoc_SAI_InitVehicleRace),
	PORT_FND(AddVehicleRace, SAI_AddVehicleRace, str_PyDoc_SAI_AddVehicleRace),
	PORT_FND(SetRotStaticObj, SAI_SetRotStaticObj, str_PyDoc_SAI_SetRotStaticObj),
	PORT_FND(SetInertia, SAI_SetInertia, str_PyDoc_SAI_SetInertia),
	PORT_FN(GetOD, SAI_GetOD),
	PORT_FNEND
};

SD_PY_MODULE(SAI, 0x42a488)

static char str_PyDoc_SAI_IniAI[] = "IniAI(levelPath) : Inicializa AI para un nivel.";

static char str_PyDoc_SAI_BuildGraph[] =
	"BuildGraph(int numNodesRadius, float sizeNodeX, float sizeNodeY, float sizeNodeZ) : Crea el Grafo as"
	"ociado al Pathfinding";


static char str_PyDoc_SAI_AnalyzeMap[] = "AnalizeMap(float sizeNode) : Analiza características mapa.";


static char str_PyDoc_SAI_AnalyzeGraph[] = "AnalizeGraph() : Analiza características grafo.";


static char str_PyDoc_SAI_AnalyzeGraph2D[] = "AnalizeGraph2D() : Analiza características grafo 2D.";


static char str_PyDoc_SAI_AnalyzeTraffic[56] = "AnalizeTraffic() : Analiza características tráfico.";


static char str_PyDoc_SAI_BuildGraph2D[] =
	"BuildGraph2D(int numNodesRadius, float sizeNodeX, float sizeNodeY, float sizeNodeZ) : Crea el Grafo "
	"asociado al Pathfinding 2D";


static char str_PyDoc_SAI_SetStateVehicle[872] =
	"SetStateVehicle(0, string nameAgent) : Estado sin movimiento ni disparo.\n"
	" SetStateVehicle(1, float posObjX, float posObjY, float posObjZ, radiusObj, string nameAgent) : Esta"
	"do alcanzar posición.\n"
	" SetStateVehicle(2, string nameObjectiveDin, string nameAgent) : Estado persecución enemigo.\n"
	" SetStateVehicle(3, string nameAgent) : Estado en ruta.\n"
	" SetStateVehicle(4, string nameAgent) : Estado tráfico.\n"
	" SetStateVehicle(5, string nameAgent, float distStop) : Estado huída con parada.\n"
	" SetStateVehicle(6, float posObjX, float posObjY, float posObjZ, radiusObj, string nameAgent) : Esta"
	"do alcanzar meta en carrera.\n"
	" SetStateVehicle(7, string nameObjectiveDin, string nameAgent) : Estado persecución.\n"
	" SetStateVehicle(8, string nameObjectiveDin, string nameAgent) : Estado lucha manteniendo posición."
	"\n SetStateVehicle(9, string nameAgent) : Estado tráfico en interiores.\n";


static char str_PyDoc_SAI_GetStateVehicle[] =
	"GetStateVehicle(string nameEntity) : Devuelve el estado de la IA del vehículo.\n"
	" 0 : Estado sin movimiento ni disparo.\n 1 : Estado alcanzar posición.\n"
	" 2 : Estado persecución enemigo.\n 3 : Estado en ruta.\n 4 : Estado tráfico.\n"
	" 5 : Estado huída con parada.\n 6 : Estado alcanzar meta en carrera.\n 7 : Estado persecución.\n"
	" 8 : Estado persecución enemigo con uso de hook.\n"
	" 9 : Estado sin movimiento ni disparo por estar objetivo en posición inválida.\n";

static char str_PyDoc_SAI_GetStateChar[] =
	"GetStateChar(string nameEntity) : Devuelve el estado de la IA del personaje.\n";

static char str_PyDoc_SAI_EnableAIVehicle[] =
	"EnableAIVehicle(string nameEntity, int enable, int controlStrafe, int controlBrake, int stupidPathfi"
	"nding) : Habilita la IA del vehículo indicando características asociadas al movimiento).";

static char str_PyDoc_SAI_SetStateChar[] =
	"SetStateChar(0, string nameAgent) : Estado sin movimiento ni disparo.\n"
	" SetStateChar(1, string nameAgent, float vel, int withStopTemp) : Estado en ruta.\n"
	" SetStateChar(2, string nameAgent, string nameObjective, float vel) : Estado en persecución objetivo"
	" con acción.\n SetStateChar(3, string nameAgent, string nameObjective, float vel) : Estado en persec"
	"ución objetivo sin acción.\n"
	" SetStateChar(4, string nameAgent, float posObjX, float posObjY, float posObjZ, float orientX, float"
	" orientY, float orientZ, float radiusObj, float velObj) : Estado ir a un punto con orientación final"
	".\n SetStateChar(5, string nameAgent, string nameObjective, float vel) : Estado huída de otro person"
	"aje.\n SetStateChar(6, string nameAgent, float centerPatrolZone.x, float centerPatrolZone.y, float c"
	"enterPatrolZone.z, float radiusPatrolZone, float vel, int withStopTemp) : Estado patrulla de zona.\n"
	" SetStateChar(7, string nameAgent, string nameObjective, float vel) : Estado en persecución objetivo"
	" con acción.\n SetStateChar(8, string nameAgent, string nameObjective, int goBack) : Orientación sob"
	"re objetivo dinámico.\n"
	" SetStateChar(9, string nameAgent, string nameObjective, int goBack) : Orientación sobre objetivo di"
	"námico con acción.";

static char str_PyDoc_SAI_EnableAIChar[] =
	"EnableAIChar(string nameEntity, int enable, int stupidPathfinding) : Habilita la IA del personaje in"
	"dicando características asociadas al movimiento).";

static char str_PyDoc_SAI_GetNextRacePoint[256] =
	"(point) GetNextRacePoint(initialPoint, minDist, maxDist) : A partir de un punto inicial 'initialPoin"
	"t', una distancia mínima 'minDist' y una distancia máxima 'maxDist', devuelve un punto aleatorio a p"
	"artir del grafo 3D de la IA en el interior actual.";

static char str_PyDoc_SAI_GetNearestItemLife[104] =
	"(itemName) GetNearestItemLife(vehicleName)) : Devuelve el item de vida más cercano a una nave dada.";

static char str_PyDoc_SAI_GetRandomVisibilityPoint[] =
	"(point) GetRandomVisibilityPoint() : Devuelve un punto aleatorio del grafo de puntos de visibilidad.";

static char str_PyDoc_SAI_GetReposCharPos[] =
	"(x,y,z) GetReposCharPos((x,y,z) ,EntityClass,[,EntityName]) : Obtiene un punto de reposicion de pers"
	"onaje (si entidad, se asigna).\n Retorna (None) si falla";

static char str_PyDoc_SAI_InitVehicleRace[60] = "InitVehicleRace() : Inicializa carrera de vehículos.";

static char str_PyDoc_SAI_AddVehicleRace[] =
	"bool AddVehicleRace(cWithLifeEntity *entidad) : Asigna entidad como perteneciente a carrera.";

static char str_PyDoc_SAI_SetRotStaticObj[] =
	"bool SetRotStaticObj(float maxVelRot, float limIncVelRot) : Asigna rotaciones para movimiento hacia "
	"objetivo estático.";

static char str_PyDoc_SAI_SetInertia[72] = "void SetInertia(bool inertia) : Indica si la nave tiene inercia.";

PORTS(PyInterface_SAIPy)
