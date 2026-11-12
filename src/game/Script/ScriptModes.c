/*
 * Native C helpers of the game mode ports (include/scrap/scriptmodes.h): the engine calls of Net, Ffa, the online and
 * split screen modes and the super deals on native values, the seams of the engine functions their case files record
 * or swap (OBJLIB 7), the module state they read and write in other modules and their adapters.
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/logic.h"
#include "scrap/mathlib.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
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
	/* through the seam, fc-nm-po records it */
	nativeHook_t hook;
	int seam = Native_Seam("Scrap", "Preload3DObject", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(&hook, NULL, "v", name);
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
	PyObject *o[MODE_PRINT_MAX] = { NULL };
	byte ok = 1;
	n = n < MODE_PRINT_MAX ? n : MODE_PRINT_MAX;
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
	return v && ModVar_SetModAttr(module, var, v) == 0;
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
	if (!Native_Failed()) {
		Native_Raise(NE_SYSTEM, "error return without exception set");
	}
	return 0;
}

byte Mode_HGet(PyObject *o, const char *var, nativeVal_t *out) {
	modVarRef_t mv;
	if (PyEntity_Check(o)) {
		return Native_EntVar(PyEntity_Get(o), ((cPyEntity *)o)->ref->name, var, out);
	}
	if (ModVar_Of(o, &mv)) {
		/* a pack module's variable in the registry */
		nativeVal_t v;
		byte ok = ModVar_AttrVal(&mv, var, &v) &&
			(NObj_Plain(&v, out) || Native_Raise(NE_TYPE, "%s: not a value native C takes", var));
		Native_Free(&v);
		return ok;
	}
	PyObject *r = ModVar_ObjAttr(o, var);
	if (!r) {
		return Mode_AttrFail();
	}
	byte ok = Native_FromPyAny(r, out) || Native_Raise(NE_TYPE, "%s: not a value native C takes", var);
	Py_DECREF(r);
	return ok;
}

byte Mode_HSet(PyObject *o, const char *var, const nativeVal_t *v) {
	modVarRef_t mv;
	if (PyEntity_Check(o)) {
		return Native_EntSet(PyEntity_Get(o), ((cPyEntity *)o)->ref->name, var, v);
	}
	if (ModVar_Of(o, &mv)) {
		return ModVar_SetAttrVal(&mv, var, v);
	}
	PyObject *x = Native_ToPy(v);
	int r = x ? ModVar_SetObjAttr(o, var, x) : -1;
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
	if (o->type == NV_ENT) {
		return Native_EntVar(Native_Ent(o), Native_EntName(o), var, out);
	}
	if (NObj_Native(o)) {
		/* as Mode_HGet: the attribute as a plain value */
		nativeVal_t v;
		Native_SetNone(out);
		if (!NObj_GetAttr(o, var, &v)) {
			return Mode_AttrFail();
		}
		byte ok = NObj_Plain(&v, out) || Native_Raise(NE_TYPE, "%s: not a value native C takes", var);
		Native_Free(&v);
		return ok;
	}
	PyObject *po = Native_ToPy(o);
	byte ok = po && Mode_HGet(po, var, out);
	Py_XDECREF(po);
	return ok;
}

byte Mode_VSet(const nativeVal_t *o, const char *var, const nativeVal_t *v) {
	if (o->type == NV_ENT) {
		return Native_EntSet(Native_Ent(o), Native_EntName(o), var, v);
	}
	if (NObj_Native(o)) {
		return NObj_SetAttr(o, var, v);
	}
	PyObject *po = Native_ToPy(o);
	byte ok = po && Mode_HSet(po, var, v);
	Py_XDECREF(po);
	return ok;
}

/* h.SetModel(file) of a handle */
static byte Mode_RefModel(const cEntityRef *ref, const nativeVal_t *file) {
	cEntity *e = ref->link.target;
	const char *f;
	if (!e) {
		nativeVal_t x;
		return Native_EntVar(NULL, ref->name, "SetModel", &x);
	}
	/* "s|d" */
	if (!Native_ArgStr(file, 1, &f)) {
		return 0;
	}
	Entity_SetModel(e, f, 1.0f, 1.0f, 1.0f, 1);
	return 1;
}

byte Mode_HModel(PyObject *o, const nativeVal_t *file) {
	if (PyEntity_Check(o)) {
		return Mode_RefModel(((cPyEntity *)o)->ref, file);
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
/* dict[key] of the global dict of module, by reference into *out */
static byte Mode_At(const char *module, const char *dict, const nativeVal_t *key, nativeVal_t *out) {
	nativeVal_t d;
	Native_SetNone(out);
	byte ok = Mode_RefG(module, dict, &d) && Mode_RefSub(&d, key, out);
	Native_Free(&d);
	return ok;
}

byte Mode_AtGet(const char *module, const char *dict, const nativeVal_t *key, const char *var, nativeVal_t *out) {
	nativeVal_t o;
	byte ok = Mode_At(module, dict, key, &o) && Mode_VGet(&o, var, out);
	Native_Free(&o);
	return ok;
}

byte Mode_AtSet(const char *module, const char *dict, const nativeVal_t *key, const char *var, const nativeVal_t *v) {
	nativeVal_t o;
	byte ok = Mode_At(module, dict, key, &o) && Mode_VSet(&o, var, v);
	Native_Free(&o);
	return ok;
}

/* ---- engine calls through their seams ---- */

/* the seam of module.name: 0 engine, 1 *hook set, -1 error */
#define MODE_SEAM(module, name) \
	nativeHook_t hook; \
	int seam = Native_Seam(module, name, &hook); \
	if (seam < 0) { \
		return 0; \
	}

/* SNet.IsClient(), SNet.IsServer() through the seam: the truth of what the test put there */
static int Mode_NetIs(const char *name, byte engine) {
	nativeHook_t hook;
	int seam = Native_Seam("SNet", name, &hook);
	if (seam <= 0) {
		return seam < 0 ? -1 : engine;
	}
	nativeVal_t o;
	if (!Native_HookCall(&hook, &o, "")) {
		return -1;
	}
	int t = Mode_Truth(&o);
	Native_Free(&o);
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
		return Native_HookCall(&hook, NULL, "vv", id, s);
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
		return Native_HookCall(&hook, NULL, "v", id);
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
		return Native_HookCall(&hook, NULL, "vd", text, time);
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
		return Native_HookCall(&hook, NULL, "is", id, func);
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
		return Native_HookCall(&hook, NULL, "vvsv", id, item, var, v);
	}
	/* "issd" after "isss": the errors of the second form */
	int i;
	const char *name;
	return Native_ArgInt(id, &i) && Native_ArgStr(item, 2, &name) && Native_ScorerSet(i, name, var, v);
}

byte Mode_FadeV(long id, const int *from, const int *to, const nativeVal_t *time) {
	double d;
	MODE_SEAM("SScorer", "Fade");
	if (seam) {
		PyObject *a = Py_BuildValue("(iiii)", from[0], from[1], from[2], from[3]);
		PyObject *b = a ? Py_BuildValue("(iiii)", to[0], to[1], to[2], to[3]) : NULL;
		byte ok = b && Native_HookCall(&hook, NULL, "iOOv", id, a, b, time);
		if (!b) {
			Native_HookFree(&hook);
		}
		Py_XDECREF(a);
		Py_XDECREF(b);
		return ok;
	}
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

byte Mode_Fade(long id, const int *from, const int *to, double time) {
	/* the time as a float value: the recorders see the float "d" gave */
	nativeVal_t t;
	Native_SetFloat(&t, time);
	return Mode_FadeV(id, from, to, &t);
}

byte Mode_Bind(const nativeVal_t *id, const char *set, const char *action, const char *func) {
	MODE_SEAM("SInput", "Bind");
	if (seam) {
		return Native_HookCall(&hook, NULL, "vsss", id, set, action, func);
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
		return Native_HookCall(&hook, NULL, "v", name);
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

/* the tuple of the n values args (NULL with the error) */
static PyObject *Mode_ArgsTuple(const nativeVal_t *args, int n) {
	nativeVal_t t;
	Native_SetEmpty(&t, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(&t), &args[i]);
	}
	PyObject *params = Native_ToPy(&t);
	Native_Free(&t);
	return params;
}

/* func with the n values args at time on the world's scheduler: the port's own function as its entry on the values
 * (Port_Scheduled), anything else as the Python call on their tuple; 0 with the error of the tuple */
static byte Mode_SchedAdd(float time, PyObject *func, const nativeVal_t *args, int n, const char *name) {
	portSite_t *site = n <= PORT_ARGS ? Port_Scheduled(func) : NULL;
	if (site) {
		Scheduler_AddEntry(&g_pWorld->scheduler, time, site, args, n, name);
		return 1;
	}
	PyObject *params = Mode_ArgsTuple(args, n);
	if (!params) {
		return 0;
	}
	Scheduler_Add(&g_pWorld->scheduler, time, func, params, name);
	Py_DECREF(params);
	return 1;
}

byte Mode_Schedule(const nativeVal_t *time, PyObject *func, const nativeVal_t *args, int n, const nativeVal_t *name) {
	nativeHook_t hook;
	int seam = Native_Seam("Scrap", "AddScheduledFunc", &hook);
	byte ok = 0;
	if (seam > 0) {
		/* the recorder gets the arguments as their tuple */
		PyObject *params = Mode_ArgsTuple(args, n);
		if (!params) {
			Native_HookFree(&hook);
			return 0;
		}
		ok = name ? Native_HookCall(&hook, NULL, "vOOv", time, func, params, name) :
			Native_HookCall(&hook, NULL, "vOO", time, func, params);
		Py_DECREF(params);
	} else if (seam == 0) {
		/* "dOO|s" */
		double d;
		const char *s = "";
		ok = Native_ArgDouble(time, &d) && (!name || Native_ArgStr(name, 4, &s)) &&
			Mode_SchedAdd((float)d, func, args, n, s);
	}
	return ok;
}

byte Mode_LaterPy(double delay, PyObject *func, const nativeVal_t *args, int n, const char *name) {
	nativeVal_t t, nm;
	Native_SetFloat(&t, Mode_Time() + delay);
	if (name) {
		Native_SetText(&nm, name, (int)strlen(name));
	}
	byte ok = Mode_Schedule(&t, func, args, n, name ? &nm : NULL);
	if (name) {
		Native_Free(&nm);
	}
	return ok;
}

/* Mode_Later with the function of the module as its global (global 1) or as its attribute */
static byte Mode_LaterBy(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name,
	byte global) {
	/* the function scheduled is the one of the module now (a mission may have replaced it): its entry while it is the
	 * port's own (Mode_Schedule) */
	PyObject *fn = global ? Script_Global(module, func) : Script_Attr(module, func);
	if (!fn) {
		return 0;
	}
	byte ok = Mode_LaterPy(delay, fn, args, n, name);
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
		return Native_HookCall(&hook, out, "vv", a, b);
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
		return Native_HookCall(&hook, NULL, "v", text);
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
		return Native_HookCall(&hook, NULL, "vv", sound, volume);
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
		/* the errors of "(ddd)(ddd)" from the engine function */
		return PORT_CALL(SVec, Add, out, "vv", a, b);
	}
	Native_SetVec(out, x[0] + y[0], x[1] + y[1], x[2] + y[2]);
	return 1;
}

byte Mode_Rotate(const nativeVal_t *v, const nativeVal_t *rot, nativeVal_t *out) {
	double x[3], r[3];
	if (!Mode_Vec3(v, x) || !Mode_Vec3(rot, r)) {
		return PORT_CALL(SVec, Rotate3D, out, "vv", v, rot);
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
		return Native_HookCall(&hook, out, "v", name);
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
		return Native_HookCall(&hook, out, "");
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
		return PORT_CALL(SVec, Sub, out, "vv", a, b);
	}
	Native_SetVec(out, x[0] - y[0], x[1] - y[1], x[2] - y[2]);
	return 1;
}

byte Mode_VecMod(const nativeVal_t *a, nativeVal_t *out) {
	double x[3];
	if (!Mode_Vec3(a, x)) {
		return PORT_CALL(SVec, Mod, out, "v", a);
	}
	Native_SetFloat(out, sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]));
	return 1;
}

