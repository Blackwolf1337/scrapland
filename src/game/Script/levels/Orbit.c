/*
 * The scripts of the level Orbit in C (levels/orbit/scripts, issue #2): the Map module (the teleporter and the doors of
 * the entrance), MapDecont (the first mission: the tutorial, the decontamination, the scenes of the bishops, the
 * functionary and the fall to the town hall) and Mission_IntroHuman (the arrival of Bill, three scenes). The scenes
 * are call tables (Sf_Calls). MapSnd is data (data/levels/orbit/mapsnd.def). The module variables
 * (FXSceneDecontaminationFile, FXSceneBishopsFile, FXSceneBillArrives_AFile) and the module code (the save variable
 * Orbit.Decontaminated) stay in the scripts.
 */
#include "scrap/script.h"

#define MD "MapDecont"
#define IH "Mission_IntroHuman"

#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define LANG(k) Sf_Lang(&f, Sf_Str(&f, k))
#define LATER(d, fn, args) Sf_Later(&f, Sf_Float(&f, d), fn, args, NULL)

/* Scrap.GetLangStr(key) % SInput.GetDefinedEntry(0, "Walk", action, "Mouse") */
static PyObject *Ob_KeyText(scriptFrame_t *f, const char *key, const char *action) {
	PyObject *t = Sf_Lang(f, Sf_Str(f, key));
	return Sf_Op(f, t, '%', Sf_Ask(f, "SInput", "GetDefinedEntry", "(isss)", 0, "Walk", action, "Mouse"));
}

/* SScorer.SetMsgText(text, Scrap.GetTime() + secs) */
static byte Ob_Msg(scriptFrame_t *f, PyObject *text, long secs) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, "SScorer", "SetMsgText");
	Sf_Call(f, fn, "(OO)", text, Sf_OpI(f, Sf_Time(f), '+', secs));
	Sf_Release(f, mark);
	return !f->failed;
}

/* e.RadarShow = 1; e.ArrowCanMission = 1; e.ArrowModMission = Scrap.Get("ArrowMissionFile") (0: the first two only) */
static byte Ob_Arrow(scriptFrame_t *f, PyObject *e, int on) {
	int mark = f->n;
	Sf_SetAttrI(f, e, "RadarShow", on);
	Sf_SetAttrI(f, e, "ArrowCanMission", on);
	if (on) {
		Sf_SetAttr(f, e, "ArrowModMission", Sf_Ask(f, "Scrap", "Get", "(s)", "ArrowMissionFile"));
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* Scrap.Set(name, file); Scrap.SetDebrisSys(file, 1, 0, 0, 1, 0, 0, Fx.<fx>) with file the global of module */
static byte Ob_SceneFx(scriptFrame_t *f, const char *module, const char *global, const char *fx) {
	int mark = f->n;
	Sf_Import(f, "Fx");
	Sf_Run(f, "Scrap", "Set", "(sO)", global, Sf_Global(f, module, global));
	PyObject *fn = Sf_Mod(f, "Scrap", "SetDebrisSys");
	PyObject *file = Sf_Global(f, module, global);
	Sf_Call(f, fn, "(OiiiiiiO)", file, 1, 0, 0, 1, 0, 0, Sf_Mod(f, "Fx", fx));
	Sf_Release(f, mark);
	return !f->failed;
}

/* ---- Map ---- */

/* InitStatus(LevelPath): an indoor map */
static PyObject *Orbit_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	return Sf_None(&f);
}

/* Init(LevelPath): the teleporters linked, the back doors of the entrance, the music */
static PyObject *Orbit_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Teleport");
	Sf_Run(&f, "Teleport", "Link", "(ss)", "DM_Door_Teleporter_00", "DM_Door_Teleporter_01");
	Sf_EntSetS(&f, "DM_Door_OrbitDoorA_04", "OnObjSlot", "!BackDoorSlot");
	Sf_EntSetS(&f, "DM_Door_OrbitDoorA_03", "OnObjSlot", "!BackDoorSlot");
	Sf_Import(&f, "SSound");
	Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/Track1.ogg", 1.0);
	return Sf_None(&f);
}

PyMethodDef g_OrbitMapMethods[] = {
	{ "InitStatus", Orbit_InitStatus, 1, NULL },
	{ "Init", Orbit_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapDecont ---- */

/* InitStatus(LevelPath): the characters of the mission, the functionary sits down at once, the credits library */
static PyObject *Decont_InitStatus(PyObject *self, PyObject *args) {
	static const char *const chars[] = { "Sentinel_Corridor", "Bishop_00", "Bishop_01", "Functionary_00", NULL };
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Challenge", "0");
	Sf_Import(&f, "CharsNPC");
	Sf_Import(&f, "Chars");
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", Sf_Global(&f, MD, "AfterBrowseNPC"));
	for (int i = 0; chars[i]; i++) {
		Sf_Run(&f, "CharsNPC", "CreateMissionChar", "(s)", chars[i]);
	}
	Sf_Run(&f, "Scrap", "Set", "(sd)", "FunctionaryMinTimeToSitDown", 0.0);
	Sf_Run(&f, "Scrap", "Set", "(sd)", "FunctionaryMaxTimeToSitDown", 1.0);
	Sf_Run(&f, "Scrap", "OpenPack", "(s)", "Scripts/Scorer");
	Sf_Run(&f, "Scrap", "PreloadLibrary", "(ss)", "CreditsText", "CreditsText.pyc");
	Sf_Run(&f, "Scrap", "ClosePack", "()");
	return Sf_None(&f);
}

/* Decontamination(UsrName, CharName, optId, FullOption): the keypad goes, the scene plays, the way point */
static PyObject *Decont_Decontamination(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	PyObject *kb = ENT("Decontamination_keyboard");
	Sf_SetAttrI(&f, kb, "MapCollision", 0);
	Sf_Method(&f, kb, "Del", "()");
	Sf_Import(&f, "Scene");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, MD, "EndOfScene"));
	Sf_Run(&f, "Scene", "Play", "(s)", "Decontamination");
	Sf_Own(&f, MD, "DecontaminatedWaypoint", "()");
	return Sf_None(&f);
}

/* EndOfScene(usr): the player behind the decontamination, the voice of the check in, the doors open */
static PyObject *Decont_EndOfScene(PyObject *self, PyObject *args) {
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	Sf_Import(&f, "Speech");
	Sf_SetAttr(&f, usr, "Pos", Sf_Build(&f, "(iii)", -20334, 1056, -27485));
	Sf_SetAttr(&f, usr, "Ang", Sf_Build(&f, "(iii)", 0, -90, 0));
	Sf_Run(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, usr, "Name"));
	Sf_Run(&f, "Speech", "OffVoice", "(s)", "Mission_Checkin_RemoteMessage2");
	Sf_Run(&f, "SScorer", "SetSpeechText", "(Oiiii)", LANG("Mission_Checkin_RemoteMessage2"), 3, 220, 235, 255);
	Sf_Own(&f, MD, "SetWaypoint", "(i)", 1);
	Sf_EntSetS(&f, "DM_Door_OrbitDoorA_02", "OnObjSlot", "!FrontDoorSlot");
	return Sf_None(&f);
}

/* DecontaminatedWaypoint(): marked as decontaminated, a way point behind the decontamination */
static PyObject *Decont_DecontaminatedWaypoint(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Orbit.Decontaminated", "yes");
	Sf_Run(&f, "SaveGame", "MarkWayPoint", "(ss)", "Levels/Orbit", "DM_Player_Spawn_Decontamination");
	return Sf_None(&f);
}

/* SetWaypoint(Decontaminated): the functionary as the target of the check in, or not yet */
static PyObject *Decont_SetWaypoint(PyObject *self, PyObject *args) {
	PyObject *dec;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &dec)) {
		return NULL;
	}
	PyObject *e = ENT("Functionary_00");
	int on = Sf_True(&f, dec);
	if (on >= 0) {
		Sf_SetAttrI(&f, e, "RadarShow", on);
		Sf_SetAttrI(&f, e, "ArrowCanUse", on);
		Sf_SetAttrI(&f, e, "ArrowCanMission", on);
		if (on) {
			Sf_SetAttr(&f, e, "ArrowModMission", Sf_Ask(&f, "Scrap", "Get", "(s)", "ArrowMissionFile"));
		}
		Sf_Import(&f, "SaveGame");
		Sf_Run(&f, "SaveGame", "MMDesc", "(si)", on ? "Mission_Checkin_MissionText1" : "Mission_Decontamination_MissionText1", 1);
	}
	return Sf_None(&f);
}

