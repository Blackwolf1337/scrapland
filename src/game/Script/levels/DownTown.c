/*
 * The scripts of the level DownTown in C (levels/downtown/scripts of Data.packed, issue #2): the Map module (the
 * traffic by who controls the robots, the metro and the elevators to the townhall and the press), MapRace (the start
 * positions of the sixteen races) and the missions of the level (Mission_FirstContactPress, Mission_BertoMassacre,
 * Mission_DefeatCrazyGambler1, Mission_FaceTheMayor, Mission_Massacre, Mission_DestroyCommSats3 (in
 * levels/LevelCommon.c), Mission_DistractHumphrey, Mission_GetDataPart1, Mission_HumphreyRace, Mission_MultiplusRace,
 * Mission_ProtectMayorTransport, Mission_StealMayorShip). MapSnd is data (data/levels/downtown/mapsnd.def), and so
 * are the plain functions of the missions. The module variables of the missions (their counters, states, names and
 * constants) and the module code (the save variables they create, MapRace.Races) stay in the scripts; the functions
 * read and write them as their globals.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h) and call the engine in C
 * (save variables, Scrap.Get, the alarm, the entity lists, the scorer items, the vector math, the money). The variables
 * of the modules are read where the script reads them and written through at once; the lists of the transports, the
 * functionaries and the taunts are changed in place on the objects of the modules (OBJLIB 5). The ships, the items,
 * the phone cabs and the cameras are handle values, the profiles VehiclesNPC.cShipAI instances (OBJLIB 7). The engine
 * functions the cases record or swap go through their seams (OBJLIB 7): Scrap.AddScheduledFunc, DeleteScheduledFuncs,
 * DelSaveVars, CreateEntity, Set, SetAlarm, UsrEntity, Rand and GetAlarm, SScorer.Set, SetMsgText and SAct. By name on
 * purpose (OBJLIB 7): SAI.SetStateVehicle (its state handlers take Python arguments), the calls the cases record with
 * _bon (SScorer.CancelSpeech, SetSpeechCallback, AddModel, SSound.StopVoice, Scrap.IncSaveVar), the camera (Scrap.GetCam,
 * SetCam), SAI.GetOD, SetInertia and SetRotStaticObj, SLogic.IsEnemyActive and GetNearestShip, Scrap.GetFarestParked,
 * CreateEntityList, SInput.GetDefinedEntry and the call function of the inferno fire (Scrap.SetCallFunc, AddParams,
 * Execute: static in ScrapPy.c). Speech, MissionsFuncs, SaveGame, RaceMaker, RacerScorer, VehiclesNPC, Vehicles,
 * PhoneCab, Parking, Elevator, Metro, Traffic, Items, Scene, Menu, Scorer, string and the functions of the modules go
 * by name (OBJLIB 3); the keyword calls (RaceMaker.InitRace, Speech.AutoAddNI with HeadName) stay Python calls.
 */
#include <limits.h>

#include "Levels.h"

#define FC "Mission_FirstContactPress"
#define BM "Mission_BertoMassacre"
#define CG "Mission_DefeatCrazyGambler1"
#define FM "Mission_FaceTheMayor"
#define MS "Mission_Massacre"
#define DH "Mission_DistractHumphrey"
#define GD "Mission_GetDataPart1"
#define HR "Mission_HumphreyRace"
#define MR "Mission_MultiplusRace"
#define PM "Mission_ProtectMayorTransport"
#define SM "Mission_StealMayorShip"

/* ---- native lines of the level (Levels.h) ---- */

/* a value of the function's modeVals_t t (None) */
#define TV() Mode_V(&t)

/* the same with Speech.AddRemoteCom, and with a float delay */
/* Rusty's remote com key 10 s later, then Scorer.ShowMissionScorer(0, 2) */
static byte Dt_RustyLater(const char *key) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *args = TV(), *rusty = TV(), *show = TV(), *pair = TV();
	LvN_Str(args, key);
	LvN_Str(rusty, "Rusty");
	Native_SetEmpty(pair, NV_TUPLE);
	Native_SetInt(Native_Append(pair), 0);
	Native_SetInt(Native_Append(pair), 2);
	byte ok = Mode_Import("Scorer") && LvN_ModGet("Scorer", "ShowMissionScorer", show) && Mode_LaterComF(10.0, args, 4);
	return Mode_Done(&t, ok);
}

/*
 * UsePhoneCab of a mission (UsrName, CharName, optId, FullOption in a): the audio com of the phone cab into the global
 * pc of module, the police warning police of who while the alarm is on, else the NI talk key of who with the end
 * function end of module (reset: the cab's use back to the default first)
 */
static byte Dt_UsePhone(const nativeVal_t *a, const char *module, const char *police, const char *who, const char *key, const char *end,
	byte reset) {
	nativeVal_t pc = { 0 };
	Native_SetNone(&pc);
	int alarm = Mode_PhoneUse(module, a);
	byte ok = alarm >= 0;
	if (alarm > 0) {
		ok = Mode_G(module, "pc", &pc) && LV_CALL("Speech", "StartObjSpch", NULL, "vss", &pc, police, who);
	} else if (alarm == 0) {
		ok = (!reset || (Mode_G(module, "pc", &pc) && LvN_SetS(&pc, "UseFunc", "PhoneCab.DefUseAudioCom"))) && Mode_Read(module, "pc", &pc) &&
			LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &pc, 0L) && LvN_OnEnd(module, end, NULL) && Mode_NISpeech(who, key);
	}
	Native_Free(&pc);
	return ok;
}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors by the engine; the elevator by the map the player comes from, the metro; the
 * traffic by who controls the robots */
static byte DownTown_InitStatus(void) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "Traffic", "OutPolice", "string", "SAI", NULL };
	modeVals_t t = { .n = 0 };
	byte ok = 1;
	for (int i = 0; ok && mods[i]; i++) {
		ok = Mode_Import(mods[i]);
	}
	if (!ok || !LvN_ModSetI("Init", "Outdoor", 0)) {
		return 0;
	}
	nativeVal_t *od = TV(), *prev = TV(), *tail = TV(), *pm = TV();
	if (!LV_CALL("SAI", "GetOD", NULL, "") || !LV_CALL("SAI", "GetOD", od, "") || !Mode_SetModVar("Init", "Outdoor", od)) {
		/* except: pass (OBJLIB 7: the error is the Python exception until errors are a C state) */
		PyErr_Clear();
	}
	Mode_SaveVar("PrevMap", prev);
	ok = Native_Slice(prev, 7, INT_MAX, tail) && Native_Upper(tail, pm) &&
		LV_CALL("Elevator", "SetDef", NULL, "ss", "Elevator", LvN_EqS(pm, "PRESS") ? "DM_Door_Elevator_Press" : "DM_Door_Elevator_TownHall") &&
		LV_CALL("Elevator", "SetDef", NULL, "ss", "Metro", "DM_Door_MetroExt_Downtown") &&
		LV_CALL("Elevator", "SetDef", NULL, "ss", "MetroExt", "DM_Door_MetroExt_Downtown");
	if (ok && LvN_SaveIs("RobotsControlledByBoss", "1")) {
		ok = Mode_BossTraffic(1);
	} else if (ok) {
		ok = Mode_GVarSetI("iTrafficNumber", 14) && Mode_GVarSetI("MaxChasingShips", -1) && Mode_ModEmpty("Traffic", "AcHeavyShips") &&
			LvN_ModSetI("Traffic", "NumNormal", 80) && LvN_ModSetI("Traffic", "NumMedium", 20) && LvN_ModSetI("Traffic", "NumHeavy", 0);
	}
	return Mode_Done(&t, ok);
}

/* Init(LevelPath): the elevators to the townhall and the press, the metro */
static byte DownTown_Init(void) {
	return Mode_Import("Elevator") && Mode_Import("Metro") && Mode_Connect("DM_Door_Elevator_TownHall", "TownHall", 0) &&
		Mode_Connect("DM_Door_Elevator_Press", "Press", 0) && LV_CALL("Metro", "SetAsMetro", NULL, "s", "DM_Door_MetroExt_Downtown");
}

LV_CORE0(DownTown_InitStatus)
LV_CORE0(DownTown_Init)
LV_FN(DownTown, InitStatusPy, 1, DownTown_InitStatus_C)
LV_FN(DownTown, InitPy, 1, DownTown_Init_C)

PyMethodDef g_DownTownMapMethods[] = {
	{ "InitStatus", DownTown_InitStatusPy, 1, NULL },
	{ "Init", DownTown_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapRace ---- */

#include "DownTownRaces.inc"

#define RACE(k) \
	static PyObject *MapRace_Race##k(PyObject *self, PyObject *args) { \
		(void)self; \
		return Lv_StartPos(args, races[k - 1].pos, races[k - 1].n); \
	}
RACE(1)
RACE(2)
RACE(3)
RACE(4)
RACE(5)
RACE(6)
RACE(7)
RACE(8)
RACE(9)
RACE(10)
RACE(11)
RACE(12)
RACE(13)
RACE(14)
RACE(15)
RACE(16)

PyMethodDef g_DownTownMapRaceMethods[] = {
	{ "EasyRaceDownTown1", MapRace_Race1, 1, NULL },
	{ "EasyRaceDownTown2", MapRace_Race2, 1, NULL },
	{ "EasyRaceDownTown3", MapRace_Race3, 1, NULL },
	{ "EasyRaceDownTown4", MapRace_Race4, 1, NULL },
	{ "MediumRaceDownTown1", MapRace_Race5, 1, NULL },
	{ "MediumRaceDownTown2", MapRace_Race6, 1, NULL },
	{ "MediumRaceDownTown3", MapRace_Race7, 1, NULL },
	{ "MediumRaceDownTown4", MapRace_Race8, 1, NULL },
	{ "MediumRaceDownTown5", MapRace_Race9, 1, NULL },
	{ "MediumRaceDownTown6", MapRace_Race10, 1, NULL },
	{ "MediumRaceDownTown7", MapRace_Race11, 1, NULL },
	{ "MediumRaceDownTown8", MapRace_Race12, 1, NULL },
	{ "HardRaceDownTown1", MapRace_Race13, 1, NULL },
	{ "HardRaceDownTown2", MapRace_Race14, 1, NULL },
	{ "HardRaceDownTown3", MapRace_Race15, 1, NULL },
	{ "HardRaceDownTown4", MapRace_Race16, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FirstContactPress: Berto calls, on to the press ---- */

static PyObject *Press_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, FC,
		"[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SMerc3\",\"SLMerc\",\"SMayor1\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SHump1\","
		"\"SBetty1\",\"SBankMaster1\",\"SArchbishop1\"]");
}

/* Init(LevelPath): Berto's call, then on to the press; C although the converter made it data, because the .def
 * reader keeps the quotes of the texts in the tuple of parameters ("Mission_MeetBoss" with its quotes) */
static byte Press_Init(void) {
	nativeVal_t p = { 0 };
	if (!Mode_Import("MissionsFuncs") || !Mode_Import("Speech")) {
		return 0;
	}
	Native_SetEmpty(&p, NV_TUPLE);
	LvN_Str(Native_Append(&p), "Mission_MeetBoss");
	LvN_Str(Native_Append(&p), "Press");
	byte ok = LvN_Com("Mission_FirstContactPress_RemoteMessage1", "Berto", "MissionsFuncs", "SetNextMission", &p);
	Native_Free(&p);
	return ok;
}

LV_CORE0(Press_Init)
LV_FN(Press, InitPy, 1, Press_Init_C)

PyMethodDef g_DownTownFirstContactPressMethods[] = {
	{ "InitStatus", Press_InitStatus, 1, NULL },
	{ "Init", Press_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_BertoMassacre: the evil Bertos in waves; after the last of the bar one of them carries the matrix ---- */

/* InitStatus(LevelPath): no Bertos before DeepThroat's message */
static byte Berto_InitStatus(void) {
	return Mode_SaveS("CreateBertos", "0") && Mode_AcShips(BM, "[\"SFunc1\",\"SHump1\",\"SBetty1\"]");
}

/* StartMission(): the scorer, the waves of Bertos and their callbacks */
static byte Berto_StartMission(void) {
	return Mode_Import("Parking") && Mode_MMDesc("Mission_BertoMassacre_MissionText1", 1) && LvN_ScorerCallG(BM, "MissionScorer") &&
		LvN_Draw() && LV_CALL("MissionsFuncs", "CreateBertos", NULL, "") &&
		LvN_ModSetGlobal("MissionsFuncs", "OnNewWaveCallback", BM, "DeepThroatMessage") &&
		LvN_ModSetGlobal("MissionsFuncs", "OnBertoDeathCallback", BM, "CheckBertoDeath") &&
		Mode_ModTo("Parking", "SwitchVehicleMissionCallbackAux", "MissionsFuncs", "ReassignBertosPlayerTarget") &&
		Mode_ModTo("Parking", "SwitchCharMissionCallbackAux", "MissionsFuncs", "ReassignBertosPlayerTarget");
}

/* DeepThroatMessage(): a new wave, the bar full again */
static byte Berto_DeepThroatMessage(void) {
	return LvN_PutI(BM, "initBar", 1) && LvN_Draw() && LvN_Com("Mission_BertoMassacre_RemoteMessage2", "DeepThroat", NULL, NULL, NULL);
}

/* CheckBertoDeath(LooserName, WinnerName, deathPos): the first death a message, the found Berto drops the matrix; a
 * Berto the player killed counts on the bar */
static byte Berto_CheckBertoDeath(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = TV(), *usr = TV();
	byte ok;
	int c = LvN_GTrue(BM, "firstDeath");
	if (c > 0) {
		ok = LvN_PutI(BM, "firstDeath", 0) && LvN_Com("Mission_BertoMassacre_RemoteMessage1", "DeepThroat", NULL, NULL, NULL);
	} else {
		c = c < 0 ? -1 : LvN_GTrue(BM, "bertoFound");
		if (c > 0) {
			c = Mode_G(BM, "bertoName", name) ? !Native_Compare(&a[0], name) : -1;
		}
		ok = c >= 0 && (!c || LV_OWN(BM, "DropMatrix", NULL, "v", &a[2]));
	}
	c = ok && Mode_UsrHandle(usr) ? LvN_UsrNameIs(usr, &a[1]) : -1;
	ok = c >= 0 && (!c || (LvN_PutI(BM, "bertoDeath", 1) && LvN_Draw()));
	return Mode_Done(&t, ok);
}

/* OnBertoFound(): no scorer, the order to destroy that Berto */
static byte Berto_OnBertoFound(void) {
	return LvN_ScorerOff() && LvN_Draw() && Mode_MMDesc("Mission_DestroyBerto_MissionText1", 1);
}

/* DropMatrix(pos): no more waves, the matrix of D-Tritus where the Berto died */
static byte Berto_DropMatrix(const nativeVal_t *a) {
	return LvN_ModNone("MissionsFuncs", "OnNewWaveCallback") && LvN_ModNone("MissionsFuncs", "OnBertoDeathCallback") &&
		Mode_Drop(BM, &a[0], "DTritusMatrix", "DTritusMatrixModel", "DTritusMatrixRadius", 1, "Mission_BertoMassacre.TakeMatrix",
			"TakeMatrixExt", "MissionCheckArrow");
}

/* TakeMatrix(ItemName, CharName) */
static byte Berto_TakeMatrix(void) {
	modeVals_t t = { .n = 0 };
	byte ok = Mode_Taken(Mode_S(&t, "DTritusMatrix"), Mode_S(&t, "MissionCheckArrow")) &&
		LvN_Com("Mission_DestroyBerto_RemoteMessage1", "DeepThroat", BM, "SetNextMission", NULL);
	return Mode_Done(&t, ok);
}

/* GetBerto(): the first Berto of the wave carries the matrix (tried again every second until there is one) */
static byte Berto_GetBerto(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cur = TV(), *name = TV(), *e = TV();
	int n = LvN_ModGet("MissionsFuncs", "currentBertos", cur) ? Native_Size(cur) : -1;
	byte ok = n >= 0;
	if (n > 0) {
		Native_Free(cur);
		ok = LvN_ModGet("MissionsFuncs", "currentBertos", cur) && LvN_Item(cur, 0, name) && Mode_Put(BM, "bertoName", NULL, name) &&
			Mode_G(BM, "bertoName", name) && Mode_EntityH(name, e) && (!Native_True(e) || LvN_SetI(e, "Money", -1)) &&
			Mode_G(BM, "bertoName", name) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name) &&
			LvN_Com("Mission_BertoMassacre_RemoteMessage3", "DeepThroat", BM, "OnBertoFound", NULL);
	} else if (n == 0) {
		ok = Mode_Later(1, BM, "GetBerto", NULL, 0, NULL);
	}
	return Mode_Done(&t, ok);
}

/* MissionScorer(id): the bar of the Bertos left before the one with the matrix */
static byte Berto_MissionScorer(const nativeVal_t *id) {
	static const char bar[] = "MassacreScorer";
	modeVals_t t = { .n = 0 };
	nativeVal_t *max = TV(), *v = TV(), *nv = TV();
	byte ok = Mode_Import("Scorer") && Mode_LvLabel(id, "MassacreText", 0, "Mission_BertoMassacre_Scorer1", 1) && Mode_ScorerAddS(id, bar, "Bar") &&
		Mode_G(BM, "MaxBarValue", max) && LvN_ScorerSet(id, bar, "MaxValue", max) && Mode_ScorerSetI(id, bar, "IsAutoUpdatable", 0) &&
		Mode_ScorerSetI(id, bar, "Visible", 1);
	int c = ok ? LvN_GTrue(BM, "initBar") : -1;
	ok = c >= 0 && (!c || (LvN_PutI(BM, "initBar", 0) && Mode_Read(BM, "MaxBarValue", max) && LvN_ScorerSet(id, bar, "Value", max)));
	c = ok ? LvN_GTrue(BM, "bertoFound") : -1;
	if (c == 0) {
		c = LvN_GTrue(BM, "bertoDeath");
		if (c > 0) {
			ok = LvN_PutI(BM, "bertoDeath", 0) && Mode_ScorerGetV(id, bar, "Value", v) && Native_Arith('-', v, Mode_I(&t, 1), nv);
			if (ok && Native_Compare(nv, Mode_I(&t, 0)) <= 0) {
				Native_Free(nv);
				Native_SetInt(nv, 0);
				ok = LvN_PutI(BM, "bertoFound", 1) && LvN_ModNone("MissionsFuncs", "OnNewWaveCallback") && LvN_ScorerOff() && LvN_Draw() &&
					LV_OWN(BM, "GetBerto", NULL, "");
			}
			ok = ok && LvN_ScorerSet(id, bar, "Value", nv);
		}
	}
	ok = ok && c >= 0 && Mode_BarDefaults(id, bar, NULL, NULL);
	return Mode_Done(&t, ok);
}

LV_CORE0(Berto_InitStatus)
LV_CORE0(Berto_StartMission)
LV_CORE0(Berto_DeepThroatMessage)
LV_COREA(Berto_CheckBertoDeath)
LV_CORE0(Berto_OnBertoFound)
LV_COREA(Berto_DropMatrix)
LV_CORE0(Berto_TakeMatrix)
LV_CORE0(Berto_GetBerto)
LV_COREA(Berto_MissionScorer)
LV_FN(Berto, InitStatusPy, 1, Berto_InitStatus_C)
LV_FN(Berto, StartMissionPy, 0, Berto_StartMission_C)
LV_FN(Berto, DeepThroatMessagePy, 0, Berto_DeepThroatMessage_C)
LV_FN(Berto, CheckBertoDeathPy, 3, Berto_CheckBertoDeath_C)
LV_FN(Berto, OnBertoFoundPy, 0, Berto_OnBertoFound_C)
LV_FN(Berto, DropMatrixPy, 1, Berto_DropMatrix_C)
LV_FN(Berto, TakeMatrixPy, 2, Berto_TakeMatrix_C)
LV_FN(Berto, GetBertoPy, 0, Berto_GetBerto_C)
LV_FN(Berto, MissionScorerPy, 1, Berto_MissionScorer_C)

PyMethodDef g_DownTownBertoMassacreMethods[] = {
	{ "InitStatus", Berto_InitStatusPy, 1, NULL },
	{ "StartMission", Berto_StartMissionPy, 1, NULL },
	{ "DeepThroatMessage", Berto_DeepThroatMessagePy, 1, NULL },
	{ "CheckBertoDeath", Berto_CheckBertoDeathPy, 1, NULL },
	{ "OnBertoFound", Berto_OnBertoFoundPy, 1, NULL },
	{ "DropMatrix", Berto_DropMatrixPy, 1, NULL },
	{ "TakeMatrix", Berto_TakeMatrixPy, 1, NULL },
	{ "GetBerto", Berto_GetBertoPy, 1, NULL },
	{ "MissionScorer", Berto_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DefeatCrazyGambler1: the crazy gambler comes back until he has lost three lives ---- */

static const char *const gambler_profile[] = {
	"Ship=SBANKMASTER1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=2", "UsePercSonic=50", "UsePercCloud=25",
	"UsePercElectro=25", "UsePercHook=15", "FactorHookMap=10", "CadenceShoot=15", "ControlAim=10", "AngAttack=3.14", "CounterAttack=1",
	"ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.45", "FactorDamageItemLife=0.35",
	"FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=20", "TimeMaxStrafe=10", "VelMaxStrafe=1.0", NULL,
};

static PyObject *Gambler_GetCrazyGamblerProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, CG, gambler_profile);
}

static PyObject *Gambler_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, CG, "[\"SFunc1\",\"SBerto1\",\"SBetty1\"]");
}

/* int(Scrap.GetSaveVar("NumCrazyGamblerLifes")) into out */
static byte Gambler_Lifes(nativeVal_t *out) {
	nativeVal_t s = { 0 };
	Mode_SaveVar("NumCrazyGamblerLifes", &s);
	Native_SetNone(out);
	byte ok = Native_ToInt(&s, out);
	Native_Free(&s);
	return ok;
}

/* lifes == CrazyGamblerLifesNextMission: 1, 0, -1 */
static int Gambler_Last(const nativeVal_t *lifes) {
	nativeVal_t n = { 0 };
	int r = Mode_G(CG, "CrazyGamblerLifesNextMission", &n) ? !Native_Compare(lifes, &n) : -1;
	Native_Free(&n);
	return r;
}

/* Init(LevelPath): the gambler and his taunts, or the next mission when he has lost enough lives */
static byte Gambler_Init(void) {
	nativeVal_t lifes = { 0 };
	int done = Gambler_Lifes(&lifes) ? Gambler_Last(&lifes) : -1;
	Native_Free(&lifes);
	if (done > 0) {
		return LV_OWN(CG, "PreSetNextMission", NULL, "");
	}
	return done == 0 &&
		LV_CALL("SScorer", "AddModel", NULL, "isssii", 0L, "EvilGambler", "Models/Heads/HeadEvilGambler/HeadEvilGambler", "", 4L, 20L) &&
		Mode_ListSet("Enemies", "OnDeath", "Mission_DefeatCrazyGambler1.OnCrazyGamblerDeath") && LV_OWN(CG, "CreateCrazyGambler", NULL, "") &&
		Mode_Later(3, CG, "LaunchTaunts", NULL, 0, NULL) && LvN_ScorerCallG(CG, "MissionScorer") && LvN_Draw();
}

/* LaunchTaunts(): a taunt of the five while the gambler lives, every 12 s */
static byte Gambler_LaunchTaunts(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = TV(), *e = TV(), *n = TV(), *s = TV(), *rep = TV(), *key = TV(), *n1 = TV();
	byte ok = Mode_G(CG, "crazyGamblerName", name) && Mode_EntityH(name, e);
	if (ok && Native_True(e)) {
		ok = Mode_G(CG, "numTaunt", n) && Native_Arith('+', Mode_I(&t, 2), n, s);
		if (ok) {
			Mode_Repr(s, rep);
		}
		ok = ok && Native_Arith('+', Mode_S(&t, "Mission_DefeatCrazyGambler1_RemoteMessage"), rep, key) &&
			LV_CALL("Speech", "RemoteComunication", NULL, "vs", key, "EvilGambler") && Mode_Read(CG, "numTaunt", n) &&
			Native_Arith('+', n, Mode_I(&t, 1), n1) && LvN_Arith('%', n1, Mode_I(&t, 5), s) && Mode_Put(CG, "numTaunt", NULL, s);
	}
	ok = ok && Mode_Later(12, CG, "LaunchTaunts", NULL, 0, NULL);
	return Mode_Done(&t, ok);
}

