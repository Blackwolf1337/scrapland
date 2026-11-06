/*
 * The scripts of the level Junkyard in C (data03.packed levels/junkyard/scripts, issue #2): the Map module (the
 * junk points of the scrap collector, the elevator to the level the player came from), MapShipEdit (the keypad of
 * the ship editor and the elevator platform that only takes a valid ship) and Mission_RustyFirstContact (Berto brings
 * Dtritus, the first ship with the editor tutorial, the scene with Rusty and Sputnik). The plain functions are data
 * (data/levels/junkyard/<module>.def): MapShipEdit.InitStatus, Mission_RustyFirstContact.LoadMecanicScene,
 * RustyFirstSpeech, OnEndOfShipEditTutorial, TutorialMsg and TutorialContinue. MapSnd is data.
 *
 * The module variables and the module code stay in the scripts: Map (Libraries, the alarm and blueprint save
 * variables), MapShipEdit (LastPlayer, ArrowMissionState), Mission_RustyFirstContact (kb, rusty, Params).
 */
#include "Levels.h"

#define MAP "Map"
#define MSE "MapShipEdit"
#define RF "Mission_RustyFirstContact"

#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define USR() Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0)
#define LANG(k) Sf_Lang(&f, Sf_Str(&f, k))
#define SAVE(n) Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", n)
#define G(m, n) Sf_Global(&f, m, n)
#define NOARGS Sf_Build(&f, "()")

/* ---- Map ---- */

/* InitStatus(LevelPath): an indoor map, the junkyard; the editor elevator, the atmosphere, no traffic ships, the junk
 * points the scrap collector goes to */
static PyObject *JunkyardMap_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Init");
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	Sf_SetMod(&f, "Init", "isJunkYard", Sf_Int(&f, 1));
	Sf_Import(&f, "Elevator");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "EditShip", "DM_Door_EditShip_00");
	Sf_Import(&f, "OutMap");
	Sf_Run(&f, "OutMap", "BrowseAtmospherePos", "()");
	Sf_Import(&f, "Traffic");
	Sf_SetMod(&f, "Traffic", "AcNormalShips", Sf_Build(&f, "[]"));
	Sf_SetMod(&f, "Traffic", "AcMediumShips", Sf_Build(&f, "[]"));
	Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[]"));
	PyObject *list = Sf_Ask(&f, "Scrap", "Get", "(s)", "JunkPointsListName");
	Sf_Run(&f, "Scrap", "CreateEntityList", "(O)", list);
	Sf_Run(&f, "Scrap", "StartDummySearch", "(si)", "DM_JunkPoint_*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		PyObject *pos = Sf_Item(&f, dummy, 1);
		PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Item(&f, dummy, 0), Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
			Sf_Item(&f, pos, 2), "Item");
		PyObject *ang = Sf_Item(&f, dummy, 2);
		Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(OOi)", Sf_Item(&f, ang, 0), Sf_Item(&f, ang, 1), 0));
		Sf_SetAttr(&f, l, "EntityList", list);
		Sf_SetAttrI(&f, l, "Radius", 100);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the editor elevator goes back to the previous level; no energy bar, no special action, the music */
static PyObject *JunkyardMap_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Elevator");
	PyObject *prev = Sf_Slice(&f, SAVE("PrevMap"), 7, 0x7fffffff);
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][O])", "DM_Door_EditShip_00", prev);
	Sf_Run(&f, "Scrap", "Set", "(si)", "EnergyBarActive", 0);
	Sf_Run(&f, "Scrap", "Set", "(si)", "SpecialActionActive", 0);
	Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/Track1.ogg", 1.0);
	return Sf_None(&f);
}

PyMethodDef g_JunkyardMapMethods[] = {
	{ "InitStatus", JunkyardMap_InitStatus, METH_VARARGS, NULL },
	{ "Init", JunkyardMap_Init, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapShipEdit ---- */

/* EnterEditShip(UsrName, CharName, optId, FullOption): the ship editor opens; the keypad off, the player frozen */
static PyObject *MapShipEdit_EnterEditShip(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Menu", "InitMenuSys", "(i)", 0);
	Sf_Run(&f, "Scrap", "SetTimeSpeed", "(i)", 1);
	Sf_Run(&f, "ShipEdit", "EnterEditShipMenu", "(is)", 0, "");
	Sf_Run(&f, "SScorer", "Set", "(issi)", 0, "Cursor", "Visible", 0);
	PyObject *kb = ENT("DM_EditKeyboard");
	Sf_SetGlobal(&f, MSE, "ArrowMissionState", Sf_Attr(&f, kb, "ArrowCanMission"));
	Sf_SetAttrI(&f, kb, "ArrowCanMission", 0);
	Sf_SetAttrI(&f, kb, "ArrowCanUse", 0);
	PyObject *last = USR();
	Sf_SetGlobal(&f, MSE, "LastPlayer", last);
	Sf_SetAttrI(&f, last, "UsrControl", -1);
	return Sf_None(&f);
}

/* SetInteract(o) */
static PyObject *MapShipEdit_SetInteract(PyObject *self, PyObject *args) {
	PyObject *o;
	SF_ARGS("O", &o);
	Sf_SetAttrI(&f, o, "ArrowCanUse", 1);
	return Sf_None(&f);
}

/* OnEndOfShipEdit(id, control): back to the game; the keypad usable again after 0.25 s, its mission arrow back */
static PyObject *MapShipEdit_OnEndOfShipEdit(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	Sf_Import(&f, "HangarTab");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Menu", "BackToGame", "(O)", id);
	PyObject *when = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 0.25));
	Sf_Schedule(&f, when, G(MSE, "SetInteract"), Sf_Build(&f, "(O)", ENT("DM_EditKeyboard")), NULL);
	Sf_SetAttrI(&f, G(MSE, "LastPlayer"), "UsrControl", 0);
	PyObject *state = G(MSE, "ArrowMissionState");
	if (Sf_Compare(&f, state, "!=", Py_None) > 0) {
		Sf_SetAttr(&f, ENT("DM_EditKeyboard"), "ArrowCanMission", state);
	}
	return Sf_None(&f);
}

