/*
 * The scripts of the level GDB in C (levels/gdb/scripts of data03.packed, issue #2): the Map module (the bishops as the
 * police, the waves and the lives they give), MapMatrix (the end of the scene of the matrix) and the missions of the
 * level (Mission_IntoTheGDB, Mission_ReinsertOwnMatrix, Mission_ReinsertOwnMatrix2). MapSnd, the scene of MapMatrix and
 * the plain functions of the missions are data (data/levels/gdb/). The module variables (the bishops, the traitor,
 * the scan state, constants, models) and the module code (Map.Libraries, the save variables) stay in the scripts; the
 * functions read and write them as their globals.
 */
#include "Levels.h"

#define GM "Map"
#define MX "MapMatrix"
#define IG "Mission_IntoTheGDB"
#define R1 "Mission_ReinsertOwnMatrix"
#define R2 "Mission_ReinsertOwnMatrix2"

#define USR() Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0)
#define LANG(k) Sf_Lang(&f, Sf_Str(&f, k))
#define SAVE(n) Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", n)
#define G(m, n) Sf_Global(&f, m, n)
#define NOARGS Sf_Build(&f, "()")

/* the scan bar of IntoTheGDB blinks (lost: Blink 1, IsAutoUpdatable 0) or runs */
static byte Gd_Blink(scriptFrame_t *f, int lost) {
	return Lv_Set0(f, "ScanRemainingTime", "Blink", Sf_Int(f, lost)) && Lv_Set0(f, "ScanRemainingTime", "IsAutoUpdatable", Sf_Int(f, !lost));
}

/* SVec.ModSqr(SVec.Sub(e.Pos, usr.Pos)) < (or >) MAX_SCANNING_DIST * MAX_SCANNING_DIST of IntoTheGDB */
static int Gd_Scan(scriptFrame_t *f, PyObject *e, PyObject *usr, const char *op) {
	int mark = f->n;
	PyObject *ep = Sf_Attr(f, e, "Pos");
	PyObject *d = Sf_Ask(f, "SVec", "ModSqr", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", ep, Sf_Attr(f, usr, "Pos")));
	PyObject *m = Sf_Global(f, IG, "MAX_SCANNING_DIST");
	int c = Sf_Compare(f, d, op, Sf_Op(f, m, '*', Sf_Global(f, IG, "MAX_SCANNING_DIST")));
	Sf_Release(f, mark);
	return c;
}

/* e = Scrap.GetEntity(Scrap.EntityListGet(list, "First")); while e: e.EntityList = ""; e.Del(); e = the next first */
static byte Gd_ClearList(scriptFrame_t *f, const char *list) {
	int mark = f->n;
	PyObject *e = Sf_Entity(f, Sf_Ask(f, "Scrap", "EntityListGet", "(ss)", list, "First"));
	while (Sf_True(f, e) > 0) {
		Sf_SetAttrS(f, e, "EntityList", "");
		Sf_Method(f, e, "Del", "()");
		PyObject *next = Sf_Entity(f, Sf_Ask(f, "Scrap", "EntityListGet", "(ss)", list, "First"));
		Sf_ReleaseKeep(f, mark, &next, 1);
		e = next;
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* the last bishop dead: import Map; Map.GiveLife(); Map.InitPoliceBishops(); Scrap.SetAlarm(1) */
static byte Gd_LastBishop(scriptFrame_t *f) {
	Sf_Import(f, "Map");
	Sf_Run(f, "Map", "GiveLife", "()");
	Sf_Run(f, "Map", "InitPoliceBishops", "()");
	return Sf_Run(f, "Scrap", "SetAlarm", "(i)", 1);
}

/* rm: the temporary entity at the dummy DM_Mission_RestoreMatrix where the matrix is reinserted (used with use), the
 * target; the global rm of module */
static byte Gd_RestorePoint(scriptFrame_t *f, const char *module, const char *use) {
	int mark = f->n;
	Sf_Run(f, "Scrap", "StartDummySearch", "(si)", "DM_Mission_RestoreMatrix", 0);
	PyObject *dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
	PyObject *fn = Sf_Mod(f, "Scrap", "CreateEntity");
	PyObject *p = Sf_Item(f, dummy, 1);
	PyObject *x = Sf_Item(f, p, 0), *y = Sf_Item(f, p, 1), *z = Sf_Item(f, p, 2);
	Sf_SetGlobal(f, module, "rm", Sf_Call(f, fn, "(sOOOs)", "DM_Mission_RestoreMatrix", x, y, z, "WithLife"));
	PyObject *rm = Sf_Global(f, module, "rm");
	PyObject *ay = Sf_OpI(f, Sf_Item(f, Sf_Item(f, dummy, 2), 1), '+', 180);
	Sf_SetAttr(f, rm, "Ang", Sf_Build(f, "(iOi)", 0, ay, 0));
	Sf_SetAttrI(f, rm, "VRadius", 60);
	Sf_SetAttrI(f, rm, "ArrowCanUse", 1);
	Sf_SetAttrI(f, rm, "Life", 0);
	Sf_SetAttrI(f, rm, "ArrnOpt", 0);
	Sf_SetAttrS(f, rm, "ArrOpt0", "RestoreMatrix:Restore Matrix");
	Sf_SetAttrS(f, rm, "UseFunc", use);
	Sf_SetAttrI(f, rm, "ActiveCMask", 0);
	Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(f, rm, "Name"));
	Sf_Release(f, mark);
	return !f->failed;
}

/* the restore point used: no target, no use, off (the global rm of module) */
static byte Gd_RestoreUsed(scriptFrame_t *f, const char *module) {
	int mark = f->n;
	Sf_Run(f, "MissionsFuncs", "ActivateTarget", "(Oi)", Sf_Global(f, module, "rm"), 0);
	Sf_SetAttrS(f, Sf_Global(f, module, "rm"), "UseFunc", "");
	Sf_SetAttrI(f, Sf_Global(f, module, "rm"), "Active", 0);
	Sf_Release(f, mark);
	return !f->failed;
}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors without police and traffic, the player is the bishop, the teleporter to the temple */
static PyObject *GDB_InitStatus(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "OutPolice", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 1));
	Sf_SetMod(&f, "OutPolice", "PoliceActive", Sf_Int(&f, 0));
	Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Bishop");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "TempleTeleporter", "DM_Door_TempleTeleporter_Temple");
	return Sf_None(&f);
}

