/*
 * The Vehicles module in C (scripts/vehicles/vehicles.py, issue #2): ships dying (the falling hull, the lost engines,
 * the explosions, the coin and the taunt of single player), respawning, their set up from a model and the camera
 * callbacks. The explosion and sound factors, HullLifeTimeDeath, OnDeathCallback, ForceDeathTaunt,
 * DelPlayerShipOnDeath and the net chain LastClientCreate/LastClientAfterCreate stay variables of the module, the
 * module code (the crash speeds) stays in the script. ShipMaker, OutSound, Parking and the other modules are called
 * through their modules.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters. The
 * variables of the module are read where the script reads them as its globals (OBJLIB 5: the missions and the cases set
 * OnDeathCallback, ForceDeathTaunt and the factors). The ships, missiles and cameras are handle values (OBJLIB 7). The
 * other ports are called through their refs (PORT_CALL: directly while the attribute holds the port's function, else
 * by name), the module's own functions through its globals as the script calls them (PORT_OWN, the same seam on the
 * global: a mission, a net weapon or a case may replace one). The net chain stays Script_Hook on Net's functions, the
 * hooks LastClientCreate and LastClientAfterCreate are called through the module's globals by name and OnDeathCallback
 * and the ShipCallback functions of ShipMaker as values (OBJLIB 5: variables). SAI, SWeap, SLogic and Scrap.SetCam go
 * through their refs (stage 7), SFX, SNet.GetObjName and SSound.PlaySound (its six arguments) too; Scrap.GetEntity,
 * CreateEntity, UsrEntity,
 * GetNetFlags, EntityListGet, Set, ConsoleError and AddScheduledFunc go through their seams (OBJLIB 7). The texts of
 * the net resources of the lost engines ("DmM:<model>") stay as they were.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define VEH "Vehicles"

/* if SVec.Mod(Vel) > 1000: Vel = SVec.Prod(SVec.Norm(Vel), 1000) */
static byte Veh_Limit(const nativeVal_t *vel, nativeVal_t *out) {
	nativeVal_t mod, norm, k;
	Native_SetNone(&mod);
	Native_SetNone(&norm);
	Native_SetInt(&k, 1000);
	int fast = Mode_VecMod(vel, &mod) ? Mode_CmpI(&mod, ">", 1000) : -1;
	byte ok = fast >= 0 && (fast ? Mode_VecNorm(vel, &norm) && Mode_VecProd(&norm, &k, out) : (Native_Copy(out, vel), 1));
	Native_Free(&mod);
	Native_Free(&norm);
	return ok;
}

/* SVec.Add(Pos, (Scrap.Rand(-5,5), Scrap.Rand(-5,5), Scrap.Rand(-5,5))) */
static byte Veh_Jitter(const nativeVal_t *pos, nativeVal_t *out) {
	double x = Scrap_RandRange(-5, 5), y = Scrap_RandRange(-5, 5), z = Scrap_RandRange(-5, 5);
	nativeVal_t d;
	Native_SetVec(&d, x, y, z);
	return Mode_VecAdd(pos, &d, out);
}

/* ShipExplosionSoundAtt<Ini|End> + radius * ShipExplosionSoundAtt<Ini|End>RadiusFactor */
static byte Veh_Att(const nativeVal_t *radius, const char *att, const char *factor, nativeVal_t *out) {
	nativeVal_t a, f, m;
	Native_SetNone(&f);
	Native_SetNone(&m);
	byte ok = Mode_G(VEH, att, &a) && Mode_G(VEH, factor, &f) && Mode_Arith('*', radius, &f, &m) && Native_Arith('+', &a, &m, out);
	Native_Free(&a);
	Native_Free(&f);
	Native_Free(&m);
	return ok;
}

/*
 * SFX.ShipExplosion(pos, vel, (Scrap.Rand(lo, hi) + radius/div) * ShipExplosionScaleFactor, Scrap.Rand(tlo, thi),
 * big ? int(Scrap.Rand(2,3)) : 0, 0), then SSound.PlaySound(soundPos, sound, ShipExplosionSoundVol, the two
 * attenuations of the radius, 0); radius is read through e and var each time the script reads it
 */
static byte Veh_Boom(const nativeVal_t *pos, const nativeVal_t *vel, const nativeVal_t *e, const char *var, double lo, double hi,
	long div, double tlo, double thi, byte big, const nativeVal_t *soundPos, const char *sound) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rnd = Mode_V(&t), *radius = Mode_V(&t), *part = Mode_V(&t), *sum = Mode_V(&t), *f = Mode_V(&t);
	nativeVal_t *size = Mode_V(&t), *tm = Mode_V(&t), *r23 = Mode_V(&t), *n = Mode_V(&t), *at = Mode_V(&t), *vol = Mode_V(&t);
	nativeVal_t *ini = Mode_V(&t), *end = Mode_V(&t);
	Mode_RandF(lo, hi, rnd);
	if (!Mode_VObj(e, var, radius) || !Mode_Arith('/', radius, Mode_I(&t, div), part) || !Native_Arith('+', rnd, part, sum) ||
		!Mode_G(VEH, "ShipExplosionScaleFactor", f) || !Mode_Arith('*', sum, f, size)) {
		return Mode_Done(&t, 0);
	}
	Mode_RandF(tlo, thi, tm);
	if (big) {
		Mode_RandF(2, 3, r23);
		if (!Native_ToInt(r23, n)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_SetInt(n, 0);
	}
	if (!PORT_CALL(SFX, ShipExplosion, NULL, "vvvvvi", pos, vel, size, tm, n, 0L)) {
		return Mode_Done(&t, 0);
	}
	if (!soundPos) {
		if (!Mode_VObj(e, "Pos", at)) {
			return Mode_Done(&t, 0);
		}
		soundPos = at;
	}
	return Mode_Done(&t, Mode_G(VEH, "ShipExplosionSoundVol", vol) && Mode_VObjRead(e, var, radius) &&
		Veh_Att(radius, "ShipExplosionSoundAttIni", "ShipExplosionSoundAttIniRadiusFactor", ini) && Mode_VObjRead(e, var, radius) &&
		Veh_Att(radius, "ShipExplosionSoundAttEnd", "ShipExplosionSoundAttEndRadiusFactor", end) &&
		PORT_CALL(SSound, PlaySound, NULL, "vsvvvi", soundPos, sound, vol, ini, end, 0L));
}

