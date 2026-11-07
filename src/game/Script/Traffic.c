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
 * InPolice and OutPolice are native C (docs/objlib-plan.md, stage 2): their functions work on C values, read the
 * variables of OutPolice where the script reads them and write them back to the module (OBJLIB 5), and set the engine
 * variables, the money and the save variables in C (Scrap.Set and Scrap.GetAlarm through their seams, OBJLIB 7). The
 * calls of VehiclesNPC, RaceMaker and SAI.EnableAIChar, the ships and profiles they give, the hook
 * OnPoliceCreatedCallback and CharsNPC.CharRes stay Python values called or read by name (OBJLIB 3, 5, 7); the steps of
 * police.def take the arguments of their lines as a tuple (OBJLIB 6). Traffic is still on the Python objects.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

static const char police_def[] = {
#embed "../../../data/missions/police.def"
	, 0 };

#define POLICE_FILE "missions/police.def"

/* ---- Traffic ---- */

/* SetPath(): the ship models of the rail traffic come from Path, the format before goes into PrevVal */
static PyObject *Traffic_SetPath(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *prev = Script_Ask("Scrap", "Get", "(s)", "ShipPathFmt");
	if (!prev || !Script_SetGlobal("Traffic", "PrevVal", prev)) {
		return NULL;
	}
	PyObject *fmt = Script_PlusS(Script_Global("Traffic", "Path"), "%s.m3d");
	byte ok = fmt && Script_Try("Scrap", "Set", "(sO)", "ShipPathFmt", fmt);
	Py_XDECREF(fmt);
	return ok ? Py_ReturnNone() : NULL;
}

/* ResetPath(): the format of PrevVal back */
static PyObject *Traffic_ResetPath(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *prev = Script_Global("Traffic", "PrevVal");
	byte ok = prev && Script_Try("Scrap", "Set", "(sO)", "ShipPathFmt", prev);
	Py_XDECREF(prev);
	if (!ok || !Script_SetGlobal("Traffic", "PrevVal", Script_Ref(Py_None))) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* a profile sp = VehiclesNPC.cShipAI() of ship into VehiclesNPC.AIProf: profile the name of the profile, or NULL
 * for the save variable StdShipAIProfile; hook 0 sets FactorHookMap to 0. 0 with the error set */
static byte Traffic_Profile(PyObject *ship, const char *profile, byte hook) {
	scriptRefs_t r = { 0 };
	PyObject *sp = Script_Keep(&r, Script_Ask("VehiclesNPC", "cShipAI", "()"));
	PyObject *prof = sp && Script_HSet(sp, "Ship", Script_Ref(ship)) ?
		Script_Keep(&r, profile ? PyString_FromString(profile) : Script_SaveVar("StdShipAIProfile")) : NULL;
	byte ok = prof && Script_Try("VehiclesNPC", "SetAIProfile", "(OO)", sp, prof) &&
		(hook || Script_HSet(sp, "FactorHookMap", PyInt_FromLong(0)));
	PyObject *all = ok ? Script_Keep(&r, Script_Attr("VehiclesNPC", "AIProf")) : NULL;
	ok = all && PyObject_SetItem(all, ship, sp) == 0;
	Script_Done(&r, NULL);
	return ok;
}

/* InitAIProfiles(): the profiles of the traffic ships (StdShipAIProfile) and of the rail traffic (Rookie) */
static PyObject *Traffic_InitAIProfiles(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "VehiclesNPC", NULL };
	scriptRefs_t r = { 0 };
	PyObject *item;
	int more = 1;
	(void)self;
	(void)args;
	if (!Script_Import(mods)) {
		return NULL;
	}
	PyObject *ships = Script_Keep(&r, Script_Global("Traffic", "AcShips"));
	for (int i = 0; ships && (more = Script_Next(ships, i, &item)) > 0; i++) {
		more = Traffic_Profile(item, NULL, 1) ? 1 : -1;
		Py_DECREF(item);
		if (more < 0) {
			break;
		}
	}
	if (!ships || more < 0) {
		return Script_Done(&r, NULL);
	}
	PyObject *rail = Script_Global("Traffic", "AcNormalShips");
	PyObject *medium = rail ? Script_Keep(&r, Script_Global("Traffic", "AcMediumShips")) : NULL;
	rail = Script_Plus(rail, medium);
	PyObject *heavy = rail ? Script_Keep(&r, Script_Global("Traffic", "AcHeavyShips")) : NULL;
	rail = Script_Keep(&r, Script_Plus(rail, heavy));
	for (int i = 0; rail && (more = Script_Next(rail, i, &item)) > 0; i++) {
		more = Traffic_Profile(item, "Rookie", 0) ? 1 : -1;
		Py_DECREF(item);
		if (more < 0) {
			break;
		}
	}
	return Script_Done(&r, rail && more >= 0 ? Py_ReturnNone() : NULL);
}