/* Init(LevelPath): the clouds; in single player the teleporter to the temple (the arrival places the player) and the
 * bishops as the police */
static PyObject *GDB_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Init");
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudRed", 191);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudGreen", 123);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudBlue", 184);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "InitMercs");
	if (Sf_True(&f, Sf_Mod(&f, "Init", "MultiPlayer")) == 0) {
		Sf_Import(&f, "OutPolice");
		Sf_Import(&f, "Elevator");
		Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_TempleTeleporter_Temple", "Temple");
		if (Sf_EqS(&f, SAVE("ComeFrom"), "DoorTempleTeleporter") > 0) {
			Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", G(GM, "EndOfAppears"));
		}
		if (Sf_True(&f, G(GM, "CreateBishops")) > 0) {
			Sf_Own(&f, GM, "InitPoliceBishops", "()");
		}
	}
	return Sf_None(&f);
}

/* InitPoliceBishops(): the alarm meter, the four waves of bishops and archbishops, the first one */
static PyObject *GDB_InitPoliceBishops(PyObject *self, PyObject *args) {
	static const int waves[4] = { 3, 2, 1, 0 };
	SF_ARGS("");
	Sf_Import(&f, "OutPolice");
	Sf_Run(&f, "Scrap", "Set", "(sd)", "AlarmFallDelta", -0.05);
	Sf_Run(&f, "Scrap", "Set", "(sd)", "AlarmGrowDelta", 0.1);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "PoliceTauntRace", "Bishop");
	for (int i = 0; i < 4 && !f.failed; i++) {
		int mark = f.n;
		PyObject *wave = Sf_Build(&f, "[]");
		for (int k = 0; k < 6; k++) {
			Sf_Method(&f, wave, "append", "(s)", k < waves[i] ? "SBishop1" : "SArchbishop1");
		}
		Sf_Method(&f, Sf_Mod(&f, "OutPolice", "PoliceWaves"), "append", "(O)", wave);
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "OutPolice", "InitShips", "()");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "PoliceWaveEliminated", "Map.NextWave");
	return Sf_None(&f);
}

/* EndOfAppears(elv): the arrival by the teleporter: the player at the spawn point, the message of the waves (three
 * times) */
static PyObject *GDB_EndOfAppears(PyObject *self, PyObject *args) {
	PyObject *elv;
	SF_ARGS("O", &elv);
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "RaceMaker");
	Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", Py_None);
	PyObject *dummy = Lv_Dummy(&f, "DM_Player_Spawn_Temple", 0);
	Sf_Print(&f, "O", dummy);
	PyObject *v = USR();
	Sf_SetAttr(&f, v, "Pos", Sf_Item(&f, dummy, 1));
	Sf_Run(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, v, "Name"));
	PyObject *doit = Sf_ToInt(&f, SAVE("GDB.BishopsMsg"));
	if (Sf_True(&f, doit) > 0) {
		Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Op(&f, LANG("OnBishopsWave3"), '%', Sf_Int(&f, 5)));
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "GDB.BishopsMsg", Sf_Repr(&f, Sf_OpI(&f, doit, '-', 1)));
	}
	return Sf_None(&f);
}

