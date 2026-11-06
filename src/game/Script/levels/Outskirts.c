/*
 * The scripts of the level Outskirts in C (levels/outskirts/scripts of data01.packed, issue #2): the Map module (the
 * traffic, the metro and the three elevators), MapRace (the start positions of the eleven races) and the logic of the
 * missions of the level (Mission_AttackTraitorius, Mission_Demo_HumphreyRace, Mission_DestroyBadDebtors,
 * Mission_DestroyCommSats1 (in levels/LevelCommon.c), Mission_DestroyHumphrey, Mission_FunctionaryHunting, Mission_GoToOutskirts,
 * Mission_IdentifyMerc, Mission_KillPolice, Mission_KillRudolph, Mission_TransportKiller, Mission_Win2ndRace). MapSnd
 * and the plain functions of the missions are data (data/levels/outskirts/<module>.def). The module variables of the
 * missions and the module code (the save variables they create, MapRace.Races) stay in the scripts; the functions read
 * and write them as their globals.
 */
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

#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define USR() Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0)
#define SAVE(n) Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", n)
#define SETSAVE(n, v) Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", n, v)
#define G(m, n) Sf_Global(&f, m, n)
#define NOARGS Sf_Build(&f, "()")

/* the ships of the traffic of the missions (Traffic.AcShips as the save variable holds it) */
static const char *const ships_police5 = "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SBanker1\",\"SBankMaster1\"]";
static const char *const ships_police4 = "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SBanker1\"]";
static const char *const ships_demo = "[\"SMerc2\", \"SMerc1\", \"SBanker1\"]";
static const char *const ships_all = "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SMerc3\",\"SLMerc\",\"SMayor1\",\"SBanker1\",\"SBishop1\","
	"\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\",\"SBankMaster1\",\"SArchbishop1\"]";

/* the gates of the level closed: the city doors of the hangars and the doors to the other outdoor levels */
static byte Os_CloseDoors(scriptFrame_t *f) {
	static const char *const doors[] = { "DM_Door_CityDoorA_00", "DM_Door_CityDoorA_01", "DM_Door_Outmap_Commercial",
		"DM_Door_Outmap_DownTown", "DM_Door_Outmap_Scrapyard", NULL };
	for (int i = 0; doors[i] && !f->failed; i++) {
		int mark = f->n;
		PyObject *e = Sf_Ask(f, "Scrap", "GetEntity", "(s)", doors[i]);
		if (Sf_True(f, e) > 0) {
			Sf_SetAttrS(f, e, "Action", "Close");
			Sf_SetAttrS(f, e, "DefLoop", "Closed");
			Sf_SetAttrS(f, e, "OnObjSlot", "!CloseDoorSlot");
		}
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* the metro entrance usable again: ArrowCanUse 1, UseFunc use, a mission target */
static PyObject *Os_Metro(scriptFrame_t *f, const char *use) {
	PyObject *e = Sf_Ask(f, "Scrap", "GetEntity", "(s)", "DM_Door_MetroExt_");
	Sf_SetAttrI(f, e, "ArrowCanUse", 1);
	Sf_SetAttrS(f, e, "UseFunc", use);
	return e;
}

/*
 * the start of UsePhoneCab(UsrName, CharName, optId, FullOption) of the missions: Speech.NPCName and PlayerName, the
 * audio com of the cab, the global pc of module the cab; the alarm state (1, 0, -1 after an error)
 */
static int Os_PhoneUse(scriptFrame_t *f, const char *module, PyObject *usr, PyObject *ch, PyObject *opt, PyObject *full) {
	Sf_Import(f, "PhoneCab");
	Sf_SetMod(f, "Speech", "NPCName", ch);
	Sf_SetMod(f, "Speech", "PlayerName", usr);
	Sf_Run(f, "PhoneCab", "UseAudioCom", "(OOOO)", usr, ch, opt, full);
	Sf_SetGlobal(f, module, "pc", Sf_Entity(f, ch));
	return Lv_Alarm(f);
}

/* the NI talk at the cab: Speech.OnEndOfSpeech = the global end of module, no parameters, who says key */
static byte Os_PhoneTalk(scriptFrame_t *f, const char *module, const char *end, const char *who, const char *key) {
	Sf_SpeechEnd(f, Sf_Global(f, module, end), NULL);
	return Sf_NISpeech(f, who, key);
}

/* `while e: ...` over an entity list: the first entity of the list (by EntityListGet(list, "First")) */
static PyObject *Os_First(scriptFrame_t *f, const char *list) {
	return Sf_Entity(f, Sf_Ask(f, "Scrap", "EntityListGet", "(ss)", list, "First"));
}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors; the traffic by who controls the robots, the metro and the elevator to the level
 * the player came from */
static PyObject *Map_InitStatus(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "OutPolice", "string", "Metro", "Traffic", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 1));
	int boss = Sf_EqS(&f, SAVE("RobotsControlledByBoss"), "1");
	if (boss > 0) {
		Lv_BossTraffic(&f, 1);
	} else if (boss == 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 17);
		Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", -1);
		Sf_SetMod(&f, "Traffic", "AcNormalShips", Sf_Build(&f, "[sss]", "TLight1", "TLight2", "TLight3"));
		Sf_SetMod(&f, "Traffic", "NumNormal", Sf_Int(&f, 80));
		Sf_SetMod(&f, "Traffic", "AcMediumShips", Sf_Build(&f, "[sss]", "TMed1", "TMed2", "TMed3"));
		Sf_SetMod(&f, "Traffic", "NumMedium", Sf_Int(&f, 40));
		Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	}
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Metro", "DM_Door_MetroExt_");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "MetroExt", "DM_Door_MetroExt_");
	PyObject *pm = Sf_Ask(&f, "string", "upper", "(O)", Sf_Slice(&f, SAVE("PrevMap"), 7, 0x7fffffff));
	const char *door = "DM_Door_Elevator_GamblinDen";
	int is = Sf_EqS(&f, pm, "BANK");
	if (is > 0) {
		door = "DM_Door_Elevator_Bank";
	} else if (is == 0 && Sf_EqS(&f, pm, "POLICE") > 0) {
		door = "DM_Door_Elevator_Police";
	}
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Elevator", door);
	return Sf_None(&f);
}

/* Init(LevelPath): the colour of the clouds, the elevators to Bank, GamblinDen and Police, the metro */
static PyObject *Map_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "Metro");
	Sf_Import(&f, "Init");
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudRed", 120);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudGreen", 60);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudBlue", 0);
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Bank", "Bank");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_GamblinDen", "GamblinDen");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Police", "Police");
	Sf_Run(&f, "Metro", "SetAsMetro", "(s)", "DM_Door_MetroExt_");
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsMapMethods[] = {
	{ "InitStatus", Map_InitStatus, 1, NULL },
	{ "Init", Map_Init, 1, NULL },
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
static PyObject *Win2nd_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Alarm");
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Run(&f, "RaceMaker", "PreloadCnDnResources", "()");
	PyObject *sp = Sf_Ask(&f, "VehiclesNPC", "cShipAI", "()");
	Sf_Run(&f, "VehiclesNPC", "SetAIProfile", "(OO)", sp, G(WR, "RACE_PROFILE"));
	Sf_SetMod(&f, "Alarm", "TempAbortMissionCallback", G(WR, "OnAlarmOn"));
	Sf_SetMod(&f, "Alarm", "TempRestoreMissionCallback", G(WR, "OnAlarmOff"));
	int alarm = Lv_Alarm(&f);
	if (alarm > 0) {
		Sf_Own(&f, WR, "OnAlarmOn", "()");
	} else if (alarm == 0) {
		Sf_Own(&f, WR, "InitRace", "()");
	}
	return Sf_None(&f);
}

/* RaceMaker.StartPos[iStartPos] */
static PyObject *Wr_Start(scriptFrame_t *f, const char *index) {
	return Sf_GetItem(f, Sf_Mod(f, "RaceMaker", "StartPos"), Sf_Global(f, WR, index));
}

/* InitRace(): the racers around the first checkpoint the player cannot see, the start buoy and its trigger */
static PyObject *Win2nd_InitRace(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Alarm", "Menu", "Speech", "Parking", "RaceMaker", "RacerScorer", "MapRace", "VehiclesNPC",
		"math", NULL };
	SF_ARGS("");
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetGlobal(&f, WR, "racers", Sf_Build(&f, "[]"));
	Sf_SetGlobal(&f, WR, "racersDict", Sf_Build(&f, "{}"));
	int numply = Sf_Len(&f, G(WR, "racersList"));
	Sf_SetGlobal(&f, WR, "iStartPos", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, WR, "nextStartPos", G(WR, "iStartPos"));
	Sf_SetGlobal(&f, WR, "BayScorer", Sf_Build(&f, "{}"));
	Sf_SetGlobal(&f, WR, "YourScore", Sf_Int(&f, -1));
	Sf_SetGlobal(&f, WR, "NumOponNotGoNextItem", Sf_Int(&f, 0));
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDeath", "Mission_Win2ndRace.RacerKilled");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDamage", "Mission_Win2ndRace.RacerDamaged");
	Sf_Run(&f, "MapRace", "Mission_Win2ndRace_Race", "()");
	Sf_SetGlobal(&f, WR, "iStartPos", Sf_Own(&f, WR, "GetRaceWaypointIndex", "()"));
	for (int i = 0; i < numply && !f.failed; i++) {
		int mark = f.n;
		PyObject *p0 = Sf_Item(&f, Wr_Start(&f, "iStartPos"), 0);
		PyObject *rot = Sf_Ask(&f, "SVec", "Rotate3D", "((iii)(iOi))", 0, 2200, 0, 0, Sf_Item(&f, Wr_Start(&f, "iStartPos"), 1),
			90 + 360 * i / numply);
		PyObject *spos = Sf_Ask(&f, "SVec", "Add", "(OO)", p0, rot);
		PyObject *v = Lv_CreateAt(&f, Sf_Cat(&f, "sO", "BadBoy", Sf_ReprI(&f, i)), spos, "Car");
		PyObject *rname = Sf_Item(&f, G(WR, "racersList"), i);
		Sf_Run(&f, "RaceMaker", "InitShipIAProfile", "(OO)", rname, G(WR, "RACE_PROFILE"));
		rname = Sf_Item(&f, G(WR, "racersList"), i);
		PyObject *ship = Sf_Ask(&f, "RaceMaker", "GetRandomShipIAProfile", "(OO)", rname, G(WR, "RACE_PROFILE"));
		Sf_SetItem(&f, G(WR, "racersDict"), Sf_Attr(&f, v, "Name"), ship);
		Sf_Run(&f, "VehiclesNPC", "SetIAProf", "(OO)", v, ship);
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iO)", 0, Sf_Attr(&f, v, "Name"));
		Sf_SetAttrS(&f, v, "TgType", "None");
		Sf_SetAttrS(&f, v, "EntityList", "Race");
		Sf_SetAttr(&f, v, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, Wr_Start(&f, "iStartPos"), 1), 0));
		Sf_SetAttr(&f, v, "MovePos", Sf_Attr(&f, v, "Pos"));
		Sf_SetAttr(&f, v, "MoveAng", Sf_Attr(&f, v, "Ang"));
		Sf_SetAttr(&f, v, "MoveTime", Sf_Float(&f, 0.001));
		Sf_Method(&f, G(WR, "racers"), "append", "(O)", Sf_Attr(&f, v, "Name"));
		Sf_SetItem(&f, G(WR, "BayScorer"), Sf_Attr(&f, v, "Name"), Sf_Int(&f, 0));
		PyObject *pd = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(O)", ship);
		Sf_SetAttrI(&f, pd, "StupidPathFinding", 0);
		Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", v, pd);
		if (Sf_EqI(&f, Sf_Attr(&f, pd, "GoNextItemRace"), 0) > 0) {
			Sf_SetGlobal(&f, WR, "NumOponNotGoNextItem", Sf_OpI(&f, G(WR, "NumOponNotGoNextItem"), '+', 1));
		}
		Sf_Release(&f, mark);
	}
	PyObject *l = Lv_CreateAt(&f, Sf_Str(&f, "CheckPoint"), Sf_Item(&f, Wr_Start(&f, "iStartPos"), 0), "Item");
	Sf_Method(&f, l, "SetModel", "(s)", "models/Misc/Wayp/Wayp.m3d");
	Sf_SetAttrS(&f, l, "OnTake", "Mission_Win2ndRace.OnRaceStart");
	Sf_SetAttrI(&f, l, "RadarShow", 1);
	Sf_SetAttrI(&f, l, "Radius", 2400);
	Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, Wr_Start(&f, "iStartPos"), 1), 0));
	PyObject *l2 = Lv_CreateAt(&f, Sf_Cat(&f, "Os", Sf_Attr(&f, l, "Name"), "_Trigg"), Sf_Item(&f, Wr_Start(&f, "iStartPos"), 0), "Item");
	Sf_SetAttr(&f, l2, "Pos", Sf_Attr(&f, l, "Pos"));
	Sf_SetAttrI(&f, l2, "Radius", 20000);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(s)", "models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d");
	Sf_SetGlobal(&f, WR, "waypointName", Sf_Attr(&f, l, "Name"));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", G(WR, "waypointName"));
	return Sf_None(&f);
}

/* OnCounterMeasure(id=0): the first countermeasure warns, another one more than 5 s later cancels the race */
static PyObject *Win2nd_OnCounterMeasure(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "id", NULL };
	PyObject *id = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", kwlist, &id)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	Sf_Import(&f, "RaceMaker");
	PyObject *usr = USR();
	int skip = Sf_Compare(&f, usr, "==", Py_None);
	if (skip == 0) {
		skip = Sf_EqI(&f, Sf_Attr(&f, usr, "iWeap"), 6);
	}
	if (skip != 0) {
		return Sf_None(&f);
	}
	if (Sf_EqI(&f, G(WR, "LastCmTime"), -1) > 0) {
		Sf_SetGlobal(&f, WR, "LastCmTime", Sf_Time(&f));
		Lv_Com(&f, "Generics_PoliceRace_1", "Police", NULL, NULL);
	} else if (!f.failed && Sf_Compare(&f, Sf_Op(&f, G(WR, "LastCmTime"), '+', Sf_Float(&f, 5.0)), "<", Sf_Time(&f)) > 0) {
		Lv_Com(&f, "Generics_PoliceRace_2", "Police", NULL, NULL);
		Sf_Own(&f, WR, "CancelMission", "()");
		Lv_Com(&f, "Mission_Win2ndRace_RemoteMessage2", "Trurl", G(WR, "InitRace"), NOARGS);
	}
	return Sf_None(&f);
}

/* GetRaceWaypointIndex(): the first checkpoint of the race the player cannot see (TestLine hits the world), else 0 */
static PyObject *Win2nd_GetRaceWaypointIndex(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *list = Sf_Mod(&f, "RaceMaker", "StartPos"), *p;
	for (int k = 0; (p = Sf_Next(&f, list, k)) != NULL; k++) {
		int mark = f.n;
		PyObject *up = Sf_Attr(&f, usr, "Pos");
		PyObject *res = Sf_Ask(&f, "Scrap", "TestLine", "(OOs)", up, Sf_Item(&f, p, 0), "");
		int hit = Sf_True(&f, res);
		if (hit > 0) {
			hit = Sf_EqS(&f, Sf_Item(&f, res, 1), "");
		}
		if (hit > 0) {
			return Sf_Return(&f, Sf_Int(&f, k));
		}
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, f.failed ? NULL : Sf_Int(&f, 0));
}

/* RacerDamaged(LooserName, WinnerName, Damage): the player hits a racer before the start: the race starts */
static PyObject *Win2nd_RacerDamaged(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", winner) > 0 && Sf_EqI(&f, G(WR, "YourScore"), -1) > 0) {
		Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Race", "OnDamage", "");
		Sf_Own(&f, WR, "OnCheckPoint", "(OO)", G(WR, "waypointName"), winner);
	}
	return Sf_None(&f);
}

/* OnRaceStart(ItemName, VehicleName): the player at the start buoy: the camera, the countdown, the start */
static PyObject *Win2nd_OnRaceStart(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	Sf_Import(&f, "RaceMaker");
	if (Lv_UsrNameIs(&f, USR(), veh) > 0) {
		PyObject *t = Sf_Ask(&f, "RaceMaker", "StartCameraTravelling", "(OO)", item, veh);
		Sf_Later(&f, t, G(WR, "RaceGo"), Sf_Build(&f, "(OO)", item, veh), NULL);
		Sf_Run(&f, "RaceMaker", "CreateRaceCountDown", "()");
	}
	return Sf_None(&f);
}

/* SSound.Play(Sound.<snd>, Sound.<vol>) */
static byte Wr_Sound(scriptFrame_t *f, const char *snd, const char *vol) {
	PyObject *fn = Sf_Mod(f, "SSound", "Play");
	PyObject *s = Sf_Mod(f, "Sound", snd);
	return Sf_Line(f, fn, "(OO)", s, Sf_Mod(f, "Sound", vol));
}

/* the race ends for the player: no scorer */
static byte Wr_NoScorer(scriptFrame_t *f, PyObject *l) {
	Sf_Method(f, l, "Del", "()");
	Lv_ScorerCall(f, Py_None);
	return Sf_DrawScorer(f);
}

/* OnCheckPoint(ItemName, VehicleName): a checkpoint taken by the player or a racer: the scores, the buoy moves on and
 * every racer heads for it */
static PyObject *Win2nd_OnCheckPoint(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Sound");
	Sf_Import(&f, "VehiclesNPC");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *usrName = Sf_Attr(&f, usr, "Name");
	int early = Sf_EqI(&f, G(WR, "YourScore"), -1);
	if (early > 0) {
		early = Sf_Compare(&f, veh, "!=", usrName);
	}
	if (early > 0) {
		if (Sf_EqI(&f, G(WR, "YourScore"), -1) > 0) {
			Sf_SetGlobal(&f, WR, "YourScore", Sf_Int(&f, 0));
		}
		return Sf_None(&f);
	}
	PyObject *l = Sf_Entity(&f, item);
	int player = Sf_Compare(&f, usrName, "==", veh);
	if (player > 0) {
		Sf_SetGlobal(&f, WR, "YourScore", Sf_OpI(&f, G(WR, "YourScore"), '+', 1));
		int first = Sf_EqI(&f, G(WR, "YourScore"), 0);
		if (first > 0) {
			Sf_Run(&f, "RacerScorer", "InitMissionScorer", "(iiO)", 0, 0, Sf_Repr(&f, G(WR, "WayPointlimit")));
			Lv_ScorerCall(&f, Sf_Mod(&f, "RacerScorer", "MissionScorer"));
			PyObject *name;
			for (int k = 0; (name = Sf_Next(&f, G(WR, "racers"), k)) != NULL; k++) {
				int mark = f.n;
				Sf_SetAttrS(&f, Sf_Entity(&f, name), "TgType", "Green");
				Sf_Release(&f, mark);
			}
		} else if (first == 0) {
			int won = Sf_Compare(&f, G(WR, "YourScore"), ">=", G(WR, "WayPointlimit"));
			Wr_Sound(&f, "WaySound", "WayVolume");
			if (won > 0) {
				Sf_Own(&f, WR, "CancelMission", "()");
				Wr_NoScorer(&f, l);
				Sf_Own(&f, WR, "PreSetNextMission", "()");
				return Sf_None(&f);
			}
		}
	} else if (player == 0) {
		int racer = Sf_True(&f, Sf_Method(&f, G(WR, "BayScorer"), "has_key", "(O)", veh));
		if (racer == 0) {
			return Sf_None(&f);
		}
		Wr_Sound(&f, "EneWaySound", "EneWayVolume");
		PyObject *bay = G(WR, "BayScorer");
		Sf_SetItem(&f, bay, veh, Sf_OpI(&f, Sf_GetItem(&f, G(WR, "BayScorer"), veh), '+', 1));
		if (Sf_Compare(&f, Sf_GetItem(&f, G(WR, "BayScorer"), veh), ">=", G(WR, "WayPointlimit")) > 0) {
			Wr_NoScorer(&f, l);
			Lv_Com(&f, "Mission_Win2ndRace_RemoteMessage2", "Trurl", G(WR, "InitRace"), NOARGS);
			Sf_Own(&f, WR, "CancelMission", "()");
			return Sf_None(&f);
		}
	}
	PyObject *best = Sf_Int(&f, 0), *e;
	PyObject *keys = Sf_Method(&f, G(WR, "BayScorer"), "keys", "()");
	for (int k = 0; (e = Sf_Next(&f, keys, k)) != NULL; k++) {
		if (Sf_Compare(&f, Sf_GetItem(&f, G(WR, "BayScorer"), e), ">", best) > 0) {
			best = Sf_GetItem(&f, G(WR, "BayScorer"), e);
		}
	}
	PyObject *next = Sf_OpI(&f, G(WR, "iStartPos"), '+', 1);
	Sf_SetGlobal(&f, WR, "iStartPos", Sf_Op(&f, next, '%', Sf_Int(&f, Sf_Len(&f, Sf_Mod(&f, "RaceMaker", "StartPos")))));
	Sf_SetAttr(&f, l, "Pos", Sf_Item(&f, Wr_Start(&f, "iStartPos"), 0));
	Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, Wr_Start(&f, "iStartPos"), 1), 0));
	Sf_SetAttrS(&f, l, "OnTake", "Mission_Win2ndRace.OnCheckPoint");
	PyObject *l2 = Sf_Entity(&f, Sf_Cat(&f, "Os", item, "_Trigg"));
	Sf_SetAttr(&f, l2, "Pos", Sf_Attr(&f, l, "Pos"));
	Sf_SetAttrS(&f, l2, "OnTake", "Mission_Win2ndRace.OnNearCheckPoint");
	PyObject *playerName = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Race", "First");
	e = Sf_Entity(&f, playerName);
	int mark = f.n;
	while (Sf_True(&f, e) > 0) {
		Sf_SetAttr(&f, e, "MoveTime", Sf_Float(&f, 0.0));
		PyObject *pd = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(O)", Sf_GetItem(&f, G(WR, "racersDict"), playerName));
		int own = Sf_Compare(&f, playerName, "==", veh);
		if (own > 0) {
			PyObject *fn = Sf_Mod(&f, "SAI", "SetStateVehicle");
			PyObject *x = Sf_Item(&f, Sf_Attr(&f, l, "Pos"), 0), *y = Sf_Item(&f, Sf_Attr(&f, l, "Pos"), 1);
			PyObject *z = Sf_Item(&f, Sf_Attr(&f, l, "Pos"), 2), *r = Sf_Attr(&f, l, "Radius");
			Sf_Line(&f, fn, "(iOOOOOO)", 6, playerName, x, y, z, r, Sf_Attr(&f, pd, "ControlBoostRace"));
		} else if (own == 0) {
			PyObject *t = Sf_Time(&f);
			t = Sf_Op(&f, t, '+', Sf_Ask(&f, "Scrap", "Rand", "(ii)", 0, 0));
			PyObject *fn = Sf_Mod(&f, "SAI", "SetStateVehicle");
			PyObject *x = Sf_Item(&f, Sf_Attr(&f, l, "Pos"), 0), *y = Sf_Item(&f, Sf_Attr(&f, l, "Pos"), 1);
			PyObject *z = Sf_Item(&f, Sf_Attr(&f, l, "Pos"), 2), *r = Sf_Attr(&f, l, "Radius");
			Sf_Schedule(&f, t, fn, Sf_Build(&f, "(iOOOOOO)", 6, playerName, x, y, z, r, Sf_Attr(&f, pd, "ControlBoostRace")), NULL);
		}
		PyObject *keep[2];
		keep[0] = Sf_Attr(&f, e, "NextInList");
		keep[1] = Sf_Entity(&f, keep[0]);
		Sf_ReleaseKeep(&f, mark, keep, 2);
		playerName = keep[0];
		e = keep[1];
	}
	Sf_SetGlobal(&f, WR, "nextStartPos", G(WR, "iStartPos"));
	Sf_Run(&f, "RacerScorer", "UpdateMissionScorer", "(iOO)", 0, G(WR, "YourScore"), best);
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* OnNearCheckPoint(ItemName, VehicleName): a racer near the buoy: the racers that go on to the next checkpoint head
 * for it, one after another, except the one that keeps going for this one */
