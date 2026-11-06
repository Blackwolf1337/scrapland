/*
 * The Vehicles module in C (scripts/vehicles/vehicles.py, issue #2): ships dying (the falling hull, the lost engines,
 * the explosions, the coin and the taunt of single player), respawning, their set up from a model and the camera
 * callbacks. The explosion and sound factors, HullLifeTimeDeath, OnDeathCallback, ForceDeathTaunt,
 * DelPlayerShipOnDeath and the net chain LastClientCreate/LastClientAfterCreate stay variables of the module, the
 * module code (the crash speeds) stays in the script. ShipMaker, OutSound, Parking and the other modules are called
 * through their modules.
 */
#include "scrap/script.h"
#include "scrap/scrap.h"

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)
#define BAD() return (Script_Done(&r, NULL), 0)

/* Scrap.Rand(a, b) */
static PyObject *Veh_Rand(double a, double b) {
	return PyFloat_FromDouble(Scrap_RandRange(a, b));
}

/* a op b with the number rules of Python (borrowed; NULL stays NULL) */
static PyObject *Veh_Op(PyObject *(*op)(PyObject *, PyObject *), PyObject *a, PyObject *b) {
	return a && b ? op(a, b) : NULL;
}

/* a op n */
static PyObject *Veh_OpN(PyObject *(*op)(PyObject *, PyObject *), PyObject *a, long n) {
	PyObject *b = a ? PyInt_FromLong(n) : NULL, *v = Veh_Op(op, a, b);
	Py_XDECREF(b);
	return v;
}

/* the module global of Vehicles (a new reference, NameError when missing) */
static PyObject *Veh_G(const char *name) {
	return Script_Global("Vehicles", name);
}

/* fn(args) through the module global fn, as the script calls its own functions (args given away; a new reference) */
static PyObject *Veh_Call(const char *fn, PyObject *args) {
	PyObject *f = Veh_G(fn);
	if (!f) {
		Py_XDECREF(args);
		return NULL;
	}
	PyObject *v = Script_Invoke(f, args);
	Py_DECREF(f);
	return v;
}

/* if SVec.Mod(Vel) > 1000: Vel = SVec.Prod(SVec.Norm(Vel), 1000) (a new reference) */
static PyObject *Veh_Limit(PyObject *vel) {
	PyObject *mod = vel ? Script_Ask("SVec", "Mod", "(O)", vel) : NULL;
	int fast = mod ? Script_NumCmp(mod, 1000) : -2;
	Py_XDECREF(mod);
	if (fast == -2) {
		return NULL;
	}
	if (fast <= 0) {
		return Script_Ref(vel);
	}
	PyObject *norm = Script_Ask("SVec", "Norm", "(O)", vel), *v = norm ? Script_Ask("SVec", "Prod", "(Oi)", norm, 1000) : NULL;
	Py_XDECREF(norm);
	return v;
}

/* SVec.Add(Pos, (Scrap.Rand(-5,5), Scrap.Rand(-5,5), Scrap.Rand(-5,5))) */
static PyObject *Veh_Jitter(PyObject *pos) {
	double x = Scrap_RandRange(-5, 5), y = Scrap_RandRange(-5, 5), z = Scrap_RandRange(-5, 5);
	return Script_Ask("SVec", "Add", "(O(ddd))", pos, x, y, z);
}

/* ShipExplosionSoundAtt<Ini|End> + radius * ShipExplosionSoundAtt<Ini|End>RadiusFactor */
static PyObject *Veh_Att(PyObject *radius, const char *att, const char *factor) {
	PyObject *a = Veh_G(att), *f = a ? Veh_G(factor) : NULL;
	PyObject *m = Veh_Op(PyNumber_Multiply, radius, f), *v = Veh_Op(PyNumber_Add, a, m);
	Py_XDECREF(m);
	Py_XDECREF(f);
	Py_XDECREF(a);
	return v;
}

/*
 * SFX.ShipExplosion(pos, vel, (Scrap.Rand(lo, hi) + radius/div) * ShipExplosionScaleFactor, Scrap.Rand(tlo, thi),
 * big ? int(Scrap.Rand(2,3)) : 0, 0), then SSound.PlaySound(soundPos, sound, ShipExplosionSoundVol, the two
 * attenuations of the radius, 0); radius is read through e and var each time the script reads it
 */
static byte Veh_Boom(PyObject *pos, PyObject *vel, PyObject *e, const char *var, double lo, double hi, long div, double tlo,
	double thi, byte big, PyObject *soundPos, const char *sound) {
	scriptRefs_t r = { .n = 0 };
	PyObject *rnd = K(Veh_Rand(lo, hi)), *radius = K(PyObject_GetAttrString(e, (char *)var));
	PyObject *part = K(Veh_OpN(PyNumber_Divide, radius, div)), *sum = K(Veh_Op(PyNumber_Add, rnd, part));
	PyObject *f = sum ? K(Veh_G("ShipExplosionScaleFactor")) : NULL, *size = K(Veh_Op(PyNumber_Multiply, sum, f));
	PyObject *t = size ? K(Veh_Rand(tlo, thi)) : NULL;
	PyObject *n = !t ? NULL : big ? K(Script_Int(K(Veh_Rand(2, 3)))) : K(PyInt_FromLong(0));
	if (!n || !Script_Try("SFX", "ShipExplosion", "(OOOOOi)", pos, vel, size, t, n, 0)) {
		BAD();
	}
	PyObject *at = soundPos ? soundPos : K(PyObject_GetAttrString(e, "Pos"));
	PyObject *vol = at ? K(Veh_G("ShipExplosionSoundVol")) : NULL;
	PyObject *r1 = vol ? K(PyObject_GetAttrString(e, (char *)var)) : NULL;
	PyObject *ini = r1 ? K(Veh_Att(r1, "ShipExplosionSoundAttIni", "ShipExplosionSoundAttIniRadiusFactor")) : NULL;
	PyObject *r2 = ini ? K(PyObject_GetAttrString(e, (char *)var)) : NULL;
	PyObject *end = r2 ? K(Veh_Att(r2, "ShipExplosionSoundAttEnd", "ShipExplosionSoundAttEndRadiusFactor")) : NULL;
	byte ok = end && Script_Try("SSound", "PlaySound", "(OsOOOi)", at, sound, vol, ini, end, 0);
	Script_Done(&r, NULL);
	return ok;
}

/* the motor loop of the lost engines and hulls */
static byte Veh_MotorLoop(PyObject *l) {
	return Script_HSet(l, "snd00_name", PyString_FromString("SoloMotorEngine")) &&
		Script_HSet(l, "snd00_loop", PyInt_FromLong(1)) && Script_HSet(l, "snd00_vol0", PyFloat_FromDouble(0.25));
}

