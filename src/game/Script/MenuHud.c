/*
 * The HUD in C (issue #2, step 5): the Scorer module (status, police meter, monitor, messages, mission scorer,
 * radar), the CharScorer module (the HUD on foot) and the RacerScorer module (the HUD in a ship, the scores of a
 * game, the scorer of race missions). The layouts are data/menus/hud.def and data/menus/racer.def.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/net.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

static const char hud_def[] = {
#embed "../../../data/menus/hud.def"
	, 0 };

static const char racer_def[] = {
#embed "../../../data/menus/racer.def"
	, 0 };

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

static void Step_Rows(int id, PyObject *args);

static const scriptStep_t hud_steps[] = {
	{ "missionvisible", Step_MissionVisible },
	{ "rows", Step_Rows },
	{ NULL, NULL },
};

/* a section of a file with the margins of the screen and more values (a dict given away, or NULL) */
static void Hud_RunFile(int id, const char *file, const char *builtin, const char *section, PyObject *more) {
	int mh = Script_GetInt("Scorer", "MarginH", 0), mv = Script_GetInt("Scorer", "MarginV", 0);
	PyObject *vars = Py_BuildValue("{s:i,s:i,s:i,s:i}", "mh", mh, "mv", mv, "right", 640 - mh, "bottom", 480 - mv);
	PyObject *k, *v;
	int pos = 0;
	while (vars && more && PyDict_Next(more, &pos, &k, &v)) {
		PyDict_SetItem(vars, k, v);
	}
	Py_XDECREF(more);
	Script_RunFile(id, file, section, builtin, hud_steps, vars);
	Py_XDECREF(vars);
}

static void Hud_Run(int id, const char *section, PyObject *more) {
	Hud_RunFile(id, "menus/hud.def", hud_def, section, more);
}

static void Racer_Run(int id, const char *section, PyObject *more) {
	Hud_RunFile(id, "menus/racer.def", racer_def, section, more);
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
	PyObject *r = f && args ? Script_CallObject(f, args, NULL) : NULL;
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
		PyObject *r = Script_Invoke(call, Py_BuildValue("(i)", id));
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

/* ---- the functions of RacerScorer ---- */

#define RACER_SECTION(func, section) \
	static PyObject *func(PyObject *self, PyObject *args) { \
		int id; \
		(void)self; \
		if (!PyArg_ParseTuple(args, "i", &id)) { \
			return NULL; \
		} \
		Racer_Run(id, section, NULL); \
		return Py_ReturnNone(); \
	}

RACER_SECTION(Racer_CreateWeaponScorer, "weapon")
RACER_SECTION(Racer_CreateCannonScorer, "cannon")
RACER_SECTION(Racer_CreateTargetScorer, "target")
RACER_SECTION(Racer_CreateWaypointScorer, "waypoint")
RACER_SECTION(Racer_CreateRadarScorer, "radar")

static PyObject *Racer_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_ReturnNone();
}

/* a true attribute of an object (Python truth) */
static byte Hud_True(PyObject *o, const char *attr) {
	PyObject *v = o ? PyObject_GetAttrString(o, (char *)attr) : NULL;
	byte t = v && PyObject_IsTrue(v) > 0;
	PyErr_Clear();
	Py_XDECREF(v);
	return t;
}

