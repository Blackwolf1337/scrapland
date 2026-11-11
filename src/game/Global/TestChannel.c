/*
 * The test channel: the test tools (the scrap MCP server tools/mcp/scrap_mcp.py, tools/test/golden.sh) talk to a
 * running game through two files in the game folder, with no Python code run in the game. Not in the original. Off in
 * normal play: on with the command line switch -TestChannel or the environment variable SCRAP_TEST_CHANNEL=1.
 *
 * Once a frame (while the world clock runs in cWorld_Update right after the scheduled functions, where the script
 * function of the old harness ran; else at the start of CApp_FrameRun, also while the window is in the background; a
 * command file that is there before the first frame runs while the clock is still at 0) the game reads the first
 * line of test_cmd.txt, a number (optionally followed by " pace=<seconds>": the file waits until that much game time
 * has passed since the last answer, as the old harness polled every 0.1 s of game time). A number it has not seen runs the rest of the file and writes the answers to
 * test_reply.txt as "<number>\n<answers>\n.end\n" (written to test_reply.tmp, then renamed). The tool writes the
 * command file the same way, so neither side sees a file half written. Nothing waits: no file, the same number or a
 * file that cannot be read is a frame without work.
 *
 * Each line of the command file is one command, its answer one or more lines (none for "do" and "recorder"):
 *   <statements>             channel statements (TestExpr.c), answered as the game side of the MCP server answered a
 *                            line of Python: a single expression its repr, statements the repr of the global "result"
 *                            (or ok); several are separated by ";", the first error stops them
 *   do <statements>          the same without an answer (only an error answers)
 *   in <module> [do] <statements>   the statements in the globals of the module (helpers kept in a module)
 *   check <name>\t<expr>     golden: "<name> = <value>": floats with 17 digits, tuples walked, an exception as
 *                            "exception <type>: <value>" (the check() of the old tools/test/golden/sdlib.py)
 *   print <expr>             what the print statement writes for the value (tp_print of the object, else str)
 *   con <line>               a console line (ConsoleCmd.c): what the console shows
 *   get <var> ...            engine variables: [('name', value), ...]
 *   ent <entity> <var> ...   entity variables: [('var', value), ...], '?' for a variable the entity does not have
 *   ents <list> [x y z r]    the sorted names of the entities of an entity list (Scrap.SphereCall lists: 'r', 'i',
 *                            'v', ...) in the sphere, by default the whole map
 *   dump [module ...]        the module variables (all pack modules without names): "== module", then
 *                            "key\t<type value>" per name in the form of the module dump of the stage checks (scratchpad
 *                            nopy0/mdump.py); a pack module's through the registry of the module variables
 *                            (include/scrap/modvars.h), another module's from its dict
 *   menu [player]            the scorer items: (cursor, [[name, Text, X, Y, W, H, Visible, Alpha, Red, Green, Blue,
 *                            OnAccept, Up, Down, Left, Right], ...]) as SScorer.Get gives each value
 *   click <item> [player]    runs the OnAccept of the item: "called <func>" or "the item has no OnAccept"
 *   recorder <target> <list> [<literal> | raise <exception> <text>]   puts a recorder function at target (a module
 *                            variable, e.g. __main__.f or CharAct.onNPCDeath): each call appends its argument tuple
 *                            to the list __main__.<list> and returns the literal (None), or raises
 *   native [str|check] <literal>   native values without Python objects (TestNative.c)
 * An error answers "error: <type>: <value>". The channel statements and expressions (TestExpr.c) are a small part of
 * Python's syntax read and run in C (operators, comparisons, lambdas, assignments, no Python code). The verbs work on
 * native values (scrap/scriptnative.h, scrap/nativeobj.h); what still goes through Python is marked: the error state
 * (OBJLIB 7), the dicts of __main__ and of the modules (OBJLIB 9b) and print of a value of a type the native values do
 * not have (OBJLIB 7e).
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/global.h"
#include "scrap/gvar.h"
#include "scrap/modvars.h"
#include "scrap/pyfunction.h"
#include "scrap/scorer.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"
#include "scrap/testdet.h"
#include "scrap/timer.h"
#include "scrap/world.h"
#include "testchannel.h"


#define TC_CMD "test_cmd.txt"
#define TC_REPLY "test_reply.txt"
#define TC_TMP "test_reply.tmp"
#define TC_DEPTH 32			/* nesting of channel expressions */

static byte tc_on;
static byte tc_started;
static long tc_seq = -1;
static double tc_answerTime;	/* game time of the last answer (pace=) */
static double tc_paceLast;		/* game time of the frame before */
static int tc_paceStill;		/* frames the game clock stood while a paced command waited */
static byte tc_worldRan;		/* the world update looked in the frame before (TestChannel_WorldFrame) */
/* the deterministic test mode (scrap/testdet.h) */
static byte tc_step;			/* SCRAP_TEST_CHANNEL=step: the clock stands between the command files */
static byte tc_hold;			/* the clock stands in this pass of the main loop (TestChannel_Hold) */
static struct {					/* the time since the last answer (TC_DetTrack) */
	uint32_t step;				/* TestDet_Frame() of the last look */
	uint32_t loads;				/* TestDet_Loads() of the last look */
	double gt;					/* game time of the last look */
	double world;				/* game time passed; after a level load the time of the new level */
	double stood;				/* time the game clock stood, without the first TC_STAND_FREE steps of each stand */
	uint32_t still;				/* steps the game clock has stood in a row (not reset by an answer, as tc_paceStill) */
	uint32_t answer;			/* TestDet_Frame() of the last answer */
	uint32_t met;				/* TestDet_Frame() + 1 of the step in which the pace of a command file was met, else 0 */
	byte loaded;				/* a level load ended since the last answer */
} tc_det;
#define TC_STILL_STEPS 60		/* a paced command file runs when the game clock stood that many steps (frames) */
#define TC_STAND_FREE 180		/* steps of a stand that do not count for a wait (the runner's live wait: 3 s) */
#define TC_PACE_SLACK 1e-4		/* the game time is a sum of float steps: 6 steps of 1/60 s are a pace of 0.1 */

/* ---- the answer: a growing text ---- */

void TC_Add(tcBuf *b, const char *s, int n) {
	if (n < 0) {
		n = (int)strlen(s);
	}
	if (b->len + n + 1 > b->cap) {
		int cap = b->cap ? b->cap : 0x1000;
		while (b->len + n + 1 > cap) {
			cap *= 2;
		}
		b->p = Mem_Realloc(b->p, (uint32_t)cap);
		b->cap = cap;
	}
	memcpy(b->p + b->len, s, (size_t)n);
	b->len += n;
	b->p[b->len] = 0;
}

void TC_Addf(tcBuf *b, const char *fmt, ...) {
	char tmp[0x800];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	TC_Add(b, tmp, -1);
}

/* OBJLIB 7: str() of a Python object of the error state; a failing one gives "?" and clears the error */
static void TC_AddPyStr(tcBuf *b, PyObject *o) {
	PyObject *s = PyObject_Str(o);

	if (s && PyString_Check(s)) {
		TC_Add(b, PyString_AsString(s), PyString_Size(s));
	} else {
		PyErr_Clear();
		TC_Add(b, "?", 1);
	}
	Py_XDECREF(s);
}