/* x, y, z = seq as a tuple (x, y, z) (a new reference) */
static PyObject *Veh_XYZ(PyObject *seq) {
	PyObject *xyz[3];
	if (!seq || !Script_Unpack(seq, 3, xyz)) {
		return NULL;
	}
	PyObject *t = Py_BuildValue("(OOO)", xyz[0], xyz[1], xyz[2]);
	Py_DECREF(xyz[0]);
	Py_DECREF(xyz[1]);
	Py_DECREF(xyz[2]);
	return t;
}

/* ResourceName[0:4] == "DmM:" (-1 with the error set) */
static int Veh_IsMotor(PyObject *res) {
	PyObject *head = PySequence_GetSlice(res, 0, 4);
	int is = head ? Script_Is(head, "DmM:") : -1;
	Py_XDECREF(head);
	return is;
}

/* ---- the net resources of the lost engines ("DmM:<model>") ---- */

static PyObject *Vehicles_ClientCreate(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *res;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &res)) {
		return NULL;
	}
	int mine = Veh_IsMotor(res);
	if (mine < 0) {
		FAIL();
	}
	if (mine) {
		PyObject *model = K(PySequence_GetSlice(res, 4, 0x7fffffff));
		PyObject *l = model ? K(Veh_Call("CreateDumbMotor", Py_BuildValue("(O(iii)(iii)O)", name, 0, 0, 0, 0, 0, 0, model))) : NULL;
		return Script_Done(&r, l && Script_HSet(l, "OnHit", PyString_FromString("Vehicles.MotorHit")) ? Py_ReturnNone() : NULL);
	}
	return Script_Done(&r, K(Veh_Call("LastClientCreate", Py_BuildValue("(OO)", name, res))) ? Py_ReturnNone() : NULL);
}

static PyObject *Vehicles_ClientAfterCreate(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *res;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &res)) {
		return NULL;
	}
	int mine = Veh_IsMotor(res);
	if (mine < 0) {
		FAIL();
	}
	PyObject *model = mine ? K(PySequence_GetSlice(res, 4, 0x7fffffff)) : NULL;
	PyObject *v = !mine ? K(Veh_Call("LastClientAfterCreate", Py_BuildValue("(OO)", name, res))) :
		model ? K(Veh_Call("SetUpDumbMotor", Py_BuildValue("(OO)", name, model))) : NULL;
	return Script_Done(&r, v ? Py_ReturnNone() : NULL);
}

/* ServerMotorHit(Missile, winner, looser): MotorHit, the one hit loses 80 (hit by itself when nobody shot) */
static PyObject *Vehicles_ServerMotorHit(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *msl, *winner, *looser;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &msl, &winner, &looser) ||
		!K(Veh_Call("MotorHit", Py_BuildValue("(OOO)", msl, winner, looser)))) {
		FAIL();
	}
	if (Script_Is(looser, "")) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *ls = K(Script_Entity(looser)), *life = ls ? K(PyObject_GetAttrString(ls, "Life")) : NULL;
	if (!life || !Script_HSet(ls, "Life", Veh_OpN(PyNumber_Subtract, life, 80)) ||
		(Script_Is(winner, "") && !Script_HSet(ls, "Hit_Target", Script_Ref(looser)))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* ls.Vel = ls.Vel[i] + msl.Vel[i]*Speed1 for the three axes, Speed1 = 10000/ls.Mass */
static byte Veh_Push(PyObject *ls, PyObject *msl) {
	scriptRefs_t r = { .n = 0 };
	PyObject *mass = K(PyObject_GetAttrString(ls, "Mass")), *k = mass ? K(PyInt_FromLong(10000)) : NULL;
	PyObject *speed = K(Veh_Op(PyNumber_Divide, k, mass));
	PyObject *lv = speed ? K(PyObject_GetAttrString(ls, "Vel")) : NULL, *mv = lv ? K(PyObject_GetAttrString(msl, "Vel")) : NULL;
	PyObject *vel = mv ? K(PyTuple_New(3)) : NULL;
	for (int i = 0; vel && i < 3; i++) {
		PyObject *a = K(Script_Index(lv, i)), *b = a ? K(Script_Index(mv, i)) : NULL;
		PyObject *m = K(Veh_Op(PyNumber_Multiply, b, speed)), *s = Veh_Op(PyNumber_Add, a, m);
		if (!s) {
			BAD();
		}
		PyTuple_SET_ITEM(vel, i, s);
	}
	byte ok = vel && Script_HSet(ls, "Vel", Script_Ref(vel));
	Script_Done(&r, NULL);
	return ok;
}

/* MotorHit(Missile, winner, looser): the engine explodes; the one hit is pushed by it */
static PyObject *Vehicles_MotorHit(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *missile, *winner, *looser, *at;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &missile, &winner, &looser)) {
		return NULL;
	}
	PyObject *msl = K(Script_Entity(missile));
	if (!msl) {
		FAIL();
	}
	if (!Script_Is(looser, "")) {
		PyObject *ls = K(Script_Entity(looser));
		at = ls ? K(Veh_XYZ(K(PyObject_GetAttrString(ls, "Pos")))) : NULL;
		if (!at || !K(PyObject_GetAttrString(ls, "VRadius")) || !Veh_Push(ls, msl)) {
			FAIL();
		}
	} else {
		at = K(Veh_XYZ(K(PyObject_GetAttrString(msl, "Pos"))));
		if (!at || !K(PyObject_GetAttrString(msl, "Radius"))) {
			FAIL();
		}
	}
	PyObject *rnd = K(Veh_Rand(0.1, 0.2)), *radius = K(PyObject_GetAttrString(msl, "Radius"));
	PyObject *part = K(Veh_OpN(PyNumber_Divide, radius, 80)), *sum = K(Veh_Op(PyNumber_Add, rnd, part));
	PyObject *f = sum ? K(Veh_G("MotorExplosionScaleFactor")) : NULL, *size = K(Veh_Op(PyNumber_Multiply, sum, f));
	PyObject *t = size ? K(Veh_Rand(0.8, 1.0)) : NULL;
	if (!t || !Script_Try("SFX", "ShipExplosion", "(O(iii)OOii)", at, 0, 0, 0, size, t, 0, 0)) {
		FAIL();
	}
	PyObject *pos = K(PyObject_GetAttrString(msl, "Pos")), *vol = pos ? K(Veh_G("MotorExplosionSoundVol")) : NULL;
	PyObject *ini = vol ? K(Veh_G("MotorExplosionSoundAttIni")) : NULL, *end = ini ? K(Veh_G("MotorExplosionSoundAttEnd")) : NULL;
	if (!end || !Script_Try("SSound", "PlaySound", "(OsOOOi)", pos, "SoloMotorBoom", vol, ini, end, 0)) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(msl));
}

