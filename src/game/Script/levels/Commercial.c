/*
 * The scripts of the level Commercial in C (levels/commercial/scripts, issue #2): the Map module (the traffic, the
 * metro and the elevator to the temple), MapRace (the start positions of the eight races) and the ten missions of the
 * level (Mission_DestroyMickus, Mission_ProtectGus, Mission_DestroyStupidus, Mission_Win1stRace,
 * Mission_DestroyCommSats2, Mission_FirstMeetDeepThroat, Mission_GetDataPart3, Mission_DefeatCrazyGambler3,
 * Mission_DestroyPorkinsTransports). MapSnd is data (data/levels/commercial/mapsnd.def). The module variables of the
 * missions (their counters, lists, constants and the models) and the module code (the save variables they create,
 * MapRace.Races) stay in the scripts; the functions read and write them as their globals.
 */
#include "scrap/script.h"
#include "Levels.h"

#define DMK "Mission_DestroyMickus"
#define PG "Mission_ProtectGus"
#define DS "Mission_DestroyStupidus"
#define WR "Mission_Win1stRace"
#define CS "Mission_DestroyCommSats2"
#define FM "Mission_FirstMeetDeepThroat"
#define GD "Mission_GetDataPart3"
#define CG "Mission_DefeatCrazyGambler3"
#define PT "Mission_DestroyPorkinsTransports"

#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define USR() Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0)
#define LANG(k) Sf_Lang(&f, Sf_Str(&f, k))
#define SAVE(n) Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", n)
#define G(m, n) Sf_Global(&f, m, n)
#define NOARGS Sf_Build(&f, "()")

static const char *const ac_ships_bertos = "[\"SMerc1\",\"SMerc2\",\"SBishop1\",\"SBerto1\",\"SArchbishop1\"]";

/* `a and b` of two values already there */
static int Cm_And(scriptFrame_t *f, PyObject *a, PyObject *b) {
	int t = Sf_True(f, a);
	return t > 0 ? Sf_True(f, b) : t;
}

/* SScorer.Set(0, item, var, v) */
static byte Cm_Set0(scriptFrame_t *f, const char *item, const char *var, PyObject *v) {
	return Sf_Run(f, "SScorer", "Set", "(issO)", 0, item, var, v);
}

/* SVec.ModSqr(SVec.Sub(a.Pos, b.Pos)) (a new value of the frame) */
static PyObject *Cm_DistSqr(scriptFrame_t *f, PyObject *a, PyObject *b) {
	PyObject *pa = Sf_Attr(f, a, "Pos");
	return Sf_Ask(f, "SVec", "ModSqr", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", pa, Sf_Attr(f, b, "Pos")));
}

/* long(o) */
static PyObject *Cm_Long(scriptFrame_t *f, PyObject *o) {
	return f->failed || !o ? NULL : Sf_Keep(f, PyNumber_Long(o));
}

/* a global times itself (MAX_SCANNING_DIST * MAX_SCANNING_DIST) */
static PyObject *Cm_Sqr(scriptFrame_t *f, const char *module, const char *name) {
	PyObject *a = Sf_Global(f, module, name);
	return Sf_Op(f, a, '*', Sf_Global(f, module, name));
}

/*
 * a mission item dropped at pos (the money, the amplifier, the data, the matrix): the model and radius are globals of
 * module (radius NULL: 3500), with the take sound and a mission arrow above it named arrow (NULL: the item's name +
 * "CheckArrow")
 */
static byte Cm_Drop(scriptFrame_t *f, const char *module, PyObject *pos, const char *name, const char *model, const char *radius,
	int onlyUser, const char *onTake, const char *sound, const char *arrow) {
	PyObject *x = Sf_Item(f, pos, 0), *y = Sf_Item(f, pos, 1), *z = Sf_Item(f, pos, 2);
	PyObject *l = Sf_Ask(f, "Scrap", "CreateEntity", "(sOOOs)", name, x, y, z, "Item");
	Sf_Method(f, l, "SetModel", "(O)", Sf_Global(f, module, model));
	Sf_SetAttr(f, l, "Radius", radius ? Sf_Global(f, module, radius) : Sf_Int(f, 3500));
	Sf_SetAttrI(f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(f, l, "AutoFace", 0);
	Sf_SetAttrI(f, l, "OnlyUser", onlyUser);
	Sf_SetAttr(f, l, "rTime", Sf_Float(f, 0.25));
	Sf_SetAttrS(f, l, "OnTake", onTake);
	Sf_Import(f, "Items");
	Sf_Run(f, "Items", "SetSound", "(Os)", l, sound);
	PyObject *an = arrow ? Sf_Str(f, arrow) : Sf_Cat(f, "Os", Sf_Attr(f, l, "Name"), "CheckArrow");
	x = Sf_Item(f, pos, 0), y = Sf_Item(f, pos, 1), z = Sf_Item(f, pos, 2);
	PyObject *l2 = Sf_Ask(f, "Scrap", "CreateEntity", "(OOOOs)", an, x, y, z, "WithLife");
	Sf_SetAttr(f, l2, "CamTgt", Sf_Build(f, "(iii)", 0, 1000, 0));
	Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(f, l2, "Name"));
	return !f->failed;
}

/* a mission item taken: itm.rTime = 20, deleted 5 s later, its arrow deleted now */
static byte Cm_Taken(scriptFrame_t *f, PyObject *item, PyObject *arrow) {
	PyObject *itm = Sf_Entity(f, item);
	Sf_SetAttrI(f, itm, "rTime", 20);
	Sf_Later(f, Sf_Int(f, 5), Sf_Attr(f, itm, "Del"), Sf_Build(f, "()"), NULL);
	Sf_Method(f, Sf_Entity(f, arrow), "Del", "()");
	return !f->failed;
}

/* the save variables of the characters' places: pairs of name and value, ending with NULL */
static byte Cm_Places(scriptFrame_t *f, const char *const *pairs) {
	for (; *pairs && !f->failed; pairs += 2) {
		Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", pairs[0], pairs[1]);
	}
	return !f->failed;
}

/* MissionsFuncs.SetNextMission(lib, map); SaveGame.MMDesc(desc, 1) */
static byte Cm_Next(scriptFrame_t *f, const char *lib, const char *map, const char *desc) {
	return Sf_Run(f, "MissionsFuncs", "SetNextMission", "(ss)", lib, map) && Sf_Run(f, "SaveGame", "MMDesc", "(si)", desc, 1);
}

/* a free phone cab as the target of the mission, used with the function use of module */
static PyObject *Cm_PhoneCab(scriptFrame_t *f, const char *use) {
	Sf_Import(f, "PhoneCab");
	PyObject *pc = Sf_Ask(f, "PhoneCab", "GetFree", "()");
	Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(f, pc, "Name"));
	Sf_SetAttrS(f, pc, "UseFunc", use);
	return pc;
}

/*
 * UsePhoneCab of a mission: the audio com of the phone cab, the police warning while the alarm is on, else the NI
 * talk of who with the end function end of module. global: pc is the global pc of module; activate: the cab stops
 * being the target; stop: the scheduled functions of that name go; after: the cab's use afterwards; outOfRange:
 * Speech.OnOutOfRange = None at the end
 */
typedef struct {
	const char *police, *who, *end, *ni, *stop, *after;
	byte global, activate, outOfRange;
} cmPhone_t;

static PyObject *Cm_UsePhone(PyObject *args, const char *module, const cmPhone_t *p) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "PhoneCab");
	Sf_SpeechNames(&f, usr, ch);
	Sf_Run(&f, "PhoneCab", "UseAudioCom", "(OOOO)", usr, ch, opt, full);
	PyObject *pc = Sf_Entity(&f, ch);
	if (p->global) {
		Sf_SetGlobal(&f, module, "pc", pc);
	}
	int alarm = Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0));
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", p->global ? G(module, "pc") : pc, p->police, p->who);
	} else if (alarm == 0) {
		if (p->activate) {
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", p->global ? G(module, "pc") : pc, 0);
		}
		if (p->stop) {
			Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", p->stop);
		}
		Sf_SpeechEnd(&f, G(module, p->end), NULL);
		Sf_NISpeech(&f, p->who, p->ni);
		if (p->after) {
			Sf_SetAttrS(&f, pc, "UseFunc", p->after);
		}
	}
	if (p->outOfRange) {
		Sf_SetMod(&f, "Speech", "OnOutOfRange", Py_None);
	}
	return Sf_None(&f);
}

/* Speech.AddRemoteCom(key, who[, then, params]) (then NULL: two arguments; params NULL: three) */
static byte Cm_Com(scriptFrame_t *f, const char *key, const char *who, PyObject *then, PyObject *params) {
	if (!then) {
		return Sf_Run(f, "Speech", "AddRemoteCom", "(ss)", key, who);
	}
	return params ? Sf_Run(f, "Speech", "AddRemoteCom", "(ssOO)", key, who, then, params) :
		Sf_Run(f, "Speech", "AddRemoteCom", "(ssO)", key, who, then);
}

/* Menu.MissionScorerCall = v */
static byte Cm_ScorerCall(scriptFrame_t *f, PyObject *v) {
	return Sf_SetMod(f, "Menu", "MissionScorerCall", v);
}

/* the X and Y of the mission scorer text plus offsets */
static PyObject *Cm_X(scriptFrame_t *f, long d) {
	PyObject *x = Sf_Mod(f, "Scorer", "MissionScorerText_X");
	return d ? Sf_OpI(f, x, '+', d) : x;
}

static PyObject *Cm_Y(scriptFrame_t *f, long d) {
	PyObject *y = Sf_Mod(f, "Scorer", "MissionScorerText_Y");
	return d ? Sf_OpI(f, y, '+', d) : y;
}

/* Scrap.GetLangStr(key) + ":" */
static PyObject *Cm_Label(scriptFrame_t *f, const char *key) {
	return Sf_Cat(f, "Os", Sf_Lang(f, Sf_Str(f, key)), ":");
}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors; the traffic by who controls the robots, the metro and the elevator */
static PyObject *Commercial_InitStatus(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "OutPolice", "Traffic", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 1));
	int boss = Sf_EqS(&f, SAVE("RobotsControlledByBoss"), "1");
	if (boss > 0) {
		PyObject *upper = Sf_Mod(&f, "Init", "UpperMap");
		int done = Sf_In(&f, upper, Sf_Eval(&f, SAVE("ComSatsMissionsMapsFinished"), "Map"));
		if (done > 0) {
			Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", 0);
			Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
			Sf_SetMod(&f, "OutPolice", "PoliceActive", Sf_Int(&f, 0));
		} else if (done == 0) {
			Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 5);
			Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", 5);
		}
		Sf_SetMod(&f, "Traffic", "AcNormalShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumNormal", Sf_Int(&f, 0));
		Sf_SetMod(&f, "Traffic", "AcMediumShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumMedium", Sf_Int(&f, 0));
		Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	} else if (boss == 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 10);
		Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", -1);
		Sf_SetMod(&f, "Traffic", "AcNormalShips", Sf_Build(&f, "[sss]", "TLight1", "TLight2", "TLight3"));
		Sf_SetMod(&f, "Traffic", "NumNormal", Sf_Int(&f, 80));
		Sf_SetMod(&f, "Traffic", "AcMediumShips", Sf_Build(&f, "[sss]", "TMed1", "TMed2", "TMed3"));
		Sf_SetMod(&f, "Traffic", "NumMedium", Sf_Int(&f, 40));
		Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	}
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Metro", "DM_Door_MetroExt_Comercial");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "MetroExt", "DM_Door_MetroExt_Comercial");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Elevator", "DM_Door_Elevator_Temple");
	return Sf_None(&f);
}

/* Init(LevelPath): the metro, the elevator to the temple, the colour of the clouds */
static PyObject *Commercial_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Metro");
	Sf_Import(&f, "Elevator");
	Sf_Run(&f, "Metro", "SetAsMetro", "(s)", "DM_Door_MetroExt_Comercial");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Temple", "Temple");
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudRed", 30);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudGreen", 60);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudBlue", 100);
	return Sf_None(&f);
}

PyMethodDef g_CommercialMapMethods[] = {
	{ "InitStatus", Commercial_InitStatus, 1, NULL },
	{ "Init", Commercial_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapRace ---- */

#include "CommercialRaces.inc"

/* InitStatus(LevelPath), Init(LevelPath): nothing */
static PyObject *MapRace_Pass(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	return Sf_None(&f);
}

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

PyMethodDef g_CommercialMapRaceMethods[] = {
	{ "InitStatus", MapRace_Pass, 1, NULL },
	{ "Init", MapRace_Pass, 1, NULL },
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

static PyObject *Mickus_GetFuncEnemiesProfile(PyObject *self, PyObject *args) {
	SF_ARGS("");
	return Sf_Return(&f, Lv_Profile(&f, DMK, func_enemies_profile));
}

static PyObject *Mickus_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, DMK, ac_ships_bertos);
}

/* Init(LevelPath): the transports, the mission when the player has a ship, the callbacks of boarding */
static PyObject *Mickus_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(DMK, "TreasureModel"));
	Sf_Run(&f, "Scrap", "CreateEntityList", "(s)", "Transports");
	Sf_Run(&f, "Scrap", "CreateEntityList", "(s)", "FuncEnemies");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Transports", "OnDeath", "Mission_DestroyMickus.OnTransportsDeath");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "FuncEnemies", "OnDeath", "Mission_DestroyMickus.OnFuncEnemiesDeath");
	Sf_Own(&f, DMK, "CreateTransports", "(O)", Sf_ToInt(&f, SAVE("NumTransports")));
	PyObject *usr = USR();
	int car = Sf_True(&f, usr);
	if (car > 0) {
		car = Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car"));
	}
	if (car > 0) {
		Sf_Own(&f, DMK, "InitMission", "()");
	}
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(DMK, "InitMission"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(DMK, "StopMission"));
	if (Sf_Compare(&f, SAVE("MissionState"), "!=", Sf_Str(&f, "FirstTime")) > 0) {
		Cm_Com(&f, "Mission_Search&DestroyMickus_RemoteMessage3", "Traitorius", NULL, NULL);
	}
	return Sf_None(&f);
}

/* CreateTransports(numTransports): the transports on the traffic paths, one of them Mickus */
static PyObject *Mickus_CreateTransports(PyObject *self, PyObject *args) {
	PyObject *num;
	SF_ARGS("O", &num);
	Sf_Import(&f, "Traffic");
	Sf_Import(&f, "VehiclesNPC");
	PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(iO)", 1, Sf_OpI(&f, num, '+', 1));
	PyObject *i;
	for (int k = 0; (i = Sf_Next(&f, range, k)) != NULL; k++) {
		int mark = f.n;
		Sf_Run(&f, "Traffic", "SetPath", "()");
		PyObject *trans = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(sO)", "TLight2", Sf_Cat(&f, "sO", "Transport-", Sf_Repr(&f, i)));
		Sf_Run(&f, "SAI", "SetInertia", "(Oi)", Sf_Attr(&f, trans, "Name"), 0);
		Sf_Run(&f, "Traffic", "ResetPath", "()");
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOidd)", 3, Sf_Attr(&f, trans, "Name"), 0, 2.0, 1.0);
		Sf_SetAttrS(&f, trans, "EntityList", "Transports");
		Sf_Method(&f, G(DMK, "transports"), "append", "(O)", Sf_Attr(&f, trans, "Name"));
		Sf_Release(&f, mark);
	}
	PyObject *hi = Sf_Op(&f, Sf_ToFloat(&f, num), '-', Sf_Float(&f, 0.05));
	PyObject *n = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "Rand", "(dO)", 0.0, hi));
	Sf_SetGlobal(&f, DMK, "mickus", Sf_GetItem(&f, G(DMK, "transports"), n));
	PyObject *e = Sf_Entity(&f, G(DMK, "mickus"));
	int t = Sf_True(&f, e);
	if (t > 0) {
		Sf_SetAttrS(&f, e, "OnDamage", "Mission_DestroyMickus.OnMickusDamage");
		Sf_SetAttr(&f, e, "MaxLife", G(DMK, "MAX_MICKUS_LIFE"));
		Sf_SetAttr(&f, e, "Life", G(DMK, "MAX_MICKUS_LIFE"));
		Sf_SetAttrI(&f, e, "Money", -1);
	} else if (t == 0) {
		Sf_RaiseText(&f, "La entidad de mickus debe existir");
	}
	return Sf_None(&f);
}

/* InitMission(): the transports as targets, the scorer of the scan, the scan every second */
static PyObject *Mickus_InitMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_EqS(&f, SAVE("MissionState"), "FirstTime") > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "InspectTransports");
		Cm_Com(&f, "Mission_Search&DestroyMickus_RemoteMessage1", "Traitorius", NULL, NULL);
	}
	Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", G(DMK, "transports"));
	Cm_ScorerCall(&f, G(DMK, "MissionScorer"));
	Sf_DrawScorer(&f);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnFinishAutoUpdate", "Mission_DestroyMickus.OnFinishScan");
	Sf_Later(&f, Sf_Float(&f, 1.0), G(DMK, "ScanShip"), NOARGS, "ScanShip");
	return Sf_None(&f);
}

