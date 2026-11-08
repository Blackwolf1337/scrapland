/*
 * The command "native" of the test channel (src/game/Global/TestChannel.c): values of the native layer of the script
 * ports (src/game/Script/ScriptNative.c) read and printed without Python objects, and the check of the native repr
 * and literal reader against the linked Python 1.5.2 (tools/test/litfuzz.sh). Test only, off with the channel.
 *   native <literal>         the repr of what the native literal reader makes of the text
 *   native str <literal>     its str
 *   native check <literal>   its text as check() of the golden test writes it (floats with 17 digits)
 *   native ports             (OBJLIB 7) the counts of the direct calls between ports (PORT_CALL, PORT_OWN) per
 *                            call site that ran: "<file>:<line> <Module>.<Func> attr|own fast <n> slow <n> errors
 *                            <n>", sorted by file and line
 *   native ports reset       the counts back to 0
 *   native py                (STAGE 7, the build with the interpreter) puts three functions into __main__ for code
 *                            that the channel's py command runs:
 *                              _native_lit(text, globals): (native taken, Script_Literal taken, the native value as
 *                                a Python object, its repr, str and check texts); None for the four when not taken
 *                              _native_repr(value): None when the value has no native form, else (the native copy
 *                                back as a Python object, its repr, str and check texts)
 *                              _native_pack(value, vec): the check of the packer of the direct calls (Native_Pass):
 *                                the value as an adapter takes it (Native_Arg; vec 1: every tuple of three floats in
 *                                it a vector), then Native_Pass of it against Native_ToPy and Native_Arg; None when
 *                                both are the same value (types, digits, texts, dict slots), else (packer repr, round
 *                                trip repr)
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/pyfunction.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

static void TN_Line(nativeBuf_t *out, const char *text, nativeRepr_t mode) {
	nativeVal_t v;
	if (!Native_Literal(text, &v)) {
		Native_BufAdd(out, "error: native: not a literal of the native reader\n", -1);
		return;
	}
	Native_Repr(out, &v, mode);
	Native_BufAdd(out, "\n", 1);
	Native_Free(&v);
}

/* the sites by file, then by line number */
static int TN_SiteOrder(const void *x, const void *y) {
	const portSite_t *a = *(const portSite_t *const *)x, *b = *(const portSite_t *const *)y;
	int c = strcmp(a->file, b->file);
	return c ? c : a->line - b->line;
}

/* native ports [reset] */
static void TN_Ports(nativeBuf_t *out, byte reset) {
	static const portSite_t *sites[4096];
	int n = 0;
	for (portSite_t *s = Port_Sites(); s; s = s->next) {
		if (reset) {
			s->fast = s->slow = s->errors = 0;
		} else if (n < (int)(sizeof(sites) / sizeof(sites[0]))) {
			sites[n++] = s;
		}
	}
	if (reset) {
		Native_BufAdd(out, "ok\n", -1);
		return;
	}
	qsort(sites, n, sizeof(sites[0]), TN_SiteOrder);
	for (int i = 0; i < n; i++) {
		const char *f = strstr(sites[i]->file, "src/game/Script/");
		char line[512];
		snprintf(line, sizeof(line), "%s:%d %s.%s %s fast %lu slow %lu errors %lu\n", f ? f + 16 : sites[i]->file,
			sites[i]->line, sites[i]->ref->module, sites[i]->ref->name, sites[i]->global ? "own" : "attr",
			sites[i]->fast, sites[i]->slow, sites[i]->errors);
		Native_BufAdd(out, line, -1);
	}
	if (!n) {
		Native_BufAdd(out, "no sites\n", -1);
	}
}

#ifndef SCRAP_NO_PYTHON
/* (value as a Python object, repr, str, check) of v; NULL with the error set */
static PyObject *TN_Texts(const nativeVal_t *v) {
	static const nativeRepr_t modes[] = { NR_REPR, NR_STR, NR_CHECK };
	PyObject *t = PyTuple_New(4), *o = t ? Native_ToPy(v) : NULL;
	if (!o) {
		Py_XDECREF(t);
		return NULL;
	}
	PyTuple_SetItem(t, 0, o);
	for (int i = 0; i < 3; i++) {
		nativeBuf_t b;
		Native_BufInit(&b);
		Native_Repr(&b, v, modes[i]);
		PyTuple_SetItem(t, i + 1, PyString_FromStringAndSize(b.p, b.len));
		Native_BufFree(&b);
	}
	return t;
}

static PyObject *TN_Lit(PyObject *self, PyObject *args) {
	char *text;
	PyObject *globals;
	nativeVal_t v;
	(void)self;
	if (!PyArg_ParseTuple(args, "sO!", &text, &PyDict_Type, &globals)) {
		return NULL;
	}
	PyObject *ref = Script_Literal(globals, text);
	byte took = Native_Literal(text, &v);
	PyObject *texts = took ? TN_Texts(&v) : Py_BuildValue("(OOOO)", Py_None, Py_None, Py_None, Py_None);
	Native_Free(&v);
	PyObject *r = texts ? Py_BuildValue("(iiO)", took, ref != NULL, texts) : NULL;
	Py_XDECREF(ref);
	Py_XDECREF(texts);
	return r;
}

static PyObject *TN_Repr(PyObject *self, PyObject *args) {
	PyObject *o;
	nativeVal_t v;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &o)) {
		return NULL;
	}
	if (!Native_FromPyAny(o, &v)) {
		return Py_ReturnNone();
	}
	PyObject *r = TN_Texts(&v);
	Native_Free(&v);
	return r;
}