/* CreateDumbMotor(Name, Pos, Ang, modelname): a lost engine flying on */
static PyObject *Vehicles_CreateDumbMotor(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *pos, *ang, *model;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &name, &pos, &ang, &model)) {
		return NULL;
	}
	PyObject *x = K(Script_Index(pos, 0)), *y = x ? K(Script_Index(pos, 1)) : NULL, *z = y ? K(Script_Index(pos, 2)) : NULL;
	PyObject *l = z ? K(Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "Missile")) : NULL;
	byte ok = l && Script_HSet(l, "Ang", Script_Ref(ang)) && Script_HSet(l, "LifeTime", PyFloat_FromDouble(4.7)) &&
		Script_HSet(l, "Speed", PyFloat_FromDouble(4000.0)) && Script_HSet(l, "Type", PyString_FromString("Dumb")) &&
		Script_HSet(l, "NetRes", Script_SPlus("DmM:", model)) && Script_HSet(l, "HitType", PyString_FromString("Hit"));
	return Script_Done(&r, ok ? Script_Ref(l) : NULL);
}

/* "<dir><model>/<model>.m3d" (a new reference) */
static PyObject *Veh_ModelFile(const char *dir, PyObject *model) {
	return Script_PlusS(Script_Plus(Script_PlusS(Script_SPlus(dir, model), "/"), model), ".m3d");
}

/* SetUpDumbMotor(Name, modelname): the engine's model, size, effect and loop */
static PyObject *Vehicles_SetUpDumbMotor(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *model;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &model)) {
		return NULL;
	}
	PyObject *l = K(Script_Entity(name)), *file = l ? K(Veh_ModelFile("Models/Vehicles/Engines/", model)) : NULL;
	byte ok = file && K(Script_Method(l, "SetModel", "(O)", file)) && Script_HSet(l, "Radius", PyInt_FromLong(100)) &&
		Script_HSet(l, "TopRadius", PyInt_FromLong(100)) && Script_HSet(l, "RadiusGrow", PyInt_FromLong(0)) &&
		K(Script_Method(l, "SetFX", "(s)", "DumbMotor")) && Veh_MotorLoop(l);
	return Script_Done(&r, ok ? Script_Ref(l) : NULL);
}

/* DumbShipHit(Missile, winner, looser): the falling hull hits and explodes */
static PyObject *Vehicles_DumbShipHit(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *missile, *winner, *looser;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &missile, &winner, &looser) || !Script_Import((const char *const[]){ "SVec", NULL })) {
		return NULL;
	}
	PyObject *msl = K(Script_Entity(missile)), *pos = msl ? K(PyObject_GetAttrString(msl, "Pos")) : NULL;
	PyObject *vel = pos ? K(PyObject_GetAttrString(msl, "Vel")) : NULL, *lim = vel ? K(Veh_Limit(vel)) : NULL;
	PyObject *at = lim ? K(Veh_Jitter(pos)) : NULL;
	if (!at || !Veh_Boom(at, lim, msl, "Radius", 2.8, 3.0, 250, 1.8, 2.1, 1, at, "ShipXplode")) {
		FAIL();
	}
	return Script_Done(&r, Script_Ref(msl));
}

/* the falling hull of OnClientDeath (doit), with the first explosion when it is not the player's */
static byte Veh_Hull(PyObject *v, PyObject *name, PyObject *pos, PyObject *vel, PyObject *ang, PyObject *speed,
	PyObject *model, int isusr) {
	scriptRefs_t r = { .n = 0 };
	int fast = Script_NumCmp(speed, 10000);
	if (fast == -2) {
		BAD();
	}
	if (fast > 0) {
		speed = K(PyInt_FromLong(10000));
	}
	PyObject *ename = K(Script_Plus(Script_Ref(model), name)), *x = ename ? K(Script_Index(pos, 0)) : NULL;
	PyObject *y = x ? K(Script_Index(pos, 1)) : NULL, *z = y ? K(Script_Index(pos, 2)) : NULL;
	PyObject *l = z ? K(Script_Ask("Scrap", "CreateEntity", "(OOOOs)", ename, x, y, z, "Missile")) : NULL;
	PyObject *hull = l && Script_HSet(l, "Ang", Script_Ref(ang)) ? K(Veh_G("HullLifeTimeDeath")) : NULL;
	PyObject *rand = hull ? K(Script_Attr("Scrap", "Rand")) : NULL, *life = rand ? K(Script_Invoke(rand, Script_Ref(hull))) : NULL;
	if (!life || !Script_HSet(l, "LifeTime", Script_Ref(life)) ||
		!Script_HSet(l, "Speed", Veh_Op(PyNumber_Multiply, speed, K(Veh_Rand(0.45, 0.65)))) ||
		!Script_HSet(l, "Type", PyString_FromString("Fall")) || !Script_HSet(l, "HitType", PyString_FromString("Hit"))) {
		BAD();
	}
	PyObject *first = K(Script_Index(model, 0)), *file = NULL;
	if (!first) {
		BAD();
	}
	if (Script_Is(first, "T")) {
		PyObject *path = Script_Import((const char *const[]){ "Traffic", NULL }) ? K(Script_Attr("Traffic", "Path")) : NULL;
		file = path ? K(Script_PlusS(Script_Plus(Script_Ref(path), model), ".m3d")) : NULL;
	} else {
		file = K(Veh_ModelFile("Models/Vehicles/Ships/", model));
	}
	if (!file || !K(Script_Method(l, "SetModel", "(O)", file)) ||
		!Script_HSet(l, "Radius", PyObject_GetAttrString(v, "WRadius")) ||
		!Script_HSet(l, "TopRadius", PyObject_GetAttrString(v, "WRadius")) || !Script_HSet(l, "RadiusGrow", PyInt_FromLong(0)) ||
		!Script_HSet(l, "OnHit", PyString_FromString("Vehicles.DumbShipHit")) || !K(Script_Method(l, "SetFX", "(s)", "DumbMotor")) ||
		!Veh_MotorLoop(l)) {
		BAD();
	}
	PyObject *at = K(Veh_Jitter(pos));
	byte ok = at && (isusr || Veh_Boom(at, vel, v, "WRadius", 0.6, 0.8, 750, 0.8, 1.0, 0, NULL, "SoloMotorBoom"));
	Script_Done(&r, NULL);
	return ok;
}

