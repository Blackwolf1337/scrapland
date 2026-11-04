/*
 * Base definitions of ScrapC, the C version of Scrapland Remastered 1.6.1 (Scrap.exe, GOG build 52159).
 *
 * It builds for 64 bit Linux and Windows. Methods take self as their first argument; __stdcall is left only on the
 * callbacks of Windows.
 */
#ifndef SCRAP_SHARED_H
#define SCRAP_SHARED_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t byte;

/* the callbacks of Windows keep __stdcall (mingw defines it); elsewhere it means nothing */
#ifndef _WIN32
#define __stdcall
#endif

/*
 * The vtable of an object. Every class keeps the pointer to its vtable as its first member, so a pointer to the
 * object is a pointer to that pointer. The Class_Method(obj, ...) functions next to each vtable type call through it.
 */
#define VT(type, obj) (*(const type *const *)(obj))

void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
