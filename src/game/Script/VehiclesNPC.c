/*
 * The VehiclesNPC module in C (scripts/vehicles/vehiclesnpc.py, issue #2): the ships of the AI. The profiles of their
 * behaviour, the hired mercenaries by reputation and the ship classes of the traffic models are data
 * (data/vehicles/vehiclesnpc.def); here the table of the standard ships, the ships made with a profile, their set up
 * and the hired mercenaries. The class cShipAI, StdShips, AIProf, Mercs, UseMercs and lmigd stay variables of the
 * module, its code (the save variable StdShipAIProfile) stays in the script.
 */
#include "scrap/script.h"
#include "scrap/scrap.h"

static const char npc_def[] = {
#embed "../../../data/vehicles/vehiclesnpc.def"
	, 0 };

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

/* "run notfound" of the profile chain: the profile was not in the file */
static byte g_NpcNotFound;

static void Npc_StepNotFound(int id, PyObject *args) {
	(void)id;
	(void)args;
	g_NpcNotFound = 1;
}

static const scriptStep_t npc_steps[] = {
	{ "notfound", Npc_StepNotFound },
	{ NULL, NULL },
};

/* a section of vehiclesnpc.def with the values of vars (a dict given away): 0 with the error set */
static byte Npc_Run(const char *section, PyObject *vars) {
	byte ok = vars && Script_RunFileTry(0, "vehicles/vehiclesnpc.def", section, npc_def, npc_steps, vars);
	Py_XDECREF(vars);
	return ok;
}

/* fn(args) through the module global fn (args given away): the result, a new reference */
static PyObject *Npc_Call(const char *fn, PyObject *args) {
	PyObject *f = Script_Global("VehiclesNPC", fn);
	if (!f) {
		Py_XDECREF(args);
		return NULL;
	}
	PyObject *v = Script_Invoke(f, args);
	Py_DECREF(f);
	return v;
}

/* string.upper(s) (a new reference) */
static PyObject *Npc_Upper(PyObject *s) {
	return Script_Ask("string", "upper", "(O)", s);
}

/* StdShips[key] = value (value given away) */
static byte Npc_Std(PyObject *ships, PyObject *key, PyObject *value) {
	int bad = !key || !value || PyObject_SetItem(ships, key, value) < 0;
	Py_XDECREF(value);
	return !bad;
}

/* the standard ships of the traffic models of one list of Traffic: (model, "", "", "", "", "0,0,0,0,0,0", life, "") */
static byte Npc_StdTraffic(PyObject *ships, const char *list, long life) {
	PyObject *names = Script_Attr("Traffic", list), *name;
	int more;
	if (!names) {
		return 0;
	}
	for (int i = 0; (more = Script_Next(names, i, &name)) > 0; i++) {
		PyObject *key = Npc_Upper(name);
		byte ok = Npc_Std(ships, key, Py_BuildValue("(Osssssls)", name, "", "", "", "", "0,0,0,0,0,0", life, ""));
		Py_XDECREF(key);
		Py_DECREF(name);
		if (!ok) {
			Py_DECREF(names);
			return 0;
		}
	}
	Py_DECREF(names);
	return more == 0;
}

