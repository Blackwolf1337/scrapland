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
 * OBJLIB 7e4b: Py_LibModule and Py_LibPath give ScrapC the built-in modules of the library and sys.path.
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

#include "osdefs.h"

/*
 * There is no interpreter (ScrapC keeps its modules natively, src/game/Script/ScriptImport.c), but the object library
 * needs a thread state (Py_ReprEnter, the module dict of this file): one of the library's own, made at the first use,
 * whose modules are the library's (OBJLIB 7e4b).
 */
static void nopy_state(void) {
	PyInterpreterState *interp;

	if (_PyThreadState_Current)
		return;
	interp = PyInterpreterState_New();
	PyThreadState_Swap(PyThreadState_New(interp));
	interp->modules = PyDict_New();
}

PyObject *Py_LibModule(const char *name) {
	PyObject *m = NULL;
	struct _inittab *p;

	nopy_state();
	if (strcmp(name, "__builtin__") == 0) {
		m = _PyBuiltin_Init_1();
		if (m)
			_PyThreadState_Current->interp->builtins = PyModule_GetDict(m);
	} else if (strcmp(name, "sys") == 0) {
		m = _PySys_Init();
		if (m)
			_PyThreadState_Current->interp->sysdict = PyModule_GetDict(m);
	} else {
		for (p = _PyImport_Inittab; p->name; p++) {
			if (p->initfunc && strcmp(name, p->name) == 0) {
				(*p->initfunc)();
				if (!PyErr_Occurred())
					m = PyDict_GetItemString(PyImport_GetModuleDict(), (char *)name);
				break;
			}
		}
		if (!p->name)
			PyErr_Format(PyExc_ImportError, "No module named %.200s", name);
	}
	Py_XINCREF(m);
	return m;
}

PyObject *Py_LibPath(void) {
	/* makepathobject of sysmodule.c on the path of getpathp.c */
	char *path = Py_GetPath(), *p;
	PyObject *v = PyList_New(0), *w;

	for (p = path; v; p++) {
		if (*p == DELIM || *p == '\0') {
			w = PyString_FromStringAndSize(path, (int)(p - path));
			if (!w || PyList_Append(v, w) != 0) {
				Py_XDECREF(w);
				Py_DECREF(v);
				return NULL;
			}
			Py_DECREF(w);
			if (*p == '\0')
				break;
			path = p + 1;
		}
	}
	return v;
}
