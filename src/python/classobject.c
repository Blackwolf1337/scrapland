/*
 * Objects/classobject.c and its part of Py_FiniObjects (src/python/pythonrun.c), rva 0x308ed0 in the exe: the
 * interned names of the special methods. The exe frees 21; the ones in function statics (__init__, __del__,
 * __repr__, ...) can not be reached from outside the vendored file and stay in the pool.
 */
#include "Objects/classobject.c"
#include "pyengine.h"

void _PyClass_FiniNames(void) {
	PY_FINI_STATIC(getattrstr);
	PY_FINI_STATIC(setattrstr);
	PY_FINI_STATIC(delattrstr);
	PY_FINI_STATIC(getitemstr);
	PY_FINI_STATIC(setitemstr);
	PY_FINI_STATIC(delitemstr);
	PY_FINI_STATIC(lenstr);
	PY_FINI_STATIC(coerce_obj);
}