/* InitStdShips(): the ships of the characters (model and pilot), the scrapyard's and the traffic's (all the data) */
static PyObject *VehiclesNPC_InitStdShips(PyObject *self, PyObject *args) {
	static const char *const std[][3] = {
		{ "SPOLI1", "SPOLI1", "P_Police" }, { "SPOLI2", "SPOLI2", "P_Police" }, { "SPOLI3", "SPOLI3", "P_Police" },
		{ "SPOLI4", "SPOLI4", "P_Police" }, { "SPOLI5", "SPOLI5", "P_Police" }, { "SPOLI6", "SPOLIBOSS1", "P_Police" },
		{ "SPOLIBOSS1", "SPOLIBOSS1", "P_PoliceBoss" }, { "SBANKER1", "SBANKER1", "P_BankDirector" },
		{ "SBANKMASTER1", "SBANKMASTER1", "P_BankMaster" }, { "SCRAZYGAMBLER1", "SBANKMASTER1", "P_CrazyGambler" },
		{ "SMERC1", "SMERC1", "P_MercenaryB" }, { "SMERC2", "SMERC2", "P_MercenaryC" },
	};
	static const char *const std2[][3] = {
		{ "SMAYOR1", "SMAYOR1", "P_Mayor" }, { "SBERTO1", "SBERTO1", "P_Berto" }, { "SHUMP1", "SHUMP1", "P_Humphrey" },
		{ "SBOSS1", "SBOSS1", "P_Boss" }, { "SBETTY1", "SBETTY1", "P_Betty" }, { "SFUNC1", "SFUNC1", "P_Functionary" },
		{ "SBISHOP1", "SBISHOP1", "P_Bishop" }, { "SARCHBISHOP1", "SARCHBISHOP1", "P_Bishop" },
	};
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *ships = Script_Import((const char *const[]){ "Init", NULL }) ? K(Script_Global("VehiclesNPC", "StdShips")) : NULL;
	if (!ships) {
		FAIL();
	}
	for (size_t i = 0; i < sizeof(std) / sizeof(std[0]); i++) {
		if (!Npc_Std(ships, K(PyString_FromString((char *)std[i][0])), Py_BuildValue("(ss)", std[i][1], std[i][2]))) {
			FAIL();
		}
	}
	PyObject *lmigd = K(Script_Global("VehiclesNPC", "lmigd"));
	int lonely = lmigd ? Script_True(lmigd) : -1;
	if (lonely < 0) {
		FAIL();
	}
	const char *mercA = lonely ? "P_MercenaryL" : "P_MercenaryA";
	if (!Npc_Std(ships, K(PyString_FromString("SMERC3")), Py_BuildValue("(ss)", "SMERC2", mercA)) ||
		!Npc_Std(ships, K(PyString_FromString("SLMERC")), Py_BuildValue("(ss)", "SMERC3", mercA))) {
		FAIL();
	}
	for (size_t i = 0; i < sizeof(std2) / sizeof(std2[0]); i++) {
		if (!Npc_Std(ships, K(PyString_FromString((char *)std2[i][0])), Py_BuildValue("(ss)", std2[i][1], std2[i][2]))) {
			FAIL();
		}
	}
	PyObject *yard = K(Script_Attr("Init", "isScrapYard"));
	int y = yard ? Script_True(yard) : -1;
	if (y < 0 || (y && (!Npc_Std(ships, K(PyString_FromString("MADHUNT")), Py_BuildValue("(ssssssis)", "MadHunt", "", "", "", "", "7,0,0,7,0,0", 75, "")) ||
		!Npc_Std(ships, K(PyString_FromString("SMOSQUITO")), Py_BuildValue("(ssssssis)", "SMosquito", "", "", "", "", "3,0,0,0,0,0", 25, ""))))) {
		FAIL();
	}
	if (!Script_Import((const char *const[]){ "Traffic", "string", NULL }) || !Npc_StdTraffic(ships, "AcNormalShips", 50) ||
		!Npc_StdTraffic(ships, "AcMediumShips", 300) || !Npc_StdTraffic(ships, "AcHeavyShips", 500)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* MakeAI(TypeShip, Name="Traffic", FarPos=0, InitState=1): a ship of that type at a free spawn point (FarPos: one of
 * ten tries far from the cameras) with its profile */
static PyObject *VehiclesNPC_MakeAI(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "TypeShip", "Name", "FarPos", "InitState", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *type, *name = NULL, *far = NULL, *init = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|OOO", names, &type, &name, &far, &init) ||
		!Script_Import((const char *const[]){ "Vehicles", "ShipMaker", "OutMap", NULL })) {
		return NULL;
	}
	if (!name) {
		name = K(PyString_FromString("Traffic"));
	}
	if (!init) {
		init = K(PyInt_FromLong(1));
	}
	int f = far ? Script_True(far) : 0;
	if (f < 0) {
		FAIL();
	}
	PyObject *sp = NULL;
	for (int i = 0; i < (f ? 10 : 1); i++) {
		Py_XDECREF(sp);
		sp = Script_Ask("OutMap", "GetFreeSpawnPos", "()");
		if (!sp || !f) {
			break;
		}
		scriptRefs_t q = { .n = 0 };
		PyObject *at = Script_Keep(&q, Script_Index(sp, 0)), *x = at ? Script_Keep(&q, Script_Index(at, 0)) : NULL;
		PyObject *y = x ? Script_Keep(&q, Script_Index(at, 1)) : NULL, *z = y ? Script_Keep(&q, Script_Index(at, 2)) : NULL;
		PyObject *d = z ? Script_Keep(&q, Script_Ask("Scrap", "GetMinCamDist", "(OOO)", x, y, z)) : NULL;
		int c = d ? Script_NumCmp(d, 30000) : -2;
		Script_Done(&q, NULL);
		if (c == -2) {
			Py_DECREF(sp);
			sp = NULL;
			break;
		}
		if (c > 0) {
			break;
		}
	}
	K(sp);
	PyObject *at = sp ? K(Script_Index(sp, 0)) : NULL, *x = at ? K(Script_Index(at, 0)) : NULL;
	PyObject *y = x ? K(Script_Index(at, 1)) : NULL, *z = y ? K(Script_Index(at, 2)) : NULL;
	PyObject *o1 = z ? K(Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "Car")) : NULL;
	PyObject *a = o1 ? K(Script_Index(sp, 1)) : NULL;
	if (!a || !Script_HSet(o1, "Ang", Py_BuildValue("(iOi)", 0, a, 0))) {
		FAIL();
	}
	return Script_Done(&r, Npc_Call("SetIAProf", Py_BuildValue("(OOO)", o1, type, init)));
}