/* OnClientDeath(LooserName, WinnerName): the hull falls on when the ship moved (always the player's), else it explodes */
static PyObject *Vehicles_OnClientDeath(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *looser, *winner;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner) || !Script_Import((const char *const[]){ "string", NULL })) {
		return NULL;
	}
	PyObject *v = K(Script_Entity(looser)), *usr = v ? K(Script_Usr()) : NULL;
	PyObject *pos = usr ? K(PyObject_GetAttrString(v, "Pos")) : NULL, *vel = pos ? K(PyObject_GetAttrString(v, "Vel")) : NULL;
	PyObject *ang = vel ? K(PyObject_GetAttrString(v, "Ang")) : NULL, *speed = ang ? K(PyObject_GetAttrString(v, "Speed")) : NULL;
	PyObject *model = speed ? K(PyObject_GetAttrString(v, "Model")) : NULL, *lim = model ? K(Veh_Limit(vel)) : NULL;
	if (!lim) {
		FAIL();
	}
	/* isusr = (usr != None and v != None and usr.Name == v.Name) */
	int isusr = Script_Eq(usr, Py_None);
	isusr = isusr < 0 ? -1 : !isusr;
	if (isusr > 0) {
		isusr = Script_Eq(v, Py_None);
		isusr = isusr < 0 ? -1 : !isusr;
	}
	if (isusr > 0) {
		PyObject *a = K(PyObject_GetAttrString(usr, "Name")), *b = a ? K(PyObject_GetAttrString(v, "Name")) : NULL;
		isusr = b ? Script_Eq(a, b) : -1;
	}
	if (isusr < 0) {
		FAIL();
	}
	int doit = 1;
	if (!isusr) {
		doit = Script_NumCmp(speed, 2000);
		if (doit == -2) {
			FAIL();
		}
		doit = doit > 0 && Scrap_RandRange(0.0, 1.0) > 0.2;
	}
	PyObject *inplace = K(Veh_G("ShipExplosionForceInplace"));
	int force = inplace ? Script_True(inplace) : -1;
	if (force < 0) {
		FAIL();
	}
	if (force) {
		doit = 0;
	}
	byte ok = doit ? Veh_Hull(v, looser, pos, lim, ang, speed, model, isusr) :
		Veh_Boom(pos, lim, v, "WRadius", 2.8, 3.0, 250, 1.8, 2.1, 1, pos, "ShipXplode");
	if (!ok || !Script_HSet(v, "Active", PyInt_FromLong(0)) || !Script_HSet(v, "Vel", Py_BuildValue("(iii)", 0, 0, 0))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* ActivateHitType(v, Hittype) */
static PyObject *Vehicles_ActivateHitType(PyObject *self, PyObject *args) {
	PyObject *v, *type;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &v, &type)) {
		return NULL;
	}
	return Script_HSet(v, "HitType", Script_Ref(type)) ? Py_ReturnNone() : NULL;
}

