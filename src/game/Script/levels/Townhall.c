/*
 * The scripts of the level Townhall in C (data02.packed levels/townhall/scripts, issue #2): the Map module (the NPCs,
 * the mayor and his gear, the elevators, the locked elevator to the orbit, the fake traffic), MapSnd.SndElements (the
 * sounds of the map elements; the rest of MapSnd is data) and the missions
 * Mission_ApologizeMayor (the staplers of the crazy mayor), Mission_CatchTrurl (the messenger and the blueprints of
 * the mayor's ship), Mission_SheriffMayor (the police boss sells his clues) and Mission_TalkToMayor (the money of
 * Corruptus and Porkins). The plain functions are data (data/levels/townhall/<module>.def): Mission_SheriffMayor.Init and
 * Mission_TalkToMayor.PreStealFunctionary.
 *
 * The module variables and the module code stay in the scripts: Map (Libraries, the discardable characters
 * _BankDirector, _Bishop, _Functionary), Mission_ApologizeMayor (MaxTargets, SMayorTaunt), Mission_SheriffMayor
 * (NextMissionData, MoneyReward, alreadyTalked), Mission_TalkToMayor (MAX_TAMAYO_MONEY, MIN_TAMAYO_MONEY,
 * functionaryTamayoName, porkinsName, porkinsMoney, tamayoMoney, porkinsHasMoney, tamayoStartOrigin,
 * tamayoHasJustReceivedMoney). Mission_CatchTrurl keeps its state in save variables.
 */
#include "scrap/scrap.h"
#include "Levels.h"

#define MAP "Map"
#define AM "Mission_ApologizeMayor"
#define CT "Mission_CatchTrurl"
#define SM "Mission_SheriffMayor"
#define TM "Mission_TalkToMayor"

/* Scrap.UsrEntity(0).ActCtrl == ctrl */
static int Th_UsrIs(scriptFrame_t *f, const char *ctrl) {
	return Sf_EqS(f, Sf_Attr(f, Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0), "ActCtrl"), ctrl);
}

/* Speech.OnSpch(msg, ans) */
static void Th_Spch(scriptFrame_t *f, const char *msg, const char *ans) {
	Sf_Ask(f, "Speech", "OnSpch", "(ss)", msg, ans);
}

/* "TalkNPC" + `int(Scrap.Rand(0, 1.9))`: one of the two talk animations */
static PyObject *Th_TalkAnim(scriptFrame_t *f) {
	return Sf_Cat(f, "sO", "TalkNPC", Sf_Repr(f, Sf_ToInt(f, Sf_Float(f, Scrap_RandRange(0, 1.9)))));
}

/* ---- Map ---- */

/* InitStatus(LevelPath): an indoor map; the elevators, the NPCs that come back, the mayor, the police boss */
static PyObject *TownhallMap_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	static const char *const mods[] = { "Elevator", "Init", "Chars", "CharsNPC" };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	static const char *const defs[][2] = { { "Elevator", "DM_Door_Elevator_DownTown" }, { "ElevatorOrbit", "DM_Door_ElevatorOrbit_Orbit" },
		{ "ElevatorOrbitB", "DM_Door_ElevatorOrbit_Orbit" }, { "Metro", "DM_Door_Metro_Press" }, { "MetroExt", "DM_Door_Metro_Press" } };
	for (int i = 0; i < 5; i++) {
		Sf_Ask(&f, "Elevator", "SetDef", "(ss)", defs[i][0], defs[i][1]);
	}
	static const struct {
		const char *name;
		int n;
	} repos[] = { { "Sentinel", 3 }, { "Police", 3 }, { "Nurse", 3 }, { "Desktop", 3 }, { "Messenger", 2 } };
	for (int i = 0; i < 5; i++) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", repos[i].name, repos[i].n);
	}
	if (Lv_PcOr(&f, MAP, "_BankDirector") > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "BankDirector", 3);
	}
	if (Lv_PcOr(&f, MAP, "_Bishop") > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Bishop", 3);
	}
	if (Lv_PcOr(&f, MAP, "_Functionary") > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Functionary", 5);
	}
	if (Lv_SaveIs(&f, "MayorAtTownHall", "1") > 0) {
		Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "Mayor_Default");
		Lv_MissionChar(&f, "Gear_Mayor", MAP, "CreateGearMayor");
	}
	int boss = Lv_SaveIs(&f, "PoliceBossAtPolice", "1");
	if (boss > 0) {
		boss = Lv_SaveIs(&f, "PoliceBossAtTownHall", "1");
	}
	if (boss > 0) {
		Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "PoliceBoss_Default");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "PoliceBossAtPolice", "0");
	}
	return Sf_None(&f);
}

/* CreateGearMayor(ch): the mayor's gear stays with the mayor */
static PyObject *TownhallMap_CreateGearMayor(PyObject *self, PyObject *args) {
	PyObject *ch;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ch)) {
		return NULL;
	}
	Lv_Stay(&f, ch, 4, Sf_Str(&f, "Mayor_Default"));
	return Sf_None(&f);
}

