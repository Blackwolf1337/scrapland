/*
 * The scripts of the level Fake in C (levels/fake/scripts of Data.packed, issue #2): the Map module of the intro (three
 * scenes with credits: the landing, the walk with Berto through the town hall, the flight in Berto's ship with the
 * fake traffic), then the Junkyard. The other scenes, the scene starts and SetMotionBlur are data (the files in
 * data/levels/fake). The module variables (TrafficDummyList, BertoShip, TrafficList) stay in the script.
 */
#include "scrap/script.h"

#define MAP "Map"

/* the scene of the flight; the events of frames 950 to 1540 set the thrust of Berto's ship (ActivateBertoEngines) */
static const char *const fake2_scene[] = {
	"Scene.Load|Fake2",
	"Scene.AddGeneric|50|SSound.SetMusic(\"Music/Fake2.ogg\", 1.0)",
	"Scene.AddGeneric|50|import CreditsText;CreditsText.ShowCredit(19)",
	"Scene.AddGeneric|425|import CreditsText;CreditsText.ShowCredit(20)",
	"Scene.AddGeneric|850|import CreditsText;CreditsText.ShowCredit(21)",
	"Scene.AddGeneric|1300|import CreditsText;CreditsText.ShowCredit(22)",
	"Scene.ActorAction|0|Dtritus|Relax",
	"Scene.ActorAction|20|Dtritus|FallElevator",
	"Scene.ActorAction|66|Dtritus|Walk",
	"Scene.ActorAction|92|Dtritus|WalkAstonished",
	"Scene.ActorAction|176|Dtritus|RelaxAstonished",
	"Scene.ActorAction|250|Dtritus|LookShip",
	"Scene.ActorAction|340|Dtritus|WalkAstonished",
	"Scene.ActorAction|640|Dtritus|Talk",
	"Scene.ActorAction|680|Dtritus|RelaxAstonished",
	"Scene.ActorAction|720|Dtritus|LookShip",
	"Scene.ActorAction|800|Dtritus|WalkSide",
	"Scene.ActorAction|845|Dtritus|Run",
	"Scene.ActorAction|930|Dtritus|RelaxShip",
	"Scene.ActorAction|0|Berto|Relax",
	"Scene.ActorAction|18|Berto|FallElevator",
	"Scene.ActorAction|64|Berto|Walk",
	"Scene.ActorAction|134|Berto|Talk",
	"Scene.ActorAction|260|Berto|TalkAboutShip",
	"Scene.ActorAction|314|Berto|Walk",
	"Scene.ActorAction|600|Berto|RelaxShipRight",
	"Scene.ActorAction|700|Berto|TalkShipRight",
	"Scene.ActorAction|770|Berto|RelaxShipRight",
	"Scene.ActorAction|930|Berto|RelaxShip",
	"Scene.ActorAction|0|Cabina|Relax|Relax",
	"Scene.ActorAction|500|Cabina|RelaxLookRight|RelaxLookRight",
	"Scene.ActorAction|600|Cabina|RelaxLookRight|RelaxLookRight",
	"Scene.ActorAction|700|Cabina|TalkLookRight|TalkLookRight",
	"Scene.ActorAction|770|Cabina|RelaxLookRight|RelaxLookRight",
	"Scene.ActorAction|930|Cabina|Relax|Relax",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame950",
	"SAct.SetAct|EventName|'1.0",
	"SAct.SetAct|EventFunc|Map.ActivateBertoEngines",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame1030",
	"SAct.SetAct|EventName|'0.75",
	"SAct.SetAct|EventFunc|Map.ActivateBertoEngines",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame1450",
	"SAct.SetAct|EventName|'-1.0",
	"SAct.SetAct|EventFunc|Map.ActivateBertoEngines",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame1540",
	"SAct.SetAct|EventName|'0.0",
	"SAct.SetAct|EventFunc|Map.ActivateBertoEngines",
	"Scene.AddText|134|Mission_Scene_Fake_TownHall_16|7",
	"Scene.AddText|640|Mission_Scene_Fake_TownHall_17|2",
	"Scene.AddText|700|Mission_Scene_Fake_TownHall_18|4",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Run|Run|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk|Talk|1",
	"Scene.CreateLocalAction|Dtritus|0|FallElevator|FallElevator|0",
	"SAct.SetAct|InterpTime|0.1",
	"Scene.CreateLocalAction|Dtritus|0|WalkAstonished|WalkAstonished|1",
	"Scene.CreateLocalAction|Dtritus|0|WalkSide|WalkSide|1",
	"Scene.CreateLocalAction|Dtritus|0|RelaxAstonished|RelaxAstonished|1",
	"Scene.CreateLocalAction|Dtritus|0|LookShip|LookShip|0",
	"Scene.CreateLocalAction|Dtritus|0|RelaxShip|RelaxShip|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"Scene.AddCharActor|Berto|Berto",
	"MakeChar.CreateRelaxAction|Berto|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Berto|0|Talk|Talk|1",
	"Scene.CreateLocalAction|Berto|0|FallElevator|FallElevator|0",
	"SAct.SetAct|InterpTime|0.1",
	"Scene.CreateLocalAction|Berto|0|TalkAboutShip|TalkAboutShip|0",
	"Scene.CreateLocalAction|Berto|0|TalkShipRight|TalkShipRight|1",
	"Scene.CreateLocalAction|Berto|0|RelaxShipRight|RelaxShipRight|1",
	"Scene.CreateLocalAction|Berto|0|RelaxShip|RelaxShip|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.SetDefSounds|Berto|Berto",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	"Scene.AddLocalActor|Cabina|Cabina",
	"Scene.CreateLocalAction|Cabina|0|Relax|Relax|1",
	"Scene.CreateLocalAction|Cabina|0|RelaxLookRight|RelaxLookRight|1",
	"Scene.CreateLocalAction|Cabina|0|TalkLookRight|TalkLookRight|1",
	"SAct.SetCls|DefAction|Relax",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* cam = Scrap.GetEntity(Scrap.GetCam(0)); cam.clipNear = near; cam.clipFar = far */
static void Fk_Clip(scriptFrame_t *f, PyObject *nearV, PyObject *farV, byte nearFirst) {
	int mark = f->n;
	PyObject *cam = Sf_Entity(f, Sf_Ask(f, "Scrap", "GetCam", "(i)", 0));
	if (nearFirst) {
		Sf_SetAttr(f, cam, "clipNear", nearV);
		Sf_SetAttr(f, cam, "clipFar", farV);
	} else {
		Sf_SetAttr(f, cam, "clipFar", farV);
		Sf_SetAttr(f, cam, "clipNear", nearV);
	}
	Sf_Release(f, mark);
}

/* o.LinkModel = model; o.OffModPos = (0,0,0); o.DeltaOffMul = 1; o.OffMul = 1; o.UseLinkAng = 1, after o.Active = 1 */
static void Fk_Link(scriptFrame_t *f, PyObject *o, PyObject *model) {
	int mark = f->n;
	Sf_SetAttrI(f, o, "Active", 1);
	Sf_SetAttr(f, o, "LinkModel", model);
	Sf_SetAttr(f, o, "OffModPos", Sf_Build(f, "(iii)", 0, 0, 0));
	Sf_SetAttrI(f, o, "DeltaOffMul", 1);
	Sf_SetAttrI(f, o, "OffMul", 1);
	Sf_SetAttrI(f, o, "UseLinkAng", 1);
	Sf_Release(f, mark);
}

/* Init(LevelPath): the fake traffic, Berto's ship, the three scenes, the first one starts; the credits */
static PyObject *Fk_Init(PyObject *self, PyObject *args) {
	static const char *const scenes[] = { "LoadFake0Scene", "LoadFake1Scene", "LoadFake2Scene", "PlayFake0" };
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "InTraffic");
	Sf_SetMod(&f, "InTraffic", "N_TrafficList", Sf_Build(&f, "[sss]", "TLight1", "TLight2", "TLight3"));
	Sf_SetMod(&f, "InTraffic", "N_NumTraffic", Sf_Int(&f, 0));
	Sf_SetMod(&f, "InTraffic", "M_TrafficList", Sf_Build(&f, "[ss]", "TMed1", "TMed2"));
	Sf_SetMod(&f, "InTraffic", "M_NumTraffic", Sf_Int(&f, 0));
	Sf_SetMod(&f, "InTraffic", "H_TrafficList", Sf_Build(&f, "[]"));
	Sf_SetMod(&f, "InTraffic", "H_NumTraffic", Sf_Int(&f, 0));
	Sf_SetMod(&f, "InTraffic", "Path", Sf_Str(&f, "Models/Vehicles/OutTraffic/"));
	Sf_Run(&f, "InTraffic", "LoadFakeTraffic", "()");
	Sf_Call(&f, Sf_Global(&f, MAP, "CreateFakeBertoShip"), "()");
	Sf_Import(&f, "Scene");
	for (int i = 0; i < 4; i++) {
		int mark = f.n;
		Sf_Call(&f, Sf_Global(&f, MAP, scenes[i]), "()");
		Sf_Release(&f, mark);
	}
	Fk_Clip(&f, Sf_Int(&f, 50), Sf_Int(&f, 100000), 1);
	Sf_Import(&f, "CreditsText");
	Sf_Run(&f, "CreditsText", "Init", "()");
	return Sf_None(&f);
}