void TC_AddError(tcBuf *b) {
	PyObject *t, *v, *tb;

	PyErr_Fetch(&t, &v, &tb);
	PyErr_NormalizeException(&t, &v, &tb);
	TC_AddPyStr(b, t ? t : Py_None);
	TC_Add(b, ": ", 2);
	TC_AddPyStr(b, v ? v : Py_None);
	Py_XDECREF(t);
	Py_XDECREF(v);
	Py_XDECREF(tb);
	PyErr_Clear();
}

/* 0 (the error set) when the repr of an object inside the list, tuple or dict by value v fails: Native_Repr leaves
 * such an item out, the repr of Python fails as a whole */
static byte TC_ReprOk(const nativeVal_t *v) {
	const nativeVal_t *k, *x;
	nativeBuf_t t;
	int pos = 0;
	byte ok = 1;

	switch (v->type) {
	case NV_LIST:
	case NV_TUPLE:
		for (int i = 0; ok && i < v->n; i++) {
			ok = TC_ReprOk(&v->items[i]);
		}
		break;
	case NV_DICT:
		while (ok && Native_DictNext(v, &pos, &k, &x)) {
			ok = TC_ReprOk(k) && TC_ReprOk(x);
		}
		break;
	case NV_REF:
		Native_BufInit(&t);
		ok = NObj_Repr(&t, v, NR_REPR);
		Native_BufFree(&t);
		break;
	}
	return ok;
}

/* repr(), str() or the check() form of v; a failing one gives "?" (OBJLIB 7: the error cleared) */
static void TC_AddVal(tcBuf *b, const nativeVal_t *v, nativeRepr_t mode) {
	nativeBuf_t t;

	Native_BufInit(&t);
	if ((v->type == NV_REF || TC_ReprOk(v)) && NObj_Repr(&t, v, mode)) {
		TC_Add(b, t.p, t.len);
	} else {
		PyErr_Clear();
		TC_Add(b, "?", 1);
	}
	Native_BufFree(&t);
}

static void TC_AddRepr(tcBuf *b, const char *s) {
	char tmp[0x1000];

	Console_ReprString(tmp, sizeof(tmp), s);
	TC_Add(b, tmp, -1);
}

static void TC_AddFloat(tcBuf *b, double d) {
	char tmp[0x40];

	Console_ReprFloat(tmp, sizeof(tmp), d);
	TC_Add(b, tmp, -1);
}

static void TC_Blanks(const char **pp) {
	while (**pp == ' ' || **pp == '\t') {
		(*pp)++;
	}
}

static byte TC_NameChar(char c, byte first) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || (!first && c >= '0' && c <= '9');
}

static void TC_Syntax(const char *at) {
	char msg[0x100];

	snprintf(msg, sizeof(msg), "channel expression: cannot read at '%.60s'", at);
	PyErr_SetString(PyExc_SyntaxError, msg);
}

/* ---- golden: the value as check() of the old sdlib.py wrote it ---- */

/* floats with 17 digits, tuples walked (a vector is a tuple of three floats), the rest as repr ("?" when it fails) */
static void TC_Fmt(tcBuf *b, const nativeVal_t *v) {
	if (v->type == NV_FLOAT) {
		TC_AddVal(b, v, NR_CHECK);
	} else if (v->type == NV_TUPLE || v->type == NV_VEC) {
		int n = v->type == NV_VEC ? 3 : v->n;
		TC_Add(b, "(", 1);
		for (int i = 0; i < n; i++) {
			if (i) {
				TC_Add(b, ", ", 2);
			}
			if (v->type == NV_VEC) {
				nativeVal_t f;
				Native_SetFloat(&f, v->v[i]);
				TC_Fmt(b, &f);
			} else {
				TC_Fmt(b, &v->items[i]);
			}
		}
		TC_Add(b, ")", 1);
	} else {
		TC_AddVal(b, v, NR_REPR);
	}
}

static void TC_Check(tcBuf *b, const char *arg) {
	const char *tab = strchr(arg, '\t');
	nativeVal_t v;

	if (!tab) {
		TC_Add(b, "error: check needs <name>\\t<expr>\n", -1);
		return;
	}
	TC_Add(b, arg, (int)(tab - arg));
	TC_Add(b, " = ", 3);
	if (TcX_Statement(tab + 1, &v)) {
		TC_Fmt(b, &v);
		Native_Free(&v);
	} else {
		TC_Add(b, "exception ", -1);
		TC_AddError(b);
	}
	TC_Add(b, "\n", 1);
}

/* ---- print: what the print statement writes ---- */

#define TC_PRINT_DEPTH 64

/* the lists and dicts being printed (Py_ReprEnter: one that holds itself prints as [...] or {...}) */
typedef struct tcPrint {
	const void *open[TC_PRINT_DEPTH];
	int depth;
} tcPrint;

/* OBJLIB 7e: a Python value of a type the native values do not have, through its tp_print (a temporary file) */
static byte TC_PrintPy(tcBuf *b, PyObject *o, byte raw) {
	FILE *f = tmpfile();
	byte ok = f && PyObject_Print(o, f, raw ? Py_PRINT_RAW : 0) == 0;

	if (ok) {
		char tmp[0x400];
		size_t n;
		rewind(f);
		while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) {
			TC_Add(b, tmp, (int)n);
		}
	}
	if (f) {
		fclose(f);
	}
	return ok;
}

static byte TC_PrintVal(tcBuf *b, const nativeVal_t *v, byte raw, tcPrint *pr);

/* the items of a sequence between open and close, each printed as an item (not raw): tuple_print and list_print */
static byte TC_PrintSeq(tcBuf *b, const nativeVal_t *v, const char *open, const char *close, tcPrint *pr) {
	int n = TcX_SeqLen(v);
	byte ok = n >= 0;

	TC_Add(b, open, 1);
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t x;
		Native_SetNone(&x);
		if (i) {
			TC_Add(b, ", ", 2);
		}
		ok = TcX_SeqGet(v, i, &x) && TC_PrintVal(b, &x, 0, pr);
		Native_Free(&x);
	}
	if (ok && n == 1 && *open == '(') {
		TC_Add(b, ",", 1);
	}
	TC_Add(b, close, 1);
	return ok;
}

/* the keys and values of a dict in its order: dict_print */
static byte TC_PrintDict(tcBuf *b, const nativeVal_t *v, tcPrint *pr) {
	nativeVal_t items;
	byte ok = 1;

	Native_SetNone(&items);
	if (v->type == NV_DICT) {
		const nativeVal_t *k, *x;
		int pos = 0;
		TC_Add(b, "{", 1);
		for (byte first = 1; ok && Native_DictNext(v, &pos, &k, &x); first = 0) {
			if (!first) {
				TC_Add(b, ", ", 2);
			}
			ok = TC_PrintVal(b, k, 0, pr);
			TC_Add(b, ": ", 2);
			ok = ok && TC_PrintVal(b, x, 0, pr);
		}
		TC_Add(b, "}", 1);
		return ok;
	}
	if (!NObj_CallMethod(v, "items", NULL, 0, &items)) {
		return 0;
	}
	int n = TcX_SeqLen(&items);
	ok = n >= 0;
	TC_Add(b, "{", 1);
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t pair;
		Native_SetNone(&pair);
		if (i) {
			TC_Add(b, ", ", 2);
		}
		ok = TcX_SeqGet(&items, i, &pair) && pair.type == NV_TUPLE && pair.n == 2;
		ok = ok && TC_PrintVal(b, &pair.items[0], 0, pr);
		TC_Add(b, ": ", 2);
		ok = ok && TC_PrintVal(b, &pair.items[1], 0, pr);
		Native_Free(&pair);
	}
	TC_Add(b, "}", 1);
	Native_Free(&items);
	return ok;
}

