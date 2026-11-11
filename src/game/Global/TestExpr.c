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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/pyfunction.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "testchannel.h"

#define TX_DEPTH 64			/* nesting of brackets in a line */
#define TX_PARSE_DEPTH 200	/* nesting of the reader */
#define TX_EVAL_DEPTH 300	/* nesting of evaluation (lambdas calling lambdas) */

enum {
	X_CONST,	/* val */
	X_LIT,		/* val: a literal with a list or dict in it, copied each time (new lists and dicts each time) */
	X_NAME,		/* name */
	X_ATTR,		/* a.name */
	X_CALL,		/* a(kids[0..n-1], names[i]=kids[n+i]); flags 1: b gives the whole argument tuple */
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
	X_LAMBDA,	/* names (n params, then *args, **kw), kids: the defaults of the last nkw params, a: body */
	X_REPR,		/* `a` */
	S_EXPR,		/* a */
	S_ASSIGN,	/* kids = ... = a */
	S_DEL,		/* del kids */
	S_IMPORT,	/* import names[0..n-1] */
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
	const char *name;
	const char **names;
	nativeVal_t *val;
	byte *ops;
};

/* a value the nodes of a tree keep */
typedef struct tcKept {
	struct tcKept *next;
	nativeVal_t v;
} tcKept;

/* the nodes of a line: freed when the line is done and no lambda of it lives on */
typedef struct tcTree {
	int refs;
	void *blocks;		/* the allocations, linked through their first word */
	tcKept *kept;		/* the values of the constant nodes */
} tcTree;

typedef struct tcParse {
	const char *p;
	tcTree *t;
	int depth;
} tcParse;

/* the parameters of a lambda that runs: shared by the lambdas made while it runs (they see them as the scopes around
 * them) */
typedef struct tcEnv {
	int refs;
	struct tcEnv *up;	/* the parameters of the lambda around it (held), NULL at the end */
	const tcNode *x;	/* the lambda: the names of v */
	int n;
	nativeVal_t v[];
} tcEnv;

/* where names are found while a node runs */
typedef struct tcScope {
	tcEnv *env;		/* the parameters of the lambda that runs and of those around it, NULL at the top level */
	PyObject *globals;	/* OBJLIB 9: the dict of __main__ or of a module (the channel's verbs hand it in) */
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
	t->kept = NULL;
	return t;
}

