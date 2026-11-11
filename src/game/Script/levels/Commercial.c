/*
 * The scripts of the level Commercial in C (levels/commercial/scripts, issue #2): the Map module (the traffic, the
 * metro and the elevator to the temple), MapRace (the start positions of the eight races) and the ten missions of the
 * level (Mission_DestroyMickus, Mission_ProtectGus, Mission_DestroyStupidus, Mission_Win1stRace,
 * Mission_DestroyCommSats2 (in levels/LevelCommon.c), Mission_FirstMeetDeepThroat, Mission_GetDataPart3, Mission_DefeatCrazyGambler3,
 * Mission_DestroyPorkinsTransports). MapSnd is data (data/levels/commercial/mapsnd.def). The module variables of the
 * missions (their counters, lists, constants and the models) and the module code (the save variables they create,
 * MapRace.Races) stay in the scripts; the functions read and write them as their globals.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values, read the globals of their module where the
 * script reads them and write them back at once (OBJLIB 5; the lists of the missions are changed in place), and call
 * the engine in C: save variables, engine variables, entity lists, the money, the scorer, the language texts. The
 * engine functions the cases record or swap go through their seams (AddScheduledFunc, DeleteScheduledFuncs,
 * CreateEntity, UsrEntity, Set, SetAlarm, GetAlarm, Rand, Preload3DObject, TestLine, DelSaveVars, SAI.GetStateVehicle,
 * SSound.Play, SScorer.Set; OBJLIB 7). The entities the script holds are handle values (OBJLIB 7).
 *
 * Direct calls (stage 3): the calls of other ports (Speech, MissionsFuncs, SaveGame, RaceMaker, VehiclesNPC, Vehicles,
 * Traffic, PhoneCab, Menu, Elevator, Metro, MapRace) and of the module's own functions through its globals (PORT_LOWN:
 * a mission may replace one) go through their refs (PORT_CALL, the keyword calls among them), the callbacks handed to
 * Speech and Menu by the refs of their functions (LV_COM, LV_ONEND, LV_SCORERG), and so do the engine functions with
 * native entries (stage 7: SAI.SetStateVehicle and SetInertia, Scrap.Achievement and Scrap.SetMoney of a long,
 * SScorer.AddModel and CancelSpeech). By name stay the functions handed on as values: to the scheduler (the entry of a
 * port's own function, stage 4: Port_Scheduled), as the callbacks of Parking and Alarm, the AfterRemoteComFunc of
 * Speech.ForceRemoteCom (values of their variables, OBJLIB 5). The InitStatus functions that only set the traffic
 * ships, the start positions of the races (LvN_StartPos) and Mission_DestroyCommSats2 are LevelCommon's.
 */
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/scrap.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL commercial

#define DMK "Mission_DestroyMickus"
#define PG "Mission_ProtectGus"
#define DS "Mission_DestroyStupidus"
#define WR "Mission_Win1stRace"
#define FM "Mission_FirstMeetDeepThroat"
#define GD "Mission_GetDataPart3"
#define CG "Mission_DefeatCrazyGambler3"
#define PT "Mission_DestroyPorkinsTransports"

static const char *const ac_ships_bertos = "[\"SMerc1\",\"SMerc2\",\"SBishop1\",\"SBerto1\",\"SArchbishop1\"]";

/* SAI.SetStateVehicle(...) through its ref (the seam sees what a case put there) */
#define STATE(...) PORT_CALL(SAI, SetStateVehicle, NULL, __VA_ARGS__)

/* ---- native helpers of this file ---- */

/* SVec.ModSqr(SVec.Sub(a.Pos, b.Pos)) (sqr 1) or SVec.Mod(SVec.Sub(a.Pos, b.Pos)) (sqr 0) into *out */
static byte Cm_Dist(const nativeVal_t *a, const nativeVal_t *b, byte sqr, nativeVal_t *out) {
	nativeVal_t pa, pb, d;
	Native_SetNone(&pa);
	Native_SetNone(&pb);
	Native_SetNone(&d);
	Native_SetNone(out);
	byte ok = Mode_VGet(a, "Pos", &pa) && Mode_VGet(b, "Pos", &pb) && Mode_VecSub(&pa, &pb, &d) &&
		(sqr ? Mode_VecModSqr(&d, out) : Mode_VecMod(&d, out));
	Native_Free(&pa);
	Native_Free(&pb);
	Native_Free(&d);
	return ok;
}

/* SVec.ModSqr(SVec.Sub(e.Pos, usr.Pos)) op MAX_SCANNING_DIST * MAX_SCANNING_DIST of Mission_DestroyMickus: 1, 0, -1 */
static int Cm_Scan(const nativeVal_t *e, const nativeVal_t *usr, const char *op) {
	nativeVal_t d, a, b, m;
	Native_SetNone(&a);
	Native_SetNone(&b);
	Native_SetNone(&m);
	int r = Cm_Dist(e, usr, 1, &d) && Mode_G(DMK, "MAX_SCANNING_DIST", &a) && Mode_G(DMK, "MAX_SCANNING_DIST", &b) &&
		Mode_Arith('*', &a, &b, &m) ? Mode_Cmp(&d, op, &m) : -1;
	Native_Free(&d);
	Native_Free(&a);
	Native_Free(&b);
	Native_Free(&m);
	return r;
}

/* `usr.IsType("Car")` of the player (None: its AttributeError): 1, 0, -1 */
static int Cm_UsrCar(const nativeVal_t *usr) {
	return Mode_VIsType(usr, "Car");
}

/* Scrap.CreateEntityList(name) (a list that is there already is the engine's fatal check, as before) */
static void Cm_NewList(const char *name) {
	CHECK(cWorld_NewEntityList(g_pWorld, name));
}

/* Scrap.Preload3DObject(model) of a global of module */
static byte Cm_PreloadG(const char *module, const char *name) {
	nativeVal_t m;
	byte ok = Mode_G(module, name, &m) && Mode_Preload(&m);
	Native_Free(&m);
	return ok;
}

/* module.var = the global name of src (a function) and = None (OBJLIB 5) */
#define CM_HOOK(module, var, src, name) LvN_ModSetGlobal(module, var, src, name)

/* Scrap.Set("CurrentWaypointName", v) */
static byte Cm_Waypoint(const nativeVal_t *v) {
	return Mode_GVarSet("CurrentWaypointName", v);
}

static byte Cm_WaypointS(const char *s) {
	return Mode_GVarSetS("CurrentWaypointName", s);
}

/* `h.var == i` of a handle: 1, 0, -1 */
static int Cm_AttrEqI(const nativeVal_t *h, const char *var, long i) {
	nativeVal_t v;
	Native_SetNone(&v);
	int r = Mode_VGet(h, var, &v) ? Mode_CmpI(&v, "==", i) : -1;
	Native_Free(&v);
	return r;
}

/* `h.var == "text"` of a handle: 1, 0, -1 */
static int Cm_AttrEqS(const nativeVal_t *h, const char *var, const char *s) {
	nativeVal_t v, t;
	Native_SetNone(&v);
	Native_SetText(&t, s, (int)strlen(s));
	int r = Mode_VGet(h, var, &v) ? Mode_Cmp(&v, "==", &t) : -1;
	Native_Free(&v);
	Native_Free(&t);
	return r;
}

/* the global name of module = [] (del name first when del: the NameError of a missing one) */
static byte Cm_GEmpty(const char *module, const char *name, byte del) {
	nativeVal_t v;
	if (del && !Mode_DelGlobal(module, name)) {
		return 0;
	}
	Native_SetEmpty(&v, NV_LIST);
	byte ok = Mode_Put(module, name, NULL, &v);
	Native_Free(&v);
	return ok;
}

/* Vehicles.OnDeath(LooserName, WinnerName) */
static byte Cm_OnDeath(const nativeVal_t *a) {
	return PORT_CALL(Vehicles, OnDeath, NULL, "vv", &a[0], &a[1]);
}

/* `usr and usr.Name == name` of the player: 1, 0, -1 */
static int Cm_UsrNameIs(const nativeVal_t *name) {
	nativeVal_t usr;
	Native_SetNone(&usr);
	int r = Mode_UsrHandle(&usr) ? LvN_UsrNameIs(&usr, name) : -1;
	Native_Free(&usr);
	return r;
}

/* `x` of the global name of module into out */
static byte Cm_GRepr(const char *module, const char *name, nativeVal_t *out) {
	nativeVal_t v;
	byte ok = Mode_G(module, name, &v);
	if (ok) {
		Mode_Repr(&v, out);
	}
	Native_Free(&v);
	return ok;
}

/* the save variable name = `v` and = `int(name) + d` (the new value into *n when given) */
static byte Cm_SaveAdd(const char *name, long d, nativeVal_t *n) {
	nativeVal_t i, k, r;
	Native_SetInt(&k, d);
	Native_SetNone(&r);
	byte ok = Mode_SaveInt(name, &i) && Native_Arith('+', &i, &k, &r);
	if (ok) {
		LvN_SaveRepr(name, &r);
	}
	Native_Free(&i);
	if (ok && n) {
		*n = r;
	} else {
		Native_Free(&r);
	}
	return ok;
}

/* a mission's description and way point: SaveGame.MMDesc(desc, 1); SaveGame.QWayPoint() */
static byte Cm_Desc(const char *desc) {
	return Mode_MMDesc(desc, 1) && Mode_WayPoint();
}

/* the scheduled call of a global function of module after delay s without arguments */
#define CM_LATER(s, module, func, name) Mode_Later(s, module, func, NULL, 0, name)

/* (a, b, c, d) of four ints */
static void Cm_Ints4(nativeVal_t *out, long a, long b, long c, long d) {
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), a);
	Native_SetInt(Native_Append(out), b);
	Native_SetInt(Native_Append(out), c);
	Native_SetInt(Native_Append(out), d);
}

/* h.Ang = (0, spw[1], 0) of a spawn position */
static byte Cm_SpawnAng(const nativeVal_t *h, const nativeVal_t *spw) {
	nativeVal_t y;
	byte ok = LvN_Item(spw, 1, &y) && Mode_SetAng(h, &y);
	Native_Free(&y);
	return ok;
}

/* VehiclesNPC.MakeAI(profile, name, InitState = 0) */
static byte Cm_MakeAI(const char *profile, const char *name, nativeVal_t *out) {
	nativeVal_t args[2];
	LvN_Str(&args[0], profile);
	LvN_Str(&args[1], name);
	byte ok = PORT_CALL(VehiclesNPC, MakeAI, out, "vv;InitState=i", &args[0], &args[1], 0L);
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	return ok;
}

/*
 * UsePhoneCab of a mission: the audio com of the phone cab, the police warning while the alarm is on, else the NI
 * talk of who with the end function end (a function of module given by its ref, for Speech.OnEndOfSpeech). global: pc
 * is the global pc of module; activate: the cab stops being the target; stop: the scheduled functions of that name go;
 * after: the cab's use afterwards; outOfRange: Speech.OnOutOfRange = None at the end
 */
typedef struct {
	const char *police, *who;
	const portRef_t *end;
	const char *ni, *stop, *after;
	byte global, activate, outOfRange;
} cmPhone_t;

static byte Cm_UsePhone(const nativeVal_t *a, const char *module, const cmPhone_t *p) {
	nativeVal_t pc, gpc;
	Native_SetNone(&pc);
	Native_SetNone(&gpc);
	byte ok = Mode_Import("PhoneCab") && LvN_Speakers(&a[0], &a[1]) &&
		PORT_CALL(PhoneCab, UseAudioCom, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]) && Mode_EntityH(&a[1], &pc) &&
		(!p->global || Mode_Put(module, "pc", NULL, &pc));
	int alarm = ok ? Mode_AlarmOn() : -1;
	/* the cab: the global pc read again where the script reads it, else the local */
	const nativeVal_t *cab = p->global ? &gpc : &pc;
	if (alarm > 0) {
		ok = (!p->global || Mode_G(module, "pc", &gpc)) && PORT_CALL(Speech, StartObjSpch, NULL, "vss", cab, p->police, p->who);
	} else if (alarm == 0) {
		ok = (!p->activate || ((!p->global || Mode_G(module, "pc", &gpc)) && LvN_Activate(cab, 0))) && (!p->stop || Mode_DelSchedS(p->stop)) &&
			LvN_OnEndRef(p->end, NULL) && Mode_NISpeech(p->who, p->ni) && (!p->after || LvN_SetS(&pc, "UseFunc", p->after));
	} else {
		ok = 0;
	}
	ok = ok && (!p->outOfRange || LvN_ModNone("Speech", "OnOutOfRange"));
	Native_Free(&pc);
	Native_Free(&gpc);
	return ok;
}

/* the adapter of a UsePhoneCab(UsrName, CharName, optId, FullOption) of a mission */
#define CM_PHONE(fn, module, ...) \
	static byte fn##_C(const nativeVal_t *a, nativeVal_t *out) { \
		static const cmPhone_t p = { __VA_ARGS__ }; \
		(void)out; \
		return Cm_UsePhone(a, module, &p); \
	}

/* the core of a profile function: the profile of the table */
#define CM_PROFILE(fn, vars) \
	static byte fn##_C(const nativeVal_t *a, nativeVal_t *out) { \
		(void)a; \
		return LvN_Profile(vars, out); \
	}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors; the traffic by who controls the robots, the metro and the elevator */
static byte CmMap_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "OutPolice", "Traffic", NULL };
	static const char *const light[] = { "TLight1", "TLight2", "TLight3", NULL }, *const med[] = { "TMed1", "TMed2", "TMed3", NULL };
	static const char *const none[] = { NULL };
	static const char *const defs[][2] = { { "Metro", "DM_Door_MetroExt_Comercial" }, { "MetroExt", "DM_Door_MetroExt_Comercial" },
		{ "Elevator", "DM_Door_Elevator_Temple" } };
	nativeVal_t v;
	(void)a;
	(void)out;
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	Native_SetInt(&v, 1);
	if (!Mode_SetModVar("Init", "Outdoor", &v)) {
		return 0;
	}
	if (LvN_SaveIs("RobotsControlledByBoss", "1")) {
		if (!Mode_BossTraffic(1)) {
			return 0;
		}
	} else {
		static const struct {
			const char *var;
			const char *const *ships;
			long n;
		} traffic[] = { { "AcNormalShips", light, 80 }, { "AcMediumShips", med, 40 }, { "AcHeavyShips", none, 0 } };
		static const char *const nums[] = { "NumNormal", "NumMedium", "NumHeavy" };
		if (!Mode_GVarSetI("iTrafficNumber", 10) || !Mode_GVarSetI("MaxChasingShips", -1)) {
			return 0;
		}
		for (int i = 0; i < 3; i++) {
			Mode_Texts(&v, traffic[i].ships, -1);
			byte ok = Mode_SetModVar("Traffic", traffic[i].var, &v);
			Native_Free(&v);
			Native_SetInt(&v, traffic[i].n);
			if (!ok || !Mode_SetModVar("Traffic", nums[i], &v)) {
				return 0;
			}
		}
	}
	for (int i = 0; i < 3; i++) {
		if (!PORT_CALL(Elevator, SetDef, NULL, "ss", defs[i][0], defs[i][1])) {
			return 0;
		}
	}
	return 1;
}

/* Init(LevelPath): the metro, the elevator to the temple, the colour of the clouds */
static byte CmMap_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Metro") && Mode_Import("Elevator") && PORT_CALL(Metro, SetAsMetro, NULL, "s", "DM_Door_MetroExt_Comercial") &&
		Mode_Connect("DM_Door_Elevator_Temple", "Temple", 0) && Mode_GVarSetI("CameraCloudRed", 30) && Mode_GVarSetI("CameraCloudGreen", 60) &&
		Mode_GVarSetI("CameraCloudBlue", 100);
}

LV_FN(CmMap, InitStatusA, 1, CmMap_InitStatus)
LV_FN(CmMap, InitA, 1, CmMap_Init)

