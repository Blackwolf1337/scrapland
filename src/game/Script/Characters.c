/*
 * The character scripts of the game in C and data (issue #2, step 6c). The Init of a character, which builds its
 * action class, is a .menu file in data/chars/ (a pack in mods/ with scrapc/chars/<name>.menu replaces it); the
 * parts that compute or are called back by the engine are here: steps of the files and the callbacks of each
 * character module. The module code of a script (file names and other values) stays in the script.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/scrap.h"
#include "scrap/script.h"

static const char betty_menu[] = {
#embed "../../../data/chars/betty.menu"
	, 0 };

static const char bankdirector_menu[] = {
#embed "../../../data/chars/bankdirector.menu"
	, 0 };

static const char boss_menu[] = {
#embed "../../../data/chars/boss.menu"
	, 0 };

static const char crazygambler_menu[] = {
#embed "../../../data/chars/crazygambler.menu"
	, 0 };

static const char humphrey_menu[] = {
#embed "../../../data/chars/humphrey.menu"
	, 0 };

static const char maintenance_menu[] = {
#embed "../../../data/chars/maintenance.menu"
	, 0 };

static const char mayor_menu[] = {
#embed "../../../data/chars/mayor.menu"
	, 0 };

static const char rusty_menu[] = {
#embed "../../../data/chars/rusty.menu"
	, 0 };

static const char sebastian_menu[] = {
#embed "../../../data/chars/sebastian.menu"
	, 0 };

static const char sentinel_menu[] = {
#embed "../../../data/chars/sentinel.menu"
	, 0 };

static const char sputnikjunkyard_menu[] = {
#embed "../../../data/chars/sputnikjunkyard.menu"
	, 0 };

static const char bishop_menu[] = {
#embed "../../../data/chars/bishop.menu"
	, 0 };

static const char functionary_menu[] = {
#embed "../../../data/chars/functionary.menu"
	, 0 };

static const char gear_menu[] = {
#embed "../../../data/chars/gear.menu"
	, 0 };

static const char bankmaster_menu[] = {
#embed "../../../data/chars/bankmaster.menu"
	, 0 };

static const char berto_menu[] = {
#embed "../../../data/chars/berto.menu"
	, 0 };

static const char mercenarya_menu[] = {
#embed "../../../data/chars/mercenarya.menu"
	, 0 };

static const char nurse_menu[] = {
#embed "../../../data/chars/nurse.menu"
	, 0 };

static const char desktop_menu[] = {
#embed "../../../data/chars/desktop.menu"
	, 0 };

static const char police_menu[] = {
#embed "../../../data/chars/police.menu"
	, 0 };

static const char gatekeeper_menu[] = {
#embed "../../../data/chars/gatekeeper.menu"
	, 0 };

static const char dtritus_menu[] = {
#embed "../../../data/chars/dtritus.menu"
	, 0 };

static const char sputnik_menu[] = {
#embed "../../../data/chars/sputnik.menu"
	, 0 };

static const char policeboss_menu[] = {
#embed "../../../data/chars/policeboss.menu"
	, 0 };

static const char messenger_menu[] = {
#embed "../../../data/chars/messenger.menu"
	, 0 };

static const char human_menu[] = {
#embed "../../../data/chars/human.menu"
	, 0 };

/* ---- the steps of the files ---- */

/* a number of SAct's current action as SAct.GetAct gives it (a new reference, None when it is not there) */
static PyObject *Char_ActVar(const char *var) {
	char *s;
	double d = 0.0;
	int i = 0;
	switch (g_pAuxAction ? cAction_Var(g_pAuxAction, var, 0, &s, &d, &i) : ACTVAR_NONE) {
	case ACTVAR_DOUBLE:
		return PyFloat_FromDouble(d);
	case ACTVAR_INT:
		return PyInt_FromLong(i);
	}
	return Py_ReturnNone();
}

/* the time of a frame of SAct's current action (0 when it is not there) */
static double Char_Frame(int frame) {
	char var[0x20];
	Str_Printf(var, sizeof(var), "TimeOfFrame%d", frame);
	PyObject *v = Char_ActVar(var);
	double d = PyFloat_Check(v) ? PyFloat_AsDouble(v) : PyInt_Check(v) ? (double)PyInt_AsLong(v) : 0.0;
	Py_DECREF(v);
	return d;
}

/* an event of SAct's current action, as the script set it */
static void Char_Event(double t, const char *name, const char *func) {
	char *s;
	int i = (int)t;
	if (!g_pAuxAction) {
		return;
	}
	cAction_Var(g_pAuxAction, "AddEventAtTime", 1, NULL, &t, &i);
	s = (char *)name;
	cAction_Var(g_pAuxAction, "EventName", 1, &s, NULL, NULL);
	s = (char *)func;
	cAction_Var(g_pAuxAction, "EventFunc", 1, &s, NULL, NULL);
}

