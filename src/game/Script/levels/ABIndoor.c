/*
 * The scripts of the level ABIndoor in C (levels/abindoor/scripts of data03.packed, issue #2): the Map module of the
 * end of the game (the friends after the final fight, Dtritus' ship, the last scene, the end title). The scenes, the
 * character callbacks, the speeches, PlaySceneA and Credits are data (the files in data/levels/abindoor). The module
 * variable FXSceneTheEndBFile and the module code (the save variable ABIndoor.Scene) stay in the script.
 */
#include "scrap/script.h"
#include "Levels.h"

#define MAP "Map"

/* InitStatus(LevelPath): an indoor map; the four friends of Dtritus, the mission after the final fight */
static PyObject *Abi_InitStatus(PyObject *self, PyObject *args) {
	static const char *const res[] = { "Human", "Betty", "CrazyGambler", "Berto" };
	static const char *const chars[][2] = { { "Human_Default", "CreateHuman" }, { "Betty_Default", "CreateBetty" },
		{ "Berto_Default", "CreateBerto" }, { "CrazyGambler_Default", "CreateCrazyGambler" } };
	static const char *const vars[][2] = { { "Mission.Map", "GamblinDen" }, { "Mission.Library", "Mission_AfterFinalFight" },
		{ "Journalist.Berto_Defaut", "noplace" }, { "Journalist.Betty_Defaut", "noplace" },
		{ "Journalist.Humprey_Defaut", "noplace" }, { "PoliceBossAtPolice", "0" }, { "CrazyGamblerAtGambinDen", "1" } };
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Import(&f, "SaveGame");
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TheEnd_MissionText1", 1);
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Teleporter", "DM_Door_Teleporter_GamblinDen");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Spawn", "DM_Player_Spawn");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
	for (int i = 0; i < 4; i++) {
		Sf_Run(&f, "Chars", "AddNewCharRes", "(s)", res[i]);
	}
	for (int i = 0; i < 4; i++) {
		int mark = f.n;
		Lv_MissionChar(&f, chars[i][0], MAP, chars[i][1]);
		Sf_Release(&f, mark);
	}
	for (int i = 0; i < 7; i++) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", vars[i][0], vars[i][1]);
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the first time the scene with the friends (1 s after the start, in black), else the ship at once;
 * the teleporter back to GamblinDen */
static PyObject *Abi_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionIndoor", 0);
	Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/AfterFinalFight.ogg", 1.0);
	int first = Lv_SaveIs(&f, "ABIndoor.Scene", "0");
	if (first > 0) {
		Sf_Call(&f, Sf_Global(&f, MAP, "LoadSceneA"), "()");
		Sf_Call(&f, Sf_Global(&f, MAP, "LoadSceneB"), "()");
		Sf_Run(&f, "SInput", "SetActionSet", "(s)", "Inactive");
		Sf_Run(&f, "Menu", "DrawScorer", "(is)", 0, "Inactive");
		Sf_Later(&f, Sf_Float(&f, 1.0), Sf_Global(&f, MAP, "PlaySceneA"), Sf_Build(&f, "()"), NULL);
		Lv_Fade(&f, lv_black, lv_black, 4.0);
	} else if (first == 0) {
		Sf_Call(&f, Sf_Global(&f, MAP, "LoadSceneB"), "()");
		Sf_Call(&f, Sf_Global(&f, MAP, "Goodbye"), "()");
	}
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Teleporter_GamblinDen", "GamblinDen");
	return Sf_None(&f);
}

/* Goodbye(usr=None): the end of the first scene: Dtritus' ship at its dummy as the mission target; the first time a
 * fade in and a save; the far plane of the camera */
static PyObject *Abi_Goodbye(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	Sf_Run(&f, "SInput", "SetActionSet", "(s)", "Walk");
	PyObject *dummy = Lv_Dummy(&f, "DM_DtritusShip", 1);
	if (Sf_True(&f, dummy) == 0) {
		Sf_RaiseText(&f, "No se encuentra el dummy: 'DM_DtritusShip'");
	}
	PyObject *pos = Sf_Item(&f, dummy, 1);
	PyObject *e = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "CheckArrow", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
		Sf_Item(&f, pos, 2), "WithLife");
	Sf_SetAttrI(&f, e, "ArrowCanUse", 1);
	Sf_Method(&f, e, "SetModel", "(s)", "Levels/ABIndoor/Map/TheEndA/Moto.m3d");
	Sf_SetAttrI(&f, e, "MapCollision", 1);
	Sf_SetAttrI(&f, e, "UseAngSel", 0);
	Sf_SetAttrI(&f, e, "UseMapCheck", 0);
	Sf_SetAttrI(&f, e, "VRadius", 200);
	Sf_SetAttrI(&f, e, "ArrnOpt", 0);
	Sf_SetAttr(&f, e, "Descriptor", Sf_Lang(&f, Sf_Str(&f, "SHIP_DTRITUSSHIP")));
	Sf_SetAttr(&f, e, "ArrOpt0", Sf_Cat(&f, "sO", "Use:", Sf_Lang(&f, Sf_Str(&f, "USE_DtritusShip"))));
	Sf_SetAttr(&f, e, "CamTgt", Sf_Build(&f, "(iii)", 0, 300, 0));
	Sf_SetAttrS(&f, e, "UseFunc", "Map.LaunchCinematic");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, e, "Name"));
	int first = Lv_SaveIs(&f, "ABIndoor.Scene", "0");
	if (first > 0) {
		Lv_Fade(&f, lv_black, lv_clear, 1.0);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ABIndoor.Scene", "1");
		Sf_Schedule(&f, Sf_Int(&f, 0), Sf_Mod(&f, "SaveGame", "SaveGame"), Sf_Build(&f, "(i)", 0), NULL);
	}
	PyObject *cam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	if (Sf_True(&f, cam) > 0) {
		Sf_SetAttr(&f, cam, "clipFar", Sf_Float(&f, 150000.0));
	}
	return Sf_None(&f);
}