static PyObject *Win2nd_OnNearCheckPoint(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "RaceMaker");
	PyObject *ply = Sf_Entity(&f, veh);
	int out = Sf_EqI(&f, Sf_Attr(&f, ply, "UsrControl"), -1);
	if (out > 0) {
		int has = Sf_True(&f, Sf_Method(&f, G(WR, "BayScorer"), "has_key", "(O)", veh));
		out = has < 0 ? -1 : !has;
	}
	if (out != 0) {
		return Sf_None(&f);
	}
	PyObject *l = Sf_Entity(&f, item);
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.5));
	PyObject *n2 = Sf_Op(&f, Sf_OpI(&f, G(WR, "iStartPos"), '+', 1), '%', Sf_Int(&f, Sf_Len(&f, Sf_Mod(&f, "RaceMaker", "StartPos"))));
	if (Sf_Compare(&f, G(WR, "nextStartPos"), "==", n2) != 0) {
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, WR, "nextStartPos", n2);
	PyObject *pos = Sf_Item(&f, Wr_Start(&f, "nextStartPos"), 0);
	PyObject *opon = NULL;
	int user = Sf_EqI(&f, Sf_Attr(&f, ply, "UsrControl"), 0);
	if (user > 0) {
		user = Sf_EqI(&f, G(WR, "NumOponNotGoNextItem"), 0);
	}
	if (user > 0) {
		opon = Sf_Own(&f, WR, "NameOponGoItem", "(O)", item);
	} else if (user == 0) {
		opon = Sf_Str(&f, "");
	}
	PyObject *timy = Sf_Time(&f);
	PyObject *name = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Race", "First");
	PyObject *ent = Sf_Entity(&f, name);
	int mark = f.n;
	while (Sf_True(&f, ent) > 0) {
		if (Sf_Compare(&f, name, "!=", veh) > 0) {
			PyObject *pd = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(O)", Sf_GetItem(&f, G(WR, "racersDict"), name));
			if (Sf_EqI(&f, Sf_Attr(&f, pd, "GoNextItemRace"), 1) > 0 && Sf_Compare(&f, name, "!=", opon) > 0) {
				PyObject *fn = Sf_Mod(&f, "SAI", "SetStateVehicle");
				PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2), *r = Sf_Attr(&f, l, "Radius");
				Sf_Schedule(&f, timy, fn, Sf_Build(&f, "(iOOOOOOi)", 6, name, x, y, z, r, Sf_Attr(&f, pd, "ControlBoostRace"), 1), NULL);
				timy = Sf_Op(&f, timy, '+', Sf_Float(&f, 0.05));
			}
		}
		PyObject *keep[3];
		keep[0] = Sf_Attr(&f, ent, "NextInList");
		keep[1] = Sf_Entity(&f, keep[0]);
		keep[2] = timy;
		Sf_ReleaseKeep(&f, mark, keep, 3);
		name = keep[0];
		ent = keep[1];
	}
	return Sf_None(&f);
}

/* NameOponGoItem(ItemName): the racer farthest from the buoy ("" when none is farther than 0) */
static PyObject *Win2nd_NameOponGoItem(PyObject *self, PyObject *args) {
	PyObject *itemName;
	SF_ARGS("O", &itemName);
	PyObject *sel = Sf_Str(&f, "");
	PyObject *item = Sf_Entity(&f, itemName);
	PyObject *best = Sf_Int(&f, 0);
	PyObject *name = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Race", "First");
	PyObject *ent = Sf_Entity(&f, name);
	int mark = f.n;
	while (Sf_True(&f, ent) > 0) {
		PyObject *ep = Sf_Attr(&f, ent, "Pos");
		PyObject *d = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", ep, Sf_Attr(&f, item, "Pos")));
		if (Sf_Compare(&f, d, ">", best) > 0) {
			best = d;
			sel = name;
		}
		PyObject *keep[4];
		keep[0] = Sf_Attr(&f, ent, "NextInList");
		keep[1] = Sf_Entity(&f, keep[0]);
		keep[2] = best;
		keep[3] = sel;
		Sf_ReleaseKeep(&f, mark, keep, 4);
		name = keep[0];
		ent = keep[1];
	}
	return Sf_Return(&f, sel);
}

/* CancelMission(): no music, no scorer, the buoy goes, the racers back into the traffic, the scores reset */
static PyObject *Win2nd_CancelMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionMusic", 0);
	Lv_ScorerCall(&f, Py_None);
	Sf_DrawScorer(&f);
	PyObject *e = Sf_Entity(&f, G(WR, "waypointName"));
	if (Sf_True(&f, e) > 0) {
		Sf_Method(&f, e, "Del", "()");
	}
	PyObject *players = Sf_Build(&f, "[]");
	e = Os_First(&f, "Race");
	int mark = f.n;
	while (Sf_True(&f, e) > 0) {
		Sf_Method(&f, players, "append", "(O)", Sf_Attr(&f, e, "Name"));
		PyObject *next = Sf_Entity(&f, Sf_Attr(&f, e, "NextInList"));
		Sf_ReleaseKeep(&f, mark, &next, 1);
		e = next;
	}
	PyObject *pl;
	for (int k = 0; (pl = Sf_Next(&f, players, k)) != NULL; k++) {
		int m = f.n;
		e = Sf_Entity(&f, pl);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttr(&f, e, "MoveTime", Sf_Float(&f, 0.0));
			Sf_SetAttrS(&f, e, "TgType", "None");
			Sf_SetAttrS(&f, e, "EntityList", "Traffic");
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, e, "Name"), 1);
		}
		Sf_Release(&f, m);
	}
	Sf_SetGlobal(&f, WR, "BayScorer", Sf_Build(&f, "{}"));
	Sf_SetGlobal(&f, WR, "YourScore", Sf_Int(&f, -1));
	Sf_SetGlobal(&f, WR, "waypointName", Sf_Str(&f, ""));
	Sf_SetGlobal(&f, WR, "iStartPos", Sf_Int(&f, 0));
	Sf_Run(&f, "SInput", "Bind", "(isss)", 0, "Racer", "CM", "");
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsWin2ndRaceMethods[] = {
	{ "InitStatus", Win2nd_InitStatus, 1, NULL },
	{ "Init", Win2nd_Init, 1, NULL },
	{ "InitRace", Win2nd_InitRace, 1, NULL },
	{ "OnCounterMeasure", (PyCFunction)(void (*)(void))Win2nd_OnCounterMeasure, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "GetRaceWaypointIndex", Win2nd_GetRaceWaypointIndex, 1, NULL },
	{ "RacerDamaged", Win2nd_RacerDamaged, 1, NULL },
	{ "OnRaceStart", Win2nd_OnRaceStart, 1, NULL },
	{ "OnCheckPoint", Win2nd_OnCheckPoint, 1, NULL },
	{ "OnNearCheckPoint", Win2nd_OnNearCheckPoint, 1, NULL },
	{ "NameOponGoItem", Win2nd_NameOponGoItem, 1, NULL },
	{ "CancelMission", Win2nd_CancelMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_Demo_HumphreyRace: the race against Humphrey of the demo, then the call to the crazy gambler ---- */

/* InitStatus(LevelPath): the traffic; the challenge of the race or none */
static PyObject *Demo_InitStatus(PyObject *self, PyObject *args) {
	static const char *const race[] = { "Revenge.NumEnemies", "3", "Challenge.NumEnemies", "3", "Challenge.Type", "Race", "Challenge.Foe",
		"Humphrey", "Challenge.Money", "0", NULL };
	static const char *const none[] = { "Revenge.Type", "", "Revenge.Foe", "", "Revenge.NumEnemies", "", "Challenge.NumEnemies", "",
		"Challenge.Type", "", "Challenge.Foe", "", "Challenge.Money", "", "Race.Profile", "", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Init");
	Lv_AcShips(&f, DH, ships_demo);
	Sf_Import(&f, "MapRace");
	int racing = Sf_EqS(&f, SAVE("MissionState"), "HumphreyRace");
	if (racing > 0) {
		Lv_Saves(&f, race);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Race.Profile", G(DH, "RACE_PROFILE"));
		Sf_SetMod(&f, "MapRace", "Races", Sf_Build(&f, "[O]", Sf_Mod(&f, "MapRace", "EasyRaceOurskirts1")));
		Lv_Com(&f, "Mission_FirstCrazyBet_RemoteMessage5", "CrazyGambler", G(DH, "ShowSystemMessage"), NOARGS);
	} else if (racing == 0) {
		Lv_Saves(&f, none);
		Sf_SetMod(&f, "MapRace", "Races", Sf_Build(&f, "[]"));
	}
	return Sf_None(&f);
}

static PyObject *Demo_ShowSystemMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Lang(&f, Sf_Str(&f, "RaceSystemGetShip")));
	return Sf_None(&f);
}

/* Init(LevelPath): the gates closed, the metro to the press; the race hooks, the call or the metro by the state */
static PyObject *Demo_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25);
	Os_CloseDoors(&f);
	Os_Metro(&f, "Mission_Demo_HumphreyRace.OnMetroUsedToPress");
	PyObject *state = SAVE("MissionState");
	int racing = Sf_EqS(&f, state, "HumphreyRace");
	if (racing > 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "TimeToCancelCurrentMission", 0);
		Sf_SetMod(&f, "RaceMaker", "SkipTimer", Sf_Int(&f, 1));
		Sf_SetMod(&f, "RaceMaker", "OnPlayerWinsRace", G(DH, "OnPlayerWinsRace"));
		Sf_SetMod(&f, "RaceMaker", "OnPlayerLoosesRace", G(DH, "OnPlayerLoosesRace"));
		Sf_SetMod(&f, "RaceMaker", "OnPreInitRace", G(DH, "DelRacers"));
		Sf_SetMod(&f, "RaceMaker", "OnInitRace", G(DH, "SetMissionTarget"));
		Sf_SetMod(&f, "RaceMaker", "OnCheckPointCallback", G(DH, "ClosePressHangar"));
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "BadBoy0");
		Sf_SetGlobal(&f, DH, "prevSwitchVehicleMissionCallback", Sf_Mod(&f, "Parking", "SwitchVehicleMissionCallback"));
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(DH, "ShowBoostMessage"));
	} else if (racing == 0) {
		int call = Sf_EqS(&f, state, "CallCrazyGambler");
		if (call > 0) {
			Sf_Own(&f, DH, "CallCrazyGambler", "()");
		} else if (call == 0) {
			Sf_Own(&f, DH, "EnableMetro", "()");
		}
	}
	return Sf_None(&f);
}

/* ShowBoostMessage(): the first ship: the previous callback back, the message about the boost key */
static PyObject *Demo_ShowBoostMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "RaceMaker");
	if (Sf_True(&f, G(DH, "prevSwitchVehicleMissionCallback")) > 0) {
		Sf_Line(&f, G(DH, "prevSwitchVehicleMissionCallback"), "()");
	}
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(DH, "prevSwitchVehicleMissionCallback"));
	PyObject *fn = Sf_Mod(&f, "SScorer", "SetMsgText");
	PyObject *msg = Sf_Lang(&f, Sf_Str(&f, "Mission_FirstCrazyBet_SystemMessage4"));
	msg = Sf_Op(&f, msg, '%', Sf_Ask(&f, "SInput", "GetDefinedEntry", "(isss)", 0, "Racer", "Boost", "Mouse"));
	Sf_Line(&f, fn, "(OO)", msg, Sf_OpI(&f, Sf_Time(&f), '+', 10));
	return Sf_None(&f);
}

/* ClosePressHangar(): the first checkpoint: the hangar of the press closes */
static PyObject *Demo_ClosePressHangar(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	Sf_SetAttrS(&f, ENT("DM_Door_CityDoorA_01"), "OnObjSlot", "!CloseDoorSlot");
	Sf_SetMod(&f, "RaceMaker", "OnCheckPointCallback", Py_None);
	return Sf_None(&f);
}

/* DelRacers(names): the racers of the names that exist go */
static PyObject *Demo_DelRacers(PyObject *self, PyObject *args) {
	PyObject *names, *name;
	SF_ARGS("O", &names);
	for (int k = 0; (name = Sf_Next(&f, names, k)) != NULL; k++) {
		int mark = f.n;
		PyObject *e = Sf_Entity(&f, name);
		if (Sf_True(&f, e) > 0) {
			Sf_Method(&f, e, "Del", "()");
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* OnPlayerWinsRace(): no race hooks, the win saved, the SMerc1 ship and engine to edit, the gambler's message */
static PyObject *Demo_OnPlayerWinsRace(PyObject *self, PyObject *args) {
	static const char *const won[] = { "Stats.WinHumphreyRace", "1", "JackInTheBox.Active", "0", "MissionState", "CallCrazyGambler",
		"Revenge.Type", "", "Revenge.Foe", "", "Revenge.NumEnemies", "", NULL };
	static const char *const hooks[] = { "OnPlayerWinsRace", "OnPlayerLoosesRace", "OnPreInitRace", "OnInitRace", NULL };
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "Scorer");
	for (int i = 0; hooks[i]; i++) {
		Sf_SetMod(&f, "RaceMaker", hooks[i], Py_None);
	}
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Lv_Saves(&f, won);
	Lv_SaveAdd(&f, DH, "Hangar.shipsToEditList", "SMerc1");
	Lv_SaveAdd(&f, DH, "Hangar.availableEnginesList", "MMERC1");
	Lv_Com(&f, "Mission_FirstCrazyBet_RemoteMessage7", "CrazyGambler", G(DH, "CallCrazyGambler"), NOARGS);
	return Sf_None(&f);
}

/* CallCrazyGambler(): a phone cab as the target, Rusty 3 s later */
static PyObject *Demo_CallCrazyGambler(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PhoneCab(&f, "Mission_Demo_HumphreyRace.UsePhoneCab", 1, NULL);
	Sf_Later(&f, Sf_Int(&f, 3), G(DH, "RustyAppears"), NOARGS, NULL);
	return Sf_None(&f);
}

static PyObject *Demo_ShowRustyMapsSystemMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_MsgText(&f, Sf_Lang(&f, Sf_Str(&f, "Mission_FirstCrazyBet_SystemMessage3")));
	return Sf_None(&f);
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): the police warning while the alarm is on, else the boss */
static PyObject *Demo_UsePhoneCab(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	int alarm = Os_PhoneUse(&f, DH, usr, ch, opt, full);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", G(DH, "pc"), "Generics_StillPolice1_CrazyGambler", "CrazyGambler");
	} else if (alarm == 0) {
		Sf_SetAttrS(&f, G(DH, "pc"), "UseFunc", "PhoneCab.DefUseAudioCom");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(DH, "pc"), 0);
		Sf_Own(&f, DH, "BossAppears", "()");
	}
	return Sf_None(&f);
}

/* EnableMetro(): the metro to the end of the demo; the first time a waypoint and the temple on the map */
static PyObject *Demo_EnableMetro(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "MissionsFuncs");
	PyObject *e = Os_Metro(&f, "Mission_Demo_HumphreyRace.OnMetroUsed");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
	if (Sf_Compare(&f, SAVE("MissionState"), "!=", Sf_Str(&f, "EndOfDemo")) > 0) {
		SETSAVE("MissionState", "EndOfDemo");
		Sf_Run(&f, "SaveGame", "QWayPoint", "()");
		Sf_Run(&f, "SScorer", "Set", "(iOss)", 0, Sf_Ask(&f, "Scrap", "Get", "(s)", "MissionScorerName"), "ForceMap", "Temple");
		Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_FirstContactPress_MissionText3", 1);
	}
	return Sf_None(&f);
}

/* OnMetroUsed(UsrName, MetroName, optId, FullOption): the metro goes up to the end of the demo */
static PyObject *Demo_OnMetroUsed(PyObject *self, PyObject *args) {
	PyObject *usr, *metro, *opt, *full;
	SF_ARGS("OOOO", &usr, &metro, &opt, &full);
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "Metro");
	Sf_Run(&f, "SAct", "GetClass", "(O)", Sf_Attr(&f, Sf_Entity(&f, metro), "ActCtrl"));
	Sf_Run(&f, "SAct", "GetAction", "(s)", "Up");
	Sf_Run(&f, "SAct", "SetAct", "(ss)", "EventFunc", "Mission_Demo_HumphreyRace.OnEndOfDemo");
	Sf_Run(&f, "Elevator", "UseElevator", "(OOsO)", usr, metro, "MoveLevel Press", full);
	Sf_Run(&f, "Metro", "SetUsrParams", "(O)", usr);
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsDemoHumphreyRaceMethods[] = {
	{ "InitStatus", Demo_InitStatus, 1, NULL },
	{ "ShowSystemMessage", Demo_ShowSystemMessage, 1, NULL },
	{ "Init", Demo_Init, 1, NULL },
	{ "ShowBoostMessage", Demo_ShowBoostMessage, 1, NULL },
	{ "ClosePressHangar", Demo_ClosePressHangar, 1, NULL },
	{ "DelRacers", Demo_DelRacers, 1, NULL },
	{ "OnPlayerWinsRace", Demo_OnPlayerWinsRace, 1, NULL },
	{ "CallCrazyGambler", Demo_CallCrazyGambler, 1, NULL },
	{ "ShowRustyMapsSystemMessage", Demo_ShowRustyMapsSystemMessage, 1, NULL },
	{ "UsePhoneCab", Demo_UsePhoneCab, 1, NULL },
	{ "EnableMetro", Demo_EnableMetro, 1, NULL },
	{ "OnMetroUsed", Demo_OnMetroUsed, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GoToOutskirts: the crazy gambler calls until the player takes the elevator to GamblinDen ---- */

static PyObject *GoTo_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, GO, ships_all);
}

/* Init(LevelPath): the gates and the metro closed, the death hook, the gambler's first call */
static PyObject *GoTo_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Os_CloseDoors(&f);
	Sf_SetAttrI(&f, ENT("DM_Door_MetroExt_"), "ArrowCanUse", 0);
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", G(GO, "OnMainCharDeath"));
	int killed = Sf_EqS(&f, SAVE("PlayerHasBeenKilled"), "1");
	if (killed > 0) {
		SETSAVE("PlayerHasBeenKilled", "0");
		Lv_Com(&f, "Mission_DestroyPolice_RemoteMessage5", "Bishop", NULL, NULL);
	} else if (killed == 0 && Sf_EqI(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "isXbox"), 1) > 0) {
		Sf_Import(&f, "XBMenu");
		Sf_Run(&f, "XBMenu", "ShowPadWindow", "(i)", 0);
	}
	Lv_Com(&f, "Mission_TalkToFicus3_RemoteMessage1", "CrazyGambler", G(GO, "LaunchCrazyGamblerComunication"),
		Sf_Build(&f, "(O)", G(GO, "messageInterval")));
	return Sf_None(&f);
}

/* LaunchCrazyGamblerComunication(time): the first time the elevator to GamblinDen as the target; the next call later */
static PyObject *GoTo_LaunchCrazyGamblerComunication(PyObject *self, PyObject *args) {
	static const char *const cad = "Mission_FirstMeetCrazyGambler_MissionText1";
	PyObject *time;
	SF_ARGS("O", &time);
	if (Sf_EqI(&f, G(GO, "numCrazyGamblerComs"), 0) > 0) {
		PyObject *e = ENT("DM_Door_Elevator_GamblinDen");
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_GoToOutskirts.UseElevator");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
		if (Sf_Compare(&f, SAVE("PrimaryMissionDesc"), "!=", Sf_Str(&f, cad)) > 0) {
			Sf_Import(&f, "SScorer");
			Sf_Run(&f, "SScorer", "Set", "(iOss)", 0, Sf_Ask(&f, "Scrap", "Get", "(s)", "MissionScorerName"), "ForceMap", "GamblinDen");
			Sf_Run(&f, "SaveGame", "MMDesc", "(si)", cad, 1);
			Sf_Run(&f, "SaveGame", "QWayPoint", "()");
		}
	}
	Sf_Later(&f, time, G(GO, "PlayCrazyGamblerComunication"), NOARGS, NULL);
	return Sf_None(&f);
}

