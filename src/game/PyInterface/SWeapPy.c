/*
 * Python module SWeap (method table SWeap_methods, rva 0x430128): weapon and ammo descriptors of the world.
 * Index checks of the tables stop on int3 like the original (TABLE_AT).
 */
#include <string.h>

#include "scrap/python.h"
#include "scrap/weaps.h"

static PyObject *ReturnNone(void) {
	Py_INCREF(Py_None);
	return Py_None;
}

/* GetNWeap() : Obtiene el numero de armas en el juego */
PyObject *SWeap_GetNWeap(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", g_pWorld->weapons.count);
}

/* GetNAmmo() : Obtiene el numero de municiones en el juego */
PyObject *SWeap_GetNAmmo(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", g_pWorld->ammo.count);
}

/* InitWeap(num) : Inicializa las armas num es el numero de armas. None for num <= 0. */
PyObject *SWeap_InitWeap(PyObject *self, PyObject *args) {
	int num;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &num)) {
		return NULL;
	}
	if (num <= 0) {
		return ReturnNone();
	}
	WeapTable_Init(&g_pWorld->weapons, num);
	return Py_BuildValue("i", 1);
}

/* InitAmmo(num) : Inicializa las municiones del juego. No check of num here, the table checks it. */
PyObject *SWeap_InitAmmo(PyObject *self, PyObject *args) {
	int num;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &num)) {
		return NULL;
	}
	AmmoTable_Init(&g_pWorld->ammo, num);
	return Py_BuildValue("i", 1);
}

/* GetFAmmo(numammo,svar) : Obtiene un valor numerico de las municiones */
PyObject *SWeap_GetFAmmo(PyObject *self, PyObject *args) {
	int n;
	char *var;
	float value;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &n, &var)) {
		return NULL;
	}
	if (!cAmmoInfo_GetF(TABLE_AT(g_pWorld->ammo, n), var, &value)) {
		return ReturnNone();
	}
	return Py_BuildValue("d", (double)value);
}

/* GetSAmmo(numammo,svar) : Obtiene un valor cadena de las municiones */
PyObject *SWeap_GetSAmmo(PyObject *self, PyObject *args) {
	int n;
	char *var, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &n, &var)) {
		return NULL;
	}
	if (!cAmmoInfo_GetS(TABLE_AT(g_pWorld->ammo, n), var, &value)) {
		return ReturnNone();
	}
	return Py_BuildValue("s", value);
}

/* SetFAmmo(numammo,svar,fvalue) : Pone un valor numerico de las municiones */
PyObject *SWeap_SetFAmmo(PyObject *self, PyObject *args) {
	int n;
	char *var;
	double value;

	(void)self;
	if (!PyArg_ParseTuple(args, "isd", &n, &var, &value)) {
		return NULL;
	}
	if (!cAmmoInfo_SetF(TABLE_AT(g_pWorld->ammo, n), var, (float)value)) {
		return ReturnNone();
	}
	return Py_BuildValue("i", 1);
}

/*
 * SetSAmmo(numammo,svar,svalue) : Pone un valor cadena de las municiones. Unlike the other setters it returns
 * NULL without a Python exception for n >= count (signed compare first) and for an unknown var; Python then
 * reports "error return without exception set". Kept as the original does it.
 */
PyObject *SWeap_SetSAmmo(PyObject *self, PyObject *args) {
	int n;
	char *var, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "iss", &n, &var, &value)) {
		return NULL;
	}
	if (n >= g_pWorld->ammo.count) {
		return NULL;
	}
	if (!cAmmoInfo_SetS(TABLE_AT(g_pWorld->ammo, n), var, value)) {
		return NULL;
	}
	return Py_BuildValue("i", 1);
}

/* GetFWeap(numWeap,svar) : Obtiene un valor numerico de las armas */
PyObject *SWeap_GetFWeap(PyObject *self, PyObject *args) {
	int n;
	char *var;
	float value;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &n, &var)) {
		return NULL;
	}
	if (!cWeapInfo_GetF(TABLE_AT(g_pWorld->weapons, n), var, &value)) {
		return ReturnNone();
	}
	return Py_BuildValue("d", (double)value);
}

/* GetSWeap(numWeap,svar) : Obtiene un valor cadena de las armas */
PyObject *SWeap_GetSWeap(PyObject *self, PyObject *args) {
	int n;
	char *var, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &n, &var)) {
		return NULL;
	}
	if (!cWeapInfo_GetS(TABLE_AT(g_pWorld->weapons, n), var, &value)) {
		return ReturnNone();
	}
	return Py_BuildValue("s", value);
}

/* SetFWeap(numWeap,svar,fvalue) : Pone un valor numerico de las armas */
PyObject *SWeap_SetFWeap(PyObject *self, PyObject *args) {
	int n;
	char *var;
	double value;

	(void)self;
	if (!PyArg_ParseTuple(args, "isd", &n, &var, &value)) {
		return NULL;
	}
	if (!cWeapInfo_SetF(TABLE_AT(g_pWorld->weapons, n), var, (float)value)) {
		return ReturnNone();
	}
	return Py_BuildValue("i", 1);
}

/* SetSWeap(numWeap,svar,svalue) : Pone un valor cadena de las armas */
PyObject *SWeap_SetSWeap(PyObject *self, PyObject *args) {
	int n;
	char *var, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "iss", &n, &var, &value)) {
		return NULL;
	}
	if (!cWeapInfo_SetS(TABLE_AT(g_pWorld->weapons, n), var, value)) {
		return ReturnNone();
	}
	return Py_BuildValue("i", 1);
}

/* FillPriority() : Inizializa las prioridades de las armas */
PyObject *SWeap_FillPriority(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	cWorld_FillWeapPriority(g_pWorld);
	return ReturnNone();
}

/* GetFirstiWeap() : Obtiene el arma primera y por defecto */
PyObject *SWeap_GetFirstiWeap(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", g_pWorld->firstWeapon);
}

/*
 * NetExec(cmd[,name]) : Rutinas de red de los misiles. Unknown commands do nothing. Without the second argument
 * the original passes an uninitialized pointer; here it is NULL.
 */
PyObject *SWeap_NetExec(PyObject *self, PyObject *args) {
	char *cmd, *name = NULL;
	const vec3_t zero = { 0.0f, 0.0f, 0.0f };

	(void)self;
	if (!PyArg_ParseTuple(args, "s|s", &cmd, &name)) {
		return NULL;
	}
	if (!strcmp(cmd, "CreateSwarm")) {
		Swarm_NetCreate(name, zero, zero);
	}
	else if (!strcmp(cmd, "SetUpSwarm")) {
		Swarm_NetSetUp(name);
	}
	else if (!strcmp(cmd, "CreateInferno")) {
		Inferno_NetCreate(name, zero, zero);
	}
	else if (!strcmp(cmd, "SetUpInferno")) {
		Inferno_NetSetUp(name);
	}
	return ReturnNone();
}
