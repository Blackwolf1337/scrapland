/*
 * String helpers of the engine3d, original source Util/Cadenas.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"
#include "scrap/material.h"

char *String_Copy(char *dst, const char *src, int size) {
	CHECK(dst && src && size > 0);
	*dst = '\0';
	crt_strncat(dst, src, size - 1);
	return dst;
}

char *String_Dup(const char *s) {
	CHECK(s);
	size_t n = strlen(s) + 1;
	return memcpy(Mem_Alloc(n), s, n);
}

/* the string members of the engine classes (CCadena of the headers): a Mem_Alloc copy, NULL for none */
void String_Set(char **self, const char *s) {
	if (*self) {
		*self = Mem_Free(*self);
	}
	if (s) {
		*self = String_Dup(s);
	}
}

void String_Read(char **self, CFileIO *f) {
	char buf[0x100];
	byte isNull;

	CFileIO_ReadStringNull(f, buf, sizeof(buf), &isNull);
	String_Set(self, isNull ? NULL : buf);
}

void String_Write(char **self, CFileIO *f) {
	CFileIO_WriteString(f, *self);
}

void String_Printf(char **self, const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	if (!fmt) {
		if (*self) {
			*self = Mem_Free(*self);
		}
		return;
	}
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (*self) {
		*self = Mem_Free(*self);
	}
	*self = String_Dup(buf);
}