/* PlayFake2(usr=None): the flight: Berto's ship and the traffic linked to the scene, the far plane, no model LOD */
static PyObject *Fk_PlayFake2(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	Sf_Run(&f, "Scene", "Play", "(s)", "Fake2");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", Sf_Global(&f, MAP, "NextLevel"));
	Fk_Link(&f, Sf_Global(&f, MAP, "BertoShip"), Sf_Str(&f, "DM_Scene_Fake2,NAVEBERTO"));
	PyObject *list = Sf_Global(&f, MAP, "TrafficList");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *dat = Sf_Next(&f, list, i);
		if (!dat) {
			break;
		}
		Fk_Link(&f, Sf_Item(&f, dat, 1), Sf_Cat(&f, "sO", "DM_Scene_Fake2,", Sf_Item(&f, dat, 0)));
		Sf_SetAttr(&f, Sf_Item(&f, dat, 1), "InCtrl_Acc", Sf_Float(&f, 1.0));
		Sf_Release(&f, mark);
	}
	Fk_Clip(&f, Sf_Float(&f, 50.0), Sf_Float(&f, 400000.0), 0);
	Sf_Run(&f, "Scrap", "Set", "(si)", "R_UseMLOD", 0);
	Sf_Run(&f, "Scrap", "Set", "(si)", "FXEngineGlowDistFadeMult", 100);
	return Sf_None(&f);
}

