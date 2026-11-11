/*
 * The scripts of the level Orbit in C (levels/orbit/scripts, issue #2): the Map module (the teleporter and the doors of
 * the entrance), MapDecont (the first mission: the tutorial, the decontamination, the scenes of the bishops, the
 * functionary and the fall to the town hall) and Mission_IntroHuman (the arrival of Bill, three scenes). The scenes
 * are call tables (Mode_Calls). MapSnd is data (data/levels/orbit/mapsnd.def). The module variables
 * (FXSceneDecontaminationFile, FXSceneBishopsFile, FXSceneBillArrives_AFile) and the module code (the save variable
 * Orbit.Decontaminated) stay in the scripts.
 *
 * The functions are native C (docs/objlib-plan.md, stage 2): they work on C values, read the variables of the level
 * modules where the script reads them (OBJLIB 5) and call the engine in C. The engine functions the cases record go
 * through their seams or their refs (Scrap.CreateEntity, Set, SetAlarm, SetLockAlarm, UsrEntity, AddScheduledFunc,
 * SScorer.SetMsgText, SAct.GetAct and SetAct, SSound.SetMusic through seams, OBJLIB 7; Scrap.OpenPack, PreloadLibrary,
 * ClosePack, SetCallFunc, AddParams, Execute, Achievement and SetDebrisSys through their refs, stage 7). Entities are
 * handle values (OBJLIB 7).
 *
 * Direct calls (stage 3): the other ports (Teleport, CharsNPC, Speech, SaveGame, CreditsText, Scorer, Scene, GateKeeper,
 * MissionsFuncs, Chars) and the C functions of the modules go through their refs (PORT_CALL, PORT_LOWN; the callbacks
 * of AfterBrowseNPC by the names of their refs), Items.Hint too; the scene tables and the function value gc.CanPassFunc
 * call a port's function through its entry (Mode_Calls, PORT_VALUE), and so do the scheduled calls (Mode_Later,
 * Mode_LaterIn, Mode_Schedule; Port_Scheduled, SScorer.SetMsgText's engine entry too). By name stay the functions
 * set as callbacks or given as values (EndOfSceneCallback,
 * AfterBrowseNPC, Speech.OnEndOfSpeech, GateKeeper.LocalCallFunc, the door functions of SetDoor, the hint function of
 * Items.Hint; OBJLIB 5).
 */
#include <string.h>

#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL orbit
#include "scrap/input.h"
#include "scrap/scrap.h"
#include "scrap/scorer.h"

#define MD "MapDecont"
#define IH "Mission_IntroHuman"

/* SScorer.SetMsgText(text, Scrap.GetTime() + secs) */
static byte Ob_Msg(const nativeVal_t *text, long secs) {
	return Mode_MsgText(text, Mode_Time() + (double)secs);
}

/* the language text of key on the message line for secs */
static byte Ob_MsgKey(const char *key, long secs) {
	nativeVal_t t = { 0 };
	Mode_Lang(key, &t);
	byte ok = Ob_Msg(&t, secs);
	Native_Free(&t);
	return ok;
}

/* the text of Mode_KeyText on the message line for secs */
static byte Ob_KeyMsg(const char *key, const char *action, long secs) {
	nativeVal_t t = { 0 };
	byte ok = Mode_KeyText(key, action, &t) && Ob_Msg(&t, secs);
	Native_Free(&t);
	return ok;
}

/* e.RadarShow = 1; e.ArrowCanMission = 1; e.ArrowModMission = Scrap.Get("ArrowMissionFile") (0: the first two only) */
static byte Ob_Arrow(const nativeVal_t *e, int on) {
	nativeVal_t file = { 0 };
	if (!LvN_SetI(e, "RadarShow", on) || !LvN_SetI(e, "ArrowCanMission", on)) {
		return 0;
	}
	if (!on) {
		return 1;
	}
	Native_SetNone(&file);
	byte ok = Native_GVarGet("ArrowMissionFile", &file) && Mode_VSet(e, "ArrowModMission", &file);
	Native_Free(&file);
	return ok;
}

/* Ob_Arrow of the entity name */
static byte Ob_EntArrow(const char *name, int on) {
	nativeVal_t e = { 0 };
	byte ok = LvN_Entity(name, &e) && Ob_Arrow(&e, on);
	Native_Free(&e);
	return ok;
}

/* Scrap.Set(global, file); Scrap.SetDebrisSys(file, 1, 0, 0, 1, 0, 0, Fx.<fx>) with file the global of module, read
 * at each use as the script does */
static byte Ob_SceneFx(const char *module, const char *global, const char *fx) {
	nativeVal_t file = { 0 }, c = { 0 };
	Native_SetNone(&file);
	Native_SetNone(&c);
	if (!Mode_Import("Fx") || !Mode_G(module, global, &file) || !Mode_GVarSet(global, &file)) {
		Native_Free(&file);
		return 0;
	}
	Native_Free(&file);
	/* through the ref of SetDebrisSys (the scene cases record it; the function looked up after the arguments) */
	byte ok = Mode_G(module, global, &file) && LvN_ModGet("Fx", fx, &c) &&
		PORT_CALL(Scrap, SetDebrisSys, NULL, "viiiiiiv", &file, 1L, 0L, 0L, 1L, 0L, 0L, &c);
	Native_Free(&file);
	Native_Free(&c);
	return ok;
}

/* usr.Pos = (x, y, z); usr.Ang = (0, ang, 0); SLogic.SetOnFloor(usr.Name) */
static byte Ob_PlaceUsr(const nativeVal_t *usr, long x, long y, long z, long ang) {
	nativeVal_t pos = { 0 }, a = { 0 }, name = { 0 };
	Mode_Ints3(&pos, x, y, z);
	Mode_Ints3(&a, 0, ang, 0);
	Native_SetNone(&name);
	byte ok = Mode_VSet(usr, "Pos", &pos) && Mode_VSet(usr, "Ang", &a) && Mode_VGet(usr, "Name", &name) &&
		Mode_SetOnFloor(&name);
	Native_Free(&pos);
	Native_Free(&a);
	Native_Free(&name);
	return ok;
}

/* Items.Hint((x, y, z), Mode_KeyText(key, "Use"), 5, mission); the lookup of Hint comes after the text now (it
 * differs only when both fail) */
static byte Ob_KeyHint(long x, long y, long z, const char *key, long mission) {
	nativeVal_t pos = { 0 }, text = { 0 };
	Mode_Ints3(&pos, x, y, z);
	byte ok = Mode_KeyText(key, "Use", &text) && PORT_CALL(Items, Hint, NULL, "vvii", &pos, &text, 5L, mission);
	Native_Free(&pos);
	Native_Free(&text);
	return ok;
}

/* Scene.EndOfSceneCallback = the global func of module (OBJLIB 5); Scene.Play(scene) */
static byte Ob_Play(const char *module, const char *func, const char *scene) {
	return LvN_ModSetGlobal("Scene", "EndOfSceneCallback", module, func) && PORT_CALL(Scene, Play, NULL, "s", scene);
}

