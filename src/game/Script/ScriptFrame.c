/*
 * A ported script function line by line (include/scrap/script.h, the Sf_ helpers). The frame keeps the temporary
 * references of the function and its error state: after the first error every helper does nothing and returns NULL
 * (or -1), as the script stops at the line that raised, and Sf_Done releases the references and returns NULL with the
 * error left set for the caller. The values keep their Python types and the operations are those of ceval (the
 * number and sequence rules of Python 1.5.2, PyObject_Compare for the comparisons, repr for the backquotes).
 */
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"

/* ---- shared with the older ports ---- */

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

/* ---- the frame ---- */

PyObject *Sf_Keep(scriptFrame_t *f, PyObject *o) {
	if (f->failed) {
		Py_XDECREF(o);
		return NULL;
	}
	if (!o) {
		if (!PyErr_Occurred()) {
			PyErr_SetString(PyExc_SystemError, "NULL result without error");
		}
		f->failed = 1;
		return NULL;
	}
	CHECK(f->n < SCRIPT_FRAME_REFS);
	f->refs[f->n++] = o;
	return o;
}

byte Sf_Check(scriptFrame_t *f, int ok) {
	if (!f->failed && !ok) {
		if (!PyErr_Occurred()) {
			PyErr_SetString(PyExc_SystemError, "error return without exception set");
		}
		f->failed = 1;
	}
	return !f->failed;
}

void Sf_Release(scriptFrame_t *f, int mark) {
	while (f->n > mark) {
		f->n--;
		Py_DECREF(f->refs[f->n]);
	}
}

PyObject *Sf_Done(scriptFrame_t *f, PyObject *result) {
	Sf_Release(f, 0);
	if (f->failed) {
		Py_XDECREF(result);
		return NULL;
	}
	return result;
}

PyObject *Sf_Return(scriptFrame_t *f, PyObject *o) {
	Py_XINCREF(o);
	return Sf_Done(f, f->failed ? NULL : o);
}

PyObject *Sf_None(scriptFrame_t *f) {
	return Sf_Done(f, f->failed ? NULL : Py_ReturnNone());
}

byte Sf_Raise(scriptFrame_t *f, PyObject *exc, const char *msg) {
	if (!f->failed) {
		PyErr_SetString(exc, (char *)msg);
		f->failed = 1;
	}
	return 0;
}

PyObject *Sf_Ref(scriptFrame_t *f, PyObject *o) {
	if (f->failed) {
		return NULL;
	}
	Py_XINCREF(o);
	return Sf_Keep(f, o);
}

PyObject *Sf_Int(scriptFrame_t *f, long i) {
	return f->failed ? NULL : Sf_Keep(f, PyInt_FromLong(i));
}

PyObject *Sf_Float(scriptFrame_t *f, double d) {
	return f->failed ? NULL : Sf_Keep(f, PyFloat_FromDouble(d));
}

PyObject *Sf_Str(scriptFrame_t *f, const char *s) {
	return f->failed ? NULL : Sf_Keep(f, PyString_FromString((char *)s));
}

PyObject *Sf_Build(scriptFrame_t *f, const char *fmt, ...) {
	if (f->failed) {
		return NULL;
	}
	va_list va;
	va_start(va, fmt);
	PyObject *v = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	return Sf_Keep(f, v);
}

/* ---- modules ---- */

PyObject *Sf_Import(scriptFrame_t *f, const char *name) {
	return f->failed ? NULL : Sf_Keep(f, PyImport_ImportModule((char *)name));
}

PyObject *Sf_Global(scriptFrame_t *f, const char *module, const char *name) {
	PyObject *m = Sf_Import(f, module);
	if (!m) {
		return NULL;
	}
	PyObject *v = PyDict_GetItemString(PyModule_GetDict(m), (char *)name);
	if (!v) {
		Sf_Raise(f, PyExc_NameError, name);
		return NULL;
	}
	return Sf_Ref(f, v);
}

byte Sf_SetGlobal(scriptFrame_t *f, const char *module, const char *name, PyObject *v) {
	PyObject *m = v ? Sf_Import(f, module) : NULL;
	return Sf_Check(f, m && PyDict_SetItemString(PyModule_GetDict(m), (char *)name, v) == 0);
}