static const char *const decont_scene[] = {
	"Scene.Load|Decontamination",
	"Scene.AddGeneric|100|import CreditsText;CreditsText.ShowCredit(1)",
	"Scene.AddText|50|Mission_Decontamination_RemoteMessage2|30",
	"Scene.AddText|250|Mission_Decontamination_RemoteMessage3|30",
	"Scene.AddText|550|Mission_Decontamination_RemoteMessage4|30",
	"Scene.AddGeneric|110|SInput.Rumble(0,0.5,0,4)",
	"Scene.AddGeneric|250|SInput.Rumble(0,0,0.5,8)",
	"Scene.ActorAction|0|Dtritus|Decon",
	"Scene.ActorAction|595|Dtritus|Walk",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Walk|Walk|1",
	"Scene.CreateLocalAction|Dtritus|0|Decon|Decon|0",
	"SAct.SetCls|DefAction|Decon",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* LoadDecontamination(): the scene of the decontamination and its effect */
static PyObject *Decont_LoadDecontamination(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "MakeChar");
	Sf_Calls(&f, MD, decont_scene);
	Ob_SceneFx(&f, MD, "FXSceneDecontaminationFile", "FXSCENEDECONTAMINATION");
	return Sf_None(&f);
}

/* the keypad entity name on the dummy pattern, turned to face the room, with its use */
static PyObject *Ob_Keypad(scriptFrame_t *f, const char *pattern, const char *name) {
	Sf_Run(f, "Scrap", "StartDummySearch", "(si)", pattern, 1);
	PyObject *dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
	PyObject *pos = Sf_Item(f, dummy, 1);
	PyObject *x = Sf_Item(f, pos, 0), *y = Sf_Item(f, Sf_Item(f, dummy, 1), 1), *z = Sf_Item(f, Sf_Item(f, dummy, 1), 2);
	PyObject *kb = Sf_Ask(f, "Scrap", "CreateEntity", "(sOOOs)", name, x, y, z, "WithLife");
	PyObject *a = Sf_OpI(f, Sf_Item(f, Sf_Item(f, dummy, 2), 1), '+', 180);
	Sf_SetAttr(f, kb, "Ang", Sf_Build(f, "(iOi)", 0, a, 0));
	return kb;
}

/* Init(LevelPath): the start of the mission, before or after the decontamination */
static PyObject *Decont_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Import(&f, "CreditsText");
	Sf_Run(&f, "CreditsText", "Init", "()");
	Sf_EntSetS(&f, "DM_Door_ElevatorOrbitC_TownHall", "OnObjSlot", "");
	int mark = f.n;
	PyObject *dec = Sf_Int(&f, Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Orbit.Decontaminated"), "yes"));
	int d = Sf_True(&f, dec);
	if (d == 0) {
		Ob_Arrow(&f, ENT("GateKeeper_Portalis"), 1);
		Sf_Import(&f, "Scorer");
		Ob_Msg(&f, LANG("Mission_Decontamination_SystemMessage1"), 50);
		Sf_Later(&f, Sf_Int(&f, 2), Sf_Mod(&f, "Scorer", "Blink"), Sf_Build(&f, "(i)", 20), NULL);
		Sf_Import(&f, "Items");
		PyObject *fn = Sf_Mod(&f, "Items", "Hint");
		PyObject *text = Ob_KeyText(&f, "Mission_Decontamination_SystemMessage5", "Use");
		Sf_Call(&f, fn, "((iii)Oii)", -18893, -654, -25042, text, 5, 0);
	}
	Sf_Own(&f, MD, "LoadBishopsScene", "()");
	Sf_Own(&f, MD, "LoadFunctionaryScene", "()");
	Sf_Own(&f, MD, "LoadFallingScene", "()");
	Sf_Own(&f, MD, "CreateBishopsLauncher", "()");
	Sf_Own(&f, MD, "SetWaypoint", "(O)", dec);
	if (d > 0) {
		Sf_EntSetS(&f, "DM_Door_OrbitDoorA_01", "OnObjSlot", "!FrontDoorSlot");
		Sf_EntSetS(&f, "DM_Door_OrbitDoorA_02", "OnObjSlot", "!FrontDoorSlot");
	} else if (d == 0) {
		PyObject *kb = Ob_Keypad(&f, "DM_Keypad_Decontamination", "Decontamination_keyboard");
		Sf_Method(&f, kb, "SetModel", "(O)", Sf_Cat(&f, "Os", Sf_Mod(&f, "Init", "Path"), "/Map/DecoActive/DecoActive.m3d"));
		Sf_SetAttrI(&f, kb, "VRadius", 60);
		Sf_SetAttrI(&f, kb, "ArrowCanUse", 1);
		Sf_SetAttrI(&f, kb, "Life", 0);
		Sf_SetAttrI(&f, kb, "ArrnOpt", 0);
		Sf_SetAttr(&f, kb, "ArrOpt0", Sf_Cat(&f, "sO", "Decontamination:", LANG("Mission_Decontamination_Hint1")));
		Sf_SetAttrS(&f, kb, "UseFunc", "MapDecont.Decontamination");
		Sf_SetAttrI(&f, kb, "MapCollision", 1);
		Sf_Own(&f, MD, "LoadDecontamination", "()");
		Sf_EntSetS(&f, "DM_Door_OrbitDoorA_01", "OnObjSlot", "!FrontDoorSlot");
		Sf_EntSetS(&f, "DM_Door_OrbitDoorA_02", "OnObjSlot", "!CloseDoorSlot");
		Sf_Later(&f, Sf_Int(&f, 5), Sf_Global(&f, MD, "MsgOffVoice"), Sf_Build(&f, "()"), NULL);
	}
	Sf_Release(&f, mark);
	Sf_Import(&f, "GateKeeper");
	PyObject *fn = Sf_Mod(&f, "GateKeeper", "SetDoor");
	Sf_Call(&f, fn, "(ssOO)", "GateKeeper_Portalis", "DM_Door_OrbitDoorA_00", Sf_Global(&f, MD, "OpenGateKeeperDoor"),
		Sf_Global(&f, MD, "CloseGateKeeperDoor"));
	Sf_EntSetI(&f, "DM_Door_Teleporter_01", "ArrnOpt", 0);
	Sf_EntSetS(&f, "DM_Door_Teleporter_01", "UseFunc", "MapDecont.CantUseElevator");
	Sf_Import(&f, "Items");
	fn = Sf_Mod(&f, "Items", "Hint");
	PyObject *pos = Sf_Attr(&f, ENT("DM_Door_Teleporter_01"), "Pos");
	PyObject *text = LANG("Mission_Decontamination_SystemMessage6");
	Sf_Call(&f, fn, "(OOiiO)", pos, text, 5, 1, Sf_Global(&f, MD, "SetFirstInfoPointAsMission"));
	Sf_EntSetS(&f, "DM_Door_OrbitDoorA_03", "OnObjSlot", "");
	return Sf_None(&f);
}

/* MsgOffVoice(): the voice of the decontamination every 20 s until it is done */
static PyObject *Decont_MsgOffVoice(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	if (Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Orbit.Decontaminated"), "no") > 0) {
		Sf_Run(&f, "Speech", "OffVoice", "(s)", "Mission_Decontamination_RemoteMessage1");
		Sf_Later(&f, Sf_Int(&f, 20), Sf_Global(&f, MD, "MsgOffVoice"), Sf_Build(&f, "()"), NULL);
	}
	return Sf_None(&f);
}

/* SetFirstInfoPointAsMission(itemname): the keypad becomes the target instead of the teleporter */
static PyObject *Decont_SetFirstInfoPointAsMission(PyObject *self, PyObject *args) {
	PyObject *item;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &item)) {
		return NULL;
	}
	Ob_Arrow(&f, ENT("DM_Door_Teleporter_00"), 0);
	Ob_Arrow(&f, ENT("Decontamination_keyboard"), 1);
	return Sf_None(&f);
}

/* CantUseElevator(UsrName, ElevatorName, optId, FullOption) */
static PyObject *Decont_CantUseElevator(PyObject *self, PyObject *args) {
	PyObject *usr, *el, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &el, &opt, &full)) {
		return NULL;
	}
	Ob_Msg(&f, LANG("Mission_Decontamination_SystemMessage7"), 5);
	return Sf_None(&f);
}

/* CloseGateKeeperDoor(Door, GateKeeperName): closed, a hint near the gatekeeper */
static PyObject *Decont_CloseGateKeeperDoor(PyObject *self, PyObject *args) {
	PyObject *door, *gk;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &door, &gk)) {
		return NULL;
	}
	Sf_SetAttrS(&f, door, "OnObjSlot", "!CloseDoorSlot");
	Sf_Import(&f, "Items");
	PyObject *fn = Sf_Mod(&f, "Items", "Hint");
	Sf_Call(&f, fn, "((iii)Oii)", -16501, -652, -24098, Ob_KeyText(&f, "Mission_Decontamination_SystemMessage2", "Use"), 5, 1);
	return Sf_None(&f);
}