/* GiveLife(): a wave beaten with the alarm on: up to five lives, at most MaxPlayerLives */
static PyObject *GDB_GiveLife(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	PyObject *lives = Sf_ToInt(&f, SAVE("Player.NumLives"));
	int more = Sf_Compare(&f, lives, "<", Sf_Ask(&f, "Scrap", "Get", "(s)", "MaxPlayerLives"));
	if (more > 0) {
		PyObject *incl = NULL;
		int top = Sf_Compare(&f, Sf_OpI(&f, lives, '+', 5), ">", Sf_Ask(&f, "Scrap", "Get", "(s)", "MaxPlayerLives"));
		if (top > 0) {
			incl = Sf_Op(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "MaxPlayerLives"), '-', lives);
		} else if (top == 0) {
			incl = Sf_Int(&f, 5);
		}
		Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Op(&f, LANG("OnBishopsWave"), '%', incl));
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Player.NumLives", Sf_Repr(&f, Sf_Op(&f, lives, '+', incl)));
	} else if (more == 0) {
		Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", LANG("OnBishopsWave2"));
	}
	Sf_Run(&f, "Scrap", "IncSaveVar", "(s)", "Stats.GDB");
	return Sf_None(&f);
}

/* NextWave(): the engine's callback of a wave eliminated: lives while the alarm is on, the next wave */
static PyObject *GDB_NextWave(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "OutPolice");
	if (Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0)) > 0) {
		Sf_Own(&f, GM, "GiveLife", "()");
	}
	Sf_Run(&f, "OutPolice", "NextWave", "(i)", 0);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "PoliceWaveEliminated", "Map.NextWave");
	return Sf_None(&f);
}

PyMethodDef g_GDBMapMethods[] = {
	{ "InitStatus", GDB_InitStatus, 1, NULL },
	{ "Init", GDB_Init, 1, NULL },
	{ "InitPoliceBishops", GDB_InitPoliceBishops, 1, NULL },
	{ "EndOfAppears", GDB_EndOfAppears, 1, NULL },
	{ "GiveLife", GDB_GiveLife, 1, NULL },
	{ "NextWave", GDB_NextWave, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapMatrix ---- */

/* EndOFScene(usr = None): the scene of the matrix ends: the fade, then the function RestoreMatrix was given */
static PyObject *Matrix_EndOFScene(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "usr", NULL };
	PyObject *usr = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", kwlist, &usr)) {
		return NULL;
	}
	Lv_Fade(&f, lv_black, lv_clear, 1.0);
	PyObject *fn = G(MX, "EndFunc");
	if (Sf_True(&f, fn) > 0) {
		Sf_Call(&f, fn, "()");
	}
	return Sf_None(&f);
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

static PyObject *Into_GetBishopProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, IG, ig_bishop_profile);
}

/* Init(LevelPath): the lists of the bishops, the eight bishops (identified when the matrix is already taken), the
 * restore point or the scan when the player boards */
static PyObject *Into_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(IG, "MatrixModel"));
	Sf_Run(&f, "Scrap", "CreateEntityList", "(s)", "Bishops");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Bishops", "OnDeath", "Mission_IntoTheGDB.OnBishopsDeath");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Bishops", "OnDamage", "Mission_IntoTheGDB.OnBishopsDamage");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_IntoTheGDB.OnBishopsDeath");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDamage", "Mission_IntoTheGDB.OnBishopsDamage");
	PyObject *state = SAVE("MissionState");
	int restore = Sf_EqS(&f, state, "RestoreMatrix");
	Sf_Own(&f, IG, "CreateBishops", "(iO)", 8, restore < 0 ? NULL : Sf_Int(&f, restore));
	restore = Sf_EqS(&f, state, "RestoreMatrix");
	if (restore > 0) {
		Sf_Own(&f, IG, "RestoreMatrix", "()");
	} else if (restore == 0) {
		Sf_Run(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_IntoTheGDB_RemoteMessage1", "DeepThroat");
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(IG, "InitMission"));
		Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(IG, "StopMission"));
	}
	return Sf_None(&f);
}

/* CreateBishop(bishopIdentified): a bishop with the player there: unidentified ones wait in the list Bishops as
 * targets, identified ones are enemies */
static PyObject *Into_CreateBishop(PyObject *self, PyObject *args) {
	PyObject *ident;
	SF_ARGS("O", &ident);
	Sf_Import(&f, "VehiclesNPC");
	if (Sf_True(&f, USR()) != 1) {
		return Sf_None(&f);
	}
	PyObject *bishop = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ss)", "SBishop1", "Bishop");
	PyObject *pd = Sf_Own(&f, IG, "GetBishopProfile", "()");
	Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", bishop, pd);
	Lv_Life(&f, bishop, IG, "BISHOP_LIFE");
	int t = Sf_True(&f, ident);
	if (t == 0) {
		Sf_SetAttrS(&f, bishop, "EntityList", "Bishops");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", bishop, 1);
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, bishop, "Name"), 0);
	} else if (t > 0) {
		Sf_SetAttrS(&f, bishop, "EntityList", "Enemies");
	}
	return Sf_Return(&f, bishop);
}