/* StopMission(): no scorer, no scan */
static PyObject *Mickus_StopMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_ScorerCall(&f, Py_None);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ScanShip");
	return Sf_None(&f);
}

/* OnMickusDamage(LooserName, WinnerName, Damage): the player shot Mickus: identified */
static PyObject *Mickus_OnMickusDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	if (Lv_UsrIs(&f, USR(), winner) > 0) {
		PyObject *e = Sf_Entity(&f, looser);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrS(&f, e, "OnDamage", "");
			Sf_Own(&f, DMK, "MickusIdentified", "()");
		}
	}
	return Sf_None(&f);
}

/* the scan bar blinks (lost) or runs */
static byte Mk_Blink(scriptFrame_t *f, int lost) {
	return Cm_Set0(f, "ScanRemainingTime", "Blink", Sf_Int(f, lost)) && Cm_Set0(f, "ScanRemainingTime", "IsAutoUpdatable", Sf_Int(f, !lost));
}

/* ScanShip(): every second: the ship being scanned stays near, else the next transport near the player is scanned */
static PyObject *Mickus_ScanShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	Sf_Later(&f, Sf_Float(&f, 1.0), G(DMK, "ScanShip"), NOARGS, "ScanShip");
	PyObject *e = Sf_Entity(&f, G(DMK, "currentScannedShip"));
	int t = Sf_True(&f, e);
	if (t > 0) {
		if (Sf_Compare(&f, Cm_DistSqr(&f, e, usr), ">", Cm_Sqr(&f, DMK, "MAX_SCANNING_DIST")) > 0) {
			if (Sf_EqI(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "ScanRemainingTime", "Blink"), 0) > 0) {
				Mk_Blink(&f, 1);
				Sf_Later(&f, G(DMK, "MAX_TIME_TARGET_LOST"), G(DMK, "CheckContinueScanning"), NOARGS, NULL);
			}
		} else if (!f.failed) {
			Mk_Blink(&f, 0);
		}
		return Sf_None(&f);
	}
	PyObject *s;
	for (int k = 0; (s = Sf_Next(&f, G(DMK, "transports"), k)) != NULL; k++) {
		int mark = f.n;
		e = Sf_Entity(&f, s);
		if (Sf_True(&f, e) > 0 && Sf_Compare(&f, Cm_DistSqr(&f, e, usr), "<", Cm_Sqr(&f, DMK, "MAX_SCANNING_DIST")) > 0) {
			Sf_SetGlobal(&f, DMK, "currentScannedShip", Sf_Attr(&f, e, "Name"));
			Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", G(DMK, "currentScannedShip"));
			Cm_Set0(&f, "ScanStatusText", "Visible", Sf_Int(&f, 1));
			Mk_Blink(&f, 0);
			Cm_Set0(&f, "ScanRemainingTime", "MaxValue", G(DMK, "MAX_SCANNING_TIME"));
			Cm_Set0(&f, "ScanRemainingTime", "Value", G(DMK, "MAX_SCANNING_TIME"));
			Cm_Set0(&f, "ScanRemainingTime", "Visible", Sf_Int(&f, 1));
			break;
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* CheckContinueScanning(): the ship came back near in time, or the scan starts again */
static PyObject *Mickus_CheckContinueScanning(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	PyObject *e = Sf_Entity(&f, G(DMK, "currentScannedShip"));
	if (Cm_And(&f, usr, e) > 0 && Sf_Compare(&f, Cm_DistSqr(&f, e, usr), "<", Cm_Sqr(&f, DMK, "MAX_SCANNING_DIST")) > 0) {
		Mk_Blink(&f, 0);
		return Sf_None(&f);
	}
	if (!f.failed) {
		Sf_Own(&f, DMK, "ResetScan", "()");
	}
	return Sf_None(&f);
}

/* ResetScan(): no ship scanned, the scorer hidden (refilled a moment later) */
static PyObject *Mickus_ResetScan(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, DMK, "currentScannedShip", Sf_Str(&f, ""));
	Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
	Cm_Set0(&f, "ScanRemainingTime", "IsAutoUpdatable", Sf_Int(&f, 0));
	Cm_Set0(&f, "ScanRemainingTime", "Visible", Sf_Int(&f, 0));
	Cm_Set0(&f, "ScanStatusText", "Visible", Sf_Int(&f, 0));
	Sf_Later(&f, Sf_Float(&f, 0.1), G(DMK, "ResetScan2"), NOARGS, NULL);
	return Sf_None(&f);
}

static PyObject *Mickus_ResetScan2(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_Set0(&f, "ScanRemainingTime", "Blink", Sf_Int(&f, 0));
	Cm_Set0(&f, "ScanRemainingTime", "MaxValue", G(DMK, "MAX_SCANNING_TIME"));
	Cm_Set0(&f, "ScanRemainingTime", "Value", G(DMK, "MAX_SCANNING_TIME"));
	return Sf_None(&f);
}

/* OnFinishScan(): the scan bar ran out: Mickus, or one transport fewer */
static PyObject *Mickus_OnFinishScan(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
	Cm_Set0(&f, "ScanStatusText", "Visible", Sf_Int(&f, 0));
	Cm_Set0(&f, "ScanRemainingTime", "Visible", Sf_Int(&f, 0));
	if (Sf_True(&f, USR()) != 1) {
		return Sf_None(&f);
	}
	int found = Sf_Compare(&f, G(DMK, "currentScannedShip"), "==", G(DMK, "mickus"));
	if (found > 0) {
		Sf_Own(&f, DMK, "MickusIdentified", "()");
	} else if (found == 0) {
		Cm_Com(&f, "Mission_Search&DestroyMickus_RemoteMessage2", "Traitorius", NULL, NULL);
		Sf_Method(&f, G(DMK, "transports"), "remove", "(O)", G(DMK, "currentScannedShip"));
		PyObject *e = Sf_Entity(&f, G(DMK, "currentScannedShip"));
		if (Sf_True(&f, e) > 0) {
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
		}
	}
	Sf_Own(&f, DMK, "ResetScan", "()");
	return Sf_None(&f);
}

/* MickusIdentified(): the other transports go, Mickus flees, the functionaries come after the message */
static PyObject *Mickus_MickusIdentified(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(DMK, "ReassignTarget"));
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *s;
	for (int k = 0; (s = Sf_Next(&f, G(DMK, "transports"), k)) != NULL; k++) {
		int mark = f.n;
		PyObject *e = Sf_Entity(&f, s);
		int other = Sf_True(&f, e);
		if (other > 0) {
			other = Sf_Compare(&f, Sf_Attr(&f, e, "Name"), "!=", G(DMK, "mickus"));
		}
		if (other > 0) {
			Sf_Method(&f, e, "Del", "()");
		}
		Sf_Release(&f, mark);
	}
	Sf_DelGlobal(&f, DMK, "transports");
	Sf_SetGlobal(&f, DMK, "transports", Sf_Build(&f, "[]"));
	PyObject *e = Sf_Entity(&f, G(DMK, "mickus"));
	if (Sf_True(&f, e) > 0) {
		Cm_Com(&f, "Mission_Search&DestroyMickus_RemoteMessage4", "Traitorius", G(DMK, "TraitoriusFuncEnemiesCom"), NULL);
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, e, "Name"));
		Sf_SetAttr(&f, e, "Style", Sf_OpI(&f, Sf_Attr(&f, e, "Style"), '+', 2000));
		Sf_SetAttrS(&f, e, "TgType", "red");
		Sf_SetAttrS(&f, e, "OnDamage", "");
		Cm_ScorerCall(&f, Py_None);
		Sf_DrawScorer(&f);
		PyObject *d = Sf_Op(&f, G(DMK, "MAX_MICKUS_DIST"), '+', Sf_OpI(&f, G(DMK, "MAX_MICKUS_DIST"), '/', 2));
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOO)", 5, G(DMK, "mickus"), Sf_Attr(&f, usr, "Name"), d);
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ScanShip");
		PyObject *n = Sf_ToInt(&f, SAVE("NumTransports"));
		if (Sf_EqI(&f, n, 10) > 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "NumTransports", "5");
		} else if (!f.failed && Sf_EqI(&f, n, 5) > 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "NumTransports", "3");
		}
		Sf_Later(&f, Sf_Int(&f, 4), G(DMK, "CheckMickusVisibility"), NOARGS, "CheckMickusVisibility");
	}
	return Sf_None(&f);
}

/* ReassignTarget(): after boarding: Mickus flees from the new ship, the functionaries attack it */
static PyObject *Mickus_ReassignTarget(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) > 0) {
		PyObject *m = Sf_Entity(&f, G(DMK, "mickus"));
		if (Sf_True(&f, m) > 0) {
			PyObject *d = Sf_Op(&f, G(DMK, "MAX_MICKUS_DIST"), '+', Sf_OpI(&f, G(DMK, "MAX_MICKUS_DIST"), '/', 2));
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOO)", 5, G(DMK, "mickus"), Sf_Attr(&f, usr, "Name"), d);
		}
		PyObject *name = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "FuncEnemies", "First");
		PyObject *e = Sf_Entity(&f, name);
		int mark = f.n;
		while (Sf_True(&f, e) > 0) {
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, e, "Name"), Sf_Attr(&f, usr, "Name"));
			PyObject *next = Sf_Entity(&f, Sf_Attr(&f, e, "NextInList"));
			Sf_ReleaseKeep(&f, mark, &next, 1);
			e = next;
		}
	}
	return Sf_None(&f);
}

/* TraitoriusFuncEnemiesCom(): two functionaries after the player, Traitorius warns 2 s later */
static PyObject *Mickus_TraitoriusFuncEnemiesCom(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Own(&f, DMK, "CreateFuncEnemies", "(i)", 2);
	Sf_Later(&f, Sf_Int(&f, 2), Sf_Mod(&f, "Speech", "AddRemoteCom"), Sf_Build(&f, "(ss)", "Mission_Search&DestroyMickus_RemoteMessage7",
		"Traitorius"), NULL);
	return Sf_None(&f);
}

/* CheckMickusVisibility(): Mickus out of range and out of sight: lost, the mission starts again with fewer transports */
static PyObject *Mickus_CheckMickusVisibility(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	PyObject *mic = Sf_Entity(&f, G(DMK, "mickus"));
	if (Cm_And(&f, usr, mic) > 0) {
		PyObject *d = Cm_Long(&f, Cm_DistSqr(&f, usr, mic));
		PyObject *m1 = Cm_Long(&f, G(DMK, "MAX_MICKUS_DIST"));
		PyObject *m2 = Cm_Long(&f, G(DMK, "MAX_MICKUS_DIST"));
		if (Sf_Compare(&f, d, ">", Sf_Op(&f, m1, '*', m2)) > 0) {
			PyObject *up = Sf_Attr(&f, usr, "Pos");
			PyObject *res = Sf_Ask(&f, "Scrap", "TestLine", "(OOs)", up, Sf_Attr(&f, mic, "Pos"), "");
			int lost = Sf_True(&f, res);
			if (lost > 0) {
				lost = Sf_EqS(&f, Sf_Item(&f, res, 1), "");
			}
			if (lost > 0) {
				PyObject *name = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "FuncEnemies", "First");
				int mark = f.n;
				while (Sf_True(&f, name) > 0) {
					PyObject *e = Sf_Entity(&f, name);
					if (Sf_True(&f, e) != 1) {
						break;
					}
					PyObject *next = Sf_Attr(&f, e, "NextInList");
					Sf_SetAttrS(&f, e, "EntityList", "Traffic");
					Sf_SetAttrS(&f, e, "TgType", "none");
					Sf_Run(&f, "SAI", "SetStateVehicle", "(iO)", 3, Sf_Attr(&f, e, "Name"));
					Sf_ReleaseKeep(&f, mark, &next, 1);
					name = next;
				}
				if (Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car")) > 0) {
					Sf_SetAttrS(&f, usr, "Target", "");
				}
				Cm_Com(&f, "Mission_Search&DestroyMickus_RemoteMessage6", "Traitorius", NULL, NULL);
				Sf_Method(&f, mic, "Del", "()");
				Sf_SetGlobal(&f, DMK, "currentScannedShip", Sf_Str(&f, ""));
				Sf_SetGlobal(&f, DMK, "transports", Sf_Build(&f, "[]"));
				Sf_SetGlobal(&f, DMK, "mickus", Sf_Str(&f, ""));
				Sf_Import(&f, "Parking");
				Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(DMK, "InitMission"));
				Sf_Own(&f, DMK, "CreateTransports", "(O)", Sf_ToInt(&f, SAVE("NumTransports")));
				Sf_Own(&f, DMK, "InitMission", "()");
				return Sf_None(&f);
			}
		}
	}
	Sf_Later(&f, Sf_Int(&f, 2), G(DMK, "CheckMickusVisibility"), NOARGS, "CheckMickusVisibility");
	return Sf_None(&f);
}

/* CreateFuncEnemies(numEnemies): functionaries at free spawn points, after the player */
static PyObject *Mickus_CreateFuncEnemies(PyObject *self, PyObject *args) {
	PyObject *num;
	SF_ARGS("O", &num);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) > 0) {
		PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(O)", num), *i;
		for (int k = 0; (i = Sf_Next(&f, range, k)) != NULL; k++) {
			int mark = f.n;
			PyObject *fn = Lv_SpawnCar(&f, "Functionary", 0);
			Lv_StdProfile(&f, DMK, "GetFuncEnemiesProfile", fn);
			Sf_SetAttrS(&f, fn, "TgType", "red");
			Sf_SetAttrS(&f, fn, "EntityList", "FuncEnemies");
			Lv_Life(&f, fn, DMK, "FUNCS_LIFE");
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, fn, "Name"), Sf_Attr(&f, usr, "Name"));
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* `not Scrap.GetAlarm()[0]` */
static int Cm_NoAlarm(scriptFrame_t *f) {
	int a = Sf_True(f, Sf_Item(f, Sf_Ask(f, "Scrap", "GetAlarm", "()"), 0));
	return a < 0 ? -1 : !a;
}

/* OnFuncEnemiesDeath(LooserName, WinnerName): the alarm when the player killed it, another one 5 s later */
static PyObject *Mickus_OnFuncEnemiesDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	int on = Lv_UsrIs(&f, USR(), winner);
	if (on > 0) {
		on = Cm_NoAlarm(&f);
	}
	if (on > 0) {
		Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	}
	Sf_Later(&f, Sf_Int(&f, 5), G(DMK, "CreateFuncEnemies"), Sf_Build(&f, "(i)", 1), NULL);
	return Sf_None(&f);
}

/* OnTransportsDeath(LooserName, WinnerName): one transport fewer; Mickus killed by the player drops his money */
static PyObject *Mickus_OnTransportsDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	if (Sf_In(&f, looser, G(DMK, "transports")) > 0) {
		Sf_Method(&f, G(DMK, "transports"), "remove", "(O)", looser);
		if (Sf_Compare(&f, looser, "==", G(DMK, "currentScannedShip")) > 0) {
			Sf_Own(&f, DMK, "ResetScan", "()");
			Sf_SetGlobal(&f, DMK, "currentScannedShip", Sf_Str(&f, ""));
		}
	}
	if (Lv_UsrIs(&f, USR(), winner) > 0) {
		if (Cm_NoAlarm(&f) > 0) {
			Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
		}
		if (Sf_Compare(&f, looser, "==", G(DMK, "mickus")) > 0) {
			PyObject *e = Sf_Entity(&f, looser);
			if (Sf_True(&f, e) > 0) {
				Sf_SetAttrS(&f, e, "EntityList", "");
				Sf_Own(&f, DMK, "DropMoney", "(O)", Sf_Attr(&f, e, "Pos"));
			}
		}
	}
	return Sf_None(&f);
}

/* DropMoney(pos): Mickus' treasure and its arrow */
static PyObject *Mickus_DropMoney(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	Cm_Drop(&f, DMK, pos, "MickusMoney", "TreasureModel", NULL, 1, "Mission_DestroyMickus.TakeMoney", "TakeTreasure", "MissionCheckArrow");
	return Sf_None(&f);
}

/* TakeMoney(ItemName, CharName): the player took the treasure */
static PyObject *Mickus_TakeMoney(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	if (Lv_UsrIs(&f, USR(), ch) > 0) {
		Cm_Taken(&f, Sf_Str(&f, "MickusMoney"), Sf_Str(&f, "MissionCheckArrow"));
		Cm_Com(&f, "Mission_Search&DestroyMickus_RemoteMessage8", "Traitorius", G(DMK, "SetNextMission"), NULL);
	}
	return Sf_None(&f);
}