/* CreateCrazyGambler(): the gambler at a free spawn point, the target of the mission */
static byte Gambler_CreateCrazyGambler(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *h = TV(), *pd = TV(), *pilot = TV(), *name = TV();
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_SpawnCar("CrazyGambler", 0, h, NULL) &&
		LvN_StdProfile(CG, "GetCrazyGamblerProfile", h, pd) && LvN_SetI(h, "LaunchTaunts", 0) && Mode_G(CG, "pilotName", pilot) &&
		Mode_VSet(h, "MPil0", pilot) && LvN_SetS(h, "TgType", "red") && LvN_SetS(h, "EntityList", "Enemies") &&
		LvN_Life(h, CG, "CRAZY_GAMBLER_LIFE") && Mode_VGet(h, "Name", name) && Mode_Put(CG, "crazyGamblerName", NULL, name) &&
		Mode_Read(CG, "crazyGamblerName", name) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name);
	return Mode_Done(&t, ok);
}

/* OnCrazyGamblerDeath(LooserName, WinnerName): a life fewer; he comes back or the mission ends */
static byte Gambler_OnCrazyGamblerDeath(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lifes = TV(), *left = TV(), *rep = TV();
	byte ok = Mode_Import("Vehicles") && LV_CALL("SScorer", "CancelSpeech", NULL, "") && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) &&
		Gambler_Lifes(lifes) && Native_Arith('-', lifes, Mode_I(&t, 1), left);
	if (ok) {
		Mode_Repr(left, rep);
		ok = Mode_SetSaveVar("NumCrazyGamblerLifes", rep);
	}
	int done = ok ? Gambler_Last(left) : -1;
	if (done > 0) {
		ok = LvN_ScorerOff() && Mode_Later(7, CG, "PreSetNextMission", NULL, 0, NULL);
	} else if (done == 0) {
		ok = Mode_Later(3, CG, "CreateCrazyGambler", NULL, 0, NULL);
	}
	ok = ok && done >= 0 && LvN_Draw();
	return Mode_Done(&t, ok);
}

/* MissionScorer(id): the lives left */
static byte Gambler_MissionScorer(const nativeVal_t *id) {
	nativeVal_t y = { 0 }, n = { 0 };
	Native_SetNone(&y);
	Native_SetNone(&n);
	byte ok = Mode_Import("Scorer") && Mode_LvLabel(id, "RemainingLifesText", 0, "Mission_DefeatCrazyGambler1_Scorer1", 0) && LvN_TextY(20, &y);
	if (ok) {
		Mode_SaveVar("NumCrazyGamblerLifes", &n);
		ok = Mode_LvText(id, "RemainingLifes", &y, &n, NULL);
	}
	Native_Free(&y);
	Native_Free(&n);
	return ok;
}

LV_CORE0(Gambler_Init)
LV_CORE0(Gambler_LaunchTaunts)
LV_CORE0(Gambler_CreateCrazyGambler)
LV_COREA(Gambler_OnCrazyGamblerDeath)
LV_COREA(Gambler_MissionScorer)
LV_FN(Gambler, InitPy, 1, Gambler_Init_C)
LV_FN(Gambler, LaunchTauntsPy, 0, Gambler_LaunchTaunts_C)
LV_FN(Gambler, CreateCrazyGamblerPy, 0, Gambler_CreateCrazyGambler_C)
LV_FN(Gambler, OnCrazyGamblerDeathPy, 2, Gambler_OnCrazyGamblerDeath_C)
LV_FN(Gambler, MissionScorerPy, 1, Gambler_MissionScorer_C)

PyMethodDef g_DownTownDefeatCrazyGambler1Methods[] = {
	{ "GetCrazyGamblerProfile", Gambler_GetCrazyGamblerProfile, 1, NULL },
	{ "InitStatus", Gambler_InitStatus, 1, NULL },
	{ "Init", Gambler_InitPy, 1, NULL },
	{ "LaunchTaunts", Gambler_LaunchTauntsPy, 1, NULL },
	{ "CreateCrazyGambler", Gambler_CreateCrazyGamblerPy, 1, NULL },
	{ "OnCrazyGamblerDeath", Gambler_OnCrazyGamblerDeathPy, 1, NULL },
	{ "MissionScorer", Gambler_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FaceTheMayor: the crazy mayor in his ship, then DeepThroat at a phone cab ---- */

static const char *const mayor_profile[] = {
	"Ship=SMAYOR1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0", "GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=3",
	"UsePercSonic=70", "UsePercCloud=20", "UsePercElectro=10", "UsePercHook=30", "FactorHookMap=6", "CadenceShoot=3", "ControlAim=8",
	"AngAttack=2.5", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.30",
	"FactorDamageItemLife=0.25", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=60", "TimeMaxStrafe=6",
	"VelMaxStrafe=0.75", NULL,
};

static PyObject *Mayor_GetMayorProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, FM, mayor_profile);
}

static PyObject *Mayor_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, FM, "[\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Init(LevelPath): the mayor is not at the townhall; DeepThroat's warning and the mayor's taunt once the player is in
 * a ship, or the phone cab after the fight */
static byte Mayor_Init(void) {
	modeVals_t t = { .n = 0 };
	if (!Mode_Import("Speech") || !Mode_SaveS("MayorAtTownHall", "0")) {
		return 0;
	}
	byte ok;
	if (LvN_SaveIs("CrazyMayor", "Begin")) {
		nativeVal_t *args = TV();
		LvN_Str(args, "Mission_FaceTheMayor_RemoteMessage1");
		LvN_Str(TV(), "DeepThroat");
		ok = Mode_Import("Parking") && Mode_LaterComF(1.5, args, 2) &&
			LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", FM, "PreLaunchTaunt");
	} else {
		ok = LV_OWN(FM, "SetPhoneCab", NULL, "");
	}
	return Mode_Done(&t, ok);
}

static byte Mayor_PreLaunchTaunt(void) {
	return Mode_Later(3.0, FM, "LaunchMayorInitialTaunt", NULL, 0, NULL);
}

/* LaunchMayorInitialTaunt(): the taunt when the player is in a ship, else a second later */
static byte Mayor_LaunchMayorInitialTaunt(void) {
	nativeVal_t usr = { 0 };
	Native_SetNone(&usr);
	int car = Mode_UsrHandle(&usr) ? (Native_True(&usr) ? Mode_VIsType(&usr, "Car") : 0) : -1;
	Native_Free(&usr);
	if (car > 0) {
		return Mode_Import("Parking") && LvN_Com("Mission_FaceTheMayor_RemoteMessage2", "Mayor", FM, "CreateCrazyMayor", LvN_NoArgs()) &&
			LvN_ModNone("Parking", "SwitchVehicleMissionCallback");
	}
	return car == 0 && Mode_Later(1.0, FM, "LaunchMayorInitialTaunt", NULL, 0, NULL);
}

/* CreateCrazyMayor(): the mayor's ship instead of one of the traffic, an enemy and the target */
static byte Mayor_CreateCrazyMayor(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = TV(), *n1 = TV(), *h = TV(), *pd = TV(), *m = TV(), *name = TV();
	byte ok = Mode_Import("MissionsFuncs") && Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Native_GVarGet("iTrafficNumber", n) &&
		Native_Arith('-', n, Mode_I(&t, 1), n1) && Mode_GVarSet("iTrafficNumber", n1) && Mode_SpawnCar("NormalTraffic", 0, h, NULL) &&
		Mode_Put(FM, "SMayor1", NULL, h) && LV_OWN(FM, "GetMayorProfile", pd, "") && Mode_G(FM, "SMayor1", m) && LvN_StdAI(m, pd) &&
		LvN_SetS(m, "WeapList", "2,2,2,2,2,0,1") && LvN_SetS(m, "EntityList", "Enemies") &&
		LvN_SetS(m, "OnDeath", "Mission_FaceTheMayor.MayorOnDeath") && LvN_Life(m, FM, "MAYOR_LIFE") && Mode_VGet(m, "Name", name) &&
		LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name);
	return Mode_Done(&t, ok);
}

/* MayorOnDeath(Looser, Winner): the alarm, the mayor killed, the phone cab */
static byte Mayor_MayorOnDeath(const nativeVal_t *a) {
	nativeVal_t none = { 0 }, one = { 0 };
	Native_SetNone(&none);
	Native_SetInt(&one, 1);
	return Mode_Import("Vehicles") && Mode_Import("Init") && Mode_Put(FM, "SMayor1", NULL, &none) && Mode_SetAlarm(&one) &&
		LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && Mode_SaveS("CrazyMayor", "Killed") && LV_OWN(FM, "SetPhoneCab", NULL, "");
}

/* SetPhoneCab(): a free phone cab as the target; DeepThroat speaks once the alarm is over */
static byte Mayor_SetPhoneCab_C(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Vehicles", "Speech", "Alarm", "SaveGame", "PhoneCab", "MissionsFuncs", NULL };
	nativeVal_t fn = { 0 };
	byte ok = 1;
	(void)a;
	for (int i = 0; ok && mods[i]; i++) {
		ok = Mode_Import(mods[i]);
	}
	int alarm = ok && Mode_PhoneCab("Mission_FaceTheMayor.UsePhoneCab", 1, NULL, out) ? Mode_AlarmOn() : -1;
	if (alarm > 0) {
		Native_SetNone(&fn);
		ok = LvN_Com("Mission_FaceTheMayor_RemoteMessage3", "DeepThroat", FM, "FleeFromPolice", NULL) &&
			LvN_GObj(FM, "SetAlarmCallback", &fn) && LvN_ModAppend("Alarm", "SetAlarmCallback", &fn);
		Native_Free(&fn);
		return ok;
	}
	return alarm == 0 && LV_OWN(FM, "SetAlarmCallback", NULL, "i", 1L);
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): DeepThroat at the phone, the end of the mission after the talk */
static byte Mayor_UsePhoneCab(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pc = TV(), *params = TV();
	int alarm = Mode_SetModVar("Speech", "NPCName", &a[1]) && LV_CALL("PhoneCab", "UseAudioCom", NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]) &&
		Mode_EntityH(&a[1], pc) ? Mode_AlarmOn() : -1;
	byte ok = alarm >= 0;
	if (alarm > 0) {
		ok = LV_CALL("Speech", "StartObjSpch", NULL, "vss", pc, "Mission_TalkToDeepThroat1_RemoteMessage2", "DeepThroat");
	} else if (alarm == 0) {
		Native_SetEmpty(params, NV_TUPLE);
		Native_Copy(Native_Append(params), pc);
		ok = LV_CALL("Speech", "OnObjSpch", NULL, "vsss", pc, "DeepThroat", "Mission_TalkToDeepThroat1_ComDeepThroat_NI_1Msg",
			"Mission_TalkToDeepThroat1_ComDeepThroat_NI_1Ans") && LvN_OnEnd(FM, "EndOfMission", params);
	}
	return Mode_Done(&t, ok);
}

/* EndOfMission(pc): on to Mission_ApologizeMayor, the places of the characters */
static byte Mayor_EndOfMission(const nativeVal_t *a) {
	static const char *const places[] = { "MayorAtTownHall", "1", "MayorAtGambinDen", "0", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "TownHall", "Journalist.Berto_Defaut", "TownHall", NULL };
	nativeVal_t obj = { 0 };
	Native_SetNone(&obj);
	byte ok = LvN_SetS(&a[0], "UseFunc", "PhoneCab.DefUseAudioCom") && LvN_SetS(&a[0], "HeadModel", "") &&
		LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", "Mission_ApologizeMayor", "TownHall") && LvN_ModGet("Speech", "ObjEntity", &obj) &&
		LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &obj, 0L) && Mode_MMDesc("Mission_ApologizeMayor_MissionText1", 1);
	Native_Free(&obj);
	if (ok) {
		Mode_Saves(places);
	}
	return ok;
}

LV_CORE0(Mayor_Init)
LV_CORE0(Mayor_PreLaunchTaunt)
LV_CORE0(Mayor_LaunchMayorInitialTaunt)
LV_CORE0(Mayor_CreateCrazyMayor)
LV_COREA(Mayor_MayorOnDeath)
LV_COREA(Mayor_UsePhoneCab)
LV_COREA(Mayor_EndOfMission)
LV_FN(Mayor, InitPy, 1, Mayor_Init_C)
LV_FN(Mayor, PreLaunchTauntPy, 0, Mayor_PreLaunchTaunt_C)
LV_FN(Mayor, LaunchMayorInitialTauntPy, 0, Mayor_LaunchMayorInitialTaunt_C)
LV_FN(Mayor, CreateCrazyMayorPy, 0, Mayor_CreateCrazyMayor_C)
LV_FN(Mayor, MayorOnDeathPy, 2, Mayor_MayorOnDeath_C)
LV_FN(Mayor, SetPhoneCabPy, 0, Mayor_SetPhoneCab_C)
LV_FN(Mayor, UsePhoneCabPy, 4, Mayor_UsePhoneCab_C)
LV_FN(Mayor, EndOfMissionPy, 1, Mayor_EndOfMission_C)

PyMethodDef g_DownTownFaceTheMayorMethods[] = {
	{ "GetMayorProfile", Mayor_GetMayorProfile, 1, NULL },
	{ "InitStatus", Mayor_InitStatus, 1, NULL },
	{ "Init", Mayor_InitPy, 1, NULL },
	{ "PreLaunchTaunt", Mayor_PreLaunchTauntPy, 1, NULL },
	{ "LaunchMayorInitialTaunt", Mayor_LaunchMayorInitialTauntPy, 1, NULL },
	{ "CreateCrazyMayor", Mayor_CreateCrazyMayorPy, 1, NULL },
	{ "MayorOnDeath", Mayor_MayorOnDeathPy, 1, NULL },
	{ "SetPhoneCab", Mayor_SetPhoneCabPy, 1, NULL },
	{ "UsePhoneCab", Mayor_UsePhoneCabPy, 1, NULL },
	{ "EndOfMission", Mayor_EndOfMissionPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_Massacre: points for every ship the player destroys before the timer runs out ---- */

static PyObject *Massacre_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, MS, "[\"SMayor1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Scrap.Set("TimeSpecialUseTimer", betweenDeathsTime + add) (add 0: the value itself) */
static byte Ms_Timer(long add) {
	nativeVal_t v = { 0 }, r = { 0 }, n = { 0 };
	Native_SetNone(&r);
	Native_SetInt(&n, add);
	byte ok = Mode_G(MS, "betweenDeathsTime", &v) && (add ? Native_Arith('+', &v, &n, &r) : (Native_Copy(&r, &v), 1)) &&
		Mode_GVarSet("TimeSpecialUseTimer", &r);
	Native_Free(&v);
	Native_Free(&r);
	return ok;
}

/* Init(LevelPath): the callback of the deaths, the timer between two of them, the scorer, Igor and Rusty */
static byte Massacre_Init(void) {
	return LvN_ModSetGlobal("Vehicles", "OnDeathCallback", MS, "OnVehicleCallback") && Mode_GVarSetI("CounterType", 2) && Ms_Timer(0) &&
		LvN_ScorerCallG(MS, "MissionScorer") && LvN_Draw() && LvN_Com("Mission_Massacre_RemoteMessage4", "Igor", NULL, NULL, NULL) &&
		LV_CALL("SScorer", "AddModel", NULL, "isssii", 0L, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3L, 25L);
}

/* `prev <= lo and now > lo` of the percentages */
static byte Ms_Crossed(const nativeVal_t *prev, const nativeVal_t *now, double lo) {
	nativeVal_t l = { 0 };
	Native_SetFloat(&l, lo);
	return Native_Compare(prev, &l) <= 0 && Native_Compare(now, &l) > 0;
}

/* float(x) / float(MAX_POINTS) */
static byte Ms_Percent(modeVals_t *t, const nativeVal_t *x, nativeVal_t *out) {
	nativeVal_t *fx = Mode_V(t), *max = Mode_V(t), *fm = Mode_V(t);
	return Native_ToFloat(x, fx) && Mode_G(MS, "MAX_POINTS", max) && Native_ToFloat(max, fm) && Native_Arith('/', fx, fm, out);
}

/* OnVehicleCallback(LooserName, WinnerName): the points of an AI ship the player destroyed (its weapon slots and its
 * kind), Igor at each quarter, the end at 100 % */
static byte Massacre_OnVehicleCallback(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *name = TV(), *v = TV(), *x = TV(), *model = TV(), *slots = TV(), *typo = TV(), *found = TV();
	nativeVal_t *sv = TV(), *ac = TV(), *prev = TV(), *sum = TV(), *total = TV(), *rep = TV(), *now = TV();
	long points = 5;
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VGet(usr, "Name", name)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(name, &a[1])) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_EntityH(&a[0], v) || !Mode_VGet(v, "isAI", x)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(x)) {
		return Mode_Done(&t, 1);
	}
	byte ok = Mode_VGet(v, "Model", x) && Native_Upper(x, model) && Mode_VGet(v, "nSlot00", slots) &&
		Mode_VGet(v, "Traf_Active", x);
	if (ok && Native_True(x)) {
		ok = Mode_VGet(v, "Traf_Type", typo);
		points = LvN_EqS(typo, "Heavy") ? 3 : LvN_EqS(typo, "Medium") ? 2 : 1;
	} else if (ok) {
		/* `elif string.find(Model, "POLI")`: 5 either way */
		ok = LV_CALL("string", "find", found, "vs", model, "POLI");
	}
	if (ok) {
		Mode_SaveVar("Mission_Massacre.Points", sv);
	}
	ok = ok && Native_EvalText(MS, sv, ac) && Ms_Percent(&t, ac, prev) && Native_Arith('+', slots, Mode_I(&t, points), sum) &&
		Native_Arith('+', sum, ac, total);
	if (ok) {
		Mode_Repr(total, rep);
		ok = Mode_SetSaveVar("Mission_Massacre.Points", rep) && LvN_Set0("MassacreScorer", "Value", total) && Ms_Percent(&t, total, now);
	}
	if (ok) {
		const nativeVal_t *items[4] = { prev, Mode_S(&t, "<= 0.25 and"), now, Mode_S(&t, "> 0.25") };
		ok = Mode_Print(items, 4);
	}
	if (ok && (Ms_Crossed(prev, now, 0.25) || Ms_Crossed(prev, now, 0.5))) {
		ok = LvN_Com("Mission_Massacre_RemoteMessage1", "Igor", NULL, NULL, NULL);
	} else if (ok && Ms_Crossed(prev, now, 0.75)) {
		ok = LvN_Com("Mission_Massacre_RemoteMessage2", "Igor", NULL, NULL, NULL);
	} else if (ok) {
		Native_SetFloat(x, 1.0);
		ok = Native_Compare(now, x) < 0 || (LvN_ModNone("Vehicles", "OnDeathCallback") && LvN_ScorerOff() && LvN_Draw() &&
			LvN_Com("Mission_Massacre_RemoteMessage3", "Igor", MS, "SetNextMission", LvN_NoArgs()));
	}
	ok = ok && Ms_Timer(1) && Mode_GVarSetI("CounterType", 0);
	return Mode_Done(&t, ok);
}

/* MissionScorer(id): the bar of the points and the timer between two deaths */
static byte Massacre_MissionScorer(const nativeVal_t *id) {
	static const char bar[] = "MassacreScorer";
	modeVals_t t = { .n = 0 };
	nativeVal_t *max = TV(), *sv = TV(), *pts = TV(), *x = TV(), *y = TV();
	byte ok = Mode_Import("Scorer") && Mode_LvLabel(id, "MassacreText", 0, "Mission_Massacre_Scorer_1", 1) && Mode_ScorerAddS(id, bar, "Bar") &&
		Mode_BarDefaults(id, bar, NULL, NULL) && Mode_G(MS, "MAX_POINTS", max) && LvN_ScorerSet(id, bar, "MaxValue", max);
	if (ok) {
		Mode_SaveVar("Mission_Massacre.Points", sv);
	}
	ok = ok && Native_EvalText(MS, sv, pts) && LvN_ScorerSet(id, bar, "Value", pts) && Mode_ScorerSetI(id, bar, "Visible", 1) && LvN_TextX(40, x) &&
		LvN_TextY(60, y) && Mode_LvTimer(id, "BetweenDeathsTimer", x, y, "Mission_Massacre.TimeExpired", 1);
	return Mode_Done(&t, ok);
}

/* TimeExpired(): the points back to 0 */
static byte Massacre_TimeExpired(void) {
	return LvN_Com("Mission_Massacre_RemoteMessage5", "Igor", NULL, NULL, NULL) && Ms_Timer(0) && Mode_GVarSetI("CounterType", 2) &&
		Mode_SaveS("Mission_Massacre.Points", "0") && LvN_Set0I("MassacreScorer", "Value", 0);
}

/* SetNextMission(): the next mission of NextMissionData, the blueprints of Berto's and Betty's ships, Rusty later */
static byte Massacre_SetNextMission(void) {
	return Mode_DelSaveVars("Mission_Massacre") && Mode_ApplyNext(MS) && Mode_MMDesc("Mission_ArrangeIgorCombat_MissionText1", 1) &&
		Mode_SaveCat2(MS, "PressBlueprints.Ships", "SBerto1", "SBetty1") && Mode_SaveCat2(MS, "PressBlueprints.Engines", "MBerto1", "MBetty1") &&
		Dt_RustyLater("Mission_ArrangeIgorCombat_RemoteMessage1");
}

LV_CORE0(Massacre_Init)
LV_COREA(Massacre_OnVehicleCallback)
LV_COREA(Massacre_MissionScorer)
LV_CORE0(Massacre_TimeExpired)
LV_CORE0(Massacre_SetNextMission)
LV_FN(Massacre, InitPy, 1, Massacre_Init_C)
LV_FN(Massacre, OnVehicleCallbackPy, 2, Massacre_OnVehicleCallback_C)
LV_FN(Massacre, MissionScorerPy, 1, Massacre_MissionScorer_C)
LV_FN(Massacre, TimeExpiredPy, 0, Massacre_TimeExpired_C)
LV_FN(Massacre, SetNextMissionPy, 0, Massacre_SetNextMission_C)

PyMethodDef g_DownTownMassacreMethods[] = {
	{ "InitStatus", Massacre_InitStatus, 1, NULL },
	{ "Init", Massacre_InitPy, 1, NULL },
	{ "OnVehicleCallback", Massacre_OnVehicleCallbackPy, 1, NULL },
	{ "MissionScorer", Massacre_MissionScorerPy, 1, NULL },
	{ "TimeExpired", Massacre_TimeExpiredPy, 1, NULL },
	{ "SetNextMission", Massacre_SetNextMissionPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyCommSats3: levels/LevelCommon.c ---- */

/* ---- Mission_GetDataPart1: the transports drop the data cubes when the player hits them ---- */

static const char *const func_profile[] = {
	"Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=4", "UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10",
	"UsePercHook=0", "FactorHookMap=10", "CadenceShoot=1", "ControlAim=3", "AngAttack=2.5", "CounterAttack=0.8", "ControlBoost=1",
	"ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.60", "FactorDamageItemLife=0.15", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=1", "UsePercHookGetAway=5", "TimeMaxStrafe=8", "VelMaxStrafe=1.0", NULL,
};

static PyObject *Data_GetFuncEnemiesProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, GD, func_profile);
}

static PyObject *Data_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, GD, "[\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Init(LevelPath): the list of the transports and its callbacks, the first transport, the mission while the player is
 * in a ship, the boss */
static byte Data_Init(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *model = TV(), *nd = TV(), *max = TV(), *m1 = TV(), *rep = TV();
	byte ok = Mode_Import("Parking") && Mode_G(GD, "DataCubeModel", model) && Mode_Preload(model) &&
		LV_CALL("Scrap", "CreateEntityList", NULL, "s", "Transports") &&
		Mode_ListSet("Transports", "OnDeath", "Mission_GetDataPart1.OnTransportsDeath") &&
		Mode_ListSet("Transports", "OnDamage", "Mission_GetDataPart1.OnTransportsDamage") && Mode_SaveInt("NumData", nd);
	int c = ok ? Mode_CmpG(nd, ">=", GD, "MAX_DATA_UNITS") : -1;
	if (c > 0) {
		ok = Mode_G(GD, "MAX_DATA_UNITS", max) && Native_Arith('-', max, Mode_I(&t, 1), m1);
		if (ok) {
			Mode_Repr(m1, rep);
			ok = Mode_SetSaveVar("NumData", rep);
		}
	}
	ok = ok && c >= 0 && LV_OWN(GD, "AddTransportToList", NULL, "") && LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", GD, "InitMission") &&
		LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", GD, "StopMission");
	if (ok && LvN_SaveIs("MissionState", "FirstTime")) {
		ok = LvN_Com("MissionGetDataPart1_RemoteMessage1", "Boss", NULL, NULL, NULL) && Mode_SaveS("MissionState", "GetData");
	} else if (ok) {
		ok = LvN_Com("MissionGetDataPart1_RemoteMessage5", "Boss", NULL, NULL, NULL);
	}
	return Mode_Done(&t, ok);
}

