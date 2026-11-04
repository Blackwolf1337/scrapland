/* Objects/stringobject.c and its part of Py_FiniObjects (src/python/pythonrun.c) in the exe: the
 * object of the interned dictionary, not its table */
#include "Objects/stringobject.c"
#include "pyengine.h"

void _PyString_FiniInterned(void) {
	PY_FINI_STATIC(interned);
}
