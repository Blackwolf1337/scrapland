/*
 * The scripts of the level Police in C (data01.packed levels/police/scripts, issue #2): the Map module (the NPCs,
 * Ficus' body, the teleporters and gatekeepers, the fake traffic, the costume of the viscoson, the return from the
 * jail), MapSnd.SndElements (the sounds of the map elements; the rest of MapSnd is data), MapPlanes (the police
 * blueprints to photograph) and the missions Mission_BeatPickus, Mission_FicusDead, Mission_GetAuthorization, Mission_InitEpilogue, Mission_JailBreak and Mission_PoliceBossCheats. The plain functions
 * are data (data/levels/police/<module>.def): MapPlanes.InitStatus, the scene loaders (Mission_BeatPickus.LoadScene,
 * Mission_GetAuthorization.LoadScene, Mission_PoliceBossCheats.LoadPoliFightScene) and the functions that only call
 * others (PreHireThugs, PreBeatPickus, OnAlarmDeactivated, PreTalkToTraitoriusAgain and SetNextMission of
 * Mission_BeatPickus, InitStatus, AfterDtritusSpeechShown and PreAfterDeepThroatMessageShown of Mission_FicusDead,
 * GetAuthorization2, PreFreeHuman and OnMainCharDeath of Mission_GetAuthorization, Mission_InitEpilogue.PreSetNextMission,
 * Mission_PoliceBossCheats.InitBossCheats).
 *
 * The module variables and the module code stay in the scripts: Map (Libraries, ficus, the discardable characters,
 * the jail escape at its load), MapPlanes (Naves, Motores), Mission_BeatPickus (followPlayerVel, thugsStopped,
 * launcher, plansNames, numPlansWithPhoto, alreadyTalked, FXSceneBeatPickusFile, traitorius), Mission_FicusDead
 * (ficus, numCrazyGamblerComs), Mission_GetAuthorization (maxHumanDist, minHumanDist, filledByUser, scenePlayed,
 * alarmFallDelta, human, conv, dd), Mission_JailBreak (PuffTurn, PuffWalk, ActPress), Mission_PoliceBossCheats
 * (NextMissionData, SpawnConversionPoint, the poker players PoliceBoss, Dumb1, Dumb2 and their cards PoliceBossc,
 * Dumb1c, Dumb2c).
 */
#include "scrap/scrap.h"
#include "Levels.h"

#define MAP "Map"
#define MP "MapPlanes"
#define BP "Mission_BeatPickus"
#define FD "Mission_FicusDead"
#define GA "Mission_GetAuthorization"
#define IE "Mission_InitEpilogue"
#define JB "Mission_JailBreak"
#define PC "Mission_PoliceBossCheats"

/* e = Scrap.CreateEntity(dummy[0], dummy[1][0], dummy[1][1], dummy[1][2], type); e.Ang = dummy[2][0], dummy[2][1], 0 */
static PyObject *Po_FromDummy(scriptFrame_t *f, PyObject *dummy, const char *type) {
	PyObject *pos = Sf_Item(f, dummy, 1);
	PyObject *e = Sf_Ask(f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Item(f, dummy, 0), Sf_Item(f, pos, 0), Sf_Item(f, pos, 1),
		Sf_Item(f, pos, 2), type);
	PyObject *ang = Sf_Item(f, dummy, 2);
	Sf_SetAttr(f, e, "Ang", Sf_Build(f, "(OOi)", Sf_Item(f, ang, 0), Sf_Item(f, ang, 1), 0));
	return e;
}

/* ---- Map ---- */

/* CreateFicusDeadClass(): the class of Ficus' body; dead and still, or in his loop in Mission_FicusDead */
static PyObject *PoliceMap_CreateFicusDeadClass(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	int fd = Lv_SaveIs(&f, "Mission.Library", "Mission_FicusDead");
	Sf_Import(&f, "Init");
	Sf_Ask(&f, "SAct", "CreateClass", "(s)", "FicusDead");
	Sf_Ask(&f, "SAct", "SetCls", "(ss)", "ModelFilePath", "Models/Chars/Police/Police.m3d");
	Sf_Ask(&f, "SAct", "CreateAction", "(s)", "FicusDead");
	Sf_Ask(&f, "SAct", "SetAct", "(ss)", "AnmFilePath", "Models/Chars/Police/Anm/FicusDead.M3D");
	Sf_Ask(&f, "SAct", "SetAct", "(si)", "isLoop", 0);
	if (fd > 0 && !f.failed) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Police.FicusDeath", "1");
		Sf_Ask(&f, "SAct", "CreateAction", "(s)", "FicusLoop");
		Sf_Ask(&f, "SAct", "SetAct", "(ss)", "AnmFilePath", "Models/Chars/Police/Anm/FicusLoop.M3D");
		Sf_Ask(&f, "SAct", "SetAct", "(si)", "isLoop", 1);
		Sf_Ask(&f, "SAct", "SetCls", "(ss)", "DefAction", "FicusLoop");
	} else {
		Sf_Ask(&f, "SAct", "SetCls", "(ss)", "DefAction", "FicusDead");
	}
	return Sf_None(&f);
}

/* CreateFicus(): Ficus' body at its dummy (the module variable ficus) */
static PyObject *PoliceMap_CreateFicus(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *dummy = Lv_Dummy(&f, "DM_Mission_FicusDead", 1);
	if (Sf_Compare(&f, dummy, "!=", Py_None) > 0) {
		PyObject *pos = Sf_Item(&f, dummy, 1);
		PyObject *e = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Item(&f, dummy, 0), Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
			Sf_Item(&f, pos, 2), "WithLife");
		Sf_SetGlobal(&f, MAP, "ficus", e);
		PyObject *ang = Sf_Item(&f, dummy, 2);
		Sf_SetAttr(&f, e, "Ang", Sf_Build(&f, "(OOi)", Sf_Item(&f, ang, 0), Sf_Item(&f, ang, 1), 0));
		Sf_SetAttrS(&f, e, "ActCtrl", "FicusDead");
		Sf_SetAttrI(&f, e, "ActiveCMask", 0);
		Sf_SetAttrI(&f, e, "MapCollision", 1);
		Sf_SetAttrI(&f, e, "UseAngSel", 1);
		Sf_SetAttrI(&f, e, "UseAbsAngSel", 0);
		Sf_SetAttrI(&f, e, "VRadius", 150);
		Sf_SetAttrI(&f, e, "UseMapCheck", 0);
	}
	return Sf_None(&f);
}

/* InitStatus(LevelPath): an indoor map next to Outskirts; the police boss, the elevators, the NPCs that come back */
static PyObject *PoliceMap_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	static const char *const mods[] = { "Init", "CharsNPC", "Elevator", "InTraffic" };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	Sf_SetMod(&f, "Init", "OutArea", Sf_Str(&f, "Outskirts"));
	int boss = Lv_SaveIs(&f, "PoliceBossAtPolice", "1");
	if (boss > 0) {
		int m = Sf_True(&f, Sf_Ask(&f, "Init", "isMission", "()"));
		boss = m < 0 ? -1 : !m;
	}
	if (boss > 0) {
		Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "PoliceBoss_Default");
	}
	Sf_Ask(&f, "Elevator", "SetDef", "(ss)", "Metro", "DM_Door_Metro_Press");
	Sf_Ask(&f, "Elevator", "SetDef", "(ss)", "MetroExt", "DM_Door_Metro_Press");
	Sf_Ask(&f, "Elevator", "SetDef", "(ss)", "Elevator", "DM_Door_Elevator_Outskirts");
	Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Sentinel", 4);
	Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Police", 6);
	Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Nurse", 2);
	Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Desktop", 2);
	if (Lv_PcOr(&f, MAP, "_BankDirector") > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "BankDirector", 2);
	}
	if (Lv_PcOr(&f, MAP, "_Bishop") > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Bishop", 2);
	}
	Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Functionary", 3);
	int m = Lv_PcOr(&f, MAP, "_Messenger");
	if (m > 0) {
		Sf_Ask(&f, "CharsNPC", "SetReposChars", "(si)", "Messenger", 2);
	} else if (m == 0) {
		Sf_Import(&f, "Messenger");
		Sf_SetMod(&f, "Messenger", "MakeMDP", Sf_Int(&f, 0));
	}
	return Sf_None(&f);
}

/* Init(LevelPath): Ficus' body, the teleporters, the gatekeepers of the security doors, the fake traffic, the costume
 * of the viscoson, the fade after the escape from the jail */
static PyObject *PoliceMap_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Own(&f, MAP, "CreateFicusDeadClass", "()");
	Sf_Own(&f, MAP, "CreateFicus", "()");
	static const char *const mods[] = { "Elevator", "Teleport", "Metro", "InTraffic" };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_Ask(&f, "Teleport", "Link", "(ss)", "DM_Door_Teleporter_01", "DM_Door_Teleporter_05");
	Sf_Ask(&f, "Teleport", "Link", "(ss)", "DM_Door_Teleporter_06", "DM_Door_Teleporter_07");
	Sf_Ask(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Outskirts", "Outskirts");
	Sf_Ask(&f, "Metro", "SetAsMetro", "(s)", "DM_Door_Metro_Press");
	Sf_Import(&f, "GateKeeper");
	Sf_Ask(&f, "GateKeeper", "SetDoor", "(ss)", "GateKeeper_07", "DM_Door_Teleporter_07");
	int dead = Lv_SaveIs(&f, "Police.FicusDeath", "1");
	if (dead >= 0) {
		Sf_Ask(&f, "GateKeeper", "DoorFoe", "(sO)", "GateKeeper_07", Sf_Mod(&f, "GateKeeper", dead ? "PoliceGroupGateKeeper" : "NoOneGroupGateKeeper"));
	}
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Str(&f, "DM_Door_Teleporter_07")), "UseFunc", "GateKeeper.UseDoor");
	PyObject *e = Sf_Entity(&f, Sf_Str(&f, "DM_Door_PoliceSegurity_00"));
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "Action", "Closed");
		Sf_SetAttrS(&f, e, "DefLoop", "Closed");
		Sf_SetAttrS(&f, e, "OnObjSlot", "");
		Sf_Ask(&f, "GateKeeper", "DoorFoe", "(sO)", "GateKeeper_08", Sf_Mod(&f, "GateKeeper", "NoOneGroupGateKeeper"));
	}
	Sf_SetMod(&f, "InTraffic", "N_TrafficList", Sf_Build(&f, "[sss]", "TLight1B", "TLight2B", "TLight3B"));
	Sf_SetMod(&f, "InTraffic", "N_NumTraffic", Sf_Int(&f, 25));
	Sf_Ask(&f, "InTraffic", "LoadFakeTraffic", "()");
	Sf_Ask(&f, "InTraffic", "FakeTraffic", "()");
	if (Lv_SaveIs(&f, "CostumeAtPolice", "1") > 0) {
		PyObject *dummy = Lv_Dummy(&f, "DM_Mission_Viscoson_00", 1);
		if (Sf_Compare(&f, dummy, "!=", Py_None) > 0) {
			PyObject *costume = Po_FromDummy(&f, dummy, "WithLife");
			Sf_Method(&f, costume, "SetModel", "(s)", "Models/Misc/Viscoson/Viscoson.m3d");
			Sf_SetAttrI(&f, costume, "ActiveCMask", 0);
			Sf_SetAttrI(&f, costume, "MapCollision", 1);
		}
	}
	int jail = Lv_SaveIs(&f, "Spawn", "DM_Mission_JailBreak_End");
	if (jail > 0) {
		jail = Lv_SaveIs(&f, "ComeFrom", "");
	}
	if (jail > 0) {
		Lv_Fade(&f, lv_black, lv_black, 1.0);
		Sf_Later(&f, Sf_Float(&f, 0.1), Sf_Mod(&f, "SScorer", "Fade"), Sf_Build(&f, "(i(iiii)(iiii)d)", 0, 0, 0, 0, 255, 0, 0, 0, 0, 1.0), NULL);
	}
	return Sf_None(&f);
}

PyMethodDef g_PoliceMapMethods[] = {
	{ "CreateFicusDeadClass", PoliceMap_CreateFicusDeadClass, METH_VARARGS, NULL },
	{ "CreateFicus", PoliceMap_CreateFicus, METH_VARARGS, NULL },
	{ "InitStatus", PoliceMap_InitStatus, METH_VARARGS, NULL },
	{ "Init", PoliceMap_Init, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapSnd ---- */

/* the sound elements of the map (the rest of MapSnd is data/levels/police/mapsnd.def) */
static const lvSndElement_t police_snd[] = {
	{ .name = "PoliceFXLighting01", .front = 1, .loop = "loop-elect2", .vol = 0.0 },
	{ .name = "PoliceFXLighting02", .front = 1, .loop = "loop-elect2", .vol = 0.0 },
	{ .name = "PoliceFXLighting03", .front = 1, .loop = "loop-elect2", .vol = 0.0 },
	{ .name = "TownhallHelix", .pc = 1, .front = 1, .loop = "ventilador3", .vol = 0.4, .pitch = -4, .hasPitch = 1 },
	{ .name = "PoliceSlight", .pc = 1, .front = 1, .loop = "light1", .vol = 0.1 },
	{ .name = "PoliceSLLight", .pc = 1, .front = 1, .loop = "light1", .vol = 0.1 },
	{ .name = NULL },
};

/* SndElements(Name, x, y, z, angx, angy) */
static PyObject *PoliceMapSnd_SndElements(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_SndElements(args, police_snd);
}

PyMethodDef g_PoliceMapSndMethods[] = {
	{ "SndElements", PoliceMapSnd_SndElements, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapPlanes ---- */

#define MPG(name) Sf_Global(&f, MP, name)

/* Init(LevelPath): a photo object for every blueprint dummy whose blueprint is still to take */
static PyObject *MapPlanes_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "CharAct");
	Sf_SetMod(&f, "CharAct", "TakenPhotoMap", MPG("TakenPhoto"));
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_LocalPlane_*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "==", Py_None) != 0) {
			break;
		}
		PyObject *plane = Sf_Slice(&f, Sf_Item(&f, dummy, 0), 14, 0x7fffffff);
		if (Sf_In(&f, plane, Sf_Op(&f, MPG("Naves"), '+', MPG("Motores"))) > 0) {
			PyObject *ang = Sf_Item(&f, dummy, 2);
			PyObject *pln = Sf_Ask(&f, "MissionsFuncs", "CreatePhotoObject", "(OOO)", Sf_Cat(&f, "sO", "Plano", Sf_Slice(&f, Sf_Item(&f, dummy, 0), 14, 0x7fffffff)),
				Sf_Item(&f, dummy, 1), Sf_Build(&f, "(OOi)", Sf_Item(&f, ang, 0), Sf_Item(&f, ang, 1), 0));
			Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", pln, 0);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* TakenPhoto(Name): the photo of a police blueprint: the ship or the engine goes to the hangar lists (1), else 0;
 * None for other photos */
static PyObject *MapPlanes_TakenPhoto(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	if (Sf_EqS(&f, Sf_Slice(&f, name, 0, 5), "Plano") <= 0) {
		return Sf_None(&f);
	}
	long res = 0;
	Sf_Import(&f, "string");
	Sf_Import(&f, "RaceMaker");
	PyObject *plano = Sf_Slice(&f, name, 5, 0x7fffffff);
	int ship = Sf_In(&f, plano, MPG("Naves")), engine = ship == 0 ? Sf_In(&f, plano, MPG("Motores")) : 0;
	if (ship > 0) {
		Sf_Method(&f, MPG("Naves"), "remove", "(O)", plano);
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "PoliceBlueprints.Ships", Sf_Repr(&f, MPG("Naves")));
		Lv_SaveAppend(&f, "Hangar.shipsToEditList", Lv_SaveList(&f, "Hangar.shipsToEditList", MP), plano);
		PyObject *key = Sf_Cat(&f, "sO", "SHIP_", Sf_Ask(&f, "string", "upper", "(O)", plano));
		PyObject *text = Sf_Ask(&f, "string", "replace", "(OsO)", Sf_Lang(&f, Sf_Str(&f, "OnPhotoShipBlueprints")), "<n1>", Sf_Lang(&f, key));
		Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", text);
		res = 1;
	} else if (engine > 0) {
		Sf_Method(&f, MPG("Motores"), "remove", "(O)", plano);
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "PoliceBlueprints.Engines", Sf_Repr(&f, MPG("Motores")));
		Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.availableEnginesList");
		PyObject *lst = Lv_SaveList(&f, "Hangar.availableEnginesList", MP);
		Lv_SaveAppend(&f, "Hangar.availableEnginesList", lst, Sf_Ask(&f, "string", "upper", "(O)", plano));
		PyObject *key = Sf_Cat(&f, "sO", "SHIP_S", Sf_Ask(&f, "string", "upper", "(O)", Sf_Slice(&f, plano, 1, 0x7fffffff)));
		PyObject *text = Sf_Ask(&f, "string", "replace", "(OsO)", Sf_Lang(&f, Sf_Str(&f, "OnPhotoEngineBlueprints")), "<n1>", Sf_Lang(&f, key));
		Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", text);
		res = 1;
	}
	if (res && !f.failed) {
		Sf_Import(&f, "SaveGame");
		Sf_SetAttrI(&f, Sf_Entity(&f, name), "ArrowCanView", 0);
		Sf_Ask(&f, "Scrap", "IncSaveVar", "(s)", "NewBluePrintAvaliable");
		Sf_Ask(&f, "SaveGame", "CheckAllBlueprints", "()");
	}
	return Sf_Return(&f, Sf_Int(&f, res));
}

