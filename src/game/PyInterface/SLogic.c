/*
 * Python module SLogic (method table SLogic_methods, rva 0x42dd20): game logic of characters, ships and the
 * domination mode. Original source: PyInterface/SLogic.cpp.
 */
#include <string.h>

#include "scrap/global.h"
#include "scrap/logic.h"
#include "scrap/python.h"
#include "scrap/vehicleentity.h"

/* the four traffic lists GetNearestShip searches (pointer table at rva 0x42ddf0) */
static const char *const shipLists[] = { "Traffic", "EnemiesTraffic", "FightingTraffic", "NormalTraffic" };

/*
 * void SetCharState(entityName, state, entityTargetName) : Pone a una entidad en un estado de logica y un target
 * determinados. Any failure (no such WalkChar, no logic) returns NULL without a Python exception, as the original.
 */
static PyObject *SLogic_SetCharState(PyObject *self, PyObject *args) {
	char *name, *targetName;
	int state;

	(void)self;
	if (!PyArg_ParseTuple(args, "sis", &name, &state, &targetName)) {
		return NULL;
	}
	cEntity *e = World_FindEntity(name);
	if (!e || !Entity_IsType(e, TYEN_WALKCHAR)) {
		return NULL;
	}
	cEntity *target = World_FindEntity(targetName);
	void *actCtrl = WithLife_GetActCtrl(e);
	CHECK(actCtrl);
	cLocalLogic *logic = Logic_GetLocalLogic(e);
	if (!logic) {
		return NULL;
	}
	Logic_SetState(logic, state, target);
	return Py_ReturnNone();
}

/* void SendSentinelToWatch(Pos) : Envia a un centinela a inspeccionar una posicion */
static PyObject *SLogic_SendSentinelToWatch(PyObject *self, PyObject *args) {
	vec3_t pos;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)", &pos.x, &pos.y, &pos.z)) {
		return NULL;
	}
	AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, &pos);
	return Py_ReturnNone();
}

/* void GearAttack(Entity) : Envia a un Gear a atacar a una entidad (NULL if there is none of that name) */
static PyObject *SLogic_GearAttack(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, World_FindEntity(name), 0);
	return Py_ReturnNone();
}

/* int IsEnemyActive() : Indica si hay un enemigo activo, mirando la list de enemigos y el tráfico. */
static PyObject *SLogic_IsEnemyActive(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (cEntityList_GetFirst(cWorld_GetList(g_pWorld, "EnemiesTraffic")) ||
		cEntityList_GetFirst(cWorld_GetList(g_pWorld, "Enemies"))) {
		return Py_BuildValue("i", 1);
	}
	return Py_BuildValue("i", 0);
}

/*
 * Name GetNearestShip(pos[,exclude]) : Cicla por varias lists y devuelve la nave más cercana a la posición.
 * As compiled: the first car of a list counts as result before any distance check, and a list entry that is
 * not a Car is used through a NULL pointer (crash in the original as here).
 */
static PyObject *SLogic_GetNearestShip(PyObject *self, PyObject *args) {
	vec3_t pos;
	char *exclude = "";

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)|s", &pos.x, &pos.y, &pos.z, &exclude)) {
		return NULL;
	}
	const char *nearest = "";
	float best = 3.4028234663852886e+38f; /* FLT_MAX, rva 0x3a3a38 */
	for (size_t i = 0; i < sizeof(shipLists) / sizeof(shipLists[0]); i++) {
		cEntityList *list = cWorld_GetList(g_pWorld, shipLists[i]);
		cEntity *first = cEntityList_GetFirst(list);
		if (!first || !Entity_IsType(first, TYEN_CAR)) {
			continue;
		}
		nearest = first->name;
		for (DListNode *node = list->head->data ? list->head : NULL; node;
				node = node->next->data ? node->next : NULL) {
			cEntity *e = node->data;
			if (e && !Entity_IsType(e, TYEN_CAR)) {
				e = NULL;
			}
			if (!strcmp(e->name, exclude) || ((cVehicleEntity *)e)->wl.life <= 0) {
				continue;
			}
			vec3_t tmp;
			const vec3_t *p = Entity_GetPos(e, &tmp);
			float dx = p->x - pos.x, dy = p->y - pos.y, dz = p->z - pos.z;
			float d = dx * dx + dy * dy + dz * dz;
			if (best > d) {
				nearest = e->name;
				best = d;
			}
		}
	}
	return Py_BuildValue("s", nearest);
}

/* void SetOnFloor(Entity) : Pone una entidad en el suelo; 1 if it worked */
static PyObject *SLogic_SetOnFloor(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return Py_BuildValue("i", Logic_SetOnFloor(World_FindEntity(name)) != 0);
}