/* PlayCrazyGamblerComunication(): the second call, then the third one again and again */
static PyObject *GoTo_PlayCrazyGamblerComunication(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_Compare(&f, G(GO, "numCrazyGamblerComs"), "<", Sf_Int(&f, 2)) > 0) {
		Sf_SetGlobal(&f, GO, "numCrazyGamblerComs", Sf_OpI(&f, G(GO, "numCrazyGamblerComs"), '+', 1));
	}
	int second = Sf_EqI(&f, G(GO, "numCrazyGamblerComs"), 1);
	if (second >= 0) {
		Lv_Com(&f, second ? "Mission_TalkToFicus3_RemoteMessage2" : "Mission_TalkToFicus3_RemoteMessage3", "CrazyGambler",
			G(GO, "LaunchCrazyGamblerComunication"), Sf_Build(&f, "(O)", G(GO, "messageInterval")));
	}
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsGoToOutskirtsMethods[] = {
	{ "InitStatus", GoTo_InitStatus, 1, NULL },
	{ "Init", GoTo_Init, 1, NULL },
	{ "LaunchCrazyGamblerComunication", GoTo_LaunchCrazyGamblerComunication, 1, NULL },
	{ "PlayCrazyGamblerComunication", GoTo_PlayCrazyGamblerComunication, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_KillPolice: two police ships to kill for the crazy gambler, then Berto and the metro to the press ---- */

/* InitStatus(LevelPath): no alarm, the traffic (fewer ships in the demo) */
static PyObject *Police_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	SETSAVE("AlarmActive", "0");
	SETSAVE("AlarmStatus", "0");
	Sf_Import(&f, "Init");
	int demo = Sf_True(&f, Sf_Mod(&f, "Init", "isDemo"));
	if (demo >= 0) {
		Lv_AcShips(&f, KP, demo ? ships_demo : ships_police5);
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the gates and the metro closed, the death hook, the mission or Berto by the state */
static PyObject *Police_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Os_CloseDoors(&f);
	Sf_SetAttrI(&f, ENT("DM_Door_MetroExt_"), "ArrowCanUse", 0);
	Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25);
	Sf_Import(&f, "Chars");
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", G(KP, "OnMainCharDeath"));
	if (Sf_EqS(&f, SAVE("PlayerHasBeenKilled"), "1") > 0) {
		SETSAVE("PlayerHasBeenKilled", "0");
		Lv_Com(&f, "Mission_DestroyPolice_RemoteMessage5", "Bishop", NULL, NULL);
	}
	int kill = Sf_EqS(&f, SAVE("MissionState"), "KillPolice");
	if (kill >= 0) {
		Sf_Own(&f, KP, kill ? "InitMission" : "BertoAppears2", "()");
	}
	return Sf_None(&f);
}

static PyObject *Police_SetPos(PyObject *self, PyObject *args) {
	PyObject *e, *pos;
	SF_ARGS("OO", &e, &pos);
	Sf_SetAttr(&f, e, "Pos", pos);
	return Sf_None(&f);
}

/* one of the two police ships of the mission (the entity of name): moved, red, its death counted */
static PyObject *Kp_Cop(scriptFrame_t *f, PyObject *name, int x, int y, int z) {
	PyObject *e = Sf_Entity(f, name);
	if (Sf_True(f, e) == 0) {
		Sf_RaiseText(f, "Tiene que haber al menos dos policias");
	}
	Sf_Schedule(f, Sf_Int(f, 3), Sf_Global(f, KP, "SetPos"), Sf_Build(f, "(O(iii))", e, x, y, z), NULL);
	Sf_SetAttrS(f, e, "TgType", "red");
	Sf_SetAttrS(f, e, "OnDeath", "Mission_KillPolice.OnPoliceDeath");
	return e;
}

/* InitMission(): the first two police ships as the targets, the alarm hook, the gambler's message */
static PyObject *Police_InitMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Alarm");
	Sf_Import(&f, "OutMap");
	PyObject *cop1 = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "OutCop", "First");
	PyObject *e = Kp_Cop(&f, cop1, -311955, 56409, -135709);
	PyObject *cop2 = Sf_Attr(&f, e, "NextInList");
	Kp_Cop(&f, cop2, -333879, 56196, -121097);
	Sf_Method(&f, G(KP, "policesList"), "append", "(O)", cop1);
	Sf_Method(&f, G(KP, "policesList"), "append", "(O)", cop2);
	Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", G(KP, "policesList"));
	Sf_Method(&f, Sf_Mod(&f, "Alarm", "SetAlarmCallback"), "append", "(O)", G(KP, "OnAlarm"));
	Lv_Com(&f, "Mission_DestroyPolice_RemoteMessage1", "CrazyGambler", G(KP, "ShowMessage2"), NOARGS);
	return Sf_None(&f);
}

/* OnAlarm(status): the alarm does not fall while the mission runs */
static PyObject *Police_OnAlarm(PyObject *self, PyObject *args) {
	PyObject *status;
	SF_ARGS("O", &status);
	Sf_SetGlobal(&f, KP, "alarmFallDelta", Sf_Ask(&f, "Scrap", "Get", "(s)", "AlarmFallDelta"));
	Sf_Run(&f, "Scrap", "Set", "(si)", "AlarmFallDelta", 0);
	return Sf_None(&f);
}

/* OnPoliceDeath(LooserName, WinnerName): one police ship of the mission fewer; both: the alarm falls again */
static PyObject *Police_OnPoliceDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "Speech");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	if (Sf_In(&f, looser, G(KP, "policesList")) > 0) {
		Sf_Method(&f, G(KP, "policesList"), "remove", "(O)", looser);
		Sf_SetGlobal(&f, KP, "policesKilled", Sf_OpI(&f, G(KP, "policesKilled"), '+', 1));
	}
	int one = Sf_EqI(&f, G(KP, "policesKilled"), 1);
	if (one > 0) {
		Sf_Run(&f, "Speech", "RemoteComunication", "(ss)", "Mission_DestroyPolice_RemoteMessage3", "CrazyGambler");
	} else if (one == 0 && Sf_EqI(&f, G(KP, "policesKilled"), 2) > 0) {
		Sf_Run(&f, "Scrap", "Set", "(sO)", "AlarmFallDelta", G(KP, "alarmFallDelta"));
		Lv_Com(&f, "Mission_DestroyPolice_RemoteMessage4", "CrazyGambler", G(KP, "BertoAppears"), NOARGS);
	}
	return Sf_None(&f);
}

/* BertoAppears(): the blueprints of the Swarm missiles, Rusty's message, a waypoint, then Berto */
static PyObject *Police_BertoAppears(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Lang(&f, Sf_Str(&f, "Mission_DestroyPolice_SystemMessage1")));
	PyObject *l = Lv_SaveList(&f, "Hangar.availableWeaponsList", KP);
	Lv_SaveAppend(&f, "Hangar.availableWeaponsList", l, Sf_Str(&f, "Swarm"));
	Sf_Run(&f, "Scrap", "IncSaveVar", "(s)", "NewBluePrintAvaliable");
	Lv_Com(&f, "Mission_DestroyPolice_RemoteMessage7", "Rusty", NULL, NULL);
	SETSAVE("MissionState", "ListenToBerto");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Own(&f, KP, "BertoAppears2", "()");
	return Sf_None(&f);
}

/* EnableMetro(): the metro to the press; after Berto's message the press on the map and the parked ships */
static PyObject *Police_EnableMetro(PyObject *self, PyObject *args) {
	static const char *const parked[] = { "Parked.Police", "['SPOLI1', 'SPOLI2']", "Parked.Mercs", "['SMERC1']", "Parked.TownHall",
		"['SPOLI1', 'SPOLI2']", "Parked.Bank", "['SPOLI2']", "Parked.Press", "['SPOLI1']", "Parked.Temple", "['SPOLI2']", NULL };
	SF_ARGS("");
	Sf_Import(&f, "MissionsFuncs");
	PyObject *e = Os_Metro(&f, "Mission_KillPolice.OnMetroUsed");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
	if (Sf_EqS(&f, SAVE("MissionState"), "ListenToBerto") > 0) {
		Sf_Run(&f, "SaveGame", "QWayPoint", "()");
		Sf_Run(&f, "SScorer", "Set", "(iOss)", 0, Sf_Ask(&f, "Scrap", "Get", "(s)", "MissionScorerName"), "ForceMap", "Press");
		Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_FirstContactPress_MissionText1", 1);
		SETSAVE("MissionState", "");
		Lv_Saves(&f, parked);
	}
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsKillPoliceMethods[] = {
	{ "InitStatus", Police_InitStatus, 1, NULL },
	{ "Init", Police_Init, 1, NULL },
	{ "SetPos", Police_SetPos, 1, NULL },
	{ "InitMission", Police_InitMission, 1, NULL },
	{ "OnAlarm", Police_OnAlarm, 1, NULL },
	{ "OnPoliceDeath", Police_OnPoliceDeath, 1, NULL },
	{ "BertoAppears", Police_BertoAppears, 1, NULL },
	{ "EnableMetro", Police_EnableMetro, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_TransportKiller: the transport that only the ATPC hurts, its data pack, then the police boss ---- */

static PyObject *Transport_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, TK, ships_police5);
}

static PyObject *Transport_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	PyObject *sv = SAVE("Mission_TransportKiller");
	int begin = Sf_EqS(&f, sv, "Begin");
	if (begin > 0) {
		Sf_Own(&f, TK, "InitTransport", "()");
	} else if (begin == 0 && Sf_EqS(&f, sv, "InitCall") > 0) {
		Sf_Own(&f, TK, "InitCall", "()");
	}
	return Sf_None(&f);
}

/* InitTransport(): the transport on a traffic path, red, hurt only by the right weapon */
static PyObject *Transport_InitTransport(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Traffic", "SetPath", "()");
	Sf_SetGlobal(&f, TK, "trans", Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ss)", "TLight2", "Transporter"));
	Sf_Run(&f, "Traffic", "ResetPath", "()");
	PyObject *t = G(TK, "trans");
	Sf_SetAttrS(&f, t, "TgType", "red");
	Sf_SetAttrI(&f, t, "Money", -1);
	Lv_Life(&f, t, TK, "TRANSPORT_LIFE");
	Sf_SetAttrI(&f, t, "Traf_Active", 1);
	Sf_SetAttr(&f, t, "Traf_Begin", Sf_Attr(&f, t, "Pos"));
	Sf_SetAttr(&f, t, "Traf_End", Sf_Attr(&f, t, "Pos"));
	Sf_SetAttrI(&f, t, "Traf_Speed", 100 * 28 + 2000);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, t, "Name"));
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOidd)", 3, Sf_Attr(&f, t, "Name"), 0, 2.0, 0.025);
	Sf_SetAttrS(&f, t, "OnDamage", "Mission_TransportKiller.TryToDamage");
	Sf_SetAttrS(&f, t, "OnDeath", "Mission_TransportKiller.TryToDeath");
	Sf_Run(&f, "Scrap", "Preload3DObject", "(Oiii)", G(TK, "Model_Datapack"), 1, 1, 1);
	return Sf_None(&f);
}

/* isValidDamage(KillerName): (usr != None) and (usr.Name == KillerName) and (usr.iWeap == 4), as the int of the
 * first false part */
static PyObject *Transport_isValidDamage(PyObject *self, PyObject *args) {
	PyObject *killer;
	SF_ARGS("O", &killer);
	PyObject *usr = USR();
	int v = Sf_Compare(&f, usr, "!=", Py_None);
	if (v > 0) {
		v = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", killer);
	}
	if (v > 0) {
		v = Sf_EqI(&f, Sf_Attr(&f, usr, "iWeap"), 4);
	}
	return Sf_Return(&f, v < 0 ? NULL : Sf_Int(&f, v));
}

/* TryToDeath(EntityName, KillerName): the alarm, the data pack and its arrow where the transport died */
static PyObject *Transport_TryToDeath(PyObject *self, PyObject *args) {
	PyObject *name, *killer;
	SF_ARGS("OO", &name, &killer);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", name, killer);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	PyObject *pos = Sf_Attr(&f, Sf_Entity(&f, name), "Pos");
	PyObject *l = Lv_CreateAt(&f, Sf_Str(&f, "MissionDataPack"), pos, "Item");
	Sf_Method(&f, l, "SetModel", "(O)", G(TK, "Model_Datapack"));
	Sf_SetAttrI(&f, l, "Radius", 3500);
	Sf_SetAttrI(&f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(&f, l, "AutoFace", 0);
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
	Sf_SetAttrS(&f, l, "OnTake", "Mission_TransportKiller.TakeDataPack");
	Sf_Import(&f, "Items");
	Sf_Run(&f, "Items", "SetSound", "(Os)", l, "TakeBDataPack");
	PyObject *l2 = Lv_CreateAt(&f, Sf_Str(&f, "MissionCheckArrow"), pos, "WithLife");
	Sf_SetAttr(&f, l2, "CamTgt", Sf_Build(&f, "(iii)", 0, 1000, 0));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, l2, "Name"));
	return Sf_None(&f);
}

/* TakeDataPack(ItemName, UsrName): the data pack and its arrow go, the police boss calls */
static PyObject *Transport_TakeDataPack(PyObject *self, PyObject *args) {
	PyObject *item, *usr;
	SF_ARGS("OO", &item, &usr);
	PyObject *it = ENT("MissionDataPack");
	Sf_SetAttrI(&f, it, "rTime", 20);
	Sf_Later(&f, Sf_Int(&f, 5), Sf_Attr(&f, it, "Del"), NOARGS, NULL);
	Sf_Method(&f, ENT("MissionCheckArrow"), "Del", "()");
	Lv_Com(&f, "Mission_InvulnerableTransport_RemoteMessage4", "PoliceBoss", G(TK, "InitCall"), NOARGS);
	return Sf_None(&f);
}

/* TryToDamage(EntityName, KillerName, Damage): the first hit sets off the alarm; a hit by the wrong weapon is healed
 * and DeepThroat says what is missing, at most every 20 s */
static PyObject *Transport_TryToDamage(PyObject *self, PyObject *args) {
	PyObject *name, *killer, *damage;
	SF_ARGS("OOO", &name, &killer, &damage);
	int alarm = Lv_Alarm(&f);
	if (alarm == 0) {
		Lv_Com(&f, "Mission_InvulnerableTransport_RemoteMessage5", "DeepThroat", NULL, NULL);
		Sf_Import(&f, "OutPolice");
		Sf_SetMod(&f, "OutPolice", "iPoliceWave", Sf_Int(&f, 9));
		Sf_SetMod(&f, "OutPolice", "skipPrice", Sf_Int(&f, 1));
		PyObject *cop = Os_First(&f, "OutCop");
		int mark = f.n;
		while (Sf_True(&f, cop) > 0) {
			Sf_Method(&f, cop, "Del", "()");
			PyObject *next = Sf_Entity(&f, Sf_Attr(&f, cop, "NextInList"));
			Sf_ReleaseKeep(&f, mark, &next, 1);
			cop = next;
		}
		Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	}
	if (Sf_True(&f, Sf_Own(&f, TK, "isValidDamage", "(O)", killer)) != 0) {
		return Sf_None(&f);
	}
	PyObject *shp = Sf_Entity(&f, name);
	Sf_SetAttr(&f, shp, "Life", Sf_Op(&f, Sf_Attr(&f, shp, "Life"), '+', damage));
	PyObject *usr = USR();
	int his = Sf_Compare(&f, usr, "!=", Py_None);
	if (his > 0) {
		his = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", killer);
	}
	if (his > 0) {
		PyObject *t = Sf_Time(&f);
		if (Sf_Compare(&f, t, ">", G(TK, "LastTimeMsg")) > 0) {
			Sf_SetGlobal(&f, TK, "LastTimeMsg", Sf_OpI(&f, t, '+', 20));
			int atpc = Sf_True(&f, Sf_Attr(&f, usr, "isWpn04"));
			if (atpc == 0) {
				Lv_Com(&f, "Mission_InvulnerableTransport_RemoteMessage2", "DeepThroat", NULL, NULL);
			} else if (atpc > 0) {
				int ammo = Sf_Compare(&f, Sf_Attr(&f, usr, "Ammo02"), "<=", Sf_Float(&f, 0.5));
				if (ammo >= 0) {
					Lv_Com(&f, ammo ? "Mission_InvulnerableTransport_RemoteMessage3" : "Mission_InvulnerableTransport_RemoteMessage1",
						"DeepThroat", NULL, NULL);
				}
			}
		}
	}
	return Sf_None(&f);
}

/* InitCall(): the call to the police boss from a phone cab */
static PyObject *Transport_InitCall(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "OutPolice");
	SETSAVE("Mission_TransportKiller", "InitCall");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_Get4thClue_MissionText1", 1);
	Sf_SetMod(&f, "OutPolice", "iPoliceWave", Sf_Int(&f, 0));
	Lv_PhoneCab(&f, "Mission_TransportKiller.UsePhoneCab", 0, TK);
	return Sf_None(&f);
}

/* SetNextMission(): the cab free again, on to NextMissionData */
static PyObject *Transport_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PcDefault(&f, TK);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "Mission_TransportKiller");
	PyObject *fn = Sf_Mod(&f, "MissionsFuncs", "SetNextMission");
	Sf_Run(&f, "__builtin__", "apply", "(OO)", fn, G(TK, "NextMissionData"));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetReputation_MissionText1", 1);
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsTransportKillerMethods[] = {
	{ "InitStatus", Transport_InitStatus, 1, NULL },
	{ "Init", Transport_Init, 1, NULL },
	{ "InitTransport", Transport_InitTransport, 1, NULL },
	{ "isValidDamage", Transport_isValidDamage, 1, NULL },
	{ "TryToDeath", Transport_TryToDeath, 1, NULL },
	{ "TakeDataPack", Transport_TakeDataPack, 1, NULL },
	{ "TryToDamage", Transport_TryToDamage, 1, NULL },
	{ "InitCall", Transport_InitCall, 1, NULL },
	{ "SetNextMission", Transport_SetNextMission, 1, NULL },
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

/* CreateHumphrey(): Humphrey near the player unless he is there; the first time quiet, later attacking; taunts */
static PyObject *Humphrey_CreateHumphrey(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *h = Sf_Entity(&f, G(HU, "humphreyName"));
	if (Sf_True(&f, h) != 0) {
		return Sf_None(&f);
	}
	PyObject *spw = Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "(O)", Sf_Attr(&f, usr, "Pos"));
	h = Lv_CreateAt(&f, Sf_Str(&f, "Humphrey"), spw, "Car");
	Sf_SetAttrI(&f, h, "LaunchTaunts", 0);
	Lv_StdAI(&f, h, Sf_Own(&f, HU, "GetHumphreyProfile", "()"));
	Sf_SetAttrS(&f, h, "OnDeath", "Mission_DestroyHumphrey.OnHumphreyDeath");
	Lv_Life(&f, h, HU, "HUMPHREY_LIFE");
	Sf_SetGlobal(&f, HU, "humphreyName", Sf_Attr(&f, h, "Name"));
	int first = Sf_EqI(&f, G(HU, "numTaunt"), -1);
	if (first > 0) {
		Sf_SetGlobal(&f, HU, "numTaunt", Sf_Int(&f, 0));
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, G(HU, "humphreyName"), 0);
	} else if (first == 0) {
		Sf_SetAttrS(&f, h, "TgType", "red");
		Sf_SetAttrS(&f, h, "EntityList", "Enemies");
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(HU, "humphreyName"));
	}
	Sf_Later(&f, Sf_Int(&f, 35), G(HU, "LaunchTaunt"), NOARGS, "LaunchTaunt");
	return Sf_None(&f);
}

/* HumphreyAttacks(): Humphrey red and the target, the scorer of his lives */
static PyObject *Humphrey_HumphreyAttacks(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *h = Sf_Entity(&f, G(HU, "humphreyName"));
	if (Sf_True(&f, h) > 0) {
		Sf_SetAttrS(&f, h, "TgType", "red");
		Sf_SetAttrS(&f, h, "EntityList", "Enemies");
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(HU, "humphreyName"));
	}
	Lv_ScorerCall(&f, G(HU, "MissionScorer"));
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* LaunchTaunt(): one of three taunts, the next one 20 s later */
static PyObject *Humphrey_LaunchTaunt(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *key = Sf_Cat(&f, "sO", "Mission_DestroyHumphrey_RemoteMessage", Sf_Repr(&f, Sf_Op(&f, Sf_Int(&f, 3), '+', G(HU, "numTaunt"))));
	Sf_Run(&f, "Speech", "AddRemoteCom", "(Os)", key, "EvilHumphrey");
	Sf_SetGlobal(&f, HU, "numTaunt", Sf_Op(&f, Sf_OpI(&f, G(HU, "numTaunt"), '+', 1), '%', Sf_Int(&f, 3)));
	Sf_Later(&f, Sf_Int(&f, 20), G(HU, "LaunchTaunt"), NOARGS, "LaunchTaunt");
	return Sf_None(&f);
}

/* OnHumphreyDeath(LooserName, WinnerName): one life fewer; the last one: Betty, the noise, the boss; else he comes
 * back 3 s later */