/* OpenGateKeeperDoor(Door, GateKeeperName): open, the teleporter becomes the target */
static PyObject *Decont_OpenGateKeeperDoor(PyObject *self, PyObject *args) {
	PyObject *door, *gk;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &door, &gk)) {
		return NULL;
	}
	Sf_SetAttrS(&f, door, "OnObjSlot", "!AutoDoorSlot");
	Ob_Arrow(&f, ENT("GateKeeper_Portalis"), 0);
	Ob_Arrow(&f, ENT("DM_Door_Teleporter_00"), 1);
	Ob_Msg(&f, Sf_Str(&f, ""), 0);
	PyObject *when = Sf_OpI(&f, Sf_Time(&f), '+', 4);
	PyObject *fn = Sf_Mod(&f, "SScorer", "SetMsgText");
	PyObject *text = LANG("Mission_Decontamination_SystemMessage4");
	Sf_Schedule(&f, when, fn, Sf_Build(&f, "(OO)", text, Sf_OpI(&f, Sf_Time(&f), '+', 10)), NULL);
	return Sf_None(&f);
}

/* AfterBrowseNPC(): the callbacks of the special characters */
static PyObject *Decont_AfterBrowseNPC(PyObject *self, PyObject *args) {
	static const char *const pairs[][2] = { { "GateKeeper_Portalis", "CreateGateKeeperInit" }, { "Sentinel_Corridor", "CreateSentinelInit" },
		{ "Bishop_01", "CreateBishopInit" }, { "Bishop_00", "CreateBishopInit" }, { "Functionary_00", "CreateFunctionaryInit" } };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "CharsNPC");
	for (int i = 0; i < 5 && !f.failed; i++) {
		int mark = f.n;
		PyObject *res = Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, pairs[i][0]));
		Sf_Method(&f, Sf_Attr(&f, res, "CallbackFunc"), "append", "(O)", Sf_Global(&f, MD, pairs[i][1]));
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* CreateFunctionaryInit(e) */
static PyObject *Decont_CreateFunctionaryInit(PyObject *self, PyObject *args) {
	static const char *const vars[] = { "ArrowCanUse=1", "UseFunc=MapDecont.PlayFunctionary", "LLogicProbTalkWhenWalking=1.0",
		"LLogicKeepWorking=1", NULL };
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttrs(&f, e, MD, vars);
	return Sf_None(&f);
}

/* CreateBishopInit(e) of MapDecont, CreateFixedInit(e) of GamblinDen's Map: the character stands where it is */
static const char *const fixed_vars[] = { "LLogicDefaultState=6", "LLogicDefaultTarget='", "LLogicState=6", "LLogicTarget='",
	"LLogicIdle=1", "LLogicIdle=0", NULL };

PyObject *Level_FixedInit(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttrs(&f, e, MD, fixed_vars);
	return Sf_None(&f);
}

static PyObject *Decont_CreateGateKeeperInit(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttrS(&f, e, "UseFunc", "MapDecont.GateKeeperSpeech");
	return Sf_None(&f);
}

static PyObject *Decont_CreateSentinelInit(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttrS(&f, e, "UseFunc", "MapDecont.SentinelSpeech");
	return Sf_None(&f);
}

/* ShowSkipConvsMsg(): how to skip the talks */
static PyObject *Decont_ShowSkipConvsMsg(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Ob_Msg(&f, Ob_KeyText(&f, "Mission_Decontamination_SystemMessage8", "Use"), 10);
	return Sf_None(&f);
}

/* GateKeeperSpeech(UsrName, CharName, optId, FullOption): the gatekeeper of the entrance and its door */
static PyObject *Decont_GateKeeperSpeech(PyObject *self, PyObject *args) {
	PyObject *usrName, *charName, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usrName, &charName, &opt, &full)) {
		return NULL;
	}
	static const char *const mods[] = { "Speech", "Chars", "Menu", "string", "GateKeeper", NULL };
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	PyObject *ch = Sf_Entity(&f, charName);
	Sf_Entity(&f, usrName);
	Sf_SpeechNames(&f, usrName, charName);
	PyObject *gc = Sf_GetItem(&f, Sf_Mod(&f, "GateKeeper", "GateKeepers"), charName);
	PyObject *door = Sf_Entity(&f, Sf_Attr(&f, gc, "Door"));
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		PyObject *cad = LANG("Mission_Decontamination_GateKeeper_InitMsg1");
		PyObject *t = Sf_Time(&f);
		PyObject *when = Sf_Op(&f, t, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", cad));
		Sf_Schedule(&f, when, Sf_Global(&f, MD, "ShowSkipConvsMsg"), Sf_Build(&f, "()"), NULL);
		Sf_Run(&f, "Speech", "OnSpch", "(sss)", "Mission_Decontamination_GateKeeper_InitMsg1", "Mission_Decontamination_GateKeeper_InitAns1", "TalkNPC");
		Sf_Run(&f, "Speech", "StdAnsLst", "([ssss])", "Mission_Decontamination_GateKeeper_1", "Mission_Decontamination_GateKeeper_2",
			"Mission_Decontamination_GateKeeper_3", "Mission_Decontamination_GateKeeper_4");
		Sf_Own(&f, MD, "CheckShowNextHowto", "()");
	} else if (!f.failed && Sf_EqS(&f, opt, "Mission_Decontamination_GateKeeper_3") > 0) {
		Sf_Run(&f, "Speech", "OnSpch", "(sss)", "Mission_Decontamination_GateKeeper_3Msg", "Mission_Decontamination_GateKeeper_3Ans", "TalkNoNPC");
		Sf_SetAttrI(&f, ch, "ArriOpt", 3);
	} else if (!f.failed && Sf_EqS(&f, opt, "Mission_Decontamination_GateKeeper_4") > 0) {
		Sf_SetAttr(&f, ch, "ArrOpt3", Sf_Cat(&f, "sO", "AllreadyOpen:", LANG("Mission_Decontamination_GateKeeper_4Opt")));
		Sf_Run(&f, "Speech", "OnSpch", "(sss)", "Mission_Decontamination_GateKeeper_4Msg", "Mission_Decontamination_GateKeeper_4Ans", "TalkNPC");
		Sf_SetMod(&f, "Speech", "OnEndOfSpeech", Sf_Mod(&f, "GateKeeper", "EndOfSpeech"));
		Sf_SetMod(&f, "GateKeeper", "LocalCallFunc", Sf_Attr(&f, gc, "CanPassFunc"));
		Sf_SetMod(&f, "GateKeeper", "LocalParams", Sf_Build(&f, "(OO)", door, charName));
		Sf_SetAttrS(&f, ch, "AbortSelectFunc", "GateKeeper.RestoreState");
	} else if (!f.failed && Sf_EqS(&f, opt, "AllreadyOpen") > 0) {
		Sf_Run(&f, "Speech", "OnSpch", "(sss)", "Mission_Decontamination_GateKeeper_4Msg", "Generics_GateKeeper_InitAns1", "TalkNPC");
		Sf_SetMod(&f, "Speech", "OnEndOfSpeech", Sf_Mod(&f, "GateKeeper", "EndOfSpeech"));
		Sf_Call(&f, Sf_Attr(&f, gc, "CanPassFunc"), "(OO)", door, charName);
		Sf_SetAttrS(&f, ch, "AbortSelectFunc", "GateKeeper.RestoreState");
	} else if (!f.failed) {
		Sf_Run(&f, "Speech", "StdOnSpch", "(O)", opt);
	}
	return Sf_None(&f);
}

/* SentinelSpeech(UsrName, CharName, optId, FullOption): the sentinel of the corridor; its second answer shows the
 * cop meter */
static PyObject *Decont_SentinelSpeech(PyObject *self, PyObject *args) {
	PyObject *usrName, *charName, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usrName, &charName, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	Sf_SpeechNames(&f, usrName, charName);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Sf_Run(&f, "Speech", "OnSpch", "(ss)", "Mission_Checkin_Sentinel_InitMsg1", "Mission_Checkin_Sentinel_InitAns1");
		Sf_Run(&f, "Speech", "StdAnsLst", "([ss])", "Mission_Checkin_Sentinel_1", "Mission_Checkin_Sentinel_2");
	} else if (!f.failed && Sf_EqS(&f, opt, "Mission_Checkin_Sentinel_2") > 0) {
		Sf_Run(&f, "Speech", "StdOnSpch", "(O)", opt);
		Sf_Later(&f, Sf_Int(&f, 8), Sf_Global(&f, MD, "GaugeBlink"), Sf_Build(&f, "()"), NULL);
	} else if (!f.failed) {
		Sf_Run(&f, "Speech", "StdOnSpch", "(O)", opt);
	}
	return Sf_None(&f);
}

/* GaugeBlink(): while the sentinel still talks, the alarm rises and the cop meter blinks */
static PyObject *Decont_GaugeBlink(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Scorer");
	PyObject *act = Sf_Attr(&f, ENT("Sentinel_Corridor"), "Action");
	if (Sf_EqS(&f, Sf_Slice(&f, act, 0, 7), "TalkNPC") > 0) {
		Sf_Run(&f, "Scrap", "SetAlarm", "(d)", 0.9);
		Sf_Run(&f, "Scorer", "Blink", "(is)", 30, "CopMeter");
	}
	return Sf_None(&f);
}

