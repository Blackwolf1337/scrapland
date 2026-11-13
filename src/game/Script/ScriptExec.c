/*
 * The lines the game execs (include/scrap/scriptnative.h, the Exec_ functions): the scene generic lines and the sound
 * lines of Scene.c, the save code strings of the missions (MissionsFuncs.c) and the lines of WeaponsTab.c, Chars.c,
 * Doors.c and Alarm.c. A line is read in C before anything of it runs: statements split by ';' (one may end the
 * line), each "import NAME", a chain NAME(.NAME | (args))* or such a chain ending in .NAME set to a value ("=" with
 * blanks or not). Arguments and values are literals (Native_Literal) or two literals joined by * or / (1/2.0). A line
 * in another form (keywords, newlines, comments, a leading blank, other expressions) is not taken and nothing of it
 * runs, as exec compiles the whole text first: Script_NotRead's SyntaxError.
 *
 * A statement runs as exec runs it in a function of the module: a name is LOAD_NAME through the variables of the
 * function (execScope_t: native values, and the names an import bound), the globals of the module (the variable
 * registry, Script_NameChain) and the builtins; an attribute through the registry (Script_GetAttr); a call through
 * Port_Values, the entry of the port or engine function the value is, else the call of what is there; the value of an
 * assignment is made before its target; the result of a call is dropped. OBJLIB 7: what a name or a call gives in
 * the middle of a chain is the object itself (a module, an entity handle, an instance).
 */
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/nativeobj.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

#define EXEC_STMTS 8
#define EXEC_TRAILERS 8
#define EXEC_VALUE 0x200

typedef struct {
	char kind;		/* '.' an attribute, '(' a call */
	const char *text;	/* the attribute name, or the arguments up to the closing bracket */
	int len;
} execTrailer_t;

typedef struct {
	byte import;		/* import <name> */
	char name[EXEC_NAME];	/* the module, or the first name of the chain */
	execTrailer_t t[EXEC_TRAILERS];
	int n;
	const char *value;	/* the value of an assignment to the last attribute (NULL: none) */
	int valueLen;
} execStmt_t;

/* a step that gave nothing: the error it set, else the SystemError of ceval: 0 */
static byte Exec_Fail(void) {
	Script_Fail();
	return 0;
}

/* ---- the variables of the function ---- */

static execVar_t *Exec_Find(execScope_t *x, const char *name) {
	for (int i = 0; i < x->n; i++) {
		if (!strcmp(x->vars[i].name, name)) {
			return &x->vars[i];
		}
	}
	return NULL;
}

void Exec_Var(execScope_t *x, const char *name, const nativeVal_t *v) {
	execVar_t *var = Exec_Find(x, name);
	if (!var) {
		CHECK(x->n < EXEC_VARS);
		var = &x->vars[x->n++];
		Str_Copy(var->name, name, sizeof(var->name));
	}
	var->v = v;
}

byte Exec_Bound(execScope_t *x, const char *name) {
	return Exec_Find(x, name) != NULL;
}

/* the value of a variable as the object a chain goes on with (as a call through Python hands it on, NObj_CallArg; a
 * module by its name): 1 with *out, 0 with the error, -1 without one when the function has no such variable */
static int Exec_Local(execScope_t *x, const char *name, nativeVal_t *out) {
	const execVar_t *var = Exec_Find(x, name);
	Native_SetNone(out);
	if (!var) {
		return -1;
	}
	if (var->v) {
		NObj_CallArg(var->v, out);
		return 1;
	}
	return Script_Module(var->name, out);
}

/* ---- the reader ---- */

static void Exec_Blanks(const char **p) {
	while (**p == ' ' || **p == '\t') {
		(*p)++;
	}
}

/* the length of the NAME at p (0 when there is none or it is a keyword) */
static int Exec_Name(const char *p) {
	int len = 0;
	while (isalnum((unsigned char)p[len]) || p[len] == '_') {
		len++;
	}
	return len < EXEC_NAME && Script_IsName(p, len) ? len : 0;
}

/* the end of a value at p: the first ',', ';' or unmatched closing bracket outside quotes and brackets, or the end
 * of the text; NULL when a quote does not close */
static const char *Exec_ValueEnd(const char *p) {
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

/* a value s[0:len] into *out (Native_Free it either way): a literal, or two literals joined by * or / (BINARY_MULTIPLY,
 * BINARY_DIVIDE, done only when run is set, else the right one). 0 without an error for another form, with the error
 * of the operation */
static byte Exec_Value(const char *s, int len, byte run, nativeVal_t *out) {
	char buf[EXEC_VALUE], *op = NULL;
	Native_SetNone(out);
	Exec_Blanks(&s);
	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
		len--;
	}
	if (len <= 0 || len >= (int)sizeof(buf)) {
		return 0;
	}
	memcpy(buf, s, len);
	buf[len] = '\0';
	const char *end = Exec_ValueEnd(buf);
	if (!end || *end) {
		return 0;
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
				return 0;
			}
			op = p;
		}
	}
	if (!op) {
		return Native_Literal(buf, out);
	}
	char o = *op;
	const char *right = op + 1;
	for (*op = '\0'; op > buf && (op[-1] == ' ' || op[-1] == '\t'); op--) {
		op[-1] = '\0';
	}
	Exec_Blanks(&right);
	nativeVal_t a, b;
	byte ok = Native_Literal(buf, &a) && Native_Literal(right, &b);
	if (ok && run) {
		ok = Mode_Arith(o, &a, &b, out);
		Native_Free(&b);
	} else if (ok) {
		*out = b;
	}
	Native_Free(&a);
	return ok;
}