/* GetCoin(vehicleName, coinPos): a live ship goes for the coin, back on its route when it cannot reach it */
static PyObject *Vehicles_GetCoin(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *coin;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &coin)) {
		return NULL;
	}
	PyObject *v = K(Script_Entity(name));
	int live = v ? Script_True(v) : -1;
	if (live > 0) {
		PyObject *active = K(PyObject_GetAttrString(v, "Active"));
		live = active ? Script_True(active) : -1;
	}
	if (live <= 0) {
		return Script_Done(&r, live < 0 ? NULL : Py_ReturnNone());
	}
	PyObject *x = Script_Import((const char *const[]){ "MisItems", NULL }) ? K(Script_Index(coin, 0)) : NULL;
	PyObject *y = x ? K(Script_Index(coin, 1)) : NULL, *z = y ? K(Script_Index(coin, 2)) : NULL;
	PyObject *rad = z ? K(Script_Attr("MisItems", "COIN_RADIUS_OUTDOOR")) : NULL;
	PyObject *st = rad ? K(Script_Ask("SAI", "SetStateVehicle", "(iOOOOOiddd)", 1, name, x, y, z, rad, 0, 1.0, 20.0, 20.0)) : NULL;
	int stuck = st ? Script_NumCmp(st, 9) : -2;
	if (stuck == -2 || (stuck == 0 && !Script_Try("SAI", "SetStateVehicle", "(iO)", 3, name))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

static PyObject *Vehicles_GetRandomMercenaryHead(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	int i = (int)Scrap_RandRange(0.0, 2.99);
	return PyString_FromString(i == 0 ? "MercenaryA" : i == 1 ? "MercenaryB" : "MercenaryC");
}

/* ---- OnDeath ---- */

/* the player's ship of single player is lost: its hangar marked destroyed, the emergency ship next to Sputnik */
static byte Veh_LosePlayerShip(void) {
	scriptRefs_t r = { .n = 0 };
	if (!Script_Import((const char *const[]){ "ShipEdit", "HangarTab", "Sputnik", "Parking", NULL })) {
		BAD();
	}
	PyObject *s = K(Script_SaveVar("Hangar.iHangarShip")), *hangar = s ? K(Script_Int(s)) : NULL;
	PyObject *text = hangar ? K(Script_SaveVar("Hangar.DestroyedShips")) : NULL;
	PyObject *lost = text ? K(Script_EvalIn("Vehicles", text)) : NULL;
	PyObject *one = lost ? K(PyInt_FromLong(1)) : NULL, *zero = one ? K(PyInt_FromLong(0)) : NULL, *nine = zero ? K(PyInt_FromLong(9)) : NULL;
	if (!nine || PyObject_SetItem(lost, hangar, one) < 0 || PyObject_SetItem(lost, nine, zero) < 0) {
		BAD();
	}
	PyObject *rep = K(Script_Repr(lost));
	PyObject *ship = rep && Script_Try("Scrap", "SetSaveVar", "(sO)", "Hangar.DestroyedShips", rep) &&
		Script_Try("Scrap", "SetSaveVar", "(ss)", "Hangar.iHangarShip", "9") ? K(Script_Attr("Parking", "PlayerShip")) : NULL;
	if (!ship || !K(Script_Method(ship, "Del", "()"))) {
		BAD();
	}
	Script_SetVar("Parking", "PlayerShip", Script_Ref(Py_None));
	PyObject *spt = K(Script_Attr("Sputnik", "spt")), *at = spt ? K(PyObject_GetAttrString(spt, "Pos")) : NULL;
	ship = at && Script_Try("Parking", "CreatePlayerShip", "(O)", at) ? K(Script_Attr("Parking", "PlayerShip")) : NULL;
	PyObject *plate = ship && Script_HSet(ship, "Active", PyInt_FromLong(0)) ? K(Script_Attr("Parking", "DtritusPlate")) : NULL;
	byte ok = plate && Script_HSet(plate, "Active", PyInt_FromLong(0));
	Script_Done(&r, NULL);
	return ok;
}

/* the attacker of FightingTraffic goes for the coin the dead ship drops */
static byte Veh_CoinForAttacker(PyObject *v, PyObject *winner) {
	scriptRefs_t r = { .n = 0 };
	PyObject *attacker = K(Script_Entity(winner));
	int is = attacker ? Script_True(attacker) : -1;
	if (is > 0) {
		PyObject *list = K(PyObject_GetAttrString(attacker, "EntityList"));
		is = list ? Script_Is(list, "FightingTraffic") : -1;
	}
	if (is <= 0) {
		Script_Done(&r, NULL);
		return is == 0;
	}
	PyObject *xyz[3];
	PyObject *pos = Script_Import((const char *const[]){ "MisItems", NULL }) ? K(PyObject_GetAttrString(v, "Pos")) : NULL;
	if (!pos || !Script_Unpack(pos, 3, xyz)) {
		BAD();
	}
	K(xyz[0]), K(xyz[1]), K(xyz[2]);
	PyObject *limit = K(Script_Ask("Scrap", "Get", "(s)", "AtmosphereLimit"));
	int high = limit ? PyObject_Compare(xyz[1], limit) : 0;
	if (!limit || PyErr_Occurred()) {
		BAD();
	}
	if (high >= 0) {
		PyObject *e = K(Script_Entity(winner));
		int live = e ? Script_True(e) : -1;
		PyObject *epos = live > 0 ? K(PyObject_GetAttrString(e, "Pos")) : NULL;
		if (live < 0 || (live > 0 && !(epos && (xyz[1] = K(Script_Index(epos, 1)))))) {
			BAD();
		}
	}
	PyObject *t = K(Script_Attr("MisItems", "MONEY_APPEARS_TIME")), *at = t ? K(PyNumber_Float(t)) : NULL;
	byte ok = at && Script_LaterIn(PyFloat_AsDouble(at), "Vehicles", "GetCoin",
		Py_BuildValue("(O(OOO))", winner, xyz[0], xyz[1], xyz[2]), NULL);
	Script_Done(&r, NULL);
	return ok;
}

/* the other FightingTraffic ships after LooserName go back to Traffic */
static byte Veh_FreeHunters(PyObject *looser, PyObject *winner) {
	PyObject *name = Script_Ask("Scrap", "EntityListGet", "(ss)", "FightingTraffic", "First");
	PyObject *e = name ? Script_Entity(name) : NULL;
	Py_XDECREF(name);
	for (;;) {
		scriptRefs_t r = { .n = 0 };
		int live = e ? Script_True(K(e)) : -1;
		if (live <= 0) {
			Script_Done(&r, NULL);
			return live == 0;
		}
		PyObject *next = K(PyObject_GetAttrString(e, "NextInList")), *ename = next ? K(PyObject_GetAttrString(e, "Name")) : NULL;
		int other = ename ? Script_Eq(ename, winner) : -1;
		other = other < 0 ? -1 : !other;
		if (other > 0) {
			PyObject *target = K(PyObject_GetAttrString(e, "AI_Target"));
			other = target ? Script_Eq(target, looser) : -1;
		}
		if (other < 0 || (other && (!Script_HSet(e, "EntityList", PyString_FromString("Traffic")) ||
			!(ename = K(PyObject_GetAttrString(e, "Name"))) || !Script_Try("SAI", "SetStateVehicle", "(iO)", 3, ename)))) {
			BAD();
		}
		e = Script_Entity(next);
		Script_Done(&r, NULL);
	}
}

/* the death taunt of the player's victim (half of the time unless ForceDeathTaunt) */
static byte Veh_Taunt(PyObject *v, PyObject *winner) {
	scriptRefs_t r = { .n = 0 };
	int go = Scrap_RandRange(0.0, 1.0) > 0.5;
	if (!go) {
		PyObject *force = K(Veh_G("ForceDeathTaunt"));
		go = force ? Script_True(force) : -1;
	}
	if (go <= 0) {
		Script_Done(&r, NULL);
		return go == 0;
	}
	PyObject *usr = K(Script_Usr());
	int mine = usr ? Script_True(usr) : -1;
	if (mine > 0) {
		PyObject *n = K(PyObject_GetAttrString(usr, "Name"));
		mine = n ? Script_Eq(winner, n) : -1;
	}
	if (mine <= 0) {
		Script_Done(&r, NULL);
		return mine == 0;
	}
	PyObject *mpil = K(PyObject_GetAttrString(v, "MPil0")), *pilot = mpil ? K(PySequence_GetSlice(mpil, 2, 0x7fffffff)) : NULL;
	if (!pilot) {
		BAD();
	}
	if (Script_In(pilot, (const char *const[]){ "None", "", NULL })) {
		Script_Done(&r, NULL);
		return 1;
	}
	PyObject *head = pilot, *text;
	if (!Script_Import((const char *const[]){ "SLogic", NULL })) {
		BAD();
	}
	if (Script_Is(pilot, "Police")) {
		PyObject *n = K(PyInt_FromLong((long)(int)Scrap_RandRange(0.0, 2.9) + 13));
		text = n ? K(Script_SPlus("Generics_PoliceTaunt_", K(Script_Repr(n)))) : NULL;
	} else {
		PyObject *at = K(Script_Ask("string", "find", "(Os)", pilot, "Merc"));
		int merc = at ? Script_NumCmp(at, -1) : -2;
		if (merc == -2) {
			BAD();
		}
		if (merc != 0) {
			pilot = K(PyString_FromString("Mercs"));
		}
		text = K(Script_PlusS(Script_SPlus("Generics_", pilot), "Taunt_8"));
	}
	byte ok = text && Script_Try("Speech", "RemoteComunication", "(OOi)", text, head, -3) &&
		Script_Try("SLogic", "UpdateTauntEndTime", "()");
	Script_Done(&r, NULL);
	return ok;
}

/* the single player part of OnDeath */
static byte Veh_DeathSingle(PyObject *v, PyObject *looser, PyObject *winner) {
	scriptRefs_t r = { .n = 0 };
	PyObject *taunt = K(Script_Attr("Speech", "LaunchedTauntEntityName"));
	int same = taunt ? Script_Eq(looser, taunt) : -1;
	if (same < 0 || (same && (!Script_Try("Scrap", "Set", "(si)", "RemainTalkTime", 0) || !Script_Try("Speech", "AbortSpeech", "()")))) {
		BAD();
	}
	PyObject *list = K(PyObject_GetAttrString(v, "EntityList"));
	if (!list) {
		BAD();
	}
	if (Script_Is(list, "Players")) {
		PyObject *msg = K(PyString_FromString("GAME OVER"));
		if (!msg || !Script_Print(&msg, 1) || !Script_Try("Parking", "SetDtritusPlate", "(Oi)", v, 0)) {
			BAD();
		}
	}
	PyObject *parked = K(Script_Ask("Parking", "ParkedDeat", "(OO)", v, winner));
	int p = parked ? Script_True(parked) : -1;
	if (p < 0 || (!p && (!Script_Import((const char *const[]){ "MisItems", NULL }) ||
		!Script_Try("MisItems", "OnVehicleDeat", "(O)", v)))) {
		BAD();
	}
	PyObject *ship = K(Script_Attr("Parking", "PlayerShip")), *sname = ship ? K(PyObject_GetAttrString(ship, "Name")) : NULL;
	PyObject *vname = sname ? K(PyObject_GetAttrString(v, "Name")) : NULL;
	int mine = vname ? Script_Eq(sname, vname) : -1;
	if (mine > 0) {
		PyObject *del = K(Veh_G("DelPlayerShipOnDeath"));
		mine = del ? Script_True(del) : -1;
	}
	byte ok = mine >= 0 && (!mine || Veh_LosePlayerShip()) && Veh_CoinForAttacker(v, winner) && Veh_FreeHunters(looser, winner) &&
		Veh_Taunt(v, winner);
	Script_Done(&r, NULL);
	return ok;
}

/* the lost engines of each of the four engine kinds (NEng<k> of model MEng<k>, at PEng<k><A..>, AEng<k><A..>) */
static byte Veh_Motors(PyObject *v) {
	for (int k = 0; k < 4; k++) {
		scriptRefs_t r = { .n = 0 };
		char var[0x10];
		Str_Printf(var, sizeof(var), "NEng%d", k);
		PyObject *num = K(PyObject_GetAttrString(v, var));
		int any = num ? Script_NumCmp(num, 0) : -2;
		if (any == -2) {
			BAD();
		}
		if (any <= 0) {
			Script_Done(&r, NULL);
			continue;
		}
		Str_Printf(var, sizeof(var), "MEng%d", k);
		PyObject *model = K(PyObject_GetAttrString(v, var));
		long count;
		if (!model || !Script_Range(num, &count)) {
			BAD();
		}
		for (long i = 0; i < count; i++) {
			scriptRefs_t q = { .n = 0 };
			if (i >= 26) {
				PyErr_SetString(PyExc_IndexError, "string index out of range");
				Script_Done(&q, NULL);
				BAD();
			}
			Str_Printf(var, sizeof(var), "PEng%d%c", k, (int)('A' + i));
			PyObject *pos = Script_Keep(&q, PyObject_GetAttrString(v, var));
			Str_Printf(var, sizeof(var), "AEng%d%c", k, (int)('A' + i));
			PyObject *ang = pos ? Script_Keep(&q, PyObject_GetAttrString(v, var)) : NULL;
			PyObject *oname = ang ? Script_Keep(&q, Script_Ask("SNet", "GetObjName", "()")) : NULL;
			PyObject *l = oname ? Script_Keep(&q, Veh_Call("CreateDumbMotor", Py_BuildValue("(OOOO)", oname, pos, ang, model))) : NULL;
			PyObject *off = l && Script_HSet(l, "OnHit", PyString_FromString("Vehicles.ServerMotorHit")) &&
				Script_HSet(l, "HitType", PyString_FromString("None")) ? Script_Keep(&q, Script_Attr("Init", "OffLineGame")) : NULL;
			int offline = off ? Script_True(off) : -1;
			PyObject *lname = NULL;
			byte ok = offline >= 0 && Script_HSet(l, "LifeTime", offline ? Veh_Rand(2.2, 5.2) : Veh_Rand(4.0, 4.5)) &&
				Script_LaterIn(0.3, "Vehicles", "ActivateHitType", Py_BuildValue("(Os)", l, "Hit"), NULL) &&
				(lname = Script_Keep(&q, PyObject_GetAttrString(l, "Name"))) &&
				Script_Keep(&q, Veh_Call("SetUpDumbMotor", Py_BuildValue("(OO)", lname, model)));
			Script_Done(&q, NULL);
			if (!ok) {
				BAD();
			}
		}
		Script_Done(&r, NULL);
	}
	return 1;
}

/* OnDeath(LooserName, WinnerName): the effects, the single player consequences, the items and the lost engines */
static PyObject *Vehicles_OnDeath(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *looser, *winner;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner) ||
		!Script_Import((const char *const[]){ "string", "Init", "Parking", "Speech", NULL })) {
		return NULL;
	}
	PyObject *v = K(Script_Entity(looser));
	if (!v || !K(PyObject_GetAttrString(v, "Pos")) || !K(PyObject_GetAttrString(v, "Ang")) ||
		!K(PyObject_GetAttrString(v, "Vel")) || !K(PyObject_GetAttrString(v, "Model")) ||
		!K(Veh_Call("OnClientDeath", Py_BuildValue("(OO)", looser, winner)))) {
		FAIL();
	}
	PyObject *flags = K(Script_Ask("Scrap", "GetNetFlags", "()")), *off = flags ? K(Py_BuildValue("(iii)", 0, 0, 0)) : NULL;
	int single = off ? Script_Eq(flags, off) : -1;
	if (single > 0) {
		PyObject *split = K(Script_Ask("Scrap", "Get", "(s)", "SplitScreen")), *zero = split ? K(PyInt_FromLong(0)) : NULL;
		single = zero ? Script_Eq(split, zero) : -1;
	}
	if (single < 0 || (single && !Veh_DeathSingle(v, looser, winner)) ||
		!Script_Import((const char *const[]){ "Items", NULL }) || !Script_Try("Items", "DropAll", "(O)", looser) || !Veh_Motors(v)) {
		FAIL();
	}
	PyObject *cb = K(Veh_G("OnDeathCallback"));
	int set = cb ? Script_True(cb) : -1;
	if (set < 0 || (set && !K(Script_Invoke(cb, Py_BuildValue("(OO)", looser, winner))))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* ---- cameras ---- */

/* SwitchView(CamName, idControl): the HUD of the racer or walker, and who the player controls */
static PyObject *Vehicles_SwitchView(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *camName, *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &camName, &id)) {
		return NULL;
	}
	int on = Script_NumCmp(id, 0);
	if (on == -2) {
		FAIL();
	}
	if (on < 0) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *set = K(Script_Ask("SInput", "GetActionSet", "()"));
	if (!set || (Script_In(set, (const char *const[]){ "Racer", "Walk", NULL }) &&
		(!Script_Import((const char *const[]){ "RacerScorer", NULL }) || !Script_Try("RacerScorer", "Create", "(O)", id)))) {
		FAIL();
	}
	PyObject *cam = K(Script_Entity(camName)), *main = cam ? K(PyObject_GetAttrString(cam, "MainTarget")) : NULL;
	PyObject *ply = main ? K(Script_Entity(main)) : NULL, *t = ply ? K(PyObject_GetAttrString(cam, "Target")) : NULL;
	PyObject *m = t ? K(PyObject_GetAttrString(cam, "MainTarget")) : NULL;
	int same = m ? Script_Eq(t, m) : -1;
	if (same < 0 || !Script_HSet(ply, "UsrControl", same ? Script_Ref(id) : PyInt_FromLong(-1))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SetActiveCam(ply, cam, id): camera cam of player id follows ply */
static PyObject *Vehicles_SetActiveCam(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *plyName, *camName, *id;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &plyName, &camName, &id)) {
		return NULL;
	}
	PyObject *cam = K(Script_Entity(camName)), *ply = cam ? K(Script_Entity(plyName)) : NULL;
	PyObject *cname = ply ? K(PyObject_GetAttrString(cam, "Name")) : NULL;
	byte ok = cname && Script_Try("Scrap", "SetCam", "(OO)", id, cname) &&
		Script_HSet(cam, "OnSwitch", PyString_FromString("Vehicles.SwitchView")) &&
		Script_HSet(cam, "MainTarget", PyObject_GetAttrString(ply, "Name")) &&
		Script_HSet(cam, "Target", PyObject_GetAttrString(ply, "Name")) && Script_HSet(cam, "FreeCam", PyInt_FromLong(0));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* ---- respawn and set up ---- */

