/*
 * The functions of the Init module in C (scripts/init.py, issue #2): the start of a level (Init: the map scripts of
 * the level, its mission libraries, the AI, the menus, the sound, the characters, vehicles, weapons, effects, doors,
 * the indoor or outdoor map, the save game values), the mission libraries of a map, the arrows, the station label and
 * the progress bar of the next level (Close). The module code (the preloads, the flags of the level such as Outdoor,
 * MultiPlayer, UpperMap, the tables) is data/modules/init.mod; its two for loops are the C hooks at the end of this
 * file. The map scripts change the flags while Init runs, so they are read where the script reads them.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters (Init,
 * LoadCharLib, AddMLib, MLibInitStatus, MLibInit and PreloadMlibs parse their one argument, isMission, SetArrowSize,
 * SetMapLabelText and Close take any arguments, as before). The flags and tables of the module are read as its globals
 * where the script reads them (OBJLIB 5); Path is written at once and MapLibs changed in place (the map libraries are
 * module objects, OBJLIB 7, as are Init.SuperDealModule and the libraries of Map). The functions of the module the
 * script calls as its globals and the other ports go through their refs (PORT_OWN, PORT_CALL: the seam, OBJLIB 5).
 * The map modules Map and MapSnd, the super deal module and the map libraries are named at run time, their functions
 * called through the entries of the level's ports (MODE_CBATTR, PORT_VALUE);
 * string.find and replace are the native twins (OBJLIB 9); Scrap.PreloadLibrary, PreloadAnm, CreateElements,
 * CreateSaveVar, GetLevelPath, GetNewLevelPath and SAI.IniAI go through their refs (stage 7), dir and __import__ stay
 * by name (OBJLIB 6, 7). Scrap.Set,
 * OpenPack, ClosePack, Preload3DObject, GetNetFlags, SetAlarm, ConsoleError, AddScheduledFunc, SScorer.Get and
 * SetLabelText go through their seams (OBJLIB 7). The evals of the save texts read literals with the native reader
 * (Mode_EvalIn; a bare name is looked up in the module, OBJLIB 6), and the C hooks of the module code keep working on the module's dict (OBJLIB 6).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define IN "Init"

/* `if <Init variable>:` (-1 with the error set) */
static int In_Flag(const char *name) {
	return LvN_GTrue(IN, name);
}

/* import module; module.func() or module.func(arg), through the ref (each use its own site) */
#define IN_CALL(module, func) (Mode_Import(#module) && PORT_CALL(module, func, NULL, ""))
#define IN_CALLV(module, func, arg) (Mode_Import(#module) && PORT_CALL(module, func, NULL, "v", arg))

/* o.func(arg) of a module object, through the entry of the function there */
static byte In_Method(const nativeVal_t *o, const char *func, const nativeVal_t *arg) {
	PyObject *m = Mode_VBound(o, func);
	byte ok = m && PORT_VALUE(m, NULL, "v", arg);
	Py_XDECREF(m);
	return ok;
}

/* Scrap.GetNewLevelPath()[7:] (fresh 1) or Scrap.GetLevelPath()[7:]: the name of the level */
static byte In_LevelName(byte fresh, nativeVal_t *out) {
	nativeVal_t p = { 0 };
	Native_Free(out);
	byte ok = (fresh ? PORT_CALL(Scrap, GetNewLevelPath, &p, "") : PORT_CALL(Scrap, GetLevelPath, &p, "")) &&
		Native_Slice(&p, 7, 0x7fffffff, out);
	Native_Free(&p);
	return ok;
}

/* LoadCharLib(filename): the script of a character and its sound script */
static byte In_LoadCharLibN(const nativeVal_t *name) {
	nativeVal_t a = { 0 }, f = { 0 }, snd = { 0 }, b = { 0 }, sf = { 0 }, pyc, sound;
	LvN_Str(&pyc, ".pyc");
	LvN_Str(&sound, "Sound");
	byte ok = LvN_Cat("Chars/", name, &a) && Native_Arith('+', &a, &pyc, &f) &&
		PORT_CALL(Scrap, PreloadLibrary, NULL, "vv", name, &f) && Native_Arith('+', name, &sound, &snd) &&
		LvN_Cat("Sound/", &snd, &b) && Native_Arith('+', &b, &pyc, &sf) &&
		PORT_CALL(Scrap, PreloadLibrary, NULL, "vv", &snd, &sf);
	Native_Free(&a);
	Native_Free(&f);
	Native_Free(&snd);
	Native_Free(&b);
	Native_Free(&sf);
	return ok;
}

