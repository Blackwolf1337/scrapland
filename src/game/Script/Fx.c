/*
 * The Fx module in C (scripts/fx/fx.py, issue #2): the effect files a level loads and the motor effects of a ship
 * model are data (data/fx/fx.def); here the conditions the file cannot write, the motor class of an engine name, the
 * scale of the indoor traffic and the money transfer. The constants FX... and the file names stay variables of the
 * module, other scripts and the file read them.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the PyMethodDef
 * entries are adapters. The motor classes FXMOTOR_* that GetMotorClass gives are mirrored from the module dict
 * (OBJLIB 5): fx.mod sets them, other scripts read them there. Init.isJunkYard and Init.isShipEdit stay by name.
 */
#include <string.h>

#include "scrap/fx.h"
#include "scrap/pyentity.h"
#include "scrap/script.h"
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

static struct {
	nativeVal_t motor[FX_MOTORS];	/* FXMOTOR_<name> */
} fx;

/* `import name` of a function of the script: 0 with the error set */
static byte Fx_Import(const char *name) {
	/* OBJLIB 6: module loading stays with the module objects until then */
	PyObject *m = PyImport_ImportModule((char *)name);
	Py_XDECREF(m);
	return m != NULL;
}

/* v == 0 of the script (a number equal to 0; a text is never equal) */
static byte Fx_IsZero(const nativeVal_t *v) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	return Native_Equal(v, &zero);
}

/* Init(): the effect files of the level; $dudv and $shipedit are the two conditions with an "or" of the script */
static byte Fx_Init(void) {
	nativeVal_t xbox = { 0 }, dudvRT = { 0 };
	PyObject *junk = NULL, *edit = NULL;
	int off = -1;
	if (Native_GVarGet("isXbox", &xbox) && Native_GVarGet("R_SceneDuDvRTarget", &dudvRT)) {
		/* OBJLIB 5: variables of Init */
		junk = Script_Global("Init", "isJunkYard");
		edit = junk ? Script_Global("Init", "isShipEdit") : NULL;
		off = edit ? Native_NetOff() : -1;
	}
	int a = Fx_IsZero(&xbox) || !Fx_IsZero(&dudvRT);
	Native_Free(&xbox);
	Native_Free(&dudvRT);
	int b = off >= 0 ? Script_True(junk) : -1, c = b > 0 ? 1 : b == 0 ? Script_True(edit) : -1;
	Py_XDECREF(junk);
	Py_XDECREF(edit);
	if (b < 0 || c < 0) {
		return 0;
	}
	/* OBJLIB 6: the .def runner takes its $names as a dict until it takes C values */
	PyObject *vars = Py_BuildValue("{s:i,s:i}", "dudv", a, "shipedit", b || c || !off);
	if (!vars) {
		return 0;
	}
	Script_RunFile(0, "fx/fx.def", "Init", fx_def, fx_steps, vars);
	Py_DECREF(vars);
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
	/* OBJLIB 5: the variable from the module dict */
	const nativeVar_t vars[] = { { var, &fx.motor[k], 0, NV_ANY }, { NULL, NULL, 0, 0 } };
	if (!Native_ImportGlobals("Fx", vars)) {
		return 0;
	}
	*out = fx.motor[k];
	return 1;
}

/* v * 0.6 of the script on a variable of the entity */
static byte Fx_Scale(const nativeVal_t *v, nativeVal_t *out) {
	switch (v->type) {
	case NV_INT:
	case NV_LONG:
	case NV_FLOAT: {
		double d;
		Native_ArgDouble(v, &d);
		Native_SetFloat(out, d * 0.6);
		return 1;
	}
	case NV_STR:
	case NV_VEC:
	case NV_LIST:
	case NV_TUPLE:
		return Native_Raise(NE_TYPE, "can't multiply sequence with non-int");
	}
	return Native_Raise(NE_TYPE, "bad operand type(s) for *");
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
	/* OBJLIB 6: the .def runner takes its $names as a dict until it takes C values */
	PyObject *m = ok ? Native_ToPy(&up) : NULL, *n = m ? Native_ToPy(&name) : NULL;
	PyObject *vars = n ? Py_BuildValue("{s:O,s:O}", "model", m, "name", n) : NULL;
	Py_XDECREF(m);
	Py_XDECREF(n);
	Native_Free(&up);
	Native_Free(&name);
	if (!vars) {
		return 0;
	}
	Script_RunFile(0, "fx/fx.def", "Motors", fx_def, fx_steps, vars);
	Py_DECREF(vars);
	if (indoor) {
		nativeVal_t scale, v;
		return Native_EntVar(ref->link.target, ref->name, "ShipMotorScale", &scale) && Fx_Scale(&scale, &v) &&
			Native_EntSet(ref->link.target, ref->name, "ShipMotorScale", &v);
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

/* ---- the adapters of the module table (OBJLIB 7: arguments, the mirror and the errors of the Python calls) ---- */

static PyObject *Fx_InitPy(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Fx_Import("Init") && Fx_Init() ? Py_ReturnNone() : NULL;
}

static PyObject *Fx_GetMotorClass(PyObject *self, PyObject *args) {
	PyObject *name;
	nativeVal_t n, r;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name) || !Fx_Import("string")) {
		return NULL;
	}
	if (!Native_FromPyAny(name, &n)) {
		/* the error of string.upper on a value that has no native form */
		return Native_Raise(NE_TYPE, "read-only character buffer, %.50s", name->ob_type->tp_name), NULL;
	}
	byte ok = Fx_MotorClass(&n, &r);
	Native_Free(&n);
	return ok ? Native_ToPy(&r) : NULL;
}

static PyObject *Fx_SetUpShipMotors(PyObject *self, PyObject *args) {
	PyObject *e;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e) || !Fx_Import("string")) {
		return NULL;
	}
	if (!PyEntity_Check(e)) {
		/* e.Model of another value: its error (an object that has a Model is no ship) */
		PyObject *m = Script_GetAttr(e, "Model");
		if (m) {
			Py_DECREF(m);
			Native_Raise(NE_TYPE, "Fx.SetUpShipMotors: not an entity");
		}
		return NULL;
	}
	return Fx_Motors(((cPyEntity *)e)->ref) ? Py_ReturnNone() : NULL;
}

static PyObject *Fx_MoneyTransferPy(PyObject *self, PyObject *args) {
	PyObject *o[3];
	nativeVal_t v[3] = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &o[0], &o[1], &o[2])) {
		return NULL;
	}
	byte ok = 1;
	for (int i = 0; ok && i < 3; i++) {
		if (!Native_FromPyAny(o[i], &v[i])) {
			/* a value without a native form: the error of SFX.MoneyTransfer for it */
			ok = i < 2 ? Native_Raise(NE_TYPE, "argument %d: expected string, %.50s found", i + 1, o[i]->ob_type->tp_name) :
				Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
		}
	}
	ok = ok && Fx_MoneyTransfer(&v[0], &v[1], &v[2]);
	for (int i = 0; i < 3; i++) {
		Native_Free(&v[i]);
	}
	return ok ? Py_ReturnNone() : NULL;
}

PyMethodDef g_FxMethods[] = {
	{ "Init", Fx_InitPy, 1, NULL },
	{ "GetMotorClass", Fx_GetMotorClass, 1, NULL },
	{ "SetUpShipMotors", Fx_SetUpShipMotors, 1, NULL },
	{ "MoneyTransfer", Fx_MoneyTransferPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