/* Scene.Play(scene); Scene.EndOfSceneCallback = the global func of module */
static byte Ob_PlayThen(const char *scene, const char *module, const char *func) {
	return PORT_CALL(Scene, Play, NULL, "s", scene) && LvN_ModSetGlobal("Scene", "EndOfSceneCallback", module, func);
}

/* ---- Map ---- */

/* InitStatus(LevelPath): an indoor map */
static byte Orbit_InitStatusN(void) {
	nativeVal_t zero = { 0 };
	Native_SetInt(&zero, 0);
	return Mode_Import("Init") && Mode_SetModVar("Init", "Outdoor", &zero);
}

/* Init(LevelPath): the teleporters linked, the back doors of the entrance, the music */
static byte Orbit_InitN(void) {
	return Mode_Import("Teleport") &&
		PORT_CALL(Teleport, Link, NULL, "ss", "DM_Door_Teleporter_00", "DM_Door_Teleporter_01") &&
		Mode_EntSetS("DM_Door_OrbitDoorA_04", "OnObjSlot", "!BackDoorSlot") &&
		Mode_EntSetS("DM_Door_OrbitDoorA_03", "OnObjSlot", "!BackDoorSlot") && Mode_Import("SSound") &&
		LvN_SetMusic("Music/Track1.ogg", 1.0);
}

MODE_FN(Orbit, InitStatus, 1, Orbit_InitStatusN())
MODE_FN(Orbit, Init, 1, Orbit_InitN())