PyMethodDef g_CommercialMapMethods[] = {
	{ "InitStatus", CmMap_InitStatusA, 1, NULL },
	{ "Init", CmMap_InitA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapRace ---- */

#include "CommercialRaces.inc"

/* InitStatus(LevelPath), Init(LevelPath): nothing */
LV_FN(MapRace, PassA, 1, LvN_Pass)

/* race k: RaceMaker.StartPos (no arguments), MapRace_Race<k> and its entry */
#define RACE(k) MODE_FN(MapRace, Race##k, 0, LvN_StartPos(races[k - 1].pos, races[k - 1].n))
RACE(1)
RACE(2)
RACE(3)
RACE(4)
RACE(5)
RACE(6)
RACE(7)
RACE(8)

PyMethodDef g_CommercialMapRaceMethods[] = {
	{ "InitStatus", MapRace_PassA, 1, NULL },
	{ "Init", MapRace_PassA, 1, NULL },
	{ "EasyRaceCommercial1", MapRace_Race1, 1, NULL },
	{ "EasyRaceCommercial2", MapRace_Race2, 1, NULL },
	{ "EasyRaceCommercial3", MapRace_Race3, 1, NULL },
	{ "EasyRaceCommercial4", MapRace_Race4, 1, NULL },
	{ "EasyRaceCommercial5", MapRace_Race5, 1, NULL },
	{ "EasyRaceCommercial6", MapRace_Race6, 1, NULL },
	{ "EasyRaceCommercial7", MapRace_Race7, 1, NULL },
	{ "EasyRaceCommercial8", MapRace_Race8, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyMickus: scan the transports for Mickus, chase him with the functionaries after the player ---- */

static const char *const func_enemies_profile[] = { "Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=5",
	"UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10", "UsePercHook=0", "FactorHookMap=4", "CadenceShoot=8", "ControlAim=3",
	"AngAttack=3.1415", "CounterAttack=0.50", "ControlBoost=1", "ControlBoostItemLife=0", "ControlBoostItemAmmo=0",
	"FactorDamageGetAway=0.30", "FactorDamageItemLife=0.25", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=2",
	"TimeMaxStrafe=6", "VelMaxStrafe=0.75", NULL };

CM_PROFILE(Mickus_GetFuncEnemiesProfile, func_enemies_profile)

static PyObject *Mickus_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, DMK, ac_ships_bertos);
}

/* Init(LevelPath): the transports, the mission when the player has a ship, the callbacks of boarding */
static byte Mickus_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t n, usr, state, first;
	(void)a;
	(void)out;
	if (!Mode_Import("Parking") || !Cm_PreloadG(DMK, "TreasureModel")) {
		return 0;
	}
	Cm_NewList("Transports");
	Cm_NewList("FuncEnemies");
	if (!Mode_ListSet("Transports", "OnDeath", "Mission_DestroyMickus.OnTransportsDeath") ||
		!Mode_ListSet("FuncEnemies", "OnDeath", "Mission_DestroyMickus.OnFuncEnemiesDeath")) {
		return 0;
	}
	byte ok = Mode_SaveInt("NumTransports", &n) && PORT_LOWN(Mission_DestroyMickus, CreateTransports, NULL, "v", &n);
	Native_Free(&n);
	Native_SetNone(&usr);
	int car = ok ? LvN_Usr(&usr) : -1;
	if (car > 0) {
		car = Cm_UsrCar(&usr);
	}
	Native_Free(&usr);
	if (car < 0 || (car && !PORT_LOWN(Mission_DestroyMickus, InitMission, NULL, "")) ||
		!CM_HOOK("Parking", "SwitchVehicleMissionCallback", DMK, "InitMission") ||
		!CM_HOOK("Parking", "SwitchCharMissionCallback", DMK, "StopMission")) {
		return 0;
	}
	Mode_SaveVar("MissionState", &state);
	LvN_Str(&first, "FirstTime");
	int again = Mode_Cmp(&state, "!=", &first);
	Native_Free(&state);
	Native_Free(&first);
	return again >= 0 && (!again || LvN_Com("Mission_Search&DestroyMickus_RemoteMessage3", "Traitorius", NULL, NULL, NULL));
}

/* CreateTransports(numTransports): the transports on the traffic paths, one of them Mickus */
static byte Mickus_CreateTransports(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *hi = Mode_V(&t), *f = Mode_V(&t), *r = Mode_V(&t), *n = Mode_V(&t), *lst = Mode_V(&t), *m = Mode_V(&t), *e = Mode_V(&t);
	nativeVal_t *life = Mode_V(&t);
	long end;
	(void)out;
	if (!Mode_Import("Traffic") || !Mode_Import("VehiclesNPC") || !Native_Arith('+', &a[0], Mode_I(&t, 1), hi) || !Mode_Range(hi, &end)) {
		return Mode_Done(&t, 0);
	}
	/* for i in range(1, numTransports + 1) */
	for (long i = 1; i < end; i++) {
		nativeVal_t name, trans, tn, id;
		char s[32];
		Str_Printf(s, sizeof(s), "Transport-%ld", i);
		LvN_Str(&name, s);
		Native_SetNone(&trans);
		Native_SetNone(&tn);
		Native_SetInt(&id, 0);
		byte ok = PORT_CALL(Traffic, SetPath, NULL, "") && PORT_CALL(VehiclesNPC, MakeAI, &trans, "sv", "TLight2", &name) &&
			Mode_VGet(&trans, "Name", &tn) && PORT_CALL(SAI, SetInertia, NULL, "vi", &tn, 0L) && PORT_CALL(Traffic, ResetPath, NULL, "");
		Native_Free(&tn);
		ok = ok && Mode_VGet(&trans, "Name", &tn) && STATE("ividd", 3L, &tn, 0L, 2.0, 1.0) && LvN_SetS(&trans, "EntityList", "Transports");
		Native_Free(&tn);
		ok = ok && Mode_AppendName(DMK, "transports", &trans);
		Native_Free(&name);
		Native_Free(&trans);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	/* mickus = transports[int(Scrap.Rand(0.0, float(numTransports) - 0.05))] */
	nativeVal_t d;
	Native_SetFloat(&d, 0.05);
	Native_Free(hi);
	if (!Native_ToFloat(&a[0], f) || !Native_Arith('-', f, &d, hi)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(&d, 0.0);
	if (!Mode_Rand(&d, hi, r) || !Native_ToInt(r, n) || !Mode_G(DMK, "transports", lst) || !Native_Subscript(lst, n, m) ||
		!Mode_Put(DMK, "mickus", NULL, m)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(m);
	if (!Mode_G(DMK, "mickus", m) || !Mode_EntityH(m, e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, LvN_RaiseText("La entidad de mickus debe existir"));
	}
	byte ok = LvN_SetS(e, "OnDamage", "Mission_DestroyMickus.OnMickusDamage") && Mode_G(DMK, "MAX_MICKUS_LIFE", life) &&
		Mode_VSet(e, "MaxLife", life);
	Native_Free(life);
	ok = ok && Mode_G(DMK, "MAX_MICKUS_LIFE", life) && Mode_VSet(e, "Life", life) && LvN_SetI(e, "Money", -1);
	return Mode_Done(&t, ok);
}

/* InitMission(): the transports as targets, the scorer of the scan, the scan every second */
static byte Mickus_InitMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t lst;
	(void)a;
	(void)out;
	if (LvN_SaveIs("MissionState", "FirstTime")) {
		SaveVar_Set("MissionState", "InspectTransports");
		if (!LvN_Com("Mission_Search&DestroyMickus_RemoteMessage1", "Traitorius", NULL, NULL, NULL)) {
			return 0;
		}
	}
	byte ok = Mode_G(DMK, "transports", &lst) && PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &lst);
	Native_Free(&lst);
	return ok && LV_SCORERG(Mission_DestroyMickus, MissionScorer) && LvN_Draw() &&
		Mode_GVarSetS("OnFinishAutoUpdate", "Mission_DestroyMickus.OnFinishScan") && CM_LATER(1.0, DMK, "ScanShip", "ScanShip");
}

/* StopMission(): no scorer, no scan */
static byte Mickus_StopMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LvN_ScorerOff() && Mode_DelSchedS("ScanShip");
}

/* OnMickusDamage(LooserName, WinnerName, Damage): the player shot Mickus: identified */
static byte Mickus_OnMickusDamage(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e;
	(void)out;
	int by = Cm_UsrNameIs(&a[1]);
	if (by <= 0) {
		return by == 0;
	}
	Native_SetNone(&e);
	byte ok = Mode_EntityH(&a[0], &e) && (!Native_True(&e) || (LvN_SetS(&e, "OnDamage", "") &&
		PORT_LOWN(Mission_DestroyMickus, MickusIdentified, NULL, "")));
	Native_Free(&e);
	return ok;
}

/* SScorer.Set(0, "ScanRemainingTime", var, MAX_SCANNING_TIME) */
static byte Mk_ScanTime(const char *var) {
	nativeVal_t v;
	byte ok = Mode_G(DMK, "MAX_SCANNING_TIME", &v) && LvN_Set0("ScanRemainingTime", var, &v);
	Native_Free(&v);
	return ok;
}

/* ScanShip(): every second: the ship being scanned stays near, else the next transport near the player is scanned */
static byte Mickus_ScanShip(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *cur = Mode_V(&t), *e = Mode_V(&t), *blink = Mode_V(&t), *lst = Mode_V(&t);
	(void)a;
	(void)out;
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	if (!CM_LATER(1.0, DMK, "ScanShip", "ScanShip") || !Mode_G(DMK, "currentScannedShip", cur) || !Mode_EntityH(cur, e)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(e)) {
		int far = Cm_Scan(e, usr, ">");
		if (far > 0) {
			nativeVal_t lost;
			Mode_ScorerGet(0, "ScanRemainingTime", "Blink", blink);
			if (LvN_EqI(blink, 0)) {
				Native_SetNone(&lost);
				byte ok = LvN_ScanBlink(1) && Mode_G(DMK, "MAX_TIME_TARGET_LOST", &lost) &&
					Mode_LaterV(&lost, DMK, "CheckContinueScanning", NULL, 0, NULL);
				Native_Free(&lost);
				return Mode_Done(&t, ok);
			}
			return Mode_Done(&t, 1);
		}
		return Mode_Done(&t, far == 0 && LvN_ScanBlink(0));
	}
	if (!Mode_G(DMK, "transports", lst)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(lst);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t s, h, name;
		Native_SetNone(&s);
		Native_SetNone(&h);
		Native_SetNone(&name);
		int near = Native_SeqItem(lst, i, &s) && Mode_EntityH(&s, &h) ? Native_True(&h) : -1;
		if (near > 0) {
			near = Cm_Scan(&h, usr, "<");
		}
		if (near > 0) {
			ok = Mode_VGet(&h, "Name", &name) && Mode_Put(DMK, "currentScannedShip", NULL, &name);
			Native_Free(&name);
			ok = ok && Mode_G(DMK, "currentScannedShip", &name) && Cm_Waypoint(&name) && LvN_Set0I("ScanStatusText", "Visible", 1) &&
				LvN_ScanBlink(0) && Mk_ScanTime("MaxValue") && Mk_ScanTime("Value") && LvN_Set0I("ScanRemainingTime", "Visible", 1);
		}
		ok = ok && near >= 0;
		Native_Free(&s);
		Native_Free(&h);
		Native_Free(&name);
		if (near > 0) {
			break;
		}
	}
	return Mode_Done(&t, ok);
}

/* CheckContinueScanning(): the ship came back near in time, or the scan starts again */
static byte Mickus_CheckContinueScanning(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *cur = Mode_V(&t), *e = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_UsrHandle(usr) || !Mode_G(DMK, "currentScannedShip", cur) || !Mode_EntityH(cur, e)) {
		return Mode_Done(&t, 0);
	}
	int near = Native_True(usr) && Native_True(e) ? Cm_Scan(e, usr, "<") : 0;
	if (near > 0) {
		return Mode_Done(&t, LvN_ScanBlink(0));
	}
	return Mode_Done(&t, near == 0 && PORT_LOWN(Mission_DestroyMickus, ResetScan, NULL, ""));
}

/* ResetScan(): no ship scanned, the scorer hidden (refilled a moment later) */
static byte Mickus_ResetScan(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LvN_PutS(DMK, "currentScannedShip", "") && Cm_WaypointS("") && LvN_Set0I("ScanRemainingTime", "IsAutoUpdatable", 0) &&
		LvN_Set0I("ScanRemainingTime", "Visible", 0) && LvN_Set0I("ScanStatusText", "Visible", 0) && CM_LATER(0.1, DMK, "ResetScan2", NULL);
}

static byte Mickus_ResetScan2(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LvN_Set0I("ScanRemainingTime", "Blink", 0) && Mk_ScanTime("MaxValue") && Mk_ScanTime("Value");
}

/* OnFinishScan(): the scan bar ran out: Mickus, or one transport fewer */
static byte Mickus_OnFinishScan(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *cur = Mode_V(&t), *m = Mode_V(&t), *e = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Cm_WaypointS("") || !LvN_Set0I("ScanStatusText", "Visible", 0) || !LvN_Set0I("ScanRemainingTime", "Visible", 0)) {
		return Mode_Done(&t, 0);
	}
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	int found = Mode_G(DMK, "currentScannedShip", cur) && Mode_G(DMK, "mickus", m) ? Mode_Cmp(cur, "==", m) : -1;
	byte ok = found >= 0;
	if (found > 0) {
		ok = PORT_LOWN(Mission_DestroyMickus, MickusIdentified, NULL, "");
	} else if (found == 0) {
		Native_Free(cur);
		ok = LvN_Com("Mission_Search&DestroyMickus_RemoteMessage2", "Traitorius", NULL, NULL, NULL) && Mode_G(DMK, "currentScannedShip", cur) &&
			Mode_Remove(DMK, "transports", cur);
		Native_Free(cur);
		ok = ok && Mode_G(DMK, "currentScannedShip", cur) && Mode_EntityH(cur, e) && (!Native_True(e) || LvN_Activate(e, 0));
	}
	return Mode_Done(&t, ok && PORT_LOWN(Mission_DestroyMickus, ResetScan, NULL, ""));
}

/* the chase: SAI.SetStateVehicle(5, mickus, usr.Name, MAX_MICKUS_DIST + MAX_MICKUS_DIST / 2) */
static byte Mk_Flee(const nativeVal_t *usr) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *m = Mode_V(&t), *un = Mode_V(&t), *d1 = Mode_V(&t), *d2 = Mode_V(&t), *h = Mode_V(&t), *d = Mode_V(&t);
	byte ok = Mode_G(DMK, "mickus", m) && Mode_VGet(usr, "Name", un) && Mode_G(DMK, "MAX_MICKUS_DIST", d1) &&
		Mode_G(DMK, "MAX_MICKUS_DIST", d2) && Mode_Arith('/', d2, Mode_I(&t, 2), h) && Native_Arith('+', d1, h, d) &&
		STATE("ivvv", 5L, m, un, d);
	return Mode_Done(&t, ok);
}

