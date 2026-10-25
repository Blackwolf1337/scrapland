/*
 * Python module SAI (method table SAI_methods, rva 0x42a488): AI setup and control of vehicles and characters.
 * Original source: PyInterface/SAIPy.cpp. The state handlers behind SetStateVehicle/SetStateChar are static
 * functions of the same file and still the originals (imports).
 */
#include <string.h>

#include "scrap/ai.h"
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
	cAIVehicleParams1 p1 = { .i4 = 1, .i5 = 1, .f2 = 0.25f, .i6 = 0, .i7 = 0, .i8 = 0, .i9 = 0, .i10 = 0 };
	cAIVehicleParams2 p2;
	cAIVehicleParams3 p3 = { .i1 = 3, .f3 = 1.0f, .i4 = 35, .i5 = 50, .i6 = 15 };

	(void)self;
	if (!PyArg_ParseTuple(args, "sifiiiiifiiff|iiffiiiiiiiiii", &name, &enable, &p1.f0, &p2.i1, &p2.i2, &p1.i3,
			&p2.i0, &p2.i3, &p2.f4, &p2.i5, &p3.i0, &p3.f2, &p1.f1, &p1.i4, &p1.i5, &p1.f2, &p3.f3, &p1.i6,
			&p1.i7, &p1.i8, &p3.i1, &p3.i5, &p3.i4, &p3.i6, &p1.i9, &p1.i10)) {
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