/* EmptyFunc(o): the default of onTrackCreate */
static PyObject *Traffic_EmptyFunc(PyObject *self, PyObject *args) {
	PyObject *o;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &o)) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* ships[int(Scrap.Rand(0, len(ships)) - 0.1)] of the module list name (a new reference) */
static PyObject *Traffic_RandomShip(const char *name) {
	PyObject *ships = Script_Global("Traffic", name);
	int n = ships ? PyObject_Length(ships) : -1;
	PyObject *ship = NULL;
	if (n >= 0) {
		PyObject *f = PyFloat_FromDouble(Scrap_RandRange(0.0, (double)n) - 0.1);
		PyObject *i = Script_Int(f);
		ship = i ? PyObject_GetItem(ships, i) : NULL;
		Py_XDECREF(i);
		Py_XDECREF(f);
	}
	Py_XDECREF(ships);
	return ship;
}

/* o1 = VehiclesNPC.MakeAI(random ship of AcShips, list); o1.EntityList = list: 0 with the error set */
static byte Traffic_Make(const char *list) {
	static const char *const mods[] = { "VehiclesNPC", NULL };
	PyObject *ship = Script_Import(mods) ? Traffic_RandomShip("AcShips") : NULL;
	PyObject *o1 = ship ? Script_Ask("VehiclesNPC", "MakeAI", "(Os)", ship, list) : NULL;
	byte ok = o1 && Script_HSet(o1, "EntityList", PyString_FromString(list));
	Py_XDECREF(o1);
	Py_XDECREF(ship);
	return ok;
}

/* MakeTrafic(): one ship of the free traffic */
static PyObject *Traffic_MakeTrafic(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Traffic_Make("Traffic") ? Py_ReturnNone() : NULL;
}

/* count ships of list from the count object n (range(n)) */
static PyObject *Traffic_MakeN(PyObject *n, const char *list) {
	long count;
	if (!Script_Range(n, &count)) {
		return NULL;
	}
	for (long f = 0; f < count; f++) {
		if (!Traffic_Make(list)) {
			return NULL;
		}
	}
	return Py_ReturnNone();
}

/* InitTraffic(): iTrafficNumber ships */
static PyObject *Traffic_InitTraffic(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *n = Script_Ask("Scrap", "Get", "(s)", "iTrafficNumber");
	PyObject *r = n ? Traffic_MakeN(n, "Traffic") : NULL;
	Py_XDECREF(n);
	return r;
}

/* OnNeedMoreTraffic(n), OnNeedMoreChasingTraffic(n): the engine asks for more ships */
static PyObject *Traffic_OnNeedMoreTraffic(PyObject *self, PyObject *args) {
	PyObject *n;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &n)) {
		return NULL;
	}
	return Traffic_MakeN(n, "Traffic");
}

static PyObject *Traffic_OnNeedMoreChasingTraffic(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "VehiclesNPC", NULL };
	PyObject *n;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &n) || !Script_Import(mods)) {
		return NULL;
	}
	return Traffic_MakeN(n, "ChasingTraffic");
}

