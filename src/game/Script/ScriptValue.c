/*
 * Python values as the scripts use them, shared by the gameplay ports (include/scrap/script.h): the references of a
 * C function released together, module globals that raise NameError like the script, string and number operations
 * of the interpreter, the print statement, tuple unpacking and the for loop over a sequence.
 */
#include <ctype.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/scrap.h"
#include "scrap/script.h"

PyObject *Script_Keep(scriptRefs_t *r, PyObject *o) {
	if (o) {
		CHECK(r->n < SCRIPT_REFS);
		r->o[r->n++] = o;
	}
	return o;
}

PyObject *Script_Done(scriptRefs_t *r, PyObject *result) {
	while (r->n > 0) {
		r->n--;
		Py_DECREF(r->o[r->n]);
	}
	return result;
}

PyObject *Script_Ref(PyObject *o) {
	Py_XINCREF(o);
	return o;
}

PyObject *Script_Global(const char *module, const char *name) {
	PyObject *m = PyImport_ImportModule((char *)module);
	PyObject *v = m ? PyDict_GetItemString(PyModule_GetDict(m), (char *)name) : NULL;
	if (m && !v) {
		PyErr_SetString(PyExc_NameError, (char *)name);
	}
	Py_XINCREF(v);
	Py_XDECREF(m);
	return v;
}

byte Script_SetGlobal(const char *module, const char *name, PyObject *v) {
	PyObject *m = v ? PyImport_ImportModule((char *)module) : NULL;
	byte ok = m && PyDict_SetItemString(PyModule_GetDict(m), (char *)name, v) == 0;
	Py_XDECREF(m);
	Py_XDECREF(v);
	return ok;
}

byte Script_Import(const char *const *names) {
	for (; *names; names++) {
		PyObject *m = PyImport_ImportModule((char *)*names);
		if (!m) {
			return 0;
		}
		Py_DECREF(m);
	}
	return 1;
}

PyObject *Script_Index(PyObject *o, int i) {
	PyObject *k = o ? PyInt_FromLong(i) : NULL;
	PyObject *v = k ? PyObject_GetItem(o, k) : NULL;
	Py_XDECREF(k);
	return v;
}

PyObject *Script_Plus(PyObject *a, PyObject *b) {
	PyObject *r = a && b ? PyNumber_Add(a, b) : NULL;
	Py_XDECREF(a);
	return r;
}

PyObject *Script_PlusS(PyObject *a, const char *text) {
	PyObject *t = a ? PyString_FromString(text) : NULL;
	PyObject *r = Script_Plus(a, t);
	Py_XDECREF(t);
	return r;
}

PyObject *Script_SPlus(const char *text, PyObject *b) {
	return Script_Plus(PyString_FromString(text), b);
}

PyObject *Script_Mod(PyObject *a, PyObject *b) {
	if (!a || !b) {
		return NULL;
	}
	return PyString_Check(a) ? PyString_Format(a, b) : PyNumber_Remainder(a, b);
}

byte Script_Is(PyObject *o, const char *text) {
	return o && PyString_Check(o) && !strcmp(PyString_AsString(o), text);
}

byte Script_In(PyObject *o, const char *const *texts) {
	for (; *texts; texts++) {
		if (Script_Is(o, *texts)) {
			return 1;
		}
	}
	return 0;
}

int Script_True(PyObject *o) {
	return o ? PyObject_IsTrue(o) : -1;
}

int Script_Eq(PyObject *a, PyObject *b) {
	if (!a || !b) {
		return -1;
	}
	int c = PyObject_Compare(a, b);
	return PyErr_Occurred() ? -1 : c == 0;
}

PyObject *Script_Int(PyObject *o) {
	return o ? PyNumber_Int(o) : NULL;
}

PyObject *Script_Repr(PyObject *o) {
	return o ? PyObject_Repr(o) : NULL;
}

byte Script_Range(PyObject *n, long *count) {
	if (!n) {
		return 0;
	}
	/* the conversion of range's argument parser, with its errors */
	return PyArg_Parse(n, "l", count) != 0;
}