/* MLibInitStatus(LevelPath), MLibInit(LevelPath): the same function of every library of the map (MapLibs) */
static byte In_ForLibs(const nativeVal_t *path, const char *func) {
	nativeVal_t libs = { 0 }, lib = { 0 };
	int n = Mode_G(IN, "MapLibs", &libs) ? Mode_Loop(&libs) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		Native_Free(&lib);
		ok = Mode_Item(&libs, i, &lib) && In_Method(&lib, func, path);
	}
	Native_Free(&libs);
	Native_Free(&lib);
	return ok;
}

/* isMission(): the level is the map of the current mission (1, 0, -1 with the error) */
static int In_IsMission(void) {
	nativeVal_t mm = { 0 }, a = { 0 }, lv = { 0 }, b = { 0 };
	if (!Mode_Import("string")) {
		return -1;
	}
	Mode_SaveVar("Mission.Map", &mm);
	int eq = Native_Upper(&mm, &a) && In_LevelName(0, &lv) && Native_Upper(&lv, &b) ? Mode_Cmp(&a, "==", &b) : -1;
	Native_Free(&mm);
	Native_Free(&a);
	Native_Free(&lv);
	Native_Free(&b);
	return eq;
}

/* PreloadMlibs(LevelPath): the libraries of the map script (Map.Libraries, with the mission's in its map) */
static byte In_PreloadMlibsN(void) {
	nativeVal_t lib = { 0 }, names = { 0 }, key, libs = { 0 }, name = { 0 }, f = { 0 }, mod = { 0 }, pyc;
	if (!Mode_Import("Map")) {
		return 0;
	}
	/* OBJLIB 6: the module object itself for dir() */
	PyObject *map = Script_Module("Map");
	int m = map ? In_IsMission() : -1;
	byte ok = m >= 0;
	if (ok && m) {
		Mode_SaveVar("Mission.Library", &lib);
		ok = LvN_ModAppend("Map", "Libraries", &lib);
	}
	int has = -1;
	if (ok && Native_CallMod("__builtin__", "dir", &names, "O", map)) {
		LvN_Str(&key, "Libraries");
		has = Native_Contains(&names, &key);
	}
	ok = has >= 0;
	if (ok && has) {
		int n = Mode_ModGet("Map", "Libraries", &libs) ? Mode_Loop(&libs) : -1;
		LvN_Str(&pyc, ".pyc");
		ok = n >= 0;
		for (int i = 0; ok && i < n; i++) {
			Native_Free(&name);
			Native_Free(&f);
			Native_Free(&mod);
			ok = Mode_Item(&libs, i, &name) && Native_Arith('+', &name, &pyc, &f) &&
				PORT_CALL(Scrap, PreloadLibrary, NULL, "vv", &name, &f) &&
				Native_CallMod("__builtin__", "__import__", &mod, "v", &name) && PORT_OWN(Init, AddMLib, NULL, "v", &mod);
		}
	}
	Py_XDECREF(map);
	Native_Free(&lib);
	Native_Free(&names);
	Native_Free(&libs);
	Native_Free(&name);
	Native_Free(&f);
	Native_Free(&mod);
	return ok;
}

