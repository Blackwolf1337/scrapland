/*
 * The scripts of the level GDB in C (levels/gdb/scripts of data03.packed, issue #2): the Map module (the bishops as the
 * police, the waves and the lives they give), MapMatrix (the end of the scene of the matrix) and the missions of the
 * level (Mission_IntoTheGDB, Mission_ReinsertOwnMatrix, Mission_ReinsertOwnMatrix2). MapSnd, the scene of MapMatrix and
 * the plain functions of the missions are data (data/levels/gdb/). The module variables (the bishops, the traitor,
 * the scan state, constants, models) and the module code (Map.Libraries, the save variables) stay in the scripts; the
 * functions read and write them as their globals.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h) and call the engine in C
 * (save variables, Scrap.Get, the alarm, the entity lists, the dummy search, the scorer items, the vector math). The
 * variables of the modules are read where the script reads them and written through at once; the lists of bishops are
 * changed in place on the objects of the modules (OBJLIB 5). The bishops, the restore point and the matrix are handle
 * values (OBJLIB 7). The engine functions the cases record or swap go through their seams:
 * Scrap.AddScheduledFunc, Scrap.DeleteScheduledFuncs, Scrap.SetAlarm, Scrap.UsrEntity, Scrap.Rand (fc-r7d-gd2), Scrap.CreateEntity
 * (fr6.py), Scrap.Set, SScorer.Set, SScorer.Fade, Scrap.Preload3DObject, Scrap.GetAlarm and SLogic.SetOnFloor;
 * SAI.SetStateVehicle goes through its ref (stage 7); the profiles of the bishops are VehiclesNPC.cShipAI instances
 * (OBJLIB 7).
 *
 * Direct calls (stage 3): the functions of the other ports (Speech, MissionsFuncs, RaceMaker, VehiclesNPC, OutPolice,
 * Elevator, Items, Scorer through Mode_BarDefaults), the keyword call of Speech.ForceRemoteCom and the own functions of
 * the modules (Map's attributes, the globals of the missions) go through their refs (PORT_CALL, PORT_LCALL, PORT_LOWN,
 * LV_SCORERG; the seam takes the call by name when a mission or a case has put something else there, OBJLIB 5). By
 * name: the sections of the .def files (PreSetNextMission of Mission_IntoTheGDB, SetNextMission of
 * Mission_ReinsertOwnMatrix, MapMatrix.RestoreMatrix; OBJLIB 6) and __builtin__.range of a count that is no int
 * (OBJLIB 9); the functions given to the scheduler and to the entities take the entry of a port's own function (stage
 * 4: Port_Scheduled, PYF_ENTRY; the bound method e.Del stays Python, OBJLIB 7), the ones given to Speech, Parking and
 * Elevator as callbacks are values of their variables (OBJLIB 5).
 */
#include "scrap/scrap.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL gdb

#define GM "Map"
#define MX "MapMatrix"
#define IG "Mission_IntoTheGDB"
#define R1 "Mission_ReinsertOwnMatrix"
#define R2 "Mission_ReinsertOwnMatrix2"

/* ---- shared lines ---- */

/* SScorer.Set(0, item, var, the global name of module) */
static byte Gd_Set0G(const char *item, const char *var, const char *module, const char *name) {
	nativeVal_t v;
	byte ok = Mode_G(module, name, &v) && LvN_Set0(item, var, &v);
	Native_Free(&v);
	return ok;
}

/* Scrap.SetAlarm(level) through its seam */
static byte Gd_Alarm(long level) {
	nativeVal_t v;
	Native_SetInt(&v, level);
	return Mode_SetAlarm(&v);
}

/* e = Scrap.GetEntity(the global name of module) */
static byte Gd_EntG(const char *module, const char *name, nativeVal_t *out) {
	nativeVal_t n;
	Native_SetNone(out);
	byte ok = Mode_G(module, name, &n) && Mode_EntityH(&n, out);
	Native_Free(&n);
	return ok;
}

/* Scrap.GetLangStr(key) % arg */
static byte Gd_LangPct(const char *key, const nativeVal_t *arg, nativeVal_t *out) {
	nativeVal_t t;
	Mode_Lang(key, &t);
	Native_SetNone(out);
	byte ok = Native_Arith('%', &t, arg, out);
	Native_Free(&t);
	return ok;
}

/* the count of range(1, n + 1) (n + 1 already made): range by name for a value that is no int (its errors, OBJLIB 9) */
static byte Gd_Range1(const nativeVal_t *n1, long *count) {
	if (n1->type == NV_INT) {
		*count = n1->i > 1 ? n1->i - 1 : 0;
		return 1;
	}
	nativeVal_t r;
	Native_SetNone(&r);
	byte ok = LV_CALL("__builtin__", "range", &r, "iv", 1L, n1);
	int n = ok ? Native_ForCount(&r) : -1;
	Native_Free(&r);
	*count = n;
	return n >= 0;
}

/* SVec.ModSqr(SVec.Sub(e.Pos, usr.Pos)) < (far 0) or > (far 1) MAX_SCANNING_DIST * MAX_SCANNING_DIST of IntoTheGDB:
 * 1, 0, -1 */
static int Gd_Scan(const nativeVal_t *e, const nativeVal_t *usr, byte far) {
	nativeVal_t ep, up, d, sq, m, m2, mm;
	Native_SetNone(&m);
	Native_SetNone(&ep);
	Native_SetNone(&up);
	Native_SetNone(&d);
	Native_SetNone(&sq);
	Native_SetNone(&m2);
	Native_SetNone(&mm);
	int r = -1;
	if (Mode_VGet(e, "Pos", &ep) && Mode_VGet(usr, "Pos", &up) && Mode_VecSub(&ep, &up, &d) && Mode_VecModSqr(&d, &sq) &&
		Mode_G(IG, "MAX_SCANNING_DIST", &m) && Mode_G(IG, "MAX_SCANNING_DIST", &m2) && Mode_Arith('*', &m, &m2, &mm)) {
		int c = Native_Compare(&sq, &mm);
		r = far ? c > 0 : c < 0;
	}
	Native_Free(&ep);
	Native_Free(&up);
	Native_Free(&d);
	Native_Free(&sq);
	Native_Free(&m);
	Native_Free(&m2);
	Native_Free(&mm);
	return r;
}

/* e = Scrap.GetEntity(Scrap.EntityListGet(list, "First")); while e: e.EntityList = ""; e.Del(); e = the next first */
static byte Gd_ClearList(const char *list) {
	nativeVal_t e;
	byte ok = Mode_FirstOf(list, &e);
	while (ok && Native_True(&e)) {
		ok = LvN_SetS(&e, "EntityList", "") && Mode_VDel(&e);
		Native_Free(&e);
		ok = ok && Mode_FirstOf(list, &e);
	}
	Native_Free(&e);
	return ok;
}

/* the last bishop dead: import Map; Map.GiveLife(); Map.InitPoliceBishops(); Scrap.SetAlarm(1) */
static byte Gd_LastBishop(void) {
	if (!Mode_Import("Map") || !PORT_LCALL(Map, GiveLife, NULL, "") || !PORT_LCALL(Map, InitPoliceBishops, NULL, "")) {
		return 0;
	}
	return Gd_Alarm(1);
}

/* rm: the temporary entity at the dummy DM_Mission_RestoreMatrix where the matrix is reinserted (used with use), the
 * target; the global rm of module */