/* Init(LevelPath): the elevators and teleporters, the metro, the locked elevator to the orbit, the fake traffic */
static PyObject *TownhallMap_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "Metro");
	Sf_Ask(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_DownTown", "DownTown");
	Sf_Ask(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_ElevatorOrbit_Orbit", "Orbit");
	Sf_Ask(&f, "Elevator", "ConnectElevators", "([ss][]i)", "DM_Door_Teleporter_01", "DM_Door_Teleporter_02", 0);
	Sf_Ask(&f, "Elevator", "ConnectElevators", "([ss][]i)", "DM_Door_Teleporter_04", "DM_Door_Teleporter_05", 0);
	Sf_Ask(&f, "Elevator", "ConnectElevators", "([ss][]i)", "DM_Door_Teleporter_06", "DM_Door_Teleporter_07", 0);
	Sf_Ask(&f, "Metro", "SetAsMetro", "(s)", "DM_Door_Metro_Press");
	PyObject *e = Sf_Entity(&f, Sf_Str(&f, "DM_Door_ElevatorOrbit_Orbit"));
	Sf_SetAttrS(&f, e, "UseFunc", "Map.OrbitLockMsg");
	Sf_SetAttrI(&f, e, "ArrnOpt", 0);
	Sf_Import(&f, "InTraffic");
	Sf_SetMod(&f, "InTraffic", "N_TrafficList", Sf_Build(&f, "[sss]", "TLight1B", "TLight2B", "TLight3B"));
	Sf_SetMod(&f, "InTraffic", "N_NumTraffic", Sf_Int(&f, 20));
	Sf_SetMod(&f, "InTraffic", "M_TrafficList", Sf_Build(&f, "[sss]", "TMed1B", "TMed2B", "TMed3B"));
	Sf_SetMod(&f, "InTraffic", "M_NumTraffic", Sf_Int(&f, 5));
	Sf_SetMod(&f, "InTraffic", "H_TrafficList", Sf_Build(&f, "[]"));
	Sf_SetMod(&f, "InTraffic", "H_NumTraffic", Sf_Int(&f, 0));
	Sf_Ask(&f, "InTraffic", "LoadFakeTraffic", "()");
	Sf_Ask(&f, "InTraffic", "FakeTraffic", "()");
	return Sf_None(&f);
}

/* OrbitLockMsg(UsrName, ElevatorName, optId, FullOption): the elevator to the orbit cannot be used */
static PyObject *TownhallMap_OrbitLockMsg(PyObject *self, PyObject *args) {
	PyObject *usr, *elev, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &elev, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	PyObject *cad = Sf_Lang(&f, Sf_Str(&f, "CantUseOrbitElevator"));
	PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", cad));
	Sf_Ask(&f, "SScorer", "SetMsgText", "(OO)", cad, Sf_OpI(&f, t, '+', 6));
	return Sf_None(&f);
}

PyMethodDef g_TownhallMapMethods[] = {
	{ "InitStatus", TownhallMap_InitStatus, METH_VARARGS, NULL },
	{ "CreateGearMayor", TownhallMap_CreateGearMayor, METH_VARARGS, NULL },
	{ "Init", TownhallMap_Init, METH_VARARGS, NULL },
	{ "OrbitLockMsg", TownhallMap_OrbitLockMsg, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapSnd ---- */

/* the sound elements of the map (the rest of MapSnd is data/levels/townhall/mapsnd.def) */
static const lvSndElement_t townhall_snd[] = {
	{ .name = "TownhallAlmLB", .beep = "beep3", .imin = 6, .imax = 10, .ivol = 0.14 },
	{ .name = "TownhallAlmLA", .beep = "beep5", .imin = 6, .imax = 10, .ivol = 0.044 },
	{ .name = "TownhallAlmLC", .beep = "beep5", .imin = 6, .imax = 10, .ivol = 0.044 },
	{ .name = "TownhallServerA", .beep = "beep2", .beepXbox = "beep6", .imin = 4, .imax = 10, .ivol = 0.129 },
	{ .name = "TownhallServerB", .beep = "beep2", .beepXbox = "beep6", .imin = 4, .imax = 10, .ivol = 0.129 },
	{ .name = "TownhallHelix", .pc = 1, .front = 1, .loop = "ventilador3", .vol = 0.30, .pitch = -8, .hasPitch = 1 },
	{ .name = "TownhallLamp", .pc = 1, .loop = "light2", .vol = 0.04, .pitch = 10, .hasPitch = 1 },
	{ .name = "TownhallLampDesk", .pc = 1, .loop = "light2", .vol = 0.04, .pitch = 10, .hasPitch = 1 },
	{ .name = "TownhallHolo00", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep6", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.08 },
	{ .name = "TownhallHolo01", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo02", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo03", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo04", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo05", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep6", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.08 },
	{ .name = "TownhallHolo06", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo07", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo08", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep6", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.08 },
	{ .name = "TownhallHolo09", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo10", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo11", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep6", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.08 },
	{ .name = "TownhallHolo12", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "Holotree00", .pc = 1, .loop = "buble1", .vol = 0.08 },
	{ .name = NULL },
};

/* SndElements(Name, x, y, z, angx, angy) */
static PyObject *TownhallMapSnd_SndElements(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_SndElements(args, townhall_snd);
}

PyMethodDef g_TownhallMapSndMethods[] = {
	{ "SndElements", TownhallMapSnd_SndElements, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ApologizeMayor ---- */

#define AMG(name) Sf_Global(&f, AM, name)

/* InitStatus(LevelPath): the mayor's AI after the NPCs are browsed, the staplers come back */
static PyObject *ApologizeMayor_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "CharsNPC");
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", AMG("AfterBrowseNPC"));
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "RespawnChars"), "append", "(s)", "Desktop");
	return Sf_None(&f);
}

/* AfterBrowseNPC(): the mayor stays (CreateMayorInit), no gear */
static PyObject *ApologizeMayor_AfterBrowseNPC(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "CharsNPC");
	Lv_Callback(&f, "Mayor_Default", AM, "CreateMayorInit");
	Sf_DelItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Gear_Mayor"));
	return Sf_None(&f);
}

/* CreateMayorInit(e): the mayor stays and talks (MayorSpeech) */
static PyObject *ApologizeMayor_CreateMayorInit(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_ApologizeMayor.MayorSpeech");
	return Sf_None(&f);
}

/* Init(LevelPath): the state of the crazy mayor: the forbidden area, the staplers, or the transport */
static PyObject *ApologizeMayor_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	if (Lv_SaveIs(&f, "CrazyMayor", "Killed") > 0) {
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Mayor_Default");
		Sf_Ask(&f, "Scrap", "Set", "(ss)", "GuaranteedChars",
			",Police,Nurse,BankDirector,Desktop,Sentinel,Mayor,Gear,Berto,Messenger,Bishop,Functionary,Betty,PoliceBoss,BankMaster,");
		Sf_Own(&f, AM, "ForbiddenMsg", "()");
	} else if (Lv_SaveIs(&f, "CrazyMayor", "Stapler") > 0) {
		Sf_Own(&f, AM, "InitStaplerMission", "()");
	} else if (Lv_SaveIs(&f, "CrazyMayor", "AskTheTransport") > 0) {
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Mayor_Default");
	}
	return Sf_None(&f);
}

/* ForbiddenMsg(): the label of the forbidden area, every second */
static PyObject *ApologizeMayor_ForbiddenMsg(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *time = Sf_Time(&f);
	Sf_Ask(&f, "SScorer", "SetLabelText", "(OO)", Sf_Lang(&f, Sf_Str(&f, "Generics_ForbiddenArea_Label")), Sf_Op(&f, time, '+', Sf_Float(&f, 0.5)));
	Sf_Schedule(&f, Sf_Op(&f, time, '+', Sf_Float(&f, 1.0)), AMG("ForbiddenMsg"), Sf_Build(&f, "()"), "ForbiddenMsg");
	return Sf_None(&f);
}

/* MayorSpeech(UsrName, CharName, optId, FullOption): the mayor talks to Dtritus for the state of the mission */
static PyObject *ApologizeMayor_MayorSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	int dt = Th_UsrIs(&f, "Dtritus");
	if (dt == 0) {
		Th_Spch(&f, "Mission_ApologizeMayor_Mayor_InitMsg2", "Mission_ApologizeMayor_Mayor_InitAns2");
	}
	if (dt <= 0) {
		return Sf_None(&f);
	}
	if (Lv_SaveIs(&f, "CrazyMayor", "Killed") > 0) {
		int guarded = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "GuaranteedChars"), "!=", Sf_Str(&f, ""));
		if (guarded > 0) {
			Sf_Ask(&f, "Speech", "StdAnsLst", "([s])", "Mission_ApologizeMayor_Mayor_1");
			Th_Spch(&f, "Mission_ApologizeMayor_Mayor_InitMsg1", "Mission_ApologizeMayor_Mayor_InitAns1");
			Sf_Ask(&f, "Scrap", "Set", "(ss)", "GuaranteedChars", "");
			Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ForbiddenMsg");
		} else if (guarded == 0) {
			Sf_Ask(&f, "Speech", "RestoreState", "(O)", ch);
			Th_Spch(&f, "Mission_ApologizeMayor_Mayor_1Msg", "Mission_ApologizeMayor_Mayor_1Ans");
			Sf_SetMod(&f, "Speech", "OnEndOfSpeech", AMG("InitStaplerMission"));
		}
	} else if (Lv_SaveIs(&f, "CrazyMayor", "Stapler") > 0) {
		Th_Spch(&f, "Mission_KillStaplers_Mayor_InitMsg1", "Mission_KillStaplers_Mayor_InitAns1");
	} else if (Lv_SaveIs(&f, "CrazyMayor", "AskTheTransport") > 0) {
		Th_Spch(&f, "Mission_TalkToMayor1_Mayor_InitMsg1", "Mission_TalkToMayor1_Mayor_InitAns1");
		Sf_SetMod(&f, "Speech", "OnEndOfSpeech", AMG("SetTransportMission"));
	} else {
		Th_Spch(&f, "Mission_TalkToMayor1_Mayor_InitMsg2", "Mission_TalkToMayor1_Mayor_InitAns2");
	}
	return Sf_None(&f);
}

/* SetTransportMission(): Mission_ProtectMayorTransport in DownTown; the mayor leaves */
static PyObject *ApologizeMayor_SetTransportMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (Lv_SaveIs(&f, "CrazyMayor", "AskTheTransport") > 0) {
		Sf_Ask(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_ProtectMayorTransport", "DownTown");
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", Sf_Entity(&f, Sf_Mod(&f, "Speech", "NPCName")), 0);
		Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_ProtectMayorTransport_MissionText1", 1);
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MayorAtTownHall", "0");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "CrazyMayor", "Transport");
	}
	return Sf_None(&f);
}

/* ChooseStapler(NotName=""): a stapler of the level at random (not NotName) is the target; none there: again in
 * 2 s */
static PyObject *ApologizeMayor_ChooseStapler(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "NotName", NULL };
	PyObject *not = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &not)) {
		return NULL;
	}
	not = not ? Sf_Ref(&f, not) : Sf_Str(&f, "");
	PyObject *mmt = Sf_Build(&f, "[]");
	PyObject *keys = Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), "keys", "()");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *ky = Sf_Next(&f, keys, i);
		if (!ky) {
			break;
		}
		int c = Sf_EqS(&f, Sf_Attr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), ky), "ActCtrl"), "Desktop");
		if (c > 0) {
			c = Sf_Compare(&f, ky, "!=", not);
		}
		if (c > 0) {
			int there = Sf_True(&f, Sf_Entity(&f, ky));
			if (there > 0) {
				int in = Sf_In(&f, ky, mmt);
				there = in < 0 ? -1 : !in;
			}
			if (there > 0) {
				Sf_Method(&f, mmt, "append", "(O)", ky);
			}
		}
		Sf_Release(&f, mark);
	}
	PyObject *stapler = Py_None;
	int n = Sf_Len(&f, mmt);
	if (n > 0) {
		PyObject *index = Sf_ToInt(&f, Sf_Float(&f, Scrap_RandRange(0.0, (double)n - 0.05)));
		stapler = Sf_Entity(&f, Sf_GetItem(&f, mmt, index));
	}
	int got = Sf_True(&f, stapler);
	if (got > 0) {
		Sf_SetAttrS(&f, stapler, "UseFunc", "Mission_ApologizeMayor.StaplerSpeech");
		Sf_SetAttrI(&f, stapler, "AI_Repos", 0);
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, stapler, "Name"));
	} else if (got == 0) {
		Sf_Later(&f, Sf_Int(&f, 2), AMG("ChooseStapler"), Sf_Build(&f, "(O)", not), NULL);
	}
	return Sf_None(&f);
}