/* `nd and int(nd) op global` of the data units taken */
static int Gd_DataCmp(const nativeVal_t *nd, const char *op, const char *name) {
	nativeVal_t n = { 0 };
	if (!Native_True(nd)) {
		return 0;
	}
	Native_SetNone(&n);
	int r = Native_ToInt(nd, &n) ? Mode_CmpG(&n, op, GD, name) : -1;
	Native_Free(&n);
	return r;
}

/* InitMission(): the scorer; past half of the data the functionaries come for the transport */
static byte Data_InitMission(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *nd = TV(), *cur = TV(), *e = TV(), *usr = TV();
	byte ok = LvN_ScorerCallG(GD, "MissionScorer") && LvN_Draw();
	if (ok) {
		Mode_SaveVar("NumData", nd);
	}
	int go = ok ? LvN_GTrue(GD, "funcsSpawned") : -1;
	go = go == 0 ? Gd_DataCmp(nd, ">", "NumDataFuncsAttackTransport") : go > 0 ? 0 : -1;
	if (go > 0) {
		ok = LvN_PutI(GD, "funcsSpawned", 1) && LvN_Com("MissionGetDataPart1_RemoteMessage8", "Functionary", NULL, NULL, NULL) &&
			LV_OWN(GD, "SpawnFunctionaries", NULL, "i", 2L) && Mode_G(GD, "currentTransport", cur) && Mode_EntityH(cur, e) && Mode_UsrHandle(usr);
		int c = ok ? (Native_True(e) ? Mode_UsrInCar(usr) : 0) : -1;
		ok = c >= 0 && (!c || (Mode_Read(GD, "currentTransport", cur) && Mode_VSet(usr, "Target", cur)));
	}
	return Mode_Done(&t, ok && go >= 0);
}

/* o1.Ang = (0, spw[1], 0) */
static byte Dt_SpawnAng(const nativeVal_t *o1, const nativeVal_t *spw) {
	nativeVal_t ang = { 0 };
	Native_SetEmpty(&ang, NV_TUPLE);
	Native_SetInt(Native_Append(&ang), 0);
	byte ok = LvN_Item(spw, 1, Native_Append(&ang));
	Native_SetInt(Native_Append(&ang), 0);
	ok = ok && Mode_VSet(o1, "Ang", &ang);
	Native_Free(&ang);
	return ok;
}

/* usr = Scrap.UsrEntity(0); if usr and usr.IsType("Car"): usr.Target = target */
static byte Dt_UsrTarget(const nativeVal_t *target) {
	nativeVal_t usr = { 0 };
	Native_SetNone(&usr);
	int c = Mode_UsrHandle(&usr) ? Mode_UsrInCar(&usr) : -1;
	byte ok = c >= 0 && (!c || Mode_VSet(&usr, "Target", target));
	Native_Free(&usr);
	return ok;
}

/* CreateTransport(): a transport of the light traffic at a free spawn point, the player's target */
static byte Data_CreateTransport_C(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *spw = TV(), *pd = TV(), *name = TV(), *ship = TV();
	(void)a;
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_Import("Traffic") && LV_CALL("Traffic", "SetPath", NULL, "") &&
		Mode_SpawnCar("Transport", 0, out, spw) && LvN_SetS(out, "TgType", "red") && Dt_SpawnAng(out, spw) &&
		LV_CALL("VehiclesNPC", "GetIAProf", pd, "s", "TLight2") && Mode_VGet(out, "Name", name) && Mode_VGet(pd, "Ship", ship) &&
		LV_CALL("VehiclesNPC", "SetStdProperties", NULL, "vv", name, ship) && LV_CALL("VehiclesNPC", "SetAIProfile", NULL, "vs", pd, "Pilot") &&
		LV_CALL("VehiclesNPC", "ModAIProff", NULL, "vv", out, pd) && Mode_VGet(out, "Name", name) &&
		LV_CALL("SAI", "SetInertia", NULL, "vi", name, 0L) && LvN_Life(out, GD, "TRANSPORT_LIFE") && LV_CALL("Traffic", "ResetPath", NULL, "") &&
		Mode_VGet(out, "Name", name) && Dt_UsrTarget(name);
	return Mode_Done(&t, ok);
}

/* CreateFunctionary(): a functionary fighter at a free spawn point */
static byte Data_CreateFunctionary_C(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *spw = TV(), *pd = TV();
	(void)a;
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_Import("Traffic") && Mode_SpawnCar("FuncFighter", 0, out, spw) &&
		LvN_SetS(out, "TgType", "red") && Dt_SpawnAng(out, spw) && LvN_StdProfile(GD, "GetFuncEnemiesProfile", out, pd) &&
		LvN_Life(out, GD, "FUNCS_LIFE");
	return Mode_Done(&t, ok);
}

/* AddTransportToList(): a new transport (the farthest one leaves a full list), the target of the mission */
static byte Data_AddTransportToList(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *upos = TV(), *lst = TV(), *max = TV(), *dist = TV(), *remove = TV(), *name = TV(), *e = TV();
	nativeVal_t *pos = TV(), *sub = TV(), *d = TV(), *trans = TV(), *cur = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	int n = Mode_VGet(usr, "Pos", upos) && Mode_G(GD, "transports", lst) ? Native_Size(lst) : -1;
	byte ok = n >= 0 && Mode_G(GD, "MAX_TRANSPORTS", max);
	if (ok && Native_Compare(Mode_I(&t, n), max) == 0) {
		Native_SetInt(dist, 0);
		LvN_Str(remove, "");
		Native_Free(lst);
		int cnt = LvN_GElems(GD, "transports", lst) ? Native_ForCount(lst) : -1;
		ok = cnt >= 0;
		for (int k = 0; ok && k < cnt; k++) {
			int mark = t.n;
			nativeVal_t *it = TV();
			ok = Native_SeqItem(lst, k, it) && Mode_EntityH(it, e);
			if (ok && Native_True(e)) {
				ok = Mode_VGet(e, "Pos", pos) && Mode_VecSub(upos, pos, sub) && Mode_VecModSqr(sub, d);
				if (ok && Native_Compare(d, dist) > 0) {
					Native_Free(dist);
					Native_Copy(dist, d);
					Native_Free(remove);
					Native_Copy(remove, it);
				}
			}
			Native_Free(e);
			Native_Free(pos);
			Native_Free(sub);
			Native_Free(d);
			Mode_Release(&t, mark);
		}
		int in = ok && Mode_Read(GD, "transports", lst) ? Native_Contains(lst, remove) : -1;
		ok = in >= 0 && (!in || Mode_Remove(GD, "transports", remove));
	}
	ok = ok && LV_OWN(GD, "CreateTransport", trans, "") && Mode_VGet(trans, "Name", name) && Mode_Put(GD, "currentTransport", NULL, name) &&
		LvN_PutI(GD, "currentTransportSpawnedData", 0) && Mode_G(GD, "currentTransport", cur) &&
		LV_CALL("SAI", "SetStateVehicle", NULL, "ividd", 3L, cur, 0L, 3.0, 2.0) && LvN_SetS(trans, "EntityList", "Transports") &&
		Mode_Read(GD, "currentTransport", cur) && Mode_Append(GD, "transports", cur) && Mode_Read(GD, "currentTransport", cur) &&
		LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", cur);
	return Mode_Done(&t, ok);
}

/* SpawnFunctionaries(numFuncs): functionaries as enemies, or with only the rail after the transport once the player
 * has more than half of the data (later when the transport is not there) */
static byte Data_SpawnFunctionaries(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *nd = TV(), *n = TV(), *cur = TV(), *e = TV(), *fn = TV(), *name = TV(), *one = TV();
	if (!Mode_Import("VehiclesNPC")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("NumData", nd);
	if (!Native_True(nd)) {
		return Mode_Done(&t, 1);
	}
	int attack = Native_ToInt(nd, n) ? Mode_CmpG(n, ">", GD, "NumDataFuncsAttackTransport") : -1;
	byte ok = attack >= 0 && Mode_G(GD, "currentTransport", cur) && Mode_EntityH(cur, e);
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	byte noE = !Native_True(e);
	if (attack && noE) {
		Native_SetInt(one, 1);
		return Mode_Done(&t, Mode_Later(3, GD, "SpawnFunctionaries", one, 1, NULL));
	}
	long count;
	ok = Mode_Range(&a[0], &count);
	for (long i = 0; ok && i < count; i++) {
		ok = LV_OWN(GD, "CreateFunctionary", fn, "") && LvN_SetS(fn, "OnDeath", "Mission_GetDataPart1.OnFunctionaryDeath");
		if (ok && (!attack || noE)) {
			ok = LvN_SetS(fn, "EntityList", "Enemies");
		} else if (ok) {
			ok = LvN_SetS(fn, "WeapList", "0,0,0,0,1,0,1") && Mode_VGet(fn, "Name", name) && Mode_Append(GD, "funcsAttackingTransport", name) &&
				Mode_VGet(fn, "Name", name) && Mode_Read(GD, "currentTransport", cur) &&
				LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, name, cur);
		}
		Native_Free(fn);
		Native_Free(name);
	}
	return Mode_Done(&t, ok);
}

/* OnFunctionaryDeath(LooserName, WinnerName): another one 10 s later */
static byte Data_OnFunctionaryDeath(const nativeVal_t *a) {
	nativeVal_t one = { 0 };
	Native_SetInt(&one, 1);
	return Mode_Import("Vehicles") && Mode_RemoveIn(&a[0], GD, "funcsAttackingTransport") &&
		LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && Mode_Later(10, GD, "SpawnFunctionaries", &one, 1, NULL);
}

/* OnTransportsDeath(LooserName, WinnerName): the transport explodes, its functionaries become enemies; a transport
 * that still had data gives a new one */
static byte Data_OnTransportsDeath(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = TV(), *lst = TV(), *rs = TV(), *empty = TV();
	/* Scrap.SetCallFunc, AddParams, Execute by name (OBJLIB 7: the call function is static in ScrapPy.c) */
	byte ok = Mode_EntityH(&a[0], e) && LvN_SetI(e, "Ammo01", 500) && LV_CALL("Scrap", "SetCallFunc", NULL, "s", "!InfernoFire") &&
		LV_CALL("Scrap", "AddParams", NULL, "v", &a[0]) && LV_CALL("Scrap", "Execute", NULL, "");
	int cnt = ok && LvN_GElems(GD, "funcsAttackingTransport", lst) ? Native_ForCount(lst) : -1;
	ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t it = { 0 };
		Native_Free(e);
		ok = Native_SeqItem(lst, k, &it) && Mode_EntityH(&it, e) && (!Native_True(e) || LvN_SetS(e, "EntityList", "Enemies"));
		Native_Free(&it);
	}
	Native_SetEmpty(empty, NV_LIST);
	ok = ok && Mode_Put(GD, "funcsAttackingTransport", NULL, empty) && Mode_RemoveIn(&a[0], GD, "transportsTaunts");
	int in = ok ? Mode_InG(&a[0], GD, "transports") : -1;
	if (in > 0) {
		Native_Free(e);
		int data = Mode_EntityH(&a[0], e) ? (Native_True(e) ? (Mode_VGet(e, "RadarShow", rs) ? LvN_EqI(rs, 1) : -1) : 0) : -1;
		ok = data >= 0 && (!data || (LvN_PutI(GD, "droppedData", 0) && LvN_Com("MissionGetDataPart1_RemoteMessage3", "Boss", NULL, NULL, NULL) &&
			Mode_Later(2, GD, "AddTransportToList", NULL, 0, "AddTransport")));
	}
	return Mode_Done(&t, ok && in >= 0);
}

/* OnTransportsDamage(LooserName, WinnerName, Damage): the player hits the transport with data: the functionaries, a
 * taunt, the data cubes start to fall */
static byte Data_OnTransportsDamage(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *e = TV(), *rs = TV(), *name = TV(), *nd = TV(), *dd = TV(), *delay = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	int off = Mode_EntityH(&a[0], e) ? (Native_True(e) ? (Mode_VGet(e, "RadarShow", rs) ? !LvN_EqI(rs, 1) : -1) : 0) : -1;
	if (off == 0) {
		off = Mode_InG(&a[1], GD, "funcsAttackingTransport");
	}
	if (off != 0) {
		return Mode_Done(&t, off > 0);
	}
	if (!Mode_VGet(usr, "Name", name)) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	if (!Native_Compare(name, &a[1])) {
		int c = LvN_GTrue(GD, "funcsSpawned");
		ok = c >= 0 && (c || (LvN_PutI(GD, "funcsSpawned", 1) && LV_OWN(GD, "SpawnFunctionaries", NULL, "i", 2L)));
		if (ok) {
			Mode_SaveVar("NumData", nd);
		}
		c = ok ? Gd_DataCmp(nd, "<=", "NumDataFuncsAttackTransport") : -1;
		if (c > 0) {
			c = Mode_InG(&a[0], GD, "transportsTaunts");
			ok = c >= 0 && (c || (LvN_Com("MissionGetDataPart1_RemoteMessage7", "Functionary", NULL, NULL, NULL) &&
				Mode_Append(GD, "transportsTaunts", &a[0])));
		}
		ok = ok && c >= 0 && Mode_G(GD, "droppedData", dd);
		if (ok && LvN_EqI(dd, 0)) {
			ok = Mode_G(GD, "TimeBetweenData", delay) && Mode_LaterV(delay, GD, "SpawnData", NULL, 0, NULL) && LvN_PutI(GD, "droppedData", 1);
		}
	}
	return Mode_Done(&t, ok);
}

/* SpawnData(): the current transport drops a data cube, four in a row */
static byte Data_SpawnData(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cur = TV(), *e = TV(), *n = TV(), *pos = TV(), *dd = TV(), *delay = TV();
	byte ok = Mode_G(GD, "currentTransport", cur) && Mode_EntityH(cur, e);
	int c = ok ? (Native_True(e) ? (Mode_SaveInt("NumData", n) ? Mode_CmpG(n, "<", GD, "MAX_DATA_UNITS") : -1) : 0) : -1;
	if (c > 0) {
		ok = Mode_VGet(e, "Pos", pos) && LV_OWN(GD, "DropData", NULL, "v", pos) && Mode_G(GD, "droppedData", dd);
		if (ok && !LvN_EqI(dd, 3)) {
			ok = Mode_G(GD, "TimeBetweenData", delay) && Mode_LaterV(delay, GD, "SpawnData", NULL, 0, "SpawnData") && LvN_GAdd(GD, "droppedData", 1, NULL);
		} else if (ok) {
			ok = LvN_PutI(GD, "droppedData", 0);
		}
	}
	return Mode_Done(&t, ok && c >= 0);
}

/* DropData(pos): a data cube that goes away after TimeDataDisappears */
static byte Data_DropData(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = TV(), *model = TV(), *delay = TV(), *name = TV();
	byte ok = LvN_CreateAt(Mode_S(&t, "DroppedData"), &a[0], "Item", l) && Mode_G(GD, "DataCubeModel", model) && Mode_VModel(l, model) &&
		LvN_SetI(l, "Radius", 3500) && LvN_SetI(l, "AppearsIfFree", 0) && LvN_SetI(l, "AutoFace", 0) && LvN_SetI(l, "OnlyUser", 1) &&
		LvN_SetF(l, "rTime", 0.25) && LvN_SetS(l, "OnTake", "Mission_GetDataPart1.TakeData") && Mode_Import("Items") &&
		LV_CALL("Items", "SetSound", NULL, "vs", l, "TakeDataCube") && Mode_G(GD, "TimeDataDisappears", delay) && Mode_VGet(l, "Name", name) &&
		Mode_LaterV(delay, GD, "DelData", name, 1, NULL);
	return Mode_Done(&t, ok);
}

static byte Data_DelData(const nativeVal_t *a) {
	nativeVal_t e = { 0 };
	Native_SetNone(&e);
	byte ok = Mode_EntityH(&a[0], &e) && (!Native_True(&e) || Mode_VDel(&e));
	Native_Free(&e);
	return ok;
}

/* the functionaries after the transport with only the rail */
static byte Gd_FuncsAttack(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *fname = TV(), *fn = TV(), *lst = TV(), *cur = TV();
	byte ok = Mode_ListGet("Enemies", "First", fname) && Mode_EntityH(fname, fn);
	while (ok && Native_True(fn)) {
		ok = Mode_Append(GD, "funcsAttackingTransport", fname);
		Native_Free(fname);
		ok = ok && Mode_VGet(fn, "NextInList", fname);
		Native_Free(fn);
		ok = ok && Mode_EntityH(fname, fn);
	}
	int cnt = ok && LvN_GElems(GD, "funcsAttackingTransport", lst) ? Native_ForCount(lst) : -1;
	ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t it = { 0 };
		Native_Free(fn);
		ok = Native_SeqItem(lst, k, &it) && Mode_EntityH(&it, fn) && LvN_SetS(fn, "WeapList", "0,0,0,0,1,0,1") && LvN_SetS(fn, "EntityList", "") &&
			Mode_Read(GD, "currentTransport", cur) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, &it, cur);
		Native_Free(&it);
	}
	return Mode_Done(&t, ok);
}

