/*
 * The character scripts of the game in C and data (issue #2, step 6c). The Init of a character, which builds its
 * action class, is a .def file in data/chars/ (a pack in mods/ with scrapc/chars/<name>.def replaces it); the
 * parts that compute or are called back by the engine are here: steps of the files and the callbacks of each
 * character module. The module code of a script (file names and other values) stays in the script.
 *
 * Native C (docs/objlib-plan.md, stage 2): the steps and the missile events of Gear read and set the current action
 * in C on C values. The $names of the files (OBJLIB 6), Init.Path and the table of Speech the speech step fills
 * (OBJLIB 5 (9c), 6: Speech calls its functions through their entries) stay Python.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
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
static void Step_Speech(int id, const nativeVal_t *args, int n) {
	const char *name, *module, *func;
	(void)id;
	if (!Mode_Parse(args, n, "sss", &name, &module, &func)) {
		return;
	}
	/* OBJLIB 5 (9c), 7: a function of a module named at run time (held as NV_OBJ) into the table of Speech, a dict
	 * variable changed in place; the table is looked up before the function, as the script evaluates them */
	nativeVal_t routines, f = { 0 }, key;
	if (Mode_ModGet("Speech", "SpeechRoutines", &routines) && Mode_ModGet(module, func, &f)) {
		Native_SetText(&key, name, (int)strlen(name));
		Mode_ModDictPut("Speech", "SpeechRoutines", &key, &f);
		Native_Free(&key);
	}
	Native_Free(&f);
	Native_Free(&routines);
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
static void Step_Spread(int id, const nativeVal_t *args, int n) {
	double first, end;
	int count;
	const char *name, *func;
	(void)id;
	if (!Mode_Parse(args, n, "ddiss", &first, &end, &count, &name, &func)) {
		Mode_PrintError();
		return;
	}
	Char_Spread(first, end, count, name, func);
}

/* timedname name func: an event at 0 named name + `SAct.GetAct("AnmTotalTime")` (Sputnik) */
static void Step_TimedName(int id, const nativeVal_t *args, int n) {
	const char *name, *func;
	char event[0x80];
	(void)id;
	if (!Mode_Parse(args, n, "ss", &name, &func)) {
		Mode_PrintError();
		return;
	}
	nativeVal_t t;
	nativeBuf_t r;
	Char_ActVar("AnmTotalTime", &t);
	Native_BufInit(&r);
	Native_Repr(&r, &t, NR_REPR);
	Str_Printf(event, sizeof(event), "%s%s", name, r.p);
	Native_BufFree(&r);
	Char_Event(0.0, event, func);
}

/* Betty's attack: the camera turns (15 steps) and the hits (16) between frames 43 and 83 */
static void Step_BettyHit(int id, const nativeVal_t *args, int n) {
	static const char *const rotations[15] = { "24,   0", "48,  -6", "72, -14", "96, -20", "120,-25", "144,-30",
		"168,-35", "192,-40", "216,-35", "240,-30", "264,-25", "288,-20", "312,-14", "336, -6", "360,  0" };
	(void)id;
	(void)args;
	(void)n;
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

#define CHAR_OWN 2

/* the Init of a character: its file with the $names xbox and level, and before them the more pairs of a name and a
 * const nativeVal_t * (up to MODE_DEFVARS - CHAR_OWN; Mode_DefRunV, OBJLIB 6: the runner takes them as a dict) */
static void Char_InitVars(const char *name, const char *file, const char *builtin, int more, ...) {
	char level[0x100];
	const char *names[MODE_DEFVARS];
	const nativeVal_t *vals[MODE_DEFVARS];
	nativeVal_t xbox, lv;
	va_list va;
	/* Init.Path is a variable of Init */
	Script_GetString("Init", "Path", level, sizeof(level));
	for (char *p = level; *p; p++) {
		if (*p >= 'a' && *p <= 'z') {
			*p -= 'a' - 'A';
		}
	}
	CHECK(more <= MODE_DEFVARS - CHAR_OWN);
	va_start(va, more);
	for (int i = 0; i < more; i++) {
		names[i] = va_arg(va, const char *);
		vals[i] = va_arg(va, const nativeVal_t *);
	}
	va_end(va);
	Native_SetInt(&xbox, GVar_GetNum("isXbox", 0) != 0);
	Native_SetText(&lv, level, (int)strlen(level));
	names[more] = "xbox";
	vals[more] = &xbox;
	names[more + 1] = "level";
	vals[more + 1] = &lv;
	Mode_DefRunV(0, file, name, builtin, char_steps, 0, names, vals, more + CHAR_OWN);
	Native_Free(&lv);
}

static void Char_Init(const char *name, const char *file, const char *builtin) {
	Char_InitVars(name, file, builtin, 0);
}

/* OnCreate(EntityName), OnFreeResources() and LoadSirenActs(): the section of the callback in the file of the
 * character, $name the entity (the text "s" of OnCreate, "" for the others, which take no argument) */
static byte Char_Callback(const char *file, const char *builtin, const char *section, const nativeVal_t *a, int n) {
	const char *name = "";
	nativeVal_t v;
	if (!Mode_Parse(a, n, strcmp(section, "OnCreate") ? "" : "s", &name)) {
		return 0;
	}
	Native_SetText(&v, name, (int)strlen(name));
	Mode_DefRun(0, file, section, builtin, char_steps, 0, 1, "name", &v);
	Native_Free(&v);
	return 1;
}

#define CHAR_CALLBACK(mod, file, fn) \
	static byte mod##_##fn##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		(void)out; \
		return Char_Callback("chars/" #file ".def", file##_def, #fn, a, n); \
	} \
	MODE_ADAPTER(mod, fn)

#define CHAR_ONCREATE(mod, file) CHAR_CALLBACK(mod, file, OnCreate)
#define CHAR_ONFREE(mod, file) CHAR_CALLBACK(mod, file, OnFreeResources)

/* ---- Betty ---- */

static byte Betty_Init_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Char_Init("Betty", "chars/betty.def", betty_def);
	return 1;
}
MODE_ADAPTER(Betty, Init)