/* the motor loop of the lost engines and hulls */
static byte Veh_MotorLoop(const nativeVal_t *l) {
	return LvN_SetS(l, "snd00_name", "SoloMotorEngine") && LvN_SetI(l, "snd00_loop", 1) && LvN_SetF(l, "snd00_vol0", 0.25);
}

/* h.SetFX(type) of a handle value (an entity in C; a handle of an entity that is gone or another value the Python way,
 * OBJLIB 7) */
static byte Veh_SetFX(const nativeVal_t *h, const char *type) {
	cEntity *e = h->type == NV_ENT ? Native_Ent(h) : NULL;
	if (e) {
		return Mode_SetFX(e, type);
	}
	PyObject *m = Mode_VBound(h, "SetFX");
	byte ok = m && Native_Call(m, NULL, "s", type);
	Py_XDECREF(m);
	return ok;
}

/* x, y, z = seq as a tuple (x, y, z) */
static byte Veh_XYZ(const nativeVal_t *seq, nativeVal_t *out) {
	nativeVal_t xyz[3];
	if (!Mode_Unpack(seq, 3, xyz)) {
		return 0;
	}
	Native_SetEmpty(out, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		*Native_Append(out) = xyz[i];
	}
	return 1;
}

/* ResourceName[0:4] == "DmM:": 1, 0, -1 with the error; the model (ResourceName[4:]) into model */
static int Veh_IsMotor(const nativeVal_t *res, nativeVal_t *model) {
	nativeVal_t head;
	Native_SetNone(&head);
	if (!Native_Slice(res, 0, 4, &head)) {
		return -1;
	}
	int is = Native_IsStr(&head, "DmM:");
	Native_Free(&head);
	return !is ? 0 : Native_Slice(res, 4, 0x7fffffff, model) ? 1 : -1;
}

/* ---- the net resources of the lost engines ("DmM:<model>") ---- */

static byte Veh_ClientCreateN(const nativeVal_t *name, const nativeVal_t *res) {
	nativeVal_t model, z, l;
	Native_SetNone(&model);
	Native_SetNone(&l);
	int mine = Veh_IsMotor(res, &model);
	byte ok = mine >= 0;
	if (mine > 0) {
		Mode_Ints3(&z, 0, 0, 0);
		ok = PORT_OWN(Vehicles, CreateDumbMotor, &l, "vvvv", name, &z, &z, &model) && LvN_SetS(&l, "OnHit", "Vehicles.MotorHit");
		Native_Free(&z);
	} else if (ok) {
		/* OBJLIB 5: the hook of the net chain, a variable of the module */
		ok = Native_CallGlobal(VEH, "LastClientCreate", NULL, "vv", name, res);
	}
	Native_Free(&model);
	Native_Free(&l);
	return ok;
}

static byte Veh_ClientAfterCreateN(const nativeVal_t *name, const nativeVal_t *res) {
	nativeVal_t model;
	Native_SetNone(&model);
	int mine = Veh_IsMotor(res, &model);
	/* OBJLIB 5: LastClientAfterCreate, the hook of the net chain, is a variable of the module */
	byte ok = mine >= 0 && (mine ? PORT_OWN(Vehicles, SetUpDumbMotor, NULL, "vv", name, &model) :
		Native_CallGlobal(VEH, "LastClientAfterCreate", NULL, "vv", name, res));
	Native_Free(&model);
	return ok;
}

/* ServerMotorHit(Missile, winner, looser): MotorHit, the one hit loses 80 (hit by itself when nobody shot) */
static byte Veh_ServerMotorHitN(const nativeVal_t *msl, const nativeVal_t *winner, const nativeVal_t *looser) {
	if (!PORT_OWN(Vehicles, MotorHit, NULL, "vvv", msl, winner, looser)) {
		return 0;
	}
	if (Native_IsStr(looser, "")) {
		return 1;
	}
	modeVals_t t = { .n = 0 };
	nativeVal_t *ls = Mode_V(&t), *life = Mode_V(&t), *left = Mode_V(&t);
	return Mode_Done(&t, Mode_EntitySeam(looser, ls) && Mode_VObj(ls, "Life", life) && Native_Arith('-', life, Mode_I(&t, 80), left) &&
		Mode_VSet(ls, "Life", left) && (!Native_IsStr(winner, "") || Mode_VSet(ls, "Hit_Target", looser)));
}

/* ls.Vel = ls.Vel[i] + msl.Vel[i]*Speed1 for the three axes, Speed1 = 10000/ls.Mass */
static byte Veh_Push(const nativeVal_t *ls, const nativeVal_t *msl) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mass = Mode_V(&t), *speed = Mode_V(&t), *lv = Mode_V(&t), *mv = Mode_V(&t), *vel = Mode_V(&t);
	if (!Mode_VObj(ls, "Mass", mass) || !Mode_Arith('/', Mode_I(&t, 10000), mass, speed) || !Mode_VObj(ls, "Vel", lv) ||
		!Mode_VObj(msl, "Vel", mv)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(vel, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		int mark = t.n;
		nativeVal_t *a = Mode_V(&t), *b = Mode_V(&t), *m = Mode_V(&t);
		if (!LvN_Item(lv, i, a) || !LvN_Item(mv, i, b) || !Mode_Arith('*', b, speed, m) || !Native_Arith('+', a, m, Native_Append(vel))) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, Mode_VSet(ls, "Vel", vel));
}

/* MotorHit(Missile, winner, looser): the engine explodes; the one hit is pushed by it */
static byte Veh_MotorHitN(const nativeVal_t *missile, const nativeVal_t *looser, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *msl = Mode_V(&t), *ls = Mode_V(&t), *p = Mode_V(&t), *at = Mode_V(&t), *x = Mode_V(&t), *rnd = Mode_V(&t);
	nativeVal_t *radius = Mode_V(&t), *part = Mode_V(&t), *sum = Mode_V(&t), *f = Mode_V(&t), *size = Mode_V(&t);
	nativeVal_t *tm = Mode_V(&t), *z = Mode_V(&t), *pos = Mode_V(&t), *vol = Mode_V(&t), *ini = Mode_V(&t), *end = Mode_V(&t);
	if (!Mode_EntitySeam(missile, msl)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_IsStr(looser, "")) {
		if (!Mode_EntitySeam(looser, ls) || !Mode_VObj(ls, "Pos", p) || !Veh_XYZ(p, at) || !Mode_VObj(ls, "VRadius", x) || !Veh_Push(ls, msl)) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_VObj(msl, "Pos", p) || !Veh_XYZ(p, at) || !Mode_VObj(msl, "Radius", x)) {
		return Mode_Done(&t, 0);
	}
	Mode_RandF(0.1, 0.2, rnd);
	if (!Mode_VObj(msl, "Radius", radius) || !Mode_Arith('/', radius, Mode_I(&t, 80), part) || !Native_Arith('+', rnd, part, sum) ||
		!Mode_G(VEH, "MotorExplosionScaleFactor", f) || !Mode_Arith('*', sum, f, size)) {
		return Mode_Done(&t, 0);
	}
	Mode_RandF(0.8, 1.0, tm);
	Mode_Ints3(z, 0, 0, 0);
	if (!PORT_CALL(SFX, ShipExplosion, NULL, "vvvvii", at, z, size, tm, 0L, 0L) || !Mode_VObj(msl, "Pos", pos) ||
		!Mode_G(VEH, "MotorExplosionSoundVol", vol) || !Mode_G(VEH, "MotorExplosionSoundAttIni", ini) ||
		!Mode_G(VEH, "MotorExplosionSoundAttEnd", end) ||
		!PORT_CALL(SSound, PlaySound, NULL, "vsvvvi", pos, "SoloMotorBoom", vol, ini, end, 0L)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, msl);
	return Mode_Done(&t, 1);
}

