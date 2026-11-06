/*
 * The scripts of the level Scrapyard in C (levels/scrapyard/scripts of data02.packed, issue #2): the Map module (the
 * traffic, the metro and the elevator to the gamblin den), MapTriggers (the lairs of the mosquitos and the mad
 * hunters) and the missions of the level (Mission_Ambush, Mission_DefeatCrazyGambler2, Mission_FindDtritusMatrix,
 * Mission_MeetBerto, Mission_MosquitoClue, Mission_StupidMerc, Mission_StupidMercAgain, Mission_TalkToDeepThroat).
 * MapSnd and the plain functions of the missions are data (data/levels/scrapyard/). The module variables (counters,
 * lists, waypoints, constants, models) and the module code (the save variables they create) stay in the scripts; the
 * functions read and write them as their globals.
 */
#include "Levels.h"

#define MT "MapTriggers"
#define AM "Mission_Ambush"
#define CG "Mission_DefeatCrazyGambler2"
#define FD "Mission_FindDtritusMatrix"
#define MB "Mission_MeetBerto"
#define MC "Mission_MosquitoClue"
#define SM "Mission_StupidMerc"
#define SA "Mission_StupidMercAgain"
#define TD "Mission_TalkToDeepThroat"

#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define USR() Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0)
#define LANG(k) Sf_Lang(&f, Sf_Str(&f, k))
#define SAVE(n) Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", n)
#define G(m, n) Sf_Global(&f, m, n)
#define NOARGS Sf_Build(&f, "()")

static const char *const ac_ships_std = "[\"SMerc1\",\"SMerc2\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SBetty1\",\"SArchbishop1\"]";
static const char *const ac_ships_all = "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SMerc3\",\"SLMerc\",\"SMayor1\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\",\"SArchbishop1\"]";

/* SVec.Mod(SVec.Sub(a.Pos, b.Pos)) */
static PyObject *Sy_Dist(scriptFrame_t *f, PyObject *a, PyObject *b) {
	PyObject *pa = Sf_Attr(f, a, "Pos");
	return Sf_Ask(f, "SVec", "Mod", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", pa, Sf_Attr(f, b, "Pos")));
}

/* SVec.ModSqr(SVec.Sub(usr.Pos, Scrap.GetEntity("DM_Door_Elevator_GamblinDen").Pos)) */
static PyObject *Sy_DistDenSqr(scriptFrame_t *f, PyObject *usr) {
	PyObject *up = Sf_Attr(f, usr, "Pos");
	PyObject *door = Sf_Ask(f, "Scrap", "GetEntity", "(s)", "DM_Door_Elevator_GamblinDen");
	return Sf_Ask(f, "SVec", "ModSqr", "(O)", Sf_Ask(f, "SVec", "Sub", "(OO)", up, Sf_Attr(f, door, "Pos")));
}

/*
 * UsePhoneCab(UsrName, CharName, optId, FullOption) of a mission: the audio com of the phone cab (the cab is the
 * global pc), the police warning of objWho while the alarm is on, else the NI talk of niWho with the end function of
 * module; activate: the cab stops being the target first; resetUse: the cab gets its default use after the talk starts
 */
typedef struct {
	const char *police, *objWho, *end, *niWho, *ni;
	byte activate, resetUse;
} syPhone_t;

static PyObject *Sy_UsePhone(PyObject *args, const char *module, const syPhone_t *p) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "PhoneCab");
	Sf_SpeechNames(&f, usr, ch);
	Sf_Run(&f, "PhoneCab", "UseAudioCom", "(OOOO)", usr, ch, opt, full);
	Sf_SetGlobal(&f, module, "pc", Sf_Entity(&f, ch));
	int alarm = Lv_Alarm(&f);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", G(module, "pc"), p->police, p->objWho);
	} else if (alarm == 0) {
		if (p->activate) {
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(module, "pc"), 0);
		}
		Sf_SpeechEnd(&f, G(module, p->end), NULL);
		Sf_NISpeech(&f, p->niWho, p->ni);
		if (p->resetUse) {
			Sf_SetAttrS(&f, Sf_Entity(&f, ch), "UseFunc", "PhoneCab.DefUseAudioCom");
		}
	}
	return Sf_None(&f);
}

/* apply(MissionsFuncs.SetNextMission, NextMissionData) with the global of module */
static byte Sy_ApplyNext(scriptFrame_t *f, const char *module) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, "MissionsFuncs", "SetNextMission");
	Sf_Ask(f, "__builtin__", "apply", "(OO)", fn, Sf_Global(f, module, "NextMissionData"));
	Sf_Release(f, mark);
	return !f->failed;
}

/* for i in range(n): body; the range of a Python value (the frame keeps it) */
static PyObject *Sy_Range(scriptFrame_t *f, PyObject *n) {
	return Sf_Ask(f, "__builtin__", "range", "(O)", n);
}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors, the scrapyard, the police; the traffic by who controls the robots, the metro and
 * the elevator */
static PyObject *Scrapyard_InitStatus(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "OutPolice", "Metro", "Traffic", "Parking", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 1));
	Sf_SetMod(&f, "Init", "isScrapYard", Sf_Int(&f, 1));
	Sf_SetMod(&f, "OutPolice", "PoliceActive", Sf_Int(&f, 1));
	int boss = Sf_EqS(&f, SAVE("RobotsControlledByBoss"), "1");
	if (boss > 0) {
		Lv_BossTraffic(&f, 0);
	} else if (boss == 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 4);
		Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", -1);
		Sf_SetMod(&f, "Traffic", "AcNormalShips", Sf_Build(&f, "[sss]", "TLight1", "TLight2", "TLight3"));
		Sf_SetMod(&f, "Traffic", "NumNormal", Sf_Int(&f, 10));
		Sf_SetMod(&f, "Traffic", "AcMediumShips", Sf_Build(&f, "[sss]", "TMed1", "TMed2", "TMed3"));
		Sf_SetMod(&f, "Traffic", "NumMedium", Sf_Int(&f, 20));
		Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[sss]", "THeavy1", "THeavy2", "THeavy3"));
		Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 10));
	}
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Metro", "DM_Door_MetroExt_ScrapYard");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "MetroExt", "DM_Door_MetroExt_ScrapYard");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Elevator", "DM_Door_Elevator_GamblinDen");
	return Sf_None(&f);
}

/* Init(LevelPath): the elevator to the gamblin den, the metro, the parkings of the lonely mercenary, the clouds */
static PyObject *Scrapyard_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "Metro");
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_GamblinDen", "GamblinDen");
	Sf_Run(&f, "Metro", "SetAsMetro", "(s)", "DM_Door_MetroExt_ScrapYard");
	if (Sf_EqS(&f, SAVE("LonelyMercLairActive"), "1") > 0) {
		Sf_Run(&f, "Parking", "CreateLonelyMercParkings", "()");
	}
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudRed", 77);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudGreen", 80);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudBlue", 60);
	return Sf_None(&f);
}

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

static PyObject *MT_GetMadHunterProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, MT, mt_madhunter_profile);
}

static PyObject *MT_GetMosquitoProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, MT, mt_mosquito_profile);
}

/* Init(LevelPath): the mosquitos reprogrammed, the kills counted from now, the triggers of the inhabitants */
static PyObject *MT_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "VehiclesNPC");
	PyObject *aiprof = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(s)", "SMosquito");
	Sf_SetAttrs(&f, aiprof, MT, mt_mosquito_reprogram);
	Sf_SetGlobal(&f, MT, "numMosquitosKilled", Sf_ToInt(&f, SAVE("Stats.Mosquito")));
	if (Sf_True(&f, G(MT, "WithScrapyardInhabitants")) > 0) {
		Sf_Own(&f, MT, "CreateMosquitosTriggers", "()");
		Sf_Own(&f, MT, "CreateMadHuntersTriggers", "()");
	}
	return Sf_None(&f);
}

/* CheckMosquitoCadenceShoot(): the mosquitos shoot faster after 20 kills (the branch of 40 is never reached) */
static PyObject *MT_CheckMosquitoCadenceShoot(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	PyObject *cur = Sf_Op(&f, Sf_ToInt(&f, SAVE("Stats.Mosquito")), '-', G(MT, "numMosquitosKilled"));
	int c = Sf_Compare(&f, cur, ">", Sf_Int(&f, 20));
	if (c == 0) {
		c = Sf_Compare(&f, cur, ">", Sf_Int(&f, 40)) > 0 ? 2 : 0;
	}
	if (c > 0) {
		PyObject *aiprof = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(s)", "SMosquito");
		Sf_SetAttrI(&f, aiprof, "CadenceShoot", c == 2 ? 5 : 20);
	}
	return Sf_None(&f);
}

/* the triggers of a kind of lair: "DM_<kind>Trigger_<name>_<radius>" dummies become items "<kind>Trigger<name>", and
 * the dummy "DM_<kind>Spawn_<name>" of each is its lair in the global table lairs */
typedef struct {
	const char *kind, *lairs, *onTake, *repeated, *missing;
} mtTriggers_t;