CHAR_ONCREATE(Betty, betty)
CHAR_ONFREE(Betty, betty)

/* the slow motion of her attack */
static byte Char_Pitch(const nativeVal_t *a, int n, double pitch) {
	if (n != 3) {
		return Port_Arity(n, 3);
	}
	(void)a;
	GVar_SetNum("S_PitchScale", pitch);
	return 1;
}

static byte Betty_EnterSlowSpeed_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Char_Pitch(a, n, 0.5);
}
MODE_ADAPTER(Betty, EnterSlowSpeed)

static byte Betty_ExitSlowSpeed_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Char_Pitch(a, n, 1.0);
}
MODE_ADAPTER(Betty, ExitSlowSpeed)

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

#define CHAR_INIT(mod, file) MODE_FNANY(mod, Init, (Char_Init(#mod, "chars/" #file ".def", file##_def), 1))

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
CHAR_CALLBACK(Gear, gear, LoadSirenActs)
CHAR_CALLBACK(Police, police, LoadSirenActs)

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
static byte Gear_DropMissileEvent_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	static const char *const what[2][2] = { { "LoadMissile", "!GearShootLoad" }, { "DropMissile", "!GearShoot" } };
	nativeVal_t frame[2];
	(void)out;
	if (n != 2) {
		return Port_Arity(n, 2);
	}
	for (int i = 0; i < 2; i++) {
		/* OBJLIB 7: a value without a native form (an object, an entity handle) is refused by the + below as None is */
		if (Native_IsObj(&a[i]) || a[i].type == NV_ENT) {
			Native_SetNone(&frame[i]);
		} else {
			Native_Copy(&frame[i], &a[i]);
		}
	}
	byte ok = Gear_MissileEvents(what, frame);
	Native_Free(&frame[0]);
	Native_Free(&frame[1]);
	return ok;
}
MODE_ADAPTER(Gear, DropMissileEvent)

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
	nativeVal_t m, d;
	Native_SetText(&m, merc, (int)strlen(merc));
	Native_SetInt(&d, dance);
	Char_InitVars("MercenaryA", "chars/mercenarya.def", mercenarya_def, 2, "merc", &m, "dance", &d);
	Native_Free(&m);
}

static byte MercenaryA_Init_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *merc = "MercenaryA";
	int dance = 1;
	(void)out;
	if (!Mode_Parse(a, n, "|si", &merc, &dance)) {
		return 0;
	}
	Mercenary_Init(merc, dance);
	return 1;
}
MODE_ADAPTER(MercenaryA, Init)

#define MERC_INIT(mod, dance) \
	MODE_FNANY(mod, Init, (Mercenary_Init(#mod, dance), 1)) \
	PyMethodDef g_##mod##Methods[] = { { "Init", mod##_Init, 1, NULL }, { NULL, NULL, 0, NULL } };

CHAR_ONCREATE(MercenaryA, mercenarya)

PyMethodDef g_MercenaryAMethods[] = { { "Init", MercenaryA_Init, 1, NULL }, { "OnCreate", MercenaryA_OnCreate, 1, NULL },
	{ NULL, NULL, 0, NULL } };
MERC_INIT(MercenaryB, 1)
MERC_INIT(MercenaryC, 1)
MERC_INIT(MercenaryL, 0)

PORTS(Characters)
