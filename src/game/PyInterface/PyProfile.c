/*
 * ScrapC only, a debug aid: the Python profile. SCRAP_PY_PROFILE=<file> records every Python code object that runs
 * (co_filename, co_name, co_firstlineno, the module's __name__) with its call count and its time, to measure what of
 * the pack scripts still runs as bytecode next to the C ports. Without the variable nothing is set and nothing
 * changes.
 *
 * It is the profile function of the 1.5.2 interpreter (tstate->sys_profilefunc, what sys.setprofile sets), here a
 * C function: eval_code2 calls it with (frame, "call") when a code object starts and (frame, "return") when it
 * returns (ceval.c, call_trace); a frame left through an exception gets no "return" and has no time. The table is
 * C memory and lives over the interpreter restarts of the level loads. It is written to the file when an
 * interpreter ends (Python_Close) and by Scrap.DumpPyProfile([file]), which exists only while the profile is on.
 *
 * The kinds: "module" is a code object named "?" (the module code at its load, or an exec/eval string), "class" a
 * class body, "function" a code object with CO_NEWLOCALS (functions, methods, lambdas). "shadowed" counts the calls
 * of a function while the module global of its name is a C function: a C or data version exists, yet the Python one
 * ran (a reference kept from before the loader replaced it).
 */
#include "scrap/global.h"
#include "scrap/platform.h"
#include "scrap/python.h"

#include <frameobject.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PYPROF_ENTRIES	8192		/* a power of 2 */
#define PYPROF_STACK	512

typedef struct {
	uint32_t hash;
	int used;
	int kind;			/* 0 module, 1 class, 2 function */
	int line;
	int codeSize;
	uint32_t calls;
	uint32_t shadowed;
	int64_t incl;			/* Plat_Counter ticks (100 ns) */
	int64_t self;
	char file[128];
	char name[64];
	char module[64];
	char names[96];			/* code of an exec/eval string ("<string>"): its co_names, to tell them apart */
} pyProfEntry_t;

typedef struct {
	PyFrameObject *frame;
	pyProfEntry_t *entry;
	int64_t start;
	int64_t children;
} pyProfCall_t;

static const char *pyprof_path;
static pyProfEntry_t *pyprof_table;
static pyProfCall_t pyprof_stack[PYPROF_STACK];
static int pyprof_depth;
static int pyprof_interpreters;
static int pyprof_dropped;

static const char *const pyprof_kinds[] = { "module", "class", "function" };

static uint32_t PyProf_Hash(uint32_t h, const char *s) {
	for (; *s; s++) {
		h = (h ^ (byte)*s) * 16777619u;
	}
	return h;
}

static const char *PyProf_Str(PyObject *o) {
	return o && PyString_Check(o) ? PyString_AsString(o) : "";
}

static pyProfEntry_t *PyProf_Entry(PyFrameObject *f) {
	PyCodeObject *co = f->f_code;
	const char *file = PyProf_Str(co->co_filename);
	const char *name = PyProf_Str(co->co_name);
	const char *module = PyProf_Str(f->f_globals ? PyDict_GetItemString(f->f_globals, "__name__") : NULL);
	int kind = (co->co_flags & CO_NEWLOCALS) ? 2 : !strcmp(name, "?") ? 0 : 1;
	char names[96] = "";
	if (*file == '<' && co->co_names && PyTuple_Check(co->co_names)) {
		for (int i = 0; i < PyTuple_Size(co->co_names); i++) {
			size_t len = strlen(names);
			PyObject *s = PyTuple_GetItem(co->co_names, i);
			snprintf(names + len, sizeof(names) - len, "%s%s", i ? "," : "", PyProf_Str(s));
		}
	}
	uint32_t h = PyProf_Hash(PyProf_Hash(PyProf_Hash(PyProf_Hash(2166136261u, file), name), module), names) ^
		(uint32_t)co->co_firstlineno;

	for (uint32_t i = 0; i < PYPROF_ENTRIES; i++) {
		pyProfEntry_t *e = &pyprof_table[(h + i) & (PYPROF_ENTRIES - 1)];
		if (!e->used) {
			e->used = 1;
			e->hash = h;
			e->kind = kind;
			e->line = co->co_firstlineno;
			e->codeSize = co->co_code ? PyString_Size((PyObject *)co->co_code) : 0;
			Str_Copy(e->file, file, sizeof(e->file));
			Str_Copy(e->name, name, sizeof(e->name));
			Str_Copy(e->module, module, sizeof(e->module));
			Str_Copy(e->names, names, sizeof(e->names));
			return e;
		}
		if (e->hash == h && e->line == co->co_firstlineno && !strcmp(e->name, name) && !strcmp(e->file, file) &&
			!strcmp(e->module, module) && !strcmp(e->names, names)) {
			return e;
		}
	}
	pyprof_dropped++;
	return NULL;
}