/* MickusIdentified(): the other transports go, Mickus flees, the functionaries come after the message */
static byte Mickus_MickusIdentified(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *lst = Mode_V(&t), *m = Mode_V(&t), *e = Mode_V(&t), *style = Mode_V(&t), *s2 = Mode_V(&t);
	nativeVal_t *n = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_Import("Parking") || !CM_HOOK("Parking", "SwitchVehicleMissionCallback", DMK, "ReassignTarget")) {
		return Mode_Done(&t, 0);
	}
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	if (!Mode_G(DMK, "transports", lst)) {
		return Mode_Done(&t, 0);
	}
	int count = Native_ForCount(lst);
	if (count < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < count; i++) {
		nativeVal_t s, h, name, mk;
		Native_SetNone(&s);
		Native_SetNone(&h);
		Native_SetNone(&name);
		Native_SetNone(&mk);
		int other = Native_SeqItem(lst, i, &s) && Mode_EntityH(&s, &h) ? Native_True(&h) : -1;
		if (other > 0) {
			other = Mode_VGet(&h, "Name", &name) && Mode_G(DMK, "mickus", &mk) ? Mode_Cmp(&name, "!=", &mk) : -1;
		}
		byte ok = other >= 0 && (!other || Mode_VDel(&h));
		Native_Free(&s);
		Native_Free(&h);
		Native_Free(&name);
		Native_Free(&mk);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Cm_GEmpty(DMK, "transports", 1) || !Mode_G(DMK, "mickus", m) || !Mode_EntityH(m, e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	byte ok = LV_COM("Mission_Search&DestroyMickus_RemoteMessage4", "Traitorius", Mission_DestroyMickus, TraitoriusFuncEnemiesCom, NULL) &&
		LvN_TargetName(e) && Mode_VGet(e, "Style", style) && Native_Arith('+', style, Mode_I(&t, 2000), s2) && Mode_VSet(e, "Style", s2) &&
		LvN_SetS(e, "TgType", "red") && LvN_SetS(e, "OnDamage", "") && LvN_ScorerOff() && LvN_Draw() && Mk_Flee(usr) &&
		Mode_DelSchedS("ScanShip") && Mode_SaveInt("NumTransports", n);
	if (ok && LvN_EqI(n, 10)) {
		SaveVar_Set("NumTransports", "5");
	} else if (ok && LvN_EqI(n, 5)) {
		SaveVar_Set("NumTransports", "3");
	}
	return Mode_Done(&t, ok && CM_LATER(4, DMK, "CheckMickusVisibility", "CheckMickusVisibility"));
}

/* ReassignTarget(): after boarding: Mickus flees from the new ship, the functionaries attack it */
static byte Mickus_ReassignTarget(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *m = Mode_V(&t), *e = Mode_V(&t), *name = Mode_V(&t);
	(void)a;
	(void)out;
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	if (!Mode_G(DMK, "mickus", m) || !Mode_EntityH(m, e) || (Native_True(e) && !Mk_Flee(usr)) ||
		!Mode_ListGet("FuncEnemies", "First", name)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(e);
	if (!Mode_EntityH(name, e)) {
		return Mode_Done(&t, 0);
	}
	while (Native_True(e)) {
		nativeVal_t en, un, next;
		Native_SetNone(&en);
		Native_SetNone(&un);
		Native_SetNone(&next);
		byte ok = Mode_VGet(e, "Name", &en) && Mode_VGet(usr, "Name", &un) && STATE("ivv", 2L, &en, &un) && Mode_VGet(e, "NextInList", &next);
		Native_Free(e);
		ok = ok && Mode_EntityH(&next, e);
		Native_Free(&en);
		Native_Free(&un);
		Native_Free(&next);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* TraitoriusFuncEnemiesCom(): two functionaries after the player, Traitorius warns 2 s later */
static byte Mickus_TraitoriusFuncEnemiesCom(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t args[2];
	(void)a;
	(void)out;
	if (!PORT_LOWN(Mission_DestroyMickus, CreateFuncEnemies, NULL, "i", 2L)) {
		return 0;
	}
	LvN_Str(&args[0], "Mission_Search&DestroyMickus_RemoteMessage7");
	LvN_Str(&args[1], "Traitorius");
	byte ok = Mode_LaterIn(2, "Speech", "AddRemoteCom", args, 2, NULL);
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	return ok;
}

/* long(v) of the script (OBJLIB 9: through the Python number function) */
static byte Mk_Long(const nativeVal_t *v, nativeVal_t *out) {
	PyObject *o = Native_ToPy(v), *r = o ? PyNumber_Long(o) : NULL;
	Native_SetNone(out);
	if (r) {
		Native_Arg(r, out);
	}
	Py_XDECREF(r);
	Py_XDECREF(o);
	return r != NULL;
}

/* the functionaries of FuncEnemies leave the chase (the loop over the list by NextInList) */
static byte Mk_FuncsOff(void) {
	nativeVal_t name;
	if (!Mode_ListGet("FuncEnemies", "First", &name)) {
		return 0;
	}
	while (Native_True(&name)) {
		nativeVal_t e, next, en;
		Native_SetNone(&e);
		Native_SetNone(&next);
		Native_SetNone(&en);
		int there = Mode_EntityH(&name, &e) ? Native_True(&e) : -1;
		byte ok = there > 0 && Mode_VGet(&e, "NextInList", &next) && LvN_SetS(&e, "EntityList", "Traffic") && LvN_SetS(&e, "TgType", "none") &&
			Mode_VGet(&e, "Name", &en) && STATE("iv", 3L, &en);
		Native_Free(&name);
		name = next;
		Native_Free(&e);
		Native_Free(&en);
		if (!ok) {
			Native_Free(&name);
			return there == 0;
		}
	}
	Native_Free(&name);
	return 1;
}

/* CheckMickusVisibility(): Mickus out of range and out of sight: lost, the mission starts again with fewer transports */
static byte Mickus_CheckMickusVisibility(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *m = Mode_V(&t), *mic = Mode_V(&t), *ds = Mode_V(&t), *d = Mode_V(&t), *g = Mode_V(&t);
	nativeVal_t *m1 = Mode_V(&t), *m2 = Mode_V(&t), *mm = Mode_V(&t), *up = Mode_V(&t), *mp = Mode_V(&t), *res = Mode_V(&t);
	nativeVal_t *r1 = Mode_V(&t), *n = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_UsrHandle(usr) || !Mode_G(DMK, "mickus", m) || !Mode_EntityH(m, mic)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(usr) && Native_True(mic)) {
		if (!Cm_Dist(usr, mic, 1, ds) || !Mk_Long(ds, d) || !Mode_G(DMK, "MAX_MICKUS_DIST", g) || !Mk_Long(g, m1)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(g);
		if (!Mode_G(DMK, "MAX_MICKUS_DIST", g) || !Mk_Long(g, m2) || !Mode_Arith('*', m1, m2, mm)) {
			return Mode_Done(&t, 0);
		}
		int far = Mode_Cmp(d, ">", mm);
		if (far < 0) {
			return Mode_Done(&t, 0);
		}
		if (far) {
			if (!Mode_VGet(usr, "Pos", up) || !Mode_VGet(mic, "Pos", mp) || !Mode_TestLineSeam(up, mp, Mode_S(&t, ""), res)) {
				return Mode_Done(&t, 0);
			}
			int lost = Native_True(res);
			if (lost && !LvN_Item(res, 1, r1)) {
				return Mode_Done(&t, 0);
			}
			if (lost && LvN_EqS(r1, "")) {
				int car;
				if (!Mk_FuncsOff() || (car = Cm_UsrCar(usr)) < 0 || (car && !LvN_SetS(usr, "Target", "")) ||
					!LvN_Com("Mission_Search&DestroyMickus_RemoteMessage6", "Traitorius", NULL, NULL, NULL) || !Mode_VDel(mic) ||
					!LvN_PutS(DMK, "currentScannedShip", "") || !Cm_GEmpty(DMK, "transports", 0) || !LvN_PutS(DMK, "mickus", "") ||
					!Mode_Import("Parking") || !CM_HOOK("Parking", "SwitchVehicleMissionCallback", DMK, "InitMission") ||
					!Mode_SaveInt("NumTransports", n) || !PORT_LOWN(Mission_DestroyMickus, CreateTransports, NULL, "v", n)) {
					return Mode_Done(&t, 0);
				}
				return Mode_Done(&t, PORT_LOWN(Mission_DestroyMickus, InitMission, NULL, ""));
			}
		}
	}
	return Mode_Done(&t, CM_LATER(2, DMK, "CheckMickusVisibility", "CheckMickusVisibility"));
}

/* CreateFuncEnemies(numEnemies): functionaries at free spawn points, after the player */
static byte Mickus_CreateFuncEnemies(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	long n;
	(void)out;
	if (!Mode_Import("VehiclesNPC") || !Mode_Import("OutMap")) {
		return 0;
	}
	Native_SetNone(&usr);
	int u = LvN_Usr(&usr);
	byte ok = u >= 0 && (!u || Mode_Range(&a[0], &n));
	for (long i = 0; ok && u > 0 && i < n; i++) {
		nativeVal_t fn, pd, fname, uname;
		Native_SetNone(&pd);
		Native_SetNone(&fname);
		Native_SetNone(&uname);
		ok = Mode_SpawnCar("Functionary", 0, &fn, NULL) && PORT_LOWN(Mission_DestroyMickus, GetFuncEnemiesProfile, &pd, "") && LvN_StdAI(&fn, &pd) &&
			LvN_SetS(&fn, "TgType", "red") && LvN_SetS(&fn, "EntityList", "FuncEnemies") && LvN_Life(&fn, DMK, "FUNCS_LIFE") &&
			Mode_VGet(&fn, "Name", &fname) && Mode_VGet(&usr, "Name", &uname) && STATE("ivv", 2L, &fname, &uname);
		Native_Free(&fn);
		Native_Free(&pd);
		Native_Free(&fname);
		Native_Free(&uname);
	}
	Native_Free(&usr);
	return ok;
}

/* the player killed it and the alarm is off: Scrap.SetAlarm(1) */
static byte Mk_Alarm(void) {
	nativeVal_t one;
	int on = Mode_AlarmOn();
	Native_SetInt(&one, 1);
	return on >= 0 && (on || Mode_SetAlarm(&one));
}

/* OnFuncEnemiesDeath(LooserName, WinnerName): the alarm when the player killed it, another one 5 s later */
static byte Mickus_OnFuncEnemiesDeath(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t one;
	(void)out;
	int by = Cm_UsrNameIs(&a[1]);
	if (by < 0 || (by && !Mk_Alarm())) {
		return 0;
	}
	Native_SetInt(&one, 1);
	return Mode_Later(5, DMK, "CreateFuncEnemies", &one, 1, NULL);
}

/* OnTransportsDeath(LooserName, WinnerName): one transport fewer; Mickus killed by the player drops his money */
static byte Mickus_OnTransportsDeath(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e, pos;
	(void)out;
	if (!Mode_Import("Vehicles")) {
		return 0;
	}
	int in = Mode_InG(&a[0], DMK, "transports");
	if (in < 0) {
		return 0;
	}
	if (in) {
		int cur = Mode_Remove(DMK, "transports", &a[0]) ? Mode_GCmp(DMK, "currentScannedShip", "==", &a[0]) : -1;
		if (cur < 0 || (cur && (!PORT_LOWN(Mission_DestroyMickus, ResetScan, NULL, "") || !LvN_PutS(DMK, "currentScannedShip", "")))) {
			return 0;
		}
	}
	int by = Cm_UsrNameIs(&a[1]);
	if (by <= 0) {
		return by == 0;
	}
	int mk = Mk_Alarm() ? Mode_GCmp(DMK, "mickus", "==", &a[0]) : -1;
	if (mk <= 0) {
		return mk == 0;
	}
	Native_SetNone(&e);
	Native_SetNone(&pos);
	byte ok = Mode_EntityH(&a[0], &e) && (!Native_True(&e) || (LvN_SetS(&e, "EntityList", "") && Mode_VGet(&e, "Pos", &pos) &&
		PORT_LOWN(Mission_DestroyMickus, DropMoney, NULL, "v", &pos)));
	Native_Free(&e);
	Native_Free(&pos);
	return ok;
}

/* DropMoney(pos): Mickus' treasure and its arrow */
static byte Mickus_DropMoney(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_Drop(DMK, &a[0], "MickusMoney", "TreasureModel", NULL, 1, "Mission_DestroyMickus.TakeMoney", "TakeTreasure", "MissionCheckArrow");
}

/* the item and the arrow taken (two texts) */
static byte Cm_TakenS(const char *item, const char *arrow) {
	nativeVal_t i, r;
	LvN_Str(&i, item);
	LvN_Str(&r, arrow);
	byte ok = Mode_Taken(&i, &r);
	Native_Free(&i);
	Native_Free(&r);
	return ok;
}

/* the item taken with its arrow item + "CheckArrow" */
static byte Cm_TakenArrow(const nativeVal_t *item) {
	nativeVal_t s, r;
	LvN_Str(&s, "CheckArrow");
	Native_SetNone(&r);
	byte ok = Native_Arith('+', item, &s, &r) && Mode_Taken(item, &r);
	Native_Free(&s);
	Native_Free(&r);
	return ok;
}

/* TakeMoney(ItemName, CharName): the player took the treasure */
static byte Mickus_TakeMoney(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	int by = Cm_UsrNameIs(&a[1]);
	return by >= 0 && (!by || (Cm_TakenS("MickusMoney", "MissionCheckArrow") &&
		LV_COM("Mission_Search&DestroyMickus_RemoteMessage8", "Traitorius", Mission_DestroyMickus, SetNextMission, NULL)));
}

/* SetNextMission(): on to Mission_ArrangeBankersCTF in GamblinDen */
static byte Mickus_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const places[] = { "MayorAtTownHall", "1", "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Press",
		"Journalist.Betty_Defaut", "GamblinDen", "Journalist.Berto_Defaut", "GamblinDen", NULL };
	(void)a;
	(void)out;
	if (!Mode_DelSaveVars("MissionState") || !Mode_DelSaveVars("NumTransports") ||
		!LvN_Next("Mission_ArrangeBankersCTF", "GamblinDen", "Mission_TalkToTraitorius3_MissionText1")) {
		return 0;
	}
	Mode_Saves(places);
	return Mode_WayPoint();
}

/* SScorer.Set(id, item, var, the global name of module) */
static byte Cm_ScorerG(const nativeVal_t *id, const char *item, const char *var, const char *module, const char *name) {
	nativeVal_t v;
	byte ok = Mode_G(module, name, &v) && LvN_ScorerSet(id, item, var, &v);
	Native_Free(&v);
	return ok;
}

/* MissionScorer(id): the scan text and bar, visible while a ship near the player is scanned */
static byte Mickus_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cur = Mode_V(&t), *e = Mode_V(&t), *usr = Mode_V(&t), *vis = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t);
	nativeVal_t *label = Mode_V(&t);
	(void)out;
	if (!Mode_G(DMK, "currentScannedShip", cur) || !Mode_EntityH(cur, e) || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int scanning = Native_True(usr) && Native_True(e) ? Cm_Scan(e, usr, "<") : 0;
	if (scanning < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(vis, scanning);
	LvN_Label("Mission_Search&DestroyMickus_Scorer_1", label);
	byte ok = LvN_TextX(0, x) && LvN_TextY(0, y) &&
		LvN_MissionText(&a[0], "ScanStatusText", "Text", x, y, "Horatio", "Shadow", label, "Left", vis) &&
		Mode_ScorerAddS(&a[0], "ScanRemainingTime", "Bar") && Mode_BarDefaults(&a[0], "ScanRemainingTime", NULL, NULL) &&
		Mode_ScorerSetI(&a[0], "ScanRemainingTime", "IsAutoUpdatable", 0) && Cm_ScorerG(&a[0], "ScanRemainingTime", "MaxValue", DMK, "MAX_SCANNING_TIME") &&
		(scanning || Cm_ScorerG(&a[0], "ScanRemainingTime", "Value", DMK, "MAX_SCANNING_TIME")) &&
		LvN_ScorerSet(&a[0], "ScanRemainingTime", "Visible", vis);
	return Mode_Done(&t, ok);
}

LV_FN(Mickus, GetFuncEnemiesProfileA, 0, Mickus_GetFuncEnemiesProfile_C)
LV_FN(Mickus, InitA, 1, Mickus_Init)
LV_FN(Mickus, CreateTransportsA, 1, Mickus_CreateTransports)
LV_FN(Mickus, InitMissionA, 0, Mickus_InitMission)
LV_FN(Mickus, StopMissionA, 0, Mickus_StopMission)
LV_FN(Mickus, OnMickusDamageA, 3, Mickus_OnMickusDamage)
LV_FN(Mickus, ScanShipA, 0, Mickus_ScanShip)
LV_FN(Mickus, CheckContinueScanningA, 0, Mickus_CheckContinueScanning)
LV_FN(Mickus, ResetScanA, 0, Mickus_ResetScan)
LV_FN(Mickus, ResetScan2A, 0, Mickus_ResetScan2)
LV_FN(Mickus, OnFinishScanA, 0, Mickus_OnFinishScan)
LV_FN(Mickus, MickusIdentifiedA, 0, Mickus_MickusIdentified)
LV_FN(Mickus, ReassignTargetA, 0, Mickus_ReassignTarget)
LV_FN(Mickus, TraitoriusFuncEnemiesComA, 0, Mickus_TraitoriusFuncEnemiesCom)
LV_FN(Mickus, CheckMickusVisibilityA, 0, Mickus_CheckMickusVisibility)
LV_FN(Mickus, CreateFuncEnemiesA, 1, Mickus_CreateFuncEnemies)
LV_FN(Mickus, OnFuncEnemiesDeathA, 2, Mickus_OnFuncEnemiesDeath)
LV_FN(Mickus, OnTransportsDeathA, 2, Mickus_OnTransportsDeath)
LV_FN(Mickus, DropMoneyA, 1, Mickus_DropMoney)
LV_FN(Mickus, TakeMoneyA, 2, Mickus_TakeMoney)
LV_FN(Mickus, SetNextMissionA, 0, Mickus_SetNextMission)
LV_FN(Mickus, MissionScorerA, 1, Mickus_MissionScorer)

PyMethodDef g_CommercialDestroyMickusMethods[] = {
	{ "GetFuncEnemiesProfile", Mickus_GetFuncEnemiesProfileA, 1, NULL },
	{ "InitStatus", Mickus_InitStatus, 1, NULL },
	{ "Init", Mickus_InitA, 1, NULL },
	{ "CreateTransports", Mickus_CreateTransportsA, 1, NULL },
	{ "InitMission", Mickus_InitMissionA, 1, NULL },
	{ "StopMission", Mickus_StopMissionA, 1, NULL },
	{ "OnMickusDamage", Mickus_OnMickusDamageA, 1, NULL },
	{ "ScanShip", Mickus_ScanShipA, 1, NULL },
	{ "CheckContinueScanning", Mickus_CheckContinueScanningA, 1, NULL },
	{ "ResetScan", Mickus_ResetScanA, 1, NULL },
	{ "ResetScan2", Mickus_ResetScan2A, 1, NULL },
	{ "OnFinishScan", Mickus_OnFinishScanA, 1, NULL },
	{ "MickusIdentified", Mickus_MickusIdentifiedA, 1, NULL },
	{ "ReassignTarget", Mickus_ReassignTargetA, 1, NULL },
	{ "TraitoriusFuncEnemiesCom", Mickus_TraitoriusFuncEnemiesComA, 1, NULL },
	{ "CheckMickusVisibility", Mickus_CheckMickusVisibilityA, 1, NULL },
	{ "CreateFuncEnemies", Mickus_CreateFuncEnemiesA, 1, NULL },
	{ "OnFuncEnemiesDeath", Mickus_OnFuncEnemiesDeathA, 1, NULL },
	{ "OnTransportsDeath", Mickus_OnTransportsDeathA, 1, NULL },
	{ "DropMoney", Mickus_DropMoneyA, 1, NULL },
	{ "TakeMoney", Mickus_TakeMoneyA, 1, NULL },
	{ "SetNextMission", Mickus_SetNextMissionA, 1, NULL },
	{ "MissionScorer", Mickus_MissionScorerA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ProtectGus: the bishop Gus and the lonely mercenary fight, Gus must win ---- */

static const char *const lonely_merc_profile[] = { "Ship=SLMerc", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"CadenceShoot=7", "ControlAim=10", "AngAttack=3.1415", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1",
	"ControlBoostItemAmmo=1", "FactorDamageGetAway=0.80", "FactorDamageItemLife=0.1", "FactorItemAmmo=0.1", "TimeMaxStrafe=10",
	"VelMaxStrafe=1.0", NULL };

static const char *const bishop_gus_profile[] = { "Ship=SBishop1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"CadenceShoot=7", "ControlAim=10", "AngAttack=3.1415", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1",
	"ControlBoostItemAmmo=1", "FactorDamageGetAway=0.80", "FactorDamageItemLife=0.1", "FactorItemAmmo=0.1", "TimeMaxStrafe=10",
	"VelMaxStrafe=1.0", NULL };

/* VehiclesNPC.AIProf[name] = the profile of the table */
static byte Pg_Profile(const char *const *vars, const char *name) {
	nativeVal_t pd, key;
	Native_SetNone(&pd);
	LvN_Str(&key, name);
	byte ok = LvN_Profile(vars, &pd) && Mode_ModDictPut("VehiclesNPC", "AIProf", &key, &pd);
	Native_Free(&pd);
	Native_Free(&key);
	return ok;
}

static byte Gus_CreateLonelyMercProfile(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pg_Profile(lonely_merc_profile, "LonelyMercNormal");
}

static byte Gus_CreateBishopGusProfile(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pg_Profile(bishop_gus_profile, "BishopGus");
}

static PyObject *Gus_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, PG, ac_ships_bertos);
}

/* Init(LevelPath): the fight (now or when the player boards a ship), or the phone cab after it */
static byte Gus_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	(void)a;
	(void)out;
	if (!LvN_SaveIs("MissionState", "ProtectGus")) {
		return PORT_LOWN(Mission_ProtectGus, SetPhoneCab, NULL, "");
	}
	Native_SetNone(&usr);
	int car = Mode_UsrHandle(&usr) ? Cm_UsrCar(&usr) : -1;
	Native_Free(&usr);
	if (car < 0 || (car ? !PORT_LOWN(Mission_ProtectGus, InitMission, NULL, "") :
		!Mode_Import("Parking") || !CM_HOOK("Parking", "SwitchVehicleMissionCallback", PG, "InitMission"))) {
		return 0;
	}
	return LvN_Com("Mission_ProtectGus_RemoteMessage1", "DeepThroat", NULL, NULL, NULL);
}

/* InitMission(): the scorer, both fighters with their profiles */
static byte Gus_InitMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Parking") && Mode_Import("Menu") && LvN_ModNone("Parking", "SwitchVehicleMissionCallback") &&
		LV_SCORERG(Mission_ProtectGus, MissionScorer) && LvN_Draw() && PORT_LOWN(Mission_ProtectGus, CreateLonelyMercProfile, NULL, "") &&
		PORT_LOWN(Mission_ProtectGus, CreateLonelyMerc, NULL, "") && PORT_LOWN(Mission_ProtectGus, CreateBishopGusProfile, NULL, "") &&
		PORT_LOWN(Mission_ProtectGus, CreateBishopGus, NULL, "") && PORT_LOWN(Mission_ProtectGus, AttackEachOther, NULL, "");
}

/* `x.Active` of a global handle: 1, 0, -1 */
static int Pg_GActive(const char *name) {
	nativeVal_t v;
	int r = LvN_GAttr(PG, name, "Active", &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* SAI.SetStateVehicle(2, a.Name, b.Name) of two global handles (the names read in this order) */
static byte Pg_Attack(const char *a, const char *b) {
	nativeVal_t an, bn;
	Native_SetNone(&bn);
	byte ok = LvN_GAttr(PG, a, "Name", &an) && LvN_GAttr(PG, b, "Name", &bn) && STATE("ivv", 2L, &an, &bn);
	Native_Free(&an);
	Native_Free(&bn);
	return ok;
}

/* AttackEachOther(): both there and active: they attack each other; one missing: try again in 2 s */
static byte Gus_AttackEachOther(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	int missing = LvN_GTrue(PG, "bishopGus");
	missing = missing < 0 ? -1 : !missing;
	if (missing == 0) {
		int t = LvN_GTrue(PG, "lonelyMerc");
		missing = t < 0 ? -1 : !t;
	}
	if (missing > 0) {
		int go = Mode_GCmpI(PG, "lonelyMercLifes", ">", 0);
		if (go > 0) {
			go = Mode_GCmpI(PG, "bishopGusLifes", ">", 0);
		}
		return go >= 0 && (!go || CM_LATER(2, PG, "AttackEachOther", NULL));
	}
	if (missing < 0) {
		return 0;
	}
	int act = Pg_GActive("lonelyMerc");
	if (act > 0) {
		act = Pg_GActive("bishopGus");
	}
	return act >= 0 && (!act || (Pg_Attack("bishopGus", "lonelyMerc") && Pg_Attack("lonelyMerc", "bishopGus")));
}

/* the global name = VehiclesNPC.MakeAI(profile, name, InitState = 0) with the callback, its attributes and the life */
static byte Pg_Fighter(const char *global, const char *profile, const char *name, nativeVal_t *h) {
	return Cm_MakeAI(profile, name, h) && Mode_Put(PG, global, NULL, h);
}

/* h.Life = life; h.MaxLife = life of a global life of the mission */
static byte Pg_Life(const nativeVal_t *h, const char *life) {
	nativeVal_t v;
	byte ok = Mode_G(PG, life, &v) && Mode_VSet(h, "Life", &v) && Mode_VSet(h, "MaxLife", &v);
	Native_Free(&v);
	return ok;
}

/* CreateLonelyMerc(): the lonely mercenary, red, no taunts */
static byte Gus_CreateLonelyMerc(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t h;
	(void)a;
	(void)out;
	Native_SetNone(&h);
	byte ok = Pg_Fighter("lonelyMerc", "LonelyMercNormal", "LonelyMerc", &h) && LvN_SetS(&h, "OnDeath", "Mission_ProtectGus.OnLonelyMercDeath") &&
		LvN_SetS(&h, "TgType", "red") && LvN_SetI(&h, "LaunchTaunts", 0) && Pg_Life(&h, "LONELY_MERC_LIFE");
	Native_Free(&h);
	return ok;
}

/* CreateBishopGus(): Gus, the target while the mercenary lives */
static byte Gus_CreateBishopGus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t h, name;
	(void)a;
	(void)out;
	Native_SetNone(&h);
	Native_SetNone(&name);
	byte ok = Pg_Fighter("bishopGus", "BishopGus", "BishopGus", &h) && LvN_SetS(&h, "OnDeath", "Mission_ProtectGus.OnBishopGusDeath") &&
		LvN_SetI(&h, "Money", -1) && Pg_Life(&h, "BISHOP_GUS_LIFE");
	int live = ok ? Mode_GCmpI(PG, "lonelyMercLifes", ">", 0) : -1;
	ok = live >= 0 && (!live || (LvN_Activate(&h, 1) && Mode_VGet(&h, "Name", &name) && Cm_Waypoint(&name)));
	Native_Free(&h);
	Native_Free(&name);
	return ok;
}

/* the fight is over: the mission state, no scorer, DeepThroat's message then the phone cab */
static byte Pg_Over(const char *msg) {
	SaveVar_Set("MissionState", "CallDeepThroat");
	return LvN_ScorerOff() && LvN_Draw() && LV_COM(msg, "DeepThroat", Mission_ProtectGus, PreCallDeepThroat, LvN_NoArgs());
}

/* the next round: the dead fighter again in 2 s, the attack 0.2 s later */
static byte Pg_Again(const char *create) {
	return LvN_Draw() && CM_LATER(2, PG, create, NULL) && CM_LATER(2.2, PG, "AttackEachOther", NULL);
}

/* `x and x.Active` of a global */
static int Pg_Active(const char *name) {
	int t = LvN_GTrue(PG, name);
	return t > 0 ? Pg_GActive(name) : t;
}

/* SAI.SetStateVehicle(3, x.Name, 0) of a global handle */
static byte Pg_Wander(const char *name) {
	nativeVal_t n;
	byte ok = LvN_GAttr(PG, name, "Name", &n) && STATE("ivi", 3L, &n, 0L);
	Native_Free(&n);
	return ok;
}

/* a fighter died: the global None, a life less in the scorer, Vehicles.OnDeath, the other one wanders */
static byte Pg_Died(const nativeVal_t *a, const char *global, const char *lifes, const char *item, const char *other) {
	nativeVal_t text;
	Native_SetNone(&text);
	if (!Mode_Import("Vehicles") || !LvN_PutNone(PG, global) || !Mode_GOpI(PG, lifes, '-', 1) || !Cm_GRepr(PG, lifes, &text)) {
		return 0;
	}
	byte ok = LvN_Set0(item, "Text", &text) && Cm_OnDeath(a);
	Native_Free(&text);
	if (!ok) {
		return 0;
	}
	int act = Pg_Active(other);
	return act >= 0 && (!act || Pg_Wander(other));
}

/* OnLonelyMercDeath(LooserName, WinnerName): a life of the mercenary less; none left: Gus has won */
static byte Gus_OnLonelyMercDeath(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!Pg_Died(a, "lonelyMerc", "lonelyMercLifes", "LonelyMercLifes", "bishopGus")) {
		return 0;
	}
	int over = LvN_GEqI(PG, "lonelyMercLifes", 0);
	if (over > 0) {
		int g = LvN_GTrue(PG, "bishopGus");
		return g >= 0 && (!g || LvN_GActivate(PG, "bishopGus", 0)) && Cm_WaypointS("") && Pg_Over("Mission_ProtectGus_RemoteMessage3");
	}
	return over == 0 && Pg_Again("CreateLonelyMerc");
}

/* OnBishopGusDeath(LooserName, WinnerName): a life of Gus less; none left: the mission goes on without him */
static byte Gus_OnBishopGusDeath(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t m;
	(void)out;
	if (!Mode_Import("Vehicles") || !LvN_PutNone(PG, "bishopGus") || !Mode_GOpI(PG, "bishopGusLifes", '-', 1)) {
		return 0;
	}
	nativeVal_t text;
	if (!Cm_GRepr(PG, "bishopGusLifes", &text)) {
		return 0;
	}
	byte ok = LvN_Set0("BishopGusLifes", "Text", &text) && Cm_OnDeath(a);
	Native_Free(&text);
	int eight = ok ? LvN_GEqI(PG, "bishopGusLifes", 8) : -1;
	if (eight < 0 || (eight && !LvN_Com("Mission_ProtectGus_RemoteMessage2", "DeepThroat", NULL, NULL, NULL))) {
		return 0;
	}
	int act = Pg_Active("lonelyMerc");
	if (act < 0 || (act && !Pg_Wander("lonelyMerc")) || !Cm_WaypointS("")) {
		return 0;
	}
	int over = LvN_GEqI(PG, "bishopGusLifes", 0);
	if (over > 0) {
		int l = LvN_GTrue(PG, "lonelyMerc");
		if (l < 0) {
			return 0;
		}
		if (l) {
			ok = Mode_G(PG, "lonelyMerc", &m) && LvN_SetS(&m, "TgType", "None") && LvN_SetS(&m, "EntityList", "Traffic") &&
				LvN_SetS(&m, "OnDeath", "Vehicles.OnDeath");
			Native_Free(&m);
			if (!ok) {
				return 0;
			}
		}
		return Pg_Over("Mission_ProtectGus_RemoteMessage4");
	}
	return over == 0 && Pg_Again("CreateBishopGus");
}

/* PreCallDeepThroat(): the description, a way point, the phone cab */
static byte Gus_PreCallDeepThroat(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Cm_Desc("Mission_TalkToDeepThroat3b_MissionText1") && PORT_LOWN(Mission_ProtectGus, SetPhoneCab, NULL, "");
}

/* the imports of SetPhoneCab */
static byte Cm_PhoneImports(void) {
	static const char *const mods[] = { "Vehicles", "Speech", "Alarm", "SaveGame", "PhoneCab", "MissionsFuncs", NULL };
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	return 1;
}

/* SetPhoneCab(): a free phone cab to call DeepThroat */
static byte Gus_SetPhoneCab(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Cm_PhoneImports() && Mode_PhoneCab("Mission_ProtectGus.UsePhoneCab", 1, NULL, NULL);
}

CM_PHONE(Gus_UsePhoneCab, PG, "Generics_StillPolice1_DeepThroat", "DeepThroat", &PORT_LREF(Mission_ProtectGus, SetNextMission),
	"Mission_TalkToDeepThroat3b_ComDeepThroat_NI_1Msg", NULL, NULL, 1, 0, 0)

/* SetNextMission(): the phone cab back to normal, on to Mission_IntoTheGDB */
static byte Gus_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const places[] = { "MayorAtTownHall", "0", "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Temple",
		"Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Temple", NULL };
	nativeVal_t pc;
	Native_SetNone(&pc);
	(void)a;
	(void)out;
	byte ok = LvN_PcDefault(PG) && Mode_G(PG, "pc", &pc) && LvN_SetI(&pc, "ArrowCanUse", 0);
	Native_Free(&pc);
	if (!ok || !LvN_GActivate(PG, "pc", 0) || !Mode_DelSaveVars("MissionState") ||
		!LvN_Next("Mission_IntoTheGDB", "GDB", "Mission_IntoTheGDB_MissionText1")) {
		return 0;
	}
	Mode_Saves(places);
	return Mode_WayPoint();
}