/* CanUseElevator(UsrName, ElevatorName, optId, FullOption): 1 with a valid ship on the platform, else 0 and why */
static PyObject *MapShipEdit_CanUseElevator(PyObject *self, PyObject *args) {
	PyObject *usr, *el, *opt, *full;
	SF_ARGS("OOOO", &usr, &el, &opt, &full);
	Sf_Import(&f, "HangarTab");
	const char *why = NULL;
	if (Sf_Compare(&f, Sf_Mod(&f, "HangarTab", "prevShip"), "==", Py_None) > 0) {
		why = "JUNKYARD_Exit1";
	} else if (!f.failed) {
		int valid = Sf_True(&f, Sf_Ask(&f, "HangarTab", "IsShipValid", "(O)", Sf_Mod(&f, "HangarTab", "prevShip")));
		if (valid == 0) {
			why = "JUNKYARD_Exit2";
		}
	}
	if (why) {
		PyObject *fn = Sf_Mod(&f, "SScorer", "SetMsgText");
		Sf_Call(&f, fn, "(OO)", LANG(why), Sf_OpI(&f, Sf_Time(&f), '+', 5));
	}
	return Sf_Return(&f, Sf_Int(&f, why ? 0 : 1));
}

/* UseElevator(UsrName, ElevatorName, optId, FullOption): the characters of the junkyard go, then the elevator */
static PyObject *MapShipEdit_UseElevator(PyObject *self, PyObject *args) {
	static const char *const chars[] = { "Rusty_0", "Maintenance_00", "SputnikJunkyard_0", "SputnikJunkyard_1", "SputnikJunkyard_2" };
	PyObject *usr, *el, *opt, *full;
	SF_ARGS("OOOO", &usr, &el, &opt, &full);
	Sf_Import(&f, "Elevator");
	if (Sf_True(&f, Sf_Own(&f, MSE, "CanUseElevator", "(OOOO)", usr, el, opt, full)) > 0) {
		for (int i = 0; i < 5 && !f.failed; i++) {
			int mark = f.n;
			PyObject *e = ENT(chars[i]);
			if (Sf_True(&f, e) > 0) {
				Sf_Method(&f, e, "Del", "()");
			}
			Sf_Release(&f, mark);
		}
		Sf_Run(&f, "Elevator", "UseElevator", "(OOOO)", usr, el, opt, full);
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the keypad of the editor, the player's ship on the platform, the platform's use */
static PyObject *MapShipEdit_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	PyObject *dummy = Lv_Dummy(&f, "DM_EditKeyboard", 0);
	PyObject *pos = Sf_Item(&f, dummy, 1);
	PyObject *kb = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "DM_EditKeyboard", Sf_Item(&f, pos, 0), Sf_Item(&f, Sf_Item(&f, dummy, 1), 1),
		Sf_Item(&f, Sf_Item(&f, dummy, 1), 2), "WithLife");
	Sf_SetAttr(&f, kb, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180), 0));
	Sf_SetAttrI(&f, kb, "VRadius", 60);
	Sf_SetAttrI(&f, kb, "ArrowCanUse", 1);
	Sf_SetAttrI(&f, kb, "Life", 0);
	Sf_SetAttrI(&f, kb, "ArrnOpt", 0);
	Sf_SetAttr(&f, kb, "ArrOpt0", Sf_Cat(&f, "sO", "EditShip:", LANG("HINT_Junkyard_ShipEdit")));
	Sf_SetAttrS(&f, kb, "UseFunc", "MapShipEdit.EnterEditShip");
	Sf_SetAttrI(&f, kb, "ActiveCMask", 0);
	Sf_SetAttrI(&f, kb, "RadarShow", 2);
	Sf_SetAttrI(&f, kb, "ArrowCanMission", 1);
	Sf_SetAttr(&f, kb, "ArrowModMission", Sf_Ask(&f, "Scrap", "Get", "(s)", "ArrowTargetFile"));
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "HangarTab");
	int missing = Sf_Compare(&f, Sf_Mod(&f, "ShipEdit", "editShipPosDummy"), "==", Py_None);
	if (missing == 0) {
		missing = Sf_Compare(&f, Sf_Mod(&f, "ShipEdit", "editPlatform"), "==", Py_None);
	}
	if (missing > 0) {
		Sf_Run(&f, "Scrap", "ConsoleError", "(s)", "No existe el dummy 'DM_Door_EditShip_*'\n");
		return Sf_None(&f);
	}
	Sf_Run(&f, "ShipEdit", "CreatePlayerShip", "()");
	if (Sf_Compare(&f, Sf_Mod(&f, "HangarTab", "prevShip"), "!=", Py_None) > 0) {
		Sf_SetAttr(&f, Sf_Mod(&f, "HangarTab", "prevShip"), "Pos", Sf_Item(&f, Sf_Mod(&f, "ShipEdit", "editShipPosDummy"), 1));
		Sf_SetAttrI(&f, Sf_Mod(&f, "HangarTab", "prevShip"), "ActiveCMask", 1);
		Sf_Run(&f, "ShipEdit", "SetupForEdit", "([O])", Sf_Mod(&f, "HangarTab", "prevShip"));
	}
	Sf_SetAttrS(&f, Sf_Mod(&f, "ShipEdit", "editPlatform"), "UseFunc", "MapShipEdit.UseElevator");
	return Sf_None(&f);
}