PyMethodDef g_PoliceMapPlanesMethods[] = {
	{ "Init", MapPlanes_Init, METH_VARARGS, NULL },
	{ "TakenPhoto", MapPlanes_TakenPhoto, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_BeatPickus ---- */

#define BPG(name) Sf_Global(&f, BP, name)
#define BPGF(name) Sf_Global(f, BP, name)

/* the entity Police_Thug<i> */
static PyObject *Bp_Thug(scriptFrame_t *f, int i) {
	return Sf_Entity(f, Sf_Cat(f, "sO", "Police_Thug", Sf_ReprI(f, i)));
}

/* InitStatus(LevelPath): the five police thugs (unless the plans are next) and Traitorius */
static PyObject *BeatPickus_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState"), "!=", Sf_Str(&f, "GetFuncShipPlans")) > 0) {
		for (int i = 1; i < 6; i++) {
			char name[0x20];
			Str_Printf(name, sizeof(name), "Police_Thug%d", i);
			Lv_MissionChar(&f, name, BP, "OnThugRespawn");
		}
	}
	Lv_MissionChar(&f, "Functionary_Traitorius", BP, "OnTraitoriusRespawn");
	return Sf_None(&f);
}

/* Init(LevelPath): Pickus and the thugs wait, Traitorius talks, the scene and Pickus beaten, the state of the mission */
static PyObject *BeatPickus_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "Set", "(si)", "LittlePoliceAsksForMoney", 0);
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "NotCleanableByGear", "Police");
	PyObject *state = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState");
	int early = Sf_Compare(&f, state, "!=", Sf_Str(&f, "GetFuncShipPlans"));
	if (early > 0) {
		early = Sf_Compare(&f, state, "!=", Sf_Str(&f, "TalkToTraitoriusAgain"));
	}
	if (early > 0) {
		Sf_Own(&f, BP, "DisableIAFunctionary", "(s)", "Pickus");
		Sf_Own(&f, BP, "SetThugsState", "()");
	}
	Sf_SetGlobal(&f, BP, "traitorius", Sf_Own(&f, BP, "DisableIAFunctionary", "(s)", "Traitorius"));
	Sf_Own(&f, BP, "LoadScene", "()");
	Sf_Own(&f, BP, "CreatePickusBeatenClass", "()");
	if (Sf_EqS(&f, state, "TalkToTraitorius") > 0) {
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, BPG("traitorius"), "Name"));
	} else if (Sf_EqS(&f, state, "HireThugs") > 0 || Sf_EqS(&f, state, "BeatPickus") > 0) {
		Sf_Own(&f, BP, "HireThugs", "()");
	} else if (Sf_EqS(&f, state, "GetFuncShipPlans") > 0) {
		Sf_Own(&f, BP, "GetFuncShipPlans", "()");
	} else if (Sf_EqS(&f, state, "TalkToTraitoriusAgain") > 0) {
		Sf_Own(&f, BP, "TalkToTraitoriusAgain", "()");
	}
	return Sf_None(&f);
}

/* DisableIAFunctionary(name): Functionary_<name> stays and talks (Traitorius through TraitoriusSpeech); the entity,
 * None when it is not there */
static PyObject *BeatPickus_DisableIAFunctionary(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	int tr = Sf_EqS(&f, name, "Traitorius");
	PyObject *e = Sf_Entity(&f, Sf_Cat(&f, "sO", "Functionary_", name));
	if (Sf_True(&f, e) == 0) {
		return Sf_None(&f);
	}
	if (tr > 0) {
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_BeatPickus.TraitoriusSpeech");
	}
	Sf_SetAttrI(&f, e, "LLogicDefaultState", 6);
	Sf_SetAttrS(&f, e, "LLogicDefaultTarget", "");
	Sf_SetAttrI(&f, e, "LLogicState", 6);
	Sf_SetAttrS(&f, e, "LLogicTarget", "");
	Sf_SetAttr(&f, e, "LLogicProbTalkWhenWalking", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, e, "LLogicIdle", 1);
	Sf_SetAttrI(&f, e, "LLogicIdle", 0);
	Sf_SetAttr(&f, e, "Descriptor", Sf_Lang(&f, Sf_Cat(&f, "sO", "CharName_", name)));
	return Sf_Return(&f, e);
}

/* OnTraitoriusRespawn(tr): Traitorius back: his name and his talk for the state of the mission */
static PyObject *BeatPickus_OnTraitoriusRespawn(PyObject *self, PyObject *args) {
	PyObject *tr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &tr)) {
		return NULL;
	}
	Sf_SetGlobal(&f, BP, "traitorius", tr);
	Sf_SetAttr(&f, BPG("traitorius"), "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Traitorius")));
	PyObject *state = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState");
	if (Sf_EqS(&f, state, "TalkToTraitorius") > 0) {
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, BPG("traitorius"), "Name"));
		Sf_SetAttrS(&f, BPG("traitorius"), "UseFunc", "Mission_BeatPickus.TraitoriusSpeech");
	} else if (Sf_EqS(&f, state, "TalkToTraitoriusAgain") > 0) {
		Sf_Own(&f, BP, "TalkToTraitoriusAgain", "()");
	} else {
		Sf_SetAttrS(&f, BPG("traitorius"), "UseFunc", "Mission_BeatPickus.TraitoriusSpeech");
	}
	return Sf_None(&f);
}

/* OnThugRespawn(thug): a thug back: he follows the player during the beating, or the thugs regroup */
static PyObject *BeatPickus_OnThugRespawn(PyObject *self, PyObject *args) {
	PyObject *thug;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &thug)) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) > 0) {
		if (Lv_SaveIs(&f, "MissionState", "BeatPickus") > 0) {
			Sf_Ask(&f, "Scrap", "Set", "(sO)", "GenCharSpecialChaseSpeed", BPG("followPlayerVel"));
			Sf_Own(&f, BP, "SetState", "(OiO)", thug, 5, Sf_Attr(&f, usr, "Name"));
		} else if (Lv_SaveIs(&f, "MissionState", "HireThugs") > 0) {
			Sf_Own(&f, BP, "SetThugsState", "()");
		}
	}
	return Sf_None(&f);
}

/* SetThugsState(vel=0.4): vel 0 stops the leader (the others follow him), up to 0.4 the leader walks his route with
 * the others behind, above all thugs follow the player (when he is Dtritus) */
static PyObject *BeatPickus_SetThugsState(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "vel", NULL };
	PyObject *vel = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &vel)) {
		return NULL;
	}
	vel = vel ? Sf_Ref(&f, vel) : Sf_Float(&f, 0.4);
	PyObject *thug1 = Bp_Thug(&f, 1);
	if (Sf_True(&f, thug1) == 0) {
		Sf_Later(&f, Sf_Float(&f, 1.0), BPG("SetThugsState"), Sf_Build(&f, "(O)", vel), NULL);
		return Sf_None(&f);
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int follow = Sf_Compare(&f, vel, ">", Sf_Float(&f, 0.4));
	if (follow > 0) {
		follow = Sf_True(&f, usr);
	}
	if (follow > 0) {
		follow = Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Dtritus");
	}
	Sf_Ask(&f, "Scrap", "Set", "(sO)", "GenCharSpecialChaseSpeed", vel);
	if (follow == 0) {
		int stop = Sf_Compare(&f, vel, "==", Sf_Float(&f, 0.0));
		if (stop > 0) {
			Sf_Own(&f, BP, "SetState", "(Ois)", thug1, 6, "");
			Sf_SetAttrS(&f, thug1, "UseFunc", "Mission_BeatPickus.ThugNormalSpeech");
		} else if (stop == 0) {
			Sf_Own(&f, BP, "SetState", "(Ois)", thug1, 0, "");
			int talk = Lv_SaveIs(&f, "MissionState", "TalkToTraitorius");
			if (talk >= 0) {
				Sf_SetAttrS(&f, thug1, "UseFunc", talk ? "CharsNPC.GenericNPCSpeech" : "Mission_BeatPickus.ThugLeaderSpeech");
			}
		}
	} else if (follow > 0) {
		Sf_Own(&f, BP, "SetState", "(OiO)", thug1, 5, Sf_Attr(&f, usr, "Name"));
		Sf_SetAttrS(&f, thug1, "UseFunc", "Mission_BeatPickus.ThugNormalSpeech");
	}
	for (int i = 2; i < 6 && !f.failed; i++) {
		int mark = f.n;
		PyObject *thug = Bp_Thug(&f, i);
		if (Sf_True(&f, thug) > 0) {
			if (follow) {
				Sf_Own(&f, BP, "SetState", "(OiO)", thug, 5, Sf_Attr(&f, usr, "Name"));
				Sf_SetAttrS(&f, thug, "UseFunc", "Mission_BeatPickus.ThugNormalSpeech");
			} else {
				Sf_Own(&f, BP, "SetState", "(OiO)", thug, 5, Sf_Attr(&f, thug1, "Name"));
				Sf_SetAttrS(&f, thug, "UseFunc", "CharsNPC.GenericNPCSpeech");
			}
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* SetState(thug, state, targetName): the state of the local logic (6 stays where he stands) */
static PyObject *BeatPickus_SetState(PyObject *self, PyObject *args) {
	PyObject *thug, *state, *target;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &thug, &state, &target)) {
		return NULL;
	}
	if (Sf_EqI(&f, state, 6) > 0) {
		Sf_SetAttr(&f, thug, "LLogicOrigin", Sf_Attr(&f, thug, "Pos"));
	}
	Sf_SetAttr(&f, thug, "LLogicDefaultState", state);
	Sf_SetAttr(&f, thug, "LLogicDefaultTarget", target);
	Sf_SetAttr(&f, thug, "LLogicState", state);
	Sf_SetAttr(&f, thug, "LLogicTarget", target);
	Sf_SetAttrI(&f, thug, "LLogicIdle", 1);
	Sf_SetAttrI(&f, thug, "LLogicIdle", 0);
	return Sf_None(&f);
}

/* Scrap.GetEntity(UsrName).ActCtrl == ctrl */
static int Bp_UsrIs(scriptFrame_t *f, PyObject *usr, const char *ctrl) {
	return Sf_EqS(f, Sf_Attr(f, Sf_Entity(f, usr), "ActCtrl"), ctrl);
}

/* Speech.AddNI(msg, ans) of the keys prefix + "InitMsg1" and so on (the pairs end with NULL) */
static void Bp_AddNI(scriptFrame_t *f, const char *const *pairs) {
	for (; *pairs; pairs += 2) {
		Sf_Ask(f, "Speech", "AddNI", "(ss)", pairs[0], pairs[1]);
	}
}

/* TraitoriusSpeech(UsrName, CharName, optId, FullOption): Traitorius talks to a functionary about the thugs, to
 * Dtritus about flowers; the others get the generic talk */
static PyObject *BeatPickus_TraitoriusSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "CharsNPC");
	Lv_Speakers(&f, usr, ch);
	if (Bp_UsrIs(&f, usr, "Functionary") > 0) {
		int talk = Lv_SaveIs(&f, "MissionState", "TalkToTraitorius");
		if (talk > 0) {
			static const char *const ni[] = { "Mission_FlowerPower_Traitorius_InitMsg1", "Mission_FlowerPower_Traitorius_InitAns1",
				"Mission_FlowerPower_Traitorius_1Msg", "Mission_FlowerPower_Traitorius_1Ans", "Mission_FlowerPower_Traitorius_1_1Msg",
				"Mission_FlowerPower_Traitorius_1_1Ans", NULL };
			Lv_OnEnd(&f, BP, "PreHireThugs", Sf_Build(&f, "(O)", ch));
			Sf_Ask(&f, "Speech", "NISpeech", "(s)", "Functionary");
			Bp_AddNI(&f, ni);
			Sf_Ask(&f, "Speech", "RunNI", "()");
		} else if (talk == 0) {
			Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_HireThugs_Traitorius_InitMsg1", "Mission_HireThugs_Traitorius_InitAns1");
		}
	} else if (Bp_UsrIs(&f, usr, "Dtritus") > 0) {
		if (Sf_EqS(&f, opt, "Talk") > 0) {
			Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_FlowerPower_Traitorius_InitMsg2", "Mission_FlowerPower_Traitorius_InitAns2");
		}
	} else {
		Sf_Ask(&f, "CharsNPC", "GenericNPCSpeech", "(OOOO)", usr, ch, opt, full);
	}
	return Sf_None(&f);
}

/* HireThugs(): the leader of the thugs is the target (as soon as he is there) */
static PyObject *BeatPickus_HireThugs(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *thug1 = Bp_Thug(&f, 1);
	if (Sf_True(&f, thug1) == 0) {
		Sf_Later(&f, Sf_Float(&f, 1.0), BPG("HireThugs"), Sf_Build(&f, "()"), NULL);
		return Sf_None(&f);
	}
	Sf_SetAttrS(&f, thug1, "UseFunc", "Mission_BeatPickus.ThugLeaderSpeech");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, thug1, "Name"));
	return Sf_None(&f);
}

/* ThugLeaderSpeech(UsrName, CharName, optId, FullOption): the leader takes Dtritus' job, PreBeatPickus at the end */
static PyObject *BeatPickus_ThugLeaderSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		int dt = Bp_UsrIs(&f, usr, "Dtritus");
		if (dt > 0) {
			static const char *const ni[] = { "Mission_HireThugs_Thug_InitMsg1", "Mission_HireThugs_Thug_InitAns1", "Mission_HireThugs_Thug_1Msg",
				"Mission_HireThugs_Thug_1Ans", NULL };
			Lv_OnEnd(&f, BP, "PreBeatPickus", Sf_Build(&f, "(O)", ch));
			Sf_Ask(&f, "Speech", "NISpeech", "(s)", "Police");
			Bp_AddNI(&f, ni);
			Sf_Ask(&f, "Speech", "RunNI", "()");
		} else if (dt == 0) {
			Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_HireThugs_Thug_InitMsg2", "Mission_HireThugs_Thug_InitAns2");
		}
	}
	return Sf_None(&f);
}

