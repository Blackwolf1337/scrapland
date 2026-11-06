/*
 * The ShipMaker module in C (scripts/vehicles/shipmaker.py, issue #2): the tables of the ship hulls, engines, weapons
 * and pilots, the animated ship classes, and SetShipParams, which computes mass, style, engine power, armour, boost
 * time and vertical balance of a ship from its parts. The data Init adds (the hulls, engines and weapons of the game
 * and the standard ships of CreateStdShips) is data/vehicles/shipmaker.def.
 *
 * The tables stay variables of the module, other scripts read them (the ship editor, VehiclesNPC, the maps): ShipTon,
 * ShipValues, ShipBalance, ShipCallback, MotorValues, WeaponValues, StdShips, Chars, PilotsList, WeaponCosts,
 * shipsCosts, enginesCosts, FLAPS_ANM, MOTOR_ANM, PILOTAMN and armorPieceCost. The module code (PilotsList with the
 * lonely mercenary, AmorPieceWeight) stays in the script.
 */
#include "scrap/scrap.h"
#include "scrap/script.h"

static const char shipmaker_def[] = {
#embed "../../../data/vehicles/shipmaker.def"
	, 0 };

#define SHIPMAKER_FILE "vehicles/shipmaker.def"
#define SM "ShipMaker"

/* table[string.upper(name)] (the script imports string first) */
static PyObject *Sm_Upper(scriptFrame_t *f, PyObject *name) {
	Sf_Import(f, "string");
	return Sf_Ask(f, "string", "upper", "(O)", name);
}

/* global table; table[string.upper(name)] = v */
static PyObject *Sm_Put(scriptFrame_t *f, const char *table, PyObject *name, PyObject *v) {
	PyObject *key = Sm_Upper(f, name);
	Sf_SetItem(f, Sf_Global(f, SM, table), key, v);
	return Sf_None(f);
}

/* return table[string.upper(name)] */
static PyObject *Sm_Get(scriptFrame_t *f, const char *table, PyObject *name) {
	PyObject *key = Sm_Upper(f, name);
	return Sf_Return(f, Sf_GetItem(f, Sf_Global(f, SM, table), key));
}

/* SetShipPrice(name, Price): shipsCosts holds the repr of the price */
static PyObject *ShipMaker_SetShipPrice(PyObject *self, PyObject *args) {
	PyObject *name, *price;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &price)) {
		return NULL;
	}
	PyObject *key = Sm_Upper(&f, name);
	Sf_SetItem(&f, Sf_Global(&f, SM, "shipsCosts"), key, Sf_Repr(&f, price));
	return Sf_None(&f);
}

static PyObject *ShipMaker_GetShipPrice(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	return Sm_Get(&f, "shipsCosts", name);
}

static PyObject *ShipMaker_SetEnginePrice(PyObject *self, PyObject *args) {
	PyObject *name, *price;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &price)) {
		return NULL;
	}
	return Sm_Put(&f, "enginesCosts", name, price);
}

static PyObject *ShipMaker_GetEnginePrice(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	return Sm_Get(&f, "enginesCosts", name);
}

/* AddShipTon(name, StandardArmor, TopArmor, TopMass) */
static PyObject *ShipMaker_AddShipTon(PyObject *self, PyObject *args) {
	PyObject *name, *a, *b, *c;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &name, &a, &b, &c)) {
		return NULL;
	}
	return Sm_Put(&f, "ShipTon", name, Sf_Build(&f, "(OOO)", a, b, c));
}

static PyObject *ShipMaker_GetShipTon(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	return Sm_Get(&f, "ShipTon", name);
}

static PyObject *ShipMaker_AddShipBalance(PyObject *self, PyObject *args) {
	PyObject *name, *val;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &val)) {
		return NULL;
	}
	return Sm_Put(&f, "ShipBalance", name, val);
}

static PyObject *ShipMaker_GetShipBalance(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	return Sm_Get(&f, "ShipBalance", name);
}