/* the transport gives no more data: not a target any more */
static byte Gd_TransportDone(const nativeVal_t *e) {
	return !Native_True(e) || (LvN_SetS(e, "TgType", "none") && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", e, 0L));
}

/* TakeData(ItemName, CharName): a data unit more; all of them end the mission, the transport's share a new transport */
static byte Data_TakeData(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *itm = TV(), *at = TV(), *sv = TV(), *ni = TV(), *num = TV(), *max = TV(), *rep = TV(), *cur = TV(), *e = TV();
	nativeVal_t *lst = TV(), *fn = TV(), *sd = TV();
	int c = Mode_UsrHandle(usr) ? LvN_UsrNameIs(usr, &a[1]) : -1;
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	byte ok = Mode_EntityH(&a[0], itm) && LvN_SetI(itm, "rTime", 20);
	if (ok) {
		PyObject *del = Mode_VBound(itm, "Del");
		Native_SetFloat(at, Mode_Time() + 5);
		ok = del && Mode_Schedule(at, del, NULL, 0, NULL);
		Py_XDECREF(del);
	}
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("NumData", sv);
	if (!Native_ToInt(sv, ni) || !Native_Arith('+', ni, Mode_I(&t, 1), num) || !Mode_G(GD, "MAX_DATA_UNITS", max)) {
		/* except: return (OBJLIB 7: the error is the Python exception until errors are a C state) */
		PyErr_Clear();
		return Mode_Done(&t, 1);
	}
	if (Native_Compare(num, max) > 0) {
		return Mode_Done(&t, 1);
	}
	Mode_Repr(num, rep);
	ok = Mode_SetSaveVar("NumData", rep) && Mode_G(GD, "currentTransport", cur) && Mode_EntityH(cur, e);
	int all = ok ? Mode_CmpG(num, "==", GD, "MAX_DATA_UNITS") : -1;
	if (all > 0) {
		ok = Gd_TransportDone(e) && Mode_DelSchedS("SpawnData") && Mode_DelSchedS("AddTransport") && Mode_Import("Parking") && LvN_ScorerOff() &&
			LvN_ModNone("Parking", "SwitchVehicleMissionCallback");
		int cnt = ok && LvN_GElems(GD, "funcsAttackingTransport", lst) ? Native_ForCount(lst) : -1;
		ok = cnt >= 0;
		for (int k = 0; ok && k < cnt; k++) {
			nativeVal_t it = { 0 };
			Native_Free(fn);
			ok = Native_SeqItem(lst, k, &it) && Mode_EntityH(&it, fn) && LvN_SetS(fn, "EntityList", "Traffic") && LvN_SetS(fn, "TgType", "none");
			Native_Free(&it);
		}
		ok = ok && Mode_ListSet("Transports", "OnDeath", "") && Mode_ListSet("Transports", "OnDamage", "") &&
			LvN_Com("MissionGetDataPart1_RemoteMessage6", "Boss", GD, "SetNextMission", LvN_NoArgs());
	} else if (all == 0) {
		int attack = Mode_CmpG(num, ">", GD, "NumDataFuncsAttackTransport");
		if (attack > 0) {
			attack = Native_True(e);
		}
		if (attack > 0) {
			c = LvN_GTrue(GD, "funcsAttackTransportMessage");
			ok = c >= 0 && (c || (LvN_PutI(GD, "funcsAttackTransportMessage", 1) &&
				LvN_Com("MissionGetDataPart1_RemoteMessage8", "Functionary", NULL, NULL, NULL))) && Gd_FuncsAttack();
		}
		ok = ok && attack >= 0 && LvN_GAdd(GD, "currentTransportSpawnedData", 1, NULL) && Mode_G(GD, "currentTransportSpawnedData", sd);
		c = ok ? Mode_CmpG(sd, ">=", GD, "MAX_TRANSPORTS_DATA_UNITS") : -1;
		ok = c >= 0 && (!c || (Gd_TransportDone(e) && LvN_PutI(GD, "droppedData", 0) && Mode_DelSchedS("SpawnData") &&
			LvN_Com("MissionGetDataPart1_RemoteMessage2", "Boss", GD, "AddTransportToList", LvN_NoArgs())));
	}
	ok = ok && all >= 0 && LvN_Draw();
	return Mode_Done(&t, ok);
}

/* SetNextMission(): on to Mission_GetDataPart2 in the bank */
static byte Data_SetNextMission(void) {
	nativeVal_t cur = { 0 }, e = { 0 };
	Native_SetNone(&cur);
	Native_SetNone(&e);
	byte ok = Mode_Import("MissionsFuncs") && Mode_G(GD, "currentTransport", &cur) && Mode_EntityH(&cur, &e) && Gd_TransportDone(&e) &&
		Mode_DelSaveVars("MissionState") && Mode_DelSaveVars("NumData") &&
		LvN_Next("Mission_GetDataPart2", "Bank", "Mission_TalkToBoss2_MissionText1") && Mode_WayPoint();
	Native_Free(&cur);
	Native_Free(&e);
	return ok;
}

/* MissionScorer(id): the bar of the data units */
static byte Data_MissionScorer(const nativeVal_t *id) {
	static const char bar[] = "DataStatus";
	nativeVal_t max = { 0 }, n = { 0 };
	Native_SetNone(&max);
	Native_SetNone(&n);
	byte ok = Mode_LvLabel(id, "DataStatusText", 0, "Mission_GetDataPart1_Scorer_1", 0) && Mode_ScorerAddS(id, bar, "Bar") &&
		Mode_BarDefaults(id, bar, NULL, NULL) && Mode_ScorerSetI(id, bar, "IsAutoUpdatable", 0) && Mode_G(GD, "MAX_DATA_UNITS", &max) &&
		LvN_ScorerSet(id, bar, "MaxValue", &max) && Mode_SaveInt("NumData", &n) && LvN_ScorerSet(id, bar, "Value", &n);
	Native_Free(&max);
	Native_Free(&n);
	return ok;
}

LV_CORE0(Data_Init)
LV_CORE0(Data_InitMission)
LV_CORE0(Data_AddTransportToList)
LV_COREA(Data_SpawnFunctionaries)
LV_COREA(Data_OnFunctionaryDeath)
LV_COREA(Data_OnTransportsDeath)
LV_COREA(Data_OnTransportsDamage)
LV_CORE0(Data_SpawnData)
LV_COREA(Data_DropData)
LV_COREA(Data_DelData)
LV_COREA(Data_TakeData)
LV_CORE0(Data_SetNextMission)
LV_COREA(Data_MissionScorer)
LV_FN(Data, InitPy, 1, Data_Init_C)
LV_FN(Data, InitMissionPy, 0, Data_InitMission_C)
LV_FN(Data, CreateTransportPy, 0, Data_CreateTransport_C)
LV_FN(Data, CreateFunctionaryPy, 0, Data_CreateFunctionary_C)
LV_FN(Data, AddTransportToListPy, 0, Data_AddTransportToList_C)
LV_FN(Data, SpawnFunctionariesPy, 1, Data_SpawnFunctionaries_C)
LV_FN(Data, OnFunctionaryDeathPy, 2, Data_OnFunctionaryDeath_C)
LV_FN(Data, OnTransportsDeathPy, 2, Data_OnTransportsDeath_C)
LV_FN(Data, OnTransportsDamagePy, 3, Data_OnTransportsDamage_C)
LV_FN(Data, SpawnDataPy, 0, Data_SpawnData_C)
LV_FN(Data, DropDataPy, 1, Data_DropData_C)
LV_FN(Data, DelDataPy, 1, Data_DelData_C)
LV_FN(Data, TakeDataPy, 2, Data_TakeData_C)
LV_FN(Data, SetNextMissionPy, 0, Data_SetNextMission_C)
LV_FN(Data, MissionScorerPy, 1, Data_MissionScorer_C)

PyMethodDef g_DownTownGetDataPart1Methods[] = {
	{ "GetFuncEnemiesProfile", Data_GetFuncEnemiesProfile, 1, NULL },
	{ "InitStatus", Data_InitStatus, 1, NULL },
	{ "Init", Data_InitPy, 1, NULL },
	{ "InitMission", Data_InitMissionPy, 1, NULL },
	{ "CreateTransport", Data_CreateTransportPy, 1, NULL },
	{ "CreateFunctionary", Data_CreateFunctionaryPy, 1, NULL },
	{ "AddTransportToList", Data_AddTransportToListPy, 1, NULL },
	{ "SpawnFunctionaries", Data_SpawnFunctionariesPy, 1, NULL },
	{ "OnFunctionaryDeath", Data_OnFunctionaryDeathPy, 1, NULL },
	{ "OnTransportsDeath", Data_OnTransportsDeathPy, 1, NULL },
	{ "OnTransportsDamage", Data_OnTransportsDamagePy, 1, NULL },
	{ "SpawnData", Data_SpawnDataPy, 1, NULL },
	{ "DropData", Data_DropDataPy, 1, NULL },
	{ "DelData", Data_DelDataPy, 1, NULL },
	{ "TakeData", Data_TakeDataPy, 1, NULL },
	{ "SetNextMission", Data_SetNextMissionPy, 1, NULL },
	{ "MissionScorer", Data_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_HumphreyRace: the first crazy bet, a race against Humphrey with the doors of the level closed ---- */

/* InitStatus(LevelPath): the challenge of the race (only the first race of the level) or none */
static byte Bet_InitStatus(void) {
	static const char *const race[] = { "Revenge.NumEnemies", "3", "Challenge.NumEnemies", "3", "Challenge.Type", "Race", "Challenge.Foe",
		"Humphrey", "Challenge.Money", "0", NULL };
	static const char *const none[] = { "Revenge.Type", "", "Revenge.Foe", "", "Revenge.NumEnemies", "", "Challenge.NumEnemies", "",
		"Challenge.Type", "", "Challenge.Foe", "", "Challenge.Money", "", "Race.Profile", "", NULL };
	if (!Mode_AcShips(HR, "[\"SMayor1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]") || !Mode_Import("MapRace")) {
		return 0;
	}
	if (LvN_SaveIs("MissionState", "HumphreyRace")) {
		Mode_Saves(race);
		return Mode_SaveG("Race.Profile", HR, "RACE_PROFILE") && Mode_ModList1("MapRace", "Races", "MapRace", "EasyRaceDownTown1") &&
			LvN_Com("Mission_FirstCrazyBet_RemoteMessage5", "CrazyGambler", HR, "ShowSystemMessage", LvN_NoArgs());
	}
	Mode_Saves(none);
	return Mode_ModEmpty("MapRace", "Races");
}

static byte Bet_ShowSystemMessage(void) {
	nativeVal_t text = { 0 };
	if (!Mode_Import("RaceMaker")) {
		return 0;
	}
	Mode_Lang("RaceSystemGetShip", &text);
	byte ok = LV_CALL("RaceMaker", "ShowSystemMessage", NULL, "v", &text);
	Native_Free(&text);
	return ok;
}

/* Init(LevelPath): the doors of the level closed; the race with its callbacks and the boost message, or the call of
 * the crazy gambler after it */
static byte Bet_Init(void) {
	nativeVal_t prev = { 0 };
	byte ok = Mode_Import("RaceMaker") && Mode_Import("Parking") &&
		LV_CALL("SScorer", "AddModel", NULL, "isssii", 0L, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3L, 25L) &&
		Mode_EntSetS("DM_Door_CityDoorA_00", "OnObjSlot", "!CloseDoorSlot") && Mode_EntSetI("DM_Door_MetroExt_Downtown", "ArrowCanUse", 0) &&
		Mode_EntSetS("DM_Door_Outmap_Commercial", "OnObjSlot", "!CloseDoorSlot") &&
		Mode_EntSetS("DM_Door_Outmap_Scrapyard", "OnObjSlot", "!CloseDoorSlot") &&
		Mode_EntSetS("DM_Door_Outmap_Outskirts", "OnObjSlot", "!CloseDoorSlot") &&
		Mode_EntSetS("DM_Door_Elevator_Press", "UseFunc", "Mission_HumphreyRace.OnTryingToExit");
	if (!ok) {
		return 0;
	}
	if (!LvN_SaveIs("MissionState", "HumphreyRace")) {
		return LV_OWN(HR, "CallCrazyGambler", NULL, "");
	}
	Native_SetNone(&prev);
	ok = Mode_GVarSetI("TimeToCancelCurrentMission", 0) && LvN_ModSetI("RaceMaker", "SkipTimer", 1) &&
		LvN_ModSetGlobal("RaceMaker", "OnPlayerWinsRace", HR, "OnPlayerWinsRace") &&
		LvN_ModSetGlobal("RaceMaker", "OnPlayerLoosesRace", HR, "OnPlayerLoosesRace") && LvN_ModSetGlobal("RaceMaker", "OnPreInitRace", HR, "DelRacers") &&
		LvN_ModSetGlobal("RaceMaker", "OnInitRace", HR, "SetMissionTarget") &&
		LvN_ModSetGlobal("RaceMaker", "OnCheckPointCallback", HR, "ClosePressHangar") && Mode_Target("BadBoy0") &&
		LvN_ModGet("Parking", "SwitchVehicleMissionCallback", &prev) && Mode_Put(HR, "prevSwitchVehicleMissionCallback", NULL, &prev) &&
		LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", HR, "ShowBoostMessage");
	Native_Free(&prev);
	return ok;
}

/* ShowBoostMessage(): the callback it replaced, then the message of the boost key */
static byte Bet_ShowBoostMessage(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = TV(), *text = TV(), *key = TV(), *msg = TV();
	byte ok = Mode_Import("Parking") && Mode_Import("RaceMaker") && Mode_G(HR, "prevSwitchVehicleMissionCallback", prev);
	if (ok && Native_True(prev)) {
		PyObject *fn = Native_ToPy(prev);
		ok = fn && Native_Call(fn, NULL, "");
		Py_XDECREF(fn);
	}
	ok = ok && Mode_Read(HR, "prevSwitchVehicleMissionCallback", prev) && Mode_SetModVar("Parking", "SwitchVehicleMissionCallback", prev);
	if (ok) {
		Mode_Lang("Mission_FirstCrazyBet_SystemMessage4", text);
		ok = LV_CALL("SInput", "GetDefinedEntry", key, "isss", 0L, "Racer", "Boost", "Mouse") && Native_Arith('%', text, key, msg) &&
			Mode_MsgText(msg, Mode_Time() + 10);
	}
	return Mode_Done(&t, ok);
}

/* ClosePressHangar(): at the first checkpoint the door of the press hangar closes */
static byte Bet_ClosePressHangar(void) {
	return Mode_Import("RaceMaker") && Mode_EntSetS("DM_Door_CityDoorA_01", "OnObjSlot", "!CloseDoorSlot") &&
		LvN_ModNone("RaceMaker", "OnCheckPointCallback");
}

/* DelRacers(names): the other racers go, only Humphrey races */
static byte Bet_DelRacers(const nativeVal_t *a) {
	return Mode_DelNames(&a[0]);
}

static byte Bet_CallCrazyGambler(void) {
	return Mode_PhoneCab("Mission_HumphreyRace.UsePhoneCab", 1, NULL, NULL);
}

static byte Bet_UsePhoneCab(const nativeVal_t *a) {
	return Dt_UsePhone(a, HR, "Generics_StillPolice1_CrazyGambler", "CrazyGambler", "Mission_FirstCrazyBet_ComCrazyGambler_NI_1Msg",
		"PreGivePlans", 1);
}

/* PreGivePlans2(): the blueprints of the mercenary ship and engine, the next mission, the plans 5 s later */
static byte Bet_PreGivePlans2(void) {
	return Mode_SaveAdd(HR, "Hangar.shipsToEditList", "SMerc1") && Mode_SaveAdd(HR, "Hangar.availableEnginesList", "MMERC1") &&
		LV_OWN(HR, "SetNextMission", NULL, "") && Mode_Later(5, HR, "GivePlans", NULL, 0, NULL);
}

/* GivePlans(): the message of the new blueprints and Rusty */
static byte Bet_GivePlans(void) {
	nativeVal_t cad = { 0 };
	Mode_Lang("Mission_FirstCrazyBet_SystemMessage3", &cad);
	byte ok = Mode_SpeechMsg(&cad) && LV_CALL("Scrap", "IncSaveVar", NULL, "si", "NewBluePrintAvaliable", 2L) &&
		LvN_Com("Mission_FirstCrazyBet_RemoteMessage9", "Rusty", NULL, NULL, NULL) && Mode_WayPoint();
	Native_Free(&cad);
	return ok;
}

/* SetNextMission(): the doors open again, on to Mission_MeetArchbishop, the places of the characters */
static byte Bet_SetNextMission(void) {
	static const char *const doors[] = { "DM_Door_CityDoorA_00", "DM_Door_CityDoorA_01", "DM_Door_Outmap_Commercial", "DM_Door_Outmap_Scrapyard",
		"DM_Door_Outmap_Outskirts", NULL };
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Temple", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Temple", NULL };
	nativeVal_t pc = { 0 };
	byte ok = Mode_Import("PhoneCab") && Mode_EntSetI("DM_Door_MetroExt_Downtown", "ArrowCanUse", 1);
	for (int i = 0; ok && doors[i]; i++) {
		ok = Mode_EntSetS(doors[i], "OnObjSlot", "!AutoDoorSlot");
	}
	Native_SetNone(&pc);
	ok = ok && LV_CALL("PhoneCab", "GetFree", &pc, "") && LvN_SetS(&pc, "UseFunc", "PhoneCab.DefUseAudioCom") &&
		Mode_DelSaveVars("HumphreyRaceTutorial") && Mode_DelSaveVars("MissionState") &&
		LvN_Next("Mission_MeetArchbishop", "Temple", "Mission_FirstContactPress_MissionText3");
	Native_Free(&pc);
	if (ok) {
		Mode_Saves(places);
	}
	return ok && Mode_WayPoint();
}

LV_CORE0(Bet_InitStatus)
LV_CORE0(Bet_ShowSystemMessage)
LV_CORE0(Bet_Init)
LV_CORE0(Bet_ShowBoostMessage)
LV_CORE0(Bet_ClosePressHangar)
LV_COREA(Bet_DelRacers)
LV_CORE0(Bet_CallCrazyGambler)
LV_COREA(Bet_UsePhoneCab)
LV_CORE0(Bet_PreGivePlans2)
LV_CORE0(Bet_GivePlans)
LV_CORE0(Bet_SetNextMission)
LV_FN(Bet, InitStatusPy, 1, Bet_InitStatus_C)
LV_FN(Bet, ShowSystemMessagePy, 0, Bet_ShowSystemMessage_C)
LV_FN(Bet, InitPy, 1, Bet_Init_C)
LV_FN(Bet, ShowBoostMessagePy, 0, Bet_ShowBoostMessage_C)
LV_FN(Bet, ClosePressHangarPy, 0, Bet_ClosePressHangar_C)
LV_FN(Bet, DelRacersPy, 1, Bet_DelRacers_C)
LV_FN(Bet, CallCrazyGamblerPy, 0, Bet_CallCrazyGambler_C)
LV_FN(Bet, UsePhoneCabPy, 4, Bet_UsePhoneCab_C)
LV_FN(Bet, PreGivePlans2Py, 0, Bet_PreGivePlans2_C)
LV_FN(Bet, GivePlansPy, 0, Bet_GivePlans_C)
LV_FN(Bet, SetNextMissionPy, 0, Bet_SetNextMission_C)

PyMethodDef g_DownTownHumphreyRaceMethods[] = {
	{ "InitStatus", Bet_InitStatusPy, 1, NULL },
	{ "ShowSystemMessage", Bet_ShowSystemMessagePy, 1, NULL },
	{ "Init", Bet_InitPy, 1, NULL },
	{ "ShowBoostMessage", Bet_ShowBoostMessagePy, 1, NULL },
	{ "ClosePressHangar", Bet_ClosePressHangarPy, 1, NULL },
	{ "DelRacers", Bet_DelRacersPy, 1, NULL },
	{ "CallCrazyGambler", Bet_CallCrazyGamblerPy, 1, NULL },
	{ "UsePhoneCab", Bet_UsePhoneCabPy, 1, NULL },
	{ "PreGivePlans2", Bet_PreGivePlans2Py, 1, NULL },
	{ "GivePlans", Bet_GivePlansPy, 1, NULL },
	{ "SetNextMission", Bet_SetNextMissionPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DistractHumphrey: Humphrey and the player race to destroy ten ships, the player must keep him busy ---- */

static const char *const humphrey_profile[] = {
	"Ship=SHUMP1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=2", "UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25",
	"UsePercHook=0", "FactorHookMap=10", "CadenceShoot=0", "ControlAim=10", "AngAttack=3.1415", "CounterAttack=1", "ControlBoost=1",
	"ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=1.0", "FactorDamageItemLife=0.35", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=0", "UsePercHookGetAway=0", "TimeMaxStrafe=10", "VelMaxStrafe=1.0", NULL,
};

static PyObject *Hump_GetHumphreyProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, DH, humphrey_profile);
}

static PyObject *Hump_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, DH, "[\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Init(LevelPath): the step of the mission by MissionState */
static byte Hump_Init(void) {
	nativeVal_t model = { 0 };
	byte ok = Mode_G(DH, "MeetingPoint_ModelFile", &model) && Mode_Preload(&model);
	Native_Free(&model);
	if (!ok) {
		return 0;
	}
	if (LvN_SaveIs("MissionState", "CallHumphrey")) {
		return LV_OWN(DH, "CallHumphrey", NULL, "");
	}
	if (LvN_SaveIs("MissionState", "DistractHumphrey")) {
		return LvN_SaveIs("DistractionStart", "1") ?
			LvN_Com("Mission_DistractHumphrey_RemoteMessage6", "DeepThroat", DH, "CreateWaypoint", LvN_NoArgs()) :
			LV_OWN(DH, "CreateWaypoint", NULL, "");
	}
	return !LvN_SaveIs("MissionState", "CallDeepThroat") || LV_OWN(DH, "PreCallDeepThroat", NULL, "");
}

static byte Hump_CallHumphrey(void) {
	return Mode_PhoneCab("Mission_DistractHumphrey.UsePhoneCab", 1, NULL, NULL);
}

static byte Hump_UsePhoneCab(const nativeVal_t *a) {
	return Dt_UsePhone(a, DH, "Generics_StillPolice1_Humphrey", "Humphrey", "Mission_TalkToHumphrey_ComHumphrey_NI_1Msg", "PreDistractHumphrey", 0);
}

/* CreateWaypoint(): the meeting point at the dummy of the mission */
static byte Hump_CreateWaypoint(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = TV(), *p = TV(), *wp = TV(), *model = TV(), *ang = TV();
	LvN_Dummy("DM_Mission_DistractHumphrey", 1, dummy);
	if (!Native_True(dummy)) {
		return Mode_Done(&t, LvN_RaiseText("No existe el dummy para esta misi\xf3n: 'DM_Mission_DistractHumphrey'"));
	}
	byte ok = LvN_Item(dummy, 1, p) && LvN_CreateAt(Mode_S(&t, "MissionStart"), p, "Item", wp) && Mode_Put(DH, "missionStartWaypoint", NULL, wp) &&
		Mode_G(DH, "MeetingPoint_ModelFile", model) && Mode_VModel(wp, model) && LvN_SetI(wp, "RadarShow", 1) && LvN_SetI(wp, "Radius", 3500);
	if (ok) {
		Mode_Ints3(ang, 0, 180, 0);
		ok = Mode_VSet(wp, "Ang", ang) && LvN_SetS(wp, "OnTake", "Mission_DistractHumphrey.OnStartDistraction") &&
			LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", wp, 1L);
	}
	return Mode_Done(&t, ok);
}

/* OnStartDistraction(ItemName, VehicleName): the player's ship at the meeting point starts the distraction (not with
 * the alarm on or an enemy around) */
static byte Hump_OnStartDistraction(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *name = TV(), *busy = TV(), *wp = TV(), *pos = TV();
	if (!Mode_Import("Parking") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VGet(usr, "Name", name)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(name, &a[1])) {
		return Mode_Done(&t, 1);
	}
	int b = LV_CALL("SLogic", "IsEnemyActive", busy, "") ? Native_True(busy) : -1;
	if (b == 0) {
		b = Mode_AlarmOn();
	}
	if (b != 0) {
		return Mode_Done(&t, b > 0 && Mode_G(DH, "missionStartWaypoint", wp) && LvN_SetS(wp, "OnTake", "") &&
			LvN_Com("Mission_DistractHumphrey_RemoteMessage11", "Humphrey", DH, "EnableMissionWaypoint", NULL));
	}
	byte ok = (!LvN_SaveIs("DistractionStart", "0") || Mode_SaveS("DistractionStart", "1")) && Mode_G(DH, "missionStartWaypoint", wp) &&
		LvN_SetI(wp, "Active", 0) && Mode_Read(DH, "missionStartWaypoint", wp) && Mode_VGet(wp, "Pos", pos) &&
		LV_OWN(DH, "CreateHumphrey", NULL, "v", pos) && LV_OWN(DH, "StartCameraTravelling", NULL, "");
	return Mode_Done(&t, ok);
}

static byte Hump_EnableMissionWaypoint(void) {
	nativeVal_t wp = { 0 };
	byte ok = Mode_G(DH, "missionStartWaypoint", &wp) &&
		(!Native_True(&wp) || LvN_SetS(&wp, "OnTake", "Mission_DistractHumphrey.OnStartDistraction"));
	Native_Free(&wp);
	return ok;
}

/* StartCameraTravelling(): the player's ship flies to the meeting point while a camera comes close to it */
static byte Hump_StartCameraTravelling(void) {
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
		Mode_G(DH, "missionStartWaypoint", wp) && Mode_VGet(wp, "Pos", wpos) && Mode_VSet(usr, "MovePos", wpos) && Mode_VGet(wp, "Ang", wang) &&
		Mode_VSet(usr, "MoveAng", wang) && LvN_SetF(usr, "MoveTime", 1.0) && LvN_SetI(usr, "MoveFade", 2) && LvN_SetI(usr, "AvoidCrash", 1) &&
		Mode_GVarSetS("EscapeEvent", "") && Mode_GVarSetS("MainMissionEvent", "") && LV_CALL("Scrap", "GetCam", prev, "i", 0L);
	if (ok) {
		Mode_Ints3(add, 15, 0, 0);
		Mode_Ints3(rot, 0, 0, -8000);
		ok = Mode_VecAdd(wang, add, ang) && Mode_Rotate(rot, ang, r0) && Mode_VecAdd(r0, wpos, p0);
	}
	if (ok) {
		Mode_Ints3(rot, 0, 0, -2000);
		ok = Mode_Rotate(rot, ang, r1) && Mode_VecAdd(r1, wpos, p1) && LvN_CreateAt(Mode_S(&t, "RaceCamera"), p0, "CamPath", cam) &&
			LvN_SetI(cam, "nWayPoint", 2) && Mode_VSet(cam, "TgtCam00", prev) && LvN_SetS(cam, "TgtCam00", "") && Mode_VSet(cam, "Pos00", p0) &&
			Mode_VSet(cam, "Ang00", ang) && LvN_SetI(cam, "FadeIn00", 1) && LvN_SetI(cam, "FadeOut00", 0) && LvN_SetF(cam, "Time00", 5.0) &&
			Mode_VSet(cam, "TgtCam01", prev) && LvN_SetS(cam, "TgtCam01", "") && Mode_VSet(cam, "Pos01", p1) && LvN_Item(ang, 0, v);
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
			Mode_AtG(at, DH, "AbortCameraTravelling", args, 2) && Mode_Import("Scene") &&
			LV_CALL("Scene", "SetText", NULL, "sv", "Mission_DistractHumphrey_RemoteMessage12", tt);
	}
	return Mode_Done(&t, ok);
}

/* AbortCameraTravelling(UsrEntityName, prevCamera): the player's control and camera back, the mission starts */
static byte Hump_AbortCameraTravelling(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *cn = TV(), *cam = TV();
	byte ok = 1;
	if (Native_True(&a[1])) {
		ok = Mode_EntityH(&a[0], usr) && (!Native_True(usr) || (LvN_SetF(usr, "MoveTime", 0.0) && LvN_SetI(usr, "UsrControl", 0))) &&
			Mode_GVarSetS("EscapeEvent", "Menu.Init") && Mode_GVarSetS("MainMissionEvent", "MissionsFuncs.ShowMissionInfo") &&
			LV_CALL("Scrap", "GetCam", cn, "i", 0L) && Mode_EntityH(cn, cam) && Mode_VDel(cam) && LV_CALL("Scrap", "SetCam", NULL, "iv", 0L, &a[1]);
	}
	ok = ok && LV_OWN(DH, "LaunchMission", NULL, "");
	return Mode_Done(&t, ok);
}

/* CreateHumphrey(targetPos): Humphrey (again), 5000 beside the meeting point flying to it, or after a ship */
static byte Hump_CreateHumphrey(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	const nativeVal_t *target = &a[0];
	nativeVal_t *initial = TV(), *x = TV(), *hn = TV(), *h = TV(), *pd = TV(), *name = TV(), *tx = TV(), *ty = TV(), *tz = TV();
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("RaceMaker") && Mode_Import("OutMap");
	if (ok && Native_True(target)) {
		Native_SetEmpty(initial, NV_TUPLE);
		ok = LvN_Item(target, 0, x) && Native_Arith('+', x, Mode_I(&t, 5000), Native_Append(initial)) && LvN_Item(target, 1, Native_Append(initial)) &&
			LvN_Item(target, 2, Native_Append(initial));
	} else {
		Native_Copy(initial, target);
	}
	ok = ok && Mode_G(DH, "humphreyName", hn) && Mode_EntityH(hn, h) && (!Native_True(h) || Mode_VDel(h));
	Native_Free(h);
	ok = ok && Mode_SpawnCar("Humphrey", 0, h, NULL) && LvN_StdProfile(DH, "GetHumphreyProfile", h, pd) && LvN_SetS(h, "TgType", "green") &&
		LvN_SetS(h, "OnDeath", "Mission_DistractHumphrey.OnHumphreyDeath") && LvN_SetS(h, "OnDamage", "Mission_DistractHumphrey.OnHumphreyDamage") &&
		LvN_Life(h, DH, "HUMPHREY_LIFE") && LvN_SetS(h, "WeapList", "0,0,7,0,7,0") && Mode_VGet(h, "Name", name) && Mode_Put(DH, "humphreyName", NULL, name);
	if (ok && Native_True(initial)) {
		ok = Mode_VSet(h, "Pos", initial) && LV_CALL("RaceMaker", "PointObjectTo", NULL, "vv", h, target) && Mode_VGet(h, "Name", name) &&
			LvN_Item(target, 0, tx) && LvN_Item(target, 1, ty) && LvN_Item(target, 2, tz) &&
			LV_CALL("SAI", "SetStateVehicle", NULL, "ivvvviiddd", 1L, name, tx, ty, tz, 4500L, 0L, 0.1, 20.0, 20.0);
	} else if (ok) {
		ok = LV_OWN(DH, "AssignNewHumphreyTarget", NULL, "");
	}
	return Mode_Done(&t, ok);
}

/* LaunchMission(): the damage callback, the timer and the scorer, Humphrey's target, DeepThroat's messages */
static byte Hump_LaunchMission(void) {
	static const char *const keys[] = { "Mission_DistractHumphrey_RemoteMessage1", "Mission_DistractHumphrey_RemoteMessage2",
		"Mission_DistractHumphrey_RemoteMessage3", "Mission_DistractHumphrey_RemoteMessage4" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *mt = TV(), *max = TV(), *rep = TV(), *wp = TV(), *iv = TV(), *d = TV(), *com = TV(), *who = TV();
	byte ok = Mode_Import("Menu") && Mode_Import("RacerScorer") && Mode_Import("Vehicles") &&
		Mode_GVarSetS("OnGeneralDamage", "Mission_DistractHumphrey.OnGeneralDamage") && Mode_G(DH, "missionTime", mt) &&
		Mode_GVarSet("TimeSpecialUseTimer", mt) && Mode_GVarSetI("CounterType", 0) && Mode_G(DH, "maxShips", max);
	if (ok) {
		Mode_Repr(max, rep);
		ok = LV_CALL("RacerScorer", "InitMissionScorer", NULL, "iiv", 0L, 0L, rep) && LvN_ScorerCallG(DH, "MissionScorer") && LvN_Draw() &&
			LV_OWN(DH, "AssignNewHumphreyTarget", NULL, "") && Mode_G(DH, "missionStartWaypoint", wp) && Mode_VDel(wp) &&
			(Native_SetNone(d), Mode_Put(DH, "missionStartWaypoint", NULL, d)) && Mode_Read(DH, "missionTime", mt) &&
			LvN_Arith('/', mt, Mode_I(&t, 4), iv);
	}
	LvN_Str(who, "DeepThroat");
	for (long i = 0; ok && i < 4; i++) {
		Native_Free(d);
		Native_Free(com);
		LvN_Str(com, keys[i]);
		if (i == 0) {
			Native_SetInt(d, 2);
		} else {
			ok = i == 1 ? (Native_Copy(d, iv), 1) : LvN_Arith('*', Mode_I(&t, i), iv, d);
		}
		/* com and who are the two parameters (one after the other in t) */
		ok = ok && Mode_LaterSpeech("AddRemoteCom", d, com, 2, "DTMessages");
	}
	ok = ok && LvN_ModSetGlobal("Vehicles", "OnDeathCallback", DH, "OnDeath");
	return Mode_Done(&t, ok);
}

/* Humphrey after the ship name: SAI.SetStateVehicle(2, humphreyName, name); a ship he takes stops its own moves (1
 * when he changed his state, 0, -1 with the error) */
static int Hp_Attack(const nativeVal_t *e, const nativeVal_t *name) {
	nativeVal_t hn = { 0 }, changed = { 0 };
	int c = Mode_Active(e);
	if (c <= 0) {
		return c;
	}
	Native_SetNone(&changed);
	c = Mode_G(DH, "humphreyName", &hn) && LV_CALL("SAI", "SetStateVehicle", &changed, "ivv", 2L, &hn, name) ? Native_True(&changed) : -1;
	if (c > 0 && !LvN_SetI(e, "AI_Repos", 0)) {
		c = -1;
	}
	Native_Free(&hn);
	Native_Free(&changed);
	return c;
}

/* AssignNewHumphreyTarget(): Humphrey after the ship the player is shooting, or the ship nearest the player (tried
 * again every second until he has one) */
static byte Hump_AssignNewHumphreyTarget(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *hn = TV(), *h = TV(), *st = TV(), *e = TV(), *r = TV(), *p = TV(), *usr = TV(), *pos = TV(), *name = TV();
	if (!Mode_G(DH, "humphreyName", hn) || !Mode_EntityH(hn, h)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(h)) {
		return Mode_Done(&t, 1);
	}
	int changed = 0;
	int taunt = Mode_G(DH, "shipNameForTaunt", st) && Mode_EntityH(st, e) ? Native_True(e) : -1;
	if (taunt > 0) {
		taunt = Mode_RandD(0.0, 0.99, r) && Mode_G(DH, "probHumpreyAttacksPlayerTarget", p) ? Native_Compare(r, p) < 0 : -1;
	}
	if (taunt > 0) {
		changed = Mode_Read(DH, "shipNameForTaunt", st) ? Hp_Attack(e, st) : -1;
	} else if (taunt == 0) {
		int u = Mode_UsrHandle(usr) ? Native_True(usr) : -1;
		if (u > 0) {
			Native_Free(e);
			changed = Mode_VGet(usr, "Pos", pos) && Mode_Read(DH, "shipNameForTaunt", st) &&
				LV_CALL("SLogic", "GetNearestShip", name, "vv", pos, st) && Mode_EntityH(name, e) ? Hp_Attack(e, name) : -1;
		} else if (u < 0) {
			changed = -1;
		}
	} else {
		changed = -1;
	}
	byte ok = changed >= 0 && (changed || Mode_Later(1, DH, "AssignNewHumphreyTarget", NULL, 0, NULL));
	return Mode_Done(&t, ok);
}

/* OnHumphreyDeath(LooserName, WinnerName): his target moves again; during the mission it starts over, after it he
 * comes back */
static byte Hump_OnHumphreyDeath(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = TV(), *tg = TV(), *e = TV(), *none = TV();
	byte ok = Mode_Import("Vehicles") && LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && Mode_EntityH(&a[0], l) &&
		Mode_VGet(l, "AI_Target", tg) && Mode_EntityH(tg, e);
	int c = ok ? Mode_Active(e) : -1;
	ok = c >= 0 && (!c || LvN_SetI(e, "AI_Repos", 1));
	c = ok ? LvN_GTrue(DH, "missionComplete") : -1;
	if (c == 0) {
		ok = LV_OWN(DH, "StopMission", NULL, "") && LvN_Com("Mission_DistractHumphrey_RemoteMessage6", "DeepThroat", DH, "CreateWaypoint", LvN_NoArgs());
	} else if (c > 0 && !LvN_SaveIs("MissionState", "CallDeepThroat")) {
		ok = Mode_Later(3, DH, "CreateHumphrey", none, 1, NULL);
	}
	return Mode_Done(&t, ok && c >= 0);
}

/* OnHumphreyDamage(LooserName, WinnerName, Damage): only the player can hurt him */
static byte Hump_OnHumphreyDamage(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *name = TV(), *h = TV(), *life = TV(), *sum = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	byte ok = Mode_VGet(usr, "Name", name);
	if (ok && Native_Compare(&a[1], name)) {
		ok = Mode_EntityH(&a[0], h) && Mode_VGet(h, "Life", life) && Native_Arith('+', life, &a[2], sum) && Mode_VSet(h, "Life", sum);
	}
	return Mode_Done(&t, ok);
}

/* OnGeneralDamage(LooserName, WinnerName, Damage): the ship the player hits is the one Humphrey may snatch */
static byte Hump_OnGeneralDamage(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *name = TV(), *hn = TV();
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	int c = Mode_VGet(usr, "Name", name) ? !Native_Compare(&a[1], name) : -1;
	if (c > 0) {
		c = Mode_G(DH, "humphreyName", hn) ? Native_Compare(&a[0], hn) != 0 : -1;
	}
	return Mode_Done(&t, c >= 0 && (!c || Mode_Put(DH, "shipNameForTaunt", NULL, &a[0])));
}

/* the end of the count when one side reached maxShips: the mission stops, Humphrey leaves; done when the time ran out
 * before (DeepThroat next), else it starts over */
static byte Hp_End(const char *doneKey, const char *failKey) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *hn = TV(), *h = TV();
	byte ok = LV_OWN(DH, "StopMission", NULL, "") && Mode_G(DH, "humphreyName", hn) && Mode_EntityH(hn, h) &&
		(!Native_True(h) || (Mode_Read(DH, "humphreyName", hn) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, hn, 0L)));
	int done = ok ? LvN_GTrue(DH, "missionComplete") : -1;
	if (done > 0) {
		ok = Mode_SaveS("MissionState", "CallDeepThroat") && LvN_Com(doneKey, "Humphrey", NULL, NULL, NULL) &&
			LvN_Com("Mission_DistractHumphrey_RemoteMessage17", "DeepThroat", DH, "PreCallDeepThroat", LvN_NoArgs());
	} else if (done == 0) {
		ok = LvN_Com(failKey, "Humphrey", NULL, NULL, NULL) &&
			LvN_Com("Mission_DistractHumphrey_RemoteMessage6", "DeepThroat", DH, "CreateWaypoint", LvN_NoArgs());
	}
	return Mode_Done(&t, ok && done >= 0 && LvN_PutI(DH, "missionComplete", 0));
}

/* `e = Scrap.GetEntity(humphreyName); if e: if SAI.GetStateVehicle(humphreyName) == 0 or LooserName == e.AI_Target:
 * AssignNewHumphreyTarget 1 s later` */
static byte Hp_Retarget(const nativeVal_t *looser) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *hn = TV(), *e = TV(), *st = TV(), *tg = TV();
	byte ok = Mode_G(DH, "humphreyName", hn) && Mode_EntityH(hn, e);
	if (ok && Native_True(e)) {
		int c = Mode_Read(DH, "humphreyName", hn) && Mode_AIState(hn, st) ? LvN_EqI(st, 0) : -1;
		if (c == 0) {
			c = Mode_VGet(e, "AI_Target", tg) ? !Native_Compare(looser, tg) : -1;
		}
		ok = c >= 0 && (!c || Mode_Later(1, DH, "AssignNewHumphreyTarget", NULL, 0, NULL));
	}
	return Mode_Done(&t, ok);
}