/* NextLevel(usr=None): the end of the intro: the first mission with Rusty in the Junkyard */
static PyObject *Fk_NextLevel(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Sf_Import(&f, "MissionsFuncs");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToRusty_MissionText1", 1);
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_RustyFirstContact", "Junkyard");
	Sf_Run(&f, "SaveGame", "MarkWayPoint", "(sss)", "Levels/Junkyard", "DM_Player_Spawn_Outskirts", "DTritus");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "DoorEditShip");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "PrevMap", "Levels/Outskirts");
	Sf_Run(&f, "SaveGame", "QLoad", "(O)", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Map"));
	Sf_Run(&f, "Scrap", "Set", "(si)", "R_UseMLOD", 1);
	Sf_Run(&f, "Scrap", "Set", "(si)", "FXEngineGlowDistFadeMult", 1);
	return Sf_None(&f);
}

/* CreateFakeBertoShip(): Berto's ship with three engines, and a hidden traffic ship per dummy name of TrafficDummyList
 * (the model is the middle part of the name), all inactive until the flight */
static PyObject *Fk_CreateFakeBertoShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	char var[0x20];
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Fx");
	Sf_Import(&f, "InTraffic");
	Sf_Import(&f, "string");
	Sf_Run(&f, "Scrap", "Set", "(si)", "FXVehicleFloatingFactor", 0);
	Sf_SetGlobal(&f, MAP, "BertoShip", Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "Actor_SBerto1", 0, 0, 0, "Car"));
	PyObject *ship = Sf_Global(&f, MAP, "BertoShip");
	Sf_Method(&f, ship, "SetModel", "(s)", "SBerto1");
	PyObject *mcls = Sf_Ask(&f, "Fx", "GetMotorClass", "(s)", "MBerto1");
	for (int i = 0; i < 3 && !f.failed; i++) {
		Str_Printf(var, sizeof(var), "MEng%d", i);
		Sf_SetAttrS(&f, ship, var, "MBerto1");
		Str_Printf(var, sizeof(var), "EngAnmF%d", i);
		Sf_SetAttrS(&f, ship, var, "Flaps");
		Str_Printf(var, sizeof(var), "EngAnmM%d", i);
		Sf_SetAttrS(&f, ship, var, "Motor");
		Str_Printf(var, sizeof(var), "EngFxClass%d", i);
		Sf_SetAttr(&f, ship, var, mcls);
	}
	Sf_Run(&f, "Fx", "SetUpShipMotors", "(O)", ship);
	Sf_SetAttrI(&f, ship, "Active", 0);
	Sf_SetGlobal(&f, MAP, "TrafficList", Sf_Build(&f, "[]"));
	PyObject *prev = Sf_Ask(&f, "Scrap", "Get", "(s)", "ShipPathFmt");
	Sf_Run(&f, "Scrap", "Set", "(sO)", "ShipPathFmt", Sf_Op(&f, Sf_Mod(&f, "InTraffic", "Path"), '+', Sf_Str(&f, "%s.m3d")));
	PyObject *ctr = Sf_Int(&f, 2000), *names = Sf_Global(&f, MAP, "TrafficDummyList");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *nam = Sf_Next(&f, names, i);
		if (!nam) {
			break;
		}
		PyObject *dat = Sf_Ask(&f, "string", "split", "(Os)", nam, "_");
		PyObject *v = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", nam, ctr, ctr, ctr, "Car");
		Sf_Method(&f, v, "SetModel", "(O)", Sf_Item(&f, dat, 1));
		Sf_Run(&f, "Fx", "SetUpShipMotors", "(O)", v);
		Sf_SetAttrI(&f, v, "Active", 0);
		Sf_SetAttrI(&f, v, "Traf_Active", 1);
		Sf_Method(&f, Sf_Global(&f, MAP, "TrafficList"), "append", "(OO)", nam, v);
		ctr = Sf_OpI(&f, ctr, '+', 2000);
		Sf_ReleaseKeep(&f, mark, &ctr, 1);
	}
	Sf_Run(&f, "Scrap", "Set", "(sO)", "ShipPathFmt", prev);
	return Sf_None(&f);
}