/* ThugNormalSpeech(UsrName, CharName, optId, FullOption): a thug talks; to Dtritus, and then they follow him */
static PyObject *BeatPickus_ThugNormalSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		int dt = Bp_UsrIs(&f, usr, "Dtritus");
		if (dt > 0) {
			Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_HireThugs_Thug_InitMsg3", "Mission_HireThugs_Thug_InitAns3");
			Sf_Own(&f, BP, "SetThugsState", "(O)", BPG("followPlayerVel"));
		} else if (dt == 0) {
			Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_HireThugs_Thug_InitMsg2", "Mission_HireThugs_Thug_InitAns2");
		}
	}
	return Sf_None(&f);
}

/* BeatPickus(): the thugs follow the player to Pickus; the alarm hooks, the launcher of the scene at Pickus, the
 * distance check */
static PyObject *BeatPickus_BeatPickus(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *pickus = Sf_Entity(&f, Sf_Str(&f, "Functionary_Pickus"));
	if (Sf_True(&f, pickus) == 0) {
		Sf_Later(&f, Sf_Float(&f, 1.0), BPG("BeatPickus"), Sf_Build(&f, "()"), NULL);
		return Sf_None(&f);
	}
	Sf_SetMod(&f, "Alarm", "RestoreMissionCallback", BPG("OnAlarmDeactivated"));
	Sf_SetMod(&f, "Alarm", "AbortMissionCallback", BPG("OnAlarmActivated"));
	Sf_Own(&f, BP, "CreateLauncher", "(O)", Sf_Attr(&f, pickus, "Pos"));
	Sf_Later(&f, Sf_Float(&f, 1.0), BPG("CheckMaxDist"), Sf_Build(&f, "(i)", 1500), "CheckMaxDist");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, pickus, "Name"));
	Sf_Own(&f, BP, "SetThugsState", "(O)", BPG("followPlayerVel"));
	return Sf_None(&f);
}

/* OnAlarmActivated(): the alarm stops the beating: no distance check, Pickus no target, the launcher off */
static PyObject *BeatPickus_OnAlarmActivated(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CheckMaxDist");
	PyObject *pickus = Sf_Entity(&f, Sf_Str(&f, "Functionary_Pickus"));
	if (Sf_True(&f, pickus) > 0) {
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", pickus, 0);
	}
	if (Sf_True(&f, BPG("launcher")) > 0) {
		Sf_SetAttrS(&f, BPG("launcher"), "OnTake", "");
	}
	return Sf_None(&f);
}

/* CheckMaxDist(d): every second: the thugs stop when the player is farther than d, and follow him again */
static PyObject *BeatPickus_CheckMaxDist(PyObject *self, PyObject *args) {
	PyObject *d;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &d)) {
		return NULL;
	}
	Sf_Later(&f, Sf_Float(&f, 1.0), BPG("CheckMaxDist"), Sf_Build(&f, "(O)", d), "CheckMaxDist");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	int near = Sf_True(&f, Sf_Own(&f, BP, "CheckThugsDist", "(OO)", Sf_Attr(&f, usr, "Pos"), d));
	if (near == 0) {
		if (Sf_True(&f, BPG("thugsStopped")) == 0) {
			Sf_SetGlobal(&f, BP, "thugsStopped", Sf_Int(&f, 1));
			Sf_Own(&f, BP, "SetThugsState", "(d)", 0.0);
			Sf_Later(&f, Sf_Float(&f, 0.1), Sf_Mod(&f, "Speech", "AddRemoteCom"), Sf_Build(&f, "(ss)", "Mission_HireThugs_RemoteMessage1", "Police"),
				"NotSoFast");
		}
	} else if (near > 0) {
		int c = Sf_True(&f, BPG("thugsStopped"));
		if (c > 0) {
			c = Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Dtritus");
		}
		if (c > 0) {
			Sf_SetGlobal(&f, BP, "thugsStopped", Sf_Int(&f, 0));
			Sf_Own(&f, BP, "SetThugsState", "(O)", BPG("followPlayerVel"));
		}
	}
	return Sf_None(&f);
}

/* CheckThugsDist(pos, d): 1 when a thug is within d of pos */
static PyObject *BeatPickus_CheckThugsDist(PyObject *self, PyObject *args) {
	PyObject *pos, *d;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &pos, &d)) {
		return NULL;
	}
	for (int i = 1; i < 6 && !f.failed; i++) {
		int mark = f.n;
		PyObject *e = Bp_Thug(&f, i);
		if (Sf_True(&f, e) > 0) {
			PyObject *dist = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", pos, Sf_Attr(&f, e, "Pos")));
			if (Sf_Compare(&f, dist, "<=", Sf_Op(&f, d, '*', d)) > 0) {
				return Sf_Return(&f, Sf_Int(&f, 1));
			}
		}
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, Sf_Int(&f, 0));
}

/* CreateLauncher(pos): the item at Pickus that starts the scene */
static PyObject *BeatPickus_CreateLauncher(PyObject *self, PyObject *args) {
	PyObject *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &pos)) {
		return NULL;
	}
	if (Sf_True(&f, BPG("launcher")) == 0) {
		PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "PickusFightLauncher", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
			Sf_Item(&f, pos, 2), "Item");
		Sf_SetGlobal(&f, BP, "launcher", l);
		Sf_SetAttrI(&f, BPG("launcher"), "Radius", 600);
		Sf_SetAttrI(&f, BPG("launcher"), "OnlyUser", 1);
	}
	Sf_SetAttrS(&f, BPG("launcher"), "OnTake", "Mission_BeatPickus.LaunchPickusFightScene");
	return Sf_None(&f);
}

/* LaunchPickusFightScene(ItemName, CharName): Dtritus reached Pickus without the alarm: with the thugs near the scene
 * Beating plays (AskPickus at its end), else the launcher comes back in 0.5 s */
static PyObject *BeatPickus_LaunchPickusFightScene(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &ch)) {
		return NULL;
	}
	if (Sf_True(&f, Sf_Entity(&f, Sf_Str(&f, "Functionary_Pickus"))) <= 0) {
		return Sf_None(&f);
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int bad = Sf_True(&f, usr);
	bad = bad < 0 ? -1 : !bad;
	if (bad == 0) {
		bad = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", ch);
	}
	if (bad == 0) {
		bad = Sf_Compare(&f, Sf_Attr(&f, usr, "ActCtrl"), "!=", Sf_Str(&f, "Dtritus"));
	}
	if (bad != 0 || Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0)) != 0) {
		return Sf_None(&f);
	}
	Sf_SetAttrS(&f, BPG("launcher"), "OnTake", "");
	int near = Sf_True(&f, Sf_Own(&f, BP, "CheckThugsDist", "(Oi)", Sf_Attr(&f, usr, "Pos"), 600));
	if (near > 0) {
		PyObject *speech = Sf_Keep(&f, PyImport_ImportModule("Speech"));
		Sf_Check(&f, speech && PyObject_SetAttrString(speech, "RemoteCom", NULL) == 0);
		Sf_SetMod(&f, "Speech", "RemoteCom", Sf_Build(&f, "[]"));
		Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "NotSoFast");
		Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CheckMaxDist");
		Sf_Own(&f, BP, "LightingActivate", "(i)", 0);
		Sf_Ask(&f, "Scene", "Play", "(s)", "Beating");
		Sf_SetMod(&f, "Scene", "EndOfSceneCallback", BPG("AskPickus"));
	} else if (near == 0) {
		Sf_Later(&f, Sf_Float(&f, 0.5), BPG("CreateLauncher"), Sf_Build(&f, "(O)", Sf_Attr(&f, Sf_Entity(&f, item), "Pos")), NULL);
	}
	return Sf_None(&f);
}

/* CreatePickusBeatenClass(): the class of Pickus beaten (models in the level folder) */
static PyObject *BeatPickus_CreatePickusBeatenClass(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Ask(&f, "SAct", "CreateClass", "(s)", "PickusBeaten");
	Sf_Ask(&f, "SAct", "SetCls", "(sO)", "ModelFilePath", Sf_Cat(&f, "Os", Sf_Mod(&f, "Init", "Path"), "/Map/Beating/PickusBeaten.m3d"));
	Sf_Ask(&f, "SAct", "CreateAction", "(s)", "PickusLoop");
	Sf_Ask(&f, "SAct", "SetAct", "(sO)", "AnmFilePath", Sf_Cat(&f, "Os", Sf_Mod(&f, "Init", "Path"), "/Map/Beating/BeatingAnm/PickusBeaten_Relax.M3D"));
	Sf_Ask(&f, "SAct", "SetAct", "(si)", "isLoop", 1);
	Sf_Ask(&f, "SAct", "CreateAction", "(s)", "Talk");
	Sf_Ask(&f, "SAct", "SetAct", "(sO)", "AnmFilePath", Sf_Cat(&f, "Os", Sf_Mod(&f, "Init", "Path"), "/Map/Beating/BeatingAnm/PickusBeaten_Talk.M3D"));
	Sf_Ask(&f, "SAct", "SetAct", "(si)", "isLoop", 1);
	Sf_Ask(&f, "SAct", "SetCls", "(ss)", "DefAction", "PickusLoop");
	return Sf_None(&f);
}

/* InitPickusBeaten(): Pickus beaten at his dummy, he talks */
static PyObject *BeatPickus_InitPickusBeaten(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *dummy = Lv_Dummy(&f, "DM_Mission_PickusFight", 1);
	if (Sf_True(&f, dummy) == 0) {
		Sf_RaiseText(&f, "No existe el dummy para situar al Pickus hecho polvo");
	}
	PyObject *e = Py_None;
	if (Sf_Compare(&f, dummy, "!=", Py_None) > 0) {
		e = Po_FromDummy(&f, dummy, "WithLife");
		Sf_SetAttrS(&f, e, "ActCtrl", "PickusBeaten");
		Sf_SetAttrI(&f, e, "ActiveCMask", 0);
		Sf_SetAttrI(&f, e, "MapCollision", 1);
		Sf_SetAttrI(&f, e, "UseAngSel", 1);
		Sf_SetAttrI(&f, e, "UseAbsAngSel", 0);
		Sf_SetAttrI(&f, e, "VRadius", 150);
		Sf_SetAttrI(&f, e, "ArrowCanUse", 1);
		Sf_SetAttrI(&f, e, "UseMapCheck", 0);
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_BeatPickus.OnTalkToPickus");
		Sf_SetAttrI(&f, e, "ArrnOpt", 0);
		Sf_SetAttr(&f, e, "ArrOpt0", Sf_Cat(&f, "sO", "Talk:", Sf_Lang(&f, Sf_Str(&f, "TalkOpt"))));
		Sf_SetAttr(&f, e, "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Pickus")));
		Sf_SetAttrI(&f, e, "OffModPosAsPos", 1);
		Sf_SetAttr(&f, e, "OffModPos", Sf_Attr(&f, e, "Pos"));
		Sf_SetAttr(&f, e, "Pos", Sf_Ask(&f, "SVec", "Add", "(O(iii))", Sf_Attr(&f, e, "Pos"), 0, 100, -550));
	}
	return Sf_Return(&f, e);
}

/* DelThug(thugName): the thug deleted (as soon as he is there) */
static PyObject *BeatPickus_DelThug(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *thug = Sf_Entity(&f, name);
	int there = Sf_True(&f, thug);
	if (there > 0) {
		Sf_Method(&f, thug, "Del", "()");
	} else if (there == 0) {
		Sf_Later(&f, Sf_Float(&f, 1.0), BPG("DelThug"), Sf_Build(&f, "(O)", name), NULL);
	}
	return Sf_None(&f);
}

/* AskPickus(usr=None): after the scene: the lights, no thugs, Pickus beaten in place of Pickus */
static PyObject *BeatPickus_AskPickus(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Own(&f, BP, "LightingActivate", "(i)", 1);
	for (int i = 1; i < 6 && !f.failed; i++) {
		int mark = f.n;
		Sf_Own(&f, BP, "DelThug", "(O)", Sf_Cat(&f, "sO", "Police_Thug", Sf_ReprI(&f, i)));
		Sf_Release(&f, mark);
	}
	Sf_Method(&f, Sf_Entity(&f, Sf_Str(&f, "Functionary_Pickus")), "Del", "()");
	PyObject *pickus = Sf_Own(&f, BP, "InitPickusBeaten", "()");
	if (Sf_True(&f, pickus) > 0) {
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, pickus, "Name"));
	}
	return Sf_None(&f);
}

/* OnTalkToPickus(UsrName, CharName, optId, FullOption): Pickus tells about the plans (PreGetFuncShipPlans at the
 * end the first time) */
static PyObject *BeatPickus_OnTalkToPickus(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState"), "!=", Sf_Str(&f, "GetFuncShipPlans")) > 0) {
			Lv_OnEnd(&f, BP, "PreGetFuncShipPlans", NULL);
		}
		PyObject *fn = Sf_Mod(&f, "Speech", "OnObjSpch");
		PyObject *kw = Sf_Build(&f, "{ssss}", "action", "Talk", "defLoop", "PickusLoop");
		PyObject *a = f.failed ? NULL : Py_BuildValue("(Osss)", Sf_Entity(&f, ch), "Pickus", "Mission_AskPickus_InitMsg1", "Mission_AskPickus_InitAns1");
		Sf_CallKw(&f, fn, Sf_Check(&f, a != NULL) ? a : NULL, kw);
	}
	return Sf_None(&f);
}

/* PreGetFuncShipPlans(): the alarm hooks off, saved, the plans of the functionaries' ship */
static PyObject *BeatPickus_PreGetFuncShipPlans(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetMod(&f, "Alarm", "RestoreMissionCallback", Py_None);
	Sf_SetMod(&f, "Alarm", "AbortMissionCallback", Py_None);
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "GetFuncShipPlans");
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetFuncShipPlans_MissionText1", 1);
	Sf_Own(&f, BP, "GetFuncShipPlans", "()");
	return Sf_None(&f);
}

/* the photo object name at the dummy pattern into plansNames; raise text when the dummy is missing */
static void Bp_Plan(scriptFrame_t *f, const char *pattern, const char *name, const char *missing) {
	PyObject *dummy = Lv_Dummy(f, pattern, 1);
	int there = Sf_True(f, dummy);
	if (there > 0) {
		PyObject *p = Sf_Item(f, dummy, 1), *a = Sf_Item(f, dummy, 2);
		PyObject *pos = Sf_Build(f, "(OOO)", Sf_Item(f, p, 0), Sf_Item(f, p, 1), Sf_Item(f, p, 2));
		PyObject *ang = Sf_Build(f, "(OOi)", Sf_Item(f, a, 0), Sf_Item(f, a, 1), 0);
		PyObject *pln = Sf_Ask(f, "MissionsFuncs", "CreatePhotoObject", "(sOO)", name, pos, ang);
		Sf_Method(f, BPGF("plansNames"), "append", "(O)", Sf_Attr(f, pln, "Name"));
	} else if (there == 0) {
		Sf_RaiseText(f, missing);
	}
}

/* GetFuncShipPlans(): the two blueprints to photograph */
static PyObject *BeatPickus_GetFuncShipPlans(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "Set", "(si)", "LittlePoliceAsksForMoney", 1);
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "NotCleanableByGear", "");
	Sf_Import(&f, "CharAct");
	Sf_SetMod(&f, "CharAct", "TakenPhoto", BPG("TakePlanPhoto"));
	Bp_Plan(&f, "DM_Mission_FunctionaryShipBlueprint", "PlanoSFunc1", "No existe el dummy de los planos de la nave de los funcionarios");
	Bp_Plan(&f, "DM_Mission_FunctionaryEngineBlueprint", "PlanoMFunc1",
		"No existe el dummy de los planos de los motores de la nave de los funcionarios");
	Sf_Ask(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", BPG("plansNames"));
	return Sf_None(&f);
}

