/*
 * Native C helpers of the game mode ports (include/scrap/scriptmodes.h): the engine calls of Net, Ffa, the online and
 * split screen modes and the super deals on native values, the seams of the engine functions their case files record
 * or swap (OBJLIB 7), the module state they read and write in other modules (OBJLIB 5) and their adapters.
 */
#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/logic.h"
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
	PyObject *o[8] = { NULL };
	byte ok = 1;
	n = n < 8 ? n : 8;
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
	MODE_SEAM("SScorer", "Fade");
	if (seam) {
		PyObject *a = Py_BuildValue("(iiii)", from[0], from[1], from[2], from[3]);
		PyObject *b = a ? Py_BuildValue("(iiii)", to[0], to[1], to[2], to[3]) : NULL;
		byte ok = b && Native_HookCall(hook, NULL, "iOOd", id, a, b, time);
		if (!b) {
			Py_DECREF(hook);
		}
		Py_XDECREF(a);
		Py_XDECREF(b);
		return ok;
	}
	cScorer *sc = Scorer_Get((int)id);
	if (sc) {
		ScorerFade_Start(&sc->fade, Scorer_PackColor((uint32_t)from[0], (uint32_t)from[1], (uint32_t)from[2], (uint32_t)from[3]),
			Scorer_PackColor((uint32_t)to[0], (uint32_t)to[1], (uint32_t)to[2], (uint32_t)to[3]), (float)time);
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