/* SetNextMission(): on to Mission_ArrangeBankersCTF in GamblinDen */
static PyObject *Mickus_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Press",
		"Journalist.Betty_Defaut", "GamblinDen", "Journalist.Berto_Defaut", "GamblinDen", NULL };
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumTransports");
	Cm_Next(&f, "Mission_ArrangeBankersCTF", "GamblinDen", "Mission_TalkToTraitorius3_MissionText1");
	Cm_Places(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the scan text and bar, visible while a ship near the player is scanned */
static PyObject *Mickus_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	PyObject *e = Sf_Entity(&f, G(DMK, "currentScannedShip"));
	PyObject *usr = USR();
	int scanning = Cm_And(&f, usr, e);
	if (scanning > 0) {
		scanning = Sf_Compare(&f, Cm_DistSqr(&f, e, usr), "<", Cm_Sqr(&f, DMK, "MAX_SCANNING_DIST"));
	}
	PyObject *vis = Sf_Int(&f, scanning > 0);
	Sf_MissionText(&f, id, "ScanStatusText", "Text", Cm_X(&f, 0), Cm_Y(&f, 0), "Horatio", "Shadow",
		Cm_Label(&f, "Mission_Search&DestroyMickus_Scorer_1"), "Left", vis);
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "ScanRemainingTime", "Bar");
	Lv_BarDefaults(&f, id, "ScanRemainingTime", NULL, NULL);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "ScanRemainingTime", "IsAutoUpdatable", 0);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "MaxValue", G(DMK, "MAX_SCANNING_TIME"));
	if (scanning == 0) {
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "Value", G(DMK, "MAX_SCANNING_TIME"));
	}
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "ScanRemainingTime", "Visible", vis);
	return Sf_None(&f);
}

PyMethodDef g_CommercialDestroyMickusMethods[] = {
	{ "GetFuncEnemiesProfile", Mickus_GetFuncEnemiesProfile, 1, NULL },
	{ "InitStatus", Mickus_InitStatus, 1, NULL },
	{ "Init", Mickus_Init, 1, NULL },
	{ "CreateTransports", Mickus_CreateTransports, 1, NULL },
	{ "InitMission", Mickus_InitMission, 1, NULL },
	{ "StopMission", Mickus_StopMission, 1, NULL },
	{ "OnMickusDamage", Mickus_OnMickusDamage, 1, NULL },
	{ "ScanShip", Mickus_ScanShip, 1, NULL },
	{ "CheckContinueScanning", Mickus_CheckContinueScanning, 1, NULL },
	{ "ResetScan", Mickus_ResetScan, 1, NULL },
	{ "ResetScan2", Mickus_ResetScan2, 1, NULL },
	{ "OnFinishScan", Mickus_OnFinishScan, 1, NULL },
	{ "MickusIdentified", Mickus_MickusIdentified, 1, NULL },
	{ "ReassignTarget", Mickus_ReassignTarget, 1, NULL },
	{ "TraitoriusFuncEnemiesCom", Mickus_TraitoriusFuncEnemiesCom, 1, NULL },
	{ "CheckMickusVisibility", Mickus_CheckMickusVisibility, 1, NULL },
	{ "CreateFuncEnemies", Mickus_CreateFuncEnemies, 1, NULL },
	{ "OnFuncEnemiesDeath", Mickus_OnFuncEnemiesDeath, 1, NULL },
	{ "OnTransportsDeath", Mickus_OnTransportsDeath, 1, NULL },
	{ "DropMoney", Mickus_DropMoney, 1, NULL },
	{ "TakeMoney", Mickus_TakeMoney, 1, NULL },
	{ "SetNextMission", Mickus_SetNextMission, 1, NULL },
	{ "MissionScorer", Mickus_MissionScorer, 1, NULL },
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
static PyObject *Pg_Profile(PyObject *args, const char *const *vars, const char *name) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *pd = Lv_Profile(&f, PG, vars);
	Sf_SetItem(&f, Sf_Mod(&f, "VehiclesNPC", "AIProf"), Sf_Str(&f, name), pd);
	return Sf_None(&f);
}

static PyObject *Gus_CreateLonelyMercProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Pg_Profile(args, lonely_merc_profile, "LonelyMercNormal");
}

static PyObject *Gus_CreateBishopGusProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Pg_Profile(args, bishop_gus_profile, "BishopGus");
}

static PyObject *Gus_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, PG, ac_ships_bertos);
}

/* Init(LevelPath): the fight (now or when the player boards a ship), or the phone cab after it */
static PyObject *Gus_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	int fight = Sf_EqS(&f, SAVE("MissionState"), "ProtectGus");
	if (fight > 0) {
		PyObject *usr = USR();
		int car = Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car"));
		if (car > 0) {
			Sf_Own(&f, PG, "InitMission", "()");
		} else if (car == 0) {
			Sf_Import(&f, "Parking");
			Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(PG, "InitMission"));
		}
		Cm_Com(&f, "Mission_ProtectGus_RemoteMessage1", "DeepThroat", NULL, NULL);
	} else if (fight == 0) {
		Sf_Own(&f, PG, "SetPhoneCab", "()");
	}
	return Sf_None(&f);
}

/* InitMission(): the scorer, both fighters with their profiles */
static PyObject *Gus_InitMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Menu");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	Cm_ScorerCall(&f, G(PG, "MissionScorer"));
	Sf_DrawScorer(&f);
	Sf_Own(&f, PG, "CreateLonelyMercProfile", "()");
	Sf_Own(&f, PG, "CreateLonelyMerc", "()");
	Sf_Own(&f, PG, "CreateBishopGusProfile", "()");
	Sf_Own(&f, PG, "CreateBishopGus", "()");
	Sf_Own(&f, PG, "AttackEachOther", "()");
	return Sf_None(&f);
}

/* AttackEachOther(): both there and active: they attack each other; one missing: try again in 2 s */
static PyObject *Gus_AttackEachOther(PyObject *self, PyObject *args) {
	SF_ARGS("");
	int missing = Sf_True(&f, G(PG, "bishopGus"));
	missing = missing < 0 ? -1 : !missing;
	if (missing == 0) {
		int t = Sf_True(&f, G(PG, "lonelyMerc"));
		missing = t < 0 ? -1 : !t;
	}
	if (missing > 0) {
		int go = Sf_Compare(&f, G(PG, "lonelyMercLifes"), ">", Sf_Int(&f, 0));
		if (go > 0) {
			go = Sf_Compare(&f, G(PG, "bishopGusLifes"), ">", Sf_Int(&f, 0));
		}
		if (go > 0) {
			Sf_Later(&f, Sf_Int(&f, 2), G(PG, "AttackEachOther"), NOARGS, NULL);
		}
	} else if (missing == 0) {
		int act = Sf_True(&f, Sf_Attr(&f, G(PG, "lonelyMerc"), "Active"));
		if (act > 0) {
			act = Sf_True(&f, Sf_Attr(&f, G(PG, "bishopGus"), "Active"));
		}
		if (act > 0) {
			PyObject *b = Sf_Attr(&f, G(PG, "bishopGus"), "Name");
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, b, Sf_Attr(&f, G(PG, "lonelyMerc"), "Name"));
			PyObject *m = Sf_Attr(&f, G(PG, "lonelyMerc"), "Name");
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, m, Sf_Attr(&f, G(PG, "bishopGus"), "Name"));
		}
	}
	return Sf_None(&f);
}

/* VehiclesNPC.MakeAI(profile, name, InitState = 0) */
static PyObject *Pg_MakeAI(scriptFrame_t *f, const char *profile, const char *name) {
	PyObject *fn = Sf_Mod(f, "VehiclesNPC", "MakeAI");
	return Sf_CallKw(f, fn, f->failed ? NULL : Py_BuildValue("(ss)", profile, name), Sf_Build(f, "{s:i}", "InitState", 0));
}

/* CreateLonelyMerc(): the lonely mercenary, red, no taunts */
static PyObject *Gus_CreateLonelyMerc(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, PG, "lonelyMerc", Pg_MakeAI(&f, "LonelyMercNormal", "LonelyMerc"));
	Sf_SetAttrS(&f, G(PG, "lonelyMerc"), "OnDeath", "Mission_ProtectGus.OnLonelyMercDeath");
	Sf_SetAttrS(&f, G(PG, "lonelyMerc"), "TgType", "red");
	Sf_SetAttrI(&f, G(PG, "lonelyMerc"), "LaunchTaunts", 0);
	PyObject *life = G(PG, "LONELY_MERC_LIFE");
	Sf_SetAttr(&f, G(PG, "lonelyMerc"), "Life", life);
	Sf_SetAttr(&f, G(PG, "lonelyMerc"), "MaxLife", life);
	return Sf_None(&f);
}

/* CreateBishopGus(): Gus, the target while the mercenary lives */
static PyObject *Gus_CreateBishopGus(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, PG, "bishopGus", Pg_MakeAI(&f, "BishopGus", "BishopGus"));
	Sf_SetAttrS(&f, G(PG, "bishopGus"), "OnDeath", "Mission_ProtectGus.OnBishopGusDeath");
	Sf_SetAttrI(&f, G(PG, "bishopGus"), "Money", -1);
	PyObject *life = G(PG, "BISHOP_GUS_LIFE");
	Sf_SetAttr(&f, G(PG, "bishopGus"), "Life", life);
	Sf_SetAttr(&f, G(PG, "bishopGus"), "MaxLife", life);
	if (Sf_Compare(&f, G(PG, "lonelyMercLifes"), ">", Sf_Int(&f, 0)) > 0) {
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(PG, "bishopGus"), 1);
		Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", Sf_Attr(&f, G(PG, "bishopGus"), "Name"));
	}
	return Sf_None(&f);
}

/* the fight is over: the mission state, no scorer, DeepThroat's message then the phone cab */
static void Pg_Over(scriptFrame_t *f, const char *msg) {
	Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "CallDeepThroat");
	Cm_ScorerCall(f, Py_None);
	Sf_DrawScorer(f);
	Cm_Com(f, msg, "DeepThroat", Sf_Global(f, PG, "PreCallDeepThroat"), Sf_Build(f, "()"));
}

/* the next round: the dead fighter again in 2 s, the attack 0.2 s later */
static void Pg_Again(scriptFrame_t *f, const char *create) {
	Sf_DrawScorer(f);
	Sf_Later(f, Sf_Int(f, 2), Sf_Global(f, PG, create), Sf_Build(f, "()"), NULL);
	Sf_Later(f, Sf_Float(f, 2.2), Sf_Global(f, PG, "AttackEachOther"), Sf_Build(f, "()"), NULL);
}

/* `x and x.Active` of a global */
static int Pg_Active(scriptFrame_t *f, const char *name) {
	int t = Sf_True(f, Sf_Global(f, PG, name));
	return t > 0 ? Sf_True(f, Sf_Attr(f, Sf_Global(f, PG, name), "Active")) : t;
}

/* OnLonelyMercDeath(LooserName, WinnerName): a life of the mercenary less; none left: Gus has won */
static PyObject *Gus_OnLonelyMercDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_SetGlobal(&f, PG, "lonelyMerc", Py_None);
	Sf_SetGlobal(&f, PG, "lonelyMercLifes", Sf_OpI(&f, G(PG, "lonelyMercLifes"), '-', 1));
	Cm_Set0(&f, "LonelyMercLifes", "Text", Sf_Repr(&f, G(PG, "lonelyMercLifes")));
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	if (Pg_Active(&f, "bishopGus") > 0) {
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, G(PG, "bishopGus"), "Name"), 0);
	}
	int over = Sf_EqI(&f, G(PG, "lonelyMercLifes"), 0);
	if (over > 0) {
		if (Sf_True(&f, G(PG, "bishopGus")) > 0) {
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(PG, "bishopGus"), 0);
		}
		Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
		Pg_Over(&f, "Mission_ProtectGus_RemoteMessage3");
	} else if (over == 0) {
		Pg_Again(&f, "CreateLonelyMerc");
	}
	return Sf_None(&f);
}

/* OnBishopGusDeath(LooserName, WinnerName): a life of Gus less; none left: the mission goes on without him */
static PyObject *Gus_OnBishopGusDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_SetGlobal(&f, PG, "bishopGus", Py_None);
	Sf_SetGlobal(&f, PG, "bishopGusLifes", Sf_OpI(&f, G(PG, "bishopGusLifes"), '-', 1));
	Cm_Set0(&f, "BishopGusLifes", "Text", Sf_Repr(&f, G(PG, "bishopGusLifes")));
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	if (Sf_EqI(&f, G(PG, "bishopGusLifes"), 8) > 0) {
		Cm_Com(&f, "Mission_ProtectGus_RemoteMessage2", "DeepThroat", NULL, NULL);
	}
	if (Pg_Active(&f, "lonelyMerc") > 0) {
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, G(PG, "lonelyMerc"), "Name"), 0);
	}
	Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
	int over = Sf_EqI(&f, G(PG, "bishopGusLifes"), 0);
	if (over > 0) {
		if (Sf_True(&f, G(PG, "lonelyMerc")) > 0) {
			Sf_SetAttrS(&f, G(PG, "lonelyMerc"), "TgType", "None");
			Sf_SetAttrS(&f, G(PG, "lonelyMerc"), "EntityList", "Traffic");
			Sf_SetAttrS(&f, G(PG, "lonelyMerc"), "OnDeath", "Vehicles.OnDeath");
		}
		Pg_Over(&f, "Mission_ProtectGus_RemoteMessage4");
	} else if (over == 0) {
		Pg_Again(&f, "CreateBishopGus");
	}
	return Sf_None(&f);
}

/* PreCallDeepThroat(): the description, a way point, the phone cab */
static PyObject *Gus_PreCallDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToDeepThroat3b_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Own(&f, PG, "SetPhoneCab", "()");
	return Sf_None(&f);
}

/* SetPhoneCab(): a free phone cab to call DeepThroat */
static PyObject *Gus_SetPhoneCab(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Vehicles", "Speech", "Alarm", "SaveGame", "PhoneCab", "MissionsFuncs", NULL };
	SF_ARGS("");
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Cm_PhoneCab(&f, "Mission_ProtectGus.UsePhoneCab");
	return Sf_None(&f);
}

static PyObject *Gus_UsePhoneCab(PyObject *self, PyObject *args) {
	static const cmPhone_t p = { "Generics_StillPolice1_DeepThroat", "DeepThroat", "SetNextMission",
		"Mission_TalkToDeepThroat3b_ComDeepThroat_NI_1Msg", NULL, NULL, 1, 0, 0 };
	(void)self;
	return Cm_UsePhone(args, PG, &p);
}

/* SetNextMission(): the phone cab back to normal, on to Mission_IntoTheGDB */
static PyObject *Gus_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "0", "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "Temple",
		"Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Temple", NULL };
	SF_ARGS("");
	Sf_SetAttrS(&f, G(PG, "pc"), "UseFunc", "PhoneCab.DefUseAudioCom");
	Sf_SetAttrI(&f, G(PG, "pc"), "ArrowCanUse", 0);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(PG, "pc"), 0);
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Cm_Next(&f, "Mission_IntoTheGDB", "GDB", "Mission_IntoTheGDB_MissionText1");
	Cm_Places(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the lives of Gus and of the mercenary */
static PyObject *Gus_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Sf_MissionText(&f, id, "BishopGusLifes", "Text", Cm_X(&f, 15), Cm_Y(&f, 12), "BigNumbers", "Shadow", Sf_Repr(&f, G(PG, "bishopGusLifes")),
		"Center", NULL);
	Sf_MissionSprite(&f, id, "BackSpriteGus", Cm_X(&f, 0), Cm_Y(&f, 3), 128, 0, 0, 0);
	Sf_MissionText(&f, id, "LonelyMercLifes", "Text", Cm_X(&f, 15), Sf_OpI(&f, Cm_Y(&f, 12), '+', 20), "BigNumbers", "Shadow",
		Sf_Repr(&f, G(PG, "lonelyMercLifes")), "Center", NULL);
	Sf_MissionSprite(&f, id, "BackSpriteMerc", Cm_X(&f, 0), Sf_OpI(&f, Cm_Y(&f, 3), '+', 20), 128, 0, 0, 0);
	Sf_MissionText(&f, id, "BishopGusText", "Text", Cm_X(&f, 34), Cm_Y(&f, 0), "Horatio", "Shadow", LANG("Mission_ProtectGus_Scorer2"),
		"Left", NULL);
	Sf_MissionText(&f, id, "LonelyMercText", "Text", Cm_X(&f, 34), Cm_Y(&f, 20), "Horatio", "Shadow", LANG("Mission_ProtectGus_Scorer1"),
		"Left", NULL);
	return Sf_None(&f);
}

