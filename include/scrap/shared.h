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

typedef uint8_t byte;

/* the layout checks hold the 32 bit layout of the original; a 64 bit build has its own (pointers of 8 bytes) */
#if UINTPTR_MAX == 0xffffffffu
#define SD_32BIT 1
#else
#define SD_32BIT 0
#endif

/* the prefix of C symbols in the assembler: an underscore on 32 bit Windows, none on x64 Windows and in ELF */
#if defined(_WIN32) && defined(__i386__)
#define SD_CPFX "_"
#elif defined(_WIN32)
#define SD_CPFX ""
#else
#define SD_CPFX ""
/* the calling conventions of the Windows build (the callbacks of Windows, COM and the tests' stdcall table); x86_64
 * has one convention */
#ifdef __i386__
#define __stdcall __attribute__((stdcall))
#define __cdecl __attribute__((cdecl))
#define __fastcall __attribute__((fastcall))
#define __thiscall __attribute__((thiscall))
#else
#define __stdcall
#define __cdecl
#define __fastcall
#define __thiscall
#endif
#endif

#define SD_OFFSET(type, member, off) \
	static_assert(!SD_32BIT || offsetof(type, member) == (off), #type "." #member " is not at " #off)
#define SD_SIZE(type, size) static_assert(!SD_32BIT || sizeof(type) == (size), "sizeof(" #type ") is not " #size)


void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