static byte Gd_RestorePoint(const char *module, const char *use) {
	nativeVal_t dummy, p, a, ay, ang, name, rm, n, k;
	nativeVal_t c[3];
	Native_SetNone(&p);
	Native_SetNone(&a);
	Native_SetNone(&ay);
	Native_SetNone(&rm);
	Native_SetNone(&n);
	Native_SetEmpty(&ang, NV_TUPLE);
	LvN_Dummy("DM_Mission_RestoreMatrix", 0, &dummy);
	LvN_Str(&name, "DM_Mission_RestoreMatrix");
	int got = 0;
	byte ok = LvN_Item(&dummy, 1, &p);
	for (; ok && got < 3; got++) {
		ok = LvN_Item(&p, got, &c[got]);
	}
	if (!ok && got) {
		Native_Free(&c[--got]);
	}
	ok = ok && LvN_Create(&name, &c[0], &c[1], &c[2], "WithLife", &rm) && Mode_Put(module, "rm", NULL, &rm);
	while (got-- > 0) {
		Native_Free(&c[got]);
	}
	Native_Free(&rm);
	Native_SetInt(&k, 180);
	ok = ok && LvN_GObj(module, "rm", &rm) && LvN_Item(&dummy, 2, &a) && LvN_Item(&a, 1, &n) && Native_Arith('+', &n, &k, &ay);
	if (ok) {
		Native_SetInt(Native_Append(&ang), 0);
		Native_Copy(Native_Append(&ang), &ay);
		Native_SetInt(Native_Append(&ang), 0);
		ok = Mode_VSet(&rm, "Ang", &ang) && LvN_SetI(&rm, "VRadius", 60) && LvN_SetI(&rm, "ArrowCanUse", 1) &&
			LvN_SetI(&rm, "Life", 0) && LvN_SetI(&rm, "ArrnOpt", 0) && LvN_SetS(&rm, "ArrOpt0", "RestoreMatrix:Restore Matrix") &&
			LvN_SetS(&rm, "UseFunc", use) && LvN_SetI(&rm, "ActiveCMask", 0);
	}
	Native_Free(&n);
	if (ok) {
		ok = Mode_VGet(&rm, "Name", &n) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &n);
	}
	Native_Free(&dummy);
	Native_Free(&p);
	Native_Free(&a);
	Native_Free(&ay);
	Native_Free(&ang);
	Native_Free(&name);
	Native_Free(&rm);
	Native_Free(&n);
	return ok;
}

/* the restore point used: no target, no use, off (the global rm of module, read at every line) */
static byte Gd_RestoreUsed(const char *module) {
	nativeVal_t rm;
	byte ok = LvN_GObj(module, "rm", &rm) && LvN_Activate(&rm, 0);
	Native_Free(&rm);
	ok = ok && LvN_GObj(module, "rm", &rm) && LvN_SetS(&rm, "UseFunc", "");
	Native_Free(&rm);
	ok = ok && LvN_GObj(module, "rm", &rm) && LvN_SetI(&rm, "Active", 0);
	Native_Free(&rm);
	return ok;
}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors without police and traffic, the player is the bishop, the teleporter to the temple */
static byte GDB_InitStatus(void) {
	nativeVal_t v;
	if (!Mode_Import("Init") || !Mode_Import("Chars") || !Mode_Import("Elevator") || !Mode_Import("OutPolice")) {
		return 0;
	}
	Native_SetInt(&v, 1);
	byte ok = Mode_SetModVar("Init", "Outdoor", &v);
	Native_SetInt(&v, 0);
	ok = ok && Mode_SetModVar("OutPolice", "PoliceActive", &v) && Mode_GVarSetI("iTrafficNumber", 0);
	if (ok) {
		SaveVar_Set("Char", "Bishop");
		ok = PORT_CALL(Elevator, SetDef, NULL, "ss", "TempleTeleporter", "DM_Door_TempleTeleporter_Temple");
	}
	return ok;
}

/* Init(LevelPath): the clouds; in single player the teleporter to the temple (the arrival places the player) and the
 * bishops as the police */
static byte GDB_Init(void) {
	nativeVal_t mp, from, to;
	if (!Mode_Import("Init") || !Mode_GVarSetI("CameraCloudRed", 191) || !Mode_GVarSetI("CameraCloudGreen", 123) ||
		!Mode_GVarSetI("CameraCloudBlue", 184) || !Mode_DelSchedS("InitMercs") || !LvN_ModGet("Init", "MultiPlayer", &mp)) {
		return 0;
	}
	int multi = Native_True(&mp);
	Native_Free(&mp);
	if (multi) {
		return 1;
	}
	if (!Mode_Import("OutPolice") || !Mode_Import("Elevator")) {
		return 0;
	}
	Native_SetEmpty(&from, NV_LIST);
	LvN_Str(Native_Append(&from), "DM_Door_TempleTeleporter_Temple");
	Native_SetEmpty(&to, NV_LIST);
	LvN_Str(Native_Append(&to), "Temple");
	byte ok = PORT_CALL(Elevator, ConnectElevators, NULL, "vv", &from, &to);
	Native_Free(&from);
	Native_Free(&to);
	if (ok && LvN_SaveIs("ComeFrom", "DoorTempleTeleporter")) {
		ok = LvN_ModSetGlobal("Elevator", "OnElevatorReachDown", GM, "EndOfAppears");
	}
	int bishops = ok ? LvN_GTrue(GM, "CreateBishops") : -1;
	return bishops > 0 ? PORT_LOWN(Map, InitPoliceBishops, NULL, "") : bishops == 0;
}

/* InitPoliceBishops(): the alarm meter, the four waves of bishops and archbishops, the first one */
static byte GDB_InitPoliceBishops(void) {
	static const int waves[4] = { 3, 2, 1, 0 };
	nativeVal_t v;
	if (!Mode_Import("OutPolice")) {
		return 0;
	}
	Native_SetFloat(&v, -0.05);
	byte ok = Mode_GVarSet("AlarmFallDelta", &v);
	Native_SetFloat(&v, 0.1);
	ok = ok && Mode_GVarSet("AlarmGrowDelta", &v) && Mode_GVarSetS("PoliceTauntRace", "Bishop");
	for (int i = 0; i < 4 && ok; i++) {
		nativeVal_t wave;
		Native_SetEmpty(&wave, NV_LIST);
		for (int k = 0; k < 6; k++) {
			LvN_Str(Native_Append(&wave), k < waves[i] ? "SBishop1" : "SArchbishop1");
		}
		ok = LvN_ModAppend("OutPolice", "PoliceWaves", &wave);
		Native_Free(&wave);
	}
	return ok && PORT_CALL(OutPolice, InitShips, NULL, "") && Mode_GVarSetS("PoliceWaveEliminated", "Map.NextWave");
}

/* EndOfAppears(elv): the arrival by the teleporter: the player at the spawn point, the message of the waves (three
 * times) */
static byte GDB_EndOfAppears(void) {
	nativeVal_t dummy, v, pos, name, msg, doit, five, text, one, left;
	if (!Mode_Import("Elevator") || !Mode_Import("RaceMaker") || !LvN_ModNone("Elevator", "OnElevatorReachDown")) {
		return 0;
	}
	Native_SetNone(&v);
	Native_SetNone(&pos);
	Native_SetNone(&name);
	Native_SetNone(&doit);
	Native_SetNone(&text);
	Native_SetNone(&left);
	Native_SetNone(&msg);
	LvN_Dummy("DM_Player_Spawn_Temple", 0, &dummy);
	byte ok = Mode_PrintV(&dummy) && Mode_UsrHandle(&v) && LvN_Item(&dummy, 1, &pos) && Mode_VSet(&v, "Pos", &pos) &&
		Mode_VGet(&v, "Name", &name) && LvN_SetOnFloor(&name);
	if (ok) {
		Mode_SaveVar("GDB.BishopsMsg", &msg);
		ok = Native_ToInt(&msg, &doit);
	}
	if (ok && Native_True(&doit)) {
		Native_SetInt(&five, 5);
		Native_SetInt(&one, 1);
		ok = Gd_LangPct("OnBishopsWave3", &five, &text) && PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text) &&
			Native_Arith('-', &doit, &one, &left);
		if (ok) {
			LvN_SaveRepr("GDB.BishopsMsg", &left);
		}
	}
	Native_Free(&dummy);
	Native_Free(&v);
	Native_Free(&pos);
	Native_Free(&name);
	Native_Free(&msg);
	Native_Free(&doit);
	Native_Free(&text);
	Native_Free(&left);
	return ok;
}

