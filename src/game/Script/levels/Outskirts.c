/*
 * The scripts of the level Outskirts in C (levels/outskirts/scripts of data01.packed, issue #2): the Map module (the
 * traffic, the metro and the three elevators), MapRace (the start positions of the eleven races) and the logic of the
 * missions of the level (Mission_AttackTraitorius, Mission_Demo_HumphreyRace, Mission_DestroyBadDebtors,
 * Mission_DestroyCommSats1 (in levels/LevelCommon.c), Mission_DestroyHumphrey, Mission_FunctionaryHunting, Mission_GoToOutskirts,
 * Mission_IdentifyMerc, Mission_KillPolice, Mission_KillRudolph, Mission_TransportKiller, Mission_Win2ndRace). MapSnd
 * and the plain functions of the missions are data (data/levels/outskirts/<module>.def). The module variables of the
 * missions and the module code (the save variables they create, MapRace.Races) stay in the scripts; the functions read
 * and write them as their globals.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h) and call the engine in C
 * (save variables, Scrap.Get, the alarm, the entity lists, the scorer items, the vector math, the money). The variables
 * of the modules are read where the script reads them and written through at once; the lists and dicts of the racers,
 * the police ships, the bankers, the mercenaries, the treasures and the police friends are changed in place on the
 * objects of the modules (OBJLIB 5). The ships, the items, the phone cabs and the cameras are handle values, the
 * profiles VehiclesNPC.cShipAI instances (OBJLIB 7). The engine functions the cases record or swap go through their
 * seams (OBJLIB 7): Scrap.AddScheduledFunc, DeleteScheduledFuncs, DelSaveVars, CreateEntity, Set, SetAlarm, SetLockAlarm,
 * UsrEntity, Rand, GetAlarm, TestLine and Preload3DObject, SAI.GetStateVehicle, SLogic.SetOnFloor, SScorer.Set and
 * SetMsgText and SAct. By name on purpose (OBJLIB 7): SAI.SetStateVehicle (its state handlers take Python arguments), the calls the
 * cases record with _bon (SScorer.CancelSpeech, AddModel, Scrap.IncSaveVar), the camera (Scrap.GetCam, SetCam),
 * SAct.GetClass of an entity's ActCtrl, SLogic.IsEnemyActive, Scrap.CreateEntityList and SInput.GetDefinedEntry. Speech,
 * MissionsFuncs, SaveGame, RaceMaker, RacerScorer, VehiclesNPC, Vehicles, PhoneCab, Parking, Elevator, Metro, Traffic,
 * Items, Scene, Menu, Scorer, OutMap, XBMenu, string and the functions of the modules go by name (OBJLIB 3); the
 * keyword calls (RaceMaker.InitRace, Speech.AddRemoteCom with InsertFirst, Speech.AutoAddNI with HeadName) stay
 * Python calls.
 */
#include <limits.h>

#include "Levels.h"

#define AT "Mission_AttackTraitorius"
#define DH "Mission_Demo_HumphreyRace"
#define BD "Mission_DestroyBadDebtors"
#define HU "Mission_DestroyHumphrey"
#define FH "Mission_FunctionaryHunting"
#define GO "Mission_GoToOutskirts"
#define IM "Mission_IdentifyMerc"
#define KP "Mission_KillPolice"
#define KR "Mission_KillRudolph"
#define TK "Mission_TransportKiller"
#define WR "Mission_Win2ndRace"

/* the ships of the traffic of the missions (Traffic.AcShips as the save variable holds it) */
static const char *const ships_police5 = "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SBanker1\",\"SBankMaster1\"]";
static const char *const ships_police4 = "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SBanker1\"]";
static const char *const ships_demo = "[\"SMerc2\", \"SMerc1\", \"SBanker1\"]";
static const char *const ships_all = "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SMerc3\",\"SLMerc\",\"SMayor1\",\"SBanker1\",\"SBishop1\","
	"\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\",\"SBankMaster1\",\"SArchbishop1\"]";

/* ---- native lines of the level (Levels.h) ---- */

/* a value of the function's modeVals_t t (None) */
#define TV() Mode_V(&t)

/* the gates of the level closed: the city doors of the hangars and the doors to the other outdoor levels */
static byte Os_CloseDoors(void) {
	static const char *const doors[] = { "DM_Door_CityDoorA_00", "DM_Door_CityDoorA_01", "DM_Door_Outmap_Commercial",
		"DM_Door_Outmap_DownTown", "DM_Door_Outmap_Scrapyard", NULL };
	byte ok = 1;
	for (int i = 0; ok && doors[i]; i++) {
		nativeVal_t e = { 0 };
		ok = LvN_Entity(doors[i], &e) && (!Native_True(&e) || (LvN_SetS(&e, "Action", "Close") && LvN_SetS(&e, "DefLoop", "Closed") &&
			LvN_SetS(&e, "OnObjSlot", "!CloseDoorSlot")));
		Native_Free(&e);
	}
	return ok;
}

/* the metro entrance usable again: ArrowCanUse 1, UseFunc use (the entrance into e) */
static byte Os_Metro(const char *use, nativeVal_t *e) {
	return LvN_Entity("DM_Door_MetroExt_", e) && LvN_SetI(e, "ArrowCanUse", 1) && LvN_SetS(e, "UseFunc", use);
}

/* the NI talk at the cab: Speech.OnEndOfSpeech = the global end of module, no parameters, who says key */
static byte Os_PhoneTalk(const char *module, const char *end, const char *who, const char *key) {
	return LvN_OnEnd(module, end, NULL) && Mode_NISpeech(who, key);
}

/* Speech.StartObjSpch(pc, key, who) with pc the global of module */
static byte Os_PoliceTalk(const char *module, const char *key, const char *who) {
	nativeVal_t pc = { 0 };
	byte ok = Mode_G(module, "pc", &pc) && LV_CALL("Speech", "StartObjSpch", NULL, "vss", &pc, key, who);
	Native_Free(&pc);
	return ok;
}

/* the cab of the global pc of module back to the default use (reset) and no target any more */
static byte Os_PcDone(const char *module, byte reset) {
	nativeVal_t pc = { 0 };
	byte ok = Mode_G(module, "pc", &pc) && (!reset || LvN_SetS(&pc, "UseFunc", "PhoneCab.DefUseAudioCom")) && Mode_Read(module, "pc", &pc) &&
		LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &pc, 0L);
	Native_Free(&pc);
	return ok;
}

/* the next entity of an entity list (the loops over a list): name = e.NextInList, e = its entity */
static byte Os_Next(nativeVal_t *name, nativeVal_t *e) {
	nativeVal_t next = { 0 };
	byte ok = Mode_VGet(e, "NextInList", &next);
	Native_Free(name);
	Native_Free(e);
	*name = next;
	return ok && Mode_EntityH(name, e);
}

/* ---- Map ---- */

/* module.var = [a, b, c] of three texts */
static byte Os_ModTexts3(const char *module, const char *var, const char *a, const char *b, const char *c) {
	const char *const texts[] = { a, b, c };
	nativeVal_t l = { 0 };
	Mode_Texts(&l, texts, 3);
	byte ok = Mode_SetModVar(module, var, &l);
	Native_Free(&l);
	return ok;
}

/* InitStatus(LevelPath): outdoors; the traffic by who controls the robots, the metro and the elevator to the level
 * the player came from */
static byte OsMap_InitStatus(void) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "OutPolice", "string", "Metro", "Traffic", NULL };
	nativeVal_t prev = { 0 }, tail = { 0 }, pm = { 0 };
	byte ok = 1;
	for (int i = 0; ok && mods[i]; i++) {
		ok = Mode_Import(mods[i]);
	}
	ok = ok && LvN_ModSetI("Init", "Outdoor", 1);
	if (ok && LvN_SaveIs("RobotsControlledByBoss", "1")) {
		ok = Mode_BossTraffic(1);
	} else if (ok) {
		ok = Mode_GVarSetI("iTrafficNumber", 17) && Mode_GVarSetI("MaxChasingShips", -1) &&
			Os_ModTexts3("Traffic", "AcNormalShips", "TLight1", "TLight2", "TLight3") && LvN_ModSetI("Traffic", "NumNormal", 80) &&
			Os_ModTexts3("Traffic", "AcMediumShips", "TMed1", "TMed2", "TMed3") && LvN_ModSetI("Traffic", "NumMedium", 40) &&
			Mode_ModEmpty("Traffic", "AcHeavyShips") && LvN_ModSetI("Traffic", "NumHeavy", 0);
	}
	ok = ok && LV_CALL("Elevator", "SetDef", NULL, "ss", "Metro", "DM_Door_MetroExt_") &&
		LV_CALL("Elevator", "SetDef", NULL, "ss", "MetroExt", "DM_Door_MetroExt_");
	if (ok) {
		Mode_SaveVar("PrevMap", &prev);
		ok = Native_Slice(&prev, 7, INT_MAX, &tail) && Native_Upper(&tail, &pm) &&
			LV_CALL("Elevator", "SetDef", NULL, "ss", "Elevator", LvN_EqS(&pm, "BANK") ? "DM_Door_Elevator_Bank" :
				LvN_EqS(&pm, "POLICE") ? "DM_Door_Elevator_Police" : "DM_Door_Elevator_GamblinDen");
	}
	Native_Free(&prev);
	Native_Free(&tail);
	Native_Free(&pm);
	return ok;
}

/* Init(LevelPath): the colour of the clouds, the elevators to Bank, GamblinDen and Police, the metro */
static byte OsMap_Init(void) {
	return Mode_Import("Elevator") && Mode_Import("Metro") && Mode_Import("Init") && Mode_GVarSetI("CameraCloudRed", 120) &&
		Mode_GVarSetI("CameraCloudGreen", 60) && Mode_GVarSetI("CameraCloudBlue", 0) && Mode_Connect("DM_Door_Elevator_Bank", "Bank", 0) &&
		Mode_Connect("DM_Door_Elevator_GamblinDen", "GamblinDen", 0) && Mode_Connect("DM_Door_Elevator_Police", "Police", 0) &&
		LV_CALL("Metro", "SetAsMetro", NULL, "s", "DM_Door_MetroExt_");
}

LV_CORE0(OsMap_InitStatus)
LV_CORE0(OsMap_Init)
LV_FN(OsMap, InitStatusPy, 1, OsMap_InitStatus_C)
LV_FN(OsMap, InitPy, 1, OsMap_Init_C)

PyMethodDef g_OutskirtsMapMethods[] = {
	{ "InitStatus", OsMap_InitStatusPy, 1, NULL },
	{ "Init", OsMap_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapRace ---- */

#include "OutskirtsRaces.inc"

#define RACE(fn, k) \
	static PyObject *fn(PyObject *self, PyObject *args) { \
		(void)self; \
		return Lv_StartPos(args, races[k].pos, races[k].n); \
	}
RACE(MapRace_Easy1, 0)
RACE(MapRace_Easy2, 1)
RACE(MapRace_Easy3, 2)
RACE(MapRace_Easy4, 3)
RACE(MapRace_Easy5, 4)
RACE(MapRace_Medium1, 5)
RACE(MapRace_Medium2, 6)
RACE(MapRace_Medium3, 7)
RACE(MapRace_Medium4, 8)
RACE(MapRace_Medium5, 9)
RACE(MapRace_Win2nd, 10)

PyMethodDef g_OutskirtsMapRaceMethods[] = {
	{ "EasyRaceOurskirts1", MapRace_Easy1, 1, NULL },
	{ "EasyRaceOurskirts2", MapRace_Easy2, 1, NULL },
	{ "EasyRaceOurskirts3", MapRace_Easy3, 1, NULL },
	{ "EasyRaceOurskirts4", MapRace_Easy4, 1, NULL },
	{ "EasyRaceOurskirts5", MapRace_Easy5, 1, NULL },
	{ "MediumRaceOurskirts1", MapRace_Medium1, 1, NULL },
	{ "MediumRaceOurskirts2", MapRace_Medium2, 1, NULL },
	{ "MediumRaceOurskirts3", MapRace_Medium3, 1, NULL },
	{ "MediumRaceOurskirts4", MapRace_Medium4, 1, NULL },
	{ "MediumRaceOurskirts5", MapRace_Medium5, 1, NULL },
	{ "Mission_Win2ndRace_Race", MapRace_Win2nd, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyCommSats1: levels/LevelCommon.c ---- */

/* ---- Mission_Win2ndRace: the great race of the crazy gambler against four racers through 20 checkpoints ---- */

static PyObject *Win2nd_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, WR, ships_police5);
}

/* Init(LevelPath): the countdown resources, the race profile, the alarm hooks, the race (unless the alarm is on) */
static byte Win2nd_Init(void) {
	nativeVal_t sp = { 0 }, prof = { 0 };
	int alarm = Mode_Import("Alarm") && Mode_Import("RaceMaker") && Mode_Import("VehiclesNPC") &&
		LV_CALL("RaceMaker", "PreloadCnDnResources", NULL, "") && LV_CALL("VehiclesNPC", "cShipAI", &sp, "") && Mode_G(WR, "RACE_PROFILE", &prof) &&
		LV_CALL("VehiclesNPC", "SetAIProfile", NULL, "vv", &sp, &prof) && LvN_ModSetGlobal("Alarm", "TempAbortMissionCallback", WR, "OnAlarmOn") &&
		LvN_ModSetGlobal("Alarm", "TempRestoreMissionCallback", WR, "OnAlarmOff") ? Mode_AlarmOn() : -1;
	Native_Free(&sp);
	Native_Free(&prof);
	return alarm >= 0 && LV_OWN(WR, alarm ? "OnAlarmOn" : "InitRace", NULL, "");
}

/* RaceMaker.StartPos[the global index of the mission][item] */
static byte Wr_Start(const char *index, long item, nativeVal_t *out) {
	nativeVal_t sp = { 0 }, i = { 0 }, p = { 0 };
	byte ok = LvN_ModGet("RaceMaker", "StartPos", &sp) && Mode_G(WR, index, &i) && Native_Subscript(&sp, &i, &p) && LvN_Item(&p, item, out);
	Native_Free(&sp);
	Native_Free(&i);
	Native_Free(&p);
	return ok;
}

/* (0, RaceMaker.StartPos[iStartPos][1], 0) */
static byte Wr_StartAng(nativeVal_t *out) {
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), 0);
	byte ok = Wr_Start("iStartPos", 1, Native_Append(out));
	Native_SetInt(Native_Append(out), 0);
	return ok;
}

/* InitRace(): the racers around the first checkpoint the player cannot see, the start buoy and its trigger */
static byte Win2nd_InitRace(void) {
	static const char *const mods[] = { "Alarm", "Menu", "Speech", "Parking", "RaceMaker", "RacerScorer", "MapRace", "VehiclesNPC",
		"math", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *lst = TV(), *idx = TV(), *l = TV(), *ang = TV(), *pos = TV(), *name = TV(), *l2 = TV(), *wn = TV();
	byte ok = 1;
	for (int i = 0; ok && mods[i]; i++) {
		ok = Mode_Import(mods[i]);
	}
	Native_SetEmpty(lst, NV_LIST);
	ok = ok && Mode_Put(WR, "racers", NULL, lst);
	Native_Free(lst);
	Native_SetEmpty(lst, NV_DICT);
	ok = ok && Mode_Put(WR, "racersDict", NULL, lst) && Mode_Read(WR, "racersList", lst);
	int numply = ok ? Native_Size(lst) : -1;
	ok = numply >= 0 && LvN_PutI(WR, "iStartPos", 0) && Mode_G(WR, "iStartPos", idx) && Mode_Put(WR, "nextStartPos", NULL, idx);
	Native_Free(lst);
	Native_SetEmpty(lst, NV_DICT);
	ok = ok && Mode_Put(WR, "BayScorer", NULL, lst) && LvN_PutI(WR, "YourScore", -1) && LvN_PutI(WR, "NumOponNotGoNextItem", 0) &&
		Mode_ListSet("Race", "OnDeath", "Mission_Win2ndRace.RacerKilled") && Mode_ListSet("Race", "OnDamage", "Mission_Win2ndRace.RacerDamaged") &&
		LV_CALL("MapRace", "Mission_Win2ndRace_Race", NULL, "") && LV_OWN(WR, "GetRaceWaypointIndex", idx, "") && Mode_Put(WR, "iStartPos", NULL, idx);
	for (int i = 0; ok && i < numply; i++) {
		int mark = t.n;
		nativeVal_t *p0 = TV(), *a1 = TV(), *base = TV(), *rot = TV(), *r = TV(), *spos = TV(), *rep = TV(), *vn = TV(), *v = TV(), *rl = TV();
		nativeVal_t *rname = TV(), *prof = TV(), *ship = TV(), *zero3 = TV(), *vp = TV(), *pd = TV(), *gn = TV();
		ok = Wr_Start("iStartPos", 0, p0) && Wr_Start("iStartPos", 1, a1);
		if (ok) {
			Mode_Ints3(base, 0, 2200, 0);
			Native_SetEmpty(rot, NV_TUPLE);
			Native_SetInt(Native_Append(rot), 0);
			Native_Copy(Native_Append(rot), a1);
			Native_SetInt(Native_Append(rot), 90 + 360 * i / numply);
			Native_SetInt(rep, i);
			Mode_Repr(rep, r);
			ok = Mode_Rotate(base, rot, rep) && Mode_VecAdd(p0, rep, spos) && Native_Arith('+', Mode_S(&t, "BadBoy"), r, vn) &&
				LvN_CreateAt(vn, spos, "Car", v) && Mode_G(WR, "racersList", rl) && LvN_Item(rl, i, rname) && Mode_G(WR, "RACE_PROFILE", prof) &&
				LV_CALL("RaceMaker", "InitShipIAProfile", NULL, "vv", rname, prof) && Mode_Read(WR, "racersList", rl) &&
				(Native_Free(rname), LvN_Item(rl, i, rname)) && Mode_Read(WR, "RACE_PROFILE", prof) &&
				LV_CALL("RaceMaker", "GetRandomShipIAProfile", ship, "vv", rname, prof) && (Native_Free(vn), Mode_VGet(v, "Name", vn)) &&
				Mode_DictPut(WR, "racersDict", NULL, vn, ship) && LV_CALL("VehiclesNPC", "SetIAProf", NULL, "vv", v, ship) &&
				LV_CALL("SAI", "SetStateVehicle", NULL, "iv", 0L, vn) && LvN_SetS(v, "TgType", "None") && LvN_SetS(v, "EntityList", "Race") &&
				Wr_StartAng(zero3) && Mode_VSet(v, "Ang", zero3) && Mode_VGet(v, "Pos", vp) && Mode_VSet(v, "MovePos", vp) &&
				(Native_Free(vp), Mode_VGet(v, "Ang", vp)) && Mode_VSet(v, "MoveAng", vp) && LvN_SetF(v, "MoveTime", 0.001) &&
				Mode_Append(WR, "racers", vn) && Mode_DictPut(WR, "BayScorer", NULL, vn, Mode_I(&t, 0)) &&
				LV_CALL("VehiclesNPC", "GetIAProf", pd, "v", ship) && LvN_SetI(pd, "StupidPathFinding", 0) &&
				LV_CALL("VehiclesNPC", "ModAIProff", NULL, "vv", v, pd) && Mode_VGet(pd, "GoNextItemRace", gn) &&
				(!LvN_EqI(gn, 0) || LvN_GAdd(WR, "NumOponNotGoNextItem", 1, NULL));
		}
		Mode_Release(&t, mark);
	}
	ok = ok && Wr_Start("iStartPos", 0, pos) && LvN_CreateAt(Mode_S(&t, "CheckPoint"), pos, "Item", l) &&
		Mode_VModel(l, Mode_S(&t, "models/Misc/Wayp/Wayp.m3d")) && LvN_SetS(l, "OnTake", "Mission_Win2ndRace.OnRaceStart") &&
		LvN_SetI(l, "RadarShow", 1) && LvN_SetI(l, "Radius", 2400) && Wr_StartAng(ang) && Mode_VSet(l, "Ang", ang) && Mode_VGet(l, "Name", name) &&
		Native_Arith('+', name, Mode_S(&t, "_Trigg"), wn) && (Native_Free(pos), Wr_Start("iStartPos", 0, pos)) && LvN_CreateAt(wn, pos, "Item", l2) &&
		(Native_Free(pos), Mode_VGet(l, "Pos", pos)) && Mode_VSet(l2, "Pos", pos) && LvN_SetI(l2, "Radius", 20000) &&
		Mode_Preload(Mode_S(&t, "models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d")) && (Native_Free(name), Mode_VGet(l, "Name", name)) &&
		Mode_Put(WR, "waypointName", NULL, name) && Mode_Read(WR, "waypointName", name) && Mode_GVarSet("CurrentWaypointName", name);
	return Mode_Done(&t, ok);
}

/* OnCounterMeasure(id=0): the first countermeasure warns, another one more than 5 s later cancels the race */
static byte Win2nd_OnCounterMeasure(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *w = TV(), *last = TV(), *now = TV(), *sum = TV();
	byte ok = Mode_Import("Speech") && Mode_Import("RaceMaker") && Mode_UsrHandle(usr);
	int skip = !ok ? -1 : usr->type == NV_NONE ? 1 : Mode_VGet(usr, "iWeap", w) ? LvN_EqI(w, 6) : -1;
	if (skip != 0) {
		return Mode_Done(&t, skip > 0);
	}
	ok = Mode_G(WR, "LastCmTime", last);
	if (ok && LvN_EqI(last, -1)) {
		Native_SetFloat(now, Mode_Time());
		ok = Mode_Put(WR, "LastCmTime", NULL, now) && LvN_Com("Generics_PoliceRace_1", "Police", NULL, NULL, NULL);
	} else if (ok) {
		nativeVal_t five;
		Native_SetFloat(&five, 5.0);
		ok = Native_Arith('+', last, &five, sum);
		Native_SetFloat(now, Mode_Time());
		if (ok && Native_Compare(sum, now) < 0) {
			ok = LvN_Com("Generics_PoliceRace_2", "Police", NULL, NULL, NULL) && LV_OWN(WR, "CancelMission", NULL, "") &&
				LvN_Com("Mission_Win2ndRace_RemoteMessage2", "Trurl", WR, "InitRace", LvN_NoArgs());
		}
	}
	return Mode_Done(&t, ok);
}

/* GetRaceWaypointIndex(): the first checkpoint of the race the player cannot see (TestLine hits the world), else 0 */
static byte Win2nd_GetRaceWaypointIndex_C(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *sp = TV(), *lst = TV();
	(void)a;
	if (!Mode_Import("RaceMaker") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	byte ok = LvN_ModGet("RaceMaker", "StartPos", sp);
	if (ok) {
		LvN_Elems(sp, lst);
	}
	int cnt = ok ? Native_ForCount(lst) : -1;
	ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t p = { 0 }, up = { 0 }, p0 = { 0 }, res = { 0 }, r1 = { 0 }, empty = { 0 };
		LvN_Str(&empty, "");
		ok = Native_SeqItem(lst, k, &p) && Mode_VGet(usr, "Pos", &up) && LvN_Item(&p, 0, &p0) && Mode_TestLineSeam(&up, &p0, &empty, &res);
		int hit = ok && Native_True(&res) ? (LvN_Item(&res, 1, &r1) ? LvN_EqS(&r1, "") : -1) : 0;
		Native_Free(&p);
		Native_Free(&up);
		Native_Free(&p0);
		Native_Free(&res);
		Native_Free(&r1);
		Native_Free(&empty);
		if (hit) {
			if (hit > 0) {
				Native_SetInt(out, k);
			}
			return Mode_Done(&t, hit > 0);
		}
	}
	if (ok) {
		Native_SetInt(out, 0);
	}
	return Mode_Done(&t, ok);
}

/* RacerDamaged(LooserName, WinnerName, Damage): the player hits a racer before the start: the race starts */
static byte Win2nd_RacerDamaged(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *name = TV(), *ys = TV(), *wn = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	byte ok = Mode_VGet(usr, "Name", name);
	if (ok && !Native_Compare(name, &a[1])) {
		ok = Mode_G(WR, "YourScore", ys);
		if (ok && LvN_EqI(ys, -1)) {
			ok = Mode_ListSet("Race", "OnDamage", "") && Mode_G(WR, "waypointName", wn) && LV_OWN(WR, "OnCheckPoint", NULL, "vv", wn, &a[1]);
		}
	}
	return Mode_Done(&t, ok);
}

/* OnRaceStart(ItemName, VehicleName): the player at the start buoy: the camera, the countdown, the start */
static byte Win2nd_OnRaceStart(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *tm = TV(), *args = TV(), *a2 = TV();
	int c = Mode_Import("RaceMaker") && Mode_UsrHandle(usr) ? LvN_UsrNameIs(usr, &a[1]) : -1;
	byte ok = c >= 0;
	if (c > 0) {
		Native_Copy(args, &a[0]);
		Native_Copy(a2, &a[1]);
		ok = LV_CALL("RaceMaker", "StartCameraTravelling", tm, "vv", &a[0], &a[1]) && Mode_LaterV(tm, WR, "RaceGo", args, 2, NULL) &&
			LV_CALL("RaceMaker", "CreateRaceCountDown", NULL, "");
	}
	return Mode_Done(&t, ok);
}

/* the race ends for the player: no scorer */
static byte Wr_NoScorer(const nativeVal_t *l) {
	return Mode_VDel(l) && LvN_ScorerOff() && LvN_Draw();
}

/* Scrap.AddScheduledFunc(at, SAI.SetStateVehicle, args) of n values */
static byte Wr_SchedState(const nativeVal_t *at, const nativeVal_t *args, int n) {
	PyObject *fn = Script_Attr("SAI", "SetStateVehicle");
	byte ok = fn && Mode_Schedule(at, fn, args, n, NULL);
	Py_XDECREF(fn);
	return ok;
}

/* (6, name, l.Pos[0], l.Pos[1], l.Pos[2], l.Radius, pd.ControlBoostRace) into the 7 values at args */
static byte Wr_StateArgs(const nativeVal_t *name, const nativeVal_t *l, const nativeVal_t *pd, nativeVal_t *args) {
	nativeVal_t pos = { 0 };
	Native_SetInt(&args[0], 6);
	Native_Copy(&args[1], name);
	byte ok = Mode_VGet(l, "Pos", &pos) && LvN_Item(&pos, 0, &args[2]) && LvN_Item(&pos, 1, &args[3]) && LvN_Item(&pos, 2, &args[4]) &&
		Mode_VGet(l, "Radius", &args[5]) && Mode_VGet(pd, "ControlBoostRace", &args[6]);
	Native_Free(&pos);
	return ok;
}

/* racersDict[name] of the mission's dict into out */
static byte Wr_Racer(const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t d = { 0 };
	byte ok = Mode_G(WR, "racersDict", &d) && Native_Subscript(&d, name, out);
	Native_Free(&d);
	return ok;
}

/* (iStartPos + 1) % len(RaceMaker.StartPos) */
static byte Wr_NextIndex(nativeVal_t *out) {
	nativeVal_t i = { 0 }, i1 = { 0 }, sp = { 0 }, n = { 0 };
	int len = Mode_G(WR, "iStartPos", &i) && Native_Arith('+', &i, &(nativeVal_t){ .type = NV_INT, .i = 1 }, &i1) &&
		LvN_ModGet("RaceMaker", "StartPos", &sp) ? Native_Size(&sp) : -1;
	byte ok = len >= 0;
	if (ok) {
		Native_SetInt(&n, len);
		ok = Mode_Arith('%', &i1, &n, out);
	}
	Native_Free(&i);
	Native_Free(&i1);
	Native_Free(&sp);
	return ok;
}

/* OnCheckPoint(ItemName, VehicleName): a checkpoint taken by the player or a racer: the scores, the buoy moves on and
 * every racer heads for it */
static byte Win2nd_OnCheckPoint(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *un = TV(), *ys = TV(), *l = TV(), *lim = TV(), *rep = TV(), *lst = TV(), *e = TV(), *bay = TV(), *bv = TV();
	nativeVal_t *b1 = TV(), *best = TV(), *keys = TV(), *idx = TV(), *pos = TV(), *ang = TV(), *tn = TV(), *l2 = TV(), *pn = TV(), *ship = TV();
	nativeVal_t *pd = TV(), *args = TV();
	for (int i = 1; i < 7; i++) {
		(void)TV();
	}
	nativeVal_t *now = TV(), *r = TV(), *at = TV(), *next = TV();
	byte ok = Mode_Import("RaceMaker") && Mode_Import("RacerScorer") && Mode_Import("Sound") && Mode_Import("VehiclesNPC") && Mode_UsrHandle(usr);
	if (!ok || !Native_True(usr)) {
		return Mode_Done(&t, ok);
	}
	ok = Mode_VGet(usr, "Name", un) && Mode_G(WR, "YourScore", ys);
	if (ok && LvN_EqI(ys, -1) && Native_Compare(&a[1], un)) {
		ok = Mode_Read(WR, "YourScore", ys) && (!LvN_EqI(ys, -1) || LvN_PutI(WR, "YourScore", 0));
		return Mode_Done(&t, ok);
	}
	ok = ok && Mode_EntityH(&a[0], l);
	if (ok && !Native_Compare(un, &a[1])) {
		ok = LvN_GAdd(WR, "YourScore", 1, NULL) && Mode_Read(WR, "YourScore", ys);
		if (ok && LvN_EqI(ys, 0)) {
			ok = Mode_G(WR, "WayPointlimit", lim);
			if (ok) {
				Mode_Repr(lim, rep);
			}
			ok = ok && LV_CALL("RacerScorer", "InitMissionScorer", NULL, "iiv", 0L, 0L, rep) &&
				Mode_ModTo("Menu", "MissionScorerCall", "RacerScorer", "MissionScorer") && LvN_GElems(WR, "racers", lst);
			int cnt = ok ? Native_ForCount(lst) : -1;
			ok = cnt >= 0;
			for (int k = 0; ok && k < cnt; k++) {
				nativeVal_t it = { 0 };
				Native_Free(e);
				ok = Native_SeqItem(lst, k, &it) && Mode_EntityH(&it, e) && LvN_SetS(e, "TgType", "Green");
				Native_Free(&it);
			}
		} else if (ok) {
			ok = Mode_G(WR, "WayPointlimit", lim);
			byte won = ok && Native_Compare(ys, lim) >= 0;
			ok = ok && Mode_Sound("WaySound", "WayVolume");
			if (ok && won) {
				return Mode_Done(&t, LV_OWN(WR, "CancelMission", NULL, "") && Wr_NoScorer(l) && LV_OWN(WR, "PreSetNextMission", NULL, ""));
			}
		}
	} else if (ok) {
		int racer = Mode_HasKey(WR, "BayScorer", 1, &a[1]);
		if (racer <= 0) {
			return Mode_Done(&t, racer == 0);
		}
		ok = Mode_Sound("EneWaySound", "EneWayVolume") && Mode_G(WR, "BayScorer", bay) && Native_Subscript(bay, &a[1], bv) &&
			Native_Arith('+', bv, Mode_I(&t, 1), b1) && Mode_DictPut(WR, "BayScorer", NULL, &a[1], b1) && Mode_Read(WR, "BayScorer", bay) &&
			(Native_Free(bv), Native_Subscript(bay, &a[1], bv)) && Mode_G(WR, "WayPointlimit", lim);
		if (ok && Native_Compare(bv, lim) >= 0) {
			return Mode_Done(&t, Wr_NoScorer(l) && LvN_Com("Mission_Win2ndRace_RemoteMessage2", "Trurl", WR, "InitRace", LvN_NoArgs()) &&
				LV_OWN(WR, "CancelMission", NULL, ""));
		}
	}
	Native_SetInt(best, 0);
	ok = ok && Mode_Keys(WR, "BayScorer", 1, keys);
	int cnt = ok ? Native_ForCount(keys) : -1;
	ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t it = { 0 };
		Native_Free(bv);
		ok = Native_SeqItem(keys, k, &it) && Mode_Read(WR, "BayScorer", bay) && Native_Subscript(bay, &it, bv);
		if (ok && Native_Compare(bv, best) > 0) {
			Native_Free(best);
			Native_Free(bv);
			ok = Mode_Read(WR, "BayScorer", bay) && Native_Subscript(bay, &it, best);
		}
		Native_Free(&it);
	}
	ok = ok && Wr_NextIndex(idx) && Mode_Put(WR, "iStartPos", NULL, idx) && Wr_Start("iStartPos", 0, pos) && Mode_VSet(l, "Pos", pos) &&
		Wr_StartAng(ang) && Mode_VSet(l, "Ang", ang) && LvN_SetS(l, "OnTake", "Mission_Win2ndRace.OnCheckPoint") &&
		Native_Arith('+', &a[0], Mode_S(&t, "_Trigg"), tn) && Mode_EntityH(tn, l2) && (Native_Free(pos), Mode_VGet(l, "Pos", pos)) &&
		Mode_VSet(l2, "Pos", pos) && LvN_SetS(l2, "OnTake", "Mission_Win2ndRace.OnNearCheckPoint") && Mode_ListGet("Race", "First", pn) &&
		(Native_Free(e), Mode_EntityH(pn, e));
	while (ok && Native_True(e)) {
		for (int i = 0; i < 7; i++) {
			Native_Free(&args[i]);
		}
		Native_Free(now);
		Native_Free(r);
		Native_Free(at);
		Native_Free(next);
		Native_Free(ship);
		Native_Free(pd);
		ok = LvN_SetF(e, "MoveTime", 0.0) && Wr_Racer(pn, ship) && LV_CALL("VehiclesNPC", "GetIAProf", pd, "v", ship) &&
			Wr_StateArgs(pn, l, pd, args);
		if (ok && !Native_Compare(pn, &a[1])) {
			ok = LV_CALL("SAI", "SetStateVehicle", NULL, "vvvvvvv", &args[0], &args[1], &args[2], &args[3], &args[4], &args[5], &args[6]);
		} else if (ok) {
			Native_SetFloat(now, Mode_Time());
			ok = Mode_Rand(Mode_I(&t, 0), Mode_I(&t, 0), r) && Native_Arith('+', now, r, at) && Wr_SchedState(at, args, 7);
		}
		ok = ok && Mode_VGet(e, "NextInList", next);
		if (ok) {
			Native_Free(pn);
			Native_Copy(pn, next);
			Native_Free(e);
			ok = Mode_EntityH(pn, e);
		}
	}
	ok = ok && Mode_Read(WR, "iStartPos", idx) && Mode_Put(WR, "nextStartPos", NULL, idx) && Mode_Read(WR, "YourScore", ys) &&
		LV_CALL("RacerScorer", "UpdateMissionScorer", NULL, "ivv", 0L, ys, best) && LvN_Draw();
	return Mode_Done(&t, ok);
}