/* CreateRacer(id): the HUD of a ship, or of a spectator who follows another player */
static PyObject *Racer_CreateRacer(PyObject *self, PyObject *args) {
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
	byte net = (g_pApp->flags & 7) != 0;
	PyObject *camName = Hud_Ask("Scrap", "GetCam", Py_BuildValue("(i)", id));
	PyObject *cam = camName ? Hud_Ask("Scrap", "GetEntity", Py_BuildValue("(O)", camName)) : NULL;
	byte camera = cam && PyObject_IsTrue(cam) > 0;
	byte spectator = 0;
	if (camera) {
		PyObject *t = PyObject_GetAttrString(cam, "Target"), *mt = PyObject_GetAttrString(cam, "MainTarget");
		spectator = t && mt && PyObject_Compare(t, mt) != 0;
		PyErr_Clear();
		Py_XDECREF(t);
		Py_XDECREF(mt);
	}
	Racer_Run(id, "racer", Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:s}", "net", net, "camera", camera, "spectator",
		spectator, "target", camera && Hud_True(cam, "Target"), "showlocal", !spectator || !net, "split",
		Script_GetInt("Scorer", "SplitScreen", 0) != 0, "online", Net_IsClient() || Net_IsServer(), "descriptor",
		"\nDescriptor"));
	Py_XDECREF(cam);
	Py_XDECREF(camName);
	PyObject *menu = PyImport_ImportModule("Menu");
	PyObject *call = menu ? PyObject_GetAttrString(menu, "MissionScorerCall") : NULL;
	if (call && PyObject_IsTrue(call) > 0) {
		PyObject *r = Script_Invoke(call, Py_BuildValue("(i)", id));
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

/* Create(id): both players in split screen */
static PyObject *Racer_Create(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	if (Script_GetInt("Scorer", "SplitScreen", 0)) {
		Script_Call("RacerScorer", "CreateRacer", "(i)", 0);
		Script_Call("RacerScorer", "CreateRacer", "(i)", 1);
	} else {
		Script_Call("RacerScorer", "CreateRacer", "(i)", id);
	}
	return Py_ReturnNone();
}

/* the data of the scores while ShowScorer runs: (score, name, (r, g, b), (r, g, b, a), entity) per player */
static PyObject *racer_scores;

/* repr of a value as text into out */
static const char *Hud_Repr(PyObject *v, char *out, int size) {
	PyObject *r = v ? PyObject_Repr(v) : NULL;
	Str_Copy(out, r ? PyString_AsString(r) : "", size);
	PyErr_Clear();
	Py_XDECREF(r);
	return out;
}

/* run rows: one row per player, at most 24, small font for more than 12 */
static void Step_Rows(int id, PyObject *args) {
	(void)args;
	PyObject *data = racer_scores;
	int n = data ? PySequence_Length(data) : 0;
	byte small = n > 12;
	int border = small ? 2 : 3;
	int step = (small ? 9 : 18) + border * 2;
	byte showPing = !Script_GetInt("Init", "OffLineGame", 0);
	PyObject *rm = PyImport_ImportModule("RacerScorer");
	PyObject *spect = rm ? PyObject_GetAttrString(rm, "SPECTATOR_POINTS") : NULL;
	PyErr_Clear();
	for (int i = 0; i < n && i < 24; i++) {
		PyObject *info = PySequence_GetItem(data, i);
		PyObject *score = info ? PySequence_GetItem(info, 0) : NULL;
		PyObject *name = info ? PySequence_GetItem(info, 1) : NULL;
		PyObject *col = info ? PySequence_GetItem(info, 2) : NULL;
		PyObject *back = info ? PySequence_GetItem(info, 3) : NULL;
		PyObject *player = info ? PySequence_GetItem(info, 4) : NULL;
		char nm[0x20] = "", sc[0x40], ping[0x40] = "", pl[0x80] = "";
		if (name && PyString_Check(name)) {
			Str_Copy(nm, PyString_AsString(name), 21);
		}
		if (spect && score && PyObject_Compare(spect, score) == 0) {
			Str_Copy(sc, "SPECT", sizeof(sc));
		} else {
			Hud_Repr(score, sc, sizeof(sc));
		}
		if (player && PyString_Check(player)) {
			Str_Copy(pl, PyString_AsString(player), sizeof(pl));
		}
		if (showPing) {
			Str_Printf(ping, sizeof(ping), "%d ms", Net_GetPing(pl));
		}
		int y = 71 + border + border + i * step;
		PyObject *c[7];
		for (int k = 0; k < 3; k++) {
			c[k] = col ? PySequence_GetItem(col, k) : NULL;
		}
		for (int k = 0; k < 4; k++) {
			c[3 + k] = back ? PySequence_GetItem(back, k) : NULL;
		}
		PyErr_Clear();
		for (int k = 0; k < 7; k++) {
			if (!c[k]) {
				c[k] = PyInt_FromLong(0);
			}
		}
		Racer_Run(id, "row", Py_BuildValue("{s:i,s:i,s:i,s:s,s:i,s:s,s:s,s:s,s:s,s:O,s:O,s:O,s:O,s:O,s:O,s:O}", "n", i,
			"y", y, "liney", y + step - 2, "font", small ? "Terminal8" : "HoratioSmall", "step", step, "name", nm, "score",
			sc, "ping", ping, "player", pl, "r", c[0], "g", c[1], "b", c[2], "br", c[3], "bg", c[4], "bb", c[5], "ba",
			c[6]));
		for (int k = 0; k < 7; k++) {
			Py_XDECREF(c[k]);
		}
		Py_XDECREF(player);
		Py_XDECREF(back);
		Py_XDECREF(col);
		Py_XDECREF(name);
		Py_XDECREF(score);
		Py_XDECREF(info);
	}
	Py_XDECREF(spect);
	Py_XDECREF(rm);
}

/* RacerScorer.MultiplayerUpdatePing again in a second */
static void Racer_SchedulePing(void) {
	PyObject *m = PyImport_ImportModule("RacerScorer");
	PyObject *f = m ? PyObject_GetAttrString(m, "MultiplayerUpdatePing") : NULL;
	PyObject *a = PyTuple_New(0);
	if (f && a) {
		Scheduler_Add(&g_pWorld->scheduler, Timer_GetTime(&g_pWorld->timer) + 1.0f, f, a, "");
	}
	PyErr_Clear();
	Py_XDECREF(a);
	Py_XDECREF(f);
	Py_XDECREF(m);
}

/* ShowScorer(id, data[, superDealResult[, ActionYes[, ActionNo]]]): the scores of the players; a super deal adds Yes
 * and No (1) or Continue (2) */
static PyObject *Racer_ShowScorer(PyObject *self, PyObject *args) {
	int id, deal = 0;
	PyObject *data;
	char *yes = "", *no = "", top[0x200], bottom[0x200];
	(void)self;
	if (!PyArg_ParseTuple(args, "iO|iss", &id, &data, &deal, &yes, &no)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_Clear(sc);
	}
	if (deal > 0) {
		Script_Call("Menu", "InitMenuSys", "(i)", 0);
		Script_Call("Menu", "StartNewMenu", "(i)", id);
	}
	Script_GetString("RacerScorer", "ScorerBotMessage", bottom, sizeof(bottom));
	Script_GetString("RacerScorer", "ScorerTopMessage", top, sizeof(top));
	PyObject *outer = racer_scores;
	racer_scores = data;
	Racer_Run(id, "scores", Py_BuildValue("{s:s,s:s,s:i,s:s,s:s}", "bottom", bottom, "top", top, "deal", deal, "yes",
		yes, "no", no));
	racer_scores = outer;
	Racer_SchedulePing();
	return Py_ReturnNone();
}

/* MultiplayerUpdatePing(): the pings of the scores once a second while they are shown */
static PyObject *Racer_MultiplayerUpdatePing(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	double now = Timer_GetTime(&g_pWorld->timer);
	PyObject *m = PyImport_ImportModule("RacerScorer");
	PyObject *last = m ? PyObject_GetAttrString(m, "MultiplayerUpdatePing_LastTime") : NULL;
	double l = last && PyFloat_Check(last) ? PyFloat_AsDouble(last) : last && PyInt_Check(last) ? PyInt_AsLong(last) : 0;
	PyErr_Clear();
	Py_XDECREF(last);
	Py_XDECREF(m);
	if (now - l < 1) {
		return Py_ReturnNone();
	}
	Script_SetVar("RacerScorer", "MultiplayerUpdatePing_LastTime", PyFloat_FromDouble(now));
	int count = 0;
	for (int i = 0; i < 64; i++) {
		char name[0x20], text[0x40], player[0x80];
		Str_Printf(name, sizeof(name), "Ping%d", i);
		if (!Script_HasItem(0, name)) {
			break;
		}
		count++;
		Str_Copy(player, Script_GetStr(0, name, "Hint"), sizeof(player));
		Str_Printf(text, sizeof(text), "%d ms", Net_GetPing(player));
		Script_SetStr(0, name, "Text", text);
	}
	if (count > 0) {
		Racer_SchedulePing();
	}
	return Py_ReturnNone();
}

/* InitMissionScorer([you[, bad[, limit]]]) */
static PyObject *Racer_InitMissionScorer(PyObject *self, PyObject *args) {
	PyObject *you = NULL, *bad = NULL, *limit = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "|OOO", &you, &bad, &limit)) {
		return NULL;
	}
	Script_SetVar("RacerScorer", "missionYou", you ? (Py_INCREF(you), you) : PyInt_FromLong(0));
	Script_SetVar("RacerScorer", "missionBad", bad ? (Py_INCREF(bad), bad) : PyInt_FromLong(0));
	Script_SetVar("RacerScorer", "missionLimit", limit ? (Py_INCREF(limit), limit) : PyString_FromString(""));
	return Py_ReturnNone();
}

