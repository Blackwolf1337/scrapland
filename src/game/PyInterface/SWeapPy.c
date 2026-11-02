/*
 * Python module SWeap (method table SWeap_methods, rva 0x430128): weapon and ammo descriptors of the world.
 * Index checks of the tables stop on int3 like the original (TABLE_AT).
 */
#include <string.h>

#include "scrap/python.h"
#include "scrap/weaps.h"
#include "scrap/world.h"

/* GetNWeap() : Obtiene el numero de armas en el juego */
static PyObject *SWeap_GetNWeap(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", g_pWorld->weapons.count);
}

/* GetNAmmo() : Obtiene el numero de municiones en el juego */
static PyObject *SWeap_GetNAmmo(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", g_pWorld->ammo.count);
}

/* InitWeap(num) : Inicializa las armas num es el numero de armas. None for num <= 0. */
static PyObject *SWeap_InitWeap(PyObject *self, PyObject *args) {
	int num;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &num)) {
		return NULL;
	}
	if (num <= 0) {
		return Py_ReturnNone();
	}
	WeapTable_Init(&g_pWorld->weapons, num);
	return Py_BuildValue("i", 1);
}

/* InitAmmo(num) : Inicializa las municiones del juego. No check of num here, the table checks it. */
static PyObject *SWeap_InitAmmo(PyObject *self, PyObject *args) {
	int num;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &num)) {
		return NULL;
	}
	AmmoTable_Init(&g_pWorld->ammo, num);
	return Py_BuildValue("i", 1);
}

/* GetFAmmo(numammo,svar) : Obtiene un valor numerico de las municiones */
static PyObject *SWeap_GetFAmmo(PyObject *self, PyObject *args) {
	int n;
	char *var;
	float value;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &n, &var)) {
		return NULL;
	}
	if (!cAmmoInfo_GetF(TABLE_AT(g_pWorld->ammo, n), var, &value)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("d", (double)value);
}

/* GetSAmmo(numammo,svar) : Obtiene un valor cadena de las municiones */
static PyObject *SWeap_GetSAmmo(PyObject *self, PyObject *args) {
	int n;
	char *var, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &n, &var)) {
		return NULL;
	}
	if (!cAmmoInfo_GetS(TABLE_AT(g_pWorld->ammo, n), var, &value)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("s", value);
}

/* SetFAmmo(numammo,svar,fvalue) : Pone un valor numerico de las municiones */
static PyObject *SWeap_SetFAmmo(PyObject *self, PyObject *args) {
	int n;
	char *var;
	double value;

	(void)self;
	if (!PyArg_ParseTuple(args, "isd", &n, &var, &value)) {
		return NULL;
	}
	if (!cAmmoInfo_SetF(TABLE_AT(g_pWorld->ammo, n), var, (float)value)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("i", 1);
}

/*
 * SetSAmmo(numammo,svar,svalue) : Pone un valor cadena de las municiones. Unlike the other setters it returns
 * NULL without a Python exception for n >= count (signed compare first) and for an unknown var; Python then
 * reports "error return without exception set". Kept as the original does it.
 */
static PyObject *SWeap_SetSAmmo(PyObject *self, PyObject *args) {
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
static PyObject *SWeap_GetFWeap(PyObject *self, PyObject *args) {
	int n;
	char *var;
	float value;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &n, &var)) {
		return NULL;
	}
	if (!cWeapInfo_GetF(TABLE_AT(g_pWorld->weapons, n), var, &value)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("d", (double)value);
}

/* GetSWeap(numWeap,svar) : Obtiene un valor cadena de las armas */
static PyObject *SWeap_GetSWeap(PyObject *self, PyObject *args) {
	int n;
	char *var, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &n, &var)) {
		return NULL;
	}
	if (!cWeapInfo_GetS(TABLE_AT(g_pWorld->weapons, n), var, &value)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("s", value);
}

/* SetFWeap(numWeap,svar,fvalue) : Pone un valor numerico de las armas */
static PyObject *SWeap_SetFWeap(PyObject *self, PyObject *args) {
	int n;
	char *var;
	double value;

	(void)self;
	if (!PyArg_ParseTuple(args, "isd", &n, &var, &value)) {
		return NULL;
	}
	if (!cWeapInfo_SetF(TABLE_AT(g_pWorld->weapons, n), var, (float)value)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("i", 1);
}

/* SetSWeap(numWeap,svar,svalue) : Pone un valor cadena de las armas */
static PyObject *SWeap_SetSWeap(PyObject *self, PyObject *args) {
	int n;
	char *var, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "iss", &n, &var, &value)) {
		return NULL;
	}
	if (!cWeapInfo_SetS(TABLE_AT(g_pWorld->weapons, n), var, value)) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("i", 1);
}

/* FillPriority() : Inizializa las prioridades de las armas */
static PyObject *SWeap_FillPriority(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	cWorld_FillWeapPriority(g_pWorld);
	return Py_ReturnNone();
}