/* OnNearCheckPoint(ItemName, VehicleName): a racer near the buoy: the racers that go on to the next checkpoint head
 * for it, one after another, except the one that keeps going for this one */
static byte Win2nd_OnNearCheckPoint(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ply = TV(), *uc = TV(), *l = TV(), *n2 = TV(), *ns = TV(), *pos = TV(), *opon = TV(), *nn = TV(), *timy = TV(), *name = TV();
	nativeVal_t *ent = TV(), *ship = TV(), *pd = TV(), *gn = TV(), *args = TV();
	for (int i = 1; i < 8; i++) {
		(void)TV();
	}
	nativeVal_t *t2 = TV(), *next = TV();
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("RaceMaker") && Mode_EntityH(&a[1], ply) && Mode_VGet(ply, "UsrControl", uc);
	int out = !ok ? -1 : LvN_EqI(uc, -1);
	if (out > 0) {
		int has = Mode_HasKey(WR, "BayScorer", 1, &a[1]);
		out = has < 0 ? -1 : !has;
	}
	if (out != 0) {
		return Mode_Done(&t, out > 0);
	}
	ok = Mode_EntityH(&a[0], l) && LvN_SetF(l, "rTime", 0.5) && Wr_NextIndex(n2) && Mode_G(WR, "nextStartPos", ns);
	if (!ok || !Native_Compare(ns, n2)) {
		return Mode_Done(&t, ok);
	}
	ok = Mode_Put(WR, "nextStartPos", NULL, n2) && Wr_Start("nextStartPos", 0, pos) && (Native_Free(uc), Mode_VGet(ply, "UsrControl", uc));
	int user = !ok ? -1 : LvN_EqI(uc, 0);
	if (user > 0) {
		user = Mode_G(WR, "NumOponNotGoNextItem", nn) ? LvN_EqI(nn, 0) : -1;
	}
	if (user > 0) {
		ok = LV_OWN(WR, "NameOponGoItem", opon, "v", &a[0]);
	} else {
		ok = user == 0;
		LvN_Str(opon, "");
	}
	Native_SetFloat(timy, Mode_Time());
	ok = ok && Mode_ListGet("Race", "First", name) && Mode_EntityH(name, ent);
	while (ok && Native_True(ent)) {
		if (Native_Compare(name, &a[1])) {
			Native_Free(ship);
			Native_Free(pd);
			Native_Free(gn);
			ok = Wr_Racer(name, ship) && LV_CALL("VehiclesNPC", "GetIAProf", pd, "v", ship) && Mode_VGet(pd, "GoNextItemRace", gn);
			if (ok && LvN_EqI(gn, 1) && Native_Compare(name, opon)) {
				for (int i = 0; i < 8; i++) {
					Native_Free(&args[i]);
				}
				Native_SetInt(&args[0], 6);
				Native_Copy(&args[1], name);
				Native_SetInt(&args[7], 1);
				ok = LvN_Item(pos, 0, &args[2]) && LvN_Item(pos, 1, &args[3]) && LvN_Item(pos, 2, &args[4]) && Mode_VGet(l, "Radius", &args[5]) &&
					Mode_VGet(pd, "ControlBoostRace", &args[6]) && Wr_SchedState(timy, args, 8);
				if (ok) {
					nativeVal_t d;
					Native_SetFloat(&d, 0.05);
					Native_Free(t2);
					ok = Native_Arith('+', timy, &d, t2);
					Native_Free(timy);
					Native_Copy(timy, t2);
				}
			}
		}
		Native_Free(next);
		ok = ok && Mode_VGet(ent, "NextInList", next);
		if (ok) {
			Native_Free(name);
			Native_Copy(name, next);
			Native_Free(ent);
			ok = Mode_EntityH(name, ent);
		}
	}
	return Mode_Done(&t, ok);
}

/* NameOponGoItem(ItemName): the racer farthest from the buoy ("" when none is farther than 0) */
static byte Win2nd_NameOponGoItem_C(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *item = TV(), *best = TV(), *name = TV(), *ent = TV(), *ep = TV(), *ip = TV(), *sub = TV(), *d = TV(), *next = TV();
	LvN_Str(out, "");
	Native_SetInt(best, 0);
	byte ok = Mode_EntityH(&a[0], item) && Mode_ListGet("Race", "First", name) && Mode_EntityH(name, ent);
	while (ok && Native_True(ent)) {
		Native_Free(ep);
		Native_Free(ip);
		Native_Free(sub);
		Native_Free(d);
		ok = Mode_VGet(ent, "Pos", ep) && Mode_VGet(item, "Pos", ip) && Mode_VecSub(ep, ip, sub) && Mode_VecModSqr(sub, d);
		if (ok && Native_Compare(d, best) > 0) {
			Native_Free(best);
			Native_Copy(best, d);
			Native_Free(out);
			Native_Copy(out, name);
		}
		Native_Free(next);
		ok = ok && Mode_VGet(ent, "NextInList", next);
		if (ok) {
			Native_Free(name);
			Native_Copy(name, next);
			Native_Free(ent);
			ok = Mode_EntityH(name, ent);
		}
	}
	return Mode_Done(&t, ok);
}

/* CancelMission(): no music, no scorer, the buoy goes, the racers back into the traffic, the scores reset */
static byte Win2nd_CancelMission(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *wn = TV(), *e = TV(), *players = TV(), *nm = TV(), *next = TV(), *empty = TV();
	byte ok = Mode_GVarSetI("isActionMusic", 0) && LvN_ScorerOff() && LvN_Draw() && Mode_G(WR, "waypointName", wn) && Mode_EntityH(wn, e) &&
		(!Native_True(e) || Mode_VDel(e));
	Native_SetEmpty(players, NV_LIST);
	Native_Free(e);
	ok = ok && Mode_FirstOf("Race", e);
	while (ok && Native_True(e)) {
		Native_Free(next);
		ok = Mode_VGet(e, "Name", Native_Append(players)) && Mode_VGet(e, "NextInList", next);
		Native_Free(e);
		ok = ok && Mode_EntityH(next, e);
	}
	for (int k = 0; ok && k < players->n; k++) {
		Native_Free(e);
		Native_Free(nm);
		ok = Mode_EntityH(&players->items[k], e) && (!Native_True(e) || (LvN_SetF(e, "MoveTime", 0.0) && LvN_SetS(e, "TgType", "None") &&
			LvN_SetS(e, "EntityList", "Traffic") && Mode_VGet(e, "Name", nm) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, nm, 1L)));
	}
	Native_SetEmpty(empty, NV_DICT);
	ok = ok && Mode_Put(WR, "BayScorer", NULL, empty) && LvN_PutI(WR, "YourScore", -1) && LvN_PutS(WR, "waypointName", "") &&
		LvN_PutI(WR, "iStartPos", 0) && LV_CALL("SInput", "Bind", NULL, "isss", 0L, "Racer", "CM", "");
	return Mode_Done(&t, ok);
}

/* the keyword adapter of OnCounterMeasure(id=0) */
static PyObject *Win2nd_OnCounterMeasurePy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "id", NULL };
	PyObject *id = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", kwlist, &id)) {
		return NULL;
	}
	return Mode_Result(Win2nd_OnCounterMeasure());
}

LV_CORE0(Win2nd_Init)
LV_CORE0(Win2nd_InitRace)
LV_COREA(Win2nd_RacerDamaged)
LV_COREA(Win2nd_OnRaceStart)
LV_COREA(Win2nd_OnCheckPoint)
LV_COREA(Win2nd_OnNearCheckPoint)
LV_CORE0(Win2nd_CancelMission)
LV_FN(Win2nd, InitPy, 1, Win2nd_Init_C)
LV_FN(Win2nd, InitRacePy, 0, Win2nd_InitRace_C)
LV_FN(Win2nd, GetRaceWaypointIndexPy, 0, Win2nd_GetRaceWaypointIndex_C)
LV_FN(Win2nd, RacerDamagedPy, 3, Win2nd_RacerDamaged_C)
LV_FN(Win2nd, OnRaceStartPy, 2, Win2nd_OnRaceStart_C)
LV_FN(Win2nd, OnCheckPointPy, 2, Win2nd_OnCheckPoint_C)
LV_FN(Win2nd, OnNearCheckPointPy, 2, Win2nd_OnNearCheckPoint_C)
LV_FN(Win2nd, NameOponGoItemPy, 1, Win2nd_NameOponGoItem_C)
LV_FN(Win2nd, CancelMissionPy, 0, Win2nd_CancelMission_C)

PyMethodDef g_OutskirtsWin2ndRaceMethods[] = {
	{ "InitStatus", Win2nd_InitStatus, 1, NULL },
	{ "Init", Win2nd_InitPy, 1, NULL },
	{ "InitRace", Win2nd_InitRacePy, 1, NULL },
	{ "OnCounterMeasure", (PyCFunction)(void (*)(void))Win2nd_OnCounterMeasurePy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "GetRaceWaypointIndex", Win2nd_GetRaceWaypointIndexPy, 1, NULL },
	{ "RacerDamaged", Win2nd_RacerDamagedPy, 1, NULL },
	{ "OnRaceStart", Win2nd_OnRaceStartPy, 1, NULL },
	{ "OnCheckPoint", Win2nd_OnCheckPointPy, 1, NULL },
	{ "OnNearCheckPoint", Win2nd_OnNearCheckPointPy, 1, NULL },
	{ "NameOponGoItem", Win2nd_NameOponGoItemPy, 1, NULL },
	{ "CancelMission", Win2nd_CancelMissionPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_Demo_HumphreyRace: the race against Humphrey of the demo, then the call to the crazy gambler ---- */

/* InitStatus(LevelPath): the traffic; the challenge of the race or none */
static byte Demo_InitStatus(void) {
	static const char *const race[] = { "Revenge.NumEnemies", "3", "Challenge.NumEnemies", "3", "Challenge.Type", "Race", "Challenge.Foe",
		"Humphrey", "Challenge.Money", "0", NULL };
	static const char *const none[] = { "Revenge.Type", "", "Revenge.Foe", "", "Revenge.NumEnemies", "", "Challenge.NumEnemies", "",
		"Challenge.Type", "", "Challenge.Foe", "", "Challenge.Money", "", "Race.Profile", "", NULL };
	if (!Mode_Import("Init") || !Mode_AcShips(DH, ships_demo) || !Mode_Import("MapRace")) {
		return 0;
	}
	if (LvN_SaveIs("MissionState", "HumphreyRace")) {
		Mode_Saves(race);
		return Mode_SaveG("Race.Profile", DH, "RACE_PROFILE") && Mode_ModList1("MapRace", "Races", "MapRace", "EasyRaceOurskirts1") &&
			LvN_Com("Mission_FirstCrazyBet_RemoteMessage5", "CrazyGambler", DH, "ShowSystemMessage", LvN_NoArgs());
	}
	Mode_Saves(none);
	return Mode_ModEmpty("MapRace", "Races");
}

static byte Demo_ShowSystemMessage(void) {
	nativeVal_t text = { 0 };
	if (!Mode_Import("RaceMaker")) {
		return 0;
	}
	Mode_Lang("RaceSystemGetShip", &text);
	byte ok = LV_CALL("RaceMaker", "ShowSystemMessage", NULL, "v", &text);
	Native_Free(&text);
	return ok;
}

/* Init(LevelPath): the gates closed, the metro to the press; the race hooks, the call or the metro by the state */
static byte Demo_Init(void) {
	nativeVal_t metro = { 0 }, prev = { 0 };
	byte ok = Mode_Import("RaceMaker") && Mode_Import("Parking") &&
		LV_CALL("SScorer", "AddModel", NULL, "isssii", 0L, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3L, 25L) && Os_CloseDoors() &&
		Os_Metro("Mission_Demo_HumphreyRace.OnMetroUsedToPress", &metro);
	Native_Free(&metro);
	if (ok && LvN_SaveIs("MissionState", "HumphreyRace")) {
		ok = Mode_GVarSetI("TimeToCancelCurrentMission", 0) && LvN_ModSetI("RaceMaker", "SkipTimer", 1) &&
			LvN_ModSetGlobal("RaceMaker", "OnPlayerWinsRace", DH, "OnPlayerWinsRace") &&
			LvN_ModSetGlobal("RaceMaker", "OnPlayerLoosesRace", DH, "OnPlayerLoosesRace") && LvN_ModSetGlobal("RaceMaker", "OnPreInitRace", DH, "DelRacers") &&
			LvN_ModSetGlobal("RaceMaker", "OnInitRace", DH, "SetMissionTarget") &&
			LvN_ModSetGlobal("RaceMaker", "OnCheckPointCallback", DH, "ClosePressHangar") && Mode_Target("BadBoy0") &&
			LvN_ModGet("Parking", "SwitchVehicleMissionCallback", &prev) && Mode_Put(DH, "prevSwitchVehicleMissionCallback", NULL, &prev) &&
			LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", DH, "ShowBoostMessage");
	} else if (ok) {
		ok = LV_OWN(DH, LvN_SaveIs("MissionState", "CallCrazyGambler") ? "CallCrazyGambler" : "EnableMetro", NULL, "");
	}
	Native_Free(&prev);
	return ok;
}

/* ShowBoostMessage(): the first ship: the previous callback back, the message about the boost key */
static byte Demo_ShowBoostMessage(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = TV(), *text = TV(), *key = TV(), *msg = TV();
	byte called;
	byte ok = Mode_Import("Parking") && Mode_Import("RaceMaker") && Mode_CallIf(DH, "prevSwitchVehicleMissionCallback", &called) &&
		Mode_G(DH, "prevSwitchVehicleMissionCallback", prev) && Mode_SetModVar("Parking", "SwitchVehicleMissionCallback", prev);
	if (ok) {
		Mode_Lang("Mission_FirstCrazyBet_SystemMessage4", text);
		ok = LV_CALL("SInput", "GetDefinedEntry", key, "isss", 0L, "Racer", "Boost", "Mouse") && Native_Arith('%', text, key, msg) &&
			Mode_MsgText(msg, Mode_Time() + 10);
	}
	return Mode_Done(&t, ok);
}

/* ClosePressHangar(): the first checkpoint: the hangar of the press closes */
static byte Demo_ClosePressHangar(void) {
	return Mode_Import("RaceMaker") && Mode_EntSetS("DM_Door_CityDoorA_01", "OnObjSlot", "!CloseDoorSlot") &&
		LvN_ModNone("RaceMaker", "OnCheckPointCallback");
}

/* DelRacers(names): the racers of the names that exist go */
static byte Demo_DelRacers(const nativeVal_t *a) {
	return Mode_DelNames(&a[0]);
}

/* OnPlayerWinsRace(): no race hooks, the win saved, the SMerc1 ship and engine to edit, the gambler's message */
static byte Demo_OnPlayerWinsRace(void) {
	static const char *const won[] = { "Stats.WinHumphreyRace", "1", "JackInTheBox.Active", "0", "MissionState", "CallCrazyGambler",
		"Revenge.Type", "", "Revenge.Foe", "", "Revenge.NumEnemies", "", NULL };
	static const char *const hooks[] = { "OnPlayerWinsRace", "OnPlayerLoosesRace", "OnPreInitRace", "OnInitRace", NULL };
	byte ok = Mode_Import("RaceMaker") && Mode_Import("Scorer");
	for (int i = 0; ok && hooks[i]; i++) {
		ok = LvN_ModNone("RaceMaker", hooks[i]);
	}
	if (!ok || !Mode_Target(NULL)) {
		return 0;
	}
	Mode_Saves(won);
	return Mode_SaveAdd(DH, "Hangar.shipsToEditList", "SMerc1") && Mode_SaveAdd(DH, "Hangar.availableEnginesList", "MMERC1") &&
		LvN_Com("Mission_FirstCrazyBet_RemoteMessage7", "CrazyGambler", DH, "CallCrazyGambler", LvN_NoArgs());
}

/* CallCrazyGambler(): a phone cab as the target, Rusty 3 s later */
static byte Demo_CallCrazyGambler(void) {
	return Mode_PhoneCab("Mission_Demo_HumphreyRace.UsePhoneCab", 1, NULL, NULL) && Mode_Later(3, DH, "RustyAppears", NULL, 0, NULL);
}

static byte Demo_ShowRustyMapsSystemMessage(void) {
	nativeVal_t cad = { 0 };
	Mode_Lang("Mission_FirstCrazyBet_SystemMessage3", &cad);
	byte ok = Mode_SpeechMsg(&cad);
	Native_Free(&cad);
	return ok;
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): the police warning while the alarm is on, else the boss */
static byte Demo_UsePhoneCab(const nativeVal_t *a) {
	int alarm = Mode_PhoneUse(DH, a);
	if (alarm > 0) {
		return Os_PoliceTalk(DH, "Generics_StillPolice1_CrazyGambler", "CrazyGambler");
	}
	return alarm == 0 && Os_PcDone(DH, 1) && LV_OWN(DH, "BossAppears", NULL, "");
}

/* EnableMetro(): the metro to the end of the demo; the first time a waypoint and the temple on the map */
static byte Demo_EnableMetro(void) {
	nativeVal_t e = { 0 };
	byte ok = Mode_Import("MissionsFuncs") && Os_Metro("Mission_Demo_HumphreyRace.OnMetroUsed", &e) &&
		LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, 1L);
	if (ok && !LvN_SaveIs("MissionState", "EndOfDemo")) {
		ok = Mode_SaveS("MissionState", "EndOfDemo") && Mode_WayPoint() && Mode_ForceMap("Temple") &&
			Mode_MMDesc("Mission_FirstContactPress_MissionText3", 1);
	}
	Native_Free(&e);
	return ok;
}

/* OnMetroUsed(UsrName, MetroName, optId, FullOption): the metro goes up to the end of the demo */
static byte Demo_OnMetroUsed(const nativeVal_t *a) {
	nativeVal_t m = { 0 }, act = { 0 }, x = { 0 }, ev = { 0 };
	LvN_Str(&ev, "Mission_Demo_HumphreyRace.OnEndOfDemo");
	byte ok = Mode_Import("Elevator") && Mode_Import("Metro") && Mode_EntityH(&a[1], &m) && Mode_VGet(&m, "ActCtrl", &act) &&
		LV_CALL("SAct", "GetClass", NULL, "v", &act) && LvN_GetAction("Up", &x) && LvN_SetAct("EventFunc", &ev) &&
		LV_CALL("Elevator", "UseElevator", NULL, "vvsv", &a[0], &a[1], "MoveLevel Press", &a[3]) && LV_CALL("Metro", "SetUsrParams", NULL, "v", &a[0]);
	Native_Free(&m);
	Native_Free(&act);
	Native_Free(&x);
	Native_Free(&ev);
	return ok;
}

LV_CORE0(Demo_InitStatus)
LV_CORE0(Demo_ShowSystemMessage)
LV_CORE0(Demo_Init)
LV_CORE0(Demo_ShowBoostMessage)
LV_CORE0(Demo_ClosePressHangar)
LV_COREA(Demo_DelRacers)
LV_CORE0(Demo_OnPlayerWinsRace)
LV_CORE0(Demo_CallCrazyGambler)
LV_CORE0(Demo_ShowRustyMapsSystemMessage)
LV_COREA(Demo_UsePhoneCab)
LV_CORE0(Demo_EnableMetro)
LV_COREA(Demo_OnMetroUsed)
LV_FN(Demo, InitStatusPy, 1, Demo_InitStatus_C)
LV_FN(Demo, ShowSystemMessagePy, 0, Demo_ShowSystemMessage_C)
LV_FN(Demo, InitPy, 1, Demo_Init_C)
LV_FN(Demo, ShowBoostMessagePy, 0, Demo_ShowBoostMessage_C)
LV_FN(Demo, ClosePressHangarPy, 0, Demo_ClosePressHangar_C)
LV_FN(Demo, DelRacersPy, 1, Demo_DelRacers_C)
LV_FN(Demo, OnPlayerWinsRacePy, 0, Demo_OnPlayerWinsRace_C)
LV_FN(Demo, CallCrazyGamblerPy, 0, Demo_CallCrazyGambler_C)
LV_FN(Demo, ShowRustyMapsSystemMessagePy, 0, Demo_ShowRustyMapsSystemMessage_C)
LV_FN(Demo, UsePhoneCabPy, 4, Demo_UsePhoneCab_C)
LV_FN(Demo, EnableMetroPy, 0, Demo_EnableMetro_C)
LV_FN(Demo, OnMetroUsedPy, 4, Demo_OnMetroUsed_C)

PyMethodDef g_OutskirtsDemoHumphreyRaceMethods[] = {
	{ "InitStatus", Demo_InitStatusPy, 1, NULL },
	{ "ShowSystemMessage", Demo_ShowSystemMessagePy, 1, NULL },
	{ "Init", Demo_InitPy, 1, NULL },
	{ "ShowBoostMessage", Demo_ShowBoostMessagePy, 1, NULL },
	{ "ClosePressHangar", Demo_ClosePressHangarPy, 1, NULL },
	{ "DelRacers", Demo_DelRacersPy, 1, NULL },
	{ "OnPlayerWinsRace", Demo_OnPlayerWinsRacePy, 1, NULL },
	{ "CallCrazyGambler", Demo_CallCrazyGamblerPy, 1, NULL },
	{ "ShowRustyMapsSystemMessage", Demo_ShowRustyMapsSystemMessagePy, 1, NULL },
	{ "UsePhoneCab", Demo_UsePhoneCabPy, 1, NULL },
	{ "EnableMetro", Demo_EnableMetroPy, 1, NULL },
	{ "OnMetroUsed", Demo_OnMetroUsedPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GoToOutskirts: the crazy gambler calls until the player takes the elevator to GamblinDen ---- */

static PyObject *GoTo_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, GO, ships_all);
}

/* Speech.AddRemoteCom(key, "CrazyGambler", LaunchCrazyGamblerComunication, (messageInterval,)) */
static byte GoTo_Call(const char *key) {
	nativeVal_t p = { 0 };
	Native_SetEmpty(&p, NV_TUPLE);
	byte ok = Mode_G(GO, "messageInterval", Native_Append(&p)) && LvN_Com(key, "CrazyGambler", GO, "LaunchCrazyGamblerComunication", &p);
	Native_Free(&p);
	return ok;
}

/* Init(LevelPath): the gates and the metro closed, the death hook, the gambler's first call */
static byte GoTo_Init(void) {
	nativeVal_t x = { 0 };
	byte ok = Os_CloseDoors() && Mode_EntSetI("DM_Door_MetroExt_", "ArrowCanUse", 0) &&
		LvN_ModSetGlobal("Chars", "OnMainCharDeath", GO, "OnMainCharDeath");
	if (ok && LvN_SaveIs("PlayerHasBeenKilled", "1")) {
		ok = Mode_SaveS("PlayerHasBeenKilled", "0") && LvN_Com("Mission_DestroyPolice_RemoteMessage5", "Bishop", NULL, NULL, NULL);
	} else if (ok) {
		ok = Native_GVarGet("isXbox", &x) && (!LvN_EqI(&x, 1) || (Mode_Import("XBMenu") && LV_CALL("XBMenu", "ShowPadWindow", NULL, "i", 0L)));
	}
	Native_Free(&x);
	return ok && GoTo_Call("Mission_TalkToFicus3_RemoteMessage1");
}

/* LaunchCrazyGamblerComunication(time): the first time the elevator to GamblinDen as the target; the next call later */
static byte GoTo_LaunchCrazyGamblerComunication(const nativeVal_t *a) {
	static const char cad[] = "Mission_FirstMeetCrazyGambler_MissionText1";
	nativeVal_t e = { 0 };
	int c = LvN_GEqI(GO, "numCrazyGamblerComs", 0);
	byte ok = c >= 0;
	if (c > 0) {
		ok = LvN_Entity("DM_Door_Elevator_GamblinDen", &e) && LvN_SetS(&e, "UseFunc", "Mission_GoToOutskirts.UseElevator") &&
			LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, 1L) &&
			(LvN_SaveIs("PrimaryMissionDesc", cad) || (Mode_Import("SScorer") && Mode_ForceMap("GamblinDen") && Mode_MMDesc(cad, 1) && Mode_WayPoint()));
	}
	Native_Free(&e);
	return ok && Mode_LaterV(&a[0], GO, "PlayCrazyGamblerComunication", NULL, 0, NULL);
}

/* PlayCrazyGamblerComunication(): the second call, then the third one again and again */
static byte GoTo_PlayCrazyGamblerComunication(void) {
	nativeVal_t n = { 0 };
	byte ok = Mode_G(GO, "numCrazyGamblerComs", &n) && (Native_Compare(&n, &(nativeVal_t){ .type = NV_INT, .i = 2 }) >= 0 ||
		LvN_GAdd(GO, "numCrazyGamblerComs", 1, NULL));
	Native_Free(&n);
	int second = ok ? LvN_GEqI(GO, "numCrazyGamblerComs", 1) : -1;
	return second >= 0 && GoTo_Call(second ? "Mission_TalkToFicus3_RemoteMessage2" : "Mission_TalkToFicus3_RemoteMessage3");
}

LV_CORE0(GoTo_Init)
LV_COREA(GoTo_LaunchCrazyGamblerComunication)
LV_CORE0(GoTo_PlayCrazyGamblerComunication)
LV_FN(GoTo, InitPy, 1, GoTo_Init_C)
LV_FN(GoTo, LaunchCrazyGamblerComunicationPy, 1, GoTo_LaunchCrazyGamblerComunication_C)
LV_FN(GoTo, PlayCrazyGamblerComunicationPy, 0, GoTo_PlayCrazyGamblerComunication_C)

