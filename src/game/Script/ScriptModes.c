/*
 * Native C helpers of the game mode ports (include/scrap/scriptmodes.h): the engine calls of Net, Ffa, the online and
 * split screen modes and the super deals on native values, the seams of the engine functions their case files record
 * or swap (OBJLIB 7), the module state they read and write in other modules (OBJLIB 5) and their adapters.
 */
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/logic.h"
#include "scrap/mapfloors.h"
#include "scrap/mathlib.h"
#include "scrap/net.h"
#include "scrap/pyentity.h"
#include "scrap/ai.h"
#include "scrap/alarm.h"
#include "scrap/crt.h"
#include "scrap/nodesworld.h"
#include "scrap/game.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrapfont.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/sound.h"
#include "scrap/timer.h"
#include "game/Script/levels/Levels.h"

/* ---- the values of a function ---- */

nativeVal_t *Mode_V(modeVals_t *t) {
	CHECK(t->n < MODE_VALS);
	nativeVal_t *v = &t->v[t->n++];
	Native_SetNone(v);
	return v;
}

void Mode_Release(modeVals_t *t, int mark) {
	while (t->n > mark) {
		Native_Free(&t->v[--t->n]);
	}
}

byte Mode_Done(modeVals_t *t, byte ok) {
	Mode_Release(t, 0);
	return ok;
}

nativeVal_t *Mode_S(modeVals_t *t, const char *s) {
	nativeVal_t *v = Mode_V(t);
	Native_SetText(v, s, (int)strlen(s));
	return v;
}

const nativeVal_t *Mode_I(modeVals_t *t, long i) {
	static nativeVal_t ints[32];
	static int count;
	for (int k = 0; k < count; k++) {
		if (ints[k].i == i) {
			return &ints[k];
		}
	}
	nativeVal_t *v = count < 32 ? &ints[count++] : Mode_V(t);
	Native_SetInt(v, i);
	return v;
}

nativeVal_t *Mode_Name(modeVals_t *t, const char *module, const char *func) {
	char name[0x40];
	Str_Printf(name, sizeof(name), "%s.%s", module, func);
	return Mode_S(t, name);
}

byte Mode_Cat(modeVals_t *t, nativeVal_t *out, int n, ...) {
	va_list va;
	va_start(va, n);
	const nativeVal_t *acc = va_arg(va, const nativeVal_t *);
	byte ok = 1;
	int mark = t->n;
	for (int i = 1; ok && i < n; i++) {
		nativeVal_t *r = Mode_V(t);
		ok = Native_Arith('+', acc, va_arg(va, const nativeVal_t *), r);
		acc = r;
	}
	va_end(va);
	if (ok) {
		Native_Copy(out, acc);
	}
	Mode_Release(t, mark);
	return ok;
}

/* ---- engine reads ---- */

double Mode_Time(void) {
	/* Scrap.GetTime gives the float timer as a double */
	return (double)Timer_GetTime(&g_pWorld->timer);
}

void Mode_Lang(const char *key, nativeVal_t *out) {
	const char *s = Lang_GetStr(key);
	if (!s) {
		Con_Warnf("STRING NOT FOUND: '%s'\n", key);
		s = "";
	}
	Native_SetText(out, s, (int)strlen(s));
}

byte Mode_LangFmt(const char *key, const nativeVal_t *args, nativeVal_t *out) {
	nativeVal_t t;
	Mode_Lang(key, &t);
	if (!args) {
		*out = t;
		return 1;
	}
	byte ok = Native_Format(&t, args, out);
	Native_Free(&t);
	return ok;
}

byte Mode_Ent(const nativeVal_t *name, cEntity **out) {
	return Native_GetEntity(name, out);
}

byte Mode_Get(cEntity *e, const char *name, const char *var, nativeVal_t *out) {
	if (!e) {
		/* getattr of None */
		return Native_Raise(NE_ATTRIBUTE, "'None' object has no attribute '%.400s'", var);
	}
	return Native_EntVar(e, name, var, out);
}

byte Mode_Set(cEntity *e, const char *name, const char *var, const nativeVal_t *v) {
	if (!e) {
		/* setattr of None */
		return Native_Raise(NE_TYPE, "attribute-less object (assign or del)");
	}
	return Native_EntSet(e, name, var, v);
}

byte Mode_SetI(cEntity *e, const char *name, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_Set(e, name, var, &v);
}

byte Mode_SetS(cEntity *e, const char *name, const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	byte ok = Mode_Set(e, name, var, &v);
	Native_Free(&v);
	return ok;
}

byte Mode_EntGet(const nativeVal_t *name, const char *var, nativeVal_t *out) {
	cEntity *e;
	return Mode_Ent(name, &e) && Mode_Get(e, Native_Text(name), var, out);
}

byte Mode_EntSet(const nativeVal_t *name, const char *var, const nativeVal_t *v) {
	cEntity *e;
	return Mode_Ent(name, &e) && Mode_Set(e, Native_Text(name), var, v);
}

byte Mode_ActionSetIs(const char *name) {
	return !strcmp(TABLE_AT(g_ActionSets, Input_GetActionSet())->name, name);
}

cEntity *Mode_Usr(void) {
	return cWorld_GetUsrEntity(g_pWorld, 0);
}

byte Mode_Create(const nativeVal_t *name, const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z,
	const char *cls, cEntity **out) {
	/* "sddds" */
	const char *n;
	double d[3];
	if (!Native_ArgStr(name, 1, &n) || !Native_ArgDouble(x, &d[0]) || !Native_ArgDouble(y, &d[1]) ||
		!Native_ArgDouble(z, &d[2])) {
		return 0;
	}
	*out = cWorld_CreateEntity(g_pWorld, n, (float)d[0], (float)d[1], (float)d[2], cls);
	return 1;
}

byte Mode_CreateAt(const nativeVal_t *name, const nativeVal_t *pos, const char *cls, cEntity **out) {
	nativeVal_t c[3], k;
	int n = 0;
	byte ok = 1;
	for (; ok && n < 3; n++) {
		Native_SetInt(&k, n);
		ok = Native_Subscript(pos, &k, &c[n]);
	}
	if (!ok) {
		n--;
	}
	ok = ok && Mode_Create(name, &c[0], &c[1], &c[2], cls, out);
	while (n-- > 0) {
		Native_Free(&c[n]);
	}
	return ok;
}

void Mode_DummyStart(const char *name, int flag) {
	DummySearch_Start(name, flag != 0);
}

void Mode_DummyNext(nativeVal_t *out) {
	/* Scrap.NextDummySearch: "s(ddd)(dd)i" */
	char name[0x40];
	vec3_t pos;
	float ang[2];
	byte flag;
	if (!DummySearch_Next(name, sizeof(name), &pos, ang, &flag)) {
		Native_SetNone(out);
		return;
	}
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetStr(Native_Append(out), name);
	Native_SetVec(Native_Append(out), (double)pos.x, (double)pos.y, (double)pos.z);
	nativeVal_t *a = Native_Append(out);
	Native_SetEmpty(a, NV_TUPLE);
	Native_SetFloat(Native_Append(a), (double)ang[0] * DEG_PER_PI / M_PI_D);
	Native_SetFloat(Native_Append(a), (double)ang[1] * DEG_PER_PI / M_PI_D);
	Native_SetInt(Native_Append(out), flag != 0);
}

byte Mode_Preload(const nativeVal_t *name) {
	/* OBJLIB 7: through the seam, fc-nm-po records it */
	PyObject *hook;
	int seam = Native_Seam("Scrap", "Preload3DObject", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, NULL, "v", name);
	}
	/* "s|ddd" */
	const char *n;
	if (!Native_ArgStr(name, 1, &n)) {
		return 0;
	}
	vec3_t scale = { 1.0f, 1.0f, 1.0f };
	Object3D_Preload(n, &scale);
	return 1;
}

byte Mode_Print(const nativeVal_t *const *items, int n) {
	PyObject *o[12] = { NULL };
	byte ok = 1;
	n = n < 12 ? n : 12;
	for (int k = 0; ok && k < n; k++) {
		ok = (o[k] = Native_ToPy(items[k])) != NULL;
	}
	ok = ok && Script_Print(o, n);
	for (int k = 0; k < n; k++) {
		Py_XDECREF(o[k]);
	}
	return ok;
}

byte Mode_SetModPy(const char *module, const char *var, PyObject *v) {
	PyObject *m = v ? PyImport_ImportModule((char *)module) : NULL;
	int r = m ? PyObject_SetAttrString(m, (char *)var, v) : -1;
	Py_XDECREF(m);
	return r == 0;
}

byte Mode_NextInSlot(cEntity **e) {
	/* e = Scrap.GetEntity(e.NextInSlot) */
	nativeVal_t next;
	Native_SetNone(&next);
	byte ok = Native_EntVar(*e, (*e)->name, "NextInSlot", &next) && Mode_Ent(&next, e);
	Native_Free(&next);
	return ok;
}

PyObject *Mode_Handle(cEntity *e) {
	PyObject *h = e ? PyEntity_New(e->name) : NULL;
	return h ? h : Py_ReturnNone();
}

/* a getattr that gave NULL: the error of the call as the interpreter reports it when none is set */
static byte Mode_AttrFail(void) {
	if (!PyErr_Occurred()) {
		PyErr_SetString(PyExc_SystemError, "error return without exception set");
	}
	return 0;
}

byte Mode_HGet(PyObject *o, const char *var, nativeVal_t *out) {
	if (PyEntity_Check(o)) {
		return Native_EntVar(PyEntity_Get(o), ((cPyEntity *)o)->ref->name, var, out);
	}
	PyObject *r = PyObject_GetAttrString(o, (char *)var);
	if (!r) {
		return Mode_AttrFail();
	}
	byte ok = Native_FromPyAny(r, out) || Native_Raise(NE_TYPE, "%s: not a value native C takes", var);
	Py_DECREF(r);
	return ok;
}

byte Mode_HSet(PyObject *o, const char *var, const nativeVal_t *v) {
	if (PyEntity_Check(o)) {
		return Native_EntSet(PyEntity_Get(o), ((cPyEntity *)o)->ref->name, var, v);
	}
	PyObject *x = Native_ToPy(v);
	int r = x ? PyObject_SetAttrString(o, (char *)var, x) : -1;
	Py_XDECREF(x);
	return r == 0;
}

byte Mode_HSetModel(PyObject *o, const char *module, const char *modelVar) {
	nativeVal_t model;
	byte ok;
	if (PyEntity_Check(o)) {
		cEntity *e = PyEntity_Get(o);
		const char *file;
		if (!e) {
			/* the method of a handle whose entity is gone */
			return Native_EntVar(NULL, ((cPyEntity *)o)->ref->name, "SetModel", &model);
		}
		if (!Mode_G(module, modelVar, &model)) {
			return 0;
		}
		/* "s|d" */
		ok = Native_ArgStr(&model, 1, &file);
		if (ok) {
			Entity_SetModel(e, file, 1.0f, 1.0f, 1.0f, 1);
		}
		Native_Free(&model);
		return ok;
	}
	PyObject *meth = PyObject_GetAttrString(o, "SetModel");
	if (!meth) {
		return Mode_AttrFail();
	}
	ok = Mode_G(module, modelVar, &model) && Native_Call(meth, NULL, "v", &model);
	if (ok || model.type != NV_NONE) {
		Native_Free(&model);
	}
	Py_DECREF(meth);
	return ok;
}

byte Mode_VGet(const nativeVal_t *o, const char *var, nativeVal_t *out) {
	PyObject *po = Native_ToPy(o);
	byte ok = po && Mode_HGet(po, var, out);
	Py_XDECREF(po);
	return ok;
}

byte Mode_VSet(const nativeVal_t *o, const char *var, const nativeVal_t *v) {
	PyObject *po = Native_ToPy(o);
	byte ok = po && Mode_HSet(po, var, v);
	Py_XDECREF(po);
	return ok;
}

byte Mode_HModel(PyObject *o, const nativeVal_t *file) {
	if (PyEntity_Check(o)) {
		cEntity *e = PyEntity_Get(o);
		const char *f;
		if (!e) {
			nativeVal_t x;
			return Native_EntVar(NULL, ((cPyEntity *)o)->ref->name, "SetModel", &x);
		}
		/* "s|d" */
		if (!Native_ArgStr(file, 1, &f)) {
			return 0;
		}
		Entity_SetModel(e, f, 1.0f, 1.0f, 1.0f, 1);
		return 1;
	}
	PyObject *meth = PyObject_GetAttrString(o, "SetModel");
	if (!meth) {
		return Mode_AttrFail();
	}
	byte ok = Native_Call(meth, NULL, "v", file);
	Py_DECREF(meth);
	return ok;
}