/* CheckShowNextHowto(): once the player has the gatekeeper selected with more than one option, how to pick the next */
static PyObject *Decont_CheckShowNextHowto(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (Sf_Compare(&f, Sf_Attr(&f, ENT("GateKeeper_Portalis"), "ArrnOpt"), "<=", Sf_Int(&f, 1)) != 0) {
		return Sf_None(&f);
	}
	Sf_Import(&f, "Scorer");
	PyObject *e = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, e) > 0) {
		int sel = Sf_EqS(&f, Sf_Attr(&f, e, "SelectedEntity"), "GateKeeper_Portalis");
		if (sel > 0) {
			sel = Sf_True(&f, Sf_Attr(&f, e, "AutoSelect"));
		}
		if (sel > 0) {
			Ob_Msg(&f, Ob_KeyText(&f, "Mission_Decontamination_SystemMessage3", "NextAction"), 10);
			Sf_Later(&f, Sf_Int(&f, 1), Sf_Mod(&f, "Scorer", "Blink"), Sf_Build(&f, "(is)", 20, "Talk"), NULL);
			return Sf_None(&f);
		}
	}
	LATER(0.25, Sf_Global(&f, MD, "CheckShowNextHowto"), Sf_Build(&f, "()"));
	return Sf_None(&f);
}

/* CreateBishopsLauncher(): the item that starts the scene of the bishops */
static PyObject *Decont_CreateBishopsLauncher(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "BishopsLauncher", -19800, 1150, -31100, "Item");
	Sf_SetAttrI(&f, l, "Radius", 400);
	Sf_SetAttrS(&f, l, "OnTake", "MapDecont.PlayBishops");
	return Sf_None(&f);
}

/* EndOfSceneBishops(usr): the player after the scene, the bishops talk, infinite lives, the back door closes */
static PyObject *Decont_EndOfSceneBishops(PyObject *self, PyObject *args) {
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	Sf_SetAttr(&f, usr, "Pos", Sf_Build(&f, "(iii)", -20423, 1046, -31145));
	Sf_SetAttr(&f, usr, "Ang", Sf_Build(&f, "(iii)", 0, 90, 0));
	Sf_Run(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, usr, "Name"));
	Sf_Import(&f, "Bishop");
	Sf_SetMod(&f, "Bishop", "skipChallengeOptions", Sf_Int(&f, 1));
	Sf_EntSetS(&f, "Bishop_01", "UseFunc", "Speech.AutoSpeech");
	Sf_EntSetS(&f, "Bishop_00", "UseFunc", "Speech.AutoSpeech");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Player.NumLives", "0");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Player.InfiniteLives", "1");
	PyObject *door = ENT("DM_Door_OrbitDoorA_03");
	Sf_SetAttrS(&f, door, "ForceAction", "Close");
	Sf_SetAttrS(&f, door, "ForceDefLoop", "Closed");
	return Sf_None(&f);
}

static const char *const bishops_scene[] = {
	"Scene.Load|Bishops",
	"Scene.ObjAction|0|DM_Door_OrbitDoorA_03|Open|Opened",
	"Scene.AddGeneric|50|import CreditsText;CreditsText.ShowCredit(2)",
	"Scene.AddGeneric|425|import CreditsText;CreditsText.ShowCredit(3)",
	"Scene.AddGeneric|850|import CreditsText;CreditsText.ShowCredit(4)",
	"Scene.AddGeneric|1300|import CreditsText;CreditsText.ShowCredit(5)",
	"Scene.ActorAction|0|Dtritus|Walk",
	"Scene.ActorAction|70|Dtritus|Hi",
	"Scene.ActorAction|130|Dtritus|RelaxLeft",
	"Scene.ActorAction|158|Dtritus|ScaredOK",
	"Scene.ActorAction|365|Dtritus|Talk2Left",
	"Scene.ActorAction|415|Dtritus|Talk1Left",
	"Scene.ActorAction|485|Dtritus|RelaxLeft",
	"Scene.ActorAction|710|Dtritus|Talk1Left",
	"Scene.ActorAction|775|Dtritus|LeftToRight",
	"Scene.ActorAction|804|Dtritus|RelaxRight",
	"Scene.ActorAction|990|Dtritus|RightToLeft",
	"Scene.ActorAction|1019|Dtritus|RelaxLeft",
	"Scene.ActorAction|1165|Dtritus|Talk2Left",
	"Scene.ActorAction|1200|Dtritus|Talk1Left",
	"Scene.ActorAction|1250|Dtritus|Talk2Left",
	"Scene.ActorAction|1295|Dtritus|LeftToRight",
	"Scene.ActorAction|1324|Dtritus|RelaxRight",
	"Scene.ActorAction|1510|Dtritus|RightToLeft",
	"Scene.ActorAction|1539|Dtritus|RelaxLeft",
	"Scene.ActorAction|1628|Dtritus|WalkHi",
	"Scene.ActorAction|0|Bishop1|Walk",
	"Scene.ActorAction|60|Bishop1|WalkEnd",
	"Scene.ActorAction|90|Bishop1|Hi",
	"Scene.ActorAction|126|Bishop1|Pray",
	"Scene.ActorAction|505|Bishop1|MorphLips",
	"Scene.ActorAction|505|Bishop1|Talk",
	"Scene.ActorAction|554|Bishop1|Talk0",
	"Scene.ActorAction|600|Bishop1|Talk1",
	"Scene.ActorAction|635|Bishop1|Talk3",
	"Scene.ActorBreakChannel|700|Bishop1|3",
	"Scene.ActorAction|700|Bishop1|Relax",
	"Scene.ActorAction|945|Bishop1|Shoulder1",
	"Scene.ActorAction|1020|Bishop1|MorphLips",
	"Scene.ActorAction|1000|Bishop1|Talk3",
	"Scene.ActorBreakChannel|1155|Bishop1|3",
	"Scene.ActorAction|1155|Bishop1|Relax",
	"Scene.ActorAction|1510|Bishop1|MorphLips",
	"Scene.ActorAction|1510|Bishop1|Talk1",
	"Scene.ActorAction|1580|Bishop1|Talk0",
	"Scene.ActorBreakChannel|1630|Bishop1|3",
	"Scene.ActorAction|1630|Bishop1|Hi",
	"Scene.ActorAction|0|Bishop2|Walk",
	"Scene.ActorAction|75|Bishop2|WalkEnd",
	"Scene.ActorAction|100|Bishop2|Hi",
	"Scene.ActorAction|130|Bishop2|Pray",
	"Scene.ActorAction|725|Bishop2|Shoulder3",
	"Scene.ActorAction|796|Bishop2|MorphLips",
	"Scene.ActorAction|796|Bishop2|Talk0",
	"Scene.ActorAction|845|Bishop2|Talk",
	"Scene.ActorAction|894|Bishop2|Talk2",
	"Scene.ActorAction|925|Bishop2|Talk0",
	"Scene.ActorBreakChannel|990|Bishop2|3",
	"Scene.ActorAction|990|Bishop2|Relax",
	"Scene.ActorAction|1240|Bishop2|Shoulder3",
	"Scene.ActorAction|1310|Bishop2|MorphLips",
	"Scene.ActorAction|1310|Bishop2|Talk3",
	"Scene.ActorBreakChannel|1500|Bishop2|3",
	"Scene.ActorAction|1500|Bishop2|Relax",
	"Scene.ActorAction|1580|Bishop2|Hi",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame126",
	"SAct.SetAct|EventName|FXBishopsPray1",
	"SAct.SetAct|EventFunc|MapDecont.FXBishopsPray",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame130",
	"SAct.SetAct|EventName|FXBishopsPray2",
	"SAct.SetAct|EventFunc|MapDecont.FXBishopsPray",
	"Scene.AddGeneric|220|SInput.Rumble(0,0.5,0,1)",
	"Scene.AddText|365|Mission_Scene_Checkin_Bishop_1|4",
	"Scene.AddText|505|Mission_Scene_Checkin_Bishop_2|7",
	"Scene.AddText|710|Mission_Scene_Checkin_Bishop_3|2",
	"Scene.AddText|796|Mission_Scene_Checkin_Bishop_4|7",
	"Scene.AddText|1020|Mission_Scene_Checkin_Bishop_5|4",
	"Scene.AddText|1165|Mission_Scene_Checkin_Bishop_6|4",
	"Scene.AddText|1310|Mission_Scene_Checkin_Bishop_7|7",
	"Scene.AddText|1510|Mission_Scene_Checkin_Bishop_8|4",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"Scene.CreateLocalAction|Dtritus|0|RightToLeft|RightToLeft|0",
	"SAct.SetAct|InterpTime|0.5",
	"Scene.CreateLocalAction|Dtritus|0|LeftToRight|LeftToRight|0",
	"SAct.SetAct|InterpTime|0.5",
	"Scene.CreateLocalAction|Dtritus|0|RelaxRight|RelaxRight|1",
	"Scene.CreateLocalAction|Dtritus|0|RelaxLeft|RelaxLeft|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Walk|Walk|1",
	"Scene.CreateLocalAction|Dtritus|0|Talk1Right|Talk1Right|1",
	"Scene.CreateLocalAction|Dtritus|0|Talk1Left|Talk1Left|1",
	"Scene.CreateLocalAction|Dtritus|0|Talk2Right|Talk2Right|1",
	"Scene.CreateLocalAction|Dtritus|0|Talk2Left|Talk2Left|1",
	"Scene.CreateLocalAction|Dtritus|0|ScaredOK|ScaredOK|0",
	"Scene.CreateLocalAction|Dtritus|0|Hi|Hi|0",
	"Scene.CreateLocalAction|Dtritus|0|WalkHi|WalkHi|0",
	"SAct.SetCls|DefAction|Walk",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"Scene.AddCharActor|Bishop1|Bishop",
	"MakeChar.CreateRelaxAction|Bishop|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Bishop|0|WalkEnd|WalkEnd|0",
	"MakeChar.CreateRelaxAction|Bishop|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Bishop|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|Bishop|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|Bishop|0|Talk2|Talk2|1",
	"Scene.CreateLocalAction|Bishop|0|Talk3|Talk3|1",
	"Scene.CreateLocalAction|Bishop|0|Hi|Hi|0",
	"Scene.CreateLocalAction|Bishop|0|Pray|Pray|0",
	"Scene.CreateLocalAction|Bishop|0|Shoulder1|Shoulder1|0",
	"Scene.SetDefSounds|Bishop1|Bishop",
	"Scene.AddCharActor|Bishop2|Bishop",
	"MakeChar.CreateRelaxAction|Bishop|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Bishop|0|WalkEnd|WalkEnd|0",
	"MakeChar.CreateRelaxAction|Bishop|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Bishop|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|Bishop|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|Bishop|0|Talk2|Talk2|1",
	"Scene.CreateLocalAction|Bishop|0|Talk3|Talk3|1",
	"Scene.CreateLocalAction|Bishop|0|Hi|Hi|0",
	"Scene.CreateLocalAction|Bishop|0|Pray|Pray|0",
	"Scene.CreateLocalAction|Bishop|0|Shoulder3|Shoulder3|0",
	"Scene.SetDefSounds|Bishop2|Bishop",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* LoadBishopsScene(): the scene of the two bishops and its effect */
static PyObject *Decont_LoadBishopsScene(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "MakeChar");
	Sf_Calls(&f, MD, bishops_scene);
	Ob_SceneFx(&f, MD, "FXSceneBishopsFile", "FXSCENEBISHOPS");
	return Sf_None(&f);
}

