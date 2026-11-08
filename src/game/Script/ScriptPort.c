/*
 * Direct calls between the script ports (include/scrap/scriptnative.h, stage 3 of docs/objlib-plan.md): the seam
 * that tells whether a module attribute is still the port's own C function (OBJLIB 5), the call of its native entry
 * with the arguments packed as the adapter would get them, and the old call through Python when something else is
 * there (OBJLIB 7, with SCRAP_PORT_SLOW=1 forcing it and the counters of each call site for the test channel).
 * At the end the registry of stage 4: the refs by module and name (Port_Find on g_PortTable), the callbacks bound
 * to port functions (Port_Callback, cPy_Function PYF_ENTRY), and the calls of values that hold port functions (Port_Own
 * by the adapter, Port_Value and Port_Values).
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

static portSite_t *port_sites;

/* OBJLIB 7: SCRAP_PORT_SLOW=1 sends every call the slow way (the same binary compared with itself) */
static byte Port_Slow(void) {
	static int slow = -1;
	if (slow < 0) {
		const char *e = getenv("SCRAP_PORT_SLOW");
		slow = e && !strcmp(e, "1");
	}
	return (byte)slow;
}

byte Port_IsOwn(const portRef_t *ref, PyObject *f) {
	return !Port_Slow() && PyCFunction_Check(f) && !PyCFunction_GET_SELF(f) &&
		((PyCFunctionObject *)f)->m_ml->ml_meth == ref->adapter;
}

