/*
 * The character scripts of the game in C and data (issue #2, step 6c). The Init of a character, which builds its
 * action class, is a .def file in data/chars/ (a pack in mods/ with scrapc/chars/<name>.def replaces it); the
 * parts that compute or are called back by the engine are here: steps of the files and the callbacks of each
 * character module. The module code of a script (file names and other values) stays in the script.
 *
 * Native C (docs/objlib-plan.md, stage 2): the steps and the missile events of Gear read and set the current action
 * in C on C values. The $names of the files (OBJLIB 6), Init.Path and the table of Speech the speech step fills
 * (OBJLIB 5, 6: Speech calls its functions through their entries) stay Python.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

static const char betty_def[] = {
#embed "../../../data/chars/betty.def"
	, 0 };

static const char bankdirector_def[] = {
#embed "../../../data/chars/bankdirector.def"
	, 0 };

static const char boss_def[] = {
#embed "../../../data/chars/boss.def"
	, 0 };

static const char crazygambler_def[] = {
#embed "../../../data/chars/crazygambler.def"
	, 0 };

static const char humphrey_def[] = {
#embed "../../../data/chars/humphrey.def"
	, 0 };

static const char maintenance_def[] = {
#embed "../../../data/chars/maintenance.def"
	, 0 };

static const char mayor_def[] = {
#embed "../../../data/chars/mayor.def"
	, 0 };

static const char rusty_def[] = {
#embed "../../../data/chars/rusty.def"
	, 0 };

static const char sebastian_def[] = {
#embed "../../../data/chars/sebastian.def"
	, 0 };

static const char sentinel_def[] = {
#embed "../../../data/chars/sentinel.def"
	, 0 };

static const char sputnikjunkyard_def[] = {
#embed "../../../data/chars/sputnikjunkyard.def"
	, 0 };

static const char bishop_def[] = {
#embed "../../../data/chars/bishop.def"
	, 0 };

static const char functionary_def[] = {
#embed "../../../data/chars/functionary.def"
	, 0 };

static const char gear_def[] = {
#embed "../../../data/chars/gear.def"
	, 0 };

static const char bankmaster_def[] = {
#embed "../../../data/chars/bankmaster.def"
	, 0 };

static const char berto_def[] = {
#embed "../../../data/chars/berto.def"
	, 0 };

static const char mercenarya_def[] = {
#embed "../../../data/chars/mercenarya.def"
	, 0 };

static const char nurse_def[] = {
#embed "../../../data/chars/nurse.def"
	, 0 };

static const char desktop_def[] = {
#embed "../../../data/chars/desktop.def"
	, 0 };

static const char police_def[] = {
#embed "../../../data/chars/police.def"
	, 0 };

static const char gatekeeper_def[] = {
#embed "../../../data/chars/gatekeeper.def"
	, 0 };

static const char dtritus_def[] = {
#embed "../../../data/chars/dtritus.def"
	, 0 };

static const char sputnik_def[] = {
#embed "../../../data/chars/sputnik.def"
	, 0 };

static const char policeboss_def[] = {
#embed "../../../data/chars/policeboss.def"
	, 0 };

static const char messenger_def[] = {
#embed "../../../data/chars/messenger.def"
	, 0 };

static const char human_def[] = {
#embed "../../../data/chars/human.def"
	, 0 };

/* ---- the steps of the files ---- */

/* a number of SAct's current action as SAct.GetAct gives it (None when it is not there or no number) */
static void Char_ActVar(const char *var, nativeVal_t *out) {
	char *s;
	double d = 0.0;
	int i = 0;
	switch (g_pAuxAction ? cAction_Var(g_pAuxAction, var, 0, &s, &d, &i) : ACTVAR_NONE) {
	case ACTVAR_DOUBLE:
		Native_SetFloat(out, d);
		return;
	case ACTVAR_INT:
		Native_SetInt(out, i);
		return;
	}
	Native_SetNone(out);
}