/* CreateDumbMotor(Name, Pos, Ang, modelname): a lost engine flying on */
static byte Veh_CreateDumbMotorN(const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *ang, const nativeVal_t *model,
	nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *l = Mode_V(&t), *res = Mode_V(&t);
	if (!LvN_Item(pos, 0, x) || !LvN_Item(pos, 1, y) || !LvN_Item(pos, 2, z) || !LvN_Create(name, x, y, z, "Missile", l) ||
		!Mode_VSet(l, "Ang", ang) || !LvN_SetF(l, "LifeTime", 4.7) || !LvN_SetF(l, "Speed", 4000.0) || !LvN_SetS(l, "Type", "Dumb") ||
		!Mode_Cat(&t, res, 2, Mode_S(&t, "DmM:"), model) || !Mode_VSet(l, "NetRes", res) || !LvN_SetS(l, "HitType", "Hit")) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, l);
	return Mode_Done(&t, 1);
}

/* "<dir><model>/<model>.m3d" */
static byte Veh_ModelFile(modeVals_t *t, const char *dir, const nativeVal_t *model, nativeVal_t *out) {
	return Mode_Cat(t, out, 5, Mode_S(t, dir), model, Mode_S(t, "/"), model, Mode_S(t, ".m3d"));
}

/* SetUpDumbMotor(Name, modelname): the engine's model, size, effect and loop */
static byte Veh_SetUpDumbMotorN(const nativeVal_t *name, const nativeVal_t *model, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *file = Mode_V(&t);
	if (!Mode_EntitySeam(name, l) || !Veh_ModelFile(&t, "Models/Vehicles/Engines/", model, file) || !Mode_VModel(l, file) ||
		!LvN_SetI(l, "Radius", 100) || !LvN_SetI(l, "TopRadius", 100) || !LvN_SetI(l, "RadiusGrow", 0) ||
		!Veh_SetFX(l, "DumbMotor") || !Veh_MotorLoop(l)) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, l);
	return Mode_Done(&t, 1);
}

/* DumbShipHit(Missile, winner, looser): the falling hull hits and explodes */
static byte Veh_DumbShipHitN(const nativeVal_t *missile, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *msl = Mode_V(&t), *pos = Mode_V(&t), *vel = Mode_V(&t), *lim = Mode_V(&t), *at = Mode_V(&t);
	if (!Mode_Import("SVec") || !Mode_EntitySeam(missile, msl) || !Mode_VObj(msl, "Pos", pos) || !Mode_VObj(msl, "Vel", vel) ||
		!Veh_Limit(vel, lim) || !Veh_Jitter(pos, at) || !Veh_Boom(at, lim, msl, "Radius", 2.8, 3.0, 250, 1.8, 2.1, 1, at, "ShipXplode")) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, msl);
	return Mode_Done(&t, 1);
}

/* f(*args) of the script with args a value: a tuple (or vector) in C, the TypeError of PyEval_CallObject for other
 * values */
static byte Veh_Apply(const nativeVal_t *f, const nativeVal_t *args, nativeVal_t *out) {
	nativeVal_t t;
	if (args->type != NV_TUPLE && args->type != NV_VEC) {
		return Native_Raise(NE_TYPE, "argument list must be a tuple");
	}
	if (args->type == NV_VEC) {
		Native_SetEmpty(&t, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetFloat(Native_Append(&t), args->v[i]);
		}
	} else {
		Native_Copy(&t, args);
	}
	byte ok = Mode_CallArgs(f, out, t.items, t.n);
	Native_Free(&t);
	return ok;
}

/* the falling hull of OnClientDeath (doit), with the first explosion when it is not the player's */
static byte Veh_Hull(const nativeVal_t *v, const nativeVal_t *name, const nativeVal_t *pos, const nativeVal_t *vel,
	const nativeVal_t *ang, const nativeVal_t *speed, const nativeVal_t *model, int isusr) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ename = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *l = Mode_V(&t), *hull = Mode_V(&t);
	nativeVal_t *rand = Mode_V(&t), *life = Mode_V(&t), *r = Mode_V(&t), *sp = Mode_V(&t), *first = Mode_V(&t);
	nativeVal_t *path = Mode_V(&t), *file = Mode_V(&t), *wr = Mode_V(&t), *at = Mode_V(&t);
	int fast = Mode_CmpI(speed, ">", 10000);
	if (fast < 0) {
		return Mode_Done(&t, 0);
	}
	if (fast) {
		speed = Mode_I(&t, 10000);
	}
	if (!Native_Arith('+', model, name, ename) || !LvN_Item(pos, 0, x) || !LvN_Item(pos, 1, y) || !LvN_Item(pos, 2, z) ||
		!LvN_Create(ename, x, y, z, "Missile", l) || !Mode_VSet(l, "Ang", ang) || !Mode_G(VEH, "HullLifeTimeDeath", hull) ||
		!LvN_ModGet("Scrap", "Rand", rand) || !Veh_Apply(rand, hull, life) || !Mode_VSet(l, "LifeTime", life)) {
		return Mode_Done(&t, 0);
	}
	Mode_RandF(0.45, 0.65, r);
	if (!Mode_Arith('*', speed, r, sp) || !Mode_VSet(l, "Speed", sp) || !LvN_SetS(l, "Type", "Fall") ||
		!LvN_SetS(l, "HitType", "Hit") || !LvN_Item(model, 0, first)) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(first, "T")) {
		if (!Mode_Import("Traffic") || !LvN_ModGet("Traffic", "Path", path) || !Mode_Cat(&t, file, 3, path, model, Mode_S(&t, ".m3d"))) {
			return Mode_Done(&t, 0);
		}
	} else if (!Veh_ModelFile(&t, "Models/Vehicles/Ships/", model, file)) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_VModel(l, file) || !Mode_VObjRead(v, "WRadius", wr) || !Mode_VSet(l, "Radius", wr) || !Mode_VObjRead(v, "WRadius", wr) ||
		!Mode_VSet(l, "TopRadius", wr) || !LvN_SetI(l, "RadiusGrow", 0) || !LvN_SetS(l, "OnHit", "Vehicles.DumbShipHit") ||
		!Veh_SetFX(l, "DumbMotor") || !Veh_MotorLoop(l) || !Veh_Jitter(pos, at)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, isusr || Veh_Boom(at, vel, v, "WRadius", 0.6, 0.8, 750, 0.8, 1.0, 0, NULL, "SoloMotorBoom"));
}