/* OnDeath(LooserName, WinnerName): the count of the ships Humphrey and the player destroyed */
static byte Hump_OnDeath(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *uname = TV(), *e = TV(), *acu = TV(), *hn = TV(), *mine = TV(), *max = TV(), *st = TV(), *nt = TV(), *n = TV();
	nativeVal_t *rep = TV(), *key = TV(), *kp = TV(), *kh = TV(), *n1 = TV();
	if (!Mode_Import("RacerScorer") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		LvN_Str(uname, "");
	} else if (!Mode_VGet(usr, "Name", uname)) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_EntityH(&a[0], e) ? (Native_True(e) ? (Mode_VGet(e, "ArrowCanUse", acu) ? LvN_EqI(acu, 1) : -1) : 0) : -1;
	if (c != 0) {
		return Mode_Done(&t, c > 0 && LV_CALL("Speech", "RemoteComunication", NULL, "ss", "Mission_DistractHumphrey_RemoteMessage13", "Humphrey"));
	}
	if (!Mode_G(DH, "humphreyName", hn)) {
		return Mode_Done(&t, 0);
	}
	byte byHump = !Native_Compare(&a[1], hn), ok = 1;
	if (byHump || !Native_Compare(&a[1], uname)) {
		ok = LvN_GAdd(DH, "shipsKilledPlayer", byHump ? -1 : 1, NULL) && LvN_GAdd(DH, "shipsKilledHumphrey", byHump ? 1 : -1, NULL) &&
			Mode_G(DH, byHump ? "shipsKilledHumphrey" : "shipsKilledPlayer", mine) && Mode_G(DH, "maxShips", max);
		if (ok && !Native_Compare(mine, max)) {
			ok = byHump ? Hp_End("Mission_DistractHumphrey_RemoteMessage10", "Mission_DistractHumphrey_RemoteMessage7") :
				Hp_End("Mission_DistractHumphrey_RemoteMessage9", "Mission_DistractHumphrey_RemoteMessage8");
		} else if (ok && byHump) {
			ok = Mode_G(DH, "shipNameForTaunt", st);
			if (ok && !Native_Compare(&a[0], st)) {
				ok = Mode_G(DH, "numTaunt", nt) && Native_Arith('+', nt, Mode_I(&t, 14), n);
				if (ok) {
					Mode_Repr(n, rep);
				}
				ok = ok && Native_Arith('+', Mode_S(&t, "Mission_DistractHumphrey_RemoteMessage"), rep, key) &&
					LV_CALL("Speech", "AddRemoteCom", NULL, "vs", key, "Humphrey") && Mode_Read(DH, "numTaunt", nt) &&
					Native_Arith('+', nt, Mode_I(&t, 1), n1) && LvN_Arith('%', n1, Mode_I(&t, 3), n) && Mode_Put(DH, "numTaunt", NULL, n);
			}
			ok = ok && Mode_G(DH, "shipsKilledPlayer", kp) && Mode_G(DH, "shipsKilledHumphrey", kh) &&
				LV_CALL("RacerScorer", "UpdateMissionScorer", NULL, "ivv", 0L, kp, kh) &&
				Mode_Later(1, DH, "AssignNewHumphreyTarget", NULL, 0, NULL);
		} else if (ok) {
			ok = Mode_G(DH, "shipsKilledPlayer", kp) && Mode_G(DH, "shipsKilledHumphrey", kh) &&
				LV_CALL("RacerScorer", "UpdateMissionScorer", NULL, "ivv", 0L, kp, kh) && Hp_Retarget(&a[0]);
		}
	} else {
		ok = Hp_Retarget(&a[0]);
	}
	ok = ok && Mode_Read(DH, "shipNameForTaunt", st) && (Native_Compare(&a[0], st) || LvN_PutS(DH, "shipNameForTaunt", ""));
	return Mode_Done(&t, ok);
}

/* StopMission(): Humphrey back to the traffic, no callbacks, the counts to 0, no scorer */
static byte Hump_StopMission(void) {
	nativeVal_t hn = { 0 }, h = { 0 };
	Native_SetNone(&h);
	byte ok = Mode_Import("Vehicles") && Mode_G(DH, "humphreyName", &hn) && Mode_EntityH(&hn, &h) &&
		(!Native_True(&h) || (LvN_SetS(&h, "OnDeath", "") && LvN_SetS(&h, "OnDamage", "") && LvN_SetS(&h, "TgType", "none") &&
			LvN_SetS(&h, "EntityList", "Traffic"))) &&
		LvN_ModNone("Vehicles", "OnDeathCallback") && Mode_GVarSetS("OnGeneralDamage", "") && Mode_DelSchedS("DTMessages") &&
		LvN_PutI(DH, "shipsKilledPlayer", 0) && LvN_PutI(DH, "shipsKilledHumphrey", 0) && LvN_PutS(DH, "shipNameForTaunt", "") &&
		LvN_PutI(DH, "numTaunt", 0) && LvN_ModNone("Vehicles", "OnDeathCallback") && LvN_ScorerOff() && LvN_Draw();
	Native_Free(&hn);
	Native_Free(&h);
	return ok;
}

static byte Hump_CallDeepThroat(void) {
	return Mode_PhoneCab("Mission_DistractHumphrey.UsePhoneCab2", 1, NULL, NULL);
}

static byte Hump_UsePhoneCab2(const nativeVal_t *a) {
	return Dt_UsePhone(a, DH, "Generics_StillPolice1_Functionary", "DeepThroat", "Mission_TalkToDeepThroat9_ComDeepThroat_NI_1Msg",
		"SetNextMission", 0);
}

/* MissionScorer(id): the time left (hidden once it ran out) and the counts of RacerScorer */
static byte Hump_MissionScorer(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lab = TV(), *area = TV(), *wh = TV(), *h = TV(), *x = TV(), *xw = TV(), *x30 = TV(), *y = TV();
	(void)h;
	byte ok = Mode_Import("RacerScorer") && Mode_LvLabel(id, "RemainingTime", 0, "Mission_DistractHumphrey_Scorer1", 0);
	int c = ok ? LvN_GTrue(DH, "missionComplete") : -1;
	ok = c >= 0 && (!c || Mode_ScorerSetI(id, "RemainingTime", "Visible", 0));
	if (ok) {
		LvN_Label("Mission_DistractHumphrey_Scorer1", lab);
		ok = Mode_TextArea("Horatio", lab, area) && Mode_Unpack(area, 2, wh) && LvN_TextX(0, x) && Native_Arith('+', x, &wh[0], xw) &&
			Native_Arith('+', xw, Mode_I(&t, 30), x30) && LvN_TextY(12, y) &&
			Mode_LvTimer(id, "MissionTimer", x30, y, "Mission_DistractHumphrey.TimeExpired", 0) && LV_CALL("RacerScorer", "MissionScorer", NULL, "i", 0L);
	}
	return Mode_Done(&t, ok);
}

LV_CORE0(Hump_Init)
LV_CORE0(Hump_CallHumphrey)
LV_COREA(Hump_UsePhoneCab)
LV_CORE0(Hump_CreateWaypoint)
LV_COREA(Hump_OnStartDistraction)
LV_CORE0(Hump_EnableMissionWaypoint)
LV_CORE0(Hump_StartCameraTravelling)
LV_COREA(Hump_AbortCameraTravelling)
LV_COREA(Hump_CreateHumphrey)
LV_CORE0(Hump_LaunchMission)
LV_CORE0(Hump_AssignNewHumphreyTarget)
LV_COREA(Hump_OnHumphreyDeath)
LV_COREA(Hump_OnHumphreyDamage)
LV_COREA(Hump_OnGeneralDamage)
LV_COREA(Hump_OnDeath)
LV_CORE0(Hump_StopMission)
LV_CORE0(Hump_CallDeepThroat)
LV_COREA(Hump_UsePhoneCab2)
LV_COREA(Hump_MissionScorer)
LV_FN(Hump, InitPy, 1, Hump_Init_C)
LV_FN(Hump, CallHumphreyPy, 0, Hump_CallHumphrey_C)
LV_FN(Hump, UsePhoneCabPy, 4, Hump_UsePhoneCab_C)
LV_FN(Hump, CreateWaypointPy, 0, Hump_CreateWaypoint_C)
LV_FN(Hump, OnStartDistractionPy, 2, Hump_OnStartDistraction_C)
LV_FN(Hump, EnableMissionWaypointPy, 0, Hump_EnableMissionWaypoint_C)
LV_FN(Hump, StartCameraTravellingPy, 0, Hump_StartCameraTravelling_C)
LV_FN(Hump, AbortCameraTravellingPy, 2, Hump_AbortCameraTravelling_C)
LV_FN(Hump, CreateHumphreyPy, 1, Hump_CreateHumphrey_C)
LV_FN(Hump, LaunchMissionPy, 0, Hump_LaunchMission_C)
LV_FN(Hump, AssignNewHumphreyTargetPy, 0, Hump_AssignNewHumphreyTarget_C)
LV_FN(Hump, OnHumphreyDeathPy, 2, Hump_OnHumphreyDeath_C)
LV_FN(Hump, OnHumphreyDamagePy, 3, Hump_OnHumphreyDamage_C)
LV_FN(Hump, OnGeneralDamagePy, 3, Hump_OnGeneralDamage_C)
LV_FN(Hump, OnDeathPy, 2, Hump_OnDeath_C)
LV_FN(Hump, StopMissionPy, 0, Hump_StopMission_C)
LV_FN(Hump, CallDeepThroatPy, 0, Hump_CallDeepThroat_C)
LV_FN(Hump, UsePhoneCab2Py, 4, Hump_UsePhoneCab2_C)
LV_FN(Hump, MissionScorerPy, 1, Hump_MissionScorer_C)

PyMethodDef g_DownTownDistractHumphreyMethods[] = {
	{ "GetHumphreyProfile", Hump_GetHumphreyProfile, 1, NULL },
	{ "InitStatus", Hump_InitStatus, 1, NULL },
	{ "Init", Hump_InitPy, 1, NULL },
	{ "CallHumphrey", Hump_CallHumphreyPy, 1, NULL },
	{ "UsePhoneCab", Hump_UsePhoneCabPy, 1, NULL },
	{ "CreateWaypoint", Hump_CreateWaypointPy, 1, NULL },
	{ "OnStartDistraction", Hump_OnStartDistractionPy, 1, NULL },
	{ "EnableMissionWaypoint", Hump_EnableMissionWaypointPy, 1, NULL },
	{ "StartCameraTravelling", Hump_StartCameraTravellingPy, 1, NULL },
	{ "AbortCameraTravelling", Hump_AbortCameraTravellingPy, 1, NULL },
	{ "CreateHumphrey", Hump_CreateHumphreyPy, 1, NULL },
	{ "LaunchMission", Hump_LaunchMissionPy, 1, NULL },
	{ "AssignNewHumphreyTarget", Hump_AssignNewHumphreyTargetPy, 1, NULL },
	{ "OnHumphreyDeath", Hump_OnHumphreyDeathPy, 1, NULL },
	{ "OnHumphreyDamage", Hump_OnHumphreyDamagePy, 1, NULL },
	{ "OnGeneralDamage", Hump_OnGeneralDamagePy, 1, NULL },
	{ "OnDeath", Hump_OnDeathPy, 1, NULL },
	{ "StopMission", Hump_StopMissionPy, 1, NULL },
	{ "CallDeepThroat", Hump_CallDeepThroatPy, 1, NULL },
	{ "UsePhoneCab2", Hump_UsePhoneCab2Py, 1, NULL },
	{ "MissionScorer", Hump_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_MultiplusRace: Ficus' race against the police, then Stupidus and his friends ---- */

static const char *const multiplus_profile[] = {
	"Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=1", "GoNextItemRace=0", "ControlHookRace=0", "CadenceShootCM=6",
	"UsePercSonic=90", "UsePercCloud=10", "UsePercElectro=0", "UsePercHook=0", "FactorHookMap=2", "CadenceShoot=6", "ControlAim=2",
	"AngAttack=1.5", "CounterAttack=0.5", "ControlBoost=0", "ControlBoostItemLife=1", "ControlBoostItemAmmo=0", "FactorDamageGetAway=0.25",
	"FactorDamageItemLife=0.4", "FactorItemAmmo=0.1", "ControlStrafeEvasion=0", "UsePercHookGetAway=0", "TimeMaxStrafe=4", "VelMaxStrafe=0.5",
	NULL,
};

static PyObject *Plus_GetFunctionaryProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, MR, multiplus_profile);
}