/* the blueprint item into the hangar list of the save variable var (once), with its message */
static void Bp_Blueprint(scriptFrame_t *f, const char *var, const char *item, const char *msg) {
	PyObject *lst = Lv_SaveList(f, var, BP);
	if (Sf_In(f, Sf_Str(f, item), lst) == 0) {
		Lv_SaveAppend(f, var, lst, Sf_Str(f, item));
		Sf_Import(f, "RaceMaker");
		Sf_Ask(f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Lang(f, Sf_Str(f, msg)));
		Sf_Ask(f, "Scrap", "IncSaveVar", "(s)", "NewBluePrintAvaliable");
	}
}

/* TakePlanPhoto(planName): the photo of a blueprint of the mission (1, else 0); after both Traitorius calls */
static PyObject *BeatPickus_TakePlanPhoto(PyObject *self, PyObject *args) {
	PyObject *plan;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &plan)) {
		return NULL;
	}
	Sf_Import(&f, "CharAct");
	if (Sf_In(&f, plan, BPG("plansNames")) == 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	Sf_Ask(&f, "MissionsFuncs", "UnsetPhotoObject", "(O)", Sf_Entity(&f, plan));
	int engine = Sf_EqS(&f, plan, "PlanoMFunc1");
	if (engine > 0) {
		Bp_Blueprint(&f, "Hangar.availableEnginesList", "MFUNC1", "Mission_GetFuncShipPlans_SystemMessage2");
	} else if (engine == 0) {
		Bp_Blueprint(&f, "Hangar.shipsToEditList", "SFunc1", "Mission_GetFuncShipPlans_SystemMessage1");
	}
	Sf_SetGlobal(&f, BP, "numPlansWithPhoto", Sf_OpI(&f, BPG("numPlansWithPhoto"), '+', 1));
	if (Sf_EqI(&f, BPG("numPlansWithPhoto"), 2) > 0) {
		Sf_SetMod(&f, "CharAct", "TakenPhoto", Py_None);
		Sf_Later(&f, Sf_Float(&f, 1.0), Sf_Mod(&f, "Speech", "AddRemoteCom"),
			Sf_Build(&f, "(ssO)", "Mission_GetFuncShipPlans_RemoteMessage1", "Traitorius", BPG("PreTalkToTraitoriusAgain")), NULL);
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* TalkToTraitoriusAgain(): Traitorius is the target again */
static PyObject *BeatPickus_TalkToTraitoriusAgain(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetGlobal(&f, BP, "traitorius", Sf_Entity(&f, Sf_Str(&f, "Functionary_Traitorius")));
	if (Sf_True(&f, BPG("traitorius")) > 0) {
		Sf_SetAttrS(&f, BPG("traitorius"), "UseFunc", "Mission_BeatPickus.TraitoriusAgainSpeech");
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, BPG("traitorius"), "Name"));
	}
	return Sf_None(&f);
}

/* TraitoriusAgainSpeech(UsrName, CharName, optId, FullOption): the plans brought: Traitorius sends Dtritus on
 * (SetNextMission at the end, once) */
static PyObject *BeatPickus_TraitoriusAgainSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Bp_UsrIs(&f, usr, "Functionary") > 0) {
		int talked = Sf_True(&f, BPG("alreadyTalked"));
		if (talked == 0) {
			static const char *const ni[] = { "Mission_TalkToTraitorius2_Traitorius_InitMsg1", "Mission_TalkToTraitorius2_Traitorius_InitAns1",
				"Mission_TalkToTraitorius2_Traitorius_1Msg", "Mission_TalkToTraitorius2_Traitorius_1Ans",
				"Mission_TalkToTraitorius2_Traitorius_2Msg", "Mission_TalkToTraitorius2_Traitorius_2Ans", NULL };
			Sf_SetGlobal(&f, BP, "traitorius", Sf_Entity(&f, Sf_Str(&f, "Functionary_Traitorius")));
			Sf_SetAttrS(&f, BPG("traitorius"), "UseFunc", "Mission_BeatPickus.TraitoriusSpeech");
			Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", BPG("traitorius"), 0);
			Lv_OnEnd(&f, BP, "SetNextMission", NULL);
			Sf_Ask(&f, "Speech", "NISpeech", "(s)", "Functionary");
			Bp_AddNI(&f, ni);
			Sf_Ask(&f, "Speech", "RunNI", "()");
		} else if (talked > 0) {
			Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_Search&DestroyMickus_Traitorius_InitMsg1", "Mission_Search&DestroyMickus_Traitorius_InitAns1");
		}
	} else if (Bp_UsrIs(&f, usr, "Dtritus") > 0) {
		if (Sf_EqS(&f, opt, "Talk") > 0) {
			Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_FlowerPower_Traitorius_InitMsg2", "Mission_FlowerPower_Traitorius_InitAns2");
		}
	} else {
		Sf_Ask(&f, "CharsNPC", "GenericNPCSpeech", "(OOOO)", usr, ch, opt, full);
	}
	return Sf_None(&f);
}

/* LightingActivate(activate=0): the electric effect of the police (PoliceFXLighting01) on or off */
static PyObject *BeatPickus_LightingActivate(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "activate", NULL };
	PyObject *activate = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &activate)) {
		return NULL;
	}
	activate = activate ? Sf_Ref(&f, activate) : Sf_Int(&f, 0);
	Sf_Ask(&f, "Scrap", "SetCallFunc", "(s)", "!MapElementsActivate");
	Sf_Ask(&f, "Scrap", "AddParams", "(s)", "PoliceFXLighting01");
	Sf_Ask(&f, "Scrap", "AddParamf", "(i)", 5120);
	Sf_Ask(&f, "Scrap", "AddParamf", "(i)", -1940);
	Sf_Ask(&f, "Scrap", "AddParamf", "(i)", 29000);
	Sf_Ask(&f, "Scrap", "AddParamf", "(i)", 1000);
	Sf_Ask(&f, "Scrap", "AddParami", "(O)", activate);
	Sf_Ask(&f, "Scrap", "Execute", "()");
	return Sf_None(&f);
}

PyMethodDef g_PoliceBeatPickusMethods[] = {
	{ "InitStatus", BeatPickus_InitStatus, METH_VARARGS, NULL },
	{ "Init", BeatPickus_Init, METH_VARARGS, NULL },
	{ "DisableIAFunctionary", BeatPickus_DisableIAFunctionary, METH_VARARGS, NULL },
	{ "OnTraitoriusRespawn", BeatPickus_OnTraitoriusRespawn, METH_VARARGS, NULL },
	{ "OnThugRespawn", BeatPickus_OnThugRespawn, METH_VARARGS, NULL },
	{ "SetThugsState", (PyCFunction)(void (*)(void))BeatPickus_SetThugsState, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "SetState", BeatPickus_SetState, METH_VARARGS, NULL },
	{ "TraitoriusSpeech", BeatPickus_TraitoriusSpeech, METH_VARARGS, NULL },
	{ "HireThugs", BeatPickus_HireThugs, METH_VARARGS, NULL },
	{ "ThugLeaderSpeech", BeatPickus_ThugLeaderSpeech, METH_VARARGS, NULL },
	{ "ThugNormalSpeech", BeatPickus_ThugNormalSpeech, METH_VARARGS, NULL },
	{ "BeatPickus", BeatPickus_BeatPickus, METH_VARARGS, NULL },
	{ "OnAlarmActivated", BeatPickus_OnAlarmActivated, METH_VARARGS, NULL },
	{ "CheckMaxDist", BeatPickus_CheckMaxDist, METH_VARARGS, NULL },
	{ "CheckThugsDist", BeatPickus_CheckThugsDist, METH_VARARGS, NULL },
	{ "CreateLauncher", BeatPickus_CreateLauncher, METH_VARARGS, NULL },
	{ "LaunchPickusFightScene", BeatPickus_LaunchPickusFightScene, METH_VARARGS, NULL },
	{ "CreatePickusBeatenClass", BeatPickus_CreatePickusBeatenClass, METH_VARARGS, NULL },
	{ "InitPickusBeaten", BeatPickus_InitPickusBeaten, METH_VARARGS, NULL },
	{ "DelThug", BeatPickus_DelThug, METH_VARARGS, NULL },
	{ "AskPickus", (PyCFunction)(void (*)(void))BeatPickus_AskPickus, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "OnTalkToPickus", BeatPickus_OnTalkToPickus, METH_VARARGS, NULL },
	{ "PreGetFuncShipPlans", BeatPickus_PreGetFuncShipPlans, METH_VARARGS, NULL },
	{ "GetFuncShipPlans", BeatPickus_GetFuncShipPlans, METH_VARARGS, NULL },
	{ "TakePlanPhoto", BeatPickus_TakePlanPhoto, METH_VARARGS, NULL },
	{ "TalkToTraitoriusAgain", BeatPickus_TalkToTraitoriusAgain, METH_VARARGS, NULL },
	{ "TraitoriusAgainSpeech", BeatPickus_TraitoriusAgainSpeech, METH_VARARGS, NULL },
	{ "LightingActivate", (PyCFunction)(void (*)(void))BeatPickus_LightingActivate, METH_VARARGS | METH_KEYWORDS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FicusDead ---- */

#define FDG(name) Sf_Global(&f, FD, name)

/* InitFicusDead(): Ficus' body of the map talks */
static PyObject *FicusDead_InitFicusDead(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Map");
	Sf_SetGlobal(&f, FD, "ficus", Sf_Mod(&f, "Map", "ficus"));
	Sf_SetAttr(&f, FDG("ficus"), "CamTgt", Sf_Build(&f, "(iii)", 0, 280, 0));
	Sf_SetAttrI(&f, FDG("ficus"), "ArrowCanUse", 1);
	Sf_SetAttrS(&f, FDG("ficus"), "UseFunc", "Mission_FicusDead.OnTalkToFicus");
	Sf_SetAttrI(&f, FDG("ficus"), "ArrnOpt", 0);
	Sf_SetAttr(&f, FDG("ficus"), "ArrOpt0", Sf_Cat(&f, "sO", "Talk:", Sf_Lang(&f, Sf_Str(&f, "TalkOpt"))));
	Sf_SetAttr(&f, FDG("ficus"), "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Ficus")));
	return Sf_None(&f);
}

/* Init(LevelPath): Ficus is the target, or Deep Throat's desktop after his message */
static PyObject *FicusDead_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	int dt = Lv_SaveIs(&f, "MissionState", "TalkToDeepThroat");
	if (dt > 0) {
		Sf_Own(&f, FD, "AfterDeepThroatMessageShown", "()");
	} else if (dt == 0) {
		Sf_Own(&f, FD, "InitFicusDead", "()");
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, FDG("ficus"), "Name"));
	}
	return Sf_None(&f);
}

/* OnTalkToFicus(UsrName, CharName, optId, FullOption): Ficus is dead; Dtritus says so */
static PyObject *FicusDead_OnTalkToFicus(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Sf_SetAttrS(&f, FDG("ficus"), "Action", "FicusDead");
		Sf_SetAttrS(&f, FDG("ficus"), "DefLoop", "FicusDead");
		Sf_SetAttrI(&f, FDG("ficus"), "ArrowCanUse", 0);
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_TalkToFicus3_Dtritus_InitMsg1", "", FDG("AfterDtritusSpeechShown"), Sf_Build(&f, "()"));
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	}
	return Sf_None(&f);
}

/* AfterDeepThroatMessageShown(): Dtritus' desktop is the target, Deep Throat talks through it */
static PyObject *FicusDead_AfterDeepThroatMessageShown(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "DTritusDesktop");
	PyObject *dd = Sf_Entity(&f, Sf_Str(&f, "DM_DTritusDesktop"));
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, dd, "Name"));
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", FDG("DeepThroatSpeech"));
	return Sf_None(&f);
}

/* DeepThroatSpeech(UsrName, CharName, optId, FullOption): Deep Throat's talk, SetNextMission at the end */
static PyObject *FicusDead_DeepThroatSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Entity(&f, Sf_Str(&f, "DM_DTritusDesktop"));
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Lv_Speakers(&f, usr, ch);
	Lv_OnEnd(&f, FD, "SetNextMission", NULL);
	Sf_Ask(&f, "Speech", "NISpeech", "(s)", "DeepThroat");
	Sf_Ask(&f, "Speech", "AutoAddNI", "(s)", "Mission_CallDeepThroat_ComDeepthroat_NI_1Msg");
	Sf_Ask(&f, "Speech", "RunNI", "()");
	return Sf_None(&f);
}

/* SetNextMission(): Mission_FunctionaryHunting in Outskirts */
static PyObject *FicusDead_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "DTritusDesktop");
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", Py_None);
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Sf_Ask(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_FunctionaryHunting", "Outskirts");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_FunctionaryHunting_MissionText1", 1);
	return Sf_None(&f);
}

PyMethodDef g_PoliceFicusDeadMethods[] = {
	{ "InitFicusDead", FicusDead_InitFicusDead, METH_VARARGS, NULL },
	{ "Init", FicusDead_Init, METH_VARARGS, NULL },
	{ "OnTalkToFicus", FicusDead_OnTalkToFicus, METH_VARARGS, NULL },
	{ "AfterDeepThroatMessageShown", FicusDead_AfterDeepThroatMessageShown, METH_VARARGS, NULL },
	{ "DeepThroatSpeech", FicusDead_DeepThroatSpeech, METH_VARARGS, NULL },
	{ "SetNextMission", FicusDead_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetAuthorization ---- */

#define GAG(name) Sf_Global(&f, GA, name)
#define GAGF(name) Sf_Global(f, GA, name)

/* InitStatus(LevelPath): the human in the jail */
static PyObject *GetAuthorization_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Lv_MissionChar(&f, "Human_Default", GA, "CreateHuman");
	return Sf_None(&f);
}

/* CreateHuman(e): the human (the module variable human), no talk */
static PyObject *GetAuthorization_CreateHuman(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetGlobal(&f, GA, "human", e);
	Sf_SetAttr(&f, GAG("human"), "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Human")));
	Sf_SetAttrS(&f, GAG("human"), "UseFunc", "");
	Sf_SetAttrI(&f, GAG("human"), "ArrowCanUse", 0);
	return Sf_None(&f);
}

/* Init(LevelPath): the scene MeetHuman, the state of the mission */
static PyObject *GetAuthorization_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Own(&f, GA, "LoadScene", "()");
	PyObject *state = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "MissionState");
	if (Sf_EqS(&f, state, "DeepThroatSpeech") > 0) {
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_GoToPolice_RemoteMessage1", "DeepThroat", GAG("AfterDeepThroatMessage"), Sf_Build(&f, "()"));
	} else if (Sf_EqS(&f, state, "GetAuthorization") > 0) {
		Sf_Own(&f, GA, "GetAuthorization", "()");
	} else if (Sf_EqS(&f, state, "GetAuthorization2") > 0) {
		Sf_Own(&f, GA, "GetAuthorization2", "()");
	} else if (Sf_EqS(&f, state, "FreeHuman") > 0 || Sf_EqS(&f, state, "FreeHuman2") > 0) {
		Sf_Own(&f, GA, "FreeHuman", "()");
	}
	return Sf_None(&f);
}

/* AfterDeepThroatMessage(): Dtritus' desktop is the target, Deep Throat talks through it */
static PyObject *GetAuthorization_AfterDeepThroatMessage(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetGlobal(&f, GA, "dd", Sf_Entity(&f, Sf_Str(&f, "DM_DTritusDesktop")));
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, GAG("dd"), "Name"));
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToDeepThroat7_MissionText1", 1);
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", GAG("DeepThroatSpeech"));
	return Sf_None(&f);
}

