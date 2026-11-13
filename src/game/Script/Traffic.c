/*
 * The traffic and the police of the game in C (issue #2, the mission scripts missions/traffic.py, outpolice.py and
 * inpolice.py). Traffic: the AI profiles of the traffic ships, the free traffic, the rail traffic of three weights
 * (a new ship for each one that dies) and the reward for helping the police. OutPolice: the police waves outdoors,
 * a new wave when one is gone, with a reward on odd waves while the alarm is on. InPolice: the police indoors.
 *
 * The module variables stay in the modules (Traffic.AcShips, AcNormalShips, AcMediumShips, AcHeavyShips, NumNormal,
 * NumMedium, NumHeavy, Path, PrevVal, onTrackCreate; OutPolice.PoliceWaves, iPoliceWave, PoliceActive, WavePrice,
 * ReputationPrice, OnPoliceCreatedCallback, skipPrice), because the level scripts set them before the level starts.
 * The code reads them as globals like the script (a missing one is a NameError). The tuning by reputation and the
 * waves are data/missions/police.def.
 *
 * The three modules are native C (docs/objlib-plan.md, stage 2): their functions work on C values, read the variables
 * of their modules from the registry where the script reads them and write them back at once (the level files,
 * ShipMaker, Vehicles and the cases read and set them), and set the engine variables, the money and the save variables
 * in C (Scrap.Set, SetMoney, SetAlarm, IncSaveVar, UsrEntity and GetAlarm through their seams). The calls of
 * VehiclesNPC, SaveGame and RaceMaker go through their refs (PORT_CALL: the seam, OBJLIB 5 (9c)). The class
 * VehiclesNPC.cShipAI is called by name (OBJLIB 5 (9c: a class the .mod defines)); SAI.SetStateVehicle and
 * SAI.EnableAIChar go through their refs (stage 7); the ships and profiles the calls give (handle values) and
 * CharsNPC.CharRes stay Python values read by name (OBJLIB 5 (9c: instances), 7), the hooks onTrackCreate and
 * OnPoliceCreatedCallback are values of the variables (OBJLIB 5 (9c: functions as values)) called through the
 * entries of port functions (Port_ValueOf); the steps of police.def take the values of their lines as native values.
 * The functions of Traffic call each other in C as the port did (the engine callbacks name them "Traffic.<name>").
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

static const char police_def[] = {
#embed "../../../data/missions/police.def"
	, 0 };

#define POLICE_FILE "missions/police.def"

/* ---- Traffic ---- */

/* a global of the Traffic module as its functions read it (out None before) */
static byte Traffic_G(const char *name, nativeVal_t *out) {
	return Mode_G("Traffic", name, out);
}

/* SetPath(): the ship models of the rail traffic come from Path, the format before goes into PrevVal */
static byte Traffic_SetPathN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *prev = Mode_V(&t), *path = Mode_V(&t), *fmt = Mode_V(&t);
	return Mode_Done(&t, Native_GVarGet("ShipPathFmt", prev) && Mode_Put("Traffic", "PrevVal", NULL, prev) &&
		Traffic_G("Path", path) && Mode_Cat(&t, fmt, 2, path, Mode_S(&t, "%s.m3d")) && Mode_GVarSet("ShipPathFmt", fmt));
}

/* ResetPath(): the format of PrevVal back */
static byte Traffic_ResetPathN(void) {
	nativeVal_t prev = { 0 }, none;
	Native_SetNone(&none);
	byte ok = Traffic_G("PrevVal", &prev) && Mode_GVarSet("ShipPathFmt", &prev) && Mode_Put("Traffic", "PrevVal", NULL, &none);
	Native_Free(&prev);
	return ok;
}

/* a profile sp = VehiclesNPC.cShipAI() of ship into VehiclesNPC.AIProf (OBJLIB 5 (9c: the class by name); the profile
 * objects are Python values, OBJLIB 7): profile the name of the profile, or NULL for the save variable
 * StdShipAIProfile; hook 0 sets FactorHookMap to 0 */