/* OnClientDeath(LooserName, WinnerName): the hull falls on when the ship moved (always the player's), else it explodes */
static byte Veh_OnClientDeathN(const nativeVal_t *looser) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *usr = Mode_V(&t), *pos = Mode_V(&t), *vel = Mode_V(&t), *ang = Mode_V(&t), *speed = Mode_V(&t);
	nativeVal_t *model = Mode_V(&t), *lim = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *inplace = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_EntitySeam(looser, v) || !Mode_UsrHandle(usr) || !Mode_VObj(v, "Pos", pos) ||
		!Mode_VObj(v, "Vel", vel) || !Mode_VObj(v, "Ang", ang) || !Mode_VObj(v, "Speed", speed) || !Mode_VObj(v, "Model", model) ||
		!Veh_Limit(vel, lim)) {
		return Mode_Done(&t, 0);
	}
	/* isusr = (usr != None and v != None and usr.Name == v.Name) */
	int isusr = Mode_Cmp(usr, "!=", Mode_V(&t));
	if (isusr > 0) {
		isusr = Mode_Cmp(v, "!=", Mode_V(&t));
	}
	if (isusr > 0) {
		isusr = Mode_VObj(usr, "Name", a) && Mode_VObj(v, "Name", b) ? Mode_Cmp(a, "==", b) : -1;
	}
	if (isusr < 0) {
		return Mode_Done(&t, 0);
	}
	int doit = 1;
	if (!isusr) {
		doit = Mode_CmpI(speed, ">", 2000);
		if (doit < 0) {
			return Mode_Done(&t, 0);
		}
		doit = doit && Scrap_RandRange(0.0, 1.0) > 0.2;
	}
	if (!Mode_G(VEH, "ShipExplosionForceInplace", inplace)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(inplace)) {
		doit = 0;
	}
	byte ok = doit ? Veh_Hull(v, looser, pos, lim, ang, speed, model, isusr) :
		Veh_Boom(pos, lim, v, "WRadius", 2.8, 3.0, 250, 1.8, 2.1, 1, pos, "ShipXplode");
	return Mode_Done(&t, ok && LvN_SetI(v, "Active", 0) && LvN_SetInts3(v, "Vel", 0, 0, 0));
}

/* GetCoin(vehicleName, coinPos): a live ship goes for the coin, back on its route when it cannot reach it */
static byte Veh_GetCoinN(const nativeVal_t *name, const nativeVal_t *coin) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *active = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *rad = Mode_V(&t);
	nativeVal_t *st = Mode_V(&t);
	if (!Mode_EntitySeam(name, v)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(v)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VObj(v, "Active", active)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(active)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Import("MisItems") || !LvN_Item(coin, 0, x) || !LvN_Item(coin, 1, y) || !LvN_Item(coin, 2, z) ||
		!LvN_ModGet("MisItems", "COIN_RADIUS_OUTDOOR", rad) ||
		!PORT_CALL(SAI, SetStateVehicle, st, "ivvvvviddd", 1L, name, x, y, z, rad, 0L, 1.0, 20.0, 20.0)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !LvN_EqI(st, 9) || PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 3L, name));
}

static PyObject *Vehicles_GetRandomMercenaryHead(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	int i = (int)Scrap_RandRange(0.0, 2.99);
	return PyString_FromString(i == 0 ? "MercenaryA" : i == 1 ? "MercenaryB" : "MercenaryC");
}

/* ---- OnDeath ---- */

/* v[key] = x of the script on a native value (OBJLIB 9: the item assignment of the Python value, with its errors) */
static byte Veh_SetItem(nativeVal_t *v, const nativeVal_t *key, const nativeVal_t *x) {
	PyObject *o = Native_ToPy(v), *k = o ? Native_ToPy(key) : NULL, *px = k ? Native_ToPy(x) : NULL;
	byte ok = px && PyObject_SetItem(o, k, px) == 0;
	if (ok) {
		Native_Free(v);
		Native_Arg(o, v);
	}
	Py_XDECREF(px);
	Py_XDECREF(k);
	Py_XDECREF(o);
	return ok;
}

