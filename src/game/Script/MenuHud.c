/*
 * The HUD in C (issue #2, step 5): the Scorer module (status, police meter, monitor, messages, mission scorer,
 * radar), the CharScorer module (the HUD on foot) and the RacerScorer module (the HUD in a ship, the scores of a
 * game, the scorer of race missions). The layouts are data/menus/hud.def and data/menus/racer.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters that
 * parse the arguments as before (the same TypeErrors) and take the objects (the scores, the values of the mission
 * scorer) as native values. The variables of other modules are read where the C read them (Init.isShipEdit, Outdoor,
 * isJunkYard, OffLineGame, Scorer.MarginH, MarginV, SplitScreen, MissionScorerBar_X and _Y, Menu.MissionScorerCall,
 * RacerScorer.SPECTATOR_POINTS and the texts of the scores), the variables of RacerScorer are written at once
 * (OBJLIB 5). The scorer functions and Menu are called through their refs (PORT_CALL: directly while the attribute
 * holds the port's function, else by name, a failure printed); the callback Menu.MissionScorerCall is called as the
 * value it holds (OBJLIB 5); SScorer.SetCinema, Scrap.GetCam, GetEntity and UsrEntity go through their seams
 * (OBJLIB 7); Blink and the ping update schedule themselves as Python functions (OBJLIB 4); the .def runner takes
 * Python values (OBJLIB 6).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/net.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/timer.h"

static const char hud_def[] = {
#embed "../../../data/menus/hud.def"
	, 0 };

static const char racer_def[] = {
#embed "../../../data/menus/racer.def"
	, 0 };

/* ---- values ---- */

/* a value as the C set it on the scorer item: a float as it is, else PyInt_AsLong of it (an int, a long int that fits,
 * an object's int; -1 for the rest, the error dropped) */
static double Hud_Num(const nativeVal_t *v) {
	switch (v->type) {
	case NV_INT:
		return (double)v->i;
	case NV_FLOAT:
		return v->f;
	case NV_LONG: {
		nativeVal_t i = { 0 };
		/* OBJLIB 9: long_int (an OverflowError for a long that does not fit) */
		double d = Native_ToInt(v, &i) && i.type == NV_INT ? (double)i.i : -1.0;
		Native_Free(&i);
		PyErr_Clear();
		return d;
	}
	case NV_OBJ: {
		/* OBJLIB 7: an object with its own int */
		long l = PyInt_AsLong(v->obj);
		PyErr_Clear();
		return (double)l;
	}
	}
	return -1.0;
}

/* repr of a value as text into out (the C text up to its first 0 byte) */
static const char *Hud_Repr(const nativeVal_t *v, char *out, int size) {
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_Repr(&b, v, NR_REPR);
	Str_Copy(out, b.p, size);
	Native_BufFree(&b);
	return out;
}

/* seq[i] as PySequence_GetItem gives it, a copy in *out (Native_Free it), lists and tuples held as one NV_OBJ made
 * native (Mode_Elems): 0 when it is not there (the error dropped) */
static byte Hud_Item(const nativeVal_t *seq, int i, nativeVal_t *out) {
	nativeVal_t tmp = { 0 };
	const nativeVal_t *e = seq ? Native_GetItem(seq, i, 0, &tmp) : NULL;
	Native_SetNone(out);
	if (e) {
		Native_Copy(out, e);
		Mode_Elems(out);
	}
	Native_Free(&tmp);
	/* OBJLIB 7: the IndexError or TypeError of an odd entry dropped */
	PyErr_Clear();
	return e != NULL;
}

/* lists and tuples held as one NV_OBJ (they hold handles) made native, depth levels down */
static void Hud_Deep(nativeVal_t *v, int depth) {
	Mode_Elems(v);
	if (depth > 1 && (v->type == NV_LIST || v->type == NV_TUPLE)) {
		for (int i = 0; i < v->n; i++) {
			Hud_Deep(&v->items[i], depth - 1);
		}
	}
}

/* ---- the steps ---- */

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

