/*
 * Start and end of the interpreter, and modules from the script pack. Original source: PyInterface/PythonInit.cpp.
 */
#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/modvars.h"
#include "scrap/pyfunction.h"
#include "scrap/mempool.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/staticinit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cGVar GVar_Viewer;

#define g_IsViewer (GVar_Viewer.value.i)		/* model viewer build: Viewer.py instead of Init.py */
static cGVar GVar_PythonExecute;
#define g_PythonExecute (GVar_PythonExecute.value.s)	/* engine variable PythonExecute */
cStaticBuffer cStaticBuffer_PyStaticBuffer[2];

#define g_PyStaticBuffer (&cStaticBuffer_PyStaticBuffer[0])

/* frees the free lists of the object types after Py_Finalize */
void Py_FiniObjects(void);

byte g_PythonReady;

/*
 * The start and end lines of the exe, each a PyRun_SimpleString of the engine in __main__ (single_input; "Scrap
 * input", CR dropped), made in C: a line that
 * fails prints its error and the next line runs; a value an expression statement gives is printed when it is not None
 * (PRINT_EXPR of the interactive mode); Py_FlushLine ends every line. Names are looked up in __main__ as LOAD_NAME
 * does, imports stored there.
 */

/* the end of a start line: its error printed, else the line flushed */
static void Py_LineEnd(byte ok) {
	if (!ok) {
		Native_ErrPrint();
	} else if (Py_FlushLine()) {
		Native_ErrClear();
	}
}

/* an expression statement of single_input (PRINT_EXPR): a value that is not None printed with its repr and kept in
 * __builtin__._; v given away; 0 with the error set */
static byte Py_LineExpr(PyObject *v) {
	int err = 0;
	if (!v) {
		return 0;
	}
	if (v != Py_None) {
		PyObject *b = PyEval_GetBuiltins(), *out = NULL;
		err = PyDict_SetItemString(b, "_", Py_None);
		if (!err) {
			err = Py_FlushLine();
		}
		if (!err && !(out = PySys_GetObject("stdout"))) {
			Native_Raise(NE_RUNTIME, "lost sys.stdout");
			err = -1;
		}
		if (!err) {
			err = PyFile_WriteObject(v, out, 0);
		}
		if (!err) {
			PyFile_SoftSpace(out, 1);
			err = Py_FlushLine();
		}
		if (!err) {
			err = PyDict_SetItemString(b, "_", v);
		}
	}
	Py_DECREF(v);
	return err == 0;
}

/* name = v in __main__ (STORE_NAME into the registry's variables of __main__, v given away) */
static byte Py_LineStore(const char *name, PyObject *v) {
	PyObject *m = v ? PyImport_AddModule("__main__") : NULL;
	int bad = !m || ModVar_DictSet(PyModule_GetDict(m), name, v) != 0;
	Py_XDECREF(v);
	return !bad;
}

/* import a, b, ... in __main__ (NULL ends the names; the line stops at the first error) */
static byte Py_LineImport(const char *const *names) {
	for (; *names; names++) {
		if (!Py_LineStore(*names, Script_Module(*names))) {
			return 0;
		}
	}
	return 1;
}

/* a name of __main__ with attributes ("Init.Init") as LOAD_NAME and LOAD_ATTR find it: a new reference */
static PyObject *Py_LineName(const char *text) {
	PyObject *m = PyImport_AddModule("__main__"), *v = NULL;
	return m && Script_NameChain(NULL, PyModule_GetDict(m), text, &v) > 0 ? v : NULL;
}

/* name(args) as an expression statement of a line (args given away) */
static byte Py_LineCall(const char *name, PyObject *args) {
	PyObject *f = args ? Py_LineName(name) : NULL, *r = f ? PyEval_CallObject(f, args) : NULL;
	Py_XDECREF(f);
	Py_XDECREF(args);
	return Py_LineExpr(r);
}

/* func('text') of a line as the exe builds it (Str_Printf in 0x100 chars): the text between the quotes becomes the
 * string the compiler makes of it (escapes, interning); a quote or a control character in it, or a cut line, is the
 * line's SyntaxError */
static byte Py_LineCallText(const char *func, const char *text) {
	char cmd[0x100];
	int n = Str_Printf(cmd, sizeof(cmd), "%s('%s')", func, text), len = (int)strlen(func);
	PyObject *m = PyImport_AddModule("__main__"), *v = NULL;
	if (m && n == (int)strlen(cmd) && n > len + 2 && cmd[n - 1] == ')') {
		cmd[n - 1] = '\0';
		v = Script_Literal(PyModule_GetDict(m), cmd + len + 1);
	}
	if (!v || !PyString_Check(v)) {
		Py_XDECREF(v);
		Script_NotRead("start line", cmd);
		return 0;
	}
	return Py_LineCall(func, Py_BuildValue("(N)", v));
}