/* the player's ship of single player is lost: its hangar marked destroyed, the emergency ship next to Sputnik */
static byte Veh_LosePlayerShip(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *hangar = Mode_V(&t), *text = Mode_V(&t), *lost = Mode_V(&t), *rep = Mode_V(&t), *ship = Mode_V(&t);
	nativeVal_t *spt = Mode_V(&t), *at = Mode_V(&t), *plate = Mode_V(&t), *none = Mode_V(&t);
	if (!Mode_Import("ShipEdit") || !Mode_Import("HangarTab") || !Mode_Import("Sputnik") || !Mode_Import("Parking") ||
		!Mode_SaveInt("Hangar.iHangarShip", hangar)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Hangar.DestroyedShips", text);
	if (!Native_EvalText(VEH, text, lost) || !Veh_SetItem(lost, hangar, Mode_I(&t, 1)) || !Veh_SetItem(lost, Mode_I(&t, 9), Mode_I(&t, 0))) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(lost, rep);
	if (!Mode_SetSaveVar("Hangar.DestroyedShips", rep) || !Mode_SetSaveVar("Hangar.iHangarShip", Mode_S(&t, "9")) ||
		!LvN_ModGet("Parking", "PlayerShip", ship) || !Mode_VDel(ship)) {
		return Mode_Done(&t, 0);
	}
	Mode_SetModVar("Parking", "PlayerShip", none);
	Native_Free(ship);
	return Mode_Done(&t, LvN_ModGet("Sputnik", "spt", spt) && Mode_VObj(spt, "Pos", at) &&
		PORT_CALL(Parking, CreatePlayerShip, NULL, "v", at) && LvN_ModGet("Parking", "PlayerShip", ship) &&
		LvN_SetI(ship, "Active", 0) && LvN_ModGet("Parking", "DtritusPlate", plate) && LvN_SetI(plate, "Active", 0));
}

/* the attacker of FightingTraffic goes for the coin the dead ship drops */
static byte Veh_CoinForAttacker(const nativeVal_t *v, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *attacker = Mode_V(&t), *list = Mode_V(&t), *pos = Mode_V(&t), *limit = Mode_V(&t), *e = Mode_V(&t);
	nativeVal_t *epos = Mode_V(&t), *mt = Mode_V(&t), *at = Mode_V(&t), *args = Mode_V(&t), *xyz = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	if (!Mode_EntitySeam(winner, attacker)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(attacker)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VObj(attacker, "EntityList", list)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_IsStr(list, "FightingTraffic")) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Import("MisItems") || !Mode_VObj(v, "Pos", pos) || !Mode_Unpack(pos, 3, xyz) || !Native_GVarGet("AtmosphereLimit", limit)) {
		return Mode_Done(&t, 0);
	}
	int high = Mode_Cmp(&xyz[1], ">=", limit);
	if (high < 0) {
		return Mode_Done(&t, 0);
	}
	if (high) {
		if (!Mode_EntitySeam(winner, e)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(e)) {
			Native_Free(&xyz[1]);
			if (!Mode_VObj(e, "Pos", epos) || !LvN_Item(epos, 1, &xyz[1])) {
				return Mode_Done(&t, 0);
			}
		}
	}
	if (!LvN_ModGet("MisItems", "MONEY_APPEARS_TIME", mt) || !Native_ToFloat(mt, at)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(args, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		Native_Copy(Native_Append(args), &xyz[i]);
	}
	const nativeVal_t params[2] = { *winner, *args };
	return Mode_Done(&t, Mode_LaterIn(at->f, VEH, "GetCoin", params, 2, NULL));
}

/* Scrap.EntityListGet("FightingTraffic", "First") through its seam (OBJLIB 7: the C port called it by name) */
static byte Veh_FirstHunter(nativeVal_t *out) {
	PyObject *hook;
	int seam = Native_Seam("Scrap", "EntityListGet", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, out, "ss", "FightingTraffic", "First");
	}
	return Mode_ListGet("FightingTraffic", "First", out);
}

/* the other FightingTraffic ships after LooserName go back to Traffic */
static byte Veh_FreeHunters(const nativeVal_t *looser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t), *next = Mode_V(&t), *ename = Mode_V(&t), *target = Mode_V(&t);
	if (!Veh_FirstHunter(name) || !Mode_EntitySeam(name, e)) {
		return Mode_Done(&t, 0);
	}
	while (Native_True(e)) {
		if (!Mode_VObjRead(e, "NextInList", next) || !Mode_VObjRead(e, "Name", ename)) {
			return Mode_Done(&t, 0);
		}
		int other = Mode_Cmp(ename, "!=", winner);
		if (other > 0) {
			other = Mode_VObjRead(e, "AI_Target", target) ? Mode_Cmp(target, "==", looser) : -1;
		}
		if (other < 0 || (other && (!LvN_SetS(e, "EntityList", "Traffic") || !Mode_VObjRead(e, "Name", ename) ||
			!PORT_CALL(SAI, SetStateVehicle, NULL, "iv", 3L, ename)))) {
			return Mode_Done(&t, 0);
		}
		Native_Free(e);
		if (!Mode_EntitySeam(next, e)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* the death taunt of the player's victim (half of the time unless ForceDeathTaunt) */
static byte Veh_Taunt(const nativeVal_t *v, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *force = Mode_V(&t), *usr = Mode_V(&t), *n = Mode_V(&t), *mpil = Mode_V(&t), *pilot = Mode_V(&t);
	nativeVal_t *text = Mode_V(&t), *num = Mode_V(&t), *rep = Mode_V(&t), *at = Mode_V(&t);
	int go = Scrap_RandRange(0.0, 1.0) > 0.5;
	if (!go) {
		if (!Mode_G(VEH, "ForceDeathTaunt", force)) {
			return Mode_Done(&t, 0);
		}
		go = Native_True(force);
	}
	if (!go) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_UsrHandle(usr)) {
		return Mode_Done(&t, 0);
	}
	int mine = Native_True(usr);
	if (mine) {
		mine = Mode_VObj(usr, "Name", n) ? Mode_Cmp(winner, "==", n) : -1;
	}
	if (mine <= 0) {
		return Mode_Done(&t, mine == 0);
	}
	if (!Mode_VObj(v, "MPil0", mpil) || !Native_Slice(mpil, 2, 0x7fffffff, pilot)) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(pilot, "None") || Native_IsStr(pilot, "")) {
		return Mode_Done(&t, 1);
	}
	const nativeVal_t *head = pilot;
	if (!Mode_Import("SLogic")) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(pilot, "Police")) {
		Native_SetInt(num, (long)(int)Scrap_RandRange(0.0, 2.9) + 13);
		Mode_Repr(num, rep);
		if (!Mode_Cat(&t, text, 2, Mode_S(&t, "Generics_PoliceTaunt_"), rep)) {
			return Mode_Done(&t, 0);
		}
	} else {
		if (!PORT_CALL(string, find, at, "vs", pilot, "Merc")) {
			return Mode_Done(&t, 0);
		}
		int merc = Mode_CmpI(at, "!=", -1);
		if (merc < 0 || !Mode_Cat(&t, text, 3, Mode_S(&t, "Generics_"), merc ? Mode_S(&t, "Mercs") : pilot, Mode_S(&t, "Taunt_8"))) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, PORT_CALL(Speech, RemoteComunication, NULL, "vvi", text, head, -3L) &&
		PORT_CALL(SLogic, UpdateTauntEndTime, NULL, ""));
}

