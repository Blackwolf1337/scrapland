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
#include "scrap/nativeobj.h"
#include "scrap/platform.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

#define LIT_NAME_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"

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
		nativeVal_t m;
		byte ok = Script_Module(*names, &m);
		Native_Free(&m);
		if (!ok) {
			return 0;
		}
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
	if (!v && o) {
		Script_Fail();
	}
	return v;
}

byte Script_Fail(void) {
	if (!Native_Failed()) {
		Native_Raise(NE_SYSTEM, "error return without exception set");
	}
	return 0;
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

/* the calls from C that run now (the recursion depth of the thread state of 1.5.2) */
static int script_depth;

byte Script_Enter(uintptr_t frame) {
	if (script_depth >= SCRIPT_MAX_DEPTH || frame < Script_StackFloor()) {
		Native_Raise(NE_RUNTIME, "Maximum recursion depth exceeded");
		return 0;
	}
	script_depth++;
	return 1;
}

void Script_Leave(void) {
	script_depth--;
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

int Script_ForItem(const nativeVal_t *seq, int i, nativeVal_t *out) {
	Native_SetNone(out);
	if (Native_IsObj(seq)) {
		return Mode_RefNext(seq, i, out);
	}
	int n = Native_ForCount(seq);
	if (n < 0) {
		return -1;
	}
	return i >= n ? 0 : Native_SeqItem(seq, i, out) ? 1 : -1;
}

/* ---- the print statement and the writes to sys.stdout (PyFile_* of fileobject.c on the objects of the scripts) ---- */

/* OBJLIB 7e4b: f is a file object of the library (sys.__stdout__ put in place) */
static PyObject *Val_PyFile(const nativeVal_t *f) {
	PyObject *py = NObj_Py(f);
	return py && PyFile_Check(py) ? py : NULL;
}

int Script_SoftSpace(const nativeVal_t *f, int newflag) {
	PyObject *py = Val_PyFile(f);
	nativeVal_t v;
	int old = 0;
	if (py) {
		return PyFile_SoftSpace(py, newflag);
	}
	if (NObj_GetAttr(f, "softspace", &v)) {
		old = v.type == NV_INT ? (int)v.i : 0;
		Native_Free(&v);
	} else {
		Native_ErrClear();
	}
	Native_SetInt(&v, newflag);
	if (!NObj_SetAttr(f, "softspace", &v)) {
		Native_ErrClear();
	}
	return old;
}

byte Script_FileWrite(const nativeVal_t *f, const nativeVal_t *v, byte raw) {
	PyObject *py = Val_PyFile(f);
	nativeVal_t w, t, r;
	nativeBuf_t b;
	if (py) {
		PyObject *o = Native_ToPy(v);
		byte ok = o && PyFile_WriteObject(o, py, raw ? Py_PRINT_RAW : 0) == 0;
		Py_XDECREF(o);
		return ok;
	}
	if (!NObj_GetAttr(f, "write", &w)) {
		return 0;
	}
	Native_BufInit(&b);
	byte ok = NObj_Repr(&b, v, raw ? NR_STR : NR_REPR);
	if (ok) {
		Native_SetText(&t, b.p, b.len);
		ok = NObj_Call(&w, &t, 1, &r);
		Native_Free(&r);
		Native_Free(&t);
	}
	Native_BufFree(&b);
	Native_Free(&w);
	return ok;
}

byte Script_FileWriteText(const nativeVal_t *f, const char *s) {
	nativeVal_t t;
	if (Native_Failed()) {
		/* PyFile_WriteString writes nothing while an error is set */
		return 0;
	}
	Native_SetText(&t, s, (int)strlen(s));
	byte ok = Script_FileWrite(f, &t, 1);
	Native_Free(&t);
	return ok;
}

byte Script_FlushLine(void) {
	nativeVal_t f;
	byte ok = 1;
	if (Script_SysVar("stdout", &f) && Script_SoftSpace(&f, 0)) {
		ok = Script_FileWriteText(&f, "\n");
	}
	Native_Free(&f);
	return ok;
}

/* sys.stdout: 1 with *f, 0 with the RuntimeError of ceval */
static byte Val_Stdout(nativeVal_t *f) {
	return Script_SysVar("stdout", f) || Native_Raise(NE_RUNTIME, "lost sys.stdout");
}

byte Script_WriteOut(const nativeVal_t *v, byte raw) {
	nativeVal_t f;
	byte ok = Val_Stdout(&f) && Script_FileWrite(&f, v, raw);
	Native_Free(&f);
	return ok;
}

byte Script_Print(const nativeVal_t *const *items, int n) {
	nativeVal_t w;
	if (!Val_Stdout(&w)) {
		return 0;
	}
	byte ok = 1;
	for (int i = 0; ok && i < n; i++) {
		ok = !(Script_SoftSpace(&w, 1) && !Script_FileWriteText(&w, " ")) && Script_FileWrite(&w, items[i], 1);
		if (ok && items[i]->type == NV_STR) {
			const char *s = Native_Text(items[i]);
			int len = items[i]->n;
			if (len > 0 && isspace((unsigned char)s[len - 1]) && s[len - 1] != ' ') {
				Script_SoftSpace(&w, 0);
			}
		}
	}
	ok = ok && Script_FileWriteText(&w, "\n");
	if (ok) {
		Script_SoftSpace(&w, 0);
	}
	Native_Free(&w);
	return ok;
}

byte Script_Hook(const char *module, const char *target, const char *name, const char *last) {
	nativeVal_t prev, mine;
	byte ok = ModVar_ModAttrVal(target, name, &prev) && ModVar_SetGlobalVal(module, last, &prev);
	Native_Free(&prev);
	if (!ok) {
		return 0;
	}
	ok = ModVar_GlobalVal(module, name, &mine) && ModVar_SetModAttrVal(target, name, &mine);
	Native_Free(&mine);
	return ok;
}

byte Script_EvalIn(const char *module, const nativeVal_t *text, nativeVal_t *out) {
	nativeVal_t m;
	byte ok = Script_Module(module, &m) && Script_EvalText(&m, text, out);
	Native_Free(&m);
	return ok;
}

PyObject *Script_NotRead(const char *where, const char *text) {
	Script_GateNote("notread", where, text);
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

/* the builtins of the frame of eval: the dict object of the globals' __builtins__ (a module: its variables), 1 with a
 * copy in *out, 0 when it has none valid (PyFrame_New raises) */
static byte Val_Builtins(const nativeVal_t *globals, nativeVal_t *out) {
	modVarRef_t ref;
	Native_SetNone(out);
	if (!globals || !ModVar_RefOfNs(globals, &ref) || !ModVar_GetVal(&ref, "__builtins__", out)) {
		return 0;
	}
	const nativeVal_t *vars = NObj_ModuleVars(out);
	if (vars) {
		nativeVal_t d;
		Native_Copy(&d, vars);
		Native_Free(out);
		*out = d;
	}
	if (out->type == NV_DICT || NObj_Kind(out) == NO_DICT) {
		return 1;
	}
	Native_Free(out);
	return 0;
}

byte Script_HasBuiltins(const nativeVal_t *globals) {
	nativeVal_t b;
	byte ok = Val_Builtins(globals, &b);
	Native_Free(&b);
	return ok;
}

/* LOAD_NAME: the locals, the globals (a module's through the registry), the builtins dict b: 1 with *out, 0 without
 * an error when none has name */
static byte Val_Load(const nativeVal_t *locals, const nativeVal_t *globals, const nativeVal_t *b, const char *name,
	nativeVal_t *out) {
	modVarRef_t ref;
	if (locals && ModVar_RefOfNs(locals, &ref) && ModVar_GetVal(&ref, name, out)) {
		return 1;
	}
	if (locals != globals && ModVar_RefOfNs(globals, &ref) && ModVar_GetVal(&ref, name, out)) {
		return 1;
	}
	nativeVal_t key;
	Native_SetStr(&key, name);
	byte ok = NObj_GetItem(b, &key, out);
	Native_Free(&key);
	if (!ok) {
		Native_ErrClear();
	}
	return ok;
}

byte Script_EvalText(const nativeVal_t *globals, const nativeVal_t *text, nativeVal_t *out) {
	/* the checks of builtin eval before it parses: a string without NUL, its leading blanks skipped */
	Native_SetNone(out);
	if (text->type != NV_STR) {
		return Native_Raise(NE_TYPE, "eval() argument 1 must be string or code object");
	}
	const char *s = Native_Text(text);
	if ((int)strlen(s) != text->n) {
		return Native_Raise(NE_VALUE, "embedded '\\0' in string arg");
	}
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	/* a plain literal (None as LOAD_NAME finds it) when the globals give the frame builtins */
	nativeVal_t b, none;
	if (Val_Builtins(globals, &b)) {
		byte has = Val_Load(NULL, globals, &b, "None", &none), ok = Native_LiteralNone(s, has ? &none : NULL, out);
		if (has) {
			Native_Free(&none);
		}
		Native_Free(&b);
		if (ok) {
			return 1;
		}
	}
	return Script_EvalOther(globals, s, out);
}

byte Script_EvalOther(const nativeVal_t *globals, const char *s, nativeVal_t *out) {
	/* a name (a save variable written as x, fc-r6b-gd4): LOAD_NAME as eval runs it, its NameError */
	char name[0x100];
	int len = (int)strlen(s);
	Native_SetNone(out);
	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n')) {
		len--;
	}
	if (len > 0 && len < (int)sizeof(name)) {
		memcpy(name, s, len);
		name[len] = '\0';
		Native_ErrClear();
		int found = Script_NameChain(NULL, globals, name, out);
		if (found) {
			return found > 0;
		}
	}
	Script_NotRead("eval", s);
	Eval_Juxtaposed(s);
	return 0;
}

byte Script_IsNameText(const char *s, int len) {
	return (int)strspn(s, LIT_NAME_CHARS) == len;
}

uintptr_t Script_ConstId(const nativeVal_t *v) {
	uintptr_t id = Native_IdOf(v);
	if (!id) {
		id = v->type == NV_STR && Script_IsNameText(Native_Text(v), v->n) ? Native_InternId(Native_Text(v), v->n) :
			Native_NewId();
	}
	return id;
}

void Script_StrConstVal(nativeVal_t *out, const char *s) {
	Native_SetStr(out, s);
	out->id = Script_ConstId(out);
}

/* a character a string literal holds as it is (no escape) */
static byte Lit_Plain(unsigned char c) {
	return c >= 0x20 && c != 0x7f && c != '\\';
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

/* "[n]" at p (n a decimal int as repr writes it: 0 or no leading zero, no sign): its length and n, else 0 */
static int Lit_Subscript(const char *p, long *n) {
	int len = (int)strspn(p + 1, "0123456789");
	if (p[0] != '[' || len == 0 || len > 9 || (p[1] == '0' && len > 1) || p[len + 1] != ']') {
		return 0;
	}
	*n = strtol(p + 1, NULL, 10);
	return len + 2;
}

int Script_NameChain(const nativeVal_t *locals, const nativeVal_t *globals, const char *text, nativeVal_t *out) {
	long n;
	nativeVal_t b, v;
	Native_SetNone(out);
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
	if (!Val_Builtins(globals, &b)) {
		return 0;
	}
	byte first = 1, ok = 1;
	Native_SetNone(&v);
	for (const char *p = text; ok; p++) {
		int len = (int)strspn(p, LIT_NAME_CHARS), sub;
		char name[0x100];
		if (len >= (int)sizeof(name)) {
			Native_Free(&v);
			Native_Free(&b);
			return 0;
		}
		memcpy(name, p, len);
		name[len] = '\0';
		if (first) {
			first = 0;
			ok = Val_Load(locals, globals, &b, name, &v) || Native_Raise(NE_NAME, "%s", name);
		} else {
			/* getattr, a NULL without an error as the SystemError of ceval */
			nativeVal_t x;
			ok = NObj_GetAttr(&v, name, &x) || Script_Fail();
			Native_Free(&v);
			v = x;
		}
		/* BINARY_SUBSCR of the constant n (a list with an int: the same IndexError as the fast path of ceval) */
		for (p += len; ok && (sub = Lit_Subscript(p, &n)) > 0; p += sub) {
			nativeVal_t k, x;
			Native_SetInt(&k, n);
			ok = NObj_GetItem(&v, &k, &x);
			Native_Free(&v);
			v = x;
		}
		if (!ok || !*p) {
			break;
		}
	}
	Native_Free(&b);
	if (!ok) {
		Native_Free(&v);
		return -1;
	}
	*out = v;
	return 1;
}