PyMethodDef g_JunkyardMapShipEditMethods[] = {
	{ "EnterEditShip", MapShipEdit_EnterEditShip, METH_VARARGS, NULL },
	{ "SetInteract", MapShipEdit_SetInteract, METH_VARARGS, NULL },
	{ "OnEndOfShipEdit", MapShipEdit_OnEndOfShipEdit, METH_VARARGS, NULL },
	{ "CanUseElevator", MapShipEdit_CanUseElevator, METH_VARARGS, NULL },
	{ "UseElevator", MapShipEdit_UseElevator, METH_VARARGS, NULL },
	{ "Init", MapShipEdit_Init, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_RustyFirstContact ---- */

/* InitStatus(LevelPath): Berto's resources; coming from the orbit counts as from Outskirts; the credits library */
static PyObject *Rusty_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Chars");
	Sf_Run(&f, "Chars", "AddNewCharRes", "(s)", "Berto");
	if (Lv_SaveIs(&f, "PrevMap", "Levels/Orbit") > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "DoorEditShip");
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "PrevMap", "Levels/Outskirts");
	}
	Sf_Run(&f, "Scrap", "OpenPack", "(s)", "Scripts/Scorer");
	Sf_Run(&f, "Scrap", "PreloadLibrary", "(ss)", "CreditsText", "CreditsText.pyc");
	Sf_Run(&f, "Scrap", "ClosePack", "()");
	return Sf_None(&f);
}

/* Init(LevelPath): arriving with Berto (Rusty is the target, the editor waits) or straight to the editor; the final
 * scene, the credits */
static PyObject *Rusty_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	PyObject *kb = ENT("DM_EditKeyboard");
	Sf_SetGlobal(&f, RF, "kb", kb);
	PyObject *rusty = ENT("Rusty_0");
	Sf_SetGlobal(&f, RF, "rusty", rusty);
	int come = Sf_Compare(&f, SAVE("ComeFrom"), "!=", Sf_Str(&f, ""));
	if (come > 0) {
		Sf_Run(&f, "Scrap", "SetMoney", "(i)", 1100);
		Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToRusty_MissionText1", 1);
		Sf_SetAttrI(&f, G(RF, "kb"), "ArrowCanUse", 0);
		Sf_SetAttrS(&f, G(RF, "kb"), "UseFunc", "");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(RF, "rusty"), 1);
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(RF, "kb"), 0);
		Sf_SetItem(&f, Sf_Mod(&f, "Speech", "SpeechRoutines"), Sf_Str(&f, "Rusty"), G(RF, "RustyFirstSpeech"));
		Sf_Own(&f, RF, "CreateFakeBertoShip", "()");
		Sf_Own(&f, RF, "LoadMecanicScene", "()");
	} else if (come == 0) {
		Sf_Own(&f, RF, "InitBuildShip", "()");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(RF, "kb"), 0);
		Sf_Own(&f, RF, "OnEndOfShipEditFirstTime", "(is)", -1, "");
	}
	Sf_Own(&f, RF, "LoadMecanicFinalScene", "()");
	Sf_Import(&f, "CreditsText");
	Sf_Run(&f, "CreditsText", "Init", "()");
	return Sf_None(&f);
}

/* CreateFakeBertoShip(): Berto's ship and Berto on the editor platform; the platform's arrival starts the scene */
static PyObject *Rusty_CreateFakeBertoShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *epl = ENT("DM_Door_EditShip_00");
	PyObject *pos = Sf_Attr(&f, epl, "Pos");
	PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
	PyObject *v = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "FakeBertoShip", x, y, z, "Car");
	Sf_Method(&f, v, "SetModel", "(s)", "SBerto1");
	Sf_SetAttrS(&f, v, "MEng0", "MBerto1");
	Sf_SetAttrS(&f, v, "MEng1", "MBerto1");
	Sf_SetAttrS(&f, v, "MEng2", "MBerto1");
	Sf_SetAttr(&f, v, "LinkModel", Sf_Cat(&f, "Os", Sf_Attr(&f, epl, "Name"), ",DC_ShipLink"));
	Sf_SetAttrI(&f, v, "OffMul", 1);
	Sf_SetAttrI(&f, v, "UseLinkAng", 1);
	x = Sf_Item(&f, pos, 0), y = Sf_Item(&f, pos, 1), z = Sf_Item(&f, pos, 2);
	PyObject *c = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "FakeBerto", x, y, z, "WalkChar");
	Sf_SetAttrS(&f, c, "ActCtrl", "Berto");
	Sf_SetAttr(&f, c, "LinkModel", Sf_Cat(&f, "Oss", Sf_Attr(&f, epl, "Name"), ",", "DC_Platform"));
	Sf_SetAttr(&f, c, "OffModPos", Sf_Build(&f, "(iOi)", 0, Sf_Attr(&f, c, "FloorDist"), -200));
	Sf_SetAttrI(&f, c, "OffMul", 1);
	Sf_SetAttrI(&f, c, "UseLinkAng", 1);
	Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", G(RF, "StartInitialScene"));
	return Sf_None(&f);
}

