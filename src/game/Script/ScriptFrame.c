/*
 * A ported script function line by line (include/scrap/script.h, the Sf_ helpers). The frame keeps the temporary
 * references of the function and its error state: after the first error every helper does nothing and returns NULL
 * (or -1), as the script stops at the line that raised, and Sf_Done releases the references and returns NULL with the
 * error left set for the caller. The values keep their Python types and the operations are those of ceval (the
 * number and sequence rules of Python 1.5.2, PyObject_Compare for the comparisons, repr for the backquotes). Where a
 * value helper of ScriptValue.c does the operation, the frame helper only keeps its result.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"

/* ---- the frame ---- */

PyObject *Sf_Keep(scriptFrame_t *f, PyObject *o) {
	if (f->failed) {
		Py_XDECREF(o);
		return NULL;
	}
	if (!o) {
		if (!PyErr_Occurred()) {
			/* an engine call that fails without an exception (an entity handle of a deleted entity): ceval's
			 * SystemError, as the line of the script raises it */
			Script_CheckSilent("Sf_Keep");
			PyErr_SetString(PyExc_SystemError, "error return without exception set");
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
			Script_CheckSilent("Sf_Check");
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

void Sf_ReleaseKeep(scriptFrame_t *f, int mark, PyObject *const *keep, int n) {
	for (int i = 0; i < n; i++) {
		Py_XINCREF(keep[i]);
	}
	Sf_Release(f, mark);
	for (int i = 0; i < n; i++) {
		if (keep[i]) {
			CHECK(f->n < SCRIPT_FRAME_REFS);
			f->refs[f->n++] = keep[i];
		}
	}
}

byte Sf_Except(scriptFrame_t *f) {
	if (!f->failed) {
		return 0;
	}
	PyErr_Clear();
	f->failed = 0;
	return 1;
}

PyObject *Sf_Done(scriptFrame_t *f, PyObject *result) {
	Sf_Release(f, 0);
	if (f->failed && !PyErr_Occurred()) {
		Script_CheckSilent("Sf_Done");
	}
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

byte Sf_RaiseText(scriptFrame_t *f, const char *text) {
	if (!f->failed) {
		PyObject *s = PyString_FromString((char *)text);
		if (s) {
			PyErr_SetObject(s, Py_None);
			Py_DECREF(s);
		}
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

/* the module object (a new reference) for the dict of its globals */
static PyObject *Sf_Module(scriptFrame_t *f, const char *name) {
	return f->failed ? NULL : Sf_Keep(f, PyImport_ImportModule((char *)name));
}

byte Sf_Import(scriptFrame_t *f, const char *name) {
	const char *names[2] = { name, NULL };
	return Sf_Check(f, f->failed || Script_Import(names));
}

PyObject *Sf_Global(scriptFrame_t *f, const char *module, const char *name) {
	return f->failed ? NULL : Sf_Keep(f, Script_Global(module, name));
}

byte Sf_SetGlobal(scriptFrame_t *f, const char *module, const char *name, PyObject *v) {
	return Sf_Check(f, !f->failed && v && Script_SetGlobal(module, name, Script_Ref(v)));
}

byte Sf_DelGlobal(scriptFrame_t *f, const char *module, const char *name) {
	PyObject *m = Sf_Module(f, module);
	if (!m) {
		return 0;
	}
	if (PyDict_DelItemString(PyModule_GetDict(m), (char *)name) != 0) {
		PyErr_Clear();
		return Sf_Raise(f, PyExc_NameError, name);
	}
	return 1;
}

PyObject *Sf_From(scriptFrame_t *f, const char *module, const char *name) {
	PyObject *m = Sf_Module(f, module);
	if (!m) {
		return NULL;
	}
	PyObject *v = PyDict_GetItemString(PyModule_GetDict(m), (char *)name);
	if (!v) {
		PyErr_Format(PyExc_ImportError, "cannot import name %.230s", name);
		f->failed = 1;
		return NULL;
	}
	return Sf_Ref(f, v);
}

PyObject *Sf_Mod(scriptFrame_t *f, const char *module, const char *name) {
	return f->failed ? NULL : Sf_Keep(f, Script_Attr(module, name));
}

byte Sf_SetMod(scriptFrame_t *f, const char *module, const char *name, PyObject *v) {
	PyObject *m = v ? Sf_Module(f, module) : NULL;
	return Sf_Check(f, m && PyObject_SetAttrString(m, (char *)name, v) == 0);
}

/* ---- objects ---- */

PyObject *Sf_Attr(scriptFrame_t *f, PyObject *o, const char *name) {
	return f->failed || !o ? NULL : Sf_Keep(f, PyObject_GetAttrString(o, (char *)name));
}

byte Sf_SetAttr(scriptFrame_t *f, PyObject *o, const char *name, PyObject *v) {
	return Sf_Check(f, !f->failed && o && v && PyObject_SetAttrString(o, (char *)name, v) == 0);
}

byte Sf_SetAttrI(scriptFrame_t *f, PyObject *o, const char *name, long i) {
	return Sf_SetAttr(f, o, name, Sf_Int(f, i));
}

byte Sf_SetAttrS(scriptFrame_t *f, PyObject *o, const char *name, const char *s) {
	return Sf_SetAttr(f, o, name, Sf_Str(f, s));
}

PyObject *Sf_Item(scriptFrame_t *f, PyObject *o, long i) {
	return f->failed || !o ? NULL : Sf_Keep(f, Script_Index(o, (int)i));
}

PyObject *Sf_GetItem(scriptFrame_t *f, PyObject *o, PyObject *key) {
	return f->failed || !o || !key ? NULL : Sf_Keep(f, PyObject_GetItem(o, key));
}

byte Sf_SetItem(scriptFrame_t *f, PyObject *o, PyObject *key, PyObject *v) {
	return Sf_Check(f, !f->failed && o && key && v && PyObject_SetItem(o, key, v) == 0);
}

byte Sf_DelItem(scriptFrame_t *f, PyObject *o, PyObject *key) {
	return Sf_Check(f, !f->failed && o && key && PyObject_DelItem(o, key) == 0);
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
		return Sf_Keep(f, Script_Plus(Script_Ref(a), b));
	case '-':
		return Sf_Keep(f, PyNumber_Subtract(a, b));
	case '*':
		return Sf_Keep(f, PyNumber_Multiply(a, b));
	case '/':
		return Sf_Keep(f, PyNumber_Divide(a, b));
	case '%':
		return Sf_Keep(f, Script_Mod(a, b));
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
	return f->failed || !o ? NULL : Sf_Keep(f, Script_Repr(o));
}

PyObject *Sf_ToInt(scriptFrame_t *f, PyObject *o) {
	return f->failed || !o ? NULL : Sf_Keep(f, Script_Int(o));
}

PyObject *Sf_ToFloat(scriptFrame_t *f, PyObject *o) {
	return f->failed || !o ? NULL : Sf_Keep(f, PyNumber_Float(o));
}

PyObject *Sf_ReprI(scriptFrame_t *f, long i) {
	return Sf_Repr(f, Sf_Int(f, i));
}

/* ---- conditions ---- */

int Sf_True(scriptFrame_t *f, PyObject *o) {
	if (f->failed || !o) {
		return -1;
	}
	int t = Script_True(o);
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
	PyObject *item;
	if (f->failed || !seq) {
		return NULL;
	}
	int r = Script_Next(seq, i, &item);
	return r > 0 ? Sf_Keep(f, item) : r < 0 ? Sf_Keep(f, NULL) : NULL;
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
	PyObject *r = Script_CallObject(callable, args, kw);
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

PyObject *Sf_Own(scriptFrame_t *f, const char *module, const char *func, const char *fmt, ...) {
	PyObject *fn = Sf_Global(f, module, func);
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
	return f->failed || !name ? NULL : Sf_Keep(f, Script_Entity(name));
}

PyObject *Sf_Lang(scriptFrame_t *f, PyObject *key) {
	return f->failed || !key ? NULL : Sf_Keep(f, Script_LangOf(Script_Ref(key)));
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

/*
 * The lines the game execs (scene generic lines, the imports and calls the ports build, the code strings of the
 * save games), read in C: statements split by ';' (one may end the line), each "import NAME", a chain NAME(.NAME |
 * (args))* or such a chain ending in .NAME set to a value ("=" with blanks or not). Arguments and values are
 * literals (Script_Literal) or two literals joined by * or / (1/2.0). A line in another form (keywords, newlines,
 * comments, a leading blank, other expressions) is not taken and nothing of it runs, as exec compiles the whole text
 * first.
 */
#define SF_LINE_STMTS 8
#define SF_LINE_TRAILERS 8
#define SF_LINE_NAME 0x40

typedef struct {
	char kind;		/* '.' an attribute, '(' a call */
	const char *text;	/* the attribute name, or the arguments up to the closing bracket */
	int len;
} sfTrailer_t;

typedef struct {
	byte import;		/* import <name> */
	char name[SF_LINE_NAME];	/* the module, or the first name of the chain */
	sfTrailer_t t[SF_LINE_TRAILERS];
	int n;
	const char *value;	/* the value of an assignment to the last attribute (NULL: none) */
	int valueLen;
} sfStmt_t;

static void Sf_LineBlanks(const char **p) {
	while (**p == ' ' || **p == '\t') {
		(*p)++;
	}
}

/* the length of the NAME at p (0 when there is none or it is a keyword) */
static int Sf_LineName(const char *p) {
	int len = 0;
	while (isalnum((unsigned char)p[len]) || p[len] == '_') {
		len++;
	}
	return len < SF_LINE_NAME && Script_IsName(p, len) ? len : 0;
}

/* the end of a value at p: the first ',', ';' or unmatched closing bracket outside quotes and brackets, or the end
 * of the text; NULL when a quote does not close */
static const char *Sf_ValueEnd(const char *p) {
	int depth = 0;
	for (; *p; p++) {
		if (*p == '\'' || *p == '"') {
			char q = *p;
			for (p++; *p && *p != q; p++) {
				if (*p == '\\' && p[1]) {
					p++;
				}
			}
			if (!*p) {
				return NULL;
			}
		} else if (*p == '(' || *p == '[' || *p == '{') {
			depth++;
		} else if (*p == ')' || *p == ']' || *p == '}') {
			if (depth-- == 0) {
				return p;
			}
		} else if (depth == 0 && (*p == ',' || *p == ';')) {
			return p;
		}
	}
	return p;
}

/* a value s[0:len] in globals: a literal, or two literals joined by * or / (BINARY_MULTIPLY, BINARY_DIVIDE, done only
 * when run is set). A new reference; NULL without an error for another form, with the error of the operation */
static PyObject *Sf_LineValue(PyObject *globals, const char *s, int len, byte run) {
	char buf[0x200], *op = NULL;
	Sf_LineBlanks(&s);
	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
		len--;
	}
	if (len <= 0 || len >= (int)sizeof(buf)) {
		return NULL;
	}
	memcpy(buf, s, len);
	buf[len] = '\0';
	const char *end = Sf_ValueEnd(buf);
	if (!end || *end) {
		return NULL;
	}
	int depth = 0;
	for (char *p = buf; *p; p++) {
		if (*p == '\'' || *p == '"') {
			char q = *p;
			for (p++; *p != q; p++) {
				if (*p == '\\') {
					p++;
				}
			}
		} else if (*p == '(' || *p == '[' || *p == '{') {
			depth++;
		} else if (*p == ')' || *p == ']' || *p == '}') {
			depth--;
		} else if (depth == 0 && (*p == '*' || *p == '/')) {
			if (op) {
				return NULL;
			}
			op = p;
		}
	}
	if (!op) {
		return Script_Literal(globals, buf);
	}
	char o = *op;
	const char *right = op + 1;
	for (*op = '\0'; op > buf && (op[-1] == ' ' || op[-1] == '\t'); op--) {
		op[-1] = '\0';
	}
	Sf_LineBlanks(&right);
	PyObject *a = Script_Literal(globals, buf), *b = a ? Script_Literal(globals, right) : NULL;
	PyObject *v = !b ? NULL : !run ? Script_Ref(b) : o == '*' ? PyNumber_Multiply(a, b) : PyNumber_Divide(a, b);
	Py_XDECREF(a);
	Py_XDECREF(b);
	return v;
}

/* one statement at *p into st; 1 with *p after it (at ';' or the end), 0 when it has another form */
static byte Sf_LineStmt(PyObject *globals, const char **pp, sfStmt_t *st) {
	const char *p = *pp;
	int len;
	memset(st, 0, sizeof(*st));
	if (!strncmp(p, "import", 6) && (p[6] == ' ' || p[6] == '\t')) {
		p += 6;
		Sf_LineBlanks(&p);
		st->import = 1;
	}
	if ((len = Sf_LineName(p)) == 0) {
		return 0;
	}
	memcpy(st->name, p, len);
	p += len;
	while (!st->import) {
		const char *q = p;
		Sf_LineBlanks(&q);
		if (*q != '.' && *q != '(') {
			break;
		}
		if (st->n >= SF_LINE_TRAILERS) {
			return 0;
		}
		sfTrailer_t *t = &st->t[st->n++];
		t->kind = *q++;
		Sf_LineBlanks(&q);
		t->text = q;
		if (t->kind == '.') {
			if ((t->len = Sf_LineName(q)) == 0) {
				return 0;
			}
			p = q + t->len;
			continue;
		}
		while (*q != ')') {
			const char *e = Sf_ValueEnd(q);
			PyObject *v = e && (*e == ',' || *e == ')') ? Sf_LineValue(globals, q, (int)(e - q), 0) : NULL;
			if (!v) {
				return 0;
			}
			Py_DECREF(v);
			q = *e == ',' ? e + 1 : e;
			Sf_LineBlanks(&q);
		}
		t->len = (int)(q - t->text);
		p = q + 1;
	}
	Sf_LineBlanks(&p);
	if (!st->import && *p == '=' && p[1] != '=') {
		/* only an attribute is set (a name would go into the locals) */
		if (st->n == 0 || st->t[st->n - 1].kind != '.') {
			return 0;
		}
		p++;
		Sf_LineBlanks(&p);
		const char *e = Sf_ValueEnd(p);
		PyObject *v = e && (*e == ';' || *e == '\0') ? Sf_LineValue(globals, p, (int)(e - p), 0) : NULL;
		if (!v) {
			return 0;
		}
		Py_DECREF(v);
		st->value = p;
		st->valueLen = (int)(e - p);
		p = e;
	}
	if (*p && *p != ';') {
		return 0;
	}
	*pp = p;
	return 1;
}

/* the arguments of a call trailer as a tuple (each value made in order, as the call pushes them) */
static PyObject *Sf_LineArgs(PyObject *globals, const sfTrailer_t *t) {
	PyObject *list = PyList_New(0);
	for (const char *q = t->text; list && q < t->text + t->len;) {
		const char *e = Sf_ValueEnd(q);
		PyObject *v = Sf_LineValue(globals, q, (int)(e - q), 1);
		if (!v || PyList_Append(list, v) != 0) {
			Py_DECREF(list);
			list = NULL;
		}
		Py_XDECREF(v);
		q = *e == ',' ? e + 1 : e;
		Sf_LineBlanks(&q);
	}
	PyObject *args = list ? PyList_AsTuple(list) : NULL;
	Py_XDECREF(list);
	return args;
}

/* a statement as exec runs it in globals and locals: LOAD_NAME through the locals, the globals and the builtins; an
 * import stored in the locals; the value of an assignment made before its target; the result of a call dropped */
static void Sf_LineRun(scriptFrame_t *f, const char *module, const sfStmt_t *st, PyObject *globals, PyObject *locals) {
	if (st->import) {
		Sf_ImportTo(f, module, locals, Sf_Keep(f, PyString_InternFromString((char *)st->name)));
		return;
	}
	PyObject *value = st->value ? Sf_LineValue(globals, st->value, st->valueLen, 1) : NULL, *v;
	if ((st->value && !Sf_Keep(f, value)) || !Sf_Check(f, Script_NameChain(locals, globals, st->name, &v) > 0)) {
		return;
	}
	Sf_Keep(f, v);
	int last = st->value ? st->n - 1 : st->n;
	for (int i = 0; i < last && !f->failed; i++) {
		const sfTrailer_t *t = &st->t[i];
		char name[SF_LINE_NAME];
		if (t->kind == '.') {
			Str_Copy(name, t->text, t->len + 1);
			v = Sf_Attr(f, v, name);
		} else if (i == last - 1 && !st->value) {
			PyObject *args = Sf_LineArgs(globals, t), *r = args ? Script_Invoke(v, args) : NULL;
			Py_XDECREF(r);
			Sf_Check(f, r != NULL);
		} else {
			v = Sf_CallArgs(f, v, Sf_LineArgs(globals, t));
			Sf_Check(f, v != NULL);
		}
	}
	if (st->value && !f->failed) {
		char name[SF_LINE_NAME];
		Str_Copy(name, st->t[last].text, st->t[last].len + 1);
		Sf_SetAttr(f, v, name, value);
	}
}

byte Sf_Exec(scriptFrame_t *f, PyObject *code, const char *module, PyObject *locals) {
	PyObject *m = code && locals ? Sf_Module(f, module) : NULL;
	if (!m) {
		return Sf_Check(f, 0);
	}
	if (!PyString_Check(code)) {
		return Sf_Raise(f, PyExc_TypeError, "exec 1st arg must be string, code or file object");
	}
	const char *s = PyString_AsString(code), *p = s;
	if ((int)strlen(s) != PyString_Size(code)) {
		return Sf_Raise(f, PyExc_ValueError, "embedded '\\0' in exec string");
	}
	PyObject *g = PyModule_GetDict(m);
	sfStmt_t st[SF_LINE_STMTS];
	int n = 0;
	/* a line that starts with a blank is exec's SyntaxError (also when it has nothing else); "" is nothing */
	byte known = Script_HasBuiltins(g) && *p != ' ' && *p != '\t';
	while (known && *p) {
		known = n < SF_LINE_STMTS && Sf_LineStmt(g, &p, &st[n++]);
		if (known && *p == ';') {
			p++;
			Sf_LineBlanks(&p);
		}
	}
	if (known) {
		for (int i = 0; i < n && !f->failed; i++) {
			Sf_LineRun(f, module, &st[i], g, locals);
		}
		return !f->failed;
	}
	/* STAGE 7: a line in another form (the console, a hand-edited save, a mod) becomes Script_NotRead's error when the
	 * interpreter goes */
	return Sf_Keep(f, PyRun_String((char *)s, Py_file_input, g, locals)) != NULL;
}

PyObject *Sf_Eval(scriptFrame_t *f, PyObject *text, const char *module) {
	PyObject *m = text ? Sf_Module(f, module) : NULL;
	return m ? Sf_Keep(f, Script_EvalValue(PyModule_GetDict(m), text)) : NULL;
}

byte Sf_RaiseValue(scriptFrame_t *f, PyObject *o) {
	if (f->failed || !o) {
		return Sf_Check(f, 0);
	}
	/* do_raise of ceval with the value and the traceback left out */
	PyObject *type = Script_Ref(o), *value = Script_Ref(Py_None), *tb = NULL;
	while (PyTuple_Check(type) && PyTuple_Size(type) > 0) {
		PyObject *tmp = type;
		type = Script_Ref(PyTuple_GET_ITEM(type, 0));
		Py_DECREF(tmp);
	}
	if (PyClass_Check(type)) {
		PyErr_NormalizeException(&type, &value, &tb);
	} else if (PyInstance_Check(type)) {
		Py_DECREF(value);
		value = type;
		type = Script_Ref((PyObject *)((PyInstanceObject *)type)->in_class);
	} else if (!PyString_Check(type)) {
		Py_DECREF(type);
		Py_DECREF(value);
		return Sf_Raise(f, PyExc_TypeError, "exceptions must be strings, classes, or instances");
	}
	PyErr_Restore(type, value, tb);
	f->failed = 1;
	return 0;
}

byte Sf_ImportTo(scriptFrame_t *f, const char *module, PyObject *locals, PyObject *name) {
	if (f->failed || !name || !locals || !PyString_Check(name) ||
		!Script_IsName(PyString_AsString(name), PyString_Size(name))) {
		return Sf_Exec(f, Sf_Cat(f, "sO", "import ", name), module, locals);
	}
	/* the exec'd line imports in the module's globals, where a name without a dot is a top level module */
	PyObject *m = Sf_Module(f, module) ? Sf_Keep(f, PyImport_ImportModule(PyString_AsString(name))) : NULL;
	return Sf_Check(f, m && PyDict_SetItem(locals, name, m) == 0);
}

byte Sf_ImportCall(scriptFrame_t *f, const char *module, PyObject *locals, const char *target, PyObject *func) {
	if (f->failed || !func || !locals || !PyString_Check(func) ||
		!Script_IsName(PyString_AsString(func), PyString_Size(func))) {
		return Sf_Exec(f, Sf_Cat(f, "sssssOs", "import ", target, ";", target, ".", func, "()"), module, locals);
	}
	PyObject *name = Sf_Str(f, target);
	if (!Sf_ImportTo(f, module, locals, name)) {
		return 0;
	}
	return Sf_Line(f, Sf_Attr(f, Sf_Keep(f, Script_Ref(PyDict_GetItem(locals, name))), PyString_AsString(func)), "()");
}

byte Sf_Line(scriptFrame_t *f, PyObject *callable, const char *fmt, ...) {
	if (f->failed || !callable) {
		return Sf_Check(f, 0);
	}
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	PyObject *r = args ? Script_Invoke(callable, args) : NULL;
	Py_XDECREF(r);
	return Sf_Check(f, r != NULL);
}

/* ---- the scorer of the menus ---- */

const char *Sf_Text(scriptFrame_t *f, PyObject *o) {
	if (f->failed || !o) {
		return NULL;
	}
	if (!PyString_Check(o)) {
		Sf_Raise(f, PyExc_TypeError, "argument 2: expected string found");
		return NULL;
	}
	return PyString_AsString(o);
}

byte Sf_ScorerSet(scriptFrame_t *f, int id, const char *item, const char *var, PyObject *v) {
	if (f->failed || !item || !v) {
		return Sf_Check(f, 0);
	}
	if (PyString_Check(v)) {
		Script_SetStr(id, item, var, PyString_AsString(v));
	} else if (PyInt_Check(v)) {
		Script_SetNum(id, item, var, (double)PyInt_AsLong(v));
	} else if (PyFloat_Check(v)) {
		Script_SetNum(id, item, var, PyFloat_AsDouble(v));
	} else {
		int mark = f->n;
		Sf_Ask(f, "SScorer", "Set", "(issO)", id, item, var, v);
		Sf_Release(f, mark);
	}
	return !f->failed;
}

byte Sf_ScorerNum(scriptFrame_t *f, int id, const char *item, const char *var, double d) {
	if (!f->failed && item) {
		Script_SetNum(id, item, var, d);
	}
	return Sf_Check(f, item != NULL);
}

byte Sf_ScorerStr(scriptFrame_t *f, int id, const char *item, const char *var, const char *s) {
	if (!f->failed && item && s) {
		Script_SetStr(id, item, var, s);
	}
	return Sf_Check(f, item && s);
}

double Sf_ScorerGet(scriptFrame_t *f, int id, const char *item, const char *var) {
	return f->failed || !item ? 0.0 : Script_GetNum(id, item, var);
}

byte Sf_ScorerDefault(scriptFrame_t *f, int id, const char *item) {
	if (!f->failed && item) {
		Script_SetDefault(id, item);
	}
	return Sf_Check(f, item != NULL);
}

/* ---- statements and tables of the level scripts ---- */

byte Sf_Run(scriptFrame_t *f, const char *module, const char *func, const char *fmt, ...) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, module, func);
	if (fn) {
		va_list va;
		va_start(va, fmt);
		PyObject *args = Py_VaBuildValue((char *)fmt, va);
		va_end(va);
		Sf_CallArgs(f, fn, Sf_Check(f, args != NULL) ? args : NULL);
	}
	Sf_Release(f, mark);
	return !f->failed;
}

PyObject *Sf_Value(scriptFrame_t *f, const char *module, const char *text, int len) {
	char buf[0x100], *end;
	if (f->failed) {
		return NULL;
	}
	if (len < 0) {
		len = (int)strlen(text);
	}
	if (len <= 0 || len >= (int)sizeof(buf)) {
		Sf_Raise(f, PyExc_SystemError, "bad value in a call table");
		return NULL;
	}
	memcpy(buf, text, len);
	buf[len] = 0;
	if (buf[0] == '\'') {
		return Sf_Str(f, buf + 1);
	}
	if (buf[0] == '%' || buf[0] == '^') {
		char *dot = strchr(buf, '.'), *colon = buf[0] == '^' && dot ? strchr(dot, ':') : NULL;
		if (!dot || (buf[0] == '^' && !colon)) {
			Sf_Raise(f, PyExc_SystemError, "bad value in a call table");
			return NULL;
		}
		*dot = 0;
		if (colon) {
			*colon = 0;
			return Sf_Ask(f, buf + 1, dot + 1, "(s)", colon + 1);
		}
		return Sf_Mod(f, buf + 1, dot + 1);
	}
	if (buf[0] == '-' || (buf[0] >= '0' && buf[0] <= '9')) {
		if (!strchr(buf, '.')) {
			long i = strtol(buf, &end, 10);
			if (*end == 0) {
				return Sf_Int(f, i);
			}
		} else {
			double d = strtod(buf, &end);
			if (*end == 0) {
				return Sf_Float(f, d);
			}
		}
	}
	(void)module;
	return Sf_Str(f, buf);
}

byte Sf_Calls(scriptFrame_t *f, const char *module, const char *const *lines) {
	for (; *lines && !f->failed; lines++) {
		const char *p = *lines, *bar = strchr(p, '|');
		char name[0x80];
		int len = bar ? (int)(bar - p) : (int)strlen(p);
		if (len >= (int)sizeof(name)) {
			return Sf_Raise(f, PyExc_SystemError, "bad call in a call table");
		}
		memcpy(name, p, len);
		name[len] = 0;
		int mark = f->n;
		PyObject *fn;
		if (name[0] == '@') {
			fn = Sf_Global(f, module, name + 1);
		} else {
			char *dot = strchr(name, '.');
			if (!dot) {
				return Sf_Raise(f, PyExc_SystemError, "bad call in a call table");
			}
			*dot = 0;
			fn = Sf_Mod(f, name, dot + 1);
		}
		PyObject *vals[8];
		int n = 0;
		for (p = bar; p && n < 8 && !f->failed; n++) {
			const char *next = strchr(p + 1, '|');
			vals[n] = Sf_Value(f, module, p + 1, next ? (int)(next - p - 1) : -1);
			p = next;
		}
		PyObject *args = f->failed ? NULL : PyTuple_New(n);
		for (int i = 0; args && i < n; i++) {
			Py_INCREF(vals[i]);
			PyTuple_SET_ITEM(args, i, vals[i]);
		}
		Sf_CallArgs(f, fn, Sf_Check(f, f->failed || args != NULL) ? args : NULL);
		Sf_Release(f, mark);
	}
	return !f->failed;
}

byte Sf_SetAttrs(scriptFrame_t *f, PyObject *o, const char *module, const char *const *pairs) {
	for (; *pairs && !f->failed; pairs++) {
		const char *eq = strchr(*pairs, '=');
		char name[0x40];
		int len = eq ? (int)(eq - *pairs) : 0;
		if (!eq || len >= (int)sizeof(name)) {
			return Sf_Raise(f, PyExc_SystemError, "bad attribute in a table");
		}
		memcpy(name, *pairs, len);
		name[len] = 0;
		int mark = f->n;
		Sf_SetAttr(f, o, name, Sf_Value(f, module, eq + 1, -1));
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* ---- the speech and the mission scorer of the missions ---- */

byte Sf_SpeechNames(scriptFrame_t *f, PyObject *usr, PyObject *npc) {
	return Sf_SetMod(f, "Speech", "PlayerName", usr) && Sf_SetMod(f, "Speech", "NPCName", npc);
}

byte Sf_SpeechEnd(scriptFrame_t *f, PyObject *fn, PyObject *params) {
	int mark = f->n;
	Sf_SetMod(f, "Speech", "OnEndOfSpeech", fn);
	Sf_SetMod(f, "Speech", "OnEndOfSpeechParams", params ? params : Sf_Build(f, "()"));
	Sf_Release(f, mark);
	return !f->failed;
}

byte Sf_NISpeech(scriptFrame_t *f, const char *who, const char *key) {
	return (who ? Sf_Run(f, "Speech", "NISpeech", "(s)", who) : Sf_Run(f, "Speech", "NISpeech", "()")) &&
		Sf_Run(f, "Speech", "AutoAddNI", "(s)", key) && Sf_Run(f, "Speech", "RunNI", "()");
}

byte Sf_DrawScorer(scriptFrame_t *f) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, "Menu", "DrawScorer");
	Sf_Call(f, fn, "(iO)", 0, Sf_Ask(f, "SInput", "GetActionSet", "()"));
	Sf_Release(f, mark);
	return !f->failed;
}

byte Sf_MissionText(scriptFrame_t *f, PyObject *id, const char *item, const char *type, PyObject *x, PyObject *y,
	const char *font, const char *effect, PyObject *text, const char *align, PyObject *visible) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	int mark = f->n;
	if (type) {
		Sf_Run(f, "SScorer", "Add", "(Oss)", id, item, type);
	}
	Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, "X", x);
	Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, "Y", y);
	Sf_Run(f, "SScorer", "Set", "(Osss)", id, item, "Font", font);
	if (effect) {
		Sf_Run(f, "SScorer", "Set", "(Osss)", id, item, "Effect", effect);
	}
	Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, "Text", text);
	Sf_Run(f, "SScorer", "Set", "(Osss)", id, item, "Align", align);
	for (int i = 0; i < 4 && !f->failed; i++) {
		Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, rgba[i], Sf_Item(f, Sf_Mod(f, "Scorer", "MissionScorerText_Color"), i));
	}
	if (visible) {
		Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, "Visible", visible);
	}
	Sf_Release(f, mark);
	return !f->failed;
}