/* OnPlayerHelpedPolice(): the reward of PlayerHelpedPoliceReward and its message */
static PyObject *Traffic_OnPlayerHelpedPolice(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "RaceMaker", "SaveGame", "string", NULL };
	scriptRefs_t r = { 0 };
	(void)self;
	(void)args;
	if (!Script_Import(mods)) {
		return NULL;
	}
	PyObject *reward = Script_Keep(&r, Script_Ask("Scrap", "Get", "(s)", "PlayerHelpedPoliceReward"));
	PyObject *cad = reward ? Script_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "PlayerHelpedPolice")) : NULL;
	PyObject *rep = cad ? Script_Keep(&r, Script_Repr(reward)) : NULL;
	cad = rep ? Script_Keep(&r, Script_Ask("string", "replace", "(OsO)", cad, "<n>", rep)) : NULL;
	PyObject *money = cad ? Script_Keep(&r, Script_Ask("Scrap", "GetMoney", "()")) : NULL;
	PyObject *sum = money ? Script_Keep(&r, PyNumber_Add(money, reward)) : NULL;
	byte ok = sum && Script_Try("Scrap", "SetMoney", "(O)", sum) && Script_Try("SaveGame", "SaveMoney", "()") &&
		Script_Try("RaceMaker", "ShowSystemMessage", "(O)", cad);
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* RailTrafficDeath(Me, Killer): the alarm and the statistics when the player killed it: 0 with the error set */
static byte Traffic_RailDeath(PyObject *killer) {
	PyObject *u = Script_Usr();
	int hit = Script_True(u);
	if (hit > 0) {
		PyObject *name = Script_GetAttr(u, "Name");
		hit = Script_Eq(name, killer);
		Py_XDECREF(name);
	}
	Py_XDECREF(u);
	return hit >= 0 && (!hit || (Script_Try("Scrap", "SetAlarm", "(i)", 1) &&
		Script_Try("Scrap", "IncSaveVar", "(s)", "Stats.Traffic")));
}

static PyObject *Traffic_RailTrafficDeath(PyObject *self, PyObject *args) {
	PyObject *me, *killer;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &me, &killer)) {
		return NULL;
	}
	return Traffic_RailDeath(killer) ? Py_ReturnNone() : NULL;
}

/* the death of a rail ship of list (HeavyTraffic...) from ships (AcHeavyShips...): a new one of type, the hook
 * onTrackCreate, the death, the path back */
static PyObject *Traffic_TrackDeath(PyObject *args, const char *ships, const char *list, const char *type) {
	static const char *const mods[] = { "VehiclesNPC", NULL };
	PyObject *p1, *p2;
	scriptRefs_t r = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &p1, &p2) || !Script_Import(mods) ||
		!Script_Keep(&r, Traffic_SetPath(NULL, NULL))) {
		return Script_Done(&r, NULL);
	}
	PyObject *ship = Script_Keep(&r, Traffic_RandomShip(ships));
	PyObject *o1 = ship ? Script_Keep(&r, Script_Ask("VehiclesNPC", "MakeAI", "(Osii)", ship, list, 0, 0)) : NULL;
	PyObject *name = o1 && Script_HSet(o1, "EntityList", PyString_FromString(list)) &&
		Script_HSet(o1, "Traf_Type", PyString_FromString(type)) ? Script_Keep(&r, Script_GetAttr(o1, "Name")) : NULL;
	PyObject *hook = name && Script_Try("SAI", "SetStateVehicle", "(iO)", 4, name) ?
		Script_Keep(&r, Script_Global("Traffic", "onTrackCreate")) : NULL;
	byte ok = hook && Script_Keep(&r, Script_Invoke(hook, Py_BuildValue("(O)", o1))) && Traffic_RailDeath(p2) &&
		Script_Keep(&r, Traffic_ResetPath(NULL, NULL));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

static PyObject *Traffic_HeavyTrafficDeath(PyObject *self, PyObject *args) {
	(void)self;
	return Traffic_TrackDeath(args, "AcHeavyShips", "HeavyTraffic", "Heavy");
}