byte Mode_NameGet(const char *name, const char *var, nativeVal_t *out) {
	return Native_EntVar(World_FindEntity(name), name, var, out);
}

byte Mode_NameSet(const char *name, const char *var, const nativeVal_t *v) {
	return Native_EntSet(World_FindEntity(name), name, var, v);
}

/* dict[key] of a dict variable of the module as an object (a new reference) */
static PyObject *Mode_At(const char *module, const char *dict, const nativeVal_t *key) {
	PyObject *d = Script_Global(module, dict), *k = d ? Native_ToPy(key) : NULL;
	PyObject *o = k ? PyObject_GetItem(d, k) : NULL;
	Py_XDECREF(k);
	Py_XDECREF(d);
	return o;
}

byte Mode_AtGet(const char *module, const char *dict, const nativeVal_t *key, const char *var, nativeVal_t *out) {
	PyObject *o = Mode_At(module, dict, key);
	byte ok = o && Mode_HGet(o, var, out);
	Py_XDECREF(o);
	return ok;
}

byte Mode_AtSet(const char *module, const char *dict, const nativeVal_t *key, const char *var, const nativeVal_t *v) {
	PyObject *o = Mode_At(module, dict, key);
	byte ok = o && Mode_HSet(o, var, v);
	Py_XDECREF(o);
	return ok;
}

/* ---- engine calls through their seams ---- */

/* the seam of module.name: 0 engine, 1 *hook set, -1 error */
#define MODE_SEAM(module, name) \
	PyObject *hook; \
	int seam = Native_Seam(module, name, &hook); \
	if (seam < 0) { \
		return 0; \
	}

/* SNet.IsClient(), SNet.IsServer() through the seam: the truth of what the test put there */
static int Mode_NetIs(const char *name, byte engine) {
	PyObject *hook;
	int seam = Native_Seam("SNet", name, &hook);
	if (seam <= 0) {
		return seam < 0 ? -1 : engine;
	}
	PyObject *o = Script_Invoke(hook, PyTuple_New(0));
	Py_DECREF(hook);
	int t = o ? PyObject_IsTrue(o) : -1;
	Py_XDECREF(o);
	return t;
}

int Mode_IsClient(void) {
	return Mode_NetIs("IsClient", Net_IsClient() != 0);
}

int Mode_IsServer(void) {
	return Mode_NetIs("IsServer", Net_IsServer() != 0);
}

byte Mode_Send(const nativeVal_t *id, const nativeVal_t *s) {
	MODE_SEAM("SNet", "SendUsrString");
	if (seam) {
		return Native_HookCall(hook, NULL, "vv", id, s);
	}
	int i;
	const char *text;
	if (!Native_ArgInt(id, &i) || !Native_ArgStr(s, 2, &text)) {
		return 0;
	}
	Net_SendUsrString(text, (int)i);
	return 1;
}

byte Mode_SendS(long id, const char *s) {
	nativeVal_t i, t;
	Native_SetInt(&i, id);
	Native_SetText(&t, s, (int)strlen(s));
	byte ok = Mode_Send(&i, &t);
	Native_Free(&t);
	return ok;
}

byte Mode_ModifyUsrData(const nativeVal_t *id) {
	MODE_SEAM("SNet", "ModifyUsrData");
	if (seam) {
		return Native_HookCall(hook, NULL, "v", id);
	}
	int i;
	if (!Native_ArgInt(id, &i)) {
		return 0;
	}
	/* a call on a machine that is no server gives NULL without an exception */
	return Net_IsServer() ? (Net_ModifyUsrData((int)i), 1) : Native_NullResult();
}

/* SScorer.SetMsgText and SetLabelText */
static byte Mode_Text(const char *name, const nativeVal_t *text, double time) {
	MODE_SEAM("SScorer", name);
	if (seam) {
		return Native_HookCall(hook, NULL, "vd", text, time);
	}
	const char *s;
	if (!Native_ArgStr(text, 1, &s)) {
		return 0;
	}
	if (name[3] == 'M') {
		Scorer_SetMsgText(s, (float)time, "Horatio");
	} else {
		Scorer_SetLabelText(s, (float)time);
	}
	return 1;
}

byte Mode_MsgText(const nativeVal_t *text, double time) {
	return Mode_Text("SetMsgText", text, time);
}

byte Mode_LabelText(const nativeVal_t *text, double time) {
	return Mode_Text("SetLabelText", text, time);
}

byte Mode_MPFunc(long id, const char *func) {
	MODE_SEAM("SScorer", "SetMPFunc");
	if (seam) {
		return Native_HookCall(hook, NULL, "is", id, func);
	}
	cScorer *sc = Scorer_Get((int)id);
	if (sc) {
		cPy_Function_SetFunc(&sc->mpFunc, func);
	}
	return 1;
}

byte Mode_ScorerSet(const nativeVal_t *id, const nativeVal_t *item, const char *var, const nativeVal_t *v) {
	MODE_SEAM("SScorer", "Set");
	if (seam) {
		return Native_HookCall(hook, NULL, "vvsv", id, item, var, v);
	}
	/* "issd" after "isss": the errors of the second form */
	int i;
	const char *name;
	return Native_ArgInt(id, &i) && Native_ArgStr(item, 2, &name) && Native_ScorerSet(i, name, var, v);
}

byte Mode_Fade(long id, const int *from, const int *to, double time) {
	nativeVal_t t;
	Native_SetFloat(&t, time);
	return Mode_FadeV(id, from, to, &t);
}

byte Mode_FadeV(long id, const int *from, const int *to, const nativeVal_t *time) {
	MODE_SEAM("SScorer", "Fade");
	if (seam) {
		PyObject *a = Py_BuildValue("(iiii)", from[0], from[1], from[2], from[3]);
		PyObject *b = a ? Py_BuildValue("(iiii)", to[0], to[1], to[2], to[3]) : NULL;
		byte ok = b && Native_HookCall(hook, NULL, "iOOv", id, a, b, time);
		if (!b) {
			Py_DECREF(hook);
		}
		Py_XDECREF(a);
		Py_XDECREF(b);
		return ok;
	}
	double d;
	if (!Native_ArgDouble(time, &d)) {
		return 0;
	}
	cScorer *sc = Scorer_Get((int)id);
	if (sc) {
		ScorerFade_Start(&sc->fade, Scorer_PackColor((uint32_t)from[0], (uint32_t)from[1], (uint32_t)from[2], (uint32_t)from[3]),
			Scorer_PackColor((uint32_t)to[0], (uint32_t)to[1], (uint32_t)to[2], (uint32_t)to[3]), (float)d);
	}
	return 1;
}

byte Mode_Bind(const nativeVal_t *id, const char *set, const char *action, const char *func) {
	MODE_SEAM("SInput", "Bind");
	if (seam) {
		return Native_HookCall(hook, NULL, "vsss", id, set, action, func);
	}
	int i;
	if (!Native_ArgInt(id, &i)) {
		return 0;
	}
	Input_Bind(i, set, action, func);
	return 1;
}

byte Mode_DelSched(const nativeVal_t *name) {
	MODE_SEAM("Scrap", "DeleteScheduledFuncs");
	if (seam) {
		return Native_HookCall(hook, NULL, "v", name);
	}
	const char *s;
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	Scheduler_Delete(&g_pWorld->scheduler, s);
	return 1;
}

byte Mode_DelSchedS(const char *name) {
	nativeVal_t n;
	Native_SetText(&n, name, (int)strlen(name));
	byte ok = Mode_DelSched(&n);
	Native_Free(&n);
	return ok;
}

byte Mode_Schedule(const nativeVal_t *time, PyObject *func, const nativeVal_t *args, int n, const nativeVal_t *name) {
	nativeVal_t t;
	Native_SetEmpty(&t, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(&t), &args[i]);
	}
	PyObject *params = Native_ToPy(&t);
	Native_Free(&t);
	if (!params) {
		return 0;
	}
	PyObject *hook;
	int seam = Native_Seam("Scrap", "AddScheduledFunc", &hook);
	byte ok = 0;
	if (seam > 0) {
		ok = name ? Native_HookCall(hook, NULL, "vOOv", time, func, params, name) :
			Native_HookCall(hook, NULL, "vOO", time, func, params);
	} else if (seam == 0) {
		/* "dOO|s" */
		double d;
		const char *s = "";
		ok = Native_ArgDouble(time, &d) && (!name || Native_ArgStr(name, 4, &s));
		if (ok) {
			Scheduler_Add(&g_pWorld->scheduler, (float)d, func, params, s);
		}
	}
	Py_DECREF(params);
	return ok;
}

/* Mode_Later with the function of the module as its global (global 1) or as its attribute */
static byte Mode_LaterBy(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name,
	byte global) {
	nativeVal_t t, nm;
	/* OBJLIB 4: the function the scheduler keeps is the one of the module (a mission may have replaced it) */
	PyObject *fn = global ? Script_Global(module, func) : Script_Attr(module, func);
	if (!fn) {
		return 0;
	}
	Native_SetFloat(&t, Mode_Time() + delay);
	if (name) {
		Native_SetText(&nm, name, (int)strlen(name));
	}
	byte ok = Mode_Schedule(&t, fn, args, n, name ? &nm : NULL);
	if (name) {
		Native_Free(&nm);
	}
	Py_DECREF(fn);
	return ok;
}

byte Mode_Later(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name) {
	return Mode_LaterBy(delay, module, func, args, n, name, 1);
}

byte Mode_LaterIn(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name) {
	return Mode_LaterBy(delay, module, func, args, n, name, 0);
}

byte Mode_GVarSet(const char *name, const nativeVal_t *v) {
	/* Native_GVarSet goes through the seam */
	return Native_GVarSet(name, v);
}

byte Mode_GVarSetS(const char *name, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	byte ok = Mode_GVarSet(name, &v);
	Native_Free(&v);
	return ok;
}

byte Mode_GVarSetN(const char *name, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	return Mode_GVarSet(name, &v);
}

byte Mode_Rand(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	MODE_SEAM("Scrap", "Rand");
	if (seam) {
		return Native_HookCall(hook, out, "vv", a, b);
	}
	/* "dd" */
	double x, y;
	if (!Native_ArgDouble(a, &x) || !Native_ArgDouble(b, &y)) {
		return 0;
	}
	Native_SetFloat(out, Scrap_RandRange(x, y));
	return 1;
}

byte Mode_RandD(double a, double b, nativeVal_t *out) {
	nativeVal_t x, y;
	Native_SetFloat(&x, a);
	Native_SetFloat(&y, b);
	return Mode_Rand(&x, &y, out);
}

byte Mode_ConsoleError(const nativeVal_t *text) {
	MODE_SEAM("Scrap", "ConsoleError");
	if (seam) {
		return Native_HookCall(hook, NULL, "v", text);
	}
	const char *s;
	if (!Native_ArgStr(text, 1, &s)) {
		return 0;
	}
	Con_Print(CON_COLOR_ERROR, s, 0);
	return 1;
}

byte Mode_Play(const nativeVal_t *sound, const nativeVal_t *volume) {
	MODE_SEAM("SSound", "Play");
	if (seam) {
		return Native_HookCall(hook, NULL, "vv", sound, volume);
	}
	/* "s|dd" */
	const char *name;
	double vol;
	if (!Native_ArgStr(sound, 1, &name) || !Native_ArgDouble(volume, &vol)) {
		return 0;
	}
	SoundSys_Play(g_pSound, name, Snd_Volume((float)vol), 0);
	return 1;
}

byte Mode_Sound(const char *sound, const char *volume) {
	nativeVal_t s, v;
	if (!Mode_ModVar("Sound", sound, &s)) {
		return 0;
	}
	if (!Mode_ModVar("Sound", volume, &v)) {
		Native_Free(&s);
		return 0;
	}
	byte ok = Mode_Play(&s, &v);
	Native_Free(&s);
	Native_Free(&v);
	return ok;
}

