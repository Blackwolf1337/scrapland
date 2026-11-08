/*
 * Python module SAct (method table SAct_methods): action classes of animated objects. Original
 * source: PyInterface/SActPy.cpp. Works on a current class (g_pAuxActClass) and a current action (g_pAuxAction)
 * that CreateClass/GetClass and CreateAction/GetAction select.
 */
#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/pyfunction.h"
#include "scrap/python.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	/* differs from the original: the values start at 0 (stack garbage there), as in SScorer.Get */
	char *name, *s = NULL;
	double d = 0.0;
	int i = 0;

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

cActClassList *g_pActClassList;
cActClass *g_pAuxActClass;

/*
 * CreateClass(classname) : Crea una clase de objeto animado. differs from the original: a parse error raises its
 * TypeError; the original returns None with the error left set, and its callers went on to set up the current class
 * (another one) with SetCls until the error came out at a later line, or a SetCls with a number cleared it.
 */
static PyObject *SAct_CreateClass(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Create(g_pActClassList, name);
	return Py_BuildValue("i", g_pAuxActClass ? 1 : 0);
}

/* DelClass(classname) : Borra una class de objeto animado */
static PyObject *SAct_DelClass(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	return Py_BuildValue("i", cActClassList_Del(g_pActClassList, name) != 0);
}

/*
 * GetClass(classname) : Activa una clase de objeto animado. differs from the original: a parse error raises its
 * TypeError; the original returns None with the error left set and keeps the current class, and its callers went on
 * (Chars.IntSetChar took None for a missing class, DoorSound.SetInitDoorSound and the sound scripts set actions of the
 * current class, another one) until the error came out at a later line, or a SetAct or SetCls with a number cleared it.
 */
static PyObject *SAct_GetClass(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Get(g_pActClassList, name);
	return Py_BuildValue("i", g_pAuxActClass ? 1 : 0);
}

/* SetCls(varname,value) : Modifica el valor de una variable de una class objeto animado; NULL without exception when
 * there is no current class, as the original (golden records it) */
static PyObject *SAct_SetCls(PyObject *self, PyObject *args) {
	int parsed;

	(void)self;
	if (!g_pAuxActClass) {
		Con_Print(CON_COLOR_ERROR, "ERROR SAct.SetCls(): AuxActClass not defined.\n", 0);
		return NULL;
	}
	return SetVar(args, (varfunc_t)cActClass_Var, g_pAuxActClass, &parsed);
}

/* GetCls(varname) : Obtiene el valor de una variable de una class objeto animado */
static PyObject *SAct_GetCls(PyObject *self, PyObject *args) {
	(void)self;
	return GetVar(args, (varfunc_t)cActClass_Var, g_pAuxActClass,
		"Caution: SAct.GetCls('%s'): AuxActClass not defined.\n",
		"Caution: SAct.GetCls('%s'): element not found.\n");
}

cAction *g_pAuxAction;

