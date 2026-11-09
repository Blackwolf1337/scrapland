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
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters. The
 * tables are read where the script reads them as its globals and changed in place on the module's objects (OBJLIB 5:
 * the .mod file, the editor tabs, Vehicles and the cases read them). The module's own functions are called through
 * its globals as the script calls them (PORT_OWN: directly while the global holds the port's function, else by name,
 * so a mission's replacement such as AB's FakeShips for CreateStdShips, or a case's, still gets the call), Fx and
 * VehiclesNPC through their refs (PORT_CALL); the ships (handle values) and the functions ShipCallback holds stay
 * Python values (OBJLIB 7). Scrap.GetEntity, Preload3DObject and the SAct calls go through their seams (fc-om-dt,
 * fc-r7b-fb and the scene cases swap or record them, OBJLIB 7); Scrap.PreloadAnm, Verbose, SWeap.GetNWeap and GetSWeap
 * go through their refs (stage 7). The .def runner takes its $names as a dict and its step stdship the values of its line
 * (OBJLIB 6).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

static const char shipmaker_def[] = {
#embed "../../../data/vehicles/shipmaker.def"
	, 0 };

#define SHIPMAKER_FILE "vehicles/shipmaker.def"
#define SM "ShipMaker"

/* string.upper(name) as the key of a table (the script imports string first) */
static byte Sm_Key(const nativeVal_t *name, nativeVal_t *key) {
	return Mode_Import("string") && Native_Upper(name, key);
}

/* global table; table[string.upper(name)] = v */
static byte Sm_Put(const char *table, const nativeVal_t *name, const nativeVal_t *v) {
	nativeVal_t key = { 0 };
	byte ok = Sm_Key(name, &key) && Mode_DictPut(SM, table, NULL, &key, v);
	Native_Free(&key);
	return ok;
}

/* table[key] of the global table */
static byte Sm_At(const char *table, const nativeVal_t *key, nativeVal_t *out) {
	nativeVal_t t;
	byte ok = Mode_G(SM, table, &t) && Mode_Sub(&t, key, out);
	Native_Free(&t);
	return ok;
}

/* return table[string.upper(name)] */
static byte Sm_Get(const char *table, const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t key = { 0 };
	byte ok = Sm_Key(name, &key) && Sm_At(table, &key, out);
	Native_Free(&key);
	return ok;
}

/* the tuple of the n values */
static void Sm_Tuple(nativeVal_t *out, const nativeVal_t *const *v, int n) {
	Native_SetEmpty(out, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(out), v[i]);
	}
}

/* SetShipPrice(name, Price): shipsCosts holds the repr of the price */
static byte Sm_SetShipPriceN(const nativeVal_t *name, const nativeVal_t *price) {
	nativeVal_t key = { 0 }, rep;
	byte ok = Sm_Key(name, &key);
	if (ok) {
		Mode_Repr(price, &rep);
		ok = Mode_DictPut(SM, "shipsCosts", NULL, &key, &rep);
		Native_Free(&rep);
	}
	Native_Free(&key);
	return ok;
}

/* AddShipTon(name, StandardArmor, TopArmor, TopMass) */
static byte Sm_AddShipTonN(const nativeVal_t *a) {
	nativeVal_t v;
	Sm_Tuple(&v, (const nativeVal_t *const[]){ &a[1], &a[2], &a[3] }, 3);
	byte ok = Sm_Put("ShipTon", &a[0], &v);
	Native_Free(&v);
	return ok;
}

/* AddShip(name, Mass, Style, Engine, BoostTime=10, TopBoostTime=10) */
/* AddShip(name, Mass, Style, Engine, BoostTime=10, TopBoostTime=10): ShipValues[name] */
static byte Sm_AddShipK(const nativeVal_t *const *a, nativeVal_t *out) {
	nativeVal_t ten, v;
	(void)out;
	Native_SetInt(&ten, 10);
	Sm_Tuple(&v, (const nativeVal_t *const[]){ a[1], a[2], a[3], a[4] ? a[4] : &ten, a[5] ? a[5] : &ten }, 5);
	byte ok = Sm_Put("ShipValues", a[0], &v);
	Native_Free(&v);
	return ok;
}

static byte ShipMaker_AddShip_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "OOOO|OO", Sm_AddShipK, out);
}

static char *ShipMaker_AddShip_N[] = { "name", "Mass", "Style", "Engine", "BoostTime", "TopBoostTime", NULL };

static PyObject *ShipMaker_AddShip(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Mode_KwCall(args, kw, "OOOO|OO", ShipMaker_AddShip_N, Sm_AddShipK);
}

/* (anm & bits) of the script with bits the global flag (ints in C; OBJLIB 9: other numbers the Python way): 1 when it
 * is not 0, 0, -1 with the error */