/* InitStaplerMission(): the staplers to kill: the counter, the scorer, the NPC death hook, the first stapler */
static PyObject *ApologizeMayor_InitStaplerMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_KillStaplers_MissionText1", 1);
	Sf_Ask(&f, "Scrap", "CreateSaveVar", "(ss)", "CrazyMayor.StaplerDeads", "0");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "CrazyMayor", "Stapler");
	Sf_SetMod(&f, "Menu", "MissionScorerCall", AMG("MissionScorer"));
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Sf_Ask(&f, "Scrap", "Set", "(si)", "RadarMMSelect", 1);
	Sf_SetMod(&f, "CharAct", "onNPCDeath", AMG("StaplerDead"));
	Sf_Own(&f, AM, "ChooseStapler", "()");
	return Sf_None(&f);
}

/* StaplerSpeech(UsrName, CharName, optId, FullOption) */
static PyObject *ApologizeMayor_StaplerSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	Th_Spch(&f, "Mission_KillStaplers_Desktop_InitMsg1", "Mission_KillStaplers_Desktop_InitAns1");
	return Sf_None(&f);
}

/* StaplerDead(name, pos): the death hook of the NPCs: the target stapler counts, the mayor taunts; the last one ends
 * the hunt; another stapler draws a remark. Always 0 */
static PyObject *ApologizeMayor_StaplerDead(PyObject *self, PyObject *args) {
	PyObject *name, *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &pos)) {
		return NULL;
	}
	PyObject *msg = Py_None;
	PyObject *e = Sf_Entity(&f, name);
	int target = Sf_True(&f, e);
	if (target > 0) {
		target = Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 1);
	}
	if (target > 0) {
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
		PyObject *deads = Sf_Eval(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "CrazyMayor.StaplerDeads"), AM);
		deads = Sf_OpI(&f, deads, '+', 1);
		int last = Sf_Compare(&f, deads, "==", AMG("MaxTargets"));
		if (last > 0) {
			Sf_SetMod(&f, "Menu", "MissionScorerCall", Py_None);
			Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
		} else if (last == 0) {
			Sf_Ask(&f, "SScorer", "Set", "(issO)", 0, "RemainingStaplersScorer", "Text", Sf_Cat(&f, "OsO", Sf_Repr(&f, deads), "/", Sf_Repr(&f, AMG("MaxTargets"))));
		}
		msg = Sf_Cat(&f, "sO", "Mission_KillStaplers_RemoteMessage", Sf_Repr(&f, Sf_OpI(&f, AMG("SMayorTaunt"), '+', 1)));
		Sf_SetGlobal(&f, AM, "SMayorTaunt", Sf_OpI(&f, AMG("SMayorTaunt"), '+', 1));
		if (Sf_Compare(&f, AMG("SMayorTaunt"), ">", Sf_Int(&f, 4)) > 0) {
			Sf_SetGlobal(&f, AM, "SMayorTaunt", Sf_Int(&f, 0));
		}
		last = Sf_Compare(&f, deads, "==", AMG("MaxTargets"));
		if (last > 0) {
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_KillStaplers_RemoteMessage6", "Mayor", AMG("MayorMessage"), Sf_Build(&f, "()"));
			return Sf_Return(&f, Sf_Int(&f, 0));
		}
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "CrazyMayor.StaplerDeads", Sf_Repr(&f, deads));
		Sf_Own(&f, AM, "ChooseStapler", "(O)", name);
		if (Sf_True(&f, msg) > 0) {
			Sf_Ask(&f, "Speech", "RemoteComunication", "(Os)", msg, "Mayor");
		}
	} else if (target == 0) {
		int desk = Sf_True(&f, e);
		if (desk > 0) {
			desk = Sf_EqS(&f, Sf_Attr(&f, e, "ActCtrl"), "Desktop");
		}
		if (desk > 0) {
			Sf_Ask(&f, "Speech", "RemoteComunication", "(ss)", "Mission_KillStaplers_RemoteMessage7", "Mayor");
		}
	}
	return Sf_Return(&f, Sf_Int(&f, 0));
}

/* MayorMessage(): the staplers done: the mayor waits to talk about the transport */
static PyObject *ApologizeMayor_MayorMessage(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetMod(&f, "CharAct", "onNPCDeath", Py_None);
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "CrazyMayor.StaplerDeads");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "CrazyMayor", "AskTheTransport");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Mayor_Default");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToMayor1_MissionText1", 1);
	return Sf_None(&f);
}

/* MissionScorer(id): the staplers killed of MaxTargets */
static PyObject *ApologizeMayor_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Scorer");
	Lv_ScorerTextBegin(&f, id, "RemainingStaplersText", 0);
	Lv_ScorerSet(&f, id, "RemainingStaplersText", "Text", Sf_Cat(&f, "Os", Sf_Lang(&f, Sf_Str(&f, "Mission_KillStaplers_Scorer")), ":"));
	Lv_ScorerTextEnd(&f, id, "RemainingStaplersText");
	Lv_ScorerTextBegin(&f, id, "RemainingStaplersScorer", 20);
	PyObject *deads = Sf_Eval(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "CrazyMayor.StaplerDeads"), AM);
	Lv_ScorerSet(&f, id, "RemainingStaplersScorer", "Text", Sf_Cat(&f, "OsO", Sf_Repr(&f, deads), "/", Sf_Repr(&f, AMG("MaxTargets"))));
	Lv_ScorerTextEnd(&f, id, "RemainingStaplersScorer");
	return Sf_None(&f);
}

PyMethodDef g_TownhallApologizeMayorMethods[] = {
	{ "InitStatus", ApologizeMayor_InitStatus, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", ApologizeMayor_AfterBrowseNPC, METH_VARARGS, NULL },
	{ "CreateMayorInit", ApologizeMayor_CreateMayorInit, METH_VARARGS, NULL },
	{ "Init", ApologizeMayor_Init, METH_VARARGS, NULL },
	{ "ForbiddenMsg", ApologizeMayor_ForbiddenMsg, METH_VARARGS, NULL },
	{ "MayorSpeech", ApologizeMayor_MayorSpeech, METH_VARARGS, NULL },
	{ "SetTransportMission", ApologizeMayor_SetTransportMission, METH_VARARGS, NULL },
	{ "ChooseStapler", (PyCFunction)(void (*)(void))ApologizeMayor_ChooseStapler, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "InitStaplerMission", ApologizeMayor_InitStaplerMission, METH_VARARGS, NULL },
	{ "StaplerSpeech", ApologizeMayor_StaplerSpeech, METH_VARARGS, NULL },
	{ "StaplerDead", ApologizeMayor_StaplerDead, METH_VARARGS, NULL },
	{ "MayorMessage", ApologizeMayor_MayorMessage, METH_VARARGS, NULL },
	{ "MissionScorer", ApologizeMayor_MissionScorer, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_CatchTrurl ---- */

#define CTG(name) Sf_Global(&f, CT, name)

/* eval(Scrap.GetSaveVar(var)) in the globals of the mission */
static PyObject *Ct_Save(scriptFrame_t *f, const char *var) {
	return Lv_SaveList(f, var, CT);
}

/* InitStatus(LevelPath): Trurl is the target, no data packs of the messengers; three functionaries */
static PyObject *CatchTrurl_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Messenger");
	Sf_SetMod(&f, "Messenger", "MakeMDP", Sf_Int(&f, 0));
	Lv_MissionChar(&f, "Messenger_Trurl", CT, "CreateTrurl");
	Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "Functionary_10");
	Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "Functionary_23");
	Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "Functionary_25");
	return Sf_None(&f);
}

/* Init(LevelPath): Trurl's name; the state of the mission (the first time its save variables) */
static PyObject *CatchTrurl_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	PyObject *trurl = Sf_Entity(&f, Sf_Str(&f, "Messenger_Trurl"));
	Sf_SetAttr(&f, trurl, "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Trurl")));
	PyObject *data = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "CatchTrurlMissionData");
	if (Sf_EqS(&f, data, "CatchTrurl") > 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_CatchTrurl_Data", "['Functionary_10', 'Functionary_23', 'Functionary_25']");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_CatchTrurl_MapsPos", "[]");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_CatchTrurl_NumMapsDropped", "0");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_CatchTrurl_NumMapsTaken", "0");
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Messenger_Trurl");
	} else if (Sf_EqS(&f, data, "KillFunctionaries") > 0) {
		Sf_Own(&f, CT, "KillFunctionaries", "()");
	} else if (Sf_EqS(&f, data, "CatchTrurlAgain") > 0) {
		Sf_Own(&f, CT, "CatchTrurlAgain", "()");
	}
	return Sf_None(&f);
}

