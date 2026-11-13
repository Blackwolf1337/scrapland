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
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
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
	const portFn_t *fns;		/* the function table (stage 7e) */
} levelModule_t;

extern const portFn_t g_MenuMapMethods[], g_TempleMapMethods[], g_TempleXlMethods[], g_TempleArchbishopDeadMethods[],
	g_TempleAskGusMethods[], g_TempleMeetArchbishopMethods[], g_TempleTalkToDeepThroatMethods[];

extern const portFn_t g_CommercialMapMethods[], g_CommercialMapRaceMethods[], g_CommercialDestroyMickusMethods[],
	g_CommercialProtectGusMethods[], g_CommercialDestroyStupidusMethods[], g_CommercialWin1stRaceMethods[],
	g_CommercialFirstMeetDeepThroatMethods[], g_CommercialGetDataPart3Methods[],
	g_CommercialDefeatCrazyGambler3Methods[], g_CommercialDestroyPorkinsTransportsMethods[];
extern const portFn_t g_OrbitMapMethods[], g_OrbitMapDecontMethods[], g_OrbitIntroHumanMethods[];
extern const portFn_t g_GamblinDenMapMethods[], g_GamblinDenBeforeFinalFightMethods[], g_GamblinDenAfterFinalFightMethods[],
	g_GamblinDenArrangeIgorCombatMethods[], g_GamblinDenArrangeBankersCTFMethods[], g_GamblinDenArrangeBankersCombatMethods[],
	g_GamblinDenIgorFirstContactMethods[], g_GamblinDenGoToGamblinDenMethods[], g_GamblinDenTalkToTraitoriusAfterCTFMethods[],
	g_GamblinDenBackFromMortalRaceMethods[], g_GamblinDenCrazyGamblerFirstContactMethods[], g_GamblinDenArrangeMortalRaceMethods[],
	g_GamblinDenThreeStoogesMethods[];
extern const portFn_t g_OmSixthMethods[], g_OmTenthMethods[], g_QfBankersCombatMethods[],
	g_ABIndoorMapMethods[], g_FakeMapMethods[], g_TucZoneMapMethods[], g_NanoMapMethods[], g_ShipEditMapMethods[],
	g_ShipEditMapSndMethods[], g_ABFinalFightMethods[], g_ABMapMethods[];
extern const portFn_t g_JunkyardMapMethods[], g_JunkyardMapShipEditMethods[], g_JunkyardRustyFirstContactMethods[];
extern const portFn_t g_PressMapMethods[], g_PressMapPlanesMethods[], g_PressMapDemoCloseMethods[], g_PressMapSndMethods[],
	g_PressFicusBankerMethods[], g_PressGetCrazyDataMethods[], g_PressGoToPressMethods[], g_PressGoWithHumanMethods[],
	g_PressHumphreyArchbishopKilledMethods[], g_PressIntroHuman2Methods[], g_PressMeetBossMethods[], g_PressPressMeetingMethods[],
	g_PressTalkToBossMethods[], g_PressWhoIsDeepThroatMethods[], g_PressWhoIsTheBaddieMethods[];

extern const portFn_t g_DownTownMapMethods[], g_DownTownMapRaceMethods[], g_DownTownFirstContactPressMethods[],
	g_DownTownBertoMassacreMethods[], g_DownTownDefeatCrazyGambler1Methods[], g_DownTownFaceTheMayorMethods[], g_DownTownMassacreMethods[],
	g_DownTownDistractHumphreyMethods[], g_DownTownGetDataPart1Methods[], g_DownTownHumphreyRaceMethods[],
	g_DownTownMultiplusRaceMethods[], g_DownTownProtectMayorTransportMethods[], g_DownTownStealMayorShipMethods[];
extern const portFn_t g_OutskirtsMapMethods[], g_OutskirtsMapRaceMethods[], g_OutskirtsAttackTraitoriusMethods[],
	g_OutskirtsDemoHumphreyRaceMethods[], g_OutskirtsDestroyBadDebtorsMethods[],
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
	int used;			/* the slots a module made in C took (Script_LevelDataFunction) */
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
	nativeVal_t path;
	if (!Mode_ModGet("Init", "Path", &path) || path.type != NV_STR) {
		Native_Free(&path);
		Native_ErrClear();
		return 0;
	}
	const char *p = Native_Text(&path), *slash = strrchr(p, '/');
	Str_Copy(out, slash ? slash + 1 : p, size);
	for (char *c = out; *c; c++) {
		if (*c >= 'A' && *c <= 'Z') {
			*c = (char)(*c - 'A' + 'a');
		}
	}
	Native_Free(&path);
	return 1;
}