byte Mode_ScorerClear(const nativeVal_t *id) {
	int i;
	if (!Native_ArgInt(id, &i)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(i);
	if (sc) {
		cScorer_Clear(sc);
	}
	return 1;
}

byte Mode_ScorerAdd(const nativeVal_t *id, const nativeVal_t *name, const char *type) {
	/* "iss|i" */
	int i;
	const char *n;
	if (!Native_ArgInt(id, &i) || !Native_ArgStr(name, 2, &n)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(i);
	if (sc) {
		cScorer_AddItem(sc, n, type, 0);
	}
	return 1;
}

void Mode_SetActionSet(const char *name) {
	Input_SelectActionSetNamed(name);
}

void Mode_SetTimeSpeed(double speed) {
	Timer_SetSpeed(&g_pWorld->timer, (float)speed);
}

byte Mode_LoadSound(const nativeVal_t *file) {
	const char *n;
	if (!Native_ArgStr(file, 1, &n)) {
		return 0;
	}
	SoundSys_LoadSound(g_pSound, n);
	return 1;
}

/* the three numbers of a vector or a tuple of three numbers, as "(ddd)" reads them: 0 for other values */
static byte Mode_Vec3(const nativeVal_t *v, double *x) {
	if (v->type == NV_VEC) {
		x[0] = v->v[0];
		x[1] = v->v[1];
		x[2] = v->v[2];
		return 1;
	}
	if (v->type != NV_TUPLE || v->n != 3) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		if (v->items[i].type != NV_INT && v->items[i].type != NV_FLOAT) {
			return 0;
		}
		x[i] = v->items[i].type == NV_INT ? (double)v->items[i].i : v->items[i].f;
	}
	return 1;
}

byte Mode_VecAdd(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	double x[3], y[3];
	if (!Mode_Vec3(a, x) || !Mode_Vec3(b, y)) {
		/* OBJLIB 7: the errors of "(ddd)(ddd)" from the engine function */
		return Native_CallMod("SVec", "Add", out, "vv", a, b);
	}
	Native_SetVec(out, x[0] + y[0], x[1] + y[1], x[2] + y[2]);
	return 1;
}

byte Mode_Rotate(const nativeVal_t *v, const nativeVal_t *rot, nativeVal_t *out) {
	double x[3], r[3];
	if (!Mode_Vec3(v, x) || !Mode_Vec3(rot, r)) {
		return Native_CallMod("SVec", "Rotate3D", out, "vv", v, rot);
	}
	/* SVec.Rotate3D: degrees, the vector math of the engine in floats */
	vec3_t rr = { (float)(r[0] * M_PI_D / DEG_PER_PI), (float)(r[1] * M_PI_D / DEG_PER_PI), (float)(r[2] * M_PI_D / DEG_PER_PI) };
	vec3_t src = { (float)x[0], (float)x[1], (float)x[2] }, res;
	Vec3_Rotate(&res, &src, &rr);
	Native_SetVec(out, (double)res.x, (double)res.y, (double)res.z);
	return 1;
}

byte Mode_AIState(const nativeVal_t *name, nativeVal_t *out) {
	MODE_SEAM("SAI", "GetStateVehicle");
	if (seam) {
		return Native_HookCall(hook, out, "v", name);
	}
	const char *n;
	if (!Native_ArgStr(name, 1, &n)) {
		return 0;
	}
	cEntity *e = World_FindEntity(n);
	cAIVehicle *ai = e ? Entity_GetAI(e) : NULL;
	if (ai) {
		Native_SetInt(out, cAIVehicle_GetState(ai));
	} else {
		Native_SetNone(out);
	}
	return 1;
}

byte Mode_Label(byte msg, const char *key, long secs) {
	nativeVal_t text;
	Mode_Lang(key, &text);
	double tm = Mode_Time() + (double)secs;
	byte ok = msg ? Mode_MsgText(&text, tm) : Mode_LabelText(&text, tm);
	Native_Free(&text);
	return ok;
}

/* ---- engine calls of the item and weapon ports ---- */

byte Mode_NetFlags(nativeVal_t *out) {
	MODE_SEAM("Scrap", "GetNetFlags");
	if (seam) {
		return Native_HookCall(hook, out, "");
	}
	uint32_t f = g_pApp->flags;
	Native_SetEmpty(out, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		Native_SetInt(Native_Append(out), (long)(f >> i & 1));
	}
	return 1;
}

/* a number as "d" reads it without an error (an int or a float) */
static byte Mode_Num(const nativeVal_t *v, double *d) {
	if (v->type == NV_INT) {
		*d = (double)v->i;
		return 1;
	}
	if (v->type == NV_FLOAT) {
		*d = v->f;
		return 1;
	}
	return 0;
}

/* a text as "s" reads it without an error (no 0 bytes) */
static byte Mode_Str(const nativeVal_t *v, const char **s) {
	if (v->type != NV_STR || (int)strlen(Native_Text(v)) != v->n) {
		return 0;
	}
	*s = Native_Text(v);
	return 1;
}

byte Mode_VecSub(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	double x[3], y[3];
	if (!Mode_Vec3(a, x) || !Mode_Vec3(b, y)) {
		return Native_CallMod("SVec", "Sub", out, "vv", a, b);
	}
	Native_SetVec(out, x[0] - y[0], x[1] - y[1], x[2] - y[2]);
	return 1;
}

byte Mode_VecMod(const nativeVal_t *a, nativeVal_t *out) {
	double x[3];
	if (!Mode_Vec3(a, x)) {
		return Native_CallMod("SVec", "Mod", out, "v", a);
	}
	Native_SetFloat(out, sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]));
	return 1;
}

byte Mode_VecNorm(const nativeVal_t *a, nativeVal_t *out) {
	double x[3];
	if (!Mode_Vec3(a, x)) {
		return Native_CallMod("SVec", "Norm", out, "v", a);
	}
	double len = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
	/* SVec.Norm: the float 1e-6 widened to double */
	if (len < (double)1e-6f) {
		Native_SetVec(out, 0.0, 1.0, 0.0);
	} else {
		Native_SetVec(out, x[0] / len, x[1] / len, x[2] / len);
	}
	return 1;
}

byte Mode_VecModSqr(const nativeVal_t *a, nativeVal_t *out) {
	double x[3];
	if (!Mode_Vec3(a, x)) {
		return Native_CallMod("SVec", "ModSqr", out, "v", a);
	}
	Native_SetFloat(out, x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
	return 1;
}

byte Mode_VecProd(const nativeVal_t *a, const nativeVal_t *f, nativeVal_t *out) {
	double x[3], d;
	if (!Mode_Vec3(a, x) || !Mode_Num(f, &d)) {
		return Native_CallMod("SVec", "Prod", out, "vv", a, f);
	}
	Native_SetVec(out, x[0] * d, x[1] * d, x[2] * d);
	return 1;
}

byte Mode_PlaySound(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume) {
	double p[3], vol;
	const char *s;
	if (!Mode_Vec3(pos, p) || !Mode_Str(name, &s) || !Mode_Num(volume, &vol)) {
		return Native_CallMod("SSound", "PlaySound", NULL, "vvv", pos, name, volume);
	}
	vec3_t at = { (float)p[0], (float)p[1], (float)p[2] };
	Sound_PlayAt(&at, s, vol, 0.0, 0.0, 1.0);
	return 1;
}

byte Mode_TestSphere(const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z, const nativeVal_t *r,
	const nativeVal_t *list, const nativeVal_t *exclude, nativeVal_t *out) {
	double c[3], rad;
	const char *l, *ex = "";
	if (!Mode_Num(x, &c[0]) || !Mode_Num(y, &c[1]) || !Mode_Num(z, &c[2]) || !Mode_Num(r, &rad) || !Mode_Str(list, &l) ||
		(exclude && !Mode_Str(exclude, &ex))) {
		return exclude ? Native_CallMod("Scrap", "TestSphere", out, "vvvvvv", x, y, z, r, list, exclude) :
			Native_CallMod("Scrap", "TestSphere", out, "vvvvv", x, y, z, r, list);
	}
	vec3_t at = { (float)c[0], (float)c[1], (float)c[2] };
	byte level;
	cEntity *e = World_TestSphere(&at, (float)rad, l, ex, &level);
	if (e) {
		Native_SetStr(out, e->name);
	} else if (level) {
		Native_SetStr(out, "");
	} else {
		Native_SetNone(out);
	}
	return 1;
}

byte Mode_TestLine(const nativeVal_t *from, const nativeVal_t *to, const nativeVal_t *list, nativeVal_t *out) {
	double a[3], b[3];
	const char *l;
	if (!Mode_Vec3(from, a) || !Mode_Vec3(to, b) || !Mode_Str(list, &l)) {
		return Native_CallMod("Scrap", "TestLine", out, "vvv", from, to, list);
	}
	vec3_t p = { (float)a[0], (float)a[1], (float)a[2] }, q = { (float)b[0], (float)b[1], (float)b[2] }, hit;
	cEntity *e;
	if (!World_TestLine(&p, &q, l, &hit, &e)) {
		Native_SetNone(out);
		return 1;
	}
	/* "(ddd)s" */
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetVec(Native_Append(out), (double)hit.x, (double)hit.y, (double)hit.z);
	Native_SetStr(Native_Append(out), e ? e->name : "");
	return 1;
}

byte Mode_SphereCall(const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z, const nativeVal_t *r,
	const nativeVal_t *list, const nativeVal_t *func) {
	double c[3], rad;
	const char *l, *f;
	if (!Mode_Num(x, &c[0]) || !Mode_Num(y, &c[1]) || !Mode_Num(z, &c[2]) || !Mode_Num(r, &rad) || !Mode_Str(list, &l) ||
		!Mode_Str(func, &f)) {
		return Native_CallMod("Scrap", "SphereCall", NULL, "vvvvvv", x, y, z, r, list, func);
	}
	vec3_t at = { (float)c[0], (float)c[1], (float)c[2] };
	cWorld_SphereCall(g_pWorld, EntityLists_Get(l), &at, (float)rad, f, 1);
	return 1;
}

/* the method of entity e (the handle made for name) found: 0 with the AttributeError of None for e NULL */
static byte Mode_Method(cEntity *e, const char *method) {
	return e ? 1 : Native_Raise(NE_ATTRIBUTE, "'None' object has no attribute '%.400s'", method);
}

byte Mode_SetModel(cEntity *e, const nativeVal_t *file) {
	/* "s|d" */
	const char *f;
	if (!Mode_Method(e, "SetModel") || !Native_ArgStr(file, 1, &f)) {
		return 0;
	}
	Entity_SetModel(e, f, 1.0f, 1.0f, 1.0f, 1);
	return 1;
}

byte Mode_SetFX(cEntity *e, const char *type) {
	if (!Mode_Method(e, "SetFX")) {
		return 0;
	}
	Entity_SetEmbeddedFX(e, type);
	return 1;
}

int Mode_IsType(cEntity *e, const char *type) {
	return Mode_Method(e, "IsType") ? Entity_IsType(e, type) != 0 : -1;
}

int Mode_Del(cEntity *e) {
	return Mode_Method(e, "Del") ? cEntity_Del(e) != 0 : -1;
}

byte Mode_UsrHandle(nativeVal_t *out) {
	MODE_SEAM("Scrap", "UsrEntity");
	if (seam) {
		return Native_HookCall(hook, out, "i", 0L);
	}
	PyObject *h = Mode_Handle(Mode_Usr());
	if (!h) {
		return 0;
	}
	Native_Arg(h, out);
	Py_DECREF(h);
	return 1;
}

byte Mode_EntHandle(cEntity *e, nativeVal_t *out) {
	PyObject *h = Mode_Handle(e);
	if (!h) {
		return 0;
	}
	Native_Arg(h, out);
	Py_DECREF(h);
	return 1;
}

byte Mode_EntityH(const nativeVal_t *name, nativeVal_t *out) {
	cEntity *e;
	return Mode_Ent(name, &e) && Mode_EntHandle(e, out);
}

/* the entity of the handle value h for its method: 1 with *e (a live entity), 0 with the error of the lookup (None's
 * AttributeError, the line and SystemError of a handle whose entity is gone), -1 for another object (the caller calls
 * its method the Python way) */
static int Mode_MethodOf(const nativeVal_t *h, const char *method, cEntity **e) {
	if (h->type == NV_OBJ && PyEntity_Check(h->obj)) {
		nativeVal_t x;
		*e = PyEntity_Get(h->obj);
		return *e ? 1 : Native_EntVar(NULL, ((cPyEntity *)h->obj)->ref->name, method, &x);
	}
	return h->type == NV_NONE ? Mode_Method(NULL, method) : -1;
}

/* h.method(args) of another object the Python way (fmt and the arguments as Native_Call takes them) */
static byte Mode_PyMethod(const nativeVal_t *h, const char *method, nativeVal_t *out, const char *fmt, const char *s) {
	PyObject *o = Native_ToPy(h), *m = o ? PyObject_GetAttrString(o, (char *)method) : NULL;
	byte ok = m ? (s ? Native_Call(m, out, fmt, s) : Native_Call(m, out, fmt)) : (o && Mode_AttrFail());
	Py_XDECREF(m);
	Py_XDECREF(o);
	return ok;
}

int Mode_VIsType(const nativeVal_t *h, const char *type) {
	cEntity *e;
	int k = Mode_MethodOf(h, "IsType", &e);
	if (k >= 0) {
		return k ? Entity_IsType(e, type) != 0 : -1;
	}
	nativeVal_t r = { 0 };
	k = Mode_PyMethod(h, "IsType", &r, "s", type) ? Native_True(&r) : -1;
	Native_Free(&r);
	return k;
}

byte Mode_VDel(const nativeVal_t *h) {
	cEntity *e;
	int k = Mode_MethodOf(h, "Del", &e);
	if (k >= 0) {
		return k && (cEntity_Del(e), 1);
	}
	return Mode_PyMethod(h, "Del", NULL, "", NULL);
}

byte Mode_VModel(const nativeVal_t *h, const nativeVal_t *file) {
	PyObject *o = Native_ToPy(h);
	byte ok = o && Mode_HModel(o, file);
	Py_XDECREF(o);
	return ok;
}

PyObject *Mode_VBound(const nativeVal_t *h, const char *method) {
	PyObject *o = Native_ToPy(h), *m = o ? PyObject_GetAttrString(o, (char *)method) : NULL;
	if (o && !m) {
		Mode_AttrFail();
	}
	Py_XDECREF(o);
	return m;
}

byte Mode_PreloadOnes(const nativeVal_t *name) {
	PyObject *hook;
	int seam = Native_Seam("Scrap", "Preload3DObject", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, NULL, "viii", name, 1L, 1L, 1L);
	}
	return Mode_Preload(name);
}