static PyObject *Humphrey_OnHumphreyDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "SScorer", "CancelSpeech", "()");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "LaunchTaunt");
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumHumphreyLifes")), '-', 1);
	int neg = Sf_Compare(&f, n, "<", Sf_Int(&f, 0));
	if (neg > 0) {
		n = Sf_Int(&f, 0);
		SETSAVE("NumHumphreyLifes", "0");
	} else if (neg == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumHumphreyLifes", Sf_Repr(&f, n));
	}
	int last = Sf_EqI(&f, n, 0);
	if (last > 0) {
		static const char *const msg = "Mission_DestroyHumphrey_RemoteMessage6";
		Lv_ScorerCall(&f, Py_None);
		PyObject *t = Sf_OpI(&f, Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", Sf_Lang(&f, Sf_Str(&f, msg))), '-', 2);
		PyObject *at = Sf_Op(&f, Sf_Time(&f), '+', t);
		Sf_Schedule(&f, at, Sf_Mod(&f, "SScorer", "Set"), Sf_Build(&f, "(issi)", 0, "Monitor", "Noise", 1), NULL);
		Lv_Com(&f, msg, "Betty", G(HU, "BossAppears"), NOARGS);
	} else if (last == 0) {
		Sf_Later(&f, Sf_Int(&f, 3), G(HU, "CreateHumphrey"), NOARGS, NULL);
	}
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* MissionScorer(id): Humphrey's lives left */
static PyObject *Humphrey_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Sf_MissionText(&f, id, "RemainingLifesText", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 0), "Horatio", "Shadow",
		Lv_Label(&f, "Mission_DestroyHumphrey_Scorer1"), "Left", NULL);
	Sf_MissionText(&f, id, "RemainingLifes", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 20), "Horatio", "Shadow", SAVE("NumHumphreyLifes"), "Left",
		NULL);
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsDestroyHumphreyMethods[] = {
	{ "GetHumphreyProfile", Humphrey_GetHumphreyProfile, 1, NULL },
	{ "InitStatus", Humphrey_InitStatus, 1, NULL },
	{ "CreateHumphrey", Humphrey_CreateHumphrey, 1, NULL },
	{ "HumphreyAttacks", Humphrey_HumphreyAttacks, 1, NULL },
	{ "LaunchTaunt", Humphrey_LaunchTaunt, 1, NULL },
	{ "OnHumphreyDeath", Humphrey_OnHumphreyDeath, 1, NULL },
	{ "MissionScorer", Humphrey_MissionScorer, 1, NULL },
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
static PyObject *Merc_CreateLonelyMercProfiles(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *pd2 = Lv_Profile(&f, IM, lonely_normal_profile);
	Sf_SetItem(&f, Sf_Mod(&f, "VehiclesNPC", "AIProf"), Sf_Str(&f, "LonelyMercNormal"), pd2);
	PyObject *pd1 = Sf_Ask(&f, "VehiclesNPC", "cShipAI", "()");
	Sf_SetAttrs(&f, pd1, IM, lonely_hard_profile);
	Sf_SetItem(&f, Sf_Mod(&f, "VehiclesNPC", "AIProf"), Sf_Str(&f, "LonelyMercHard"), pd1);
	return Sf_None(&f);
}

/* InitStatus(LevelPath): no infinite lives, the traffic, the player comes out of the elevator of GamblinDen */
static PyObject *Merc_InitStatus(PyObject *self, PyObject *args) {
	static const char *const from[] = { "ComeFrom", "DoorElevator", "PrevMap", "Levels/GamblinDen", "Spawn", "DM_Player_Spawn_GamblinDen", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Traffic");
	SETSAVE("Player.InfiniteLives", "0");
	Lv_AcShips(&f, IM, ships_police5);
	Lv_Saves(&f, from);
	return Sf_None(&f);
}

/* the boarding callbacks of the mission and the profiles of the mercenary */
static byte Im_Setup(scriptFrame_t *f) {
	Sf_Import(f, "Parking");
	Sf_SetMod(f, "Parking", "SwitchVehicleMissionCallback", Sf_Global(f, IM, "InitMission"));
	Sf_SetMod(f, "Parking", "SwitchCharMissionCallback", Sf_Global(f, IM, "StopMission"));
	Sf_Own(f, IM, "CreateLonelyMercProfiles", "()");
	return !f->failed;
}

/* Init(LevelPath): the scene when the elevator is down, the mission after the scene, else the phone cab */
static PyObject *Merc_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	PyObject *state = SAVE("MissionState");
	int first = Sf_EqS(&f, state, "IdentifyMerc");
	if (first > 0) {
		Im_Setup(&f);
		Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", G(IM, "InitIdentifyMerc"));
	} else if (first == 0) {
		int second = Sf_EqS(&f, state, "IdentifyMerc2");
		if (second > 0) {
			Im_Setup(&f);
			Lv_Com(&f, "Mission_MeetingWithMrX_RemoteMessage2", "DeepThroat", G(IM, "PreInitMission"), NULL);
		} else if (second == 0) {
			Sf_Own(&f, IM, "SetPhoneCab", "()");
		}
	}
	return Sf_None(&f);
}

/* the camera of the player behind or in front of target: CamDist, CamRot, CamAng, no turn */
static byte Im_Camera(scriptFrame_t *f, long dist, long rot, PyObject *target) {
	int mark = f->n;
	PyObject *cam = Sf_Entity(f, Sf_Ask(f, "Scrap", "GetCam", "(i)", 0));
	Sf_SetAttrI(f, cam, "CamDist", dist);
	Sf_SetAttrI(f, cam, "CamRot", rot);
	Sf_SetAttrI(f, cam, "CamAng", -rot);
	Sf_SetAttrI(f, cam, "AddTurn", 0);
	Sf_SetAttr(f, cam, "Target", target);
	Sf_Release(f, mark);
	return !f->failed;
}

/* InitIdentifyMerc(elevator=None): the scene: the player out of the elevator, Traitorius waiting in front of him */
static PyObject *Merc_InitIdentifyMerc(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "elevator", NULL };
	PyObject *elevator = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", kwlist, &elevator)) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", Py_None);
	PyObject *usr = USR();
	Sf_SetAttr(&f, usr, "Pos", Sf_Build(&f, "(ddd)", -258322.203125, 65950.4609375, -194309.25));
	Sf_SetAttr(&f, usr, "Ang", Sf_Build(&f, "(ddd)", 0.0, -142.108001709, 0.0));
	Sf_SetAttr(&f, usr, "ViewAng", Sf_Build(&f, "(ddd)", 3.38299465179, -142.108016968, 0.0));
	Sf_Run(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, usr, "Name"));
	Sf_Run(&f, "Scene", "InitOutDoorScene", "(i)", 0);
	Sf_SetGlobal(&f, IM, "traitorius", Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ss)", "SFunc1", "Traitorius"));
	PyObject *t = G(IM, "traitorius");
	Sf_SetAttr(&f, t, "Pos", Sf_Build(&f, "(ddd)", -252843.578125, 67933.2734375, -187826.109375));
	Sf_SetAttrI(&f, t, "MaxLife", 1);
	Sf_SetAttrI(&f, t, "Life", 1);
	Sf_SetAttrI(&f, t, "Money", -1);
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iO)", 0, Sf_Attr(&f, t, "Name"));
	Sf_Run(&f, "RaceMaker", "PointObjectTo", "(OO)", t, Sf_Attr(&f, USR(), "Pos"));
	Im_Camera(&f, 1000, 20, Sf_Attr(&f, t, "Name"));
	Sf_Run(&f, "Scene", "SetText", "(si)", "Mission_MeetingWithMrX_RemoteMessage1", 0);
	Sf_Later(&f, Sf_Float(&f, 3.0), G(IM, "CreateLonelyMercInScene"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CreateLonelyMercInScene(): the mercenary comes for Traitorius, the camera on him */
static PyObject *Merc_CreateLonelyMercInScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	Sf_SetGlobal(&f, IM, "lonelyMerc", Sf_Own(&f, IM, "CreateLonelyMerc", "()"));
	PyObject *m = G(IM, "lonelyMerc");
	Sf_SetAttr(&f, m, "Pos", Sf_Build(&f, "(ddd)", -232476.484375, 68936.765625, -144671.078125));
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, m, "Name"), Sf_Attr(&f, G(IM, "traitorius"), "Name"));
	Sf_Run(&f, "RaceMaker", "PointObjectTo", "(OO)", m, Sf_Attr(&f, G(IM, "traitorius"), "Pos"));
	Im_Camera(&f, 1500, 10, Sf_Attr(&f, m, "Name"));
	Sf_Later(&f, Sf_Float(&f, 3.0), G(IM, "CheckTraitoriusDeath"), NOARGS, NULL);
	Sf_Later(&f, Sf_Int(&f, 2), G(IM, "EndOfScene"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CheckTraitoriusDeath(): Traitorius dies if the mercenary missed */
static PyObject *Merc_CheckTraitoriusDeath(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, IM, "traitorius", ENT("Traitorius"));
	PyObject *t = G(IM, "traitorius");
	if (Sf_True(&f, t) > 0) {
		Sf_SetAttrI(&f, t, "Life", 0);
	}
	return Sf_None(&f);
}

/* EndOfScene(): the scene ends, the mercenary goes (created again later), DeepThroat's message */
static PyObject *Merc_EndOfScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scene", "EndOutDoorScene", "()");
	SETSAVE("MissionState", "IdentifyMerc2");
	Sf_Later(&f, Sf_Float(&f, 0.5), G(IM, "DelLonelyMerc"), NOARGS, NULL);
	Sf_Later(&f, Sf_Int(&f, 2), Sf_Mod(&f, "Speech", "AddRemoteCom"), Sf_Build(&f, "(ssO)", "Mission_MeetingWithMrX_RemoteMessage2", "DeepThroat",
		G(IM, "PreInitMission")), NULL);
	return Sf_None(&f);
}

/* CreateLonelyMerc(difficulty="Hard"): the mercenary; hard: only one missile (his weapons kept for later); normal:
 * the weapons kept and the life of the mission */
static PyObject *Merc_CreateLonelyMerc(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "difficulty", NULL };
	PyObject *difficulty = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", kwlist, &difficulty)) {
		return NULL;
	}
	if (!difficulty) {
		difficulty = Sf_Str(&f, "Hard");
	}
	int normal = Sf_EqS(&f, difficulty, "Normal");
	PyObject *m = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ss)", normal > 0 ? "LonelyMercNormal" : "LonelyMercHard", "LonelyMerc");
	Sf_SetGlobal(&f, IM, "lonelyMerc", m);
	m = G(IM, "lonelyMerc");
	Sf_SetAttrS(&f, m, "OnDeath", "Mission_IdentifyMerc.OnLonelyMercDeath");
	Sf_SetAttrS(&f, m, "OnDamage", "Mission_IdentifyMerc.OnLonelyMercDamage");
	Sf_SetAttrI(&f, m, "LaunchTaunts", 0);
	Sf_SetAttrI(&f, m, "Money", 300);
	normal = Sf_EqS(&f, difficulty, "Normal");
	if (normal > 0) {
		Sf_SetAttr(&f, m, "WeapList", G(IM, "prevWeapList"));
		Sf_SetAttr(&f, m, "Ammo01", G(IM, "prevAmmo"));
		Lv_Life(&f, m, IM, "LONELY_MERC_LIFE");
	} else if (normal == 0) {
		Sf_SetGlobal(&f, IM, "prevWeapList", Sf_Attr(&f, m, "WeapList"));
		Sf_SetGlobal(&f, IM, "prevAmmo", Sf_Attr(&f, m, "Ammo01"));
		Sf_SetAttrS(&f, m, "WeapList", "0,15,0,0,0,0");
	}
	Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", Sf_Attr(&f, m, "Name"));
	return Sf_Return(&f, G(IM, "lonelyMerc"));
}

/* InitMission(): in a ship: the mercenary as the target, the scorer, the scan every second */
static PyObject *Merc_InitMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, G(IM, "lonelyMerc")) == 0) {
		Sf_SetGlobal(&f, IM, "lonelyMerc", Sf_Own(&f, IM, "CreateLonelyMerc", "(s)", "Normal"));
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, G(IM, "lonelyMerc"), "Name"), 0);
	}
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(IM, "lonelyMerc"), "Name"));
	Lv_ScorerCall(&f, G(IM, "MissionScorer"));
	Sf_DrawScorer(&f);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnFinishAutoUpdate", "Mission_IdentifyMerc.OnFinishScan");
	Sf_Later(&f, Sf_Float(&f, 1.0), G(IM, "ScanShip"), NOARGS, "ScanShip");
	return Sf_None(&f);
}

/* ScanShip(): every second: the scan runs while the mercenary is near; lost too long, it starts again; halfway he
 * flees */
static PyObject *Merc_ScanShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *mpos = Sf_Attr(&f, G(IM, "lonelyMerc"), "Pos");
	if (Sf_Except(&f)) {
		Sf_Later(&f, Sf_Float(&f, 1.0), G(IM, "ScanShip"), NOARGS, "ScanShip");
		return Sf_None(&f);
	}
	PyObject *d = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", mpos, Sf_Attr(&f, usr, "Pos")));
	PyObject *max = G(IM, "MAX_SCANNING_DIST");
	max = Sf_Op(&f, max, '*', G(IM, "MAX_SCANNING_DIST"));
	int scanning = Sf_True(&f, G(IM, "scanningLonelyMerc"));
	if (scanning > 0) {
		PyObject *state = Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", Sf_Attr(&f, G(IM, "lonelyMerc"), "Name"));
		int lost = Sf_Compare(&f, d, ">", max);
		if (lost > 0) {
			Sf_SetGlobal(&f, IM, "scanningLonelyMerc", Sf_Int(&f, 0));
			Lv_Set0(&f, "ScanRemainingTime", "Blink", Sf_Int(&f, 1));
			Lv_Set0(&f, "ScanRemainingTime", "IsAutoUpdatable", Sf_Int(&f, 0));
			if (Sf_Compare(&f, state, "!=", Sf_Int(&f, 2)) > 0) {
				Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, G(IM, "lonelyMerc"), "Name"), 0);
			}
			Sf_Later(&f, G(IM, "MAX_TIME_TARGET_LOST"), G(IM, "ResetScan"), NOARGS, "ResetScan");
		} else if (lost == 0) {
			PyObject *v = Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "ScanRemainingTime", "Value");
			if (Sf_Compare(&f, v, "<", Sf_OpI(&f, G(IM, "MAX_SCANNING_TIME"), '/', 2)) > 0) {
				int run = Sf_Compare(&f, state, "!=", Sf_Int(&f, 2));
				if (run > 0) {
					run = Sf_Compare(&f, state, "!=", Sf_Int(&f, 5));
				}
				if (run > 0) {
					Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOi)", 5, Sf_Attr(&f, G(IM, "lonelyMerc"), "Name"), Sf_Attr(&f, usr, "Name"), 100000);
				}
			}
		}
	} else if (scanning == 0) {
		if (Sf_Compare(&f, d, "<", max) > 0) {
			Sf_SetGlobal(&f, IM, "scanningLonelyMerc", Sf_Int(&f, 1));
			Lv_Set0(&f, "ScanRemainingTime", "Blink", Sf_Int(&f, 0));
			Lv_Set0(&f, "ScanRemainingTime", "IsAutoUpdatable", Sf_Int(&f, 1));
			Lv_Set0(&f, "ScanStatusText", "Visible", Sf_Int(&f, 1));
			Lv_Set0(&f, "ScanRemainingTime", "Visible", Sf_Int(&f, 1));
			Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ResetScan");
		}
	}
	Sf_Later(&f, Sf_Float(&f, 1.0), G(IM, "ScanShip"), NOARGS, "ScanShip");
	return Sf_None(&f);
}

/* OnFinishScan(): the scan bar ran out: identified, DeepThroat calls, no more scan */
static PyObject *Merc_OnFinishScan(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
	Lv_Set0(&f, "ScanStatusText", "Visible", Sf_Int(&f, 0));
	Lv_Set0(&f, "ScanRemainingTime", "Visible", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, IM, "scanningLonelyMerc", Sf_Int(&f, 0));
	if (Sf_True(&f, USR()) != 1) {
		return Sf_None(&f);
	}
	if (Sf_True(&f, G(IM, "lonelyMerc")) > 0) {
		Sf_Import(&f, "Parking");
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
		Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
		Sf_SetAttrS(&f, G(IM, "lonelyMerc"), "EntityList", "");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(IM, "lonelyMerc"), 0);
		SETSAVE("MissionState", "CallDeepThroat");
		Lv_Com(&f, "Mission_IdentifyMerc_RemoteMessage3", "DeepThroat", G(IM, "PreCallDeepThroat"), NULL);
		Lv_ScorerCall(&f, Py_None);
		Sf_DrawScorer(&f);
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ScanShip");
	}
	return Sf_None(&f);
}

/* OnLonelyMercDeath(LooserName, WinnerName): killed before the scan: DeepThroat, he comes back */
static PyObject *Merc_OnLonelyMercDeath(PyObject *self, PyObject *args) {
	static const char *const states[] = { "IdentifyMerc", "IdentifyMerc2", NULL };
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Own(&f, IM, "ResetScan", "()");
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ScanShip");
	if (Sf_InS(&f, SAVE("MissionState"), states) > 0) {
		Lv_Com(&f, "Mission_IdentifyMerc_RemoteMessage1", "DeepThroat", G(IM, "PreRespawnLonelyMerc"), NULL);
	}
	Sf_SetGlobal(&f, IM, "lonelyMerc", Py_None);
	return Sf_None(&f);
}

/* OnLonelyMercDamage(LooserName, WinnerName, Damage): hit by the player: he attacks him */
static PyObject *Merc_OnLonelyMercDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	int hit = Sf_Compare(&f, winner, "==", Sf_Attr(&f, usr, "Name"));
	if (hit > 0) {
		hit = Sf_Compare(&f, Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", looser), "!=", Sf_Int(&f, 2));
	}
	if (hit > 0) {
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, looser, winner);
		Sf_SetAttrS(&f, Sf_Entity(&f, looser), "TgType", "red");
	}
	return Sf_None(&f);
}

static PyObject *Merc_PreRespawnLonelyMerc(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Later(&f, Sf_Float(&f, 2.0), G(IM, "RespawnLonelyMerc"), NOARGS, NULL);
	return Sf_None(&f);
}

/* RespawnLonelyMerc(): a new mercenary as the target, the scan goes on */
static PyObject *Merc_RespawnLonelyMerc(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, IM, "lonelyMerc", Sf_Own(&f, IM, "CreateLonelyMerc", "(s)", "Normal"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(IM, "lonelyMerc"), "Name"));
	Lv_Com(&f, "Mission_IdentifyMerc_RemoteMessage2", "DeepThroat", NULL, NULL);
	Sf_Own(&f, IM, "ScanShip", "()");
	return Sf_None(&f);
}

/* SetPhoneCab(): the call to DeepThroat from a phone cab */
static PyObject *Merc_SetPhoneCab(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Vehicles", "Speech", "Alarm", "SaveGame", "PhoneCab", "MissionsFuncs", NULL };
	SF_ARGS("");
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Lv_PhoneCab(&f, "Mission_IdentifyMerc.UsePhoneCab", 1, IM);
	return Sf_None(&f);
}

static PyObject *Merc_UsePhoneCab(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	int alarm = Os_PhoneUse(&f, IM, usr, ch, opt, full);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", G(IM, "pc"), "Generics_StillPolice1_DeepThroat", "DeepThroat");
	} else if (alarm == 0) {
		Os_PhoneTalk(&f, IM, "SetNextMission", "DeepThroat", "Mission_TalkToDeepThroat3_ComDeepThroat_NI_1Msg");
	}
	return Sf_None(&f);
}

/* SetNextMission(): the cab no longer a target, on to Mission_ProtectGus in Commercial */
static PyObject *Merc_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	if (Sf_True(&f, G(IM, "pc")) > 0) {
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(IM, "pc"), 0);
		Sf_SetAttrI(&f, G(IM, "pc"), "ArrowCanUse", 0);
	}
	Lv_PcDefault(&f, IM);
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_ProtectGus", "Commercial");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ProtectGus_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the scan text and bar, visible while the scan runs */
static PyObject *Merc_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Sf_MissionText(&f, id, "ScanStatusText", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 0), "Horatio", "Shadow", Lv_Label(&f, "Mission_IdentifyMerc_Scorer1"),
		"Left", G(IM, "scanningLonelyMerc"));
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "ScanRemainingTime", "Bar");
	Lv_BarDefaults(&f, id, "ScanRemainingTime", NULL, NULL);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "ScanRemainingTime", "IsAutoUpdatable", 1);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "MaxValue", G(IM, "MAX_SCANNING_TIME"));
	if (Sf_True(&f, G(IM, "scanningLonelyMerc")) == 0) {
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "Value", G(IM, "MAX_SCANNING_TIME"));
	}
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "Visible", G(IM, "scanningLonelyMerc"));
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsIdentifyMercMethods[] = {
	{ "CreateLonelyMercProfiles", Merc_CreateLonelyMercProfiles, 1, NULL },
	{ "InitStatus", Merc_InitStatus, 1, NULL },
	{ "Init", Merc_Init, 1, NULL },
	{ "InitIdentifyMerc", (PyCFunction)(void (*)(void))Merc_InitIdentifyMerc, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateLonelyMercInScene", Merc_CreateLonelyMercInScene, 1, NULL },
	{ "CheckTraitoriusDeath", Merc_CheckTraitoriusDeath, 1, NULL },
	{ "EndOfScene", Merc_EndOfScene, 1, NULL },
	{ "CreateLonelyMerc", (PyCFunction)(void (*)(void))Merc_CreateLonelyMerc, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "InitMission", Merc_InitMission, 1, NULL },
	{ "ScanShip", Merc_ScanShip, 1, NULL },
	{ "OnFinishScan", Merc_OnFinishScan, 1, NULL },
	{ "OnLonelyMercDeath", Merc_OnLonelyMercDeath, 1, NULL },
	{ "OnLonelyMercDamage", Merc_OnLonelyMercDamage, 1, NULL },
	{ "PreRespawnLonelyMerc", Merc_PreRespawnLonelyMerc, 1, NULL },
	{ "RespawnLonelyMerc", Merc_RespawnLonelyMerc, 1, NULL },
	{ "SetPhoneCab", Merc_SetPhoneCab, 1, NULL },
	{ "UsePhoneCab", Merc_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Merc_SetNextMission, 1, NULL },
	{ "MissionScorer", Merc_MissionScorer, 1, NULL },
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
static PyObject *Rudolph_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "MapRace");
	Sf_SetMod(&f, "MapRace", "Races", Sf_Build(&f, "[O]", G(KR, "RaceList")));
	SETSAVE("AlarmActive", "0");
	SETSAVE("AlarmStatus", "0");
	Lv_AcShips(&f, KR, ships_police5);
	return Sf_None(&f);
}

/* Init(LevelPath): the race, Rudolph to kill or the call by the state */
static PyObject *Rudolph_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(KR, "DataPackModel"));
	int race = Sf_EqS(&f, SAVE("KillRudolphMissionData"), "RudolphRace");
	if (race > 0) {
		Sf_Own(&f, KR, "InitRace", "()");
	} else if (race == 0) {
		int killer = Sf_EqS(&f, SAVE("KillRudolphMissionData"), "RudolphKiller");
		if (killer >= 0) {
			Sf_Own(&f, KR, killer ? "initCreateRudolph" : "InitTalkToPoliceBoss", "()");
		}
	}
	return Sf_None(&f);
}

static PyObject *Rudolph_GetDealType(PyObject *self, PyObject *args) {
	SF_ARGS("");
	return Sf_Return(&f, Sf_Str(&f, "Race"));
}

/* InitRace(): the race against Rudolph and three more functionaries */
static PyObject *Rudolph_InitRace(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	SETSAVE("Challenge.Money", "0");
	SETSAVE("Race.Profile", "Pilot");
	Sf_SetMod(&f, "RaceMaker", "GetDealType", G(KR, "GetDealType"));
	Sf_SetMod(&f, "RaceMaker", "EndOfRaceCallback", G(KR, "EndOfRace"));
	PyObject *fn = Sf_Mod(&f, "RaceMaker", "InitRace");
	PyObject *ship = Sf_Method(&f, usr, "IsType", "(s)", "Car");
	PyObject *kw = Sf_Build(&f, "{s:i,s:i,s:O}", "numply", 4, "waypointlimit", 15, "userOnShip", ship);
	Sf_CallKw(&f, fn, f.failed ? NULL : Py_BuildValue("(s)", "Functionary"), kw);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(KR, "rudolphEntityName"));
	PyObject *v = Sf_Entity(&f, G(KR, "rudolphEntityName"));
	if (Sf_True(&f, v) > 0) {
		Sf_Own(&f, KR, "SetRudolphDef", "(O)", v);
	}
	return Sf_None(&f);
}

/* initCreateRudolph(): Rudolph at a free spawn point, out of the race, as the target */
static PyObject *Rudolph_initCreateRudolph(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *spw = Sf_Ask(&f, "OutMap", "GetFreeSpawnPos", "()");
	PyObject *fn = Lv_CreateAt(&f, G(KR, "rudolphEntityName"), Sf_Item(&f, spw, 0), "Car");
	PyObject *pd = Sf_Own(&f, KR, "GetRudolphProfile", "()");
	Sf_Run(&f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(&f, fn, "Name"), Sf_Attr(&f, pd, "Ship"));
	Sf_Own(&f, KR, "SetRudolphDef", "(O)", fn);
	Sf_Own(&f, KR, "SetRudolphAsNone", "()");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(KR, "rudolphEntityName"));
	return Sf_None(&f);
}