/* the single player part of OnDeath */
static byte Veh_DeathSingle(const nativeVal_t *v, const nativeVal_t *looser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *taunt = Mode_V(&t), *list = Mode_V(&t), *parked = Mode_V(&t), *ship = Mode_V(&t), *sname = Mode_V(&t);
	nativeVal_t *vname = Mode_V(&t), *del = Mode_V(&t);
	if (!LvN_ModGet("Speech", "LaunchedTauntEntityName", taunt)) {
		return Mode_Done(&t, 0);
	}
	int same = Mode_Cmp(looser, "==", taunt);
	if (same < 0 || (same && (!Mode_GVarSetI("RemainTalkTime", 0) || !PORT_CALL(Speech, AbortSpeech, NULL, "")))) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_VObj(v, "EntityList", list)) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(list, "Players")) {
		if (!Mode_PrintV(Mode_S(&t, "GAME OVER")) || !PORT_CALL(Parking, SetDtritusPlate, NULL, "vi", v, 0L)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!PORT_CALL(Parking, ParkedDeat, parked, "vv", v, winner) ||
		(!Native_True(parked) && (!Mode_Import("MisItems") || !PORT_CALL(MisItems, OnVehicleDeat, NULL, "v", v)))) {
		return Mode_Done(&t, 0);
	}
	if (!LvN_ModGet("Parking", "PlayerShip", ship) || !Mode_VObj(ship, "Name", sname) || !Mode_VObj(v, "Name", vname)) {
		return Mode_Done(&t, 0);
	}
	int mine = Mode_Cmp(sname, "==", vname);
	if (mine > 0) {
		mine = Mode_G(VEH, "DelPlayerShipOnDeath", del) ? Native_True(del) : -1;
	}
	return Mode_Done(&t, mine >= 0 && (!mine || Veh_LosePlayerShip()) && Veh_CoinForAttacker(v, winner) &&
		Veh_FreeHunters(looser, winner) && Veh_Taunt(v, winner));
}

/* the lost engines of each of the four engine kinds (NEng<k> of model MEng<k>, at PEng<k><A..>, AEng<k><A..>) */
static byte Veh_Motors(const nativeVal_t *v) {
	for (int k = 0; k < 4; k++) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *num = Mode_V(&t), *model = Mode_V(&t);
		char var[0x10];
		long count;
		Str_Printf(var, sizeof(var), "NEng%d", k);
		if (!Mode_VObj(v, var, num)) {
			return Mode_Done(&t, 0);
		}
		int any = Mode_CmpI(num, ">", 0);
		if (any <= 0) {
			if (any < 0) {
				return Mode_Done(&t, 0);
			}
			Mode_Done(&t, 1);
			continue;
		}
		Str_Printf(var, sizeof(var), "MEng%d", k);
		if (!Mode_VObj(v, var, model) || !Mode_Range(num, &count)) {
			return Mode_Done(&t, 0);
		}
		for (long i = 0; i < count; i++) {
			int mark = t.n;
			nativeVal_t *pos = Mode_V(&t), *ang = Mode_V(&t), *oname = Mode_V(&t), *l = Mode_V(&t), *off = Mode_V(&t);
			nativeVal_t *life = Mode_V(&t), *lname = Mode_V(&t), *args = Mode_V(&t);
			if (i >= 26) {
				return Mode_Done(&t, Native_Raise(NE_INDEX, "string index out of range"));
			}
			Str_Printf(var, sizeof(var), "PEng%d%c", k, (int)('A' + i));
			if (!Mode_VObj(v, var, pos)) {
				return Mode_Done(&t, 0);
			}
			Str_Printf(var, sizeof(var), "AEng%d%c", k, (int)('A' + i));
			if (!Mode_VObj(v, var, ang) || !PORT_CALL(SNet, GetObjName, oname, "") ||
				!PORT_OWN(Vehicles, CreateDumbMotor, l, "vvvv", oname, pos, ang, model) || !LvN_SetS(l, "OnHit", "Vehicles.ServerMotorHit") ||
				!LvN_SetS(l, "HitType", "None") || !LvN_ModGet("Init", "OffLineGame", off)) {
				return Mode_Done(&t, 0);
			}
			if (Native_True(off)) {
				Mode_RandF(2.2, 5.2, life);
			} else {
				Mode_RandF(4.0, 4.5, life);
			}
			Native_SetEmpty(args, NV_TUPLE);
			Native_Copy(Native_Append(args), l);
			Native_SetStr(Native_Append(args), "Hit");
			if (!Mode_VSet(l, "LifeTime", life) || !Mode_LaterIn(0.3, VEH, "ActivateHitType", args->items, 2, NULL) ||
				!Mode_VObj(l, "Name", lname) || !PORT_OWN(Vehicles, SetUpDumbMotor, NULL, "vv", lname, model)) {
				return Mode_Done(&t, 0);
			}
			Mode_Release(&t, mark);
		}
		Mode_Done(&t, 1);
	}
	return 1;
}

/* OnDeath(LooserName, WinnerName): the effects, the single player consequences, the items and the lost engines */
static byte Veh_OnDeathN(const nativeVal_t *looser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *x = Mode_V(&t), *split = Mode_V(&t), *cb = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Import("Init") || !Mode_Import("Parking") || !Mode_Import("Speech") || !Mode_EntitySeam(looser, v) ||
		!Mode_VObjRead(v, "Pos", x) || !Mode_VObjRead(v, "Ang", x) || !Mode_VObjRead(v, "Vel", x) || !Mode_VObjRead(v, "Model", x) ||
		!PORT_OWN(Vehicles, OnClientDeath, NULL, "vv", looser, winner)) {
		return Mode_Done(&t, 0);
	}
	int single = Native_NetOff();
	if (single > 0) {
		single = Native_GVarGet("SplitScreen", split) ? LvN_EqI(split, 0) : -1;
	}
	if (single < 0 || (single && !Veh_DeathSingle(v, looser, winner)) || !Mode_Import("Items") ||
		!PORT_CALL(Items, DropAll, NULL, "v", looser) || !Veh_Motors(v) || !Mode_G(VEH, "OnDeathCallback", cb)) {
		return Mode_Done(&t, 0);
	}
	const nativeVal_t params[2] = { *looser, *winner };
	return Mode_Done(&t, !Native_True(cb) || Mode_CallArgs(cb, NULL, params, 2));
}

/* ---- cameras ---- */

/* SwitchView(CamName, idControl): the HUD of the racer or walker, and who the player controls */
static byte Veh_SwitchViewN(const nativeVal_t *camName, const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cam = Mode_V(&t), *main = Mode_V(&t), *ply = Mode_V(&t), *tg = Mode_V(&t), *m = Mode_V(&t);
	int on = Mode_CmpI(id, ">=", 0);
	if (on <= 0) {
		return Mode_Done(&t, on == 0);
	}
	if ((Mode_ActionSetIs("Racer") || Mode_ActionSetIs("Walk")) &&
		(!Mode_Import("RacerScorer") || !PORT_CALL(RacerScorer, Create, NULL, "v", id))) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_EntitySeam(camName, cam) || !Mode_VObj(cam, "MainTarget", main) || !Mode_EntitySeam(main, ply) || !Mode_VObj(cam, "Target", tg) ||
		!Mode_VObj(cam, "MainTarget", m)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_VSet(ply, "UsrControl", !Native_Compare(tg, m) ? id : Mode_I(&t, -1)));
}