byte Mode_ListGet(const char *list, const char *var, nativeVal_t *out) {
	char *s;
	double d;
	int i;
	switch (EntityList_Get(list, var, &s, &d, &i)) {
	case -1:
		return Native_NullResult();
	case 1:
		if (s) {
			Native_SetText(out, s, (int)strlen(s));
		} else {
			Native_SetNone(out);
		}
		return 1;
	case 2:
		Native_SetFloat(out, d);
		return 1;
	case 3:
		Native_SetInt(out, i);
		return 1;
	}
	return Native_Raise(NE_ATTRIBUTE, "EntityListGet: list '%.64s' has no variable '%.64s'", list, var);
}

byte Mode_SetOnFloor(const nativeVal_t *name) {
	const char *n;
	if (!Native_ArgStr(name, 1, &n)) {
		return 0;
	}
	Logic_SetOnFloor(World_FindEntity(n));
	return 1;
}

int Mode_AlarmOn(void) {
	PyObject *hook;
	int seam = Native_Seam("Scrap", "GetAlarm", &hook);
	if (!seam) {
		return g_pWorld->alarm.on != 0;
	}
	if (seam < 0) {
		return -1;
	}
	nativeVal_t a = { 0 }, k, on = { 0 };
	Native_SetInt(&k, 0);
	int t = Native_HookCall(hook, &a, "") && Native_Subscript(&a, &k, &on) ? Native_True(&on) : -1;
	Native_Free(&a);
	Native_Free(&on);
	return t;
}

byte Mode_ScorerPut(int id, const char *item, const char *var, const nativeVal_t *v) {
	if (v->type == NV_STR) {
		/* the C text, up to its first 0 byte */
		Script_SetStr(id, item, var, Native_Text(v));
	} else if (v->type == NV_INT || v->type == NV_FLOAT) {
		Script_SetNum(id, item, var, v->type == NV_INT ? (double)v->i : v->f);
	} else {
		return Native_CallMod("SScorer", "Set", NULL, "issv", (long)id, item, var, v);
	}
	return 1;
}

void Mode_ScorerGet(int id, const char *item, const char *var, nativeVal_t *out) {
	char *s;
	double d;
	int i;
	switch (Scorer_GetItemVar(id, item, var, &s, &d, &i)) {
	case 1:
		if (s) {
			Native_SetText(out, s, (int)strlen(s));
		} else {
			Native_SetNone(out);
		}
		return;
	case 2:
		Native_SetFloat(out, d);
		return;
	case 3:
		Native_SetInt(out, i);
		return;
	}
	Native_SetNone(out);
}

int Mode_GetMoney(void) {
	return g_pWorld->money;
}

byte Mode_SetMoney(const nativeVal_t *money) {
	int i;
	if (!Native_ArgInt(money, &i)) {
		return 0;
	}
	g_pWorld->money = i;
	return 1;
}

void Mode_SaveVar(const char *name, nativeVal_t *out) {
	const char *s = SaveVar_Get(name);
	if (s) {
		Native_SetText(out, s, (int)strlen(s));
	} else {
		Native_SetNone(out);
	}
}

byte Mode_SetSaveVar(const char *name, const nativeVal_t *value) {
	const char *s;
	if (!Native_ArgStr(value, 2, &s)) {
		return 0;
	}
	SaveVar_Set(name, s);
	return 1;
}

/* ---- module state ---- */

byte Mode_Import(const char *name) {
	const char *names[2] = { name, NULL };
	return Script_Import(names);
}

byte Mode_Read(const char *module, const char *name, nativeVal_t *slot) {
	Native_Free(slot);
	return Mode_G(module, name, slot);
}

PyObject *Mode_GlobalPy(const char *module, const char *name) {
	return Script_Global(module, name);
}

byte Mode_SetModVar(const char *module, const char *var, const nativeVal_t *v) {
	PyObject *o = Native_ToPy(v);
	PyObject *m = o ? PyImport_ImportModule((char *)module) : NULL;
	int r = m ? PyObject_SetAttrString(m, (char *)var, o) : -1;
	Py_XDECREF(m);
	Py_XDECREF(o);
	return r == 0;
}

byte Mode_ModVar(const char *module, const char *var, nativeVal_t *out) {
	PyObject *o = Script_Attr(module, var);
	if (!o) {
		return 0;
	}
	byte ok = Native_FromPyAny(o, out) ||
		Native_Raise(NE_TYPE, "%s.%s: not a value native C takes", module, var);
	Py_DECREF(o);
	return ok;
}

/* var[key] = v (v NULL: del var[key]) on the dict object of a module variable (its global, or its attribute for
 * another module), then the dict read back into *d (NULL: not kept) */
static byte Mode_DictChange(const char *module, const char *var, byte global, nativeVal_t *d, const nativeVal_t *key,
	const nativeVal_t *v) {
	PyObject *o = global ? Script_Global(module, var) : Script_Attr(module, var);
	PyObject *k = o ? Native_ToPy(key) : NULL;
	PyObject *x = k && v ? Native_ToPy(v) : NULL;
	int r = !k || (v && !x) ? -1 : v ? PyObject_SetItem(o, k, x) : PyObject_DelItem(o, k);
	Py_XDECREF(x);
	Py_XDECREF(k);
	if (r == 0 && d) {
		Native_Free(d);
		if (!Native_FromPyAny(o, d)) {
			r = -1;
			Native_Raise(NE_TYPE, "%s.%s: not a value native C takes", module, var);
		}
	}
	Py_XDECREF(o);
	return r == 0;
}

byte Mode_DictPut(const char *module, const char *var, nativeVal_t *d, const nativeVal_t *key, const nativeVal_t *v) {
	return Mode_DictChange(module, var, 1, d, key, v);
}

byte Mode_DictDel(const char *module, const char *var, nativeVal_t *d, const nativeVal_t *key) {
	return Mode_DictChange(module, var, 1, d, key, NULL);
}

byte Mode_ModDictPut(const char *module, const char *var, const nativeVal_t *key, const nativeVal_t *v) {
	return Mode_DictChange(module, var, 0, NULL, key, v);
}

byte Mode_ModDictDel(const char *module, const char *var, const nativeVal_t *key) {
	return Mode_DictChange(module, var, 0, NULL, key, NULL);
}

byte Mode_G(const char *module, const char *name, nativeVal_t *out) {
	const nativeVar_t vars[] = { { name, out, 0, NV_ANY }, { NULL, NULL, 0, 0 } };
	Native_SetNone(out);
	return Native_ImportGlobals(module, vars);
}

/* var.method(args) of a variable of a module (a new reference, NULL with the error) */
static PyObject *Mode_VarMethod(const char *module, const char *var, byte global, const char *method, PyObject *arg) {
	PyObject *o = global ? Script_Global(module, var) : Script_Attr(module, var);
	PyObject *r = o ? (arg ? Script_Method(o, method, "(O)", arg) : Script_Method(o, method, "()")) : NULL;
	Py_XDECREF(o);
	return r;
}

byte Mode_Keys(const char *module, const char *var, byte global, nativeVal_t *out) {
	PyObject *r = Mode_VarMethod(module, var, global, "keys", NULL);
	if (!r) {
		return 0;
	}
	byte ok = Native_FromPyAny(r, out) || Native_Raise(NE_TYPE, "%s.%s.keys(): not a value native C takes", module, var);
	Py_DECREF(r);
	return ok;
}

int Mode_HasKey(const char *module, const char *var, byte global, const nativeVal_t *key) {
	PyObject *k = Native_ToPy(key);
	PyObject *r = k ? Mode_VarMethod(module, var, global, "has_key", k) : NULL;
	int t = r ? PyObject_IsTrue(r) : -1;
	Py_XDECREF(r);
	Py_XDECREF(k);
	return t;
}

byte Mode_Index(const nativeVal_t *list, const nativeVal_t *x, nativeVal_t *out) {
	if (list->type != NV_LIST) {
		return Native_Raise(NE_ATTRIBUTE, "'%.50s' object has no attribute 'index'", Native_TypeName(list));
	}
	for (int i = 0; i < list->n; i++) {
		if (!Native_Compare(&list->items[i], x)) {
			Native_SetInt(out, i);
			return 1;
		}
	}
	return Native_Raise(NE_VALUE, "list.index(x): x not in list");
}

byte Mode_Append(const char *module, const char *var, const nativeVal_t *v) {
	/* the append method of whatever value is there (a value that is no list: the AttributeError of the script) */
	PyObject *x = Native_ToPy(v);
	PyObject *r = x ? Mode_VarMethod(module, var, 1, "append", x) : NULL;
	Py_XDECREF(r);
	Py_XDECREF(x);
	return r != NULL;
}

byte Mode_Unpack(const nativeVal_t *v, int n, nativeVal_t *out) {
	/* Script_Unpack */
	byte t = v->type == NV_VEC ? NV_TUPLE : v->type;
	if (t != NV_STR && t != NV_LIST && t != NV_TUPLE) {
		return Native_Raise(NE_TYPE, "unpack non-sequence");
	}
	if (Native_Size(v) != n) {
		return Native_Raise(NE_VALUE, t == NV_TUPLE ? "unpack tuple of wrong size" : t == NV_LIST ?
			"unpack list of wrong size" : "unpack sequence of wrong size");
	}
	for (int i = 0; i < n; i++) {
		Native_SeqItem(v, i, &out[i]);
	}
	return 1;
}

byte Mode_Put(const char *module, const char *var, nativeVal_t *slot, const nativeVal_t *v) {
	PyObject *o = Native_ToPy(v);
	if (!o || !Script_SetGlobal(module, var, o)) {
		return 0;
	}
	if (slot && slot != v) {
		nativeVal_t c;
		Native_Copy(&c, v);
		Native_Free(slot);
		*slot = c;
	}
	return 1;
}

/* ---- shared functions of the modes ---- */