/* SetRudolphDef(v): his profile, no coin, his death drops the clue */
static PyObject *Rudolph_SetRudolphDef(PyObject *self, PyObject *args) {
	PyObject *v;
	SF_ARGS("O", &v);
	PyObject *pd = Sf_Own(&f, KR, "GetRudolphProfile", "()");
	Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", v, pd);
	Sf_SetAttrI(&f, v, "Money", -1);
	Sf_SetAttrS(&f, v, "OnDeath", "Mission_KillRudolph.RudolphKilled");
	PyObject *life = G(KR, "RUDOLPH_LIFE");
	Sf_SetAttr(&f, v, "Life", life);
	Sf_SetAttr(&f, v, "MinLife", life);
	Sf_SetAttr(&f, v, "MaxLife", life);
	return Sf_None(&f);
}

/* OnDamage(Looser, Winner, damage): hit: an enemy */
static PyObject *Rudolph_OnDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *v = Sf_Entity(&f, looser);
	if (Sf_True(&f, v) > 0) {
		Sf_SetAttrS(&f, v, "EntityList", "Enemies");
	}
	return Sf_None(&f);
}

/* EndOfRace(ItemName, VehicleName): Rudolph to kill now; his comment on the result, the reward if the player won */
static PyObject *Rudolph_EndOfRace(PyObject *self, PyObject *args) {
	static const char *const stopped[] = { "Alarm", "Killed", "CounterMeasure", NULL };
	static const char *const mods[] = { "Speech", "Parking", "Alarm", "Menu", "RaceMaker", "string", "SaveGame", NULL };
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	SETSAVE("KillRudolphMissionData", "RudolphKiller");
	if (Sf_Compare(&f, veh, "!=", Py_None) > 0) {
		int stop = Sf_InS(&f, veh, stopped);
		if (stop > 0) {
			Lv_Com(&f, "RaceInterruptedFunctionary", "Rudolph", NULL, NULL);
		} else if (stop == 0) {
			int left = Sf_EqS(&f, veh, "Left");
			if (left > 0) {
				Lv_Com(&f, "RaceLeftFunctionary", "Rudolph", NULL, NULL);
			} else if (left == 0) {
				int won = Sf_EqI(&f, Sf_Attr(&f, Sf_Entity(&f, veh), "UsrControl"), 0);
				if (won > 0) {
					PyObject *text = Sf_Ask(&f, "string", "replace", "(OsO)", Sf_Lang(&f, Sf_Str(&f, "RaceSystemWin")), "<n>",
						Sf_Repr(&f, G(KR, "MONEY_REWARD")));
					Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", text);
					Lv_Com(&f, "RaceWin", "Rudolph", NULL, NULL);
					PyObject *fn = Sf_Mod(&f, "Scrap", "SetMoney");
					Sf_Line(&f, fn, "(O)", Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '+', G(KR, "MONEY_REWARD")));
					Sf_Run(&f, "SaveGame", "SaveMoney", "()");
				} else if (won == 0) {
					Lv_Com(&f, "RaceLooseFunctionary", "Rudolph", NULL, NULL);
				}
			}
		}
	}
	Sf_Schedule(&f, Sf_Time(&f), G(KR, "SetRudolphAsNone"), NOARGS, NULL);
	return Sf_None(&f);
}

/* SetRudolphAsNone(): Rudolph out of the race on the traffic paths, an enemy once hit */
static PyObject *Rudolph_SetRudolphAsNone(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *v = Sf_Entity(&f, G(KR, "rudolphEntityName"));
	if (Sf_True(&f, v) > 0) {
		Sf_SetAttrS(&f, v, "OnDamage", "Mission_KillRudolph.OnDamage");
		Sf_SetAttrS(&f, v, "EntityList", "");
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, G(KR, "rudolphEntityName"), 0);
	}
	return Sf_None(&f);
}

/* RudolphKilled(LooserName, WinnerName): the clue where he died, active after the police boss' message */
static PyObject *Rudolph_RudolphKilled(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	PyObject *l = Sf_Own(&f, KR, "CreateClue", "(O)", Sf_Attr(&f, Sf_Entity(&f, looser), "Pos"));
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Lv_Com(&f, "Mission_KillRudolph_RemoteMessage2", "PoliceBoss", G(KR, "ActivateClue"), f.failed ? NULL : Sf_Build(&f, "(O)", l));
	return Sf_None(&f);
}

/* CreateClue(pos): the data pack of the clue (not takeable yet) */
static PyObject *Rudolph_CreateClue(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	PyObject *l = Lv_CreateAt(&f, Sf_Str(&f, "RudolphClue"), pos, "Item");
	Sf_Method(&f, l, "SetModel", "(O)", G(KR, "DataPackModel"));
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_Import(&f, "Items");
	Sf_Run(&f, "Items", "SetSound", "(Os)", l, "TakeBDataPack");
	return Sf_Return(&f, l);
}

/* ActivateClue(l): the clue takeable, an arrow above it as the target */
static PyObject *Rudolph_ActivateClue(PyObject *self, PyObject *args) {
	PyObject *l;
	SF_ARGS("O", &l);
	Sf_SetAttrS(&f, l, "OnTake", "Mission_KillRudolph.OnClueTaken");
	PyObject *l2 = Lv_CreateAt(&f, Sf_Str(&f, "MissionCheckArrow"), Sf_Attr(&f, l, "Pos"), "WithLife");
	Sf_SetAttr(&f, l2, "CamTgt", Sf_Build(&f, "(iii)", 0, 1000, 0));
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", l2, 1);
	return Sf_None(&f);
}

/* OnClueTaken(ItemName, VehicleName): the player took the clue: it and the arrow go, the police boss' message */
static PyObject *Rudolph_OnClueTaken(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	if (Lv_UsrNameIs(&f, USR(), veh) > 0) {
		PyObject *itm = Sf_Entity(&f, item);
		if (Sf_True(&f, itm) > 0) {
			Sf_SetAttrI(&f, itm, "rTime", 20);
			Sf_Later(&f, Sf_Int(&f, 5), Sf_Attr(&f, itm, "Del"), NOARGS, NULL);
		}
		itm = ENT("MissionCheckArrow");
		if (Sf_True(&f, itm) > 0) {
			Sf_Method(&f, itm, "Del", "()");
		}
		Lv_Com(&f, "Mission_KillRudolph_RemoteMessage1", "PoliceBoss", G(KR, "SetNextMission"), NULL);
	}
	return Sf_None(&f);
}

static PyObject *Rudolph_InitTalkToPoliceBoss(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PhoneCab(&f, "Mission_KillRudolph.UsePhoneCab", 1, KR);
	return Sf_None(&f);
}

static PyObject *Rudolph_UsePhoneCab(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	int alarm = Os_PhoneUse(&f, KR, usr, ch, opt, full);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", G(KR, "pc"), "Generics_StillPolice1_PoliceBoss", "PoliceBoss");
	} else if (alarm == 0) {
		Os_PhoneTalk(&f, KR, "SetNextMission2", "PoliceBoss", "Mission_Get3rdClue_ComPoliceBoss_NI_1Msg");
	}
	return Sf_None(&f);
}

/* SetNextMission2(): the cab free again, the characters' places, on to Mission_CatchTrurl in TownHall */
static PyObject *Rudolph_SetNextMission2(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "PoliceBossAtTownHall", "1", "BankMasterAtBank",
		"1", "Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "TownHall", NULL };
	SF_ARGS("");
	Lv_PcDefault(&f, KR);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "KillRudolphMissionData");
	Lv_Saves(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_CatchTrurl", "TownHall");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetTrurl_MissionText1", 1);
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsKillRudolphMethods[] = {
	{ "GetRudolphProfile", Rudolph_GetRudolphProfile, 1, NULL },
	{ "RaceList", Rudolph_RaceList, 1, NULL },
	{ "InitStatus", Rudolph_InitStatus, 1, NULL },
	{ "Init", Rudolph_Init, 1, NULL },
	{ "GetDealType", Rudolph_GetDealType, 1, NULL },
	{ "InitRace", Rudolph_InitRace, 1, NULL },
	{ "initCreateRudolph", Rudolph_initCreateRudolph, 1, NULL },
	{ "SetRudolphDef", Rudolph_SetRudolphDef, 1, NULL },
	{ "OnDamage", Rudolph_OnDamage, 1, NULL },
	{ "EndOfRace", Rudolph_EndOfRace, 1, NULL },
	{ "SetRudolphAsNone", Rudolph_SetRudolphAsNone, 1, NULL },
	{ "RudolphKilled", Rudolph_RudolphKilled, 1, NULL },
	{ "CreateClue", Rudolph_CreateClue, 1, NULL },
	{ "ActivateClue", Rudolph_ActivateClue, 1, NULL },
	{ "OnClueTaken", Rudolph_OnClueTaken, 1, NULL },
	{ "InitTalkToPoliceBoss", Rudolph_InitTalkToPoliceBoss, 1, NULL },
	{ "UsePhoneCab", Rudolph_UsePhoneCab, 1, NULL },
	{ "SetNextMission2", Rudolph_SetNextMission2, 1, NULL },
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
static PyObject *Debtors_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Traffic");
	Sf_SetMod(&f, "Traffic", "NumNormal", Sf_Int(&f, 3));
	Sf_SetMod(&f, "Traffic", "NumMedium", Sf_Int(&f, 0));
	Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	Lv_AcShips(&f, BD, ships_police4);
	return Sf_None(&f);
}

/* the meeting point that starts a mission at the dummy pattern: an Item with the OnTake onTake, the global
 * missionStartWaypoint of module */
static PyObject *Os_StartWaypoint(scriptFrame_t *f, const char *module, const char *pattern, const char *missing, const char *onTake) {
	PyObject *dummy = Lv_Dummy(f, pattern, 1);
	if (Sf_True(f, dummy) == 0) {
		Sf_RaiseText(f, missing);
	}
	Sf_SetGlobal(f, module, "missionStartWaypoint", Lv_CreateAt(f, Sf_Str(f, "MissionStart"), Sf_Item(f, dummy, 1), "Item"));
	PyObject *w = Sf_Global(f, module, "missionStartWaypoint");
	Sf_Method(f, w, "SetModel", "(s)", "models/Misc/MeetingPoint/MeetingPoint.m3d");
	Sf_SetAttrI(f, w, "RadarShow", 1);
	Sf_SetAttrI(f, w, "Radius", 3500);
	Sf_SetAttr(f, w, "Ang", Sf_Build(f, "(iii)", 0, 180, 0));
	Sf_SetAttrS(f, w, "OnTake", onTake);
	Sf_Run(f, "MissionsFuncs", "ActivateTarget", "(Oi)", w, 1);
	return w;
}

static PyObject *Debtors_CreateWaypoint(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Os_StartWaypoint(&f, BD, "DM_Mission_DestroyBadDebtors", "No existe el dummy para esta misi\xf3n: 'DM_Mission_DestroyBadDebtors'",
		"Mission_DestroyBadDebtors.OnInitMission");
	return Sf_None(&f);
}

/* CreateShip(TypeShip, Name): a ship at the nearest free spawn point with the profile and life of its type */
static PyObject *Debtors_CreateShip(PyObject *self, PyObject *args) {
	static const struct {
		const char *type, *profile, *life;
	} types[] = {
		{ "SMERC2", "GetMercenariesProfile", "MERCS_LIFE" },
		{ "SBANKMASTER1", "GetBankDirectorProfile", "BANK_DIRECTOR_LIFE" },
		{ "SBANKER1", "GetBankerProfile", "BANKER_LIFE" },
	};
	PyObject *type, *name;
	SF_ARGS("OO", &type, &name);
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "VehiclesNPC");
	PyObject *spw = Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "()");
	PyObject *o1 = Lv_CreateAt(&f, name, spw, "Car");
	Sf_SetAttrI(&f, o1, "Money", -1);
	PyObject *life = Sf_Int(&f, 100), *pd = NULL;
	int is = Sf_EqS(&f, type, "TLight2");
	if (is > 0) {
		pd = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(O)", type);
	}
	for (int i = 0; i < 3 && is == 0; i++) {
		is = Sf_EqS(&f, type, types[i].type);
		if (is > 0) {
			pd = Sf_Own(&f, BD, types[i].profile, "()");
			life = G(BD, types[i].life);
		}
	}
	if (is == 0) {
		Sf_Raise(&f, PyExc_NameError, "pd");
	}
	Lv_StdAI(&f, o1, pd);
	if (Sf_Compare(&f, type, "!=", Sf_Str(&f, "TLight2")) > 0) {
		Sf_SetAttr(&f, o1, "Life", life);
		Sf_SetAttr(&f, o1, "MaxLife", life);
	}
	return Sf_Return(&f, o1);
}

/* OnInitMission(ItemName, VehicleName): the player at the meeting point: without alarm and enemies the mission
 * starts: a money transport, the bank master and two bankers after it */
static PyObject *Debtors_OnInitMission(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	Sf_Import(&f, "Traffic");
	Sf_Import(&f, "OutPolice");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1 || Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", veh) != 0) {
		return Sf_None(&f);
	}
	int busy = Sf_True(&f, Sf_Ask(&f, "SLogic", "IsEnemyActive", "()"));
	if (busy == 0) {
		busy = Lv_Alarm(&f);
	}
	if (busy > 0) {
		Sf_SetAttrS(&f, G(BD, "missionStartWaypoint"), "OnTake", "");
		Lv_Com(&f, "Mission_DistractHumphrey_RemoteMessage11", "DeepThroat", G(BD, "EnableMissionWaypoint"), NULL);
		return Sf_None(&f);
	}
	Sf_Method(&f, G(BD, "missionStartWaypoint"), "Del", "()");
	Sf_SetGlobal(&f, BD, "missionStartWaypoint", Py_None);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnItemTakenAI", "Mission_DestroyBadDebtors.OnShipReachedTreasurePos");
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 1);
	Sf_Own(&f, BD, "SelectNewMissionTarget", "()");
	PyObject *pos = Py_None;
	PyObject *e = Sf_Entity(&f, G(BD, "currentMissionTarget"));
	if (Sf_True(&f, e) > 0) {
		pos = Sf_Attr(&f, e, "Pos");
	}
	Sf_Own(&f, BD, "CreateBankMaster", "(O)", pos);
	Sf_Own(&f, BD, "CreateBanker", "(iO)", 0, pos);
	Sf_Own(&f, BD, "CreateBanker", "(iO)", 0, pos);
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, G(BD, "bankMasterName"), G(BD, "currentMissionTarget"));
	PyObject *name;
	for (int k = 0; (name = Sf_Next(&f, G(BD, "bankers"), k)) != NULL; k++) {
		int mark = f.n;
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 7, name, G(BD, "bankMasterName"));
		Sf_Release(&f, mark);
	}
	Lv_Com(&f, "Mission_MeetBankMaster_RemoteMessage2", "BankMaster", G(BD, "ShowScorer"), NULL);
	return Sf_None(&f);
}

/* the current mission target red (when it exists) */
static byte Bd_TargetRed(scriptFrame_t *f) {
	PyObject *e = Sf_Entity(f, Sf_Global(f, BD, "currentMissionTarget"));
	if (Sf_True(f, e) > 0) {
		Sf_SetAttrS(f, e, "TgType", "red");
	}
	return !f->failed;
}

/* ShowScorer(): the transport red and the target, the scorer of the money */
static PyObject *Debtors_ShowScorer(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Bd_TargetRed(&f);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(BD, "currentMissionTarget"));
	Lv_ScorerCall(&f, G(BD, "MissionScorer"));
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* EnableMissionWaypoint(): the meeting point works again (FH: the same for Mission_FunctionaryHunting) */
static PyObject *Os_EnableWaypoint(PyObject *args, const char *module, const char *onTake) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *w = G(module, "missionStartWaypoint");
	if (Sf_True(&f, w) > 0) {
		Sf_SetAttrS(&f, w, "OnTake", onTake);
	}
	return Sf_None(&f);
}

static PyObject *Debtors_EnableMissionWaypoint(PyObject *self, PyObject *args) {
	(void)self;
	return Os_EnableWaypoint(args, BD, "Mission_DestroyBadDebtors.OnInitMission");
}

/* SelectNewMissionTarget(): a new money transport on a traffic path */
static PyObject *Debtors_SelectNewMissionTarget(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Traffic");
	Sf_Import(&f, "OutMap");
	Sf_Run(&f, "Traffic", "SetPath", "()");
	PyObject *o1 = Sf_Own(&f, BD, "CreateShip", "(ss)", "TLight2", "MoneyTransport");
	Sf_SetAttrI(&f, o1, "Life", 100);
	Sf_SetAttrI(&f, o1, "MaxLife", 100);
	Sf_SetAttrS(&f, o1, "OnDeath", "Mission_DestroyBadDebtors.OnMissionTargetDeath");
	Sf_SetAttrS(&f, o1, "OnDamage", "Mission_DestroyBadDebtors.OnMissionTargetDamage");
	Sf_SetAttr(&f, o1, "Pos", Sf_Item(&f, Sf_Ask(&f, "OutMap", "GetFreeSpawnPos", "()"), 0));
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOidd)", 3, Sf_Attr(&f, o1, "Name"), 0, 2.0, 0.025);
	Sf_Run(&f, "Traffic", "ResetPath", "()");
	Sf_SetGlobal(&f, BD, "currentMissionTarget", Sf_Attr(&f, o1, "Name"));
	return Sf_None(&f);
}

/* OnMissionTargetDeath(LooserName, WinnerName): the treasure, the next transport, the mercenaries and the bank master
 * a second later */
static PyObject *Debtors_OnMissionTargetDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Own(&f, BD, "DropTreasure", "(O)", Sf_Attr(&f, Sf_Entity(&f, looser), "Pos"));
	Sf_Own(&f, BD, "SelectNewMissionTarget", "()");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(BD, "currentMissionTarget"));
	Bd_TargetRed(&f);
	Sf_Later(&f, Sf_Int(&f, 1), G(BD, "CheckMercsFreezed"), NOARGS, NULL);
	Sf_Later(&f, Sf_Int(&f, 1), G(BD, "BankMasterAttacksMissionTarget"), NOARGS, NULL);
	return Sf_None(&f);
}

static PyObject *Debtors_BankMasterAttacksMissionTarget(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, Sf_Entity(&f, G(BD, "bankMasterName"))) > 0) {
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, G(BD, "bankMasterName"), G(BD, "currentMissionTarget"));
	}
	return Sf_None(&f);
}

/* DropTreasure(pos): the first time the bankers follow the player; the treasure for 20 s, the nearest mercenary and
 * banker go for it */
static PyObject *Debtors_DropTreasure(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	if (Sf_True(&f, G(BD, "firstTimeDropped")) > 0) {
		PyObject *usr = USR();
		if (Sf_True(&f, usr) > 0) {
			PyObject *usrName = Sf_Attr(&f, usr, "Name"), *name;
			for (int k = 0; (name = Sf_Next(&f, G(BD, "bankers"), k)) != NULL; k++) {
				int mark = f.n;
				Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 7, name, usrName);
				Sf_Release(&f, mark);
			}
			Sf_SetGlobal(&f, BD, "firstTimeDropped", Sf_Int(&f, 0));
		}
	}
	PyObject *l = Lv_CreateAt(&f, Sf_Str(&f, "Treasure"), pos, "Item");
	Sf_Method(&f, l, "SetModel", "(O)", G(BD, "TreasureModel"));
	Sf_SetAttr(&f, l, "Radius", G(BD, "TreasureRadius"));
	Sf_SetAttrI(&f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(&f, l, "AutoFace", 0);
	Sf_SetAttrI(&f, l, "OnlyUser", 0);
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
	Sf_SetAttrS(&f, l, "OnTake", "Mission_DestroyBadDebtors.OnCoinTaken");
	Sf_Import(&f, "Items");
	Sf_Run(&f, "Items", "SetSound", "(Os)", l, "TakeTreasure");
	PyObject *l2 = Lv_CreateAt(&f, Sf_Cat(&f, "Os", Sf_Attr(&f, l, "Name"), "CheckArrow"), pos, "WithLife");
	Sf_SetAttr(&f, l2, "CamTgt", Sf_Build(&f, "(iii)", 0, 1000, 0));
	PyObject *time = Sf_Time(&f);
	Sf_Schedule(&f, Sf_OpI(&f, time, '+', 20), G(BD, "DelTreasure"), Sf_Build(&f, "(O)", Sf_Attr(&f, l, "Name")), NULL);
	Sf_Own(&f, BD, "SendShipToGetTreasure", "(OO)", G(BD, "mercsActive"), pos);
	Sf_Own(&f, BD, "SendShipToGetTreasure", "(OO)", G(BD, "bankers"), pos);
	Sf_Method(&f, G(BD, "treasuresActive"), "append", "((OO))", Sf_Attr(&f, l, "Name"), time);
	return Sf_None(&f);
}

