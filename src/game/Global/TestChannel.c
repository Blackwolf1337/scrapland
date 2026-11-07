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
 *                            "exception <type>: <value>" (the check() of tools/test/golden/sdlib.py)
 *   print <expr>             what the print statement writes for the value (tp_print of the object, else str)
 *   con <line>               a console line (ConsoleCmd.c): what the console shows
 *   get <var> ...            engine variables: [('name', value), ...]
 *   ent <entity> <var> ...   entity variables: [('var', value), ...], '?' for a variable the entity does not have
 *   ents <list> [x y z r]    the sorted names of the entities of an entity list (Scrap.SphereCall lists: 'r', 'i',
 *                            'v', ...) in the sphere, by default the whole map
 *   dump [module ...]        the module dicts (all pack modules without names): "== module", then "key\t<type value>"
 *                            per key in the form of the module dump of the stage checks (scratchpad nopy0/mdump.py)
 *   menu [player]            the scorer items: (cursor, [[name, Text, X, Y, W, H, Visible, Alpha, Red, Green, Blue,
 *                            OnAccept, Up, Down, Left, Right], ...]) as SScorer.Get gives each value
 *   click <item> [player]    runs the OnAccept of the item: "called <func>" or "the item has no OnAccept"
 *   recorder <target> <list> [<literal> | raise <exception> <text>]   puts a recorder function at target (a module
 *                            variable, e.g. __main__.f or CharAct.onNPCDeath): each call appends its argument tuple
 *                            to the list __main__.<list> and returns the literal (None), or raises
 *   py <code>                (STAGE 7) Python: an expression gives its repr, statements the repr of the global
 *                            "result" or ok; "py" alone on the first line takes the rest of the file as the code
 * An error answers "error: <type>: <value>". The channel statements and expressions (TestExpr.c) are a small part of
 * Python's syntax read and run in C (operators, comparisons, lambdas, assignments, no Python code).
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
#include "scrap/pyfunction.h"
#include "scrap/scorer.h"
#include "scrap/script.h"
#include "scrap/timer.h"
#include "scrap/world.h"
#include "testchannel.h"

#include <eval.h>

/* STAGE 7 of the removal of the interpreter: set TC_PYTHON to 0 (the "py" command goes with the interpreter) */
#define TC_PYTHON 1

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

/* str() or repr() of o; a failing one gives "?" and clears the error */
void TC_AddObj(tcBuf *b, PyObject *o, byte repr) {
	PyObject *s = o ? (repr ? PyObject_Repr(o) : PyObject_Str(o)) : NULL;

	if (s && PyString_Check(s)) {
		TC_Add(b, PyString_AsString(s), PyString_Size(s));
	} else {
		PyErr_Clear();
		TC_Add(b, "?", 1);
	}
	Py_XDECREF(s);
}

/* the pending Python error as "<type>: <value>" (normalized, as sys.exc_type and sys.exc_value in an except clause
 * show it), then cleared */
