/*
 * The scripts of the level Scrapyard in C (levels/scrapyard/scripts of data02.packed, issue #2): the Map module (the
 * traffic, the metro and the elevator to the gamblin den), MapTriggers (the lairs of the mosquitos and the mad
 * hunters) and the missions of the level (Mission_Ambush, Mission_DefeatCrazyGambler2, Mission_FindDtritusMatrix,
 * Mission_MeetBerto, Mission_MosquitoClue, Mission_StupidMerc, Mission_StupidMercAgain, Mission_TalkToDeepThroat).
 * MapSnd and the plain functions of the missions are data (data/levels/scrapyard/). The module variables (counters,
 * lists, waypoints, constants, models) and the module code (the save variables they create) stay in the scripts; the
 * functions read and write them as their globals.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values, read the globals of their module where the
 * script reads them and write them back at once (OBJLIB 5; the lists and dicts of the modules are changed in place),
 * and call the engine in C: save variables, engine variables, entities, the dummy search, the entity lists, the money,
 * the scorer heads, the ammunition table, the parkings. The engine functions the cases record or swap go through
 * their seams (AddScheduledFunc, CreateEntity, UsrEntity, Set, ConsoleError, Preload3DObject, GetAlarm,
 * SAI.GetStateVehicle, DeleteScheduledFuncs, SScorer.Add and Set; OBJLIB 7). The entities the script holds are handle
 * values (OBJLIB 7). SAI.SetStateVehicle, SAI.GetNearestItemLife (a case swaps it) and Scrap.GetNearestParked of a
 * position that is no vector go through their refs (stage 7); the profile objects of VehiclesNPC are the objects they
 * are (OBJLIB 5).
 *
 * Direct calls (stage 3): the functions of the other ports (Elevator, Metro, Parking, VehiclesNPC, Traffic,
 * MissionsFuncs, Speech, PhoneCab, Items, OutMap, RaceMaker, Vehicles), the keyword calls of Speech.ForceRemoteCom and
 * VehiclesNPC.MakeAI and the own functions of the modules (their globals, the profile functions included) go through
 * their refs (PORT_CALL, PORT_LOWN, LV_COM, LV_SCORERG, LvN_OnEndRef; the seam takes the call by name when a mission or
 * a case has put something else there, OBJLIB 5). By name: the sections of the .def files (LaunchAmbush and
 * PreLaunchAmbush of Mission_Ambush, PreSetNextMission of Mission_DefeatCrazyGambler2, BadGuyAppears,
 * CreateMissionPlans and SetNextMission of Mission_FindDtritusMatrix, InitEvasion of Mission_StupidMercAgain; OBJLIB 6)
 * and the class VehiclesNPC.cShipAI (OBJLIB 5); the functions given to the scheduler and to the entities (Mode_Later,
 * Mode_AtG, the trigger schedules, the OnDeath and UseFunc texts) take the entry of a port's own function (stage 4:
 * Port_Scheduled, PYF_ENTRY; bound methods stay Python, OBJLIB 7), the ones given to Parking and CharAct as callbacks
 * (LvN_ModSetGlobal) are values of their variables (OBJLIB 5).
 */
#include <string.h>

#include "scrap/game.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/world.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL scrapyard

#define MT "MapTriggers"
#define AM "Mission_Ambush"
#define CG "Mission_DefeatCrazyGambler2"
#define FD "Mission_FindDtritusMatrix"
#define MB "Mission_MeetBerto"
#define MC "Mission_MosquitoClue"
#define SM "Mission_StupidMerc"
#define SA "Mission_StupidMercAgain"
#define TD "Mission_TalkToDeepThroat"

static const char *const ac_ships_std = "[\"SMerc1\",\"SMerc2\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SBetty1\",\"SArchbishop1\"]";
static const char *const ac_ships_all = "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SMerc3\",\"SLMerc\",\"SMayor1\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\",\"SArchbishop1\"]";

/* ---- native helpers of this file ---- */

static int Sy_CmpS(const nativeVal_t *a, const char *op, const char *s) {
	nativeVal_t b = { 0 };
	LvN_Str(&b, s);
	int r = Mode_Cmp(a, op, &b);
	Native_Free(&b);
	return r;
}

static int Sy_CmpNone(const nativeVal_t *a, const char *op) {
	nativeVal_t b = { 0 };
	Native_SetNone(&b);
	return Mode_Cmp(a, op, &b);
}

/* h.var into *out where the script reads it (out None before) */
static byte Sy_Get(const nativeVal_t *h, const char *var, nativeVal_t *out) {
	Native_SetNone(out);
	return Mode_VGet(h, var, out);
}

/* Vehicles.OnDeath(LooserName, WinnerName) after import Vehicles */
static byte Sy_OnDeath(const nativeVal_t *looser, const nativeVal_t *winner) {
	return Mode_Import("Vehicles") && PORT_CALL(Vehicles, OnDeath, NULL, "vv", looser, winner);
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, h.Del, ()) of a handle value (the bound method by reference;
 * OBJLIB 7: Python until the entities are C) */
static byte Sy_LaterDel(double delay, const nativeVal_t *h) {
	nativeVal_t del, at;
	byte ok = Mode_VBoundV(h, "Del", &del);
	if (ok) {
		Native_SetFloat(&at, Mode_Time() + delay);
		ok = Mode_ScheduleV(&at, &del, NULL, 0, NULL);
	}
	Native_Free(&del);
	return ok;
}

/* SVec.Mod(SVec.Sub(a.Pos, b.Pos)) into *out */
static byte Sy_Dist(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	nativeVal_t pa = { 0 }, pb = { 0 }, d = { 0 };
	Native_SetNone(&pa);
	Native_SetNone(&pb);
	Native_SetNone(&d);
	Native_SetNone(out);
	byte ok = Mode_VGet(a, "Pos", &pa) && Mode_VGet(b, "Pos", &pb) && Mode_VecSub(&pa, &pb, &d) && Mode_VecMod(&d, out);
	Native_Free(&pa);
	Native_Free(&pb);
	Native_Free(&d);
	return ok;
}

/* SVec.ModSqr(SVec.Sub(a, b.Pos)) into *out */
static byte Sy_DistSqr(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	nativeVal_t pb = { 0 }, d = { 0 };
	Native_SetNone(&pb);
	Native_SetNone(&d);
	Native_SetNone(out);
	byte ok = Mode_VGet(b, "Pos", &pb) && Mode_VecSub(a, &pb, &d) && Mode_VecModSqr(&d, out);
	Native_Free(&pb);
	Native_Free(&d);
	return ok;
}

/* SVec.ModSqr(SVec.Sub(usr.Pos, Scrap.GetEntity("DM_Door_Elevator_GamblinDen").Pos)) */
static byte Sy_DistDenSqr(const nativeVal_t *usr, nativeVal_t *out) {
	nativeVal_t up = { 0 }, door = { 0 };
	Native_SetNone(&up);
	Native_SetNone(&door);
	Native_SetNone(out);
	byte ok = Mode_VGet(usr, "Pos", &up) && LvN_Entity("DM_Door_Elevator_GamblinDen", &door) && Sy_DistSqr(&up, &door, out);
	Native_Free(&up);
	Native_Free(&door);
	return ok;
}

/* SAI.SetStateVehicle(state, name) and SAI.SetStateVehicle(1, name, pos[0], pos[1], pos[2], ...); tail "radius" passes
 * radius, else 3000 and the again flag (1) or 0, 0.25 */
static byte Sy_State(long state, const nativeVal_t *name) {
	return PORT_CALL(SAI, SetStateVehicle, NULL, "iv", state, name);
}

static byte Sy_StateTo(const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *radius, byte again) {
	nativeVal_t c[3] = { 0 };
	int n = 0;
	byte ok = 1;
	for (; ok && n < 3; n++) {
		ok = LvN_Item(pos, n, &c[n]);
	}
	if (ok) {
		ok = radius ? PORT_CALL(SAI, SetStateVehicle, NULL, "ivvvvv", 1L, name, &c[0], &c[1], &c[2], radius) :
			again ? PORT_CALL(SAI, SetStateVehicle, NULL, "ivvvvii", 1L, name, &c[0], &c[1], &c[2], 3000L, 1L) :
			PORT_CALL(SAI, SetStateVehicle, NULL, "ivvvviid", 1L, name, &c[0], &c[1], &c[2], 3000L, 0L, 0.25);
	}
	while (n-- > 0) {
		Native_Free(&c[n]);
	}
	return ok;
}

/* SWeap.GetFAmmo(0, "Max"): a float, None when the table has no such value */
static void Sy_AmmoMax(nativeVal_t *out) {
	float value;
	if (!cAmmoInfo_GetF(TABLE_AT(g_pWorld->ammo, 0), "Max", &value)) {
		Native_SetNone(out);
		return;
	}
	Native_SetFloat(out, (double)value);
}

/* SScorer.AddModel(0, name, model, "", 4, radius[, height]) (the head of a caller) */
static void Sy_AddHead(const char *name, const char *model, float radius, float height) {
	cScorer *sc = Scorer_Get(0);
	if (sc) {
		cScorer_AddModel(sc, name, model, NULL, 4, radius, height, 0);
	}
}

/* Scrap.StartDummySearch(name) of a value (flag 1, the TypeError of "s") */
static byte Sy_DummyStart(const nativeVal_t *name) {
	const char *s;
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	Mode_DummyStart(s, 1);
	return 1;
}

/* `usr and name == usr.Name` of the triggers (MapTriggers' order of the compare) */
static int Sy_UsrIs(const nativeVal_t *usr, const nativeVal_t *name) {
	nativeVal_t n = { 0 };
	if (!Native_True(usr)) {
		return 0;
	}
	int r = Sy_Get(usr, "Name", &n) ? Mode_Cmp(name, "==", &n) : -1;
	Native_Free(&n);
	return r;
}

/* Scrap.GetNearestParked(1, pos): (name, (x, y, z)), ("", (0, 0, 0)) when none; a pos that is no three numbers goes to
 * the engine function through its ref for its TypeError */
static byte Sy_NearestParked(const nativeVal_t *pos, nativeVal_t *out) {
	double x[3];
	if (!Mode_VecOf(pos, x)) {
		return PORT_CALL(Scrap, GetNearestParked, out, "iv", 1L, pos);
	}
	/* "|i(fff)" */
	vec3_t p = { (float)x[0], (float)x[1], (float)x[2] }, o = p;
	const char *name = Parking_Find(&g_pWorld->logic.parking, 1, &p, &o, 0);
	if (!name) {
		o.x = o.y = o.z = 0.0f;
	}
	Native_SetEmpty(out, NV_TUPLE);
	LvN_Str(Native_Append(out), name ? name : "");
	nativeVal_t *v = Native_Append(out);
	Native_SetEmpty(v, NV_TUPLE);
	Native_SetFloat(Native_Append(v), (double)o.x);
	Native_SetFloat(Native_Append(v), (double)o.y);
	Native_SetFloat(Native_Append(v), (double)o.z);
	return 1;
}

/* the native lines of LevelCommon's frame helpers (Lv_X) this file needs */

/* raise v of the script after import module (the do_raise of ceval without value and traceback): a text is a string
 * exception, a tuple raises its first item, a class or an instance (OBJLIB 7: Python objects) as Python raises them,
 * any other value the TypeError; 0 */
static byte Sy_RaiseValue(const char *module, const nativeVal_t *v) {
	if (!Mode_Import(module)) {
		return 0;
	}
	while (v->type == NV_TUPLE && v->n > 0) {
		v = &v->items[0];
	}
	if (v->type == NV_STR) {
		PyObject *s = Native_ToPy(v);
		if (s) {
			PyErr_SetObject(s, Py_None);
			Py_DECREF(s);
		}
		return 0;
	}
	if (!Native_IsObj(v)) {
		return Native_Raise(NE_TYPE, "exceptions must be strings, classes, or instances");
	}
	/* OBJLIB 7: the exception is a class or an instance of Python (the error state is Python's until stage 7d) */
	PyObject *type = Script_Ref(Native_PyOf(v)), *value = Script_Ref(Py_None), *tb = NULL;
	while (PyTuple_Check(type) && PyTuple_Size(type) > 0) {
		PyObject *tmp = type;
		type = Script_Ref(PyTuple_GET_ITEM(type, 0));
		Py_DECREF(tmp);
	}
	if (PyClass_Check(type)) {
		PyErr_NormalizeException(&type, &value, &tb);
	} else if (PyInstance_Check(type)) {
		Py_DECREF(value);
		value = type;
		type = Script_Ref((PyObject *)((PyInstanceObject *)type)->in_class);
	} else if (!PyString_Check(type)) {
		Py_DECREF(type);
		Py_DECREF(value);
		return Native_Raise(NE_TYPE, "exceptions must be strings, classes, or instances");
	}
	PyErr_Restore(type, value, tb);
	return 0;
}

/*
 * UsePhoneCab(UsrName, CharName, optId, FullOption) of a mission: the audio com of the phone cab (the cab is the
 * global pc), the police warning of objWho while the alarm is on, else the NI talk of niWho with the end function of
 * module (end: its ref, LvN_OnEndRef); activate: the cab stops being the target first; resetUse: the cab gets its default
 * use after the talk starts
 */
typedef struct {
	const char *police, *objWho;
	const portRef_t *end;
	const char *niWho, *ni;
	byte activate, resetUse;
} syPhone_t;

static byte Sy_UsePhone(const nativeVal_t *a, const char *module, const syPhone_t *p) {
	nativeVal_t pc = { 0 }, e = { 0 };
	Native_SetNone(&pc);
	Native_SetNone(&e);
	byte ok = Mode_Import("PhoneCab") && LvN_Speakers(&a[0], &a[1]) &&
		PORT_CALL(PhoneCab, UseAudioCom, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]) && Mode_EntityH(&a[1], &e) &&
		Mode_Put(module, "pc", NULL, &e);
	int alarm = ok ? Mode_AlarmOn() : -1;
	if (alarm > 0) {
		ok = Mode_G(module, "pc", &pc) && PORT_CALL(Speech, StartObjSpch, NULL, "vss", &pc, p->police, p->objWho);
	} else if (alarm == 0) {
		ok = (!p->activate || (Mode_G(module, "pc", &pc) && PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", &pc, 0L))) &&
			LvN_OnEndRef(p->end, NULL) && Mode_NISpeech(p->niWho, p->ni);
		if (ok && p->resetUse) {
			Native_Free(&e);
			ok = Mode_EntityH(&a[1], &e) && LvN_SetS(&e, "UseFunc", "PhoneCab.DefUseAudioCom");
		}
	} else {
		ok = 0;
	}
	Native_Free(&pc);
	Native_Free(&e);
	return ok;
}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors, the scrapyard, the police; the traffic by who controls the robots, the metro and
 * the elevator */
static byte Map_InitStatusN(void) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "OutPolice", "Metro", "Traffic", "Parking", NULL };
	static const char *const normal[] = { "TLight1", "TLight2", "TLight3", NULL };
	static const char *const medium[] = { "TMed1", "TMed2", "TMed3", NULL };
	static const char *const heavy[] = { "THeavy1", "THeavy2", "THeavy3", NULL };
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	if (!LvN_ModSetI("Init", "Outdoor", 1) || !LvN_ModSetI("Init", "isScrapYard", 1) || !LvN_ModSetI("OutPolice", "PoliceActive", 1)) {
		return 0;
	}
	if (LvN_SaveIs("RobotsControlledByBoss", "1")) {
		if (!Mode_BossTraffic(0)) {
			return 0;
		}
	} else if (!Mode_GVarSetI("iTrafficNumber", 4) || !Mode_GVarSetI("MaxChasingShips", -1) || !LvN_ModTexts("Traffic", "AcNormalShips", normal) ||
		!LvN_ModSetI("Traffic", "NumNormal", 10) || !LvN_ModTexts("Traffic", "AcMediumShips", medium) || !LvN_ModSetI("Traffic", "NumMedium", 20) ||
		!LvN_ModTexts("Traffic", "AcHeavyShips", heavy) || !LvN_ModSetI("Traffic", "NumHeavy", 10)) {
		return 0;
	}
	return PORT_CALL(Elevator, SetDef, NULL, "ss", "Metro", "DM_Door_MetroExt_ScrapYard") &&
		PORT_CALL(Elevator, SetDef, NULL, "ss", "MetroExt", "DM_Door_MetroExt_ScrapYard") &&
		PORT_CALL(Elevator, SetDef, NULL, "ss", "Elevator", "DM_Door_Elevator_GamblinDen");
}

/* Init(LevelPath): the elevator to the gamblin den, the metro, the parkings of the lonely mercenary, the clouds */
static byte Map_InitN(void) {
	if (!Mode_Import("Elevator") || !Mode_Import("Metro") || !Mode_Import("Parking")) {
		return 0;
	}
	return Mode_Connect("DM_Door_Elevator_GamblinDen", "GamblinDen", 0) &&
		PORT_CALL(Metro, SetAsMetro, NULL, "s", "DM_Door_MetroExt_ScrapYard") &&
		(!LvN_SaveIs("LonelyMercLairActive", "1") || PORT_CALL(Parking, CreateLonelyMercParkings, NULL, "")) &&
		Mode_GVarSetI("CameraCloudRed", 77) && Mode_GVarSetI("CameraCloudGreen", 80) && Mode_GVarSetI("CameraCloudBlue", 60);
}

MODE_FN(Scrapyard, InitStatus, 1, Map_InitStatusN())
MODE_FN(Scrapyard, Init, 1, Map_InitN())