/* v.<prefix>0<d> = value for d in range(count) (the exec lines of ReSpawnShip) */
static byte Veh_SetEach(PyObject *v, const char *prefix, int d, PyObject *value) {
	char var[0x20];
	Str_Printf(var, sizeof(var), "%s0%d", prefix, d);
	return Script_HSet(v, var, value);
}

/* the ammo (an eighth of the most), and online the upgrades cleared, of a ship coming back */
static byte Veh_Restock(PyObject *v) {
	scriptRefs_t r = { .n = 0 };
	PyObject *na = K(Script_Ask("SWeap", "GetNAmmo", "()"));
	long n;
	if (!na || !Script_Range(na, &n)) {
		BAD();
	}
	for (long d = 0; d < n; d++) {
		PyObject *max = Script_Ask("SWeap", "GetFAmmo", "(ls)", d, "Max"), *part = Veh_OpN(PyNumber_Divide, max, 8);
		PyObject *i = Script_Int(part);
		Py_XDECREF(part);
		Py_XDECREF(max);
		if (!i || !Veh_SetEach(v, "Ammo", (int)d, i)) {
			BAD();
		}
	}
	PyObject *off = K(Script_Attr("Init", "OffLineGame"));
	int offline = off ? Script_True(off) : -1;
	if (offline < 0) {
		BAD();
	}
	if (!offline) {
		PyObject *nw = K(Script_Ask("SWeap", "GetNWeap", "()"));
		if (!nw || !Script_Range(nw, &n)) {
			BAD();
		}
		for (long d = 0; d < n; d++) {
			if (!Veh_SetEach(v, "Upgrd", (int)d, PyInt_FromLong(0)) || !Veh_SetEach(v, "UAmmo", (int)d, PyInt_FromLong(0))) {
				BAD();
			}
		}
	}
	Script_Done(&r, NULL);
	return 1;
}