/* GiveLife(): a wave beaten with the alarm on: up to five lives, at most MaxPlayerLives */
static byte GDB_GiveLife(void) {
	nativeVal_t s, lives, max, five, sum, incl, text, total;
	if (!Mode_Import("RaceMaker")) {
		return 0;
	}
	Native_SetNone(&lives);
	Native_SetNone(&max);
	Native_SetNone(&sum);
	Native_SetNone(&incl);
	Native_SetNone(&text);
	Native_SetNone(&total);
	Native_SetInt(&five, 5);
	Mode_SaveVar("Player.NumLives", &s);
	byte ok = Native_ToInt(&s, &lives) && Native_GVarGet("MaxPlayerLives", &max);
	if (ok && Native_Compare(&lives, &max) < 0) {
		Native_Free(&max);
		ok = Native_Arith('+', &lives, &five, &sum) && Native_GVarGet("MaxPlayerLives", &max);
		if (ok && Native_Compare(&sum, &max) > 0) {
			Native_Free(&max);
			ok = Native_GVarGet("MaxPlayerLives", &max) && Native_Arith('-', &max, &lives, &incl);
		} else if (ok) {
			Native_SetInt(&incl, 5);
		}
		ok = ok && Gd_LangPct("OnBishopsWave", &incl, &text) && PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text) &&
			Native_Arith('+', &lives, &incl, &total);
		if (ok) {
			LvN_SaveRepr("Player.NumLives", &total);
		}
	} else if (ok) {
		Mode_Lang("OnBishopsWave2", &text);
		ok = PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text);
	}
	if (ok) {
		SaveVar_Inc("Stats.GDB", 1);
	}
	Native_Free(&s);
	Native_Free(&lives);
	Native_Free(&max);
	Native_Free(&sum);
	Native_Free(&incl);
	Native_Free(&text);
	Native_Free(&total);
	return ok;
}

/* NextWave(): the engine's callback of a wave eliminated: lives while the alarm is on, the next wave */
static byte GDB_NextWave(void) {
	if (!Mode_Import("OutPolice")) {
		return 0;
	}
	int on = Mode_AlarmOn();
	return on >= 0 && (!on || PORT_LOWN(Map, GiveLife, NULL, "")) && PORT_CALL(OutPolice, NextWave, NULL, "i", 0L) &&
		Mode_GVarSetS("PoliceWaveEliminated", "Map.NextWave");
}

LV_CORE0(GDB_InitStatus)
LV_CORE0(GDB_Init)
LV_CORE0(GDB_InitPoliceBishops)
LV_CORE0(GDB_EndOfAppears)
LV_CORE0(GDB_GiveLife)
LV_CORE0(GDB_NextWave)

LV_FN(GDB, InitStatusPy, 1, GDB_InitStatus_C)
LV_FN(GDB, InitPy, 1, GDB_Init_C)
LV_FN(GDB, InitPoliceBishopsPy, 0, GDB_InitPoliceBishops_C)
LV_FN(GDB, EndOfAppearsPy, 1, GDB_EndOfAppears_C)
LV_FN(GDB, GiveLifePy, 0, GDB_GiveLife_C)
LV_FN(GDB, NextWavePy, 0, GDB_NextWave_C)

