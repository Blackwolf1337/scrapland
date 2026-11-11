/*
 * The scripts of the level Police in C (data01.packed levels/police/scripts, issue #2): the Map module (the NPCs,
 * Ficus' body, the teleporters and gatekeepers, the fake traffic, the costume of the viscoson, the return from the
 * jail), MapSnd.SndElements (the sounds of the map elements; the rest of MapSnd is data), MapPlanes (the police
 * blueprints to photograph) and the missions Mission_BeatPickus, Mission_FicusDead, Mission_GetAuthorization,
 * Mission_InitEpilogue, Mission_JailBreak and Mission_PoliceBossCheats. The plain functions are data
 * (data/levels/police/<module>.def): MapPlanes.InitStatus, the scene loaders (Mission_BeatPickus.LoadScene,
 * Mission_GetAuthorization.LoadScene, Mission_PoliceBossCheats.LoadPoliFightScene) and the functions that only call
 * others (PreHireThugs, PreBeatPickus, OnAlarmDeactivated, PreTalkToTraitoriusAgain and SetNextMission of
 * Mission_BeatPickus, InitStatus, AfterDtritusSpeechShown and PreAfterDeepThroatMessageShown of Mission_FicusDead,
 * GetAuthorization2, PreFreeHuman and OnMainCharDeath of Mission_GetAuthorization,
 * Mission_InitEpilogue.PreSetNextMission, Mission_PoliceBossCheats.InitBossCheats).
 *
 * The module variables and the module code stay in the scripts: Map (Libraries, ficus, the discardable characters,
 * the jail escape at its load), MapPlanes (Naves, Motores), Mission_BeatPickus (followPlayerVel, thugsStopped,
 * launcher, plansNames, numPlansWithPhoto, alreadyTalked, FXSceneBeatPickusFile, traitorius), Mission_FicusDead
 * (ficus, numCrazyGamblerComs), Mission_GetAuthorization (maxHumanDist, minHumanDist, filledByUser, scenePlayed,
 * alarmFallDelta, human, conv, dd), Mission_JailBreak (PuffTurn, PuffWalk, ActPress), Mission_PoliceBossCheats
 * (NextMissionData, SpawnConversionPoint, the poker players PoliceBoss, Dumb1, Dumb2 and their cards PoliceBossc,
 * Dumb1c, Dumb2c).
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values, read the globals of their module where the
 * script reads them and write them back at once (OBJLIB 5), and call the engine in C: save variables, engine
 * variables, the dummy search, the language texts, the time speed and the action set. The engine functions the cases
 * record or swap go through their seams (AddScheduledFunc, DeleteScheduledFuncs, CreateEntity, UsrEntity, Set,
 * SetAlarm, GetAlarm, ConsoleError, DelSaveVars, SScorer.Fade, the SAct functions; OBJLIB 7). The entities the script
 * holds are handle values (OBJLIB 7). SAI.SetStateChar and GetStateChar, SLogic.SetOnFloor, the call of the map
 * elements (Scrap.SetCallFunc and its parameters), SAct.GetCls and SScorer.SetSpeechText go through their refs
 * (stage 7); CharsNPC.CharRes and the GateKeeper groups are
 * the objects they are (OBJLIB 5).
 *
 * Direct calls (stage 3): the functions of the other ports (Speech, MissionsFuncs, SaveGame, CharsNPC, Chars,
 * CharConversor, Elevator, Teleport, Metro, InTraffic, GateKeeper, Scene, RaceMaker, Menu, Init), the keyword call of
 * Speech.OnObjSpch and the own functions of the modules (their globals) go through their refs (PORT_CALL, PORT_LOWN,
 * LV_COM, LV_ONEND, LV_CALLBACK, LV_MISSIONCHAR; the seam takes the call by name when a mission or a case has put
 * something else there, OBJLIB 5). The GateKeeper refs take Python objects and always go by name. By name: the
 * sections of the .def files (LoadScene, GetAuthorization2, LoadPoliFightScene, InitBossCheats, the Pre and
 * SetNextMission sections of Mission_BeatPickus, MapSnd.PoliceCheats and PoliceBossCheats; OBJLIB 6); apply on
 * MissionsFuncs.SetNextMission takes its entry (Mode_ApplyNext); the functions given to the scheduler and to the
 * entities (Mode_Later, the UseFunc texts) take the entry of a port's own function (stage 4: Port_Scheduled,
 * PYF_ENTRY; SScorer.Fade its engine entry, stage 7), the ones given to Scene, Alarm and CharConversor as callbacks
 * (PO_HOOK) are values of their variables (OBJLIB 5). MapSnd.SndElements and MapPlanes are LevelCommon's.
 */
#include "scrap/scrap.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL police

#define MAP "Map"
#define MP "MapPlanes"
#define BP "Mission_BeatPickus"
#define FD "Mission_FicusDead"
#define GA "Mission_GetAuthorization"
#define IE "Mission_InitEpilogue"
#define JB "Mission_JailBreak"
#define PC "Mission_PoliceBossCheats"

/* SAI.SetStateChar(...) through its ref (the seam sees the recorder of a case) */
#define CHAR_STATE(...) PORT_CALL(SAI, SetStateChar, NULL, __VA_ARGS__)

/* ---- native helpers of this file ---- */

/* MissionsFuncs.SetMissionTarget(None) */
static byte Po_TargetNone(void) {
	return Mode_Target(NULL);
}

/* h.ArrOpt0 = "Talk:" + Scrap.GetLangStr("TalkOpt") */
static byte Po_TalkOpt(const nativeVal_t *h) {
	nativeVal_t a, b, r;
	LvN_Str(&a, "Talk:");
	Mode_Lang("TalkOpt", &b);
	Native_SetNone(&r);
	byte ok = Native_Arith('+', &a, &b, &r) && Mode_VSet(h, "ArrOpt0", &r);
	Native_Free(&a);
	Native_Free(&b);
	Native_Free(&r);
	return ok;
}

/* the entity "<prefix><i>" (`prefix + `i``) as a handle value */
static byte Po_EntI(const char *prefix, long i, nativeVal_t *out) {
	char s[0x40];
	Str_Printf(s, sizeof(s), "%s%ld", prefix, i);
	return LvN_Entity(s, out);
}

/* the entity of the player's name: Scrap.GetEntity(UsrName).ActCtrl == ctrl: 1, 0, -1 */
static int Po_NameIs(const nativeVal_t *usrName, const char *ctrl) {
	nativeVal_t e, a;
	Native_SetNone(&e);
	Native_SetNone(&a);
	int r = Mode_EntityH(usrName, &e) && Mode_VGet(&e, "ActCtrl", &a) ? LvN_EqS(&a, ctrl) : -1;
	Native_Free(&e);
	Native_Free(&a);
	return r;
}

/* `usr and usr.ActCtrl == ctrl` of the player (usr into *usr): 1, 0, -1 */
static int Po_UsrCtrl(nativeVal_t *usr, const char *ctrl) {
	if (!Mode_UsrHandle(usr)) {
		return -1;
	}
	return Native_True(usr) ? LvN_CtrlIs(usr, ctrl) : 0;
}

/* an item at pos (Scrap.CreateEntity(name, pos[0], pos[1], pos[2], "Item") through its seam) with Radius, OnlyUser (-1:
 * not set) and OnTake */
static byte Po_Item(const char *name, const nativeVal_t *pos, long radius, long onlyUser, const char *onTake, nativeVal_t *out) {
	nativeVal_t n;
	LvN_Str(&n, name);
	byte ok = LvN_CreateAt(&n, pos, "Item", out) && LvN_SetI(out, "Radius", radius) && (onlyUser < 0 || LvN_SetI(out, "OnlyUser", onlyUser)) &&
		LvN_SetS(out, "OnTake", onTake);
	Native_Free(&n);
	return ok;
}

/* SScorer.Fade(0, from, to, time) through its seam */
static byte Po_Fade(const int *from, const int *to, double time) {
	return Mode_Fade(0, from, to, time);
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, SScorer.Fade, (0, from, to, time)) (the function of SScorer
 * as the module holds it: the scheduler takes its entry, Port_Scheduled) */
static byte Po_FadeLater(double delay, const int *from, const int *to, double time) {
	nativeVal_t args[4];
	Native_SetInt(&args[0], 0);
	Native_SetEmpty(&args[1], NV_TUPLE);
	Native_SetEmpty(&args[2], NV_TUPLE);
	for (int i = 0; i < 4; i++) {
		Native_SetInt(Native_Append(&args[1]), from[i]);
		Native_SetInt(Native_Append(&args[2]), to[i]);
	}
	Native_SetFloat(&args[3], time);
	byte ok = Mode_LaterIn(delay, "SScorer", "Fade", args, 4, NULL);
	Native_Free(&args[1]);
	Native_Free(&args[2]);
	return ok;
}

/* Speech.AddNI(msg, ans) of the pairs (they end with NULL) */
static byte Po_AddNI(const char *const *pairs) {
	for (; *pairs; pairs += 2) {
		if (!PORT_CALL(Speech, AddNI, NULL, "ss", pairs[0], pairs[1])) {
			return 0;
		}
	}
	return 1;
}

/* Speech.NISpeech(who); the AddNI pairs; Speech.RunNI() */
static byte Po_NI(const char *who, const char *const *pairs) {
	return PORT_CALL(Speech, NISpeech, NULL, "s", who) && Po_AddNI(pairs) && PORT_CALL(Speech, RunNI, NULL, "");
}

/* CharsNPC.GenericNPCSpeech(UsrName, CharName, optId, FullOption) */
static byte Po_Generic(const nativeVal_t *a) {
	return PORT_CALL(CharsNPC, GenericNPCSpeech, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]);
}

/* the scheduled call of a global function of module after delay s without arguments, and with one */
#define PO_LATER(s, module, func) Mode_Later(s, module, func, NULL, 0, NULL)
#define PO_LATER1(s, module, func, v) Mode_Later(s, module, func, v, 1, NULL)

/* module.var = the global name of src (a function) and = None (OBJLIB 5) */
#define PO_HOOK(module, var, src, name) LvN_ModSetGlobal(module, var, src, name)

/* the entity of a C name with h.var = "text", = i (Scrap.GetEntity(name).var = ...: the error of None when it is not
 * there) */
#define PO_ENTS(name, var, s) Mode_EntSetS(name, var, s)
#define PO_ENTI(name, var, i) Mode_EntSetI(name, var, i)

/* ---- Map ---- */

/* CreateFicusDeadClass(): the class of Ficus' body; dead and still, or in his loop in Mission_FicusDead */
static byte PoMap_CreateFicusDeadClass(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	byte fd = LvN_SaveIs("Mission.Library", "Mission_FicusDead");
	if (!Mode_Import("Init") || !Mode_SAct("CreateClass", "FicusDead") || !Mode_SActSet(1, "ModelFilePath", "Models/Chars/Police/Police.m3d", 0) ||
		!Mode_SAct("CreateAction", "FicusDead") || !Mode_SActSet(0, "AnmFilePath", "Models/Chars/Police/Anm/FicusDead.M3D", 0) || !Mode_SActSet(0, "isLoop", NULL, 0)) {
		return 0;
	}
	if (!fd) {
		return Mode_SActSet(1, "DefAction", "FicusDead", 0);
	}
	SaveVar_Set("Police.FicusDeath", "1");
	return Mode_SAct("CreateAction", "FicusLoop") && Mode_SActSet(0, "AnmFilePath", "Models/Chars/Police/Anm/FicusLoop.M3D", 0) && Mode_SActSet(0, "isLoop", NULL, 1) &&
		Mode_SActSet(1, "DefAction", "FicusLoop", 0);
}

/* CreateFicus(): Ficus' body at its dummy (the module variable ficus) */
static byte PoMap_CreateFicus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t dummy, e;
	(void)a;
	(void)out;
	LvN_Dummy("DM_Mission_FicusDead", 1, &dummy);
	if (dummy.type == NV_NONE) {
		return 1;
	}
	Native_SetNone(&e);
	byte ok = Mode_FromDummy(&dummy, "WithLife", &e) && Mode_Put(MAP, "ficus", NULL, &e) &&
		LvN_SetS(&e, "ActCtrl", "FicusDead") && LvN_SetI(&e, "ActiveCMask", 0) && LvN_SetI(&e, "MapCollision", 1) &&
		LvN_SetI(&e, "UseAngSel", 1) && LvN_SetI(&e, "UseAbsAngSel", 0) && LvN_SetI(&e, "VRadius", 150) && LvN_SetI(&e, "UseMapCheck", 0);
	Native_Free(&dummy);
	Native_Free(&e);
	return ok;
}

/* InitStatus(LevelPath): an indoor map next to Outskirts; the police boss, the elevators, the NPCs that come back */
static byte PoMap_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Init", "CharsNPC", "Elevator", "InTraffic" };
	static const char *const defs[][2] = { { "Metro", "DM_Door_Metro_Press" }, { "MetroExt", "DM_Door_Metro_Press" },
		{ "Elevator", "DM_Door_Elevator_Outskirts" } };
	static const struct {
		const char *var, *name;
		long n;
	} repos[] = { { NULL, "Sentinel", 4 }, { NULL, "Police", 6 }, { NULL, "Nurse", 2 }, { NULL, "Desktop", 2 }, { "_BankDirector", "BankDirector", 2 },
		{ "_Bishop", "Bishop", 2 }, { NULL, "Functionary", 3 } };
	nativeVal_t v, r;
	(void)a;
	(void)out;
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	Native_SetInt(&v, 0);
	LvN_Str(&r, "Outskirts");
	byte ok = Mode_SetModVar("Init", "Outdoor", &v) && Mode_SetModVar("Init", "OutArea", &r);
	Native_Free(&r);
	if (!ok) {
		return 0;
	}
	if (LvN_SaveIs("PoliceBossAtPolice", "1")) {
		Native_SetNone(&r);
		int m = PORT_CALL(Init, isMission, &r, "") ? Native_True(&r) : -1;
		Native_Free(&r);
		if (m < 0 || (!m && !PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", "PoliceBoss_Default"))) {
			return 0;
		}
	}
	for (int i = 0; i < 3; i++) {
		if (!PORT_CALL(Elevator, SetDef, NULL, "ss", defs[i][0], defs[i][1])) {
			return 0;
		}
	}
	for (int i = 0; i < 7; i++) {
		int go = repos[i].var ? LvN_PcOr(MAP, repos[i].var) : 1;
		if (go < 0 || (go && !PORT_CALL(CharsNPC, SetReposChars, NULL, "si", repos[i].name, repos[i].n))) {
			return 0;
		}
	}
	int m = LvN_PcOr(MAP, "_Messenger");
	if (m > 0) {
		return PORT_CALL(CharsNPC, SetReposChars, NULL, "si", "Messenger", 2L);
	}
	Native_SetInt(&v, 0);
	return m == 0 && Mode_Import("Messenger") && Mode_SetModVar("Messenger", "MakeMDP", &v);
}

/* Init(LevelPath): Ficus' body, the teleporters, the gatekeepers of the security doors, the fake traffic, the costume
 * of the viscoson, the fade after the escape from the jail */
static byte PoMap_Init(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Elevator", "Teleport", "Metro", "InTraffic" };
	static const char *const light[] = { "TLight1B", "TLight2B", "TLight3B", NULL };
	nativeVal_t e, v;
	(void)a;
	(void)out;
	if (!PORT_LOWN(Map, CreateFicusDeadClass, NULL, "") || !PORT_LOWN(Map, CreateFicus, NULL, "")) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	if (!PORT_CALL(Teleport, Link, NULL, "ss", "DM_Door_Teleporter_01", "DM_Door_Teleporter_05") ||
		!PORT_CALL(Teleport, Link, NULL, "ss", "DM_Door_Teleporter_06", "DM_Door_Teleporter_07") ||
		!Mode_Connect("DM_Door_Elevator_Outskirts", "Outskirts", 0) || !PORT_CALL(Metro, SetAsMetro, NULL, "s", "DM_Door_Metro_Press") ||
		!Mode_Import("GateKeeper") || !PORT_CALL(GateKeeper, SetDoor, NULL, "ss", "GateKeeper_07", "DM_Door_Teleporter_07") ||
		!Mode_DoorFoe("GateKeeper_07", LvN_SaveIs("Police.FicusDeath", "1") ? "PoliceGroupGateKeeper" : "NoOneGroupGateKeeper") ||
		!PO_ENTS("DM_Door_Teleporter_07", "UseFunc", "GateKeeper.UseDoor")) {
		return 0;
	}
	Native_SetNone(&e);
	byte ok = LvN_Entity("DM_Door_PoliceSegurity_00", &e) && (!Native_True(&e) || (LvN_SetS(&e, "Action", "Closed") &&
		LvN_SetS(&e, "DefLoop", "Closed") && LvN_SetS(&e, "OnObjSlot", "") && Mode_DoorFoe("GateKeeper_08", "NoOneGroupGateKeeper")));
	Native_Free(&e);
	if (!ok) {
		return 0;
	}
	Mode_Texts(&v, light, -1);
	ok = Mode_SetModVar("InTraffic", "N_TrafficList", &v);
	Native_Free(&v);
	Native_SetInt(&v, 25);
	if (!ok || !Mode_SetModVar("InTraffic", "N_NumTraffic", &v) || !PORT_CALL(InTraffic, LoadFakeTraffic, NULL, "") ||
		!PORT_CALL(InTraffic, FakeTraffic, NULL, "")) {
		return 0;
	}
	if (LvN_SaveIs("CostumeAtPolice", "1")) {
		nativeVal_t dummy, costume, model;
		LvN_Dummy("DM_Mission_Viscoson_00", 1, &dummy);
		if (dummy.type != NV_NONE) {
			Native_SetNone(&costume);
			LvN_Str(&model, "Models/Misc/Viscoson/Viscoson.m3d");
			ok = Mode_FromDummy(&dummy, "WithLife", &costume) && Mode_VModel(&costume, &model) && LvN_SetI(&costume, "ActiveCMask", 0) &&
				LvN_SetI(&costume, "MapCollision", 1);
			Native_Free(&costume);
			Native_Free(&model);
		}
		Native_Free(&dummy);
		if (!ok) {
			return 0;
		}
	}
	if (!LvN_SaveIs("Spawn", "DM_Mission_JailBreak_End") || !LvN_SaveIs("ComeFrom", "")) {
		return 1;
	}
	return Po_Fade(lv_black, lv_black, 1.0) && Po_FadeLater(0.1, lv_black, lv_clear, 1.0);
}