/* "run vecrot <name> <pos> <offset> <angles>": $name = SVec.Add(pos, SVec.Rotate3D(offset, angles)), the position of a
 * sound beside an element of the map */
static void Level_StepVecRot(int id, const nativeVal_t *args, int n) {
	const nativeVal_t *name, *pos, *off, *ang;
	(void)id;
	if (!Mode_Parse(args, n, "SOOO", &name, &pos, &off, &ang)) {
		return;
	}
	nativeVal_t rot = { 0 }, v = { 0 };
	if (PORT_CALL(SVec, Rotate3D, &rot, "vv", off, ang) && PORT_CALL(SVec, Add, &v, "vv", pos, &rot)) {
		Mode_SetValue(Native_Text(name), &v);
	}
	Native_Free(&v);
	Native_Free(&rot);
}

/* run setdef <var>: Scrap.Set(var, Scrap.Def(var)), the engine variable back to its default (the end of the scene
 * loaders); a failed call leaves its error set and stops the section */
static void Level_StepSetDef(int id, const nativeVal_t *args, int n) {
	(void)id;
	if (n != 1) {
		Native_Raise(NE_TYPE, "setdef takes one variable name");
		return;
	}
	nativeVal_t def = { 0 };
	if (PORT_CALL(Scrap, Def, &def, "v", &args[0])) {
		PORT_CALL(Scrap, Set, NULL, "vv", &args[0], &def);
	}
	Native_Free(&def);
}

/* run saveint <name> <savevar>: $name = int(Scrap.GetSaveVar(savevar)) */
static void Level_StepSaveInt(int id, const nativeVal_t *args, int n) {
	const char *name;
	const nativeVal_t *var;
	(void)id;
	if (!Mode_Parse(args, n, "sO", &name, &var)) {
		return;
	}
	nativeVal_t s = { 0 }, v = { 0 };
	if (PORT_CALL(Scrap, GetSaveVar, &s, "v", var) && Mode_Int(&s, &v)) {
		Mode_SetValue(name, &v);
	}
	Native_Free(&v);
	Native_Free(&s);
}

/* OBJLIB 6: savelistadd and evalappend below keep Python values: the save text may be a name that evaluates to a list
 * other holders see (Script_EvalIn), and the list of a $name is changed as the object it is */

/* Scrap.SetSaveVar(var, text) of a step, its error printed by the runner */
static void Level_SetSave(const nativeVal_t *var, const nativeBuf_t *text) {
	nativeVal_t t;
	Native_SetText(&t, text->p, text->len);
	PORT_CALL(Scrap, SetSaveVar, NULL, "vv", var, &t);
	Native_Free(&t);
}

/* an argument of a step as the native value a call passes on (Native_Pass: a list of a $name that holds only native
 * values is one): 1 with *out (Native_Free it), 0 with *out None when it holds an object (OBJLIB 7) */
static byte Level_ArgNative(const nativeVal_t *v, nativeVal_t *out) {
	if (Native_Pass(v, out) && !Native_Holds(out, NV_OBJS)) {
		return 1;
	}
	Native_Free(out);
	return 0;
}

/* savelistadd on native values when the save text is a list literal and the item a native value: 1 when it was done
 * that way (the result as the Python lines give it, the same repr), else 0 and nothing done */
static byte Level_SaveListAddNative(const nativeVal_t *var, const nativeVal_t *s, const nativeVal_t *item) {
	nativeVal_t lst, x;
	if (!Mode_LiteralVal(s, &lst) || lst.type != NV_LIST || !Level_ArgNative(item, &x)) {
		Native_Free(&lst);
		return 0;
	}
	/* in on a list only compares (Native_Compare as PyObject_Compare): no error */
	if (Native_Contains(&lst, &x) == 0) {
		nativeBuf_t b;
		Native_Copy(Native_Append(&lst), &x);
		Native_BufInit(&b);
		Native_Repr(&b, &lst, NR_REPR);
		Level_SetSave(var, &b);
		Native_BufFree(&b);
	}
	Native_Free(&x);
	Native_Free(&lst);
	return 1;
}

