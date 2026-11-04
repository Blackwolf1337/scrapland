/* Objects/stringobject.c and its part of Py_FiniObjects (src/python/pythonrun.c) in the exe: the
 * object of the interned dictionary, not its table */
#include <string.h>

/* string_compare returns what memcmp gives and cmp() shows it to the scripts: the MSVC runtime gives the difference of
 * the first unequal bytes, glibc only the sign (its vector versions give other values, by the address of the
 * strings) */
static int Py_MemCmp(const void *a, const void *b, size_t n) {
	const unsigned char *x = a, *y = b;
	size_t i;

	for (i = 0; i < n; i++) {
		if (x[i] != y[i]) {
			return x[i] - y[i];
		}
	}
	return 0;
}
#define memcmp Py_MemCmp
#include "Objects/stringobject.c"
#undef memcmp
#include "pyengine.h"

void _PyString_FiniInterned(void) {
	PY_FINI_STATIC(interned);
}
