/*
 * Python module SAI (method table SAI_methods, rva 0x42a488): AI setup and control of vehicles and characters.
 * Original source: PyInterface/SAIPy.cpp, with the state handlers behind SetStateVehicle/SetStateChar (static
 * functions there).
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/console.h"
#include "scrap/global.h"

static PyObject *ReturnNone(void) {
	Py_INCREF(Py_None);
	return Py_None;
}

/* new T, NULL if the memory is not there (the original wraps the constructor in an EH frame for the delete) */
#define NEW_OBJ(size, ctor, ...) ({ void *mem_ = operator_new(size); mem_ ? ctor(mem_, ##__VA_ARGS__) : NULL; })

/* an entity that is a Car or WalkChar with flag_4c set, else NULL */
static cEntity *FindAIEntity(const char *name, const char *type) {
	cEntity *e = World_FindEntity(name);
	if (!e || !e->flag_4c || !Entity_IsType(e, type)) {
		return NULL;
	}
	return e;
}

/* IniAI(levelname, IsMapOutDoor) : creates the navigation graphs, traffic, A* and AI groups of the level */
PyObject *SAI_IniAI(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "si", &name, &g_IsMapOutDoor)) {
		return NULL;
	}
	g_NodesWorld = NEW_OBJ(0x3f0, CNodesWorld_ctor);
	CHECK(g_NodesWorld);
	vec3_t zero = { 0.0f, 0.0f, 0.0f };
	CNodesWorld_Load(g_NodesWorld, -1, name, &zero);
	cNodesTraffic *traffic;
	if (!g_IsMapOutDoor) {
		g_NodesWorld2D = NEW_OBJ(0x378, CNodesWorld2D_ctor);
		CHECK(g_NodesWorld2D);
		CNodesWorld2D_Load(g_NodesWorld2D, -1, name, &zero);
	}
	if (!g_IsMapOutDoor) { /* read again: loading the 2D graph may change it */
		g_NodesTrafficInDoor = NEW_OBJ(0xfa4, CNodesTrafficInDoor_ctor);
		CHECK(g_NodesTrafficInDoor);
		traffic = g_NodesTrafficInDoor;
	}
	else {
		g_NodesTraffic = NEW_OBJ(0x70, CNodesTraffic_ctor);
		CHECK(g_NodesTraffic);
		traffic = g_NodesTraffic;
	}
	traffic->vftable->Init(traffic);
	g_AStar = NEW_OBJ(0x48, CAStar_ctor, g_NodesWorld);
	CHECK(g_AStar);
	if (!g_IsMapOutDoor) {
		g_AStar2D = NEW_OBJ(0x50, CAStar2D_ctor, g_NodesWorld2D);
		CHECK(g_AStar2D);
	}
	g_VehicleGroup = NEW_OBJ(0xf4, cAIVehicleGroup_ctor);
	CHECK(g_VehicleGroup);
	if (!g_IsMapOutDoor) {
		g_CharGroup = NEW_OBJ(0x68, cAICharGroup_ctor);
		CHECK(g_CharGroup);
	}
	return ReturnNone();
}

/* BuildGraph(arg, x, y, z) */
PyObject *SAI_BuildGraph(PyObject *self, PyObject *args) {
	int arg;
	float x, y, z;

	(void)self;
	if (!PyArg_ParseTuple(args, "ifff", &arg, &x, &y, &z)) {
		return NULL;
	}
	vec3_t pos = { x, y, z };
	CNodesWorld_Load(g_NodesWorld, arg, "", &pos);
	return ReturnNone();
}

/* AnalizeMap(size) : analyses the level with a cube of that size; makes the graph if there is none */
PyObject *SAI_AnalizeMap(PyObject *self, PyObject *args) {
	float size;

	(void)self;
	if (!PyArg_ParseTuple(args, "f", &size)) {
		return NULL;
	}
	vec3_t cube = { size, size, size };
	if (!g_NodesWorld) {
		g_NodesWorld = NEW_OBJ(0x3f0, CNodesWorld_ctor);
		CHECK(g_NodesWorld);
	}
	CNodesWorld_Analize(g_NodesWorld, &cube);
	return ReturnNone();
}