PyMethodDef g_OutskirtsGoToOutskirtsMethods[] = {
	{ "InitStatus", GoTo_InitStatus, 1, NULL },
	{ "Init", GoTo_InitPy, 1, NULL },
	{ "LaunchCrazyGamblerComunication", GoTo_LaunchCrazyGamblerComunicationPy, 1, NULL },
	{ "PlayCrazyGamblerComunication", GoTo_PlayCrazyGamblerComunicationPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_KillPolice: two police ships to kill for the crazy gambler, then Berto and the metro to the press ---- */

/* InitStatus(LevelPath): no alarm, the traffic (fewer ships in the demo) */
static byte Police_InitStatus(void) {
	nativeVal_t demo = { 0 };
	byte ok = Mode_SaveS("AlarmActive", "0") && Mode_SaveS("AlarmStatus", "0") && Mode_Import("Init") && LvN_ModGet("Init", "isDemo", &demo) &&
		Mode_AcShips(KP, Native_True(&demo) ? ships_demo : ships_police5);
	Native_Free(&demo);
	return ok;
}

/* Init(LevelPath): the gates and the metro closed, the death hook, the mission or Berto by the state */
static byte Police_Init(void) {
	byte ok = Os_CloseDoors() && Mode_EntSetI("DM_Door_MetroExt_", "ArrowCanUse", 0) &&
		LV_CALL("SScorer", "AddModel", NULL, "isssii", 0L, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3L, 25L) && Mode_Import("Chars") &&
		LvN_ModSetGlobal("Chars", "OnMainCharDeath", KP, "OnMainCharDeath") &&
		(!LvN_SaveIs("PlayerHasBeenKilled", "1") ||
			(Mode_SaveS("PlayerHasBeenKilled", "0") && LvN_Com("Mission_DestroyPolice_RemoteMessage5", "Bishop", NULL, NULL, NULL)));
	return ok && LV_OWN(KP, LvN_SaveIs("MissionState", "KillPolice") ? "InitMission" : "BertoAppears2", NULL, "");
}

static byte Police_SetPos(const nativeVal_t *a) {
	return Mode_VSet(&a[0], "Pos", &a[1]);
}

/* one of the two police ships of the mission (the entity of name, into e): moved 3 s into the game, red, its death
 * counted */
static byte Kp_Cop(const nativeVal_t *name, long x, long y, long z, nativeVal_t *e) {
	nativeVal_t at = { 0 }, args[2] = { { 0 }, { 0 } };
	if (!Mode_EntityH(name, e)) {
		return 0;
	}
	if (!Native_True(e)) {
		return LvN_RaiseText("Tiene que haber al menos dos policias");
	}
	Native_SetInt(&at, 3);
	Native_Copy(&args[0], e);
	Mode_Ints3(&args[1], x, y, z);
	byte ok = Mode_AtG(&at, KP, "SetPos", args, 2) && LvN_SetS(e, "TgType", "red") && LvN_SetS(e, "OnDeath", "Mission_KillPolice.OnPoliceDeath");
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	return ok;
}

/* InitMission(): the first two police ships as the targets, the alarm hook, the gambler's message */
static byte Police_InitMission(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cop1 = TV(), *e = TV(), *cop2 = TV(), *e2 = TV(), *lst = TV(), *fn = TV();
	byte ok = Mode_Import("Alarm") && Mode_Import("OutMap") && Mode_ListGet("OutCop", "First", cop1) && Kp_Cop(cop1, -311955, 56409, -135709, e) &&
		Mode_VGet(e, "NextInList", cop2) && Kp_Cop(cop2, -333879, 56196, -121097, e2) && Mode_Append(KP, "policesList", cop1) &&
		Mode_Append(KP, "policesList", cop2) && Mode_G(KP, "policesList", lst) && LV_CALL("MissionsFuncs", "SetMultipleMissionTargets", NULL, "v", lst) &&
		LvN_GObj(KP, "OnAlarm", fn) && LvN_ModAppend("Alarm", "SetAlarmCallback", fn) &&
		LvN_Com("Mission_DestroyPolice_RemoteMessage1", "CrazyGambler", KP, "ShowMessage2", LvN_NoArgs());
	return Mode_Done(&t, ok);
}

/* OnAlarm(status): the alarm does not fall while the mission runs */
static byte Police_OnAlarm(void) {
	nativeVal_t v = { 0 };
	byte ok = Native_GVarGet("AlarmFallDelta", &v) && Mode_Put(KP, "alarmFallDelta", NULL, &v) && Mode_GVarSetI("AlarmFallDelta", 0);
	Native_Free(&v);
	return ok;
}

/* OnPoliceDeath(LooserName, WinnerName): one police ship of the mission fewer; both: the alarm falls again */
static byte Police_OnPoliceDeath(const nativeVal_t *a) {
	nativeVal_t d = { 0 };
	int in = Mode_Import("Vehicles") && Mode_Import("Speech") && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) ?
		Mode_InG(&a[0], KP, "policesList") : -1;
	byte ok = in >= 0 && (!in || (Mode_Remove(KP, "policesList", &a[0]) && LvN_GAdd(KP, "policesKilled", 1, NULL)));
	int one = ok ? LvN_GEqI(KP, "policesKilled", 1) : -1;
	int two = one == 0 ? LvN_GEqI(KP, "policesKilled", 2) : 0;
	ok = one >= 0 && two >= 0;
	if (one > 0) {
		ok = LV_CALL("Speech", "RemoteComunication", NULL, "ss", "Mission_DestroyPolice_RemoteMessage3", "CrazyGambler");
	} else if (two > 0) {
		ok = Mode_G(KP, "alarmFallDelta", &d) && Mode_GVarSet("AlarmFallDelta", &d) &&
			LvN_Com("Mission_DestroyPolice_RemoteMessage4", "CrazyGambler", KP, "BertoAppears", LvN_NoArgs());
	}
	Native_Free(&d);
	return ok;
}

/* BertoAppears(): the blueprints of the Swarm missiles, Rusty's message, a waypoint, then Berto */
static byte Police_BertoAppears(void) {
	nativeVal_t text = { 0 }, l = { 0 }, swarm = { 0 };
	LvN_Str(&swarm, "Swarm");
	byte ok = Mode_Import("RaceMaker");
	if (ok) {
		Mode_Lang("Mission_DestroyPolice_SystemMessage1", &text);
		ok = LV_CALL("RaceMaker", "ShowSystemMessage", NULL, "v", &text) && LvN_SaveList("Hangar.availableWeaponsList", KP, &l) &&
			LvN_SaveAppend("Hangar.availableWeaponsList", &l, &swarm) && LV_CALL("Scrap", "IncSaveVar", NULL, "s", "NewBluePrintAvaliable") &&
			LvN_Com("Mission_DestroyPolice_RemoteMessage7", "Rusty", NULL, NULL, NULL) && Mode_SaveS("MissionState", "ListenToBerto") &&
			Mode_WayPoint() && LV_OWN(KP, "BertoAppears2", NULL, "");
	}
	Native_Free(&text);
	Native_Free(&l);
	Native_Free(&swarm);
	return ok;
}

/* EnableMetro(): the metro to the press; after Berto's message the press on the map and the parked ships */
static byte Police_EnableMetro(void) {
	static const char *const parked[] = { "Parked.Police", "['SPOLI1', 'SPOLI2']", "Parked.Mercs", "['SMERC1']", "Parked.TownHall",
		"['SPOLI1', 'SPOLI2']", "Parked.Bank", "['SPOLI2']", "Parked.Press", "['SPOLI1']", "Parked.Temple", "['SPOLI2']", NULL };
	nativeVal_t e = { 0 };
	byte ok = Mode_Import("MissionsFuncs") && Os_Metro("Mission_KillPolice.OnMetroUsed", &e) && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, 1L);
	if (ok && LvN_SaveIs("MissionState", "ListenToBerto")) {
		ok = Mode_WayPoint() && Mode_ForceMap("Press") && Mode_MMDesc("Mission_FirstContactPress_MissionText1", 1) && Mode_SaveS("MissionState", "");
		if (ok) {
			Mode_Saves(parked);
		}
	}
	Native_Free(&e);
	return ok;
}

LV_CORE0(Police_InitStatus)
LV_CORE0(Police_Init)
LV_COREA(Police_SetPos)
LV_CORE0(Police_InitMission)
LV_CORE0(Police_OnAlarm)
LV_COREA(Police_OnPoliceDeath)
LV_CORE0(Police_BertoAppears)
LV_CORE0(Police_EnableMetro)
LV_FN(Police, InitStatusPy, 1, Police_InitStatus_C)
LV_FN(Police, InitPy, 1, Police_Init_C)
LV_FN(Police, SetPosPy, 2, Police_SetPos_C)
LV_FN(Police, InitMissionPy, 0, Police_InitMission_C)
LV_FN(Police, OnAlarmPy, 1, Police_OnAlarm_C)
LV_FN(Police, OnPoliceDeathPy, 2, Police_OnPoliceDeath_C)
LV_FN(Police, BertoAppearsPy, 0, Police_BertoAppears_C)
LV_FN(Police, EnableMetroPy, 0, Police_EnableMetro_C)

PyMethodDef g_OutskirtsKillPoliceMethods[] = {
	{ "InitStatus", Police_InitStatusPy, 1, NULL },
	{ "Init", Police_InitPy, 1, NULL },
	{ "SetPos", Police_SetPosPy, 1, NULL },
	{ "InitMission", Police_InitMissionPy, 1, NULL },
	{ "OnAlarm", Police_OnAlarmPy, 1, NULL },
	{ "OnPoliceDeath", Police_OnPoliceDeathPy, 1, NULL },
	{ "BertoAppears", Police_BertoAppearsPy, 1, NULL },
	{ "EnableMetro", Police_EnableMetroPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_TransportKiller: the transport that only the ATPC hurts, its data pack, then the police boss ---- */

static PyObject *Transport_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, TK, ships_police5);
}

static byte Transport_Init(void) {
	if (LvN_SaveIs("Mission_TransportKiller", "Begin")) {
		return LV_OWN(TK, "InitTransport", NULL, "");
	}
	return !LvN_SaveIs("Mission_TransportKiller", "InitCall") || LV_OWN(TK, "InitCall", NULL, "");
}

/* InitTransport(): the transport on a traffic path, red, hurt only by the right weapon */
static byte Transport_InitTransport(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tr = TV(), *pos = TV(), *name = TV(), *model = TV();
	byte ok = LV_CALL("Traffic", "SetPath", NULL, "") && LV_CALL("VehiclesNPC", "MakeAI", tr, "ss", "TLight2", "Transporter") &&
		Mode_Put(TK, "trans", NULL, tr) && LV_CALL("Traffic", "ResetPath", NULL, "") && Mode_Read(TK, "trans", tr) && LvN_SetS(tr, "TgType", "red") &&
		LvN_SetI(tr, "Money", -1) && LvN_Life(tr, TK, "TRANSPORT_LIFE") && LvN_SetI(tr, "Traf_Active", 1) && Mode_VGet(tr, "Pos", pos) &&
		Mode_VSet(tr, "Traf_Begin", pos) && (Native_Free(pos), Mode_VGet(tr, "Pos", pos)) && Mode_VSet(tr, "Traf_End", pos) &&
		LvN_SetI(tr, "Traf_Speed", 100 * 28 + 2000) && Mode_VGet(tr, "Name", name) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name) &&
		LV_CALL("SAI", "SetStateVehicle", NULL, "ividd", 3L, name, 0L, 2.0, 0.025) && LvN_SetS(tr, "OnDamage", "Mission_TransportKiller.TryToDamage") &&
		LvN_SetS(tr, "OnDeath", "Mission_TransportKiller.TryToDeath") && Mode_G(TK, "Model_Datapack", model) && Mode_PreloadOnes(model);
	return Mode_Done(&t, ok);
}

/* isValidDamage(KillerName): (usr != None) and (usr.Name == KillerName) and (usr.iWeap == 4), as the int of the
 * first false part */
static byte Transport_isValidDamage_C(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr = { 0 }, x = { 0 };
	int v = Mode_UsrHandle(&usr) ? usr.type != NV_NONE : -1;
	if (v > 0) {
		v = Mode_VGet(&usr, "Name", &x) ? !Native_Compare(&x, &a[0]) : -1;
	}
	if (v > 0) {
		Native_Free(&x);
		v = Mode_VGet(&usr, "iWeap", &x) ? LvN_EqI(&x, 4) : -1;
	}
	if (v >= 0) {
		Native_SetInt(out, v);
	}
	Native_Free(&usr);
	Native_Free(&x);
	return v >= 0;
}

/* TryToDeath(EntityName, KillerName): the alarm, the data pack and its arrow where the transport died */
static byte Transport_TryToDeath(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *one = TV(), *e = TV(), *pos = TV(), *l = TV(), *model = TV(), *l2 = TV(), *cam = TV(), *name = TV();
	Native_SetInt(one, 1);
	byte ok = LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && Mode_SetAlarm(one) && Mode_EntityH(&a[0], e) && Mode_VGet(e, "Pos", pos) &&
		LvN_CreateAt(Mode_S(&t, "MissionDataPack"), pos, "Item", l) && Mode_G(TK, "Model_Datapack", model) && Mode_VModel(l, model) &&
		LvN_SetI(l, "Radius", 3500) && LvN_SetI(l, "AppearsIfFree", 0) && LvN_SetI(l, "AutoFace", 0) && LvN_SetI(l, "OnlyUser", 1) &&
		LvN_SetF(l, "rTime", 0.25) && LvN_SetS(l, "OnTake", "Mission_TransportKiller.TakeDataPack") && Mode_Import("Items") &&
		LV_CALL("Items", "SetSound", NULL, "vs", l, "TakeBDataPack") && LvN_CreateAt(Mode_S(&t, "MissionCheckArrow"), pos, "WithLife", l2);
	if (ok) {
		Mode_Ints3(cam, 0, 1000, 0);
		ok = Mode_VSet(l2, "CamTgt", cam) && Mode_VGet(l2, "Name", name) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name);
	}
	return Mode_Done(&t, ok);
}

/* TakeDataPack(ItemName, UsrName): the data pack and its arrow go, the police boss calls */
static byte Transport_TakeDataPack(void) {
	modeVals_t t = { .n = 0 };
	byte ok = Mode_Taken(Mode_S(&t, "MissionDataPack"), Mode_S(&t, "MissionCheckArrow")) &&
		LvN_Com("Mission_InvulnerableTransport_RemoteMessage4", "PoliceBoss", TK, "InitCall", LvN_NoArgs());
	return Mode_Done(&t, ok);
}

/* TryToDamage(EntityName, KillerName, Damage): the first hit sets off the alarm; a hit by the wrong weapon is healed
 * and DeepThroat says what is missing, at most every 20 s */
static byte Transport_TryToDamage(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cop = TV(), *next = TV(), *one = TV(), *valid = TV(), *shp = TV(), *life = TV(), *sum = TV(), *usr = TV(), *un = TV();
	nativeVal_t *now = TV(), *last = TV(), *later = TV(), *w = TV(), *ammo = TV();
	int alarm = Mode_AlarmOn();
	byte ok = alarm >= 0;
	if (alarm == 0) {
		ok = LvN_Com("Mission_InvulnerableTransport_RemoteMessage5", "DeepThroat", NULL, NULL, NULL) && Mode_Import("OutPolice") &&
			LvN_ModSetI("OutPolice", "iPoliceWave", 9) && LvN_ModSetI("OutPolice", "skipPrice", 1) && Mode_FirstOf("OutCop", cop);
		while (ok && Native_True(cop)) {
			Native_Free(next);
			ok = Mode_VDel(cop) && Mode_VGet(cop, "NextInList", next);
			Native_Free(cop);
			ok = ok && Mode_EntityH(next, cop);
		}
		Native_SetInt(one, 1);
		ok = ok && Mode_SetAlarm(one);
	}
	if (!ok || !LV_OWN(TK, "isValidDamage", valid, "v", &a[1])) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(valid)) {
		return Mode_Done(&t, 1);
	}
	ok = Mode_EntityH(&a[0], shp) && Mode_VGet(shp, "Life", life) && Native_Arith('+', life, &a[2], sum) && Mode_VSet(shp, "Life", sum) &&
		Mode_UsrHandle(usr);
	int his = !ok ? -1 : usr->type == NV_NONE ? 0 : Mode_VGet(usr, "Name", un) ? !Native_Compare(un, &a[1]) : -1;
	if (his > 0) {
		Native_SetFloat(now, Mode_Time());
		ok = Mode_G(TK, "LastTimeMsg", last);
		if (ok && Native_Compare(now, last) > 0) {
			ok = Native_Arith('+', now, Mode_I(&t, 20), later) && Mode_Put(TK, "LastTimeMsg", NULL, later) && Mode_VGet(usr, "isWpn04", w);
			if (ok && !Native_True(w)) {
				ok = LvN_Com("Mission_InvulnerableTransport_RemoteMessage2", "DeepThroat", NULL, NULL, NULL);
			} else if (ok) {
				nativeVal_t half;
				Native_SetFloat(&half, 0.5);
				ok = Mode_VGet(usr, "Ammo02", ammo) && LvN_Com(Native_Compare(ammo, &half) <= 0 ? "Mission_InvulnerableTransport_RemoteMessage3" :
					"Mission_InvulnerableTransport_RemoteMessage1", "DeepThroat", NULL, NULL, NULL);
			}
		}
	}
	return Mode_Done(&t, ok && his >= 0);
}

/* InitCall(): the call to the police boss from a phone cab */
static byte Transport_InitCall(void) {
	return Mode_Import("OutPolice") && Mode_SaveS("Mission_TransportKiller", "InitCall") && Mode_MMDesc("Mission_Get4thClue_MissionText1", 1) &&
		LvN_ModSetI("OutPolice", "iPoliceWave", 0) && Mode_PhoneCab("Mission_TransportKiller.UsePhoneCab", 0, TK, NULL);
}

/* SetNextMission(): the cab free again, on to NextMissionData */
static byte Transport_SetNextMission(void) {
	return Mode_GSetS(TK, "pc", "UseFunc", "PhoneCab.DefUseAudioCom") && Mode_Target(NULL) && Mode_DelSaveVars("Mission_TransportKiller") &&
		Mode_ApplyNext(TK) && Mode_MMDesc("Mission_GetReputation_MissionText1", 1);
}

LV_CORE0(Transport_Init)
LV_CORE0(Transport_InitTransport)
LV_COREA(Transport_TryToDeath)
LV_CORE0(Transport_TakeDataPack)
LV_COREA(Transport_TryToDamage)
LV_CORE0(Transport_InitCall)
LV_CORE0(Transport_SetNextMission)
LV_FN(Transport, InitPy, 1, Transport_Init_C)
LV_FN(Transport, InitTransportPy, 0, Transport_InitTransport_C)
LV_FN(Transport, isValidDamagePy, 1, Transport_isValidDamage_C)
LV_FN(Transport, TryToDeathPy, 2, Transport_TryToDeath_C)
LV_FN(Transport, TakeDataPackPy, 2, Transport_TakeDataPack_C)
LV_FN(Transport, TryToDamagePy, 3, Transport_TryToDamage_C)
LV_FN(Transport, InitCallPy, 0, Transport_InitCall_C)
LV_FN(Transport, SetNextMissionPy, 0, Transport_SetNextMission_C)

PyMethodDef g_OutskirtsTransportKillerMethods[] = {
	{ "InitStatus", Transport_InitStatus, 1, NULL },
	{ "Init", Transport_InitPy, 1, NULL },
	{ "InitTransport", Transport_InitTransportPy, 1, NULL },
	{ "isValidDamage", Transport_isValidDamagePy, 1, NULL },
	{ "TryToDeath", Transport_TryToDeathPy, 1, NULL },
	{ "TakeDataPack", Transport_TakeDataPackPy, 1, NULL },
	{ "TryToDamage", Transport_TryToDamagePy, 1, NULL },
	{ "InitCall", Transport_InitCallPy, 1, NULL },
	{ "SetNextMission", Transport_SetNextMissionPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyHumphrey: Humphrey attacks again and again until his lives are gone ---- */

static const char *const humphrey_profile[] = { "Ship=SHUMP1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=2",
	"UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25", "UsePercHook=20", "FactorHookMap=10", "CadenceShoot=13", "ControlAim=10",
	"AngAttack=3.14", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.25",
	"FactorDamageItemLife=0.35", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=30", "TimeMaxStrafe=10",
	"VelMaxStrafe=1.0", NULL };

static PyObject *Humphrey_GetHumphreyProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, HU, humphrey_profile);
}

static PyObject *Humphrey_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, HU, ships_police4);
}

/* Humphrey red, an enemy and the target */
static byte Hu_Enemy(const nativeVal_t *h) {
	nativeVal_t name = { 0 };
	byte ok = LvN_SetS(h, "TgType", "red") && LvN_SetS(h, "EntityList", "Enemies") && Mode_G(HU, "humphreyName", &name) &&
		LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", &name);
	Native_Free(&name);
	return ok;
}

/* CreateHumphrey(): Humphrey near the player unless he is there; the first time quiet, later attacking; taunts */
static byte Humphrey_CreateHumphrey(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *hn = TV(), *h = TV(), *pos = TV(), *spw = TV(), *pd = TV(), *name = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_G(HU, "humphreyName", hn) || !Mode_EntityH(hn, h)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(h)) {
		return Mode_Done(&t, 1);
	}
	Native_Free(h);
	byte ok = Mode_VGet(usr, "Pos", pos) && LV_CALL("OutMap", "GetNearestFreeSpawnPos", spw, "v", pos) &&
		LvN_CreateAt(Mode_S(&t, "Humphrey"), spw, "Car", h) && LvN_SetI(h, "LaunchTaunts", 0) && LV_OWN(HU, "GetHumphreyProfile", pd, "") &&
		LvN_StdAI(h, pd) && LvN_SetS(h, "OnDeath", "Mission_DestroyHumphrey.OnHumphreyDeath") && LvN_Life(h, HU, "HUMPHREY_LIFE") &&
		Mode_VGet(h, "Name", name) && Mode_Put(HU, "humphreyName", NULL, name);
	int first = ok ? LvN_GEqI(HU, "numTaunt", -1) : -1;
	if (first > 0) {
		ok = LvN_PutI(HU, "numTaunt", 0) && Mode_Read(HU, "humphreyName", name) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, name, 0L);
	} else if (first == 0) {
		ok = Hu_Enemy(h);
	}
	return Mode_Done(&t, ok && first >= 0 && Mode_Later(35, HU, "LaunchTaunt", NULL, 0, "LaunchTaunt"));
}

/* HumphreyAttacks(): Humphrey red and the target, the scorer of his lives */
static byte Humphrey_HumphreyAttacks(void) {
	nativeVal_t hn = { 0 }, h = { 0 };
	byte ok = Mode_G(HU, "humphreyName", &hn) && Mode_EntityH(&hn, &h) && (!Native_True(&h) || Hu_Enemy(&h)) && LvN_ScorerCallG(HU, "MissionScorer") &&
		LvN_Draw();
	Native_Free(&hn);
	Native_Free(&h);
	return ok;
}

/* LaunchTaunt(): one of three taunts, the next one 20 s later */
static byte Humphrey_LaunchTaunt(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = TV(), *s = TV(), *rep = TV(), *key = TV(), *n1 = TV();
	byte ok = Mode_G(HU, "numTaunt", n) && Native_Arith('+', Mode_I(&t, 3), n, s);
	if (ok) {
		Mode_Repr(s, rep);
	}
	ok = ok && Native_Arith('+', Mode_S(&t, "Mission_DestroyHumphrey_RemoteMessage"), rep, key) &&
		LV_CALL("Speech", "AddRemoteCom", NULL, "vs", key, "EvilHumphrey") && Mode_Read(HU, "numTaunt", n) && Native_Arith('+', n, Mode_I(&t, 1), n1) &&
		(Native_Free(s), Mode_Arith('%', n1, Mode_I(&t, 3), s)) && Mode_Put(HU, "numTaunt", NULL, s) &&
		Mode_Later(20, HU, "LaunchTaunt", NULL, 0, "LaunchTaunt");
	return Mode_Done(&t, ok);
}

/* OnHumphreyDeath(LooserName, WinnerName): one life fewer; the last one: Betty, the noise, the boss; else he comes
 * back 3 s later */
static byte Humphrey_OnHumphreyDeath(const nativeVal_t *a) {
	static const char msg[] = "Mission_DestroyHumphrey_RemoteMessage6";
	modeVals_t t = { .n = 0 };
	nativeVal_t *lifes = TV(), *n = TV(), *rep = TV(), *text = TV(), *st = TV(), *d = TV(), *now = TV(), *at = TV(), *args = TV();
	for (int i = 1; i < 4; i++) {
		(void)TV();
	}
	byte ok = Mode_Import("Vehicles") && LV_CALL("SScorer", "CancelSpeech", NULL, "") && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) &&
		Mode_DelSchedS("LaunchTaunt") && Mode_SaveInt("NumHumphreyLifes", lifes) && Native_Arith('-', lifes, Mode_I(&t, 1), n);
	if (ok && Native_Compare(n, Mode_I(&t, 0)) < 0) {
		Native_Free(n);
		Native_SetInt(n, 0);
		ok = Mode_SaveS("NumHumphreyLifes", "0");
	} else if (ok) {
		Mode_Repr(n, rep);
		ok = Mode_SetSaveVar("NumHumphreyLifes", rep);
	}
	if (ok && LvN_EqI(n, 0)) {
		PyObject *set = NULL;
		Mode_Lang(msg, text);
		Native_SetFloat(now, Mode_Time());
		Native_SetInt(&args[0], 0);
		LvN_Str(&args[1], "Monitor");
		LvN_Str(&args[2], "Noise");
		Native_SetInt(&args[3], 1);
		ok = LvN_ScorerOff() && LV_CALL("Speech", "CalculateSpeechTime", st, "v", text) && Native_Arith('-', st, Mode_I(&t, 2), d) &&
			Native_Arith('+', now, d, at) && (set = Script_Attr("SScorer", "Set")) != NULL && Mode_Schedule(at, set, args, 4, NULL) &&
			LvN_Com(msg, "Betty", HU, "BossAppears", LvN_NoArgs());
		Py_XDECREF(set);
	} else if (ok) {
		ok = Mode_Later(3, HU, "CreateHumphrey", NULL, 0, NULL);
	}
	return Mode_Done(&t, ok && LvN_Draw());
}

/* MissionScorer(id): Humphrey's lives left */
static byte Humphrey_MissionScorer(const nativeVal_t *id) {
	nativeVal_t y = { 0 }, n = { 0 };
	byte ok = Mode_Import("Scorer") && Mode_LvLabel(id, "RemainingLifesText", 0, "Mission_DestroyHumphrey_Scorer1", 0) && LvN_TextY(20, &y);
	if (ok) {
		Mode_SaveVar("NumHumphreyLifes", &n);
		ok = Mode_LvText(id, "RemainingLifes", &y, &n, NULL);
	}
	Native_Free(&y);
	Native_Free(&n);
	return ok;
}

LV_CORE0(Humphrey_CreateHumphrey)
LV_CORE0(Humphrey_HumphreyAttacks)
LV_CORE0(Humphrey_LaunchTaunt)
LV_COREA(Humphrey_OnHumphreyDeath)
LV_COREA(Humphrey_MissionScorer)
LV_FN(Humphrey, CreateHumphreyPy, 0, Humphrey_CreateHumphrey_C)
LV_FN(Humphrey, HumphreyAttacksPy, 0, Humphrey_HumphreyAttacks_C)
LV_FN(Humphrey, LaunchTauntPy, 0, Humphrey_LaunchTaunt_C)
LV_FN(Humphrey, OnHumphreyDeathPy, 2, Humphrey_OnHumphreyDeath_C)
LV_FN(Humphrey, MissionScorerPy, 1, Humphrey_MissionScorer_C)

