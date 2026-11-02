/*
 * Where the MSVC runtime of Scrap.exe is (rvas): its code from operator delete (0x313634, after the Python) up to
 * the C++ unwind funclets of the game (0x37b3d0, .text$x), followed by the atexit destructors of the game's globals
 * (0x38c720..0x38f097, C in the files of their globals, scrap/staticinit.h). In it lie the import
 * thunks the linker made for the game's DLL calls (fmod, avifil32, msvfw32, ddraw at 0x3179c6..0x317b2e, wsock32 and
 * d3d8 at 0x3761d0..0x3761dc), which are not the runtime's; ScrapC.exe calls the DLLs through its own imports (stage
 * 3c1a). ScrapC.exe runs the C of src/standalone/crt.c and crtinit.c instead (docs/standalone.md, stage 2b).
 */
#ifndef SCRAP_ORIGCRT_H
#define SCRAP_ORIGCRT_H

#define ORIG_CRT_CODE_START 0x313634
#define ORIG_CRT_CODE_END 0x37b3d0

#endif