void TC_AddError(tcBuf *b) {
	PyObject *t, *v, *tb;

	PyErr_Fetch(&t, &v, &tb);
	PyErr_NormalizeException(&t, &v, &tb);
	TC_AddObj(b, t ? t : Py_None, 0);
	TC_Add(b, ": ", 2);
	TC_AddObj(b, v ? v : Py_None, 0);
	Py_XDECREF(t);
	Py_XDECREF(v);
	Py_XDECREF(tb);
	PyErr_Clear();
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

/* ---- golden: the value as check() of sdlib.py writes it ---- */

static void TC_Fmt(tcBuf *b, PyObject *v) {
	if (PyFloat_Check(v)) {
		char tmp[0x40];
		snprintf(tmp, sizeof(tmp), "%.17g", PyFloat_AsDouble(v));
		TC_Add(b, strstr(tmp, "nan") ? "nan" : tmp, -1);
	} else if (PyTuple_Check(v)) {
		TC_Add(b, "(", 1);
		for (int i = 0; i < PyTuple_Size(v); i++) {
			if (i) {
				TC_Add(b, ", ", 2);
			}
			TC_Fmt(b, PyTuple_GetItem(v, i));
		}
		TC_Add(b, ")", 1);
	} else {
		TC_AddObj(b, v, 1);
	}
}

static void TC_Check(tcBuf *b, const char *arg) {
	const char *tab = strchr(arg, '\t');

	if (!tab) {
		TC_Add(b, "error: check needs <name>\\t<expr>\n", -1);
		return;
	}
	TC_Add(b, arg, (int)(tab - arg));
	TC_Add(b, " = ", 3);
	PyObject *v = TcX_Statement(tab + 1);
	if (v) {
		TC_Fmt(b, v);
		Py_DECREF(v);
	} else {
		TC_Add(b, "exception ", -1);
		TC_AddError(b);
	}
	TC_Add(b, "\n", 1);
}

/* print <expr>: the print statement (tp_print of the object, else str) through a temporary file */
static void TC_Print(tcBuf *b, const char *arg) {
	PyObject *v = TcX_Eval(arg);

	if (!v) {
		TC_Add(b, "error: ", -1);
		TC_AddError(b);
		TC_Add(b, "\n", 1);
		return;
	}
	FILE *f = tmpfile();
	if (f && PyObject_Print(v, f, Py_PRINT_RAW) == 0) {
		char tmp[0x400];
		size_t n;
		rewind(f);
		while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) {
			TC_Add(b, tmp, (int)n);
		}
		TC_Add(b, "\n", 1);
	} else {
		TC_Add(b, "error: ", -1);
		TC_AddError(b);
		TC_Add(b, "\n", 1);
	}
	if (f) {
		fclose(f);
	}
	Py_DECREF(v);
}

/* ---- recorders ---- */

/* self: (list, value, exception, text); the arguments appended to the list, then the value or the exception */
static PyObject *TC_RecorderCall(PyObject *self, PyObject *args) {
	if (PyList_Append(PyTuple_GetItem(self, 0), args) != 0) {
		return NULL;
	}
	PyObject *exc = PyTuple_GetItem(self, 2);
	if (exc != Py_None) {
		PyErr_SetObject(exc, PyTuple_GetItem(self, 3));
		return NULL;
	}
	PyObject *v = PyTuple_GetItem(self, 1);
	Py_INCREF(v);
	return v;
}

static PyMethodDef tc_recorderDef = { "recorder", TC_RecorderCall, METH_VARARGS, NULL };

/* recorder <target> <list> [<literal> | raise <exception> <text>] */
static byte TC_Recorder(const char *arg) {
	char target[0x200], list[0x80], line[0x300];
	const char *p = arg;
	int n = 0;
	PyObject *value = NULL, *exc = NULL, *text = NULL;

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
		exc = TcX_Name(name);
		text = exc ? PyString_FromString(p) : NULL;
	} else {
		value = *p ? TcX_Eval(p) : (Py_INCREF(Py_None), Py_None);
	}
	PyObject *calls = PyList_New(0);
	PyObject *self = (value || text) && calls ? Py_BuildValue("(OOOO)", calls, value ? value : Py_None,
		exc ? exc : Py_None, text ? text : Py_None) : NULL;
	PyObject *f = self ? PyCFunction_New(&tc_recorderDef, self) : NULL;
	byte ok = 0;
	if (f && (strcmp(list, "-") == 0 || PyDict_SetItemString(TcX_Main(), list, calls) == 0)) {
		/* the function goes to the target as an assignment would put it there */
		PyObject *main = TcX_Main();
		snprintf(line, sizeof(line), "%s = __tc_recorder", target);
		if (PyDict_SetItemString(main, "__tc_recorder", f) == 0) {
			PyObject *r = TcX_Statement(line);
			ok = r != NULL;
			Py_XDECREF(r);
			PyDict_DelItemString(main, "__tc_recorder");
		}
	}
	Py_XDECREF(f);
	Py_XDECREF(self);
	Py_XDECREF(calls);
	Py_XDECREF(value);
	Py_XDECREF(exc);
	Py_XDECREF(text);
	return ok;
}

/* ---- the module dump (the form of scratchpad nopy0/mdump.py) ---- */