/* GetFirstiWeap() : Obtiene el arma primera y por defecto */
static PyObject *SWeap_GetFirstiWeap(PyObject *self, PyObject *args) {
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
static PyObject *SWeap_NetExec(PyObject *self, PyObject *args) {
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
	return Py_ReturnNone();
}

static const char str_PyMethod_SWeap_GetNWeap[] = "GetNWeap";

static char str_PyDoc_SWeap_GetNWeap[] = "GetNWeap() : Obtiene el numero de armas en el juego";

static const char str_PyMethod_SWeap_GetNAmmo[] = "GetNAmmo";

static char str_PyDoc_SWeap_GetNAmmo[] = "GetNAmmo() : Obtiene el numero de municiones en el juego";

static const char str_PyMethod_SWeap_InitWeap[] = "InitWeap";

static char str_PyDoc_SWeap_InitWeap[] = "InitWeap(num) : Inicializa las armas num es el numero de armas";

static const char str_PyMethod_SWeap_InitAmmo[] = "InitAmmo";

static char str_PyDoc_SWeap_InitAmmo[56] = "InitAmmo(num) : Inicializa las municiones del juego";

static const char str_PyMethod_SWeap_GetFAmmo[] = "GetFAmmo";

static char str_PyDoc_SWeap_GetFAmmo[] =
	"GetFAmmo(numammo,svar) : Obtiene un valor numerico de las municiones";

static const char str_PyMethod_SWeap_GetSAmmo[] = "GetSAmmo";

static char str_PyDoc_SWeap_GetSAmmo[72] =
	"GetSAmmo(numammo,svar) : Obtiene un valor cadena de las municiones";

static const char str_PyMethod_SWeap_SetFAmmo[] = "SetFAmmo";

static char str_PyDoc_SWeap_SetFAmmo[80] =
	"GetFAmmo(numammo,svar,fvalue) : Pone un valor numerico de las municiones";

static const char str_PyMethod_SWeap_SetSAmmo[] = "SetSAmmo";

static char str_PyDoc_SWeap_SetSAmmo[] =
	"SetSAmmo(numammo,svar,svalue) : Pone un valor cadena de las municiones";

static const char str_PyMethod_SWeap_GetFWeap[] = "GetFWeap";

static char str_PyDoc_SWeap_GetFWeap[] =
	"GetFWeap(numWeap,svar) : Obtiene un valor numerico de las municiones";

static const char str_PyMethod_SWeap_GetSWeap[] = "GetSWeap";

static char str_PyDoc_SWeap_GetSWeap[72] =
	"GetSWeap(numWeap,svar) : Obtiene un valor cadena de las municiones";

static const char str_PyMethod_SWeap_SetFWeap[] = "SetFWeap";

static char str_PyDoc_SWeap_SetFWeap[80] =
	"GetFWeap(numWeap,svar,fvalue) : Pone un valor numerico de las municiones";

static const char str_PyMethod_SWeap_SetSWeap[] = "SetSWeap";
static const char str_PyMethod_SWeap_FillPriority[] = "FillPriority";
static const char str_PyMethod_SWeap_GetFirstiWeap[] = "GetFirstiWeap";
static const char str_PyMethod_SWeap_NetExec[] = "NetExec";
static char str_PyDoc_SWeap_FillPriority[];
static char str_PyDoc_SWeap_GetFirstiWeap[];
static char str_PyDoc_SWeap_NetExec[];
static char str_PyDoc_SWeap_SetSWeap[];
static PyMethodDef SWeap_methods[16] = {
	{
		.ml_name = (void *)str_PyMethod_SWeap_GetNWeap,
		.ml_meth = (void *)SWeap_GetNWeap,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_GetNWeap,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_GetNAmmo,
		.ml_meth = (void *)SWeap_GetNAmmo,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_GetNAmmo,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_InitWeap,
		.ml_meth = (void *)SWeap_InitWeap,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_InitWeap,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_InitAmmo,
		.ml_meth = (void *)SWeap_InitAmmo,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_InitAmmo,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_GetFAmmo,
		.ml_meth = (void *)SWeap_GetFAmmo,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_GetFAmmo,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_GetSAmmo,
		.ml_meth = (void *)SWeap_GetSAmmo,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_GetSAmmo,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_SetFAmmo,
		.ml_meth = (void *)SWeap_SetFAmmo,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_SetFAmmo,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_SetSAmmo,
		.ml_meth = (void *)SWeap_SetSAmmo,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_SetSAmmo,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_GetFWeap,
		.ml_meth = (void *)SWeap_GetFWeap,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_GetFWeap,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_GetSWeap,
		.ml_meth = (void *)SWeap_GetSWeap,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_GetSWeap,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_SetFWeap,
		.ml_meth = (void *)SWeap_SetFWeap,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_SetFWeap,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_SetSWeap,
		.ml_meth = (void *)SWeap_SetSWeap,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_SetSWeap,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_FillPriority,
		.ml_meth = (void *)SWeap_FillPriority,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_FillPriority,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_GetFirstiWeap,
		.ml_meth = (void *)SWeap_GetFirstiWeap,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_GetFirstiWeap,
	},
	{
		.ml_name = (void *)str_PyMethod_SWeap_NetExec,
		.ml_meth = (void *)SWeap_NetExec,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SWeap_NetExec,
	},
};

SD_PY_MODULE(SWeap, 0x430128)

static char str_PyDoc_SWeap_SetSWeap[] =
	"SetSWeap(numammo,svar,svalue) : Pone un valor cadena de las municiones";

static char str_PyDoc_SWeap_FillPriority[] = "FillPriority() : Inizializa las prioridades de las armas";

static char str_PyDoc_SWeap_GetFirstiWeap[] = "GetFirstiWeap() : Obtiene el arma primera y por defecto";

static char str_PyDoc_SWeap_NetExec[] = "NetExec() : Rutinas de red de los misiles";