/* CreateTrurl(t): Trurl's talk for the state of the mission */
static PyObject *CatchTrurl_CreateTrurl(PyObject *self, PyObject *args) {
	PyObject *t;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &t)) {
		return NULL;
	}
	PyObject *data = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "CatchTrurlMissionData");
	if (Sf_EqS(&f, data, "CatchTrurl") > 0) {
		Sf_SetAttrS(&f, t, "UseFunc", "Mission_CatchTrurl.OnCatchTrurl");
	} else if (Sf_EqS(&f, data, "CatchTrurlAgain") > 0) {
		Sf_SetAttrS(&f, t, "UseFunc", "Mission_CatchTrurl.OnCatchTrurlAgain");
	}
	return Sf_None(&f);
}

/* OnCatchTrurl(UsrName, CharName, optId, FullOption): Trurl caught; his talk ends in KillFunctionaries */
static PyObject *CatchTrurl_OnCatchTrurl(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "CatchTrurlMissionData", "KillFunctionaries");
		Lv_OnEnd(&f, CT, "KillFunctionaries", NULL);
		Sf_Ask(&f, "Speech", "NISpeech", "(s)", "Messenger");
		Sf_Ask(&f, "Speech", "AutoAddNI", "(s)", "Mission_GetTrurl_Trurl_NI_1Msg");
		Sf_Ask(&f, "Speech", "RunNI", "()");
	}
	return Sf_None(&f);
}

/* OnCatchTrurlAgain(UsrName, CharName, optId, FullOption): Trurl again; SetNextMission at the end */
static PyObject *CatchTrurl_OnCatchTrurlAgain(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Lv_OnEnd(&f, CT, "SetNextMission", NULL);
		Th_Spch(&f, "Mission_GetTrurlAgain_Trurl_InitMsg1", "Mission_GetTrurlAgain_Trurl_InitAns1");
	}
	return Sf_None(&f);
}

/* FunctionaryMissionSpeech(UsrName, CharName, optId, FullOption): a functionary with blueprints talks */
static PyObject *CatchTrurl_FunctionaryMissionSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Th_Spch(&f, "Mission_GetMayorOutlines_InitMsg1", "Mission_GetMayorOutlines_InitAns1");
	}
	return Sf_None(&f);
}

/* SetCallbackFuncs(missionData): while functionaries live their death or possession drops a blueprint (SpawnMap) */
static PyObject *CatchTrurl_SetCallbackFuncs(PyObject *self, PyObject *args) {
	PyObject *data;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &data)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	long alive = 0;
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *target = Sf_Next(&f, data, i);
		if (!target) {
			break;
		}
		if (Sf_EqI(&f, Sf_Ask(&f, "string", "find", "(Os)", target, "Blueprint"), -1) > 0) {
			alive++;
		}
		Sf_Release(&f, mark);
	}
	if (alive == 0) {
		Sf_SetMod(&f, "CharAct", "onNPCDeath", Py_None);
		Sf_SetMod(&f, "CharConversor", "OnPossession", Py_None);
	} else {
		if (Sf_Compare(&f, Sf_Mod(&f, "CharAct", "onNPCDeath"), "==", Py_None) > 0) {
			Sf_SetMod(&f, "CharAct", "onNPCDeath", CTG("SpawnMap"));
		}
		if (Sf_Compare(&f, Sf_Mod(&f, "CharConversor", "OnPossession"), "==", Py_None) > 0) {
			Sf_SetMod(&f, "CharConversor", "OnPossession", CTG("SpawnMap"));
		}
	}
	return Sf_None(&f);
}

/* SpawnMap(name, pos): a functionary of the mission died or was possessed: he drops the next blueprint (1); else 0 */
static PyObject *CatchTrurl_SpawnMap(PyObject *self, PyObject *args) {
	PyObject *name, *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &pos)) {
		return NULL;
	}
	PyObject *data = Ct_Save(&f, "Mission_CatchTrurl_Data");
	PyObject *maps = Ct_Save(&f, "Mission_CatchTrurl_MapsPos");
	if (Sf_In(&f, name, data) <= 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	PyObject *dropped = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission_CatchTrurl_NumMapsDropped"));
	PyObject *e = Sf_Own(&f, CT, "CreateMap", "(OO)", pos, dropped);
	Sf_Ask(&f, "MissionsFuncs", "DelMissionTarget", "(O)", name);
	Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", Sf_Entity(&f, name), 0);
	Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
	Sf_Method(&f, data, "remove", "(O)", name);
	Sf_Method(&f, data, "append", "(O)", Sf_Attr(&f, e, "Name"));
	Sf_Method(&f, maps, "append", "(O)", Sf_Build(&f, "(OO)", Sf_Attr(&f, e, "Name"), Sf_Attr(&f, e, "Pos")));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_CatchTrurl_Data", Sf_Repr(&f, data));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_CatchTrurl_MapsPos", Sf_Repr(&f, maps));
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission_CatchTrurl_NumMapsDropped")), '+', 1);
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_CatchTrurl_NumMapsDropped", Sf_Repr(&f, n));
	Sf_Own(&f, CT, "SetCallbackFuncs", "(O)", data);
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* CreateMap(pos, mapType): a blueprint on the floor to photograph: 0 the weapon, 1 the engine, 2 the ship, else the
 * name itself */
static PyObject *CatchTrurl_CreateMap(PyObject *self, PyObject *args) {
	PyObject *pos, *type;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &pos, &type)) {
		return NULL;
	}
	static const char *const names[] = { "PlanoDevastator", "PlanoMMayor1", "PlanoSMayor1" };
	PyObject *cad = type;
	for (int i = 0; i < 3; i++) {
		int c = Sf_EqI(&f, type, i);
		if (c != 0) {
			cad = c > 0 ? Sf_Str(&f, names[i]) : NULL;
			break;
		}
	}
	PyObject *e = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", cad, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "WithLife");
	Sf_SetAttrI(&f, e, "ArrowCanView", 1);
	Sf_SetAttrS(&f, e, "ViewFunc", "");
	Sf_SetAttrI(&f, e, "CanPhoto", 1);
	Sf_SetAttrI(&f, e, "VRadius", 30);
	Sf_Method(&f, e, "SetModel", "(O)", Sf_Cat(&f, "sOs", "Models/Misc/Blueprint/", cad, ".M3D"));
	Sf_Ask(&f, "Items", "SetInteractiveObjOnFloor", "(O)", e);
	return Sf_Return(&f, e);
}

/* the blueprint item into the hangar list of the save variable var (once; always when item is NULL: the weapon), the
 * message, a new blueprint */
static void Ct_Blueprint(scriptFrame_t *f, const char *var, const char *item, const char *msg) {
	PyObject *l = Ct_Save(f, var);
	int add = item ? Sf_In(f, Sf_Str(f, item), l) : 0;
	if (add == 0) {
		Lv_SaveAppend(f, var, l, Sf_Str(f, item ? item : "Devastator"));
	}
	Sf_Ask(f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Lang(f, Sf_Str(f, msg)));
	Sf_Ask(f, "Scrap", "IncSaveVar", "(s)", "NewBluePrintAvaliable");
}