/*
 * run savelistadd <savevar> <item>: the four lines of the missions that add a blueprint to a hangar list
 *   lst = eval(Scrap.GetSaveVar(savevar))
 *   if not item in lst:
 *       lst.append(item)
 *       Scrap.SetSaveVar(savevar, `lst`)
 * A list literal and a native item on native values; any other text (a name, the errors) or item the Python way
 * (OBJLIB 6: a name evaluates to the object other holders see).
 */
static void Level_StepSaveListAdd(int id, const nativeVal_t *args, int n) {
	const nativeVal_t *var, *item;
	(void)id;
	if (!Mode_Parse(args, n, "OO", &var, &item)) {
		return;
	}
	nativeVal_t text, lst, x, r;
	if (!PORT_CALL(Scrap, GetSaveVar, &text, "v", var)) {
		return;
	}
	if (Level_SaveListAddNative(var, &text, item)) {
		Native_Free(&text);
		return;
	}
	if (Script_EvalIn("__main__", &text, &lst)) {
		NObj_CallArg(item, &x);
		if (NObj_Contains(&lst, &x) == 0) {
			nativeBuf_t b;
			Native_BufInit(&b);
			if (NObj_CallMethod(&lst, "append", &x, 1, &r) && NObj_Repr(&b, &lst, NR_REPR)) {
				Level_SetSave(var, &b);
			}
			Native_BufFree(&b);
			Native_Free(&r);
		}
		Native_Free(&x);
	}
	Native_Free(&lst);
	Native_Free(&text);
}

/* run newlist <name>: $name = [] */
static void Level_StepNewList(int id, const nativeVal_t *args, int n) {
	const char *name;
	(void)id;
	if (Mode_Parse(args, n, "s", &name)) {
		nativeVal_t empty;
		Native_SetEmpty(&empty, NV_LIST);
		Mode_SetValue(name, &empty);
		Native_Free(&empty);
	}
}

/* run evalappend <list> <text>: list.append(<text>) of a literal (the waypoints of the races, list.append(a, b) of
 * Python 1.5.2 appends the tuple (a, b)), read as a save value (Mode_EvalSave). The list of a $name is the object
 * itself, so the append is seen there; any other value gets the append its type takes or refuses */
static void Level_StepEvalAppend(int id, const nativeVal_t *args, int n) {
	const nativeVal_t *lst, *text;
	(void)id;
	if (!Mode_Parse(args, n, "OS", &lst, &text)) {
		return;
	}
	nativeVal_t v;
	if (Mode_EvalSave("__main__", text, &v)) {
		Mode_RefCall(lst, "append", &v, 1, NULL);
	}
	Native_Free(&v);
}

/* run repr <name> <value>: $name = `value`, Native_Repr of a native value (the same text), any other value the
 * Python way */