/* Scorer.MissionScorerText_Y + d + d2 */
static byte Cm_Y2(long d, long d2, nativeVal_t *out) {
	nativeVal_t y, k;
	Native_SetInt(&k, d2);
	Native_SetNone(out);
	byte ok = LvN_TextY(d, &y) && Native_Arith('+', &y, &k, out);
	Native_Free(&y);
	return ok;
}

/* MissionScorer(id): the lives of Gus and of the mercenary */
static byte Gus_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	static const struct {
		const char *item, *lifes, *sprite;
		long dy;
	} rows[] = { { "BishopGusLifes", "bishopGusLifes", "BackSpriteGus", 0 }, { "LonelyMercLifes", "lonelyMercLifes", "BackSpriteMerc", 20 } };
	static const char *const texts[][2] = { { "BishopGusText", "Mission_ProtectGus_Scorer2" }, { "LonelyMercText", "Mission_ProtectGus_Scorer1" } };
	(void)out;
	if (!Mode_Import("Scorer")) {
		return 0;
	}
	for (int i = 0; i < 2; i++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *n = Mode_V(&t), *x0 = Mode_V(&t), *y3 = Mode_V(&t);
		byte ok = LvN_TextX(15, x) && Cm_Y2(12, rows[i].dy, y) && Cm_GRepr(PG, rows[i].lifes, n) &&
			LvN_MissionText(&a[0], rows[i].item, "Text", x, y, "BigNumbers", "Shadow", n, "Center", NULL) && LvN_TextX(0, x0) &&
			Cm_Y2(3, rows[i].dy, y3) && Mode_MissionSprite(&a[0], rows[i].sprite, x0, y3, 128, 0, 0, 0);
		if (!Mode_Done(&t, ok)) {
			return 0;
		}
	}
	for (int i = 0; i < 2; i++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *text = Mode_V(&t);
		Mode_Lang(texts[i][1], text);
		byte ok = LvN_TextX(34, x) && LvN_TextY(i * 20, y) && LvN_MissionText(&a[0], texts[i][0], "Text", x, y, "Horatio", "Shadow", text,
			"Left", NULL);
		if (!Mode_Done(&t, ok)) {
			return 0;
		}
	}
	return 1;
}

LV_FN(Gus, CreateLonelyMercProfileA, 0, Gus_CreateLonelyMercProfile)
LV_FN(Gus, CreateBishopGusProfileA, 0, Gus_CreateBishopGusProfile)
LV_FN(Gus, InitA, 1, Gus_Init)
LV_FN(Gus, InitMissionA, 0, Gus_InitMission)
LV_FN(Gus, AttackEachOtherA, 0, Gus_AttackEachOther)
LV_FN(Gus, CreateLonelyMercA, 0, Gus_CreateLonelyMerc)
LV_FN(Gus, CreateBishopGusA, 0, Gus_CreateBishopGus)
LV_FN(Gus, OnLonelyMercDeathA, 2, Gus_OnLonelyMercDeath)
LV_FN(Gus, OnBishopGusDeathA, 2, Gus_OnBishopGusDeath)
LV_FN(Gus, PreCallDeepThroatA, 0, Gus_PreCallDeepThroat)
LV_FN(Gus, SetPhoneCabA, 0, Gus_SetPhoneCab)
LV_FN(Gus, UsePhoneCabA, 4, Gus_UsePhoneCab_C)
LV_FN(Gus, SetNextMissionA, 0, Gus_SetNextMission)
LV_FN(Gus, MissionScorerA, 1, Gus_MissionScorer)

PyMethodDef g_CommercialProtectGusMethods[] = {
	{ "CreateLonelyMercProfile", Gus_CreateLonelyMercProfileA, 1, NULL },
	{ "CreateBishopGusProfile", Gus_CreateBishopGusProfileA, 1, NULL },
	{ "InitStatus", Gus_InitStatus, 1, NULL },
	{ "Init", Gus_InitA, 1, NULL },
	{ "InitMission", Gus_InitMissionA, 1, NULL },
	{ "AttackEachOther", Gus_AttackEachOtherA, 1, NULL },
	{ "CreateLonelyMerc", Gus_CreateLonelyMercA, 1, NULL },
	{ "CreateBishopGus", Gus_CreateBishopGusA, 1, NULL },
	{ "OnLonelyMercDeath", Gus_OnLonelyMercDeathA, 1, NULL },
	{ "OnBishopGusDeath", Gus_OnBishopGusDeathA, 1, NULL },
	{ "PreCallDeepThroat", Gus_PreCallDeepThroatA, 1, NULL },
	{ "SetPhoneCab", Gus_SetPhoneCabA, 1, NULL },
	{ "UsePhoneCab", Gus_UsePhoneCabA, 1, NULL },
	{ "SetNextMission", Gus_SetNextMissionA, 1, NULL },
	{ "MissionScorer", Gus_MissionScorerA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyStupidus: the functionary Stupidus drops the amplifier ---- */

static const char *const stupidus_profile[] = { "Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=0",
	"UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25", "UsePercHook=20", "FactorHookMap=10", "CadenceShoot=9", "ControlAim=4",
	"AngAttack=3.14", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.25",
	"FactorDamageItemLife=0.75", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=40", "TimeMaxStrafe=2",
	"VelMaxStrafe=0.25", NULL };

CM_PROFILE(Stupidus_GetStupidusProfile, stupidus_profile)

static PyObject *Stupidus_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, DS, "[\"SMerc1\",\"SMerc2\",\"SBishop1\",\"SArchbishop1\"]");
}

/* Init(LevelPath): the head of evil Berto, the amplifier model, Stupidus */
static byte Stupidus_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	/* SScorer.AddModel through its ref: fc-r6b-cm4 records it */
	return PORT_CALL(SScorer, AddModel, NULL, "isssiii", 0L, "EvilBerto", "Models/Heads/HeadEvilBerto/HeadEvilBerto", "", 4L, 25L, 10L) &&
		Cm_PreloadG(DS, "AmplifierModel") && PORT_LOWN(Mission_DestroyStupidus, CreateStupidus, NULL, "");
}

/* CreateStupidus(): Stupidus at a free spawn point, wandering until he is shot */
static byte Stupidus_CreateStupidus(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *o1 = Mode_V(&t), *spw = Mode_V(&t), *name = Mode_V(&t), *pd = Mode_V(&t), *ship = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_Import("VehiclesNPC") || !Mode_Import("OutMap") || !Mode_Import("Traffic")) {
		return Mode_Done(&t, 0);
	}
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	byte ok = Mode_SpawnCar("FuncFighter", 0, o1, spw) && LvN_SetI(o1, "Money", -1) && LvN_SetS(o1, "TgType", "red") && Cm_SpawnAng(o1, spw) &&
		Mode_VGet(o1, "Name", name) && PORT_LOWN(Mission_DestroyStupidus, GetStupidusProfile, pd, "") && Mode_VGet(pd, "Ship", ship) &&
		PORT_CALL(VehiclesNPC, SetStdProperties, NULL, "vv", name, ship) && LvN_SetS(o1, "OnDeath", "Mission_DestroyStupidus.OnStupidusDeath") &&
		LvN_SetS(o1, "OnDamage", "Mission_DestroyStupidus.OnStupidusDamage") && PORT_CALL(VehiclesNPC, ModAIProff, NULL, "vv", o1, pd) &&
		STATE("ivi", 3L, name, 0L) && LvN_Life(o1, DS, "STUPIDUS_LIFE") && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", name);
	return Mode_Done(&t, ok);
}

/* the entity of LooserName drops the amplifier at its position */
static byte Stupidus_DropAt(const nativeVal_t *looser) {
	nativeVal_t e, pos;
	Native_SetNone(&e);
	Native_SetNone(&pos);
	byte ok = Mode_EntityH(looser, &e) && (!Native_True(&e) || (Mode_VGet(&e, "Pos", &pos) &&
		PORT_LOWN(Mission_DestroyStupidus, DropAmplifier, NULL, "v", &pos)));
	Native_Free(&e);
	Native_Free(&pos);
	return ok;
}

/* OnStupidusDeath(LooserName, WinnerName): he drops the amplifier */
static byte Stupidus_OnStupidusDeath(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_Import("Vehicles") && Cm_OnDeath(a) && Stupidus_DropAt(&a[0]);
}

/* OnStupidusDamage(LooserName, WinnerName, Damage): shot: he attacks the player */
static byte Stupidus_OnStupidusDamage(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, e, un;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&e);
	Native_SetNone(&un);
	byte ok = Mode_UsrHandle(&usr) && Mode_EntityH(&a[0], &e) && (!Native_True(&usr) || !Native_True(&e) ||
		(LvN_SetS(&e, "OnDamage", "") && Mode_VGet(&usr, "Name", &un) && STATE("ivv", 2L, &a[0], &un)));
	Native_Free(&usr);
	Native_Free(&e);
	Native_Free(&un);
	return ok;
}

static byte Stupidus_DropAmplifier(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_Drop(DS, &a[0], "Amplifier", "AmplifierModel", "AmplifierRadius", 1, "Mission_DestroyStupidus.TakeAmplifier", "TakeAmplifier",
		"MissionCheckArrow");
}

/* TakeAmplifier(ItemName, CharName): DeepThroat's message, Berto found 10 s later */
static byte Stupidus_TakeAmplifier(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	int by = Cm_UsrNameIs(&a[1]);
	return by >= 0 && (!by || (Cm_TakenS("Amplifier", "MissionCheckArrow") &&
		LvN_Com("Mission_DestroyStupidus_RemoteMessage1", "DeepThroat", NULL, NULL, NULL) && CM_LATER(10, DS, "BertoFound", NULL)));
}

static byte Stupidus_BertoFound(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LV_COM("Mission_DestroyStupidus_RemoteMessage2", "DeepThroat", Mission_DestroyStupidus, SetNextMission, NULL);
}

static byte Stupidus_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LvN_Next("Mission_BertoMassacre", "DownTown", "Mission_GoToDowntown_MissionText1") && Mode_WayPoint();
}

LV_FN(Stupidus, GetStupidusProfileA, 0, Stupidus_GetStupidusProfile_C)
LV_FN(Stupidus, InitA, 1, Stupidus_Init)
LV_FN(Stupidus, CreateStupidusA, 0, Stupidus_CreateStupidus)
LV_FN(Stupidus, OnStupidusDeathA, 2, Stupidus_OnStupidusDeath)
LV_FN(Stupidus, OnStupidusDamageA, 3, Stupidus_OnStupidusDamage)
LV_FN(Stupidus, DropAmplifierA, 1, Stupidus_DropAmplifier)
LV_FN(Stupidus, TakeAmplifierA, 2, Stupidus_TakeAmplifier)
LV_FN(Stupidus, BertoFoundA, 0, Stupidus_BertoFound)
LV_FN(Stupidus, SetNextMissionA, 0, Stupidus_SetNextMission)

PyMethodDef g_CommercialDestroyStupidusMethods[] = {
	{ "GetStupidusProfile", Stupidus_GetStupidusProfileA, 1, NULL },
	{ "InitStatus", Stupidus_InitStatus, 1, NULL },
	{ "Init", Stupidus_InitA, 1, NULL },
	{ "CreateStupidus", Stupidus_CreateStupidusA, 1, NULL },
	{ "OnStupidusDeath", Stupidus_OnStupidusDeathA, 1, NULL },
	{ "OnStupidusDamage", Stupidus_OnStupidusDamageA, 1, NULL },
	{ "DropAmplifier", Stupidus_DropAmplifierA, 1, NULL },
	{ "TakeAmplifier", Stupidus_TakeAmplifierA, 1, NULL },
	{ "BertoFound", Stupidus_BertoFoundA, 1, NULL },
	{ "SetNextMission", Stupidus_SetNextMissionA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_Win1stRace: a time race through the checkpoints of race 8 ---- */

static PyObject *Race_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, WR, ac_ships_bertos);
}

/* Init(LevelPath): the alarm hooks, the countdown resources, the race (unless the alarm is on) */
static byte Race_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_Import("RaceMaker") || !Mode_Import("RacerScorer") || !Mode_Import("Alarm") ||
		!CM_HOOK("Alarm", "TempAbortMissionCallback", WR, "OnAlarmOn") || !CM_HOOK("Alarm", "TempRestoreMissionCallback", WR, "OnAlarmOff") ||
		!PORT_CALL(RaceMaker, PreloadCnDnResources, NULL, "")) {
		return 0;
	}
	int alarm = Mode_AlarmOn();
	if (alarm > 0) {
		if (!PORT_LOWN(Mission_Win1stRace, OnAlarmOn, NULL, "")) {
			return 0;
		}
	} else if (alarm == 0) {
		byte first = LvN_SaveIs("MissionState", "FirstTime");
		if (!LvN_Com(first ? "Mission_Win1stRace_RemoteMessage1" : "Mission_Win1stRace_RemoteMessage3", first ? "DeepThroat" : "Trurl", NULL, NULL,
			NULL) || !PORT_LOWN(Mission_Win1stRace, InitTimeRace, NULL, "")) {
			return 0;
		}
	} else {
		return 0;
	}
	SaveVar_Set("MissionState", "");
	return 1;
}

/* RaceMaker.StartPos[i][j] into out */
static byte Wr_StartPos(const nativeVal_t *i, long j, nativeVal_t *out) {
	nativeVal_t sp, p;
	Native_SetNone(&p);
	Native_SetNone(out);
	byte ok = LvN_ModGet("RaceMaker", "StartPos", &sp) && Native_Subscript(&sp, i, &p) && LvN_Item(&p, j, out);
	Native_Free(&sp);
	Native_Free(&p);
	return ok;
}

/* InitTimeRace(): the start buoy of race 8 */
static byte Race_InitTimeRace(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pos = Mode_V(&t), *l = Mode_V(&t), *ang = Mode_V(&t), *name = Mode_V(&t);
	(void)a;
	(void)out;
	byte ok = Mode_Import("MapRace") && Mode_Import("RaceMaker") && PORT_LCALL(MapRace, EasyRaceCommercial8, NULL, "") &&
		Wr_StartPos(Mode_I(&t, 0), 0, pos) && LvN_CreateAt(Mode_S(&t, "RaceWaypoint"), pos, "Item", l) &&
		Mode_VModel(l, Mode_S(&t, "models/Misc/Wayp/Wayp.m3d")) && LvN_SetS(l, "OnTake", "Mission_Win1stRace.OnRaceStart") &&
		LvN_SetI(l, "OnlyUser", 1) && LvN_SetI(l, "RadarShow", 1) && LvN_SetI(l, "Radius", 2400) && Wr_StartPos(Mode_I(&t, 0), 1, ang) &&
		Mode_SetAng(l, ang) && Mode_Preload(Mode_S(&t, "models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d")) && Mode_VGet(l, "Name", name) &&
		Mode_Put(WR, "waypointName", NULL, name);
	Native_Free(name);
	ok = ok && Mode_G(WR, "waypointName", name) && Cm_Waypoint(name);
	return Mode_Done(&t, ok);
}

/* OnRaceStart(ItemName, VehicleName): the player at the buoy: the camera, the countdown, the start */
static byte Race_OnRaceStart(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t d;
	(void)out;
	if (!Mode_Import("RaceMaker")) {
		return 0;
	}
	int by = Cm_UsrNameIs(&a[1]);
	if (by <= 0) {
		return by == 0;
	}
	Native_SetNone(&d);
	byte ok = PORT_CALL(RaceMaker, StartCameraTravelling, &d, "vv", &a[0], &a[1]) && Mode_LaterV(&d, WR, "RaceGo", a, 2, NULL) &&
		PORT_CALL(RaceMaker, CreateRaceCountDown, NULL, "");
	Native_Free(&d);
	return ok;
}

/* RaceGo(ItemName, VehicleName): the race starts at the first checkpoint */
static byte Race_RaceGo(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e;
	(void)out;
	Native_SetNone(&e);
	byte ok = Mode_Import("RaceMaker") && PORT_CALL(RaceMaker, AbortCameraTravelling, NULL, "") &&
		PORT_CALL(RaceMaker, DestroyRaceCountDown, NULL, "") && Mode_EntityH(&a[0], &e) &&
		LvN_SetS(&e, "OnTake", "Mission_Win1stRace.OnCheckPoint") && PORT_LOWN(Mission_Win1stRace, OnCheckPoint, NULL, "vv", &a[0], &a[1]) &&
		Mode_GVarSetI("isActionMusic", 1);
	Native_Free(&e);
	return ok;
}

/* OnCheckPoint(ItemName, VehicleName): the buoy moves on and gives time, the last one wins */
static byte Race_OnCheckPoint(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rt = Mode_V(&t), *l = Mode_V(&t), *cur = Mode_V(&t), *max = Mode_V(&t), *tm = Mode_V(&t), *inc = Mode_V(&t);
	nativeVal_t *sum = Mode_V(&t), *next = Mode_V(&t), *sp = Mode_V(&t), *mod = Mode_V(&t), *len = Mode_V(&t), *i = Mode_V(&t);
	nativeVal_t *p = Mode_V(&t);
	(void)out;
	if (!Mode_Import("RaceMaker") || !Mode_Import("Sound") || !LvN_GAdd(WR, "currentWaypoints", 1, NULL)) {
		return Mode_Done(&t, 0);
	}
	int first = LvN_GEqI(WR, "currentWaypoints", 0);
	if (first > 0) {
		if (!Mode_G(WR, "raceTime", rt) || !Mode_GVarSet("TimeSpecialUseTimer", rt)) {
			return Mode_Done(&t, 0);
		}
	} else if (first < 0 || !Mode_Sound("WaySound", "WayVolume")) {
		return Mode_Done(&t, 0);
	}
	int last = Mode_EntityH(&a[0], l) && Mode_G(WR, "currentWaypoints", cur) && Mode_G(WR, "maxWaypoints", max) ? Mode_Cmp(cur, "==", max) : -1;
	if (last > 0) {
		if (!Mode_VDel(l) || !LvN_ScorerOff() || !PORT_LOWN(Mission_Win1stRace, PreSetNextMission, NULL, "")) {
			return Mode_Done(&t, 0);
		}
	} else if (last == 0) {
		if (!LV_SCORERG(Mission_Win1stRace, MissionScorer)) {
			return Mode_Done(&t, 0);
		}
		int more = Mode_GCmpI(WR, "currentWaypoints", ">", 0);
		if (more < 0 || (more && (!Native_GVarGet("TimeSpecialUseTimer", tm) || !Mode_G(WR, "timeIncrement", inc) ||
			!Native_Arith('+', tm, inc, sum) || !Mode_GVarSet("TimeSpecialUseTimer", sum)))) {
			return Mode_Done(&t, 0);
		}
		/* iStartPos = (iStartPos + 1) % len(RaceMaker.StartPos) */
		int n = Mode_G(WR, "iStartPos", next) && Native_Arith('+', next, Mode_I(&t, 1), sum) && LvN_ModGet("RaceMaker", "StartPos", sp) ?
			Native_Size(sp) : -1;
		if (n < 0) {
			return Mode_Done(&t, 0);
		}
		Native_SetInt(len, n);
		if (!Mode_Arith('%', sum, len, mod) || !Mode_Put(WR, "iStartPos", NULL, mod) || !Mode_G(WR, "iStartPos", i) || !Wr_StartPos(i, 0, p) ||
			!Mode_VSet(l, "Pos", p)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(i);
		Native_Free(p);
		if (!Mode_G(WR, "iStartPos", i) || !Wr_StartPos(i, 1, p) || !Mode_SetAng(l, p)) {
			return Mode_Done(&t, 0);
		}
	} else {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, LvN_Draw());
}

/* the race stops: no action music, no scorer */
static byte Wr_Stop(void) {
	return Mode_GVarSetI("isActionMusic", 0) && LvN_ScorerOff() && LvN_Draw();
}

/* the buoy goes, the race state back to its start */
static byte Wr_Reset(void) {
	nativeVal_t n, e;
	Native_SetNone(&e);
	byte ok = Mode_G(WR, "waypointName", &n) && Mode_EntityH(&n, &e) && (!Native_True(&e) || Mode_VDel(&e));
	Native_Free(&n);
	Native_Free(&e);
	return ok && LvN_PutI(WR, "currentWaypoints", -1) && LvN_PutS(WR, "waypointName", "") && LvN_PutI(WR, "iStartPos", 0);
}

/* OnAlarmOn(): the police stops the race */
static byte Race_OnAlarmOn(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Alarm") && Wr_Stop() && LvN_ModNone("Alarm", "TempAbortMissionCallback") &&
		LvN_Com("Mission_Win1stRace_RemoteMessage2", "Trurl", NULL, NULL, NULL) && Wr_Reset();
}

/* OnAlarmOff(): the race can start again after Trurl's message */
static byte Race_OnAlarmOff(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Alarm") && CM_HOOK("Alarm", "TempAbortMissionCallback", WR, "OnAlarmOn") &&
		LV_COM("Mission_Win1stRace_RemoteMessage3", "Trurl", Mission_Win1stRace, InitTimeRace, LvN_NoArgs());
}

/* TimeExpired(): out of time: again from the start */
static byte Race_TimeExpired(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Alarm") && Wr_Stop() && Wr_Reset() &&
		LV_COM("Mission_Win1stRace_RemoteMessage3", "Trurl", Mission_Win1stRace, InitTimeRace, LvN_NoArgs());
}

/* PreSetNextMission(): won: no alarm hooks, Trurl's message, then the next mission */
static byte Race_PreSetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Alarm") && Mode_GVarSetI("isActionMusic", 0) && LvN_ModNone("Alarm", "TempAbortMissionCallback") &&
		LvN_ModNone("Alarm", "TempRestoreMissionCallback") &&
		LV_COM("Mission_Win1stRace_RemoteMessage4", "Trurl", Mission_Win1stRace, SetNextMission, LvN_NoArgs());
}