static void TC_MdVal(tcBuf *b, PyObject *v, const char *path, PyObject *seen, int depth);

static int TC_StrCmp(const void *a, const void *b) {
	return strcmp(PyString_AsString(*(PyObject *const *)a), PyString_AsString(*(PyObject *const *)b));
}

/* "key: value" pairs of a dict, sorted by the repr of the key */
static void TC_MdDict(tcBuf *b, PyObject *d, const char *path, PyObject *seen, int depth) {
	PyObject *keys = PyDict_Keys(d);
	int n = keys ? PyList_Size(keys) : 0;
	PyObject **reprs = n ? Mem_Alloc((uint32_t)(sizeof(PyObject *) * 2 * (size_t)n)) : NULL;

	for (int i = 0; i < n; i++) {
		reprs[2 * i] = PyObject_Repr(PyList_GetItem(keys, i));
		reprs[2 * i + 1] = PyList_GetItem(keys, i);
		if (!reprs[2 * i]) {
			PyErr_Clear();
			reprs[2 * i] = PyString_FromString("?");
		}
	}
	qsort(reprs, (size_t)n, sizeof(PyObject *) * 2, TC_StrCmp);
	for (int i = 0; i < n; i++) {
		char sub[0x400];
		const char *rk = PyString_AsString(reprs[2 * i]);
		if (i) {
			TC_Add(b, ", ", 2);
		}
		TC_Addf(b, "%s: ", rk);
		snprintf(sub, sizeof(sub), "%s[%s]", path, rk);
		TC_MdVal(b, PyDict_GetItem(d, reprs[2 * i + 1]), sub, seen, depth + 1);
		Py_DECREF(reprs[2 * i]);
	}
	if (reprs) {
		Mem_Free(reprs);
	}
	Py_XDECREF(keys);
}

/* builtin <name>[ data(..) | self=<type>]: the self of a C function (a 4-tuple: the data of a script function) */
static void TC_MdBuiltin(tcBuf *b, PyObject *v) {
	PyObject *o = PyCFunction_GetSelf(v);
	tcBuf s = { 0 };

	TC_Add(b, "builtin ", -1);
	TC_Add(b, ((PyCFunctionObject *)v)->m_ml->ml_name, -1);
	if (o && PyTuple_Check(o) && PyTuple_Size(o) == 4) {
		PyObject *list = PyTuple_GetItem(o, 2);
		int n = PySequence_Length(list);
		byte bad = n < 0;
		TC_Add(&s, " data(", -1);
		TC_AddObj(&s, PyTuple_GetItem(o, 1), 0);
		TC_Add(&s, ";", 1);
		for (int i = 0; i < n && !bad; i++) {
			PyObject *x = PySequence_GetItem(list, i);
			if (i) {
				TC_Add(&s, ",", 1);
			}
			bad = !x;
			TC_AddObj(&s, x, 0);
			Py_XDECREF(x);
		}
		TC_Add(&s, ";", 1);
		TC_AddObj(&s, PyTuple_GetItem(o, 3), 1);
		TC_Add(&s, ")", 1);
		if (bad) {
			PyErr_Clear();
			s.len = 0;
			TC_Add(&s, " self=?", -1);
		}
		TC_Add(b, s.p, s.len);
		Mem_Free(s.p);
	} else if (o && o != Py_None) {
		TC_Addf(b, " self=%s", o->ob_type->tp_name);
	}
}