static void Level_StepRepr(int id, const nativeVal_t *args, int n) {
	const char *name;
	const nativeVal_t *v;
	nativeVal_t x;
	(void)id;
	if (!Mode_Parse(args, n, "sO", &name, &v)) {
		return;
	}
	nativeVal_t r = { 0 };
	byte ok = 1;
	if (Level_ArgNative(v, &x)) {
		Mode_Repr(&x, &r);
		Native_Free(&x);
	} else {
		ok = Mode_ReprV(v, &r);
	}
	if (ok) {
		Mode_SetValue(name, &r);
	}
	Native_Free(&r);
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
 * like those of a Python function (the error texts of ceval), then the section runs with them as $names. OBJLIB 7:
 * the data functions of the level modules are function objects (a C function bound to that self tuple, with keyword
 * arguments and defaults), made by Level_SectionFunction for the module dicts; they and the functions below that
 * make, replay and look them up stay on Python objects until the tables go
 */
static byte Level_Run(int file, const char *section, nativeVal_t *vars) {
	const levelFile_t *lf = &level_files[file];
	nativeVal_t mod;
	Native_SetStr(&mod, lf->module);
	Script_DefVar(vars, "__module__", &mod);
	Native_Free(&mod);
	return Script_RunFileTry(0, lf->name, section, lf->text, level_steps, vars);
}

/* the value of the parameter name in the $names, by its text (NULL: none) */
static const nativeVal_t *Level_VarOf(const nativeVal_t *vars, const nativeVal_t *name) {
	const nativeVal_t *d = NObj_DictData(vars);
	return d ? Native_DictGet(d, name) : NULL;
}

/* vars[key] = v for a key that is a text */
static void Level_VarSet(nativeVal_t *vars, const nativeVal_t *key, const nativeVal_t *v) {
	if (key->type == NV_STR) {
		Script_DefVar(vars, Native_Text(key), v);
	}
}

/* the same with Python's key and value (Level_Call) */
static void Level_VarSetPy(nativeVal_t *vars, PyObject *key, PyObject *v) {
	nativeVal_t x;
	if (PyString_Check(key)) {
		NObj_FromPy(v, &x);
		Script_DefVar(vars, PyString_AsString(key), &x);
		Native_Free(&x);
	}
}

/* the Python key is in the $names */
static byte Level_HasPy(const nativeVal_t *vars, PyObject *key) {
	nativeVal_t k;
	NObj_FromPy(key, &k);
	byte has = Level_VarOf(vars, &k) != NULL;
	Native_Free(&k);
	return has;
}

static PyObject *Level_Call(PyObject *self, PyObject *args, PyObject *kw) {
	int file = (int)PyInt_AsLong(PyTuple_GetItem(self, 0));
	PyObject *section = PyTuple_GetItem(self, 1), *params = PyTuple_GetItem(self, 2), *defs = PyTuple_GetItem(self, 3);
	int np = PyTuple_Size(params), na = PyTuple_Size(args);
	if (np == 0 && (na > 0 || (kw && PyDict_Size(kw) > 0))) {
		Native_Raise(NE_TYPE, "no arguments expected");
		return NULL;
	}
	if (na > np) {
		char msg[0x80];
		Str_Printf(msg, sizeof(msg), "too many arguments; expected %d, got %d", np, na);
		Native_Raise(NE_TYPE, "%s", msg);
		return NULL;
	}
	nativeVal_t vars;
	Native_SetNone(&vars);
	for (int i = 0; i < na; i++) {
		Level_VarSetPy(&vars, PyTuple_GetItem(params, i), PyTuple_GetItem(args, i));
	}
	if (kw) {
		PyObject *key, *value;
		int pos = 0;
		while (PyDict_Next(kw, &pos, &key, &value)) {
			int known = 0;
			for (int i = 0; i < np && !known; i++) {
				known = PyObject_Compare(key, PyTuple_GetItem(params, i)) == 0;
			}
			const char *err = !known ? "unexpected keyword argument: %.400s" : Level_HasPy(&vars, key) ? "keyword parameter redefined" : NULL;
			if (err) {
				char msg[0x80];
				Str_Printf(msg, sizeof(msg), err, PyString_Check(key) ? PyString_AsString(key) : "?");
				Native_Raise(NE_TYPE, "%s", msg);
				Native_Free(&vars);
				return NULL;
			}
			Level_VarSetPy(&vars, key, value);
		}
	}
	for (int i = 0; i < np; i++) {
		PyObject *p = PyTuple_GetItem(params, i);
		if (Level_HasPy(&vars, p)) {
			continue;
		}
		PyObject *d = PyDict_GetItem(defs, p);
		if (!d) {
			char msg[0x80];
			Str_Printf(msg, sizeof(msg), "not enough arguments; expected %d, got %d", np - PyDict_Size(defs), i);
			Native_Raise(NE_TYPE, "%s", msg);
			Native_Free(&vars);
			return NULL;
		}
		Level_VarSetPy(&vars, p, d);
	}
	byte ok = Level_Run(file, PyString_AsString(section), &vars);
	Native_Free(&vars);
	return ok ? Py_ReturnNone() : NULL;
}

/* the data function of a level module as a native function object: the self tuple of Level_Call as native values */
typedef struct {
	int file;
	nativeVal_t self;		/* (file index, section, parameter names, defaults dict) */
} levelFn_t;

static void Level_FnFree(void *ctx) {
	levelFn_t *f = ctx;
	Native_Free(&f->self);
	Mem_Free(f);
}

/* the TypeError of a binding: the text cut as Level_Call cuts it */
static byte Level_BindError(const char *fmt, const char *s, int x, int y) {
	char msg[0x80];
	if (s) {
		Str_Printf(msg, sizeof(msg), fmt, s);
	} else {
		Str_Printf(msg, sizeof(msg), fmt, x, y);
	}
	return Native_Raise(NE_TYPE, "%s", msg);
}

/* the call of a native data function: the binding of Level_Call on native values (the keywords in the slot order of
 * the dict a call through Python gives them in), then the section with the $names */
static byte Level_NCallKw(void *ctx, const nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out) {
	levelFn_t *f = ctx;
	const nativeVal_t *params = &f->self.items[2], *defs = &f->self.items[3], *k, *x;
	int np = params->n, pos = 0;
	nativeVal_t kd;
	Native_SetNone(out);
	Native_SetEmpty(&kd, NV_DICT);
	for (int i = 0; kw && i < kw->n; i++) {
		nativeVal_t pk, px;
		Native_Copy(&pk, &kw->items[i].items[0]);
		Native_Copy(&px, &kw->items[i].items[1]);
		Native_DictSet(&kd, &pk, &px);
	}
	if (np == 0 && (n > 0 || kd.n > 0)) {
		Native_Free(&kd);
		return Native_Raise(NE_TYPE, "no arguments expected");
	}
	if (n > np) {
		Native_Free(&kd);
		return Level_BindError("too many arguments; expected %d, got %d", NULL, np, n);
	}
	nativeVal_t vars;
	byte ok = 1;
	Native_SetNone(&vars);
	for (int i = 0; i < n; i++) {
		Level_VarSet(&vars, &params->items[i], &a[i]);
	}
	while (ok && Native_DictNext(&kd, &pos, &k, &x)) {
		int known = 0;
		for (int i = 0; i < np && !known; i++) {
			known = Native_Equal(k, &params->items[i]);
		}
		const char *err = !known ? "unexpected keyword argument: %.400s" : Level_VarOf(&vars, k) ?
			"keyword parameter redefined" : NULL;
		if (err) {
			ok = Level_BindError(err, k->type == NV_STR ? Native_Text(k) : "?", 0, 0);
		} else {
			Level_VarSet(&vars, k, x);
		}
	}
	for (int i = 0; ok && i < np; i++) {
		const nativeVal_t *p = &params->items[i], *d;
		if (Level_VarOf(&vars, p)) {
			continue;
		}
		if ((d = Native_DictGet(defs, p)) == NULL) {
			ok = Level_BindError("not enough arguments; expected %d, got %d", NULL, np - defs->n, i);
		} else {
			Level_VarSet(&vars, p, d);
		}
	}
	Native_Free(&kd);
	ok = ok && Level_Run(f->file, Native_Text(&f->self.items[1]), &vars);
	Native_Free(&vars);
	return ok;
}

static byte Level_NCall(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	return Level_NCallKw(ctx, a, n, NULL, out);
}

/* the value of a default parameter of a section line, as the .def parser reads a value: 1 with *out (Native_Free
 * it), 0 when the text is no plain literal */
static byte Level_DefaultN(const char *text, nativeVal_t *out) {
	char buf[LEVEL_NAME + 8];
	nativeVal_t t;
	Str_Printf(buf, sizeof(buf), "(%s,)", text);
	Native_SetNone(out);
	if (!Native_Literal(buf, &t)) {
		return 0;
	}
	byte ok = t.type == NV_TUPLE && t.n == 1;
	if (ok) {
		*out = t.items[0];
		Native_SetNone(&t.items[0]);
	}
	Native_Free(&t);
	return ok;
}

/* the same as a Python value (a new reference, NULL without one; OBJLIB 7, for Level_Call) */
static PyObject *Level_Default(const char *text) {
	nativeVal_t v;
	PyObject *o = Level_DefaultN(text, &v) ? Native_ToPy(&v) : NULL;
	Native_Free(&v);
	return o;
}

/* the words of a section line "section <Func> <params>" (line: the text after "section ", len chars) into line and
 * tok; the number of words */
static int Level_SectionWords(const char *text, int len, char line[0x200], char *tok[24]) {
	int nt = 0;
	Str_Copy(line, text, len + 1 < 0x200 ? len + 1 : 0x200);
	for (char *s = strtok(line, " \t\r"); s && nt < 24; s = strtok(NULL, " \t\r")) {
		tok[nt++] = s;
	}
	return nt;
}

/* the self of the function of the section tok of file i: (i, name, parameter names, defaults dict); a new reference,
 * NULL on an error */
static PyObject *Level_SectionSelf(int i, char *tok[24], int nt) {
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
	return self;
}

/* the same self as native values into *out: (i, name, parameter names, defaults dict by value) */
static void Level_SectionSelfN(int i, char *tok[24], int nt, nativeVal_t *out) {
	nativeVal_t *params, *defs;
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), i);
	Native_SetStr(Native_Append(out), tok[0]);
	Native_SetEmpty(Native_Append(out), NV_TUPLE);
	Native_SetEmpty(Native_Append(out), NV_DICT);
	params = &out->items[2];
	defs = &out->items[3];
	for (int k = 1; k < nt; k++) {
		char *eq = strchr(tok[k], '=');
		nativeVal_t key, d;
		if (eq) {
			*eq = 0;
			if (Level_DefaultN(eq + 1, &d)) {
				Native_SetStr(&key, tok[k]);
				Native_DictSet(defs, &key, &d);
			}
		}
		Native_SetStr(Native_Append(params), tok[k]);
	}
}

