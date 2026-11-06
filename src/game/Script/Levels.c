/*
 * The scripts of the levels in C (issue #2): each level loads modules of the same names (Map, MapSnd, the missions)
 * from its own pack. The functions of a level module that are data are the sections of a file
 * data/levels/<level>/<module>.def ("section <Func> <params>"; a parameter with a default is name=value); when the
 * module loads in that level, each section becomes the function of that name. The levels whose logic is C too have
 * a table per module in src/game/Script/levels/<Level>.c (level_modules), put in after the data. The functions neither
 * has stay Python, and so do the module variables and the module code.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/script.h"
#include "levels/Levels.h"

typedef struct {
	const char *level;		/* the folder of the level in the packs, lower case: "police" */
	const char *module;		/* the module name: "MapSnd" */
	const char *name;		/* the file for Script_RunFile: "levels/police/mapsnd.def" */
	const char *text;		/* the built-in copy */
} levelFile_t;

#include "LevelFiles.inc"

/* the functions of level modules that are C (logic): a table per level and module, put in after the data */
typedef struct {
	const char *level;
	const char *module;
	PyMethodDef *methods;
} levelModule_t;

extern PyMethodDef g_MenuMapMethods[], g_TempleMapMethods[], g_TempleXlMethods[], g_TempleArchbishopDeadMethods[],
	g_TempleAskGusMethods[], g_TempleMeetArchbishopMethods[], g_TempleTalkToDeepThroatMethods[];

extern PyMethodDef g_CommercialMapMethods[], g_CommercialMapRaceMethods[], g_CommercialDestroyMickusMethods[],
	g_CommercialProtectGusMethods[], g_CommercialDestroyStupidusMethods[], g_CommercialWin1stRaceMethods[],
	g_CommercialDestroyCommSats2Methods[], g_CommercialFirstMeetDeepThroatMethods[], g_CommercialGetDataPart3Methods[],
	g_CommercialDefeatCrazyGambler3Methods[], g_CommercialDestroyPorkinsTransportsMethods[];
extern PyMethodDef g_OrbitMapMethods[], g_OrbitMapDecontMethods[], g_OrbitIntroHumanMethods[];
extern PyMethodDef g_GamblinDenMapMethods[], g_GamblinDenBeforeFinalFightMethods[], g_GamblinDenAfterFinalFightMethods[],
	g_GamblinDenArrangeIgorCombatMethods[], g_GamblinDenArrangeBankersCTFMethods[], g_GamblinDenArrangeBankersCombatMethods[],
	g_GamblinDenIgorFirstContactMethods[], g_GamblinDenGoToGamblinDenMethods[], g_GamblinDenTalkToTraitoriusAfterCTFMethods[],
	g_GamblinDenBackFromMortalRaceMethods[], g_GamblinDenCrazyGamblerFirstContactMethods[], g_GamblinDenArrangeMortalRaceMethods[],
	g_GamblinDenThreeStoogesMethods[];
extern PyMethodDef g_OmSixthMethods[], g_OmTenthMethods[], g_QfBankersCombatMethods[],
	g_ABIndoorMapMethods[], g_FakeMapMethods[], g_TucZoneMapMethods[], g_NanoMapMethods[], g_ShipEditMapMethods[],
	g_ShipEditMapSndMethods[], g_ABFinalFightMethods[], g_ABMapMethods[];
extern PyMethodDef g_JunkyardMapMethods[], g_JunkyardMapShipEditMethods[], g_JunkyardRustyFirstContactMethods[];
extern PyMethodDef g_PressMapMethods[], g_PressMapPlanesMethods[], g_PressMapDemoCloseMethods[], g_PressMapSndMethods[],
	g_PressFicusBankerMethods[], g_PressGetCrazyDataMethods[], g_PressGoToPressMethods[], g_PressGoWithHumanMethods[],
	g_PressHumphreyArchbishopKilledMethods[], g_PressIntroHuman2Methods[], g_PressMeetBossMethods[], g_PressPressMeetingMethods[],
	g_PressTalkToBossMethods[], g_PressWhoIsDeepThroatMethods[], g_PressWhoIsTheBaddieMethods[];