byte Sf_DelGlobal(scriptFrame_t *f, const char *module, const char *name) {
	PyObject *m = Sf_Import(f, module);
	if (!m) {
		return 0;
	}
	if (PyDict_DelItemString(PyModule_GetDict(m), (char *)name) != 0) {
		PyErr_Clear();
		return Sf_Raise(f, PyExc_NameError, name);
	}
	return 1;
}

PyObject *Sf_Mod(scriptFrame_t *f, const char *module, const char *name) {
	PyObject *m = Sf_Import(f, module);
	return m ? Sf_Keep(f, PyObject_GetAttrString(m, (char *)name)) : NULL;
}

byte Sf_SetMod(scriptFrame_t *f, const char *module, const char *name, PyObject *v) {
	PyObject *m = v ? Sf_Import(f, module) : NULL;
	return Sf_Check(f, m && PyObject_SetAttrString(m, (char *)name, v) == 0);
}

/* ---- objects ---- */

PyObject *Sf_Attr(scriptFrame_t *f, PyObject *o, const char *name) {
	return f->failed || !o ? NULL : Sf_Keep(f, PyObject_GetAttrString(o, (char *)name));
}

byte Sf_SetAttr(scriptFrame_t *f, PyObject *o, const char *name, PyObject *v) {
	return Sf_Check(f, o && v && PyObject_SetAttrString(o, (char *)name, v) == 0);
}

byte Sf_SetAttrI(scriptFrame_t *f, PyObject *o, const char *name, long i) {
	return Sf_SetAttr(f, o, name, Sf_Int(f, i));
}

byte Sf_SetAttrS(scriptFrame_t *f, PyObject *o, const char *name, const char *s) {
	return Sf_SetAttr(f, o, name, Sf_Str(f, s));
}

PyObject *Sf_Item(scriptFrame_t *f, PyObject *o, long i) {
	return Sf_GetItem(f, o, Sf_Int(f, i));
}

PyObject *Sf_GetItem(scriptFrame_t *f, PyObject *o, PyObject *key) {
	return f->failed || !o || !key ? NULL : Sf_Keep(f, PyObject_GetItem(o, key));
}

byte Sf_SetItem(scriptFrame_t *f, PyObject *o, PyObject *key, PyObject *v) {
	return Sf_Check(f, o && key && v && PyObject_SetItem(o, key, v) == 0);
}

byte Sf_DelItem(scriptFrame_t *f, PyObject *o, PyObject *key) {
	return Sf_Check(f, o && key && PyObject_DelItem(o, key) == 0);
}

PyObject *Sf_Slice(scriptFrame_t *f, PyObject *o, int lo, int hi) {
	return f->failed || !o ? NULL : Sf_Keep(f, PySequence_GetSlice(o, lo, hi));
}

PyObject *Sf_Op(scriptFrame_t *f, PyObject *a, char op, PyObject *b) {
	if (f->failed || !a || !b) {
		return NULL;
	}
	switch (op) {
	case '+':
		return Sf_Keep(f, PyNumber_Add(a, b));
	case '-':
		return Sf_Keep(f, PyNumber_Subtract(a, b));
	case '*':
		return Sf_Keep(f, PyNumber_Multiply(a, b));
	case '/':
		return Sf_Keep(f, PyNumber_Divide(a, b));
	case '%':
		return Sf_Keep(f, PyNumber_Remainder(a, b));
	}
	Sf_Raise(f, PyExc_SystemError, "unknown operator");
	return NULL;
}

PyObject *Sf_OpI(scriptFrame_t *f, PyObject *a, char op, long b) {
	return Sf_Op(f, a, op, Sf_Int(f, b));
}

PyObject *Sf_Cat(scriptFrame_t *f, const char *fmt, ...) {
	va_list va;
	PyObject *r = NULL;
	va_start(va, fmt);
	for (const char *p = fmt; *p && !f->failed; p++) {
		PyObject *v = *p == 's' ? Sf_Str(f, va_arg(va, const char *)) : va_arg(va, PyObject *);
		r = r ? Sf_Op(f, r, '+', v) : v;
	}
	va_end(va);
	return f->failed ? NULL : r;
}