/* FXBishopsPray(EntityName, EventName, Time): the effect of a bishop selling a life to Dtritus */
static PyObject *Decont_FXBishopsPray(PyObject *self, PyObject *args) {
	PyObject *ent, *ev, *t;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ent, &ev, &t)) {
		return NULL;
	}
	Sf_Run(&f, "Scrap", "SetCallFunc", "(s)", "!FXBishopSellLife");
	Sf_Run(&f, "Scrap", "AddParams", "(O)", Sf_Cat(&f, "sO", "BishopsBishop", Sf_Item(&f, ev, -1)));
	Sf_Run(&f, "Scrap", "AddParams", "(s)", "BishopsDtritus");
	Sf_Run(&f, "Scrap", "Execute", "()");
	return Sf_None(&f);
}

/* PlayBishops(ItemName=None, CharName=None): the scene of the bishops, started a moment later (an item cannot hide
 * cars or characters) */
static PyObject *Decont_PlayBishops(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "ItemName", "CharName", NULL };
	PyObject *item = Py_None, *ch = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|OO", names, &item, &ch)) {
		return NULL;
	}
	int go = Sf_Compare(&f, ch, "==", Py_None);
	if (go == 0) {
		go = Sf_Compare(&f, ch, "==", Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Name"));
	}
	if (go > 0) {
		if (Sf_True(&f, item) > 0) {
			Sf_Method(&f, Sf_Entity(&f, item), "Del", "()");
		}
		Sf_Import(&f, "Scene");
		Sf_EntSetS(&f, "DM_Door_OrbitDoorA_03", "OnObjSlot", "");
		Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, MD, "EndOfSceneBishops"));
		Sf_Schedule(&f, Sf_Time(&f), Sf_Mod(&f, "Scene", "Play"), Sf_Build(&f, "(s)", "Bishops"), NULL);
	}
	return Sf_None(&f);
}

static const char *const functionary_scene1[] = {
	"Scene.Load|Functionary",
	"Scene.AddGeneric|50|import CreditsText;CreditsText.ShowCredit( 6)",
	"Scene.AddGeneric|425|import CreditsText;CreditsText.ShowCredit( 7)",
	"Scene.AddGeneric|850|import CreditsText;CreditsText.ShowCredit( 8)",
	"Scene.AddGeneric|1300|import CreditsText;CreditsText.ShowCredit( 9)",
	"Scene.AddGeneric|1725|import CreditsText;CreditsText.ShowCredit(10)",
	"Scene.AddGeneric|2150|import CreditsText;CreditsText.ShowCredit(11)",
	"Scene.ActorAction|0|Functionary|Work",
	"Scene.ActorAction|70|Functionary|Talk1Up",
	"Scene.ActorAction|116|Functionary|RelaxUp",
	"Scene.ActorAction|170|Functionary|Talk1Up",
	"Scene.ActorAction|244|Functionary|Talk2Up",
	"Scene.ActorAction|318|Functionary|Talk1Up",
	"Scene.ActorAction|392|Functionary|Talk2Up",
	"Scene.ActorAction|466|Functionary|Talk1Up",
	"Scene.ActorAction|521|Functionary|RelaxUp",
	"Scene.ActorAction|590|Functionary|Talk1Up",
	"Scene.ActorAction|664|Functionary|Talk2Up",
	"Scene.ActorAction|738|Functionary|Talk1Up",
	"Scene.ActorAction|791|Functionary|Work",
	"Scene.ActorAction|840|Functionary|TalkWork",
	"Scene.ActorAction|1506|Functionary|Talk1Up",
	"Scene.ActorAction|1576|Functionary|RelaxUp",
	"Scene.ActorAction|1655|Functionary|Talk1Up",
	"Scene.ActorAction|1729|Functionary|Talk2Up",
	"Scene.ActorAction|1803|Functionary|Talk1Up",
	"Scene.ActorAction|1808|Functionary|RelaxUp",
	"Scene.ActorAction|1862|Functionary|Talk1Up",
	"Scene.ActorAction|1936|Functionary|Talk2Up",
	"Scene.ActorAction|2084|Functionary|GoOut",
	"Scene.ActorAction|2255|Functionary|Work",
	"Scene.ActorAction|2292|Functionary|RelaxDown",
	"Scene.ActorAction|2341|Functionary|Talk1Up",
	"Scene.ActorAction|2415|Functionary|Talk2Up",
	"Scene.ActorAction|2489|Functionary|Talk1Up",
	"Scene.ActorAction|2569|Functionary|Work",
	"@AddDF|0|LoopUp|LoopUp",
	"@AddDF|70|CineDown|CineLoop",
	"@AddDF|800|CineUp|LoopUp",
	"@AddDF|1525|CineDown|CineLoop",
	"@AddDF|2100|CineUp|LoopUp",
	"@AddDF|2350|CineDown|CineLoop",
	"@AddDF|2550|CineUp|LoopUp",
	"Scene.ActorAction|0|DownPanel|UpLoop|UpLoop",
	"Scene.ActorAction|1879|DownPanel|Down|Loop",
	"Scene.ActorAction|0|Dtritus|Relax",
	"Scene.ActorAction|30|Dtritus|Attention",
	"Scene.ActorAction|89|Dtritus|Relax",
	"Scene.ActorAction|120|Dtritus|Talk",
	"Scene.ActorAction|166|Dtritus|Relax",
	"Scene.ActorAction|525|Dtritus|Talk",
	"Scene.ActorAction|586|Dtritus|Relax",
	"Scene.ActorAction|1580|Dtritus|Talk",
	"Scene.ActorAction|1651|Dtritus|Relax",
	"Scene.ActorAction|1812|Dtritus|Talk",
	"Scene.ActorAction|1858|Dtritus|Relax",
	"Scene.ActorAction|2189|Dtritus|GoThink",
	"Scene.ActorAction|2259|Dtritus|Talk",
	"Scene.ActorAction|2337|Dtritus|Relax",
	"Scene.AddText|70|Mission_Scene_Checkin_Func_1|20",
	"Scene.AddText|120|Mission_Scene_Checkin_Func_2|20",
	"Scene.AddText|170|Mission_Scene_Checkin_Func_3|20",
	"Scene.AddText|350|Mission_Scene_Checkin_Func_4|20",
	"Scene.AddText|525|Mission_Scene_Checkin_Func_5|20",
	"Scene.AddText|590|Mission_Scene_Checkin_Func_6|6",
	"Scene.AddText|840|Mission_Scene_Checkin_Func_7|20",
	"Scene.AddText|1000|Mission_Scene_Checkin_Func_8|20",
	"Scene.AddText|1250|Mission_Scene_Checkin_Func_9|20",
	"Scene.AddText|1580|Mission_Scene_Checkin_Func_10|20",
	"Scene.AddText|1655|Mission_Scene_Checkin_Func_11|20",
	"Scene.AddText|1812|Mission_Scene_Checkin_Func_12|20",
	"Scene.AddText|1862|Mission_Scene_Checkin_Func_13|20",
	"Scene.AddText|2000|Mission_Scene_Checkin_Func_14|20",
	"Scene.AddText|2259|Mission_Scene_Checkin_Func_15|20",
	"Scene.AddText|2341|Mission_Scene_Checkin_Func_16|20",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddLocalActor|DownPanel|DownPanel",
	"Scene.CreateLocalAction|DownPanel|0|UpLoop|UpLoop|1",
	"Scene.CreateLocalAction|DownPanel|0|Loop|Loop|1",
	"Scene.CreateLocalAction|DownPanel|0|Up|Up|0",
	"Scene.CreateLocalAction|DownPanel|0|Down|Down|0",
	"SAct.SetCls|DefAction|UpLoop",
	"SAct.GetClass|BureaucracyDesktopFunctionary",
	"SAct.CreateAction|CineUp",
	"SAct.SetAct|AnmFilePath|Levels/Orbit/Map/Functionary/FunctionaryAnm/DesktopFunctionary_CineUp.M3D",
	"SAct.SetAct|isLoop|0",
	"SAct.CreateAction|CineDown",
	"SAct.SetAct|AnmFilePath|Levels/Orbit/Map/Functionary/FunctionaryAnm/DesktopFunctionary_CineDown.M3D",
	"SAct.SetAct|isLoop|0",
	"SAct.CreateAction|CineLoop",
	"SAct.SetAct|AnmFilePath|Levels/Orbit/Map/Functionary/FunctionaryAnm/DesktopFunctionary_CineLoop.M3D",
	"SAct.SetAct|isLoop|1",
	NULL,
};