PyMethodDef g_ScrapyardMapMethods[] = {
	{ "InitStatus", Scrapyard_InitStatus, 1, NULL },
	{ "Init", Scrapyard_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapTriggers: the triggers that launch the mosquitos and the mad hunters from their lairs ---- */

static const char *const mt_madhunter_profile[] = { "Ship=MadHunt", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=7",
	"UsePercSonic=100", "UsePercCloud=0", "UsePercElectro=0", "UsePercHook=0", "FactorHookMap=0", "CadenceShoot=1", "ControlAim=1",
	"AngAttack=3.1415", "CounterAttack=1.0", "ControlBoost=1", "ControlBoostItemLife=0", "ControlBoostItemAmmo=0",
	"FactorDamageGetAway=1", "FactorDamageItemLife=0.15", "FactorItemAmmo=0.1", "ControlStrafeEvasion=0", "UsePercHookGetAway=0",
	"TimeMaxStrafe=2", "VelMaxStrafe=0.25", NULL };

static const char *const mt_mosquito_profile[] = { "Ship=SMOSQUITO", "ControlBrake=1", "CadenceShoot=100", "ControlAim=2",
	"AngAttack=0", "CounterAttack=0.30", "ControlBoost=0", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1",
	"FactorDamageGetAway=1.0", "FactorDamageItemLife=0", "FactorItemAmmo=0.25", "TimeMaxStrafe=2", "VelMaxStrafe=0.25", NULL };

static const char *const mt_mosquito_reprogram[] = { "FactorDamageGetAway=1.0", "CadenceShoot=75", "ControlAim=1", "TimeMaxStrafe=2",
	"VelMaxStrafe=0.25", NULL };

/* aiprof = VehiclesNPC.GetIAProf("SMosquito") (the profile object, OBJLIB 5) */
static byte Sy_MosquitoProf(nativeVal_t *aiprof) {
	Native_SetNone(aiprof);
	return PORT_CALL(VehiclesNPC, GetIAProf, aiprof, "s", "SMosquito");
}

/* Init(LevelPath): the mosquitos reprogrammed, the kills counted from now, the triggers of the inhabitants */
static byte MT_InitN(void) {
	nativeVal_t aiprof = { 0 }, n = { 0 };
	Native_SetNone(&aiprof);
	Native_SetNone(&n);
	byte ok = Mode_Import("VehiclesNPC") && Sy_MosquitoProf(&aiprof) && Mode_SetAttrs(&aiprof, mt_mosquito_reprogram) &&
		Mode_SaveInt("Stats.Mosquito", &n) && Mode_Put(MT, "numMosquitosKilled", NULL, &n);
	Native_Free(&aiprof);
	Native_Free(&n);
	int t = ok ? LvN_GTrue(MT, "WithScrapyardInhabitants") : -1;
	return t == 0 || (t > 0 && PORT_LOWN(MapTriggers, CreateMosquitosTriggers, NULL, "") &&
		PORT_LOWN(MapTriggers, CreateMadHuntersTriggers, NULL, ""));
}

/* CheckMosquitoCadenceShoot(): the mosquitos shoot faster after 20 kills (the branch of 40 is never reached) */
static byte MT_CheckMosquitoCadenceShootN(void) {
	nativeVal_t s = { 0 }, g = { 0 }, cur = { 0 }, aiprof = { 0 };
	Native_SetNone(&s);
	Native_SetNone(&cur);
	Native_SetNone(&aiprof);
	int c = Mode_Import("VehiclesNPC") && Mode_SaveInt("Stats.Mosquito", &s) && Mode_G(MT, "numMosquitosKilled", &g) &&
		Native_Arith('-', &s, &g, &cur) ? Mode_CmpI(&cur, ">", 20) : -1;
	if (c == 0) {
		c = Mode_CmpI(&cur, ">", 40);
		c = c > 0 ? 2 : c;
	}
	byte ok = c >= 0 && (c == 0 || (Sy_MosquitoProf(&aiprof) && LvN_SetI(&aiprof, "CadenceShoot", c == 2 ? 5 : 20)));
	Native_Free(&s);
	Native_Free(&g);
	Native_Free(&cur);
	Native_Free(&aiprof);
	return ok;
}

/* the triggers of a kind of lair: "DM_<kind>Trigger_<name>_<radius>" dummies become items "<kind>Trigger<name>", and
 * the dummy "DM_<kind>Spawn_<name>" of each is its lair in the global table lairs */
typedef struct {
	const char *kind, *lairs, *onTake, *repeated, *missing;
} mtTriggers_t;

static byte MT_CreateTriggersN(const mtTriggers_t *tr) {
	char text[0x40];
	nativeVal_t names = { 0 }, dummy = { 0 };
	if (!Mode_Import("string")) {
		return 0;
	}
	Native_SetEmpty(&names, NV_LIST);
	Str_Printf(text, sizeof(text), "DM_%sTrigger_*", tr->kind);
	LvN_Dummy(text, 1, &dummy);
	byte ok = 1;
	while (ok && Native_True(&dummy)) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *d0 = Mode_V(&t), *fields = Mode_V(&t), *f2 = Mode_V(&t), *f3 = Mode_V(&t), *pre = Mode_V(&t);
		nativeVal_t *name = Mode_V(&t), *p = Mode_V(&t), *l = Mode_V(&t), *r = Mode_V(&t);
		Str_Printf(text, sizeof(text), "%sTrigger", tr->kind);
		LvN_Str(pre, text);
		ok = LvN_Item(&dummy, 0, d0) && Native_Split(d0, "_", fields) && LvN_Item(fields, 2, f2) && Native_Arith('+', pre, f2, name) &&
			LvN_Item(&dummy, 1, p) && LvN_CreateAt(name, p, "Item", l) && LvN_Item(fields, 3, f3) && Native_ToInt(f3, r) &&
			Mode_VSet(l, "Radius", r) && LvN_SetS(l, "TgTypeMarker", "RGBK") && LvN_SetS(l, "OnTake", tr->onTake);
		if (ok) {
			Native_Free(f2);
			ok = LvN_Item(fields, 2, f2);
		}
		int in = ok ? Native_Contains(&names, f2) : -1;
		if (in == 0) {
			Native_Copy(Native_Append(&names), f2);
		} else if (in > 0) {
			ok = LvN_RaiseText(tr->repeated);
		} else {
			ok = 0;
		}
		Mode_Done(&t, ok);
		if (ok) {
			Native_Free(&dummy);
			Mode_DummyNext(&dummy);
		}
	}
	for (int k = 0; ok && k < names.n; k++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *pre = Mode_V(&t), *spawn = Mode_V(&t), *key = Mode_V(&t), *v = Mode_V(&t);
		const char *s;
		Str_Printf(text, sizeof(text), "DM_%sSpawn_", tr->kind);
		LvN_Str(pre, text);
		ok = Native_Arith('+', pre, &names.items[k], spawn) && Native_ArgStr(spawn, 1, &s);
		if (ok) {
			Mode_DummyStart(s, 0);
			Native_Free(&dummy);
			Mode_DummyNext(&dummy);
			if (Native_True(&dummy)) {
				Native_Free(pre);
				Str_Printf(text, sizeof(text), "%sTrigger", tr->kind);
				LvN_Str(pre, text);
				ok = LvN_Item(&dummy, 1, v) && Native_Arith('+', pre, &names.items[k], key) && Mode_DictPut(MT, tr->lairs, NULL, key, v);
			} else {
				ok = LvN_RaiseText(tr->missing);
			}
		}
		Mode_Done(&t, ok);
	}
	Native_Free(&names);
	Native_Free(&dummy);
	return ok;
}

static const mtTriggers_t mt_mosquitos = { "Mosquito", "mosquitosLairs", "MapTriggers.OnMosquitoTriggerTaken",
	"El nombre del trigger del mosquito est\xe1 repetido", "No existe guarida de mosquito asociada a su correspondiente trigger" };
static const mtTriggers_t mt_madhunters = { "MadHunter", "madHuntersLairs", "MapTriggers.OnMadHunterTriggerTaken",
	"El nombre del trigger del mad hunter est\xe1 repetido", "No existe guarida de mad hunter asociada a su correspondiente trigger" };

/* a trigger taken by the player or a ship allowed to: the list fills up to the maximum from the lair, one ship each
 * step seconds */
typedef struct {
	const char *allowed, *shipVar, *maxVar, *list, *create, *lairs;
	double step;
} mtTaken_t;

static byte MT_TriggerTakenN(const nativeVal_t *a, const mtTaken_t *tt) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *allowed = Mode_V(&t), *e = Mode_V(&t), *max = Mode_V(&t), *size = Mode_V(&t);
	nativeVal_t *n = Mode_V(&t), *timy = Mode_V(&t), *nm = Mode_V(&t), *step = Mode_V(&t);
	const nativeVal_t *item = &a[0], *ch = &a[1];
	int go = Mode_UsrHandle(usr) ? Sy_UsrIs(usr, ch) : -1;
	if (go == 0) {
		go = Mode_G(MT, tt->allowed, allowed) ? Native_Contains(allowed, ch) : -1;
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	int c = Mode_GVarSet(tt->shipVar, ch) && Mode_EntityH(item, e) && LvN_SetI(e, "rTime", 10) && Native_GVarGet(tt->maxVar, max) &&
		Mode_ListGet(tt->list, "Num", size) ? Mode_Cmp(size, "<", max) : -1;
	long count = 0;
	if (c > 0) {
		c = Native_Arith('-', max, size, n) && Mode_Range(n, &count) ? 1 : -1;
	}
	Native_SetFloat(timy, Mode_Time());
	LvN_Str(nm, "CreateMosquito");
	Native_SetFloat(step, tt->step);
	for (long k = 0; c > 0 && k < count; k++) {
		nativeVal_t lairs = { 0 }, lair = { 0 }, next = { 0 };
		Native_SetNone(&lair);
		Native_SetNone(&next);
		nativeVal_t fn;
		byte ok = Mode_RefG(MT, tt->create, &fn) && Mode_G(MT, tt->lairs, &lairs) && Native_Subscript(&lairs, item, &lair) &&
			Mode_ScheduleV(timy, &fn, &lair, 1, nm) && Native_Arith('+', timy, step, &next);
		Native_Free(&fn);
		Native_Free(&lairs);
		Native_Free(&lair);
		if (ok) {
			Native_Free(timy);
			*timy = next;
		}
		c = ok ? 1 : -1;
	}
	return Mode_Done(&t, c >= 0);
}

static const mtTaken_t mt_mosquito_taken = { "shipsAllowedToLaunchMosquitos", "MosquitosTriggeredShipName", "MaxMosquitos", "Mosquitos",
	"CreateMosquito", "mosquitosLairs", 0.25 };
static const mtTaken_t mt_madhunter_taken = { "shipsAllowedToLaunchMadHunters", "MadHuntersTriggeredShipName", "MaxMadHunters",
	"MadHunters", "CreateMadHunter", "madHuntersLairs", 0.5 };

/* a ship of a lair: at a free spawn point with its profile (GetMosquitoProfile or GetMadHunterProfile and LvN_StdAI,
 * the two lines of LvN_StdProfile), then moved to the lair; a mosquito checks the cadence of the mosquitos */
static byte MT_CreateShipN(const nativeVal_t *lair, const char *name, const char *list, long money, byte mosquito) {
	nativeVal_t o = { 0 }, pd = { 0 };
	Native_SetNone(&pd);
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_SpawnCar(name, 0, &o, NULL) &&
		(mosquito ? PORT_LOWN(MapTriggers, GetMosquitoProfile, &pd, "") : PORT_LOWN(MapTriggers, GetMadHunterProfile, &pd, "")) &&
		LvN_StdAI(&o, &pd) && LvN_SetS(&o, "EntityList", list) && LvN_SetS(&o, "TgType", "red") && Mode_VSet(&o, "Pos", lair) &&
		Mode_VSet(&o, "LLogicLairPos", lair) && LvN_SetI(&o, "Money", money) &&
		(!mosquito || PORT_LOWN(MapTriggers, CheckMosquitoCadenceShoot, NULL, ""));
	Native_Free(&o);
	Native_Free(&pd);
	return ok;
}

MODE_FN(MT, GetMadHunterProfile, 0, LvN_Profile(mt_madhunter_profile, out))
MODE_FN(MT, GetMosquitoProfile, 0, LvN_Profile(mt_mosquito_profile, out))
MODE_FN(MT, Init, 1, MT_InitN())
MODE_FN(MT, CheckMosquitoCadenceShoot, 0, MT_CheckMosquitoCadenceShootN())
MODE_FN(MT, CreateMosquitosTriggers, 0, MT_CreateTriggersN(&mt_mosquitos))
MODE_FN(MT, CreateMadHuntersTriggers, 0, MT_CreateTriggersN(&mt_madhunters))
MODE_FN(MT, OnMosquitoTriggerTaken, 2, MT_TriggerTakenN(a, &mt_mosquito_taken))
MODE_FN(MT, OnMadHunterTriggerTaken, 2, MT_TriggerTakenN(a, &mt_madhunter_taken))
MODE_FN(MT, CreateMosquito, 1, MT_CreateShipN(&a[0], "Mosquito", "Mosquitos", 25, 1))
MODE_FN(MT, CreateMadHunter, 1, MT_CreateShipN(&a[0], "MadHunter", "MadHunters", 100, 0))

PyMethodDef g_ScrapyardMapTriggersMethods[] = {
	{ "GetMadHunterProfile", MT_GetMadHunterProfile, 1, NULL },
	{ "GetMosquitoProfile", MT_GetMosquitoProfile, 1, NULL },
	{ "Init", MT_Init, 1, NULL },
	{ "CheckMosquitoCadenceShoot", MT_CheckMosquitoCadenceShoot, 1, NULL },
	{ "CreateMosquitosTriggers", MT_CreateMosquitosTriggers, 1, NULL },
	{ "OnMosquitoTriggerTaken", MT_OnMosquitoTriggerTaken, 1, NULL },
	{ "CreateMosquito", MT_CreateMosquito, 1, NULL },
	{ "CreateMadHuntersTriggers", MT_CreateMadHuntersTriggers, 1, NULL },
	{ "OnMadHunterTriggerTaken", MT_OnMadHunterTriggerTaken, 1, NULL },
	{ "CreateMadHunter", MT_CreateMadHunter, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_Ambush: call Traitorius, destroy the probes among the mad hunters, call Deep Throat ---- */

static const char *const am_madhunters_profile[] = { "Ship=MadHunt", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=7",
	"UsePercSonic=100", "UsePercCloud=0", "UsePercElectro=0", "UsePercHook=0", "FactorHookMap=0", "CadenceShoot=1", "ControlAim=1",
	"AngAttack=3.1415", "CounterAttack=1.0", "ControlBoost=1", "ControlBoostItemLife=0", "ControlBoostItemAmmo=0",
	"FactorDamageGetAway=1", "FactorDamageItemLife=0", "FactorItemAmmo=0.1", "ControlStrafeEvasion=0", "UsePercHookGetAway=0",
	"TimeMaxStrafe=2", "VelMaxStrafe=0.25", NULL };

/* InitStatus(LevelPath): the probes in the heavy traffic (none yet), no inhabitants */
static byte Ambush_InitStatusN(void) {
	static const char *const probe[] = { "TProbe", NULL };
	return Mode_Import("VehiclesNPC") && Mode_Import("Traffic") && Mode_Import("MapTriggers") &&
		LvN_ModTexts("Traffic", "AcHeavyShips", probe) && LvN_ModSetI("Traffic", "NumHeavy", 0) &&
		LvN_ModSetI("MapTriggers", "WithScrapyardInhabitants", 0) && LvN_AcShips(AM, ac_ships_std);
}

/* Init(LevelPath): the state of the save variable NumProbes: call Traitorius (-1), Deep Throat (-2), or the ambush */
static byte Ambush_InitN(void) {
	nativeVal_t n = { 0 };
	byte ok = Mode_SaveInt("NumProbes", &n) && Mode_Put(AM, "numProbes", NULL, &n);
	Native_Free(&n);
	int c = ok ? LvN_GEqI(AM, "numProbes", -1) : -1;
	if (c > 0) {
		return PORT_LOWN(Mission_Ambush, CallTraitorius, NULL, "");
	}
	c = c == 0 ? LvN_GEqI(AM, "numProbes", -2) : -1;
	if (c > 0) {
		return PORT_LOWN(Mission_Ambush, CallDeepThroat, NULL, "");
	}
	/* OBJLIB 6: LaunchAmbush is a section of mission_ambush.def */
	return c == 0 && LV_OWN(AM, "LaunchAmbush", NULL, "");
}

static const syPhone_t am_phone1 = { "Generics_StillPolice1_Functionary", "Traitorius", &PORT_LREF(Mission_Ambush, DeepThroatCom), "Functionary",
	"Mission_Ambush1_Traitorius_NI_1Msg", 1, 0 };
static const syPhone_t am_phone2 = { "Generics_StillPolice1_DeepThroat", "DeepThroat", &PORT_LREF(Mission_Ambush, SetNextMission), "DeepThroat",
	"Mission_TalkToDeepThroat2_ComDeepThroat_NI_1Msg", 0, 1 };

/* DeepThroatCom(): after Traitorius: Deep Throat's message, then the ambush (OBJLIB 6: PreLaunchAmbush is a section of
 * mission_ambush.def) */
static byte Ambush_DeepThroatComN(void) {
	return LvN_PcDefault(AM) && LvN_ModNone("Speech", "OnOutOfRange") &&
		LvN_Com("Mission_Ambush1_RemoteMessage1", "DeepThroat", AM, "PreLaunchAmbush", LvN_NoArgs());
}

/* IsValidPos(usrPos, pos): at least minPlayerDist from the player */
static byte Ambush_IsValidPosN(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t d = { 0 }, m = { 0 }, dist = { 0 };
	Native_SetNone(&d);
	Native_SetNone(&dist);
	int c = Mode_VecSub(&a[1], &a[0], &d) && Mode_VecMod(&d, &dist) && Mode_G(AM, "minPlayerDist", &m) ? Mode_Cmp(&dist, "<", &m) : -1;
	Native_Free(&d);
	Native_Free(&dist);
	Native_Free(&m);
	if (c >= 0) {
		Native_SetInt(out, c == 0);
	}
	return c >= 0;
}

/* CreateProbe(): a probe at a random visible point far from the player, a target */
static byte Ambush_CreateProbeN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *pos = Mode_V(&t), *p = Mode_V(&t), *life = Mode_V(&t), *name = Mode_V(&t);
	if (!Mode_Import("OutMap") || !Mode_Import("VehiclesNPC") || !Mode_Import("Traffic") || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	Mode_RandomVisPoint(pos);
	for (;;) {
		nativeVal_t up = { 0 }, r = { 0 };
		Native_SetNone(&up);
		Native_SetNone(&r);
		int ok = Mode_VGet(usr, "Pos", &up) && PORT_LOWN(Mission_Ambush, IsValidPos, &r, "vv", &up, pos) ? Native_True(&r) : -1;
		Native_Free(&up);
		Native_Free(&r);
		if (ok < 0) {
			return Mode_Done(&t, 0);
		}
		if (ok) {
			break;
		}
		Native_Free(pos);
		Mode_RandomVisPoint(pos);
	}
	byte ok = PORT_CALL(Traffic, SetPath, NULL, "") && PORT_CALL(VehiclesNPC, MakeAI, p, "ssii", "TProbe", "ProbeMos", 0L, 0L) &&
		PORT_CALL(Traffic, ResetPath, NULL, "") && LvN_SetS(p, "OnDeath", "Mission_Ambush.OnProbeDeath") &&
		Mode_VSet(p, "MovePos", pos) && Mode_VSet(p, "Pos", pos) && LvN_SetInts3(p, "MoveAng", 0, 0, 0) && LvN_SetInts3(p, "Ang", 0, 0, 0) &&
		LvN_SetF(p, "MoveTime", 0.1) && Mode_G(AM, "PROBE_LIFE", life) && Mode_VSet(p, "Life", life) && Mode_VSet(p, "MaxLife", life) &&
		Mode_VSet(p, "MinLife", life) && PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", p, 1L) && Mode_VGet(p, "Name", name) &&
		Mode_PrintV(name);
	return Mode_Done(&t, ok);
}

/* CreateMadHunter(): a mad hunter at a free spawn point in the enemies */
static byte Ambush_CreateMadHunterN(void) {
	nativeVal_t m = { 0 }, pd = { 0 };
	Native_SetNone(&pd);
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_SpawnCar("Traffic", 0, &m, NULL) &&
		PORT_LOWN(Mission_Ambush, GetMadHuntersProfile, &pd, "") && LvN_StdAI(&m, &pd) && LvN_SetS(&m, "EntityList", "Enemies") &&
		LvN_Life(&m, AM, "MADHUNTER_LIFE");
	Native_Free(&m);
	Native_Free(&pd);
	return ok;
}

/* CreateMadHunters(): two mad hunters per probe left */
static byte Ambush_CreateMadHuntersN(void) {
	nativeVal_t n = { 0 }, two = { 0 }, r = { 0 };
	long count;
	Native_SetInt(&two, 2);
	Native_SetNone(&r);
	byte ok = Mode_Import("VehiclesNPC") && Mode_ListSet("Enemies", "OnDeath", "Mission_Ambush.OnMadHunterDeath") &&
		Mode_G(AM, "numProbes", &n) && Mode_Arith('*', &n, &two, &r) && Mode_Range(&r, &count);
	Native_Free(&n);
	Native_Free(&r);
	for (long k = 0; ok && k < count; k++) {
		ok = PORT_LOWN(Mission_Ambush, CreateMadHunter, NULL, "");
	}
	return ok;
}

/* the entity of the first enemy (and of the next) leaves the list and dies */
static byte Am_Off(const nativeVal_t *m) {
	return LvN_SetS(m, "EntityList", "") && LvN_SetI(m, "Life", 0);
}

/* OnProbeDeath(LooserName, WinnerName): two mad hunters go with the probe; the next probe or the end */
static byte Ambush_OnProbeDeathN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *first = Mode_V(&t), *m1 = Mode_V(&t), *next = Mode_V(&t), *m2 = Mode_V(&t), *n = Mode_V(&t), *d = Mode_V(&t);
	nativeVal_t *r = Mode_V(&t), *pre = Mode_V(&t), *key = Mode_V(&t), *four = Mode_V(&t);
	if (!Sy_OnDeath(&a[0], &a[1]) || !Mode_GOpI(AM, "numProbes", '-', 1) || !Mode_ListGet("Enemies", "First", first) ||
		!Mode_EntityH(first, m1)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(m1)) {
		if (!Mode_VGet(m1, "NextInList", next) || !Mode_EntityH(next, m2) || (Native_True(m2) && !Am_Off(m2)) || !Am_Off(m1)) {
			return Mode_Done(&t, 0);
		}
	}
	int done = LvN_GEqI(AM, "numProbes", 0);
	if (done > 0) {
		return Mode_Done(&t, LV_COM("Mission_Ambush2_RemoteMessage4", "DeepThroat", Mission_Ambush, PreCallDeepThroat, LvN_NoArgs()));
	}
	if (done < 0 || !Mode_G(AM, "numProbes", n)) {
		return Mode_Done(&t, 0);
	}
	LvN_SaveRepr("NumProbes", n);
	Native_Free(n);
	Native_SetInt(four, 4);
	LvN_Str(pre, "Mission_Ambush2_RemoteMessage");
	byte ok = Mode_WayPoint() && PORT_LOWN(Mission_Ambush, CreateProbe, NULL, "") && Mode_G(AM, "numProbes", n) && Native_Arith('-', four, n, d);
	if (ok) {
		Mode_Repr(d, r);
		ok = Native_Arith('+', pre, r, key) && PORT_CALL(Speech, AddRemoteCom, NULL, "vs", key, "DeepThroat");
	}
	return Mode_Done(&t, ok);
}

/* PreCallDeepThroat(): the probes are done: call Deep Throat */
static byte Ambush_PreCallDeepThroatN(void) {
	if (!LvN_PutI(AM, "numProbes", -2)) {
		return 0;
	}
	SaveVar_Set("NumProbes", "-2");
	return Mode_MMDesc("Mission_TalkToDeepThroat2_MissionText1", 1L) && Mode_WayPoint() && PORT_LOWN(Mission_Ambush, CallDeepThroat, NULL, "");
}

static byte Ambush_SetNextMissionN(void) {
	if (!LvN_PcDefault(AM)) {
		return 0;
	}
	SaveVars_Delete("NumProbes");
	return Mode_Target("") &&
		LvN_Next("Mission_AttackTraitorius", "Outskirts", "Mission_AttackTraitorius_MissionText1") && Mode_WayPoint();
}

MODE_FN(Ambush, GetMadHuntersProfile, 0, LvN_Profile(am_madhunters_profile, out))
MODE_FN(Ambush, InitStatus, 1, Ambush_InitStatusN())
MODE_FN(Ambush, Init, 1, Ambush_InitN())
MODE_FN(Ambush, CallTraitorius, 0, Mode_PhoneCab("Mission_Ambush.UsePhoneCab", 1, NULL, NULL))
MODE_FN(Ambush, UsePhoneCab, 4, Sy_UsePhone(a, AM, &am_phone1))
MODE_FN(Ambush, DeepThroatCom, 0, Ambush_DeepThroatComN())
MODE_FN(Ambush, IsValidPos, 2, Ambush_IsValidPosN(a, out))
MODE_FN(Ambush, CreateProbe, 0, Ambush_CreateProbeN())
MODE_FN(Ambush, CreateMadHunter, 0, Ambush_CreateMadHunterN())
MODE_FN(Ambush, CreateMadHunters, 0, Ambush_CreateMadHuntersN())
MODE_FN(Ambush, OnProbeDeath, 2, Ambush_OnProbeDeathN(a))
MODE_FN(Ambush, PreCallDeepThroat, 0, Ambush_PreCallDeepThroatN())
MODE_FN(Ambush, CallDeepThroat, 0, Mode_PhoneCab("Mission_Ambush.UsePhoneCab2", 1, NULL, NULL))
MODE_FN(Ambush, UsePhoneCab2, 4, Sy_UsePhone(a, AM, &am_phone2))
MODE_FN(Ambush, SetNextMission, 0, Ambush_SetNextMissionN())

PyMethodDef g_ScrapyardAmbushMethods[] = {
	{ "GetMadHuntersProfile", Ambush_GetMadHuntersProfile, 1, NULL },
	{ "InitStatus", Ambush_InitStatus, 1, NULL },
	{ "Init", Ambush_Init, 1, NULL },
	{ "CallTraitorius", Ambush_CallTraitorius, 1, NULL },
	{ "UsePhoneCab", Ambush_UsePhoneCab, 1, NULL },
	{ "DeepThroatCom", Ambush_DeepThroatCom, 1, NULL },
	{ "IsValidPos", Ambush_IsValidPos, 1, NULL },
	{ "CreateProbe", Ambush_CreateProbe, 1, NULL },
	{ "CreateMadHunter", Ambush_CreateMadHunter, 1, NULL },
	{ "CreateMadHunters", Ambush_CreateMadHunters, 1, NULL },
	{ "OnProbeDeath", Ambush_OnProbeDeath, 1, NULL },
	{ "PreCallDeepThroat", Ambush_PreCallDeepThroat, 1, NULL },
	{ "CallDeepThroat", Ambush_CallDeepThroat, 1, NULL },
	{ "UsePhoneCab2", Ambush_UsePhoneCab2, 1, NULL },
	{ "SetNextMission", Ambush_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DefeatCrazyGambler2: kill the crazy gambler's lives among the waves of mosquitos ---- */

static const char *const cg_gambler_profile[] = { "Ship=SBANKMASTER1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=2",
	"UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25", "UsePercHook=10", "FactorHookMap=10", "CadenceShoot=13", "ControlAim=8",
	"AngAttack=2.7", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1",
	"FactorDamageGetAway=0.25", "FactorDamageItemLife=0.35", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=20",
	"TimeMaxStrafe=10", "VelMaxStrafe=1.0", NULL };

static const char *const cg_mosquito_profile[] = { "Ship=SMOSQUITO", "CadenceShoot=15", "ControlAim=3", "AngAttack=1.5423",
	"CounterAttack=0.50", "ControlBoost=0", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=1.0",
	"FactorDamageItemLife=0.25", "FactorItemAmmo=0.25", "TimeMaxStrafe=2", "VelMaxStrafe=0.25", NULL };

static byte Gambler_InitStatusN(void) {
	return Mode_Import("MapTriggers") && LvN_ModSetI("MapTriggers", "WithScrapyardInhabitants", 0) && LvN_AcShips(CG, ac_ships_all);
}

/* int(Scrap.GetSaveVar("NumCrazyGamblerLifes")) == CrazyGamblerLifesNextMission of a number: 1, 0, -1 */
static int Cg_LastLife(const nativeVal_t *lifes) {
	nativeVal_t g = { 0 };
	int r = Mode_G(CG, "CrazyGamblerLifesNextMission", &g) ? Mode_Cmp(lifes, "==", &g) : -1;
	Native_Free(&g);
	return r;
}

/* Init(LevelPath): the lives are done: the next mission; else the crazy gambler, the mosquitos after the message,
 * the taunts and the scorer */
static byte Gambler_InitN(void) {
	nativeVal_t lifes = { 0 };
	int done = Mode_SaveInt("NumCrazyGamblerLifes", &lifes) ? Cg_LastLife(&lifes) : -1;
	Native_Free(&lifes);
	if (done > 0) {
		/* OBJLIB 6: PreSetNextMission is a section of mission_defeatcrazygambler2.def */
		return LV_OWN(CG, "PreSetNextMission", NULL, "");
	}
	if (done < 0) {
		return 0;
	}
	Sy_AddHead("EvilGambler", "Models/Heads/HeadEvilGambler/HeadEvilGambler", 20.0f, 0.0f);
	return Mode_ListSet("Enemies", "OnDeath", "Mission_DefeatCrazyGambler2.OnCrazyGamblerDeath") &&
		LV_COM("Mission_DefeatCrazyGambler2_RemoteMessage1", "DeepThroat", Mission_DefeatCrazyGambler2, CreateMosquitos, LvN_NoArgs()) &&
		PORT_LOWN(Mission_DefeatCrazyGambler2, CreateCrazyGambler, NULL, "") && Mode_Later(3.0, CG, "LaunchTaunts", NULL, 0, NULL) &&
		LV_SCORERG(Mission_DefeatCrazyGambler2, MissionScorer) && LvN_Draw();
}

/* LaunchTaunts(): every 12 s one of the five taunts while the crazy gambler lives */
static byte Gambler_LaunchTauntsN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t), *n = Mode_V(&t), *s = Mode_V(&t), *r = Mode_V(&t), *pre = Mode_V(&t);
	nativeVal_t *key = Mode_V(&t), *n1 = Mode_V(&t), *m = Mode_V(&t);
	if (!Mode_G(CG, "crazyGamblerName", name) || !Mode_EntityH(name, e)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(e)) {
		LvN_Str(pre, "Mission_DefeatCrazyGambler2_RemoteMessage");
		byte ok = Mode_G(CG, "numTaunt", n) && Native_Arith('+', Mode_I(&t, 2), n, s);
		if (ok) {
			Mode_Repr(s, r);
			Native_Free(n);
			ok = Native_Arith('+', pre, r, key) && PORT_CALL(Speech, RemoteComunication, NULL, "vs", key, "EvilGambler") &&
				Mode_G(CG, "numTaunt", n) && Native_Arith('+', n, Mode_I(&t, 1), n1) && Mode_Arith('%', n1, Mode_I(&t, 5), m) &&
				Mode_Put(CG, "numTaunt", NULL, m);
		}
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_Later(12.0, CG, "LaunchTaunts", NULL, 0, NULL));
}

/* CreateMosquito(): a mosquito at the nearest free spawn point; every other one drops nothing */
static byte Gambler_CreateMosquitoN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *o1 = Mode_V(&t), *pd = Mode_V(&t), *ammo = Mode_V(&t), *g = Mode_V(&t), *r = Mode_V(&t), *name = Mode_V(&t);
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_SpawnCar("Traffic", 1, o1, NULL) &&
		PORT_LOWN(Mission_DefeatCrazyGambler2, GetMosquitoProfile, pd, "") && LvN_StdAI(o1, pd) && LvN_SetS(o1, "EntityList", "Enemies");
	if (ok) {
		Sy_AmmoMax(ammo);
		ok = Mode_VSet(o1, "Ammo00", ammo) && LvN_Life(o1, CG, "MOSQUITO_LIFE") && Mode_G(CG, "givesSomething", g) &&
			Native_Arith('-', Mode_I(&t, 1), g, r) && Mode_Put(CG, "givesSomething", NULL, r);
	}
	int gives = ok ? LvN_GTrue(CG, "givesSomething") : -1;
	ok = gives >= 0 && (gives || LvN_SetI(o1, "Money", -1)) && Mode_VGet(o1, "Name", name) && Mode_Append(CG, "mosquitos", name);
	return Mode_Done(&t, ok);
}

/* CreateMosquitos(): the mosquitos up to the maximum, again every timeMosquitosNewWave seconds */
static byte Gambler_CreateMosquitosN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *max = Mode_V(&t), *lst = Mode_V(&t), *len = Mode_V(&t), *n = Mode_V(&t), *wave = Mode_V(&t), *when = Mode_V(&t);
	nativeVal_t *now = Mode_V(&t), *step = Mode_V(&t);
	long count = 0;
	int size = Mode_G(CG, "maxMosquitos", max) && Mode_G(CG, "mosquitos", lst) ? Native_Size(lst) : -1;
	if (size >= 0) {
		Native_SetInt(len, size);
	}
	byte ok = size >= 0 && Native_Arith('-', max, len, n) && Mode_Range(n, &count);
	Native_SetFloat(step, 0.2);
	for (long k = 0; ok && k < count; k++) {
		nativeVal_t i = { 0 }, d = { 0 }, time = { 0 };
		Native_SetInt(&i, k);
		Native_SetFloat(now, Mode_Time());
		Native_SetNone(&d);
		Native_SetNone(&time);
		ok = Mode_Arith('*', &i, step, &d) && Native_Arith('+', now, &d, &time) && Mode_AtG(&time, CG, "CreateMosquito", NULL, 0);
		Native_Free(&d);
		Native_Free(&time);
	}
	Native_SetFloat(now, Mode_Time());
	ok = ok && Mode_G(CG, "timeMosquitosNewWave", wave) && Native_Arith('+', now, wave, when) &&
		Mode_AtG(when, CG, "CreateMosquitos", NULL, 0);
	return Mode_Done(&t, ok);
}

