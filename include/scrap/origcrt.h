/*
 * Where the MSVC runtime of Scrap.exe is (rvas): its code from operator delete (0x313634, after the Python) up to
 * the C++ unwind funclets of the game (0x37b3d0, .text$x), followed by the atexit destructors of the game's globals
 * (0x38c720..0x38f097, C in src/game/Global/StaticInit.c and the files of their classes). In it lie the import thunks the linker made for the game's DLL calls (fmod, avifil32, msvfw32,
 * ddraw at 0x3179c6, wsock32 and d3d8 at 0x3761d0), which are not the runtime's. ScrapC.exe runs the C of
 * src/standalone/crt.c and crtinit.c instead (docs/standalone.md, stage 2b).
 */
#ifndef SCRAP_ORIGCRT_H
#define SCRAP_ORIGCRT_H

#define ORIG_CRT_CODE_START 0x313634
#define ORIG_CRT_CODE_END 0x37b3d0

/* the import thunks of the game inside that range */
#define ORIG_CRT_THUNKS \
	{ { 0x3179c6, 0x317b2e }, { 0x3761d0, 0x3761dc } }

#endif