/* DeepThroatSpeech(UsrName, CharName, optId, FullOption): Deep Throat's talk, PreGetAuthorization at the end */
static PyObject *GetAuthorization_DeepThroatSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	Lv_OnEnd(&f, GA, "PreGetAuthorization", NULL);
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Ask(&f, "Speech", "NISpeech", "(s)", "DeepThroat");
	Sf_Ask(&f, "Speech", "AutoAddNI", "(s)", "Mission_TalkToDeepThroat7__ComDeepThroat_NI_1Msg");
	Sf_Ask(&f, "Speech", "RunNI", "()");
	return Sf_None(&f);
}

/* PreGetAuthorization(): saved; a messenger already goes for the password, else first to a conversion point */
static PyObject *GetAuthorization_PreGetAuthorization(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", Py_None);
	Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", GAG("dd"), 0);
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetPassword_MissionText1", 1);
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "GetAuthorization");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int m = Sf_True(&f, usr);
	if (m > 0) {
		m = Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Messenger");
	}
	if (m > 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "GetAuthorization2");
		Sf_Own(&f, GA, "GetAuthorization2", "()");
	} else if (m == 0) {
		Sf_Own(&f, GA, "GetAuthorization", "()");
	}
	return Sf_None(&f);
}

/* GetAuthorization(): the nearest conversion point is the target until the player is a messenger */
static PyObject *GetAuthorization_GetAuthorization(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetMod(&f, "CharConversor", "afterExitConversor", GAG("ShowAfterConversionMessage"));
	Sf_Own(&f, GA, "ShowNearestConversor", "()");
	return Sf_None(&f);
}

/* ShowNearestConversor(): every 3 s the conversion point nearest to the player is the target */
static PyObject *GetAuthorization_ShowNearestConversor(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) > 0) {
		Sf_SetGlobal(&f, GA, "conv", Sf_Ask(&f, "CharConversor", "GetNearestConversionPoint", "(O)", Sf_Attr(&f, usr, "Pos")));
		if (Sf_True(&f, GAG("conv")) > 0) {
			Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, GAG("conv"), "Name"));
			Sf_Later(&f, Sf_Int(&f, 3), GAG("ShowNearestConversor"), Sf_Build(&f, "()"), "ShowConversor");
		}
	}
	return Sf_None(&f);
}

/* ShowAfterConversionMessage(): out of the conversor as a messenger: Deep Throat's message, the password next */
static PyObject *GetAuthorization_ShowAfterConversionMessage(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) > 0 && Sf_EqS(&f, Sf_Attr(&f, Sf_Entity(&f, Sf_Attr(&f, usr, "Name")), "ActCtrl"), "Messenger") > 0) {
		Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ShowConversor");
		Sf_SetMod(&f, "CharConversor", "afterExitConversor", Py_None);
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", GAG("conv"), 0);
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_GetPassword_RemoteMessage1", "DeepThroat");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "GetAuthorization2");
		Sf_SetGlobal(&f, GA, "conv", Py_None);
		Sf_Own(&f, GA, "GetAuthorization2", "()");
	}
	return Sf_None(&f);
}

/* OnDataTaken(messengerName): the player took a tube: empty, filled by him, or filled by another */
static PyObject *GetAuthorization_OnDataTaken(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int mine = Sf_True(&f, usr);
	if (mine > 0) {
		mine = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", name);
	}
	if (mine > 0) {
		int full = Sf_True(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "MessengerTubeState"));
		int byUser = full > 0 ? Sf_True(&f, GAG("filledByUser")) : 0;
		const char *msg = full == 0 ? "Mission_GetPassword_RemoteMessage2" : byUser > 0 ? "Mission_GetPassword_RemoteMessage3" :
			"Mission_GetPassword_RemoteMessage6";
		if (full >= 0 && byUser >= 0) {
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", msg, "DeepThroat");
		}
	}
	return Sf_None(&f);
}

/* OnDataDropped(messengerName, dataStatus): a full tube of the player at a desk wins (if he left it there himself);
 * an empty tube at the recharger tells who fills it */
static PyObject *GetAuthorization_OnDataDropped(PyObject *self, PyObject *args) {
	PyObject *name, *status;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &status)) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	int full = Sf_EqI(&f, status, 1);
	if (full > 0) {
		full = Sf_True(&f, GAG("filledByUser"));
	}
	if (full > 0) {
		int self_ = Sf_Compare(&f, name, "==", Sf_Attr(&f, usr, "Name"));
		if (self_ > 0) {
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_GetPassword_RemoteMessage5", "DeepThroat", GAG("PreFreeHuman"), Sf_Build(&f, "()"));
		} else if (self_ == 0) {
			Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_GetPassword_RemoteMessage4", "DeepThroat");
		}
	} else if (full == 0 && Sf_EqI(&f, status, 0) > 0) {
		int by = Sf_Compare(&f, name, "==", Sf_Attr(&f, usr, "Name"));
		if (by >= 0) {
			Sf_SetGlobal(&f, GA, "filledByUser", Sf_Int(&f, by));
		}
	}
	return Sf_None(&f);
}

/* FreeHuman(): the gatekeeper of the security door lets Dtritus pass (StartScene), the trigger of the human */
static PyObject *GetAuthorization_FreeHuman(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "GateKeeper", "DoorFoe", "(sO)", "GateKeeper_08", Sf_Mod(&f, "GateKeeper", "DtritusGroupGateKeeper"));
	Sf_Ask(&f, "GateKeeper", "SetDoor", "(ssO)", "GateKeeper_08", "DM_Door_PoliceSegurity_00", GAG("StartScene"));
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Str(&f, "GateKeeper_08")), "UseFunc", "Mission_GetAuthorization.TalkPepe");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "GateKeeper_08");
	PyObject *dummy = Lv_Dummy(&f, "DM_Trigger_Human", 1);
	if (Sf_True(&f, dummy) == 0) {
		Sf_RaiseText(&f, "No se encuentra el dummy: 'DM_Trigger_Human'");
	}
	PyObject *pos = Sf_Item(&f, dummy, 1);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "TriggerHuman", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "Item");
	Sf_SetAttrI(&f, l, "Radius", 500);
	Sf_SetAttrS(&f, l, "OnTake", "Mission_GetAuthorization.OnTriggerActivated");
	return Sf_None(&f);
}

/* TalkPepe(UsrName, CharName, optId, FullOption): the gatekeeper talks; Dtritus cannot use the conversion point */
static PyObject *GetAuthorization_TalkPepe(PyObject *self, PyObject *args) {
	PyObject *usrName, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usrName, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int dt = Sf_True(&f, usr);
	if (dt > 0) {
		dt = Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Dtritus");
	}
	if (dt > 0) {
		Sf_SetAttrI(&f, Sf_Entity(&f, Sf_Str(&f, "DM_Conversion_Point_08")), "ArrowCanUse", 0);
	}
	Sf_Ask(&f, "Speech", "AutoSpeech", "(OOOO)", usrName, ch, opt, full);
	return Sf_None(&f);
}

/* StartScene(Door, CharName): through the security door: the scene MeetHuman the first time (GoToPress at its
 * end), else straight to the metro */
static PyObject *GetAuthorization_StartScene(PyObject *self, PyObject *args) {
	PyObject *door, *ch;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &door, &ch)) {
		return NULL;
	}
	Sf_SetAttrI(&f, Sf_Entity(&f, Sf_Str(&f, "DM_Conversion_Point_08")), "ArrowCanUse", 1);
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Str(&f, "GateKeeper_08")), "UseFunc", "Speech.AutoSpeech");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Ask(&f, "GateKeeper", "DoorFoe", "(sO)", "GateKeeper_08", Sf_Mod(&f, "GateKeeper", "NoOneGroupGateKeeper"));
	Sf_Ask(&f, "GateKeeper", "SetDoor", "(ss)", "GateKeeper_08", "DM_Door_PoliceSegurity_00");
	int first = Lv_SaveIs(&f, "MissionState", "FreeHuman");
	if (first > 0) {
		Sf_SetGlobal(&f, GA, "scenePlayed", Sf_Int(&f, 0));
		PyObject *e = Sf_Entity(&f, Sf_Str(&f, "DM_Door_PoliceSegurity_00"));
		Sf_SetAttrS(&f, e, "Action", "Open");
		Sf_SetAttrS(&f, e, "DefLoop", "Opened");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "FreeHuman2");
		Sf_Ask(&f, "Scene", "Play", "(s)", "MeetHuman");
		Sf_SetMod(&f, "Scene", "EndOfSceneCallback", GAG("GoToPress"));
		Sf_Ask(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "Scene.AbortScene");
	} else if (first == 0) {
		Sf_SetGlobal(&f, GA, "scenePlayed", Sf_Int(&f, 1));
		Lv_Fade(&f, lv_clear, lv_black, 0.5);
		PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
		Sf_SetAttrI(&f, usr, "Life", 100);
		Sf_SetAttrI(&f, usr, "Invulnerable", 1);
		Sf_Later(&f, Sf_Float(&f, 0.75), GAG("GoToPress"), Sf_Build(&f, "(O)", usr), NULL);
	}
	return Sf_None(&f);
}

/* OnTriggerActivated(ItemName, CharName): the human passed his trigger: the alarm, which does not fall */
static PyObject *GetAuthorization_OnTriggerActivated(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &ch)) {
		return NULL;
	}
	if (Sf_Compare(&f, ch, "==", Sf_Attr(&f, GAG("human"), "Name")) > 0) {
		Sf_Method(&f, Sf_Entity(&f, item), "Del", "()");
		Sf_SetGlobal(&f, GA, "alarmFallDelta", Sf_Ask(&f, "Scrap", "Get", "(s)", "AlarmFallDelta"));
		Sf_Ask(&f, "Scrap", "SetAlarm", "(i)", 1);
		Sf_Ask(&f, "Scrap", "Set", "(si)", "AlarmFallDelta", 0);
	}
	return Sf_None(&f);
}

/* GoToPress(usr): the door closes; the player and the human out of the jail, the human follows him to the metro */
static PyObject *GetAuthorization_GoToPress(PyObject *self, PyObject *args) {
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, Sf_Str(&f, "DM_Door_PoliceSegurity_00"));
	Sf_SetAttrS(&f, e, "Action", "Close");
	Sf_SetAttrS(&f, e, "DefLoop", "Closed");
	Lv_Fade(&f, lv_black, lv_clear, 0.5);
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	int played = Sf_EqI(&f, GAG("scenePlayed"), 0);
	if (played >= 0) {
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ssOO)", played ? "Mission_FirstMeetHuman_RemoteMessage1" : "Mission_FirstMeetHuman_RemoteMessage2",
			"DeepThroat", GAG("GoToPress2"), Sf_Build(&f, "()"));
	}
	Sf_SetAttrI(&f, usr, "Invulnerable", 0);
	Sf_SetAttr(&f, usr, "Pos", Sf_Build(&f, "(iii)", 4227, -1771, 28400));
	Sf_SetAttr(&f, usr, "Ang", Sf_Build(&f, "(iii)", 0, 160, 0));
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, usr, "Name"));
	Sf_Import(&f, "MissionsFuncs");
	Sf_SetAttr(&f, GAG("human"), "Pos", Sf_Build(&f, "(iii)", 4227, -1771, 28866));
	Sf_SetAttr(&f, GAG("human"), "Ang", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", GAG("human"), 2);
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, GAG("human"), "Name"));
	Sf_Ask(&f, "SAI", "SetStateChar", "(iOOd)", 3, Sf_Attr(&f, GAG("human"), "Name"), Sf_Attr(&f, usr, "Name"), 1.0);
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", GAG("OnMainCharDeath"));
	Sf_SetMod(&f, "CharConversor", "OnPossession", GAG("OnPossession"));
	Sf_SetMod(&f, "CharConversor", "afterExitConversor", GAG("OnExitConversor"));
	Sf_Own(&f, GA, "CheckMaxDist", "()");
	return Sf_None(&f);
}

/* GoToPress2(): the metro to Press is the target */
static PyObject *GetAuthorization_GoToPress2(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, Sf_Str(&f, "DM_Door_Metro_Press"));
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_GetAuthorization.OnMetroUsed");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, e, "Name"));
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_SetHumanFree_MissionText1", 1);
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* the human stops and asks the player to come back */
static void Ga_HumanStops(scriptFrame_t *f) {
	Sf_Ask(f, "SAI", "SetStateChar", "(iO)", 0, Sf_Attr(f, GAGF("human"), "Name"));
	Sf_Ask(f, "Speech", "AddRemoteCom", "(ss)", "Mission_SetHumanFree_RemoteMessage2", "Human");
}

/* OnPossession(newCharName, pos): a possession: the human stops */
static PyObject *GetAuthorization_OnPossession(PyObject *self, PyObject *args) {
	PyObject *name, *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &pos)) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	Ga_HumanStops(&f);
	Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", GAG("human"), 0);
	return Sf_None(&f);
}

/* OnExitConversor(): out of a conversor: the human follows only Dtritus */
static PyObject *GetAuthorization_OnExitConversor(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	int other = Sf_Compare(&f, Sf_Attr(&f, usr, "ActCtrl"), "!=", Sf_Str(&f, "Dtritus"));
	if (other > 0) {
		Ga_HumanStops(&f);
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", GAG("human"), 0);
	} else if (other == 0) {
		Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", GAG("human"), 2);
	}
	return Sf_None(&f);
}

/* CheckMaxDist(): every second: the human stops when the player is too far, and follows Dtritus again */
static PyObject *GetAuthorization_CheckMaxDist(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Later(&f, Sf_Float(&f, 1.0), GAG("CheckMaxDist"), Sf_Build(&f, "()"), "CheckMaxDist");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	int near = Sf_True(&f, Sf_Own(&f, GA, "CheckHumanDist", "()"));
	if (near == 0) {
		if (Sf_Compare(&f, Sf_Ask(&f, "SAI", "GetStateChar", "(O)", Sf_Attr(&f, GAG("human"), "Name")), "!=", Sf_Int(&f, 0)) > 0) {
			Ga_HumanStops(&f);
		}
	} else if (near > 0 && Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Dtritus") > 0) {
		Sf_Ask(&f, "SAI", "SetStateChar", "(iOOd)", 3, Sf_Attr(&f, GAG("human"), "Name"), Sf_Attr(&f, usr, "Name"), 1.0);
	}
	return Sf_None(&f);
}

/* the squared distance from the player to the human */
static PyObject *Ga_HumanDist(scriptFrame_t *f, PyObject *usr) {
	return Sf_Ask(f, "SVec", "ModSqr", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", Sf_Attr(f, usr, "Pos"), Sf_Attr(f, GAGF("human"), "Pos")));
}

/* CheckHumanDist(): 1 when the human is within maxHumanDist of the player */
static PyObject *GetAuthorization_CheckHumanDist(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) > 0) {
		PyObject *dist = Ga_HumanDist(&f, usr);
		if (Sf_Compare(&f, dist, "<=", Sf_Op(&f, GAG("maxHumanDist"), '*', GAG("maxHumanDist"))) > 0) {
			return Sf_Return(&f, Sf_Int(&f, 1));
		}
	}
	return Sf_Return(&f, Sf_Int(&f, 0));
}

/* OnMetroUsed(UsrName, MetroName, optId, FullOption): with the human near the metro goes to Press, the mission
 * Mission_TalkToBoss next */
