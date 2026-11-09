/*
 * The scripts of the level ABIndoor in C (levels/abindoor/scripts of data03.packed, issue #2): the Map module of the
 * end of the game (the friends after the final fight, Dtritus' ship, the last scene, the end title). The scenes, the
 * character callbacks, the speeches, PlaySceneA and Credits are data (the files in data/levels/abindoor). The module
 * variable FXSceneTheEndBFile and the module code (the save variable ABIndoor.Scene) stay in the script.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h). The save and engine
 * variables, the scorer items, the action set, the camera and the music are C; Scrap.Set, SScorer.Set, SScorer.Fade,
 * SSound.SetMusic, Scrap.MusicPlayer, Scrap.CreateEntity, Scrap.AddScheduledFunc and the dummy search (fc-abi swaps
 * Scrap.StartDummySearch and NextDummySearch) go through their seams (OBJLIB 7); the ship and the camera are handle
 * values (OBJLIB 7); Init.Outdoor and Scene.EndOfSceneCallback are variables of other modules (OBJLIB 5).
 *
 * Direct calls (stage 3): the functions of SaveGame, Elevator, Chars, Menu, MissionsFuncs and Scene and the map's own
 * Goodbye go through their refs (PORT_CALL, PORT_LOWN; the seam takes the call by name when a case has put something
 * else there, OBJLIB 5). By name: the sections of map.def (LoadSceneA, LoadSceneB and the Create functions of the
 * mission characters; OBJLIB 6); PlaySceneA, Credits and SaveGame.SaveGame given to the scheduler take their entries
 * (stage 4: Port_Scheduled), PlaySceneA and Credits given to Scene are values of its variables (OBJLIB 5).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/world.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL abindoor

#define MAP "Map"

/* the hook of module.name when a case replaced the engine function (1), 0 for the engine's own, -1 with the error */
#define ABI_SEAM(module, name) \
	PyObject *hook; \
	int seam = Native_Seam(module, name, &hook); \
	if (seam < 0) { \
		return 0; \
	}

/* SScorer.Set(id, item, var, d) */
static byte Abi_ScF(const nativeVal_t *id, const char *item, const char *var, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	return LvN_ScorerSet(id, item, var, &v);
}

/* Scrap.StartDummySearch(pattern, 1); dummy = Scrap.NextDummySearch(), through their seams (fc-abi swaps both) */
static byte Abi_Dummy(const char *pattern, nativeVal_t *dummy) {
	Native_SetNone(dummy);
	return Mode_DummyStartSeam(pattern, 1) && Mode_DummyNextSeam(dummy);
}

/* InitStatus(LevelPath): an indoor map; the four friends of Dtritus, the mission after the final fight */
static byte Abi_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const res[] = { "Human", "Betty", "CrazyGambler", "Berto" };
	static const char *const chars[][2] = { { "Human_Default", "CreateHuman" }, { "Betty_Default", "CreateBetty" },
		{ "Berto_Default", "CreateBerto" }, { "CrazyGambler_Default", "CreateCrazyGambler" } };
	static const char *const vars[][2] = { { "Mission.Map", "GamblinDen" }, { "Mission.Library", "Mission_AfterFinalFight" },
		{ "Journalist.Berto_Defaut", "noplace" }, { "Journalist.Betty_Defaut", "noplace" },
		{ "Journalist.Humprey_Defaut", "noplace" }, { "PoliceBossAtPolice", "0" }, { "CrazyGamblerAtGambinDen", "1" } };
	nativeVal_t zero;
	(void)a;
	(void)out;
	Native_SetInt(&zero, 0);
	if (!Mode_Import("Init") || !Mode_Import("SaveGame") || !Mode_SetModVar("Init", "Outdoor", &zero) ||
		!PORT_CALL(SaveGame, MMDesc, NULL, "si", "Mission_TheEnd_MissionText1", 1L) ||
		!PORT_CALL(Elevator, SetDef, NULL, "ss", "Teleporter", "DM_Door_Teleporter_GamblinDen")) {
		return 0;
	}
	SaveVar_Set("ComeFrom", "");
	SaveVar_Set("Spawn", "DM_Player_Spawn");
	SaveVar_Set("Char", "Dtritus");
	for (int i = 0; i < 4; i++) {
		if (!PORT_CALL(Chars, AddNewCharRes, NULL, "s", res[i])) {
			return 0;
		}
	}
	for (int i = 0; i < 4; i++) {
		/* OBJLIB 6: the Create functions are sections of map.def */
		if (!LvN_MissionChar(chars[i][0], MAP, chars[i][1])) {
			return 0;
		}
	}
	for (int i = 0; i < 7; i++) {
		SaveVar_Set(vars[i][0], vars[i][1]);
	}
	return 1;
}