static byte Traffic_Profile(const nativeVal_t *ship, const char *profile, byte hook) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *prof = Mode_V(&t);
	if (!Native_CallMod("VehiclesNPC", "cShipAI", sp, "") || !Mode_VSet(sp, "Ship", ship)) {
		return Mode_Done(&t, 0);
	}
	if (profile) {
		Native_SetStr(prof, profile);
	} else {
		Mode_SaveVar("StdShipAIProfile", prof);
	}
	return Mode_Done(&t, PORT_CALL(VehiclesNPC, SetAIProfile, NULL, "vv", sp, prof) &&
		(hook || LvN_SetI(sp, "FactorHookMap", 0)) && Mode_ModDictPut("VehiclesNPC", "AIProf", ship, sp));
}

/* the profiles of the ships of a list */
static byte Traffic_Profiles(nativeVal_t *ships, const char *profile, byte hook) {
	int n = Mode_Loop(ships);
	for (int i = 0; i < n; i++) {
		nativeVal_t item = { 0 };
		byte ok = Native_SeqItem(ships, i, &item) && Traffic_Profile(&item, profile, hook);
		Native_Free(&item);
		if (!ok) {
			return 0;
		}
	}
	return n >= 0;
}

/* InitAIProfiles(): the profiles of the traffic ships (StdShipAIProfile) and of the rail traffic (Rookie) */
static byte Traffic_InitAIProfilesN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ships = Mode_V(&t), *normal = Mode_V(&t), *medium = Mode_V(&t), *heavy = Mode_V(&t), *a = Mode_V(&t);
	nativeVal_t *rail = Mode_V(&t);
	return Mode_Done(&t, Mode_Import("VehiclesNPC") && Traffic_G("AcShips", ships) && Traffic_Profiles(ships, NULL, 1) &&
		Traffic_G("AcNormalShips", normal) && Traffic_G("AcMediumShips", medium) && Mode_Arith('+', normal, medium, a) &&
		Traffic_G("AcHeavyShips", heavy) && Mode_Arith('+', a, heavy, rail) && Traffic_Profiles(rail, "Rookie", 0));
}

/* ships[int(Scrap.Rand(0, len(ships)) - 0.1)] of the module list name */
static byte Traffic_RandomShip(const char *name, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ships = Mode_V(&t), *f = Mode_V(&t), *i = Mode_V(&t);
	int n = Traffic_G(name, ships) ? Mode_Len(ships) : -1;
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(f, Scrap_RandRange(0.0, (double)n) - 0.1);
	return Mode_Done(&t, Native_ToInt(f, i) && Mode_Sub(ships, i, out));
}

/* o1 = VehiclesNPC.MakeAI(random ship of AcShips, list); o1.EntityList = list (the ship it gives a handle value) */
static byte Traffic_Make(const char *list) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ship = Mode_V(&t), *o1 = Mode_V(&t);
	return Mode_Done(&t, Mode_Import("VehiclesNPC") && Traffic_RandomShip("AcShips", ship) &&
		PORT_CALL(VehiclesNPC, MakeAI, o1, "vs", ship, list) && LvN_SetS(o1, "EntityList", list));
}

/* count ships of list from the count n (range(n)) */
static byte Traffic_MakeN(const nativeVal_t *n, const char *list) {
	long count;
	if (!Mode_Range(n, &count)) {
		return 0;
	}
	for (long f = 0; f < count; f++) {
		if (!Traffic_Make(list)) {
			return 0;
		}
	}
	return 1;
}

/* InitTraffic(): iTrafficNumber ships */
static byte Traffic_InitTrafficN(void) {
	nativeVal_t n = { 0 };
	byte ok = Native_GVarGet("iTrafficNumber", &n) && Traffic_MakeN(&n, "Traffic");
	Native_Free(&n);
	return ok;
}