static PyObject *GetAuthorization_OnMetroUsed(PyObject *self, PyObject *args) {
	PyObject *usrName, *metro, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usrName, &metro, &opt, &full)) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	PyObject *dist = Ga_HumanDist(&f, usr);
	int near = Sf_Compare(&f, dist, "<=", Sf_Op(&f, GAG("minHumanDist"), '*', GAG("minHumanDist")));
	if (near > 0) {
		Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CheckMaxDist");
		Sf_Ask(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_TalkToBoss", "Press");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "BankMasterAtBank", "1");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Journalist.Humphrey_Defaut", "Bank");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Journalist.Betty_Defaut", "Bank");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Journalist.Berto_Defaut", "Bank");
		Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
		Sf_Ask(&f, "Elevator", "UseElevator", "(OOsO)", usrName, metro, "MoveLevel Press", full);
	} else if (near == 0) {
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_SetHumanFree_RemoteMessage2", "Human");
	}
	return Sf_None(&f);
}

PyMethodDef g_PoliceGetAuthorizationMethods[] = {
	{ "InitStatus", GetAuthorization_InitStatus, METH_VARARGS, NULL },
	{ "CreateHuman", GetAuthorization_CreateHuman, METH_VARARGS, NULL },
	{ "Init", GetAuthorization_Init, METH_VARARGS, NULL },
	{ "AfterDeepThroatMessage", GetAuthorization_AfterDeepThroatMessage, METH_VARARGS, NULL },
	{ "DeepThroatSpeech", GetAuthorization_DeepThroatSpeech, METH_VARARGS, NULL },
	{ "PreGetAuthorization", GetAuthorization_PreGetAuthorization, METH_VARARGS, NULL },
	{ "GetAuthorization", GetAuthorization_GetAuthorization, METH_VARARGS, NULL },
	{ "ShowNearestConversor", GetAuthorization_ShowNearestConversor, METH_VARARGS, NULL },
	{ "ShowAfterConversionMessage", GetAuthorization_ShowAfterConversionMessage, METH_VARARGS, NULL },
	{ "OnDataTaken", GetAuthorization_OnDataTaken, METH_VARARGS, NULL },
	{ "OnDataDropped", GetAuthorization_OnDataDropped, METH_VARARGS, NULL },
	{ "FreeHuman", GetAuthorization_FreeHuman, METH_VARARGS, NULL },
	{ "TalkPepe", GetAuthorization_TalkPepe, METH_VARARGS, NULL },
	{ "StartScene", GetAuthorization_StartScene, METH_VARARGS, NULL },
	{ "OnTriggerActivated", GetAuthorization_OnTriggerActivated, METH_VARARGS, NULL },
	{ "GoToPress", GetAuthorization_GoToPress, METH_VARARGS, NULL },
	{ "GoToPress2", GetAuthorization_GoToPress2, METH_VARARGS, NULL },
	{ "OnPossession", GetAuthorization_OnPossession, METH_VARARGS, NULL },
	{ "OnExitConversor", GetAuthorization_OnExitConversor, METH_VARARGS, NULL },
	{ "CheckMaxDist", GetAuthorization_CheckMaxDist, METH_VARARGS, NULL },
	{ "CheckHumanDist", GetAuthorization_CheckHumanDist, METH_VARARGS, NULL },
	{ "OnMetroUsed", GetAuthorization_OnMetroUsed, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_InitEpilogue ---- */

#define IEG(name) Sf_Global(&f, IE, name)

/* InitStatus(LevelPath): Betty and Berto in the jail, Dtritus wakes up there too, the viscoson's costume */
static PyObject *InitEpilogue_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Lv_MissionChar(&f, "Betty_Jail", IE, "CreateBettyJail");
	Lv_MissionChar(&f, "Berto_Jail", IE, "CreateBertoJail");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Spawn", "DM_Mission_Dtritus_Jail");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "CostumeAtPolice", "1");
	return Sf_None(&f);
}

/* a prisoner stays at origin facing ang, with his talk */
static void Ie_Prisoner(scriptFrame_t *f, PyObject *e, const int origin[3], int ang, const char *use) {
	Sf_SetAttr(f, e, "LLogicOrigin", Sf_Build(f, "(iii)", origin[0], origin[1], origin[2]));
	Sf_SetAttr(f, e, "LLogicOriginAng", Sf_Build(f, "(iii)", 0, ang, 0));
	Lv_Stay(f, e, 6, Sf_Str(f, ""));
	Sf_SetAttrS(f, e, "UseFunc", use);
}

/* CreateBettyJail(e), CreateBertoJail(e) */
static PyObject *InitEpilogue_CreateBettyJail(PyObject *self, PyObject *args) {
	static const int origin[3] = { -743, -1284, 22411 };
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Ie_Prisoner(&f, e, origin, 130, "Mission_InitEpilogue.OnBettySpeech");
	return Sf_None(&f);
}

static PyObject *InitEpilogue_CreateBertoJail(PyObject *self, PyObject *args) {
	static const int origin[3] = { 364, -1284, 22524 };
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Ie_Prisoner(&f, e, origin, 210, "Mission_InitEpilogue.OnBertoSpeech");
	return Sf_None(&f);
}

/* Init(LevelPath): the crazy gambler calls in 0.25 s */
static PyObject *InitEpilogue_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Later(&f, Sf_Float(&f, 0.25), IEG("PreSetNextMission"), Sf_Build(&f, "()"), NULL);
	return Sf_None(&f);
}

/* the talk of a prisoner: msg and ans for Talk */
static PyObject *Ie_Speech(PyObject *args, const char *msg, const char *ans) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Sf_Ask(&f, "Speech", "OnSpch", "(ss)", msg, ans);
	}
	return Sf_None(&f);
}

/* OnBettySpeech(UsrName, CharName, optId, FullOption), OnBertoSpeech(...) */
static PyObject *InitEpilogue_OnBettySpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Ie_Speech(args, "Mission_DestroyHumphrey_Betty_InitMsg1", "Mission_DestroyHumphrey_Betty_InitAns1");
}

static PyObject *InitEpilogue_OnBertoSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Ie_Speech(args, "Mission_DestroyHumphrey_Berto_InitMsg1", "Mission_DestroyHumphrey_Berto_InitAns1");
}

/* item into the hangar list of the save variable var, once */
static void Ie_Blueprint(scriptFrame_t *f, const char *var, const char *item) {
	PyObject *lst = Lv_SaveList(f, var, IE);
	if (Sf_In(f, Sf_Str(f, item), lst) == 0) {
		Lv_SaveAppend(f, var, lst, Sf_Str(f, item));
	}
}

/* SetNextMission(): the archbishop's ship and engine, Mission_DestroyHumphrey in Outskirts, the hard enemies */
static PyObject *InitEpilogue_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Ie_Blueprint(&f, "Hangar.shipsToEditList", "SArchbishop1");
	Ie_Blueprint(&f, "Hangar.availableEnginesList", "MARCHBISHOP1");
	Sf_Ask(&f, "Scrap", "IncSaveVar", "(si)", "NewBluePrintAvaliable", 2);
	Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Lang(&f, Sf_Str(&f, "Mission_DestroyHumphrey_SystemMessage1")));
	Sf_Ask(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_DestroyHumphrey", "Outskirts");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_DestroyHumphrey_MissionText1", 1);
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "BadReputation", "5");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "StdShipAIProfile", "MegaAce");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "CrazyGamblerAtGambinDen", "1");
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

PyMethodDef g_PoliceInitEpilogueMethods[] = {
	{ "InitStatus", InitEpilogue_InitStatus, METH_VARARGS, NULL },
	{ "CreateBettyJail", InitEpilogue_CreateBettyJail, METH_VARARGS, NULL },
	{ "CreateBertoJail", InitEpilogue_CreateBertoJail, METH_VARARGS, NULL },
	{ "Init", InitEpilogue_Init, METH_VARARGS, NULL },
	{ "OnBettySpeech", InitEpilogue_OnBettySpeech, METH_VARARGS, NULL },
	{ "OnBertoSpeech", InitEpilogue_OnBertoSpeech, METH_VARARGS, NULL },
	{ "SetNextMission", InitEpilogue_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_JailBreak ---- */

#define JBG(name) Sf_Global(&f, JB, name)

/* InitStatus(LevelPath): the NPCs of the jail after the level's own are browsed */
static PyObject *JailBreak_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", JBG("AfterBrowseNPC"));
	return Sf_None(&f);
}

/* AfterBrowseNPC(): only the gatekeepers stay; Poof the stapler, the guards of the jail, Dtritus (or Berto) with no
 * lives */
static PyObject *JailBreak_AfterBrowseNPC(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *keys = Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), "keys", "()");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *k = Sf_Next(&f, keys, i);
		if (!k) {
			break;
		}
		PyObject *res = Sf_Mod(&f, "CharsNPC", "CharRes");
		if (Sf_Compare(&f, Sf_Attr(&f, Sf_GetItem(&f, res, k), "ActCtrl"), "!=", Sf_Str(&f, "GateKeeper")) > 0) {
			Sf_DelItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), k);
		}
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "Desktop_JailBreak");
	Lv_Callback(&f, "Desktop_JailBreak", JB, "CreatePuff");
	Sf_SetAttr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Desktop_JailBreak")), "Pos", Sf_Build(&f, "(iii)", 6059, -1846, 26780));
	Sf_SetAttr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Desktop_JailBreak")), "Ang", Sf_Build(&f, "(iii)", 0, 270, 0));
	Sf_Own(&f, JB, "CreateMCList", "(s)", "Gear");
	Sf_Own(&f, JB, "CreateMCList", "(s)", "Police");
	Sf_Own(&f, JB, "CreateMCList", "(s)", "Sentinel");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Player.NumLives", "0");
	int dt = Sf_In(&f, Sf_Str(&f, "Dtritus"), Sf_Mod(&f, "CharConversor", "AvailableChars"));
	if (dt > 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
	} else if (dt == 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Berto");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "InitialMessage", Sf_Lang(&f, Sf_Str(&f, "Generics_NoMatrix")));
	}
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "RespawnChars"), "append", "(s)", "Desktop");
	return Sf_None(&f);
}

/* CreateMCList(Name): the mission characters <Name>_JailBreak_0<i> of the dummies there are (the gears stay) */
static PyObject *JailBreak_CreateMCList(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	for (int i = 0; i < 10 && !f.failed; i++) {
		int mark = f.n;
		PyObject *jbn = Sf_Cat(&f, "OsO", name, "_JailBreak_0", Sf_ReprI(&f, i));
		Sf_Ask(&f, "Scrap", "StartDummySearch", "(Oi)", Sf_Cat(&f, "sO", "DM_Mission_", jbn), 0);
		Sf_Print(&f, "O", jbn);
		if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "NextDummySearch", "()"), "==", Py_None) > 0) {
			break;
		}
		Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(O)", jbn);
		if (Sf_EqS(&f, name, "Gear") > 0) {
			PyObject *cb = Sf_Attr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), jbn), "CallbackFunc");
			Sf_Call(&f, Sf_Attr(&f, cb, "append"), "(O)", JBG("CreateGearStay"));
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* CreateGearStay(ch): a gear guard stays */
static PyObject *JailBreak_CreateGearStay(PyObject *self, PyObject *args) {
	PyObject *ch;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ch)) {
		return NULL;
	}
	Lv_Stay(&f, ch, 3, Sf_Str(&f, ""));
	return Sf_None(&f);
}

/* CreatePuff(e): Poof; unless the player is a guard he starts jumping in 5 s */
static PyObject *JailBreak_CreatePuff(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	static const char *const guards[] = { "Gear", "Sentinel", "Police", "Desktop", NULL };
	Sf_SetAttr(&f, e, "Descriptor", Sf_Lang(&f, Sf_Str(&f, "CharName_Poof")));
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int c = Sf_True(&f, usr);
	if (c > 0) {
		c = Sf_InS(&f, Sf_Attr(&f, usr, "ActCtrl"), guards);
		c = c < 0 ? -1 : !c;
	}
	if (c > 0) {
		Sf_Later(&f, Sf_Float(&f, 5.0), JBG("InitPuffReborn"), Sf_Build(&f, "(O)", e), NULL);
	}
	return Sf_None(&f);
}

/* InitPuffReborn(e): Poof at his place walks and turns, CheckForJump makes him jump */
static PyObject *JailBreak_InitPuffReborn(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttrI(&f, e, "LLogicDefaultState", 6);
	Sf_SetAttrS(&f, e, "LLogicDefaultTarget", "");
	Sf_SetAttrI(&f, e, "LLogicState", 6);
	Sf_SetAttrS(&f, e, "LLogicTarget", "");
	Sf_SetAttrI(&f, e, "LLogicIdle", 1);
	Sf_SetAttrI(&f, e, "LLogicAlwaysLock", 1);
	PyObject *cr = Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Desktop_JailBreak"));
	Sf_SetAttr(&f, e, "Pos", Sf_Attr(&f, cr, "Pos"));
	Sf_SetAttr(&f, e, "Ang", Sf_Attr(&f, cr, "Ang"));
	Sf_SetAttr(&f, e, "InCtrl_Walk", JBG("PuffWalk"));
	Sf_SetAttr(&f, e, "InCtrl_HRot", JBG("PuffTurn"));
	Sf_Later(&f, Sf_Float(&f, 2.0), JBG("CheckForJump"), Sf_Build(&f, "(O)", Sf_Attr(&f, e, "Name")), NULL);
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_JailBreak.StaplerSpeech");
	return Sf_None(&f);
}

/* StaplerSpeech(UsrName, CharName, optId, FullOption) */
static PyObject *JailBreak_StaplerSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_JailBreak_Stapler_InitMsg1", "Mission_JailBreak_Stapler_InitAns1");
	return Sf_None(&f);
}

/* CheckForJump(Name): Poof walks to the window; there he presses and releases the action key (jumps) */
static PyObject *JailBreak_CheckForJump(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) <= 0) {
		return Sf_None(&f);
	}
	PyObject *timy = NULL;
	int at = Sf_Compare(&f, Sf_Item(&f, Sf_Attr(&f, e, "Pos"), 2), ">=", Sf_Int(&f, 27170));
	if (at > 0) {
		Sf_SetAttrI(&f, e, "InCtrl_Walk", 0);
		Sf_SetAttrI(&f, e, "InCtrl_HRot", 0);
		int press = Sf_True(&f, JBG("ActPress"));
		if (press > 0) {
			Sf_SetAttrI(&f, e, "InCtrl_Act", 0);
			Sf_SetGlobal(&f, JB, "ActPress", Sf_Int(&f, 0));
			timy = f.failed ? NULL : Sf_Float(&f, Scrap_RandRange(0.2, 5.0));
		} else if (press == 0) {
			Sf_SetAttrI(&f, e, "InCtrl_Act", 1);
			Sf_SetGlobal(&f, JB, "ActPress", Sf_Int(&f, 1));
			timy = Sf_Float(&f, 0.1);
		}
	} else if (at == 0) {
		timy = Sf_Float(&f, 0.5);
		Sf_SetAttr(&f, e, "InCtrl_Walk", JBG("PuffWalk"));
		Sf_SetAttr(&f, e, "InCtrl_HRot", JBG("PuffTurn"));
	}
	Sf_Later(&f, timy, JBG("CheckForJump"), Sf_Build(&f, "(O)", name), NULL);
	return Sf_None(&f);
}

/* OnPossession(name, pos): after the first possession anyone may go */
static PyObject *JailBreak_OnPossession(PyObject *self, PyObject *args) {
	PyObject *name, *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &pos)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "GuaranteedChars", ",Anyone,");
	Sf_SetMod(&f, "CharConversor", "OnPossession", Py_None);
	return Sf_None(&f);
}

/* FirstTimeJail(): the warning of the first time in the jail */
static PyObject *JailBreak_FirstTimeJail(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "Menu", "ShowWarning", "(iO)", 0, Sf_Lang(&f, Sf_Str(&f, "Mission_JailBreak_Warning")));
	return Sf_None(&f);
}

