/*
 * The string module of the packs in C (scripts/python/string.pyc, the string.py of Python 1.5.2). Its module code is
 * data/modules/string.mod: the variables, and for most functions the C functions of the built-in module strop (the
 * script ends with `from strop import *`, which replaces its own Python versions). The functions string.py keeps in
 * Python are here: index, rindex, ljust, rjust, center, zfill and capwords. They use the operators of the script on
 * any object (len, -, *, +, the comparisons), so a wrong argument fails with the error of the line of string.py, and
 * load the other functions (find, rfind, split, capitalize, join) and the builtins as the script does (LOAD_GLOBAL:
 * the module, then the builtins).
 */
#include <limits.h>

#include "scrap/script.h"

#define K(o) Script_Keep(&r, (o))

/* a global of string as its functions load it (the module, then the builtins): a new reference */
static PyObject *Str_Global(const char *name) {
	PyObject *m = Script_Module("string"), *v = NULL;
	int found = m ? Script_NameChain(NULL, PyModule_GetDict(m), name, &v) : -1;
	if (found == 0) {
		PyErr_Format(PyExc_SystemError, "string: cannot load %.100s", name);
	}
	Py_XDECREF(m);
	return found > 0 ? v : NULL;
}

/* width - len(s) (a new reference) */
static PyObject *Str_Room(PyObject *width, PyObject *s) {
	int len = PyObject_Length(s);
	PyObject *n = len < 0 && PyErr_Occurred() ? NULL : PyInt_FromLong(len);
	PyObject *d = n ? PyNumber_Subtract(width, n) : NULL;
	Py_XDECREF(n);
	return d;
}

/* the sign of PyObject_Compare(a, b) as cmp_outcome of ceval takes it; -2 with the error set */
static int Str_Cmp(PyObject *a, PyObject *b) {
	if (!a || !b) {
		return -2;
	}
	int c = PyObject_Compare(a, b);
	return c && PyErr_Occurred() ? -2 : c < 0 ? -1 : c > 0;
}

/* `n <= 0` of the padding functions: 1, 0, or -1 with the error set */
static int Str_None(PyObject *n) {
	PyObject *zero = PyInt_FromLong(0);
	int c = Str_Cmp(n, zero);
	Py_XDECREF(zero);
	return c == -2 ? -1 : c <= 0;
}

/* text * n (a new reference) */
static PyObject *Str_Times(const char *text, PyObject *n) {
	PyObject *t = n ? PyString_FromString(text) : NULL;
	PyObject *v = t ? PyNumber_Multiply(t, n) : NULL;
	Py_XDECREF(t);
	return v;
}

/* index(s, sub, i=0, last=None) and rindex: find (rfind) of the module, ValueError when the result is negative */
static PyObject *Str_Index(PyObject *args, PyObject *kw, const char *find) {
	static char *kwlist[] = { "s", "sub", "i", "last", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *s, *sub, *i = NULL, *last = Py_None;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OO", kwlist, &s, &sub, &i, &last)) {
		return NULL;
	}
	i = i ? i : K(PyInt_FromLong(0));
	if (i && last == Py_None) {
		int len = PyObject_Length(s);
		last = len < 0 && PyErr_Occurred() ? NULL : K(PyInt_FromLong(len));
	}
	PyObject *f = i && last ? K(Str_Global(find)) : NULL;
	PyObject *res = f ? K(Script_CallObject(f, K(Py_BuildValue("(OOOO)", s, sub, i, last)), NULL)) : NULL;
	PyObject *zero = res ? K(PyInt_FromLong(0)) : NULL;
	int c = Str_Cmp(res, zero);
	if (c == -2) {
		return Script_Done(&r, NULL);
	}
	if (c < 0) {
		PyErr_SetString(PyExc_ValueError, "substring not found in string.index");
		return Script_Done(&r, NULL);
	}
	return Script_Done(&r, Script_Ref(res));
}

static PyObject *String_index(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Str_Index(args, kw, "find");
}

static PyObject *String_rindex(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Str_Index(args, kw, "rfind");
}

/* ljust(s, width), rjust(s, width): s with blanks after (before) it up to width; s itself when it is that long */
static PyObject *Str_Just(PyObject *args, byte right) {
	scriptRefs_t r = { .n = 0 };
	PyObject *s, *width;
	if (!PyArg_ParseTuple(args, "OO", &s, &width)) {
		return NULL;
	}
	PyObject *n = K(Str_Room(width, s));
	int le = n ? Str_None(n) : -1;
	if (le) {
		return Script_Done(&r, le > 0 ? Script_Ref(s) : NULL);
	}
	PyObject *pad = K(Str_Times(" ", n));
	return Script_Done(&r, !pad ? NULL : right ? PyNumber_Add(pad, s) : PyNumber_Add(s, pad));
}