static PyObject *Traffic_NormalTrafficDeath(PyObject *self, PyObject *args) {
	(void)self;
	return Traffic_TrackDeath(args, "AcNormalShips", "NormalTraffic", "Light");
}

static PyObject *Traffic_MediumTrafficDeath(PyObject *self, PyObject *args) {
	(void)self;
	return Traffic_TrackDeath(args, "AcMediumShips", "MediumTraffic", "Medium");
}

/* InitCivilTraffic(): the death callbacks of the rail lists and NumNormal, NumHeavy and NumMedium ships */
static PyObject *Traffic_InitCivilTraffic(PyObject *self, PyObject *args) {
	static const char *const lists[3][2] = {
		{ "HeavyTraffic", "Traffic.HeavyTrafficDeath" },
		{ "NormalTraffic", "Traffic.NormalTrafficDeath" },
		{ "MediumTraffic", "Traffic.MediumTrafficDeath" },
	};
	/* the order of the script's loops */
	static const char *const nums[3] = { "NumNormal", "NumHeavy", "NumMedium" };
	static PyObject *(*const deaths[3])(PyObject *, PyObject *) = {
		Traffic_NormalTrafficDeath, Traffic_HeavyTrafficDeath, Traffic_MediumTrafficDeath,
	};
	(void)self;
	(void)args;
	for (int i = 0; i < 3; i++) {
		if (!Script_Try("Scrap", "EntityListSet", "(sss)", lists[i][0], "OnDeath", lists[i][1])) {
			return NULL;
		}
	}
	PyObject *a = Py_BuildValue("(ss)", "", "");
	for (int i = 0; a && i < 3; i++) {
		PyObject *num = Script_Global("Traffic", nums[i]);
		long count;
		byte ok = Script_Range(num, &count);
		Py_XDECREF(num);
		for (long f = 0; ok && f < count; f++) {
			PyObject *v = deaths[i](NULL, a);
			ok = v != NULL;
			Py_XDECREF(v);
		}
		if (!ok) {
			Py_DECREF(a);
			return NULL;
		}
	}
	Py_XDECREF(a);
	return a ? Py_ReturnNone() : NULL;
}

