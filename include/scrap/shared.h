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
 * ScrapC.exe only (docs/standalone.md, stage 3b): a C object defined at an rva of the original's data, in a section
 * .sd.<rva> of its own, which tools/standalone/datagen.py places there; the original's bytes there are left out.
 * The object must hold what the original holds (datagen.py checks it). It is aligned to 4 only (GCC would align
 * larger arrays to 32), so it starts at a multiple of 4, and covers its size rounded up to 4 (the assembler pads PE
 * sections): small neighbours go into one struct. Only in the sources of
 * scrapcode_sa (src/core, src/game, src/lib), whose objects datagen.py reads. The hook DLL has the exe's own data.
 *	const int g_Foo[4] SD_AT(0x2a1230) = { ... };
 */
#ifdef SD_STANDALONE
#define SD_AT(rva) SD_AT_(rva)
#define SD_AT_(rva) __attribute__((section(".sd." #rva), used, aligned(4)))
#endif

void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