/* v as PyObject_Print writes it (raw: Py_PRINT_RAW, a text as it is): the tp_print of ints, floats, texts, tuples,
 * lists, dicts and entity handles, else str (raw) or repr. 0 with the error set */
static byte TC_PrintVal(tcBuf *b, const nativeVal_t *v, byte raw, tcPrint *pr) {
	char tmp[0x100];

	switch (v->type) {
	case NV_STR:
		if (raw) {
			TC_Add(b, Native_Text(v), v->n);
			return 1;
		}
		break;
	case NV_ENT:
		/* the text goes out through fprintf as its format (the original's tp_print); here as it is */
		Native_EntPrint(Native_EntName(v), tmp, sizeof(tmp));
		TC_Add(b, tmp, -1);
		return 1;
	case NV_OBJ:
		/* a Python value of a type without a native kind, unlike a view (NV_REF below), which prints natively */
		return TC_PrintPy(b, Native_PyOf(v), raw);
	case NV_TUPLE:
	case NV_VEC:
		return TC_PrintSeq(b, v, "(", ")", pr);
	case NV_LIST:
		return TC_PrintSeq(b, v, "[", "]", pr);
	case NV_DICT:
		return TC_PrintDict(b, v, pr);
	case NV_REF:
		if (NObj_Kind(v) == NO_LIST || NObj_Kind(v) == NO_DICT) {
			byte list = NObj_Kind(v) == NO_LIST;
			for (int i = 0; i < pr->depth && i < TC_PRINT_DEPTH; i++) {
				if (pr->open[i] == v->ref) {
					TC_Add(b, list ? "[...]" : "{...}", 5);
					return 1;
				}
			}
			/* deeper than the room: printed without the check */
			if (pr->depth < TC_PRINT_DEPTH) {
				pr->open[pr->depth] = v->ref;
			}
			pr->depth++;
			byte ok = list ? TC_PrintSeq(b, v, "[", "]", pr) : TC_PrintDict(b, v, pr);
			pr->depth--;
			return ok;
		}
		break;
	}
	nativeBuf_t t;
	Native_BufInit(&t);
	byte ok = NObj_Repr(&t, v, raw ? NR_STR : NR_REPR);
	if (ok) {
		TC_Add(b, t.p, t.len);
	}
	Native_BufFree(&t);
	return ok;
}

/* print <expr>: the print statement */
static void TC_Print(tcBuf *b, const char *arg) {
	nativeVal_t v;
	tcBuf out = { 0 };
	tcPrint pr;

	pr.depth = 0;
	if (TcX_Eval(arg, &v) && TC_PrintVal(&out, &v, 1, &pr)) {
		TC_Add(b, out.p ? out.p : "", out.len);
		TC_Add(b, "\n", 1);
	} else {
		TC_Add(b, "error: ", -1);
		TC_AddError(b);
		TC_Add(b, "\n", 1);
	}
	Native_Free(&v);
	if (out.p) {
		Mem_Free(out.p);
	}
}

/* ---- recorders ---- */

/* what a recorder keeps: the list its calls go to, what a call returns, or the exception it raises (exc not None) */
typedef struct tcRecorder {
	nativeVal_t calls, value, exc, text;
} tcRecorder;

/* the arguments appended to the list as a tuple, then the value or the exception */
static byte TC_RecorderCall(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	tcRecorder *r = ctx;
	nativeVal_t args, res;

	Native_SetEmpty(&args, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(&args), &a[i]);
	}
	byte ok = NObj_CallMethod(&r->calls, "append", &args, 1, &res);
	Native_Free(&args);
	Native_Free(&res);
	if (!ok) {
		return 0;
	}
	if (r->exc.type != NV_NONE) {
		/* OBJLIB 7: the error state of Python */
		PyObject *e = Native_ToPy(&r->exc), *t = e ? Native_ToPy(&r->text) : NULL;
		if (t) {
			PyErr_SetObject(e, t);
		}
		Py_XDECREF(e);
		Py_XDECREF(t);
		return 0;
	}
	Native_Copy(out, &r->value);
	return 1;
}

static void TC_RecorderFree(void *ctx) {
	tcRecorder *r = ctx;

	Native_Free(&r->calls);
	Native_Free(&r->value);
	Native_Free(&r->exc);
	Native_Free(&r->text);
	Mem_Free(r);
}

/* recorder <target> <list> [<literal> | raise <exception> <text>] */
static byte TC_Recorder(const char *arg) {
	char target[0x200], list[0x80], line[0x300];
	const char *p = arg;
	int n = 0;
	nativeVal_t value, exc, f;
	byte ok = 0;

	for (int k = 0; k < 2; k++) {
		char *dst = k ? list : target;
		int size = k ? (int)sizeof(list) : (int)sizeof(target);
		TC_Blanks(&p);
		for (n = 0; *p && *p != ' ' && *p != '\t'; p++) {
			if (n < size - 1) {
				dst[n++] = *p;
			}
		}
		dst[n] = 0;
	}
	TC_Blanks(&p);
	if (!target[0] || !list[0]) {
		TC_Syntax(arg);
		return 0;
	}
	Native_SetNone(&value);
	Native_SetNone(&exc);
	if (!strncmp(p, "raise ", 6)) {
		char name[0x80];
		p += 6;
		TC_Blanks(&p);
		for (n = 0; TC_NameChar(*p, 0); p++) {
			if (n < (int)sizeof(name) - 1) {
				name[n++] = *p;
			}
		}
		name[n] = 0;
		TC_Blanks(&p);
		ok = TcX_Name(name, &exc);
	} else {
		ok = !*p || TcX_Eval(p, &value);
	}
	if (!ok) {
		return 0;
	}
	tcRecorder *r = Mem_Alloc(sizeof(tcRecorder));
	NObj_NewList(&r->calls);
	/* one object for every call */
	NObj_Share(&value);
	r->value = value;
	r->exc = exc;
	Native_SetNone(&r->text);
	if (exc.type != NV_NONE) {
		Native_SetText(&r->text, p, (int)strlen(p));
	}
	ok = 0;
	/* the self the dump shows: (calls, value, exception, text) before batch 8b */
	nativeVal_t self[3];
	self[0] = r->value;
	self[1] = r->exc;
	self[2] = r->text;
	if (TcX_NewFunc(&f, "recorder", TC_RecorderCall, r, TC_RecorderFree, self)
		&& (strcmp(list, "-") == 0 || TcX_SetMain(list, &r->calls))) {
		/* the function goes to the target as an assignment would put it there */
		snprintf(line, sizeof(line), "%s = __tc_recorder", target);
		ok = TcX_Bind(line, "__tc_recorder", &f);
	}
	Native_Free(&f);
	return ok;
}

/* the next blank separated word of *pp into buf; 0 at the end */
static byte TC_Word(const char **pp, char *buf, int size) {
	int n = 0;

	TC_Blanks(pp);
	for (; **pp && **pp != ' ' && **pp != '\t'; (*pp)++) {
		if (n < size - 1) {
			buf[n++] = **pp;
		}
	}
	buf[n] = 0;
	return n > 0;
}