/* AddShip(name, Mass, Style, Engine, BoostTime=10, TopBoostTime=10) */
static PyObject *ShipMaker_AddShip(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "name", "Mass", "Style", "Engine", "BoostTime", "TopBoostTime", NULL };
	PyObject *name, *mass, *style, *engine, *boost = NULL, *top = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOOO|OO", names, &name, &mass, &style, &engine, &boost, &top)) {
		return NULL;
	}
	boost = boost ? Sf_Ref(&f, boost) : Sf_Int(&f, 10);
	top = top ? Sf_Ref(&f, top) : Sf_Int(&f, 10);
	return Sm_Put(&f, "ShipValues", name, Sf_Build(&f, "(OOOOO)", mass, style, engine, boost, top));
}

static PyObject *ShipMaker_GetShip(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	return Sm_Get(&f, "ShipValues", name);
}

/* (Anm & FLAPS_ANM) != 0 */
static int Sm_HasAnm(scriptFrame_t *f, PyObject *anm, const char *flag) {
	PyObject *bits = Sf_Global(f, SM, flag);
	PyObject *and = f->failed || !anm || !bits ? NULL : Sf_Keep(f, PyNumber_And(anm, bits));
	return Sf_Compare(f, and, "!=", Sf_Int(f, 0));
}

/* AddMotor(name, Mass, Style, Engine, Anm): the values and the preloaded model with its animations */
static PyObject *ShipMaker_AddMotor(PyObject *self, PyObject *args) {
	PyObject *name, *mass, *style, *engine, *anm;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOOO", &name, &mass, &style, &engine, &anm)) {
		return NULL;
	}
	PyObject *key = Sm_Upper(&f, name);
	Sf_SetItem(&f, Sf_Global(&f, SM, "MotorValues"), key, Sf_Build(&f, "(OOOO)", mass, style, engine, anm));
	PyObject *path = Sf_Cat(&f, "sOs", "Models/Vehicles/Engines/", name, "/");
	PyObject *file = Sf_Cat(&f, "OOs", path, name, ".M3D");
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(Oiii)", file, 1, 1, 1);
	if (Sm_HasAnm(&f, anm, "FLAPS_ANM") > 0) {
		Sf_Ask(&f, "Scrap", "PreloadAnm", "(OO)", file, Sf_Cat(&f, "Os", path, "Flaps.M3D"));
	}
	if (Sm_HasAnm(&f, anm, "MOTOR_ANM") > 0) {
		Sf_Ask(&f, "Scrap", "PreloadAnm", "(OO)", file, Sf_Cat(&f, "Os", path, "Motor.M3D"));
	}
	Sf_Ask(&f, "Scrap", "Verbose", "(O)", Sf_Cat(&f, "sOs", "Engine : '", name, "'  Loaded.\n"));
	return Sf_None(&f);
}

/* GetMotor(name): (0, 0, 0, 0) for no engine */
static PyObject *ShipMaker_GetMotor(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	int none = Sf_EqS(&f, name, "");
	if (none > 0) {
		return Sf_Return(&f, Sf_Build(&f, "(iiii)", 0, 0, 0, 0));
	}
	if (none < 0) {
		return Sf_None(&f);
	}
	return Sm_Get(&f, "MotorValues", name);
}

/* IncludeMotor(e, neng, name, n, Mass, Style, Engine): (Mass, Style, Engine) with n engines name; sets the flaps and
 * motor animations and the effect class of engine neng (the script builds the assignments as text for exec) */
