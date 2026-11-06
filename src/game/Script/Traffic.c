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
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

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
		PyObject *name = PyObject_GetAttrString(u, "Name");
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
		Script_HSet(o1, "Traf_Type", PyString_FromString(type)) ? Script_Keep(&r, PyObject_GetAttrString(o1, "Name")) : NULL;
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

/* int(Scrap.GetSaveVar("BadReputation")) into *rptn: 0 with the error set */
static byte Police_Reputation(long *rptn) {
	PyObject *sv = Script_SaveVar("BadReputation");
	PyObject *i = Script_Int(sv);
	Py_XDECREF(sv);
	if (!i) {
		return 0;
	}
	*rptn = PyInt_AsLong(i);
	Py_DECREF(i);
	return 1;
}

/* if Scrap.GetSaveVar("RobotsControlledByBoss") == "1": the alarm does not fall. 0 with the error set */
static byte Police_BossRobots(void) {
	PyObject *sv = Script_SaveVar("RobotsControlledByBoss");
	byte ok = sv && (!Script_Is(sv, "1") || Script_Try("Scrap", "Set", "(sd)", "AlarmFallDelta", 0.0));
	Py_XDECREF(sv);
	return ok;
}

/* ---- OutPolice ---- */

/* MakePoli(TypeShip): a police ship into OutCop, the hook OnPoliceCreatedCallback */
static PyObject *OutPolice_MakePoli(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "VehiclesNPC", NULL };
	PyObject *type;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &type) || !Script_Import(mods)) {
		return NULL;
	}
	PyObject *o1 = Script_Keep(&r, Script_Ask("VehiclesNPC", "MakeAI", "(Osi)", type, "Police", 1));
	PyObject *hook = o1 && Script_HSet(o1, "EntityList", PyString_FromString("OutCop")) ?
		Script_Keep(&r, Script_Global("OutPolice", "OnPoliceCreatedCallback")) : NULL;
	int t = Script_True(hook);
	byte ok = t >= 0 && (!t || Script_Keep(&r, Script_Invoke(hook, Py_BuildValue("(O)", o1))));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* InitShips(): the ships of the wave iPoliceWave, then the next wave (the last one stays) */