/* ActivateBertoEngines(EntityName, EventName, Time): an event of the flight scene: the thrust of Berto's ship is the
 * number in the event name */
static PyObject *Fk_ActivateBertoEngines(PyObject *self, PyObject *args) {
	PyObject *ent, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ent, &event, &time)) {
		return NULL;
	}
	PyObject *ship = Sf_Global(&f, MAP, "BertoShip");
	Sf_SetAttr(&f, ship, "InCtrl_Acc", Sf_Eval(&f, event, MAP));
	return Sf_None(&f);
}

/* LoadFake2Scene(): the flight scene */
static PyObject *Fk_LoadFake2Scene(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "MakeChar");
	Sf_Calls(&f, MAP, fake2_scene);
	return Sf_None(&f);
}

PyMethodDef g_FakeMapMethods[] = {
	{ "Init", Fk_Init, 1, NULL },
	{ "PlayFake2", (PyCFunction)(void (*)(void))Fk_PlayFake2, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "NextLevel", (PyCFunction)(void (*)(void))Fk_NextLevel, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateFakeBertoShip", Fk_CreateFakeBertoShip, 1, NULL },
	{ "ActivateBertoEngines", Fk_ActivateBertoEngines, 1, NULL },
	{ "LoadFake2Scene", Fk_LoadFake2Scene, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