/* Init(LevelPath): the first time the scene with the friends (1 s after the start, in black), else the ship at once;
 * the teleporter back to GamblinDen */
static byte Abi_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_Import("Menu") || !Mode_GVarSetI("isActionIndoor", 0) || !LvN_SetMusic("Music/AfterFinalFight.ogg", 1.0)) {
		return 0;
	}
	byte ok;
	/* OBJLIB 6: LoadSceneA and LoadSceneB are sections of map.def */
	if (LvN_SaveIs("ABIndoor.Scene", "0")) {
		ok = LV_OWN(MAP, "LoadSceneA", NULL, "") && LV_OWN(MAP, "LoadSceneB", NULL, "");
		if (ok) {
			Mode_SetActionSet("Inactive");
			ok = PORT_CALL(Menu, DrawScorer, NULL, "is", 0L, "Inactive") && Mode_Later(1.0, MAP, "PlaySceneA", NULL, 0, NULL) &&
				Mode_Fade(0, lv_black, lv_black, 4.0);
		}
	} else {
		ok = LV_OWN(MAP, "LoadSceneB", NULL, "") && PORT_LOWN(Map, Goodbye, NULL, "");
	}
	if (!ok) {
		return 0;
	}
	nativeVal_t doors, maps;
	Native_SetEmpty(&doors, NV_LIST);
	LvN_Str(Native_Append(&doors), "DM_Door_Teleporter_GamblinDen");
	Native_SetEmpty(&maps, NV_LIST);
	LvN_Str(Native_Append(&maps), "GamblinDen");
	ok = PORT_CALL(Elevator, ConnectElevators, NULL, "vv", &doors, &maps);
	Native_Free(&doors);
	Native_Free(&maps);
	return ok;
}

/* the ship of Goodbye at pos: its model, use, texts and camera target, the mission target */
static byte Abi_Ship(const nativeVal_t *pos, nativeVal_t *e) {
	nativeVal_t name, v, t, n;
	LvN_Str(&name, "CheckArrow");
	Native_SetNone(&v);
	Native_SetNone(&t);
	Native_SetNone(&n);
	byte ok = LvN_CreateAt(&name, pos, "WithLife", e) && LvN_SetI(e, "ArrowCanUse", 1);
	if (ok) {
		LvN_Str(&v, "Levels/ABIndoor/Map/TheEndA/Moto.m3d");
		ok = Mode_VModel(e, &v);
		Native_Free(&v);
	}
	ok = ok && LvN_SetI(e, "MapCollision", 1) && LvN_SetI(e, "UseAngSel", 0) && LvN_SetI(e, "UseMapCheck", 0) &&
		LvN_SetI(e, "VRadius", 200) && LvN_SetI(e, "ArrnOpt", 0);
	if (ok) {
		Mode_Lang("SHIP_DTRITUSSHIP", &v);
		ok = Mode_VSet(e, "Descriptor", &v);
		Native_Free(&v);
	}
	if (ok) {
		/* "Use:" + Scrap.GetLangStr("USE_DtritusShip") */
		LvN_Str(&t, "Use:");
		Mode_Lang("USE_DtritusShip", &v);
		ok = Native_Arith('+', &t, &v, &n) && Mode_VSet(e, "ArrOpt0", &n);
		Native_Free(&t);
		Native_Free(&v);
		Native_Free(&n);
	}
	if (ok) {
		Native_SetEmpty(&t, NV_TUPLE);
		Native_SetInt(Native_Append(&t), 0);
		Native_SetInt(Native_Append(&t), 300);
		Native_SetInt(Native_Append(&t), 0);
		ok = Mode_VSet(e, "CamTgt", &t) && LvN_SetS(e, "UseFunc", "Map.LaunchCinematic") && Mode_VGet(e, "Name", &n) &&
			PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &n);
	}
	Native_Free(&name);
	Native_Free(&t);
	Native_Free(&n);
	return ok;
}

