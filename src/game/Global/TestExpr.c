/*
 * The expressions of the test channel (TestChannel.c): the test tools ask the game with lines in a small part of
 * Python's syntax, read and run here in C over the objects of the script modules, with no Python code run (no
 * compiler, no byte code). Not in the original, test tools only.
 *
 * A line is read into a tree of nodes once, then run. What a line can hold:
 *   statements     expression; target = ... = expression; del targets; import names; pass. Several separated by ";".
 *                  Targets: names (the globals of the line), attributes, items, slices, tuples and lists of targets.
 *   expressions    literals (the reader of the scripts' constants, Script_Literal: a literal tuple, list or dict is
 *                  made as the compiler makes its constants), inf, nan, names, tuples, lists, dicts, `x` (repr),
 *                  attributes, calls with positional and keyword arguments, items, slices, unary - + ~, not,
 *                  ** * / % + - << >> & ^ |, the comparisons < > == >= <= <> != in, not in, is, is not (chained),
 *                  and, or, "a if c else b" and lambda (parameters with defaults, *args, **kw).
 * Names are found in the locals of the lambdas around them (nested lambdas see the parameters of the outer ones),
 * then in the globals the line runs in (__globals__ is that dict itself), the builtins, the channel's own functions (tc_..., below), the loaded
 * modules, else the module is imported (a name nothing gives raises NameError as Python does). The variables of a pack
 * module (its attributes, and its globals in the lines "in <module>" runs) are read, set and deleted through the
 * registry of the module variables (include/scrap/modvars.h), with the errors of the module object of 1.5.2; the
 * builtins getattr, setattr, hasattr and delattr are twins that do the same.
 * The operators and comparisons do what the byte code of Python 1.5.2 does (the abstract object calls of ceval.c).
 *
 * The channel's own functions, for the helpers of the regression cases (tools/test/regress/channel/<name>.tc):
 *   tc_sdn(x)                    the names of the objects of x: entity handles 'ent:<Name>', functions 'fn:<name>',
 *                                instances 'inst:<class>', floats rounded to 2 digits, tuples, lists and dict values
 *                                walked (the _sdn of the super deal cases)
 *   tc_catch(f, args[, kw[, handler]])   f(*args, **kw); an exception gives handler(type, value) (as sys.exc_type
 *                                and sys.exc_value in an except clause), or (type, value) without a handler
 *   tc_try(f, args, default)     f(*args), or default after any exception (a bare except clause)
 *   tc_finally(f, args, after)   f(*args), then after() also when f raised (the exception goes on): try/finally
 *   tc_swap(obj, name, value, f) f() while obj.name is value (put back afterwards, also after an exception)
 *   tc_obj(name, attrs[, call[, repr]])   an instance of a new class name ("Module.Class" sets its module) with the
 *                                attributes of the dict attrs; call and repr become its __call__ and __repr__
 *   tc_class(name[, attrs])      a new class name with the class attributes of the dict attrs
 *   tc_walk(x, next)             [x, next(x), next(next(x)), ...] up to the first value that is not true
 *   tc_sorted(seq)               a sorted list of the items
 *   tc_set(o, key, value), tc_del(o, key), tc_clear(list)   o[key] = value, del o[key], del list[:]
 */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/modvars.h"
#include "scrap/pyfunction.h"
#include "scrap/script.h"
#include "testchannel.h"

#define TX_DEPTH 64			/* nesting of brackets in a line */
#define TX_PARSE_DEPTH 200	/* nesting of the reader */
#define TX_EVAL_DEPTH 300	/* nesting of evaluation (lambdas calling lambdas) */

enum {
	X_CONST,	/* obj */
	X_LIT,		/* obj: the text of a literal with a list or dict in it, read each time (new objects each time) */
	X_NAME,		/* obj: the name */
	X_ATTR,		/* a.obj */
	X_CALL,		/* a(kids[0..n-1], obj[i]=kids[n+i]); flags 1: b gives the whole argument tuple */
	X_INDEX,	/* a[b] */
	X_SLICE,	/* a[b:c], b and c may be NULL */
	X_TUPLE,	/* kids */
	X_LIST,		/* kids */
	X_DICT,		/* kids: key, value, key, value, ... */
	X_UNARY,	/* op a: '-', '+', '~', '!' (not) */
	X_BIN,		/* a op b */
	X_CMP,		/* kids[0] ops[0] kids[1] ops[1] ... */
	X_AND,		/* a and b */
	X_OR,		/* a or b */
	X_IF,		/* a if b else c */
	X_LAMBDA,	/* obj: the names (n params, then *args, **kw), kids: the defaults of the last nkw params, a: body */
	X_REPR,		/* `a` */
	S_EXPR,		/* a */
	S_ASSIGN,	/* kids = ... = a */
	S_DEL,		/* del kids */
	S_IMPORT,	/* import obj (a tuple of names) */
	S_PASS,
	S_LIST		/* kids; flags 1: a single expression without ";" (answered with its repr) */
};

/* comparison operators of X_CMP */
enum { C_LT, C_LE, C_EQ, C_NE, C_GT, C_GE, C_IN, C_NOTIN, C_IS, C_ISNOT };

typedef struct tcNode tcNode;
struct tcNode {
	byte kind, op, flags;
	int n, nkw;
	tcNode **kids;
	tcNode *a, *b, *c;
	PyObject *obj;
	byte *ops;
};

/* the nodes of a line: freed when the line is done and no lambda of it lives on */
typedef struct tcTree {
	int refs;
	void *blocks;		/* the allocations, linked through their first word */
	PyObject *objs;		/* a list: the objects the nodes keep */
} tcTree;

typedef struct tcParse {
	const char *p;
	tcTree *t;
	int depth;
} tcParse;

/* where names are found while a node runs */
typedef struct tcScope {
	PyObject *locals;	/* the parameters of the lambda that runs, NULL at the top level */
	PyObject *chain;	/* the locals of the lambdas around it: (dict, chain) tuples, NULL at the end */
	PyObject *globals;
	byte inMod;		/* the globals are those of a pack module: its variables through the registry (mv) */
	modVarRef_t mv;
} tcScope;

static int tx_depth;
/* the tree of the nodes that run (a line, or the line of a lambda that runs): a lambda made now keeps it */
static tcTree *tx_tree;

/* ---- the tree ---- */

static tcTree *TX_New(void) {
	tcTree *t = Mem_Alloc(sizeof(tcTree));

	t->refs = 1;
	t->blocks = NULL;
	t->objs = PyList_New(0);
	return t;
}

static void TX_Release(tcTree *t) {
	if (!t || --t->refs > 0) {
		return;
	}
	Py_XDECREF(t->objs);
	while (t->blocks) {
		void **blk = t->blocks;
		t->blocks = blk[0];
		Mem_Free(blk);
	}
	Mem_Free(t);
}

static void *TX_Alloc(tcTree *t, size_t size) {
	void **blk = Mem_Alloc((uint32_t)(sizeof(void *) + size));

	memset(blk, 0, sizeof(void *) + size);
	blk[0] = t->blocks;
	t->blocks = blk;
	return blk + 1;
}

/* o (a new reference) kept by the tree: borrowed from it afterwards; NULL stays NULL */
static PyObject *TX_Keep(tcTree *t, PyObject *o) {
	if (o && t->objs) {
		PyList_Append(t->objs, o);
		Py_DECREF(o);
	}
	return o;
}

static tcNode *TX_Node(tcParse *ps, byte kind) {
	tcNode *x = TX_Alloc(ps->t, sizeof(tcNode));

	x->kind = kind;
	return x;
}

typedef struct tcVec {
	tcNode **v;
	int n, cap;
} tcVec;

static void TX_Push(tcVec *vec, tcNode *x) {
	if (vec->n == vec->cap) {
		vec->cap = vec->cap ? vec->cap * 2 : 8;
		vec->v = Mem_Realloc(vec->v, (uint32_t)(sizeof(tcNode *) * (size_t)vec->cap));
	}
	vec->v[vec->n++] = x;
}

/* the nodes of vec into the tree (vec emptied) */
static tcNode **TX_Kids(tcParse *ps, tcVec *vec) {
	tcNode **k = vec->n ? TX_Alloc(ps->t, sizeof(tcNode *) * (size_t)vec->n) : NULL;

	if (k) {
		memcpy(k, vec->v, sizeof(tcNode *) * (size_t)vec->n);
	}
	if (vec->v) {
		Mem_Free(vec->v);
	}
	vec->v = NULL;
	vec->cap = 0;
	return k;
}

static void TX_Drop(tcVec *vec) {
	if (vec->v) {
		Mem_Free(vec->v);
	}
	vec->v = NULL;
	vec->n = vec->cap = 0;
}

/* ---- reading ---- */

static void TX_Blanks(tcParse *ps) {
	while (*ps->p == ' ' || *ps->p == '\t') {
		ps->p++;
	}
}

static byte TX_NameChar(char c, byte first) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || (!first && c >= '0' && c <= '9');
}

static tcNode *TX_Syntax(const char *at) {
	char msg[0x100];

	snprintf(msg, sizeof(msg), "channel expression: cannot read at '%.60s'", at);
	PyErr_SetString(PyExc_SyntaxError, msg);
	return NULL;
}

/* p at a quote: the closing quote (backslash escapes skipped), NULL when it is open */
static const char *TX_StrEnd(const char *p) {
	char q = *p++;

	for (; *p && *p != q; p++) {
		if (*p == '\\' && p[1]) {
			p++;
		}
	}
	return *p ? p : NULL;
}