static PyObject *MT_CreateTriggers(PyObject *args, const mtTriggers_t *t) {
	char text[0x40];
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *names = Sf_Build(&f, "[]");
	Str_Printf(text, sizeof(text), "DM_%sTrigger_*", t->kind);
	PyObject *dummy = Lv_Dummy(&f, text, 1);
	int mark = f.n;
	while (Sf_True(&f, dummy) > 0) {
		PyObject *fields = Sf_Ask(&f, "string", "split", "(Os)", Sf_Item(&f, dummy, 0), "_");
		PyObject *fn = Sf_Mod(&f, "Scrap", "CreateEntity");
		Str_Printf(text, sizeof(text), "%sTrigger", t->kind);
		PyObject *name = Sf_Cat(&f, "sO", text, Sf_Item(&f, fields, 2));
		PyObject *p = Sf_Item(&f, dummy, 1);
		PyObject *x = Sf_Item(&f, p, 0), *y = Sf_Item(&f, p, 1), *z = Sf_Item(&f, p, 2);
		PyObject *l = Sf_Call(&f, fn, "(OOOOs)", name, x, y, z, "Item");
		Sf_SetAttr(&f, l, "Radius", Sf_ToInt(&f, Sf_Item(&f, fields, 3)));
		Sf_SetAttrS(&f, l, "TgTypeMarker", "RGBK");
		Sf_SetAttrS(&f, l, "OnTake", t->onTake);
		int in = Sf_In(&f, Sf_Item(&f, fields, 2), names);
		if (in == 0) {
			Sf_Method(&f, names, "append", "(O)", Sf_Item(&f, fields, 2));
		} else if (in > 0) {
			Sf_RaiseText(&f, t->repeated);
		}
		PyObject *next = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		Sf_ReleaseKeep(&f, mark, &next, 1);
		dummy = next;
	}
	PyObject *name;
	mark = f.n;
	for (int k = 0; (name = Sf_Next(&f, names, k)) != NULL; k++) {
		Str_Printf(text, sizeof(text), "DM_%sSpawn_", t->kind);
		Sf_Run(&f, "Scrap", "StartDummySearch", "(Oi)", Sf_Cat(&f, "sO", text, name), 0);
		dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		int found = Sf_True(&f, dummy);
		if (found > 0) {
			Str_Printf(text, sizeof(text), "%sTrigger", t->kind);
			PyObject *lairs = G(MT, t->lairs);
			Sf_SetItem(&f, lairs, Sf_Cat(&f, "sO", text, name), Sf_Item(&f, dummy, 1));
		} else if (found == 0) {
			Sf_RaiseText(&f, t->missing);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

static const mtTriggers_t mt_mosquitos = { "Mosquito", "mosquitosLairs", "MapTriggers.OnMosquitoTriggerTaken",
	"El nombre del trigger del mosquito est\xe1 repetido", "No existe guarida de mosquito asociada a su correspondiente trigger" };
static const mtTriggers_t mt_madhunters = { "MadHunter", "madHuntersLairs", "MapTriggers.OnMadHunterTriggerTaken",
	"El nombre del trigger del mad hunter est\xe1 repetido", "No existe guarida de mad hunter asociada a su correspondiente trigger" };

static PyObject *MT_CreateMosquitosTriggers(PyObject *self, PyObject *args) {
	(void)self;
	return MT_CreateTriggers(args, &mt_mosquitos);
}

static PyObject *MT_CreateMadHuntersTriggers(PyObject *self, PyObject *args) {
	(void)self;
	return MT_CreateTriggers(args, &mt_madhunters);
}

/* a trigger taken by the player or a ship allowed to: the list fills up to the maximum from the lair, one ship each
 * step seconds */
typedef struct {
	const char *allowed, *shipVar, *maxVar, *list, *create, *lairs;
	double step;
} mtTaken_t;

static PyObject *MT_TriggerTaken(PyObject *args, const mtTaken_t *t) {
	PyObject *item, *ch;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &item, &ch)) {
		return NULL;
	}
	int go = Lv_UsrIs(&f, USR(), ch);
	if (go == 0) {
		go = Sf_In(&f, ch, G(MT, t->allowed));
	}
	if (go <= 0) {
		return Sf_None(&f);
	}
	Sf_Run(&f, "Scrap", "Set", "(sO)", t->shipVar, ch);
	Sf_SetAttrI(&f, Sf_Entity(&f, item), "rTime", 10);
	PyObject *max = Sf_Ask(&f, "Scrap", "Get", "(s)", t->maxVar);
	PyObject *size = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", t->list, "Num");
	if (Sf_Compare(&f, size, "<", max) > 0) {
		PyObject *range = Sy_Range(&f, Sf_Op(&f, max, '-', size));
		PyObject *timy = Sf_Time(&f);
		int mark = f.n;
		for (int k = 0; Sf_Next(&f, range, k) != NULL; k++) {
			PyObject *lair = Sf_GetItem(&f, G(MT, t->lairs), item);
			Sf_Schedule(&f, timy, G(MT, t->create), Sf_Build(&f, "(O)", lair), "CreateMosquito");
			PyObject *next = Sf_Op(&f, timy, '+', Sf_Float(&f, t->step));
			Sf_ReleaseKeep(&f, mark, &next, 1);
			timy = next;
		}
	}
	return Sf_None(&f);
}

static PyObject *MT_OnMosquitoTriggerTaken(PyObject *self, PyObject *args) {
	static const mtTaken_t t = { "shipsAllowedToLaunchMosquitos", "MosquitosTriggeredShipName", "MaxMosquitos", "Mosquitos",
		"CreateMosquito", "mosquitosLairs", 0.25 };
	(void)self;
	return MT_TriggerTaken(args, &t);
}

static PyObject *MT_OnMadHunterTriggerTaken(PyObject *self, PyObject *args) {
	static const mtTaken_t t = { "shipsAllowedToLaunchMadHunters", "MadHuntersTriggeredShipName", "MaxMadHunters", "MadHunters",
		"CreateMadHunter", "madHuntersLairs", 0.5 };
	(void)self;
	return MT_TriggerTaken(args, &t);
}

/* a ship of a lair: at a free spawn point with its profile, then moved to the lair */
static PyObject *MT_CreateShip(PyObject *args, const char *name, const char *profile, const char *list, long money, byte cadence) {
	PyObject *lair;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &lair)) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *o = Lv_SpawnCar(&f, name, 0);
	Lv_StdProfile(&f, MT, profile, o);
	Sf_SetAttrS(&f, o, "EntityList", list);
	Sf_SetAttrS(&f, o, "TgType", "red");
	Sf_SetAttr(&f, o, "Pos", lair);
	Sf_SetAttr(&f, o, "LLogicLairPos", lair);
	Sf_SetAttrI(&f, o, "Money", money);
	if (cadence) {
		Sf_Own(&f, MT, "CheckMosquitoCadenceShoot", "()");
	}
	return Sf_None(&f);
}

static PyObject *MT_CreateMosquito(PyObject *self, PyObject *args) {
	(void)self;
	return MT_CreateShip(args, "Mosquito", "GetMosquitoProfile", "Mosquitos", 25, 1);
}

static PyObject *MT_CreateMadHunter(PyObject *self, PyObject *args) {
	(void)self;
	return MT_CreateShip(args, "MadHunter", "GetMadHunterProfile", "MadHunters", 100, 0);
}

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

static PyObject *Ambush_GetMadHuntersProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, AM, am_madhunters_profile);
}

/* InitStatus(LevelPath): the probes in the heavy traffic (none yet), no inhabitants */
static PyObject *Ambush_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "Traffic");
	Sf_Import(&f, "MapTriggers");
	Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[s]", "TProbe"));
	Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	Sf_SetMod(&f, "MapTriggers", "WithScrapyardInhabitants", Sf_Int(&f, 0));
	Lv_AcShips(&f, AM, ac_ships_std);
	return Sf_None(&f);
}

/* Init(LevelPath): the state of the save variable NumProbes: call Traitorius (-1), Deep Throat (-2), or the ambush */
static PyObject *Ambush_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_SetGlobal(&f, AM, "numProbes", Sf_ToInt(&f, SAVE("NumProbes")));
	int c = Sf_EqI(&f, G(AM, "numProbes"), -1);
	if (c > 0) {
		Sf_Own(&f, AM, "CallTraitorius", "()");
	} else if (c == 0) {
		c = Sf_EqI(&f, G(AM, "numProbes"), -2);
		if (c > 0) {
			Sf_Own(&f, AM, "CallDeepThroat", "()");
		} else if (c == 0) {
			Sf_Own(&f, AM, "LaunchAmbush", "()");
		}
	}
	return Sf_None(&f);
}

/* CallTraitorius(), CallDeepThroat(): a free phone cab is the target (pc is a local of the script) */
static PyObject *Ambush_CallTraitorius(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PhoneCab(&f, "Mission_Ambush.UsePhoneCab", 1, NULL);
	return Sf_None(&f);
}

static PyObject *Ambush_CallDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PhoneCab(&f, "Mission_Ambush.UsePhoneCab2", 1, NULL);
	return Sf_None(&f);
}

static PyObject *Ambush_UsePhoneCab(PyObject *self, PyObject *args) {
	static const syPhone_t p = { "Generics_StillPolice1_Functionary", "Traitorius", "DeepThroatCom", "Functionary",
		"Mission_Ambush1_Traitorius_NI_1Msg", 1, 0 };
	(void)self;
	return Sy_UsePhone(args, AM, &p);
}

static PyObject *Ambush_UsePhoneCab2(PyObject *self, PyObject *args) {
	static const syPhone_t p = { "Generics_StillPolice1_DeepThroat", "DeepThroat", "SetNextMission", "DeepThroat",
		"Mission_TalkToDeepThroat2_ComDeepThroat_NI_1Msg", 0, 1 };
	(void)self;
	return Sy_UsePhone(args, AM, &p);
}

/* DeepThroatCom(): after Traitorius: Deep Throat's message, then the ambush */
static PyObject *Ambush_DeepThroatCom(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PcDefault(&f, AM);
	Sf_SetMod(&f, "Speech", "OnOutOfRange", Py_None);
	Lv_Com(&f, "Mission_Ambush1_RemoteMessage1", "DeepThroat", G(AM, "PreLaunchAmbush"), NOARGS);
	return Sf_None(&f);
}

/* IsValidPos(usrPos, pos): at least minPlayerDist from the player */
static PyObject *Ambush_IsValidPos(PyObject *self, PyObject *args) {
	PyObject *usrPos, *pos;
	SF_ARGS("OO", &usrPos, &pos);
	PyObject *d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", pos, usrPos));
	int c = Sf_Compare(&f, d, "<", G(AM, "minPlayerDist"));
	return c < 0 ? Sf_None(&f) : Sf_Return(&f, Sf_Int(&f, c == 0));
}

/* CreateProbe(): a probe at a random visible point far from the player, a target */
static PyObject *Ambush_CreateProbe(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "Traffic");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *pos = Sf_Ask(&f, "SAI", "GetRandomVisibilityPoint", "()");
	int mark = f.n;
	for (;;) {
		int ok = Sf_True(&f, Sf_Own(&f, AM, "IsValidPos", "(OO)", Sf_Attr(&f, usr, "Pos"), pos));
		if (ok != 0) {
			break;
		}
		PyObject *next = Sf_Ask(&f, "SAI", "GetRandomVisibilityPoint", "()");
		Sf_ReleaseKeep(&f, mark, &next, 1);
		pos = next;
	}
	Sf_Run(&f, "Traffic", "SetPath", "()");
	PyObject *p = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ssii)", "TProbe", "ProbeMos", 0, 0);
	Sf_Run(&f, "Traffic", "ResetPath", "()");
	Sf_SetAttrS(&f, p, "OnDeath", "Mission_Ambush.OnProbeDeath");
	Sf_SetAttr(&f, p, "MovePos", pos);
	Sf_SetAttr(&f, p, "Pos", pos);
	PyObject *ang = Sf_Build(&f, "(iii)", 0, 0, 0);
	Sf_SetAttr(&f, p, "MoveAng", ang);
	Sf_SetAttr(&f, p, "Ang", ang);
	Sf_SetAttr(&f, p, "MoveTime", Sf_Float(&f, 0.1));
	PyObject *life = G(AM, "PROBE_LIFE");
	Sf_SetAttr(&f, p, "Life", life);
	Sf_SetAttr(&f, p, "MaxLife", life);
	Sf_SetAttr(&f, p, "MinLife", life);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", p, 1);
	Sf_Print(&f, "O", Sf_Attr(&f, p, "Name"));
	return Sf_None(&f);
}

/* CreateMadHunter(): a mad hunter at a free spawn point in the enemies */
static PyObject *Ambush_CreateMadHunter(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *m = Lv_SpawnCar(&f, "Traffic", 0);
	Lv_StdProfile(&f, AM, "GetMadHuntersProfile", m);
	Sf_SetAttrS(&f, m, "EntityList", "Enemies");
	Lv_Life(&f, m, AM, "MADHUNTER_LIFE");
	return Sf_None(&f);
}

/* CreateMadHunters(): two mad hunters per probe left */
static PyObject *Ambush_CreateMadHunters(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_Ambush.OnMadHunterDeath");
	PyObject *range = Sy_Range(&f, Sf_OpI(&f, G(AM, "numProbes"), '*', 2));
	int mark = f.n;
	for (int k = 0; Sf_Next(&f, range, k) != NULL; k++) {
		Sf_Own(&f, AM, "CreateMadHunter", "()");
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* OnProbeDeath(LooserName, WinnerName): two mad hunters go with the probe; the next probe or the end */
static PyObject *Ambush_OnProbeDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_SetGlobal(&f, AM, "numProbes", Sf_OpI(&f, G(AM, "numProbes"), '-', 1));
	PyObject *m1 = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Enemies", "First"));
	if (Sf_True(&f, m1) > 0) {
		PyObject *m2 = Sf_Entity(&f, Sf_Attr(&f, m1, "NextInList"));
		if (Sf_True(&f, m2) > 0) {
			Sf_SetAttrS(&f, m2, "EntityList", "");
			Sf_SetAttrI(&f, m2, "Life", 0);
		}
		Sf_SetAttrS(&f, m1, "EntityList", "");
		Sf_SetAttrI(&f, m1, "Life", 0);
	}
	int done = Sf_EqI(&f, G(AM, "numProbes"), 0);
	if (done > 0) {
		Lv_Com(&f, "Mission_Ambush2_RemoteMessage4", "DeepThroat", G(AM, "PreCallDeepThroat"), NOARGS);
	} else if (done == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumProbes", Sf_Repr(&f, G(AM, "numProbes")));
		Sf_Run(&f, "SaveGame", "QWayPoint", "()");
		Sf_Own(&f, AM, "CreateProbe", "()");
		PyObject *n = Sf_Repr(&f, Sf_Op(&f, Sf_Int(&f, 4), '-', G(AM, "numProbes")));
		Sf_Run(&f, "Speech", "AddRemoteCom", "(Os)", Sf_Cat(&f, "sO", "Mission_Ambush2_RemoteMessage", n), "DeepThroat");
	}
	return Sf_None(&f);
}

/* PreCallDeepThroat(): the probes are done: call Deep Throat */
static PyObject *Ambush_PreCallDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, AM, "numProbes", Sf_Int(&f, -2));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumProbes", Sf_ReprI(&f, -2));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToDeepThroat2_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Own(&f, AM, "CallDeepThroat", "()");
	return Sf_None(&f);
}

static PyObject *Ambush_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PcDefault(&f, AM);
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumProbes");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_AttackTraitorius", "Outskirts");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_AttackTraitorius_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

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

static PyObject *Gambler_GetCrazyGamblerProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, CG, cg_gambler_profile);
}