/* the extras of the junkyard (the three Sputniks and the maintenance robot) come alive */
static byte Jy_Extras(scriptFrame_t *f) {
	static const char *const names[] = { "SputnikJunkyard_0", "SputnikJunkyard_1", "SputnikJunkyard_2", "Maintenance_00" };
	for (int i = 0; i < 4 && !f->failed; i++) {
		int mark = f->n;
		PyObject *e = Sf_Ask(f, "Scrap", "GetEntity", "(s)", names[i]);
		Sf_SetAttrI(f, e, "Active", 1);
		Sf_SetAttrI(f, e, "TimeSpeed", 1);
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* ShowExtras() */
static PyObject *Rusty_ShowExtras(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Jy_Extras(&f);
	return Sf_None(&f);
}

/* StartInitialScene(el): the platform is down: the player in place, the scene with Berto, Rusty and Sputnik */
static PyObject *Rusty_StartInitialScene(PyObject *self, PyObject *args) {
	PyObject *el;
	SF_ARGS("O", &el);
	Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", Py_None);
	PyObject *usr = USR();
	Sf_SetAttr(&f, usr, "Pos", Sf_Build(&f, "(ddd)", -618.68, -1940.08, 955.28));
	Sf_SetAttr(&f, usr, "Ang", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(RF, "BertoGoHome"));
	Sf_Run(&f, "Scene", "Play", "(s)", "Mecanic");
	Sf_Import(&f, "SputnikSound");
	Sf_Run(&f, "SputnikSound", "InitLoops", "(s)", "MecanicSputnik");
	Sf_Own(&f, RF, "ShowExtras", "()");
	Sf_SetAttrI(&f, ENT("FakeBertoShip"), "Active", 1);
	return Sf_None(&f);
}

static const char *const mecanic_final_actions[] = {
	"Scene.Load|MecanicFinal",
	"Scene.ActorAction|0|Dtritus|Walk",
	"Scene.ActorAction|27|Dtritus|Turn",
	"Scene.ActorAction|55|Dtritus|Relax",
	"Scene.ActorAction|260|Dtritus|Talk",
	"Scene.ActorAction|300|Dtritus|Relax",
	"Scene.ActorAction|550|Dtritus|Talk",
	"Scene.ActorAction|610|Dtritus|Turn",
	"Scene.ActorAction|630|Dtritus|WalkHi",
	"Scene.ActorAction|0|Rusty|Walk",
	"Scene.ActorAction|60|Rusty|Talk",
	"Scene.ActorAction|160|Rusty|TalkPoint",
	"Scene.ActorAction|210|Rusty|Relax",
	"Scene.ActorAction|305|Rusty|Talk",
	"Scene.ActorAction|365|Rusty|Talk0",
	"Scene.ActorAction|405|Rusty|Talk",
	"Scene.ActorAction|455|Rusty|Talk2",
	"Scene.ActorAction|505|Rusty|Talk",
	"Scene.ActorAction|545|Rusty|Relax",
	"Scene.ActorAction|615|Rusty|Turn",
	"Scene.ActorAction|640|Rusty|Hi",
	"Scene.ActorAction|675|Rusty|Relax",
	"Scene.ActorAction|0|Sputnik|Relax",
	"Scene.ActorAction|260|Sputnik|Relax3",
	"Scene.ActorAction|305|Sputnik|Relax",
	NULL,
};

static const char *const mecanic_final_actors[] = {
	"SAct.SetAct|EventName|FXMecanicFinalSputnik",
	"SAct.SetAct|EventFunc|Mission_RustyFirstContact.FXMecanicFinalSputnik",
	"Scene.AddText|60|Mission_Scene_BuildFirstShip_Rusty_1|15",
	"Scene.AddText|260|Mission_Scene_BuildFirstShip_Rusty_2|2",
	"Scene.AddText|305|Mission_Scene_BuildFirstShip_Rusty_3|10",
	"Scene.AddText|550|Mission_Scene_BuildFirstShip_Rusty_4|2",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk2|Talk2|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk3|Talk3|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk4|Talk4|1",
	"Scene.CreateLocalAction|Dtritus|0|WalkHi|WalkHi|1",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|Rusty|Rusty",
	"MakeChar.CreateRelaxAction|Rusty|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Rusty|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|Rusty|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Rusty|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|Rusty|0|Talk2|Talk2|1",
	"Scene.CreateLocalAction|Rusty|0|TalkPoint|TalkPoint|1",
	"Scene.CreateLocalAction|Rusty|0|Hi|Hi|1",
	"Scene.SetDefSounds|Rusty|Rusty",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|Sputnik|Sputnik",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax0|Relax0|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax1|Relax1|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax2|Relax2|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax3|Relax3|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax4|Relax4|1",
	"MakeChar.CreateRelaxAction|Sputnik|0|Relax5|Relax5|1",
	"Scene.SetDefSounds|Sputnik|Sputnik",
	"SAct.SetCls|DefAction|Relax",
	NULL,
};

/* LoadMecanicFinalScene(): the scene of the farewell with Rusty, Sputnik's effect at frame 499 (the interpolation
 * time is put back by FXMecanicFinalSputnik, where the script has that line) */
static PyObject *Rusty_LoadMecanicFinalScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "MakeChar");
	Sf_Calls(&f, RF, mecanic_final_actions);
	PyObject *fn = Sf_Mod(&f, "SAct", "SetAct");
	Sf_Call(&f, fn, "(sO)", "AddEventAtTime", Sf_Ask(&f, "SAct", "GetAct", "(O)", Sf_Cat(&f, "sO", "TimeOfFrame", Sf_ReprI(&f, 499))));
	Sf_Calls(&f, RF, mecanic_final_actors);
	return Sf_None(&f);
}

/* FXMecanicFinalSputnik(EntityName, EventName, Time): Sputnik's effect in the scene; the default interpolation time */
static PyObject *Rusty_FXMecanicFinalSputnik(PyObject *self, PyObject *args) {
	PyObject *ent, *ev, *time;
	SF_ARGS("OOO", &ent, &ev, &time);
	PyObject *e = ENT("MecanicFinalSputnik");
	if (Sf_True(&f, e) > 0) {
		Sf_Method(&f, e, "SetFX", "(s)", "CharacterSputnikSceneMecanicFinal");
	}
	Sf_Run(&f, "Scrap", "Set", "(sO)", "DefInterpTime", Sf_Ask(&f, "Scrap", "Def", "(s)", "DefInterpTime"));
	return Sf_None(&f);
}