/* GetIAProf(TypeShip): the profile of the type, made once with the profile of the save variable StdShipAIProfile */
static PyObject *VehiclesNPC_GetIAProf(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *type;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &type)) {
		return NULL;
	}
	PyObject *prof = K(Script_Global("VehiclesNPC", "AIProf")), *has = prof ? K(Script_Method(prof, "has_key", "(O)", type)) : NULL;
	int known = has ? Script_True(has) : -1;
	if (known < 0) {
		FAIL();
	}
	if (known) {
		return Script_Done(&r, PyObject_GetItem(prof, type));
	}
	PyObject *cls = K(Script_Global("VehiclesNPC", "cShipAI")), *sp = cls ? K(Script_Invoke(cls, PyTuple_New(0))) : NULL;
	PyObject *std = sp && PyObject_SetAttrString(sp, "Ship", type) == 0 ? K(Script_SaveVar("StdShipAIProfile")) : NULL;
	if (!std || !K(Npc_Call("SetAIProfile", Py_BuildValue("(OO)", sp, std)))) {
		FAIL();
	}
	prof = K(Script_Global("VehiclesNPC", "AIProf"));
	if (!prof || PyObject_SetItem(prof, type, sp) < 0) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(sp));
}

/* ModAIProff(o1, pd, useLife=0): the AI of the ship from the profile (and its life by LifeMult) */
static PyObject *VehiclesNPC_ModAIProff(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "o1", "pd", "useLife", NULL };
	static const char *const vars[] = { "FactorDamageGetAway", "ControlStrafe", "ControlBrake", "ControlBoost",
		"StupidPathFinding", "TimeMaxStrafe", "VelMaxStrafe", "ControlAim", "CadenceShoot", "AngAttack",
		"FactorDamageItemLife", "ControlBoostItemLife", "ControlBoostItemAmmo", "FactorItemAmmo", "CounterAttack",
		"ControlHookRace", "FactorHookMap", "ControlStrafeEvasion", "CadenceShootCM", "UsePercSonic", "UsePercCloud",
		"UsePercElectro", "UsePercHook", "UsePercHookGetAway" };
	enum { NVARS = sizeof(vars) / sizeof(vars[0]) };
	scriptRefs_t r = { .n = 0 };
	PyObject *o1, *pd, *use = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|O", names, &o1, &pd, &use)) {
		return NULL;
	}
	PyObject *a = K(PyTuple_New(NVARS + 2)), *name = a ? PyObject_GetAttrString(o1, "Name") : NULL;
	if (!name) {
		FAIL();
	}
	PyTuple_SET_ITEM(a, 0, name);
	PyTuple_SET_ITEM(a, 1, PyInt_FromLong(1));
	for (int i = 0; i < NVARS; i++) {
		PyObject *v = PyObject_GetAttrString(pd, (char *)vars[i]);
		if (!v) {
			FAIL();
		}
		PyTuple_SET_ITEM(a, i + 2, v);
	}
	PyObject *f = K(Script_Attr("SAI", "EnableAIVehicle"));
	if (!f || !K(Script_Invoke(f, Script_Ref(a)))) {
		FAIL();
	}
	int life = use ? Script_True(use) : 0;
	if (life < 0) {
		FAIL();
	}
	if (life) {
		PyObject *l = K(PyObject_GetAttrString(o1, "Life")), *m = l ? K(PyObject_GetAttrString(pd, "LifeMult")) : NULL;
		if (!m || !Script_HSet(o1, "Life", PyNumber_Multiply(l, m))) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SetIAProf(o1, TypeShip, InitState=1): the ship gets the type's model and AI, its route, its life by LifeMult */
static PyObject *VehiclesNPC_SetIAProf(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "o1", "TypeShip", "InitState", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *o1, *type, *init = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|O", names, &o1, &type, &init)) {
		return NULL;
	}
	PyObject *pd = K(Npc_Call("GetIAProf", Py_BuildValue("(O)", type))), *name = pd ? K(PyObject_GetAttrString(o1, "Name")) : NULL;
	PyObject *ship = name ? K(PyObject_GetAttrString(pd, "Ship")) : NULL;
	if (!ship || !K(Npc_Call("SetStdProperties", Py_BuildValue("(OO)", name, ship))) ||
		!K(Npc_Call("ModAIProff", Py_BuildValue("(OO)", o1, pd)))) {
		FAIL();
	}
	int st = init ? Script_True(init) : 1;
	PyObject *n2 = st > 0 ? K(PyObject_GetAttrString(o1, "Name")) : NULL;
	if (st < 0 || (st && (!n2 || !Script_Try("SAI", "SetStateVehicle", "(iO)", 3, n2)))) {
		FAIL();
	}
	PyObject *l = K(PyObject_GetAttrString(o1, "Life")), *m = l ? K(PyObject_GetAttrString(pd, "LifeMult")) : NULL;
	if (!m || !Script_HSet(o1, "Life", PyNumber_Multiply(l, m))) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(o1));
}