/* the function of the section tok of file i into *out: a native function on the self as native values; under
 * SCRAP_NULL_CHECK the C function object of Python, its method definition in slot n of the pool (OBJLIB 7, see
 * Level_Call). 1, 0 with the error */
static byte Level_SectionFunction(int i, levelPool_t *pool, int n, char *tok[24], int nt, nativeVal_t *out) {
	Native_SetNone(out);
	if (!Script_Checking()) {
		levelFn_t *f = Mem_Alloc(sizeof(*f));
		f->file = i;
		Level_SectionSelfN(i, tok, nt, &f->self);
		NObj_NewCallback(out, tok[0], Level_NCall, f, Level_FnFree, 0);
		NObj_SetCallback(out, &f->self, Level_NCallKw);
		return 1;
	}
	PyObject *self = Level_SectionSelf(i, tok, nt);
	if (!self) {
		return 0;
	}
	Str_Copy(pool->names[n], tok[0], LEVEL_NAME);
	PyMethodDef *def = &pool->defs[n];
	def->ml_name = pool->names[n];
	def->ml_meth = (PyCFunction)(void (*)(void))Level_Call;
	def->ml_flags = METH_VARARGS | METH_KEYWORDS;
	def->ml_doc = NULL;
	PyObject *f = Script_CheckFunction(def, self, level_files[i].module);
	Py_DECREF(self);
	if (!f) {
		return 0;
	}
	NObj_FromPy(f, out);
	Py_DECREF(f);
	return 1;
}