/* AnalizeGraph() : Analiza características grafo. No argument check. */
PyObject *SAI_AnalizeGraph(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	CNodesWorld_AnalizeGraph(g_NodesWorld);
	return ReturnNone();
}

/* AnalizeGraph2D() : No argument check. */
PyObject *SAI_AnalizeGraph2D(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	CNodesWorld2D_AnalizeGraph(g_NodesWorld2D);
	return ReturnNone();
}

/* AnalizeTraffic() : traffic graph of the level (indoor or outdoor) */
PyObject *SAI_AnalizeTraffic(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	cNodesTraffic *traffic = g_IsMapOutDoor ? g_NodesTraffic : g_NodesTrafficInDoor;
	traffic->vftable->Analize(traffic);
	g_TrafficState = 3;
	return ReturnNone();
}

/* BuildGraph2D(arg, x, y, z) : indoor levels only */
PyObject *SAI_BuildGraph2D(PyObject *self, PyObject *args) {
	int arg;
	float x, y, z;

	(void)self;
	CHECK(!g_IsMapOutDoor);
	if (!PyArg_ParseTuple(args, "ifff", &arg, &x, &y, &z)) {
		return NULL;
	}
	vec3_t pos = { x, y, z };
	CNodesWorld2D_Load(g_NodesWorld2D, arg, "", &pos);
	return ReturnNone();
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

/* the report in the console (Spanish texts of the original), returns None */
PyObject *SetState_Error(const char *entity, const char *state, int err, const char *target) {
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
	return ReturnNone();
}

static int CheckAIEntity(cEntity *e, const char *type) {
	if (!e) {
		return SSERR_ENTITY_NULL;
	}
	if (!Entity_IsType(e, type)) {
		return SSERR_ENTITY_TYPE;
	}
	if (e->flag_4c && !e->vftable->GetAI(e)) {
		return SSERR_NO_AI;
	}
	return 0;
}

int SetState_CheckChar(cEntity *e) {
	return CheckAIEntity(e, TYEN_WALKCHAR);
}

int SetState_CheckVehicle(cEntity *e) {
	return CheckAIEntity(e, TYEN_CAR);
}

int SetState_CheckTarget(cEntity *e) {
	if (!e) {
		return SSERR_TARGET_NULL;
	}
	return e->flag_4c ? 0 : SSERR_TARGET_INACTIVE;
}

/* the AI of the entity after the check, NULL with *err set */
static void *StateAI(const char *name, int (*check)(cEntity *), int *err) {
	cEntity *e = World_FindEntity(name);
	*err = check(e);
	if (*err) {
		return NULL;
	}
	void *ai = e->vftable->GetAI(e);
	AI_Debugf(ai, "CAMBIO de ESTADO desde PYTHON");
	return ai;
}

static PyObject *StateResult(byte ok, const char *name, const char *state) {
	if (ok) {
		return Py_BuildValue("i", 1);
	}
	return SetState_Error(name, state, SSERR_STATE, NULL);
}

/* state(int), entity: vehicle states without arguments */
static PyObject *VehicleStateSimple(PyObject *args, const char *state) {
	int st, err = SSERR_PARAMS;
	char *name = NULL;

	if (PyArg_ParseTuple(args, "is", &st, &name)) {
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st), name, state);
		}
	}
	return SetState_Error(name, state, err, NULL);
}

/* state(int), entity, target: the target is checked first */
static cEntity *StateTarget(const char *name, const char *target, const char *state, PyObject **error) {
	cEntity *t = World_FindEntity(target);
	int err = SetState_CheckTarget(t);
	if (err) {
		*error = SetState_Error(name, state, err, target);
		return NULL;
	}
	return t;
}

/* SAI_VEHICLE_FREEZE: (state, vehicle) */
PyObject *SetStateVehicle0(PyObject *self, PyObject *args) {
	(void)self;
	return VehicleStateSimple(args, "FREEZE");
}