/* every tuple of three floats in v a vector (the packer check of values with vectors) */
static void TN_Vecs(nativeVal_t *v) {
	if (v->type == NV_TUPLE && v->n == 3 && v->items[0].type == NV_FLOAT && v->items[1].type == NV_FLOAT &&
		v->items[2].type == NV_FLOAT) {
		double x = v->items[0].f, y = v->items[1].f, z = v->items[2].f;
		Native_Free(v);
		Native_SetVec(v, x, y, z);
	} else if (v->type == NV_LIST || v->type == NV_TUPLE) {
		for (int i = 0; i < v->n; i++) {
			TN_Vecs(&v->items[i]);
		}
	} else if (v->type == NV_DICT) {
		/* the values only: a key keeps its hash */
		for (int i = 0; i < v->max; i++) {
			if (v->items[2 * i].type <= NV_OBJ) {
				TN_Vecs(&v->items[2 * i + 1]);
			}
		}
	}
}

/* a and b are the same value: types, numbers to the bit, texts, items, the slots of a dict, the object of an NV_OBJ */
static byte TN_Same(const nativeVal_t *a, const nativeVal_t *b) {
	if (a->type != b->type) {
		return 0;
	}
	switch (a->type) {
	case NV_INT:
		return a->i == b->i;
	case NV_FLOAT:
		return !memcmp(&a->f, &b->f, sizeof(a->f));
	case NV_VEC:
		return !memcmp(a->v, b->v, sizeof(a->v));
	case NV_STR:
	case NV_LONG:
		return a->n == b->n && !memcmp(Native_Text(a), Native_Text(b), a->n);
	case NV_LIST:
	case NV_TUPLE:
		for (int i = 0; a->n == b->n && i < a->n; i++) {
			if (!TN_Same(&a->items[i], &b->items[i])) {
				return 0;
			}
		}
		return a->n == b->n;
	case NV_DICT:
		if (a->n != b->n || a->max != b->max || a->i != b->i) {
			return 0;
		}
		for (int i = 0; i < 2 * a->max; i++) {
			/* a free or deleted slot has a key type beyond NV_OBJ and no value */
			if (a->items[i].type > NV_OBJ ? a->items[i].type != b->items[i].type : !TN_Same(&a->items[i], &b->items[i])) {
				return 0;
			}
			if (!(i & 1) && a->items[i].type > NV_OBJ) {
				i++;
			}
		}
		return 1;
	case NV_OBJ:
		return a->obj == b->obj;
	}
	return 1;
}

static PyObject *TN_Pack(PyObject *self, PyObject *args) {
	PyObject *o, *r = NULL;
	int vec = 0;
	nativeVal_t v, pass, trip;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|i", &o, &vec)) {
		return NULL;
	}
	Native_Arg(o, &v);
	if (vec && v.type != NV_OBJ) {
		TN_Vecs(&v);
	}
	PyObject *p = Native_ToPy(&v);
	byte ok = p && Native_Pass(&v, &pass);
	if (ok) {
		Native_Arg(p, &trip);
		if (TN_Same(&pass, &trip)) {
			r = Py_ReturnNone();
		} else {
			nativeBuf_t a, b;
			Native_BufInit(&a);
			Native_BufInit(&b);
			Native_Repr(&a, &pass, NR_REPR);
			Native_Repr(&b, &trip, NR_REPR);
			r = Py_BuildValue("(s#s#)", a.p, a.len, b.p, b.len);
			Native_BufFree(&a);
			Native_BufFree(&b);
		}
		Native_Free(&trip);
		Native_Free(&pass);
	}
	Py_XDECREF(p);
	Native_Free(&v);
	return r;
}

static PyMethodDef tn_methods[] = {
	{ "_native_lit", TN_Lit, METH_VARARGS, NULL },
	{ "_native_repr", TN_Repr, METH_VARARGS, NULL },
	{ "_native_pack", TN_Pack, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

static void TN_Install(nativeBuf_t *out) {
	PyObject *main = g_PythonReady ? PyImport_AddModule("__main__") : NULL;
	for (PyMethodDef *m = tn_methods; main && m->ml_name; m++) {
		PyObject *f = PyCFunction_New(m, NULL);
		if (!f || PyDict_SetItemString(PyModule_GetDict(main), m->ml_name, f) != 0) {
			main = NULL;
		}
		Py_XDECREF(f);
	}
	PyErr_Clear();
	Native_BufAdd(out, main ? "ok\n" : "error: native: the script modules are not ready\n", -1);
}
#endif

void TestNative_Command(nativeBuf_t *out, const char *arg) {
	if (!strncmp(arg, "str ", 4)) {
		TN_Line(out, arg + 4, NR_STR);
	} else if (!strncmp(arg, "check ", 6)) {
		TN_Line(out, arg + 6, NR_CHECK);
	} else if (!strcmp(arg, "ports") || !strcmp(arg, "ports reset")) {
		TN_Ports(out, arg[5] != 0);
	} else if (!strcmp(arg, "py")) {
#ifndef SCRAP_NO_PYTHON
		TN_Install(out);
#else
		Native_BufAdd(out, "error: SystemError: py: no Python interpreter in this build\n", -1);
#endif
	} else {
		TN_Line(out, arg, NR_REPR);
	}
}
