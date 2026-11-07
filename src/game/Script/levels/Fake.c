/*
 * The scripts of the level Fake in C (levels/fake/scripts of Data.packed, issue #2): the Map module of the intro (three
 * scenes with credits: the landing, the walk with Berto through the town hall, the flight in Berto's ship with the
 * fake traffic), then the Junkyard. The other scenes, the scene starts and SetMotionBlur are data (the files in
 * data/levels/fake). The module variables (TrafficDummyList, BertoShip, TrafficList) stay in the script.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h). The variables of Map are
 * read where the script reads them and written through at once, TrafficList is changed in place, the lists of InTraffic
 * and Scene.EndOfSceneCallback get new values as the script sets them (OBJLIB 5). Berto's ship and the traffic ships are
 * handle values, TrafficList holds them and is looped as a list of handle values (OBJLIB 7). The camera, the engine variables, the
 * save variables, string.split and the eval of the event name are C; Scrap.CreateEntity and Scrap.Set go through their
 * seams (OBJLIB 7). The call table of the flight scene runs its "Module.Func" names by name (the scene recorders of the
 * cases see them); InTraffic, Scene, Fx, CreditsText, SaveGame, MissionsFuncs and the functions of Map go by name
 * (OBJLIB 3).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/world.h"
#include "Levels.h"

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

/* ---- shared lines ---- */

/* cam = Scrap.GetEntity(Scrap.GetCam(0)); cam.clipNear = near; cam.clipFar = far (nearFirst: in this order, else
 * the other way round) */
static byte Fk_Clip(const nativeVal_t *nearV, const nativeVal_t *farV, byte nearFirst) {
	nativeVal_t name, cam;
	const char *s = cWorld_GetCamName(g_pWorld, 0);
	if (s) {
		LvN_Str(&name, s);
	} else {
		Native_SetNone(&name);
	}
	Native_SetNone(&cam);
	byte ok = Mode_EntityH(&name, &cam) &&
		(nearFirst ? Mode_VSet(&cam, "clipNear", nearV) && Mode_VSet(&cam, "clipFar", farV) :
			Mode_VSet(&cam, "clipFar", farV) && Mode_VSet(&cam, "clipNear", nearV));
	Native_Free(&name);
	Native_Free(&cam);
	return ok;
}

/* o.Active = 1; o.LinkModel = model; o.OffModPos = (0,0,0); o.DeltaOffMul = 1; o.OffMul = 1; o.UseLinkAng = 1 */
static byte Fk_Link(const nativeVal_t *o, const nativeVal_t *model) {
	nativeVal_t off;
	Native_SetEmpty(&off, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		Native_SetInt(Native_Append(&off), 0);
	}
	byte ok = LvN_SetI(o, "Active", 1) && Mode_VSet(o, "LinkModel", model) && Mode_VSet(o, "OffModPos", &off) &&
		LvN_SetI(o, "DeltaOffMul", 1) && LvN_SetI(o, "OffMul", 1) && LvN_SetI(o, "UseLinkAng", 1);
	Native_Free(&off);
	return ok;
}

/* ---- the functions ---- */

/* Init(LevelPath): the fake traffic, Berto's ship, the three scenes, the first one starts; the credits */
static byte Fk_Init(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const scenes[] = { "LoadFake0Scene", "LoadFake1Scene", "LoadFake2Scene", "PlayFake0" };
	static const char *const light[] = { "TLight1", "TLight2", "TLight3", NULL }, *const med[] = { "TMed1", "TMed2", NULL };
	static const char *const none[] = { NULL };
	nativeVal_t path, n, f;
	(void)a;
	(void)out;
	LvN_Str(&path, "Models/Vehicles/OutTraffic/");
	byte ok = Mode_Import("InTraffic") && LvN_ModTexts("InTraffic", "N_TrafficList", light) && LvN_ModSetI("InTraffic", "N_NumTraffic", 0) &&
		LvN_ModTexts("InTraffic", "M_TrafficList", med) && LvN_ModSetI("InTraffic", "M_NumTraffic", 0) &&
		LvN_ModTexts("InTraffic", "H_TrafficList", none) && LvN_ModSetI("InTraffic", "H_NumTraffic", 0) &&
		Mode_SetModVar("InTraffic", "Path", &path) && LV_CALL("InTraffic", "LoadFakeTraffic", NULL, "") &&
		LV_OWN(MAP, "CreateFakeBertoShip", NULL, "") && Mode_Import("Scene");
	Native_Free(&path);
	for (int i = 0; i < 4 && ok; i++) {
		ok = LV_OWN(MAP, scenes[i], NULL, "");
	}
	Native_SetInt(&n, 50);
	Native_SetInt(&f, 100000);
	return ok && Fk_Clip(&n, &f, 1) && Mode_Import("CreditsText") && LV_CALL("CreditsText", "Init", NULL, "");
}