extern PyMethodDef g_DownTownMapMethods[], g_DownTownMapRaceMethods[], g_DownTownFirstContactPressMethods[],
	g_DownTownBertoMassacreMethods[], g_DownTownDefeatCrazyGambler1Methods[], g_DownTownFaceTheMayorMethods[], g_DownTownMassacreMethods[],
	g_DownTownDestroyCommSats3Methods[], g_DownTownDistractHumphreyMethods[], g_DownTownGetDataPart1Methods[], g_DownTownHumphreyRaceMethods[],
	g_DownTownMultiplusRaceMethods[], g_DownTownProtectMayorTransportMethods[], g_DownTownStealMayorShipMethods[];
extern PyMethodDef g_OutskirtsMapMethods[], g_OutskirtsMapRaceMethods[], g_OutskirtsAttackTraitoriusMethods[],
	g_OutskirtsDemoHumphreyRaceMethods[], g_OutskirtsDestroyBadDebtorsMethods[], g_OutskirtsDestroyCommSats1Methods[],
	g_OutskirtsDestroyHumphreyMethods[], g_OutskirtsFunctionaryHuntingMethods[], g_OutskirtsGoToOutskirtsMethods[],
	g_OutskirtsIdentifyMercMethods[], g_OutskirtsKillPoliceMethods[], g_OutskirtsKillRudolphMethods[],
	g_OutskirtsTransportKillerMethods[], g_OutskirtsWin2ndRaceMethods[];

