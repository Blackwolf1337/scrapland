/*
 * Direct calls between the script ports (include/scrap/scriptnative.h, stage 3 of docs/objlib-plan.md): the seam
 * that tells whether a module attribute is still the port's own C function (OBJLIB 5), the call of its native entry
 * with the arguments packed as the adapter would get them, and the old call through Python when something else is
 * there (OBJLIB 7, with SCRAP_PORT_SLOW=1 forcing it and the counters of each call site for the test channel).
 * At the end the registry of stage 4: the refs by module and name (Port_Find on g_PortTable) and the callbacks bound
 * to port functions (Port_Callback, cPy_Function PYF_ENTRY).
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

/* the arguments of a call: the positional ones (npos), then the keywords with their names */
typedef struct {
	nativeArg_t a[PORT_ARGS];
	char names[PORT_ARGS][0x40];
	int npos, n;
} portArgs_t;

/* the arguments of fmt: its letters, then after a ';' the keywords "name=x" separated by ',' */
static void Port_Read(const char *fmt, va_list va, portArgs_t *p) {
	char letters[PORT_ARGS + 1];
	const char *semi = strchr(fmt, ';');
	int n = semi ? (int)(semi - fmt) : (int)strlen(fmt);
	CHECK(n <= PORT_ARGS);
	memcpy(letters, fmt, n);
	p->npos = n;
	for (const char *q = semi; q; n++) {
		const char *name = q + 1, *eq = strchr(name, '=');
		CHECK(eq && eq > name && eq - name < (int)sizeof(p->names[0]) && eq[1] && (!eq[2] || eq[2] == ',') && n < PORT_ARGS);
		memcpy(p->names[n], name, eq - name);
		p->names[n][eq - name] = 0;
		letters[n] = eq[1];
		q = eq[2] ? eq + 2 : NULL;
	}
	letters[n] = 0;
	p->n = Native_ReadArgs(letters, va, p->a);
}

/* the slot of the entry for each argument (slot[i]) and their count (*m): 1, or 0 when the entry cannot take them
 * that way (keywords to a ref without names, a name it does not have, a slot given twice) */
static byte Port_Slots(const portRef_t *ref, const portArgs_t *p, int *slot, int *m) {
	byte used[PORT_ARGS] = { 0 };
	*m = p->npos;
	for (int i = 0; i < p->npos; i++) {
		slot[i] = i;
		used[i] = 1;
	}
	for (int i = p->npos; i < p->n; i++) {
		int k = 0;
		while (ref->kw && ref->kw[k] && strcmp(ref->kw[k], p->names[i])) {
			k++;
		}
		if (!ref->kw || !ref->kw[k] || k >= PORT_ARGS || used[k]) {
			return 0;
		}
		used[k] = 1;
		slot[i] = k;
		*m = k + 1 > *m ? k + 1 : *m;
	}
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
static byte Port_Fast(const portRef_t *ref, const portArgs_t *p, const int *slot, int m, nativeVal_t *r) {
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

/* the call through Python (the hook taken over): the positional arguments as Native_Call passes them, the keywords as
 * the dict of Mode_CallKw, in their order */
static PyObject *Port_SlowCall(PyObject *hook, const portArgs_t *p) {
	PyObject *t = Native_PyArgs(p->a, p->npos), *kw = NULL;
	byte ok = t != NULL;
	if (ok && p->n > p->npos) {
		ok = (kw = PyDict_New()) != NULL;
		for (int i = p->npos; ok && i < p->n; i++) {
			PyObject *v = Native_ArgPy(&p->a[i]);
			ok = v && PyDict_SetItemString(kw, (char *)p->names[i], v) == 0;
			Py_XDECREF(v);
		}
	}
	PyObject *r = ok ? Script_CallObject(hook, t, kw) : NULL;
	Py_XDECREF(kw);
	Py_XDECREF(t);
	Py_DECREF(hook);
	return r;
}

/* the way of a native twin of a library function (a ref without an adapter, OBJLIB 9): its entry (0) after the import
 * of its module, as the lookup by name imported it, or the function by name (1, with *hook) for an argument that
 * holds an object, keywords and under SCRAP_PORT_SLOW=1; -1 with the error of the import or the lookup */
static int Port_Twin(const portRef_t *ref, const portArgs_t *p, byte direct, PyObject **hook) {
	byte slow = !direct || p->n > p->npos || Port_Slow();
	for (int i = 0; !slow && i < p->n; i++) {
		slow = p->a[i].type == 'O' || p->a[i].type == 'P' || (p->a[i].type == 'v' && Port_HoldsObj(p->a[i].v));
	}
	if (slow) {
		return Port_Look(ref, 0, 1, hook);
	}
	PyObject *m = PyImport_ImportModule((char *)ref->module);
	Py_XDECREF(m);
	return m ? 0 : -1;
}

/* a call of a site: the result into *out as Native_Arg takes it (pyout NULL), or as the Python object into *pyout */
static byte Port_Run(portSite_t *site, nativeVal_t *out, PyObject **pyout, const char *fmt, va_list va) {
	portArgs_t p;
	int slot[PORT_ARGS], m = 0;
	PyObject *hook = NULL;
	byte ok = 0;
	if (!site->listed) {
		site->listed = 1;
		site->next = port_sites;
		port_sites = site;
	}
	Port_Read(fmt, va, &p);
	byte direct = !site->ref->py && Port_Slots(site->ref, &p, slot, &m);
	int seam = site->ref->adapter ? Port_Look(site->ref, site->global, !direct, &hook) : Port_Twin(site->ref, &p, direct, &hook);
	if (seam > 0) {
		site->slow++;
		PyObject *r = Port_SlowCall(hook, &p);
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
		nativeVal_t r, res;
		site->fast++;
		ok = Port_Fast(site->ref, &p, slot, m, &r);
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

portSite_t *Port_Callback(const char *module, const char *name, PyObject *f) {
	for (int i = Port_Index(module, name); i < PORT_COUNT && !Port_Order(g_PortTable[i], module, name); i++) {
		const portRef_t *ref = g_PortTable[i];
		/* a function of the same adapter under another name stays Python: the name of the binding is the
		 * function's own (__name__) */
		if (ref->py || !Port_IsOwn(ref, f) || strcmp(((PyCFunctionObject *)f)->m_ml->ml_name, ref->name)) {
			continue;
		}
		portSite_t *s = &port_callbacks[i];
		if (!s->listed) {
			s->ref = ref;
			s->file = "callback";
			s->listed = 1;
			s->next = port_callbackList;
			port_callbackList = s;
			Py_INCREF(f);
			port_callbackFuncs[i] = f;
		}
		return s;
	}
	return NULL;
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