PyMethodDef g_OutskirtsDestroyHumphreyMethods[] = {
	{ "GetHumphreyProfile", Humphrey_GetHumphreyProfile, 1, NULL },
	{ "InitStatus", Humphrey_InitStatus, 1, NULL },
	{ "CreateHumphrey", Humphrey_CreateHumphreyPy, 1, NULL },
	{ "HumphreyAttacks", Humphrey_HumphreyAttacksPy, 1, NULL },
	{ "LaunchTaunt", Humphrey_LaunchTauntPy, 1, NULL },
	{ "OnHumphreyDeath", Humphrey_OnHumphreyDeathPy, 1, NULL },
	{ "MissionScorer", Humphrey_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_IdentifyMerc: the lonely mercenary kills Traitorius in a scene, the player scans him, DeepThroat ---- */

static const char *const lonely_normal_profile[] = { "Ship=SLMerc", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"GoNextItemRace=1", "ControlHookRace=1", "CadenceShootCM=0", "UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25",
	"UsePercHook=100", "FactorHookMap=5", "CadenceShoot=2", "ControlAim=10", "AngAttack=3.1415", "CounterAttack=1", "ControlBoost=1",
	"ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.45", "FactorDamageItemLife=0.35", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=1", "UsePercHookGetAway=100", "TimeMaxStrafe=10", "VelMaxStrafe=1.0", NULL };

/* the profile of the scene: he must kill Traitorius for sure */
static const char *const lonely_hard_profile[] = { "Ship=SLMerc", "ControlStrafe=2", "ControlBoost=1", "ControlAim=10", "CadenceShoot=0",
	"AngAttack=3.14", "FactorDamageItemLife=0.0", "ControlBrake=0", "StupidPathFinding=0", "FactorDamageGetAway=0.1", "TimeMaxStrafe=2",
	"VelMaxStrafe=0.25", NULL };

/* CreateLonelyMercProfiles(): VehiclesNPC.AIProf["LonelyMercNormal"] and ["LonelyMercHard"] */
static byte Merc_CreateLonelyMercProfiles(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pd2 = TV(), *pd1 = TV();
	byte ok = LvN_Profile(lonely_normal_profile, pd2) && Mode_ModDictPut("VehiclesNPC", "AIProf", Mode_S(&t, "LonelyMercNormal"), pd2) &&
		LV_CALL("VehiclesNPC", "cShipAI", pd1, "") && Mode_SetAttrs(pd1, lonely_hard_profile) &&
		Mode_ModDictPut("VehiclesNPC", "AIProf", Mode_S(&t, "LonelyMercHard"), pd1);
	return Mode_Done(&t, ok);
}

/* InitStatus(LevelPath): no infinite lives, the traffic, the player comes out of the elevator of GamblinDen */
static byte Merc_InitStatus(void) {
	static const char *const from[] = { "ComeFrom", "DoorElevator", "PrevMap", "Levels/GamblinDen", "Spawn", "DM_Player_Spawn_GamblinDen", NULL };
	if (!Mode_Import("Traffic") || !Mode_SaveS("Player.InfiniteLives", "0") || !Mode_AcShips(IM, ships_police5)) {
		return 0;
	}
	Mode_Saves(from);
	return 1;
}

/* the boarding callbacks of the mission and the profiles of the mercenary */
static byte Im_Setup(void) {
	return Mode_Import("Parking") && LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", IM, "InitMission") &&
		LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", IM, "StopMission") && LV_OWN(IM, "CreateLonelyMercProfiles", NULL, "");
}

/* Init(LevelPath): the scene when the elevator is down, the mission after the scene, else the phone cab */
static byte Merc_Init(void) {
	if (LvN_SaveIs("MissionState", "IdentifyMerc")) {
		return Im_Setup() && LvN_ModSetGlobal("Elevator", "OnElevatorReachDown", IM, "InitIdentifyMerc");
	}
	if (LvN_SaveIs("MissionState", "IdentifyMerc2")) {
		return Im_Setup() && LvN_Com("Mission_MeetingWithMrX_RemoteMessage2", "DeepThroat", IM, "PreInitMission", NULL);
	}
	return LV_OWN(IM, "SetPhoneCab", NULL, "");
}

/* the camera of the player behind or in front of target: CamDist, CamRot, CamAng, no turn */
static byte Im_Camera(long dist, long rot, const nativeVal_t *target) {
	nativeVal_t cn = { 0 }, cam = { 0 };
	byte ok = LV_CALL("Scrap", "GetCam", &cn, "i", 0L) && Mode_EntityH(&cn, &cam) && LvN_SetI(&cam, "CamDist", dist) && LvN_SetI(&cam, "CamRot", rot) &&
		LvN_SetI(&cam, "CamAng", -rot) && LvN_SetI(&cam, "AddTurn", 0) && Mode_VSet(&cam, "Target", target);
	Native_Free(&cn);
	Native_Free(&cam);
	return ok;
}

/* InitIdentifyMerc(elevator=None): the scene: the player out of the elevator, Traitorius waiting in front of him */
static byte Merc_InitIdentifyMerc(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *v = TV(), *name = TV(), *tr = TV(), *u2 = TV(), *up = TV();
	byte ok = Mode_Import("RaceMaker") && LvN_ModNone("Elevator", "OnElevatorReachDown") && Mode_UsrHandle(usr);
	if (ok) {
		Native_SetVec(v, -258322.203125, 65950.4609375, -194309.25);
		ok = Mode_VSet(usr, "Pos", v);
		Native_SetVec(v, 0.0, -142.108001709, 0.0);
		ok = ok && Mode_VSet(usr, "Ang", v);
		Native_SetVec(v, 3.38299465179, -142.108016968, 0.0);
		ok = ok && Mode_VSet(usr, "ViewAng", v) && Mode_VGet(usr, "Name", name) && LvN_SetOnFloor(name) &&
			LV_CALL("Scene", "InitOutDoorScene", NULL, "i", 0L) && LV_CALL("VehiclesNPC", "MakeAI", tr, "ss", "SFunc1", "Traitorius") &&
			Mode_Put(IM, "traitorius", NULL, tr) && Mode_Read(IM, "traitorius", tr);
	}
	if (ok) {
		Native_SetVec(v, -252843.578125, 67933.2734375, -187826.109375);
		ok = Mode_VSet(tr, "Pos", v) && LvN_SetI(tr, "MaxLife", 1) && LvN_SetI(tr, "Life", 1) && LvN_SetI(tr, "Money", -1) &&
			(Native_Free(name), Mode_VGet(tr, "Name", name)) && LV_CALL("SAI", "SetStateVehicle", NULL, "iv", 0L, name) && Mode_UsrHandle(u2) &&
			Mode_VGet(u2, "Pos", up) && LV_CALL("RaceMaker", "PointObjectTo", NULL, "vv", tr, up) && Im_Camera(1000, 20, name) &&
			LV_CALL("Scene", "SetText", NULL, "si", "Mission_MeetingWithMrX_RemoteMessage1", 0L) &&
			Mode_Later(3.0, IM, "CreateLonelyMercInScene", NULL, 0, NULL);
	}
	return Mode_Done(&t, ok);
}

/* CreateLonelyMercInScene(): the mercenary comes for Traitorius, the camera on him */
static byte Merc_CreateLonelyMercInScene(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *m = TV(), *v = TV(), *name = TV(), *tr = TV(), *tn = TV(), *tp = TV();
	byte ok = Mode_Import("RaceMaker") && LV_OWN(IM, "CreateLonelyMerc", m, "") && Mode_Put(IM, "lonelyMerc", NULL, m) && Mode_Read(IM, "lonelyMerc", m);
	if (ok) {
		Native_SetVec(v, -232476.484375, 68936.765625, -144671.078125);
		ok = Mode_VSet(m, "Pos", v) && Mode_VGet(m, "Name", name) && Mode_G(IM, "traitorius", tr) && Mode_VGet(tr, "Name", tn) &&
			LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, name, tn) && Mode_Read(IM, "traitorius", tr) && Mode_VGet(tr, "Pos", tp) &&
			LV_CALL("RaceMaker", "PointObjectTo", NULL, "vv", m, tp) && Im_Camera(1500, 10, name) &&
			Mode_Later(3.0, IM, "CheckTraitoriusDeath", NULL, 0, NULL) && Mode_Later(2, IM, "EndOfScene", NULL, 0, NULL);
	}
	return Mode_Done(&t, ok);
}

/* CheckTraitoriusDeath(): Traitorius dies if the mercenary missed */
static byte Merc_CheckTraitoriusDeath(void) {
	nativeVal_t e = { 0 };
	byte ok = LvN_Entity("Traitorius", &e) && Mode_Put(IM, "traitorius", NULL, &e) && Mode_Read(IM, "traitorius", &e) &&
		(!Native_True(&e) || LvN_SetI(&e, "Life", 0));
	Native_Free(&e);
	return ok;
}

/* EndOfScene(): the scene ends, the mercenary goes (created again later), DeepThroat's message */
static byte Merc_EndOfScene(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *args = TV(), *who = TV(), *fn = TV(), *d = TV();
	LvN_Str(args, "Mission_MeetingWithMrX_RemoteMessage2");
	LvN_Str(who, "DeepThroat");
	Native_SetInt(d, 2);
	byte ok = LV_CALL("Scene", "EndOutDoorScene", NULL, "") && Mode_SaveS("MissionState", "IdentifyMerc2") &&
		Mode_Later(0.5, IM, "DelLonelyMerc", NULL, 0, NULL) && LvN_GObj(IM, "PreInitMission", fn) && Mode_LaterSpeech("AddRemoteCom", d, args, 3, NULL);
	return Mode_Done(&t, ok);
}

/* DelLonelyMerc(): the mercenary of the scene goes. C, not data: the converter wrote the method call on the global
 * as "call lonelyMerc.Del", which the .def runs as a module call (ImportError lonelyMerc), so he stayed */
static byte Merc_DelLonelyMerc(void) {
	nativeVal_t m = { 0 };
	byte ok = Mode_G(IM, "lonelyMerc", &m) && (!Native_True(&m) || (Mode_VDel(&m) && (Native_Free(&m), Mode_Put(IM, "lonelyMerc", NULL, &m))));
	Native_Free(&m);
	return ok;
}

/* CreateLonelyMerc(difficulty="Hard"): the mercenary; hard: only one missile (his weapons kept for later); normal:
 * the weapons kept and the life of the mission */
static byte Merc_CreateLonelyMercCore(const nativeVal_t *difficulty, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *m = TV(), *v = TV(), *name = TV();
	byte normal = LvN_EqS(difficulty, "Normal");
	byte ok = LV_CALL("VehiclesNPC", "MakeAI", m, "ss", normal ? "LonelyMercNormal" : "LonelyMercHard", "LonelyMerc") &&
		Mode_Put(IM, "lonelyMerc", NULL, m) && Mode_Read(IM, "lonelyMerc", m) && LvN_SetS(m, "OnDeath", "Mission_IdentifyMerc.OnLonelyMercDeath") &&
		LvN_SetS(m, "OnDamage", "Mission_IdentifyMerc.OnLonelyMercDamage") && LvN_SetI(m, "LaunchTaunts", 0) && LvN_SetI(m, "Money", 300);
	if (ok && normal) {
		ok = Mode_G(IM, "prevWeapList", v) && Mode_VSet(m, "WeapList", v) && (Native_Free(v), Mode_G(IM, "prevAmmo", v)) && Mode_VSet(m, "Ammo01", v) &&
			LvN_Life(m, IM, "LONELY_MERC_LIFE");
	} else if (ok) {
		ok = Mode_VGet(m, "WeapList", v) && Mode_Put(IM, "prevWeapList", NULL, v) && (Native_Free(v), Mode_VGet(m, "Ammo01", v)) &&
			Mode_Put(IM, "prevAmmo", NULL, v) && LvN_SetS(m, "WeapList", "0,15,0,0,0,0");
	}
	ok = ok && Mode_VGet(m, "Name", name) && Mode_GVarSet("CurrentWaypointName", name) && Mode_G(IM, "lonelyMerc", out);
	return Mode_Done(&t, ok);
}

static PyObject *Merc_CreateLonelyMercPy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "difficulty", NULL };
	PyObject *difficulty = NULL;
	nativeVal_t d = { 0 }, out = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", kwlist, &difficulty)) {
		return NULL;
	}
	if (difficulty) {
		Native_Arg(difficulty, &d);
	} else {
		LvN_Str(&d, "Hard");
	}
	byte ok = Merc_CreateLonelyMercCore(&d, &out);
	Native_Free(&d);
	return Mode_Return(ok, &out);
}

/* lonelyMerc = CreateLonelyMerc("Normal") (the global function of the module) */
static byte Im_NewNormalMerc(void) {
	nativeVal_t m = { 0 };
	byte ok = LV_OWN(IM, "CreateLonelyMerc", &m, "s", "Normal") && Mode_Put(IM, "lonelyMerc", NULL, &m);
	Native_Free(&m);
	return ok;
}

/* MissionsFuncs.SetMissionTarget(lonelyMerc.Name) */
static byte Im_MercTarget(void) {
	nativeVal_t m = { 0 }, name = { 0 };
	byte ok = Mode_G(IM, "lonelyMerc", &m) && Mode_VGet(&m, "Name", &name) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", &name);
	Native_Free(&m);
	Native_Free(&name);
	return ok;
}

/* InitMission(): in a ship: the mercenary as the target, the scorer, the scan every second */
static byte Merc_InitMission(void) {
	nativeVal_t m = { 0 }, name = { 0 };
	byte ok = Mode_G(IM, "lonelyMerc", &m) && (Native_True(&m) || (Im_NewNormalMerc() && Mode_Read(IM, "lonelyMerc", &m) &&
		Mode_VGet(&m, "Name", &name) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, &name, 0L))) && Im_MercTarget() &&
		LvN_ScorerCallG(IM, "MissionScorer") && LvN_Draw() && Mode_GVarSetS("OnFinishAutoUpdate", "Mission_IdentifyMerc.OnFinishScan") &&
		Mode_Later(1.0, IM, "ScanShip", NULL, 0, "ScanShip");
	Native_Free(&m);
	Native_Free(&name);
	return ok;
}

/* the lonely mercenary as SAI.SetStateVehicle(state, lonelyMerc.Name, ...) with the rest of the arguments */
static byte Im_MercState(long state, const nativeVal_t *target, long dist) {
	nativeVal_t m = { 0 }, name = { 0 };
	byte ok = Mode_G(IM, "lonelyMerc", &m) && Mode_VGet(&m, "Name", &name) &&
		(target ? LV_CALL("SAI", "SetStateVehicle", NULL, "ivvi", state, &name, target, dist) :
			LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", state, &name, dist));
	Native_Free(&m);
	Native_Free(&name);
	return ok;
}

/* ScanShip(): every second: the scan runs while the mercenary is near; lost too long, it starts again; halfway he
 * flees */
static byte Merc_ScanShip(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *m = TV(), *mpos = TV(), *up = TV(), *sub = TV(), *d = TV(), *m1 = TV(), *m2 = TV(), *max = TV(), *name = TV();
	nativeVal_t *state = TV(), *delay = TV(), *v = TV(), *mt = TV(), *half = TV(), *un = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_G(IM, "lonelyMerc", m) || !Mode_VGet(m, "Pos", mpos)) {
		/* except: the scan again a second later (OBJLIB 7: the error is the Python exception until errors are a C state) */
		PyErr_Clear();
		return Mode_Done(&t, Mode_Later(1.0, IM, "ScanShip", NULL, 0, "ScanShip"));
	}
	byte ok = Mode_VGet(usr, "Pos", up) && Mode_VecSub(mpos, up, sub) && Mode_VecModSqr(sub, d) && Mode_G(IM, "MAX_SCANNING_DIST", m1) &&
		Mode_G(IM, "MAX_SCANNING_DIST", m2) && Mode_Arith('*', m1, m2, max);
	int scanning = ok ? LvN_GTrue(IM, "scanningLonelyMerc") : -1;
	if (scanning > 0) {
		ok = Mode_Read(IM, "lonelyMerc", m) && Mode_VGet(m, "Name", name) && Mode_AIState(name, state);
		if (ok && Native_Compare(d, max) > 0) {
			ok = LvN_PutI(IM, "scanningLonelyMerc", 0) && LvN_Set0I("ScanRemainingTime", "Blink", 1) &&
				LvN_Set0I("ScanRemainingTime", "IsAutoUpdatable", 0) && (LvN_EqI(state, 2) || Im_MercState(3, NULL, 0)) &&
				Mode_G(IM, "MAX_TIME_TARGET_LOST", delay) && Mode_LaterV(delay, IM, "ResetScan", NULL, 0, "ResetScan");
		} else if (ok) {
			Mode_ScorerGet(0, "ScanRemainingTime", "Value", v);
			ok = Mode_G(IM, "MAX_SCANNING_TIME", mt) && Mode_Arith('/', mt, Mode_I(&t, 2), half);
			if (ok && Native_Compare(v, half) < 0 && !LvN_EqI(state, 2) && !LvN_EqI(state, 5)) {
				ok = Mode_VGet(usr, "Name", un) && Im_MercState(5, un, 100000);
			}
		}
	} else if (scanning == 0 && Native_Compare(d, max) < 0) {
		ok = LvN_PutI(IM, "scanningLonelyMerc", 1) && LvN_Set0I("ScanRemainingTime", "Blink", 0) &&
			LvN_Set0I("ScanRemainingTime", "IsAutoUpdatable", 1) && LvN_Set0I("ScanStatusText", "Visible", 1) &&
			LvN_Set0I("ScanRemainingTime", "Visible", 1) && Mode_DelSchedS("ResetScan");
	}
	return Mode_Done(&t, ok && scanning >= 0 && Mode_Later(1.0, IM, "ScanShip", NULL, 0, "ScanShip"));
}

/* OnFinishScan(): the scan bar ran out: identified, DeepThroat calls, no more scan */
static byte Merc_OnFinishScan(void) {
	nativeVal_t usr = { 0 }, m = { 0 };
	byte ok = Mode_GVarSetS("CurrentWaypointName", "") && LvN_Set0I("ScanStatusText", "Visible", 0) && LvN_Set0I("ScanRemainingTime", "Visible", 0) &&
		LvN_PutI(IM, "scanningLonelyMerc", 0) && Mode_UsrHandle(&usr);
	if (ok && Native_True(&usr)) {
		ok = Mode_G(IM, "lonelyMerc", &m) && (!Native_True(&m) || (Mode_Import("Parking") && LvN_ModNone("Parking", "SwitchVehicleMissionCallback") &&
			LvN_ModNone("Parking", "SwitchCharMissionCallback") && LvN_SetS(&m, "EntityList", "") && Mode_Read(IM, "lonelyMerc", &m) &&
			LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &m, 0L) && Mode_SaveS("MissionState", "CallDeepThroat") &&
			LvN_Com("Mission_IdentifyMerc_RemoteMessage3", "DeepThroat", IM, "PreCallDeepThroat", NULL) && LvN_ScorerOff() && LvN_Draw() &&
			Mode_DelSchedS("ScanShip")));
	}
	Native_Free(&usr);
	Native_Free(&m);
	return ok;
}

/* OnLonelyMercDeath(LooserName, WinnerName): killed before the scan: DeepThroat, he comes back */
static byte Merc_OnLonelyMercDeath(const nativeVal_t *a) {
	nativeVal_t none = { 0 };
	byte ok = Mode_Import("Vehicles") && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && LV_OWN(IM, "ResetScan", NULL, "") &&
		Mode_DelSchedS("ScanShip") && (!(LvN_SaveIs("MissionState", "IdentifyMerc") || LvN_SaveIs("MissionState", "IdentifyMerc2")) ||
			LvN_Com("Mission_IdentifyMerc_RemoteMessage1", "DeepThroat", IM, "PreRespawnLonelyMerc", NULL));
	return ok && Mode_Put(IM, "lonelyMerc", NULL, &none);
}

/* OnLonelyMercDamage(LooserName, WinnerName, Damage): hit by the player: he attacks him */
static byte Merc_OnLonelyMercDamage(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *un = TV(), *st = TV(), *e = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	int hit = Mode_VGet(usr, "Name", un) ? !Native_Compare(&a[1], un) : -1;
	if (hit > 0) {
		hit = Mode_AIState(&a[0], st) ? !LvN_EqI(st, 2) : -1;
	}
	byte ok = hit >= 0 && (!hit || (LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, &a[0], &a[1]) && Mode_EntityH(&a[0], e) &&
		LvN_SetS(e, "TgType", "red")));
	return Mode_Done(&t, ok);
}

static byte Merc_PreRespawnLonelyMerc(void) {
	return Mode_Later(2.0, IM, "RespawnLonelyMerc", NULL, 0, NULL);
}

/* RespawnLonelyMerc(): a new mercenary as the target, the scan goes on */
static byte Merc_RespawnLonelyMerc(void) {
	return Im_NewNormalMerc() && Im_MercTarget() && LvN_Com("Mission_IdentifyMerc_RemoteMessage2", "DeepThroat", NULL, NULL, NULL) &&
		LV_OWN(IM, "ScanShip", NULL, "");
}

/* SetPhoneCab(): the call to DeepThroat from a phone cab */
static byte Merc_SetPhoneCab(void) {
	static const char *const mods[] = { "Vehicles", "Speech", "Alarm", "SaveGame", "PhoneCab", "MissionsFuncs", NULL };
	byte ok = 1;
	for (int i = 0; ok && mods[i]; i++) {
		ok = Mode_Import(mods[i]);
	}
	return ok && Mode_PhoneCab("Mission_IdentifyMerc.UsePhoneCab", 1, IM, NULL);
}

static byte Merc_UsePhoneCab(const nativeVal_t *a) {
	int alarm = Mode_PhoneUse(IM, a);
	if (alarm > 0) {
		return Os_PoliceTalk(IM, "Generics_StillPolice1_DeepThroat", "DeepThroat");
	}
	return alarm == 0 && Os_PhoneTalk(IM, "SetNextMission", "DeepThroat", "Mission_TalkToDeepThroat3_ComDeepThroat_NI_1Msg");
}

/* SetNextMission(): the cab no longer a target, on to Mission_ProtectGus in Commercial */
static byte Merc_SetNextMission(void) {
	nativeVal_t pc = { 0 };
	byte ok = Mode_DelSaveVars("MissionState") && Mode_G(IM, "pc", &pc) &&
		(!Native_True(&pc) || (LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &pc, 0L) && LvN_SetI(&pc, "ArrowCanUse", 0))) &&
		Mode_GSetS(IM, "pc", "UseFunc", "PhoneCab.DefUseAudioCom") && LvN_Next("Mission_ProtectGus", "Commercial", "Mission_ProtectGus_MissionText1") &&
		Mode_WayPoint();
	Native_Free(&pc);
	return ok;
}

/* MissionScorer(id): the scan text and bar, visible while the scan runs */
static byte Merc_MissionScorer(const nativeVal_t *id) {
	static const char bar[] = "ScanRemainingTime";
	modeVals_t t = { .n = 0 };
	nativeVal_t *y = TV(), *lab = TV(), *vis = TV(), *mt = TV();
	byte ok = Mode_Import("Scorer") && LvN_TextY(0, y);
	if (ok) {
		LvN_Label("Mission_IdentifyMerc_Scorer1", lab);
		ok = Mode_G(IM, "scanningLonelyMerc", vis) && Mode_LvText(id, "ScanStatusText", y, lab, vis) && Mode_ScorerAddS(id, bar, "Bar") &&
			Mode_BarDefaults(id, bar, NULL, NULL) && Mode_ScorerSetI(id, bar, "IsAutoUpdatable", 1) && Mode_G(IM, "MAX_SCANNING_TIME", mt) &&
			LvN_ScorerSet(id, bar, "MaxValue", mt);
	}
	int c = ok ? LvN_GTrue(IM, "scanningLonelyMerc") : -1;
	ok = c >= 0 && (c || (Mode_Read(IM, "MAX_SCANNING_TIME", mt) && LvN_ScorerSet(id, bar, "Value", mt))) && Mode_Read(IM, "scanningLonelyMerc", vis) &&
		LvN_ScorerSet(id, bar, "Visible", vis);
	return Mode_Done(&t, ok);
}

/* the keyword adapter of InitIdentifyMerc(elevator=None) */
static PyObject *Merc_InitIdentifyMercPy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "elevator", NULL };
	PyObject *elevator = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", kwlist, &elevator)) {
		return NULL;
	}
	return Mode_Result(Merc_InitIdentifyMerc());
}

LV_CORE0(Merc_CreateLonelyMercProfiles)
LV_CORE0(Merc_InitStatus)
LV_CORE0(Merc_Init)
LV_CORE0(Merc_CreateLonelyMercInScene)
LV_CORE0(Merc_CheckTraitoriusDeath)
LV_CORE0(Merc_EndOfScene)
LV_CORE0(Merc_DelLonelyMerc)
LV_CORE0(Merc_InitMission)
LV_CORE0(Merc_ScanShip)
LV_CORE0(Merc_OnFinishScan)
LV_COREA(Merc_OnLonelyMercDeath)
LV_COREA(Merc_OnLonelyMercDamage)
LV_CORE0(Merc_PreRespawnLonelyMerc)
LV_CORE0(Merc_RespawnLonelyMerc)
LV_CORE0(Merc_SetPhoneCab)
LV_COREA(Merc_UsePhoneCab)
LV_CORE0(Merc_SetNextMission)
LV_COREA(Merc_MissionScorer)
LV_FN(Merc, CreateLonelyMercProfilesPy, 0, Merc_CreateLonelyMercProfiles_C)
LV_FN(Merc, InitStatusPy, 1, Merc_InitStatus_C)
LV_FN(Merc, InitPy, 1, Merc_Init_C)
LV_FN(Merc, CreateLonelyMercInScenePy, 0, Merc_CreateLonelyMercInScene_C)
LV_FN(Merc, CheckTraitoriusDeathPy, 0, Merc_CheckTraitoriusDeath_C)
LV_FN(Merc, EndOfScenePy, 0, Merc_EndOfScene_C)
LV_FN(Merc, DelLonelyMercPy, 0, Merc_DelLonelyMerc_C)
LV_FN(Merc, InitMissionPy, 0, Merc_InitMission_C)
LV_FN(Merc, ScanShipPy, 0, Merc_ScanShip_C)
LV_FN(Merc, OnFinishScanPy, 0, Merc_OnFinishScan_C)
LV_FN(Merc, OnLonelyMercDeathPy, 2, Merc_OnLonelyMercDeath_C)
LV_FN(Merc, OnLonelyMercDamagePy, 3, Merc_OnLonelyMercDamage_C)
LV_FN(Merc, PreRespawnLonelyMercPy, 0, Merc_PreRespawnLonelyMerc_C)
LV_FN(Merc, RespawnLonelyMercPy, 0, Merc_RespawnLonelyMerc_C)
LV_FN(Merc, SetPhoneCabPy, 0, Merc_SetPhoneCab_C)
LV_FN(Merc, UsePhoneCabPy, 4, Merc_UsePhoneCab_C)
LV_FN(Merc, SetNextMissionPy, 0, Merc_SetNextMission_C)
LV_FN(Merc, MissionScorerPy, 1, Merc_MissionScorer_C)

