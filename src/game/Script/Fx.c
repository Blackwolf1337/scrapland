/*
 * The Fx module in C (scripts/fx/fx.py, issue #2): the effect files a level loads and the motor effects of a ship
 * model are data (data/fx/fx.def); here the conditions the file cannot write, the motor class of an engine name, the
 * scale of the indoor traffic and the money transfer. The constants FX... and the file names stay variables of the
 * module, other scripts and the file read them.
 */
#include <string.h>

#include "scrap/script.h"

static const char fx_def[] = {
#embed "../../../data/fx/fx.def"
	, 0 };

static const scriptStep_t fx_steps[] = {
	{ "print", Script_StepPrint },
	{ NULL, NULL },
};

/* Init(): the effect files of the level; $dudv and $shipedit are the two conditions with an "or" of the script */
static PyObject *Fx_Init(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	if (!Script_Import((const char *const[]){ "Init", NULL })) {
		return NULL;
	}
	PyObject *xbox = Script_Keep(&r, Script_Ask("Scrap", "Get", "(s)", "isXbox"));
	PyObject *dudvRT = xbox ? Script_Keep(&r, Script_Ask("Scrap", "Get", "(s)", "R_SceneDuDvRTarget")) : NULL;
	PyObject *junk = dudvRT ? Script_Keep(&r, Script_Global("Init", "isJunkYard")) : NULL;
	PyObject *edit = junk ? Script_Keep(&r, Script_Global("Init", "isShipEdit")) : NULL;
	PyObject *net = edit ? Script_Keep(&r, Script_Ask("Scrap", "GetNetFlags", "()")) : NULL;
	PyObject *off = net ? Script_Keep(&r, Py_BuildValue("(iii)", 0, 0, 0)) : NULL;
	if (!off) {
		return Script_Done(&r, NULL);
	}
	int a = Script_NumCmp(xbox, 0) == 0 || Script_NumCmp(dudvRT, 0) != 0;
	int b = Script_True(junk), c = b ? 1 : Script_True(edit), d = b || c > 0 ? 1 : !Script_Eq(net, off);
	if (b < 0 || c < 0 || d < 0) {
		return Script_Done(&r, NULL);
	}
	PyObject *vars = Script_Keep(&r, Py_BuildValue("{s:i,s:i}", "dudv", a, "shipedit", b || c || d));
	Script_RunFile(0, "fx/fx.def", "Init", fx_def, fx_steps, vars);
	return Script_Done(&r, Py_ReturnNone());
}

/* GetMotorClass(name): the FXMOTOR_ constant of an engine (FXMOTOR_NONE for others) */
static PyObject *Fx_GetMotorClass(PyObject *self, PyObject *args) {
	static const char *const motors[] = { "LIFEBOAT", "MOSQUITO", "MPOLI1", "MPOLI2", "MPOLI3", "MPOLI4", "MPOLI5",
		"MPOLIBOSS1", "MBANKER1", "MBANKMASTER1", "MMERC1", "MMERC2", "MMERC3", "MMAYOR1", "MBISHOP1", "MARCHBISHOP1",
		"MFUNC1", "MBERTO1", "MHUMP1", "MBETTY1", "MBOSS1" };
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name) || !Script_Import((const char *const[]){ "string", NULL })) {
		return NULL;
	}
	PyObject *up = Script_Ask("string", "upper", "(O)", name);
	if (!up) {
		return NULL;
	}
	char var[0x40];
	Str_Copy(var, "FXMOTOR_NONE", sizeof(var));
	for (size_t i = 0; i < sizeof(motors) / sizeof(motors[0]); i++) {
		if (Script_Is(up, motors[i])) {
			Str_Printf(var, sizeof(var), "FXMOTOR_%s", motors[i]);
			break;
		}
	}
	Py_DECREF(up);
	return Script_Global("Fx", var);
}

/* SetUpShipMotors(e): the motor effects of the ship's model; the indoor traffic (a model ending in B) at 0.6 */
static PyObject *Fx_SetUpShipMotors(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *e;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e) || !Script_Import((const char *const[]){ "string", NULL })) {
		return NULL;
	}
	PyObject *model = Script_Keep(&r, PyObject_GetAttrString(e, "Model"));
	PyObject *up = model ? Script_Keep(&r, Script_Ask("string", "upper", "(O)", model)) : NULL;
	PyObject *last = up ? Script_Keep(&r, PySequence_GetItem(up, -1)) : NULL;
	PyObject *name = last ? Script_Keep(&r, PyObject_GetAttrString(e, "Name")) : NULL;
	if (!name) {
		return Script_Done(&r, NULL);
	}
	byte indoor = Script_Is(last, "B");
	if (indoor) {
		up = Script_Keep(&r, PySequence_GetSlice(up, 0, PySequence_Length(up) - 1));
		if (!up) {
			return Script_Done(&r, NULL);
		}
	}
	PyObject *vars = Script_Keep(&r, Py_BuildValue("{s:O,s:O}", "model", up, "name", name));
	if (!vars) {
		return Script_Done(&r, NULL);
	}
	Script_RunFile(0, "fx/fx.def", "Motors", fx_def, fx_steps, vars);
	if (indoor) {
		PyObject *scale = Script_Keep(&r, PyObject_GetAttrString(e, "ShipMotorScale"));
		PyObject *f = scale ? Script_Keep(&r, PyFloat_FromDouble(0.6)) : NULL;
		PyObject *v = f ? PyNumber_Multiply(scale, f) : NULL;
		if (!Script_HSet(e, "ShipMotorScale", v)) {
			return Script_Done(&r, NULL);
		}
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* MoneyTransfer(EntityFrom, EntityTo, Time): the effect of SFX */
static PyObject *Fx_MoneyTransfer(PyObject *self, PyObject *args) {
	PyObject *from, *to, *time;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &from, &to, &time)) {
		return NULL;
	}
	return Script_Try("SFX", "MoneyTransfer", "(OOO)", from, to, time) ? Py_ReturnNone() : NULL;
}

PyMethodDef g_FxMethods[] = {
	{ "Init", Fx_Init, 1, NULL },
	{ "GetMotorClass", Fx_GetMotorClass, 1, NULL },
	{ "SetUpShipMotors", Fx_SetUpShipMotors, 1, NULL },
	{ "MoneyTransfer", Fx_MoneyTransfer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