/* the time of a frame of SAct's current action (0 when it is not there) */
static double Char_Frame(int frame) {
	char var[0x20];
	nativeVal_t v;
	Str_Printf(var, sizeof(var), "TimeOfFrame%d", frame);
	Char_ActVar(var, &v);
	return v.type == NV_FLOAT ? v.f : v.type == NV_INT ? (double)v.i : 0.0;
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

/* speech name module func: Speech.SpeechRoutines[name] = module.func (an error is left set: the loader prints it, as
 * it does for every step, or stops a file that stops where the script stops) */
static void Step_Speech(int id, PyObject *args) {
	char *name, *module, *func;
	(void)id;
	if (!PyArg_ParseTuple(args, "sss", &name, &module, &func)) {
		return;
	}
	/* OBJLIB 5, 6: a function of a module named at run time (the import) into the table of Speech, a dict variable */
	PyObject *speech = Script_Module("Speech");
	PyObject *routines = speech ? PyObject_GetAttrString(speech, "SpeechRoutines") : NULL;
	PyObject *m = routines ? Script_Module(module) : NULL;
	PyObject *f = m ? PyObject_GetAttrString(m, func) : NULL;
	PyObject *key = f ? PyString_FromString(name) : NULL;
	if (key) {
		PyObject_SetItem(routines, key, f);
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
	nativeVal_t t;
	nativeBuf_t r;
	Char_ActVar("AnmTotalTime", &t);
	Native_BufInit(&r);
	Native_Repr(&r, &t, NR_REPR);
	Str_Printf(n, sizeof(n), "%s%s", name, r.p);
	Native_BufFree(&r);
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
	/* OBJLIB 5: Init.Path is a variable of Init; OBJLIB 6: the .def runner takes its $names as a dict */
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
	Script_RunFile(0, file, name, builtin, char_steps, vars);
	Py_XDECREF(vars);
}

static void Char_Init(const char *name, const char *file, const char *builtin) {
	Char_InitVars(name, file, builtin, NULL);
}

/* OnCreate(EntityName), OnFreeResources(): the section of the callback in the file of the character, $name the
 * entity */
static PyObject *Char_Callback(const char *file, const char *builtin, const char *section, PyObject *args) {
	char *name = "";
	if (!PyArg_ParseTuple(args, strcmp(section, "OnCreate") ? "" : "s", &name)) {
		return NULL;
	}
	PyObject *vars = Py_BuildValue("{s:s}", "name", name);
	Script_RunFile(0, file, section, builtin, char_steps, vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

#define CHAR_ONCREATE(mod, file) \
	static PyObject *mod##_OnCreate(PyObject *self, PyObject *args) { \
		(void)self; \
		return Char_Callback("chars/" #file ".def", file##_def, "OnCreate", args); \
	}

#define CHAR_ONFREE(mod, file) \
	static PyObject *mod##_OnFreeResources(PyObject *self, PyObject *args) { \
		(void)self; \
		return Char_Callback("chars/" #file ".def", file##_def, "OnFreeResources", args); \
	}

/* ---- Betty ---- */

static PyObject *Betty_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Char_Init("Betty", "chars/betty.def", betty_def);
	return Py_ReturnNone();
}

CHAR_ONCREATE(Betty, betty)
CHAR_ONFREE(Betty, betty)

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

/* ---- the other characters: Init, OnCreate and OnFreeResources from the file (as far as the script has them); their
 * other functions are still in their scripts ---- */

#define CHAR_INIT(mod, file) \
	static PyObject *mod##_Init(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		Char_Init(#mod, "chars/" #file ".def", file##_def); \
		return Py_ReturnNone(); \
	}

/* Init only (the OnCreate of GateKeeper, which makes an instance of the script's class per gatekeeper, is in
 * GateKeeper.c) */
#define CHAR_MODULE0(mod, file) \
	CHAR_INIT(mod, file) \
	PyMethodDef g_##mod##Methods[] = { { "Init", mod##_Init, 1, NULL }, { NULL, NULL, 0, NULL } };

/* Init and OnCreate */
#define CHAR_MODULE1(mod, file) \
	CHAR_INIT(mod, file) \
	CHAR_ONCREATE(mod, file) \
	PyMethodDef g_##mod##Methods[] = { { "Init", mod##_Init, 1, NULL }, { "OnCreate", mod##_OnCreate, 1, NULL }, \
		{ NULL, NULL, 0, NULL } };

/* Init, OnCreate and OnFreeResources */
#define CHAR_MODULE2(mod, file) \
	CHAR_INIT(mod, file) \
	CHAR_ONCREATE(mod, file) \
	CHAR_ONFREE(mod, file) \
	PyMethodDef g_##mod##Methods[] = { { "Init", mod##_Init, 1, NULL }, { "OnCreate", mod##_OnCreate, 1, NULL }, \
		{ "OnFreeResources", mod##_OnFreeResources, 1, NULL }, { NULL, NULL, 0, NULL } };

CHAR_MODULE2(BankDirector, bankdirector)
CHAR_MODULE1(Boss, boss)
CHAR_MODULE1(CrazyGambler, crazygambler)
CHAR_MODULE1(Humphrey, humphrey)
CHAR_MODULE1(Maintenance, maintenance)
CHAR_MODULE1(Mayor, mayor)
CHAR_MODULE2(Rusty, rusty)
CHAR_MODULE2(Sebastian, sebastian)
CHAR_MODULE1(Sentinel, sentinel)
CHAR_MODULE1(SputnikJunkyard, sputnikjunkyard)
CHAR_MODULE2(Bishop, bishop)
CHAR_MODULE2(Functionary, functionary)
CHAR_MODULE2(Gear, gear)
CHAR_MODULE2(BankMaster, bankmaster)
CHAR_MODULE2(Berto, berto)
CHAR_MODULE2(Nurse, nurse)
CHAR_MODULE2(Desktop, desktop)
CHAR_MODULE2(Police, police)
CHAR_MODULE0(GateKeeper, gatekeeper)
CHAR_MODULE2(Dtritus, dtritus)
CHAR_MODULE1(Sputnik, sputnik)
CHAR_MODULE2(PoliceBoss, policeboss)
CHAR_MODULE2(Messenger, messenger)
CHAR_MODULE1(Human, human)

/* Gear.LoadSirenActs(), Police.LoadSirenActs(): the siren actions on channel 1 of the current class, the section
 * LoadSirenActs of the file (the Init sections run it with do) */
static PyObject *Gear_LoadSirenActs(PyObject *self, PyObject *args) {
	(void)self;
	return Char_Callback("chars/gear.def", gear_def, "LoadSirenActs", args);
}

static PyObject *Police_LoadSirenActs(PyObject *self, PyObject *args) {
	(void)self;
	return Char_Callback("chars/police.def", police_def, "LoadSirenActs", args);
}

/* text + frame of the script into v (Native_Free it) */
static byte Gear_Cat(const char *text, const nativeVal_t *frame, nativeVal_t *v) {
	nativeVal_t t;
	Native_SetStr(&t, text);
	return Native_Arith('+', &t, frame, v);
}

/* the two events of DropMissileEvent: SAct.GetAct("TimeOfFrame" + frame), then SetAct of AddEventAtTime, EventName
 * (name + frame) and EventFunc */
static byte Gear_MissileEvents(const char *const what[2][2], const nativeVal_t *frame) {
	for (int i = 0; i < 2; i++) {
		nativeVal_t key, t, v;
		const char *k;
		if (!Gear_Cat("TimeOfFrame", &frame[i], &key)) {
			return 0;
		}
		byte ok = Native_ArgStr(&key, 1, &k);
		if (ok) {
			Native_ActVar(k, &t);
			Native_ActSet("AddEventAtTime", &t);
			Native_Free(&t);
		}
		Native_Free(&key);
		if (!ok || !Gear_Cat(what[i][0], &frame[i], &v)) {
			return 0;
		}
		Native_ActSet("EventName", &v);
		Native_Free(&v);
		Native_SetStr(&v, what[i][1]);
		Native_ActSet("EventFunc", &v);
	}
	return 1;
}

/* Gear.DropMissileEvent(LoadFrame, DropFrame): the events of loading and shooting the missile on the current action
 * at the two frames (texts, "TimeOfFrame" + LoadFrame like the script); gear.def writes its own two events */
static PyObject *Gear_DropMissileEvent(PyObject *self, PyObject *args) {
	static const char *const what[2][2] = { { "LoadMissile", "!GearShootLoad" }, { "DropMissile", "!GearShoot" } };
	PyObject *o[2];
	nativeVal_t frame[2];
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &o[0], &o[1])) {
		return NULL;
	}
	for (int i = 0; i < 2; i++) {
		/* OBJLIB 7: a value without a native form is refused by the + below as None is */
		if (!Native_FromPyAny(o[i], &frame[i])) {
			Native_SetNone(&frame[i]);
		}
	}
	byte ok = Gear_MissileEvents(what, frame);
	Native_Free(&frame[0]);
	Native_Free(&frame[1]);
	return ok ? Py_ReturnNone() : NULL;
}

PyMethodDef g_GearDefFuncs[] = {
	{ "LoadSirenActs", Gear_LoadSirenActs, 1, NULL },
	{ "DropMissileEvent", Gear_DropMissileEvent, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_PoliceDefFuncs[] = {
	{ "LoadSirenActs", Police_LoadSirenActs, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the mercenaries: one file, the class and the dance from Init(MercName="MercenaryA", doDance=1) ---- */

static void Mercenary_Init(const char *merc, int dance) {
	PyObject *vars = Py_BuildValue("{s:s,s:i}", "merc", merc, "dance", dance);
	Char_InitVars("MercenaryA", "chars/mercenarya.def", mercenarya_def, vars);
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

CHAR_ONCREATE(MercenaryA, mercenarya)

PyMethodDef g_MercenaryAMethods[] = { { "Init", MercenaryA_Init, 1, NULL }, { "OnCreate", MercenaryA_OnCreate, 1, NULL },
	{ NULL, NULL, 0, NULL } };
MERC_INIT(MercenaryB, 1)
MERC_INIT(MercenaryC, 1)
MERC_INIT(MercenaryL, 0)