/* SAI_VEHICLE_GO_TO: (state, vehicle, x, y, z, f[, flag, f, f, f, flag]); flags count as on only when 1 */
PyObject *SetStateVehicle1(PyObject *self, PyObject *args) {
	int st, err = SSERR_PARAMS, i5 = 0, i9 = 0;
	char *name = NULL;
	vec3_t pos;
	float f4, f6 = 1.0f, f7 = 20.0f, f8 = 20.0f;

	(void)self;
	if (PyArg_ParseTuple(args, "isffff|ifffi", &st, &name, &pos.x, &pos.y, &pos.z, &f4, &i5, &f6, &f7, &f8, &i9)) {
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, pos, (double)f4, i5 == 1, (double)f6, (double)f7,
				(double)f8, i9 == 1), name, "GO_TO");
		}
	}
	return SetState_Error(name, "GO_TO", err, NULL);
}

/* (state, vehicle, target) */
static PyObject *VehicleStateTarget(PyObject *args, const char *state) {
	int st, err = SSERR_PARAMS;
	char *name = NULL, *target;

	if (PyArg_ParseTuple(args, "iss", &st, &name, &target)) {
		PyObject *error;
		cEntity *t = StateTarget(name, target, state, &error);
		if (!t) {
			return error;
		}
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, t), name, state);
		}
	}
	return SetState_Error(name, state, err, NULL);
}

/* SAI_VEHICLE_FIGHTING: (state, vehicle, target) */
PyObject *SetStateVehicle2(PyObject *self, PyObject *args) {
	(void)self;
	return VehicleStateTarget(args, "FIGHTING");
}

/* SAI_VEHICLE_IN_ROUTE: (state, vehicle[, flag, f, f]) */
PyObject *SetStateVehicle3(PyObject *self, PyObject *args) {
	int st, err = SSERR_PARAMS, flag = 1;
	char *name = NULL;
	float a = 5.0f, b = 2.0f;

	(void)self;
	if (PyArg_ParseTuple(args, "is|iff", &st, &name, &flag, &a, &b)) {
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, flag == 1, (double)a, (double)b), name, "IN_ROUTE");
		}
	}
	return SetState_Error(name, "IN_ROUTE", err, NULL);
}

/* SAI_VEHICLE_TRAFFIC: (state, vehicle) */
PyObject *SetStateVehicle4(PyObject *self, PyObject *args) {
	(void)self;
	return VehicleStateSimple(args, "TRAFFIC");
}

/* SAI_VEHICLE_GET_AWAY_STOP: (state, vehicle, target, f); every error but the target one reports PARAMETROS */
PyObject *SetStateVehicle5(PyObject *self, PyObject *args) {
	int st, err;
	char *name = NULL, *target;
	float f;

	(void)self;
	if (PyArg_ParseTuple(args, "issf", &st, &name, &target, &f)) {
		PyObject *error;
		cEntity *t = StateTarget(name, target, "GET_AWAY_STOP", &error);
		if (!t) {
			return error;
		}
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai && cAIVehicle_SetState(ai, st, t, (double)f)) {
			return Py_BuildValue("i", 1);
		}
	}
	return SetState_Error(name, "GET_AWAY_STOP", SSERR_PARAMS, NULL);
}

/* SAI_VEHICLE_RACE: (state, vehicle, x, y, z, f[, flag, flag]) */
PyObject *SetStateVehicle6(PyObject *self, PyObject *args) {
	int st, err = SSERR_PARAMS, i1 = 1, i2 = 0;
	char *name = NULL;
	vec3_t pos;
	float f;

	(void)self;
	if (PyArg_ParseTuple(args, "isffff|ii", &st, &name, &pos.x, &pos.y, &pos.z, &f, &i1, &i2)) {
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, pos, (double)f, i1 == 1, i2 == 1), name, "RACE");
		}
	}
	return SetState_Error(name, "RACE", err, NULL);
}

/* SAI_VEHICLE_CHASE: (state, vehicle, target[, f, f]), defaults from the engine */
PyObject *SetStateVehicle7(PyObject *self, PyObject *args) {
	int st, err = SSERR_PARAMS;
	char *name = NULL, *target;
	float a = g_AIChaseDefault1, b = g_AIChaseDefault2;

	(void)self;
	if (PyArg_ParseTuple(args, "iss|ff", &st, &name, &target, &a, &b)) {
		PyObject *error;
		cEntity *t = StateTarget(name, target, "CHASE", &error);
		if (!t) {
			return error;
		}
		cAIVehicle *ai = StateAI(name, SetState_CheckVehicle, &err);
		if (ai) {
			return StateResult(cAIVehicle_SetState(ai, st, t, (double)a, (double)b), name, "CHASE");
		}
	}
	return SetState_Error(name, "CHASE", err, NULL);
}

