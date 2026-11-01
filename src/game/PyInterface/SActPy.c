/*
 * Python module SAct (method table SAct_methods, rva 0x429288): action classes of animated objects. Original
 * source: PyInterface/SActPy.cpp. Works on a current class (g_pAuxActClass) and a current action (g_pAuxAction)
 * that CreateClass/GetClass and CreateAction/GetAction select.
 */
#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/python.h"

typedef int(*varfunc_t)(void *obj, const char *name, int set, char **s, double *d, int *i);

/* Set*(varname,value): value as string ("ss") or number ("sd", stored as double and truncated int) */
static PyObject *SetVar(PyObject *args, varfunc_t var, void *obj, int *parsed) {
	char *name, *s;
	double d;
	int i;

	*parsed = 1;
	if (PyArg_ParseTuple(args, "ss", &name, &s)) {
		return Py_BuildValue("i", var(obj, name, 1, &s, NULL, NULL));
	}
	PyErr_Clear();
	if (!PyArg_ParseTuple(args, "sd", &name, &d)) {
		*parsed = 0;
		return NULL;
	}
	i = (int)d;
	return Py_BuildValue("i", var(obj, name, 1, NULL, &d, &i));
}

/* Get*(varname): the value in its own type; None and a debug message when there is no object or variable */
static PyObject *GetVar(PyObject *args, varfunc_t var, void *obj, const char *nameNoObj, const char *nameNoVar) {
	char *name, *s;
	double d;
	int i;

	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	if (!obj) {
		Con_Debugf(nameNoObj, name);
		return Py_ReturnNone();
	}
	switch (var(obj, name, 0, &s, &d, &i)) {
	case ACTVAR_STRING:
		return Py_BuildValue("s", s);
	case ACTVAR_DOUBLE:
		return Py_BuildValue("d", d);
	case ACTVAR_INT:
		return Py_BuildValue("i", i);
	}
	Con_Debugf(nameNoVar, name);
	return Py_ReturnNone();
}

/* CreateClass(classname) : Crea una clase de objeto animado. A parse error returns None with the error set. */
PyObject *SAct_CreateClass(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return Py_ReturnNone();
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Create(g_pActClassList, name);
	return Py_BuildValue("i", g_pAuxActClass ? 1 : 0);
}

/* DelClass(classname) : Borra una class de objeto animado */
PyObject *SAct_DelClass(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return Py_BuildValue("i", cActClassList_Del(g_pActClassList, name) != 0);
}

/* GetClass(classname) : Activa una clase de objeto animado. A parse error returns None with the error set. */
PyObject *SAct_GetClass(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return Py_ReturnNone();
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Get(g_pActClassList, name);
	return Py_BuildValue("i", g_pAuxActClass ? 1 : 0);
}

/* SetCls(varname,value) : Modifica el valor de una variable de una class objeto animado */
PyObject *SAct_SetCls(PyObject *self, PyObject *args) {
	int parsed;

	(void)self;
	if (!g_pAuxActClass) {
		Con_Print(CON_COLOR_ERROR, "ERROR SAct.SetCls(): AuxActClass not defined.\n", 0);
		return NULL;
	}
	return SetVar(args, (varfunc_t)cActClass_Var, g_pAuxActClass, &parsed);
}

/* GetCls(varname) : Obtiene el valor de una variable de una class objeto animado */
PyObject *SAct_GetCls(PyObject *self, PyObject *args) {
	(void)self;
	return GetVar(args, (varfunc_t)cActClass_Var, g_pAuxActClass,
		"Caution: SAct.GetCls('%s'): AuxActClass not defined.\n",
		"Caution: SAct.GetCls('%s'): element not found.\n");
}

/* CreateAction(varname) : Crea una accion en la class actual. */
PyObject *SAct_CreateAction(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	if (!g_pAuxActClass) {
		Con_Debugf("Caution: SAct.CreateAction(): AuxActClass not defined.\n");
		return Py_ReturnNone();
	}
	g_pAuxAction = cActClass_CreateAction(g_pAuxActClass, name);
	return Py_BuildValue("i", g_pAuxAction ? 1 : 0);
}

/* GetAction(varname) : Obtiene una accion de la class actual. */
PyObject *SAct_GetAction(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	if (!g_pAuxActClass) {
		Con_Debugf("Caution: SAct.GetAction(): AuxActClass not defined.\n");
		return Py_ReturnNone();
	}
	g_pAuxAction = cActClass_GetAction(g_pAuxActClass, name);
	return Py_BuildValue("i", g_pAuxAction ? 1 : 0);
}

/*
 * SetAct(varname,value) : Modifica el valor de una variable de la accion. Without a current action, and also when
 * the arguments do not parse, the original clears the error and prints the "AuxActClass" message.
 */
PyObject *SAct_SetAct(PyObject *self, PyObject *args) {
	int parsed = 0;

	(void)self;
	if (g_pAuxAction) {
		PyObject *r = SetVar(args, (varfunc_t)cAction_Var, g_pAuxAction, &parsed);
		if (parsed) {
			return r;
		}
	}
	PyErr_Clear();
	Con_Debugf("Caution: SAct.SetAct(): AuxActClass not defined.\n");
	return Py_ReturnNone();
}

/* GetAct(varname) : Obtiene el valor de una variable de la accion de una class objeto animado */
PyObject *SAct_GetAct(PyObject *self, PyObject *args) {
	(void)self;
	return GetVar(args, (varfunc_t)cAction_Var, g_pAuxAction,
		"Caution: SAct.GetAct('%s'): AuxActClass not defined.\n",
		"Caution: SAct.GetAct('%s'): element not found.\n");
}

SD_PY_MODULE(SAct, 0x429288)