/* Init(): the profiles, the traffic and the engine callbacks */
static PyObject *Traffic_Init(PyObject *self, PyObject *args) {
	scriptRefs_t r = { 0 };
	(void)self;
	(void)args;
	byte ok = Script_Keep(&r, Traffic_InitAIProfiles(NULL, NULL)) && Script_Keep(&r, Traffic_InitTraffic(NULL, NULL)) &&
		Script_Keep(&r, Traffic_InitCivilTraffic(NULL, NULL)) &&
		Script_Try("Scrap", "Set", "(ss)", "OnNeedMoreTraffic", "Traffic.OnNeedMoreTraffic") &&
		Script_Try("Scrap", "Set", "(ss)", "OnNeedMoreChasingTraffic", "Traffic.OnNeedMoreChasingTraffic") &&
		Script_Try("Scrap", "Set", "(ss)", "OnPlayerHelpedPolice", "Traffic.OnPlayerHelpedPolice");
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

PyMethodDef g_TrafficMethods[] = {
	{ "SetPath", Traffic_SetPath, 1, NULL },
	{ "ResetPath", Traffic_ResetPath, 1, NULL },
	{ "InitAIProfiles", Traffic_InitAIProfiles, 1, NULL },
	{ "EmptyFunc", Traffic_EmptyFunc, 1, NULL },
	{ "MakeTrafic", Traffic_MakeTrafic, 1, NULL },
	{ "InitTraffic", Traffic_InitTraffic, 1, NULL },
	{ "OnNeedMoreTraffic", Traffic_OnNeedMoreTraffic, 1, NULL },
	{ "OnNeedMoreChasingTraffic", Traffic_OnNeedMoreChasingTraffic, 1, NULL },
	{ "OnPlayerHelpedPolice", Traffic_OnPlayerHelpedPolice, 1, NULL },
	{ "RailTrafficDeath", Traffic_RailTrafficDeath, 1, NULL },
	{ "HeavyTrafficDeath", Traffic_HeavyTrafficDeath, 1, NULL },
	{ "NormalTrafficDeath", Traffic_NormalTrafficDeath, 1, NULL },
	{ "MediumTrafficDeath", Traffic_MediumTrafficDeath, 1, NULL },
	{ "InitCivilTraffic", Traffic_InitCivilTraffic, 1, NULL },
	{ "Init", Traffic_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the police by reputation (police.def) ---- */

/* run wave <ship> ...: one wave appended to OutPolice.PoliceWaves */
static void Step_Wave(int id, PyObject *args) {
	(void)id;
	PyObject *waves = Script_Global("OutPolice", "PoliceWaves");
	PyObject *wave = waves ? PySequence_List(args) : NULL;
	PyObject *r = wave ? Script_Method(waves, "append", "(O)", wave) : NULL;
	if (!r) {
		PyErr_Print();
	}
	Py_XDECREF(r);
	Py_XDECREF(wave);
	Py_XDECREF(waves);
}

/* run defscale <var> <factor>: Scrap.Set(var, Scrap.Def(var) * factor) */
static void Step_DefScale(int id, PyObject *args) {
	PyObject *var, *factor;
	(void)id;
	if (!PyArg_ParseTuple(args, "OO", &var, &factor)) {
		PyErr_Print();
		return;
	}
	PyObject *def = Script_Ask("Scrap", "Def", "(O)", var);
	PyObject *v = def ? PyNumber_Multiply(def, factor) : NULL;
	if (!v || !Script_Try("Scrap", "Set", "(OO)", var, v)) {
		PyErr_Print();
	}
	Py_XDECREF(v);
	Py_XDECREF(def);
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
	/* OBJLIB 3: VehiclesNPC.MakeAI by name; the ship it gives is a Python value (OBJLIB 7) */
	if (!Mode_Import("VehiclesNPC") || !Native_CallMod("VehiclesNPC", "MakeAI", &ship, "vsi", type, "Police", 1L)) {
		return 0;
	}
	Native_SetStr(&list, "OutCop");
	/* OBJLIB 5: the hook is a function value of the module */
	PyObject *hook = Mode_VSet(&ship, "EntityList", &list) ? Mode_GlobalPy("OutPolice", "OnPoliceCreatedCallback") : NULL;
	int t = hook ? PyObject_IsTrue(hook) : -1;
	byte ok = t >= 0 && (!t || Native_Call(hook, NULL, "v", &ship));
	Py_XDECREF(hook);
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
	if (PyErr_Occurred()) {
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

/* InitAIProfiles(): the profiles SPoli1..6 of each skill, as VehiclesNPC.AIProf["SPoli1_Rookie"] (OBJLIB 3, 7: the
 * profile objects of VehiclesNPC by name) */
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
				Native_CallMod("VehiclesNPC", "SetAIProfile", NULL, "vs", &sp, types[t]) &&
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
		if (PyErr_Occurred()) {
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
		/* OBJLIB 3: RaceMaker.ShowSystemMessage by name */
		go = Mode_Import("RaceMaker") && OutPolice_G("WavePrice", wp) && OutPolice_G("iPoliceWave", i) &&
			Native_Arith('*', wp, i, a) && OutPolice_G("ReputationPrice", rp) && Native_ToInt(sv, bad) &&
			Native_Arith('*', rp, bad, b) && Native_Arith('+', a, b, price) &&
			(Native_SetInt(money, Mode_GetMoney()), Native_Arith('+', money, price, sum)) && Mode_SetMoney(sum) &&
			(Mode_Lang("OnPoliceWave", msg), Native_Arith('%', msg, price, text)) &&
			Native_CallMod("RaceMaker", "ShowSystemMessage", NULL, "v", text) ? 1 : -1;
	}
	return Mode_Done(&t, go >= 0);
}

/* the adapters (no arguments: the arity of the script's functions is not checked, as the port did) */
static PyObject *OutPolice_MakePoli(PyObject *self, PyObject *args) {
	PyObject *o;
	nativeVal_t type;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &o)) {
		return NULL;
	}
	Native_Arg(o, &type);
	byte ok = OutPolice_MakePoliN(&type);
	Native_Free(&type);
	return Mode_Result(ok);
}

#define OUTPOLICE_FN(fn, call) \
	static PyObject *fn(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		return Mode_Result(call); \
	}

OUTPOLICE_FN(OutPolice_InitShips, OutPolice_InitShipsN())
OUTPOLICE_FN(OutPolice_InitWaves, OutPolice_InitWavesN())
OUTPOLICE_FN(OutPolice_InitAIProfiles, OutPolice_InitAIProfilesN())
OUTPOLICE_FN(OutPolice_Init, OutPolice_InitN())

static PyObject *OutPolice_NextWave(PyObject *self, PyObject *args) {
	PyObject *o = NULL;
	nativeVal_t doprice;
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &o)) {
		return NULL;
	}
	if (o) {
		Native_Arg(o, &doprice);
	}
	byte ok = OutPolice_NextWaveN(o ? &doprice : NULL);
	if (o) {
		Native_Free(&doprice);
	}
	return Mode_Result(ok);
}

