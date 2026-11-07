/*
 * Python/ceval.c without the bytecode loop (SCRAP_NO_PYTHON): the calls of C functions, classes, instances and
 * methods of C functions stay (PyEval_CallObjectWithKeywords), as do the thread state, the pending calls and the
 * PyEval_Get* of the current frame (there is none). With PyFunction_Check false here a Python function is no
 * callable ("call of non-function"); PyEval_EvalCode gets another name, so a caller does not link, and
 * --gc-sections drops it with eval_code2 and the opcode helpers.
 */
#define PyEval_EvalCode Py_NoEvalCode

#include "Python.h"

#undef PyFunction_Check
#define PyFunction_Check(op) 0
/* PyEval_GetLocals of the frame that never is */
#define PyFrame_FastToLocals(f) ((void)(f))

#include "Python/ceval.c"