/* ReSpawnShip(EntityName): the ship back to life (a client's net ship only gets its death function), boost, fire and
 * weapons reset, the laser selected */
static PyObject *Vehicles_ReSpawnShip(PyObject *self, PyObject *args) {
	static const struct {
		const char *var;
		long value;
	} reset[] = { { "isBoost", 1 }, { "BoostDeact", 0 }, { "MotorDCnt", 0 }, { "EMICounter", 0 }, { "FireRemainTime", 0 },
		{ "FireDelayTime", 1 }, { "FireStamp", 0 }, { "CMStamp", 0 }, { "WeapA06", 1 }, { "AcWeap", 6 } };
	scriptRefs_t r = { .n = 0 };
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name) || !Script_Import((const char *const[]){ "string", "Init", "Parking", NULL })) {
		return NULL;
	}
	PyObject *v = K(Script_Entity(name)), *flags = v ? K(Script_Ask("Scrap", "GetNetFlags", "()")) : NULL;
	PyObject *f0 = flags ? K(Script_Index(flags, 0)) : NULL;
	int client = f0 ? Script_True(f0) : -1;
	if (client > 0) {
		PyObject *head = K(PySequence_GetSlice(name, 0, 3));
		client = head ? Script_Is(head, "net") : -1;
	}
	if (client < 0) {
		FAIL();
	}
	if (client) {
		if (!Script_HSet(v, "OnDeath", PyString_FromString("Vehicles.OnClientDeath"))) {
			FAIL();
		}
	} else if (!Script_HSet(v, "OnDeath", PyString_FromString("Vehicles.OnDeath")) || !Script_HSet(v, "Active", PyInt_FromLong(1)) ||
		!Script_HSet(v, "Life", PyObject_GetAttrString(v, "MaxLife")) || !Script_HSet(v, "Vel", Py_BuildValue("(iii)", 0, 0, 0)) ||
		!Script_HSet(v, "ForceAll", PyInt_FromLong(1)) || !Veh_Restock(v) || !Script_Try("Parking", "SetDtritusPlate", "(Oi)", v, 1)) {
		FAIL();
	}
	for (size_t i = 0; i < sizeof(reset) / sizeof(reset[0]); i++) {
		if (!Script_HSet(v, reset[i].var, PyInt_FromLong(reset[i].value))) {
			FAIL();
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* SetProperties(EntityName, Model, MaxLife, Pilot, Motor0..3, WeapList, PlayerName, localedit=0): the ship's model
 * and parts (not on a net client), then the parameters of ShipMaker; the handle, None for a bad configuration */
static PyObject *Vehicles_SetProperties(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "EntityName", "Model", "MaxLife", "Pilot", "Motor0", "Motor1", "Motor2", "Motor3", "WeapList",
		"PlayerName", "localedit", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *model, *life, *pilot, *m[4], *weap, *player, *local = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOOOOOOOOO|O", names, &name, &model, &life, &pilot, &m[0], &m[1], &m[2], &m[3],
		&weap, &player, &local) || !Script_Import((const char *const[]){ "ShipMaker", "OutSound", NULL })) {
		return NULL;
	}
	PyObject *v = K(Script_Entity(name));
	if (!v || !Script_HSet(v, "isInit", PyInt_FromLong(1)) || !Script_HSet(v, "FxEngineTime", PyFloat_FromDouble(0.5)) ||
		!Script_HSet(v, "FxBoostEngineTime", PyFloat_FromDouble(0.25))) {
		FAIL();
	}
	PyObject *flags = K(Script_Ask("Scrap", "GetNetFlags", "()"));
	int edit = local ? Script_True(local) : 0;
	if (!flags || edit < 0) {
		FAIL();
	}
	if (!edit) {
		PyObject *f0 = K(Script_Index(flags, 0));
		int client = f0 ? Script_True(f0) : -1;
		if (client < 0) {
			FAIL();
		}
		edit = !client;
	}
	if (edit) {
		PyObject *cbs = K(Script_Attr("ShipMaker", "ShipCallback")), *has = cbs ? K(Script_Method(cbs, "has_key", "(O)", model)) : NULL;
		int own = has ? Script_True(has) : -1;
		if (own < 0) {
			FAIL();
		}
		if (own) {
			PyObject *entry = K(PyObject_GetItem(cbs, model)), *fn = entry ? K(Script_Index(entry, 0)) : NULL;
			if (!fn || !K(Script_Invoke(fn, Py_BuildValue("(OO)", v, model)))) {
				FAIL();
			}
		} else if (!K(Script_Method(v, "SetModel", "(O)", model))) {
			FAIL();
		}
		static const char *const parts[] = { "MEng0", "MEng1", "MEng2", "MEng3" };
		if (!Script_HSet(v, "WeapList", Script_Ref(weap))) {
			FAIL();
		}
		for (int i = 0; i < 4; i++) {
			if (!Script_HSet(v, parts[i], Script_Ref(m[i]))) {
				FAIL();
			}
		}
		if (!Script_HSet(v, "Descriptor", Script_Ref(player)) || !Script_HSet(v, "MPil0", Script_Ref(pilot)) ||
			!Script_HSet(v, "MaxLife", Script_Ref(life))) {
			FAIL();
		}
	}
	PyObject *valid = K(Script_Ask("ShipMaker", "SetShipParams", "(O)", name));
	int ok = valid ? Script_True(valid) : -1;
	if (ok < 0) {
		FAIL();
	}
	if (ok) {
		return Script_Done(&r, K(Veh_Call("ReSpawnShip", Py_BuildValue("(O)", name))) &&
			Script_Try("OutSound", "SetVehicleSounds", "(O)", name) ? Script_Ref(v) : NULL);
	}
	PyObject *msg = K(Script_PlusS(Script_SPlus("H E R E T I C    E R R O R : ", name), " is out of configuration.\n"));
	return Script_Done(&r, msg && Script_Try("Scrap", "ConsoleError", "(O)", msg) ? Py_ReturnNone() : NULL);
}