static PyObject *ShipMaker_IncludeMotor(PyObject *self, PyObject *args) {
	PyObject *e, *neng, *name, *n, *mass0, *style0, *engine0, *v[4];
	char var[0x40];
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOOOOO", &e, &neng, &name, &n, &mass0, &style0, &engine0)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	Sf_Import(&f, "Fx");
	int none = Sf_EqS(&f, name, "");
	PyObject *mass = NULL, *style = NULL, *engine = NULL;
	if (none > 0) {
		mass = style = engine = Sf_Int(&f, 0);
	} else if (none == 0) {
		PyObject *key = Sf_Ask(&f, "string", "upper", "(O)", name);
		if (Sf_Unpack(&f, Sf_GetItem(&f, Sf_Global(&f, SM, "MotorValues"), key), 4, v)) {
			mass = v[0];
			style = v[1];
			engine = v[2];
			PyObject *num = Sf_Repr(&f, neng);
			const char *s = num && PyString_Check(num) ? PyString_AsString(num) : "";
			if (Sm_HasAnm(&f, v[3], "FLAPS_ANM") > 0) {
				Str_Printf(var, sizeof(var), "EngAnmF%s", s);
				Sf_SetAttrS(&f, e, var, "Flaps");
			}
			if (Sm_HasAnm(&f, v[3], "MOTOR_ANM") > 0) {
				Str_Printf(var, sizeof(var), "EngAnmM%s", s);
				Sf_SetAttrS(&f, e, var, "Motor");
			}
			Str_Printf(var, sizeof(var), "EngFxClass%s", s);
			Sf_SetAttr(&f, e, var, Sf_Ask(&f, "Fx", "GetMotorClass", "(O)", name));
		}
	}
	PyObject *rm = Sf_Op(&f, mass0, '+', Sf_Op(&f, mass, '*', n));
	PyObject *rs = Sf_Op(&f, style0, '+', Sf_Op(&f, style, '*', n));
	PyObject *re = Sf_Op(&f, engine0, '+', Sf_Op(&f, engine, '*', n));
	return Sf_Return(&f, Sf_Build(&f, "(OOO)", rm, rs, re));
}

/* AddWeapon(name, mass, cost) */
static PyObject *ShipMaker_AddWeapon(PyObject *self, PyObject *args) {
	PyObject *name, *mass, *cost;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &mass, &cost)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	Sf_SetItem(&f, Sf_Global(&f, SM, "WeaponValues"), Sf_Ask(&f, "string", "upper", "(O)", name), mass);
	Sf_SetItem(&f, Sf_Global(&f, SM, "WeaponCosts"), Sf_Ask(&f, "string", "upper", "(O)", name), cost);
	return Sf_None(&f);
}

/* GetWeapon(name): the mass of the weapon, 0 when it is not in the table */
static PyObject *ShipMaker_GetWeapon(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *key = Sm_Upper(&f, name);
	PyObject *table = Sf_Global(&f, SM, "WeaponValues");
	if (Sf_True(&f, Sf_Method(&f, table, "has_key", "(O)", key)) > 0) {
		return Sf_Return(&f, Sf_GetItem(&f, table, key));
	}
	return Sf_Return(&f, Sf_Int(&f, 0));
}

/* CalculateVerticalBalance(e): the engine power of each group weighted by the balance of the hull */
static PyObject *ShipMaker_CalculateVerticalBalance(PyObject *self, PyObject *args) {
	static const char *const eng[] = { "MEng0", "MEng1", "MEng2", "MEng3" };
	static const char *const num[] = { "NEng0", "NEng1", "NEng2", "NEng3" };
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	PyObject *dist = Sf_Float(&f, 0.0);
	PyObject *total = Sf_Float(&f, 0.0);
	PyObject *balance = Sf_Own(&f, SM, "GetShipBalance", "(O)", Sf_Attr(&f, e, "Model"));
	for (int i = 0; i < 4 && !f.failed; i++) {
		PyObject *motor = Sf_Own(&f, SM, "GetMotor", "(O)", Sf_Attr(&f, e, eng[i]));
		PyObject *power = Sf_Op(&f, Sf_Item(&f, motor, 2), '*', Sf_Attr(&f, e, num[i]));
		dist = Sf_Op(&f, dist, '+', Sf_Op(&f, power, '*', Sf_Item(&f, balance, i)));
		total = Sf_Op(&f, total, '+', power);
	}
	int zero = Sf_Compare(&f, total, "==", Sf_Float(&f, 0.0));
	if (zero > 0) {
		Sf_SetAttr(&f, e, "VerticalBalance", Sf_Float(&f, 0.0));
	} else if (zero == 0) {
		Sf_SetAttr(&f, e, "VerticalBalance", Sf_Op(&f, dist, '/', total));
	}
	return Sf_None(&f);
}

