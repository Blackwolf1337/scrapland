/*
 * Python module SAct (function table SAct_methods): action classes of animated objects. Original
 * source: PyInterface/SActPy.cpp. Works on a current class (g_pAuxActClass) and a current action (g_pAuxAction)
 * that CreateClass/GetClass and CreateAction/GetAction select.
 */
#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/pyfunction.h"
#include "scrap/scriptapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int(*varfunc_t)(void *obj, const char *name, int set, char **s, double *d, int *i);

/* Set*(varname,value): value as string ("ss") or number ("sd", stored as double and truncated int); 0 with the error
 * of the "sd" parse when neither parses */
static byte SetVar(const nativeVal_t *a, int n, varfunc_t var, void *obj, nativeVal_t *out) {
	const char *name, *s;
	double d;
	int i;

	if (Mode_Parse(a, n, "ss", &name, &s)) {
		return Api_Build(out, "i", var(obj, name, 1, (char **)&s, NULL, NULL));
	}
	Native_ErrClear();
	if (!Mode_Parse(a, n, "sd", &name, &d)) {
		return 0;
	}
	i = (int)d;
	return Api_Build(out, "i", var(obj, name, 1, NULL, &d, &i));
}

/* Get*(varname): the value in its own type; None and a debug message when there is no object or variable */
static byte GetVar(const nativeVal_t *a, int n, varfunc_t var, void *obj, const char *nameNoObj,
	const char *nameNoVar, nativeVal_t *out) {
	/* differs from the original: the values start at 0 (stack garbage there), as in SScorer.Get */
	const char *name;
	char *s = NULL;
	double d = 0.0;
	int i = 0;

	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	if (!obj) {
		Con_Debugf(nameNoObj, name);
		return 1;
	}
	switch (var(obj, name, 0, &s, &d, &i)) {
	case ACTVAR_STRING:
		return Api_Build(out, "s", s);
	case ACTVAR_DOUBLE:
		return Api_Build(out, "d", d);
	case ACTVAR_INT:
		return Api_Build(out, "i", i);
	}
	Con_Debugf(nameNoVar, name);
	return 1;
}

cActClassList *g_pActClassList;
cActClass *g_pAuxActClass;

/*
 * CreateClass(classname) : Crea una clase de objeto animado. differs from the original: a parse error raises its
 * TypeError; the original returns None with the error left set, and its callers went on to set up the current class
 * (another one) with SetCls until the error came out at a later line, or a SetCls with a number cleared it.
 */
API_FN(SAct, CreateClass) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Create(g_pActClassList, name);
	return Api_Build(out, "i", g_pAuxActClass ? 1 : 0);
}

/* DelClass(classname) : Borra una class de objeto animado */
API_FN(SAct, DelClass) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && Api_Build(out, "i", cActClassList_Del(g_pActClassList, name) != 0);
}

/*
 * GetClass(classname) : Activa una clase de objeto animado. differs from the original: a parse error raises its
 * TypeError; the original returns None with the error left set and keeps the current class, and its callers went on
 * (Chars.IntSetChar took None for a missing class, DoorSound.SetInitDoorSound and the sound scripts set actions of the
 * current class, another one) until the error came out at a later line, or a SetAct or SetCls with a number cleared it.
 */
API_FN(SAct, GetClass) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Get(g_pActClassList, name);
	return Api_Build(out, "i", g_pAuxActClass ? 1 : 0);
}

/* SetCls(varname,value) : Modifica el valor de una variable de una class objeto animado; NULL without exception when
 * there is no current class, as the original (golden records it) */
API_FN(SAct, SetCls) {
	if (!g_pAuxActClass) {
		Con_Print(CON_COLOR_ERROR, "ERROR SAct.SetCls(): AuxActClass not defined.\n", 0);
		return 0;
	}
	return SetVar(a, n, (varfunc_t)cActClass_Var, g_pAuxActClass, out);
}

/* GetCls(varname) : Obtiene el valor de una variable de una class objeto animado */
API_FN(SAct, GetCls) {
	return GetVar(a, n, (varfunc_t)cActClass_Var, g_pAuxActClass,
		"Caution: SAct.GetCls('%s'): AuxActClass not defined.\n",
		"Caution: SAct.GetCls('%s'): element not found.\n", out);
}

cAction *g_pAuxAction;

/* CreateAction(varname) : Crea una accion en la class actual. */
API_FN(SAct, CreateAction) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	if (!g_pAuxActClass) {
		Con_Debugf("Caution: SAct.CreateAction(): AuxActClass not defined.\n");
		return 1;
	}
	g_pAuxAction = cActClass_CreateAction(g_pAuxActClass, name);
	return Api_Build(out, "i", g_pAuxAction ? 1 : 0);
}

/* GetAction(varname) : Obtiene una accion de la class actual. */
API_FN(SAct, GetAction) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	if (!g_pAuxActClass) {
		Con_Debugf("Caution: SAct.GetAction(): AuxActClass not defined.\n");
		return 1;
	}
	g_pAuxAction = cActClass_GetAction(g_pAuxActClass, name);
	return Api_Build(out, "i", g_pAuxAction ? 1 : 0);
}

/*
 * SetAct(varname,value) : Modifica el valor de una variable de la accion. Without a current action, and also when
 * the arguments do not parse, the original clears the error and prints the "AuxActClass" message.
 */