/* the value s[0:len] is one the reader takes */
static byte Exec_IsValue(const char *s, int len) {
	nativeVal_t v;
	byte ok = Exec_Value(s, len, 0, &v);
	Native_Free(&v);
	return ok;
}

/* one statement at *p into st; 1 with *p after it (at ';' or the end), 0 when it has another form */
static byte Exec_Stmt(const char **pp, execStmt_t *st) {
	const char *p = *pp;
	int len;
	memset(st, 0, sizeof(*st));
	if (!strncmp(p, "import", 6) && (p[6] == ' ' || p[6] == '\t')) {
		p += 6;
		Exec_Blanks(&p);
		st->import = 1;
	}
	if ((len = Exec_Name(p)) == 0) {
		return 0;
	}
	memcpy(st->name, p, len);
	p += len;
	while (!st->import) {
		const char *q = p;
		Exec_Blanks(&q);
		if (*q != '.' && *q != '(') {
			break;
		}
		if (st->n >= EXEC_TRAILERS) {
			return 0;
		}
		execTrailer_t *t = &st->t[st->n++];
		t->kind = *q++;
		Exec_Blanks(&q);
		t->text = q;
		if (t->kind == '.') {
			if ((t->len = Exec_Name(q)) == 0) {
				return 0;
			}
			p = q + t->len;
			continue;
		}
		while (*q != ')') {
			const char *e = Exec_ValueEnd(q);
			if (!e || (*e != ',' && *e != ')') || !Exec_IsValue(q, (int)(e - q))) {
				return 0;
			}
			q = *e == ',' ? e + 1 : e;
			Exec_Blanks(&q);
		}
		t->len = (int)(q - t->text);
		p = q + 1;
	}
	Exec_Blanks(&p);
	if (!st->import && *p == '=' && p[1] != '=') {
		/* only an attribute is set (a name would go into the variables of the function) */
		if (st->n == 0 || st->t[st->n - 1].kind != '.') {
			return 0;
		}
		p++;
		Exec_Blanks(&p);
		const char *e = Exec_ValueEnd(p);
		if (!e || (*e != ';' && *e != '\0') || !Exec_IsValue(p, (int)(e - p))) {
			return 0;
		}
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

/* ---- the run ---- */

/* the arguments of a call trailer into the tuple *args (each value made in order, as the call pushes them;
 * Native_Free it either way) */
static byte Exec_Args(const execTrailer_t *t, nativeVal_t *args) {
	Native_SetEmpty(args, NV_TUPLE);
	for (const char *q = t->text; q < t->text + t->len;) {
		const char *e = Exec_ValueEnd(q);
		if (!Exec_Value(q, (int)(e - q), 1, Native_Append(args))) {
			return Exec_Fail();
		}
		q = *e == ',' ? e + 1 : e;
		Exec_Blanks(&q);
	}
	return 1;
}

/* f(a[0], ...) with f what the chain gave: the entry of the port or engine function f is, else the call of the value
 * (Port_ValuesOf). The result into *out, or dropped at once (out NULL: the statement) */
static byte Exec_CallValue(const nativeVal_t *f, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t r;
	byte ok = Port_ValuesOf(PORT_VSITE(), f, a, n, &r, 0);
	if (ok && out) {
		*out = r;
	} else if (ok) {
		Native_Free(&r);
	}
	return ok || Exec_Fail();
}

byte Exec_Import(execScope_t *x, const char *name) {
	if (!Mode_Import(x->module) || !Mode_Import(name)) {
		return 0;
	}
	/* no module has a name the variables do not hold */
	if ((int)strlen(name) < EXEC_NAME) {
		Exec_Var(x, name, NULL);
	}
	return 1;
}

/* a statement as exec runs it: LOAD_NAME through the variables of the function, the globals and the builtins; an
 * import stored in the variables; the value of an assignment made before its target; the result of a call dropped */
static byte Exec_Run(execScope_t *x, const nativeVal_t *globals, const execStmt_t *st) {
	char name[EXEC_NAME];
	nativeVal_t value;
	if (st->import) {
		return Exec_Import(x, st->name);
	}
	Native_SetNone(&value);
	if (st->value && !Exec_Value(st->value, st->valueLen, 1, &value)) {
		Native_Free(&value);
		return Exec_Fail();
	}
	nativeVal_t v;
	int found = Exec_Local(x, st->name, &v);
	if (found < 0 && !Exec_Bound(x, st->name)) {
		found = Script_NameChain(NULL, globals, st->name, &v);
	}
	int last = st->value ? st->n - 1 : st->n;
	byte ok = found > 0 || Exec_Fail();
	for (int i = 0; ok && i < last; i++) {
		const execTrailer_t *t = &st->t[i];
		nativeVal_t r;
		if (t->kind == '.') {
			Str_Copy(name, t->text, t->len + 1);
			ok = NObj_GetAttr(&v, name, &r) || Exec_Fail();
			Native_Free(&v);
			v = r;
		} else {
			nativeVal_t args;
			byte drop = i == last - 1 && !st->value;
			ok = Exec_Args(t, &args) && Exec_CallValue(&v, args.items, args.n, drop ? NULL : &r);
			Native_Free(&args);
			if (ok && !drop) {
				Native_Free(&v);
				v = r;
			}
		}
	}
	if (ok && st->value) {
		Str_Copy(name, st->t[last].text, st->t[last].len + 1);
		ok = Mode_VSet(&v, name, &value) || Exec_Fail();
	}
	Native_Free(&v);
	Native_Free(&value);
	return ok;
}

byte Exec_Text(execScope_t *x, const nativeVal_t *code) {
	nativeVal_t m;
	if (!Script_Module(x->module, &m)) {
		return Exec_Fail();
	}
	if (code->type != NV_STR) {
		Native_Free(&m);
		return Native_Raise(NE_TYPE, "exec 1st arg must be string, code or file object");
	}
	const char *s = Native_Text(code), *p = s;
	if ((int)strlen(s) != code->n) {
		Native_Free(&m);
		return Native_Raise(NE_VALUE, "embedded '\\0' in exec string");
	}
	const nativeVal_t *g = &m;
	execStmt_t st[EXEC_STMTS];
	int n = 0;
	/* a line that starts with a blank is exec's SyntaxError (also when it has nothing else); "" is nothing */
	byte known = Script_HasBuiltins(g) && *p != ' ' && *p != '\t', ok = 1;
	while (known && *p) {
		known = n < EXEC_STMTS && Exec_Stmt(&p, &st[n++]);
		if (known && *p == ';') {
			p++;
			Exec_Blanks(&p);
		}
	}
	if (known) {
		for (int i = 0; ok && i < n; i++) {
			ok = Exec_Run(x, g, &st[i]);
		}
	} else {
		/* a line in another form (the console, a hand-edited save, a mod) is Script_NotRead's error */
		ok = Script_NotRead("exec", s) != NULL;
	}
	Native_Free(&m);
	return ok;
}

byte Exec_Cat(nativeVal_t *out, int n, ...) {
	va_list va;
	nativeVal_t acc, r;
	byte ok = 1;
	va_start(va, n);
	Native_Copy(&acc, va_arg(va, const nativeVal_t *));
	for (int i = 1; ok && i < n; i++) {
		const nativeVal_t *b = va_arg(va, const nativeVal_t *);
		Native_SetNone(&r);
		if (!Native_IsObj(&acc) && !Native_IsObj(b)) {
			ok = Native_Arith('+', &acc, b, &r);
		} else {
			/* OBJLIB 7: an instance adds with its __add__ or __radd__ */
			PyObject *x = Native_ToPy(&acc), *y = x ? Native_ToPy(b) : NULL, *sum = y ? PyNumber_Add(x, y) : NULL;
			ok = sum != NULL && Native_Arg(sum, &r);
			Py_XDECREF(sum);
			Py_XDECREF(y);
			Py_XDECREF(x);
		}
		Native_Free(&acc);
		acc = r;
	}
	va_end(va);
	if (ok) {
		*out = acc;
	} else {
		Native_SetNone(out);
	}
	return ok;
}

byte Exec_Call(execScope_t *x, const char *name, const char *func, const nativeVal_t *a, int n) {
	nativeVal_t o, f;
	byte ok = Exec_Local(x, name, &o) > 0 && (NObj_GetAttr(&o, func, &f) || Script_Fail());
	Native_Free(&o);
	if (!ok) {
		return Exec_Fail();
	}
	ok = Exec_CallValue(&f, a, n, NULL);
	Native_Free(&f);
	return ok;
}

byte Exec_ImportText(execScope_t *x, const nativeVal_t *name) {
	if (name->type == NV_STR && Script_IsName(Native_Text(name), name->n)) {
		return Exec_Import(x, Native_Text(name));
	}
	nativeVal_t head, code;
	Native_SetStr(&head, "import ");
	byte ok = Exec_Cat(&code, 2, &head, name) && Exec_Text(x, &code);
	Native_Free(&code);
	return ok;
}

byte Exec_ImportCall(execScope_t *x, const char *target, const nativeVal_t *func) {
	if (func->type == NV_STR && Script_IsName(Native_Text(func), func->n)) {
		return Exec_Import(x, target) && Exec_Call(x, target, Native_Text(func), NULL, 0);
	}
	char text[2 * EXEC_NAME + 0x10];
	nativeVal_t head, tail, code;
	Str_Printf(text, sizeof(text), "import %s;%s.", target, target);
	Native_SetStr(&head, text);
	Native_SetStr(&tail, "()");
	byte ok = Exec_Cat(&code, 3, &head, func, &tail) && Exec_Text(x, &code);
	Native_Free(&code);
	return ok;
}
