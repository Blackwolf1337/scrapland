/*
 * cString of the engine (Global/): one pointer to a string from operator new[], NULL for none. Fields of engine
 * structs that are a cString are declared as char *.
 */
#ifndef SCRAP_CSTRING_H
#define SCRAP_CSTRING_H

#include "scrap/shared.h"

typedef char *cString;

cString *__thiscall cString_ctor(cString *self);
cString *__thiscall cString_ctorStr(cString *self, const char *s);
cString *__thiscall cString_ctorCopy(cString *self, const cString *src);
void __thiscall cString_dtor(cString *self);
/* n characters of s (s may be NULL with n 0: no string) */
void __thiscall cString_SetN(cString *self, const char *s, int n);
void __thiscall cString_Set(cString *self, const char *s);
void __thiscall cString_Assign(cString *self, const cString *src);
/* NULL fmt clears the string; up to 1023 characters */
void cString_Printf(cString *self, const char *fmt, ...);
void __thiscall cString_Append(cString *self, const char *s, int n);
cString *__thiscall cString_AppendStr(cString *self, const char *s);
cString *__thiscall cString_AppendC(cString *self, const cString *s);
cString *__thiscall cString_AppendChar(cString *self, char c);
/* case-insensitive equality, NULL equal only to NULL */
byte cString_Eq(const cString *a, const cString *b);
byte cString_EqStr(const cString *a, const char *b);
byte cString_StrEq(const char *a, const cString *b);
/* strcmp order, NULL after everything */
int __thiscall cString_Cmp(const cString *self, const cString *other);
int __thiscall cString_CmpStr(const cString *self, const char *other);
int __thiscall cString_ICmp(const cString *self, const cString *other);
int __thiscall cString_ICmpStr(const cString *self, const char *other);
/* strlen, 0 for no string (the copy compiled into Net/, rva 0x1907a0) */
int __fastcall cString_Length(const cString *self);

#define g_StrTmp (GAME_ARRAY(char, g_StrTmp, 0x44b648))	/* 0x100 bytes, the old text in cString_Append */

#endif