/* OnPlayerHelpedPolice(): the reward of PlayerHelpedPoliceReward and its message */
static byte Traffic_OnPlayerHelpedPoliceN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *reward = Mode_V(&t), *lang = Mode_V(&t), *rep = Mode_V(&t), *cad = Mode_V(&t), *money = Mode_V(&t);
	nativeVal_t *sum = Mode_V(&t);
	if (!Mode_Import("RaceMaker") || !Mode_Import("SaveGame") || !Mode_Import("string") ||
		!Native_GVarGet("PlayerHelpedPoliceReward", reward)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang("PlayerHelpedPolice", lang);
	Mode_Repr(reward, rep);
	Mode_Replace(lang, "<n>", rep, cad);
	Native_SetInt(money, Mode_GetMoney());
	return Mode_Done(&t, Mode_Arith('+', money, reward, sum) && Mode_SetMoneySeam(sum) &&
		PORT_CALL(SaveGame, SaveMoney, NULL, "") && PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", cad));
}

/* RailTrafficDeath(Me, Killer): the alarm and the statistics when the player killed it */
static byte Traffic_RailDeath(const nativeVal_t *killer) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *u = Mode_V(&t), *name = Mode_V(&t);
	if (!Mode_UsrHandle(u)) {
		return Mode_Done(&t, 0);
	}
	int hit = Native_True(u);
	if (hit) {
		hit = Mode_VObj(u, "Name", name) ? Mode_Cmp(name, "==", killer) : -1;
	}
	return Mode_Done(&t, hit >= 0 && (!hit || (Mode_SetAlarm(Mode_I(&t, 1)) && Mode_IncSaveVar("Stats.Traffic"))));
}

/* the death of a rail ship of list (HeavyTraffic...) from ships (AcHeavyShips...): a new one of type, the hook
 * onTrackCreate, the death, the path back (OBJLIB 5 (9c: the hook a function value of the module)) */
static byte Traffic_TrackDeath(const nativeVal_t *killer, const char *ships, const char *list, const char *type) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ship = Mode_V(&t), *o1 = Mode_V(&t), *name = Mode_V(&t);
	if (!Mode_Import("VehiclesNPC") || !Traffic_SetPathN() || !Traffic_RandomShip(ships, ship) ||
		!PORT_CALL(VehiclesNPC, MakeAI, o1, "vsii", ship, list, 0L, 0L) || !LvN_SetS(o1, "EntityList", list) ||
		!LvN_SetS(o1, "Traf_Type", type) || !Mode_VGet(o1, "Name", name) ||
		!PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 4L, name)) {
		return Mode_Done(&t, 0);
	}
	/* OBJLIB 5 (9c): the hook is a function value of the module */
	nativeVal_t *hook = Mode_V(&t);
	return Mode_Done(&t, Mode_G("Traffic", "onTrackCreate", hook) && Mode_CallArgs(hook, NULL, o1, 1) &&
		Traffic_RailDeath(killer) && Traffic_ResetPathN());
}