/* p after an opening bracket: the matching close at the same depth (quotes skipped), NULL when there is none */
static const char *TX_Close(const char *p, char close) {
	char stack[TX_DEPTH];
	int depth = 0;

	stack[0] = close;
	for (; *p; p++) {
		if (*p == '"' || *p == '\'') {
			p = TX_StrEnd(p);
			if (!p) {
				return NULL;
			}
		} else if (*p == '(' || *p == '[' || *p == '{') {
			if (++depth >= TX_DEPTH) {
				return NULL;
			}
			stack[depth] = *p == '(' ? ')' : *p == '[' ? ']' : '}';
		} else if (*p == ')' || *p == ']' || *p == '}') {
			if (*p != stack[depth]) {
				return NULL;
			}
			if (!depth--) {
				return p;
			}
		}
	}
	return NULL;
}

/* the keyword kw next (not the start of a longer name): read it */
static byte TX_Kw(tcParse *ps, const char *kw) {
	size_t n = strlen(kw);

	TX_Blanks(ps);
	if (strncmp(ps->p, kw, n) || TX_NameChar(ps->p[n], 0)) {
		return 0;
	}
	ps->p += n;
	return 1;
}

/* the operator text op next: read it (not when the next character makes it another operator, e.g. "*" of "**") */
static byte TX_Op(tcParse *ps, const char *op, const char *notNext) {
	size_t n = strlen(op);

	TX_Blanks(ps);
	if (strncmp(ps->p, op, n) || (notNext && ps->p[n] && strchr(notNext, ps->p[n]))) {
		return 0;
	}
	ps->p += n;
	return 1;
}

static byte TX_IsKeyword(const char *s, int n) {
	static const char *const kws[] = { "and", "or", "not", "in", "is", "lambda", "if", "else", "del", "import",
		"pass", "print", "exec", "def", "class", "return", "for", "while", "try", "except", "finally", "raise",
		"global", "from", "elif", "break", "continue", "assert", NULL };

	for (int i = 0; kws[i]; i++) {
		if ((int)strlen(kws[i]) == n && !strncmp(kws[i], s, (size_t)n)) {
			return 1;
		}
	}
	return 0;
}

/* a name at ps->p into an interned string kept by the tree; NULL (no error) when there is none */
static PyObject *TX_Name(tcParse *ps) {
	char name[0x80];
	const char *p = ps->p;
	int n = 0;

	TX_Blanks(ps);
	p = ps->p;
	if (!TX_NameChar(*p, 1)) {
		return NULL;
	}
	while (TX_NameChar(p[n], 0)) {
		n++;
	}
	if (TX_IsKeyword(p, n) || n >= (int)sizeof(name)) {
		return NULL;
	}
	memcpy(name, p, (size_t)n);
	name[n] = 0;
	ps->p = p + n;
	return TX_Keep(ps->t, PyString_InternFromString(name));
}

/* an object with no list or dict in it (a constant the node may give again) */
static byte TX_Immutable(PyObject *o) {
	if (PyTuple_Check(o)) {
		for (int i = 0; i < PyTuple_Size(o); i++) {
			if (!TX_Immutable(PyTuple_GetItem(o, i))) {
				return 0;
			}
		}
		return 1;
	}
	return PyInt_Check(o) || PyLong_Check(o) || PyFloat_Check(o) || PyString_Check(o) || o == Py_None;
}

/* text through the reader of the scripts' literals: a constant node, a literal node read each time (lists and dicts
 * are new objects each time), NULL without an error when it is no literal */
static tcNode *TX_Literal(tcParse *ps, const char *text) {
	PyObject *v = Script_Literal(TcX_Main(), text);

	if (!v) {
		return NULL;
	}
	tcNode *x;
	if (TX_Immutable(v)) {
		x = TX_Node(ps, X_CONST);
		x->obj = TX_Keep(ps->t, v);
	} else {
		Py_DECREF(v);
		x = TX_Node(ps, X_LIT);
		x->obj = TX_Keep(ps->t, PyString_FromString(text));
	}
	return x;
}

/* the text p[0:n] as a literal (see TX_Literal) */
static tcNode *TX_LiteralN(tcParse *ps, const char *p, int n, const char *suffix) {
	char small[0x400];
	int len = n + (int)strlen(suffix) + 1;
	char *buf = len <= (int)sizeof(small) ? small : Mem_Alloc((uint32_t)len);

	memcpy(buf, p, (size_t)n);
	Str_Copy(buf + n, suffix, len - n);
	tcNode *x = TX_Literal(ps, buf);
	if (buf != small) {
		Mem_Free(buf);
	}
	return x;
}

/* the extent of a number token at p */
static const char *TX_NumberEnd(const char *p) {
	while ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'x' || *p == 'X' || *p == 'l' || *p == 'L'
		|| (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')
		|| ((*p == '+' || *p == '-') && (p[-1] == 'e' || p[-1] == 'E'))) {
		p++;
	}
	return p;
}

static tcNode *P_Test(tcParse *ps);
static tcNode *P_TestList(tcParse *ps);

/* the items of a tuple, list or backquote up to close: *comma set when a comma came */
static byte P_Items(tcParse *ps, char close, tcVec *vec, byte *comma) {
	*comma = 0;
	TX_Blanks(ps);
	while (*ps->p != close) {
		tcNode *x = P_Test(ps);
		if (!x) {
			return 0;
		}
		TX_Push(vec, x);
		TX_Blanks(ps);
		if (*ps->p == ',') {
			*comma = 1;
			ps->p++;
			TX_Blanks(ps);
		} else if (*ps->p != close) {
			TX_Syntax(ps->p);
			return 0;
		}
	}
	ps->p++;
	return 1;
}

static tcNode *P_Atom(tcParse *ps) {
	const char *p;

	TX_Blanks(ps);
	p = ps->p;
	if (*p == '"' || *p == '\'') {
		/* adjacent strings are one */
		PyObject *s = NULL;
		while (*p == '"' || *p == '\'') {
			const char *e = TX_StrEnd(p);
			tcNode *x = e ? TX_LiteralN(ps, p, (int)(e - p) + 1, "") : NULL;
			if (!x || x->kind != X_CONST) {
				Py_XDECREF(s);
				return TX_Syntax(p);
			}
			if (!s) {
				s = x->obj;
				Py_INCREF(s);
			} else {
				PyString_Concat(&s, x->obj);
			}
			ps->p = e + 1;
			TX_Blanks(ps);
			p = ps->p;
		}
		tcNode *x = TX_Node(ps, X_CONST);
		x->obj = TX_Keep(ps->t, s);
		return x;
	}
	if ((*p >= '0' && *p <= '9') || (*p == '.' && p[1] >= '0' && p[1] <= '9')) {
		const char *e = TX_NumberEnd(p + 1);
		tcNode *x = TX_LiteralN(ps, p, (int)(e - p), "");
		if (!x) {
			return TX_Syntax(p);
		}
		ps->p = e;
		return x;
	}
	if (*p == '(' || *p == '[' || *p == '{') {
		char close = *p == '(' ? ')' : *p == '[' ? ']' : '}';
		const char *e = TX_Close(p + 1, close);
		if (!e) {
			return TX_Syntax(p);
		}
		/* a plain literal is made as the compiler makes its constants */
		tcNode *x = TX_LiteralN(ps, p, (int)(e - p) + 1, "");
		if (x) {
			ps->p = e + 1;
			return x;
		}
		ps->p = p + 1;
		tcVec vec = { 0 };
		byte comma;
		if (close == '}') {
			TX_Blanks(ps);
			while (*ps->p != '}') {
				tcNode *k = P_Test(ps), *v = NULL;
				if (k && TX_Op(ps, ":", NULL)) {
					v = P_Test(ps);
				} else if (k) {
					TX_Syntax(ps->p);
				}
				if (!v) {
					TX_Drop(&vec);
					return NULL;
				}
				TX_Push(&vec, k);
				TX_Push(&vec, v);
				TX_Blanks(ps);
				if (*ps->p == ',') {
					ps->p++;
					TX_Blanks(ps);
				} else if (*ps->p != '}') {
					TX_Drop(&vec);
					return TX_Syntax(ps->p);
				}
			}
			ps->p++;
			x = TX_Node(ps, X_DICT);
		} else {
			if (!P_Items(ps, close, &vec, &comma)) {
				TX_Drop(&vec);
				return NULL;
			}
			if (close == ')' && vec.n == 1 && !comma) {
				x = vec.v[0];
				TX_Drop(&vec);
				return x;
			}
			x = TX_Node(ps, close == ')' ? X_TUPLE : X_LIST);
		}
		x->n = vec.n;
		x->kids = TX_Kids(ps, &vec);
		return x;
	}
	if (*p == '`') {
		ps->p = p + 1;
		tcNode *v = P_TestList(ps);
		if (!v) {
			return NULL;
		}
		if (!TX_Op(ps, "`", NULL)) {
			return TX_Syntax(ps->p);
		}
		tcNode *x = TX_Node(ps, X_REPR);
		x->a = v;
		return x;
	}
	PyObject *name = TX_Name(ps);
	if (!name) {
		return TX_Syntax(p);
	}
	tcNode *x;
	if (!strcmp(PyString_AsString(name), "inf") || !strcmp(PyString_AsString(name), "nan")) {
		x = TX_Node(ps, X_CONST);
		x->obj = TX_Keep(ps->t, PyFloat_FromDouble(*PyString_AsString(name) == 'i' ? HUGE_VAL : nan("")));
		return x;
	}
	x = TX_Node(ps, X_NAME);
	x->obj = name;
	return x;
}