static const levelModule_t level_modules[] = {
	{ "menu", "Map", g_MenuMapMethods },
	{ "temple", "Map", g_TempleMapMethods },
	{ "temple", "MapExtraLifes", g_TempleXlMethods },
	{ "temple", "Mission_ArchbishopDead", g_TempleArchbishopDeadMethods },
	{ "temple", "Mission_AskGus", g_TempleAskGusMethods },
	{ "temple", "Mission_MeetArchbishop", g_TempleMeetArchbishopMethods },
	{ "temple", "Mission_TalkToDeepThroat", g_TempleTalkToDeepThroatMethods },
	{ "bank", "Map", g_BankMapMethods },
	{ "bank", "MapTreasure", g_BankMapTreasureMethods },
	{ "bank", "Mission_CatchRudolph", g_BankCatchRudolphMethods },
	{ "bank", "Mission_GetDataPart2", g_BankGetDataPart2Methods },
	{ "bank", "Mission_InfiltrateBank", g_BankInfiltrateBankMethods },
	{ "police", "Map", g_PoliceMapMethods },
	{ "police", "MapSnd", g_PoliceMapSndMethods },
	{ "police", "MapPlanes", g_PoliceMapPlanesMethods },
	{ "police", "Mission_BeatPickus", g_PoliceBeatPickusMethods },
	{ "police", "Mission_FicusDead", g_PoliceFicusDeadMethods },
	{ "police", "Mission_GetAuthorization", g_PoliceGetAuthorizationMethods },
	{ "police", "Mission_InitEpilogue", g_PoliceInitEpilogueMethods },
	{ "police", "Mission_JailBreak", g_PoliceJailBreakMethods },
	{ "police", "Mission_PoliceBossCheats", g_PolicePoliceBossCheatsMethods },
	{ "townhall", "Map", g_TownhallMapMethods },
	{ "townhall", "MapSnd", g_TownhallMapSndMethods },
	{ "townhall", "Mission_ApologizeMayor", g_TownhallApologizeMayorMethods },
	{ "townhall", "Mission_CatchTrurl", g_TownhallCatchTrurlMethods },
	{ "townhall", "Mission_SheriffMayor", g_TownhallSheriffMayorMethods },
	{ "townhall", "Mission_TalkToMayor", g_TownhallTalkToMayorMethods },
	{ "commercial", "Map", g_CommercialMapMethods },
	{ "commercial", "MapRace", g_CommercialMapRaceMethods },
	{ "commercial", "Mission_DestroyMickus", g_CommercialDestroyMickusMethods },
	{ "commercial", "Mission_ProtectGus", g_CommercialProtectGusMethods },
	{ "commercial", "Mission_DestroyStupidus", g_CommercialDestroyStupidusMethods },
	{ "commercial", "Mission_Win1stRace", g_CommercialWin1stRaceMethods },
	{ "commercial", "Mission_DestroyCommSats2", g_CommercialDestroyCommSats2Methods },
	{ "commercial", "Mission_FirstMeetDeepThroat", g_CommercialFirstMeetDeepThroatMethods },
	{ "commercial", "Mission_GetDataPart3", g_CommercialGetDataPart3Methods },
	{ "commercial", "Mission_DefeatCrazyGambler3", g_CommercialDefeatCrazyGambler3Methods },
	{ "commercial", "Mission_DestroyPorkinsTransports", g_CommercialDestroyPorkinsTransportsMethods },
	{ "orbit", "Map", g_OrbitMapMethods },
	{ "orbit", "MapDecont", g_OrbitMapDecontMethods },
	{ "orbit", "Mission_IntroHuman", g_OrbitIntroHumanMethods },
	{ "om", "SuperDeal_Sixth", g_OmSixthMethods },
	{ "om", "SuperDeal_Tenth", g_OmTenthMethods },
	{ "qf", "Mission_BankersCombat", g_QfBankersCombatMethods },
	{ "abindoor", "Map", g_ABIndoorMapMethods },
	{ "fake", "Map", g_FakeMapMethods },
	{ "tuczone", "Map", g_TucZoneMapMethods },
	{ "nano", "Map", g_NanoMapMethods },
	{ "shipedit", "Map", g_ShipEditMapMethods },
	{ "shipedit", "MapSnd", g_ShipEditMapSndMethods },
	{ "ab", "MapFinalFight", g_ABFinalFightMethods },
	{ "ab", "Map", g_ABMapMethods },
	{ "gamblinden", "Map", g_GamblinDenMapMethods },
	{ "gamblinden", "Mission_BeforeFinalFight", g_GamblinDenBeforeFinalFightMethods },
	{ "gamblinden", "Mission_AfterFinalFight", g_GamblinDenAfterFinalFightMethods },
	{ "gamblinden", "Mission_ArrangeIgorCombat", g_GamblinDenArrangeIgorCombatMethods },
	{ "gamblinden", "Mission_ArrangeBankersCTF", g_GamblinDenArrangeBankersCTFMethods },
	{ "gamblinden", "Mission_ArrangeBankersCombat", g_GamblinDenArrangeBankersCombatMethods },
	{ "gamblinden", "Mission_IgorFirstContact", g_GamblinDenIgorFirstContactMethods },
	{ "gamblinden", "Mission_GoToGamblinDen", g_GamblinDenGoToGamblinDenMethods },
	{ "gamblinden", "Mission_TalkToTraitoriusAfterCTF", g_GamblinDenTalkToTraitoriusAfterCTFMethods },
	{ "gamblinden", "Mission_BackFromMortalRace", g_GamblinDenBackFromMortalRaceMethods },
	{ "gamblinden", "Mission_CrazyGamblerFirstContact", g_GamblinDenCrazyGamblerFirstContactMethods },
	{ "gamblinden", "Mission_ArrangeMortalRace", g_GamblinDenArrangeMortalRaceMethods },
	{ "gamblinden", "Mission_ThreeStooges", g_GamblinDenThreeStoogesMethods },
	{ "scrapyard", "Map", g_ScrapyardMapMethods },
	{ "scrapyard", "MapTriggers", g_ScrapyardMapTriggersMethods },
	{ "scrapyard", "Mission_Ambush", g_ScrapyardAmbushMethods },
	{ "scrapyard", "Mission_DefeatCrazyGambler2", g_ScrapyardDefeatCrazyGambler2Methods },
	{ "scrapyard", "Mission_FindDtritusMatrix", g_ScrapyardFindDtritusMatrixMethods },
	{ "scrapyard", "Mission_MeetBerto", g_ScrapyardMeetBertoMethods },
	{ "scrapyard", "Mission_MosquitoClue", g_ScrapyardMosquitoClueMethods },
	{ "scrapyard", "Mission_StupidMerc", g_ScrapyardStupidMercMethods },
	{ "scrapyard", "Mission_StupidMercAgain", g_ScrapyardStupidMercAgainMethods },
	{ "scrapyard", "Mission_TalkToDeepThroat", g_ScrapyardTalkToDeepThroatMethods },
	{ "gdb", "Map", g_GDBMapMethods },
	{ "gdb", "MapMatrix", g_GDBMapMatrixMethods },
	{ "gdb", "Mission_IntoTheGDB", g_GDBIntoTheGDBMethods },
	{ "gdb", "Mission_ReinsertOwnMatrix", g_GDBReinsertOwnMatrixMethods },
	{ "gdb", "Mission_ReinsertOwnMatrix2", g_GDBReinsertOwnMatrix2Methods },
	{ "junkyard", "Map", g_JunkyardMapMethods },
	{ "junkyard", "MapShipEdit", g_JunkyardMapShipEditMethods },
	{ "junkyard", "Mission_RustyFirstContact", g_JunkyardRustyFirstContactMethods },
	{ "press", "Map", g_PressMapMethods },
	{ "press", "MapPlanes", g_PressMapPlanesMethods },
	{ "press", "MapDemoClose", g_PressMapDemoCloseMethods },
	{ "press", "MapSnd", g_PressMapSndMethods },
	{ "press", "Mission_FicusBanker", g_PressFicusBankerMethods },
	{ "press", "Mission_GetCrazyData", g_PressGetCrazyDataMethods },
	{ "press", "Mission_GoToPress", g_PressGoToPressMethods },
	{ "press", "Mission_GoWithHuman", g_PressGoWithHumanMethods },
	{ "press", "Mission_HumphreyArchbishopKilled", g_PressHumphreyArchbishopKilledMethods },
	{ "press", "Mission_IntroHuman2", g_PressIntroHuman2Methods },
	{ "press", "Mission_MeetBoss", g_PressMeetBossMethods },
	{ "press", "Mission_PressMeeting", g_PressPressMeetingMethods },
	{ "press", "Mission_TalkToBoss", g_PressTalkToBossMethods },
	{ "press", "Mission_WhoIsDeepThroat", g_PressWhoIsDeepThroatMethods },
	{ "press", "Mission_WhoIsTheBaddie", g_PressWhoIsTheBaddieMethods },
	{ "downtown", "Map", g_DownTownMapMethods },
	{ "downtown", "MapRace", g_DownTownMapRaceMethods },
	{ "downtown", "Mission_FirstContactPress", g_DownTownFirstContactPressMethods },
	{ "downtown", "Mission_BertoMassacre", g_DownTownBertoMassacreMethods },
	{ "downtown", "Mission_DefeatCrazyGambler1", g_DownTownDefeatCrazyGambler1Methods },
	{ "downtown", "Mission_FaceTheMayor", g_DownTownFaceTheMayorMethods },
	{ "downtown", "Mission_Massacre", g_DownTownMassacreMethods },
	{ "downtown", "Mission_DestroyCommSats3", g_DownTownDestroyCommSats3Methods },
	{ "downtown", "Mission_DistractHumphrey", g_DownTownDistractHumphreyMethods },
	{ "downtown", "Mission_GetDataPart1", g_DownTownGetDataPart1Methods },
	{ "downtown", "Mission_HumphreyRace", g_DownTownHumphreyRaceMethods },
	{ "downtown", "Mission_MultiplusRace", g_DownTownMultiplusRaceMethods },
	{ "downtown", "Mission_ProtectMayorTransport", g_DownTownProtectMayorTransportMethods },
	{ "downtown", "Mission_StealMayorShip", g_DownTownStealMayorShipMethods },
	{ "outskirts", "Map", g_OutskirtsMapMethods },
	{ "outskirts", "MapRace", g_OutskirtsMapRaceMethods },
	{ "outskirts", "Mission_AttackTraitorius", g_OutskirtsAttackTraitoriusMethods },
	{ "outskirts", "Mission_Demo_HumphreyRace", g_OutskirtsDemoHumphreyRaceMethods },
	{ "outskirts", "Mission_DestroyBadDebtors", g_OutskirtsDestroyBadDebtorsMethods },
	{ "outskirts", "Mission_DestroyCommSats1", g_OutskirtsDestroyCommSats1Methods },
	{ "outskirts", "Mission_DestroyHumphrey", g_OutskirtsDestroyHumphreyMethods },
	{ "outskirts", "Mission_FunctionaryHunting", g_OutskirtsFunctionaryHuntingMethods },
	{ "outskirts", "Mission_GoToOutskirts", g_OutskirtsGoToOutskirtsMethods },
	{ "outskirts", "Mission_IdentifyMerc", g_OutskirtsIdentifyMercMethods },
	{ "outskirts", "Mission_KillPolice", g_OutskirtsKillPoliceMethods },
	{ "outskirts", "Mission_KillRudolph", g_OutskirtsKillRudolphMethods },
	{ "outskirts", "Mission_TransportKiller", g_OutskirtsTransportKillerMethods },
	{ "outskirts", "Mission_Win2ndRace", g_OutskirtsWin2ndRaceMethods },
	{ NULL, NULL, NULL },
};