PyMethodDef g_CommercialProtectGusMethods[] = {
	{ "CreateLonelyMercProfile", Gus_CreateLonelyMercProfile, 1, NULL },
	{ "CreateBishopGusProfile", Gus_CreateBishopGusProfile, 1, NULL },
	{ "InitStatus", Gus_InitStatus, 1, NULL },
	{ "Init", Gus_Init, 1, NULL },
	{ "InitMission", Gus_InitMission, 1, NULL },
	{ "AttackEachOther", Gus_AttackEachOther, 1, NULL },
	{ "CreateLonelyMerc", Gus_CreateLonelyMerc, 1, NULL },
	{ "CreateBishopGus", Gus_CreateBishopGus, 1, NULL },
	{ "OnLonelyMercDeath", Gus_OnLonelyMercDeath, 1, NULL },
	{ "OnBishopGusDeath", Gus_OnBishopGusDeath, 1, NULL },
	{ "PreCallDeepThroat", Gus_PreCallDeepThroat, 1, NULL },
	{ "SetPhoneCab", Gus_SetPhoneCab, 1, NULL },
	{ "UsePhoneCab", Gus_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Gus_SetNextMission, 1, NULL },
	{ "MissionScorer", Gus_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyStupidus: the functionary Stupidus drops the amplifier ---- */

static const char *const stupidus_profile[] = { "Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=0",
	"UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25", "UsePercHook=20", "FactorHookMap=10", "CadenceShoot=9", "ControlAim=4",
	"AngAttack=3.14", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.25",
	"FactorDamageItemLife=0.75", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=40", "TimeMaxStrafe=2",
	"VelMaxStrafe=0.25", NULL };

static PyObject *Stupidus_GetStupidusProfile(PyObject *self, PyObject *args) {
	SF_ARGS("");
	return Sf_Return(&f, Lv_Profile(&f, DS, stupidus_profile));
}

static PyObject *Stupidus_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, DS, "[\"SMerc1\",\"SMerc2\",\"SBishop1\",\"SArchbishop1\"]");
}

/* Init(LevelPath): the head of evil Berto, the amplifier model, Stupidus */
static PyObject *Stupidus_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "SScorer", "AddModel", "(isssiii)", 0, "EvilBerto", "Models/Heads/HeadEvilBerto/HeadEvilBerto", "", 4, 25, 10);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(DS, "AmplifierModel"));
	Sf_Own(&f, DS, "CreateStupidus", "()");
	return Sf_None(&f);
}

/* CreateStupidus(): Stupidus at a free spawn point, wandering until he is shot */
static PyObject *Stupidus_CreateStupidus(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "Traffic");
	if (Sf_True(&f, USR()) > 0) {
		PyObject *spw;
		PyObject *o1 = Lv_SpawnCarAt(&f, "FuncFighter", 0, &spw);
		Sf_SetAttrI(&f, o1, "Money", -1);
		Sf_SetAttrS(&f, o1, "TgType", "red");
		Sf_SetAttr(&f, o1, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, spw, 1), 0));
		PyObject *name = Sf_Attr(&f, o1, "Name");
		PyObject *pd = Sf_Own(&f, DS, "GetStupidusProfile", "()");
		Sf_Run(&f, "VehiclesNPC", "SetStdProperties", "(OO)", name, Sf_Attr(&f, pd, "Ship"));
		Sf_SetAttrS(&f, o1, "OnDeath", "Mission_DestroyStupidus.OnStupidusDeath");
		Sf_SetAttrS(&f, o1, "OnDamage", "Mission_DestroyStupidus.OnStupidusDamage");
		Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", o1, pd);
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, name, 0);
		Lv_Life(&f, o1, DS, "STUPIDUS_LIFE");
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", name);
	}
	return Sf_None(&f);
}

/* OnStupidusDeath(LooserName, WinnerName): he drops the amplifier */
static PyObject *Stupidus_OnStupidusDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	PyObject *e = Sf_Entity(&f, looser);
	if (Sf_True(&f, e) > 0) {
		Sf_Own(&f, DS, "DropAmplifier", "(O)", Sf_Attr(&f, e, "Pos"));
	}
	return Sf_None(&f);
}

/* OnStupidusDamage(LooserName, WinnerName, Damage): shot: he attacks the player */
static PyObject *Stupidus_OnStupidusDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *usr = USR();
	PyObject *e = Sf_Entity(&f, looser);
	if (Cm_And(&f, usr, e) > 0) {
		Sf_SetAttrS(&f, e, "OnDamage", "");
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, looser, Sf_Attr(&f, usr, "Name"));
	}
	return Sf_None(&f);
}

static PyObject *Stupidus_DropAmplifier(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	Cm_Drop(&f, DS, pos, "Amplifier", "AmplifierModel", "AmplifierRadius", 1, "Mission_DestroyStupidus.TakeAmplifier", "TakeAmplifier",
		"MissionCheckArrow");
	return Sf_None(&f);
}

/* TakeAmplifier(ItemName, CharName): DeepThroat's message, Berto found 10 s later */
static PyObject *Stupidus_TakeAmplifier(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	if (Lv_UsrIs(&f, USR(), ch) > 0) {
		Cm_Taken(&f, Sf_Str(&f, "Amplifier"), Sf_Str(&f, "MissionCheckArrow"));
		Cm_Com(&f, "Mission_DestroyStupidus_RemoteMessage1", "DeepThroat", NULL, NULL);
		Sf_Later(&f, Sf_Int(&f, 10), G(DS, "BertoFound"), NOARGS, NULL);
	}
	return Sf_None(&f);
}

static PyObject *Stupidus_BertoFound(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_Com(&f, "Mission_DestroyStupidus_RemoteMessage2", "DeepThroat", G(DS, "SetNextMission"), NULL);
	return Sf_None(&f);
}

static PyObject *Stupidus_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_Next(&f, "Mission_BertoMassacre", "DownTown", "Mission_GoToDowntown_MissionText1");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

PyMethodDef g_CommercialDestroyStupidusMethods[] = {
	{ "GetStupidusProfile", Stupidus_GetStupidusProfile, 1, NULL },
	{ "InitStatus", Stupidus_InitStatus, 1, NULL },
	{ "Init", Stupidus_Init, 1, NULL },
	{ "CreateStupidus", Stupidus_CreateStupidus, 1, NULL },
	{ "OnStupidusDeath", Stupidus_OnStupidusDeath, 1, NULL },
	{ "OnStupidusDamage", Stupidus_OnStupidusDamage, 1, NULL },
	{ "DropAmplifier", Stupidus_DropAmplifier, 1, NULL },
	{ "TakeAmplifier", Stupidus_TakeAmplifier, 1, NULL },
	{ "BertoFound", Stupidus_BertoFound, 1, NULL },
	{ "SetNextMission", Stupidus_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_Win1stRace: a time race through the checkpoints of race 8 ---- */

static PyObject *Race_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, WR, ac_ships_bertos);
}

/* Init(LevelPath): the alarm hooks, the countdown resources, the race (unless the alarm is on) */
static PyObject *Race_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Alarm");
	Sf_SetMod(&f, "Alarm", "TempAbortMissionCallback", G(WR, "OnAlarmOn"));
	Sf_SetMod(&f, "Alarm", "TempRestoreMissionCallback", G(WR, "OnAlarmOff"));
	Sf_Run(&f, "RaceMaker", "PreloadCnDnResources", "()");
	int alarm = Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0));
	if (alarm > 0) {
		Sf_Own(&f, WR, "OnAlarmOn", "()");
	} else if (alarm == 0) {
		int first = Sf_EqS(&f, SAVE("MissionState"), "FirstTime");
		if (first > 0) {
			Cm_Com(&f, "Mission_Win1stRace_RemoteMessage1", "DeepThroat", NULL, NULL);
		} else if (first == 0) {
			Cm_Com(&f, "Mission_Win1stRace_RemoteMessage3", "Trurl", NULL, NULL);
		}
		Sf_Own(&f, WR, "InitTimeRace", "()");
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "");
	return Sf_None(&f);
}

/* InitTimeRace(): the start buoy of race 8 */
static PyObject *Race_InitTimeRace(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "MapRace");
	Sf_Import(&f, "RaceMaker");
	Sf_Run(&f, "MapRace", "EasyRaceCommercial8", "()");
	PyObject *pos = Sf_Item(&f, Sf_Item(&f, Sf_Mod(&f, "RaceMaker", "StartPos"), 0), 0);
	PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "RaceWaypoint", x, y, z, "Item");
	Sf_Method(&f, l, "SetModel", "(s)", "models/Misc/Wayp/Wayp.m3d");
	Sf_SetAttrS(&f, l, "OnTake", "Mission_Win1stRace.OnRaceStart");
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttrI(&f, l, "RadarShow", 1);
	Sf_SetAttrI(&f, l, "Radius", 2400);
	Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, Sf_Item(&f, Sf_Mod(&f, "RaceMaker", "StartPos"), 0), 1), 0));
	Sf_Run(&f, "Scrap", "Preload3DObject", "(s)", "models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d");
	Sf_SetGlobal(&f, WR, "waypointName", Sf_Attr(&f, l, "Name"));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", G(WR, "waypointName"));
	return Sf_None(&f);
}

/* OnRaceStart(ItemName, VehicleName): the player at the buoy: the camera, the countdown, the start */
static PyObject *Race_OnRaceStart(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	Sf_Import(&f, "RaceMaker");
	if (Lv_UsrIs(&f, USR(), veh) > 0) {
		PyObject *t = Sf_Ask(&f, "RaceMaker", "StartCameraTravelling", "(OO)", item, veh);
		Sf_Later(&f, t, G(WR, "RaceGo"), Sf_Build(&f, "(OO)", item, veh), NULL);
		Sf_Run(&f, "RaceMaker", "CreateRaceCountDown", "()");
	}
	return Sf_None(&f);
}

/* RaceGo(ItemName, VehicleName): the race starts at the first checkpoint */
static PyObject *Race_RaceGo(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	Sf_Import(&f, "RaceMaker");
	Sf_Run(&f, "RaceMaker", "AbortCameraTravelling", "()");
	Sf_Run(&f, "RaceMaker", "DestroyRaceCountDown", "()");
	Sf_SetAttrS(&f, Sf_Entity(&f, item), "OnTake", "Mission_Win1stRace.OnCheckPoint");
	Sf_Own(&f, WR, "OnCheckPoint", "(OO)", item, veh);
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionMusic", 1);
	return Sf_None(&f);
}

/* OnCheckPoint(ItemName, VehicleName): the buoy moves on and gives time, the last one wins */
static PyObject *Race_OnCheckPoint(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "Sound");
	Sf_SetGlobal(&f, WR, "currentWaypoints", Sf_OpI(&f, G(WR, "currentWaypoints"), '+', 1));
	int first = Sf_EqI(&f, G(WR, "currentWaypoints"), 0);
	if (first > 0) {
		Sf_Run(&f, "Scrap", "Set", "(sO)", "TimeSpecialUseTimer", G(WR, "raceTime"));
	} else if (first == 0) {
		Sf_Run(&f, "SSound", "Play", "(OO)", Sf_Mod(&f, "Sound", "WaySound"), Sf_Mod(&f, "Sound", "WayVolume"));
	}
	PyObject *l = Sf_Entity(&f, item);
	int last = Sf_Compare(&f, G(WR, "currentWaypoints"), "==", G(WR, "maxWaypoints"));
	if (last > 0) {
		Sf_Method(&f, l, "Del", "()");
		Cm_ScorerCall(&f, Py_None);
		Sf_Own(&f, WR, "PreSetNextMission", "()");
	} else if (last == 0) {
		Cm_ScorerCall(&f, G(WR, "MissionScorer"));
		if (Sf_Compare(&f, G(WR, "currentWaypoints"), ">", Sf_Int(&f, 0)) > 0) {
			PyObject *t = Sf_Ask(&f, "Scrap", "Get", "(s)", "TimeSpecialUseTimer");
			t = Sf_Op(&f, t, '+', G(WR, "timeIncrement"));
			Sf_Run(&f, "Scrap", "Set", "(sO)", "TimeSpecialUseTimer", t);
		}
		PyObject *next = Sf_OpI(&f, G(WR, "iStartPos"), '+', 1);
		Sf_SetGlobal(&f, WR, "iStartPos", Sf_Op(&f, next, '%', Sf_Int(&f, Sf_Len(&f, Sf_Mod(&f, "RaceMaker", "StartPos")))));
		PyObject *sp = Sf_GetItem(&f, Sf_Mod(&f, "RaceMaker", "StartPos"), G(WR, "iStartPos"));
		Sf_SetAttr(&f, l, "Pos", Sf_Item(&f, sp, 0));
		sp = Sf_GetItem(&f, Sf_Mod(&f, "RaceMaker", "StartPos"), G(WR, "iStartPos"));
		Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, sp, 1), 0));
	}
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* the race stops: no action music, no scorer */
static void Wr_Stop(scriptFrame_t *f) {
	Sf_Run(f, "Scrap", "Set", "(si)", "isActionMusic", 0);
	Cm_ScorerCall(f, Py_None);
	Sf_DrawScorer(f);
}

/* the buoy goes, the race state back to its start */
static void Wr_Reset(scriptFrame_t *f) {
	PyObject *e = Sf_Entity(f, Sf_Global(f, WR, "waypointName"));
	if (Sf_True(f, e) > 0) {
		Sf_Method(f, e, "Del", "()");
	}
	Sf_SetGlobal(f, WR, "currentWaypoints", Sf_Int(f, -1));
	Sf_SetGlobal(f, WR, "waypointName", Sf_Str(f, ""));
	Sf_SetGlobal(f, WR, "iStartPos", Sf_Int(f, 0));
}

/* OnAlarmOn(): the police stops the race */
static PyObject *Race_OnAlarmOn(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Alarm");
	Wr_Stop(&f);
	Sf_SetMod(&f, "Alarm", "TempAbortMissionCallback", Py_None);
	Cm_Com(&f, "Mission_Win1stRace_RemoteMessage2", "Trurl", NULL, NULL);
	Wr_Reset(&f);
	return Sf_None(&f);
}

/* OnAlarmOff(): the race can start again after Trurl's message */
static PyObject *Race_OnAlarmOff(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Alarm");
	Sf_SetMod(&f, "Alarm", "TempAbortMissionCallback", G(WR, "OnAlarmOn"));
	Cm_Com(&f, "Mission_Win1stRace_RemoteMessage3", "Trurl", G(WR, "InitTimeRace"), NOARGS);
	return Sf_None(&f);
}

/* TimeExpired(): out of time: again from the start */
static PyObject *Race_TimeExpired(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Alarm");
	Wr_Stop(&f);
	Wr_Reset(&f);
	Cm_Com(&f, "Mission_Win1stRace_RemoteMessage3", "Trurl", G(WR, "InitTimeRace"), NOARGS);
	return Sf_None(&f);
}

/* PreSetNextMission(): won: no alarm hooks, Trurl's message, then the next mission */
static PyObject *Race_PreSetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Alarm");
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionMusic", 0);
	Sf_SetMod(&f, "Alarm", "TempAbortMissionCallback", Py_None);
	Sf_SetMod(&f, "Alarm", "TempRestoreMissionCallback", Py_None);
	Cm_Com(&f, "Mission_Win1stRace_RemoteMessage4", "Trurl", G(WR, "SetNextMission"), NOARGS);
	return Sf_None(&f);
}

static PyObject *Race_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Cm_Next(&f, "Mission_Win2ndRace", "Outskirts", "Mission_Win2ndRace_MissionText1");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the checkpoints passed of all, the label and the timer */
static PyObject *Race_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Sf_MissionText(&f, id, "MissionBest", "Text", Cm_X(&f, 20), Sf_Int(&f, 190), "BigNumbers", NULL, Sf_Repr(&f, G(WR, "currentWaypoints")),
		"Center", NULL);
	Sf_MissionText(&f, id, "MissionLimit", "Text", Cm_X(&f, 50), Sf_Int(&f, 190), "BigNumbers", NULL, Sf_Repr(&f, G(WR, "maxWaypoints")),
		"Center", NULL);
	Sf_MissionSprite(&f, id, "MissionYou", Cm_X(&f, 5), Sf_Int(&f, 180), 128, 0, 0, 255);
	Sf_MissionSprite(&f, id, "MissionSLimit", Cm_X(&f, 35), Sf_Int(&f, 180), 92, 0, 0, 0);
	Sf_MissionText(&f, id, "RaceText", "Text", Cm_X(&f, 0), Cm_Y(&f, 0), "Horatio", "Shadow", Cm_Label(&f, "Mission_Win1stRace_Scorer1"),
		"Left", Sf_Int(&f, 1));
	PyObject *area[2];
	Sf_Unpack(&f, Sf_Ask(&f, "SScorer", "GetTextArea", "(sO)", "Horatio", Cm_Label(&f, "Mission_DistractHumphrey_Scorer1")), 2, area);
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "RaceTimer", "Timer");
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "RaceTimer", "X", f.failed ? NULL : Sf_OpI(&f, Sf_Op(&f, Cm_X(&f, 0), '+', area[0]), '+', 30));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "RaceTimer", "Y", Cm_Y(&f, 12));
	Sf_Run(&f, "SScorer", "Set", "(Osss)", id, "RaceTimer", "Align", "Left");
	Sf_Run(&f, "SScorer", "Set", "(Osss)", id, "RaceTimer", "TimerType", "TimeSpecialUse");
	Sf_Run(&f, "SScorer", "Set", "(Osss)", id, "RaceTimer", "OnTimeExpired", "Mission_Win1stRace.TimeExpired");
	for (int i = 0; i < 4; i++) {
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "RaceTimer", rgba[i], Sf_Item(&f, Sf_Mod(&f, "Scorer", "MissionScorerText_Color"), i));
	}
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "RaceTimer", "Visible", 1);
	return Sf_None(&f);
}