/* CreateCrazyGambler(): the crazy gambler at the nearest free spawn point, the target */
static byte Gambler_CreateCrazyGamblerN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *h = Mode_V(&t), *pd = Mode_V(&t), *pilot = Mode_V(&t), *name = Mode_V(&t), *g = Mode_V(&t);
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_SpawnCar("CrazyGambler", 1, h, NULL) &&
		PORT_LOWN(Mission_DefeatCrazyGambler2, GetCrazyGamblerProfile, pd, "") &&
		LvN_StdAI(h, pd) && LvN_SetI(h, "LaunchTaunts", 0) && Mode_G(CG, "pilotName", pilot) &&
		Mode_VSet(h, "MPil0", pilot) && LvN_SetS(h, "TgType", "red") && LvN_SetS(h, "EntityList", "Enemies") &&
		LvN_Life(h, CG, "CRAZY_GAMBLER_LIFE") && Mode_VGet(h, "Name", name) && Mode_Put(CG, "crazyGamblerName", NULL, name) &&
		Mode_G(CG, "crazyGamblerName", g) && LvN_Target(g);
	return Mode_Done(&t, ok);
}

/* OnCrazyGamblerDeath(LooserName, WinnerName): one life fewer (the next mission or the gambler again), or a mosquito
 * less */
static byte Gambler_OnCrazyGamblerDeathN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *lifes = Mode_V(&t), *left = Mode_V(&t), *lst = Mode_V(&t);
	int gambler = Sy_OnDeath(&a[0], &a[1]) && Mode_G(CG, "crazyGamblerName", name) ? Mode_Cmp(&a[0], "==", name) : -1;
	if (gambler > 0) {
		Scorer_CancelSpeech(0.0f);
		int done = Mode_SaveInt("NumCrazyGamblerLifes", lifes) && Native_Arith('-', lifes, Mode_I(&t, 1), left) ? 1 : -1;
		if (done > 0) {
			LvN_SaveRepr("NumCrazyGamblerLifes", left);
			done = Cg_LastLife(left);
		}
		byte ok = done > 0 ? LvN_ScorerOff() && Mode_Later(7.0, CG, "PreSetNextMission", NULL, 0, NULL) :
			done == 0 && Mode_Later(3.0, CG, "CreateCrazyGambler", NULL, 0, NULL);
		return Mode_Done(&t, ok && LvN_Draw());
	}
	int in = gambler == 0 && Mode_G(CG, "mosquitos", lst) ? Native_Contains(lst, &a[0]) : -1;
	return Mode_Done(&t, in == 0 || (in > 0 && Mode_Remove(CG, "mosquitos", &a[0])));
}

/* the text item of the mission scorer with the text */
static byte Cg_Text(const nativeVal_t *id, const char *item, long dy, const nativeVal_t *text) {
	return LvN_ScorerTextBegin(id, item, dy) && LvN_ScorerSet(id, item, "Text", text) && LvN_ScorerTextEnd(id, item);
}

/* MissionScorer(id): the lives left */
static byte Gambler_MissionScorerN(const nativeVal_t *id) {
	nativeVal_t label = { 0 }, lifes = { 0 };
	if (!Mode_Import("Scorer")) {
		return 0;
	}
	LvN_Label("Mission_DefeatCrazyGambler2_Scorer1", &label);
	Native_SetNone(&lifes);
	byte ok = Cg_Text(id, "RemainingLifesText", 0, &label);
	if (ok) {
		Mode_SaveVar("NumCrazyGamblerLifes", &lifes);
		ok = Cg_Text(id, "RemainingLifes", 20, &lifes);
	}
	Native_Free(&label);
	Native_Free(&lifes);
	return ok;
}

MODE_FN(Gambler, GetCrazyGamblerProfile, 0, LvN_Profile(cg_gambler_profile, out))
MODE_FN(Gambler, GetMosquitoProfile, 0, LvN_Profile(cg_mosquito_profile, out))
MODE_FN(Gambler, InitStatus, 1, Gambler_InitStatusN())
MODE_FN(Gambler, Init, 1, Gambler_InitN())
MODE_FN(Gambler, LaunchTaunts, 0, Gambler_LaunchTauntsN())
MODE_FN(Gambler, CreateMosquito, 0, Gambler_CreateMosquitoN())
MODE_FN(Gambler, CreateMosquitos, 0, Gambler_CreateMosquitosN())
MODE_FN(Gambler, CreateCrazyGambler, 0, Gambler_CreateCrazyGamblerN())
MODE_FN(Gambler, OnCrazyGamblerDeath, 2, Gambler_OnCrazyGamblerDeathN(a))
MODE_FN(Gambler, MissionScorer, 1, Gambler_MissionScorerN(&a[0]))