/* BertoGoHome(usr): the end of the scene: Berto leaves on the platform, the camera follows it, no control */
static PyObject *Rusty_BertoGoHome(PyObject *self, PyObject *args) {
	PyObject *usr;
	SF_ARGS("O", &usr);
	Sf_Run(&f, "Elevator", "ElevatorActiveToGet", "(s)", "DM_Door_EditShip_00");
	Sf_SetMod(&f, "Elevator", "OnElevatorReachUp", G(RF, "BackDownBerto"));
	Sf_Run(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Run(&f, "Menu", "DrawScorer", "(is)", 0, "Inactive");
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0)), "EntityLink", "DM_Door_EditShip_00");
	PyObject *v = Sf_Build(&f, "(s(OO)i)", "Player0", Sf_Mod(&f, "Elevator", "TELEPORTER_UP_REACH"), Py_None, 0);
	Sf_SetItem(&f, Sf_Mod(&f, "Elevator", "LinkedElevator"), Sf_Str(&f, "DM_Door_EditShip_00"), v);
	return Sf_None(&f);
}

/* BackDownBerto(elv): up: Berto and his ship go, the platform comes down empty */
static PyObject *Rusty_BackDownBerto(PyObject *self, PyObject *args) {
	PyObject *elv;
	SF_ARGS("O", &elv);
	Sf_SetAttrS(&f, elv, "Action", "Down");
	Sf_SetAttrS(&f, elv, "DefLoop", "Closed");
	Sf_SetMod(&f, "Elevator", "OnElevatorReachUp", Py_None);
	Sf_Method(&f, ENT("FakeBertoShip"), "Del", "()");
	Sf_Method(&f, ENT("FakeBerto"), "Del", "()");
	Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", G(RF, "EndOfBertoBye"));
	return Sf_None(&f);
}

/* EndOfBertoBye(elv): down again: the player walks, the camera is free */
static PyObject *Rusty_EndOfBertoBye(PyObject *self, PyObject *args) {
	PyObject *elv;
	SF_ARGS("O", &elv);
	Sf_Run(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_Run(&f, "Menu", "DrawScorer", "(is)", 0, "Walk");
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0)), "EntityLink", "");
	Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", Py_None);
	return Sf_None(&f);
}

/* StartInitBuildShip(): after Rusty's talk: the editor keypad is the target, Rusty talks as usual, a way point */
static PyObject *Rusty_StartInitBuildShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Hangar.iHangarShip", "-1");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_BuildFirstShip_MissionText2", 1);
	Sf_SetAttrI(&f, G(RF, "kb"), "ArrowCanUse", 1);
	Sf_SetAttrI(&f, G(RF, "kb"), "ArrnOpt", 0);
	PyObject *kb = G(RF, "kb");
	Sf_SetAttr(&f, kb, "ArrOpt0", Sf_Cat(&f, "sO", "EditShip:", LANG("HINT_Junkyard_ShipEdit")));
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(RF, "rusty"), 0);
	Sf_SetItem(&f, Sf_Mod(&f, "Speech", "SpeechRoutines"), Sf_Str(&f, "Rusty"), Sf_Mod(&f, "CharsNPC", "GenericNPCSpeech"));
	Sf_Import(&f, "Init");
	Sf_Run(&f, "SaveGame", "MarkWayPoint", "(Oss)", Sf_Mod(&f, "Init", "Path"), "DM_Player_Spawn_", "");
	Sf_Own(&f, RF, "InitBuildShip", "()");
	return Sf_None(&f);
}

/* InitBuildShip(): the keypad opens the editor the first time, with its own end */
static PyObject *Rusty_InitBuildShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetAttrS(&f, G(RF, "kb"), "UseFunc", "Mission_RustyFirstContact.EnterEditShipFirstTime");
	Sf_SetMod(&f, "ShipEdit", "EndShipEditCallBack", G(RF, "OnEndOfShipEditFirstTime"));
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(RF, "kb"), 1);
	return Sf_None(&f);
}

/* EnterEditShipFirstTime(UsrName, CharName, optId, FullOption): the question whether to see the tutorial */
static PyObject *Rusty_EnterEditShipFirstTime(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(RF, "kb"), 0);
	Sf_Run(&f, "Menu", "InitMenuSys", "(i)", 0);
	PyObject *fn = Sf_Mod(&f, "Menu", "YesNoMenu");
	Sf_Call(&f, fn, "(iOss)", 0, LANG("Mission_BuildFirstShip_Tut_Question"), "Mission_RustyFirstContact.EnterEditShipTutorial",
		"Mission_RustyFirstContact.NoTutorialEnterEditShip");
	Sf_Run(&f, "SScorer", "Set", "(issi)", 0, "Cursor", "Visible", 0);
	Sf_Run(&f, "SScorer", "SetDefault", "(is)", 0, "Yes");
	return Sf_None(&f);
}

/* NoTutorialEnterEditShip(id, control) */
static PyObject *Rusty_NoTutorialEnterEditShip(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Menu", "BackToGame", "(Os)", id, "");
	PyObject *fn = Sf_Mod(&f, "MapShipEdit", "EnterEditShip");
	Sf_Call(&f, fn, "(Osss)", Sf_Attr(&f, USR(), "Name"), "", "", "");
	return Sf_None(&f);
}

/* OnEndOfShipEditFirstTime(id, control): with a ship built it is the target and the platform ends the mission, else
 * the keypad again (id -1: only that, from Init) */
static PyObject *Rusty_OnEndOfShipEditFirstTime(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	PyObject *elevator = ENT("DM_Door_EditShip_00");
	int ship = Sf_True(&f, Sf_Mod(&f, "HangarTab", "prevShip"));
	if (ship > 0) {
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", Sf_Mod(&f, "HangarTab", "prevShip"), 1);
		Sf_SetAttrS(&f, elevator, "UseFunc", "Mission_RustyFirstContact.UseElevatorFirstTime");
	} else if (ship == 0) {
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(RF, "kb"), 1);
		Sf_SetAttrS(&f, elevator, "UseFunc", "MapShipEdit.UseElevator");
	}
	if (Sf_Compare(&f, id, "!=", Sf_Int(&f, -1)) > 0) {
		Sf_Run(&f, "MapShipEdit", "OnEndOfShipEdit", "(OO)", id, control);
	}
	return Sf_None(&f);
}

