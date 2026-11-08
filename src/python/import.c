/*
 * Python/import.c with the change of the engine in Scrap.exe: once the startup imports are done (g_PyPackImport,
 * set by Python_Init and Pack_PreloadLibrary), reading a .pyc warns on stderr that the module was not preloaded from
 * the script pack (the exe prints it first thing in read_compiled_module, here through its
 * PyMarshal_ReadObjectFromFile call). The exe's warning after writing a .pyc has no place any more: nothing is
 * compiled, so nothing is written.
 *
 * There is no interpreter (src/python/nopy): no .py or .pyc is read. The gate is told (it may log the file) and the
 * import raises ImportError, as does a frozen module; the readers of code return a NULL the compiler sees, so what
 * would parse, compile, write or run the code drops, and PyImport_ExecCodeModule(Ex) are not built (NOPY_DROP).
 */
#include "Python.h"
#include "compile.h"
#include "pyengine.h"


int g_PyPackImport;
/* ScrapC's gate of the pack modules (SCRAP_PY_GATE, src/game/PyInterface/PythonInit.c): told of every .py or .pyc
 * this import reads; it refuses them all */
void (*g_PyImportGate)(const char *path);

static void pack_gate(const char *path) {
	if (g_PyImportGate)
		g_PyImportGate(path);
}

static void pack_read(const char *cpathname) {
	if (g_PyPackImport)
		PySys_WriteStderr("WARNING: import %s (read)\n the library was not preloaded\n", cpathname);
}

#define PyImport_ExecCodeModule(args...) nopy_PyImport_ExecCodeModule(args)
#define PyImport_ExecCodeModuleEx(args...) nopy_PyImport_ExecCodeModuleEx(args)
NOPY_DROP(PyObject *, PyImport_ExecCodeModule);
NOPY_DROP(PyObject *, PyImport_ExecCodeModuleEx);
#define PyMarshal_ReadObjectFromFile(fp) ((void)(fp), pack_read(cpathname), pack_gate(cpathname), \
	Py_NOINTERP(PyObject *, PyExc_ImportError, cpathname))
#define PyMarshal_ReadObjectFromString(s, n) ((void)(s), (void)(n), \
	Py_NOINTERP(PyObject *, PyExc_ImportError, "frozen module"))
#define PyParser_SimpleParseFile(fp, path, start) ((void)(fp), (void)(start), pack_gate(path), \
	Py_NOINTERP(node *, PyExc_ImportError, path))

#include "Python/import.c"