/* speech name module func: Speech.SpeechRoutines[name] = module.func */
static void Step_Speech(int id, PyObject *args) {
	char *name, *module, *func;
	(void)id;
	if (!PyArg_ParseTuple(args, "sss", &name, &module, &func)) {
		PyErr_Print();
		return;
	}
	PyObject *speech = PyImport_ImportModule("Speech");
	PyObject *routines = speech ? PyObject_GetAttrString(speech, "SpeechRoutines") : NULL;
	PyObject *m = PyImport_ImportModule(module);
	PyObject *f = m ? PyObject_GetAttrString(m, func) : NULL;
	PyObject *key = PyString_FromString(name);
	if (!routines || !f || !key || PyObject_SetItem(routines, key, f) < 0) {
		PyErr_Print();
	}
	Py_XDECREF(key);
	Py_XDECREF(f);
	Py_XDECREF(m);
	Py_XDECREF(routines);
	Py_XDECREF(speech);
}

/* count events between first and end as the scripts loop them: first + (end - first) * i / count */
static void Char_Spread(double first, double end, int count, const char *name, const char *func) {
	double last = end - first;
	for (int i = 0; i < count; i++) {
		char n[0x40];
		if (!name) {
			Str_Printf(n, sizeof(n), "%d", i + 1);
		}
		Char_Event(last * i / count + first, name ? name : n, func);
	}
}

/* spread first end count name func */
static void Step_Spread(int id, PyObject *args) {
	double first, end;
	int count;
	char *name, *func;
	(void)id;
	if (!PyArg_ParseTuple(args, "ddiss", &first, &end, &count, &name, &func)) {
		PyErr_Print();
		return;
	}
	Char_Spread(first, end, count, name, func);
}

/* timedname name func: an event at 0 named name + `SAct.GetAct("AnmTotalTime")` (Sputnik) */
static void Step_TimedName(int id, PyObject *args) {
	char *name, *func, n[0x80];
	(void)id;
	if (!PyArg_ParseTuple(args, "ss", &name, &func)) {
		PyErr_Print();
		return;
	}
	PyObject *t = Char_ActVar("AnmTotalTime");
	PyObject *r = t ? PyObject_Repr(t) : NULL;
	Str_Printf(n, sizeof(n), "%s%s", name, r ? PyString_AsString(r) : "");
	Py_XDECREF(r);
	Py_XDECREF(t);
	Char_Event(0.0, n, func);
}

/* Betty's attack: the camera turns (15 steps) and the hits (16) between frames 43 and 83 */
static void Step_BettyHit(int id, PyObject *args) {
	static const char *const rotations[15] = { "24,   0", "48,  -6", "72, -14", "96, -20", "120,-25", "144,-30",
		"168,-35", "192,-40", "216,-35", "240,-30", "264,-25", "288,-20", "312,-14", "336, -6", "360,  0" };
	(void)id;
	(void)args;
	double hit = Char_Frame(43), end = Char_Frame(83), last = end - hit;
	for (int i = 0; i < 15; i++) {
		Char_Event(hit + last * i / 16, rotations[i], "!BettyCameraSet");
	}
	Char_Spread(hit, end, 16, NULL, "!BettyHitEvent");
}

static const scriptStep_t char_steps[] = {
	{ "speech", Step_Speech },
	{ "spread", Step_Spread },
	{ "timedname", Step_TimedName },
	{ "bettyhit", Step_BettyHit },
	{ NULL, NULL },
};

/* the Init of a character: its file, the values it may use (more in vars when it is not NULL) */
static void Char_InitVars(const char *name, const char *file, const char *builtin, PyObject *vars) {
	char level[0x100];
	Script_GetString("Init", "Path", level, sizeof(level));
	for (char *p = level; *p; p++) {
		if (*p >= 'a' && *p <= 'z') {
			*p -= 'a' - 'A';
		}
	}
	if (vars) {
		Py_INCREF(vars);
	} else {
		vars = PyDict_New();
	}
	PyObject *xbox = PyInt_FromLong(GVar_GetNum("isXbox", 0) != 0), *lv = PyString_FromString(level);
	if (vars && xbox && lv) {
		PyDict_SetItemString(vars, "xbox", xbox);
		PyDict_SetItemString(vars, "level", lv);
	}
	Py_XDECREF(xbox);
	Py_XDECREF(lv);
	Script_RunMenu(0, file, name, builtin, char_steps, vars);
	Py_XDECREF(vars);
}

static void Char_Init(const char *name, const char *file, const char *builtin) {
	Char_InitVars(name, file, builtin, NULL);
}

/* ---- Betty ---- */

static PyObject *Betty_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Char_Init("Betty", "chars/betty.menu", betty_menu);
	return Py_ReturnNone();
}

static PyObject *Betty_OnCreate(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Script_CallKw("CharAct", "OnCreate", Py_BuildValue("(O)", name), NULL);
	return Py_ReturnNone();
}