PyMethodDef g_OutskirtsIdentifyMercMethods[] = {
	{ "CreateLonelyMercProfiles", Merc_CreateLonelyMercProfilesPy, 1, NULL },
	{ "InitStatus", Merc_InitStatusPy, 1, NULL },
	{ "Init", Merc_InitPy, 1, NULL },
	{ "InitIdentifyMerc", (PyCFunction)(void (*)(void))Merc_InitIdentifyMercPy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateLonelyMercInScene", Merc_CreateLonelyMercInScenePy, 1, NULL },
	{ "CheckTraitoriusDeath", Merc_CheckTraitoriusDeathPy, 1, NULL },
	{ "EndOfScene", Merc_EndOfScenePy, 1, NULL },
	{ "DelLonelyMerc", Merc_DelLonelyMercPy, 1, NULL },
	{ "CreateLonelyMerc", (PyCFunction)(void (*)(void))Merc_CreateLonelyMercPy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "InitMission", Merc_InitMissionPy, 1, NULL },
	{ "ScanShip", Merc_ScanShipPy, 1, NULL },
	{ "OnFinishScan", Merc_OnFinishScanPy, 1, NULL },
	{ "OnLonelyMercDeath", Merc_OnLonelyMercDeathPy, 1, NULL },
	{ "OnLonelyMercDamage", Merc_OnLonelyMercDamagePy, 1, NULL },
	{ "PreRespawnLonelyMerc", Merc_PreRespawnLonelyMercPy, 1, NULL },
	{ "RespawnLonelyMerc", Merc_RespawnLonelyMercPy, 1, NULL },
	{ "SetPhoneCab", Merc_SetPhoneCabPy, 1, NULL },
	{ "UsePhoneCab", Merc_UsePhoneCabPy, 1, NULL },
	{ "SetNextMission", Merc_SetNextMissionPy, 1, NULL },
	{ "MissionScorer", Merc_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_KillRudolph: the race against Rudolph, then his death for the clue, then the police boss ---- */

static const char *const rudolph_profile[] = { "Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"GoNextItemRace=1", "ControlHookRace=1", "CadenceShootCM=3", "UsePercSonic=90", "UsePercCloud=0", "UsePercElectro=10",
	"UsePercHook=35", "FactorHookMap=8", "CadenceShoot=5", "ControlAim=4", "AngAttack=1.5", "CounterAttack=1", "ControlBoost=1",
	"ControlBoostItemLife=1", "ControlBoostItemAmmo=0", "FactorDamageGetAway=0.20", "FactorDamageItemLife=0.25", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=1", "UsePercHookGetAway=70", "TimeMaxStrafe=6", "VelMaxStrafe=0.75", "LifeMult=1.5", NULL };

static PyObject *Rudolph_GetRudolphProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, KR, rudolph_profile);
}

/* RaceList(): the checkpoints of the race (the same as MapRace.EasyRaceOurskirts1) */
static PyObject *Rudolph_RaceList(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_StartPos(args, races[0].pos, races[0].n);
}


/* InitStatus(LevelPath): the race of the mission, no alarm, the traffic */
static byte Rudolph_InitStatus(void) {
	return Mode_Import("MapRace") && Mode_ModListG("MapRace", "Races", KR, "RaceList") && Mode_SaveS("AlarmActive", "0") &&
		Mode_SaveS("AlarmStatus", "0") && Mode_AcShips(KR, ships_police5);
}

/* Init(LevelPath): the race, Rudolph to kill or the call by the state */
static byte Rudolph_Init(void) {
	nativeVal_t model = { 0 };
	byte ok = Mode_G(KR, "DataPackModel", &model) && Mode_Preload(&model);
	Native_Free(&model);
	if (!ok) {
		return 0;
	}
	if (LvN_SaveIs("KillRudolphMissionData", "RudolphRace")) {
		return LV_OWN(KR, "InitRace", NULL, "");
	}
	return LV_OWN(KR, LvN_SaveIs("KillRudolphMissionData", "RudolphKiller") ? "initCreateRudolph" : "InitTalkToPoliceBoss", NULL, "");
}

static PyObject *Rudolph_GetDealType(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return PyString_FromString("Race");
}

/* MissionsFuncs.SetMissionTarget(rudolphEntityName) */
static byte Kr_Target(void) {
	nativeVal_t n = { 0 };
	byte ok = Mode_G(KR, "rudolphEntityName", &n) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", &n);
	Native_Free(&n);
	return ok;
}

/* InitRace(): the race against Rudolph and three more functionaries */
static byte Rudolph_InitRace(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *zero = TV(), *fn = TV(), *n = TV(), *v = TV();
	Native_SetInt(zero, 0);
	LvN_Str(fn, "Functionary");
	byte ok = Mode_UsrHandle(usr) && Mode_SetAlarm(zero) && Mode_SaveS("Challenge.Money", "0") && Mode_SaveS("Race.Profile", "Pilot") &&
		LvN_ModSetGlobal("RaceMaker", "GetDealType", KR, "GetDealType") && LvN_ModSetGlobal("RaceMaker", "EndOfRaceCallback", KR, "EndOfRace");
	int car = ok ? Mode_VIsType(usr, "Car") : -1;
	ok = car >= 0 && Mode_CallKw("RaceMaker", "InitRace", NULL, fn, 1, Py_BuildValue("{s:i,s:i,s:i}", "numply", 4, "waypointlimit", 15, "userOnShip", car)) &&
		Kr_Target() && Mode_G(KR, "rudolphEntityName", n) && Mode_EntityH(n, v) && (!Native_True(v) || LV_OWN(KR, "SetRudolphDef", NULL, "v", v));
	return Mode_Done(&t, ok);
}

/* initCreateRudolph(): Rudolph at a free spawn point, out of the race, as the target */
static byte Rudolph_initCreateRudolph(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *spw = TV(), *p = TV(), *rn = TV(), *e = TV(), *pd = TV(), *name = TV(), *ship = TV();
	byte ok = LV_CALL("OutMap", "GetFreeSpawnPos", spw, "") && Mode_G(KR, "rudolphEntityName", rn) && LvN_Item(spw, 0, p) &&
		LvN_CreateAt(rn, p, "Car", e) && LV_OWN(KR, "GetRudolphProfile", pd, "") && Mode_VGet(e, "Name", name) && Mode_VGet(pd, "Ship", ship) &&
		LV_CALL("VehiclesNPC", "SetStdProperties", NULL, "vv", name, ship) && LV_OWN(KR, "SetRudolphDef", NULL, "v", e) &&
		LV_OWN(KR, "SetRudolphAsNone", NULL, "") && Kr_Target();
	return Mode_Done(&t, ok);
}

/* SetRudolphDef(v): his profile, no coin, his death drops the clue */
static byte Rudolph_SetRudolphDef(const nativeVal_t *a) {
	nativeVal_t pd = { 0 }, life = { 0 };
	byte ok = LV_OWN(KR, "GetRudolphProfile", &pd, "") && LV_CALL("VehiclesNPC", "ModAIProff", NULL, "vv", &a[0], &pd) && LvN_SetI(&a[0], "Money", -1) &&
		LvN_SetS(&a[0], "OnDeath", "Mission_KillRudolph.RudolphKilled") && Mode_G(KR, "RUDOLPH_LIFE", &life) && Mode_VSet(&a[0], "Life", &life) &&
		Mode_VSet(&a[0], "MinLife", &life) && Mode_VSet(&a[0], "MaxLife", &life);
	Native_Free(&pd);
	Native_Free(&life);
	return ok;
}

/* OnDamage(Looser, Winner, damage): hit: an enemy */
static byte Rudolph_OnDamage(const nativeVal_t *a) {
	nativeVal_t v = { 0 };
	byte ok = Mode_EntityH(&a[0], &v) && (!Native_True(&v) || LvN_SetS(&v, "EntityList", "Enemies"));
	Native_Free(&v);
	return ok;
}

/* EndOfRace(ItemName, VehicleName): Rudolph to kill now; his comment on the result, the reward if the player won */
static byte Rudolph_EndOfRace(const nativeVal_t *a) {
	static const char *const mods[] = { "Speech", "Parking", "Alarm", "Menu", "RaceMaker", "string", "SaveGame", NULL };
	modeVals_t t = { .n = 0 };
	const nativeVal_t *veh = &a[1];
	nativeVal_t *e = TV(), *uc = TV(), *text = TV(), *mr = TV(), *rep = TV(), *msg = TV(), *money = TV(), *sum = TV(), *now = TV();
	byte ok = 1;
	for (int i = 0; ok && mods[i]; i++) {
		ok = Mode_Import(mods[i]);
	}
	ok = ok && Mode_SaveS("KillRudolphMissionData", "RudolphKiller");
	if (ok && veh->type != NV_NONE) {
		if (LvN_EqS(veh, "Alarm") || LvN_EqS(veh, "Killed") || LvN_EqS(veh, "CounterMeasure")) {
			ok = LvN_Com("RaceInterruptedFunctionary", "Rudolph", NULL, NULL, NULL);
		} else if (LvN_EqS(veh, "Left")) {
			ok = LvN_Com("RaceLeftFunctionary", "Rudolph", NULL, NULL, NULL);
		} else if (Mode_EntityH(veh, e) && Mode_VGet(e, "UsrControl", uc)) {
			if (LvN_EqI(uc, 0)) {
				Mode_Lang("RaceSystemWin", text);
				ok = Mode_G(KR, "MONEY_REWARD", mr);
				if (ok) {
					Mode_Repr(mr, rep);
					Mode_Replace(text, "<n>", rep, msg);
				}
				ok = ok && LV_CALL("RaceMaker", "ShowSystemMessage", NULL, "v", msg) && LvN_Com("RaceWin", "Rudolph", NULL, NULL, NULL) &&
					(Native_SetInt(money, Mode_GetMoney()), Mode_Read(KR, "MONEY_REWARD", mr)) && Native_Arith('+', money, mr, sum) && Mode_SetMoney(sum) &&
					LV_CALL("SaveGame", "SaveMoney", NULL, "");
			} else {
				ok = LvN_Com("RaceLooseFunctionary", "Rudolph", NULL, NULL, NULL);
			}
		} else {
			ok = 0;
		}
	}
	Native_SetFloat(now, Mode_Time());
	ok = ok && Mode_AtG(now, KR, "SetRudolphAsNone", NULL, 0);
	return Mode_Done(&t, ok);
}

/* SetRudolphAsNone(): Rudolph out of the race on the traffic paths, an enemy once hit */
static byte Rudolph_SetRudolphAsNone(void) {
	nativeVal_t n = { 0 }, v = { 0 };
	byte ok = Mode_G(KR, "rudolphEntityName", &n) && Mode_EntityH(&n, &v) && (!Native_True(&v) || (LvN_SetS(&v, "OnDamage", "Mission_KillRudolph.OnDamage") &&
		LvN_SetS(&v, "EntityList", "") && Mode_Read(KR, "rudolphEntityName", &n) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, &n, 0L)));
	Native_Free(&n);
	Native_Free(&v);
	return ok;
}

/* RudolphKilled(LooserName, WinnerName): the clue where he died, active after the police boss' message */
static byte Rudolph_RudolphKilled(const nativeVal_t *a) {
	nativeVal_t e = { 0 }, pos = { 0 }, p = { 0 };
	Native_SetEmpty(&p, NV_TUPLE);
	byte ok = Mode_EntityH(&a[0], &e) && Mode_VGet(&e, "Pos", &pos) && LV_OWN(KR, "CreateClue", Native_Append(&p), "v", &pos) && Mode_Import("Vehicles") &&
		LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && LvN_Com("Mission_KillRudolph_RemoteMessage2", "PoliceBoss", KR, "ActivateClue", &p);
	Native_Free(&e);
	Native_Free(&pos);
	Native_Free(&p);
	return ok;
}

/* CreateClue(pos): the data pack of the clue (not takeable yet) */
static byte Rudolph_CreateClue_C(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t n = { 0 }, model = { 0 };
	LvN_Str(&n, "RudolphClue");
	byte ok = LvN_CreateAt(&n, &a[0], "Item", out) && Mode_G(KR, "DataPackModel", &model) && Mode_VModel(out, &model) && LvN_SetF(out, "rTime", 0.25) &&
		LvN_SetI(out, "OnlyUser", 1) && Mode_Import("Items") && LV_CALL("Items", "SetSound", NULL, "vs", out, "TakeBDataPack");
	Native_Free(&n);
	Native_Free(&model);
	return ok;
}

/* ActivateClue(l): the clue takeable, an arrow above it as the target */
static byte Rudolph_ActivateClue(const nativeVal_t *a) {
	nativeVal_t n = { 0 }, pos = { 0 }, l2 = { 0 }, cam = { 0 };
	LvN_Str(&n, "MissionCheckArrow");
	Mode_Ints3(&cam, 0, 1000, 0);
	byte ok = LvN_SetS(&a[0], "OnTake", "Mission_KillRudolph.OnClueTaken") && Mode_VGet(&a[0], "Pos", &pos) && LvN_CreateAt(&n, &pos, "WithLife", &l2) &&
		Mode_VSet(&l2, "CamTgt", &cam) && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &l2, 1L);
	Native_Free(&n);
	Native_Free(&pos);
	Native_Free(&l2);
	Native_Free(&cam);
	return ok;
}

/* OnClueTaken(ItemName, VehicleName): the player took the clue: it and the arrow go, the police boss' message */
static byte Rudolph_OnClueTaken(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *itm = TV(), *arrow = TV();
	int c = Mode_UsrHandle(usr) ? LvN_UsrNameIs(usr, &a[1]) : -1;
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	byte ok = Mode_EntityH(&a[0], itm) && (!Native_True(itm) || Mode_DelLater(itm)) && LvN_Entity("MissionCheckArrow", arrow) && (!Native_True(arrow) || Mode_VDel(arrow)) &&
		LvN_Com("Mission_KillRudolph_RemoteMessage1", "PoliceBoss", KR, "SetNextMission", NULL);
	return Mode_Done(&t, ok);
}

static byte Rudolph_InitTalkToPoliceBoss(void) {
	return Mode_PhoneCab("Mission_KillRudolph.UsePhoneCab", 1, KR, NULL);
}

static byte Rudolph_UsePhoneCab(const nativeVal_t *a) {
	int alarm = Mode_PhoneUse(KR, a);
	if (alarm > 0) {
		return Os_PoliceTalk(KR, "Generics_StillPolice1_PoliceBoss", "PoliceBoss");
	}
	return alarm == 0 && Os_PhoneTalk(KR, "SetNextMission2", "PoliceBoss", "Mission_Get3rdClue_ComPoliceBoss_NI_1Msg");
}

/* SetNextMission2(): the cab free again, the characters' places, on to Mission_CatchTrurl in TownHall */
static byte Rudolph_SetNextMission2(void) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "PoliceBossAtTownHall", "1", "BankMasterAtBank",
		"1", "Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "TownHall", NULL };
	if (!Mode_GSetS(KR, "pc", "UseFunc", "PhoneCab.DefUseAudioCom") || !Mode_Target("") || !Mode_DelSaveVars("KillRudolphMissionData")) {
		return 0;
	}
	Mode_Saves(places);
	return Mode_WayPoint() && LvN_Next("Mission_CatchTrurl", "TownHall", "Mission_GetTrurl_MissionText1");
}

LV_CORE0(Rudolph_InitStatus)
LV_CORE0(Rudolph_Init)
LV_CORE0(Rudolph_InitRace)
LV_CORE0(Rudolph_initCreateRudolph)
LV_COREA(Rudolph_SetRudolphDef)
LV_COREA(Rudolph_OnDamage)
LV_COREA(Rudolph_EndOfRace)
LV_CORE0(Rudolph_SetRudolphAsNone)
LV_COREA(Rudolph_RudolphKilled)
LV_COREA(Rudolph_ActivateClue)
LV_COREA(Rudolph_OnClueTaken)
LV_CORE0(Rudolph_InitTalkToPoliceBoss)
LV_COREA(Rudolph_UsePhoneCab)
LV_CORE0(Rudolph_SetNextMission2)
LV_FN(Rudolph, InitStatusPy, 1, Rudolph_InitStatus_C)
LV_FN(Rudolph, InitPy, 1, Rudolph_Init_C)
LV_FN(Rudolph, InitRacePy, 0, Rudolph_InitRace_C)
LV_FN(Rudolph, initCreateRudolphPy, 0, Rudolph_initCreateRudolph_C)
LV_FN(Rudolph, SetRudolphDefPy, 1, Rudolph_SetRudolphDef_C)
LV_FN(Rudolph, OnDamagePy, 3, Rudolph_OnDamage_C)
LV_FN(Rudolph, EndOfRacePy, 2, Rudolph_EndOfRace_C)
LV_FN(Rudolph, SetRudolphAsNonePy, 0, Rudolph_SetRudolphAsNone_C)
LV_FN(Rudolph, RudolphKilledPy, 2, Rudolph_RudolphKilled_C)
LV_FN(Rudolph, CreateCluePy, 1, Rudolph_CreateClue_C)
LV_FN(Rudolph, ActivateCluePy, 1, Rudolph_ActivateClue_C)
LV_FN(Rudolph, OnClueTakenPy, 2, Rudolph_OnClueTaken_C)
LV_FN(Rudolph, InitTalkToPoliceBossPy, 0, Rudolph_InitTalkToPoliceBoss_C)
LV_FN(Rudolph, UsePhoneCabPy, 4, Rudolph_UsePhoneCab_C)
LV_FN(Rudolph, SetNextMission2Py, 0, Rudolph_SetNextMission2_C)

PyMethodDef g_OutskirtsKillRudolphMethods[] = {
	{ "GetRudolphProfile", Rudolph_GetRudolphProfile, 1, NULL },
	{ "RaceList", Rudolph_RaceList, 1, NULL },
	{ "InitStatus", Rudolph_InitStatusPy, 1, NULL },
	{ "Init", Rudolph_InitPy, 1, NULL },
	{ "GetDealType", Rudolph_GetDealType, 1, NULL },
	{ "InitRace", Rudolph_InitRacePy, 1, NULL },
	{ "initCreateRudolph", Rudolph_initCreateRudolphPy, 1, NULL },
	{ "SetRudolphDef", Rudolph_SetRudolphDefPy, 1, NULL },
	{ "OnDamage", Rudolph_OnDamagePy, 1, NULL },
	{ "EndOfRace", Rudolph_EndOfRacePy, 1, NULL },
	{ "SetRudolphAsNone", Rudolph_SetRudolphAsNonePy, 1, NULL },
	{ "RudolphKilled", Rudolph_RudolphKilledPy, 1, NULL },
	{ "CreateClue", Rudolph_CreateCluePy, 1, NULL },
	{ "ActivateClue", Rudolph_ActivateCluePy, 1, NULL },
	{ "OnClueTaken", Rudolph_OnClueTakenPy, 1, NULL },
	{ "InitTalkToPoliceBoss", Rudolph_InitTalkToPoliceBossPy, 1, NULL },
	{ "UsePhoneCab", Rudolph_UsePhoneCabPy, 1, NULL },
	{ "SetNextMission2", Rudolph_SetNextMission2Py, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyBadDebtors: the bank's money transports, the treasures the bankers and mercenaries fight for ---- */

static const char *const mercs_profile[] = { "Ship=SMERC2", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0", "GoNextItemRace=1",
	"ControlHookRace=0", "CadenceShootCM=3", "UsePercSonic=70", "UsePercCloud=15", "UsePercElectro=15", "UsePercHook=10", "FactorHookMap=10",
	"CadenceShoot=7", "ControlAim=8", "AngAttack=2", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1",
	"FactorDamageGetAway=0.70", "FactorDamageItemLife=0.3", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=10",
	"TimeMaxStrafe=8", "VelMaxStrafe=1.0", NULL };

static const char *const bank_director_profile[] = { "Ship=SBANKMASTER1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=3", "UsePercSonic=70", "UsePercCloud=15", "UsePercElectro=15", "UsePercHook=0",
	"FactorHookMap=3", "CadenceShoot=15", "ControlAim=8", "AngAttack=0.1", "CounterAttack=0", "ControlBoost=1", "ControlBoostItemLife=1",
	"ControlBoostItemAmmo=1", "FactorDamageGetAway=0.70", "FactorDamageItemLife=0.3", "FactorItemAmmo=0.1", "ControlStrafeEvasion=0",
	"UsePercHookGetAway=0", "TimeMaxStrafe=8", "VelMaxStrafe=1.0", NULL };

static const char *const banker_profile[] = { "Ship=SBANKER1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0", "GoNextItemRace=1",
	"ControlHookRace=0", "CadenceShootCM=3", "UsePercSonic=70", "UsePercCloud=15", "UsePercElectro=15", "UsePercHook=0", "FactorHookMap=3",
	"CadenceShoot=2", "ControlAim=1", "AngAttack=2", "CounterAttack=0", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1",
	"FactorDamageGetAway=0.70", "FactorDamageItemLife=0.3", "FactorItemAmmo=0.1", "ControlStrafeEvasion=0", "UsePercHookGetAway=0",
	"TimeMaxStrafe=8", "VelMaxStrafe=1.0", NULL };

static PyObject *Debtors_GetMercenariesProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, BD, mercs_profile);
}

static PyObject *Debtors_GetBankDirectorProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, BD, bank_director_profile);
}

static PyObject *Debtors_GetBankerProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, BD, banker_profile);
}

/* InitStatus(LevelPath): less traffic, its ships */
static byte Debtors_InitStatus(void) {
	return Mode_Import("Traffic") && LvN_ModSetI("Traffic", "NumNormal", 3) && LvN_ModSetI("Traffic", "NumMedium", 0) &&
		LvN_ModSetI("Traffic", "NumHeavy", 0) && Mode_AcShips(BD, ships_police4);
}

/* the meeting point that starts a mission at the dummy pattern: an Item with the OnTake onTake, the global
 * missionStartWaypoint of module (missing: the text raised without the dummy) */
static byte Os_StartWaypoint(const char *module, const char *pattern, const char *missing, const char *onTake) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = TV(), *p = TV(), *wp = TV(), *ang = TV();
	LvN_Dummy(pattern, 1, dummy);
	if (!Native_True(dummy)) {
		return Mode_Done(&t, LvN_RaiseText(missing));
	}
	byte ok = LvN_Item(dummy, 1, p) && LvN_CreateAt(Mode_S(&t, "MissionStart"), p, "Item", wp) && Mode_Put(module, "missionStartWaypoint", NULL, wp) &&
		Mode_Read(module, "missionStartWaypoint", wp) && Mode_VModel(wp, Mode_S(&t, "models/Misc/MeetingPoint/MeetingPoint.m3d")) &&
		LvN_SetI(wp, "RadarShow", 1) && LvN_SetI(wp, "Radius", 3500);
	if (ok) {
		Mode_Ints3(ang, 0, 180, 0);
		ok = Mode_VSet(wp, "Ang", ang) && LvN_SetS(wp, "OnTake", onTake) && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", wp, 1L);
	}
	return Mode_Done(&t, ok);
}

/* EnableMissionWaypoint() of a mission with a meeting point: it works again (missionStartWaypoint.OnTake = onTake) */
static byte Os_EnableWaypoint(const char *module, const char *onTake) {
	nativeVal_t wp = { 0 };
	byte ok = Mode_G(module, "missionStartWaypoint", &wp) && (!Native_True(&wp) || LvN_SetS(&wp, "OnTake", onTake));
	Native_Free(&wp);
	return ok;
}

/* `usr.Name != veh` of the player usr at a meeting point or a treasure: 1, 0, -1 */
static int Os_NotUsr(const nativeVal_t *usr, const nativeVal_t *veh) {
	nativeVal_t name = { 0 };
	int r = Mode_VGet(usr, "Name", &name) ? Native_Compare(&name, veh) != 0 : -1;
	Native_Free(&name);
	return r;
}

/* `SLogic.IsEnemyActive() or the alarm`: 1, 0, -1 */
static int Os_Busy(void) {
	nativeVal_t busy = { 0 };
	int b = LV_CALL("SLogic", "IsEnemyActive", &busy, "") ? Native_True(&busy) : -1;
	Native_Free(&busy);
	return b == 0 ? Mode_AlarmOn() : b;
}

static byte Debtors_CreateWaypoint(void) {
	return Os_StartWaypoint(BD, "DM_Mission_DestroyBadDebtors", "No existe el dummy para esta misi\xf3n: 'DM_Mission_DestroyBadDebtors'",
		"Mission_DestroyBadDebtors.OnInitMission");
}

/* CreateShip(TypeShip, Name): a ship at the nearest free spawn point with the profile and life of its type */
static byte Debtors_CreateShip_C(const nativeVal_t *a, nativeVal_t *out) {
	static const struct {
		const char *type, *profile, *life;
	} types[] = {
		{ "SMERC2", "GetMercenariesProfile", "MERCS_LIFE" },
		{ "SBANKMASTER1", "GetBankDirectorProfile", "BANK_DIRECTOR_LIFE" },
		{ "SBANKER1", "GetBankerProfile", "BANKER_LIFE" },
	};
	modeVals_t t = { .n = 0 };
	nativeVal_t *spw = TV(), *life = TV(), *pd = TV();
	byte ok = Mode_Import("OutMap") && Mode_Import("VehiclesNPC") && LV_CALL("OutMap", "GetNearestFreeSpawnPos", spw, "") &&
		LvN_CreateAt(&a[1], spw, "Car", out) && LvN_SetI(out, "Money", -1);
	byte light = LvN_EqS(&a[0], "TLight2"), found = light;
	Native_SetInt(life, 100);
	if (ok && light) {
		ok = LV_CALL("VehiclesNPC", "GetIAProf", pd, "v", &a[0]);
	}
	for (int i = 0; ok && !found && i < 3; i++) {
		if (LvN_EqS(&a[0], types[i].type)) {
			found = 1;
			ok = LV_OWN(BD, types[i].profile, pd, "") && Mode_Read(BD, types[i].life, life);
		}
	}
	/* pd is set only for the four types (the NameError of the script) */
	ok = ok && (found || Native_Raise(NE_NAME, "pd")) && LvN_StdAI(out, pd) && (light || (Mode_VSet(out, "Life", life) && Mode_VSet(out, "MaxLife", life)));
	return Mode_Done(&t, ok);
}

/* the global var of the module as the value of a loop (a list of names) and its count: -1 after an error */
static int Bd_Elems(const char *var, nativeVal_t *lst) {
	return LvN_GElems(BD, var, lst) ? Native_ForCount(lst) : -1;
}

/* SAI.SetStateVehicle(state, name, the global target of the module) */
static byte Bd_StateG(long state, const nativeVal_t *name, const char *target) {
	nativeVal_t tg = { 0 };
	byte ok = Mode_G(BD, target, &tg) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", state, name, &tg);
	Native_Free(&tg);
	return ok;
}

/* SAI.SetStateVehicle(7, name, target) of every banker */
static byte Bd_BankersFollow(const nativeVal_t *target, const char *targetG) {
	nativeVal_t lst = { 0 };
	int cnt = Bd_Elems("bankers", &lst);
	byte ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t name = { 0 };
		ok = Native_SeqItem(&lst, k, &name) && (target ? LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 7L, &name, target) : Bd_StateG(7, &name, targetG));
		Native_Free(&name);
	}
	Native_Free(&lst);
	return ok;
}

/* the entity of the global name of the module into e */
static byte Bd_EntityG(const char *var, nativeVal_t *e) {
	nativeVal_t name = { 0 };
	byte ok = Mode_G(BD, var, &name) && Mode_EntityH(&name, e);
	Native_Free(&name);
	return ok;
}

/* the position of the current money transport, None when there is none */
static byte Bd_TargetPos(nativeVal_t *pos) {
	nativeVal_t e = { 0 };
	byte ok = Bd_EntityG("currentMissionTarget", &e) && (!Native_True(&e) || Mode_VGet(&e, "Pos", pos));
	Native_Free(&e);
	return ok;
}

/* OnInitMission(ItemName, VehicleName): the player at the meeting point: without alarm and enemies the mission
 * starts: a money transport, the bank master and two bankers after it */
static byte Debtors_OnInitMission(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *wp = TV(), *zero = TV(), *one = TV(), *pos = TV(), *mt = TV(), *none = TV();
	if (!Mode_Import("Traffic") || !Mode_Import("OutPolice") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int skip = Native_True(usr) ? Os_NotUsr(usr, &a[1]) : 1;
	if (skip != 0) {
		return Mode_Done(&t, skip > 0);
	}
	int busy = Os_Busy();
	if (busy != 0) {
		return Mode_Done(&t, busy > 0 && Mode_G(BD, "missionStartWaypoint", wp) && LvN_SetS(wp, "OnTake", "") &&
			LvN_Com("Mission_DistractHumphrey_RemoteMessage11", "DeepThroat", BD, "EnableMissionWaypoint", NULL));
	}
	Native_SetInt(zero, 0);
	Native_SetInt(one, 1);
	byte ok = Mode_G(BD, "missionStartWaypoint", wp) && Mode_VDel(wp) && Mode_Put(BD, "missionStartWaypoint", NULL, none) &&
		Mode_GVarSetS("OnItemTakenAI", "Mission_DestroyBadDebtors.OnShipReachedTreasurePos") && Mode_SetAlarm(zero) && Mode_SetLockAlarm(one) &&
		LV_OWN(BD, "SelectNewMissionTarget", NULL, "") && Bd_TargetPos(pos) && LV_OWN(BD, "CreateBankMaster", NULL, "v", pos) &&
		LV_OWN(BD, "CreateBanker", NULL, "iv", 0L, pos) && LV_OWN(BD, "CreateBanker", NULL, "iv", 0L, pos) && Mode_G(BD, "bankMasterName", mt) &&
		Bd_StateG(2, mt, "currentMissionTarget") && Bd_BankersFollow(NULL, "bankMasterName") &&
		LvN_Com("Mission_MeetBankMaster_RemoteMessage2", "BankMaster", BD, "ShowScorer", NULL);
	return Mode_Done(&t, ok);
}

/* the current mission target red (when it exists) */
static byte Bd_TargetRed(void) {
	nativeVal_t e = { 0 };
	byte ok = Bd_EntityG("currentMissionTarget", &e) && (!Native_True(&e) || LvN_SetS(&e, "TgType", "red"));
	Native_Free(&e);
	return ok;
}

/* MissionsFuncs.SetMissionTarget(currentMissionTarget) */
static byte Bd_Target(void) {
	nativeVal_t n = { 0 };
	byte ok = Mode_G(BD, "currentMissionTarget", &n) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", &n);
	Native_Free(&n);
	return ok;
}

/* ShowScorer(): the transport red and the target, the scorer of the money */
static byte Debtors_ShowScorer(void) {
	return Bd_TargetRed() && Bd_Target() && LvN_ScorerCallG(BD, "MissionScorer") && LvN_Draw();
}

static byte Debtors_EnableMissionWaypoint(void) {
	return Os_EnableWaypoint(BD, "Mission_DestroyBadDebtors.OnInitMission");
}

/* SelectNewMissionTarget(): a new money transport on a traffic path */
static byte Debtors_SelectNewMissionTarget(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *o1 = TV(), *spw = TV(), *p = TV(), *name = TV();
	byte ok = Mode_Import("Traffic") && Mode_Import("OutMap") && LV_CALL("Traffic", "SetPath", NULL, "") &&
		LV_OWN(BD, "CreateShip", o1, "ss", "TLight2", "MoneyTransport") && LvN_SetI(o1, "Life", 100) && LvN_SetI(o1, "MaxLife", 100) &&
		LvN_SetS(o1, "OnDeath", "Mission_DestroyBadDebtors.OnMissionTargetDeath") && LvN_SetS(o1, "OnDamage", "Mission_DestroyBadDebtors.OnMissionTargetDamage") &&
		LV_CALL("OutMap", "GetFreeSpawnPos", spw, "") && LvN_Item(spw, 0, p) && Mode_VSet(o1, "Pos", p) && Mode_VGet(o1, "Name", name) &&
		LV_CALL("SAI", "SetStateVehicle", NULL, "ividd", 3L, name, 0L, 2.0, 0.025) && LV_CALL("Traffic", "ResetPath", NULL, "") &&
		(Native_Free(name), Mode_VGet(o1, "Name", name)) && Mode_Put(BD, "currentMissionTarget", NULL, name);
	return Mode_Done(&t, ok);
}

/* OnMissionTargetDeath(LooserName, WinnerName): the treasure, the next transport, the mercenaries and the bank master
 * a second later */
static byte Debtors_OnMissionTargetDeath(const nativeVal_t *a) {
	nativeVal_t e = { 0 }, pos = { 0 };
	byte ok = Mode_Import("Vehicles") && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && Mode_EntityH(&a[0], &e) &&
		Mode_VGet(&e, "Pos", &pos) && LV_OWN(BD, "DropTreasure", NULL, "v", &pos) && LV_OWN(BD, "SelectNewMissionTarget", NULL, "") && Bd_Target() &&
		Bd_TargetRed() && Mode_Later(1, BD, "CheckMercsFreezed", NULL, 0, NULL) && Mode_Later(1, BD, "BankMasterAttacksMissionTarget", NULL, 0, NULL);
	Native_Free(&e);
	Native_Free(&pos);
	return ok;
}

static byte Debtors_BankMasterAttacksMissionTarget(void) {
	nativeVal_t e = { 0 }, name = { 0 };
	byte ok = Bd_EntityG("bankMasterName", &e) && (!Native_True(&e) || (Mode_G(BD, "bankMasterName", &name) && Bd_StateG(2, &name, "currentMissionTarget")));
	Native_Free(&e);
	Native_Free(&name);
	return ok;
}

/* DropTreasure(pos): the first time the bankers follow the player; the treasure for 20 s, the nearest mercenary and
 * banker go for it */
static byte Debtors_DropTreasure(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	const nativeVal_t *pos = &a[0];
	nativeVal_t *usr = TV(), *un = TV(), *l = TV(), *model = TV(), *radius = TV(), *ln = TV(), *an = TV(), *l2 = TV(), *cam = TV(), *now = TV();
	nativeVal_t *at = TV(), *lst = TV(), *elem = TV();
	int first = LvN_GTrue(BD, "firstTimeDropped");
	byte ok = first >= 0;
	if (first > 0) {
		ok = Mode_UsrHandle(usr) && (!Native_True(usr) || (Mode_VGet(usr, "Name", un) && Bd_BankersFollow(un, NULL) && LvN_PutI(BD, "firstTimeDropped", 0)));
	}
	ok = ok && LvN_CreateAt(Mode_S(&t, "Treasure"), pos, "Item", l) && Mode_G(BD, "TreasureModel", model) && Mode_VModel(l, model) &&
		Mode_G(BD, "TreasureRadius", radius) && Mode_VSet(l, "Radius", radius) && LvN_SetI(l, "AppearsIfFree", 0) && LvN_SetI(l, "AutoFace", 0) &&
		LvN_SetI(l, "OnlyUser", 0) && LvN_SetF(l, "rTime", 0.25) && LvN_SetS(l, "OnTake", "Mission_DestroyBadDebtors.OnCoinTaken") &&
		Mode_Import("Items") && LV_CALL("Items", "SetSound", NULL, "vs", l, "TakeTreasure") && Mode_VGet(l, "Name", ln) &&
		Native_Arith('+', ln, Mode_S(&t, "CheckArrow"), an) && LvN_CreateAt(an, pos, "WithLife", l2);
	if (ok) {
		Mode_Ints3(cam, 0, 1000, 0);
		Native_SetFloat(now, Mode_Time());
		ok = Mode_VSet(l2, "CamTgt", cam) && Native_Arith('+', now, Mode_I(&t, 20), at) && (Native_Free(ln), Mode_VGet(l, "Name", ln)) &&
			Mode_AtG(at, BD, "DelTreasure", ln, 1) && Mode_G(BD, "mercsActive", lst) && LV_OWN(BD, "SendShipToGetTreasure", NULL, "vv", lst, pos) &&
			Mode_Read(BD, "bankers", lst) && LV_OWN(BD, "SendShipToGetTreasure", NULL, "vv", lst, pos);
	}
	if (ok) {
		Native_SetEmpty(elem, NV_TUPLE);
		ok = Mode_VGet(l, "Name", Native_Append(elem));
		Native_Copy(Native_Append(elem), now);
		ok = ok && Mode_Append(BD, "treasuresActive", elem);
	}
	return Mode_Done(&t, ok);
}

/* DelTreasure(ItemName): the treasure goes 5 s later, its arrow now, out of the active treasures */
static byte Debtors_DelTreasure(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = TV(), *an = TV(), *arrow = TV(), *lst = TV();
	byte ok = Mode_EntityH(&a[0], e) && (!Native_True(e) || Mode_DelLater(e)) && Native_Arith('+', &a[0], Mode_S(&t, "CheckArrow"), an) &&
		Mode_EntityH(an, arrow) && (!Native_True(arrow) || Mode_VDel(arrow));
	int cnt = ok ? Bd_Elems("treasuresActive", lst) : -1;
	ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t elem = { 0 }, x = { 0 };
		ok = Native_SeqItem(lst, k, &elem) && LvN_Item(&elem, 0, &x);
		byte is = ok && !Native_Compare(&a[0], &x);
		ok = ok && (!is || Mode_Remove(BD, "treasuresActive", &elem));
		Native_Free(&elem);
		Native_Free(&x);
		if (is) {
			break;
		}
	}
	return Mode_Done(&t, ok);
}

