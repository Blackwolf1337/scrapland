/*
 * Python/bltinmodule.c without the interpreter (SCRAP_NO_PYTHON): eval, execfile, compile and input raise
 * SystemError; __import__ and reload find only the built-in and the loaded modules (src/python/import.c refuses
 * files).
 */
#include "Python.h"
#include "compile.h"
#include "eval.h"
#include "pyengine.h"

#undef PyCode_Check
#define PyCode_Check(op) 0
#define PyEval_EvalCode(co, g, l) ((void)(co), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, "eval"))
#define PyRun_String(str, start, g, l) ((void)(str), (void)(start), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, "eval and input"))
#define PyRun_File(fp, file, start, g, l) ((void)(fp), (void)(start), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, file))
#define Py_CompileString(str, file, start) ((void)(str), (void)(file), (void)(start), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, "compile"))

#include "Python/bltinmodule.c"
