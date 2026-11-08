/*
 * Python/ceval.c without the bytecode loop: the calls of C functions, classes, instances and methods of C functions
 * stay (PyEval_CallObjectWithKeywords), as do the thread state, the pending calls and the PyEval_Get* of the current
 * frame (there is none). With PyFunction_Check false here a Python function is no callable ("call of non-function");
 * PyEval_EvalCode is not built (NOPY_DROP), so eval_code2, the exec statement and the opcode helpers drop with it.
 */
#include "Python.h"
#include "compile.h"
#include "pyengine.h"

#define PyEval_EvalCode(args...) nopy_PyEval_EvalCode(args)
NOPY_DROP(PyObject *, PyEval_EvalCode);
#undef PyFunction_Check
#define PyFunction_Check(op) 0

#include "Python/ceval.c"