typedef struct tcNames {
	const char **names;
	int n, cap;
} tcNames;

static void TC_NamePush(tcNames *l, const char *name) {
	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 64;
		l->names = Mem_Realloc(l->names, (uint32_t)(sizeof(*l->names) * (size_t)l->cap));
	}
	l->names[l->n++] = name;
}

static int TC_CmpName(const void *a, const void *b) {
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* ---- the module dump (the form of scratchpad nopy0/mdump.py) ---- */

/* the lists, dicts, classes and instances shown so far and their paths (a later one shows as "alias <path>"); the
 * values are held, so a view stays the one object of its Python object */
typedef struct tcSeen {
	nativeVal_t *vals;
	char **paths;
	int n, cap;
} tcSeen;

static void TC_MdVal(tcBuf *b, const nativeVal_t *v, const char *path, tcSeen *seen, int depth);

/* a key of a dict: its repr and its place in keys() */
typedef struct tcKey {
	char *repr;
	int i;
} tcKey;

static int TC_KeyCmp(const void *a, const void *b) {
	const tcKey *x = a, *y = b;
	int c = strcmp(x->repr, y->repr);

	return c ? c : x->i - y->i;
}

/* "key: value" pairs of a dict, sorted by the repr of the key */
static void TC_MdDict(tcBuf *b, const nativeVal_t *d, const char *path, tcSeen *seen, int depth) {
	nativeVal_t keys;
	Native_SetNone(&keys);
	int n = NObj_CallMethod(d, "keys", NULL, 0, &keys) ? TcX_SeqLen(&keys) : 0;

	if (n <= 0) {
		PyErr_Clear();
		Native_Free(&keys);
		return;
	}
	tcKey *ks = Mem_Alloc((uint32_t)(sizeof(tcKey) * (size_t)n));
	nativeVal_t *kv = Mem_Alloc((uint32_t)(sizeof(nativeVal_t) * (size_t)n));
	for (int i = 0; i < n; i++) {
		tcBuf r = { 0 };
		Native_SetNone(&kv[i]);
		if (!TcX_SeqGet(&keys, i, &kv[i])) {
			PyErr_Clear();
		}
		TC_AddVal(&r, &kv[i], NR_REPR);
		ks[i].repr = r.p;
		ks[i].i = i;
	}
	qsort(ks, (size_t)n, sizeof(tcKey), TC_KeyCmp);
	for (int i = 0; i < n; i++) {
		char sub[0x400];
		nativeVal_t x;
		Native_SetNone(&x);
		if (i) {
			TC_Add(b, ", ", 2);
		}
		TC_Addf(b, "%s: ", ks[i].repr);
		snprintf(sub, sizeof(sub), "%s[%s]", path, ks[i].repr);
		if (NObj_GetItem(d, &kv[ks[i].i], &x)) {
			TC_MdVal(b, &x, sub, seen, depth + 1);
		} else {
			PyErr_Clear();
			TC_MdVal(b, NULL, sub, seen, depth + 1);
		}
		Native_Free(&x);
	}
	for (int i = 0; i < n; i++) {
		Mem_Free(ks[i].repr);
		Native_Free(&kv[i]);
	}
	Mem_Free(ks);
	Mem_Free(kv);
	Native_Free(&keys);
}

/* the text of attribute name of v ("?" when it has none) */
static void TC_MdAttr(tcBuf *b, const nativeVal_t *v, const char *name) {
	nativeVal_t a;

	if (NObj_GetAttr(v, name, &a)) {
		TC_AddVal(b, &a, NR_STR);
	} else {
		PyErr_Clear();
		TC_Add(b, "?", 1);
	}
	Native_Free(&a);
}

/* builtin <name>[ data(..) | self=<type>]: the self of a C function (a 4-tuple: the data of a script function) */
static void TC_MdBuiltin(tcBuf *b, const nativeVal_t *v) {
	nativeVal_t self;
	tcBuf s = { 0 };

	TC_Add(b, "builtin ", -1);
	TC_MdAttr(b, v, "__name__");
	if (!NObj_GetAttr(v, "__self__", &self)) {
		PyErr_Clear();
	}
	if (self.type == NV_TUPLE && self.n == 4) {
		const nativeVal_t *list = &self.items[2];
		int n = TcX_SeqOf(list) == 1 ? TcX_SeqLen(list) : -1;
		byte bad = n < 0;
		TC_Add(&s, " data(", -1);
		TC_AddVal(&s, &self.items[1], NR_STR);
		TC_Add(&s, ";", 1);
		for (int i = 0; i < n && !bad; i++) {
			nativeVal_t x;
			Native_SetNone(&x);
			if (i) {
				TC_Add(&s, ",", 1);
			}
			bad = !TcX_SeqGet(list, i, &x);
			if (bad) {
				TC_Add(&s, "?", 1);
			} else {
				TC_AddVal(&s, &x, NR_STR);
			}
			Native_Free(&x);
		}
		TC_Add(&s, ";", 1);
		TC_AddVal(&s, &self.items[3], NR_REPR);
		TC_Add(&s, ")", 1);
		if (bad) {
			PyErr_Clear();
			s.len = 0;
			TC_Add(&s, " self=?", -1);
		}
		TC_Add(b, s.p, s.len);
		Mem_Free(s.p);
	} else if (self.type != NV_NONE) {
		TC_Addf(b, " self=%s", Native_TypeName(&self));
	}
	Native_Free(&self);
}

static void TC_MdVal(tcBuf *b, const nativeVal_t *v, const char *path, tcSeen *seen, int depth) {
	char sub[0x400];
	int kind = v ? NObj_Kind(v) : -1;

	if (!v) {
		TC_Add(b, "?", 1);
		return;
	}
	switch (v->type) {
	case NV_INT:
		TC_Addf(b, "int %ld", v->i);
		return;
	case NV_LONG:
		TC_Add(b, "long ", -1);
		TC_AddVal(b, v, NR_REPR);
		return;
	case NV_FLOAT:
		TC_Addf(b, "float %.17g", v->f);
		return;
	case NV_STR:
		TC_Add(b, "str ", -1);
		TC_AddVal(b, v, NR_REPR);
		return;
	case NV_NONE:
		TC_Add(b, "None", -1);
		return;
	}
	if (kind == NO_FUNCTION) {
		if (strcmp(Native_TypeName(v), "function")) {
			TC_MdBuiltin(b, v);
		} else {
			TC_Add(b, "function ", -1);
			TC_MdAttr(b, v, "__name__");
		}
		return;
	}
	if (kind == NO_MODULE) {
		nativeVal_t name;
		byte ok = NObj_GetAttr(v, "__name__", &name) && name.type == NV_STR;
		PyErr_Clear();
		TC_Addf(b, "module %s", ok ? Native_Text(&name) : "(null)");
		Native_Free(&name);
		return;
	}
	byte tuple = v->type == NV_TUPLE || v->type == NV_VEC;
	if (tuple || v->type == NV_LIST || v->type == NV_DICT || kind == NO_LIST || kind == NO_DICT || kind == NO_CLASS
		|| kind == NO_INSTANCE) {
		const char *tname = Native_TypeName(v);
		if (v->type == NV_REF) {
			for (int i = 0; i < seen->n; i++) {
				if (seen->vals[i].ref == v->ref) {
					TC_Addf(b, "alias %s", seen->paths[i]);
					return;
				}
			}
			if (seen->n == seen->cap) {
				seen->cap = seen->cap ? seen->cap * 2 : 64;
				seen->vals = Mem_Realloc(seen->vals, (uint32_t)(sizeof(nativeVal_t) * (size_t)seen->cap));
				seen->paths = Mem_Realloc(seen->paths, (uint32_t)(sizeof(char *) * (size_t)seen->cap));
			}
			size_t len = strlen(path) + 1;
			Native_Copy(&seen->vals[seen->n], v);
			seen->paths[seen->n] = Mem_Alloc((uint32_t)len);
			memcpy(seen->paths[seen->n++], path, len);
		}
		if (depth > 6) {
			TC_Addf(b, "%s ...", tname);
			return;
		}
		if (tuple || v->type == NV_LIST || kind == NO_LIST) {
			int n = TcX_SeqLen(v);
			TC_Addf(b, "%s [", tname);
			for (int i = 0; i < n; i++) {
				nativeVal_t x;
				Native_SetNone(&x);
				if (i) {
					TC_Add(b, ", ", 2);
				}
				snprintf(sub, sizeof(sub), "%s[%d]", path, i);
				if (TcX_SeqGet(v, i, &x)) {
					TC_MdVal(b, &x, sub, seen, depth + 1);
				} else {
					PyErr_Clear();
					TC_MdVal(b, NULL, sub, seen, depth + 1);
				}
				Native_Free(&x);
			}
			TC_Add(b, "]", 1);
			return;
		}
		nativeVal_t d;
		Native_SetNone(&d);
		if (v->type == NV_DICT || kind == NO_DICT) {
			Native_Copy(&d, v);
			TC_Add(b, "dict", -1);
		} else if (kind == NO_CLASS) {
			nativeVal_t bases, names;
			Native_SetNone(&bases);
			Native_SetEmpty(&names, NV_LIST);
			if (NObj_GetAttr(v, "__bases__", &bases) && bases.type == NV_TUPLE) {
				for (int i = 0; i < bases.n; i++) {
					if (!NObj_GetAttr(&bases.items[i], "__name__", Native_Append(&names))) {
						PyErr_Clear();
					}
				}
			}
			PyErr_Clear();
			Native_Free(&bases);
			if (!NObj_GetAttr(v, "__dict__", &d)) {
				PyErr_Clear();
			}
			TC_Add(b, "class ", -1);
			TC_MdAttr(b, v, "__name__");
			TC_Add(b, " bases=", -1);
			TC_AddVal(b, &names, NR_REPR);
			Native_Free(&names);
		} else {
			nativeVal_t cls;
			Native_SetNone(&cls);
			if (!NObj_GetAttr(v, "__dict__", &d) || !NObj_GetAttr(v, "__class__", &cls)) {
				PyErr_Clear();
			}
			TC_Add(b, "instance of ", -1);
			TC_MdAttr(b, &cls, "__name__");
			Native_Free(&cls);
		}
		TC_Add(b, " {", 2);
		TC_MdDict(b, &d, path, seen, depth);
		TC_Add(b, "}", 1);
		Native_Free(&d);
		return;
	}
	/* anything else: its type and its repr up to the address */
	tcBuf r = { 0 };
	TC_AddVal(&r, v, NR_REPR);
	const char *at = strstr(r.p, " at ");
	if (at && at > r.p) {
		TC_Addf(b, "%s %.*s>", Native_TypeName(v), (int)(at - r.p), r.p);
	} else {
		TC_Addf(b, "%s %s", Native_TypeName(v), r.p);
	}
	Mem_Free(r.p);
}

/* one line of the dump: "key\t<type value>" (not __builtins__) */
static void TC_DumpVar(tcBuf *b, const char *module, const char *key, const nativeVal_t *v, tcSeen *seen) {
	char path[0x400];

	if (!strcmp(key, "__builtins__")) {
		return;
	}
	TC_Addf(b, "%s\t", key);
	snprintf(path, sizeof(path), "%s.%s", module, key);
	TC_MdVal(b, v, path, seen, 0);
	TC_Add(b, "\n", 1);
}

/* OBJLIB 9b: a value read from the dict of a module (NULL: none) as TC_DumpVar shows it */
static void TC_DumpPy(tcBuf *b, const char *module, const char *key, PyObject *o, tcSeen *seen) {
	nativeVal_t v;

	if (!o) {
		TC_DumpVar(b, module, key, NULL, seen);
		return;
	}
	NObj_FromPy(o, &v);
	TC_DumpVar(b, module, key, &v, seen);
	Native_Free(&v);
}

/*
 * dump [module ...]: without names every pack module (made from a .mod file, its __file__ starts with "PACK:"). The
 * variables of a pack module come from the registry of the module variables (sorted names, the value of each); a
 * module without a .mod file (dump Scrap) shows its dict
 */
static void TC_Dump(tcBuf *b, const char *arg) {
	size_t len = strlen(arg) + 1;
	char *words = Mem_Alloc((uint32_t)(len + 0x80));
	char *w = words;
	tcNames names = { 0 };
	tcSeen seen = { 0 };
	modVarRef_t ref;

	/* the words cut to 127 characters each */
	while (TC_Word(&arg, w, 0x80)) {
		TC_NamePush(&names, w);
		w += strlen(w) + 1;
	}
	if (!names.n) {
		for (int i = 0; i < g_ModVarModuleCount; i++) {
			const char *module = g_ModVarModules[i].module;
			if ((i && !strcmp(module, g_ModVarModules[i - 1].module)) || !ModVar_Named(module, &ref)) {
				continue;
			}
			TC_NamePush(&names, module);
		}
	}
	if (names.n) {
		qsort(names.names, (size_t)names.n, sizeof(*names.names), TC_CmpName);
	}
	for (int i = 0; i < names.n; i++) {
		const char *name = names.names[i];
		/* OBJLIB 9b: the module object and, for one without a .mod file, its dict */
		PyObject *m = Script_ModuleLoaded(name);
		if (!m || !PyModule_Check(m)) {
			TC_Addf(b, "== %s missing\n", name);
			continue;
		}
		TC_Addf(b, "== %s\n", name);
		if (ModVar_Of(m, &ref)) {
			const char **vars;
			int n = ModVar_Names(&ref, &vars);
			for (int j = 0; j < n; j++) {
				TC_DumpPy(b, name, vars[j], ModVar_Get(&ref, vars[j]), &seen);
			}
			Mem_Free(vars);
			continue;
		}
		PyObject *d = PyModule_GetDict(m), *keys = PyDict_Keys(d);
		if (!keys) {
			continue;
		}
		PyList_Sort(keys);
		for (int j = 0; j < PyList_Size(keys); j++) {
			PyObject *k = PyList_GetItem(keys, j);
			if (PyString_Check(k)) {
				TC_DumpPy(b, name, PyString_AsString(k), PyDict_GetItem(d, k), &seen);
			}
		}
		Py_DECREF(keys);
	}
	PyErr_Clear();
	for (int i = 0; i < seen.n; i++) {
		Native_Free(&seen.vals[i]);
		Mem_Free(seen.paths[i]);
	}
	Mem_Free(seen.vals);
	Mem_Free(seen.paths);
	Mem_Free(names.names);
	Mem_Free(words);
}

/* ---- engine and entity queries in plain C ---- */

/* get <var> ...: [('name', value), ...] as Scrap.Get gives the values */
static void TC_Get(tcBuf *b, const char *arg) {
	char name[0x100], val[0x1000];
	int n = 0;

	TC_Add(b, "[", 1);
	while (TC_Word(&arg, name, sizeof(name))) {
		TC_Add(b, n++ ? ", (" : "(", -1);
		TC_AddRepr(b, name);
		TC_Add(b, ", ", 2);
		TC_Add(b, Console_ReprVar(name, val, sizeof(val)) ? val : "'?'", -1);
		TC_Add(b, ")", 1);
	}
	TC_Add(b, "]\n", 2);
}

/* the value of an entity variable as the attribute of its handle gives it (PyEntity_GetAttr); 0 when it has none */
static byte TC_EntVar(tcBuf *b, cEntity *e, const char *var) {
	int i = 0;
	double d = 0.0, x = 0.0, y = 0.0, z = 0.0;
	char *s = NULL;

	if (!e) {
		return 0;
	}
	if (Entity_VarI(e, var, &i, 0)) {
		TC_Addf(b, "%d", i);
	} else if (Entity_VarF(e, var, &d, 0)) {
		TC_AddFloat(b, d);
	} else if (Entity_VarV(e, var, &x, &y, &z, 0)) {
		TC_Add(b, "(", 1);
		TC_AddFloat(b, x);
		TC_Add(b, ", ", 2);
		TC_AddFloat(b, y);
		TC_Add(b, ", ", 2);
		TC_AddFloat(b, z);
		TC_Add(b, ")", 1);
	} else if (Entity_VarS(e, var, &s, 0)) {
		if (s) {
			TC_AddRepr(b, s);
		} else {
			TC_Add(b, "None", -1);
		}
	} else {
		return 0;
	}
	return 1;
}

/* ent <entity> <var> ...: [('var', value), ...], '?' for a variable it does not have (or no such entity) */
static void TC_Ent(tcBuf *b, const char *arg) {
	char name[0x100], var[0x100];
	int n = 0;

	if (!TC_Word(&arg, name, sizeof(name))) {
		TC_Add(b, "error: ent <entity> <var> ...\n", -1);
		return;
	}
	cEntity *e = World_FindEntity(name);
	TC_Add(b, "[", 1);
	while (TC_Word(&arg, var, sizeof(var))) {
		TC_Add(b, n++ ? ", (" : "(", -1);
		TC_AddRepr(b, var);
		TC_Add(b, ", ", 2);
		if (!TC_EntVar(b, e, var)) {
			TC_Add(b, "'?'", 3);
		}
		TC_Add(b, ")", 1);
	}
	TC_Add(b, "]\n", 2);
}

static void TC_CollectEnt(cEntity *e, void *ctx) {
	TC_NamePush(ctx, e->name);
}

/* ents <list> [x y z radius] */
static void TC_Ents(tcBuf *b, const char *arg) {
	char list[0x40], num[0x40];
	double v[4] = { 0.0, 0.0, 0.0, 1e7 };
	tcNames l = { 0 };

	if (!TC_Word(&arg, list, sizeof(list))) {
		TC_Add(b, "error: ents <list> [x y z radius]\n", -1);
		return;
	}
	for (int i = 0; i < 4 && TC_Word(&arg, num, sizeof(num)); i++) {
		v[i] = atof(num);
	}
	if (!g_pWorld) {
		TC_Add(b, "error: no world\n", -1);
		return;
	}
	vec3_t c = { (float)v[0], (float)v[1], (float)v[2] };
	cWorld_SphereEach(g_pWorld, EntityLists_Get(list), &c, (float)v[3], 1, TC_CollectEnt, &l);
	qsort(l.names, (size_t)l.n, sizeof(*l.names), TC_CmpName);
	TC_Add(b, "[", 1);
	for (int i = 0; i < l.n; i++) {
		if (i) {
			TC_Add(b, ", ", 2);
		}
		TC_AddRepr(b, l.names[i] ? l.names[i] : "");
	}
	TC_Add(b, "]\n", 2);
	if (l.names) {
		Mem_Free(l.names);
	}
}

/* a value of a scorer item as SScorer.Get gives it */
static void TC_ItemVar(tcBuf *b, cScorerItem *item, const char *var) {
	char *s = NULL;
	double d = 0.0;
	int i = 0;

	switch (ScorerItem_Var(item, var, 0, &s, &d, &i)) {
	case 1:
		if (s) {
			TC_AddRepr(b, s);
		} else {
			TC_Add(b, "None", -1);
		}
		break;
	case 2:
		TC_AddFloat(b, d);
		break;
	case 3:
		TC_Addf(b, "%d", i);
		break;
	default:
		TC_Add(b, "None", -1);
		break;
	}
}

static const char *const tc_menuVars[] = { "Text", "X", "Y", "W", "H", "Visible", "Alpha", "Red", "Green", "Blue",
	"OnAccept", "Up", "Down", "Left", "Right", NULL };

/* menu [player] */
static void TC_Menu(tcBuf *b, const char *arg) {
	char num[0x20];
	int player = TC_Word(&arg, num, sizeof(num)) ? atoi(num) : 0;
	cScorer *sc = Scorer_Get(player);

	TC_Add(b, "(", 1);
	if (sc && sc->current) {
		TC_AddRepr(b, sc->current);
	} else {
		TC_Add(b, "None", -1);
	}
	TC_Add(b, ", [", 3);
	int n = 0;
	for (cScorerItem *it = sc ? sc->items : NULL; it; it = it->next) {
		if (it->delMe) {
			continue;
		}
		TC_Add(b, n++ ? ", [" : "[", -1);
		TC_AddRepr(b, it->name ? it->name : "");
		for (int k = 0; tc_menuVars[k]; k++) {
			TC_Add(b, ", ", 2);
			TC_ItemVar(b, it, tc_menuVars[k]);
		}
		TC_Add(b, "]", 1);
	}
	TC_Add(b, "])\n", 3);
}

/* click <item> [player] */
static void TC_Click(tcBuf *b, const char *arg) {
	char item[0x100], num[0x20], *s = NULL;
	double d;
	int i;

	if (!TC_Word(&arg, item, sizeof(item))) {
		TC_Add(b, "error: click <item> [player]\n", -1);
		return;
	}
	int player = TC_Word(&arg, num, sizeof(num)) ? atoi(num) : 0;
	cScorer *sc = Scorer_Get(player);
	cScorerItem *it = sc ? cScorer_FindItem(sc, item) : NULL;
	char func[0x200];
	func[0] = 0;
	if (it && ScorerItem_Var(it, "OnAccept", 0, &s, &d, &i) == 1 && s) {
		Str_Copy(func, s, sizeof(func));
	}
	if (Scorer_AcceptItem(player, item)) {
		TC_Addf(b, "called %s\n", func);
	} else {
		TC_Add(b, "the item has no OnAccept\n", -1);
	}
}

/* ---- console lines ---- */

static tcBuf *tc_capture;

static void TC_Capture(const char *text) {
	TC_Add(tc_capture, text, -1);
}

static void TC_Con(tcBuf *b, const char *arg) {
	int start = b->len;

	tc_capture = b;
	g_ConCapture = TC_Capture;
	Console_Execute(arg);
	g_ConCapture = NULL;
	tc_capture = NULL;
	if (b->len > start && b->p[b->len - 1] != '\n') {
		TC_Add(b, "\n", 1);
	}
}

/* ---- the commands ---- */

/* native ...: the native values of the script ports (TestNative.c) */
static void TC_Native(tcBuf *b, const char *arg) {
	nativeBuf_t out;

	Native_BufInit(&out);
	TestNative_Command(&out, arg);
	TC_Add(b, out.p, out.len);
	Native_BufFree(&out);
}

/* in <module> <statements>: the statements in the globals of the module (imported when needed), answered as a line */
static void TC_In(tcBuf *b, const char *arg) {
	char name[0x80];
	int n = 0;
	nativeVal_t m;

	TC_Blanks(&arg);
	while (TC_NameChar(*arg, 0) && n < (int)sizeof(name) - 1) {
		name[n++] = *arg++;
	}
	name[n] = 0;
	Native_SetNone(&m);
	if (!n || !NObj_Module(&m, name) || NObj_Kind(&m) != NO_MODULE) {
		if (!PyErr_Occurred()) {
			TC_Syntax(arg);
		}
		Native_Free(&m);
		TC_Add(b, "error: ", -1);
		TC_AddError(b);
		TC_Add(b, "\n", 1);
		return;
	}
	byte quiet = 0;
	TC_Blanks(&arg);
	if (!strncmp(arg, "do ", 3)) {
		quiet = 1;
		arg += 3;
	}
	/* OBJLIB 9b: the module's dict as the globals of the statements */
	TcX_Run(b, arg, PyModule_GetDict(Native_PyOf(&m)), quiet);
	Native_Free(&m);
}

/* the verb of a line ("word " or the word alone); else 0 */
static byte TC_Verb(const char *line, const char *verb, const char **arg) {
	size_t n = strlen(verb);

	if (strncmp(line, verb, n) || (line[n] && line[n] != ' ' && line[n] != '\t')) {
		return 0;
	}
	*arg = line + n + (line[n] ? 1 : 0);
	return 1;
}

static void TC_Line(tcBuf *b, const char *line) {
	const char *arg;

	if (TC_Verb(line, "con", &arg)) {
		TC_Con(b, arg);
		return;
	}
	if (TC_Verb(line, "get", &arg)) {
		TC_Get(b, arg);
		return;
	}
	if (TC_Verb(line, "ent", &arg)) {
		TC_Ent(b, arg);
		return;
	}
	if (TC_Verb(line, "ents", &arg)) {
		TC_Ents(b, arg);
		return;
	}
	if (TC_Verb(line, "menu", &arg)) {
		TC_Menu(b, arg);
		return;
	}
	if (TC_Verb(line, "click", &arg)) {
		TC_Click(b, arg);
		return;
	}
	if (TC_Verb(line, "native", &arg)) {
		TC_Native(b, arg);
		return;
	}
	if (!g_PythonReady) {
		TC_Add(b, "error: the script modules are not ready\n", -1);
		return;
	}
	if (TC_Verb(line, "check", &arg)) {
		TC_Check(b, arg);
	} else if (TC_Verb(line, "print", &arg)) {
		TC_Print(b, arg);
	} else if (TC_Verb(line, "dump", &arg)) {
		TC_Dump(b, arg);
	} else if (TC_Verb(line, "recorder", &arg)) {
		if (!TC_Recorder(arg)) {
			TC_Add(b, "error: ", -1);
			TC_AddError(b);
			TC_Add(b, "\n", 1);
		}
	} else if (TC_Verb(line, "in", &arg)) {
		TC_In(b, arg);
	} else {
		byte quiet = TC_Verb(line, "do", &arg);
		TcX_Run(b, quiet ? arg : line, NULL, quiet);
	}
	/* nothing is left pending for the game */
	PyErr_Clear();
}

/* runs the body of a command file (CR dropped) */
static void TC_Run(tcBuf *b, char *body) {
	for (char *line = body; line && *line;) {
		char *nl = strchr(line, '\n');
		if (nl) {
			*nl = 0;
		}
		if (line[0]) {
			TC_Line(b, line);
		}
		line = nl ? nl + 1 : NULL;
	}
}

/* ---- the files ---- */

/* the number on the first line of a file; -1 when there is none. pace (may be NULL, then credit too): the seconds of
 * "pace=<s>" after the number, else 0; credit: those of "credit=<s>" after the pace, -1 without it */
static long TC_FileSeq(const char *name, double *pace, double *credit) {
	char head[0x40];
	void *f = crt_fopen(name, "rb");

	if (pace) {
		*pace = 0.0;
		*credit = -1.0;
	}
	if (!f) {
		return -1;
	}
	size_t n = crt_fread(head, 1, sizeof(head) - 1, f);
	crt_fclose(f);
	head[n] = 0;
	char *end;
	long seq = strtol(head, &end, 10);
	if (end > head && pace && !strncmp(end, " pace=", 6)) {
		*pace = strtod(end + 6, &end);
		if (!strncmp(end, " credit=", 8)) {
			*credit = strtod(end + 8, &end);
		}
	}
	return end > head && (*end == '\n' || *end == '\r') ? seq : -1;
}

/* the whole file as text (CR dropped); NULL when it cannot be read */
static char *TC_ReadFile(const char *name) {
	void *f = crt_fopen(name, "rb");

	if (!f) {
		return NULL;
	}
	crt_fseek(f, 0, 2);
	long size = crt_ftell(f);
	crt_fseek(f, 0, 0);
	char *text = size >= 0 ? Mem_Alloc((uint32_t)size + 1) : NULL;
	size_t n = text ? crt_fread(text, 1, (size_t)size, f) : 0;
	crt_fclose(f);
	if (!text) {
		return NULL;
	}
	text[n] = 0;
	char *w = text;
	for (char *r = text; *r; r++) {
		if (*r != '\r') {
			*w++ = *r;
		}
	}
	*w = 0;
	return text;
}

/* the deterministic test mode: the heap addresses in the reprs of an answer (" at 55d0c8a41b2c>", " at 0x7f..>")
 * differ from run to run even without the random layout of the address space (the heap of a run depends on its
 * environment and on the threads of the drivers). Each address is replaced by the number of its first appearance
 * in the answers of this run, in the same width, so equal runs answer equal bytes and one object keeps one value */
static void TC_NumberAddresses(tcBuf *b) {
	static uint64_t *seen;
	static int count, cap;

	for (char *p = b->p ? strstr(b->p, " at ") : NULL; p; p = strstr(p, " at ")) {
		p += 4;
		char *digits = p + (p[0] == '0' && p[1] == 'x' ? 2 : 0), *end;
		uint64_t addr = strtoull(digits, &end, 16);
		int width = (int)(end - digits), i;

		if (width < 6 || width > 16 || *end != '>') {
			continue;
		}
		for (i = 0; i < count && seen[i] != addr; i++) {
		}
		if (i == count) {
			if (count == cap) {
				cap = cap ? cap * 2 : 0x100;
				seen = Mem_Realloc(seen, (uint32_t)cap * sizeof(*seen));
			}
			seen[count++] = addr;
		}
		char num[0x20];
		snprintf(num, sizeof(num), "%0*x", width, (unsigned)i + 1);
		memcpy(digits, num, (size_t)width);
		p = end;
	}
}

static void TC_WriteReply(long seq, const tcBuf *b) {
	char head[0x20];
	void *f = crt_fopen(TC_TMP, "wb");

	if (!f) {
		return;
	}
	int n = snprintf(head, sizeof(head), "%ld\n", seq);
	crt_fwrite(head, 1, (size_t)n, f);
	if (b->len) {
		crt_fwrite(b->p, 1, (size_t)b->len, f);
	}
	if (!b->len || b->p[b->len - 1] != '\n') {
		crt_fwrite("\n", 1, 1, f);
	}
	crt_fwrite(".end\n", 1, 5, f);
	crt_fclose(f);
	/* Windows' rename does not replace a file */
	crt_remove(TC_REPLY);
	crt_rename(TC_TMP, TC_REPLY);
}

void TestChannel_Enable(void) {
	tc_on = 1;
}

/* the deterministic test mode: the time since the last answer, looked at once per step of the mode's clock (several
 * looks in one step change nothing, so the hold and the poll of one pass agree) */
static void TC_DetTrack(void) {
	double gt = g_pWorld ? (double)Timer_GetTime(&g_pWorld->timer) : 0.0;
	uint32_t steps = TestDet_Frame() - tc_det.step;

	if (TestDet_Loads() != tc_det.loads) {
		/* a new level: its own time so far */
		tc_det.loads = TestDet_Loads();
		tc_det.loaded = 1;
		tc_det.world = gt;
		tc_det.stood = 0.0;
		tc_det.still = 0;
	} else if (gt != tc_det.gt) {
		tc_det.world += gt > tc_det.gt ? gt - tc_det.gt : 0.0;
		tc_det.still = 0;
	} else if (steps) {
		uint32_t before = tc_det.still;

		tc_det.still += steps;
		if (tc_det.still > TC_STAND_FREE) {
			tc_det.stood += (double)(tc_det.still - (before > TC_STAND_FREE ? before : TC_STAND_FREE))
				/ (double)TestDet_Fps();
		}
	}
	tc_det.gt = gt;
	tc_det.step = TestDet_Frame();
}

/* "pace=<s>" has passed: not before s seconds of game time after the last answer, as the polling script function of
 * the old harness answered (mcpgame.py looked every 0.1 s of game time); at once after a level load (the clock starts
 * over) or when the clock stood for 60 frames.
 * The deterministic test mode keeps these rules on its own count (TC_DetTrack: game time passed, steps instead of
 * frames looked at), so the hold of the clock and the poll decide alike. "credit=<s>" after the pace makes the
 * command file a wait (the !sleep of the regress runner, which the runner does itself in a live run): the pace is
 * game time plus the time the game clock stood (after its first 3 s); after a level load that ended since the last
 * answer it is the time of the new level, and the load counts for the credit */
static int TC_PaceDone(double pace, double credit) {
	if (TestDet_On()) {
		TC_DetTrack();
		if (credit >= 0.0) {
			return tc_det.world + tc_det.stood + (tc_det.loaded ? credit : 0.0) + TC_PACE_SLACK >= pace;
		}
		if (pace <= 0.0) {
			return 1;
		}
		/* a live game gets no command file in the frame of its answer */
		if (TestDet_Frame() == tc_det.answer) {
			return 0;
		}
		if (tc_det.loaded || tc_det.still >= TC_STILL_STEPS) {
			return 1;
		}
		if (tc_det.world + TC_PACE_SLACK < pace) {
			return 0;
		}
		/* and it sees the file one frame after the pace is met, more often than not (6.4 to 7 frames a line
		 * measured for the pace 0.1 at 60 frames a second, 0.1035 s of game time a line at the time speed 0.175) */
		if (!tc_det.met) {
			tc_det.met = TestDet_Frame() + 1;
		}
		return TestDet_Frame() >= tc_det.met;
	}
	double gt = g_pWorld ? (double)Timer_GetTime(&g_pWorld->timer) : 0.0;
	if (gt != tc_paceLast) {
		tc_paceStill = 0;
	}
	tc_paceLast = gt;
	return !(pace > 0.0 && gt >= tc_answerTime && gt < tc_answerTime + pace && ++tc_paceStill < 60);
}

/* a new command file runs (paced) and is answered */
static void TC_Poll(void) {
	double pace, credit;
	long seq = TC_FileSeq(TC_CMD, &pace, &credit);
	if (seq < 0 || seq == tc_seq || !TC_PaceDone(pace, credit)) {
		return;
	}
	char *text = TC_ReadFile(TC_CMD);
	char *nl = text ? strchr(text, '\n') : NULL;
	if (!nl || strtol(text, NULL, 10) != seq) {
		Mem_Free(text);
		return;
	}
	tcBuf b = { 0 };
	tc_seq = seq;
	if (g_TestDetTrace) {
		TestDet_Trace("command file%s: %.60s", tc_hold ? " (clock held)" : "", nl + 1);
	}
	TC_Run(&b, nl + 1);
	if (TestDet_On()) {
		TC_NumberAddresses(&b);
	}
	TC_WriteReply(seq, &b);
	tc_answerTime = g_pWorld ? (double)Timer_GetTime(&g_pWorld->timer) : 0.0;
	if (TestDet_On()) {
		/* the count of TC_DetTrack starts over */
		TC_DetTrack();
		tc_det.world = tc_det.stood = 0.0;
		tc_det.loaded = 0;
		tc_det.answer = TestDet_Frame();
		tc_det.met = 0;
	}
	Mem_Free(b.p);
	Mem_Free(text);
}

/* the start, once: the environment */
static void TC_Start(void) {
	if (tc_started) {
		return;
	}
	const char *e = getenv("SCRAP_TEST_CHANNEL");
	tc_started = 1;
	tc_on |= e && *e && strcmp(e, "0");
	if (!tc_on) {
		return;
	}
	tc_step = e && !strcmp(e, "step") && TestDet_On();
	/* the answer of a run before this start (the same game folder) is not run again */
	tc_seq = TC_FileSeq(TC_REPLY, NULL, NULL);
	Con_Printf("Test channel on: %s\n", TC_CMD);
}

int TestChannel_Hold(void) {
	double pace, credit;

	TC_Start();
	if (!tc_step) {
		return 0;
	}
	long seq = TC_FileSeq(TC_CMD, &pace, &credit);
	tc_hold = seq < 0 || seq == tc_seq || TC_PaceDone(pace, credit);
	return tc_hold;
}

void TestChannel_Frame(double now) {
	static double last;

	/* the clock of the deterministic test mode is no real time: it looks in every frame */
	if (tc_started && (!tc_on || (!TestDet_On() && now >= last && now - last < 0.005))) {
		return;
	}
	last = now;
	TC_Start();
	/* the world update of the frame before looked already (the clock runs): it looks again in this frame. While the
	 * clock of the deterministic test mode stands there is no world update: a command file whose pace has passed runs
	 * here, between two frames */
	if (!tc_on || (tc_worldRan && !tc_hold)) {
		tc_worldRan = 0;
		return;
	}
	TC_Poll();
}

void TestChannel_WorldFrame(void) {
	if (!tc_on) {
		return;
	}
	tc_worldRan = 1;
	TC_Poll();
}