enum { LEVEL_FUNCS = 96, LEVEL_POOLS = 256, LEVEL_NAME = 0x40 };

/* the method definitions of the functions of one module name; reused when the module loads again */
typedef struct {
	char module[LEVEL_NAME];
	PyMethodDef defs[LEVEL_FUNCS];
	char names[LEVEL_FUNCS][LEVEL_NAME];
} levelPool_t;

/* one pool per module name seen (104 in the packs, more with mods), made on first use: its method definitions stay
 * in use by the module's functions until the end */
static levelPool_t *g_LevelPools[LEVEL_POOLS];

static levelPool_t *Level_Pool(const char *module) {
	int i = 0;
	for (; i < LEVEL_POOLS && g_LevelPools[i]; i++) {
		if (!strcmp(g_LevelPools[i]->module, module)) {
			return g_LevelPools[i];
		}
	}
	if (i == LEVEL_POOLS) {
		Con_Warnf("Levels: more than %d level module names, %s stays Python\n", LEVEL_POOLS, module);
		return NULL;
	}
	levelPool_t *pool = calloc(1, sizeof(*pool));
	if (pool) {
		Str_Copy(pool->module, module, sizeof(pool->module));
		g_LevelPools[i] = pool;
	}
	return pool;
}

/* the folder of the level that loads: the last part of Init.Path in lower case ("Levels/Police" -> "police") */
static byte Level_Current(char *out, int size) {
	PyObject *path = Script_Attr("Init", "Path");
	if (!path || !PyString_Check(path)) {
		Py_XDECREF(path);
		PyErr_Clear();
		return 0;
	}
	const char *p = PyString_AsString(path), *slash = strrchr(p, '/');
	Str_Copy(out, slash ? slash + 1 : p, size);
	for (char *c = out; *c; c++) {
		if (*c >= 'A' && *c <= 'Z') {
			*c = (char)(*c - 'A' + 'a');
		}
	}
	Py_DECREF(path);
	return 1;
}