/* SetStdProperties(EntityName, Model, Pilot): SetProperties from ShipMaker.StdShips of the model */
static PyObject *Vehicles_SetStdProperties(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name, *model, *pilot;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &model, &pilot) || !Script_Import((const char *const[]){ "ShipMaker", "string", NULL })) {
		return NULL;
	}
	PyObject *std = K(Script_Attr("ShipMaker", "StdShips")), *up = std ? K(Script_Ask("string", "upper", "(O)", model)) : NULL;
	PyObject *a = up ? K(PyObject_GetItem(std, up)) : NULL, *it[7] = { NULL };
	for (int i = 0; a && i < 7; i++) {
		if (!(it[i] = K(Script_Index(a, i)))) {
			FAIL();
		}
	}
	PyObject *up2 = a ? K(Script_Ask("string", "upper", "(O)", model)) : NULL;
	PyObject *title = up2 ? K(Script_LangOf(Script_SPlus("SHIP_", up2))) : NULL;
	PyObject *v = title ? K(Veh_Call("SetProperties", Py_BuildValue("(OOOOOOOOOO)", name, it[0], it[6], pilot, it[1], it[2], it[3],
		it[4], it[5], title))) : NULL;
	return Script_Done(&r, v ? Py_ReturnNone() : NULL);
}

/* Init(): the net chain, then ShipMaker and OutSound */
static PyObject *Vehicles_Init(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	if (!Script_Import((const char *const[]){ "Net", NULL }) ||
		!Script_Hook("Vehicles", "Net", "ClientAfterCreate", "LastClientAfterCreate") ||
		!Script_Hook("Vehicles", "Net", "ClientCreate", "LastClientCreate") ||
		!Script_Import((const char *const[]){ "ShipMaker", NULL }) || !Script_Try("ShipMaker", "Init", "()") ||
		!Script_Import((const char *const[]){ "OutSound", NULL }) || !Script_Try("OutSound", "Init", "()")) {
		return NULL;
	}
	PyObject *msg = PyString_FromString("Vehicle.Init() executed.");
	byte ok = msg && Script_Print(&msg, 1);
	Py_XDECREF(msg);
	return ok ? Py_ReturnNone() : NULL;
}

PyMethodDef g_VehiclesMethods[] = {
	{ "ClientCreate", Vehicles_ClientCreate, 1, NULL },
	{ "ClientAfterCreate", Vehicles_ClientAfterCreate, 1, NULL },
	{ "ServerMotorHit", Vehicles_ServerMotorHit, 1, NULL },
	{ "MotorHit", Vehicles_MotorHit, 1, NULL },
	{ "CreateDumbMotor", Vehicles_CreateDumbMotor, 1, NULL },
	{ "SetUpDumbMotor", Vehicles_SetUpDumbMotor, 1, NULL },
	{ "DumbShipHit", Vehicles_DumbShipHit, 1, NULL },
	{ "OnClientDeath", Vehicles_OnClientDeath, 1, NULL },
	{ "ActivateHitType", Vehicles_ActivateHitType, 1, NULL },
	{ "GetCoin", Vehicles_GetCoin, 1, NULL },
	{ "GetRandomMercenaryHead", Vehicles_GetRandomMercenaryHead, 1, NULL },
	{ "OnDeath", Vehicles_OnDeath, 1, NULL },
	{ "SwitchView", Vehicles_SwitchView, 1, NULL },
	{ "SetActiveCam", Vehicles_SetActiveCam, 1, NULL },
	{ "ReSpawnShip", Vehicles_ReSpawnShip, 1, NULL },
	{ "SetProperties", (PyCFunction)(void (*)(void))Vehicles_SetProperties, 3, NULL },
	{ "SetStdProperties", Vehicles_SetStdProperties, 1, NULL },
	{ "Init", Vehicles_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