static const char *const functionary_scene2[] = {
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk|Talk|1",
	"Scene.CreateLocalAction|Dtritus|0|Attention|Attention|0",
	"SAct.SetAct|InterpTime|0.3",
	"Scene.CreateLocalAction|Dtritus|0|GoThink|GoThink|0",
	"SAct.SetAct|InterpTime|0.5",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"Scene.AddCharActor|Functionary|Functionary",
	"MakeChar.CreateRelaxAction|Functionary|0|Work|Work|1",
	"Scene.CreateLocalAction|Functionary|0|GoOut|GoOut|1",
	"Scene.CreateLocalAction|Functionary|0|RelaxDown|RelaxDown|1",
	"Scene.CreateLocalAction|Functionary|0|RelaxUp|RelaxUp|1",
	"Scene.CreateLocalAction|Functionary|0|TalkWork|TalkWork|1",
	"Scene.CreateLocalAction|Functionary|0|Talk1Up|Talk1Up|1",
	"Scene.CreateLocalAction|Functionary|0|Talk2Up|Talk2Up|1",
	"Scene.SetDefSounds|Functionary|Functionary",
	"SAct.SetCls|DefAction|RelaxDown",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* LoadFunctionaryScene(): the scene of the check in at the functionary's desk, with the trap panel */
static PyObject *Decont_LoadFunctionaryScene(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "MakeChar");
	Sf_Calls(&f, MD, functionary_scene1);
	PyObject *df = ENT("DM_DesktopFunctionary_00");
	Sf_SetAttrS(&f, df, "ActCtrl", "BureaucracyDesktopFunctionary");
	Sf_SetAttrI(&f, df, "MapCollision", 1);
	Sf_Calls(&f, MD, functionary_scene2);
	return Sf_None(&f);
}

/* AddDF(frame, Action, DefLoop, EntityName="DM_DesktopFunctionary_00"): an event of the current action at the frame
 * that forces the animation of an entity */
static PyObject *Decont_AddDF(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "frame", "Action", "DefLoop", "EntityName", NULL };
	PyObject *frame, *act, *loop, *ent = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOO|O", names, &frame, &act, &loop, &ent)) {
		return NULL;
	}
	if (!ent) {
		ent = Sf_Str(&f, "DM_DesktopFunctionary_00");
	}
	PyObject *t = Sf_Ask(&f, "SAct", "GetAct", "(O)", Sf_Cat(&f, "sO", "TimeOfFrame", Sf_Repr(&f, frame)));
	Sf_Run(&f, "SAct", "SetAct", "(sO)", "AddEventAtTime", t);
	Sf_Run(&f, "SAct", "SetAct", "(sO)", "EventName", Sf_Cat(&f, "OsOsO", act, ",", loop, ",", ent));
	Sf_Run(&f, "SAct", "SetAct", "(ss)", "EventFunc", "MapDecont.SetDFAnm");
	return Sf_None(&f);
}

/* SetDFAnm(EntityName, EventName, Time): EventName is "action,loop,entity" */
static PyObject *Decont_SetDFAnm(PyObject *self, PyObject *args) {
	PyObject *ent, *ev, *t;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ent, &ev, &t)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *values = Sf_Ask(&f, "string", "split", "(Os)", ev, ",");
	PyObject *df = Sf_Entity(&f, Sf_Item(&f, values, 2));
	Sf_SetAttr(&f, df, "ForceAction", Sf_Item(&f, values, 0));
	Sf_SetAttr(&f, df, "ForceDefLoop", Sf_Item(&f, values, 1));
	return Sf_None(&f);
}

/* EndOfSceneFunctionary(usr): the functionary talks normally again, the keypad down to the town hall */
static PyObject *Decont_EndOfSceneFunctionary(PyObject *self, PyObject *args) {
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Functionary");
	Sf_Import(&f, "SaveGame");
	PyObject *c = ENT("Functionary_00");
	Sf_SetMod(&f, "Functionary", "skipChallengeOptions", Sf_Int(&f, 1));
	Sf_SetAttrS(&f, c, "UseFunc", "Speech.AutoSpeech");
	Sf_SetAttrI(&f, c, "ArrowCanMission", 0);
	Sf_SetAttrI(&f, c, "RadarShow", 0);
	Sf_Run(&f, "Scrap", "Set", "(sO)", "FunctionaryMinTimeToSitDown", Sf_Ask(&f, "Scrap", "Def", "(s)", "FunctionaryMinTimeToSitDown"));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "FunctionaryMaxTimeToSitDown", Sf_Ask(&f, "Scrap", "Def", "(s)", "FunctionaryMaxTimeToSitDown"));
	Sf_Own(&f, MD, "CreateDownKeyPad", "()");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_Fake_MissionText1", 0);
	return Sf_None(&f);
}

/* CreateDownKeyPad(): the keypad of the trap panel, the target of the mission */
static PyObject *Decont_CreateDownKeyPad(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *kb = Ob_Keypad(&f, "DM_Keypad_CineDown", "DownPanel_keyboard");
	Sf_SetAttrS(&f, kb, "ActCtrl", "FunctionaryDownPanel");
	Sf_SetAttrS(&f, kb, "Action", "Loop");
	Sf_SetAttrI(&f, kb, "VRadius", 60);
	Sf_SetAttrI(&f, kb, "ArrowCanUse", 1);
	Sf_SetAttrI(&f, kb, "Life", 0);
	Sf_SetAttrI(&f, kb, "ArrnOpt", 0);
	Sf_SetAttr(&f, kb, "ArrOpt0", Sf_Cat(&f, "sO", "GoDown:", LANG("TownHallGoto")));
	Sf_SetAttrS(&f, kb, "UseFunc", "MapDecont.PlayFalling");
	Sf_SetAttrI(&f, kb, "MapCollision", 1);
	Sf_SetAttrI(&f, kb, "ArrowCanMission", 1);
	Sf_SetAttr(&f, kb, "ArrowModMission", Sf_Ask(&f, "Scrap", "Get", "(s)", "ArrowMissionFile"));
	Sf_SetAttrI(&f, kb, "RadarShow", 1);
	return Sf_None(&f);
}