static int Sm_HasAnm(const nativeVal_t *anm, const char *flag) {
	nativeVal_t bits, and;
	int r = -1;
	Native_SetNone(&and);
	if (!Mode_G(SM, flag, &bits)) {
		return -1;
	}
	if (anm->type == NV_INT && bits.type == NV_INT) {
		Native_SetInt(&and, anm->i & bits.i);
		r = Mode_CmpI(&and, "!=", 0);
	} else {
		PyObject *x = Native_ToPy(anm), *y = x ? Native_ToPy(&bits) : NULL, *z = y ? PyNumber_And(x, y) : NULL;
		if (z) {
			Native_Arg(z, &and);
			r = Mode_CmpI(&and, "!=", 0);
		}
		Py_XDECREF(z);
		Py_XDECREF(y);
		Py_XDECREF(x);
	}
	Native_Free(&and);
	Native_Free(&bits);
	return r;
}

/* Scrap.PreloadAnm(file, path + name + rest) (name NULL: none) and Scrap.Verbose(text) */
static byte Sm_PreloadAnm(modeVals_t *t, const nativeVal_t *file, const nativeVal_t *path, const nativeVal_t *name, const char *rest) {
	nativeVal_t *anm = Mode_V(t);
	byte ok = name ? Mode_Cat(t, anm, 3, path, name, Mode_S(t, rest)) : Mode_Cat(t, anm, 2, path, Mode_S(t, rest));
	return ok && PORT_CALL(Scrap, PreloadAnm, NULL, "vv", file, anm);
}

static byte Sm_Verbose(modeVals_t *t, const char *head, const nativeVal_t *name, const char *tail) {
	nativeVal_t *text = Mode_V(t);
	return Mode_Cat(t, text, 3, Mode_S(t, head), name, Mode_S(t, tail)) && PORT_CALL(Scrap, Verbose, NULL, "v", text);
}

/* path = dir + name + "/"; file = path + name + ".M3D", preloaded */
static byte Sm_Model(modeVals_t *t, const char *dir, const nativeVal_t *name, nativeVal_t *path, nativeVal_t *file) {
	return Mode_Cat(t, path, 3, Mode_S(t, dir), name, Mode_S(t, "/")) && Mode_Cat(t, file, 3, path, name, Mode_S(t, ".M3D")) &&
		Mode_PreloadOnes(file);
}

/* AddMotor(name, Mass, Style, Engine, Anm): the values and the preloaded model with its animations */
static byte Sm_AddMotorN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *path = Mode_V(&t), *file = Mode_V(&t);
	Sm_Tuple(v, (const nativeVal_t *const[]){ &a[1], &a[2], &a[3], &a[4] }, 4);
	if (!Sm_Put("MotorValues", &a[0], v) || !Sm_Model(&t, "Models/Vehicles/Engines/", &a[0], path, file)) {
		return Mode_Done(&t, 0);
	}
	int flaps = Sm_HasAnm(&a[4], "FLAPS_ANM");
	if (flaps < 0 || (flaps && !Sm_PreloadAnm(&t, file, path, NULL, "Flaps.M3D"))) {
		return Mode_Done(&t, 0);
	}
	int motor = Sm_HasAnm(&a[4], "MOTOR_ANM");
	if (motor < 0 || (motor && !Sm_PreloadAnm(&t, file, path, NULL, "Motor.M3D"))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Sm_Verbose(&t, "Engine : '", &a[0], "'  Loaded.\n"));
}

/* GetMotor(name): (0, 0, 0, 0) for no engine */
static byte Sm_GetMotorN(const nativeVal_t *name, nativeVal_t *out) {
	if (LvN_EqS(name, "")) {
		Native_SetEmpty(out, NV_TUPLE);
		for (int i = 0; i < 4; i++) {
			Native_SetInt(Native_Append(out), 0);
		}
		return 1;
	}
	return Sm_Get("MotorValues", name, out);
}

/* base + v * n of the script */
static byte Sm_AddTimes(const nativeVal_t *base, const nativeVal_t *v, const nativeVal_t *n, nativeVal_t *out) {
	nativeVal_t p;
	Native_SetNone(&p);
	byte ok = Mode_Arith('*', v, n, &p) && Native_Arith('+', base, &p, out);
	Native_Free(&p);
	return ok;
}

/* e.<prefix><num> = v */
static byte Sm_SetNum(const nativeVal_t *e, const char *prefix, const char *num, const nativeVal_t *v) {
	char var[0x40];
	Str_Printf(var, sizeof(var), "%s%s", prefix, num);
	return Mode_VSet(e, var, v);
}

/* IncludeMotor(e, neng, name, n, Mass, Style, Engine): (Mass, Style, Engine) with n engines name; sets the flaps and
 * motor animations and the effect class of engine neng (the script builds the assignments as text for exec) */