/* SetAIProfile(sp, profileType): the attributes of the profile (vehiclesnpc.def), the console error of an unknown one */
static PyObject *VehiclesNPC_SetAIProfile(PyObject *self, PyObject *args) {
	PyObject *sp, *type;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &sp, &type)) {
		return NULL;
	}
	g_NpcNotFound = 0;
	if (!Npc_Run("Profile", Py_BuildValue("{s:O,s:O}", "sp", sp, "type", type))) {
		return NULL;
	}
	if (!g_NpcNotFound) {
		return Py_ReturnNone();
	}
	g_NpcNotFound = 0;
	PyObject *msg = Script_PlusS(Script_SPlus("ERROR : SetAIProfile('", type), "')\n   |____\\ Profile not found.\n        /\n");
	byte ok = msg && Script_Try("Scrap", "ConsoleError", "(O)", msg);
	Py_XDECREF(msg);
	return ok ? Py_ReturnNone() : NULL;
}

/* SetStdProperties(EntityName, Model): the standard ship of the model (by Vehicles), its weapons checked, half ammo */
static PyObject *VehiclesNPC_SetStdProperties(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *model;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &model) ||
		!Script_Import((const char *const[]){ "ShipMaker", "Vehicles", "string", "Parking", NULL })) {
		return NULL;
	}
	PyObject *std = K(Script_Global("VehiclesNPC", "StdShips")), *up = std ? K(Npc_Upper(model)) : NULL;
	PyObject *a = up ? K(PyObject_GetItem(std, up)) : NULL;
	int n = a ? PyObject_Length(a) : -1;
	if (n < 0) {
		FAIL();
	}
	if (n == 2) {
		PyObject *m = K(Script_Index(a, 0)), *p = m ? K(Script_Index(a, 1)) : NULL;
		if (!p || !Script_Try("Vehicles", "SetStdProperties", "(OOO)", name, m, p)) {
			FAIL();
		}
	} else {
		static const int order[] = { 0, 6, 7, 1, 2, 3, 4, 5 };
		PyObject *it[8];
		for (int i = 0; i < 8; i++) {
			if (!(it[i] = K(Script_Index(a, order[i])))) {
				FAIL();
			}
		}
		PyObject *up2 = K(Npc_Upper(model)), *title = up2 ? K(Script_LangOf(Script_SPlus("SHIP_", up2))) : NULL;
		if (!title || !Script_Try("Vehicles", "SetProperties", "(OOOOOOOOOO)", name, it[0], it[1], it[2], it[3], it[4], it[5], it[6],
			it[7], title)) {
			FAIL();
		}
	}
	PyObject *o1 = K(Script_Entity(name));
	if (!o1 || !Script_Try("Parking", "CheckWeapons", "(O)", o1)) {
		FAIL();
	}
	static const char *const ammo[] = { "Ammo00", "Ammo01", "Ammo02" };
	for (int i = 0; i < 3; i++) {
		PyObject *max = K(Script_Ask("SWeap", "GetFAmmo", "(is)", i, "Max")), *two = max ? K(PyInt_FromLong(2)) : NULL;
		if (!two || !Script_HSet(o1, ammo[i], PyNumber_Divide(max, two))) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* MercOnDeath(LooserName, WinnerName): a hired mercenary dies and is no longer hired */
static PyObject *VehiclesNPC_MercOnDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	(void)self;
	return PyArg_ParseTuple(args, "OO", &looser, &winner) && Script_Import((const char *const[]){ "Vehicles", NULL }) &&
		Script_Try("Vehicles", "OnDeath", "(OO)", looser, winner) && Script_Try("Scrap", "SetSaveVar", "(Os)", looser, "0") ?
		Py_ReturnNone() : NULL;
}