API_FN(SAct, SetAct) {
	if (g_pAuxAction && SetVar(a, n, (varfunc_t)cAction_Var, g_pAuxAction, out)) {
		return 1;
	}
	Native_ErrClear();
	Con_Debugf("Caution: SAct.SetAct(): AuxActClass not defined.\n");
	return 1;
}

/* GetAct(varname) : Obtiene el valor de una variable de la accion de una class objeto animado */
API_FN(SAct, GetAct) {
	return GetVar(a, n, (varfunc_t)cAction_Var, g_pAuxAction,
		"Caution: SAct.GetAct('%s'): AuxActClass not defined.\n",
		"Caution: SAct.GetAct('%s'): element not found.\n", out);
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
/* the name of the function of an event: !native, the name of a port entry or a function value ("?" for a value
 * without one), or "" */
static void Event_FuncName(const cPy_Function *f, char *out, int size) {
	out[0] = 0;
	if ((f->flags & PYF_NATIVE) && f->func) {
		const char *n = cPy_NativeFunc_Name((nativefunc_t)f->func);
		Str_Printf(out, size, "!%s", n ? n : "?");
	} else if (f->flags & PYF_ENTRY) {
		Str_Copy(out, cPy_Function_Name((cPy_Function *)f), size);
	} else if ((f->flags & PYF_VALUE) && f->func) {
		const char *n = cPy_Function_Name((cPy_Function *)f);
		Str_Copy(out, *n ? n : "?", size);
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
API_FN(SAct, DumpEvents) {
	const char *path;
	if (!Mode_Parse(a, n, "s", &path)) {
		return 0;
	}
	FILE *f = g_pActClassList ? fopen(path, "w") : NULL;
	if (!f) {
		return Api_Build(out, "i", 0);
	}
	HashTable *t = &g_pActClassList->classes;
	int count = 0;
	for (uint32_t i = 0; i < t->numSlots; i++) {
		for (HashNode *h = t->slots[i]; h; h = h->next) {
			count++;
		}
	}
	cActClass **list = count ? malloc(count * sizeof(*list)) : NULL;
	count = 0;
	for (uint32_t i = 0; list && i < t->numSlots; i++) {
		for (HashNode *h = t->slots[i]; h; h = h->next) {
			list[count++] = h->value;
		}
	}
	if (list) {
		qsort(list, count, sizeof(*list), Dump_Compare);
	}
	static const char *const actVars[] = { "AnmFilePath", "AnmName", "AnmTotalTime", "AnmFPS", "isLoop", "RotSpeed",
		"AngRotLimit", "AngleModifierSpeed", "MaxSpeed", "MinSpeed", "NodeModifier", "PriorityLevel", "InterpTime",
		"Chanel", "UseDisp", "UseFlyDisp", "UseFlyFall", "SlideAsFly", "AutoFaceSpeed", "AutoFaceOffsetH",
		"AutoFaceOffsetV", "anmConeView", NULL };
	static const char *const clsVars[] = { "DefAction", "ModelFilePath", "TurnAngLimit", "FallSound", "FallSoundVol",
		"PhotoPress", "PossessionPress", "Discardable", "SoundDispX", "SoundDispY", "SoundDispZ", NULL };
	for (int c = 0; c < count; c++) {
		fprintf(f, "class %s\n", list[c]->name);
		Dump_Vars(f, "  ", (varfunc_t)cActClass_Var, list[c], clsVars);
		for (cAction *act = list[c]->first; act; act = act->next) {
			fprintf(f, " action %s\n", act->name);
			Dump_Vars(f, "   ", (varfunc_t)cAction_Var, act, actVars);
			for (cActEvent *e = act->events; e; e = e->next) {
				char func[0x100];
				Event_FuncName(&e->func, func, sizeof(func));
				fprintf(f, "  %.6f %s %s %d\n", e->time, e->name, func, e->discardable);
			}
		}
	}
	free(list);
	fclose(f);
	return Api_Build(out, "i", 1);
}

static const portFn_t SAct_methods[] = {
	PORT_FND(CreateClass, SAct_CreateClass, str_PyDoc_SAct_CreateClass),
	PORT_FND(DelClass, SAct_DelClass, str_PyDoc_SAct_DelClass),
	PORT_FND(GetClass, SAct_GetClass, str_PyDoc_SAct_GetClass),
	PORT_FND(SetCls, SAct_SetCls, str_PyDoc_SAct_SetCls),
	PORT_FND(GetCls, SAct_GetCls, str_PyDoc_SAct_GetCls),
	PORT_FND(CreateAction, SAct_CreateAction, str_PyDoc_SAct_CreateAction),
	PORT_FND(GetAction, SAct_GetAction, str_PyDoc_SAct_GetAction),
	PORT_FND(SetAct, SAct_SetAct, str_PyDoc_SAct_SetAct),
	PORT_FND(GetAct, SAct_GetAct, str_PyDoc_SAct_GetAct),
	PORT_FND(DumpEvents, SAct_DumpEvents,
		"DumpEvents(file) : ScrapC only, every class, action and event into the file"),
	PORT_FNEND
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


static char str_PyDoc_SAct_GetAction[] = "GetAction(varname) :  Obtiene una accion de la clase actual.";


static char str_PyDoc_SAct_SetAct[104] =
	"SetAct(varname,value) :  Modifica el valor de una variable de la accion de una clase objeto animado";


static char str_PyDoc_SAct_GetAct[] =
	"GetAct(varname) :  Obtiene el valor de una variable de la accion de una clase objeto animado";


PORTS(PyInterface_SActPy)