PyMethodDef g_ScrapyardDefeatCrazyGambler2Methods[] = {
	{ "GetCrazyGamblerProfile", Gambler_GetCrazyGamblerProfile, 1, NULL },
	{ "GetMosquitoProfile", Gambler_GetMosquitoProfile, 1, NULL },
	{ "InitStatus", Gambler_InitStatus, 1, NULL },
	{ "Init", Gambler_Init, 1, NULL },
	{ "LaunchTaunts", Gambler_LaunchTaunts, 1, NULL },
	{ "CreateMosquito", Gambler_CreateMosquito, 1, NULL },
	{ "CreateMosquitos", Gambler_CreateMosquitos, 1, NULL },
	{ "CreateCrazyGambler", Gambler_CreateCrazyGambler, 1, NULL },
	{ "OnCrazyGamblerDeath", Gambler_OnCrazyGamblerDeath, 1, NULL },
	{ "MissionScorer", Gambler_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FindDtritusMatrix: take the lonely mercenary's ship into his lair, photograph the plans, take the
 * matrix ---- */

static const char *const fd_lonelymerc_profile[] = { "Ship=SLMERC", "CadenceShootCM=0", "UsePercSonic=50", "UsePercCloud=25",
	"UsePercElectro=25", "UsePercHook=100", "FactorHookMap=10", "CadenceShoot=7", "ControlAim=10", "AngAttack=0.5", "CounterAttack=1",
	"ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.45", "FactorDamageItemLife=0.35",
	"FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=80", "TimeMaxStrafe=10", "VelMaxStrafe=1.0", NULL };

/* SetLonelyMercProfile(): the profile, also VehiclesNPC.AIProf[sp.Ship] (OBJLIB 5, 7: the profile objects of
 * VehiclesNPC) */
static byte Dtritus_SetLonelyMercProfileN(nativeVal_t *out) {
	nativeVal_t ship = { 0 };
	Native_SetNone(&ship);
	byte ok = LvN_Profile(fd_lonelymerc_profile, out) && Mode_VGet(out, "Ship", &ship) && Mode_ModDictPut("VehiclesNPC", "AIProf", &ship, out);
	Native_Free(&ship);
	return ok;
}

static byte Dtritus_InitStatusN(void) {
	if (!LvN_AcShips(FD, ac_ships_std)) {
		return 0;
	}
	SaveVar_Set("LonelyMercLairActive", "0");
	SaveVar_Set("LonelyMercActive", "0");
	return 1;
}

/* e.Active = v for the entities of the names in the global list (missing ones skipped) */
static byte Fd_Active(const char *list, long v) {
	nativeVal_t lst = { 0 };
	int n = Mode_G(FD, list, &lst) ? Native_ForCount(&lst) : -1;
	byte ok = n >= 0;
	for (int k = 0; ok && k < n; k++) {
		nativeVal_t p = { 0 }, e = { 0 };
		Native_SetNone(&p);
		Native_SetNone(&e);
		ok = Native_SeqItem(&lst, k, &p) && Mode_EntityH(&p, &e) && (!Native_True(&e) || LvN_SetI(&e, "Active", v));
		Native_Free(&p);
		Native_Free(&e);
	}
	Native_Free(&lst);
	return ok;
}

/* Init(LevelPath): the plans and the matrix, the lair's parkings off; the player in a lonely mercenary ship of his own
 * goes on with it, else the ship waits at the gamblin den's hangar */
static byte Dtritus_InitN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *model = Mode_V(&t), *lp = Mode_V(&t), *usr = Mode_V(&t), *ps = Mode_V(&t), *m = Mode_V(&t), *up = Mode_V(&t);
	nativeVal_t *n = Mode_V(&t), *psn = Mode_V(&t);
	/* OBJLIB 6: CreateMissionPlans is a section of mission_finddtritusmatrix.def */
	byte ok = Mode_Import("Parking") && Mode_G(FD, "MatrixModel", model) && Mode_Preload(model) &&
		LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", FD, "TestForLonelyMercShip") &&
		LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", FD, "OutOfLonelyMercShip") && LV_OWN(FD, "CreateMissionPlans", NULL, "") &&
		PORT_LOWN(Mission_FindDtritusMatrix, CreateDtritusMatrix, NULL, "") && PORT_CALL(Parking, CreateLonelyMercParkings, lp, "") &&
		Mode_Put(FD, "lonelyMercParkings", NULL, lp) && Fd_Active("lonelyMercParkings", 0) && Mode_UsrHandle(usr);
	int own = ok ? Native_True(usr) : -1;
	if (own > 0) {
		own = Mode_VIsType(usr, "Car");
	}
	if (own > 0) {
		/* OBJLIB 5: Parking.PlayerShip is a handle of Parking */
		own = LvN_ModGet("Parking", "PlayerShip", ps) ? Native_True(ps) : -1;
	}
	if (own > 0) {
		int merc = Mode_VGet(usr, "Model", m) && Native_Upper(m, up) ? LvN_EqS(up, "SMERC3") : -1;
		if (merc > 0) {
			Native_Free(ps);
			merc = Mode_VGet(usr, "Name", n) && LvN_ModGet("Parking", "PlayerShip", ps) && Mode_VGet(ps, "Name", psn) ?
				Mode_Cmp(n, "!=", psn) : -1;
		}
		if (merc > 0) {
			Native_Free(n);
			ok = Mode_VGet(usr, "Name", n) && Mode_Put(FD, "lonelyMercShipName", NULL, n) &&
				PORT_LOWN(Mission_FindDtritusMatrix, CreateBadGuy, NULL, "") &&
				PORT_LOWN(Mission_FindDtritusMatrix, CheckLonelyMercShipExist, NULL, "");
			return Mode_Done(&t, ok);
		}
		own = merc;
	}
	ok = own >= 0 && PORT_LOWN(Mission_FindDtritusMatrix, CheckCreateBadGuy, NULL, "") &&
		PORT_LOWN(Mission_FindDtritusMatrix, CreateLonelyMercShip, NULL, "");
	return Mode_Done(&t, ok);
}

/* the save variable LonelyMercActive is "0": Deep Throat's message key */
static byte Fd_Com(const char *key) {
	return !LvN_SaveIs("LonelyMercActive", "0") || LvN_Com(key, "DeepThroat", NULL, NULL, NULL);
}

/* CreateLonelyMercShip(): the ship parked nearest the gamblin den's hangar becomes the lonely mercenary's (invulnerable,
 * the target) */
static byte Dtritus_CreateLonelyMercShipN(void) {
	modeVals_t t = { .n = 0 };
	/* ent and ppos follow each other: the two values of the unpack */
	nativeVal_t *last = Mode_V(&t), *door = Mode_V(&t), *pos = Mode_V(&t), *np = Mode_V(&t), *ent = Mode_V(&t), *ppos = Mode_V(&t);
	nativeVal_t *e = Mode_V(&t), *ship = Mode_V(&t), *name = Mode_V(&t), *cam = Mode_V(&t);
	int c = Mode_G(FD, "lastShipUsed", last) ? Sy_CmpS(last, "!=", "") : -1;
	byte ok = c >= 0 && (c == 0 || Fd_Com("Mission_FindDtritusMatrix1_RemoteMessage4")) &&
		LvN_Entity("DM_Door_Elevator_GamblinDen", door) && Mode_VGet(door, "Pos", pos) && Sy_NearestParked(pos, np) &&
		Mode_Unpack(np, 2, ent) && Mode_EntityH(ent, e) && (!Native_True(e) || Mode_VDel(e)) &&
		PORT_CALL(Parking, CreateParkedShip, ship, "vis", ppos, 0L, "LonelyMerc") &&
		LvN_SetS(ship, "OnDeath", "Mission_FindDtritusMatrix.OnLonelyMercShipDeath") && LvN_SetI(ship, "Invulnerable", 1) &&
		Mode_VGet(ship, "Name", name) && Mode_Put(FD, "lonelyMercShipName", NULL, name) && Mode_VGet(ship, "CamTgt", cam) &&
		Mode_Put(FD, "prevLonelyMercShipCameraPos", NULL, cam) && LvN_SetInts3(ship, "CamTgt", 0, 400, 0);
	if (ok) {
		Native_Free(name);
		ok = Mode_G(FD, "lonelyMercShipName", name) && LvN_Target(name) && PORT_LOWN(Mission_FindDtritusMatrix, CheckLonelyMercShipExist, NULL, "");
	}
	return Mode_Done(&t, ok);
}

static byte Dtritus_OnLonelyMercShipDeathN(const nativeVal_t *a) {
	return Sy_OnDeath(&a[0], &a[1]) && LvN_PutI(FD, "lonelyMercShipDestroyed", 1) && Fd_Com("Mission_FindDtritusMatrix1_RemoteMessage4");
}

/* the entity of the global lonelyMercShipName (a handle or None) */
static byte Fd_Ship(nativeVal_t *ship) {
	nativeVal_t name = { 0 };
	Native_SetNone(ship);
	byte ok = Mode_G(FD, "lonelyMercShipName", &name) && Mode_EntityH(&name, ship);
	Native_Free(&name);
	return ok;
}

/* CheckLonelyMercShipExist(): every second: a lost ship comes back */
static byte Dtritus_CheckLonelyMercShipExistN(void) {
	nativeVal_t e = { 0 };
	int t = Fd_Ship(&e) ? Native_True(&e) : -1;
	Native_Free(&e);
	if (t > 0) {
		return Mode_Later(1.0, FD, "CheckLonelyMercShipExist", NULL, 0, "CheckMercShip");
	}
	int kept = t == 0 ? LvN_GEqI(FD, "lonelyMercShipDestroyed", 0) : -1;
	if (kept > 0) {
		return !LvN_SaveIs("LonelyMercActive", "0") || (LvN_Com("Mission_FindDtritusMatrix1_RemoteMessage3", "DeepThroat", NULL, NULL, NULL) &&
			PORT_LOWN(Mission_FindDtritusMatrix, CreateLonelyMercShip, NULL, ""));
	}
	return kept == 0 && LvN_PutI(FD, "lonelyMercShipDestroyed", 0) && PORT_LOWN(Mission_FindDtritusMatrix, CreateLonelyMercShip, NULL, "");
}

/* TestForLonelyMercShip(): on boarding: the lonely mercenary's ship opens the lair (the parkings, the plans and the
 * matrix as targets); another ship closes the parkings unless the lair is open for good */
static byte Dtritus_TestForLonelyMercShipN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *un = Mode_V(&t), *ship = Mode_V(&t), *sn = Mode_V(&t), *prev = Mode_V(&t), *targets = Mode_V(&t);
	nativeVal_t *plans = Mode_V(&t), *dm = Mode_V(&t), *arrow = Mode_V(&t), *x = Mode_V(&t);
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	int mine = Mode_VGet(usr, "Name", un) && Mode_Put(FD, "lastShipUsed", NULL, un) && Fd_Ship(ship) ? Native_True(ship) : -1;
	if (mine > 0) {
		Native_Free(un);
		mine = Mode_G(FD, "lonelyMercShipName", sn) && Mode_VGet(usr, "Name", un) ? Mode_Cmp(sn, "==", un) : -1;
	}
	if (mine < 0) {
		return Mode_Done(&t, 0);
	}
	if (mine) {
		Native_SetEmpty(targets, NV_LIST);
		int n = Mode_G(FD, "prevLonelyMercShipCameraPos", prev) && Mode_VSet(ship, "CamTgt", prev) && LvN_SetI(ship, "Invulnerable", 0) &&
			PORT_LOWN(Mission_FindDtritusMatrix, ActivateMissionObjects, NULL, "i", 1L) && Fd_Active("lonelyMercParkings", 1) &&
			Mode_G(FD, "plans", plans) ? Native_ForCount(plans) : -1;
		for (int k = 0; k < n; k++) {
			Native_SeqItem(plans, k, Native_Append(targets));
		}
		int both = n >= 0 && Mode_G(FD, "dtritusMatrix", dm) ? Native_True(dm) : -1;
		if (both > 0) {
			both = Mode_G(FD, "dtritusMatrixArrow", arrow) ? Native_True(arrow) : -1;
		}
		if (both > 0) {
			Native_Free(dm);
			Native_Free(arrow);
			both = Mode_G(FD, "dtritusMatrix", dm) && Mode_VGet(dm, "Name", Native_Append(targets)) &&
				Mode_G(FD, "dtritusMatrixArrow", arrow) && Mode_VGet(arrow, "Name", x) ? 1 : -1;
			if (both > 0) {
				*Native_Append(targets) = *x;
				Native_SetNone(x);
			}
		}
		return Mode_Done(&t, both >= 0 && PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", targets));
	}
	return Mode_Done(&t, !LvN_SaveIs("LonelyMercLairActive", "0") || Fd_Active("lonelyMercParkings", 0));
}

/* CheckCreateBadGuy(): every second until the player is at the hangar doors: the bad guy comes 7 s later */
static byte Dtritus_CheckCreateBadGuyN(void) {
	nativeVal_t usr = { 0 }, d = { 0 };
	Native_SetNone(&d);
	int near = Mode_UsrHandle(&usr) ? Native_True(&usr) : -1;
	if (near > 0) {
		near = Sy_DistDenSqr(&usr, &d) ? Mode_CmpI(&d, "<=", 216160784) : -1;
	}
	Native_Free(&usr);
	Native_Free(&d);
	if (near > 0) {
		/* OBJLIB 6: BadGuyAppears is a section of mission_finddtritusmatrix.def */
		return LV_OWN(FD, "BadGuyAppears", NULL, "") && Mode_Later(7.0, FD, "CreateBadGuy", NULL, 0, NULL);
	}
	return near == 0 && Mode_Later(1.0, FD, "CheckCreateBadGuy", NULL, 0, "CheckCreateBadGuy");
}

/* OutOfLonelyMercShip(): leaving the lonely mercenary's ship: invulnerable only at the gamblin den's hangar; parked far
 * from the matrix it is the target again, else the lair stays open */
static byte Dtritus_OutOfLonelyMercShipN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *ship = Mode_V(&t), *sn = Mode_V(&t), *last = Mode_V(&t), *toDen = Mode_V(&t), *up = Mode_V(&t);
	nativeVal_t *arrow = Mode_V(&t), *d = Mode_V(&t), *cam = Mode_V(&t), *inv = Mode_V(&t);
	const long distSqr = 12000 * 12000;
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr)) {
		return Mode_Done(&t, 1);
	}
	int go = Fd_Ship(ship) ? Native_True(ship) : -1;
	if (go > 0) {
		go = Mode_G(FD, "lonelyMercShipName", sn) && Mode_G(FD, "lastShipUsed", last) ? Mode_Cmp(sn, "==", last) : -1;
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	int c = Sy_DistDenSqr(usr, toDen) ? Mode_CmpI(toDen, "<=", distSqr) : -1;
	if (c >= 0) {
		Native_SetInt(inv, c);
	}
	int far = c >= 0 && Mode_VSet(ship, "Invulnerable", inv) && Mode_VGet(usr, "Pos", up) && Mode_G(FD, "dtritusMatrixArrow", arrow) &&
		Sy_DistSqr(up, arrow, d) ? Mode_CmpI(d, ">", distSqr) : -1;
	if (far > 0) {
		/* a local of the script (no global statement): the camera of the ship is read and dropped */
		Native_Free(sn);
		byte ok = PORT_LOWN(Mission_FindDtritusMatrix, ActivateMissionObjects, NULL, "i", 0L) && Mode_VGet(ship, "CamTgt", cam) &&
			LvN_SetInts3(ship, "CamTgt", 0, 400, 0) && Mode_G(FD, "lonelyMercShipName", sn) && LvN_Target(sn);
		return Mode_Done(&t, ok);
	}
	if (far == 0) {
		SaveVar_Set("LonelyMercLairActive", "1");
		return Mode_Done(&t, Mode_DelSchedS("CheckMercShip"));
	}
	return Mode_Done(&t, 0);
}

/* `if (g): g.Active = status` of a global of the module */
static byte Fd_GActive(const char *name, const nativeVal_t *status) {
	nativeVal_t g = { 0 };
	int t = Mode_G(FD, name, &g) ? Native_True(&g) : -1;
	Native_Free(&g);
	byte ok = t == 0 || (t > 0 && Mode_G(FD, name, &g) && Mode_VSet(&g, "Active", status));
	if (t > 0) {
		Native_Free(&g);
	}
	return ok;
}

/* ActivateMissionObjects(status): the plans, the matrix and its arrow on or off */
static byte Dtritus_ActivateMissionObjectsN(const nativeVal_t *status) {
	nativeVal_t plans = { 0 };
	int n = Mode_G(FD, "plans", &plans) ? Native_ForCount(&plans) : -1;
	byte ok = n >= 0;
	for (int k = 0; ok && k < n; k++) {
		nativeVal_t name = { 0 }, plan = { 0 };
		Native_SetNone(&name);
		Native_SetNone(&plan);
		ok = Native_SeqItem(&plans, k, &name) && Mode_EntityH(&name, &plan) && (!Native_True(&plan) || Mode_VSet(&plan, "Active", status));
		Native_Free(&name);
		Native_Free(&plan);
	}
	Native_Free(&plans);
	return ok && Fd_GActive("dtritusMatrix", status) && Fd_GActive("dtritusMatrixArrow", status);
}

/* CreateBadGuy(): the bad guy (a lonely mercenary) at a free spawn point, moved to the nearest one */
static byte Dtritus_CreateBadGuyN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *bad = Mode_V(&t), *pd = Mode_V(&t), *ship = Mode_V(&t), *pos = Mode_V(&t);
	byte ok = Mode_Import("VehiclesNPC") && Mode_Import("OutMap") && Mode_SpawnCar("TheBadGuy", 0, bad, NULL) &&
		PORT_LOWN(Mission_FindDtritusMatrix, SetLonelyMercProfile, pd, "") && Mode_VGet(pd, "Ship", ship) &&
		PORT_CALL(VehiclesNPC, SetIAProf, NULL, "vv", bad, ship) &&
		PORT_CALL(OutMap, GetNearestFreeSpawnPos, pos, "") && (!Native_True(pos) || Mode_VSet(bad, "Pos", pos)) &&
		LvN_SetS(bad, "EntityList", "Enemies") && LvN_Life(bad, FD, "LONELY_MERC_LIFE");
	return Mode_Done(&t, ok);
}