/* InitStatus(LevelPath): the race of the mission is the only one of the level, no alarm */
static byte Plus_InitStatus(void) {
	return Mode_Import("MapRace") && Mode_ModListG("MapRace", "Races", MR, "RaceList") && Mode_SaveS("AlarmActive", "0") &&
		Mode_SaveS("AlarmStatus", "0") && Mode_AcShips(MR, "[\"SMayor1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Init(LevelPath): the race, the phone cab of Ficus or the hunt of Stupidus by the step of the mission */
static byte Plus_Init(void) {
	byte ok = 1;
	if (LvN_SaveIs("MultiplusRace", "Begin")) {
		ok = LV_OWN(MR, "InitRace", NULL, "");
	} else if (LvN_SaveIs("MultiplusRace", "Alarm") || LvN_SaveIs("MultiplusRace", "Left") || LvN_SaveIs("MultiplusRace", "Loose") ||
		LvN_SaveIs("MultiplusRace", "KillStupidus")) {
		ok = LV_OWN(MR, "SetPhoneCab", NULL, "");
	}
	return ok && (!LvN_SaveIs("MultiplusRace", "Killing") || LV_OWN(MR, "InitKill", NULL, ""));
}

/* RaceList(): the checkpoints of the race (those of EasyRaceDownTown2) */
static PyObject *Plus_RaceList(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_StartPos(args, races[1].pos, races[1].n);
}

static PyObject *Plus_GetDealType(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return PyString_FromString("Race");
}

/* EndOfRace(ItemName, VehicleName): Ficus on the end of the race (the police, a win, a defeat), his phone cab */
static byte Plus_EndOfRace(const nativeVal_t *a) {
	const nativeVal_t *vehicle = &a[1];
	nativeVal_t usr = { 0 }, name = { 0 };
	byte ok;
	Native_SetNone(&usr);
	Native_SetNone(&name);
	if (LvN_EqS(vehicle, "Alarm") || LvN_EqS(vehicle, "Killed") || LvN_EqS(vehicle, "CounterMeasure")) {
		ok = LvN_Com("Mission_WinRaceMultiplus_RemoteMessage1", "Ficus", NULL, NULL, NULL) && Mode_SaveS("MultiplusRace", "Alarm");
	} else if (Mode_UsrHandle(&usr) && Mode_VGet(&usr, "Name", &name)) {
		ok = !Native_Compare(vehicle, &name) ?
			LvN_Com("Mission_WinRaceMultiplus_RemoteMessage3", "Ficus", MR, "PreKillStupidus", LvN_NoArgs()) :
			LvN_Com("Mission_WinRaceMultiplus_RemoteMessage2", "Ficus", NULL, NULL, NULL) &&
				Mode_SaveS("MultiplusRace", LvN_EqS(vehicle, "Left") ? "Left" : "Loose");
	} else {
		ok = 0;
	}
	Native_Free(&usr);
	Native_Free(&name);
	return ok && Mode_Later(0.5, MR, "SetPhoneCab", NULL, 0, NULL);
}

static byte Plus_SetPhoneCab(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pc = TV(), *name = TV();
	byte ok = LV_CALL("PhoneCab", "GetFree", pc, "") && Mode_VGet(pc, "Name", name) &&
		LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name) && LvN_SetS(pc, "UseFunc", "Mission_MultiplusRace.UsePhoneCab");
	return Mode_Done(&t, ok);
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): Ficus by the end of the race: the race again or the hunt of
 * Stupidus */
static byte Plus_UsePhoneCab(const nativeVal_t *a) {
	static const char *const steps[][3] = {
		{ "Alarm", "Mission_WinRaceMultiplus_ComFicus_InitMsg1", "Mission_WinRaceMultiplus_ComFicus_InitAns1" },
		{ "Left", "Mission_WinRaceMultiplus_ComFicus_InitMsg2", "Mission_WinRaceMultiplus_ComFicus_InitAns2" },
		{ "Loose", "Mission_WinRaceMultiplus_ComFicus_InitMsg3", "Mission_WinRaceMultiplus_ComFicus_InitAns3" },
		{ "KillStupidus", "Mission_WinRaceMultiplus_ComFicus_InitMsg4", "Mission_WinRaceMultiplus_ComFicus_InitAns4" },
	};
	nativeVal_t pc = { 0 };
	Native_SetNone(&pc);
	int alarm = Mode_SetModVar("Speech", "NPCName", &a[1]) && LV_CALL("PhoneCab", "UseAudioCom", NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]) &&
		Mode_EntityH(&a[1], &pc) ? Mode_AlarmOn() : -1;
	byte ok = alarm >= 0;
	if (alarm > 0) {
		ok = LV_CALL("Speech", "OnObjSpch", NULL, "vsss", &pc, "Ficus", "Mission_WinRaceMultiplus_ComFicus_InitMsg1",
			"Mission_WinRaceMultiplus_RemoteMessage1");
	} else if (alarm == 0) {
		int i = 0;
		while (i < 4 && !LvN_SaveIs("MultiplusRace", steps[i][0])) {
			i++;
		}
		if (i == 4) {
			ok = LV_CALL("PhoneCab", "DefUseAudioCom", NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]);
		} else {
			ok = LV_CALL("Speech", "OnObjSpch", NULL, "vsss", &pc, "Ficus", steps[i][1], steps[i][2]) &&
				(i < 3 ? LvN_OnEnd(MR, "InitRace", NULL) && Mode_SaveS("MultiplusRace", "Begin") : LvN_OnEnd(MR, "PreInitKill", NULL));
		}
	}
	Native_Free(&pc);
	return ok;
}

/* InitRace(): the race against the police with the callbacks of the mission */
static byte Plus_InitRace(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *zero = TV(), *police = TV(), *name = TV(), *oor = TV();
	Native_SetInt(zero, 0);
	LvN_Str(police, "Police");
	byte ok = Mode_UsrHandle(usr) && Mode_SetAlarm(zero) && Mode_SaveS("Challenge.Money", "0") && Mode_SaveG("Race.Profile", MR, "RACE_PROFILE") &&
		LvN_ModSetGlobal("RaceMaker", "GetDealType", MR, "GetDealType") && LvN_ModSetGlobal("RaceMaker", "EndOfRaceCallback", MR, "EndOfRace") &&
		LvN_ModSetGlobal("RaceMaker", "OnInitialTaunt", MR, "AfterInitRaceTaunt");
	int car = ok ? Mode_VIsType(usr, "Car") : -1;
	ok = car >= 0 && Mode_CallKw("RaceMaker", "InitRace", NULL, police, 1,
		Py_BuildValue("{sisisi}", "numply", 5, "waypointlimit", 15, "userOnShip", car)) &&
		LvN_ModGet("RaceMaker", "ChallengerName", name) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name) &&
		LvN_ModGet("Speech", "OnOutOfRange", oor) && Mode_Put(MR, "outOfRange", NULL, oor) && LvN_ModNone("Speech", "OnOutOfRange");
	return Mode_Done(&t, ok);
}

static byte Plus_AfterInitRaceTaunt(void) {
	byte called;
	return Mode_CallIf(MR, "outOfRange", &called) && (!called || Mode_GVarSetN("RemainTalkTime", 0.0));
}

/* InitKill(): the scorer, Stupidus, the warning when the player leaves his ship */
static byte Plus_InitKill(void) {
	return LvN_ScorerCallG(MR, "MissionScorer") && LvN_Draw() && LV_OWN(MR, "SpawnStupidus", NULL, "") &&
		LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", MR, "HeyYouFight");
}

/* a functionary of Stupidus' gang into e: NormalTraffic at a free spawn point with the profile of the mission (global:
 * the global of the module it goes to first, read back) */
static byte Mr_Functionary(const char *global, nativeVal_t *e) {
	nativeVal_t pd = { 0 }, name = { 0 }, ship = { 0 };
	Native_SetNone(&pd);
	Native_SetNone(&name);
	Native_SetNone(&ship);
	byte ok = Mode_SpawnCar("NormalTraffic", 0, e, NULL) && (!global || (Mode_Put(MR, global, NULL, e) && Mode_Read(MR, global, e))) &&
		LV_OWN(MR, "GetFunctionaryProfile", &pd, "") && Mode_VGet(e, "Name", &name) && Mode_VGet(&pd, "Ship", &ship) &&
		LV_CALL("VehiclesNPC", "SetStdProperties", NULL, "vv", &name, &ship) && LV_CALL("VehiclesNPC", "ModAIProff", NULL, "vvi", e, &pd, 1L) &&
		LvN_Life(e, MR, "FUNC_LIFE");
	Native_Free(&pd);
	Native_Free(&name);
	Native_Free(&ship);
	return ok;
}

/* TOTAL_STUPIDUS_LIVES - d op lives: 1, 0, -1 */
static int Mr_LivesAre(const nativeVal_t *lives, const char *op, long d) {
	nativeVal_t total = { 0 }, x = { 0 };
	Native_SetNone(&x);
	int r = Mode_G(MR, "TOTAL_STUPIDUS_LIVES", &total) && Native_Arith('-', &total, &(nativeVal_t){ .type = NV_INT, .i = d }, &x) ?
		Mode_Cmp(lives, op, &x) : -1;
	Native_Free(&total);
	Native_Free(&x);
	return r;
}

/* SpawnStupidus(): Stupidus again; after his first life he brings messages and, from the fourth, friends */
static byte Plus_SpawnStupidus(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *st = TV(), *name = TV(), *lives = TV(), *num = TV();
	byte ok = Mode_Import("Speech") && Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mr_Functionary("Stupidus", st) &&
		LvN_SetS(st, "OnDeath", "Mission_MultiplusRace.StupidusOnDeath") && Mode_VGet(st, "Name", name) &&
		LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, name, 0L) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name) &&
		Mode_SaveInt("MultiplusRace.Lives", lives);
	int c = ok ? Mr_LivesAre(lives, "==", 0) : -1;
	if (c > 0) {
		ok = LvN_SetS(st, "OnDamage", "Mission_MultiplusRace.StupidusDamage") && LvN_SetS(st, "TgType", "red");
	} else if (c == 0) {
		ok = LvN_SetS(st, "EntityList", "Enemies");
		int s = ok ? Mr_LivesAre(lives, "==", 1) : -1;
		if (s > 0) {
			ok = LvN_Com("Mission_KillStupidus_RemoteMessage2", "Stupidus", NULL, NULL, NULL);
		} else if (s == 0 && (s = Mr_LivesAre(lives, "==", 2)) > 0) {
			ok = LvN_Com("Mission_KillStupidus_RemoteMessage3", "Stupidus", NULL, NULL, NULL);
		} else if (s == 0 && (s = Mr_LivesAre(lives, "<=", 3)) > 0) {
			s = Mr_LivesAre(lives, "==", 3);
			ok = s >= 0 && (!s || LvN_Com("Mission_KillStupidus_RemoteMessage4", "Stupidus", NULL, NULL, NULL)) &&
				Mode_ListGet("Enemies", "Num", num) && (!LvN_EqI(num, 1) || LV_OWN(MR, "SpawnStupidusFriends", NULL, "i", 2L));
		}
		ok = ok && s >= 0;
	}
	return Mode_Done(&t, ok && c >= 0);
}

/* SpawnStupidusFriends(numFriends): enemy functionaries beside Stupidus */
static byte Plus_SpawnStupidusFriends(const nativeVal_t *a) {
	long count;
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_Range(&a[0], &count);
	for (long i = 0; ok && i < count; i++) {
		nativeVal_t e = { 0 };
		ok = Mr_Functionary(NULL, &e) && LvN_SetS(&e, "OnDeath", "Mission_MultiplusRace.StupidusFriendOnDeath") &&
			LvN_SetS(&e, "EntityList", "Enemies");
		Native_Free(&e);
	}
	return ok;
}

static byte Plus_StupidusFriendOnDeath(const nativeVal_t *a) {
	nativeVal_t one = { 0 };
	Native_SetInt(&one, 1);
	return LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && Mode_Later(5.0, MR, "SpawnStupidusFriends", &one, 1, NULL);
}

/* RestoreEnemies(): the enemies back to the traffic, Ficus ends the mission */
static byte Plus_RestoreEnemies(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = TV(), *next = TV(), *name = TV();
	byte ok = Mode_FirstOf("Enemies", e);
	while (ok && Native_True(e)) {
		Native_Free(next);
		Native_Free(name);
		ok = LvN_SetS(e, "OnDeath", "Vehicles.OnDeath") && Mode_VGet(e, "NextInList", next) && LvN_SetS(e, "EntityList", "Traffic") &&
			LvN_SetS(e, "TgType", "none") && Mode_VGet(e, "Name", name) && LV_CALL("SAI", "SetStateVehicle", NULL, "ivi", 3L, name, 1L);
		Native_Free(e);
		ok = ok && Mode_EntityH(next, e);
	}
	ok = ok && LvN_Com("Mission_KillStupidus_RemoteMessage6", "Ficus", MR, "SetNextMission", LvN_NoArgs());
	return Mode_Done(&t, ok);
}

/* `int(Scrap.GetSaveVar("MultiplusRace.Lives")) + 1`: the lives shown */
static byte Mr_Lives(nativeVal_t *out) {
	nativeVal_t n = { 0 }, n1 = { 0 }, one = { 0 };
	Native_SetNone(&n1);
	Native_SetInt(&one, 1);
	byte ok = Mode_SaveInt("MultiplusRace.Lives", &n) && Native_Arith('+', &n, &one, &n1);
	if (ok) {
		Mode_Repr(&n1, out);
	}
	Native_Free(&n);
	Native_Free(&n1);
	return ok;
}

/* StupidusOnDeath(Looser, Winner): a life fewer and Stupidus again 5 s later, or the end of the gang */
static byte Plus_StupidusOnDeath(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = TV(), *lives = TV(), *rep = TV(), *shown = TV();
	byte ok = LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1]) && Mode_Target(NULL) && Mode_SaveInt("MultiplusRace.Lives", n) &&
		Native_Arith('-', n, Mode_I(&t, 1), lives);
	if (ok && Native_Compare(lives, Mode_I(&t, 0)) < 0) {
		ok = Mode_Later(0.5, MR, "RestoreEnemies", NULL, 0, NULL) && LvN_ModNone("Parking", "SwitchCharMissionCallback") && LvN_ScorerOff() &&
			LvN_Draw();
	} else if (ok) {
		Mode_Repr(lives, rep);
		ok = Mode_Later(5.0, MR, "SpawnStupidus", NULL, 0, NULL) && Mode_SetSaveVar("MultiplusRace.Lives", rep) && Mr_Lives(shown) &&
			LvN_Set0("RemainingLivesScorer", "Text", shown);
	}
	return Mode_Done(&t, ok);
}

/* MissionScorer(id): Stupidus' lives left */
static byte Plus_MissionScorer(const nativeVal_t *id) {
	nativeVal_t y = { 0 }, lives = { 0 };
	Native_SetNone(&y);
	Native_SetNone(&lives);
	byte ok = Mode_Import("Scorer") && Mode_LvLabel(id, "RemainingLives", 0, "Mission_KillStupidus_Scorer_1", 0) && LvN_TextY(20, &y) &&
		Mr_Lives(&lives) && Mode_LvText(id, "RemainingLivesScorer", &y, &lives, NULL);
	Native_Free(&y);
	Native_Free(&lives);
	return ok;
}

/* SetNextMission(): the places of the characters, the next mission of NextMissionData, the crazy gambler 2 s later */
static byte Plus_SetNextMission(void) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Police", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Police", NULL };
	if (!Mode_Import("Init")) {
		return 0;
	}
	Mode_Saves(places);
	return Mode_DelSaveVars("MultiplusRace") && Mode_ApplyNext(MR) && Mode_MMDesc("Mission_TalkToFicus3_MissionText1", 1) &&
		Mode_Later(2, MR, "CGCalls", NULL, 0, NULL);
}

static byte Plus_CGCalls(void) {
	static const char *const keys[] = { "Mission_TalkToFicus3_RemoteMessage4", "Mission_TalkToFicus3_RemoteMessage5",
		"Mission_TalkToFicus3_RemoteMessage6", "Mission_TalkToFicus3_RemoteMessage7" };
	return Mode_CGCalls(keys, 4);
}

LV_CORE0(Plus_InitStatus)
LV_CORE0(Plus_Init)
LV_COREA(Plus_EndOfRace)
LV_CORE0(Plus_SetPhoneCab)
LV_COREA(Plus_UsePhoneCab)
LV_CORE0(Plus_InitRace)
LV_CORE0(Plus_AfterInitRaceTaunt)
LV_CORE0(Plus_InitKill)
LV_CORE0(Plus_SpawnStupidus)
LV_COREA(Plus_SpawnStupidusFriends)
LV_COREA(Plus_StupidusFriendOnDeath)
LV_CORE0(Plus_RestoreEnemies)
LV_COREA(Plus_StupidusOnDeath)
LV_COREA(Plus_MissionScorer)
LV_CORE0(Plus_SetNextMission)
LV_CORE0(Plus_CGCalls)
LV_FN(Plus, InitStatusPy, 1, Plus_InitStatus_C)
LV_FN(Plus, InitPy, 1, Plus_Init_C)
LV_FN(Plus, EndOfRacePy, 2, Plus_EndOfRace_C)
LV_FN(Plus, SetPhoneCabPy, 0, Plus_SetPhoneCab_C)
LV_FN(Plus, UsePhoneCabPy, 4, Plus_UsePhoneCab_C)
LV_FN(Plus, InitRacePy, 0, Plus_InitRace_C)
LV_FN(Plus, AfterInitRaceTauntPy, 0, Plus_AfterInitRaceTaunt_C)
LV_FN(Plus, InitKillPy, 0, Plus_InitKill_C)
LV_FN(Plus, SpawnStupidusPy, 0, Plus_SpawnStupidus_C)
LV_FN(Plus, SpawnStupidusFriendsPy, 1, Plus_SpawnStupidusFriends_C)
LV_FN(Plus, StupidusFriendOnDeathPy, 2, Plus_StupidusFriendOnDeath_C)
LV_FN(Plus, RestoreEnemiesPy, 0, Plus_RestoreEnemies_C)
LV_FN(Plus, StupidusOnDeathPy, 2, Plus_StupidusOnDeath_C)
LV_FN(Plus, MissionScorerPy, 1, Plus_MissionScorer_C)
LV_FN(Plus, SetNextMissionPy, 0, Plus_SetNextMission_C)
LV_FN(Plus, CGCallsPy, 0, Plus_CGCalls_C)

PyMethodDef g_DownTownMultiplusRaceMethods[] = {
	{ "GetFunctionaryProfile", Plus_GetFunctionaryProfile, 1, NULL },
	{ "InitStatus", Plus_InitStatusPy, 1, NULL },
	{ "Init", Plus_InitPy, 1, NULL },
	{ "RaceList", Plus_RaceList, 1, NULL },
	{ "GetDealType", Plus_GetDealType, 1, NULL },
	{ "EndOfRace", Plus_EndOfRacePy, 1, NULL },
	{ "SetPhoneCab", Plus_SetPhoneCabPy, 1, NULL },
	{ "UsePhoneCab", Plus_UsePhoneCabPy, 1, NULL },
	{ "InitRace", Plus_InitRacePy, 1, NULL },
	{ "AfterInitRaceTaunt", Plus_AfterInitRaceTauntPy, 1, NULL },
	{ "InitKill", Plus_InitKillPy, 1, NULL },
	{ "SpawnStupidus", Plus_SpawnStupidusPy, 1, NULL },
	{ "SpawnStupidusFriends", Plus_SpawnStupidusFriendsPy, 1, NULL },
	{ "StupidusFriendOnDeath", Plus_StupidusFriendOnDeathPy, 1, NULL },
	{ "RestoreEnemies", Plus_RestoreEnemiesPy, 1, NULL },
	{ "StupidusOnDeath", Plus_StupidusOnDeathPy, 1, NULL },
	{ "MissionScorer", Plus_MissionScorerPy, 1, NULL },
	{ "SetNextMission", Plus_SetNextMissionPy, 1, NULL },
	{ "CGCalls", Plus_CGCallsPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ProtectMayorTransport: the mayor's transport through the mercenaries, then the mercenaries ---- */

#define KW(fn) (PyCFunction)(void (*)(void))fn, METH_VARARGS | METH_KEYWORDS

static const char *const merc_profile[] = {
	"StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0", "GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=5", "UsePercSonic=80",
	"UsePercCloud=10", "UsePercElectro=10", "UsePercHook=15", "FactorHookMap=6", "CadenceShoot=12", "ControlAim=7", "AngAttack=1.2",
	"CounterAttack=0.75", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.50",
	"FactorDamageItemLife=0.25", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=10", "TimeMaxStrafe=6", "VelMaxStrafe=0.75",
	NULL,
};

/* GetMercEnemiesProfile(TypeShip) */
static byte Escort_GetMercEnemiesProfile_C(const nativeVal_t *a, nativeVal_t *out) {
	return Mode_Import("VehiclesNPC") && LV_CALL("VehiclesNPC", "cShipAI", out, "") && Mode_VSet(out, "Ship", &a[0]) &&
		Mode_SetAttrs(out, merc_profile);
}

/* InitStatus(LevelPath): no hired mercenaries, the traffic of the mission */
static byte Escort_InitStatus(void) {
	return Mode_Import("VehiclesNPC") && LvN_ModSetI("VehiclesNPC", "UseMercs", 0) &&
		Mode_AcShips(PM, "[\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\",\"SBankMaster1\",\"SArchbishop1\"]");
}

/* Init(LevelPath): the taunts of the mercenaries, Rusty; the escort or the hunt by the step of the mission */
static byte Escort_Init(void) {
	if (!LV_OWN(PM, "MercTauntMessager", NULL, "") ||
		!LV_CALL("SScorer", "AddModel", NULL, "isssii", 0L, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3L, 25L)) {
		return 0;
	}
	if (LvN_SaveIs("CrazyMayor", "Transport")) {
		return LV_OWN(PM, "MissionInit", NULL, "");
	}
	return !LvN_SaveIs("CrazyMayor", "KillThem") || LV_OWN(PM, "KillThem", NULL, "");
}

/* Scrap.CreateEntity(name, pos[0], pos[1], pos[2], "Item") with pos a global of the mission, the handle into out */
static byte Pt_Item(const char *name, const char *pos, nativeVal_t *out) {
	nativeVal_t p = { 0 }, n = { 0 };
	LvN_Str(&n, name);
	byte ok = Mode_G(PM, pos, &p) && LvN_CreateAt(&n, &p, "Item", out);
	Native_Free(&p);
	Native_Free(&n);
	return ok;
}

/* MissionInit(): the meeting point where the escort starts, the mayor's call */
static byte Escort_MissionInit(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = TV(), *ang = TV(), *d = TV(), *args = TV(), *who = TV();
	Mode_Ints3(ang, 0, 180, 0);
	Native_SetFloat(d, 4.0);
	LvN_Str(args, "Mission_ProtectMayorTransport_RemoteMessage1");
	LvN_Str(who, "Mayor");
	byte ok = Pt_Item("Winit", "MissionBegin", l) && Mode_VModel(l, Mode_S(&t, "models/Misc/MeetingPoint/MeetingPoint.m3d")) &&
		LvN_SetI(l, "RadarShow", 1) && LvN_SetI(l, "Radius", 3500) && Mode_VSet(l, "Ang", ang) && LvN_SetI(l, "OnlyUser", 1) &&
		LvN_SetS(l, "OnTake", "Mission_ProtectMayorTransport.OnMissionStart") && Mode_LaterSpeech("RemoteComunication", d, args, 2, NULL);
	return Mode_Done(&t, ok);
}

/* OnMissionStart(ItemName, VehicleName): the player at the meeting point (not with the alarm on): the transport and
 * the scene of the start */
static byte Escort_OnMissionStart(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *name = TV(), *itm = TV(), *v = TV(), *zero = TV(), *tr = TV(), *desc = TV(), *pos = TV();
	Mode_Ints3(zero, 0, 0, 0);
	byte ok = Mode_Import("Scene") && Mode_Import("Chars") && Mode_UsrHandle(usr) && Mode_VGet(usr, "Name", name);
	if (!ok || Native_Compare(&a[1], name)) {
		return Mode_Done(&t, ok);
	}
	int alarm = Mode_EntityH(&a[0], itm) && Mode_VGet(itm, "Pos", v) && Mode_VSet(usr, "Pos", v) && Mode_VGet(itm, "Ang", v) &&
		Mode_VSet(usr, "Ang", v) && Mode_VSet(usr, "Vel", zero) && LvN_SetI(itm, "rTime", 15) ? Mode_AlarmOn() : -1;
	if (alarm != 0) {
		return Mode_Done(&t, alarm > 0 &&
			LV_CALL("Speech", "RemoteComunication", NULL, "ss", "Mission_ProtectMayorTransport_RemoteMessage2", "Mayor"));
	}
	ok = Mode_VDel(itm) && LV_CALL("Scene", "InitOutDoorScene", NULL, "") && LV_CALL("Traffic", "SetPath", NULL, "") &&
		LV_CALL("VehiclesNPC", "MakeAI", tr, "ssii", "TLight2", "Transporter", 0L, 0L) && Mode_Put(PM, "trans", NULL, tr) &&
		LV_CALL("Traffic", "ResetPath", NULL, "") && Mode_G(PM, "trans", tr) && Mode_G(PM, "RouteBegin", pos) && Mode_VSet(tr, "Pos", pos) &&
		Mode_VSet(tr, "Ang", zero) && LvN_SetS(tr, "TgType", "green") && LvN_Life(tr, PM, "TRANSPORT_LIFE") &&
		LvN_SetS(tr, "OnDeath", "Mission_ProtectMayorTransport.FailMission") &&
		LvN_SetS(tr, "OnDamage", "Mission_ProtectMayorTransport.ActualizeScorer");
	if (ok) {
		Mode_Lang("Mission_ProtectMayorTransport_Scorer1", desc);
		ok = Mode_VSet(tr, "Descriptor", desc) && Mode_VGet(tr, "Name", name) && Mode_GVarSet("CurrentWaypointName", name) &&
			Mode_G(PM, "RouteEnd", pos) && LV_OWN(PM, "TransGoto", NULL, "v", pos) && LV_OWN(PM, "OnMissionStart2", NULL, "");
	}
	return Mode_Done(&t, ok);
}

/* a shot of the scene: the camera at dist, rot (and ang unless noAng) turning by turn around the global target, the
 * voices stopped, the text of the shot for secs, the function of the next shot */
static byte Pt_Shot(long dist, long rot, long ang, int noAng, long turn, const char *target, const char *text, long secs, const char *next) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cn = TV(), *cam = TV(), *tg = TV(), *name = TV();
	byte ok = LV_CALL("Scrap", "GetCam", cn, "i", 0L) && Mode_EntityH(cn, cam) && LvN_SetI(cam, "CamDist", dist) && LvN_SetI(cam, "CamRot", rot) &&
		(noAng || LvN_SetI(cam, "CamAng", ang)) && LvN_SetI(cam, "AddTurn", turn) && Mode_G(PM, target, tg) && Mode_VGet(tg, "Name", name) &&
		Mode_VSet(cam, "Target", name);
	for (long i = 0; ok && i < 3; i++) {
		ok = LV_CALL("SSound", "StopVoice", NULL, "i", i);
	}
	ok = ok && LV_CALL("Scene", "SetText", NULL, "si", text, secs) && LV_CALL("SScorer", "SetSpeechCallback", NULL, "s", next);
	return Mode_Done(&t, ok);
}

/* OnMissionStart2(): the camera on the transport, the player's death ends the game during the escort */
static byte Escort_OnMissionStart2(void) {
	return Mode_Import("Scene") && Mode_Import("Chars") &&
		Pt_Shot(4000, 0, 0, 0, 90 + 45, "trans", "Mission_Scene_ProtectMayorTransport_1", 8, "Mission_ProtectMayorTransport.AppearsBadGuys") &&
		LvN_ModSetGlobal("Chars", "OnMainCharDeath", PM, "OnPlayerDeathScorting") &&
		Mode_GVarSetS("EscapeEvent", "Mission_ProtectMayorTransport.AbortConversation");
}

/* TransGoto(Pos): the transport flies to Pos */
static byte Escort_TransGoto(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tr = TV(), *name = TV(), *x = TV(), *y = TV(), *z = TV(), *rb = TV();
	byte ok = Mode_G(PM, "trans", tr) && Mode_VGet(tr, "Name", name) && LvN_Item(&a[0], 0, x) && LvN_Item(&a[0], 1, y) && LvN_Item(&a[0], 2, z) &&
		LV_CALL("SAI", "SetStateVehicle", NULL, "ivvvviiddd", 1L, name, x, y, z, 3000L, 0L, 1.0, 2.0, 0.025) && LvN_SetI(tr, "Traf_Active", 1) &&
		Mode_G(PM, "RouteBegin", rb) && Mode_VSet(tr, "Traf_Begin", rb) && Mode_Read(PM, "RouteBegin", rb) && Mode_VSet(tr, "Traf_End", rb) &&
		LvN_SetI(tr, "Traf_Speed", 100 * 28);
	return Mode_Done(&t, ok);
}

/* CreateBadGuy(idnum): a mercenary (Bad1 or Bad2) after the transport, or an enemy of the player without it */
static byte Escort_CreateBadGuy(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *bmt = TV(), *e = TV(), *pd = TV(), *who = TV(), *name = TV(), *ship = TV(), *at = TV(), *ang = TV(), *tr = TV(), *tn = TV();
	byte ok = Mode_Import("Vehicles") && Mode_Import("OutMap") && LvN_GAdd(PM, "BornMercTaunt", 1, NULL) && Mode_G(PM, "BornMercTaunt", bmt);
	ok = ok && (Native_Compare(bmt, Mode_I(&t, 2)) <= 0 || LvN_PutI(PM, "BornMercTaunt", 0));
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	byte first = LvN_EqI(&a[0], 0);
	const char *bad = first ? "Bad1" : "Bad2";
	ok = Mode_SpawnCar(first ? "Bad1_Fighter" : "Bad2_Fighter", 0, e, NULL) && Mode_Put(PM, bad, NULL, e) &&
		LV_OWN(PM, "GetMercEnemiesProfile", pd, "s", first ? "SMerc2" : "SMerc1") && Mode_G(PM, bad, who) && Mode_VGet(who, "Name", name) &&
		Mode_VGet(pd, "Ship", ship) && LV_CALL("VehiclesNPC", "SetStdProperties", NULL, "vv", name, ship) &&
		LV_CALL("VehiclesNPC", "ModAIProff", NULL, "vv", who, pd) && Mode_Read(PM, "BornMercTaunt", bmt);
	if (ok && Native_Compare(bmt, Mode_I(&t, 0)) < 0) {
		if (first) {
			Native_SetVec(at, -383020.28125, -63158.125, 337138.46875);
		} else {
			Native_SetVec(at, -388646.28125, -64070.9414063, 342182.59375);
		}
		ok = Mode_VSet(who, "Pos", at);
	}
	Mode_Ints3(ang, 0, -75, 0);
	ok = ok && Mode_Read(PM, bad, who) && Mode_VSet(who, "Ang", ang) && LvN_SetS(who, "TgType", "red") && LvN_SetI(who, "InCtrl_Acc", 1) &&
		LvN_Life(who, PM, "MERCS_LIFE") && Mode_G(PM, "trans", tr);
	if (ok && Native_True(tr)) {
		ok = LvN_SetS(who, "EntityList", "MadGuys") && Mode_ListSet("MadGuys", "OnDamage", "Mission_ProtectMayorTransport.MadGuyDamage") &&
			Mode_VGet(who, "Name", name) && Mode_Read(PM, "trans", tr) && Mode_VGet(tr, "Name", tn) &&
			LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, name, tn);
	} else if (ok) {
		ok = LvN_SetS(who, "EntityList", "Enemies") && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", who, 1L);
	}
	return Mode_Done(&t, ok);
}

