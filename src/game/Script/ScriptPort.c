/*
 * Direct calls between the script ports (include/scrap/scriptnative.h, stage 3 of docs/objlib-plan.md): the seam
 * that tells whether a module attribute is still the port's own C function (OBJLIB 5 (9c)), the call of its native
 * entry with the arguments packed as the adapter would get them, and the old call through Python when something else
 * is there (OBJLIB 7, with SCRAP_PORT_SLOW=1 forcing it and the counters of each call site for the test channel).
 * At the end the registry of stage 4: the refs by module and name (Port_Find on g_PortTable), the callbacks bound
 * to port functions (Port_CallbackOf, cPy_Function PYF_ENTRY), the functions given to the scheduler (Port_ScheduledOf,
 * cScheduledFunc), the call of such a bound site (Port_BoundCall), and the calls of values that hold port functions
 * (Port_Own by the adapter, Port_Value and Port_Values). Last the seams of the engine functions as C hooks
 * (Native_Seam, stage 7d).
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

static portSite_t *port_sites;

/* OBJLIB 7: SCRAP_PORT_SLOW=1 sends every call the slow way (the same binary compared with itself) */
byte Port_Slow(void) {
	static int slow = -1;
	if (slow < 0) {
		const char *e = getenv("SCRAP_PORT_SLOW");
		slow = e && !strcmp(e, "1");
	}
	return (byte)slow;
}

byte Port_IsOwn(const portRef_t *ref, PyObject *f) {
	const portRef_t *face;
	if (Port_Slow() || !PyCFunction_Check(f)) {
		return 0;
	}
	/* the C function object of a port function (OBJLIB 7e4b: its face) */
	return (face = NObj_PortFace(f)) != NULL && face->entry == ref->entry;
}

/* Port_Seam, slow 1: what is there as the hook also when it is the port's own (a call the entry cannot take). The
 * variable of a pack module is read natively first: a native function of the adapter is the port's own without a
 * Python object; anything else is read again as the Python object it gives (the hook) */
static int Port_Look(const portRef_t *ref, byte global, byte slow, PyObject **hook) {
	if (!slow && ModVar_HasModule(ref->module)) {
		nativeVal_t v;
		if (!(global ? ModVar_GlobalVal(ref->module, ref->name, &v) : ModVar_ModAttrVal(ref->module, ref->name, &v))) {
			return -1;
		}
		const portRef_t *r = NObj_PortRef(&v);
		Native_Free(&v);
		if (r && r->entry == ref->entry) {
			return 0;
		}
	}
	PyObject *f = global ? Script_Global(ref->module, ref->name) : Script_Attr(ref->module, ref->name);
	if (!f) {
		return -1;
	}
	if (!slow && Port_IsOwn(ref, f)) {
		Py_DECREF(f);
		return 0;
	}
	*hook = f;
	return 1;
}

int Port_Seam(const portRef_t *ref, byte global, PyObject **hook) {
	return Port_Look(ref, global, 0, hook);
}

byte Port_Arity(int n, int nargs) {
	return Native_Raise(NE_TYPE, "function requires exactly %d argument%s; %d given", nargs, nargs == 1 ? "" : "s", n);
}

portSite_t *Port_Sites(void) {
	return port_sites;
}

/* an argument that holds an NV_OBJ, an entity handle or a native object */
static byte Port_HoldsObj(const nativeVal_t *v) {
	if (NV_REFS >> v->type & 1) {
		return 1;
	}
	if (v->type == NV_LIST || v->type == NV_TUPLE) {
		for (int i = 0; i < v->n; i++) {
			if (Port_HoldsObj(&v->items[i])) {
				return 1;
			}
		}
	} else if (v->type == NV_DICT) {
		/* key and value pairs; a free or deleted slot has a key type beyond NV_REF */
		for (int i = 0; i < v->max; i++) {
			if (v->items[2 * i].type <= NV_REF && (Port_HoldsObj(&v->items[2 * i]) || Port_HoldsObj(&v->items[2 * i + 1]))) {
				return 1;
			}
		}
	}
	return 0;
}

/* the longest keyword name of a call */
#define PORT_NAME 0x40

/* the arguments of a call: the positional ones (npos), then the keywords, their names (lens[i] bytes) in fmt */
typedef struct {
	nativeArg_t a[PORT_ARGS];
	const char *names[PORT_ARGS];
	int lens[PORT_ARGS];
	int npos, n;
} portArgs_t;

/* fmt is its letters, then after a ';' the keywords "name=x" separated by ',': the count of the letters, *kw the ';'
 * or NULL */
static int Port_Npos(const char *fmt, const char **kw) {
	*kw = strchr(fmt, ';');
	int n = *kw ? (int)(*kw - fmt) : (int)strlen(fmt);
	CHECK(n <= PORT_ARGS);
	return n;
}

/* the keyword at q (its ';' or ','), argument n of the call: its name (*len bytes) and letter; the next one or NULL */
static const char *Port_Kw(const char *q, int n, const char **name, int *len, char *letter) {
	const char *eq = strchr(q + 1, '=');
	*name = q + 1;
	CHECK(eq && eq > *name && eq - *name < PORT_NAME && eq[1] && (!eq[2] || eq[2] == ',') && n < PORT_ARGS);
	*len = (int)(eq - *name);
	*letter = eq[1];
	return eq[2] ? eq + 2 : NULL;
}