/* the effects of the attack and the kill are freed with her resources */
static PyObject *Betty_OnFreeResources(PyObject *self, PyObject *args) {
	char attack[0x100], kill[0x100];
	(void)self;
	(void)args;
	int fxAttack = Script_GetInt("Fx", "FXBETTYATTACK", 0), fxKill = Script_GetInt("Fx", "FXBETTYKILL", 0);
	Script_GetString("Betty", "FXBettyAttackFile", attack, sizeof(attack));
	Script_GetString("Betty", "FXBettyKillFile", kill, sizeof(kill));
	Script_Call("Scrap", "SetDebrisSys", "(siiiiiii)", attack, 0, 0, 0, 1, 0, 0, fxAttack);
	Script_Call("Scrap", "SetDebrisSys", "(siiiiiii)", kill, 0, 0, 0, 1, 0, 0, fxKill);
	return Py_ReturnNone();
}

/* the slow motion of her attack */
static PyObject *Char_Pitch(PyObject *args, double pitch) {
	PyObject *a, *b, *c;
	if (!PyArg_ParseTuple(args, "OOO", &a, &b, &c)) {
		return NULL;
	}
	GVar_SetNum("S_PitchScale", pitch);
	return Py_ReturnNone();
}

static PyObject *Betty_EnterSlowSpeed(PyObject *self, PyObject *args) {
	(void)self;
	return Char_Pitch(args, 0.5);
}

static PyObject *Betty_ExitSlowSpeed(PyObject *self, PyObject *args) {
	(void)self;
	return Char_Pitch(args, 1.0);
}

PyMethodDef g_BettyMethods[] = {
	{ "Init", Betty_Init, 1, NULL },
	{ "OnCreate", Betty_OnCreate, 1, NULL },
	{ "OnFreeResources", Betty_OnFreeResources, 1, NULL },
	{ "EnterSlowSpeed", Betty_EnterSlowSpeed, 1, NULL },
	{ "ExitSlowSpeed", Betty_ExitSlowSpeed, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the characters whose other functions are still in their scripts: only Init ---- */

#define CHAR_INIT(mod, file) \
	static PyObject *mod##_Init(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		Char_Init(#mod, "chars/" #file ".menu", file##_menu); \
		return Py_ReturnNone(); \
	} \
	PyMethodDef g_##mod##Methods[] = { { "Init", mod##_Init, 1, NULL }, { NULL, NULL, 0, NULL } };

CHAR_INIT(BankDirector, bankdirector)
CHAR_INIT(Boss, boss)
CHAR_INIT(CrazyGambler, crazygambler)
CHAR_INIT(Humphrey, humphrey)
CHAR_INIT(Maintenance, maintenance)
CHAR_INIT(Mayor, mayor)
CHAR_INIT(Rusty, rusty)
CHAR_INIT(Sebastian, sebastian)
CHAR_INIT(Sentinel, sentinel)
CHAR_INIT(SputnikJunkyard, sputnikjunkyard)
CHAR_INIT(Bishop, bishop)
CHAR_INIT(Functionary, functionary)
CHAR_INIT(Gear, gear)
CHAR_INIT(BankMaster, bankmaster)
CHAR_INIT(Berto, berto)
CHAR_INIT(Nurse, nurse)
CHAR_INIT(Desktop, desktop)
CHAR_INIT(Police, police)
CHAR_INIT(GateKeeper, gatekeeper)
CHAR_INIT(Dtritus, dtritus)
CHAR_INIT(Sputnik, sputnik)
CHAR_INIT(PoliceBoss, policeboss)
CHAR_INIT(Messenger, messenger)
CHAR_INIT(Human, human)

/* ---- the mercenaries: one file, the class and the dance from Init(MercName="MercenaryA", doDance=1) ---- */

static void Mercenary_Init(const char *merc, int dance) {
	PyObject *vars = Py_BuildValue("{s:s,s:i}", "merc", merc, "dance", dance);
	Char_InitVars("MercenaryA", "chars/mercenarya.menu", mercenarya_menu, vars);
	Py_XDECREF(vars);
}

static PyObject *MercenaryA_Init(PyObject *self, PyObject *args) {
	char *merc = "MercenaryA";
	int dance = 1;
	(void)self;
	if (!PyArg_ParseTuple(args, "|si", &merc, &dance)) {
		return NULL;
	}
	Mercenary_Init(merc, dance);
	return Py_ReturnNone();
}

#define MERC_INIT(mod, dance) \
	static PyObject *mod##_Init(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		Mercenary_Init(#mod, dance); \
		return Py_ReturnNone(); \
	} \
	PyMethodDef g_##mod##Methods[] = { { "Init", mod##_Init, 1, NULL }, { NULL, NULL, 0, NULL } };

PyMethodDef g_MercenaryAMethods[] = { { "Init", MercenaryA_Init, 1, NULL }, { NULL, NULL, 0, NULL } };
MERC_INIT(MercenaryB, 1)
MERC_INIT(MercenaryC, 1)
MERC_INIT(MercenaryL, 0)