/* Goodbye(usr=None): the end of the first scene: Dtritus' ship at its dummy as the mission target; the first time a
 * fade in and a save; the far plane of the camera */
static byte Abi_Goodbye(void) {
	nativeVal_t dummy, pos, e, t, zero, name, cam;
	if (!Mode_Import("MissionsFuncs")) {
		return 0;
	}
	Mode_SetActionSet("Walk");
	if (!Abi_Dummy("DM_DtritusShip", &dummy)) {
		return 0;
	}
	Native_SetNone(&pos);
	Native_SetNone(&e);
	byte ok = Native_True(&dummy) || LvN_RaiseText("No se encuentra el dummy: 'DM_DtritusShip'");
	ok = ok && LvN_Item(&dummy, 1, &pos) && Abi_Ship(&pos, &e);
	Native_Free(&dummy);
	Native_Free(&pos);
	Native_Free(&e);
	if (ok && LvN_SaveIs("ABIndoor.Scene", "0")) {
		/* Scrap.AddScheduledFunc(0, SaveGame.SaveGame, (0,)) */
		PyObject *fn;
		ok = Mode_Fade(0, lv_black, lv_clear, 1.0);
		if (ok) {
			SaveVar_Set("ABIndoor.Scene", "1");
			fn = Script_Attr("SaveGame", "SaveGame");
			Native_SetInt(&t, 0);
			Native_SetInt(&zero, 0);
			ok = fn && Mode_Schedule(&t, fn, &zero, 1, NULL);
			Py_XDECREF(fn);
		}
	}
	if (!ok) {
		return 0;
	}
	/* cam = Scrap.GetEntity(Scrap.GetCam(0)); if cam: cam.clipFar = 150000.0 */
	const char *c = cWorld_GetCamName(g_pWorld, 0);
	if (c) {
		LvN_Str(&name, c);
	} else {
		Native_SetNone(&name);
	}
	Native_SetNone(&cam);
	ok = Mode_EntityH(&name, &cam) && (!Native_True(&cam) || LvN_SetF(&cam, "clipFar", 150000.0));
	Native_Free(&name);
	Native_Free(&cam);
	return ok;
}

MODE_KW(Abi, GoodbyePy, "|O", "usr") {
	return Abi_Goodbye();
}

/* LaunchCinematic(UsrName, CharName, optId, FullOption): the use of the ship: the last scene, then the credits */
static byte Abi_LaunchCinematic(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t name, zero;
	(void)a;
	(void)out;
	if (!Mode_Import("Menu") || !Mode_GVarSetI("isActionIndoor", 0) || !LvN_SetMusic("Music/Mantrap.ogg", 1.0)) {
		return 0;
	}
	LvN_Str(&name, "CheckArrow");
	Native_SetInt(&zero, 0);
	byte ok = Mode_EntSet(&name, "Active", &zero) && PORT_CALL(Scene, Play, NULL, "s", "TheEndB") &&
		LvN_ModSetGlobal("Scene", "EndOfSceneCallback", MAP, "Credits");
	Native_Free(&name);
	return ok;
}