byte Mode_VecNorm(const nativeVal_t *a, nativeVal_t *out) {
	double x[3];
	if (!Mode_Vec3(a, x)) {
		return PORT_CALL(SVec, Norm, out, "v", a);
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
		return PORT_CALL(SVec, ModSqr, out, "v", a);
	}
	Native_SetFloat(out, x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
	return 1;
}

byte Mode_VecProd(const nativeVal_t *a, const nativeVal_t *f, nativeVal_t *out) {
	double x[3], d;
	if (!Mode_Vec3(a, x) || !Mode_Num(f, &d)) {
		return PORT_CALL(SVec, Prod, out, "vv", a, f);
	}
	Native_SetVec(out, x[0] * d, x[1] * d, x[2] * d);
	return 1;
}

byte Mode_PlaySound(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume) {
	double p[3], vol;
	const char *s;
	if (!Mode_Vec3(pos, p) || !Mode_Str(name, &s) || !Mode_Num(volume, &vol)) {
		return PORT_CALL(SSound, PlaySound, NULL, "vvv", pos, name, volume);
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
		return exclude ? PORT_CALL(Scrap, TestSphere, out, "vvvvvv", x, y, z, r, list, exclude) :
			PORT_CALL(Scrap, TestSphere, out, "vvvvv", x, y, z, r, list);
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
		return PORT_CALL(Scrap, TestLine, out, "vvv", from, to, list);
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
		return PORT_CALL(Scrap, SphereCall, NULL, "vvvvvv", x, y, z, r, list, func);
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
		return Native_HookCall(&hook, out, "i", 0L);
	}
	return Mode_EntHandle(Mode_Usr(), out);
}

byte Mode_EntHandle(cEntity *e, nativeVal_t *out) {
	/* the handle is made by the name, as Scrap.GetEntity makes it */
	if (!e || !Native_SetEnt(out, e->name)) {
		Native_SetNone(out);
	}
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
	if (h->type == NV_ENT) {
		nativeVal_t x;
		*e = Native_Ent(h);
		return *e ? 1 : Native_EntVar(NULL, Native_EntName(h), method, &x);
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
	if (h->type == NV_ENT) {
		return Mode_RefModel(h->ent, file);
	}
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
	nativeHook_t hook;
	int seam = Native_Seam("Scrap", "Preload3DObject", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(&hook, NULL, "viii", name, 1L, 1L, 1L);
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
	nativeHook_t hook;
	int seam = Native_Seam("Scrap", "GetAlarm", &hook);
	if (!seam) {
		return g_pWorld->alarm.on != 0;
	}
	if (seam < 0) {
		return -1;
	}
	nativeVal_t a = { 0 }, k, on = { 0 };
	Native_SetInt(&k, 0);
	int t = Native_HookCall(&hook, &a, "") && Native_Subscript(&a, &k, &on) ? Native_True(&on) : -1;
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
		return PORT_CALL(SScorer, Set, NULL, "issv", (long)id, item, var, v);
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

byte Mode_VRead(const nativeVal_t *h, const char *var, nativeVal_t *slot) {
	Native_Free(slot);
	return Mode_VGet(h, var, slot);
}

PyObject *Mode_GlobalPy(const char *module, const char *name) {
	return Script_Global(module, name);
}

byte Mode_SetModVar(const char *module, const char *var, const nativeVal_t *v) {
	return ModVar_SetModAttrVal(module, var, v);
}

byte Mode_ModVar(const char *module, const char *var, nativeVal_t *out) {
	nativeVal_t o;
	if (!ModVar_ModAttrVal(module, var, &o)) {
		return 0;
	}
	byte ok = NObj_Plain(&o, out) || Native_Raise(NE_TYPE, "%s.%s: not a value native C takes", module, var);
	Native_Free(&o);
	return ok;
}

/* the value of a variable of module: its global (NameError), or its attribute for another module */
static byte Mode_VarOf(const char *module, const char *var, byte global, nativeVal_t *out) {
	return global ? ModVar_GlobalVal(module, var, out) : ModVar_ModAttrVal(module, var, out);
}

/* the value holds a list or dict object (native data or a view): the in-place changes go to it natively */
static byte Mode_IsContainer(const nativeVal_t *o) {
	return NObj_Kind(o) == NO_LIST || NObj_Kind(o) == NO_DICT;
}

/* var[key] = v (v NULL: del var[key]) on the dict object of a module variable (its global, or its attribute for
 * another module), then the dict read back into *d (NULL: not kept). A value that is no list or dict: the item store
 * of Python, with its error */
static byte Mode_DictChange(const char *module, const char *var, byte global, nativeVal_t *d, const nativeVal_t *key,
	const nativeVal_t *v) {
	nativeVal_t o;
	byte ok;
	if (!Mode_VarOf(module, var, global, &o)) {
		return 0;
	}
	if (Mode_IsContainer(&o)) {
		ok = v ? NObj_SetItem(&o, key, v) : NObj_DelItem(&o, key);
	} else {
		PyObject *po = Native_ToPy(&o), *k = po ? Native_ToPy(key) : NULL, *x = k && v ? Native_ToPy(v) : NULL;
		ok = k && (!v || x) && (v ? PyObject_SetItem(po, k, x) : PyObject_DelItem(po, k)) == 0;
		Py_XDECREF(x);
		Py_XDECREF(k);
		Py_XDECREF(po);
	}
	if (ok && d) {
		Native_Free(d);
		ok = NObj_Plain(&o, d) || Native_Raise(NE_TYPE, "%s.%s: not a value native C takes", module, var);
	}
	Native_Free(&o);
	return ok;
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
	Native_SetNone(out);
	return Native_VarTake(module, name, 1, NV_ANY, out);
}

static byte Mode_ObjMethod(const nativeVal_t *o, const char *method, const nativeVal_t *a, int n, nativeVal_t *out);

/* var.method(a[0], ..., a[n - 1]) of a variable of a module (n at most 2) into *out (Native_Free it): on a list or dict
 * object natively, on another value through Python (Script_Method, its errors); 0 with the error */
static byte Mode_VarMethod(const char *module, const char *var, byte global, const char *method, const nativeVal_t *a,
	int n, nativeVal_t *out) {
	nativeVal_t o;
	Native_SetNone(out);
	if (!Mode_VarOf(module, var, global, &o)) {
		return 0;
	}
	byte ok = Mode_ObjMethod(&o, method, a, n, out);
	Native_Free(&o);
	return ok;
}

/* o.method(a[0], ..., a[n - 1]) (n at most 2): on a list or dict object natively, on another value through Python */
static byte Mode_ObjMethod(const nativeVal_t *o, const char *method, const nativeVal_t *a, int n, nativeVal_t *out) {
	byte ok;
	Native_SetNone(out);
	if (Mode_IsContainer(o)) {
		ok = NObj_CallMethod(o, method, a, n, out);
	} else {
		PyObject *po = Native_ToPy(o), *x = po && n > 0 ? Native_ToPy(&a[0]) : NULL;
		PyObject *y = x && n > 1 ? Native_ToPy(&a[1]) : NULL, *r = NULL;
		if (po && (n < 1 || x) && (n < 2 || y)) {
			r = n == 0 ? Script_Method(po, method, "()") : n == 1 ? Script_Method(po, method, "(O)", x) :
				Script_Method(po, method, "(OO)", x, y);
		}
		ok = r != NULL;
		if (r) {
			Native_Arg(r, out);
		}
		Py_XDECREF(r);
		Py_XDECREF(y);
		Py_XDECREF(x);
		Py_XDECREF(po);
	}
	return ok;
}

/* var.method() of a variable of a module (keys, values) as a native value */
static byte Mode_VarList(const char *module, const char *var, byte global, const char *method, nativeVal_t *out) {
	nativeVal_t r;
	if (!Mode_VarMethod(module, var, global, method, NULL, 0, &r)) {
		return 0;
	}
	byte ok = NObj_Plain(&r, out) || Native_Raise(NE_TYPE, "%s.%s.%s(): not a value native C takes", module, var, method);
	Native_Free(&r);
	return ok;
}

/* var.method(v) of a variable of a module (append, remove), in place */
byte Mode_VarCall(const char *module, const char *var, byte global, const char *method, const nativeVal_t *v) {
	nativeVal_t r;
	byte ok = Mode_VarMethod(module, var, global, method, v, 1, &r);
	Native_Free(&r);
	return ok;
}

byte Mode_Keys(const char *module, const char *var, byte global, nativeVal_t *out) {
	return Mode_VarList(module, var, global, "keys", out);
}

int Mode_HasKey(const char *module, const char *var, byte global, const nativeVal_t *key) {
	nativeVal_t r;
	int t = Mode_VarMethod(module, var, global, "has_key", key, 1, &r) ? Native_True(&r) : -1;
	Native_Free(&r);
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
	return Mode_VarCall(module, var, 1, "append", v);
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
	if (!ModVar_SetGlobalVal(module, var, v)) {
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
	if (Native_EvalText(module, &text, &prev) && Mode_G(module, "ModesMap", &modes) && Native_GVarGet("ServerType", &type) &&
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
	return Mode_VarList(module, var, global, "values", out);
}

byte Mode_Remove(const char *module, const char *var, const nativeVal_t *v) {
	return Mode_VarCall(module, var, 1, "remove", v);
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
	byte imported;
	PyObject *v = ModVar_Lookup(module, name, &imported);
	if (imported && !v) {
		Native_Raise(NE_IMPORT, "cannot import name %.230s", name);
	}
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
		return PORT_CALL(SScorer, Get2DPos, out, "v", pos);
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
		return Native_HookCall(&hook, NULL, "v", level);
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
		/* the entry while fn is a port's own function (Port_Values) */
		if (ok) {
			ok = Port_Values(PORT_VSITE(), fn, v, n, NULL, NULL);
		}
		Py_DECREF(fn);
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
	return (who ? PORT_CALL(Speech, NISpeech, NULL, "s", who) : PORT_CALL(Speech, NISpeech, NULL, "")) &&
		PORT_CALL(Speech, AutoAddNI, NULL, "s", key) && PORT_CALL(Speech, RunNI, NULL, "");
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
		return Native_HookCall(&hook, NULL, "v", lock);
	}
	int i;
	if (!Native_ArgInt(lock, &i)) {
		return 0;
	}
	Alarm_SetLock(&g_pWorld->alarm, i != 0);
	return 1;
}

byte Mode_SetLockAlarmI(long lock) {
	nativeVal_t v;
	Native_SetInt(&v, lock);
	return Mode_SetLockAlarm(&v);
}

byte Mode_VObj(const nativeVal_t *o, const char *var, nativeVal_t *out) {
	if (o->type == NV_ENT) {
		return Mode_VGet(o, var, out);
	}
	if (NObj_Native(o)) {
		nativeVal_t v;
		Native_SetNone(out);
		if (!NObj_GetAttr(o, var, &v)) {
			return Mode_AttrFail();
		}
		NObj_ArgOf(&v, out);
		Native_Free(&v);
		return 1;
	}
	PyObject *po = Native_ToPy(o);
	if (!po) {
		return 0;
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

byte Mode_CharRes(const nativeVal_t *key, nativeVal_t *out) {
	nativeVal_t res;
	Native_SetNone(out);
	byte ok = Mode_RefMod("CharsNPC", "CharRes", &res) && Mode_RefSub(&res, key, out);
	Native_Free(&res);
	return ok;
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
	if (c && Native_Failed()) {
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
	return how ? PORT_CALL(Speech, OnSpch, NULL, "sss", msg, ans, how) : PORT_CALL(Speech, OnSpch, NULL, "ss", msg, ans);
}

byte Mode_AnsLst(const char *const *texts, int n) {
	nativeVal_t l;
	Mode_Texts(&l, texts, n);
	byte ok = PORT_CALL(Speech, StdAnsLst, NULL, "v", &l);
	Native_Free(&l);
	return ok;
}

byte Mode_MMDesc(const char *desc, long flag) {
	return PORT_CALL(SaveGame, MMDesc, NULL, "si", desc, flag);
}

byte Mode_WayPoint(void) {
	return PORT_CALL(SaveGame, QWayPoint, NULL, "");
}

byte Mode_Target(const char *name) {
	return name ? PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "s", name) :
		PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "O", Py_None);
}

byte Mode_Connect(const char *a, const char *b, byte pair) {
	const char *names[2] = { a, b };
	nativeVal_t l1, l2;
	Mode_Texts(&l1, names, pair ? 2 : 1);
	Mode_Texts(&l2, names + 1, pair ? 0 : 1);
	byte ok = pair ? PORT_CALL(Elevator, ConnectElevators, NULL, "vvi", &l1, &l2, 0L) :
		PORT_CALL(Elevator, ConnectElevators, NULL, "vv", &l1, &l2);
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

byte Mode_GSetI(const char *module, const char *var, const char *attr, long i) {
	nativeVal_t h;
	byte ok = Mode_G(module, var, &h) && LvN_SetI(&h, attr, i);
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

byte Mode_ScorerSetI(const nativeVal_t *id, const char *item, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return LvN_ScorerSet(id, item, var, &v);
}

/* SScorer.Add(id, item, type) with a C item name */
byte Mode_ScorerAddS(const nativeVal_t *id, const char *item, const char *type) {
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

byte Mode_MissionSprite(const nativeVal_t *id, const char *item, const nativeVal_t *x, const nativeVal_t *y, long alpha, long r,
	long g, long b) {
	return Mode_ScorerAddS(id, item, "Sprite") && LvN_ScorerSet(id, item, "X", x) && LvN_ScorerSet(id, item, "Y", y) &&
		Mode_ScorerSetI(id, item, "ScaleX", 30) && Mode_ScorerSetI(id, item, "ScaleY", 19) && Mode_ScorerSetI(id, item, "Unit", 1) &&
		Mode_ScorerSetI(id, item, "Alpha", alpha) && Mode_ScorerSetI(id, item, "Red", r) && Mode_ScorerSetI(id, item, "Green", g) &&
		Mode_ScorerSetI(id, item, "Blue", b);
}

byte Mode_BarDefaults(const nativeVal_t *id, const char *item, const nativeVal_t *main, const nativeVal_t *black) {
	nativeVal_t back, m, k;
	Native_SetNone(&m);
	Native_SetNone(&k);
	/* the function is looked up after the colours (the script before them: the same unless both are gone) */
	byte ok = LvN_ModGet("Scorer", "MissionScorerBar_ColorBack", &back) && (main || LvN_ModGet("Scorer", "MissionScorerBar_ColorMain", &m)) &&
		(black || LvN_ModGet("Scorer", "MissionScorerBar_ColorBlack", &k)) &&
		PORT_CALL(Scorer, MissionScorerBar_SetDefaults, NULL, "vsvvv", id, item, &back, main ? main : &m, black ? black : &k);
	Native_Free(&back);
	Native_Free(&m);
	Native_Free(&k);
	return ok;
}

byte Mode_PhoneCab(const char *use, byte targetFirst, const char *pcModule, nativeVal_t *out) {
	nativeVal_t pc, name;
	Native_SetNone(&pc);
	Native_SetNone(&name);
	byte ok = Mode_Import("PhoneCab") && PORT_CALL(PhoneCab, GetFree, &pc, "") && (!pcModule || Mode_Put(pcModule, "pc", NULL, &pc)) &&
		(targetFirst || LvN_SetS(&pc, "UseFunc", use)) && Mode_VGet(&pc, "Name", &name) &&
		PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &name) && (!targetFirst || LvN_SetS(&pc, "UseFunc", use));
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
	if (!(nearest ? PORT_CALL(OutMap, GetNearestFreeSpawnPos, &s, "") : PORT_CALL(OutMap, GetFreeSpawnPos, &s, ""))) {
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
		PORT_CALL(Items, SetSound, NULL, "vs", l, sound);
	if (ok && arrow) {
		Native_SetText(an, arrow, (int)strlen(arrow));
	} else if (ok) {
		ok = Mode_VGet(l, "Name", ln) && Native_Arith('+', ln, Mode_S(&t, "CheckArrow"), an);
	}
	Mode_Ints3(cam, 0, 1000, 0);
	ok = ok && LvN_CreateAt(an, pos, "WithLife", l2) && Mode_VSet(l2, "CamTgt", cam) && Mode_VGet(l2, "Name", ln) &&
		PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", ln);
	return Mode_Done(&t, ok);
}

byte Mode_Taken(const nativeVal_t *item, const nativeVal_t *arrow) {
	nativeVal_t itm, a;
	Native_SetNone(&itm);
	Native_SetNone(&a);
	byte ok = Mode_EntityH(item, &itm) && Mode_DelLater(&itm) && Mode_EntityH(arrow, &a) && Mode_VDel(&a);
	Native_Free(&itm);
	Native_Free(&a);
	return ok;
}

byte Mode_ModEmpty(const char *module, const char *var) {
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
	if (done ? !Mode_GVarSetI("MaxChasingShips", 0) || !Mode_GVarSetI("iTrafficNumber", 0) || !LvN_ModSetI("OutPolice", "PoliceActive", 0) :
		!Mode_GVarSetI("iTrafficNumber", 5) || !Mode_GVarSetI("MaxChasingShips", 5)) {
		return 0;
	}
	return Mode_ModEmpty("Traffic", "AcNormalShips") && LvN_ModSetI("Traffic", "NumNormal", 0) &&
		Mode_ModEmpty("Traffic", "AcMediumShips") && LvN_ModSetI("Traffic", "NumMedium", 0) &&
		Mode_ModEmpty("Traffic", "AcHeavyShips") && LvN_ModSetI("Traffic", "NumHeavy", 0);
}

byte Mode_DelSaveVars(const char *prefix) {
	MODE_SEAM("Scrap", "DelSaveVars");
	if (seam) {
		return Native_HookCall(&hook, NULL, "s", prefix);
	}
	SaveVars_Delete(prefix);
	return 1;
}

byte Mode_DelGlobal(const char *module, const char *name) {
	return ModVar_DelGlobal(module, name) == 0;
}

byte Mode_LaterV(const nativeVal_t *delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name) {
	nativeVal_t now, time, nm;
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&time);
	if (!Native_Arith('+', &now, delay, &time)) {
		return 0;
	}
	/* the function scheduled is the one of the module now (a mission may have replaced it): its entry while it is the
	 * port's own (Mode_Schedule) */
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
		return Native_HookCall(&hook, out, "vvv", from, to, list);
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
		byte ok = delay ? PORT_CALL(RaceMaker, ShowSystemMessageDelay, NULL, "vd", &text, 0.25) :
			Mode_Import("RaceMaker") && PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text);
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
	byte ok = PORT_CALL(Speech, CalculateSpeechTime, &r, "v", cad) && Native_Arith('+', &now, &r, &t) && Native_ArgDouble(&t, &d) &&
		Mode_MsgText(cad, d);
	Native_Free(&r);
	Native_Free(&t);
	return ok;
}

byte Mode_DoorFoe(const char *gk, const char *group) {
	PyObject *g = Script_Attr("GateKeeper", group);
	if (!g) {
		return 0;
	}
	byte ok = PORT_CALL(GateKeeper, DoorFoe, NULL, "sP", gk, g);
	Py_DECREF(g);
	return ok;
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

byte Mode_CGCalls(const char *const *msgs, int n) {
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
	byte ok = PORT_CALL(MissionsFuncs, GetNumCrazyDealCompleted, &num, "");
	for (int i = 0; ok && i < n; i++) {
		int eq = Mode_CmpI(&num, "==", i);
		if (eq) {
			ok = eq > 0 && LvN_Com(msgs[i], "CrazyGambler", NULL, NULL, NULL);
			break;
		}
	}
	Native_Free(&num);
	return ok;
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
		return Native_HookCall(&hook, NULL, "v", file);
	}
	return Mode_LoadSound(file);
}

/* the result r of an operation on a native object (taken over) as Native_Arg makes a result: 1 with *out, 0 */
static byte Mode_NativeOut(byte ok, nativeVal_t *r, nativeVal_t *out) {
	if (ok) {
		NObj_ArgOf(r, out);
	}
	Native_Free(r);
	return ok;
}

void Mode_Elems(nativeVal_t *v) {
	if (NObj_Kind(v) == NO_LIST && NObj_Native(v)) {
		/* the items of a list object as Native_Arg makes them */
		nativeVal_t items, k, x;
		int n = NObj_Len(v);
		Native_SetEmpty(&items, NV_LIST);
		for (int i = 0; i < n; i++) {
			Native_SetInt(&k, i);
			if (!Mode_NativeOut(NObj_GetItem(v, &k, &x), &x, Native_Append(&items))) {
				Native_ErrClear();
				break;
			}
		}
		Native_Free(v);
		*v = items;
		return;
	}
	PyObject *o = Native_IsObj(v) ? Native_PyOf(v) : NULL;
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
			Native_ErrClear();
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

/* ---- helpers of batch I (the super deals and the native level files of batch I) ---- */

byte Mode_VSetI(const nativeVal_t *h, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_VSet(h, var, &v);
}

byte Mode_VSetS(const nativeVal_t *h, const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	byte ok = Mode_VSet(h, var, &v);
	Native_Free(&v);
	return ok;
}

byte Mode_VSetF(const nativeVal_t *h, const char *var, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	return Mode_VSet(h, var, &v);
}

byte Mode_Item(const nativeVal_t *x, long i, nativeVal_t *out) {
	nativeVal_t k, r;
	Native_SetNone(out);
	if (NObj_Native(x)) {
		Native_SetInt(&k, i);
		return Mode_NativeOut(NObj_GetItem(x, &k, &r), &r, out);
	}
	if (Native_IsObj(x)) {
		/* OBJLIB 7 */
		PyObject *n = PyInt_FromLong(i), *r = n ? PyObject_GetItem(Native_PyOf(x), n) : NULL;
		Py_XDECREF(n);
		if (!r) {
			return 0;
		}
		Native_Arg(r, out);
		Py_DECREF(r);
		return 1;
	}
	Native_SetInt(&k, i);
	return Native_Subscript(x, &k, out);
}

byte Mode_ModGet(const char *module, const char *var, nativeVal_t *out) {
	nativeVal_t o;
	if (!ModVar_ModAttrVal(module, var, &o)) {
		Native_SetNone(out);
		return 0;
	}
	NObj_ArgOf(&o, out);
	Native_Free(&o);
	return 1;
}

byte Mode_ModNone(const char *module, const char *var) {
	return Mode_SetModPy(module, var, Py_None);
}

/* SScorer.Get(id, item, var) (the TypeError of "i" for an id that is no number) */
byte Mode_ScorerGetV(const nativeVal_t *id, const char *item, const char *var, nativeVal_t *out) {
	int i;
	Native_SetNone(out);
	if (!Native_ArgInt(id, &i)) {
		return 0;
	}
	Mode_ScorerGet(i, item, var, out);
	return 1;
}

/* the text item of a mission scorer in Horatio with a shadow at the left (Lv_Text): X, y, text, visible (NULL: not
 * set) */
byte Mode_LvText(const nativeVal_t *id, const char *item, const nativeVal_t *y, const nativeVal_t *text, const nativeVal_t *visible) {
	nativeVal_t x = { 0 };
	Native_SetNone(&x);
	byte ok = LvN_TextX(0, &x) && LvN_MissionText(id, item, "Text", &x, y, "Horatio", "Shadow", text, "Left", visible);
	Native_Free(&x);
	return ok;
}

/* Mode_LvText at Scorer.MissionScorerText_Y + dy with Scrap.GetLangStr(key) + ":" (visible: Visible 1) */
byte Mode_LvLabel(const nativeVal_t *id, const char *item, long dy, const char *key, byte visible) {
	nativeVal_t y = { 0 }, text = { 0 }, one = { 0 };
	Native_SetNone(&y);
	Native_SetInt(&one, 1);
	byte ok = LvN_TextY(dy, &y);
	if (ok) {
		LvN_Label(key, &text);
		ok = Mode_LvText(id, item, &y, &text, visible ? &one : NULL);
		Native_Free(&text);
	}
	Native_Free(&y);
	return ok;
}

/* Scrap.SetSaveVar(name, text) */
byte Mode_SaveS(const char *name, const char *text) {
	const char *const pairs[] = { name, text, NULL };
	Mode_Saves(pairs);
	return 1;
}

/* module.var = src.name of another module as the object it is (a function of that module) */
byte Mode_ModTo(const char *module, const char *var, const char *src, const char *name) {
	PyObject *o = Script_Attr(src, name);
	byte ok = o && Mode_SetModPy(module, var, o);
	Py_XDECREF(o);
	return ok;
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, Speech.<func>, args[, name]) of n values (name NULL: none) */
byte Mode_LaterSpeech(const char *func, const nativeVal_t *delay, const nativeVal_t *args, int n, const char *name) {
	nativeVal_t now = { 0 }, at = { 0 }, nm = { 0 };
	PyObject *fn = Script_Attr("Speech", func);
	if (!fn) {
		return 0;
	}
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&at);
	if (name) {
		LvN_Str(&nm, name);
	}
	byte ok = Native_Arith('+', &now, delay, &at) && Mode_Schedule(&at, fn, args, n, name ? &nm : NULL);
	if (name) {
		Native_Free(&nm);
	}
	Native_Free(&at);
	Py_DECREF(fn);
	return ok;
}

byte Mode_LaterComF(double delay, const nativeVal_t *args, int n) {
	nativeVal_t d = { 0 };
	Native_SetFloat(&d, delay);
	return Mode_LaterSpeech("AddRemoteCom", &d, args, n, NULL);
}

/* `num` + "/" + `max` of two globals of module (num read first) */
byte Mode_CountText(const char *module, const char *num, const char *max, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *m = Mode_V(&t), *rn = Mode_V(&t), *rm = Mode_V(&t);
	byte ok = Mode_G(module, num, n) && Mode_G(module, max, m);
	if (ok) {
		Mode_Repr(n, rn);
		Mode_Repr(m, rm);
		ok = Mode_Cat(&t, out, 3, rn, Mode_S(&t, "/"), rm);
	}
	return Mode_Done(&t, ok);
}

/* the timer of a mission scorer counting TimeSpecialUse, with the function of its end (visible: Visible 1) */
byte Mode_LvTimer(const nativeVal_t *id, const char *item, const nativeVal_t *x, const nativeVal_t *y, const char *expired, byte visible) {
	return Mode_ScorerAddS(id, item, "Timer") && LvN_ScorerSet(id, item, "X", x) && LvN_ScorerSet(id, item, "Y", y) &&
		LvN_ScorerSetS(id, item, "Align", "Left") && LvN_ScorerSetS(id, item, "TimerType", "TimeSpecialUse") &&
		LvN_ScorerSetS(id, item, "OnTimeExpired", expired) && Mode_TextColour(id, item) && (!visible || Mode_ScorerSetI(id, item, "Visible", 1));
}

/* bp = eval(Scrap.GetSaveVar(var)); bp = bp + [a, b]; Scrap.SetSaveVar(var, `bp`) (eval in the globals of module) */
byte Mode_SaveCat(const char *module, const char *var, const char *const *texts, int n) {
	nativeVal_t bp = { 0 }, add = { 0 }, r = { 0 };
	Native_SetNone(&r);
	Mode_Texts(&add, texts, n);
	byte ok = LvN_SaveList(var, module, &bp) && Native_Arith('+', &bp, &add, &r);
	if (ok) {
		LvN_SaveRepr(var, &r);
	}
	Native_Free(&bp);
	Native_Free(&add);
	Native_Free(&r);
	return ok;
}

byte Mode_SaveCat2(const char *module, const char *var, const char *a, const char *b) {
	const char *const texts[] = { a, b };
	return Mode_SaveCat(module, var, texts, 2);
}

/* `e and e.Active`: 1, 0, -1 */
int Mode_Active(const nativeVal_t *e) {
	nativeVal_t v = { 0 };
	if (!Native_True(e)) {
		return 0;
	}
	Native_SetNone(&v);
	int r = Mode_VGet(e, "Active", &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* `for name in names: e = Scrap.GetEntity(name); if e: e.Del()` */
byte Mode_DelNames(const nativeVal_t *names) {
	nativeVal_t lst = { 0 };
	Native_Copy(&lst, names);
	Mode_Elems(&lst);
	int cnt = Native_ForCount(&lst);
	byte ok = cnt >= 0;
	for (int k = 0; ok && k < cnt; k++) {
		nativeVal_t it = { 0 }, e = { 0 };
		Native_SetNone(&e);
		ok = Native_SeqItem(&lst, k, &it) && Mode_EntityH(&it, &e) && (!Native_True(&e) || Mode_VDel(&e));
		Native_Free(&it);
		Native_Free(&e);
	}
	Native_Free(&lst);
	return ok;
}

/* Scrap.SetSaveVar(name, the global var of module) */
byte Mode_SaveG(const char *name, const char *module, const char *var) {
	nativeVal_t v = { 0 };
	byte ok = Mode_G(module, var, &v) && Mode_SetSaveVar(name, &v);
	Native_Free(&v);
	return ok;
}

/* module.var = [src.name] of a function of another module */
byte Mode_ModList1(const char *module, const char *var, const char *src, const char *name) {
	nativeVal_t l = { 0 };
	Native_SetEmpty(&l, NV_LIST);
	byte ok = LvN_ModGet(src, name, Native_Append(&l)) && Mode_SetModVar(module, var, &l);
	Native_Free(&l);
	return ok;
}

/* CGCalls() of the missions: the crazy gambler's message by the number of crazy deals done (none during the twelfth) */
/* module.var = [the global name of module] */
byte Mode_ModListG(const char *module, const char *var, const char *src, const char *name) {
	nativeVal_t l = { 0 };
	Native_SetEmpty(&l, NV_LIST);
	byte ok = LvN_GObj(src, name, Native_Append(&l)) && Mode_SetModVar(module, var, &l);
	Native_Free(&l);
	return ok;
}

/* the result of the global function of module called, when it is set (`if fn: fn()`) */
byte Mode_CallIf(const char *module, const char *name, byte *called) {
	nativeVal_t fn = { 0 };
	*called = 0;
	if (!Mode_G(module, name, &fn)) {
		return 0;
	}
	byte ok = 1;
	if (Native_True(&fn)) {
		ok = Mode_CallArgs(&fn, NULL, NULL, 0);
		*called = 1;
	}
	Native_Free(&fn);
	return ok;
}

/* apply(MissionsFuncs.SetNextMission, NextMissionData) of the module */
byte Mode_ApplyNext(const char *module) {
	nativeVal_t data = { 0 };
	PyObject *fn = Script_Attr("MissionsFuncs", "SetNextMission");
	/* the entry while fn is the port's own function */
	byte ok = fn && Mode_G(module, "NextMissionData", &data) && MODE_CBAPPLYSEQ(fn, &data, NULL);
	Py_XDECREF(fn);
	if (fn) {
		Native_Free(&data);
	}
	return ok;
}

/* a op the global name of the module: 1, 0, -1 */
int Mode_CmpG(const nativeVal_t *a, const char *op, const char *module, const char *name) {
	nativeVal_t g = { 0 };
	int r = Mode_G(module, name, &g) ? Mode_Cmp(a, op, &g) : -1;
	Native_Free(&g);
	return r;
}

byte Mode_EqI(const nativeVal_t *a, long i) {
	nativeVal_t b;
	Native_SetInt(&b, i);
	return !Native_Compare(a, &b);
}

byte Mode_RaiseText(const char *text) {
	nativeVal_t s;
	Native_SetText(&s, text, (int)strlen(text));
	Native_RaiseObj(&s, NULL);
	Native_Free(&s);
	return 0;
}

int Mode_UsrNameIs(const nativeVal_t *usr, const nativeVal_t *name) {
	nativeVal_t n;
	if (!Native_True(usr)) {
		return 0;
	}
	Native_SetNone(&n);
	int r = Mode_VGet(usr, "Name", &n) ? !Native_Compare(&n, name) : -1;
	Native_Free(&n);
	return r;
}

byte Mode_ModSetGlobal(const char *module, const char *var, const char *src, const char *name) {
	PyObject *o = Mode_GlobalPy(src, name);
	byte ok = o && Mode_SetModPy(module, var, o);
	Py_XDECREF(o);
	return ok;
}

int Mode_GCmp(const char *module, const char *name, const char *op, const nativeVal_t *v) {
	nativeVal_t g = { 0 };
	int r = Mode_G(module, name, &g) ? Mode_Cmp(&g, op, v) : -1;
	Native_Free(&g);
	return r;
}

int Mode_GCmpI(const char *module, const char *name, const char *op, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_GCmp(module, name, op, &v);
}

/* `usr and usr.IsType("Car")` */
int Mode_UsrInCar(const nativeVal_t *usr) {
	return Native_True(usr) ? Mode_VIsType(usr, "Car") : 0;
}

/* `x in` the global list name: 1, 0, -1 */
int Mode_InG(const nativeVal_t *x, const char *module, const char *name) {
	nativeVal_t l = { 0 };
	int r = Mode_G(module, name, &l) ? Native_Contains(&l, x) : -1;
	Native_Free(&l);
	return r;
}

/* if x in list: list.remove(x) of a global list of the module */
byte Mode_RemoveIn(const nativeVal_t *x, const char *module, const char *name) {
	int in = Mode_InG(x, module, name);
	return in >= 0 && (!in || Mode_Remove(module, name, x));
}

byte Mode_FirstOf(const char *list, nativeVal_t *e) {
	nativeVal_t name;
	Native_SetNone(&name);
	Native_SetNone(e);
	byte ok = Mode_ListGet(list, "First", &name) && Mode_EntityH(&name, e);
	Native_Free(&name);
	return ok;
}

/* itm.rTime = 20 and its Del 5 s later (OBJLIB 7: the scheduler keeps the bound method, the Python call until the
 * entities are native) */
byte Mode_DelLater(const nativeVal_t *itm) {
	nativeVal_t time;
	if (!LvN_SetI(itm, "rTime", 20)) {
		return 0;
	}
	PyObject *del = Mode_VBound(itm, "Del");
	if (!del) {
		return 0;
	}
	Native_SetFloat(&time, Mode_Time() + 5);
	byte ok = Mode_Schedule(&time, del, NULL, 0, NULL);
	Py_DECREF(del);
	return ok;
}

int Mode_PhoneUse(const char *module, const nativeVal_t *a) {
	nativeVal_t e = { 0 };
	int r = Mode_Import("PhoneCab") && Mode_SetModVar("Speech", "NPCName", &a[1]) && Mode_SetModVar("Speech", "PlayerName", &a[0]) &&
		PORT_CALL(PhoneCab, UseAudioCom, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]) && Mode_EntityH(&a[1], &e) && Mode_Put(module, "pc", NULL, &e) ?
		Mode_AlarmOn() : -1;
	Native_Free(&e);
	return r;
}

byte Mode_ForceMap(const char *map) {
	nativeVal_t zero = { 0 }, name = { 0 }, m = { 0 };
	Native_SetInt(&zero, 0);
	LvN_Str(&m, map);
	byte ok = Native_GVarGet("MissionScorerName", &name) && Mode_ScorerSet(&zero, &name, "ForceMap", &m);
	Native_Free(&name);
	Native_Free(&m);
	return ok;
}

int Mode_ModelHas(const nativeVal_t *v, const char *text) {
	nativeVal_t m = { 0 }, up = { 0 }, at = { 0 };
	int r = Mode_VGet(v, "Model", &m) && Native_Upper(&m, &up) && PORT_CALL(string, find, &at, "vs", &up, text) ? !LvN_EqI(&at, -1) : -1;
	Native_Free(&m);
	Native_Free(&up);
	Native_Free(&at);
	return r;
}

/* ---- the hubs of batch J (Traffic, OutMap, Challenge, CharConversor, RaceMaker, Doors) ---- */

#include <limits.h>

#include "scrap/collisiongrid.h"
#include "scrap/mapfloors.h"

int Mode_Len(nativeVal_t *v) {
	Mode_Elems(v);
	return Native_Size(v);
}

byte Mode_Sub(const nativeVal_t *v, const nativeVal_t *key, nativeVal_t *out) {
	Native_SetNone(out);
	if (!Native_IsObj(v)) {
		return Native_Subscript(v, key, out);
	}
	if (NObj_Native(v) && !Native_Holds(key, 1u << NV_OBJ)) {
		nativeVal_t r;
		return Mode_NativeOut(NObj_GetItem(v, key, &r), &r, out);
	}
	/* OBJLIB 7: PyObject_GetItem of the object, the item as Native_Arg makes it */
	PyObject *k = Native_ToPy(key), *r = k ? PyObject_GetItem(Native_PyOf(v), k) : NULL;
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
		return Native_HookCall(&hook, out, "vii", pos, minDist, maxDist);
	}
	if (!Mode_VecOf(pos, x)) {
		return PORT_CALL(SAI, GetNextRacePoint, out, "vii", pos, minDist, maxDist);
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
		nativeHook_t hook;
		int seam = Native_Seam("Scrap", "TestSphere", &hook);
		Native_SetInt(&rad, r);
		Native_SetStr(&l, list);
		ok = seam < 0 ? 0 : seam ? Native_HookCall(&hook, out, "vvvis", &c[0], &c[1], &c[2], r, list) :
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
		return Native_HookCall(&hook, NULL, "si", name, flag);
	}
	Mode_DummyStart(name, (int)flag);
	return 1;
}

byte Mode_DummyNextSeam(nativeVal_t *out) {
	MODE_SEAM("Scrap", "NextDummySearch");
	if (seam) {
		return Native_HookCall(&hook, out, "");
	}
	Mode_DummyNext(out);
	return 1;
}

byte Mode_SetGrid(long x, long y, long z) {
	MODE_SEAM("Scrap", "SetGrid");
	if (seam) {
		return Native_HookCall(&hook, NULL, "iii", x, y, z);
	}
	vec3_t size = { (float)x, (float)y, (float)z };
	Grid_Set(&g_pWorld->grid, &size);
	return 1;
}

byte Mode_EntitySeam(const nativeVal_t *name, nativeVal_t *out) {
	Native_SetNone(out);
	MODE_SEAM("Scrap", "GetEntity");
	if (seam) {
		return Native_HookCall(&hook, out, "v", name);
	}
	return Mode_EntityH(name, out);
}

byte Mode_ActionSet(const char *name) {
	MODE_SEAM("SInput", "SetActionSet");
	if (seam) {
		return Native_HookCall(&hook, NULL, "s", name);
	}
	Mode_SetActionSet(name);
	return 1;
}

byte Mode_SetMoneySeam(const nativeVal_t *money) {
	MODE_SEAM("Scrap", "SetMoney");
	if (seam) {
		return Native_HookCall(&hook, NULL, "v", money);
	}
	return Mode_SetMoney(money);
}

byte Mode_IncSaveVar(const char *name) {
	MODE_SEAM("Scrap", "IncSaveVar");
	if (seam) {
		return Native_HookCall(&hook, NULL, "s", name);
	}
	SaveVar_Inc(name, 1);
	return 1;
}

byte Mode_TimeSpeed(double speed) {
	MODE_SEAM("Scrap", "SetTimeSpeed");
	if (seam) {
		return Native_HookCall(&hook, NULL, "d", speed);
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
		/* float_abs: -0.0 stays */
		Native_SetFloat(out, v->f < 0 ? -v->f : v->f);
		return 1;
	case NV_LONG: {
		const char *d = Native_Text(v);
		d += *d == '-';
		Native_SetLong(out, d, (int)strlen(d));
		return 1;
	}
	case NV_OBJ:
	case NV_REF: {
		/* OBJLIB 7: an object with its own abs */
		PyObject *r = PyNumber_Absolute(Native_PyOf(v));
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

byte Mode_Neg(const nativeVal_t *v, nativeVal_t *out) {
	Native_SetNone(out);
	switch (v->type) {
	case NV_INT:
		if (v->i == LONG_MIN) {
			return Native_Raise(NE_OVERFLOW, "integer negation");
		}
		Native_SetInt(out, -v->i);
		return 1;
	case NV_FLOAT:
		Native_SetFloat(out, -v->f);
		return 1;
	case NV_LONG: {
		const char *d = Native_Text(v);
		if (*d == '-') {
			Native_SetLong(out, d + 1, (int)strlen(d + 1));
		} else if (!strcmp(d, "0")) {
			Native_SetLong(out, d, 1);
		} else {
			nativeBuf_t b;
			Native_BufInit(&b);
			Native_BufAdd(&b, "-", 1);
			Native_BufAdd(&b, d, -1);
			Native_SetLong(out, b.p, b.len);
			Native_BufFree(&b);
		}
		return 1;
	}
	case NV_OBJ:
	case NV_REF: {
		/* OBJLIB 7: an object with its own negation */
		PyObject *r = PyNumber_Negative(Native_PyOf(v));
		if (!r) {
			return 0;
		}
		Native_Arg(r, out);
		Py_DECREF(r);
		return 1;
	}
	}
	return Native_Raise(NE_TYPE, "bad operand type for unary -");
}

/* ---- the hubs of batch K (CharAct, Alarm, NewsPanel, Scene, Speech) ---- */

int Mode_In(const nativeVal_t *seq, const nativeVal_t *x) {
	if (!Native_IsObj(seq) && !Native_IsObj(x)) {
		return Native_Contains(seq, x);
	}
	if ((NObj_Native(seq) || NObj_Native(x)) && !Native_Holds(seq, 1u << NV_OBJ) && !Native_Holds(x, 1u << NV_OBJ)) {
		return NObj_Contains(seq, x);
	}
	/* OBJLIB 7: PySequence_In of the objects (a list that holds handles, a handle tested) */
	PyObject *s = Native_ToPy(seq), *o = s ? Native_ToPy(x) : NULL;
	int r = o ? PySequence_In(s, o) : -1;
	Py_XDECREF(o);
	Py_XDECREF(s);
	return r;
}

byte Mode_VSetFX(const nativeVal_t *h, const char *type) {
	cEntity *e;
	int k = Mode_MethodOf(h, "SetFX", &e);
	if (k >= 0) {
		return k && Mode_SetFX(e, type);
	}
	return Mode_PyMethod(h, "SetFX", NULL, "s", type);
}

byte Mode_SActV(const char *fn, const nativeVal_t *name, nativeVal_t *out) {
	const char *s;
	Native_SetNone(out);
	MODE_SEAM("SAct", fn);
	if (seam) {
		return Native_HookCall(&hook, out, "v", name);
	}
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	if (!strcmp(fn, "DelClass")) {
		Native_SetInt(out, cActClassList_Del(g_pActClassList, s) != 0);
		return 1;
	}
	/* the seam was looked at: the LvN_ functions take the engine's way */
	return !strcmp(fn, "GetClass") ? LvN_GetClass(s, out) : !strcmp(fn, "CreateClass") ? LvN_CreateClass(s, out) :
		!strcmp(fn, "CreateAction") ? LvN_CreateAction(s, out) : LvN_GetAct(s, out);
}

byte Mode_ActionSetV(const nativeVal_t *name) {
	const char *s;
	MODE_SEAM("SInput", "SetActionSet");
	if (seam) {
		return Native_HookCall(&hook, NULL, "v", name);
	}
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	Mode_SetActionSet(s);
	return 1;
}

void Mode_ActionSetName(nativeVal_t *out) {
	const char *s = TABLE_AT(g_ActionSets, Input_GetActionSet())->name;
	Native_SetText(out, s, (int)strlen(s));
}

byte Mode_Pack(const nativeVal_t *path) {
	const char *s;
	MODE_SEAM("Scrap", path ? "OpenPack" : "ClosePack");
	if (seam) {
		return path ? Native_HookCall(&hook, NULL, "v", path) : Native_HookCall(&hook, NULL, "");
	}
	if (!path) {
		cString_Set(g_PackName, "");
		return 1;
	}
	if (!Native_ArgStr(path, 1, &s)) {
		return 0;
	}
	cString_Printf(g_PackName, "%s/", s);
	return 1;
}

/* SScorer.SetMsgText (msg 1) or SetLabelText with the time a value of the script */
static byte Mode_TextV(byte msg, const nativeVal_t *text, const nativeVal_t *time) {
	const char *s;
	double d;
	MODE_SEAM("SScorer", msg ? "SetMsgText" : "SetLabelText");
	if (seam) {
		return Native_HookCall(&hook, NULL, "vv", text, time);
	}
	if (!Native_ArgStr(text, 1, &s) || !Native_ArgDouble(time, &d)) {
		return 0;
	}
	return msg ? Mode_MsgText(text, d) : Mode_LabelText(text, d);
}

byte Mode_MsgTextV(const nativeVal_t *text, const nativeVal_t *time) {
	return Mode_TextV(1, text, time);
}

byte Mode_LabelTextV(const nativeVal_t *text, const nativeVal_t *time) {
	return Mode_TextV(0, text, time);
}

byte Mode_SpeechText(const nativeVal_t *text, const nativeVal_t *time, const nativeVal_t *r, const nativeVal_t *g,
	const nativeVal_t *b) {
	const char *s;
	double d;
	int c[3];
	MODE_SEAM("SScorer", "SetSpeechText");
	if (seam) {
		return Native_HookCall(&hook, NULL, "vvvvv", text, time, r, g, b);
	}
	/* "sdiii" */
	if (!Native_ArgStr(text, 1, &s) || !Native_ArgDouble(time, &d) || !Native_ArgInt(r, &c[0]) || !Native_ArgInt(g, &c[1]) ||
		!Native_ArgInt(b, &c[2])) {
		return 0;
	}
	Scorer_SetSpeechText(s, (float)d, c[0], c[1], c[2]);
	return 1;
}

byte Mode_SpeechCallbackV(const nativeVal_t *func) {
	const char *s;
	MODE_SEAM("SScorer", "SetSpeechCallback");
	if (seam) {
		return Native_HookCall(&hook, NULL, "v", func);
	}
	/* "s|O" */
	if (!Native_ArgStr(func, 1, &s)) {
		return 0;
	}
	Scorer_SetSpeechCallback(s, NULL);
	return 1;
}

byte Mode_SpeechCallback(const char *func) {
	nativeVal_t v;
	Native_SetText(&v, func, (int)strlen(func));
	byte ok = Mode_SpeechCallbackV(&v);
	Native_Free(&v);
	return ok;
}

byte Mode_CancelSpeech(void) {
	MODE_SEAM("SScorer", "CancelSpeech");
	if (seam) {
		return Native_HookCall(&hook, NULL, "");
	}
	Scorer_CancelSpeech(0);
	return 1;
}

byte Mode_HeadMonitor(const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c) {
	const char *s, *t;
	int i;
	MODE_SEAM("SScorer", "SetHeadMonitor");
	if (seam) {
		return Native_HookCall(&hook, NULL, "vvv", a, b, c);
	}
	if (!Native_ArgStr(a, 1, &s) || !Native_ArgStr(b, 2, &t) || !Native_ArgInt(c, &i)) {
		return 0;
	}
	Scorer_SetHeadMonitor(s, t, i);
	return 1;
}

byte Mode_StopVoice(const nativeVal_t *ch) {
	int i;
	MODE_SEAM("SSound", "StopVoice");
	if (seam) {
		return Native_HookCall(&hook, NULL, "v", ch);
	}
	if (!Native_ArgInt(ch, &i)) {
		return 0;
	}
	SoundSys_StopVoice(g_pSound, i);
	return 1;
}

byte Mode_PlayVoice(const nativeVal_t *ch, const nativeVal_t *text) {
	const char *s = "";
	int i;
	MODE_SEAM("SSound", "PlayVoice");
	if (seam) {
		return text ? Native_HookCall(&hook, NULL, "vv", ch, text) : Native_HookCall(&hook, NULL, "v", ch);
	}
	/* "i|s" */
	if (!Native_ArgInt(ch, &i) || (text && !Native_ArgStr(text, 2, &s))) {
		return 0;
	}
	SoundSys_PlayVoice(g_pSound, i);
	SoundSys_SetVoiceString(g_pSound, i, s);
	return 1;
}

byte Mode_OpenVoice(const nativeVal_t *file, const nativeVal_t *ch, const nativeVal_t *arg) {
	const char *s;
	int i, a = 1;
	MODE_SEAM("SSound", "OpenVoice");
	if (seam) {
		return arg ? Native_HookCall(&hook, NULL, "vvv", file, ch, arg) : Native_HookCall(&hook, NULL, "vv", file, ch);
	}
	/* "si|i" */
	if (!Native_ArgStr(file, 1, &s) || !Native_ArgInt(ch, &i) || (arg && !Native_ArgInt(arg, &a))) {
		return 0;
	}
	SoundSys_OpenVoice(g_pSound, s, i, a);
	return 1;
}

byte Mode_VoiceRemain(const nativeVal_t *ch, const nativeVal_t *def, nativeVal_t *out) {
	int i;
	double d;
	float remain;
	Native_SetNone(out);
	MODE_SEAM("SSound", "VoiceRemain");
	if (seam) {
		return Native_HookCall(&hook, out, "vv", ch, def);
	}
	/* "id" */
	if (!Native_ArgInt(ch, &i) || !Native_ArgDouble(def, &d)) {
		return 0;
	}
	if (SoundSys_VoiceRemain(g_pSound, i, &remain)) {
		d = (double)remain;
	}
	Native_SetFloat(out, d);
	return 1;
}

byte Mode_VoiceVolume(const nativeVal_t *ch, const nativeVal_t *vol) {
	int i;
	double d;
	MODE_SEAM("SSound", "SetVoiceVolume");
	if (seam) {
		return Native_HookCall(&hook, NULL, "vv", ch, vol);
	}
	if (!Native_ArgInt(ch, &i) || !Native_ArgDouble(vol, &d)) {
		return 0;
	}
	SoundSys_SetVoiceVolume(g_pSound, i, (float)d);
	return 1;
}

byte Mode_StopAllSounds(void) {
	MODE_SEAM("SSound", "StopAllSounds");
	if (seam) {
		return Native_HookCall(&hook, NULL, "");
	}
	SoundSys_StopAllSounds(g_pSound);
	return 1;
}

byte Mode_First(nativeVal_t *out) {
	return Mode_EntHandle(cWorld_GetFirstEntity(g_pWorld), out);
}

byte Mode_AtVal(const char *module, const char *dict, const nativeVal_t *key, nativeVal_t *out) {
	return Mode_At(module, dict, key, out);
}

byte Mode_Kw(PyObject *args, PyObject *kw, const char *fmt, char **names, int n, nativeVal_t *a, byte *given) {
	PyObject *o[8] = { NULL };
	for (int i = 0; i < n; i++) {
		Native_SetNone(&a[i]);
	}
	if (!PyArg_ParseTupleAndKeywords(args, kw, (char *)fmt, names, &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7])) {
		return 0;
	}
	for (int i = 0; i < n; i++) {
		if (o[i]) {
			Native_Arg(o[i], &a[i]);
		}
		if (given) {
			given[i] = o[i] != NULL;
		}
	}
	return 1;
}

PyObject *Mode_KwOut(nativeVal_t *a, int n, byte ok, nativeVal_t *out) {
	for (int i = 0; i < n; i++) {
		Native_Free(&a[i]);
	}
	PyObject *r = ok ? (!out || out->type == NV_NONE ? Py_ReturnNone() : Native_ToPy(out)) : NULL;
	if (out) {
		Native_Free(out);
	}
	return r;
}

byte Mode_PreloadTexture(const nativeVal_t *name) {
	const char *s;
	MODE_SEAM("SScorer", "PreloadTexture");
	if (seam) {
		return Native_HookCall(&hook, NULL, "v", name);
	}
	/* "s|i" */
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	Scorer_PreloadTexture(s, 0);
	return 1;
}

byte Mode_ListAddG(const char *module, const char *var, const nativeVal_t *v, byte first) {
	/* PyList_Append or PyList_Insert on the list object of the global (the SystemError of a value that is no list, as
	 * the C ports changed it) */
	nativeVal_t o, a[2], r;
	byte ok;
	if (!ModVar_GlobalVal(module, var, &o)) {
		return 0;
	}
	if (NObj_Kind(&o) == NO_LIST) {
		Native_SetInt(&a[0], 0);
		a[1] = *v;
		ok = first ? NObj_CallMethod(&o, "insert", a, 2, &r) : NObj_CallMethod(&o, "append", v, 1, &r);
		Native_Free(&r);
	} else {
		PyObject *l = Native_ToPy(&o), *x = l ? Native_ToPy(v) : NULL;
		ok = x && (first ? PyList_Insert(l, 0, x) : PyList_Append(l, x)) == 0;
		Py_XDECREF(x);
		Py_XDECREF(l);
	}
	Native_Free(&o);
	return ok;
}

byte Mode_DelItemG(const char *module, const char *var, int i) {
	/* PySequence_DelItem on the object of the global */
	nativeVal_t o, k;
	byte ok;
	if (!ModVar_GlobalVal(module, var, &o)) {
		return 0;
	}
	if (NObj_Kind(&o) == NO_LIST) {
		Native_SetInt(&k, i);
		ok = NObj_DelItem(&o, &k);
	} else {
		PyObject *l = Native_ToPy(&o);
		ok = l && PySequence_DelItem(l, i) == 0;
		Py_XDECREF(l);
	}
	Native_Free(&o);
	return ok;
}
/* ---- the vehicle modules of batch L (Vehicles, VehiclesNPC, ShipMaker, SputnikInterface) ---- */

int Mode_ModTrue(const char *module, const char *var) {
	nativeVal_t v;
	int r = LvN_ModGet(module, var, &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* ---- shared after the merge of the level batches with batch K ---- */

/* module.var.append(gmod.name): a function appended to the list of another module in place */
byte Mode_AppendG(const char *module, const char *var, const char *gmod, const char *name) {
	nativeVal_t o, fn = { 0 }, r = { 0 };
	if (!ModVar_ModAttrVal(module, var, &o)) {
		return 0;
	}
	byte ok = Mode_RefG(gmod, name, &fn) && Mode_ObjMethod(&o, "append", &fn, 1, &r);
	Native_Free(&r);
	Native_Free(&fn);
	Native_Free(&o);
	return ok;
}

/* SScorer.Set(0, item, var, v) */
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

/* ---- the menus of batch O (MenuBase, MenuControls, MenuVideo, MenuVideoVars) ---- */

byte Mode_Printed(byte ok, const char *module, const char *func) {
	if (!ok) {
		/* the error printed where Script_Call printed it; the menu goes on */
		Con_Warnf("Script: %s.%s failed\n", module, func);
		Native_ErrPrint();
	}
	return ok;
}

byte Mode_GVarGetV(const nativeVal_t *name, nativeVal_t *out) {
	const char *s;
	MODE_SEAM("Scrap", "Get");
	if (seam) {
		return Native_HookCall(&hook, out, "v", name);
	}
	return Native_ArgStr(name, 1, &s) && Native_GVarGet(s, out);
}

byte Mode_ScorerGetVal(const nativeVal_t *id, const nativeVal_t *item, const char *var, nativeVal_t *out) {
	int i;
	const char *s;
	MODE_SEAM("SScorer", "Get");
	if (seam) {
		return Native_HookCall(&hook, out, "vvs", id, item, var);
	}
	/* "iss" */
	if (!Native_ArgInt(id, &i) || !Native_ArgStr(item, 2, &s)) {
		return 0;
	}
	Mode_ScorerGet(i, s, var, out);
	return 1;
}

byte Mode_ScorerAddV(const nativeVal_t *id, const nativeVal_t *name, const char *type) {
	MODE_SEAM("SScorer", "Add");
	if (seam) {
		return Native_HookCall(&hook, NULL, "vvs", id, name, type);
	}
	return Mode_ScorerAdd(id, name, type);
}

byte Mode_ResetKeys(void) {
	MODE_SEAM("SInput", "ResetToDefault");
	if (seam) {
		return Native_HookCall(&hook, NULL, "");
	}
	Input_ResetToDefault();
	return 1;
}

void Mode_ElemsIn(nativeVal_t *v) {
	Mode_Elems(v);
	if (v->type == NV_LIST || v->type == NV_TUPLE) {
		for (int i = 0; i < v->n; i++) {
			Mode_Elems(&v->items[i]);
		}
	}
}

/* SSound.PlaySound(pos, name, volume) through its seam */
byte Mode_PlaySoundSeam(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume) {
	MODE_SEAM("SSound", "PlaySound");
	if (seam) {
		return Native_HookCall(&hook, NULL, "vvv", pos, name, volume);
	}
	return Mode_PlaySound(pos, name, volume);
}

/* ---- the character hubs of batch M (MakeChar, Chars, CharFuncs) ---- */

long Mode_ModGetI(const char *module, const char *var, long def) {
	nativeVal_t v = { 0 };
	long r = def;
	if (!LvN_ModGet(module, var, &v)) {
		Native_ErrClear();
	} else if (v.type == NV_INT) {
		r = (int)v.i;
	} else if (v.type == NV_FLOAT) {
		r = (int)v.f;
	}
	Native_Free(&v);
	return r;
}

double Mode_ModGetN(const char *module, const char *var, double def) {
	nativeVal_t v = { 0 };
	double r = def;
	if (!LvN_ModGet(module, var, &v)) {
		Native_ErrClear();
	} else if (v.type == NV_FLOAT) {
		r = v.f;
	} else if (v.type == NV_INT) {
		r = (double)v.i;
	}
	Native_Free(&v);
	return r;
}

void Mode_ModPut(const char *module, const char *var, const nativeVal_t *v) {
	if (!Mode_SetModVar(module, var, v)) {
		Native_ErrClear();
	}
}

void Mode_ModPutI(const char *module, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	Mode_ModPut(module, var, &v);
}

/* ---- batch P (CharsNPC, GameSounds, MenuMission) ---- */

/* the n pairs of a name and a const nativeVal_t * of va into names and vals (up to MODE_DEFVARS) */
static void Mode_DefPairs(int n, va_list va, const char **names, const nativeVal_t **vals) {
	CHECK(n <= MODE_DEFVARS);
	for (int i = 0; i < n; i++) {
		names[i] = va_arg(va, const char *);
		vals[i] = va_arg(va, const nativeVal_t *);
	}
}

/* the dict of the $names (OBJLIB 6): a new reference, NULL with the error */
static PyObject *Mode_DefDict(const char *const *names, const nativeVal_t *const *vals, int n) {
	nativeVal_t d, k, v;
	Native_SetEmpty(&d, NV_DICT);
	for (int i = 0; i < n; i++) {
		Native_SetStr(&k, names[i]);
		Native_Copy(&v, vals[i]);
		Native_DictSet(&d, &k, &v);
	}
	PyObject *vars = Native_ToPy(&d);
	Native_Free(&d);
	return vars;
}

PyObject *Mode_DefVars(int n, ...) {
	const char *names[MODE_DEFVARS];
	const nativeVal_t *vals[MODE_DEFVARS];
	va_list va;
	va_start(va, n);
	Mode_DefPairs(n, va, names, vals);
	va_end(va);
	return Mode_DefDict(names, vals, n);
}

/* ---- the menus of batch Q (MenuMain, MenuHud) ---- */

byte Mode_SetCinema(long id, long on, double time) {
	MODE_SEAM("SScorer", "SetCinema");
	if (seam) {
		return Native_HookCall(&hook, NULL, "iid", id, on, time);
	}
	/* "iif" */
	cScorer *sc = Scorer_Get((int)id);
	if (sc) {
		ScorerCinema_Set(&sc->cinema, (int)on != 0, (float)time);
	}
	return 1;
}

byte Mode_CamSeam(long i, nativeVal_t *out) {
	Native_SetNone(out);
	MODE_SEAM("Scrap", "GetCam");
	if (seam) {
		return Native_HookCall(&hook, out, "i", i);
	}
	Mode_CamName((int)i, out);
	return 1;
}

byte Mode_UsrSeam(long id, nativeVal_t *out) {
	Native_SetNone(out);
	MODE_SEAM("Scrap", "UsrEntity");
	if (seam) {
		return Native_HookCall(&hook, out, "i", id);
	}
	/* "i" read as unsigned: a player above 3 (or below 0) is NULL without an exception, which the call reports */
	if ((uint32_t)(int)id > 3) {
		return Native_NullResult();
	}
	return Mode_EntHandle(cWorld_GetUsrEntity(g_pWorld, (int)id), out);
}

byte Mode_SchedAt(double time, const char *module, const char *func, const nativeVal_t *args, int n, const char *name) {
	PyObject *f = Script_Attr(module, func);
	byte ok = f && Mode_SchedAdd((float)time, f, args, n, name ? name : "");
	Native_ErrClear();
	Py_XDECREF(f);
	return ok;
}

void Mode_CallVarIf(const char *module, const char *var, long id) {
	nativeVal_t f, a;
	if (Mode_ModGet(module, var, &f) && Native_True(&f)) {
		Native_SetInt(&a, id);
		/* the callback through its entry while it is a port's own function (Port_Value) */
		if (!Mode_CallArgs(&f, NULL, &a, 1)) {
			Native_ErrPrint();
		}
	}
	Native_ErrClear();
	Native_Free(&f);
}

byte Mode_CreateSeam(const char *type, long x, long y, long z, const char *name, nativeVal_t *out) {
	Native_SetNone(out);
	MODE_SEAM("Scrap", "CreateEntity");
	if (seam) {
		return Native_HookCall(&hook, out, "siiis", type, x, y, z, name);
	}
	/* "sddds" */
	return Mode_EntHandle(cWorld_CreateEntity(g_pWorld, type, (float)x, (float)y, (float)z, name), out);
}

byte Mode_Exit(void) {
	MODE_SEAM("Scrap", "Exit");
	if (seam) {
		return Native_HookCall(&hook, NULL, "");
	}
	App_Exit(g_pApp);
	return 1;
}

/* ---- the multiplayer menus of batch R (MenuNet) ---- */

const char *Mode_ModGetS(const char *module, const char *var, char *out, int size) {
	nativeVal_t v = { 0 };
	out[0] = 0;
	if (!LvN_ModGet(module, var, &v)) {
		Native_ErrClear();
	} else if (v.type == NV_STR) {
		Str_Copy(out, Native_Text(&v), size);
	}
	Native_Free(&v);
	return out;
}

/* ---- the hubs of batch S (Parking, Accelerator, InitLevel) ---- */

byte Mode_EvalIn(const char *module, const nativeVal_t *text, nativeVal_t *out) {
	return Mode_EvalSave(module, text, out);
}

byte Mode_ListSetSeam(const char *list, const char *var, const char *value) {
	MODE_SEAM("Scrap", "EntityListSet");
	if (seam) {
		return Native_HookCall(&hook, NULL, "sss", list, var, value);
	}
	return Mode_ListSet(list, var, value);
}

/* ---- the hubs of batch T (MissionsFuncs, SaveGame) ---- */

byte Mode_ActionSetGet(nativeVal_t *out) {
	MODE_SEAM("SInput", "GetActionSet");
	if (seam) {
		return Native_HookCall(&hook, out, "");
	}
	Mode_ActionSetName(out);
	return 1;
}

/* the literal of an eval text of n bytes (no 0 byte among them, read after its leading blanks, as Script_EvalValue
 * reads it) with the native reader: 1 with *out, else 0 without an error and *out None */
static byte Mode_EvalLiteral(const char *s, int n, nativeVal_t *out) {
	Native_SetNone(out);
	if ((int)strlen(s) != n) {
		return 0;
	}
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	return Native_Literal(s, out);
}

/* a dict in v, at any depth */
static byte Mode_HasDict(const nativeVal_t *v) {
	if (v->type == NV_DICT) {
		return 1;
	}
	if (v->type == NV_LIST || v->type == NV_TUPLE) {
		for (int i = 0; i < v->n; i++) {
			if (Mode_HasDict(&v->items[i])) {
				return 1;
			}
		}
	}
	return 0;
}

byte Mode_LiteralPy(PyObject *text, nativeVal_t *out) {
	if (!PyString_Check(text)) {
		Native_SetNone(out);
		return 0;
	}
	return Mode_EvalLiteral(PyString_AsString(text), PyString_Size(text), out);
}

PyObject *Mode_EvalPy(const char *module, PyObject *text) {
	nativeVal_t v;
	if (Mode_LiteralPy(text, &v) && !Mode_HasDict(&v)) {
		PyObject *o = Native_ToPy(&v);
		Native_Free(&v);
		return o;
	}
	Native_Free(&v);
	return Script_EvalIn(module, text);
}

byte Mode_EvalSave(const char *module, const nativeVal_t *text, nativeVal_t *out) {
	if (text->type == NV_STR && Mode_EvalLiteral(Native_Text(text), text->n, out)) {
		return 1;
	}
	Native_SetNone(out);
	/* OBJLIB 6: the bare names, name chains and errors of the reader of Script_EvalIn */
	PyObject *t = Native_ToPy(text), *v = t ? Script_EvalIn(module, t) : NULL;
	Py_XDECREF(t);
	if (!v) {
		return 0;
	}
	Native_Arg(v, out);
	Py_DECREF(v);
	return 1;
}

/* ---- shared after stage 2 (the copies the batches wrote, joined) ---- */

byte Mode_PrintV(const nativeVal_t *v) {
	const nativeVal_t *items[1] = { v };
	return Mode_Print(items, 1);
}

byte Mode_PrintS(const char *text) {
	nativeVal_t t;
	Native_SetText(&t, text, (int)strlen(text));
	byte ok = Mode_PrintV(&t);
	Native_Free(&t);
	return ok;
}

byte Mode_SchedPy(double time, const char *module, const char *func, PyObject *args, const char *name) {
	PyObject *f = Script_Attr(module, func);
	if (f) {
		Scheduler_Add(&g_pWorld->scheduler, (float)time, f, args, name ? name : "");
	}
	Native_ErrClear();
	Py_XDECREF(f);
	return f != NULL;
}

void Mode_PrintedCall(portSite_t *site, const char *module, const char *func, long id) {
	Mode_Printed(Mode_CbAttr(site, module, func, NULL, "i", id), module, func);
}

int Mode_SeqLen(const nativeVal_t *seq) {
	int n;
	return Native_Len(seq, &n) ? n : -1;
}

byte Mode_LangUse(const char *module, const char *key, nativeVal_t *out) {
	nativeVal_t lang, use;
	Mode_Lang(key, &lang);
	byte ok = Mode_G(module, "UseTxt", &use) && Native_Arith('%', &lang, &use, out);
	Native_Free(&lang);
	Native_Free(&use);
	return ok;
}

void Mode_ItemColor(int id, const char *item, const int *c) {
	Script_SetNum(id, item, "Red", c[0]);
	Script_SetNum(id, item, "Green", c[1]);
	Script_SetNum(id, item, "Blue", c[2]);
}

void Mode_ItemVisible(int id, const char *item, int v) {
	Script_SetNum(id, item, "Visible", v);
}

byte Mode_SoundOf(const char *name) {
	char snd[0x40], vol[0x40];
	Str_Printf(snd, sizeof(snd), "%sSound", name);
	Str_Printf(vol, sizeof(vol), "%sVolume", name);
	return Mode_Sound(snd, vol);
}

const nativeVal_t *Mode_IntVal(nativeVal_t *v, long i) {
	Native_SetInt(v, i);
	return v;
}

void Mode_Tuple3(nativeVal_t *out, const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c) {
	Native_SetEmpty(out, NV_TUPLE);
	Native_Copy(Native_Append(out), a);
	Native_Copy(Native_Append(out), b);
	Native_Copy(Native_Append(out), c);
}

void Mode_RandF(double a, double b, nativeVal_t *out) {
	Native_SetFloat(out, Scrap_RandRange(a, b));
}

PyObject *Mode_PyNone(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Py_ReturnNone();
}

byte Mode_ApplyFlash(int id) {
	Script_SetStr(id, "Apply", "Effect", "Outline");
	Script_SetNum(id, "Apply", "Red", 255);
	Script_SetNum(id, "Apply", "Green", 255);
	Script_SetNum(id, "Apply", "Blue", 180);
	Script_SetStr(id, "Apply", "OnRender", "!ScorerSpriteFlash");
	return 1;
}

byte Mode_DummyStartV(const nativeVal_t *name, long flag) {
	nativeHook_t hook;
	const char *s;
	int seam = Native_Seam("Scrap", "StartDummySearch", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(&hook, NULL, "vi", name, flag);
	}
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	Mode_DummyStart(s, (int)flag);
	return 1;
}

void Mode_ModPutS(const char *module, const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	Mode_ModPut(module, var, &v);
	Native_Free(&v);
}

int Mode_CmpOp(const nativeVal_t *a, const nativeVal_t *b, char op) {
	int c = Native_Compare(a, b);
	if (Native_Failed()) {
		return -1;
	}
	switch (op) {
	case '=':
		return c == 0;
	case '!':
		return c != 0;
	case '>':
		return c > 0;
	case '<':
		return c < 0;
	case 'g':
		return c >= 0;
	}
	return c <= 0;
}

int Mode_CmpOpI(const nativeVal_t *a, long i, char op) {
	nativeVal_t b;
	Native_SetInt(&b, i);
	return Mode_CmpOp(a, &b, op);
}

byte Mode_VObjRead(const nativeVal_t *h, const char *var, nativeVal_t *slot) {
	Native_Free(slot);
	return Mode_VObj(h, var, slot);
}

byte Mode_AppendName(const char *module, const char *name, const nativeVal_t *h) {
	nativeVal_t n;
	Native_SetNone(&n);
	byte ok = Mode_VGet(h, "Name", &n) && Mode_Append(module, name, &n);
	Native_Free(&n);
	return ok;
}

byte Mode_SetAng(const nativeVal_t *h, const nativeVal_t *x) {
	nativeVal_t a;
	Native_SetEmpty(&a, NV_TUPLE);
	Native_SetInt(Native_Append(&a), 0);
	Native_Copy(Native_Append(&a), x);
	Native_SetInt(Native_Append(&a), 0);
	byte ok = Mode_VSet(h, "Ang", &a);
	Native_Free(&a);
	return ok;
}

byte Mode_LangMsg(const char *key) {
	nativeVal_t t;
	Mode_Lang(key, &t);
	byte ok = Mode_SpeechMsg(&t);
	Native_Free(&t);
	return ok;
}

PyObject *Mode_PyNone2(PyObject *self, PyObject *args) {
	PyObject *a, *b;
	(void)self;
	return PyArg_ParseTuple(args, "OO", &a, &b) ? Py_ReturnNone() : NULL;
}

byte Mode_FlagSet(const char *module, const char *var, const nativeVal_t *v) {
	PyObject *flag = Mode_GlobalPy(module, "Flag");
	byte ok = flag && Mode_HSet(flag, var, v);
	Py_XDECREF(flag);
	return ok;
}

byte Mode_Cam0(nativeVal_t *out) {
	nativeVal_t name;
	Mode_CamName(0, &name);
	byte ok = Mode_EntitySeam(&name, out);
	Native_Free(&name);
	return ok;
}

byte Mode_CamLink(const nativeVal_t *link) {
	nativeVal_t cam, e = { 0 };
	Mode_CamName(0, &cam);
	byte ok = Mode_EntitySeam(&cam, &e) && Mode_VSet(&e, "EntityLink", link);
	Native_Free(&cam);
	Native_Free(&e);
	return ok;
}

byte Mode_GEntity(const char *module, const char *var, nativeVal_t *out) {
	nativeVal_t name = { 0 };
	Native_SetNone(out);
	byte ok = Mode_G(module, var, &name) && Mode_EntityH(&name, out);
	Native_Free(&name);
	return ok;
}

byte Mode_SaveVarV(const nativeVal_t *name, nativeVal_t *out) {
	const char *s;
	Native_SetNone(out);
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	Mode_SaveVar(s, out);
	return 1;
}

byte Mode_SetSaveV(const nativeVal_t *name, const nativeVal_t *v) {
	const char *s;
	return Native_ArgStr(name, 1, &s) && Mode_SetSaveVar(s, v);
}

byte Mode_SaveCopy(const char *name, const char *from) {
	nativeVal_t v;
	Mode_SaveVar(from, &v);
	byte ok = Mode_SetSaveVar(name, &v);
	Native_Free(&v);
	return ok;
}

/* ---- the native entries of stage 3, batch A2b (optional arguments, adapters on Python objects) ---- */

byte Mode_Arity(int n, int min, int max) {
	if (n >= min && n <= max) {
		return 1;
	}
	if (min == max) {
		return Port_Arity(n, min);
	}
	/* the TypeError of PyArg_ParseTuple (and of PyArg_ParseTupleAndKeywords without keywords) */
	int m = n < min ? min : max;
	return Native_Raise(NE_TYPE, "function requires %s %d argument%s; %d given", n < min ? "at least" : "at most", m,
		m == 1 ? "" : "s", n);
}

int Mode_OptArgs(PyObject *args, int min, int max, modeVals_t *t, nativeVal_t **a) {
	PyObject *o[11] = { NULL };
	char fmt[16];
	int k = 0, n = 0;
	CHECK(min >= 0 && min <= max && max <= 11);
	for (int i = 0; i < max; i++) {
		if (i == min) {
			fmt[k++] = '|';
		}
		fmt[k++] = 'O';
	}
	fmt[k] = 0;
	if (!PyArg_ParseTuple(args, fmt, &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7], &o[8], &o[9], &o[10])) {
		return -1;
	}
	/* max values one after the other, those not given None */
	*a = max ? Mode_V(t) : NULL;
	for (int i = 1; i < max; i++) {
		Mode_V(t);
	}
	for (; n < max && o[n]; n++) {
		Native_Arg(o[n], &(*a)[n]);
	}
	return n;
}

/* the adapter fn or kwfn on the arguments as Python values, its result into out */
static byte Mode_PyCall(PyCFunction fn, PyCFunctionWithKeywords kwfn, const nativeVal_t *a, int n, nativeVal_t *out) {
	PyObject *args = PyTuple_New(n);
	for (int i = 0; args && i < n; i++) {
		PyObject *o = Native_ToPy(&a[i]);
		if (!o) {
			Py_DECREF(args);
			return 0;
		}
		PyTuple_SET_ITEM(args, i, o);
	}
	PyObject *r = !args ? NULL : fn ? fn(NULL, args) : kwfn(NULL, args, NULL);
	Py_XDECREF(args);
	if (!r) {
		return 0;
	}
	Native_Arg(r, out);
	Py_DECREF(r);
	return 1;
}

byte Mode_PyEntry(PyCFunction fn, const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_PyCall(fn, NULL, a, n, out);
}

byte Mode_PyEntryKw(PyCFunctionWithKeywords fn, const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_PyCall(NULL, fn, a, n, out);
}

/* ---- native entries of hand and keyword adapters (stage 3, batch A2a) ---- */

byte Mode_ArgInt(const nativeVal_t *v, int *out) {
	if (!Native_IsObj(v)) {
		return Native_ArgInt(v, out);
	}
	/* "i" of getargs.c: PyInt_AsLong through the number methods of the object */
	long l = PyInt_AsLong(Native_PyOf(v));
	if (l == -1 && Native_Failed()) {
		return 0;
	}
	*out = (int)l;
	return 1;
}

/* the number of v as "l" of PyArg reads it: PyInt_AsLong as Mode_ArgInt takes it, not cut to an int */
static byte Mode_ArgLong(const nativeVal_t *v, long *out) {
	int i;
	if (Native_IsObj(v)) {
		*out = PyInt_AsLong(Native_PyOf(v));
		return *out != -1 || !Native_Failed();
	}
	if (!Native_ArgInt(v, &i)) {
		return 0;
	}
	*out = v->type == NV_INT ? v->i : v->type == NV_FLOAT ? (long)v->f :
		v->type == NV_LONG ? strtol(Native_Text(v), NULL, 10) : i;
	return 1;
}

/* the number of v as "d" and "f" of PyArg read it: Native_ArgDouble, and an NV_OBJ through PyFloat_AsDouble as
 * getargs.c has it (an instance's __float__) */
static byte Mode_ArgDouble(const nativeVal_t *v, double *out) {
	if (!Native_IsObj(v)) {
		return Native_ArgDouble(v, out);
	}
	*out = PyFloat_AsDouble(Native_PyOf(v));
	return !Native_Failed();
}

static byte Mode_FmtLetter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* the counts of a format as vgetargs1 of getargs.c takes them: letters and groups at the top level, min those before
 * the '|' */
static void Mode_FmtCount(const char *fmt, int *min, int *max) {
	int level = 0;
	*min = -1;
	*max = 0;
	for (; *fmt && *fmt != ':' && *fmt != ';'; fmt++) {
		if (*fmt == '(') {
			*max += !level;
			level++;
		} else if (*fmt == ')') {
			level--;
		} else if (!level && Mode_FmtLetter(*fmt)) {
			(*max)++;
		} else if (!level && *fmt == '|') {
			*min = *max;
		}
	}
	if (*min < 0) {
		*min = *max;
	}
}

static const char *Mode_ParseItem(const nativeVal_t *v, const char **f, va_list *va, int *levels, char *buf, int size,
	byte top);

/* converttuple of getargs.c (not at the top level) on v, *f after the '(' */
static const char *Mode_ParseGroup(const nativeVal_t *v, const char **f, va_list *va, int *levels, char *buf,
	int size) {
	int n = 0, level = 0, len = 0;
	for (const char *s = *f; *s && *s != ':' && *s != ';'; s++) {
		if (*s == '(') {
			n += !level;
			level++;
		} else if (*s == ')') {
			if (!level) {
				break;
			}
			level--;
		} else if (!level && Mode_FmtLetter(*s)) {
			n++;
		}
	}
	/* OBJLIB 7: an NV_OBJ (a list that holds a handle) the Python way */
	PyObject *obj = Native_IsObj(v) ? Native_PyOf(v) : NULL;
	byte seq = v->type == NV_LIST || v->type == NV_TUPLE || v->type == NV_STR || v->type == NV_VEC;
	if (obj ? !PySequence_Check(obj) : !seq) {
		levels[0] = 0;
		snprintf(buf, size, "%d-sequence, %s", n, Native_TypeName(v));
		return buf;
	}
	if (obj) {
		len = PySequence_Length(obj);
	} else {
		Native_Len(v, &len);
	}
	if (len != n) {
		levels[0] = 0;
		snprintf(buf, size, "%d-sequence, %d-sequence", n, len);
		return buf;
	}
	for (int i = 0; i < n; i++) {
		nativeVal_t tmp;
		const nativeVal_t *item = &tmp;
		Native_SetNone(&tmp);
		if (obj) {
			/* a NULL item goes on as None: PyInt_AsLong(NULL) gives the TypeError of None */
			PyObject *o = PySequence_GetItem(obj, i);
			if (o) {
				Native_Arg(o, &tmp);
				Py_DECREF(o);
			}
		} else {
			item = Native_GetItem(v, i, 0, &tmp);
		}
		const char *msg = Mode_ParseItem(item, f, va, levels + 1, buf, size, 0);
		Native_Free(&tmp);
		if (msg) {
			levels[0] = i + 1;
			return msg;
		}
	}
	(*f)++;
	return NULL;
}

/* convertitem of getargs.c on v for the format at *f: NULL, or the text for "expected %s found" ("string, int": the
 * expected one and the type found; seterror leaves an error raised already) */
static const char *Mode_ParseItem(const nativeVal_t *v, const char **f, va_list *va, int *levels, char *buf, int size,
	byte top) {
	const char *msg = NULL;
	char c = *(*f)++;
	if (c == '(') {
		return Mode_ParseGroup(v, f, va, levels, buf, size);
	}
	/* "s", "S" and "O" give pointers into v: top level only (an item of a group is a copy) */
	CHECK(top || c == 'i' || c == 'l' || c == 'd' || c == 'f');
	if (c == 'i') {
		msg = Mode_ArgInt(v, va_arg(*va, int *)) ? NULL : "integer<i>";
	} else if (c == 'l') {
		msg = Mode_ArgLong(v, va_arg(*va, long *)) ? NULL : "integer<l>";
	} else if (c == 'd') {
		/* PyFloat_AsDouble raised already: seterror keeps its error */
		msg = Mode_ArgDouble(v, va_arg(*va, double *)) ? NULL : "float<d>";
	} else if (c == 'f') {
		/* the double cut to a float, as getargs.c stores it */
		double d;
		float *p = va_arg(*va, float *);
		if (Mode_ArgDouble(v, &d)) {
			*p = (float)d;
		} else {
			msg = "float<f>";
		}
	} else if (c == 'S') {
		/* a text of any bytes: the argument itself */
		if (v->type == NV_STR) {
			*va_arg(*va, const nativeVal_t **) = v;
		} else {
			(void)va_arg(*va, const nativeVal_t **);
			msg = "string";
		}
	} else if (c == 's') {
		const char **p = va_arg(*va, const char **);
		if (v->type != NV_STR) {
			msg = "string";
		} else {
			/* convertsimple1 stores the text before it checks for null bytes (SAI's state report prints it) */
			*p = Native_Text(v);
			if (strlen(*p) != (size_t)v->n) {
				msg = "string without null bytes";
			}
		}
	} else {
		CHECK(c == 'O');
		*va_arg(*va, const nativeVal_t **) = v;
	}
	if (msg) {
		/* convertsimple: the type of the value after the expected one */
		levels[0] = 0;
		snprintf(buf, size, "%.50s, %.50s", msg, Native_TypeName(v));
		msg = buf;
	}
	return msg;
}

byte Mode_Parse(const nativeVal_t *a, int n, const char *fmt, ...) {
	int min, max, levels[32];
	char buf[256];
	const char *f = fmt, *msg = NULL;
	va_list va;
	Mode_FmtCount(fmt, &min, &max);
	if (!Mode_Arity(n, min, max)) {
		return 0;
	}
	va_start(va, fmt);
	int i;
	for (i = 0; i < n && !msg; i++) {
		if (*f == '|') {
			f++;
		}
		msg = Mode_ParseItem(&a[i], &f, &va, levels, buf, sizeof(buf), 1);
	}
	va_end(va);
	if (!msg) {
		return 1;
	}
	if (Native_Failed()) {
		return 0;
	}
	/* seterror: "argument <i>[, item <j>...]: expected <msg> found" */
	char where[128];
	int k = snprintf(where, sizeof(where), "argument %d", i);
	for (int l = 0; levels[l] > 0 && k < (int)sizeof(where); l++) {
		k += snprintf(where + k, sizeof(where) - k, ", item %d", levels[l] - 1);
	}
	return Native_Raise(NE_TYPE, "%s: expected %s found", where, msg);
}

PyObject *Mode_SelfEntry(PyObject *self, PyObject *args, portEntry_t entry) {
	nativeVal_t a[1 + PORT_ARGS], out;
	/* more than PORT_ARGS arguments: no entry takes them, its arity error comes before it reads one */
	int k = self != NULL, n = (int)PyTuple_Size(args), m = n < PORT_ARGS ? n : PORT_ARGS;
	if (self) {
		Native_Arg(self, &a[0]);
	}
	for (int i = 0; i < m; i++) {
		Native_Arg(PyTuple_GET_ITEM(args, i), &a[k + i]);
	}
	Native_SetNone(&out);
	return Mode_KwOut(a, k + m, entry(a, k + n, &out), &out);
}

PyObject *Mode_Entry(PyObject *args, portEntry_t entry) {
	return Mode_SelfEntry(NULL, args, entry);
}

PyObject *Mode_KwCall(PyObject *args, PyObject *kw, const char *fmt, char **names, modeKwCore_t core) {
	PyObject *o[PORT_ARGS] = { NULL };
	nativeVal_t v[PORT_ARGS], out;
	const nativeVal_t *a[PORT_ARGS] = { NULL };
	int min, max;
	Mode_FmtCount(fmt, &min, &max);
	CHECK(max <= PORT_ARGS);
	if (!PyArg_ParseTupleAndKeywords(args, kw, (char *)fmt, names, &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6],
		&o[7], &o[8], &o[9], &o[10])) {
		return NULL;
	}
	for (int i = 0; i < max; i++) {
		if (o[i]) {
			Native_Arg(o[i], &v[i]);
			a[i] = &v[i];
		}
	}
	Native_SetNone(&out);
	byte ok = core(a, &out);
	for (int i = 0; i < max; i++) {
		if (a[i]) {
			Native_Free(&v[i]);
		}
	}
	return Mode_KwOut(NULL, 0, ok, &out);
}

byte Mode_KwGiven(const nativeVal_t *a, int n, int i) {
	return i < n && !Native_IsMissing(&a[i]);
}

byte Mode_KwEntry(const nativeVal_t *a, int n, const char *fmt, modeKwCore_t core, nativeVal_t *out) {
	const nativeVal_t *p[PORT_ARGS] = { NULL };
	int min, max, req = 0;
	Mode_FmtCount(fmt, &min, &max);
	if (!Mode_Arity(n, min, max)) {
		return 0;
	}
	/* a required argument skipped (Mode_KwSkip): the arity error of the keywords that give fewer */
	for (int i = 0; i < n; i++) {
		p[i] = Mode_KwGiven(a, n, i) ? &a[i] : NULL;
		req += i < min && p[i];
	}
	return Mode_Arity(req < min ? req : min, min, max) && core(p, out);
}

PyObject *Mode_KwIdEntry(PyObject *args, PyObject *kw, const char *fmt, char **names, portEntry_t entry) {
	PyObject *o[PORT_ARGS - 1] = { NULL };
	nativeVal_t a[PORT_ARGS], out;
	int id, min, max, n = 1;
	Mode_FmtCount(fmt, &min, &max);
	/* the arguments given are the first n: the optional ones cannot leave a gap */
	CHECK(fmt[0] == 'i' && max - min <= 1 && max <= PORT_ARGS);
	if (!PyArg_ParseTupleAndKeywords(args, kw, (char *)fmt, names, &id, &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6],
		&o[7], &o[8], &o[9])) {
		return NULL;
	}
	Native_SetInt(&a[0], id);
	for (; n < max && o[n - 1]; n++) {
		Native_Arg(o[n - 1], &a[n]);
	}
	Native_SetNone(&out);
	return Mode_KwOut(a, n, entry(a, n, &out), &out);
}

/* ---- the native entries of stage 3 batch A1 (hand, keyword and PyArg adapters on their entries) ---- */

void Mode_KwSkip(nativeVal_t *v) {
	Native_SetMissing(v);
}

byte Mode_KwSlots(const nativeVal_t *a, int n, const char *fmt, nativeVal_t *v, byte *given) {
	int min, max, req = 0;
	Mode_FmtCount(fmt, &min, &max);
	for (int i = 0; i < max; i++) {
		given[i] = Mode_KwGiven(a, n, i);
		if (given[i]) {
			v[i] = a[i];
		} else {
			Native_SetNone(&v[i]);
		}
		req += i < min && given[i];
	}
	/* PyArg_ParseTupleAndKeywords: the count, then the required ones given by position or name */
	if (n > max) {
		return Mode_Arity(n, min, max);
	}
	return Mode_Arity(req, min, max);
}

void Mode_KwDefS(nativeVal_t *v, const byte *given, int i, const char *s) {
	if (!given[i]) {
		Native_SetStr(&v[i], s);
	}
}

void Mode_KwDefI(nativeVal_t *v, const byte *given, int i, long l) {
	if (!given[i]) {
		Native_SetInt(&v[i], l);
	}
}

void Mode_KwDefF(nativeVal_t *v, const byte *given, int i, double f) {
	if (!given[i]) {
		Native_SetFloat(&v[i], f);
	}
}

PyObject *Mode_KwAdapter(PyObject *args, PyObject *kw, const char *fmt, char **names, portEntry_t entry) {
	nativeVal_t a[8], out;
	byte given[8];
	int n = 0;
	while (names[n]) {
		n++;
	}
	if (!Mode_Kw(args, kw, fmt, names, n, a, given)) {
		return NULL;
	}
	for (int i = 0; i < n; i++) {
		if (!given[i]) {
			Mode_KwSkip(&a[i]);
		}
	}
	Native_SetNone(&out);
	return Mode_KwOut(a, n, entry(a, n, &out), &out);
}

/* ---- string and math of the ports (stage 3, batch B1): native twins of the functions of strop and math that the
 * ports call (PORT_CALL(string, find, ...) on the refs below: no seam, OBJLIB 9; an NV_OBJ argument and
 * SCRAP_PORT_SLOW=1 call the module's function by name, include/scrap/scriptnative.h) ---- */

#ifndef _WIN32
/* src/standalone/crtposix.c: the libm of the Windows runtime, as the math module of Python sees it (pyshim.h) */
double crt_ms_fmod(double x, double y);
double crt_ms_sqrt(double x);
#define LIB_FMOD crt_ms_fmod
#define LIB_SQRT crt_ms_sqrt
#else
#define LIB_FMOD fmod
#define LIB_SQRT sqrt
#endif

/* "t#" of getargs.c: the bytes of a text, 0 bytes too */
static byte Lib_Text(const nativeVal_t *v, int arg, const char **s, int *len) {
	if (v->type != NV_STR) {
		return Native_Raise(NE_TYPE, "argument %d: expected read-only character buffer, %s found", arg, Native_TypeName(v));
	}
	*s = Native_Text(v);
	*len = v->n;
	return 1;
}

/* mymemfind of stropmodule.c: the first place of pat in the len bytes of mem, -1 when it is not there */
static int Lib_MemFind(const char *mem, int len, const char *pat, int plen) {
	len -= plen;
	for (int i = 0; i <= len; i++) {
		if (mem[i] == pat[0] && (plen == 1 || !memcmp(&mem[i + 1], &pat[1], (size_t)(plen - 1)))) {
			return i;
		}
	}
	return -1;
}

/* find(s, sub[, start[, end]]): the first index of sub in s[start:end], -1 when it is not there */
static byte Lib_Find(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s = NULL, *sub = NULL;
	int len = 0, m = 0, i = 0, last = INT_MAX;
	if (!Mode_Arity(n, 2, 4) || !Lib_Text(&a[0], 1, &s, &len) || !Lib_Text(&a[1], 2, &sub, &m) ||
		(n > 2 && !Native_ArgInt(&a[2], &i)) || (n > 3 && !Native_ArgInt(&a[3], &last))) {
		return 0;
	}
	if (last > len) {
		last = len;
	}
	if (last < 0) {
		last += len;
	}
	if (last < 0) {
		last = 0;
	}
	if (i < 0) {
		i += len;
	}
	if (i < 0) {
		i = 0;
	}
	if (m == 0 && i <= last) {
		Native_SetInt(out, i);
		return 1;
	}
	last -= m;
	for (; i <= last; i++) {
		if (s[i] == sub[0] && (m == 1 || !memcmp(&s[i + 1], &sub[1], (size_t)(m - 1)))) {
			Native_SetInt(out, i);
			return 1;
		}
	}
	Native_SetInt(out, -1);
	return 1;
}

/* replace(str, old, new[, maxsplit]): mymemreplace of stropmodule.c (maxsplit 0 or below: all) */
static byte Lib_Replace(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *str = NULL, *pat = NULL, *sub = NULL;
	int len = 0, plen = 0, slen = 0, count = 0;
	if (!Mode_Arity(n, 3, 4) || !Lib_Text(&a[0], 1, &str, &len) || !Lib_Text(&a[1], 2, &pat, &plen) ||
		!Lib_Text(&a[2], 3, &sub, &slen) || (n > 3 && !Native_ArgInt(&a[3], &count))) {
		return 0;
	}
	if (plen <= 0) {
		return Native_Raise(NE_VALUE, "empty pattern string");
	}
	/* mymemcnt: the number of places (none for an empty text or a longer pattern) */
	int found = 0;
	if (len && plen <= len) {
		for (int rest = len, at = 0, off; rest >= 0 && (off = Lib_MemFind(str + at, rest, pat, plen)) != -1; found++) {
			at += off + plen;
			rest -= off + plen;
		}
	}
	if (count > 0 && found > count) {
		found = count;
	}
	if (!found) {
		Native_SetText(out, str, len);
		return 1;
	}
	nativeBuf_t b;
	Native_BufInit(&b);
	while (len > 0) {
		int off = Lib_MemFind(str, len, pat, plen);
		if (off == -1) {
			break;
		}
		Native_BufAdd(&b, str, off);
		Native_BufAdd(&b, sub, slen);
		str += off + plen;
		len -= off + plen;
		if (--count == 0) {
			break;
		}
	}
	if (len > 0) {
		Native_BufAdd(&b, str, len);
	}
	Native_SetText(out, b.p ? b.p : "", b.len);
	Native_BufFree(&b);
	return 1;
}

/* atoi(s[, base]): PyOS_strtol of Python (PyOS_strtoul for base 0 and a leading 0) on the text after its blanks */
static byte Lib_Atoi(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s;
	char *end;
	int base = 10;
	if (!Mode_Arity(n, 1, 2) || !Native_ArgStr(&a[0], 1, &s) || (n > 1 && !Native_ArgInt(&a[1], &base))) {
		return 0;
	}
	if ((base != 0 && base < 2) || base > 36) {
		return Native_Raise(NE_VALUE, "invalid base for atoi()");
	}
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	errno = 0;
	long x = base == 0 && s[0] == '0' ? (long)PyOS_strtoul((char *)s, &end, base) : PyOS_strtol((char *)s, &end, base);
	byte bad = end == s || !isalnum((unsigned char)end[-1]);
	while (!bad && *end && isspace((unsigned char)*end)) {
		end++;
	}
	if (bad || *end) {
		return Native_Raise(NE_VALUE, "invalid literal for atoi(): %.200s", s);
	}
	if (errno) {
		return Native_Raise(NE_VALUE, "atoi() literal too large: %.200s", s);
	}
	Native_SetInt(out, x);
	return 1;
}

/* atof(s): strtod of the Windows runtime (crt_strtod) on the text after its blanks */
static byte Lib_Atof(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s;
	char *end;
	if (!Mode_Arity(n, 1, 1) || !Native_ArgStr(&a[0], 1, &s)) {
		return 0;
	}
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	if (!*s) {
		return Native_Raise(NE_VALUE, "empty string for atof()");
	}
	errno = 0;
	double x = crt_strtod(s, &end);
	while (*end && isspace((unsigned char)*end)) {
		end++;
	}
	if (*end) {
		return Native_Raise(NE_VALUE, "invalid literal for atof(): %.200s", s);
	}
	if (errno) {
		return Native_Raise(NE_VALUE, "atof() literal too large: %.200s", s);
	}
	Native_SetFloat(out, x);
	return 1;
}

/* split(s[, sep[, maxsplit]]) and splitfields, the same function of strop ("t#|z#i") */
static byte Lib_Split(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s = NULL, *sep = NULL;
	int len = 0, m = 0, maxsplit = 0;
	if (!Mode_Arity(n, 1, 3) || !Lib_Text(&a[0], 1, &s, &len)) {
		return 0;
	}
	if (n > 1 && a[1].type != NV_NONE) {
		if (a[1].type != NV_STR) {
			return Native_Raise(NE_TYPE, "argument 2: expected read-only buffer, %s found", Native_TypeName(&a[1]));
		}
		sep = Native_Text(&a[1]);
		m = a[1].n;
	}
	if (n > 2 && !Native_ArgInt(&a[2], &maxsplit)) {
		return 0;
	}
	return Native_SplitText(s, len, sep, m, maxsplit, out);
}

/* the result of a function of math: CHECK and math_error of mathmodule.c (a NaN is a range error) */
static byte Lib_MathOut(double x, nativeVal_t *out) {
	if (errno == 0 && !(-HUGE_VAL <= x && x <= HUGE_VAL)) {
		errno = ERANGE;
	}
	if (errno == EDOM) {
		return Native_Raise(NE_VALUE, "math domain error");
	}
	if (errno == ERANGE) {
		return Native_Raise(NE_OVERFLOW, "math range error");
	}
	if (errno) {
		/* the (errno, text) value of PyErr_SetFromErrno */
		nativeVal_t v;
		const char *text = strerror(errno);
		Native_SetEmpty(&v, NV_TUPLE);
		Native_SetInt(Native_Append(&v), errno);
		Native_SetText(Native_Append(&v), text, (int)strlen(text));
		Native_RaiseVal(NE_VALUE, &v);
		Native_Free(&v);
		return 0;
	}
	Native_SetFloat(out, x);
	return 1;
}

/* math_1 of mathmodule.c: PyArg_Parse(args, "d") of a function of old arguments (none: NULL; one: that value; more:
 * the tuple, which PyFloat_AsDouble refuses) */
static byte Lib_Math1(const nativeVal_t *a, int n, nativeVal_t *out, double (*fn)(double)) {
	nativeVal_t t;
	double x;
	if (!n) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	Native_SetEmpty(&t, NV_TUPLE);
	byte ok = Native_ArgDouble(n == 1 ? &a[0] : &t, &x);
	Native_Free(&t);
	if (!ok) {
		return 0;
	}
	errno = 0;
	return Lib_MathOut(fn(x), out);
}

/* math_2: PyArg_Parse(args, "(dd)"), the arguments (or the one value) as a sequence of two numbers */
static byte Lib_Math2(const nativeVal_t *a, int n, nativeVal_t *out, double (*fn)(double, double)) {
	nativeVal_t t, tmp;
	double x[2];
	if (!n) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	/* the arguments as the tuple of old arguments (a view of them, not freed) */
	Native_SetNone(&t);
	t.type = NV_TUPLE;
	t.items = (nativeVal_t *)a;
	t.n = n;
	const nativeVal_t *seq = n == 1 ? &a[0] : &t;
	int len = seq->type == NV_STR || seq->type == NV_LIST || seq->type == NV_TUPLE ? seq->n : seq->type == NV_VEC ? 3 : -1;
	byte ok = 1;
	if (len < 0) {
		ok = Native_Raise(NE_TYPE, "2-sequence, %s", Native_TypeName(seq));
	} else if (len != 2) {
		ok = Native_Raise(NE_TYPE, "2-sequence, %d-sequence", len);
	}
	for (int i = 0; ok && i < 2; i++) {
		Native_SetNone(&tmp);
		const nativeVal_t *item = Native_GetItem(seq, i, 0, &tmp);
		ok = item && Native_ArgDouble(item, &x[i]);
		Native_Free(&tmp);
	}
	if (!ok) {
		return 0;
	}
	errno = 0;
	return Lib_MathOut(fn(x[0], x[1]), out);
}

static double Lib_Fabs(double x) {
	return fabs(x);
}

static double Lib_Sqrt(double x) {
	return LIB_SQRT(x);
}

static double Lib_Fmod(double x, double y) {
	return LIB_FMOD(x, y);
}

static byte Lib_MathSqrt(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_Math1(a, n, out, Lib_Sqrt);
}

static byte Lib_MathFabs(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_Math1(a, n, out, Lib_Fabs);
}

static byte Lib_MathFmod(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_Math2(a, n, out, Lib_Fmod);
}

portRef_t port_string_find = { "string", "find", NULL, Lib_Find, NULL, 0 };
portRef_t port_string_replace = { "string", "replace", NULL, Lib_Replace, NULL, 0 };
portRef_t port_string_atoi = { "string", "atoi", NULL, Lib_Atoi, NULL, 0 };
portRef_t port_string_atof = { "string", "atof", NULL, Lib_Atof, NULL, 0 };
portRef_t port_string_split = { "string", "split", NULL, Lib_Split, NULL, 0 };
portRef_t port_string_splitfields = { "string", "splitfields", NULL, Lib_Split, NULL, 0 };
portRef_t port_math_sqrt = { "math", "sqrt", NULL, Lib_MathSqrt, NULL, 0 };
portRef_t port_math_fabs = { "math", "fabs", NULL, Lib_MathFabs, NULL, 0 };
portRef_t port_math_fmod = { "math", "fmod", NULL, Lib_MathFmod, NULL, 0 };

/* ---- the menus of stage 3 batch B11 ---- */

byte Mode_IdControl(const nativeVal_t *a, int n, int *id, const char **control) {
	const nativeVal_t *c = NULL;
	if (!Mode_Parse(a, n, "i|O", id, &c)) {
		return 0;
	}
	*control = c && c->type == NV_STR ? Native_Text(c) : "";
	return 1;
}

/* ---- calls of values (stage 4 of docs/objlib-plan.md, batch 4d): the entry of the port function a value holds
 * (Port_Value), else the call through Python; one site per call site through the macros of scrap/scriptmodes.h ---- */

byte Mode_CbArgs(portSite_t *site, const nativeVal_t *f, nativeVal_t *out, const nativeVal_t *args, int n) {
	/* a native object natively; a value that is no object is called the Python way too: the TypeError of a value that
	 * is no callable */
	return Port_ValuesOf(site, f, args, n, out, NULL);
}

byte Mode_CbApplyPy(portSite_t *site, PyObject *fn, const nativeVal_t *args, nativeVal_t *out) {
	if (args->type == NV_TUPLE) {
		return Port_Values(site, fn, args->items, args->n, out, NULL);
	}
	/* any other value (a vector is a tuple in Python) through Python: the TypeError of an args that is no tuple */
	PyObject *a = Native_ToPy(args);
	PyObject *r = a ? Script_CallObject(fn, a, NULL) : NULL;
	Py_XDECREF(a);
	if (!r) {
		return 0;
	}
	if (out) {
		Native_Arg(r, out);
	}
	Py_DECREF(r);
	return 1;
}

byte Mode_CbApplySeq(portSite_t *site, PyObject *fn, const nativeVal_t *args, nativeVal_t *out) {
	/* the builtin makes a tuple of a list: the same call */
	if (args->type == NV_TUPLE || args->type == NV_LIST) {
		return Port_Values(site, fn, args->items, args->n, out, NULL);
	}
	return Native_CallMod("__builtin__", "apply", out, "Ov", fn, args);
}

byte Mode_CbApply(portSite_t *site, const nativeVal_t *f, const nativeVal_t *args, nativeVal_t *out) {
	if (NObj_Native(f) && args->type == NV_TUPLE) {
		return Port_ValuesOf(site, f, args->items, args->n, out, NULL);
	}
	PyObject *fn = Native_ToPy(f);
	byte ok = fn && Mode_CbApplyPy(site, fn, args, out);
	Py_XDECREF(fn);
	return ok;
}

byte Mode_CbLink(portSite_t *site, PyObject *fn, int id, const char *a, const char *b) {
	return Port_Value(site, fn, NULL, "iss", (long)id, a, b);
}

byte Mode_CbGlobal(portSite_t *site, const char *module, const char *name, nativeVal_t *out, const char *fmt, ...) {
	va_list va;
	nativeVal_t nf;
	va_start(va, fmt);
	if (Native_NativeVar(module, name, &nf)) {
		byte ok = Port_NativeCallVa(&nf, out, fmt, va);
		va_end(va);
		Native_Free(&nf);
		return ok;
	}
	PyObject *f = Script_Global(module, name);
	byte ok = f && Port_ValueVa(site, f, out, fmt, va);
	va_end(va);
	Py_XDECREF(f);
	return ok;
}

byte Mode_CbAttr(portSite_t *site, const char *module, const char *name, nativeVal_t *out, const char *fmt, ...) {
	va_list va;
	nativeVal_t nf;
	va_start(va, fmt);
	if (Native_NativeVar(module, name, &nf)) {
		byte ok = Port_NativeCallVa(&nf, out, fmt, va);
		va_end(va);
		Native_Free(&nf);
		return ok;
	}
	PyObject *f = Script_Attr(module, name);
	byte ok = f && Port_ValueVa(site, f, out, fmt, va);
	va_end(va);
	Py_XDECREF(f);
	return ok;
}

/* ---- the state of the online and split screen modes on values (stage 6, batch 6f) ---- */

int Mode_Truth(const nativeVal_t *v) {
	/* an object by its kind (no crossing); OBJLIB 7: the truth of a Python object (an instance's __nonzero__ or __len__
	 * may raise) */
	return NObj_Native(v) ? NObj_True(v) : Native_IsObj(v) ? PyObject_IsTrue(Native_PyOf(v)) : Native_True(v);
}

byte Mode_ScheduleV(const nativeVal_t *time, const nativeVal_t *func, const nativeVal_t *args, int n,
	const nativeVal_t *name) {
	PyObject *fn = Native_ToPy(func);
	byte ok = fn && Mode_Schedule(time, fn, args, n, name);
	Py_XDECREF(fn);
	return ok;
}

byte Mode_VSetModel(const nativeVal_t *h, const char *module, const char *modelVar) {
	PyObject *o = Native_ToPy(h);
	byte ok = o && Mode_HSetModel(o, module, modelVar);
	Py_XDECREF(o);
	return ok;
}

byte Mode_ModToV(const char *module, const char *var, const nativeVal_t *o, const char *name) {
	PyObject *po = Native_ToPy(o), *x = po ? PyObject_GetAttrString(po, (char *)name) : NULL;
	byte ok = x && Mode_SetModPy(module, var, x);
	Py_XDECREF(x);
	Py_XDECREF(po);
	return ok;
}

byte Mode_GObjAttr(const char *module, const char *var, const char *name, nativeVal_t *out) {
	nativeVal_t o;
	Native_SetNone(out);
	if (!Mode_G(module, var, &o)) {
		return 0;
	}
	PyObject *po = Native_ToPy(&o), *r = po ? PyObject_GetAttrString(po, (char *)name) : NULL;
	Native_Free(&o);
	Py_XDECREF(po);
	if (!r) {
		return 0;
	}
	Native_Arg(r, out);
	Py_DECREF(r);
	return 1;
}

byte Mode_PutHandle(const char *module, const char *var, cEntity *e) {
	nativeVal_t h;
	byte ok = Mode_EntHandle(e, &h) && Mode_Put(module, var, NULL, &h);
	Native_Free(&h);
	return ok;
}

byte Mode_FlagGet(const char *module, const char *var, nativeVal_t *out) {
	nativeVal_t flag;
	byte ok = Mode_G(module, "Flag", &flag) && Mode_VGet(&flag, var, out);
	Native_Free(&flag);
	return ok;
}

/* ---- the ports without Python objects (stage 6, batch 6g) ---- */

/* Mode_KwTyped; ref: an O argument by reference (Native_SetObj), else as Native_Arg makes it */
static PyObject *Mode_KwParsed(PyObject *args, PyObject *kw, const char *fmt, char **names, portEntry_t entry, byte ref) {
	union {
		char *s;
		int i;
		double d;
		float f;
		PyObject *o;
	} u[PORT_ARGS];
	PyObject *o[PORT_ARGS] = { NULL };
	char plain[PORT_ARGS + 2], type[PORT_ARGS];
	nativeVal_t a[PORT_ARGS], out;
	int n = 0, k = 0;
	for (const char *f = fmt; *f; f++) {
		CHECK(k < (int)sizeof(plain) - 1);
		if (*f != '|') {
			CHECK(n < PORT_ARGS && strchr("sidfO", *f));
			type[n++] = *f;
		}
		plain[k++] = *f == '|' ? '|' : 'O';
	}
	plain[k] = 0;
	/* the parse of the format, then the objects given (the same parse without conversions: it cannot fail) */
	if (!PyArg_ParseTupleAndKeywords(args, kw, (char *)fmt, names, &u[0], &u[1], &u[2], &u[3], &u[4], &u[5], &u[6],
		&u[7], &u[8], &u[9], &u[10]) ||
		!PyArg_ParseTupleAndKeywords(args, kw, plain, names, &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7],
		&o[8], &o[9], &o[10])) {
		return NULL;
	}
	for (int i = 0; i < n; i++) {
		if (!o[i]) {
			Mode_KwSkip(&a[i]);
		} else if (type[i] == 's') {
			Native_SetText(&a[i], u[i].s, (int)strlen(u[i].s));
		} else if (type[i] == 'i') {
			Native_SetInt(&a[i], u[i].i);
		} else if (type[i] == 'd' || type[i] == 'f') {
			Native_SetFloat(&a[i], type[i] == 'd' ? u[i].d : (double)u[i].f);
		} else if (ref) {
			Native_SetObj(&a[i], o[i]);
		} else {
			Native_Arg(o[i], &a[i]);
		}
	}
	Native_SetNone(&out);
	return Mode_KwOut(a, n, entry(a, n, &out), &out);
}

PyObject *Mode_KwTyped(PyObject *args, PyObject *kw, const char *fmt, char **names, portEntry_t entry) {
	return Mode_KwParsed(args, kw, fmt, names, entry, 0);
}

/* the object r as a value into out (r released): 0 for NULL, with its error */
static byte Mode_ObjOut(PyObject *r, nativeVal_t *out) {
	Native_SetNone(out);
	if (!r) {
		return 0;
	}
	Native_Arg(r, out);
	Py_DECREF(r);
	return 1;
}

byte Mode_FromV(const char *module, const char *name, nativeVal_t *out) {
	return Mode_ObjOut(Mode_From(module, name), out);
}

byte Mode_Slice(const nativeVal_t *v, long lo, long hi, nativeVal_t *out) {
	if (!Native_IsObj(v)) {
		return Native_Slice(v, lo, hi, out);
	}
	if (NObj_Native(v)) {
		nativeVal_t r;
		Native_SetNone(out);
		return Mode_NativeOut(NObj_GetSlice(v, lo, hi, &r), &r, out);
	}
	return Mode_ObjOut(PySequence_GetSlice(Native_PyOf(v), (int)lo, (int)hi), out);
}

byte Mode_Sliceable(const nativeVal_t *v) {
	if (!Native_IsObj(v)) {
		return v->type == NV_STR || v->type == NV_LIST || v->type == NV_TUPLE || v->type == NV_VEC;
	}
	PyObject *p = Native_PyOf(v);
	PySequenceMethods *sq = p ? p->ob_type->tp_as_sequence : NULL;
	return sq && sq->sq_slice;
}

byte Mode_RefTake(PyObject *o, nativeVal_t *out) {
	Native_SetNone(out);
	if (!o) {
		return 0;
	}
	Native_SetObj(out, o);
	Py_DECREF(o);
	return 1;
}

byte Mode_RefG(const char *module, const char *name, nativeVal_t *out) {
	return ModVar_GlobalVal(module, name, out);
}

byte Mode_RefMod(const char *module, const char *var, nativeVal_t *out) {
	return ModVar_ModAttrVal(module, var, out);
}

void Mode_RefPlain(const nativeVal_t *o, nativeVal_t *out) {
	if (NObj_Native(o)) {
		NObj_ArgOf(o, out);
	} else if (Native_IsObj(o) && Native_PyOf(o)) {
		Native_Arg(Native_PyOf(o), out);
	} else {
		Native_Copy(out, o);
	}
}

/* the kinds of native objects that are no sequence for PySequence_GetItem and PySequence_Length (1.5.2: no
 * tp_as_sequence; an instance has one, it asks __getitem__ and __len__) */
static byte Mode_RefNoSeq(const nativeVal_t *o) {
	int k = NObj_Kind(o);
	return k != NO_LIST && k != NO_INSTANCE;
}

int Mode_RefLen(const nativeVal_t *o) {
	if (NObj_Native(o)) {
		return Mode_RefNoSeq(o) ? (Native_Raise(NE_TYPE, "len() of unsized object"), -1) : NObj_Len(o);
	}
	PyObject *p = Native_ToPy(o);
	int n = p ? PySequence_Length(p) : -1;
	Py_XDECREF(p);
	return n;
}

int Mode_RefSize(const nativeVal_t *o) {
	if (NObj_Native(o)) {
		return NObj_Len(o);
	}
	PyObject *p = Native_ToPy(o);
	int n = p ? PyObject_Length(p) : -1;
	Py_XDECREF(p);
	return n;
}

/* o[i] (x NULL) or o[i] = x of a native object as PySequence_GetItem and PySequence_SetItem take it: a negative
 * index plus the length once, then the item of the list or the instance's special method */
static byte Mode_RefSeqAt(const nativeVal_t *o, long i, const nativeVal_t *x, nativeVal_t *out) {
	nativeVal_t key;
	if (Mode_RefNoSeq(o)) {
		return Native_Raise(NE_TYPE, x ? "object doesn't support item assignment" : "unindexable object");
	}
	if (i < 0 && NObj_Kind(o) == NO_LIST) {
		i += NObj_Len(o);
		if (i < 0) {
			return Native_Raise(NE_INDEX, x ? "list assignment index out of range" : "list index out of range");
		}
	}
	Native_SetInt(&key, i);
	return x ? NObj_SetItem(o, &key, x) : NObj_GetItem(o, &key, out);
}

byte Mode_RefItem(const nativeVal_t *o, long i, nativeVal_t *out) {
	if (NObj_Native(o)) {
		return Mode_RefSeqAt(o, i, NULL, out);
	}
	PyObject *p = Native_ToPy(o);
	byte ok = Mode_RefTake(p ? PySequence_GetItem(p, (int)i) : NULL, out);
	Py_XDECREF(p);
	return ok;
}

/* op 0: o[key] into out, 1: o[key] = v, 2: del o[key] */
static byte Mode_RefKey(const nativeVal_t *o, const nativeVal_t *key, int op, const nativeVal_t *v, nativeVal_t *out) {
	PyObject *p = Native_ToPy(o), *k = p ? Native_ToPy(key) : NULL, *x = k && v ? Native_ToPy(v) : NULL;
	byte ok = k && (!v || x);
	if (ok && op == 0) {
		ok = Mode_RefTake(PyObject_GetItem(p, k), out);
	} else if (ok) {
		ok = (op == 1 ? PyObject_SetItem(p, k, x) : PyObject_DelItem(p, k)) == 0;
	} else if (out) {
		Native_SetNone(out);
	}
	Py_XDECREF(x);
	Py_XDECREF(k);
	Py_XDECREF(p);
	return ok;
}

byte Mode_RefSub(const nativeVal_t *o, const nativeVal_t *key, nativeVal_t *out) {
	return NObj_Native(o) ? NObj_GetItem(o, key, out) : Mode_RefKey(o, key, 0, NULL, out);
}

byte Mode_RefSetItem(const nativeVal_t *o, long i, const nativeVal_t *v) {
	if (NObj_Native(o)) {
		return Mode_RefSeqAt(o, i, v, NULL);
	}
	PyObject *p = Native_ToPy(o), *x = p ? Native_ToPy(v) : NULL;
	byte ok = x && PySequence_SetItem(p, (int)i, x) == 0;
	Py_XDECREF(x);
	Py_XDECREF(p);
	return ok;
}

byte Mode_RefSetSub(const nativeVal_t *o, const nativeVal_t *key, const nativeVal_t *v) {
	return NObj_Native(o) ? NObj_SetItem(o, key, v) : Mode_RefKey(o, key, 1, v, NULL);
}

byte Mode_RefDelSub(const nativeVal_t *o, const nativeVal_t *key) {
	return NObj_Native(o) ? NObj_DelItem(o, key) : Mode_RefKey(o, key, 2, NULL, NULL);
}

byte Mode_RefAttr(const nativeVal_t *o, const char *name, nativeVal_t *out) {
	if (NObj_Native(o)) {
		return NObj_GetAttr(o, name, out);
	}
	PyObject *p = Native_ToPy(o);
	byte ok = Mode_RefTake(p ? PyObject_GetAttrString(p, (char *)name) : NULL, out);
	Py_XDECREF(p);
	return ok;
}

byte Mode_RefSetAttr(const nativeVal_t *o, const char *name, const nativeVal_t *v) {
	if (NObj_Native(o)) {
		return NObj_SetAttr(o, name, v);
	}
	PyObject *p = Native_ToPy(o), *x = p ? Native_ToPy(v) : NULL;
	byte ok = x && PyObject_SetAttrString(p, (char *)name, x) == 0;
	Py_XDECREF(x);
	Py_XDECREF(p);
	return ok;
}

byte Mode_RefCall(const nativeVal_t *o, const char *method, const nativeVal_t *args, int n, nativeVal_t *out) {
	if (NObj_Native(o)) {
		nativeVal_t r;
		byte ok = NObj_CallMethod(o, method, args, n, &r);
		if (out) {
			*out = r;
		} else {
			Native_Free(&r);
		}
		return ok;
	}
	/* the method as Script_Method reads it (Script_GetAttr: an object that fails without an exception, the handle of a
	 * deleted entity, gets ceval's SystemError) */
	PyObject *p = Native_ToPy(o), *f = p ? Script_GetAttr(p, method) : NULL;
	PyObject *params = f ? Mode_ArgsTuple(args, n) : NULL, *r = params ? Script_CallObject(f, params, NULL) : NULL;
	Py_XDECREF(params);
	Py_XDECREF(f);
	Py_XDECREF(p);
	if (out) {
		return Mode_ObjOut(r, out);
	}
	Py_XDECREF(r);
	return r != NULL;
}

byte Mode_RefInsert(const nativeVal_t *o, int at, const nativeVal_t *v) {
	if (NObj_Native(o)) {
		if (NObj_Kind(o) != NO_LIST) {
			/* PyList_Insert's PyErr_BadInternalCall */
			return Native_Raise(NE_SYSTEM, "bad argument to internal function");
		}
		nativeVal_t a[2], r;
		Native_SetInt(&a[0], at);
		a[1] = *v;
		byte ok = at < 0 ? NObj_CallMethod(o, "append", v, 1, &r) : NObj_CallMethod(o, "insert", a, 2, &r);
		Native_Free(&r);
		return ok;
	}
	PyObject *p = Native_ToPy(o), *x = p ? Native_ToPy(v) : NULL;
	byte ok = x && (at < 0 ? PyList_Append(p, x) : PyList_Insert(p, at, x)) == 0;
	Py_XDECREF(x);
	Py_XDECREF(p);
	return ok;
}

byte Mode_RefToMod(const char *module, const char *var, const nativeVal_t *v) {
	if (!Native_Holds(v, 1u << NV_OBJ)) {
		/* the value as it is (an object of native data stays one) */
		return ModVar_SetModAttrVal(module, var, v);
	}
	PyObject *x = Native_ToPy(v);
	byte ok = Mode_SetModPy(module, var, x);
	Py_XDECREF(x);
	return ok;
}

int Mode_RefHasKey(const nativeVal_t *d, const nativeVal_t *key) {
	nativeVal_t r;
	int t = Mode_RefCall(d, "has_key", key, 1, &r) ? Mode_Truth(&r) : -1;
	Native_Free(&r);
	return t;
}

byte Mode_VBoundV(const nativeVal_t *h, const char *method, nativeVal_t *out) {
	return Mode_RefTake(Mode_VBound(h, method), out);
}

int Mode_EvalChain(const execScope_t *x, const char *text, nativeVal_t *out) {
	/* the module dict as the globals: the key of its variables in the registry (Script_NameChain reads through it) */
	PyObject *m = Script_Module(x->module), *g = m && PyModule_Check(m) ? PyModule_GetDict(m) : NULL;
	PyObject *loc = g ? PyDict_New() : NULL, *v = NULL;
	if (m && !g) {
		/* what module_getattr of no module gives for __dict__ */
		Py_DECREF(m);
		m = NULL;
		Native_Raise(NE_ATTRIBUTE, "__dict__");
	}
	byte ok = loc != NULL;
	Native_SetNone(out);
	for (int i = 0; ok && i < x->n; i++) {
		PyObject *o = x->vars[i].v ? Native_ToPy(x->vars[i].v) : Script_Module(x->vars[i].name);
		ok = o && PyDict_SetItemString(loc, (char *)x->vars[i].name, o) == 0;
		Py_XDECREF(o);
	}
	int found = ok ? Script_NameChain(loc, g, text, &v) : -1;
	Py_XDECREF(loc);
	Py_XDECREF(m);
	if (!ok) {
		return 0;
	}
	if (found > 0) {
		Native_Arg(v, out);
		Py_DECREF(v);
	}
	return found > 0 ? 1 : found < 0 ? 0 : -1;
}

byte Mode_RefModule(const char *name, nativeVal_t *out) {
	return Mode_RefTake(Script_Module(name), out);
}

byte Mode_CatchKeyError(void) {
	if (!Native_ErrIs(NE_KEY)) {
		return 0;
	}
	Native_ErrClear();
	return 1;
}

byte Mode_WriteLine(const nativeVal_t *const *items, int n) {
	PyObject *out = PySys_GetObject("stdout"), *s[MODE_PRINT_MAX] = { NULL };
	byte ok = out != NULL;
	n = n < MODE_PRINT_MAX ? n : MODE_PRINT_MAX;
	/* every text is made before the first is written; a failed one does not stop the others */
	for (int i = 0; i < n; i++) {
		PyObject *o = Native_ToPy(items[i]);
		s[i] = o ? PyObject_Str(o) : NULL;
		Py_XDECREF(o);
		ok = ok && s[i];
	}
	for (int i = 0; ok && i < n; i++) {
		ok = PyFile_WriteObject(s[i], out, Py_PRINT_RAW) == 0 && PyFile_WriteString(i + 1 < n ? " " : "\n", out) == 0;
	}
	for (int i = 0; i < n; i++) {
		Py_XDECREF(s[i]);
	}
	return ok;
}

byte Mode_RefGetAttr(const nativeVal_t *o, const char *name, nativeVal_t *out) {
	if (NObj_Native(o)) {
		return NObj_GetAttr(o, name, out);
	}
	PyObject *p = Native_ToPy(o);
	byte ok = Mode_RefTake(p ? Script_GetAttr(p, name) : NULL, out);
	Py_XDECREF(p);
	return ok;
}

PyObject *Mode_RefEntry(PyObject *args, portEntry_t entry) {
	nativeVal_t a[PORT_ARGS], out;
	/* more than PORT_ARGS arguments: no entry takes them, its arity error comes before it reads one (Mode_Entry) */
	int n = (int)PyTuple_Size(args), m = n < PORT_ARGS ? n : PORT_ARGS;
	for (int i = 0; i < m; i++) {
		Native_SetObj(&a[i], PyTuple_GET_ITEM(args, i));
	}
	Native_SetNone(&out);
	return Mode_KwOut(a, m, entry(a, n, &out), &out);
}

int Mode_RefNext(const nativeVal_t *o, int i, nativeVal_t *out) {
	if (NObj_Native(o)) {
		/* Script_Next: PySequence_Check, then the item up to the IndexError */
		Native_SetNone(out);
		if (Mode_RefNoSeq(o)) {
			Native_Raise(NE_TYPE, "loop over non-sequence");
			return -1;
		}
		if (Mode_RefSeqAt(o, i, NULL, out)) {
			return 1;
		}
		if (Native_ErrIs(NE_INDEX)) {
			Native_ErrClear();
			return 0;
		}
		return -1;
	}
	PyObject *p = Native_ToPy(o), *item = NULL;
	int r = p ? Script_Next(p, i, &item) : -1;
	Py_XDECREF(p);
	Native_SetNone(out);
	return r > 0 ? Mode_RefTake(item, out) : r;
}

/* a op b the Python way (OBJLIB 9): '+', '-', '*', '/', '&', else '%'; a new reference, NULL with the error */
static PyObject *Mode_NumOp(char op, const nativeVal_t *a, const nativeVal_t *b) {
	PyObject *x = Native_ToPy(a), *y = x ? Native_ToPy(b) : NULL, *r = NULL;
	if (y) {
		r = op == '+' ? PyNumber_Add(x, y) : op == '-' ? PyNumber_Subtract(x, y) : op == '*' ? PyNumber_Multiply(x, y) :
			op == '/' ? PyNumber_Divide(x, y) : op == '&' ? PyNumber_And(x, y) : PyNumber_Remainder(x, y);
	}
	Py_XDECREF(y);
	Py_XDECREF(x);
	return r;
}

byte Mode_RefOp(char op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	return Mode_RefTake(Mode_NumOp(op, a, b), out);
}

int Mode_RefCmp(const nativeVal_t *a, const nativeVal_t *b) {
	if ((NObj_Native(a) || NObj_Native(b)) && !Native_Holds(a, 1u << NV_OBJ) && !Native_Holds(b, 1u << NV_OBJ)) {
		return NObj_Compare(a, b);
	}
	PyObject *x = Native_ToPy(a), *y = x ? Native_ToPy(b) : NULL;
	int c = y ? PyObject_Compare(x, y) : -2;
	if (y) {
		/* cmp_outcome of ceval: an error only counts with a result that is not 0 */
		c = c && Native_Failed() ? -2 : c < 0 ? -1 : c > 0;
	}
	Py_XDECREF(y);
	Py_XDECREF(x);
	return c;
}

byte Mode_RefRepr(const nativeVal_t *o, nativeVal_t *out) {
	if (NObj_Native(o)) {
		nativeBuf_t b;
		Native_BufInit(&b);
		byte ok = NObj_Repr(&b, o, NR_REPR);
		Native_SetNone(out);
		if (ok) {
			Native_SetText(out, b.p, b.len);
		}
		Native_BufFree(&b);
		return ok;
	}
	PyObject *p = Native_ToPy(o);
	byte ok = Mode_RefTake(p ? PyObject_Repr(p) : NULL, out);
	Py_XDECREF(p);
	return ok;
}

byte Mode_RefSlice(const nativeVal_t *o, long lo, long hi, nativeVal_t *out) {
	if (NObj_Native(o)) {
		return NObj_GetSlice(o, lo, hi, out);
	}
	PyObject *p = Native_ToPy(o);
	byte ok = Mode_RefTake(p ? PySequence_GetSlice(p, (int)lo, (int)hi) : NULL, out);
	Py_XDECREF(p);
	return ok;
}

byte Mode_RefIsNone(const nativeVal_t *o) {
	return o->type == NV_NONE || (Native_IsObj(o) && Native_PyOf(o) == Py_None);
}

PyObject *Mode_RefKwAdapter(PyObject *args, PyObject *kw, const char *fmt, char **names, portEntry_t entry) {
	return Mode_KwParsed(args, kw, fmt, names, entry, 1);
}

byte Mode_RefApply(const nativeVal_t *f, const nativeVal_t *args, int n, nativeVal_t *out) {
	if (NObj_Native(f)) {
		nativeVal_t r;
		byte ok = NObj_Call(f, args, n, &r);
		if (out) {
			*out = r;
		} else {
			Native_Free(&r);
		}
		return ok;
	}
	PyObject *fn = Native_ToPy(f), *params = fn ? Mode_ArgsTuple(args, n) : NULL;
	PyObject *r = params ? Script_CallObject(fn, params, NULL) : NULL;
	Py_XDECREF(params);
	Py_XDECREF(fn);
	if (out) {
		return Mode_RefTake(r, out);
	}
	Py_XDECREF(r);
	return r != NULL;
}

byte Mode_RefDelAttr(const nativeVal_t *o, const char *name) {
	if (NObj_Native(o)) {
		return NObj_DelAttr(o, name);
	}
	PyObject *p = Native_ToPy(o);
	byte ok = p && PyObject_SetAttrString(p, (char *)name, NULL) == 0;
	Py_XDECREF(p);
	return ok;
}

byte Mode_ToLong(const nativeVal_t *v, nativeVal_t *out) {
	PyObject *o = Native_ToPy(v);
	byte ok = Mode_ObjOut(o ? PyNumber_Long(o) : NULL, out);
	Py_XDECREF(o);
	return ok;
}

/* ---- the menus, the ship editor and the other ports on the native helpers (stage 6, batch 6h) ---- */

byte Mode_DefRunV(int id, const char *file, const char *section, const char *builtin, const struct scriptStep_s *steps,
	byte stop, const char *const *names, const nativeVal_t *const *vals, int n) {
	/* OBJLIB 6: the $names as the dict the runner takes (NULL for none); a dict that could not be made runs nothing */
	PyObject *vars = n ? Mode_DefDict(names, vals, n) : NULL;
	if (n && !vars) {
		return 0;
	}
	byte ok = stop ? Script_RunFileTry(id, file, section, builtin, steps, vars) :
		Script_RunFile(id, file, section, builtin, steps, vars);
	Py_XDECREF(vars);
	return ok;
}

byte Mode_DefRunVa(int id, const char *file, const char *section, const char *builtin, const struct scriptStep_s *steps,
	byte stop, int n, va_list va) {
	const char *names[MODE_DEFVARS];
	const nativeVal_t *vals[MODE_DEFVARS];
	Mode_DefPairs(n, va, names, vals);
	return Mode_DefRunV(id, file, section, builtin, steps, stop, names, vals, n);
}

byte Mode_DefRun(int id, const char *file, const char *section, const char *builtin, const struct scriptStep_s *steps,
	byte stop, int n, ...) {
	va_list va;
	va_start(va, n);
	byte ok = Mode_DefRunVa(id, file, section, builtin, steps, stop, n, va);
	va_end(va);
	return ok;
}


byte Mode_CbLinkV(portSite_t *site, const nativeVal_t *fn, int id, const char *a, const char *b) {
	PyObject *f = Native_ToPy(fn);
	byte ok = f && Mode_CbLink(site, f, id, a, b);
	Py_XDECREF(f);
	return ok;
}

byte Mode_StrBuf(const nativeVal_t *v, nativeBuf_t *b) {
	if (!Native_IsObj(v)) {
		Native_Repr(b, v, NR_STR);
		return 1;
	}
	if (NObj_Native(v)) {
		return NObj_Repr(b, v, NR_STR);
	}
	PyObject *t = PyObject_Str(Native_PyOf(v));
	if (!t) {
		return 0;
	}
	Native_BufAdd(b, PyString_AsString(t), PyString_Size(t));
	Py_DECREF(t);
	return 1;
}

long Mode_AsLong(const nativeVal_t *v) {
	PyObject *o = Native_ToPy(v);
	long l = o ? PyInt_AsLong(o) : -1;
	Py_XDECREF(o);
	return l;
}

void Mode_SetValue(const char *name, const nativeVal_t *v) {
	Script_SetValue(name, Native_ToPy(v));
}

byte Mode_OwnFunc(PyMethodDef *table, const char *name, nativeVal_t *fn) {
	Native_SetNone(fn);
	for (PyMethodDef *row = table; row->ml_name; row++) {
		if (!strcmp(row->ml_name, name)) {
			PyObject *f = PyCFunction_New(row, NULL);
			if (!f) {
				return 0;
			}
			Native_SetObj(fn, f);
			Py_DECREF(f);
			return 1;
		}
	}
	return Native_Raise(NE_ATTRIBUTE, "%s", name);
}

byte Mode_SchedFn(double time, const nativeVal_t *fn, const nativeVal_t *args, int n, const char *name) {
	PyObject *f = Native_ToPy(fn);
	byte ok = f && Mode_SchedAdd((float)time, f, args, n, name ? name : "");
	Py_XDECREF(f);
	return ok;
}

byte Mode_VarCall0(const char *module, const char *var, byte global, const char *method) {
	nativeVal_t r;
	byte ok = Mode_VarMethod(module, var, global, method, NULL, 0, &r);
	Native_Free(&r);
	return ok;
}

/* Mode_NumOp with the result as Native_Arg makes it (Mode_RefOp keeps it by reference); *out None when it fails */
static byte Mode_PyOp(char op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	PyObject *r = Mode_NumOp(op, a, b);
	Native_SetNone(out);
	if (r) {
		Native_Arg(r, out);
	}
	Py_XDECREF(r);
	return r != NULL;
}

byte Mode_And(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	if (a->type == NV_INT && b->type == NV_INT) {
		Native_SetInt(out, a->i & b->i);
		return 1;
	}
	return Mode_PyOp('&', a, b, out);
}

byte Mode_Times(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	return Mode_PyOp('*', a, b, out);
}

byte Mode_ModAt(const char *module, const char *var, const nativeVal_t *key, nativeVal_t *out) {
	nativeVal_t d;
	if (Mode_RefMod(module, var, &d) && NObj_Native(&d)) {
		/* a dict or list object of a pack module: its item natively */
		byte ok = Mode_Sub(&d, key, out);
		Native_Free(&d);
		return ok;
	}
	Native_Free(&d);
	Native_ErrClear();
	PyObject *o = Script_Attr(module, var), *k = o ? Native_ToPy(key) : NULL;
	PyObject *x = k ? PyObject_GetItem(o, k) : NULL;
	Native_SetNone(out);
	if (x) {
		Native_Arg(x, out);
	}
	Py_XDECREF(x);
	Py_XDECREF(k);
	Py_XDECREF(o);
	return x != NULL;
}

byte Mode_Plus(const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	return Mode_PyOp('+', a, b, out);
}

byte Mode_SetItem(nativeVal_t *v, const nativeVal_t *key, const nativeVal_t *x) {
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

byte Mode_Quotable(const nativeVal_t *text) {
	return text->type == NV_STR && Script_IsQuotableText(Native_Text(text), text->n);
}


byte Mode_CbApplySeqV(portSite_t *site, const nativeVal_t *f, const nativeVal_t *args, nativeVal_t *out) {
	PyObject *fn = Native_ToPy(f);
	byte ok = fn && Mode_CbApplySeq(site, fn, args, out);
	Py_XDECREF(fn);
	return ok;
}

PyObject *Mode_EntryObj(PyObject *args, portEntry_t entry, unsigned objs) {
	nativeVal_t a[PORT_ARGS], out;
	int n = (int)PyTuple_Size(args), m = n < PORT_ARGS ? n : PORT_ARGS;
	for (int i = 0; i < m; i++) {
		PyObject *o = PyTuple_GET_ITEM(args, i);
		if (objs >> i & 1) {
			/* the object itself, also an entity handle: the entry stores or changes this very object */
			Native_SetNone(&a[i]);
			a[i].type = NV_OBJ;
			a[i].obj = o;
			Py_INCREF(o);
		} else {
			Native_Arg(o, &a[i]);
		}
	}
	Native_SetNone(&out);
	return Mode_KwOut(a, m, entry(a, n, &out), &out);
}

byte Mode_Int(const nativeVal_t *v, nativeVal_t *out) {
	if (!Native_IsObj(v)) {
		return Native_ToInt(v, out);
	}
	PyObject *r = PyNumber_Int(Native_PyOf(v));
	if (!r) {
		return 0;
	}
	Native_Arg(r, out);
	Py_DECREF(r);
	return 1;
}

byte Mode_ReprV(const nativeVal_t *v, nativeVal_t *out) {
	PyObject *o = Native_ToPy(v), *r = o ? PyObject_Repr(o) : NULL;
	Native_SetNone(out);
	if (r) {
		Native_SetText(out, PyString_AsString(r), PyString_Size(r));
	}
	Py_XDECREF(r);
	Py_XDECREF(o);
	return r != NULL;
}

byte Mode_PutFromMod(const char *module, const char *var, const char *src, const char *name) {
	PyObject *o = Script_Attr(src, name);
	/* Script_SetGlobal takes the reference over */
	return o && Script_SetGlobal(module, var, o);
}

byte Mode_VarCall2(const char *module, const char *var, byte global, const char *method, const nativeVal_t *a,
	const nativeVal_t *b) {
	nativeVal_t ab[2], r;
	ab[0] = *a;
	ab[1] = *b;
	byte ok = Mode_VarMethod(module, var, global, method, ab, 2, &r);
	Native_Free(&r);
	return ok;
}