/* SetArrowSize(): the models and animations of the action and mission arrows */
static byte In_SetArrowSizeN(void) {
	nativeVal_t arrows = { 0 }, nam = { 0 }, m3d = { 0 }, anm = { 0 }, s1, s2;
	LvN_Str(&s1, ".M3D");
	LvN_Str(&s2, ".anim.M3D");
	int n = Mode_G(IN, "Arrows", &arrows) ? Mode_Loop(&arrows) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		Native_Free(&nam);
		Native_Free(&m3d);
		Native_Free(&anm);
		ok = Mode_Item(&arrows, i, &nam) && Native_Arith('+', &nam, &s1, &m3d) && Native_Arith('+', &nam, &s2, &anm) &&
			Mode_PreloadOnes(&m3d) && PORT_CALL(Scrap, PreloadAnm, NULL, "vv", &m3d, &anm);
	}
	Native_Free(&arrows);
	Native_Free(&nam);
	Native_Free(&m3d);
	Native_Free(&anm);
	return ok;
}

/* the police boss' ship is dead after the prologue and the first murder: SPoli6 instead in the traffic */
static byte In_BossShip(void) {
	nativeVal_t ga = { 0 }, cad = { 0 }, at = { 0 }, rep = { 0 }, ships = { 0 };
	Mode_SaveVar("GameAct", &ga);
	byte ok = 1;
	if (!Native_IsStr(&ga, "Prologue") && !Native_IsStr(&ga, "1stMurder")) {
		int has = -1;
		if (Mode_Import("string")) {
			Mode_SaveVar("Traffic.AcShips", &cad);
			has = Native_True(&cad);
		}
		if (has > 0) {
			has = PORT_CALL(string, find, &at, "vs", &cad, "SPoliBoss1") ? !Mode_EqI(&at, -1) : -1;
		}
		ok = has >= 0 && (!has || (Mode_Import("Traffic") &&
			PORT_CALL(string, replace, &rep, "vssi", &cad, "SPoliBoss1", "SPoli6", 1L) &&
			Mode_SetSaveVar("Traffic.AcShips", &rep) && Mode_Import(IN) && Mode_EvalIn(IN, &rep, &ships) &&
			Mode_SetModVar("Traffic", "AcShips", &ships)));
	}
	Native_Free(&ga);
	Native_Free(&cad);
	Native_Free(&at);
	Native_Free(&rep);
	Native_Free(&ships);
	return ok;
}

/* the part of Init that a map menu does not have: the characters, vehicles, weapons, effects, doors, the map and the
 * save game values */
static byte In_Game(const nativeVal_t *path) {
	int outdoor = In_Flag("Outdoor");
	if (outdoor < 0 || (!outdoor && !IN_CALL(Bureaucracy, Init))) {
		return 0;
	}
	nativeVal_t upper = { 0 }, deals = { 0 };
	int dealMap = Mode_G(IN, "UpperMap", &upper) && Mode_G(IN, "SUPERDEALSMAPS", &deals) ? Mode_In(&deals, &upper) : -1;
	Native_Free(&upper);
	Native_Free(&deals);
	int multi = dealMap ? 0 : In_Flag("MultiPlayer");
	if (dealMap < 0 || multi < 0 || ((dealMap > 0 || multi) && !Mode_GVarSetI("isActionMusic", 1))) {
		return 0;
	}
	int edit = In_Flag("isShipEdit"), multi2 = edit == 0 ? In_Flag("MultiPlayer") : 0;
	if (edit < 0 || multi2 < 0) {
		return 0;
	}
	if (!edit && !multi2) {
		int junk = IN_CALL(Chars, Init) ? In_Flag("isJunkYard") : -1;
		int out = junk == 0 ? In_Flag("Outdoor") : 0;
		if (junk < 0 || out < 0 || (out && (!IN_CALL(PhoneCab, CreatePhoneCabClass) ||
			!PORT_CALL(PhoneCab, CreateAudioComClass, NULL, "")))) {
			return 0;
		}
	}
	int out = In_Flag("Outdoor"), junk = out == 0 ? In_Flag("isJunkYard") : 0;
	if (out < 0 || junk < 0 || ((out || junk) && (!IN_CALL(Vehicles, Init) || !IN_CALL(Weapons, Init)))) {
		return 0;
	}
	if (!IN_CALL(Fx, Init)) {
		return 0;
	}
	edit = In_Flag("isShipEdit");
	if (edit < 0 || (!edit && (!IN_CALL(Alarm, Init) || !IN_CALL(MisItems, Init))) ||
		!IN_CALL(Doors, Init)) {
		return 0;
	}
	edit = In_Flag("isShipEdit");
	if (edit < 0) {
		return 0;
	}
	if (!edit) {
		out = In_Flag("Outdoor");
		if (out < 0 || !(out ? IN_CALLV(OutMap, Init, path) : IN_CALLV(InMap, Init, path))) {
			return 0;
		}
		multi = In_Flag("MultiPlayer");
		if (multi < 0 || (!multi && (!IN_CALL(SaveGame, InitVars) || !IN_CALL(MissionsFuncs, Init)))) {
			return 0;
		}
	}
	return 1;
}

