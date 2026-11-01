/*
 * cString (include/scrap/cstring.h). The original source file is not known (Global/). Strings come from
 * operator new[]; the empty string is a real allocation, only NULL means none.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/cstring.h"
#include "scrap/game.h"

void *operator_new_array(uint32_t size) {
	return operator_new(size);
}

void operator_delete_array(void *p) {
	operator_delete(p);
}

static int StrLen(const char *s) {
	return s ? (int)strlen(s) : 0;
}

/* room for n characters, empty */
static char *StrAlloc(int n) {
	char *s = operator_new_array(n + 1);
	*s = 0;
	return s;
}

static void StrFree(cString *self) {
	if (*self) {
		operator_delete_array(*self);
		*self = NULL;
	}
}

cString *cString_ctor(cString *self) {
	*self = NULL;
	return self;
}

/* a followed by b; the lengths only size the buffer, both strings are copied whole */
cString *cString_ctorConcat(cString *self, const char *a, int alen, const char *b, int blen) {
	*self = alen + blen ? StrAlloc(alen + blen) : NULL;
	if (a) {
		memcpy(*self, a, strlen(a) + 1);
	}
	if (b) {
		memcpy(*self + alen, b, strlen(b) + 1);
	}
	return self;
}

void cString_SetN(cString *self, const char *s, int n) {
	StrFree(self);
	if (n || s) {
		*self = StrAlloc(n);
		if (s) {
			memcpy(*self, s, n);
			(*self)[n] = 0;
		}
	}
}

cString *cString_ctorStr(cString *self, const char *s) {
	*self = NULL;
	cString_SetN(self, s, StrLen(s));
	return self;
}

void cString_dtor(cString *self) {
	StrFree(self);
}

void cString_Set(cString *self, const char *s) {
	cString_SetN(self, s, StrLen(s));
}

void cString_Assign(cString *self, const cString *src) {
	cString_SetN(self, *src, StrLen(*src));
}

void cString_Printf(cString *self, const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	if (!fmt) {
		StrFree(self);
		return;
	}
	va_start(ap, fmt);
	Str_VPrintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	cString_SetN(self, buf, (int)strlen(buf));
}

/* n characters of s at the end; a string of more than 255 characters goes through a concatenated copy */
void cString_Append(cString *self, const char *s, int n) {
	if (n <= 0) {
		return;
	}
	int len = StrLen(*self);
	if (*self && len > 0xff) {
		cString tmp;
		cString_ctorConcat(&tmp, *self, len, s, n);
		cString_SetN(self, tmp, StrLen(tmp));
		if (tmp) {
			operator_delete_array(tmp);
		}
		return;
	}
	const char *old = *self ? *self : "";
	memcpy(g_StrTmp, old, len + 1);
	StrFree(self);
	if (n + len) {
		*self = StrAlloc(n + len);
	}
	memcpy(*self, g_StrTmp, len + 1);
	memcpy(*self + len, s, n);
	(*self)[len + n] = 0;
}

cString *cString_AppendStr(cString *self, const char *s) {
	cString_Append(self, s, StrLen(s));
	return self;
}

static byte StrIEq(const char *a, const char *b) {
	if (a == b) {
		return 1;
	}
	if (!a || !b) {
		return 0;
	}
	return crt_stricmp(a, b) == 0;
}

byte cString_Eq(const cString *a, const cString *b) {
	return StrIEq(*a, *b);
}

byte cString_EqStr(const cString *a, const char *b) {
	return StrIEq(*a, b);
}

static int StrCmp(const char *a, const char *b) {
	if (a == b) {
		return 0;
	}
	if (!a) {
		return 1;
	}
	if (!b) {
		return -1;
	}
	for (; *a == *b; a++, b++) {
		if (!*a) {
			return 0;
		}
	}
	return (byte)*a < (byte)*b ? -1 : 1;
}

static int StrICmp(const char *a, const char *b) {
	if (a == b) {
		return 0;
	}
	if (!a) {
		return 1;
	}
	if (!b) {
		return -1;
	}
	return crt_stricmp(a, b);
}

int cString_Cmp(const cString *self, const cString *other) {
	return StrCmp(*self, *other);
}

int cString_CmpStr(const cString *self, const char *other) {
	return StrCmp(*self, other);
}

int cString_ICmp(const cString *self, const cString *other) {
	return StrICmp(*self, *other);
}

int cString_ICmpStr(const cString *self, const char *other) {
	return StrICmp(*self, other);
}

int cString_Length(const cString *self) {
	return *self ? (int)strlen(*self) : 0;
}