/* CreateBishops(numBishops, bishopIdentified): the bishops, one of them the traitor (who drops nothing) */
static PyObject *Into_CreateBishops(PyObject *self, PyObject *args) {
	PyObject *num, *ident;
	SF_ARGS("OO", &num, &ident);
	PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(iO)", 1, Sf_OpI(&f, num, '+', 1));
	int mark = f.n;
	for (int k = 0; Sf_Next(&f, range, k) != NULL; k++) {
		PyObject *bishop = Sf_Own(&f, IG, "CreateBishop", "(O)", ident);
		Sf_Method(&f, G(IG, "bishops"), "append", "(O)", Sf_Attr(&f, bishop, "Name"));
		Sf_Release(&f, mark);
	}
	if (Sf_True(&f, ident) == 0) {
		PyObject *hi = Sf_Op(&f, Sf_ToFloat(&f, num), '-', Sf_Float(&f, 0.05));
		PyObject *i = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "Rand", "(dO)", 0.0, hi));
		Sf_SetGlobal(&f, IG, "traitor", Sf_GetItem(&f, G(IG, "bishops"), i));
		PyObject *e = Sf_Entity(&f, G(IG, "traitor"));
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrI(&f, e, "Money", -1);
		}
	}
	return Sf_None(&f);
}

/* InitMission(): the bishops as targets, the scorer of the scan, the scan every second */
static PyObject *Into_InitMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", G(IG, "bishops"));
	Sf_SetMod(&f, "Menu", "MissionScorerCall", G(IG, "MissionScorer"));
	Sf_DrawScorer(&f);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnFinishAutoUpdate", "Mission_IntoTheGDB.OnFinishScan");
	Sf_Later(&f, Sf_Float(&f, 1.0), G(IG, "ScanShip"), NOARGS, "ScanShip");
	return Sf_None(&f);
}