PyMethodDef g_OutPoliceMethods[] = {
	{ "MakePoli", OutPolice_MakePoli, 1, NULL },
	{ "Init", OutPolice_Init, 1, NULL },
	{ "NextWave", OutPolice_NextWave, 1, NULL },
	{ "InitShips", OutPolice_InitShips, 1, NULL },
	{ "InitWaves", OutPolice_InitWaves, 1, NULL },
	{ "InitAIProfiles", OutPolice_InitAIProfiles, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- InPolice ---- */

/* Include(chrname): a Sentinel, Gear or Police of CharsNPC.CharRes gets its AI and its police list */
static byte InPolice_IncludeN(const nativeVal_t *name) {
	nativeVal_t act = { 0 }, el, ch = { 0 };
	if (!Mode_Import("CharsNPC")) {
		return 0;
	}
	/* OBJLIB 5 and 7: CharRes holds the character objects of CharsNPC; CharRes[chrname] the Python way */
	PyObject *res = Script_Attr("CharsNPC", "CharRes"), *key = res ? Native_ToPy(name) : NULL;
	PyObject *ctr = key ? PyObject_GetItem(res, key) : NULL;
	byte ok = ctr && Mode_HGet(ctr, "ActCtrl", &act);
	Py_XDECREF(ctr);
	Py_XDECREF(key);
	Py_XDECREF(res);
	if (!ok) {
		return 0;
	}
	const char *s = act.type == NV_STR ? Native_Text(&act) : NULL;
	byte police = s && !strcmp(s, "Police");
	if (s && (!strcmp(s, "Sentinel") || !strcmp(s, "Gear") || police)) {
		Native_SetStr(&el, police ? "PoliceSmall" : s);
		/* OBJLIB 7: SAI.EnableAIChar by name (its NULL result for a name the AI table does not have) */
		ok = Mode_EntityH(name, &ch) && Native_CallMod("SAI", "EnableAIChar", NULL, "vii", name, 1L, 0L) &&
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

static PyObject *InPolice_Include(PyObject *self, PyObject *args) {
	PyObject *o;
	nativeVal_t name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &o)) {
		return NULL;
	}
	Native_Arg(o, &name);
	byte ok = InPolice_IncludeN(&name);
	Native_Free(&name);
	return Mode_Result(ok);
}

OUTPOLICE_FN(InPolice_SetGVars, InPolice_SetGVarsN())
OUTPOLICE_FN(InPolice_Init, InPolice_InitN())

PyMethodDef g_InPoliceMethods[] = {
	{ "Include", InPolice_Include, 1, NULL },
	{ "SetGVars", InPolice_SetGVars, 1, NULL },
	{ "Init", InPolice_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