/* write(message) of the classes ConsoleOutput and ConsoleErrorOutput: Scrap.ConsoleOut(message) or
 * Scrap.ConsoleError(message), 'None' for None */
static PyObject *Py_ConsoleWrite(PyObject *args, const char *func) {
	PyObject *message;
	if (!PyArg_ParseTuple(args, "O", &message)) {
		return NULL;
	}
	PyObject *r = message == Py_None ? Script_Ask("Scrap", func, "(s)", "None") : Script_Ask("Scrap", func, "(O)",
		message);
	if (!r) {
		return NULL;
	}
	Py_DECREF(r);
	return Py_ReturnNone();
}

static PyObject *Py_ConsoleOutWrite(PyObject *self, PyObject *args) {
	(void)self;
	return Py_ConsoleWrite(args, "ConsoleOut");
}

static PyObject *Py_ConsoleErrorWrite(PyObject *self, PyObject *args) {
	(void)self;
	return Py_ConsoleWrite(args, "ConsoleError");
}

static PyObject *Py_ConsoleFlush(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_ReturnNone();
}

static PyMethodDef py_consoleOut[] = { { "write", Py_ConsoleOutWrite, 1, NULL }, { "flush", Py_ConsoleFlush, 1, NULL } };
static PyMethodDef py_consoleError[] = { { "write", Py_ConsoleErrorWrite, 1, NULL },
	{ "flush", Py_ConsoleFlush, 1, NULL } };

/* class name: softspace = 1, write(message), flush() (C functions in the class, called from the instance with the
 * message alone), in __main__; the dict filled in the order of the class body, then __doc__ and __module__ as
 * PyClass_New adds them */
static byte Py_LineConsoleClass(const char *name, PyMethodDef *methods) {
	PyObject *d = PyDict_New(), *n = d ? PyString_InternFromString((char *)name) : NULL, *c = NULL;
	PyObject *one = n ? PyInt_FromLong(1) : NULL, *w = one ? PyCFunction_New(&methods[0], NULL) : NULL;
	PyObject *fl = w ? PyCFunction_New(&methods[1], NULL) : NULL, *b = fl ? PyTuple_New(0) : NULL;
	if (b && !PyDict_SetItemString(d, "softspace", one) && !PyDict_SetItemString(d, "write", w) &&
		!PyDict_SetItemString(d, "flush", fl) && !PyDict_SetItemString(d, "__doc__", Py_None) &&
		!PyDict_SetItemString(d, "__module__", PyDict_GetItemString(PyModule_GetDict(PyImport_AddModule("__main__")),
		"__name__"))) {
		c = PyClass_New(b, d, n);
	}
	Py_XDECREF(b);
	Py_XDECREF(fl);
	Py_XDECREF(w);
	Py_XDECREF(one);
	Py_XDECREF(n);
	Py_XDECREF(d);
	return Py_LineStore(name, c);
}

/* sys.<attr> = <name of a class>() */
static byte Py_LineSysOutput(const char *attr, const char *cls) {
	PyObject *c = Py_LineName(cls), *o = c ? PyEval_CallObject(c, NULL) : NULL, *sys = o ? Py_LineName("sys") : NULL;
	int bad = !sys || PyObject_SetAttrString(sys, (char *)attr, o) != 0;
	Py_XDECREF(sys);
	Py_XDECREF(o);
	Py_XDECREF(c);
	return !bad;
}

/* print 'Python Ver '+sys.version */
static byte Py_LineVersion(void) {
	PyObject *v = Py_LineName("sys.version"), *t = v ? Script_SPlus("Python Ver ", v) : NULL;
	byte ok = t && Script_Print(&t, 1);
	Py_XDECREF(t);
	Py_XDECREF(v);
	return ok;
}