/* SendShipToGetTreasure(shipsList, pos): the nearest ship of the list goes to the treasure */
static byte Debtors_SendShipToGetTreasure(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	const nativeVal_t *pos = &a[1];
	nativeVal_t *lst = TV(), *cur = TV(), *min = TV(), *e = TV(), *x = TV(), *y = TV(), *z = TV(), *r = TV();
	LvN_Elems(&a[0], lst);
	int n = Native_Size(lst);
	if (n <= 0) {
		return Mode_Done(&t, n == 0);
	}
	Native_SetInt(min, 9000000);
	byte ok = LvN_Item(&a[0], 0, cur);
	for (int k = 0; ok && k < n; k++) {
		nativeVal_t ship = { 0 }, ep = { 0 }, sub = { 0 }, d = { 0 };
		Native_Free(e);
		ok = Native_SeqItem(lst, k, &ship) && Mode_EntityH(&ship, e) &&
			(!Native_True(e) || (Mode_VGet(e, "Pos", &ep) && Mode_VecSub(&ep, pos, &sub) && Mode_VecMod(&sub, &d)));
		if (ok && Native_True(e) && Native_Compare(&d, min) < 0) {
			Native_Free(min);
			Native_Copy(min, &d);
			Native_Free(cur);
			Native_Copy(cur, &ship);
		}
		Native_Free(&ship);
		Native_Free(&ep);
		Native_Free(&sub);
		Native_Free(&d);
	}
	Native_Free(e);
	ok = ok && Mode_EntityH(cur, e) && (!Native_True(e) || (LvN_Item(pos, 0, x) && LvN_Item(pos, 1, y) && LvN_Item(pos, 2, z) &&
		Mode_G(BD, "TreasureRadius", r) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivvvvvi", 1L, cur, x, y, z, r, 1L)));
	return Mode_Done(&t, ok);
}

/* GetPossibleMercTargets(): the bankers that exist and the money transport */
static byte Debtors_GetPossibleMercTargets_C(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t lst = { 0 }, e = { 0 };
	(void)a;
	Native_SetEmpty(out, NV_LIST);
	int cnt = Bd_Elems("bankers", &lst);
	byte ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t name = { 0 };
		Native_Free(&e);
		ok = Native_SeqItem(&lst, k, &name) && Mode_EntityH(&name, &e) && (!Native_True(&e) || Mode_VGet(&e, "Name", Native_Append(out)));
		Native_Free(&name);
	}
	Native_Free(&e);
	ok = ok && Bd_EntityG("currentMissionTarget", &e) && (!Native_True(&e) || Mode_VGet(&e, "Name", Native_Append(out)));
	Native_Free(&lst);
	Native_Free(&e);
	return ok;
}

/* float(len(targets)) - 0.01: the range of Scrap.Rand for an index of targets */
static byte Bd_MaxRange(const nativeVal_t *targets, nativeVal_t *out) {
	int n = Native_Size(targets);
	if (n < 0) {
		return 0;
	}
	Native_SetFloat(out, (double)n - 0.01);
	return 1;
}

/* SAI.SetStateVehicle(2, name, targets[int(Scrap.Rand(0.0, maxRange))]) */
static byte Bd_Attack(const nativeVal_t *name, const nativeVal_t *targets, const nativeVal_t *maxRange) {
	nativeVal_t zero = { 0 }, r = { 0 }, i = { 0 }, tg = { 0 };
	Native_SetFloat(&zero, 0.0);
	byte ok = Mode_Rand(&zero, maxRange, &r) && Native_ToInt(&r, &i) && Native_Subscript(targets, &i, &tg) &&
		LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, name, &tg);
	Native_Free(&r);
	Native_Free(&i);
	Native_Free(&tg);
	return ok;
}

/* `targets == []` of a value of the script */
static byte Bd_NoTargets(const nativeVal_t *targets) {
	nativeVal_t empty = { 0 };
	Native_SetEmpty(&empty, NV_LIST);
	byte none = !Native_Compare(targets, &empty);
	Native_Free(&empty);
	return none;
}

/* a mercenary attacks one of the possible targets at random, or the player usr when there is none */
static byte Bd_MercAttack(const nativeVal_t *name, const nativeVal_t *usr) {
	nativeVal_t targets = { 0 }, un = { 0 }, max = { 0 };
	byte ok = LV_OWN(BD, "GetPossibleMercTargets", &targets, "");
	if (ok && Bd_NoTargets(&targets)) {
		ok = Mode_VGet(usr, "Name", &un) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, name, &un);
	} else if (ok) {
		ok = Bd_MaxRange(&targets, &max) && Bd_Attack(name, &targets, &max);
	}
	Native_Free(&targets);
	Native_Free(&un);
	Native_Free(&max);
	return ok;
}

/* CreateMercs(numMercs): the mercenaries near the transport, each after a random target (later when there is none) */
static byte Debtors_CreateMercs(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *targets = TV(), *max = TV(), *lst = TV();
	byte ok = Mode_Import("OutMap") && LV_OWN(BD, "GetPossibleMercTargets", targets, "");
	if (ok && Bd_NoTargets(targets)) {
		return Mode_Done(&t, Mode_Later(2, BD, "CreateMercs", &a[0], 1, NULL));
	}
	long count = 0;
	ok = ok && Mode_Range(&a[0], &count);
	for (long i = 0; ok && i < count; i++) {
		int mark = t.n;
		nativeVal_t *iv = TV(), *rep = TV(), *mn = TV(), *o1 = TV(), *p = TV(), *spw = TV(), *name = TV();
		Native_SetInt(iv, i);
		Mode_Repr(iv, rep);
		ok = Native_Arith('+', Mode_S(&t, "Merc"), rep, mn) && LV_OWN(BD, "CreateShip", o1, "sv", "SMERC2", mn) && LvN_SetS(o1, "TgType", "red") &&
			LvN_SetS(o1, "OnDeath", "Mission_DestroyBadDebtors.OnMercDeath") && LvN_SetS(o1, "OnDamage", "Mission_DestroyBadDebtors.OnMercDamage") &&
			Bd_TargetPos(p) && LV_CALL("OutMap", "GetNearestFreeSpawnPos", spw, "v", p) && Mode_VSet(o1, "Pos", spw) && Mode_VGet(o1, "Name", name) &&
			Mode_DictPut(BD, "shipsMoney", NULL, name, Mode_I(&t, 0)) && Mode_Append(BD, "mercsActive", name);
		Mode_Release(&t, mark);
	}
	int cnt = ok && Bd_MaxRange(targets, max) ? Bd_Elems("mercsActive", lst) : -1;
	ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t merc = { 0 };
		ok = Native_SeqItem(lst, k, &merc) && Bd_Attack(&merc, targets, max);
		Native_Free(&merc);
	}
	return Mode_Done(&t, ok);
}

/* o1.Pos = OutMap.GetNearestFreeSpawnPos(pos) when pos is set */
static byte Bd_NearPos(const nativeVal_t *o1, const nativeVal_t *pos) {
	nativeVal_t spw = { 0 };
	byte ok = !Native_True(pos) || (LV_CALL("OutMap", "GetNearestFreeSpawnPos", &spw, "v", pos) && Mode_VSet(o1, "Pos", &spw));
	Native_Free(&spw);
	return ok;
}

/* shipsMoney[name] = 0 */
static byte Bd_NoMoney(const nativeVal_t *name) {
	nativeVal_t zero = { 0 };
	Native_SetInt(&zero, 0);
	return Mode_DictPut(BD, "shipsMoney", NULL, name, &zero);
}

/* CreateBankMaster(pos=None): the bank master, green, waiting (near pos) */
static byte Bd_CreateBankMaster(const nativeVal_t *pos) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *o1 = TV(), *name = TV();
	if (!Mode_Import("OutMap") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	byte ok = LV_OWN(BD, "CreateShip", o1, "ss", "SBANKMASTER1", "BankMaster") && LvN_SetS(o1, "TgType", "green") &&
		LvN_SetS(o1, "OnDeath", "Mission_DestroyBadDebtors.OnBankMasterDeath") && LvN_SetS(o1, "OnDamage", "Mission_DestroyBadDebtors.OnBankMasterDamage") &&
		Bd_NearPos(o1, pos) && Mode_VGet(o1, "Name", name) && Mode_Put(BD, "bankMasterName", NULL, name) && Mode_Read(BD, "bankMasterName", name) &&
		LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, name, 0L) && Mode_Read(BD, "bankMasterName", name) && Bd_NoMoney(name);
	return Mode_Done(&t, ok);
}

/* the newest active treasure (the one with the smallest time - now; the first of equal ones) into e (None when its
 * entity is gone) */
static byte Bd_NewestTreasure(nativeVal_t *e) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lst = TV(), *min = TV(), *now = TV(), *tr = TV(), *name = TV();
	int cnt = Bd_Elems("treasuresActive", lst);
	long index = 0;
	byte ok = cnt >= 0;
	Native_SetInt(min, 100);
	Native_SetFloat(now, Mode_Time());
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t elem = { 0 }, t1 = { 0 }, d = { 0 };
		ok = Native_SeqItem(lst, k, &elem) && LvN_Item(&elem, 1, &t1) && Native_Arith('-', &t1, now, &d);
		if (ok && Native_Compare(&d, min) < 0) {
			Native_Free(min);
			Native_Copy(min, &d);
			index = k;
		}
		Native_Free(&elem);
		Native_Free(&t1);
		Native_Free(&d);
	}
	ok = ok && Mode_Read(BD, "treasuresActive", lst) && LvN_Item(lst, index, tr) && LvN_Item(tr, 0, name) && Mode_EntityH(name, e);
	return Mode_Done(&t, ok);
}

/* CreateBanker(followPlayer=0, pos=None): a banker, green (near pos); he goes for the newest treasure, else he
 * follows the player or the bank master */
static byte Bd_CreateBanker(const nativeVal_t *follow, const nativeVal_t *pos) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *o1 = TV(), *lst = TV(), *e = TV(), *ep = TV(), *spw = TV(), *name = TV(), *un = TV(), *one = TV();
	if (!Mode_Import("OutMap") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	byte ok = LV_OWN(BD, "CreateShip", o1, "ss", "SBANKER1", "BankEmployee") && LvN_SetS(o1, "TgType", "green") &&
		LvN_SetS(o1, "OnDeath", "Mission_DestroyBadDebtors.OnBankerDeath") && LvN_SetS(o1, "OnDamage", "Mission_DestroyBadDebtors.OnBankerDamage") &&
		Bd_NearPos(o1, pos) && Mode_G(BD, "treasuresActive", lst);
	int n = ok ? Native_Size(lst) : -1;
	ok = n >= 0;
	byte treasure = 0;
	if (ok && n > 0) {
		ok = Bd_NewestTreasure(e);
		treasure = ok && Native_True(e);
		ok = ok && (!treasure || (Mode_VGet(e, "Pos", ep) && LV_CALL("OutMap", "GetNearestFreeSpawnPos", spw, "v", ep) && Mode_VSet(o1, "Pos", spw)));
	}
	if (ok && !treasure) {
		ok = Mode_VGet(o1, "Name", name) && (Native_True(follow) ? Mode_VGet(usr, "Name", un) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 7L, name, un) :
			Bd_StateG(7, name, "bankMasterName"));
	} else if (ok) {
		Native_SetEmpty(one, NV_LIST);
		Native_Free(ep);
		ok = Mode_VGet(o1, "Name", Native_Append(one)) && Mode_VGet(e, "Pos", ep) && LV_OWN(BD, "SendShipToGetTreasure", NULL, "vv", one, ep);
	}
	ok = ok && (Native_Free(name), Mode_VGet(o1, "Name", name)) && Mode_Append(BD, "bankers", name) && Bd_NoMoney(name);
	return Mode_Done(&t, ok);
}

/* RespawnBankMaster(): the bank master back after the transport (later when there is none) */
static byte Debtors_RespawnBankMaster(void) {
	nativeVal_t e = { 0 }, name = { 0 };
	byte ok = Bd_EntityG("currentMissionTarget", &e);
	if (ok && !Native_True(&e)) {
		ok = Mode_Later(3, BD, "RespawnBankMaster", NULL, 0, NULL);
	} else if (ok) {
		ok = LV_OWN(BD, "CreateBankMaster", NULL, "") && Mode_G(BD, "bankMasterName", &name) && Bd_StateG(2, &name, "currentMissionTarget");
	}
	Native_Free(&e);
	Native_Free(&name);
	return ok;
}

static byte Debtors_OnBankMasterDeath(const nativeVal_t *a) {
	return Mode_Import("Vehicles") && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) &&
		Mode_Later(5, BD, "RespawnBankMaster", NULL, 0, "RespawnBankMaster");
}

/* CheckMercsFreezed(): every mercenary that exists attacks a new target */
static byte Debtors_CheckMercsFreezed(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *lst = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	int cnt = Bd_Elems("mercsActive", lst);
	byte ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t merc = { 0 }, e = { 0 };
		ok = Native_SeqItem(lst, k, &merc) && Mode_EntityH(&merc, &e) && (!Native_True(&e) || Bd_MercAttack(&merc, usr));
		Native_Free(&merc);
		Native_Free(&e);
	}
	return Mode_Done(&t, ok);
}

/* a banker or mercenary dies: his treasure falls when he carries money, else the coins of money */
static byte Bd_Drop(const nativeVal_t *looser, long money) {
	nativeVal_t e = { 0 }, sm = { 0 }, m = { 0 }, pos = { 0 };
	int rich = Mode_EntityH(looser, &e) ? Native_True(&e) : -1;
	if (rich > 0) {
		rich = Mode_G(BD, "shipsMoney", &sm) && Native_Subscript(&sm, looser, &m) ? Mode_CmpI(&m, ">", 0) : -1;
	}
	byte ok = rich >= 0 && (rich ? Mode_VGet(&e, "Pos", &pos) && LV_OWN(BD, "DropTreasure", NULL, "v", &pos) : LvN_SetI(&e, "Money", money));
	Native_Free(&e);
	Native_Free(&sm);
	Native_Free(&m);
	Native_Free(&pos);
	return ok;
}

/* OnBankerDeath(LooserName, WinnerName): his treasure, the mercenaries retarget, another banker 10 s later */
static byte Debtors_OnBankerDeath(const nativeVal_t *a) {
	nativeVal_t args[2] = { { 0 }, { 0 } }, wp = { 0 };
	byte ok = Mode_Import("Vehicles") && Bd_Drop(&a[0], 100) && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) &&
		Mode_Later(1, BD, "CheckMercsFreezed", NULL, 0, NULL) && Mode_G(BD, "missionStartWaypoint", &wp);
	if (ok) {
		Native_SetInt(&args[0], wp.type == NV_NONE);
		ok = Mode_Later(10, BD, "CreateBanker", args, 2, "RespawnBanker") && Mode_RemoveIn(&a[0], BD, "bankers");
	}
	Native_Free(&wp);
	return ok;
}

/* OnMercDeath(LooserName, WinnerName): his treasure, another mercenary 20 s later */
static byte Debtors_OnMercDeath(const nativeVal_t *a) {
	nativeVal_t one = { 0 };
	Native_SetInt(&one, 1);
	return Mode_Import("Vehicles") && Bd_Drop(&a[0], 5) && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) &&
		Mode_RemoveIn(&a[0], BD, "mercsActive") && Mode_Later(20, BD, "CreateMercs", &one, 1, "RespawnMerc");
}

/* OnTreasureReached(VehicleName): a ship at a treasure it did not get: a mercenary attacks again, a banker follows the
 * player */
static byte Debtors_OnTreasureReached(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *e = TV(), *state = TV(), *un = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_EntityH(&a[0], e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	int merc = Mode_InG(&a[0], BD, "mercsActive");
	byte ok = merc >= 0 && Mode_AIState(&a[0], state);
	if (ok && merc && !LvN_EqI(state, 2)) {
		ok = Bd_MercAttack(&a[0], usr);
	} else if (ok && !merc && !LvN_EqI(state, 7)) {
		ok = Mode_VGet(usr, "Name", un) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 7L, &a[0], un);
	}
	return Mode_Done(&t, ok);
}

static byte Debtors_OnShipReachedTreasurePos(const nativeVal_t *a) {
	return Mode_Later(1, BD, "OnTreasureReached", &a[0], 1, NULL);
}

/* `m = int(Scrap.Rand(0.0, 1.99))`: Speech.RemoteComunication of the first key and speaker when m is 0, else of the
 * second ones */
static byte Bd_OneOf(const char *first, const char *who1, const char *second, const char *who2) {
	nativeVal_t r = { 0 }, m = { 0 };
	byte ok = Mode_RandD(0.0, 1.99, &r) && Native_ToInt(&r, &m);
	byte one = ok && LvN_EqI(&m, 0);
	Native_Free(&r);
	Native_Free(&m);
	return ok && LV_CALL("Speech", "RemoteComunication", NULL, "ss", one ? first : second, one ? who1 : who2);
}

/* the money of the player + the global reward of the module (Scrap.SetMoney(Scrap.GetMoney() + reward)) */
static byte Bd_Reward(const char *reward) {
	nativeVal_t money = { 0 }, r = { 0 }, sum = { 0 };
	Native_SetInt(&money, Mode_GetMoney());
	byte ok = Mode_G(BD, reward, &r) && Native_Arith('+', &money, &r, &sum) && Mode_SetMoney(&sum);
	Native_Free(&r);
	Native_Free(&sum);
	return ok;
}

/* the bank recovered its money: no scorer, the reward, every ship of the mission back into the traffic */
static byte Bd_Recovered(void) {
	static const char *const funcs[] = { "RespawnBankMaster", "RespawnBanker", "RespawnMerc", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ships = TV(), *lst = TV(), *mt = TV();
	byte ok = LvN_ScorerOff() && LvN_Com("Mission_DestroyBadDebtors_RemoteMessage5", "BankMaster", BD, "ShowDeepThroatMessage", LvN_NoArgs()) &&
		Bd_Reward("MISSION_BANKMASTER_REWARD") && Mode_Import("SaveGame") && LV_CALL("SaveGame", "SaveMoney", NULL, "");
	Native_SetEmpty(ships, NV_LIST);
	for (int l = 0; ok && l < 2; l++) {
		int cnt = Bd_Elems(l ? "mercsActive" : "bankers", lst);
		ok = cnt >= 0;
		for (int k = 0; ok && k < cnt; k++) {
			ok = Native_SeqItem(lst, k, Native_Append(ships));
		}
		Native_Free(lst);
	}
	ok = ok && Mode_G(BD, "bankMasterName", Native_Append(ships)) && Mode_G(BD, "currentMissionTarget", Native_Append(ships));
	for (int k = 0; ok && k < ships->n; k++) {
		const nativeVal_t *name = &ships->items[k];
		nativeVal_t e = { 0 };
		ok = Mode_EntityH(name, &e);
		if (ok && Native_True(&e)) {
			ok = LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, name, 1L) && LvN_SetS(&e, "TgType", "none") &&
				LvN_SetS(&e, "OnDeath", "Vehicles.OnDeath") && LvN_SetS(&e, "OnDamage", "") && Mode_Read(BD, "currentMissionTarget", mt) &&
				(Native_Compare(name, mt) || LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, 0L));
		}
		Native_Free(&e);
	}
	for (int i = 0; ok && funcs[i]; i++) {
		ok = Mode_DelSchedS(funcs[i]);
	}
	return Mode_Done(&t, ok);
}

/* shipsMoney[veh] = shipsMoney[veh] + TREASURE_REWARD; moneyRecovered = moneyRecovered op TREASURE_REWARD */
static byte Bd_Carry(const nativeVal_t *veh, char op) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sm = TV(), *m = TV(), *rw = TV(), *sum = TV(), *mr = TV(), *r = TV();
	byte ok = Mode_G(BD, "shipsMoney", sm) && Native_Subscript(sm, veh, m) && Mode_G(BD, "TREASURE_REWARD", rw) && Native_Arith('+', m, rw, sum) &&
		Mode_DictPut(BD, "shipsMoney", NULL, veh, sum) && Mode_G(BD, "moneyRecovered", mr) && Mode_Read(BD, "TREASURE_REWARD", rw) &&
		Native_Arith(op, mr, rw, r) && Mode_Put(BD, "moneyRecovered", NULL, r);
	return Mode_Done(&t, ok);
}

/* OnCoinTaken(ItemName, VehicleName): a treasure taken by the player (his reward), a mercenary (money lost) or a
 * banker (money recovered; all of it ends the mission) */
static byte Debtors_OnCoinTaken(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	const nativeVal_t *veh = &a[1];
	nativeVal_t *usr = TV(), *un = TV(), *mr = TV(), *call = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int skip = Native_True(usr) ? Mode_CmpG(veh, "==", BD, "bankMasterName") : 1;
	if (skip != 0) {
		return Mode_Done(&t, skip > 0);
	}
	if (!LV_OWN(BD, "DelTreasure", NULL, "v", &a[0]) || !Mode_VGet(usr, "Name", un)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_Compare(veh, un)) {
		return Mode_Done(&t, Bd_Reward("TREASURE_REWARD") &&
			LV_CALL("Speech", "RemoteComunication", NULL, "ss", "Mission_DestroyBadDebtors_RemoteMessage4", "DeepThroat"));
	}
	int in = Mode_InG(veh, BD, "mercsActive");
	if (in != 0) {
		int neg = in > 0 && Bd_Carry(veh, '-') && Mode_G(BD, "moneyRecovered", mr) ? Mode_CmpI(mr, "<", 0) : -1;
		return Mode_Done(&t, neg >= 0 && (!neg || LvN_PutI(BD, "moneyRecovered", 0)) && Bd_MercAttack(veh, usr) &&
			Bd_OneOf("Mission_DestroyBadDebtors_RemoteMessage9", "MercenaryA", "Mission_DestroyBadDebtors_RemoteMessage10", "MercenaryB") && LvN_Draw());
	}
	in = Mode_InG(veh, BD, "bankers");
	if (in <= 0) {
		return Mode_Done(&t, in == 0);
	}
	int all = Bd_Carry(veh, '+') && Mode_G(BD, "moneyRecovered", mr) ? Mode_CmpG(mr, ">=", BD, "MAX_MONEY") : -1;
	byte ok = all >= 0;
	if (all > 0) {
		ok = LvN_ModGet("Menu", "MissionScorerCall", call) && (call->type == NV_NONE || Bd_Recovered());
	} else if (ok) {
		ok = Bd_OneOf("Mission_DestroyBadDebtors_RemoteMessage7", "BankDirector", "Mission_DestroyBadDebtors_RemoteMessage8", "BankDirector");
		int appear = ok ? LvN_GEqI(BD, "mercsHaveAppeared", 0) : -1;
		if (appear > 0) {
			appear = Mode_Read(BD, "moneyRecovered", mr) ? Mode_CmpG(mr, ">=", BD, "MONEY_MERCS_APPEAR") : -1;
		}
		ok = appear >= 0 && (!appear || (LvN_PutI(BD, "mercsHaveAppeared", 1) && Mode_Later(5, BD, "MercsAppear", NULL, 0, NULL))) &&
			LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 7L, veh, un) && LvN_ScorerCallG(BD, "MissionScorer");
	}
	return Mode_Done(&t, ok && LvN_Draw());
}

static byte Debtors_BankMasterSpeech(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *args = TV(), *who = TV(), *d = TV();
	LvN_Str(args, "Mission_DestroyBadDebtors_RemoteMessage3");
	LvN_Str(who, "BankMaster");
	Native_SetInt(d, 5);
	return Mode_Done(&t, Mode_LaterSpeech("AddRemoteCom", d, args, 2, NULL));
}

/* ShowDeepThroatMessage(): the bank master's ship and engine to edit, the message, DeepThroat 5 s later */
static byte Debtors_ShowDeepThroatMessage(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cad = TV(), *args = TV(), *who = TV(), *fn = TV(), *none = TV(), *d = TV();
	Mode_Lang("Mission_DestroyBadDebtors_SystemMessage1", cad);
	LvN_Str(args, "Mission_DestroyBadDebtors_RemoteMessage6");
	LvN_Str(who, "DeepThroat");
	Native_SetEmpty(none, NV_TUPLE);
	Native_SetInt(d, 5);
	byte ok = Mode_SaveAdd(BD, "Hangar.shipsToEditList", "SBankMaster1") && Mode_SaveAdd(BD, "Hangar.availableEnginesList", "MBANKMASTER1") &&
		Mode_SpeechMsg(cad) && LV_CALL("Scrap", "IncSaveVar", NULL, "si", "NewBluePrintAvaliable", 2L) && LvN_GObj(BD, "SetNextMission", fn) &&
		Mode_LaterSpeech("AddRemoteCom", d, args, 4, NULL);
	return Mode_Done(&t, ok);
}

/* SetNextMission(): the alarm free, the traffic refilled bit by bit, on to Mission_InfiltrateBank */
static byte Debtors_SetNextMission(void) {
	static const char *const places[] = { "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Bank", "Journalist.Betty_Defaut", "Press",
		"Journalist.Berto_Defaut", "NoPlace", "BadReputation", "4", "StdShipAIProfile", "Ace", NULL };
	nativeVal_t zero = { 0 }, args[2] = { { 0 }, { 0 } };
	Native_SetInt(&zero, 0);
	LvN_Str(&args[0], "");
	LvN_Str(&args[1], "");
	byte ok = Mode_Import("Traffic") && Mode_SetAlarm(&zero) && Mode_SetLockAlarm(&zero) && Mode_GVarSetI("MaxFightingTrafficSize", 4) &&
		Mode_GVarSetI("iTrafficNumber", 7);
	for (int i = 0; ok && i < 37; i++) {
		ok = Mode_LaterIn(0.25 * i, "Traffic", "NormalTrafficDeath", args, 2, NULL);
	}
	for (int i = 0; ok && i < 20; i++) {
		ok = Mode_LaterIn(0.375 * i, "Traffic", "MediumTrafficDeath", args, 2, NULL);
	}
	ok = ok && LvN_Next("Mission_InfiltrateBank", "Bank", "Mission_InfiltrateBank_MissionText1");
	if (ok) {
		Mode_Saves(places);
	}
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	return ok && Mode_WayPoint();
}

/* MissionScorer(id): the money recovered as a bar */
static byte Debtors_MissionScorer(const nativeVal_t *id) {
	static const char bar[] = "MoneyRecovered";
	nativeVal_t v = { 0 };
	byte ok = Mode_LvLabel(id, "MoneyRecoveredText", 0, "Mission_DestroyBadDebtors_Scorer1", 0) && Mode_ScorerAddS(id, bar, "Bar") &&
		Mode_BarDefaults(id, bar, NULL, NULL) && Mode_ScorerSetI(id, bar, "IsAutoUpdatable", 0) && Mode_G(BD, "MAX_MONEY", &v) &&
		LvN_ScorerSet(id, bar, "MaxValue", &v) && Mode_Read(BD, "moneyRecovered", &v) && LvN_ScorerSet(id, bar, "Value", &v);
	Native_Free(&v);
	return ok;
}

/* the keyword adapters (CreateBankMaster(pos=None), CreateBanker(followPlayer=0, pos=None)) */
static PyObject *Debtors_CreateBankMasterPy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "pos", NULL };
	PyObject *pos = Py_None;
	nativeVal_t p = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &pos)) {
		return NULL;
	}
	Native_Arg(pos, &p);
	byte ok = Bd_CreateBankMaster(&p);
	Native_Free(&p);
	return Mode_Result(ok);
}

static PyObject *Debtors_CreateBankerPy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "followPlayer", "pos", NULL };
	PyObject *follow = NULL, *pos = Py_None;
	nativeVal_t fp = { 0 }, p = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|OO", names, &follow, &pos)) {
		return NULL;
	}
	if (follow) {
		Native_Arg(follow, &fp);
	} else {
		Native_SetInt(&fp, 0);
	}
	Native_Arg(pos, &p);
	byte ok = Bd_CreateBanker(&fp, &p);
	Native_Free(&fp);
	Native_Free(&p);
	return Mode_Result(ok);
}

