/*
 * The native layer of the script ports (include/scrap/scriptnative.h, docs/objlib-plan.md): C values, the errors a
 * native function raises, the engine reads with their test seams and the mirror of module state in the module dict.
 */
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

/* ---- values ---- */

void Native_SetNone(nativeVal_t *v) {
	v->type = NV_NONE;
}

void Native_SetInt(nativeVal_t *v, long i) {
	v->type = NV_INT;
	v->i = i;
}

void Native_SetFloat(nativeVal_t *v, double f) {
	v->type = NV_FLOAT;
	v->f = f;
}

void Native_SetStr(nativeVal_t *v, const char *s) {
	v->type = NV_STR;
	Str_Copy(v->s, s, sizeof(v->s));
}

byte Native_IsStr(const nativeVal_t *v, const char *text) {
	return v->type == NV_STR && !strcmp(v->s, text);
}

byte Native_Equal(const nativeVal_t *a, const nativeVal_t *b) {
	byte an = a->type == NV_INT || a->type == NV_FLOAT, bn = b->type == NV_INT || b->type == NV_FLOAT;
	if (an && bn) {
		if (a->type == NV_INT && b->type == NV_INT) {
			return a->i == b->i;
		}
		return (a->type == NV_INT ? (double)a->i : a->f) == (b->type == NV_INT ? (double)b->i : b->f);
	}
	if (a->type != b->type) {
		return 0;
	}
	switch (a->type) {
	case NV_NONE:
		return 1;
	case NV_STR:
		return !strcmp(a->s, b->s);
	case NV_VEC:
		return a->v[0] == b->v[0] && a->v[1] == b->v[1] && a->v[2] == b->v[2];
	}
	return 0;
}

/* ---- errors ---- */

byte Native_Raise(nativeError_t kind, const char *fmt, ...) {
	/* OBJLIB 7: the Python exception of the same name */
	static PyObject **const types[] = {
		[NE_INDEX] = &PyExc_IndexError,
		[NE_VALUE] = &PyExc_ValueError,
		[NE_TYPE] = &PyExc_TypeError,
		[NE_ATTRIBUTE] = &PyExc_AttributeError,
		[NE_SYSTEM] = &PyExc_SystemError,
	};
	char text[0x200];
	va_list va;
	va_start(va, fmt);
	vsnprintf(text, sizeof(text), fmt, va);
	va_end(va);
	PyErr_SetString(*types[kind], text);
	return 0;
}

byte Native_Index(long i, int count, int *out) {
	if (i < 0) {
		i += count;
	}
	if (i < 0 || i >= count) {
		return Native_Raise(NE_INDEX, "list index out of range");
	}
	*out = (int)i;
	return 1;
}

byte Native_IntOfText(const char *text, long *out) {
	/* int_from_string of abstract.c over PyOS_strtol (mystrtoul.c), base 10 */
	const char *s = text;
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	const char *p = s;
	char sign = *p;
	if (sign == '+' || sign == '-') {
		p++;
	}
	unsigned long result = 0;
	byte range = 0;
	for (; isdigit((unsigned char)*p); p++) {
		unsigned long c = (unsigned long)(*p - '0'), prev = result;
		result = result * 10 + c;
		if ((long)(result - c) / 10 != (long)prev) {
			range = 1;
		}
	}
	const char *end = p;
	if (range) {
		result = ~0UL;
	}
	/* a result that looks negative overflows, except the largest negative long (without the overflow of -v) */
	byte lowest = sign == '-' && result == (unsigned long)LONG_MAX + 1;
	long v;
	if (result > (unsigned long)LONG_MAX && !lowest) {
		range = 1;
		v = 0x7fffffff;
	} else {
		v = lowest ? LONG_MIN : (long)result;
	}
	if (sign == '-' && !lowest) {
		v = -v;
	}
	if (end == s || !isdigit((unsigned char)end[-1])) {
		return Native_Raise(NE_VALUE, "invalid literal for int(): %.200s", s);
	}
	while (*end && isspace((unsigned char)*end)) {
		end++;
	}
	if (*end) {
		return Native_Raise(NE_VALUE, "invalid literal for int(): %.200s", s);
	}
	if (range) {
		return Native_Raise(NE_VALUE, "int() literal too large: %.200s", s);
	}
	*out = v;
	return 1;
}

/* ---- the engine ---- */

byte Native_EntVar(cEntity *e, const char *ref, const char *name, nativeVal_t *out) {
	/* PyEntity_GetAttr: the values start at 0, the types in this order */
	int i = 0;
	double d = 0.0, x = 0.0, y = 0.0, z = 0.0;
	char *s = NULL;
	if (Entity_VarI(e, name, &i, 0)) {
		Native_SetInt(out, i);
	} else if (Entity_VarF(e, name, &d, 0)) {
		Native_SetFloat(out, d);
	} else if (Entity_VarV(e, name, &x, &y, &z, 0)) {
		out->type = NV_VEC;
		out->v[0] = x;
		out->v[1] = y;
		out->v[2] = z;
	} else if (Entity_VarS(e, name, &s, 0)) {
		if (s) {
			Native_SetStr(out, s);
		} else {
			Native_SetNone(out);
		}
	} else {
		/* no variable: the handle looks for a method of that name (none is a variable name of the ports) */
		Con_Errorf("'%s'->%s : Member invalid.\n", ref, name);
		return Native_Raise(NE_ATTRIBUTE, "%s", name);
	}
	return 1;
}