void Python_Init(void) {
	static const char *const sys[] = { "sys", NULL }, *const engine[] = { "Scrap", "SInput", "SNet", "SWeap", "SScorer",
		"SVec", "SSound", "SAI", "SAct", "SFX", "SLogic", NULL }, *const init[] = { "Init", NULL },
		*const viewer[] = { "Viewer", NULL };

	Py_Initialize();
	g_PythonReady = 1;
	/* the variables of __main__ (the start lines' imports, the test channel's names) are native values */
	ModVar_Keep(PyModule_GetDict(PyImport_AddModule("__main__")));
	Py_LineEnd(Py_LineConsoleClass("ConsoleOutput", py_consoleOut));
	Py_LineEnd(Py_LineConsoleClass("ConsoleErrorOutput", py_consoleError));
	Py_LineEnd(Py_LineImport(sys));
	Py_LineEnd(Py_LineSysOutput("stdout", "ConsoleOutput"));
	Py_LineEnd(Py_LineSysOutput("stderr", "ConsoleErrorOutput"));
	Py_LineEnd(Py_LineVersion());
	Py_LineEnd(Py_LineCall("sys.path.append", Py_BuildValue("(s)", "Scripts")));
	Py_LineEnd(Py_LineImport(engine));
	Script_CheckEngine();
	g_PyPackImport = 1;
	if (g_IsViewer) {
		Py_LineEnd(Py_LineImport(viewer));
		Py_LineEnd(Py_LineCallText("Viewer.Init", g_pApp->levelPath));
	} else {
		Py_LineEnd(Py_LineCall("Scrap.OpenPack", Py_BuildValue("(s)", "Scripts")));
		Py_LineEnd(Py_LineCall("Scrap.PreloadLibrary", Py_BuildValue("(ss)", "Init", "Init.pyc")));
		Py_LineEnd(Py_LineCall("Scrap.ClosePack", PyTuple_New(0)));
		Py_LineEnd(Py_LineImport(init));
		Py_LineEnd(Py_LineCallText("Init.Init", g_pApp->levelPath));
	}
	/* the engine variable PythonExecute (the command line) was a line of Python run here: there is no interpreter, an
	 * error says so when it is set */
	if (g_PythonExecute && *g_PythonExecute && (!GVar_PythonExecute.def.s ||
		strcmp(g_PythonExecute, GVar_PythonExecute.def.s))) {
		Con_Errorf("PythonExecute ignored, no Python interpreter in this build: %s\n", g_PythonExecute);
	}
	/* the switches that ran the .pyc of the packs instead of the C modules (Script.c, ScriptModule.c) */
	static byte told;
	const char *pack = getenv("SCRAP_PACK_SCRIPTS"), *cmod = getenv("SCRAP_CMOD");
	if (!told && ((pack && *pack == '1') || (cmod && *cmod == '0'))) {
		told = 1;
		Con_Errorf("%s: no script module loads, the scripts of the packs need the Python interpreter (not in this "
			"build)\n", pack && *pack == '1' ? "SCRAP_PACK_SCRIPTS=1" : "SCRAP_CMOD=0");
	}
	/* the buffer the exe's Pack_PreloadLibrary read the .pyc into is not needed after the start (the net code gets it
	 * again when it needs it) */
	cStaticBuffer *buf = g_PyStaticBuffer;
	CHECK(!buf->lock);
	Mem_Free(buf->data);
	buf->data = NULL;
	buf->size = 0;
}

void Python_Close(void) {
	static const char *const init[] = { "Init", NULL };

	Py_LineEnd(Py_LineImport(init) && Py_LineCall("Init.Close", PyTuple_New(0)));
	cPy_Function_ClearAll();
	CharsNPC_Close();
	/* the module variables live in the registry, not in the module dicts that the cleanup of Py_Finalize clears */
	ModVar_Close();
	Native_HookClear();
	Py_Finalize();
	g_PythonReady = 0;
}

void Python_Shutdown(void) {
	Py_FiniObjects();
	ScriptPool_Delete();
}

/*
 * The gate of the pack modules. There is no interpreter, so a module without a C definition is not loaded (a .pyc of
 * the packs or of a mod, a .py or .pyc the import would read): it stays out of sys.modules (an import of it raises
 * ImportError) and a console line says so the first time for each name. SCRAP_PY_GATE=<file> appends a line per pack
 * module the engine would load: the level, what happened, the module name and the file. What happened: "cmod" (the
 * module is made from its C definition, Script_ModuleMake), "loaded" (it is loaded already, PreloadLibrary does
 * nothing), "import" (a file the import of Python would read, not preloaded), "refused" and "notread" (a text
 * Script_NotRead does not run).
 */
static int py_gate = -1;
static FILE *py_gateFile;

#define PY_IGNORED_MAX 512

/* the console line of a module that is not loaded, once for each name */
static void Py_IgnoredOnce(const char *name, const char *file) {
	static char seen[PY_IGNORED_MAX][0x40];
	static int n;

	for (int i = 0; i < n; i++) {
		if (!strncmp(seen[i], name, sizeof(seen[i]) - 1)) {
			return;
		}
	}
	if (n < PY_IGNORED_MAX) {
		Str_Copy(seen[n++], name, sizeof(seen[0]));
	}
	Con_Printf("%s (%s) not loaded: it is Python code, this build has no interpreter\n", name, file);
}

