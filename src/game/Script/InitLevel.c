/*
 * The functions of the Init module in C (scripts/init.py, issue #2): the start of a level (Init: the map scripts of
 * the level, its mission libraries, the AI, the menus, the sound, the characters, vehicles, weapons, effects, doors,
 * the indoor or outdoor map, the save game values), the mission libraries of a map, the arrows, the station label and
 * the progress bar of the next level (Close). The module code (the preloads, the flags of the level such as Outdoor,
 * MultiPlayer, UpperMap, the tables) stays in the script; the map scripts change those flags while Init runs, so they
 * are read where the script reads them.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

/* `if <Init variable>:` (-1 with the error set) */
static int In_Flag(const char *name) {
	PyObject *v = Script_Global("Init", name);
	int t = v ? Script_True(v) : -1;
	Py_XDECREF(v);
	return t;
}

/* a function of this module through its global, as the script calls it */
static byte In_Self(const char *func, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *a = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	PyObject *f = Script_Global("Init", func);
	PyObject *res = Script_Invoke(f, a);
	Py_XDECREF(f);
	Py_XDECREF(res);
	return res != NULL;
}

/* import module; module.func(args) */
static byte In_Call(const char *module, const char *func, const char *fmt, PyObject *arg) {
	const char *const names[] = { module, NULL };
	return Script_Import(names) && (arg ? Script_Try(module, func, fmt, arg) : Script_Try(module, func, "()"));
}

/* Scrap.GetLevelPath()[7:] (a new reference) */
static PyObject *In_LevelName(const char *getter) {
	PyObject *p = Script_Ask("Scrap", getter, "()");
	PyObject *s = p ? PySequence_GetSlice(p, 7, PySequence_Length(p)) : NULL;
	Py_XDECREF(p);
	return s;
}

/* LoadCharLib(filename): the script of a character and its sound script */
static PyObject *Init_LoadCharLib(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *f = K(Script_PlusS(Script_SPlus("Chars/", name), ".pyc"));
	PyObject *snd = f && Script_Try("Scrap", "PreloadLibrary", "(OO)", name, f) ? K(Script_PlusS(Script_Ref(name), "Sound")) : NULL;
	PyObject *sf = snd ? K(Script_PlusS(Script_SPlus("Sound/", snd), ".pyc")) : NULL;
	if (!sf || !Script_Try("Scrap", "PreloadLibrary", "(OO)", snd, sf)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* AddMLib(library): a library of the map (MapLibs) */
static PyObject *Init_AddMLib(PyObject *self, PyObject *args) {
	PyObject *lib;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &lib)) {
		return NULL;
	}
	PyObject *libs = Script_Global("Init", "MapLibs");
	PyObject *v = libs ? Script_Method(libs, "append", "(O)", lib) : NULL;
	Py_XDECREF(libs);
	Py_XDECREF(v);
	return v ? Py_ReturnNone() : NULL;
}

/* MLibInitStatus(LevelPath), MLibInit(LevelPath): the same function of every library of the map */
static PyObject *In_ForLibs(PyObject *args, const char *func) {
	scriptRefs_t r = { .n = 0 };
	PyObject *path;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	PyObject *libs = K(Script_Global("Init", "MapLibs")), *lib;
	int st = libs ? 1 : -1;
	for (int i = 0; libs && (st = Script_Next(libs, i, &lib)) > 0; i++) {
		PyObject *v = Script_Method(lib, func, "(O)", path);
		Py_DECREF(lib);
		if (!v) {
			FAIL();
		}
		Py_DECREF(v);
	}
	return st < 0 ? Script_Done(&r, NULL) : Script_Done(&r, Py_ReturnNone());
}

static PyObject *Init_MLibInitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return In_ForLibs(args, "InitStatus");
}

static PyObject *Init_MLibInit(PyObject *self, PyObject *args) {
	(void)self;
	return In_ForLibs(args, "Init");
}

/* isMission(): the level is the map of the current mission */
static int In_IsMission(void) {
	scriptRefs_t r = { .n = 0 };
	PyObject *mm = Script_Import((const char *const[]){ "string", NULL }) ? K(Script_SaveVar("Mission.Map")) : NULL;
	PyObject *a = mm ? K(Script_Ask("string", "upper", "(O)", mm)) : NULL;
	PyObject *lv = a ? K(In_LevelName("GetLevelPath")) : NULL, *b = lv ? K(Script_Ask("string", "upper", "(O)", lv)) : NULL;
	int eq = b ? Script_Eq(a, b) : -1;
	Script_Done(&r, NULL);
	return eq;
}