LV_CORE0(Debtors_InitStatus)
LV_CORE0(Debtors_CreateWaypoint)
LV_COREA(Debtors_OnInitMission)
LV_CORE0(Debtors_ShowScorer)
LV_CORE0(Debtors_EnableMissionWaypoint)
LV_CORE0(Debtors_SelectNewMissionTarget)
LV_COREA(Debtors_OnMissionTargetDeath)
LV_CORE0(Debtors_BankMasterAttacksMissionTarget)
LV_COREA(Debtors_DropTreasure)
LV_COREA(Debtors_DelTreasure)
LV_COREA(Debtors_SendShipToGetTreasure)
LV_COREA(Debtors_CreateMercs)
LV_CORE0(Debtors_RespawnBankMaster)
LV_COREA(Debtors_OnBankMasterDeath)
LV_CORE0(Debtors_CheckMercsFreezed)
LV_COREA(Debtors_OnBankerDeath)
LV_COREA(Debtors_OnMercDeath)
LV_COREA(Debtors_OnTreasureReached)
LV_COREA(Debtors_OnShipReachedTreasurePos)
LV_COREA(Debtors_OnCoinTaken)
LV_CORE0(Debtors_BankMasterSpeech)
LV_CORE0(Debtors_ShowDeepThroatMessage)
LV_CORE0(Debtors_SetNextMission)
LV_COREA(Debtors_MissionScorer)
LV_FN(Debtors, InitStatusPy, 1, Debtors_InitStatus_C)
LV_FN(Debtors, CreateWaypointPy, 0, Debtors_CreateWaypoint_C)
LV_FN(Debtors, CreateShipPy, 2, Debtors_CreateShip_C)
LV_FN(Debtors, OnInitMissionPy, 2, Debtors_OnInitMission_C)
LV_FN(Debtors, ShowScorerPy, 0, Debtors_ShowScorer_C)
LV_FN(Debtors, EnableMissionWaypointPy, 0, Debtors_EnableMissionWaypoint_C)
LV_FN(Debtors, SelectNewMissionTargetPy, 0, Debtors_SelectNewMissionTarget_C)
LV_FN(Debtors, OnMissionTargetDeathPy, 2, Debtors_OnMissionTargetDeath_C)
LV_FN(Debtors, BankMasterAttacksMissionTargetPy, 0, Debtors_BankMasterAttacksMissionTarget_C)
LV_FN(Debtors, DropTreasurePy, 1, Debtors_DropTreasure_C)
LV_FN(Debtors, DelTreasurePy, 1, Debtors_DelTreasure_C)
LV_FN(Debtors, SendShipToGetTreasurePy, 2, Debtors_SendShipToGetTreasure_C)
LV_FN(Debtors, GetPossibleMercTargetsPy, 0, Debtors_GetPossibleMercTargets_C)
LV_FN(Debtors, CreateMercsPy, 1, Debtors_CreateMercs_C)
LV_FN(Debtors, RespawnBankMasterPy, 0, Debtors_RespawnBankMaster_C)
LV_FN(Debtors, OnBankMasterDeathPy, 2, Debtors_OnBankMasterDeath_C)
LV_FN(Debtors, CheckMercsFreezedPy, 0, Debtors_CheckMercsFreezed_C)
LV_FN(Debtors, OnBankerDeathPy, 2, Debtors_OnBankerDeath_C)
LV_FN(Debtors, OnMercDeathPy, 2, Debtors_OnMercDeath_C)
LV_FN(Debtors, OnTreasureReachedPy, 1, Debtors_OnTreasureReached_C)
LV_FN(Debtors, OnShipReachedTreasurePosPy, 1, Debtors_OnShipReachedTreasurePos_C)
LV_FN(Debtors, OnCoinTakenPy, 2, Debtors_OnCoinTaken_C)
LV_FN(Debtors, BankMasterSpeechPy, 0, Debtors_BankMasterSpeech_C)
LV_FN(Debtors, ShowDeepThroatMessagePy, 0, Debtors_ShowDeepThroatMessage_C)
LV_FN(Debtors, SetNextMissionPy, 0, Debtors_SetNextMission_C)
LV_FN(Debtors, MissionScorerPy, 1, Debtors_MissionScorer_C)

PyMethodDef g_OutskirtsDestroyBadDebtorsMethods[] = {
	{ "GetMercenariesProfile", Debtors_GetMercenariesProfile, 1, NULL },
	{ "GetBankDirectorProfile", Debtors_GetBankDirectorProfile, 1, NULL },
	{ "GetBankerProfile", Debtors_GetBankerProfile, 1, NULL },
	{ "InitStatus", Debtors_InitStatusPy, 1, NULL },
	{ "CreateWaypoint", Debtors_CreateWaypointPy, 1, NULL },
	{ "CreateShip", Debtors_CreateShipPy, 1, NULL },
	{ "OnInitMission", Debtors_OnInitMissionPy, 1, NULL },
	{ "ShowScorer", Debtors_ShowScorerPy, 1, NULL },
	{ "EnableMissionWaypoint", Debtors_EnableMissionWaypointPy, 1, NULL },
	{ "SelectNewMissionTarget", Debtors_SelectNewMissionTargetPy, 1, NULL },
	{ "OnMissionTargetDeath", Debtors_OnMissionTargetDeathPy, 1, NULL },
	{ "BankMasterAttacksMissionTarget", Debtors_BankMasterAttacksMissionTargetPy, 1, NULL },
	{ "DropTreasure", Debtors_DropTreasurePy, 1, NULL },
	{ "DelTreasure", Debtors_DelTreasurePy, 1, NULL },
	{ "SendShipToGetTreasure", Debtors_SendShipToGetTreasurePy, 1, NULL },
	{ "GetPossibleMercTargets", Debtors_GetPossibleMercTargetsPy, 1, NULL },
	{ "CreateMercs", Debtors_CreateMercsPy, 1, NULL },
	{ "CreateBankMaster", (PyCFunction)(void (*)(void))Debtors_CreateBankMasterPy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateBanker", (PyCFunction)(void (*)(void))Debtors_CreateBankerPy, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "RespawnBankMaster", Debtors_RespawnBankMasterPy, 1, NULL },
	{ "OnBankMasterDeath", Debtors_OnBankMasterDeathPy, 1, NULL },
	{ "CheckMercsFreezed", Debtors_CheckMercsFreezedPy, 1, NULL },
	{ "OnBankerDeath", Debtors_OnBankerDeathPy, 1, NULL },
	{ "OnMercDeath", Debtors_OnMercDeathPy, 1, NULL },
	{ "OnTreasureReached", Debtors_OnTreasureReachedPy, 1, NULL },
	{ "OnShipReachedTreasurePos", Debtors_OnShipReachedTreasurePosPy, 1, NULL },
	{ "OnCoinTaken", Debtors_OnCoinTakenPy, 1, NULL },
	{ "BankMasterSpeech", Debtors_BankMasterSpeechPy, 1, NULL },
	{ "ShowDeepThroatMessage", Debtors_ShowDeepThroatMessagePy, 1, NULL },
	{ "SetNextMission", Debtors_SetNextMissionPy, 1, NULL },
	{ "MissionScorer", Debtors_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_AttackTraitorius: frighten Traitorius within a minute without killing him, then his call ---- */

static const char *const traitorius_profile[] = { "Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=2", "UsePercSonic=70", "UsePercCloud=20", "UsePercElectro=10",
	"UsePercHook=80", "FactorHookMap=10", "CadenceShoot=9", "ControlAim=4", "AngAttack=1.2", "CounterAttack=0.75", "ControlBoost=1",
	"ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.30", "ControlStrafeEvasion=1", "UsePercHookGetAway=80",
	"FactorDamageItemLife=0.3", "FactorItemAmmo=0.05", "TimeMaxStrafe=2", "VelMaxStrafe=0.25", NULL };

static PyObject *Traitorius_GetTraitoriusProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, AT, traitorius_profile);
}

static PyObject *Traitorius_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, AT, ships_all);
}

/* Init(LevelPath): Traitorius to frighten, or his call once frightened */
static byte Traitorius_Init(void) {
	nativeVal_t n = { 0 };
	byte ok = Mode_Import("Menu") && Mode_Import("Parking") && LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", AT, "ReassignTarget") &&
		Mode_SaveInt("TraitoriusFrightened", &n);
	if (ok && LvN_EqI(&n, 0)) {
		ok = Mode_GVarSetS("OnLifeItemTaken", "Mission_AttackTraitorius.TraitoriusTakesLife") &&
			LV_CALL("Scrap", "CreateEntityList", NULL, "s", "Traitorius") && LV_OWN(AT, "CreateTraitorius", NULL, "");
	} else if (ok) {
		ok = LV_OWN(AT, "CallTraitorius", NULL, "");
	}
	Native_Free(&n);
	return ok;
}

/* SAI.SetStateVehicle(state, traitorius.Name, ...) with the rest of the arguments: target and dist (target NULL: dist
 * only) */
static byte At_State(long state, const nativeVal_t *target, long dist) {
	nativeVal_t tr = { 0 }, name = { 0 };
	byte ok = Mode_G(AT, "traitorius", &tr) && Mode_VGet(&tr, "Name", &name) &&
		(target ? LV_CALL("SAI", "SetStateVehicle", NULL, "ivvi", state, &name, target, dist) :
			LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", state, &name, dist));
	Native_Free(&tr);
	Native_Free(&name);
	return ok;
}

/* ReassignTarget(): after boarding: Traitorius flees from the new ship */
static byte Traitorius_ReassignTarget(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *tr = TV(), *name = TV(), *state = TV(), *un = TV();
	int both = Mode_UsrHandle(usr) ? Native_True(usr) : -1;
	if (both > 0) {
		both = Mode_G(AT, "traitorius", tr) ? Native_True(tr) : -1;
	}
	if (both <= 0) {
		return Mode_Done(&t, both == 0);
	}
	byte ok = Mode_VGet(tr, "Name", name) && Mode_AIState(name, state);
	if (ok && (LvN_EqI(state, 5) || LvN_EqI(state, 0))) {
		ok = Mode_VGet(usr, "Name", un) && At_State(5, un, 75000);
	}
	return Mode_Done(&t, ok);
}

/* CreateTraitorius(): the countdown, Traitorius at a free spawn point (calm, without missiles), the scorer */
static byte Traitorius_CreateTraitorius(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = TV(), *spw = TV(), *p = TV(), *tr = TV(), *ang = TV(), *pd = TV(), *name = TV(), *ship = TV(), *st = TV(), *off = TV();
	nativeVal_t *st2 = TV();
	byte ok = Mode_Import("OutMap") && Mode_Import("Menu") && Mode_GVarSetI("CounterType", 2) && Mode_G(AT, "frightenedTime", v) &&
		Mode_GVarSet("TimeSpecialUseTimer", v) && LV_CALL("OutMap", "GetFreeSpawnPos", spw, "") && LvN_Item(spw, 0, p) &&
		LvN_CreateAt(Mode_S(&t, "Traitorius"), p, "Car", tr) && Mode_Put(AT, "traitorius", NULL, tr) && Mode_Read(AT, "traitorius", tr) &&
		LvN_SetS(tr, "EntityList", "Traitorius") && LvN_SetS(tr, "TgType", "red") && LvN_SetI(tr, "LaunchTaunts", 0);
	if (ok) {
		Native_SetEmpty(ang, NV_TUPLE);
		Native_SetInt(Native_Append(ang), 0);
		ok = LvN_Item(spw, 1, Native_Append(ang));
		Native_SetInt(Native_Append(ang), 0);
	}
	ok = ok && Mode_VSet(tr, "Ang", ang) && LV_OWN(AT, "GetTraitoriusProfile", pd, "") && Mode_Put(AT, "pd", NULL, pd) && Mode_VGet(tr, "Name", name) &&
		Mode_Read(AT, "pd", pd) && Mode_VGet(pd, "Ship", ship) && LV_CALL("VehiclesNPC", "SetStdProperties", NULL, "vv", name, ship) &&
		Mode_VGet(tr, "Style", st) && Mode_G(AT, "styleOffset", off) && Native_Arith('-', st, off, st2) && Mode_VSet(tr, "Style", st2) &&
		LvN_SetS(tr, "Descriptor", "Traitorius") && Mode_Read(AT, "pd", pd) && LV_CALL("VehiclesNPC", "ModAIProff", NULL, "vv", tr, pd) &&
		LvN_Life(tr, AT, "TRAITORIUS_LIFE") && LvN_SetS(tr, "WeapList", "1,1,1,0,0,0") && (Native_Free(name), Mode_VGet(tr, "Name", name)) &&
		LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, name, 0L) &&
		Mode_ListSet("Traitorius", "OnDamage", "Mission_AttackTraitorius.OnTraitoriusDamage") &&
		Mode_ListSet("Traitorius", "OnDeath", "Mission_AttackTraitorius.OnTraitoriusDeath") && (Native_Free(name), Mode_VGet(tr, "Name", name)) &&
		LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name) && LvN_ScorerCallG(AT, "MissionScorer") && LvN_Draw();
	return Mode_Done(&t, ok);
}

/* `life <= traitorius.MaxLife * minLifePercentage` (op "<=") or `>` of Traitorius: 1, 0, -1 */
static int At_LifeCmp(const nativeVal_t *tr, const nativeVal_t *life, const char *op) {
	nativeVal_t max = { 0 }, pct = { 0 }, lim = { 0 };
	int r = Mode_VGet(tr, "MaxLife", &max) && Mode_G(AT, "minLifePercentage", &pct) && Mode_Arith('*', &max, &pct, &lim) ? Mode_Cmp(life, op, &lim) : -1;
	Native_Free(&max);
	Native_Free(&pct);
	Native_Free(&lim);
	return r;
}

/* VehiclesNPC.ModAIProff(traitorius, pd) and the counter type of the countdown */
static byte At_Calm(long counter) {
	nativeVal_t tr = { 0 }, pd = { 0 };
	byte ok = Mode_G(AT, "traitorius", &tr) && Mode_G(AT, "pd", &pd) && LV_CALL("VehiclesNPC", "ModAIProff", NULL, "vv", &tr, &pd) &&
		Mode_GVarSetI("CounterType", counter);
	Native_Free(&tr);
	Native_Free(&pd);
	return ok;
}

/* the life bar of the scorer: SScorer.Set(0, "RemainingLifeScorer", "Value", traitorius.Life) */
static byte At_LifeBar(void) {
	nativeVal_t tr = { 0 }, life = { 0 };
	byte ok = Mode_G(AT, "traitorius", &tr) && Mode_VGet(&tr, "Life", &life) && LvN_Set0("RemainingLifeScorer", "Value", &life);
	Native_Free(&tr);
	Native_Free(&life);
	return ok;
}

/* `not Scrap.GetAlarm()[0] and Scrap.Get("TimeSpecialUseTimer") <= frightenedTime * timeCallsPolicePercentage`: 1, 0,
 * -1 */
static int At_CallsPolice(void) {
	nativeVal_t ft = { 0 }, pct = { 0 }, lim = { 0 }, left = { 0 };
	int alarm = Mode_AlarmOn();
	int r = alarm < 0 ? -1 : !alarm;
	if (r > 0) {
		r = Mode_G(AT, "frightenedTime", &ft) && Mode_G(AT, "timeCallsPolicePercentage", &pct) && Mode_Arith('*', &ft, &pct, &lim) &&
			Native_GVarGet("TimeSpecialUseTimer", &left) ? Mode_Cmp(&left, "<=", &lim) : -1;
	}
	Native_Free(&ft);
	Native_Free(&pct);
	Native_Free(&lim);
	Native_Free(&left);
	return r;
}

/* Speech.AddRemoteCom(key, "Traitorius", Scrap.SetAlarm, (1,), InsertFirst=1): his call of the police (OBJLIB 3: the
 * keyword call stays Python) */
static byte At_PoliceCall(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *args = TV(), *who = TV(), *fn = TV(), *p = TV();
	LvN_Str(args, "Mission_AttackTraitorius_RemoteMessage6");
	LvN_Str(who, "Traitorius");
	Native_SetEmpty(p, NV_TUPLE);
	Native_SetInt(Native_Append(p), 1);
	byte ok = LvN_ModGet("Scrap", "SetAlarm", fn) && Mode_CallKw("Speech", "AddRemoteCom", NULL, args, 4, Py_BuildValue("{s:i}", "InsertFirst", 1));
	return Mode_Done(&t, ok);
}

/* OnTraitoriusDamage(Looser, Winner, damage): hit by the player: the life bar; frightened he calls the police (after
 * a while); his taunts the first two times */
static byte Traitorius_OnTraitoriusDamage(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *tr = TV(), *life = TV(), *st = TV(), *off = TV(), *st2 = TV();
	int skip = Mode_UsrHandle(usr) ? !Native_True(usr) : -1;
	if (skip == 0) {
		skip = Os_NotUsr(usr, &a[1]);
	}
	if (skip == 0) {
		skip = Mode_G(AT, "traitorius", tr) ? !Native_True(tr) : -1;
	}
	if (skip != 0) {
		return Mode_Done(&t, skip > 0);
	}
	skip = Mode_VGet(tr, "Life", life) ? Mode_CmpI(life, "<=", 0) : -1;
	if (skip != 0) {
		return Mode_Done(&t, skip > 0);
	}
	int scared = At_LifeBar() && Mode_Read(AT, "traitorius", tr) && (Native_Free(life), Mode_VGet(tr, "Life", life)) ? At_LifeCmp(tr, life, "<=") : -1;
	if (scared > 0) {
		int call = At_Calm(0) ? At_CallsPolice() : -1;
		if (call != 0) {
			return Mode_Done(&t, call > 0 && At_PoliceCall());
		}
	}
	if (scared < 0) {
		return Mode_Done(&t, 0);
	}
	const char *key = scared ? "Mission_AttackTraitorius_RemoteMessage3" : NULL;
	int n = LvN_GEqI(AT, "firstTimeDamage", 0);
	byte ok = n >= 0;
	if (n > 0) {
		ok = LvN_GAdd(AT, "firstTimeDamage", 1, NULL) && Mode_Read(AT, "traitorius", tr) && Mode_VGet(tr, "Style", st) && Mode_G(AT, "styleOffset", off) &&
			Native_Arith('+', st, off, st2) && Mode_VSet(tr, "Style", st2) && At_State(5, &a[1], 75000) &&
			LvN_Com(key ? key : "Mission_AttackTraitorius_RemoteMessage1", "Traitorius", NULL, NULL, NULL);
	} else if (ok) {
		n = LvN_GEqI(AT, "firstTimeDamage", 1);
		ok = n >= 0;
		if (n > 0) {
			ok = LvN_GAdd(AT, "firstTimeDamage", 1, NULL) && LvN_Com(key ? key : "Mission_AttackTraitorius_RemoteMessage2", "Traitorius", NULL, NULL, NULL);
		} else if (ok && key) {
			ok = LV_CALL("Speech", "RemoteComunication", NULL, "ss", key, "Traitorius");
		}
	}
	return Mode_Done(&t, ok);
}

/* OnTraitoriusDeath(LooserName, WinnerName): killed: the alarm, DeepThroat, a new Traitorius */
static byte Traitorius_OnTraitoriusDeath(void) {
	nativeVal_t none = { 0 }, one = { 0 };
	Native_SetInt(&one, 1);
	return Mode_Import("Menu") && Mode_Put(AT, "traitorius", NULL, &none) && LvN_ScorerOff() && LvN_Draw() && LvN_PutI(AT, "firstTimeDamage", 0) &&
		Mode_SetAlarm(&one) && LvN_Com("Mission_AttackTraitorius_RemoteMessage5", "DeepThroat", AT, "CreateTraitorius", LvN_NoArgs());
}

/* TraitoriusTakesLife(vehicleName): Traitorius took a life item: calm again above the limit, the bar */
static byte Traitorius_TraitoriusTakesLife(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tr = TV(), *name = TV(), *life = TV();
	int his = Mode_G(AT, "traitorius", tr) ? Native_True(tr) : -1;
	if (his > 0) {
		his = Mode_VGet(tr, "Name", name) ? !Native_Compare(name, &a[0]) : -1;
	}
	if (his <= 0) {
		return Mode_Done(&t, his == 0);
	}
	int calm = Mode_Read(AT, "traitorius", tr) && Mode_VGet(tr, "Life", life) ? At_LifeCmp(tr, life, ">") : -1;
	return Mode_Done(&t, calm >= 0 && (!calm || At_Calm(2)) && At_LifeBar());
}

/* TimeExpired(): the minute is over: no scorer, no more hits counted, Traitorius gives up */
static byte Traitorius_TimeExpired(void) {
	return Mode_Import("Menu") && LvN_ScorerOff() && LvN_Draw() && Mode_ListSet("Traitorius", "OnDamage", "") && Mode_ListSet("Traitorius", "OnDeath", "") &&
		LvN_Com("Mission_AttackTraitorius_RemoteMessage4", "Traitorius", AT, "PreCallTraitorius", LvN_NoArgs());
}

/* PreCallTraitorius(): frightened: a waypoint, his call, Traitorius back into the traffic */
static byte Traitorius_PreCallTraitorius(void) {
	return Mode_Import("Parking") && LvN_ModNone("Parking", "SwitchVehicleMissionCallback") && Mode_SaveS("TraitoriusFrightened", "1") &&
		Mode_MMDesc("Mission_TalkToTraitorius_MissionText1", 1) && Mode_WayPoint() && LV_OWN(AT, "CallTraitorius", NULL, "") &&
		Mode_GSetS(AT, "traitorius", "TgType", "None") && At_State(3, NULL, 0);
}

static byte Traitorius_CallTraitorius(void) {
	return Mode_PhoneCab("Mission_AttackTraitorius.UsePhoneCab", 1, AT, NULL);
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): the police warning while the alarm is on, else the talk with
 * Traitorius and DeepThroat */
static byte Traitorius_UsePhoneCab(const nativeVal_t *a) {
	nativeVal_t key = { 0 }, e = { 0 };
	int alarm = Mode_PhoneUse(AT, a);
	if (alarm > 0) {
		return Os_PoliceTalk(AT, "Generics_StillPolice1_Functionary", "Traitorius");
	}
	LvN_Str(&key, "Mission_TalkToTraitorius_ComDeepThroat_NI_1Msg");
	byte ok = alarm == 0 && LvN_OnEnd(AT, "SetNextMission", NULL) && LV_CALL("Speech", "NISpeech", NULL, "s", "Functionary") &&
		LV_CALL("Speech", "AutoAddNI", NULL, "s", "Mission_TalkToTraitorius_ComTraitorius_NI_1Msg") &&
		Mode_CallKw("Speech", "AutoAddNI", NULL, &key, 1, Py_BuildValue("{s:s}", "HeadName", "DeepThroat")) && LV_CALL("Speech", "RunNI", NULL, "") &&
		Mode_EntityH(&a[1], &e) && LvN_SetS(&e, "UseFunc", "PhoneCab.UseAudioCom");
	Native_Free(&key);
	Native_Free(&e);
	return ok;
}

/* SetNextMission(): the cab free again, on to Mission_BeatPickus in Police */
static byte Traitorius_SetNextMission(void) {
	static const char *const places[] = { "MayorAtTownHall", "1", "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Police",
		"Journalist.Betty_Defaut", "Police", "Journalist.Berto_Defaut", "TownHall", NULL };
	if (!Mode_GSetS(AT, "pc", "UseFunc", "PhoneCab.DefUseAudioCom") || !Mode_Target("") || !Mode_DelSaveVars("TraitoriusFrightened") ||
		!LvN_Next("Mission_BeatPickus", "Police", "Mission_FlowerPower_MissionText1")) {
		return 0;
	}
	Mode_Saves(places);
	return Mode_WayPoint();
}

/* SScorer.Set(id, item, var, traitorius.<attr>) */
static byte At_ScorerAttr(const nativeVal_t *id, const char *item, const char *var, const char *attr) {
	nativeVal_t tr = { 0 }, v = { 0 };
	byte ok = Mode_G(AT, "traitorius", &tr) && Mode_VGet(&tr, attr, &v) && LvN_ScorerSet(id, item, var, &v);
	Native_Free(&tr);
	Native_Free(&v);
	return ok;
}

/* MissionScorer(id): Traitorius' life bar with the mark of the limit, the countdown */
static byte Traitorius_MissionScorer(const nativeVal_t *id) {
	static const char bar[] = "RemainingLifeScorer", mark[] = "MinimunLifeScorer";
	modeVals_t t = { .n = 0 };
	nativeVal_t *w = TV(), *h = TV(), *x = TV(), *pct = TV(), *wp = TV(), *mx = TV(), *y = TV(), *sy = TV(), *tx = TV(), *ty = TV(), *half = TV();
	Native_SetFloat(half, 1.0 / 2);
	byte ok = Mode_LvLabel(id, "RemainingLifeText", 0, "Mission_AttackTraitorius_Scorer_1", 1) && Mode_ScorerAddS(id, mark, "Sprite") &&
		Mode_ScorerSetI(id, mark, "Red", 0) && Mode_ScorerSetI(id, mark, "Green", 255) && Mode_ScorerSetI(id, mark, "Blue", 0) &&
		Mode_ScorerAddS(id, bar, "Bar") && Mode_BarDefaults(id, bar, NULL, NULL) && At_ScorerAttr(id, bar, "MaxValue", "MaxLife") &&
		At_ScorerAttr(id, bar, "Value", "Life") && Mode_ScorerSetI(id, bar, "Visible", 1) && Mode_ScorerGetV(id, bar, "W", w) &&
		Mode_ScorerGetV(id, bar, "H", h) && Mode_ScorerGetV(id, bar, "X", x) && Mode_G(AT, "minLifePercentage", pct) && Mode_Arith('*', w, pct, wp) &&
		Native_Arith('+', x, wp, mx) && LvN_ScorerSet(id, mark, "X", mx) && Mode_ScorerGetV(id, bar, "Y", y) && LvN_ScorerSet(id, mark, "Y", y) &&
		LvN_ScorerSet(id, mark, "ScaleX", half) && Mode_Arith('/', h, Mode_I(&t, 4), sy) && LvN_ScorerSet(id, mark, "ScaleY", sy) &&
		Mode_ScorerSetI(id, mark, "Visible", 1) && LvN_TextX(40, tx) && LvN_TextY(60, ty) &&
		Mode_LvTimer(id, "MinimunLifeTimer", tx, ty, "Mission_AttackTraitorius.TimeExpired", 1);
	return Mode_Done(&t, ok);
}

LV_CORE0(Traitorius_Init)
LV_CORE0(Traitorius_ReassignTarget)
LV_CORE0(Traitorius_CreateTraitorius)
LV_COREA(Traitorius_OnTraitoriusDamage)
LV_CORE0(Traitorius_OnTraitoriusDeath)
LV_COREA(Traitorius_TraitoriusTakesLife)
LV_CORE0(Traitorius_TimeExpired)
LV_CORE0(Traitorius_PreCallTraitorius)
LV_CORE0(Traitorius_CallTraitorius)
LV_COREA(Traitorius_UsePhoneCab)
LV_CORE0(Traitorius_SetNextMission)
LV_COREA(Traitorius_MissionScorer)
LV_FN(Traitorius, InitPy, 1, Traitorius_Init_C)
LV_FN(Traitorius, ReassignTargetPy, 0, Traitorius_ReassignTarget_C)
LV_FN(Traitorius, CreateTraitoriusPy, 0, Traitorius_CreateTraitorius_C)
LV_FN(Traitorius, OnTraitoriusDamagePy, 3, Traitorius_OnTraitoriusDamage_C)
LV_FN(Traitorius, OnTraitoriusDeathPy, 2, Traitorius_OnTraitoriusDeath_C)
LV_FN(Traitorius, TraitoriusTakesLifePy, 1, Traitorius_TraitoriusTakesLife_C)
LV_FN(Traitorius, TimeExpiredPy, 0, Traitorius_TimeExpired_C)
LV_FN(Traitorius, PreCallTraitoriusPy, 0, Traitorius_PreCallTraitorius_C)
LV_FN(Traitorius, CallTraitoriusPy, 0, Traitorius_CallTraitorius_C)
LV_FN(Traitorius, UsePhoneCabPy, 4, Traitorius_UsePhoneCab_C)
LV_FN(Traitorius, SetNextMissionPy, 0, Traitorius_SetNextMission_C)
LV_FN(Traitorius, MissionScorerPy, 1, Traitorius_MissionScorer_C)

PyMethodDef g_OutskirtsAttackTraitoriusMethods[] = {
	{ "GetTraitoriusProfile", Traitorius_GetTraitoriusProfile, 1, NULL },
	{ "InitStatus", Traitorius_InitStatus, 1, NULL },
	{ "Init", Traitorius_InitPy, 1, NULL },
	{ "ReassignTarget", Traitorius_ReassignTargetPy, 1, NULL },
	{ "CreateTraitorius", Traitorius_CreateTraitoriusPy, 1, NULL },
	{ "OnTraitoriusDamage", Traitorius_OnTraitoriusDamagePy, 1, NULL },
	{ "OnTraitoriusDeath", Traitorius_OnTraitoriusDeathPy, 1, NULL },
	{ "TraitoriusTakesLife", Traitorius_TraitoriusTakesLifePy, 1, NULL },
	{ "TimeExpired", Traitorius_TimeExpiredPy, 1, NULL },
	{ "PreCallTraitorius", Traitorius_PreCallTraitoriusPy, 1, NULL },
	{ "CallTraitorius", Traitorius_CallTraitoriusPy, 1, NULL },
	{ "UsePhoneCab", Traitorius_UsePhoneCabPy, 1, NULL },
	{ "SetNextMission", Traitorius_SetNextMissionPy, 1, NULL },
	{ "MissionScorer", Traitorius_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FunctionaryHunting: in a police ship with four police friends, 15 functionaries to kill ---- */

static const char *const func_enemies_profile[] = { "Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=1",
	"GoNextItemRace=0", "ControlHookRace=0", "CadenceShootCM=4", "UsePercSonic=100", "UsePercCloud=0", "UsePercElectro=0", "UsePercHook=0",
	"FactorHookMap=2", "CadenceShoot=6", "ControlAim=3", "AngAttack=1.5423", "CounterAttack=0.50", "ControlBoost=1", "ControlBoostItemLife=1",
	"ControlBoostItemAmmo=1", "FactorDamageGetAway=0.25", "FactorDamageItemLife=0.2", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1",
	"UsePercHookGetAway=0", "TimeMaxStrafe=4", "VelMaxStrafe=0.5", "LifeMult=1.25", NULL };

static const char *const police_friends_profile[] = { "Ship=SPOLI2", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=1",
	"GoNextItemRace=0", "ControlHookRace=0", "CadenceShootCM=6", "UsePercSonic=90", "UsePercCloud=10", "UsePercElectro=0", "UsePercHook=0",
	"FactorHookMap=2", "CadenceShoot=4", "ControlAim=7", "AngAttack=2.7", "CounterAttack=0.50", "ControlBoost=0", "ControlBoostItemLife=1",
	"ControlBoostItemAmmo=1", "FactorDamageGetAway=0.40", "FactorDamageItemLife=0.25", "FactorItemAmmo=0.25", "ControlStrafeEvasion=0",
	"UsePercHookGetAway=0", "TimeMaxStrafe=4", "VelMaxStrafe=0.5", "LifeMult=1.25", NULL };

static PyObject *Hunt_GetFuncEnemiesProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, FH, func_enemies_profile);
}