static PyObject *String_ljust(PyObject *self, PyObject *args) {
	(void)self;
	return Str_Just(args, 0);
}

static PyObject *String_rjust(PyObject *self, PyObject *args) {
	(void)self;
	return Str_Just(args, 1);
}

/* center(s, width): half the blanks before s (one more when both n and width are odd), the rest after it */
static PyObject *String_center(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *s, *width;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &s, &width)) {
		return NULL;
	}
	PyObject *n = K(Str_Room(width, s));
	int le = n ? Str_None(n) : -1;
	if (le) {
		return Script_Done(&r, le > 0 ? Script_Ref(s) : NULL);
	}
	PyObject *two = K(PyInt_FromLong(2));
	PyObject *half = two ? K(PyNumber_Divide(n, two)) : NULL;
	/* if n%2 and width%2: half = half+1 */
	int odd = half ? Script_True(K(PyNumber_Remainder(n, two))) : -1;
	if (odd > 0) {
		odd = Script_True(K(PyNumber_Remainder(width, two)));
		if (odd > 0) {
			PyObject *one = K(PyInt_FromLong(1));
			half = one ? K(PyNumber_Add(half, one)) : NULL;
			odd = half ? 0 : -1;
		}
	}
	PyObject *left = odd >= 0 ? K(Str_Times(" ", half)) : NULL;
	PyObject *mid = left ? K(PyNumber_Add(left, s)) : NULL;
	PyObject *rest = mid ? K(PyNumber_Subtract(n, half)) : NULL;
	PyObject *right = rest ? K(Str_Times(" ", rest)) : NULL;
	return Script_Done(&r, right ? PyNumber_Add(mid, right) : NULL);
}

/* zfill(x, width): the string (or the repr of x) with zeros after its sign up to width */
static PyObject *String_zfill(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *x, *width;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &x, &width)) {
		return NULL;
	}
	PyObject *s = x->ob_type == &PyString_Type ? x : K(PyObject_Repr(x));
	int len = s ? PyObject_Length(s) : -1;
	PyObject *n = len < 0 && (!s || PyErr_Occurred()) ? NULL : K(PyInt_FromLong(len));
	int c = Str_Cmp(n, width);
	if (c == -2 || c >= 0) {
		return Script_Done(&r, c == -2 ? NULL : Script_Ref(s));
	}
	PyObject *sign = K(PyString_FromString(""));
	PyObject *first = sign ? K(PySequence_GetItem(s, 0)) : NULL;
	if (!first) {
		return Script_Done(&r, NULL);
	}
	const char *f = PyString_Check(first) ? PyString_AS_STRING(first) : "";
	if ((f[0] == '-' || f[0] == '+') && f[1] == 0) {
		sign = first;
		s = K(PySequence_GetSlice(s, 1, INT_MAX));
	}
	PyObject *zeros = s ? K(Str_Times("0", K(PyNumber_Subtract(width, n)))) : NULL;
	PyObject *head = zeros ? K(PyNumber_Add(sign, zeros)) : NULL;
	return Script_Done(&r, head ? PyNumber_Add(head, s) : NULL);
}

/* capwords(s, sep=None): join(map(capitalize, split(s, sep)), sep or ' ') with the functions of the module */
static PyObject *String_capwords(PyObject *self, PyObject *args, PyObject *kw) {
	static char *kwlist[] = { "s", "sep", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *s, *sep = Py_None;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", kwlist, &s, &sep)) {
		return NULL;
	}
	PyObject *join = K(Str_Global("join"));
	PyObject *map = join ? K(Str_Global("map")) : NULL;
	PyObject *cap = map ? K(Str_Global("capitalize")) : NULL;
	PyObject *split = cap ? K(Str_Global("split")) : NULL;
	PyObject *words = split ? K(Script_CallObject(split, K(Py_BuildValue("(OO)", s, sep)), NULL)) : NULL;
	PyObject *caps = words ? K(Script_CallObject(map, K(Py_BuildValue("(OO)", cap, words)), NULL)) : NULL;
	int t = caps ? PyObject_IsTrue(sep) : -1;
	PyObject *with = t < 0 ? NULL : t ? sep : K(PyString_FromString(" "));
	return Script_Done(&r, with ? Script_CallObject(join, K(Py_BuildValue("(OO)", caps, with)), NULL) : NULL);
}

PyMethodDef g_StringMethods[] = {
	{ "index", (PyCFunction)(void (*)(void))String_index, 3, NULL },
	{ "rindex", (PyCFunction)(void (*)(void))String_rindex, 3, NULL },
	{ "ljust", String_ljust, 1, NULL },
	{ "rjust", String_rjust, 1, NULL },
	{ "center", String_center, 1, NULL },
	{ "zfill", String_zfill, 1, NULL },
	{ "capwords", (PyCFunction)(void (*)(void))String_capwords, 3, NULL },
	{ NULL, NULL, 0, NULL },
};