/* PlayFalling(UsrName, CharName, optId, FullOption): the fall through the trap panel */
static PyObject *Decont_PlayFalling(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	PyObject *e = ENT("DownPanel_keyboard");
	Sf_SetAttrI(&f, e, "ArrowCanMission", 0);
	Sf_SetAttrI(&f, e, "ArrowCanUse", 0);
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, MD, "JumpNextMission"));
	Sf_Run(&f, "Scene", "Play", "(s)", "Falling");
	return Sf_None(&f);
}

/* JumpNextMission(usr): the game proper starts in the town hall (Levels/Fake picks the start) */
static PyObject *Decont_JumpNextMission(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "SaveGame", "Init", "MissionsFuncs", "Elevator", NULL };
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Challenge", "1");
	Sf_Run(&f, "SaveGame", "MarkWayPoint", "(ss)", "Levels/Fake", "DM_Player_Spawn_Orbit");
	Sf_Run(&f, "SaveGame", "StartGame", "()");
	return Sf_None(&f);
}

/* PlayFunctionary(UsrName, CharName, optId, FullOption): the scene of the check in */
static PyObject *Decont_PlayFunctionary(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, MD, "EndOfSceneFunctionary"));
	Sf_Run(&f, "Scene", "Play", "(s)", "Functionary");
	return Sf_None(&f);
}

static const char *const falling_scene[] = {
	"Scene.Load|Falling",
	"Scene.ActorAction|0|Dtritus|Push",
	"Scene.ActorAction|40|Dtritus|Falling",
	"@AddDF|20|Up|UpLoop|DownPanel_keyboard",
	"@AddDF|28|Open|Opened|DM_Door_ElevatorOrbitC_TownHall",
	"Scene.AddText|110|Mission_Scene_Falling_Dtritus_1|30",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"Scene.CreateLocalAction|Dtritus|0|Push|Push|0",
	"Scene.CreateLocalAction|Dtritus|0|Falling|Falling|0",
	"SAct.SetCls|DefAction|Push",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* LoadFallingScene(): the scene of the fall; the back door of the entrance again */
static PyObject *Decont_LoadFallingScene(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "MakeChar");
	Sf_Calls(&f, MD, falling_scene);
	Sf_EntSetS(&f, "DM_Door_OrbitDoorA_03", "OnObjSlot", "!BackDoorSlot");
	return Sf_None(&f);
}

PyMethodDef g_OrbitMapDecontMethods[] = {
	{ "InitStatus", Decont_InitStatus, 1, NULL },
	{ "Decontamination", Decont_Decontamination, 1, NULL },
	{ "EndOfScene", Decont_EndOfScene, 1, NULL },
	{ "DecontaminatedWaypoint", Decont_DecontaminatedWaypoint, 1, NULL },
	{ "SetWaypoint", Decont_SetWaypoint, 1, NULL },
	{ "LoadDecontamination", Decont_LoadDecontamination, 1, NULL },
	{ "Init", Decont_Init, 1, NULL },
	{ "MsgOffVoice", Decont_MsgOffVoice, 1, NULL },
	{ "SetFirstInfoPointAsMission", Decont_SetFirstInfoPointAsMission, 1, NULL },
	{ "CantUseElevator", Decont_CantUseElevator, 1, NULL },
	{ "CloseGateKeeperDoor", Decont_CloseGateKeeperDoor, 1, NULL },
	{ "OpenGateKeeperDoor", Decont_OpenGateKeeperDoor, 1, NULL },
	{ "AfterBrowseNPC", Decont_AfterBrowseNPC, 1, NULL },
	{ "CreateFunctionaryInit", Decont_CreateFunctionaryInit, 1, NULL },
	{ "CreateBishopInit", Level_FixedInit, 1, NULL },
	{ "CreateGateKeeperInit", Decont_CreateGateKeeperInit, 1, NULL },
	{ "CreateSentinelInit", Decont_CreateSentinelInit, 1, NULL },
	{ "ShowSkipConvsMsg", Decont_ShowSkipConvsMsg, 1, NULL },
	{ "GateKeeperSpeech", Decont_GateKeeperSpeech, 1, NULL },
	{ "SentinelSpeech", Decont_SentinelSpeech, 1, NULL },
	{ "GaugeBlink", Decont_GaugeBlink, 1, NULL },
	{ "CheckShowNextHowto", Decont_CheckShowNextHowto, 1, NULL },
	{ "CreateBishopsLauncher", Decont_CreateBishopsLauncher, 1, NULL },
	{ "EndOfSceneBishops", Decont_EndOfSceneBishops, 1, NULL },
	{ "LoadBishopsScene", Decont_LoadBishopsScene, 1, NULL },
	{ "FXBishopsPray", Decont_FXBishopsPray, 1, NULL },
	{ "PlayBishops", (PyCFunction)(void (*)(void))Decont_PlayBishops, 3, NULL },
	{ "LoadFunctionaryScene", Decont_LoadFunctionaryScene, 1, NULL },
	{ "AddDF", (PyCFunction)(void (*)(void))Decont_AddDF, 3, NULL },
	{ "SetDFAnm", Decont_SetDFAnm, 1, NULL },
	{ "EndOfSceneFunctionary", Decont_EndOfSceneFunctionary, 1, NULL },
	{ "CreateDownKeyPad", Decont_CreateDownKeyPad, 1, NULL },
	{ "PlayFalling", Decont_PlayFalling, 1, NULL },
	{ "JumpNextMission", Decont_JumpNextMission, 1, NULL },
	{ "PlayFunctionary", Decont_PlayFunctionary, 1, NULL },
	{ "LoadFallingScene", Decont_LoadFallingScene, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_IntroHuman ---- */

/* InitStatus(LevelPath): the resources of the Gears and of Bill */
static PyObject *Intro_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Run(&f, "Chars", "AddNewCharRes", "(s)", "Gear");
	Sf_Run(&f, "Chars", "AddNewCharRes", "(s)", "Human");
	return Sf_None(&f);
}

/* the gatekeeper goes on (its time runs, active) */
static void Ih_GateKeeper(scriptFrame_t *f) {
	PyObject *gc = Sf_Ask(f, "Scrap", "GetEntity", "(s)", "GateKeeper_Portalis");
	Sf_SetAttr(f, gc, "TimeSpeed", Sf_Float(f, 1.0));
	Sf_SetAttrI(f, gc, "Active", 1);
}

/* Init(LevelPath): the doors open, the alarm on and locked, the three scenes of Bill's arrival */
static PyObject *Intro_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	for (int i = 0; i < 5 && !f.failed; i++) {
		int mark = f.n;
		PyObject *d = Sf_Entity(&f, Sf_Cat(&f, "sO", "DM_Door_OrbitDoorA_0", Sf_ReprI(&f, i)));
		Sf_SetAttrS(&f, d, "OnObjSlot", "");
		Sf_SetAttrS(&f, d, "Action", "Opened");
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 1);
	Sf_Own(&f, IH, "LoadSceneA", "()");
	Sf_Own(&f, IH, "LoadSceneB", "()");
	Sf_Own(&f, IH, "LoadSceneC", "()");
	Sf_Run(&f, "Scene", "Play", "(s)", "BillArrives_A");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, IH, "BillArrives_B"));
	Ih_GateKeeper(&f);
	return Sf_None(&f);
}

/* BillArrives_B(usr=None), BillArrives_C(usr=None): the next scene */
static PyObject *Intro_BillArrives_B(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Run(&f, "Scene", "Play", "(s)", "BillArrives_B");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, IH, "BillArrives_C"));
	Ih_GateKeeper(&f);
	return Sf_None(&f);
}

static PyObject *Intro_BillArrives_C(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Run(&f, "Scene", "Play", "(s)", "BillArrives_C");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, IH, "SetNextMission"));
	return Sf_None(&f);
}

/* SetNextMission(usr=None): on to Mission_IntroHuman2 in Press, Bill's ship parked there */
static PyObject *Intro_SetNextMission(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	static const char *const saves[][2] = { { "BankMasterAtBank", "1" }, { "Journalist.Humphrey_Defaut", "Press" },
		{ "Journalist.Betty_Defaut", "Press" }, { "Journalist.Berto_Defaut", "Press" } };
	PyObject *usr = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "SaveGame");
	Sf_Run(&f, "Scrap", "Achievement", "(i)", 5);
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_IntroHuman2", "Press");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
	Sf_Run(&f, "SaveGame", "MarkWayPoint", "(sss)", "Levels/Press", "DM_Player_Spawn_Press", "");
	for (int i = 0; i < 4; i++) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", saves[i][0], saves[i][1]);
	}
	PyObject *l = Sf_Eval(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Parked.Press"), IH);
	Sf_Method(&f, l, "append", "(s)", "SHUMP1");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Parked.Press", Sf_Repr(&f, l));
	Sf_Run(&f, "SaveGame", "StartGame", "()");
	return Sf_None(&f);
}

