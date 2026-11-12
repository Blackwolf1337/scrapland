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
#include "scrap/modvars.h"
#include "scrap/platform.h"
#include "scrap/scrap.h"
#include "scrap/script.h"

#define LIT_NAME_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"

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
	byte imported;
	PyObject *v = ModVar_Lookup(module, name, &imported);
	if (imported && !v) {
		Native_Raise(NE_NAME, "%s", name);
	}
	return v;
}

byte Script_SetGlobal(const char *module, const char *name, PyObject *v) {
	byte ok = v && ModVar_SetGlobal(module, name, v) == 0;
	Py_XDECREF(v);
	return ok;
}

byte Script_Import(const char *const *names) {
	for (; *names; names++) {
		PyObject *m = Script_Module(*names);
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

PyObject *Script_GetAttr(PyObject *o, const char *name) {
	PyObject *v = o ? ModVar_ObjAttr(o, name) : NULL;
	return v || !o ? v : Script_Fail();
}

PyObject *Script_Fail(void) {
	if (!Native_Failed()) {
		Native_Raise(NE_SYSTEM, "error return without exception set");
	}
	return NULL;
}

/* ceval.c MAX_RECURSION_DEPTH */
#define SCRIPT_MAX_DEPTH 10000
/* the stack left below which a call from C raises the same RuntimeError: ceval checks no stack on this build, the
 * scripts reach the depth limit in 8 MB, a chain of C functions needs more per level (more again under ASan) and
 * the Windows build has 2 MB */
#define SCRIPT_STACK_MARGIN (512 * 1024)

/* the lowest address a call from C may run at (0: unknown, no check) */
static uintptr_t Script_StackFloor(void) {
	static uintptr_t floor;
	static byte known;
	if (!known) {
		uintptr_t low, high;
		known = 1;
		if (Plat_StackBounds(&low, &high) && high - low > 2 * SCRIPT_STACK_MARGIN) {
			floor = low + SCRIPT_STACK_MARGIN;
		}
	}
	return floor;
}

byte Script_Enter(uintptr_t frame) {
	PyThreadState *ts = PyThreadState_Get();
	if (ts->recursion_depth >= SCRIPT_MAX_DEPTH || frame < Script_StackFloor()) {
		Native_Raise(NE_RUNTIME, "Maximum recursion depth exceeded");
		return 0;
	}
	ts->recursion_depth++;
	return 1;
}

void Script_Leave(void) {
	PyThreadState_Get()->recursion_depth--;
}

PyObject *Script_CallObject(PyObject *callable, PyObject *args, PyObject *kw) {
	/* the frame address, not a local's: ASan may keep locals on its fake stack */
	if (!Script_Enter((uintptr_t)__builtin_frame_address(0))) {
		return NULL;
	}
	PyObject *r = PyEval_CallObjectWithKeywords(callable, args, kw);
	Script_Leave();
	return r;
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

int Script_True(PyObject *o) {
	return o ? PyObject_IsTrue(o) : -1;
}

int Script_Eq(PyObject *a, PyObject *b) {
	if (!a || !b) {
		return -1;
	}
	int c = PyObject_Compare(a, b);
	return Native_Failed() ? -1 : c == 0;
}

PyObject *Script_Repr(PyObject *o) {
	return o ? PyObject_Repr(o) : NULL;
}

int Script_Next(PyObject *seq, int i, PyObject **item) {
	*item = NULL;
	if (!seq) {
		return -1;
	}
	if (!PySequence_Check(seq)) {
		Native_Raise(NE_TYPE, "loop over non-sequence");
		return -1;
	}
	*item = PySequence_GetItem(seq, i);
	if (*item) {
		return 1;
	}
	if (Native_ErrIs(NE_INDEX)) {
		Native_ErrClear();
		return 0;
	}
	return -1;
}

byte Script_Print(PyObject *const *items, int n) {
	PyObject *w = PySys_GetObject("stdout");
	if (!w) {
		Native_Raise(NE_RUNTIME, "lost sys.stdout");
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

byte Script_Hook(const char *module, const char *target, const char *name, const char *last) {
	PyObject *prev = Script_Attr(target, name);
	if (!prev || !Script_SetGlobal(module, last, prev)) {
		return 0;
	}
	PyObject *mine = Script_Global(module, name);
	int bad = !mine || ModVar_SetModAttr(target, name, mine) < 0;
	Py_XDECREF(mine);
	return !bad;
}

PyObject *Script_EvalIn(const char *module, PyObject *text) {
	PyObject *mod = Script_Module(module);
	PyObject *v = mod ? Script_EvalValue(PyModule_GetDict(mod), text) : NULL;
	Py_XDECREF(mod);
	return v;
}

PyObject *Script_NotRead(const char *where, const char *text) {
	Py_GateNote("notread", where, text);
	Native_Raise(NE_SYNTAX, "invalid syntax");
	return NULL;
}

/* the end of a name or a decimal number at p, else p */
static const char *Eval_Atom(const char *p) {
	const char *q = p;
	if (*q >= '0' && *q <= '9') {
		q += strspn(q, "0123456789");
		if (*q == '.') {
			q += 1 + strspn(q + 1, "0123456789");
		}
		return q;
	}
	int len = (int)strspn(q, LIT_NAME_CHARS);
	return Script_IsName(q, len) ? q + len : p;
}

/*
 * eval of two names or numbers with blanks between them ("bad list", "x y", a save variable of a case): the parser of
 * Python 1.5.2 fails at the second one. When reading it took the tokenizer to the end of the text, parsetok reports
 * E_EOF (Parser/parsetok.c: tok->done == E_EOF on line 1), else E_SYNTAX; err_input raises SyntaxError((msg,
 * (filename, line, offset, text))) with no file name and the offset after that token. 0 when s has another form.
 */
static byte Eval_Juxtaposed(const char *s) {
	const char *a = Eval_Atom(s);
	if (a == s || (*a != ' ' && *a != '\t')) {
		return 0;
	}
	const char *b = a + strspn(a, " \t"), *e = Eval_Atom(b);
	if (e == b) {
		return 0;
	}
	const char *msg = *e ? "invalid syntax" : "unexpected EOF while parsing";
	nativeVal_t v, *at;
	Native_SetEmpty(&v, NV_TUPLE);
	Native_SetText(Native_Append(&v), msg, (int)strlen(msg));
	Native_SetEmpty(at = Native_Append(&v), NV_TUPLE);
	Native_Append(at);
	Native_SetInt(Native_Append(at), 1);
	Native_SetInt(Native_Append(at), (long)(e - s));
	Native_SetText(Native_Append(at), s, (int)strlen(s));
	Native_RaiseVal(NE_SYNTAX, &v);
	Native_Free(&v);
	return 1;
}

PyObject *Script_EvalValue(PyObject *globals, PyObject *text) {
	/* the checks of builtin eval before it parses: a string without NUL, its leading blanks skipped */
	if (!PyString_Check(text)) {
		Native_Raise(NE_TYPE, "eval() argument 1 must be string or code object");
		return NULL;
	}
	const char *s = PyString_AsString(text);
	if ((int)strlen(s) != PyString_Size(text)) {
		Native_Raise(NE_VALUE, "embedded '\\0' in string arg");
		return NULL;
	}
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	PyObject *v = Script_Literal(globals, s);
	return v ? v : Script_EvalOther(globals, s);
}

PyObject *Script_EvalOther(PyObject *globals, const char *s) {
	/* a name (a save variable written as x, fc-r6b-gd4): LOAD_NAME as eval runs it, its NameError */
	PyObject *v;
	char name[0x100];
	int len = (int)strlen(s);
	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n')) {
		len--;
	}
	if (len > 0 && len < (int)sizeof(name)) {
		memcpy(name, s, len);
		name[len] = '\0';
		Native_ErrClear();
		int found = Script_NameChain(NULL, globals, name, &v);
		if (found) {
			return found > 0 ? v : NULL;
		}
	}
	Script_NotRead("eval", s);
	Eval_Juxtaposed(s);
	return NULL;
}

PyObject *Script_Entity(PyObject *name) {
	return name ? PORT_ASK(Scrap, GetEntity, "O", name) : NULL;
}

PyObject *Script_Usr(void) {
	return PORT_ASK(Scrap, UsrEntity, "i", 0L);
}

/*
 * Plain literals as eval makes them (Python 1.5.2: Parser/tokenizer.c, Python/compile.c parsenumber, parsestr,
 * com_addconst and the interning of PyCode_New; UNARY_NEGATIVE, BUILD_LIST, BUILD_TUPLE, BUILD_MAP and STORE_SUBSCR
 * of ceval). Only the tokens whose value cannot differ are taken, and the name None (looked up as LOAD_NAME does):
 * ints, longs and floats in decimal (exponents too), strings in single or double quotes with the escapes of parsestr,
 * lists, tuples and dicts. Anything else (other names, octal or hex numbers, raw or triple quoted strings, adjacent
 * strings, control characters, newlines, comments, deep nesting) gives NULL: the values the game writes and reads
 * back never have those forms, Script_EvalValue raises SyntaxError for them.
 */
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
	if (Native_Failed() || PyList_Append(l->consts, v) != 0) {
		Py_DECREF(v);
		return NULL;
	}
	return v;
}

/* the end of a token: a blank, a comma, a colon, a closing bracket or the end of the text */
static byte Lit_End(char c) {
	return c == '\0' || c == ' ' || c == '\t' || c == ',' || c == ':' || c == ')' || c == ']' || c == '}';
}

static byte Lit_Digit(char c) {
	return c >= '0' && c <= '9';
}

/* 0, 123, 0.5, 12., 3.25, 1e-05, 2.5E+12, 7L (an int or long as parsenumber reads it, a float through atof as it
 * does; the tokenizer reads an exponent only after a fraction or a number that does not start with 0) */
static PyObject *Lit_Number(scriptLit_t *l) {
	const char *s = l->p, *e = s;
	char buf[64];
	byte isFloat = 0, isLong = 0;
	if (*e == '0') {
		e++;
	} else {
		while (Lit_Digit(*e)) {
			e++;
		}
	}
	if (*e == 'l' || *e == 'L') {
		isLong = 1;
		e++;
	} else {
		if (*e == '.') {
			isFloat = 1;
			for (e++; Lit_Digit(*e); e++) {
			}
		}
		if ((*e == 'e' || *e == 'E') && (isFloat || *s != '0')) {
			const char *x = e + 1;
			if (*x == '+' || *x == '-') {
				x++;
			}
			if (!Lit_Digit(*x)) {
				return NULL;
			}
			while (Lit_Digit(*x)) {
				x++;
			}
			isFloat = 1;
			e = x;
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
	if (isLong) {
		return Lit_Const(l, PyLong_FromString(buf, NULL, 0));
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

/* the escapes of parsestr in s[0:len] (no newline after a backslash) into out, its length */
static int Lit_Unescape(const char *s, int len, char *out) {
	const char *end = s + len;
	char *p = out;
	while (s < end) {
		if (*s != '\\') {
			*p++ = *s++;
			continue;
		}
		s++;
		switch (*s++) {
		case '\\': *p++ = '\\'; break;
		case '\'': *p++ = '\''; break;
		case '"': *p++ = '"'; break;
		case 'b': *p++ = '\b'; break;
		case 'f': *p++ = '\014'; break;
		case 't': *p++ = '\t'; break;
		case 'n': *p++ = '\n'; break;
		case 'r': *p++ = '\r'; break;
		case 'v': *p++ = '\013'; break;
		case 'a': *p++ = '\007'; break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
			int c = s[-1] - '0';
			if ('0' <= *s && *s <= '7') {
				c = (c << 3) + *s++ - '0';
				if ('0' <= *s && *s <= '7') {
					c = (c << 3) + *s++ - '0';
				}
			}
			*p++ = (char)c;
			break;
		}
		case 'x':
			if (isxdigit((unsigned char)*s)) {
				unsigned int x = 0;
				do {
					int c = (unsigned char)*s++;
					x = (x << 4) & ~0xFu;
					x += isdigit(c) ? (unsigned int)(c - '0') : islower(c) ? (unsigned int)(10 + c - 'a') :
						(unsigned int)(10 + c - 'A');
				} while (isxdigit((unsigned char)*s));
				*p++ = (char)x;
				break;
			}
			/* fall through */
		default:
			*p++ = '\\';
			*p++ = s[-1];
			break;
		}
	}
	return (int)(p - out);
}

/* 'text' or "text", with backslash escapes */
static PyObject *Lit_String(scriptLit_t *l) {
	char q = *l->p;
	const char *s = l->p + 1, *e = s;
	byte escapes = 0;
	if (s[0] == q && s[1] == q) {
		return NULL;
	}
	while (*e != q) {
		if (*e == '\\' && (e[1] == '\\' || Lit_Plain((unsigned char)e[1]))) {
			/* the tokenizer takes the character after a backslash as it is, a quote too */
			escapes = 1;
			e += 2;
			continue;
		}
		if (!Lit_Plain((unsigned char)*e)) {
			return NULL;
		}
		e++;
	}
	if (!Lit_End(e[1])) {
		return NULL;
	}
	l->p = e + 1;
	if (!escapes) {
		return Lit_Const(l, Script_StrConst(s, (int)(e - s)));
	}
	/* as parsestr: the string made at the full length, then cut; interned as Script_StrConst does */
	PyObject *v = PyString_FromStringAndSize(NULL, (int)(e - s));
	if (!v || _PyString_Resize(&v, Lit_Unescape(s, (int)(e - s), PyString_AsString(v))) != 0) {
		return NULL;
	}
	if ((int)strspn(PyString_AsString(v), LIT_NAME_CHARS) == PyString_Size(v)) {
		PyString_InternInPlace(&v);
	}
	return Lit_Const(l, v);
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

/* {key: value, ...}: BUILD_MAP, then each pair stored as STORE_SUBSCR does (an unhashable key fails here and raises
 * in eval) */
static PyObject *Lit_Dict(scriptLit_t *l) {
	PyObject *d = PyDict_New();
	l->p++;
	l->depth++;
	Lit_Blanks(l);
	while (d && *l->p != '}') {
		PyObject *k = Lit_Item(l), *v = NULL;
		if (k) {
			Lit_Blanks(l);
			if (*l->p == ':') {
				l->p++;
				Lit_Blanks(l);
				v = Lit_Item(l);
			}
		}
		int bad = !v || PyDict_SetItem(d, k, v) != 0;
		Py_XDECREF(k);
		Py_XDECREF(v);
		if (!bad) {
			Lit_Blanks(l);
			if (*l->p == ',') {
				l->p++;
				Lit_Blanks(l);
			} else if (*l->p != '}') {
				bad = 1;
			}
		}
		if (bad) {
			Py_DECREF(d);
			d = NULL;
		}
	}
	l->depth--;
	if (!d) {
		return NULL;
	}
	l->p++;
	if (!Lit_End(*l->p)) {
		Py_DECREF(d);
		return NULL;
	}
	return d;
}

/* the builtins of the frame of eval (a dict, the one of a module), a module's through the registry */
static PyObject *Lit_BuiltinsDict(PyObject *globals) {
	PyObject *b = ModVar_DictGet(globals, "__builtins__");
	return PyModule_Check(b) ? PyModule_GetDict(b) : b;
}

static PyObject *Lit_Item(scriptLit_t *l) {
	char c = *l->p;
	if (c == '\'' || c == '"') {
		return Lit_String(l);
	}
	if (!strncmp(l->p, "None", 4) && Lit_End(l->p[4])) {
		/* LOAD_NAME None: the globals (the locals of eval, a module's through the registry), the builtins; a NameError
		 * is left to eval */
		PyObject *v = ModVar_DictGet(l->globals, "None");
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
	if (c == '{' && l->depth < LIT_DEPTH) {
		return Lit_Dict(l);
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
	PyObject *b = globals && PyDict_Check(globals) ? ModVar_DictGet(globals, "__builtins__") : NULL;
	return b && (PyDict_Check(b) || PyModule_Check(b));
}

byte Script_HasBuiltins(PyObject *globals) {
	return Lit_Builtins(globals);
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
		Native_ErrClear();
	}
	return v;
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

byte Script_IsQuotableText(const char *s, int n) {
	for (int i = 0; i < n; i++) {
		if (!Lit_Plain((unsigned char)s[i]) || s[i] == '\'') {
			return 0;
		}
	}
	return 1;
}

byte Script_IsQuotable(PyObject *o) {
	return o && PyString_Check(o) && Script_IsQuotableText(PyString_AsString(o), PyString_Size(o));
}

/* "[n]" at p (n a decimal int as repr writes it: 0 or no leading zero, no sign): its length and n, else 0 */
static int Lit_Subscript(const char *p, long *n) {
	int len = (int)strspn(p + 1, "0123456789");
	if (p[0] != '[' || len == 0 || len > 9 || (p[1] == '0' && len > 1) || p[len + 1] != ']') {
		return 0;
	}
	*n = strtol(p + 1, NULL, 10);
	return len + 2;
}

int Script_NameChain(PyObject *locals, PyObject *globals, const char *text, PyObject **out) {
	long n;
	*out = NULL;
	if (!Lit_Builtins(globals)) {
		return 0;
	}
	for (const char *p = text;; p++) {
		int len = (int)strspn(p, LIT_NAME_CHARS), sub;
		if (!Script_IsName(p, len)) {
			return 0;
		}
		for (p += len; (sub = Lit_Subscript(p, &n)) > 0;) {
			p += sub;
		}
		if (*p != '.' && *p != '\0') {
			return 0;
		}
		if (!*p) {
			break;
		}
	}
	PyObject *b = Lit_BuiltinsDict(globals), *v = NULL;
	for (const char *p = text;; p++) {
		int len = (int)strspn(p, LIT_NAME_CHARS), sub;
		char name[0x100];
		if (len >= (int)sizeof(name)) {
			Py_XDECREF(v);
			return 0;
		}
		memcpy(name, p, len);
		name[len] = '\0';
		if (!v) {
			/* LOAD_NAME: the locals, the globals (a module's through the registry), the builtins */
			PyObject *x = locals ? ModVar_DictGet(locals, name) : NULL;
			x = x || locals == globals ? x : ModVar_DictGet(globals, name);
			x = x ? x : PyDict_GetItemString(b, name);
			if (!x) {
				Native_Raise(NE_NAME, "%s", name);
			}
			v = Script_Ref(x);
		} else {
			PyObject *x = Script_GetAttr(v, name);
			Py_DECREF(v);
			v = x;
		}
		/* BINARY_SUBSCR of the constant n (a list with an int: the same IndexError as the fast path of ceval) */
		for (p += len; v && (sub = Lit_Subscript(p, &n)) > 0; p += sub) {
			PyObject *k = PyInt_FromLong(n), *x = k ? PyObject_GetItem(v, k) : NULL;
			Py_XDECREF(k);
			Py_DECREF(v);
			v = x;
		}
		if (!v) {
			return -1;
		}
		if (!*p) {
			break;
		}
	}
	*out = v;
	return 1;
}