/* a section of a file with the margins of the screen and more values (OBJLIB 6: the $names as a dict given away, or
 * NULL) */
static void Hud_RunFile(int id, const char *file, const char *builtin, const char *section, PyObject *more) {
	int mh = Mode_ModGetI("Scorer", "MarginH", 0), mv = Mode_ModGetI("Scorer", "MarginV", 0);
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
	return Mode_ModGetI("Init", "isShipEdit", 0) || (g_pApp->flags & 7);
}

/* SScorer.SetCinema(id, 0, 0.25) through its seam, the failure printed as the call by name printed it */
static void Hud_NoCinema(int id) {
	Mode_Printed(Mode_SetCinema(id, 0, 0.25), "SScorer", "SetCinema");
}

/* ---- the cores of Scorer ---- */

/* ShowMissionScorer(id[, status[, value]]) (value NULL: 1): a status of the mission scorer on or off, it blinks */
static void Hud_ShowMission(int id, int status, const nativeVal_t *value) {
	char name[0x40], var[0x10];
	Str_Copy(name, GVar_GetStr("MissionScorerName"), sizeof(name));
	Script_SetNum(id, name, "Visible", 1);
	Script_SetNum(id, name, "BlinkTime", 15);
	Script_SetNum(id, name, "SolidTime", 5);
	Str_Printf(var, sizeof(var), "Status%d", status);
	Script_SetNum(id, name, var, !value ? 1 : Hud_Num(value));
	byte off = Script_HasItem(id, name);
	for (int i = 0; off && i < 3; i++) {
		Str_Printf(var, sizeof(var), "Status%d", i);
		off = Script_GetNum(id, name, var) == 0;
	}
	if (off) {
		Script_SetNum(id, "Mission", "Visible", 0);
	}
}

/* Blink(n[, item]): the item (the radar) blinks n times, an eighth of a second each */
static void Hud_Blink(int n, const char *item) {
	Script_SetNum(0, item, "Visible", ((1 + n) % 2 + 2) % 2);
	if (n > 0) {
		nativeVal_t a[2];
		Native_SetInt(&a[0], n - 1);
		Native_SetText(&a[1], item, (int)strlen(item));
		Mode_SchedAt(Mode_Time() + 0.125, "Scorer", "Blink", a, 2, NULL);
		Native_Free(&a[1]);
	}
}

/* MissionScorerBar_SetDefaults(id, name, (back colour), (main colour), (black colour)) */
static void Hud_BarDefaults(int id, const char *name, const int *c) {
	nativeVal_t x, y;
	if (!Mode_ModGet("Scorer", "MissionScorerBar_X", &x)) {
		PyErr_Clear();
	}
	if (!Mode_ModGet("Scorer", "MissionScorerBar_Y", &y)) {
		PyErr_Clear();
	}
	PyObject *px = Native_ToPy(&x), *py = px ? Native_ToPy(&y) : NULL;
	Native_Free(&x);
	Native_Free(&y);
	Hud_Run(id, "missionbar", py ? Py_BuildValue("{s:s,s:O,s:O,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i}",
		"name", name, "x", px, "y", py, "backr", c[0], "backg", c[1], "backb", c[2], "backa", c[3], "mainr", c[4],
		"maing", c[5], "mainb", c[6], "maina", c[7], "blackr", c[8], "blackg", c[9], "blackb", c[10], "blacka", c[11]) :
		NULL);
	Py_XDECREF(px);
	Py_XDECREF(py);
}

/* ---- the functions of Scorer (adapters) ---- */

/* CreateStatusScorer(id, char=0) */
static byte Scorer_CreateStatusScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id, ch = 0;
	(void)out;
	if (!Mode_Parse(a, n, "i|i", &id, &ch)) {
		return 0;
	}
	Hud_Run(id, "status", Py_BuildValue("{s:i,s:i,s:i}", "char", ch != 0, "multi", Hud_Multi(), "split",
		Mode_ModGetI("Scorer", "SplitScreen", 0) != 0));
	return 1;
}
MODE_ADAPTER(Scorer, CreateStatusScorer)

