/*
 * Base definitions of the Scrapland Remastered 1.6.1 decompilation (Scrap.exe, GOG build 52159).
 *
 * Target: i686, MSVC 14 compiled game. Pointers, int and long are 4 bytes. Struct layouts follow MSVC rules
 * (natural alignment, 8 byte default packing); every engine struct states its offsets and checks them with
 * SD_OFFSET. Game functions are __cdecl (methods take self as the first argument); __stdcall is left only on the
 * callbacks and imports of Windows, COM and FMOD.
 */
#ifndef SCRAP_SHARED_H
#define SCRAP_SHARED_H

#include <stddef.h>
#include <stdint.h>

static_assert(sizeof(void *) == 4, "Scrap.exe is 32 bit");
static_assert(sizeof(long) == 4, "MSVC long is 4 bytes");

typedef uint8_t byte;

/* the prefix of C symbols in the assembler: an underscore on 32 bit Windows, none in ELF */
#ifdef _WIN32
#define SD_CPFX "_"
#else
#define SD_CPFX ""
/* the calling conventions of the Windows build (the callbacks of Windows, COM and the tests' stdcall table) */
#define __stdcall __attribute__((stdcall))
#define __cdecl __attribute__((cdecl))
#define __fastcall __attribute__((fastcall))
#define __thiscall __attribute__((thiscall))
#endif

#define SD_OFFSET(type, member, off) \
	static_assert(offsetof(type, member) == (off), #type "." #member " is not at " #off)
#define SD_SIZE(type, size) static_assert(sizeof(type) == (size), "sizeof(" #type ") is not " #size)

/* the address of a C symbol as the array sd_<name>, whatever its type (the pointers of the generated data) */
#define SD_SYM(name) extern char sd_##name[] __asm__(SD_CPFX #name)

void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
