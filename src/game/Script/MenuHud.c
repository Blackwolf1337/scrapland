/*
 * The HUD in C (issue #2, step 5a): the Scorer module (status, police meter, monitor, messages, mission scorer,
 * radar) and the CharScorer module (the HUD on foot). The layout is data/menus/hud.menu.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

static const char hud_menu[] = {
#embed "../../../data/menus/hud.menu"
	, 0 };

#define HUD_FILE "menus/hud.menu"

/* the mission scorer is shown when one of its three statuses is on */
static void Step_MissionVisible(int id, PyObject *args) {
	(void)args;
	for (int i = 0; i < 3; i++) {
		char var[0x10];
		Str_Printf(var, sizeof(var), "Status%d", i);
		if (Script_GetNum(id, "Mission", var) == 1) {
			Script_SetNum(id, "Mission", "Visible", 1);
			return;
		}
	}
}

static const scriptStep_t hud_steps[] = {
	{ "missionvisible", Step_MissionVisible },
	{ NULL, NULL },
};

/* the margins of the screen and more values (format and arguments as Py_BuildValue for a dict, or NULL) */
static void Hud_Run(int id, const char *section, PyObject *more) {
	int mh = Script_GetInt("Scorer", "MarginH", 0), mv = Script_GetInt("Scorer", "MarginV", 0);
	PyObject *vars = Py_BuildValue("{s:i,s:i,s:i,s:i}", "mh", mh, "mv", mv, "right", 640 - mh, "bottom", 480 - mv);
	PyObject *k, *v;
	int pos = 0;
	while (vars && more && PyDict_Next(more, &pos, &k, &v)) {
		PyDict_SetItem(vars, k, v);
	}
	Py_XDECREF(more);
	Script_RunMenu(id, HUD_FILE, section, hud_menu, hud_steps, vars);
	Py_XDECREF(vars);
}

/* a garage or a network game */
static byte Hud_Multi(void) {
	return Script_GetInt("Init", "isShipEdit", 0) || (g_pApp->flags & 7);
}

/* ---- the functions of Scorer ---- */

static PyObject *Scorer_CreateStatusScorer(PyObject *self, PyObject *args) {
	int id, ch = 0;
	(void)self;
	if (!PyArg_ParseTuple(args, "i|i", &id, &ch)) {
		return NULL;
	}
	Hud_Run(id, "status", Py_BuildValue("{s:i,s:i,s:i}", "char", ch != 0, "multi", Hud_Multi(), "split",
		Script_GetInt("Scorer", "SplitScreen", 0) != 0));
	return Py_ReturnNone();
}

static PyObject *Scorer_CreateCopMeterScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Hud_Run(id, "copmeter", NULL);
	return Py_ReturnNone();
}

static PyObject *Scorer_CreateMonitorScorer(PyObject *self, PyObject *args) {
	int id, ch = 0;
	(void)self;
	if (!PyArg_ParseTuple(args, "i|i", &id, &ch)) {
		return NULL;
	}
	Hud_Run(id, "monitor", Py_BuildValue("{s:i}", "char", ch != 0));
	return Py_ReturnNone();
}

static PyObject *Scorer_CreateMsgScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Hud_Run(id, "msg", Py_BuildValue("{s:i,s:i}", "splitnow", (int)GVar_GetNum("SplitScreen", 0), "enabled", 1));
	return Py_ReturnNone();
}

static PyObject *Scorer_CreateMissionScorer(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "enabled", NULL };
	int id, enabled = 1;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "i|i", names, &id, &enabled)) {
		return NULL;
	}
	Hud_Run(id, "mission", Py_BuildValue("{s:i}", "enabled", enabled != 0));
	return Py_ReturnNone();
}

/* ShowMissionScorer(id[, status[, value]]): a status of the mission scorer on or off, it blinks */
static PyObject *Scorer_ShowMissionScorer(PyObject *self, PyObject *args) {
	int id, status = 0;
	PyObject *value = NULL;
	char name[0x40], var[0x10];
	(void)self;
	if (!PyArg_ParseTuple(args, "i|iO", &id, &status, &value)) {
		return NULL;
	}
	Str_Copy(name, GVar_GetStr("MissionScorerName"), sizeof(name));
	Script_SetNum(id, name, "Visible", 1);
	Script_SetNum(id, name, "BlinkTime", 15);
	Script_SetNum(id, name, "SolidTime", 5);
	Str_Printf(var, sizeof(var), "Status%d", status);
	Script_SetNum(id, name, var, !value ? 1 : PyFloat_Check(value) ? PyFloat_AsDouble(value) : PyInt_AsLong(value));
	PyErr_Clear();
	byte off = Script_HasItem(id, name);
	for (int i = 0; off && i < 3; i++) {
		Str_Printf(var, sizeof(var), "Status%d", i);
		off = Script_GetNum(id, name, var) == 0;
	}
	if (off) {
		Script_SetNum(id, "Mission", "Visible", 0);
	}
	return Py_ReturnNone();
}