/* "run vecrot <name> <pos> <offset> <angles>": $name = SVec.Add(pos, SVec.Rotate3D(offset, angles)), the position of a
 * sound beside an element of the map */
static void Level_StepVecRot(int id, PyObject *args) {
	PyObject *name, *pos, *off, *ang;
	(void)id;
	if (!PyArg_ParseTuple(args, "SOOO", &name, &pos, &off, &ang)) {
		return;
	}
	PyObject *rot = Script_Ask("SVec", "Rotate3D", "(OO)", off, ang), *v = rot ? Script_Ask("SVec", "Add", "(OO)", pos, rot) : NULL;
	Py_XDECREF(rot);
	if (v) {
		Script_SetValue(PyString_AsString(name), v);
	}
}

/* run setdef <var>: Scrap.Set(var, Scrap.Def(var)), the engine variable back to its default (the end of the scene
 * loaders); a failed call leaves its error set and stops the section */
static void Level_StepSetDef(int id, PyObject *args) {
	(void)id;
	PyObject *var = PyTuple_Size(args) == 1 ? PyTuple_GetItem(args, 0) : NULL;
	if (!var) {
		PyErr_SetString(PyExc_TypeError, "setdef takes one variable name");
		return;
	}
	PyObject *def = Script_Ask("Scrap", "Def", "(O)", var);
	if (def) {
		Script_Try("Scrap", "Set", "(OO)", var, def);
		Py_DECREF(def);
	}
}