/* SetUpChars(EntityName): the animations of the pilot (the module's Chars table) */
static PyObject *ShipMaker_SetUpChars(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *v = Sf_Entity(&f, name);
	PyObject *chars = Sf_Global(&f, SM, "Chars");
	if (Sf_True(&f, Sf_Method(&f, chars, "has_key", "(O)", Sf_Attr(&f, v, "MPil0"))) > 0) {
		PyObject *list = Sf_GetItem(&f, chars, Sf_Attr(&f, v, "MPil0"));
		for (int i = 0; !f.failed; i++) {
			int mark = f.n;
			PyObject *anm = Sf_Next(&f, list, i);
			if (!anm) {
				break;
			}
			Sf_SetAttr(&f, v, "AnmP0", anm);
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* AddChar(NewChar, Values): a pilot with the animations Values (indexes of PILOTAMN), preloaded */
static PyObject *ShipMaker_AddChar(PyObject *self, PyObject *args) {
	PyObject *name, *values;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &values)) {
		return NULL;
	}
	Sf_SetItem(&f, Sf_Global(&f, SM, "Chars"), name, values);
	PyObject *path = Sf_Cat(&f, "sOs", "Models/Vehicles/Characters/", name, "/");
	PyObject *file = Sf_Cat(&f, "OOs", path, name, ".M3D");
	Sf_Ask(&f, "Scrap", "Preload3DObject", "(Oiii)", file, 1, 1, 1);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *inam = Sf_Next(&f, values, i);
		if (!inam) {
			break;
		}
		PyObject *anm = Sf_GetItem(&f, Sf_Global(&f, SM, "PILOTAMN"), inam);
		Sf_Ask(&f, "Scrap", "PreloadAnm", "(OO)", file, Sf_Cat(&f, "OOs", path, anm, ".M3D"));
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "Scrap", "Verbose", "(O)", Sf_Cat(&f, "sOs", "Character loaded : '", name, "'  Loaded.\n"));
	return Sf_None(&f);
}

/* float(o) */
static PyObject *Sm_Float(scriptFrame_t *f, PyObject *o) {
	return f->failed || !o ? NULL : Sf_Keep(f, PyNumber_Float(o));
}

/* GetBoostFactor(Model, NewMass): the boost time grows from BoostTime to TopBoostTime with the mass up to TopWeight */
static PyObject *ShipMaker_GetBoostFactor(PyObject *self, PyObject *args) {
	PyObject *model, *newMass, *s[5], *t[3];
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &model, &newMass)) {
		return NULL;
	}
	Sf_Unpack(&f, Sf_Own(&f, SM, "GetShip", "(O)", model), 5, s);
	Sf_Unpack(&f, Sf_Own(&f, SM, "GetShipTon", "(O)", model), 3, t);
	if (f.failed) {
		return Sf_None(&f);
	}
	PyObject *mass = s[0], *boost = s[3], *top = s[4], *topWeight = t[2];
	if (Sf_Compare(&f, topWeight, ">", mass) > 0) {
		PyObject *a = Sm_Float(&f, Sf_Op(&f, newMass, '-', mass));
		PyObject *b = Sm_Float(&f, Sf_Op(&f, top, '-', boost));
		PyObject *c = Sm_Float(&f, Sf_Op(&f, topWeight, '-', mass));
		return Sf_Return(&f, Sf_Op(&f, boost, '+', Sf_Op(&f, Sf_Op(&f, a, '*', b), '/', c)));
	}
	return Sf_Return(&f, boost);
}