/* SAI_VEHICLE_FIGHTING_FREEZE: (state, vehicle, target) */
PyObject *SetStateVehicle8(PyObject *self, PyObject *args) {
	(void)self;
	return VehicleStateTarget(args, "FIGHTING_FREEZE");
}

/* SAI_VEHICLE_TRAFFIC_INDOOR: (state, vehicle) */
PyObject *SetStateVehicle9(PyObject *self, PyObject *args) {
	(void)self;
	return VehicleStateSimple(args, "TRAFFIC_INDOOR");
}

/* SAI_CHAR_STATIC: (state, char) */
PyObject *SetStateChar0(PyObject *self, PyObject *args) {
	int st, err = SSERR_PARAMS;
	char *name = NULL;

	(void)self;
	if (PyArg_ParseTuple(args, "is", &st, &name)) {
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(ai->vftable->SetState(ai, st), name, "STATIC_CHAR");
		}
	}
	return SetState_Error(name, "STATIC_CHAR", err, NULL);
}

/* SAI_CHAR_IN_ROUTE: (state, char, f[, flag]) */
PyObject *SetStateChar1(PyObject *self, PyObject *args) {
	int st, err = SSERR_PARAMS, flag = 1;
	char *name = NULL;
	float f;

	(void)self;
	if (PyArg_ParseTuple(args, "isf|i", &st, &name, &f, &flag)) {
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(ai->vftable->SetState(ai, st, (double)f, flag == 1), name, "IN_ROUTE_CHAR");
		}
	}
	return SetState_Error(name, "IN_ROUTE_CHAR", err, NULL);
}

/* (state, char, target, f) */
static PyObject *CharStateTargetF(PyObject *args, const char *state) {
	int st, err = SSERR_PARAMS;
	char *name = NULL, *target;
	float f;

	if (PyArg_ParseTuple(args, "issf", &st, &name, &target, &f)) {
		PyObject *error;
		cEntity *t = StateTarget(name, target, state, &error);
		if (!t) {
			return error;
		}
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(ai->vftable->SetState(ai, st, t, (double)f), name, state);
		}
	}
	return SetState_Error(name, state, err, NULL);
}

/* (state, char, target, flag) */
static PyObject *CharStateTargetI(PyObject *args, const char *state) {
	int st, err = SSERR_PARAMS, flag;
	char *name = NULL, *target;

	if (PyArg_ParseTuple(args, "issi", &st, &name, &target, &flag)) {
		PyObject *error;
		cEntity *t = StateTarget(name, target, state, &error);
		if (!t) {
			return error;
		}
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(ai->vftable->SetState(ai, st, t, flag == 1), name, state);
		}
	}
	return SetState_Error(name, state, err, NULL);
}

/* SAI_CHAR_CHASE_WITH_ACTION: (state, char, target, f) */
PyObject *SetStateChar2(PyObject *self, PyObject *args) {
	(void)self;
	return CharStateTargetF(args, "CHASE_WITH_ACTION_CHAR");
}

/* SAI_CHAR_CHASE_WITHOUT_ACTION: (state, char, target, f) */
PyObject *SetStateChar3(PyObject *self, PyObject *args) {
	(void)self;
	return CharStateTargetF(args, "CHASE_WITHOUT_ACTION_CHAR");
}

/* SAI_CHAR_GO_TO: (state, char, x, y, z, x, y, z, f, f) */
PyObject *SetStateChar4(PyObject *self, PyObject *args) {
	int st, err = SSERR_PARAMS;
	char *name = NULL;
	vec3_t a, b;
	float f1, f2;

	(void)self;
	if (PyArg_ParseTuple(args, "isffffffff", &st, &name, &a.x, &a.y, &a.z, &b.x, &b.y, &b.z, &f1, &f2)) {
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(ai->vftable->SetState(ai, st, a, b, (double)f1, (double)f2), name, "GO_TO_CHAR");
		}
	}
	return SetState_Error(name, "GO_TO_CHAR", err, NULL);
}

