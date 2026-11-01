/*
 * Start and end of the interpreter, and modules from the script pack. Original source: PyInterface/PythonInit.cpp.
 */
#include "scrap/console.h"
#include "scrap/pyfunction.h"
#include "scrap/pymempool.h"
#include "scrap/scrap.h"

#include <marshal.h>

#define g_IsViewer GAME_GLOBAL_IN(int, GVar_Viewer, 16)		/* model viewer build: Viewer.py instead of Init.py */
#define g_PythonExecute GAME_GLOBAL_IN(char *, GVar_PythonExecute, 16)	/* engine variable PythonExecute */
#define g_PyStaticBuffer (&GAME_GLOBAL_IN(cStaticBuffer, cStaticBuffer_PyStaticBuffer, 0))

/* PyRun_SimpleString of the exe, changed by the engine: CR dropped, the code is called "Scrap input" */
int PyRun_ScrapString(const char *code);
/* frees the free lists of the object types after Py_Finalize */
void Py_FiniObjects(void);

void Python_Init(void) {
	char cmd[0x100];

	Py_Initialize();
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
	g_PythonReady = 0;
}

void Python_Shutdown(void) {
	Py_FiniObjects();
	PyPool_Delete();
}

/*
 * PreloadLibrary(name, file): imports the compiled module file of the open pack as name. While the engine
 * records the files read from packs it imports through the normal path instead and logs the file.
 */
void Pack_PreloadLibrary(const char *name, const char *file) {
	cFilePack f;

	cFilePack_ctor(&f);
	CHECK(Py_IsReady());
	if (PackLog_Closed()) {
		if (!PyDict_GetItemString(PyImport_GetModuleDict(), (char *)name)) {
			CHECK(cFilePack_Open(&f, file, FILE_MODE_READ));
			int size = cFilePack_Size(&f);
			char *data = cStaticBuffer_Get(g_PyStaticBuffer, size);
			cFilePack_Read(&f, data, size);
			CHECK(cFilePack_Close(&f));
			/* .pyc: magic and time, then the marshaled code object */
			PyObject *co = PyMarshal_ReadObjectFromString(data + 8, size - 8);
			CHECK(g_PyStaticBuffer->lock);
			g_PyStaticBuffer->lock = 0;
			CHECK(co != NULL);
			CHECK(co->ob_type == &PyCode_Type);
			char path[0x100];
			Str_Printf(path, sizeof(path), "PACK: %s", file);
			PyObject *m = PyImport_ExecCodeModuleEx((char *)name, co, path);
			Py_DECREF(co);
			CHECK(m != NULL);
			Py_DECREF(m);
		}
	} else {
		Con_Verbosef("Trying to open 'PACK: %s', using import %s\n", file, name);
		PackLog_Add(file);
		g_PyPackImport = 0;
		PyObject *m = PyImport_ImportModule((char *)name);
		g_PyPackImport = 1;
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