/* SetShipParams(EntityName): mass, style, engine, armour, boost time, vertical balance and pilot of the ship; true
 * when the ship is not heavier than the top weight of its hull */
static PyObject *ShipMaker_SetShipParams(PyObject *self, PyObject *args) {
	static const char *const eng[] = { "MEng0", "MEng1", "MEng2", "MEng3" };
	static const char *const num[] = { "NEng0", "NEng1", "NEng2", "NEng3" };
	static const char *const slot[] = { "A", "B", "C", "D" };
	PyObject *name, *s[5], *t[3], *m[3];
	long count = 0;
	char var[0x40];
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_Import(&f, "Fx");
	PyObject *e = Sf_Entity(&f, name);
	PyObject *model = Sf_Attr(&f, e, "Model");
	Sf_Unpack(&f, Sf_Own(&f, SM, "GetShip", "(O)", model), 5, s);
	Sf_Unpack(&f, Sf_Own(&f, SM, "GetShipTon", "(O)", model), 3, t);
	if (f.failed) {
		return Sf_None(&f);
	}
	PyObject *mass = s[0], *style = s[1], *engine = s[2], *minLife = t[0], *maxLife = t[1], *topWeight = t[2];
	for (int i = 0; i < 4 && !f.failed; i++) {
		PyObject *r = Sf_Own(&f, SM, "IncludeMotor", "(OiOOOOO)", e, i, Sf_Attr(&f, e, eng[i]), Sf_Attr(&f, e, num[i]),
			mass, style, engine);
		if (Sf_Unpack(&f, r, 3, m)) {
			mass = m[0];
			style = m[1];
			engine = m[2];
		}
	}
	Sf_Ask(&f, "Fx", "SetUpShipMotors", "(O)", e);
	PyObject *nweap = Sf_Ask(&f, "SWeap", "GetNWeap", "()");
	Sf_Check(&f, f.failed || Script_Range(nweap, &count));
	for (long iw = 0; iw < count && !f.failed; iw++) {
		/* the mass is the value a turn carries to the next one */
		int mark = f.n;
		PyObject *wMass = Sf_Own(&f, SM, "GetWeapon", "(O)", Sf_Ask(&f, "SWeap", "GetSWeap", "(ls)", iw, "Name"));
		if (Sf_Compare(&f, wMass, ">", Sf_Int(&f, 0)) > 0) {
			for (int k = 0; k < 4 && !f.failed; k++) {
				Str_Printf(var, sizeof(var), "Weap%s0%ld", slot[k], iw);
				if (Sf_EqI(&f, Sf_Attr(&f, e, var), 1) > 0) {
					mass = Sf_Op(&f, mass, '+', wMass);
				}
			}
		}
		Sf_ReleaseKeep(&f, mark, &mass, 1);
	}
	Sf_SetAttr(&f, e, "Mass", mass);
	Sf_SetAttr(&f, e, "Style", style);
	Sf_SetAttr(&f, e, "Engine", engine);
	Sf_SetAttr(&f, e, "MinLife", minLife);
	if (Sf_Compare(&f, maxLife, "<", Sf_Attr(&f, e, "MaxLife")) > 0) {
		Sf_SetAttr(&f, e, "MaxLife", maxLife);
	}
	if (Sf_Compare(&f, Sf_Attr(&f, e, "MaxLife"), "<", Sf_Attr(&f, e, "MinLife")) > 0) {
		Sf_SetAttr(&f, e, "MaxLife", Sf_Attr(&f, e, "MinLife"));
	}
	PyObject *armour = Sf_Op(&f, Sf_Attr(&f, e, "MaxLife"), '-', Sf_Attr(&f, e, "MinLife"));
	mass = Sf_Op(&f, mass, '+', Sf_Op(&f, armour, '*', Sf_Ask(&f, "Scrap", "Get", "(s)", "AmorPieceWeight")));
	Sf_SetAttr(&f, e, "BoostTime", Sf_Own(&f, SM, "GetBoostFactor", "(OO)", model, mass));
	Sf_Own(&f, SM, "CalculateVerticalBalance", "(O)", e);
	Sf_Own(&f, SM, "SetUpChars", "(O)", name);
	int ok = Sf_Compare(&f, Sf_ToInt(&f, topWeight), ">=", Sf_ToInt(&f, mass));
	return Sf_Return(&f, ok >= 0 ? Sf_Int(&f, ok) : NULL);
}