/* ScanShip(): every second: the bishop being scanned stays near, else the next bishop near the player is scanned */
static PyObject *Into_ScanShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	Sf_Later(&f, Sf_Float(&f, 1.0), G(IG, "ScanShip"), NOARGS, "ScanShip");
	PyObject *e = Sf_Entity(&f, G(IG, "currentScannedShip"));
	int t = Sf_True(&f, e);
	if (t > 0) {
		int far = Gd_Scan(&f, e, usr, ">");
		if (far > 0) {
			if (Sf_EqI(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "ScanRemainingTime", "Blink"), 0) > 0) {
				Gd_Blink(&f, 1);
				Sf_Later(&f, G(IG, "MAX_TIME_TARGET_LOST"), G(IG, "CheckContinueScanning"), NOARGS, NULL);
			}
		} else if (far == 0) {
			Gd_Blink(&f, 0);
		}
		return Sf_None(&f);
	}
	PyObject *s;
	int mark = f.n;
	for (int k = 0; (s = Sf_Next(&f, G(IG, "bishops"), k)) != NULL; k++) {
		e = Sf_Entity(&f, s);
		if (Sf_True(&f, e) > 0 && Gd_Scan(&f, e, usr, "<") > 0) {
			Sf_SetGlobal(&f, IG, "currentScannedShip", Sf_Attr(&f, e, "Name"));
			Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", G(IG, "currentScannedShip"));
			Lv_Set0(&f, "ScanStatusText", "Visible", Sf_Int(&f, 1));
			Gd_Blink(&f, 0);
			Lv_Set0(&f, "ScanRemainingTime", "MaxValue", G(IG, "MAX_SCANNING_TIME"));
			Lv_Set0(&f, "ScanRemainingTime", "Value", G(IG, "MAX_SCANNING_TIME"));
			Lv_Set0(&f, "ScanRemainingTime", "Visible", Sf_Int(&f, 1));
			break;
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* CheckContinueScanning(): the bishop came back near in time, or the scan starts again */
static PyObject *Into_CheckContinueScanning(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	PyObject *e = Sf_Entity(&f, G(IG, "currentScannedShip"));
	int both = Sf_True(&f, usr);
	if (both > 0) {
		both = Sf_True(&f, e);
	}
	if (both > 0 && Gd_Scan(&f, e, usr, "<") > 0) {
		Gd_Blink(&f, 0);
		return Sf_None(&f);
	}
	Sf_Own(&f, IG, "ResetScan", "()");
	return Sf_None(&f);
}

/* ResetScan(): no bishop scanned, the scorer hidden (refilled a moment later) */
static PyObject *Into_ResetScan(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, IG, "currentScannedShip", Sf_Str(&f, ""));
	if (Sf_True(&f, G(IG, "traitorFound")) == 0) {
		Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
	}
	Lv_Set0(&f, "ScanRemainingTime", "Visible", Sf_Int(&f, 0));
	Lv_Set0(&f, "ScanStatusText", "Visible", Sf_Int(&f, 0));
	Sf_Later(&f, Sf_Float(&f, 0.25), G(IG, "ResetScan2"), NOARGS, NULL);
	return Sf_None(&f);
}

/* OnFinishScan(): the scan bar ran out: the traitor found (the others stop being targets), or one bishop fewer */
static PyObject *Into_OnFinishScan(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_Set0(&f, "ScanStatusText", "Visible", Sf_Int(&f, 0));
	Lv_Set0(&f, "ScanRemainingTime", "Visible", Sf_Int(&f, 0));
	if (Sf_True(&f, USR()) != 1) {
		return Sf_None(&f);
	}
	int found = Sf_Compare(&f, G(IG, "currentScannedShip"), "==", G(IG, "traitor"));
	if (found > 0) {
		Sf_SetGlobal(&f, IG, "traitorFound", Sf_Int(&f, 1));
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ScanShip");
		Sf_Run(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_IntoTheGDB_RemoteMessage4", "DeepThroat");
		PyObject *s;
		int mark = f.n;
		for (int k = 0; (s = Sf_Next(&f, G(IG, "bishops"), k)) != NULL; k++) {
			PyObject *e = Sf_Entity(&f, s);
			if (Sf_True(&f, e) > 0 && Sf_Compare(&f, Sf_Attr(&f, e, "Name"), "!=", G(IG, "traitor")) > 0) {
				Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
			}
			Sf_Release(&f, mark);
		}
	} else if (found == 0) {
		Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
		Sf_Run(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_IntoTheGDB_RemoteMessage3", "DeepThroat");
		if (Sf_In(&f, G(IG, "currentScannedShip"), G(IG, "bishops")) > 0) {
			Sf_Method(&f, G(IG, "bishops"), "remove", "(O)", G(IG, "currentScannedShip"));
		}
		PyObject *e = Sf_Entity(&f, G(IG, "currentScannedShip"));
		if (Sf_True(&f, e) > 0) {
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
		}
	}
	Sf_Own(&f, IG, "ResetScan", "()");
	return Sf_None(&f);
}

/* OnBishopsDeath(LooserName, WinnerName): the player killed the traitor: the matrix drops; the last bishop dead: a
 * life and the bishops as the police */
static PyObject *Into_OnBishopsDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "Map");
	if (Sf_In(&f, looser, G(IG, "bishops")) > 0) {
		Sf_Method(&f, G(IG, "bishops"), "remove", "(O)", looser);
	}
	PyObject *usr = USR();
	int t = Sf_True(&f, usr);
	if (t > 0) {
		t = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", winner);
	}
	if (t > 0) {
		t = Sf_Compare(&f, looser, "==", G(IG, "traitor"));
	}
	if (t > 0) {
		PyObject *e = Sf_Entity(&f, looser);
		if (Sf_True(&f, e) > 0) {
			Sf_Print(&f, "s", "Grabe ahora!");
			Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
			Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ScanShip");
			Sf_SetGlobal(&f, IG, "traitor", Sf_Str(&f, ""));
			Sf_Own(&f, IG, "DropMatrix", "(O)", Sf_Attr(&f, e, "Pos"));
		}
	}
	if (Sf_Compare(&f, G(IG, "currentScannedShip"), "==", looser) > 0) {
		Sf_Own(&f, IG, "ResetScan", "()");
	}
	if (Sf_Len(&f, G(IG, "bishops")) == 0) {
		Gd_LastBishop(&f);
	}
	return Sf_None(&f);
}

/* OnBishopsDamage(LooserName, WinnerName, Damage): a bishop shot: all of them become enemies */
static PyObject *Into_OnBishopsDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Run(&f, "Speech", "AddRemoteCom", "(ss)", "Mission_IntoTheGDB_RemoteMessage6", "Bishop");
	PyObject *list = Sf_Build(&f, "[]");
	PyObject *e = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Bishops", "First"));
	int mark = f.n;
	while (Sf_True(&f, e) > 0) {
		Sf_Method(&f, list, "append", "(O)", Sf_Attr(&f, e, "Name"));
		PyObject *next = Sf_Entity(&f, Sf_Attr(&f, e, "NextInList"));
		Sf_ReleaseKeep(&f, mark, &next, 1);
		e = next;
	}
	PyObject *name;
	mark = f.n;
	for (int k = 0; (name = Sf_Next(&f, list, k)) != NULL; k++) {
		e = Sf_Entity(&f, name);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrS(&f, e, "EntityList", "Enemies");
		}
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Bishops", "OnDamage", "");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDamage", "");
	return Sf_None(&f);
}

/* DropMatrix(pos): Gus' matrix (not takeable yet) and its arrow where the traitor died; Deep Throat's message makes it
 * takeable */