static PyObject *Gambler_GetMosquitoProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, CG, cg_mosquito_profile);
}

static PyObject *Gambler_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "MapTriggers");
	Sf_SetMod(&f, "MapTriggers", "WithScrapyardInhabitants", Sf_Int(&f, 0));
	Lv_AcShips(&f, CG, ac_ships_all);
	return Sf_None(&f);
}

/* Init(LevelPath): the lives are done: the next mission; else the crazy gambler, the mosquitos after the message,
 * the taunts and the scorer */
static PyObject *Gambler_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	int done = Sf_Compare(&f, Sf_ToInt(&f, SAVE("NumCrazyGamblerLifes")), "==", G(CG, "CrazyGamblerLifesNextMission"));
	if (done > 0) {
		Sf_Own(&f, CG, "PreSetNextMission", "()");
	} else if (done == 0) {
		Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "EvilGambler", "Models/Heads/HeadEvilGambler/HeadEvilGambler", "", 4, 20);
		Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_DefeatCrazyGambler2.OnCrazyGamblerDeath");
		Lv_Com(&f, "Mission_DefeatCrazyGambler2_RemoteMessage1", "DeepThroat", G(CG, "CreateMosquitos"), NOARGS);
		Sf_Own(&f, CG, "CreateCrazyGambler", "()");
		Sf_Later(&f, Sf_Int(&f, 3), G(CG, "LaunchTaunts"), NOARGS, NULL);
		Sf_SetMod(&f, "Menu", "MissionScorerCall", G(CG, "MissionScorer"));
		Sf_DrawScorer(&f);
	}
	return Sf_None(&f);
}

/* LaunchTaunts(): every 12 s one of the five taunts while the crazy gambler lives */
static PyObject *Gambler_LaunchTaunts(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, Sf_Entity(&f, G(CG, "crazyGamblerName"))) > 0) {
		PyObject *n = Sf_Repr(&f, Sf_Op(&f, Sf_Int(&f, 2), '+', G(CG, "numTaunt")));
		Sf_Run(&f, "Speech", "RemoteComunication", "(Os)", Sf_Cat(&f, "sO", "Mission_DefeatCrazyGambler2_RemoteMessage", n), "EvilGambler");
		Sf_SetGlobal(&f, CG, "numTaunt", Sf_OpI(&f, Sf_OpI(&f, G(CG, "numTaunt"), '+', 1), '%', 5));
	}
	Sf_Later(&f, Sf_Int(&f, 12), G(CG, "LaunchTaunts"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CreateMosquito(): a mosquito at the nearest free spawn point; every other one drops nothing */
static PyObject *Gambler_CreateMosquito(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *o1 = Lv_SpawnCar(&f, "Traffic", 1);
	Lv_StdProfile(&f, CG, "GetMosquitoProfile", o1);
	Sf_SetAttrS(&f, o1, "EntityList", "Enemies");
	Sf_SetAttr(&f, o1, "Ammo00", Sf_Ask(&f, "SWeap", "GetFAmmo", "(is)", 0, "Max"));
	Lv_Life(&f, o1, CG, "MOSQUITO_LIFE");
	Sf_SetGlobal(&f, CG, "givesSomething", Sf_Op(&f, Sf_Int(&f, 1), '-', G(CG, "givesSomething")));
	if (Sf_True(&f, G(CG, "givesSomething")) == 0) {
		Sf_SetAttrI(&f, o1, "Money", -1);
	}
	Sf_Method(&f, G(CG, "mosquitos"), "append", "(O)", Sf_Attr(&f, o1, "Name"));
	return Sf_None(&f);
}

/* CreateMosquitos(): the mosquitos up to the maximum, again every timeMosquitosNewWave seconds */
static PyObject *Gambler_CreateMosquitos(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *max = G(CG, "maxMosquitos");
	int n = Sf_Len(&f, G(CG, "mosquitos"));
	PyObject *range = Sy_Range(&f, Sf_Op(&f, max, '-', Sf_Int(&f, n)));
	PyObject *i;
	int mark = f.n;
	for (int k = 0; (i = Sf_Next(&f, range, k)) != NULL; k++) {
		PyObject *t = Sf_Time(&f);
		Sf_Schedule(&f, Sf_Op(&f, t, '+', Sf_Op(&f, i, '*', Sf_Float(&f, 0.2))), G(CG, "CreateMosquito"), NOARGS, NULL);
		Sf_Release(&f, mark);
	}
	Sf_Later(&f, G(CG, "timeMosquitosNewWave"), G(CG, "CreateMosquitos"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CreateCrazyGambler(): the crazy gambler at the nearest free spawn point, the target */
static PyObject *Gambler_CreateCrazyGambler(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *h = Lv_SpawnCar(&f, "CrazyGambler", 1);
	Lv_StdProfile(&f, CG, "GetCrazyGamblerProfile", h);
	Sf_SetAttrI(&f, h, "LaunchTaunts", 0);
	Sf_SetAttr(&f, h, "MPil0", G(CG, "pilotName"));
	Sf_SetAttrS(&f, h, "TgType", "red");
	Sf_SetAttrS(&f, h, "EntityList", "Enemies");
	Lv_Life(&f, h, CG, "CRAZY_GAMBLER_LIFE");
	Sf_SetGlobal(&f, CG, "crazyGamblerName", Sf_Attr(&f, h, "Name"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(CG, "crazyGamblerName"));
	return Sf_None(&f);
}

/* OnCrazyGamblerDeath(LooserName, WinnerName): one life fewer (the next mission or the gambler again), or a mosquito
 * less */
static PyObject *Gambler_OnCrazyGamblerDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	int gambler = Sf_Compare(&f, looser, "==", G(CG, "crazyGamblerName"));
	if (gambler > 0) {
		Sf_Run(&f, "SScorer", "CancelSpeech", "()");
		PyObject *lifes = Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumCrazyGamblerLifes")), '-', 1);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumCrazyGamblerLifes", Sf_Repr(&f, lifes));
		int done = Sf_Compare(&f, lifes, "==", G(CG, "CrazyGamblerLifesNextMission"));
		if (done > 0) {
			Sf_SetMod(&f, "Menu", "MissionScorerCall", Py_None);
			Sf_Later(&f, Sf_Int(&f, 7), G(CG, "PreSetNextMission"), NOARGS, NULL);
		} else if (done == 0) {
			Sf_Later(&f, Sf_Int(&f, 3), G(CG, "CreateCrazyGambler"), NOARGS, NULL);
		}
		Sf_DrawScorer(&f);
	} else if (gambler == 0 && Sf_In(&f, looser, G(CG, "mosquitos")) > 0) {
		Sf_Method(&f, G(CG, "mosquitos"), "remove", "(O)", looser);
	}
	return Sf_None(&f);
}

/* MissionScorer(id): the lives left */
static PyObject *Gambler_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	PyObject *x = Sf_Mod(&f, "Scorer", "MissionScorerText_X"), *y = Sf_Mod(&f, "Scorer", "MissionScorerText_Y");
	Sf_MissionText(&f, id, "RemainingLifesText", "Text", x, y, "Horatio", "Shadow",
		Sf_Cat(&f, "Os", LANG("Mission_DefeatCrazyGambler2_Scorer1"), ":"), "Left", NULL);
	x = Sf_Mod(&f, "Scorer", "MissionScorerText_X");
	y = Sf_OpI(&f, Sf_Mod(&f, "Scorer", "MissionScorerText_Y"), '+', 20);
	Sf_MissionText(&f, id, "RemainingLifes", "Text", x, y, "Horatio", "Shadow", SAVE("NumCrazyGamblerLifes"), "Left", NULL);
	return Sf_None(&f);
}

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

/* SetLonelyMercProfile(): the profile, also VehiclesNPC.AIProf[sp.Ship] */
static PyObject *Dtritus_SetLonelyMercProfile(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *sp = Lv_Profile(&f, FD, fd_lonelymerc_profile);
	Sf_SetItem(&f, Sf_Mod(&f, "VehiclesNPC", "AIProf"), Sf_Attr(&f, sp, "Ship"), sp);
	return Sf_Return(&f, sp);
}

static PyObject *Dtritus_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Lv_AcShips(&f, FD, ac_ships_std);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "LonelyMercLairActive", "0");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "LonelyMercActive", "0");
	return Sf_None(&f);
}

/* e.Active = v for the entities of the names in the global list (missing ones skipped) */
static byte Fd_Active(scriptFrame_t *f, const char *list, long v) {
	PyObject *p;
	int mark = f->n;
	for (int k = 0; (p = Sf_Next(f, Sf_Global(f, FD, list), k)) != NULL; k++) {
		PyObject *e = Sf_Entity(f, p);
		if (Sf_True(f, e) > 0) {
			Sf_SetAttrI(f, e, "Active", v);
		}
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* Init(LevelPath): the plans and the matrix, the lair's parkings off; the player in a lonely mercenary ship of his own
 * goes on with it, else the ship waits at the gamblin den's hangar */
static PyObject *Dtritus_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(FD, "MatrixModel"));
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(FD, "TestForLonelyMercShip"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(FD, "OutOfLonelyMercShip"));
	Sf_Own(&f, FD, "CreateMissionPlans", "()");
	Sf_Own(&f, FD, "CreateDtritusMatrix", "()");
	Sf_SetGlobal(&f, FD, "lonelyMercParkings", Sf_Ask(&f, "Parking", "CreateLonelyMercParkings", "()"));
	Fd_Active(&f, "lonelyMercParkings", 0);
	PyObject *usr = USR();
	int own = Sf_True(&f, usr);
	if (own > 0) {
		own = Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car"));
	}
	if (own > 0) {
		own = Sf_True(&f, Sf_Mod(&f, "Parking", "PlayerShip"));
	}
	if (own > 0) {
		int merc = Sf_EqS(&f, Sf_Ask(&f, "string", "upper", "(O)", Sf_Attr(&f, usr, "Model")), "SMERC3");
		if (merc > 0) {
			PyObject *n = Sf_Attr(&f, usr, "Name");
			merc = Sf_Compare(&f, n, "!=", Sf_Attr(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Name"));
		}
		if (merc > 0) {
			Sf_SetGlobal(&f, FD, "lonelyMercShipName", Sf_Attr(&f, usr, "Name"));
			Sf_Own(&f, FD, "CreateBadGuy", "()");
			Sf_Own(&f, FD, "CheckLonelyMercShipExist", "()");
			return Sf_None(&f);
		}
	}
	Sf_Own(&f, FD, "CheckCreateBadGuy", "()");
	Sf_Own(&f, FD, "CreateLonelyMercShip", "()");
	return Sf_None(&f);
}

/* CreateLonelyMercShip(): the ship parked nearest the gamblin den's hangar becomes the lonely mercenary's (invulnerable,
 * the target) */
static PyObject *Dtritus_CreateLonelyMercShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_Compare(&f, G(FD, "lastShipUsed"), "!=", Sf_Str(&f, "")) > 0 && Sf_EqS(&f, SAVE("LonelyMercActive"), "0") > 0) {
		Lv_Com(&f, "Mission_FindDtritusMatrix1_RemoteMessage4", "DeepThroat", NULL, NULL);
	}
	PyObject *fn = Sf_Mod(&f, "Scrap", "GetNearestParked");
	PyObject *door = ENT("DM_Door_Elevator_GamblinDen");
	PyObject *np[2] = { NULL, NULL };
	Sf_Unpack(&f, Sf_Call(&f, fn, "(iO)", 1, Sf_Attr(&f, door, "Pos")), 2, np);
	PyObject *e = Sf_Entity(&f, np[0]);
	if (Sf_True(&f, e) > 0) {
		Sf_Method(&f, e, "Del", "()");
	}
	PyObject *ship = Sf_Ask(&f, "Parking", "CreateParkedShip", "(Ois)", np[1], 0, "LonelyMerc");
	Sf_SetAttrS(&f, ship, "OnDeath", "Mission_FindDtritusMatrix.OnLonelyMercShipDeath");
	Sf_SetAttrI(&f, ship, "Invulnerable", 1);
	Sf_SetGlobal(&f, FD, "lonelyMercShipName", Sf_Attr(&f, ship, "Name"));
	Sf_SetGlobal(&f, FD, "prevLonelyMercShipCameraPos", Sf_Attr(&f, ship, "CamTgt"));
	Sf_SetAttr(&f, ship, "CamTgt", Sf_Build(&f, "(iii)", 0, 400, 0));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(FD, "lonelyMercShipName"));
	Sf_Own(&f, FD, "CheckLonelyMercShipExist", "()");
	return Sf_None(&f);
}

