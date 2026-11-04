/* Objects/dictobject.c and its part of Py_FiniObjects (src/python/pythonrun.c) in the exe */
#include "Objects/dictobject.c"
#include "pyengine.h"

void _PyDict_FiniDummy(void) {
	PY_FINI_STATIC(dummy);
}