static PyObject *OutPolice_InitShips(PyObject *self, PyObject *args) {
	scriptRefs_t r = { 0 };
	PyObject *item;
	int more = -1;
	(void)self;
	(void)args;
	PyObject *waves = Script_Keep(&r, Script_Global("OutPolice", "PoliceWaves"));
	PyObject *i = waves ? Script_Keep(&r, Script_Global("OutPolice", "iPoliceWave")) : NULL;
	PyObject *wave = i ? Script_Keep(&r, PyObject_GetItem(waves, i)) : NULL;
	for (int n = 0; wave && (more = Script_Next(wave, n, &item)) > 0; n++) {
		PyObject *a = Py_BuildValue("(O)", item);
		PyObject *v = a ? OutPolice_MakePoli(NULL, a) : NULL;
		Py_XDECREF(a);
		Py_DECREF(item);
		if (!v) {
			more = -1;
			break;
		}
		Py_DECREF(v);
	}
	if (more < 0) {
		return Script_Done(&r, NULL);
	}
	PyObject *one = Script_Keep(&r, PyInt_FromLong(1));
	PyObject *cur = Script_Keep(&r, Script_Global("OutPolice", "iPoliceWave"));
	if (!cur || !Script_SetGlobal("OutPolice", "iPoliceWave", PyNumber_Add(cur, one))) {
		return Script_Done(&r, NULL);
	}
	cur = Script_Keep(&r, Script_Global("OutPolice", "iPoliceWave"));
	waves = cur ? Script_Keep(&r, Script_Global("OutPolice", "PoliceWaves")) : NULL;
	int n = waves ? PyObject_Length(waves) : -1;
	PyObject *len = n >= 0 ? Script_Keep(&r, PyInt_FromLong(n)) : NULL;
	int c = len ? PyObject_Compare(cur, len) : 0;
	if (!len || PyErr_Occurred()) {
		return Script_Done(&r, NULL);
	}
	if (c >= 0) {
		waves = Script_Keep(&r, Script_Global("OutPolice", "PoliceWaves"));
		n = waves ? PyObject_Length(waves) : -1;
		if (n < 0 || !Script_SetGlobal("OutPolice", "iPoliceWave", PyInt_FromLong(n - 1))) {
			return Script_Done(&r, NULL);
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* InitWaves(): the alarm speeds and the waves of the reputation (police.def), from the first wave */
static PyObject *OutPolice_InitWaves(PyObject *self, PyObject *args) {
	long rptn;
	(void)self;
	(void)args;
	if (!Police_Reputation(&rptn)) {
		return NULL;
	}
	if (rptn >= 0 && rptn <= 5) {
		char section[16];
		Str_Printf(section, sizeof(section), "waves%ld", rptn);
		Script_RunFile(0, POLICE_FILE, section, police_def, police_steps, NULL);
	}
	return Script_SetGlobal("OutPolice", "iPoliceWave", PyInt_FromLong(0)) ? Py_ReturnNone() : NULL;
}

/* InitAIProfiles(): the profiles SPoli1..6 of each skill, as VehiclesNPC.AIProf["SPoli1_Rookie"] */
static PyObject *OutPolice_InitAIProfiles(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "VehiclesNPC", NULL };
	static const char *const types[] = { "Rookie", "Pilot", "Veteran", "Elite", "Ace", "MegaAce" };
	(void)self;
	(void)args;
	if (!Script_Import(mods)) {
		return NULL;
	}
	for (int s = 1; s <= 6; s++) {
		for (int t = 0; t < 6; t++) {
			char ship[16], key[32];
			Str_Printf(ship, sizeof(ship), "SPoli%d", s);
			Str_Printf(key, sizeof(key), "SPoli%d_%s", s, types[t]);
			scriptRefs_t r = { 0 };
			PyObject *sp = Script_Keep(&r, Script_Ask("VehiclesNPC", "cShipAI", "()"));
			byte ok = sp && Script_HSet(sp, "Ship", PyString_FromString(ship)) &&
				Script_Try("VehiclesNPC", "SetAIProfile", "(Os)", sp, types[t]);
			PyObject *all = ok ? Script_Keep(&r, Script_Attr("VehiclesNPC", "AIProf")) : NULL;
			ok = all && PyMapping_SetItemString(all, key, sp) == 0;
			Script_Done(&r, NULL);
			if (!ok) {
				return NULL;
			}
		}
	}
	return Py_ReturnNone();
}

/* Init(): the profiles; while PoliceActive the waves, the first wave and the engine callback for the next */
static PyObject *OutPolice_Init(PyObject *self, PyObject *args) {
	scriptRefs_t r = { 0 };
	(void)self;
	(void)args;
	PyObject *active = Script_Keep(&r, OutPolice_InitAIProfiles(NULL, NULL)) ?
		Script_Keep(&r, Script_Global("OutPolice", "PoliceActive")) : NULL;
	int t = Script_True(active);
	byte ok = t >= 0 && (!t || (Script_Keep(&r, OutPolice_InitWaves(NULL, NULL)) &&
		Script_Keep(&r, OutPolice_InitShips(NULL, NULL)) &&
		Script_Try("Scrap", "Set", "(ss)", "PoliceWaveEliminated", "OutPolice.NextWave"))) && Police_BossRobots();
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* NextWave(doprice=1): the next wave; after an odd wave (not the first) while the alarm is on the reward
 * WavePrice * wave + ReputationPrice * BadReputation, unless skipPrice skips it once */
static PyObject *OutPolice_NextWave(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "RaceMaker", NULL };
	PyObject *doprice = NULL;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "|O", &doprice)) {
		return NULL;
	}
	PyObject *skip = Script_Keep(&r, OutPolice_InitShips(NULL, NULL)) &&
		Script_Try("Scrap", "Set", "(ss)", "PoliceWaveEliminated", "OutPolice.NextWave") ?
		Script_Keep(&r, Script_Global("OutPolice", "skipPrice")) : NULL;
	int t = Script_True(skip);
	if (t < 0) {
		return Script_Done(&r, NULL);
	}
	if (t) {
		return Script_Done(&r, Script_SetGlobal("OutPolice", "skipPrice", PyInt_FromLong(0)) ? Py_ReturnNone() : NULL);
	}
	t = doprice ? Script_True(doprice) : 1;
	if (t > 0) {
		PyObject *alarm = Script_Keep(&r, Script_Ask("Scrap", "GetAlarm", "()"));
		t = Script_True(alarm ? Script_Keep(&r, Script_Index(alarm, 0)) : NULL);
	}
	PyObject *i = NULL;
	if (t > 0) {
		i = Script_Keep(&r, Script_Global("OutPolice", "iPoliceWave"));
		PyObject *one = i ? Script_Keep(&r, PyInt_FromLong(1)) : NULL;
		t = one ? Script_Eq(i, one) : -1;
		t = t < 0 ? -1 : !t;
	}
	if (t > 0) {
		i = Script_Keep(&r, Script_Global("OutPolice", "iPoliceWave"));
		PyObject *two = i ? Script_Keep(&r, PyInt_FromLong(2)) : NULL;
		t = Script_True(two ? Script_Keep(&r, Script_Mod(i, two)) : NULL);
	}
	if (t > 0) {
		if (!Script_Import(mods)) {
			return Script_Done(&r, NULL);
		}
		PyObject *wp = Script_Keep(&r, Script_Global("OutPolice", "WavePrice"));
		i = wp ? Script_Keep(&r, Script_Global("OutPolice", "iPoliceWave")) : NULL;
		PyObject *a = i ? Script_Keep(&r, PyNumber_Multiply(wp, i)) : NULL;
		PyObject *rp = a ? Script_Keep(&r, Script_Global("OutPolice", "ReputationPrice")) : NULL;
		PyObject *bad = rp ? Script_Keep(&r, Script_Int(Script_Keep(&r, Script_SaveVar("BadReputation")))) : NULL;
		PyObject *b = bad ? Script_Keep(&r, PyNumber_Multiply(rp, bad)) : NULL;
		PyObject *price = b ? Script_Keep(&r, PyNumber_Add(a, b)) : NULL;
		PyObject *money = price ? Script_Keep(&r, Script_Ask("Scrap", "GetMoney", "()")) : NULL;
		PyObject *sum = money ? Script_Keep(&r, PyNumber_Add(money, price)) : NULL;
		PyObject *msg = sum && Script_Try("Scrap", "SetMoney", "(O)", sum) ?
			Script_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "OnPoliceWave")) : NULL;
		PyObject *text = msg ? Script_Keep(&r, Script_Mod(msg, price)) : NULL;
		t = text && Script_Try("RaceMaker", "ShowSystemMessage", "(O)", text) ? 1 : -1;
	}
	return Script_Done(&r, t >= 0 ? Py_ReturnNone() : NULL);
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
static PyObject *InPolice_Include(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "CharsNPC", NULL };
	static const char *const own[] = { "Sentinel", "Gear", NULL };
	PyObject *name;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name) || !Script_Import(mods)) {
		return NULL;
	}
	PyObject *res = Script_Keep(&r, Script_Attr("CharsNPC", "CharRes"));
	PyObject *ctr = res ? Script_Keep(&r, PyObject_GetItem(res, name)) : NULL;
	PyObject *act = ctr ? Script_Keep(&r, PyObject_GetAttrString(ctr, "ActCtrl")) : NULL;
	if (!act) {
		return Script_Done(&r, NULL);
	}
	PyObject *el = Script_In(act, own) ? act : Script_Is(act, "Police") ? Script_Keep(&r, PyString_FromString("PoliceSmall")) : NULL;
	if (el) {
		PyObject *ch = Script_Keep(&r, Script_Entity(name));
		if (!ch || !Script_Try("SAI", "EnableAIChar", "(Oii)", name, 1, 0) || !Script_HSet(ch, "EntityList", Script_Ref(el))) {
			return Script_Done(&r, NULL);
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SetGVars(): the alarm speeds and the police tuning of the reputation (police.def: in0, in1, in2 for the rest) */
static PyObject *InPolice_SetGVars(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "CharAct", NULL };
	long rptn;
	(void)self;
	(void)args;
	if (!Script_Import(mods) || !Police_Reputation(&rptn)) {
		return NULL;
	}
	Script_RunFile(0, POLICE_FILE, rptn == 0 ? "in0" : rptn == 1 ? "in1" : "in2", police_def, police_steps, NULL);
	return Py_ReturnNone();
}

/* Init(): the tuning and the engine callbacks that create Gears and Police */
static PyObject *InPolice_Init(PyObject *self, PyObject *args) {
	scriptRefs_t r = { 0 };
	(void)self;
	(void)args;
	byte ok = Script_Keep(&r, InPolice_SetGVars(NULL, NULL)) &&
		Script_Try("Scrap", "Set", "(ss)", "CreateGear", "CharsNPC.CallCreateChar") &&
		Script_Try("Scrap", "Set", "(ss)", "CreatePolice", "CharsNPC.CallCreateChar") && Police_BossRobots();
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

PyMethodDef g_InPoliceMethods[] = {
	{ "Include", InPolice_Include, 1, NULL },
	{ "SetGVars", InPolice_SetGVars, 1, NULL },
	{ "Init", InPolice_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