MODE_IFN(Scorer_CreateCopMeterScorer, 1, (Hud_Run(id, "copmeter", NULL), 1))

/* CreateMonitorScorer(id, char=0) */
static byte Scorer_CreateMonitorScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id, ch = 0;
	(void)out;
	if (!Mode_Parse(a, n, "i|i", &id, &ch)) {
		return 0;
	}
	Hud_Run(id, "monitor", Py_BuildValue("{s:i}", "char", ch != 0));
	return 1;
}
MODE_ADAPTER(Scorer, CreateMonitorScorer)

MODE_IFN(Scorer_CreateMsgScorer, 1, (Hud_Run(id, "msg", Py_BuildValue("{s:i,s:i}", "splitnow",
	(int)GVar_GetNum("SplitScreen", 0), "enabled", 1)), 1))

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

/* ShowMissionScorer(id, status=0, value) */
static byte Scorer_ShowMissionScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *value = NULL;
	int id, status = 0;
	(void)out;
	if (!Mode_Parse(a, n, "i|iO", &id, &status, &value)) {
		return 0;
	}
	Hud_ShowMission(id, status, value);
	return 1;
}

static PyObject *Scorer_ShowMissionScorer(PyObject *self, PyObject *args) {
	(void)self;
	return Mode_Entry(args, Scorer_ShowMissionScorer_E);
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

/* Blink(n, item="Radar") */
static byte Scorer_Blink_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *item = "Radar";
	int k;
	(void)out;
	if (!Mode_Parse(a, n, "i|s", &k, &item)) {
		return 0;
	}
	Hud_Blink(k, item);
	return 1;
}

static PyObject *Scorer_Blink(PyObject *self, PyObject *args) {
	(void)self;
	return Mode_Entry(args, Scorer_Blink_E);
}

/* MissionScorerBar_SetDefaults(id, name, (back rgba), (main rgba), (black rgba)) */
static byte Scorer_MissionScorerBar_SetDefaults_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	int id, c[12];
	(void)out;
	if (!Mode_Parse(a, n, "is(iiii)(iiii)(iiii)", &id, &name, &c[0], &c[1], &c[2], &c[3], &c[4], &c[5], &c[6], &c[7],
		&c[8], &c[9], &c[10], &c[11])) {
		return 0;
	}
	Hud_BarDefaults(id, name, c);
	return 1;
}