static PyObject *Scorer_CreateRadarScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Hud_Run(id, "radar", NULL);
	return Py_ReturnNone();
}

/* Blink(n[, item]): the item (the radar) blinks n times, an eighth of a second each */
static PyObject *Scorer_Blink(PyObject *self, PyObject *args) {
	int n;
	char *item = "Radar";
	(void)self;
	if (!PyArg_ParseTuple(args, "i|s", &n, &item)) {
		return NULL;
	}
	Script_SetNum(0, item, "Visible", ((1 + n) % 2 + 2) % 2);
	if (n > 0) {
		PyObject *m = PyImport_ImportModule("Scorer");
		PyObject *f = m ? PyObject_GetAttrString(m, "Blink") : NULL;
		PyObject *a = Py_BuildValue("(is)", n - 1, item);
		if (f && a) {
			Scheduler_Add(&g_pWorld->scheduler, Timer_GetTime(&g_pWorld->timer) + 0.125f, f, a, "");
		}
		PyErr_Clear();
		Py_XDECREF(a);
		Py_XDECREF(f);
		Py_XDECREF(m);
	}
	return Py_ReturnNone();
}

/* MissionScorerBar_SetDefaults(id, name, (back colour), (main colour), (black colour)) */
static PyObject *Scorer_MissionScorerBar_SetDefaults(PyObject *self, PyObject *args) {
	int id, c[12];
	char *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "is(iiii)(iiii)(iiii)", &id, &name, &c[0], &c[1], &c[2], &c[3], &c[4], &c[5], &c[6],
		&c[7], &c[8], &c[9], &c[10], &c[11])) {
		return NULL;
	}
	PyObject *m = PyImport_ImportModule("Scorer");
	PyObject *x = m ? PyObject_GetAttrString(m, "MissionScorerBar_X") : NULL;
	PyObject *y = m ? PyObject_GetAttrString(m, "MissionScorerBar_Y") : NULL;
	PyErr_Clear();
	Hud_Run(id, "missionbar", Py_BuildValue("{s:s,s:O,s:O,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i}", "name",
		name, "x", x ? x : Py_None, "y", y ? y : Py_None, "backr", c[0], "backg", c[1], "backb", c[2], "backa", c[3],
		"mainr", c[4], "maing", c[5], "mainb", c[6], "maina", c[7], "blackr", c[8], "blackg", c[9], "blackb", c[10],
		"blacka", c[11]));
	Py_XDECREF(x);
	Py_XDECREF(y);
	Py_XDECREF(m);
	return Py_ReturnNone();
}

PyMethodDef g_ScorerMethods[] = {
	{ "CreateStatusScorer", Scorer_CreateStatusScorer, 1, NULL },
	{ "CreateCopMeterScorer", Scorer_CreateCopMeterScorer, 1, NULL },
	{ "CreateMonitorScorer", Scorer_CreateMonitorScorer, 1, NULL },
	{ "CreateMsgScorer", Scorer_CreateMsgScorer, 1, NULL },
	{ "CreateMissionScorer", (PyCFunction)(void (*)(void))Scorer_CreateMissionScorer, 3, NULL },
	{ "ShowMissionScorer", Scorer_ShowMissionScorer, 1, NULL },
	{ "CreateRadarScorer", Scorer_CreateRadarScorer, 1, NULL },
	{ "Blink", Scorer_Blink, 1, NULL },
	{ "MissionScorerBar_SetDefaults", Scorer_MissionScorerBar_SetDefaults, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the functions of CharScorer ---- */

static PyObject *CharScorer_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_ReturnNone();
}

static PyObject *CharScorer_CreateRadarScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Hud_Run(id, "charradar", Py_BuildValue("{s:i}", "outdoor", Script_GetInt("Init", "Outdoor", 0) != 0));
	return Py_ReturnNone();
}