/* CreateAction(varname) : Crea una accion en la class actual. */
static PyObject *SAct_CreateAction(PyObject *self, PyObject *args) {
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
static PyObject *SAct_GetAction(PyObject *self, PyObject *args) {
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
static PyObject *SAct_SetAct(PyObject *self, PyObject *args) {
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
static PyObject *SAct_GetAct(PyObject *self, PyObject *args) {
	(void)self;
	return GetVar(args, (varfunc_t)cAction_Var, g_pAuxAction,
		"Caution: SAct.GetAct('%s'): AuxActClass not defined.\n",
		"Caution: SAct.GetAct('%s'): element not found.\n");
}

static char str_PyDoc_SAct_CreateAction[];
static char str_PyDoc_SAct_CreateClass[];
static char str_PyDoc_SAct_DelClass[];
static char str_PyDoc_SAct_GetAct[];
static char str_PyDoc_SAct_GetAction[];
static char str_PyDoc_SAct_GetClass[];
static char str_PyDoc_SAct_GetCls[];
static char str_PyDoc_SAct_SetAct[104];
static char str_PyDoc_SAct_SetCls[];
static const char str_PyMethod_SAct_CreateAction[];
static const char str_PyMethod_SAct_CreateClass[];
static const char str_PyMethod_SAct_DelClass[];
static const char str_PyMethod_SAct_GetAct[];
static const char str_PyMethod_SAct_GetAction[];
static const char str_PyMethod_SAct_GetClass[];
static const char str_PyMethod_SAct_GetCls[];
static const char str_PyMethod_SAct_SetAct[];
static const char str_PyMethod_SAct_SetCls[];
/* the name of the function of an event: !native, the Python function name (of a port entry too), or "" */
static void Event_FuncName(const cPy_Function *f, char *out, int size) {
	out[0] = 0;
	if ((f->flags & PYF_NATIVE) && f->func) {
		const char *n = cPy_NativeFunc_Name((nativefunc_t)f->func);
		Str_Printf(out, size, "!%s", n ? n : "?");
	} else if (f->flags & PYF_ENTRY) {
		Str_Copy(out, cPy_Function_Name((cPy_Function *)f), size);
	} else if ((f->flags & PYF_PYTHON) && f->func) {
		PyObject *name = PyObject_GetAttrString((PyObject *)f->func, "__name__");
		Str_Copy(out, name && PyString_Check(name) ? PyString_AsString(name) : "?", size);
		Py_XDECREF(name);
		PyErr_Clear();
	}
}

/* the variables of a class or an action that can be read, one line */
static void Dump_Vars(FILE *f, const char *indent, varfunc_t var, void *obj, const char *const *names) {
	fprintf(f, "%s", indent);
	for (int k = 0; names[k]; k++) {
		char *s = NULL;
		double d = 0.0;
		int i = 0;
		switch (var(obj, names[k], 0, &s, &d, &i)) {
		case ACTVAR_STRING:
			fprintf(f, " %s=%s", names[k], s ? s : "");
			break;
		case ACTVAR_DOUBLE:
			fprintf(f, " %s=%.6g", names[k], d);
			break;
		case ACTVAR_INT:
			fprintf(f, " %s=%d", names[k], i);
			break;
		}
	}
	fprintf(f, "\n");
}

static int Dump_Compare(const void *a, const void *b) {
	return strcmp((*(cActClass *const *)a)->name, (*(cActClass *const *)b)->name);
}

/*
 * differs from the original (ScrapC only): DumpEvents(file) writes every class with its readable variables, its
 * actions with theirs and their events (time, name, function, discardable) to file, the classes by name; to compare
 * script ports. 1 when written.
 */
static PyObject *SAct_DumpEvents(PyObject *self, PyObject *args) {
	char *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &path)) {
		return NULL;
	}
	FILE *f = g_pActClassList ? fopen(path, "w") : NULL;
	if (!f) {
		return Py_BuildValue("i", 0);
	}
	HashTable *t = &g_pActClassList->classes;
	int n = 0;
	for (uint32_t i = 0; i < t->numSlots; i++) {
		for (HashNode *h = t->slots[i]; h; h = h->next) {
			n++;
		}
	}
	cActClass **list = n ? malloc(n * sizeof(*list)) : NULL;
	n = 0;
	for (uint32_t i = 0; list && i < t->numSlots; i++) {
		for (HashNode *h = t->slots[i]; h; h = h->next) {
			list[n++] = h->value;
		}
	}
	if (list) {
		qsort(list, n, sizeof(*list), Dump_Compare);
	}
	static const char *const actVars[] = { "AnmFilePath", "AnmName", "AnmTotalTime", "AnmFPS", "isLoop", "RotSpeed",
		"AngRotLimit", "AngleModifierSpeed", "MaxSpeed", "MinSpeed", "NodeModifier", "PriorityLevel", "InterpTime",
		"Chanel", "UseDisp", "UseFlyDisp", "UseFlyFall", "SlideAsFly", "AutoFaceSpeed", "AutoFaceOffsetH",
		"AutoFaceOffsetV", "anmConeView", NULL };
	static const char *const clsVars[] = { "DefAction", "ModelFilePath", "TurnAngLimit", "FallSound", "FallSoundVol",
		"PhotoPress", "PossessionPress", "Discardable", "SoundDispX", "SoundDispY", "SoundDispZ", NULL };
	for (int c = 0; c < n; c++) {
		fprintf(f, "class %s\n", list[c]->name);
		Dump_Vars(f, "  ", (varfunc_t)cActClass_Var, list[c], clsVars);
		for (cAction *a = list[c]->first; a; a = a->next) {
			fprintf(f, " action %s\n", a->name);
			Dump_Vars(f, "   ", (varfunc_t)cAction_Var, a, actVars);
			for (cActEvent *e = a->events; e; e = e->next) {
				char func[0x100];
				Event_FuncName(&e->func, func, sizeof(func));
				fprintf(f, "  %.6f %s %s %d\n", e->time, e->name, func, e->discardable);
			}
		}
	}
	free(list);
	fclose(f);
	return Py_BuildValue("i", 1);
}

static PyMethodDef SAct_methods[11] = {
	{
		.ml_name = (void *)str_PyMethod_SAct_CreateClass,
		.ml_meth = (void *)SAct_CreateClass,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_CreateClass,
	},
	{
		.ml_name = (void *)str_PyMethod_SAct_DelClass,
		.ml_meth = (void *)SAct_DelClass,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_DelClass,
	},
	{
		.ml_name = (void *)str_PyMethod_SAct_GetClass,
		.ml_meth = (void *)SAct_GetClass,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_GetClass,
	},
	{
		.ml_name = (void *)str_PyMethod_SAct_SetCls,
		.ml_meth = (void *)SAct_SetCls,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_SetCls,
	},
	{
		.ml_name = (void *)str_PyMethod_SAct_GetCls,
		.ml_meth = (void *)SAct_GetCls,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_GetCls,
	},
	{
		.ml_name = (void *)str_PyMethod_SAct_CreateAction,
		.ml_meth = (void *)SAct_CreateAction,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_CreateAction,
	},
	{
		.ml_name = (void *)str_PyMethod_SAct_GetAction,
		.ml_meth = (void *)SAct_GetAction,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_GetAction,
	},
	{
		.ml_name = (void *)str_PyMethod_SAct_SetAct,
		.ml_meth = (void *)SAct_SetAct,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_SetAct,
	},
	{
		.ml_name = (void *)str_PyMethod_SAct_GetAct,
		.ml_meth = (void *)SAct_GetAct,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SAct_GetAct,
	},
	{
		.ml_name = "DumpEvents",
		.ml_meth = (void *)SAct_DumpEvents,
		.ml_flags = 1,
		.ml_doc = "DumpEvents(file) : ScrapC only, every class, action and event into the file",
	},
};

SD_PY_MODULE(SAct, 0x429288)

static char str_PyDoc_SAct_CreateClass[] = "CreateClass(classname) : Crea una clase de objeto animado";

static char str_PyDoc_SAct_DelClass[] = "DelClass(classname) : Crea una clase de objeto animado";

static char str_PyDoc_SAct_GetClass[] = "GetClass(classname) : Activa una clase de objeto animado";

static char str_PyDoc_SAct_SetCls[] =
	"SetCls(varname,value) :  Modifica el valor de una variable de una clase objeto animado";

static char str_PyDoc_SAct_GetCls[] =
	"GetCls(varname) :  Obtiene el valor de una variable de una clase objeto animado";

static char str_PyDoc_SAct_CreateAction[] = "CreateAction(varname) :  Crea una accion en la clase actual.";

static const char str_PyMethod_SAct_CreateClass[] = "CreateClass";

static char str_PyDoc_SAct_GetAction[] = "GetAction(varname) :  Obtiene una accion de la clase actual.";

static const char str_PyMethod_SAct_DelClass[] = "DelClass";
static const char str_PyMethod_SAct_GetClass[] = "GetClass";

static char str_PyDoc_SAct_SetAct[104] =
	"SetAct(varname,value) :  Modifica el valor de una variable de la accion de una clase objeto animado";

static const char str_PyMethod_SAct_SetCls[] = "SetCls";
static const char str_PyMethod_SAct_GetCls[] = "GetCls";
static const char str_PyMethod_SAct_CreateAction[] = "CreateAction";

static char str_PyDoc_SAct_GetAct[] =
	"GetAct(varname) :  Obtiene el valor de una variable de la accion de una clase objeto animado";

static const char str_PyMethod_SAct_GetAction[] = "GetAction";
static const char str_PyMethod_SAct_SetAct[] = "SetAct";
static const char str_PyMethod_SAct_GetAct[] = "GetAct";

