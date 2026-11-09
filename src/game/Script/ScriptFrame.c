/*
 * A ported script function line by line (include/scrap/script.h, the Sf_ helpers). The frame keeps the temporary
 * references of the function and its error state: after the first error every helper does nothing and returns NULL
 * (or -1), as the script stops at the line that raised, and Sf_Done releases the references and returns NULL with the
 * error left set for the caller. The values keep their Python types and the operations are those of ceval (the
 * number and sequence rules of Python 1.5.2). Where a value helper of ScriptValue.c does the operation, the frame
 * helper only keeps its result.
 *
 * What is left are the users of the exec lines: Sf_Exec for the scene generic lines (Scene.c), the save code strings
 * of the missions (MissionsFuncs.c) and the lines of WeaponsTab.c, Chars.c, Doors.c and Alarm.c, with the frame
 * helpers those lines need (data paths, OBJLIB 6).
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

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

PyObject *Sf_Int(scriptFrame_t *f, long i) {
	return f->failed ? NULL : Sf_Keep(f, PyInt_FromLong(i));
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
	return f->failed ? NULL : Sf_Keep(f, Script_Module(name));
}

PyObject *Sf_Global(scriptFrame_t *f, const char *module, const char *name) {
	return f->failed ? NULL : Sf_Keep(f, Script_Global(module, name));
}

/* ---- objects ---- */

PyObject *Sf_Attr(scriptFrame_t *f, PyObject *o, const char *name) {
	return f->failed || !o ? NULL : Sf_Keep(f, PyObject_GetAttrString(o, (char *)name));
}

byte Sf_SetAttr(scriptFrame_t *f, PyObject *o, const char *name, PyObject *v) {
	return Sf_Check(f, !f->failed && o && v && PyObject_SetAttrString(o, (char *)name, v) == 0);
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

/* ---- calls ---- */

PyObject *Sf_CallArgs(scriptFrame_t *f, PyObject *callable, PyObject *args) {
	if (f->failed || !callable || !args) {
		Py_XDECREF(args);
		return NULL;
	}
	return Sf_Keep(f, Script_Invoke(callable, args));
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
	/* a line in another form (the console, a hand-edited save, a mod) is Script_NotRead's error */
	return Sf_Keep(f, Script_NotRead("exec", s)) != NULL;
}

byte Sf_ImportTo(scriptFrame_t *f, const char *module, PyObject *locals, PyObject *name) {
	if (f->failed || !name || !locals || !PyString_Check(name) ||
		!Script_IsName(PyString_AsString(name), PyString_Size(name))) {
		return Sf_Exec(f, Sf_Cat(f, "sO", "import ", name), module, locals);
	}
	/* the exec'd line imports in the module's globals, where a name without a dot is a top level module */
	PyObject *m = Sf_Module(f, module) ? Sf_Keep(f, Script_Module(PyString_AsString(name))) : NULL;
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