/* the robots of the boss: the mercenaries help Dtritus; the alarm off in the maps whose com sat is done */
static byte In_BossRobots(void) {
	nativeVal_t robots = { 0 }, help = { 0 }, upper = { 0 }, done = { 0 }, list = { 0 }, lv;
	Mode_SaveVar("RobotsControlledByBoss", &robots);
	byte ok = 1;
	if (Native_IsStr(&robots, "1")) {
		Mode_SaveVar("MercsHelpDtritus", &help);
		if (Native_IsStr(&help, "1")) {
			static const char *const friends[] = { "MercFriends.MercenaryA_Smartie", "1", "MercFriends.MercenaryC_Brutus", "1",
				"MercFriends.MercenaryB_Dumber", "1", NULL };
			Mode_Saves(friends);
		}
		int in = -1;
		if (Mode_G(IN, "UpperMap", &upper)) {
			Mode_SaveVar("ComSatsMissionsMapsFinished", &done);
			in = Mode_Import(IN) && Mode_EvalIn(IN, &done, &list) ? Mode_In(&list, &upper) : -1;
		}
		Native_SetInt(&lv, !in);
		ok = in >= 0 && (in ? Mode_GVarSetN("AlarmFallDelta", -0.05) && Mode_SetAlarm(&lv) :
			Mode_SetAlarm(&lv) && Mode_GVarSetI("AlarmFallDelta", 0));
	}
	Native_Free(&robots);
	Native_Free(&help);
	Native_Free(&upper);
	Native_Free(&done);
	Native_Free(&list);
	return ok;
}