static byte Race_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_DelSaveVars("MissionState") && LvN_Next("Mission_Win2ndRace", "Outskirts", "Mission_Win2ndRace_MissionText1") && Mode_WayPoint();
}

/* MissionScorer(id): the checkpoints passed of all, the label and the timer */
static byte Race_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *n = Mode_V(&t), *x2 = Mode_V(&t), *m = Mode_V(&t), *x5 = Mode_V(&t), *x35 = Mode_V(&t);
	nativeVal_t *x0 = Mode_V(&t), *y0 = Mode_V(&t), *label = Mode_V(&t), *l2 = Mode_V(&t), *area = Mode_V(&t), *ar = Mode_V(&t);
	nativeVal_t *ar1 = Mode_V(&t), *s1 = Mode_V(&t), *s2 = Mode_V(&t), *y12 = Mode_V(&t);
	const nativeVal_t *id = &a[0];
	(void)out;
	(void)ar1;
	if (!Mode_Import("Scorer") || !LvN_TextX(20, x) || !Cm_GRepr(WR, "currentWaypoints", n) ||
		!LvN_MissionText(id, "MissionBest", "Text", x, Mode_I(&t, 190), "BigNumbers", NULL, n, "Center", NULL) || !LvN_TextX(50, x2) ||
		!Cm_GRepr(WR, "maxWaypoints", m) || !LvN_MissionText(id, "MissionLimit", "Text", x2, Mode_I(&t, 190), "BigNumbers", NULL, m, "Center", NULL) ||
		!LvN_TextX(5, x5) || !Mode_MissionSprite(id, "MissionYou", x5, Mode_I(&t, 180), 128, 0, 0, 255) || !LvN_TextX(35, x35) ||
		!Mode_MissionSprite(id, "MissionSLimit", x35, Mode_I(&t, 180), 92, 0, 0, 0) || !LvN_TextX(0, x0) || !LvN_TextY(0, y0)) {
		return Mode_Done(&t, 0);
	}
	LvN_Label("Mission_Win1stRace_Scorer1", label);
	if (!LvN_MissionText(id, "RaceText", "Text", x0, y0, "Horatio", "Shadow", label, "Left", Mode_I(&t, 1))) {
		return Mode_Done(&t, 0);
	}
	/* width, height = SScorer.GetTextArea("Horatio", the label of DistractHumphrey) */
	LvN_Label("Mission_DistractHumphrey_Scorer1", l2);
	Native_Free(x0);
	if (!Mode_TextArea("Horatio", l2, area) || !Mode_Unpack(area, 2, ar) || !Mode_ScorerAddS(id, "RaceTimer", "Timer") || !LvN_TextX(0, x0) ||
		!Native_Arith('+', x0, ar, s1) || !Native_Arith('+', s1, Mode_I(&t, 30), s2) || !LvN_ScorerSet(id, "RaceTimer", "X", s2) ||
		!LvN_TextY(12, y12) || !LvN_ScorerSet(id, "RaceTimer", "Y", y12) || !LvN_ScorerSet(id, "RaceTimer", "Align", Mode_S(&t, "Left")) ||
		!LvN_ScorerSet(id, "RaceTimer", "TimerType", Mode_S(&t, "TimeSpecialUse")) ||
		!LvN_ScorerSet(id, "RaceTimer", "OnTimeExpired", Mode_S(&t, "Mission_Win1stRace.TimeExpired")) || !Mode_TextColour(id, "RaceTimer")) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_ScorerSetI(id, "RaceTimer", "Visible", 1));
}

LV_FN(Race, InitA, 1, Race_Init)
LV_FN(Race, InitTimeRaceA, 0, Race_InitTimeRace)
LV_FN(Race, OnRaceStartA, 2, Race_OnRaceStart)
LV_FN(Race, RaceGoA, 2, Race_RaceGo)
LV_FN(Race, OnCheckPointA, 2, Race_OnCheckPoint)
LV_FN(Race, OnAlarmOnA, 0, Race_OnAlarmOn)
LV_FN(Race, OnAlarmOffA, 0, Race_OnAlarmOff)
LV_FN(Race, TimeExpiredA, 0, Race_TimeExpired)
LV_FN(Race, PreSetNextMissionA, 0, Race_PreSetNextMission)
LV_FN(Race, SetNextMissionA, 0, Race_SetNextMission)
LV_FN(Race, MissionScorerA, 1, Race_MissionScorer)

PyMethodDef g_CommercialWin1stRaceMethods[] = {
	{ "InitStatus", Race_InitStatus, 1, NULL },
	{ "Init", Race_InitA, 1, NULL },
	{ "InitTimeRace", Race_InitTimeRaceA, 1, NULL },
	{ "OnRaceStart", Race_OnRaceStartA, 1, NULL },
	{ "RaceGo", Race_RaceGoA, 1, NULL },
	{ "OnCheckPoint", Race_OnCheckPointA, 1, NULL },
	{ "OnAlarmOn", Race_OnAlarmOnA, 1, NULL },
	{ "OnAlarmOff", Race_OnAlarmOffA, 1, NULL },
	{ "TimeExpired", Race_TimeExpiredA, 1, NULL },
	{ "PreSetNextMission", Race_PreSetNextMissionA, 1, NULL },
	{ "SetNextMission", Race_SetNextMissionA, 1, NULL },
	{ "MissionScorer", Race_MissionScorerA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyCommSats2: levels/LevelCommon.c ---- */

/* ---- Mission_FirstMeetDeepThroat: DeepThroat calls every minute until the player uses the phone cab ---- */

static PyObject *Deep_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, FM, ac_ships_bertos);
}

/* DeepThroat's call in 60 s: Speech.RemoteComunication(key, "DeepThroat", 1, then) under the name DeepThroatFirstMeet */
static byte Fm_Call(const char *key, const char *then) {
	nativeVal_t time, args[4], name;
	Native_SetFloat(&time, Mode_Time() + 60.0);
	/* the function of Speech: the scheduler takes its entry (Port_Scheduled) */
	PyObject *fn = Script_Attr("Speech", "RemoteComunication");
	if (!fn) {
		return 0;
	}
	if (!LvN_GObj(FM, then, &args[3])) {
		Py_DECREF(fn);
		return 0;
	}
	LvN_Str(&args[0], key);
	LvN_Str(&args[1], "DeepThroat");
	Native_SetInt(&args[2], 1);
	LvN_Str(&name, "DeepThroatFirstMeet");
	byte ok = Mode_Schedule(&time, fn, args, 4, &name);
	for (int i = 0; i < 4; i++) {
		Native_Free(&args[i]);
	}
	Native_Free(&name);
	Py_DECREF(fn);
	return ok;
}

/* Init(LevelPath): the boarding callbacks, the first call in a minute, the phone cab */
static byte Deep_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Parking") && CM_HOOK("Parking", "SwitchVehicleMissionCallback", FM, "RestoreDeepThroatMessages") &&
		CM_HOOK("Parking", "SwitchCharMissionCallback", FM, "CancelDeepThroatMessages") &&
		Fm_Call("Mission_FirstMeetDeepThroat_RemoteMessage3", "AfterDeepThroatMessageShown") &&
		PORT_LOWN(Mission_FirstMeetDeepThroat, SetPhoneCab, NULL, "");
}

static byte Deep_AfterDeepThroatMessageShown(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Fm_Call("Mission_FirstMeetDeepThroat_RemoteMessage4", "AfterDeepThroatMessageShown2");
}

static byte Deep_AfterDeepThroatMessageShown2(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Fm_Call("Mission_FirstMeetDeepThroat_RemoteMessage5", "AfterDeepThroatMessageShown2");
}

/* RestoreDeepThroatMessages(): back on a ship before the talk: the calls again */
static byte Deep_RestoreDeepThroatMessages(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	int talked = LvN_GTrue(FM, "talked");
	return talked >= 0 && (talked || Fm_Call("Mission_FirstMeetDeepThroat_RemoteMessage5", "AfterDeepThroatMessageShown2"));
}

/* CancelDeepThroatMessages(): on foot within 100 m of the phone cab: no more calls */
static byte Deep_CancelDeepThroatMessages(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, pc, d;
	(void)a;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&pc);
	Native_SetNone(&d);
	byte ok = Mode_UsrHandle(&usr);
	int near = ok && Native_True(&usr) ? (Mode_G(FM, "phoneCab", &pc) ? Native_True(&pc) : -1) : 0;
	if (near > 0) {
		near = Cm_Dist(&pc, &usr, 0, &d) ? Mode_CmpI(&d, "<", 10000) : -1;
	}
	ok = ok && near >= 0 && (!near || Mode_DelSchedS("DeepThroatFirstMeet"));
	Native_Free(&usr);
	Native_Free(&pc);
	Native_Free(&d);
	return ok;
}

static byte Deep_SetPhoneCab(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t pc;
	(void)a;
	(void)out;
	Native_SetNone(&pc);
	byte ok = Cm_PhoneImports() && Mode_PhoneCab("Mission_FirstMeetDeepThroat.UsePhoneCab", 1, NULL, &pc) && Mode_Put(FM, "phoneCab", NULL, &pc);
	Native_Free(&pc);
	return ok;
}

CM_PHONE(Deep_UsePhoneCab, FM, "Generics_StillPolice1_DeepThroat", "DeepThroat", &PORT_LREF(Mission_FirstMeetDeepThroat, SetNextMission),
	"Mission_FirstMeetDeepThroat_ComDeepthroat_NI_1Msg", "DeepThroatFirstMeet", "PhoneCab.DefUseAudioCom", 0, 0, 0)

/* SetNextMission(): talked; on to Mission_FicusBanker in Press, the crazy gambler calls 2 s later */
static byte Deep_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Press", NULL };
	(void)a;
	(void)out;
	if (!LvN_ModNone("Speech", "OnOutOfRange") || !LvN_PutI(FM, "talked", 1) || !Mode_Target("") || !Mode_WayPoint() ||
		!PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_FicusBanker", "Press")) {
		return 0;
	}
	Mode_Saves(places);
	return Mode_MMDesc("Mission_TalkToFicus_MissionText1", 1) && CM_LATER(2, FM, "CGCalls", NULL);
}

/* CGCalls(): the crazy gambler's message by the crazy deals done (not during the twelfth deal) */
static byte Deep_CGCalls(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const msgs[] = { "Mission_TalkToFicus_RemoteMessage1", "Mission_TalkToFicus_RemoteMessage2",
		"Mission_TalkToFicus_RemoteMessage3", "Mission_TalkToFicus_RemoteMessage4" };
	(void)a;
	(void)out;
	return Mode_CGCalls(msgs, 4);
}

LV_FN(Deep, InitA, 1, Deep_Init)
LV_FN(Deep, AfterDeepThroatMessageShownA, 0, Deep_AfterDeepThroatMessageShown)
LV_FN(Deep, AfterDeepThroatMessageShown2A, 0, Deep_AfterDeepThroatMessageShown2)
LV_FN(Deep, RestoreDeepThroatMessagesA, 0, Deep_RestoreDeepThroatMessages)
LV_FN(Deep, CancelDeepThroatMessagesA, 0, Deep_CancelDeepThroatMessages)
LV_FN(Deep, SetPhoneCabA, 0, Deep_SetPhoneCab)
LV_FN(Deep, UsePhoneCabA, 4, Deep_UsePhoneCab_C)
LV_FN(Deep, SetNextMissionA, 0, Deep_SetNextMission)
LV_FN(Deep, CGCallsA, 0, Deep_CGCalls)

PyMethodDef g_CommercialFirstMeetDeepThroatMethods[] = {
	{ "InitStatus", Deep_InitStatus, 1, NULL },
	{ "Init", Deep_InitA, 1, NULL },
	{ "AfterDeepThroatMessageShown", Deep_AfterDeepThroatMessageShownA, 1, NULL },
	{ "AfterDeepThroatMessageShown2", Deep_AfterDeepThroatMessageShown2A, 1, NULL },
	{ "RestoreDeepThroatMessages", Deep_RestoreDeepThroatMessagesA, 1, NULL },
	{ "CancelDeepThroatMessages", Deep_CancelDeepThroatMessagesA, 1, NULL },
	{ "SetPhoneCab", Deep_SetPhoneCabA, 1, NULL },
	{ "UsePhoneCab", Deep_UsePhoneCabA, 1, NULL },
	{ "SetNextMission", Deep_SetNextMissionA, 1, NULL },
	{ "CGCalls", Deep_CGCallsA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetDataPart3: waves of functionaries carry the data cubes ---- */

static const char *const functionaries_profile[] = { "Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=1",
	"UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10", "UsePercHook=5", "FactorHookMap=10", "CadenceShoot=2", "ControlAim=3",
	"AngAttack=2.5", "CounterAttack=0.8", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.60",
	"FactorDamageItemLife=0.15", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=10", "TimeMaxStrafe=8",
	"VelMaxStrafe=1.0", NULL };

CM_PROFILE(Data_GetFunctionariesEnemiesProfile, functionaries_profile)

static PyObject *Data_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, GD, ac_ships_bertos);
}

/* Init(LevelPath): the part of the mission of the save state: call the boss, get the data, call DeepThroat */
static byte Data_Init(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *state = Mode_V(&t), *usr = Mode_V(&t), *n = Mode_V(&t), *max = Mode_V(&t), *m1 = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_Import("Parking") || !Cm_PreloadG(GD, "DataCubeModel")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("MissionState", state);
	if (LvN_EqS(state, "CallBoss")) {
		return Mode_Done(&t, PORT_LOWN(Mission_GetDataPart3, CallBoss, NULL, ""));
	}
	if (LvN_EqS(state, "CallDeepThroat")) {
		return Mode_Done(&t, PORT_LOWN(Mission_GetDataPart3, CallDeepThroat, NULL, ""));
	}
	if (!LvN_EqS(state, "GetData")) {
		return Mode_Done(&t, 1);
	}
	int car = Mode_UsrHandle(usr) ? Cm_UsrCar(usr) : -1;
	if (car < 0 || (car && !PORT_LOWN(Mission_GetDataPart3, InitMission, NULL, "")) || !Mode_SaveInt("NumData", n) ||
		!Mode_G(GD, "MAX_DATA_UNITS", max)) {
		return Mode_Done(&t, 0);
	}
	int over = Mode_Cmp(n, ">=", max);
	if (over < 0) {
		return Mode_Done(&t, 0);
	}
	if (over) {
		Native_Free(max);
		if (!Mode_G(GD, "MAX_DATA_UNITS", max) || !Native_Arith('-', max, Mode_I(&t, 1), m1)) {
			return Mode_Done(&t, 0);
		}
		LvN_SaveRepr("NumData", m1);
	}
	return Mode_Done(&t, CM_HOOK("Parking", "SwitchVehicleMissionCallback", GD, "InitMission") &&
		CM_HOOK("Parking", "SwitchCharMissionCallback", GD, "StopMission") && PORT_LOWN(Mission_GetDataPart3, GetData, NULL, ""));
}

/* usr.Target = funcWithDataName when the functionary with the data is there and the player has a ship */
static byte Gd_Target(const nativeVal_t *usr, const nativeVal_t *e) {
	int t = Native_True(e) ? Cm_UsrCar(usr) : 0;
	return t >= 0 && (!t || LvN_SetG(usr, "Target", GD, "funcWithDataName"));
}

/* the functionaries of Funcs that flee or idle flee from the player (the script never moves name on: the state of the
 * first in the list is asked for every one) */
static byte Gd_Flee(const nativeVal_t *usr) {
	nativeVal_t name, e;
	Native_SetNone(&e);
	byte ok = Mode_ListGet("Funcs", "First", &name) && Mode_EntityH(&name, &e);
	while (ok && Native_True(&e)) {
		nativeVal_t state, un, next;
		Native_SetNone(&state);
		Native_SetNone(&un);
		Native_SetNone(&next);
		ok = Mode_AIState(&name, &state);
		int flee = ok ? Mode_CmpI(&state, "==", 5) : -1;
		if (flee == 0) {
			flee = Mode_CmpI(&state, "==", 0);
		}
		ok = flee >= 0 && (!flee || (Mode_VGet(usr, "Name", &un) && STATE("ivvi", 5L, &name, &un, 100000L))) && Mode_VGet(&e, "NextInList", &next);
		Native_Free(&e);
		ok = ok && Mode_EntityH(&next, &e);
		Native_Free(&state);
		Native_Free(&un);
		Native_Free(&next);
	}
	Native_Free(&name);
	Native_Free(&e);
	return ok;
}

/* InitMission(): the scorer, the functionary with the data as the target, the fleeing ones flee from the player */
static byte Data_InitMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t fn, e, usr;
	(void)a;
	(void)out;
	if (!LV_SCORERG(Mission_GetDataPart3, MissionScorer) || !LvN_Draw()) {
		return 0;
	}
	Native_SetNone(&e);
	Native_SetNone(&usr);
	byte ok = Mode_G(GD, "funcWithDataName", &fn) && Mode_EntityH(&fn, &e) && Mode_UsrHandle(&usr);
	if (ok && Native_True(&usr)) {
		ok = Gd_Target(&usr, &e);
		/* try: ... except: pass */
		if (ok && !Gd_Flee(&usr)) {
			PyErr_Clear();
		}
	}
	Native_Free(&fn);
	Native_Free(&e);
	Native_Free(&usr);
	return ok;
}

static byte Data_StopMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LvN_ScorerOff();
}

/* CreateWave(n): up to three functionaries, the first with the data, the others its escort */
static byte Data_CreateWave(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *n = Mode_V(&t), *dataPos = Mode_V(&t), *e = Mode_V(&t), *fwd = Mode_V(&t);
	long count;
	(void)out;
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	int few = Mode_CmpI(&a[0], "<", 1);
	if (few) {
		return Mode_Done(&t, few > 0 ? LvN_RaiseText("N\372mero inv\341lido de funcionarios en la oleada") : 0);
	}
	int many = Mode_CmpI(&a[0], ">", 3);
	if (many < 0) {
		return Mode_Done(&t, 0);
	}
	if (many) {
		Native_SetInt(n, 3);
	} else {
		Native_Copy(n, &a[0]);
	}
	if (!LvN_PutS(GD, "funcWithDataName", "") || !Mode_Range(n, &count)) {
		return Mode_Done(&t, 0);
	}
	for (long i = 0; i < count; i++) {
		nativeVal_t fn, name;
		Native_SetNone(&fn);
		Native_SetNone(&name);
		byte ok = PORT_LOWN(Mission_GetDataPart3, CreateFunctionary, &fn, "") && LvN_SetS(&fn, "EntityList", "Funcs") &&
			Mode_AppendName(GD, "funcsInWave", &fn);
		if (ok && i == 0) {
			Native_Free(dataPos);
			ok = Mode_VGet(&fn, "Pos", dataPos) && Mode_VGet(&fn, "Name", &name) && Mode_Put(GD, "funcWithDataName", NULL, &name);
			Native_Free(&name);
			ok = ok && LvN_TargetName(&fn) && Mode_VGet(&fn, "Name", &name) && STATE("ivi", 3L, &name, 0L);
		} else if (ok) {
			ok = Mode_VSet(&fn, "Pos", dataPos) && Mode_VGet(&fn, "Name", &name) && Mode_G(GD, "funcWithDataName", fwd) &&
				STATE("ivv", 7L, &name, fwd);
			Native_Free(fwd);
		}
		Native_Free(&fn);
		Native_Free(&name);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_G(GD, "funcWithDataName", fwd) && Mode_EntityH(fwd, e) && Gd_Target(usr, e));
}

/* CreateFunctionary(): a functionary fighter at a free spawn point */
static byte Data_CreateFunctionary(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t spw, pd;
	(void)a;
	Native_SetNone(&spw);
	Native_SetNone(&pd);
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_Import("Traffic") && Mode_SpawnCar("FuncFighter", 0, out, &spw) &&
		LvN_SetI(out, "Money", -1) && LvN_SetS(out, "TgType", "red") && Cm_SpawnAng(out, &spw) &&
		PORT_LOWN(Mission_GetDataPart3, GetFunctionariesEnemiesProfile, &pd, "") && LvN_StdAI(out, &pd) && LvN_Life(out, GD, "FUNC_LIFE");
	Native_Free(&spw);
	Native_Free(&pd);
	return ok;
}

/* SAI.SetStateVehicle(1, name, droppedDataPos[0], [1], [2], DataCubeRadius, 1): to the dropped data */
static byte Gd_ToData(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *p = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *r = Mode_V(&t);
	byte ok = Mode_G(GD, "droppedDataPos", p) && LvN_Item(p, 0, x);
	Native_Free(p);
	ok = ok && Mode_G(GD, "droppedDataPos", p) && LvN_Item(p, 1, y);
	Native_Free(p);
	ok = ok && Mode_G(GD, "droppedDataPos", p) && LvN_Item(p, 2, z) && Mode_G(GD, "DataCubeRadius", r) && STATE("ivvvvvi", 1L, name, x, y, z, r, 1L);
	return Mode_Done(&t, ok);
}