static byte Py_GateOn(void) {
	if (py_gate < 0) {
		const char *e = getenv("SCRAP_PY_GATE");
		py_gate = e && *e && strcmp(e, "0");
		if (py_gate) {
			py_gateFile = strcmp(e, "1") ? fopen(e, "a") : NULL;
		}
	}
	return py_gate > 0;
}

void Py_GateNote(const char *what, const char *name, const char *file) {
	if (!Py_GateOn()) {
		return;
	}
	const char *level = g_pApp && g_pApp->levelPath ? g_pApp->levelPath : "-";
	if (py_gateFile) {
		fprintf(py_gateFile, "%s\t%s\t%s\t%s\n", level, what, name, file);
		fflush(py_gateFile);
	} else {
		Con_Printf("PYGATE %s %s %s %s\n", level, what, name, file);
	}
}

/* a module without a C definition: the refusal is logged and ImportError set */
static void Py_GateRefuse(const char *name, const char *file) {
	Py_IgnoredOnce(name, file);
	Py_GateNote("refused", name, file);
	Native_Raise(NE_IMPORT, "pack module %.100s (%.100s) refused: no C definition", name, file);
}

/* the import of Python would read a .py or .pyc that was not preloaded (src/python/import.c) */
static void Py_GateImport(const char *path) {
	const char *base = strrchr(path, '/');
	Py_GateNote("import", base ? base + 1 : path, path);
	Py_GateRefuse(base ? base + 1 : path, path);
}

/* a module made from its C definition (1), none (0, it is refused) or a failed one (-1, error set); logged */
static int Py_ModuleMake(const char *name, const char *file, PyObject **m) {
	int made = Script_ModuleMake(name, file, m);
	if (made) {
		Py_GateNote("cmod", name, file);
	}
	return made;
}

/*
 * PreloadLibrary(name, file): the module name of the compiled module file of the open pack (the exe imported the
 * .pyc; while it recorded the files read from packs, through the normal path). A module that has a C definition
 * (Script_ModuleMake) is made from it, one without is refused; no .pyc is read. While the engine records the files
 * read from packs it logs the file.
 */
void Pack_PreloadLibrary(const char *name, const char *file) {
	CHECK(Py_IsReady());
	g_PyImportGate = Py_GateImport;
	if (PackLog_Closed()) {
		if (Script_ModuleLoaded(name)) {
			Script_ModuleCountLoaded();
			Py_GateNote("loaded", name, file);
		} else {
			PyObject *m = NULL;
			int made = Py_ModuleMake(name, file, &m);
			if (made) {
				if (made < 0) {
					Native_ErrPrint();
				}
				CHECK(m != NULL);
			} else {
				Py_GateRefuse(name, file);
				Native_ErrClear();
			}
			Py_XDECREF(m);
		}
	} else {
		Con_Verbosef("Trying to open 'PACK: %s', using import %s\n", file, name);
		PackLog_Add(file);
		PyObject *m = NULL;
		int made = Script_ModuleLoaded(name) ? 0 : Py_ModuleMake(name, file, &m);
		if (!made) {
			Py_GateRefuse(name, file);
		}
		if (m) {
			Py_DECREF(m);
		} else {
			PyObject *err = PySys_GetObject("stderr");
			if (Py_VerboseFlag) {
				PyFile_WriteString("'import site' failed; traceback:\n", err);
				Native_ErrPrint();
			} else {
				PyFile_WriteString("'import site' failed; use -v for traceback\n", err);
				Native_ErrClear();
			}
		}
	}
}

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_VECTOR(Fini_cStaticBuffer_PyStaticBuffer, cStaticBuffer_PyStaticBuffer, sizeof(cStaticBuffer), 2, cStaticBuffer_dtor)
INIT_VECTOR(Init_cStaticBuffer_PyStaticBuffer, Fini_cStaticBuffer_PyStaticBuffer, cStaticBuffer_PyStaticBuffer, sizeof(cStaticBuffer), 2, cStaticBuffer_ctor, cStaticBuffer_dtor)
FINI_GVAR_STRING(Fini_GVar_PythonExecute, &GVar_PythonExecute)
INIT_GVAR_STRING(Init_GVar_PythonExecute, Fini_GVar_PythonExecute, &GVar_PythonExecute, "PythonExecute", 4, "This will run a pyhon command.", "print 'No PythonExecute'")
INIT_GVAR_INT(Init_GVar_Viewer, &GVar_Viewer, "Viewer", 4, " 0(default) : Normal game,  1 : Init Viewer", 0)