/* Init(LevelPath): the jail door locked, the exit item, the policemen do not fight; the first time a warning */
static PyObject *JailBreak_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Str(&f, "DM_Door_PoliceJail_00")), "OnObjSlot", "!CloseDoorSlot");
	Sf_SetMod(&f, "CharConversor", "OnPossession", JBG("OnPossession"));
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_JailBreak_MissionText1", 0);
	PyObject *dmm = Lv_Dummy(&f, "DM_Mission_JailBreak_End", 0);
	PyObject *pos = Sf_Item(&f, dmm, 1);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "ExitJail", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "Item");
	Sf_SetAttrI(&f, l, "Radius", 500);
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttrS(&f, l, "OnTake", "Mission_JailBreak.NextMission");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "PoliceLogicBelligerant", 0);
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "FirstJailBreak"), "==", Py_None) > 0) {
		Sf_Schedule(&f, Sf_Time(&f), JBG("FirstTimeJail"), Sf_Build(&f, "()"), NULL);
		Sf_Ask(&f, "Scrap", "CreateSaveVar", "(ss)", "FirstJailBreak", "No");
		Lv_Fade(&f, lv_black, lv_black, 2.0);
		Sf_Later(&f, Sf_Float(&f, 0.1), Sf_Mod(&f, "SScorer", "Fade"), Sf_Build(&f, "(i(iiii)(iiii)d)", 0, 0, 0, 0, 255, 0, 0, 0, 0, 0.5), NULL);
	}
	return Sf_None(&f);
}

/* NextMission(ItemName, CharName): out of the jail: black, no input, the level loads again in 0.5 s */
static PyObject *JailBreak_NextMission(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &ch)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Lv_Fade(&f, lv_clear, lv_black, 0.5);
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Ask(&f, "Menu", "DrawScorer", "(is)", 0, "Inactive");
	if (Sf_True(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "IsXbox")) == 0) {
		Sf_Ask(&f, "SScorer", "SetSpeechText", "(Oiiii)", Sf_Lang(&f, Sf_Str(&f, "XBox_Load_Msg_1")), 10, 255, 255, 255);
	}
	PyObject *i = Sf_Entity(&f, item);
	Sf_SetAttrI(&f, i, "rTime", 10);
	Sf_SetAttrI(&f, i, "AppearsIfFree", 1);
	Sf_Method(&f, i, "Del", "()");
	Sf_Later(&f, Sf_Float(&f, 0.5), JBG("LoadPolice"), Sf_Build(&f, "()"), NULL);
	return Sf_None(&f);
}

/* LoadPolice(): the mission before the jail back, the alarm off, the game starts again at the jail's exit */
static PyObject *JailBreak_LoadPolice(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "SaveGame", "MarkWayPoint", "(sss)", "Levels/Police", "DM_Mission_JailBreak_End", "");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Mission.Map", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission_JailBreak.Map"));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Mission.Library", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission_JailBreak.Library"));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "AlarmActive", "0");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "AlarmStatus", "0.0");
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "Mission_JailBreak");
	Sf_Ask(&f, "SaveGame", "StartGame", "()");
	return Sf_None(&f);
}

PyMethodDef g_PoliceJailBreakMethods[] = {
	{ "InitStatus", JailBreak_InitStatus, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", JailBreak_AfterBrowseNPC, METH_VARARGS, NULL },
	{ "CreateMCList", JailBreak_CreateMCList, METH_VARARGS, NULL },
	{ "CreateGearStay", JailBreak_CreateGearStay, METH_VARARGS, NULL },
	{ "CreatePuff", JailBreak_CreatePuff, METH_VARARGS, NULL },
	{ "InitPuffReborn", JailBreak_InitPuffReborn, METH_VARARGS, NULL },
	{ "StaplerSpeech", JailBreak_StaplerSpeech, METH_VARARGS, NULL },
	{ "CheckForJump", JailBreak_CheckForJump, METH_VARARGS, NULL },
	{ "OnPossession", JailBreak_OnPossession, METH_VARARGS, NULL },
	{ "FirstTimeJail", JailBreak_FirstTimeJail, METH_VARARGS, NULL },
	{ "Init", JailBreak_Init, METH_VARARGS, NULL },
	{ "NextMission", JailBreak_NextMission, METH_VARARGS, NULL },
	{ "LoadPolice", JailBreak_LoadPolice, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_PoliceBossCheats ---- */

#define PCG(name) Sf_Global(&f, PC, name)
#define PCGF(name) Sf_Global(f, PC, name)

/* InitStatus(LevelPath): Ficus' friend; the police boss is discardable */
static PyObject *PoliceBossCheats_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Lv_MissionChar(&f, "Police_FicusPal", PC, "CreatePoliceFicusPal");
	Sf_Ask(&f, "Chars", "AddNewCharRes", "(si)", "PoliceBoss", 1);
	return Sf_None(&f);
}

/* Init(LevelPath): the poker game, the scene PoliFight, the state of the mission */
static PyObject *PoliceBossCheats_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Own(&f, PC, "CheatingLoop", "()");
	Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CrazyWind");
	PyObject *svv = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission_PoliceBossCheats");
	if (Sf_EqS(&f, svv, "TalkFicusPal") > 0) {
		Sf_Own(&f, PC, "LoadPoliFightScene", "()");
		Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Police_FicusPal");
	} else if (Sf_EqS(&f, svv, "BossCheats") > 0) {
		Sf_Own(&f, PC, "LoadPoliFightScene", "()");
		Sf_Own(&f, PC, "InitBossCheats", "()");
	} else if (Sf_EqS(&f, svv, "AskCrazyGambler") > 0) {
		Sf_Own(&f, PC, "InitAskCrazyGambler", "()");
	}
	return Sf_None(&f);
}

/* CreatePoliceFicusPal(e): Ficus' friend stays and talks */
static PyObject *PoliceBossCheats_CreatePoliceFicusPal(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_PoliceBossCheats.FicusPalMissionSpeech");
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	return Sf_None(&f);
}

/* Speech.SpeechRoutines["Police"](UsrName, CharName, optId, FullOption) */
static void Pc_PoliceTalk(scriptFrame_t *f, PyObject *usr, PyObject *ch, PyObject *opt, PyObject *full) {
	PyObject *fn = Sf_GetItem(f, Sf_Mod(f, "Speech", "SpeechRoutines"), Sf_Str(f, "Police"));
	Sf_Call(f, fn, "(OOOO)", usr, ch, opt, full);
}

/* FicusPalMissionSpeech(UsrName, CharName, optId, FullOption): Ficus' friend tells a policeman that the boss cheats
 * (InitBossCheats at the end); otherwise the police talk */
static PyObject *PoliceBossCheats_FicusPalMissionSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Lv_Speakers(&f, usr, ch);
	int other = Sf_Compare(&f, Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "ActCtrl"), "!=", Sf_Str(&f, "Police"));
	if (other > 0) {
		Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_TalkToFicusFriend_Police_InitMsg2", "Mission_TalkToFicusFriend_Police_InitAns2");
		return Sf_None(&f);
	}
	PyObject *svv = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Mission_PoliceBossCheats");
	int talk = Sf_EqS(&f, opt, "Talk") > 0;
	if (Sf_EqS(&f, svv, "TalkFicusPal") > 0) {
		if (talk) {
			Lv_OnEnd(&f, PC, "InitBossCheats", NULL);
			Sf_Ask(&f, "Speech", "NISpeech", "()");
			Sf_Ask(&f, "Speech", "AutoAddNI", "(s)", "Mission_TalkToFicusFriend_Police_NI_1Msg");
			Sf_Ask(&f, "Speech", "RunNI", "()");
		} else {
			Pc_PoliceTalk(&f, usr, ch, opt, full);
		}
	} else if (Sf_EqS(&f, svv, "BossCheats") > 0 && talk) {
		Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_PhotoPoliceBoss_Police_InitMsg1", "Mission_PhotoPoliceBoss_Police_InitAns1");
	} else {
		Pc_PoliceTalk(&f, usr, ch, opt, full);
	}
	return Sf_None(&f);
}

/* CreateFakeChar(CharName, CharType, CardType): a poker player at his dummy with his cards: the two entities */
static PyObject *PoliceBossCheats_CreateFakeChar(PyObject *self, PyObject *args) {
	PyObject *name, *type, *card;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &type, &card)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(Oi)", name, 0);
	PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	PyObject *pos = Sf_Item(&f, dummy, 1);
	PyObject *csp = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "WalkChar");
	PyObject *turn = Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180);
	Sf_SetAttr(&f, csp, "Ang", Sf_Build(&f, "(iOi)", 0, turn, 0));
	Sf_SetAttr(&f, csp, "ActCtrl", type);
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, csp, "Name"));
	Sf_SetAttr(&f, csp, "MoveTime", Sf_Float(&f, 0.1));
	Sf_SetAttr(&f, csp, "MovePos", Sf_Item(&f, dummy, 1));
	Sf_SetAttr(&f, csp, "MoveAng", Sf_Attr(&f, csp, "Ang"));
	Sf_SetAttrI(&f, csp, "Invulnerable", 1);
	Sf_SetAttrS(&f, csp, "ForceDefLoop", "StayPoker");
	Sf_SetAttrS(&f, csp, "ForceAction", "StayPoker");
	pos = Sf_Item(&f, dummy, 1);
	PyObject *csp2 = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Cat(&f, "Os", name, "Card"), Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
		Sf_Item(&f, pos, 2), "WithLife");
	turn = Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180);
	Sf_SetAttr(&f, csp2, "Ang", Sf_Build(&f, "(iOi)", 0, turn, 0));
	Sf_SetAttr(&f, csp2, "ActCtrl", card);
	Sf_SetAttr(&f, csp2, "MoveTime", Sf_Float(&f, 0.1));
	Sf_Ask(&f, "SAct", "GetClass", "(O)", type);
	pos = Sf_Item(&f, dummy, 1);
	PyObject *x = Sf_Item(&f, pos, 0);
	PyObject *y = Sf_Op(&f, Sf_Item(&f, Sf_Item(&f, dummy, 1), 1), '+', Sf_Ask(&f, "SAct", "GetCls", "(s)", "YModelDisp"));
	Sf_SetAttr(&f, csp2, "MovePos", Sf_Build(&f, "(OOO)", x, y, Sf_Item(&f, Sf_Item(&f, dummy, 1), 2)));
	Sf_SetAttr(&f, csp2, "MoveAng", Sf_Attr(&f, csp2, "Ang"));
	Sf_SetAttrS(&f, csp2, "DefLoop", "StayPoker");
	Sf_SetAttrS(&f, csp2, "Action", "StayPoker");
	Sf_SetAttrI(&f, csp2, "Invulnerable", 1);
	return Sf_Return(&f, Sf_Build(&f, "(OO)", csp, csp2));
}

/* Scene.CreateLocalAction(cls, 0, name, anm, loop) for the poker actions of cls */
static void Pc_PokerActions(scriptFrame_t *f, PyObject *cls, byte brup) {
	static const struct {
		const char *name, *anm;
		int loop;
	} acts[] = { { "StayPoker", "RelaxPoker", 1 }, { "Stay0Poker", "Relax0Poker", 0 }, { "PlayPoker", "PlayPoker", 0 },
		{ "BrupPoker", "BrupPoker", 0 }, { "CheatPoker", "CheatPoker", 0 } };
	int mark = f->n;
	for (int i = 0; i < (brup ? 4 : 5); i++) {
		Sf_Ask(f, "Scene", "CreateLocalAction", "(Oissi)", cls, 0, acts[i].name, acts[i].anm, acts[i].loop);
	}
	Sf_Release(f, mark);
}

/* an event of the current action: SAct.SetAct AddEventAtTime, EventName, EventFunc */
static void Pc_Event(scriptFrame_t *f, PyObject *time, const char *name, const char *func) {
	int mark = f->n;
	Sf_Ask(f, "SAct", "SetAct", "(sO)", "AddEventAtTime", time);
	Sf_Ask(f, "SAct", "SetAct", "(ss)", "EventName", name);
	Sf_Ask(f, "SAct", "SetAct", "(ss)", "EventFunc", func);
	Sf_Release(f, mark);
}

/* try: exec(code) except: Scrap.ConsoleError(msg) */
static void Pc_TrySound(scriptFrame_t *f, const char *code, const char *msg) {
	if (f->failed) {
		return;
	}
	int mark = f->n;
	PyObject *locals = Sf_Keep(f, PyDict_New());
	Sf_Exec(f, Sf_Str(f, code), PC, locals);
	if (Sf_Except(f)) {
		Sf_Ask(f, "Scrap", "ConsoleError", "(s)", msg);
	}
	Sf_Release(f, mark);
}

/* CheatingLoop(): the poker table: the actions of the players and the cards with the belch events and the photo
 * window of the cheat, their sounds, the three players, the photo hook */
static PyObject *PoliceBossCheats_CheatingLoop(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetMod(&f, "Scene", "SceneName", Sf_Str(&f, "Poker"));
	Sf_SetItem(&f, Sf_Mod(&f, "Scene", "Actors"), Sf_Mod(&f, "Scene", "SceneName"), Sf_Build(&f, "[]"));
	Sf_Ask(&f, "SAct", "GetClass", "(s)", "PoliceBoss");
	Sf_Ask(&f, "SAct", "SetCls", "(si)", "CanBePossessed", 0);
	Pc_PokerActions(&f, Sf_Str(&f, "PoliceBoss"), 1);
	Pc_Event(&f, Sf_Float(&f, 1.10), "PoliceBossBrup", "!PoliceBossBrup");
	static const double fx[] = { 1.21, 1.33, 1.46, 1.61, 1.77, 1.94 };
	for (int i = 0; i < 6; i++) {
		Pc_Event(&f, Sf_Float(&f, fx[i]), "FXPoliceBossBrup", "!FXPoliceBossBrup");
	}
	Sf_Ask(&f, "Scene", "CreateLocalAction", "(sissi)", "PoliceBoss", 0, "CheatPoker", "CheatPoker", 0);
	Pc_Event(&f, Sf_Ask(&f, "SAct", "GetAct", "(s)", "TimeOfFrame50"), "DeactivatePhoto", "Mission_PoliceBossCheats.PhotoOn");
	Pc_Event(&f, Sf_Ask(&f, "SAct", "GetAct", "(s)", "TimeOfFrame140"), "DeactivatePhoto", "Mission_PoliceBossCheats.PhotoOff");
	Sf_Ask(&f, "SAct", "GetClass", "(s)", "Police");
	Pc_PokerActions(&f, Sf_Str(&f, "Police"), 0);
	static const char *const cards[] = { "CartasBoss", "CartasPoli" };
	for (int i = 0; i < 2; i++) {
		int mark = f.n;
		PyObject *type = Sf_Str(&f, cards[i]);
		Sf_Ask(&f, "Scene", "AddLocalActor", "(OO)", type, type);
		Pc_PokerActions(&f, type, 0);
		Sf_Ask(&f, "SAct", "SetCls", "(ss)", "DefAction", "StayPoker");
		Sf_Ask(&f, "Scrap", "Set", "(sO)", "DefInterpTime", Sf_Ask(&f, "Scrap", "Def", "(s)", "DefInterpTime"));
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "SAct", "GetClass", "(s)", "Police");
	Pc_TrySound(&f, "import MapSnd;MapSnd.PoliceCheats()", "SOUND ERROR : MapSnd.PoliceCheats() \n");
	Sf_Ask(&f, "SAct", "GetClass", "(s)", "PoliceBoss");
	Pc_TrySound(&f, "import MapSnd;MapSnd.PoliceBossCheats()", "SOUND ERROR : MapSnd.PoliceBossCheats() \n");
	static const struct {
		const char *dummy, *type, *card, *player, *cards;
	} players[] = { { "DM_Mission_Police_Dumb", "Police", "PokerCartasPoli", "Dumb2", "Dumb2c" },
		{ "DM_Mission_Police_Dumber", "Police", "PokerCartasPoli", "Dumb1", "Dumb1c" },
		{ "DM_Mission_PoliceBoss_BossCheater", "PoliceBoss", "PokerCartasBoss", "PoliceBoss", "PoliceBossc" } };
	for (int i = 0; i < 3; i++) {
		int mark = f.n;
		PyObject *pair[2];
		PyObject *r = Sf_Own(&f, PC, "CreateFakeChar", "(sss)", players[i].dummy, players[i].type, players[i].card);
		if (Sf_Unpack(&f, r, 2, pair)) {
			Sf_SetGlobal(&f, PC, players[i].player, pair[0]);
			Sf_SetGlobal(&f, PC, players[i].cards, pair[1]);
		}
		Sf_Release(&f, mark);
	}
	Sf_SetAttrI(&f, PCG("PoliceBoss"), "ArrowCanUse", 1);
	Sf_SetMod(&f, "CharAct", "TakenPhoto", PCG("OnPhoto"));
	return Sf_None(&f);
}