/* the arguments of fmt */
static void Port_Read(const char *fmt, va_list va, portArgs_t *p) {
	char letters[PORT_ARGS + 1];
	const char *q;
	int n = Port_Npos(fmt, &q);
	memcpy(letters, fmt, n);
	p->npos = n;
	for (; q; n++) {
		q = Port_Kw(q, n, &p->names[n], &p->lens[n], &letters[n]);
	}
	letters[n] = 0;
	p->n = Native_ReadArgs(letters, va, p->a);
}

/* the slot of the entry for each argument of a call (slot[i]) and their count (m) */
typedef struct {
	signed char slot[PORT_ARGS], m;
} portSlots_t;

/* the slots of the arguments of fmt: 1, or 0 when the entry cannot take them that way (keywords to a ref without
 * names, a name it does not have, a slot given twice). Not inlined: its parsing stays off the frame of Port_Run (see
 * below) */
static __attribute__((noinline)) byte Port_Slots(const portRef_t *ref, const char *fmt, portSlots_t *ps) {
	byte used[PORT_ARGS] = { 0 };
	const char *q;
	int n = Port_Npos(fmt, &q), most = n;
	for (int i = 0; i < n; i++) {
		used[i] = 1;
		ps->slot[i] = (signed char)i;
	}
	for (; q; n++) {
		const char *name;
		int len, k = 0;
		char letter;
		q = Port_Kw(q, n, &name, &len, &letter);
		while (ref->kw && ref->kw[k] && (strncmp(ref->kw[k], name, len) || ref->kw[k][len])) {
			k++;
		}
		if (!ref->kw || !ref->kw[k] || k >= PORT_ARGS || used[k]) {
			return 0;
		}
		used[k] = 1;
		ps->slot[n] = (signed char)k;
		most = k + 1 > most ? k + 1 : most;
	}
	ps->m = (signed char)most;
	return 1;
}

/* one argument as the adapter's Mode_Args would get it from Native_Args */
static byte Port_Pack(const nativeArg_t *x, nativeVal_t *v) {
	switch (x->type) {
	case 'v':
		return Native_Pass(x->v, v);
	case 'r':
		/* the object itself (P for a native value) */
		if (Native_IsObj(x->v) || x->v->type == NV_ENT) {
			Native_Copy(v, x->v);
			return 1;
		}
		return Native_Pass(x->v, v);
	case 'i':
		Native_SetInt(v, x->i);
		return 1;
	case 'd':
		Native_SetFloat(v, x->d);
		return 1;
	case 's':
		if (x->s) {
			Native_SetText(v, x->s, (int)strlen(x->s));
		} else {
			Native_SetNone(v);
		}
		return 1;
	case 'O':
		/* a NULL object fails the call as Native_Args does */
		return x->o && Native_Arg(x->o, v);
	case 'P':
		/* the object itself */
		if (!x->o) {
			return 0;
		}
		Native_SetObj(v, x->o);
		return 1;
	}
	return 0;
}

byte Port_NativeCallVa(const nativeVal_t *f, nativeVal_t *out, const char *fmt, va_list va) {
	nativeArg_t x[NATIVE_ARGS];
	nativeVal_t a[NATIVE_ARGS], r;
	int n = Native_ReadArgs(fmt, va, x), k = 0;
	byte ok = 1;
	/* each argument as the call through Python handed it to the native callee */
	for (; ok && k < n; k++) {
		if (x[k].type == 'v') {
			NObj_CallArg(x[k].v, &a[k]);
		} else if (x[k].type == 'O' || x[k].type == 'P') {
			Native_SetNone(&a[k]);
			if (x[k].o) {
				NObj_FromPy(x[k].o, &a[k]);
			} else {
				ok = 0;
			}
		} else {
			ok = Port_Pack(&x[k], &a[k]);
			if (!ok) {
				Native_SetNone(&a[k]);
			}
		}
	}
	Native_SetNone(&r);
	ok = ok && NObj_Call(f, a, n, &r);
	while (k-- > 0) {
		Native_Free(&a[k]);
	}
	if (ok && out) {
		/* as Native_Arg makes the result of a call through Python */
		NObj_ArgOf(&r, out);
	}
	Native_Free(&r);
	return ok;
}

/* the entry of ref (Port_Entry) on the n arguments in their slots (the ones between not given): the letters x, or the
 * native values v (x NULL) as Native_Pass makes them; the result into *r (freed by the caller also on a failure).
 * Inlined: the packed values share the frame of Port_FastVa as before stage 4 (3296 bytes with them, gcc -O2 x86_64
 * Linux; mingw-w64 3200), not a frame of their own below it */
static inline __attribute__((always_inline)) byte Port_Fast(const portRef_t *ref, const nativeArg_t *x,
	const nativeVal_t *v, int n, const signed char *slot, int m, nativeVal_t *r) {
	nativeVal_t a[PORT_ARGS];
	byte ok = 1;
	for (int i = 0; i < m; i++) {
		Mode_KwSkip(&a[i]);
	}
	for (int i = 0; ok && i < n; i++) {
		ok = x ? Port_Pack(&x[i], &a[slot[i]]) : Native_Pass(&v[i], &a[slot[i]]);
	}
	if (ok) {
		ok = Port_Entry(ref, a, m, r);
	} else {
		Native_SetNone(r);
	}
	for (int i = 0; i < m; i++) {
		Native_Free(&a[i]);
	}
	return ok;
}