/* ReinsertFunctionaryInWave(): a new escort; it fetches dropped data nobody goes for */
static byte Data_ReinsertFunctionaryInWave(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *lst = Mode_V(&t), *dataName = Mode_V(&t), *fn = Mode_V(&t), *st = Mode_V(&t), *un = Mode_V(&t);
	nativeVal_t *fname = Mode_V(&t);
	int withData = 0, toData = 0;
	(void)a;
	(void)out;
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	LvN_Str(dataName, "");
	if (!Mode_G(GD, "funcsInWave", lst)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(lst);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name, e, s;
		Native_SetNone(&name);
		Native_SetNone(&e);
		Native_SetNone(&s);
		ok = Native_SeqItem(lst, i, &name) && Mode_EntityH(&name, &e);
		if (ok && Native_True(&e)) {
			int radar = Cm_AttrEqI(&e, "RadarShow", 1);
			if (radar > 0) {
				withData = 1;
				Native_Free(dataName);
				Native_Copy(dataName, &name);
			} else if (radar == 0) {
				ok = Mode_AIState(&name, &s);
				if (ok && LvN_EqI(&s, 5)) {
					toData = 1;
				}
			} else {
				ok = 0;
			}
		}
		Native_Free(&name);
		Native_Free(&e);
		Native_Free(&s);
	}
	if (!ok || !PORT_LOWN(Mission_GetDataPart3, CreateFunctionary, fn, "") || !LvN_SetS(fn, "EntityList", "Enemies") ||
		!Mode_AppendName(GD, "funcsInWave", fn)) {
		return Mode_Done(&t, 0);
	}
	if (withData) {
		return Mode_Done(&t, Mode_AIState(dataName, st) && (!LvN_EqI(st, 2) || (Mode_VGet(usr, "Name", un) &&
			STATE("ivvi", 5L, dataName, un, 100000L))));
	}
	int go = LvN_GTrue(GD, "droppedDataPos");
	if (go > 0) {
		go = !toData;
	}
	return Mode_Done(&t, go >= 0 && (!go || (LvN_SetS(fn, "EntityList", "Funcs") && Mode_VGet(fn, "Name", fname) && Gd_ToData(fname))));
}

/* the escort funcsInWave[0] goes for the dropped data */
static byte Gd_Fetch(void) {
	nativeVal_t lst, n, e;
	Native_SetNone(&n);
	Native_SetNone(&e);
	byte ok = Mode_G(GD, "funcsInWave", &lst) && LvN_Item(&lst, 0, &n) && Mode_EntityH(&n, &e);
	Native_Free(&lst);
	Native_Free(&n);
	if (ok && Native_True(&e)) {
		ok = LvN_SetS(&e, "EntityList", "Funcs") && Mode_G(GD, "funcsInWave", &lst) && LvN_Item(&lst, 0, &n) && Gd_ToData(&n);
		Native_Free(&lst);
		Native_Free(&n);
	}
	Native_Free(&e);
	return ok;
}

/* the global funcsInWave[0] into out */
static byte Gd_First(nativeVal_t *out) {
	nativeVal_t lst;
	byte ok = Mode_G(GD, "funcsInWave", &lst) && LvN_Item(&lst, 0, out);
	Native_Free(&lst);
	return ok;
}

/* OnFunctionaryDeath(LooserName, WinnerName): the one with the data drops it; an escort comes back later */
static byte Data_OnFunctionaryDeath(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *lst = Mode_V(&t), *pos = Mode_V(&t), *delay = Mode_V(&t), *f0 = Mode_V(&t), *usr = Mode_V(&t);
	nativeVal_t *un = Mode_V(&t);
	(void)out;
	int in = Mode_InG(&a[0], GD, "funcsInWave");
	if (in <= 0) {
		return Mode_Done(&t, in == 0);
	}
	if (!Mode_Remove(GD, "funcsInWave", &a[0]) || !Mode_EntityH(&a[0], e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	int len = Mode_G(GD, "funcsInWave", lst) ? Native_Size(lst) : -1;
	int data = len >= 0 ? Cm_AttrEqI(e, "RadarShow", 1) : -1;
	if (data > 0) {
		if (!Mode_VGet(e, "Pos", pos) || !Mode_Put(GD, "droppedDataPos", NULL, pos)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(pos);
		return Mode_Done(&t, Mode_G(GD, "droppedDataPos", pos) && PORT_LOWN(Mission_GetDataPart3, DropData, NULL, "v", pos) &&
			(len <= 0 || Gd_Fetch()));
	}
	if (data < 0 || !Mode_G(GD, "timeToReinsertEnemy", delay) ||
		!Mode_LaterV(delay, GD, "ReinsertFunctionaryInWave", NULL, 0, "ReinsertFunctionaryInWave")) {
		return Mode_Done(&t, 0);
	}
	if (len != 1) {
		return Mode_Done(&t, 1);
	}
	Native_Free(e);
	if (!Gd_First(f0) || !Mode_EntityH(f0, e) || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int alone = Native_True(usr) && Native_True(e) ? Cm_AttrEqI(e, "RadarShow", 1) : 0;
	if (alone <= 0) {
		return Mode_Done(&t, alone == 0);
	}
	Native_Free(f0);
	return Mode_Done(&t, Gd_First(f0) && Mode_VGet(usr, "Name", un) && STATE("ivv", 2L, f0, un));
}

/* the functionaries of the wave (but skip) join the enemies; onlyEscort: only those without the data */
static byte Gd_Attack(const nativeVal_t *skip, int onlyEscort) {
	nativeVal_t lst;
	if (!Mode_G(GD, "funcsInWave", &lst)) {
		return 0;
	}
	int n = Native_ForCount(&lst);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name, fn;
		Native_SetNone(&name);
		Native_SetNone(&fn);
		int go = Native_SeqItem(&lst, i, &name) ? (skip ? Mode_Cmp(&name, "!=", skip) : 1) : -1;
		if (go > 0) {
			go = Mode_EntityH(&name, &fn) ? Native_True(&fn) : -1;
			if (go > 0 && onlyEscort) {
				go = Cm_AttrEqI(&fn, "RadarShow", 0);
			}
			if (go > 0) {
				go = LvN_SetS(&fn, "EntityList", "Enemies") ? 1 : -1;
			}
		}
		ok = go >= 0;
		Native_Free(&name);
		Native_Free(&fn);
	}
	Native_Free(&lst);
	return ok;
}

/* OnFunctionaryDamage(LooserName, WinnerName, Damage): the first hit: the one with the data attacks (first wave) or
 * flees with the escort attacking, a hit escort makes the escort attack */
static byte Data_OnFunctionaryDamage(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *st = Mode_V(&t), *n = Mode_V(&t), *usr = Mode_V(&t), *un = Mode_V(&t);
	(void)out;
	if (!Mode_EntityH(&a[0], e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_AIState(&a[0], st)) {
		return Mode_Done(&t, 0);
	}
	int other = Mode_CmpI(st, "!=", 3);
	if (other > 0) {
		other = Mode_CmpI(st, "!=", 7);
	}
	if (other) {
		return Mode_Done(&t, other > 0);
	}
	int data = Cm_AttrEqI(e, "RadarShow", 1);
	if (data > 0) {
		int first = Mode_SaveInt("NumData", n) ? Mode_CmpI(n, "==", 0) : -1;
		if (first > 0) {
			return Mode_Done(&t, LvN_SetS(e, "EntityList", "Enemies"));
		}
		if (first < 0) {
			return Mode_Done(&t, 0);
		}
		int u = LvN_Usr(usr);
		return Mode_Done(&t, u >= 0 && (!u || (Mode_VGet(usr, "Name", un) && STATE("ivvi", 5L, &a[0], un, 100000L))) && Gd_Attack(&a[0], 0));
	}
	return Mode_Done(&t, data == 0 && Gd_Attack(NULL, 1));
}

static byte Data_DropData(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_Drop(GD, &a[0], "DroppedData", "DataCubeModel", "DataCubeRadius", 0, "Mission_GetDataPart3.TakeData", "TakeDataCube", NULL);
}

/* CheckAITakesData(name): checked a second later, in case it runs before TakeData */
static byte Data_CheckAITakesData(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_Later(1, GD, "CheckAI", &a[0], 1, NULL);
}

/* CheckAI(name): an escort that reached the data without taking it attacks the player */
static byte Data_CheckAI(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e, usr, en, un;
	(void)out;
	Native_SetNone(&e);
	Native_SetNone(&usr);
	Native_SetNone(&en);
	Native_SetNone(&un);
	int go = Mode_EntityH(&a[0], &e) ? Native_True(&e) : -1;
	if (go > 0) {
		go = Cm_AttrEqS(&e, "EntityList", "Funcs");
	}
	if (go > 0) {
		go = Cm_AttrEqI(&e, "RadarShow", 0);
	}
	byte ok = go >= 0;
	if (go > 0) {
		int u = LvN_Usr(&usr);
		if (u == 0) {
			ok = Mode_VGet(&e, "Name", &en) && STATE("ivi", 3L, &en, 1L);
		} else if (u > 0) {
			ok = LvN_SetS(&e, "EntityList", "Enemies") && Mode_VGet(&e, "Name", &en) && Mode_VGet(&usr, "Name", &un) && STATE("ivv", 2L, &en, &un);
		} else {
			ok = 0;
		}
	}
	Native_Free(&e);
	Native_Free(&usr);
	Native_Free(&en);
	Native_Free(&un);
	return ok;
}

/* the functionaries of the wave join list with state (2 attacks the player, 3 wanders), then the wave is empty */
static byte Gd_EndWave(const char *list, int state, const nativeVal_t *usr) {
	nativeVal_t lst;
	if (!Mode_G(GD, "funcsInWave", &lst)) {
		return 0;
	}
	int n = Native_ForCount(&lst);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name, e, un;
		Native_SetNone(&name);
		Native_SetNone(&e);
		Native_SetNone(&un);
		ok = Native_SeqItem(&lst, i, &name) && Mode_EntityH(&name, &e) && (!Native_True(&e) || (LvN_SetS(&e, "EntityList", list) &&
			(state == 3 ? STATE("ivi", 3L, &name, 1L) : (Mode_VGet(usr, "Name", &un) && STATE("ivv", 2L, &name, &un)))));
		Native_Free(&name);
		Native_Free(&e);
		Native_Free(&un);
	}
	Native_Free(&lst);
	return ok && Cm_GEmpty(GD, "funcsInWave", 1);
}

/* TakeData(ItemName, CharName): the player takes a cube (the next wave, or all eight), or an escort takes it and flees */
static byte Data_TakeData(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *n = Mode_V(&t), *max = Mode_V(&t), *n1 = Mode_V(&t), *e = Mode_V(&t);
	nativeVal_t *arrow = Mode_V(&t);
	(void)out;
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	int player = Mode_VGet(usr, "Name", un) ? Mode_Cmp(un, "==", &a[1]) : -1;
	if (player > 0) {
		if (!LvN_PutNone(GD, "droppedDataPos") || !Cm_TakenArrow(&a[0]) || !Cm_SaveAdd("NumData", 1, n) || !Mode_DelSchedS("ReinsertFunctionaryInWave") ||
			!Mode_G(GD, "MAX_DATA_UNITS", max)) {
			return Mode_Done(&t, 0);
		}
		int all = Mode_Cmp(n, ">=", max);
		if (all > 0) {
			return Mode_Done(&t, LvN_ModNone("Parking", "SwitchVehicleMissionCallback") && LvN_ModNone("Parking", "SwitchCharMissionCallback") &&
				LvN_ScorerOff() && LvN_Draw() && Mode_GVarSetS("OnItemTakenAI", "") &&
				LV_COM("Mission_GetDataPart3_RemoteMessage1", "Boss", Mission_GetDataPart3, PreCallDeepThroat, LvN_NoArgs()) &&
				Gd_EndWave("Traffic", 3, usr));
		}
		return Mode_Done(&t, all == 0 && LvN_Com("Mission_GetDataPart3_RemoteMessage3", "Boss", NULL, NULL, NULL) &&
			LvN_Set0("DataStatus", "Value", n) && LvN_Draw() && Gd_EndWave("Enemies", 2, usr) && Native_Arith('+', n, Mode_I(&t, 1), n1) &&
			PORT_LOWN(Mission_GetDataPart3, CreateWave, NULL, "v", n1));
	}
	if (player < 0) {
		return Mode_Done(&t, 0);
	}
	int in = Mode_InG(&a[1], GD, "funcsInWave");
	if (in <= 0) {
		return Mode_Done(&t, in == 0);
	}
	byte ok = LvN_PutNone(GD, "droppedDataPos") && Mode_EntityH(&a[0], e) && Mode_VDel(e) && Native_Arith('+', &a[0], Mode_S(&t, "CheckArrow"), arrow);
	Native_Free(e);
	ok = ok && Mode_EntityH(arrow, e) && Mode_VDel(e);
	Native_Free(e);
	ok = ok && Mode_EntityH(&a[1], e);
	if (ok && Native_True(e)) {
		Native_Free(un);
		ok = Mode_Put(GD, "funcWithDataName", NULL, &a[1]) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &a[1]) &&
			LvN_SetS(e, "EntityList", "Funcs") && Mode_VGet(usr, "Name", un) && STATE("ivvi", 5L, &a[1], un, 100000L);
	}
	return Mode_Done(&t, ok);
}

/* CallBoss(), CallDeepThroat(): a free phone cab (the global pc) for the call */
static byte Gd_Call(const char *use) {
	nativeVal_t pc;
	Native_SetNone(&pc);
	byte ok = Mode_PhoneCab(use, 1, NULL, &pc) && Mode_Put(GD, "pc", NULL, &pc);
	Native_Free(&pc);
	return ok;
}

static byte Data_CallBoss(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Gd_Call("Mission_GetDataPart3.UsePhoneCab");
}

CM_PHONE(Data_UsePhoneCab, GD, "Generics_StillPolice1_Boss", "Boss", &PORT_LREF(Mission_GetDataPart3, PreGetData),
	"Mission_TalkToBoss3_ComBoss_NI_1Msg", NULL, NULL, 0, 1, 0)

/* PreGetData(): after the boss: the boarding callbacks, the save state, the first wave */
static byte Data_PreGetData(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_Import("Parking") || !CM_HOOK("Parking", "SwitchVehicleMissionCallback", GD, "InitMission") ||
		!CM_HOOK("Parking", "SwitchCharMissionCallback", GD, "StopMission") || !LvN_PcDefault(GD)) {
		return 0;
	}
	SaveVar_Set("MissionState", "GetData");
	return Cm_Desc("Mission_GetDataPart3_MissionText1") && PORT_LOWN(Mission_GetDataPart3, GetData, NULL, "") &&
		PORT_LOWN(Mission_GetDataPart3, InitMission, NULL, "");
}

/* GetData(): the lists of the functionaries with their callbacks, the wave of the data count */
static byte Data_GetData(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t n, n1, one;
	(void)a;
	(void)out;
	Cm_NewList("Funcs");
	if (!Mode_ListSet("Funcs", "OnDeath", "Mission_GetDataPart3.OnFunctionaryDeath") ||
		!Mode_ListSet("Funcs", "OnDamage", "Mission_GetDataPart3.OnFunctionaryDamage") ||
		!Mode_ListSet("Enemies", "OnDeath", "Mission_GetDataPart3.OnFunctionaryDeath") ||
		!Mode_ListSet("Enemies", "OnDamage", "Mission_GetDataPart3.OnFunctionaryDamage") ||
		!Mode_GVarSetS("OnItemTakenAI", "Mission_GetDataPart3.CheckAITakesData")) {
		return 0;
	}
	Native_SetInt(&one, 1);
	Native_SetNone(&n1);
	byte ok = Mode_SaveInt("NumData", &n) && Native_Arith('+', &n, &one, &n1) && PORT_LOWN(Mission_GetDataPart3, CreateWave, NULL, "v", &n1);
	Native_Free(&n);
	Native_Free(&n1);
	return ok;
}

static byte Data_PreCallDeepThroat(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LV_COM("Mission_GetDataPart3_RemoteMessage2", "DeepThroat", Mission_GetDataPart3, PreCallDeepThroat2, LvN_NoArgs());
}

static byte Data_PreCallDeepThroat2(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("MissionState", "CallDeepThroat");
	return Cm_Desc("Mission_TalkToDeepThroat8_MissionText1") && PORT_LOWN(Mission_GetDataPart3, CallDeepThroat, NULL, "");
}

static byte Data_CallDeepThroat(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Gd_Call("Mission_GetDataPart3.UsePhoneCab2");
}

CM_PHONE(Data_UsePhoneCab2, GD, "Generics_StillPolice1_DeepThroat", "DeepThroat", &PORT_LREF(Mission_GetDataPart3, SetNextMission),
	"Mission_TalkToDeepThroat8_ComDeepThroat_NI_1Msg", NULL, NULL, 1, 1, 0)

/* SetNextMission(): on to Mission_DistractHumphrey in DownTown */
static byte Data_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const places[] = { "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "NoPlace", "Journalist.Betty_Defaut",
		"NoPlace", "Journalist.Berto_Defaut", "Press", NULL };
	(void)a;
	(void)out;
	if (!LvN_PcDefault(GD) || !Mode_DelSaveVars("NumData") || !Mode_DelSaveVars("MissionState") ||
		!LvN_Next("Mission_DistractHumphrey", "DownTown", "Mission_TalkToHumphrey_MissionText1")) {
		return 0;
	}
	Mode_Saves(places);
	return Mode_WayPoint();
}

/* MissionScorer(id): the data cubes got */
static byte Data_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t x, y, label, n;
	(void)out;
	Native_SetNone(&x);
	Native_SetNone(&y);
	Native_SetNone(&n);
	LvN_Label("Mission_GetDataPart3_Scorer_1", &label);
	byte ok = LvN_TextX(0, &x) && LvN_TextY(0, &y) &&
		LvN_MissionText(&a[0], "DataStatusText", "Text", &x, &y, "Horatio", "Shadow", &label, "Left", NULL) && Mode_ScorerAddS(&a[0], "DataStatus", "Bar") &&
		Mode_BarDefaults(&a[0], "DataStatus", NULL, NULL) && Mode_ScorerSetI(&a[0], "DataStatus", "IsAutoUpdatable", 0) &&
		Cm_ScorerG(&a[0], "DataStatus", "MaxValue", GD, "MAX_DATA_UNITS") && Mode_SaveInt("NumData", &n) && LvN_ScorerSet(&a[0], "DataStatus", "Value", &n);
	Native_Free(&x);
	Native_Free(&y);
	Native_Free(&label);
	Native_Free(&n);
	return ok;
}

LV_FN(Data, GetFunctionariesEnemiesProfileA, 0, Data_GetFunctionariesEnemiesProfile_C)
LV_FN(Data, InitA, 1, Data_Init)
LV_FN(Data, InitMissionA, 0, Data_InitMission)
LV_FN(Data, StopMissionA, 0, Data_StopMission)
LV_FN(Data, CreateWaveA, 1, Data_CreateWave)
LV_FN(Data, CreateFunctionaryA, 0, Data_CreateFunctionary)
LV_FN(Data, ReinsertFunctionaryInWaveA, 0, Data_ReinsertFunctionaryInWave)
LV_FN(Data, OnFunctionaryDeathA, 2, Data_OnFunctionaryDeath)
LV_FN(Data, OnFunctionaryDamageA, 3, Data_OnFunctionaryDamage)
LV_FN(Data, DropDataA, 1, Data_DropData)
LV_FN(Data, CheckAITakesDataA, 1, Data_CheckAITakesData)
LV_FN(Data, CheckAIA, 1, Data_CheckAI)
LV_FN(Data, TakeDataA, 2, Data_TakeData)
LV_FN(Data, CallBossA, 0, Data_CallBoss)
LV_FN(Data, UsePhoneCabA, 4, Data_UsePhoneCab_C)
LV_FN(Data, PreGetDataA, 0, Data_PreGetData)
LV_FN(Data, GetDataA, 0, Data_GetData)
LV_FN(Data, PreCallDeepThroatA, 0, Data_PreCallDeepThroat)
LV_FN(Data, PreCallDeepThroat2A, 0, Data_PreCallDeepThroat2)
LV_FN(Data, CallDeepThroatA, 0, Data_CallDeepThroat)
LV_FN(Data, UsePhoneCab2A, 4, Data_UsePhoneCab2_C)
LV_FN(Data, SetNextMissionA, 0, Data_SetNextMission)
LV_FN(Data, MissionScorerA, 1, Data_MissionScorer)