static PyObject *Into_DropMatrix(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	PyObject *fn = Sf_Mod(&f, "Scrap", "CreateEntity");
	PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
	PyObject *l = Sf_Call(&f, fn, "(sOOOs)", "GusMatrix", x, y, z, "Item");
	Sf_Method(&f, l, "SetModel", "(O)", G(IG, "MatrixModel"));
	Sf_SetAttrI(&f, l, "Radius", 3500);
	Sf_SetAttrI(&f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(&f, l, "AutoFace", 0);
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
	Sf_SetAttrS(&f, l, "OnTake", "");
	Sf_Import(&f, "Items");
	Sf_Run(&f, "Items", "SetSound", "(Os)", l, "TakeMatrixExt");
	fn = Sf_Mod(&f, "Scrap", "CreateEntity");
	x = Sf_Item(&f, pos, 0), y = Sf_Item(&f, pos, 1), z = Sf_Item(&f, pos, 2);
	PyObject *l2 = Sf_Call(&f, fn, "(sOOOs)", "MissionCheckArrow", x, y, z, "WithLife");
	Sf_SetAttr(&f, l2, "CamTgt", Sf_Build(&f, "(iii)", 0, 1000, 0));
	PyObject *s;
	int mark = f.n;
	for (int k = 0; (s = Sf_Next(&f, G(IG, "bishops"), k)) != NULL; k++) {
		PyObject *e = Sf_Entity(&f, s);
		if (Sf_True(&f, e) > 0) {
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
		}
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, l2, "Name"));
	Sf_SetMod(&f, "Speech", "RemoteCom", Sf_Build(&f, "[]"));
	PyObject *kw = Sf_Build(&f, "{s:O,s:(O)}", "AfterRemoteComFunc", G(IG, "ReinsertMatrix"), "Parameters", Sf_Attr(&f, l, "Name"));
	Sf_CallKw(&f, Sf_Mod(&f, "Speech", "ForceRemoteCom"), Py_BuildValue("(ss)", "Mission_IntoTheGDB_RemoteMessage5", "DeepThroat"), kw);
	return Sf_None(&f);
}

/* ActivateMatrix(name): the matrix can be taken */
static PyObject *Into_ActivateMatrix(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "OnTake", "Mission_IntoTheGDB.TakeMatrix");
	}
	return Sf_None(&f);
}

/* TakeMatrix(ItemName, CharName): the player took the matrix: to the restore point */
static PyObject *Into_TakeMatrix(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	if (Lv_UsrNameIs(&f, USR(), ch) <= 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sf_Ask(&f, "Scrap", "GetEntity", "(s)", "GusMatrix");
	if (Sf_True(&f, e) > 0) {
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
		Sf_SetAttrI(&f, e, "rTime", 20);
		Sf_Later(&f, Sf_Int(&f, 5), Sf_Attr(&f, e, "Del"), NOARGS, NULL);
	}
	e = Sf_Ask(&f, "Scrap", "GetEntity", "(s)", "MissionCheckArrow");
	if (Sf_True(&f, e) > 0) {
		Sf_Method(&f, e, "Del", "()");
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "RestoreMatrix");
	Sf_Own(&f, IG, "RestoreMatrix", "()");
	return Sf_None(&f);
}

static PyObject *Into_RestoreMatrix(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Gd_RestorePoint(&f, IG, "Mission_IntoTheGDB.LaunchRestoreMatrixAnim");
	return Sf_None(&f);
}

/* LaunchRestoreMatrixAnim(UsrName, CharName, optId, FullOption): the bishops go, the scene of the matrix plays */
static PyObject *Into_LaunchRestoreMatrixAnim(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Gd_RestoreUsed(&f, IG);
	Sf_Own(&f, IG, "RelaxBishops", "()");
	Sf_Own(&f, IG, "PreSetNextMission", "()");
	Sf_Import(&f, "MapMatrix");
	Sf_Run(&f, "MapMatrix", "RestoreMatrix", "()");
	return Sf_None(&f);
}

/* RelaxBishops(): the police, the bishops and the enemies go, no alarm, the bishops as the police again */
static PyObject *Into_RelaxBishops(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "OutPolice");
	Sf_Import(&f, "Map");
	Gd_ClearList(&f, "OutCop");
	Gd_ClearList(&f, "Bishops");
	Gd_ClearList(&f, "Enemies");
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Run(&f, "Map", "InitPoliceBishops", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the scan status and its bar, visible while a bishop near the player is scanned */
static PyObject *Into_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	PyObject *e = Sf_Entity(&f, G(IG, "currentScannedShip"));
	PyObject *usr = USR();
	int scanning = Sf_True(&f, usr);
	if (scanning > 0) {
		scanning = Sf_True(&f, e);
	}
	if (scanning > 0) {
		scanning = Gd_Scan(&f, e, usr, "<");
	}
	PyObject *vis = Sf_Int(&f, scanning > 0);
	PyObject *x = Sf_Mod(&f, "Scorer", "MissionScorerText_X"), *y = Sf_Mod(&f, "Scorer", "MissionScorerText_Y");
	Sf_MissionText(&f, id, "ScanStatusText", "Text", x, y, "Horatio", "Shadow", Sf_Cat(&f, "Os", LANG("Mission_IntoTheGDB_Scorer1"), ":"),
		"Left", vis);
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "ScanRemainingTime", "Bar");
	PyObject *fn = Sf_Mod(&f, "Scorer", "MissionScorerBar_SetDefaults");
	PyObject *back = Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorBack");
	PyObject *main = Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorMain");
	PyObject *black = Sf_Mod(&f, "Scorer", "MissionScorerBar_ColorBlack");
	Sf_Call(&f, fn, "(OsOOO)", id, "ScanRemainingTime", back, main, black);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "ScanRemainingTime", "IsAutoUpdatable", 1);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "MaxValue", G(IG, "MAX_SCANNING_TIME"));
	if (scanning == 0) {
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "Value", G(IG, "MAX_SCANNING_TIME"));
	}
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "Visible", vis);
	return Sf_None(&f);
}

