/*
 * Modules/newmodule.c without code objects (SCRAP_NO_PYTHON): new.code raises SystemError, the rest stays.
 */
#include "Python.h"
#include "compile.h"
#include "pyengine.h"

#define PyCode_New(args...) ((PyCodeObject *)Py_NoInterpreter(PyExc_SystemError, "new.code"))

#include "Modules/newmodule.c"