static void TC_MdVal(tcBuf *b, PyObject *v, const char *path, PyObject *seen, int depth) {
	char sub[0x400];

	if (!v) {
		TC_Add(b, "?", 1);
	} else if (PyInt_Check(v)) {
		TC_Addf(b, "int %ld", PyInt_AsLong(v));
	} else if (PyLong_Check(v)) {
		TC_Add(b, "long ", -1);
		TC_AddObj(b, v, 1);
	} else if (PyFloat_Check(v)) {
		TC_Addf(b, "float %.17g", PyFloat_AsDouble(v));
	} else if (PyString_Check(v)) {
		TC_Add(b, "str ", -1);
		TC_AddObj(b, v, 1);
	} else if (v == Py_None) {
		TC_Add(b, "None", -1);
	} else if (PyCFunction_Check(v)) {
		TC_MdBuiltin(b, v);
	} else if (PyFunction_Check(v)) {
		TC_Add(b, "function ", -1);
		TC_AddObj(b, ((PyFunctionObject *)v)->func_name, 0);
	} else if (PyModule_Check(v)) {
		TC_Addf(b, "module %s", PyModule_GetName(v));
	} else if (PyList_Check(v) || PyDict_Check(v) || PyClass_Check(v) || PyInstance_Check(v) || PyTuple_Check(v)) {
		const char *tname = v->ob_type->tp_name;
		if (!PyTuple_Check(v)) {
			PyObject *id = PyLong_FromVoidPtr(v);
			PyObject *was = id ? PyDict_GetItem(seen, id) : NULL;
			if (was) {
				TC_Addf(b, "alias %s", PyString_AsString(was));
				Py_DECREF(id);
				return;
			}
			PyObject *ps = PyString_FromString(path);
			if (id && ps) {
				PyDict_SetItem(seen, id, ps);
			}
			Py_XDECREF(ps);
			Py_XDECREF(id);
		}
		if (depth > 6) {
			TC_Addf(b, "%s ...", tname);
			return;
		}
		if (PyTuple_Check(v) || PyList_Check(v)) {
			int n = PySequence_Length(v);
			TC_Addf(b, "%s [", tname);
			for (int i = 0; i < n; i++) {
				PyObject *x = PySequence_GetItem(v, i);
				if (i) {
					TC_Add(b, ", ", 2);
				}
				snprintf(sub, sizeof(sub), "%s[%d]", path, i);
				TC_MdVal(b, x, sub, seen, depth + 1);
				Py_XDECREF(x);
			}
			TC_Add(b, "]", 1);
			return;
		}
		PyObject *d;
		if (PyDict_Check(v)) {
			d = v;
			TC_Add(b, "dict", -1);
		} else if (PyClass_Check(v)) {
			PyClassObject *c = (PyClassObject *)v;
			PyObject *names = PyList_New(0);
			for (int i = 0; names && i < PyTuple_Size(c->cl_bases); i++) {
				PyList_Append(names, ((PyClassObject *)PyTuple_GetItem(c->cl_bases, i))->cl_name);
			}
			d = c->cl_dict;
			TC_Add(b, "class ", -1);
			TC_AddObj(b, c->cl_name, 0);
			TC_Add(b, " bases=", -1);
			TC_AddObj(b, names, 1);
			Py_XDECREF(names);
		} else {
			PyInstanceObject *in = (PyInstanceObject *)v;
			d = in->in_dict;
			TC_Add(b, "instance of ", -1);
			TC_AddObj(b, in->in_class->cl_name, 0);
		}
		TC_Add(b, " {", 2);
		TC_MdDict(b, d, path, seen, depth);
		TC_Add(b, "}", 1);
	} else {
		PyObject *r = PyObject_Repr(v);
		const char *s = r && PyString_Check(r) ? PyString_AsString(r) : "?";
		const char *at = strstr(s, " at ");
		PyErr_Clear();
		if (at && at > s) {
			TC_Addf(b, "%s %.*s>", v->ob_type->tp_name, (int)(at - s), s);
		} else {
			TC_Addf(b, "%s %s", v->ob_type->tp_name, s);
		}
		Py_XDECREF(r);
	}
}