static PyObject *Scorer_MissionScorerBar_SetDefaults(PyObject *self, PyObject *args) {
	(void)self;
	return Mode_Entry(args, Scorer_MissionScorerBar_SetDefaults_E);
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

/* ---- the cores of CharScorer ---- */

/*
 * the entity the player controls, or the one the camera follows: 1 with *out (the handle value), 0 where the script
 * returns, -1 with the error set where it raises (None.IsType without a camera entity, None.MainTarget when the
 * camera follows nobody)
 */
static int Hud_Controlled(int id, nativeVal_t *out) {
	nativeVal_t u = { 0 }, x = { 0 }, none = { 0 };
	int r = -1, c;
	/* from Scorer import MarginH, MarginV */
	PyObject *mh = Mode_From("Scorer", "MarginH"), *mv = mh ? Mode_From("Scorer", "MarginV") : NULL;
	byte ok = mv != NULL;
	Py_XDECREF(mh);
	Py_XDECREF(mv);
	Native_SetNone(out);
	if (!ok || !Mode_UsrSeam(id, &u) || (c = Mode_Cmp(&u, "==", &none)) < 0) {
		goto done;
	}
	if (c) {
		if (!Mode_CamSeam(id, &x) || !Mode_EntitySeam(&x, &u)) {
			goto done;
		}
		if ((c = Mode_VIsType(&u, "FCam")) <= 0) {
			r = c;
			goto done;
		}
		Native_Free(&x);
		if (!Mode_VObj(&u, "Target", &x)) {
			goto done;
		}
		Native_Free(&u);
		if (!Mode_EntitySeam(&x, &u) || (c = Mode_Cmp(&u, "==", &none)) < 0) {
			goto done;
		}
		if (c) {
			/* the attribute of the None just found, as the C read it */
			Native_Free(&x);
			if (!Mode_VObj(&u, "MainTarget", &x)) {
				goto done;
			}
			Native_Free(&u);
			if (!Mode_EntitySeam(&x, &u)) {
				goto done;
			}
		}
	}
	c = Mode_Cmp(&u, "==", &none);
	if (c == 0) {
		*out = u;
		Native_SetNone(&u);
	}
	r = c == 0 ? 1 : c > 0 ? 0 : -1;
done:
	Native_Free(&u);
	Native_Free(&x);
	return r;
}

/* the HUD of a special action: 0 with the error set */
static byte Hud_SpecialAction(int id) {
	nativeVal_t u, act = { 0 };
	int c = Hud_Controlled(id, &u);
	if (c <= 0) {
		return c == 0;
	}
	if (!Mode_VObj(&u, "ActCtrl", &act)) {
		PyErr_Clear();
	}
	Hud_Run(id, "specialaction", Py_BuildValue("{s:s}", "act", act.type == NV_STR ? Native_Text(&act) : ""));
	Native_Free(&act);
	Native_Free(&u);
	return 1;
}

/* Create(id): the HUD on foot */
static void Hud_CharCreate(int id) {
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_Clear(sc);
	}
	Hud_NoCinema(id);
	MODE_PRINTED(Scorer, CreateMsgScorer, "i", (long)id);
	MODE_PRINTED(Scorer, CreateStatusScorer, "ii", (long)id, 1L);
	MODE_PRINTED(Scorer, CreateCopMeterScorer, "i", (long)id);
	MODE_PRINTED(CharScorer, CreateRadarScorer, "i", (long)id);
	MODE_PRINTED(Scorer, CreateMonitorScorer, "ii", (long)id, 1L);
	if (!Mode_ModGetI("Init", "Outdoor", 0) && !Mode_ModGetI("Init", "isJunkYard", 0)) {
		MODE_PRINTED(CharScorer, CreateSpecialActionScorer, "i", (long)id);
	}
	/* OBJLIB 5: the callback variable of Menu */
	Mode_CallVarIf("Menu", "MissionScorerCall", id);
}

/* ---- the functions of CharScorer (adapters) ---- */

MODE_IFN(CharScorer_CreateRadarScorer, 1, (Hud_Run(id, "charradar", Py_BuildValue("{s:i}", "outdoor",
	Mode_ModGetI("Init", "Outdoor", 0) != 0)), 1))
MODE_IFN(CharScorer_CreateSpecialActionScorer, 1, Hud_SpecialAction(id))

MODE_IFN(CharScorer_Create, 1, (Hud_CharCreate(id), 1))

PyMethodDef g_CharScorerMethods[] = {
	{ "Init", Mode_PyNone, 1, NULL },
	{ "CreateRadarScorer", CharScorer_CreateRadarScorer, 1, NULL },
	{ "CreateSpecialActionScorer", CharScorer_CreateSpecialActionScorer, 1, NULL },
	{ "Create", CharScorer_Create, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the cores of RacerScorer ---- */

/* `if v.attr:` of a value (the error of the attribute dropped) */
static byte Hud_True(const nativeVal_t *v, const char *attr) {
	nativeVal_t a = { 0 };
	byte t = Mode_VObj(v, attr, &a) && Native_True(&a);
	PyErr_Clear();
	Native_Free(&a);
	return t;
}

/* CreateRacer(id): the HUD of a ship, or of a spectator who follows another player */
static void Racer_CreateRacerCore(int id) {
	nativeVal_t camName, cam = { 0 };
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_Clear(sc);
	}
	Hud_NoCinema(id);
	byte net = (g_pApp->flags & 7) != 0;
	/* the errors of Scrap.GetCam and Scrap.GetEntity printed, as the C asked them */
	byte haveName = Mode_CamSeam(id, &camName);
	if (!haveName) {
		PyErr_Print();
	}
	byte haveCam = haveName && Mode_EntitySeam(&camName, &cam);
	if (haveName && !haveCam) {
		PyErr_Print();
	}
	byte camera = haveCam && Native_True(&cam);
	byte spectator = 0;
	if (camera) {
		nativeVal_t t = { 0 }, mt = { 0 };
		byte ht = Mode_VObj(&cam, "Target", &t);
		byte hmt = Mode_VObj(&cam, "MainTarget", &mt);
		spectator = ht && hmt && Native_Compare(&t, &mt) != 0;
		PyErr_Clear();
		Native_Free(&t);
		Native_Free(&mt);
	}
	Racer_Run(id, "racer", Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:s}", "net", net, "camera", camera, "spectator",
		spectator, "target", camera && Hud_True(&cam, "Target"), "showlocal", !spectator || !net, "split",
		Mode_ModGetI("Scorer", "SplitScreen", 0) != 0, "online", Net_IsClient() || Net_IsServer(), "descriptor",
		"\nDescriptor"));
	Native_Free(&cam);
	Native_Free(&camName);
	/* OBJLIB 5: the callback variable of Menu */
	Mode_CallVarIf("Menu", "MissionScorerCall", id);
}

