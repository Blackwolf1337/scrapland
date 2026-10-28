/* Objects/dictobject.c and its part of Py_FiniObjects (src/python/pythonrun.c), rva 0x2e0720 in the exe */
#include "Objects/dictobject.c"
#include "pyengine.h"

void _PyDict_FiniDummy(void) {
	PY_FINI_STATIC(dummy);
}