/* the arguments of a call, ps->p at "(" */
static tcNode *P_Call(tcParse *ps, tcNode *fn) {
	const char *open = ps->p, *close = TX_Close(open + 1, ')');
	tcNode *x = TX_Node(ps, X_CALL);

	x->a = fn;
	if (!close) {
		return TX_Syntax(open);
	}
	/* all plain literals: the whole tuple made as the compiler makes its constants */
	const char *last = close - 1;
	while (last > open && (*last == ' ' || *last == '\t')) {
		last--;
	}
	tcNode *lit = last == open ? TX_LiteralN(ps, "()", 2, "")
		: TX_LiteralN(ps, open, (int)(last - open) + 1, *last == ',' ? ")" : ",)");
	if (lit && (lit->kind == X_LIT || PyTuple_Check(lit->obj))) {
		x->b = lit;
		x->flags = 1;
		ps->p = close + 1;
		return x;
	}
	tcVec pos = { 0 }, kwv = { 0 };
	PyObject *names = PyList_New(0);
	ps->p = open + 1;
	TX_Blanks(ps);
	while (*ps->p != ')') {
		const char *at = ps->p;
		PyObject *key = TX_Name(ps);
		if (key) {
			TX_Blanks(ps);
			if (*ps->p == '=' && ps->p[1] != '=') {
				ps->p++;
			} else {
				key = NULL;
				ps->p = at;
			}
		}
		tcNode *v = P_Test(ps);
		if (!v) {
			TX_Drop(&pos);
			TX_Drop(&kwv);
			Py_DECREF(names);
			return NULL;
		}
		if (key) {
			PyList_Append(names, key);
			TX_Push(&kwv, v);
		} else {
			TX_Push(&pos, v);
		}
		TX_Blanks(ps);
		if (*ps->p == ',') {
			ps->p++;
			TX_Blanks(ps);
		} else if (*ps->p != ')') {
			TX_Drop(&pos);
			TX_Drop(&kwv);
			Py_DECREF(names);
			return TX_Syntax(ps->p);
		}
	}
	ps->p++;
	x->n = pos.n;
	x->nkw = kwv.n;
	for (int i = 0; i < kwv.n; i++) {
		TX_Push(&pos, kwv.v[i]);
	}
	TX_Drop(&kwv);
	x->kids = TX_Kids(ps, &pos);
	x->obj = TX_Keep(ps->t, PyList_AsTuple(names));
	Py_DECREF(names);
	return x;
}

static tcNode *P_Postfix(tcParse *ps) {
	tcNode *x = P_Atom(ps);

	while (x) {
		TX_Blanks(ps);
		if (*ps->p == '.' && !(ps->p[1] >= '0' && ps->p[1] <= '9')) {
			ps->p++;
			PyObject *name = TX_Name(ps);
			if (!name) {
				return TX_Syntax(ps->p);
			}
			tcNode *y = TX_Node(ps, X_ATTR);
			y->a = x;
			y->obj = name;
			x = y;
		} else if (*ps->p == '(') {
			x = P_Call(ps, x);
		} else if (*ps->p == '[') {
			tcNode *lo = NULL, *hi = NULL;
			ps->p++;
			TX_Blanks(ps);
			if (*ps->p != ':' && !(lo = P_Test(ps))) {
				return NULL;
			}
			TX_Blanks(ps);
			byte slice = *ps->p == ':';
			if (slice) {
				ps->p++;
				TX_Blanks(ps);
				if (*ps->p != ']' && !(hi = P_Test(ps))) {
					return NULL;
				}
			}
			if (!TX_Op(ps, "]", NULL) || (!slice && !lo)) {
				return TX_Syntax(ps->p);
			}
			tcNode *y = TX_Node(ps, slice ? X_SLICE : X_INDEX);
			y->a = x;
			y->b = lo;
			y->c = hi;
			x = y;
		} else {
			break;
		}
	}
	return x;
}

static tcNode *TX_Bin(tcParse *ps, byte kind, byte op, tcNode *a, tcNode *b) {
	if (!b) {
		return NULL;
	}
	tcNode *x = TX_Node(ps, kind);
	x->op = op;
	x->a = a;
	x->b = b;
	return x;
}

static tcNode *P_Factor(tcParse *ps);

static tcNode *P_Power(tcParse *ps) {
	tcNode *x = P_Postfix(ps);

	if (x && TX_Op(ps, "**", NULL)) {
		return TX_Bin(ps, X_BIN, 'p', x, P_Factor(ps));
	}
	return x;
}

static tcNode *P_Factor(tcParse *ps) {
	TX_Blanks(ps);
	char c = *ps->p;
	if (c == '-' || c == '+' || c == '~') {
		ps->p++;
		tcNode *a = P_Factor(ps);
		if (!a) {
			return NULL;
		}
		tcNode *x = TX_Node(ps, X_UNARY);
		x->op = (byte)c;
		x->a = a;
		return x;
	}
	return P_Power(ps);
}

static tcNode *P_Term(tcParse *ps) {
	tcNode *x = P_Factor(ps);

	while (x) {
		if (TX_Op(ps, "*", "*=")) {
			x = TX_Bin(ps, X_BIN, '*', x, P_Factor(ps));
		} else if (TX_Op(ps, "/", "=")) {
			x = TX_Bin(ps, X_BIN, '/', x, P_Factor(ps));
		} else if (TX_Op(ps, "%", "=")) {
			x = TX_Bin(ps, X_BIN, '%', x, P_Factor(ps));
		} else {
			break;
		}
	}
	return x;
}

static tcNode *P_Arith(tcParse *ps) {
	tcNode *x = P_Term(ps);

	while (x) {
		if (TX_Op(ps, "+", "=")) {
			x = TX_Bin(ps, X_BIN, '+', x, P_Term(ps));
		} else if (TX_Op(ps, "-", "=")) {
			x = TX_Bin(ps, X_BIN, '-', x, P_Term(ps));
		} else {
			break;
		}
	}
	return x;
}

static tcNode *P_Shift(tcParse *ps) {
	tcNode *x = P_Arith(ps);

	while (x) {
		if (TX_Op(ps, "<<", "=")) {
			x = TX_Bin(ps, X_BIN, '<', x, P_Arith(ps));
		} else if (TX_Op(ps, ">>", "=")) {
			x = TX_Bin(ps, X_BIN, '>', x, P_Arith(ps));
		} else {
			break;
		}
	}
	return x;
}

static tcNode *P_BitAnd(tcParse *ps) {
	tcNode *x = P_Shift(ps);

	while (x && TX_Op(ps, "&", "=")) {
		x = TX_Bin(ps, X_BIN, '&', x, P_Shift(ps));
	}
	return x;
}

static tcNode *P_Xor(tcParse *ps) {
	tcNode *x = P_BitAnd(ps);

	while (x && TX_Op(ps, "^", "=")) {
		x = TX_Bin(ps, X_BIN, '^', x, P_BitAnd(ps));
	}
	return x;
}

static tcNode *P_BitOr(tcParse *ps) {
	tcNode *x = P_Xor(ps);

	while (x && TX_Op(ps, "|", "=")) {
		x = TX_Bin(ps, X_BIN, '|', x, P_Xor(ps));
	}
	return x;
}

/* a comparison operator next: read it, its C_ code; -1 when there is none */
static int P_CmpOp(tcParse *ps) {
	const char *save = ps->p;

	if (TX_Op(ps, "<>", NULL) || TX_Op(ps, "!=", NULL)) {
		return C_NE;
	}
	if (TX_Op(ps, "<=", NULL)) {
		return C_LE;
	}
	if (TX_Op(ps, ">=", NULL)) {
		return C_GE;
	}
	if (TX_Op(ps, "==", NULL)) {
		return C_EQ;
	}
	if (TX_Op(ps, "<", "<")) {
		return C_LT;
	}
	if (TX_Op(ps, ">", ">")) {
		return C_GT;
	}
	if (TX_Kw(ps, "in")) {
		return C_IN;
	}
	if (TX_Kw(ps, "not")) {
		if (TX_Kw(ps, "in")) {
			return C_NOTIN;
		}
		ps->p = save;
		return -1;
	}
	if (TX_Kw(ps, "is")) {
		return TX_Kw(ps, "not") ? C_ISNOT : C_IS;
	}
	return -1;
}

static tcNode *P_Cmp(tcParse *ps) {
	tcNode *x = P_BitOr(ps);
	int op;

	if (!x || (op = P_CmpOp(ps)) < 0) {
		return x;
	}
	tcVec vec = { 0 };
	byte ops[0x40];
	int nops = 0;
	TX_Push(&vec, x);
	do {
		tcNode *y = P_BitOr(ps);
		if (!y || nops >= (int)sizeof(ops)) {
			TX_Drop(&vec);
			return y ? TX_Syntax(ps->p) : NULL;
		}
		ops[nops++] = (byte)op;
		TX_Push(&vec, y);
	} while ((op = P_CmpOp(ps)) >= 0);
	x = TX_Node(ps, X_CMP);
	x->n = vec.n;
	x->kids = TX_Kids(ps, &vec);
	x->ops = TX_Alloc(ps->t, (size_t)nops);
	memcpy(x->ops, ops, (size_t)nops);
	return x;
}

static tcNode *P_Not(tcParse *ps) {
	if (TX_Kw(ps, "not")) {
		tcNode *a = P_Not(ps);
		if (!a) {
			return NULL;
		}
		tcNode *x = TX_Node(ps, X_UNARY);
		x->op = '!';
		x->a = a;
		return x;
	}
	return P_Cmp(ps);
}

static tcNode *P_And(tcParse *ps) {
	tcNode *x = P_Not(ps);

	while (x && TX_Kw(ps, "and")) {
		x = TX_Bin(ps, X_AND, 0, x, P_Not(ps));
	}
	return x;
}

static tcNode *P_Or(tcParse *ps) {
	tcNode *x = P_And(ps);

	while (x && TX_Kw(ps, "or")) {
		x = TX_Bin(ps, X_OR, 0, x, P_And(ps));
	}
	return x;
}