/* the data of the scores while ShowScorer runs: (score, name, (r, g, b), (r, g, b, a), entity) per player */
static const nativeVal_t *racer_scores;

/* run rows: one row per player, at most 24, small font for more than 12 */
static void Step_Rows(int id, PyObject *args) {
	(void)args;
	const nativeVal_t *data = racer_scores;
	int n = 0;
	if (data && !Native_Len(data, &n)) {
		/* the TypeError of a value that is no sequence dropped: no rows */
		PyErr_Clear();
		n = 0;
	}
	byte small = n > 12;
	int border = small ? 2 : 3;
	int step = (small ? 9 : 18) + border * 2;
	byte showPing = !Mode_ModGetI("Init", "OffLineGame", 0);
	nativeVal_t spect;
	byte haveSpect = Mode_ModGet("RacerScorer", "SPECTATOR_POINTS", &spect);
	PyErr_Clear();
	for (int i = 0; i < n && i < 24; i++) {
		nativeVal_t info, f[5], c[7];
		char nm[0x20] = "", sc[0x40] = "", ping[0x40] = "", pl[0x80] = "";
		byte have[5];
		byte haveInfo = Hud_Item(data, i, &info);
		for (int k = 0; k < 5; k++) {
			have[k] = haveInfo && Hud_Item(&info, k, &f[k]);
			if (!have[k]) {
				Native_SetNone(&f[k]);
			}
		}
		if (have[1] && f[1].type == NV_STR) {
			Str_Copy(nm, Native_Text(&f[1]), 21);
		}
		if (haveSpect && have[0] && Native_Compare(&spect, &f[0]) == 0) {
			Str_Copy(sc, "SPECT", sizeof(sc));
		} else if (have[0]) {
			Hud_Repr(&f[0], sc, sizeof(sc));
		}
		PyErr_Clear();
		if (have[4] && f[4].type == NV_STR) {
			Str_Copy(pl, Native_Text(&f[4]), sizeof(pl));
		}
		if (showPing) {
			Str_Printf(ping, sizeof(ping), "%d ms", Net_GetPing(pl));
		}
		int y = 71 + border + border + i * step;
		for (int k = 0; k < 7; k++) {
			/* the colour (r, g, b) and the back (r, g, b, a), 0 for what is not there */
			if (!(k < 3 ? have[2] && Hud_Item(&f[2], k, &c[k]) : have[3] && Hud_Item(&f[3], k - 3, &c[k]))) {
				Native_SetInt(&c[k], 0);
			}
		}
		PyObject *pc[7];
		byte ok = 1;
		for (int k = 0; k < 7; k++) {
			pc[k] = ok ? Native_ToPy(&c[k]) : NULL;
			ok = pc[k] != NULL;
		}
		Racer_Run(id, "row", ok ? Py_BuildValue("{s:i,s:i,s:i,s:s,s:i,s:s,s:s,s:s,s:s,s:O,s:O,s:O,s:O,s:O,s:O,s:O}",
			"n", i, "y", y, "liney", y + step - 2, "font", small ? "Terminal8" : "HoratioSmall", "step", step, "name",
			nm, "score", sc, "ping", ping, "player", pl, "r", pc[0], "g", pc[1], "b", pc[2], "br", pc[3], "bg", pc[4],
			"bb", pc[5], "ba", pc[6]) : NULL);
		for (int k = 0; k < 7; k++) {
			Py_XDECREF(pc[k]);
			Native_Free(&c[k]);
		}
		for (int k = 0; k < 5; k++) {
			Native_Free(&f[k]);
		}
		Native_Free(&info);
	}
	Native_Free(&spect);
}

