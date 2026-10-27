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

cString *__thiscall cString_ctor(cString *self) {
	*self = NULL;
	return self;
}

/* room for n characters (none for 0) */
cString *__thiscall cString_ctorReserve(cString *self, int n) {
	*self = n ? StrAlloc(n) : NULL;
	return self;
}

/* a followed by b; the lengths only size the buffer, both strings are copied whole */
cString *__thiscall cString_ctorConcat(cString *self, const char *a, int alen, const char *b, int blen) {
	*self = alen + blen ? StrAlloc(alen + blen) : NULL;
	if (a) {
		memcpy(*self, a, strlen(a) + 1);
	}
	if (b) {
		memcpy(*self + alen, b, strlen(b) + 1);
	}
	return self;
}

void __thiscall cString_SetN(cString *self, const char *s, int n) {
	StrFree(self);
	if (n || s) {
		*self = StrAlloc(n);
		if (s) {
			memcpy(*self, s, n);
			(*self)[n] = 0;
		}
	}
}

cString *__thiscall cString_ctorStr(cString *self, const char *s) {
	*self = NULL;
	cString_SetN(self, s, StrLen(s));
	return self;
}

cString *__thiscall cString_ctorCopy(cString *self, const cString *src) {
	*self = NULL;
	cString_SetN(self, *src, StrLen(*src));
	return self;
}

void __thiscall cString_dtor(cString *self) {
	StrFree(self);
}

void __thiscall cString_Set(cString *self, const char *s) {
	cString_SetN(self, s, StrLen(s));
}

void __thiscall cString_Assign(cString *self, const cString *src) {
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
void __thiscall cString_Append(cString *self, const char *s, int n) {
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

cString *__thiscall cString_AppendStr(cString *self, const char *s) {
	cString_Append(self, s, StrLen(s));
	return self;
}

cString *__thiscall cString_AppendC(cString *self, const cString *s) {
	cString_Append(self, *s, StrLen(*s));
	return self;
}

cString *__thiscall cString_AppendChar(cString *self, char c) {
	char s[2] = { c, 0 };
	cString_Append(self, s, 1);
	return self;
}

/* operator+ of cString and char *, the result in *out */
cString *cString_Add(cString *out, const cString *a, const cString *b) {
	return cString_ctorConcat(out, *a, StrLen(*a), *b, StrLen(*b));
}

cString *cString_AddStr(cString *out, const cString *a, const char *b) {
	return cString_ctorConcat(out, *a, StrLen(*a), b, StrLen(b));
}

cString *cString_StrAdd(cString *out, const char *a, const cString *b) {
	return cString_ctorConcat(out, a, StrLen(a), *b, StrLen(*b));
}

/* the first sub in the string and its position counted from 1 */
char *__thiscall cString_Find(const cString *self, const char *sub, int *pos) {
	if (!*self || !sub) {
		return NULL;
	}
	char *p = strstr(*self, sub);
	*pos = (int)(p - *self) + 1;
	return p;
}

/* the first (Left) or last (Right) n characters into *out, through the exe's strncat like the original */
static cString *StrPart(const cString *self, cString *out, int n, int right) {
	int len = StrLen(*self);
	*out = NULL;
	if (n < 0) {
		n = 0;
	} else {
		if (len < n) {
			n = len;
		}
		if (n) {
			*out = StrAlloc(n);
		}
	}
	crt_strncat(*out, right ? *self - n + len : *self, n);
	return out;
}

cString *__thiscall cString_Left(const cString *self, cString *out, int n) {
	return StrPart(self, out, n, 0);
}

cString *__thiscall cString_Right(const cString *self, cString *out, int n) {
	return StrPart(self, out, n, 1);
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

byte cString_StrEq(const char *a, const cString *b) {
	return StrIEq(a, *b);
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

int __thiscall cString_Cmp(const cString *self, const cString *other) {
	return StrCmp(*self, *other);
}

int __thiscall cString_CmpStr(const cString *self, const char *other) {
	return StrCmp(*self, other);
}

int __thiscall cString_ICmp(const cString *self, const cString *other) {
	return StrICmp(*self, *other);
}

int __thiscall cString_ICmpStr(const cString *self, const char *other) {
	return StrICmp(*self, other);
}

int __fastcall cString_Length(const cString *self) {
	return *self ? (int)strlen(*self) : 0;
}