LV_FN(PoMap, CreateFicusDeadClassA, 0, PoMap_CreateFicusDeadClass)
LV_FN(PoMap, CreateFicusA, 0, PoMap_CreateFicus)
LV_FN(PoMap, InitStatusA, 1, PoMap_InitStatus)
LV_FN(PoMap, InitA, 1, PoMap_Init)

PyMethodDef g_PoliceMapMethods[] = {
	{ "CreateFicusDeadClass", PoMap_CreateFicusDeadClassA, METH_VARARGS, NULL },
	{ "CreateFicus", PoMap_CreateFicusA, METH_VARARGS, NULL },
	{ "InitStatus", PoMap_InitStatusA, METH_VARARGS, NULL },
	{ "Init", PoMap_InitA, METH_VARARGS, NULL },
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
MODE_FN(PoliceMapSnd, SndElements, 6, LvN_SndElements(a, police_snd))

PyMethodDef g_PoliceMapSndMethods[] = {
	{ "SndElements", PoliceMapSnd_SndElements, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapPlanes ---- */

/* Init(LevelPath), TakenPhoto(Name): the police blueprints (levels/LevelCommon.c, shared with Press) */
MODE_FN(MapPlanes, Init, 1, LvN_PlanesInit(MP))

MODE_FN(MapPlanes, TakenPhoto, 1, LvN_PlanesPhoto(&a[0], MP, "PoliceBlueprints.Ships", "PoliceBlueprints.Engines", out))

PyMethodDef g_PoliceMapPlanesMethods[] = {
	{ "Init", MapPlanes_Init, METH_VARARGS, NULL },
	{ "TakenPhoto", MapPlanes_TakenPhoto, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_BeatPickus ---- */

/* the entity Police_Thug<i> */
static byte Bp_Thug(long i, nativeVal_t *out) {
	return Po_EntI("Police_Thug", i, out);
}

/* InitStatus(LevelPath): the five police thugs (unless the plans are next) and Traitorius */
static byte BeatPickus_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t state, plans;
	(void)a;
	(void)out;
	Mode_SaveVar("MissionState", &state);
	LvN_Str(&plans, "GetFuncShipPlans");
	int thugs = Mode_Cmp(&state, "!=", &plans);
	Native_Free(&state);
	Native_Free(&plans);
	for (int i = 1; thugs > 0 && i < 6; i++) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "Police_Thug%d", i);
		if (!LV_MISSIONCHAR(name, Mission_BeatPickus, OnThugRespawn)) {
			return 0;
		}
	}
	return thugs >= 0 && LV_MISSIONCHAR("Functionary_Traitorius", Mission_BeatPickus, OnTraitoriusRespawn);
}

/* Init(LevelPath): Pickus and the thugs wait, Traitorius talks, the scene and Pickus beaten, the state of the mission */
static byte BeatPickus_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t state, s1, s2, tr;
	(void)a;
	(void)out;
	if (!Mode_GVarSetI("LittlePoliceAsksForMoney", 0) || !Mode_GVarSetS("NotCleanableByGear", "Police")) {
		return 0;
	}
	Mode_SaveVar("MissionState", &state);
	LvN_Str(&s1, "GetFuncShipPlans");
	LvN_Str(&s2, "TalkToTraitoriusAgain");
	int early = Mode_Cmp(&state, "!=", &s1);
	if (early > 0) {
		early = Mode_Cmp(&state, "!=", &s2);
	}
	Native_Free(&s1);
	Native_Free(&s2);
	Native_SetNone(&tr);
	/* OBJLIB 6: LoadScene is a section of mission_beatpickus.def */
	byte ok = early >= 0 && (!early || (PORT_LOWN(Mission_BeatPickus, DisableIAFunctionary, NULL, "s", "Pickus") &&
		PORT_LOWN(Mission_BeatPickus, SetThugsState, NULL, ""))) && PORT_LOWN(Mission_BeatPickus, DisableIAFunctionary, &tr, "s", "Traitorius") &&
		Mode_Put(BP, "traitorius", NULL, &tr) && LV_OWN(BP, "LoadScene", NULL, "") && PORT_LOWN(Mission_BeatPickus, CreatePickusBeatenClass, NULL, "");
	Native_Free(&tr);
	if (ok) {
		if (LvN_EqS(&state, "TalkToTraitorius")) {
			ok = LvN_TargetG(BP, "traitorius");
		} else if (LvN_EqS(&state, "HireThugs") || LvN_EqS(&state, "BeatPickus")) {
			ok = PORT_LOWN(Mission_BeatPickus, HireThugs, NULL, "");
		} else if (LvN_EqS(&state, "GetFuncShipPlans")) {
			ok = PORT_LOWN(Mission_BeatPickus, GetFuncShipPlans, NULL, "");
		} else if (LvN_EqS(&state, "TalkToTraitoriusAgain")) {
			ok = PORT_LOWN(Mission_BeatPickus, TalkToTraitoriusAgain, NULL, "");
		}
	}
	Native_Free(&state);
	return ok;
}

/* DisableIAFunctionary(name): Functionary_<name> stays and talks (Traitorius through TraitoriusSpeech); the entity,
 * None when it is not there */
static byte BeatPickus_DisableIAFunctionary(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t name, key, desc;
	byte tr = LvN_EqS(&a[0], "Traitorius");
	Native_SetNone(&name);
	if (!LvN_Cat("Functionary_", &a[0], &name) || !Mode_EntityH(&name, out)) {
		Native_Free(&name);
		return 0;
	}
	Native_Free(&name);
	if (!Native_True(out)) {
		Native_Free(out);
		return 1;
	}
	Native_SetNone(&key);
	Native_SetNone(&desc);
	byte ok = (!tr || LvN_SetS(out, "UseFunc", "Mission_BeatPickus.TraitoriusSpeech")) && LvN_SetI(out, "LLogicDefaultState", 6) &&
		LvN_SetS(out, "LLogicDefaultTarget", "") && LvN_SetI(out, "LLogicState", 6) && LvN_SetS(out, "LLogicTarget", "") &&
		LvN_SetF(out, "LLogicProbTalkWhenWalking", 1.0) && LvN_SetI(out, "LLogicIdle", 1) && LvN_SetI(out, "LLogicIdle", 0) &&
		LvN_Cat("CharName_", &a[0], &key) && Mode_LangV(&key, &desc) && Mode_VSet(out, "Descriptor", &desc);
	Native_Free(&key);
	Native_Free(&desc);
	return ok;
}

/* OnTraitoriusRespawn(tr): Traitorius back: his name and his talk for the state of the mission */
static byte BeatPickus_OnTraitoriusRespawn(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t tr, state;
	Native_SetNone(&tr);
	(void)out;
	if (!Mode_Put(BP, "traitorius", NULL, &a[0]) || !Mode_G(BP, "traitorius", &tr)) {
		return 0;
	}
	byte ok = LvN_SetLang(&tr, "Descriptor", "CharName_Traitorius");
	Native_Free(&tr);
	if (!ok) {
		return 0;
	}
	Mode_SaveVar("MissionState", &state);
	if (LvN_EqS(&state, "TalkToTraitorius")) {
		ok = LvN_TargetG(BP, "traitorius") && Mode_GSetS(BP, "traitorius", "UseFunc", "Mission_BeatPickus.TraitoriusSpeech");
	} else if (LvN_EqS(&state, "TalkToTraitoriusAgain")) {
		ok = PORT_LOWN(Mission_BeatPickus, TalkToTraitoriusAgain, NULL, "");
	} else {
		ok = Mode_GSetS(BP, "traitorius", "UseFunc", "Mission_BeatPickus.TraitoriusSpeech");
	}
	Native_Free(&state);
	return ok;
}

/* OnThugRespawn(thug): a thug back: he follows the player during the beating, or the thugs regroup */
static byte BeatPickus_OnThugRespawn(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, vel, un;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&vel);
	Native_SetNone(&un);
	byte ok = Mode_UsrHandle(&usr);
	if (ok && Native_True(&usr)) {
		if (LvN_SaveIs("MissionState", "BeatPickus")) {
			ok = Mode_G(BP, "followPlayerVel", &vel) && Mode_GVarSet("GenCharSpecialChaseSpeed", &vel) && Mode_VGet(&usr, "Name", &un) &&
				PORT_LOWN(Mission_BeatPickus, SetState, NULL, "viv", &a[0], 5L, &un);
		} else if (LvN_SaveIs("MissionState", "HireThugs")) {
			ok = PORT_LOWN(Mission_BeatPickus, SetThugsState, NULL, "");
		}
	}
	Native_Free(&usr);
	Native_Free(&vel);
	Native_Free(&un);
	return ok;
}

/* SetState(thug, state, targetName) of the module through its global, with an int state */
static byte Bp_State(const nativeVal_t *thug, long state, const nativeVal_t *target) {
	return PORT_LOWN(Mission_BeatPickus, SetState, NULL, "viv", thug, state, target);
}

/* SetState(thug, 5, h.Name) */
static byte Bp_Follow(const nativeVal_t *thug, const nativeVal_t *h) {
	nativeVal_t n;
	Native_SetNone(&n);
	byte ok = Mode_VGet(h, "Name", &n) && Bp_State(thug, 5, &n);
	Native_Free(&n);
	return ok;
}

/* SetThugsState(vel=0.4): vel 0 stops the leader (the others follow him), up to 0.4 the leader walks his route with
 * the others behind, above all thugs follow the player (when he is Dtritus) */
static byte Bp_SetThugsState(const nativeVal_t *vel) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *thug1 = Mode_V(&t), *usr = Mode_V(&t), *lim = Mode_V(&t);
	if (!Bp_Thug(1, thug1)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(thug1)) {
		return Mode_Done(&t, PO_LATER1(1.0, BP, "SetThugsState", vel));
	}
	Native_SetFloat(lim, 0.4);
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int follow = Mode_Cmp(vel, ">", lim);
	if (follow > 0) {
		follow = Native_True(usr);
	}
	if (follow > 0) {
		follow = LvN_CtrlIs(usr, "Dtritus");
	}
	if (follow < 0 || !Mode_GVarSet("GenCharSpecialChaseSpeed", vel)) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (follow == 0) {
		Native_SetFloat(lim, 0.0);
		int stop = Mode_Cmp(vel, "==", lim);
		if (stop > 0) {
			ok = Bp_State(thug1, 6, LvN_Empty()) && LvN_SetS(thug1, "UseFunc", "Mission_BeatPickus.ThugNormalSpeech");
		} else if (stop == 0) {
			ok = Bp_State(thug1, 0, LvN_Empty()) && LvN_SetS(thug1, "UseFunc", LvN_SaveIs("MissionState", "TalkToTraitorius") ?
				"CharsNPC.GenericNPCSpeech" : "Mission_BeatPickus.ThugLeaderSpeech");
		} else {
			ok = 0;
		}
	} else {
		ok = Bp_Follow(thug1, usr) && LvN_SetS(thug1, "UseFunc", "Mission_BeatPickus.ThugNormalSpeech");
	}
	for (int i = 2; ok && i < 6; i++) {
		nativeVal_t thug;
		Native_SetNone(&thug);
		ok = Bp_Thug(i, &thug) && (!Native_True(&thug) || (follow ? Bp_Follow(&thug, usr) && LvN_SetS(&thug, "UseFunc",
			"Mission_BeatPickus.ThugNormalSpeech") : Bp_Follow(&thug, thug1) && LvN_SetS(&thug, "UseFunc", "CharsNPC.GenericNPCSpeech")));
		Native_Free(&thug);
	}
	return Mode_Done(&t, ok);
}

MODE_KW(BeatPickus, SetThugsState, "|O", "vel") {
	Mode_KwDefF(v, given, 0, 0.4);
	return Bp_SetThugsState(&v[0]);
}

/* SetState(thug, state, targetName): the state of the local logic (6 stays where he stands) */
static byte BeatPickus_SetState(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t pos;
	(void)out;
	if (LvN_EqI(&a[1], 6)) {
		Native_SetNone(&pos);
		byte ok = Mode_VGet(&a[0], "Pos", &pos) && Mode_VSet(&a[0], "LLogicOrigin", &pos);
		Native_Free(&pos);
		if (!ok) {
			return 0;
		}
	}
	return Mode_VSet(&a[0], "LLogicDefaultState", &a[1]) && Mode_VSet(&a[0], "LLogicDefaultTarget", &a[2]) &&
		Mode_VSet(&a[0], "LLogicState", &a[1]) && Mode_VSet(&a[0], "LLogicTarget", &a[2]) && LvN_SetI(&a[0], "LLogicIdle", 1) &&
		LvN_SetI(&a[0], "LLogicIdle", 0);
}

/* TraitoriusSpeech(UsrName, CharName, optId, FullOption): Traitorius talks to a functionary about the thugs, to
 * Dtritus about flowers; the others get the generic talk */
static byte BeatPickus_TraitoriusSpeech(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const ni[] = { "Mission_FlowerPower_Traitorius_InitMsg1", "Mission_FlowerPower_Traitorius_InitAns1",
		"Mission_FlowerPower_Traitorius_1Msg", "Mission_FlowerPower_Traitorius_1Ans", "Mission_FlowerPower_Traitorius_1_1Msg",
		"Mission_FlowerPower_Traitorius_1_1Ans", NULL };
	(void)out;
	if (!Mode_Import("CharsNPC") || !LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	int fn = Po_NameIs(&a[0], "Functionary");
	if (fn > 0) {
		if (!LvN_SaveIs("MissionState", "TalkToTraitorius")) {
			return Mode_Spch("Mission_HireThugs_Traitorius_InitMsg1", "Mission_HireThugs_Traitorius_InitAns1", NULL);
		}
		nativeVal_t params;
		Native_SetEmpty(&params, NV_TUPLE);
		Native_Copy(Native_Append(&params), &a[1]);
		/* OBJLIB 6: PreHireThugs is a section of mission_beatpickus.def */
		byte ok = LvN_OnEnd(BP, "PreHireThugs", &params) && Po_NI("Functionary", ni);
		Native_Free(&params);
		return ok;
	}
	int dt = fn == 0 ? Po_NameIs(&a[0], "Dtritus") : -1;
	if (dt > 0) {
		return !LvN_EqS(&a[2], "Talk") || Mode_Spch("Mission_FlowerPower_Traitorius_InitMsg2", "Mission_FlowerPower_Traitorius_InitAns2", NULL);
	}
	return dt == 0 && Po_Generic(a);
}

/* HireThugs(): the leader of the thugs is the target (as soon as he is there) */
static byte BeatPickus_HireThugs(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t thug1;
	(void)a;
	(void)out;
	if (!Bp_Thug(1, &thug1)) {
		return 0;
	}
	byte ok = Native_True(&thug1) ? LvN_SetS(&thug1, "UseFunc", "Mission_BeatPickus.ThugLeaderSpeech") && LvN_TargetName(&thug1) :
		PO_LATER(1.0, BP, "HireThugs");
	Native_Free(&thug1);
	return ok;
}

/* ThugLeaderSpeech(UsrName, CharName, optId, FullOption): the leader takes Dtritus' job, PreBeatPickus at the end */
static byte BeatPickus_ThugLeaderSpeech(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const ni[] = { "Mission_HireThugs_Thug_InitMsg1", "Mission_HireThugs_Thug_InitAns1", "Mission_HireThugs_Thug_1Msg",
		"Mission_HireThugs_Thug_1Ans", NULL };
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	int dt = Po_NameIs(&a[0], "Dtritus");
	if (dt > 0) {
		nativeVal_t params;
		Native_SetEmpty(&params, NV_TUPLE);
		Native_Copy(Native_Append(&params), &a[1]);
		/* OBJLIB 6: PreBeatPickus is a section of mission_beatpickus.def */
		byte ok = LvN_OnEnd(BP, "PreBeatPickus", &params) && Po_NI("Police", ni);
		Native_Free(&params);
		return ok;
	}
	return dt == 0 && Mode_Spch("Mission_HireThugs_Thug_InitMsg2", "Mission_HireThugs_Thug_InitAns2", NULL);
}

/* SetThugsState(followPlayerVel) of the module */
static byte Bp_FollowVel(void) {
	nativeVal_t v;
	byte ok = Mode_G(BP, "followPlayerVel", &v) && PORT_LOWN(Mission_BeatPickus, SetThugsState, NULL, "v", &v);
	Native_Free(&v);
	return ok;
}

/* ThugNormalSpeech(UsrName, CharName, optId, FullOption): a thug talks; to Dtritus, and then they follow him */
static byte BeatPickus_ThugNormalSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	int dt = Po_NameIs(&a[0], "Dtritus");
	if (dt > 0) {
		return Mode_Spch("Mission_HireThugs_Thug_InitMsg3", "Mission_HireThugs_Thug_InitAns3", NULL) && Bp_FollowVel();
	}
	return dt == 0 && Mode_Spch("Mission_HireThugs_Thug_InitMsg2", "Mission_HireThugs_Thug_InitAns2", NULL);
}

/* BeatPickus(): the thugs follow the player to Pickus; the alarm hooks, the launcher of the scene at Pickus, the
 * distance check */
static byte BeatPickus_BeatPickus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t pickus, pos, d;
	(void)a;
	(void)out;
	if (!LvN_Entity("Functionary_Pickus", &pickus)) {
		return 0;
	}
	if (!Native_True(&pickus)) {
		Native_Free(&pickus);
		return PO_LATER(1.0, BP, "BeatPickus");
	}
	Native_SetNone(&pos);
	Native_SetInt(&d, 1500);
	byte ok = PO_HOOK("Alarm", "RestoreMissionCallback", BP, "OnAlarmDeactivated") && PO_HOOK("Alarm", "AbortMissionCallback", BP, "OnAlarmActivated") &&
		Mode_VGet(&pickus, "Pos", &pos) && PORT_LOWN(Mission_BeatPickus, CreateLauncher, NULL, "v", &pos) &&
		Mode_Later(1.0, BP, "CheckMaxDist", &d, 1, "CheckMaxDist") && LvN_TargetName(&pickus) && Bp_FollowVel();
	Native_Free(&pickus);
	Native_Free(&pos);
	return ok;
}

/* OnAlarmActivated(): the alarm stops the beating: no distance check, Pickus no target, the launcher off */
static byte BeatPickus_OnAlarmActivated(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t pickus;
	Native_SetNone(&pickus);
	(void)a;
	(void)out;
	if (!Mode_DelSchedS("CheckMaxDist") || !LvN_Entity("Functionary_Pickus", &pickus)) {
		return 0;
	}
	byte ok = !Native_True(&pickus) || PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", &pickus, 0L);
	Native_Free(&pickus);
	int l = ok ? LvN_GTrue(BP, "launcher") : -1;
	return l >= 0 && (!l || Mode_GSetS(BP, "launcher", "OnTake", ""));
}