PyMethodDef g_CommercialWin1stRaceMethods[] = {
	{ "InitStatus", Race_InitStatus, 1, NULL },
	{ "Init", Race_Init, 1, NULL },
	{ "InitTimeRace", Race_InitTimeRace, 1, NULL },
	{ "OnRaceStart", Race_OnRaceStart, 1, NULL },
	{ "RaceGo", Race_RaceGo, 1, NULL },
	{ "OnCheckPoint", Race_OnCheckPoint, 1, NULL },
	{ "OnAlarmOn", Race_OnAlarmOn, 1, NULL },
	{ "OnAlarmOff", Race_OnAlarmOff, 1, NULL },
	{ "TimeExpired", Race_TimeExpired, 1, NULL },
	{ "PreSetNextMission", Race_PreSetNextMission, 1, NULL },
	{ "SetNextMission", Race_SetNextMission, 1, NULL },
	{ "MissionScorer", Race_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyCommSats2: the communication probes of the level ---- */

/* InitStatus(LevelPath): the probes are the heavy traffic, none of it at the start */
static PyObject *Probes_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "Traffic");
	Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[s]", "TProbe"));
	Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	return Sf_None(&f);
}

static PyObject *Probes_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Own(&f, CS, "CreateProbes", "()");
	Cm_ScorerCall(&f, G(CS, "MissionScorer"));
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* IsValidPoint(usrPos, pos, posList): a new point far enough from the player and from the other probes */
static PyObject *Probes_IsValidPoint(PyObject *self, PyObject *args) {
	PyObject *usrPos, *pos, *list;
	SF_ARGS("OOO", &usrPos, &pos, &list);
	int in = Sf_In(&f, pos, list);
	if (in != 0) {
		return Sf_Return(&f, in > 0 ? Sf_Int(&f, 0) : NULL);
	}
	PyObject *d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", pos, usrPos));
	int near = Sf_Compare(&f, d, "<", G(CS, "minPlayerDist"));
	if (near != 0) {
		return Sf_Return(&f, near > 0 ? Sf_Int(&f, 0) : NULL);
	}
	PyObject *p;
	for (int k = 0; (p = Sf_Next(&f, list, k)) != NULL; k++) {
		d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", p, pos));
		if (Sf_Compare(&f, d, "<", G(CS, "minDistBetweenProbes")) > 0) {
			return Sf_Return(&f, Sf_Int(&f, 0));
		}
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* CreateProbes(): the probes left at random visible points */
static PyObject *Probes_CreateProbes(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "Traffic");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *list = Sf_Build(&f, "[]");
	PyObject *usrPos = Sf_Attr(&f, usr, "Pos");
	PyObject *num = Sf_ToInt(&f, SAVE("NumProbes"));
	long i = 0;
	for (;;) {
		int mark = f.n;
		if (Sf_Compare(&f, Sf_Int(&f, i), "<", num) <= 0) {
			break;
		}
		PyObject *pos = Sf_Ask(&f, "SAI", "GetRandomVisibilityPoint", "()");
		int ok = Sf_True(&f, Sf_Own(&f, CS, "IsValidPoint", "(OOO)", usrPos, pos, list));
		if (ok == 0) {
			Sf_Release(&f, mark);
			continue;
		}
		Sf_Run(&f, "Traffic", "SetPath", "()");
		PyObject *p = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ssii)", "TProbe", "ProbeMos", 0, 0);
		Sf_Run(&f, "Traffic", "ResetPath", "()");
		Sf_SetAttrS(&f, p, "OnDeath", "Mission_DestroyCommSats2.OnProbeDeath");
		Sf_SetAttr(&f, p, "MovePos", pos);
		Sf_SetAttr(&f, p, "Pos", pos);
		PyObject *ang = Sf_Build(&f, "(iii)", 0, 0, 0);
		Sf_SetAttr(&f, p, "MoveAng", ang);
		Sf_SetAttr(&f, p, "Ang", ang);
		Sf_SetAttr(&f, p, "MoveTime", Sf_Float(&f, 0.1));
		PyObject *life = G(CS, "PROBE_LIFE");
		Sf_SetAttr(&f, p, "Life", life);
		Sf_SetAttr(&f, p, "MaxLife", life);
		Sf_SetAttr(&f, p, "MinLife", life);
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", p, 1);
		Sf_Method(&f, list, "append", "(O)", pos);
		Sf_Release(&f, mark);
		i++;
	}
	return Sf_None(&f);
}

/* OnProbeDeath(LooserName, WinnerName): one probe fewer; the last one ends the mission */
static PyObject *Probes_OnProbeDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumProbes")), '-', 1);
	int last = Sf_EqI(&f, n, 0);
	if (last > 0) {
		Sf_Own(&f, CS, "DestroyEnemyShips", "()");
		Cm_ScorerCall(&f, Py_None);
		PyObject *t = Sf_OpI(&f, Sf_Time(&f), '+', 2);
		PyObject *fn = Sf_Mod(&f, "Speech", "AddRemoteCom");
		Sf_Schedule(&f, t, fn, Sf_Build(&f, "(ssOO)", "Mission_DestroyCommSats2_RemoteMessage1", "CrazyGambler", G(CS, "SetNextMission"), NOARGS),
			NULL);
	} else if (last == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumProbes", Sf_Repr(&f, n));
		Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	}
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* DestroyShipsInList(listName): every ship of the list explodes */
static PyObject *Probes_DestroyShipsInList(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *e = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "EntityListGet", "(Os)", name, "First"));
	int mark = f.n;
	while (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "EntityList", "");
		Sf_SetAttrI(&f, e, "Life", 0);
		PyObject *next = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "EntityListGet", "(Os)", name, "First"));
		Sf_ReleaseKeep(&f, mark, &next, 1);
		e = next;
	}
	return Sf_None(&f);
}

/* DestroyEnemyShips(): no more police nor traffic, the alarm falls, the ships of the lists explode one after another */
static PyObject *Probes_DestroyEnemyShips(PyObject *self, PyObject *args) {
	static const char *const lists[] = { "EnemiesTraffic", "ChasingTraffic", "Traffic", "OutCop" };
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", -1);
	Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "PoliceWaveEliminated", "");
	Sf_Run(&f, "Scrap", "Set", "(sd)", "AlarmFallDelta", -0.05);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	for (int i = 0; i < 4; i++) {
		Sf_Later(&f, Sf_Float(&f, 0.5 * i), G(CS, "DestroyShipsInList"), Sf_Build(&f, "(s)", lists[i]), NULL);
	}
	return Sf_None(&f);
}

/* SetNextMission(): the level counts as freed, on to Mission_DestroyCommSats3 */
static PyObject *Probes_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumProbes");
	PyObject *l = Sf_Eval(&f, SAVE("ComSatsMissionsMapsFinished"), CS);
	Sf_Method(&f, l, "append", "(s)", "COMMERCIAL");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "ComSatsMissionsMapsFinished", Sf_Repr(&f, l));
	Cm_Next(&f, "Mission_DestroyCommSats3", "DownTown", "Mission_DestroyCommSats3_MissionText1");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the probes left */
static PyObject *Probes_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Sf_MissionText(&f, id, "NumProbesText", "Text", Cm_X(&f, 0), Cm_Y(&f, 0), "Horatio", "Shadow",
		Cm_Label(&f, "Mission_DestroyCommSats2_Scorer1"), "Left", NULL);
	Sf_MissionText(&f, id, "NumProbes", "Text", Cm_X(&f, 0), Cm_Y(&f, 20), "Horatio", "Shadow", SAVE("NumProbes"), "Left", NULL);
	return Sf_None(&f);
}

PyMethodDef g_CommercialDestroyCommSats2Methods[] = {
	{ "InitStatus", Probes_InitStatus, 1, NULL },
	{ "Init", Probes_Init, 1, NULL },
	{ "IsValidPoint", Probes_IsValidPoint, 1, NULL },
	{ "CreateProbes", Probes_CreateProbes, 1, NULL },
	{ "OnProbeDeath", Probes_OnProbeDeath, 1, NULL },
	{ "DestroyShipsInList", Probes_DestroyShipsInList, 1, NULL },
	{ "DestroyEnemyShips", Probes_DestroyEnemyShips, 1, NULL },
	{ "SetNextMission", Probes_SetNextMission, 1, NULL },
	{ "MissionScorer", Probes_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FirstMeetDeepThroat: DeepThroat calls every minute until the player uses the phone cab ---- */

static PyObject *Deep_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, FM, ac_ships_bertos);
}

/* DeepThroat's call in 60 s: Speech.RemoteComunication(key, "DeepThroat", 1, then) under the name DeepThroatFirstMeet */
static byte Fm_Call(scriptFrame_t *f, const char *key, const char *then) {
	PyObject *t = Sf_Op(f, Sf_Time(f), '+', Sf_Float(f, 60.0));
	PyObject *fn = Sf_Mod(f, "Speech", "RemoteComunication");
	return Sf_Schedule(f, t, fn, Sf_Build(f, "(ssiO)", key, "DeepThroat", 1, Sf_Global(f, FM, then)), "DeepThroatFirstMeet");
}

/* Init(LevelPath): the boarding callbacks, the first call in a minute, the phone cab */
static PyObject *Deep_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Parking");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(FM, "RestoreDeepThroatMessages"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(FM, "CancelDeepThroatMessages"));
	Fm_Call(&f, "Mission_FirstMeetDeepThroat_RemoteMessage3", "AfterDeepThroatMessageShown");
	Sf_Own(&f, FM, "SetPhoneCab", "()");
	return Sf_None(&f);
}

static PyObject *Deep_AfterDeepThroatMessageShown(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Fm_Call(&f, "Mission_FirstMeetDeepThroat_RemoteMessage4", "AfterDeepThroatMessageShown2");
	return Sf_None(&f);
}

static PyObject *Deep_AfterDeepThroatMessageShown2(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Fm_Call(&f, "Mission_FirstMeetDeepThroat_RemoteMessage5", "AfterDeepThroatMessageShown2");
	return Sf_None(&f);
}

/* RestoreDeepThroatMessages(): back on a ship before the talk: the calls again */
static PyObject *Deep_RestoreDeepThroatMessages(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, G(FM, "talked")) == 0) {
		Fm_Call(&f, "Mission_FirstMeetDeepThroat_RemoteMessage5", "AfterDeepThroatMessageShown2");
	}
	return Sf_None(&f);
}

/* CancelDeepThroatMessages(): on foot within 100 m of the phone cab: no more calls */
static PyObject *Deep_CancelDeepThroatMessages(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Cm_And(&f, usr, G(FM, "phoneCab")) > 0) {
		PyObject *pp = Sf_Attr(&f, G(FM, "phoneCab"), "Pos");
		PyObject *d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", pp, Sf_Attr(&f, usr, "Pos")));
		if (Sf_Compare(&f, d, "<", Sf_Int(&f, 10000)) > 0) {
			Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DeepThroatFirstMeet");
		}
	}
	return Sf_None(&f);
}

static PyObject *Deep_SetPhoneCab(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Vehicles", "Speech", "Alarm", "SaveGame", "PhoneCab", "MissionsFuncs", NULL };
	SF_ARGS("");
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetGlobal(&f, FM, "phoneCab", Cm_PhoneCab(&f, "Mission_FirstMeetDeepThroat.UsePhoneCab"));
	return Sf_None(&f);
}

static PyObject *Deep_UsePhoneCab(PyObject *self, PyObject *args) {
	static const cmPhone_t p = { "Generics_StillPolice1_DeepThroat", "DeepThroat", "SetNextMission",
		"Mission_FirstMeetDeepThroat_ComDeepthroat_NI_1Msg", "DeepThroatFirstMeet", "PhoneCab.DefUseAudioCom", 0, 0, 0 };
	(void)self;
	return Cm_UsePhone(args, FM, &p);
}

/* SetNextMission(): talked; on to Mission_FicusBanker in Press, the crazy gambler calls 2 s later */
static PyObject *Deep_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Press", NULL };
	SF_ARGS("");
	Sf_SetMod(&f, "Speech", "OnOutOfRange", Py_None);
	Sf_SetGlobal(&f, FM, "talked", Sf_Int(&f, 1));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_FicusBanker", "Press");
	Cm_Places(&f, places);
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToFicus_MissionText1", 1);
	Sf_Later(&f, Sf_Int(&f, 2), G(FM, "CGCalls"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CGCalls(): the crazy gambler's message by the crazy deals done (not during the twelfth deal) */
static PyObject *Deep_CGCalls(PyObject *self, PyObject *args) {
	static const char *const msgs[] = { "Mission_TalkToFicus_RemoteMessage1", "Mission_TalkToFicus_RemoteMessage2",
		"Mission_TalkToFicus_RemoteMessage3", "Mission_TalkToFicus_RemoteMessage4" };
	SF_ARGS("");
	if (Sf_Compare(&f, SAVE("SuperDeal.Library"), "!=", Sf_Str(&f, "SuperDeal_Twelfth")) > 0) {
		PyObject *num = Sf_Ask(&f, "MissionsFuncs", "GetNumCrazyDealCompleted", "()");
		for (int i = 0; i < 4 && !f.failed; i++) {
			if (Sf_EqI(&f, num, i) > 0) {
				Cm_Com(&f, msgs[i], "CrazyGambler", NULL, NULL);
				break;
			}
		}
	}
	return Sf_None(&f);
}

PyMethodDef g_CommercialFirstMeetDeepThroatMethods[] = {
	{ "InitStatus", Deep_InitStatus, 1, NULL },
	{ "Init", Deep_Init, 1, NULL },
	{ "AfterDeepThroatMessageShown", Deep_AfterDeepThroatMessageShown, 1, NULL },
	{ "AfterDeepThroatMessageShown2", Deep_AfterDeepThroatMessageShown2, 1, NULL },
	{ "RestoreDeepThroatMessages", Deep_RestoreDeepThroatMessages, 1, NULL },
	{ "CancelDeepThroatMessages", Deep_CancelDeepThroatMessages, 1, NULL },
	{ "SetPhoneCab", Deep_SetPhoneCab, 1, NULL },
	{ "UsePhoneCab", Deep_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Deep_SetNextMission, 1, NULL },
	{ "CGCalls", Deep_CGCalls, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetDataPart3: waves of functionaries carry the data cubes ---- */

static const char *const functionaries_profile[] = { "Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=1",
	"UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10", "UsePercHook=5", "FactorHookMap=10", "CadenceShoot=2", "ControlAim=3",
	"AngAttack=2.5", "CounterAttack=0.8", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.60",
	"FactorDamageItemLife=0.15", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=10", "TimeMaxStrafe=8",
	"VelMaxStrafe=1.0", NULL };

static PyObject *Data_GetFunctionariesEnemiesProfile(PyObject *self, PyObject *args) {
	SF_ARGS("");
	return Sf_Return(&f, Lv_Profile(&f, GD, functionaries_profile));
}

static PyObject *Data_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, GD, ac_ships_bertos);
}

/* Init(LevelPath): the part of the mission of the save state: call the boss, get the data, call DeepThroat */
static PyObject *Data_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(GD, "DataCubeModel"));
	PyObject *state = SAVE("MissionState");
	if (Sf_EqS(&f, state, "CallBoss") > 0) {
		Sf_Own(&f, GD, "CallBoss", "()");
	} else if (!f.failed && Sf_EqS(&f, state, "GetData") > 0) {
		PyObject *usr = USR();
		if (Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car")) > 0) {
			Sf_Own(&f, GD, "InitMission", "()");
		}
		PyObject *n = Sf_ToInt(&f, SAVE("NumData"));
		if (Sf_Compare(&f, n, ">=", G(GD, "MAX_DATA_UNITS")) > 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumData", Sf_Repr(&f, Sf_OpI(&f, G(GD, "MAX_DATA_UNITS"), '-', 1)));
		}
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(GD, "InitMission"));
		Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(GD, "StopMission"));
		Sf_Own(&f, GD, "GetData", "()");
	} else if (!f.failed && Sf_EqS(&f, state, "CallDeepThroat") > 0) {
		Sf_Own(&f, GD, "CallDeepThroat", "()");
	}
	return Sf_None(&f);
}

/* InitMission(): the scorer, the functionary with the data as the target, the fleeing ones flee from the player */
static PyObject *Data_InitMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_ScorerCall(&f, G(GD, "MissionScorer"));
	Sf_DrawScorer(&f);
	PyObject *e = Sf_Entity(&f, G(GD, "funcWithDataName"));
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	int t = Sf_True(&f, e);
	if (t > 0) {
		t = Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car"));
	}
	if (t > 0) {
		Sf_SetAttr(&f, usr, "Target", G(GD, "funcWithDataName"));
	}
	if (!f.failed) {
		/* the script never moves name on: the state of the first in the list is asked for every one */
		PyObject *name = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Funcs", "First");
		e = Sf_Entity(&f, name);
		int mark = f.n;
		while (Sf_True(&f, e) > 0) {
			PyObject *state = Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", name);
			int flee = Sf_EqI(&f, state, 5);
			if (flee == 0) {
				flee = Sf_EqI(&f, state, 0);
			}
			if (flee > 0) {
				Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOi)", 5, name, Sf_Attr(&f, usr, "Name"), 100000);
			}
			PyObject *next = Sf_Entity(&f, Sf_Attr(&f, e, "NextInList"));
			Sf_ReleaseKeep(&f, mark, &next, 1);
			e = next;
		}
		Sf_Except(&f);
	}
	return Sf_None(&f);
}