static byte Sm_IncludeMotorN(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *key = Mode_V(&t), *mv = Mode_V(&t), *v = Mode_V(&t), *num = Mode_V(&t), *cls = Mode_V(&t);
	const nativeVal_t *e = &a[0], *name = &a[2], *n = &a[3];
	const nativeVal_t *mass = Mode_I(&t, 0), *style = mass, *engine = mass;
	if (!Mode_Import("string") || !Mode_Import("Fx")) {
		return Mode_Done(&t, 0);
	}
	if (!LvN_EqS(name, "")) {
		v = Mode_V(&t);
		Mode_V(&t);
		Mode_V(&t);
		Mode_V(&t);
		if (!Native_Upper(name, key) || !Sm_At("MotorValues", key, mv) || !Mode_Unpack(mv, 4, v)) {
			return Mode_Done(&t, 0);
		}
		mass = &v[0];
		style = &v[1];
		engine = &v[2];
		Mode_Repr(&a[1], num);
		const char *s = Native_Text(num);
		int flaps = Sm_HasAnm(&v[3], "FLAPS_ANM");
		if (flaps < 0 || (flaps && !Sm_SetNum(e, "EngAnmF", s, Mode_S(&t, "Flaps")))) {
			return Mode_Done(&t, 0);
		}
		int motor = Sm_HasAnm(&v[3], "MOTOR_ANM");
		if (motor < 0 || (motor && !Sm_SetNum(e, "EngAnmM", s, Mode_S(&t, "Motor"))) ||
			!PORT_CALL(Fx, GetMotorClass, cls, "v", name) || !Sm_SetNum(e, "EngFxClass", s, cls)) {
			return Mode_Done(&t, 0);
		}
	}
	Native_SetEmpty(out, NV_TUPLE);
	byte ok = Sm_AddTimes(&a[4], mass, n, Native_Append(out)) && Sm_AddTimes(&a[5], style, n, Native_Append(out)) &&
		Sm_AddTimes(&a[6], engine, n, Native_Append(out));
	return Mode_Done(&t, ok);
}

/* AddWeapon(name, mass, cost) */
static byte Sm_AddWeaponN(const nativeVal_t *name, const nativeVal_t *mass, const nativeVal_t *cost) {
	return Sm_Put("WeaponValues", name, mass) && Sm_Put("WeaponCosts", name, cost);
}

/* GetWeapon(name): the mass of the weapon, 0 when it is not in the table */
static byte Sm_GetWeaponN(const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t key = { 0 }, t = { 0 };
	byte ok = Sm_Key(name, &key) && Mode_G(SM, "WeaponValues", &t);
	int has = ok ? Mode_HasKey(SM, "WeaponValues", 1, &key) : -1;
	if (has > 0) {
		ok = Mode_Sub(&t, &key, out);
	} else {
		ok = has == 0;
		Native_SetInt(out, 0);
	}
	Native_Free(&t);
	Native_Free(&key);
	return ok;
}