/* CheckMaxDist(d): every second: the thugs stop when the player is farther than d, and follow him again */
static byte BeatPickus_CheckMaxDist(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *pos = Mode_V(&t), *r = Mode_V(&t), *args = Mode_V(&t), *who = Mode_V(&t);
	(void)out;
	if (!Mode_Later(1.0, BP, "CheckMaxDist", &a[0], 1, "CheckMaxDist")) {
		return Mode_Done(&t, 0);
	}
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	int near = Mode_VGet(usr, "Pos", pos) && PORT_LOWN(Mission_BeatPickus, CheckThugsDist, r, "vv", pos, &a[0]) ? Native_True(r) : -1;
	if (near == 0) {
		int stopped = LvN_GTrue(BP, "thugsStopped");
		if (stopped) {
			return Mode_Done(&t, stopped > 0);
		}
		LvN_Str(args, "Mission_HireThugs_RemoteMessage1");
		LvN_Str(who, "Police");
		return Mode_Done(&t, LvN_PutI(BP, "thugsStopped", 1) && PORT_LOWN(Mission_BeatPickus, SetThugsState, NULL, "d", 0.0) &&
			Mode_LaterIn(0.1, "Speech", "AddRemoteCom", args, 2, "NotSoFast"));
	}
	if (near < 0) {
		return Mode_Done(&t, 0);
	}
	int c = LvN_GTrue(BP, "thugsStopped");
	if (c > 0) {
		c = LvN_CtrlIs(usr, "Dtritus");
	}
	return Mode_Done(&t, c >= 0 && (!c || (LvN_PutI(BP, "thugsStopped", 0) && Bp_FollowVel())));
}

/* CheckThugsDist(pos, d): 1 when a thug is within d of pos */
static byte BeatPickus_CheckThugsDist(const nativeVal_t *a, nativeVal_t *out) {
	for (long i = 1; i < 6; i++) {
		nativeVal_t e, ep, sub, dist, dd;
		Native_SetNone(&e);
		Native_SetNone(&ep);
		Native_SetNone(&sub);
		Native_SetNone(&dist);
		Native_SetNone(&dd);
		int near = Bp_Thug(i, &e) ? Native_True(&e) : -1;
		if (near > 0) {
			near = Mode_VGet(&e, "Pos", &ep) && Mode_VecSub(&a[0], &ep, &sub) && Mode_VecModSqr(&sub, &dist) && Mode_Arith('*', &a[1], &a[1], &dd) ?
				Mode_Cmp(&dist, "<=", &dd) : -1;
		}
		Native_Free(&e);
		Native_Free(&ep);
		Native_Free(&sub);
		Native_Free(&dist);
		Native_Free(&dd);
		if (near) {
			Native_SetInt(out, 1);
			return near > 0;
		}
	}
	Native_SetInt(out, 0);
	return 1;
}

/* CreateLauncher(pos): the item at Pickus that starts the scene */
static byte BeatPickus_CreateLauncher(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t name, l;
	(void)out;
	int there = LvN_GTrue(BP, "launcher");
	if (there < 0) {
		return 0;
	}
	if (!there) {
		LvN_Str(&name, "PickusFightLauncher");
		Native_SetNone(&l);
		byte ok = LvN_CreateAt(&name, &a[0], "Item", &l) && Mode_Put(BP, "launcher", NULL, &l) && Mode_GSetI(BP, "launcher", "Radius", 600) &&
			Mode_GSetI(BP, "launcher", "OnlyUser", 1);
		Native_Free(&name);
		Native_Free(&l);
		if (!ok) {
			return 0;
		}
	}
	return Mode_GSetS(BP, "launcher", "OnTake", "Mission_BeatPickus.LaunchPickusFightScene");
}

/* del Speech.RemoteCom; Speech.RemoteCom = [] (OBJLIB 5: the attribute of the module deleted, the error of a missing
 * one) */
static byte Bp_NoComs(void) {
	nativeVal_t l, speech;
	byte ok = Mode_RefModule("Speech", &speech) && Mode_RefDelAttr(&speech, "RemoteCom");
	Native_Free(&speech);
	if (!ok) {
		return 0;
	}
	Native_SetEmpty(&l, NV_LIST);
	ok = Mode_SetModVar("Speech", "RemoteCom", &l);
	Native_Free(&l);
	return ok;
}

/* LaunchPickusFightScene(ItemName, CharName): Dtritus reached Pickus without the alarm: with the thugs near the scene
 * Beating plays (AskPickus at its end), else the launcher comes back in 0.5 s */
static byte BeatPickus_LaunchPickusFightScene(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pickus = Mode_V(&t), *usr = Mode_V(&t), *un = Mode_V(&t), *pos = Mode_V(&t), *r = Mode_V(&t), *item = Mode_V(&t);
	(void)out;
	if (!LvN_Entity("Functionary_Pickus", pickus)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(pickus)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int bad = !Native_True(usr);
	if (bad == 0) {
		bad = Mode_VGet(usr, "Name", un) ? Mode_Cmp(un, "!=", &a[1]) : -1;
	}
	if (bad == 0) {
		bad = LvN_CtrlIs(usr, "Dtritus");
		bad = bad < 0 ? -1 : !bad;
	}
	if (bad) {
		return Mode_Done(&t, bad > 0);
	}
	int alarm = Mode_AlarmOn();
	if (alarm) {
		return Mode_Done(&t, alarm > 0);
	}
	if (!Mode_GSetS(BP, "launcher", "OnTake", "")) {
		return Mode_Done(&t, 0);
	}
	int near = Mode_VGet(usr, "Pos", pos) && PORT_LOWN(Mission_BeatPickus, CheckThugsDist, r, "vi", pos, 600L) ? Native_True(r) : -1;
	if (near > 0) {
		return Mode_Done(&t, Bp_NoComs() && Mode_DelSchedS("NotSoFast") && Mode_DelSchedS("CheckMaxDist") &&
			PORT_LOWN(Mission_BeatPickus, LightingActivate, NULL, "i", 0L) && PORT_CALL(Scene, Play, NULL, "s", "Beating") &&
			PO_HOOK("Scene", "EndOfSceneCallback", BP, "AskPickus"));
	}
	Native_Free(pos);
	return Mode_Done(&t, near == 0 && Mode_EntityH(&a[0], item) && Mode_VGet(item, "Pos", pos) && PO_LATER1(0.5, BP, "CreateLauncher", pos));
}

/* CreatePickusBeatenClass(): the class of Pickus beaten (models in the level folder) */
static byte BeatPickus_CreatePickusBeatenClass(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Init") && Mode_SAct("CreateClass", "PickusBeaten") && Mode_PathSet(1, "ModelFilePath", "/Map/Beating/PickusBeaten.m3d") &&
		Mode_SAct("CreateAction", "PickusLoop") && Mode_PathSet(0, "AnmFilePath", "/Map/Beating/BeatingAnm/PickusBeaten_Relax.M3D") &&
		Mode_SActSet(0, "isLoop", NULL, 1) && Mode_SAct("CreateAction", "Talk") && Mode_PathSet(0, "AnmFilePath", "/Map/Beating/BeatingAnm/PickusBeaten_Talk.M3D") &&
		Mode_SActSet(0, "isLoop", NULL, 1) && Mode_SActSet(1, "DefAction", "PickusLoop", 0);
}

/* InitPickusBeaten(): Pickus beaten at his dummy, he talks */
static byte BeatPickus_InitPickusBeaten(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t dummy, pos, add, np;
	(void)a;
	LvN_Dummy("DM_Mission_PickusFight", 1, &dummy);
	if (!Native_True(&dummy)) {
		Native_Free(&dummy);
		return LvN_RaiseText("No existe el dummy para situar al Pickus hecho polvo");
	}
	Native_SetNone(&pos);
	Native_SetNone(&np);
	Mode_Ints3(&add, 0, 100, -550);
	byte ok = Mode_FromDummy(&dummy, "WithLife", out) && LvN_SetS(out, "ActCtrl", "PickusBeaten") && LvN_SetI(out, "ActiveCMask", 0) &&
		LvN_SetI(out, "MapCollision", 1) && LvN_SetI(out, "UseAngSel", 1) && LvN_SetI(out, "UseAbsAngSel", 0) && LvN_SetI(out, "VRadius", 150) &&
		LvN_SetI(out, "ArrowCanUse", 1) && LvN_SetI(out, "UseMapCheck", 0) && LvN_SetS(out, "UseFunc", "Mission_BeatPickus.OnTalkToPickus") &&
		LvN_SetI(out, "ArrnOpt", 0) && Po_TalkOpt(out) && LvN_SetLang(out, "Descriptor", "CharName_Pickus") && LvN_SetI(out, "OffModPosAsPos", 1) &&
		Mode_VGet(out, "Pos", &pos) && Mode_VSet(out, "OffModPos", &pos);
	Native_Free(&pos);
	ok = ok && Mode_VGet(out, "Pos", &pos) && Mode_VecAdd(&pos, &add, &np) && Mode_VSet(out, "Pos", &np);
	Native_Free(&dummy);
	Native_Free(&pos);
	Native_Free(&add);
	Native_Free(&np);
	return ok;
}

/* DelThug(thugName): the thug deleted (as soon as he is there) */
static byte BeatPickus_DelThug(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t thug;
	(void)out;
	if (!Mode_EntityH(&a[0], &thug)) {
		return 0;
	}
	byte ok = Native_True(&thug) ? Mode_VDel(&thug) : PO_LATER1(1.0, BP, "DelThug", &a[0]);
	Native_Free(&thug);
	return ok;
}

/* AskPickus(usr=None): after the scene: the lights, no thugs, Pickus beaten in place of Pickus */
static byte Bp_AskPickus(void) {
	nativeVal_t e, pickus;
	if (!PORT_LOWN(Mission_BeatPickus, LightingActivate, NULL, "i", 1L)) {
		return 0;
	}
	for (int i = 1; i < 6; i++) {
		char name[0x20];
		Str_Printf(name, sizeof(name), "Police_Thug%d", i);
		if (!PORT_LOWN(Mission_BeatPickus, DelThug, NULL, "s", name)) {
			return 0;
		}
	}
	Native_SetNone(&e);
	Native_SetNone(&pickus);
	byte ok = LvN_Entity("Functionary_Pickus", &e) && Mode_VDel(&e) && PORT_LOWN(Mission_BeatPickus, InitPickusBeaten, &pickus, "") &&
		(!Native_True(&pickus) || LvN_TargetName(&pickus));
	Native_Free(&e);
	Native_Free(&pickus);
	return ok;
}

MODE_KW(BeatPickus, AskPickus, "|O", "usr") {
	return Bp_AskPickus();
}

/* OnTalkToPickus(UsrName, CharName, optId, FullOption): Pickus tells about the plans (PreGetFuncShipPlans at the
 * end the first time) */
static byte BeatPickus_OnTalkToPickus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t state, plans, args[4];
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	Mode_SaveVar("MissionState", &state);
	LvN_Str(&plans, "GetFuncShipPlans");
	int first = Mode_Cmp(&state, "!=", &plans);
	Native_Free(&state);
	Native_Free(&plans);
	if (first < 0 || (first && !LV_ONEND(Mission_BeatPickus, PreGetFuncShipPlans, NULL))) {
		return 0;
	}
	/* Speech.OnObjSpch(Scrap.GetEntity(CharName), "Pickus", msg, ans, action = "Talk", defLoop = "PickusLoop") */
	Native_SetNone(&args[0]);
	if (!Mode_EntityH(&a[1], &args[0])) {
		return 0;
	}
	LvN_Str(&args[1], "Pickus");
	LvN_Str(&args[2], "Mission_AskPickus_InitMsg1");
	LvN_Str(&args[3], "Mission_AskPickus_InitAns1");
	byte ok = PORT_CALL(Speech, OnObjSpch, NULL, "vvvv;action=s,defLoop=s", &args[0], &args[1], &args[2], &args[3], "Talk", "PickusLoop");
	for (int i = 0; i < 4; i++) {
		Native_Free(&args[i]);
	}
	return ok;
}

/* PreGetFuncShipPlans(): the alarm hooks off, saved, the plans of the functionaries' ship */
static byte BeatPickus_PreGetFuncShipPlans(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!LvN_ModNone("Alarm", "RestoreMissionCallback") || !LvN_ModNone("Alarm", "AbortMissionCallback")) {
		return 0;
	}
	SaveVar_Set("MissionState", "GetFuncShipPlans");
	return Mode_WayPoint() && Mode_MMDesc("Mission_GetFuncShipPlans_MissionText1", 1) && PORT_LOWN(Mission_BeatPickus, GetFuncShipPlans, NULL, "");
}

/* the photo object name at the dummy pattern into plansNames; raise text when the dummy is missing */
static byte Bp_Plan(const char *pattern, const char *name, const char *missing) {
	nativeVal_t dummy, pos, ang, pln, pn;
	LvN_Dummy(pattern, 1, &dummy);
	if (!Native_True(&dummy)) {
		Native_Free(&dummy);
		return LvN_RaiseText(missing);
	}
	Native_SetNone(&pln);
	Native_SetNone(&pn);
	byte ok = LvN_DummyPosAng(&dummy, &pos, &ang) && PORT_CALL(MissionsFuncs, CreatePhotoObject, &pln, "svv", name, &pos, &ang) &&
		Mode_VGet(&pln, "Name", &pn) && Mode_Append(BP, "plansNames", &pn);
	Native_Free(&dummy);
	Native_Free(&pos);
	Native_Free(&ang);
	Native_Free(&pln);
	Native_Free(&pn);
	return ok;
}

/* GetFuncShipPlans(): the two blueprints to photograph */
static byte BeatPickus_GetFuncShipPlans(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t plans;
	Native_SetNone(&plans);
	(void)a;
	(void)out;
	if (!Mode_GVarSetI("LittlePoliceAsksForMoney", 1) || !Mode_GVarSetS("NotCleanableByGear", "") || !Mode_Import("CharAct") ||
		!PO_HOOK("CharAct", "TakenPhoto", BP, "TakePlanPhoto") ||
		!Bp_Plan("DM_Mission_FunctionaryShipBlueprint", "PlanoSFunc1", "No existe el dummy de los planos de la nave de los funcionarios") ||
		!Bp_Plan("DM_Mission_FunctionaryEngineBlueprint", "PlanoMFunc1",
			"No existe el dummy de los planos de los motores de la nave de los funcionarios") || !Mode_G(BP, "plansNames", &plans)) {
		return 0;
	}
	byte ok = PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &plans);
	Native_Free(&plans);
	return ok;
}

/* TakePlanPhoto(planName): the photo of a blueprint of the mission (1, else 0); after both Traitorius calls */
static byte BeatPickus_TakePlanPhoto(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t plans, e, args[3];
	Native_SetNone(&plans);
	if (!Mode_Import("CharAct") || !Mode_G(BP, "plansNames", &plans)) {
		return 0;
	}
	int in = Native_Contains(&plans, &a[0]);
	Native_Free(&plans);
	if (in <= 0) {
		Native_SetInt(out, 0);
		return in == 0;
	}
	Native_SetNone(&e);
	byte ok = Mode_EntityH(&a[0], &e) && PORT_CALL(MissionsFuncs, UnsetPhotoObject, NULL, "v", &e);
	Native_Free(&e);
	if (!ok) {
		return 0;
	}
	ok = LvN_EqS(&a[0], "PlanoMFunc1") ? Mode_Blueprint(BP, "Hangar.availableEnginesList", "MFUNC1", "Mission_GetFuncShipPlans_SystemMessage2", 0) :
		Mode_Blueprint(BP, "Hangar.shipsToEditList", "SFunc1", "Mission_GetFuncShipPlans_SystemMessage1", 0);
	int two = ok && LvN_GAdd(BP, "numPlansWithPhoto", 1, NULL) ? LvN_GEqI(BP, "numPlansWithPhoto", 2) : -1;
	if (two > 0) {
		if (!LvN_ModNone("CharAct", "TakenPhoto") || !LvN_GObj(BP, "PreTalkToTraitoriusAgain", &args[2])) {
			return 0;
		}
		LvN_Str(&args[0], "Mission_GetFuncShipPlans_RemoteMessage1");
		LvN_Str(&args[1], "Traitorius");
		ok = Mode_LaterIn(1.0, "Speech", "AddRemoteCom", args, 3, NULL);
		for (int i = 0; i < 3; i++) {
			Native_Free(&args[i]);
		}
		if (!ok) {
			return 0;
		}
	}
	Native_SetInt(out, 1);
	return two >= 0;
}

/* traitorius = Scrap.GetEntity("Functionary_Traitorius") */
static byte Bp_GetTraitorius(void) {
	nativeVal_t tr;
	byte ok = LvN_Entity("Functionary_Traitorius", &tr) && Mode_Put(BP, "traitorius", NULL, &tr);
	Native_Free(&tr);
	return ok;
}

/* TalkToTraitoriusAgain(): Traitorius is the target again */
static byte BeatPickus_TalkToTraitoriusAgain(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	int there = Bp_GetTraitorius() ? LvN_GTrue(BP, "traitorius") : -1;
	return there >= 0 && (!there || (Mode_GSetS(BP, "traitorius", "UseFunc", "Mission_BeatPickus.TraitoriusAgainSpeech") &&
		LvN_TargetG(BP, "traitorius")));
}

/* TraitoriusAgainSpeech(UsrName, CharName, optId, FullOption): the plans brought: Traitorius sends Dtritus on
 * (SetNextMission at the end, once) */
static byte BeatPickus_TraitoriusAgainSpeech(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const ni[] = { "Mission_TalkToTraitorius2_Traitorius_InitMsg1", "Mission_TalkToTraitorius2_Traitorius_InitAns1",
		"Mission_TalkToTraitorius2_Traitorius_1Msg", "Mission_TalkToTraitorius2_Traitorius_1Ans",
		"Mission_TalkToTraitorius2_Traitorius_2Msg", "Mission_TalkToTraitorius2_Traitorius_2Ans", NULL };
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	int fn = Po_NameIs(&a[0], "Functionary");
	if (fn > 0) {
		int talked = LvN_GTrue(BP, "alreadyTalked");
		if (talked > 0) {
			return Mode_Spch("Mission_Search&DestroyMickus_Traitorius_InitMsg1", "Mission_Search&DestroyMickus_Traitorius_InitAns1", NULL);
		}
		/* OBJLIB 6: SetNextMission is a section of mission_beatpickus.def */
		return talked == 0 && Bp_GetTraitorius() && Mode_GSetS(BP, "traitorius", "UseFunc", "Mission_BeatPickus.TraitoriusSpeech") &&
			LvN_GActivate(BP, "traitorius", 0) && LvN_OnEnd(BP, "SetNextMission", NULL) && Po_NI("Functionary", ni);
	}
	int dt = fn == 0 ? Po_NameIs(&a[0], "Dtritus") : -1;
	if (dt > 0) {
		return !LvN_EqS(&a[2], "Talk") || Mode_Spch("Mission_FlowerPower_Traitorius_InitMsg2", "Mission_FlowerPower_Traitorius_InitAns2", NULL);
	}
	return dt == 0 && Po_Generic(a);
}