/* run saveint <name> <savevar>: $name = int(Scrap.GetSaveVar(savevar)) */
static void Level_StepSaveInt(int id, PyObject *args) {
	char *name;
	PyObject *var;
	(void)id;
	if (!PyArg_ParseTuple(args, "sO", &name, &var)) {
		return;
	}
	PyObject *s = Script_Ask("Scrap", "GetSaveVar", "(O)", var), *v = s ? PyNumber_Int(s) : NULL;
	Py_XDECREF(s);
	if (v) {
		Script_SetValue(name, v);
	}
}

/*
 * run savelistadd <savevar> <item>: the four lines of the missions that add a blueprint to a hangar list
 *   lst = eval(Scrap.GetSaveVar(savevar))
 *   if not item in lst:
 *       lst.append(item)
 *       Scrap.SetSaveVar(savevar, `lst`)
 */
static void Level_StepSaveListAdd(int id, PyObject *args) {
	PyObject *var, *item;
	(void)id;
	if (!PyArg_ParseTuple(args, "OO", &var, &item)) {
		return;
	}
	PyObject *s = Script_Ask("Scrap", "GetSaveVar", "(O)", var);
	PyObject *lst = s ? Script_EvalIn("__main__", s) : NULL;
	Py_XDECREF(s);
	if (!lst) {
		return;
	}
	int in = PySequence_Contains(lst, item);
	if (in == 0) {
		PyObject *r = Script_Method(lst, "append", "(O)", item), *text = r ? PyObject_Repr(lst) : NULL;
		Py_XDECREF(r);
		if (text) {
			Script_Try("Scrap", "SetSaveVar", "(OO)", var, text);
			Py_DECREF(text);
		}
	}
	Py_DECREF(lst);
}

/* run newlist <name>: $name = [] */
static void Level_StepNewList(int id, PyObject *args) {
	char *name;
	(void)id;
	if (PyArg_ParseTuple(args, "s", &name)) {
		Script_SetValue(name, PyList_New(0));
	}
}

/* run evalappend <list> <text>: list.append(<text>) of a literal (the waypoints of the races, list.append(a, b) of
 * Python 1.5.2 appends the tuple (a, b)) */
static void Level_StepEvalAppend(int id, PyObject *args) {
	PyObject *lst, *text;
	(void)id;
	if (!PyArg_ParseTuple(args, "OS", &lst, &text)) {
		return;
	}
	PyObject *v = Script_EvalIn("__main__", text), *r = v ? Script_Method(lst, "append", "(O)", v) : NULL;
	Py_XDECREF(v);
	Py_XDECREF(r);
}

