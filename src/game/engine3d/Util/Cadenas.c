/*
 * String helpers of the engine3d, original source Util/Cadenas.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"
#include "scrap/material.h"

#define g_ThousandsSep GAME_GLOBAL(char, g_ThousandsSep, 0x431b88)
#define g_IntBuf (GAME_ARRAY(char, g_IntBuf, 0x4a8eb4))	/* the result of Cad_FromInt(NULL, n) */

/* c at pos, the characters from pos up to len (the terminator at len included) one further */
void Cad_InsertChar(char *s, char c, int pos, int len) {
	for (int i = len + 1; i > pos; i--) {
		s[i] = s[i - 1];
	}
	s[pos] = c;
}

char *Cad_Copy(char *dst, const char *src, int size) {
	CHECK(dst && src && size > 0);
	*dst = '\0';
	crt_strncat(dst, src, size - 1);
	return dst;
}

char *Cad_Dup(const char *s) {
	CHECK(s);
	size_t n = strlen(s) + 1;
	return memcpy(Mem_Alloc(n), s, n);
}

int Cad_Thousands(char *s) {
	int len = (int)strlen(s);
	int count = 3;

	for (int i = len; i > 0; i--) {
		if (!crt_isdigit(s[i - 1])) {
			break;
		}
		if (count-- == 0) {
			Cad_InsertChar(s, g_ThousandsSep, i, len++);
			count = 2;
		}
	}
	return len;
}

char *Cad_FromInt(char *buf, int n) {
	if (!buf) {
		buf = g_IntBuf;
	}
	crt_sprintf(buf, "%d", n);
	Cad_Thousands(buf);
	return buf;
}

/* the string members of the engine classes (CCadena of the headers): a Mem_Alloc copy, NULL for none */
void __thiscall Cad_Set(char **self, const char *s) {
	if (*self) {
		*self = Mem_Free(*self);
	}
	if (s) {
		*self = Cad_Dup(s);
	}
}

void __thiscall Cad_Read(char **self, CArchIO *f) {
	char buf[0x100];
	byte isNull;

	CArchIO_ReadStringNull(f, buf, sizeof(buf), &isNull);
	Cad_Set(self, isNull ? NULL : buf);
}

void __thiscall Cad_Write(char **self, CArchIO *f) {
	CArchIO_WriteString(f, *self);
}

void Cad_Printf(char **self, const char *fmt, ...) {
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
	*self = Cad_Dup(buf);
}