PyObject *Sf_Repr(scriptFrame_t *f, PyObject *o) {
	return f->failed || !o ? NULL : Sf_Keep(f, PyObject_Repr(o));
}

PyObject *Sf_ReprI(scriptFrame_t *f, long i) {
	return Sf_Repr(f, Sf_Int(f, i));
}

/* ---- conditions ---- */

int Sf_True(scriptFrame_t *f, PyObject *o) {
	if (f->failed || !o) {
		return -1;
	}
	int t = PyObject_IsTrue(o);
	return Sf_Check(f, t >= 0) ? t : -1;
}

int Sf_Compare(scriptFrame_t *f, PyObject *a, const char *op, PyObject *b) {
	if (f->failed || !a || !b) {
		return -1;
	}
	int c = PyObject_Compare(a, b);
	if (c && PyErr_Occurred()) {
		f->failed = 1;
		return -1;
	}
	switch (op[0]) {
	case '=':
		return c == 0;
	case '!':
		return c != 0;
	case '<':
		return op[1] == '=' ? c <= 0 : c < 0;
	case '>':
		return op[1] == '=' ? c >= 0 : c > 0;
	}
	return Sf_Raise(f, PyExc_SystemError, "unknown comparison") ? 0 : -1;
}

int Sf_EqS(scriptFrame_t *f, PyObject *a, const char *s) {
	return Sf_Compare(f, a, "==", Sf_Str(f, s));
}

int Sf_EqI(scriptFrame_t *f, PyObject *a, long i) {
	return Sf_Compare(f, a, "==", Sf_Int(f, i));
}

int Sf_In(scriptFrame_t *f, PyObject *x, PyObject *seq) {
	if (f->failed || !x || !seq) {
		return -1;
	}
	int r = PySequence_Contains(seq, x);
	return Sf_Check(f, r >= 0) ? r : -1;
}

int Sf_InS(scriptFrame_t *f, PyObject *x, const char *const *texts) {
	for (; *texts; texts++) {
		int r = Sf_EqS(f, x, *texts);
		if (r != 0) {
			return r;
		}
	}
	return f->failed ? -1 : 0;
}

int Sf_Len(scriptFrame_t *f, PyObject *o) {
	if (f->failed || !o) {
		return -1;
	}
	int n = PyObject_Length(o);
	return Sf_Check(f, n >= 0) ? n : -1;
}

PyObject *Sf_Next(scriptFrame_t *f, PyObject *seq, int i) {
	if (f->failed || !seq) {
		return NULL;
	}
	PySequenceMethods *sq = seq->ob_type->tp_as_sequence;
	if (!sq || !sq->sq_item) {
		Sf_Raise(f, PyExc_TypeError, "loop over non-sequence");
		return NULL;
	}
	PyObject *v = sq->sq_item(seq, i);
	if (!v && PyErr_Occurred() && PyErr_ExceptionMatches(PyExc_IndexError)) {
		PyErr_Clear();
		return NULL;
	}
	return Sf_Keep(f, v);
}

/* ---- calls ---- */

PyObject *Sf_CallArgs(scriptFrame_t *f, PyObject *callable, PyObject *args) {
	if (f->failed || !callable || !args) {
		Py_XDECREF(args);
		return NULL;
	}
	return Sf_Keep(f, Script_Invoke(callable, args));
}

PyObject *Sf_CallKw(scriptFrame_t *f, PyObject *callable, PyObject *args, PyObject *kw) {
	if (f->failed || !callable || !args || !kw) {
		Py_XDECREF(args);
		return NULL;
	}
	PyObject *r = PyEval_CallObjectWithKeywords(callable, args, kw);
	Py_DECREF(args);
	return Sf_Keep(f, r);
}

PyObject *Sf_Call(scriptFrame_t *f, PyObject *callable, const char *fmt, ...) {
	if (f->failed || !callable) {
		return NULL;
	}
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	return Sf_CallArgs(f, callable, Sf_Check(f, args != NULL) ? args : NULL);
}