/* LightingActivate(activate=0): the electric effect of the police (PoliceFXLighting01) on or off (the call of the map
 * elements) */
static byte Bp_LightingActivate(const nativeVal_t *activate) {
	static const long params[] = { 5120, -1940, 29000, 1000 };
	if (!PORT_CALL(Scrap, SetCallFunc, NULL, "s", "!MapElementsActivate") || !PORT_CALL(Scrap, AddParams, NULL, "s", "PoliceFXLighting01")) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		if (!PORT_CALL(Scrap, AddParamf, NULL, "i", params[i])) {
			return 0;
		}
	}
	return PORT_CALL(Scrap, AddParami, NULL, "v", activate) && PORT_CALL(Scrap, Execute, NULL, "");
}

MODE_KW(BeatPickus, LightingActivate, "|O", "activate") {
	Mode_KwDefI(v, given, 0, 0);
	return Bp_LightingActivate(&v[0]);
}

LV_FN(BeatPickus, InitStatusA, 1, BeatPickus_InitStatus)
LV_FN(BeatPickus, InitA, 1, BeatPickus_Init)
LV_FN(BeatPickus, DisableIAFunctionaryA, 1, BeatPickus_DisableIAFunctionary)
LV_FN(BeatPickus, OnTraitoriusRespawnA, 1, BeatPickus_OnTraitoriusRespawn)
LV_FN(BeatPickus, OnThugRespawnA, 1, BeatPickus_OnThugRespawn)
LV_FN(BeatPickus, SetStateA, 3, BeatPickus_SetState)
LV_FN(BeatPickus, TraitoriusSpeechA, 4, BeatPickus_TraitoriusSpeech)
LV_FN(BeatPickus, HireThugsA, 0, BeatPickus_HireThugs)
LV_FN(BeatPickus, ThugLeaderSpeechA, 4, BeatPickus_ThugLeaderSpeech)
LV_FN(BeatPickus, ThugNormalSpeechA, 4, BeatPickus_ThugNormalSpeech)
LV_FN(BeatPickus, BeatPickusA, 0, BeatPickus_BeatPickus)
LV_FN(BeatPickus, OnAlarmActivatedA, 0, BeatPickus_OnAlarmActivated)
LV_FN(BeatPickus, CheckMaxDistA, 1, BeatPickus_CheckMaxDist)
LV_FN(BeatPickus, CheckThugsDistA, 2, BeatPickus_CheckThugsDist)
LV_FN(BeatPickus, CreateLauncherA, 1, BeatPickus_CreateLauncher)
LV_FN(BeatPickus, LaunchPickusFightSceneA, 2, BeatPickus_LaunchPickusFightScene)
LV_FN(BeatPickus, CreatePickusBeatenClassA, 0, BeatPickus_CreatePickusBeatenClass)
LV_FN(BeatPickus, InitPickusBeatenA, 0, BeatPickus_InitPickusBeaten)
LV_FN(BeatPickus, DelThugA, 1, BeatPickus_DelThug)
LV_FN(BeatPickus, OnTalkToPickusA, 4, BeatPickus_OnTalkToPickus)
LV_FN(BeatPickus, PreGetFuncShipPlansA, 0, BeatPickus_PreGetFuncShipPlans)
LV_FN(BeatPickus, GetFuncShipPlansA, 0, BeatPickus_GetFuncShipPlans)
LV_FN(BeatPickus, TakePlanPhotoA, 1, BeatPickus_TakePlanPhoto)
LV_FN(BeatPickus, TalkToTraitoriusAgainA, 0, BeatPickus_TalkToTraitoriusAgain)
LV_FN(BeatPickus, TraitoriusAgainSpeechA, 4, BeatPickus_TraitoriusAgainSpeech)

PyMethodDef g_PoliceBeatPickusMethods[] = {
	{ "InitStatus", BeatPickus_InitStatusA, METH_VARARGS, NULL },
	{ "Init", BeatPickus_InitA, METH_VARARGS, NULL },
	{ "DisableIAFunctionary", BeatPickus_DisableIAFunctionaryA, METH_VARARGS, NULL },
	{ "OnTraitoriusRespawn", BeatPickus_OnTraitoriusRespawnA, METH_VARARGS, NULL },
	{ "OnThugRespawn", BeatPickus_OnThugRespawnA, METH_VARARGS, NULL },
	{ "SetThugsState", (PyCFunction)(void (*)(void))BeatPickus_SetThugsState, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "SetState", BeatPickus_SetStateA, METH_VARARGS, NULL },
	{ "TraitoriusSpeech", BeatPickus_TraitoriusSpeechA, METH_VARARGS, NULL },
	{ "HireThugs", BeatPickus_HireThugsA, METH_VARARGS, NULL },
	{ "ThugLeaderSpeech", BeatPickus_ThugLeaderSpeechA, METH_VARARGS, NULL },
	{ "ThugNormalSpeech", BeatPickus_ThugNormalSpeechA, METH_VARARGS, NULL },
	{ "BeatPickus", BeatPickus_BeatPickusA, METH_VARARGS, NULL },
	{ "OnAlarmActivated", BeatPickus_OnAlarmActivatedA, METH_VARARGS, NULL },
	{ "CheckMaxDist", BeatPickus_CheckMaxDistA, METH_VARARGS, NULL },
	{ "CheckThugsDist", BeatPickus_CheckThugsDistA, METH_VARARGS, NULL },
	{ "CreateLauncher", BeatPickus_CreateLauncherA, METH_VARARGS, NULL },
	{ "LaunchPickusFightScene", BeatPickus_LaunchPickusFightSceneA, METH_VARARGS, NULL },
	{ "CreatePickusBeatenClass", BeatPickus_CreatePickusBeatenClassA, METH_VARARGS, NULL },
	{ "InitPickusBeaten", BeatPickus_InitPickusBeatenA, METH_VARARGS, NULL },
	{ "DelThug", BeatPickus_DelThugA, METH_VARARGS, NULL },
	{ "AskPickus", (PyCFunction)(void (*)(void))BeatPickus_AskPickus, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "OnTalkToPickus", BeatPickus_OnTalkToPickusA, METH_VARARGS, NULL },
	{ "PreGetFuncShipPlans", BeatPickus_PreGetFuncShipPlansA, METH_VARARGS, NULL },
	{ "GetFuncShipPlans", BeatPickus_GetFuncShipPlansA, METH_VARARGS, NULL },
	{ "TakePlanPhoto", BeatPickus_TakePlanPhotoA, METH_VARARGS, NULL },
	{ "TalkToTraitoriusAgain", BeatPickus_TalkToTraitoriusAgainA, METH_VARARGS, NULL },
	{ "TraitoriusAgainSpeech", BeatPickus_TraitoriusAgainSpeechA, METH_VARARGS, NULL },
	{ "LightingActivate", (PyCFunction)(void (*)(void))BeatPickus_LightingActivate, METH_VARARGS | METH_KEYWORDS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FicusDead ---- */

/* InitFicusDead(): Ficus' body of the map talks */
static byte FicusDead_InitFicusDead(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t ficus, cam;
	Native_SetNone(&ficus);
	(void)a;
	(void)out;
	if (!Mode_Import("Map") || !LvN_ModGet("Map", "ficus", &ficus)) {
		return 0;
	}
	byte ok = Mode_Put(FD, "ficus", NULL, &ficus);
	Native_Free(&ficus);
	if (!ok || !Mode_G(FD, "ficus", &ficus)) {
		return 0;
	}
	Mode_Ints3(&cam, 0, 280, 0);
	ok = Mode_VSet(&ficus, "CamTgt", &cam) && LvN_SetI(&ficus, "ArrowCanUse", 1) && LvN_SetS(&ficus, "UseFunc", "Mission_FicusDead.OnTalkToFicus") &&
		LvN_SetI(&ficus, "ArrnOpt", 0) && Po_TalkOpt(&ficus) && LvN_SetLang(&ficus, "Descriptor", "CharName_Ficus");
	Native_Free(&ficus);
	Native_Free(&cam);
	return ok;
}

/* Init(LevelPath): Ficus is the target, or Deep Throat's desktop after his message */
static byte FicusDead_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (LvN_SaveIs("MissionState", "TalkToDeepThroat")) {
		return PORT_LOWN(Mission_FicusDead, AfterDeepThroatMessageShown, NULL, "");
	}
	return PORT_LOWN(Mission_FicusDead, InitFicusDead, NULL, "") && LvN_TargetG(FD, "ficus");
}

/* OnTalkToFicus(UsrName, CharName, optId, FullOption): Ficus is dead; Dtritus says so */
static byte FicusDead_OnTalkToFicus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t ficus;
	(void)out;
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	if (!Mode_G(FD, "ficus", &ficus)) {
		return 0;
	}
	byte ok = LvN_SetS(&ficus, "Action", "FicusDead") && LvN_SetS(&ficus, "DefLoop", "FicusDead") && LvN_SetI(&ficus, "ArrowCanUse", 0);
	Native_Free(&ficus);
	/* OBJLIB 6: AfterDtritusSpeechShown is a section of mission_ficusdead.def */
	return ok && LvN_Com("Mission_TalkToFicus3_Dtritus_InitMsg1", "", FD, "AfterDtritusSpeechShown", LvN_NoArgs()) && Mode_Target("");
}

/* Dtritus' desktop as the target, the dialog of the desktop the global func of module */
static byte Po_Desktop(const char *module, const char *func) {
	nativeVal_t dd;
	Native_SetNone(&dd);
	if (!Mode_Import("DTritusDesktop") || !LvN_Entity("DM_DTritusDesktop", &dd)) {
		return 0;
	}
	byte ok = LvN_TargetName(&dd) && PO_HOOK("DTritusDesktop", "DialogController", module, func);
	Native_Free(&dd);
	return ok;
}

/* AfterDeepThroatMessageShown(): Dtritus' desktop is the target, Deep Throat talks through it */
static byte FicusDead_AfterDeepThroatMessageShown(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Po_Desktop(FD, "DeepThroatSpeech");
}

/* Scrap.GetEntity("DM_DTritusDesktop") of the scripts that look it up for nothing */
static byte Po_DesktopLookup(void) {
	nativeVal_t dd;
	byte ok = LvN_Entity("DM_DTritusDesktop", &dd);
	Native_Free(&dd);
	return ok;
}

/* DeepThroatSpeech(UsrName, CharName, optId, FullOption): Deep Throat's talk, SetNextMission at the end */
static byte FicusDead_DeepThroatSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Po_DesktopLookup() && Mode_Target("") && LvN_Speakers(&a[0], &a[1]) && LV_ONEND(Mission_FicusDead, SetNextMission, NULL) &&
		Mode_NISpeech("DeepThroat", "Mission_CallDeepThroat_ComDeepthroat_NI_1Msg");
}

/* SetNextMission(): Mission_FunctionaryHunting in Outskirts */
static byte FicusDead_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("DTritusDesktop") && LvN_ModNone("DTritusDesktop", "DialogController") && Mode_DelSaveVars("MissionState") &&
		PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_FunctionaryHunting", "Outskirts") &&
		Mode_MMDesc("Mission_FunctionaryHunting_MissionText1", 1);
}

LV_FN(FicusDead, InitFicusDeadA, 0, FicusDead_InitFicusDead)
LV_FN(FicusDead, InitA, 1, FicusDead_Init)
LV_FN(FicusDead, OnTalkToFicusA, 4, FicusDead_OnTalkToFicus)
LV_FN(FicusDead, AfterDeepThroatMessageShownA, 0, FicusDead_AfterDeepThroatMessageShown)
LV_FN(FicusDead, DeepThroatSpeechA, 4, FicusDead_DeepThroatSpeech)
LV_FN(FicusDead, SetNextMissionA, 0, FicusDead_SetNextMission)

PyMethodDef g_PoliceFicusDeadMethods[] = {
	{ "InitFicusDead", FicusDead_InitFicusDeadA, METH_VARARGS, NULL },
	{ "Init", FicusDead_InitA, METH_VARARGS, NULL },
	{ "OnTalkToFicus", FicusDead_OnTalkToFicusA, METH_VARARGS, NULL },
	{ "AfterDeepThroatMessageShown", FicusDead_AfterDeepThroatMessageShownA, METH_VARARGS, NULL },
	{ "DeepThroatSpeech", FicusDead_DeepThroatSpeechA, METH_VARARGS, NULL },
	{ "SetNextMission", FicusDead_SetNextMissionA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetAuthorization ---- */

/* InitStatus(LevelPath): the human in the jail */
static byte GetAuthorization_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LV_MISSIONCHAR("Human_Default", Mission_GetAuthorization, CreateHuman);
}

/* CreateHuman(e): the human (the module variable human), no talk */
static byte GetAuthorization_CreateHuman(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t h;
	Native_SetNone(&h);
	(void)out;
	if (!Mode_Put(GA, "human", NULL, &a[0]) || !Mode_G(GA, "human", &h)) {
		return 0;
	}
	byte ok = LvN_SetLang(&h, "Descriptor", "CharName_Human") && LvN_SetS(&h, "UseFunc", "") && LvN_SetI(&h, "ArrowCanUse", 0);
	Native_Free(&h);
	return ok;
}

/* Init(LevelPath): the scene MeetHuman, the state of the mission */
static byte GetAuthorization_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t state;
	byte ok;
	(void)a;
	(void)out;
	/* OBJLIB 6: LoadScene and GetAuthorization2 are sections of mission_getauthorization.def */
	if (!LV_OWN(GA, "LoadScene", NULL, "")) {
		return 0;
	}
	Mode_SaveVar("MissionState", &state);
	if (LvN_EqS(&state, "DeepThroatSpeech")) {
		ok = LV_COM("Mission_GoToPolice_RemoteMessage1", "DeepThroat", Mission_GetAuthorization, AfterDeepThroatMessage, LvN_NoArgs());
	} else if (LvN_EqS(&state, "GetAuthorization")) {
		ok = PORT_LOWN(Mission_GetAuthorization, GetAuthorization, NULL, "");
	} else if (LvN_EqS(&state, "GetAuthorization2")) {
		ok = LV_OWN(GA, "GetAuthorization2", NULL, "");
	} else if (LvN_EqS(&state, "FreeHuman") || LvN_EqS(&state, "FreeHuman2")) {
		ok = PORT_LOWN(Mission_GetAuthorization, FreeHuman, NULL, "");
	} else {
		ok = 1;
	}
	Native_Free(&state);
	return ok;
}

/* AfterDeepThroatMessage(): Dtritus' desktop is the target, Deep Throat talks through it */
static byte GetAuthorization_AfterDeepThroatMessage(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t dd;
	(void)a;
	(void)out;
	if (!LvN_Entity("DM_DTritusDesktop", &dd)) {
		return 0;
	}
	byte ok = Mode_Put(GA, "dd", NULL, &dd);
	Native_Free(&dd);
	return ok && LvN_TargetG(GA, "dd") && Mode_MMDesc("Mission_TalkToDeepThroat7_MissionText1", 1) &&
		PO_HOOK("DTritusDesktop", "DialogController", GA, "DeepThroatSpeech");
}

/* DeepThroatSpeech(UsrName, CharName, optId, FullOption): Deep Throat's talk, PreGetAuthorization at the end */
static byte GetAuthorization_DeepThroatSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_Speakers(&a[0], &a[1]) && LV_ONEND(Mission_GetAuthorization, PreGetAuthorization, NULL) && Po_TargetNone() &&
		Mode_NISpeech("DeepThroat", "Mission_TalkToDeepThroat7__ComDeepThroat_NI_1Msg");
}

/* PreGetAuthorization(): saved; a messenger already goes for the password, else first to a conversion point */
static byte GetAuthorization_PreGetAuthorization(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	(void)a;
	(void)out;
	if (!LvN_ModNone("DTritusDesktop", "DialogController") || !LvN_GActivate(GA, "dd", 0) || !Mode_MMDesc("Mission_GetPassword_MissionText1", 1) ||
		!Mode_WayPoint()) {
		return 0;
	}
	SaveVar_Set("MissionState", "GetAuthorization");
	Native_SetNone(&usr);
	int m = Po_UsrCtrl(&usr, "Messenger");
	Native_Free(&usr);
	if (m > 0) {
		SaveVar_Set("MissionState", "GetAuthorization2");
		/* OBJLIB 6: GetAuthorization2 is a section of mission_getauthorization.def */
		return LV_OWN(GA, "GetAuthorization2", NULL, "");
	}
	return m == 0 && PORT_LOWN(Mission_GetAuthorization, GetAuthorization, NULL, "");
}

/* GetAuthorization(): the nearest conversion point is the target until the player is a messenger */
static byte GetAuthorization_GetAuthorization(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return PO_HOOK("CharConversor", "afterExitConversor", GA, "ShowAfterConversionMessage") &&
		PORT_LOWN(Mission_GetAuthorization, ShowNearestConversor, NULL, "");
}

/* ShowNearestConversor(): every 3 s the conversion point nearest to the player is the target */
static byte GetAuthorization_ShowNearestConversor(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, pos, conv;
	(void)a;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&pos);
	Native_SetNone(&conv);
	int u = LvN_Usr(&usr);
	byte ok = u >= 0;
	if (u > 0) {
		ok = Mode_VGet(&usr, "Pos", &pos) && PORT_CALL(CharConversor, GetNearestConversionPoint, &conv, "v", &pos) &&
			Mode_Put(GA, "conv", NULL, &conv);
		int c = ok ? LvN_GTrue(GA, "conv") : -1;
		ok = c >= 0 && (!c || (LvN_TargetG(GA, "conv") && Mode_Later(3, GA, "ShowNearestConversor", NULL, 0, "ShowConversor")));
	}
	Native_Free(&usr);
	Native_Free(&pos);
	Native_Free(&conv);
	return ok;
}