/* SAI.SetRotStaticObj(Bad1.Name, a, b) and the same for Bad2 */
static byte Pt_RotBad(double a, double b) {
	nativeVal_t bad = { 0 }, name = { 0 };
	Native_SetNone(&name);
	byte ok = Mode_G(PM, "Bad1", &bad) && Mode_VGet(&bad, "Name", &name) && LV_CALL("SAI", "SetRotStaticObj", NULL, "vdd", &name, a, b);
	Native_Free(&name);
	ok = ok && Mode_Read(PM, "Bad2", &bad) && Mode_VGet(&bad, "Name", &name) && LV_CALL("SAI", "SetRotStaticObj", NULL, "vdd", &name, a, b);
	Native_Free(&bad);
	Native_Free(&name);
	return ok;
}

/* AppearsBadGuys(): the lists of the mercenaries, the two of them standing still for the scene */
static byte Escort_AppearsBadGuys(void) {
	return Mode_Import("Scene") && LV_CALL("Scrap", "CreateEntityList", NULL, "s", "MadGuys") &&
		Mode_ListSet("MadGuys", "OnDeath", "Mission_ProtectMayorTransport.MadGuyDeath") &&
		Mode_ListSet("Enemies", "OnDeath", "Mission_ProtectMayorTransport.MadGuyDeath") && LV_OWN(PM, "CreateBadGuy", NULL, "i", 0L) &&
		LV_OWN(PM, "CreateBadGuy", NULL, "i", 1L) && Pt_RotBad(3.0, 2.0) && Mode_Later(0.25, PM, "AppearsBadGuys2", NULL, 0, NULL);
}

static byte Escort_AppearsBadGuys2(void) {
	return Mode_Import("Scene") &&
		Pt_Shot(1600, 0, 0, 1, 30, "Bad2", "Mission_Scene_ProtectMayorTransport_2", 4, "Mission_ProtectMayorTransport.NextBadGuy");
}

static byte Escort_NextBadGuy(void) {
	return Mode_Import("Scene") &&
		Pt_Shot(3500, 0, 0, 0, 200, "Bad1", "Mission_Scene_ProtectMayorTransport_3", 5, "Mission_ProtectMayorTransport.BackToTheTransport");
}

static byte Escort_BackToTheTransport(void) {
	return Mode_Import("Scene") &&
		Pt_Shot(3000, +90, -90, 0, 0, "trans", "Mission_Scene_ProtectMayorTransport_4", 7, "Mission_ProtectMayorTransport.BackToPlayer");
}

/* BackToPlayer(): the end of the scene: the scorer, the mercenaries turn fast again, the transport to its target */
static byte Escort_BackToPlayer(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rt = TV(), *l = TV(), *tr = TV();
	byte ok = Mode_Import("Scene");
	for (long i = 0; ok && i < 3; i++) {
		ok = LV_CALL("SSound", "StopVoice", NULL, "i", i);
	}
	ok = ok && LvN_ScorerCallG(PM, "MissionScorer") && LV_CALL("Scene", "EndOutDoorScene", NULL, "") && Mode_GVarSetS("EscapeEvent", "Menu.Init") &&
		Pt_RotBad(20.0, 20.0) && Mode_G(PM, "RouteTarget", rt) && LV_OWN(PM, "TransGoto", NULL, "v", rt) && Pt_Item("Wend", "RouteTarget", l) &&
		LvN_SetI(l, "Radius", 4000) && LvN_SetS(l, "OnTake", "Mission_ProtectMayorTransport.OnTravelComplete") && Mode_G(PM, "trans", tr) &&
		LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", tr, 1L) &&
		LV_CALL("Speech", "RemoteComunication", NULL, "ss", "Mission_ProtectMayorTransport_RemoteMessage3", "Mayor");
	return Mode_Done(&t, ok);
}

/* Scrap.SetSaveVar(name, `v`) */
static byte Dt_SaveRepr(const char *name, const nativeVal_t *v) {
	LvN_SaveRepr(name, v);
	return 1;
}

/* MadGuyDeath(MainCharnName, Param2): without the transport a mercenary more counts; a dead mercenary comes back */
static byte Escort_MadGuyDeath(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tr = TV(), *n = TV(), *max = TV(), *cnt = TV(), *m1 = TV(), *n1 = TV(), *bad1 = TV(), *name = TV(), *arg = TV();
	byte ok = Mode_Import("Vehicles") && Mode_G(PM, "trans", tr);
	if (ok && tr->type == NV_NONE) {
		ok = LvN_GAdd(PM, "NumTargets", 1, NULL) && Mode_G(PM, "NumTargets", n) && Dt_SaveRepr("CrazyMayor.KillThem", n) &&
			Mode_CountText(PM, "NumTargets", "MaxTargets", cnt) && LvN_Set0("RemainingCharsScorer", "Text", cnt) && Mode_Read(PM, "NumTargets", n) &&
			Mode_G(PM, "MaxTargets", max);
		if (ok && Native_Compare(n, max) >= 0) {
			ok = LvN_ScorerOff() && LvN_Draw() && Mode_Read(PM, "MaxTargets", max) && Native_Arith('-', max, Mode_I(&t, 1), m1) &&
				Dt_SaveRepr("CrazyMayor.KillThem", m1) && LvN_Com("Mission_KillMercs_RemoteMessage1", "Mayor", PM, "PreLaunchTraitorius", NULL);
		}
		ok = ok && Mode_Read(PM, "NumTargets", n) && Native_Arith('+', n, Mode_I(&t, 1), n1) && Mode_Read(PM, "MaxTargets", max);
		if (!ok || Native_Compare(n1, max) >= 0) {
			return Mode_Done(&t, ok);
		}
	}
	int one = ok && Mode_G(PM, "Bad1", bad1) ? (Native_True(bad1) ? (Mode_VGet(bad1, "Name", name) ? !Native_Compare(&a[0], name) : -1) : 0) : -1;
	ok = one >= 0 && Mode_Read(PM, "trans", tr);
	if (ok) {
		byte tv = Native_True(tr);
		Native_SetInt(arg, one ? 0 : 1);
		ok = Mode_Later(one ? (tv ? 10 : 5) : (tv ? 15 : 10), PM, "CreateBadGuy", arg, 1, NULL) && (Native_SetNone(n), Mode_Put(PM, one ? "Bad1" : "Bad2", NULL, n));
	}
	return Mode_Done(&t, ok);
}

static byte Escort_PreLaunchTraitorius(void) {
	return Mode_Later(3, PM, "TraitoriusCom", NULL, 0, NULL);
}

/* SetNextMission(): on to Mission_Ambush, the blueprints of Humphrey's ship, Rusty 10 s later */
static byte Escort_SetNextMission(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lst = TV();
	byte ok = Mode_Import("Menu") && Mode_DelSaveVars("CrazyMayor") && Mode_DelSaveVars("TransportState") && Mode_WayPoint() &&
		LvN_Next("Mission_Ambush", "Scrapyard", "Mission_Ambush1_MissionText1") && Mode_SaveS("MayorAtTownHall", "1") &&
		LvN_SaveList("PressBlueprints.Ships", PM, lst) && LvN_SaveAppend("PressBlueprints.Ships", lst, Mode_S(&t, "SHump1")) &&
		(Native_Free(lst), 1) && LvN_SaveList("PressBlueprints.Engines", PM, lst) &&
		LvN_SaveAppend("PressBlueprints.Engines", lst, Mode_S(&t, "MHump1")) && Mode_SaveS("MayorAtGambinDen", "1") &&
		Dt_RustyLater("Mission_ProtectMayorTransport_RemoteMessage6");
	return Mode_Done(&t, ok);
}

/* MadGuyDamage(Name, LUser, DamageNum): the mercenary the player hits turns on him */
static byte Escort_MadGuyDamage(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *bad = TV(), *name = TV(), *un = TV();
	byte ok = Mode_ListSet("MadGuys", "OnDamage", "") && Mode_UsrHandle(usr);
	if (ok && Native_True(usr)) {
		int one = Mode_G(PM, "Bad1", bad) ? (Native_True(bad) ? (Mode_VGet(bad, "Name", name) ? !Native_Compare(name, &a[0]) : -1) : 0) : -1;
		int two = one == 0 ? (Mode_Read(PM, "Bad2", bad) ? Native_True(bad) : -1) : 0;
		ok = one >= 0 && two >= 0;
		if (ok && (one || two)) {
			ok = LvN_SetS(bad, "EntityList", "Enemies") && Mode_VGet(bad, "Name", name) && Mode_VGet(usr, "Name", un) &&
				LV_CALL("SAI", "SetStateVehicle", NULL, "ivv", 2L, name, un);
		}
	}
	return Mode_Done(&t, ok);
}

/* MercTauntMessager(): again 10 to 20 s later while the mission lasts */
static byte Escort_MercTauntMessager(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *now = TV(), *r = TV(), *at = TV();
	byte ok = Mode_Import("Vehicles");
	if (ok && (LvN_SaveIs("CrazyMayor", "Transport") || LvN_SaveIs("CrazyMayor", "KillThem"))) {
		Native_SetFloat(now, Mode_Time());
		ok = Mode_Rand(Mode_I(&t, 10), Mode_I(&t, 20), r) && Native_Arith('+', now, r, at) && Mode_AtG(at, PM, "MercTauntMessager", NULL, 0);
	}
	return Mode_Done(&t, ok);
}

/* OnTravelComplete(ItemName, VehicleName): the transport at its target: it lands, the reward, the hunt of the
 * mercenaries */
static byte Escort_OnTravelComplete(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tr = TV(), *name = TV(), *itm = TV(), *v = TV(), *money = TV(), *rw = TV(), *sum = TV();
	byte ok = Mode_G(PM, "trans", tr);
	if (ok && tr->type == NV_NONE) {
		ok = Mode_EntityH(&a[0], itm) && LvN_SetI(itm, "rTime", 15) && Mode_VDel(itm);
	} else if (ok && Mode_VGet(tr, "Name", name) && !Native_Compare(&a[1], name)) {
		ok = Mode_EntityH(&a[0], itm) && LvN_SetI(itm, "rTime", 15) && Mode_VGet(itm, "Pos", v) && Mode_VSet(tr, "MovePos", v) &&
			Mode_VGet(itm, "Ang", v) && Mode_VSet(tr, "MoveAng", v) && LvN_SetF(tr, "MoveTime", 2.5) && Mode_VGet(tr, "MaxLife", v) &&
			Mode_VSet(tr, "Life", v) && LvN_SetS(tr, "OnDeath", "Vehicles.OnDeath") && LvN_SetS(tr, "OnDamage", "") &&
			LvN_SetS(tr, "TgType", "None") && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", tr, 0L) &&
			Mode_GVarSetS("CurrentWaypointName", "") && Mode_VGet(tr, "Name", name) && Mode_Later(30, PM, "RestoreTransport", name, 1, NULL) &&
			Mode_VDel(itm) && (Native_SetInt(money, Mode_GetMoney()), 1) && Mode_G(PM, "ScortReward", rw) && Native_Arith('+', money, rw, sum) &&
			Mode_SetMoney(sum) && Mode_SaveS("TransportState", "Saved") &&
			LvN_Com("Mission_ProtectMayorTransport_RemoteMessage5", "Mayor", PM, "InitKillBadGuys", NULL);
	}
	return Mode_Done(&t, ok);
}

/* ActualizeScorer(Name, LUser, DamageNum): the life of the transport on its bar */
static byte Escort_ActualizeScorer(void) {
	nativeVal_t tr = { 0 }, v = { 0 };
	Native_SetNone(&v);
	byte ok = Mode_G(PM, "trans", &tr) && Mode_VGet(&tr, "MaxLife", &v) && LvN_Set0("RemainingLifeScorer", "MaxValue", &v);
	Native_Free(&v);
	ok = ok && Mode_Read(PM, "trans", &tr) && Mode_VGet(&tr, "Life", &v) && LvN_Set0("RemainingLifeScorer", "Value", &v);
	Native_Free(&tr);
	Native_Free(&v);
	return ok;
}

/* MissionScorer(id): the life of the transport, or the count of the mercenaries */
static byte Escort_MissionScorer(const nativeVal_t *id) {
	static const char bar[] = "RemainingLifeScorer";
	modeVals_t t = { .n = 0 };
	nativeVal_t *tr = TV(), *v = TV(), *y = TV(), *cnt = TV();
	byte ok = Mode_Import("Scorer") && Mode_G(PM, "trans", tr);
	if (ok && Native_True(tr)) {
		ok = Mode_LvLabel(id, "RemainingLifeText", 0, "Mission_ProtectMayorTransport_Scorer1", 1) && Mode_ScorerAddS(id, bar, "Bar") &&
			Mode_BarDefaults(id, bar, NULL, NULL) && Mode_Read(PM, "trans", tr) && Mode_VGet(tr, "MaxLife", v) && LvN_ScorerSet(id, bar, "MaxValue", v) &&
			Mode_Read(PM, "trans", tr) && Mode_VGet(tr, "Life", v) && LvN_ScorerSet(id, bar, "Value", v) && Mode_ScorerSetI(id, bar, "Visible", 1);
	} else if (ok) {
		ok = Mode_LvLabel(id, "RemainingCharsText", 0, "Mission_KillMercs_Scorer1", 1) && LvN_TextY(26, y) &&
			Mode_CountText(PM, "NumTargets", "MaxTargets", cnt) && Mode_LvText(id, "RemainingCharsScorer", y, cnt, Mode_I(&t, 1));
	}
	return Mode_Done(&t, ok);
}

/* KillThem(): the mayor calls for the hunt of the mercenaries */
static byte Escort_KillThem(void) {
	return LvN_Com(LvN_SaveIs("TransportState", "Killed") ? "Mission_KillMercs_RemoteMessage3" : "Mission_KillMercs_RemoteMessage4", "Mayor", PM,
		"InitKillThemMission", NULL);
}

/* InitKillThemMission(id=None): the mercenaries (the second only while more than one is left), the hunt */
static byte Escort_InitKillThemMission(void) {
	nativeVal_t n = { 0 }, n1 = { 0 }, one = { 0 };
	Native_SetNone(&n1);
	Native_SetInt(&one, 1);
	byte ok = LV_OWN(PM, "CreateBadGuy", NULL, "i", 0L) && Mode_SaveInt("CrazyMayor.KillThem", &n) && Native_Arith('+', &n, &one, &n1);
	int c = ok ? Mode_CmpG(&n1, "<", PM, "MaxTargets") : -1;
	ok = c >= 0 && (!c || LV_OWN(PM, "CreateBadGuy", NULL, "i", 1L)) && LV_OWN(PM, "InitKillBadGuys", NULL, "");
	Native_Free(&n);
	Native_Free(&n1);
	return ok;
}

/* InitKillBadGuys(itm=None): the hunt of the mercenaries: no transport, the count from the save variable */
static byte Escort_InitKillBadGuys(void) {
	static const char *const bads[] = { "Bad1", "Bad2" };
	nativeVal_t n = { 0 }, bad = { 0 };
	Native_SetNone(&bad);
	byte ok = Mode_Import("Chars") && LvN_ModNone("Chars", "OnMainCharDeath") && Mode_SaveS("CrazyMayor", "KillThem") &&
		Mode_MMDesc("Mission_KillMercs_MissionText1", 1) && Mode_WayPoint() && Mode_SaveInt("CrazyMayor.KillThem", &n) && Mode_Put(PM, "NumTargets", NULL, &n) &&
		(Native_Free(&n), Mode_Put(PM, "trans", NULL, &n)) && LvN_ScorerCallG(PM, "MissionScorer") && LvN_Draw() &&
		Mode_ListSet("Enemies", "OnDeath", "Mission_ProtectMayorTransport.MadGuyDeath");
	for (int i = 0; ok && i < 2; i++) {
		ok = Mode_Read(PM, bads[i], &bad) &&
			(!Native_True(&bad) || (LvN_SetS(&bad, "EntityList", "Enemies") && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &bad, 1L)));
	}
	Native_Free(&n);
	Native_Free(&bad);
	return ok;
}

static byte Escort_RestoreTransport(const nativeVal_t *a) {
	nativeVal_t v = { 0 };
	Native_SetNone(&v);
	byte ok = Mode_EntityH(&a[0], &v) && (!Native_True(&v) || LvN_SetI(&v, "MoveTime", 0));
	Native_Free(&v);
	return ok;
}

/* the keyword adapters (InitKillThemMission(id=None), InitKillBadGuys(itm=None)) */
static PyObject *Escort_InitKillThemMissionPy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = Py_None;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id)) {
		return NULL;
	}
	return Mode_Result(Escort_InitKillThemMission());
}

static PyObject *Escort_InitKillBadGuysPy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "itm", NULL };
	PyObject *itm = Py_None;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &itm)) {
		return NULL;
	}
	return Mode_Result(Escort_InitKillBadGuys());
}

LV_CORE0(Escort_InitStatus)
LV_CORE0(Escort_Init)
LV_CORE0(Escort_MissionInit)
LV_COREA(Escort_OnMissionStart)
LV_CORE0(Escort_OnMissionStart2)
LV_COREA(Escort_TransGoto)
LV_COREA(Escort_CreateBadGuy)
LV_CORE0(Escort_AppearsBadGuys)
LV_CORE0(Escort_AppearsBadGuys2)
LV_CORE0(Escort_NextBadGuy)
LV_CORE0(Escort_BackToTheTransport)
LV_CORE0(Escort_BackToPlayer)
LV_COREA(Escort_MadGuyDeath)
LV_CORE0(Escort_PreLaunchTraitorius)
LV_CORE0(Escort_SetNextMission)
LV_COREA(Escort_MadGuyDamage)
LV_CORE0(Escort_MercTauntMessager)
LV_COREA(Escort_OnTravelComplete)
LV_CORE0(Escort_ActualizeScorer)
LV_COREA(Escort_MissionScorer)
LV_CORE0(Escort_KillThem)
LV_COREA(Escort_RestoreTransport)
LV_FN(Escort, GetMercEnemiesProfilePy, 1, Escort_GetMercEnemiesProfile_C)
LV_FN(Escort, InitStatusPy, 1, Escort_InitStatus_C)
LV_FN(Escort, InitPy, 1, Escort_Init_C)
LV_FN(Escort, MissionInitPy, 0, Escort_MissionInit_C)
LV_FN(Escort, OnMissionStartPy, 2, Escort_OnMissionStart_C)
LV_FN(Escort, OnMissionStart2Py, 0, Escort_OnMissionStart2_C)
LV_FN(Escort, TransGotoPy, 1, Escort_TransGoto_C)
LV_FN(Escort, CreateBadGuyPy, 1, Escort_CreateBadGuy_C)
LV_FN(Escort, AppearsBadGuysPy, 0, Escort_AppearsBadGuys_C)
LV_FN(Escort, AppearsBadGuys2Py, 0, Escort_AppearsBadGuys2_C)
LV_FN(Escort, NextBadGuyPy, 0, Escort_NextBadGuy_C)
LV_FN(Escort, BackToTheTransportPy, 0, Escort_BackToTheTransport_C)
LV_FN(Escort, BackToPlayerPy, 0, Escort_BackToPlayer_C)
LV_FN(Escort, MadGuyDeathPy, 2, Escort_MadGuyDeath_C)
LV_FN(Escort, PreLaunchTraitoriusPy, 0, Escort_PreLaunchTraitorius_C)
LV_FN(Escort, SetNextMissionPy, 0, Escort_SetNextMission_C)
LV_FN(Escort, MadGuyDamagePy, 3, Escort_MadGuyDamage_C)
LV_FN(Escort, MercTauntMessagerPy, 0, Escort_MercTauntMessager_C)
LV_FN(Escort, OnTravelCompletePy, 2, Escort_OnTravelComplete_C)
LV_FN(Escort, ActualizeScorerPy, 3, Escort_ActualizeScorer_C)
LV_FN(Escort, MissionScorerPy, 1, Escort_MissionScorer_C)
LV_FN(Escort, KillThemPy, 0, Escort_KillThem_C)
LV_FN(Escort, RestoreTransportPy, 1, Escort_RestoreTransport_C)