/* lambda [params]: body, after the keyword */
static tcNode *P_Lambda(tcParse *ps) {
	tcVec defs = { 0 };
	PyObject *names = PyList_New(0);
	PyObject *star = NULL, *starstar = NULL;
	int np = 0;

	TX_Blanks(ps);
	while (*ps->p != ':') {
		PyObject *name;
		if (TX_Op(ps, "**", NULL)) {
			starstar = TX_Name(ps);
			name = starstar;
		} else if (TX_Op(ps, "*", NULL)) {
			star = TX_Name(ps);
			name = star;
		} else {
			name = TX_Name(ps);
			if (name && !star && !starstar) {
				PyList_Append(names, name);
				np++;
				TX_Blanks(ps);
				if (*ps->p == '=' && ps->p[1] != '=') {
					ps->p++;
					tcNode *d = P_Test(ps);
					if (!d) {
						TX_Drop(&defs);
						Py_DECREF(names);
						return NULL;
					}
					TX_Push(&defs, d);
				} else if (defs.n) {
					name = NULL;
				}
			} else {
				name = NULL;
			}
		}
		TX_Blanks(ps);
		if (!name || (*ps->p != ',' && *ps->p != ':')) {
			TX_Drop(&defs);
			Py_DECREF(names);
			return TX_Syntax(ps->p);
		}
		if (*ps->p == ',') {
			ps->p++;
			TX_Blanks(ps);
		}
	}
	ps->p++;
	tcNode *body = P_Test(ps);
	if (!body) {
		TX_Drop(&defs);
		Py_DECREF(names);
		return NULL;
	}
	tcNode *x = TX_Node(ps, X_LAMBDA);
	if (star) {
		PyList_Append(names, star);
		x->flags |= 1;
	}
	if (starstar) {
		PyList_Append(names, starstar);
		x->flags |= 2;
	}
	x->n = np;
	x->nkw = defs.n;
	x->kids = TX_Kids(ps, &defs);
	x->a = body;
	x->obj = TX_Keep(ps->t, PyList_AsTuple(names));
	Py_DECREF(names);
	return x;
}

static tcNode *P_Test(tcParse *ps) {
	if (++ps->depth > TX_PARSE_DEPTH) {
		return TX_Syntax(ps->p);
	}
	tcNode *x;
	if (TX_Kw(ps, "lambda")) {
		x = P_Lambda(ps);
	} else {
		x = P_Or(ps);
		if (x && TX_Kw(ps, "if")) {
			tcNode *c = P_Or(ps), *o = NULL;
			if (c && TX_Kw(ps, "else")) {
				o = P_Test(ps);
			} else if (c) {
				TX_Syntax(ps->p);
			}
			if (!o) {
				return NULL;
			}
			tcNode *y = TX_Node(ps, X_IF);
			y->a = x;
			y->b = c;
			y->c = o;
			x = y;
		}
	}
	ps->depth--;
	return x;
}

/* a test, or a tuple of tests without brackets ("a, b"); stops before ";", "=", "`", ")" and the end */
static tcNode *P_TestList(tcParse *ps) {
	tcNode *x = P_Test(ps);

	TX_Blanks(ps);
	if (!x || *ps->p != ',') {
		return x;
	}
	tcVec vec = { 0 };
	TX_Push(&vec, x);
	while (*ps->p == ',') {
		ps->p++;
		TX_Blanks(ps);
		if (!*ps->p || strchr(";=`)", *ps->p)) {
			break;
		}
		tcNode *y = P_Test(ps);
		if (!y) {
			TX_Drop(&vec);
			return NULL;
		}
		TX_Push(&vec, y);
		TX_Blanks(ps);
	}
	x = TX_Node(ps, X_TUPLE);
	x->n = vec.n;
	x->kids = TX_Kids(ps, &vec);
	return x;
}

static byte TX_IsTarget(tcNode *x) {
	if (x->kind == X_TUPLE || x->kind == X_LIST) {
		for (int i = 0; i < x->n; i++) {
			if (!TX_IsTarget(x->kids[i])) {
				return 0;
			}
		}
		return 1;
	}
	return x->kind == X_NAME || x->kind == X_ATTR || x->kind == X_INDEX || x->kind == X_SLICE;
}

static tcNode *P_Stmt(tcParse *ps) {
	tcNode *x;

	if (TX_Kw(ps, "pass")) {
		return TX_Node(ps, S_PASS);
	}
	if (TX_Kw(ps, "del")) {
		tcNode *t = P_TestList(ps);
		if (!t) {
			return NULL;
		}
		if (!TX_IsTarget(t)) {
			return TX_Syntax(ps->p);
		}
		x = TX_Node(ps, S_DEL);
		x->a = t;
		return x;
	}
	if (TX_Kw(ps, "import")) {
		PyObject *names = PyList_New(0);
		do {
			char full[0x100];
			int n = 0;
			TX_Blanks(ps);
			while (TX_NameChar(*ps->p, 0) || *ps->p == '.') {
				if (n < (int)sizeof(full) - 1) {
					full[n++] = *ps->p;
				}
				ps->p++;
			}
			full[n] = 0;
			if (!n) {
				Py_DECREF(names);
				return TX_Syntax(ps->p);
			}
			PyObject *s = PyString_FromString(full);
			PyList_Append(names, s);
			Py_XDECREF(s);
		} while (TX_Op(ps, ",", NULL));
		x = TX_Node(ps, S_IMPORT);
		x->obj = TX_Keep(ps->t, PyList_AsTuple(names));
		Py_DECREF(names);
		return x;
	}
	tcNode *e = P_TestList(ps);
	if (!e) {
		return NULL;
	}
	TX_Blanks(ps);
	if (*ps->p != '=' || ps->p[1] == '=') {
		x = TX_Node(ps, S_EXPR);
		x->a = e;
		return x;
	}
	tcVec vec = { 0 };
	while (*ps->p == '=' && ps->p[1] != '=') {
		if (!TX_IsTarget(e)) {
			TX_Drop(&vec);
			return TX_Syntax(ps->p);
		}
		TX_Push(&vec, e);
		ps->p++;
		e = P_TestList(ps);
		if (!e) {
			TX_Drop(&vec);
			return NULL;
		}
		TX_Blanks(ps);
	}
	x = TX_Node(ps, S_ASSIGN);
	x->n = vec.n;
	x->kids = TX_Kids(ps, &vec);
	x->a = e;
	return x;
}

/* the statements of a line: an S_LIST node; NULL with the error set */
static tcNode *P_Program(tcParse *ps) {
	tcVec vec = { 0 };
	byte semi = 0;

	TX_Blanks(ps);
	while (*ps->p) {
		tcNode *s = P_Stmt(ps);
		if (!s) {
			TX_Drop(&vec);
			return NULL;
		}
		TX_Push(&vec, s);
		TX_Blanks(ps);
		if (*ps->p == ';') {
			semi = 1;
			ps->p++;
			TX_Blanks(ps);
		} else if (*ps->p) {
			TX_Drop(&vec);
			return TX_Syntax(ps->p);
		}
	}
	tcNode *x = TX_Node(ps, S_LIST);
	x->flags = vec.n == 1 && !semi && vec.v[0]->kind == S_EXPR;
	x->n = vec.n;
	x->kids = TX_Kids(ps, &vec);
	return x;
}

/* ---- names ---- */

PyObject *TcX_Main(void) {
	PyObject *m = PyImport_AddModule("__main__");
	return m ? PyModule_GetDict(m) : NULL;
}

static PyObject *TP_Function(const char *name);
static PyObject *TX_AttrTwin(const char *name, PyObject *builtin);

/* a name the scopes of the line do not give: the builtins (getattr, setattr, hasattr and delattr as the twins that
 * reach the pack modules through the registry), the channel's functions, the modules (new reference) */
static PyObject *TX_Global(const char *name) {
	PyObject *b = PyImport_AddModule("__builtin__");
	PyObject *v = b ? PyDict_GetItemString(PyModule_GetDict(b), (char *)name) : NULL;
	PyObject *twin = v ? TX_AttrTwin(name, v) : NULL;

	if (twin) {
		return twin;
	}
	if (!v) {
		if (!strncmp(name, "tc_", 3) && (v = TP_Function(name)) != NULL) {
			return v;
		}
		v = Script_ModuleLoaded(name);
	}
	if (v) {
		Py_INCREF(v);
		return v;
	}
	v = Script_Module(name);
	if (!v && PyErr_ExceptionMatches(PyExc_ImportError)) {
		/* what LOAD_NAME raises */
		PyErr_Clear();
		PyErr_SetString(PyExc_NameError, name);
	}
	return v;
}

PyObject *TcX_Name(const char *name) {
	PyObject *v = PyDict_GetItemString(TcX_Main(), (char *)name);

	if (v) {
		Py_INCREF(v);
		return v;
	}
	return TX_Global(name);
}

static PyObject *X_Name(PyObject *name, tcScope *s) {
	PyObject *v = s->locals ? PyDict_GetItem(s->locals, name) : NULL;

	if (!v && !strcmp(PyString_AsString(name), "__globals__")) {
		/* the globals the line or lambda runs in (helpers set their module variables through it) */
		Py_INCREF(s->globals);
		return s->globals;
	}
	for (PyObject *c = s->chain; !v && c && PyTuple_Check(c); c = PyTuple_GetItem(c, 1)) {
		v = PyDict_GetItem(PyTuple_GetItem(c, 0), name);
	}
	if (!v) {
		v = s->inMod ? ModVar_Get(&s->mv, PyString_AsString(name)) : PyDict_GetItem(s->globals, name);
	}
	if (v) {
		Py_INCREF(v);
		return v;
	}
	return TX_Global(PyString_AsString(name));
}

/* name = v in the scope: the locals of a lambda, else the globals (a variable of a pack module through the registry) */
static byte X_SetName(tcScope *s, PyObject *name, PyObject *v) {
	if (!s->locals && s->inMod) {
		return ModVar_Set(&s->mv, PyString_AsString(name), v) == 0;
	}
	return PyDict_SetItem(s->locals ? s->locals : s->globals, name, v) == 0;
}

