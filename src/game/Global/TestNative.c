/*
 * The command "native" of the test channel (src/game/Global/TestChannel.c): values of the native layer of the script
 * ports (src/game/Script/ScriptNative.c) read and printed without Python objects, and the check of the native repr
 * and literal reader against the linked Python 1.5.2 (tools/test/litfuzz.sh). Test only, off with the channel.
 *   native <literal>         the repr of what the native literal reader makes of the text
 *   native str <literal>     its str
 *   native check <literal>   its text as check() of the golden test writes it (floats with 17 digits)
 *   native py                (STAGE 7, the build with the interpreter) puts two functions into __main__ for code
 *                            that the channel's py command runs:
 *                              _native_lit(text, globals): (native taken, Script_Literal taken, the native value as
 *                                a Python object, its repr, str and check texts); None for the four when not taken
 *                              _native_repr(value): None when the value has no native form, else (the native copy
 *                                back as a Python object, its repr, str and check texts)
 */
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

static PyMethodDef tn_methods[] = {
	{ "_native_lit", TN_Lit, METH_VARARGS, NULL },
	{ "_native_repr", TN_Repr, METH_VARARGS, NULL },
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