/* ShowAfterConversionMessage(): out of the conversor as a messenger: Deep Throat's message, the password next */
static byte GetAuthorization_ShowAfterConversionMessage(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, un;
	(void)a;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&un);
	int m = LvN_Usr(&usr);
	if (m > 0) {
		m = Mode_VGet(&usr, "Name", &un) ? Po_NameIs(&un, "Messenger") : -1;
	}
	Native_Free(&usr);
	Native_Free(&un);
	if (m <= 0) {
		return m == 0;
	}
	if (!Mode_DelSchedS("ShowConversor") || !LvN_ModNone("CharConversor", "afterExitConversor") || !LvN_GActivate(GA, "conv", 0) ||
		!LvN_Com("Mission_GetPassword_RemoteMessage1", "DeepThroat", NULL, NULL, NULL)) {
		return 0;
	}
	SaveVar_Set("MissionState", "GetAuthorization2");
	/* OBJLIB 6: GetAuthorization2 is a section of mission_getauthorization.def */
	return LvN_PutNone(GA, "conv") && LV_OWN(GA, "GetAuthorization2", NULL, "");
}

/* OnDataTaken(messengerName): the player took a tube: empty, filled by him, or filled by another */
static byte GetAuthorization_OnDataTaken(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, un, st;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&un);
	Native_SetNone(&st);
	int mine = LvN_Usr(&usr);
	if (mine > 0) {
		mine = Mode_VGet(&usr, "Name", &un) ? Mode_Cmp(&un, "==", &a[0]) : -1;
	}
	byte ok = mine >= 0;
	if (mine > 0) {
		int full = Native_GVarGet("MessengerTubeState", &st) ? Native_True(&st) : -1;
		int byUser = full > 0 ? LvN_GTrue(GA, "filledByUser") : 0;
		ok = full >= 0 && byUser >= 0 && LvN_Com(full == 0 ? "Mission_GetPassword_RemoteMessage2" : byUser > 0 ?
			"Mission_GetPassword_RemoteMessage3" : "Mission_GetPassword_RemoteMessage6", "DeepThroat", NULL, NULL, NULL);
	}
	Native_Free(&usr);
	Native_Free(&un);
	Native_Free(&st);
	return ok;
}

/* OnDataDropped(messengerName, dataStatus): a full tube of the player at a desk wins (if he left it there himself);
 * an empty tube at the recharger tells who fills it */
static byte GetAuthorization_OnDataDropped(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, un;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&un);
	int u = LvN_Usr(&usr);
	byte ok = u >= 0;
	if (u > 0) {
		int full = LvN_EqI(&a[1], 1);
		if (full > 0) {
			full = LvN_GTrue(GA, "filledByUser");
		}
		if (full > 0) {
			int self = Mode_VGet(&usr, "Name", &un) ? Mode_Cmp(&a[0], "==", &un) : -1;
			/* OBJLIB 6: PreFreeHuman is a section of mission_getauthorization.def */
			ok = self > 0 ? LvN_Com("Mission_GetPassword_RemoteMessage5", "DeepThroat", GA, "PreFreeHuman", LvN_NoArgs()) :
				self == 0 && LvN_Com("Mission_GetPassword_RemoteMessage4", "DeepThroat", NULL, NULL, NULL);
		} else if (full == 0 && LvN_EqI(&a[1], 0)) {
			int by = Mode_VGet(&usr, "Name", &un) ? Mode_Cmp(&a[0], "==", &un) : -1;
			ok = by >= 0 && LvN_PutI(GA, "filledByUser", by);
		} else {
			ok = full >= 0;
		}
	}
	Native_Free(&usr);
	Native_Free(&un);
	return ok;
}

/* FreeHuman(): the gatekeeper of the security door lets Dtritus pass (StartScene), the trigger of the human */
static byte GetAuthorization_FreeHuman(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t dummy, pos, l;
	(void)a;
	(void)out;
	nativeVal_t start;
	if (!Mode_DoorFoe("GateKeeper_08", "DtritusGroupGateKeeper") || !Mode_RefG(GA, "StartScene", &start)) {
		return 0;
	}
	byte ok = PORT_CALL(GateKeeper, SetDoor, NULL, "ssO", "GateKeeper_08", "DM_Door_PoliceSegurity_00", start.obj);
	Native_Free(&start);
	if (!ok || !PO_ENTS("GateKeeper_08", "UseFunc", "Mission_GetAuthorization.TalkPepe") || !Mode_Target("GateKeeper_08")) {
		return 0;
	}
	LvN_Dummy("DM_Trigger_Human", 1, &dummy);
	if (!Native_True(&dummy)) {
		Native_Free(&dummy);
		return LvN_RaiseText("No se encuentra el dummy: 'DM_Trigger_Human'");
	}
	Native_SetNone(&pos);
	Native_SetNone(&l);
	ok = LvN_Item(&dummy, 1, &pos) && Po_Item("TriggerHuman", &pos, 500, -1, "Mission_GetAuthorization.OnTriggerActivated", &l);
	Native_Free(&dummy);
	Native_Free(&pos);
	Native_Free(&l);
	return ok;
}

/* TalkPepe(UsrName, CharName, optId, FullOption): the gatekeeper talks; Dtritus cannot use the conversion point */
static byte GetAuthorization_TalkPepe(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	(void)out;
	if (!Mode_Import("Speech")) {
		return 0;
	}
	Native_SetNone(&usr);
	int dt = Po_UsrCtrl(&usr, "Dtritus");
	Native_Free(&usr);
	return dt >= 0 && (!dt || PO_ENTI("DM_Conversion_Point_08", "ArrowCanUse", 0)) &&
		PORT_CALL(Speech, AutoSpeech, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]);
}

/* StartScene(Door, CharName): through the security door: the scene MeetHuman the first time (GoToPress at its
 * end), else straight to the metro */
static byte GetAuthorization_StartScene(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e, usr;
	Native_SetNone(&e);
	(void)a;
	(void)out;
	if (!PO_ENTI("DM_Conversion_Point_08", "ArrowCanUse", 1) || !PO_ENTS("GateKeeper_08", "UseFunc", "Speech.AutoSpeech") || !Po_TargetNone() ||
		!Mode_DoorFoe("GateKeeper_08", "NoOneGroupGateKeeper") ||
		!PORT_CALL(GateKeeper, SetDoor, NULL, "ss", "GateKeeper_08", "DM_Door_PoliceSegurity_00")) {
		return 0;
	}
	if (LvN_SaveIs("MissionState", "FreeHuman")) {
		if (!LvN_PutI(GA, "scenePlayed", 0) || !LvN_Entity("DM_Door_PoliceSegurity_00", &e)) {
			return 0;
		}
		byte ok = LvN_SetS(&e, "Action", "Open") && LvN_SetS(&e, "DefLoop", "Opened");
		Native_Free(&e);
		if (!ok) {
			return 0;
		}
		SaveVar_Set("MissionState", "FreeHuman2");
		return PORT_CALL(Scene, Play, NULL, "s", "MeetHuman") && PO_HOOK("Scene", "EndOfSceneCallback", GA, "GoToPress") &&
			Mode_GVarSetS("EscapeEvent", "Scene.AbortScene");
	}
	Native_SetNone(&usr);
	byte ok = LvN_PutI(GA, "scenePlayed", 1) && Po_Fade(lv_clear, lv_black, 0.5) && Mode_UsrHandle(&usr) && LvN_SetI(&usr, "Life", 100) &&
		LvN_SetI(&usr, "Invulnerable", 1) && PO_LATER1(0.75, GA, "GoToPress", &usr);
	Native_Free(&usr);
	return ok;
}

/* OnTriggerActivated(ItemName, CharName): the human passed his trigger: the alarm, which does not fall */
static byte GetAuthorization_OnTriggerActivated(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t hn, e, fall, one;
	(void)out;
	Native_SetNone(&hn);
	Native_SetNone(&e);
	Native_SetNone(&fall);
	int it = LvN_GAttr(GA, "human", "Name", &hn) ? Mode_Cmp(&a[1], "==", &hn) : -1;
	byte ok = it >= 0;
	if (it > 0) {
		Native_SetInt(&one, 1);
		ok = Mode_EntityH(&a[0], &e) && Mode_VDel(&e) && Native_GVarGet("AlarmFallDelta", &fall) && Mode_Put(GA, "alarmFallDelta", NULL, &fall) &&
			Mode_SetAlarm(&one) && Mode_GVarSetI("AlarmFallDelta", 0);
	}
	Native_Free(&hn);
	Native_Free(&e);
	Native_Free(&fall);
	return ok;
}

/* SLogic.SetOnFloor(h.Name) */
static byte Po_OnFloor(const nativeVal_t *h) {
	nativeVal_t n;
	Native_SetNone(&n);
	byte ok = Mode_VGet(h, "Name", &n) && PORT_CALL(SLogic, SetOnFloor, NULL, "v", &n);
	Native_Free(&n);
	return ok;
}

/* GoToPress(usr): the door closes; the player and the human out of the jail, the human follows him to the metro */
static byte GetAuthorization_GoToPress(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e, h;
	Native_SetNone(&h);
	(void)out;
	if (!LvN_Entity("DM_Door_PoliceSegurity_00", &e)) {
		return 0;
	}
	byte ok = LvN_SetS(&e, "Action", "Close") && LvN_SetS(&e, "DefLoop", "Closed");
	Native_Free(&e);
	if (!ok || !Po_Fade(lv_black, lv_clear, 0.5)) {
		return 0;
	}
	if (!Native_True(&a[0])) {
		return 1;
	}
	int played = LvN_GEqI(GA, "scenePlayed", 0);
	if (played < 0 || !LV_COM(played ? "Mission_FirstMeetHuman_RemoteMessage1" : "Mission_FirstMeetHuman_RemoteMessage2", "DeepThroat",
		Mission_GetAuthorization, GoToPress2, LvN_NoArgs()) || !LvN_SetI(&a[0], "Invulnerable", 0) || !LvN_SetInts3(&a[0], "Pos", 4227, -1771, 28400) ||
		!LvN_SetInts3(&a[0], "Ang", 0, 160, 0) || !Po_OnFloor(&a[0]) || !Mode_Import("MissionsFuncs") || !Mode_G(GA, "human", &h)) {
		return 0;
	}
	ok = LvN_SetInts3(&h, "Pos", 4227, -1771, 28866) && LvN_SetInts3(&h, "Ang", 0, 0, 0);
	Native_Free(&h);
	if (!ok || !LvN_GActivate(GA, "human", 2) || !Mode_G(GA, "human", &h)) {
		return 0;
	}
	ok = Po_OnFloor(&h);
	Native_Free(&h);
	return ok && LvN_HumanFollow(GA, &a[0]) && PO_HOOK("Chars", "OnMainCharDeath", GA, "OnMainCharDeath") &&
		PO_HOOK("CharConversor", "OnPossession", GA, "OnPossession") && PO_HOOK("CharConversor", "afterExitConversor", GA, "OnExitConversor") &&
		PORT_LOWN(Mission_GetAuthorization, CheckMaxDist, NULL, "");
}

/* GoToPress2(): the metro to Press is the target */
static byte GetAuthorization_GoToPress2(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e;
	(void)a;
	(void)out;
	if (!LvN_Entity("DM_Door_Metro_Press", &e)) {
		return 0;
	}
	byte ok = LvN_SetS(&e, "UseFunc", "Mission_GetAuthorization.OnMetroUsed") && LvN_TargetName(&e);
	Native_Free(&e);
	return ok && Mode_MMDesc("Mission_SetHumanFree_MissionText1", 1) && Mode_WayPoint();
}

/* the human stops and asks the player to come back */
static byte Ga_HumanStops(void) {
	nativeVal_t hn;
	byte ok = LvN_GAttr(GA, "human", "Name", &hn) && CHAR_STATE("iv", 0L, &hn);
	Native_Free(&hn);
	return ok && LvN_Com("Mission_SetHumanFree_RemoteMessage2", "Human", NULL, NULL, NULL);
}

/* OnPossession(newCharName, pos): a possession: the human stops */
static byte GetAuthorization_OnPossession(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("MissionsFuncs") && Ga_HumanStops() && LvN_GActivate(GA, "human", 0);
}

/* OnExitConversor(): out of a conversor: the human follows only Dtritus */
static byte GetAuthorization_OnExitConversor(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	(void)a;
	(void)out;
	if (!Mode_Import("MissionsFuncs")) {
		return 0;
	}
	Native_SetNone(&usr);
	int u = LvN_Usr(&usr);
	int dt = u > 0 ? LvN_CtrlIs(&usr, "Dtritus") : 0;
	Native_Free(&usr);
	if (u <= 0) {
		return u == 0;
	}
	if (dt == 0) {
		return Ga_HumanStops() && LvN_GActivate(GA, "human", 0);
	}
	return dt > 0 && LvN_GActivate(GA, "human", 2);
}

/* CheckMaxDist(): every second: the human stops when the player is too far, and follows Dtritus again */
static byte GetAuthorization_CheckMaxDist(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, r, hn, st;
	(void)a;
	(void)out;
	if (!Mode_Later(1.0, GA, "CheckMaxDist", NULL, 0, "CheckMaxDist")) {
		return 0;
	}
	Native_SetNone(&usr);
	Native_SetNone(&r);
	Native_SetNone(&hn);
	Native_SetNone(&st);
	int u = LvN_Usr(&usr);
	byte ok = u >= 0;
	if (u > 0) {
		int near = PORT_LOWN(Mission_GetAuthorization, CheckHumanDist, &r, "") ? Native_True(&r) : -1;
		if (near == 0) {
			int moving = LvN_GAttr(GA, "human", "Name", &hn) && PORT_CALL(SAI, GetStateChar, &st, "v", &hn) ? Mode_CmpI(&st, "!=", 0) : -1;
			ok = moving >= 0 && (!moving || Ga_HumanStops());
		} else if (near > 0) {
			int dt = LvN_CtrlIs(&usr, "Dtritus");
			ok = dt >= 0 && (!dt || LvN_HumanFollow(GA, &usr));
		} else {
			ok = 0;
		}
	}
	Native_Free(&usr);
	Native_Free(&r);
	Native_Free(&hn);
	Native_Free(&st);
	return ok;
}

/* the squared distance from the player to the human compared with dist * dist of a global dist: `<=`: 1, 0, -1 */
static int Ga_Near(const nativeVal_t *usr, const char *dist) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *up = Mode_V(&t), *hp = Mode_V(&t), *sub = Mode_V(&t), *d = Mode_V(&t), *m1 = Mode_V(&t), *m2 = Mode_V(&t), *mm = Mode_V(&t);
	int r = Mode_VGet(usr, "Pos", up) && LvN_GAttr(GA, "human", "Pos", hp) && Mode_VecSub(up, hp, sub) && Mode_VecModSqr(sub, d) &&
		Mode_G(GA, dist, m1) && Mode_G(GA, dist, m2) && Mode_Arith('*', m1, m2, mm) ? Mode_Cmp(d, "<=", mm) : -1;
	Mode_Done(&t, 1);
	return r;
}

/* CheckHumanDist(): 1 when the human is within maxHumanDist of the player */
static byte GetAuthorization_CheckHumanDist(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	(void)a;
	Native_SetNone(&usr);
	int near = LvN_Usr(&usr);
	if (near > 0) {
		near = Ga_Near(&usr, "maxHumanDist");
	}
	Native_Free(&usr);
	Native_SetInt(out, near > 0);
	return near >= 0;
}

/* OnMetroUsed(UsrName, MetroName, optId, FullOption): with the human near the metro goes to Press, the mission
 * Mission_TalkToBoss next */
static byte GetAuthorization_OnMetroUsed(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const saves[] = { "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Bank", "Journalist.Betty_Defaut", "Bank",
		"Journalist.Berto_Defaut", "Bank", NULL };
	nativeVal_t usr;
	(void)out;
	Native_SetNone(&usr);
	int near = LvN_Usr(&usr);
	if (near > 0) {
		near = Ga_Near(&usr, "minHumanDist");
	} else if (near == 0) {
		Native_Free(&usr);
		return 1;
	}
	Native_Free(&usr);
	if (near > 0) {
		if (!Mode_DelSchedS("CheckMaxDist") || !PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_TalkToBoss", "Press")) {
			return 0;
		}
		Mode_Saves(saves);
		return Mode_WayPoint() && PORT_CALL(Elevator, UseElevator, NULL, "vvsv", &a[0], &a[1], "MoveLevel Press", &a[3]);
	}
	return near == 0 && LvN_Com("Mission_SetHumanFree_RemoteMessage2", "Human", NULL, NULL, NULL);
}

LV_FN(GetAuthorization, InitStatusA, 1, GetAuthorization_InitStatus)
LV_FN(GetAuthorization, CreateHumanA, 1, GetAuthorization_CreateHuman)
LV_FN(GetAuthorization, InitA, 1, GetAuthorization_Init)
LV_FN(GetAuthorization, AfterDeepThroatMessageA, 0, GetAuthorization_AfterDeepThroatMessage)
LV_FN(GetAuthorization, DeepThroatSpeechA, 4, GetAuthorization_DeepThroatSpeech)
LV_FN(GetAuthorization, PreGetAuthorizationA, 0, GetAuthorization_PreGetAuthorization)
LV_FN(GetAuthorization, GetAuthorizationA, 0, GetAuthorization_GetAuthorization)
LV_FN(GetAuthorization, ShowNearestConversorA, 0, GetAuthorization_ShowNearestConversor)
LV_FN(GetAuthorization, ShowAfterConversionMessageA, 0, GetAuthorization_ShowAfterConversionMessage)
LV_FN(GetAuthorization, OnDataTakenA, 1, GetAuthorization_OnDataTaken)
LV_FN(GetAuthorization, OnDataDroppedA, 2, GetAuthorization_OnDataDropped)
LV_FN(GetAuthorization, FreeHumanA, 0, GetAuthorization_FreeHuman)
LV_FN(GetAuthorization, TalkPepeA, 4, GetAuthorization_TalkPepe)
LV_FN(GetAuthorization, StartSceneA, 2, GetAuthorization_StartScene)
LV_FN(GetAuthorization, OnTriggerActivatedA, 2, GetAuthorization_OnTriggerActivated)
LV_FN(GetAuthorization, GoToPressA, 1, GetAuthorization_GoToPress)
LV_FN(GetAuthorization, GoToPress2A, 0, GetAuthorization_GoToPress2)
LV_FN(GetAuthorization, OnPossessionA, 2, GetAuthorization_OnPossession)
LV_FN(GetAuthorization, OnExitConversorA, 0, GetAuthorization_OnExitConversor)
LV_FN(GetAuthorization, CheckMaxDistA, 0, GetAuthorization_CheckMaxDist)
LV_FN(GetAuthorization, CheckHumanDistA, 0, GetAuthorization_CheckHumanDist)
LV_FN(GetAuthorization, OnMetroUsedA, 4, GetAuthorization_OnMetroUsed)