static PyObject *Dtritus_OnLonelyMercShipDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_SetGlobal(&f, FD, "lonelyMercShipDestroyed", Sf_Int(&f, 1));
	if (Sf_EqS(&f, SAVE("LonelyMercActive"), "0") > 0) {
		Lv_Com(&f, "Mission_FindDtritusMatrix1_RemoteMessage4", "DeepThroat", NULL, NULL);
	}
	return Sf_None(&f);
}

/* CheckLonelyMercShipExist(): every second: a lost ship comes back */
static PyObject *Dtritus_CheckLonelyMercShipExist(PyObject *self, PyObject *args) {
	SF_ARGS("");
	int t = Sf_True(&f, Sf_Entity(&f, G(FD, "lonelyMercShipName")));
	if (t == 0) {
		int kept = Sf_EqI(&f, G(FD, "lonelyMercShipDestroyed"), 0);
		if (kept > 0) {
			if (Sf_EqS(&f, SAVE("LonelyMercActive"), "0") > 0) {
				Lv_Com(&f, "Mission_FindDtritusMatrix1_RemoteMessage3", "DeepThroat", NULL, NULL);
				Sf_Own(&f, FD, "CreateLonelyMercShip", "()");
			}
		} else if (kept == 0) {
			Sf_SetGlobal(&f, FD, "lonelyMercShipDestroyed", Sf_Int(&f, 0));
			Sf_Own(&f, FD, "CreateLonelyMercShip", "()");
		}
	} else if (t > 0) {
		Sf_Later(&f, Sf_Int(&f, 1), G(FD, "CheckLonelyMercShipExist"), NOARGS, "CheckMercShip");
	}
	return Sf_None(&f);
}

/* TestForLonelyMercShip(): on boarding: the lonely mercenary's ship opens the lair (the parkings, the plans and the
 * matrix as targets); another ship closes the parkings unless the lair is open for good */
static PyObject *Dtritus_TestForLonelyMercShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, FD, "lastShipUsed", Sf_Attr(&f, usr, "Name"));
	PyObject *ship = Sf_Entity(&f, G(FD, "lonelyMercShipName"));
	if (Sf_True(&f, ship) > 0 && Sf_Compare(&f, G(FD, "lonelyMercShipName"), "==", Sf_Attr(&f, usr, "Name")) > 0) {
		Sf_SetAttr(&f, ship, "CamTgt", G(FD, "prevLonelyMercShipCameraPos"));
		Sf_SetAttrI(&f, ship, "Invulnerable", 0);
		Sf_Own(&f, FD, "ActivateMissionObjects", "(i)", 1);
		PyObject *targets = Sf_Build(&f, "[]");
		Fd_Active(&f, "lonelyMercParkings", 1);
		PyObject *p;
		int mark = f.n;
		for (int k = 0; (p = Sf_Next(&f, G(FD, "plans"), k)) != NULL; k++) {
			Sf_Method(&f, targets, "append", "(O)", p);
			Sf_Release(&f, mark);
		}
		if (Lv_And(&f, G(FD, "dtritusMatrix"), G(FD, "dtritusMatrixArrow")) > 0) {
			Sf_Method(&f, targets, "append", "(O)", Sf_Attr(&f, G(FD, "dtritusMatrix"), "Name"));
			Sf_Method(&f, targets, "append", "(O)", Sf_Attr(&f, G(FD, "dtritusMatrixArrow"), "Name"));
		}
		Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", targets);
		return Sf_None(&f);
	}
	if (Sf_EqS(&f, SAVE("LonelyMercLairActive"), "0") > 0) {
		Fd_Active(&f, "lonelyMercParkings", 0);
	}
	return Sf_None(&f);
}

/* CheckCreateBadGuy(): every second until the player is at the hangar doors: the bad guy comes 7 s later */
static PyObject *Dtritus_CheckCreateBadGuy(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) > 0 && Sf_Compare(&f, Sy_DistDenSqr(&f, usr), "<=", Sf_Float(&f, 216160784.0)) > 0) {
		Sf_Own(&f, FD, "BadGuyAppears", "()");
		Sf_Later(&f, Sf_Int(&f, 7), G(FD, "CreateBadGuy"), NOARGS, NULL);
		return Sf_None(&f);
	}
	Sf_Later(&f, Sf_Int(&f, 1), G(FD, "CheckCreateBadGuy"), NOARGS, "CheckCreateBadGuy");
	return Sf_None(&f);
}

/* OutOfLonelyMercShip(): leaving the lonely mercenary's ship: invulnerable only at the gamblin den's hangar; parked far
 * from the matrix it is the target again, else the lair stays open */
static PyObject *Dtritus_OutOfLonelyMercShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *ship = Sf_Entity(&f, G(FD, "lonelyMercShipName"));
	int t = Sf_True(&f, ship);
	if (t > 0) {
		t = Sf_Compare(&f, G(FD, "lonelyMercShipName"), "==", G(FD, "lastShipUsed"));
	}
	if (t <= 0) {
		return Sf_None(&f);
	}
	PyObject *distSqr = Sf_Int(&f, 12000 * 12000);
	PyObject *toDen = Sy_DistDenSqr(&f, usr);
	int c = Sf_Compare(&f, toDen, "<=", distSqr);
	Sf_SetAttr(&f, ship, "Invulnerable", c < 0 ? NULL : Sf_Int(&f, c));
	PyObject *up = Sf_Attr(&f, usr, "Pos");
	PyObject *d = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", up, Sf_Attr(&f, G(FD, "dtritusMatrixArrow"), "Pos")));
	int far = Sf_Compare(&f, d, ">", distSqr);
	if (far > 0) {
		Sf_Own(&f, FD, "ActivateMissionObjects", "(i)", 0);
		/* a local of the script (no global statement): the camera of the ship is read and dropped */
		Sf_Attr(&f, ship, "CamTgt");
		Sf_SetAttr(&f, ship, "CamTgt", Sf_Build(&f, "(iii)", 0, 400, 0));
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(FD, "lonelyMercShipName"));
	} else if (far == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "LonelyMercLairActive", "1");
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CheckMercShip");
	}
	return Sf_None(&f);
}

/* ActivateMissionObjects(status): the plans, the matrix and its arrow on or off */
static PyObject *Dtritus_ActivateMissionObjects(PyObject *self, PyObject *args) {
	PyObject *status, *name;
	SF_ARGS("O", &status);
	int mark = f.n;
	for (int k = 0; (name = Sf_Next(&f, G(FD, "plans"), k)) != NULL; k++) {
		PyObject *plan = Sf_Entity(&f, name);
		if (Sf_True(&f, plan) > 0) {
			Sf_SetAttr(&f, plan, "Active", status);
		}
		Sf_Release(&f, mark);
	}
	if (Sf_True(&f, G(FD, "dtritusMatrix")) > 0) {
		Sf_SetAttr(&f, G(FD, "dtritusMatrix"), "Active", status);
	}
	if (Sf_True(&f, G(FD, "dtritusMatrixArrow")) > 0) {
		Sf_SetAttr(&f, G(FD, "dtritusMatrixArrow"), "Active", status);
	}
	return Sf_None(&f);
}

/* CreateBadGuy(): the bad guy (a lonely mercenary) at a free spawn point, moved to the nearest one */
static PyObject *Dtritus_CreateBadGuy(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *bad = Lv_SpawnCar(&f, "TheBadGuy", 0);
	PyObject *pd = Sf_Own(&f, FD, "SetLonelyMercProfile", "()");
	Sf_Run(&f, "VehiclesNPC", "SetIAProf", "(OO)", bad, Sf_Attr(&f, pd, "Ship"));
	PyObject *pos = Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "()");
	if (Sf_True(&f, pos) > 0) {
		Sf_SetAttr(&f, bad, "Pos", pos);
	}
	Sf_SetAttrS(&f, bad, "EntityList", "Enemies");
	Lv_Life(&f, bad, FD, "LONELY_MERC_LIFE");
	return Sf_None(&f);
}