/* void Flash((x,y,z),radius) : flash at a position */
static PyObject *SLogic_Flash(PyObject *self, PyObject *args) {
	float x, y, z, radius;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)f", &x, &y, &z, &radius)) {
		return NULL;
	}
	vec3_t pos = { x, y, z };
	return Py_BuildValue("i", Logic_Flash(&pos, radius, -1));
}

/* void SetShipToRegenerate(shipName, regSpeed, regEndSpeed): Pone una nave a regenerarse. Not a Car: NULL ship. */
static PyObject *SLogic_SetShipToRegenerate(PyObject *self, PyObject *args) {
	char *name;
	float speed, endSpeed;

	(void)self;
	if (!PyArg_ParseTuple(args, "sff", &name, &speed, &endSpeed)) {
		return NULL;
	}
	cEntity *e = World_FindEntity(name);
	if (e) {
		ShipRegen_Set(&g_pWorld->logic.shipRegen, Entity_IsType(e, TYEN_CAR) ? e : NULL, speed, endSpeed);
	}
	return Py_ReturnNone();
}

/* void SetDominationZones(zonesList): Establece la list de las zones de dominación (WithLife entities by name) */
static PyObject *SLogic_SetDominationZones(PyObject *self, PyObject *args) {
	PyObject *zonesList;

	(void)self;
	if (!PyArg_ParseTuple(args, "O", &zonesList)) {
		return NULL;
	}
	CHECK(PyList_Check(zonesList) && "El argumento pasado no es una lista");
	int size = PyList_Size(zonesList);
	CHECK(size > 0 && "El tamaño de la lista es inválido");
	for (int i = 0; i < size; i++) {
		PyObject *elem = PyList_GetItem(zonesList, i);
		if (!elem) {
			continue;
		}
		CHECK(PyString_Check(elem) && "El elemento no es un string");
		char *name = PyString_AsString(elem);
		CHECK(name && "String invalido");
		cEntity *e = World_FindEntity(name);
		CHECK(e && Entity_IsType(e, TYEN_WITH_LIFE) && "La entidad no es un 'WithLifeEntity'");
		Domination_AddZone(e);
	}
	return Py_ReturnNone();
}

/* void ChangeZoneState(zoneId, state): Pone el estado de una zona de dominación */
static PyObject *SLogic_ChangeZoneState(PyObject *self, PyObject *args) {
	int zone, state;

	(void)self;
	if (!PyArg_ParseTuple(args, "ii", &zone, &state)) {
		return NULL;
	}
	Domination_SetZoneState(zone, state);
	return Py_ReturnNone();
}

/* void ChangeBatonState(pos, state): Actualiza el estado del testigo en el modo dominación */
static PyObject *SLogic_ChangeBatonState(PyObject *self, PyObject *args) {
	vec3_t pos;
	int state;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)i", &pos.x, &pos.y, &pos.z, &state)) {
		return NULL;
	}
	Domination_SetBatonState(&pos, state);
	return Py_ReturnNone();
}

/* void UpdateTauntEndTime(time): Tiempo final del taunt. The original parses no arguments at all. */
static PyObject *SLogic_UpdateTauntEndTime(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Taunts_UpdateEndTime(&g_pWorld->logic);
	return Py_ReturnNone();
}

static char str_PyDoc_SLogic_ChangeBatonState[];
static char str_PyDoc_SLogic_ChangeZoneState[];
static char str_PyDoc_SLogic_Flash[];
static char str_PyDoc_SLogic_GearAttack[72];
static char str_PyDoc_SLogic_GetNearestShip[];
static char str_PyDoc_SLogic_IsEnemyActive[];
static char str_PyDoc_SLogic_SendSentinelToWatch[88];
static char str_PyDoc_SLogic_SetCharState[];
static char str_PyDoc_SLogic_SetDominationZones[136];
static char str_PyDoc_SLogic_SetOnFloor[60];
static char str_PyDoc_SLogic_SetShipToRegenerate[];
static char str_PyDoc_SLogic_UpdateTauntEndTime[60];
static const char str_PyMethod_SLogic_ChangeBatonState[];
static const char str_PyMethod_SLogic_ChangeZoneState[];
static const char str_PyMethod_SLogic_Flash[];
static const char str_PyMethod_SLogic_GearAttack[];
static const char str_PyMethod_SLogic_GetNearestShip[];
static const char str_PyMethod_SLogic_IsEnemyActive[];
static const char str_PyMethod_SLogic_SendSentinelToWatch[];
static const char str_PyMethod_SLogic_SetCharState[];
static const char str_PyMethod_SLogic_SetDominationZones[];
static const char str_PyMethod_SLogic_SetOnFloor[];
static const char str_PyMethod_SLogic_SetShipToRegenerate[];
static const char str_PyMethod_SLogic_UpdateTauntEndTime[];
static PyMethodDef SLogic_methods[13] = {
	{
		.ml_name = (void *)str_PyMethod_SLogic_SendSentinelToWatch,
		.ml_meth = (void *)SLogic_SendSentinelToWatch,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_SendSentinelToWatch,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_GearAttack,
		.ml_meth = (void *)SLogic_GearAttack,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_GearAttack,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_SetCharState,
		.ml_meth = (void *)SLogic_SetCharState,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_SetCharState,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_IsEnemyActive,
		.ml_meth = (void *)SLogic_IsEnemyActive,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_IsEnemyActive,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_GetNearestShip,
		.ml_meth = (void *)SLogic_GetNearestShip,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_GetNearestShip,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_SetOnFloor,
		.ml_meth = (void *)SLogic_SetOnFloor,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_SetOnFloor,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_Flash,
		.ml_meth = (void *)SLogic_Flash,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_Flash,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_SetShipToRegenerate,
		.ml_meth = (void *)SLogic_SetShipToRegenerate,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_SetShipToRegenerate,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_SetDominationZones,
		.ml_meth = (void *)SLogic_SetDominationZones,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_SetDominationZones,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_ChangeZoneState,
		.ml_meth = (void *)SLogic_ChangeZoneState,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_ChangeZoneState,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_ChangeBatonState,
		.ml_meth = (void *)SLogic_ChangeBatonState,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_ChangeBatonState,
	},
	{
		.ml_name = (void *)str_PyMethod_SLogic_UpdateTauntEndTime,
		.ml_meth = (void *)SLogic_UpdateTauntEndTime,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SLogic_UpdateTauntEndTime,
	},
};

