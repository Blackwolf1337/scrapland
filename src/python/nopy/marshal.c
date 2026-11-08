/*
 * Python/marshal.c without code objects: values marshal as before, a code object is "unmarshallable" when written
 * and raises ValueError when read. PyMarshal_ReadObjectFromFile and PyMarshal_ReadObjectFromString, the readers of
 * .pyc files and frozen modules, are not built (NOPY_DROP).
 */
#include "Python.h"
#include "compile.h"
#include "pyengine.h"

#undef PyCode_Check
#define PyCode_Check(op) 0
#define PyCode_New(args...) ((PyCodeObject *)Py_NoInterpreter(PyExc_ValueError, "marshal of code"))
#define PyMarshal_ReadObjectFromFile(args...) nopy_PyMarshal_ReadObjectFromFile(args)
#define PyMarshal_ReadObjectFromString(args...) nopy_PyMarshal_ReadObjectFromString(args)
NOPY_DROP(PyObject *, PyMarshal_ReadObjectFromFile);
NOPY_DROP(PyObject *, PyMarshal_ReadObjectFromString);

#include "Python/marshal.c"
