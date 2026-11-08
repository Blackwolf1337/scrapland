/*
 * Python/import.c with the change of the engine in Scrap.exe: once the startup imports are done (g_PyPackImport,
 * set by Python_Init and Pack_PreloadLibrary), reading or writing a .pyc warns on stderr that the module was not
 * preloaded from the script pack. The exe prints the read warning first thing in read_compiled_module, the write
 * warning last in write_compiled_module (after "# wrote" of -v); the macros below put them there without changing
 * the vendored file: the read through its PyMarshal_ReadObjectFromFile call, the write through the fflush of that
 * function, which is the only one of the file, and the fclose or the -v message that follow it.
 */
/* see below: the runners of module code get other names (before the headers declare them) */
#define PyImport_ExecCodeModule Py_NoImportExecCodeModule
#define PyImport_ExecCodeModuleEx Py_NoImportExecCodeModuleEx
#include "Python.h"
#include "marshal.h"


int g_PyPackImport;
/* ScrapC's gate of the pack modules (SCRAP_PY_GATE, src/game/PyInterface/PythonInit.c): told of every .py or .pyc
 * this import reads, 0 refuses it (ImportError set) */
int (*g_PyImportGate)(const char *path);

static int pack_gate(const char *path) {
	return !g_PyImportGate || g_PyImportGate(path);
}

/* the .pyc that write_compiled_module wrote, until its warning is out */
static const char *pack_wrote;

static void pack_read(const char *cpathname) {
	if (g_PyPackImport)
		PySys_WriteStderr("WARNING: import %s (read)\n the library was not preloaded\n", cpathname);
}

static void pack_wrote_warn(void) {
	const char *cpathname = pack_wrote;

	pack_wrote = NULL;
	if (g_PyPackImport)
		PySys_WriteStderr("WARNING: import %s (wrote)\n the library was not preloaded\n", cpathname);
}

static int pack_fflush(FILE *fp, const char *cpathname) {
	pack_wrote = cpathname;
	return crt_fflush(fp);
}

/* without -v the warning follows the fclose, with -v the "# wrote" message after it */
static int pack_fclose(FILE *fp) {
	int r = crt_fclose(fp);

	if (pack_wrote && !Py_VerboseFlag)
		pack_wrote_warn();
	return r;
}

/*
 * Without the interpreter (src/python/nopy) no .py or .pyc is read: the gate is told (it may log the file) and the
 * import raises ImportError; the frozen modules and PyImport_ExecCodeModule(Ex) go (other names, dropped by
 * --gc-sections), what they would run raises too.
 */
#include "pyengine.h"

#define PyMarshal_ReadObjectFromFile(fp) (pack_read(cpathname), pack_gate(cpathname), \
	(PyObject *)Py_NoInterpreter(PyExc_ImportError, cpathname))
#define PyMarshal_ReadObjectFromString(s, n) ((void)(s), (void)(n), \
	(PyObject *)Py_NoInterpreter(PyExc_ImportError, "frozen module"))
#define PyParser_SimpleParseFile(fp, path, start) ((void)(fp), (void)(start), pack_gate(path), \
	(node *)Py_NoInterpreter(PyExc_ImportError, path))
#define PyNode_Compile(n, path) ((void)(n), (PyCodeObject *)Py_NoInterpreter(PyExc_ImportError, path))
#define PyNode_Free(n) ((void)(n))
#define PyEval_EvalCode(co, g, l) ((void)(co), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_ImportError, "module code"))
#define crt_fflush(fp) pack_fflush(fp, cpathname)
#define crt_fclose_file(fp) pack_fclose(fp)
#define PySys_WriteStderr(args...) (PySys_WriteStderr(args), pack_wrote ? pack_wrote_warn() : (void)0)

#include "Python/import.c"
