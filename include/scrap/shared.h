/*
 * Base definitions of ScrapC, the C version of Scrapland Remastered 1.6.1 (Scrap.exe, GOG build 52159).
 *
 * It builds for 64 bit Linux and Windows. The offsets in the comments of the structs are the ones of the original
 * (i686, 4-byte pointers), kept as a reference. Methods take self as their first argument; __stdcall is left only
 * on the callbacks of Windows.
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

void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
