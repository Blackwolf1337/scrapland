/*
 * Native C helpers of the game mode ports (include/scrap/scriptmodes.h): the engine calls of Net, Ffa, the online and
 * split screen modes and the super deals on native values, the seams of the engine functions their case files record
 * or swap (OBJLIB 7), the module state they read and write in other modules (OBJLIB 5) and their adapters.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/net.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/sound.h"
#include "scrap/timer.h"

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
	long i;
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
	long i;
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
	long i;
	const char *name;
	return Native_ArgInt(id, &i) && Native_ArgStr(item, 2, &name) && Native_ScorerSet((int)i, name, var, v);
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
	long i;
	if (!Native_ArgInt(id, &i)) {
		return 0;
	}
	Input_Bind((int)i, set, action, func);
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

byte Mode_Later(double delay, const char *module, const char *func, const nativeVal_t *args, int n, const char *name) {
	nativeVal_t t, nm;
	/* OBJLIB 4: the function the scheduler keeps is the global of the module (a mission may have replaced it) */
	PyObject *fn = Script_Global(module, func);
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

byte Mode_GVarSet(const char *name, const nativeVal_t *v) {
	MODE_SEAM("Scrap", "Set");
	if (seam) {
		return Native_HookCall(hook, NULL, "sv", name, v);
	}
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

byte Mode_Rand(double a, double b, nativeVal_t *out) {
	MODE_SEAM("Scrap", "Rand");
	if (seam) {
		return Native_HookCall(hook, out, "dd", a, b);
	}
	Native_SetFloat(out, Scrap_RandRange(a, b));
	return 1;
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

byte Mode_Sound(const char *sound, const char *volume) {
	nativeVal_t s, v;
	if (!Mode_ModVar("Sound", sound, &s)) {
		return 0;
	}
	if (!Mode_ModVar("Sound", volume, &v)) {
		Native_Free(&s);
		return 0;
	}
	PyObject *hook;
	int seam = Native_Seam("SSound", "Play", &hook);
	byte ok = 0;
	if (seam > 0) {
		ok = Native_HookCall(hook, NULL, "vv", &s, &v);
	} else if (seam == 0) {
		/* "s|dd" */
		const char *name;
		double vol;
		ok = Native_ArgStr(&s, 1, &name) && Native_ArgDouble(&v, &vol);
		if (ok) {
			SoundSys_Play(g_pSound, name, Snd_Volume((float)vol), 0);
		}
	}
	Native_Free(&s);
	Native_Free(&v);
	return ok;
}

/* ---- module state ---- */

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

/* the dict object of a module variable changed by f, then read back into *d */
static byte Mode_DictChange(const char *module, const char *var, nativeVal_t *d, const nativeVal_t *key,
	const nativeVal_t *v) {
	PyObject *o = Script_Global(module, var);
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
	return Mode_DictChange(module, var, d, key, v);
}

byte Mode_DictDel(const char *module, const char *var, nativeVal_t *d, const nativeVal_t *key) {
	return Mode_DictChange(module, var, d, key, NULL);
}

byte Mode_Put(const char *module, const char *var, nativeVal_t *slot, const nativeVal_t *v) {
	PyObject *o = Native_ToPy(v);
	if (!o || !Script_SetGlobal(module, var, o)) {
		return 0;
	}
	if (slot != v) {
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
		Native_GetItem(&modes, &type, &valid)) {
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

byte Mode_Arg(PyObject *o, const char *func, nativeVal_t *out) {
	return Native_FromPyAny(o, out) ||
		Native_Raise(NE_TYPE, "%s: an argument of type %.50s native C does not take", func, o->ob_type->tp_name);
}

PyObject *Mode_Result(byte ok) {
	return ok ? Py_ReturnNone() : NULL;
}

PyObject *Mode_Return(byte ok, nativeVal_t *v) {
	PyObject *r = ok ? Native_ToPy(v) : NULL;
	Native_Free(v);
	return r;
}