/* RacerScorer.MultiplayerUpdatePing again in a second */
static void Racer_SchedulePing(void) {
	Mode_SchedAt(Mode_Time() + 1.0, "RacerScorer", "MultiplayerUpdatePing", NULL, 0, NULL);
}

/* ShowScorer(id, data[, superDealResult[, ActionYes[, ActionNo]]]): the scores of the players; a super deal adds Yes
 * and No (1) or Continue (2) */
static void Racer_ShowScorerCore(int id, const nativeVal_t *data, int deal, const char *yes, const char *no) {
	char top[0x200], bottom[0x200];
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_Clear(sc);
	}
	if (deal > 0) {
		MODE_PRINTED(Menu, InitMenuSys, "i", 0L);
		MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	}
	Mode_ModGetS("RacerScorer", "ScorerBotMessage", bottom, sizeof(bottom));
	Mode_ModGetS("RacerScorer", "ScorerTopMessage", top, sizeof(top));
	const nativeVal_t *outer = racer_scores;
	racer_scores = data;
	Racer_Run(id, "scores", Py_BuildValue("{s:s,s:s,s:i,s:s,s:s}", "bottom", bottom, "top", top, "deal", deal, "yes",
		yes, "no", no));
	racer_scores = outer;
	Racer_SchedulePing();
}

/* MultiplayerUpdatePing(): the pings of the scores once a second while they are shown */
static void Racer_UpdatePing(void) {
	double now = Mode_Time();
	nativeVal_t last;
	double l = 0;
	if (Mode_ModGet("RacerScorer", "MultiplayerUpdatePing_LastTime", &last)) {
		l = last.type == NV_FLOAT ? last.f : last.type == NV_INT ? (double)last.i : 0;
	}
	PyErr_Clear();
	Native_Free(&last);
	if (now - l < 1) {
		return;
	}
	Native_SetFloat(&last, now);
	Mode_ModPut("RacerScorer", "MultiplayerUpdatePing_LastTime", &last);
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
}

/* UpdateMissionScorer(id, you, bad): the better value on top, the player's mark beside it */
static void Racer_UpdateMission(int id, const nativeVal_t *you, const nativeVal_t *bad) {
	char a[0x40], b[0x40];
	Mode_ModPut("RacerScorer", "missionYou", you);
	Mode_ModPut("RacerScorer", "missionBad", bad);
	byte ahead = Native_Compare(you, bad) >= 0;
	PyErr_Clear();
	Script_SetStr(id, "MissionBest", "Text", Hud_Repr(ahead ? you : bad, a, sizeof(a)));
	Script_SetStr(id, "MissionBad", "Text", Hud_Repr(ahead ? bad : you, b, sizeof(b)));
	Script_SetNum(id, "MissionYou", "Y", ahead ? 180 : 200);
	Script_SetNum(id, "MissionYou", "Blue", ahead ? 255 : 0);
	Script_SetNum(id, "MissionYou", "Red", ahead ? 0 : 255);
}

