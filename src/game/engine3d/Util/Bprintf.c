/*
 * printf into a buffer, original source Util/Bprintf.cpp (include/scrap/e3dutil.h).
 */
#include "scrap/e3dutil.h"

int Bprintf(char *buf, int size, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	int n = VBprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}

int VBprintf(char *buf, int size, const char *fmt, va_list ap) {
	CHECK(buf);
	return Str_VPrintf(buf, size, fmt, ap);
}