PyMethodDef g_DownTownProtectMayorTransportMethods[] = {
	{ "GetMercEnemiesProfile", Escort_GetMercEnemiesProfilePy, 1, NULL },
	{ "InitStatus", Escort_InitStatusPy, 1, NULL },
	{ "Init", Escort_InitPy, 1, NULL },
	{ "MissionInit", Escort_MissionInitPy, 1, NULL },
	{ "OnMissionStart", Escort_OnMissionStartPy, 1, NULL },
	{ "OnMissionStart2", Escort_OnMissionStart2Py, 1, NULL },
	{ "TransGoto", Escort_TransGotoPy, 1, NULL },
	{ "CreateBadGuy", Escort_CreateBadGuyPy, 1, NULL },
	{ "AppearsBadGuys", Escort_AppearsBadGuysPy, 1, NULL },
	{ "AppearsBadGuys2", Escort_AppearsBadGuys2Py, 1, NULL },
	{ "NextBadGuy", Escort_NextBadGuyPy, 1, NULL },
	{ "BackToTheTransport", Escort_BackToTheTransportPy, 1, NULL },
	{ "BackToPlayer", Escort_BackToPlayerPy, 1, NULL },
	{ "MadGuyDeath", Escort_MadGuyDeathPy, 1, NULL },
	{ "PreLaunchTraitorius", Escort_PreLaunchTraitoriusPy, 1, NULL },
	{ "SetNextMission", Escort_SetNextMissionPy, 1, NULL },
	{ "MadGuyDamage", Escort_MadGuyDamagePy, 1, NULL },
	{ "MercTauntMessager", Escort_MercTauntMessagerPy, 1, NULL },
	{ "OnTravelComplete", Escort_OnTravelCompletePy, 1, NULL },
	{ "ActualizeScorer", Escort_ActualizeScorerPy, 1, NULL },
	{ "MissionScorer", Escort_MissionScorerPy, 1, NULL },
	{ "KillThem", Escort_KillThemPy, 1, NULL },
	{ "InitKillThemMission", KW(Escort_InitKillThemMissionPy), NULL },
	{ "InitKillBadGuys", KW(Escort_InitKillBadGuysPy), NULL },
	{ "RestoreTransport", Escort_RestoreTransportPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_StealMayorShip: in the mayor's ship the player destroys twenty police ships, then parks it ---- */

static PyObject *Steal_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, SM, "[\"SMayor1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* `numCopsKilled < COPS_TO_KILL`: 1, 0, -1 */
static int Sm_Hunting(void) {
	nativeVal_t n = { 0 };
	int r = Mode_G(SM, "numCopsKilled", &n) ? Mode_CmpG(&n, "<", SM, "COPS_TO_KILL") : -1;
	Native_Free(&n);
	return r;
}

/* Init(LevelPath): the hunt of the police or the parking by the count, the mayor's ship half a second later and
 * every 5 s */
static byte Steal_Init(void) {
	nativeVal_t n = { 0 };
	byte ok = Mode_Import("Parking") && Mode_Import("Vehicles") && LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", SM, "StartMission") &&
		Mode_SaveInt("NumCopsKilled", &n) && Mode_Put(SM, "numCopsKilled", NULL, &n);
	Native_Free(&n);
	int hunt = ok ? Sm_Hunting() : -1;
	if (hunt > 0) {
		ok = LvN_ModSetGlobal("Vehicles", "OnDeathCallback", SM, "OnPoliceDeath") &&
			LvN_Com("Mission_TestMayorShip_RemoteMessage1", "Trurl", NULL, NULL, NULL);
	} else if (hunt == 0) {
		ok = LvN_Com("Mission_ParkMayorShip_RemoteMessage2", "Trurl", NULL, NULL, NULL);
	}
	return ok && hunt >= 0 && Mode_Later(0.5, SM, "CreateMissionShip", NULL, 0, NULL) &&
		Mode_Later(5.0, SM, "TestForMissionShip", NULL, 0, "TestForMissionShip");
}

/* `string.find(string.upper(v.Model), text) != -1`: 1, 0, -1 */
static int Sm_ModelHas(const nativeVal_t *v, const char *text) {
	nativeVal_t m = { 0 }, up = { 0 }, at = { 0 };
	int r = Mode_VGet(v, "Model", &m) && Native_Upper(&m, &up) && LV_CALL("string", "find", &at, "vs", &up, text) ? !LvN_EqI(&at, -1) : -1;
	Native_Free(&m);
	Native_Free(&up);
	Native_Free(&at);
	return r;
}

/* CreateMissionShip(): the player's mayor ship (not his own) is the mission ship, else a parked mayor ship instead of
 * the farthest parked one */
static byte Steal_CreateMissionShip(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *ps = TV(), *name = TV(), *pn = TV(), *fp = TV(), *ep = TV(), *ep1 = TV(), *e = TV(), *ship = TV();
	(void)ep1;
	byte ok = Mode_Import("Parking") && Mode_UsrHandle(usr);
	int c = ok ? Mode_UsrInCar(usr) : -1;
	if (c > 0) {
		c = LvN_ModGet("Parking", "PlayerShip", ps) ? Native_True(ps) : -1;
	}
	if (c > 0) {
		int mayor = Sm_ModelHas(usr, "MAYOR");
		if (mayor > 0) {
			Native_Free(ps);
			mayor = Mode_VGet(usr, "Name", name) && LvN_ModGet("Parking", "PlayerShip", ps) && Mode_VGet(ps, "Name", pn) ? Native_Compare(name, pn) != 0 :
				-1;
		}
		if (mayor > 0) {
			int hunt = Mode_VGet(usr, "Name", name) && Mode_Put(SM, "missionShipName", NULL, name) ? Sm_Hunting() : -1;
			ok = hunt >= 0 && (hunt ? LV_OWN(SM, "SetPoliceTargets", NULL, "") : LV_OWN(SM, "SetPlatformTarget", NULL, ""));
			return Mode_Done(&t, ok);
		}
		c = mayor;
	}
	ok = c >= 0 && LV_CALL("Scrap", "GetFarestParked", fp, "") && Mode_Unpack(fp, 2, ep);
	if (ok && !LvN_EqS(&ep[0], "")) {
		ok = Mode_EntityH(&ep[0], e) && (!Native_True(e) || Mode_VDel(e));
	}
	ok = ok && LV_CALL("Parking", "CreateParkedShip", ship, "vis", &ep[1], 0L, "Mayor") && Mode_VGet(ship, "Name", name) &&
		Mode_Put(SM, "missionShipName", NULL, name) && Mode_G(SM, "missionShipName", name) && LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "v", name);
	return Mode_Done(&t, ok);
}

/* TestForMissionShip(): the mayor's ship again when it is gone, every 5 s */
static byte Steal_TestForMissionShip(void) {
	nativeVal_t name = { 0 }, e = { 0 };
	byte ok = Mode_G(SM, "missionShipName", &name) && Mode_EntityH(&name, &e) &&
		(Native_True(&e) || (LV_OWN(SM, "CreateMissionShip", NULL, "") && LvN_Com("Mission_ParkMayorShip_RemoteMessage2", "Trurl", NULL, NULL, NULL))) &&
		Mode_Later(5.0, SM, "TestForMissionShip", NULL, 0, "TestForMissionShip");
	Native_Free(&name);
	Native_Free(&e);
	return ok;
}

/* SetPlatformTarget(): the phone cab is the platform where the ship is parked */
static byte Steal_SetPlatformTarget(void) {
	return Mode_Import("PhoneCab") && Mode_Import("Parking") && Mode_PhoneCab("Mission_StealMayorShip.UsePhoneCab", 0, SM, NULL) &&
		LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", SM, "TestForCorrectPlatform");
}

/* TestForCorrectPlatform(): the player leaves the ship: at the platform (70 m across) the ship stays, else Trurl
 * complains */
static byte Steal_TestForCorrectPlatform(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *pc = TV(), *up = TV(), *pp = TV(), *sub = TV(), *d = TV(), *name = TV(), *e = TV();
	int c = Mode_UsrHandle(usr) ? Native_True(usr) : -1;
	if (c > 0) {
		c = Mode_G(SM, "pc", pc) ? Native_True(pc) : -1;
	}
	byte ok = c >= 0;
	if (c > 0) {
		ok = Mode_VGet(usr, "Pos", up) && Mode_Read(SM, "pc", pc) && Mode_VGet(pc, "Pos", pp) && Mode_VecSub(up, pp, sub) && Mode_VecModSqr(sub, d);
		if (ok && Native_Compare(d, Mode_I(&t, 49000000)) > 0) {
			ok = LvN_Com("Mission_ParkMayorShip_RemoteMessage1", "Trurl", NULL, NULL, NULL);
		} else if (ok && Mode_G(SM, "missionShipName", name) && Mode_EntityH(name, e)) {
			ok = !Native_True(e) || (Mode_Import("Parking") && Mode_DelSchedS("TestForMissionShip") &&
				LvN_SetS(e, "UseFunc", "Mission_StealMayorShip.UseParkedMayorShip") && LvN_ModNone("Parking", "SwitchCharMissionCallback") &&
				LvN_Com("Mission_ParkMayorShip_RemoteMessage3", "Trurl", NULL, NULL, NULL) && Mode_WayPoint() &&
				Mode_MMDesc("Mission_ByeByeTrurl_MissionText1", 1));
		} else {
			ok = 0;
		}
	}
	return Mode_Done(&t, ok);
}

/* UseParkedMayorShip(UsrName, CharName, optId, FullOption): the parked ship is not to be used any more */
static byte Steal_UseParkedMayorShip(void) {
	nativeVal_t text = { 0 };
	if (!Mode_Import("RaceMaker")) {
		return 0;
	}
	Mode_Lang("Mission_ParkMayorShip_SystemMessage1", &text);
	byte ok = LV_CALL("RaceMaker", "ShowSystemMessage", NULL, "v", &text);
	Native_Free(&text);
	return ok;
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): Trurl's farewell and DeepThroat at the phone of the platform */
static byte Steal_UsePhoneCab(const nativeVal_t *a) {
	nativeVal_t pc = { 0 }, key = { 0 };
	int alarm = Mode_Import("PhoneCab") && Mode_SetModVar("Speech", "NPCName", &a[1]) && Mode_SetModVar("Speech", "PlayerName", &a[0]) &&
		LV_CALL("PhoneCab", "UseAudioCom", NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]) ? Mode_AlarmOn() : -1;
	byte ok = alarm >= 0;
	if (alarm > 0) {
		ok = Mode_G(SM, "pc", &pc) && LV_CALL("Speech", "StartObjSpch", NULL, "vss", &pc, "Generics_StillPolice1_Messenger", "Trurl");
	} else if (alarm == 0) {
		LvN_Str(&key, "Mission_ByeByeTrurl_ComDeepThroat_NI_1Msg");
		ok = LvN_OnEnd(SM, "SetNextMission", NULL) && LV_CALL("Speech", "NISpeech", NULL, "s", "Messenger") &&
			LV_CALL("Speech", "AutoAddNI", NULL, "s", "Mission_ByeByeTrurl_ComTrurl_NI_1Msg") &&
			Mode_CallKw("Speech", "AutoAddNI", NULL, &key, 1, Py_BuildValue("{ss}", "HeadName", "DeepThroat")) && LV_CALL("Speech", "RunNI", NULL, "");
	}
	Native_Free(&pc);
	Native_Free(&key);
	return ok;
}

/* SetNextMission(): on to Mission_ThreeStooges, the places of the characters, the crazy gambler 2 s later */
static byte Steal_SetNextMission(void) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "PoliceBossAtTownHall", "0", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "GamblinDen", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "GamblinDen", NULL };
	byte ok = Mode_GSetS(SM, "pc", "UseFunc", "PhoneCab.DefUseAudioCom") && LvN_ModNone("Speech", "OnOutOfRange") && Mode_Target("") &&
		Mode_DelSaveVars("NumCopsKilled") && Mode_WayPoint() && LvN_Next("Mission_ThreeStooges", "GamblinDen", "Mission_FirstContactMercs_MissionText1");
	if (ok) {
		Mode_Saves(places);
	}
	return ok && Mode_Later(2, SM, "CGCalls", NULL, 0, NULL);
}

static byte Steal_CGCalls(void) {
	static const char *const keys[] = { "Mission_FirstContactMercs_RemoteMessage1", "Mission_FirstContactMercs_RemoteMessage2",
		"Mission_FirstContactMercs_RemoteMessage3" };
	return Mode_CGCalls(keys, 3);
}

/* `usr and usr.Name != missionShipName` with usr = Scrap.UsrEntity(0): the player is not in the mayor's ship (1, 0,
 * -1) */
static int Sm_NotInShip(void) {
	nativeVal_t usr = { 0 }, name = { 0 }, ms = { 0 };
	int r = Mode_UsrHandle(&usr) ? (Native_True(&usr) ? (Mode_VGet(&usr, "Name", &name) && Mode_G(SM, "missionShipName", &ms) ?
		Native_Compare(&name, &ms) != 0 : -1) : 0) : -1;
	Native_Free(&usr);
	Native_Free(&name);
	Native_Free(&ms);
	return r;
}

/* StartMission(): the player in the mayor's ship: the hunt of the police or the platform */
static byte Steal_StartMission(void) {
	static const char *const mods[] = { "Menu", "Parking", "OutPolice", "Speech", NULL };
	nativeVal_t name = { 0 }, e = { 0 };
	byte ok = 1;
	for (int i = 0; ok && mods[i]; i++) {
		ok = Mode_Import(mods[i]);
	}
	int c = ok ? Sm_NotInShip() : -1;
	if (c != 0) {
		return c > 0;
	}
	ok = Mode_G(SM, "missionShipName", &name) && Mode_EntityH(&name, &e);
	if (ok && Native_True(&e)) {
		int hunt = LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, 0L) ? Sm_Hunting() : -1;
		ok = hunt >= 0 && (hunt ? LvN_Com("Mission_TestMayorShip_RemoteMessage5", "Trurl", NULL, NULL, NULL) && LV_OWN(SM, "SetPoliceTargets", NULL, "") :
			LV_OWN(SM, "SetPlatformTarget", NULL, ""));
	}
	Native_Free(&name);
	Native_Free(&e);
	return ok;
}

/* ContinueMission(): back in a ship: the mission goes on in the mayor's, else Trurl complains */
static byte Steal_ContinueMission(void) {
	int c = Sm_NotInShip();
	if (c > 0) {
		return Mode_Import("Menu") && LvN_ScorerOff() && LvN_Draw() && LvN_Com("Mission_TestMayorShip_RemoteMessage3", "Trurl", NULL, NULL, NULL);
	}
	return c == 0 && LV_OWN(SM, "StartMission", NULL, "");
}

/* SetPoliceListTargetStatus(status): the police ships of the list OutCop as targets or not */
static byte Steal_SetPoliceListTargetStatus(const nativeVal_t *a) {
	nativeVal_t name = { 0 }, e = { 0 };
	byte ok = Mode_ListGet("OutCop", "First", &name);
	while (ok && Native_True(&name)) {
		Native_Free(&e);
		ok = Mode_EntityH(&name, &e);
		if (!ok || !Native_True(&e)) {
			break;
		}
		Native_Free(&name);
		ok = LV_OWN(SM, "SetPoliceShipTargetStatus", NULL, "vv", &e, &a[0]) && Mode_VGet(&e, "NextInList", &name);
	}
	Native_Free(&name);
	Native_Free(&e);
	return ok;
}

/* ComeBack(): the player left the mayor's ship: the ship is the target again, not the police */
static byte Steal_ComeBack(void) {
	nativeVal_t name = { 0 }, e = { 0 };
	byte ok = Mode_Import("Parking") && Mode_Import("Menu") && Mode_Import("OutPolice") && Mode_G(SM, "missionShipName", &name) &&
		Mode_EntityH(&name, &e) && (!Native_True(&e) || LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, 1L)) &&
		LvN_ModNone("OutPolice", "OnPoliceCreatedCallback") && LV_OWN(SM, "SetPoliceListTargetStatus", NULL, "i", 0L) &&
		LvN_Com("Mission_TestMayorShip_RemoteMessage2", "Trurl", NULL, NULL, NULL) && LvN_ModNone("Parking", "SwitchCharMissionCallback") &&
		LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", SM, "ContinueMission");
	Native_Free(&name);
	Native_Free(&e);
	return ok;
}

/* MissionScorer(id): the police ships destroyed */
static byte Steal_MissionScorer(const nativeVal_t *id) {
	nativeVal_t y = { 0 }, cnt = { 0 };
	byte ok = Mode_Import("Scorer") && Mode_LvLabel(id, "RemainingCharsText", 0, "Mission_TestMayorShip_Scorer_1", 0) && LvN_TextY(20, &y) &&
		Mode_CountText(SM, "numCopsKilled", "COPS_TO_KILL", &cnt) && Mode_LvText(id, "RemainingCharsScorer", &y, &cnt, NULL);
	Native_Free(&y);
	Native_Free(&cnt);
	return ok;
}

/* OnPoliceDeath(LooserName, WinnerName): a police ship the player destroyed in the mayor's ship counts; at twenty the
 * ship is to be parked */
static byte Steal_OnPoliceDeath(const nativeVal_t *a) {
	static const char *const mods[] = { "Menu", "Parking", "Vehicles", "OutPolice", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = TV(), *name = TV(), *v = TV(), *ms = TV(), *n = TV(), *cnt = TV();
	int c = Mode_UsrHandle(usr) ? (Native_True(usr) ? (Mode_VGet(usr, "Name", name) ? !Native_Compare(name, &a[1]) : -1) : 0) : -1;
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	c = Mode_EntityH(&a[0], v) ? Sm_ModelHas(v, "POLI") : -1;
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	if (!Mode_G(SM, "missionShipName", ms)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(name, ms)) {
		return Mode_Done(&t, LvN_Com("Mission_TestMayorShip_RemoteMessage3", "Trurl", NULL, NULL, NULL));
	}
	byte ok = LvN_GAdd(SM, "numCopsKilled", 1, NULL) && Mode_G(SM, "numCopsKilled", n);
	int done = ok ? Mode_CmpG(n, "==", SM, "COPS_TO_KILL") : -1;
	if (done > 0) {
		for (int i = 0; ok && mods[i]; i++) {
			ok = Mode_Import(mods[i]);
		}
		ok = ok && LV_OWN(SM, "SetPoliceListTargetStatus", NULL, "i", 0L) && LvN_ModNone("OutPolice", "OnPoliceCreatedCallback") &&
			LvN_ModNone("Vehicles", "OnDeathCallback") && LvN_ModNone("Parking", "SwitchCharMissionCallback") &&
			LvN_ModNone("Parking", "SwitchVehicleMissionCallback") && LvN_ScorerOff() && LvN_Draw() &&
			LvN_Com("Mission_TestMayorShip_RemoteMessage4", "Trurl", SM, "ParkMayorShip", NULL);
	} else if (done == 0) {
		ok = Mode_Read(SM, "numCopsKilled", n) && Dt_SaveRepr("NumCopsKilled", n) && Mode_CountText(SM, "numCopsKilled", "COPS_TO_KILL", cnt) &&
			LvN_Set0("RemainingCharsScorer", "Text", cnt);
	}
	return Mode_Done(&t, ok && done >= 0);
}

/* ParkMayorShip(): to the platform with the ship; Berto's ship parked at the press */
static byte Steal_ParkMayorShip(void) {
	nativeVal_t n = { 0 };
	byte ok = Mode_G(SM, "COPS_TO_KILL", &n) && Dt_SaveRepr("NumCopsKilled", &n) && LV_OWN(SM, "SetPlatformTarget", NULL, "") && Mode_WayPoint() &&
		Mode_MMDesc("Mission_ParkMayorShip_MissionText1", 1) && Mode_SaveS("Parked.Press", "['SBERTO1']");
	Native_Free(&n);
	return ok;
}

LV_CORE0(Steal_Init)
LV_CORE0(Steal_CreateMissionShip)
LV_CORE0(Steal_TestForMissionShip)
LV_CORE0(Steal_SetPlatformTarget)
LV_CORE0(Steal_TestForCorrectPlatform)
LV_CORE0(Steal_UseParkedMayorShip)
LV_COREA(Steal_UsePhoneCab)
LV_CORE0(Steal_SetNextMission)
LV_CORE0(Steal_CGCalls)
LV_CORE0(Steal_StartMission)
LV_CORE0(Steal_ContinueMission)
LV_COREA(Steal_SetPoliceListTargetStatus)
LV_CORE0(Steal_ComeBack)
LV_COREA(Steal_MissionScorer)
LV_COREA(Steal_OnPoliceDeath)
LV_CORE0(Steal_ParkMayorShip)
LV_FN(Steal, InitPy, 1, Steal_Init_C)
LV_FN(Steal, CreateMissionShipPy, 0, Steal_CreateMissionShip_C)
LV_FN(Steal, TestForMissionShipPy, 0, Steal_TestForMissionShip_C)
LV_FN(Steal, SetPlatformTargetPy, 0, Steal_SetPlatformTarget_C)
LV_FN(Steal, TestForCorrectPlatformPy, 0, Steal_TestForCorrectPlatform_C)
LV_FN(Steal, UseParkedMayorShipPy, 4, Steal_UseParkedMayorShip_C)
LV_FN(Steal, UsePhoneCabPy, 4, Steal_UsePhoneCab_C)
LV_FN(Steal, SetNextMissionPy, 0, Steal_SetNextMission_C)
LV_FN(Steal, CGCallsPy, 0, Steal_CGCalls_C)
LV_FN(Steal, StartMissionPy, 0, Steal_StartMission_C)
LV_FN(Steal, ContinueMissionPy, 0, Steal_ContinueMission_C)
LV_FN(Steal, SetPoliceListTargetStatusPy, 1, Steal_SetPoliceListTargetStatus_C)
LV_FN(Steal, ComeBackPy, 0, Steal_ComeBack_C)
LV_FN(Steal, MissionScorerPy, 1, Steal_MissionScorer_C)
LV_FN(Steal, OnPoliceDeathPy, 2, Steal_OnPoliceDeath_C)
LV_FN(Steal, ParkMayorShipPy, 0, Steal_ParkMayorShip_C)

PyMethodDef g_DownTownStealMayorShipMethods[] = {
	{ "InitStatus", Steal_InitStatus, 1, NULL },
	{ "Init", Steal_InitPy, 1, NULL },
	{ "CreateMissionShip", Steal_CreateMissionShipPy, 1, NULL },
	{ "TestForMissionShip", Steal_TestForMissionShipPy, 1, NULL },
	{ "SetPlatformTarget", Steal_SetPlatformTargetPy, 1, NULL },
	{ "TestForCorrectPlatform", Steal_TestForCorrectPlatformPy, 1, NULL },
	{ "UseParkedMayorShip", Steal_UseParkedMayorShipPy, 1, NULL },
	{ "UsePhoneCab", Steal_UsePhoneCabPy, 1, NULL },
	{ "SetNextMission", Steal_SetNextMissionPy, 1, NULL },
	{ "CGCalls", Steal_CGCallsPy, 1, NULL },
	{ "StartMission", Steal_StartMissionPy, 1, NULL },
	{ "ContinueMission", Steal_ContinueMissionPy, 1, NULL },
	{ "SetPoliceListTargetStatus", Steal_SetPoliceListTargetStatusPy, 1, NULL },
	{ "ComeBack", Steal_ComeBackPy, 1, NULL },
	{ "MissionScorer", Steal_MissionScorerPy, 1, NULL },
	{ "OnPoliceDeath", Steal_OnPoliceDeathPy, 1, NULL },
	{ "ParkMayorShip", Steal_ParkMayorShipPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