/* the scope of the top level of a line or of a lambda that runs in globals */
static void X_Scope(tcScope *s, PyObject *locals, PyObject *chain, PyObject *globals) {
	s->locals = locals;
	s->chain = chain;
	s->globals = globals;
	s->inMod = ModVar_OfDict(globals, &s->mv);
}

/* ---- running ---- */

static PyObject *X_Eval(tcNode *x, tcScope *s);

/* set while a check line runs: golden's checks (tools/test/golden) read and set attributes with the getattr and setattr
 * builtins */
static byte tx_builtinAttr;

/* the error raised when o.name or o.name = v failed without one (an object that gives NULL or -1 without an error): the
 * interpreter's (ceval.c), in a check line the one of the getattr and setattr builtins */
static void X_NoError(void) {
	if (!PyErr_Occurred()) {
		PyErr_SetString(PyExc_SystemError, tx_builtinAttr ? "NULL result without error in call_object"
			: "error return without exception set");
	}
}

/* o.name: the variable of a pack module through the registry, else the attribute of the object */
static PyObject *TX_GetAttr(PyObject *o, PyObject *name) {
	modVarRef_t mv;

	if (ModVar_Of(o, &mv)) {
		return ModVar_Attr(&mv, PyString_AsString(name));
	}
	return PyObject_GetAttr(o, name);
}

/* o.name = v (v NULL: del o.name), the variable of a pack module through the registry: 0, -1 with the error set */
static int TX_SetAttr(PyObject *o, PyObject *name, PyObject *v) {
	modVarRef_t mv;

	if (ModVar_Of(o, &mv)) {
		return ModVar_SetAttr(&mv, PyString_AsString(name), v);
	}
	return PyObject_SetAttr(o, name, v);
}

static PyObject *X_GetAttr(PyObject *o, PyObject *name) {
	PyObject *v = TX_GetAttr(o, name);

	if (!v) {
		X_NoError();
	}
	return v;
}

/* the int of a slice bound as SLICE+n takes it: none is lo or hi */
static byte X_Index(PyObject *v, int none, int *out) {
	if (!v) {
		*out = none;
		return 1;
	}
	if (!PyInt_Check(v)) {
		PyErr_SetString(PyExc_TypeError, "slice index must be int");
		return 0;
	}
	*out = (int)PyInt_AsLong(v);
	return 1;
}

/* getattr, hasattr, setattr and delattr of __builtin__ (bltinmodule.c of 1.5.2: the same argument formats, errors and
 * results), the variables of the pack modules through the registry */
static PyObject *TB_GetAttr(PyObject *self, PyObject *args) {
	PyObject *o, *name, *dflt = NULL;

	(void)self;
	if (!PyArg_ParseTuple(args, "OS|O:getattr", &o, &name, &dflt)) {
		return NULL;
	}
	PyObject *v = TX_GetAttr(o, name);
	if (!v && dflt) {
		PyErr_Clear();
		Py_INCREF(dflt);
		v = dflt;
	}
	return v;
}

static PyObject *TB_HasAttr(PyObject *self, PyObject *args) {
	PyObject *o, *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "OS:hasattr", &o, &name)) {
		return NULL;
	}
	PyObject *v = TX_GetAttr(o, name);
	if (!v) {
		PyErr_Clear();
		Py_INCREF(Py_False);
		return Py_False;
	}
	Py_DECREF(v);
	Py_INCREF(Py_True);
	return Py_True;
}