static PyObject *Dtritus_OnBadGuyDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Later(&f, Sf_Int(&f, 5), G(FD, "CreateBadGuy"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CreatePlan(dummyName, exceptString): the plan of the dummy as a photo object (off until the lair opens) */
static PyObject *Dtritus_CreatePlan(PyObject *self, PyObject *args) {
	static const char *const plans[][2] = { { "DM_Mission_TeslaPlan", "PlanoTesla" }, { "DM_Mission_LonelyMercShipPlan", "PlanoSMerc3" },
		{ "DM_Mission_LonelyMercEnginesPlan", "PlanoMMerc3" } };
	PyObject *dummyName, *exc;
	SF_ARGS("OO", &dummyName, &exc);
	Sf_Run(&f, "Scrap", "StartDummySearch", "(O)", dummyName);
	PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	int t = Sf_True(&f, dummy);
	if (t == 0) {
		Lv_RaiseValue(&f, FD, exc);
	}
	if (t <= 0) {
		return Sf_None(&f);
	}
	PyObject *pos, *ang;
	Lv_DummyPosAng(&f, dummy, &pos, &ang);
	const char *photo = NULL;
	for (int i = 0; i < 3 && !photo && !f.failed; i++) {
		if (Sf_EqS(&f, dummyName, plans[i][0]) > 0) {
			photo = plans[i][1];
		}
	}
	if (!photo) {
		Sf_Raise(&f, PyExc_NameError, "photoObjectName");
		return Sf_None(&f);
	}
	PyObject *pln = Sf_Ask(&f, "MissionsFuncs", "CreatePhotoObject", "(sOO)", photo, pos, ang);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", pln, 0);
	Sf_Method(&f, G(FD, "plans"), "append", "(O)", Sf_Attr(&f, pln, "Name"));
	Sf_SetItem(&f, G(FD, "plansDictHelper"), Sf_Attr(&f, pln, "Name"), dummyName);
	Sf_SetAttrI(&f, pln, "Active", 0);
	return Sf_None(&f);
}

/* the end of the mission once the plans and the matrix are taken: the callbacks go */
static byte Fd_EndCallbacks(scriptFrame_t *f) {
	return Sf_SetMod(f, "Parking", "SwitchVehicleMissionCallback", Py_None) && Sf_SetMod(f, "Parking", "SwitchCharMissionCallback", Py_None) &&
		Sf_SetMod(f, "CharAct", "TakenPhoto", Py_None);
}

/* TakePlanPhoto(plName): a photo of a plan: its blueprint into the hangar lists; the last thing taken ends the mission */
static PyObject *Dtritus_TakePlanPhoto(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	Sf_Import(&f, "CharAct");
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "Parking");
	int in = Sf_In(&f, name, G(FD, "plans"));
	if (in == 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	Sf_Run(&f, "MissionsFuncs", "UnsetPhotoObject", "(O)", Sf_Entity(&f, name));
	Sf_Method(&f, G(FD, "plans"), "remove", "(O)", name);
	PyObject *dummyName = Sf_GetItem(&f, G(FD, "plansDictHelper"), name);
	if (Sf_EqS(&f, dummyName, "DM_Mission_TeslaPlan") > 0) {
		Lv_Blueprint(&f, FD, "Hangar.availableWeaponsList", "Tesla", "Mission_FindDtritusMatrix1_SystemMessage1", 1);
	} else if (!f.failed && Sf_EqS(&f, dummyName, "DM_Mission_LonelyMercShipPlan") > 0) {
		Lv_Blueprint(&f, FD, "Hangar.shipsToEditList", "SMerc3", "Mission_FindDtritusMatrix1_SystemMessage2", 1);
	} else if (!f.failed && Sf_EqS(&f, dummyName, "DM_Mission_LonelyMercEnginesPlan") > 0) {
		Lv_Blueprint(&f, FD, "Hangar.availableEnginesList", "MMERC3", "Mission_FindDtritusMatrix1_SystemMessage3", 1);
	}
	if (Sf_True(&f, Sf_Own(&f, FD, "CheckEndOfMission", "()")) > 0) {
		Fd_EndCallbacks(&f);
		Sf_CallKw(&f, Sf_Mod(&f, "Speech", "ForceRemoteCom"), Py_BuildValue("(ss)", "Mission_FindDtritusMatrix1_RemoteMessage5", "DeepThroat"),
			Sf_Build(&f, "{s:O}", "AfterRemoteComFunc", G(FD, "SetNextMission")));
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* CreateDtritusMatrix(): Dtritus' matrix and its arrow at the dummy (off until the lair opens) */
static PyObject *Dtritus_CreateDtritusMatrix(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *dummy = Lv_Dummy(&f, "DM_Mission_DtritusMatrix", 1);
	PyObject *pos = NULL, *ang;
	int t = Sf_True(&f, dummy);
	if (t > 0) {
		Lv_DummyPosAng(&f, dummy, &pos, &ang);
	} else if (t == 0) {
		Sf_RaiseText(&f, "No existe el dummy de la matriz de Dtritus");
	}
	if (f.failed) {
		return Sf_None(&f);
	}
	PyObject *m = Lv_CreateAt(&f, Sf_Str(&f, "DTritusMatrix"), pos, "Item");
	Sf_SetGlobal(&f, FD, "dtritusMatrix", m);
	Sf_Method(&f, m, "SetModel", "(O)", G(FD, "MatrixModel"));
	Sf_SetAttrI(&f, m, "Radius", 100);
	Sf_SetAttrI(&f, m, "AppearsIfFree", 0);
	Sf_SetAttrI(&f, m, "AutoFace", 0);
	Sf_SetAttrI(&f, m, "OnlyUser", 1);
	Sf_SetAttr(&f, m, "rTime", Sf_Float(&f, 0.25));
	Sf_SetAttrS(&f, m, "OnTake", "Mission_FindDtritusMatrix.TakeMatrix");
	Sf_SetAttrI(&f, m, "Active", 0);
	PyObject *a = Lv_CreateAt(&f, Sf_Str(&f, "MissionCheckArrow"), pos, "WithLife");
	Sf_SetGlobal(&f, FD, "dtritusMatrixArrow", a);
	Sf_SetAttr(&f, a, "CamTgt", Sf_Build(&f, "(iii)", 0, 150, 0));
	Sf_SetAttrI(&f, a, "Active", 0);
	return Sf_None(&f);
}

/* TakeMatrix(ItemName, CharName): the matrix taken where the arrow allows it; the last thing taken ends the mission */
static PyObject *Dtritus_TakeMatrix(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	Sf_Import(&f, "CharAct");
	Sf_Import(&f, "Parking");
	if (Lv_UsrNameIs(&f, USR(), ch) <= 0) {
		return Sf_None(&f);
	}
	PyObject *arrow = G(FD, "dtritusMatrixArrow");
	int ok = Sf_True(&f, arrow);
	if (ok > 0) {
		ok = Sf_EqI(&f, Sf_Attr(&f, arrow, "ArrowCanMission"), 1);
	}
	if (ok <= 0) {
		return Sf_None(&f);
	}
	Sf_SetAttrI(&f, G(FD, "dtritusMatrix"), "rTime", 20);
	Sf_Later(&f, Sf_Int(&f, 5), Sf_Attr(&f, G(FD, "dtritusMatrix"), "Del"), NOARGS, NULL);
	Sf_Method(&f, G(FD, "dtritusMatrixArrow"), "Del", "()");
	Sf_SetGlobal(&f, FD, "dtritusMatrix", Py_None);
	Sf_SetGlobal(&f, FD, "dtritusMatrixArrow", Py_None);
	if (Sf_True(&f, Sf_Own(&f, FD, "CheckEndOfMission", "()")) > 0) {
		Fd_EndCallbacks(&f);
		Lv_Com(&f, "Mission_FindDtritusMatrix1_RemoteMessage5", "DeepThroat", G(FD, "SetNextMission"), NULL);
	}
	return Sf_None(&f);
}

/* CheckEndOfMission(): no matrix, no arrow, no plans left */
static PyObject *Dtritus_CheckEndOfMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	int r = Sf_Compare(&f, G(FD, "dtritusMatrix"), "==", Py_None);
	if (r > 0) {
		r = Sf_Compare(&f, G(FD, "dtritusMatrixArrow"), "==", Py_None);
	}
	if (r > 0) {
		int n = Sf_Len(&f, G(FD, "plans"));
		r = n < 0 ? -1 : n == 0;
	}
	return r < 0 ? Sf_None(&f) : Sf_Return(&f, Sf_Int(&f, r));
}

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

static PyObject *Berto_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, MB, "[\"SMerc1\",\"SMerc2\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBetty1\",\"SArchbishop1\"]");
}

/* Init(LevelPath) of the two: (the head of the evil Berto,) a free phone cab as the target */
static PyObject *Sy_PhoneInit(PyObject *args, const char *module, const char *use, byte berto) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "PhoneCab");
	if (berto) {
		Sf_Run(&f, "SScorer", "AddModel", "(isssiii)", 0, "EvilBerto", "Models/Heads/HeadEvilBerto/HeadEvilBerto", "", 4, 25, 10);
	}
	Sf_SetGlobal(&f, module, "pc", Sf_Ask(&f, "PhoneCab", "GetFree", "()"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(module, "pc"), "Name"));
	Sf_SetAttrS(&f, G(module, "pc"), "UseFunc", use);
	return Sf_None(&f);
}

static PyObject *Berto_Init(PyObject *self, PyObject *args) {
	(void)self;
	return Sy_PhoneInit(args, MB, "Mission_MeetBerto.UsePhoneCab", 1);
}

static PyObject *Berto_UsePhoneCab(PyObject *self, PyObject *args) {
	static const syPhone_t p = { "Generics_StillPolice1_Berto", "Berto", "SetNextMission", "DeepThroat",
		"Mission_MeetBerto_ComDeepThroat_NI_1Msg", 1, 0 };
	(void)self;
	return Sy_UsePhone(args, MB, &p);
}

/* SetNextMission(): the Bertos come after the player from now on, Dtritus loses his matrix */
static PyObject *Berto_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Lv_PcDefault(&f, MB);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "CreateBertos", "1");
	Sf_Run(&f, "Scrap", "CreateSaveVar", "(sO)", "BertosProfile", G(MB, "BERTO_PROFILE"));
	Sf_Run(&f, "Scrap", "CreateSaveVar", "(sO)", "BertosLife", Sf_Repr(&f, G(MB, "BERTO_LIFE")));
	Sf_Run(&f, "MissionsFuncs", "CreateBertos", "()");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Sf_Mod(&f, "MissionsFuncs", "ReassignBertosPlayerTarget"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Sf_Mod(&f, "MissionsFuncs", "ReassignBertosPlayerTarget"));
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_DestroyStupidus", "Commercial");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_DestroyStupidus_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	PyObject *chars = Lv_SaveList(&f, "Conversor.AvailableChars", MB);
	if (Sf_In(&f, Sf_Str(&f, "Dtritus"), chars) > 0) {
		Sf_Method(&f, chars, "remove", "(s)", "Dtritus");
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Conversor.AvailableChars", Sf_Repr(&f, chars));
	return Sf_None(&f);
}

PyMethodDef g_ScrapyardMeetBertoMethods[] = {
	{ "InitStatus", Berto_InitStatus, 1, NULL },
	{ "Init", Berto_Init, 1, NULL },
	{ "UsePhoneCab", Berto_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Berto_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

static PyObject *Throat_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, TD, "[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SMerc3\",\"SLMerc\",\"SMayor1\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\",\"SBankMaster1\",\"SArchbishop1\"]");
}

static PyObject *Throat_Init(PyObject *self, PyObject *args) {
	(void)self;
	return Sy_PhoneInit(args, TD, "Mission_TalkToDeepThroat.UsePhoneCab", 0);
}

static PyObject *Throat_UsePhoneCab(PyObject *self, PyObject *args) {
	static const syPhone_t p = { "Generics_StillPolice1_DeepThroat", "DeepThroat", "SetNextMission", "DeepThroat",
		"Mission_TalkToDeepThroat10_ComDeepThroat_NI_1Msg", 1, 0 };
	(void)self;
	return Sy_UsePhone(args, TD, &p);
}

static PyObject *Throat_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Lv_PcDefault(&f, TD);
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_DestroyBadDebtors", "Outskirts");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_MeetBankMaster_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

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

static PyObject *Clue_GetMosquitoProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, MC, mc_mosquito_profile);
}

static PyObject *Clue_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "MapTriggers");
	Sf_SetMod(&f, "Traffic", "NumNormal", Sf_Int(&f, 5));
	Sf_SetMod(&f, "Traffic", "NumMedium", Sf_Int(&f, 5));
	Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 5));
	Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
	Sf_SetMod(&f, "Traffic", "AcShips", Sf_Build(&f, "[s]", "SMosquito"));
	Sf_SetMod(&f, "MapTriggers", "WithScrapyardInhabitants", Sf_Int(&f, 0));
	Lv_AcShips(&f, MC, ac_ships_std);
	return Sf_None(&f);
}

/* Init(LevelPath): the swarm (the lairs from their dummies, the police boss' message the first time) or the call */
static PyObject *Clue_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(Oiii)", G(MC, "Model_Datapack"), 1, 1, 1);
	PyObject *svv = SAVE("Mission_MosquitoClue");
	int swarm = Sf_EqS(&f, svv, "Swarm");
	if (swarm > 0) {
		PyObject *dummy = Lv_Dummy(&f, "DM_Mission_Lair*", 1);
		int mark = f.n;
		while (Sf_True(&f, dummy) > 0) {
			Sf_Method(&f, G(MC, "lairsList"), "append", "(O)", Sf_Item(&f, dummy, 1));
			PyObject *next = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
			Sf_ReleaseKeep(&f, mark, &next, 1);
			dummy = next;
		}
		int first = Sf_EqS(&f, SAVE("Mission_MosquitoClue.NumLairsExplored"), "0");
		if (first > 0) {
			Lv_Com(&f, "Mission_Get1stClue_RemoteMessage1", "PoliceBoss", G(MC, "InitSwarm"), NOARGS);
		} else if (first == 0) {
			Sf_Own(&f, MC, "InitSwarm", "()");
		}
	} else if (swarm == 0 && Sf_EqS(&f, svv, "PoliChat") > 0) {
		Sf_Own(&f, MC, "InitPoliChat", "()");
	}
	return Sf_None(&f);
}

/* InitSwarm(): the arrow and the launcher item of the next lair */
static PyObject *Clue_InitSwarm(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *n = Sf_ToInt(&f, SAVE("Mission_MosquitoClue.NumLairsExplored"));
	PyObject *pos = Sf_GetItem(&f, G(MC, "lairsList"), n);
	Sf_SetGlobal(&f, MC, "checkArrow", Lv_CreateAt(&f, Sf_Str(&f, "MosquitosLauncherCheckArrow"), pos, "WithLife"));
	Sf_SetAttr(&f, G(MC, "checkArrow"), "CamTgt", Sf_Build(&f, "(iii)", 0, 500, 0));
	PyObject *l = Lv_CreateAt(&f, Sf_Str(&f, "MosquitosLauncher"), pos, "Item");
	Sf_SetAttrI(&f, l, "Radius", 20000);
	Sf_SetAttrS(&f, l, "OnTake", "Mission_MosquitoClue.InitMosquito");
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(MC, "checkArrow"), "Name"));
	return Sf_None(&f);
}