/* PlayFake2(usr=None): the flight: Berto's ship and the traffic linked to the scene, the far plane, no model LOD */
static byte Fk_PlayFake2(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ship = Mode_V(&t), *list = Mode_V(&t), *dat = Mode_V(&t), *name = Mode_V(&t), *model = Mode_V(&t);
	nativeVal_t *v = Mode_V(&t), *nearV = Mode_V(&t), *farV = Mode_V(&t);
	if (!Mode_Import("Scene") || !LV_CALL("Scene", "Play", NULL, "s", "Fake2") ||
		!LvN_ModSetGlobal("Scene", "EndOfSceneCallback", MAP, "NextLevel") || !Mode_G(MAP, "BertoShip", ship) ||
		!Fk_Link(ship, Mode_S(&t, "DM_Scene_Fake2,NAVEBERTO")) || !LvN_GElems(MAP, "TrafficList", list)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(list), mark = t.n;
	for (int i = 0; i < n; i++) {
		byte ok = Native_SeqItem(list, i, dat) && LvN_Item(dat, 0, name) && Mode_Cat(&t, model, 2, Mode_S(&t, "DM_Scene_Fake2,"), name) &&
			LvN_Item(dat, 1, v) && Fk_Link(v, model) && LvN_SetF(v, "InCtrl_Acc", 1.0);
		Native_Free(dat);
		Native_Free(name);
		Native_Free(model);
		Native_Free(v);
		Mode_Release(&t, mark);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	Native_SetFloat(nearV, 50.0);
	Native_SetFloat(farV, 400000.0);
	return Mode_Done(&t, n >= 0 && Fk_Clip(nearV, farV, 0) && Mode_GVarSetI("R_UseMLOD", 0) && Mode_GVarSetI("FXEngineGlowDistFadeMult", 100));
}

/* NextLevel(usr=None): the end of the intro: the first mission with Rusty in the Junkyard */
static byte Fk_NextLevel(void) {
	nativeVal_t map;
	if (!Mode_Import("SaveGame") || !Mode_Import("MissionsFuncs") ||
		!LV_CALL("SaveGame", "MMDesc", NULL, "si", "Mission_TalkToRusty_MissionText1", 1L) ||
		!LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", "Mission_RustyFirstContact", "Junkyard") ||
		!LV_CALL("SaveGame", "MarkWayPoint", NULL, "sss", "Levels/Junkyard", "DM_Player_Spawn_Outskirts", "DTritus")) {
		return 0;
	}
	SaveVar_Set("ComeFrom", "DoorEditShip");
	SaveVar_Set("PrevMap", "Levels/Outskirts");
	Mode_SaveVar("Map", &map);
	byte ok = LV_CALL("SaveGame", "QLoad", NULL, "v", &map) && Mode_GVarSetI("R_UseMLOD", 1) && Mode_GVarSetI("FXEngineGlowDistFadeMult", 1);
	Native_Free(&map);
	return ok;
}

/* the hidden traffic ship of a dummy name of TrafficDummyList at (ctr, ctr, ctr), appended to TrafficList */
static byte Fk_Traffic(const nativeVal_t *nam, const nativeVal_t *ctr) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dat = Mode_V(&t), *v = Mode_V(&t), *model = Mode_V(&t);
	if (!Native_Split(nam, "_", dat) || !LvN_Create(nam, ctr, ctr, ctr, "Car", v) || !LvN_Item(dat, 1, model) ||
		!Mode_VModel(v, model) || !LV_CALL("Fx", "SetUpShipMotors", NULL, "v", v) || !LvN_SetI(v, "Active", 0) ||
		!LvN_SetI(v, "Traf_Active", 1)) {
		return Mode_Done(&t, 0);
	}
	/* TrafficList.append(nam, v): the append of 1.5.2 with two arguments appends the pair, in place (OBJLIB 5) */
	PyObject *list = Mode_GlobalPy(MAP, "TrafficList");
	PyObject *n = list ? Native_ToPy(nam) : NULL, *h = n ? Native_ToPy(v) : NULL;
	PyObject *r = h ? Script_Method(list, "append", "(OO)", n, h) : NULL;
	Py_XDECREF(r);
	Py_XDECREF(h);
	Py_XDECREF(n);
	Py_XDECREF(list);
	return Mode_Done(&t, r != NULL);
}

/* CreateFakeBertoShip(): Berto's ship with three engines, and a hidden traffic ship per dummy name of TrafficDummyList
 * (the model is the middle part of the name), all inactive until the flight */
static byte Fk_CreateFakeBertoShip(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *zero = Mode_V(&t), *made = Mode_V(&t), *ship = Mode_V(&t), *mcls = Mode_V(&t), *empty = Mode_V(&t);
	nativeVal_t *prev = Mode_V(&t), *path = Mode_V(&t), *fmt = Mode_V(&t), *ctr = Mode_V(&t), *names = Mode_V(&t);
	nativeVal_t *nam = Mode_V(&t), *next = Mode_V(&t);
	char var[0x20];
	Native_SetInt(zero, 0);
	if (!Mode_Import("Fx") || !Mode_Import("InTraffic") || !Mode_Import("string") || !Mode_GVarSetI("FXVehicleFloatingFactor", 0) ||
		!LvN_Create(Mode_S(&t, "Actor_SBerto1"), zero, zero, zero, "Car", made) || !Mode_Put(MAP, "BertoShip", NULL, made) ||
		!Mode_G(MAP, "BertoShip", ship) || !Mode_VModel(ship, Mode_S(&t, "SBerto1")) ||
		!LV_CALL("Fx", "GetMotorClass", mcls, "s", "MBerto1")) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 3; i++) {
		byte ok;
		Str_Printf(var, sizeof(var), "MEng%d", i);
		ok = LvN_SetS(ship, var, "MBerto1");
		Str_Printf(var, sizeof(var), "EngAnmF%d", i);
		ok = ok && LvN_SetS(ship, var, "Flaps");
		Str_Printf(var, sizeof(var), "EngAnmM%d", i);
		ok = ok && LvN_SetS(ship, var, "Motor");
		Str_Printf(var, sizeof(var), "EngFxClass%d", i);
		if (!ok || !Mode_VSet(ship, var, mcls)) {
			return Mode_Done(&t, 0);
		}
	}
	Native_SetEmpty(empty, NV_LIST);
	if (!LV_CALL("Fx", "SetUpShipMotors", NULL, "v", ship) || !LvN_SetI(ship, "Active", 0) ||
		!Mode_Put(MAP, "TrafficList", NULL, empty) || !Native_GVarGet("ShipPathFmt", prev) ||
		!LvN_ModGet("InTraffic", "Path", path) || !Native_Arith('+', path, Mode_S(&t, "%s.m3d"), fmt) ||
		!Mode_GVarSet("ShipPathFmt", fmt) || !LvN_GElems(MAP, "TrafficDummyList", names)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(ctr, 2000);
	int n = Native_ForCount(names);
	for (int i = 0; i < n; i++) {
		byte ok = Native_SeqItem(names, i, nam) && Fk_Traffic(nam, ctr) && Native_Arith('+', ctr, Mode_I(&t, 2000), next);
		Native_Free(nam);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
		Native_Free(ctr);
		*ctr = *next;
		Native_SetNone(next);
	}
	return Mode_Done(&t, n >= 0 && Mode_GVarSet("ShipPathFmt", prev));
}

/* ActivateBertoEngines(EntityName, EventName, Time): an event of the flight scene: the thrust of Berto's ship is the
 * number in the event name */
static byte Fk_ActivateBertoEngines(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t ship, acc;
	(void)out;
	Native_SetNone(&acc);
	if (!Mode_G(MAP, "BertoShip", &ship)) {
		return 0;
	}
	byte ok = Native_EvalText(MAP, &a[1], &acc) && Mode_VSet(&ship, "InCtrl_Acc", &acc);
	Native_Free(&ship);
	Native_Free(&acc);
	return ok;
}

/* LoadFake2Scene(): the flight scene */
static byte Fk_LoadFake2Scene(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Scene") && Mode_Import("MakeChar") && Mode_Calls(MAP, fake2_scene);
}

LV_CORE0(Fk_CreateFakeBertoShip)

LV_FN(Fk, InitPy, 1, Fk_Init)
LV_FN(Fk, CreateFakeBertoShipPy, 0, Fk_CreateFakeBertoShip_C)
LV_FN(Fk, ActivateBertoEnginesPy, 3, Fk_ActivateBertoEngines)
LV_FN(Fk, LoadFake2ScenePy, 0, Fk_LoadFake2Scene)

/* PlayFake2(usr=None), NextLevel(usr=None): the argument is not used */
static PyObject *Fk_UsrKw(PyObject *args, PyObject *kw, byte (*fn)(void)) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = Py_None;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	return Mode_Result(fn());
}

static PyObject *Fk_PlayFake2Py(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Fk_UsrKw(args, kw, Fk_PlayFake2);
}

static PyObject *Fk_NextLevelPy(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Fk_UsrKw(args, kw, Fk_NextLevel);
}

PyMethodDef g_FakeMapMethods[] = {
	{ "Init", Fk_InitPy, 1, NULL },
	{ "PlayFake2", (PyCFunction)(void (*)(void))Fk_PlayFake2Py, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "NextLevel", (PyCFunction)(void (*)(void))Fk_NextLevelPy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateFakeBertoShip", Fk_CreateFakeBertoShipPy, 1, NULL },
	{ "ActivateBertoEngines", Fk_ActivateBertoEnginesPy, 1, NULL },
	{ "LoadFake2Scene", Fk_LoadFake2ScenePy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