PyMethodDef g_OrbitMapMethods[] = {
	{ "InitStatus", Orbit_InitStatus, 1, NULL },
	{ "Init", Orbit_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapDecont ---- */

/* InitStatus(LevelPath): the characters of the mission, the functionary sits down at once, the credits library */
static byte Decont_InitStatusN(void) {
	static const char *const chars[] = { "Sentinel_Corridor", "Bishop_00", "Bishop_01", "Functionary_00", NULL };
	nativeVal_t fn = { 0 };
	SaveVar_Set("Challenge", "0");
	if (!Mode_Import("CharsNPC") || !Mode_Import("Chars") || !LvN_GObj(MD, "AfterBrowseNPC", &fn)) {
		return 0;
	}
	byte ok = LvN_ModAppend("CharsNPC", "AfterBrowseNPC", &fn);
	Native_Free(&fn);
	for (int i = 0; chars[i] && ok; i++) {
		ok = PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", chars[i]);
	}
	/* the pack calls through their refs (fc-r6b-or1 records them) */
	return ok && Mode_GVarSetN("FunctionaryMinTimeToSitDown", 0.0) && Mode_GVarSetN("FunctionaryMaxTimeToSitDown", 1.0) &&
		PORT_CALL(Scrap, OpenPack, NULL, "s", "Scripts/Scorer") &&
		PORT_CALL(Scrap, PreloadLibrary, NULL, "ss", "CreditsText", "CreditsText.pyc") &&
		PORT_CALL(Scrap, ClosePack, NULL, "");
}

/* Decontamination(UsrName, CharName, optId, FullOption): the keypad goes, the scene plays, the way point */
static byte Decont_DecontaminationN(void) {
	nativeVal_t kb = { 0 };
	byte ok = LvN_Entity("Decontamination_keyboard", &kb) && LvN_SetI(&kb, "MapCollision", 0) && Mode_VDel(&kb) &&
		Mode_Import("Scene") && Ob_Play(MD, "EndOfScene", "Decontamination") && PORT_LOWN(MapDecont, DecontaminatedWaypoint, NULL, "");
	Native_Free(&kb);
	return ok;
}

/* EndOfScene(usr): the player behind the decontamination, the voice of the check in, the doors open */
static byte Decont_EndOfSceneN(const nativeVal_t *usr) {
	nativeVal_t text = { 0 };
	if (!Mode_Import("Chars") || !Mode_Import("Speech") || !Ob_PlaceUsr(usr, -20334, 1056, -27485, -90) ||
		!PORT_CALL(Speech, OffVoice, NULL, "s", "Mission_Checkin_RemoteMessage2")) {
		return 0;
	}
	Mode_Lang("Mission_Checkin_RemoteMessage2", &text);
	/* "siiii": the text up to its first 0 byte */
	Scorer_SetSpeechText(Native_Text(&text), 3.0f, 220, 235, 255);
	Native_Free(&text);
	return PORT_LOWN(MapDecont, SetWaypoint, NULL, "i", 1L) && Mode_EntSetS("DM_Door_OrbitDoorA_02", "OnObjSlot", "!FrontDoorSlot");
}

/* DecontaminatedWaypoint(): marked as decontaminated, a way point behind the decontamination */
static byte Decont_DecontaminatedWaypointN(void) {
	if (!Mode_Import("SaveGame")) {
		return 0;
	}
	SaveVar_Set("Orbit.Decontaminated", "yes");
	return PORT_CALL(SaveGame, MarkWayPoint, NULL, "ss", "Levels/Orbit", "DM_Player_Spawn_Decontamination");
}

/* SetWaypoint(Decontaminated): the functionary as the target of the check in, or not yet */
static byte Decont_SetWaypointN(const nativeVal_t *dec) {
	nativeVal_t e = { 0 };
	if (!LvN_Entity("Functionary_00", &e)) {
		return 0;
	}
	int on = Native_True(dec);
	nativeVal_t file = { 0 };
	byte ok = LvN_SetI(&e, "RadarShow", on) && LvN_SetI(&e, "ArrowCanUse", on) && LvN_SetI(&e, "ArrowCanMission", on) &&
		(!on || (Native_GVarGet("ArrowMissionFile", &file) && Mode_VSet(&e, "ArrowModMission", &file))) &&
		Mode_Import("SaveGame") &&
		Mode_MMDesc(on ? "Mission_Checkin_MissionText1" : "Mission_Decontamination_MissionText1", 1L);
	Native_Free(&e);
	Native_Free(&file);
	return ok;
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
static byte Decont_LoadDecontaminationN(void) {
	return Mode_Import("Scene") && Mode_Import("MakeChar") && Mode_Calls(MD, decont_scene) &&
		Ob_SceneFx(MD, "FXSceneDecontaminationFile", "FXSCENEDECONTAMINATION");
}

/* the keypad entity name on the dummy pattern, turned to face the room (dummy[2][1] + 180) */
static byte Ob_Keypad(const char *pattern, const char *name, nativeVal_t *kb) {
	nativeVal_t dummy = { 0 }, pos = { 0 }, c[3] = { 0 }, ang = { 0 }, a = { 0 }, n = { 0 }, d = { 0 };
	int k = 0;
	Native_SetNone(&pos);
	Native_SetNone(&ang);
	Native_SetNone(&a);
	Native_SetNone(kb);
	LvN_Dummy(pattern, 1, &dummy);
	byte ok = LvN_Item(&dummy, 1, &pos);
	for (; ok && k < 3; k++) {
		ok = LvN_Item(&pos, k, &c[k]);
	}
	if (ok) {
		LvN_Str(&n, name);
		ok = LvN_Create(&n, &c[0], &c[1], &c[2], "WithLife", kb);
		Native_Free(&n);
	}
	ok = ok && LvN_Item(&dummy, 2, &ang) && LvN_Item(&ang, 1, &a);
	if (ok) {
		Native_SetInt(&n, 180);
		Native_SetNone(&d);
		ok = Native_Arith('+', &a, &n, &d);
		if (ok) {
			nativeVal_t t = { 0 };
			Native_SetEmpty(&t, NV_TUPLE);
			Native_SetInt(Native_Append(&t), 0);
			Native_Copy(Native_Append(&t), &d);
			Native_SetInt(Native_Append(&t), 0);
			ok = Mode_VSet(kb, "Ang", &t);
			Native_Free(&t);
		}
		Native_Free(&d);
	}
	while (k-- > 0) {
		Native_Free(&c[k]);
	}
	Native_Free(&dummy);
	Native_Free(&pos);
	Native_Free(&ang);
	Native_Free(&a);
	return ok;
}

/* Init(LevelPath): the start of the mission, before or after the decontamination */
static byte Decont_InitN(void) {
	nativeVal_t sv = { 0 }, dec = { 0 }, kb = { 0 }, path = { 0 }, file = { 0 }, fn1 = { 0 }, fn2 = { 0 }, pos = { 0 };
	nativeVal_t text = { 0 }, e = { 0 };
	if (!Mode_Import("Init") || !Mode_Import("CreditsText") || !PORT_CALL(CreditsText, Init, NULL, "") ||
		!Mode_EntSetS("DM_Door_ElevatorOrbitC_TownHall", "OnObjSlot", "")) {
		return 0;
	}
	Mode_SaveVar("Orbit.Decontaminated", &sv);
	int d = Native_IsStr(&sv, "yes");
	Native_Free(&sv);
	Native_SetInt(&dec, d);
	if (!d) {
		nativeVal_t twenty = { 0 };
		Native_SetInt(&twenty, 20);
		if (!Ob_EntArrow("GateKeeper_Portalis", 1) || !Mode_Import("Scorer") ||
			!Ob_MsgKey("Mission_Decontamination_SystemMessage1", 50) || !Mode_LaterIn(2.0, "Scorer", "Blink", &twenty, 1, NULL) ||
			!Mode_Import("Items") || !Ob_KeyHint(-18893, -654, -25042, "Mission_Decontamination_SystemMessage5", 0)) {
			return 0;
		}
	}
	if (!PORT_LOWN(MapDecont, LoadBishopsScene, NULL, "") || !PORT_LOWN(MapDecont, LoadFunctionaryScene, NULL, "") ||
		!PORT_LOWN(MapDecont, LoadFallingScene, NULL, "") || !PORT_LOWN(MapDecont, CreateBishopsLauncher, NULL, "") ||
		!PORT_LOWN(MapDecont, SetWaypoint, NULL, "v", &dec)) {
		return 0;
	}
	if (d) {
		if (!Mode_EntSetS("DM_Door_OrbitDoorA_01", "OnObjSlot", "!FrontDoorSlot") ||
			!Mode_EntSetS("DM_Door_OrbitDoorA_02", "OnObjSlot", "!FrontDoorSlot")) {
			return 0;
		}
	} else {
		nativeVal_t tail = { 0 };
		Native_SetNone(&path);
		Native_SetNone(&file);
		LvN_Str(&tail, "/Map/DecoActive/DecoActive.m3d");
		byte ok = Ob_Keypad("DM_Keypad_Decontamination", "Decontamination_keyboard", &kb) && LvN_ModGet("Init", "Path", &path) &&
			Native_Arith('+', &path, &tail, &file) && Mode_VModel(&kb, &file) && LvN_SetI(&kb, "VRadius", 60) &&
			LvN_SetI(&kb, "ArrowCanUse", 1) && LvN_SetI(&kb, "Life", 0) && LvN_SetI(&kb, "ArrnOpt", 0) &&
			LvN_SetLangOpt(&kb, "ArrOpt0", "Decontamination:", "Mission_Decontamination_Hint1") &&
			LvN_SetS(&kb, "UseFunc", "MapDecont.Decontamination") && LvN_SetI(&kb, "MapCollision", 1) &&
			PORT_LOWN(MapDecont, LoadDecontamination, NULL, "") && Mode_EntSetS("DM_Door_OrbitDoorA_01", "OnObjSlot", "!FrontDoorSlot") &&
			Mode_EntSetS("DM_Door_OrbitDoorA_02", "OnObjSlot", "!CloseDoorSlot") &&
			Mode_Later(5.0, MD, "MsgOffVoice", NULL, 0, NULL);
		Native_Free(&kb);
		Native_Free(&path);
		Native_Free(&tail);
		Native_Free(&file);
		if (!ok) {
			return 0;
		}
	}
	if (!Mode_Import("GateKeeper")) {
		return 0;
	}
	/* GateKeeper.SetDoor with the door functions of the module (OBJLIB 5: the functions as values); the lookup of
	 * SetDoor comes after the two functions now (it differs only when both fail) */
	byte ok = LvN_GObj(MD, "OpenGateKeeperDoor", &fn1) && LvN_GObj(MD, "CloseGateKeeperDoor", &fn2) &&
		PORT_CALL(GateKeeper, SetDoor, NULL, "ssvv", "GateKeeper_Portalis", "DM_Door_OrbitDoorA_00", &fn1, &fn2);
	Native_Free(&fn1);
	Native_Free(&fn2);
	if (!ok || !Mode_EntSetI("DM_Door_Teleporter_01", "ArrnOpt", 0) ||
		!Mode_EntSetS("DM_Door_Teleporter_01", "UseFunc", "MapDecont.CantUseElevator") || !Mode_Import("Items")) {
		return 0;
	}
	/* Items.Hint(<the teleporter door>.Pos, the text, 5, 1, SetFirstInfoPointAsMission) (OBJLIB 5: the function as a
	 * value); the lookup of Hint comes after its arguments now (it differs only when it fails: the door and the
	 * function are read before its AttributeError then) */
	Native_SetNone(&pos);
	Native_SetNone(&fn1);
	ok = LvN_Entity("DM_Door_Teleporter_01", &e) && Mode_VGet(&e, "Pos", &pos);
	Native_Free(&e);
	if (ok) {
		Mode_Lang("Mission_Decontamination_SystemMessage6", &text);
		ok = LvN_GObj(MD, "SetFirstInfoPointAsMission", &fn1) &&
			PORT_CALL(Items, Hint, NULL, "vviiv", &pos, &text, 5L, 1L, &fn1);
		Native_Free(&text);
	}
	Native_Free(&pos);
	Native_Free(&fn1);
	return ok && Mode_EntSetS("DM_Door_OrbitDoorA_03", "OnObjSlot", "");
}

/* MsgOffVoice(): the voice of the decontamination every 20 s until it is done */
static byte Decont_MsgOffVoiceN(void) {
	nativeVal_t sv = { 0 };
	if (!Mode_Import("Speech")) {
		return 0;
	}
	Mode_SaveVar("Orbit.Decontaminated", &sv);
	byte no = Native_IsStr(&sv, "no");
	Native_Free(&sv);
	return !no || (PORT_CALL(Speech, OffVoice, NULL, "s", "Mission_Decontamination_RemoteMessage1") &&
		Mode_Later(20.0, MD, "MsgOffVoice", NULL, 0, NULL));
}

/* SetFirstInfoPointAsMission(itemname): the keypad becomes the target instead of the teleporter */
static byte Decont_SetFirstInfoPointAsMissionN(void) {
	return Ob_EntArrow("DM_Door_Teleporter_00", 0) && Ob_EntArrow("Decontamination_keyboard", 1);
}

/* CantUseElevator(UsrName, ElevatorName, optId, FullOption) */
static byte Decont_CantUseElevatorN(void) {
	return Ob_MsgKey("Mission_Decontamination_SystemMessage7", 5);
}

/* CloseGateKeeperDoor(Door, GateKeeperName): closed, a hint near the gatekeeper */
static byte Decont_CloseGateKeeperDoorN(const nativeVal_t *door) {
	return LvN_SetS(door, "OnObjSlot", "!CloseDoorSlot") && Mode_Import("Items") &&
		Ob_KeyHint(-16501, -652, -24098, "Mission_Decontamination_SystemMessage2", 1);
}

/* OpenGateKeeperDoor(Door, GateKeeperName): open, the teleporter becomes the target */
static byte Decont_OpenGateKeeperDoorN(const nativeVal_t *door) {
	nativeVal_t empty = { 0 }, when = { 0 }, args[2] = { 0 };
	LvN_Str(&empty, "");
	byte ok = LvN_SetS(door, "OnObjSlot", "!AutoDoorSlot") && Ob_EntArrow("GateKeeper_Portalis", 0) &&
		Ob_EntArrow("DM_Door_Teleporter_00", 1) && Ob_Msg(&empty, 0);
	Native_Free(&empty);
	if (!ok) {
		return 0;
	}
	Native_SetFloat(&when, Mode_Time() + 4.0);
	/* SScorer.SetMsgText as the object of the module (the scheduler takes its entry, Port_Scheduled) */
	nativeVal_t fn;
	if (!Mode_RefMod("SScorer", "SetMsgText", &fn)) {
		return 0;
	}
	Mode_Lang("Mission_Decontamination_SystemMessage4", &args[0]);
	Native_SetFloat(&args[1], Mode_Time() + 10.0);
	ok = Mode_ScheduleV(&when, &fn, args, 2, NULL);
	Native_Free(&fn);
	Native_Free(&args[0]);
	return ok;
}

/* AfterBrowseNPC(): the callbacks of the special characters (OBJLIB 5: the functions as values, by the names of
 * their refs) */
static byte Decont_AfterBrowseNPCN(void) {
	static const struct {
		const char *name;
		const portRef_t *ref;
	} pairs[] = { { "GateKeeper_Portalis", &PORT_LREF(MapDecont, CreateGateKeeperInit) },
		{ "Sentinel_Corridor", &PORT_LREF(MapDecont, CreateSentinelInit) }, { "Bishop_01", &PORT_LREF(MapDecont, CreateBishopInit) },
		{ "Bishop_00", &PORT_LREF(MapDecont, CreateBishopInit) }, { "Functionary_00", &PORT_LREF(MapDecont, CreateFunctionaryInit) } };
	if (!Mode_Import("CharsNPC")) {
		return 0;
	}
	for (int i = 0; i < 5; i++) {
		if (!LvN_CallbackRef(pairs[i].name, pairs[i].ref)) {
			return 0;
		}
	}
	return 1;
}

/* CreateFunctionaryInit(e) */
static const char *const functionary_vars[] = { "ArrowCanUse=1", "UseFunc=MapDecont.PlayFunctionary", "LLogicProbTalkWhenWalking=1.0",
	"LLogicKeepWorking=1", NULL };

/* CreateBishopInit(e) of MapDecont, CreateFixedInit(e) of GamblinDen's Map: the character stands where it is */
static const char *const fixed_vars[] = { "LLogicDefaultState=6", "LLogicDefaultTarget='", "LLogicState=6", "LLogicTarget='",
	"LLogicIdle=1", "LLogicIdle=0", NULL };

byte Level_FixedInitN(const nativeVal_t *e) {
	return Mode_SetAttrs(e, fixed_vars);
}

/* ShowSkipConvsMsg(): how to skip the talks */
static byte Decont_ShowSkipConvsMsgN(void) {
	return Ob_KeyMsg("Mission_Decontamination_SystemMessage8", "Use", 10);
}

/* Speech.OnEndOfSpeech = GateKeeper.EndOfSpeech (OBJLIB 5: a function of another port) */
static byte Ob_GateKeeperEnd(void) {
	nativeVal_t fn = { 0 };
	byte ok = LvN_ModGet("GateKeeper", "EndOfSpeech", &fn) && Mode_SetModVar("Speech", "OnEndOfSpeech", &fn);
	Native_Free(&fn);
	return ok;
}

/* GateKeeperSpeech(UsrName, CharName, optId, FullOption): the gatekeeper of the entrance and its door */
static byte Decont_GateKeeperSpeechN(const nativeVal_t *usrName, const nativeVal_t *charName, const nativeVal_t *opt) {
	static const char *const mods[] = { "Speech", "Chars", "Menu", "string", "GateKeeper", NULL };
	static const char *const answers[] = { "Mission_Decontamination_GateKeeper_1", "Mission_Decontamination_GateKeeper_2",
		"Mission_Decontamination_GateKeeper_3", "Mission_Decontamination_GateKeeper_4" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *usr = Mode_V(&t), *gc = Mode_V(&t), *dn = Mode_V(&t), *door = Mode_V(&t);
	nativeVal_t *cad = Mode_V(&t), *now = Mode_V(&t), *st = Mode_V(&t), *when = Mode_V(&t), *fn = Mode_V(&t);
	nativeVal_t *params = Mode_V(&t);
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_EntityH(charName, ch) || !Mode_EntityH(usrName, usr) || !LvN_Speakers(usrName, charName)) {
		return Mode_Done(&t, 0);
	}
	/* OBJLIB 5 and 7: GateKeeper.GateKeepers holds the gatekeeper objects; GateKeepers[CharName] by reference */
	nativeVal_t *gks = Mode_V(&t), *g = Mode_V(&t);
	if (!Mode_RefMod("GateKeeper", "GateKeepers", gks) || !Mode_RefSub(gks, charName, g)) {
		return Mode_Done(&t, 0);
	}
	Mode_RefPlain(g, gc);
	if (!Mode_VObj(gc, "Door", dn) || !Mode_EntityH(dn, door)) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (LvN_EqS(opt, "Talk")) {
		Mode_Lang("Mission_Decontamination_GateKeeper_InitMsg1", cad);
		Native_SetFloat(now, Mode_Time());
		ok = PORT_CALL(Speech, CalculateSpeechTime, st, "v", cad) && Native_Arith('+', now, st, when) &&
			LvN_GObj(MD, "ShowSkipConvsMsg", fn);
		ok = ok && Mode_ScheduleV(when, fn, NULL, 0, NULL) &&
			Mode_Spch("Mission_Decontamination_GateKeeper_InitMsg1", "Mission_Decontamination_GateKeeper_InitAns1", "TalkNPC") &&
			Mode_AnsLst(answers, 4) && PORT_LOWN(MapDecont, CheckShowNextHowto, NULL, "");
	} else if (LvN_EqS(opt, "Mission_Decontamination_GateKeeper_3")) {
		ok = Mode_Spch("Mission_Decontamination_GateKeeper_3Msg", "Mission_Decontamination_GateKeeper_3Ans", "TalkNoNPC") &&
			LvN_SetI(ch, "ArriOpt", 3);
	} else if (LvN_EqS(opt, "Mission_Decontamination_GateKeeper_4")) {
		ok = LvN_SetLangOpt(ch, "ArrOpt3", "AllreadyOpen:", "Mission_Decontamination_GateKeeper_4Opt") &&
			Mode_Spch("Mission_Decontamination_GateKeeper_4Msg", "Mission_Decontamination_GateKeeper_4Ans", "TalkNPC") &&
			Ob_GateKeeperEnd() && Mode_VObj(gc, "CanPassFunc", fn) && Mode_SetModVar("GateKeeper", "LocalCallFunc", fn);
		if (ok) {
			Native_SetEmpty(params, NV_TUPLE);
			Native_Copy(Native_Append(params), door);
			Native_Copy(Native_Append(params), charName);
			ok = Mode_SetModVar("GateKeeper", "LocalParams", params) && LvN_SetS(ch, "AbortSelectFunc", "GateKeeper.RestoreState");
		}
	} else if (LvN_EqS(opt, "AllreadyOpen")) {
		ok = Mode_Spch("Mission_Decontamination_GateKeeper_4Msg", "Generics_GateKeeper_InitAns1", "TalkNPC") &&
			Ob_GateKeeperEnd() && Mode_VObj(gc, "CanPassFunc", fn);
		if (ok) {
			/* gc.CanPassFunc(door, CharName): the function value of the gatekeeper (MapDecont.OpenGateKeeperDoor
			 * through its entry while it is the port's) */
			const nativeVal_t args[2] = { *door, *charName };
			ok = Mode_CallArgs(fn, NULL, args, 2);
		}
		ok = ok && LvN_SetS(ch, "AbortSelectFunc", "GateKeeper.RestoreState");
	} else {
		ok = PORT_CALL(Speech, StdOnSpch, NULL, "v", opt);
	}
	return Mode_Done(&t, ok);
}

/* SentinelSpeech(UsrName, CharName, optId, FullOption): the sentinel of the corridor; its second answer shows the
 * cop meter */
static byte Decont_SentinelSpeechN(const nativeVal_t *usrName, const nativeVal_t *charName, const nativeVal_t *opt) {
	static const char *const answers[] = { "Mission_Checkin_Sentinel_1", "Mission_Checkin_Sentinel_2" };
	if (!Mode_Import("Speech") || !LvN_Speakers(usrName, charName)) {
		return 0;
	}
	if (LvN_EqS(opt, "Talk")) {
		return Mode_Spch("Mission_Checkin_Sentinel_InitMsg1", "Mission_Checkin_Sentinel_InitAns1", NULL) && Mode_AnsLst(answers, 2);
	}
	if (LvN_EqS(opt, "Mission_Checkin_Sentinel_2")) {
		return PORT_CALL(Speech, StdOnSpch, NULL, "v", opt) && Mode_Later(8.0, MD, "GaugeBlink", NULL, 0, NULL);
	}
	return PORT_CALL(Speech, StdOnSpch, NULL, "v", opt);
}

/* GaugeBlink(): while the sentinel still talks, the alarm rises and the cop meter blinks */
static byte Decont_GaugeBlinkN(void) {
	nativeVal_t e = { 0 }, act = { 0 }, head = { 0 }, level = { 0 };
	if (!Mode_Import("Scorer")) {
		return 0;
	}
	Native_SetNone(&act);
	Native_SetNone(&head);
	byte ok = LvN_Entity("Sentinel_Corridor", &e) && Mode_VGet(&e, "Action", &act) && Native_Slice(&act, 0, 7, &head);
	if (ok && LvN_EqS(&head, "TalkNPC")) {
		Native_SetFloat(&level, 0.9);
		ok = Mode_SetAlarm(&level) && PORT_CALL(Scorer, Blink, NULL, "is", 30L, "CopMeter");
	}
	Native_Free(&e);
	Native_Free(&act);
	Native_Free(&head);
	return ok;
}

/* CheckShowNextHowto(): once the player has the gatekeeper selected with more than one option, how to pick the next */
static byte Decont_CheckShowNextHowtoN(void) {
	nativeVal_t e = { 0 }, n = { 0 }, one = { 0 }, usr = { 0 }, sel = { 0 }, aut = { 0 }, args[2] = { 0 };
	Native_SetNone(&n);
	byte ok = LvN_Entity("GateKeeper_Portalis", &e) && Mode_VGet(&e, "ArrnOpt", &n);
	Native_Free(&e);
	Native_SetInt(&one, 1);
	if (!ok || Native_Compare(&n, &one) <= 0) {
		Native_Free(&n);
		return ok;
	}
	Native_Free(&n);
	if (!Mode_Import("Scorer") || !Mode_UsrHandle(&usr)) {
		return 0;
	}
	if (Native_True(&usr)) {
		Native_SetNone(&sel);
		Native_SetNone(&aut);
		int go = Mode_VGet(&usr, "SelectedEntity", &sel) ? LvN_EqS(&sel, "GateKeeper_Portalis") : -1;
		if (go > 0) {
			go = Mode_VGet(&usr, "AutoSelect", &aut) ? Native_True(&aut) : -1;
		}
		Native_Free(&sel);
		Native_Free(&aut);
		if (go != 0) {
			Native_Free(&usr);
			if (go < 0) {
				return 0;
			}
			Native_SetInt(&args[0], 20);
			LvN_Str(&args[1], "Talk");
			ok = Ob_KeyMsg("Mission_Decontamination_SystemMessage3", "NextAction", 10) &&
				Mode_LaterIn(1.0, "Scorer", "Blink", args, 2, NULL);
			Native_Free(&args[1]);
			return ok;
		}
	}
	Native_Free(&usr);
	return Mode_Later(0.25, MD, "CheckShowNextHowto", NULL, 0, NULL);
}

/* CreateBishopsLauncher(): the item that starts the scene of the bishops */
static byte Decont_CreateBishopsLauncherN(void) {
	nativeVal_t name = { 0 }, x = { 0 }, y = { 0 }, z = { 0 }, l = { 0 };
	LvN_Str(&name, "BishopsLauncher");
	Native_SetInt(&x, -19800);
	Native_SetInt(&y, 1150);
	Native_SetInt(&z, -31100);
	byte ok = LvN_Create(&name, &x, &y, &z, "Item", &l) && LvN_SetI(&l, "Radius", 400) &&
		LvN_SetS(&l, "OnTake", "MapDecont.PlayBishops");
	Native_Free(&name);
	Native_Free(&l);
	return ok;
}

/* EndOfSceneBishops(usr): the player after the scene, the bishops talk, infinite lives, the back door closes */
static byte Decont_EndOfSceneBishopsN(const nativeVal_t *usr) {
	nativeVal_t one = { 0 }, door = { 0 };
	Native_SetInt(&one, 1);
	if (!Mode_Import("Chars") || !Ob_PlaceUsr(usr, -20423, 1046, -31145, 90) || !Mode_Import("Bishop") ||
		!Mode_SetModVar("Bishop", "skipChallengeOptions", &one) || !Mode_EntSetS("Bishop_01", "UseFunc", "Speech.AutoSpeech") ||
		!Mode_EntSetS("Bishop_00", "UseFunc", "Speech.AutoSpeech")) {
		return 0;
	}
	SaveVar_Set("Player.NumLives", "0");
	SaveVar_Set("Player.InfiniteLives", "1");
	byte ok = LvN_Entity("DM_Door_OrbitDoorA_03", &door) && LvN_SetS(&door, "ForceAction", "Close") &&
		LvN_SetS(&door, "ForceDefLoop", "Closed");
	Native_Free(&door);
	return ok;
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
static byte Decont_LoadBishopsSceneN(void) {
	return Mode_Import("Scene") && Mode_Import("MakeChar") && Mode_Calls(MD, bishops_scene) &&
		Ob_SceneFx(MD, "FXSceneBishopsFile", "FXSCENEBISHOPS");
}

/* FXBishopsPray(EntityName, EventName, Time): the effect of a bishop selling a life to Dtritus (the engine call
 * through the refs, fc-r6b-or1 records SetCallFunc, AddParams and Execute) */
static byte Decont_FXBishopsPrayN(const nativeVal_t *ev) {
	nativeVal_t pre = { 0 }, last = { 0 }, p = { 0 };
	Native_SetNone(&last);
	Native_SetNone(&p);
	LvN_Str(&pre, "BishopsBishop");
	byte ok = PORT_CALL(Scrap, SetCallFunc, NULL, "s", "!FXBishopSellLife") && LvN_Item(ev, -1, &last) &&
		Native_Arith('+', &pre, &last, &p) && PORT_CALL(Scrap, AddParams, NULL, "v", &p) &&
		PORT_CALL(Scrap, AddParams, NULL, "s", "BishopsDtritus") && PORT_CALL(Scrap, Execute, NULL, "");
	Native_Free(&pre);
	Native_Free(&last);
	Native_Free(&p);
	return ok;
}

/* PlayBishops(ItemName=None, CharName=None): the scene of the bishops, started a moment later (an item cannot hide
 * cars or characters) */
static byte Decont_PlayBishopsN(const nativeVal_t *item, const nativeVal_t *ch) {
	nativeVal_t none = { 0 }, usr = { 0 }, name = { 0 }, h = { 0 }, when = { 0 }, scene = { 0 };
	Native_SetNone(&none);
	int go = !Native_Compare(ch, &none);
	if (!go) {
		Native_SetNone(&name);
		go = Mode_UsrHandle(&usr) && Mode_VGet(&usr, "Name", &name) ? !Native_Compare(ch, &name) : -1;
		Native_Free(&usr);
		Native_Free(&name);
		if (go < 0) {
			return 0;
		}
	}
	if (!go) {
		return 1;
	}
	if (Native_True(item)) {
		byte ok = Mode_EntityH(item, &h) && Mode_VDel(&h);
		Native_Free(&h);
		if (!ok) {
			return 0;
		}
	}
	if (!Mode_Import("Scene") || !Mode_EntSetS("DM_Door_OrbitDoorA_03", "OnObjSlot", "") ||
		!LvN_ModSetGlobal("Scene", "EndOfSceneCallback", MD, "EndOfSceneBishops")) {
		return 0;
	}
	Native_SetFloat(&when, Mode_Time());
	/* Scene.Play as the object of the module: the scheduler takes its entry */
	nativeVal_t fn;
	if (!Mode_RefMod("Scene", "Play", &fn)) {
		return 0;
	}
	LvN_Str(&scene, "Bishops");
	byte ok = Mode_ScheduleV(&when, &fn, &scene, 1, NULL);
	Native_Free(&fn);
	Native_Free(&scene);
	return ok;
}

/* PlayBishops(ItemName=None, CharName=None) */
MODE_KW(Decont, PlayBishops, "|OO", "ItemName", "CharName") {
	return Decont_PlayBishopsN(&v[0], &v[1]);
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
static byte Decont_LoadFunctionarySceneN(void) {
	nativeVal_t df = { 0 };
	if (!Mode_Import("Scene") || !Mode_Import("MakeChar") || !Mode_Calls(MD, functionary_scene1)) {
		return 0;
	}
	byte ok = LvN_Entity("DM_DesktopFunctionary_00", &df) && LvN_SetS(&df, "ActCtrl", "BureaucracyDesktopFunctionary") &&
		LvN_SetI(&df, "MapCollision", 1) && Mode_Calls(MD, functionary_scene2);
	Native_Free(&df);
	return ok;
}

/* AddDF(frame, Action, DefLoop, EntityName="DM_DesktopFunctionary_00"): an event of the current action at the frame
 * that forces the animation of an entity */
static byte Decont_AddDFN(const nativeVal_t *frame, const nativeVal_t *act, const nativeVal_t *loop,
	const nativeVal_t *ent) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rep = Mode_V(&t), *key = Mode_V(&t), *at = Mode_V(&t), *comma = Mode_V(&t), *name = Mode_V(&t);
	Mode_Repr(frame, rep);
	LvN_Str(comma, ",");
	if (!Mode_Cat(&t, key, 2, Mode_S(&t, "TimeOfFrame"), rep) || !LvN_GetAct(Native_Text(key), at) ||
		!LvN_SetAct("AddEventAtTime", at) || !Mode_Cat(&t, name, 5, act, comma, loop, comma, ent) ||
		!LvN_SetAct("EventName", name)) {
		return Mode_Done(&t, 0);
	}
	LvN_Str(name, "MapDecont.SetDFAnm");
	return Mode_Done(&t, LvN_SetAct("EventFunc", name));
}

/* AddDF(frame, Action, DefLoop, EntityName="DM_DesktopFunctionary_00") */
MODE_KW(Decont, AddDF, "OOO|O", "frame", "Action", "DefLoop", "EntityName") {
	Mode_KwDefS(v, given, 3, "DM_DesktopFunctionary_00");
	return Decont_AddDFN(&v[0], &v[1], &v[2], &v[3]);
}

/* SetDFAnm(EntityName, EventName, Time): EventName is "action,loop,entity" */
static byte Decont_SetDFAnmN(const nativeVal_t *ev) {
	nativeVal_t values = { 0 }, name = { 0 }, df = { 0 }, v = { 0 };
	Native_SetNone(&values);
	Native_SetNone(&name);
	Native_SetNone(&df);
	Native_SetNone(&v);
	byte ok = Mode_Import("string") && Native_Split(ev, ",", &values) && LvN_Item(&values, 2, &name) &&
		Mode_EntityH(&name, &df) && LvN_Item(&values, 0, &v) && Mode_VSet(&df, "ForceAction", &v);
	Native_Free(&v);
	ok = ok && LvN_Item(&values, 1, &v) && Mode_VSet(&df, "ForceDefLoop", &v);
	Native_Free(&values);
	Native_Free(&name);
	Native_Free(&df);
	Native_Free(&v);
	return ok;
}

/* EndOfSceneFunctionary(usr): the functionary talks normally again, the keypad down to the town hall */
static byte Decont_EndOfSceneFunctionaryN(void) {
	nativeVal_t c = { 0 }, one = { 0 };
	if (!Mode_Import("Functionary") || !Mode_Import("SaveGame") || !LvN_Entity("Functionary_00", &c)) {
		return 0;
	}
	Native_SetInt(&one, 1);
	byte ok = Mode_SetModVar("Functionary", "skipChallengeOptions", &one) && LvN_SetS(&c, "UseFunc", "Speech.AutoSpeech") &&
		LvN_SetI(&c, "ArrowCanMission", 0) && LvN_SetI(&c, "RadarShow", 0) && Mode_GVarSetDef("FunctionaryMinTimeToSitDown") &&
		Mode_GVarSetDef("FunctionaryMaxTimeToSitDown") && PORT_LOWN(MapDecont, CreateDownKeyPad, NULL, "") &&
		Mode_MMDesc("Mission_Fake_MissionText1", 0L);
	Native_Free(&c);
	return ok;
}

/* CreateDownKeyPad(): the keypad of the trap panel, the target of the mission */
static byte Decont_CreateDownKeyPadN(void) {
	nativeVal_t kb = { 0 };
	byte ok = Ob_Keypad("DM_Keypad_CineDown", "DownPanel_keyboard", &kb) && LvN_SetS(&kb, "ActCtrl", "FunctionaryDownPanel") &&
		LvN_SetS(&kb, "Action", "Loop") && LvN_SetI(&kb, "VRadius", 60) && LvN_SetI(&kb, "ArrowCanUse", 1) &&
		LvN_SetI(&kb, "Life", 0) && LvN_SetI(&kb, "ArrnOpt", 0) && LvN_SetLangOpt(&kb, "ArrOpt0", "GoDown:", "TownHallGoto") &&
		LvN_SetS(&kb, "UseFunc", "MapDecont.PlayFalling") && LvN_SetI(&kb, "MapCollision", 1);
	if (ok) {
		/* ArrowCanMission, ArrowModMission, then RadarShow as the script sets them */
		nativeVal_t file = { 0 };
		Native_SetNone(&file);
		ok = LvN_SetI(&kb, "ArrowCanMission", 1) && Native_GVarGet("ArrowMissionFile", &file) &&
			Mode_VSet(&kb, "ArrowModMission", &file) && LvN_SetI(&kb, "RadarShow", 1);
		Native_Free(&file);
	}
	Native_Free(&kb);
	return ok;
}

/* PlayFalling(UsrName, CharName, optId, FullOption): the fall through the trap panel */
static byte Decont_PlayFallingN(void) {
	nativeVal_t e = { 0 };
	if (!Mode_Import("Scene") || !LvN_Entity("DownPanel_keyboard", &e)) {
		return 0;
	}
	byte ok = LvN_SetI(&e, "ArrowCanMission", 0) && LvN_SetI(&e, "ArrowCanUse", 0) &&
		Ob_Play(MD, "JumpNextMission", "Falling");
	Native_Free(&e);
	return ok;
}

/* JumpNextMission(usr): the game proper starts in the town hall (Levels/Fake picks the start) */
static byte Decont_JumpNextMissionN(void) {
	static const char *const mods[] = { "SaveGame", "Init", "MissionsFuncs", "Elevator", NULL };
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	SaveVar_Set("Challenge", "1");
	return PORT_CALL(SaveGame, MarkWayPoint, NULL, "ss", "Levels/Fake", "DM_Player_Spawn_Orbit") &&
		PORT_CALL(SaveGame, StartGame, NULL, "");
}

/* PlayFunctionary(UsrName, CharName, optId, FullOption): the scene of the check in */
static byte Decont_PlayFunctionaryN(void) {
	return Mode_Import("Scene") && Ob_Play(MD, "EndOfSceneFunctionary", "Functionary");
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
static byte Decont_LoadFallingSceneN(void) {
	return Mode_Import("Scene") && Mode_Import("MakeChar") && Mode_Calls(MD, falling_scene) &&
		Mode_EntSetS("DM_Door_OrbitDoorA_03", "OnObjSlot", "!BackDoorSlot");
}

MODE_FN(Decont, InitStatus, 1, Decont_InitStatusN())
MODE_FN(Decont, Decontamination, 4, Decont_DecontaminationN())
MODE_FN(Decont, EndOfScene, 1, Decont_EndOfSceneN(&a[0]))
MODE_FN(Decont, DecontaminatedWaypoint, 0, Decont_DecontaminatedWaypointN())
MODE_FN(Decont, SetWaypoint, 1, Decont_SetWaypointN(&a[0]))
MODE_FN(Decont, LoadDecontamination, 0, Decont_LoadDecontaminationN())
MODE_FN(Decont, Init, 1, Decont_InitN())
MODE_FN(Decont, MsgOffVoice, 0, Decont_MsgOffVoiceN())
MODE_FN(Decont, SetFirstInfoPointAsMission, 1, Decont_SetFirstInfoPointAsMissionN())
MODE_FN(Decont, CantUseElevator, 4, Decont_CantUseElevatorN())
MODE_FN(Decont, CloseGateKeeperDoor, 2, Decont_CloseGateKeeperDoorN(&a[0]))
MODE_FN(Decont, OpenGateKeeperDoor, 2, Decont_OpenGateKeeperDoorN(&a[0]))
MODE_FN(Decont, AfterBrowseNPC, 0, Decont_AfterBrowseNPCN())
MODE_FN(Decont, CreateFunctionaryInit, 1, Mode_SetAttrs(&a[0], functionary_vars))
MODE_FN(Decont, CreateBishopInit, 1, Level_FixedInitN(&a[0]))
MODE_FN(Decont, CreateGateKeeperInit, 1, LvN_SetS(&a[0], "UseFunc", "MapDecont.GateKeeperSpeech"))
MODE_FN(Decont, CreateSentinelInit, 1, LvN_SetS(&a[0], "UseFunc", "MapDecont.SentinelSpeech"))
MODE_FN(Decont, ShowSkipConvsMsg, 0, Decont_ShowSkipConvsMsgN())
MODE_FN(Decont, GateKeeperSpeech, 4, Decont_GateKeeperSpeechN(&a[0], &a[1], &a[2]))
MODE_FN(Decont, SentinelSpeech, 4, Decont_SentinelSpeechN(&a[0], &a[1], &a[2]))
MODE_FN(Decont, GaugeBlink, 0, Decont_GaugeBlinkN())
MODE_FN(Decont, CheckShowNextHowto, 0, Decont_CheckShowNextHowtoN())
MODE_FN(Decont, CreateBishopsLauncher, 0, Decont_CreateBishopsLauncherN())
MODE_FN(Decont, EndOfSceneBishops, 1, Decont_EndOfSceneBishopsN(&a[0]))
MODE_FN(Decont, LoadBishopsScene, 0, Decont_LoadBishopsSceneN())
MODE_FN(Decont, FXBishopsPray, 3, Decont_FXBishopsPrayN(&a[1]))
MODE_FN(Decont, LoadFunctionaryScene, 0, Decont_LoadFunctionarySceneN())
MODE_FN(Decont, SetDFAnm, 3, Decont_SetDFAnmN(&a[1]))
MODE_FN(Decont, EndOfSceneFunctionary, 1, Decont_EndOfSceneFunctionaryN())
MODE_FN(Decont, CreateDownKeyPad, 0, Decont_CreateDownKeyPadN())
MODE_FN(Decont, PlayFalling, 4, Decont_PlayFallingN())
MODE_FN(Decont, JumpNextMission, 1, Decont_JumpNextMissionN())
MODE_FN(Decont, PlayFunctionary, 4, Decont_PlayFunctionaryN())
MODE_FN(Decont, LoadFallingScene, 0, Decont_LoadFallingSceneN())

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
	{ "CreateBishopInit", Decont_CreateBishopInit, 1, NULL },
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
static byte Intro_InitStatusN(void) {
	return PORT_CALL(Chars, AddNewCharRes, NULL, "s", "Gear") && PORT_CALL(Chars, AddNewCharRes, NULL, "s", "Human");
}

/* the gatekeeper goes on (its time runs, active) */
static byte Ih_GateKeeper(void) {
	nativeVal_t gc = { 0 };
	byte ok = LvN_Entity("GateKeeper_Portalis", &gc) && LvN_SetF(&gc, "TimeSpeed", 1.0) && LvN_SetI(&gc, "Active", 1);
	Native_Free(&gc);
	return ok;
}

/* Init(LevelPath): the doors open, the alarm on and locked, the three scenes of Bill's arrival */
static byte Intro_InitN(void) {
	nativeVal_t one = { 0 };
	for (int i = 0; i < 5; i++) {
		char name[32];
		nativeVal_t d = { 0 };
		Str_Printf(name, sizeof(name), "DM_Door_OrbitDoorA_0%d", i);
		byte ok = LvN_Entity(name, &d) && LvN_SetS(&d, "OnObjSlot", "") && LvN_SetS(&d, "Action", "Opened");
		Native_Free(&d);
		if (!ok) {
			return 0;
		}
	}
	Native_SetInt(&one, 1);
	return Mode_SetAlarm(&one) && Mode_SetLockAlarm(&one) && PORT_LOWN(Mission_IntroHuman, LoadSceneA, NULL, "") &&
		PORT_LOWN(Mission_IntroHuman, LoadSceneB, NULL, "") && PORT_LOWN(Mission_IntroHuman, LoadSceneC, NULL, "") &&
		Ob_PlayThen("BillArrives_A", IH, "BillArrives_B") && Ih_GateKeeper();
}

/* SetNextMission(usr=None): on to Mission_IntroHuman2 in Press, Bill's ship parked there */
static byte Intro_SetNextMissionN(void) {
	static const char *const saves[][2] = { { "BankMasterAtBank", "1" }, { "Journalist.Humphrey_Defaut", "Press" },
		{ "Journalist.Betty_Defaut", "Press" }, { "Journalist.Berto_Defaut", "Press" } };
	nativeVal_t zero = { 0 }, l = { 0 }, item = { 0 };
	Native_SetInt(&zero, 0);
	/* Achievement through its ref (the cases record it) */
	if (!Mode_Import("MissionsFuncs") || !Mode_Import("SaveGame") || !PORT_CALL(Scrap, Achievement, NULL, "i", 5L) ||
		!Mode_SetLockAlarm(&zero) || !Mode_SetAlarm(&zero) ||
		!PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_IntroHuman2", "Press")) {
		return 0;
	}
	SaveVar_Set("Char", "Dtritus");
	if (!PORT_CALL(SaveGame, MarkWayPoint, NULL, "sss", "Levels/Press", "DM_Player_Spawn_Press", "")) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		SaveVar_Set(saves[i][0], saves[i][1]);
	}
	LvN_Str(&item, "SHUMP1");
	byte ok = LvN_SaveList("Parked.Press", IH, &l) && LvN_SaveAppend("Parked.Press", &l, &item) &&
		PORT_CALL(SaveGame, StartGame, NULL, "");
	Native_Free(&l);
	Native_Free(&item);
	return ok;
}

/* a function (usr=None) of the scene chain (MODE_KW) */
#define IH_USR_FN(name, call) \
	MODE_KW(Intro, name, "|O", "usr") { \
		return call; \
	}

/* BillArrives_B(usr=None), BillArrives_C(usr=None): the next scene */
IH_USR_FN(BillArrives_B, Ob_PlayThen("BillArrives_B", IH, "BillArrives_C") && Ih_GateKeeper())
IH_USR_FN(BillArrives_C, Ob_PlayThen("BillArrives_C", IH, "SetNextMission"))
IH_USR_FN(SetNextMission, Intro_SetNextMissionN())

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
MODE_FN(Intro, InitStatus, 1, Intro_InitStatusN())
MODE_FN(Intro, Init, 1, Intro_InitN())
MODE_FN(Intro, LoadSceneA, 0,
	Mode_Calls(IH, intro_scene_a) && Ob_SceneFx(IH, "FXSceneBillArrives_AFile", "FXSCENEBILLARRIVES_A"))
MODE_FN(Intro, LoadSceneB, 0, Mode_Calls(IH, intro_scene_b))
MODE_FN(Intro, LoadSceneC, 0,
	Mode_Calls(IH, intro_scene_c) && Ob_SceneFx(IH, "FXSceneDecontaminationFile", "FXSCENEDECONTAMINATION"))

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

PORTS(levels_Orbit)