/* DelTreasure(ItemName): the treasure goes 5 s later, its arrow now, out of the active treasures */
static PyObject *Debtors_DelTreasure(PyObject *self, PyObject *args) {
	PyObject *item;
	SF_ARGS("O", &item);
	PyObject *e = Sf_Entity(&f, item);
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttrI(&f, e, "rTime", 20);
		Sf_Later(&f, Sf_Int(&f, 5), Sf_Attr(&f, e, "Del"), NOARGS, NULL);
	}
	e = Sf_Entity(&f, Sf_Cat(&f, "Os", item, "CheckArrow"));
	if (Sf_True(&f, e) > 0) {
		Sf_Method(&f, e, "Del", "()");
	}
	PyObject *elem;
	for (int k = 0; (elem = Sf_Next(&f, G(BD, "treasuresActive"), k)) != NULL; k++) {
		int mark = f.n;
		if (Sf_Compare(&f, item, "==", Sf_Item(&f, elem, 0)) > 0) {
			Sf_Method(&f, G(BD, "treasuresActive"), "remove", "(O)", elem);
			break;
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* SendShipToGetTreasure(shipsList, pos): the nearest ship of the list goes to the treasure */
static PyObject *Debtors_SendShipToGetTreasure(PyObject *self, PyObject *args) {
	PyObject *list, *pos;
	SF_ARGS("OO", &list, &pos);
	int n = Sf_Len(&f, list);
	if (n == 0) {
		return Sf_None(&f);
	}
	PyObject *cur = Sf_Item(&f, list, 0);
	PyObject *minDist = Sf_Int(&f, 9000000), *ship;
	for (int k = 0; (ship = Sf_Next(&f, list, k)) != NULL; k++) {
		PyObject *e = Sf_Entity(&f, ship);
		if (Sf_True(&f, e) > 0) {
			PyObject *d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", Sf_Attr(&f, e, "Pos"), pos));
			if (Sf_Compare(&f, d, "<", minDist) > 0) {
				minDist = d;
				cur = ship;
			}
		}
	}
	if (Sf_True(&f, Sf_Entity(&f, cur)) > 0) {
		PyObject *fn = Sf_Mod(&f, "SAI", "SetStateVehicle");
		PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
		Sf_Line(&f, fn, "(iOOOOOi)", 1, cur, x, y, z, G(BD, "TreasureRadius"), 1);
	}
	return Sf_None(&f);
}

/* GetPossibleMercTargets(): the bankers that exist and the money transport */
static PyObject *Debtors_GetPossibleMercTargets(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *targets = Sf_Build(&f, "[]"), *name;
	for (int k = 0; (name = Sf_Next(&f, G(BD, "bankers"), k)) != NULL; k++) {
		int mark = f.n;
		PyObject *e = Sf_Entity(&f, name);
		if (Sf_True(&f, e) > 0) {
			Sf_Method(&f, targets, "append", "(O)", Sf_Attr(&f, e, "Name"));
		}
		Sf_Release(&f, mark);
	}
	PyObject *e = Sf_Entity(&f, G(BD, "currentMissionTarget"));
	if (Sf_True(&f, e) > 0) {
		Sf_Method(&f, targets, "append", "(O)", Sf_Attr(&f, e, "Name"));
	}
	return Sf_Return(&f, targets);
}

/* float(len(targets)) - 0.01 */
static PyObject *Bd_MaxRange(scriptFrame_t *f, PyObject *targets) {
	return Sf_Op(f, Sf_ToFloat(f, Sf_Int(f, Sf_Len(f, targets))), '-', Sf_Float(f, 0.01));
}

/* SAI.SetStateVehicle(2, name, targets[int(Scrap.Rand(0.0, maxRange))]) */
static byte Bd_Attack(scriptFrame_t *f, PyObject *name, PyObject *targets, PyObject *maxRange) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, "SAI", "SetStateVehicle");
	PyObject *t = Sf_GetItem(f, targets, Sf_ToInt(f, Sf_Ask(f, "Scrap", "Rand", "(dO)", 0.0, maxRange)));
	Sf_Line(f, fn, "(iOO)", 2, name, t);
	Sf_Release(f, mark);
	return !f->failed;
}

/* a mercenary attacks one of the possible targets at random, or the player when there is none */
static byte Bd_MercAttack(scriptFrame_t *f, PyObject *name, PyObject *usr) {
	int mark = f->n;
	PyObject *targets = Sf_Own(f, BD, "GetPossibleMercTargets", "()");
	int none = Sf_Compare(f, targets, "==", Sf_Build(f, "[]"));
	if (none > 0) {
		Sf_Run(f, "SAI", "SetStateVehicle", "(iOO)", 2, name, Sf_Attr(f, usr, "Name"));
	} else if (none == 0) {
		Bd_Attack(f, name, targets, Bd_MaxRange(f, targets));
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* CreateMercs(numMercs): the mercenaries near the transport, each after a random target (later when there is none) */
static PyObject *Debtors_CreateMercs(PyObject *self, PyObject *args) {
	PyObject *num;
	SF_ARGS("O", &num);
	Sf_Import(&f, "OutMap");
	PyObject *targets = Sf_Own(&f, BD, "GetPossibleMercTargets", "()");
	if (Sf_Compare(&f, targets, "==", Sf_Build(&f, "[]")) > 0) {
		Sf_Later(&f, Sf_Int(&f, 2), G(BD, "CreateMercs"), Sf_Build(&f, "(O)", num), NULL);
		return Sf_None(&f);
	}
	PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(O)", num), *i;
	for (int k = 0; (i = Sf_Next(&f, range, k)) != NULL; k++) {
		int mark = f.n;
		PyObject *o1 = Sf_Own(&f, BD, "CreateShip", "(sO)", "SMERC2", Sf_Cat(&f, "sO", "Merc", Sf_Repr(&f, i)));
		Sf_SetAttrS(&f, o1, "TgType", "red");
		Sf_SetAttrS(&f, o1, "OnDeath", "Mission_DestroyBadDebtors.OnMercDeath");
		Sf_SetAttrS(&f, o1, "OnDamage", "Mission_DestroyBadDebtors.OnMercDamage");
		PyObject *p = Py_None;
		PyObject *e = Sf_Entity(&f, G(BD, "currentMissionTarget"));
		if (Sf_True(&f, e) > 0) {
			p = Sf_Attr(&f, e, "Pos");
		}
		Sf_SetAttr(&f, o1, "Pos", Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "(O)", p));
		Sf_SetItem(&f, G(BD, "shipsMoney"), Sf_Attr(&f, o1, "Name"), Sf_Int(&f, 0));
		Sf_Method(&f, G(BD, "mercsActive"), "append", "(O)", Sf_Attr(&f, o1, "Name"));
		Sf_Release(&f, mark);
	}
	PyObject *maxRange = Bd_MaxRange(&f, targets), *merc;
	for (int k = 0; (merc = Sf_Next(&f, G(BD, "mercsActive"), k)) != NULL; k++) {
		Bd_Attack(&f, merc, targets, maxRange);
	}
	return Sf_None(&f);
}

/* CreateBankMaster(pos=None): the bank master, green, waiting (near pos) */
static PyObject *Debtors_CreateBankMaster(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "pos", NULL };
	PyObject *pos = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", kwlist, &pos)) {
		return NULL;
	}
	Sf_Import(&f, "OutMap");
	if (Sf_True(&f, USR()) != 1) {
		return Sf_None(&f);
	}
	PyObject *o1 = Sf_Own(&f, BD, "CreateShip", "(ss)", "SBANKMASTER1", "BankMaster");
	Sf_SetAttrS(&f, o1, "TgType", "green");
	Sf_SetAttrS(&f, o1, "OnDeath", "Mission_DestroyBadDebtors.OnBankMasterDeath");
	Sf_SetAttrS(&f, o1, "OnDamage", "Mission_DestroyBadDebtors.OnBankMasterDamage");
	if (Sf_True(&f, pos) > 0) {
		Sf_SetAttr(&f, o1, "Pos", Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "(O)", pos));
	}
	Sf_SetGlobal(&f, BD, "bankMasterName", Sf_Attr(&f, o1, "Name"));
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, G(BD, "bankMasterName"), 0);
	Sf_SetItem(&f, G(BD, "shipsMoney"), G(BD, "bankMasterName"), Sf_Int(&f, 0));
	return Sf_None(&f);
}

/* CreateBanker(followPlayer=0, pos=None): a banker, green (near pos); he goes for the newest treasure, else he
 * follows the player or the bank master */
static PyObject *Debtors_CreateBanker(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "followPlayer", "pos", NULL };
	PyObject *follow = NULL, *pos = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|OO", kwlist, &follow, &pos)) {
		return NULL;
	}
	if (!follow) {
		follow = Sf_Int(&f, 0);
	}
	Sf_Import(&f, "OutMap");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *o1 = Sf_Own(&f, BD, "CreateShip", "(ss)", "SBANKER1", "BankEmployee");
	Sf_SetAttrS(&f, o1, "TgType", "green");
	Sf_SetAttrS(&f, o1, "OnDeath", "Mission_DestroyBadDebtors.OnBankerDeath");
	Sf_SetAttrS(&f, o1, "OnDamage", "Mission_DestroyBadDebtors.OnBankerDamage");
	if (Sf_True(&f, pos) > 0) {
		Sf_SetAttr(&f, o1, "Pos", Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "(O)", pos));
	}
	int treasure = 0;
	PyObject *e = NULL;
	if (Sf_Len(&f, G(BD, "treasuresActive")) > 0) {
		long i = 0, index = 0;
		PyObject *minTime = Sf_Int(&f, 100), *now = Sf_Time(&f), *elem;
		for (int k = 0; (elem = Sf_Next(&f, G(BD, "treasuresActive"), k)) != NULL; k++) {
			PyObject *t = Sf_Op(&f, Sf_Item(&f, elem, 1), '-', now);
			if (Sf_Compare(&f, t, "<", minTime) > 0) {
				minTime = t;
				index = i;
			}
			i++;
		}
		e = Sf_Entity(&f, Sf_Item(&f, Sf_Item(&f, G(BD, "treasuresActive"), index), 0));
		if (Sf_True(&f, e) > 0) {
			treasure = 1;
			Sf_SetAttr(&f, o1, "Pos", Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "(O)", Sf_Attr(&f, e, "Pos")));
		}
	}
	if (!treasure) {
		int fp = Sf_True(&f, follow);
		if (fp > 0) {
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 7, Sf_Attr(&f, o1, "Name"), Sf_Attr(&f, usr, "Name"));
		} else if (fp == 0) {
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 7, Sf_Attr(&f, o1, "Name"), G(BD, "bankMasterName"));
		}
	} else {
		Sf_Own(&f, BD, "SendShipToGetTreasure", "([O]O)", Sf_Attr(&f, o1, "Name"), Sf_Attr(&f, e, "Pos"));
	}
	Sf_Method(&f, G(BD, "bankers"), "append", "(O)", Sf_Attr(&f, o1, "Name"));
	Sf_SetItem(&f, G(BD, "shipsMoney"), Sf_Attr(&f, o1, "Name"), Sf_Int(&f, 0));
	return Sf_None(&f);
}

/* RespawnBankMaster(): the bank master back after the transport (later when there is none) */
static PyObject *Debtors_RespawnBankMaster(PyObject *self, PyObject *args) {
	SF_ARGS("");
	int there = Sf_True(&f, Sf_Entity(&f, G(BD, "currentMissionTarget")));
	if (there == 0) {
		Sf_Later(&f, Sf_Int(&f, 3), G(BD, "RespawnBankMaster"), NOARGS, NULL);
	} else if (there > 0) {
		Sf_Own(&f, BD, "CreateBankMaster", "()");
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, G(BD, "bankMasterName"), G(BD, "currentMissionTarget"));
	}
	return Sf_None(&f);
}

static PyObject *Debtors_OnBankMasterDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Later(&f, Sf_Int(&f, 5), G(BD, "RespawnBankMaster"), NOARGS, "RespawnBankMaster");
	return Sf_None(&f);
}

/* CheckMercsFreezed(): every mercenary that exists attacks a new target */
static PyObject *Debtors_CheckMercsFreezed(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *merc;
	for (int k = 0; (merc = Sf_Next(&f, G(BD, "mercsActive"), k)) != NULL; k++) {
		int mark = f.n;
		if (Sf_True(&f, Sf_Entity(&f, merc)) > 0) {
			Bd_MercAttack(&f, merc, usr);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* a banker or mercenary dies: his treasure falls when he carries money, else the coins of money */
static byte Bd_Drop(scriptFrame_t *f, PyObject *looser, long money) {
	PyObject *e = Sf_Entity(f, looser);
	int rich = Sf_True(f, e);
	if (rich > 0) {
		rich = Sf_Compare(f, Sf_GetItem(f, Sf_Global(f, BD, "shipsMoney"), looser), ">", Sf_Int(f, 0));
	}
	if (rich > 0) {
		Sf_Own(f, BD, "DropTreasure", "(O)", Sf_Attr(f, e, "Pos"));
	} else if (rich == 0) {
		Sf_SetAttrI(f, e, "Money", money);
	}
	return !f->failed;
}

/* OnBankerDeath(LooserName, WinnerName): his treasure, the mercenaries retarget, another banker 10 s later */
static PyObject *Debtors_OnBankerDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Bd_Drop(&f, looser, 100);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Later(&f, Sf_Int(&f, 1), G(BD, "CheckMercsFreezed"), NOARGS, NULL);
	PyObject *t = Sf_OpI(&f, Sf_Time(&f), '+', 10);
	PyObject *fn = G(BD, "CreateBanker");
	int started = Sf_Compare(&f, G(BD, "missionStartWaypoint"), "==", Py_None);
	Sf_Schedule(&f, t, fn, started < 0 ? NULL : Sf_Build(&f, "(iO)", started, Py_None), "RespawnBanker");
	if (Sf_In(&f, looser, G(BD, "bankers")) > 0) {
		Sf_Method(&f, G(BD, "bankers"), "remove", "(O)", looser);
	}
	return Sf_None(&f);
}

/* OnMercDeath(LooserName, WinnerName): his treasure, another mercenary 20 s later */
static PyObject *Debtors_OnMercDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Bd_Drop(&f, looser, 5);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	if (Sf_In(&f, looser, G(BD, "mercsActive")) > 0) {
		Sf_Method(&f, G(BD, "mercsActive"), "remove", "(O)", looser);
	}
	Sf_Later(&f, Sf_Int(&f, 20), G(BD, "CreateMercs"), Sf_Build(&f, "(i)", 1), "RespawnMerc");
	return Sf_None(&f);
}

/* OnTreasureReached(VehicleName): a ship at a treasure it did not get: a mercenary attacks again, a banker follows the
 * player */
static PyObject *Debtors_OnTreasureReached(PyObject *self, PyObject *args) {
	PyObject *veh;
	SF_ARGS("O", &veh);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1 || Sf_True(&f, Sf_Entity(&f, veh)) != 1) {
		return Sf_None(&f);
	}
	int merc = Sf_In(&f, veh, G(BD, "mercsActive"));
	PyObject *state = merc >= 0 ? Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", veh) : NULL;
	if (merc > 0) {
		if (Sf_Compare(&f, state, "!=", Sf_Int(&f, 2)) > 0) {
			Bd_MercAttack(&f, veh, usr);
		}
	} else if (merc == 0 && Sf_Compare(&f, state, "!=", Sf_Int(&f, 7)) > 0) {
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 7, veh, Sf_Attr(&f, usr, "Name"));
	}
	return Sf_None(&f);
}

static PyObject *Debtors_OnShipReachedTreasurePos(PyObject *self, PyObject *args) {
	PyObject *veh;
	SF_ARGS("O", &veh);
	Sf_Later(&f, Sf_Int(&f, 1), G(BD, "OnTreasureReached"), Sf_Build(&f, "(O)", veh), NULL);
	return Sf_None(&f);
}

/* `m = int(Scrap.Rand(0.0, 1.99))`: Speech.RemoteComunication of the first key and speaker when m is 0, else of the
 * second ones */
static byte Bd_OneOf(scriptFrame_t *f, const char *first, const char *who1, const char *second, const char *who2) {
	int m = Sf_EqI(f, Sf_ToInt(f, Sf_Ask(f, "Scrap", "Rand", "(dd)", 0.0, 1.99)), 0);
	if (m < 0) {
		return 0;
	}
	return Sf_Run(f, "Speech", "RemoteComunication", "(ss)", m ? first : second, m ? who1 : who2);
}

/* the bank recovered its money: no scorer, the reward, every ship of the mission back into the traffic */
static byte Bd_Recovered(scriptFrame_t *f) {
	static const char *const funcs[] = { "RespawnBankMaster", "RespawnBanker", "RespawnMerc", NULL };
	Lv_ScorerCall(f, Py_None);
	Lv_Com(f, "Mission_DestroyBadDebtors_RemoteMessage5", "BankMaster", Sf_Global(f, BD, "ShowDeepThroatMessage"), Sf_Build(f, "()"));
	PyObject *money = Sf_Op(f, Sf_Ask(f, "Scrap", "GetMoney", "()"), '+', Sf_Global(f, BD, "MISSION_BANKMASTER_REWARD"));
	Sf_Run(f, "Scrap", "SetMoney", "(O)", money);
	Sf_Import(f, "SaveGame");
	Sf_Run(f, "SaveGame", "SaveMoney", "()");
	PyObject *ships = Sf_Build(f, "[]"), *name;
	for (int k = 0; (name = Sf_Next(f, Sf_Global(f, BD, "bankers"), k)) != NULL; k++) {
		Sf_Method(f, ships, "append", "(O)", name);
	}
	for (int k = 0; (name = Sf_Next(f, Sf_Global(f, BD, "mercsActive"), k)) != NULL; k++) {
		Sf_Method(f, ships, "append", "(O)", name);
	}
	Sf_Method(f, ships, "append", "(O)", Sf_Global(f, BD, "bankMasterName"));
	Sf_Method(f, ships, "append", "(O)", Sf_Global(f, BD, "currentMissionTarget"));
	for (int k = 0; (name = Sf_Next(f, ships, k)) != NULL; k++) {
		int mark = f->n;
		PyObject *e = Sf_Entity(f, name);
		if (Sf_True(f, e) > 0) {
			Sf_Run(f, "SAI", "SetStateVehicle", "(iOi)", 3, name, 1);
			Sf_SetAttrS(f, e, "TgType", "none");
			Sf_SetAttrS(f, e, "OnDeath", "Vehicles.OnDeath");
			Sf_SetAttrS(f, e, "OnDamage", "");
			if (Sf_Compare(f, name, "==", Sf_Global(f, BD, "currentMissionTarget")) > 0) {
				Sf_Run(f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
			}
		}
		Sf_Release(f, mark);
	}
	for (int i = 0; funcs[i]; i++) {
		Sf_Run(f, "Scrap", "DeleteScheduledFuncs", "(s)", funcs[i]);
	}
	return !f->failed;
}

/* shipsMoney[veh] = shipsMoney[veh] + TREASURE_REWARD; moneyRecovered = moneyRecovered op TREASURE_REWARD */
static byte Bd_Carry(scriptFrame_t *f, PyObject *veh, char op) {
	PyObject *money = Sf_Global(f, BD, "shipsMoney");
	Sf_SetItem(f, money, veh, Sf_Op(f, Sf_GetItem(f, Sf_Global(f, BD, "shipsMoney"), veh), '+', Sf_Global(f, BD, "TREASURE_REWARD")));
	PyObject *r = Sf_Op(f, Sf_Global(f, BD, "moneyRecovered"), op, Sf_Global(f, BD, "TREASURE_REWARD"));
	return Sf_SetGlobal(f, BD, "moneyRecovered", r);
}

/* OnCoinTaken(ItemName, VehicleName): a treasure taken by the player (his reward), a mercenary (money lost) or a
 * banker (money recovered; all of it ends the mission) */
static PyObject *Debtors_OnCoinTaken(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1 || Sf_Compare(&f, veh, "==", G(BD, "bankMasterName")) != 0) {
		return Sf_None(&f);
	}
	Sf_Own(&f, BD, "DelTreasure", "(O)", item);
	int who = Sf_Compare(&f, veh, "==", Sf_Attr(&f, usr, "Name"));
	if (who > 0) {
		PyObject *fn = Sf_Mod(&f, "Scrap", "SetMoney");
		Sf_Line(&f, fn, "(O)", Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '+', G(BD, "TREASURE_REWARD")));
		Sf_Run(&f, "Speech", "RemoteComunication", "(ss)", "Mission_DestroyBadDebtors_RemoteMessage4", "DeepThroat");
		return Sf_None(&f);
	}
	if (who == 0 && Sf_In(&f, veh, G(BD, "mercsActive")) > 0) {
		Bd_Carry(&f, veh, '-');
		if (Sf_Compare(&f, G(BD, "moneyRecovered"), "<", Sf_Int(&f, 0)) > 0) {
			Sf_SetGlobal(&f, BD, "moneyRecovered", Sf_Int(&f, 0));
		}
		Bd_MercAttack(&f, veh, usr);
		Bd_OneOf(&f, "Mission_DestroyBadDebtors_RemoteMessage9", "MercenaryA", "Mission_DestroyBadDebtors_RemoteMessage10", "MercenaryB");
		Sf_DrawScorer(&f);
		return Sf_None(&f);
	}
	if (f.failed || Sf_In(&f, veh, G(BD, "bankers")) != 1) {
		return Sf_None(&f);
	}
	Bd_Carry(&f, veh, '+');
	int all = Sf_Compare(&f, G(BD, "moneyRecovered"), ">=", G(BD, "MAX_MONEY"));
	if (all > 0) {
		if (Sf_Compare(&f, Sf_Mod(&f, "Menu", "MissionScorerCall"), "!=", Py_None) > 0) {
			Bd_Recovered(&f);
		}
	} else if (all == 0) {
		Bd_OneOf(&f, "Mission_DestroyBadDebtors_RemoteMessage7", "BankDirector", "Mission_DestroyBadDebtors_RemoteMessage8", "BankDirector");
		int appear = Sf_EqI(&f, G(BD, "mercsHaveAppeared"), 0);
		if (appear > 0) {
			appear = Sf_Compare(&f, G(BD, "moneyRecovered"), ">=", G(BD, "MONEY_MERCS_APPEAR"));
		}
		if (appear > 0) {
			Sf_SetGlobal(&f, BD, "mercsHaveAppeared", Sf_Int(&f, 1));
			Sf_Later(&f, Sf_Int(&f, 5), G(BD, "MercsAppear"), NOARGS, NULL);
		}
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 7, veh, Sf_Attr(&f, usr, "Name"));
		Lv_ScorerCall(&f, G(BD, "MissionScorer"));
	}
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

static PyObject *Debtors_BankMasterSpeech(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Later(&f, Sf_Int(&f, 5), Sf_Mod(&f, "Speech", "AddRemoteCom"), Sf_Build(&f, "(ss)", "Mission_DestroyBadDebtors_RemoteMessage3",
		"BankMaster"), NULL);
	return Sf_None(&f);
}

/* ShowDeepThroatMessage(): the bank master's ship and engine to edit, the message, DeepThroat 5 s later */
static PyObject *Debtors_ShowDeepThroatMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_SaveAdd(&f, BD, "Hangar.shipsToEditList", "SBankMaster1");
	Lv_SaveAdd(&f, BD, "Hangar.availableEnginesList", "MBANKMASTER1");
	Lv_MsgText(&f, Sf_Lang(&f, Sf_Str(&f, "Mission_DestroyBadDebtors_SystemMessage1")));
	Sf_Run(&f, "Scrap", "IncSaveVar", "(si)", "NewBluePrintAvaliable", 2);
	Sf_Later(&f, Sf_Int(&f, 5), Sf_Mod(&f, "Speech", "AddRemoteCom"), Sf_Build(&f, "(ssOO)", "Mission_DestroyBadDebtors_RemoteMessage6",
		"DeepThroat", G(BD, "SetNextMission"), NOARGS), NULL);
	return Sf_None(&f);
}

