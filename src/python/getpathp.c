/* PC/getpathp.c and its part of Py_FiniObjects (src/python/pythonrun.c) in the exe: the path of
 * calculate_path, also when that is the PYTHONPATH literal (as in the exe) */
#include "PC/getpathp.c"
#include "pyengine.h"

void _PyPath_FiniSearchPath(void) {
	PY_FINI_STATIC(module_search_path);
}