static PyObject *Hunt_GetPoliceFriendsProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, FH, police_friends_profile);
}

static PyObject *Hunt_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, FH, ships_all);
}

/* `isPoliceShip(v)` of the module (its global function): 1, 0, -1 */
static int Fh_IsPolice(const nativeVal_t *v) {
	nativeVal_t r = { 0 };
	int t = LV_OWN(FH, "isPoliceShip", &r, "v", v) ? Native_True(&r) : -1;
	Native_Free(&r);
	return t;
}

/* Init(LevelPath): back after an interruption, or the meeting point (DeepThroat's hint without a police ship) */
static byte Hunt_Init(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *zero = TV(), *n = TV(), *e = TV();
	Native_SetInt(zero, 0);
	int back = Mode_SetAlarm(zero) && Mode_SaveInt("NumFunctionariesKilled", n) && Mode_Put(FH, "numFunctionariesKilled", NULL, n) &&
		LV_CALL("SScorer", "AddModel", NULL, "isssii", 0L, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3L, 25L) &&
		Mode_Read(FH, "numFunctionariesKilled", n) ? Mode_CmpI(n, ">=", 1) : -1;
	if (back != 0) {
		return Mode_Done(&t, back > 0 && LV_OWN(FH, "ComeBack", NULL, ""));
	}
	int police = LV_OWN(FH, "CreateWaypoint", NULL, "") && Mode_UsrHandle(e) ? Native_True(e) : -1;
	if (police > 0) {
		police = Mode_VIsType(e, "Car");
	}
	if (police > 0) {
		police = Fh_IsPolice(e);
	}
	return Mode_Done(&t, police >= 0 && (police || LvN_Com("Mission_FunctionaryHunting_RemoteMessage1", "DeepThroat", NULL, NULL, NULL)));
}

static byte Hunt_CreateWaypoint(void) {
	return Os_StartWaypoint(FH, "DM_Mission_FunctionaryHunting", "No existe el dummy para esta misi\xf3n: 'DM_Mission_FunctionaryHunting'",
		"Mission_FunctionaryHunting.OnMissionStart");
}

static byte Hunt_EnableMissionWaypoint(void) {
	return Os_EnableWaypoint(FH, "Mission_FunctionaryHunting.OnMissionStart");
}

/* MissionScorer(id): the functionaries killed of all */
static byte Hunt_MissionScorer(const nativeVal_t *id) {
	nativeVal_t y = { 0 }, n = { 0 };
	byte ok = Mode_Import("Scorer") && Mode_LvLabel(id, "RemainingCharsText", 0, "Mission_FunctionaryHunting_Scorer_1", 0) && LvN_TextY(20, &y) &&
		Mode_CountText(FH, "numFunctionariesKilled", "FUNCTIONARIES_TO_KILL", &n) && Mode_LvText(id, "RemainingCharsScorer", &y, &n, NULL);
	Native_Free(&y);
	Native_Free(&n);
	return ok;
}

/* every ship of the list taken out of it and deleted */
static byte Fh_Clear(const char *list) {
	nativeVal_t e = { 0 };
	byte ok = 1;
	while (ok) {
		Native_Free(&e);
		ok = Mode_FirstOf(list, &e);
		if (!ok || !Native_True(&e)) {
			break;
		}
		ok = LvN_SetS(&e, "EntityList", "") && Mode_VDel(&e);
	}
	Native_Free(&e);
	return ok;
}

/* ComeBack(): the mission interrupted: no new functionaries nor scorer, the friends and enemies gone, the meeting
 * point again */
static byte Hunt_ComeBack(void) {
	nativeVal_t zero = { 0 };
	Native_SetInt(&zero, 0);
	return Mode_Import("Parking") && Mode_Import("Menu") && Mode_DelSchedS("CreateNewFuncShip") && LvN_ScorerOff() && Mode_SetLockAlarm(&zero) &&
		LvN_Com("Mission_FunctionaryHunting_RemoteMessage5", "Police", NULL, NULL, NULL) && LvN_ModNone("Parking", "SwitchCharMissionCallback") &&
		LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", FH, "OnChangeVehicle") && Fh_Clear("Friends") && Fh_Clear("Enemies") &&
		LV_OWN(FH, "CreateWaypoint", NULL, "");
}

/* OnChangeVehicle(): not a police ship: DeepThroat's hint */
static byte Hunt_OnChangeVehicle(void) {
	nativeVal_t v = { 0 };
	int police = Mode_Import("Parking") && Mode_UsrHandle(&v) ? Fh_IsPolice(&v) : -1;
	Native_Free(&v);
	return police >= 0 && (police || (LvN_ModNone("Parking", "SwitchVehicleMissionCallback") &&
		LvN_Com("Mission_FunctionaryHunting_RemoteMessage6", "DeepThroat", NULL, NULL, NULL)));
}

/* "<prefix>" + `(i + 1)` + "_Fighter" at the nearest free spawn point with the profile of the mission (the global
 * function profile), into e; its Style - 500 and WeapList as both kinds of ships have them */
static byte Fh_Fighter(const char *prefix, const nativeVal_t *i, const char *profile, nativeVal_t *e) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *i1 = TV(), *rep = TV(), *n1 = TV(), *name = TV(), *spw = TV(), *pd = TV();
	byte ok = Native_Arith('+', i, Mode_I(&t, 1), i1);
	if (ok) {
		Mode_Repr(i1, rep);
	}
	ok = ok && Native_Arith('+', Mode_S(&t, prefix), rep, n1) && Native_Arith('+', n1, Mode_S(&t, "_Fighter"), name) &&
		LV_CALL("OutMap", "GetNearestFreeSpawnPos", spw, "") && LvN_CreateAt(name, spw, "Car", e) && LvN_StdProfile(FH, profile, e, pd);
	return Mode_Done(&t, ok);
}

/* e.WeapList and e.Style - 500 of the ships of the mission */
static byte Fh_Weapons(const nativeVal_t *e) {
	nativeVal_t st = { 0 }, st2 = { 0 };
	byte ok = LvN_SetS(e, "WeapList", "0,1,1,1,0,0,1") && Mode_VGet(e, "Style", &st) && Native_Arith('-', &st, &(nativeVal_t){ .type = NV_INT, .i = 500 }, &st2) &&
		Mode_VSet(e, "Style", &st2);
	Native_Free(&st);
	Native_Free(&st2);
	return ok;
}

/* CreateNewPoliShip(i, pos, targetPos): a police friend at pos facing targetPos; its name */
static byte Hunt_CreateNewPoliShip_C(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t poli = { 0 };
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("RaceMaker") && Mode_Import("OutMap") && Fh_Fighter("Poli", &a[0], "GetPoliceFriendsProfile", &poli) &&
		Mode_VSet(&poli, "Pos", &a[1]) && LvN_SetI(&poli, "Life", 40) && LvN_SetI(&poli, "MinLife", 40) && LvN_SetI(&poli, "MaxLife", 40) &&
		Fh_Weapons(&poli) && LvN_SetS(&poli, "TgType", "green") && LV_CALL("RaceMaker", "PointObjectTo", NULL, "vv", &poli, &a[2]) &&
		Mode_VGet(&poli, "Name", out);
	Native_Free(&poli);
	return ok;
}

/* CreateNewFuncShip(i): another functionary enemy unless enough are there for the kills left */
static byte Hunt_CreateNewFuncShip(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *num = TV(), *all = TV(), *killed = TV(), *left = TV(), *fn = TV(), *ang = TV();
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_ListGet("Enemies", "Num", num) && Mode_G(FH, "FUNCTIONARIES_TO_KILL", all) &&
		Mode_G(FH, "numFunctionariesKilled", killed) && Native_Arith('-', all, killed, left);
	int enough = ok ? Mode_Cmp(num, ">=", left) : -1;
	if (enough != 0) {
		return Mode_Done(&t, enough > 0);
	}
	Mode_Ints3(ang, 0, 0, 0);
	ok = Fh_Fighter("Func", &a[0], "GetFuncEnemiesProfile", fn) && LvN_Life(fn, FH, "FUNCS_LIFE") && Fh_Weapons(fn) && Mode_VSet(fn, "Ang", ang) &&
		LvN_SetS(fn, "TgType", "red") && LvN_SetS(fn, "EntityList", "Enemies") && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", fn, 1L);
	return Mode_Done(&t, ok);
}

/* the four corners around center at dist: [(x + dist, y, z + dist), (x + dist, y, z - dist), (x - dist, y, z + dist),
 * (x - dist, y, z - dist)] */
static byte Fh_Corners(const nativeVal_t *center, long dist, nativeVal_t *out) {
	static const int sx[4] = { 1, 1, -1, -1 }, sz[4] = { 1, -1, 1, -1 };
	nativeVal_t x = { 0 }, y = { 0 }, z = { 0 }, d = { 0 };
	byte ok = LvN_Item(center, 0, &x) && LvN_Item(center, 1, &y) && LvN_Item(center, 2, &z);
	Native_SetInt(&d, dist);
	Native_SetEmpty(out, NV_LIST);
	for (int i = 0; ok && i < 4; i++) {
		nativeVal_t *c = Native_Append(out);
		Native_SetEmpty(c, NV_TUPLE);
		ok = Native_Arith(sx[i] > 0 ? '+' : '-', &x, &d, Native_Append(c));
		Native_Copy(Native_Append(c), &y);
		ok = ok && Native_Arith(sz[i] > 0 ? '+' : '-', &z, &d, Native_Append(c));
	}
	Native_Free(&x);
	Native_Free(&y);
	Native_Free(&z);
	return ok;
}

/* CreateFriendsShips(center): four police friends around the meeting point fly in towards it */
static byte Hunt_CreateFriendsShips(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *initial = TV(), *final = TV();
	byte ok = Fh_Corners(&a[0], 5000, initial) && Fh_Corners(&a[0], 2000, final);
	for (int i = 0; ok && i < 4; i++) {
		nativeVal_t name = { 0 };
		const nativeVal_t *p = &final->items[i];
		ok = LV_OWN(FH, "CreateNewPoliShip", &name, "ivv", (long)i, &initial->items[i], &a[0]) && Mode_Append(FH, "initialFriendsList", &name) &&
			LV_CALL("SAI", "SetStateVehicle", NULL, "ivvvviiddd", 1L, &name, &p->items[0], &p->items[1], &p->items[2], 450L, 0L, 0.1, 20.0, 20.0);
		Native_Free(&name);
	}
	return Mode_Done(&t, ok);
}

/* CreateEnemiesShips(): up to five functionaries at once */
static byte Hunt_CreateEnemiesShips(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *all = TV(), *killed = TV(), *n = TV(), *key = TV(), *six = TV();
	LvN_Str(key, "SFunc1");
	Native_SetInt(six, 6);
	byte ok = Mode_Import("VehiclesNPC") && Mode_AtSet("VehiclesNPC", "AIProf", key, "CadenceShoot", six) &&
		Mode_ListSet("Enemies", "OnDeath", "Mission_FunctionaryHunting.OnFuncDeath") && Mode_G(FH, "FUNCTIONARIES_TO_KILL", all) &&
		Mode_G(FH, "numFunctionariesKilled", killed) && Native_Arith('-', all, killed, n);
	int five = ok ? Mode_CmpI(n, ">=", 5) : -1;
	if (five > 0) {
		Native_Free(n);
		Native_SetInt(n, 5);
	}
	long count = 0;
	ok = five >= 0 && Mode_Range(n, &count);
	for (long i = 0; ok && i < count; i++) {
		ok = LV_OWN(FH, "CreateNewFuncShip", NULL, "i", i);
	}
	return Mode_Done(&t, ok);
}

/* SetNextMission(): the characters' places, on to Mission_PoliceBossCheats, the police blueprints, Rusty and the
 * crazy gambler afterwards */
static byte Hunt_SetNextMission(void) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "Police", "Journalist.Berto_Defaut", "Police", NULL };
	static const char *const ships[] = { "SPoli2", "SPoli3", "SPoli4" }, *const engines[] = { "MPoli2", "MPoli3", "MPoli4" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *args = TV(), *who = TV(), *show = TV(), *p = TV(), *d = TV();
	if (!Mode_DelSaveVars("NumFunctionariesKilled")) {
		return Mode_Done(&t, 0);
	}
	Mode_Saves(places);
	LvN_Str(args, "Mission_FunctionaryHunting_RemoteMessage8");
	LvN_Str(who, "Rusty");
	Native_SetEmpty(p, NV_TUPLE);
	Native_SetInt(Native_Append(p), 0);
	Native_SetInt(Native_Append(p), 2);
	Native_SetFloat(d, 10.0);
	byte ok = Mode_WayPoint() && LvN_Next("Mission_PoliceBossCheats", "Police", "Mission_TalkToFicusFriend_MissionText1") &&
		Mode_SaveCat(FH, "PoliceBlueprints.Ships", ships, 3) && Mode_SaveCat(FH, "PoliceBlueprints.Engines", engines, 3) && Mode_Import("Scorer") &&
		LvN_ModGet("Scorer", "ShowMissionScorer", show) && Mode_LaterSpeech("AddRemoteCom", d, args, 4, NULL) && Mode_Later(2, FH, "CGCalls", NULL, 0, NULL);
	return Mode_Done(&t, ok);
}

/* CGCalls(): the crazy gambler's message by the number of crazy deals done (not during the twelfth one) */
static byte Hunt_CGCalls(void) {
	static const char *const keys[] = { "Mission_TalkToFicusFriend_RemoteMessage1", "Mission_TalkToFicusFriend_RemoteMessage2",
		"Mission_TalkToFicusFriend_RemoteMessage3" };
	return Mode_CGCalls(keys, 3);
}

/* OnFuncDeath(LooserName, WinnerName): one more killed; another functionary later, or the end: the friends back to
 * the police, the enemies to the traffic, the police's message */
static byte Hunt_OnFuncDeath(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = TV(), *text = TV(), *zero = TV(), *e = TV(), *mp = TV(), *sl = TV(), *name = TV();
	int more = LvN_GAdd(FH, "numFunctionariesKilled", 1, NULL) && Mode_G(FH, "numFunctionariesKilled", n) ? Mode_CmpG(n, "<", FH, "FUNCTIONARIES_TO_KILL") : -1;
	if (more > 0) {
		Native_SetInt(zero, 0);
		byte ok = Mode_CountText(FH, "numFunctionariesKilled", "FUNCTIONARIES_TO_KILL", text) && LvN_Set0("RemainingCharsScorer", "Text", text) &&
			Mode_Read(FH, "numFunctionariesKilled", n);
		if (ok) {
			LvN_SaveRepr("NumFunctionariesKilled", n);
		}
		return Mode_Done(&t, ok && Mode_Later(3, FH, "CreateNewFuncShip", zero, 1, "CreateNewFuncShip"));
	}
	if (more < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(zero, 0);
	byte ok = Mode_Import("Menu") && Mode_Import("Parking") && Mode_SetLockAlarm(zero) && LvN_ModNone("Parking", "SwitchCharMissionCallback") &&
		LvN_ScorerOff() && LvN_Draw() && Mode_DelSchedS("CreateNewFuncShip") && Mode_FirstOf("Friends", e);
	while (ok && Native_True(e)) {
		Native_Free(mp);
		Native_Free(sl);
		ok = Mode_VGet(e, "MPil0", mp) && Native_Slice(mp, 2, INT_MAX, sl);
		if (ok && LvN_EqS(sl, "Police")) {
			ok = LvN_SetS(e, "EntityList", "OutCop") && LvN_SetS(e, "TgType", "none");
			Native_Free(e);
			ok = ok && Mode_FirstOf("Friends", e);
		} else if (ok) {
			ok = Os_Next(name, e);
		}
	}
	Native_Free(e);
	ok = ok && Mode_FirstOf("Enemies", e);
	while (ok && Native_True(e)) {
		ok = LvN_SetS(e, "EntityList", "Traffic");
		Native_Free(e);
		ok = ok && Mode_FirstOf("Enemies", e);
	}
	ok = ok && LvN_Com("Mission_FunctionaryHunting_RemoteMessage7", "Police", FH, "SetNextMission", NULL);
	return Mode_Done(&t, ok);
}

/* isPoliceShip(v): string.find(string.upper(v.Model), "POLI") != -1 */
static byte Hunt_isPoliceShip_C(const nativeVal_t *a, nativeVal_t *out) {
	int r = Mode_ModelHas(&a[0], "POLI");
	if (r >= 0) {
		Native_SetInt(out, r);
	}
	return r >= 0;
}

/* OnMissionStart(ItemName, VehicleName): the player at the meeting point: in a police ship, without alarm and
 * enemies, the friends come and the camera shows them */
static byte Hunt_OnMissionStart(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *v = TV(), *wp = TV(), *pos = TV(), *msg = TV();
	if (!Mode_Import("Parking") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int skip = Native_True(usr) ? Os_NotUsr(usr, &a[1]) : 1;
	if (skip != 0) {
		return Mode_Done(&t, skip > 0);
	}
	int police = Mode_EntityH(&a[1], v) ? Fh_IsPolice(v) : -1;
	if (police == 0) {
		return Mode_Done(&t, Mode_G(FH, "missionStartWaypoint", wp) && LvN_SetS(wp, "OnTake", "") &&
			LvN_Com("Mission_FunctionaryHunting_RemoteMessage4", "DeepThroat", FH, "EnableMissionWaypoint", NULL));
	}
	int busy = police > 0 ? Os_Busy() : -1;
	if (busy > 0) {
		const nativeVal_t *items[1] = { msg };
		LvN_Str(msg, "No deberia entrar aqui por segunda vez");
		return Mode_Done(&t, Mode_Print(items, 1) && Mode_G(FH, "missionStartWaypoint", wp) && LvN_SetS(wp, "OnTake", "") &&
			LvN_Com("Mission_FunctionaryHunting_RemoteMessage3", "Police", FH, "EnableMissionWaypoint", NULL));
	}
	byte ok = busy == 0 && Mode_G(FH, "missionStartWaypoint", wp) && LvN_SetI(wp, "Active", 0) && Mode_Read(FH, "missionStartWaypoint", wp) &&
		Mode_VGet(wp, "Pos", pos) && LV_OWN(FH, "CreateFriendsShips", NULL, "v", pos) && LV_OWN(FH, "StartCameraTravelling", NULL, "");
	return Mode_Done(&t, ok);
}

/* LaunchMission(): the alarm locked, the scorer, the meeting point gone, the enemies, the friends in their list */
static byte Hunt_LaunchMission(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *one = TV(), *wp = TV(), *none = TV(), *lst = TV();
	Native_SetInt(one, 1);
	byte ok = Mode_Import("Menu") && Mode_Import("Parking") && Mode_SetLockAlarm(one) && LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", FH, "ComeBack") &&
		LV_OWN(FH, "MissionScorer", NULL, "i", 0L) && LvN_ScorerCallG(FH, "MissionScorer") && Mode_G(FH, "missionStartWaypoint", wp) && Mode_VDel(wp) &&
		Mode_Put(FH, "missionStartWaypoint", NULL, none) && LV_OWN(FH, "CreateEnemiesShips", NULL, "") && LvN_GElems(FH, "initialFriendsList", lst);
	int cnt = ok ? Native_ForCount(lst) : -1;
	ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t name = { 0 }, e = { 0 };
		ok = Native_SeqItem(lst, k, &name) && Mode_EntityH(&name, &e) && (!Native_True(&e) || LvN_SetS(&e, "EntityList", "Friends"));
		Native_Free(&name);
		Native_Free(&e);
	}
	return Mode_Done(&t, ok);
}

/* StartCameraTravelling(): the player stops at the meeting point, a camera path towards it shows the friends */
static byte Hunt_StartCameraTravelling(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *uc = TV(), *wp = TV(), *v = TV(), *prev = TV(), *wang = TV(), *add = TV(), *ang = TV(), *rot = TV(), *r0 = TV();
	nativeVal_t *wpos = TV(), *p0 = TV(), *r1 = TV(), *p1 = TV(), *cam = TV(), *ang1 = TV(), *name = TV(), *tt = TV(), *now = TV();
	nativeVal_t *at1 = TV(), *at = TV(), *args = TV(), *args2 = TV();
	int c = Mode_UsrHandle(usr) ? (Native_True(usr) ? (Mode_VGet(usr, "UsrControl", uc) ? !LvN_EqI(uc, 0) : -1) : 0) : -1;
	if (c != 0) {
		return Mode_Done(&t, c > 0);
	}
	Mode_Ints3(v, 0, 0, 0);
	byte ok = Mode_Import("Menu") && LV_CALL("Menu", "MovieScorer", NULL, "i", 0L) && LvN_SetI(usr, "UsrControl", -1) && Mode_VSet(usr, "Vel", v) &&
		Mode_G(FH, "missionStartWaypoint", wp) && Mode_VGet(wp, "Pos", wpos) && Mode_VSet(usr, "MovePos", wpos) && Mode_VGet(wp, "Ang", wang) &&
		Mode_VSet(usr, "MoveAng", wang) && LvN_SetF(usr, "MoveTime", 1.0) && LvN_SetI(usr, "MoveFade", 2) && LvN_SetI(usr, "AvoidCrash", 1) &&
		Mode_GVarSetS("EscapeEvent", "") && Mode_GVarSetS("MainMissionEvent", "") && LV_CALL("Scrap", "GetCam", prev, "i", 0L);
	if (ok) {
		Mode_Ints3(add, 15, 0, 0);
		Mode_Ints3(rot, 0, 0, -8000);
		ok = Mode_Read(FH, "missionStartWaypoint", wp) && (Native_Free(wang), Mode_VGet(wp, "Ang", wang)) && Mode_VecAdd(wang, add, ang) &&
			Mode_Rotate(rot, ang, r0) && Mode_Read(FH, "missionStartWaypoint", wp) && (Native_Free(wpos), Mode_VGet(wp, "Pos", wpos)) &&
			Mode_VecAdd(r0, wpos, p0);
	}
	if (ok) {
		Native_Free(rot);
		Mode_Ints3(rot, 0, 0, -2000);
		ok = Mode_Rotate(rot, ang, r1) && Mode_Read(FH, "missionStartWaypoint", wp) && (Native_Free(wpos), Mode_VGet(wp, "Pos", wpos)) &&
			Mode_VecAdd(r1, wpos, p1) && LvN_CreateAt(Mode_S(&t, "RaceCamera"), p0, "CamPath", cam) && LvN_SetI(cam, "nWayPoint", 2) &&
			Mode_VSet(cam, "TgtCam00", prev) && LvN_SetS(cam, "TgtCam00", "") && Mode_VSet(cam, "Pos00", p0) && Mode_VSet(cam, "Ang00", ang) &&
			LvN_SetI(cam, "FadeIn00", 1) && LvN_SetI(cam, "FadeOut00", 0) && LvN_SetF(cam, "Time00", 5.0) && Mode_VSet(cam, "TgtCam01", prev) &&
			LvN_SetS(cam, "TgtCam01", "") && Mode_VSet(cam, "Pos01", p1) && (Native_Free(v), LvN_Item(ang, 0, v));
	}
	if (ok) {
		nativeVal_t four = { 0 };
		Native_SetFloat(&four, 4.0);
		Native_SetEmpty(ang1, NV_TUPLE);
		ok = Native_Arith('/', v, &four, Native_Append(ang1)) && LvN_Item(ang, 1, Native_Append(ang1)) && LvN_Item(ang, 2, Native_Append(ang1)) &&
			Mode_VSet(cam, "Ang01", ang1) && LvN_SetI(cam, "FadeIn01", 0) && LvN_SetI(cam, "FadeOut01", 1) && LvN_SetI(cam, "Time", 0) &&
			Mode_VGet(cam, "Name", name) && LV_CALL("Scrap", "SetCam", NULL, "iv", 0L, name) && Mode_VGet(cam, "Time00", tt);
	}
	if (ok) {
		nativeVal_t d = { 0 };
		Native_SetFloat(now, Mode_Time());
		Native_SetFloat(&d, 0.2);
		ok = Native_Arith('+', now, tt, at1) && Native_Arith('+', at1, &d, at) && Mode_VGet(usr, "Name", args) && (Native_Copy(args2, prev), 1) &&
			Mode_AtG(at, FH, "AbortCameraTravelling", args, 2) && Mode_Import("Scene") &&
			LV_CALL("Scene", "SetText", NULL, "sv", "Mission_FunctionaryHunting_RemoteMessage2", tt);
	}
	return Mode_Done(&t, ok);
}

/* AbortCameraTravelling(UsrEntityName, prevCamera): the player and the menus back, the camera back, the race scorer;
 * the mission starts */
static byte Hunt_AbortCameraTravelling(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *cn = TV(), *cam = TV();
	byte ok = 1;
	if (Native_True(&a[1])) {
		ok = Mode_EntityH(&a[0], usr) && (!Native_True(usr) || (LvN_SetF(usr, "MoveTime", 0.0) && LvN_SetI(usr, "UsrControl", 0))) &&
			Mode_GVarSetS("EscapeEvent", "Menu.Init") && Mode_GVarSetS("MainMissionEvent", "MissionsFuncs.ShowMissionInfo") &&
			LV_CALL("Scrap", "GetCam", cn, "i", 0L) && Mode_EntityH(cn, cam) && Mode_VDel(cam) && LV_CALL("Scrap", "SetCam", NULL, "iv", 0L, &a[1]) &&
			Mode_Import("RacerScorer") && LV_CALL("RacerScorer", "Create", NULL, "i", 0L);
	}
	ok = ok && LV_OWN(FH, "LaunchMission", NULL, "");
	return Mode_Done(&t, ok);
}

LV_CORE0(Hunt_Init)
LV_CORE0(Hunt_CreateWaypoint)
LV_CORE0(Hunt_EnableMissionWaypoint)
LV_COREA(Hunt_MissionScorer)
LV_CORE0(Hunt_ComeBack)
LV_CORE0(Hunt_OnChangeVehicle)
LV_COREA(Hunt_CreateNewFuncShip)
LV_COREA(Hunt_CreateFriendsShips)
LV_CORE0(Hunt_CreateEnemiesShips)
LV_CORE0(Hunt_SetNextMission)
LV_CORE0(Hunt_CGCalls)
LV_CORE0(Hunt_OnFuncDeath)
LV_COREA(Hunt_OnMissionStart)
LV_CORE0(Hunt_LaunchMission)
LV_CORE0(Hunt_StartCameraTravelling)
LV_COREA(Hunt_AbortCameraTravelling)
LV_FN(Hunt, InitPy, 1, Hunt_Init_C)
LV_FN(Hunt, CreateWaypointPy, 0, Hunt_CreateWaypoint_C)
LV_FN(Hunt, EnableMissionWaypointPy, 0, Hunt_EnableMissionWaypoint_C)
LV_FN(Hunt, MissionScorerPy, 1, Hunt_MissionScorer_C)
LV_FN(Hunt, ComeBackPy, 0, Hunt_ComeBack_C)
LV_FN(Hunt, OnChangeVehiclePy, 0, Hunt_OnChangeVehicle_C)
LV_FN(Hunt, CreateNewPoliShipPy, 3, Hunt_CreateNewPoliShip_C)
LV_FN(Hunt, CreateNewFuncShipPy, 1, Hunt_CreateNewFuncShip_C)
LV_FN(Hunt, CreateFriendsShipsPy, 1, Hunt_CreateFriendsShips_C)
LV_FN(Hunt, CreateEnemiesShipsPy, 0, Hunt_CreateEnemiesShips_C)
LV_FN(Hunt, SetNextMissionPy, 0, Hunt_SetNextMission_C)
LV_FN(Hunt, CGCallsPy, 0, Hunt_CGCalls_C)
LV_FN(Hunt, OnFuncDeathPy, 2, Hunt_OnFuncDeath_C)
LV_FN(Hunt, isPoliceShipPy, 1, Hunt_isPoliceShip_C)
LV_FN(Hunt, OnMissionStartPy, 2, Hunt_OnMissionStart_C)
LV_FN(Hunt, LaunchMissionPy, 0, Hunt_LaunchMission_C)
LV_FN(Hunt, StartCameraTravellingPy, 0, Hunt_StartCameraTravelling_C)
LV_FN(Hunt, AbortCameraTravellingPy, 2, Hunt_AbortCameraTravelling_C)

PyMethodDef g_OutskirtsFunctionaryHuntingMethods[] = {
	{ "GetFuncEnemiesProfile", Hunt_GetFuncEnemiesProfile, 1, NULL },
	{ "GetPoliceFriendsProfile", Hunt_GetPoliceFriendsProfile, 1, NULL },
	{ "InitStatus", Hunt_InitStatus, 1, NULL },
	{ "Init", Hunt_InitPy, 1, NULL },
	{ "CreateWaypoint", Hunt_CreateWaypointPy, 1, NULL },
	{ "EnableMissionWaypoint", Hunt_EnableMissionWaypointPy, 1, NULL },
	{ "MissionScorer", Hunt_MissionScorerPy, 1, NULL },
	{ "ComeBack", Hunt_ComeBackPy, 1, NULL },
	{ "OnChangeVehicle", Hunt_OnChangeVehiclePy, 1, NULL },
	{ "CreateNewPoliShip", Hunt_CreateNewPoliShipPy, 1, NULL },
	{ "CreateNewFuncShip", Hunt_CreateNewFuncShipPy, 1, NULL },
	{ "CreateFriendsShips", Hunt_CreateFriendsShipsPy, 1, NULL },
	{ "CreateEnemiesShips", Hunt_CreateEnemiesShipsPy, 1, NULL },
	{ "SetNextMission", Hunt_SetNextMissionPy, 1, NULL },
	{ "CGCalls", Hunt_CGCallsPy, 1, NULL },
	{ "OnFuncDeath", Hunt_OnFuncDeathPy, 1, NULL },
	{ "isPoliceShip", Hunt_isPoliceShipPy, 1, NULL },
	{ "OnMissionStart", Hunt_OnMissionStartPy, 1, NULL },
	{ "LaunchMission", Hunt_LaunchMissionPy, 1, NULL },
	{ "StartCameraTravelling", Hunt_StartCameraTravellingPy, 1, NULL },
	{ "AbortCameraTravelling", Hunt_AbortCameraTravellingPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