/*
 * the dict writes the loader made after the .pyc for the sections of file i, on a module made in C with the values in
 * place (Script_ReplaySet): the sections whose name the module code defined (defs holds those functions by name) and
 * no table of script_modules took before
 */
static void Level_ReplaySections(int i, const nativeVal_t *module, const nativeVal_t *defs) {
	const char *p = level_files[i].text;
	modVarRef_t ref;
	int n = 0;
	if (!ModVar_OfVal(module, &ref)) {
		return;
	}
	while (*p && n < LEVEL_FUNCS) {
		const char *eol = strchr(p, '\n');
		int len = eol ? (int)(eol - p) : (int)strlen(p);
		if (len > 8 && !strncmp(p, "section ", 8)) {
			char line[0x200], *tok[24];
			nativeVal_t old;
			int nt = Level_SectionWords(p + 8, len - 8, line, tok);
			Native_SetNone(&old);
			nativeVal_t key;
			Native_SetStr(&key, nt ? tok[0] : "");
			const nativeVal_t *def = Native_DictGet(defs, &key);
			Native_Free(&key);
			if (nt && ModVar_GetVal(&ref, tok[0], &old) && def && NObj_Is(def, &old) && old.type == NV_REF &&
				!Script_TableHas(level_files[i].module, tok[0])) {
				Script_ReplaySet(&ref, tok[0], &old);
				n++;
			}
			Native_Free(&old);
		}
		p += len + (eol != NULL);
	}
	Native_ErrClear();
	Con_Verbosef("Script: %d functions of %s are data in %s\n", n, level_files[i].module, level_files[i].name);
}