byte Mode_PurgeServerMapList(const char *module) {
	nativeVal_t text, prev, modes, type, valid, keep;
	const nativeVar_t vars[] = { { "ModesMap", &modes, 0, NV_ANY }, { NULL, NULL, 0, 0 } };
	byte ok = 0;
	Native_SetNone(&prev);
	Native_SetNone(&modes);
	Native_SetNone(&type);
	Native_SetNone(&valid);
	Native_SetEmpty(&keep, NV_LIST);
	/* prev = eval(Scrap.Get("ServerMapList")) in the module; valid = ModesMap[Scrap.Get("ServerType")] */
	if (!Native_GVarGet("ServerMapList", &text)) {
		return 0;
	}
	if (Native_EvalText(module, &text, &prev) && Native_ImportGlobals(module, vars) && Native_GVarGet("ServerType", &type) &&
		Native_Subscript(&modes, &type, &valid)) {
		int n = Native_ForCount(&prev);
		ok = n >= 0;
		for (int i = 0; ok && i < n; i++) {
			nativeVal_t a;
			ok = Native_SeqItem(&prev, i, &a);
			int in = ok ? Native_Contains(&valid, &a) : -1;
			if (in > 0) {
				*Native_Append(&keep) = a;
			} else if (ok) {
				Native_Free(&a);
			}
			ok = in >= 0;
		}
		if (ok) {
			nativeBuf_t b;
			nativeVal_t rep;
			Native_BufInit(&b);
			Native_Repr(&b, &keep, NR_REPR);
			Native_SetText(&rep, b.p, b.len);
			Native_BufFree(&b);
			ok = Mode_GVarSet("ServerMapList", &rep);
			Native_Free(&rep);
		}
	}
	Native_Free(&text);
	Native_Free(&prev);
	Native_Free(&modes);
	Native_Free(&type);
	Native_Free(&valid);
	Native_Free(&keep);
	return ok;
}

/* ---- adapters ---- */

byte Mode_Args(PyObject *args, int n, modeVals_t *t, nativeVal_t **a) {
	static const char fmts[12][12] = { "", "O", "OO", "OOO", "OOOO", "OOOOO", "OOOOOO", "OOOOOOO", "OOOOOOOO", "OOOOOOOOO",
		"OOOOOOOOOO", "OOOOOOOOOOO" };
	PyObject *o[11];
	if (!PyArg_ParseTuple(args, (char *)fmts[n], &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7], &o[8], &o[9], &o[10])) {
		return 0;
	}
	/* n values one after the other */
	*a = n ? Mode_V(t) : NULL;
	for (int i = 1; i < n; i++) {
		Mode_V(t);
	}
	for (int i = 0; i < n; i++) {
		Native_Arg(o[i], &(*a)[i]);
	}
	return 1;
}

PyObject *Mode_Out(modeVals_t *t, byte ok, const nativeVal_t *out) {
	PyObject *r = ok ? (out->type == NV_NONE ? Py_ReturnNone() : Native_ToPy(out)) : NULL;
	Mode_Done(t, 0);
	return r;
}

PyObject *Mode_Result(byte ok) {
	return ok ? Py_ReturnNone() : NULL;
}

PyObject *Mode_Return(byte ok, nativeVal_t *v) {
	PyObject *r = ok ? Native_ToPy(v) : NULL;
	Native_Free(v);
	return r;
}

/* ---- helpers of the native super deals (SuperDeals.c) ---- */

byte Mode_Values(const char *module, const char *var, byte global, nativeVal_t *out) {
	PyObject *r = Mode_VarMethod(module, var, global, "values", NULL);
	if (!r) {
		return 0;
	}
	byte ok = Native_FromPyAny(r, out) || Native_Raise(NE_TYPE, "%s.%s.values(): not a value native C takes", module, var);
	Py_DECREF(r);
	return ok;
}

byte Mode_Remove(const char *module, const char *var, const nativeVal_t *v) {
	PyObject *x = Native_ToPy(v);
	PyObject *r = x ? Mode_VarMethod(module, var, 1, "remove", x) : NULL;
	Py_XDECREF(r);
	Py_XDECREF(x);
	return r != NULL;
}

byte Mode_ListSet(const char *list, const char *var, const char *value) {
	void *l = cWorld_GetList(g_pWorld, list);
	if (!l) {
		Con_Errorf("ERROR: EntityListSet '%s','%s','%s' list not found.\n", list, var, value);
		return Native_NullResult();
	}
	char *s = (char *)value;
	cEntityList_Var(l, var, 1, &s, NULL, NULL);
	return 1;
}

byte Mode_VecOf(const nativeVal_t *v, double *x) {
	return Mode_Vec3(v, x);
}

/* ---- the ship editor tabs ---- */

void Mode_Repr(const nativeVal_t *v, nativeVal_t *out) {
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_Repr(&b, v, NR_REPR);
	Native_SetText(out, b.p, b.len);
	Native_BufFree(&b);
}

byte Mode_ItemText(const nativeVal_t *v, const char **out) {
	if (v->type != NV_STR) {
		return Native_Raise(NE_TYPE, "argument 2: expected string found");
	}
	*out = Native_Text(v);
	return 1;
}

PyObject *Mode_From(const char *module, const char *name) {
	PyObject *m = PyImport_ImportModule((char *)module);
	PyObject *v = m ? PyDict_GetItemString(PyModule_GetDict(m), (char *)name) : NULL;
	if (m && !v) {
		PyErr_Format(PyExc_ImportError, "cannot import name %.230s", name);
	}
	Py_XINCREF(v);
	Py_XDECREF(m);
	return v;
}

byte Mode_PrevShip(nativeVal_t *ship) {
	PyObject *o = Script_Attr("HangarTab", "prevShip");
	Native_SetNone(ship);
	if (!o) {
		return 0;
	}
	Native_Arg(o, ship);
	Py_DECREF(o);
	return 1;
}

byte Mode_ShipGet(const char *var, nativeVal_t *out) {
	nativeVal_t ship;
	byte ok = Mode_PrevShip(&ship) && Mode_VGet(&ship, var, out);
	Native_Free(&ship);
	return ok;
}

byte Mode_ShipSet(const char *var, const nativeVal_t *v) {
	nativeVal_t ship;
	byte ok = Mode_PrevShip(&ship) && Mode_VSet(&ship, var, v);
	Native_Free(&ship);
	return ok;
}

byte Mode_ShipTable(const char *table, long i, nativeVal_t *out) {
	nativeVal_t model = { 0 }, up = { 0 }, tab = { 0 }, row = { 0 }, k;
	Native_SetInt(&k, i);
	byte ok = Mode_ShipGet("Model", &model) && Native_Upper(&model, &up) && Mode_ModVar("ShipMaker", table, &tab) &&
		Native_Subscript(&tab, &up, &row) && Native_Subscript(&row, &k, out);
	Native_Free(&model);
	Native_Free(&up);
	Native_Free(&tab);
	Native_Free(&row);
	return ok;
}

void Mode_Replace(const nativeVal_t *s, const char *old, const nativeVal_t *with, nativeVal_t *out) {
	nativeBuf_t b;
	const char *t = Native_Text(s);
	int len = (int)strlen(old), from = 0;
	Native_BufInit(&b);
	for (int i = 0; i + len <= s->n; i++) {
		if (!memcmp(t + i, old, (size_t)len)) {
			Native_BufAdd(&b, t + from, i - from);
			Native_BufAdd(&b, Native_Text(with), with->n);
			from = i + len;
			i += len - 1;
		}
	}
	Native_BufAdd(&b, t + from, s->n - from);
	Native_SetText(out, b.p, b.len);
	Native_BufFree(&b);
}

byte Mode_Range(const nativeVal_t *n, long *count) {
	int i;
	if (n->type == NV_INT) {
		*count = n->i;
		return 1;
	}
	if (n->type == NV_LONG) {
		nativeVal_t x;
		Native_SetNone(&x);
		byte ok = Native_ToInt(n, &x) && (x.type == NV_INT || Native_Raise(NE_OVERFLOW, "long int too long to convert"));
		if (ok) {
			*count = x.i;
		}
		Native_Free(&x);
		return ok;
	}
	if (!Native_ArgInt(n, &i)) {
		return 0;
	}
	*count = i;
	return 1;
}

/* the pair (a, b) of floats an engine function gives into *out */
static void Mode_Pair(double a, double b, nativeVal_t *out) {
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetFloat(Native_Append(out), a);
	Native_SetFloat(Native_Append(out), b);
}

byte Mode_Get2DPos(const nativeVal_t *pos, nativeVal_t *out) {
	double x[3];
	float uv[2];
	if (!Mode_VecOf(pos, x)) {
		/* "(fff)" of the engine function: its TypeError */
		return Native_CallMod("SScorer", "Get2DPos", out, "v", pos);
	}
	vec3_t p = { (float)x[0], (float)x[1], (float)x[2] };
	Engine_WorldToScreen(&p, uv, 0);
	Mode_Pair((double)uv[0], (double)uv[1], out);
	return 1;
}

byte Mode_TextArea(const char *font, const nativeVal_t *text, nativeVal_t *out) {
	const char *s;
	float rect[2], size[2];
	/* "ss" */
	if (!Native_ArgStr(text, 2, &s)) {
		return 0;
	}
	Font_TextSize(Font_Get(font), rect, size, 0.0f, 0.0f, "%s", s);
	Mode_Pair((double)size[0], (double)size[1], out);
	return 1;
}

/* ---- helpers of the native level scripts ---- */

void Mode_RandomVisPoint(nativeVal_t *out) {
	int n = NodesWorld_NumVisPoints(g_NodesWorld);
	if (n <= 0) {
		Native_SetNone(out);
		return;
	}
	const vec3_t *p = NodesWorld_GetVisPoint(g_NodesWorld, crt_rand() % n);
	/* "(fff)" */
	Native_SetVec(out, (double)p->x, (double)p->y, (double)p->z);
}

byte Mode_SetAlarm(const nativeVal_t *level) {
	MODE_SEAM("Scrap", "SetAlarm");
	if (seam) {
		return Native_HookCall(hook, NULL, "v", level);
	}
	double d;
	if (!Native_ArgDouble(level, &d)) {
		return 0;
	}
	Alarm_Set(&g_pWorld->alarm, (float)d);
	return 1;
}

byte Mode_GVarSetI(const char *name, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_GVarSet(name, &v);
}

/* ---- the native level files of batch G ---- */