/* SetNextMission(): the alarm free, the traffic refilled bit by bit, on to Mission_InfiltrateBank */
static PyObject *Debtors_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Bank", "Journalist.Betty_Defaut", "Press",
		"Journalist.Berto_Defaut", "NoPlace", "BadReputation", "4", "StdShipAIProfile", "Ace", NULL };
	SF_ARGS("");
	Sf_Import(&f, "Traffic");
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "Set", "(si)", "MaxFightingTrafficSize", 4);
	Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 7);
	for (int i = 0; i < 37 && !f.failed; i++) {
		int mark = f.n;
		Sf_Later(&f, Sf_Float(&f, 0.25 * i), Sf_Mod(&f, "Traffic", "NormalTrafficDeath"), Sf_Build(&f, "(ss)", "", ""), NULL);
		Sf_Release(&f, mark);
	}
	for (int i = 0; i < 20 && !f.failed; i++) {
		int mark = f.n;
		Sf_Later(&f, Sf_Float(&f, 0.375 * i), Sf_Mod(&f, "Traffic", "MediumTrafficDeath"), Sf_Build(&f, "(ss)", "", ""), NULL);
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_InfiltrateBank", "Bank");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_InfiltrateBank_MissionText1", 1);
	Lv_Saves(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the money recovered as a bar */
static PyObject *Debtors_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_MissionText(&f, id, "MoneyRecoveredText", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 0), "Horatio", "Shadow",
		Lv_Label(&f, "Mission_DestroyBadDebtors_Scorer1"), "Left", NULL);
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "MoneyRecovered", "Bar");
	Lv_BarDefaults(&f, id, "MoneyRecovered", NULL, NULL);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "MoneyRecovered", "IsAutoUpdatable", 0);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "MoneyRecovered", "MaxValue", G(BD, "MAX_MONEY"));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "MoneyRecovered", "Value", G(BD, "moneyRecovered"));
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsDestroyBadDebtorsMethods[] = {
	{ "GetMercenariesProfile", Debtors_GetMercenariesProfile, 1, NULL },
	{ "GetBankDirectorProfile", Debtors_GetBankDirectorProfile, 1, NULL },
	{ "GetBankerProfile", Debtors_GetBankerProfile, 1, NULL },
	{ "InitStatus", Debtors_InitStatus, 1, NULL },
	{ "CreateWaypoint", Debtors_CreateWaypoint, 1, NULL },
	{ "CreateShip", Debtors_CreateShip, 1, NULL },
	{ "OnInitMission", Debtors_OnInitMission, 1, NULL },
	{ "ShowScorer", Debtors_ShowScorer, 1, NULL },
	{ "EnableMissionWaypoint", Debtors_EnableMissionWaypoint, 1, NULL },
	{ "SelectNewMissionTarget", Debtors_SelectNewMissionTarget, 1, NULL },
	{ "OnMissionTargetDeath", Debtors_OnMissionTargetDeath, 1, NULL },
	{ "BankMasterAttacksMissionTarget", Debtors_BankMasterAttacksMissionTarget, 1, NULL },
	{ "DropTreasure", Debtors_DropTreasure, 1, NULL },
	{ "DelTreasure", Debtors_DelTreasure, 1, NULL },
	{ "SendShipToGetTreasure", Debtors_SendShipToGetTreasure, 1, NULL },
	{ "GetPossibleMercTargets", Debtors_GetPossibleMercTargets, 1, NULL },
	{ "CreateMercs", Debtors_CreateMercs, 1, NULL },
	{ "CreateBankMaster", (PyCFunction)(void (*)(void))Debtors_CreateBankMaster, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateBanker", (PyCFunction)(void (*)(void))Debtors_CreateBanker, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "RespawnBankMaster", Debtors_RespawnBankMaster, 1, NULL },
	{ "OnBankMasterDeath", Debtors_OnBankMasterDeath, 1, NULL },
	{ "CheckMercsFreezed", Debtors_CheckMercsFreezed, 1, NULL },
	{ "OnBankerDeath", Debtors_OnBankerDeath, 1, NULL },
	{ "OnMercDeath", Debtors_OnMercDeath, 1, NULL },
	{ "OnTreasureReached", Debtors_OnTreasureReached, 1, NULL },
	{ "OnShipReachedTreasurePos", Debtors_OnShipReachedTreasurePos, 1, NULL },
	{ "OnCoinTaken", Debtors_OnCoinTaken, 1, NULL },
	{ "BankMasterSpeech", Debtors_BankMasterSpeech, 1, NULL },
	{ "ShowDeepThroatMessage", Debtors_ShowDeepThroatMessage, 1, NULL },
	{ "SetNextMission", Debtors_SetNextMission, 1, NULL },
	{ "MissionScorer", Debtors_MissionScorer, 1, NULL },
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
static PyObject *Traitorius_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(AT, "ReassignTarget"));
	int calm = Sf_EqI(&f, Sf_ToInt(&f, SAVE("TraitoriusFrightened")), 0);
	if (calm > 0) {
		Sf_Run(&f, "Scrap", "Set", "(ss)", "OnLifeItemTaken", "Mission_AttackTraitorius.TraitoriusTakesLife");
		Sf_Run(&f, "Scrap", "CreateEntityList", "(s)", "Traitorius");
		Sf_Own(&f, AT, "CreateTraitorius", "()");
	} else if (calm == 0) {
		Sf_Own(&f, AT, "CallTraitorius", "()");
	}
	return Sf_None(&f);
}

/* ReassignTarget(): after boarding: Traitorius flees from the new ship */
static PyObject *Traitorius_ReassignTarget(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	int both = Sf_True(&f, usr);
	if (both > 0) {
		both = Sf_True(&f, G(AT, "traitorius"));
	}
	if (both > 0) {
		PyObject *state = Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", Sf_Attr(&f, G(AT, "traitorius"), "Name"));
		int flee = Sf_EqI(&f, state, 5);
		if (flee == 0) {
			flee = Sf_EqI(&f, state, 0);
		}
		if (flee > 0) {
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOi)", 5, Sf_Attr(&f, G(AT, "traitorius"), "Name"), Sf_Attr(&f, usr, "Name"), 75000);
		}
	}
	return Sf_None(&f);
}

/* CreateTraitorius(): the countdown, Traitorius at a free spawn point (calm, without missiles), the scorer */
static PyObject *Traitorius_CreateTraitorius(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Scrap", "Set", "(si)", "CounterType", 2);
	Sf_Run(&f, "Scrap", "Set", "(sO)", "TimeSpecialUseTimer", G(AT, "frightenedTime"));
	PyObject *spw = Sf_Ask(&f, "OutMap", "GetFreeSpawnPos", "()");
	Sf_SetGlobal(&f, AT, "traitorius", Lv_CreateAt(&f, Sf_Str(&f, "Traitorius"), Sf_Item(&f, spw, 0), "Car"));
	PyObject *t = G(AT, "traitorius");
	Sf_SetAttrS(&f, t, "EntityList", "Traitorius");
	Sf_SetAttrS(&f, t, "TgType", "red");
	Sf_SetAttrI(&f, t, "LaunchTaunts", 0);
	Sf_SetAttr(&f, t, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, spw, 1), 0));
	Sf_SetGlobal(&f, AT, "pd", Sf_Own(&f, AT, "GetTraitoriusProfile", "()"));
	Sf_Run(&f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(&f, t, "Name"), Sf_Attr(&f, G(AT, "pd"), "Ship"));
	Sf_SetAttr(&f, t, "Style", Sf_Op(&f, Sf_Attr(&f, t, "Style"), '-', G(AT, "styleOffset")));
	Sf_SetAttrS(&f, t, "Descriptor", "Traitorius");
	Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", t, G(AT, "pd"));
	Lv_Life(&f, t, AT, "TRAITORIUS_LIFE");
	Sf_SetAttrS(&f, t, "WeapList", "1,1,1,0,0,0");
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, t, "Name"), 0);
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Traitorius", "OnDamage", "Mission_AttackTraitorius.OnTraitoriusDamage");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Traitorius", "OnDeath", "Mission_AttackTraitorius.OnTraitoriusDeath");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, t, "Name"));
	Lv_ScorerCall(&f, G(AT, "MissionScorer"));
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* OnTraitoriusDamage(Looser, Winner, damage): hit by the player: the life bar; frightened he calls the police (after
 * a while); his taunts the first two times */
static PyObject *Traitorius_OnTraitoriusDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *usr = USR();
	int skip = Sf_True(&f, usr);
	skip = skip < 0 ? -1 : !skip;
	if (skip == 0) {
		skip = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", winner);
	}
	if (skip == 0) {
		int t = Sf_True(&f, G(AT, "traitorius"));
		skip = t < 0 ? -1 : !t;
	}
	if (skip != 0) {
		return Sf_None(&f);
	}
	PyObject *t = G(AT, "traitorius");
	if (Sf_Compare(&f, Sf_Attr(&f, t, "Life"), "<=", Sf_Int(&f, 0)) != 0) {
		return Sf_None(&f);
	}
	Lv_Set0(&f, "RemainingLifeScorer", "Value", Sf_Attr(&f, t, "Life"));
	PyObject *life = Sf_Attr(&f, t, "Life");
	int scared = Sf_Compare(&f, life, "<=", Sf_Op(&f, Sf_Attr(&f, t, "MaxLife"), '*', G(AT, "minLifePercentage")));
	if (scared > 0) {
		Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", G(AT, "traitorius"), G(AT, "pd"));
		Sf_Run(&f, "Scrap", "Set", "(si)", "CounterType", 0);
		int call = Lv_Alarm(&f);
		call = call < 0 ? -1 : !call;
		if (call > 0) {
			PyObject *limit = Sf_Op(&f, G(AT, "frightenedTime"), '*', G(AT, "timeCallsPolicePercentage"));
			call = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "TimeSpecialUseTimer"), "<=", limit);
		}
		if (call > 0) {
			PyObject *fn = Sf_Mod(&f, "Speech", "AddRemoteCom");
			PyObject *a = Sf_Build(&f, "(ssO(i))", "Mission_AttackTraitorius_RemoteMessage6", "Traitorius", Sf_Mod(&f, "Scrap", "SetAlarm"), 1);
			Sf_CallKw(&f, fn, Script_Ref(a), Sf_Build(&f, "{s:i}", "InsertFirst", 1));
			return Sf_None(&f);
		}
	}
	const char *key = scared > 0 ? "Mission_AttackTraitorius_RemoteMessage3" : NULL;
	int n = Sf_EqI(&f, G(AT, "firstTimeDamage"), 0);
	if (n > 0) {
		Sf_SetGlobal(&f, AT, "firstTimeDamage", Sf_OpI(&f, G(AT, "firstTimeDamage"), '+', 1));
		t = G(AT, "traitorius");
		Sf_SetAttr(&f, t, "Style", Sf_Op(&f, Sf_Attr(&f, t, "Style"), '+', G(AT, "styleOffset")));
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOi)", 5, Sf_Attr(&f, G(AT, "traitorius"), "Name"), winner, 75000);
		Lv_Com(&f, key ? key : "Mission_AttackTraitorius_RemoteMessage1", "Traitorius", NULL, NULL);
	} else if (n == 0) {
		n = Sf_EqI(&f, G(AT, "firstTimeDamage"), 1);
		if (n > 0) {
			Sf_SetGlobal(&f, AT, "firstTimeDamage", Sf_OpI(&f, G(AT, "firstTimeDamage"), '+', 1));
			Lv_Com(&f, key ? key : "Mission_AttackTraitorius_RemoteMessage2", "Traitorius", NULL, NULL);
		} else if (n == 0 && key) {
			Sf_Run(&f, "Speech", "RemoteComunication", "(ss)", key, "Traitorius");
		}
	}
	return Sf_None(&f);
}

/* OnTraitoriusDeath(LooserName, WinnerName): killed: the alarm, DeepThroat, a new Traitorius */
static PyObject *Traitorius_OnTraitoriusDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Menu");
	Sf_SetGlobal(&f, AT, "traitorius", Py_None);
	Lv_ScorerCall(&f, Py_None);
	Sf_DrawScorer(&f);
	Sf_SetGlobal(&f, AT, "firstTimeDamage", Sf_Int(&f, 0));
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	Lv_Com(&f, "Mission_AttackTraitorius_RemoteMessage5", "DeepThroat", G(AT, "CreateTraitorius"), NOARGS);
	return Sf_None(&f);
}

/* TraitoriusTakesLife(vehicleName): Traitorius took a life item: calm again above the limit, the bar */
static PyObject *Traitorius_TraitoriusTakesLife(PyObject *self, PyObject *args) {
	PyObject *veh;
	SF_ARGS("O", &veh);
	int his = Sf_True(&f, G(AT, "traitorius"));
	if (his > 0) {
		his = Sf_Compare(&f, Sf_Attr(&f, G(AT, "traitorius"), "Name"), "==", veh);
	}
	if (his > 0) {
		PyObject *t = G(AT, "traitorius");
		PyObject *life = Sf_Attr(&f, t, "Life");
		if (Sf_Compare(&f, life, ">", Sf_Op(&f, Sf_Attr(&f, t, "MaxLife"), '*', G(AT, "minLifePercentage"))) > 0) {
			Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", G(AT, "traitorius"), G(AT, "pd"));
			Sf_Run(&f, "Scrap", "Set", "(si)", "CounterType", 2);
		}
		Lv_Set0(&f, "RemainingLifeScorer", "Value", Sf_Attr(&f, G(AT, "traitorius"), "Life"));
	}
	return Sf_None(&f);
}

/* TimeExpired(): the minute is over: no scorer, no more hits counted, Traitorius gives up */
static PyObject *Traitorius_TimeExpired(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Menu");
	Lv_ScorerCall(&f, Py_None);
	Sf_DrawScorer(&f);
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Traitorius", "OnDamage", "");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Traitorius", "OnDeath", "");
	Lv_Com(&f, "Mission_AttackTraitorius_RemoteMessage4", "Traitorius", G(AT, "PreCallTraitorius"), NOARGS);
	return Sf_None(&f);
}

/* PreCallTraitorius(): frightened: a waypoint, his call, Traitorius back into the traffic */
static PyObject *Traitorius_PreCallTraitorius(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	SETSAVE("TraitoriusFrightened", "1");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToTraitorius_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Own(&f, AT, "CallTraitorius", "()");
	Sf_SetAttrS(&f, G(AT, "traitorius"), "TgType", "None");
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, G(AT, "traitorius"), "Name"), 0);
	return Sf_None(&f);
}

static PyObject *Traitorius_CallTraitorius(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PhoneCab(&f, "Mission_AttackTraitorius.UsePhoneCab", 1, AT);
	return Sf_None(&f);
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): the police warning while the alarm is on, else the talk with
 * Traitorius and DeepThroat */
static PyObject *Traitorius_UsePhoneCab(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	int alarm = Os_PhoneUse(&f, AT, usr, ch, opt, full);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", G(AT, "pc"), "Generics_StillPolice1_Functionary", "Traitorius");
	} else if (alarm == 0) {
		Sf_SpeechEnd(&f, G(AT, "SetNextMission"), NULL);
		Sf_Run(&f, "Speech", "NISpeech", "(s)", "Functionary");
		Sf_Run(&f, "Speech", "AutoAddNI", "(s)", "Mission_TalkToTraitorius_ComTraitorius_NI_1Msg");
		PyObject *fn = Sf_Mod(&f, "Speech", "AutoAddNI");
		Sf_CallKw(&f, fn, f.failed ? NULL : Py_BuildValue("(s)", "Mission_TalkToTraitorius_ComDeepThroat_NI_1Msg"),
			Sf_Build(&f, "{s:s}", "HeadName", "DeepThroat"));
		Sf_Run(&f, "Speech", "RunNI", "()");
		Sf_SetAttrS(&f, Sf_Entity(&f, ch), "UseFunc", "PhoneCab.UseAudioCom");
	}
	return Sf_None(&f);
}

/* SetNextMission(): the cab free again, on to Mission_BeatPickus in Police */
static PyObject *Traitorius_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Police",
		"Journalist.Betty_Defaut", "Police", "Journalist.Berto_Defaut", "TownHall", NULL };
	SF_ARGS("");
	Lv_PcDefault(&f, AT);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "TraitoriusFrightened");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_BeatPickus", "Police");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_FlowerPower_MissionText1", 1);
	Lv_Saves(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): Traitorius' life bar with the mark of the limit, the countdown */
static PyObject *Traitorius_MissionScorer(PyObject *self, PyObject *args) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_MissionText(&f, id, "RemainingLifeText", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 0), "Horatio", "Shadow",
		Lv_Label(&f, "Mission_AttackTraitorius_Scorer_1"), "Left", Sf_Int(&f, 1));
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "MinimunLifeScorer", "Sprite");
	Lv_ScorerSet(&f, id, "MinimunLifeScorer", "Red", Sf_Int(&f, 0));
	Lv_ScorerSet(&f, id, "MinimunLifeScorer", "Green", Sf_Int(&f, 255));
	Lv_ScorerSet(&f, id, "MinimunLifeScorer", "Blue", Sf_Int(&f, 0));
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "RemainingLifeScorer", "Bar");
	Lv_BarDefaults(&f, id, "RemainingLifeScorer", NULL, NULL);
	Lv_ScorerSet(&f, id, "RemainingLifeScorer", "MaxValue", Sf_Attr(&f, G(AT, "traitorius"), "MaxLife"));
	Lv_ScorerSet(&f, id, "RemainingLifeScorer", "Value", Sf_Attr(&f, G(AT, "traitorius"), "Life"));
	Lv_ScorerSet(&f, id, "RemainingLifeScorer", "Visible", Sf_Int(&f, 1));
	PyObject *w = Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, "RemainingLifeScorer", "W");
	PyObject *h = Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, "RemainingLifeScorer", "H");
	PyObject *x = Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, "RemainingLifeScorer", "X");
	Lv_ScorerSet(&f, id, "MinimunLifeScorer", "X", Sf_Op(&f, x, '+', Sf_Op(&f, w, '*', G(AT, "minLifePercentage"))));
	Lv_ScorerSet(&f, id, "MinimunLifeScorer", "Y", Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, "RemainingLifeScorer", "Y"));
	Lv_ScorerSet(&f, id, "MinimunLifeScorer", "ScaleX", Sf_Float(&f, 1.0 / 2));
	Lv_ScorerSet(&f, id, "MinimunLifeScorer", "ScaleY", Sf_OpI(&f, h, '/', 4));
	Lv_ScorerSet(&f, id, "MinimunLifeScorer", "Visible", Sf_Int(&f, 1));
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "MinimunLifeTimer", "Timer");
	Lv_ScorerSet(&f, id, "MinimunLifeTimer", "X", Lv_TextX(&f, 40));
	Lv_ScorerSet(&f, id, "MinimunLifeTimer", "Y", Lv_TextY(&f, 60));
	Lv_ScorerSet(&f, id, "MinimunLifeTimer", "Align", Sf_Str(&f, "Left"));
	Lv_ScorerSet(&f, id, "MinimunLifeTimer", "TimerType", Sf_Str(&f, "TimeSpecialUse"));
	Lv_ScorerSet(&f, id, "MinimunLifeTimer", "OnTimeExpired", Sf_Str(&f, "Mission_AttackTraitorius.TimeExpired"));
	for (int i = 0; i < 4; i++) {
		Lv_ScorerSet(&f, id, "MinimunLifeTimer", rgba[i], Sf_Item(&f, Sf_Mod(&f, "Scorer", "MissionScorerText_Color"), i));
	}
	Lv_ScorerSet(&f, id, "MinimunLifeTimer", "Visible", Sf_Int(&f, 1));
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsAttackTraitoriusMethods[] = {
	{ "GetTraitoriusProfile", Traitorius_GetTraitoriusProfile, 1, NULL },
	{ "InitStatus", Traitorius_InitStatus, 1, NULL },
	{ "Init", Traitorius_Init, 1, NULL },
	{ "ReassignTarget", Traitorius_ReassignTarget, 1, NULL },
	{ "CreateTraitorius", Traitorius_CreateTraitorius, 1, NULL },
	{ "OnTraitoriusDamage", Traitorius_OnTraitoriusDamage, 1, NULL },
	{ "OnTraitoriusDeath", Traitorius_OnTraitoriusDeath, 1, NULL },
	{ "TraitoriusTakesLife", Traitorius_TraitoriusTakesLife, 1, NULL },
	{ "TimeExpired", Traitorius_TimeExpired, 1, NULL },
	{ "PreCallTraitorius", Traitorius_PreCallTraitorius, 1, NULL },
	{ "CallTraitorius", Traitorius_CallTraitorius, 1, NULL },
	{ "UsePhoneCab", Traitorius_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Traitorius_SetNextMission, 1, NULL },
	{ "MissionScorer", Traitorius_MissionScorer, 1, NULL },
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

/* Init(LevelPath): back after an interruption, or the meeting point (DeepThroat's hint without a police ship) */
static PyObject *Hunt_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_SetGlobal(&f, FH, "numFunctionariesKilled", Sf_ToInt(&f, SAVE("NumFunctionariesKilled")));
	Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25);
	int back = Sf_Compare(&f, G(FH, "numFunctionariesKilled"), ">=", Sf_Int(&f, 1));
	if (back > 0) {
		Sf_Own(&f, FH, "ComeBack", "()");
	} else if (back == 0) {
		Sf_Own(&f, FH, "CreateWaypoint", "()");
		PyObject *e = USR();
		int police = Sf_True(&f, e);
		if (police > 0) {
			police = Sf_True(&f, Sf_Method(&f, e, "IsType", "(s)", "Car"));
		}
		if (police > 0) {
			police = Sf_True(&f, Sf_Own(&f, FH, "isPoliceShip", "(O)", e));
		}
		if (police == 0) {
			Lv_Com(&f, "Mission_FunctionaryHunting_RemoteMessage1", "DeepThroat", NULL, NULL);
		}
	}
	return Sf_None(&f);
}

static PyObject *Hunt_CreateWaypoint(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Os_StartWaypoint(&f, FH, "DM_Mission_FunctionaryHunting", "No existe el dummy para esta misi\xf3n: 'DM_Mission_FunctionaryHunting'",
		"Mission_FunctionaryHunting.OnMissionStart");
	return Sf_None(&f);
}

static PyObject *Hunt_EnableMissionWaypoint(PyObject *self, PyObject *args) {
	(void)self;
	return Os_EnableWaypoint(args, FH, "Mission_FunctionaryHunting.OnMissionStart");
}

/* MissionScorer(id): the functionaries killed of all */
static PyObject *Hunt_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Sf_MissionText(&f, id, "RemainingCharsText", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 0), "Horatio", "Shadow",
		Lv_Label(&f, "Mission_FunctionaryHunting_Scorer_1"), "Left", NULL);
	Sf_MissionText(&f, id, "RemainingCharsScorer", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 20), "Horatio", "Shadow",
		Lv_Count(&f, FH, "numFunctionariesKilled", "FUNCTIONARIES_TO_KILL"), "Left", NULL);
	return Sf_None(&f);
}