SD_PY_MODULE(SLogic, 0x42dd20)

static const char str_PyMethod_SLogic_SendSentinelToWatch[] = "SendSentinelToWatch";
static const char str_PyMethod_SLogic_GearAttack[] = "GearAttack";
static const char str_PyMethod_SLogic_SetCharState[] = "SetCharState";
static const char str_PyMethod_SLogic_IsEnemyActive[] = "IsEnemyActive";
static const char str_PyMethod_SLogic_GetNearestShip[] = "GetNearestShip";
static const char str_PyMethod_SLogic_SetOnFloor[] = "SetOnFloor";
static const char str_PyMethod_SLogic_Flash[] = "Flash";
static const char str_PyMethod_SLogic_SetShipToRegenerate[] = "SetShipToRegenerate";
static const char str_PyMethod_SLogic_SetDominationZones[] = "SetDominationZones";
static const char str_PyMethod_SLogic_ChangeZoneState[] = "ChangeZoneState";
static const char str_PyMethod_SLogic_ChangeBatonState[] = "ChangeBatonState";
static const char str_PyMethod_SLogic_UpdateTauntEndTime[] = "UpdateTauntEndTime";

static char str_PyDoc_SLogic_SetCharState[] =
	"void SetCharState(entityName, state, entityTargetName) : Pone a una entidad en un estado de logica y"
	" un target determinados";

static char str_PyDoc_SLogic_Flash[] = "void Flash((x,y,z),radius) : Pone una entidad en el suelo";

static char str_PyDoc_SLogic_SendSentinelToWatch[88] =
	"void SendSentinelToWatch(Pos) : Envia a un centinela a inspeccionar una posicion";

static char str_PyDoc_SLogic_GearAttack[72] =
	"void GearAttack(Entity) : Envia a un Gear a atacar a una entidad";

static char str_PyDoc_SLogic_IsEnemyActive[] =
	"int IsEnemyActive() : Indica si hay un enemigo activo, mirando el contenido de la lista de enemigos "
	"y en el tráfico.";

static char str_PyDoc_SLogic_GetNearestShip[] =
	"Name GetNearestShip(pos) : Cicla por varias listas y devuelve la nave más cercana a la posición indi"
	"cada";

static char str_PyDoc_SLogic_SetOnFloor[60] = "void SetOnFloor(Entity) : Pone una entidad en el suelo";

static char str_PyDoc_SLogic_SetShipToRegenerate[] =
	"void SetShipToRegenerate(shipName, regSpeed,regEndSpeed): Pone una nave a regenerarse";

static char str_PyDoc_SLogic_SetDominationZones[136] =
	"void SetDominationZones(zonesList): Establece la lista de posiciones de las zonas de dominación en l"
	"a super apuesta de dominación";

static char str_PyDoc_SLogic_ChangeZoneState[] =
	"void ChangeZoneState(zoneId, state): Pone el estado de una zona de dominación";

static char str_PyDoc_SLogic_ChangeBatonState[] =
	"void ChangeBatonState(pos, state): Actualiza el estado del testigo en el modo dominación";

static char str_PyDoc_SLogic_UpdateTauntEndTime[60] = "void UpdateTauntEndTime(time): Tiempo final del taunt";