byte Mode_Calls(const char *module, const char *const *lines) {
	static const char fmts[] = "vvvvvvvv";
	for (; *lines; lines++) {
		const char *p = *lines, *bar = strchr(p, '|');
		char name[0x80];
		int len = bar ? (int)(bar - p) : (int)strlen(p);
		if (len >= (int)sizeof(name)) {
			return Native_Raise(NE_SYSTEM, "bad call in a call table");
		}
		memcpy(name, p, len);
		name[len] = 0;
		PyObject *fn;
		if (name[0] == '@') {
			fn = Script_Global(module, name + 1);
		} else {
			char *dot = strchr(name, '.');
			if (!dot) {
				return Native_Raise(NE_SYSTEM, "bad call in a call table");
			}
			*dot = 0;
			fn = Script_Attr(name, dot + 1);
		}
		if (!fn) {
			return 0;
		}
		nativeVal_t v[8];
		int n = 0;
		byte ok = 1;
		for (p = bar; p && n < 8 && ok; n++) {
			const char *next = strchr(p + 1, '|');
			ok = LvN_Value(p + 1, next ? (int)(next - p - 1) : -1, &v[n]);
			p = next;
		}
		if (ok) {
			ok = Native_HookCall(fn, NULL, fmts + 8 - n, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
		} else {
			Py_DECREF(fn);
		}
		while (n-- > 0) {
			Native_Free(&v[n]);
		}
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

byte Mode_SetAttrs(const nativeVal_t *o, const char *const *pairs) {
	for (; *pairs; pairs++) {
		const char *eq = strchr(*pairs, '=');
		char name[0x40];
		int len = eq ? (int)(eq - *pairs) : 0;
		if (!eq || len >= (int)sizeof(name)) {
			return Native_Raise(NE_SYSTEM, "bad attribute in a table");
		}
		memcpy(name, *pairs, len);
		name[len] = 0;
		nativeVal_t v;
		byte ok = LvN_Value(eq + 1, -1, &v) && Mode_VSet(o, name, &v);
		Native_Free(&v);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

byte Mode_NISpeech(const char *who, const char *key) {
	return (who ? Native_CallMod("Speech", "NISpeech", NULL, "s", who) : Native_CallMod("Speech", "NISpeech", NULL, "")) &&
		Native_CallMod("Speech", "AutoAddNI", NULL, "s", key) && Native_CallMod("Speech", "RunNI", NULL, "");
}

byte Mode_EntSetS(const char *name, const char *var, const char *s) {
	nativeVal_t n, v;
	Native_SetText(&n, name, (int)strlen(name));
	Native_SetText(&v, s, (int)strlen(s));
	byte ok = Mode_EntSet(&n, var, &v);
	Native_Free(&n);
	Native_Free(&v);
	return ok;
}

byte Mode_EntSetI(const char *name, const char *var, long i) {
	nativeVal_t n, v;
	Native_SetText(&n, name, (int)strlen(name));
	Native_SetInt(&v, i);
	byte ok = Mode_EntSet(&n, var, &v);
	Native_Free(&n);
	return ok;
}

byte Mode_SetLockAlarm(const nativeVal_t *lock) {
	MODE_SEAM("Scrap", "SetLockAlarm");
	if (seam) {
		return Native_HookCall(hook, NULL, "v", lock);
	}
	int i;
	if (!Native_ArgInt(lock, &i)) {
		return 0;
	}
	Alarm_SetLock(&g_pWorld->alarm, i != 0);
	return 1;
}

byte Mode_VObj(const nativeVal_t *o, const char *var, nativeVal_t *out) {
	PyObject *po = Native_ToPy(o);
	if (!po) {
		return 0;
	}
	if (PyEntity_Check(po)) {
		byte ok = Mode_HGet(po, var, out);
		Py_DECREF(po);
		return ok;
	}
	PyObject *r = PyObject_GetAttrString(po, (char *)var);
	Py_DECREF(po);
	if (!r) {
		return Mode_AttrFail();
	}
	Native_Arg(r, out);
	Py_DECREF(r);
	return 1;
}

void Mode_Ints3(nativeVal_t *out, long a, long b, long c) {
	Native_SetEmpty(out, NV_TUPLE);
	Native_SetInt(Native_Append(out), a);
	Native_SetInt(Native_Append(out), b);
	Native_SetInt(Native_Append(out), c);
}

void Mode_Texts(nativeVal_t *out, const char *const *texts, int n) {
	Native_SetEmpty(out, NV_LIST);
	for (int i = 0; n < 0 ? texts[i] != NULL : i < n; i++) {
		Native_SetText(Native_Append(out), texts[i], (int)strlen(texts[i]));
	}
}

byte Mode_KeyText(const char *key, const char *action, nativeVal_t *out) {
	nativeVal_t e;
	const char *s = Input_GetDefinedEntry(0, "Walk", action, "Mouse", 0);
	if (s) {
		Native_SetText(&e, s, (int)strlen(s));
	} else {
		Native_SetNone(&e);
	}
	Native_SetNone(out);
	byte ok = Mode_LangFmt(key, &e, out);
	Native_Free(&e);
	return ok;
}

PyObject *Mode_CharRes(const nativeVal_t *key) {
	PyObject *res = Script_Attr("CharsNPC", "CharRes");
	PyObject *k = res ? Native_ToPy(key) : NULL;
	PyObject *ch = k ? PyObject_GetItem(res, k) : NULL;
	Py_XDECREF(k);
	Py_XDECREF(res);
	return ch;
}

void Mode_Saves(const char *const *pairs) {
	for (; *pairs; pairs += 2) {
		SaveVar_Set(pairs[0], pairs[1]);
	}
}

byte Mode_SaveInt(const char *name, nativeVal_t *out) {
	nativeVal_t s;
	Mode_SaveVar(name, &s);
	Native_SetNone(out);
	byte ok = Native_ToInt(&s, out);
	Native_Free(&s);
	return ok;
}

int Mode_Cmp(const nativeVal_t *a, const char *op, const nativeVal_t *b) {
	int c = Native_Compare(a, b);
	if (c && PyErr_Occurred()) {
		return -1;
	}
	switch (op[0]) {
	case '=':
		return c == 0;
	case '!':
		return c != 0;
	case '<':
		return op[1] == '=' ? c <= 0 : c < 0;
	}
	return op[1] == '=' ? c >= 0 : c > 0;
}

int Mode_CmpI(const nativeVal_t *a, const char *op, long i) {
	nativeVal_t b;
	Native_SetInt(&b, i);
	return Mode_Cmp(a, op, &b);
}

int Mode_UsrIs(const char *ctrl) {
	nativeVal_t u, a;
	Native_SetNone(&u);
	Native_SetNone(&a);
	int r = Mode_UsrHandle(&u) && Mode_VObj(&u, "ActCtrl", &a) ? LvN_EqS(&a, ctrl) : -1;
	Native_Free(&u);
	Native_Free(&a);
	return r;
}

byte Mode_Spch(const char *msg, const char *ans, const char *how) {
	return how ? LV_CALL("Speech", "OnSpch", NULL, "sss", msg, ans, how) : LV_CALL("Speech", "OnSpch", NULL, "ss", msg, ans);
}

byte Mode_AnsLst(const char *const *texts, int n) {
	nativeVal_t l;
	Mode_Texts(&l, texts, n);
	byte ok = LV_CALL("Speech", "StdAnsLst", NULL, "v", &l);
	Native_Free(&l);
	return ok;
}

byte Mode_MMDesc(const char *desc, long flag) {
	return LV_CALL("SaveGame", "MMDesc", NULL, "si", desc, flag);
}

byte Mode_WayPoint(void) {
	return LV_CALL("SaveGame", "QWayPoint", NULL, "");
}

byte Mode_Target(const char *name) {
	return name ? LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "s", name) :
		LV_CALL("MissionsFuncs", "SetMissionTarget", NULL, "O", Py_None);
}

byte Mode_Connect(const char *a, const char *b, byte pair) {
	const char *names[2] = { a, b };
	nativeVal_t l1, l2;
	Mode_Texts(&l1, names, pair ? 2 : 1);
	Mode_Texts(&l2, names + 1, pair ? 0 : 1);
	byte ok = pair ? LV_CALL("Elevator", "ConnectElevators", NULL, "vvi", &l1, &l2, 0L) :
		LV_CALL("Elevator", "ConnectElevators", NULL, "vv", &l1, &l2);
	Native_Free(&l1);
	Native_Free(&l2);
	return ok;
}

byte Mode_GSetS(const char *module, const char *var, const char *attr, const char *s) {
	nativeVal_t h;
	byte ok = Mode_G(module, var, &h) && LvN_SetS(&h, attr, s);
	Native_Free(&h);
	return ok;
}

byte Mode_AtG(const nativeVal_t *time, const char *module, const char *func, const nativeVal_t *args, int n) {
	PyObject *fn = Script_Global(module, func);
	if (!fn) {
		return 0;
	}
	byte ok = Mode_Schedule(time, fn, args, n, NULL);
	Py_DECREF(fn);
	return ok;
}

byte Mode_Arith(char op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	if ((op != '*' && op != '/' && op != '%') || (a->type != NV_LONG && b->type != NV_LONG)) {
		return Native_Arith(op, a, b, out);
	}
	/* OBJLIB 9 */
	PyObject *x = Native_ToPy(a), *y = x ? Native_ToPy(b) : NULL, *r = NULL;
	if (y) {
		r = op == '*' ? PyNumber_Multiply(x, y) : op == '/' ? PyNumber_Divide(x, y) : PyNumber_Remainder(x, y);
	}
	Native_SetNone(out);
	if (r) {
		Native_Arg(r, out);
	}
	Py_XDECREF(r);
	Py_XDECREF(y);
	Py_XDECREF(x);
	return r != NULL;
}

byte Mode_GVarSetDef(const char *name) {
	nativeVal_t d;
	Native_SetNone(&d);
	byte ok = Native_GVarDef(name, &d) && Mode_GVarSet(name, &d);
	Native_Free(&d);
	return ok;
}

/* ---- the native level files of batch H (Commercial, Police, Press) ---- */

byte Mode_Set0(const char *item, const char *var, const nativeVal_t *v) {
	nativeVal_t id, it;
	Native_SetInt(&id, 0);
	Native_SetText(&it, item, (int)strlen(item));
	byte ok = Mode_ScorerSet(&id, &it, var, v);
	Native_Free(&it);
	return ok;
}

byte Mode_Set0I(const char *item, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_Set0(item, var, &v);
}

/* Scorer.<var> + d */
static byte Mode_TextAt(const char *var, long d, nativeVal_t *out) {
	nativeVal_t v, n;
	if (!LvN_ModGet("Scorer", var, &v)) {
		Native_SetNone(out);
		return 0;
	}
	if (!d) {
		*out = v;
		return 1;
	}
	Native_SetInt(&n, d);
	Native_SetNone(out);
	byte ok = Native_Arith('+', &v, &n, out);
	Native_Free(&v);
	return ok;
}

byte Mode_TextX(long d, nativeVal_t *out) {
	return Mode_TextAt("MissionScorerText_X", d, out);
}

byte Mode_TextY(long d, nativeVal_t *out) {
	return Mode_TextAt("MissionScorerText_Y", d, out);
}

/* SScorer.Set(id, item, var, "text") */
static byte Mode_ScorerSetS(const nativeVal_t *id, const char *item, const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	byte ok = LvN_ScorerSet(id, item, var, &v);
	Native_Free(&v);
	return ok;
}

static byte Mode_ScorerSetI(const nativeVal_t *id, const char *item, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return LvN_ScorerSet(id, item, var, &v);
}

/* SScorer.Add(id, item, type) with a C item name */
static byte Mode_ScorerAddS(const nativeVal_t *id, const char *item, const char *type) {
	nativeVal_t it;
	Native_SetText(&it, item, (int)strlen(item));
	byte ok = Mode_ScorerAdd(id, &it, type);
	Native_Free(&it);
	return ok;
}

byte Mode_TextColour(const nativeVal_t *id, const char *item) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	for (int i = 0; i < 4; i++) {
		nativeVal_t c, k;
		Native_SetNone(&k);
		byte ok = LvN_ModGet("Scorer", "MissionScorerText_Color", &c) && LvN_Item(&c, i, &k) && LvN_ScorerSet(id, item, rgba[i], &k);
		Native_Free(&c);
		Native_Free(&k);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

byte Mode_MissionText(const nativeVal_t *id, const char *item, const char *type, const nativeVal_t *x, const nativeVal_t *y,
	const char *font, const char *effect, const nativeVal_t *text, const char *align, const nativeVal_t *visible) {
	if ((type && !Mode_ScorerAddS(id, item, type)) || !LvN_ScorerSet(id, item, "X", x) || !LvN_ScorerSet(id, item, "Y", y) ||
		!Mode_ScorerSetS(id, item, "Font", font) || (effect && !Mode_ScorerSetS(id, item, "Effect", effect)) ||
		!LvN_ScorerSet(id, item, "Text", text) || !Mode_ScorerSetS(id, item, "Align", align)) {
		return 0;
	}
	return Mode_TextColour(id, item) && (!visible || LvN_ScorerSet(id, item, "Visible", visible));
}

byte Mode_MissionSprite(const nativeVal_t *id, const char *item, const nativeVal_t *x, const nativeVal_t *y, long alpha, long r,
	long g, long b) {
	return Mode_ScorerAddS(id, item, "Sprite") && LvN_ScorerSet(id, item, "X", x) && LvN_ScorerSet(id, item, "Y", y) &&
		Mode_ScorerSetI(id, item, "ScaleX", 30) && Mode_ScorerSetI(id, item, "ScaleY", 19) && Mode_ScorerSetI(id, item, "Unit", 1) &&
		Mode_ScorerSetI(id, item, "Alpha", alpha) && Mode_ScorerSetI(id, item, "Red", r) && Mode_ScorerSetI(id, item, "Green", g) &&
		Mode_ScorerSetI(id, item, "Blue", b);
}

byte Mode_BarDefaults(const nativeVal_t *id, const char *item, const nativeVal_t *main, const nativeVal_t *black) {
	nativeVal_t back, m, k;
	PyObject *fn = Script_Attr("Scorer", "MissionScorerBar_SetDefaults");
	if (!fn) {
		return 0;
	}
	Native_SetNone(&m);
	Native_SetNone(&k);
	byte ok = LvN_ModGet("Scorer", "MissionScorerBar_ColorBack", &back) && (main || LvN_ModGet("Scorer", "MissionScorerBar_ColorMain", &m)) &&
		(black || LvN_ModGet("Scorer", "MissionScorerBar_ColorBlack", &k));
	if (ok) {
		ok = Native_HookCall(fn, NULL, "vsvvv", id, item, &back, main ? main : &m, black ? black : &k);
	} else {
		Py_DECREF(fn);
	}
	Native_Free(&back);
	Native_Free(&m);
	Native_Free(&k);
	return ok;
}

byte Mode_PhoneCab(const char *use, byte targetFirst, const char *pcModule, nativeVal_t *out) {
	nativeVal_t pc, name;
	Native_SetNone(&pc);
	Native_SetNone(&name);
	byte ok = Mode_Import("PhoneCab") && Native_CallMod("PhoneCab", "GetFree", &pc, "") && (!pcModule || Mode_Put(pcModule, "pc", NULL, &pc)) &&
		(targetFirst || LvN_SetS(&pc, "UseFunc", use)) && Mode_VGet(&pc, "Name", &name) &&
		Native_CallMod("MissionsFuncs", "SetMissionTarget", NULL, "v", &name) && (!targetFirst || LvN_SetS(&pc, "UseFunc", use));
	Native_Free(&name);
	if (ok && out) {
		*out = pc;
	} else {
		Native_Free(&pc);
	}
	return ok;
}

byte Mode_SpawnCar(const char *name, byte nearest, nativeVal_t *out, nativeVal_t *spw) {
	nativeVal_t s, p, n;
	Native_SetNone(&s);
	Native_SetNone(&p);
	Native_SetNone(out);
	if (!Native_CallMod("OutMap", nearest ? "GetNearestFreeSpawnPos" : "GetFreeSpawnPos", &s, "")) {
		return 0;
	}
	Native_SetText(&n, name, (int)strlen(name));
	byte ok = (nearest || LvN_Item(&s, 0, &p)) && LvN_CreateAt(&n, nearest ? &s : &p, "Car", out);
	Native_Free(&n);
	Native_Free(&p);
	if (spw) {
		*spw = s;
	} else {
		Native_Free(&s);
	}
	return ok;
}

byte Mode_Drop(const char *module, const nativeVal_t *pos, const char *name, const char *model, const char *radius, long onlyUser,
	const char *onTake, const char *sound, const char *arrow) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *m = Mode_V(&t), *r = Mode_V(&t), *an = Mode_V(&t), *l2 = Mode_V(&t), *cam = Mode_V(&t);
	nativeVal_t *ln = Mode_V(&t);
	if (!LvN_CreateAt(Mode_S(&t, name), pos, "Item", l) || !Mode_G(module, model, m) || !Mode_VModel(l, m)) {
		return Mode_Done(&t, 0);
	}
	if (radius ? !Mode_G(module, radius, r) : (Native_SetInt(r, 3500), 0)) {
		return Mode_Done(&t, 0);
	}
	byte ok = Mode_VSet(l, "Radius", r) && LvN_SetI(l, "AppearsIfFree", 0) && LvN_SetI(l, "AutoFace", 0) && LvN_SetI(l, "OnlyUser", onlyUser) &&
		LvN_SetF(l, "rTime", 0.25) && LvN_SetS(l, "OnTake", onTake) && Mode_Import("Items") &&
		Native_CallMod("Items", "SetSound", NULL, "vs", l, sound);
	if (ok && arrow) {
		Native_SetText(an, arrow, (int)strlen(arrow));
	} else if (ok) {
		ok = Mode_VGet(l, "Name", ln) && Native_Arith('+', ln, Mode_S(&t, "CheckArrow"), an);
	}
	Mode_Ints3(cam, 0, 1000, 0);
	ok = ok && LvN_CreateAt(an, pos, "WithLife", l2) && Mode_VSet(l2, "CamTgt", cam) && Mode_VGet(l2, "Name", ln) &&
		Native_CallMod("MissionsFuncs", "SetMissionTarget", NULL, "v", ln);
	return Mode_Done(&t, ok);
}

byte Mode_Taken(const nativeVal_t *item, const nativeVal_t *arrow) {
	nativeVal_t itm, a, time;
	Native_SetNone(&itm);
	Native_SetNone(&a);
	if (!Mode_EntityH(item, &itm) || !LvN_SetI(&itm, "rTime", 20)) {
		Native_Free(&itm);
		return 0;
	}
	/* OBJLIB 4: the scheduler keeps the bound method */
	PyObject *del = Mode_VBound(&itm, "Del");
	Native_Free(&itm);
	if (!del) {
		return 0;
	}
	Native_SetFloat(&time, Mode_Time() + 5);
	byte ok = Mode_Schedule(&time, del, NULL, 0, NULL);
	Py_DECREF(del);
	ok = ok && Mode_EntityH(arrow, &a) && Mode_VDel(&a);
	Native_Free(&a);
	return ok;
}

/* module.var = i and = [] of another module */
static byte Mode_ModInt(const char *module, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_SetModVar(module, var, &v);
}

static byte Mode_ModEmpty(const char *module, const char *var) {
	nativeVal_t v;
	Native_SetEmpty(&v, NV_LIST);
	byte ok = Mode_SetModVar(module, var, &v);
	Native_Free(&v);
	return ok;
}

byte Mode_BossTraffic(byte comSats) {
	int done = 0;
	if (comSats) {
		nativeVal_t upper, lst;
		Native_SetNone(&lst);
		done = LvN_ModGet("Init", "UpperMap", &upper) && LvN_SaveList("ComSatsMissionsMapsFinished", "Map", &lst) ?
			Native_Contains(&lst, &upper) : -1;
		Native_Free(&upper);
		Native_Free(&lst);
		if (done < 0) {
			return 0;
		}
	}
	if (done ? !Mode_GVarSetI("MaxChasingShips", 0) || !Mode_GVarSetI("iTrafficNumber", 0) || !Mode_ModInt("OutPolice", "PoliceActive", 0) :
		!Mode_GVarSetI("iTrafficNumber", 5) || !Mode_GVarSetI("MaxChasingShips", 5)) {
		return 0;
	}
	return Mode_ModEmpty("Traffic", "AcNormalShips") && Mode_ModInt("Traffic", "NumNormal", 0) &&
		Mode_ModEmpty("Traffic", "AcMediumShips") && Mode_ModInt("Traffic", "NumMedium", 0) &&
		Mode_ModEmpty("Traffic", "AcHeavyShips") && Mode_ModInt("Traffic", "NumHeavy", 0);
}

byte Mode_AcShips(const char *module, const char *ships) {
	nativeVal_t v;
	if (!Mode_Import("Traffic")) {
		return 0;
	}
	SaveVar_Set("Traffic.AcShips", ships);
	byte ok = LvN_SaveList("Traffic.AcShips", module, &v) && Mode_SetModVar("Traffic", "AcShips", &v);
	Native_Free(&v);
	return ok;
}

byte Mode_DelSaveVars(const char *prefix) {
	MODE_SEAM("Scrap", "DelSaveVars");
	if (seam) {
		return Native_HookCall(hook, NULL, "s", prefix);
	}
	SaveVars_Delete(prefix);
	return 1;
}

byte Mode_DelGlobal(const char *module, const char *name) {
	/* OBJLIB 5: the globals are the module dict */
	PyObject *m = PyImport_ImportModule((char *)module);
	if (!m) {
		return 0;
	}
	int r = PyDict_DelItemString(PyModule_GetDict(m), (char *)name);
	Py_DECREF(m);
	if (r != 0) {
		PyErr_Clear();
		PyErr_SetString(PyExc_NameError, (char *)name);
		return 0;
	}
	return 1;
}

byte Mode_LaterV(const nativeVal_t *delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name) {
	nativeVal_t now, time, nm;
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&time);
	if (!Native_Arith('+', &now, delay, &time)) {
		return 0;
	}
	/* OBJLIB 4: the function the scheduler keeps is the one of the module (a mission may have replaced it) */
	PyObject *fn = Script_Global(module, func);
	if (!fn) {
		Native_Free(&time);
		return 0;
	}
	if (name) {
		Native_SetText(&nm, name, (int)strlen(name));
	}
	byte ok = Mode_Schedule(&time, fn, args, n, name ? &nm : NULL);
	if (name) {
		Native_Free(&nm);
	}
	Native_Free(&time);
	Py_DECREF(fn);
	return ok;
}

byte Mode_TestLineSeam(const nativeVal_t *from, const nativeVal_t *to, const nativeVal_t *list, nativeVal_t *out) {
	MODE_SEAM("Scrap", "TestLine");
	if (seam) {
		return Native_HookCall(hook, out, "vvv", from, to, list);
	}
	return Mode_TestLine(from, to, list, out);
}

/* the list of the save variable var with item added when it was not there: 1 added, 0 there, -1 with the error */
static int Mode_SaveAddTo(const char *module, const char *var, const char *item) {
	nativeVal_t lst, it;
	Native_SetText(&it, item, (int)strlen(item));
	int in = LvN_SaveList(var, module, &lst) ? Native_Contains(&lst, &it) : -1;
	int r = in < 0 ? -1 : in ? 0 : LvN_SaveAppend(var, &lst, &it) ? 1 : -1;
	Native_Free(&lst);
	Native_Free(&it);
	return r;
}

byte Mode_SaveAdd(const char *module, const char *var, const char *item) {
	return Mode_SaveAddTo(module, var, item) >= 0;
}

byte Mode_Blueprint(const char *module, const char *var, const char *item, const char *msg, byte delay) {
	nativeVal_t text;
	int r = Mode_SaveAddTo(module, var, item);
	if (r <= 0) {
		return r == 0;
	}
	if (msg) {
		Mode_Lang(msg, &text);
		byte ok = delay ? Native_CallMod("RaceMaker", "ShowSystemMessageDelay", NULL, "vd", &text, 0.25) :
			Mode_Import("RaceMaker") && Native_CallMod("RaceMaker", "ShowSystemMessage", NULL, "v", &text);
		Native_Free(&text);
		if (!ok) {
			return 0;
		}
	}
	SaveVar_Inc("NewBluePrintAvaliable", 1);
	return 1;
}

byte Mode_SpeechMsg(const nativeVal_t *cad) {
	nativeVal_t now, r, t;
	double d;
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&r);
	Native_SetNone(&t);
	byte ok = Native_CallMod("Speech", "CalculateSpeechTime", &r, "v", cad) && Native_Arith('+', &now, &r, &t) && Native_ArgDouble(&t, &d) &&
		Mode_MsgText(cad, d);
	Native_Free(&r);
	Native_Free(&t);
	return ok;
}

byte Mode_CallKw(const char *module, const char *name, nativeVal_t *out, const nativeVal_t *args, int n, PyObject *kw) {
	nativeVal_t t;
	PyObject *fn = kw ? Script_Attr(module, name) : NULL;
	Native_SetEmpty(&t, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(&t), &args[i]);
	}
	PyObject *a = fn ? Native_ToPy(&t) : NULL;
	Native_Free(&t);
	PyObject *r = a ? Script_CallObject(fn, a, kw) : NULL;
	if (r && out) {
		Native_Arg(r, out);
	}
	Py_XDECREF(r);
	Py_XDECREF(a);
	Py_XDECREF(fn);
	Py_XDECREF(kw);
	return r != NULL;
}

byte Mode_DoorFoe(const char *gk, const char *group) {
	PyObject *g = Script_Attr("GateKeeper", group);
	if (!g) {
		return 0;
	}
	byte ok = Native_CallMod("GateKeeper", "DoorFoe", NULL, "sO", gk, g);
	Py_DECREF(g);
	return ok;
}

byte Mode_PlaySoundSeam(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume) {
	MODE_SEAM("SSound", "PlaySound");
	if (seam) {
		return Native_HookCall(hook, NULL, "vvv", pos, name, volume);
	}
	return Mode_PlaySound(pos, name, volume);
}

byte Mode_LangV(const nativeVal_t *key, nativeVal_t *out) {
	const char *s;
	if (!Native_ArgStr(key, 1, &s)) {
		return 0;
	}
	Mode_Lang(s, out);
	return 1;
}

byte Mode_SAct(const char *fn, const char *name) {
	nativeVal_t r;
	byte ok = !strcmp(fn, "CreateClass") ? LvN_CreateClass(name, &r) : !strcmp(fn, "GetClass") ? LvN_GetClass(name, &r) :
		LvN_CreateAction(name, &r);
	Native_Free(&r);
	return ok;
}

byte Mode_SActSet(byte cls, const char *name, const char *s, long i) {
	nativeVal_t v;
	if (s) {
		Native_SetText(&v, s, (int)strlen(s));
	} else {
		Native_SetInt(&v, i);
	}
	byte ok = cls ? LvN_SetCls(name, &v) : LvN_SetAct(name, &v);
	Native_Free(&v);
	return ok;
}

byte Mode_PathSet(byte cls, const char *name, const char *rest) {
	nativeVal_t p, r, v;
	Native_SetNone(&v);
	Native_SetText(&r, rest, (int)strlen(rest));
	byte ok = LvN_ModGet("Init", "Path", &p) && Native_Arith('+', &p, &r, &v) && (cls ? LvN_SetCls(name, &v) : LvN_SetAct(name, &v));
	Native_Free(&p);
	Native_Free(&r);
	Native_Free(&v);
	return ok;
}

byte Mode_FromDummy(const nativeVal_t *dummy, const char *type, nativeVal_t *e) {
	nativeVal_t name, pos, a, ang;
	Native_SetNone(&name);
	Native_SetNone(&pos);
	Native_SetNone(&a);
	Native_SetEmpty(&ang, NV_TUPLE);
	byte ok = LvN_Item(dummy, 0, &name) && LvN_Item(dummy, 1, &pos) && LvN_CreateAt(&name, &pos, type, e) && LvN_Item(dummy, 2, &a) &&
		LvN_Item(&a, 0, Native_Append(&ang)) && LvN_Item(&a, 1, Native_Append(&ang));
	if (ok) {
		Native_SetInt(Native_Append(&ang), 0);
		ok = Mode_VSet(e, "Ang", &ang);
	}
	Native_Free(&name);
	Native_Free(&pos);
	Native_Free(&a);
	Native_Free(&ang);
	return ok;
}

byte Mode_CGCalls(const char *const *msgs) {
	nativeVal_t lib, twelfth, num;
	Mode_SaveVar("SuperDeal.Library", &lib);
	Native_SetText(&twelfth, "SuperDeal_Twelfth", 17);
	int other = Mode_Cmp(&lib, "!=", &twelfth);
	Native_Free(&lib);
	Native_Free(&twelfth);
	if (other <= 0) {
		return other == 0;
	}
	Native_SetNone(&num);
	byte ok = Native_CallMod("MissionsFuncs", "GetNumCrazyDealCompleted", &num, "");
	for (int i = 0; ok && i < 4; i++) {
		int eq = Mode_CmpI(&num, "==", i);
		if (eq) {
			ok = eq > 0 && LvN_Com(msgs[i], "CrazyGambler", NULL, NULL, NULL);
			break;
		}
	}
	Native_Free(&num);
	return ok;
}

byte Mode_AppendG(const char *module, const char *var, const char *gmod, const char *name) {
	PyObject *o = Script_Attr(module, var);
	PyObject *fn = o ? Mode_GlobalPy(gmod, name) : NULL;
	PyObject *r = fn ? Script_Method(o, "append", "(O)", fn) : NULL;
	Py_XDECREF(r);
	Py_XDECREF(fn);
	Py_XDECREF(o);
	return r != NULL;
}

byte Mode_GOp(const char *module, const char *name, char op, const nativeVal_t *v) {
	nativeVal_t g, r;
	Native_SetNone(&r);
	byte ok = Mode_G(module, name, &g) && Native_Arith(op, &g, v, &r) && Mode_Put(module, name, NULL, &r);
	Native_Free(&g);
	Native_Free(&r);
	return ok;
}

byte Mode_GOpI(const char *module, const char *name, char op, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_GOp(module, name, op, &v);
}

byte Mode_LoadSoundSeam(const nativeVal_t *file) {
	MODE_SEAM("SSound", "LoadSound");
	if (seam) {
		return Native_HookCall(hook, NULL, "v", file);
	}
	return Mode_LoadSound(file);
}

void Mode_Elems(nativeVal_t *v) {
	PyObject *o = v->type == NV_OBJ ? v->obj : NULL;
	if (!o || (!PyList_Check(o) && !PyTuple_Check(o))) {
		return;
	}
	/* OBJLIB 7: the items of a list held as one NV_OBJ (it holds handles) as Native_Arg makes them */
	nativeVal_t items;
	int n = PyObject_Length(o);
	Native_SetEmpty(&items, PyList_Check(o) ? NV_LIST : NV_TUPLE);
	for (int i = 0; i < n; i++) {
		PyObject *x = PySequence_GetItem(o, i);
		if (!x) {
			PyErr_Clear();
			break;
		}
		Native_Arg(x, Native_Append(&items));
		Py_DECREF(x);
	}
	Native_Free(v);
	*v = items;
}

int Mode_Loop(nativeVal_t *v) {
	Mode_Elems(v);
	return Native_ForCount(v);
}

/* ---- the hubs of batch J (Traffic, OutMap, Challenge, CharConversor, RaceMaker, Doors) ---- */

int Mode_Len(nativeVal_t *v) {
	Mode_Elems(v);
	return Native_Size(v);
}

byte Mode_Sub(const nativeVal_t *v, const nativeVal_t *key, nativeVal_t *out) {
	Native_SetNone(out);
	if (v->type != NV_OBJ) {
		return Native_Subscript(v, key, out);
	}
	/* OBJLIB 7: PyObject_GetItem of the object, the item as Native_Arg makes it */
	PyObject *k = Native_ToPy(key), *r = k ? PyObject_GetItem(v->obj, k) : NULL;
	Py_XDECREF(k);
	if (!r) {
		return 0;
	}
	Native_Arg(r, out);
	Py_DECREF(r);
	return 1;
}

byte Mode_RacePoint(const nativeVal_t *pos, long minDist, long maxDist, nativeVal_t *out) {
	double x[3];
	MODE_SEAM("SAI", "GetNextRacePoint");
	if (seam) {
		return Native_HookCall(hook, out, "vii", pos, minDist, maxDist);
	}
	if (!Mode_VecOf(pos, x)) {
		return Native_CallMod("SAI", "GetNextRacePoint", out, "vii", pos, minDist, maxDist);
	}
	vec3_t at = { (float)x[0], (float)x[1], (float)x[2] }, p;
	if (!AI_NextRacePoint(&at, (float)minDist, (float)maxDist, &p)) {
		Native_SetNone(out);
	} else {
		Native_SetVec(out, (double)p.x, (double)p.y, (double)p.z);
	}
	return 1;
}

byte Mode_TestSpherePos(const nativeVal_t *pos, long r, const char *list, nativeVal_t *out) {
	nativeVal_t c[3], rad, l;
	int n = 0;
	byte ok = 1;
	Native_SetNone(out);
	for (; ok && n < 3; n++) {
		ok = LvN_Item(pos, n, &c[n]);
	}
	if (!ok) {
		Native_Free(&c[--n]);
	}
	if (ok) {
		PyObject *hook;
		int seam = Native_Seam("Scrap", "TestSphere", &hook);
		Native_SetInt(&rad, r);
		Native_SetStr(&l, list);
		ok = seam < 0 ? 0 : seam ? Native_HookCall(hook, out, "vvvis", &c[0], &c[1], &c[2], r, list) :
			Mode_TestSphere(&c[0], &c[1], &c[2], &rad, &l, NULL, out);
	}
	while (n-- > 0) {
		Native_Free(&c[n]);
	}
	return ok;
}

byte Mode_DummyStartSeam(const char *name, long flag) {
	MODE_SEAM("Scrap", "StartDummySearch");
	if (seam) {
		return Native_HookCall(hook, NULL, "si", name, flag);
	}
	Mode_DummyStart(name, (int)flag);
	return 1;
}

byte Mode_DummyNextSeam(nativeVal_t *out) {
	MODE_SEAM("Scrap", "NextDummySearch");
	if (seam) {
		return Native_HookCall(hook, out, "");
	}
	Mode_DummyNext(out);
	return 1;
}

byte Mode_SetGrid(long x, long y, long z) {
	MODE_SEAM("Scrap", "SetGrid");
	if (seam) {
		return Native_HookCall(hook, NULL, "iii", x, y, z);
	}
	vec3_t size = { (float)x, (float)y, (float)z };
	Grid_Set(&g_pWorld->grid, &size);
	return 1;
}

byte Mode_EntitySeam(const nativeVal_t *name, nativeVal_t *out) {
	Native_SetNone(out);
	MODE_SEAM("Scrap", "GetEntity");
	if (seam) {
		return Native_HookCall(hook, out, "v", name);
	}
	return Mode_EntityH(name, out);
}

byte Mode_ActionSet(const char *name) {
	MODE_SEAM("SInput", "SetActionSet");
	if (seam) {
		return Native_HookCall(hook, NULL, "s", name);
	}
	Mode_SetActionSet(name);
	return 1;
}

byte Mode_SetMoneySeam(const nativeVal_t *money) {
	MODE_SEAM("Scrap", "SetMoney");
	if (seam) {
		return Native_HookCall(hook, NULL, "v", money);
	}
	return Mode_SetMoney(money);
}

byte Mode_IncSaveVar(const char *name) {
	MODE_SEAM("Scrap", "IncSaveVar");
	if (seam) {
		return Native_HookCall(hook, NULL, "s", name);
	}
	SaveVar_Inc(name, 1);
	return 1;
}

byte Mode_IsText(const nativeVal_t *v, const char *text) {
	return v->type == NV_STR && !strcmp(Native_Text(v), text);
}

byte Mode_TimeSpeed(double speed) {
	MODE_SEAM("Scrap", "SetTimeSpeed");
	if (seam) {
		return Native_HookCall(hook, NULL, "d", speed);
	}
	Mode_SetTimeSpeed(speed);
	return 1;
}

void Mode_CamName(int i, nativeVal_t *out) {
	const char *name = cWorld_GetCamName(g_pWorld, i);
	if (name) {
		Native_SetText(out, name, (int)strlen(name));
	} else {
		Native_SetNone(out);
	}
}

byte Mode_Floor(const nativeVal_t *y, nativeVal_t *out) {
	double d;
	if (!Native_ArgDouble(y, &d)) {
		return 0;
	}
	Native_SetInt(out, Floor_Get(&g_pWorld->floor, (float)d));
	return 1;
}

byte Mode_Abs(const nativeVal_t *v, nativeVal_t *out) {
	Native_SetNone(out);
	switch (v->type) {
	case NV_INT:
		if (v->i == LONG_MIN) {
			return Native_Raise(NE_OVERFLOW, "integer negation");
		}
		Native_SetInt(out, v->i < 0 ? -v->i : v->i);
		return 1;
	case NV_FLOAT:
		Native_SetFloat(out, fabs(v->f));
		return 1;
	case NV_LONG: {
		const char *d = Native_Text(v);
		d += *d == '-';
		Native_SetLong(out, d, (int)strlen(d));
		return 1;
	}
	case NV_OBJ: {
		/* OBJLIB 7: an object with its own abs */
		PyObject *r = PyNumber_Absolute(v->obj);
		if (!r) {
			return 0;
		}
		Native_Arg(r, out);
		Py_DECREF(r);
		return 1;
	}
	}
	return Native_Raise(NE_TYPE, "bad operand type for abs()");
}

byte Mode_CallArgs(const nativeVal_t *f, nativeVal_t *out, const nativeVal_t *args, int n) {
	nativeVal_t t;
	Native_SetEmpty(&t, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(&t), &args[i]);
	}
	/* OBJLIB 3, 4: the value called the Python way (a function, a bound method, or the TypeError of a value that is no
	 * callable) */
	PyObject *fn = Native_ToPy(f), *a = fn ? Native_ToPy(&t) : NULL;
	Native_Free(&t);
	PyObject *r = a ? Script_Invoke(fn, a) : NULL;
	Py_XDECREF(fn);
	if (!r) {
		return 0;
	}
	if (out) {
		Native_Arg(r, out);
	}
	Py_DECREF(r);
	return 1;
}