/* run repr <name> <value>: $name = `value` */
static void Level_StepRepr(int id, PyObject *args) {
	char *name;
	PyObject *v;
	(void)id;
	if (PyArg_ParseTuple(args, "sO", &name, &v)) {
		PyObject *r = PyObject_Repr(v);
		if (r) {
			Script_SetValue(name, r);
		}
	}
}

static const scriptStep_t level_steps[] = {
	{ "newlist", Level_StepNewList },
	{ "evalappend", Level_StepEvalAppend },
	{ "repr", Level_StepRepr },
	{ "vecrot", Level_StepVecRot },
	{ "setdef", Level_StepSetDef },
	{ "saveint", Level_StepSaveInt },
	{ "savelistadd", Level_StepSaveListAdd },
	{ NULL, NULL },
};

/*
 * a function of a level module: self is (file index, section, parameter names, defaults dict); the arguments bound
 * like those of a Python function (the error texts of ceval), then the section runs with them as $names
 */
static PyObject *Level_Call(PyObject *self, PyObject *args, PyObject *kw) {
	int file = (int)PyInt_AsLong(PyTuple_GetItem(self, 0));
	PyObject *section = PyTuple_GetItem(self, 1), *params = PyTuple_GetItem(self, 2), *defs = PyTuple_GetItem(self, 3);
	int np = PyTuple_Size(params), na = PyTuple_Size(args);
	if (np == 0 && (na > 0 || (kw && PyDict_Size(kw) > 0))) {
		PyErr_SetString(PyExc_TypeError, "no arguments expected");
		return NULL;
	}
	if (na > np) {
		char msg[0x80];
		Str_Printf(msg, sizeof(msg), "too many arguments; expected %d, got %d", np, na);
		PyErr_SetString(PyExc_TypeError, msg);
		return NULL;
	}
	PyObject *vars = PyDict_New();
	if (!vars) {
		return NULL;
	}
	for (int i = 0; i < na; i++) {
		PyDict_SetItem(vars, PyTuple_GetItem(params, i), PyTuple_GetItem(args, i));
	}
	if (kw) {
		PyObject *key, *value;
		int pos = 0;
		while (PyDict_Next(kw, &pos, &key, &value)) {
			int known = 0;
			for (int i = 0; i < np && !known; i++) {
				known = PyObject_Compare(key, PyTuple_GetItem(params, i)) == 0;
			}
			const char *err = !known ? "unexpected keyword argument: %.400s" : PyDict_GetItem(vars, key) ? "keyword parameter redefined" : NULL;
			if (err) {
				char msg[0x80];
				Str_Printf(msg, sizeof(msg), err, PyString_Check(key) ? PyString_AsString(key) : "?");
				PyErr_SetString(PyExc_TypeError, msg);
				Py_DECREF(vars);
				return NULL;
			}
			PyDict_SetItem(vars, key, value);
		}
	}
	for (int i = 0; i < np; i++) {
		PyObject *p = PyTuple_GetItem(params, i);
		if (PyDict_GetItem(vars, p)) {
			continue;
		}
		PyObject *d = PyDict_GetItem(defs, p);
		if (!d) {
			char msg[0x80];
			Str_Printf(msg, sizeof(msg), "not enough arguments; expected %d, got %d", np - PyDict_Size(defs), i);
			PyErr_SetString(PyExc_TypeError, msg);
			Py_DECREF(vars);
			return NULL;
		}
		PyDict_SetItem(vars, p, d);
	}
	const levelFile_t *lf = &level_files[file];
	PyObject *mod = PyString_FromString((char *)lf->module);
	if (mod) {
		PyDict_SetItemString(vars, "__module__", mod);
		Py_DECREF(mod);
	}
	byte ok = Script_RunFileTry(0, lf->name, PyString_AsString(section), lf->text, level_steps, vars);
	Py_DECREF(vars);
	return ok ? Py_ReturnNone() : NULL;
}

