/*
 * Start and end of the interpreter, and modules from the script pack. Original source: PyInterface/PythonInit.cpp.
 */
#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/pyfunction.h"
#include "scrap/pymempool.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/staticinit.h"

#include <marshal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cGVar GVar_Viewer;

#define g_IsViewer (GVar_Viewer.value.i)		/* model viewer build: Viewer.py instead of Init.py */
static cGVar GVar_PythonExecute;
#define g_PythonExecute (GVar_PythonExecute.value.s)	/* engine variable PythonExecute */
cStaticBuffer cStaticBuffer_PyStaticBuffer[2];

#define g_PyStaticBuffer (&cStaticBuffer_PyStaticBuffer[0])

/* PyRun_SimpleString of the exe, changed by the engine: CR dropped, the code is called "Scrap input" */
int PyRun_ScrapString(const char *code);
/* frees the free lists of the object types after Py_Finalize */
void Py_FiniObjects(void);

byte g_PythonReady;

/*
 * The start and end lines of the exe, each a PyRun_ScrapString in __main__ (single_input), made in C: a line that
 * fails prints its error and the next line runs; a value an expression statement gives is printed when it is not None
 * (PRINT_EXPR of the interactive mode); Py_FlushLine ends every line. Names are looked up in __main__ as LOAD_NAME
 * does, imports stored there.
 */

