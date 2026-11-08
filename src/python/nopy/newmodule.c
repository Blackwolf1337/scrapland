/*
 * Modules/newmodule.c without code objects: new.code raises SystemError, the rest stays.
 */
#include "Python.h"
#include "compile.h"
#include "pyengine.h"

#define PyCode_New(args...) ((PyCodeObject *)Py_NoInterpreter(PyExc_SystemError, "new.code"))

#include "Modules/newmodule.c"