/* CalculateVerticalBalance(e): the engine power of each group weighted by the balance of the hull */
static byte Sm_CalculateVerticalBalanceN(const nativeVal_t *e) {
	static const char *const eng[] = { "MEng0", "MEng1", "MEng2", "MEng3" };
	static const char *const num[] = { "NEng0", "NEng1", "NEng2", "NEng3" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *dist = Mode_V(&t), *total = Mode_V(&t), *model = Mode_V(&t), *balance = Mode_V(&t);
	Native_SetFloat(dist, 0.0);
	Native_SetFloat(total, 0.0);
	if (!Mode_VGet(e, "Model", model) || !PORT_OWN(ShipMaker, GetShipBalance, balance, "v", model)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		int mark = t.n;
		nativeVal_t *m = Mode_V(&t), *motor = Mode_V(&t), *p2 = Mode_V(&t), *n = Mode_V(&t), *power = Mode_V(&t);
		nativeVal_t *b = Mode_V(&t), *pb = Mode_V(&t), *d = Mode_V(&t), *s = Mode_V(&t);
		if (!Mode_VGet(e, eng[i], m) || !PORT_OWN(ShipMaker, GetMotor, motor, "v", m) || !LvN_Item(motor, 2, p2) ||
			!Mode_VGet(e, num[i], n) || !Mode_Arith('*', p2, n, power) || !LvN_Item(balance, i, b) ||
			!Mode_Arith('*', power, b, pb) || !Native_Arith('+', dist, pb, d) || !Native_Arith('+', total, power, s)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(dist);
		Native_Free(total);
		*dist = *d;
		*total = *s;
		Native_SetNone(d);
		Native_SetNone(s);
		Mode_Release(&t, mark);
	}
	nativeVal_t *vb = Mode_V(&t);
	Native_SetFloat(vb, 0.0);
	if (Native_Compare(total, vb) && !Mode_Arith('/', dist, total, vb)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_VSet(e, "VerticalBalance", vb));
}

/* SetUpChars(EntityName): the animations of the pilot (the module's Chars table) */
static byte Sm_SetUpCharsN(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *chars = Mode_V(&t), *pil = Mode_V(&t), *list = Mode_V(&t), *anm = Mode_V(&t);
	if (!Mode_EntitySeam(name, v) || !Mode_G(SM, "Chars", chars) || !Mode_VGet(v, "MPil0", pil)) {
		return Mode_Done(&t, 0);
	}
	int has = Mode_HasKey(SM, "Chars", 1, pil);
	if (has <= 0) {
		return Mode_Done(&t, has == 0);
	}
	Native_Free(pil);
	if (!Mode_VGet(v, "MPil0", pil) || !Mode_Sub(chars, pil, list)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Loop(list);
	for (int i = 0; i < n; i++) {
		Native_Free(anm);
		if (!Native_SeqItem(list, i, anm) || !Mode_VSet(v, "AnmP0", anm)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0);
}

/* AddChar(NewChar, Values): a pilot with the animations Values (indexes of PILOTAMN), preloaded */
static byte Sm_AddCharN(const nativeVal_t *name, const nativeVal_t *values) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *path = Mode_V(&t), *file = Mode_V(&t), *list = Mode_V(&t), *inam = Mode_V(&t), *anm = Mode_V(&t);
	if (!Mode_DictPut(SM, "Chars", NULL, name, values) || !Sm_Model(&t, "Models/Vehicles/Characters/", name, path, file)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(list, values);
	int n = Mode_Loop(list);
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		Native_Free(inam);
		Native_Free(anm);
		if (!Native_SeqItem(list, i, inam) || !Sm_At("PILOTAMN", inam, anm) || !Sm_PreloadAnm(&t, file, path, anm, ".M3D")) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, n >= 0 && Sm_Verbose(&t, "Character loaded : '", name, "'  Loaded.\n"));
}

/* float(v) */
static byte Sm_Float(const nativeVal_t *a, char op, const nativeVal_t *b, nativeVal_t *out) {
	nativeVal_t d;
	Native_SetNone(&d);
	byte ok = Native_Arith(op, a, b, &d) && Native_ToFloat(&d, out);
	Native_Free(&d);
	return ok;
}

/* GetShip(model) and GetShipTon(model) unpacked into s (5) and t (3) through the module's globals */
static byte Sm_ShipAndTon(const nativeVal_t *model, modeVals_t *t, nativeVal_t **s, nativeVal_t **ton) {
	nativeVal_t *a = Mode_V(t), *b = Mode_V(t);
	*s = Mode_V(t);
	for (int i = 1; i < 5; i++) {
		Mode_V(t);
	}
	*ton = Mode_V(t);
	Mode_V(t);
	Mode_V(t);
	return PORT_OWN(ShipMaker, GetShip, a, "v", model) && Mode_Unpack(a, 5, *s) &&
		PORT_OWN(ShipMaker, GetShipTon, b, "v", model) && Mode_Unpack(b, 3, *ton);
}

/* GetBoostFactor(Model, NewMass): the boost time grows from BoostTime to TopBoostTime with the mass up to TopWeight */
static byte Sm_GetBoostFactorN(const nativeVal_t *model, const nativeVal_t *newMass, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s, *ton;
	if (!Sm_ShipAndTon(model, &t, &s, &ton)) {
		return Mode_Done(&t, 0);
	}
	const nativeVal_t *mass = &s[0], *boost = &s[3], *top = &s[4], *topWeight = &ton[2];
	int up = Mode_Cmp(topWeight, ">", mass);
	if (up <= 0) {
		if (up == 0) {
			Native_Copy(out, boost);
		}
		return Mode_Done(&t, up == 0);
	}
	nativeVal_t *a = Mode_V(&t), *b = Mode_V(&t), *c = Mode_V(&t), *ab = Mode_V(&t), *q = Mode_V(&t);
	return Mode_Done(&t, Sm_Float(newMass, '-', mass, a) && Sm_Float(top, '-', boost, b) && Sm_Float(topWeight, '-', mass, c) &&
		Native_Arith('*', a, b, ab) && Native_Arith('/', ab, c, q) && Native_Arith('+', boost, q, out));
}

/* SetShipParams(EntityName): mass, style, engine, armour, boost time, vertical balance and pilot of the ship; true
 * when the ship is not heavier than the top weight of its hull */
static byte Sm_SetShipParamsN(const nativeVal_t *name, nativeVal_t *out) {
	static const char *const eng[] = { "MEng0", "MEng1", "MEng2", "MEng3" };
	static const char *const num[] = { "NEng0", "NEng1", "NEng2", "NEng3" };
	static const char *const slot[] = { "A", "B", "C", "D" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *model = Mode_V(&t), *s, *ton, *nweap = Mode_V(&t);
	nativeVal_t *mass = Mode_V(&t), *style = Mode_V(&t), *engine = Mode_V(&t);
	char var[0x40];
	long count;
	if (!Mode_Import("Fx") || !Mode_EntitySeam(name, e) || !Mode_VGet(e, "Model", model) || !Sm_ShipAndTon(model, &t, &s, &ton)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(mass, &s[0]);
	Native_Copy(style, &s[1]);
	Native_Copy(engine, &s[2]);
	const nativeVal_t *minLife = &ton[0], *maxLife = &ton[1], *topWeight = &ton[2];
	for (int i = 0; i < 4; i++) {
		int mark = t.n;
		nativeVal_t *m = Mode_V(&t), *n = Mode_V(&t), *r = Mode_V(&t), *mse = Mode_V(&t);
		Mode_V(&t);
		Mode_V(&t);
		if (!Mode_VGet(e, eng[i], m) || !Mode_VGet(e, num[i], n) ||
			!PORT_OWN(ShipMaker, IncludeMotor, r, "vivvvvv", e, (long)i, m, n, mass, style, engine) ||
			!Mode_Unpack(r, 3, mse)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(mass);
		Native_Free(style);
		Native_Free(engine);
		*mass = mse[0];
		*style = mse[1];
		*engine = mse[2];
		Native_SetNone(&mse[0]);
		Native_SetNone(&mse[1]);
		Native_SetNone(&mse[2]);
		Mode_Release(&t, mark);
	}
	if (!PORT_CALL(Fx, SetUpShipMotors, NULL, "v", e) || !PORT_CALL(SWeap, GetNWeap, nweap, "") ||
		!Mode_Range(nweap, &count)) {
		return Mode_Done(&t, 0);
	}
	for (long iw = 0; iw < count; iw++) {
		int mark = t.n;
		nativeVal_t *wn = Mode_V(&t), *wMass = Mode_V(&t);
		if (!PORT_CALL(SWeap, GetSWeap, wn, "is", iw, "Name") ||
			!PORT_OWN(ShipMaker, GetWeapon, wMass, "v", wn)) {
			return Mode_Done(&t, 0);
		}
		int heavy = Mode_CmpI(wMass, ">", 0);
		if (heavy < 0) {
			return Mode_Done(&t, 0);
		}
		for (int k = 0; heavy && k < 4; k++) {
			nativeVal_t *w = Mode_V(&t), *sum = Mode_V(&t);
			Str_Printf(var, sizeof(var), "Weap%s0%ld", slot[k], iw);
			if (!Mode_VGet(e, var, w)) {
				return Mode_Done(&t, 0);
			}
			int on = Mode_CmpI(w, "==", 1);
			if (on < 0 || (on && !Native_Arith('+', mass, wMass, sum))) {
				return Mode_Done(&t, 0);
			}
			if (on) {
				Native_Free(mass);
				*mass = *sum;
				Native_SetNone(sum);
			}
		}
		Mode_Release(&t, mark);
	}
	nativeVal_t *cur = Mode_V(&t), *cur2 = Mode_V(&t), *armour = Mode_V(&t), *apw = Mode_V(&t), *aw = Mode_V(&t);
	nativeVal_t *total = Mode_V(&t), *boost = Mode_V(&t), *ti = Mode_V(&t), *mi = Mode_V(&t);
	if (!Mode_VSet(e, "Mass", mass) || !Mode_VSet(e, "Style", style) || !Mode_VSet(e, "Engine", engine) ||
		!Mode_VSet(e, "MinLife", minLife) || !Mode_VGet(e, "MaxLife", cur)) {
		return Mode_Done(&t, 0);
	}
	int less = Mode_Cmp(maxLife, "<", cur);
	if (less < 0 || (less && !Mode_VSet(e, "MaxLife", maxLife))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(cur);
	if (!Mode_VGet(e, "MaxLife", cur) || !Mode_VGet(e, "MinLife", cur2)) {
		return Mode_Done(&t, 0);
	}
	less = Mode_Cmp(cur, "<", cur2);
	if (less < 0) {
		return Mode_Done(&t, 0);
	}
	if (less) {
		Native_Free(cur2);
		if (!Mode_VGet(e, "MinLife", cur2) || !Mode_VSet(e, "MaxLife", cur2)) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(cur);
	Native_Free(cur2);
	if (!Mode_VGet(e, "MaxLife", cur) || !Mode_VGet(e, "MinLife", cur2) || !Native_Arith('-', cur, cur2, armour) ||
		!Native_GVarGet("AmorPieceWeight", apw) || !Mode_Arith('*', armour, apw, aw) || !Native_Arith('+', mass, aw, total) ||
		!PORT_OWN(ShipMaker, GetBoostFactor, boost, "vv", model, total) || !Mode_VSet(e, "BoostTime", boost) ||
		!PORT_OWN(ShipMaker, CalculateVerticalBalance, NULL, "v", e) ||
		!PORT_OWN(ShipMaker, SetUpChars, NULL, "v", name) ||
		!Native_ToInt(topWeight, ti) || !Native_ToInt(total, mi)) {
		return Mode_Done(&t, 0);
	}
	int ok = Mode_Cmp(ti, ">=", mi);
	if (ok >= 0) {
		Native_SetInt(out, ok);
	}
	return Mode_Done(&t, ok >= 0);
}

/* the step "stdship <key> <model> <eng0> <eng1> <eng2> <eng3> <weapons> <life>": StdShips[key] = the other values */
static void ShipMaker_StepStdShip(int id, PyObject *args) {
	nativeVal_t a, table, v;
	(void)id;
	/* OBJLIB 6: the step takes the values of its line as a tuple */
	Native_Arg(args, &a);
	Native_SetNone(&v);
	if (Mode_G(SM, "StdShips", &table)) {
		if (a.type != NV_TUPLE || a.n != 8) {
			Native_Raise(NE_TYPE, "stdship takes 8 values");
		} else if (Native_Slice(&a, 1, 8, &v)) {
			Mode_DictPut(SM, "StdShips", NULL, &a.items[0], &v);
		}
	}
	Native_Free(&v);
	Native_Free(&table);
	Native_Free(&a);
}

static const scriptStep_t shipmaker_steps[] = {
	{ "stdship", ShipMaker_StepStdShip },
	{ NULL, NULL },
};

/* CreateStdShips(): the standard ships, section StdShips of the file */
static byte Sm_CreateStdShipsN(void) {
	return Script_RunFileTry(0, SHIPMAKER_FILE, "StdShips", shipmaker_def, shipmaker_steps, NULL);
}

/* text in module.var of another module: 1, 0, -1 with the error */
static int Sm_ModIn(const char *text, const char *module, const char *var) {
	nativeVal_t v, x;
	Native_SetStr(&x, text);
	int r = LvN_ModGet(module, var, &v) ? (Mode_Elems(&v), Native_Contains(&v, &x)) : -1;
	Native_Free(&v);
	return r;
}

/* text in eval(Scrap.GetSaveVar(save)) in the module: 1, 0, -1 with the error */
static int Sm_SaveIn(const char *text, const char *save) {
	nativeVal_t s, v, x;
	Native_SetStr(&x, text);
	Native_SetNone(&v);
	Mode_SaveVar(save, &s);
	int r = Native_EvalText(SM, &s, &v) ? (Mode_Elems(&v), Native_Contains(&v, &x)) : -1;
	Native_Free(&v);
	Native_Free(&s);
	return r;
}

/* the isBoss of Init: the boss ship is needed in this level (the conditions in the order of the script, which stops
 * at the first true one) */
static int Sm_IsBoss(void) {
	int r;
	if ((r = Mode_ModTrue("Init", "isShipEdit")) != 0 || (r = Mode_ModTrue("Init", "MultiPlayer")) != 0 ||
		(r = Sm_ModIn("SBOSS1", "Parking", "AllShips")) != 0 || (r = Sm_ModIn("SBoss1", "Traffic", "AcShips")) != 0) {
		return r;
	}
	nativeVal_t path, map, up;
	Native_SetNone(&map);
	Native_SetNone(&up);
	r = LvN_ModGet("Init", "Path", &path) && Native_Slice(&path, 7, 0x7fffffff, &map) && Native_Upper(&map, &up) ?
		LvN_EqS(&up, "AB") : -1;
	Native_Free(&path);
	Native_Free(&map);
	Native_Free(&up);
	if (r != 0 || (r = Sm_SaveIn("SBoss1", "Hangar.shipsToEditList")) != 0) {
		return r;
	}
	return Sm_SaveIn("MBOSS1", "Hangar.availableEnginesList");
}

/* Init(): the hulls, engines and weapons (section Init of the file), the pilots of this game and the standard ships,
 * then VehiclesNPC.Init */
static byte Sm_InitN(void) {
	if (!Mode_Import("Init") || !Mode_Import("Traffic") || !Mode_Import("Parking") || !Mode_Import("string")) {
		return 0;
	}
	int boss = Sm_IsBoss();
	int demo = boss < 0 ? -1 : Mode_ModTrue("Init", "isDemo");
	int online = demo < 0 ? -1 : Mode_ModTrue("Init", "MultiPlayer");
	if (online == 0) {
		online = Mode_ModTrue("Init", "isShipEdit");
	}
	if (online < 0) {
		return 0;
	}
	/* demo and online are read where the script reads them; Init does not change them on the way. OBJLIB 6: the .def
	 * runner takes its $names as a dict */
	PyObject *vars = Py_BuildValue("{s:i,s:i,s:i}", "full", !demo, "boss", !demo && boss, "online", online);
	byte ok = vars && Script_RunFileTry(0, SHIPMAKER_FILE, "Init", shipmaker_def, NULL, vars);
	Py_XDECREF(vars);
	if (!ok) {
		return 0;
	}
	/* the pilots */
	int edit = Mode_ModTrue("Init", "isShipEdit");
	if (edit == 0) {
		edit = Mode_ModTrue("Init", "MultiPlayer");
	}
	if (edit < 0) {
		return 0;
	}
	if (edit == 0) {
		nativeVal_t pilots, act, x;
		if (!Mode_G(SM, "PilotsList", &pilots)) {
			return 0;
		}
		Native_Free(&pilots);
		Mode_SaveVar("GameAct", &act);
		byte epilogue = LvN_EqS(&act, "Epilogue");
		Native_Free(&act);
		if (epilogue) {
			Native_SetStr(&x, "P_PoliceBoss");
			ok = Mode_Remove(SM, "PilotsList", &x);
			Native_SetStr(&x, "P_Mayor");
			ok = ok && Mode_Remove(SM, "PilotsList", &x);
		} else {
			Native_SetStr(&x, "P_Boss");
			ok = Mode_Remove(SM, "PilotsList", &x);
		}
		if (!ok) {
			return 0;
		}
	}
	modeVals_t t = { .n = 0 };
	nativeVal_t *pilots = Mode_V(&t), *pilot = Mode_V(&t), *g = Mode_V(&t), *all5 = Mode_V(&t), *none = Mode_V(&t);
	if (!Mode_G(SM, "PilotsList", pilots)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(all5, NV_TUPLE);
	for (long i = 0; i < 5; i++) {
		Native_SetInt(Native_Append(all5), i);
	}
	Native_SetEmpty(none, NV_TUPLE);
	int n = Mode_Loop(pilots);
	for (int i = 0; i < n; i++) {
		Native_Free(pilot);
		Native_Free(g);
		if (!Native_SeqItem(pilots, i, pilot) || !Native_GVarGet("IsXbox", g)) {
			return Mode_Done(&t, 0);
		}
		int all = !Native_True(g);
		if (!all) {
			Native_Free(g);
			if (!Native_GVarGet("SplitScreen", g)) {
				return Mode_Done(&t, 0);
			}
			all = LvN_EqI(g, 1) || LvN_EqS(pilot, "P_Dtritus");
		}
		if (!PORT_OWN(ShipMaker, AddChar, NULL, "vv", pilot, all ? all5 : none)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0 && PORT_OWN(ShipMaker, CreateStdShips, NULL, "") && Mode_Import("VehiclesNPC") &&
		PORT_CALL(VehiclesNPC, Init, NULL, ""));
}

/* SetAnmShip(v, model): the class of the animated ship and its procedural actions (ShipCallback[model]) */
static byte Sm_SetAnmShipN(const nativeVal_t *v, const nativeVal_t *model) {
	nativeVal_t sc, act;
	Native_SetNone(&sc);
	Native_SetNone(&act);
	byte ok = Mode_VSet(v, "ActCtrl", model) && Sm_At("ShipCallback", model, &sc);
	for (long i = 1; ok && i <= 4; i++) {
		Native_Free(&act);
		ok = LvN_Item(&sc, i, &act) && (!Native_True(&act) || Mode_VSet(v, "Action", &act));
	}
	Native_Free(&act);
	Native_Free(&sc);
	return ok;
}

/* SAct.CreateClass(name) or SAct.CreateAction(name) (fn) through the seam for a text, through the ref for another value
 * (its error) */
static byte Sm_SActName(const char *fn, const nativeVal_t *name) {
	if (name->type == NV_STR && (int)strlen(Native_Text(name)) == name->n) {
		return Mode_SAct(fn, Native_Text(name));
	}
	return !strcmp(fn, "CreateClass") ? PORT_CALL(SAct, CreateClass, NULL, "v", name) :
		PORT_CALL(SAct, CreateAction, NULL, "v", name);
}

/* InitAnmShip(model, ch0=None, ch1=None, ch2=None, ch3=None): a static ship model preloaded, or with actions on the
 * channels an animated class of the model (the traffic models start with T and live in Traffic.Path) */
static byte Sm_InitAnmShipN(const nativeVal_t *model, const nativeVal_t *ch) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *first = Mode_V(&t), *tpath = Mode_V(&t), *path = Mode_V(&t), *cb = Mode_V(&t), *fn = Mode_V(&t);
	nativeVal_t *anm = Mode_V(&t);
	if (!LvN_Item(model, 0, first)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqS(first, "T")) {
		if (!Mode_Import("Traffic") || !LvN_ModGet("Traffic", "Path", tpath) || !Mode_Cat(&t, path, 3, tpath, model, Mode_S(&t, ".M3D"))) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_Cat(&t, path, 5, Mode_S(&t, "Models/Vehicles/Ships/"), model, Mode_S(&t, "/"), model, Mode_S(&t, ".M3D"))) {
		return Mode_Done(&t, 0);
	}
	int none = 1;
	for (int i = 0; i < 4 && none; i++) {
		none = ch[i].type == NV_NONE;
	}
	if (none) {
		return Mode_Done(&t, Mode_PreloadOnes(path) && Sm_Verbose(&t, "Static Vehicle: '", model, "'  Loaded.\n"));
	}
	if (!LvN_GObj(SM, "SetAnmShip", fn)) {
		return Mode_Done(&t, 0);
	}
	Sm_Tuple(cb, (const nativeVal_t *const[]){ fn, &ch[0], &ch[1], &ch[2], &ch[3] }, 5);
	if (!Mode_DictPut(SM, "ShipCallback", NULL, model, cb) || !Sm_SActName("CreateClass", model) ||
		!LvN_SetCls("ModelFilePath", path) || !LvN_SetCls("DinaShip", Mode_I(&t, 1))) {
		return Mode_Done(&t, 0);
	}
	for (long i = 0; i < 4; i++) {
		if (!Native_True(&ch[i])) {
			continue;
		}
		Native_Free(anm);
		if (!Sm_SActName("CreateAction", &ch[i]) ||
			!Mode_Cat(&t, anm, 5, Mode_S(&t, "Models/Vehicles/Ships/"), model, Mode_S(&t, "/Anm/"), &ch[i], Mode_S(&t, ".M3D")) ||
			!LvN_SetAct("AnmFilePath", anm) || !LvN_SetAct("Chanel", Mode_I(&t, i))) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, LvN_SetCls("DefAction", &ch[0]) && Sm_Verbose(&t, "Animated Vehicle: '", model, "'  Loaded.\n"));
}

/* the channels ch0 to ch3 (None when not given) as the array the core takes */
static byte Sm_InitAnmShipK(const nativeVal_t *const *a, nativeVal_t *out) {
	nativeVal_t ch[4];
	(void)out;
	for (int i = 0; i < 4; i++) {
		if (a[1 + i]) {
			Native_Copy(&ch[i], a[1 + i]);
		} else {
			Native_SetNone(&ch[i]);
		}
	}
	byte ok = Sm_InitAnmShipN(a[0], ch);
	for (int i = 0; i < 4; i++) {
		Native_Free(&ch[i]);
	}
	return ok;
}

static byte ShipMaker_InitAnmShip_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "O|OOOO", Sm_InitAnmShipK, out);
}

static char *ShipMaker_InitAnmShip_N[] = { "model", "ch0", "ch1", "ch2", "ch3", NULL };

static PyObject *ShipMaker_InitAnmShip(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Mode_KwCall(args, kw, "O|OOOO", ShipMaker_InitAnmShip_N, Sm_InitAnmShipK);
}

MODE_FN(ShipMaker, SetShipPrice, 2, Sm_SetShipPriceN(&a[0], &a[1]))
MODE_FN(ShipMaker, GetShipPrice, 1, Sm_Get("shipsCosts", &a[0], out))
MODE_FN(ShipMaker, SetEnginePrice, 2, Sm_Put("enginesCosts", &a[0], &a[1]))
MODE_FN(ShipMaker, GetEnginePrice, 1, Sm_Get("enginesCosts", &a[0], out))
MODE_FN(ShipMaker, AddShipTon, 4, Sm_AddShipTonN(a))
MODE_FN(ShipMaker, GetShipTon, 1, Sm_Get("ShipTon", &a[0], out))
MODE_FN(ShipMaker, AddShipBalance, 2, Sm_Put("ShipBalance", &a[0], &a[1]))
MODE_FN(ShipMaker, GetShipBalance, 1, Sm_Get("ShipBalance", &a[0], out))
MODE_FN(ShipMaker, GetShip, 1, Sm_Get("ShipValues", &a[0], out))
MODE_FN(ShipMaker, AddMotor, 5, Sm_AddMotorN(a))
MODE_FN(ShipMaker, GetMotor, 1, Sm_GetMotorN(&a[0], out))
MODE_FN(ShipMaker, IncludeMotor, 7, Sm_IncludeMotorN(a, out))
MODE_FN(ShipMaker, AddWeapon, 3, Sm_AddWeaponN(&a[0], &a[1], &a[2]))
MODE_FN(ShipMaker, GetWeapon, 1, Sm_GetWeaponN(&a[0], out))
MODE_FN(ShipMaker, CalculateVerticalBalance, 1, Sm_CalculateVerticalBalanceN(&a[0]))
MODE_FN(ShipMaker, SetUpChars, 1, Sm_SetUpCharsN(&a[0]))
MODE_FN(ShipMaker, AddChar, 2, Sm_AddCharN(&a[0], &a[1]))
MODE_FN(ShipMaker, GetBoostFactor, 2, Sm_GetBoostFactorN(&a[0], &a[1], out))
MODE_FN(ShipMaker, SetShipParams, 1, Sm_SetShipParamsN(&a[0], out))
MODE_FN(ShipMaker, CreateStdShips, 0, Sm_CreateStdShipsN())
MODE_FN(ShipMaker, Init, 0, Sm_InitN())
MODE_FN(ShipMaker, SetAnmShip, 2, Sm_SetAnmShipN(&a[0], &a[1]))

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

PORTS(ShipMaker)