/* SetActiveCam(ply, cam, id): camera cam of player id follows ply */
static byte Veh_SetActiveCamN(const nativeVal_t *plyName, const nativeVal_t *camName, const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cam = Mode_V(&t), *ply = Mode_V(&t), *cname = Mode_V(&t), *pname = Mode_V(&t);
	return Mode_Done(&t, Mode_EntitySeam(camName, cam) && Mode_EntitySeam(plyName, ply) && Mode_VObj(cam, "Name", cname) &&
		PORT_CALL(Scrap, SetCam, NULL, "vv", id, cname) && LvN_SetS(cam, "OnSwitch", "Vehicles.SwitchView") &&
		Mode_VObj(ply, "Name", pname) && Mode_VSet(cam, "MainTarget", pname) && Mode_VObjRead(ply, "Name", pname) &&
		Mode_VSet(cam, "Target", pname) && LvN_SetI(cam, "FreeCam", 0));
}

/* ---- respawn and set up ---- */

/* v.<prefix>0<d> = value for d in range(count) (the exec lines of ReSpawnShip) */
static byte Veh_SetEach(const nativeVal_t *v, const char *prefix, long d, const nativeVal_t *value) {
	char var[0x20];
	Str_Printf(var, sizeof(var), "%s0%ld", prefix, d);
	return Mode_VSet(v, var, value);
}

/* the ammo (an eighth of the most), and online the upgrades cleared, of a ship coming back */
static byte Veh_Restock(const nativeVal_t *v) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *na = Mode_V(&t), *max = Mode_V(&t), *part = Mode_V(&t), *i = Mode_V(&t), *off = Mode_V(&t), *nw = Mode_V(&t);
	long n;
	if (!PORT_CALL(SWeap, GetNAmmo, na, "") || !Mode_Range(na, &n)) {
		return Mode_Done(&t, 0);
	}
	for (long d = 0; d < n; d++) {
		Native_Free(max);
		Native_Free(part);
		Native_Free(i);
		if (!PORT_CALL(SWeap, GetFAmmo, max, "is", d, "Max") || !Mode_Arith('/', max, Mode_I(&t, 8), part) ||
			!Native_ToInt(part, i) || !Veh_SetEach(v, "Ammo", d, i)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!LvN_ModGet("Init", "OffLineGame", off)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(off)) {
		if (!PORT_CALL(SWeap, GetNWeap, nw, "") || !Mode_Range(nw, &n)) {
			return Mode_Done(&t, 0);
		}
		for (long d = 0; d < n; d++) {
			if (!Veh_SetEach(v, "Upgrd", d, Mode_I(&t, 0)) || !Veh_SetEach(v, "UAmmo", d, Mode_I(&t, 0))) {
				return Mode_Done(&t, 0);
			}
		}
	}
	return Mode_Done(&t, 1);
}

/* ReSpawnShip(EntityName): the ship back to life (a client's net ship only gets its death function), boost, fire and
 * weapons reset, the laser selected */