byte Sf_MissionSprite(scriptFrame_t *f, PyObject *id, const char *item, PyObject *x, PyObject *y, int alpha, int r, int g, int b) {
	int mark = f->n;
	Sf_Run(f, "SScorer", "Add", "(Oss)", id, item, "Sprite");
	Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, "X", x);
	Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, "Y", y);
	Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, "ScaleX", 30);
	Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, "ScaleY", 19);
	Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, "Unit", 1);
	Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, "Alpha", alpha);
	Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, "Red", r);
	Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, "Green", g);
	Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, "Blue", b);
	Sf_Release(f, mark);
	return !f->failed;
}

byte Sf_EntSet(scriptFrame_t *f, const char *name, const char *var, PyObject *v) {
	int mark = f->n;
	Sf_SetAttr(f, Sf_Ask(f, "Scrap", "GetEntity", "(s)", name), var, v);
	Sf_Release(f, mark);
	return !f->failed;
}

byte Sf_EntSetS(scriptFrame_t *f, const char *name, const char *var, const char *s) {
	int mark = f->n;
	PyObject *e = Sf_Ask(f, "Scrap", "GetEntity", "(s)", name);
	Sf_SetAttr(f, e, var, Sf_Str(f, s));
	Sf_Release(f, mark);
	return !f->failed;
}

byte Sf_EntSetI(scriptFrame_t *f, const char *name, const char *var, long i) {
	int mark = f->n;
	PyObject *e = Sf_Ask(f, "Scrap", "GetEntity", "(s)", name);
	Sf_SetAttr(f, e, var, Sf_Int(f, i));
	Sf_Release(f, mark);
	return !f->failed;
}