/* dump [module ...]: without names every module whose __file__ starts with "PACK:" */
static void TC_Dump(tcBuf *b, const char *arg) {
	PyObject *names = PyList_New(0), *modules = PyImport_GetModuleDict(), *seen = PyDict_New();
	const char *p = arg;

	TC_Blanks(&p);
	while (*p && names) {
		char name[0x80];
		int n = 0;
		for (; *p && *p != ' ' && *p != '\t'; p++) {
			if (n < (int)sizeof(name) - 1) {
				name[n++] = *p;
			}
		}
		name[n] = 0;
		TC_Blanks(&p);
		PyObject *s = PyString_FromString(name);
		PyList_Append(names, s);
		Py_XDECREF(s);
	}
	if (names && !PyList_Size(names)) {
		int pos = 0;
		PyObject *k, *m;
		while (PyDict_Next(modules, &pos, &k, &m)) {
			PyObject *f = PyModule_Check(m) ? PyDict_GetItemString(PyModule_GetDict(m), "__file__") : NULL;
			if (f && PyString_Check(f) && !strncmp(PyString_AsString(f), "PACK:", 5)) {
				PyList_Append(names, k);
			}
		}
	}
	if (names && seen) {
		PyList_Sort(names);
		for (int i = 0; i < PyList_Size(names); i++) {
			const char *name = PyString_AsString(PyList_GetItem(names, i));
			PyObject *m = PyDict_GetItemString(modules, (char *)name);
			if (!m || !PyModule_Check(m)) {
				TC_Addf(b, "== %s missing\n", name);
				continue;
			}
			TC_Addf(b, "== %s\n", name);
			PyObject *d = PyModule_GetDict(m), *keys = PyDict_Keys(d);
			if (!keys) {
				continue;
			}
			PyList_Sort(keys);
			for (int j = 0; j < PyList_Size(keys); j++) {
				PyObject *k = PyList_GetItem(keys, j);
				char path[0x400];
				if (!PyString_Check(k) || !strcmp(PyString_AsString(k), "__builtins__")) {
					continue;
				}
				TC_Addf(b, "%s\t", PyString_AsString(k));
				snprintf(path, sizeof(path), "%s.%s", name, PyString_AsString(k));
				TC_MdVal(b, PyDict_GetItem(d, k), path, seen, 0);
				TC_Add(b, "\n", 1);
			}
			Py_DECREF(keys);
		}
	}
	PyErr_Clear();
	Py_XDECREF(names);
	Py_XDECREF(seen);
}

/* ---- engine and entity queries in plain C ---- */

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

typedef struct tcNames {
	const char **names;
	int n, cap;
} tcNames;

static void TC_CollectEnt(cEntity *e, void *ctx) {
	tcNames *l = ctx;

	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 64;
		l->names = Mem_Realloc(l->names, (uint32_t)(sizeof(*l->names) * (size_t)l->cap));
	}
	l->names[l->n++] = e->name;
}