PyMethodDef g_CommercialGetDataPart3Methods[] = {
	{ "GetFunctionariesEnemiesProfile", Data_GetFunctionariesEnemiesProfileA, 1, NULL },
	{ "InitStatus", Data_InitStatus, 1, NULL },
	{ "Init", Data_InitA, 1, NULL },
	{ "InitMission", Data_InitMissionA, 1, NULL },
	{ "StopMission", Data_StopMissionA, 1, NULL },
	{ "CreateWave", Data_CreateWaveA, 1, NULL },
	{ "CreateFunctionary", Data_CreateFunctionaryA, 1, NULL },
	{ "ReinsertFunctionaryInWave", Data_ReinsertFunctionaryInWaveA, 1, NULL },
	{ "OnFunctionaryDeath", Data_OnFunctionaryDeathA, 1, NULL },
	{ "OnFunctionaryDamage", Data_OnFunctionaryDamageA, 1, NULL },
	{ "DropData", Data_DropDataA, 1, NULL },
	{ "CheckAITakesData", Data_CheckAITakesDataA, 1, NULL },
	{ "CheckAI", Data_CheckAIA, 1, NULL },
	{ "TakeData", Data_TakeDataA, 1, NULL },
	{ "CallBoss", Data_CallBossA, 1, NULL },
	{ "UsePhoneCab", Data_UsePhoneCabA, 1, NULL },
	{ "PreGetData", Data_PreGetDataA, 1, NULL },
	{ "GetData", Data_GetDataA, 1, NULL },
	{ "PreCallDeepThroat", Data_PreCallDeepThroatA, 1, NULL },
	{ "PreCallDeepThroat2", Data_PreCallDeepThroat2A, 1, NULL },
	{ "CallDeepThroat", Data_CallDeepThroatA, 1, NULL },
	{ "UsePhoneCab2", Data_UsePhoneCab2A, 1, NULL },
	{ "SetNextMission", Data_SetNextMissionA, 1, NULL },
	{ "MissionScorer", Data_MissionScorerA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DefeatCrazyGambler3: the crazy gambler's copies, then his matrix ---- */

static const char *const crazy_gambler_profile[] = { "Ship=SBANKMASTER1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=2",
	"UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25", "UsePercHook=20", "FactorHookMap=10", "CadenceShoot=5", "ControlAim=3",
	"AngAttack=1.7", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.25",
	"FactorDamageItemLife=0.35", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=30", "TimeMaxStrafe=10",
	"VelMaxStrafe=1.0", NULL };

CM_PROFILE(Gambler_GetCrazyGamblerProfile, crazy_gambler_profile)

static PyObject *Gambler_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, CG, ac_ships_bertos);
}

/* Init(LevelPath): the evil gambler's head, his copies (or the matrix when all are dead), the taunts */
static byte Gambler_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t pos;
	(void)a;
	(void)out;
	/* SScorer.AddModel through its ref: the cases record it */
	if (!PORT_CALL(SScorer, AddModel, NULL, "isssii", 0L, "EvilGambler", "Models/Heads/HeadEvilGambler/HeadEvilGambler", "", 4L, 20L) ||
		!Cm_PreloadG(CG, "BossMatrixModel") || !Mode_ListSet("Enemies", "OnDeath", "Mission_DefeatCrazyGambler3.OnCrazyGamblerDeath")) {
		return 0;
	}
	if (LvN_SaveIs("NumCrazyGamblerLifes", "6") && !LvN_Com("Mission_DefeatCrazyGambler3_RemoteMessage1", "DeepThroat", NULL, NULL, NULL)) {
		return 0;
	}
	if (LvN_SaveIs("NumCrazyGamblerLifes", "0")) {
		Native_SetNone(&pos);
		byte ok = LvN_SaveList("BossMatrixPos", CG, &pos) && (!Native_True(&pos) ||
			PORT_LOWN(Mission_DefeatCrazyGambler3, DropBossMatrix, NULL, "v", &pos));
		Native_Free(&pos);
		if (!ok) {
			return 0;
		}
	} else if (!PORT_LOWN(Mission_DefeatCrazyGambler3, CreateCrazyGamblers, NULL, "") || !LV_SCORERG(Mission_DefeatCrazyGambler3, MissionScorer)) {
		return 0;
	}
	return CM_LATER(3, CG, "LaunchFirstTaunt", NULL);
}

static byte Gambler_LaunchFirstTaunt(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return (!LvN_SaveIs("NumCrazyGamblerLifes", "6") || LvN_Com("Mission_DefeatCrazyGambler3_RemoteMessage2", "EvilGambler", NULL, NULL, NULL)) &&
		CM_LATER(15, CG, "LaunchTaunts", NULL);
}

/* LaunchTaunts(): the four taunts in turn every 12 s while copies live */
static byte Gambler_LaunchTaunts(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lifes = Mode_V(&t), *n = Mode_V(&t), *num = Mode_V(&t), *sum = Mode_V(&t), *rep = Mode_V(&t), *key = Mode_V(&t);
	nativeVal_t *n1 = Mode_V(&t), *m = Mode_V(&t);
	(void)a;
	(void)out;
	Mode_SaveVar("NumCrazyGamblerLifes", lifes);
	int go = Native_True(lifes);
	if (go > 0) {
		go = Native_ToInt(lifes, n) ? Mode_CmpI(n, ">", 0) : -1;
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	byte ok = Mode_G(CG, "numTaunt", num) && Native_Arith('+', Mode_I(&t, 3), num, sum);
	if (ok) {
		Mode_Repr(sum, rep);
		ok = Native_Arith('+', Mode_S(&t, "Mission_DefeatCrazyGambler3_RemoteMessage"), rep, key) &&
			PORT_CALL(Speech, RemoteComunication, NULL, "vs", key, "EvilGambler");
	}
	Native_Free(num);
	return Mode_Done(&t, ok && Mode_G(CG, "numTaunt", num) && Native_Arith('+', num, Mode_I(&t, 1), n1) && Mode_Arith('%', n1, Mode_I(&t, 4), m) &&
		Mode_Put(CG, "numTaunt", NULL, m) && CM_LATER(12, CG, "LaunchTaunts", NULL));
}

/* RefreshScorer(id): Menu.DrawScorer(id, SInput.GetActionSet()) */
static byte Gambler_RefreshScorer(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t set;
	(void)out;
	/* the lookup of Menu.DrawScorer comes after its arguments now (they cannot fail) */
	Native_SetStr(&set, TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
	return PORT_CALL(Menu, DrawScorer, NULL, "vv", &a[0], &set);
}

/* CreateCrazyGambler(): one copy of the gambler at a free spawn point */
static byte Gambler_CreateCrazyGambler(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t h, pd;
	Native_SetNone(&h);
	(void)a;
	(void)out;
	Native_SetNone(&pd);
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_SpawnCar("CrazyGambler", 0, &h, NULL) &&
		PORT_LOWN(Mission_DefeatCrazyGambler3, GetCrazyGamblerProfile, &pd, "") && LvN_StdAI(&h, &pd) && LvN_SetI(&h, "LaunchTaunts", 0) &&
		LvN_SetI(&h, "Money", -1) && LvN_SetG(&h, "MPil0", CG, "pilotName") && LvN_SetS(&h, "TgType", "red") &&
		LvN_SetS(&h, "EntityList", "Enemies") && LvN_Life(&h, CG, "GAMBLER_LIFE") && Mode_AppendName(CG, "crazyGamblers", &h) && LvN_Activate(&h, 1);
	Native_Free(&h);
	Native_Free(&pd);
	return ok;
}

/* CreateCrazyGamblers(): a copy for each life but the last, 0.2 s apart, then the scorer */
static byte Gambler_CreateCrazyGamblers(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *none = Mode_V(&t), *n = Mode_V(&t), *n1 = Mode_V(&t), *step = Mode_V(&t), *d = Mode_V(&t);
	long count;
	(void)a;
	(void)out;
	Native_SetFloat(step, 0.2);
	if (!Mode_Append(CG, "crazyGamblers", none) || !Mode_SaveInt("NumCrazyGamblerLifes", n)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqI(n, 1)) {
		if (!PORT_LOWN(Mission_DefeatCrazyGambler3, CreateCrazyGambler, NULL, "")) {
			return Mode_Done(&t, 0);
		}
	} else {
		if (!Native_Arith('-', n, Mode_I(&t, 1), n1) || !Mode_Range(n1, &count)) {
			return Mode_Done(&t, 0);
		}
		for (long i = 0; i < count; i++) {
			nativeVal_t iv, di;
			Native_SetInt(&iv, i);
			Native_SetNone(&di);
			byte ok = Mode_Arith('*', &iv, step, &di) && Mode_LaterV(&di, CG, "CreateCrazyGambler", NULL, 0, NULL);
			Native_Free(&di);
			if (!ok) {
				return Mode_Done(&t, 0);
			}
		}
		Native_Free(n1);
	}
	return Mode_Done(&t, Native_Arith('-', n, Mode_I(&t, 1), n1) && Mode_Arith('*', n1, step, d) &&
		Mode_LaterV(d, CG, "RefreshScorer", Mode_I(&t, 0), 1, NULL));
}

/* Speech.ForceRemoteCom(key, who, AfterRemoteComFunc = func, Parameters = ()) with func the global of module (OBJLIB 5:
 * a function handed on as a value; Speech keeps it in its variable OnEndOfSpeech and calls it with Mode_Apply, the
 * entry of a port's own function) */
static byte Cm_ForceCom(const char *key, const char *who, const char *module, const char *func) {
	nativeVal_t args[2];
	PyObject *fn = Mode_GlobalPy(module, func);
	if (!fn) {
		return 0;
	}
	LvN_Str(&args[0], key);
	LvN_Str(&args[1], who);
	byte ok = PORT_CALL(Speech, ForceRemoteCom, NULL, "vv;AfterRemoteComFunc=O,Parameters=v", &args[0], &args[1], fn, LvN_NoArgs());
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	Py_DECREF(fn);
	return ok;
}

/* OnCrazyGamblerDeath(LooserName, WinnerName): a life less; the last drops the matrix, one left: DeepThroat */
static byte Gambler_OnCrazyGamblerDeath(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *e = Mode_V(&t), *pos = Mode_V(&t);
	(void)out;
	if (!Mode_Import("Vehicles")) {
		return Mode_Done(&t, 0);
	}
	int in = Mode_InG(&a[0], CG, "crazyGamblers");
	if (in < 0) {
		return Mode_Done(&t, 0);
	}
	if (in) {
		if (!Mode_Remove(CG, "crazyGamblers", &a[0]) || !Cm_SaveAdd("NumCrazyGamblerLifes", -1, n)) {
			return Mode_Done(&t, 0);
		}
		if (LvN_EqI(n, 0)) {
			if (!PORT_CALL(SScorer, CancelSpeech, NULL, "") || !LvN_ScorerOff() || !Mode_EntityH(&a[0], e) || !LvN_SetI(e, "Money", -1) ||
				!Mode_VGet(e, "Pos", pos)) {
				return Mode_Done(&t, 0);
			}
			LvN_SaveRepr("BossMatrixPos", pos);
			Native_Free(pos);
			if (!Mode_VGet(e, "Pos", pos) || !PORT_LOWN(Mission_DefeatCrazyGambler3, DropBossMatrix, NULL, "v", pos)) {
				return Mode_Done(&t, 0);
			}
		} else if (LvN_EqI(n, 1) && !Cm_ForceCom("Mission_DefeatCrazyGambler3_RemoteMessage7", "DeepThroat", CG, "AfterDeepThroatMessage")) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Cm_OnDeath(a) && LvN_Draw());
}

/* AfterDeepThroatMessage(): the last copy */
static byte Gambler_AfterDeepThroatMessage(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_MMDesc("Mission_DefeatCrazyGambler4_MissionText1", 0) && Cm_GEmpty(CG, "crazyGamblers", 0) &&
		PORT_LOWN(Mission_DefeatCrazyGambler3, CreateCrazyGambler, NULL, "");
}

static byte Gambler_DropBossMatrix(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_Drop(CG, &a[0], "BossMatrix", "BossMatrixModel", "BossMatrixRadius", 1, "Mission_DefeatCrazyGambler3.TakeMatrix", "TakeMatrixExt", NULL);
}

/* TakeMatrix(ItemName, CharName): DeepThroat, then the next mission */
static byte Gambler_TakeMatrix(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Cm_TakenArrow(&a[0]) &&
		LV_COM("Mission_DefeatCrazyGambler4_RemoteMessage1", "DeepThroat", Mission_DefeatCrazyGambler3, SetNextMission, LvN_NoArgs());
}

/* SetNextMission(): on to Mission_WhoIsDeepThroat in Press */
static byte Gambler_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const places[] = { "Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "Press",
		"Journalist.Berto_Defaut", "Press", NULL };
	(void)a;
	(void)out;
	if (!Mode_DelSaveVars("NumCrazyGamblerLifes") || !LvN_Next("Mission_WhoIsDeepThroat", "Press", "Mission_WhoIsDeepThroat_MissionText1")) {
		return 0;
	}
	Mode_Saves(places);
	return Mode_WayPoint();
}

/* MissionScorer(id): the lives left */
static byte Gambler_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *label = Mode_V(&t), *x2 = Mode_V(&t), *y2 = Mode_V(&t), *lifes = Mode_V(&t);
	(void)out;
	LvN_Label("Mission_DefeatCrazyGambler2_Scorer1", label);
	if (!Mode_Import("Scorer") || !LvN_TextX(0, x) || !LvN_TextY(0, y) ||
		!LvN_MissionText(&a[0], "RemainingLifesText", "Text", x, y, "Horatio", "Shadow", label, "Left", NULL) || !LvN_TextX(0, x2) ||
		!LvN_TextY(20, y2)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("NumCrazyGamblerLifes", lifes);
	return Mode_Done(&t, LvN_MissionText(&a[0], "RemainingLifes", "Text", x2, y2, "Horatio", "Shadow", lifes, "Left", NULL));
}

LV_FN(Gambler, GetCrazyGamblerProfileA, 0, Gambler_GetCrazyGamblerProfile_C)
LV_FN(Gambler, InitA, 1, Gambler_Init)
LV_FN(Gambler, LaunchFirstTauntA, 0, Gambler_LaunchFirstTaunt)
LV_FN(Gambler, LaunchTauntsA, 0, Gambler_LaunchTaunts)
LV_FN(Gambler, RefreshScorerA, 1, Gambler_RefreshScorer)
LV_FN(Gambler, CreateCrazyGamblerA, 0, Gambler_CreateCrazyGambler)
LV_FN(Gambler, CreateCrazyGamblersA, 0, Gambler_CreateCrazyGamblers)
LV_FN(Gambler, OnCrazyGamblerDeathA, 2, Gambler_OnCrazyGamblerDeath)
LV_FN(Gambler, AfterDeepThroatMessageA, 0, Gambler_AfterDeepThroatMessage)
LV_FN(Gambler, DropBossMatrixA, 1, Gambler_DropBossMatrix)
LV_FN(Gambler, TakeMatrixA, 2, Gambler_TakeMatrix)
LV_FN(Gambler, SetNextMissionA, 0, Gambler_SetNextMission)
LV_FN(Gambler, MissionScorerA, 1, Gambler_MissionScorer)

PyMethodDef g_CommercialDefeatCrazyGambler3Methods[] = {
	{ "GetCrazyGamblerProfile", Gambler_GetCrazyGamblerProfileA, 1, NULL },
	{ "InitStatus", Gambler_InitStatus, 1, NULL },
	{ "Init", Gambler_InitA, 1, NULL },
	{ "LaunchFirstTaunt", Gambler_LaunchFirstTauntA, 1, NULL },
	{ "LaunchTaunts", Gambler_LaunchTauntsA, 1, NULL },
	{ "RefreshScorer", Gambler_RefreshScorerA, 1, NULL },
	{ "CreateCrazyGambler", Gambler_CreateCrazyGamblerA, 1, NULL },
	{ "CreateCrazyGamblers", Gambler_CreateCrazyGamblersA, 1, NULL },
	{ "OnCrazyGamblerDeath", Gambler_OnCrazyGamblerDeathA, 1, NULL },
	{ "AfterDeepThroatMessage", Gambler_AfterDeepThroatMessageA, 1, NULL },
	{ "DropBossMatrix", Gambler_DropBossMatrixA, 1, NULL },
	{ "TakeMatrix", Gambler_TakeMatrixA, 1, NULL },
	{ "SetNextMission", Gambler_SetNextMissionA, 1, NULL },
	{ "MissionScorer", Gambler_MissionScorerA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyPorkinsTransports: the mayor's campaign against bishop Porkins ---- */

static const char *const bishops_profile[] = { "Ship=SBishop1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"CadenceShoot=5", "ControlAim=4", "AngAttack=3.14", "CounterAttack=0.50", "ControlBoost=1", "ControlBoostItemLife=1",
	"ControlBoostItemAmmo=1", "FactorDamageGetAway=0.7", "FactorDamageItemLife=0.25", "FactorItemAmmo=0.25", "TimeMaxStrafe=10",
	"VelMaxStrafe=0.75", NULL };

CM_PROFILE(Porkins_GetBishopsEnemiesProfile, bishops_profile)

/* Traffic.var = Traffic.var / 2 */
static byte Pt_Half(const char *var) {
	nativeVal_t v, two, r;
	Native_SetInt(&two, 2);
	Native_SetNone(&r);
	byte ok = LvN_ModGet("Traffic", var, &v) && Mode_Arith('/', &v, &two, &r) && Mode_SetModVar("Traffic", var, &r);
	Native_Free(&v);
	Native_Free(&r);
	return ok;
}

/* InitStatus(LevelPath): half the traffic, the vote transports as heavy traffic, no traffic until the end */
static byte Porkins_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const heavy[] = { "TVoteMayor", "TVoteBishop", NULL };
	nativeVal_t v;
	(void)a;
	(void)out;
	if (!Mode_Import("Traffic")) {
		return 0;
	}
	Mode_Texts(&v, heavy, -1);
	byte ok = Mode_SetModVar("Traffic", "AcHeavyShips", &v);
	Native_Free(&v);
	Native_SetInt(&v, 0);
	if (!ok || !Pt_Half("NumNormal") || !Pt_Half("NumMedium") || !Mode_SetModVar("Traffic", "NumHeavy", &v)) {
		return 0;
	}
	ok = Native_GVarGet("iTrafficNumber", &v) && Mode_Put(PT, "currentTrafficNumber", NULL, &v);
	Native_Free(&v);
	if (!ok || !Mode_GVarSetI("iTrafficNumber", 0) ||
		(LvN_SaveIs("MissionState", "Popularity") && !CM_HOOK("Parking", "SwitchVehicleMissionCallback", PT, "InitMission"))) {
		return 0;
	}
	return LvN_AcShips(PT, ac_ships_bertos);
}

static byte Porkins_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (LvN_SaveIs("MissionState", "Popularity")) {
		return LvN_Com("Mission_Campaign2_RemoteMessage1", "Mayor", NULL, NULL, NULL);
	}
	return PORT_LOWN(Mission_DestroyPorkinsTransports, CallDeepThroat, NULL, "");
}

/* InitMission(): the scorer, five transports of each side, two bishops */
static byte Porkins_InitMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!LvN_Com("Mission_Campaign2_RemoteMessage2", "Functionary", NULL, NULL, NULL) || !LvN_ModNone("Parking", "SwitchVehicleMissionCallback") ||
		!LV_SCORERG(Mission_DestroyPorkinsTransports, MissionScorer) || !LvN_Draw()) {
		return 0;
	}
	for (int i = 0; i < 5; i++) {
		if (!PORT_LOWN(Mission_DestroyPorkinsTransports, CreateMayorTransport, NULL, "")) {
			return 0;
		}
	}
	for (int i = 0; i < 5; i++) {
		if (!PORT_LOWN(Mission_DestroyPorkinsTransports, CreatePorkinsTransport, NULL, "")) {
			return 0;
		}
	}
	for (int i = 0; i < 2; i++) {
		if (!PORT_LOWN(Mission_DestroyPorkinsTransports, CreateBishop, NULL, "i", 0L)) {
			return 0;
		}
	}
	return LvN_Com("Mission_Campaign2_RemoteMessage6", "Bishop", NULL, NULL, NULL);
}

/* a vote transport on the traffic paths with its life, death callback and colour into p */
static byte Pt_Transport(const char *model, const char *life, const char *onDeath, const char *tg, nativeVal_t *p) {
	nativeVal_t name;
	Native_SetNone(&name);
	Native_SetNone(p);
	byte ok = Mode_Import("Traffic") && Mode_Import("VehiclesNPC") && PORT_CALL(Traffic, SetPath, NULL, "") &&
		PORT_CALL(VehiclesNPC, MakeAI, p, "ssii", model, "Traffic", 1L, 0L) && PORT_CALL(Traffic, ResetPath, NULL, "") &&
		Mode_VGet(p, "Name", &name) && STATE("ivi", 3L, &name, 0L) && LvN_Life(p, PT, life) && LvN_SetS(p, "OnDeath", onDeath) &&
		LvN_SetS(p, "TgType", tg) && LvN_SetI(p, "FixedModel", 1);
	Native_Free(&name);
	return ok;
}

static byte Porkins_CreateMayorTransport(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t p;
	(void)a;
	(void)out;
	byte ok = Pt_Transport("TVoteMayor", "MAYOR_TRANSPORT_LIFE", "Mission_DestroyPorkinsTransports.OnMayorTransportDeath", "green", &p) &&
		Mode_AppendName(PT, "mayorTransports", &p);
	Native_Free(&p);
	return ok;
}

static byte Porkins_CreatePorkinsTransport(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t p;
	(void)a;
	(void)out;
	byte ok = Pt_Transport("TVoteBishop", "BISHOP_TRANSPORT_LIFE", "Mission_DestroyPorkinsTransports.OnPorkinsTransportDeath", "red", &p) &&
		LvN_Activate(&p, 1) && Mode_AppendName(PT, "bishopTransports", &p);
	Native_Free(&p);
	return ok;
}