/* the end of a start line: its error printed, else the line flushed */
static void Py_LineEnd(byte ok) {
	if (!ok) {
		PyErr_Print();
	} else if (Py_FlushLine()) {
		PyErr_Clear();
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
			PyErr_SetString(PyExc_RuntimeError, "lost sys.stdout");
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

/* name = v in __main__ (STORE_NAME, v given away) */
static byte Py_LineStore(const char *name, PyObject *v) {
	PyObject *m = v ? PyImport_AddModule("__main__") : NULL, *k = m ? PyString_InternFromString((char *)name) : NULL;
	int bad = !k || PyDict_SetItem(PyModule_GetDict(m), k, v) != 0;
	Py_XDECREF(k);
	Py_XDECREF(v);
	return !bad;
}

/* import a, b, ... in __main__ (NULL ends the names; the line stops at the first error) */
static byte Py_LineImport(const char *const *names) {
	for (; *names; names++) {
		if (!Py_LineStore(*names, PyImport_ImportModule((char *)*names))) {
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
	PyProfile_Start();
	g_PythonReady = 1;
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
	/* STAGE 7: the engine variable PythonExecute (the command line, the MCP harness) is Python; it goes with the
	 * interpreter (a C command channel takes the harness): the build without it says so when it is set */
#ifdef SCRAP_NO_PYTHON
	if (g_PythonExecute && *g_PythonExecute && (!GVar_PythonExecute.def.s ||
		strcmp(g_PythonExecute, GVar_PythonExecute.def.s))) {
		Con_Errorf("PythonExecute ignored, no Python interpreter in this build: %s\n", g_PythonExecute);
	}
	/* the switches that run the .pyc of the packs instead of the C modules (Script.c, ScriptModule.c) */
	static byte told;
	const char *pack = getenv("SCRAP_PACK_SCRIPTS"), *cmod = getenv("SCRAP_CMOD");
	if (!told && ((pack && *pack == '1') || (cmod && *cmod == '0'))) {
		told = 1;
		Con_Errorf("%s: no script module loads, the scripts of the packs need the Python interpreter (not in this "
			"build)\n", pack && *pack == '1' ? "SCRAP_PACK_SCRIPTS=1" : "SCRAP_CMOD=0");
	}
#else
	PyRun_ScrapString(g_PythonExecute);
#endif
	/* the marshal buffer of Pack_PreloadLibrary is not needed after the start */
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
	Py_Finalize();
	PyProfile_End();
	g_PythonReady = 0;
}

void Python_Shutdown(void) {
	Py_FiniObjects();
	PyPool_Delete();
}

/*
 * The gate of the pack modules (the way to a build without Python). SCRAP_PY_GATE=<file> appends a line per pack
 * module the engine would load: the level, what happened, the module name and the file. What happened: "pyc" (the
 * module code of the .pyc runs), "cmod" (the module is made from its C definition, Script_ModuleMake), "loaded" (it
 * is loaded already, PreloadLibrary does nothing), "import" (a file the import of Python reads, not preloaded),
 * "packlog" (the import while the engine records the files of the packs) and "refused". With SCRAP_PY_REFUSE=1 as
 * well no module code runs: a module without a C definition stays out of sys.modules (an import of it raises
 * ImportError). Without the variables nothing changes.
 *
 * The build without the interpreter (SCRAP_NO_PYTHON) always refuses: a module without a C definition is not
 * loaded (a .pyc of the packs or of a mod, a .py or .pyc the import would read), a console line says so the first
 * time for each name; the gate log lines need SCRAP_PY_GATE as before.
 */
static int py_gate = -1;
static byte py_refuse;
static FILE *py_gateFile;

#ifdef SCRAP_NO_PYTHON
#define PY_IGNORED_MAX 512

/* the console line of a module the build without the interpreter does not load, once for each name */
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
#endif

static byte Py_GateOn(void) {
	if (py_gate < 0) {
		const char *e = getenv("SCRAP_PY_GATE");
		py_gate = e && *e && strcmp(e, "0");
		if (py_gate) {
			py_gateFile = strcmp(e, "1") ? fopen(e, "a") : NULL;
			e = getenv("SCRAP_PY_REFUSE");
			py_refuse = e && *e == '1';
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

/* 1 when the module code may run; else the refusal is logged and ImportError set */
static byte Py_GateAllow(const char *name, const char *file) {
#ifdef SCRAP_NO_PYTHON
	Py_IgnoredOnce(name, file);
#else
	if (!Py_GateOn() || !py_refuse) {
		return 1;
	}
#endif
	Py_GateNote("refused", name, file);
	PyErr_Format(PyExc_ImportError, "pack module %.100s (%.100s) refused: no C definition", name, file);
	return 0;
}

/* the import of Python reads a .py or .pyc that was not preloaded (src/python/import.c) */
static int Py_GateImport(const char *path) {
	const char *base = strrchr(path, '/');
	Py_GateNote("import", base ? base + 1 : path, path);
	return Py_GateAllow(base ? base + 1 : path, path);
}

/* a module made from its C definition (1), none (0, the .pyc runs) or a failed one (-1, error set); logged */
static int Py_ModuleMake(const char *name, const char *file, PyObject **m) {
	int made = Script_ModuleMake(name, file, m);
	if (made) {
		Py_GateNote("cmod", name, file);
	}
	return made;
}

/*
 * PreloadLibrary(name, file): imports the compiled module file of the open pack as name. While the engine
 * records the files read from packs it imports through the normal path instead and logs the file. A module that has
 * a C definition (Script_ModuleMake) is made from it and its .pyc is not read.
 */
void Pack_PreloadLibrary(const char *name, const char *file) {
	cFilePack f;

	cFilePack_ctor(&f);
	CHECK(Py_IsReady());
#ifdef SCRAP_NO_PYTHON
	g_PyImportGate = Py_GateImport;
#else
	if (Py_GateOn()) {
		g_PyImportGate = Py_GateImport;
	}
#endif
	if (PackLog_Closed()) {
		if (PyDict_GetItemString(PyImport_GetModuleDict(), (char *)name)) {
			Py_GateNote("loaded", name, file);
		} else {
			PyObject *m = NULL;
			int made = Py_ModuleMake(name, file, &m);
			if (made) {
				if (made < 0) {
					PyErr_Print();
				}
				CHECK(m != NULL);
			} else if (Py_GateAllow(name, file)) {
#ifndef SCRAP_NO_PYTHON /* never there: Py_GateAllow refuses */
				Py_GateNote("pyc", name, file);
				CHECK(cFilePack_Open(&f.file.base, file, FILE_MODE_READ));
				int size = cFilePack_Size(&f.file.base);
				char *data = cStaticBuffer_Get(g_PyStaticBuffer, size);
				cFilePack_Read(&f.file.base, data, size);
				CHECK(cFilePack_Close(&f.file.base));
				/* .pyc: magic and time, then the marshaled code object */
				PyObject *co = PyMarshal_ReadObjectFromString(data + 8, size - 8);
				CHECK(g_PyStaticBuffer->lock);
				g_PyStaticBuffer->lock = 0;
				CHECK(co != NULL);
				CHECK(co->ob_type == &PyCode_Type);
				char path[0x100];
				Str_Printf(path, sizeof(path), "PACK: %s", file);
				m = PyImport_ExecCodeModuleEx((char *)name, co, path);
				Py_DECREF(co);
				CHECK(m != NULL);
				Script_ModuleLoaded(name, m);
#endif
			} else {
				PyErr_Clear();
			}
			Py_XDECREF(m);
		}
	} else {
		Con_Verbosef("Trying to open 'PACK: %s', using import %s\n", file, name);
		PackLog_Add(file);
		PyObject *m = NULL;
		int made = PyDict_GetItemString(PyImport_GetModuleDict(), (char *)name) ? 0 : Py_ModuleMake(name, file, &m);
		if (!made && Py_GateAllow(name, file)) {
			Py_GateNote("packlog", name, file);
			g_PyPackImport = 0;
			m = PyImport_ImportModule((char *)name);
			g_PyPackImport = 1;
			if (m) {
				Script_ModuleLoaded(name, m);
			}
		}
		if (m) {
			Py_DECREF(m);
		} else {
			PyObject *err = PySys_GetObject("stderr");
			if (Py_VerboseFlag) {
				PyFile_WriteString("'import site' failed; traceback:\n", err);
				PyErr_Print();
			} else {
				PyFile_WriteString("'import site' failed; use -v for traceback\n", err);
				PyErr_Clear();
			}
		}
	}
	cFilePack_dtor(&f);
}

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_VECTOR(Fini_cStaticBuffer_PyStaticBuffer, cStaticBuffer_PyStaticBuffer, sizeof(cStaticBuffer), 2, cStaticBuffer_dtor)
INIT_VECTOR(Init_cStaticBuffer_PyStaticBuffer, Fini_cStaticBuffer_PyStaticBuffer, cStaticBuffer_PyStaticBuffer, sizeof(cStaticBuffer), 2, cStaticBuffer_ctor, cStaticBuffer_dtor)
FINI_GVAR_STRING(Fini_GVar_PythonExecute, &GVar_PythonExecute)
INIT_GVAR_STRING(Init_GVar_PythonExecute, Fini_GVar_PythonExecute, &GVar_PythonExecute, "PythonExecute", 4, "This will run a pyhon command.", "print 'No PythonExecute'")
INIT_GVAR_INT(Init_GVar_Viewer, &GVar_Viewer, "Viewer", 4, " 0(default) : Normal game,  1 : Init Viewer", 0)