static PyObject *Init_isMission(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	int m = In_IsMission();
	return m < 0 ? NULL : PyInt_FromLong(m);
}

/* PreloadMlibs(LevelPath): the libraries of the map script (Map.Libraries, with the mission's in its map) */
static PyObject *Init_PreloadMlibs(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Script_Import((const char *const[]){ "Map", NULL })) {
		return NULL;
	}
	PyObject *map = K(PyImport_ImportModule("Map"));
	int m = map ? In_IsMission() : -1;
	if (m < 0) {
		FAIL();
	}
	if (m) {
		PyObject *libs = K(PyObject_GetAttrString(map, "Libraries")), *lib = libs ? K(Script_SaveVar("Mission.Library")) : NULL;
		if (!lib || !K(Script_Method(libs, "append", "(O)", lib))) {
			FAIL();
		}
	}
	PyObject *names = K(Script_Ask("__builtin__", "dir", "(O)", map)), *key = names ? K(PyString_FromString("Libraries")) : NULL;
	int has = key ? PySequence_Contains(names, key) : -1;
	if (has < 0) {
		FAIL();
	}
	if (has) {
		PyObject *libs = K(PyObject_GetAttrString(map, "Libraries")), *name;
		int st = libs ? 1 : -1;
		for (int i = 0; libs && (st = Script_Next(libs, i, &name)) > 0; i++) {
			scriptRefs_t t = { .n = 0 };
			Script_Keep(&t, name);
			PyObject *f = Script_Keep(&t, Script_PlusS(Script_Ref(name), ".pyc"));
			PyObject *mod = f && Script_Try("Scrap", "PreloadLibrary", "(OO)", name, f) ?
				Script_Keep(&t, Script_Ask("__builtin__", "__import__", "(O)", name)) : NULL;
			byte ok = mod && In_Self("AddMLib", "(O)", mod);
			Script_Done(&t, NULL);
			if (!ok) {
				FAIL();
			}
		}
		if (st < 0) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SetArrowSize(): the models and animations of the action and mission arrows */
static PyObject *Init_SetArrowSize(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *arrows = K(Script_Global("Init", "Arrows")), *nam;
	int st = arrows ? 1 : -1;
	for (int i = 0; arrows && (st = Script_Next(arrows, i, &nam)) > 0; i++) {
		scriptRefs_t t = { .n = 0 };
		Script_Keep(&t, nam);
		PyObject *m3d = Script_Keep(&t, Script_PlusS(Script_Ref(nam), ".M3D"));
		PyObject *anm = m3d ? Script_Keep(&t, Script_PlusS(Script_Ref(nam), ".anim.M3D")) : NULL;
		byte ok = anm && Script_Try("Scrap", "Preload3DObject", "(Oiii)", m3d, 1, 1, 1) && Script_Try("Scrap", "PreloadAnm", "(OO)", m3d, anm);
		Script_Done(&t, NULL);
		if (!ok) {
			FAIL();
		}
	}
	return st < 0 ? Script_Done(&r, NULL) : Script_Done(&r, Py_ReturnNone());
}

/* the police boss' ship is dead after the prologue and the first murder: SPoli6 instead in the traffic */
static byte In_BossShip(void) {
	scriptRefs_t r = { .n = 0 };
	PyObject *ga = K(Script_SaveVar("GameAct"));
	if (!ga) {
		return Script_Done(&r, NULL) != NULL;
	}
	byte ok = 1;
	if (!Script_Is(ga, "Prologue") && !Script_Is(ga, "1stMurder")) {
		PyObject *cad = Script_Import((const char *const[]){ "string", NULL }) ? K(Script_SaveVar("Traffic.AcShips")) : NULL;
		int has = cad ? Script_True(cad) : -1;
		if (has > 0) {
			PyObject *at = K(Script_Ask("string", "find", "(Os)", cad, "SPoliBoss1"));
			has = at ? Script_NumCmp(at, -1) != 0 : -1;
		}
		ok = has >= 0;
		if (has > 0) {
			PyObject *rep = Script_Import((const char *const[]){ "Traffic", NULL }) ?
				K(Script_Ask("string", "replace", "(Ossi)", cad, "SPoliBoss1", "SPoli6", 1)) : NULL;
			PyObject *mod = rep && Script_Try("Scrap", "SetSaveVar", "(sO)", "Traffic.AcShips", rep) ? K(PyImport_ImportModule("Init")) : NULL;
			PyObject *ships = mod ? K(Script_Ask("__builtin__", "eval", "(OO)", rep, PyModule_GetDict(mod))) : NULL;
			ok = ships && Script_SetGlobal("Traffic", "AcShips", Script_Ref(ships));
		}
	}
	Script_Done(&r, NULL);
	return ok;
}

/* the part of Init that a map menu does not have: the characters, vehicles, weapons, effects, doors, the map and the
 * save game values */
static byte In_Game(PyObject *path) {
	int outdoor = In_Flag("Outdoor");
	if (outdoor < 0 || (!outdoor && !In_Call("Bureaucracy", "Init", "()", NULL))) {
		return 0;
	}
	PyObject *upper = Script_Global("Init", "UpperMap"), *deals = upper ? Script_Global("Init", "SUPERDEALSMAPS") : NULL;
	int dealMap = deals ? PySequence_Contains(deals, upper) : -1;
	Py_XDECREF(upper);
	Py_XDECREF(deals);
	int multi = dealMap > 0 ? 0 : In_Flag("MultiPlayer");
	if (dealMap < 0 || multi < 0 || ((dealMap > 0 || multi) && !Script_Try("Scrap", "Set", "(si)", "isActionMusic", 1))) {
		return 0;
	}
	int edit = In_Flag("isShipEdit"), multi2 = edit == 0 ? In_Flag("MultiPlayer") : 0;
	if (edit < 0 || multi2 < 0) {
		return 0;
	}
	if (!edit && !multi2) {
		int junk = In_Call("Chars", "Init", "()", NULL) ? In_Flag("isJunkYard") : -1;
		int out = junk == 0 ? In_Flag("Outdoor") : 0;
		if (junk < 0 || out < 0 || (out && (!In_Call("PhoneCab", "CreatePhoneCabClass", "()", NULL) ||
			!Script_Try("PhoneCab", "CreateAudioComClass", "()")))) {
			return 0;
		}
	}
	int out = In_Flag("Outdoor"), junk = out == 0 ? In_Flag("isJunkYard") : 0;
	if (out < 0 || junk < 0 || ((out || junk) && (!In_Call("Vehicles", "Init", "()", NULL) || !In_Call("Weapons", "Init", "()", NULL)))) {
		return 0;
	}
	if (!In_Call("Fx", "Init", "()", NULL)) {
		return 0;
	}
	edit = In_Flag("isShipEdit");
	if (edit < 0 || (!edit && (!In_Call("Alarm", "Init", "()", NULL) || !In_Call("MisItems", "Init", "()", NULL))) ||
		!In_Call("Doors", "Init", "()", NULL)) {
		return 0;
	}
	edit = In_Flag("isShipEdit");
	if (edit < 0) {
		return 0;
	}
	if (!edit) {
		out = In_Flag("Outdoor");
		if (out < 0 || !(out ? In_Call("OutMap", "Init", "(O)", path) : In_Call("InMap", "Init", "(O)", path))) {
			return 0;
		}
		multi = In_Flag("MultiPlayer");
		if (multi < 0 || (!multi && (!In_Call("SaveGame", "InitVars", "()", NULL) || !In_Call("MissionsFuncs", "Init", "()", NULL)))) {
			return 0;
		}
	}
	return 1;
}

/* the robots of the boss: the mercenaries help Dtritus; the alarm off in the maps whose com sat is done */
static byte In_BossRobots(void) {
	scriptRefs_t r = { .n = 0 };
	PyObject *robots = K(Script_SaveVar("RobotsControlledByBoss"));
	if (!robots) {
		return Script_Done(&r, NULL) != NULL;
	}
	if (!Script_Is(robots, "1")) {
		Script_Done(&r, NULL);
		return 1;
	}
	PyObject *help = K(Script_SaveVar("MercsHelpDtritus"));
	if (!help || (Script_Is(help, "1") && (!Script_Try("Scrap", "SetSaveVar", "(ss)", "MercFriends.MercenaryA_Smartie", "1") ||
		!Script_Try("Scrap", "SetSaveVar", "(ss)", "MercFriends.MercenaryC_Brutus", "1") ||
		!Script_Try("Scrap", "SetSaveVar", "(ss)", "MercFriends.MercenaryB_Dumber", "1")))) {
		return Script_Done(&r, NULL) != NULL;
	}
	PyObject *upper = K(Script_Global("Init", "UpperMap")), *done = upper ? K(Script_SaveVar("ComSatsMissionsMapsFinished")) : NULL;
	PyObject *mod = done ? K(PyImport_ImportModule("Init")) : NULL;
	PyObject *list = mod ? K(Script_Ask("__builtin__", "eval", "(OO)", done, PyModule_GetDict(mod))) : NULL;
	int in = list ? PySequence_Contains(list, upper) : -1;
	byte ok = in >= 0 && (in ? Script_Try("Scrap", "Set", "(sd)", "AlarmFallDelta", -0.05) && Script_Try("Scrap", "SetAlarm", "(i)", 0) :
		Script_Try("Scrap", "SetAlarm", "(i)", 1) && Script_Try("Scrap", "Set", "(si)", "AlarmFallDelta", 0));
	Script_Done(&r, NULL);
	return ok;
}

/* Init(LevelPath): the start of a level */
static PyObject *Init_Init(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Script_SetGlobal("Init", "Path", Script_Ref(path)) ||
		!Script_Import((const char *const[]){ "sys", NULL })) {
		return NULL;
	}
	/* the map scripts of the level and its libraries */
	PyObject *sp = K(Script_Global("sys", "path")), *dir = sp ? K(Script_PlusS(Script_Ref(path), "/Scripts")) : NULL;
	if (!dir || !K(Script_Method(sp, "append", "(O)", dir)) || !Script_Try("Scrap", "OpenPack", "(O)", dir) ||
		!Script_Try("Scrap", "PreloadLibrary", "(ss)", "Map", "Map.pyc") ||
		!Script_Try("Scrap", "PreloadLibrary", "(ss)", "MapSnd", "MapSnd.pyc") || !In_Self("PreloadMlibs", "(O)", path) ||
		!Script_Try("Scrap", "ClosePack", "()")) {
		FAIL();
	}
	PyObject *xbox = K(Script_Ask("Scrap", "Get", "(s)", "IsXbox"));
	int x = xbox ? Script_True(xbox) : -1;
	if (x > 0) {
		PyObject *credits = K(Script_Ask("Scrap", "Get", "(s)", "ShowCredits"));
		int c = credits ? Script_True(credits) : -1;
		PyObject *upper = c == 0 ? K(Script_Global("Init", "UpperMap")) : NULL;
		x = c < 0 ? -1 : c ? 0 : upper ? Script_In(upper, (const char *const[]){ "SHIPEDIT", "MENU", NULL }) : -1;
		if (x > 0) {
			PyObject *menu = Script_Try("Scrap", "PreloadLibrary", "(ss)", "XboxMenu", "Scripts/Scorer/XboxMenu.pyc") ?
				K(Script_Ask("__builtin__", "__import__", "(s)", "XboxMenu")) : NULL;
			if (!menu || !In_Self("AddMLib", "(O)", menu)) {
				FAIL();
			}
		}
	}
	if (x < 0 || !Script_Import((const char *const[]){ "Map", "MapSnd", NULL }) || !Script_Try("Map", "InitStatus", "(O)", path) ||
		!In_Self("MLibInitStatus", "(O)", path) || !In_BossShip()) {
		FAIL();
	}
	PyObject *deal = K(Script_Global("Init", "SuperDealModule"));
	int d = deal ? Script_True(deal) : -1;
	if (d < 0 || (d && !K(Script_Method(deal, "InitStatus", "(O)", path)))) {
		FAIL();
	}
	/* the size of the mission arrows, the AI */
	int menu = In_Flag("inMainMenu"), edit = menu == 0 ? In_Flag("isShipEdit") : 0;
	PyObject *out = menu >= 0 && edit >= 0 && (menu || edit || In_Self("SetArrowSize", "()")) ? K(Script_Global("Init", "Outdoor")) : NULL;
	if (!out || !Script_Try("SAI", "IniAI", "(OO)", path, out)) {
		FAIL();
	}
	/* the menus, the mission info, the chat, the sound, the decoration */
	PyObject *flags = Script_Try("Scrap", "Set", "(ss)", "EscapeEvent", "Menu.Init") && In_Call("Menu", "Initialize", "()", NULL) ?
		K(Script_Ask("Scrap", "GetNetFlags", "()")) : NULL;
	PyObject *off = flags ? K(Py_BuildValue("(iii)", 0, 0, 0)) : NULL;
	int single = off ? Script_Eq(flags, off) : -1;
	if (single < 0 || !Script_Try("Scrap", "Set", "(ss)", "MainMissionEvent", single ? "MissionsFuncs.ShowMissionInfo" : "Ffa.ShowHelp")) {
		FAIL();
	}
	int offline = In_Flag("OffLineGame");
	if (offline < 0 || (!offline && !In_Call("Chat", "SetChatProc", "()", NULL)) || !In_Call("Sound", "Init", "()", NULL) ||
		!Script_Try("Scrap", "CreateElements", "()")) {
		FAIL();
	}
	menu = In_Flag("inMainMenu");
	if (menu < 0 || (!menu && !In_Game(path))) {
		FAIL();
	}
	/* the Bertos hunting the player outdoors */
	int outdoor = In_Flag("Outdoor");
	if (outdoor < 0) {
		FAIL();
	}
	if (outdoor) {
		PyObject *bertos = K(Script_SaveVar("CreateBertos")), *upper = bertos ? K(Script_Global("Init", "UpperMap")) : NULL;
		int has = upper ? In_Flag("HasSuperDeal") : -1;
		if (has < 0) {
			FAIL();
		}
		if (Script_Is(bertos, "1") && !Script_Is(upper, "GDB") && !has) {
			PyObject *f = Script_Import((const char *const[]){ "MissionsFuncs", "Parking", NULL }) &&
				Script_Try("MissionsFuncs", "CreateBertos", "()") ? K(Script_Global("MissionsFuncs", "ReassignBertosPlayerTarget")) : NULL;
			if (!f || !Script_SetGlobal("Parking", "SwitchVehicleMissionCallbackAux", Script_Ref(f)) ||
				!Script_SetGlobal("Parking", "SwitchCharMissionCallbackAux", Script_Ref(f))) {
				FAIL();
			}
		}
	}
	if (!In_BossRobots() || !Script_Try("Scrap", "Set", "(ss)", "OnCrazyDealFinished", "MissionsFuncs.OnCrazyDealFinishedMessage") ||
		!Script_Try("Scrap", "Set", "(ss)", "OnCrazyDealTarget", "MissionsFuncs.OnCrazyDealTarget")) {
		FAIL();
	}
	/* the map */
	if (!Script_Try("Map", "Init", "(O)", path) || !Script_Try("MapSnd", "Init", "(O)", path) || !In_Self("MLibInit", "(O)", path)) {
		FAIL();
	}
	deal = K(Script_Global("Init", "SuperDealModule"));
	d = deal ? Script_True(deal) : -1;
	if (d < 0 || (d && !K(Script_Method(deal, "Init", "(O)", path))) ||
		!Script_LaterIn(0.5, "Init", "SetMapLabelText", PyTuple_New(0), NULL)) {
		FAIL();
	}
	int demo = In_Flag("isDemo");
	PyObject *lives = demo > 0 ? K(Script_SaveVar("Player.NumLives")) : NULL;
	if (demo < 0 || (demo && (!lives || (Script_Is(lives, "-1") && !Script_Try("Scrap", "SetSaveVar", "(ss)", "Player.NumLives", "0")))) ||
		!Script_Try("Scrap", "Set", "(si)", "R_SceneBlurValue", 0)) {
		FAIL();
	}
	/* the skill of a single player game */
	offline = In_Flag("OffLineGame");
	menu = offline > 0 ? In_Flag("inMainMenu") : 0;
	edit = offline > 0 && menu == 0 ? In_Flag("isShipEdit") : 0;
	if (offline < 0 || menu < 0 || edit < 0) {
		FAIL();
	}
	if (offline && !menu && !edit) {
		PyObject *skill = Script_Try("Scrap", "CreateSaveVar", "(ss)", "GameSkill", "1") ? K(Script_SaveVar("GameSkill")) : NULL;
		PyObject *n = skill ? K(Script_Int(skill)) : NULL;
		if (!n || !Script_Try("Scrap", "Set", "(sO)", "GameSkill", n)) {
			FAIL();
		}
	}
	PyObject *items[2] = { K(PyString_FromString("Actual Level at : ")), path };
	if (!items[0] || !Script_Print(items, 2)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SetMapLabelText(): the name of the station on the screen, unless the mission scorer shows another map's mission */
static PyObject *Init_SetMapLabelText(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *name = Script_Import((const char *const[]){ "Scorer", NULL }) ? K(Script_Global("Scorer", "MissionScorerName")) : NULL;
	PyObject *vis = name ? K(Script_Ask("SScorer", "Get", "(iOs)", 0, name, "Visible")) : NULL;
	int v = vis ? Script_True(vis) : -1;
	if (v > 0) {
		v = Script_NumCmp(vis, 1) == 0;
	}
	if (v < 0) {
		FAIL();
	}
	if (!v) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *status = K(Script_Ask("SScorer", "Get", "(iOs)", 0, name, "Status0"));
	if (!status) {
		FAIL();
	}
	int show = Script_NumCmp(status, 1) != 0;
	if (!show) {
		PyObject *mm = K(Script_SaveVar("Mission.Map")), *lv = mm ? K(In_LevelName("GetLevelPath")) : NULL;
		show = lv ? Script_Eq(mm, lv) : -1;
	}
	if (show < 0) {
		FAIL();
	}
	if (show) {
		PyObject *lv = K(In_LevelName("GetLevelPath")), *key = lv ? K(Script_SPlus("Station_", lv)) : NULL;
		PyObject *text = key ? K(Script_LangOf(Script_Ref(key))) : NULL;
		if (!text || !Script_Try("SScorer", "SetLabelText", "(Od)", text, (double)Timer_GetTime(&g_pWorld->timer) + 10)) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* Close(): the progress bar of the next level (the files it loads, from the tables of the module) */
static PyObject *Init_Close(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *lv = K(In_LevelName("GetNewLevelPath")), *upper = lv ? K(Script_Ask("string", "upper", "(O)", lv)) : NULL;
	PyObject *line = upper ? K(Script_Mod(K(PyString_FromString("Next level: %s")), upper)) : NULL;
	if (!line || !Script_Print(&line, 1)) {
		FAIL();
	}
	PyObject *xbox = K(Script_Ask("Scrap", "Get", "(s)", "IsXbox"));
	PyObject *bars = xbox ? K(Script_Global("Init", Script_NumCmp(xbox, 1) == 0 ? "PROGRESSBAR_XBOX" : "PROGRESSBAR_PC")) : NULL;
	int has = bars ? PyMapping_HasKey(bars, upper) : -1;
	if (has < 0) {
		FAIL();
	}
	if (has) {
		PyObject *n = K(PyObject_GetItem(bars, upper));
		if (!n || !Script_Try("Scrap", "Set", "(sO)", "ProgressBar", n)) {
			FAIL();
		}
	} else {
		PyObject *msg = K(Script_Mod(K(PyString_FromString("ERROR: '%s' progress bar info not found \n")), upper));
		PyObject *def = msg && Script_Try("Scrap", "ConsoleError", "(O)", msg) ? K(Script_Ask("Scrap", "Def", "(s)", "ProgressBar")) : NULL;
		if (!def || !Script_Try("Scrap", "Set", "(sO)", "ProgressBar", def)) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

PyMethodDef g_InitMethods[] = {
	{ "LoadCharLib", Init_LoadCharLib, 1, NULL },
	{ "AddMLib", Init_AddMLib, 1, NULL },
	{ "MLibInitStatus", Init_MLibInitStatus, 1, NULL },
	{ "MLibInit", Init_MLibInit, 1, NULL },
	{ "isMission", Init_isMission, 1, NULL },
	{ "PreloadMlibs", Init_PreloadMlibs, 1, NULL },
	{ "SetArrowSize", Init_SetArrowSize, 1, NULL },
	{ "Init", Init_Init, 1, NULL },
	{ "SetMapLabelText", Init_SetMapLabelText, 1, NULL },
	{ "Close", Init_Close, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