/* the value of a default parameter of a section line, as the .def parser reads a value (a new reference) */
static PyObject *Level_Default(const char *text) {
	char buf[LEVEL_NAME + 8];
	Str_Printf(buf, sizeof(buf), "(%s,)", text);
	PyObject *t = Script_Eval(buf);
	PyObject *v = t && PyTuple_Check(t) && PyTuple_Size(t) == 1 ? PyTuple_GetItem(t, 0) : NULL;
	Py_XINCREF(v);
	Py_XDECREF(t);
	return v;
}

/* the sections of file i into functions of the module (only those the script has, under their names) */
static void Level_Install(int i, PyObject *module, levelPool_t *pool, PyObject *kept) {
	const char *p = level_files[i].text;
	PyObject *dict = PyModule_GetDict(module);
	int n = 0;
	while (*p && n < LEVEL_FUNCS) {
		const char *eol = strchr(p, '\n');
		int len = eol ? (int)(eol - p) : (int)strlen(p);
		if (len > 8 && !strncmp(p, "section ", 8)) {
			char line[0x200], *tok[24];
			int nt = 0;
			Str_Copy(line, p + 8, len - 8 + 1 < (int)sizeof(line) ? len - 8 + 1 : (int)sizeof(line));
			for (char *s = strtok(line, " \t\r"); s && nt < 24; s = strtok(NULL, " \t\r")) {
				tok[nt++] = s;
			}
			PyObject *old = nt ? PyDict_GetItemString(dict, tok[0]) : NULL;
			if (old && PyFunction_Check(old)) {
				PyObject *params = PyTuple_New(nt - 1), *defs = PyDict_New();
				for (int k = 1; params && defs && k < nt; k++) {
					char *eq = strchr(tok[k], '=');
					if (eq) {
						*eq = 0;
						PyObject *d = Level_Default(eq + 1);
						if (d) {
							PyDict_SetItemString(defs, tok[k], d);
							Py_DECREF(d);
						}
					}
					PyTuple_SET_ITEM(params, k - 1, PyString_FromString(tok[k]));
				}
				PyObject *self = params && defs ? Py_BuildValue("(isOO)", i, tok[0], params, defs) : NULL;
				Py_XDECREF(params);
				Py_XDECREF(defs);
				Str_Copy(pool->names[n], tok[0], LEVEL_NAME);
				PyMethodDef *def = &pool->defs[n];
				def->ml_name = pool->names[n];
				def->ml_meth = (PyCFunction)(void (*)(void))Level_Call;
				def->ml_flags = METH_VARARGS | METH_KEYWORDS;
				def->ml_doc = NULL;
				PyObject *f = self ? PyCFunction_New(def, self) : NULL;
				Py_XDECREF(self);
				if (f) {
					if (kept) {
						PyList_Append(kept, old);
					}
					PyObject *name = PyString_FromString(tok[0]);
					if (name) {
						Script_ReplaceRefs(dict, name, old, f);
						Py_DECREF(name);
					}
					PyDict_SetItemString(dict, tok[0], f);
					Py_DECREF(f);
					n++;
				}
			}
		}
		p += len + (eol != NULL);
	}
	PyErr_Clear();
	Con_Verbosef("Script: %d functions of %s are data in %s\n", n, level_files[i].module, level_files[i].name);
}

void Script_LevelModuleLoaded(const char *name, PyObject *module, PyObject *kept) {
	char level[LEVEL_NAME];
	if (!Level_Current(level, sizeof(level))) {
		return;
	}
	for (size_t i = 0; i < sizeof(level_files) / sizeof(level_files[0]); i++) {
		if (!strcmp(level_files[i].level, level) && !strcmp(level_files[i].module, name)) {
			levelPool_t *pool = Level_Pool(name);
			if (pool) {
				Level_Install((int)i, module, pool, kept);
			}
			break;
		}
	}
	for (const levelModule_t *lm = level_modules; lm->level; lm++) {
		if (!strcmp(lm->level, level) && !strcmp(lm->module, name)) {
			Script_Install(module, lm->methods, kept);
		}
	}
}
