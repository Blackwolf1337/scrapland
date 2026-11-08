/*
 * Direct calls between the script ports (include/scrap/scriptnative.h, stage 3 of docs/objlib-plan.md): the seam
 * that tells whether a module attribute is still the port's own C function (OBJLIB 5), the call of its native entry
 * with the arguments packed as the adapter would get them, and the old call through Python when something else is
 * there (OBJLIB 7, with SCRAP_PORT_SLOW=1 forcing it and the counters of each call site for the test channel).
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

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

int Port_Seam(const portRef_t *ref, byte global, PyObject **hook) {
	PyObject *f = global ? Script_Global(ref->module, ref->name) : Script_Attr(ref->module, ref->name);
	if (!f) {
		return -1;
	}
	/* the port's own: the C function of its table entry (by the adapter: Mission_X.Init and Mission_Y.Init share the
	 * name), without self (the recorder of the test channel has one) */
	if (!Port_Slow() && PyCFunction_Check(f) && !PyCFunction_GET_SELF(f) &&
		((PyCFunctionObject *)f)->m_ml->ml_meth == ref->adapter) {
		Py_DECREF(f);
		return 0;
	}
	*hook = f;
	return 1;
}

byte Port_Arity(int n, int nargs) {
	return Native_Raise(NE_TYPE, "function requires exactly %d argument%s; %d given", nargs, nargs == 1 ? "" : "s", n);
}

portSite_t *Port_Sites(void) {
	return port_sites;
}

/* the entry of ref on the arguments of fmt, as Native_Args and the adapter's Mode_Args would give them, and the
 * result as the caller's Native_Arg would take it from the adapter's Mode_Out */
static byte Port_Fast(const portRef_t *ref, nativeVal_t *out, const char *fmt, va_list va) {
	nativeVal_t a[PORT_ARGS], r;
	int n = (int)strlen(fmt);
	byte ok = 1;
	CHECK(n <= PORT_ARGS);
	for (int i = 0; i < n; i++) {
		Native_SetNone(&a[i]);
	}
	for (int i = 0; ok && i < n; i++) {
		switch (fmt[i]) {
		case 'v':
			ok = Native_Pass(va_arg(va, const nativeVal_t *), &a[i]);
			break;
		case 'i':
			Native_SetInt(&a[i], va_arg(va, long));
			break;
		case 'd':
			Native_SetFloat(&a[i], va_arg(va, double));
			break;
		case 's': {
			const char *s = va_arg(va, const char *);
			Native_SetText(&a[i], s, (int)strlen(s));
			break;
		}
		case 'O': {
			/* a NULL object fails the call as Native_Args does */
			PyObject *o = va_arg(va, PyObject *);
			ok = o && Native_Arg(o, &a[i]);
			break;
		}
		}
	}
	Native_SetNone(&r);
	/* the frame address, as Script_CallObject takes it */
	if (ok && Script_Enter((uintptr_t)__builtin_frame_address(0))) {
		ok = ref->entry(a, n, &r);
		Script_Leave();
		if (!ok && !PyErr_Occurred()) {
			PyErr_SetString(PyExc_SystemError, "NULL result without error in call_object");
		}
		/* the result goes through the conversions also when it is dropped (they can fail) */
		nativeVal_t res;
		if (ok && (ok = Native_Pass(&r, &res)) != 0) {
			if (out) {
				*out = res;
			} else {
				Native_Free(&res);
			}
		}
	} else {
		ok = 0;
	}
	Native_Free(&r);
	for (int i = 0; i < n; i++) {
		Native_Free(&a[i]);
	}
	return ok;
}

byte Port_Invoke(portSite_t *site, nativeVal_t *out, const char *fmt, ...) {
	PyObject *hook = NULL;
	va_list va;
	byte ok = 0;
	if (!site->listed) {
		site->listed = 1;
		site->next = port_sites;
		port_sites = site;
	}
	int seam = Port_Seam(site->ref, site->global, &hook);
	va_start(va, fmt);
	if (seam > 0) {
		site->slow++;
		ok = Native_CallVa(hook, out, fmt, va);
	} else if (seam == 0) {
		site->fast++;
		ok = Port_Fast(site->ref, out, fmt, va);
	}
	va_end(va);
	if (!ok) {
		site->errors++;
	}
	return ok;
}
