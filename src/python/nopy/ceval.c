/*
 * Python/ceval.c without the bytecode loop (SCRAP_NO_PYTHON): the calls of C functions, classes, instances and
 * methods of C functions stay (PyEval_CallObjectWithKeywords), as do the thread state, the pending calls and the
 * PyEval_Get* of the current frame (there is none). With PyFunction_Check false here a Python function is no
 * callable ("call of non-function"); PyEval_EvalCode gets another name, so a caller does not link, and
 * --gc-sections drops it with eval_code2 and the opcode helpers.
 */
#define PyEval_EvalCode Py_NoEvalCode

#include "Python.h"
#include "pythonrun.h"
#include "pyengine.h"

#undef PyFunction_Check
#define PyFunction_Check(op) 0
/* the exec statement of the bytecode loop (dropped with it) */
#define PyRun_File(fp, file, start, g, l) ((void)(fp), (void)(start), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, file))
/* PE links keep exec_statement (the x64 unwind data), so its PyRun_String needs no definition elsewhere either */
#define PyRun_String(s, start, g, l) ((void)(s), (void)(start), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, "exec"))

#include "Python/ceval.c"