/* the profile function: (frame, event, arg) */
static PyObject *PyProf_Event(PyObject *self, PyObject *args) {
	PyFrameObject *f;
	char *event;
	PyObject *arg;
	int64_t now;

	(void)self;
	if (!PyArg_ParseTuple(args, "OsO", &f, &event, &arg) || !PyFrame_Check(f)) {
		PyErr_Clear();
		return Py_ReturnNone();
	}
	Plat_Counter(&now);
	if (!strcmp(event, "call")) {
		pyProfEntry_t *e = PyProf_Entry(f);
		if (!e) {
			return Py_ReturnNone();
		}
		e->calls++;
		if (e->kind == 2) {
			PyObject *g = PyDict_GetItemString(f->f_globals, (char *)e->name);
			if (g && PyCFunction_Check(g)) {
				e->shadowed++;
			}
		}
		if (pyprof_depth < PYPROF_STACK) {
			pyprof_stack[pyprof_depth++] = (pyProfCall_t){ f, e, now, 0 };
		}
	} else if (!strcmp(event, "return")) {
		/* calls above it on the stack left through an exception: dropped without time */
		int d = pyprof_depth;
		while (d > 0 && pyprof_stack[d - 1].frame != f) {
			d--;
		}
		if (d > 0) {
			pyProfCall_t *c = &pyprof_stack[d - 1];
			int64_t t = now - c->start;
			c->entry->incl += t;
			c->entry->self += t - c->children;
			pyprof_depth = d - 1;
			if (pyprof_depth > 0) {
				pyprof_stack[pyprof_depth - 1].children += t;
			}
		}
	}
	return Py_ReturnNone();
}

static PyMethodDef pyprof_event_def = { "PyProfileEvent", (PyCFunction)PyProf_Event, METH_VARARGS, NULL };

static int PyProf_Write(const char *path) {
	FILE *fp = pyprof_table ? fopen(path, "w") : NULL;
	if (!fp) {
		return 0;
	}
	fprintf(fp, "# SCRAP_PY_PROFILE interpreters %d dropped %d; times in us\n", pyprof_interpreters, pyprof_dropped);
	fprintf(fp, "kind\tcalls\tshadowed\tincl_us\tself_us\tmodule\tfile\tname\tline\tcode_bytes\tnames\n");
	for (int i = 0; i < PYPROF_ENTRIES; i++) {
		const pyProfEntry_t *e = &pyprof_table[i];
		if (e->used) {
			fprintf(fp, "%s\t%u\t%u\t%lld\t%lld\t%s\t%s\t%s\t%d\t%d\t%s\n", pyprof_kinds[e->kind], e->calls,
				e->shadowed, (long long)(e->incl / 10), (long long)(e->self / 10), e->module, e->file, e->name, e->line,
				e->codeSize, e->names);
		}
	}
	fclose(fp);
	return 1;
}

/* DumpPyProfile([file]) : ScrapC only, while SCRAP_PY_PROFILE is set; the table into file (default the variable's) */
static PyObject *PyProf_Dump(PyObject *self, PyObject *args) {
	char *path = (char *)pyprof_path;
	(void)self;
	if (!PyArg_ParseTuple(args, "|s", &path)) {
		return NULL;
	}
	return Py_BuildValue("i", PyProf_Write(path));
}

static PyMethodDef pyprof_dump_def = { "DumpPyProfile", (PyCFunction)PyProf_Dump, METH_VARARGS, NULL };

/* after Py_Initialize: the profile function of the new thread state and Scrap.DumpPyProfile */
void PyProfile_Start(void) {
	if (!pyprof_path) {
		const char *path = getenv("SCRAP_PY_PROFILE");
		if (!path || !*path) {
			return;
		}
		pyprof_table = calloc(PYPROF_ENTRIES, sizeof(*pyprof_table));
		if (!pyprof_table) {
			return;
		}
		pyprof_path = path;
	}
	pyprof_interpreters++;
	pyprof_depth = 0;
	PyThreadState *ts = PyThreadState_Get();
	Py_XDECREF(ts->sys_profilefunc);
	ts->sys_profilefunc = PyCFunction_New(&pyprof_event_def, NULL);
	PyObject *scrap = PyImport_ImportModule("Scrap");
	PyObject *dump = scrap ? PyCFunction_New(&pyprof_dump_def, NULL) : NULL;
	if (dump) {
		PyDict_SetItemString(PyModule_GetDict(scrap), "DumpPyProfile", dump);
	}
	Py_XDECREF(dump);
	Py_XDECREF(scrap);
	PyErr_Clear();
}

/* after Py_Finalize: the table so far into the file */
void PyProfile_End(void) {
	if (pyprof_path) {
		PyProf_Write(pyprof_path);
	}
}