/* MissionScorer(id): the scorer of a race mission with the values of InitMissionScorer */
static void Racer_MissionScorerCore(int id) {
	nativeVal_t limit, you, bad;
	byte haveLimit = Mode_ModGet("RacerScorer", "missionLimit", &limit);
	byte haveYou = Mode_ModGet("RacerScorer", "missionYou", &you);
	byte haveBad = Mode_ModGet("RacerScorer", "missionBad", &bad);
	PyErr_Clear();
	if (!haveLimit) {
		Native_SetNone(&limit);
	}
	PyObject *pl = Native_ToPy(&limit);
	Racer_Run(id, "mission", pl ? Py_BuildValue("{s:O}", "limit", pl) : NULL);
	Py_XDECREF(pl);
	if (haveYou && haveBad) {
		/* through the module: a mod or a case may replace it */
		MODE_PRINTED(RacerScorer, UpdateMissionScorer, "ivv", (long)id, &you, &bad);
	}
	Native_Free(&bad);
	Native_Free(&you);
	Native_Free(&limit);
}

/* ---- the functions of RacerScorer (adapters) ---- */

#define RACER_SECTION(func, section) MODE_IFN(func, 1, (Racer_Run(id, section, NULL), 1))

RACER_SECTION(Racer_CreateWeaponScorer, "weapon")
RACER_SECTION(Racer_CreateCannonScorer, "cannon")
RACER_SECTION(Racer_CreateTargetScorer, "target")
RACER_SECTION(Racer_CreateWaypointScorer, "waypoint")
RACER_SECTION(Racer_CreateRadarScorer, "radar")

MODE_IFN(Racer_CreateRacer, 1, (Racer_CreateRacerCore(id), 1))

/* Create(id): both players in split screen */
static void Racer_CreateCore(int id) {
	if (Mode_ModGetI("Scorer", "SplitScreen", 0)) {
		MODE_PRINTED(RacerScorer, CreateRacer, "i", 0L);
		MODE_PRINTED(RacerScorer, CreateRacer, "i", 1L);
	} else {
		MODE_PRINTED(RacerScorer, CreateRacer, "i", (long)id);
	}
}

MODE_IFN(Racer_Create, 1, (Racer_CreateCore(id), 1))

/* ShowScorer(id, data, deal=0, yes="", no="") */
static byte Racer_ShowScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *data;
	const char *yes = "", *no = "";
	int id, deal = 0;
	nativeVal_t d;
	(void)out;
	if (!Mode_Parse(a, n, "iO|iss", &id, &data, &deal, &yes, &no)) {
		return 0;
	}
	/* the list, its rows and their colours native where they are held as one NV_OBJ (a row holds a handle) */
	Native_Copy(&d, data);
	Hud_Deep(&d, 3);
	Racer_ShowScorerCore(id, &d, deal, yes, no);
	Native_Free(&d);
	return 1;
}

static PyObject *Racer_ShowScorer(PyObject *self, PyObject *args) {
	(void)self;
	return Mode_Entry(args, Racer_ShowScorer_E);
}

static PyObject *Racer_MultiplayerUpdatePing(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Racer_UpdatePing();
	return Py_ReturnNone();
}

/* InitMissionScorer([you[, bad[, limit]]]) */
static byte Racer_InitMissionScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *o[3] = { NULL, NULL, NULL };
	static const char *const vars[3] = { "missionYou", "missionBad", "missionLimit" };
	(void)out;
	if (!Mode_Parse(a, n, "|OOO", &o[0], &o[1], &o[2])) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		nativeVal_t def;
		if (i < 2) {
			Native_SetInt(&def, 0);
		} else {
			Native_SetText(&def, "", 0);
		}
		Mode_ModPut("RacerScorer", vars[i], o[i] ? o[i] : &def);
	}
	return 1;
}

static PyObject *Racer_InitMissionScorer(PyObject *self, PyObject *args) {
	(void)self;
	return Mode_Entry(args, Racer_InitMissionScorer_E);
}

MODE_IFN(Racer_UpdateMissionScorer, 3, (Racer_UpdateMission(id, &a[1], &a[2]), 1))

MODE_IFN(Racer_MissionScorer, 1, (Racer_MissionScorerCore(id), 1))

RACER_SECTION(Racer_MissionScorerTeam, "missionteam")

PyMethodDef g_RacerScorerMethods[] = {
	{ "Init", Mode_PyNone, 1, NULL },
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

PORTS(MenuHud)