static const char *const intro_scene_a[] = {
	"Scene.Load|BillArrives_A",
	"Scene.ActorAction|0|Human|JumpPos",
	"Scene.ActorAction|530|Human|Jump",
	"Scene.ActorAction|0|Capsule|LoopA|LoopA",
	"Scene.ActorAction|330|Capsule|Landing|LoopA",
	"Scene.ActorAction|380|Capsule|LoopB|LoopA",
	"Scene.ActorAction|404|Capsule|GateOpen|LoopA",
	"Scene.ActorAction|488|Capsule|LoopC|LoopA",
	"Scrap.Set|DefInterpTime|0.0",
	"Scene.AddCharActor|Human|Human",
	"Scene.CreateLocalAction|Human|0|Jump|Jump|1",
	"Scene.CreateLocalAction|Human|0|JumpPos|JumpPos|1",
	"Scene.SetDefSounds|Human|Human",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|Capsule|Capsule",
	"Scene.CreateLocalAction|Capsule|0|GateOpen|GateOpen|1",
	"Scene.CreateLocalAction|Capsule|0|Landing|Landing|1",
	"Scene.CreateLocalAction|Capsule|0|LoopA|LoopA|1",
	"Scene.CreateLocalAction|Capsule|0|LoopB|LoopB|1",
	"Scene.CreateLocalAction|Capsule|0|LoopC|LoopC|1",
	"SAct.SetCls|DefAction|LoopA",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

static const char *const intro_scene_b[] = {
	"Scene.Load|BillArrives_B",
	"Scene.ActorAction|530|Human|TurnSlow",
	"Scene.ActorAction|550|Human|Relax0",
	"Scene.ActorAction|660|Human|WalkLook",
	"Scene.ActorAction|860|Human|Relax0",
	"Scene.ActorAction|960|Human|WalkLookSininter",
	"Scene.ActorAction|1090|Human|Think",
	"Scene.ActorAction|530|Capsule|LoopC|LoopC",
	"Scene.ObjAction|1000|DM_Door_OrbitDoorA_01|Close|Closed",
	"Scene.AddGeneric|710|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),1.0)",
	"Scene.AddGeneric|740|SScorer.Fade(0,(0,0,0,255),(0,0,0,0),1.0)",
	"Scene.AddGeneric|800|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),1.0)",
	"Scene.AddGeneric|830|SScorer.Fade(0,(0,0,0,255),(0,0,0,0),1.0)",
	"Scene.AddGeneric|930|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),1.0)",
	"Scene.AddGeneric|980|SScorer.Fade(0,(0,0,0,255),(0,0,0,0),1.0)",
	"Scene.AddText|650|Mission_Scene_IntroHuman_1|3",
	"Scene.AddText|760|Mission_Scene_IntroHuman_2|3",
	"Scene.AddText|1125|Mission_Scene_IntroHuman_3|3",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Human|Human",
	"MakeChar.CreateRelaxAction|Human|0|Relax0|Relax0|1",
	"Scene.CreateLocalAction|Human|0|Think|Think|1",
	"Scene.CreateLocalAction|Human|0|TurnSlow|TurnSlow|1",
	"Scene.CreateLocalAction|Human|0|WalkLook|WalkLook|1",
	"Scene.CreateLocalAction|Human|0|WalkLookSininter|WalkLookSininter|1",
	"Scrap.Set|DefInterpTime|0.0",
	"Scene.SetDefSounds|Human|Human",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|Capsule|Capsule",
	"Scene.CreateLocalAction|Capsule|0|LoopC|LoopC|1",
	"SAct.SetCls|DefAction|LoopC",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

static const char *const intro_scene_c[] = {
	"Scene.Load|BillArrives_C",
	"Scene.ObjAction|0|DM_Door_OrbitDoorA_01|Closed|Closed",
	"Scene.ActorAction|0|Human|DropCase",
	"Scene.ActorAction|40|Human|Descontamination",
	"Scene.ActorAction|280|Human|Alarm",
	"Scene.ActorAction|378|Human|TurnDuck",
	"Scene.ActorAction|395|Human|Alarm",
	"Scene.ActorAction|0|Case|Invisible|Invisible",
	"Scene.ActorAction|20|Case|Visible|Invisible",
	"Scene.ActorAction|0|Gear1|Run",
	"Scene.ActorAction|260|Gear1|SirenInit",
	"Scene.ActorAction|280|Gear1|Siren",
	"Scene.ActorAction|494|Gear1|RunEnd",
	"Scene.ActorAction|518|Gear1|Relax",
	"Scene.ActorAction|0|Gear2|Run",
	"Scene.ActorAction|260|Gear2|SirenInit",
	"Scene.ActorAction|280|Gear2|Siren",
	"Scene.ActorAction|510|Gear2|RunEnd",
	"Scene.ActorAction|534|Gear2|Relax",
	"Scene.ActorAction|540|Gear2|Talk",
	"Scene.AddGeneric|535|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),1.5)",
	"Scene.AddText|80|Mission_Scene_IntroHuman_4|3",
	"Scene.AddText|160|Mission_IntroHuman_RemoteMessage1|3",
	"Scene.AddText|260|Mission_IntroHuman_RemoteMessage2|3",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Human|Human",
	"Scene.CreateLocalAction|Human|0|Alarm|Alarm|1",
	"Scene.CreateLocalAction|Human|0|TurnDuck|TurnDuck|1",
	"Scene.CreateLocalAction|Human|0|DropCase|DropCase|1",
	"Scene.CreateLocalAction|Human|0|Descontamination|Descontamination|0",
	"Scene.SetDefSounds|Human|Human",
	"SAct.SetCls|DefAction|DropCase",
	"Scene.AddCharActor|Gear1|Gear",
	"MakeChar.CreateRelaxAction|Gear|0|Run|Run|1",
	"MakeChar.CreateRelaxAction|Gear|0|RunEnd|RunEnd|0",
	"Scene.SetDefSounds|Gear1|Gear",
	"MakeChar.CreateRelaxAction|Gear|0|SirenInit|SirenInit|0",
	"SAct.SetAct|Chanel|1",
	"SAct.SetAct|AddEventAtTime|0",
	"SAct.SetAct|EventName|FXPoliceAlarmInit",
	"SAct.SetAct|EventFunc|!FXPoliceAlarmInit",
	"MakeChar.CreateRelaxAction|Gear|0|Siren|Siren|1",
	"SAct.SetAct|Chanel|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|Gear2|Gear",
	"MakeChar.CreateRelaxAction|Gear|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Gear|0|Run|Run|1",
	"MakeChar.CreateRelaxAction|Gear|0|RunEnd|RunEnd|0",
	"Scene.SetDefSounds|Gear2|Gear",
	"MakeChar.CreateRelaxAction|Gear|0|SirenInit|SirenInit|0",
	"SAct.SetAct|Chanel|1",
	"SAct.SetAct|AddEventAtTime|0",
	"SAct.SetAct|EventName|FXPoliceAlarmInit",
	"SAct.SetAct|EventFunc|!FXPoliceAlarmInit",
	"MakeChar.CreateRelaxAction|Gear|0|Siren|Siren|1",
	"SAct.SetAct|Chanel|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|Case|Case",
	"Scene.CreateLocalAction|Case|0|Visible|Visible|1",
	"Scrap.Set|DefInterpTime|0.0",
	"Scene.CreateLocalAction|Case|0|Invisible|Invisible|1",
	"Scrap.Set|DefInterpTime|0.0",
	"SAct.SetCls|DefAction|Invisible",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* LoadSceneA(), LoadSceneB(), LoadSceneC(): the scenes of Bill's arrival (the capsule lands, Bill looks around, the
 * Gears come with the alarm), A and C with their effects */
static PyObject *Intro_LoadSceneA(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Calls(&f, IH, intro_scene_a);
	Ob_SceneFx(&f, IH, "FXSceneBillArrives_AFile", "FXSCENEBILLARRIVES_A");
	return Sf_None(&f);
}

static PyObject *Intro_LoadSceneB(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Calls(&f, IH, intro_scene_b);
	return Sf_None(&f);
}

static PyObject *Intro_LoadSceneC(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Calls(&f, IH, intro_scene_c);
	Ob_SceneFx(&f, IH, "FXSceneDecontaminationFile", "FXSCENEDECONTAMINATION");
	return Sf_None(&f);
}

PyMethodDef g_OrbitIntroHumanMethods[] = {
	{ "InitStatus", Intro_InitStatus, 1, NULL },
	{ "Init", Intro_Init, 1, NULL },
	{ "BillArrives_B", (PyCFunction)(void (*)(void))Intro_BillArrives_B, 3, NULL },
	{ "BillArrives_C", (PyCFunction)(void (*)(void))Intro_BillArrives_C, 3, NULL },
	{ "SetNextMission", (PyCFunction)(void (*)(void))Intro_SetNextMission, 3, NULL },
	{ "LoadSceneA", Intro_LoadSceneA, 1, NULL },
	{ "LoadSceneB", Intro_LoadSceneB, 1, NULL },
	{ "LoadSceneC", Intro_LoadSceneC, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
