/*
 * The Fx module in C (scripts/fx/fx.py, issue #2): the effect files a level loads and the motor effects of a ship
 * model are data (data/fx/fx.def); here the conditions the file cannot write, the motor class of an engine name, the
 * scale of the indoor traffic and the money transfer. The constants FX... and the file names stay variables of the
 * module, other scripts and the file read them.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the function table
 * rows name the entries. The motor classes FXMOTOR_* that GetMotorClass gives are variables of the module (fx.mod sets
 * them, other scripts read them), read from the registry at each call. Init.isJunkYard and Init.isShipEdit stay by
 * name.
 */
#include <string.h>

#include "scrap/fx.h"
#include "scrap/pyentity.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"

static const char fx_def[] = {
#embed "../../../data/fx/fx.def"
	, 0 };

static const scriptStep_t fx_steps[] = {
	{ "print", Script_StepPrint },
	{ NULL, NULL },
};

/* the engines with a motor class of their own; FXMOTOR_NONE for the others */
static const char *const fx_motors[] = { "NONE", "LIFEBOAT", "MOSQUITO", "MPOLI1", "MPOLI2", "MPOLI3", "MPOLI4",
	"MPOLI5", "MPOLIBOSS1", "MBANKER1", "MBANKMASTER1", "MMERC1", "MMERC2", "MMERC3", "MMAYOR1", "MBISHOP1",
	"MARCHBISHOP1", "MFUNC1", "MBERTO1", "MHUMP1", "MBETTY1", "MBOSS1" };
#define FX_MOTORS ((int)(sizeof(fx_motors) / sizeof(fx_motors[0])))

/* v == 0 of the script (cmp of 1.5.2: a number equal to 0; a text is never equal) */
static byte Fx_IsZero(const nativeVal_t *v) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Native_Compare(v, &zero) == 0;
}

/* Init(): the effect files of the level; $dudv and $shipedit are the two conditions with an "or" of the script */
static byte Fx_Init(void) {
	nativeVal_t xbox = { 0 }, dudvRT = { 0 }, junk = { 0 }, edit = { 0 }, v[2];
	int off = -1;
	if (Native_GVarGet("isXbox", &xbox) && Native_GVarGet("R_SceneDuDvRTarget", &dudvRT)) {
		/* variables of Init (any value: the truth of an object is its own) */
		off = Mode_G("Init", "isJunkYard", &junk) && Mode_G("Init", "isShipEdit", &edit) ? Native_NetOff() : -1;
	}
	int a = Fx_IsZero(&xbox) || !Fx_IsZero(&dudvRT);
	Native_Free(&xbox);
	Native_Free(&dudvRT);
	int b = off >= 0 ? Mode_Truth(&junk) : -1, c = b > 0 ? 1 : b == 0 ? Mode_Truth(&edit) : -1;
	Native_Free(&junk);
	Native_Free(&edit);
	if (b < 0 || c < 0) {
		return 0;
	}
	Native_SetInt(&v[0], a);
	Native_SetInt(&v[1], b || c || !off);
	Mode_DefRun(0, "fx/fx.def", "Init", fx_def, fx_steps, 0, 2, "dudv", &v[0], "shipedit", &v[1]);
	return 1;
}

/* GetMotorClass(name): the motor class of the engine name (FXMOTOR_<NAME> of the module, FXMOTOR_NONE for others);
 * 1 with *out (Native_Free it) */
static byte Fx_MotorClass(const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t up;
	if (!Native_Upper(name, &up)) {
		return 0;
	}
	int k = 0;
	for (int i = 1; i < FX_MOTORS && !k; i++) {
		if (Native_IsStr(&up, fx_motors[i])) {
			k = i;
		}
	}
	Native_Free(&up);
	char var[0x20];
	Str_Printf(var, sizeof(var), "FXMOTOR_%s", fx_motors[k]);
	return Mode_G("Fx", var, out);
}

/* v * 0.6 of the script on a variable of the entity */
static byte Fx_Scale(const nativeVal_t *v, nativeVal_t *out) {
	nativeVal_t f;
	Native_SetFloat(&f, 0.6);
	return Native_Arith('*', v, &f, out);
}

/* SetUpShipMotors(e): the motor effects of the ship's model; the indoor traffic (a model ending in B) at 0.6. ref is
 * the link of the script's handle (its target is NULL once the entity is gone) */