/* LaunchCinematic(UsrName, CharName, optId, FullOption): the use of the ship: the last scene, then the credits */
static PyObject *Abi_LaunchCinematic(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionIndoor", 0);
	Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/Mantrap.ogg", 1.0);
	Sf_EntSetI(&f, "CheckArrow", "Active", 0);
	Sf_Run(&f, "Scene", "Play", "(s)", "TheEndB");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, MAP, "Credits"));
	return Sf_None(&f);
}

/* ShowEndTitle(id): a black square (clear) and the typed end title; the motion blur */
static PyObject *Abi_ShowEndTitle(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "BlackSquare", "Sprite");
	static const char *const square[] = { "X", "Y", "ScaleX", "ScaleY", "Red", "Green", "Blue", "Alpha" };
	static const int squareV[] = { 0, 0, 640, 480, 0, 0, 0, 0 };
	for (int i = 0; i < 8; i++) {
		Lv_ScorerSet(&f, id, "BlackSquare", square[i], Sf_Int(&f, squareV[i]));
	}
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "EndTitle", "TextTyping");
	Lv_ScorerSet(&f, id, "EndTitle", "X", Sf_Int(&f, 320));
	Lv_ScorerSet(&f, id, "EndTitle", "Y", Sf_Int(&f, 240));
	Lv_ScorerSet(&f, id, "EndTitle", "Font", Sf_Str(&f, "ScrapBig"));
	Lv_ScorerSet(&f, id, "EndTitle", "Effect", Sf_Str(&f, "Outline"));
	Lv_ScorerSet(&f, id, "EndTitle", "Text", Sf_Lang(&f, Sf_Str(&f, "GameFinal")));
	Lv_ScorerSet(&f, id, "EndTitle", "CentralText", Sf_Int(&f, 1));
	Lv_ScorerSet(&f, id, "EndTitle", "Red", Sf_Int(&f, 70));
	Lv_ScorerSet(&f, id, "EndTitle", "Green", Sf_Int(&f, 173));
	Lv_ScorerSet(&f, id, "EndTitle", "Blue", Sf_Int(&f, 255));
	Lv_ScorerSet(&f, id, "EndTitle", "Alpha", Sf_Int(&f, 255));
	Lv_ScorerSet(&f, id, "EndTitle", "TypingTime", Sf_Float(&f, -1.0));
	Lv_ScorerSet(&f, id, "EndTitle", "TypingSpeed", Sf_Float(&f, 0.15));
	Lv_ScorerSet(&f, id, "EndTitle", "CursorAtInit", Sf_Int(&f, 1));
	Lv_ScorerSet(&f, id, "EndTitle", "CursorAtEnd", Sf_Int(&f, 0));
	Lv_ScorerSet(&f, id, "EndTitle", "TypingSound", Sf_Int(&f, 0));
	Sf_Run(&f, "Scrap", "Set", "(si)", "MotionBlurFactorBegin", 0);
	Sf_Run(&f, "Scrap", "Set", "(sd)", "MotionBlurFactorEnd", 0.75);
	Sf_Run(&f, "Scrap", "Set", "(sd)", "MotionBlurTime", 0.0);
	Sf_Run(&f, "Scrap", "Set", "(sd)", "MotionBlurEndTime", 3.0);
	Sf_Run(&f, "Scrap", "Set", "(si)", "MotionBlurOffVar", 0);
	return Sf_None(&f);
}

/* FadeEndTitle(id): the music fades out in 5 s, the black square fades in over the end title */
static PyObject *Abi_FadeEndTitle(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Run(&f, "Scrap", "MusicPlayer", "(ssd)", "", "fadeout", 1 / 5.0);
	Sf_Run(&f, "Scrap", "Set", "(sd)", "FadeOutFinalTime", 3.0);
	Lv_ScorerSet(&f, id, "BlackSquare", "OnRender", Sf_Str(&f, "!ScorerSpriteFadeOutFinal"));
	return Sf_None(&f);
}

PyMethodDef g_ABIndoorMapMethods[] = {
	{ "InitStatus", Abi_InitStatus, 1, NULL },
	{ "Init", Abi_Init, 1, NULL },
	{ "Goodbye", (PyCFunction)(void (*)(void))Abi_Goodbye, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "LaunchCinematic", Abi_LaunchCinematic, 1, NULL },
	{ "ShowEndTitle", Abi_ShowEndTitle, 1, NULL },
	{ "FadeEndTitle", Abi_FadeEndTitle, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