static byte Veh_ReSpawnShipN(const nativeVal_t *name) {
	static const struct {
		const char *var;
		long value;
	} reset[] = { { "isBoost", 1 }, { "BoostDeact", 0 }, { "MotorDCnt", 0 }, { "EMICounter", 0 }, { "FireRemainTime", 0 },
		{ "FireDelayTime", 1 }, { "FireStamp", 0 }, { "CMStamp", 0 }, { "WeapA06", 1 }, { "AcWeap", 6 } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *flags = Mode_V(&t), *f0 = Mode_V(&t), *head = Mode_V(&t), *life = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Import("Init") || !Mode_Import("Parking") || !Mode_EntitySeam(name, v) || !Mode_NetFlags(flags) ||
		!LvN_Item(flags, 0, f0)) {
		return Mode_Done(&t, 0);
	}
	int client = Native_True(f0);
	if (client) {
		if (!Native_Slice(name, 0, 3, head)) {
			return Mode_Done(&t, 0);
		}
		client = Native_IsStr(head, "net");
	}
	if (client) {
		if (!LvN_SetS(v, "OnDeath", "Vehicles.OnClientDeath")) {
			return Mode_Done(&t, 0);
		}
	} else if (!LvN_SetS(v, "OnDeath", "Vehicles.OnDeath") || !LvN_SetI(v, "Active", 1) || !Mode_VObj(v, "MaxLife", life) ||
		!Mode_VSet(v, "Life", life) || !LvN_SetInts3(v, "Vel", 0, 0, 0) || !LvN_SetI(v, "ForceAll", 1) || !Veh_Restock(v) ||
		!PORT_CALL(Parking, SetDtritusPlate, NULL, "vi", v, 1L)) {
		return Mode_Done(&t, 0);
	}
	for (size_t i = 0; i < sizeof(reset) / sizeof(reset[0]); i++) {
		if (!LvN_SetI(v, reset[i].var, reset[i].value)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* SetProperties(EntityName, Model, MaxLife, Pilot, Motor0..3, WeapList, PlayerName, localedit=0): the ship's model
 * and parts (not on a net client), then the parameters of ShipMaker; the handle, None for a bad configuration. The
 * arguments by slot (a[10] NULL when localedit is not given) */
static byte Veh_SetPropertiesN(const nativeVal_t *const *a, nativeVal_t *out) {
	static const char *const parts[] = { "MEng0", "MEng1", "MEng2", "MEng3" };
	const nativeVal_t *name = a[0], *model = a[1], *life = a[2], *pilot = a[3], *weap = a[8], *player = a[9];
	const nativeVal_t *local = a[10];
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *flags = Mode_V(&t), *f0 = Mode_V(&t), *cbs = Mode_V(&t), *entry = Mode_V(&t);
	nativeVal_t *fn = Mode_V(&t), *valid = Mode_V(&t), *msg = Mode_V(&t);
	if (!Mode_Import("ShipMaker") || !Mode_Import("OutSound") || !Mode_EntitySeam(name, v) || !LvN_SetI(v, "isInit", 1) ||
		!LvN_SetF(v, "FxEngineTime", 0.5) || !LvN_SetF(v, "FxBoostEngineTime", 0.25) || !Mode_NetFlags(flags)) {
		return Mode_Done(&t, 0);
	}
	int edit = local ? Native_True(local) : 0;
	if (!edit) {
		if (!LvN_Item(flags, 0, f0)) {
			return Mode_Done(&t, 0);
		}
		edit = !Native_True(f0);
	}
	if (edit) {
		if (!LvN_ModGet("ShipMaker", "ShipCallback", cbs)) {
			return Mode_Done(&t, 0);
		}
		int own = Mode_HasKey("ShipMaker", "ShipCallback", 0, model);
		if (own < 0) {
			return Mode_Done(&t, 0);
		}
		if (own) {
			const nativeVal_t params[2] = { *v, *model };
			if (!Mode_Sub(cbs, model, entry) || !LvN_Item(entry, 0, fn) || !Mode_CallArgs(fn, NULL, params, 2)) {
				return Mode_Done(&t, 0);
			}
		} else if (!Mode_VModel(v, model)) {
			return Mode_Done(&t, 0);
		}
		if (!Mode_VSet(v, "WeapList", weap)) {
			return Mode_Done(&t, 0);
		}
		for (int i = 0; i < 4; i++) {
			if (!Mode_VSet(v, parts[i], a[4 + i])) {
				return Mode_Done(&t, 0);
			}
		}
		if (!Mode_VSet(v, "Descriptor", player) || !Mode_VSet(v, "MPil0", pilot) || !Mode_VSet(v, "MaxLife", life)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!PORT_CALL(ShipMaker, SetShipParams, valid, "v", name)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(valid)) {
		if (!PORT_OWN(Vehicles, ReSpawnShip, NULL, "v", name) || !PORT_CALL(OutSound, SetVehicleSounds, NULL, "v", name)) {
			return Mode_Done(&t, 0);
		}
		Native_Copy(out, v);
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, Mode_Cat(&t, msg, 3, Mode_S(&t, "H E R E T I C    E R R O R : "), name,
		Mode_S(&t, " is out of configuration.\n")) && Mode_ConsoleError(msg));
}

/* SetStdProperties(EntityName, Model, Pilot): SetProperties from ShipMaker.StdShips of the model */
static byte Veh_SetStdPropertiesN(const nativeVal_t *name, const nativeVal_t *model, const nativeVal_t *pilot) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *std = Mode_V(&t), *up = Mode_V(&t), *a = Mode_V(&t), *key = Mode_V(&t), *title = Mode_V(&t), *it = Mode_V(&t);
	for (int i = 1; i < 7; i++) {
		Mode_V(&t);
	}
	if (!Mode_Import("ShipMaker") || !Mode_Import("string") || !LvN_ModGet("ShipMaker", "StdShips", std) || !Native_Upper(model, up) ||
		!Mode_Sub(std, up, a)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 7; i++) {
		if (!LvN_Item(a, i, &it[i])) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(up);
	return Mode_Done(&t, Native_Upper(model, up) && Mode_Cat(&t, key, 2, Mode_S(&t, "SHIP_"), up) && Mode_LangV(key, title) &&
		PORT_OWN(Vehicles, SetProperties, NULL, "vvvvvvvvvv", name, &it[0], &it[6], pilot, &it[1], &it[2], &it[3], &it[4], &it[5], title));
}

/* Init(): the net chain, then ShipMaker and OutSound */
static byte Veh_InitN(void) {
	/* OBJLIB 5: the chains of Net's functions stay Python functions until they are C hook pointers */
	if (!Mode_Import("Net") || !Script_Hook(VEH, "Net", "ClientAfterCreate", "LastClientAfterCreate") ||
		!Script_Hook(VEH, "Net", "ClientCreate", "LastClientCreate") || !Mode_Import("ShipMaker") ||
		!PORT_CALL(ShipMaker, Init, NULL, "") || !Mode_Import("OutSound") || !PORT_CALL(OutSound, Init, NULL, "")) {
		return 0;
	}
	return Mode_PrintS("Vehicle.Init() executed.");
}

/* SetProperties takes localedit as a keyword */
static byte Vehicles_SetProperties_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "OOOOOOOOOO|O", Veh_SetPropertiesN, out);
}

static char *Vehicles_SetProperties_N[] = { "EntityName", "Model", "MaxLife", "Pilot", "Motor0", "Motor1", "Motor2", "Motor3", "WeapList",
		"PlayerName", "localedit", NULL };

static PyObject *Vehicles_SetProperties(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Mode_KwCall(args, kw, "OOOOOOOOOO|O", Vehicles_SetProperties_N, Veh_SetPropertiesN);
}

MODE_FN(Vehicles, ClientCreate, 2, Veh_ClientCreateN(&a[0], &a[1]))
MODE_FN(Vehicles, ClientAfterCreate, 2, Veh_ClientAfterCreateN(&a[0], &a[1]))
MODE_FN(Vehicles, ServerMotorHit, 3, Veh_ServerMotorHitN(&a[0], &a[1], &a[2]))
MODE_FN(Vehicles, MotorHit, 3, Veh_MotorHitN(&a[0], &a[2], out))
MODE_FN(Vehicles, CreateDumbMotor, 4, Veh_CreateDumbMotorN(&a[0], &a[1], &a[2], &a[3], out))
MODE_FN(Vehicles, SetUpDumbMotor, 2, Veh_SetUpDumbMotorN(&a[0], &a[1], out))
MODE_FN(Vehicles, DumbShipHit, 3, Veh_DumbShipHitN(&a[0], out))
MODE_FN(Vehicles, OnClientDeath, 2, Veh_OnClientDeathN(&a[0]))
MODE_FN(Vehicles, ActivateHitType, 2, Mode_VSet(&a[0], "HitType", &a[1]))
MODE_FN(Vehicles, GetCoin, 2, Veh_GetCoinN(&a[0], &a[1]))
MODE_FN(Vehicles, OnDeath, 2, Veh_OnDeathN(&a[0], &a[1]))
MODE_FN(Vehicles, SwitchView, 2, Veh_SwitchViewN(&a[0], &a[1]))
MODE_FN(Vehicles, SetActiveCam, 3, Veh_SetActiveCamN(&a[0], &a[1], &a[2]))
MODE_FN(Vehicles, ReSpawnShip, 1, Veh_ReSpawnShipN(&a[0]))
MODE_FN(Vehicles, SetStdProperties, 3, Veh_SetStdPropertiesN(&a[0], &a[1], &a[2]))
MODE_FN(Vehicles, Init, 0, Veh_InitN())

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

PORTS(Vehicles)
