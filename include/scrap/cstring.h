/*
 * cString of the engine (Global/): one pointer to a string from operator new[], NULL for none. Fields of engine
 * structs that are a cString are declared as char *.
 */
#ifndef SCRAP_CSTRING_H
#define SCRAP_CSTRING_H

#include "scrap/shared.h"

typedef char *cString;

cString *cString_ctor(cString *self);
cString *cString_ctorStr(cString *self, const char *s);
void cString_dtor(cString *self);
void cString_Set(cString *self, const char *s);
void cString_Assign(cString *self, const cString *src);
/* NULL fmt clears the string; up to 1023 characters */
void cString_Printf(cString *self, const char *fmt, ...);
cString *cString_AppendStr(cString *self, const char *s);
/* case-insensitive equality, NULL equal only to NULL */
byte cString_Eq(const cString *a, const cString *b);
byte cString_EqStr(const cString *a, const char *b);
/* strcmp order, NULL after everything */
int cString_Cmp(const cString *self, const cString *other);
int cString_CmpStr(const cString *self, const char *other);
int cString_ICmp(const cString *self, const cString *other);
int cString_ICmpStr(const cString *self, const char *other);
/* strlen, 0 for no string (the copy compiled into Net/, rva 0x1907a0) */
int cString_Length(const cString *self);

#endif