PyMethodDef g_PoliceGetAuthorizationMethods[] = {
	{ "InitStatus", GetAuthorization_InitStatusA, METH_VARARGS, NULL },
	{ "CreateHuman", GetAuthorization_CreateHumanA, METH_VARARGS, NULL },
	{ "Init", GetAuthorization_InitA, METH_VARARGS, NULL },
	{ "AfterDeepThroatMessage", GetAuthorization_AfterDeepThroatMessageA, METH_VARARGS, NULL },
	{ "DeepThroatSpeech", GetAuthorization_DeepThroatSpeechA, METH_VARARGS, NULL },
	{ "PreGetAuthorization", GetAuthorization_PreGetAuthorizationA, METH_VARARGS, NULL },
	{ "GetAuthorization", GetAuthorization_GetAuthorizationA, METH_VARARGS, NULL },
	{ "ShowNearestConversor", GetAuthorization_ShowNearestConversorA, METH_VARARGS, NULL },
	{ "ShowAfterConversionMessage", GetAuthorization_ShowAfterConversionMessageA, METH_VARARGS, NULL },
	{ "OnDataTaken", GetAuthorization_OnDataTakenA, METH_VARARGS, NULL },
	{ "OnDataDropped", GetAuthorization_OnDataDroppedA, METH_VARARGS, NULL },
	{ "FreeHuman", GetAuthorization_FreeHumanA, METH_VARARGS, NULL },
	{ "TalkPepe", GetAuthorization_TalkPepeA, METH_VARARGS, NULL },
	{ "StartScene", GetAuthorization_StartSceneA, METH_VARARGS, NULL },
	{ "OnTriggerActivated", GetAuthorization_OnTriggerActivatedA, METH_VARARGS, NULL },
	{ "GoToPress", GetAuthorization_GoToPressA, METH_VARARGS, NULL },
	{ "GoToPress2", GetAuthorization_GoToPress2A, METH_VARARGS, NULL },
	{ "OnPossession", GetAuthorization_OnPossessionA, METH_VARARGS, NULL },
	{ "OnExitConversor", GetAuthorization_OnExitConversorA, METH_VARARGS, NULL },
	{ "CheckMaxDist", GetAuthorization_CheckMaxDistA, METH_VARARGS, NULL },
	{ "CheckHumanDist", GetAuthorization_CheckHumanDistA, METH_VARARGS, NULL },
	{ "OnMetroUsed", GetAuthorization_OnMetroUsedA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_InitEpilogue ---- */

/* InitStatus(LevelPath): Betty and Berto in the jail, Dtritus wakes up there too, the viscoson's costume */
static byte InitEpilogue_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const saves[] = { "Spawn", "DM_Mission_Dtritus_Jail", "ComeFrom", "", "CostumeAtPolice", "1", NULL };
	(void)a;
	(void)out;
	if (!LV_MISSIONCHAR("Betty_Jail", Mission_InitEpilogue, CreateBettyJail) ||
		!LV_MISSIONCHAR("Berto_Jail", Mission_InitEpilogue, CreateBertoJail)) {
		return 0;
	}
	Mode_Saves(saves);
	return 1;
}

/* a prisoner stays at origin facing ang, with his talk */
static byte Ie_Prisoner(const nativeVal_t *e, const long origin[3], long ang, const char *use) {
	return LvN_SetInts3(e, "LLogicOrigin", origin[0], origin[1], origin[2]) && LvN_SetInts3(e, "LLogicOriginAng", 0, ang, 0) &&
		LvN_Stay(e, 6, LvN_Empty()) && LvN_SetS(e, "UseFunc", use);
}

/* CreateBettyJail(e), CreateBertoJail(e) */
static byte InitEpilogue_CreateBettyJail(const nativeVal_t *a, nativeVal_t *out) {
	static const long origin[3] = { -743, -1284, 22411 };
	(void)out;
	return Ie_Prisoner(&a[0], origin, 130, "Mission_InitEpilogue.OnBettySpeech");
}

static byte InitEpilogue_CreateBertoJail(const nativeVal_t *a, nativeVal_t *out) {
	static const long origin[3] = { 364, -1284, 22524 };
	(void)out;
	return Ie_Prisoner(&a[0], origin, 210, "Mission_InitEpilogue.OnBertoSpeech");
}

/* Init(LevelPath): the crazy gambler calls in 0.25 s */
static byte InitEpilogue_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return PO_LATER(0.25, IE, "PreSetNextMission");
}

/* the talk of a prisoner: msg and ans for Talk */
static byte Ie_Speech(const nativeVal_t *a, const char *msg, const char *ans) {
	return LvN_Speakers(&a[0], &a[1]) && (!LvN_EqS(&a[2], "Talk") || Mode_Spch(msg, ans, NULL));
}

/* OnBettySpeech(UsrName, CharName, optId, FullOption), OnBertoSpeech(...) */
static byte InitEpilogue_OnBettySpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Ie_Speech(a, "Mission_DestroyHumphrey_Betty_InitMsg1", "Mission_DestroyHumphrey_Betty_InitAns1");
}

static byte InitEpilogue_OnBertoSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Ie_Speech(a, "Mission_DestroyHumphrey_Berto_InitMsg1", "Mission_DestroyHumphrey_Berto_InitAns1");
}

/* SetNextMission(): the archbishop's ship and engine, Mission_DestroyHumphrey in Outskirts, the hard enemies */
static byte InitEpilogue_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const saves[] = { "BadReputation", "5", "StdShipAIProfile", "MegaAce", "CrazyGamblerAtGambinDen", "1", NULL };
	nativeVal_t text;
	(void)a;
	(void)out;
	if (!Mode_SaveAdd(IE, "Hangar.shipsToEditList", "SArchbishop1") || !Mode_SaveAdd(IE, "Hangar.availableEnginesList", "MARCHBISHOP1")) {
		return 0;
	}
	SaveVar_Inc("NewBluePrintAvaliable", 2);
	Mode_Lang("Mission_DestroyHumphrey_SystemMessage1", &text);
	byte ok = PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text);
	Native_Free(&text);
	if (!ok || !PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_DestroyHumphrey", "Outskirts") ||
		!Mode_MMDesc("Mission_DestroyHumphrey_MissionText1", 1)) {
		return 0;
	}
	Mode_Saves(saves);
	return Mode_WayPoint();
}

LV_FN(InitEpilogue, InitStatusA, 1, InitEpilogue_InitStatus)
LV_FN(InitEpilogue, CreateBettyJailA, 1, InitEpilogue_CreateBettyJail)
LV_FN(InitEpilogue, CreateBertoJailA, 1, InitEpilogue_CreateBertoJail)
LV_FN(InitEpilogue, InitA, 1, InitEpilogue_Init)
LV_FN(InitEpilogue, OnBettySpeechA, 4, InitEpilogue_OnBettySpeech)
LV_FN(InitEpilogue, OnBertoSpeechA, 4, InitEpilogue_OnBertoSpeech)
LV_FN(InitEpilogue, SetNextMissionA, 0, InitEpilogue_SetNextMission)

PyMethodDef g_PoliceInitEpilogueMethods[] = {
	{ "InitStatus", InitEpilogue_InitStatusA, METH_VARARGS, NULL },
	{ "CreateBettyJail", InitEpilogue_CreateBettyJailA, METH_VARARGS, NULL },
	{ "CreateBertoJail", InitEpilogue_CreateBertoJailA, METH_VARARGS, NULL },
	{ "Init", InitEpilogue_InitA, METH_VARARGS, NULL },
	{ "OnBettySpeech", InitEpilogue_OnBettySpeechA, METH_VARARGS, NULL },
	{ "OnBertoSpeech", InitEpilogue_OnBertoSpeechA, METH_VARARGS, NULL },
	{ "SetNextMission", InitEpilogue_SetNextMissionA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_JailBreak ---- */

/* InitStatus(LevelPath): the NPCs of the jail after the level's own are browsed */
static byte JailBreak_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LvN_ModAppendG("CharsNPC", "AfterBrowseNPC", JB, "AfterBrowseNPC");
}

/* CharsNPC.CharRes[key].var = v (OBJLIB 5: the character objects of CharsNPC) */
static byte Jb_ResSet(const char *key, const char *var, const nativeVal_t *v) {
	nativeVal_t k, ch;
	LvN_Str(&k, key);
	byte ok = Mode_RefTake(Mode_CharRes(&k), &ch) && Mode_VSet(&ch, var, v);
	Native_Free(&k);
	Native_Free(&ch);
	return ok;
}

/* AfterBrowseNPC(): only the gatekeepers stay; Poof the stapler, the guards of the jail, Dtritus (or Berto) with no
 * lives */
static byte JailBreak_AfterBrowseNPC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const lists[] = { "Gear", "Police", "Sentinel" };
	nativeVal_t keys, v, chars, dt, d;
	(void)a;
	(void)out;
	if (!Mode_Keys("CharsNPC", "CharRes", 0, &keys)) {
		return 0;
	}
	int n = Native_ForCount(&keys);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t k, act;
		Native_SetNone(&k);
		Native_SetNone(&act);
		ok = Native_SeqItem(&keys, i, &k);
		nativeVal_t ch = { 0 };
		int other = ok && Mode_RefTake(Mode_CharRes(&k), &ch) && Mode_VGet(&ch, "ActCtrl", &act) ? !LvN_EqS(&act, "GateKeeper") : -1;
		ok = other >= 0 && (!other || Mode_ModDictDel("CharsNPC", "CharRes", &k));
		Native_Free(&ch);
		Native_Free(&k);
		Native_Free(&act);
	}
	Native_Free(&keys);
	if (!ok || !PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", "Desktop_JailBreak") ||
		!LV_CALLBACK("Desktop_JailBreak", Mission_JailBreak, CreatePuff)) {
		return 0;
	}
	Mode_Ints3(&v, 6059, -1846, 26780);
	ok = Jb_ResSet("Desktop_JailBreak", "Pos", &v);
	Native_Free(&v);
	Mode_Ints3(&v, 0, 270, 0);
	ok = ok && Jb_ResSet("Desktop_JailBreak", "Ang", &v);
	Native_Free(&v);
	for (int i = 0; ok && i < 3; i++) {
		ok = PORT_LOWN(Mission_JailBreak, CreateMCList, NULL, "s", lists[i]);
	}
	if (!ok) {
		return 0;
	}
	SaveVar_Set("Player.NumLives", "0");
	LvN_Str(&dt, "Dtritus");
	int in = LvN_ModGet("CharConversor", "AvailableChars", &chars) ? Native_Contains(&chars, &dt) : -1;
	Native_Free(&chars);
	Native_Free(&dt);
	if (in < 0) {
		return 0;
	}
	if (in) {
		SaveVar_Set("Char", "Dtritus");
	} else {
		SaveVar_Set("Char", "Berto");
		Mode_Lang("Generics_NoMatrix", &v);
		ok = Mode_SetSaveVar("InitialMessage", &v);
		Native_Free(&v);
		if (!ok) {
			return 0;
		}
	}
	LvN_Str(&d, "Desktop");
	ok = LvN_ModAppend("CharsNPC", "RespawnChars", &d);
	Native_Free(&d);
	return ok;
}

/* CreateMCList(Name): the mission characters <Name>_JailBreak_0<i> of the dummies there are (the gears stay) */
static byte JailBreak_CreateMCList(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	for (long i = 0; i < 10; i++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *rep = Mode_V(&t), *ri = Mode_V(&t), *jbn = Mode_V(&t), *dn = Mode_V(&t), *dummy = Mode_V(&t);
		const char *s;
		Native_SetInt(rep, i);
		Mode_Repr(rep, ri);
		if (!Mode_Cat(&t, jbn, 3, &a[0], Mode_S(&t, "_JailBreak_0"), ri) || !Mode_Cat(&t, dn, 2, Mode_S(&t, "DM_Mission_"), jbn) ||
			!Native_ArgStr(dn, 1, &s)) {
			return Mode_Done(&t, 0);
		}
		Mode_DummyStart(s, 0);
		if (!Mode_PrintV(jbn)) {
			return Mode_Done(&t, 0);
		}
		Mode_DummyNext(dummy);
		if (dummy->type == NV_NONE) {
			return Mode_Done(&t, 1);
		}
		byte ok = PORT_CALL(CharsNPC, CreateMissionChar, NULL, "v", jbn) && (!LvN_EqS(&a[0], "Gear") ||
			LV_CALLBACK(Native_Text(jbn), Mission_JailBreak, CreateGearStay));
		if (!Mode_Done(&t, ok)) {
			return 0;
		}
	}
	return 1;
}

/* CreateGearStay(ch): a gear guard stays */
static byte JailBreak_CreateGearStay(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_Stay(&a[0], 3, LvN_Empty());
}

/* CreatePuff(e): Poof; unless the player is a guard he starts jumping in 5 s */
static byte JailBreak_CreatePuff(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const guards[] = { "Gear", "Sentinel", "Police", "Desktop" };
	nativeVal_t usr, act;
	(void)out;
	if (!LvN_SetLang(&a[0], "Descriptor", "CharName_Poof")) {
		return 0;
	}
	Native_SetNone(&usr);
	Native_SetNone(&act);
	int c = LvN_Usr(&usr);
	if (c > 0) {
		c = Mode_VGet(&usr, "ActCtrl", &act) ? 1 : -1;
		for (int i = 0; c > 0 && i < 4; i++) {
			if (LvN_EqS(&act, guards[i])) {
				c = 0;
			}
		}
	}
	Native_Free(&usr);
	Native_Free(&act);
	return c >= 0 && (!c || PO_LATER1(5.0, JB, "InitPuffReborn", &a[0]));
}

/* InitPuffReborn(e): Poof at his place walks and turns, CheckForJump makes him jump */
static byte JailBreak_InitPuffReborn(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t k, v, name;
	(void)out;
	if (!LvN_SetI(&a[0], "LLogicDefaultState", 6) || !LvN_SetS(&a[0], "LLogicDefaultTarget", "") || !LvN_SetI(&a[0], "LLogicState", 6) ||
		!LvN_SetS(&a[0], "LLogicTarget", "") || !LvN_SetI(&a[0], "LLogicIdle", 1) || !LvN_SetI(&a[0], "LLogicAlwaysLock", 1)) {
		return 0;
	}
	LvN_Str(&k, "Desktop_JailBreak");
	nativeVal_t cr;
	byte ok = Mode_RefTake(Mode_CharRes(&k), &cr);
	Native_Free(&k);
	if (!ok) {
		return 0;
	}
	Native_SetNone(&v);
	ok = Mode_VGet(&cr, "Pos", &v) && Mode_VSet(&a[0], "Pos", &v);
	Native_Free(&v);
	ok = ok && Mode_VGet(&cr, "Ang", &v) && Mode_VSet(&a[0], "Ang", &v);
	Native_Free(&v);
	Native_Free(&cr);
	Native_SetNone(&name);
	ok = ok && LvN_SetG(&a[0], "InCtrl_Walk", JB, "PuffWalk") && LvN_SetG(&a[0], "InCtrl_HRot", JB, "PuffTurn") && Mode_VGet(&a[0], "Name", &name) &&
		PO_LATER1(2.0, JB, "CheckForJump", &name) && LvN_SetS(&a[0], "UseFunc", "Mission_JailBreak.StaplerSpeech");
	Native_Free(&name);
	return ok;
}

/* StaplerSpeech(UsrName, CharName, optId, FullOption) */
static byte JailBreak_StaplerSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_Speakers(&a[0], &a[1]) && Mode_Spch("Mission_JailBreak_Stapler_InitMsg1", "Mission_JailBreak_Stapler_InitAns1", NULL);
}

/* CheckForJump(Name): Poof walks to the window; there he presses and releases the action key (jumps) */
static byte JailBreak_CheckForJump(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e, pos, z, timy;
	(void)out;
	if (!Mode_EntityH(&a[0], &e)) {
		return 0;
	}
	if (!Native_True(&e)) {
		Native_Free(&e);
		return 1;
	}
	Native_SetNone(&pos);
	Native_SetNone(&z);
	Native_SetNone(&timy);
	int at = Mode_VGet(&e, "Pos", &pos) && LvN_Item(&pos, 2, &z) ? Mode_CmpI(&z, ">=", 27170) : -1;
	byte ok = at >= 0;
	if (at > 0) {
		int press = LvN_SetI(&e, "InCtrl_Walk", 0) && LvN_SetI(&e, "InCtrl_HRot", 0) ? LvN_GTrue(JB, "ActPress") : -1;
		if (press > 0) {
			ok = LvN_SetI(&e, "InCtrl_Act", 0) && LvN_PutI(JB, "ActPress", 0);
			Native_SetFloat(&timy, Scrap_RandRange(0.2, 5.0));
		} else if (press == 0) {
			ok = LvN_SetI(&e, "InCtrl_Act", 1) && LvN_PutI(JB, "ActPress", 1);
			Native_SetFloat(&timy, 0.1);
		} else {
			ok = 0;
		}
	} else if (at == 0) {
		Native_SetFloat(&timy, 0.5);
		ok = LvN_SetG(&e, "InCtrl_Walk", JB, "PuffWalk") && LvN_SetG(&e, "InCtrl_HRot", JB, "PuffTurn");
	}
	ok = ok && Mode_LaterV(&timy, JB, "CheckForJump", &a[0], 1, NULL);
	Native_Free(&e);
	Native_Free(&pos);
	Native_Free(&z);
	return ok;
}

/* OnPossession(name, pos): after the first possession anyone may go */
static byte JailBreak_OnPossession(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_GVarSetS("GuaranteedChars", ",Anyone,") && LvN_ModNone("CharConversor", "OnPossession");
}

/* FirstTimeJail(): the warning of the first time in the jail */
static byte JailBreak_FirstTimeJail(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t text;
	(void)a;
	(void)out;
	if (!Mode_Import("Menu")) {
		return 0;
	}
	Mode_Lang("Mission_JailBreak_Warning", &text);
	byte ok = PORT_CALL(Menu, ShowWarning, NULL, "iv", 0L, &text);
	Native_Free(&text);
	return ok;
}