/* UpdateMissionScorer(id, you, bad): the better value on top, the player's mark beside it */
static void Racer_UpdateMission(int id, PyObject *you, PyObject *bad) {
	char a[0x40], b[0x40];
	Py_INCREF(you);
	Script_SetVar("RacerScorer", "missionYou", you);
	Py_INCREF(bad);
	Script_SetVar("RacerScorer", "missionBad", bad);
	byte ahead = PyObject_Compare(you, bad) >= 0;
	PyErr_Clear();
	Script_SetStr(id, "MissionBest", "Text", Hud_Repr(ahead ? you : bad, a, sizeof(a)));
	Script_SetStr(id, "MissionBad", "Text", Hud_Repr(ahead ? bad : you, b, sizeof(b)));
	Script_SetNum(id, "MissionYou", "Y", ahead ? 180 : 200);
	Script_SetNum(id, "MissionYou", "Blue", ahead ? 255 : 0);
	Script_SetNum(id, "MissionYou", "Red", ahead ? 0 : 255);
}

static PyObject *Racer_UpdateMissionScorer(PyObject *self, PyObject *args) {
	int id;
	PyObject *you, *bad;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &you, &bad)) {
		return NULL;
	}
	Racer_UpdateMission(id, you, bad);
	return Py_ReturnNone();
}