static PyObject *Data_StopMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_ScorerCall(&f, Py_None);
	return Sf_None(&f);
}

/* CreateWave(n): up to three functionaries, the first with the data, the others its escort */
static PyObject *Data_CreateWave(PyObject *self, PyObject *args) {
	PyObject *n;
	SF_ARGS("O", &n);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, n, "<", Sf_Int(&f, 1)) > 0) {
		Sf_RaiseText(&f, "N\372mero inv\341lido de funcionarios en la oleada");
	}
	if (Sf_Compare(&f, n, ">", Sf_Int(&f, 3)) > 0) {
		n = Sf_Int(&f, 3);
	}
	PyObject *dataPos = Py_None;
	Sf_SetGlobal(&f, GD, "funcWithDataName", Sf_Str(&f, ""));
	PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(O)", n), *i;
	for (int k = 0; (i = Sf_Next(&f, range, k)) != NULL; k++) {
		int mark = f.n;
		PyObject *fn = Sf_Own(&f, GD, "CreateFunctionary", "()");
		Sf_SetAttrS(&f, fn, "EntityList", "Funcs");
		Sf_Method(&f, G(GD, "funcsInWave"), "append", "(O)", Sf_Attr(&f, fn, "Name"));
		int lead = Sf_EqI(&f, i, 0);
		if (lead > 0) {
			PyObject *pos = Sf_Attr(&f, fn, "Pos");
			Sf_SetGlobal(&f, GD, "funcWithDataName", Sf_Attr(&f, fn, "Name"));
			Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, fn, "Name"));
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, fn, "Name"), 0);
			Sf_ReleaseKeep(&f, mark, &pos, 1);
			dataPos = pos;
			continue;
		} else if (lead == 0) {
			Sf_SetAttr(&f, fn, "Pos", dataPos);
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 7, Sf_Attr(&f, fn, "Name"), G(GD, "funcWithDataName"));
		}
		Sf_Release(&f, mark);
	}
	PyObject *e = Sf_Entity(&f, G(GD, "funcWithDataName"));
	int t = Sf_True(&f, e);
	if (t > 0) {
		t = Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car"));
	}
	if (t > 0) {
		Sf_SetAttr(&f, usr, "Target", G(GD, "funcWithDataName"));
	}
	return Sf_None(&f);
}

/* CreateFunctionary(): a functionary fighter at a free spawn point */
static PyObject *Data_CreateFunctionary(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "Traffic");
	PyObject *spw;
	PyObject *o1 = Lv_SpawnCarAt(&f, "FuncFighter", 0, &spw);
	Sf_SetAttrI(&f, o1, "Money", -1);
	Sf_SetAttrS(&f, o1, "TgType", "red");
	Sf_SetAttr(&f, o1, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, spw, 1), 0));
	Lv_StdProfile(&f, GD, "GetFunctionariesEnemiesProfile", o1);
	Lv_Life(&f, o1, GD, "FUNC_LIFE");
	return Sf_Return(&f, o1);
}

/* ReinsertFunctionaryInWave(): a new escort; it fetches dropped data nobody goes for */
static PyObject *Data_ReinsertFunctionaryInWave(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	int withData = 0, toData = 0;
	PyObject *dataName = Sf_Str(&f, ""), *name;
	for (int k = 0; (name = Sf_Next(&f, G(GD, "funcsInWave"), k)) != NULL; k++) {
		PyObject *e = Sf_Entity(&f, name);
		if (Sf_True(&f, e) > 0) {
			if (Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 1) > 0) {
				withData = 1;
				dataName = name;
			} else if (!f.failed && Sf_EqI(&f, Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", name), 5) > 0) {
				toData = 1;
			}
		}
	}
	PyObject *fn = Sf_Own(&f, GD, "CreateFunctionary", "()");
	Sf_SetAttrS(&f, fn, "EntityList", "Enemies");
	Sf_Method(&f, G(GD, "funcsInWave"), "append", "(O)", Sf_Attr(&f, fn, "Name"));
	if (f.failed) {
		return Sf_None(&f);
	}
	if (withData) {
		if (Sf_EqI(&f, Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", dataName), 2) > 0) {
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOi)", 5, dataName, Sf_Attr(&f, usr, "Name"), 100000);
		}
	} else {
		int go = Sf_True(&f, G(GD, "droppedDataPos"));
		if (go > 0) {
			go = !toData;
		}
		if (go > 0) {
			Sf_SetAttrS(&f, fn, "EntityList", "Funcs");
			PyObject *p = G(GD, "droppedDataPos");
			PyObject *x = Sf_Item(&f, p, 0), *y = Sf_Item(&f, G(GD, "droppedDataPos"), 1), *z = Sf_Item(&f, G(GD, "droppedDataPos"), 2);
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOOOOi)", 1, Sf_Attr(&f, fn, "Name"), x, y, z, G(GD, "DataCubeRadius"), 1);
		}
	}
	return Sf_None(&f);
}

/* the escort funcsInWave[0] goes for the dropped data */
static void Gd_Fetch(scriptFrame_t *f) {
	PyObject *e = Sf_Entity(f, Sf_Item(f, Sf_Global(f, GD, "funcsInWave"), 0));
	if (Sf_True(f, e) > 0) {
		Sf_SetAttrS(f, e, "EntityList", "Funcs");
		PyObject *n = Sf_Item(f, Sf_Global(f, GD, "funcsInWave"), 0);
		PyObject *x = Sf_Item(f, Sf_Global(f, GD, "droppedDataPos"), 0), *y = Sf_Item(f, Sf_Global(f, GD, "droppedDataPos"), 1);
		PyObject *z = Sf_Item(f, Sf_Global(f, GD, "droppedDataPos"), 2);
		Sf_Run(f, "SAI", "SetStateVehicle", "(iOOOOOi)", 1, n, x, y, z, Sf_Global(f, GD, "DataCubeRadius"), 1);
	}
}

/* OnFunctionaryDeath(LooserName, WinnerName): the one with the data drops it; an escort comes back later */
static PyObject *Data_OnFunctionaryDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	if (Sf_In(&f, looser, G(GD, "funcsInWave")) > 0) {
		Sf_Method(&f, G(GD, "funcsInWave"), "remove", "(O)", looser);
		PyObject *e = Sf_Entity(&f, looser);
		if (Sf_True(&f, e) > 0) {
			int len = Sf_Len(&f, G(GD, "funcsInWave"));
			int data = Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 1);
			if (data > 0) {
				Sf_SetGlobal(&f, GD, "droppedDataPos", Sf_Attr(&f, e, "Pos"));
				Sf_Own(&f, GD, "DropData", "(O)", G(GD, "droppedDataPos"));
				if (len > 0) {
					Gd_Fetch(&f);
				}
			} else if (data == 0) {
				Sf_Later(&f, G(GD, "timeToReinsertEnemy"), G(GD, "ReinsertFunctionaryInWave"), NOARGS, "ReinsertFunctionaryInWave");
				if (len == 1) {
					e = Sf_Entity(&f, Sf_Item(&f, G(GD, "funcsInWave"), 0));
					PyObject *usr = USR();
					int alone = Cm_And(&f, usr, e);
					if (alone > 0) {
						alone = Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 1);
					}
					if (alone > 0) {
						Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Item(&f, G(GD, "funcsInWave"), 0), Sf_Attr(&f, usr, "Name"));
					}
				}
			}
		}
	}
	return Sf_None(&f);
}

/* the functionaries of the wave (but skip) attack the player; withData: only those without the data */
static void Gd_Attack(scriptFrame_t *f, PyObject *skip, int onlyEscort) {
	PyObject *name;
	for (int k = 0; (name = Sf_Next(f, Sf_Global(f, GD, "funcsInWave"), k)) != NULL; k++) {
		int mark = f->n;
		int go = skip ? Sf_Compare(f, name, "!=", skip) : 1;
		if (go > 0) {
			PyObject *fn = Sf_Entity(f, name);
			go = Sf_True(f, fn);
			if (go > 0 && onlyEscort) {
				go = Sf_EqI(f, Sf_Attr(f, fn, "RadarShow"), 0);
			}
			if (go > 0) {
				Sf_SetAttrS(f, fn, "EntityList", "Enemies");
			}
		}
		Sf_Release(f, mark);
	}
}

/* OnFunctionaryDamage(LooserName, WinnerName, Damage): the first hit: the one with the data attacks (first wave) or
 * flees with the escort attacking, a hit escort makes the escort attack */
static PyObject *Data_OnFunctionaryDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *e = Sf_Entity(&f, looser);
	if (Sf_True(&f, e) != 1) {
		return Sf_None(&f);
	}
	PyObject *st = Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", looser);
	int other = Sf_Compare(&f, st, "!=", Sf_Int(&f, 3));
	if (other > 0) {
		other = Sf_Compare(&f, st, "!=", Sf_Int(&f, 7));
	}
	if (other != 0) {
		return Sf_None(&f);
	}
	int data = Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 1);
	if (data > 0) {
		int first = Sf_EqI(&f, Sf_ToInt(&f, SAVE("NumData")), 0);
		if (first > 0) {
			Sf_SetAttrS(&f, e, "EntityList", "Enemies");
		} else if (first == 0) {
			PyObject *usr = USR();
			if (Sf_True(&f, usr) > 0) {
				Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOi)", 5, looser, Sf_Attr(&f, usr, "Name"), 100000);
			}
			Gd_Attack(&f, looser, 0);
		}
	} else if (data == 0) {
		Gd_Attack(&f, NULL, 1);
	}
	return Sf_None(&f);
}

static PyObject *Data_DropData(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	Cm_Drop(&f, GD, pos, "DroppedData", "DataCubeModel", "DataCubeRadius", 0, "Mission_GetDataPart3.TakeData", "TakeDataCube", NULL);
	return Sf_None(&f);
}

/* CheckAITakesData(name): checked a second later, in case it runs before TakeData */
static PyObject *Data_CheckAITakesData(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	Sf_Later(&f, Sf_Int(&f, 1), G(GD, "CheckAI"), Sf_Build(&f, "(O)", name), NULL);
	return Sf_None(&f);
}

/* CheckAI(name): an escort that reached the data without taking it attacks the player */
static PyObject *Data_CheckAI(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *e = Sf_Entity(&f, name);
	int go = Sf_True(&f, e);
	if (go > 0) {
		go = Sf_EqS(&f, Sf_Attr(&f, e, "EntityList"), "Funcs");
	}
	if (go > 0) {
		go = Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 0);
	}
	if (go > 0) {
		PyObject *usr = USR();
		int t = Sf_True(&f, usr);
		if (t == 0) {
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, e, "Name"), 1);
		} else if (t > 0) {
			Sf_SetAttrS(&f, e, "EntityList", "Enemies");
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, e, "Name"), Sf_Attr(&f, usr, "Name"));
		}
	}
	return Sf_None(&f);
}

/* the functionaries of the wave join list with state (2 attacks the player, 3 wanders), then the wave is empty */
static void Gd_EndWave(scriptFrame_t *f, const char *list, int state, PyObject *usr) {
	PyObject *name;
	for (int k = 0; (name = Sf_Next(f, Sf_Global(f, GD, "funcsInWave"), k)) != NULL; k++) {
		int mark = f->n;
		PyObject *e = Sf_Entity(f, name);
		if (Sf_True(f, e) > 0) {
			Sf_SetAttrS(f, e, "EntityList", list);
			if (state == 3) {
				Sf_Run(f, "SAI", "SetStateVehicle", "(iOi)", 3, name, 1);
			} else {
				Sf_Run(f, "SAI", "SetStateVehicle", "(iOO)", 2, name, Sf_Attr(f, usr, "Name"));
			}
		}
		Sf_Release(f, mark);
	}
	Sf_DelGlobal(f, GD, "funcsInWave");
	Sf_SetGlobal(f, GD, "funcsInWave", Sf_Build(f, "[]"));
}

/* TakeData(ItemName, CharName): the player takes a cube (the next wave, or all eight), or an escort takes it and flees */
static PyObject *Data_TakeData(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	int player = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", ch);
	if (player > 0) {
		Sf_SetGlobal(&f, GD, "droppedDataPos", Py_None);
		Cm_Taken(&f, item, Sf_Cat(&f, "Os", item, "CheckArrow"));
		PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumData")), '+', 1);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumData", Sf_Repr(&f, n));
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "ReinsertFunctionaryInWave");
		int all = Sf_Compare(&f, n, ">=", G(GD, "MAX_DATA_UNITS"));
		if (all > 0) {
			Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
			Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
			Cm_ScorerCall(&f, Py_None);
			Sf_DrawScorer(&f);
			Sf_Run(&f, "Scrap", "Set", "(ss)", "OnItemTakenAI", "");
			Cm_Com(&f, "Mission_GetDataPart3_RemoteMessage1", "Boss", G(GD, "PreCallDeepThroat"), NOARGS);
			Gd_EndWave(&f, "Traffic", 3, usr);
		} else if (all == 0) {
			Cm_Com(&f, "Mission_GetDataPart3_RemoteMessage3", "Boss", NULL, NULL);
			Cm_Set0(&f, "DataStatus", "Value", n);
			Sf_DrawScorer(&f);
			Gd_EndWave(&f, "Enemies", 2, usr);
			Sf_Own(&f, GD, "CreateWave", "(O)", Sf_OpI(&f, n, '+', 1));
		}
	} else if (player == 0 && Sf_In(&f, ch, G(GD, "funcsInWave")) > 0) {
		Sf_SetGlobal(&f, GD, "droppedDataPos", Py_None);
		Sf_Method(&f, Sf_Entity(&f, item), "Del", "()");
		Sf_Method(&f, Sf_Entity(&f, Sf_Cat(&f, "Os", item, "CheckArrow")), "Del", "()");
		PyObject *e = Sf_Entity(&f, ch);
		if (Sf_True(&f, e) > 0) {
			Sf_SetGlobal(&f, GD, "funcWithDataName", ch);
			Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", ch);
			Sf_SetAttrS(&f, e, "EntityList", "Funcs");
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOi)", 5, ch, Sf_Attr(&f, usr, "Name"), 100000);
		}
	}
	return Sf_None(&f);
}

/* CallBoss(), CallDeepThroat(): a free phone cab (the global pc) for the call */
static PyObject *Data_CallBoss(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, GD, "pc", Cm_PhoneCab(&f, "Mission_GetDataPart3.UsePhoneCab"));
	return Sf_None(&f);
}

static PyObject *Data_UsePhoneCab(PyObject *self, PyObject *args) {
	static const cmPhone_t p = { "Generics_StillPolice1_Boss", "Boss", "PreGetData", "Mission_TalkToBoss3_ComBoss_NI_1Msg", NULL, NULL, 0, 1, 0 };
	(void)self;
	return Cm_UsePhone(args, GD, &p);
}

/* PreGetData(): after the boss: the boarding callbacks, the save state, the first wave */
static PyObject *Data_PreGetData(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(GD, "InitMission"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(GD, "StopMission"));
	Sf_SetAttrS(&f, G(GD, "pc"), "UseFunc", "PhoneCab.DefUseAudioCom");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "GetData");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetDataPart3_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Own(&f, GD, "GetData", "()");
	Sf_Own(&f, GD, "InitMission", "()");
	return Sf_None(&f);
}

/* GetData(): the lists of the functionaries with their callbacks, the wave of the data count */
static PyObject *Data_GetData(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "CreateEntityList", "(s)", "Funcs");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Funcs", "OnDeath", "Mission_GetDataPart3.OnFunctionaryDeath");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Funcs", "OnDamage", "Mission_GetDataPart3.OnFunctionaryDamage");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_GetDataPart3.OnFunctionaryDeath");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDamage", "Mission_GetDataPart3.OnFunctionaryDamage");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnItemTakenAI", "Mission_GetDataPart3.CheckAITakesData");
	Sf_Own(&f, GD, "CreateWave", "(O)", Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumData")), '+', 1));
	return Sf_None(&f);
}

static PyObject *Data_PreCallDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_Com(&f, "Mission_GetDataPart3_RemoteMessage2", "DeepThroat", G(GD, "PreCallDeepThroat2"), NOARGS);
	return Sf_None(&f);
}

static PyObject *Data_PreCallDeepThroat2(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "CallDeepThroat");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToDeepThroat8_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Own(&f, GD, "CallDeepThroat", "()");
	return Sf_None(&f);
}

static PyObject *Data_CallDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, GD, "pc", Cm_PhoneCab(&f, "Mission_GetDataPart3.UsePhoneCab2"));
	return Sf_None(&f);
}