/* Init(LevelPath): the jail door locked, the exit item, the policemen do not fight; the first time a warning */
static byte JailBreak_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t dmm, pos, l, first, now;
	(void)a;
	(void)out;
	if (!PO_ENTS("DM_Door_PoliceJail_00", "OnObjSlot", "!CloseDoorSlot") || !PO_HOOK("CharConversor", "OnPossession", JB, "OnPossession") ||
		!Mode_MMDesc("Mission_JailBreak_MissionText1", 0)) {
		return 0;
	}
	LvN_Dummy("DM_Mission_JailBreak_End", 0, &dmm);
	Native_SetNone(&pos);
	Native_SetNone(&l);
	byte ok = LvN_Item(&dmm, 1, &pos) && Po_Item("ExitJail", &pos, 500, 1, "Mission_JailBreak.NextMission", &l) &&
		Mode_GVarSetI("PoliceLogicBelligerant", 0);
	Native_Free(&dmm);
	Native_Free(&pos);
	Native_Free(&l);
	if (!ok) {
		return 0;
	}
	Mode_SaveVar("FirstJailBreak", &first);
	byte none = first.type == NV_NONE;
	Native_Free(&first);
	if (!none) {
		return 1;
	}
	Native_SetFloat(&now, Mode_Time());
	if (!Mode_AtG(&now, JB, "FirstTimeJail", NULL, 0)) {
		return 0;
	}
	SaveVar_Create("FirstJailBreak", "No");
	return Po_Fade(lv_black, lv_black, 2.0) && Po_FadeLater(0.1, lv_black, lv_clear, 0.5);
}

/* NextMission(ItemName, CharName): out of the jail: black, no input, the level loads again in 0.5 s */
static byte JailBreak_NextMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t x, text, i;
	Native_SetNone(&x);
	(void)out;
	if (!Mode_Import("Menu") || !Po_Fade(lv_clear, lv_black, 0.5)) {
		return 0;
	}
	Mode_SetActionSet("Inactive");
	if (!PORT_CALL(Menu, DrawScorer, NULL, "is", 0L, "Inactive") || !Native_GVarGet("IsXbox", &x)) {
		return 0;
	}
	byte pc = !Native_True(&x);
	Native_Free(&x);
	if (pc) {
		Mode_Lang("XBox_Load_Msg_1", &text);
		byte ok = PORT_CALL(SScorer, SetSpeechText, NULL, "viiii", &text, 10L, 255L, 255L, 255L);
		Native_Free(&text);
		if (!ok) {
			return 0;
		}
	}
	Native_SetNone(&i);
	byte ok = Mode_EntityH(&a[0], &i) && LvN_SetI(&i, "rTime", 10) && LvN_SetI(&i, "AppearsIfFree", 1) && Mode_VDel(&i) &&
		PO_LATER(0.5, JB, "LoadPolice");
	Native_Free(&i);
	return ok;
}

/* LoadPolice(): the mission before the jail back, the alarm off, the game starts again at the jail's exit */
static byte JailBreak_LoadPolice(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!PORT_CALL(SaveGame, MarkWayPoint, NULL, "sss", "Levels/Police", "DM_Mission_JailBreak_End", "") ||
		!Mode_SaveCopy("Mission.Map", "Mission_JailBreak.Map") || !Mode_SaveCopy("Mission.Library", "Mission_JailBreak.Library")) {
		return 0;
	}
	SaveVar_Set("AlarmActive", "0");
	SaveVar_Set("AlarmStatus", "0.0");
	return Mode_DelSaveVars("Mission_JailBreak") && PORT_CALL(SaveGame, StartGame, NULL, "");
}

LV_FN(JailBreak, InitStatusA, 1, JailBreak_InitStatus)
LV_FN(JailBreak, AfterBrowseNPCA, 0, JailBreak_AfterBrowseNPC)
LV_FN(JailBreak, CreateMCListA, 1, JailBreak_CreateMCList)
LV_FN(JailBreak, CreateGearStayA, 1, JailBreak_CreateGearStay)
LV_FN(JailBreak, CreatePuffA, 1, JailBreak_CreatePuff)
LV_FN(JailBreak, InitPuffRebornA, 1, JailBreak_InitPuffReborn)
LV_FN(JailBreak, StaplerSpeechA, 4, JailBreak_StaplerSpeech)
LV_FN(JailBreak, CheckForJumpA, 1, JailBreak_CheckForJump)
LV_FN(JailBreak, OnPossessionA, 2, JailBreak_OnPossession)
LV_FN(JailBreak, FirstTimeJailA, 0, JailBreak_FirstTimeJail)
LV_FN(JailBreak, InitA, 1, JailBreak_Init)
LV_FN(JailBreak, NextMissionA, 2, JailBreak_NextMission)
LV_FN(JailBreak, LoadPoliceA, 0, JailBreak_LoadPolice)