/* Init(LevelPath): the start of a level */
static byte In_InitN(const nativeVal_t *path) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dir = Mode_V(&t), *x = Mode_V(&t), *c = Mode_V(&t), *upper = Mode_V(&t), *menuMod = Mode_V(&t);
	nativeVal_t *deal = Mode_V(&t), *out = Mode_V(&t), *bertos = Mode_V(&t), *lives = Mode_V(&t), *skill = Mode_V(&t);
	nativeVal_t *n = Mode_V(&t), *label = Mode_V(&t);
	if (!Mode_Put(IN, "Path", NULL, path) || !Mode_Import("sys")) {
		return Mode_Done(&t, 0);
	}
	/* the map scripts of the level and its libraries */
	if (!Native_Arith('+', path, Mode_S(&t, "/Scripts"), dir) || !Mode_Append("sys", "path", dir) || !Mode_Pack(dir) ||
		!PORT_CALL(Scrap, PreloadLibrary, NULL, "ss", "Map", "Map.pyc") ||
		!PORT_CALL(Scrap, PreloadLibrary, NULL, "ss", "MapSnd", "MapSnd.pyc") ||
		!PORT_OWN(Init, PreloadMlibs, NULL, "v", path) || !Mode_Pack(NULL) || !Native_GVarGet("IsXbox", x)) {
		return Mode_Done(&t, 0);
	}
	int xb = Native_True(x);
	if (xb) {
		if (!Native_GVarGet("ShowCredits", c)) {
			return Mode_Done(&t, 0);
		}
		xb = Native_True(c) ? 0 : Mode_G(IN, "UpperMap", upper) ? Native_IsStr(upper, "SHIPEDIT") || Native_IsStr(upper, "MENU") : -1;
		if (xb > 0 && (!PORT_CALL(Scrap, PreloadLibrary, NULL, "ss", "XboxMenu", "Scripts/Scorer/XboxMenu.pyc") ||
			!Native_CallMod("__builtin__", "__import__", menuMod, "s", "XboxMenu") ||
			!PORT_OWN(Init, AddMLib, NULL, "v", menuMod))) {
			return Mode_Done(&t, 0);
		}
	}
	/* Map is the module of the current level: the function it holds through its entry */
	if (xb < 0 || !Mode_Import("Map") || !Mode_Import("MapSnd") || !MODE_CBATTR("Map", "InitStatus", NULL, "v", path) ||
		!PORT_OWN(Init, MLibInitStatus, NULL, "v", path) || !In_BossShip() || !Mode_G(IN, "SuperDealModule", deal) ||
		(Native_True(deal) && !In_Method(deal, "InitStatus", path))) {
		return Mode_Done(&t, 0);
	}
	/* the size of the mission arrows, the AI */
	int menu = In_Flag("inMainMenu"), edit = menu == 0 ? In_Flag("isShipEdit") : 0;
	if (menu < 0 || edit < 0 || (!menu && !edit && !PORT_OWN(Init, SetArrowSize, NULL, "")) ||
		!Mode_G(IN, "Outdoor", out) || !PORT_CALL(SAI, IniAI, NULL, "vv", path, out)) {
		return Mode_Done(&t, 0);
	}
	/* the menus, the mission info, the chat, the sound, the decoration */
	if (!Mode_GVarSetS("EscapeEvent", "Menu.Init") || !IN_CALL(Menu, Initialize)) {
		return Mode_Done(&t, 0);
	}
	int single = Native_NetOff();
	if (single < 0 || !Mode_GVarSetS("MainMissionEvent", single ? "MissionsFuncs.ShowMissionInfo" : "Ffa.ShowHelp")) {
		return Mode_Done(&t, 0);
	}
	int offline = In_Flag("OffLineGame");
	if (offline < 0 || (!offline && !IN_CALL(Chat, SetChatProc)) || !IN_CALL(Sound, Init) ||
		!PORT_CALL(Scrap, CreateElements, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	menu = In_Flag("inMainMenu");
	if (menu < 0 || (!menu && !In_Game(path))) {
		return Mode_Done(&t, 0);
	}
	/* the Bertos hunting the player outdoors */
	int outdoor = In_Flag("Outdoor");
	if (outdoor < 0) {
		return Mode_Done(&t, 0);
	}
	if (outdoor) {
		Mode_SaveVar("CreateBertos", bertos);
		int has = Mode_Read(IN, "UpperMap", upper) ? In_Flag("HasSuperDeal") : -1;
		if (has < 0) {
			return Mode_Done(&t, 0);
		}
		if (Native_IsStr(bertos, "1") && !Native_IsStr(upper, "GDB") && !has && (!Mode_Import("MissionsFuncs") ||
			!Mode_Import("Parking") || !PORT_CALL(MissionsFuncs, CreateBertos, NULL, "") ||
			!Mode_ModSetGlobal("Parking", "SwitchVehicleMissionCallbackAux", "MissionsFuncs", "ReassignBertosPlayerTarget") ||
			!Mode_ModSetGlobal("Parking", "SwitchCharMissionCallbackAux", "MissionsFuncs", "ReassignBertosPlayerTarget"))) {
			return Mode_Done(&t, 0);
		}
	}
	if (!In_BossRobots() || !Mode_GVarSetS("OnCrazyDealFinished", "MissionsFuncs.OnCrazyDealFinishedMessage") ||
		!Mode_GVarSetS("OnCrazyDealTarget", "MissionsFuncs.OnCrazyDealTarget")) {
		return Mode_Done(&t, 0);
	}
	/* the map (Map and MapSnd are the modules of the current level: the functions they hold through their entries) */
	if (!MODE_CBATTR("Map", "Init", NULL, "v", path) || !MODE_CBATTR("MapSnd", "Init", NULL, "v", path) ||
		!PORT_OWN(Init, MLibInit, NULL, "v", path) || !Mode_Read(IN, "SuperDealModule", deal) ||
		(Native_True(deal) && !In_Method(deal, "Init", path)) || !Mode_LaterIn(0.5, IN, "SetMapLabelText", NULL, 0, NULL)) {
		return Mode_Done(&t, 0);
	}
	int demo = In_Flag("isDemo");
	if (demo < 0) {
		return Mode_Done(&t, 0);
	}
	if (demo) {
		Mode_SaveVar("Player.NumLives", lives);
		if (Native_IsStr(lives, "-1")) {
			Mode_SaveS("Player.NumLives", "0");
		}
	}
	if (!Mode_GVarSetI("R_SceneBlurValue", 0)) {
		return Mode_Done(&t, 0);
	}
	/* the skill of a single player game */
	offline = In_Flag("OffLineGame");
	menu = offline > 0 ? In_Flag("inMainMenu") : 0;
	edit = offline > 0 && menu == 0 ? In_Flag("isShipEdit") : 0;
	if (offline < 0 || menu < 0 || edit < 0) {
		return Mode_Done(&t, 0);
	}
	if (offline && !menu && !edit) {
		if (!PORT_CALL(Scrap, CreateSaveVar, NULL, "ss", "GameSkill", "1")) {
			return Mode_Done(&t, 0);
		}
		Mode_SaveVar("GameSkill", skill);
		if (!Native_ToInt(skill, n) || !Native_GVarSet("GameSkill", n)) {
			return Mode_Done(&t, 0);
		}
	}
	LvN_Str(label, "Actual Level at : ");
	const nativeVal_t *items[2] = { label, path };
	return Mode_Done(&t, Mode_Print(items, 2));
}

/* SetMapLabelText(): the name of the station on the screen, unless the mission scorer shows another map's mission */
static byte In_SetMapLabelTextN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *vis = Mode_V(&t), *status = Mode_V(&t), *mm = Mode_V(&t), *lv = Mode_V(&t);
	nativeVal_t *key = Mode_V(&t), *text = Mode_V(&t), *zero = Mode_V(&t);
	Native_SetInt(zero, 0);
	if (!Mode_Import("Scorer") || !Mode_G("Scorer", "MissionScorerName", name) || !Mode_ScorerGetVal(zero, name, "Visible", vis)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(vis) || !Mode_EqI(vis, 1)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_ScorerGetVal(zero, name, "Status0", status)) {
		return Mode_Done(&t, 0);
	}
	int show = !Mode_EqI(status, 1);
	if (!show) {
		Mode_SaveVar("Mission.Map", mm);
		show = In_LevelName(0, lv) ? Mode_Cmp(mm, "==", lv) : -1;
	}
	return Mode_Done(&t, show >= 0 && (!show || (In_LevelName(0, lv) && LvN_Cat("Station_", lv, key) &&
		Mode_LangV(key, text) && Mode_LabelText(text, Mode_Time() + 10))));
}