/*
 * The helpers below that read the arguments are not inlined: the arguments and the packed values (about 3.5 KB) stay
 * off the frame of Port_Run, which stays on the stack during a slow call. A script recursion through a port call
 * (an alias of a level function to itself) has one Port_Run per level: 3952 bytes with them, 144 without (gcc -O2
 * x86_64 Linux; mingw-w64 3856 and 192). An endless self call stops at 687 levels instead of 516 (690 by name
 * before stage 3).
 */

/* the fast path of Port_Run on the arguments of fmt in their slots */
static __attribute__((noinline)) byte Port_FastVa(const portRef_t *ref, const portSlots_t *ps, nativeVal_t *out,
	PyObject **pyout, const char *fmt, va_list va) {
	portArgs_t p;
	nativeVal_t r, res;
	Port_Read(fmt, va, &p);
	byte ok = Port_Fast(ref, p.a, NULL, p.n, ps->slot, ps->m, &r);
	if (pyout) {
		/* what the adapter returns (Mode_Out) */
		*pyout = !ok ? NULL : r.type == NV_NONE ? Py_ReturnNone() : Native_ToPy(&r);
		ok = *pyout != NULL;
	} else if (ok && (ok = Native_Pass(&r, &res)) != 0) {
		/* the result goes through the conversions also when it is dropped (they can fail) */
		if (out) {
			*out = res;
		} else {
			Native_Free(&res);
		}
	}
	Native_Free(&r);
	return ok;
}

/* the arguments of the call through Python: the positional ones as Native_Call passes them (*t), the keywords as a
 * dict in their order (*kw, NULL without keywords); 0 with the error (*t and *kw to be released) */
static __attribute__((noinline)) byte Port_SlowArgs(const char *fmt, va_list va, PyObject **t, PyObject **kw) {
	portArgs_t p;
	char name[PORT_NAME];
	Port_Read(fmt, va, &p);
	*kw = NULL;
	*t = Native_PyArgs(p.a, p.npos);
	byte ok = *t != NULL;
	if (ok && p.n > p.npos) {
		ok = (*kw = PyDict_New()) != NULL;
		for (int i = p.npos; ok && i < p.n; i++) {
			PyObject *v = Native_ArgPy(&p.a[i]);
			memcpy(name, p.names[i], p.lens[i]);
			name[p.lens[i]] = 0;
			ok = v && PyDict_SetItemString(*kw, name, v) == 0;
			Py_XDECREF(v);
		}
	}
	return ok;
}

/* the call through Python (the hook taken over) */
static PyObject *Port_SlowCall(PyObject *hook, const char *fmt, va_list va) {
	PyObject *t, *kw;
	PyObject *r = Port_SlowArgs(fmt, va, &t, &kw) ? Script_CallObject(hook, t, kw) : NULL;
	Py_XDECREF(kw);
	Py_XDECREF(t);
	Py_DECREF(hook);
	return r;
}

/* an argument of fmt holds an object (O, P, or a value with an NV_OBJ), read from a copy of va */
static __attribute__((noinline)) byte Port_ObjArgs(const char *fmt, va_list va) {
	portArgs_t p;
	va_list c;
	byte obj = 0;
	va_copy(c, va);
	Port_Read(fmt, c, &p);
	va_end(c);
	for (int i = 0; !obj && i < p.n; i++) {
		obj = p.a[i].type == 'O' || p.a[i].type == 'P' || ((p.a[i].type == 'v' || p.a[i].type == 'r') && Port_HoldsObj(p.a[i].v));
	}
	return obj;
}

/* the way of a native twin of a library function (a ref with twin set, OBJLIB 9): its entry (0) after the import
 * of its module, as the lookup by name imported it, or the function by name (1, with *hook) for an argument that
 * holds an object, keywords and under SCRAP_PORT_SLOW=1; -1 with the error of the import or the lookup */
static int Port_Twin(const portRef_t *ref, const char *fmt, va_list va, byte direct, PyObject **hook) {
	if (!direct || strchr(fmt, ';') || Port_Slow() || Port_ObjArgs(fmt, va)) {
		return Port_Look(ref, 0, 1, hook);
	}
	nativeVal_t m;
	byte ok = Script_Module(ref->module, &m);
	Native_Free(&m);
	return ok ? 0 : -1;
}

/* a call of a site: the result into *out as Native_Arg takes it (pyout NULL), or as the Python object into *pyout;
 * va is read once, by the path the call takes */
static byte Port_Run(portSite_t *site, nativeVal_t *out, PyObject **pyout, const char *fmt, va_list va) {
	PyObject *hook = NULL;
	portSlots_t ps;
	byte ok = 0;
	if (!site->listed) {
		site->listed = 1;
		site->next = port_sites;
		port_sites = site;
	}
	byte direct = Port_Slots(site->ref, fmt, &ps);
	int seam = !site->ref->twin ? Port_Look(site->ref, site->global, !direct, &hook) : Port_Twin(site->ref, fmt, va, direct, &hook);
	if (seam > 0) {
		site->slow++;
		PyObject *r = Port_SlowCall(hook, fmt, va);
		ok = r != NULL;
		if (pyout) {
			*pyout = r;
		} else {
			/* any other object is an NV_OBJ (OBJLIB 7) */
			if (r && out) {
				Native_Arg(r, out);
			}
			Py_XDECREF(r);
		}
	} else if (seam == 0) {
		site->fast++;
		ok = Port_FastVa(site->ref, &ps, out, pyout, fmt, va);
	}
	if (!ok) {
		site->errors++;
	}
	return ok;
}