/* module.func(args): the result, a new reference (NULL with the error printed) */
static PyObject *Hud_Ask(const char *module, const char *func, PyObject *args) {
	PyObject *m = PyImport_ImportModule((char *)module);
	PyObject *f = m ? PyObject_GetAttrString(m, (char *)func) : NULL;
	PyObject *r = f && args ? PyEval_CallObject(f, args) : NULL;
	if (!r) {
		PyErr_Print();
	}
	Py_XDECREF(args);
	Py_XDECREF(f);
	Py_XDECREF(m);
	return r;
}

/* the entity the player controls, or the one the camera follows; NULL when there is none (a new reference) */
static PyObject *Hud_Controlled(int id) {
	PyObject *u = Hud_Ask("Scrap", "UsrEntity", Py_BuildValue("(i)", id));
	if (u && u != Py_None) {
		return u;
	}
	Py_XDECREF(u);
	PyObject *cam = Hud_Ask("Scrap", "GetCam", Py_BuildValue("(i)", id));
	u = cam ? Hud_Ask("Scrap", "GetEntity", Py_BuildValue("(O)", cam)) : NULL;
	Py_XDECREF(cam);
	if (!u || u == Py_None) {
		/* the script failed here on None.IsType */
		Py_XDECREF(u);
		return NULL;
	}
	PyObject *r = PyObject_CallMethod(u, "IsType", "(s)", "FCam");
	byte fcam = r && PyObject_IsTrue(r) > 0;
	Py_XDECREF(r);
	PyErr_Clear();
	if (!fcam) {
		Py_DECREF(u);
		return NULL;
	}
	PyObject *target = PyObject_GetAttrString(u, "Target");
	Py_DECREF(u);
	u = target ? Hud_Ask("Scrap", "GetEntity", Py_BuildValue("(O)", target)) : NULL;
	Py_XDECREF(target);
	if (!u || u == Py_None) {
		/* the script asked None for its MainTarget here and failed */
		Py_XDECREF(u);
		return NULL;
	}
	return u;
}

static void Hud_SpecialAction(int id) {
	PyObject *u = Hud_Controlled(id);
	if (!u) {
		PyErr_Clear();
		return;
	}
	PyObject *act = PyObject_GetAttrString(u, "ActCtrl");
	PyErr_Clear();
	Hud_Run(id, "specialaction", Py_BuildValue("{s:s}", "act", act && PyString_Check(act) ? PyString_AsString(act) : ""));
	Py_XDECREF(act);
	Py_DECREF(u);
}

static PyObject *CharScorer_CreateSpecialActionScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Hud_SpecialAction(id);
	return Py_ReturnNone();
}

/* Create(id): the HUD on foot */
static PyObject *CharScorer_Create(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_Clear(sc);
	}
	Script_Call("SScorer", "SetCinema", "(iid)", id, 0, 0.25);
	Script_Call("Scorer", "CreateMsgScorer", "(i)", id);
	Script_Call("Scorer", "CreateStatusScorer", "(ii)", id, 1);
	Script_Call("Scorer", "CreateCopMeterScorer", "(i)", id);
	Script_Call("CharScorer", "CreateRadarScorer", "(i)", id);
	Script_Call("Scorer", "CreateMonitorScorer", "(ii)", id, 1);
	if (!Script_GetInt("Init", "Outdoor", 0) && !Script_GetInt("Init", "isJunkYard", 0)) {
		Script_Call("CharScorer", "CreateSpecialActionScorer", "(i)", id);
	}
	PyObject *menu = PyImport_ImportModule("Menu");
	PyObject *call = menu ? PyObject_GetAttrString(menu, "MissionScorerCall") : NULL;
	if (call && PyObject_IsTrue(call) > 0) {
		PyObject *r = PyEval_CallObject(call, Py_BuildValue("(i)", id));
		if (!r) {
			PyErr_Print();
		}
		Py_XDECREF(r);
	}
	PyErr_Clear();
	Py_XDECREF(call);
	Py_XDECREF(menu);
	return Py_ReturnNone();
}

PyMethodDef g_CharScorerMethods[] = {
	{ "Init", CharScorer_Init, 1, NULL },
	{ "CreateRadarScorer", CharScorer_CreateRadarScorer, 1, NULL },
	{ "CreateSpecialActionScorer", CharScorer_CreateSpecialActionScorer, 1, NULL },
	{ "Create", CharScorer_Create, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