/* Close(): the progress bar of the next level (the files it loads, from the tables of the module) */
static byte In_CloseN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lv = Mode_V(&t), *upper = Mode_V(&t), *line = Mode_V(&t), *xbox = Mode_V(&t), *bars = Mode_V(&t);
	nativeVal_t *n = Mode_V(&t), *msg = Mode_V(&t);
	const nativeVal_t *items[1] = { line };
	if (!In_LevelName(1, lv) || !Native_Upper(lv, upper) || !Native_Arith('%', Mode_S(&t, "Next level: %s"), upper,
		line) || !Mode_Print(items, 1) || !Native_GVarGet("IsXbox", xbox) ||
		!Mode_G(IN, Mode_EqI(xbox, 1) ? "PROGRESSBAR_XBOX" : "PROGRESSBAR_PC", bars)) {
		return Mode_Done(&t, 0);
	}
	/* PyMapping_HasKey: any error of the lookup is no key */
	if (Mode_Sub(bars, upper, n)) {
		return Mode_Done(&t, Native_GVarSet("ProgressBar", n));
	}
	Mode_DropError();
	return Mode_Done(&t, Native_Arith('%', Mode_S(&t, "ERROR: '%s' progress bar info not found \n"), upper, msg) &&
		Mode_ConsoleError(msg) && Mode_GVarSetDef("ProgressBar"));
}