PyObject *Sf_Ask(scriptFrame_t *f, const char *module, const char *func, const char *fmt, ...) {
	PyObject *fn = Sf_Mod(f, module, func);
	if (!fn) {
		return NULL;
	}
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	return Sf_CallArgs(f, fn, Sf_Check(f, args != NULL) ? args : NULL);
}

PyObject *Sf_Method(scriptFrame_t *f, PyObject *o, const char *name, const char *fmt, ...) {
	PyObject *fn = Sf_Attr(f, o, name);
	if (!fn) {
		return NULL;
	}
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	return Sf_CallArgs(f, fn, Sf_Check(f, args != NULL) ? args : NULL);
}

PyObject *Sf_Entity(scriptFrame_t *f, PyObject *name) {
	return name ? Sf_Ask(f, "Scrap", "GetEntity", "(O)", name) : NULL;
}

PyObject *Sf_Lang(scriptFrame_t *f, PyObject *key) {
	return key ? Sf_Ask(f, "Scrap", "GetLangStr", "(O)", key) : NULL;
}

PyObject *Sf_Time(scriptFrame_t *f) {
	return Sf_Ask(f, "Scrap", "GetTime", "()");
}

byte Sf_Schedule(scriptFrame_t *f, PyObject *time, PyObject *func, PyObject *args, const char *name) {
	if (!time || !func || !args) {
		return Sf_Check(f, 0);
	}
	return Sf_Check(f, (name ? Sf_Ask(f, "Scrap", "AddScheduledFunc", "(OOOs)", time, func, args, name) :
		Sf_Ask(f, "Scrap", "AddScheduledFunc", "(OOO)", time, func, args)) != NULL);
}

byte Sf_Later(scriptFrame_t *f, PyObject *delay, PyObject *func, PyObject *args, const char *name) {
	return Sf_Schedule(f, Sf_Op(f, Sf_Time(f), '+', delay), func, args, name);
}

byte Sf_Print(scriptFrame_t *f, const char *fmt, ...) {
	PyObject *items[16];
	int n = 0;
	va_list va;
	va_start(va, fmt);
	for (const char *p = fmt; *p && n < 16; p++) {
		items[n++] = *p == 's' ? Sf_Str(f, va_arg(va, const char *)) : va_arg(va, PyObject *);
	}
	va_end(va);
	for (int i = 0; i < n; i++) {
		if (!items[i]) {
			return Sf_Check(f, 0);
		}
	}
	return Sf_Check(f, !f->failed && Script_Print(items, n));
}

byte Sf_Unpack(scriptFrame_t *f, PyObject *v, int n, PyObject **out) {
	if (f->failed || !v || !Sf_Check(f, Script_Unpack(v, n, out))) {
		return 0;
	}
	for (int i = 0; i < n; i++) {
		Sf_Keep(f, out[i]);
	}
	return 1;
}

byte Sf_Exec(scriptFrame_t *f, PyObject *code, const char *module, PyObject *locals) {
	PyObject *m = code && locals ? Sf_Import(f, module) : NULL;
	if (!m) {
		return Sf_Check(f, 0);
	}
	if (!PyString_Check(code)) {
		return Sf_Raise(f, PyExc_TypeError, "exec 1st arg must be string, code or file object");
	}
	const char *s = PyString_AsString(code);
	if ((int)strlen(s) != PyString_Size(code)) {
		return Sf_Raise(f, PyExc_ValueError, "embedded '\\0' in exec string");
	}
	return Sf_Keep(f, PyRun_String((char *)s, Py_file_input, PyModule_GetDict(m), locals)) != NULL;
}

PyObject *Sf_Eval(scriptFrame_t *f, PyObject *text, const char *module) {
	PyObject *m = text ? Sf_Import(f, module) : NULL;
	if (!m) {
		return NULL;
	}
	if (!PyString_Check(text)) {
		Sf_Raise(f, PyExc_TypeError, "eval() argument 1 must be string or code object");
		return NULL;
	}
	const char *s = PyString_AsString(text);
	if ((int)strlen(s) != PyString_Size(text)) {
		Sf_Raise(f, PyExc_ValueError, "embedded '\\0' in string arg");
		return NULL;
	}
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	PyObject *g = PyModule_GetDict(m);
	return Sf_Keep(f, PyRun_String((char *)s, Py_eval_input, g, g));
}