/* SAI_CHAR_GET_AWAY: (state, char, target, f) */
PyObject *SetStateChar5(PyObject *self, PyObject *args) {
	(void)self;
	return CharStateTargetF(args, "GET_AWAY_CHAR");
}

/* SAI_CHAR_PATROL_ZONE: (state, char, x, y, z, f, f[, flag]) */
PyObject *SetStateChar6(PyObject *self, PyObject *args) {
	int st, err = SSERR_PARAMS, flag = 1;
	char *name = NULL;
	vec3_t pos;
	float f1, f2;

	(void)self;
	if (PyArg_ParseTuple(args, "isfffff|i", &st, &name, &pos.x, &pos.y, &pos.z, &f1, &f2, &flag)) {
		cAIChar *ai = StateAI(name, SetState_CheckChar, &err);
		if (ai) {
			return StateResult(ai->vftable->SetState(ai, st, pos, (double)f1, (double)f2, flag == 1), name,
				"PATROL_ZONE_CHAR");
		}
	}
	return SetState_Error(name, "PATROL_ZONE_CHAR", err, NULL);
}

/* SAI_CHAR_CHASE_WITH_ACTION_BACK: (state, char, target, f) */
PyObject *SetStateChar7(PyObject *self, PyObject *args) {
	(void)self;
	return CharStateTargetF(args, "CHASE_WITH_ACTION_BACK_CHAR");
}

/* SAI_CHAR_ORIENT_DINAMIC_OBJECTIVE: (state, char, target, flag) */
PyObject *SetStateChar8(PyObject *self, PyObject *args) {
	(void)self;
	return CharStateTargetI(args, "ORIENT_DINAMIC_OBJECTIVE_CHAR");
}

/* SAI_CHAR_ORIENT_DINAMIC_OBJECTIVE_WITH_ACTION: (state, char, target, flag) */
PyObject *SetStateChar9(PyObject *self, PyObject *args) {
	(void)self;
	return CharStateTargetI(args, "ORIENT_DINAMIC_OBJECTIVE_WITH_ACTION_CHAR");
}

/*
 * SetStateVehicle(state, ...) / SetStateChar(state, ...) : the handler for the state number. args[0] is read
 * without checks: an empty tuple leaves the Python error set and returns None, as the original.
 */
static PyObject *(*const vehicleStates[10])(PyObject *, PyObject *) = {
	SetStateVehicle0, SetStateVehicle1, SetStateVehicle2, SetStateVehicle3, SetStateVehicle4,
	SetStateVehicle5, SetStateVehicle6, SetStateVehicle7, SetStateVehicle8, SetStateVehicle9,
};
static PyObject *(*const charStates[10])(PyObject *, PyObject *) = {
	SetStateChar0, SetStateChar1, SetStateChar2, SetStateChar3, SetStateChar4,
	SetStateChar5, SetStateChar6, SetStateChar7, SetStateChar8, SetStateChar9,
};

PyObject *SAI_SetStateVehicle(PyObject *self, PyObject *args) {
	uint32_t state = (uint32_t)PyInt_AsLong(PyTuple_GetItem(args, 0));
	if (state > 9) {
		return ReturnNone();
	}
	return vehicleStates[state](self, args);
}

PyObject *SAI_SetStateChar(PyObject *self, PyObject *args) {
	uint32_t state = (uint32_t)PyInt_AsLong(PyTuple_GetItem(args, 0));
	if (state > 9) {
		return ReturnNone();
	}
	return charStates[state](self, args);
}

/* GetStateVehicle(name) : state number of the vehicle AI, None without AI */
PyObject *SAI_GetStateVehicle(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (PyArg_ParseTuple(args, "s", &name)) {
		cEntity *e = World_FindEntity(name);
		cAIVehicle *ai = e ? e->vftable->GetAI(e) : NULL;
		if (ai) {
			return Py_BuildValue("i", cAIVehicle_GetState(ai));
		}
	}
	return ReturnNone();
}