static PyObject *Racer_MissionScorer(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	PyObject *m = PyImport_ImportModule("RacerScorer");
	PyObject *limit = m ? PyObject_GetAttrString(m, "missionLimit") : NULL;
	PyObject *you = m ? PyObject_GetAttrString(m, "missionYou") : NULL;
	PyObject *bad = m ? PyObject_GetAttrString(m, "missionBad") : NULL;
	PyErr_Clear();
	Racer_Run(id, "mission", Py_BuildValue("{s:O}", "limit", limit ? limit : Py_None));
	if (you && bad) {
		Script_CallKw("RacerScorer", "UpdateMissionScorer", Py_BuildValue("(iOO)", id, you, bad), NULL);
	}
	Py_XDECREF(bad);
	Py_XDECREF(you);
	Py_XDECREF(limit);
	Py_XDECREF(m);
	return Py_ReturnNone();
}

RACER_SECTION(Racer_MissionScorerTeam, "missionteam")

PyMethodDef g_RacerScorerMethods[] = {
	{ "Init", Racer_Init, 1, NULL },
	{ "CreateWeaponScorer", Racer_CreateWeaponScorer, 1, NULL },
	{ "CreateCannonScorer", Racer_CreateCannonScorer, 1, NULL },
	{ "CreateTargetScorer", Racer_CreateTargetScorer, 1, NULL },
	{ "CreateWaypointScorer", Racer_CreateWaypointScorer, 1, NULL },
	{ "CreateRadarScorer", Racer_CreateRadarScorer, 1, NULL },
	{ "Create", Racer_Create, 1, NULL },
	{ "CreateRacer", Racer_CreateRacer, 1, NULL },
	{ "ShowScorer", Racer_ShowScorer, 1, NULL },
	{ "MultiplayerUpdatePing", Racer_MultiplayerUpdatePing, 1, NULL },
	{ "InitMissionScorer", Racer_InitMissionScorer, 1, NULL },
	{ "MissionScorer", Racer_MissionScorer, 1, NULL },
	{ "MissionScorerTeam", Racer_MissionScorerTeam, 1, NULL },
	{ "UpdateMissionScorer", Racer_UpdateMissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