byte Script_LevelName(char *out, int size) {
	return Level_Current(out, size);
}

/* the data file of module in the level that loads, -1 if none */
static int Level_File(const char *module) {
	char level[LEVEL_NAME];
	if (!Level_Current(level, sizeof(level))) {
		return -1;
	}
	for (size_t i = 0; i < sizeof(level_files) / sizeof(level_files[0]); i++) {
		if (!strcmp(level_files[i].level, level) && !strcmp(level_files[i].module, module)) {
			return (int)i;
		}
	}
	return -1;
}

void Script_LevelModuleStart(const char *module) {
	levelPool_t *pool = Level_File(module) >= 0 ? Level_Pool(module) : NULL;
	if (pool) {
		pool->used = 0;
	}
}

/* the table and data functions of a level module as function objects */
byte Script_LevelTableFunction(const char *module, const char *name, nativeVal_t *out) {
	char level[LEVEL_NAME];
	Native_SetNone(out);
	if (!Level_Current(level, sizeof(level))) {
		return 0;
	}
	/* the last row that has the name wins (as the loader put them in) */
	scriptRow_t found = { NULL }, row;
	const char *rn;
	for (const levelModule_t *lm = level_modules; lm->level; lm++) {
		if (!strcmp(lm->level, level) && !strcmp(lm->module, module)) {
			for (int k = 0; (rn = Script_Row(lm->fns, k, &row)) != NULL; k++) {
				if (!strcmp(rn, name)) {
					found = row;
				}
			}
		}
	}
	return found.fn && Script_FunctionOf(&found, module, out);
}

byte Script_LevelDataFunction(const char *module, const char *name, nativeVal_t *out) {
	int i = Level_File(module);
	levelPool_t *pool = i >= 0 ? Level_Pool(module) : NULL;
	Native_SetNone(out);
	if (!pool) {
		return 0;
	}
	/* the last section of the name (a script that defines a function twice gives the last one, so does the converter) */
	const char *p = level_files[i].text, *at = NULL;
	int atLen = 0;
	size_t nlen = strlen(name);
	while (*p) {
		const char *eol = strchr(p, '\n');
		int len = eol ? (int)(eol - p) : (int)strlen(p);
		const char *e = p + 8 + nlen;
		if (len >= 8 + (int)nlen && !strncmp(p, "section ", 8) && !strncmp(p + 8, name, nlen) &&
			(len == 8 + (int)nlen || *e == ' ' || *e == '\t' || *e == '\r')) {
			at = p + 8;
			atLen = len - 8;
		}
		p += len + (eol != NULL);
	}
	if (!at) {
		return 0;
	}
	if (pool->used >= LEVEL_FUNCS) {
		return Native_Raise(NE_SYSTEM, "more than %d data functions in %s", LEVEL_FUNCS, module);
	}
	char line[0x200], *tok[24];
	int nt = Level_SectionWords(at, atLen, line, tok);
	return nt && Level_SectionFunction(i, pool, pool->used++, tok, nt, out);
}

void Script_LevelModuleExtras(const char *name, const nativeVal_t *module, const nativeVal_t *defs) {
	char level[LEVEL_NAME];
	if (!Level_Current(level, sizeof(level))) {
		return;
	}
	int i = Level_File(name);
	if (i >= 0) {
		Level_ReplaySections(i, module, defs);
	}
	for (const levelModule_t *lm = level_modules; lm->level; lm++) {
		if (!strcmp(lm->level, level) && !strcmp(lm->module, name)) {
			Script_TableReplay(module, lm->fns);
		}
	}
}