/* PlayAnm(Ent1, Ent2, Action): the action of a player and of his cards */
static PyObject *PoliceBossCheats_PlayAnm(PyObject *self, PyObject *args) {
	PyObject *e1, *e2, *action;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &e1, &e2, &action)) {
		return NULL;
	}
	Sf_SetAttr(&f, e1, "ForceAction", action);
	Sf_SetAttr(&f, e2, "ForceAction", action);
	return Sf_None(&f);
}

/* PlayAnm(<player>, <player>c, action) of the module's players */
static void Pc_Play(scriptFrame_t *f, const char *player, const char *action) {
	char cards[0x20];
	Str_Printf(cards, sizeof(cards), "%sc", player);
	Sf_Own(f, PC, "PlayAnm", "(OOs)", PCGF(player), PCGF(cards), action);
}

/* the same in delay seconds (PokerPlay) */
static void Pc_PlayLater(scriptFrame_t *f, PyObject *time, double delay, const char *player, const char *action) {
	char cards[0x20];
	Str_Printf(cards, sizeof(cards), "%sc", player);
	PyObject *at = time ? Sf_Op(f, time, '+', Sf_Float(f, delay)) : Sf_Op(f, Sf_Time(f), '+', Sf_Float(f, delay));
	Sf_Schedule(f, at, PCGF("PlayAnm"), Sf_Build(f, "(OOs)", PCGF(player), PCGF(cards), action), "PokerPlay");
}

/* WatchEvents(FuncCall): the three players relax one after the other, then the next turn */
static PyObject *PoliceBossCheats_WatchEvents(PyObject *self, PyObject *args) {
	PyObject *call;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &call)) {
		return NULL;
	}
	PyObject *time = Sf_Time(&f);
	Pc_PlayLater(&f, time, 5.0, "Dumb1", "Stay0Poker");
	Pc_PlayLater(&f, time, 5.5, "Dumb2", "Stay0Poker");
	Pc_PlayLater(&f, time, 6.0, "PoliceBoss", "Stay0Poker");
	Sf_Schedule(&f, Sf_Op(&f, time, '+', Sf_Float(&f, 8.0)), call, Sf_Build(&f, "()"), "PokerPlay");
	return Sf_None(&f);
}

/* the turns of the poker game: two plays, then WatchEvents(next) */
static PyObject *Pc_Turn(PyObject *args, const char *p1, const char *a1, const char *p2, const char *a2, const char *next) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Pc_Play(&f, p1, a1);
	Pc_Play(&f, p2, a2);
	Sf_Own(&f, PC, "WatchEvents", "(O)", PCG(next));
	return Sf_None(&f);
}

/* PlayDumb1(), PlayDumb2(), PlayPoliceBoss() */
static PyObject *PoliceBossCheats_PlayDumb1(PyObject *self, PyObject *args) {
	(void)self;
	return Pc_Turn(args, "Dumb2", "PlayPoker", "PoliceBoss", "Stay0Poker", "CheatPoliceBoss");
}

static PyObject *PoliceBossCheats_PlayDumb2(PyObject *self, PyObject *args) {
	(void)self;
	return Pc_Turn(args, "Dumb1", "PlayPoker", "Dumb2", "Stay0Poker", "BrupPoliceBoss");
}

static PyObject *PoliceBossCheats_PlayPoliceBoss(PyObject *self, PyObject *args) {
	(void)self;
	return Pc_Turn(args, "PoliceBoss", "PlayPoker", "Dumb2", "Stay0Poker", "PlayDumb1");
}

/* CheatPoliceBoss(): the boss cheats (the window for the photo) */
static PyObject *PoliceBossCheats_CheatPoliceBoss(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetAttrS(&f, PCG("PoliceBoss"), "ForceAction", "CheatPoker");
	Pc_PlayLater(&f, NULL, 0.5, "Dumb1", "CheatPoker");
	Pc_Play(&f, "Dumb2", "CheatPoker");
	Pc_Play(&f, "PoliceBoss", "CheatPoker");
	Sf_Own(&f, PC, "WatchEvents", "(O)", PCG("PlayDumb2"));
	return Sf_None(&f);
}

/* BrupPoliceBoss(): the boss belches */
static PyObject *PoliceBossCheats_BrupPoliceBoss(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Pc_Play(&f, "PoliceBoss", "BrupPoker");
	Pc_Play(&f, "Dumb1", "BrupPoker");
	Pc_PlayLater(&f, NULL, 0.5, "Dumb2", "BrupPoker");
	Sf_Own(&f, PC, "WatchEvents", "(O)", PCG("PlayPoliceBoss"));
	return Sf_None(&f);
}

/* PhotoOn(EntityName, EventName, Time), PhotoOff(...): the events of the cheat open and close the photo window */
static PyObject *Pc_Photo(PyObject *args, long on) {
	PyObject *ent, *ev, *time;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOO", &ent, &ev, &time)) {
		return NULL;
	}
	Sf_SetAttrI(&f, PCG("PoliceBoss"), "CanPhoto", on);
	return Sf_None(&f);
}

static PyObject *PoliceBossCheats_PhotoOn(PyObject *self, PyObject *args) {
	(void)self;
	return Pc_Photo(args, 1);
}

static PyObject *PoliceBossCheats_PhotoOff(PyObject *self, PyObject *args) {
	(void)self;
	return Pc_Photo(args, 0);
}

/* OnPhoto(SelectedEntity): the photo of the cheating boss: the game slows down, the scene follows (1); else 0 */
static PyObject *PoliceBossCheats_OnPhoto(PyObject *self, PyObject *args) {
	PyObject *sel;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &sel)) {
		return NULL;
	}
	if (Sf_Compare(&f, sel, "==", Sf_Attr(&f, PCG("PoliceBoss"), "Name")) > 0) {
		Sf_SetMod(&f, "CharAct", "TakenPhoto", Py_None);
		Sf_Ask(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "PokerPlay");
		Sf_Ask(&f, "Scrap", "SetTimeSpeed", "(d)", 0.05);
		Sf_Later(&f, Sf_Float(&f, 0.025), PCG("EndOfPhoto"), Sf_Build(&f, "()"), NULL);
		Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
		return Sf_Return(&f, Sf_Int(&f, 1));
	}
	return Sf_Return(&f, Sf_Int(&f, 0));
}

/* the Active of the three card entities */
static void Pc_CardsActive(scriptFrame_t *f, long on) {
	Sf_SetAttrI(f, PCGF("PoliceBossc"), "Active", on);
	Sf_SetAttrI(f, PCGF("Dumb1c"), "Active", on);
	Sf_SetAttrI(f, PCGF("Dumb2c"), "Active", on);
}

/* EndOfPhoto(): the scene PoliFight, InitAfterPhoto at its end; the cards hidden */
static PyObject *PoliceBossCheats_EndOfPhoto(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_Ask(&f, "Scrap", "SetTimeSpeed", "(d)", 1.0);
	Lv_Fade(&f, lv_black, lv_clear, 1.0);
	Sf_Ask(&f, "Scene", "Play", "(s)", "PoliFight");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", PCG("InitAfterPhoto"));
	Pc_CardsActive(&f, 0);
	return Sf_None(&f);
}

/* InitAfterPhoto(Usr): after the scene: white to black, InitAfterPhoto2 in 2 s */
static PyObject *PoliceBossCheats_InitAfterPhoto(PyObject *self, PyObject *args) {
	static const int white[4] = { 255, 255, 255, 255 };
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Sf_SetAttrI(&f, usr, "Active", 0);
	Lv_Fade(&f, white, lv_black, 2.0);
	Sf_Later(&f, Sf_Float(&f, 2.0), PCG("InitAfterPhoto2"), Sf_Build(&f, "(O)", usr), NULL);
	return Sf_None(&f);
}

/* InitAfterPhoto2(Usr): the policeman beaten: the player is Dtritus at the conversion point */
static PyObject *PoliceBossCheats_InitAfterPhoto2(PyObject *self, PyObject *args) {
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
	Sf_Ask(&f, "CharConversor", "ConvertChar", "(OOii)", Sf_Attr(&f, usr, "Name"), PCG("SpawnConversionPoint"), 0, 0);
	Sf_Later(&f, Sf_Float(&f, 1.0), Sf_Mod(&f, "SScorer", "Fade"), Sf_Build(&f, "(i(iiii)(iiii)d)", 0, 0, 0, 0, 255, 255, 255, 255, 0, 2.0), NULL);
	Sf_Later(&f, Sf_Float(&f, 3.0), PCG("InitAfterPhoto3"), Sf_Build(&f, "(O)", usr), NULL);
	return Sf_None(&f);
}

/* InitAfterPhoto3(Usr): the player back in control, a way point, Dtritus' remark, the crazy gambler next */
static PyObject *PoliceBossCheats_InitAfterPhoto3(PyObject *self, PyObject *args) {
	PyObject *usrArg;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usrArg)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	PyObject *usr = Sf_Entity(&f, Sf_Mod(&f, "CharConversor", "_usrName"));
	Sf_SetAttrI(&f, usr, "Active", 1);
	Sf_SetAttrI(&f, usr, "MoveTime", 0);
	Sf_SetAttrI(&f, usr, "AutoSelect", 1);
	Sf_Ask(&f, "SaveGame", "MarkWayPoint", "(OOO)", Sf_Mod(&f, "Init", "Path"), Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Spawn"), PCG("SpawnConversionPoint"));
	Sf_Ask(&f, "Speech", "RemoteComunication", "(ss)", "Mission_PhotoPoliceBoss_Dtritus_InitMsg1", "");
	Sf_Own(&f, PC, "InitAskCrazyGambler", "()");
	Pc_CardsActive(&f, 1);
	return Sf_None(&f);
}

/* InitAskCrazyGambler(): Dtritus' desktop is the target, the crazy gambler talks through it */
static PyObject *PoliceBossCheats_InitAskCrazyGambler(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_PoliceBossCheats", "AskCrazyGambler");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "DM_DTritusDesktop");
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToCrazyGambler_MissionText1", 1);
	Sf_Import(&f, "DTritusDesktop");
	Sf_Entity(&f, Sf_Str(&f, "DM_DTritusDesktop"));
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", PCG("CrazyGamblerSpeech"));
	return Sf_None(&f);
}

/* CrazyGamblerSpeech(UsrName, CharName, optId, FullOption): the crazy gambler's talk, SetNextMission at the end */
static PyObject *PoliceBossCheats_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "DTritusDesktop");
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", Py_None);
	Sf_Entity(&f, Sf_Str(&f, "DM_DTritusDesktop"));
	Lv_Speakers(&f, usr, ch);
	Lv_OnEnd(&f, PC, "SetNextMission", NULL);
	Sf_Ask(&f, "Speech", "NISpeech", "(s)", "CrazyGambler");
	Sf_Ask(&f, "Speech", "AutoAddNI", "(s)", "Mission_TalkToCrazyGambler_ComCrazyGambler_NI_1Msg");
	Sf_Ask(&f, "Speech", "RunNI", "()");
	return Sf_None(&f);
}

/* SetNextMission(): where the important characters are, the mission of NextMissionData */
static PyObject *PoliceBossCheats_SetNextMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	static const char *const vars[][2] = { { "MayorAtTownHall", "0" }, { "PoliceBossAtPolice", "0" }, { "BankMasterAtBank", "1" },
		{ "Journalist.Humphrey_Defaut", "Press" }, { "Journalist.Betty_Defaut", "TownHall" }, { "Journalist.Berto_Defaut", "TownHall" } };
	for (int i = 0; i < 6; i++) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", vars[i][0], vars[i][1]);
	}
	Sf_Ask(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Ask(&f, "__builtin__", "apply", "(OO)", Sf_Mod(&f, "MissionsFuncs", "SetNextMission"), PCG("NextMissionData"));
	Sf_Ask(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetInfoClues_MissionText1", 1);
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "Mission_PoliceBossCheats");
	return Sf_None(&f);
}

PyMethodDef g_PolicePoliceBossCheatsMethods[] = {
	{ "InitStatus", PoliceBossCheats_InitStatus, METH_VARARGS, NULL },
	{ "Init", PoliceBossCheats_Init, METH_VARARGS, NULL },
	{ "CreatePoliceFicusPal", PoliceBossCheats_CreatePoliceFicusPal, METH_VARARGS, NULL },
	{ "FicusPalMissionSpeech", PoliceBossCheats_FicusPalMissionSpeech, METH_VARARGS, NULL },
	{ "CreateFakeChar", PoliceBossCheats_CreateFakeChar, METH_VARARGS, NULL },
	{ "CheatingLoop", PoliceBossCheats_CheatingLoop, METH_VARARGS, NULL },
	{ "PlayAnm", PoliceBossCheats_PlayAnm, METH_VARARGS, NULL },
	{ "WatchEvents", PoliceBossCheats_WatchEvents, METH_VARARGS, NULL },
	{ "PlayDumb1", PoliceBossCheats_PlayDumb1, METH_VARARGS, NULL },
	{ "CheatPoliceBoss", PoliceBossCheats_CheatPoliceBoss, METH_VARARGS, NULL },
	{ "PlayDumb2", PoliceBossCheats_PlayDumb2, METH_VARARGS, NULL },
	{ "BrupPoliceBoss", PoliceBossCheats_BrupPoliceBoss, METH_VARARGS, NULL },
	{ "PlayPoliceBoss", PoliceBossCheats_PlayPoliceBoss, METH_VARARGS, NULL },
	{ "PhotoOn", PoliceBossCheats_PhotoOn, METH_VARARGS, NULL },
	{ "PhotoOff", PoliceBossCheats_PhotoOff, METH_VARARGS, NULL },
	{ "OnPhoto", PoliceBossCheats_OnPhoto, METH_VARARGS, NULL },
	{ "EndOfPhoto", PoliceBossCheats_EndOfPhoto, METH_VARARGS, NULL },
	{ "InitAfterPhoto", PoliceBossCheats_InitAfterPhoto, METH_VARARGS, NULL },
	{ "InitAfterPhoto2", PoliceBossCheats_InitAfterPhoto2, METH_VARARGS, NULL },
	{ "InitAfterPhoto3", PoliceBossCheats_InitAfterPhoto3, METH_VARARGS, NULL },
	{ "InitAskCrazyGambler", PoliceBossCheats_InitAskCrazyGambler, METH_VARARGS, NULL },
	{ "CrazyGamblerSpeech", PoliceBossCheats_CrazyGamblerSpeech, METH_VARARGS, NULL },
	{ "SetNextMission", PoliceBossCheats_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};