/* the hired mercenary of Mercs key (its save variable is "1") as a friendly ship with the profile of the reputation */
static byte Npc_Merc(PyObject *mercs, PyObject *key) {
	scriptRefs_t r = { .n = 0 };
	PyObject *var = K(PyObject_GetItem(mercs, key)), *hired = var ? K(Script_SaveVar(PyString_Check(var) ? PyString_AsString(var) : "")) : NULL;
	if (!hired) {
		return Script_Done(&r, NULL), 0;
	}
	if (!Script_Is(hired, "1")) {
		return Script_Done(&r, NULL), 1;
	}
	PyObject *usr = K(Script_PlusS(Script_Ref(key), "_usr")), *sp = usr ? K(Npc_Call("GetIAProf", Py_BuildValue("(O)", usr))) : NULL;
	PyObject *lmigd = sp ? K(Script_Global("VehiclesNPC", "lmigd")) : NULL;
	int lonely = lmigd ? Script_True(lmigd) : -1;
	if (lonely < 0 || PyObject_SetAttrString(sp, "Ship", lonely && Script_Is(key, "SMERC2") ? K(PyString_FromString("SMERC3")) : key) < 0 ||
		!Npc_Run("MercBase", Py_BuildValue("{s:O}", "sp", sp))) {
		return Script_Done(&r, NULL), 0;
	}
	PyObject *rep = K(Script_SaveVar("BadReputation")), *rptn = rep ? K(Script_Int(rep)) : NULL;
	PyObject *ship = rptn && Npc_Run("Merc", Py_BuildValue("{s:O,s:O}", "sp", sp, "rptn", rptn)) ?
		K(Npc_Call("MakeAI", Py_BuildValue("(OOii)", usr, var, 0, 0))) : NULL;
	byte ok = ship && Script_HSet(ship, "EntityList", PyString_FromString("Friends")) &&
		Script_HSet(ship, "OnDeath", PyString_FromString("VehiclesNPC.MercOnDeath"));
	Script_Done(&r, NULL);
	return ok;
}

