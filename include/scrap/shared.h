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

/* the address of a C symbol as the array sd_<name>, whatever its type (the pointers of the generated data);
 * SD_SYM_AS with the assembler name of a stdcall or fastcall function */
#define SD_SYM(name) extern char sd_##name[] __asm__("_" #name)
#define SD_SYM_AS(name, label) extern char sd_##name[] __asm__(label)

void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