/* InitMosquito(ItemName, CharName): the launcher taken: the swarm of the lair, one mosquito every half second */
static PyObject *Clue_InitMosquito(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	PyObject *n = Sf_ToInt(&f, SAVE("Mission_MosquitoClue.NumLairsExplored"));
	if (Sf_EqI(&f, n, 1) > 0) {
		Lv_Com(&f, "Mission_Get1stClue_RemoteMessage2", "PoliceBoss", NULL, NULL);
	} else if (!f.failed && Sf_EqI(&f, n, 2) > 0) {
		Lv_Com(&f, "Mission_Get1stClue_RemoteMessage3", "PoliceBoss", NULL, NULL);
	}
	PyObject *it = Sf_Entity(&f, item);
	Sf_SetAttrI(&f, it, "rTime", 20);
	Sf_Later(&f, Sf_Int(&f, 5), Sf_Attr(&f, it, "Del"), NOARGS, NULL);
	Sf_Method(&f, Sf_Entity(&f, Sf_Cat(&f, "Os", item, "CheckArrow")), "Del", "()");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_MosquitoClue.MosquitoDeath");
	PyObject *pos = Sf_GetItem(&f, G(MC, "lairsList"), n);
	PyObject *range = Sy_Range(&f, G(MC, "numMosquitosInLair"));
	PyObject *i;
	int mark = f.n;
	for (int k = 0; (i = Sf_Next(&f, range, k)) != NULL; k++) {
		int last = Sf_Compare(&f, i, "==", Sf_OpI(&f, G(MC, "numMosquitosInLair"), '-', 1));
		if (last >= 0) {
			PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Op(&f, Sf_Float(&f, 0.5), '*', i));
			Sf_Schedule(&f, t, G(MC, "CreateMosquito"), Sf_Build(&f, "(Oi)", pos, last), NULL);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* CreateMosquito(pos, isLastMosquito): a mosquito of the lair; the last one and every other one drop nothing */
static PyObject *Clue_CreateMosquito(PyObject *self, PyObject *args) {
	PyObject *pos, *last;
	SF_ARGS("OO", &pos, &last);
	Sf_Import(&f, "OutMap");
	PyObject *o1 = Lv_SpawnCar(&f, "Traffic", 0);
	Lv_StdProfile(&f, MC, "GetMosquitoProfile", o1);
	Sf_SetAttr(&f, o1, "Pos", pos);
	Sf_SetAttrS(&f, o1, "EntityList", "Enemies");
	Sf_SetAttr(&f, o1, "Ammo00", Sf_Ask(&f, "SWeap", "GetFAmmo", "(is)", 0, "Max"));
	Lv_Life(&f, o1, MC, "MOSQUITO_LIFE");
	int isLast = Sf_EqI(&f, last, 1);
	if (isLast > 0) {
		Sf_SetAttrI(&f, o1, "Money", -1);
	} else if (isLast == 0) {
		int none = Sf_EqI(&f, G(MC, "WithAmmo"), 0);
		if (none >= 0) {
			Sf_SetAttrI(&f, o1, "Money", none ? -1 : 25);
		}
		Sf_SetGlobal(&f, MC, "WithAmmo", Sf_Op(&f, Sf_Int(&f, 1), '-', G(MC, "WithAmmo")));
	}
	return Sf_None(&f);
}

/* MosquitoDeath(MainCharnName, Param2): money for each mosquito; the swarm done: the data pack after the third lair,
 * else the next lair */
static PyObject *Clue_MosquitoDeath(PyObject *self, PyObject *args) {
	PyObject *name, *p2;
	SF_ARGS("OO", &name, &p2);
	Sf_SetGlobal(&f, MC, "numMosquitosKilled", Sf_OpI(&f, G(MC, "numMosquitosKilled"), '+', 1));
	PyObject *money = Sf_Ask(&f, "Scrap", "GetMoney", "()");
	Sf_Run(&f, "Scrap", "SetMoney", "(O)", Sf_Op(&f, money, '+', Sf_Ask(&f, "Scrap", "Get", "(s)", "OnMosquitoDeathMoney")));
	Sf_Run(&f, "Scrap", "IncSaveVar", "(s)", "Stats.Mosquito");
	if (Sf_Compare(&f, G(MC, "numMosquitosKilled"), "==", G(MC, "numMosquitosInLair")) <= 0) {
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, MC, "numMosquitosKilled", Sf_Int(&f, 0));
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, SAVE("Mission_MosquitoClue.NumLairsExplored")), '+', 1);
	int third = Sf_EqI(&f, n, 3);
	if (third > 0) {
		PyObject *pos = Sf_Attr(&f, Sf_Entity(&f, name), "Pos");
		PyObject *l = Lv_CreateAt(&f, Sf_Str(&f, "MissionDataPack"), pos, "Item");
		Sf_Method(&f, l, "SetModel", "(O)", G(MC, "Model_Datapack"));
		Sf_SetAttrI(&f, l, "Radius", 3500);
		Sf_SetAttrI(&f, l, "AppearsIfFree", 0);
		Sf_SetAttrI(&f, l, "AutoFace", 0);
		Sf_SetAttrI(&f, l, "OnlyUser", 1);
		Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
		Sf_SetAttrS(&f, l, "OnTake", "Mission_MosquitoClue.TakeDataPack");
		Sf_Import(&f, "Items");
		Sf_Run(&f, "Items", "SetSound", "(Os)", l, "TakeBDataPack");
		PyObject *l2 = Lv_CreateAt(&f, Sf_Str(&f, "MissionCheckArrow"), pos, "WithLife");
		Sf_SetAttr(&f, l2, "CamTgt", Sf_Build(&f, "(iii)", 0, 1000, 0));
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, l2, "Name"));
		Lv_Com(&f, "Mission_Get1stClue_RemoteMessage4", "PoliceBoss", NULL, NULL);
	} else if (third == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_MosquitoClue.NumLairsExplored", Sf_Repr(&f, n));
		Lv_Com(&f, "Mission_Get1stClue_RemoteMessage6", "PoliceBoss", NULL, NULL);
		Sf_Own(&f, MC, "InitSwarm", "()");
	}
	return Sf_None(&f);
}

/* TakeDataPack(ItemName, UsrName): the clue: the police boss' message, then the call */
static PyObject *Clue_TakeDataPack(PyObject *self, PyObject *args) {
	PyObject *item, *usr;
	SF_ARGS("OO", &item, &usr);
	PyObject *it = ENT("MissionDataPack");
	Sf_SetAttrI(&f, it, "rTime", 20);
	Sf_Later(&f, Sf_Int(&f, 5), Sf_Attr(&f, it, "Del"), NOARGS, NULL);
	Sf_Method(&f, ENT("MissionCheckArrow"), "Del", "()");
	Sf_Run(&f, "Scrap", "Set", "(si)", "RemainTalkTime", 0);
	Lv_Com(&f, "Mission_Get1stClue_RemoteMessage5", "PoliceBoss", G(MC, "InitPoliChat"), NULL);
	return Sf_None(&f);
}

/* InitPoliChat(): call the police boss from a free phone cab */
static PyObject *Clue_InitPoliChat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_MosquitoClue", "PoliChat");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_Get2ndClue_MissionText1", 1);
	Sf_SetGlobal(&f, MC, "pc", Sf_Ask(&f, "PhoneCab", "GetFree", "()"));
	Sf_SetAttrS(&f, G(MC, "pc"), "UseFunc", "Mission_MosquitoClue.UsePhoneCab");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(MC, "pc"), "Name"));
	return Sf_None(&f);
}

static PyObject *Clue_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Bank", "Journalist.Betty_Defaut", "Bank", "Journalist.Berto_Defaut", "Bank", NULL };
	SF_ARGS("");
	Lv_PcDefault(&f, MC);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sy_ApplyNext(&f, MC);
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetRudolph_MissionText1", 1);
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "Mission_MosquitoClue");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "Mission_MosquitoClue.NumLairsExplored");
	Lv_Saves(&f, places);
	return Sf_None(&f);
}

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

/*
 * GotoNext() of the two: every 2 s: the mercenary goes on to his next waypoint when the player is near (again: the
 * smart mercenaries come at the third, a waypoint of 3000 with state 1; else 3000, 0, 0.25 and the schedule named
 * "GoToNext"); after the last one his plans appear; the first time the player falls behind he gets a hint
 */
static PyObject *Sy_GotoNext(PyObject *args, const char *module, byte again) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *o1 = Sf_Entity(&f, G(module, "MercName"));
	int t = Sf_True(&f, o1);
	if (t == 0) {
		if (again) {
			Sf_Own(&f, module, "CreateMerc", "()");
		} else {
			Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "GoToNext");
		}
		return Sf_None(&f);
	}
	if (t < 0) {
		return Sf_None(&f);
	}
	PyObject *usr = USR();
	if (Sf_True(&f, usr) > 0) {
		int go = Sf_EqI(&f, G(module, "iWaypoint"), 0);
		if (go == 0) {
			go = Sf_EqI(&f, Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", G(module, "MercName")), 0);
			if (go > 0) {
				go = Sf_Compare(&f, Sy_Dist(&f, usr, o1), "<", Sf_Int(&f, 15000));
			}
		}
		if (go > 0) {
			PyObject *wp = G(module, "iWaypoint");
			int n = Sf_Len(&f, G(module, "MercWaypoints"));
			int end = Sf_Compare(&f, wp, ">=", Sf_Int(&f, n));
			if (end > 0) {
				if (again) {
					Lv_Com(&f, "Mission_FollowStupidMerc_RemoteMessage6", "MercenaryB", G(module, "CreateMercPlans"), NOARGS);
				} else {
					Lv_Com(&f, "Mission_FollowStupidMerc_RemoteMessage6", "MercenaryB", G(module, "CreateATPCPlans"),
						Sf_Build(&f, "(O)", Sf_Attr(&f, o1, "Name")));
				}
				Sf_SetAttrS(&f, o1, "TgType", "None");
				Sf_SetAttrS(&f, o1, "EntityList", "Traffic");
				Sf_SetAttrS(&f, o1, "OnDeath", "Vehicles.OnDeath");
				return Sf_None(&f);
			}
			if (end < 0) {
				return Sf_None(&f);
			}
			int third = again ? Sf_EqI(&f, G(module, "iWaypoint"), 3) : 0;
			if (third > 0) {
				Sf_Own(&f, module, "CreateMercs", "()");
			} else if (third == 0 && Sf_Compare(&f, G(module, "iWaypoint"), ">", Sf_Int(&f, 1)) > 0) {
				int rm = Sf_True(&f, G(module, "rmmess"));
				if (rm > 0) {
					Lv_Com(&f, "Mission_FollowStupidMerc_RemoteMessage3", "MercenaryB", NULL, NULL);
				} else if (rm == 0) {
					Lv_Com(&f, "Mission_FollowStupidMerc_RemoteMessage5", "MercenaryB", NULL, NULL);
				}
				rm = Sf_True(&f, G(module, "rmmess"));
				Sf_SetGlobal(&f, module, "rmmess", rm < 0 ? NULL : Sf_Int(&f, !rm));
			}
			PyObject *pos = Sf_GetItem(&f, G(module, "MercWaypoints"), G(module, "iWaypoint"));
			PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
			if (again) {
				Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOOOii)", 1, G(module, "MercName"), x, y, z, 3000, 1);
			} else {
				Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOOOiid)", 1, G(module, "MercName"), x, y, z, 3000, 0, 0.25);
			}
			Sf_SetGlobal(&f, module, "iWaypoint", Sf_OpI(&f, G(module, "iWaypoint"), '+', 1));
		} else if (go == 0) {
			int hint = Sf_Compare(&f, G(module, "iWaypoint"), ">", Sf_Int(&f, 1));
			if (hint > 0) {
				hint = Sf_True(&f, G(module, "FirstHint"));
			}
			if (hint > 0 && Sf_EqI(&f, Sf_Ask(&f, "SAI", "GetStateVehicle", "(O)", G(module, "MercName")), 0) > 0) {
				Sf_SetGlobal(&f, module, "FirstHint", Sf_Int(&f, 0));
				Sf_Run(&f, "Speech", "RemoteComunication", "(ss)", "Mission_FollowStupidMerc_RemoteMessage2", "MercenaryB");
			}
		}
	}
	Sf_Later(&f, Sf_Float(&f, 2.0), G(module, "GotoNext"), NOARGS, again ? NULL : "GoToNext");
	return Sf_None(&f);
}

/* the dummy of a blueprint as a photo object (the mission target goes first); NULL and the console error when missing */
static PyObject *Sy_PlanPhoto(scriptFrame_t *f, const char *module, const char *dummyName, const char *model, const char *taken) {
	char msg[0x80];
	Sf_Run(f, "Scrap", "StartDummySearch", "(si)", dummyName, 0);
	PyObject *dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
	int t = Sf_True(f, dummy);
	if (t == 0) {
		Str_Printf(msg, sizeof(msg), "ERROR : %s not found! \n", dummyName);
		Sf_Run(f, "Scrap", "ConsoleError", "(s)", msg);
	}
	if (t <= 0) {
		return NULL;
	}
	Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Run(f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
	PyObject *pos, *ang;
	Lv_DummyPosAng(f, dummy, &pos, &ang);
	PyObject *prf = Sf_Ask(f, "MissionsFuncs", "CreatePhotoObject", "(sOO)", model, pos, ang);
	Sf_SetMod(f, "CharAct", "TakenPhoto", Sf_Global(f, module, taken));
	return prf;
}

static const char *const sm_mosquito_profile[] = { "CadenceShootCM=7", "UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10",
	"UsePercHook=0", "FactorHookMap=0", "CadenceShoot=15", "ControlAim=3", "AngAttack=1.2", "CounterAttack=0.2", "ControlBoost=0",
	"ControlBoostItemLife=0", "ControlBoostItemAmmo=0", "FactorDamageGetAway=0.90", "FactorDamageItemLife=0.05", "FactorItemAmmo=0.05",
	"ControlStrafeEvasion=0", "UsePercHookGetAway=0", "TimeMaxStrafe=2", "VelMaxStrafe=0.25", NULL };

/* InitStatus(LevelPath): no mercenaries in the traffic, the lairs launch from farther */
static PyObject *Merc_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "VehiclesNPC");
	Sf_SetMod(&f, "VehiclesNPC", "UseMercs", Sf_Int(&f, 0));
	Lv_AcShips(&f, SM, ac_ships_std);
	Sf_Run(&f, "Scrap", "Set", "(sd)", "MaxMosquitosDist", 60000.0);
	Sf_Run(&f, "Scrap", "Set", "(sd)", "MaxMadHuntersDist", 90000.0);
	return Sf_None(&f);
}