static PyObject *Data_UsePhoneCab2(PyObject *self, PyObject *args) {
	static const cmPhone_t p = { "Generics_StillPolice1_DeepThroat", "DeepThroat", "SetNextMission",
		"Mission_TalkToDeepThroat8_ComDeepThroat_NI_1Msg", NULL, NULL, 1, 1, 0 };
	(void)self;
	return Cm_UsePhone(args, GD, &p);
}

/* SetNextMission(): on to Mission_DistractHumphrey in DownTown */
static PyObject *Data_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut", "NoPlace", "Journalist.Betty_Defaut",
		"NoPlace", "Journalist.Berto_Defaut", "Press", NULL };
	SF_ARGS("");
	Sf_SetAttrS(&f, G(GD, "pc"), "UseFunc", "PhoneCab.DefUseAudioCom");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumData");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Cm_Next(&f, "Mission_DistractHumphrey", "DownTown", "Mission_TalkToHumphrey_MissionText1");
	Cm_Places(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the data cubes got */
static PyObject *Data_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_MissionText(&f, id, "DataStatusText", "Text", Cm_X(&f, 0), Cm_Y(&f, 0), "Horatio", "Shadow", Cm_Label(&f, "Mission_GetDataPart3_Scorer_1"),
		"Left", NULL);
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "DataStatus", "Bar");
	Lv_BarDefaults(&f, id, "DataStatus", NULL, NULL);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "DataStatus", "IsAutoUpdatable", 0);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "DataStatus", "MaxValue", G(GD, "MAX_DATA_UNITS"));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "DataStatus", "Value", Sf_ToInt(&f, SAVE("NumData")));
	return Sf_None(&f);
}

PyMethodDef g_CommercialGetDataPart3Methods[] = {
	{ "GetFunctionariesEnemiesProfile", Data_GetFunctionariesEnemiesProfile, 1, NULL },
	{ "InitStatus", Data_InitStatus, 1, NULL },
	{ "Init", Data_Init, 1, NULL },
	{ "InitMission", Data_InitMission, 1, NULL },
	{ "StopMission", Data_StopMission, 1, NULL },
	{ "CreateWave", Data_CreateWave, 1, NULL },
	{ "CreateFunctionary", Data_CreateFunctionary, 1, NULL },
	{ "ReinsertFunctionaryInWave", Data_ReinsertFunctionaryInWave, 1, NULL },
	{ "OnFunctionaryDeath", Data_OnFunctionaryDeath, 1, NULL },
	{ "OnFunctionaryDamage", Data_OnFunctionaryDamage, 1, NULL },
	{ "DropData", Data_DropData, 1, NULL },
	{ "CheckAITakesData", Data_CheckAITakesData, 1, NULL },
	{ "CheckAI", Data_CheckAI, 1, NULL },
	{ "TakeData", Data_TakeData, 1, NULL },
	{ "CallBoss", Data_CallBoss, 1, NULL },
	{ "UsePhoneCab", Data_UsePhoneCab, 1, NULL },
	{ "PreGetData", Data_PreGetData, 1, NULL },
	{ "GetData", Data_GetData, 1, NULL },
	{ "PreCallDeepThroat", Data_PreCallDeepThroat, 1, NULL },
	{ "PreCallDeepThroat2", Data_PreCallDeepThroat2, 1, NULL },
	{ "CallDeepThroat", Data_CallDeepThroat, 1, NULL },
	{ "UsePhoneCab2", Data_UsePhoneCab2, 1, NULL },
	{ "SetNextMission", Data_SetNextMission, 1, NULL },
	{ "MissionScorer", Data_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DefeatCrazyGambler3: the crazy gambler's copies, then his matrix ---- */

static const char *const crazy_gambler_profile[] = { "Ship=SBANKMASTER1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=2",
	"UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25", "UsePercHook=20", "FactorHookMap=10", "CadenceShoot=5", "ControlAim=3",
	"AngAttack=1.7", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.25",
	"FactorDamageItemLife=0.35", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=30", "TimeMaxStrafe=10",
	"VelMaxStrafe=1.0", NULL };

static PyObject *Gambler_GetCrazyGamblerProfile(PyObject *self, PyObject *args) {
	SF_ARGS("");
	return Sf_Return(&f, Lv_Profile(&f, CG, crazy_gambler_profile));
}

static PyObject *Gambler_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, CG, ac_ships_bertos);
}

/* Init(LevelPath): the evil gambler's head, his copies (or the matrix when all are dead), the taunts */
static PyObject *Gambler_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "EvilGambler", "Models/Heads/HeadEvilGambler/HeadEvilGambler", "", 4, 20);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(CG, "BossMatrixModel"));
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_DefeatCrazyGambler3.OnCrazyGamblerDeath");
	PyObject *lifes = SAVE("NumCrazyGamblerLifes");
	if (Sf_EqS(&f, lifes, "6") > 0) {
		Cm_Com(&f, "Mission_DefeatCrazyGambler3_RemoteMessage1", "DeepThroat", NULL, NULL);
	}
	int dead = Sf_EqS(&f, lifes, "0");
	if (dead > 0) {
		PyObject *pos = Sf_Eval(&f, SAVE("BossMatrixPos"), CG);
		if (Sf_True(&f, pos) > 0) {
			Sf_Own(&f, CG, "DropBossMatrix", "(O)", pos);
		}
	} else if (dead == 0) {
		Sf_Own(&f, CG, "CreateCrazyGamblers", "()");
		Cm_ScorerCall(&f, G(CG, "MissionScorer"));
	}
	Sf_Later(&f, Sf_Int(&f, 3), G(CG, "LaunchFirstTaunt"), NOARGS, NULL);
	return Sf_None(&f);
}

static PyObject *Gambler_LaunchFirstTaunt(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_EqS(&f, SAVE("NumCrazyGamblerLifes"), "6") > 0) {
		Cm_Com(&f, "Mission_DefeatCrazyGambler3_RemoteMessage2", "EvilGambler", NULL, NULL);
	}
	Sf_Later(&f, Sf_Int(&f, 15), G(CG, "LaunchTaunts"), NOARGS, NULL);
	return Sf_None(&f);
}

/* LaunchTaunts(): the four taunts in turn every 12 s while copies live */
static PyObject *Gambler_LaunchTaunts(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *lifes = SAVE("NumCrazyGamblerLifes");
	int go = Sf_True(&f, lifes);
	if (go > 0) {
		go = Sf_Compare(&f, Sf_ToInt(&f, lifes), ">", Sf_Int(&f, 0));
	}
	if (go > 0) {
		PyObject *n = Sf_Op(&f, Sf_Int(&f, 3), '+', G(CG, "numTaunt"));
		Sf_Run(&f, "Speech", "RemoteComunication", "(Os)", Sf_Cat(&f, "sO", "Mission_DefeatCrazyGambler3_RemoteMessage", Sf_Repr(&f, n)),
			"EvilGambler");
		Sf_SetGlobal(&f, CG, "numTaunt", Sf_OpI(&f, Sf_OpI(&f, G(CG, "numTaunt"), '+', 1), '%', 4));
		Sf_Later(&f, Sf_Int(&f, 12), G(CG, "LaunchTaunts"), NOARGS, NULL);
	}
	return Sf_None(&f);
}

static PyObject *Gambler_RefreshScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	PyObject *fn = Sf_Mod(&f, "Menu", "DrawScorer");
	Sf_Call(&f, fn, "(OO)", id, Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	return Sf_None(&f);
}

/* CreateCrazyGambler(): one copy of the gambler at a free spawn point */
static PyObject *Gambler_CreateCrazyGambler(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *h = Lv_SpawnCar(&f, "CrazyGambler", 0);
	Lv_StdProfile(&f, CG, "GetCrazyGamblerProfile", h);
	Sf_SetAttrI(&f, h, "LaunchTaunts", 0);
	Sf_SetAttrI(&f, h, "Money", -1);
	Sf_SetAttr(&f, h, "MPil0", G(CG, "pilotName"));
	Sf_SetAttrS(&f, h, "TgType", "red");
	Sf_SetAttrS(&f, h, "EntityList", "Enemies");
	Lv_Life(&f, h, CG, "GAMBLER_LIFE");
	Sf_Method(&f, G(CG, "crazyGamblers"), "append", "(O)", Sf_Attr(&f, h, "Name"));
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", h, 1);
	return Sf_None(&f);
}

/* CreateCrazyGamblers(): a copy for each life but the last, 0.2 s apart, then the scorer */
static PyObject *Gambler_CreateCrazyGamblers(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Method(&f, G(CG, "crazyGamblers"), "append", "(O)", Py_None);
	PyObject *n = Sf_ToInt(&f, SAVE("NumCrazyGamblerLifes"));
	int one = Sf_EqI(&f, n, 1);
	if (one > 0) {
		Sf_Own(&f, CG, "CreateCrazyGambler", "()");
	} else if (one == 0) {
		PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(O)", Sf_OpI(&f, n, '-', 1)), *i;
		for (int k = 0; (i = Sf_Next(&f, range, k)) != NULL; k++) {
			int mark = f.n;
			Sf_Later(&f, Sf_Op(&f, i, '*', Sf_Float(&f, 0.2)), G(CG, "CreateCrazyGambler"), NOARGS, NULL);
			Sf_Release(&f, mark);
		}
	}
	Sf_Later(&f, Sf_Op(&f, Sf_OpI(&f, n, '-', 1), '*', Sf_Float(&f, 0.2)), G(CG, "RefreshScorer"), Sf_Build(&f, "(i)", 0), NULL);
	return Sf_None(&f);
}

/* OnCrazyGamblerDeath(LooserName, WinnerName): a life less; the last drops the matrix, one left: DeepThroat */
static PyObject *Gambler_OnCrazyGamblerDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	if (Sf_In(&f, looser, G(CG, "crazyGamblers")) > 0) {
		Sf_Method(&f, G(CG, "crazyGamblers"), "remove", "(O)", looser);
		PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumCrazyGamblerLifes")), '-', 1);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumCrazyGamblerLifes", Sf_Repr(&f, n));
		if (Sf_EqI(&f, n, 0) > 0) {
			Sf_Run(&f, "SScorer", "CancelSpeech", "()");
			Cm_ScorerCall(&f, Py_None);
			PyObject *e = Sf_Entity(&f, looser);
			Sf_SetAttrI(&f, e, "Money", -1);
			Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "BossMatrixPos", Sf_Repr(&f, Sf_Attr(&f, e, "Pos")));
			Sf_Own(&f, CG, "DropBossMatrix", "(O)", Sf_Attr(&f, e, "Pos"));
		} else if (!f.failed && Sf_EqI(&f, n, 1) > 0) {
			PyObject *fn = Sf_Mod(&f, "Speech", "ForceRemoteCom");
			PyObject *kw = Sf_Build(&f, "{s:O,s:O}", "AfterRemoteComFunc", G(CG, "AfterDeepThroatMessage"), "Parameters", NOARGS);
			Sf_CallKw(&f, fn, f.failed ? NULL : Py_BuildValue("(ss)", "Mission_DefeatCrazyGambler3_RemoteMessage7", "DeepThroat"), kw);
		}
	}
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* AfterDeepThroatMessage(): the last copy */
static PyObject *Gambler_AfterDeepThroatMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_DefeatCrazyGambler4_MissionText1", 0);
	Sf_SetGlobal(&f, CG, "crazyGamblers", Sf_Build(&f, "[]"));
	Sf_Own(&f, CG, "CreateCrazyGambler", "()");
	return Sf_None(&f);
}

static PyObject *Gambler_DropBossMatrix(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	Cm_Drop(&f, CG, pos, "BossMatrix", "BossMatrixModel", "BossMatrixRadius", 1, "Mission_DefeatCrazyGambler3.TakeMatrix", "TakeMatrixExt", NULL);
	return Sf_None(&f);
}

/* TakeMatrix(ItemName, CharName): DeepThroat, then the next mission */
static PyObject *Gambler_TakeMatrix(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	Cm_Taken(&f, item, Sf_Cat(&f, "Os", item, "CheckArrow"));
	Cm_Com(&f, "Mission_DefeatCrazyGambler4_RemoteMessage1", "DeepThroat", G(CG, "SetNextMission"), NOARGS);
	return Sf_None(&f);
}

/* SetNextMission(): on to Mission_WhoIsDeepThroat in Press */
static PyObject *Gambler_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "Press",
		"Journalist.Berto_Defaut", "Press", NULL };
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumCrazyGamblerLifes");
	Cm_Next(&f, "Mission_WhoIsDeepThroat", "Press", "Mission_WhoIsDeepThroat_MissionText1");
	Cm_Places(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the lives left */
static PyObject *Gambler_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Sf_MissionText(&f, id, "RemainingLifesText", "Text", Cm_X(&f, 0), Cm_Y(&f, 0), "Horatio", "Shadow",
		Cm_Label(&f, "Mission_DefeatCrazyGambler2_Scorer1"), "Left", NULL);
	Sf_MissionText(&f, id, "RemainingLifes", "Text", Cm_X(&f, 0), Cm_Y(&f, 20), "Horatio", "Shadow", SAVE("NumCrazyGamblerLifes"), "Left",
		NULL);
	return Sf_None(&f);
}

PyMethodDef g_CommercialDefeatCrazyGambler3Methods[] = {
	{ "GetCrazyGamblerProfile", Gambler_GetCrazyGamblerProfile, 1, NULL },
	{ "InitStatus", Gambler_InitStatus, 1, NULL },
	{ "Init", Gambler_Init, 1, NULL },
	{ "LaunchFirstTaunt", Gambler_LaunchFirstTaunt, 1, NULL },
	{ "LaunchTaunts", Gambler_LaunchTaunts, 1, NULL },
	{ "RefreshScorer", Gambler_RefreshScorer, 1, NULL },
	{ "CreateCrazyGambler", Gambler_CreateCrazyGambler, 1, NULL },
	{ "CreateCrazyGamblers", Gambler_CreateCrazyGamblers, 1, NULL },
	{ "OnCrazyGamblerDeath", Gambler_OnCrazyGamblerDeath, 1, NULL },
	{ "AfterDeepThroatMessage", Gambler_AfterDeepThroatMessage, 1, NULL },
	{ "DropBossMatrix", Gambler_DropBossMatrix, 1, NULL },
	{ "TakeMatrix", Gambler_TakeMatrix, 1, NULL },
	{ "SetNextMission", Gambler_SetNextMission, 1, NULL },
	{ "MissionScorer", Gambler_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyPorkinsTransports: the mayor's campaign against bishop Porkins ---- */

static const char *const bishops_profile[] = { "Ship=SBishop1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0",
	"CadenceShoot=5", "ControlAim=4", "AngAttack=3.14", "CounterAttack=0.50", "ControlBoost=1", "ControlBoostItemLife=1",
	"ControlBoostItemAmmo=1", "FactorDamageGetAway=0.7", "FactorDamageItemLife=0.25", "FactorItemAmmo=0.25", "TimeMaxStrafe=10",
	"VelMaxStrafe=0.75", NULL };

static PyObject *Porkins_GetBishopsEnemiesProfile(PyObject *self, PyObject *args) {
	SF_ARGS("");
	return Sf_Return(&f, Lv_Profile(&f, PT, bishops_profile));
}

/* InitStatus(LevelPath): half the traffic, the vote transports as heavy traffic, no traffic until the end */
static PyObject *Porkins_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Traffic");
	Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[ss]", "TVoteMayor", "TVoteBishop"));
	Sf_SetMod(&f, "Traffic", "NumNormal", Sf_OpI(&f, Sf_Mod(&f, "Traffic", "NumNormal"), '/', 2));
	Sf_SetMod(&f, "Traffic", "NumMedium", Sf_OpI(&f, Sf_Mod(&f, "Traffic", "NumMedium"), '/', 2));
	Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, PT, "currentTrafficNumber", Sf_Ask(&f, "Scrap", "Get", "(s)", "iTrafficNumber"));
	Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
	if (Sf_EqS(&f, SAVE("MissionState"), "Popularity") > 0) {
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(PT, "InitMission"));
	}
	Lv_AcShips(&f, PT, ac_ships_bertos);
	return Sf_None(&f);
}

static PyObject *Porkins_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	int pop = Sf_EqS(&f, SAVE("MissionState"), "Popularity");
	if (pop > 0) {
		Cm_Com(&f, "Mission_Campaign2_RemoteMessage1", "Mayor", NULL, NULL);
	} else if (pop == 0) {
		Sf_Own(&f, PT, "CallDeepThroat", "()");
	}
	return Sf_None(&f);
}

/* InitMission(): the scorer, five transports of each side, two bishops */
static PyObject *Porkins_InitMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_Com(&f, "Mission_Campaign2_RemoteMessage2", "Functionary", NULL, NULL);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	Cm_ScorerCall(&f, G(PT, "MissionScorer"));
	Sf_DrawScorer(&f);
	for (int i = 0; i < 5; i++) {
		Sf_Own(&f, PT, "CreateMayorTransport", "()");
	}
	for (int i = 0; i < 5; i++) {
		Sf_Own(&f, PT, "CreatePorkinsTransport", "()");
	}
	for (int i = 0; i < 2; i++) {
		Sf_Own(&f, PT, "CreateBishop", "(i)", 0);
	}
	Cm_Com(&f, "Mission_Campaign2_RemoteMessage6", "Bishop", NULL, NULL);
	return Sf_None(&f);
}