/* TakeMapPhoto(mapName): the photo of a blueprint of the mission (1, else 0); after the last Trurl calls */
static PyObject *CatchTrurl_TakeMapPhoto(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	PyObject *data = Ct_Save(&f, "Mission_CatchTrurl_Data");
	PyObject *maps = Ct_Save(&f, "Mission_CatchTrurl_MapsPos");
	if (Sf_In(&f, name, data) <= 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	PyObject *m = Sf_Entity(&f, name);
	Sf_SetAttrI(&f, m, "RadarShow", 0);
	Sf_SetAttrI(&f, m, "ArrowCanView", 0);
	Sf_SetAttrI(&f, m, "ArrowCanMission", 0);
	if (Sf_EqS(&f, name, "PlanoSMayor1") > 0) {
		Ct_Blueprint(&f, "Hangar.shipsToEditList", "SMayor1", "Mission_GetMayorOutlines_SystemMessage1");
	} else if (Sf_EqS(&f, name, "PlanoMMayor1") > 0) {
		Ct_Blueprint(&f, "Hangar.availableEnginesList", "MMAYOR1", "Mission_GetMayorOutlines_SystemMessage2");
	} else {
		Ct_Blueprint(&f, "Hangar.availableWeaponsList", NULL, "Mission_GetMayorOutlines_SystemMessage3");
	}
	Sf_Method(&f, data, "remove", "(O)", name);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *elem = Sf_Next(&f, maps, i);
		if (!elem) {
			break;
		}
		if (Sf_Compare(&f, Sf_Item(&f, elem, 0), "==", name) > 0) {
			Sf_Method(&f, maps, "remove", "(O)", elem);
			break;
		}
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_CatchTrurl_Data", Sf_Repr(&f, data));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_CatchTrurl_MapsPos", Sf_Repr(&f, maps));
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission_CatchTrurl_NumMapsTaken")), '+', 1);
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_CatchTrurl_NumMapsTaken", Sf_Repr(&f, n));
	int left = Sf_Len(&f, data);
	if (left == 0) {
		static const char *const vars[] = { "Mission_CatchTrurl_Data", "Mission_CatchTrurl_MapsPos", "Mission_CatchTrurl_NumMapsDropped",
			"Mission_CatchTrurl_NumMapsTaken" };
		for (int i = 0; i < 4; i++) {
			Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", vars[i]);
		}
		Sf_SetMod(&f, "CharAct", "onNPCDeath", Py_None);
		Sf_SetMod(&f, "CharConversor", "OnPossession", Py_None);
		Sf_SetMod(&f, "CharAct", "TakenPhoto", Py_None);
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "CatchTrurlMissionData", "CatchTrurlAgain");
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssO)", "Mission_GetMayorOutlines_RemoteMessage2", "Trurl", CTG("CatchTrurlAgain"));
	} else if (left > 0) {
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_GetMayorOutlines_RemoteMessage1", "Trurl");
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* KillFunctionaries(): the functionaries with blueprints flee and are the targets; the blueprints already dropped
 * lie again where they fell */
static PyObject *CatchTrurl_KillFunctionaries(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *data = Ct_Save(&f, "Mission_CatchTrurl_Data");
	PyObject *maps = Ct_Save(&f, "Mission_CatchTrurl_MapsPos");
	long index = 0;
	Sf_Len(&f, maps);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *fn = Sf_Next(&f, data, i);
		if (!fn) {
			break;
		}
		PyObject *e = Sf_Entity(&f, fn);
		int there = Sf_True(&f, e);
		if (there > 0) {
			Sf_SetAttr(&f, e, "LLogicProbTalkWhenWalking", Sf_Float(&f, 1.0));
			Sf_SetAttrI(&f, e, "LLogicEscapeWithinRadius", 1);
			Sf_SetAttrI(&f, e, "LLogicEscapeRadiusSqr", 562500);
			Sf_SetAttrS(&f, e, "UseFunc", "Mission_CatchTrurl.FunctionaryMissionSpeech");
		} else if (there == 0) {
			Sf_Own(&f, CT, "CreateMap", "(OO)", Sf_Item(&f, Sf_Item(&f, maps, index), 1), fn);
			index++;
		}
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", data);
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetMayorOutlines_MissionText1", 1);
	Sf_Own(&f, CT, "SetCallbackFuncs", "(O)", data);
	Sf_SetMod(&f, "CharAct", "TakenPhoto", CTG("TakeMapPhoto"));
	return Sf_None(&f);
}

/* CatchTrurlAgain(): saved, Trurl is the target again */
static PyObject *CatchTrurl_CatchTrurlAgain(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Messenger_Trurl");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetTrurlAgain_MissionText1", 1);
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Str(&f, "Messenger_Trurl")), "UseFunc", "Mission_CatchTrurl.OnCatchTrurlAgain");
	return Sf_None(&f);
}

/* SetNextMission(): Mission_StealMayorShip in DownTown, the parked ships, the crazy gambler calls in 2 s */
static PyObject *CatchTrurl_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "CatchTrurlMissionData");
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_StealMayorShip", "DownTown");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_TestMayorShip_MissionText1", 1);
	static const char *const parked[][2] = { { "Parked.Police", "['SPOLI1', 'SPOLI2', 'SPOLI3']" }, { "Parked.Mercs", "['SMERC1']" },
		{ "Parked.TownHall", "['SBANKER1', 'SPOLI2']" }, { "Parked.Bank", "['SBANKER1']" }, { "Parked.Temple", "['SPOLI3']" } };
	for (int i = 0; i < 5; i++) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", parked[i][0], parked[i][1]);
	}
	Sf_Later(&f, Sf_Int(&f, 2), CTG("CGCalls"), Sf_Build(&f, "()"), NULL);
	return Sf_None(&f);
}

/* CGCalls(): the crazy gambler's call after the first crazy deals */
static PyObject *CatchTrurl_CGCalls(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "SuperDeal.Library"), "!=", Sf_Str(&f, "SuperDeal_Twelfth")) <= 0) {
		return Sf_None(&f);
	}
	PyObject *num = Sf_Ask(&f, "MissionsFuncs", "GetNumCrazyDealCompleted", "()");
	static const char *const msgs[] = { "Mission_GetTrurl_RemoteMessage1", "Mission_GetTrurl_RemoteMessage2", "Mission_GetTrurl_RemoteMessage3" };
	for (int i = 0; i < 3; i++) {
		int c = Sf_EqI(&f, num, i);
		if (c > 0) {
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", msgs[i], "CrazyGambler");
		}
		if (c != 0) {
			break;
		}
	}
	return Sf_None(&f);
}

PyMethodDef g_TownhallCatchTrurlMethods[] = {
	{ "InitStatus", CatchTrurl_InitStatus, METH_VARARGS, NULL },
	{ "Init", CatchTrurl_Init, METH_VARARGS, NULL },
	{ "CreateTrurl", CatchTrurl_CreateTrurl, METH_VARARGS, NULL },
	{ "OnCatchTrurl", CatchTrurl_OnCatchTrurl, METH_VARARGS, NULL },
	{ "OnCatchTrurlAgain", CatchTrurl_OnCatchTrurlAgain, METH_VARARGS, NULL },
	{ "FunctionaryMissionSpeech", CatchTrurl_FunctionaryMissionSpeech, METH_VARARGS, NULL },
	{ "SetCallbackFuncs", CatchTrurl_SetCallbackFuncs, METH_VARARGS, NULL },
	{ "SpawnMap", CatchTrurl_SpawnMap, METH_VARARGS, NULL },
	{ "CreateMap", CatchTrurl_CreateMap, METH_VARARGS, NULL },
	{ "TakeMapPhoto", CatchTrurl_TakeMapPhoto, METH_VARARGS, NULL },
	{ "KillFunctionaries", CatchTrurl_KillFunctionaries, METH_VARARGS, NULL },
	{ "CatchTrurlAgain", CatchTrurl_CatchTrurlAgain, METH_VARARGS, NULL },
	{ "SetNextMission", CatchTrurl_SetNextMission, METH_VARARGS, NULL },
	{ "CGCalls", CatchTrurl_CGCalls, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_SheriffMayor ---- */

#define SMG(name) Sf_Global(&f, SM, name)

/* InitStatus(LevelPath): the police boss and the mayor of the mission */
static PyObject *SheriffMayor_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Lv_MissionChar(&f, "PoliceBoss_SheriffMayor", SM, "CreatePoliceBoss");
	Lv_MissionChar(&f, "Mayor_SheriffMayor", SM, "CreateMayor");
	return Sf_None(&f);
}

/* CreateMayor(e): the mayor stays */
static PyObject *SheriffMayor_CreateMayor(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	return Sf_None(&f);
}

/* CreatePoliceBoss(e): the police boss stays and talks */
static PyObject *SheriffMayor_CreatePoliceBoss(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_SheriffMayor.PoliceBossMissionSpeech");
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	return Sf_None(&f);
}

/* `e and e.LLogicIdle == 0 and e.LLogicState == 6`: the value of the and (borrowed, kept) */
static PyObject *Sm_Ready(scriptFrame_t *f, PyObject *e) {
	if (Sf_True(f, e) <= 0) {
		return f->failed ? NULL : e;
	}
	int idle = Sf_EqI(f, Sf_Attr(f, e, "LLogicIdle"), 0);
	if (idle <= 0) {
		return idle < 0 ? NULL : Sf_Int(f, 0);
	}
	int st = Sf_EqI(f, Sf_Attr(f, e, "LLogicState"), 6);
	return st < 0 ? NULL : Sf_Int(f, st);
}

/* ReadyToTalk(e): the character idles in place */
static PyObject *SheriffMayor_ReadyToTalk(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	return Sf_Return(&f, Sm_Ready(&f, e));
}

/* Blablibla(EntityName="Mayor_SheriffMayor"): the police boss and the mayor talk in turns while both idle */
static PyObject *SheriffMayor_Blablibla(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "EntityName", NULL };
	PyObject *name = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &name)) {
		return NULL;
	}
	name = name ? Sf_Ref(&f, name) : Sf_Str(&f, "Mayor_SheriffMayor");
	PyObject *sheriff = Sf_Entity(&f, Sf_Str(&f, "PoliceBoss_SheriffMayor"));
	PyObject *mayor = Sf_Entity(&f, Sf_Str(&f, "Mayor_SheriffMayor"));
	PyObject *me = Sf_Entity(&f, name);
	PyObject *doit = Sf_Own(&f, SM, "ReadyToTalk", "(O)", sheriff);
	if (Sf_True(&f, doit) > 0) {
		doit = Sf_Own(&f, SM, "ReadyToTalk", "(O)", mayor);
	}
	if (Sf_True(&f, doit) > 0) {
		int relax = Sf_EqS(&f, Sf_Slice(&f, Sf_Attr(&f, me, "Action"), 0, 5), "Relax");
		doit = relax < 0 ? NULL : Sf_Int(&f, relax);
	}
	if (Sf_True(&f, doit) > 0) {
		Sf_SetAttr(&f, me, "Action", Th_TalkAnim(&f));
	}
	int notMayor = Sf_Compare(&f, name, "!=", Sf_Str(&f, "Mayor_SheriffMayor"));
	int other = notMayor < 0 ? -1 : Sf_Compare(&f, doit, "!=", Sf_Int(&f, notMayor));
	if (other >= 0) {
		Sf_Later(&f, Sf_Float(&f, 2.0), SMG("Blablibla"), Sf_Build(&f, "(s)", other ? "PoliceBoss_SheriffMayor" : "Mayor_SheriffMayor"), NULL);
	}
	return Sf_None(&f);
}