PyMethodDef g_GDBMapMethods[] = {
	{ "InitStatus", GDB_InitStatusPy, 1, NULL },
	{ "Init", GDB_InitPy, 1, NULL },
	{ "InitPoliceBishops", GDB_InitPoliceBishopsPy, 1, NULL },
	{ "EndOfAppears", GDB_EndOfAppearsPy, 1, NULL },
	{ "GiveLife", GDB_GiveLifePy, 1, NULL },
	{ "NextWave", GDB_NextWavePy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapMatrix ---- */

/* EndOFScene(usr = None): the scene of the matrix ends: the fade, then the function RestoreMatrix was given */
MODE_KW(Matrix, EndOFScene, "|O", "usr") {
	nativeVal_t fn;
	if (!Mode_Fade(0, lv_black, lv_clear, 1.0)) {
		return 0;
	}
	/* EndFunc is the function value RestoreMatrix put into the module (None, or a function of the data: OBJLIB 6, held
	 * by reference; a port's function through its entry) */
	int t = Mode_RefG(MX, "EndFunc", &fn) ? Mode_Truth(&fn) : -1;
	byte ok = t >= 0 && (!t || Mode_CallArgs(&fn, NULL, NULL, 0));
	Native_Free(&fn);
	return ok;
}

PyMethodDef g_GDBMapMatrixMethods[] = {
	{ "EndOFScene", (PyCFunction)(void (*)(void))Matrix_EndOFScene, METH_VARARGS | METH_KEYWORDS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_IntoTheGDB: scan the bishops for the traitor, kill him for Gus' matrix and reinsert it ---- */

static const char *const ig_bishop_profile[] = { "Ship=SBISHOP1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=1",
	"GoNextItemRace=0", "ControlHookRace=0", "CadenceShootCM=2", "UsePercSonic=100", "UsePercCloud=0", "UsePercElectro=0",
	"UsePercHook=0", "FactorHookMap=5", "CadenceShoot=15", "ControlAim=1", "AngAttack=2.5", "CounterAttack=0.0", "ControlBoost=1",
	"ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.2", "FactorDamageItemLife=0.50", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=1", "UsePercHookGetAway=3", "TimeMaxStrafe=6", "VelMaxStrafe=0.75", NULL };

/* GetBishopProfile(): the cShipAI of the table */
MODE_FN(Into, GetBishopProfile, 0, LvN_Profile(ig_bishop_profile, out))

/* the profile of the bishops of a mission (0 IntoTheGDB, 1 ReinsertOwnMatrix, 2 ReinsertOwnMatrix2) through its global
 * GetBishopProfile */
static byte Gd_BishopProfile(int mission, nativeVal_t *pd) {
	if (mission == 0) {
		return PORT_LOWN(Mission_IntoTheGDB, GetBishopProfile, pd, "");
	}
	if (mission == 1) {
		return PORT_LOWN(Mission_ReinsertOwnMatrix, GetBishopProfile, pd, "");
	}
	return PORT_LOWN(Mission_ReinsertOwnMatrix2, GetBishopProfile, pd, "");
}

/* the bishop of a mission (Gd_BishopProfile): VehiclesNPC.MakeAI, the profile of the mission, its life */
static byte Gd_MakeBishop(int mission, nativeVal_t *bishop) {
	nativeVal_t pd;
	Native_SetNone(&pd);
	byte ok = PORT_CALL(VehiclesNPC, MakeAI, bishop, "ss", "SBishop1", "Bishop") && Gd_BishopProfile(mission, &pd) &&
		PORT_CALL(VehiclesNPC, ModAIProff, NULL, "vv", bishop, &pd);
	Native_Free(&pd);
	return ok;
}

/* bishops.append(bishop.Name) of the module, in place */
static byte Gd_AppendName(const char *module, const nativeVal_t *bishop) {
	nativeVal_t n;
	Native_SetNone(&n);
	byte ok = Mode_VGet(bishop, "Name", &n) && Mode_Append(module, "bishops", &n);
	Native_Free(&n);
	return ok;
}

/* `if not Scrap.UsrEntity(0): return` of the bishops: 1 with the player in *usr, 0 without one (*usr None), -1 */
static int Gd_UsrThere(nativeVal_t *usr) {
	if (!Mode_UsrHandle(usr)) {
		Native_SetNone(usr);
		return -1;
	}
	return Native_True(usr);
}

/* Init(LevelPath): the lists of the bishops, the eight bishops (identified when the matrix is already taken), the
 * restore point or the scan when the player boards */
static byte Into_Init(void) {
	nativeVal_t model, state, ident;
	if (!Mode_Import("Parking") || !Mode_G(IG, "MatrixModel", &model)) {
		return 0;
	}
	byte ok = Mode_Preload(&model);
	Native_Free(&model);
	if (ok) {
		CHECK(cWorld_NewEntityList(g_pWorld, "Bishops"));
		ok = Mode_ListSet("Bishops", "OnDeath", "Mission_IntoTheGDB.OnBishopsDeath") &&
			Mode_ListSet("Bishops", "OnDamage", "Mission_IntoTheGDB.OnBishopsDamage") &&
			Mode_ListSet("Enemies", "OnDeath", "Mission_IntoTheGDB.OnBishopsDeath") &&
			Mode_ListSet("Enemies", "OnDamage", "Mission_IntoTheGDB.OnBishopsDamage");
	}
	if (!ok) {
		return 0;
	}
	Mode_SaveVar("MissionState", &state);
	Native_SetInt(&ident, LvN_EqS(&state, "RestoreMatrix"));
	ok = PORT_LOWN(Mission_IntoTheGDB, CreateBishops, NULL, "iv", 8L, &ident);
	int restore = LvN_EqS(&state, "RestoreMatrix");
	Native_Free(&state);
	if (!ok) {
		return 0;
	}
	if (restore) {
		return PORT_LOWN(Mission_IntoTheGDB, RestoreMatrix, NULL, "");
	}
	return LvN_Com("Mission_IntoTheGDB_RemoteMessage1", "DeepThroat", NULL, NULL, NULL) &&
		LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", IG, "InitMission") &&
		LvN_ModSetGlobal("Parking", "SwitchCharMissionCallback", IG, "StopMission");
}

/* CreateBishop(bishopIdentified): a bishop with the player there: unidentified ones wait in the list Bishops as
 * targets, identified ones are enemies */
static byte Into_CreateBishop(const nativeVal_t *ident, nativeVal_t *out) {
	nativeVal_t usr, name;
	if (!Mode_Import("VehiclesNPC")) {
		return 0;
	}
	int there = Gd_UsrThere(&usr);
	Native_Free(&usr);
	if (there <= 0) {
		return there == 0;
	}
	Native_SetNone(&name);
	byte ok = Gd_MakeBishop(0, out) && LvN_Life(out, IG, "BISHOP_LIFE");
	if (ok && !Native_True(ident)) {
		ok = LvN_SetS(out, "EntityList", "Bishops") && LvN_Activate(out, 1) && Mode_VGet(out, "Name", &name) &&
			PORT_CALL(SAI, SetStateVehicle, NULL, "ivi", 3L, &name, 0L);
	} else if (ok) {
		ok = LvN_SetS(out, "EntityList", "Enemies");
	}
	Native_Free(&name);
	return ok;
}

/* CreateBishops(numBishops, bishopIdentified): the bishops, one of them the traitor (who drops nothing) */
static byte Into_CreateBishops(const nativeVal_t *num, const nativeVal_t *ident) {
	nativeVal_t one, n1, bishop, f, d, hi, zero, r, i, list, traitor, e;
	long count = 0;
	Native_SetInt(&one, 1);
	Native_SetNone(&n1);
	if (!Native_Arith('+', num, &one, &n1) || !Gd_Range1(&n1, &count)) {
		Native_Free(&n1);
		return 0;
	}
	Native_Free(&n1);
	byte ok = 1;
	for (long k = 0; k < count && ok; k++) {
		Native_SetNone(&bishop);
		ok = PORT_LOWN(Mission_IntoTheGDB, CreateBishop, &bishop, "v", ident) && Gd_AppendName(IG, &bishop);
		Native_Free(&bishop);
	}
	if (!ok || Native_True(ident)) {
		return ok;
	}
	Native_SetNone(&f);
	Native_SetNone(&hi);
	Native_SetNone(&r);
	Native_SetNone(&i);
	Native_SetNone(&list);
	Native_SetNone(&traitor);
	Native_SetNone(&e);
	Native_SetFloat(&d, 0.05);
	Native_SetFloat(&zero, 0.0);
	ok = Native_ToFloat(num, &f) && Native_Arith('-', &f, &d, &hi) && Mode_Rand(&zero, &hi, &r) && Native_ToInt(&r, &i) &&
		LvN_GElems(IG, "bishops", &list) && Native_Subscript(&list, &i, &traitor) && Mode_Put(IG, "traitor", NULL, &traitor) &&
		Gd_EntG(IG, "traitor", &e);
	if (ok && Native_True(&e)) {
		ok = LvN_SetI(&e, "Money", -1);
	}
	Native_Free(&f);
	Native_Free(&hi);
	Native_Free(&r);
	Native_Free(&i);
	Native_Free(&list);
	Native_Free(&traitor);
	Native_Free(&e);
	return ok;
}

/* InitMission(): the bishops as targets, the scorer of the scan, the scan every second */
static byte Into_InitMission(void) {
	if (!Mode_Import("Parking")) {
		return 0;
	}
	/* OBJLIB 5: the list object of the module, as the script passes it (P: the entry keeps it as the targets) */
	nativeVal_t bishops;
	byte ok = Mode_RefG(IG, "bishops", &bishops) && PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &bishops);
	Native_Free(&bishops);
	return ok && LV_SCORERG(Mission_IntoTheGDB, MissionScorer) && LvN_Draw() &&
		Mode_GVarSetS("OnFinishAutoUpdate", "Mission_IntoTheGDB.OnFinishScan") &&
		Mode_Later(1.0, IG, "ScanShip", NULL, 0, "ScanShip");
}

/* ScanShip(): every second: the bishop being scanned stays near, else the next bishop near the player is scanned */
static byte Into_ScanShip(void) {
	nativeVal_t usr, e, blink, list, cur;
	Native_SetNone(&e);
	Native_SetNone(&list);
	int there = Gd_UsrThere(&usr);
	if (there <= 0) {
		Native_Free(&usr);
		return there == 0;
	}
	byte ok = Mode_Later(1.0, IG, "ScanShip", NULL, 0, "ScanShip") && Gd_EntG(IG, "currentScannedShip", &e);
	if (ok && Native_True(&e)) {
		int far = Gd_Scan(&e, &usr, 1);
		if (far > 0) {
			Mode_ScorerGet(0, "ScanRemainingTime", "Blink", &blink);
			if (LvN_EqI(&blink, 0)) {
				ok = LvN_ScanBlink(1) && LvN_LaterG(IG, "MAX_TIME_TARGET_LOST", "CheckContinueScanning", NULL, 0);
			}
			Native_Free(&blink);
		} else {
			ok = far == 0 && LvN_ScanBlink(0);
		}
		Native_Free(&e);
		Native_Free(&usr);
		return ok;
	}
	Native_Free(&e);
	ok = ok && LvN_GElems(IG, "bishops", &list);
	int n = ok ? Native_ForCount(&list) : 0;
	ok = ok && n >= 0;
	for (int k = 0; k < n && ok; k++) {
		nativeVal_t s;
		Native_SetNone(&s);
		Native_SetNone(&e);
		ok = Native_SeqItem(&list, k, &s) && Mode_EntityH(&s, &e);
		int near = ok && Native_True(&e) ? Gd_Scan(&e, &usr, 0) : 0;
		ok = ok && near >= 0;
		if (near > 0) {
			Native_SetNone(&cur);
			ok = Mode_VGet(&e, "Name", &cur) && Mode_Put(IG, "currentScannedShip", NULL, &cur);
			Native_Free(&cur);
			ok = ok && Mode_G(IG, "currentScannedShip", &cur) && Mode_GVarSet("CurrentWaypointName", &cur);
			Native_Free(&cur);
			ok = ok && LvN_Set0I("ScanStatusText", "Visible", 1) && LvN_ScanBlink(0) &&
				Gd_Set0G("ScanRemainingTime", "MaxValue", IG, "MAX_SCANNING_TIME") &&
				Gd_Set0G("ScanRemainingTime", "Value", IG, "MAX_SCANNING_TIME") && LvN_Set0I("ScanRemainingTime", "Visible", 1);
			n = 0;
		}
		Native_Free(&s);
		Native_Free(&e);
	}
	Native_Free(&list);
	Native_Free(&usr);
	return ok;
}

/* `usr and e and the bishop near`: 1, 0, -1 */
static int Gd_Scanning(const nativeVal_t *usr, const nativeVal_t *e) {
	return Native_True(usr) && Native_True(e) ? Gd_Scan(e, usr, 0) : 0;
}

/* CheckContinueScanning(): the bishop came back near in time, or the scan starts again */
static byte Into_CheckContinueScanning(void) {
	nativeVal_t usr, e;
	Native_SetNone(&e);
	if (!Mode_UsrHandle(&usr)) {
		return 0;
	}
	int near = Gd_EntG(IG, "currentScannedShip", &e) ? Gd_Scanning(&usr, &e) : -1;
	Native_Free(&usr);
	Native_Free(&e);
	return near > 0 ? LvN_ScanBlink(0) : near == 0 && PORT_LOWN(Mission_IntoTheGDB, ResetScan, NULL, "");
}

/* ResetScan(): no bishop scanned, the scorer hidden (refilled a moment later) */
static byte Into_ResetScan(void) {
	if (!LvN_PutS(IG, "currentScannedShip", "")) {
		return 0;
	}
	int found = LvN_GTrue(IG, "traitorFound");
	return found >= 0 && (found || Mode_GVarSetS("CurrentWaypointName", "")) && LvN_Set0I("ScanRemainingTime", "Visible", 0) &&
		LvN_Set0I("ScanStatusText", "Visible", 0) && Mode_Later(0.25, IG, "ResetScan2", NULL, 0, NULL);
}

/* the bishops of the list (read as the loop starts) that are there, not the traitor (skip: the global traitor read for
 * each), stop being targets */
static byte Gd_UntargetBishops(byte skipTraitor) {
	nativeVal_t list;
	if (!LvN_GElems(IG, "bishops", &list)) {
		return 0;
	}
	int n = Native_ForCount(&list);
	byte ok = n >= 0;
	for (int k = 0; k < n && ok; k++) {
		nativeVal_t s, e, name, traitor;
		Native_SetNone(&s);
		Native_SetNone(&e);
		Native_SetNone(&name);
		Native_SetNone(&traitor);
		ok = Native_SeqItem(&list, k, &s) && Mode_EntityH(&s, &e);
		int t = ok && Native_True(&e);
		if (t && skipTraitor) {
			ok = Mode_VGet(&e, "Name", &name) && Mode_G(IG, "traitor", &traitor);
			t = ok && Native_Compare(&name, &traitor) != 0;
		}
		if (t) {
			ok = LvN_Activate(&e, 0);
		}
		Native_Free(&s);
		Native_Free(&e);
		Native_Free(&name);
		Native_Free(&traitor);
	}
	Native_Free(&list);
	return ok;
}

/* OnFinishScan(): the scan bar ran out: the traitor found (the others stop being targets), or one bishop fewer */
static byte Into_OnFinishScan(void) {
	nativeVal_t usr, cur, traitor, list, e;
	if (!LvN_Set0I("ScanStatusText", "Visible", 0) || !LvN_Set0I("ScanRemainingTime", "Visible", 0)) {
		return 0;
	}
	int there = Gd_UsrThere(&usr);
	Native_Free(&usr);
	if (there <= 0) {
		return there == 0;
	}
	Native_SetNone(&traitor);
	byte ok = Mode_G(IG, "currentScannedShip", &cur) && Mode_G(IG, "traitor", &traitor);
	if (ok && !Native_Compare(&cur, &traitor)) {
		ok = LvN_PutI(IG, "traitorFound", 1) && Mode_DelSchedS("ScanShip") &&
			LvN_Com("Mission_IntoTheGDB_RemoteMessage4", "DeepThroat", NULL, NULL, NULL) && Gd_UntargetBishops(1);
	} else if (ok) {
		Native_Free(&cur);
		Native_SetNone(&list);
		ok = Mode_GVarSetS("CurrentWaypointName", "") && LvN_Com("Mission_IntoTheGDB_RemoteMessage3", "DeepThroat", NULL, NULL, NULL) &&
			Mode_G(IG, "currentScannedShip", &cur) && LvN_GElems(IG, "bishops", &list);
		int in = ok ? Native_Contains(&list, &cur) : -1;
		ok = in >= 0;
		if (in > 0) {
			Native_Free(&cur);
			ok = Mode_G(IG, "currentScannedShip", &cur) && Mode_Remove(IG, "bishops", &cur);
		}
		Native_Free(&list);
		Native_SetNone(&e);
		ok = ok && Gd_EntG(IG, "currentScannedShip", &e);
		if (ok && Native_True(&e)) {
			ok = LvN_Activate(&e, 0);
		}
		Native_Free(&e);
	}
	Native_Free(&cur);
	Native_Free(&traitor);
	return ok && PORT_LOWN(Mission_IntoTheGDB, ResetScan, NULL, "");
}

/* if x in bishops: bishops.remove(x) of the module, in place */
static byte Gd_RemoveIn(const char *module, const nativeVal_t *x) {
	nativeVal_t list;
	if (!LvN_GElems(module, "bishops", &list)) {
		return 0;
	}
	int in = Native_Contains(&list, x);
	Native_Free(&list);
	return in > 0 ? Mode_Remove(module, "bishops", x) : in == 0;
}

/* `if len(bishops) == 0:` the last bishop of the module */
static byte Gd_IfLast(const char *module) {
	nativeVal_t list;
	if (!LvN_GElems(module, "bishops", &list)) {
		return 0;
	}
	int n = Native_Size(&list);
	Native_Free(&list);
	return n < 0 ? 0 : n == 0 ? Gd_LastBishop() : 1;
}

/* OnBishopsDeath(LooserName, WinnerName): the player killed the traitor: the matrix drops; the last bishop dead: a
 * life and the bishops as the police */
static byte Into_OnBishopsDeath(const nativeVal_t *looser, const nativeVal_t *winner) {
	nativeVal_t usr, name, traitor, e, pos, cur, msg;
	if (!Mode_Import("Vehicles") || !Mode_Import("Map") || !Gd_RemoveIn(IG, looser) || !Mode_UsrHandle(&usr)) {
		return 0;
	}
	Native_SetNone(&name);
	Native_SetNone(&traitor);
	Native_SetNone(&e);
	Native_SetNone(&pos);
	Native_SetNone(&cur);
	byte ok = 1;
	int t = Native_True(&usr);
	if (t) {
		ok = Mode_VGet(&usr, "Name", &name);
		t = ok && !Native_Compare(&name, winner);
	}
	if (t) {
		ok = Mode_G(IG, "traitor", &traitor);
		t = ok && !Native_Compare(looser, &traitor);
	}
	if (t) {
		ok = Mode_EntityH(looser, &e);
		if (ok && Native_True(&e)) {
			LvN_Str(&msg, "Grabe ahora!");
			ok = Mode_PrintV(&msg) && Mode_GVarSetS("CurrentWaypointName", "") && Mode_DelSchedS("ScanShip") &&
				LvN_PutS(IG, "traitor", "") && Mode_VGet(&e, "Pos", &pos) && PORT_LOWN(Mission_IntoTheGDB, DropMatrix, NULL, "v", &pos);
			Native_Free(&msg);
		}
	}
	ok = ok && Mode_G(IG, "currentScannedShip", &cur);
	if (ok && !Native_Compare(&cur, looser)) {
		ok = PORT_LOWN(Mission_IntoTheGDB, ResetScan, NULL, "");
	}
	Native_Free(&usr);
	Native_Free(&name);
	Native_Free(&traitor);
	Native_Free(&e);
	Native_Free(&pos);
	Native_Free(&cur);
	return ok && Gd_IfLast(IG);
}

/* OnBishopsDamage(LooserName, WinnerName, Damage): a bishop shot: all of them become enemies */
static byte Into_OnBishopsDamage(void) {
	nativeVal_t list, e, name;
	if (!Mode_Import("VehiclesNPC") || !LvN_Com("Mission_IntoTheGDB_RemoteMessage6", "Bishop", NULL, NULL, NULL)) {
		return 0;
	}
	Native_SetEmpty(&list, NV_LIST);
	byte ok = Mode_FirstOf("Bishops", &e);
	while (ok && Native_True(&e)) {
		nativeVal_t next;
		Native_SetNone(&name);
		Native_SetNone(&next);
		ok = Mode_VGet(&e, "Name", &name) && Mode_VGet(&e, "NextInList", &next);
		if (ok) {
			Native_Copy(Native_Append(&list), &name);
			Native_Free(&e);
			ok = Mode_EntityH(&next, &e);
		}
		Native_Free(&name);
		Native_Free(&next);
	}
	Native_Free(&e);
	for (int k = 0; k < list.n && ok; k++) {
		Native_SetNone(&e);
		ok = Mode_EntityH(&list.items[k], &e) && (!Native_True(&e) || LvN_SetS(&e, "EntityList", "Enemies"));
		Native_Free(&e);
	}
	Native_Free(&list);
	return ok && Mode_ListSet("Bishops", "OnDamage", "") && Mode_ListSet("Enemies", "OnDamage", "");
}

/* Speech.ForceRemoteCom(key, who, AfterRemoteComFunc = func, Parameters = (param,)) (func the function object, OBJLIB
 * 5) */
static byte Gd_ForceRemote(const char *key, const char *who, const nativeVal_t *func, const nativeVal_t *param) {
	nativeVal_t params;
	Native_SetEmpty(&params, NV_TUPLE);
	Native_Copy(Native_Append(&params), param);
	byte ok = PORT_CALL(Speech, ForceRemoteCom, NULL, "ss;AfterRemoteComFunc=v,Parameters=v", key, who, func, &params);
	Native_Free(&params);
	return ok;
}

/* DropMatrix(pos): Gus' matrix (not takeable yet) and its arrow where the traitor died; Deep Throat's message makes it
 * takeable */
static byte Into_DropMatrix(const nativeVal_t *pos) {
	nativeVal_t name, l, model, l2, tgt, n, empty;
	LvN_Str(&name, "GusMatrix");
	Native_SetNone(&l);
	Native_SetNone(&model);
	Native_SetNone(&l2);
	Native_SetNone(&n);
	byte ok = LvN_CreateAt(&name, pos, "Item", &l) && Mode_G(IG, "MatrixModel", &model) && Mode_VModel(&l, &model) &&
		LvN_SetI(&l, "Radius", 3500) && LvN_SetI(&l, "AppearsIfFree", 0) && LvN_SetI(&l, "AutoFace", 0) &&
		LvN_SetI(&l, "OnlyUser", 1) && LvN_SetF(&l, "rTime", 0.25) && LvN_SetS(&l, "OnTake", "") && Mode_Import("Items") &&
		PORT_CALL(Items, SetSound, NULL, "vs", &l, "TakeMatrixExt");
	Native_Free(&name);
	LvN_Str(&name, "MissionCheckArrow");
	ok = ok && LvN_CreateAt(&name, pos, "WithLife", &l2);
	if (ok) {
		Native_SetEmpty(&tgt, NV_TUPLE);
		Native_SetInt(Native_Append(&tgt), 0);
		Native_SetInt(Native_Append(&tgt), 1000);
		Native_SetInt(Native_Append(&tgt), 0);
		ok = Mode_VSet(&l2, "CamTgt", &tgt);
		Native_Free(&tgt);
	}
	ok = ok && Gd_UntargetBishops(0) && Mode_VGet(&l2, "Name", &n) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &n);
	Native_Free(&n);
	if (ok) {
		Native_SetEmpty(&empty, NV_LIST);
		ok = Mode_SetModVar("Speech", "RemoteCom", &empty);
		Native_Free(&empty);
	}
	if (ok) {
		nativeVal_t fn;
		ok = Mode_RefG(IG, "ReinsertMatrix", &fn) && Mode_VGet(&l, "Name", &n) &&
			Gd_ForceRemote("Mission_IntoTheGDB_RemoteMessage5", "DeepThroat", &fn, &n);
		Native_Free(&fn);
	}
	Native_Free(&name);
	Native_Free(&l);
	Native_Free(&model);
	Native_Free(&l2);
	Native_Free(&n);
	return ok;
}

/* ActivateMatrix(name): the matrix can be taken */
static byte Into_ActivateMatrix(const nativeVal_t *name) {
	nativeVal_t e;
	Native_SetNone(&e);
	byte ok = Mode_EntityH(name, &e) && (!Native_True(&e) || LvN_SetS(&e, "OnTake", "Mission_IntoTheGDB.TakeMatrix"));
	Native_Free(&e);
	return ok;
}

/* TakeMatrix(ItemName, CharName): the player took the matrix: to the restore point */
static byte Into_TakeMatrix(const nativeVal_t *ch) {
	nativeVal_t usr, e, t;
	if (!Mode_UsrHandle(&usr)) {
		return 0;
	}
	int is = LvN_UsrNameIs(&usr, ch);
	Native_Free(&usr);
	if (is <= 0) {
		return is == 0;
	}
	byte ok = LvN_Entity("GusMatrix", &e);
	if (ok && Native_True(&e)) {
		ok = LvN_Activate(&e, 0) && LvN_SetI(&e, "rTime", 20);
		/* OBJLIB 7: the scheduler keeps the bound method e.Del (Python until the entities are C) */
		nativeVal_t del = { 0 };
		ok = ok && Mode_VBoundV(&e, "Del", &del);
		Native_SetFloat(&t, Mode_Time() + 5);
		ok = ok && Mode_ScheduleV(&t, &del, NULL, 0, NULL);
		Native_Free(&del);
	}
	Native_Free(&e);
	ok = ok && LvN_Entity("MissionCheckArrow", &e) && (!Native_True(&e) || Mode_VDel(&e));
	Native_Free(&e);
	if (ok) {
		SaveVar_Set("MissionState", "RestoreMatrix");
	}
	return ok && PORT_LOWN(Mission_IntoTheGDB, RestoreMatrix, NULL, "");
}

static byte Into_RestoreMatrix(void) {
	return Gd_RestorePoint(IG, "Mission_IntoTheGDB.LaunchRestoreMatrixAnim");
}

/* LaunchRestoreMatrixAnim(UsrName, CharName, optId, FullOption): the bishops go, the scene of the matrix plays */
static byte Into_LaunchRestoreMatrixAnim(void) {
	/* OBJLIB 6: PreSetNextMission and MapMatrix.RestoreMatrix are functions of the data
	 * (data/levels/gdb/mission_intothegdb.def, the scene of MapMatrix) */
	return Gd_RestoreUsed(IG) && PORT_LOWN(Mission_IntoTheGDB, RelaxBishops, NULL, "") && LV_OWN(IG, "PreSetNextMission", NULL, "") &&
		Mode_Import("MapMatrix") && LV_CALL("MapMatrix", "RestoreMatrix", NULL, "");
}

/* RelaxBishops(): the police, the bishops and the enemies go, no alarm, the bishops as the police again */
static byte Into_RelaxBishops(void) {
	if (!Mode_Import("OutPolice") || !Mode_Import("Map") || !Gd_ClearList("OutCop") || !Gd_ClearList("Bishops") ||
		!Gd_ClearList("Enemies")) {
		return 0;
	}
	return Gd_Alarm(0) && PORT_LCALL(Map, InitPoliceBishops, NULL, "");
}

/* MissionScorer(id): the scan status and its bar, visible while a bishop near the player is scanned */
static byte Into_MissionScorer(const nativeVal_t *id) {
	nativeVal_t e, usr, vis, x, y, label, it, v, max;
	if (!Gd_EntG(IG, "currentScannedShip", &e)) {
		return 0;
	}
	if (!Mode_UsrHandle(&usr)) {
		Native_Free(&e);
		return 0;
	}
	int scanning = Gd_Scanning(&usr, &e);
	Native_Free(&e);
	Native_Free(&usr);
	if (scanning < 0) {
		return 0;
	}
	Native_SetInt(&vis, scanning > 0);
	Native_SetNone(&y);
	Native_SetNone(&max);
	LvN_Str(&it, "ScanRemainingTime");
	byte ok = LvN_TextX(0, &x) && LvN_TextY(0, &y);
	if (ok) {
		LvN_Label("Mission_IntoTheGDB_Scorer1", &label);
		ok = LvN_MissionText(id, "ScanStatusText", "Text", &x, &y, "Horatio", "Shadow", &label, "Left", &vis);
		Native_Free(&label);
	}
	/* the function is looked up after the colours now (Mode_BarDefaults; the same unless both are gone) */
	ok = ok && Mode_ScorerAdd(id, &it, "Bar") && Mode_BarDefaults(id, "ScanRemainingTime", NULL, NULL);
	Native_SetInt(&v, 1);
	ok = ok && LvN_ScorerSet(id, "ScanRemainingTime", "IsAutoUpdatable", &v) && Mode_G(IG, "MAX_SCANNING_TIME", &max) &&
		LvN_ScorerSet(id, "ScanRemainingTime", "MaxValue", &max);
	if (ok && !scanning) {
		Native_Free(&max);
		ok = Mode_G(IG, "MAX_SCANNING_TIME", &max) && LvN_ScorerSet(id, "ScanRemainingTime", "Value", &max);
	}
	ok = ok && LvN_ScorerSet(id, "ScanRemainingTime", "Visible", &vis);
	Native_Free(&x);
	Native_Free(&y);
	Native_Free(&it);
	Native_Free(&max);
	return ok;
}

LV_CORE0(Into_Init)
LV_CORE0(Into_InitMission)
LV_CORE0(Into_ScanShip)
LV_CORE0(Into_CheckContinueScanning)
LV_CORE0(Into_ResetScan)
LV_CORE0(Into_OnFinishScan)
LV_CORE0(Into_OnBishopsDamage)
LV_CORE0(Into_RestoreMatrix)
LV_CORE0(Into_LaunchRestoreMatrixAnim)
LV_CORE0(Into_RelaxBishops)

static byte Into_CreateBishop_C(const nativeVal_t *a, nativeVal_t *out) {
	return Into_CreateBishop(&a[0], out);
}

static byte Into_CreateBishops_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Into_CreateBishops(&a[0], &a[1]);
}

static byte Into_OnBishopsDeath_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Into_OnBishopsDeath(&a[0], &a[1]);
}

static byte Into_DropMatrix_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Into_DropMatrix(&a[0]);
}