static PyObject *TB_SetAttr(PyObject *self, PyObject *args) {
	PyObject *o, *name, *value;

	(void)self;
	if (!PyArg_ParseTuple(args, "OSO:setattr", &o, &name, &value) || TX_SetAttr(o, name, value) != 0) {
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *TB_DelAttr(PyObject *self, PyObject *args) {
	PyObject *o, *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "OS:delattr", &o, &name) || TX_SetAttr(o, name, NULL) != 0) {
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef tx_attrTwins[] = {
	{ "getattr", TB_GetAttr, METH_VARARGS, NULL },
	{ "hasattr", TB_HasAttr, METH_VARARGS, NULL },
	{ "setattr", TB_SetAttr, METH_VARARGS, NULL },
	{ "delattr", TB_DelAttr, METH_VARARGS, NULL },
};

/* the twin of the builtin function of that name (with its doc), NULL for every other name */
static PyObject *TX_AttrTwin(const char *name, PyObject *builtin) {
	if (!PyCFunction_Check(builtin)) {
		return NULL;
	}
	for (size_t i = 0; i < sizeof(tx_attrTwins) / sizeof(tx_attrTwins[0]); i++) {
		if (!strcmp(name, tx_attrTwins[i].ml_name)) {
			tx_attrTwins[i].ml_doc = ((PyCFunctionObject *)builtin)->m_ml->ml_doc;
			return PyCFunction_New(&tx_attrTwins[i], NULL);
		}
	}
	return NULL;
}

/* the bounds of a slice node: 0 with the error set */
static byte X_Bounds(tcNode *x, tcScope *s, int *lo, int *hi) {
	PyObject *a = x->b ? X_Eval(x->b, s) : NULL, *b = NULL;
	byte ok = 0;

	if (!x->b || a) {
		b = x->c ? X_Eval(x->c, s) : NULL;
		ok = (!x->c || b) && X_Index(a, 0, lo) && X_Index(b, INT_MAX, hi);
	}
	Py_XDECREF(a);
	Py_XDECREF(b);
	return ok;
}

/* the nodes as a new list; NULL with the error set */
static PyObject *X_List(tcNode **kids, int n, tcScope *s) {
	PyObject *list = PyList_New(n);

	for (int i = 0; list && i < n; i++) {
		PyObject *v = X_Eval(kids[i], s);
		if (!v) {
			Py_DECREF(list);
			return NULL;
		}
		PyList_SetItem(list, i, v);
	}
	return list;
}

static PyObject *X_Call(tcNode *x, tcScope *s) {
	PyObject *f = X_Eval(x->a, s), *args = NULL, *kw = NULL, *v = NULL;

	if (!f) {
		return NULL;
	}
	if (x->flags & 1) {
		args = X_Eval(x->b, s);
	} else {
		PyObject *list = X_List(x->kids, x->n, s);
		args = list ? PyList_AsTuple(list) : NULL;
		Py_XDECREF(list);
		if (args && x->nkw) {
			kw = PyDict_New();
			for (int i = 0; kw && i < x->nkw; i++) {
				PyObject *a = X_Eval(x->kids[x->n + i], s);
				if (!a || PyDict_SetItem(kw, PyTuple_GetItem(x->obj, i), a) != 0) {
					Py_XDECREF(a);
					Py_DECREF(kw);
					kw = NULL;
					Py_DECREF(args);
					args = NULL;
					break;
				}
				Py_DECREF(a);
			}
		}
	}
	if (args) {
		v = PyEval_CallObjectWithKeywords(f, args, kw);
		/* a value with an error left set is the error (as the next check of the interpreter raises it) */
		if (v && PyErr_Occurred()) {
			Py_DECREF(v);
			v = NULL;
		}
	}
	Py_XDECREF(args);
	Py_XDECREF(kw);
	Py_DECREF(f);
	return v;
}

static PyObject *X_Binary(byte op, PyObject *a, PyObject *b) {
	switch (op) {
	case '+':
		return PyNumber_Add(a, b);
	case '-':
		return PyNumber_Subtract(a, b);
	case '*':
		return PyNumber_Multiply(a, b);
	case '/':
		return PyNumber_Divide(a, b);
	case '%':
		return PyNumber_Remainder(a, b);
	case 'p':
		return PyNumber_Power(a, b, Py_None);
	case '<':
		return PyNumber_Lshift(a, b);
	case '>':
		return PyNumber_Rshift(a, b);
	case '&':
		return PyNumber_And(a, b);
	case '^':
		return PyNumber_Xor(a, b);
	default:
		return PyNumber_Or(a, b);
	}
}

/* cmp_outcome of ceval.c: 1, 0, -1 with the error set */
static int X_Compare(byte op, PyObject *v, PyObject *w) {
	int r;

	switch (op) {
	case C_IS:
		return v == w;
	case C_ISNOT:
		return v != w;
	case C_IN:
	case C_NOTIN:
		r = PySequence_Contains(w, v);
		return r < 0 ? -1 : op == C_IN ? r : !r;
	default:
		r = PyObject_Compare(v, w);
		if (r && PyErr_Occurred()) {
			return -1;
		}
		switch (op) {
		case C_LT:
			return r < 0;
		case C_LE:
			return r <= 0;
		case C_EQ:
			return r == 0;
		case C_NE:
			return r != 0;
		case C_GT:
			return r > 0;
		default:
			return r >= 0;
		}
	}
}

static PyObject *X_Lambda(tcNode *x, tcScope *s);

static PyObject *X_Eval(tcNode *x, tcScope *s) {
	PyObject *a, *b, *v = NULL;
	int t;

	if (++tx_depth > TX_EVAL_DEPTH) {
		tx_depth--;
		PyErr_SetString(PyExc_RuntimeError, "maximum recursion depth exceeded");
		return NULL;
	}
	switch (x->kind) {
	case X_CONST:
		v = x->obj;
		Py_INCREF(v);
		break;
	case X_LIT:
		v = Script_Literal(TcX_Main(), PyString_AsString(x->obj));
		if (!v) {
			TX_Syntax(PyString_AsString(x->obj));
		}
		break;
	case X_NAME:
		v = X_Name(x->obj, s);
		break;
	case X_ATTR:
		a = X_Eval(x->a, s);
		v = a ? X_GetAttr(a, x->obj) : NULL;
		Py_XDECREF(a);
		break;
	case X_CALL:
		v = X_Call(x, s);
		break;
	case X_INDEX:
		a = X_Eval(x->a, s);
		b = a ? X_Eval(x->b, s) : NULL;
		v = b ? PyObject_GetItem(a, b) : NULL;
		Py_XDECREF(a);
		Py_XDECREF(b);
		break;
	case X_SLICE: {
		int lo, hi;
		a = X_Eval(x->a, s);
		if (a && X_Bounds(x, s, &lo, &hi)) {
			v = PySequence_GetSlice(a, lo, hi);
		}
		Py_XDECREF(a);
		break;
	}
	case X_TUPLE:
		a = X_List(x->kids, x->n, s);
		v = a ? PyList_AsTuple(a) : NULL;
		Py_XDECREF(a);
		break;
	case X_LIST:
		v = X_List(x->kids, x->n, s);
		break;
	case X_DICT:
		v = PyDict_New();
		for (int i = 0; v && i + 1 < x->n; i += 2) {
			a = X_Eval(x->kids[i], s);
			b = a ? X_Eval(x->kids[i + 1], s) : NULL;
			if (!b || PyDict_SetItem(v, a, b) != 0) {
				Py_DECREF(v);
				v = NULL;
			}
			Py_XDECREF(a);
			Py_XDECREF(b);
		}
		break;
	case X_UNARY:
		a = X_Eval(x->a, s);
		if (!a) {
			break;
		}
		if (x->op == '!') {
			t = PyObject_IsTrue(a);
			v = t < 0 ? NULL : PyInt_FromLong(!t);
		} else {
			v = x->op == '-' ? PyNumber_Negative(a) : x->op == '+' ? PyNumber_Positive(a) : PyNumber_Invert(a);
		}
		Py_DECREF(a);
		break;
	case X_BIN:
		a = X_Eval(x->a, s);
		b = a ? X_Eval(x->b, s) : NULL;
		v = b ? X_Binary(x->op, a, b) : NULL;
		Py_XDECREF(a);
		Py_XDECREF(b);
		break;
	case X_CMP:
		a = X_Eval(x->kids[0], s);
		for (int i = 0; a && i + 1 < x->n; i++) {
			b = X_Eval(x->kids[i + 1], s);
			t = b ? X_Compare(x->ops[i], a, b) : -1;
			Py_DECREF(a);
			a = b;
			if (t <= 0 || i + 2 == x->n) {
				v = t < 0 ? NULL : PyInt_FromLong(t);
				break;
			}
		}
		Py_XDECREF(a);
		break;
	case X_AND:
	case X_OR:
		v = X_Eval(x->a, s);
		t = v ? PyObject_IsTrue(v) : -1;
		if (t < 0) {
			Py_XDECREF(v);
			v = NULL;
		} else if (t == (x->kind == X_AND)) {
			Py_DECREF(v);
			v = X_Eval(x->b, s);
		}
		break;
	case X_IF:
		a = X_Eval(x->b, s);
		t = a ? PyObject_IsTrue(a) : -1;
		Py_XDECREF(a);
		v = t < 0 ? NULL : X_Eval(t ? x->a : x->c, s);
		break;
	case X_LAMBDA:
		v = X_Lambda(x, s);
		break;
	case X_REPR:
		a = X_Eval(x->a, s);
		v = a ? PyObject_Repr(a) : NULL;
		Py_XDECREF(a);
		break;
	default:
		TX_Syntax("?");
		break;
	}
	tx_depth--;
	return v;
}

/* ---- lambdas: C functions that run their node ---- */

typedef struct tcFunc {
	tcTree *t;
	tcNode *x;
} tcFunc;

static void TX_FuncFree(void *p) {
	tcFunc *f = p;
	tcTree *t = f->t;

	Mem_Free(f);
	TX_Release(t);
}

static PyObject *TX_ArgError(const char *fmt, int a, int b) {
	char msg[0x80];

	snprintf(msg, sizeof(msg), fmt, a, b);
	PyErr_SetString(PyExc_TypeError, msg);
	return NULL;
}

/* self: (function, defaults, globals, chain); the parameters bound as the interpreter binds them */
static PyObject *TX_LambdaCall(PyObject *self, PyObject *args, PyObject *kw) {
	tcFunc *f = PyCObject_AsVoidPtr(PyTuple_GetItem(self, 0));
	PyObject *defaults = PyTuple_GetItem(self, 1), *chain = PyTuple_GetItem(self, 3);
	tcNode *x = f->x;
	PyObject *names = x->obj, *locals = PyDict_New(), *v = NULL, *kwd = NULL;
	int np = x->n, na = args ? PyTuple_Size(args) : 0, extra = np;

	if (!locals) {
		return NULL;
	}
	for (int i = 0; i < np && i < na; i++) {
		PyDict_SetItem(locals, PyTuple_GetItem(names, i), PyTuple_GetItem(args, i));
	}
	if (x->flags & 1) {
		PyObject *rest = PySequence_GetSlice(args, np, INT_MAX);
		if (rest) {
			PyDict_SetItem(locals, PyTuple_GetItem(names, extra++), rest);
			Py_DECREF(rest);
		}
	} else if (na > np) {
		Py_DECREF(locals);
		return TX_ArgError("too many arguments; expected %d, got %d", np, na);
	}
	if (x->flags & 2) {
		kwd = PyDict_New();
		PyDict_SetItem(locals, PyTuple_GetItem(names, extra), kwd);
	}
	int pos = 0;
	PyObject *k, *a;
	while (kw && PyDict_Next(kw, &pos, &k, &a)) {
		int i;
		for (i = 0; i < np && PyObject_Compare(k, PyTuple_GetItem(names, i)) != 0; i++) {
		}
		if (i < np) {
			if (i < na || PyDict_GetItem(locals, k)) {
				PyErr_SetString(PyExc_TypeError, "keyword parameter redefined");
				goto done;
			}
			PyDict_SetItem(locals, k, a);
		} else if (kwd) {
			PyDict_SetItem(kwd, k, a);
		} else {
			char msg[0x100];
			snprintf(msg, sizeof(msg), "unexpected keyword argument: %.80s", PyString_Check(k) ? PyString_AsString(k) : "?");
			PyErr_SetString(PyExc_TypeError, msg);
			goto done;
		}
	}
	for (int i = 0; i < np; i++) {
		PyObject *name = PyTuple_GetItem(names, i);
		if (PyDict_GetItem(locals, name)) {
			continue;
		}
		int d = i - (np - x->nkw);
		if (d < 0) {
			TX_ArgError("not enough arguments; expected %d, got %d", np - x->nkw, na);
			goto done;
		}
		PyDict_SetItem(locals, name, PyTuple_GetItem(defaults, d));
	}
	tcScope s;
	X_Scope(&s, locals, chain == Py_None ? NULL : chain, PyTuple_GetItem(self, 2));
	tcTree *was = tx_tree;
	tx_tree = f->t;
	v = X_Eval(x->a, &s);
	tx_tree = was;
done:
	Py_XDECREF(kwd);
	Py_DECREF(locals);
	return v;
}

static PyMethodDef tx_lambdaDef = { "<lambda>", (PyCFunction)(void (*)(void))TX_LambdaCall, METH_VARARGS | METH_KEYWORDS, NULL };

static PyObject *X_Lambda(tcNode *x, tcScope *s) {
	PyObject *defaults = X_List(x->kids, x->nkw, s);

	if (!defaults) {
		return NULL;
	}
	PyObject *deft = PyList_AsTuple(defaults);
	Py_DECREF(defaults);
	/* the scopes around: the locals of the lambda that runs and those around it */
	PyObject *chain = s->locals ? Py_BuildValue("(OO)", s->locals, s->chain ? s->chain : Py_None)
		: (Py_INCREF(s->chain ? s->chain : Py_None), s->chain ? s->chain : Py_None);
	tcFunc *f = Mem_Alloc(sizeof(tcFunc));
	f->t = tx_tree;
	f->x = x;
	tx_tree->refs++;
	PyObject *cf = PyCObject_FromVoidPtr(f, TX_FuncFree);
	if (!cf) {
		TX_FuncFree(f);
	}
	PyObject *self = cf && deft && chain ? Py_BuildValue("(OOOO)", cf, deft, s->globals, chain) : NULL;
	PyObject *v = self ? PyCFunction_New(&tx_lambdaDef, self) : NULL;
	Py_XDECREF(self);
	Py_XDECREF(cf);
	Py_XDECREF(deft);
	Py_XDECREF(chain);
	return v;
}

/* ---- statements ---- */

static byte X_Store(tcNode *t, PyObject *v, tcScope *s) {
	PyObject *o, *k;
	int r = -1, lo, hi;

	switch (t->kind) {
	case X_NAME:
		return X_SetName(s, t->obj, v);
	case X_ATTR:
		o = X_Eval(t->a, s);
		if (o) {
			r = TX_SetAttr(o, t->obj, v);
			if (r != 0) {
				X_NoError();
			}
		}
		Py_XDECREF(o);
		return r == 0;
	case X_INDEX:
		o = X_Eval(t->a, s);
		k = o ? X_Eval(t->b, s) : NULL;
		r = k ? PyObject_SetItem(o, k, v) : -1;
		Py_XDECREF(o);
		Py_XDECREF(k);
		return r == 0;
	case X_SLICE:
		o = X_Eval(t->a, s);
		if (o && X_Bounds(t, s, &lo, &hi)) {
			r = PySequence_SetSlice(o, lo, hi, v);
		}
		Py_XDECREF(o);
		return r == 0;
	default: {
		int n = PySequence_Length(v);
		if (n < 0) {
			return 0;
		}
		if (n != t->n) {
			PyErr_SetString(PyExc_ValueError, t->kind == X_LIST ? "unpack list of wrong size" : "unpack tuple of wrong size");
			return 0;
		}
		for (int i = 0; i < n; i++) {
			PyObject *item = PySequence_GetItem(v, i);
			byte ok = item && X_Store(t->kids[i], item, s);
			Py_XDECREF(item);
			if (!ok) {
				return 0;
			}
		}
		return 1;
	}
	}
}

static byte X_Delete(tcNode *t, tcScope *s) {
	PyObject *o, *k;
	int r = -1, lo, hi;

	switch (t->kind) {
	case X_NAME:
		if ((!s->locals && s->inMod ? ModVar_Del(&s->mv, PyString_AsString(t->obj)) :
			PyDict_DelItem(s->locals ? s->locals : s->globals, t->obj)) != 0) {
			PyErr_Clear();
			PyErr_SetObject(PyExc_NameError, t->obj);
			return 0;
		}
		return 1;
	case X_ATTR:
		o = X_Eval(t->a, s);
		r = o ? TX_SetAttr(o, t->obj, NULL) : -1;
		Py_XDECREF(o);
		return r == 0;
	case X_INDEX:
		o = X_Eval(t->a, s);
		k = o ? X_Eval(t->b, s) : NULL;
		r = k ? PyObject_DelItem(o, k) : -1;
		Py_XDECREF(o);
		Py_XDECREF(k);
		return r == 0;
	case X_SLICE:
		o = X_Eval(t->a, s);
		if (o && X_Bounds(t, s, &lo, &hi)) {
			r = PySequence_DelSlice(o, lo, hi);
		}
		Py_XDECREF(o);
		return r == 0;
	default:
		for (int i = 0; i < t->n; i++) {
			if (!X_Delete(t->kids[i], s)) {
				return 0;
			}
		}
		return 1;
	}
}

/* one statement; with value set the value of an expression statement (new reference) */
static byte X_Exec(tcNode *st, tcScope *s, PyObject **value) {
	PyObject *v;

	switch (st->kind) {
	case S_EXPR:
		v = X_Eval(st->a, s);
		if (value) {
			*value = v;
		} else {
			Py_XDECREF(v);
		}
		return v != NULL;
	case S_ASSIGN:
		v = X_Eval(st->a, s);
		for (int i = 0; v && i < st->n; i++) {
			if (!X_Store(st->kids[i], v, s)) {
				Py_DECREF(v);
				v = NULL;
			}
		}
		Py_XDECREF(v);
		return v != NULL;
	case S_DEL:
		return X_Delete(st->a, s);
	case S_IMPORT:
		for (int i = 0; i < PyTuple_Size(st->obj); i++) {
			PyObject *name = PyTuple_GetItem(st->obj, i);
			PyObject *m = Script_Module(PyString_AsString(name));
			byte ok = m && X_SetName(s, name, m);
			Py_XDECREF(m);
			if (!ok) {
				return 0;
			}
		}
		return 1;
	default:
		return 1;
	}
}

/* the statements of a line read: the tree (tx_tree set to it) and its S_LIST node; NULL with the error set */
static tcNode *TX_Read(const char *line, tcTree **tree) {
	tcParse ps = { line, TX_New(), 0 };
	tcNode *prog = ps.t->objs ? P_Program(&ps) : NULL;

	if (!prog) {
		TX_Release(ps.t);
		*tree = NULL;
		return NULL;
	}
	*tree = ps.t;
	return prog;
}

/* runs prog of tree in globals: 0 with the error set; *last the value of a last expression statement or NULL */
static byte TX_RunTree(tcTree *tree, tcNode *prog, PyObject *globals, PyObject **last) {
	tcTree *was = tx_tree;
	tcScope s;
	byte ok = 1;

	X_Scope(&s, NULL, NULL, globals);

	tx_tree = tree;
	if (last) {
		*last = NULL;
	}
	for (int i = 0; ok && i < prog->n; i++) {
		if (last) {
			Py_XDECREF(*last);
			*last = NULL;
		}
		ok = X_Exec(prog->kids[i], &s, last);
	}
	tx_tree = was;
	return ok;
}

PyObject *TcX_Eval(const char *line) {
	tcTree *tree;
	tcNode *prog = TX_Read(line, &tree);
	PyObject *v = NULL;

	if (prog && !(prog->flags & 1)) {
		TX_Syntax(line);
	} else if (prog) {
		TX_RunTree(tree, prog, TcX_Main(), &v);
	}
	TX_Release(tree);
	return v;
}

PyObject *TcX_Statement(const char *line) {
	tcTree *tree;
	tcNode *prog = TX_Read(line, &tree);
	PyObject *v = NULL;
	byte was = tx_builtinAttr;

	tx_builtinAttr = 1;
	if (prog && TX_RunTree(tree, prog, TcX_Main(), &v) && !v) {
		v = Py_None;
		Py_INCREF(v);
	}
	tx_builtinAttr = was;
	TX_Release(tree);
	return v;
}

/* the global "result" of globals (a variable of a pack module through the registry), borrowed; del: removed */
static PyObject *TX_Result(PyObject *globals, byte del) {
	modVarRef_t mv;
	byte inMod = ModVar_OfDict(globals, &mv);
	PyObject *v = inMod ? ModVar_Get(&mv, "result") : PyDict_GetItemString(globals, "result");

	if (v && del) {
		if (inMod) {
			ModVar_Del(&mv, "result");
		} else {
			PyDict_DelItemString(globals, "result");
		}
		v = NULL;
	}
	return v;
}

void TcX_Run(tcBuf *b, const char *line, PyObject *globals, byte quiet) {
	tcTree *tree;
	tcNode *prog = TX_Read(line, &tree);
	PyObject *v = NULL;
	byte ok = 0;

	if (prog) {
		if (!(prog->flags & 1)) {
			TX_Result(globals, 1);
		}
		ok = TX_RunTree(tree, prog, globals, &v);
		if (ok && !(prog->flags & 1)) {
			Py_XDECREF(v);
			v = TX_Result(globals, 0);
			Py_XINCREF(v);
		}
	}
	TX_Release(tree);
	if (!ok) {
		Py_XDECREF(v);
		TC_Add(b, "error: ", -1);
		TC_AddError(b);
		TC_Add(b, "\n", 1);
		return;
	}
	if (!quiet) {
		if (v) {
			TC_AddObj(b, v, 1);
		} else {
			TC_Add(b, "ok", 2);
		}
		TC_Add(b, "\n", 1);
	}
	Py_XDECREF(v);
}

/* ---- the channel's own functions ---- */

static PyObject *TP_SdnObj(PyObject *x, int depth);

/* the items of a tuple or list through TP_SdnObj, as a new list */
static PyObject *TP_SdnList(PyObject *x, int depth) {
	int n = PySequence_Length(x);
	PyObject *out = n >= 0 ? PyList_New(n) : NULL;

	for (int i = 0; out && i < n; i++) {
		PyObject *item = PySequence_GetItem(x, i);
		PyObject *v = item ? TP_SdnObj(item, depth + 1) : NULL;
		Py_XDECREF(item);
		if (!v) {
			Py_DECREF(out);
			return NULL;
		}
		PyList_SetItem(out, i, v);
	}
	return out;
}

/* the prefix and a string attribute: "ent:" + x.Name; NULL with the error set */
static PyObject *TP_Prefixed(const char *prefix, PyObject *s) {
	PyObject *v = PyString_FromString(prefix);

	if (v) {
		PyString_Concat(&v, s);
	}
	return v;
}

static PyObject *TP_SdnObj(PyObject *x, int depth) {
	if (depth > TX_EVAL_DEPTH) {
		PyErr_SetString(PyExc_RuntimeError, "maximum recursion depth exceeded");
		return NULL;
	}
	if (PyTuple_Check(x)) {
		PyObject *list = TP_SdnList(x, depth);
		PyObject *v = list ? PyList_AsTuple(list) : NULL;
		Py_XDECREF(list);
		return v;
	}
	if (PyList_Check(x)) {
		return TP_SdnList(x, depth);
	}
	if (PyDict_Check(x)) {
		PyObject *d = PyDict_New(), *k, *item;
		int pos = 0;
		while (d && PyDict_Next(x, &pos, &k, &item)) {
			PyObject *v = TP_SdnObj(item, depth + 1);
			if (!v || PyDict_SetItem(d, k, v) != 0) {
				Py_XDECREF(v);
				Py_DECREF(d);
				return NULL;
			}
			Py_DECREF(v);
		}
		return d;
	}
	if (PyFloat_Check(x)) {
		PyObject *round = TX_Global("round");
		PyObject *v = round ? PyObject_CallFunction(round, "(Oi)", x, 2) : NULL;
		Py_XDECREF(round);
		return v;
	}
	if (!strcmp(x->ob_type->tp_name, "cPyEntity")) {
		PyObject *name = PyObject_GetAttrString(x, "Name");
		PyObject *v = name ? TP_Prefixed("ent:", name) : NULL;
		Py_XDECREF(name);
		if (!v) {
			PyErr_Clear();
			v = PyString_FromString("ent:?");
		}
		return v;
	}
	if (!PyString_Check(x)) {
		PyObject *name = PyObject_GetAttrString(x, "__name__");
		if (name) {
			PyObject *v = TP_Prefixed("fn:", name);
			Py_DECREF(name);
			return v;
		}
		PyErr_Clear();
	}
	if (PyInstance_Check(x)) {
		return TP_Prefixed("inst:", ((PyInstanceObject *)x)->in_class->cl_name);
	}
	Py_INCREF(x);
	return x;
}

static PyObject *TP_Sdn(PyObject *self, PyObject *args) {
	PyObject *x;

	(void)self;
	if (!PyArg_ParseTuple(args, "O", &x)) {
		return NULL;
	}
	return TP_SdnObj(x, 0);
}

static PyObject *TP_Catch(PyObject *self, PyObject *args) {
	PyObject *f, *fargs, *kw = Py_None, *handler = Py_None;

	(void)self;
	if (!PyArg_ParseTuple(args, "OO|OO", &f, &fargs, &kw, &handler)) {
		return NULL;
	}
	PyObject *v = PyEval_CallObjectWithKeywords(f, fargs, kw == Py_None ? NULL : kw);
	if (v && PyErr_Occurred()) {
		Py_DECREF(v);
		v = NULL;
	}
	if (v) {
		return v;
	}
	PyObject *t, *val, *tb;
	PyErr_Fetch(&t, &val, &tb);
	PyErr_NormalizeException(&t, &val, &tb);
	if (!t) {
		Py_INCREF(Py_None);
		t = Py_None;
	}
	if (!val) {
		Py_INCREF(Py_None);
		val = Py_None;
	}
	if (handler == Py_None) {
		v = Py_BuildValue("(OO)", t, val);
	} else {
		v = PyObject_CallFunction(handler, "(OO)", t, val);
	}
	Py_DECREF(t);
	Py_DECREF(val);
	Py_XDECREF(tb);
	return v;
}

static PyObject *TP_Try(PyObject *self, PyObject *args) {
	PyObject *f, *fargs, *dflt;

	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &f, &fargs, &dflt)) {
		return NULL;
	}
	PyObject *v = PyEval_CallObject(f, fargs);
	if (v && PyErr_Occurred()) {
		Py_DECREF(v);
		v = NULL;
	}
	if (!v) {
		PyErr_Clear();
		Py_INCREF(dflt);
		v = dflt;
	}
	return v;
}