/* GetStateChar(name) : state number of the character AI, None without AI */
PyObject *SAI_GetStateChar(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (PyArg_ParseTuple(args, "s", &name)) {
		cEntity *e = World_FindEntity(name);
		cAIChar *ai = e ? e->vftable->GetAI(e) : NULL;
		if (ai) {
			return Py_BuildValue("i", ai->vftable->GetState(ai));
		}
	}
	return ReturnNone();
}

/*
 * EnableAIVehicle(name, enable, ...) : 25 more values in three parameter blocks. As compiled the format has 27
 * conversions for 26 pointers, so a 27th argument writes through an undefined pointer; the same here. Failure
 * returns NULL without a Python exception.
 */
PyObject *SAI_EnableAIVehicle(PyObject *self, PyObject *args) {
	char *name;
	int enable;
	cAIVehicleParams1 p1 = { .boostItemLife = 1, .boostItemAmmo = 1, .itemAmmo = 0.25f, .hookRace = 0, .hookMap = 0,
		.strafeEvasion = 0, .percHook = 0, .percHookGetAway = 0 };
	cAIVehicleParams2 p2;
	cAIVehicleParams3 p3 = { .cadenceCM = 3, .counterattack = 1.0f, .percCloud = 35, .percSonic = 50,
		.percElectro = 15 };

	(void)self;
	if (!PyArg_ParseTuple(args, "sifiiiiifiiff|iiffiiiiiiiiii", &name, &enable, &p1.damageGetAway, &p2.strafe,
			&p2.brake, &p1.boostGetAway, &p2.stupidPathfinding, &p2.maxTimeStrafe, &p2.maxVelStrafe, &p2.aim,
			&p3.cadence, &p3.angAttack, &p1.damageItemLife, &p1.boostItemLife, &p1.boostItemAmmo, &p1.itemAmmo,
			&p3.counterattack, &p1.hookRace, &p1.hookMap, &p1.strafeEvasion, &p3.cadenceCM, &p3.percSonic,
			&p3.percCloud, &p3.percElectro, &p1.percHook, &p1.percHookGetAway)) {
		return NULL;
	}
	cEntity *e = FindAIEntity(name, TYEN_CAR);
	if (!e || cVehicleEntity_GetOwnerSlot(e) >= 0) {
		return NULL;
	}
	byte on = enable != 0;
	cAIVehicle *ai = e->vftable->GetAI(e);
	if (on && ai) {
		cAIVehicle_Setup(ai, &p1, &p2, &p3);
		*cAIVehicle_GetParams1(ai) = p1;
		*cAIVehicle_GetParams2(ai) = p2;
		*cAIVehicle_GetParams3(ai) = p3;
	}
	else {
		((const cVehicleEntityVtbl *)e->vftable)->EnableAI(e, on, &p1, &p2, &p3);
	}
	return Py_BuildValue("i", 1);
}

/* EnableAIChar(name, enable, arg) : the name must match the entity exactly (the table lookup ignores case) */
PyObject *SAI_EnableAIChar(PyObject *self, PyObject *args) {
	char *name;
	int enable, arg;

	(void)self;
	if (!PyArg_ParseTuple(args, "sii", &name, &enable, &arg)) {
		return NULL;
	}
	cEntity *e = FindAIEntity(name, TYEN_WALKCHAR);
	if (!e || cWalkCharEntity_GetOwnerSlot(e) >= 0 || strcmp(name, e->name)) {
		return NULL;
	}
	((const cWalkCharEntityVtbl *)e->vftable)->EnableAI(e, enable != 0, &arg);
	return ReturnNone();
}

/*
 * GetNextRacePoint(initialPoint, minDist, maxDist) : next race point; maxDist grows by 1000 until one is found
 * (outdoors only one try). Past 1e8 the CHECK fails. A parse error returns None with the error set.
 */
PyObject *SAI_GetNextRacePoint(PyObject *self, PyObject *args) {
	vec3_t initial, out;
	float minDist, maxDist;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)ff", &initial.x, &initial.y, &initial.z, &minDist, &maxDist)) {
		return ReturnNone();
	}
	int outdoor = g_IsMapOutDoor == 1;
	byte found;
	for (;;) {
		found = CNodesWorld_GetRacePoint(g_NodesWorld, &out, &initial, minDist, maxDist, 0);
		maxDist += 1000.0f;
		if (found || outdoor || !(1e8f >= maxDist)) {
			break;
		}
	}
	CHECK(maxDist <= 1e8f && "Se ha superado la distancia máxima sin haber encontrado un punto");
	if (outdoor && !found) {
		return ReturnNone();
	}
	return Py_BuildValue("fff", (double)out.x, (double)out.y, (double)out.z);
}