static byte Into_ActivateMatrix_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Into_ActivateMatrix(&a[0]);
}

static byte Into_TakeMatrix_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Into_TakeMatrix(&a[1]);
}

static byte Into_MissionScorer_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Into_MissionScorer(&a[0]);
}

LV_FN(Into, InitPy, 1, Into_Init_C)
LV_FN(Into, CreateBishopPy, 1, Into_CreateBishop_C)
LV_FN(Into, CreateBishopsPy, 2, Into_CreateBishops_C)
LV_FN(Into, InitMissionPy, 0, Into_InitMission_C)
LV_FN(Into, ScanShipPy, 0, Into_ScanShip_C)
LV_FN(Into, CheckContinueScanningPy, 0, Into_CheckContinueScanning_C)
LV_FN(Into, ResetScanPy, 0, Into_ResetScan_C)
LV_FN(Into, OnFinishScanPy, 0, Into_OnFinishScan_C)
LV_FN(Into, OnBishopsDeathPy, 2, Into_OnBishopsDeath_C)
LV_FN(Into, OnBishopsDamagePy, 3, Into_OnBishopsDamage_C)
LV_FN(Into, DropMatrixPy, 1, Into_DropMatrix_C)
LV_FN(Into, ActivateMatrixPy, 1, Into_ActivateMatrix_C)
LV_FN(Into, TakeMatrixPy, 2, Into_TakeMatrix_C)
LV_FN(Into, RestoreMatrixPy, 0, Into_RestoreMatrix_C)
LV_FN(Into, LaunchRestoreMatrixAnimPy, 4, Into_LaunchRestoreMatrixAnim_C)
LV_FN(Into, RelaxBishopsPy, 0, Into_RelaxBishops_C)
LV_FN(Into, MissionScorerPy, 1, Into_MissionScorer_C)