/* Init(LevelPath): the mercenary 10 s later at his first waypoint, the mosquitos reprogrammed, the life items */
static PyObject *Merc_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Later(&f, Sf_Float(&f, 10.0), G(SM, "CreateMerc"), Sf_Build(&f, "(ii)", 0, 1), NULL);
	PyObject *aiprof = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(s)", "SMosquito");
	Sf_SetAttrs(&f, aiprof, SM, sm_mosquito_profile);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(siii)", "Models/Misc/Blueprint/PlanoRail.m3d", 1, 1, 1);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnItemTakenAI", "Mission_StupidMerc.CheckAITakesLife");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnLifeItemTaken", "Mission_StupidMerc.CheckLife");
	return Sf_None(&f);
}

/* CreateMerc(killedByPlayer = 0, firstfit = 0): the stupid mercenary (he alone launches the lairs) on his way */
static PyObject *Merc_CreateMerc(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "killedByPlayer", "firstfit", NULL };
	PyObject *killed = NULL, *firstfit = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|OO", kwlist, &killed, &firstfit)) {
		return NULL;
	}
	killed = killed ? killed : Sf_Int(&f, 0);
	firstfit = firstfit ? firstfit : Sf_Int(&f, 0);
	Sf_Import(&f, "MapTriggers");
	PyObject *o1 = Sf_CallKw(&f, Sf_Mod(&f, "VehiclesNPC", "MakeAI"), Py_BuildValue("(ss)", "SMerc1", "Traffic"),
		Sf_Build(&f, "{s:i}", "InitState", 0));
	Sf_SetAttrI(&f, o1, "Money", -1);
	Sf_SetAttrS(&f, o1, "TgType", "green");
	Sf_SetAttrS(&f, o1, "OnDeath", "Mission_StupidMerc.OnStupidMercDeath");
	Sf_SetAttrS(&f, o1, "OnDamage", "Mission_StupidMerc.OnStupidMercDamage");
	Lv_Life(&f, o1, SM, "MERC_LIFE");
	Sf_SetGlobal(&f, SM, "MercName", Sf_Attr(&f, o1, "Name"));
	if (Sf_True(&f, firstfit) > 0) {
		Sf_SetAttr(&f, o1, "Pos", Sf_Item(&f, G(SM, "MercWaypoints"), 0));
	}
	Sf_SetMod(&f, "MapTriggers", "shipsAllowedToLaunchMosquitos", Sf_Build(&f, "[O]", G(SM, "MercName")));
	Sf_SetMod(&f, "MapTriggers", "shipsAllowedToLaunchMadHunters", Sf_Build(&f, "[O]", G(SM, "MercName")));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", G(SM, "MercName"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, o1, "Name"));
	int first = Sf_True(&f, G(SM, "FirstAppereance"));
	if (first > 0) {
		Lv_Com(&f, "Mission_GetMercShipPlans_RemoteMessage1", "MercenaryB", NULL, NULL);
		Sf_SetGlobal(&f, SM, "FirstAppereance", Sf_Int(&f, 0));
	} else if (first == 0) {
		int k = Sf_True(&f, killed);
		if (k > 0) {
			Lv_Com(&f, "Mission_FollowStupidMerc_RemoteMessage4", "MercenaryB", NULL, NULL);
		} else if (k == 0) {
			Lv_Com(&f, "Mission_FollowStupidMerc_RemoteMessage8", "MercenaryB", NULL, NULL);
		}
	}
	Sf_SetGlobal(&f, SM, "iWaypoint", Sf_Int(&f, 0));
	Sf_Own(&f, SM, "GotoNext", "()");
	return Sf_Return(&f, o1);
}

/* OnStupidMercDeath(LooserName, WinnerName): he comes back 2 s later (killed by the player or not) */
static PyObject *Merc_OnStupidMercDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	PyObject *usr = USR();
	int t = Sf_True(&f, usr);
	PyObject *by = usr;
	if (t > 0) {
		int c = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", winner);
		by = c < 0 ? NULL : Sf_Int(&f, c);
	}
	Sf_Later(&f, Sf_Int(&f, 2), G(SM, "CreateMerc"), t < 0 || !by ? NULL : Sf_Build(&f, "(O)", by), NULL);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "GoToNext");
	return Sf_None(&f);
}

/* the nearest life item of the mercenary: SAI.SetStateVehicle(1, name, pos[0], pos[1], pos[2], radius); 1 when there
 * is one, 0, -1 */
static int Sm_GoForLife(scriptFrame_t *f, PyObject *name, PyObject *looser) {
	PyObject *item = Sf_Entity(f, Sf_Ask(f, "SAI", "GetNearestItemLife", "(O)", looser));
	int t = Sf_True(f, item);
	if (t > 0) {
		PyObject *pos = Sf_Attr(f, item, "Pos");
		PyObject *radius = Sf_Attr(f, item, "Radius");
		PyObject *x = Sf_Item(f, pos, 0), *y = Sf_Item(f, pos, 1), *z = Sf_Item(f, pos, 2);
		Sf_Run(f, "SAI", "SetStateVehicle", "(iOOOOO)", 1, name, x, y, z, radius);
	}
	return f->failed ? -1 : t;
}

/* OnStupidMercDamage(LooserName, WinnerName, Damage): low on life he goes for the nearest life item */
static PyObject *Merc_OnStupidMercDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *e = Sf_Entity(&f, looser);
	int go = Sf_True(&f, e);
	if (go > 0) {
		go = Sf_EqI(&f, G(SM, "gettingLife"), 0);
	}
	if (go > 0) {
		PyObject *life = Sf_Attr(&f, e, "Life");
		go = Sf_Compare(&f, life, "<", Sf_Op(&f, G(SM, "percStupidMercGoesForLife"), '*', Sf_Attr(&f, e, "MaxLife")));
	}
	if (go <= 0) {
		return Sf_None(&f);
	}
	PyObject *item = Sf_Entity(&f, Sf_Ask(&f, "SAI", "GetNearestItemLife", "(O)", looser));
	if (Sf_True(&f, item) > 0) {
		Sf_SetGlobal(&f, SM, "mercLife", Sf_Op(&f, Sf_Attr(&f, e, "Life"), '-', damage));
		if (Sf_Compare(&f, G(SM, "mercLife"), ">", Sf_Int(&f, 0)) > 0) {
			PyObject *pos = Sf_Attr(&f, item, "Pos");
			PyObject *radius = Sf_Attr(&f, item, "Radius");
			Sf_SetGlobal(&f, SM, "mercName", looser);
			Sf_SetGlobal(&f, SM, "gettingLife", Sf_Int(&f, 1));
			PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOOOO)", 1, looser, x, y, z, radius);
		}
	}
	return Sf_None(&f);
}

/* name == mercName and gettingLife == 1 */
static int Sm_Getting(scriptFrame_t *f, PyObject *name) {
	int t = Sf_Compare(f, name, "==", Sf_Global(f, SM, "mercName"));
	return t > 0 ? Sf_EqI(f, Sf_Global(f, SM, "gettingLife"), 1) : t;
}

/* CheckAITakesLife(name): an AI took an item: it goes on; the mercenary going for life is checked a second later */
static PyObject *Merc_CheckAITakesLife(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	if (Sf_True(&f, name) > 0) {
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iO)", 0, name);
		if (Sm_Getting(&f, name) > 0) {
			Sf_Later(&f, Sf_Int(&f, 1), G(SM, "CheckLife"), Sf_Build(&f, "(O)", name), NULL);
		}
	}
	return Sf_None(&f);
}

/* CheckLife(name): the mercenary got no life yet: the next item; else back to his waypoint */
static PyObject *Merc_CheckLife(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	if (Sm_Getting(&f, name) <= 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) != 1) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, Sf_Attr(&f, e, "Life"), "<=", G(SM, "mercLife")) > 0 && Sm_GoForLife(&f, name, name) > 0) {
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, SM, "gettingLife", Sf_Int(&f, 0));
	PyObject *wps = G(SM, "MercWaypoints");
	int end = Sf_Compare(&f, G(SM, "iWaypoint"), ">=", Sf_Int(&f, Sf_Len(&f, wps)));
	PyObject *pos = NULL;
	if (end > 0) {
		pos = Sf_Item(&f, G(SM, "MercWaypoints"), Sf_Len(&f, G(SM, "MercWaypoints")) - 1);
	} else if (end == 0) {
		pos = Sf_GetItem(&f, G(SM, "MercWaypoints"), G(SM, "iWaypoint"));
	}
	PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOOOiid)", 1, name, x, y, z, 3000, 0, 0.25);
	return Sf_None(&f);
}

static PyObject *Merc_GotoNext(PyObject *self, PyObject *args) {
	(void)self;
	return Sy_GotoNext(args, SM, 0);
}

/* CreateATPCPlans(mercName): the mercenary stops, the ATPC blueprint is the target */
static PyObject *Merc_CreateATPCPlans(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	if (Sf_True(&f, Sf_Entity(&f, name)) > 0) {
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iO)", 3, name);
	}
	PyObject *prf = Sy_PlanPhoto(&f, SM, "DM_Mission_ATPC_Blueprint", "PlanoRail", "AddATPCPlans");
	if (prf) {
		Sf_SetGlobal(&f, SM, "ATPCPlansName", Sf_Attr(&f, prf, "Name"));
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(SM, "ATPCPlansName"));
	}
	return Sf_None(&f);
}

/* AddATPCPlans(proofName): the photo of the blueprint: the ATPC into the hangar, Deep Throat's message, the next mission */
static PyObject *Merc_AddATPCPlans(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	Sf_Import(&f, "RaceMaker");
	int c = Sf_Compare(&f, G(SM, "ATPCPlansName"), "==", name);
	if (c == 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	if (c < 0) {
		return Sf_None(&f);
	}
	Lv_Blueprint(&f, SM, "Hangar.availableWeaponsList", "ATPC", NULL, 0);
	Sf_SetAttrI(&f, Sf_Entity(&f, G(SM, "ATPCPlansName")), "CanPhoto", 0);
	Sf_SetMod(&f, "CharAct", "TakenPhoto", Py_None);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", LANG("Mission_FollowStupidMerc_SystemMessage1"));
	Lv_Com(&f, "Mission_FollowStupidMerc_RemoteMessage7", "DeepThroat", G(SM, "SetNextMission"), NOARGS);
	return Sf_Return(&f, Sf_Int(&f, 1));
}

static PyObject *Merc_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnItemTakenAI", "");
	Sf_Run(&f, "Scrap", "Set", "(sO)", "MaxMosquitosDist", Sf_Ask(&f, "Scrap", "Def", "(s)", "MaxMosquitosDist"));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "MaxMadHuntersDist", Sf_Ask(&f, "Scrap", "Def", "(s)", "MaxMadHuntersDist"));
	Sy_ApplyNext(&f, SM);
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_InvulnerableTransport_MissionText1", 1);
	return Sf_None(&f);
}

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

/* GetMercsEnemiesProfile(TypeShip): the smart mercenaries */
static PyObject *Again_GetMercsEnemiesProfile(PyObject *self, PyObject *args) {
	PyObject *type;
	SF_ARGS("O", &type);
	Sf_Import(&f, "VehiclesNPC");
	PyObject *sp = Sf_Ask(&f, "VehiclesNPC", "cShipAI", "()");
	Sf_SetAttr(&f, sp, "Ship", type);
	Sf_SetAttrs(&f, sp, SA, sa_mercs_profile);
	return Sf_Return(&f, sp);
}