PyMethodDef g_PoliceJailBreakMethods[] = {
	{ "InitStatus", JailBreak_InitStatusA, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", JailBreak_AfterBrowseNPCA, METH_VARARGS, NULL },
	{ "CreateMCList", JailBreak_CreateMCListA, METH_VARARGS, NULL },
	{ "CreateGearStay", JailBreak_CreateGearStayA, METH_VARARGS, NULL },
	{ "CreatePuff", JailBreak_CreatePuffA, METH_VARARGS, NULL },
	{ "InitPuffReborn", JailBreak_InitPuffRebornA, METH_VARARGS, NULL },
	{ "StaplerSpeech", JailBreak_StaplerSpeechA, METH_VARARGS, NULL },
	{ "CheckForJump", JailBreak_CheckForJumpA, METH_VARARGS, NULL },
	{ "OnPossession", JailBreak_OnPossessionA, METH_VARARGS, NULL },
	{ "FirstTimeJail", JailBreak_FirstTimeJailA, METH_VARARGS, NULL },
	{ "Init", JailBreak_InitA, METH_VARARGS, NULL },
	{ "NextMission", JailBreak_NextMissionA, METH_VARARGS, NULL },
	{ "LoadPolice", JailBreak_LoadPoliceA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_PoliceBossCheats ---- */

/* InitStatus(LevelPath): Ficus' friend; the police boss is discardable */
static byte PoliceBossCheats_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LV_MISSIONCHAR("Police_FicusPal", Mission_PoliceBossCheats, CreatePoliceFicusPal) &&
		PORT_CALL(Chars, AddNewCharRes, NULL, "si", "PoliceBoss", 1L);
}

/* Init(LevelPath): the poker game, the scene PoliFight, the state of the mission */
static byte PoliceBossCheats_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t svv;
	byte ok = 1;
	(void)a;
	(void)out;
	if (!Mode_Import("Elevator") || !PORT_LOWN(Mission_PoliceBossCheats, CheatingLoop, NULL, "") || !Mode_DelSchedS("CrazyWind")) {
		return 0;
	}
	Mode_SaveVar("Mission_PoliceBossCheats", &svv);
	/* OBJLIB 6: LoadPoliFightScene and InitBossCheats are sections of mission_policebosscheats.def */
	if (LvN_EqS(&svv, "TalkFicusPal")) {
		ok = LV_OWN(PC, "LoadPoliFightScene", NULL, "") && Mode_Target("Police_FicusPal");
	} else if (LvN_EqS(&svv, "BossCheats")) {
		ok = LV_OWN(PC, "LoadPoliFightScene", NULL, "") && LV_OWN(PC, "InitBossCheats", NULL, "");
	} else if (LvN_EqS(&svv, "AskCrazyGambler")) {
		ok = PORT_LOWN(Mission_PoliceBossCheats, InitAskCrazyGambler, NULL, "");
	}
	Native_Free(&svv);
	return ok;
}

/* CreatePoliceFicusPal(e): Ficus' friend stays and talks */
static byte PoliceBossCheats_CreatePoliceFicusPal(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_SetS(&a[0], "UseFunc", "Mission_PoliceBossCheats.FicusPalMissionSpeech") && LvN_Stay(&a[0], 6, LvN_Empty());
}

/* FicusPalMissionSpeech(UsrName, CharName, optId, FullOption): Ficus' friend tells a policeman that the boss cheats
 * (InitBossCheats at the end); otherwise the police talk */
static byte PoliceBossCheats_FicusPalMissionSpeech(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, svv;
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	Native_SetNone(&usr);
	int police = Mode_UsrHandle(&usr) ? LvN_CtrlIs(&usr, "Police") : -1;
	Native_Free(&usr);
	if (police <= 0) {
		return police == 0 && Mode_Spch("Mission_TalkToFicusFriend_Police_InitMsg2", "Mission_TalkToFicusFriend_Police_InitAns2", NULL);
	}
	Mode_SaveVar("Mission_PoliceBossCheats", &svv);
	byte talk = LvN_EqS(&a[2], "Talk"), ok;
	if (LvN_EqS(&svv, "TalkFicusPal")) {
		/* OBJLIB 6: InitBossCheats is a section of mission_policebosscheats.def */
		ok = talk ? LvN_OnEnd(PC, "InitBossCheats", NULL) && Mode_NISpeech(NULL, "Mission_TalkToFicusFriend_Police_NI_1Msg") : LvN_RoutineCall("Police", a);
	} else if (LvN_EqS(&svv, "BossCheats") && talk) {
		ok = Mode_Spch("Mission_PhotoPoliceBoss_Police_InitMsg1", "Mission_PhotoPoliceBoss_Police_InitAns1", NULL);
	} else {
		ok = LvN_RoutineCall("Police", a);
	}
	Native_Free(&svv);
	return ok;
}

/* the poker player or his cards at the dummy: Scrap.CreateEntity(name, dummy[1][0], [1], [2], type) through its seam,
 * h.Ang = (0, dummy[2][1] + 180, 0) */
static byte Pc_AtDummy(const nativeVal_t *dummy, const nativeVal_t *name, const char *type, nativeVal_t *h) {
	nativeVal_t pos, a, a1, turn, ang;
	Native_SetNone(&pos);
	Native_SetNone(&a);
	Native_SetNone(&a1);
	Native_SetNone(&turn);
	Native_SetEmpty(&ang, NV_TUPLE);
	Native_SetInt(Native_Append(&ang), 0);
	byte ok = LvN_Item(dummy, 1, &pos) && LvN_CreateAt(name, &pos, type, h) && LvN_Item(dummy, 2, &a) && LvN_Item(&a, 1, &a1);
	if (ok) {
		Native_SetInt(&turn, 180);
		ok = Native_Arith('+', &a1, &turn, Native_Append(&ang));
	}
	if (ok) {
		Native_SetInt(Native_Append(&ang), 0);
		ok = Mode_VSet(h, "Ang", &ang);
	}
	Native_Free(&pos);
	Native_Free(&a);
	Native_Free(&a1);
	Native_Free(&ang);
	return ok;
}

/* CreateFakeChar(CharName, CharType, CardType): a poker player at his dummy with his cards: the two entities */
static byte PoliceBossCheats_CreateFakeChar(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *csp = Mode_V(&t), *csp2 = Mode_V(&t), *mv = Mode_V(&t), *ang = Mode_V(&t), *cname = Mode_V(&t);
	nativeVal_t *cls = Mode_V(&t), *pos = Mode_V(&t), *x = Mode_V(&t), *y1 = Mode_V(&t), *disp = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t);
	nativeVal_t *p2 = Mode_V(&t);
	const char *name, *type;
	if (!Native_ArgStr(&a[0], 1, &name)) {
		return Mode_Done(&t, 0);
	}
	Mode_DummyStart(name, 0);
	Mode_DummyNext(dummy);
	if (!Pc_AtDummy(dummy, &a[0], "WalkChar", csp) || !Mode_VSet(csp, "ActCtrl", &a[1]) || !Po_OnFloor(csp) || !LvN_SetF(csp, "MoveTime", 0.1) ||
		!LvN_Item(dummy, 1, mv) || !Mode_VSet(csp, "MovePos", mv) || !Mode_VGet(csp, "Ang", ang) || !Mode_VSet(csp, "MoveAng", ang) ||
		!LvN_SetI(csp, "Invulnerable", 1) || !LvN_SetS(csp, "ForceDefLoop", "StayPoker") || !LvN_SetS(csp, "ForceAction", "StayPoker") ||
		!Native_Arith('+', &a[0], Mode_S(&t, "Card"), cname) || !Pc_AtDummy(dummy, cname, "WithLife", csp2) ||
		!Mode_VSet(csp2, "ActCtrl", &a[2]) || !LvN_SetF(csp2, "MoveTime", 0.1) || !Native_ArgStr(&a[1], 1, &type) ||
		!LvN_GetClass(type, cls)) {
		return Mode_Done(&t, 0);
	}
	/* (dummy[1][0], dummy[1][1] + SAct.GetCls("YModelDisp"), dummy[1][2]) */
	Native_SetEmpty(p2, NV_TUPLE);
	if (!LvN_Item(dummy, 1, pos) || !LvN_Item(pos, 0, x)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(pos);
	if (!LvN_Item(dummy, 1, pos) || !LvN_Item(pos, 1, y1) || !PORT_CALL(SAct, GetCls, disp, "s", "YModelDisp") ||
		!Native_Arith('+', y1, disp, y)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(pos);
	if (!LvN_Item(dummy, 1, pos) || !LvN_Item(pos, 2, z)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(Native_Append(p2), x);
	Native_Copy(Native_Append(p2), y);
	Native_Copy(Native_Append(p2), z);
	Native_Free(ang);
	if (!Mode_VSet(csp2, "MovePos", p2) || !Mode_VGet(csp2, "Ang", ang) || !Mode_VSet(csp2, "MoveAng", ang) ||
		!LvN_SetS(csp2, "DefLoop", "StayPoker") || !LvN_SetS(csp2, "Action", "StayPoker") || !LvN_SetI(csp2, "Invulnerable", 1)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(out, NV_TUPLE);
	Native_Copy(Native_Append(out), csp);
	Native_Copy(Native_Append(out), csp2);
	return Mode_Done(&t, 1);
}

/* Scene.CreateLocalAction(cls, 0, name, anm, loop) for the poker actions of cls */
static byte Pc_PokerActions(const char *cls, byte brup) {
	static const struct {
		const char *name, *anm;
		long loop;
	} acts[] = { { "StayPoker", "RelaxPoker", 1 }, { "Stay0Poker", "Relax0Poker", 0 }, { "PlayPoker", "PlayPoker", 0 },
		{ "BrupPoker", "BrupPoker", 0 }, { "CheatPoker", "CheatPoker", 0 } };
	for (int i = 0; i < (brup ? 4 : 5); i++) {
		if (!PORT_CALL(Scene, CreateLocalAction, NULL, "sissi", cls, 0L, acts[i].name, acts[i].anm, acts[i].loop)) {
			return 0;
		}
	}
	return 1;
}

/* an event of the current action: SAct.SetAct AddEventAtTime, EventName, EventFunc */
static byte Pc_Event(const nativeVal_t *time, const char *name, const char *func) {
	return LvN_SetAct("AddEventAtTime", time) && Mode_SActSet(0, "EventName", name, 0) && Mode_SActSet(0, "EventFunc", func, 0);
}

static byte Pc_EventAt(double time, const char *name, const char *func) {
	nativeVal_t t;
	Native_SetFloat(&t, time);
	return Pc_Event(&t, name, func);
}

/* the event at the time of SAct.GetAct(frame) */
static byte Pc_EventFrame(const char *frame, const char *name, const char *func) {
	nativeVal_t t;
	byte ok = LvN_GetAct(frame, &t) && Pc_Event(&t, name, func);
	Native_Free(&t);
	return ok;
}

/* try: exec("import MapSnd;MapSnd.<func>()") except: Scrap.ConsoleError(msg) (OBJLIB 6: PoliceCheats and
 * PoliceBossCheats are sections of mapsnd.def) */
static byte Pc_TrySound(const char *func, const char *msg) {
	nativeVal_t m;
	if (Mode_Import("MapSnd") && LV_CALL("MapSnd", func, NULL, "")) {
		return 1;
	}
	Mode_DropError();
	LvN_Str(&m, msg);
	byte ok = Mode_ConsoleError(&m);
	Native_Free(&m);
	return ok;
}

/* CheatingLoop(): the poker table: the actions of the players and the cards with the belch events and the photo
 * window of the cheat, their sounds, the three players, the photo hook */
static byte PoliceBossCheats_CheatingLoop(const nativeVal_t *a, nativeVal_t *out) {
	static const double fx[] = { 1.21, 1.33, 1.46, 1.61, 1.77, 1.94 };
	static const char *const cards[] = { "CartasBoss", "CartasPoli" };
	static const struct {
		const char *dummy, *type, *card, *player, *cards;
	} players[] = { { "DM_Mission_Police_Dumb", "Police", "PokerCartasPoli", "Dumb2", "Dumb2c" },
		{ "DM_Mission_Police_Dumber", "Police", "PokerCartasPoli", "Dumb1", "Dumb1c" },
		{ "DM_Mission_PoliceBoss_BossCheater", "PoliceBoss", "PokerCartasBoss", "PoliceBoss", "PoliceBossc" } };
	nativeVal_t v, name, empty;
	Native_SetNone(&name);
	(void)a;
	(void)out;
	LvN_Str(&v, "Poker");
	byte ok = Mode_SetModVar("Scene", "SceneName", &v);
	Native_Free(&v);
	Native_SetEmpty(&empty, NV_LIST);
	ok = ok && LvN_ModGet("Scene", "SceneName", &name) && Mode_ModDictPut("Scene", "Actors", &name, &empty);
	Native_Free(&name);
	Native_Free(&empty);
	if (!ok || !Mode_SAct("GetClass", "PoliceBoss") || !Mode_SActSet(1, "CanBePossessed", NULL, 0) || !Pc_PokerActions("PoliceBoss", 1) ||
		!Pc_EventAt(1.10, "PoliceBossBrup", "!PoliceBossBrup")) {
		return 0;
	}
	for (int i = 0; i < 6; i++) {
		if (!Pc_EventAt(fx[i], "FXPoliceBossBrup", "!FXPoliceBossBrup")) {
			return 0;
		}
	}
	if (!PORT_CALL(Scene, CreateLocalAction, NULL, "sissi", "PoliceBoss", 0L, "CheatPoker", "CheatPoker", 0L) ||
		!Pc_EventFrame("TimeOfFrame50", "DeactivatePhoto", "Mission_PoliceBossCheats.PhotoOn") ||
		!Pc_EventFrame("TimeOfFrame140", "DeactivatePhoto", "Mission_PoliceBossCheats.PhotoOff") || !Mode_SAct("GetClass", "Police") ||
		!Pc_PokerActions("Police", 0)) {
		return 0;
	}
	for (int i = 0; i < 2; i++) {
		if (!PORT_CALL(Scene, AddLocalActor, NULL, "ss", cards[i], cards[i]) || !Pc_PokerActions(cards[i], 0) ||
			!Mode_SActSet(1, "DefAction", "StayPoker", 0) || !Mode_GVarSetDef("DefInterpTime")) {
			return 0;
		}
	}
	if (!Mode_SAct("GetClass", "Police") || !Pc_TrySound("PoliceCheats", "SOUND ERROR : MapSnd.PoliceCheats() \n") || !Mode_SAct("GetClass", "PoliceBoss") ||
		!Pc_TrySound("PoliceBossCheats", "SOUND ERROR : MapSnd.PoliceBossCheats() \n")) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		nativeVal_t r, pair[2];
		Native_SetNone(&r);
		Native_SetNone(&pair[0]);
		Native_SetNone(&pair[1]);
		ok = PORT_LOWN(Mission_PoliceBossCheats, CreateFakeChar, &r, "sss", players[i].dummy, players[i].type, players[i].card);
		if (ok) {
			/* (e, cards): a tuple of handles */
			Mode_Elems(&r);
			ok = Mode_Unpack(&r, 2, pair) && Mode_Put(PC, players[i].player, NULL, &pair[0]) && Mode_Put(PC, players[i].cards, NULL, &pair[1]);
		}
		Native_Free(&r);
		Native_Free(&pair[0]);
		Native_Free(&pair[1]);
		if (!ok) {
			return 0;
		}
	}
	return Mode_GSetI(PC, "PoliceBoss", "ArrowCanUse", 1) && PO_HOOK("CharAct", "TakenPhoto", PC, "OnPhoto");
}

/* PlayAnm(Ent1, Ent2, Action): the action of a player and of his cards */
static byte PoliceBossCheats_PlayAnm(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_VSet(&a[0], "ForceAction", &a[2]) && Mode_VSet(&a[1], "ForceAction", &a[2]);
}

/* PlayAnm(<player>, <player>c, action) of the module's players */
static byte Pc_Play(const char *player, const char *action) {
	char cards[0x20];
	nativeVal_t p, c;
	Str_Printf(cards, sizeof(cards), "%sc", player);
	Native_SetNone(&c);
	byte ok = Mode_G(PC, player, &p) && Mode_G(PC, cards, &c) && PORT_LOWN(Mission_PoliceBossCheats, PlayAnm, NULL, "vvs", &p, &c, action);
	Native_Free(&p);
	Native_Free(&c);
	return ok;
}

/* the same in delay seconds after time (now for NULL) (PokerPlay) */
static byte Pc_PlayLater(const nativeVal_t *time, double delay, const char *player, const char *action) {
	char cards[0x20];
	nativeVal_t base, d, at, args[3], name;
	Str_Printf(cards, sizeof(cards), "%sc", player);
	if (time) {
		Native_Copy(&base, time);
	} else {
		Native_SetFloat(&base, Mode_Time());
	}
	Native_SetFloat(&d, delay);
	Native_SetNone(&at);
	byte ok = Native_Arith('+', &base, &d, &at);
	Native_Free(&base);
	nativeVal_t fn = { 0 };
	ok = ok && Mode_RefG(PC, "PlayAnm", &fn);
	Native_SetNone(&args[0]);
	Native_SetNone(&args[1]);
	LvN_Str(&args[2], action);
	LvN_Str(&name, "PokerPlay");
	ok = ok && Mode_G(PC, player, &args[0]) && Mode_G(PC, cards, &args[1]) && Mode_ScheduleV(&at, &fn, args, 3, &name);
	Native_Free(&fn);
	for (int i = 0; i < 3; i++) {
		Native_Free(&args[i]);
	}
	Native_Free(&at);
	Native_Free(&name);
	return ok;
}

/* WatchEvents(FuncCall): the three players relax one after the other, then the next turn */
static byte PoliceBossCheats_WatchEvents(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t time, d, at, name;
	(void)out;
	Native_SetFloat(&time, Mode_Time());
	if (!Pc_PlayLater(&time, 5.0, "Dumb1", "Stay0Poker") || !Pc_PlayLater(&time, 5.5, "Dumb2", "Stay0Poker") ||
		!Pc_PlayLater(&time, 6.0, "PoliceBoss", "Stay0Poker")) {
		return 0;
	}
	Native_SetFloat(&d, 8.0);
	Native_SetNone(&at);
	byte ok = Native_Arith('+', &time, &d, &at);
	LvN_Str(&name, "PokerPlay");
	ok = ok && Mode_ScheduleV(&at, &a[0], NULL, 0, &name);
	Native_Free(&at);
	Native_Free(&name);
	return ok;
}

/* the turns of the poker game: two plays, then WatchEvents(next) */
static byte Pc_Turn(const char *p1, const char *a1, const char *p2, const char *a2, const char *next) {
	nativeVal_t n;
	Native_SetNone(&n);
	if (!Pc_Play(p1, a1) || !Pc_Play(p2, a2) || !Mode_G(PC, next, &n)) {
		return 0;
	}
	byte ok = PORT_LOWN(Mission_PoliceBossCheats, WatchEvents, NULL, "v", &n);
	Native_Free(&n);
	return ok;
}

/* PlayDumb1(), PlayDumb2(), PlayPoliceBoss() */
static byte PoliceBossCheats_PlayDumb1(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pc_Turn("Dumb2", "PlayPoker", "PoliceBoss", "Stay0Poker", "CheatPoliceBoss");
}

static byte PoliceBossCheats_PlayDumb2(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pc_Turn("Dumb1", "PlayPoker", "Dumb2", "Stay0Poker", "BrupPoliceBoss");
}

static byte PoliceBossCheats_PlayPoliceBoss(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pc_Turn("PoliceBoss", "PlayPoker", "Dumb2", "Stay0Poker", "PlayDumb1");
}

/* WatchEvents(next) of the module */
static byte Pc_Watch(const char *next) {
	nativeVal_t n;
	if (!Mode_G(PC, next, &n)) {
		return 0;
	}
	byte ok = PORT_LOWN(Mission_PoliceBossCheats, WatchEvents, NULL, "v", &n);
	Native_Free(&n);
	return ok;
}

/* CheatPoliceBoss(): the boss cheats (the window for the photo) */
static byte PoliceBossCheats_CheatPoliceBoss(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_GSetS(PC, "PoliceBoss", "ForceAction", "CheatPoker") && Pc_PlayLater(NULL, 0.5, "Dumb1", "CheatPoker") &&
		Pc_Play("Dumb2", "CheatPoker") && Pc_Play("PoliceBoss", "CheatPoker") && Pc_Watch("PlayDumb2");
}

/* BrupPoliceBoss(): the boss belches */
static byte PoliceBossCheats_BrupPoliceBoss(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pc_Play("PoliceBoss", "BrupPoker") && Pc_Play("Dumb1", "BrupPoker") && Pc_PlayLater(NULL, 0.5, "Dumb2", "BrupPoker") &&
		Pc_Watch("PlayPoliceBoss");
}

/* PhotoOn(EntityName, EventName, Time), PhotoOff(...): the events of the cheat open and close the photo window */
static byte PoliceBossCheats_PhotoOn(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_GSetI(PC, "PoliceBoss", "CanPhoto", 1);
}

static byte PoliceBossCheats_PhotoOff(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_GSetI(PC, "PoliceBoss", "CanPhoto", 0);
}

/* OnPhoto(SelectedEntity): the photo of the cheating boss: the game slows down, the scene follows (1); else 0 */
static byte PoliceBossCheats_OnPhoto(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t n;
	int it = LvN_GAttr(PC, "PoliceBoss", "Name", &n) ? Mode_Cmp(&a[0], "==", &n) : -1;
	Native_Free(&n);
	if (it <= 0) {
		Native_SetInt(out, 0);
		return it == 0;
	}
	if (!LvN_ModNone("CharAct", "TakenPhoto") || !Mode_DelSchedS("PokerPlay")) {
		return 0;
	}
	Mode_SetTimeSpeed(0.05);
	if (!PO_LATER(0.025, PC, "EndOfPhoto")) {
		return 0;
	}
	Mode_SetActionSet("Inactive");
	Native_SetInt(out, 1);
	return 1;
}

/* the Active of the three card entities */
static byte Pc_CardsActive(long on) {
	return Mode_GSetI(PC, "PoliceBossc", "Active", on) && Mode_GSetI(PC, "Dumb1c", "Active", on) && Mode_GSetI(PC, "Dumb2c", "Active", on);
}

/* EndOfPhoto(): the scene PoliFight, InitAfterPhoto at its end; the cards hidden */
static byte PoliceBossCheats_EndOfPhoto(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	Mode_SetActionSet("Walk");
	Mode_SetTimeSpeed(1.0);
	return Po_Fade(lv_black, lv_clear, 1.0) && PORT_CALL(Scene, Play, NULL, "s", "PoliFight") &&
		PO_HOOK("Scene", "EndOfSceneCallback", PC, "InitAfterPhoto") && Pc_CardsActive(0);
}

/* InitAfterPhoto(Usr): after the scene: white to black, InitAfterPhoto2 in 2 s */
static byte PoliceBossCheats_InitAfterPhoto(const nativeVal_t *a, nativeVal_t *out) {
	static const int white[4] = { 255, 255, 255, 255 };
	(void)out;
	if (!Mode_Import("Menu")) {
		return 0;
	}
	Mode_SetActionSet("Inactive");
	return LvN_Draw() && LvN_SetI(&a[0], "Active", 0) && Po_Fade(white, lv_black, 2.0) && PO_LATER1(2.0, PC, "InitAfterPhoto2", &a[0]);
}

/* InitAfterPhoto2(Usr): the policeman beaten: the player is Dtritus at the conversion point */
static byte PoliceBossCheats_InitAfterPhoto2(const nativeVal_t *a, nativeVal_t *out) {
	static const int to[4] = { 255, 255, 255, 0 };
	nativeVal_t un, sp;
	(void)out;
	SaveVar_Set("Char", "Dtritus");
	Native_SetNone(&un);
	Native_SetNone(&sp);
	byte ok = Mode_VGet(&a[0], "Name", &un) && Mode_G(PC, "SpawnConversionPoint", &sp) &&
		PORT_CALL(CharConversor, ConvertChar, NULL, "vvii", &un, &sp, 0L, 0L);
	Native_Free(&un);
	Native_Free(&sp);
	return ok && Po_FadeLater(1.0, lv_black, to, 2.0) && PO_LATER1(3.0, PC, "InitAfterPhoto3", &a[0]);
}

/* InitAfterPhoto3(Usr): the player back in control, a way point, Dtritus' remark, the crazy gambler next */
static byte PoliceBossCheats_InitAfterPhoto3(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t un, usr, path, spawn, sp;
	Native_SetNone(&path);
	Native_SetNone(&un);
	(void)a;
	(void)out;
	if (!Mode_Import("Init") || !Mode_Import("Menu")) {
		return 0;
	}
	Mode_SetActionSet("Walk");
	if (!LvN_Draw() || !LvN_ModGet("CharConversor", "_usrName", &un)) {
		return 0;
	}
	Native_SetNone(&usr);
	byte ok = Mode_EntityH(&un, &usr) && LvN_SetI(&usr, "Active", 1) && LvN_SetI(&usr, "MoveTime", 0) && LvN_SetI(&usr, "AutoSelect", 1);
	Native_Free(&un);
	Native_Free(&usr);
	if (!ok || !LvN_ModGet("Init", "Path", &path)) {
		return 0;
	}
	Mode_SaveVar("Spawn", &spawn);
	Native_SetNone(&sp);
	ok = Mode_G(PC, "SpawnConversionPoint", &sp) && PORT_CALL(SaveGame, MarkWayPoint, NULL, "vvv", &path, &spawn, &sp);
	Native_Free(&path);
	Native_Free(&spawn);
	Native_Free(&sp);
	return ok && PORT_CALL(Speech, RemoteComunication, NULL, "ss", "Mission_PhotoPoliceBoss_Dtritus_InitMsg1", "") &&
		PORT_LOWN(Mission_PoliceBossCheats, InitAskCrazyGambler, NULL, "") && Pc_CardsActive(1);
}

/* InitAskCrazyGambler(): Dtritus' desktop is the target, the crazy gambler talks through it */
static byte PoliceBossCheats_InitAskCrazyGambler(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("Mission_PoliceBossCheats", "AskCrazyGambler");
	return Mode_Target("DM_DTritusDesktop") && Mode_MMDesc("Mission_TalkToCrazyGambler_MissionText1", 1) && Mode_Import("DTritusDesktop") &&
		Po_DesktopLookup() && PO_HOOK("DTritusDesktop", "DialogController", PC, "CrazyGamblerSpeech");
}

/* CrazyGamblerSpeech(UsrName, CharName, optId, FullOption): the crazy gambler's talk, SetNextMission at the end */
static byte PoliceBossCheats_CrazyGamblerSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_Import("DTritusDesktop") && Mode_Target("") && LvN_ModNone("DTritusDesktop", "DialogController") && Po_DesktopLookup() &&
		LvN_Speakers(&a[0], &a[1]) && LV_ONEND(Mission_PoliceBossCheats, SetNextMission, NULL) &&
		Mode_NISpeech("CrazyGambler", "Mission_TalkToCrazyGambler_ComCrazyGambler_NI_1Msg");
}

/* SetNextMission(): where the important characters are, the mission of NextMissionData */
static byte PoliceBossCheats_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const saves[] = { "MayorAtTownHall", "0", "PoliceBossAtPolice", "0", "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut",
		"Press", "Journalist.Betty_Defaut", "TownHall", "Journalist.Berto_Defaut", "TownHall", NULL };
	(void)a;
	(void)out;
	Mode_Saves(saves);
	return Po_TargetNone() && Mode_ApplyNext(PC) && Mode_MMDesc("Mission_GetInfoClues_MissionText1", 1) && Mode_DelSaveVars("Mission_PoliceBossCheats");
}

LV_FN(PoliceBossCheats, InitStatusA, 1, PoliceBossCheats_InitStatus)
LV_FN(PoliceBossCheats, InitA, 1, PoliceBossCheats_Init)
LV_FN(PoliceBossCheats, CreatePoliceFicusPalA, 1, PoliceBossCheats_CreatePoliceFicusPal)
LV_FN(PoliceBossCheats, FicusPalMissionSpeechA, 4, PoliceBossCheats_FicusPalMissionSpeech)
LV_FN(PoliceBossCheats, CreateFakeCharA, 3, PoliceBossCheats_CreateFakeChar)
LV_FN(PoliceBossCheats, CheatingLoopA, 0, PoliceBossCheats_CheatingLoop)
LV_FN(PoliceBossCheats, PlayAnmA, 3, PoliceBossCheats_PlayAnm)
LV_FN(PoliceBossCheats, WatchEventsA, 1, PoliceBossCheats_WatchEvents)
LV_FN(PoliceBossCheats, PlayDumb1A, 0, PoliceBossCheats_PlayDumb1)
LV_FN(PoliceBossCheats, CheatPoliceBossA, 0, PoliceBossCheats_CheatPoliceBoss)
LV_FN(PoliceBossCheats, PlayDumb2A, 0, PoliceBossCheats_PlayDumb2)
LV_FN(PoliceBossCheats, BrupPoliceBossA, 0, PoliceBossCheats_BrupPoliceBoss)
LV_FN(PoliceBossCheats, PlayPoliceBossA, 0, PoliceBossCheats_PlayPoliceBoss)
LV_FN(PoliceBossCheats, PhotoOnA, 3, PoliceBossCheats_PhotoOn)
LV_FN(PoliceBossCheats, PhotoOffA, 3, PoliceBossCheats_PhotoOff)
LV_FN(PoliceBossCheats, OnPhotoA, 1, PoliceBossCheats_OnPhoto)
LV_FN(PoliceBossCheats, EndOfPhotoA, 0, PoliceBossCheats_EndOfPhoto)
LV_FN(PoliceBossCheats, InitAfterPhotoA, 1, PoliceBossCheats_InitAfterPhoto)
LV_FN(PoliceBossCheats, InitAfterPhoto2A, 1, PoliceBossCheats_InitAfterPhoto2)
LV_FN(PoliceBossCheats, InitAfterPhoto3A, 1, PoliceBossCheats_InitAfterPhoto3)
LV_FN(PoliceBossCheats, InitAskCrazyGamblerA, 0, PoliceBossCheats_InitAskCrazyGambler)
LV_FN(PoliceBossCheats, CrazyGamblerSpeechA, 4, PoliceBossCheats_CrazyGamblerSpeech)
LV_FN(PoliceBossCheats, SetNextMissionA, 0, PoliceBossCheats_SetNextMission)

PyMethodDef g_PolicePoliceBossCheatsMethods[] = {
	{ "InitStatus", PoliceBossCheats_InitStatusA, METH_VARARGS, NULL },
	{ "Init", PoliceBossCheats_InitA, METH_VARARGS, NULL },
	{ "CreatePoliceFicusPal", PoliceBossCheats_CreatePoliceFicusPalA, METH_VARARGS, NULL },
	{ "FicusPalMissionSpeech", PoliceBossCheats_FicusPalMissionSpeechA, METH_VARARGS, NULL },
	{ "CreateFakeChar", PoliceBossCheats_CreateFakeCharA, METH_VARARGS, NULL },
	{ "CheatingLoop", PoliceBossCheats_CheatingLoopA, METH_VARARGS, NULL },
	{ "PlayAnm", PoliceBossCheats_PlayAnmA, METH_VARARGS, NULL },
	{ "WatchEvents", PoliceBossCheats_WatchEventsA, METH_VARARGS, NULL },
	{ "PlayDumb1", PoliceBossCheats_PlayDumb1A, METH_VARARGS, NULL },
	{ "CheatPoliceBoss", PoliceBossCheats_CheatPoliceBossA, METH_VARARGS, NULL },
	{ "PlayDumb2", PoliceBossCheats_PlayDumb2A, METH_VARARGS, NULL },
	{ "BrupPoliceBoss", PoliceBossCheats_BrupPoliceBossA, METH_VARARGS, NULL },
	{ "PlayPoliceBoss", PoliceBossCheats_PlayPoliceBossA, METH_VARARGS, NULL },
	{ "PhotoOn", PoliceBossCheats_PhotoOnA, METH_VARARGS, NULL },
	{ "PhotoOff", PoliceBossCheats_PhotoOffA, METH_VARARGS, NULL },
	{ "OnPhoto", PoliceBossCheats_OnPhotoA, METH_VARARGS, NULL },
	{ "EndOfPhoto", PoliceBossCheats_EndOfPhotoA, METH_VARARGS, NULL },
	{ "InitAfterPhoto", PoliceBossCheats_InitAfterPhotoA, METH_VARARGS, NULL },
	{ "InitAfterPhoto2", PoliceBossCheats_InitAfterPhoto2A, METH_VARARGS, NULL },
	{ "InitAfterPhoto3", PoliceBossCheats_InitAfterPhoto3A, METH_VARARGS, NULL },
	{ "InitAskCrazyGambler", PoliceBossCheats_InitAskCrazyGamblerA, METH_VARARGS, NULL },
	{ "CrazyGamblerSpeech", PoliceBossCheats_CrazyGamblerSpeechA, METH_VARARGS, NULL },
	{ "SetNextMission", PoliceBossCheats_SetNextMissionA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Police)