static PyObject *TP_Finally(PyObject *self, PyObject *args) {
	PyObject *f, *fargs, *after;

	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &f, &fargs, &after)) {
		return NULL;
	}
	PyObject *v = PyEval_CallObject(f, fargs);
	if (v && PyErr_Occurred()) {
		Py_DECREF(v);
		v = NULL;
	}
	PyObject *t, *val, *tb;
	PyErr_Fetch(&t, &val, &tb);
	PyObject *r = PyEval_CallObject(after, NULL);
	if (!r) {
		/* an exception of the cleanup replaces the one before, as in a finally clause */
		Py_XDECREF(t);
		Py_XDECREF(val);
		Py_XDECREF(tb);
		Py_XDECREF(v);
		return NULL;
	}
	Py_DECREF(r);
	PyErr_Restore(t, val, tb);
	return v;
}

static PyObject *TP_Swap(PyObject *self, PyObject *args) {
	PyObject *o, *name, *value, *f;

	(void)self;
	if (!PyArg_ParseTuple(args, "OSOO", &o, &name, &value, &f)) {
		return NULL;
	}
	PyObject *old = TX_GetAttr(o, name);
	if (!old || TX_SetAttr(o, name, value) != 0) {
		Py_XDECREF(old);
		return NULL;
	}
	PyObject *v = PyEval_CallObject(f, NULL);
	if (v && PyErr_Occurred()) {
		Py_DECREF(v);
		v = NULL;
	}
	PyObject *t, *val, *tb;
	PyErr_Fetch(&t, &val, &tb);
	if (TX_SetAttr(o, name, old) != 0) {
		PyErr_Clear();
	}
	PyErr_Restore(t, val, tb);
	Py_DECREF(old);
	return v;
}