/* GetNearestItemLife(name) : name of the nearest life item for the AI of the entity, else None */
PyObject *SAI_GetNearestItemLife(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return ReturnNone();
	}
	cEntity *e = World_FindEntity(name);
	cAIVehicle *ai = e ? e->vftable->GetAI(e) : NULL;
	cEntity *item = ai ? cAIVehicle_GetNearestItemLife(ai) : NULL;
	if (item) {
		return Py_BuildValue("s", item->name);
	}
	return ReturnNone();
}

/* GetRandomVisibilityPoint() : a random point of the visibility graph (CRT rand), None if there is none */
PyObject *SAI_GetRandomVisibilityPoint(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	int n = g_NodesWorld->vftable->NumVisPoints(g_NodesWorld);
	if (n <= 0) {
		return ReturnNone();
	}
	vec3_t p;
	memcpy(&p, g_NodesWorld->vftable->GetVisPoint(g_NodesWorld, crt_rand() % n), sizeof(p));
	return Py_BuildValue("(fff)", (double)p.x, (double)p.y, (double)p.z);
}

/* GetReposCharPos(pos, name[, targetName]) : free position for a character, None if it fails */
PyObject *SAI_GetReposCharPos(PyObject *self, PyObject *args) {
	vec3_t pos, out;
	char *name, *targetName = NULL;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)s|s", &pos.x, &pos.y, &pos.z, &name, &targetName)) {
		return ReturnNone();
	}
	cEntity *target = NULL;
	if (targetName && *targetName) {
		target = World_FindEntity(targetName);
		if (target && !Entity_IsType(target, TYEN_WALKCHAR)) {
			target = NULL;
		}
	}
	if (!cAICharGroup_GetReposPos(g_CharGroup, &pos, name, &out, target)) {
		return ReturnNone();
	}
	return Py_BuildValue("fff", (double)out.x, (double)out.y, (double)out.z);
}

/* InitVehicleRace() : No argument check. */
PyObject *SAI_InitVehicleRace(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	g_VehicleGroup->vftable->InitRace(g_VehicleGroup);
	return ReturnNone();
}

/* AddVehicleRace(name) : adds a vehicle to the race group */
PyObject *SAI_AddVehicleRace(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (PyArg_ParseTuple(args, "s", &name)) {
		cEntity *e = FindAIEntity(name, TYEN_CAR);
		if (e) {
			g_VehicleGroup->vftable->AddVehicle(g_VehicleGroup, e);
		}
	}
	return ReturnNone();
}

/* SetRotStaticObj(name, a, b) */
PyObject *SAI_SetRotStaticObj(PyObject *self, PyObject *args) {
	char *name;
	float a, b;

	(void)self;
	if (PyArg_ParseTuple(args, "sff", &name, &a, &b)) {
		cEntity *e = FindAIEntity(name, TYEN_CAR);
		cAIVehicle *ai = e ? e->vftable->GetAI(e) : NULL;
		if (ai) {
			cAIVehicle_SetRotStatic(ai, a, b);
		}
	}
	return ReturnNone();
}

/* SetInertia(name, on) : on only for the value 1 */
PyObject *SAI_SetInertia(PyObject *self, PyObject *args) {
	char *name;
	int on;

	(void)self;
	if (PyArg_ParseTuple(args, "si", &name, &on)) {
		cEntity *e = FindAIEntity(name, TYEN_CAR);
		cAIVehicle *ai = e ? e->vftable->GetAI(e) : NULL;
		if (ai) {
			cAIVehicle_SetInertia(ai, on == 1);
		}
	}
	return ReturnNone();
}

/* GetOD() : returns the object at rva 0x46797c without a new reference (a folded stub in the original) */
PyObject *SAI_GetOD(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return GAME_DATA(PyObject *, 0x46797c);
}

SD_PY_MODULE(SAI, 0x42a488)
