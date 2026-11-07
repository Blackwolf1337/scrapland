/*
 * Python/marshal.c without code objects (SCRAP_NO_PYTHON): values marshal as before, a code object is
 * "unmarshallable" when written and raises ValueError when read.
 */
#include "Python.h"
#include "compile.h"
#include "pyengine.h"

#undef PyCode_Check
#define PyCode_Check(op) 0
#define PyCode_New(args...) ((PyCodeObject *)Py_NoInterpreter(PyExc_ValueError, "marshal of code"))

#include "Python/marshal.c"