/* a new class name ("Module.Class" sets its module, else __main__) with the items of the dict attrs; NULL with the error
 * set */
static PyObject *TP_MakeClass(const char *name, PyObject *attrs) {
	char mod[0x80];
	const char *dot = strrchr(name, '.');

	if (attrs != Py_None && !PyDict_Check(attrs)) {
		PyErr_SetString(PyExc_TypeError, "the attributes must be a dict");
		return NULL;
	}
	Str_Copy(mod, "__main__", sizeof(mod));
	if (dot) {
		snprintf(mod, sizeof(mod), "%.*s", (int)(dot - name), name);
	}
	PyObject *cdict = PyDict_New(), *k, *v;
	int pos = 0;
	while (cdict && attrs != Py_None && PyDict_Next(attrs, &pos, &k, &v)) {
		if (PyDict_SetItem(cdict, k, v) != 0) {
			Py_DECREF(cdict);
			return NULL;
		}
	}
	PyObject *ms = PyString_FromString(mod);
	if (!cdict || !ms || PyDict_SetItemString(cdict, "__module__", ms) != 0) {
		Py_XDECREF(cdict);
		Py_XDECREF(ms);
		return NULL;
	}
	Py_DECREF(ms);
	PyObject *bases = PyTuple_New(0);
	PyObject *cname = PyString_FromString(dot ? dot + 1 : name);
	PyObject *klass = bases && cname ? PyClass_New(bases, cdict, cname) : NULL;
	Py_DECREF(cdict);
	Py_XDECREF(bases);
	Py_XDECREF(cname);
	return klass;
}

static PyObject *TP_Class(PyObject *self, PyObject *args) {
	char *name;
	PyObject *attrs = Py_None;

	(void)self;
	return PyArg_ParseTuple(args, "s|O", &name, &attrs) ? TP_MakeClass(name, attrs) : NULL;
}

static PyObject *TP_Obj(PyObject *self, PyObject *args) {
	char *name;
	PyObject *attrs, *call = Py_None, *repr = Py_None;

	(void)self;
	if (!PyArg_ParseTuple(args, "sO|OO", &name, &attrs, &call, &repr)) {
		return NULL;
	}
	PyObject *klass = TP_MakeClass(name, Py_None);
	PyObject *inst = klass ? PyInstance_New(klass, NULL, NULL) : NULL;
	Py_XDECREF(klass);
	if (inst && attrs != Py_None) {
		PyObject *k, *v;
		int pos = 0;
		if (!PyDict_Check(attrs)) {
			PyErr_SetString(PyExc_TypeError, "the attributes must be a dict");
			Py_DECREF(inst);
			return NULL;
		}
		while (PyDict_Next(attrs, &pos, &k, &v)) {
			if (PyObject_SetAttr(inst, k, v) != 0) {
				Py_DECREF(inst);
				return NULL;
			}
		}
	}
	if (inst && ((call != Py_None && PyObject_SetAttrString(inst, "__call__", call) != 0)
			|| (repr != Py_None && PyObject_SetAttrString(inst, "__repr__", repr) != 0))) {
		Py_DECREF(inst);
		return NULL;
	}
	return inst;
}

static PyObject *TP_Walk(PyObject *self, PyObject *args) {
	PyObject *x, *next;

	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &x, &next)) {
		return NULL;
	}
	PyObject *list = PyList_New(0);
	Py_INCREF(x);
	for (int n = 0; list && x; n++) {
		int t = PyObject_IsTrue(x);
		if (t <= 0 || n > 100000) {
			if (t < 0) {
				Py_DECREF(list);
				list = NULL;
			}
			break;
		}
		if (PyList_Append(list, x) != 0) {
			Py_DECREF(list);
			list = NULL;
			break;
		}
		PyObject *y = PyObject_CallFunction(next, "(O)", x);
		Py_DECREF(x);
		x = y;
		if (!x) {
			Py_DECREF(list);
			list = NULL;
		}
	}
	Py_XDECREF(x);
	return list;
}

static PyObject *TP_Sorted(PyObject *self, PyObject *args) {
	PyObject *seq;

	(void)self;
	if (!PyArg_ParseTuple(args, "O", &seq)) {
		return NULL;
	}
	PyObject *list = PySequence_GetSlice(seq, 0, INT_MAX);
	if (list && !PyList_Check(list)) {
		PyObject *l = PySequence_List(list);
		Py_DECREF(list);
		list = l;
	}
	if (list && PyList_Sort(list) != 0) {
		Py_DECREF(list);
		list = NULL;
	}
	return list;
}

static PyObject *TP_None(int r) {
	if (r != 0) {
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *TP_Set(PyObject *self, PyObject *args) {
	PyObject *o, *k, *v;

	(void)self;
	return PyArg_ParseTuple(args, "OOO", &o, &k, &v) ? TP_None(PyObject_SetItem(o, k, v)) : NULL;
}

static PyObject *TP_Del(PyObject *self, PyObject *args) {
	PyObject *o, *k;

	(void)self;
	return PyArg_ParseTuple(args, "OO", &o, &k) ? TP_None(PyObject_DelItem(o, k)) : NULL;
}

static PyObject *TP_Clear(PyObject *self, PyObject *args) {
	PyObject *o;

	(void)self;
	return PyArg_ParseTuple(args, "O", &o) ? TP_None(PySequence_DelSlice(o, 0, INT_MAX)) : NULL;
}

static PyMethodDef tp_functions[] = {
	{ "tc_sdn", TP_Sdn, METH_VARARGS, NULL },
	{ "tc_catch", TP_Catch, METH_VARARGS, NULL },
	{ "tc_try", TP_Try, METH_VARARGS, NULL },
	{ "tc_finally", TP_Finally, METH_VARARGS, NULL },
	{ "tc_swap", TP_Swap, METH_VARARGS, NULL },
	{ "tc_obj", TP_Obj, METH_VARARGS, NULL },
	{ "tc_class", TP_Class, METH_VARARGS, NULL },
	{ "tc_walk", TP_Walk, METH_VARARGS, NULL },
	{ "tc_sorted", TP_Sorted, METH_VARARGS, NULL },
	{ "tc_set", TP_Set, METH_VARARGS, NULL },
	{ "tc_del", TP_Del, METH_VARARGS, NULL },
	{ "tc_clear", TP_Clear, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL }
};

/* a new function object of the channel's function name, NULL (no error) when there is none; nothing is kept between
 * lines (the interpreter's objects go with every level load) */
static PyObject *TP_Function(const char *name) {
	for (PyMethodDef *m = tp_functions; m->ml_name; m++) {
		if (!strcmp(m->ml_name, name)) {
			return PyCFunction_New(m, NULL);
		}
	}
	return NULL;
}