byte Port_Invoke(portSite_t *site, nativeVal_t *out, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	byte ok = Port_Run(site, out, NULL, fmt, va);
	va_end(va);
	return ok;
}

PyObject *Port_AskVa(portSite_t *site, const char *fmt, va_list va) {
	PyObject *r = NULL;
	Port_Run(site, NULL, &r, fmt, va);
	return r;
}

PyObject *Port_Ask(portSite_t *site, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *r = Port_AskVa(site, fmt, va);
	va_end(va);
	return r;
}

/* ---- the registry (stage 4 of docs/objlib-plan.md) ---- */

/* the order of g_PortTable: module, then name */
static int Port_Order(const portRef_t *ref, const char *module, const char *name) {
	int c = strcmp(ref->module, module);
	return c ? c : strcmp(ref->name, name);
}

/* the index of the first ref of module.name in g_PortTable, PORT_COUNT when there is none */
static int Port_Index(const char *module, const char *name) {
	int lo = 0, hi = PORT_COUNT;
	while (lo < hi) {
		int mid = lo + (hi - lo) / 2;
		if (Port_Order(g_PortTable[mid], module, name) < 0) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo < PORT_COUNT && !Port_Order(g_PortTable[lo], module, name) ? lo : PORT_COUNT;
}

const portRef_t *Port_Find(const char *module, const char *name) {
	int i = Port_Index(module, name);
	return i < PORT_COUNT ? g_PortTable[i] : NULL;
}

/* OBJLIB 7: the counters of the port functions bound for a later call, one site per ref of the table and kind */
enum { PORT_CALLBACK, PORT_SCHEDULED, PORT_BOUNDKINDS };
static portSite_t port_bound[PORT_BOUNDKINDS][PORT_COUNT];
static portSite_t *port_boundList[PORT_BOUNDKINDS];

/* the site of ref i of the table for kind, listed at its first use */
static portSite_t *Port_Bound(int kind, int i) {
	static const char *const files[PORT_BOUNDKINDS] = { "callback", "scheduled" };
	portSite_t *s = &port_bound[kind][i];
	if (!s->listed) {
		s->ref = g_PortTable[i];
		s->file = files[kind];
		s->listed = 1;
		s->next = port_boundList[kind];
		port_boundList[kind] = s;
	}
	return s;
}

static int Port_KeyIndex(uintptr_t key, const char *name);

/* the index in g_PortTable of the port function the value f may be: a native port function or the C function object
 * of one (OBJLIB 7e4b: its face), by its entry and its own name (__name__: an alias is the ref of its name); -1 for any
 * other value and under SCRAP_PORT_SLOW=1 */
static int Port_FuncIndex(const nativeVal_t *f) {
	const portRef_t *ref = NObj_PortRef(f);
	PyObject *py = ref ? NULL : NObj_Py(f);
	if (Port_Slow()) {
		return -1;
	}
	if (!ref && py) {
		ref = NObj_PortFace(py);
	}
	return !ref || ref->twin ? -1 : Port_KeyIndex((uintptr_t)ref->entry, ref->name);
}

portSite_t *Port_CallbackOf(const char *module, const char *name, const nativeVal_t *f) {
	int i = Port_FuncIndex(f);
	if (i < 0) {
		return NULL;
	}
	/* the ref of module.name when f is its function under its own name, else the ref of the function itself (a
	 * variable that holds a port function of another module or name: Net.ClientCreate = Ffa.ClientCreate, an alias
	 * Alarm.ActivateAlarm = Alarm.Init), as the Python call would run its entry */
	for (int k = Port_Index(module, name); k < PORT_COUNT && !Port_Order(g_PortTable[k], module, name); k++) {
		if (g_PortTable[k]->entry == g_PortTable[i]->entry && !strcmp(g_PortTable[i]->name, g_PortTable[k]->name)) {
			return Port_Bound(PORT_CALLBACK, k);
		}
	}
	return Port_Bound(PORT_CALLBACK, i);
}

portSite_t *Port_Callbacks(void) {
	return port_boundList[PORT_CALLBACK];
}

portSite_t *Port_ScheduledOf(const nativeVal_t *f) {
	int i = Port_FuncIndex(f);
	return i < 0 ? NULL : Port_Bound(PORT_SCHEDULED, i);
}

portSite_t *Port_ScheduledSites(void) {
	return port_boundList[PORT_SCHEDULED];
}

byte Port_BoundCall(portSite_t *site, const nativeArg_t *x, const nativeVal_t *v, int n, nativeVal_t *res,
	PyObject **pyout) {
	static_assert(PORT_ARGS == 11, "one slot per argument");
	static const signed char slot[PORT_ARGS] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	nativeVal_t r, drop, *out = res ? res : &drop;
	CHECK(n >= 0 && n <= PORT_ARGS);
	site->fast++;
	byte ok = Port_Fast(site->ref, x, v, n, slot, n, &r);
	if (pyout) {
		/* what the adapter returns (Mode_Out) */
		*pyout = !ok ? NULL : r.type == NV_NONE ? Py_ReturnNone() : Native_ToPy(&r);
		ok = *pyout != NULL;
	} else if (ok) {
		/* the result goes through the conversions of the adapter also when it is dropped (they can fail) */
		ok = Native_Pass(&r, out);
	} else {
		Native_SetNone(out);
	}
	Native_Free(&r);
	if (!res && !pyout) {
		Native_Free(&drop);
	}
	if (!ok) {
		site->errors++;
	}
	return ok;
}

byte Port_Entry(const portRef_t *ref, const nativeVal_t *a, int n, nativeVal_t *r) {
	byte ok = 0;
	Native_SetNone(r);
	/* the frame address, as Script_CallObject takes it */
	if (Script_Enter((uintptr_t)__builtin_frame_address(0))) {
		ok = ref->entry(a, n, r);
		Script_Leave();
		if (!ok && !Native_Failed()) {
			Native_Raise(NE_SYSTEM, "NULL result without error in call_object");
		}
	}
	return ok;
}

/* the name of keyword pair p (an NV_STR; any other key matches no name) */
static const char *Port_KwName(const nativeVal_t *p) {
	return p->items[0].type == NV_STR ? Native_Text(&p->items[0]) : NULL;
}

/* the keyword pair of name in kw, -1 when there is none */
static int Port_KwFind(const nativeVal_t *kw, const char *name) {
	for (int i = 0; i < kw->n; i++) {
		const char *k = Port_KwName(&kw->items[i]);
		if (k && !strcmp(k, name)) {
			return i;
		}
	}
	return -1;
}

byte Port_KwCall(const portRef_t *ref, const nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out) {
	nativeVal_t v[PORT_ARGS], r;
	signed char slot[PORT_ARGS];
	int nk = kw ? kw->n : 0, names = 0, m = n, given = n;
	/* "&": an entry that takes the caller's values as they are (MODE_REFKW: its adapter took the objects by reference) */
	byte byRef = ref->kwfmt && ref->kwfmt[0] == '&';
	const char *kwfmt = ref->kwfmt ? ref->kwfmt + byRef : NULL;
	Native_SetNone(out);
	if (nk && !ref->kw) {
		return Native_Raise(NE_TYPE, "this function takes no keyword arguments");
	}
	if (n > PORT_ARGS) {
		/* only the arity error of the entry can come (SAI.EnableAIVehicle reads up to 27): the values as they are */
		byte ok = Port_Entry(ref, a, n, &r) && Native_Pass(&r, out);
		Native_Free(&r);
		return ok;
	}
	while (ref->kw && ref->kw[names]) {
		names++;
	}
	CHECK(names <= PORT_ARGS);
	/* vgetargskeywords of 1.5.2: a slot given by position and by name, the count with the required ones given by name,
	 * then a name the function does not have (the first in the order of the keyword dict) */
	for (int i = 0; nk && i < n && i < names; i++) {
		if (Port_KwFind(kw, ref->kw[i]) >= 0) {
			return Native_Raise(NE_TYPE, "keyword parameter %s redefined", ref->kw[i]);
		}
	}
	for (int i = 0; i < n; i++) {
		v[i] = a[i];
		slot[i] = (signed char)i;
	}
	for (int k = n; k < names; k++) {
		int i = nk ? Port_KwFind(kw, ref->kw[k]) : -1;
		if (i >= 0) {
			v[given] = kw->items[i].items[1];
			slot[given++] = (signed char)k;
			m = k + 1;
		}
	}
	if (given - n < nk) {
		nativeVal_t d;
		const nativeVal_t *key, *x;
		int pos = 0, min, max, len = n;
		Mode_FmtCount(kwfmt, &min, &max);
		for (int k = n; k < min; k++) {
			len += Port_KwFind(kw, ref->kw[k]) >= 0;
		}
		if (len < min || len > max) {
			return Mode_Arity(len, min, max);
		}
		/* the conversions of the slots given, in slot order */
		nativeVal_t slots[PORT_ARGS];
		for (int k = 0; k < m; k++) {
			Mode_KwSkip(&slots[k]);
		}
		for (int i = 0; i < given; i++) {
			slots[(int)slot[i]] = v[i];
		}
		if (!Mode_ParseSlots(slots, m, kwfmt)) {
			return 0;
		}
		Native_SetEmpty(&d, NV_DICT);
		for (int i = 0; i < nk; i++) {
			nativeVal_t k2, x2;
			Native_Copy(&k2, &kw->items[i].items[0]);
			Native_SetNone(&x2);
			Native_DictSet(&d, &k2, &x2);
		}
		const char *bad = NULL;
		while (!bad && Native_DictNext(&d, &pos, &key, &x)) {
			int k = 0;
			while (k < names && (key->type != NV_STR || strcmp(Native_Text(key), ref->kw[k]))) {
				k++;
			}
			if (k == names) {
				bad = key->type == NV_STR ? Native_Text(key) : "?";
			}
		}
		byte ok = bad ? Native_Raise(NE_TYPE, "%s is an invalid keyword argument for this function", bad) : 0;
		Native_Free(&d);
		return ok;
	}
	byte ok;
	if (byRef) {
		nativeVal_t slots[PORT_ARGS];
		for (int k = 0; k < m; k++) {
			Mode_KwSkip(&slots[k]);
		}
		for (int i = 0; i < given; i++) {
			Native_Copy(&slots[(int)slot[i]], &v[i]);
		}
		ok = Port_Entry(ref, slots, m, &r);
		for (int k = 0; k < m; k++) {
			Native_Free(&slots[k]);
		}
	} else {
		ok = Port_Fast(ref, NULL, v, given, slot, m, &r);
	}
	ok = ok && Native_Pass(&r, out);
	Native_Free(&r);
	return ok;
}

/* ---- values that hold port functions (stage 4) ---- */

const portRef_t port_value = { .module = "value", .name = "?" };

/* the indices of the refs (not the twins) in the order of their entries (the address), then of g_PortTable; sorted at
 * the first use */
static int port_byKey[PORT_COUNT];
static int port_keys = -1;

static uintptr_t Port_Key(int i) {
	return (uintptr_t)g_PortTable[i]->entry;
}

static int Port_KeyOrder(const void *x, const void *y) {
	int a = *(const int *)x, b = *(const int *)y;
	uintptr_t p = Port_Key(a), q = Port_Key(b);
	return p < q ? -1 : p > q ? 1 : a - b;
}

/* the index in g_PortTable of the ref of the entry key under the name (one entry under several names: an alias in the
 * table), -1 when there is none */
static int Port_KeyIndex(uintptr_t key, const char *name) {
	int *by = port_byKey;
	if (port_keys < 0) {
		int n = 0;
		for (int i = 0; i < PORT_COUNT; i++) {
			if (!g_PortTable[i]->twin && Port_Key(i)) {
				by[n++] = i;
			}
		}
		qsort(by, n, sizeof(by[0]), Port_KeyOrder);
		port_keys = n;
	}
	int lo = 0, hi = port_keys;
	while (lo < hi) {
		int mid = lo + (hi - lo) / 2;
		if (Port_Key(by[mid]) < key) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	for (; lo < port_keys && Port_Key(by[lo]) == key; lo++) {
		if (!strcmp(g_PortTable[by[lo]]->name, name)) {
			return by[lo];
		}
	}
	return -1;
}

/* the index in g_PortTable of the ref whose own function f is (Port_Own), -1 when there is none */
static int Port_OwnIndex(PyObject *f) {
	const portRef_t *face = f ? NObj_PortFace(f) : NULL;
	if (Port_Slow() || !f || !PyCFunction_Check(f)) {
		return -1;
	}
	return face ? Port_KeyIndex((uintptr_t)face->entry, face->name) : -1;
}

int Port_FaceIndex(const portRef_t *ref) {
	int i = Port_KeyIndex((uintptr_t)ref->entry, ref->name);
	if (i < 0) {
		for (i = 0; i < PORT_COUNT && g_PortTable[i] != ref; i++) {
		}
		return i < PORT_COUNT ? i : -1;
	}
	/* the first ref of the entry: the run of the entry in port_byKey is in table order */
	int lo = 0;
	while (port_byKey[lo] != i) {
		lo++;
	}
	while (lo > 0 && Port_Key(port_byKey[lo - 1]) == (uintptr_t)ref->entry) {
		lo--;
	}
	return port_byKey[lo];
}

const portRef_t *Port_OfFn(const char *module, const portFn_t *row) {
	for (int i = Port_Index(module, row->name); i < PORT_COUNT && !Port_Order(g_PortTable[i], module, row->name); i++) {
		if (g_PortTable[i]->entry == row->entry) {
			return g_PortTable[i];
		}
	}
	return NULL;
}

const portRef_t *Port_Own(PyObject *f) {
	int i = Port_OwnIndex(f);
	return i >= 0 ? g_PortTable[i] : NULL;
}

/* the tuple of the n values (NULL with the error of a conversion) */
static PyObject *Port_PyValues(const nativeVal_t *a, int n) {
	PyObject *t = PyTuple_New(n);
	for (int i = 0; t && i < n; i++) {
		PyObject *v = Native_ToPy(&a[i]);
		if (!v) {
			Py_DECREF(t);
			return NULL;
		}
		PyTuple_SET_ITEM(t, i, v);
	}
	return t;
}

/* the call of the value f on n arguments, as letters (x) or as native values (a): the entry of the port function f is
 * (Port_OwnIndex, through Port_BoundCall as a bound site), else the call through Python of Native_Call; the result
 * into *out as Native_Arg takes it, or as the Python object into *pyout. The packed values are in the frame of
 * Port_BoundCall only: a call through Python keeps a small frame on the stack */
static byte Port_ValueRun(portSite_t *site, PyObject *f, const nativeArg_t *x, const nativeVal_t *a, int n,
	nativeVal_t *out, PyObject **pyout) {
	if (!f) {
		return 0;
	}
	if (!site->listed) {
		site->listed = 1;
		site->next = port_sites;
		port_sites = site;
	}
	int i = n <= PORT_ARGS ? Port_OwnIndex(f) : -1;
	if (i >= 0) {
		/* the site names the port function it called last; Port_BoundCall counts the call and a failure */
		site->ref = g_PortTable[i];
		return Port_BoundCall(site, x, a, n, out, pyout);
	}
	site->slow++;
	PyObject *r = Script_Invoke(f, x ? Native_PyArgs(x, n) : Port_PyValues(a, n));
	byte ok = r != NULL;
	if (pyout) {
		*pyout = r;
	} else {
		/* any other object is an NV_OBJ (OBJLIB 7) */
		if (r && out) {
			Native_Arg(r, out);
		}
		Py_XDECREF(r);
	}
	if (!ok) {
		site->errors++;
	}
	return ok;
}

byte Port_ValueVa(portSite_t *site, PyObject *f, nativeVal_t *out, const char *fmt, va_list va) {
	nativeArg_t x[NATIVE_ARGS];
	int n = Native_ReadArgs(fmt, va, x);
	return Port_ValueRun(site, f, x, NULL, n, out, NULL);
}

byte Port_Value(portSite_t *site, PyObject *f, nativeVal_t *out, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	byte ok = Port_ValueVa(site, f, out, fmt, va);
	va_end(va);
	return ok;
}

byte Port_Values(portSite_t *site, PyObject *f, const nativeVal_t *a, int n, nativeVal_t *out, PyObject **pyout) {
	return Port_ValueRun(site, f, NULL, a, n, out, pyout);
}

/* NObj_Call of f on the arguments as the call through Python handed them to a native callee (NObj_CallArg), the
 * result as Native_Arg takes it into *out (NULL: dropped) or as the Python object into *pyout */
static byte Port_ObjCall(const nativeVal_t *f, const nativeVal_t *a, int n, nativeVal_t *out, PyObject **pyout) {
	nativeVal_t r, *c = Mem_Alloc((uint32_t)((size_t)(n + 1) * sizeof(*c)));
	if (out) {
		Native_SetNone(out);
	}
	for (int i = 0; i < n; i++) {
		NObj_CallArg(&a[i], &c[i]);
	}
	byte ok = NObj_Call(f, c, n, &r);
	for (int i = 0; i < n; i++) {
		Native_Free(&c[i]);
	}
	Mem_Free(c);
	if (pyout) {
		*pyout = ok ? Native_ToPy(&r) : NULL;
		ok = *pyout != NULL;
	} else if (out && ok) {
		NObj_ArgOf(&r, out);
	}
	Native_Free(&r);
	return ok;
}

byte Port_ValuesOf(portSite_t *site, const nativeVal_t *f, const nativeVal_t *a, int n, nativeVal_t *out,
	PyObject **pyout) {
	const portRef_t *ref = NObj_PortRef(f);
	if (ref && n <= PORT_ARGS && !Port_Slow()) {
		if (!site->listed) {
			site->listed = 1;
			site->next = port_sites;
			port_sites = site;
		}
		site->ref = ref;
		return Port_BoundCall(site, NULL, a, n, out, pyout);
	}
	PyObject *py = NObj_Py(f);
	if (py && !ref && !NObj_Native(f)) {
		return Port_Values(site, py, a, n, out, pyout);
	}
	if (f->type == NV_REF && NObj_Native(f) && !ref) {
		return Port_ObjCall(f, a, n, out, pyout);
	}
	/* a value by value (not callable) or a port function the entry cannot take: through Python */
	py = Native_ToPy(f);
	byte ok = Port_Values(site, py, a, n, out, pyout);
	Py_XDECREF(py);
	return ok;
}

byte Port_CallKept(portSite_t *bound, const nativeVal_t *f, const nativeVal_t *a, int n, nativeVal_t *out) {
	if (bound) {
		return Port_BoundCall(bound, NULL, a, n, out, NULL);
	}
	return Port_ObjCall(f, a, n, out, NULL);
}

/* ---- the seams of the engine functions as C hooks (stage 7d) ---- */

enum { HOOK_OWN, HOOK_SET, HOOK_DELETED };

/* the seam entry of module.name (a list: the seams a run uses, a few dozen) */
typedef struct portHook_s {
	char *module, *name;
	byte state;			/* HOOK_ */
	nativeHook_t hook;		/* HOOK_SET: what is there */
	struct portHook_s *next;
} portHook_t;

static portHook_t *port_hooks;
static byte (*port_hookFind)(const nativeVal_t *f, nativeHook_t *hook);

void Native_HookFinder(byte (*find)(const nativeVal_t *f, nativeHook_t *hook)) {
	port_hookFind = find;
}

static portHook_t *Port_HookOf(const char *module, const char *name) {
	portHook_t *h = port_hooks;
	while (h && (strcmp(h->name, name) || strcmp(h->module, module))) {
		h = h->next;
	}
	return h;
}

/* f is the engine's own function of name: a port function of that name (the native function of the module's table, or
 * its C function object), or a C function object of that name without self (a function of the test channel has one) */
static byte Port_HookOwn(const char *name, const nativeVal_t *f) {
	const portRef_t *ref = NObj_PortRef(f);
	PyObject *py = ref ? NULL : NObj_Py(f);
	if (!ref && py) {
		ref = NObj_PortFace(py);
	}
	if (ref) {
		return !strcmp(ref->name, name);
	}
	return py && PyCFunction_Check(py) && !PyCFunction_GET_SELF(py) &&
		!strcmp(((PyCFunctionObject *)py)->m_ml->ml_name, name);
}

/* the entry follows what module.name holds now (f NULL: nothing) */
static void Port_HookSet(portHook_t *h, const nativeVal_t *f) {
	nativeHook_t old = h->hook;
	h->hook.call = NULL;
	h->hook.ctx = NULL;
	Native_SetNone(&h->hook.f);
	h->state = !f ? HOOK_DELETED : Port_HookOwn(h->name, f) ? HOOK_OWN : HOOK_SET;
	if (h->state == HOOK_SET) {
		Native_Copy(&h->hook.f, f);
		if (port_hookFind && !port_hookFind(f, &h->hook)) {
			h->hook.call = NULL;
			h->hook.ctx = NULL;
		}
	}
	/* dropped after: a hook running now holds its own copy (Native_Seam) */
	Native_HookFree(&old);
}

static char *Port_Dup(const char *s) {
	size_t len = strlen(s) + 1;
	return memcpy(Mem_Alloc((uint32_t)len), s, len);
}

int Native_Seam(const char *module, const char *name, nativeHook_t *hook) {
	portHook_t *h = Port_HookOf(module, name);
	if (!h) {
		/* the first question: what the module holds now (OBJLIB 7: its attribute); the stores after it come through
		 * Native_HookStore */
		PyObject *f = Script_Attr(module, name);
		if (!f && !Native_ErrIs(NE_ATTRIBUTE)) {
			return -1;
		}
		nativeVal_t v;
		h = Mem_Alloc(sizeof(*h));
		memset(h, 0, sizeof(*h));
		h->module = Port_Dup(module);
		h->name = Port_Dup(name);
		h->next = port_hooks;
		port_hooks = h;
		if (f) {
			Native_Arg(f, &v);
			Py_DECREF(f);
		}
		Port_HookSet(h, f ? &v : NULL);
		if (f) {
			Native_Free(&v);
		}
	}
	if (h->state == HOOK_OWN) {
		return 0;
	}
	if (h->state == HOOK_DELETED) {
		/* the AttributeError of the module's getattr */
		Native_Raise(NE_ATTRIBUTE, "%s", name);
		return -1;
	}
	*hook = h->hook;
	Native_Copy(&hook->f, &h->hook.f);
	return 1;
}

void Native_HookClear(void) {
	while (port_hooks) {
		portHook_t *h = port_hooks;
		port_hooks = h->next;
		Native_HookFree(&h->hook);
		Mem_Free(h->module);
		Mem_Free(h->name);
		Mem_Free(h);
	}
}

void Native_HookFree(nativeHook_t *hook) {
	Native_Free(&hook->f);
	hook->call = NULL;
	hook->ctx = NULL;
}

/* a channel function on the arguments of fmt as its adapter got them, with the depth check of the calls */
static byte Port_HookRun(const nativeHook_t *hook, nativeVal_t *out, const char *fmt, va_list va) {
	nativeArg_t x[NATIVE_ARGS];
	nativeVal_t a[NATIVE_ARGS], r;
	int n = Native_ReadArgs(fmt, va, x);
	byte ok = 1;
	for (int i = 0; i < n; i++) {
		Native_SetNone(&a[i]);
	}
	for (int i = 0; ok && i < n; i++) {
		ok = Port_Pack(&x[i], &a[i]);
	}
	Native_SetNone(&r);
	if (ok && Script_Enter((uintptr_t)__builtin_frame_address(0))) {
		ok = hook->call(hook->ctx, a, n, &r);
		Script_Leave();
		if (!ok && !Native_Failed()) {
			Native_NullResult();
		}
	} else {
		ok = 0;
	}
	/* the result in the form the script got it, also when it is dropped (the conversion can fail) */
	nativeVal_t res;
	if (ok && (ok = Native_Pass(&r, &res)) != 0) {
		if (out) {
			*out = res;
		} else {
			Native_Free(&res);
		}
	}
	Native_Free(&r);
	for (int i = 0; i < n; i++) {
		Native_Free(&a[i]);
	}
	return ok;
}

byte Native_HookCall(nativeHook_t *hook, nativeVal_t *out, const char *fmt, ...) {
	va_list va;
	byte ok;
	va_start(va, fmt);
	if (hook->call) {
		ok = Port_HookRun(hook, out, fmt, va);
	} else if (NObj_Native(&hook->f) && !NObj_PortRef(&hook->f)) {
		/* a native object (an instance with __call__, a class, a method) natively */
		ok = Port_NativeCallVa(&hook->f, out, fmt, va);
	} else {
		/* OBJLIB 7: any other callable through Python, as the script called it */
		ok = Native_CallVa(Native_ToPy(&hook->f), out, fmt, va);
	}
	va_end(va);
	Native_HookFree(hook);
	return ok;
}

void Native_HookStore(const nativeVal_t *o, const char *name, const nativeVal_t *v) {
	nativeVal_t mod;
	if (!port_hooks || NObj_Kind(o) != NO_MODULE) {
		return;
	}
	if (!NObj_GetAttr(o, "__name__", &mod)) {
		Native_ErrClear();
		return;
	}
	portHook_t *h = mod.type == NV_STR ? Port_HookOf(Native_Text(&mod), name) : NULL;
	Native_Free(&mod);
	if (h) {
		Port_HookSet(h, v);
	}
}