int Script_Next(PyObject *seq, int i, PyObject **item) {
	*item = NULL;
	if (!seq) {
		return -1;
	}
	if (!PySequence_Check(seq)) {
		PyErr_SetString(PyExc_TypeError, "loop over non-sequence");
		return -1;
	}
	*item = PySequence_GetItem(seq, i);
	if (*item) {
		return 1;
	}
	if (PyErr_ExceptionMatches(PyExc_IndexError)) {
		PyErr_Clear();
		return 0;
	}
	return -1;
}

byte Script_Print(PyObject *const *items, int n) {
	PyObject *w = PySys_GetObject("stdout");
	if (!w) {
		PyErr_SetString(PyExc_RuntimeError, "lost sys.stdout");
		return 0;
	}
	for (int i = 0; i < n; i++) {
		if ((PyFile_SoftSpace(w, 1) && PyFile_WriteString(" ", w) != 0) || PyFile_WriteObject(items[i], w, Py_PRINT_RAW) != 0) {
			return 0;
		}
		if (PyString_Check(items[i])) {
			const char *s = PyString_AsString(items[i]);
			int len = PyString_Size(items[i]);
			if (len > 0 && isspace((unsigned char)s[len - 1]) && s[len - 1] != ' ') {
				PyFile_SoftSpace(w, 0);
			}
		}
	}
	if (PyFile_WriteString("\n", w) != 0) {
		return 0;
	}
	PyFile_SoftSpace(w, 0);
	return 1;
}

byte Script_Unpack(PyObject *v, int n, PyObject **out) {
	if (!PySequence_Check(v)) {
		PyErr_SetString(PyExc_TypeError, "unpack non-sequence");
		return 0;
	}
	if (PySequence_Length(v) != n) {
		PyErr_SetString(PyExc_ValueError, PyTuple_Check(v) ? "unpack tuple of wrong size" :
			PyList_Check(v) ? "unpack list of wrong size" : "unpack sequence of wrong size");
		return 0;
	}
	for (int i = 0; i < n; i++) {
		out[i] = PySequence_GetItem(v, i);
		if (!out[i]) {
			while (i-- > 0) {
				Py_DECREF(out[i]);
			}
			return 0;
		}
	}
	return 1;
}

PyObject *Script_LangOf(PyObject *key) {
	PyObject *s = key ? Script_Ask("Scrap", "GetLangStr", "(O)", key) : NULL;
	Py_XDECREF(key);
	return s;
}

PyObject *Script_SaveVar(const char *name) {
	return Script_Ask("Scrap", "GetSaveVar", "(s)", name);
}

byte Script_Hook(const char *module, const char *target, const char *name, const char *last) {
	PyObject *prev = Script_Attr(target, name);
	if (!prev || !Script_SetGlobal(module, last, prev)) {
		return 0;
	}
	PyObject *mine = Script_Global(module, name), *t = mine ? PyImport_ImportModule((char *)target) : NULL;
	int bad = !t || PyObject_SetAttrString(t, (char *)name, mine) < 0;
	Py_XDECREF(t);
	Py_XDECREF(mine);
	return !bad;
}

PyObject *Script_EvalIn(const char *module, PyObject *text) {
	PyObject *mod = PyImport_ImportModule((char *)module);
	PyObject *g = mod ? PyModule_GetDict(mod) : NULL;
	PyObject *v = g ? Script_Ask("__builtin__", "eval", "(OO)", text, g) : NULL;
	Py_XDECREF(mod);
	return v;
}

PyObject *Script_Entity(PyObject *name) {
	return name ? Script_Ask("Scrap", "GetEntity", "(O)", name) : NULL;
}

PyObject *Script_Usr(void) {
	return Script_Ask("Scrap", "UsrEntity", "(i)", 0);
}

PyObject *Script_AskKw(const char *module, const char *func, PyObject *args, PyObject *kw) {
	PyObject *f = args ? Script_Attr(module, func) : NULL;
	PyObject *r = f ? PyEval_CallObjectWithKeywords(f, args, kw) : NULL;
	Py_XDECREF(f);
	Py_XDECREF(args);
	Py_XDECREF(kw);
	return r;
}