/* UseElevatorFirstTime(UsrName, ElevatorName, optId, FullOption): with a valid ship the final scene, then the
 * mission ends (the arguments kept in Params for the elevator) */
static PyObject *Rusty_UseElevatorFirstTime(PyObject *self, PyObject *args) {
	PyObject *usr, *el, *opt, *full;
	SF_ARGS("OOOO", &usr, &el, &opt, &full);
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "MapShipEdit");
	Sf_SetGlobal(&f, RF, "Params", Sf_Build(&f, "(OOOO)", usr, el, opt, full));
	if (Sf_True(&f, Sf_Ask(&f, "MapShipEdit", "CanUseElevator", "(OOOO)", usr, el, opt, full)) > 0) {
		Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(RF, "EndOfMission"));
		Sf_Run(&f, "Scene", "Play", "(s)", "MecanicFinal");
		Sf_Import(&f, "SputnikSound");
		Sf_Run(&f, "SputnikSound", "InitLoops", "(s)", "MecanicFinalSputnik");
		Sf_Own(&f, RF, "ShowExtras", "()");
		Sf_SetAttrI(&f, Sf_Mod(&f, "HangarTab", "prevShip"), "Active", 1);
	}
	return Sf_None(&f);
}

/* EndOfMission(usr): saved, the next mission in Outskirts, out by the elevator */
static PyObject *Rusty_EndOfMission(PyObject *self, PyObject *args) {
	PyObject *usr;
	SF_ARGS("O", &usr);
	Sf_Import(&f, "MapShipEdit");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_GoToOutskirts", "Outskirts");
	Sf_Run(&f, "__builtin__", "apply", "(OO)", Sf_Mod(&f, "MapShipEdit", "UseElevator"), G(RF, "Params"));
	return Sf_None(&f);
}

/* EnterEditShipTutorial(id, control): the editor with the tutorial: the menu keys off, the messages, the steps
 * (selling the ships in the hangars first when there are any) */
static PyObject *Rusty_EnterEditShipTutorial(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "ShipEdit", "HangarTab", "EnginesTab", "WeaponsTab", "HullTab" };
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	for (int i = 0; i < 5; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_Run(&f, "Menu", "BackToGame", "(Os)", id, "");
	PyObject *name = Sf_Attr(&f, USR(), "Name");
	Sf_Run(&f, "MapShipEdit", "EnterEditShip", "(Osss)", name, "", "", "");
	Sf_SetMod(&f, "ShipEdit", "EndShipEditCallBack", G(RF, "OnEndOfShipEditTutorial"));
	Sf_Run(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "MainMissionEvent", "");
	Sf_Own(&f, RF, "TutorialMsgInit", "(OO)", id, control);
	PyObject *time = Sf_Time(&f);
	Sf_Run(&f, "SScorer", "SetDefault", "(Os)", id, "ShipEditTabHangar");
	Sf_Run(&f, "ShipEdit", "ChangeTab", "(Os)", id, "ShipEditTabHangar");
	int sell = 0;
	for (int i = 0; i < 9 && !f.failed; i++) {
		int mark = f.n;
		if (Sf_Compare(&f, Sf_Item(&f, Sf_Mod(&f, "ShipEdit", "editedShips"), i), "!=", Py_None) > 0) {
			sell = 1;
		}
		Sf_Release(&f, mark);
	}
	Sf_Schedule(&f, Sf_Op(&f, time, '+', Sf_Float(&f, 1.0)), G(RF, "EditShipTutorial_Step"), Sf_Build(&f, "(OOi)", id, control, sell ? 0 : 1),
		NULL);
	return Sf_None(&f);
}

/* SScorer.Set(id, item, var, v) as a line of the tutorial */
static byte Jy_Set(scriptFrame_t *f, PyObject *id, const char *item, const char *var, PyObject *v) {
	return Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, var, v);
}

static byte Jy_SetI(scriptFrame_t *f, PyObject *id, const char *item, const char *var, long i) {
	return Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, var, i);
}

/* the two typing texts of the tutorial: Add, Text, X 320, Y, CentralText, the colour without alpha, hidden, no cursor */
static byte Jy_TypingText(scriptFrame_t *f, PyObject *id, const char *item, PyObject *text, long y) {
	static const char *const rgb[] = { "Red", "Green", "Blue" };
	int mark = f->n;
	Sf_Run(f, "SScorer", "Add", "(Ossi)", id, item, "TextTyping", 1);
	Jy_Set(f, id, item, "Text", text);
	Jy_SetI(f, id, item, "X", 320);
	Jy_SetI(f, id, item, "Y", y);
	Jy_SetI(f, id, item, "CentralText", 1);
	for (int i = 0; i < 3 && !f->failed; i++) {
		Jy_Set(f, id, item, rgb[i], Sf_Item(f, Sf_Mod(f, "Scorer", "MissionScorerText_Color"), i));
	}
	Jy_SetI(f, id, item, "Visible", 0);
	Jy_SetI(f, id, item, "CursorAtInit", 0);
	Jy_SetI(f, id, item, "CursorAtEnd", 0);
	Sf_Release(f, mark);
	return !f->failed;
}

