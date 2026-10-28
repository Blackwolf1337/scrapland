/* Objects/listobject.c and its part of Py_FiniObjects (src/python/pythonrun.c), rva 0x2dcec0 in the exe */
#include "Objects/listobject.c"
#include "pyengine.h"

void _PyList_FiniIndexErr(void) {
	PY_FINI_STATIC(indexerr);
}
