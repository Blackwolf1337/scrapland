/* Objects/listobject.c and its part of Py_FiniObjects (src/python/pythonrun.c) in the exe */
#include "Objects/listobject.c"
#include "pyengine.h"

void _PyList_FiniIndexErr(void) {
	PY_FINI_STATIC(indexerr);
}