/* PoliceBossMissionSpeech(UsrName, CharName, optId, FullOption): the police boss tells his clues to the mayor, or
 * sells them to Dtritus (OnPay); after that his usual talk */
static PyObject *SheriffMayor_PoliceBossMissionSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	int done = Sf_EqI(&f, Sf_Attr(&f, Sf_Entity(&f, Sf_Str(&f, "PoliceBoss_SheriffMayor")), "ArrowCanMission"), 0);
	if (done > 0) {
		int talked = Sf_True(&f, SMG("alreadyTalked"));
		if (talked == 0) {
			PyObject *fn = Sf_GetItem(&f, Sf_Mod(&f, "Speech", "SpeechRoutines"), Sf_Str(&f, "PoliceBoss"));
			Sf_Call(&f, fn, "(OOOO)", usr, ch, opt, full);
		} else if (talked > 0) {
			int mayor = Th_UsrIs(&f, "Mayor");
			if (mayor > 0) {
				Th_Spch(&f, "Mission_Get1stClue_PoliceBoss_InitMsg1", "Mission_Get1stClue_PoliceBoss_InitAns1");
			} else if (mayor == 0) {
				Th_Spch(&f, "Mission_GetInfoClues_PoliceBoss_InitMsg3", "Mission_GetInfoClues_PoliceBoss_InitAns3");
			}
		}
		return Sf_None(&f);
	}
	int mayor = done == 0 ? Th_UsrIs(&f, "Mayor") : -1;
	if (mayor > 0) {
		Sf_Ask(&f, "Scrap", "SetLockAlarm", "(i)", 1);
		Lv_OnEnd(&f, SM, "SetNextMission", NULL);
		Th_Spch(&f, "Mission_GetInfoClues_PoliceBoss_InitMsg2", "Mission_GetInfoClues_PoliceBoss_InitAns2");
	} else if (mayor == 0) {
		int rich = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), ">=", SMG("MoneyReward"));
		if (rich > 0) {
			Sf_Ask(&f, "Scrap", "SetLockAlarm", "(i)", 1);
			Lv_OnEnd(&f, SM, "OnPay", Sf_Build(&f, "(O)", ch));
			Th_Spch(&f, "Mission_GetInfoClues_PoliceBoss_InitMsg1", "Mission_GetInfoClues_PoliceBoss_InitAns1");
		} else if (rich == 0) {
			Th_Spch(&f, "Mission_GetInfoClues_PoliceBoss_InitMsg3", "Mission_GetInfoClues_PoliceBoss_InitAns3");
		}
	}
	return Sf_None(&f);
}

/* OnPay(CharName): the clues paid; the police boss carries the money */
static PyObject *SheriffMayor_OnPay(PyObject *self, PyObject *args) {
	PyObject *ch;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ch)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "SetMoney", "(O)", Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '-', SMG("MoneyReward")));
	Sf_Ask(&f, "SaveGame", "SaveMoney", "()");
	PyObject *pb = Sf_Entity(&f, ch);
	Sf_SetAttr(&f, pb, "Money", SMG("MoneyReward"));
	Sf_SetAttrI(&f, pb, "MoneyReward", 1000);
	Sf_Own(&f, SM, "SetNextMission", "()");
	return Sf_None(&f);
}

/* SetNextMission(): the police boss goes, the mayor comes back, the mission of NextMissionData */
static PyObject *SheriffMayor_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetGlobal(&f, SM, "alreadyTalked", Sf_Int(&f, 1));
	Sf_Ask(&f, "Scrap", "SetLockAlarm", "(i)", 0);
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "PoliceBossAtPolice", "0");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MayorAtTownHall", "1");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Ask(&f, "__builtin__", "apply", "(OO)", Sf_Mod(&f, "MissionsFuncs", "SetNextMission"), SMG("NextMissionData"));
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_Get1stClue_MissionText1", 1);
	return Sf_None(&f);
}

PyMethodDef g_TownhallSheriffMayorMethods[] = {
	{ "InitStatus", SheriffMayor_InitStatus, METH_VARARGS, NULL },
	{ "CreateMayor", SheriffMayor_CreateMayor, METH_VARARGS, NULL },
	{ "CreatePoliceBoss", SheriffMayor_CreatePoliceBoss, METH_VARARGS, NULL },
	{ "ReadyToTalk", SheriffMayor_ReadyToTalk, METH_VARARGS, NULL },
	{ "Blablibla", (PyCFunction)(void (*)(void))SheriffMayor_Blablibla, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "PoliceBossMissionSpeech", SheriffMayor_PoliceBossMissionSpeech, METH_VARARGS, NULL },
	{ "OnPay", SheriffMayor_OnPay, METH_VARARGS, NULL },
	{ "SetNextMission", SheriffMayor_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_TalkToMayor ---- */

#define TMG(name) Sf_Global(&f, TM, name)
#define TMGF(name) Sf_Global(f, TM, name)

/* InitStatus(LevelPath): the mayor's AI after the NPCs are browsed, Porkins the bishop and Corruptus */
static PyObject *TalkToMayor_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", TMG("AfterBrowseNPC"));
	Lv_MissionChar(&f, "Bishop_Porkins", TM, "CreateBishopPorkins");
	Lv_MissionChar(&f, "Functionary_Tamayo", TM, "CreateFunctionaryTamayo");
	return Sf_None(&f);
}

/* AfterBrowseNPC(): the mayor stays (CreateMayorInit), his gear stays too */
static PyObject *TalkToMayor_AfterBrowseNPC(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "CharsNPC");
	Lv_Callback(&f, "Mayor_Default", TM, "CreateMayorInit");
	Sf_SetAttrS(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Gear_Mayor")), "ExtraInfo", "STAY");
	return Sf_None(&f);
}

/* CreateMayorInit(e): the mayor stays and talks (MayorSpeech) */
static PyObject *TalkToMayor_CreateMayorInit(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_TalkToMayor.MayorSpeech");
	return Sf_None(&f);
}

/* CreateBishopPorkins(e): Porkins stays with his money (40 slaps empty him) */
static PyObject *TalkToMayor_CreateBishopPorkins(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	Sf_SetAttr(&f, e, "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Porkins")));
	Sf_SetAttr(&f, e, "Money", TMG("porkinsMoney"));
	Sf_SetAttr(&f, e, "MoneyReward", Sf_OpI(&f, TMG("porkinsMoney"), '/', 40));
	Sf_SetGlobal(&f, TM, "porkinsName", Sf_Attr(&f, e, "Name"));
	return Sf_None(&f);
}

/* CreateFunctionaryTamayo(e): Corruptus walks around with his money (10 slaps empty him) */
static PyObject *TalkToMayor_CreateFunctionaryTamayo(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttr(&f, e, "LLogicProbTalkWhenWalking", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, e, "LLogicFastEscape", 0);
	Sf_SetAttrI(&f, e, "LLogicStopInRelax", 0);
	Sf_SetAttrI(&f, e, "LLogicDefaultState", 0);
	Sf_SetAttrS(&f, e, "LLogicDefaultTarget", "");
	Sf_SetAttrI(&f, e, "LLogicState", 0);
	Sf_SetAttrS(&f, e, "LLogicTarget", "");
	Sf_SetAttr(&f, e, "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Corruptus")));
	Sf_SetAttr(&f, e, "Money", TMG("tamayoMoney"));
	Sf_SetAttr(&f, e, "MoneyReward", Sf_OpI(&f, TMG("tamayoMoney"), '/', 10));
	Sf_SetGlobal(&f, TM, "tamayoStartOrigin", Sf_Attr(&f, e, "LLogicOrigin"));
	Sf_SetGlobal(&f, TM, "functionaryTamayoName", Sf_Attr(&f, e, "Name"));
	if (Lv_SaveIs(&f, "MissionState", "StealFunctionary") > 0) {
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_TalkToMayor.FunctionarySpeech");
	}
	return Sf_None(&f);
}

