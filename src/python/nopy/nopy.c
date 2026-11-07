/*
 * The library without the interpreter (SCRAP_NO_PYTHON, stage 7 of the removal of Python, docs/scripts.md): the
 * files of src/python/nopy wrap the vendored ones that keep a part, what would parse, compile or run Python code
 * raises through Py_NoInterpreter instead.
 */
#include "Python.h"
#include "pyengine.h"


void *Py_NoInterpreter(PyObject *exc, const char *what) {
	if (!PyErr_Occurred())
		PyErr_Format(exc, "%.200s: no Python interpreter in this build", what);
	return NULL;
}
