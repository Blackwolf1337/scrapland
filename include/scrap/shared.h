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

/*
 * ScrapC.exe only (docs/standalone.md, stages 3b and 3c2): a C object of the original's data, with the rva where the
 * original has it. In ScrapC.exe it is a plain definition, the linker places it like any other global (stage 3c2);
 * SD_BSS is one without an initializer (the original's bss: the zero tail of .data, .bss_ste). The object must hold
 * what the original holds, which datagen.py --check compares byte for byte. Only in the sources of scrapcode_sa
 * (src/core, src/game, src/lib). The hook DLL has the exe's own data.
 *	const int g_Foo[4] SD_AT(0x2a1230) = { ... };
 *
 * The check build (SD_DATACHECK, the object library scrapdata_check of CMakeLists.txt, which only the check and the
 * generators read) puts each object into a section .sd.<rva> of its own, so the rva and the bytes and relocations of
 * every object can be read from the object files. There it is aligned to 4 only (GCC would align larger arrays to
 * 32), as the room of the stages before 3c2 needed. GCC gives a named section the flags of initialized data ("w");
 * the section name of SD_BSS ends the directive with the flags of gas ("bw") and comments out the ones GCC writes
 * after it.
 */
#ifdef SD_STANDALONE
#ifdef SD_DATACHECK
#define SD_AT(rva) SD_AT_(rva)
#define SD_AT_(rva) __attribute__((section(".sd." #rva), used, aligned(4)))
#define SD_BSS(rva) SD_BSS_(rva)
#define SD_BSS_(rva) __attribute__((section(".sd." #rva ",\"bw\"\n#"), used, aligned(4)))
#else
#define SD_AT(rva) __attribute__((used))
#define SD_BSS(rva) __attribute__((used))
#endif
/* the address of a C symbol as the array sd_<name>, whatever its type (the pointers of the generated data);
 * SD_SYM_AS with the assembler name of a stdcall or fastcall function */
#define SD_SYM(name) extern char sd_##name[] __asm__("_" #name)
#define SD_SYM_AS(name, label) extern char sd_##name[] __asm__(label)
#endif

void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