static byte Fx_Motors(const cEntityRef *ref) {
	nativeVal_t model = { 0 }, up = { 0 }, name = { 0 };
	byte ok = Native_EntVar(ref->link.target, ref->name, "Model", &model) && Native_Upper(&model, &up);
	Native_Free(&model);
	/* up[-1] */
	if (ok && !up.n) {
		ok = Native_Raise(NE_INDEX, "string index out of range");
	}
	ok = ok && Native_EntVar(ref->link.target, ref->name, "Name", &name);
	byte indoor = ok && Native_Text(&up)[up.n - 1] == 'B';
	if (indoor) {
		/* up[:-1] */
		nativeVal_t cut = { 0 };
		Native_SetText(&cut, Native_Text(&up), up.n - 1);
		Native_Free(&up);
		up = cut;
	}
	if (ok) {
		Mode_DefRun(0, "fx/fx.def", "Motors", fx_def, fx_steps, 0, 2, "model", &up, "name", &name);
	}
	Native_Free(&up);
	Native_Free(&name);
	if (!ok) {
		return 0;
	}
	if (indoor) {
		nativeVal_t scale = { 0 }, v = { 0 };
		byte ok = Native_EntVar(ref->link.target, ref->name, "ShipMotorScale", &scale) && Fx_Scale(&scale, &v) &&
			Native_EntSet(ref->link.target, ref->name, "ShipMotorScale", &v);
		Native_Free(&scale);
		Native_Free(&v);
		return ok;
	}
	return 1;
}

/* MoneyTransfer(EntityFrom, EntityTo, Time): the effect of SFX.MoneyTransfer (two entities with life) */
static byte Fx_MoneyTransfer(const nativeVal_t *from, const nativeVal_t *to, const nativeVal_t *time) {
	const char *f, *t;
	double d;
	if (!Native_ArgStr(from, 1, &f) || !Native_ArgStr(to, 2, &t) || !Native_ArgDouble(time, &d)) {
		return 0;
	}
	cEntity *vFrom = World_FindEntity(f);
	CHECK(vFrom);
	CHECK(Entity_IsType(vFrom, TYEN_WITH_LIFE));
	cEntity *vTo = World_FindEntity(t);
	CHECK(vTo);
	CHECK(Entity_IsType(vTo, TYEN_WITH_LIFE));
	FX_MoneyTransfer(vFrom, vTo, (float)d);
	return 1;
}

/* ---- the entries of the module table (the mirror) ---- */

/* Init takes any arguments; its entry for the call of InitLevel through its ref (stage 3) */
MODE_FNANY(Fx, InitPy, Mode_Import("Init") && Fx_Init())

/* GetMotorClass(name): string.upper of a value without a native form raises its TypeError */
static byte Fx_MotorClassArg(const nativeVal_t *name, nativeVal_t *out) {
	if (!Mode_Import("string")) {
		return 0;
	}
	if (NV_REFS >> name->type & 1) {
		return Native_Raise(NE_TYPE, "read-only character buffer, %.50s", Native_TypeName(name));
	}
	return Fx_MotorClass(name, out);
}

/* SetUpShipMotors(e) of an entity handle; e.Model of another value: its error (an object that has a Model is no
 * ship) */
static byte Fx_MotorsArg(const nativeVal_t *e) {
	if (!Mode_Import("string")) {
		return 0;
	}
	if (e->type == NV_ENT) {
		return Fx_Motors(e->ent);
	}
	nativeVal_t m;
	if (Mode_VObj(e, "Model", &m)) {
		Native_Free(&m);
		Native_Raise(NE_TYPE, "Fx.SetUpShipMotors: not an entity");
	}
	return 0;
}

/* MoneyTransfer(from, to, amount): a value without a native form raises the error of SFX.MoneyTransfer for it */
static byte Fx_MoneyTransferArgs(const nativeVal_t *v) {
	for (int i = 0; i < 3; i++) {
		if (NV_REFS >> v[i].type & 1) {
			return i < 2 ? Native_Raise(NE_TYPE, "argument %d: expected string, %.50s found", i + 1, Native_TypeName(&v[i])) :
				Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
		}
	}
	return Fx_MoneyTransfer(&v[0], &v[1], &v[2]);
}

MODE_FN(Fx, GetMotorClass, 1, Fx_MotorClassArg(&a[0], out))
MODE_FN(Fx, SetUpShipMotors, 1, Fx_MotorsArg(&a[0]))
MODE_FN(Fx, MoneyTransferPy, 3, Fx_MoneyTransferArgs(a))

const portFn_t g_FxMethods[] = {
	PORT_FN(Init, Fx_InitPy),
	PORT_FN(GetMotorClass, Fx_GetMotorClass),
	PORT_FN(SetUpShipMotors, Fx_SetUpShipMotors),
	PORT_FN(MoneyTransfer, Fx_MoneyTransferPy),
	PORT_FNEND
};

PORTS(Fx)