/* TutorialMsgInit(id, control): the panel and the two texts of the tutorial messages, hidden */
static PyObject *Rusty_TutorialMsgInit(PyObject *self, PyObject *args) {
	static const char *const vars[] = { "BlendingMode", "X", "Y", "SizeX", "SizeY", "Type", "Alpha", "Red", "Green", "Blue", "Visible" };
	static const long vals[] = { 4, 46, 215 - 45, 640 - 46 * 2, 120, 4, 192, 16, 32, 128, 0 };
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "ShipEdit");
	Sf_Import(&f, "Scorer");
	Sf_Run(&f, "SScorer", "Add", "(Ossi)", id, "TutorialMsgBack", "Tab", 1);
	Sf_Run(&f, "Menu", "SetTabDefaults", "(Os)", id, "TutorialMsgBack");
	for (int i = 0; i < 11 && !f.failed; i++) {
		Jy_SetI(&f, id, "TutorialMsgBack", vars[i], vals[i]);
	}
	Jy_TypingText(&f, id, "TutorialMsg", Sf_Str(&f, ""), 215);
	PyObject *text = Sf_Op(&f, LANG("Mission_BuildFirstShip_Tut_Continue"), '%', Sf_Mod(&f, "ShipEdit", "UseTxt"));
	Jy_TypingText(&f, id, "TutorialMsgContinue", text, 215 + 30);
	return Sf_None(&f);
}

/* time + k * tf with Python floats */
static PyObject *Jy_At(scriptFrame_t *f, PyObject *time, double k, double tf) {
	return Sf_Op(f, time, '+', Sf_Float(f, k * tf));
}

/* a step of the tutorial: (when, module, function, item) scheduled as module.function(id, item), when * tf after the
 * step's time; a NULL module ends the table */
typedef struct {
	double when;
	const char *module;
	const char *func;
	const char *item;
} jyStep_t;

static byte Jy_Steps(scriptFrame_t *f, PyObject *time, double tf, PyObject *id, const jyStep_t *steps) {
	for (; steps->module && !f->failed; steps++) {
		int mark = f->n;
		Sf_Schedule(f, Jy_At(f, time, steps->when, tf), Sf_Mod(f, steps->module, steps->func), Sf_Build(f, "(Os)", id, steps->item), NULL);
		Sf_Release(f, mark);
	}
	return !f->failed;
}

static const jyStep_t step_hangar[] = {
	{ 1.0, "SScorer", "SetDefault", "Hangar1" },
	{ 2.0, "HangarTab", "SelectItem", "Hangar1" },
	{ 2.01, "SScorer", "SetDefault", "Hangar1" },
	{ 3.5, "SScorer", "SetDefault", "ShipType" },
	{ 5.5, "HangarTab", "BuyShip", "BuyShip" },
	{ 5.51, "SScorer", "SetDefault", "Hangar1" },
	{ 0, NULL, NULL, NULL },
};

static const jyStep_t step_engines[] = {
	{ 1.0, "SScorer", "SetDefault", "ShipEditTabHangar" },
	{ 2.0, "SScorer", "SetDefault", "ShipEditTabEngines" },
	{ 3.0, "ShipEdit", "ChangeTab", "ShipEditTabEngines" },
	{ 4.5, "SScorer", "SetDefault", "Engine1" },
	{ 6.0, "EnginesTab", "PutEngine", "Engine1" },
	{ 0, NULL, NULL, NULL },
};

static const jyStep_t step_weapons[] = {
	{ 1.0, "SScorer", "SetDefault", "ShipEditTabEngines" },
	{ 2.0, "SScorer", "SetDefault", "ShipEditTabWeapons" },
	{ 3.0, "ShipEdit", "ChangeTab", "ShipEditTabWeapons" },
	{ 4.0, "SScorer", "SetDefault", "Vulcan" },
	{ 5.5, "WeaponsTab", "SelectWeapon", "Vulcan" },
	{ 7.0, "SScorer", "SetDefault", "VulcanSlot0" },
	{ 8.5, "WeaponsTab", "LoadWeapon", "VulcanSlot0" },
	{ 0, NULL, NULL, NULL },
};

static const jyStep_t step_hull[] = {
	{ 1.0, "SScorer", "SetDefault", "Vulcan" },
	{ 2.0, "SScorer", "SetDefault", "ShipEditTabWeapons" },
	{ 3.0, "SScorer", "SetDefault", "ShipEditTabHull" },
	{ 4.0, "ShipEdit", "ChangeTab", "ShipEditTabHull" },
	{ 5.0, "SScorer", "SetDefault", "HullUp" },
	{ 0, NULL, NULL, NULL },
};

static const jyStep_t step_exit[] = {
	{ 1.0, "SScorer", "SetDefault", "Exit" },
	{ 3.0, "ShipEdit", "ExitEditShipMenu", "Exit" },
	{ 0, NULL, NULL, NULL },
};

/* EditShipTutorial_Step(id, control, step): the message of the step and the clicks of the editor it shows, scheduled
 * (tf 2 s per unit), then the next step */