static byte Dtritus_OnBadGuyDeathN(const nativeVal_t *a) {
	return Sy_OnDeath(&a[0], &a[1]) && Mode_Later(5.0, FD, "CreateBadGuy", NULL, 0, NULL);
}

/* CreatePlan(dummyName, exceptString): the plan of the dummy as a photo object (off until the lair opens) */
static byte Dtritus_CreatePlanN(const nativeVal_t *a) {
	static const char *const plans[][2] = { { "DM_Mission_TeslaPlan", "PlanoTesla" }, { "DM_Mission_LonelyMercShipPlan", "PlanoSMerc3" },
		{ "DM_Mission_LonelyMercEnginesPlan", "PlanoMMerc3" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t), *pln = Mode_V(&t), *name = Mode_V(&t);
	const nativeVal_t *dummyName = &a[0];
	if (!Sy_DummyStart(dummyName)) {
		return Mode_Done(&t, 0);
	}
	Mode_DummyNext(dummy);
	if (!Native_True(dummy)) {
		return Mode_Done(&t, Sy_RaiseValue(FD, &a[1]));
	}
	if (!LvN_DummyPosAng(dummy, pos, ang)) {
		return Mode_Done(&t, 0);
	}
	const char *photo = NULL;
	for (int i = 0; i < 3 && !photo; i++) {
		int eq = Sy_CmpS(dummyName, "==", plans[i][0]);
		if (eq < 0) {
			return Mode_Done(&t, 0);
		}
		photo = eq ? plans[i][1] : NULL;
	}
	if (!photo) {
		return Mode_Done(&t, Native_Raise(NE_NAME, "photoObjectName"));
	}
	byte ok = PORT_CALL(MissionsFuncs, CreatePhotoObject, pln, "svv", photo, pos, ang) &&
		PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", pln, 0L) && Mode_VGet(pln, "Name", name) && Mode_Append(FD, "plans", name);
	if (ok) {
		Native_Free(name);
		ok = Mode_VGet(pln, "Name", name) && Mode_DictPut(FD, "plansDictHelper", NULL, name, dummyName) && LvN_SetI(pln, "Active", 0);
	}
	return Mode_Done(&t, ok);
}

/* the end of the mission once the plans and the matrix are taken: the callbacks go */
static byte Fd_EndCallbacks(void) {
	return LvN_ModNone("Parking", "SwitchVehicleMissionCallback") && LvN_ModNone("Parking", "SwitchCharMissionCallback") &&
		LvN_ModNone("CharAct", "TakenPhoto");
}

/* CheckEndOfMission() of the module is true: 1, 0, -1 */
static int Fd_End(void) {
	nativeVal_t r = { 0 };
	Native_SetNone(&r);
	int t = PORT_LOWN(Mission_FindDtritusMatrix, CheckEndOfMission, &r, "") ? Native_True(&r) : -1;
	Native_Free(&r);
	return t;
}

/* Speech.ForceRemoteCom(key, "DeepThroat", AfterRemoteComFunc = SetNextMission): the global SetNextMission (a section of
 * mission_finddtritusmatrix.def, OBJLIB 6) is the function object of the keyword. Its lookup comes before the one of
 * Speech.ForceRemoteCom (the script looked that up first; the error differs only when both fail) */
static byte Fd_ForceCom(const char *key) {
	nativeVal_t then;
	byte ok = Mode_RefG(FD, "SetNextMission", &then) &&
		PORT_CALL(Speech, ForceRemoteCom, NULL, "ss;AfterRemoteComFunc=O", key, "DeepThroat", Native_PyOf(&then));
	Native_Free(&then);
	return ok;
}

/* TakePlanPhoto(plName): a photo of a plan: its blueprint into the hangar lists; the last thing taken ends the mission */
static byte Dtritus_TakePlanPhotoN(const nativeVal_t *name, nativeVal_t *out) {
	static const char *const bp[][4] = { { "DM_Mission_TeslaPlan", "Hangar.availableWeaponsList", "Tesla", "Mission_FindDtritusMatrix1_SystemMessage1" },
		{ "DM_Mission_LonelyMercShipPlan", "Hangar.shipsToEditList", "SMerc3", "Mission_FindDtritusMatrix1_SystemMessage2" },
		{ "DM_Mission_LonelyMercEnginesPlan", "Hangar.availableEnginesList", "MMERC3", "Mission_FindDtritusMatrix1_SystemMessage3" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *plans = Mode_V(&t), *e = Mode_V(&t), *helper = Mode_V(&t), *dummyName = Mode_V(&t);
	int in = Mode_Import("CharAct") && Mode_Import("RaceMaker") && Mode_Import("Parking") && Mode_G(FD, "plans", plans) ?
		Native_Contains(plans, name) : -1;
	if (in == 0) {
		Native_SetInt(out, 0);
	}
	if (in <= 0) {
		return Mode_Done(&t, in == 0);
	}
	byte ok = Mode_EntityH(name, e) && PORT_CALL(MissionsFuncs, UnsetPhotoObject, NULL, "v", e) && Mode_Remove(FD, "plans", name) &&
		Mode_G(FD, "plansDictHelper", helper) && Native_Subscript(helper, name, dummyName);
	for (int i = 0; ok && i < 3; i++) {
		int eq = Sy_CmpS(dummyName, "==", bp[i][0]);
		if (eq) {
			ok = eq > 0 && Mode_Blueprint(FD, bp[i][1], bp[i][2], bp[i][3], 1);
			break;
		}
	}
	int end = ok ? Fd_End() : -1;
	ok = end == 0 || (end > 0 && Fd_EndCallbacks() && Fd_ForceCom("Mission_FindDtritusMatrix1_RemoteMessage5"));
	if (ok) {
		Native_SetInt(out, 1);
	}
	return Mode_Done(&t, ok);
}

/* CreateDtritusMatrix(): Dtritus' matrix and its arrow at the dummy (off until the lair opens) */
static byte Dtritus_CreateDtritusMatrixN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t), *name = Mode_V(&t), *m = Mode_V(&t), *model = Mode_V(&t);
	nativeVal_t *aname = Mode_V(&t), *arrow = Mode_V(&t);
	LvN_Dummy("DM_Mission_DtritusMatrix", 1, dummy);
	if (!Native_True(dummy)) {
		return Mode_Done(&t, LvN_RaiseText("No existe el dummy de la matriz de Dtritus"));
	}
	LvN_Str(name, "DTritusMatrix");
	LvN_Str(aname, "MissionCheckArrow");
	byte ok = LvN_DummyPosAng(dummy, pos, ang) && LvN_CreateAt(name, pos, "Item", m) && Mode_Put(FD, "dtritusMatrix", NULL, m) &&
		Mode_G(FD, "MatrixModel", model) && Mode_VModel(m, model) && LvN_SetI(m, "Radius", 100) && LvN_SetI(m, "AppearsIfFree", 0) &&
		LvN_SetI(m, "AutoFace", 0) && LvN_SetI(m, "OnlyUser", 1) && LvN_SetF(m, "rTime", 0.25) &&
		LvN_SetS(m, "OnTake", "Mission_FindDtritusMatrix.TakeMatrix") && LvN_SetI(m, "Active", 0) &&
		LvN_CreateAt(aname, pos, "WithLife", arrow) && Mode_Put(FD, "dtritusMatrixArrow", NULL, arrow) &&
		LvN_SetInts3(arrow, "CamTgt", 0, 150, 0) && LvN_SetI(arrow, "Active", 0);
	return Mode_Done(&t, ok);
}

/* TakeMatrix(ItemName, CharName): the matrix taken where the arrow allows it; the last thing taken ends the mission */
static byte Dtritus_TakeMatrixN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *arrow = Mode_V(&t), *can = Mode_V(&t), *dm = Mode_V(&t);
	int go = Mode_Import("CharAct") && Mode_Import("Parking") && Mode_UsrHandle(usr) ? LvN_UsrNameIs(usr, &a[1]) : -1;
	if (go > 0) {
		go = Mode_G(FD, "dtritusMatrixArrow", arrow) ? Native_True(arrow) : -1;
	}
	if (go > 0) {
		go = Mode_VGet(arrow, "ArrowCanMission", can) ? Mode_CmpI(can, "==", 1) : -1;
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	byte ok = Mode_G(FD, "dtritusMatrix", dm) && LvN_SetI(dm, "rTime", 20);
	if (ok) {
		Native_Free(dm);
		ok = Mode_G(FD, "dtritusMatrix", dm) && Sy_LaterDel(5.0, dm);
	}
	if (ok) {
		Native_Free(arrow);
		ok = Mode_G(FD, "dtritusMatrixArrow", arrow) && Mode_VDel(arrow) && LvN_PutNone(FD, "dtritusMatrix") &&
			LvN_PutNone(FD, "dtritusMatrixArrow");
	}
	int end = ok ? Fd_End() : -1;
	/* OBJLIB 6: SetNextMission is a section of mission_finddtritusmatrix.def */
	ok = end == 0 || (end > 0 && Fd_EndCallbacks() &&
		LvN_Com("Mission_FindDtritusMatrix1_RemoteMessage5", "DeepThroat", FD, "SetNextMission", NULL));
	return Mode_Done(&t, ok);
}

/* CheckEndOfMission(): no matrix, no arrow, no plans left */
static byte Dtritus_CheckEndOfMissionN(nativeVal_t *out) {
	nativeVal_t v = { 0 };
	int r = Mode_G(FD, "dtritusMatrix", &v) ? Sy_CmpNone(&v, "==") : -1;
	Native_Free(&v);
	if (r > 0) {
		r = Mode_G(FD, "dtritusMatrixArrow", &v) ? Sy_CmpNone(&v, "==") : -1;
		Native_Free(&v);
	}
	if (r > 0) {
		int n = Mode_G(FD, "plans", &v) ? Native_Size(&v) : -1;
		Native_Free(&v);
		r = n < 0 ? -1 : n == 0;
	}
	if (r >= 0) {
		Native_SetInt(out, r);
	}
	return r >= 0;
}

MODE_FN(Dtritus, SetLonelyMercProfile, 0, Dtritus_SetLonelyMercProfileN(out))
MODE_FN(Dtritus, InitStatus, 1, Dtritus_InitStatusN())
MODE_FN(Dtritus, Init, 1, Dtritus_InitN())
MODE_FN(Dtritus, CreateLonelyMercShip, 0, Dtritus_CreateLonelyMercShipN())
MODE_FN(Dtritus, OnLonelyMercShipDeath, 2, Dtritus_OnLonelyMercShipDeathN(a))
MODE_FN(Dtritus, CheckLonelyMercShipExist, 0, Dtritus_CheckLonelyMercShipExistN())
MODE_FN(Dtritus, TestForLonelyMercShip, 0, Dtritus_TestForLonelyMercShipN())
MODE_FN(Dtritus, CheckCreateBadGuy, 0, Dtritus_CheckCreateBadGuyN())
MODE_FN(Dtritus, OutOfLonelyMercShip, 0, Dtritus_OutOfLonelyMercShipN())
MODE_FN(Dtritus, ActivateMissionObjects, 1, Dtritus_ActivateMissionObjectsN(&a[0]))
MODE_FN(Dtritus, CreateBadGuy, 0, Dtritus_CreateBadGuyN())
MODE_FN(Dtritus, OnBadGuyDeath, 2, Dtritus_OnBadGuyDeathN(a))
MODE_FN(Dtritus, CreatePlan, 2, Dtritus_CreatePlanN(a))
MODE_FN(Dtritus, TakePlanPhoto, 1, Dtritus_TakePlanPhotoN(&a[0], out))
MODE_FN(Dtritus, CreateDtritusMatrix, 0, Dtritus_CreateDtritusMatrixN())
MODE_FN(Dtritus, TakeMatrix, 2, Dtritus_TakeMatrixN(a))
MODE_FN(Dtritus, CheckEndOfMission, 0, Dtritus_CheckEndOfMissionN(out))

PyMethodDef g_ScrapyardFindDtritusMatrixMethods[] = {
	{ "SetLonelyMercProfile", Dtritus_SetLonelyMercProfile, 1, NULL },
	{ "InitStatus", Dtritus_InitStatus, 1, NULL },
	{ "Init", Dtritus_Init, 1, NULL },
	{ "CreateLonelyMercShip", Dtritus_CreateLonelyMercShip, 1, NULL },
	{ "OnLonelyMercShipDeath", Dtritus_OnLonelyMercShipDeath, 1, NULL },
	{ "CheckLonelyMercShipExist", Dtritus_CheckLonelyMercShipExist, 1, NULL },
	{ "TestForLonelyMercShip", Dtritus_TestForLonelyMercShip, 1, NULL },
	{ "CheckCreateBadGuy", Dtritus_CheckCreateBadGuy, 1, NULL },
	{ "OutOfLonelyMercShip", Dtritus_OutOfLonelyMercShip, 1, NULL },
	{ "ActivateMissionObjects", Dtritus_ActivateMissionObjects, 1, NULL },
	{ "CreateBadGuy", Dtritus_CreateBadGuy, 1, NULL },
	{ "OnBadGuyDeath", Dtritus_OnBadGuyDeath, 1, NULL },
	{ "CreatePlan", Dtritus_CreatePlan, 1, NULL },
	{ "TakePlanPhoto", Dtritus_TakePlanPhoto, 1, NULL },
	{ "CreateDtritusMatrix", Dtritus_CreateDtritusMatrix, 1, NULL },
	{ "TakeMatrix", Dtritus_TakeMatrix, 1, NULL },
	{ "CheckEndOfMission", Dtritus_CheckEndOfMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_MeetBerto and Mission_TalkToDeepThroat: a call from a phone cab ---- */

/* Init(LevelPath) of the two: (the head of the evil Berto,) a free phone cab as the target */
static byte Sy_PhoneInitN(const char *module, const char *use, byte berto) {
	nativeVal_t pc = { 0 };
	Native_SetNone(&pc);
	if (!Mode_Import("PhoneCab")) {
		return 0;
	}
	if (berto) {
		Sy_AddHead("EvilBerto", "Models/Heads/HeadEvilBerto/HeadEvilBerto", 25.0f, 10.0f);
	}
	byte ok = PORT_CALL(PhoneCab, GetFree, &pc, "") && Mode_Put(module, "pc", NULL, &pc) && LvN_TargetG(module, "pc") &&
		Mode_GSetS(module, "pc", "UseFunc", use);
	Native_Free(&pc);
	return ok;
}

static const syPhone_t mb_phone = { "Generics_StillPolice1_Berto", "Berto", &PORT_LREF(Mission_MeetBerto, SetNextMission), "DeepThroat",
	"Mission_MeetBerto_ComDeepThroat_NI_1Msg", 1, 0 };

/* module.var = MissionsFuncs.ReassignBertosPlayerTarget (the function object, OBJLIB 5) */
static byte Mb_Reassign(const char *var) {
	nativeVal_t fn = { 0 };
	byte ok = LvN_ModGet("MissionsFuncs", "ReassignBertosPlayerTarget", &fn) && Mode_SetModVar("Parking", var, &fn);
	Native_Free(&fn);
	return ok;
}

/* SetNextMission(): the Bertos come after the player from now on, Dtritus loses his matrix */
static byte Berto_SetNextMissionN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *profile = Mode_V(&t), *life = Mode_V(&t), *r = Mode_V(&t), *chars = Mode_V(&t), *dt = Mode_V(&t);
	const char *s;
	if (!Mode_Import("Parking") || !LvN_PcDefault(MB)) {
		return Mode_Done(&t, 0);
	}
	SaveVar_Set("CreateBertos", "1");
	if (!Mode_G(MB, "BERTO_PROFILE", profile) || !Native_ArgStr(profile, 2, &s)) {
		return Mode_Done(&t, 0);
	}
	SaveVar_Create("BertosProfile", s);
	if (!Mode_G(MB, "BERTO_LIFE", life)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(life, r);
	SaveVar_Create("BertosLife", Native_Text(r));
	LvN_Str(dt, "Dtritus");
	byte ok = PORT_CALL(MissionsFuncs, CreateBertos, NULL, "") && Mb_Reassign("SwitchVehicleMissionCallback") &&
		Mb_Reassign("SwitchCharMissionCallback") && LvN_Next("Mission_DestroyStupidus", "Commercial", "Mission_DestroyStupidus_MissionText1") &&
		Mode_WayPoint() && LvN_SaveList("Conversor.AvailableChars", MB, chars);
	int in = ok ? Native_Contains(chars, dt) : -1;
	ok = in >= 0 && (in == 0 || LvN_ListRemove(chars, dt));
	if (ok) {
		LvN_SaveRepr("Conversor.AvailableChars", chars);
	}
	return Mode_Done(&t, ok);
}

static const syPhone_t td_phone = { "Generics_StillPolice1_DeepThroat", "DeepThroat", &PORT_LREF(Mission_TalkToDeepThroat, SetNextMission),
	"DeepThroat", "Mission_TalkToDeepThroat10_ComDeepThroat_NI_1Msg", 1, 0 };

static byte Throat_SetNextMissionN(void) {
	return LvN_PcDefault(TD) && LvN_Next("Mission_DestroyBadDebtors", "Outskirts", "Mission_MeetBankMaster_MissionText1") && Mode_WayPoint();
}

MODE_FN(Berto, InitStatus, 1, LvN_AcShips(MB, "[\"SMerc1\",\"SMerc2\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBetty1\",\"SArchbishop1\"]"))
MODE_FN(Berto, Init, 1, Sy_PhoneInitN(MB, "Mission_MeetBerto.UsePhoneCab", 1))
MODE_FN(Berto, UsePhoneCab, 4, Sy_UsePhone(a, MB, &mb_phone))
MODE_FN(Berto, SetNextMission, 0, Berto_SetNextMissionN())

PyMethodDef g_ScrapyardMeetBertoMethods[] = {
	{ "InitStatus", Berto_InitStatus, 1, NULL },
	{ "Init", Berto_Init, 1, NULL },
	{ "UsePhoneCab", Berto_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Berto_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

MODE_FN(Throat, InitStatus, 1, LvN_AcShips(TD, "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SMerc3\",\"SLMerc\",\"SMayor1\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\",\"SBankMaster1\",\"SArchbishop1\"]"))
MODE_FN(Throat, Init, 1, Sy_PhoneInitN(TD, "Mission_TalkToDeepThroat.UsePhoneCab", 0))
MODE_FN(Throat, UsePhoneCab, 4, Sy_UsePhone(a, TD, &td_phone))
MODE_FN(Throat, SetNextMission, 0, Throat_SetNextMissionN())

PyMethodDef g_ScrapyardTalkToDeepThroatMethods[] = {
	{ "InitStatus", Throat_InitStatus, 1, NULL },
	{ "Init", Throat_Init, 1, NULL },
	{ "UsePhoneCab", Throat_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Throat_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_MosquitoClue: the swarms of the three lairs, the data pack of the last mosquito, the call to the police
 * boss ---- */

static const char *const mc_mosquito_profile[] = { "Ship=SMOSQUITO", "ControlBrake=1", "CadenceShoot=100", "ControlAim=2",
	"AngAttack=0", "CounterAttack=0.30", "ControlBoost=0", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1",
	"FactorDamageGetAway=1.0", "FactorDamageItemLife=0.25", "FactorItemAmmo=0.25", "TimeMaxStrafe=2", "VelMaxStrafe=0.25", NULL };

static byte Clue_InitStatusN(void) {
	static const char *const mosquito[] = { "SMosquito", NULL };
	return Mode_Import("MapTriggers") && LvN_ModSetI("Traffic", "NumNormal", 5) && LvN_ModSetI("Traffic", "NumMedium", 5) &&
		LvN_ModSetI("Traffic", "NumHeavy", 5) && Mode_GVarSetI("iTrafficNumber", 0) && LvN_ModTexts("Traffic", "AcShips", mosquito) &&
		LvN_ModSetI("MapTriggers", "WithScrapyardInhabitants", 0) && LvN_AcShips(MC, ac_ships_std);
}

/* Init(LevelPath): the swarm (the lairs from their dummies, the police boss' message the first time) or the call */
static byte Clue_InitN(void) {
	nativeVal_t model = { 0 }, svv = { 0 }, dummy = { 0 };
	byte ok = Mode_G(MC, "Model_Datapack", &model) && Mode_PreloadOnes(&model);
	Native_Free(&model);
	if (!ok) {
		return 0;
	}
	Mode_SaveVar("Mission_MosquitoClue", &svv);
	byte swarm = LvN_EqS(&svv, "Swarm"), chat = !swarm && LvN_EqS(&svv, "PoliChat");
	Native_Free(&svv);
	if (chat) {
		return PORT_LOWN(Mission_MosquitoClue, InitPoliChat, NULL, "");
	}
	if (!swarm) {
		return 1;
	}
	LvN_Dummy("DM_Mission_Lair*", 1, &dummy);
	while (ok && Native_True(&dummy)) {
		nativeVal_t p = { 0 };
		Native_SetNone(&p);
		ok = LvN_Item(&dummy, 1, &p) && Mode_Append(MC, "lairsList", &p);
		Native_Free(&p);
		Native_Free(&dummy);
		Mode_DummyNext(&dummy);
	}
	Native_Free(&dummy);
	if (!ok) {
		return 0;
	}
	if (LvN_SaveIs("Mission_MosquitoClue.NumLairsExplored", "0")) {
		return LV_COM("Mission_Get1stClue_RemoteMessage1", "PoliceBoss", Mission_MosquitoClue, InitSwarm, LvN_NoArgs());
	}
	return PORT_LOWN(Mission_MosquitoClue, InitSwarm, NULL, "");
}

/* the item of a lair: Scrap.CreateEntity(name, pos[0], pos[1], pos[2], cls) through its seam */
static byte Mc_Create(const char *name, const nativeVal_t *pos, const char *cls, nativeVal_t *out) {
	nativeVal_t n = { 0 };
	LvN_Str(&n, name);
	byte ok = LvN_CreateAt(&n, pos, cls, out);
	Native_Free(&n);
	return ok;
}

/* InitSwarm(): the arrow and the launcher item of the next lair */
static byte Clue_InitSwarmN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *lairs = Mode_V(&t), *pos = Mode_V(&t), *arrow = Mode_V(&t), *g = Mode_V(&t), *l = Mode_V(&t);
	byte ok = Mode_SaveInt("Mission_MosquitoClue.NumLairsExplored", n) && Mode_G(MC, "lairsList", lairs) && Native_Subscript(lairs, n, pos) &&
		Mc_Create("MosquitosLauncherCheckArrow", pos, "WithLife", arrow) && Mode_Put(MC, "checkArrow", NULL, arrow) &&
		Mode_G(MC, "checkArrow", g) && LvN_SetInts3(g, "CamTgt", 0, 500, 0) && Mc_Create("MosquitosLauncher", pos, "Item", l) &&
		LvN_SetI(l, "Radius", 20000) && LvN_SetS(l, "OnTake", "Mission_MosquitoClue.InitMosquito") && LvN_SetI(l, "OnlyUser", 1) &&
		LvN_TargetG(MC, "checkArrow");
	return Mode_Done(&t, ok);
}

/* InitMosquito(ItemName, CharName): the launcher taken: the swarm of the lair, one mosquito every half second */
static byte Clue_InitMosquitoN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *it = Mode_V(&t), *suffix = Mode_V(&t), *an = Mode_V(&t), *arrow = Mode_V(&t), *lairs = Mode_V(&t);
	nativeVal_t *pos = Mode_V(&t), *count = Mode_V(&t), *half = Mode_V(&t);
	long num = 0;
	if (!Mode_SaveInt("Mission_MosquitoClue.NumLairsExplored", n)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqI(n, 1) ? !LvN_Com("Mission_Get1stClue_RemoteMessage2", "PoliceBoss", NULL, NULL, NULL) :
		LvN_EqI(n, 2) && !LvN_Com("Mission_Get1stClue_RemoteMessage3", "PoliceBoss", NULL, NULL, NULL)) {
		return Mode_Done(&t, 0);
	}
	LvN_Str(suffix, "CheckArrow");
	byte ok = Mode_EntityH(&a[0], it) && LvN_SetI(it, "rTime", 20) && Sy_LaterDel(5.0, it) &&
		Native_Arith('+', &a[0], suffix, an) && Mode_EntityH(an, arrow) && Mode_VDel(arrow) &&
		Mode_ListSet("Enemies", "OnDeath", "Mission_MosquitoClue.MosquitoDeath") && Mode_G(MC, "lairsList", lairs) &&
		Native_Subscript(lairs, n, pos) && Mode_G(MC, "numMosquitosInLair", count) && Mode_Range(count, &num);
	Native_SetFloat(half, 0.5);
	for (long k = 0; ok && k < num; k++) {
		nativeVal_t i = { 0 }, inLair = { 0 }, lastN = { 0 }, d = { 0 }, now = { 0 }, time = { 0 }, args[2] = { 0 };
		Native_SetInt(&i, k);
		Native_SetNone(&lastN);
		Native_SetNone(&d);
		Native_SetNone(&time);
		int last = Mode_G(MC, "numMosquitosInLair", &inLair) && Native_Arith('-', &inLair, Mode_I(&t, 1), &lastN) ?
			Mode_Cmp(&i, "==", &lastN) : -1;
		if (last >= 0) {
			Native_SetFloat(&now, Mode_Time());
			Native_Copy(&args[0], pos);
			Native_SetInt(&args[1], last);
			ok = Mode_Arith('*', half, &i, &d) && Native_Arith('+', &now, &d, &time) && Mode_AtG(&time, MC, "CreateMosquito", args, 2);
			Native_Free(&args[0]);
		} else {
			ok = 0;
		}
		Native_Free(&inLair);
		Native_Free(&lastN);
		Native_Free(&d);
		Native_Free(&time);
	}
	return Mode_Done(&t, ok);
}

/* CreateMosquito(pos, isLastMosquito): a mosquito of the lair; the last one and every other one drop nothing */
static byte Clue_CreateMosquitoN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *o1 = Mode_V(&t), *pd = Mode_V(&t), *ammo = Mode_V(&t), *w = Mode_V(&t), *r = Mode_V(&t);
	byte ok = Mode_Import("OutMap") && Mode_SpawnCar("Traffic", 0, o1, NULL) && PORT_LOWN(Mission_MosquitoClue, GetMosquitoProfile, pd, "") &&
		LvN_StdAI(o1, pd) && Mode_VSet(o1, "Pos", &a[0]) && LvN_SetS(o1, "EntityList", "Enemies");
	if (ok) {
		Sy_AmmoMax(ammo);
		ok = Mode_VSet(o1, "Ammo00", ammo) && LvN_Life(o1, MC, "MOSQUITO_LIFE");
	}
	int last = ok ? Mode_CmpI(&a[1], "==", 1) : -1;
	if (last > 0) {
		return Mode_Done(&t, LvN_SetI(o1, "Money", -1));
	}
	int none = last == 0 ? LvN_GEqI(MC, "WithAmmo", 0) : -1;
	ok = none >= 0 && LvN_SetI(o1, "Money", none ? -1 : 25) && Mode_G(MC, "WithAmmo", w) && Native_Arith('-', Mode_I(&t, 1), w, r) &&
		Mode_Put(MC, "WithAmmo", NULL, r);
	return Mode_Done(&t, ok);
}

/* MosquitoDeath(MainCharnName, Param2): money for each mosquito; the swarm done: the data pack after the third lair,
 * else the next lair */
static byte Clue_MosquitoDeathN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *money = Mode_V(&t), *bonus = Mode_V(&t), *sum = Mode_V(&t), *killed = Mode_V(&t), *inLair = Mode_V(&t);
	nativeVal_t *s = Mode_V(&t), *n = Mode_V(&t), *e = Mode_V(&t), *pos = Mode_V(&t), *l = Mode_V(&t), *model = Mode_V(&t);
	nativeVal_t *l2 = Mode_V(&t);
	Native_SetInt(money, Mode_GetMoney());
	byte ok = Mode_GOpI(MC, "numMosquitosKilled", '+', 1) && Native_GVarGet("OnMosquitoDeathMoney", bonus) &&
		Native_Arith('+', money, bonus, sum) && Mode_SetMoney(sum);
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	SaveVar_Inc("Stats.Mosquito", 1);
	int all = Mode_G(MC, "numMosquitosKilled", killed) && Mode_G(MC, "numMosquitosInLair", inLair) ? Mode_Cmp(killed, "==", inLair) : -1;
	if (all <= 0) {
		return Mode_Done(&t, all == 0);
	}
	int third = LvN_PutI(MC, "numMosquitosKilled", 0) && Mode_SaveInt("Mission_MosquitoClue.NumLairsExplored", s) &&
		Native_Arith('+', s, Mode_I(&t, 1), n) ? LvN_EqI(n, 3) : -1;
	if (third > 0) {
		ok = Mode_EntityH(&a[0], e) && Mode_VGet(e, "Pos", pos) && Mc_Create("MissionDataPack", pos, "Item", l) &&
			Mode_G(MC, "Model_Datapack", model) && Mode_VModel(l, model) && LvN_SetI(l, "Radius", 3500) && LvN_SetI(l, "AppearsIfFree", 0) &&
			LvN_SetI(l, "AutoFace", 0) && LvN_SetI(l, "OnlyUser", 1) && LvN_SetF(l, "rTime", 0.25) &&
			LvN_SetS(l, "OnTake", "Mission_MosquitoClue.TakeDataPack") && Mode_Import("Items") &&
			PORT_CALL(Items, SetSound, NULL, "vs", l, "TakeBDataPack") && Mc_Create("MissionCheckArrow", pos, "WithLife", l2) &&
			LvN_SetInts3(l2, "CamTgt", 0, 1000, 0) && LvN_TargetName(l2) && LvN_Com("Mission_Get1stClue_RemoteMessage4", "PoliceBoss", NULL, NULL, NULL);
		return Mode_Done(&t, ok);
	}
	if (third < 0) {
		return Mode_Done(&t, 0);
	}
	LvN_SaveRepr("Mission_MosquitoClue.NumLairsExplored", n);
	return Mode_Done(&t, LvN_Com("Mission_Get1stClue_RemoteMessage6", "PoliceBoss", NULL, NULL, NULL) &&
		PORT_LOWN(Mission_MosquitoClue, InitSwarm, NULL, ""));
}

/* TakeDataPack(ItemName, UsrName): the clue: the police boss' message, then the call */
static byte Clue_TakeDataPackN(void) {
	nativeVal_t it = { 0 }, arrow = { 0 };
	Native_SetNone(&arrow);
	byte ok = LvN_Entity("MissionDataPack", &it) && LvN_SetI(&it, "rTime", 20) && Sy_LaterDel(5.0, &it) &&
		LvN_Entity("MissionCheckArrow", &arrow) && Mode_VDel(&arrow) && Mode_GVarSetI("RemainTalkTime", 0) &&
		LV_COM("Mission_Get1stClue_RemoteMessage5", "PoliceBoss", Mission_MosquitoClue, InitPoliChat, NULL);
	Native_Free(&it);
	Native_Free(&arrow);
	return ok;
}

/* call the police boss or the bishop from a free phone cab: the save state (and the description, NULL for none), the
 * global pc, its use and the target */
static byte Sy_CallFrom(const char *module, const char *state, const char *value, const char *desc, const char *use) {
	nativeVal_t pc = { 0 };
	Native_SetNone(&pc);
	SaveVar_Set(state, value);
	byte ok = (!desc || Mode_MMDesc(desc, 1L)) && PORT_CALL(PhoneCab, GetFree, &pc, "") && Mode_Put(module, "pc", NULL, &pc) &&
		Mode_GSetS(module, "pc", "UseFunc", use) && LvN_TargetG(module, "pc");
	Native_Free(&pc);
	return ok;
}

/* the end of a mission called from a phone cab: the cab's default use, no target, the next mission of the module */
static byte Sy_PhoneNext(const char *module, const char *desc, const char *del) {
	if (!LvN_PcDefault(module) || !Mode_Target(NULL) || !Mode_ApplyNext(module) || !Mode_MMDesc(desc, 1L)) {
		return 0;
	}
	SaveVars_Delete(del);
	return 1;
}

static byte Clue_SetNextMissionN(void) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Bank", "Journalist.Betty_Defaut", "Bank", "Journalist.Berto_Defaut", "Bank", NULL };
	if (!Sy_PhoneNext(MC, "Mission_GetRudolph_MissionText1", "Mission_MosquitoClue")) {
		return 0;
	}
	SaveVars_Delete("Mission_MosquitoClue.NumLairsExplored");
	Mode_Saves(places);
	return 1;
}

MODE_FN(Clue, GetMosquitoProfile, 0, LvN_Profile(mc_mosquito_profile, out))
MODE_FN(Clue, InitStatus, 1, Clue_InitStatusN())
MODE_FN(Clue, Init, 1, Clue_InitN())
MODE_FN(Clue, InitSwarm, 0, Clue_InitSwarmN())
MODE_FN(Clue, InitMosquito, 2, Clue_InitMosquitoN(a))
MODE_FN(Clue, CreateMosquito, 2, Clue_CreateMosquitoN(a))
MODE_FN(Clue, MosquitoDeath, 2, Clue_MosquitoDeathN(a))
MODE_FN(Clue, TakeDataPack, 2, Clue_TakeDataPackN())
MODE_FN(Clue, InitPoliChat, 0, Sy_CallFrom(MC, "Mission_MosquitoClue", "PoliChat", "Mission_Get2ndClue_MissionText1", "Mission_MosquitoClue.UsePhoneCab"))
MODE_FN(Clue, SetNextMission, 0, Clue_SetNextMissionN())

PyMethodDef g_ScrapyardMosquitoClueMethods[] = {
	{ "GetMosquitoProfile", Clue_GetMosquitoProfile, 1, NULL },
	{ "InitStatus", Clue_InitStatus, 1, NULL },
	{ "Init", Clue_Init, 1, NULL },
	{ "InitSwarm", Clue_InitSwarm, 1, NULL },
	{ "InitMosquito", Clue_InitMosquito, 1, NULL },
	{ "CreateMosquito", Clue_CreateMosquito, 1, NULL },
	{ "MosquitoDeath", Clue_MosquitoDeath, 1, NULL },
	{ "TakeDataPack", Clue_TakeDataPack, 1, NULL },
	{ "InitPoliChat", Clue_InitPoliChat, 1, NULL },
	{ "SetNextMission", Clue_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the stupid mercenary: Mission_StupidMerc and Mission_StupidMercAgain follow him along his waypoints ---- */

/* SAI.GetStateVehicle(MercName) == 0 with the global of module (through its seam): 1, 0, -1 */
static int Sy_MercIdle(const char *module) {
	nativeVal_t name = { 0 }, st = { 0 };
	Native_SetNone(&st);
	int r = Mode_G(module, "MercName", &name) && Mode_AIState(&name, &st) ? Mode_CmpI(&st, "==", 0) : -1;
	Native_Free(&name);
	Native_Free(&st);
	return r;
}

/* the mercenary to MercWaypoints[iWaypoint] (again: state 1 at 3000; else 3000, 0, 0.25), then iWaypoint + 1 */
static byte Sy_NextWaypoint(const char *module, byte again) {
	nativeVal_t wps = { 0 }, i = { 0 }, pos = { 0 }, name = { 0 };
	Native_SetNone(&pos);
	byte ok = Mode_G(module, "MercWaypoints", &wps) && Mode_G(module, "iWaypoint", &i) && Native_Subscript(&wps, &i, &pos) &&
		Mode_G(module, "MercName", &name) && Sy_StateTo(&name, &pos, NULL, again) && Mode_GOpI(module, "iWaypoint", '+', 1);
	Native_Free(&wps);
	Native_Free(&i);
	Native_Free(&pos);
	Native_Free(&name);
	return ok;
}

/* the hint of rmmess (two talks in turn) after the second waypoint: rmmess = not rmmess */
static byte Sy_Hint(const char *module) {
	int rm = LvN_GTrue(module, "rmmess");
	if (rm < 0 || !LvN_Com(rm ? "Mission_FollowStupidMerc_RemoteMessage3" : "Mission_FollowStupidMerc_RemoteMessage5", "MercenaryB", NULL,
		NULL, NULL)) {
		return 0;
	}
	rm = LvN_GTrue(module, "rmmess");
	return rm >= 0 && LvN_PutI(module, "rmmess", !rm);
}

/*
 * GotoNext() of the two: every 2 s: the mercenary goes on to his next waypoint when the player is near (again: the
 * smart mercenaries come at the third, a waypoint of 3000 with state 1; else 3000, 0, 0.25 and the schedule named
 * "GoToNext"); after the last one his plans appear; the first time the player falls behind he gets a hint. module is
 * Mission_StupidMercAgain when again, else Mission_StupidMerc: the own calls name their refs
 */
static byte Sy_GotoNextN(const char *module, byte again) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *o1 = Mode_V(&t), *usr = Mode_V(&t), *d = Mode_V(&t), *wp = Mode_V(&t), *wps = Mode_V(&t);
	nativeVal_t *len = Mode_V(&t), *oname = Mode_V(&t), *params = Mode_V(&t);
	int t1 = Mode_G(module, "MercName", name) && Mode_EntityH(name, o1) ? Native_True(o1) : -1;
	if (t1 == 0) {
		return Mode_Done(&t, again ? PORT_LOWN(Mission_StupidMercAgain, CreateMerc, NULL, "") : Mode_DelSchedS("GoToNext"));
	}
	if (t1 < 0 || !Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(usr)) {
		int go = LvN_GEqI(module, "iWaypoint", 0);
		if (go == 0) {
			go = Sy_MercIdle(module);
			if (go > 0) {
				go = Sy_Dist(usr, o1, d) ? Mode_CmpI(d, "<", 15000) : -1;
			}
		}
		if (go > 0) {
			int n = Mode_G(module, "iWaypoint", wp) && Mode_G(module, "MercWaypoints", wps) ? Native_Size(wps) : -1;
			if (n >= 0) {
				Native_SetInt(len, n);
			}
			int end = n >= 0 ? Mode_Cmp(wp, ">=", len) : -1;
			if (end > 0) {
				byte ok;
				if (again) {
					ok = LV_COM("Mission_FollowStupidMerc_RemoteMessage6", "MercenaryB", Mission_StupidMercAgain, CreateMercPlans,
						LvN_NoArgs());
				} else {
					Native_SetEmpty(params, NV_TUPLE);
					ok = Mode_VGet(o1, "Name", oname);
					if (ok) {
						*Native_Append(params) = *oname;
						Native_SetNone(oname);
						ok = LV_COM("Mission_FollowStupidMerc_RemoteMessage6", "MercenaryB", Mission_StupidMerc, CreateATPCPlans, params);
					}
				}
				ok = ok && LvN_SetS(o1, "TgType", "None") && LvN_SetS(o1, "EntityList", "Traffic") && LvN_SetS(o1, "OnDeath", "Vehicles.OnDeath");
				return Mode_Done(&t, ok);
			}
			if (end < 0) {
				return Mode_Done(&t, 0);
			}
			int third = again ? LvN_GEqI(module, "iWaypoint", 3) : 0;
			byte ok = third >= 0;
			if (third > 0) {
				ok = PORT_LOWN(Mission_StupidMercAgain, CreateMercs, NULL, "");
			} else if (third == 0) {
				int hint = Mode_GCmpI(module, "iWaypoint", ">", 1);
				ok = hint >= 0 && (!hint || Sy_Hint(module));
			}
			if (!ok || !Sy_NextWaypoint(module, again)) {
				return Mode_Done(&t, 0);
			}
		} else if (go == 0) {
			int hint = Mode_GCmpI(module, "iWaypoint", ">", 1);
			if (hint > 0) {
				hint = LvN_GTrue(module, "FirstHint");
			}
			if (hint > 0) {
				hint = Sy_MercIdle(module);
			}
			if (hint > 0 && (!LvN_PutI(module, "FirstHint", 0) ||
				!PORT_CALL(Speech, RemoteComunication, NULL, "ss", "Mission_FollowStupidMerc_RemoteMessage2", "MercenaryB"))) {
				return Mode_Done(&t, 0);
			}
			if (hint < 0) {
				return Mode_Done(&t, 0);
			}
		} else {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_Later(2.0, module, "GotoNext", NULL, 0, again ? NULL : "GoToNext"));
}

/* the dummy of a blueprint as a photo object into *prf (the mission target goes first); *prf None and the console
 * error when missing */
static byte Sy_PlanPhoto(const char *module, const char *dummyName, const char *model, const char *taken, nativeVal_t *prf) {
	char msg[0x80];
	nativeVal_t dummy = { 0 }, pos = { 0 }, ang = { 0 }, text = { 0 };
	Native_SetNone(prf);
	Mode_DummyStart(dummyName, 0);
	Mode_DummyNext(&dummy);
	if (!Native_True(&dummy)) {
		Str_Printf(msg, sizeof(msg), "ERROR : %s not found! \n", dummyName);
		LvN_Str(&text, msg);
		byte ok = Mode_ConsoleError(&text);
		Native_Free(&text);
		return ok;
	}
	Native_SetNone(&pos);
	Native_SetNone(&ang);
	byte ok = Mode_Target(NULL) && Mode_GVarSetS("CurrentWaypointName", "") && LvN_DummyPosAng(&dummy, &pos, &ang) &&
		PORT_CALL(MissionsFuncs, CreatePhotoObject, prf, "svv", model, &pos, &ang) && LvN_ModSetGlobal("CharAct", "TakenPhoto", module, taken);
	Native_Free(&dummy);
	Native_Free(&pos);
	Native_Free(&ang);
	return ok;
}

static const char *const sm_mosquito_profile[] = { "CadenceShootCM=7", "UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10",
	"UsePercHook=0", "FactorHookMap=0", "CadenceShoot=15", "ControlAim=3", "AngAttack=1.2", "CounterAttack=0.2", "ControlBoost=0",
	"ControlBoostItemLife=0", "ControlBoostItemAmmo=0", "FactorDamageGetAway=0.90", "FactorDamageItemLife=0.05", "FactorItemAmmo=0.05",
	"ControlStrafeEvasion=0", "UsePercHookGetAway=0", "TimeMaxStrafe=2", "VelMaxStrafe=0.25", NULL };

/* InitStatus(LevelPath) of the two: no mercenaries in the traffic (the first mission: the lairs launch from farther) */
static byte Sy_MercStatusN(const char *module, byte farther) {
	return Mode_Import("VehiclesNPC") && LvN_ModSetI("VehiclesNPC", "UseMercs", 0) && LvN_AcShips(module, ac_ships_std) &&
		(!farther || (Mode_GVarSetN("MaxMosquitosDist", 60000.0) && Mode_GVarSetN("MaxMadHuntersDist", 90000.0)));
}

/* Scrap.Preload3DObject(model, 1, 1, 1) of a C name */
static byte Sy_PreloadOnes(const char *model) {
	nativeVal_t m = { 0 };
	LvN_Str(&m, model);
	byte ok = Mode_PreloadOnes(&m);
	Native_Free(&m);
	return ok;
}

/* Init(LevelPath): the mercenary 10 s later at his first waypoint, the mosquitos reprogrammed, the life items */
static byte Merc_InitN(void) {
	nativeVal_t args[2] = { 0 }, aiprof = { 0 };
	Native_SetInt(&args[0], 0);
	Native_SetInt(&args[1], 1);
	byte ok = Mode_Later(10.0, SM, "CreateMerc", args, 2, NULL) && Sy_MosquitoProf(&aiprof) && Mode_SetAttrs(&aiprof, sm_mosquito_profile) &&
		Sy_PreloadOnes("Models/Misc/Blueprint/PlanoRail.m3d") && Mode_GVarSetS("OnItemTakenAI", "Mission_StupidMerc.CheckAITakesLife") &&
		Mode_GVarSetS("OnLifeItemTaken", "Mission_StupidMerc.CheckLife");
	Native_Free(&aiprof);
	return ok;
}

/* VehiclesNPC.MakeAI("SMerc1", "Traffic", InitState = 0) */
static byte Sm_MakeAI(nativeVal_t *out) {
	return PORT_CALL(VehiclesNPC, MakeAI, out, "ss;InitState=i", "SMerc1", "Traffic", 0L);
}

/* MapTriggers.<var> = [MercName] of the global of module */
static byte Sy_Allowed(const char *module, const char *var) {
	nativeVal_t lst = { 0 }, name = { 0 };
	Native_SetEmpty(&lst, NV_LIST);
	byte ok = Mode_G(module, "MercName", &name);
	if (ok) {
		*Native_Append(&lst) = name;
		ok = Mode_SetModVar("MapTriggers", var, &lst);
	} else {
		Native_Free(&name);
	}
	Native_Free(&lst);
	return ok;
}

/* Scrap.Set("CurrentWaypointName", MercName); MissionsFuncs.SetMissionTarget(o1.Name) */
static byte Sy_MercTarget(const char *module, const nativeVal_t *o1) {
	nativeVal_t name = { 0 };
	byte ok = Mode_G(module, "MercName", &name) && Mode_GVarSet("CurrentWaypointName", &name) && LvN_TargetName(o1);
	Native_Free(&name);
	return ok;
}

/* CreateMerc(killedByPlayer = 0, firstfit = 0): the stupid mercenary (he alone launches the lairs) on his way */
static byte Merc_CreateMercN(const nativeVal_t *killed, const nativeVal_t *firstfit, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *wps = Mode_V(&t), *pos = Mode_V(&t);
	byte ok = Mode_Import("MapTriggers") && Sm_MakeAI(out) && LvN_SetI(out, "Money", -1) && LvN_SetS(out, "TgType", "green") &&
		LvN_SetS(out, "OnDeath", "Mission_StupidMerc.OnStupidMercDeath") && LvN_SetS(out, "OnDamage", "Mission_StupidMerc.OnStupidMercDamage") &&
		LvN_Life(out, SM, "MERC_LIFE") && Mode_VGet(out, "Name", name) && Mode_Put(SM, "MercName", NULL, name) &&
		(!Native_True(firstfit) || (Mode_G(SM, "MercWaypoints", wps) && LvN_Item(wps, 0, pos) && Mode_VSet(out, "Pos", pos))) &&
		Sy_Allowed(SM, "shipsAllowedToLaunchMosquitos") && Sy_Allowed(SM, "shipsAllowedToLaunchMadHunters") && Sy_MercTarget(SM, out);
	int first = ok ? LvN_GTrue(SM, "FirstAppereance") : -1;
	if (first > 0) {
		ok = LvN_Com("Mission_GetMercShipPlans_RemoteMessage1", "MercenaryB", NULL, NULL, NULL) && LvN_PutI(SM, "FirstAppereance", 0);
	} else if (first == 0) {
		ok = LvN_Com(Native_True(killed) ? "Mission_FollowStupidMerc_RemoteMessage4" : "Mission_FollowStupidMerc_RemoteMessage8", "MercenaryB",
			NULL, NULL, NULL);
	} else {
		ok = 0;
	}
	ok = ok && LvN_PutI(SM, "iWaypoint", 0) && PORT_LOWN(Mission_StupidMerc, GotoNext, NULL, "");
	return Mode_Done(&t, ok);
}

/* CreateMerc(killedByPlayer=0, firstfit=0) */
MODE_KW(Merc, CreateMerc, "|OO", "killedByPlayer", "firstfit") {
	Mode_KwDefI(v, given, 0, 0);
	Mode_KwDefI(v, given, 1, 0);
	return Merc_CreateMercN(&v[0], &v[1], out);
}

/* OnStupidMercDeath(LooserName, WinnerName): he comes back 2 s later (killed by the player or not) */
static byte Merc_OnStupidMercDeathN(const nativeVal_t *a) {
	nativeVal_t usr = { 0 }, by = { 0 }, name = { 0 };
	Native_SetNone(&usr);
	Native_SetNone(&by);
	Native_SetNone(&name);
	int t = Sy_OnDeath(&a[0], &a[1]) && Mode_UsrHandle(&usr) ? Native_True(&usr) : -1;
	if (t > 0) {
		/* (usr and usr.Name == WinnerName,) */
		int c = Mode_VGet(&usr, "Name", &name) ? Mode_Cmp(&name, "==", &a[1]) : -1;
		t = c < 0 ? -1 : 1;
		Native_SetInt(&by, c);
	} else if (t == 0) {
		Native_Copy(&by, &usr);
	}
	byte ok = t >= 0 && Mode_Later(2.0, SM, "CreateMerc", &by, 1, NULL) && Mode_DelSchedS("GoToNext");
	Native_Free(&usr);
	Native_Free(&by);
	Native_Free(&name);
	return ok;
}

/* the nearest life item of the mercenary (SAI.GetNearestItemLife through its ref, a case swaps it): the mercenary
 * name goes there; 1 when there is one, 0, -1 */
static int Sm_GoForLife(const nativeVal_t *name, const nativeVal_t *looser, nativeVal_t *item) {
	nativeVal_t in = { 0 }, pos = { 0 }, radius = { 0 };
	Native_SetNone(&in);
	Native_SetNone(item);
	int t = PORT_CALL(SAI, GetNearestItemLife, &in, "v", looser) && Mode_EntityH(&in, item) ? Native_True(item) : -1;
	Native_Free(&in);
	if (t > 0 && name) {
		Native_SetNone(&pos);
		Native_SetNone(&radius);
		t = Mode_VGet(item, "Pos", &pos) && Mode_VGet(item, "Radius", &radius) && Sy_StateTo(name, &pos, &radius, 0) ? 1 : -1;
		Native_Free(&pos);
		Native_Free(&radius);
	}
	return t;
}

/* OnStupidMercDamage(LooserName, WinnerName, Damage): low on life he goes for the nearest life item */
static byte Merc_OnStupidMercDamageN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *life = Mode_V(&t), *perc = Mode_V(&t), *max = Mode_V(&t), *lim = Mode_V(&t), *item = Mode_V(&t);
	nativeVal_t *left = Mode_V(&t), *pos = Mode_V(&t), *radius = Mode_V(&t);
	int go = Mode_EntityH(&a[0], e) ? Native_True(e) : -1;
	if (go > 0) {
		go = LvN_GEqI(SM, "gettingLife", 0);
	}
	if (go > 0) {
		go = Mode_VGet(e, "Life", life) && Mode_G(SM, "percStupidMercGoesForLife", perc) && Mode_VGet(e, "MaxLife", max) &&
			Mode_Arith('*', perc, max, lim) ? Mode_Cmp(life, "<", lim) : -1;
	}
	if (go > 0) {
		go = Sm_GoForLife(NULL, &a[0], item);
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	Native_Free(life);
	int c = Mode_VGet(e, "Life", life) && Native_Arith('-', life, &a[2], left) && Mode_Put(SM, "mercLife", NULL, left) ?
		Mode_GCmpI(SM, "mercLife", ">", 0) : -1;
	byte ok = c == 0 || (c > 0 && Mode_VGet(item, "Pos", pos) && Mode_VGet(item, "Radius", radius) &&
		Mode_Put(SM, "mercName", NULL, &a[0]) && LvN_PutI(SM, "gettingLife", 1) && Sy_StateTo(&a[0], pos, radius, 0));
	return Mode_Done(&t, ok);
}

/* name == mercName and gettingLife == 1 */
static int Sm_Getting(const nativeVal_t *name) {
	nativeVal_t m = { 0 };
	int r = Mode_G(SM, "mercName", &m) ? Mode_Cmp(name, "==", &m) : -1;
	Native_Free(&m);
	return r > 0 ? LvN_GEqI(SM, "gettingLife", 1) : r;
}

/* CheckAITakesLife(name): an AI took an item: it goes on; the mercenary going for life is checked a second later */
static byte Merc_CheckAITakesLifeN(const nativeVal_t *name) {
	if (!Native_True(name)) {
		return 1;
	}
	if (!Sy_State(0, name)) {
		return 0;
	}
	int g = Sm_Getting(name);
	return g == 0 || (g > 0 && Mode_Later(1.0, SM, "CheckLife", name, 1, NULL));
}

/* CheckLife(name): the mercenary got no life yet: the next item; else back to his waypoint */
static byte Merc_CheckLifeN(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *life = Mode_V(&t), *ml = Mode_V(&t), *item = Mode_V(&t), *wp = Mode_V(&t), *wps = Mode_V(&t);
	nativeVal_t *len = Mode_V(&t), *pos = Mode_V(&t);
	int g = Sm_Getting(name);
	if (g <= 0) {
		return Mode_Done(&t, g == 0);
	}
	if (!Mode_EntityH(name, e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	int low = Mode_VGet(e, "Life", life) && Mode_G(SM, "mercLife", ml) ? Mode_Cmp(life, "<=", ml) : -1;
	if (low > 0) {
		low = Sm_GoForLife(name, name, item);
		if (low > 0) {
			return Mode_Done(&t, 1);
		}
	}
	int n = low >= 0 && LvN_PutI(SM, "gettingLife", 0) && Mode_G(SM, "iWaypoint", wp) && Mode_G(SM, "MercWaypoints", wps) ?
		Native_Size(wps) : -1;
	if (n >= 0) {
		Native_SetInt(len, n);
	}
	int end = n >= 0 ? Mode_Cmp(wp, ">=", len) : -1;
	byte ok = end >= 0;
	if (end > 0) {
		Native_Free(wps);
		int k = Mode_G(SM, "MercWaypoints", wps) ? Native_Size(wps) : -1;
		ok = k >= 0 && LvN_Item(wps, (long)k - 1, pos);
	} else if (end == 0) {
		Native_Free(wps);
		Native_Free(wp);
		ok = Mode_G(SM, "MercWaypoints", wps) && Mode_G(SM, "iWaypoint", wp) && Native_Subscript(wps, wp, pos);
	}
	return Mode_Done(&t, ok && Sy_StateTo(name, pos, NULL, 0));
}

/* CreateATPCPlans(mercName): the mercenary stops, the ATPC blueprint is the target */
static byte Merc_CreateATPCPlansN(const nativeVal_t *name) {
	nativeVal_t e = { 0 }, prf = { 0 }, pn = { 0 };
	Native_SetNone(&pn);
	byte ok = Mode_EntityH(name, &e) && (!Native_True(&e) || Sy_State(3, name)) &&
		Sy_PlanPhoto(SM, "DM_Mission_ATPC_Blueprint", "PlanoRail", "AddATPCPlans", &prf);
	if (ok && prf.type != NV_NONE) {
		ok = Mode_VGet(&prf, "Name", &pn) && Mode_Put(SM, "ATPCPlansName", NULL, &pn);
		Native_Free(&pn);
		ok = ok && Mode_G(SM, "ATPCPlansName", &pn) && LvN_Target(&pn);
	}
	Native_Free(&e);
	Native_Free(&prf);
	Native_Free(&pn);
	return ok;
}

/* AddATPCPlans(proofName): the photo of the blueprint: the ATPC into the hangar, Deep Throat's message, the next mission */
static byte Merc_AddATPCPlansN(const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t pn = { 0 }, e = { 0 }, text = { 0 };
	Native_SetNone(&e);
	int c = Mode_Import("RaceMaker") && Mode_G(SM, "ATPCPlansName", &pn) ? Mode_Cmp(&pn, "==", name) : -1;
	Native_Free(&pn);
	if (c == 0) {
		Native_SetInt(out, 0);
	}
	if (c <= 0) {
		return c == 0;
	}
	Native_SetNone(&text);
	byte ok = Mode_Blueprint(SM, "Hangar.availableWeaponsList", "ATPC", NULL, 0) && Mode_G(SM, "ATPCPlansName", &pn) &&
		Mode_EntityH(&pn, &e) && LvN_SetI(&e, "CanPhoto", 0) && LvN_ModNone("CharAct", "TakenPhoto") && Mode_Target(NULL) &&
		(Mode_Lang("Mission_FollowStupidMerc_SystemMessage1", &text), PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text)) &&
		LV_COM("Mission_FollowStupidMerc_RemoteMessage7", "DeepThroat", Mission_StupidMerc, SetNextMission, LvN_NoArgs());
	Native_Free(&pn);
	Native_Free(&e);
	Native_Free(&text);
	if (ok) {
		Native_SetInt(out, 1);
	}
	return ok;
}

static byte Merc_SetNextMissionN(void) {
	return Mode_GVarSetS("OnItemTakenAI", "") && Mode_GVarSetDef("MaxMosquitosDist") && Mode_GVarSetDef("MaxMadHuntersDist") && Mode_ApplyNext(SM) &&
		Mode_MMDesc("Mission_InvulnerableTransport_MissionText1", 1L);
}

MODE_FN(Merc, InitStatus, 1, Sy_MercStatusN(SM, 1))
MODE_FN(Merc, Init, 1, Merc_InitN())
MODE_FN(Merc, OnStupidMercDeath, 2, Merc_OnStupidMercDeathN(a))
MODE_FN(Merc, OnStupidMercDamage, 3, Merc_OnStupidMercDamageN(a))
MODE_FN(Merc, CheckAITakesLife, 1, Merc_CheckAITakesLifeN(&a[0]))
MODE_FN(Merc, CheckLife, 1, Merc_CheckLifeN(&a[0]))
MODE_FN(Merc, GotoNext, 0, Sy_GotoNextN(SM, 0))
MODE_FN(Merc, CreateATPCPlans, 1, Merc_CreateATPCPlansN(&a[0]))
MODE_FN(Merc, AddATPCPlans, 1, Merc_AddATPCPlansN(&a[0], out))
MODE_FN(Merc, SetNextMission, 0, Merc_SetNextMissionN())

PyMethodDef g_ScrapyardStupidMercMethods[] = {
	{ "InitStatus", Merc_InitStatus, 1, NULL },
	{ "Init", Merc_Init, 1, NULL },
	{ "CreateMerc", (PyCFunction)(void (*)(void))Merc_CreateMerc, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "OnStupidMercDeath", Merc_OnStupidMercDeath, 1, NULL },
	{ "OnStupidMercDamage", Merc_OnStupidMercDamage, 1, NULL },
	{ "CheckAITakesLife", Merc_CheckAITakesLife, 1, NULL },
	{ "CheckLife", Merc_CheckLife, 1, NULL },
	{ "GotoNext", Merc_GotoNext, 1, NULL },
	{ "CreateATPCPlans", Merc_CreateATPCPlans, 1, NULL },
	{ "AddATPCPlans", Merc_AddATPCPlans, 1, NULL },
	{ "SetNextMission", Merc_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

static const char *const sa_mercs_profile[] = { "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=5", "UsePercSonic=70",
	"UsePercCloud=20", "UsePercElectro=10", "UsePercHook=25", "FactorHookMap=4", "CadenceShoot=0", "ControlAim=3", "AngAttack=3.14",
	"CounterAttack=0.75", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.70",
	"FactorDamageItemLife=0.2", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=2", "TimeMaxStrafe=4",
	"VelMaxStrafe=0.5", NULL };

/* GetMercsEnemiesProfile(TypeShip): the smart mercenaries (OBJLIB 5: the class VehiclesNPC.cShipAI by name, a variable of
 * VehiclesNPC made by its .mod code, until the variable registry holds it) */
static byte Again_GetMercsEnemiesProfileN(const nativeVal_t *type, nativeVal_t *out) {
	return Mode_Import("VehiclesNPC") && LV_CALL("VehiclesNPC", "cShipAI", out, "") && Mode_VSet(out, "Ship", type) &&
		Mode_SetAttrs(out, sa_mercs_profile);
}

/* Init(LevelPath): the part of the mission of the save variable: follow, evade or call the bishop */
static byte Again_InitN(void) {
	static const char *const parts[] = { "Follow", "Evasion", "TalkBishop" };
	nativeVal_t sv = { 0 }, aiprof = { 0 };
	Native_SetNone(&aiprof);
	if (!Mode_ListSet("Enemies", "OnDeath", "Mission_StupidMercAgain.SmartMerDeath")) {
		return 0;
	}
	Mode_SaveVar("Mission_StupidMercAgain", &sv);
	byte ok = 1;
	for (int i = 0; i < 3; i++) {
		if (LvN_EqS(&sv, parts[i])) {
			/* OBJLIB 6: InitEvasion is a section of mission_stupidmercagain.def */
			ok = i == 0 ? PORT_LOWN(Mission_StupidMercAgain, InitFollow, NULL, "") : i == 1 ? LV_OWN(SA, "InitEvasion", NULL, "") :
				PORT_LOWN(Mission_StupidMercAgain, InitTalkBishop, NULL, "");
			break;
		}
	}
	Native_Free(&sv);
	ok = ok && Sy_MosquitoProf(&aiprof) && LvN_SetI(&aiprof, "ControlBoost", 1) && Sy_PreloadOnes("Models/Misc/Blueprint/PlanoSMerc2.m3d") &&
		Sy_PreloadOnes("Models/Misc/Blueprint/PlanoMMerc2.m3d");
	Native_Free(&aiprof);
	return ok;
}

/* CreateMerc(): the stupid mercenary on his way (at his first waypoint the first time) */
static byte Again_CreateMercN(nativeVal_t *out) {
	nativeVal_t name = { 0 }, wps = { 0 }, pos = { 0 };
	Native_SetNone(&name);
	Native_SetNone(&wps);
	Native_SetNone(&pos);
	byte ok = PORT_CALL(VehiclesNPC, MakeAI, out, "ss", "SMerc1", "Traffic") && LvN_SetI(out, "Money", -1) &&
		LvN_SetS(out, "TgType", "green") && Mode_VGet(out, "Name", &name) && Mode_Put(SA, "MercName", NULL, &name) && Sy_MercTarget(SA, out);
	int first = ok ? LvN_GTrue(SA, "FirstAppereance") : -1;
	if (first > 0) {
		ok = LvN_Com("Mission_GetMercShipPlans_RemoteMessage1", "MercenaryB", NULL, NULL, NULL) && LvN_PutI(SA, "FirstAppereance", 0) &&
			Mode_G(SA, "MercWaypoints", &wps) && LvN_Item(&wps, 0, &pos) && Mode_VSet(out, "Pos", &pos);
	} else {
		ok = first == 0 && LvN_Com("Mission_FollowStupidMerc_RemoteMessage4", "MercenaryB", NULL, NULL, NULL);
	}
	Native_Free(&name);
	Native_Free(&wps);
	Native_Free(&pos);
	return ok && LvN_PutI(SA, "iWaypoint", 0) && PORT_LOWN(Mission_StupidMercAgain, GotoNext, NULL, "");
}

/* one of the two smart mercenaries when its global name has no entity */
static byte Sa_Merc(const char *var, const char *name, const char *ship) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *o = Mode_V(&t), *pd = Mode_V(&t), *on = Mode_V(&t);
	int none = Mode_G(SA, var, n) && Mode_EntityH(n, o) ? Sy_CmpNone(o, "==") : -1;
	if (none <= 0) {
		return Mode_Done(&t, none == 0);
	}
	Native_Free(o);
	byte ok = Mode_SpawnCar(name, 0, o, NULL) && PORT_LOWN(Mission_StupidMercAgain, GetMercsEnemiesProfile, pd, "s", ship) && LvN_StdAI(o, pd) &&
		LvN_SetS(o, "EntityList", "Enemies") && LvN_Life(o, SA, "MERCS_LIFE") && Mode_VGet(o, "Name", on) && Mode_Put(SA, var, NULL, on);
	return Mode_Done(&t, ok);
}

/* the global names SmartMercName and MedMercName are both "": 1, 0, -1 */
static int Sa_NoMercs(void) {
	nativeVal_t v = { 0 };
	int r = Mode_G(SA, "SmartMercName", &v) ? LvN_EqS(&v, "") : -1;
	Native_Free(&v);
	if (r > 0) {
		r = Mode_G(SA, "MedMercName", &v) ? LvN_EqS(&v, "") : -1;
		Native_Free(&v);
	}
	return r;
}

/* CreateMercs(): the smart mercenaries (their first appearance with a message) */
static byte Again_CreateMercsN(void) {
	int none = Mode_Import("OutMap") ? Sa_NoMercs() : -1;
	return none >= 0 && (!none || LvN_Com("Mission_GetMercShipPlans_RemoteMessage2", "MercenaryA", NULL, NULL, NULL)) &&
		Sa_Merc("SmartMercName", "MercenaryMed", "SMerc2") && Sa_Merc("MedMercName", "MercenarySmart", "SMerc3");
}

/* SmartMerDeath(Name, Killer): a smart mercenary is back 15 s later unless the player is evading */
static byte Again_SmartMerDeathN(void) {
	nativeVal_t sv = { 0 };
	Mode_SaveVar("Mission_StupidMercAgain", &sv);
	byte evading = LvN_EqS(&sv, "Evasion");
	Native_Free(&sv);
	return evading || Mode_Later(15.0, SA, "CreateMercs", NULL, 0, "DelayedCreateMercs");
}

/* CreateMercPlans(): the blueprints of the ship and the engines of the stupid mercenary as targets */
static byte Again_CreateMercPlansN(void) {
	static const char *const plans[][2] = { { "DM_Mission_SMerc2_Blueprint", "PlanoSMerc2" }, { "DM_Mission_MMERC2_Blueprint", "PlanoMMerc2" } };
	nativeVal_t names = { 0 };
	byte ok = 1;
	Native_SetEmpty(&names, NV_LIST);
	for (int i = 0; ok && i < 2; i++) {
		nativeVal_t prf = { 0 };
		ok = Sy_PlanPhoto(SA, plans[i][0], plans[i][1], "AddMercPlans", &prf) && (prf.type == NV_NONE || Mode_VGet(&prf, "Name", Native_Append(&names)));
		Native_Free(&prf);
	}
	ok = ok && PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &names);
	Native_Free(&names);
	return ok;
}

/* AddMercPlans(proofName): the photo of a blueprint: both blueprints into the hangar, then the evasion */
static byte Again_AddMercPlansN(const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t r = { 0 }, e = { 0 };
	Native_SetNone(&r);
	Native_SetNone(&e);
	int obj = PORT_CALL(MissionsFuncs, IsCharMissionObjective, &r, "v", name) ? Native_True(&r) : -1;
	Native_Free(&r);
	if (obj == 0) {
		Native_SetInt(out, 0);
	}
	if (obj <= 0) {
		return obj == 0;
	}
	int del = Mode_EntityH(name, &e) && LvN_SetI(&e, "CanPhoto", 0) && PORT_CALL(MissionsFuncs, DelMissionTarget, &r, "v", name) ?
		Native_True(&r) : -1;
	Native_Free(&r);
	Native_Free(&e);
	/* OBJLIB 6: InitEvasion is a section of mission_stupidmercagain.def */
	byte ok = del == 0 || (del > 0 && Mode_Blueprint(SA, "Hangar.shipsToEditList", "SMerc2", "Mission_FollowStupidMerc_SystemMessage1", 0) &&
		Mode_Blueprint(SA, "Hangar.availableEnginesList", "MMERC2", "Mission_FollowStupidMerc_SystemMessage2", 0) &&
		LvN_ModNone("CharAct", "TakenPhoto") && LvN_Com("Mission_GetMercShipPlans_RemoteMessage3", "MercenaryC", SA, "InitEvasion", LvN_NoArgs()));
	if (ok && del > 0) {
		SaveVar_Set("Mission_StupidMercAgain", "Evasion");
	}
	if (ok) {
		Native_SetInt(out, 1);
	}
	return ok;
}

/* StartEvasion(): the smart mercenaries after the player (only the second name is set when none was there) */
static byte Again_StartEvasionN(void) {
	int none = Sa_NoMercs();
	if (none < 0 || (none && !LvN_PutS(SA, "MedMercName", "MercAbsurd")) || !PORT_LOWN(Mission_StupidMercAgain, CreateMercs, NULL, "") ||
		!Mode_MMDesc("Mission_TalkToIgor2_MissionText1", 1L)) {
		return 0;
	}
	SaveVar_Set("BadReputation", "2");
	SaveVar_Set("StdShipAIProfile", "Veteran");
	return PORT_LOWN(Mission_StupidMercAgain, CheckEvasion, NULL, "");
}

/* a smart mercenary within EVASION_DISTANCE of the player: 1, 0, -1 */
static int Sa_Near(const nativeVal_t *o, const nativeVal_t *usr) {
	nativeVal_t d = { 0 }, ev = { 0 };
	int t = Sy_CmpNone(o, "!=");
	if (t <= 0) {
		return t;
	}
	Native_SetNone(&ev);
	t = Sy_Dist(o, usr, &d) && Mode_G(SA, "EVASION_DISTANCE", &ev) ? Mode_Cmp(&d, "<", &ev) : -1;
	Native_Free(&d);
	Native_Free(&ev);
	return t;
}

/* the smart mercenary gives up */
static byte Sa_GiveUp(const nativeVal_t *o) {
	nativeVal_t name = { 0 };
	if (!Native_True(o)) {
		return 1;
	}
	Native_SetNone(&name);
	byte ok = LvN_SetS(o, "EntityList", "Traffic") && LvN_SetS(o, "TgType", "None") && Mode_VGet(o, "Name", &name) && Sy_State(3, &name);
	Native_Free(&name);
	return ok;
}

/* CheckEvasion(): every 2 s: both smart mercenaries dead, or EVASION_TIME away from them: the call to the bishop */
static byte Again_CheckEvasionN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *now = Mode_V(&t), *o1 = Mode_V(&t), *o2 = Mode_V(&t), *et = Mode_V(&t), *zero = Mode_V(&t);
	nativeVal_t *dt = Mode_V(&t), *limit = Mode_V(&t), *when = Mode_V(&t), *two = Mode_V(&t);
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(now, Mode_Time());
	int dead = Mode_GEntity(SA, "SmartMercName", o1) && Mode_GEntity(SA, "MedMercName", o2) ? Sy_CmpNone(o1, "==") : -1;
	if (dead > 0) {
		dead = Sy_CmpNone(o2, "==");
	}
	if (dead > 0) {
		dead = Sy_CmpNone(usr, "!=");
	}
	if (dead > 0) {
		return Mode_Done(&t, Mode_DelSchedS("DelayedCreateMercs") && PORT_LOWN(Mission_StupidMercAgain, InitTalkBishop, NULL, ""));
	}
	int evade = dead < 0 ? -1 : Sy_CmpNone(usr, "==");
	if (evade >= 0) {
		evade = !evade;
	}
	if (evade > 0) {
		int n = Sa_Near(o1, usr);
		evade = n < 0 ? -1 : n ? 0 : 1;
	}
	if (evade > 0) {
		int n = Sa_Near(o2, usr);
		evade = n < 0 ? -1 : n ? 0 : 1;
	}
	if (evade < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(zero, 0.0);
	int reset = Mode_G(SA, "EvasionTime", et) ? Mode_Cmp(et, "==", zero) : -1;
	if (reset == 0) {
		reset = !evade;
	}
	if (reset > 0 && !Mode_Put(SA, "EvasionTime", NULL, now)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(et);
	int wait = reset >= 0 && Mode_G(SA, "EvasionTime", et) && Native_Arith('-', now, et, dt) && Mode_G(SA, "EVASION_TIME", limit) ?
		Mode_Cmp(dt, "<", limit) : -1;
	if (wait > 0) {
		Native_SetFloat(two, 2.0);
		return Mode_Done(&t, Native_Arith('+', now, two, when) && Mode_AtG(when, SA, "CheckEvasion", NULL, 0));
	}
	byte ok = wait == 0 && Sa_GiveUp(o1) && Sa_GiveUp(o2) && Mode_DelSchedS("DelayedCreateMercs") &&
		LvN_Com("Mission_TalkToIgor2_RemoteMessage1", "MercenaryC", NULL, NULL, NULL) &&
		LV_COM("Mission_TalkToIgor2_RemoteMessage2", "MercenaryC", Mission_StupidMercAgain, InitTalkBishop, LvN_NoArgs());
	return Mode_Done(&t, ok);
}

static byte Again_SetNextMissionN(void) {
	static const char *const parked[] = { "Parked.Police", "['SPOLI1', 'SPOLI2', 'SPOLI3']", "Parked.Mercs", "['SMERC1', 'SMERC2']",
		"Parked.TownHall", "['SBANKER1', 'SPOLI2']", "Parked.Bank", "['SBANKER1']", "Parked.Temple", "['SPOLI3']", NULL };
	if (!Sy_PhoneNext(SA, "Mission_Massacre_MissionText1", "Mission_StupidMercAgain")) {
		return 0;
	}
	Mode_Saves(parked);
	return 1;
}

MODE_FN(Again, GetMercsEnemiesProfile, 1, Again_GetMercsEnemiesProfileN(&a[0], out))
MODE_FN(Again, InitStatus, 1, Sy_MercStatusN(SA, 0))
MODE_FN(Again, Init, 1, Again_InitN())
MODE_FN(Again, InitFollow, 0, Mode_Later(10.0, SA, "CreateMerc", NULL, 0, NULL))
MODE_FN(Again, CreateMerc, 0, Again_CreateMercN(out))
MODE_FN(Again, CreateMercs, 0, Again_CreateMercsN())
MODE_FN(Again, SmartMerDeath, 2, Again_SmartMerDeathN())
MODE_FN(Again, GotoNext, 0, Sy_GotoNextN(SA, 1))
MODE_FN(Again, CreateMercPlans, 0, Again_CreateMercPlansN())
MODE_FN(Again, AddMercPlans, 1, Again_AddMercPlansN(&a[0], out))
MODE_FN(Again, StartEvasion, 0, Again_StartEvasionN())
MODE_FN(Again, CheckEvasion, 0, Again_CheckEvasionN())
MODE_FN(Again, InitTalkBishop, 0, Sy_CallFrom(SA, "Mission_StupidMercAgain", "TalkBishop", NULL, "Mission_StupidMercAgain.UsePhoneCab"))
MODE_FN(Again, SetNextMission, 0, Again_SetNextMissionN())

PyMethodDef g_ScrapyardStupidMercAgainMethods[] = {
	{ "GetMercsEnemiesProfile", Again_GetMercsEnemiesProfile, 1, NULL },
	{ "InitStatus", Again_InitStatus, 1, NULL },
	{ "Init", Again_Init, 1, NULL },
	{ "InitFollow", Again_InitFollow, 1, NULL },
	{ "CreateMerc", Again_CreateMerc, 1, NULL },
	{ "CreateMercs", Again_CreateMercs, 1, NULL },
	{ "SmartMerDeath", Again_SmartMerDeath, 1, NULL },
	{ "GotoNext", Again_GotoNext, 1, NULL },
	{ "CreateMercPlans", Again_CreateMercPlans, 1, NULL },
	{ "AddMercPlans", Again_AddMercPlans, 1, NULL },
	{ "StartEvasion", Again_StartEvasion, 1, NULL },
	{ "CheckEvasion", Again_CheckEvasion, 1, NULL },
	{ "InitTalkBishop", Again_InitTalkBishop, 1, NULL },
	{ "SetNextMission", Again_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Scrapyard)