/* Init(LevelPath): Rusty's head; the mayor is the target, or the stealing goes on */
static PyObject *TalkToMayor_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	Sf_Ask(&f, "SScorer", "AddModel", "(isssii)", 0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25);
	PyObject *state = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState");
	if (Sf_Compare(&f, Sf_Ask(&f, "string", "find", "(Os)", state, "TalkToMayor"), "!=", Sf_Int(&f, -1)) > 0) {
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Mayor_Default");
	} else if (Sf_EqS(&f, state, "StealFunctionary") > 0) {
		Sf_Own(&f, TM, "StealFunctionary", "()");
	}
	return Sf_None(&f);
}

/* MayorSpeech(UsrName, CharName, optId, FullOption): the mayor's talk to Dtritus for the state of the mission */
static PyObject *TalkToMayor_MayorSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	int dt = Th_UsrIs(&f, "Dtritus");
	if (dt == 0) {
		Th_Spch(&f, "Mission_TalkToMayor_Mayor_InitMsg2", "Mission_TalkToMayor_Mayor_InitAns2");
	}
	if (dt <= 0) {
		return Sf_None(&f);
	}
	PyObject *state = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState");
	int none = Sf_True(&f, state);
	if (none == 0) {
		Th_Spch(&f, "Mission_Campaign2_Mayor_InitMsg1", "Mission_Campaign2_Mayor_InitAns1");
	} else if (none < 0) {
	} else if (Sf_EqS(&f, state, "TalkToMayor1") > 0) {
		if (Sf_EqS(&f, opt, "Talk") > 0) {
			Th_Spch(&f, "Mission_TalkToMayor_Mayor_InitMsg1", "Mission_TalkToMayor_Mayor_InitAns1");
			Sf_Ask(&f, "Speech", "StdAnsLst", "([s])", "Mission_TalkToMayor_Mayor_1");
		} else {
			Sf_Ask(&f, "Speech", "RestoreState", "(O)", ch);
			Th_Spch(&f, "Mission_TalkToMayor_Mayor_1Msg", "Mission_TalkToMayor_Mayor_1Ans");
			Lv_OnEnd(&f, TM, "PreStealFunctionary", NULL);
		}
	} else if (Sf_EqS(&f, state, "StealFunctionary") > 0) {
		Th_Spch(&f, "Mission_Campaign1_Mayor_InitMsg1", "Mission_Campaign1_Mayor_InitAns1");
	} else if (Sf_EqS(&f, state, "TalkToMayor2") > 0) {
		Th_Spch(&f, "Mission_TalkToMayor2_Mayor_InitMsg1", "Mission_TalkToMayor2_Mayor_InitAns1");
		Lv_OnEnd(&f, TM, "SetNextMission", NULL);
	}
	return Sf_None(&f);
}

/* BishopSpeech(...), FunctionarySpeech(...): the talks of Porkins and Corruptus */
static PyObject *Tm_Speech(PyObject *args, const char *msg, const char *ans) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	Th_Spch(&f, msg, ans);
	return Sf_None(&f);
}

static PyObject *TalkToMayor_BishopSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Tm_Speech(args, "Mission_Campaign1_Porkins_InitMsg1", "Mission_Campaign1_Porkins_InitAns1");
}

static PyObject *TalkToMayor_FunctionarySpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Tm_Speech(args, "Mission_Campaign_Functionary_InitMsg1", "Mission_Campaign_Functionary_InitAns1");
}

/* StealFunctionary(): Porkins and Corruptus are the targets, the money bar, the money check */
static PyObject *TalkToMayor_StealFunctionary(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, TMG("porkinsName"));
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_TalkToMayor.BishopSpeech");
	}
	e = Sf_Entity(&f, TMG("functionaryTamayoName"));
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_TalkToMayor.FunctionarySpeech");
		Sf_SetAttr(&f, e, "Money", TMG("tamayoMoney"));
	}
	Sf_Ask(&f, "MissionsFuncs", "SetMultipleMissionTargets", "([OO])", TMG("functionaryTamayoName"), TMG("porkinsName"));
	Sf_Own(&f, TM, "CheckTamayoMoney", "()");
	Sf_SetMod(&f, "Menu", "MissionScorerCall", TMG("MissionScorer"));
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	return Sf_None(&f);
}

/* Corruptus low on money goes to Porkins and waits in front of him */
static void Tm_GoToPorkins(scriptFrame_t *f, PyObject *e, PyObject *p) {
	PyObject *dir = Sf_Ask(f, "SVec", "Rotate3D", "((ddd)O)", 0.0, 0.0, 1.0, Sf_Attr(f, p, "LLogicOriginAng"));
	PyObject *dist = Sf_OpI(f, Sf_Op(f, Sf_Attr(f, p, "VRadius"), '+', Sf_Attr(f, e, "VRadius")), '+', 50);
	Sf_SetAttr(f, e, "LLogicOrigin", Sf_Ask(f, "SVec", "Add", "(OO)", Sf_Attr(f, p, "LLogicOrigin"), Sf_Ask(f, "SVec", "Prod", "(OO)", dir, dist)));
	Sf_SetAttr(f, e, "LLogicOriginAng", Sf_Ask(f, "SVec", "Rotate3D", "((ddd)O)", 0.0, 0.0, -1.0, dir));
	Lv_Stay(f, e, 6, TMGF("porkinsName"));
	Sf_SetGlobal(f, TM, "tamayoHasJustReceivedMoney", Sf_Int(f, 0));
}

/* Corruptus in front of Porkins: they talk and Porkins fills Corruptus' money up to MAX_TAMAYO_MONEY */
static void Tm_Refill(scriptFrame_t *f, PyObject *e, PyObject *p) {
	if (Sf_Compare(f, Sf_Ask(f, "SVec", "ModSqr", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", Sf_Attr(f, e, "Pos"), Sf_Attr(f, p, "Pos"))), "<=",
		Sf_Int(f, 400 * 400)) <= 0) {
		return;
	}
	PyObject *bishopDir = Sf_Ask(f, "SVec", "Rotate3D", "((ddd)O)", 0.0, 0.0, -1.0, Sf_Attr(f, p, "Ang"));
	PyObject *toFunc = Sf_Ask(f, "SVec", "Sub", "(OO)", Sf_Attr(f, e, "Pos"), Sf_Attr(f, p, "Pos"));
	if (Sf_Compare(f, Sf_Ask(f, "SVec", "GetAngle", "(OO)", bishopDir, toFunc), "<=", Sf_Int(f, 30)) <= 0) {
		return;
	}
	Sf_SetAttr(f, e, "Action", Th_TalkAnim(f));
	Sf_SetAttr(f, p, "Action", Th_TalkAnim(f));
	if (Sf_Compare(f, Sf_Attr(f, p, "Money"), ">", Sf_Int(f, 0)) > 0) {
		Sf_Import(f, "Fx");
		Sf_Ask(f, "Fx", "MoneyTransfer", "(OOi)", TMGF("porkinsName"), TMGF("functionaryTamayoName"), 1);
		PyObject *gives = Sf_Op(f, TMGF("MAX_TAMAYO_MONEY"), '-', Sf_Attr(f, e, "Money"));
		if (Sf_Compare(f, gives, ">", Sf_Attr(f, p, "Money")) > 0) {
			gives = Sf_Attr(f, p, "Money");
		}
		Sf_SetAttr(f, e, "Money", Sf_Op(f, Sf_Attr(f, e, "Money"), '+', gives));
		Sf_SetAttr(f, p, "Money", Sf_Op(f, Sf_Attr(f, p, "Money"), '-', gives));
	}
	Sf_SetGlobal(f, TM, "tamayoHasJustReceivedMoney", Sf_Int(f, 1));
	Sf_Ask(f, "SAct", "GetClass", "(s)", "Functionary");
	Sf_Ask(f, "SAct", "GetAction", "(O)", Sf_Attr(f, e, "Action"));
	PyObject *time = Sf_Ask(f, "SAct", "GetAct", "(s)", "AnmTotalTime");
	PyObject *at = Sf_Op(f, Sf_Op(f, Sf_Time(f), '+', time), '-', Sf_Float(f, 0.1));
	Sf_Schedule(f, at, TMGF("RestoreTamayoState"), Sf_Build(f, "()"), NULL);
}

/* CheckTamayoMoney(): every second: Corruptus' money on the bar; low on money he goes to Porkins for more */
static PyObject *TalkToMayor_CheckTamayoMoney(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, TMG("functionaryTamayoName"));
	if (Sf_True(&f, e) > 0) {
		Sf_SetGlobal(&f, TM, "tamayoMoney", Sf_Attr(&f, e, "Money"));
		Sf_Ask(&f, "SScorer", "Set", "(issO)", 0, "TamayoMoneyBar", "Value", Sf_ToFloat(&f, TMG("tamayoMoney")));
		PyObject *p = Sf_Entity(&f, TMG("porkinsName"));
		if (Sf_True(&f, p) > 0) {
			int low = Sf_Compare(&f, TMG("tamayoMoney"), "<=", TMG("MIN_TAMAYO_MONEY"));
			if (low > 0) {
				low = Sf_Compare(&f, Sf_Attr(&f, e, "LLogicDefaultState"), "!=", Sf_Int(&f, 6));
			}
			if (low > 0) {
				Tm_GoToPorkins(&f, e, p);
			} else if (low == 0) {
				int waits = Sf_EqI(&f, Sf_Attr(&f, e, "LLogicDefaultState"), 6);
				if (waits > 0) {
					waits = Sf_EqI(&f, Sf_Attr(&f, p, "LLogicDefaultState"), 6);
				}
				if (waits > 0) {
					waits = Sf_EqI(&f, Sf_Attr(&f, p, "LLogicIdle"), 0);
				}
				if (waits > 0) {
					waits = Sf_EqI(&f, TMG("tamayoHasJustReceivedMoney"), 0);
				}
				if (waits > 0) {
					Tm_Refill(&f, e, p);
				}
			}
		}
	}
	Sf_Later(&f, Sf_Int(&f, 1), TMG("CheckTamayoMoney"), Sf_Build(&f, "()"), "CheckTamayoMoney");
	Sf_Own(&f, TM, "OnBankerSteal", "()");
	return Sf_None(&f);
}

