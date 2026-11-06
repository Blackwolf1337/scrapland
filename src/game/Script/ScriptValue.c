/*
 * Python values as the scripts use them, shared by the gameplay ports (include/scrap/script.h): the references of a
 * C function released together, module globals that raise NameError like the script, string and number operations
 * of the interpreter, the print statement, tuple unpacking and the for loop over a sequence.
 */
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
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
	PyObject *g = mod ? PyModule_GetDict(mod) : NULL, *v = NULL;
	/* the checks of builtin eval before it parses: a string without NUL, its leading blanks skipped */
	if (g && PyString_Check(text) && (int)strlen(PyString_AsString(text)) == PyString_Size(text)) {
		const char *s = PyString_AsString(text);
		while (*s == ' ' || *s == '\t') {
			s++;
		}
		v = Script_Literal(g, s);
	}
	if (g && !v) {
		v = Script_Ask("__builtin__", "eval", "(OO)", text, g);
	}
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

/*
 * Plain literals as eval makes them (Python 1.5.2: Parser/tokenizer.c, Python/compile.c parsenumber, parsestr,
 * com_addconst and the interning of PyCode_New; UNARY_NEGATIVE, BUILD_LIST and BUILD_TUPLE of ceval). Only the
 * tokens whose value cannot differ are taken; anything else (names, dicts, longs, exponents, octal or hex numbers,
 * escapes, raw or triple quoted strings, adjacent strings, newlines, comments, deep nesting) gives NULL and the
 * caller runs eval.
 */
#define LIT_NAME_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"
#define LIT_DEPTH 32

typedef struct {
	const char *p;
	PyObject *consts;	/* the constants so far, a list as com_add keeps them */
	PyObject *globals;	/* also the locals of eval */
	int depth;
} scriptLit_t;

static void Lit_Blanks(scriptLit_t *l) {
	while (*l->p == ' ' || *l->p == '\t') {
		l->p++;
	}
}

PyObject *Script_StrConst(const char *s, int len) {
	PyObject *v = PyString_FromStringAndSize((char *)s, len);
	if (v && (int)strspn(PyString_AsString(v), LIT_NAME_CHARS) == len) {
		PyString_InternInPlace(&v);
	}
	return v;
}

/* v (a new constant, given away) or the equal constant of the same type made before it */
static PyObject *Lit_Const(scriptLit_t *l, PyObject *v) {
	if (!v) {
		return NULL;
	}
	for (int i = PyList_Size(l->consts); --i >= 0;) {
		PyObject *w = PyList_GetItem(l->consts, i);
		if (v->ob_type == w->ob_type && PyObject_Compare(v, w) == 0) {
			Py_DECREF(v);
			return Script_Ref(w);
		}
	}
	if (PyErr_Occurred() || PyList_Append(l->consts, v) != 0) {
		Py_DECREF(v);
		return NULL;
	}
	return v;
}

/* the end of a token: a blank, a comma, a closing bracket or the end of the text */
static byte Lit_End(char c) {
	return c == '\0' || c == ' ' || c == '\t' || c == ',' || c == ')' || c == ']';
}

/* 0, 123, 0.5, 12. or 3.25 (an int as parsenumber reads it, a float through atof as it does) */
static PyObject *Lit_Number(scriptLit_t *l) {
	const char *s = l->p, *e = s;
	char buf[64];
	byte isFloat = 0;
	if (*e == '0') {
		e++;
	} else {
		while (*e >= '0' && *e <= '9') {
			e++;
		}
	}
	if (*e == '.') {
		isFloat = 1;
		for (e++; *e >= '0' && *e <= '9'; e++) {
		}
	}
	if (!Lit_End(*e) || e - s >= (int)sizeof(buf)) {
		return NULL;
	}
	memcpy(buf, s, e - s);
	buf[e - s] = '\0';
	l->p = e;
	if (isFloat) {
		return Lit_Const(l, PyFloat_FromDouble(atof(buf)));
	}
	char *end;
	errno = 0;
	long x = buf[0] == '0' ? (long)PyOS_strtoul(buf, &end, 0) : PyOS_strtol(buf, &end, 0);
	if (*end != '\0' || errno != 0) {
		return NULL;
	}
	return Lit_Const(l, PyInt_FromLong(x));
}