static PyObject *Again_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "VehiclesNPC");
	Sf_SetMod(&f, "VehiclesNPC", "UseMercs", Sf_Int(&f, 0));
	Lv_AcShips(&f, SA, ac_ships_std);
	return Sf_None(&f);
}

/* Init(LevelPath): the part of the mission of the save variable: follow, evade or call the bishop */
static PyObject *Again_Init(PyObject *self, PyObject *args) {
	static const char *const parts[][2] = { { "Follow", "InitFollow" }, { "Evasion", "InitEvasion" }, { "TalkBishop", "InitTalkBishop" } };
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_StupidMercAgain.SmartMerDeath");
	PyObject *sv = SAVE("Mission_StupidMercAgain");
	for (int i = 0; i < 3 && !f.failed; i++) {
		if (Sf_EqS(&f, sv, parts[i][0]) > 0) {
			Sf_Own(&f, SA, parts[i][1], "()");
			break;
		}
	}
	Sf_SetAttrI(&f, Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(s)", "SMosquito"), "ControlBoost", 1);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(siii)", "Models/Misc/Blueprint/PlanoSMerc2.m3d", 1, 1, 1);
	Sf_Run(&f, "Scrap", "Preload3DObject", "(siii)", "Models/Misc/Blueprint/PlanoMMerc2.m3d", 1, 1, 1);
	return Sf_None(&f);
}

static PyObject *Again_InitFollow(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Later(&f, Sf_Float(&f, 10.0), G(SA, "CreateMerc"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CreateMerc(): the stupid mercenary on his way (at his first waypoint the first time) */
static PyObject *Again_CreateMerc(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *o1 = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ss)", "SMerc1", "Traffic");
	Sf_SetAttrI(&f, o1, "Money", -1);
	Sf_SetAttrS(&f, o1, "TgType", "green");
	Sf_SetGlobal(&f, SA, "MercName", Sf_Attr(&f, o1, "Name"));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", G(SA, "MercName"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, o1, "Name"));
	int first = Sf_True(&f, G(SA, "FirstAppereance"));
	if (first > 0) {
		Lv_Com(&f, "Mission_GetMercShipPlans_RemoteMessage1", "MercenaryB", NULL, NULL);
		Sf_SetGlobal(&f, SA, "FirstAppereance", Sf_Int(&f, 0));
		Sf_SetAttr(&f, o1, "Pos", Sf_Item(&f, G(SA, "MercWaypoints"), 0));
	} else if (first == 0) {
		Lv_Com(&f, "Mission_FollowStupidMerc_RemoteMessage4", "MercenaryB", NULL, NULL);
	}
	Sf_SetGlobal(&f, SA, "iWaypoint", Sf_Int(&f, 0));
	Sf_Own(&f, SA, "GotoNext", "()");
	return Sf_Return(&f, o1);
}

/* one of the two smart mercenaries when its global name has no entity */
static byte Sa_Merc(scriptFrame_t *f, const char *var, const char *name, const char *ship) {
	int mark = f->n;
	PyObject *o = Sf_Entity(f, Sf_Global(f, SA, var));
	if (Sf_Compare(f, o, "==", Py_None) > 0) {
		o = Lv_SpawnCar(f, name, 0);
		Lv_StdAI(f, o, Sf_Own(f, SA, "GetMercsEnemiesProfile", "(s)", ship));
		Sf_SetAttrS(f, o, "EntityList", "Enemies");
		Lv_Life(f, o, SA, "MERCS_LIFE");
		Sf_SetGlobal(f, SA, var, Sf_Attr(f, o, "Name"));
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* CreateMercs(): the smart mercenaries (their first appearance with a message) */
static PyObject *Again_CreateMercs(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "OutMap");
	int none = Sf_EqS(&f, G(SA, "SmartMercName"), "");
	if (none > 0) {
		none = Sf_EqS(&f, G(SA, "MedMercName"), "");
	}
	if (none > 0) {
		Lv_Com(&f, "Mission_GetMercShipPlans_RemoteMessage2", "MercenaryA", NULL, NULL);
	}
	Sa_Merc(&f, "SmartMercName", "MercenaryMed", "SMerc2");
	Sa_Merc(&f, "MedMercName", "MercenarySmart", "SMerc3");
	return Sf_None(&f);
}

/* SmartMerDeath(Name, Killer): a smart mercenary is back 15 s later unless the player is evading */
static PyObject *Again_SmartMerDeath(PyObject *self, PyObject *args) {
	PyObject *name, *killer;
	SF_ARGS("OO", &name, &killer);
	if (Sf_Compare(&f, SAVE("Mission_StupidMercAgain"), "!=", Sf_Str(&f, "Evasion")) > 0) {
		Sf_Later(&f, Sf_Float(&f, 15.0), G(SA, "CreateMercs"), NOARGS, "DelayedCreateMercs");
	}
	return Sf_None(&f);
}

static PyObject *Again_GotoNext(PyObject *self, PyObject *args) {
	(void)self;
	return Sy_GotoNext(args, SA, 1);
}

/* CreateMercPlans(): the blueprints of the ship and the engines of the stupid mercenary as targets */
static PyObject *Again_CreateMercPlans(PyObject *self, PyObject *args) {
	static const char *const plans[][2] = { { "DM_Mission_SMerc2_Blueprint", "PlanoSMerc2" }, { "DM_Mission_MMERC2_Blueprint", "PlanoMMerc2" } };
	SF_ARGS("");
	PyObject *names = Sf_Build(&f, "[]");
	for (int i = 0; i < 2 && !f.failed; i++) {
		int mark = f.n;
		PyObject *prf = Sy_PlanPhoto(&f, SA, plans[i][0], plans[i][1], "AddMercPlans");
		if (prf) {
			Sf_Method(&f, names, "append", "(O)", Sf_Attr(&f, prf, "Name"));
		}
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", names);
	return Sf_None(&f);
}

/* AddMercPlans(proofName): the photo of a blueprint: both blueprints into the hangar, then the evasion */
static PyObject *Again_AddMercPlans(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	int obj = Sf_True(&f, Sf_Ask(&f, "MissionsFuncs", "IsCharMissionObjective", "(O)", name));
	if (obj == 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	if (obj < 0) {
		return Sf_None(&f);
	}
	Sf_SetAttrI(&f, Sf_Entity(&f, name), "CanPhoto", 0);
	if (Sf_True(&f, Sf_Ask(&f, "MissionsFuncs", "DelMissionTarget", "(O)", name)) > 0) {
		Lv_Blueprint(&f, SA, "Hangar.shipsToEditList", "SMerc2", "Mission_FollowStupidMerc_SystemMessage1", 0);
		Lv_Blueprint(&f, SA, "Hangar.availableEnginesList", "MMERC2", "Mission_FollowStupidMerc_SystemMessage2", 0);
		Sf_SetMod(&f, "CharAct", "TakenPhoto", Py_None);
		Lv_Com(&f, "Mission_GetMercShipPlans_RemoteMessage3", "MercenaryC", G(SA, "InitEvasion"), NOARGS);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_StupidMercAgain", "Evasion");
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* StartEvasion(): the smart mercenaries after the player (only the second name is set when none was there) */
static PyObject *Again_StartEvasion(PyObject *self, PyObject *args) {
	SF_ARGS("");
	int none = Sf_EqS(&f, G(SA, "SmartMercName"), "");
	if (none > 0) {
		none = Sf_EqS(&f, G(SA, "MedMercName"), "");
	}
	if (none > 0) {
		Sf_SetGlobal(&f, SA, "MedMercName", Sf_Str(&f, "MercAbsurd"));
	}
	Sf_Own(&f, SA, "CreateMercs", "()");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToIgor2_MissionText1", 1);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "BadReputation", "2");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "StdShipAIProfile", "Veteran");
	Sf_Own(&f, SA, "CheckEvasion", "()");
	return Sf_None(&f);
}

/* a smart mercenary within EVASION_DISTANCE of the player: 1, 0, -1 */
static int Sa_Near(scriptFrame_t *f, PyObject *o, PyObject *usr) {
	int t = Sf_Compare(f, o, "!=", Py_None);
	return t > 0 ? Sf_Compare(f, Sy_Dist(f, o, usr), "<", Sf_Global(f, SA, "EVASION_DISTANCE")) : t;
}

/* the smart mercenary gives up */
static byte Sa_GiveUp(scriptFrame_t *f, PyObject *o) {
	if (Sf_True(f, o) > 0) {
		Sf_SetAttrS(f, o, "EntityList", "Traffic");
		Sf_SetAttrS(f, o, "TgType", "None");
		Sf_Run(f, "SAI", "SetStateVehicle", "(iO)", 3, Sf_Attr(f, o, "Name"));
	}
	return !f->failed;
}

/* CheckEvasion(): every 2 s: both smart mercenaries dead, or EVASION_TIME away from them: the call to the bishop */
static PyObject *Again_CheckEvasion(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	PyObject *now = Sf_Time(&f);
	PyObject *o1 = Sf_Entity(&f, G(SA, "SmartMercName"));
	PyObject *o2 = Sf_Entity(&f, G(SA, "MedMercName"));
	int dead = Sf_Compare(&f, o1, "==", Py_None);
	if (dead > 0) {
		dead = Sf_Compare(&f, o2, "==", Py_None);
	}
	if (dead > 0) {
		dead = Sf_Compare(&f, usr, "!=", Py_None);
	}
	if (dead > 0) {
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DelayedCreateMercs");
		Sf_Own(&f, SA, "InitTalkBishop", "()");
		return Sf_None(&f);
	}
	int evade = Sf_Compare(&f, usr, "==", Py_None);
	if (evade >= 0) {
		evade = !evade;
	}
	if (evade > 0 && Sa_Near(&f, o1, usr) > 0) {
		evade = 0;
	}
	if (evade > 0 && Sa_Near(&f, o2, usr) > 0) {
		evade = 0;
	}
	if (f.failed) {
		return Sf_None(&f);
	}
	int reset = Sf_Compare(&f, G(SA, "EvasionTime"), "==", Sf_Float(&f, 0.0));
	if (reset == 0) {
		reset = !evade;
	}
	if (reset > 0) {
		Sf_SetGlobal(&f, SA, "EvasionTime", now);
	}
	int wait = Sf_Compare(&f, Sf_Op(&f, now, '-', G(SA, "EvasionTime")), "<", G(SA, "EVASION_TIME"));
	if (wait > 0) {
		Sf_Schedule(&f, Sf_Op(&f, now, '+', Sf_Float(&f, 2.0)), G(SA, "CheckEvasion"), NOARGS, NULL);
	} else if (wait == 0) {
		Sa_GiveUp(&f, o1);
		Sa_GiveUp(&f, o2);
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DelayedCreateMercs");
		Lv_Com(&f, "Mission_TalkToIgor2_RemoteMessage1", "MercenaryC", NULL, NULL);
		Lv_Com(&f, "Mission_TalkToIgor2_RemoteMessage2", "MercenaryC", G(SA, "InitTalkBishop"), NOARGS);
	}
	return Sf_None(&f);
}

/* InitTalkBishop(): call the bishop from a free phone cab */
static PyObject *Again_InitTalkBishop(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_StupidMercAgain", "TalkBishop");
	Sf_SetGlobal(&f, SA, "pc", Sf_Ask(&f, "PhoneCab", "GetFree", "()"));
	Sf_SetAttrS(&f, G(SA, "pc"), "UseFunc", "Mission_StupidMercAgain.UsePhoneCab");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(SA, "pc"), "Name"));
	return Sf_None(&f);
}

static PyObject *Again_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const parked[] = { "Parked.Police", "['SPOLI1', 'SPOLI2', 'SPOLI3']", "Parked.Mercs", "['SMERC1', 'SMERC2']",
		"Parked.TownHall", "['SBANKER1', 'SPOLI2']", "Parked.Bank", "['SBANKER1']", "Parked.Temple", "['SPOLI3']", NULL };
	SF_ARGS("");
	Lv_PcDefault(&f, SA);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sy_ApplyNext(&f, SA);
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_Massacre_MissionText1", 1);
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "Mission_StupidMercAgain");
	Lv_Saves(&f, parked);
	return Sf_None(&f);
}

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
