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

void Python_Init(void) {
	char cmd[0x100];

	Py_Initialize();
	PyProfile_Start();
	g_PythonReady = 1;
	PyRun_ScrapString("class ConsoleOutput:\n  softspace=1\n  def write(self,message):\n    import Scrap\n"
		"    if message is None:\n      Scrap.ConsoleOut('None')\n    else:\n      Scrap.ConsoleOut(message)\n"
		"  def flush(self):\n    pass\n");
	PyRun_ScrapString("class ConsoleErrorOutput:\n  softspace=1\n  def write(self,message):\n    import Scrap\n"
		"    if message is None:\n      Scrap.ConsoleError('None')\n    else:\n      Scrap.ConsoleError(message)\n"
		"  def flush(self):\n    pass\n");
	PyRun_ScrapString("import sys");
	PyRun_ScrapString("sys.stdout=ConsoleOutput()");
	PyRun_ScrapString("sys.stderr=ConsoleErrorOutput()");
	PyRun_ScrapString("print 'Python Ver '+sys.version");
	PyRun_ScrapString("sys.path.append('Scripts')");
	PyRun_ScrapString("import Scrap,SInput,SNet,SWeap,SScorer,SVec,SSound,SAI,SAct,SFX,SLogic");
	Script_CheckEngine();
	g_PyPackImport = 1;
	if (g_IsViewer) {
		PyRun_ScrapString("import Viewer");
		Str_Printf(cmd, sizeof(cmd), "Viewer.Init('%s')", g_pApp->levelPath);
	} else {
		PyRun_ScrapString("Scrap.OpenPack('Scripts')");
		PyRun_ScrapString("Scrap.PreloadLibrary('Init','Init.pyc')");
		PyRun_ScrapString("Scrap.ClosePack()");
		PyRun_ScrapString("import Init");
		Str_Printf(cmd, sizeof(cmd), "Init.Init('%s')", g_pApp->levelPath);
	}
	PyRun_ScrapString(cmd);
	PyRun_ScrapString(g_PythonExecute);
	/* the marshal buffer of Pack_PreloadLibrary is not needed after the start */
	cStaticBuffer *buf = g_PyStaticBuffer;
	CHECK(!buf->lock);
	Mem_Free(buf->data);
	buf->data = NULL;
	buf->size = 0;
}

void Python_Close(void) {
	PyRun_ScrapString("import Init;Init.Close()");
	cPy_Function_ClearAll();
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
 */
static int py_gate = -1;
static byte py_refuse;
static FILE *py_gateFile;

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
	if (!Py_GateOn() || !py_refuse) {
		return 1;
	}
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
	if (Py_GateOn()) {
		g_PyImportGate = Py_GateImport;
	}
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
