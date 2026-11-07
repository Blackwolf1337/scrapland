/*
 * Python/bltinmodule.c without the interpreter (SCRAP_NO_PYTHON): eval, execfile, compile and input of __builtin__
 * raise SystemError before they look at their arguments (C functions of their own put over the vendored ones, which
 * would read the globals of the frame that never is, or a line of stdin); __import__ and reload find only the
 * built-in and the loaded modules (src/python/import.c refuses files).
 */
#include "Python.h"
#include "compile.h"
#include "eval.h"
#include "pyengine.h"

/* the vendored bodies, out of reach by name, refer to nothing that is gone */
#undef PyCode_Check
#define PyCode_Check(op) 0
#define PyEval_EvalCode(co, g, l) ((void)(co), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, "eval"))
#define PyRun_String(str, start, g, l) ((void)(str), (void)(start), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, "eval"))
#define PyRun_File(fp, file, start, g, l) ((void)(fp), (void)(start), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, file))
#define Py_CompileString(str, file, start) ((void)(str), (void)(file), (void)(start), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, "compile"))

#define NOPY_BUILTIN(name) \
	static PyObject *nopy_##name(PyObject *self, PyObject *args) { \
		(void)self; \
		(void)args; \
		return Py_NoInterpreter(PyExc_SystemError, #name); \
	}
NOPY_BUILTIN(compile)
NOPY_BUILTIN(eval)
NOPY_BUILTIN(execfile)
NOPY_BUILTIN(input)

static PyMethodDef nopy_builtins[] = {
	{ "compile", nopy_compile, 1, NULL },
	{ "eval", nopy_eval, 1, NULL },
	{ "execfile", nopy_execfile, 1, NULL },
	{ "input", nopy_input, 1, NULL },
	{ NULL, NULL, 0, NULL }
};

/* __builtin__ as _PyBuiltin_Init made it, with the refusals in its dict; NULL after an error */
static PyObject *nopy_builtin_module(PyObject *m) {
	PyMethodDef *ml;
	PyObject *d = m ? PyModule_GetDict(m) : NULL;

	for (ml = nopy_builtins; d && ml->ml_name; ml++) {
		PyObject *f = PyCFunction_New(ml, NULL);
		int bad = !f || PyDict_SetItemString(d, ml->ml_name, f);
		Py_XDECREF(f);
		if (bad)
			return NULL;
	}
	return m;
}

#define Py_InitModule4(name, methods, doc, self, api) nopy_builtin_module(Py_InitModule4(name, methods, doc, self, api))

#include "Python/bltinmodule.c"