static PyObject *VehiclesNPC_InitMercs(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *mercs = Script_Global("VehiclesNPC", "Mercs"), *keys = mercs ? Script_Method(mercs, "keys", "()") : NULL, *key;
	int more = keys ? 1 : -1;
	for (int i = 0; keys && (more = Script_Next(keys, i, &key)) > 0; i++) {
		byte ok = Npc_Merc(mercs, key);
		Py_DECREF(key);
		if (!ok) {
			more = -1;
			break;
		}
	}
	Py_XDECREF(keys);
	Py_XDECREF(mercs);
	return more == 0 ? Py_ReturnNone() : NULL;
}

/* the ship classes of the traffic models of one list (a section of vehiclesnpc.def per model) */
static byte Npc_Classes(const char *list, const char *section) {
	PyObject *names = Script_Attr("Traffic", list), *name;
	int more;
	if (!names) {
		return 0;
	}
	for (int i = 0; (more = Script_Next(names, i, &name)) > 0; i++) {
		byte ok = Npc_Run(section, Py_BuildValue("{s:O}", "name", name));
		Py_DECREF(name);
		if (!ok) {
			more = -1;
			break;
		}
	}
	Py_DECREF(names);
	return more == 0;
}

/* Init(): the ship classes, the standard ships, the hired mercenaries (scheduled) outside the super deals */
static PyObject *VehiclesNPC_Init(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	if (!Script_Import((const char *const[]){ "ShipMaker", "Traffic", "Init", "string", NULL })) {
		return NULL;
	}
	PyObject *yard = K(Script_Attr("Init", "isScrapYard"));
	int y = yard ? Script_True(yard) : -1;
	if (y < 0 || (y && !Npc_Run("ScrapYard", PyDict_New())) || !Script_Try("Traffic", "SetPath", "()") ||
		!Npc_Classes("AcNormalShips", "Normal") || !Npc_Classes("AcMediumShips", "Medium") || !Npc_Classes("AcHeavyShips", "Heavy") ||
		!Script_Try("Traffic", "ResetPath", "()") || !K(Npc_Call("InitStdShips", PyTuple_New(0)))) {
		FAIL();
	}
	PyObject *use = K(Script_Global("VehiclesNPC", "UseMercs"));
	int u = use ? Script_True(use) : -1;
	if (u <= 0) {
		return Script_Done(&r, u < 0 ? NULL : Py_ReturnNone());
	}
	PyObject *map = K(Script_Attr("Init", "UpperMap")), *deals = map ? K(Script_Attr("Init", "SUPERDEALSMAPS")) : NULL;
	int in = deals ? PySequence_Contains(deals, map) : -1;
	if (in < 0) {
		FAIL();
	}
	if (in || Script_Is(map, "JUNKYARD")) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *f = K(Script_Global("VehiclesNPC", "InitMercs"));
	return Script_Done(&r, f && Script_Try("Scrap", "AddScheduledFunc", "(iO()s)", 0, f, "InitMercs") ? Py_ReturnNone() : NULL);
}

PyMethodDef g_VehiclesNPCMethods[] = {
	{ "InitStdShips", VehiclesNPC_InitStdShips, 1, NULL },
	{ "MakeAI", (PyCFunction)(void (*)(void))VehiclesNPC_MakeAI, 3, NULL },
	{ "GetIAProf", VehiclesNPC_GetIAProf, 1, NULL },
	{ "ModAIProff", (PyCFunction)(void (*)(void))VehiclesNPC_ModAIProff, 3, NULL },
	{ "SetIAProf", (PyCFunction)(void (*)(void))VehiclesNPC_SetIAProf, 3, NULL },
	{ "SetAIProfile", VehiclesNPC_SetAIProfile, 1, NULL },
	{ "SetStdProperties", VehiclesNPC_SetStdProperties, 1, NULL },
	{ "MercOnDeath", VehiclesNPC_MercOnDeath, 1, NULL },
	{ "InitMercs", VehiclesNPC_InitMercs, 1, NULL },
	{ "Init", VehiclesNPC_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