/* the adapters: the argument formats of the C port */
MODE_FN(Init, LoadCharLib, 1, In_LoadCharLibN(&a[0]))
MODE_FN(Init, AddMLib, 1, Mode_Append(IN, "MapLibs", &a[0]))
MODE_FN(Init, MLibInitStatus, 1, In_ForLibs(&a[0], "InitStatus"))
MODE_FN(Init, MLibInit, 1, In_ForLibs(&a[0], "Init"))
MODE_FN(Init, PreloadMlibs, 1, In_PreloadMlibsN())
MODE_FN(Init, Init, 1, In_InitN(&a[0]))

/* isMission() as an int */
static byte In_IsMissionOut(nativeVal_t *out) {
	int m = In_IsMission();
	Native_SetInt(out, m);
	return m >= 0;
}

MODE_FNANY(Init, isMission, In_IsMissionOut(out))

MODE_FNANY(Init, SetArrowSize, In_SetArrowSizeN())

static PyObject *Init_SetMapLabelText(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(In_SetMapLabelTextN());
}

static PyObject *Init_Close(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(In_CloseN());
}

/* ---- the loops of the module code (data/modules/init.mod; OBJLIB 6: they work on the dict of the module code) ---- */

static byte In_LoadCharBody(PyObject *locals, PyObject *fn, void *ctx) {
	(void)ctx;
	PyObject *f = Script_ModuleName(locals, "LoadCharLib");
	PyObject *args = f ? Py_BuildValue("(O)", fn) : NULL;
	PyObject *res = args ? Script_CallObject(f, args, NULL) : NULL;
	Py_XDECREF(args);
	Py_XDECREF(f);
	Py_XDECREF(res);
	return res != NULL;
}

/* init.py:205, `for fn in ('Nurse', ...): LoadCharLib(fn)`: the characters of an indoor level and their sounds */
int Init_ModLoadChars(PyObject *module, PyObject *locals) {
	static const char *const chars[] = { "Nurse", "Police", "Desktop", "Gear", "Sentinel", "Berto", "BankDirector",
		"Mayor", "GateKeeper", "Betty", "Dtritus", "Bishop", "Functionary", "Humphrey", "PoliceBoss", "Messenger",
		"Human", "BankMaster" };
	enum { NUM_CHARS = sizeof(chars) / sizeof(chars[0]) };
	(void)module;
	PyObject *seq = PyTuple_New(NUM_CHARS);
	for (int i = 0; seq && i < NUM_CHARS; i++) {
		PyObject *v = Script_StrConst(chars[i], (int)strlen(chars[i]));
		if (!v) {
			Py_DECREF(seq);
			seq = NULL;
			break;
		}
		PyTuple_SET_ITEM(seq, i, v);
	}
	byte ok = seq && Script_ModuleFor(locals, "fn", seq, In_LoadCharBody, NULL);
	Py_XDECREF(seq);
	return ok;
}

/* apply(apply, func): func is (Scrap.PreloadLibrary, (module, file)) */
static byte In_ExtraBody(PyObject *locals, PyObject *func, void *ctx) {
	(void)ctx;
	PyObject *apply = Script_ModuleName(locals, "apply");
	PyObject *args = apply ? Py_BuildValue("(OO)", apply, func) : NULL;
	PyObject *res = args ? Script_CallObject(apply, args, NULL) : NULL;
	Py_XDECREF(args);
	Py_XDECREF(apply);
	Py_XDECREF(res);
	return res != NULL;
}

/* init.py:251, `for func in ExtraExecs[UpperMap]: apply(apply, func)`: the extra characters of some levels */
int Init_ModExtraExecs(PyObject *module, PyObject *locals) {
	(void)module;
	PyObject *table = Script_ModuleName(locals, "ExtraExecs");
	PyObject *key = table ? Script_ModuleName(locals, "UpperMap") : NULL;
	PyObject *seq = key ? PyObject_GetItem(table, key) : NULL;
	byte ok = seq && Script_ModuleFor(locals, "func", seq, In_ExtraBody, NULL);
	Py_XDECREF(table);
	Py_XDECREF(key);
	Py_XDECREF(seq);
	return ok;
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

PORTS(InitLevel)