int Native_Seam(const char *module, const char *name, PyObject **hook) {
	PyObject *f = Script_Attr(module, name);
	if (!f) {
		return -1;
	}
	/* the engine's own: a C function of the module table (no self; the recorder of the test channel has one) */
	if (PyCFunction_Check(f) && !PyCFunction_GET_SELF(f) && !strcmp(((PyCFunctionObject *)f)->m_ml->ml_name, name)) {
		Py_DECREF(f);
		return 0;
	}
	*hook = f;
	return 1;
}

int Native_NetFlag(int i) {
	PyObject *hook;
	int s = Native_Seam("Scrap", "GetNetFlags", &hook);
	if (s == 0) {
		return (int)(g_pApp->flags >> i & 1);
	}
	if (s < 0) {
		return -1;
	}
	PyObject *flags = Script_Invoke(hook, PyTuple_New(0));
	Py_DECREF(hook);
	PyObject *f = flags ? PySequence_GetItem(flags, i) : NULL;
	Py_XDECREF(flags);
	int r = f ? PyObject_IsTrue(f) : -1;
	Py_XDECREF(f);
	return r;
}

/* ---- module state mirrored into the module dict ---- */

PyObject *Native_ToPy(const nativeVal_t *v) {
	switch (v->type) {
	case NV_INT:
		return PyInt_FromLong(v->i);
	case NV_FLOAT:
		return PyFloat_FromDouble(v->f);
	case NV_STR:
		return PyString_FromString((char *)v->s);
	case NV_VEC:
		return Py_BuildValue("(ddd)", v->v[0], v->v[1], v->v[2]);
	}
	return Py_ReturnNone();
}

byte Native_FromPy(PyObject *o, unsigned types, nativeVal_t *v) {
	if (o == Py_None && types & 1 << NV_NONE) {
		Native_SetNone(v);
	} else if (PyInt_Check(o) && types & 1 << NV_INT) {
		Native_SetInt(v, PyInt_AsLong(o));
	} else if (PyFloat_Check(o) && types & 1 << NV_FLOAT) {
		Native_SetFloat(v, PyFloat_AsDouble(o));
	} else if (PyString_Check(o) && types & 1 << NV_STR && PyString_Size(o) < NATIVE_TEXT &&
		strlen(PyString_AsString(o)) == (size_t)PyString_Size(o)) {
		Native_SetStr(v, PyString_AsString(o));
	} else if (PyTuple_Check(o) && PyTuple_Size(o) == 3 && types & 1 << NV_VEC &&
		PyArg_ParseTuple(o, "ddd", &v->v[0], &v->v[1], &v->v[2])) {
		v->type = NV_VEC;
	} else {
		PyErr_Clear();
		return 0;
	}
	return 1;
}

byte Native_Import(const char *module, const nativeVar_t *vars) {
	for (const nativeVar_t *var = vars; var->name; var++) {
		PyObject *o = Script_Attr(module, var->name);
		if (!o) {
			return 0;
		}
		byte ok = 1;
		if (!var->count) {
			ok = Native_FromPy(o, var->types, var->values);
		} else if (!PyList_Check(o) || PyList_Size(o) != var->count) {
			ok = 0;
		} else {
			for (int i = 0; ok && i < var->count; i++) {
				ok = Native_FromPy(PyList_GetItem(o, i), var->types, &var->values[i]);
			}
		}
		Py_DECREF(o);
		if (!ok) {
			/* a value only Python code can have put there (the C writes the forms it takes) */
			return Native_Raise(NE_TYPE, var->count ? "%s.%s: not a list of %d values the C module takes" :
				"%s.%s: not a value the C module takes", module, var->name, var->count);
		}
	}
	return 1;
}

byte Native_Put(const char *module, const nativeVar_t *var, int index) {
	const nativeVal_t *v = var->count ? &var->values[index] : var->values;
	PyObject *o = Native_ToPy(v);
	if (!o) {
		return 0;
	}
	int r;
	if (var->count) {
		PyObject *list = Script_Attr(module, var->name);
		r = list ? PySequence_SetItem(list, index, o) : -1;
		Py_XDECREF(list);
	} else {
		PyObject *m = PyImport_ImportModule((char *)module);
		r = m ? PyObject_SetAttrString(m, (char *)var->name, o) : -1;
		Py_XDECREF(m);
	}
	Py_DECREF(o);
	return r == 0;
}