static int TC_CmpName(const void *a, const void *b) {
	return strcmp(*(const char *const *)a, *(const char *const *)b);
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

/* ---- Python (STAGE 7) ---- */

#if TC_PYTHON
/* the code as the game side of the MCP server ran it (tools/mcp/mcpgame.py): an expression gives its repr,
 * statements the repr of the global "result" (or ok), in the globals of __main__ */
static void TC_Python(tcBuf *b, const char *code) {
	PyObject *g = TcX_Main();
	PyObject *v = NULL;

	for (const char *const *m = (const char *const[]){ "string", "sys", NULL }; *m; m++) {
		if (!PyDict_GetItemString(g, (char *)*m)) {
			PyObject *mod = PyImport_ImportModule((char *)*m);
			if (mod) {
				PyDict_SetItemString(g, (char *)*m, mod);
			}
			Py_XDECREF(mod);
			PyErr_Clear();
		}
	}
	PyObject *co = Py_CompileString((char *)code, "mcp", Py_eval_input);
	if (co) {
		v = PyEval_EvalCode((PyCodeObject *)co, g, g);
		Py_DECREF(co);
	} else if (PyErr_ExceptionMatches(PyExc_SyntaxError)) {
		PyErr_Clear();
		if (PyDict_GetItemString(g, "result")) {
			PyDict_DelItemString(g, "result");
		}
		co = Py_CompileString((char *)code, "mcp", Py_file_input);
		PyObject *r = co ? PyEval_EvalCode((PyCodeObject *)co, g, g) : NULL;
		Py_XDECREF(co);
		if (r) {
			Py_DECREF(r);
			v = PyDict_GetItemString(g, "result");
			if (v) {
				Py_INCREF(v);
			} else {
				TC_Add(b, "ok\n", 3);
				return;
			}
		}
	}
	if (v) {
		TC_AddObj(b, v, 1);
		Py_DECREF(v);
	} else {
		TC_Add(b, "error: ", -1);
		TC_AddError(b);
	}
	TC_Add(b, "\n", 1);
}
#endif

/* ---- the commands ---- */

/* in <module> <statements>: the statements in the globals of the module (imported when needed), answered as a line */
static void TC_In(tcBuf *b, const char *arg) {
	char name[0x80];
	int n = 0;

	TC_Blanks(&arg);
	while (TC_NameChar(*arg, 0) && n < (int)sizeof(name) - 1) {
		name[n++] = *arg++;
	}
	name[n] = 0;
	PyObject *m = n ? PyImport_ImportModule(name) : NULL;
	if (!m || !PyModule_Check(m)) {
		if (!PyErr_Occurred()) {
			TC_Syntax(arg);
		}
		Py_XDECREF(m);
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
	TcX_Run(b, arg, PyModule_GetDict(m), quiet);
	Py_DECREF(m);
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
#if TC_PYTHON
	} else if (TC_Verb(line, "py", &arg)) {
		TC_Python(b, arg);
#endif
	} else if (TC_Verb(line, "in", &arg)) {
		TC_In(b, arg);
	} else {
		byte quiet = TC_Verb(line, "do", &arg);
		TcX_Run(b, quiet ? arg : line, TcX_Main(), quiet);
	}
	/* nothing is left pending for the game */
	PyErr_Clear();
}

/* runs the body of a command file (CR dropped) */
static void TC_Run(tcBuf *b, char *body) {
#if TC_PYTHON
	if (!strncmp(body, "py\n", 3)) {
		if (g_PythonReady) {
			TC_Python(b, body + 3);
			PyErr_Clear();
		} else {
			TC_Add(b, "error: the script modules are not ready\n", -1);
		}
		return;
	}
#endif
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

/* the number on the first line of a file; -1 when there is none */
/* the number on the first line of a file; -1 when there is none. pace (may be NULL): the seconds of "pace=<s>" after
 * the number, else 0 */
static long TC_FileSeq(const char *name, double *pace) {
	char head[0x40];
	void *f = crt_fopen(name, "rb");

	if (pace) {
		*pace = 0.0;
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

/* a new command file runs (paced) and is answered */
static void TC_Poll(void) {
	double pace;
	long seq = TC_FileSeq(TC_CMD, &pace);
	if (seq < 0 || seq == tc_seq) {
		return;
	}
	/* "pace=<s>": not before s seconds of game time after the last answer, as the polling script function of the old
	 * harness answered (mcpgame.py looked every 0.1 s of game time); at once after a level load (the clock starts
	 * over) or when the clock stood for 60 frames */
	double gt = g_pWorld ? (double)Timer_GetTime(&g_pWorld->timer) : 0.0;
	if (gt != tc_paceLast) {
		tc_paceStill = 0;
	}
	tc_paceLast = gt;
	if (pace > 0.0 && gt >= tc_answerTime && gt < tc_answerTime + pace && ++tc_paceStill < 60) {
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
	TC_Run(&b, nl + 1);
	TC_WriteReply(seq, &b);
	tc_answerTime = g_pWorld ? (double)Timer_GetTime(&g_pWorld->timer) : 0.0;
	Mem_Free(b.p);
	Mem_Free(text);
}

void TestChannel_Frame(double now) {
	static double last;

	if (tc_started && (!tc_on || (now >= last && now - last < 0.005))) {
		return;
	}
	last = now;
	if (!tc_started) {
		const char *e = getenv("SCRAP_TEST_CHANNEL");
		tc_started = 1;
		tc_on |= e && *e && strcmp(e, "0");
		if (!tc_on) {
			return;
		}
		/* the answer of a run before this start (the same game folder) is not run again */
		tc_seq = TC_FileSeq(TC_REPLY, NULL);
		Con_Printf("Test channel on: %s\n", TC_CMD);
	}
	/* the world update of the frame before looked already (the clock runs): it looks again in this frame */
	if (!tc_on || tc_worldRan) {
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