static void TX_Release(tcTree *t) {
	if (!t || --t->refs > 0) {
		return;
	}
	for (tcKept *k = t->kept; k; k = k->next) {
		Native_Free(&k->v);
	}
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

/* the value v moved into the tree (freed with it) */
static nativeVal_t *TX_Keep(tcTree *t, nativeVal_t *v) {
	tcKept *k = TX_Alloc(t, sizeof(tcKept));

	k->v = *v;
	Native_SetNone(v);
	k->next = t->kept;
	t->kept = k;
	return &k->v;
}

/* the text s[0:n] kept by the tree */
static const char *TX_Text(tcTree *t, const char *s, int n) {
	char *d = TX_Alloc(t, (size_t)n + 1);

	memcpy(d, s, (size_t)n);
	return d;
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

/* n names (texts of the tree) as an array kept by the tree, NULL for none */
static const char **TX_Names(tcTree *t, const char *const *names, int n) {
	const char **k = n ? TX_Alloc(t, sizeof(char *) * (size_t)n) : NULL;

	if (k) {
		memcpy(k, names, sizeof(char *) * (size_t)n);
	}
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

/* a name at ps->p as a text kept by the tree; NULL (no error) when there is none */
static const char *TX_Name(tcParse *ps) {
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
	if (TX_IsKeyword(p, n) || n >= 0x80) {
		return NULL;
	}
	ps->p = p + n;
	return TX_Text(ps->t, p, n);
}

/* text through the reader of the scripts' literals (Native_Literal): a constant node, or a literal node for a value
 * with a list or dict in it (copied each time it runs: new lists and dicts each time); NULL without an error when it
 * is no literal */
static tcNode *TX_Literal(tcParse *ps, const char *text) {
	nativeVal_t v;

	if (!Native_Literal(text, &v)) {
		return NULL;
	}
	tcNode *x = TX_Node(ps, Native_Holds(&v, 1u << NV_LIST | 1u << NV_DICT) ? X_LIT : X_CONST);
	x->val = TX_Keep(ps->t, &v);
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
		nativeBuf_t s;
		nativeVal_t v;
		Native_BufInit(&s);
		while (*p == '"' || *p == '\'') {
			const char *e = TX_StrEnd(p);
			tcNode *x = e ? TX_LiteralN(ps, p, (int)(e - p) + 1, "") : NULL;
			if (!x || x->kind != X_CONST || x->val->type != NV_STR) {
				Native_BufFree(&s);
				return TX_Syntax(p);
			}
			Native_BufAdd(&s, Native_Text(x->val), x->val->n);
			ps->p = e + 1;
			TX_Blanks(ps);
			p = ps->p;
		}
		tcNode *x = TX_Node(ps, X_CONST);
		Native_SetText(&v, s.p, s.len);
		Native_BufFree(&s);
		x->val = TX_Keep(ps->t, &v);
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
	const char *name = TX_Name(ps);
	if (!name) {
		return TX_Syntax(p);
	}
	tcNode *x;
	if (!strcmp(name, "inf") || !strcmp(name, "nan")) {
		nativeVal_t v;
		x = TX_Node(ps, X_CONST);
		Native_SetFloat(&v, *name == 'i' ? HUGE_VAL : nan(""));
		x->val = TX_Keep(ps->t, &v);
		return x;
	}
	x = TX_Node(ps, X_NAME);
	x->name = name;
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
	if (lit && lit->val->type == NV_TUPLE) {
		x->b = lit;
		x->flags = 1;
		ps->p = close + 1;
		return x;
	}
	tcVec pos = { 0 }, kwv = { 0 };
	const char *names[0x40];
	ps->p = open + 1;
	TX_Blanks(ps);
	while (*ps->p != ')') {
		const char *at = ps->p;
		const char *key = TX_Name(ps);
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
		if (!v || (key && kwv.n >= (int)(sizeof(names) / sizeof(names[0])))) {
			TX_Drop(&pos);
			TX_Drop(&kwv);
			return v ? TX_Syntax(at) : NULL;
		}
		if (key) {
			names[kwv.n] = key;
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
			return TX_Syntax(ps->p);
		}
	}
	ps->p++;
	x->n = pos.n;
	x->nkw = kwv.n;
	x->names = TX_Names(ps->t, names, kwv.n);
	for (int i = 0; i < kwv.n; i++) {
		TX_Push(&pos, kwv.v[i]);
	}
	TX_Drop(&kwv);
	x->kids = TX_Kids(ps, &pos);
	return x;
}

static tcNode *P_Postfix(tcParse *ps) {
	tcNode *x = P_Atom(ps);

	while (x) {
		TX_Blanks(ps);
		if (*ps->p == '.' && !(ps->p[1] >= '0' && ps->p[1] <= '9')) {
			ps->p++;
			const char *name = TX_Name(ps);
			if (!name) {
				return TX_Syntax(ps->p);
			}
			tcNode *y = TX_Node(ps, X_ATTR);
			y->a = x;
			y->name = name;
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
	const char *names[0x42];
	const char *star = NULL, *starstar = NULL;
	int np = 0;

	TX_Blanks(ps);
	while (*ps->p != ':') {
		const char *name;
		if (TX_Op(ps, "**", NULL)) {
			starstar = TX_Name(ps);
			name = starstar;
		} else if (TX_Op(ps, "*", NULL)) {
			star = TX_Name(ps);
			name = star;
		} else {
			name = TX_Name(ps);
			if (name && !star && !starstar && np < 0x40) {
				names[np++] = name;
				TX_Blanks(ps);
				if (*ps->p == '=' && ps->p[1] != '=') {
					ps->p++;
					tcNode *d = P_Test(ps);
					if (!d) {
						TX_Drop(&defs);
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
		return NULL;
	}
	tcNode *x = TX_Node(ps, X_LAMBDA);
	int count = np;
	if (star) {
		names[count++] = star;
		x->flags |= 1;
	}
	if (starstar) {
		names[count++] = starstar;
		x->flags |= 2;
	}
	x->n = np;
	x->nkw = defs.n;
	x->kids = TX_Kids(ps, &defs);
	x->a = body;
	x->names = TX_Names(ps->t, names, count);
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
		const char *names[0x40];
		int count = 0;
		do {
			const char *from;
			int n = 0;
			TX_Blanks(ps);
			from = ps->p;
			while (TX_NameChar(*ps->p, 0) || *ps->p == '.') {
				if (n < 0xff) {
					n++;
				}
				ps->p++;
			}
			if (!n || count >= (int)(sizeof(names) / sizeof(names[0]))) {
				return TX_Syntax(ps->p);
			}
			names[count++] = TX_Text(ps->t, from, n);
		} while (TX_Op(ps, ",", NULL));
		x = TX_Node(ps, S_IMPORT);
		x->n = count;
		x->names = TX_Names(ps->t, names, count);
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

/* ---- what still goes through Python (OBJLIB 9) ---- */

/* the counts by what it was ("builtin dir", "operator **", ...): "native ports" prints them */
#define TX_COUNTS 128
static struct {
	char what[48];
	unsigned long n;
} tx_counts[TX_COUNTS];

static void TX_Count(const char *kind, const char *name) {
	char what[48];
	int i;

	snprintf(what, sizeof(what), "%s %s", kind, name);
	for (i = 0; i < TX_COUNTS - 1 && tx_counts[i].what[0] && strcmp(tx_counts[i].what, what); i++) {
	}
	if (!tx_counts[i].what[0]) {
		/* the last entry takes what has no room */
		Str_Copy(tx_counts[i].what, i == TX_COUNTS - 1 ? "other" : what, sizeof(tx_counts[i].what));
	}
	tx_counts[i].n++;
}

void TcX_Counts(nativeBuf_t *out, byte reset) {
	char line[0x80];

	for (int i = 0; i < TX_COUNTS && tx_counts[i].what[0]; i++) {
		if (reset) {
			tx_counts[i].n = 0;
		} else if (tx_counts[i].n) {
			snprintf(line, sizeof(line), "channel %.47s %lu\n", tx_counts[i].what, tx_counts[i].n);
			Native_BufAdd(out, line, -1);
		}
	}
}

/* ---- values ---- */

PyObject *TcX_Main(void) {
	PyObject *m = PyImport_AddModule("__main__");
	return m ? PyModule_GetDict(m) : NULL;
}

/* OBJLIB 9: the Python object behind a value (borrowed): the one of a view or of an NV_OBJ; NULL for native data and
 * the kinds by value */
static PyObject *TX_Py(const nativeVal_t *v) {
	return v->type == NV_REF ? NObj_Py(v) : v->type == NV_OBJ ? v->obj : NULL;
}

/* o as the value the evaluator works on: numbers, texts, None and tuples by value, a handle as NV_ENT, a list, dict,
 * instance, class, function, method or module as its view. OBJLIB 9: a tuple keeps its Python object in .obj (copied
 * and freed with the value), so `is` finds one tuple twice and the tuple goes back to Python as that object */
static void TX_FromPy(PyObject *o, nativeVal_t *out) {
	NObj_FromPy(o, out);
	if (out->type == NV_TUPLE && PyTuple_Check(o) && !out->obj) {
		Py_INCREF(o);
		out->obj = o;
	}
}

/* r (a new reference, given away; NULL: the error is set) as TX_FromPy gives it */
static byte TX_Result(PyObject *r, nativeVal_t *out) {
	Native_SetNone(out);
	if (!r) {
		return 0;
	}
	TX_FromPy(r, out);
	Py_DECREF(r);
	return 1;
}

/* a value by value before it leaves the evaluator (an argument, a stored value): a dict in it becomes a dict object,
 * which crosses to Python with its table as it is (Native_FillPyDict); a dict by value would be laid out anew */
static void TX_Give(nativeVal_t *v) {
	if (Native_Holds(v, 1u << NV_DICT)) {
		NObj_Share(v);
	}
}

/* OBJLIB 9: the value as a Python object (a new reference, NULL with the error), for the globals, the module
 * variables and the channel's verbs */
static PyObject *TX_ToPy(const nativeVal_t *v) {
	nativeVal_t t;

	if (v->type == NV_TUPLE && v->obj && PyTuple_Check(v->obj)) {
		/* the tuple it came from (TX_FromPy) */
		Py_INCREF(v->obj);
		return v->obj;
	}
	if (!Native_Holds(v, 1u << NV_DICT)) {
		return Native_ToPy(v);
	}
	Native_Copy(&t, v);
	NObj_Share(&t);
	PyObject *o = Native_ToPy(&t);
	Native_Free(&t);
	return o;
}

/* repr (or str) of v as a text value: 0 with the error of a __repr__ that failed */
static byte TX_Repr(const nativeVal_t *v, nativeRepr_t mode, nativeVal_t *out) {
	nativeBuf_t b;

	Native_SetNone(out);
	Native_BufInit(&b);
	byte ok = NObj_Repr(&b, v, mode);
	if (ok) {
		Native_SetText(out, b.p, b.len);
	}
	Native_BufFree(&b);
	return ok;
}

/* ---- names ---- */

static PyObject *TP_Function(const char *name);
static PyObject *TX_AttrTwin(const char *name, PyObject *builtin);

/* a name the scopes of the line do not give: the builtins (getattr, setattr, hasattr and delattr as the twins that
 * reach the pack modules through the registry), the channel's functions, the modules (new reference) */
static PyObject *TX_GlobalPy(const char *name) {
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
	return TX_GlobalPy(name);
}

/* the parameter name of the lambda of e, NULL when it has none */
static const nativeVal_t *TX_EnvFind(const tcEnv *e, const char *name) {
	for (int i = e->n - 1; i >= 0; i--) {
		if (!strcmp(e->x->names[i], name)) {
			return &e->v[i];
		}
	}
	return NULL;
}

static byte X_Name(const char *name, tcScope *s, nativeVal_t *out) {
	const nativeVal_t *v = s->env ? TX_EnvFind(s->env, name) : NULL;

	Native_SetNone(out);
	if (!v && !strcmp(name, "__globals__")) {
		/* the globals the line or lambda runs in (helpers set their module variables through it) */
		return Native_FromPyRef(s->globals, out);
	}
	for (const tcEnv *e = s->env ? s->env->up : NULL; !v && e; e = e->up) {
		v = TX_EnvFind(e, name);
	}
	if (v) {
		Native_Copy(out, v);
		return 1;
	}
	PyObject *g = s->inMod ? ModVar_Get(&s->mv, name) : PyDict_GetItemString(s->globals, (char *)name);
	if (g) {
		TX_FromPy(g, out);
		return 1;
	}
	return TX_Result(TX_GlobalPy(name), out);
}

/* name = v in the globals (a variable of a pack module through the registry); statements run at the top level only,
 * a lambda has no assignment */
static byte X_SetName(tcScope *s, const char *name, const nativeVal_t *v) {
	PyObject *o = TX_ToPy(v);

	if (!o) {
		return 0;
	}
	int r = s->inMod ? ModVar_Set(&s->mv, name, o) : PyDict_SetItemString(s->globals, (char *)name, o);
	Py_DECREF(o);
	return r == 0;
}

/* the scope of the top level of a line (env NULL) or of a lambda that runs in globals */
static void X_Scope(tcScope *s, tcEnv *env, PyObject *globals) {
	s->env = env;
	s->globals = globals;
	s->inMod = ModVar_OfDict(globals, &s->mv);
}

/* ---- running ---- */

static byte X_Eval(tcNode *x, tcScope *s, nativeVal_t *out);

/* set while a check line runs: golden's checks (tools/test/golden) read and set attributes with the getattr and setattr
 * builtins */
static byte tx_builtinAttr;

/* the error raised when o.name or o.name = v failed without one (an entity handle that gives no value and sets no
 * error): the interpreter's (ceval.c), in a check line the one of the getattr and setattr builtins */
static void X_NoError(void) {
	if (!PyErr_Occurred()) {
		PyErr_SetString(PyExc_SystemError, tx_builtinAttr ? "NULL result without error in call_object"
			: "error return without exception set");
	}
}

/* o.name: a variable of an entity through the core of its handle (Native_EntRead), the variable of a pack module
 * through the registry, the attribute of a native object, of a view through Python (NObj_GetAttr does the three);
 * 0 with the error set, or without one for a handle that gives none */
static byte TX_GetAttr(const nativeVal_t *o, const char *name, nativeVal_t *out) {
	Native_SetNone(out);
	if (o->type == NV_ENT) {
		cEntity *e = Native_Ent(o);
		if (e && Native_EntRead(e, name, out)) {
			if (out->type == NV_VEC) {
				/* a tuple of three floats, as the handle gives it */
				nativeVal_t vec = *out;
				Native_SetEmpty(out, NV_TUPLE);
				for (int i = 0; i < 3; i++) {
					Native_SetFloat(Native_Append(out), vec.v[i]);
				}
			}
			return 1;
		}
		/* gone, a method, or no such member: the handle's own path, with its console lines */
	} else if (TX_Py(o) && NObj_Kind(o) != NO_FUNCTION) {
		/* a view: the attribute of its Python object (as NObj_GetAttr gets it), a tuple with its object */
		return TX_Result(ModVar_ObjAttr(TX_Py(o), name), out);
	}
	return NObj_GetAttr(o, name, out);
}

/* o.name = v (v NULL: del o.name), as TX_GetAttr reads: 0 with the error set, or without one for a handle that takes
 * none */
static byte TX_SetAttr(const nativeVal_t *o, const char *name, const nativeVal_t *v) {
	nativeVal_t t;

	if (!v) {
		return NObj_DelAttr(o, name);
	}
	if (o->type == NV_ENT && !Native_Holds(v, NV_REFS)) {
		return Native_EntStore(Native_Ent(o), Native_EntName(o), name, v);
	}
	if (!Native_Holds(v, 1u << NV_DICT)) {
		return NObj_SetAttr(o, name, v);
	}
	Native_Copy(&t, v);
	TX_Give(&t);
	byte ok = NObj_SetAttr(o, name, &t);
	Native_Free(&t);
	return ok;
}

static byte X_GetAttr(const nativeVal_t *o, const char *name, nativeVal_t *out) {
	if (!TX_GetAttr(o, name, out)) {
		X_NoError();
		return 0;
	}
	return 1;
}

/* the bounds of a slice node as SLICE+n takes them (ints; an omitted one 0 and NOBJ_END): 0 with the error set */
static byte X_Bounds(tcNode *x, tcScope *s, long *lo, long *hi) {
	nativeVal_t a, b;
	byte ok;

	Native_SetNone(&a);
	Native_SetNone(&b);
	*lo = 0;
	*hi = NOBJ_END;
	ok = (!x->b || X_Eval(x->b, s, &a)) && (!x->c || X_Eval(x->c, s, &b));
	if (ok && ((x->b && a.type != NV_INT) || (x->c && b.type != NV_INT))) {
		ok = Native_Raise(NE_TYPE, "slice index must be int");
	}
	/* the int of SLICE+n (a C int: a larger long is cut) */
	if (ok && x->b) {
		*lo = (int)a.i;
	}
	if (ok && x->c) {
		*hi = (int)b.i;
	}
	Native_Free(&a);
	Native_Free(&b);
	return ok;
}

/* the nodes as a new list or tuple by value: 0 with the error set */
static byte X_Items(tcNode **kids, int n, tcScope *s, nativeType_t type, nativeVal_t *out) {
	Native_SetEmpty(out, type);
	for (int i = 0; i < n; i++) {
		nativeVal_t v;
		if (!X_Eval(kids[i], s, &v)) {
			Native_Free(out);
			return 0;
		}
		*Native_Append(out) = v;
	}
	return 1;
}

/* OBJLIB 9: f(a[0..n-1], **kw) through Python: the keywords of a call go to the callable as Python passes them */
static byte TX_CallPy(const nativeVal_t *f, const nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out) {
	PyObject *pf = TX_ToPy(f), *args = pf ? PyTuple_New(n) : NULL, *pkw = NULL, *r = NULL;

	for (int i = 0; args && i < n; i++) {
		PyObject *x = TX_ToPy(&a[i]);
		if (!x) {
			Py_DECREF(args);
			args = NULL;
			break;
		}
		PyTuple_SET_ITEM(args, i, x);
	}
	if (args && kw) {
		pkw = TX_ToPy(kw);
	}
	if (args && (pkw || !kw)) {
		r = PyEval_CallObjectWithKeywords(pf, args, pkw);
	}
	Py_XDECREF(pf);
	Py_XDECREF(args);
	Py_XDECREF(pkw);
	return TX_Result(r, out);
}

/* the call of a native function of the evaluator (a lambda, a builtin, a function of the channel): 1 or 0 as the call
 * gives, -1 (nothing done) for any other callable */
static int TX_Native(const nativeVal_t *f, nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out);
static byte TX_NullResult(void);

/* f(a[0..n-1], **kw): kw NULL, or the dict of the keywords. The values of a may be taken (they are None then). 0 with
 * the error set */
static byte TX_Call(const nativeVal_t *f, nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out) {
	byte ok;

	Native_SetNone(out);
	int r = TX_Native(f, a, n, kw, out);
	if (r >= 0) {
		ok = (byte)r;
	} else if (kw) {
		/* OBJLIB 9: keywords to a callable that is no lambda (a port function): through Python */
		TX_Count("call", "keywords");
		ok = TX_CallPy(f, a, n, kw, out);
	} else {
		for (int i = 0; i < n; i++) {
			TX_Give(&a[i]);
		}
		if (TX_Py(f)) {
			/* a view (a native function that crossed too): its Python object called as NObj_Call calls a view (the
			 * entry of a port's own function), the result as TX_FromPy gives it */
			PyObject *r = NULL;
			Port_Values(PORT_VSITE(), TX_Py(f), a, n, NULL, &r);
			ok = TX_Result(r, out);
		} else {
			ok = NObj_Call(f, a, n, out);
		}
	}
	/* a value with an error left set is the error (as the next check of the interpreter raises it) */
	if (ok && PyErr_Occurred()) {
		Native_Free(out);
		ok = 0;
	}
	return ok || TX_NullResult();
}

static byte X_Call(tcNode *x, tcScope *s, nativeVal_t *out) {
	nativeVal_t f, args, kw;
	byte ok;

	Native_SetNone(out);
	if (!X_Eval(x->a, s, &f)) {
		return 0;
	}
	Native_SetNone(&kw);
	if (x->flags & 1) {
		ok = X_Eval(x->b, s, &args);
	} else {
		ok = X_Items(x->kids, x->n, s, NV_TUPLE, &args);
		if (ok && x->nkw) {
			/* the dict of the keywords, set in their order */
			Native_SetEmpty(&kw, NV_DICT);
			for (int i = 0; ok && i < x->nkw; i++) {
				nativeVal_t k, v;
				ok = X_Eval(x->kids[x->n + i], s, &v);
				if (ok) {
					Native_SetText(&k, x->names[i], (int)strlen(x->names[i]));
					Native_DictSet(&kw, &k, &v);
				}
			}
		}
	}
	if (ok) {
		ok = TX_Call(&f, args.items, args.n, x->nkw ? &kw : NULL, out);
	}
	Native_Free(&args);
	Native_Free(&kw);
	Native_Free(&f);
	return ok;
}

/* a value the native operators take as it is: one by value, or a native list object as a list by value in *tmp
 * (Native_Free tmp after); NULL for a Python object (a view, a handle) and the other native objects */
static const nativeVal_t *TX_Plain(const nativeVal_t *v, nativeVal_t *tmp) {
	Native_SetNone(tmp);
	if (v->type == NV_OBJ || v->type == NV_ENT) {
		return NULL;
	}
	if (v->type != NV_REF) {
		return v;
	}
	if (NObj_Kind(v) != NO_LIST || NObj_Py(v) || !NObj_GetSlice(v, 0, NOBJ_END, tmp)) {
		PyErr_Clear();
		return NULL;
	}
	return tmp;
}

static const char *X_OpName(byte op) {
	static char name[3];

	name[0] = op == 'p' ? '*' : (char)op;
	name[1] = op == 'p' ? '*' : op == '<' || op == '>' ? (char)op : 0;
	return name;
}

/* int op int of intobject.c for << >> & ^ | */
static byte X_IntBits(byte op, long a, long b, nativeVal_t *out) {
	switch (op) {
	case '<':
	case '>':
		if (b < 0) {
			return Native_Raise(NE_VALUE, "negative shift count");
		}
		if (a == 0 || b == 0) {
			Native_SetInt(out, a);
		} else if (b >= (long)(8 * sizeof(long))) {
			Native_SetInt(out, op == '<' ? 0 : a < 0 ? -1 : 0);
		} else if (op == '<') {
			Native_SetInt(out, (long)((unsigned long)a << b));
		} else {
			Native_SetInt(out, a < 0 ? ~(~a >> b) : a >> b);
		}
		return 1;
	case '&':
		Native_SetInt(out, a & b);
		return 1;
	case '^':
		Native_SetInt(out, a ^ b);
		return 1;
	default:
		Native_SetInt(out, a | b);
		return 1;
	}
}

static byte X_Binary(byte op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	nativeVal_t ta, tb;
	const nativeVal_t *pa = TX_Plain(a, &ta), *pb = TX_Plain(b, &tb);
	byte ok, done = 0;

	Native_SetNone(out);
	if (pa && pb) {
		if (strchr("+-*/%", op)) {
			/* the native values multiply and divide no long ints */
			if (op == '+' || op == '-' || (pa->type != NV_LONG && pb->type != NV_LONG)) {
				ok = Native_Arith((char)op, pa, pb, out);
				done = 1;
			}
		} else if (op != 'p' && pa->type == NV_INT && pb->type == NV_INT) {
			ok = X_IntBits(op, pa->i, pb->i, out);
			done = 1;
		}
	}
	Native_Free(&ta);
	Native_Free(&tb);
	if (done) {
		return ok;
	}
	/* OBJLIB 9: the operators of objects, the power, long ints */
	PyObject *x = TX_ToPy(a), *y = x ? TX_ToPy(b) : NULL, *r = NULL;
	TX_Count("operator", X_OpName(op));
	if (y) {
		switch (op) {
		case '+':
			r = PyNumber_Add(x, y);
			break;
		case '-':
			r = PyNumber_Subtract(x, y);
			break;
		case '*':
			r = PyNumber_Multiply(x, y);
			break;
		case '/':
			r = PyNumber_Divide(x, y);
			break;
		case '%':
			r = PyNumber_Remainder(x, y);
			break;
		case 'p':
			r = PyNumber_Power(x, y, Py_None);
			break;
		case '<':
			r = PyNumber_Lshift(x, y);
			break;
		case '>':
			r = PyNumber_Rshift(x, y);
			break;
		case '&':
			r = PyNumber_And(x, y);
			break;
		case '^':
			r = PyNumber_Xor(x, y);
			break;
		default:
			r = PyNumber_Or(x, y);
			break;
		}
	}
	Py_XDECREF(x);
	Py_XDECREF(y);
	return TX_Result(r, out);
}

/* - + ~ of a number natively, of anything else through Python (its error for a value that takes none) */
static byte X_Unary(byte op, const nativeVal_t *a, nativeVal_t *out) {
	Native_SetNone(out);
	if (a->type == NV_INT || a->type == NV_FLOAT || a->type == NV_LONG) {
		if (op == '-') {
			return Mode_Neg(a, out);
		}
		if (op == '+') {
			Native_Copy(out, a);
			return 1;
		}
		if (a->type == NV_INT) {
			Native_SetInt(out, ~a->i);
			return 1;
		}
	}
	char name[8];
	snprintf(name, sizeof(name), "unary %c", op);
	TX_Count("operator", name);
	PyObject *x = TX_ToPy(a);
	PyObject *r = !x ? NULL : op == '-' ? PyNumber_Negative(x) : op == '+' ? PyNumber_Positive(x) : PyNumber_Invert(x);
	Py_XDECREF(x);
	return TX_Result(r, out);
}

/* v is w: one object (NObj_Is). Numbers and texts by value have no identity: the ints and the texts 1.5.2 keeps one
 * object of (-1 to 99, the empty text and the texts of one character) are the same when they are equal */
static byte X_Is(const nativeVal_t *v, const nativeVal_t *w) {
	if (v->type == NV_INT && w->type == NV_INT) {
		return v->i == w->i && v->i >= -1 && v->i < 100;
	}
	if (v->type == NV_STR && w->type == NV_STR) {
		return v->n <= 1 && Native_Equal(v, w);
	}
	if (v->type == NV_TUPLE && w->type == NV_TUPLE) {
		/* the empty tuple is one object; another tuple is the same one when both came from one Python tuple */
		return (!v->n && !w->n) || (v->obj && v->obj == w->obj);
	}
	return NObj_Is(v, w);
}

/* cmp_outcome of ceval.c: 1, 0, -1 with the error set */
static int X_Compare(byte op, const nativeVal_t *v, const nativeVal_t *w) {
	int r;

	switch (op) {
	case C_IS:
		return X_Is(v, w);
	case C_ISNOT:
		return !X_Is(v, w);
	case C_IN:
	case C_NOTIN:
		r = NObj_Contains(w, v);
		return r < 0 ? -1 : op == C_IN ? r : !r;
	default:
		r = Native_Compare(v, w);
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

static byte X_Lambda(tcNode *x, tcScope *s, nativeVal_t *out);

static byte X_Eval(tcNode *x, tcScope *s, nativeVal_t *out) {
	nativeVal_t a, b;
	byte ok = 0;
	int t;

	Native_SetNone(out);
	if (++tx_depth > TX_EVAL_DEPTH) {
		tx_depth--;
		PyErr_SetString(PyExc_RuntimeError, "maximum recursion depth exceeded");
		return 0;
	}
	switch (x->kind) {
	case X_CONST:
	case X_LIT:
		Native_Copy(out, x->val);
		ok = 1;
		break;
	case X_NAME:
		ok = X_Name(x->name, s, out);
		break;
	case X_ATTR:
		if (X_Eval(x->a, s, &a)) {
			ok = X_GetAttr(&a, x->name, out);
			Native_Free(&a);
		}
		break;
	case X_CALL:
		ok = X_Call(x, s, out);
		break;
	case X_INDEX:
		if (X_Eval(x->a, s, &a)) {
			if (X_Eval(x->b, s, &b)) {
				ok = NObj_GetItem(&a, &b, out);
				Native_Free(&b);
			}
			Native_Free(&a);
		}
		break;
	case X_SLICE:
		if (X_Eval(x->a, s, &a)) {
			long lo, hi;
			ok = X_Bounds(x, s, &lo, &hi);
			if (ok && a.type == NV_TUPLE && a.obj && (lo < 0 ? lo + a.n : lo) <= 0 && (hi < 0 ? hi + a.n : hi) >= a.n) {
				/* tupleslice: the whole tuple (the bounds as PySequence_GetSlice takes them) is the tuple itself */
				Native_Copy(out, &a);
			} else if (ok) {
				ok = NObj_GetSlice(&a, lo, hi, out);
			}
			Native_Free(&a);
		}
		break;
	case X_TUPLE:
		ok = X_Items(x->kids, x->n, s, NV_TUPLE, out);
		break;
	case X_LIST:
		ok = X_Items(x->kids, x->n, s, NV_LIST, out);
		break;
	case X_DICT:
		Native_SetEmpty(out, NV_DICT);
		ok = 1;
		for (int i = 0; ok && i + 1 < x->n; i += 2) {
			long h;
			ok = X_Eval(x->kids[i], s, &a);
			if (ok) {
				ok = X_Eval(x->kids[i + 1], s, &b) && NObj_Hash(&a, &h);
				if (ok) {
					Native_DictSet(out, &a, &b);
				} else {
					Native_Free(&a);
					Native_Free(&b);
				}
			}
		}
		break;
	case X_UNARY:
		if (X_Eval(x->a, s, &a)) {
			if (x->op == '!') {
				Native_SetInt(out, !Native_True(&a));
				ok = 1;
			} else {
				ok = X_Unary(x->op, &a, out);
			}
			Native_Free(&a);
		}
		break;
	case X_BIN:
		if (X_Eval(x->a, s, &a)) {
			if (X_Eval(x->b, s, &b)) {
				ok = X_Binary(x->op, &a, &b, out);
				Native_Free(&b);
			}
			Native_Free(&a);
		}
		break;
	case X_CMP:
		if (!X_Eval(x->kids[0], s, &a)) {
			break;
		}
		for (int i = 0; i + 1 < x->n; i++) {
			if (!X_Eval(x->kids[i + 1], s, &b)) {
				break;
			}
			t = X_Compare(x->ops[i], &a, &b);
			Native_Free(&a);
			a = b;
			if (t <= 0 || i + 2 == x->n) {
				if (t >= 0) {
					Native_SetInt(out, t);
					ok = 1;
				}
				break;
			}
		}
		Native_Free(&a);
		break;
	case X_AND:
	case X_OR:
		ok = X_Eval(x->a, s, out);
		if (ok && Native_True(out) == (x->kind == X_AND)) {
			Native_Free(out);
			ok = X_Eval(x->b, s, out);
		}
		break;
	case X_IF:
		if (X_Eval(x->b, s, &a)) {
			t = Native_True(&a);
			Native_Free(&a);
			ok = X_Eval(t ? x->a : x->c, s, out);
		}
		break;
	case X_LAMBDA:
		ok = X_Lambda(x, s, out);
		break;
	case X_REPR:
		if (X_Eval(x->a, s, &a)) {
			ok = TX_Repr(&a, NR_REPR, out);
			Native_Free(&a);
		}
		break;
	default:
		TX_Syntax("?");
		break;
	}
	if (!ok) {
		Native_Free(out);
	}
	tx_depth--;
	return ok;
}

/* ---- lambdas ---- */

/*
 * A lambda is a function record of native parts: its node, the values of its defaults, the parameters of the lambdas
 * around it and the globals it was made in. OBJLIB 9: while Python is linked its object is a C function object of
 * Python that holds the record (the value the evaluator works on is the view of that object, as for every function of
 * the game), so the game calls it as any callable (a function put into a module variable, the __call__ of a recorder
 * object) with positional and keyword arguments; the arguments come in as native values and the result goes back as
 * a Python object.
 */
typedef struct tcFunc {
	tcTree *t;
	tcNode *x;
	tcEnv *env;
	PyObject *globals;
	nativeVal_t defaults[];
} tcFunc;

static void TX_EnvDrop(tcEnv *e) {
	while (e && --e->refs == 0) {
		tcEnv *up = e->up;
		for (int i = 0; i < e->n; i++) {
			Native_Free(&e->v[i]);
		}
		Mem_Free(e);
		e = up;
	}
}

static void TX_FuncFree(void *p) {
	tcFunc *f = p;

	for (int i = 0; i < f->x->nkw; i++) {
		Native_Free(&f->defaults[i]);
	}
	TX_EnvDrop(f->env);
	Py_XDECREF(f->globals);
	tcTree *t = f->t;
	Mem_Free(f);
	TX_Release(t);
}

/* the lambda on na arguments (taken: None after) and the keywords pairs (NULL, or a list of (name, value) tuples in
 * the order of the dict of the call), the parameters bound as the interpreter binds them: 0 with the error set */
static byte TX_LambdaRun(tcFunc *f, nativeVal_t *a, int na, const nativeVal_t *pairs, nativeVal_t *out) {
	tcNode *x = f->x;
	int np = x->n, slots = np + (x->flags & 1) + ((x->flags & 2) >> 1), extra = np;
	byte bound[0x40] = { 0 };
	byte ok = 1;
	tcEnv *e = Mem_Alloc((uint32_t)(sizeof(tcEnv) + (size_t)slots * sizeof(nativeVal_t)));
	nativeVal_t *kwd = NULL;

	Native_SetNone(out);
	e->refs = 1;
	e->up = f->env;
	if (e->up) {
		e->up->refs++;
	}
	e->x = x;
	e->n = slots;
	for (int i = 0; i < slots; i++) {
		Native_SetNone(&e->v[i]);
	}
	for (int i = 0; i < np && i < na; i++) {
		e->v[i] = a[i];
		Native_SetNone(&a[i]);
		bound[i] = 1;
	}
	if (x->flags & 1) {
		nativeVal_t *rest = &e->v[extra++];
		Native_SetEmpty(rest, NV_TUPLE);
		for (int i = np; i < na; i++) {
			*Native_Append(rest) = a[i];
			Native_SetNone(&a[i]);
		}
	} else if (na > np) {
		ok = Native_Raise(NE_TYPE, "too many arguments; expected %d, got %d", np, na);
	}
	if (ok && x->flags & 2) {
		kwd = &e->v[extra];
		Native_SetEmpty(kwd, NV_DICT);
	}
	for (int k = 0; ok && pairs && k < pairs->n; k++) {
		const nativeVal_t *key = &pairs->items[k].items[0], *v = &pairs->items[k].items[1];
		int i;
		for (i = 0; i < np && !(key->type == NV_STR && Native_IsStr(key, x->names[i])); i++) {
		}
		if (i < np) {
			if (bound[i]) {
				ok = Native_Raise(NE_TYPE, "keyword parameter redefined");
			} else {
				Native_Copy(&e->v[i], v);
				bound[i] = 1;
			}
		} else if (kwd) {
			nativeVal_t kc, vc;
			Native_Copy(&kc, key);
			Native_Copy(&vc, v);
			Native_DictSet(kwd, &kc, &vc);
		} else {
			ok = Native_Raise(NE_TYPE, "unexpected keyword argument: %.80s", key->type == NV_STR ? Native_Text(key) : "?");
		}
	}
	for (int i = 0; ok && i < np; i++) {
		if (!bound[i]) {
			int d = i - (np - x->nkw);
			if (d < 0) {
				ok = Native_Raise(NE_TYPE, "not enough arguments; expected %d, got %d", np - x->nkw, na);
			} else {
				Native_Copy(&e->v[i], &f->defaults[d]);
			}
		}
	}
	if (ok) {
		tcScope s;
		tcTree *was = tx_tree;
		/* a list or dict a parameter holds is an object: the body and the lambdas made in it share it */
		for (int i = 0; i < slots; i++) {
			NObj_Share(&e->v[i]);
		}
		X_Scope(&s, e, f->globals);
		tx_tree = f->t;
		ok = X_Eval(x->a, &s, out);
		tx_tree = was;
	}
	TX_EnvDrop(e);
	return ok;
}

/* OBJLIB 9: the C function of the Python object of a lambda; self: (the record, None, globals, None) */
static PyObject *TX_LambdaPy(PyObject *self, PyObject *args, PyObject *kw) {
	tcFunc *f = PyCObject_AsVoidPtr(PyTuple_GetItem(self, 0));
	int n = args ? PyTuple_Size(args) : 0;
	nativeVal_t *a = Mem_Alloc((uint32_t)((size_t)(n + 1) * sizeof(*a))), pairs, r;

	for (int i = 0; i < n; i++) {
		TX_FromPy(PyTuple_GET_ITEM(args, i), &a[i]);
	}
	Native_SetEmpty(&pairs, NV_LIST);
	if (kw) {
		PyObject *k, *v;
		int pos = 0;
		while (PyDict_Next(kw, &pos, &k, &v)) {
			nativeVal_t *pair = Native_Append(&pairs);
			Native_SetEmpty(pair, NV_TUPLE);
			NObj_FromPy(k, Native_Append(pair));
			TX_FromPy(v, Native_Append(pair));
		}
	}
	byte ok = TX_LambdaRun(f, a, n, kw ? &pairs : NULL, &r);
	for (int i = 0; i < n; i++) {
		Native_Free(&a[i]);
	}
	Mem_Free(a);
	Native_Free(&pairs);
	PyObject *res = ok ? TX_ToPy(&r) : NULL;
	Native_Free(&r);
	return res;
}

static PyMethodDef tx_lambdaDef = { "<lambda>", (PyCFunction)(void (*)(void))TX_LambdaPy, METH_VARARGS | METH_KEYWORDS, NULL };

static byte X_Lambda(tcNode *x, tcScope *s, nativeVal_t *out) {
	tcFunc *f = Mem_Alloc((uint32_t)(sizeof(tcFunc) + (size_t)x->nkw * sizeof(nativeVal_t)));

	Native_SetNone(out);
	for (int i = 0; i < x->nkw; i++) {
		if (!X_Eval(x->kids[i], s, &f->defaults[i])) {
			while (i-- > 0) {
				Native_Free(&f->defaults[i]);
			}
			Mem_Free(f);
			return 0;
		}
		/* one object for every call that takes the default */
		NObj_Share(&f->defaults[i]);
	}
	f->t = tx_tree;
	tx_tree->refs++;
	f->x = x;
	/* the scopes around: the parameters of the lambda that runs and those around it */
	f->env = s->env;
	if (f->env) {
		f->env->refs++;
	}
	f->globals = s->globals;
	Py_INCREF(f->globals);
	PyObject *cf = PyCObject_FromVoidPtr(f, TX_FuncFree);
	if (!cf) {
		TX_FuncFree(f);
		return 0;
	}
	/* the self of the C function: (record, None, globals, None), the form the module dump of the channel shows as
	 * "self=?" (the 4-tuple of a lambda before batch 8a) */
	PyObject *self = Py_BuildValue("(OOOO)", cf, Py_None, f->globals, Py_None);
	PyObject *fn = self ? PyCFunction_New(&tx_lambdaDef, self) : NULL;
	Py_XDECREF(self);
	Py_DECREF(cf);
	return TX_Result(fn, out);
}

/* ---- the builtins and the channel's own functions on native values ---- */

/*
 * The builtin functions the cases and the helpers call (map, filter, reduce, apply, getattr, hasattr, setattr, delattr,
 * type, len, str, repr, int, float, round, range, abs, min, max, cmp, tuple, list, callable, __import__, id) run here when
 * the evaluator calls them, with the argument forms, results and error texts of bltinmodule.c of 1.5.2. A name still
 * gives the builtin's own function object of Python (its identity, repr and doc stay); the call is native. A form a
 * native one does not take (an instance or a Python object as the sequence, cmp of texts, a dotted __import__, ...)
 * goes to the Python builtin and is counted ("builtin <name>"), as every other builtin is (dir, id, open, ...).
 *
 * The channel's functions (tc_..., the helpers of the regression cases, see the head of the file) and the twins of
 * getattr, hasattr, setattr and delattr (which reach the pack modules through the registry) are native only. OBJLIB 9:
 * their function objects are C function objects of Python on adapters, so Python code and the game can hold and call
 * them too.
 *
 * Each takes n arguments (copies the function may take over or change) and gives 1 with *out, 0 with the error raised,
 * -1 (nothing raised) for a form that goes to the Python builtin.
 */
typedef int (*tcNative_t)(nativeVal_t *a, int n, nativeVal_t *out);

/* the TypeError of PyArg_ParseTuple for n arguments to fname (NULL: "function") of min to max: 1 when n fits */
static byte TX_Arity(const char *fname, int n, int min, int max) {
	if (n >= min && n <= max) {
		return 1;
	}
	int k = n < min ? min : max;
	return Native_Raise(NE_TYPE, "%s requires %s %d argument%s; %d given", fname ? fname : "function",
		min == max ? "exactly" : n < min ? "at least" : "at most", k, k == 1 ? "" : "s", n);
}

/* the "S" or "s" (nul 1: no 0 bytes) of PyArg_ParseTuple for argument i of fname: 1 when v is such a text */
static byte TX_ArgText(const char *fname, int i, const nativeVal_t *v, byte nul) {
	const char *expected = "string";

	if (v->type == NV_STR) {
		if (!nul || strlen(Native_Text(v)) == (size_t)v->n) {
			return 1;
		}
		expected = "string without null bytes";
	}
	if (fname) {
		return Native_Raise(NE_TYPE, "%s, argument %d: expected %s, %s found", fname, i, expected, Native_TypeName(v));
	}
	return Native_Raise(NE_TYPE, "argument %d: expected %s, %s found", i, expected, Native_TypeName(v));
}

/* the "NULL result without error in call_object" of a call that failed without an error: 0 */
static byte TX_NullResult(void) {
	if (!PyErr_Occurred()) {
		PyErr_SetString(PyExc_SystemError, "NULL result without error in call_object");
	}
	return 0;
}

/* how the sequence protocol takes v: 1 a sequence the evaluator reads (a text, a tuple or list by value, a list
 * object or view), 0 a value without sequence methods, -1 one only Python reads (an instance, a Python object) */
static int TX_SeqOf(const nativeVal_t *v) {
	switch (v->type) {
	case NV_STR:
	case NV_TUPLE:
	case NV_LIST:
	case NV_VEC:
		return 1;
	case NV_OBJ:
		return -1;
	case NV_REF:
		return NObj_Kind(v) == NO_LIST ? 1 : NObj_Kind(v) == NO_INSTANCE ? -1 : 0;
	}
	return 0;
}

/* the length of a sequence of TX_SeqOf 1, -1 with the error */
static int TX_SeqLen(const nativeVal_t *v) {
	int n;

	if (v->type == NV_REF) {
		return NObj_Len(v);
	}
	return Native_Len(v, &n) ? n : -1;
}

/* item i of such a sequence */
static byte TX_SeqGet(const nativeVal_t *v, int i, nativeVal_t *out) {
	if (v->type == NV_REF) {
		nativeVal_t key;
		Native_SetInt(&key, i);
		return NObj_GetItem(v, &key, out);
	}
	return Native_SeqItem(v, i, out);
}

/* the items of such a sequence as a list by value (PySequence_List): 0 with the error */
static byte TX_SeqList(const nativeVal_t *v, nativeType_t type, nativeVal_t *out) {
	Native_SetEmpty(out, type);
	for (int i = 0;; i++) {
		int n = TX_SeqLen(v);
		if (n < 0) {
			Native_Free(out);
			return 0;
		}
		if (i >= n) {
			return 1;
		}
		if (!TX_SeqGet(v, i, Native_Append(out))) {
			Native_Free(out);
			return 0;
		}
	}
}

/* the keywords of a call (a dict by value or an object) as (name, value) tuples in the dict's order: 0 with the error */
static byte TX_Pairs(const nativeVal_t *kw, nativeVal_t *pairs) {
	nativeVal_t items;

	Native_SetEmpty(pairs, NV_LIST);
	if (kw->type == NV_DICT) {
		const nativeVal_t *k, *v;
		int pos = 0;
		while (Native_DictNext(kw, &pos, &k, &v)) {
			nativeVal_t *pair = Native_Append(pairs);
			Native_SetEmpty(pair, NV_TUPLE);
			Native_Copy(Native_Append(pair), k);
			Native_Copy(Native_Append(pair), v);
		}
		return 1;
	}
	if (!NObj_CallMethod(kw, "items", NULL, 0, &items)) {
		return 0;
	}
	byte ok = TX_SeqList(&items, NV_LIST, pairs);
	Native_Free(&items);
	return ok;
}

/* a dict by value or a dict object */
static byte TX_IsDict(const nativeVal_t *v) {
	return v->type == NV_DICT || NObj_Kind(v) == NO_DICT;
}

/* kw is a dict with keywords (a call with an empty dict has none) */
static byte TX_HasKw(const nativeVal_t *kw) {
	return kw && TX_IsDict(kw) && (kw->type == NV_DICT ? kw->n > 0 : NObj_Len(kw) > 0);
}

/* round(x, digits) of bltinmodule.c */
static double TX_Round(double x, int digits) {
	double f = 1.0;

	for (int i = digits < 0 ? -digits : digits; --i >= 0;) {
		f = f * 10.0;
	}
	x = digits < 0 ? x / f : x * f;
	x = x >= 0.0 ? floor(x + 0.5) : ceil(x - 0.5);
	return digits < 0 ? x * f : x / f;
}

/* a Python object by value: NV_OBJ, a view, an entity handle (an operation of Python on it) */
static byte TX_IsPy(const nativeVal_t *v) {
	return v->type == NV_OBJ || v->type == NV_ENT || (v->type == NV_REF && NObj_Py(v));
}

static int TB_Map(nativeVal_t *a, int n, nativeVal_t *out) {
	if (n < 2) {
		return Native_Raise(NE_TYPE, "map() requires at least two args");
	}
	if (a[0].type == NV_NONE && n == 2) {
		/* map(None, S) is list(S) */
		int k = TX_SeqOf(&a[1]);
		return k < 0 ? -1 : k ? TX_SeqList(&a[1], NV_LIST, out) : Native_Raise(NE_TYPE, "list() argument must be a sequence");
	}
	for (int i = 1; i < n; i++) {
		int k = TX_SeqOf(&a[i]);
		if (k <= 0) {
			return k < 0 ? -1 : Native_Raise(NE_TYPE, "argument %d to map() must be a sequence object", i + 1);
		}
	}
	Native_SetEmpty(out, NV_LIST);
	for (int i = 0;; i++) {
		nativeVal_t args, value;
		byte any = 0;
		Native_SetEmpty(&args, NV_TUPLE);
		for (int j = 1; j < n; j++) {
			int len = TX_SeqLen(&a[j]);
			nativeVal_t *item = Native_Append(&args);
			if (len < 0 || (i < len && !TX_SeqGet(&a[j], i, item))) {
				Native_Free(&args);
				Native_Free(out);
				return 0;
			}
			any |= i < len;
		}
		if (!any) {
			Native_Free(&args);
			return 1;
		}
		if (a[0].type == NV_NONE) {
			value = args;
		} else {
			byte ok = TX_Call(&a[0], args.items, args.n, NULL, &value);
			Native_Free(&args);
			if (!ok) {
				Native_Free(out);
				return 0;
			}
		}
		*Native_Append(out) = value;
	}
}

/* func(item) is true (func None: the item is): 1, 0, -1 with the error */
static int TX_Good(nativeVal_t *func, const nativeVal_t *item) {
	nativeVal_t arg, r;

	if (func->type == NV_NONE) {
		return Native_True(item);
	}
	Native_Copy(&arg, item);
	byte ok = TX_Call(func, &arg, 1, NULL, &r);
	Native_Free(&arg);
	int t = ok ? Native_True(&r) : -1;
	Native_Free(&r);
	return t;
}

static int TB_Filter(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("filter", n, 2, 2)) {
		return 0;
	}
	nativeVal_t *seq = &a[1];
	if (seq->type == NV_STR) {
		/* filterstring: no character is false */
		nativeBuf_t b;
		if (a[0].type == NV_NONE) {
			Native_Copy(out, seq);
			return 1;
		}
		Native_BufInit(&b);
		for (int i = 0; i < seq->n; i++) {
			nativeVal_t c;
			Native_SetText(&c, Native_Text(seq) + i, 1);
			int t = TX_Good(&a[0], &c);
			Native_Free(&c);
			if (t < 0) {
				Native_BufFree(&b);
				return 0;
			}
			if (t) {
				Native_BufAdd(&b, Native_Text(seq) + i, 1);
			}
		}
		Native_SetText(out, b.p, b.len);
		Native_BufFree(&b);
		return 1;
	}
	int k = TX_SeqOf(seq);
	if (k <= 0) {
		return k < 0 ? -1 : Native_Raise(NE_TYPE, "argument 2 to filter() must be a sequence type");
	}
	/* filtertuple gives a tuple, the rest a list */
	byte tuple = seq->type == NV_TUPLE || seq->type == NV_VEC;
	Native_SetEmpty(out, tuple ? NV_TUPLE : NV_LIST);
	for (int i = 0;; i++) {
		nativeVal_t item;
		int len = TX_SeqLen(seq);
		if (len < 0) {
			Native_Free(out);
			return 0;
		}
		if (i >= len) {
			return 1;
		}
		if (!TX_SeqGet(seq, i, &item)) {
			Native_Free(out);
			return 0;
		}
		int t = TX_Good(&a[0], &item);
		if (t > 0) {
			*Native_Append(out) = item;
		} else {
			Native_Free(&item);
			if (t < 0) {
				Native_Free(out);
				return 0;
			}
		}
	}
}

static int TB_Reduce(nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t r;
	byte has = n > 2;

	if (!TX_Arity("reduce", n, 2, 3)) {
		return 0;
	}
	int k = TX_SeqOf(&a[1]);
	if (k <= 0) {
		return k < 0 ? -1 : Native_Raise(NE_TYPE, "2nd argument to reduce() must be a sequence object");
	}
	Native_SetNone(&r);
	if (has) {
		Native_Copy(&r, &a[2]);
	}
	for (int i = 0;; i++) {
		nativeVal_t args[2];
		int len = TX_SeqLen(&a[1]);
		if (len < 0) {
			Native_Free(&r);
			return 0;
		}
		if (i >= len) {
			break;
		}
		if (!TX_SeqGet(&a[1], i, &args[1])) {
			Native_Free(&r);
			return 0;
		}
		if (!has) {
			r = args[1];
			has = 1;
			continue;
		}
		args[0] = r;
		byte ok = TX_Call(&a[0], args, 2, NULL, &r);
		Native_Free(&args[0]);
		Native_Free(&args[1]);
		if (!ok) {
			return 0;
		}
	}
	if (!has) {
		return Native_Raise(NE_TYPE, "reduce of empty sequence with no initial value");
	}
	*out = r;
	return 1;
}

static int TB_Apply(nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t args;

	if (!TX_Arity("apply", n, 1, 3)) {
		return 0;
	}
	Native_SetEmpty(&args, NV_TUPLE);
	if (n > 1 && a[1].type != NV_TUPLE) {
		int k = TX_SeqOf(&a[1]);
		if (k <= 0) {
			return k < 0 ? -1 : Native_Raise(NE_TYPE, "apply() 2nd argument must be a sequence");
		}
		if (!TX_SeqList(&a[1], NV_TUPLE, &args)) {
			return 0;
		}
	} else if (n > 1) {
		Native_Copy(&args, &a[1]);
	}
	if (n > 2 && !TX_IsDict(&a[2])) {
		Native_Free(&args);
		return Native_Raise(NE_TYPE, "apply() 3rd argument must be dictionary");
	}
	byte ok = TX_Call(&a[0], args.items, args.n, n > 2 && TX_HasKw(&a[2]) ? &a[2] : NULL, out);
	Native_Free(&args);
	return ok;
}

static int TB_GetAttr(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("getattr", n, 2, 3) || !TX_ArgText("getattr", 2, &a[1], 0)) {
		return 0;
	}
	if (TX_GetAttr(&a[0], Native_Text(&a[1]), out)) {
		return 1;
	}
	if (n > 2) {
		PyErr_Clear();
		Native_Copy(out, &a[2]);
		return 1;
	}
	return TX_NullResult();
}

static int TB_HasAttr(nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t v;

	if (!TX_Arity("hasattr", n, 2, 2) || !TX_ArgText("hasattr", 2, &a[1], 0)) {
		return 0;
	}
	byte ok = TX_GetAttr(&a[0], Native_Text(&a[1]), &v);
	Native_Free(&v);
	if (!ok) {
		PyErr_Clear();
	}
	Native_SetInt(out, ok);
	return 1;
}

static int TB_SetAttr(nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	if (!TX_Arity("setattr", n, 3, 3) || !TX_ArgText("setattr", 2, &a[1], 0)) {
		return 0;
	}
	return TX_SetAttr(&a[0], Native_Text(&a[1]), &a[2]) || TX_NullResult();
}

static int TB_DelAttr(nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	if (!TX_Arity("delattr", n, 2, 2) || !TX_ArgText("delattr", 2, &a[1], 0)) {
		return 0;
	}
	return TX_SetAttr(&a[0], Native_Text(&a[1]), NULL) || TX_NullResult();
}

static int TB_Type(nativeVal_t *a, int n, nativeVal_t *out) {
	/* OBJLIB 9: the type objects are Python's (a value by value names the type it has as a Python object) */
	PyTypeObject *t = NULL;

	if (!TX_Arity("type", n, 1, 1)) {
		return 0;
	}
	PyObject *py = a->type == NV_REF ? NObj_Py(a) : a->type == NV_OBJ ? a->obj : NULL;
	if (py) {
		t = py->ob_type;
	} else {
		switch (a->type) {
		case NV_NONE:
			t = Py_None->ob_type;
			break;
		case NV_INT:
			t = &PyInt_Type;
			break;
		case NV_FLOAT:
			t = &PyFloat_Type;
			break;
		case NV_STR:
			t = &PyString_Type;
			break;
		case NV_LONG:
			t = &PyLong_Type;
			break;
		case NV_LIST:
			t = &PyList_Type;
			break;
		case NV_TUPLE:
		case NV_VEC:
			t = &PyTuple_Type;
			break;
		case NV_DICT:
			t = &PyDict_Type;
			break;
		case NV_REF:
			switch (NObj_Kind(a)) {
			case NO_LIST:
				t = &PyList_Type;
				break;
			case NO_DICT:
				t = &PyDict_Type;
				break;
			case NO_INSTANCE:
				t = &PyInstance_Type;
				break;
			case NO_CLASS:
				t = &PyClass_Type;
				break;
			case NO_METHOD:
				t = &PyMethod_Type;
				break;
			case NO_MODULE:
				t = &PyModule_Type;
				break;
			default:
				t = !strcmp(NObj_TypeName(a), "function") ? &PyFunction_Type : &PyCFunction_Type;
				break;
			}
			break;
		default:
			/* an entity handle: the type of its face */
			return -1;
		}
	}
	Native_SetObj(out, (PyObject *)t);
	return 1;
}

static int TB_Len(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("len", n, 1, 1)) {
		return 0;
	}
	int len = NObj_Len(a);
	if (len < 0) {
		return 0;
	}
	Native_SetInt(out, len);
	return 1;
}

static int TB_Str(nativeVal_t *a, int n, nativeVal_t *out) {
	return TX_Arity("str", n, 1, 1) && TX_Repr(a, NR_STR, out);
}

static int TB_Repr(nativeVal_t *a, int n, nativeVal_t *out) {
	return TX_Arity("repr", n, 1, 1) && TX_Repr(a, NR_REPR, out);
}

static int TB_Int(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("int", n, 1, 1)) {
		return 0;
	}
	return a->type == NV_REF || TX_IsPy(a) ? -1 : Native_ToInt(a, out);
}

static int TB_Float(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("float", n, 1, 1)) {
		return 0;
	}
	return a->type == NV_REF || TX_IsPy(a) ? -1 : Native_ToFloat(a, out);
}

static int TB_RoundFn(nativeVal_t *a, int n, nativeVal_t *out) {
	double x;
	int digits = 0;

	if (!TX_Arity("round", n, 1, 2)) {
		return 0;
	}
	for (int i = 0; i < n; i++) {
		if (a[i].type == NV_REF || TX_IsPy(&a[i])) {
			return -1;
		}
	}
	if (!Native_ArgDouble(&a[0], &x) || (n > 1 && !Native_ArgInt(&a[1], &digits))) {
		return 0;
	}
	Native_SetFloat(out, TX_Round(x, digits));
	return 1;
}

static int TB_Range(nativeVal_t *a, int n, nativeVal_t *out) {
	long lo = 0, hi = 0, step = 1, *bound[3] = { &hi, NULL, NULL };

	if (n < 1 || n > 3) {
		return Native_Raise(NE_TYPE, "range() requires 1-3 int arguments");
	}
	if (n > 1) {
		bound[0] = &lo;
		bound[1] = &hi;
		bound[2] = &step;
	}
	for (int i = 0; i < n; i++) {
		if (a[i].type == NV_REF || TX_IsPy(&a[i])) {
			return -1;
		}
		if (!Mode_Range(&a[i], bound[i])) {
			return 0;
		}
	}
	if (step == 0) {
		return Native_Raise(NE_VALUE, "zero step for range()");
	}
	/* get_len_of_range */
	unsigned long count = 0;
	if (step > 0 && lo < hi) {
		count = ((unsigned long)hi - (unsigned long)lo - 1) / (unsigned long)step + 1;
	} else if (step < 0 && hi < lo) {
		count = ((unsigned long)lo - (unsigned long)hi - 1) / (0ul - (unsigned long)step) + 1;
	}
	if (count > (unsigned long)INT_MAX) {
		return Native_Raise(NE_OVERFLOW, "range() has too many items");
	}
	Native_SetEmpty(out, NV_LIST);
	for (unsigned long i = 0; i < count; i++, lo += step) {
		Native_SetInt(Native_Append(out), lo);
	}
	return 1;
}

static int TB_Abs(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("abs", n, 1, 1)) {
		return 0;
	}
	return a->type == NV_REF && (NObj_Py(a) || NObj_Kind(a) == NO_INSTANCE) ? -1 : Mode_Abs(a, out);
}

/* min (sign -1) and max (1) */
static int TB_MinMax(nativeVal_t *a, int n, nativeVal_t *out, int sign) {
	nativeVal_t args, w;
	const nativeVal_t *seq = &args;
	byte has = 0;

	if (n > 1) {
		args.type = NV_TUPLE;
		args.items = a;
		args.n = n;
	} else if (!TX_Arity("min/max", n, 1, 1)) {
		return 0;
	} else {
		int k = TX_SeqOf(a);
		if (k <= 0) {
			return k < 0 ? -1 : Native_Raise(NE_TYPE, "min() or max() of non-sequence");
		}
		seq = a;
	}
	Native_SetNone(&w);
	for (int i = 0;; i++) {
		nativeVal_t x;
		int len = n > 1 ? n : TX_SeqLen(seq);
		if (len < 0) {
			Native_Free(&w);
			return 0;
		}
		if (i >= len) {
			break;
		}
		if (!(n > 1 ? (Native_Copy(&x, &a[i]), 1) : TX_SeqGet(seq, i, &x))) {
			Native_Free(&w);
			return 0;
		}
		if (!has || Native_Compare(&x, &w) * sign > 0) {
			Native_Free(&w);
			w = x;
			has = 1;
		} else {
			Native_Free(&x);
		}
	}
	if (!has) {
		return Native_Raise(NE_VALUE, "min() or max() of empty sequence");
	}
	*out = w;
	return 1;
}

static int TB_Min(nativeVal_t *a, int n, nativeVal_t *out) {
	return TB_MinMax(a, n, out, -1);
}

static int TB_Max(nativeVal_t *a, int n, nativeVal_t *out) {
	return TB_MinMax(a, n, out, 1);
}

static int TB_Cmp(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("cmp", n, 2, 2)) {
		return 0;
	}
	/* the compare of texts and of containers gives the difference of two bytes, not only the sign: Python's */
	for (int i = 0; i < 2; i++) {
		if (a[i].type != NV_INT && a[i].type != NV_FLOAT && a[i].type != NV_LONG && a[i].type != NV_NONE) {
			return -1;
		}
	}
	Native_SetInt(out, Native_Compare(&a[0], &a[1]));
	return 1;
}

/* tuple(v) and list(v) */
static int TB_Seq(nativeVal_t *a, int n, nativeVal_t *out, nativeType_t type) {
	if (!TX_Arity(type == NV_TUPLE ? "tuple" : "list", n, 1, 1)) {
		return 0;
	}
	if (type == NV_TUPLE && a->type == NV_TUPLE) {
		Native_Copy(out, a);
		return 1;
	}
	int k = TX_SeqOf(a);
	if (k <= 0) {
		return k < 0 ? -1 : Native_Raise(NE_TYPE, type == NV_TUPLE ? "tuple() argument must be a sequence" :
			"list() argument must be a sequence");
	}
	return TX_SeqList(a, type, out);
}

static int TB_Tuple(nativeVal_t *a, int n, nativeVal_t *out) {
	return TB_Seq(a, n, out, NV_TUPLE);
}

static int TB_List(nativeVal_t *a, int n, nativeVal_t *out) {
	return TB_Seq(a, n, out, NV_LIST);
}

static int TB_Callable(nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t call;
	int k;

	if (!TX_Arity("callable", n, 1, 1)) {
		return 0;
	}
	if (a->type == NV_OBJ) {
		return -1;
	}
	k = NObj_Kind(a);
	if (k == NO_INSTANCE) {
		/* an instance with __call__ */
		k = NObj_GetAttr(a, "__call__", &call);
		Native_Free(&call);
		if (!k) {
			PyErr_Clear();
		}
	} else {
		k = k == NO_FUNCTION || k == NO_METHOD || k == NO_CLASS;
	}
	Native_SetInt(out, k);
	return 1;
}

static int TB_Id(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("id", n, 1, 1)) {
		return 0;
	}
	/* OBJLIB 9: the address of the Python object of a view (a value without one is made into one by Python's id) */
	PyObject *py = a->type == NV_REF ? NObj_Py(a) : a->type == NV_OBJ ? a->obj : NULL;
	if (!py) {
		return -1;
	}
	Native_SetInt(out, (long)(intptr_t)py);
	return 1;
}

static int TB_Import(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity("__import__", n, 1, 4) || !TX_ArgText("__import__", 1, a, 1)) {
		return 0;
	}
	/* `import name`; a package or the globals, locals and fromlist of the call: Python's */
	if (n > 1 || !a->n || strchr(Native_Text(a), '.')) {
		return -1;
	}
	return TX_Result(Script_Module(Native_Text(a)), out);
}

/* tc_sdn: the names of the objects of x */
static byte TP_SdnVal(const nativeVal_t *x, int depth, nativeVal_t *out) {
	nativeVal_t name, cls;

	Native_SetNone(out);
	if (depth > TX_EVAL_DEPTH) {
		PyErr_SetString(PyExc_RuntimeError, "maximum recursion depth exceeded");
		return 0;
	}
	if (x->type == NV_TUPLE || x->type == NV_VEC || x->type == NV_LIST || NObj_Kind(x) == NO_LIST) {
		/* the items walked; a tuple stays a tuple, a list becomes a new one */
		nativeVal_t items;
		if (!TX_SeqList(x, x->type == NV_TUPLE || x->type == NV_VEC ? NV_TUPLE : NV_LIST, &items)) {
			return 0;
		}
		Native_SetEmpty(out, items.type);
		for (int i = 0; i < items.n; i++) {
			if (!TP_SdnVal(&items.items[i], depth + 1, Native_Append(out))) {
				Native_Free(&items);
				Native_Free(out);
				return 0;
			}
		}
		Native_Free(&items);
		return 1;
	}
	if (TX_IsDict(x)) {
		/* a new dict: the keys in the order of x, the values walked */
		nativeVal_t pairs;
		if (!TX_Pairs(x, &pairs)) {
			return 0;
		}
		Native_SetEmpty(out, NV_DICT);
		for (int i = 0; i < pairs.n; i++) {
			nativeVal_t k, v;
			if (!TP_SdnVal(&pairs.items[i].items[1], depth + 1, &v)) {
				Native_Free(&pairs);
				Native_Free(out);
				return 0;
			}
			Native_Copy(&k, &pairs.items[i].items[0]);
			Native_DictSet(out, &k, &v);
		}
		Native_Free(&pairs);
		return 1;
	}
	if (x->type == NV_FLOAT) {
		Native_SetFloat(out, TX_Round(x->f, 2));
		return 1;
	}
	if (x->type == NV_ENT) {
		/* 'ent:' + x.Name; '?' when it gives no text */
		if (TX_GetAttr(x, "Name", &name) && name.type == NV_STR) {
			nativeVal_t prefix;
			Native_SetStr(&prefix, "ent:");
			byte ok = Native_Arith('+', &prefix, &name, out);
			Native_Free(&name);
			return ok;
		}
		Native_Free(&name);
		PyErr_Clear();
		Native_SetStr(out, "ent:?");
		return 1;
	}
	if (x->type != NV_STR) {
		if (TX_GetAttr(x, "__name__", &name)) {
			/* 'fn:' + x.__name__ (the TypeError of + for a name that is no text) */
			nativeVal_t prefix;
			Native_SetStr(&prefix, "fn:");
			byte ok = name.type == NV_STR ? Native_Arith('+', &prefix, &name, out)
				: Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
			Native_Free(&name);
			return ok;
		}
		PyErr_Clear();
	}
	if (NObj_Kind(x) == NO_INSTANCE) {
		/* 'inst:' + the name of its class */
		byte ok = NObj_GetAttr(x, "__class__", &cls) && NObj_GetAttr(&cls, "__name__", &name);
		Native_Free(&cls);
		if (ok) {
			nativeVal_t prefix;
			Native_SetStr(&prefix, "inst:");
			ok = Native_Arith('+', &prefix, &name, out);
		}
		Native_Free(&name);
		return ok;
	}
	Native_Copy(out, x);
	return 1;
}

static int TP_Sdn(nativeVal_t *a, int n, nativeVal_t *out) {
	return TX_Arity(NULL, n, 1, 1) && TP_SdnVal(a, 0, out);
}

/* f(*args, **kw) as PyEval_CallObjectWithKeywords takes the arguments (args a tuple, kw None or a dict): 0 with the
 * error */
static byte TP_CallArgs(nativeVal_t *f, const nativeVal_t *args, const nativeVal_t *kw, nativeVal_t *out) {
	nativeVal_t list;

	Native_SetNone(out);
	if (args->type != NV_TUPLE) {
		return Native_Raise(NE_TYPE, "argument list must be a tuple");
	}
	if (kw && kw->type != NV_NONE && !TX_IsDict(kw)) {
		return Native_Raise(NE_TYPE, "keyword list must be a dictionary");
	}
	Native_Copy(&list, args);
	byte ok = TX_Call(f, list.items, list.n, kw && TX_HasKw(kw) ? kw : NULL, out);
	Native_Free(&list);
	return ok;
}

/* the pending error as (type, value) native values, cleared (sys.exc_type and sys.exc_value in an except clause).
 * OBJLIB 7: the error state of Python */
static void TP_Caught(nativeVal_t *type, nativeVal_t *value) {
	PyObject *t, *v, *tb;

	PyErr_Fetch(&t, &v, &tb);
	PyErr_NormalizeException(&t, &v, &tb);
	NObj_FromPy(t ? t : Py_None, type);
	NObj_FromPy(v ? v : Py_None, value);
	Py_XDECREF(t);
	Py_XDECREF(v);
	Py_XDECREF(tb);
}

static int TP_Catch(nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t tv[2];

	if (!TX_Arity(NULL, n, 2, 4)) {
		return 0;
	}
	if (TP_CallArgs(&a[0], &a[1], n > 2 ? &a[2] : NULL, out)) {
		return 1;
	}
	TP_Caught(&tv[0], &tv[1]);
	byte ok = 1;
	if (n < 4 || a[3].type == NV_NONE) {
		Native_SetEmpty(out, NV_TUPLE);
		*Native_Append(out) = tv[0];
		*Native_Append(out) = tv[1];
	} else {
		ok = TX_Call(&a[3], tv, 2, NULL, out);
		Native_Free(&tv[0]);
		Native_Free(&tv[1]);
	}
	return ok;
}

static int TP_Try(nativeVal_t *a, int n, nativeVal_t *out) {
	if (!TX_Arity(NULL, n, 3, 3)) {
		return 0;
	}
	if (!TP_CallArgs(&a[0], &a[1], NULL, out)) {
		PyErr_Clear();
		Native_Copy(out, &a[2]);
	}
	return 1;
}

static int TP_Finally(nativeVal_t *a, int n, nativeVal_t *out) {
	PyObject *t, *v, *tb;
	nativeVal_t r;

	if (!TX_Arity(NULL, n, 3, 3)) {
		return 0;
	}
	byte ok = TP_CallArgs(&a[0], &a[1], NULL, out);
	PyErr_Fetch(&t, &v, &tb);
	if (!TX_Call(&a[2], NULL, 0, NULL, &r)) {
		/* an exception of the cleanup replaces the one before, as in a finally clause */
		Py_XDECREF(t);
		Py_XDECREF(v);
		Py_XDECREF(tb);
		Native_Free(out);
		return 0;
	}
	Native_Free(&r);
	PyErr_Restore(t, v, tb);
	return ok;
}

static int TP_Swap(nativeVal_t *a, int n, nativeVal_t *out) {
	PyObject *t, *v, *tb;
	nativeVal_t old;

	if (!TX_Arity(NULL, n, 4, 4) || !TX_ArgText(NULL, 2, &a[1], 0)) {
		return 0;
	}
	const char *name = Native_Text(&a[1]);
	if (!TX_GetAttr(&a[0], name, &old)) {
		return TX_NullResult();
	}
	if (!TX_SetAttr(&a[0], name, &a[2])) {
		Native_Free(&old);
		return TX_NullResult();
	}
	byte ok = TX_Call(&a[3], NULL, 0, NULL, out);
	PyErr_Fetch(&t, &v, &tb);
	if (!TX_SetAttr(&a[0], name, &old)) {
		PyErr_Clear();
	}
	PyErr_Restore(t, v, tb);
	Native_Free(&old);
	return ok;
}

/* a new class name ("Module.Class" sets its module, else __main__) with the items of the dict attrs (NULL or None:
 * none): 0 with the error */
static byte TP_MakeClass(const nativeVal_t *name, const nativeVal_t *attrs, nativeVal_t *out) {
	const char *full = Native_Text(name), *dot = strrchr(full, '.');
	nativeVal_t dict, pairs, k, v;

	Native_SetNone(out);
	if (attrs && attrs->type != NV_NONE && !TX_IsDict(attrs)) {
		return Native_Raise(NE_TYPE, "the attributes must be a dict");
	}
	Native_SetEmpty(&dict, NV_DICT);
	if (attrs && attrs->type != NV_NONE) {
		if (!TX_Pairs(attrs, &pairs)) {
			return 0;
		}
		for (int i = 0; i < pairs.n; i++) {
			Native_Copy(&k, &pairs.items[i].items[0]);
			Native_Copy(&v, &pairs.items[i].items[1]);
			Native_DictSet(&dict, &k, &v);
		}
		Native_Free(&pairs);
	}
	/* __module__ before PyClass_New adds __doc__ */
	Native_SetStr(&k, "__module__");
	if (dot) {
		Native_SetText(&v, full, (int)(dot - full));
	} else {
		Native_SetStr(&v, "__main__");
	}
	Native_DictSet(&dict, &k, &v);
	return NObj_NewClass(out, dot ? dot + 1 : full, NULL, NULL, &dict);
}

static int TP_Class(nativeVal_t *a, int n, nativeVal_t *out) {
	return TX_Arity(NULL, n, 1, 2) && TX_ArgText(NULL, 1, a, 1) && TP_MakeClass(a, n > 1 ? &a[1] : NULL, out);
}

static int TP_Obj(nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t klass, pairs;
	static const char *const special[2] = { "__call__", "__repr__" };

	if (!TX_Arity(NULL, n, 2, 4) || !TX_ArgText(NULL, 1, a, 1) || !TP_MakeClass(a, NULL, &klass)) {
		return 0;
	}
	byte ok = NObj_Call(&klass, NULL, 0, out);
	Native_Free(&klass);
	if (ok && a[1].type != NV_NONE) {
		if (!TX_IsDict(&a[1])) {
			ok = Native_Raise(NE_TYPE, "the attributes must be a dict");
		} else if ((ok = TX_Pairs(&a[1], &pairs)) != 0) {
			for (int i = 0; ok && i < pairs.n; i++) {
				const nativeVal_t *k = &pairs.items[i].items[0];
				ok = k->type == NV_STR ? TX_SetAttr(out, Native_Text(k), &pairs.items[i].items[1])
					: Native_Raise(NE_TYPE, "attribute name must be string");
			}
			Native_Free(&pairs);
		}
	}
	for (int i = 0; ok && i < 2; i++) {
		if (n > i + 2 && a[i + 2].type != NV_NONE) {
			ok = TX_SetAttr(out, special[i], &a[i + 2]);
		}
	}
	if (!ok) {
		Native_Free(out);
	}
	return ok;
}

static int TP_Walk(nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t x;

	if (!TX_Arity(NULL, n, 2, 2)) {
		return 0;
	}
	Native_SetEmpty(out, NV_LIST);
	Native_Copy(&x, &a[0]);
	for (int k = 0; Native_True(&x) && k <= 100000; k++) {
		nativeVal_t arg, y;
		Native_Copy(Native_Append(out), &x);
		arg = x;
		byte ok = TX_Call(&a[1], &arg, 1, NULL, &y);
		Native_Free(&arg);
		if (!ok) {
			Native_Free(out);
			return 0;
		}
		x = y;
	}
	Native_Free(&x);
	return 1;
}

static int TP_Sorted(nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t slice, r;

	if (!TX_Arity(NULL, n, 1, 1) || !NObj_GetSlice(a, 0, NOBJ_END, &slice)) {
		return 0;
	}
	/* a new list of the items, sorted as list.sort does */
	byte ok = TX_SeqList(&slice, NV_LIST, out);
	Native_Free(&slice);
	if (ok) {
		NObj_Share(out);
		ok = NObj_CallMethod(out, "sort", NULL, 0, &r);
		Native_Free(&r);
	}
	if (!ok) {
		Native_Free(out);
	}
	return ok;
}

static int TP_Set(nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return TX_Arity(NULL, n, 3, 3) && NObj_SetItem(&a[0], &a[1], &a[2]);
}

static int TP_Del(nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return TX_Arity(NULL, n, 2, 2) && NObj_DelItem(&a[0], &a[1]);
}

static int TP_Clear(nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return TX_Arity(NULL, n, 1, 1) && NObj_SetSlice(a, 0, NOBJ_END, NULL);
}

/* the native functions: the channel's own and the twins first (in the order of tx_defs), then the builtins */
static const struct {
	const char *name;
	tcNative_t fn;
} tx_natives[] = {
	{ "tc_sdn", TP_Sdn },
	{ "tc_catch", TP_Catch },
	{ "tc_try", TP_Try },
	{ "tc_finally", TP_Finally },
	{ "tc_swap", TP_Swap },
	{ "tc_obj", TP_Obj },
	{ "tc_class", TP_Class },
	{ "tc_walk", TP_Walk },
	{ "tc_sorted", TP_Sorted },
	{ "tc_set", TP_Set },
	{ "tc_del", TP_Del },
	{ "tc_clear", TP_Clear },
	{ "getattr", TB_GetAttr },
	{ "hasattr", TB_HasAttr },
	{ "setattr", TB_SetAttr },
	{ "delattr", TB_DelAttr },
	{ "map", TB_Map },
	{ "filter", TB_Filter },
	{ "reduce", TB_Reduce },
	{ "apply", TB_Apply },
	{ "type", TB_Type },
	{ "len", TB_Len },
	{ "str", TB_Str },
	{ "repr", TB_Repr },
	{ "int", TB_Int },
	{ "float", TB_Float },
	{ "round", TB_RoundFn },
	{ "range", TB_Range },
	{ "abs", TB_Abs },
	{ "min", TB_Min },
	{ "max", TB_Max },
	{ "cmp", TB_Cmp },
	{ "tuple", TB_Tuple },
	{ "list", TB_List },
	{ "callable", TB_Callable },
	{ "__import__", TB_Import },
	{ "id", TB_Id },
};

#define TX_OWN 16		/* the channel's functions and the twins: tx_defs */
#define TX_NATIVES ((int)(sizeof(tx_natives) / sizeof(tx_natives[0])))

/* OBJLIB 9: native function i called from Python: the arguments as native values, the result as a Python object */
static PyObject *TX_Adapt(int i, PyObject *args) {
	int n = PyTuple_Size(args);
	nativeVal_t *a = Mem_Alloc((uint32_t)((size_t)(n + 1) * sizeof(*a))), r;

	for (int k = 0; k < n; k++) {
		TX_FromPy(PyTuple_GET_ITEM(args, k), &a[k]);
	}
	Native_SetNone(&r);
	byte ok = tx_natives[i].fn(a, n, &r) > 0;
	for (int k = 0; k < n; k++) {
		Native_Free(&a[k]);
	}
	Mem_Free(a);
	PyObject *res = ok ? TX_ToPy(&r) : NULL;
	Native_Free(&r);
	if (!ok) {
		TX_NullResult();
	}
	return res;
}

#define TX_ADAPTER(i) \
	static PyObject *TX_Adapt##i(PyObject *self, PyObject *args) { \
		(void)self; \
		return TX_Adapt(i, args); \
	}
TX_ADAPTER(0)
TX_ADAPTER(1)
TX_ADAPTER(2)
TX_ADAPTER(3)
TX_ADAPTER(4)
TX_ADAPTER(5)
TX_ADAPTER(6)
TX_ADAPTER(7)
TX_ADAPTER(8)
TX_ADAPTER(9)
TX_ADAPTER(10)
TX_ADAPTER(11)
TX_ADAPTER(12)
TX_ADAPTER(13)
TX_ADAPTER(14)
TX_ADAPTER(15)

static PyMethodDef tx_defs[TX_OWN] = {
	{ "tc_sdn", TX_Adapt0, METH_VARARGS, NULL },
	{ "tc_catch", TX_Adapt1, METH_VARARGS, NULL },
	{ "tc_try", TX_Adapt2, METH_VARARGS, NULL },
	{ "tc_finally", TX_Adapt3, METH_VARARGS, NULL },
	{ "tc_swap", TX_Adapt4, METH_VARARGS, NULL },
	{ "tc_obj", TX_Adapt5, METH_VARARGS, NULL },
	{ "tc_class", TX_Adapt6, METH_VARARGS, NULL },
	{ "tc_walk", TX_Adapt7, METH_VARARGS, NULL },
	{ "tc_sorted", TX_Adapt8, METH_VARARGS, NULL },
	{ "tc_set", TX_Adapt9, METH_VARARGS, NULL },
	{ "tc_del", TX_Adapt10, METH_VARARGS, NULL },
	{ "tc_clear", TX_Adapt11, METH_VARARGS, NULL },
	{ "getattr", TX_Adapt12, METH_VARARGS, NULL },
	{ "hasattr", TX_Adapt13, METH_VARARGS, NULL },
	{ "setattr", TX_Adapt14, METH_VARARGS, NULL },
	{ "delattr", TX_Adapt15, METH_VARARGS, NULL },
};

/* a new function object of the channel's function name, NULL (no error) when there is none; nothing is kept between
 * lines (the interpreter's objects go with every level load) */
static PyObject *TP_Function(const char *name) {
	for (int i = 0; i < 12; i++) {
		if (!strcmp(tx_defs[i].ml_name, name)) {
			return PyCFunction_New(&tx_defs[i], NULL);
		}
	}
	return NULL;
}

/* the twin of the builtin getattr, hasattr, setattr or delattr (with its doc), NULL for every other name */
static PyObject *TX_AttrTwin(const char *name, PyObject *builtin) {
	if (!PyCFunction_Check(builtin)) {
		return NULL;
	}
	for (int i = 12; i < TX_OWN; i++) {
		if (!strcmp(name, tx_defs[i].ml_name)) {
			tx_defs[i].ml_doc = ((PyCFunctionObject *)builtin)->m_ml->ml_doc;
			return PyCFunction_New(&tx_defs[i], NULL);
		}
	}
	return NULL;
}

static int TX_Native(const nativeVal_t *f, nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out) {
	PyObject *py = TX_Py(f);
	int i = TX_NATIVES;

	if (!py || !PyCFunction_Check(py)) {
		return -1;
	}
	PyCFunctionObject *cf = (PyCFunctionObject *)py;
	if (cf->m_ml == &tx_lambdaDef) {
		nativeVal_t pairs;
		tcFunc *rec = PyCObject_AsVoidPtr(PyTuple_GetItem(cf->m_self, 0));
		if (kw && !TX_Pairs(kw, &pairs)) {
			return 0;
		}
		byte ok = TX_LambdaRun(rec, a, n, kw ? &pairs : NULL, out);
		if (kw) {
			Native_Free(&pairs);
		}
		return ok;
	}
	if (cf->m_ml >= tx_defs && cf->m_ml < tx_defs + TX_OWN) {
		i = (int)(cf->m_ml - tx_defs);
	} else if (!cf->m_self) {
		/* a builtin of that name that is still the builtin's own function object */
		PyObject *b = PyImport_AddModule("__builtin__");
		PyObject *own = b ? PyDict_GetItemString(PyModule_GetDict(b), cf->m_ml->ml_name) : NULL;
		for (i = TX_OWN; i < TX_NATIVES && strcmp(tx_natives[i].name, cf->m_ml->ml_name); i++) {
		}
		if (i == TX_NATIVES || !own || !PyCFunction_Check(own) || ((PyCFunctionObject *)own)->m_ml != cf->m_ml) {
			if (!own || own != py) {
				return -1;
			}
			/* a builtin that is not native here */
			TX_Count("builtin", cf->m_ml->ml_name);
			return TX_CallPy(f, a, n, kw, out);
		}
	} else {
		return -1;
	}
	if (TX_HasKw(kw)) {
		return Native_Raise(NE_TYPE, "this function takes no keyword arguments");
	}
	/* the arguments are copies: a list or dict among them is an object the function may change */
	for (int k = 0; k < n; k++) {
		NObj_Share(&a[k]);
	}
	int r = tx_natives[i].fn(a, n, out);
	if (r < 0) {
		Native_Free(out);
		TX_Count("builtin", tx_natives[i].name);
		return TX_CallPy(f, a, n, NULL, out);
	}
	if (!r) {
		Native_Free(out);
	}
	return r;
}

/* ---- statements ---- */

/* the items of a sequence a tuple or list of targets takes (PySequence_Length, then PySequence_GetItem): by value, of
 * a native list object, of a Python object */
static int X_SeqLength(const nativeVal_t *v) {
	PyObject *py = TX_Py(v);
	int n;

	if (py) {
		return PySequence_Length(py);
	}
	if (v->type == NV_REF) {
		if (NObj_Kind(v) == NO_LIST || NObj_Kind(v) == NO_INSTANCE) {
			return NObj_Len(v);
		}
		Native_Raise(NE_TYPE, "len() of unsized object");
		return -1;
	}
	if (v->type == NV_ENT) {
		Native_Raise(NE_TYPE, "len() of unsized object");
		return -1;
	}
	return Native_Len(v, &n) ? n : -1;
}

static byte X_SeqItem(const nativeVal_t *v, int i, nativeVal_t *out) {
	nativeVal_t key;

	if (v->type == NV_REF || v->type == NV_OBJ) {
		Native_SetInt(&key, i);
		return NObj_GetItem(v, &key, out);
	}
	return Native_SeqItem(v, i, out);
}

static byte X_Store(tcNode *t, const nativeVal_t *v, tcScope *s) {
	nativeVal_t o, k;
	byte ok = 0;
	long lo, hi;

	switch (t->kind) {
	case X_NAME:
		return X_SetName(s, t->name, v);
	case X_ATTR:
		if (X_Eval(t->a, s, &o)) {
			ok = TX_SetAttr(&o, t->name, v);
			if (!ok) {
				X_NoError();
			}
			Native_Free(&o);
		}
		return ok;
	case X_INDEX:
		if (X_Eval(t->a, s, &o)) {
			if (X_Eval(t->b, s, &k)) {
				nativeVal_t x;
				Native_Copy(&x, v);
				TX_Give(&x);
				ok = NObj_SetItem(&o, &k, &x);
				Native_Free(&x);
				Native_Free(&k);
			}
			Native_Free(&o);
		}
		return ok;
	case X_SLICE:
		if (X_Eval(t->a, s, &o)) {
			if (X_Bounds(t, s, &lo, &hi)) {
				nativeVal_t x;
				Native_Copy(&x, v);
				TX_Give(&x);
				ok = NObj_SetSlice(&o, lo, hi, &x);
				Native_Free(&x);
			}
			Native_Free(&o);
		}
		return ok;
	default: {
		int n = X_SeqLength(v);
		if (n < 0) {
			return 0;
		}
		if (n != t->n) {
			return Native_Raise(NE_VALUE, t->kind == X_LIST ? "unpack list of wrong size" : "unpack tuple of wrong size");
		}
		for (int i = 0; i < n; i++) {
			nativeVal_t item;
			ok = X_SeqItem(v, i, &item) && X_Store(t->kids[i], &item, s);
			Native_Free(&item);
			if (!ok) {
				return 0;
			}
		}
		return 1;
	}
	}
}

static byte X_Delete(tcNode *t, tcScope *s) {
	nativeVal_t o, k;
	byte ok = 0;
	long lo, hi;

	switch (t->kind) {
	case X_NAME:
		if ((s->inMod ? ModVar_Del(&s->mv, t->name) : PyDict_DelItemString(s->globals, (char *)t->name)) != 0) {
			PyErr_Clear();
			return Native_Raise(NE_NAME, "%s", t->name);
		}
		return 1;
	case X_ATTR:
		if (X_Eval(t->a, s, &o)) {
			ok = TX_SetAttr(&o, t->name, NULL);
			Native_Free(&o);
		}
		return ok;
	case X_INDEX:
		if (X_Eval(t->a, s, &o)) {
			if (X_Eval(t->b, s, &k)) {
				ok = NObj_DelItem(&o, &k);
				Native_Free(&k);
			}
			Native_Free(&o);
		}
		return ok;
	case X_SLICE:
		if (X_Eval(t->a, s, &o)) {
			ok = X_Bounds(t, s, &lo, &hi) && NObj_SetSlice(&o, lo, hi, NULL);
			Native_Free(&o);
		}
		return ok;
	default:
		for (int i = 0; i < t->n; i++) {
			if (!X_Delete(t->kids[i], s)) {
				return 0;
			}
		}
		return 1;
	}
}

/* one statement; with value set the value of an expression statement (*has 1; Native_Free it) */
static byte X_Exec(tcNode *st, tcScope *s, nativeVal_t *value, byte *has) {
	nativeVal_t v;
	byte ok;

	switch (st->kind) {
	case S_EXPR:
		ok = X_Eval(st->a, s, &v);
		if (ok && value) {
			*value = v;
			*has = 1;
		} else {
			Native_Free(&v);
		}
		return ok;
	case S_ASSIGN:
		ok = X_Eval(st->a, s, &v);
		/* one list or dict for every target of the chain (a = b = []) */
		NObj_Share(&v);
		for (int i = 0; ok && i < st->n; i++) {
			ok = X_Store(st->kids[i], &v, s);
		}
		Native_Free(&v);
		return ok;
	case S_DEL:
		return X_Delete(st->a, s);
	case S_IMPORT:
		for (int i = 0; i < st->n; i++) {
			ok = TX_Result(Script_Module(st->names[i]), &v) && X_SetName(s, st->names[i], &v);
			Native_Free(&v);
			if (!ok) {
				return 0;
			}
		}
		return 1;
	default:
		return 1;
	}
}

/* the statements of a line read: the tree and its S_LIST node; NULL with the error set */
static tcNode *TX_Read(const char *line, tcTree **tree) {
	tcParse ps = { line, TX_New(), 0 };
	tcNode *prog = P_Program(&ps);

	if (!prog) {
		TX_Release(ps.t);
		*tree = NULL;
		return NULL;
	}
	*tree = ps.t;
	return prog;
}

/* runs prog of tree in globals: 0 with the error set; *has 1 with *last the value of a last expression statement
 * (Native_Free it) */
static byte TX_RunTree(tcTree *tree, tcNode *prog, PyObject *globals, nativeVal_t *last, byte *has) {
	tcTree *was = tx_tree;
	tcScope s;
	byte ok = 1;

	X_Scope(&s, NULL, globals);
	tx_tree = tree;
	*has = 0;
	for (int i = 0; ok && i < prog->n; i++) {
		if (*has) {
			Native_Free(last);
			*has = 0;
		}
		ok = X_Exec(prog->kids[i], &s, last, has);
	}
	tx_tree = was;
	return ok;
}

PyObject *TcX_Eval(const char *line) {
	tcTree *tree;
	tcNode *prog = TX_Read(line, &tree);
	nativeVal_t v;
	byte has = 0;
	PyObject *o = NULL;

	if (prog && !(prog->flags & 1)) {
		TX_Syntax(line);
	} else if (prog && TX_RunTree(tree, prog, TcX_Main(), &v, &has) && has) {
		o = TX_ToPy(&v);
	}
	if (has) {
		Native_Free(&v);
	}
	TX_Release(tree);
	return o;
}

PyObject *TcX_Statement(const char *line) {
	tcTree *tree;
	tcNode *prog = TX_Read(line, &tree);
	nativeVal_t v;
	byte has = 0, was = tx_builtinAttr;
	PyObject *o = NULL;

	tx_builtinAttr = 1;
	if (prog && TX_RunTree(tree, prog, TcX_Main(), &v, &has)) {
		o = has ? TX_ToPy(&v) : Py_ReturnNone();
	}
	if (has) {
		Native_Free(&v);
	}
	tx_builtinAttr = was;
	TX_Release(tree);
	return o;
}

/* the global "result" of globals (a variable of a pack module through the registry) into *out: 1, 0 when it has
 * none; del: removed (out unused) */
static byte TX_ResultVar(PyObject *globals, byte del, nativeVal_t *out) {
	modVarRef_t mv;
	byte inMod = ModVar_OfDict(globals, &mv);
	PyObject *v = inMod ? ModVar_Get(&mv, "result") : PyDict_GetItemString(globals, "result");

	if (v && del) {
		if (inMod) {
			ModVar_Del(&mv, "result");
		} else {
			PyDict_DelItemString(globals, "result");
		}
		return 0;
	}
	if (v && !del) {
		TX_FromPy(v, out);
	}
	return v != NULL;
}

void TcX_Run(tcBuf *b, const char *line, PyObject *globals, byte quiet) {
	tcTree *tree;
	tcNode *prog = TX_Read(line, &tree);
	nativeVal_t v;
	byte ok = 0, has = 0;

	if (prog) {
		if (!(prog->flags & 1)) {
			TX_ResultVar(globals, 1, NULL);
		}
		ok = TX_RunTree(tree, prog, globals, &v, &has);
		if (ok && !(prog->flags & 1)) {
			if (has) {
				Native_Free(&v);
			}
			has = TX_ResultVar(globals, 0, &v);
		}
	}
	TX_Release(tree);
	if (!ok) {
		if (has) {
			Native_Free(&v);
		}
		TC_Add(b, "error: ", -1);
		TC_AddError(b);
		TC_Add(b, "\n", 1);
		return;
	}
	if (!quiet) {
		if (has) {
			/* the repr; a failing one gives "?" */
			nativeVal_t text;
			if (TX_Repr(&v, NR_REPR, &text)) {
				TC_Add(b, Native_Text(&text), text.n);
			} else {
				PyErr_Clear();
				TC_Add(b, "?", 1);
			}
			Native_Free(&text);
		} else {
			TC_Add(b, "ok", 2);
		}
		TC_Add(b, "\n", 1);
	}
	if (has) {
		Native_Free(&v);
	}
}