/* a character the tokenizer and parsestr keep as it is inside a string without escapes */
static byte Lit_Plain(unsigned char c) {
	return c >= 0x20 && c != 0x7f && c != '\\';
}

/* 'text' or "text" */
static PyObject *Lit_String(scriptLit_t *l) {
	char q = *l->p;
	const char *s = l->p + 1, *e = s;
	if (s[0] == q && s[1] == q) {
		return NULL;
	}
	while (*e != q) {
		if (!Lit_Plain((unsigned char)*e)) {
			return NULL;
		}
		e++;
	}
	if (!Lit_End(e[1])) {
		return NULL;
	}
	l->p = e + 1;
	return Lit_Const(l, Script_StrConst(s, (int)(e - s)));
}

static PyObject *Lit_Item(scriptLit_t *l);

/* the items up to close (')' or ']', '\0' at the top) into a new list: *commas tells whether a comma came */
static PyObject *Lit_Items(scriptLit_t *l, char close, byte *commas) {
	PyObject *list = PyList_New(0);
	*commas = 0;
	Lit_Blanks(l);
	while (list && *l->p != close) {
		PyObject *v = Lit_Item(l);
		if (!v || PyList_Append(list, v) != 0) {
			Py_XDECREF(v);
			Py_DECREF(list);
			return NULL;
		}
		Py_DECREF(v);
		Lit_Blanks(l);
		if (*l->p == ',') {
			*commas = 1;
			l->p++;
			Lit_Blanks(l);
		} else if (*l->p != close) {
			Py_DECREF(list);
			return NULL;
		}
	}
	return list;
}

/* the builtins of the frame of eval (a dict, the one of a module) */
static PyObject *Lit_BuiltinsDict(PyObject *globals) {
	PyObject *b = PyDict_GetItemString(globals, "__builtins__");
	return PyModule_Check(b) ? PyModule_GetDict(b) : b;
}

static PyObject *Lit_Item(scriptLit_t *l) {
	char c = *l->p;
	if (c == '\'' || c == '"') {
		return Lit_String(l);
	}
	if (!strncmp(l->p, "None", 4) && Lit_End(l->p[4])) {
		/* LOAD_NAME None: the globals (the locals of eval), the builtins; a NameError is left to eval */
		PyObject *v = PyDict_GetItemString(l->globals, "None");
		v = v ? v : PyDict_GetItemString(Lit_BuiltinsDict(l->globals), "None");
		l->p += 4;
		return Script_Ref(v);
	}
	if (c >= '0' && c <= '9') {
		return Lit_Number(l);
	}
	if (c == '-') {
		l->p++;
		Lit_Blanks(l);
		PyObject *n = *l->p >= '0' && *l->p <= '9' ? Lit_Number(l) : NULL;
		PyObject *v = n ? PyNumber_Negative(n) : NULL;
		Py_XDECREF(n);
		return v;
	}
	if ((c != '[' && c != '(') || l->depth >= LIT_DEPTH) {
		return NULL;
	}
	char close = c == '[' ? ']' : ')';
	byte commas;
	l->p++;
	l->depth++;
	PyObject *list = Lit_Items(l, close, &commas);
	l->depth--;
	if (!list) {
		return NULL;
	}
	l->p++;
	if (!Lit_End(*l->p)) {
		Py_DECREF(list);
		return NULL;
	}
	if (close == ']') {
		return list;
	}
	/* (x) is x, (x,) and () are tuples */
	PyObject *v = PyList_Size(list) == 1 && !commas ? Script_Ref(PyList_GetItem(list, 0)) : PyList_AsTuple(list);
	Py_DECREF(list);
	return v;
}