/* RestoreTamayoState(): after the talk Corruptus walks again from Porkins (or his start) */
static PyObject *TalkToMayor_RestoreTamayoState(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, TMG("functionaryTamayoName"));
	if (Sf_True(&f, e) <= 0) {
		return Sf_None(&f);
	}
	PyObject *p = Sf_Entity(&f, TMG("porkinsName"));
	int there = Sf_True(&f, p);
	if (there >= 0) {
		Sf_SetAttr(&f, e, "LLogicOrigin", there ? Sf_Attr(&f, p, "Pos") : TMG("tamayoStartOrigin"));
	}
	Lv_Stay(&f, e, 0, Sf_Str(&f, ""));
	Sf_SetAttr(&f, e, "LLogicProbTalkWhenWalking", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, e, "LLogicIgnoreBankerAttack", 0);
	return Sf_None(&f);
}

/* OnBankerSteal(): Corruptus without money ends the stealing (the mayor calls); low on money the bankers leave him;
 * Porkins without money once draws the mayor's call */
static PyObject *TalkToMayor_OnBankerSteal(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, TMG("functionaryTamayoName"));
	if (Sf_True(&f, e) > 0) {
		int empty = Sf_Compare(&f, Sf_Attr(&f, e, "Money"), "<=", Sf_Int(&f, 0));
		if (empty > 0) {
			Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
			Sf_SetAttrS(&f, e, "UseFunc", "CharsNPC.GenericNPCSpeech");
			PyObject *p = Sf_Entity(&f, TMG("porkinsName"));
			if (Sf_True(&f, p) > 0) {
				Sf_SetAttrS(&f, p, "UseFunc", "CharsNPC.GenericNPCSpeech");
			}
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_Campaign1_RemoteMessage1", "Mayor", TMG("EndOfStealFunctionary"), Sf_Build(&f, "()"));
			Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CheckTamayoMoney");
			return Sf_None(&f);
		}
		if (empty == 0 && Sf_Compare(&f, Sf_Attr(&f, e, "Money"), "<=", TMG("MIN_TAMAYO_MONEY")) > 0) {
			Sf_SetAttrI(&f, e, "LLogicIgnoreBankerAttack", 1);
		}
	}
	PyObject *p = Sf_Entity(&f, TMG("porkinsName"));
	int poor = Sf_True(&f, p);
	if (poor > 0) {
		poor = Sf_Compare(&f, Sf_Attr(&f, p, "Money"), "<=", Sf_Int(&f, 0));
	}
	if (poor > 0) {
		poor = Sf_True(&f, TMG("porkinsHasMoney"));
	}
	if (poor > 0) {
		Sf_SetGlobal(&f, TM, "porkinsHasMoney", Sf_Int(&f, 0));
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_Campaign1_RemoteMessage2", "Mayor");
	}
	return Sf_None(&f);
}

/* EndOfStealFunctionary(): the mayor is the target again */
static PyObject *TalkToMayor_EndOfStealFunctionary(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "TalkToMayor2");
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Py_None);
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToMayor2_MissionText1", 1);
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Mayor_Default");
	return Sf_None(&f);
}

/* SetNextMission(): Mission_DestroyPorkinsTransports in Commercial, two police blueprints more, Rusty calls in 10 s */
static PyObject *TalkToMayor_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Ask(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_DestroyPorkinsTransports", "Commercial");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_Campaign2_MissionText1", 1);
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Lv_SaveAppend(&f, "PoliceBlueprints.Ships", Lv_SaveList(&f, "PoliceBlueprints.Ships", TM), Sf_Str(&f, "SPoli5"));
	Lv_SaveAppend(&f, "PoliceBlueprints.Engines", Lv_SaveList(&f, "PoliceBlueprints.Engines", TM), Sf_Str(&f, "MPoli5"));
	Sf_Import(&f, "Scorer");
	PyObject *call = Sf_Build(&f, "(ssO(ii))", "Mission_Campaign2_RemoteMessage5", "Rusty", Sf_Mod(&f, "Scorer", "ShowMissionScorer"), 0, 2);
	Sf_Later(&f, Sf_Float(&f, 10.0), Sf_Mod(&f, "Speech", "AddRemoteCom"), call, NULL);
	return Sf_None(&f);
}

/* MissionScorer(id): Corruptus' money as a bar */
static PyObject *TalkToMayor_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Lv_ScorerTextBegin(&f, id, "TamayoMoney", 0);
	Lv_ScorerSet(&f, id, "TamayoMoney", "Text", Sf_Cat(&f, "Os", Sf_Lang(&f, Sf_Str(&f, "Mission_Campaign1_Scorer1")), ":"));
	Lv_ScorerTextEnd(&f, id, "TamayoMoney");
	Sf_Ask(&f, "SScorer", "Add", "(Oss)", id, "TamayoMoneyBar", "Bar");
	Sf_Ask(&f, "Scorer", "MissionScorerBar_SetDefaults", "(OsOOO)", id, "TamayoMoneyBar", Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorBack"),
		Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorMain"), Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorBlack"));
	Sf_Ask(&f, "SScorer", "Set", "(Ossi)", id, "TamayoMoneyBar", "IsAutoUpdatable", 0);
	Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "TamayoMoneyBar", "MaxValue", TMG("MAX_TAMAYO_MONEY"));
	Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "TamayoMoneyBar", "Value", TMG("tamayoMoney"));
	return Sf_None(&f);
}

PyMethodDef g_TownhallTalkToMayorMethods[] = {
	{ "InitStatus", TalkToMayor_InitStatus, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", TalkToMayor_AfterBrowseNPC, METH_VARARGS, NULL },
	{ "CreateMayorInit", TalkToMayor_CreateMayorInit, METH_VARARGS, NULL },
	{ "CreateBishopPorkins", TalkToMayor_CreateBishopPorkins, METH_VARARGS, NULL },
	{ "CreateFunctionaryTamayo", TalkToMayor_CreateFunctionaryTamayo, METH_VARARGS, NULL },
	{ "Init", TalkToMayor_Init, METH_VARARGS, NULL },
	{ "MayorSpeech", TalkToMayor_MayorSpeech, METH_VARARGS, NULL },
	{ "BishopSpeech", TalkToMayor_BishopSpeech, METH_VARARGS, NULL },
	{ "FunctionarySpeech", TalkToMayor_FunctionarySpeech, METH_VARARGS, NULL },
	{ "StealFunctionary", TalkToMayor_StealFunctionary, METH_VARARGS, NULL },
	{ "CheckTamayoMoney", TalkToMayor_CheckTamayoMoney, METH_VARARGS, NULL },
	{ "RestoreTamayoState", TalkToMayor_RestoreTamayoState, METH_VARARGS, NULL },
	{ "OnBankerSteal", TalkToMayor_OnBankerSteal, METH_VARARGS, NULL },
	{ "EndOfStealFunctionary", TalkToMayor_EndOfStealFunctionary, METH_VARARGS, NULL },
	{ "SetNextMission", TalkToMayor_SetNextMission, METH_VARARGS, NULL },
	{ "MissionScorer", TalkToMayor_MissionScorer, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};