PyMethodDef g_GDBIntoTheGDBMethods[] = {
	{ "GetBishopProfile", Into_GetBishopProfile, 1, NULL },
	{ "Init", Into_InitPy, 1, NULL },
	{ "CreateBishop", Into_CreateBishopPy, 1, NULL },
	{ "CreateBishops", Into_CreateBishopsPy, 1, NULL },
	{ "InitMission", Into_InitMissionPy, 1, NULL },
	{ "ScanShip", Into_ScanShipPy, 1, NULL },
	{ "CheckContinueScanning", Into_CheckContinueScanningPy, 1, NULL },
	{ "ResetScan", Into_ResetScanPy, 1, NULL },
	{ "OnFinishScan", Into_OnFinishScanPy, 1, NULL },
	{ "OnBishopsDeath", Into_OnBishopsDeathPy, 1, NULL },
	{ "OnBishopsDamage", Into_OnBishopsDamagePy, 1, NULL },
	{ "DropMatrix", Into_DropMatrixPy, 1, NULL },
	{ "ActivateMatrix", Into_ActivateMatrixPy, 1, NULL },
	{ "TakeMatrix", Into_TakeMatrixPy, 1, NULL },
	{ "RestoreMatrix", Into_RestoreMatrixPy, 1, NULL },
	{ "LaunchRestoreMatrixAnim", Into_LaunchRestoreMatrixAnimPy, 1, NULL },
	{ "RelaxBishops", Into_RelaxBishopsPy, 1, NULL },
	{ "MissionScorer", Into_MissionScorerPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ReinsertOwnMatrix and Mission_ReinsertOwnMatrix2: reinsert Dtritus' matrix among eight bishops ---- */

static const char *const r1_bishop_profile[] = { "Ship=SBISHOP1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=5", "UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10",
	"UsePercHook=50", "FactorHookMap=4", "CadenceShoot=1", "ControlAim=1", "AngAttack=3.1415", "CounterAttack=0.75", "ControlBoost=1",
	"ControlBoostItemLife=0", "ControlBoostItemAmmo=0", "FactorDamageGetAway=1", "FactorDamageItemLife=0", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=1", "UsePercHookGetAway=2", "TimeMaxStrafe=6", "VelMaxStrafe=0.75", NULL };

static const char *const r2_bishop_profile[] = { "Ship=SBISHOP1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=5", "UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10",
	"UsePercHook=90", "FactorHookMap=4", "CadenceShoot=1", "ControlAim=4", "AngAttack=3.1415", "CounterAttack=0.75", "ControlBoost=1",
	"ControlBoostItemLife=0", "ControlBoostItemAmmo=0", "FactorDamageGetAway=1", "FactorDamageItemLife=0", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=1", "UsePercHookGetAway=2", "TimeMaxStrafe=6", "VelMaxStrafe=0.75", NULL };

MODE_FN(Re1, GetBishopProfile, 0, LvN_Profile(r1_bishop_profile, out))
MODE_FN(Re2, GetBishopProfile, 0, LvN_Profile(r2_bishop_profile, out))

/* the functions below take the module of the mission and second: 0 for Mission_ReinsertOwnMatrix, 1 for
 * Mission_ReinsertOwnMatrix2 (the module of the refs) */

/* InitMission(): the restore point and the eight bishops (the second mission: they attack when the player boards) */
static byte Re_InitMission(const char *module, const char *use, byte second) {
	return Gd_RestorePoint(module, use) &&
		(second ? PORT_LOWN(Mission_ReinsertOwnMatrix2, CreateBishops, NULL, "i", 8L) :
			PORT_LOWN(Mission_ReinsertOwnMatrix, CreateBishops, NULL, "i", 8L)) &&
		(!second || (Mode_Import("Parking") && LvN_ModSetGlobal("Parking", "SwitchVehicleMissionCallback", module, "BishopsAttackPlayer")));
}

/* BishopsAttackPlayer(): on boarding the bishops attack the player (the first mission drops the callback only with a
 * player, the second always) */
static byte Re_BishopsAttackPlayer(const char *module, byte second) {
	nativeVal_t usr, list;
	Native_SetNone(&list);
	if (!Mode_Import("Parking") || (second && !LvN_ModNone("Parking", "SwitchVehicleMissionCallback"))) {
		return 0;
	}
	int there = Gd_UsrThere(&usr);
	if (there <= 0) {
		Native_Free(&usr);
		return there == 0;
	}
	byte ok = (second || LvN_ModNone("Parking", "SwitchVehicleMissionCallback")) && LvN_GElems(module, "bishops", &list);
	int n = ok ? Native_ForCount(&list) : 0;
	ok = ok && n >= 0;
	for (int k = 0; k < n && ok; k++) {
		nativeVal_t s, b, bn, un;
		Native_SetNone(&s);
		Native_SetNone(&b);
		Native_SetNone(&bn);
		Native_SetNone(&un);
		ok = Native_SeqItem(&list, k, &s) && Mode_EntityH(&s, &b);
		if (ok && Native_True(&b)) {
			ok = Mode_VGet(&b, "Name", &bn) && Mode_VGet(&usr, "Name", &un) &&
				PORT_CALL(SAI, SetStateVehicle, NULL, "ivv", 2L, &bn, &un);
		}
		Native_Free(&s);
		Native_Free(&b);
		Native_Free(&bn);
		Native_Free(&un);
	}
	Native_Free(&list);
	Native_Free(&usr);
	return ok;
}

/* CreateBishop(): a bishop with the player there, in the enemies (red in the second mission) */
static byte Re_CreateBishop(const char *module, byte second) {
	nativeVal_t usr, bishop;
	if (!Mode_Import("VehiclesNPC")) {
		return 0;
	}
	int there = Gd_UsrThere(&usr);
	Native_Free(&usr);
	if (there <= 0) {
		return there == 0;
	}
	Native_SetNone(&bishop);
	byte ok = Gd_MakeBishop(1 + second, &bishop) && (!second || LvN_SetS(&bishop, "TgType", "red")) &&
		LvN_SetS(&bishop, "EntityList", "Enemies") && LvN_Life(&bishop, module, "BISHOP_LIFE") && Gd_AppendName(module, &bishop);
	Native_Free(&bishop);
	return ok;
}

/* CreateBishops(numBishops) */
static byte Re_CreateBishops(byte second, const nativeVal_t *num) {
	nativeVal_t one, n1;
	long count = 0;
	Native_SetInt(&one, 1);
	Native_SetNone(&n1);
	byte ok = Mode_Import("VehiclesNPC") && Native_Arith('+', num, &one, &n1) && Gd_Range1(&n1, &count);
	Native_Free(&n1);
	for (long k = 0; k < count && ok; k++) {
		ok = second ? PORT_LOWN(Mission_ReinsertOwnMatrix2, CreateBishop, NULL, "") :
			PORT_LOWN(Mission_ReinsertOwnMatrix, CreateBishop, NULL, "");
	}
	return ok;
}

/* OnBishopsDeath(LooserName, WinnerName): the last bishop dead: a life and the bishops as the police */
static byte Re_OnBishopsDeath(const char *module, const nativeVal_t *looser) {
	return Mode_Import("Vehicles") && Mode_Remove(module, "bishops", looser) && Gd_IfLast(module);
}

/* PreSetNextMission(UsrName, CharName, optId, FullOption): Dtritus gets his matrix back, the bishops go, the scene of
 * the matrix plays (the first mission: Deep Throat's message now; the second: no more Bertos, the message after the
 * scene) */
static byte Re_PreSetNextMission(const char *module, byte second) {
	nativeVal_t chars, dt;
	int rm = LvN_GTrue(module, "rm");
	if (rm < 0 || (rm && !Gd_RestoreUsed(module)) || !LvN_SaveList("Conversor.AvailableChars", module, &chars)) {
		return 0;
	}
	LvN_Str(&dt, "Dtritus");
	byte ok = LvN_SaveAppend("Conversor.AvailableChars", &chars, &dt);
	Native_Free(&chars);
	Native_Free(&dt);
	if (ok && second) {
		SaveVar_Set("CreateBertos", "0");
	} else if (ok) {
		/* OBJLIB 6: SetNextMission is a function of the data (data/levels/gdb/mission_reinsertownmatrix.def) */
		ok = LvN_Com("Mission_ReInsertOwnMatrix_RemoteMessage1", "DeepThroat", module, "SetNextMission", NULL);
	}
	if (!ok || !Gd_ClearList("Enemies") || !Gd_ClearList("OutCop") || !Gd_Alarm(0)) {
		return 0;
	}
	if (!Mode_Import("Map") || !PORT_LCALL(Map, InitPoliceBishops, NULL, "") || !Mode_Import("MapMatrix")) {
		return 0;
	}
	/* OBJLIB 6: MapMatrix.RestoreMatrix is the scene of the data (data/levels/gdb/mapmatrix.def); ShowDTMessage a
	 * function of mission_reinsertownmatrix2.def, passed as the object (OBJLIB 5) */
	if (!second) {
		return LV_CALL("MapMatrix", "RestoreMatrix", NULL, "");
	}
	nativeVal_t fn;
	ok = Mode_RefG(module, "ShowDTMessage", &fn) && LV_CALL("MapMatrix", "RestoreMatrix", NULL, "v", &fn);
	Native_Free(&fn);
	return ok;
}

/* the entries of the two missions: the same functions with the module and the differences (second) */
#define RE_FNS(n, mod, use, second) \
	static byte Re##n##_InitMission_C(const nativeVal_t *a, nativeVal_t *out) { (void)a; (void)out; return Re_InitMission(mod, use, second); } \
	static byte Re##n##_BishopsAttackPlayer_C(const nativeVal_t *a, nativeVal_t *out) { (void)a; (void)out; return Re_BishopsAttackPlayer(mod, second); } \
	static byte Re##n##_CreateBishop_C(const nativeVal_t *a, nativeVal_t *out) { (void)a; (void)out; return Re_CreateBishop(mod, second); } \
	static byte Re##n##_CreateBishops_C(const nativeVal_t *a, nativeVal_t *out) { (void)out; return Re_CreateBishops(second, &a[0]); } \
	static byte Re##n##_OnBishopsDeath_C(const nativeVal_t *a, nativeVal_t *out) { (void)out; return Re_OnBishopsDeath(mod, &a[0]); } \
	static byte Re##n##_PreSetNextMission_C(const nativeVal_t *a, nativeVal_t *out) { (void)a; (void)out; return Re_PreSetNextMission(mod, second); } \
	LV_FN(Re##n, InitMissionPy, 0, Re##n##_InitMission_C) \
	LV_FN(Re##n, BishopsAttackPlayerPy, 0, Re##n##_BishopsAttackPlayer_C) \
	LV_FN(Re##n, CreateBishopPy, 0, Re##n##_CreateBishop_C) \
	LV_FN(Re##n, CreateBishopsPy, 1, Re##n##_CreateBishops_C) \
	LV_FN(Re##n, OnBishopsDeathPy, 2, Re##n##_OnBishopsDeath_C) \
	LV_FN(Re##n, PreSetNextMissionPy, 4, Re##n##_PreSetNextMission_C)

RE_FNS(1, R1, "Mission_ReinsertOwnMatrix.PreSetNextMission", 0)
RE_FNS(2, R2, "Mission_ReinsertOwnMatrix2.PreSetNextMission", 1)

PyMethodDef g_GDBReinsertOwnMatrixMethods[] = {
	{ "GetBishopProfile", Re1_GetBishopProfile, 1, NULL },
	{ "InitMission", Re1_InitMissionPy, 1, NULL },
	{ "BishopsAttackPlayer", Re1_BishopsAttackPlayerPy, 1, NULL },
	{ "CreateBishop", Re1_CreateBishopPy, 1, NULL },
	{ "CreateBishops", Re1_CreateBishopsPy, 1, NULL },
	{ "OnBishopsDeath", Re1_OnBishopsDeathPy, 1, NULL },
	{ "PreSetNextMission", Re1_PreSetNextMissionPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_GDBReinsertOwnMatrix2Methods[] = {
	{ "GetBishopProfile", Re2_GetBishopProfile, 1, NULL },
	{ "InitMission", Re2_InitMissionPy, 1, NULL },
	{ "BishopsAttackPlayer", Re2_BishopsAttackPlayerPy, 1, NULL },
	{ "CreateBishop", Re2_CreateBishopPy, 1, NULL },
	{ "CreateBishops", Re2_CreateBishopsPy, 1, NULL },
	{ "OnBishopsDeath", Re2_OnBishopsDeathPy, 1, NULL },
	{ "PreSetNextMission", Re2_PreSetNextMissionPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_GDB)