static PyObject *Rusty_EditShipTutorial_Step(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Menu", "ShipEdit", "HangarTab", "EnginesTab", "WeaponsTab", "HullTab" };
	static const char *const msgs[] = { "Mission_BuildFirstShip_Tut_Hangars", "Mission_BuildFirstShip_Tut_Engines",
		"Mission_BuildFirstShip_Tut_Weapons", "Mission_BuildFirstShip_Tut_Hull", "Mission_BuildFirstShip_Tut_Exit" };
	static const jyStep_t *const tables[] = { step_hangar, step_engines, step_weapons, step_hull, step_exit };
	static const double next[] = { 7.5, 8.0, 10.5, 9.5, 0 };
	const double tf = 2.0;
	PyObject *id, *control, *step;
	SF_ARGS("OOO", &id, &control, &step);
	for (int i = 0; i < 6; i++) {
		Sf_Import(&f, mods[i]);
	}
	PyObject *time = Sf_Time(&f);
	if (Sf_EqI(&f, step, 0) > 0) {
		Sf_Own(&f, RF, "TutorialMsg", "(OOO)", id, control, LANG("Mission_BuildFirstShip_Tut_Recycle"));
		for (int n = 0; n < 9 && !f.failed; n++) {
			int mark = f.n;
			if (Sf_Compare(&f, Sf_Item(&f, Sf_Mod(&f, "ShipEdit", "editedShips"), n), "!=", Py_None) > 0) {
				Sf_Print(&f, "O", Sf_Cat(&f, "sO", "Recycle Hangar", Sf_ReprI(&f, n + 1)));
				PyObject *hangar = Sf_Cat(&f, "sO", "Hangar", Sf_ReprI(&f, n + 1));
				Sf_Schedule(&f, Jy_At(&f, time, 0.0, tf), Sf_Mod(&f, "SScorer", "SetDefault"), Sf_Build(&f, "(OO)", id, hangar), NULL);
				hangar = Sf_Cat(&f, "sO", "Hangar", Sf_ReprI(&f, n + 1));
				Sf_Schedule(&f, Jy_At(&f, time, 1.0, tf), Sf_Mod(&f, "HangarTab", "SelectItem"), Sf_Build(&f, "(OO)", id, hangar), NULL);
				Sf_Schedule(&f, Jy_At(&f, time, 3.5, tf), Sf_Mod(&f, "SScorer", "SetDefault"), Sf_Build(&f, "(Os)", id, "Sell"), NULL);
				Sf_Schedule(&f, Jy_At(&f, time, 5.0, tf), Sf_Mod(&f, "HangarTab", "SellShip"), Sf_Build(&f, "(Os)", id, "Sell"), NULL);
				hangar = Sf_Cat(&f, "sO", "Hangar", Sf_ReprI(&f, n + 1));
				Sf_Schedule(&f, Jy_At(&f, time, 5.01, tf), Sf_Mod(&f, "SScorer", "SetDefault"), Sf_Build(&f, "(OO)", id, hangar), NULL);
				time = Jy_At(&f, time, 6.5, tf);
				Sf_ReleaseKeep(&f, mark, &time, 1);
			} else {
				Sf_Release(&f, mark);
			}
		}
		Sf_Schedule(&f, Jy_At(&f, time, 1.0, tf), G(RF, "EditShipTutorial_Step"), Sf_Build(&f, "(OOi)", id, control, 1), NULL);
	}
	int which = -1;
	for (int i = 0; i < 5 && which < 0 && !f.failed; i++) {
		if (Sf_EqI(&f, step, i + 1) > 0) {
			which = i;
		}
	}
	if (which >= 0) {
		Sf_Own(&f, RF, "TutorialMsg", "(OOO)", id, control, LANG(msgs[which]));
		Jy_Steps(&f, time, tf, id, tables[which]);
		if (which == 3) {
			for (int i = 0; i < 40 && !f.failed; i++) {
				int mark = f.n;
				PyObject *when = Sf_Op(&f, Jy_At(&f, time, 5.5, tf), '+', Sf_Float(&f, i * 0.05 * tf));
				Sf_Schedule(&f, when, Sf_Mod(&f, "HullTab", "IncrementHull"), Sf_Build(&f, "(Os)", id, "HullUp"), NULL);
				Sf_Release(&f, mark);
			}
		}
		if (next[which] > 0) {
			Sf_Schedule(&f, Jy_At(&f, time, next[which], tf), G(RF, "EditShipTutorial_Step"), Sf_Build(&f, "(OOi)", id, control, which + 2),
				NULL);
		}
	}
	return Sf_None(&f);
}

PyMethodDef g_JunkyardRustyFirstContactMethods[] = {
	{ "InitStatus", Rusty_InitStatus, METH_VARARGS, NULL },
	{ "Init", Rusty_Init, METH_VARARGS, NULL },
	{ "CreateFakeBertoShip", Rusty_CreateFakeBertoShip, METH_VARARGS, NULL },
	{ "ShowExtras", Rusty_ShowExtras, METH_VARARGS, NULL },
	{ "StartInitialScene", Rusty_StartInitialScene, METH_VARARGS, NULL },
	{ "LoadMecanicFinalScene", Rusty_LoadMecanicFinalScene, METH_VARARGS, NULL },
	{ "FXMecanicFinalSputnik", Rusty_FXMecanicFinalSputnik, METH_VARARGS, NULL },
	{ "BertoGoHome", Rusty_BertoGoHome, METH_VARARGS, NULL },
	{ "BackDownBerto", Rusty_BackDownBerto, METH_VARARGS, NULL },
	{ "EndOfBertoBye", Rusty_EndOfBertoBye, METH_VARARGS, NULL },
	{ "StartInitBuildShip", Rusty_StartInitBuildShip, METH_VARARGS, NULL },
	{ "InitBuildShip", Rusty_InitBuildShip, METH_VARARGS, NULL },
	{ "EnterEditShipFirstTime", Rusty_EnterEditShipFirstTime, METH_VARARGS, NULL },
	{ "NoTutorialEnterEditShip", Rusty_NoTutorialEnterEditShip, METH_VARARGS, NULL },
	{ "OnEndOfShipEditFirstTime", Rusty_OnEndOfShipEditFirstTime, METH_VARARGS, NULL },
	{ "UseElevatorFirstTime", Rusty_UseElevatorFirstTime, METH_VARARGS, NULL },
	{ "EndOfMission", Rusty_EndOfMission, METH_VARARGS, NULL },
	{ "EnterEditShipTutorial", Rusty_EnterEditShipTutorial, METH_VARARGS, NULL },
	{ "TutorialMsgInit", Rusty_TutorialMsgInit, METH_VARARGS, NULL },
	{ "EditShipTutorial_Step", Rusty_EditShipTutorial_Step, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};