/* CreateBishop(launchTaunt=1): a bishop that attacks the nearest transport of the mayor; every other one taunts */
static byte Porkins_CreateBishopN(const nativeVal_t *taunt) {
	nativeVal_t lst, p, pd, name;
	Native_SetNone(&lst);
	if (!Mode_Import("VehiclesNPC") || !Mode_Import("OutMap") || !Mode_G(PT, "mayorTransports", &lst)) {
		return 0;
	}
	int n = Native_Size(&lst);
	Native_Free(&lst);
	if (n <= 0) {
		return n == 0 ? LvN_RaiseText("Se han de crear primero los transportes del alcalde") : 0;
	}
	Native_SetNone(&pd);
	Native_SetNone(&name);
	byte ok = Mode_SpawnCar("Bishop", 0, &p, NULL) && LvN_SetS(&p, "TgType", "red") &&
		PORT_LOWN(Mission_DestroyPorkinsTransports, GetBishopsEnemiesProfile, &pd, "") && LvN_StdAI(&p, &pd) &&
		LvN_Life(&p, PT, "BISHOPS_LIFE") && Mode_VGet(&p, "Name", &name) &&
		PORT_LOWN(Mission_DestroyPorkinsTransports, AttackMayorTransport, NULL, "v", &name) &&
		Mode_AppendName(PT, "bishops", &p);
	Native_Free(&p);
	Native_Free(&pd);
	Native_Free(&name);
	if (!ok || !Native_True(taunt)) {
		return ok;
	}
	int show = LvN_GTrue(PT, "showBishopTaunt");
	if (show > 0) {
		return LvN_PutI(PT, "showBishopTaunt", 0) && LvN_Com("Mission_Campaign2_RemoteMessage6", "Bishop", NULL, NULL, NULL);
	}
	return show == 0 && LvN_PutI(PT, "showBishopTaunt", 1);
}

MODE_KW(Porkins, CreateBishop, "|O", "launchTaunt") {
	Mode_KwDefI(v, given, 0, 1);
	return Porkins_CreateBishopN(&v[0]);
}

/* AttackMayorTransport(bishopName): the bishop's callbacks, it attacks the nearest transport of the mayor */
static byte Porkins_AttackMayorTransport(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *bishop = Mode_V(&t), *target = Mode_V(&t), *minDist = Mode_V(&t), *lst = Mode_V(&t), *bn = Mode_V(&t);
	(void)out;
	if (!Mode_EntityH(&a[0], bishop)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(bishop)) {
		return Mode_Done(&t, 1);
	}
	if (!LvN_SetS(bishop, "OnDeath", "Mission_DestroyPorkinsTransports.OnBishopDeath") ||
		!LvN_SetS(bishop, "OnDamage", "Mission_DestroyPorkinsTransports.OnBishopDamage") || !LvN_SetS(bishop, "EntityList", "") ||
		!Mode_G(PT, "mayorTransports", lst)) {
		return Mode_Done(&t, 0);
	}
	LvN_Str(target, "");
	Native_SetInt(minDist, 5000000);
	int n = Native_ForCount(lst);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t tr, e, d;
		Native_SetNone(&tr);
		Native_SetNone(&e);
		Native_SetNone(&d);
		ok = Native_SeqItem(lst, i, &tr) && Mode_EntityH(&tr, &e);
		if (ok && Native_True(&e)) {
			int c = Cm_Dist(bishop, &e, 0, &d) ? Mode_Cmp(&d, "<", minDist) : -1;
			ok = c >= 0;
			if (c > 0) {
				Native_Free(minDist);
				Native_Copy(minDist, &d);
				Native_Free(target);
				Native_Copy(target, &tr);
			}
		}
		Native_Free(&tr);
		Native_Free(&e);
		Native_Free(&d);
	}
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	int some = Mode_Cmp(target, "!=", Mode_S(&t, ""));
	return Mode_Done(&t, some >= 0 && (!some || (Mode_VGet(bishop, "Name", bn) && STATE("ivv", 2L, bn, target))));
}

/* the ships of a list of names (a global) leave the mission: no target colour, deaths as normal traffic */
static byte Pt_Release(const char *list, int bishops, int deactivate) {
	nativeVal_t lst;
	if (!Mode_G(PT, list, &lst)) {
		return 0;
	}
	int n = Native_ForCount(&lst);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name, e;
		Native_SetNone(&name);
		Native_SetNone(&e);
		ok = Native_SeqItem(&lst, i, &name) && Mode_EntityH(&name, &e) && (!Native_True(&e) || (LvN_SetS(&e, "TgType", "none") &&
			(bishops ? LvN_SetS(&e, "EntityList", "Traffic") : LvN_SetS(&e, "OnDeath", "Vehicles.OnDeath")) && (!deactivate || LvN_Activate(&e, 0))));
		Native_Free(&name);
		Native_Free(&e);
	}
	Native_Free(&lst);
	return ok;
}

/* mayorPopularity = mayorPopularity op 1.0 */
static byte Cm_PopOp(char op) {
	nativeVal_t one;
	Native_SetFloat(&one, 1.0);
	return Mode_GOp(PT, "mayorPopularity", op, &one);
}

/* OnPorkinsTransportDeath(LooserName, WinnerName): the player's kills raise the mayor's popularity; full: won */
static byte Porkins_OnPorkinsTransportDeath(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!Mode_Import("Vehicles") || !Mode_Remove(PT, "bishopTransports", &a[0]) || !Cm_OnDeath(a)) {
		return 0;
	}
	int by = Cm_UsrNameIs(&a[1]);
	if (by < 0 || (by && !Cm_PopOp('+')) || !PORT_LOWN(Mission_DestroyPorkinsTransports, UpdateScorer, NULL, "")) {
		return 0;
	}
	nativeVal_t pop, max;
	Native_SetNone(&max);
	int full = Mode_G(PT, "mayorPopularity", &pop) && Mode_G(PT, "MAX_POPULARITY", &max) ? Mode_Cmp(&pop, "==", &max) : -1;
	Native_Free(&pop);
	Native_Free(&max);
	if (full > 0) {
		return Pt_Release("mayorTransports", 0, 0) && Pt_Release("bishopTransports", 0, 1) && Pt_Release("bishops", 1, 0) &&
			Mode_DelSchedS("CreateBishop") &&
			LV_COM("Mission_Campaign2_RemoteMessage3", "Mayor", Mission_DestroyPorkinsTransports, GetMayorReward, NULL);
	}
	return full == 0 && PORT_LOWN(Mission_DestroyPorkinsTransports, CreatePorkinsTransport, NULL, "");
}

/* `num % k == 0` of the global numMayorTransportsDeaths: 1, 0, -1 */
static int Pt_Every(long k) {
	nativeVal_t n, kv, r;
	Native_SetInt(&kv, k);
	Native_SetNone(&r);
	int c = Mode_G(PT, "numMayorTransportsDeaths", &n) && Mode_Arith('%', &n, &kv, &r) ? Mode_CmpI(&r, "==", 0) : -1;
	Native_Free(&n);
	Native_Free(&r);
	return c;
}

/* OnMayorTransportDeath(LooserName, WinnerName): popularity down, a new transport, the functionary and the mayor
 * complain now and then */
static byte Porkins_OnMayorTransportDeath(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t zero, taunt, one, r;
	(void)out;
	if (!Mode_Import("Vehicles") || !Mode_Remove(PT, "mayorTransports", &a[0]) || !Cm_OnDeath(a) || !Cm_PopOp('-')) {
		return 0;
	}
	Native_SetFloat(&zero, 0.0);
	int neg = Mode_GCmp(PT, "mayorPopularity", "<", &zero);
	if (neg < 0 || (neg && !Mode_Put(PT, "mayorPopularity", NULL, &zero)) || !PORT_LOWN(Mission_DestroyPorkinsTransports, UpdateScorer, NULL, "") ||
		!PORT_LOWN(Mission_DestroyPorkinsTransports, CreateMayorTransport, NULL, "") || !LvN_GAdd(PT, "numMayorTransportsDeaths", 1, NULL)) {
		return 0;
	}
	int two = Pt_Every(2);
	if (two < 0 || (two && !LvN_Com("Mission_Campaign2_RemoteMessage2", "Functionary", NULL, NULL, NULL))) {
		return 0;
	}
	int three = Pt_Every(3);
	if (three < 0) {
		return 0;
	}
	if (three) {
		Native_SetInt(&one, 1);
		Native_SetNone(&r);
		byte ok = Mode_G(PT, "taunt", &taunt) && Native_Arith('-', &one, &taunt, &r) && Mode_Put(PT, "taunt", NULL, &r);
		Native_Free(&taunt);
		Native_Free(&r);
		int t0 = ok ? LvN_GEqI(PT, "taunt", 0) : -1;
		if (t0 < 0 || !LvN_Com(t0 ? "Mission_Campaign2_RemoteMessage7" : "Mission_Campaign2_RemoteMessage8", "Mayor", NULL, NULL, NULL)) {
			return 0;
		}
	}
	return CM_LATER(1.0, PT, "CheckBishopWithoutTarget", NULL);
}

/* CheckBishopWithoutTarget(): idle bishops attack again */
static byte Porkins_CheckBishopWithoutTarget(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t lst;
	(void)a;
	(void)out;
	if (!Mode_G(PT, "bishops", &lst)) {
		return 0;
	}
	int n = Native_ForCount(&lst);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t name, e, st;
		Native_SetNone(&name);
		Native_SetNone(&e);
		Native_SetNone(&st);
		int idle = Native_SeqItem(&lst, i, &name) && Mode_EntityH(&name, &e) ? Native_True(&e) : -1;
		if (idle > 0) {
			idle = Mode_AIState(&name, &st) ? Mode_CmpI(&st, "==", 0) : -1;
		}
		ok = idle >= 0 && (!idle || PORT_LOWN(Mission_DestroyPorkinsTransports, AttackMayorTransport, NULL, "v", &name));
		Native_Free(&name);
		Native_Free(&e);
		Native_Free(&st);
	}
	Native_Free(&lst);
	return ok;
}

/* OnBishopDeath(LooserName, WinnerName): another bishop in a minute */
static byte Porkins_OnBishopDeath(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Mode_Import("Vehicles") && Mode_Remove(PT, "bishops", &a[0]) && Cm_OnDeath(a) && CM_LATER(60, PT, "CreateBishop", "CreateBishop");
}

/* OnBishopDamage(LooserName, WinnerName, Damage): shot by the player: it attacks him for 10 s */
static byte Porkins_OnBishopDamage(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, un, e, en;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&un);
	Native_SetNone(&e);
	Native_SetNone(&en);
	int by = LvN_Usr(&usr);
	if (by > 0) {
		by = Mode_VGet(&usr, "Name", &un) ? Mode_Cmp(&a[1], "==", &un) : -1;
	}
	byte ok = by >= 0;
	if (by > 0) {
		ok = Mode_EntityH(&a[0], &e) && (!Native_True(&e) || (LvN_SetS(&e, "EntityList", "Enemies") && Mode_VGet(&e, "Name", &en) &&
			Mode_Later(10, PT, "AttackMayorTransport", &en, 1, NULL)));
	}
	Native_Free(&usr);
	Native_Free(&un);
	Native_Free(&e);
	Native_Free(&en);
	return ok;
}

/* GetMayorReward(): the mayor's money, DeepThroat 2 s later */
static byte Porkins_GetMayorReward(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *money = Mode_V(&t), *reward = Mode_V(&t), *sum = Mode_V(&t), *time = Mode_V(&t), *args = Mode_V(&t);
	nativeVal_t *who = Mode_V(&t), *then = Mode_V(&t);
	(void)a;
	(void)out;
	if (!LvN_ScorerOff() || !LvN_Draw()) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(money, Mode_GetMoney());
	/* Scrap.SetMoney(money): a long int (or another value) through the entry for the error of its "i" */
	if (!Mode_G(PT, "mayorReward", reward) || !Native_Arith('+', money, reward, sum) ||
		!(sum->type == NV_INT || sum->type == NV_FLOAT ? Mode_SetMoney(sum) : PORT_CALL(Scrap, SetMoney, NULL, "v", sum))) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(time, Mode_Time() + 2);
	/* the function of Speech: the scheduler takes its entry (Port_Scheduled) */
	PyObject *fn = Script_Attr("Speech", "AddRemoteCom");
	if (!fn) {
		return Mode_Done(&t, 0);
	}
	LvN_Str(args, "Mission_Campaign2_RemoteMessage4");
	LvN_Str(who, "DeepThroat");
	byte ok = LvN_GObj(PT, "PreCallDeepThroat", then) && Mode_Schedule(time, fn, args, 3, NULL);
	Py_DECREF(fn);
	return Mode_Done(&t, ok);
}

static byte Porkins_PreCallDeepThroat(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("MissionState", "CallDeepThroat");
	return Cm_Desc("Mission_TalkToDeepThroat6_MissionText1") && PORT_LOWN(Mission_DestroyPorkinsTransports, CallDeepThroat, NULL, "");
}

static byte Porkins_CallDeepThroat(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_PhoneCab("Mission_DestroyPorkinsTransports.UsePhoneCab", 1, NULL, NULL);
}

CM_PHONE(Porkins_UsePhoneCab, PT, "Generics_StillPolice1_Functionary", "DeepThroat", &PORT_LREF(Mission_DestroyPorkinsTransports, SetNextMission),
	"Mission_TalkToDeepThroat6__ComDeepThroat_NI_1Msg", NULL, NULL, 1, 1, 1)

/* SetNextMission(): the traffic back, the mayor no longer to possess, the video, on to Orbit */
static byte Porkins_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t n, chars, mayor;
	(void)a;
	(void)out;
	if (!LvN_PcDefault(PT) || !Mode_DelSaveVars("MissionState")) {
		return 0;
	}
	byte ok = Mode_G(PT, "currentTrafficNumber", &n) && Mode_GVarSet("iTrafficNumber", &n);
	Native_Free(&n);
	if (!ok) {
		return 0;
	}
	SaveVar_Set("GameAct", "3rdMurder");
	LvN_Str(&mayor, "Mayor");
	ok = LvN_SaveList("Conversor.AvailableChars", PT, &chars) && LvN_ListRemove(&chars, &mayor);
	if (ok) {
		LvN_SaveRepr("Conversor.AvailableChars", &chars);
	}
	Native_Free(&chars);
	Native_Free(&mayor);
	if (!ok || !PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_IntroHuman", "Orbit") || !Mode_WayPoint()) {
		return 0;
	}
	SaveVar_Set("Map", "Levels/Orbit");
	/* Scrap.Achievement through its ref: the cases record it */
	return Mode_GVarSetS("PlayVideo", "Video/Mayor.avi") && PORT_CALL(Scrap, Achievement, NULL, "i", 4L) &&
		PORT_CALL(SaveGame, QLoad, NULL, "s", "Levels/Orbit");
}

/* UpdateScorer(): the popularity bar and its separator */
static byte Porkins_UpdateScorer(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pop = Mode_V(&t), *max = Mode_V(&t), *scale = Mode_V(&t), *x = Mode_V(&t), *w = Mode_V(&t), *ws = Mode_V(&t);
	nativeVal_t *xw = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_G(PT, "mayorPopularity", pop) || !Mode_G(PT, "MAX_POPULARITY", max) || !Mode_Arith('/', pop, max, scale)) {
		return Mode_Done(&t, 0);
	}
	Mode_ScorerGet(0, "PopularityBar", "X", x);
	Mode_ScorerGet(0, "PopularityBar", "W", w);
	if (!Mode_Arith('*', w, scale, ws)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(pop);
	return Mode_Done(&t, Mode_G(PT, "mayorPopularity", pop) && LvN_Set0("PopularityBar", "Value", pop) && Native_Arith('+', x, ws, xw) &&
		LvN_Set0("PopularitySeparator", "X", xw));
}

/* MissionScorer(id): the popularity text, bar and separator */
static byte Porkins_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *label = Mode_V(&t), *main = Mode_V(&t), *black = Mode_V(&t), *h = Mode_V(&t);
	nativeVal_t *bx = Mode_V(&t), *by = Mode_V(&t), *h4 = Mode_V(&t), *half = Mode_V(&t);
	const nativeVal_t *id = &a[0];
	(void)out;
	LvN_Label("Mission_Campaign2_Scorer1", label);
	Cm_Ints4(main, 16, 16, 255, 192);
	Cm_Ints4(black, 255, 16, 192, 192);
	if (!Mode_ScorerAddS(id, "PopularityText", "Text") || !Mode_ScorerAddS(id, "PopularitySeparator", "Sprite") || !Mode_ScorerAddS(id, "PopularityBar", "Bar") ||
		!LvN_TextX(0, x) || !LvN_TextY(0, y) || !LvN_MissionText(id, "PopularityText", NULL, x, y, "Horatio", "Shadow", label, "Left", NULL) ||
		!Mode_BarDefaults(id, "PopularityBar", main, black) || !Cm_ScorerG(id, "PopularityBar", "MaxValue", PT, "MAX_POPULARITY") ||
		!Cm_ScorerG(id, "PopularityBar", "Value", PT, "mayorPopularity") || !Mode_ScorerSetI(id, "PopularityBar", "Red", 255) ||
		!Mode_ScorerSetI(id, "PopularityBar", "Green", 0) || !Mode_ScorerSetI(id, "PopularityBar", "Blue", 0)) {
		return Mode_Done(&t, 0);
	}
	Mode_ScorerGet(0, "PopularityBar", "H", h);
	if (!Mode_ScorerSetI(id, "PopularitySeparator", "Red", 0) || !Mode_ScorerSetI(id, "PopularitySeparator", "Green", 255) ||
		!Mode_ScorerSetI(id, "PopularitySeparator", "Blue", 0)) {
		return Mode_Done(&t, 0);
	}
	Mode_ScorerGet(0, "PopularityBar", "X", bx);
	if (!LvN_ScorerSet(id, "PopularitySeparator", "X", bx)) {
		return Mode_Done(&t, 0);
	}
	Mode_ScorerGet(0, "PopularityBar", "Y", by);
	Native_SetFloat(half, 1.0 / 2);
	return Mode_Done(&t, LvN_ScorerSet(id, "PopularitySeparator", "Y", by) && LvN_ScorerSet(id, "PopularitySeparator", "ScaleX", half) &&
		Mode_Arith('/', h, Mode_I(&t, 4), h4) && LvN_ScorerSet(id, "PopularitySeparator", "ScaleY", h4) &&
		PORT_LOWN(Mission_DestroyPorkinsTransports, UpdateScorer, NULL, ""));
}

LV_FN(Porkins, GetBishopsEnemiesProfileA, 0, Porkins_GetBishopsEnemiesProfile_C)
LV_FN(Porkins, InitStatusA, 1, Porkins_InitStatus)
LV_FN(Porkins, InitA, 1, Porkins_Init)
LV_FN(Porkins, InitMissionA, 0, Porkins_InitMission)
LV_FN(Porkins, CreateMayorTransportA, 0, Porkins_CreateMayorTransport)
LV_FN(Porkins, CreatePorkinsTransportA, 0, Porkins_CreatePorkinsTransport)
LV_FN(Porkins, AttackMayorTransportA, 1, Porkins_AttackMayorTransport)
LV_FN(Porkins, OnPorkinsTransportDeathA, 2, Porkins_OnPorkinsTransportDeath)
LV_FN(Porkins, OnMayorTransportDeathA, 2, Porkins_OnMayorTransportDeath)
LV_FN(Porkins, CheckBishopWithoutTargetA, 0, Porkins_CheckBishopWithoutTarget)
LV_FN(Porkins, OnBishopDeathA, 2, Porkins_OnBishopDeath)
LV_FN(Porkins, OnBishopDamageA, 3, Porkins_OnBishopDamage)
LV_FN(Porkins, GetMayorRewardA, 0, Porkins_GetMayorReward)
LV_FN(Porkins, PreCallDeepThroatA, 0, Porkins_PreCallDeepThroat)
LV_FN(Porkins, CallDeepThroatA, 0, Porkins_CallDeepThroat)
LV_FN(Porkins, UsePhoneCabA, 4, Porkins_UsePhoneCab_C)
LV_FN(Porkins, SetNextMissionA, 0, Porkins_SetNextMission)
LV_FN(Porkins, UpdateScorerA, 0, Porkins_UpdateScorer)
LV_FN(Porkins, MissionScorerA, 1, Porkins_MissionScorer)

PyMethodDef g_CommercialDestroyPorkinsTransportsMethods[] = {
	{ "GetBishopsEnemiesProfile", Porkins_GetBishopsEnemiesProfileA, 1, NULL },
	{ "InitStatus", Porkins_InitStatusA, 1, NULL },
	{ "Init", Porkins_InitA, 1, NULL },
	{ "InitMission", Porkins_InitMissionA, 1, NULL },
	{ "CreateMayorTransport", Porkins_CreateMayorTransportA, 1, NULL },
	{ "CreatePorkinsTransport", Porkins_CreatePorkinsTransportA, 1, NULL },
	{ "CreateBishop", (PyCFunction)(void (*)(void))Porkins_CreateBishop, 3, NULL },
	{ "AttackMayorTransport", Porkins_AttackMayorTransportA, 1, NULL },
	{ "OnPorkinsTransportDeath", Porkins_OnPorkinsTransportDeathA, 1, NULL },
	{ "OnMayorTransportDeath", Porkins_OnMayorTransportDeathA, 1, NULL },
	{ "CheckBishopWithoutTarget", Porkins_CheckBishopWithoutTargetA, 1, NULL },
	{ "OnBishopDeath", Porkins_OnBishopDeathA, 1, NULL },
	{ "OnBishopDamage", Porkins_OnBishopDamageA, 1, NULL },
	{ "GetMayorReward", Porkins_GetMayorRewardA, 1, NULL },
	{ "PreCallDeepThroat", Porkins_PreCallDeepThroatA, 1, NULL },
	{ "CallDeepThroat", Porkins_CallDeepThroatA, 1, NULL },
	{ "UsePhoneCab", Porkins_UsePhoneCabA, 1, NULL },
	{ "SetNextMission", Porkins_SetNextMissionA, 1, NULL },
	{ "UpdateScorer", Porkins_UpdateScorerA, 1, NULL },
	{ "MissionScorer", Porkins_MissionScorerA, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Commercial)