/* ShowEndTitle(id): a black square (clear) and the typed end title; the motion blur */
static byte Abi_ShowEndTitle(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const square[] = { "X", "Y", "ScaleX", "ScaleY", "Red", "Green", "Blue", "Alpha" };
	static const int squareV[] = { 0, 0, 640, 480, 0, 0, 0, 0 };
	const nativeVal_t *id = &a[0];
	nativeVal_t text;
	(void)out;
	if (!Mode_ScorerAddS(id, "BlackSquare", "Sprite")) {
		return 0;
	}
	for (int i = 0; i < 8; i++) {
		if (!Mode_ScorerSetI(id, "BlackSquare", square[i], squareV[i])) {
			return 0;
		}
	}
	if (!Mode_ScorerAddS(id, "EndTitle", "TextTyping") || !Mode_ScorerSetI(id, "EndTitle", "X", 320) || !Mode_ScorerSetI(id, "EndTitle", "Y", 240) ||
		!LvN_ScorerSetS(id, "EndTitle", "Font", "ScrapBig") || !LvN_ScorerSetS(id, "EndTitle", "Effect", "Outline")) {
		return 0;
	}
	Mode_Lang("GameFinal", &text);
	byte ok = LvN_ScorerSet(id, "EndTitle", "Text", &text);
	Native_Free(&text);
	return ok && Mode_ScorerSetI(id, "EndTitle", "CentralText", 1) && Mode_ScorerSetI(id, "EndTitle", "Red", 70) &&
		Mode_ScorerSetI(id, "EndTitle", "Green", 173) && Mode_ScorerSetI(id, "EndTitle", "Blue", 255) && Mode_ScorerSetI(id, "EndTitle", "Alpha", 255) &&
		Abi_ScF(id, "EndTitle", "TypingTime", -1.0) && Abi_ScF(id, "EndTitle", "TypingSpeed", 0.15) &&
		Mode_ScorerSetI(id, "EndTitle", "CursorAtInit", 1) && Mode_ScorerSetI(id, "EndTitle", "CursorAtEnd", 0) &&
		Mode_ScorerSetI(id, "EndTitle", "TypingSound", 0) && Mode_GVarSetI("MotionBlurFactorBegin", 0) &&
		Mode_GVarSetN("MotionBlurFactorEnd", 0.75) && Mode_GVarSetN("MotionBlurTime", 0.0) && Mode_GVarSetN("MotionBlurEndTime", 3.0) &&
		Mode_GVarSetI("MotionBlurOffVar", 0);
}

/* Scrap.MusicPlayer(name, cmd, volume) through its seam (fc-lvl-menu swaps it) */
static byte Abi_MusicPlayer(const char *name, const char *cmd, double volume) {
	ABI_SEAM("Scrap", "MusicPlayer");
	if (seam) {
		return Native_HookCall(hook, NULL, "ssd", name, cmd, volume);
	}
	Music_Play(name, cmd, (float)volume);
	return 1;
}

/* FadeEndTitle(id): the music fades out in 5 s, the black square fades in over the end title */
static byte Abi_FadeEndTitle(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Abi_MusicPlayer("", "fadeout", 1 / 5.0) && Mode_GVarSetN("FadeOutFinalTime", 3.0) &&
		LvN_ScorerSetS(&a[0], "BlackSquare", "OnRender", "!ScorerSpriteFadeOutFinal");
}

LV_FN(Abi, InitStatusPy, 1, Abi_InitStatus)
LV_FN(Abi, InitPy, 1, Abi_Init)
LV_FN(Abi, LaunchCinematicPy, 4, Abi_LaunchCinematic)
LV_FN(Abi, ShowEndTitlePy, 1, Abi_ShowEndTitle)
LV_FN(Abi, FadeEndTitlePy, 1, Abi_FadeEndTitle)

PyMethodDef g_ABIndoorMapMethods[] = {
	{ "InitStatus", Abi_InitStatusPy, 1, NULL },
	{ "Init", Abi_InitPy, 1, NULL },
	{ "Goodbye", (PyCFunction)(void (*)(void))Abi_GoodbyePy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "LaunchCinematic", Abi_LaunchCinematicPy, 1, NULL },
	{ "ShowEndTitle", Abi_ShowEndTitlePy, 1, NULL },
	{ "FadeEndTitle", Abi_FadeEndTitlePy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_ABIndoor)