/* the globals give the frame of eval its builtins (else PyFrame_New raises) */
static byte Lit_Builtins(PyObject *globals) {
	PyObject *b = globals && PyDict_Check(globals) ? PyDict_GetItemString(globals, "__builtins__") : NULL;
	return b && (PyDict_Check(b) || PyModule_Check(b));
}

PyObject *Script_Literal(PyObject *globals, const char *s) {
	if (!s || !Lit_Builtins(globals) || *s == ' ' || *s == '\t' || *s == '\0') {
		return NULL;
	}
	scriptLit_t l = { .p = s, .consts = PyList_New(0), .globals = globals, .depth = 0 };
	byte commas;
	PyObject *list = l.consts ? Lit_Items(&l, '\0', &commas) : NULL, *v = NULL;
	if (list) {
		/* a, b is a tuple, a alone is a */
		v = PyList_Size(list) == 1 && !commas ? Script_Ref(PyList_GetItem(list, 0)) : PyList_AsTuple(list);
		Py_DECREF(list);
	}
	Py_XDECREF(l.consts);
	if (!v) {
		PyErr_Clear();
	}
	return v;
}

PyObject *Script_EvalMain(const char *text) {
	PyObject *main = PyImport_AddModule("__main__");
	PyObject *v = main ? Script_Literal(PyModule_GetDict(main), text) : NULL;
	PyErr_Clear();
	return v ? v : Script_Eval(text);
}

byte Script_IsName(const char *s, int len) {
	static const char *const keywords[] = { "and", "assert", "break", "class", "continue", "def", "del", "elif", "else",
		"except", "exec", "finally", "for", "from", "global", "if", "import", "in", "is", "lambda", "not", "or", "pass",
		"print", "raise", "return", "try", "while", NULL };
	if (len <= 0 || (s[0] >= '0' && s[0] <= '9') || (int)strspn(s, LIT_NAME_CHARS) < len) {
		return 0;
	}
	for (int i = 0; keywords[i]; i++) {
		if ((int)strlen(keywords[i]) == len && !strncmp(keywords[i], s, len)) {
			return 0;
		}
	}
	return 1;
}

byte Script_IsQuotable(PyObject *o) {
	if (!o || !PyString_Check(o)) {
		return 0;
	}
	const char *s = PyString_AsString(o);
	int n = PyString_Size(o);
	for (int i = 0; i < n; i++) {
		if (!Lit_Plain((unsigned char)s[i]) || s[i] == '\'') {
			return 0;
		}
	}
	return 1;
}

int Script_NameChain(PyObject *locals, PyObject *globals, const char *text, PyObject **out) {
	*out = NULL;
	if (!Lit_Builtins(globals)) {
		return 0;
	}
	for (const char *p = text;; p++) {
		int len = (int)strspn(p, LIT_NAME_CHARS);
		if (!Script_IsName(p, len) || (p[len] != '.' && p[len] != '\0')) {
			return 0;
		}
		p += len;
		if (!*p) {
			break;
		}
	}
	PyObject *b = Lit_BuiltinsDict(globals), *v = NULL;
	for (const char *p = text;; p++) {
		int len = (int)strspn(p, LIT_NAME_CHARS);
		char name[0x100];
		if (len >= (int)sizeof(name)) {
			Py_XDECREF(v);
			return 0;
		}
		memcpy(name, p, len);
		name[len] = '\0';
		if (!v) {
			/* LOAD_NAME: the locals, the globals, the builtins */
			PyObject *w = PyString_InternFromString(name);
			PyObject *x = !w ? NULL : locals && PyDict_GetItem(locals, w) ? PyDict_GetItem(locals, w) :
				PyDict_GetItem(globals, w) ? PyDict_GetItem(globals, w) : PyDict_GetItem(b, w);
			if (w && !x) {
				PyErr_SetObject(PyExc_NameError, w);
			}
			Py_XDECREF(w);
			v = Script_Ref(x);
		} else {
			PyObject *x = PyObject_GetAttrString(v, name);
			Py_DECREF(v);
			v = x;
		}
		if (!v) {
			return -1;
		}
		p += len;
		if (!*p) {
			break;
		}
	}
	*out = v;
	return 1;
}