PyMethodDef g_GDBIntoTheGDBMethods[] = {
	{ "GetBishopProfile", Into_GetBishopProfile, 1, NULL },
	{ "Init", Into_Init, 1, NULL },
	{ "CreateBishop", Into_CreateBishop, 1, NULL },
	{ "CreateBishops", Into_CreateBishops, 1, NULL },
	{ "InitMission", Into_InitMission, 1, NULL },
	{ "ScanShip", Into_ScanShip, 1, NULL },
	{ "CheckContinueScanning", Into_CheckContinueScanning, 1, NULL },
	{ "ResetScan", Into_ResetScan, 1, NULL },
	{ "OnFinishScan", Into_OnFinishScan, 1, NULL },
	{ "OnBishopsDeath", Into_OnBishopsDeath, 1, NULL },
	{ "OnBishopsDamage", Into_OnBishopsDamage, 1, NULL },
	{ "DropMatrix", Into_DropMatrix, 1, NULL },
	{ "ActivateMatrix", Into_ActivateMatrix, 1, NULL },
	{ "TakeMatrix", Into_TakeMatrix, 1, NULL },
	{ "RestoreMatrix", Into_RestoreMatrix, 1, NULL },
	{ "LaunchRestoreMatrixAnim", Into_LaunchRestoreMatrixAnim, 1, NULL },
	{ "RelaxBishops", Into_RelaxBishops, 1, NULL },
	{ "MissionScorer", Into_MissionScorer, 1, NULL },
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

static PyObject *Re1_GetBishopProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, R1, r1_bishop_profile);
}

static PyObject *Re2_GetBishopProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, R2, r2_bishop_profile);
}

/* InitMission(): the restore point and the eight bishops (the second mission: they attack when the player boards) */
static PyObject *Re_InitMission(PyObject *args, const char *module, const char *use, byte second) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Gd_RestorePoint(&f, module, use);
	Sf_Own(&f, module, "CreateBishops", "(i)", 8);
	if (second) {
		Sf_Import(&f, "Parking");
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(module, "BishopsAttackPlayer"));
	}
	return Sf_None(&f);
}

static PyObject *Re1_InitMission(PyObject *self, PyObject *args) {
	(void)self;
	return Re_InitMission(args, R1, "Mission_ReinsertOwnMatrix.PreSetNextMission", 0);
}

static PyObject *Re2_InitMission(PyObject *self, PyObject *args) {
	(void)self;
	return Re_InitMission(args, R2, "Mission_ReinsertOwnMatrix2.PreSetNextMission", 1);
}

/* BishopsAttackPlayer(): on boarding the bishops attack the player (the first mission drops the callback only with a
 * player, the second always) */
static PyObject *Re_BishopsAttackPlayer(PyObject *args, const char *module, byte second) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	if (second) {
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	}
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	if (!second) {
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	}
	PyObject *name;
	int mark = f.n;
	for (int k = 0; (name = Sf_Next(&f, G(module, "bishops"), k)) != NULL; k++) {
		PyObject *bishop = Sf_Entity(&f, name);
		if (Sf_True(&f, bishop) > 0) {
			PyObject *bn = Sf_Attr(&f, bishop, "Name");
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, bn, Sf_Attr(&f, usr, "Name"));
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

static PyObject *Re1_BishopsAttackPlayer(PyObject *self, PyObject *args) {
	(void)self;
	return Re_BishopsAttackPlayer(args, R1, 0);
}

static PyObject *Re2_BishopsAttackPlayer(PyObject *self, PyObject *args) {
	(void)self;
	return Re_BishopsAttackPlayer(args, R2, 1);
}

/* CreateBishop(): a bishop with the player there, in the enemies (red in the second mission) */
static PyObject *Re_CreateBishop(PyObject *args, const char *module, byte red) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	if (Sf_True(&f, USR()) != 1) {
		return Sf_None(&f);
	}
	PyObject *bishop = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ss)", "SBishop1", "Bishop");
	PyObject *pd = Sf_Own(&f, module, "GetBishopProfile", "()");
	Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", bishop, pd);
	if (red) {
		Sf_SetAttrS(&f, bishop, "TgType", "red");
	}
	Sf_SetAttrS(&f, bishop, "EntityList", "Enemies");
	Lv_Life(&f, bishop, module, "BISHOP_LIFE");
	Sf_Method(&f, G(module, "bishops"), "append", "(O)", Sf_Attr(&f, bishop, "Name"));
	return Sf_None(&f);
}