/* Port_Seam, slow 1: what is there as the hook also when it is the port's own (a call the entry cannot take) */
static int Port_Look(const portRef_t *ref, byte global, byte slow, PyObject **hook) {
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

/* an argument that holds an NV_OBJ */
static byte Port_HoldsObj(const nativeVal_t *v) {
	if (v->type == NV_OBJ) {
		return 1;
	}
	if (v->type == NV_LIST || v->type == NV_TUPLE) {
		for (int i = 0; i < v->n; i++) {
			if (Port_HoldsObj(&v->items[i])) {
				return 1;
			}
		}
	} else if (v->type == NV_DICT) {
		/* key and value pairs; a free or deleted slot has a key type beyond NV_OBJ */
		for (int i = 0; i < v->max; i++) {
			if (v->items[2 * i].type <= NV_OBJ && (Port_HoldsObj(&v->items[2 * i]) || Port_HoldsObj(&v->items[2 * i + 1]))) {
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
		Native_SetNone(v);
		v->type = NV_OBJ;
		v->obj = x->o;
		Py_INCREF(x->o);
		return 1;
	}
	return 0;
}

/* the entry of ref on the arguments in their slots (the ones between not given), the result into *r (freed by the
 * caller also on a failure) */
static byte Port_Fast(const portRef_t *ref, const portArgs_t *p, const signed char *slot, int m, nativeVal_t *r) {
	nativeVal_t a[PORT_ARGS];
	byte ok = 1;
	for (int i = 0; i < m; i++) {
		Mode_KwSkip(&a[i]);
	}
	for (int i = 0; ok && i < p->n; i++) {
		ok = Port_Pack(&p->a[i], &a[slot[i]]);
	}
	Native_SetNone(r);
	/* the frame address, as Script_CallObject takes it */
	if (ok && Script_Enter((uintptr_t)__builtin_frame_address(0))) {
		ok = ref->entry(a, m, r);
		Script_Leave();
		if (!ok && !PyErr_Occurred()) {
			PyErr_SetString(PyExc_SystemError, "NULL result without error in call_object");
		}
	} else {
		ok = 0;
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
	byte ok = Port_Fast(ref, &p, ps->slot, ps->m, &r);
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

/* the arguments of the call through Python: the positional ones as Native_Call passes them (*t), the keywords as the
 * dict of Mode_CallKw, in their order (*kw, NULL without keywords); 0 with the error (*t and *kw to be released) */
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
		obj = p.a[i].type == 'O' || p.a[i].type == 'P' || (p.a[i].type == 'v' && Port_HoldsObj(p.a[i].v));
	}
	return obj;
}

/* the way of a native twin of a library function (a ref without an adapter, OBJLIB 9): its entry (0) after the import
 * of its module, as the lookup by name imported it, or the function by name (1, with *hook) for an argument that
 * holds an object, keywords and under SCRAP_PORT_SLOW=1; -1 with the error of the import or the lookup */
static int Port_Twin(const portRef_t *ref, const char *fmt, va_list va, byte direct, PyObject **hook) {
	if (!direct || strchr(fmt, ';') || Port_Slow() || Port_ObjArgs(fmt, va)) {
		return Port_Look(ref, 0, 1, hook);
	}
	PyObject *m = PyImport_ImportModule((char *)ref->module);
	Py_XDECREF(m);
	return m ? 0 : -1;
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
	byte direct = !site->ref->py && Port_Slots(site->ref, fmt, &ps);
	int seam = site->ref->adapter ? Port_Look(site->ref, site->global, !direct, &hook) : Port_Twin(site->ref, fmt, va, direct, &hook);
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

/* OBJLIB 7: the counters of the callbacks, one site per ref of the table, and the C function object a callback of
 * the ref was first bound to (a reference kept for the Python call of Port_CallbackFunc) */
static portSite_t port_callbacks[PORT_COUNT];
static portSite_t *port_callbackList;
static PyObject *port_callbackFuncs[PORT_COUNT];

/* the site of the callbacks of ref i, listed at its first binding (f the function bound) */
static portSite_t *Port_CallbackSite(int i, PyObject *f) {
	portSite_t *s = &port_callbacks[i];
	if (!s->listed) {
		s->ref = g_PortTable[i];
		s->file = "callback";
		s->listed = 1;
		s->next = port_callbackList;
		port_callbackList = s;
		Py_INCREF(f);
		port_callbackFuncs[i] = f;
	}
	return s;
}

static int Port_OwnIndex(PyObject *f);

portSite_t *Port_Callback(const char *module, const char *name, PyObject *f) {
	for (int i = Port_Index(module, name); i < PORT_COUNT && !Port_Order(g_PortTable[i], module, name); i++) {
		const portRef_t *ref = g_PortTable[i];
		/* a function of the same adapter under another name is found below by its own name: the name of the
		 * binding is the function's own (__name__) */
		if (ref->py || !Port_IsOwn(ref, f) || strcmp(((PyCFunctionObject *)f)->m_ml->ml_name, ref->name)) {
			continue;
		}
		return Port_CallbackSite(i, f);
	}
	/* a variable that holds a port function of another module or name (Net.ClientCreate = Ffa.ClientCreate, an alias
	 * Alarm.ActivateAlarm = Alarm.Init): the ref of the function itself, as the Python call would run its adapter */
	int i = Port_OwnIndex(f);
	return i >= 0 ? Port_CallbackSite(i, f) : NULL;
}

portSite_t *Port_Callbacks(void) {
	return port_callbackList;
}

PyObject *Port_CallbackFunc(const portSite_t *site) {
	return port_callbackFuncs[site - port_callbacks];
}

byte Port_Entry(const portRef_t *ref, const nativeVal_t *a, int n, nativeVal_t *r) {
	byte ok = 0;
	Native_SetNone(r);
	/* the frame address, as Script_CallObject takes it */
	if (Script_Enter((uintptr_t)__builtin_frame_address(0))) {
		ok = ref->entry(a, n, r);
		Script_Leave();
		if (!ok && !PyErr_Occurred()) {
			PyErr_SetString(PyExc_SystemError, "NULL result without error in call_object");
		}
	}
	return ok;
}

/* ---- values that hold port functions (stage 4) ---- */

const portRef_t port_value = { "value", "?", NULL, NULL, NULL, 0 };

/* the indices of the refs with an adapter (not the twins) in the order of their adapters (the address), then of
 * g_PortTable; sorted at the first use */
static int port_byAdapter[PORT_COUNT];
static int port_adapters = -1;

static uintptr_t Port_Addr(int i) {
	return (uintptr_t)g_PortTable[i]->adapter;
}

static int Port_AdapterOrder(const void *x, const void *y) {
	int a = *(const int *)x, b = *(const int *)y;
	uintptr_t p = Port_Addr(a), q = Port_Addr(b);
	return p < q ? -1 : p > q ? 1 : a - b;
}

/* the index in g_PortTable of the ref whose own function f is (Port_Own), -1 when there is none */
static int Port_OwnIndex(PyObject *f) {
	if (Port_Slow() || !f || !PyCFunction_Check(f) || PyCFunction_GET_SELF(f)) {
		return -1;
	}
	if (port_adapters < 0) {
		port_adapters = 0;
		for (int i = 0; i < PORT_COUNT; i++) {
			if (g_PortTable[i]->adapter) {
				port_byAdapter[port_adapters++] = i;
			}
		}
		qsort(port_byAdapter, port_adapters, sizeof(port_byAdapter[0]), Port_AdapterOrder);
	}
	const PyMethodDef *ml = ((PyCFunctionObject *)f)->m_ml;
	uintptr_t m = (uintptr_t)ml->ml_meth;
	int lo = 0, hi = port_adapters;
	while (lo < hi) {
		int mid = lo + (hi - lo) / 2;
		if (Port_Addr(port_byAdapter[mid]) < m) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	/* one adapter under several names (an alias in the table): the ref of the function's own name */
	for (; lo < port_adapters && Port_Addr(port_byAdapter[lo]) == m; lo++) {
		const portRef_t *ref = g_PortTable[port_byAdapter[lo]];
		if (!ref->py && !strcmp(ref->name, ml->ml_name)) {
			return port_byAdapter[lo];
		}
	}
	return -1;
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
 * (Port_OwnIndex, the arguments packed as its adapter would get them), else the call through Python of Native_Call;
 * the result into *out as Native_Arg takes it, or as the Python object into *pyout */
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
	byte ok;
	if (i >= 0) {
		nativeVal_t b[PORT_ARGS], r, res;
		/* the site names the port function it called last */
		site->ref = g_PortTable[i];
		site->fast++;
		for (int k = 0; k < n; k++) {
			Native_SetNone(&b[k]);
		}
		ok = 1;
		for (int k = 0; ok && k < n; k++) {
			ok = x ? Port_Pack(&x[k], &b[k]) : Native_Pass(&a[k], &b[k]);
		}
		if (ok) {
			ok = Port_Entry(site->ref, b, n, &r);
		} else {
			Native_SetNone(&r);
		}
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
		for (int k = 0; k < n; k++) {
			Native_Free(&b[k]);
		}
	} else {
		site->slow++;
		PyObject *r = Script_Invoke(f, x ? Native_PyArgs(x, n) : Port_PyValues(a, n));
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