/* the step "stdship <key> <model> <eng0> <eng1> <eng2> <eng3> <weapons> <life>": StdShips[key] = the other values */
static void ShipMaker_StepStdShip(int id, PyObject *args) {
	(void)id;
	PyObject *table = Script_Global(SM, "StdShips");
	PyObject *key = PyTuple_Size(args) == 8 ? PyTuple_GetItem(args, 0) : NULL;
	PyObject *v = key ? PyTuple_GetSlice(args, 1, 8) : NULL;
	if (table && !key) {
		PyErr_SetString(PyExc_TypeError, "stdship takes 8 values");
	}
	if (table && v) {
		PyObject_SetItem(table, key, v);
	}
	Py_XDECREF(v);
	Py_XDECREF(table);
}

static const scriptStep_t shipmaker_steps[] = {
	{ "stdship", ShipMaker_StepStdShip },
	{ NULL, NULL },
};

/* CreateStdShips(): the standard ships, section StdShips of the file */
static PyObject *ShipMaker_CreateStdShips(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Script_RunFileTry(0, SHIPMAKER_FILE, "StdShips", shipmaker_def, shipmaker_steps, NULL) ? Py_ReturnNone() : NULL;
}

/* the isBoss of Init: the boss ship is needed in this level (the conditions in the order of the script, which stops
 * at the first true one) */
static int Sm_IsBoss(scriptFrame_t *f) {
	int t;
	if ((t = Sf_True(f, Sf_Mod(f, "Init", "isShipEdit"))) != 0 || (t = Sf_True(f, Sf_Mod(f, "Init", "MultiPlayer"))) != 0) {
		return t;
	}
	if ((t = Sf_In(f, Sf_Str(f, "SBOSS1"), Sf_Mod(f, "Parking", "AllShips"))) != 0 ||
		(t = Sf_In(f, Sf_Str(f, "SBoss1"), Sf_Mod(f, "Traffic", "AcShips"))) != 0) {
		return t;
	}
	PyObject *map = Sf_Slice(f, Sf_Mod(f, "Init", "Path"), 7, 0x7fffffff);
	if ((t = Sf_EqS(f, Sf_Ask(f, "string", "upper", "(O)", map), "AB")) != 0) {
		return t;
	}
	PyObject *ships = Sf_Eval(f, Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", "Hangar.shipsToEditList"), SM);
	if ((t = Sf_In(f, Sf_Str(f, "SBoss1"), ships)) != 0) {
		return t;
	}
	PyObject *engines = Sf_Eval(f, Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", "Hangar.availableEnginesList"), SM);
	return Sf_In(f, Sf_Str(f, "MBOSS1"), engines);
}

/* Init(): the hulls, engines and weapons (section Init of the file), the pilots of this game and the standard ships,
 * then VehiclesNPC.Init */
static PyObject *ShipMaker_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Import(&f, "Traffic");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "string");
	int boss = Sm_IsBoss(&f);
	int demo = Sf_True(&f, Sf_Mod(&f, "Init", "isDemo"));
	int online = Sf_True(&f, Sf_Mod(&f, "Init", "MultiPlayer"));
	if (online == 0) {
		online = Sf_True(&f, Sf_Mod(&f, "Init", "isShipEdit"));
	}
	if (f.failed) {
		return Sf_None(&f);
	}
	/* demo and online are read where the script reads them; Init does not change them on the way */
	PyObject *vars = Sf_Build(&f, "{s:i,s:i,s:i}", "full", !demo, "boss", !demo && boss, "online", online);
	Sf_Check(&f, vars && Script_RunFileTry(0, SHIPMAKER_FILE, "Init", shipmaker_def, NULL, vars));
	/* the pilots */
	int edit = Sf_True(&f, Sf_Mod(&f, "Init", "isShipEdit"));
	if (edit == 0) {
		edit = Sf_True(&f, Sf_Mod(&f, "Init", "MultiPlayer"));
	}
	if (edit == 0) {
		PyObject *pilots = Sf_Global(&f, SM, "PilotsList");
		if (Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "GameAct"), "Epilogue") > 0) {
			Sf_Method(&f, pilots, "remove", "(s)", "P_PoliceBoss");
			Sf_Method(&f, pilots, "remove", "(s)", "P_Mayor");
		} else {
			Sf_Method(&f, pilots, "remove", "(s)", "P_Boss");
		}
	}
	PyObject *pilots = Sf_Global(&f, SM, "PilotsList");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *pilot = Sf_Next(&f, pilots, i);
		if (!pilot) {
			break;
		}
		int all = Sf_True(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "IsXbox"));
		all = all < 0 ? -1 : !all;
		if (all == 0) {
			all = Sf_EqI(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "SplitScreen"), 1);
		}
		if (all == 0) {
			all = Sf_EqS(&f, pilot, "P_Dtritus");
		}
		if (all > 0) {
			Sf_Own(&f, SM, "AddChar", "(O(iiiii))", pilot, 0, 1, 2, 3, 4);
		} else if (all == 0) {
			Sf_Own(&f, SM, "AddChar", "(O())", pilot);
		}
		Sf_Release(&f, mark);
	}
	Sf_Own(&f, SM, "CreateStdShips", "()");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Ask(&f, "VehiclesNPC", "Init", "()");
	return Sf_None(&f);
}