/* InitCivilTraffic(): the death callbacks of the rail lists and NumNormal, NumHeavy and NumMedium ships */
static byte Traffic_InitCivilTrafficN(void) {
	static const char *const lists[3][2] = {
		{ "HeavyTraffic", "Traffic.HeavyTrafficDeath" },
		{ "NormalTraffic", "Traffic.NormalTrafficDeath" },
		{ "MediumTraffic", "Traffic.MediumTrafficDeath" },
	};
	/* the order of the script's loops */
	static const char *const nums[3][4] = {
		{ "NumNormal", "AcNormalShips", "NormalTraffic", "Light" },
		{ "NumHeavy", "AcHeavyShips", "HeavyTraffic", "Heavy" },
		{ "NumMedium", "AcMediumShips", "MediumTraffic", "Medium" },
	};
	nativeVal_t empty;
	for (int i = 0; i < 3; i++) {
		if (!Mode_ListSet(lists[i][0], "OnDeath", lists[i][1])) {
			return 0;
		}
	}
	Native_SetStr(&empty, "");
	for (int i = 0; i < 3; i++) {
		nativeVal_t num = { 0 };
		long count = 0;
		byte ok = Traffic_G(nums[i][0], &num) && Mode_Range(&num, &count);
		Native_Free(&num);
		for (long f = 0; ok && f < count; f++) {
			ok = Traffic_TrackDeath(&empty, nums[i][1], nums[i][2], nums[i][3]);
		}
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* Init(): the profiles, the traffic and the engine callbacks */
static byte Traffic_InitN(void) {
	return Traffic_InitAIProfilesN() && Traffic_InitTrafficN() && Traffic_InitCivilTrafficN() &&
		Mode_GVarSetS("OnNeedMoreTraffic", "Traffic.OnNeedMoreTraffic") &&
		Mode_GVarSetS("OnNeedMoreChasingTraffic", "Traffic.OnNeedMoreChasingTraffic") &&
		Mode_GVarSetS("OnPlayerHelpedPolice", "Traffic.OnPlayerHelpedPolice");
}

/* the entries (no arguments: the arity of the script's functions is not checked, as the port did) */
MODE_FNANY(Traffic, SetPath, Traffic_SetPathN())
MODE_FNANY(Traffic, ResetPath, Traffic_ResetPathN())
MODE_FNANY(Traffic, InitAIProfiles, Traffic_InitAIProfilesN())
MODE_FNANY(Traffic, MakeTrafic, Traffic_Make("Traffic"))
MODE_FNANY(Traffic, InitTraffic, Traffic_InitTrafficN())
MODE_FNANY(Traffic, OnPlayerHelpedPolice, Traffic_OnPlayerHelpedPoliceN())
MODE_FNANY(Traffic, InitCivilTraffic, Traffic_InitCivilTrafficN())
MODE_FNANY(Traffic, Init, Traffic_InitN())
MODE_FN(Traffic, EmptyFunc, 1, 1)
MODE_FN(Traffic, OnNeedMoreTraffic, 1, Traffic_MakeN(&a[0], "Traffic"))
MODE_FN(Traffic, OnNeedMoreChasingTraffic, 1, Mode_Import("VehiclesNPC") && Traffic_MakeN(&a[0], "ChasingTraffic"))
MODE_FN(Traffic, RailTrafficDeath, 2, Traffic_RailDeath(&a[1]))
MODE_FN(Traffic, HeavyTrafficDeath, 2, Traffic_TrackDeath(&a[1], "AcHeavyShips", "HeavyTraffic", "Heavy"))
MODE_FN(Traffic, NormalTrafficDeath, 2, Traffic_TrackDeath(&a[1], "AcNormalShips", "NormalTraffic", "Light"))
MODE_FN(Traffic, MediumTrafficDeath, 2, Traffic_TrackDeath(&a[1], "AcMediumShips", "MediumTraffic", "Medium"))

const portFn_t g_TrafficMethods[] = {
	PORT_FN(SetPath, Traffic_SetPath),
	PORT_FN(ResetPath, Traffic_ResetPath),
	PORT_FN(InitAIProfiles, Traffic_InitAIProfiles),
	PORT_FN(EmptyFunc, Traffic_EmptyFunc),
	PORT_FN(MakeTrafic, Traffic_MakeTrafic),
	PORT_FN(InitTraffic, Traffic_InitTraffic),
	PORT_FN(OnNeedMoreTraffic, Traffic_OnNeedMoreTraffic),
	PORT_FN(OnNeedMoreChasingTraffic, Traffic_OnNeedMoreChasingTraffic),
	PORT_FN(OnPlayerHelpedPolice, Traffic_OnPlayerHelpedPolice),
	PORT_FN(RailTrafficDeath, Traffic_RailTrafficDeath),
	PORT_FN(HeavyTrafficDeath, Traffic_HeavyTrafficDeath),
	PORT_FN(NormalTrafficDeath, Traffic_NormalTrafficDeath),
	PORT_FN(MediumTrafficDeath, Traffic_MediumTrafficDeath),
	PORT_FN(InitCivilTraffic, Traffic_InitCivilTraffic),
	PORT_FN(Init, Traffic_Init),
	PORT_FNEND
};

/* ---- the police by reputation (police.def) ---- */

/* run wave <ship> ...: one wave appended to OutPolice.PoliceWaves */
static void Step_Wave(int id, const nativeVal_t *args, int n) {
	nativeVal_t wave;
	(void)id;
	/* the list holds the values as they are: a list of a $name in it is that list */
	if (!Script_StepValues(args, n, NV_LIST, 0, &wave) || !Mode_VarCall("OutPolice", "PoliceWaves", 1, "append", &wave)) {
		Native_ErrPrint();
	}
	Native_Free(&wave);
}

/* run defscale <var> <factor>: Scrap.Set(var, Scrap.Def(var) * factor) */
static void Step_DefScale(int id, const nativeVal_t *args, int n) {
	const nativeVal_t *var, *factor;
	(void)id;
	if (!Mode_Parse(args, n, "OO", &var, &factor)) {
		Native_ErrPrint();
		return;
	}
	/* OBJLIB 9: the * of the script on whatever Scrap.Def gives and the factor is (Mode_Times) */
	nativeVal_t def = { 0 }, v = { 0 };
	if (!PORT_CALL(Scrap, Def, &def, "v", var) || !Mode_Times(&def, factor, &v) ||
		!PORT_CALL(Scrap, Set, NULL, "vv", var, &v)) {
		Native_ErrPrint();
	}
	Native_Free(&v);
	Native_Free(&def);
}

static const scriptStep_t police_steps[] = {
	{ "wave", Step_Wave },
	{ "defscale", Step_DefScale },
	{ NULL, NULL },
};

/* int(Scrap.GetSaveVar("BadReputation")) into *rptn: 0 with the error */
static byte Police_Reputation(long *rptn) {
	nativeVal_t sv, i = { 0 };
	Mode_SaveVar("BadReputation", &sv);
	byte ok = Native_ToInt(&sv, &i);
	if (ok) {
		*rptn = i.i;
	}
	Native_Free(&sv);
	Native_Free(&i);
	return ok;
}

/* if Scrap.GetSaveVar("RobotsControlledByBoss") == "1": the alarm does not fall. 0 with the error */
static byte Police_BossRobots(void) {
	nativeVal_t sv;
	Mode_SaveVar("RobotsControlledByBoss", &sv);
	byte ok = !Native_IsStr(&sv, "1") || Mode_GVarSetN("AlarmFallDelta", 0.0);
	Native_Free(&sv);
	return ok;
}

/* a global of the OutPolice module as its functions read it (out None before) */
static byte OutPolice_G(const char *name, nativeVal_t *out) {
	return Mode_G("OutPolice", name, out);
}

/* ---- OutPolice ---- */

/* MakePoli(TypeShip): a police ship into OutCop, the hook OnPoliceCreatedCallback */
static byte OutPolice_MakePoliN(const nativeVal_t *type) {
	nativeVal_t ship = { 0 }, list;
	/* the ship VehiclesNPC.MakeAI gives is a Python value (OBJLIB 7) */
	if (!Mode_Import("VehiclesNPC") || !PORT_CALL(VehiclesNPC, MakeAI, &ship, "vsi", type, "Police", 1L)) {
		return 0;
	}
	Native_SetStr(&list, "OutCop");
	/* OBJLIB 5 (9c): the hook is a function value of the module */
	nativeVal_t hook = { 0 };
	int t = Mode_VSet(&ship, "EntityList", &list) && Mode_G("OutPolice", "OnPoliceCreatedCallback", &hook) ?
		Mode_Truth(&hook) : -1;
	byte ok = t >= 0 && (!t || Mode_CallArgs(&hook, NULL, &ship, 1));
	Native_Free(&hook);
	Native_Free(&ship);
	return ok;
}

/* iPoliceWave = v */
static byte OutPolice_SetWave(const nativeVal_t *v) {
	return Mode_Put("OutPolice", "iPoliceWave", NULL, v);
}

/* InitShips(): the ships of the wave iPoliceWave, then the next wave (the last one stays) */
static byte OutPolice_InitShipsN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *waves = Mode_V(&t), *i = Mode_V(&t), *wave = Mode_V(&t), *cur = Mode_V(&t), *next = Mode_V(&t);
	nativeVal_t *len = Mode_V(&t);
	if (!OutPolice_G("PoliceWaves", waves) || !OutPolice_G("iPoliceWave", i) || !Native_Subscript(waves, i, wave)) {
		return Mode_Done(&t, 0);
	}
	/* for item in wave */
	int n = Native_ForCount(wave);
	if (n < 0) {
		Native_Raise(NE_TYPE, "loop over non-sequence");
		return Mode_Done(&t, 0);
	}
	for (int k = 0; k < n; k++) {
		nativeVal_t item = { 0 };
		byte ok = Native_SeqItem(wave, k, &item) && OutPolice_MakePoliN(&item);
		Native_Free(&item);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	if (!OutPolice_G("iPoliceWave", cur) || !Native_Arith('+', cur, Mode_I(&t, 1), next) || !OutPolice_SetWave(next)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(cur);
	Native_Free(waves);
	int count = OutPolice_G("iPoliceWave", cur) && OutPolice_G("PoliceWaves", waves) ? Native_Size(waves) : -1;
	if (count < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(len, count);
	int c = Native_Compare(cur, len);
	if (Native_Failed()) {
		return Mode_Done(&t, 0);
	}
	if (c >= 0) {
		Native_Free(waves);
		count = OutPolice_G("PoliceWaves", waves) ? Native_Size(waves) : -1;
		Native_SetInt(len, count - 1);
		if (count < 0 || !OutPolice_SetWave(len)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* InitWaves(): the alarm speeds and the waves of the reputation (police.def), from the first wave */
static byte OutPolice_InitWavesN(void) {
	long rptn;
	nativeVal_t zero;
	if (!Police_Reputation(&rptn)) {
		return 0;
	}
	if (rptn >= 0 && rptn <= 5) {
		char section[16];
		Str_Printf(section, sizeof(section), "waves%ld", rptn);
		Script_RunFile(0, POLICE_FILE, section, police_def, police_steps, NULL);
	}
	Native_SetInt(&zero, 0);
	return OutPolice_SetWave(&zero);
}

/* InitAIProfiles(): the profiles SPoli1..6 of each skill, as VehiclesNPC.AIProf["SPoli1_Rookie"] (OBJLIB 5 (9c:
 * the class VehiclesNPC.cShipAI by name); the profile objects are Python values, OBJLIB 7) */
static byte OutPolice_InitAIProfilesN(void) {
	static const char *const types[] = { "Rookie", "Pilot", "Veteran", "Elite", "Ace", "MegaAce" };
	if (!Mode_Import("VehiclesNPC")) {
		return 0;
	}
	for (int s = 1; s <= 6; s++) {
		for (int t = 0; t < 6; t++) {
			char ship[16], key[32];
			nativeVal_t sp = { 0 }, name, k;
			Str_Printf(ship, sizeof(ship), "SPoli%d", s);
			Str_Printf(key, sizeof(key), "SPoli%d_%s", s, types[t]);
			Native_SetStr(&name, ship);
			Native_SetStr(&k, key);
			byte ok = Native_CallMod("VehiclesNPC", "cShipAI", &sp, "") && Mode_VSet(&sp, "Ship", &name) &&
				PORT_CALL(VehiclesNPC, SetAIProfile, NULL, "vs", &sp, types[t]) &&
				Mode_ModDictPut("VehiclesNPC", "AIProf", &k, &sp);
			Native_Free(&sp);
			if (!ok) {
				return 0;
			}
		}
	}
	return 1;
}

/* Init(): the profiles; while PoliceActive the waves, the first wave and the engine callback for the next */
static byte OutPolice_InitN(void) {
	nativeVal_t active = { 0 };
	byte ok = OutPolice_InitAIProfilesN() && OutPolice_G("PoliceActive", &active) &&
		(!Native_True(&active) || (OutPolice_InitWavesN() && OutPolice_InitShipsN() &&
		Mode_GVarSetS("PoliceWaveEliminated", "OutPolice.NextWave"))) && Police_BossRobots();
	Native_Free(&active);
	return ok;
}

/* skipPrice = 0 */
static byte OutPolice_Put0(void) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Mode_Put("OutPolice", "skipPrice", NULL, &zero);
}

/* NextWave(doprice=1): the next wave; after an odd wave (not the first) while the alarm is on the reward
 * WavePrice * wave + ReputationPrice * BadReputation, unless skipPrice skips it once */
static byte OutPolice_NextWaveN(const nativeVal_t *doprice) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *skip = Mode_V(&t), *i = Mode_V(&t), *r = Mode_V(&t), *wp = Mode_V(&t), *a = Mode_V(&t);
	nativeVal_t *rp = Mode_V(&t), *sv = Mode_V(&t), *bad = Mode_V(&t), *b = Mode_V(&t), *price = Mode_V(&t);
	nativeVal_t *money = Mode_V(&t), *sum = Mode_V(&t), *msg = Mode_V(&t), *text = Mode_V(&t);
	if (!OutPolice_InitShipsN() || !Mode_GVarSetS("PoliceWaveEliminated", "OutPolice.NextWave") ||
		!OutPolice_G("skipPrice", skip)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(skip)) {
		return Mode_Done(&t, OutPolice_Put0());
	}
	int go = doprice ? Native_True(doprice) : 1;
	if (go) {
		go = Mode_AlarmOn();
	}
	if (go > 0) {
		/* iPoliceWave != 1 */
		go = OutPolice_G("iPoliceWave", i) ? Native_Compare(i, Mode_I(&t, 1)) != 0 : -1;
		if (Native_Failed()) {
			go = -1;
		}
	}
	if (go > 0) {
		Native_Free(i);
		go = OutPolice_G("iPoliceWave", i) && Native_Arith('%', i, Mode_I(&t, 2), r) ? Native_True(r) : -1;
	}
	if (go > 0) {
		Native_Free(i);
		Mode_SaveVar("BadReputation", sv);
		go = Mode_Import("RaceMaker") && OutPolice_G("WavePrice", wp) && OutPolice_G("iPoliceWave", i) &&
			Native_Arith('*', wp, i, a) && OutPolice_G("ReputationPrice", rp) && Native_ToInt(sv, bad) &&
			Native_Arith('*', rp, bad, b) && Native_Arith('+', a, b, price) &&
			(Native_SetInt(money, Mode_GetMoney()), Native_Arith('+', money, price, sum)) && Mode_SetMoney(sum) &&
			(Mode_Lang("OnPoliceWave", msg), Native_Arith('%', msg, price, text)) &&
			PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", text) ? 1 : -1;
	}
	return Mode_Done(&t, go >= 0);
}

/* the entries (MakePoli and NextWave check their arguments, the others take any, as the port did) */
MODE_FN(OutPolice, MakePoli, 1, OutPolice_MakePoliN(&a[0]))

MODE_FNANY(OutPolice, InitShips, OutPolice_InitShipsN())
MODE_FNANY(OutPolice, InitWaves, OutPolice_InitWavesN())
MODE_FNANY(OutPolice, InitAIProfiles, OutPolice_InitAIProfilesN())
MODE_FNANY(OutPolice, Init, OutPolice_InitN())

/* NextWave(doprice), doprice optional */
static byte OutPolice_NextWave_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *doprice = NULL;
	(void)out;
	return Mode_Parse(a, n, "|O", &doprice) && OutPolice_NextWaveN(doprice);
}

const portFn_t g_OutPoliceMethods[] = {
	PORT_FN(MakePoli, OutPolice_MakePoli),
	PORT_FN(Init, OutPolice_Init),
	PORT_FN(NextWave, OutPolice_NextWave),
	PORT_FN(InitShips, OutPolice_InitShips),
	PORT_FN(InitWaves, OutPolice_InitWaves),
	PORT_FN(InitAIProfiles, OutPolice_InitAIProfiles),
	PORT_FNEND
};

/* ---- InPolice ---- */

/* Include(chrname): a Sentinel, Gear or Police of CharsNPC.CharRes gets its AI and its police list */
static byte InPolice_IncludeN(const nativeVal_t *name) {
	nativeVal_t act = { 0 }, el, ch = { 0 };
	if (!Mode_Import("CharsNPC")) {
		return 0;
	}
	/* CharRes holds the character objects of CharsNPC (native instances, held by reference) */
	nativeVal_t ctr;
	byte ok = Mode_ModAt("CharsNPC", "CharRes", name, &ctr) && Mode_VGet(&ctr, "ActCtrl", &act);
	Native_Free(&ctr);
	if (!ok) {
		return 0;
	}
	const char *s = act.type == NV_STR ? Native_Text(&act) : NULL;
	byte police = s && !strcmp(s, "Police");
	if (s && (!strcmp(s, "Sentinel") || !strcmp(s, "Gear") || police)) {
		Native_SetStr(&el, police ? "PoliceSmall" : s);
		/* SAI.EnableAIChar (its NULL result for a name the AI table does not have: the SystemError of the call) */
		ok = Mode_EntityH(name, &ch) && PORT_CALL(SAI, EnableAIChar, NULL, "vii", name, 1L, 0L) &&
			Mode_VSet(&ch, "EntityList", &el);
	}
	Native_Free(&act);
	Native_Free(&ch);
	return ok;
}

/* SetGVars(): the alarm speeds and the police tuning of the reputation (police.def: in0, in1, in2 for the rest) */
static byte InPolice_SetGVarsN(void) {
	long rptn;
	if (!Mode_Import("CharAct") || !Police_Reputation(&rptn)) {
		return 0;
	}
	Script_RunFile(0, POLICE_FILE, rptn == 0 ? "in0" : rptn == 1 ? "in1" : "in2", police_def, police_steps, NULL);
	return 1;
}

/* Init(): the tuning and the engine callbacks that create Gears and Police */
static byte InPolice_InitN(void) {
	return InPolice_SetGVarsN() && Mode_GVarSetS("CreateGear", "CharsNPC.CallCreateChar") &&
		Mode_GVarSetS("CreatePolice", "CharsNPC.CallCreateChar") && Police_BossRobots();
}

MODE_FN(InPolice, Include, 1, InPolice_IncludeN(&a[0]))

MODE_FNANY(InPolice, SetGVars, InPolice_SetGVarsN())
MODE_FNANY(InPolice, Init, InPolice_InitN())

const portFn_t g_InPoliceMethods[] = {
	PORT_FN(Include, InPolice_Include),
	PORT_FN(SetGVars, InPolice_SetGVars),
	PORT_FN(Init, InPolice_Init),
	PORT_FNEND
};

PORTS(Traffic)