static PyObject *Re1_CreateBishop(PyObject *self, PyObject *args) {
	(void)self;
	return Re_CreateBishop(args, R1, 0);
}

static PyObject *Re2_CreateBishop(PyObject *self, PyObject *args) {
	(void)self;
	return Re_CreateBishop(args, R2, 1);
}

/* CreateBishops(numBishops) */
static PyObject *Re_CreateBishops(PyObject *args, const char *module) {
	PyObject *num;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &num)) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(iO)", 1, Sf_OpI(&f, num, '+', 1));
	int mark = f.n;
	for (int k = 0; Sf_Next(&f, range, k) != NULL; k++) {
		Sf_Own(&f, module, "CreateBishop", "()");
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

static PyObject *Re1_CreateBishops(PyObject *self, PyObject *args) {
	(void)self;
	return Re_CreateBishops(args, R1);
}

static PyObject *Re2_CreateBishops(PyObject *self, PyObject *args) {
	(void)self;
	return Re_CreateBishops(args, R2);
}

/* OnBishopsDeath(LooserName, WinnerName): the last bishop dead: a life and the bishops as the police */
static PyObject *Re_OnBishopsDeath(PyObject *args, const char *module) {
	PyObject *looser, *winner;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Method(&f, G(module, "bishops"), "remove", "(O)", looser);
	if (Sf_Len(&f, G(module, "bishops")) == 0) {
		Gd_LastBishop(&f);
	}
	return Sf_None(&f);
}

static PyObject *Re1_OnBishopsDeath(PyObject *self, PyObject *args) {
	(void)self;
	return Re_OnBishopsDeath(args, R1);
}

static PyObject *Re2_OnBishopsDeath(PyObject *self, PyObject *args) {
	(void)self;
	return Re_OnBishopsDeath(args, R2);
}

/* PreSetNextMission(UsrName, CharName, optId, FullOption): Dtritus gets his matrix back, the bishops go, the scene of
 * the matrix plays (the first mission: Deep Throat's message now; the second: no more Bertos, the message after the
 * scene) */
static PyObject *Re_PreSetNextMission(PyObject *args, const char *module, byte second) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	if (Sf_True(&f, G(module, "rm")) > 0) {
		Gd_RestoreUsed(&f, module);
	}
	PyObject *chars = Lv_SaveList(&f, "Conversor.AvailableChars", module);
	Lv_SaveAppend(&f, "Conversor.AvailableChars", chars, Sf_Str(&f, "Dtritus"));
	if (second) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "CreateBertos", "0");
	} else {
		Sf_Run(&f, "Speech", "AddRemoteCom", "(ssO)", "Mission_ReInsertOwnMatrix_RemoteMessage1", "DeepThroat", G(module, "SetNextMission"));
	}
	Gd_ClearList(&f, "Enemies");
	Gd_ClearList(&f, "OutCop");
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Import(&f, "Map");
	Sf_Run(&f, "Map", "InitPoliceBishops", "()");
	Sf_Import(&f, "MapMatrix");
	if (second) {
		Sf_Run(&f, "MapMatrix", "RestoreMatrix", "(O)", G(module, "ShowDTMessage"));
	} else {
		Sf_Run(&f, "MapMatrix", "RestoreMatrix", "()");
	}
	return Sf_None(&f);
}

static PyObject *Re1_PreSetNextMission(PyObject *self, PyObject *args) {
	(void)self;
	return Re_PreSetNextMission(args, R1, 0);
}

static PyObject *Re2_PreSetNextMission(PyObject *self, PyObject *args) {
	(void)self;
	return Re_PreSetNextMission(args, R2, 1);
}

PyMethodDef g_GDBReinsertOwnMatrixMethods[] = {
	{ "GetBishopProfile", Re1_GetBishopProfile, 1, NULL },
	{ "InitMission", Re1_InitMission, 1, NULL },
	{ "BishopsAttackPlayer", Re1_BishopsAttackPlayer, 1, NULL },
	{ "CreateBishop", Re1_CreateBishop, 1, NULL },
	{ "CreateBishops", Re1_CreateBishops, 1, NULL },
	{ "OnBishopsDeath", Re1_OnBishopsDeath, 1, NULL },
	{ "PreSetNextMission", Re1_PreSetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_GDBReinsertOwnMatrix2Methods[] = {
	{ "GetBishopProfile", Re2_GetBishopProfile, 1, NULL },
	{ "InitMission", Re2_InitMission, 1, NULL },
	{ "BishopsAttackPlayer", Re2_BishopsAttackPlayer, 1, NULL },
	{ "CreateBishop", Re2_CreateBishop, 1, NULL },
	{ "CreateBishops", Re2_CreateBishops, 1, NULL },
	{ "OnBishopsDeath", Re2_OnBishopsDeath, 1, NULL },
	{ "PreSetNextMission", Re2_PreSetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