/* SetAnmShip(v, model): the class of the animated ship and its procedural actions (ShipCallback[model]) */
static PyObject *ShipMaker_SetAnmShip(PyObject *self, PyObject *args) {
	PyObject *v, *model;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &v, &model)) {
		return NULL;
	}
	Sf_SetAttr(&f, v, "ActCtrl", model);
	PyObject *sc = Sf_GetItem(&f, Sf_Global(&f, SM, "ShipCallback"), model);
	for (int i = 1; i <= 4 && !f.failed; i++) {
		PyObject *act = Sf_Item(&f, sc, i);
		if (Sf_True(&f, act) > 0) {
			Sf_SetAttr(&f, v, "Action", act);
		}
	}
	return Sf_None(&f);
}

/* InitAnmShip(model, ch0=None, ch1=None, ch2=None, ch3=None): a static ship model preloaded, or with actions on the
 * channels an animated class of the model (the traffic models start with T and live in Traffic.Path) */
static PyObject *ShipMaker_InitAnmShip(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "model", "ch0", "ch1", "ch2", "ch3", NULL };
	PyObject *model, *ch[4] = { Py_None, Py_None, Py_None, Py_None };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|OOOO", names, &model, &ch[0], &ch[1], &ch[2], &ch[3])) {
		return NULL;
	}
	PyObject *path = NULL;
	int traffic = Sf_EqS(&f, Sf_Item(&f, model, 0), "T");
	if (traffic > 0) {
		Sf_Import(&f, "Traffic");
		path = Sf_Cat(&f, "OOs", Sf_Mod(&f, "Traffic", "Path"), model, ".M3D");
	} else if (traffic == 0) {
		path = Sf_Cat(&f, "sOsOs", "Models/Vehicles/Ships/", model, "/", model, ".M3D");
	}
	int none = 1;
	for (int i = 0; i < 4 && none > 0; i++) {
		none = Sf_Compare(&f, ch[i], "==", Py_None);
	}
	if (none > 0) {
		Sf_Ask(&f, "Scrap", "Preload3DObject", "(Oiii)", path, 1, 1, 1);
		Sf_Ask(&f, "Scrap", "Verbose", "(O)", Sf_Cat(&f, "sOs", "Static Vehicle: '", model, "'  Loaded.\n"));
		return Sf_None(&f);
	}
	Sf_SetItem(&f, Sf_Global(&f, SM, "ShipCallback"), model,
		Sf_Build(&f, "(OOOOO)", Sf_Global(&f, SM, "SetAnmShip"), ch[0], ch[1], ch[2], ch[3]));
	Sf_Ask(&f, "SAct", "CreateClass", "(O)", model);
	Sf_Ask(&f, "SAct", "SetCls", "(sO)", "ModelFilePath", path);
	Sf_Ask(&f, "SAct", "SetCls", "(si)", "DinaShip", 1);
	for (int i = 0; i < 4 && !f.failed; i++) {
		if (Sf_True(&f, ch[i]) > 0) {
			Sf_Ask(&f, "SAct", "CreateAction", "(O)", ch[i]);
			Sf_Ask(&f, "SAct", "SetAct", "(sO)", "AnmFilePath",
				Sf_Cat(&f, "sOsOs", "Models/Vehicles/Ships/", model, "/Anm/", ch[i], ".M3D"));
			Sf_Ask(&f, "SAct", "SetAct", "(si)", "Chanel", i);
		}
	}
	Sf_Ask(&f, "SAct", "SetCls", "(sO)", "DefAction", ch[0]);
	Sf_Ask(&f, "Scrap", "Verbose", "(O)", Sf_Cat(&f, "sOs", "Animated Vehicle: '", model, "'  Loaded.\n"));
	return Sf_None(&f);
}