/* every ship of the list taken out of it and deleted */
static byte Fh_Clear(scriptFrame_t *f, const char *list) {
	for (;;) {
		int mark = f->n;
		PyObject *e = Os_First(f, list);
		if (Sf_True(f, e) != 1) {
			break;
		}
		Sf_SetAttrS(f, e, "EntityList", "");
		Sf_Method(f, e, "Del", "()");
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* ComeBack(): the mission interrupted: no new functionaries nor scorer, the friends and enemies gone, the meeting
 * point again */
static PyObject *Hunt_ComeBack(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CreateNewFuncShip");
	Lv_ScorerCall(&f, Py_None);
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 0);
	Lv_Com(&f, "Mission_FunctionaryHunting_RemoteMessage5", "Police", NULL, NULL);
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(FH, "OnChangeVehicle"));
	Fh_Clear(&f, "Friends");
	Fh_Clear(&f, "Enemies");
	Sf_Own(&f, FH, "CreateWaypoint", "()");
	return Sf_None(&f);
}

/* OnChangeVehicle(): not a police ship: DeepThroat's hint */
static PyObject *Hunt_OnChangeVehicle(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	PyObject *v = USR();
	if (Sf_True(&f, Sf_Own(&f, FH, "isPoliceShip", "(O)", v)) == 0) {
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
		Lv_Com(&f, "Mission_FunctionaryHunting_RemoteMessage6", "DeepThroat", NULL, NULL);
	}
	return Sf_None(&f);
}

/* "<prefix>" + `(i + 1)` + "_Fighter" at the nearest free spawn point with the profile of the mission */
static PyObject *Fh_Fighter(scriptFrame_t *f, const char *prefix, PyObject *i, const char *profile) {
	PyObject *name = Sf_Cat(f, "sOs", prefix, Sf_Repr(f, Sf_OpI(f, i, '+', 1)), "_Fighter");
	PyObject *spw = Sf_Ask(f, "OutMap", "GetNearestFreeSpawnPos", "()");
	PyObject *e = Lv_CreateAt(f, name, spw, "Car");
	Lv_StdAI(f, e, Sf_Own(f, FH, profile, "()"));
	return e;
}

/* CreateNewPoliShip(i, pos, targetPos): a police friend at pos facing targetPos; its name */
static PyObject *Hunt_CreateNewPoliShip(PyObject *self, PyObject *args) {
	PyObject *i, *pos, *target;
	SF_ARGS("OOO", &i, &pos, &target);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "OutMap");
	PyObject *poli = Fh_Fighter(&f, "Poli", i, "GetPoliceFriendsProfile");
	Sf_SetAttr(&f, poli, "Pos", pos);
	Sf_SetAttrI(&f, poli, "Life", 40);
	Sf_SetAttrI(&f, poli, "MinLife", 40);
	Sf_SetAttrI(&f, poli, "MaxLife", 40);
	Sf_SetAttrS(&f, poli, "WeapList", "0,1,1,1,0,0,1");
	Sf_SetAttr(&f, poli, "Style", Sf_OpI(&f, Sf_Attr(&f, poli, "Style"), '-', 500));
	Sf_SetAttrS(&f, poli, "TgType", "green");
	Sf_Run(&f, "RaceMaker", "PointObjectTo", "(OO)", poli, target);
	return Sf_Return(&f, Sf_Attr(&f, poli, "Name"));
}

/* CreateNewFuncShip(i): another functionary enemy unless enough are there for the kills left */
static PyObject *Hunt_CreateNewFuncShip(PyObject *self, PyObject *args) {
	PyObject *i;
	SF_ARGS("O", &i);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *num = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Enemies", "Num");
	PyObject *left = Sf_Op(&f, G(FH, "FUNCTIONARIES_TO_KILL"), '-', G(FH, "numFunctionariesKilled"));
	if (Sf_Compare(&f, num, ">=", left) != 0) {
		return Sf_None(&f);
	}
	PyObject *fn = Fh_Fighter(&f, "Func", i, "GetFuncEnemiesProfile");
	Lv_Life(&f, fn, FH, "FUNCS_LIFE");
	Sf_SetAttrS(&f, fn, "WeapList", "0,1,1,1,0,0,1");
	Sf_SetAttr(&f, fn, "Style", Sf_OpI(&f, Sf_Attr(&f, fn, "Style"), '-', 500));
	Sf_SetAttr(&f, fn, "Ang", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetAttrS(&f, fn, "TgType", "red");
	Sf_SetAttrS(&f, fn, "EntityList", "Enemies");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", fn, 1);
	return Sf_None(&f);
}

/* the four corners around center at dist: (x +- dist, y, z +- dist) */
static PyObject *Fh_Corners(scriptFrame_t *f, PyObject *center, long dist) {
	static const int sx[4] = { 1, 1, -1, -1 }, sz[4] = { 1, -1, 1, -1 };
	PyObject *c[4];
	for (int i = 0; i < 4; i++) {
		PyObject *x = Sf_OpI(f, Sf_Item(f, center, 0), sx[i] > 0 ? '+' : '-', dist);
		PyObject *y = Sf_Item(f, center, 1);
		PyObject *z = Sf_OpI(f, Sf_Item(f, center, 2), sz[i] > 0 ? '+' : '-', dist);
		c[i] = f->failed ? NULL : Sf_Build(f, "(OOO)", x, y, z);
	}
	return f->failed ? NULL : Sf_Build(f, "[OOOO]", c[0], c[1], c[2], c[3]);
}

/* CreateFriendsShips(center): four police friends around the meeting point fly in towards it */
static PyObject *Hunt_CreateFriendsShips(PyObject *self, PyObject *args) {
	PyObject *center;
	SF_ARGS("O", &center);
	PyObject *initial = Fh_Corners(&f, center, 5000);
	PyObject *final = Fh_Corners(&f, center, 2000);
	for (int i = 0; i < 4 && !f.failed; i++) {
		int mark = f.n;
		PyObject *name = Sf_Own(&f, FH, "CreateNewPoliShip", "(iOO)", i, Sf_Item(&f, initial, i), center);
		Sf_Method(&f, G(FH, "initialFriendsList"), "append", "(O)", name);
		PyObject *p = Sf_Item(&f, final, i);
		PyObject *x = Sf_Item(&f, p, 0), *y = Sf_Item(&f, p, 1), *z = Sf_Item(&f, p, 2);
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOOOiiddd)", 1, name, x, y, z, 450, 0, 0.1, 20.0, 20.0);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* CreateEnemiesShips(): up to five functionaries at once */
static PyObject *Hunt_CreateEnemiesShips(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_SetAttrI(&f, Sf_GetItem(&f, Sf_Mod(&f, "VehiclesNPC", "AIProf"), Sf_Str(&f, "SFunc1")), "CadenceShoot", 6);
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_FunctionaryHunting.OnFuncDeath");
	PyObject *n = Sf_Op(&f, G(FH, "FUNCTIONARIES_TO_KILL"), '-', G(FH, "numFunctionariesKilled"));
	if (Sf_Compare(&f, n, ">=", Sf_Int(&f, 5)) > 0) {
		n = Sf_Int(&f, 5);
	}
	PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(O)", n), *i;
	for (int k = 0; (i = Sf_Next(&f, range, k)) != NULL; k++) {
		int mark = f.n;
		Sf_Own(&f, FH, "CreateNewFuncShip", "(O)", i);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* SetNextMission(): the characters' places, on to Mission_PoliceBossCheats, the police blueprints, Rusty and the
 * crazy gambler afterwards */
static PyObject *Hunt_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "Police", "Journalist.Berto_Defaut", "Police", NULL };
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumFunctionariesKilled");
	Lv_Saves(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_PoliceBossCheats", "Police");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToFicusFriend_MissionText1", 1);
	Lv_SaveCat(&f, FH, "PoliceBlueprints.Ships", "[sss]", "SPoli2", "SPoli3", "SPoli4");
	Lv_SaveCat(&f, FH, "PoliceBlueprints.Engines", "[sss]", "MPoli2", "MPoli3", "MPoli4");
	Sf_Import(&f, "Scorer");
	PyObject *fn = Sf_Mod(&f, "Speech", "AddRemoteCom");
	Sf_Later(&f, Sf_Float(&f, 10.0), fn, Sf_Build(&f, "(ssO(ii))", "Mission_FunctionaryHunting_RemoteMessage8", "Rusty",
		Sf_Mod(&f, "Scorer", "ShowMissionScorer"), 0, 2), NULL);
	Sf_Later(&f, Sf_Int(&f, 2), G(FH, "CGCalls"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CGCalls(): the crazy gambler's message by the number of crazy deals done (not during the twelfth one) */
static PyObject *Hunt_CGCalls(PyObject *self, PyObject *args) {
	static const char *const keys[] = { "Mission_TalkToFicusFriend_RemoteMessage1", "Mission_TalkToFicusFriend_RemoteMessage2",
		"Mission_TalkToFicusFriend_RemoteMessage3" };
	SF_ARGS("");
	if (Sf_Compare(&f, SAVE("SuperDeal.Library"), "!=", Sf_Str(&f, "SuperDeal_Twelfth")) > 0) {
		PyObject *num = Sf_Ask(&f, "MissionsFuncs", "GetNumCrazyDealCompleted", "()");
		for (int i = 0; i < 3; i++) {
			int is = Sf_EqI(&f, num, i);
			if (is != 0) {
				if (is > 0) {
					Lv_Com(&f, keys[i], "CrazyGambler", NULL, NULL);
				}
				break;
			}
		}
	}
	return Sf_None(&f);
}

/* OnFuncDeath(LooserName, WinnerName): one more killed; another functionary later, or the end: the friends back to
 * the police, the enemies to the traffic, the police's message */
static PyObject *Hunt_OnFuncDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_SetGlobal(&f, FH, "numFunctionariesKilled", Sf_OpI(&f, G(FH, "numFunctionariesKilled"), '+', 1));
	int more = Sf_Compare(&f, G(FH, "numFunctionariesKilled"), "<", G(FH, "FUNCTIONARIES_TO_KILL"));
	if (more > 0) {
		Lv_Set0(&f, "RemainingCharsScorer", "Text", Lv_Count(&f, FH, "numFunctionariesKilled", "FUNCTIONARIES_TO_KILL"));
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumFunctionariesKilled", Sf_Repr(&f, G(FH, "numFunctionariesKilled")));
		Sf_Later(&f, Sf_Int(&f, 3), G(FH, "CreateNewFuncShip"), Sf_Build(&f, "(i)", 0), "CreateNewFuncShip");
		return Sf_None(&f);
	}
	if (more < 0) {
		return Sf_None(&f);
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 0);
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
	Lv_ScorerCall(&f, Py_None);
	Sf_DrawScorer(&f);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CreateNewFuncShip");
	PyObject *e = Os_First(&f, "Friends");
	int mark = f.n;
	while (Sf_True(&f, e) > 0) {
		PyObject *next;
		if (Sf_EqS(&f, Sf_Slice(&f, Sf_Attr(&f, e, "MPil0"), 2, 0x7fffffff), "Police") > 0) {
			Sf_SetAttrS(&f, e, "EntityList", "OutCop");
			Sf_SetAttrS(&f, e, "TgType", "none");
			next = Os_First(&f, "Friends");
		} else {
			next = Sf_Entity(&f, Sf_Attr(&f, e, "NextInList"));
		}
		Sf_ReleaseKeep(&f, mark, &next, 1);
		e = next;
	}
	e = Os_First(&f, "Enemies");
	mark = f.n;
	while (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "EntityList", "Traffic");
		PyObject *next = Os_First(&f, "Enemies");
		Sf_ReleaseKeep(&f, mark, &next, 1);
		e = next;
	}
	Lv_Com(&f, "Mission_FunctionaryHunting_RemoteMessage7", "Police", G(FH, "SetNextMission"), NULL);
	return Sf_None(&f);
}

/* isPoliceShip(v): string.find(string.upper(v.Model), "POLI") != -1 */
static PyObject *Hunt_isPoliceShip(PyObject *self, PyObject *args) {
	PyObject *v;
	SF_ARGS("O", &v);
	PyObject *model = Sf_Ask(&f, "string", "upper", "(O)", Sf_Attr(&f, v, "Model"));
	int is = Sf_Compare(&f, Sf_Ask(&f, "string", "find", "(Os)", model, "POLI"), "!=", Sf_Int(&f, -1));
	return Sf_Return(&f, is < 0 ? NULL : Sf_Int(&f, is));
}

/* OnMissionStart(ItemName, VehicleName): the player at the meeting point: in a police ship, without alarm and
 * enemies, the friends come and the camera shows them */
static PyObject *Hunt_OnMissionStart(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	Sf_Import(&f, "Parking");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1 || Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", veh) != 0) {
		return Sf_None(&f);
	}
	PyObject *v = Sf_Entity(&f, veh);
	int police = Sf_True(&f, Sf_Own(&f, FH, "isPoliceShip", "(O)", v));
	if (police == 0) {
		Sf_SetAttrS(&f, G(FH, "missionStartWaypoint"), "OnTake", "");
		Lv_Com(&f, "Mission_FunctionaryHunting_RemoteMessage4", "DeepThroat", G(FH, "EnableMissionWaypoint"), NULL);
		return Sf_None(&f);
	}
	int busy = Sf_True(&f, Sf_Ask(&f, "SLogic", "IsEnemyActive", "()"));
	if (busy == 0) {
		busy = Lv_Alarm(&f);
	}
	if (busy > 0) {
		Sf_Print(&f, "s", "No deberia entrar aqui por segunda vez");
		Sf_SetAttrS(&f, G(FH, "missionStartWaypoint"), "OnTake", "");
		Lv_Com(&f, "Mission_FunctionaryHunting_RemoteMessage3", "Police", G(FH, "EnableMissionWaypoint"), NULL);
		return Sf_None(&f);
	}
	Sf_SetAttrI(&f, G(FH, "missionStartWaypoint"), "Active", 0);
	Sf_Own(&f, FH, "CreateFriendsShips", "(O)", Sf_Attr(&f, G(FH, "missionStartWaypoint"), "Pos"));
	Sf_Own(&f, FH, "StartCameraTravelling", "()");
	return Sf_None(&f);
}

/* LaunchMission(): the alarm locked, the scorer, the meeting point gone, the enemies, the friends in their list */
static PyObject *Hunt_LaunchMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 1);
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(FH, "ComeBack"));
	Sf_Own(&f, FH, "MissionScorer", "(i)", 0);
	Lv_ScorerCall(&f, G(FH, "MissionScorer"));
	Sf_Method(&f, G(FH, "missionStartWaypoint"), "Del", "()");
	Sf_SetGlobal(&f, FH, "missionStartWaypoint", Py_None);
	Sf_Own(&f, FH, "CreateEnemiesShips", "()");
	PyObject *name;
	for (int k = 0; (name = Sf_Next(&f, G(FH, "initialFriendsList"), k)) != NULL; k++) {
		int mark = f.n;
		PyObject *e = Sf_Entity(&f, name);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrS(&f, e, "EntityList", "Friends");
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* a waypoint of the camera path: TgtCam0n set to the previous camera and then to "" (as the script does), Pos, Ang,
 * the fades */
static byte Fh_CamPoint(scriptFrame_t *f, PyObject *cam, char n, PyObject *prev, PyObject *pos, PyObject *ang, int in, int out) {
	char var[16];
	Str_Printf(var, sizeof(var), "TgtCam0%c", n);
	Sf_SetAttr(f, cam, var, prev);
	Sf_SetAttrS(f, cam, var, "");
	Str_Printf(var, sizeof(var), "Pos0%c", n);
	Sf_SetAttr(f, cam, var, pos);
	Str_Printf(var, sizeof(var), "Ang0%c", n);
	Sf_SetAttr(f, cam, var, ang);
	Str_Printf(var, sizeof(var), "FadeIn0%c", n);
	Sf_SetAttrI(f, cam, var, in);
	Str_Printf(var, sizeof(var), "FadeOut0%c", n);
	return Sf_SetAttrI(f, cam, var, out);
}

/* StartCameraTravelling(): the player stops at the meeting point, a camera path towards it shows the friends */
static PyObject *Hunt_StartCameraTravelling(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	int busy = Sf_True(&f, usr);
	if (busy > 0) {
		busy = Sf_Compare(&f, Sf_Attr(&f, usr, "UsrControl"), "!=", Sf_Int(&f, 0));
	}
	if (busy != 0) {
		return Sf_None(&f);
	}
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Menu", "MovieScorer", "(i)", 0);
	Sf_SetAttrI(&f, usr, "UsrControl", -1);
	Sf_SetAttr(&f, usr, "Vel", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetAttr(&f, usr, "MovePos", Sf_Attr(&f, G(FH, "missionStartWaypoint"), "Pos"));
	Sf_SetAttr(&f, usr, "MoveAng", Sf_Attr(&f, G(FH, "missionStartWaypoint"), "Ang"));
	Sf_SetAttr(&f, usr, "MoveTime", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, usr, "MoveFade", 2);
	Sf_SetAttrI(&f, usr, "AvoidCrash", 1);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "MainMissionEvent", "");
	PyObject *prev = Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0);
	PyObject *ang = Sf_Ask(&f, "SVec", "Add", "(O(iii))", Sf_Attr(&f, G(FH, "missionStartWaypoint"), "Ang"), 15, 0, 0);
	PyObject *r = Sf_Ask(&f, "SVec", "Rotate3D", "((iii)O)", 0, 0, -8000, ang);
	PyObject *p0 = Sf_Ask(&f, "SVec", "Add", "(OO)", r, Sf_Attr(&f, G(FH, "missionStartWaypoint"), "Pos"));
	r = Sf_Ask(&f, "SVec", "Rotate3D", "((iii)O)", 0, 0, -2000, ang);
	PyObject *p1 = Sf_Ask(&f, "SVec", "Add", "(OO)", r, Sf_Attr(&f, G(FH, "missionStartWaypoint"), "Pos"));
	PyObject *cam = Lv_CreateAt(&f, Sf_Str(&f, "RaceCamera"), p0, "CamPath");
	Sf_SetAttrI(&f, cam, "nWayPoint", 2);
	Fh_CamPoint(&f, cam, '0', prev, p0, ang, 1, 0);
	Sf_SetAttr(&f, cam, "Time00", Sf_Float(&f, 5.0));
	PyObject *a1 = f.failed ? NULL : Sf_Build(&f, "(OOO)", Sf_Op(&f, Sf_Item(&f, ang, 0), '/', Sf_Float(&f, 4.0)), Sf_Item(&f, ang, 1),
		Sf_Item(&f, ang, 2));
	Fh_CamPoint(&f, cam, '1', prev, p1, a1, 0, 1);
	Sf_SetAttrI(&f, cam, "Time", 0);
	Sf_Run(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cam, "Name"));
	PyObject *tt = Sf_Attr(&f, cam, "Time00");
	PyObject *at = Sf_Op(&f, Sf_Op(&f, Sf_Time(&f), '+', tt), '+', Sf_Float(&f, 0.2));
	Sf_Schedule(&f, at, G(FH, "AbortCameraTravelling"), f.failed ? NULL : Sf_Build(&f, "(OO)", Sf_Attr(&f, usr, "Name"), prev), NULL);
	Sf_Import(&f, "Scene");
	Sf_Run(&f, "Scene", "SetText", "(sO)", "Mission_FunctionaryHunting_RemoteMessage2", tt);
	return Sf_None(&f);
}

/* AbortCameraTravelling(UsrEntityName, prevCamera): the player and the menus back, the camera back, the race scorer;
 * the mission starts */
static PyObject *Hunt_AbortCameraTravelling(PyObject *self, PyObject *args) {
	PyObject *name, *prev;
	SF_ARGS("OO", &name, &prev);
	if (Sf_True(&f, prev) > 0) {
		PyObject *usr = Sf_Entity(&f, name);
		if (Sf_True(&f, usr) > 0) {
			Sf_SetAttr(&f, usr, "MoveTime", Sf_Float(&f, 0.0));
			Sf_SetAttrI(&f, usr, "UsrControl", 0);
		}
		Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "Menu.Init");
		Sf_Run(&f, "Scrap", "Set", "(ss)", "MainMissionEvent", "MissionsFuncs.ShowMissionInfo");
		Sf_Method(&f, Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0)), "Del", "()");
		Sf_Run(&f, "Scrap", "SetCam", "(iO)", 0, prev);
		Sf_Import(&f, "RacerScorer");
		Sf_Run(&f, "RacerScorer", "Create", "(i)", 0);
	}
	Sf_Own(&f, FH, "LaunchMission", "()");
	return Sf_None(&f);
}

PyMethodDef g_OutskirtsFunctionaryHuntingMethods[] = {
	{ "GetFuncEnemiesProfile", Hunt_GetFuncEnemiesProfile, 1, NULL },
	{ "GetPoliceFriendsProfile", Hunt_GetPoliceFriendsProfile, 1, NULL },
	{ "InitStatus", Hunt_InitStatus, 1, NULL },
	{ "Init", Hunt_Init, 1, NULL },
	{ "CreateWaypoint", Hunt_CreateWaypoint, 1, NULL },
	{ "EnableMissionWaypoint", Hunt_EnableMissionWaypoint, 1, NULL },
	{ "MissionScorer", Hunt_MissionScorer, 1, NULL },
	{ "ComeBack", Hunt_ComeBack, 1, NULL },
	{ "OnChangeVehicle", Hunt_OnChangeVehicle, 1, NULL },
	{ "CreateNewPoliShip", Hunt_CreateNewPoliShip, 1, NULL },
	{ "CreateNewFuncShip", Hunt_CreateNewFuncShip, 1, NULL },
	{ "CreateFriendsShips", Hunt_CreateFriendsShips, 1, NULL },
	{ "CreateEnemiesShips", Hunt_CreateEnemiesShips, 1, NULL },
	{ "SetNextMission", Hunt_SetNextMission, 1, NULL },
	{ "CGCalls", Hunt_CGCalls, 1, NULL },
	{ "OnFuncDeath", Hunt_OnFuncDeath, 1, NULL },
	{ "isPoliceShip", Hunt_isPoliceShip, 1, NULL },
	{ "OnMissionStart", Hunt_OnMissionStart, 1, NULL },
	{ "LaunchMission", Hunt_LaunchMission, 1, NULL },
	{ "StartCameraTravelling", Hunt_StartCameraTravelling, 1, NULL },
	{ "AbortCameraTravelling", Hunt_AbortCameraTravelling, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