/* a vote transport on the traffic paths with its life, death callback and colour */
static PyObject *Pt_Transport(scriptFrame_t *f, const char *model, const char *life, const char *onDeath, const char *tg) {
	Sf_Import(f, "Traffic");
	Sf_Import(f, "VehiclesNPC");
	Sf_Run(f, "Traffic", "SetPath", "()");
	PyObject *p = Sf_Ask(f, "VehiclesNPC", "MakeAI", "(ssii)", model, "Traffic", 1, 0);
	Sf_Run(f, "Traffic", "ResetPath", "()");
	Sf_Run(f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(f, p, "Name"), 0);
	Lv_Life(f, p, PT, life);
	Sf_SetAttrS(f, p, "OnDeath", onDeath);
	Sf_SetAttrS(f, p, "TgType", tg);
	Sf_SetAttrI(f, p, "FixedModel", 1);
	return p;
}

static PyObject *Porkins_CreateMayorTransport(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *p = Pt_Transport(&f, "TVoteMayor", "MAYOR_TRANSPORT_LIFE", "Mission_DestroyPorkinsTransports.OnMayorTransportDeath", "green");
	Sf_Method(&f, G(PT, "mayorTransports"), "append", "(O)", Sf_Attr(&f, p, "Name"));
	return Sf_None(&f);
}

static PyObject *Porkins_CreatePorkinsTransport(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *p = Pt_Transport(&f, "TVoteBishop", "BISHOP_TRANSPORT_LIFE", "Mission_DestroyPorkinsTransports.OnPorkinsTransportDeath", "red");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", p, 1);
	Sf_Method(&f, G(PT, "bishopTransports"), "append", "(O)", Sf_Attr(&f, p, "Name"));
	return Sf_None(&f);
}

/* CreateBishop(launchTaunt=1): a bishop that attacks the nearest transport of the mayor; every other one taunts */
static PyObject *Porkins_CreateBishop(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "launchTaunt", NULL };
	PyObject *taunt = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &taunt)) {
		return NULL;
	}
	if (!taunt) {
		taunt = Sf_Int(&f, 1);
	}
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	if (Sf_Len(&f, G(PT, "mayorTransports")) == 0) {
		Sf_RaiseText(&f, "Se han de crear primero los transportes del alcalde");
	}
	PyObject *p = Lv_SpawnCar(&f, "Bishop", 0);
	Sf_SetAttrS(&f, p, "TgType", "red");
	Lv_StdProfile(&f, PT, "GetBishopsEnemiesProfile", p);
	Lv_Life(&f, p, PT, "BISHOPS_LIFE");
	Sf_Own(&f, PT, "AttackMayorTransport", "(O)", Sf_Attr(&f, p, "Name"));
	Sf_Method(&f, G(PT, "bishops"), "append", "(O)", Sf_Attr(&f, p, "Name"));
	if (Sf_True(&f, taunt) > 0) {
		int show = Sf_True(&f, G(PT, "showBishopTaunt"));
		if (show > 0) {
			Sf_SetGlobal(&f, PT, "showBishopTaunt", Sf_Int(&f, 0));
			Cm_Com(&f, "Mission_Campaign2_RemoteMessage6", "Bishop", NULL, NULL);
		} else if (show == 0) {
			Sf_SetGlobal(&f, PT, "showBishopTaunt", Sf_Int(&f, 1));
		}
	}
	return Sf_None(&f);
}

/* AttackMayorTransport(bishopName): the bishop's callbacks, it attacks the nearest transport of the mayor */
static PyObject *Porkins_AttackMayorTransport(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *bishop = Sf_Entity(&f, name);
	if (Sf_True(&f, bishop) > 0) {
		Sf_SetAttrS(&f, bishop, "OnDeath", "Mission_DestroyPorkinsTransports.OnBishopDeath");
		Sf_SetAttrS(&f, bishop, "OnDamage", "Mission_DestroyPorkinsTransports.OnBishopDamage");
		Sf_SetAttrS(&f, bishop, "EntityList", "");
		PyObject *target = Sf_Str(&f, ""), *minDist = Sf_Int(&f, 5000000), *t;
		for (int k = 0; (t = Sf_Next(&f, G(PT, "mayorTransports"), k)) != NULL; k++) {
			PyObject *e = Sf_Entity(&f, t);
			if (Sf_True(&f, e) > 0) {
				PyObject *bp = Sf_Attr(&f, bishop, "Pos");
				PyObject *d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", bp, Sf_Attr(&f, e, "Pos")));
				if (Sf_Compare(&f, d, "<", minDist) > 0) {
					minDist = d;
					target = t;
				}
			}
		}
		if (Sf_Compare(&f, target, "!=", Sf_Str(&f, "")) > 0) {
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, bishop, "Name"), target);
		}
	}
	return Sf_None(&f);
}

/* the ships of a list of names (a global) leave the mission: no target colour, deaths as normal traffic */
static void Pt_Release(scriptFrame_t *f, const char *list, int bishops, int deactivate) {
	PyObject *name;
	for (int k = 0; (name = Sf_Next(f, Sf_Global(f, PT, list), k)) != NULL; k++) {
		int mark = f->n;
		PyObject *e = Sf_Entity(f, name);
		if (Sf_True(f, e) > 0) {
			Sf_SetAttrS(f, e, "TgType", "none");
			if (bishops) {
				Sf_SetAttrS(f, e, "EntityList", "Traffic");
			} else {
				Sf_SetAttrS(f, e, "OnDeath", "Vehicles.OnDeath");
			}
			if (deactivate) {
				Sf_Run(f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
			}
		}
		Sf_Release(f, mark);
	}
}

/* OnPorkinsTransportDeath(LooserName, WinnerName): the player's kills raise the mayor's popularity; full: won */
static PyObject *Porkins_OnPorkinsTransportDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Method(&f, G(PT, "bishopTransports"), "remove", "(O)", looser);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	if (Lv_UsrIs(&f, USR(), winner) > 0) {
		Sf_SetGlobal(&f, PT, "mayorPopularity", Sf_Op(&f, G(PT, "mayorPopularity"), '+', Sf_Float(&f, 1.0)));
	}
	Sf_Own(&f, PT, "UpdateScorer", "()");
	int full = Sf_Compare(&f, G(PT, "mayorPopularity"), "==", G(PT, "MAX_POPULARITY"));
	if (full > 0) {
		Pt_Release(&f, "mayorTransports", 0, 0);
		Pt_Release(&f, "bishopTransports", 0, 1);
		Pt_Release(&f, "bishops", 1, 0);
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CreateBishop");
		Cm_Com(&f, "Mission_Campaign2_RemoteMessage3", "Mayor", G(PT, "GetMayorReward"), NULL);
	} else if (full == 0) {
		Sf_Own(&f, PT, "CreatePorkinsTransport", "()");
	}
	return Sf_None(&f);
}

/* OnMayorTransportDeath(LooserName, WinnerName): popularity down, a new transport, the functionary and the mayor
 * complain now and then */
static PyObject *Porkins_OnMayorTransportDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Method(&f, G(PT, "mayorTransports"), "remove", "(O)", looser);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_SetGlobal(&f, PT, "mayorPopularity", Sf_Op(&f, G(PT, "mayorPopularity"), '-', Sf_Float(&f, 1.0)));
	if (Sf_Compare(&f, G(PT, "mayorPopularity"), "<", Sf_Float(&f, 0.0)) > 0) {
		Sf_SetGlobal(&f, PT, "mayorPopularity", Sf_Float(&f, 0.0));
	}
	Sf_Own(&f, PT, "UpdateScorer", "()");
	Sf_Own(&f, PT, "CreateMayorTransport", "()");
	Sf_SetGlobal(&f, PT, "numMayorTransportsDeaths", Sf_OpI(&f, G(PT, "numMayorTransportsDeaths"), '+', 1));
	if (Sf_EqI(&f, Sf_OpI(&f, G(PT, "numMayorTransportsDeaths"), '%', 2), 0) > 0) {
		Cm_Com(&f, "Mission_Campaign2_RemoteMessage2", "Functionary", NULL, NULL);
	}
	if (Sf_EqI(&f, Sf_OpI(&f, G(PT, "numMayorTransportsDeaths"), '%', 3), 0) > 0) {
		Sf_SetGlobal(&f, PT, "taunt", Sf_Op(&f, Sf_Int(&f, 1), '-', G(PT, "taunt")));
		int t = Sf_EqI(&f, G(PT, "taunt"), 0);
		if (t >= 0) {
			Cm_Com(&f, t ? "Mission_Campaign2_RemoteMessage7" : "Mission_Campaign2_RemoteMessage8", "Mayor", NULL, NULL);
		}
	}
	Sf_Later(&f, Sf_Float(&f, 1.0), G(PT, "CheckBishopWithoutTarget"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CheckBishopWithoutTarget(): idle bishops attack again */
static PyObject *Porkins_CheckBishopWithoutTarget(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *name;
	for (int k = 0; (name = Sf_Next(&f, G(PT, "bishops"), k)) != NULL; k++) {
		int mark = f.n;
		if (Sf_True(&f, Sf_Entity(&f, name)) > 0 && Sf_EqI(&f, Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", name), 0) > 0) {
			Sf_Own(&f, PT, "AttackMayorTransport", "(O)", name);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* OnBishopDeath(LooserName, WinnerName): another bishop in a minute */
static PyObject *Porkins_OnBishopDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Method(&f, G(PT, "bishops"), "remove", "(O)", looser);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Later(&f, Sf_Int(&f, 60), G(PT, "CreateBishop"), NOARGS, "CreateBishop");
	return Sf_None(&f);
}

/* OnBishopDamage(LooserName, WinnerName, Damage): shot by the player: it attacks him for 10 s */
static PyObject *Porkins_OnBishopDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) > 0 && Sf_Compare(&f, winner, "==", Sf_Attr(&f, usr, "Name")) > 0) {
		PyObject *e = Sf_Entity(&f, looser);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrS(&f, e, "EntityList", "Enemies");
			Sf_Later(&f, Sf_Int(&f, 10), G(PT, "AttackMayorTransport"), Sf_Build(&f, "(O)", Sf_Attr(&f, e, "Name")), NULL);
		}
	}
	return Sf_None(&f);
}

/* GetMayorReward(): the mayor's money, DeepThroat 2 s later */
static PyObject *Porkins_GetMayorReward(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_ScorerCall(&f, Py_None);
	Sf_DrawScorer(&f);
	PyObject *money = Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '+', G(PT, "mayorReward"));
	Sf_Run(&f, "Scrap", "SetMoney", "(O)", money);
	PyObject *t = Sf_OpI(&f, Sf_Time(&f), '+', 2);
	PyObject *fn = Sf_Mod(&f, "Speech", "AddRemoteCom");
	Sf_Schedule(&f, t, fn, Sf_Build(&f, "(ssO)", "Mission_Campaign2_RemoteMessage4", "DeepThroat", G(PT, "PreCallDeepThroat")), NULL);
	return Sf_None(&f);
}

static PyObject *Porkins_PreCallDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "CallDeepThroat");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToDeepThroat6_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Own(&f, PT, "CallDeepThroat", "()");
	return Sf_None(&f);
}

static PyObject *Porkins_CallDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Cm_PhoneCab(&f, "Mission_DestroyPorkinsTransports.UsePhoneCab");
	return Sf_None(&f);
}

static PyObject *Porkins_UsePhoneCab(PyObject *self, PyObject *args) {
	static const cmPhone_t p = { "Generics_StillPolice1_Functionary", "DeepThroat", "SetNextMission",
		"Mission_TalkToDeepThroat6__ComDeepThroat_NI_1Msg", NULL, NULL, 1, 1, 1 };
	(void)self;
	return Cm_UsePhone(args, PT, &p);
}

/* SetNextMission(): the traffic back, the mayor no longer to possess, the video, on to Orbit */
static PyObject *Porkins_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetAttrS(&f, G(PT, "pc"), "UseFunc", "PhoneCab.DefUseAudioCom");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Sf_Run(&f, "Scrap", "Set", "(sO)", "iTrafficNumber", G(PT, "currentTrafficNumber"));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "GameAct", "3rdMurder");
	PyObject *chars = Sf_Eval(&f, SAVE("Conversor.AvailableChars"), PT);
	Sf_Method(&f, chars, "remove", "(s)", "Mayor");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Conversor.AvailableChars", Sf_Repr(&f, chars));
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_IntroHuman", "Orbit");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Map", "Levels/Orbit");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "PlayVideo", "Video/Mayor.avi");
	Sf_Run(&f, "Scrap", "Achievement", "(i)", 4);
	Sf_Run(&f, "SaveGame", "QLoad", "(s)", "Levels/Orbit");
	return Sf_None(&f);
}

/* UpdateScorer(): the popularity bar and its separator */
static PyObject *Porkins_UpdateScorer(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *scale = Sf_Op(&f, G(PT, "mayorPopularity"), '/', G(PT, "MAX_POPULARITY"));
	PyObject *x = Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "PopularityBar", "X");
	PyObject *w = Sf_Op(&f, Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "PopularityBar", "W"), '*', scale);
	Cm_Set0(&f, "PopularityBar", "Value", G(PT, "mayorPopularity"));
	Cm_Set0(&f, "PopularitySeparator", "X", Sf_Op(&f, x, '+', w));
	return Sf_None(&f);
}

/* MissionScorer(id): the popularity text, bar and separator */
static PyObject *Porkins_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "PopularityText", "Text");
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "PopularitySeparator", "Sprite");
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "PopularityBar", "Bar");
	Sf_MissionText(&f, id, "PopularityText", NULL, Cm_X(&f, 0), Cm_Y(&f, 0), "Horatio", "Shadow", Cm_Label(&f, "Mission_Campaign2_Scorer1"),
		"Left", NULL);
	Lv_BarDefaults(&f, id, "PopularityBar", Sf_Build(&f, "(iiii)", 16, 16, 255, 192), Sf_Build(&f, "(iiii)", 255, 16, 192, 192));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "PopularityBar", "MaxValue", G(PT, "MAX_POPULARITY"));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "PopularityBar", "Value", G(PT, "mayorPopularity"));
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "PopularityBar", "Red", 255);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "PopularityBar", "Green", 0);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "PopularityBar", "Blue", 0);
	PyObject *height = Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "PopularityBar", "H");
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "PopularitySeparator", "Red", 0);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "PopularitySeparator", "Green", 255);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "PopularitySeparator", "Blue", 0);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "PopularitySeparator", "X", Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "PopularityBar", "X"));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "PopularitySeparator", "Y", Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "PopularityBar", "Y"));
	Sf_Run(&f, "SScorer", "Set", "(Ossd)", id, "PopularitySeparator", "ScaleX", 1.0 / 2);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "PopularitySeparator", "ScaleY", Sf_OpI(&f, height, '/', 4));
	Sf_Own(&f, PT, "UpdateScorer", "()");
	return Sf_None(&f);
}

PyMethodDef g_CommercialDestroyPorkinsTransportsMethods[] = {
	{ "GetBishopsEnemiesProfile", Porkins_GetBishopsEnemiesProfile, 1, NULL },
	{ "InitStatus", Porkins_InitStatus, 1, NULL },
	{ "Init", Porkins_Init, 1, NULL },
	{ "InitMission", Porkins_InitMission, 1, NULL },
	{ "CreateMayorTransport", Porkins_CreateMayorTransport, 1, NULL },
	{ "CreatePorkinsTransport", Porkins_CreatePorkinsTransport, 1, NULL },
	{ "CreateBishop", (PyCFunction)(void (*)(void))Porkins_CreateBishop, 3, NULL },
	{ "AttackMayorTransport", Porkins_AttackMayorTransport, 1, NULL },
	{ "OnPorkinsTransportDeath", Porkins_OnPorkinsTransportDeath, 1, NULL },
	{ "OnMayorTransportDeath", Porkins_OnMayorTransportDeath, 1, NULL },
	{ "CheckBishopWithoutTarget", Porkins_CheckBishopWithoutTarget, 1, NULL },
	{ "OnBishopDeath", Porkins_OnBishopDeath, 1, NULL },
	{ "OnBishopDamage", Porkins_OnBishopDamage, 1, NULL },
	{ "GetMayorReward", Porkins_GetMayorReward, 1, NULL },
	{ "PreCallDeepThroat", Porkins_PreCallDeepThroat, 1, NULL },
	{ "CallDeepThroat", Porkins_CallDeepThroat, 1, NULL },
	{ "UsePhoneCab", Porkins_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Porkins_SetNextMission, 1, NULL },
	{ "UpdateScorer", Porkins_UpdateScorer, 1, NULL },
	{ "MissionScorer", Porkins_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