PyMethodDef g_ShipMakerMethods[] = {
	{ "SetShipPrice", ShipMaker_SetShipPrice, 1, NULL },
	{ "GetShipPrice", ShipMaker_GetShipPrice, 1, NULL },
	{ "SetEnginePrice", ShipMaker_SetEnginePrice, 1, NULL },
	{ "GetEnginePrice", ShipMaker_GetEnginePrice, 1, NULL },
	{ "AddShipTon", ShipMaker_AddShipTon, 1, NULL },
	{ "GetShipTon", ShipMaker_GetShipTon, 1, NULL },
	{ "AddShipBalance", ShipMaker_AddShipBalance, 1, NULL },
	{ "GetShipBalance", ShipMaker_GetShipBalance, 1, NULL },
	{ "AddShip", (PyCFunction)(void (*)(void))ShipMaker_AddShip, 3, NULL },
	{ "GetShip", ShipMaker_GetShip, 1, NULL },
	{ "AddMotor", ShipMaker_AddMotor, 1, NULL },
	{ "GetMotor", ShipMaker_GetMotor, 1, NULL },
	{ "IncludeMotor", ShipMaker_IncludeMotor, 1, NULL },
	{ "AddWeapon", ShipMaker_AddWeapon, 1, NULL },
	{ "GetWeapon", ShipMaker_GetWeapon, 1, NULL },
	{ "CalculateVerticalBalance", ShipMaker_CalculateVerticalBalance, 1, NULL },
	{ "SetUpChars", ShipMaker_SetUpChars, 1, NULL },
	{ "AddChar", ShipMaker_AddChar, 1, NULL },
	{ "GetBoostFactor", ShipMaker_GetBoostFactor, 1, NULL },
	{ "SetShipParams", ShipMaker_SetShipParams, 1, NULL },
	{ "CreateStdShips", ShipMaker_CreateStdShips, 1, NULL },
	{ "Init", ShipMaker_Init, 1, NULL },
	{ "SetAnmShip", ShipMaker_SetAnmShip, 1, NULL },
	{ "InitAnmShip", (PyCFunction)(void (*)(void))ShipMaker_InitAnmShip, 3, NULL },
	{ NULL, NULL, 0, NULL },
};
