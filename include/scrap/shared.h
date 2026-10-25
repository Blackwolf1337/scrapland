/*
 * Base definitions of the Scrapland Remastered 1.6.1 decompilation (Scrap.exe, GOG build 52159).
 *
 * Target: i686, MSVC 14 compiled game. Pointers, int and long are 4 bytes. Struct layouts follow MSVC rules
 * (natural alignment, 8 byte default packing); every engine struct states its offsets and checks them with
 * SD_OFFSET. Calling conventions are written on every function that is not __cdecl.
 */
#ifndef SCRAP_SHARED_H
#define SCRAP_SHARED_H

#include <stddef.h>
#include <stdint.h>

static_assert(sizeof(void *) == 4, "Scrap.exe is 32 bit");
static_assert(sizeof(long) == 4, "MSVC long is 4 bytes");

typedef uint8_t byte;

#define SD_OFFSET(type, member, off) \
	static_assert(offsetof(type, member) == (off), #type "." #member " is not at " #off)
#define SD_SIZE(type, size) static_assert(sizeof(type) == (size), "sizeof(" #type ") is not " #size)

void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
