# Standalone build

Goal: a `scrap.exe` built only from the C code of `src/` and real libraries, with no code of the original Scrap.exe.
It is reached in stages, each one runs the difftest.

## Stage 0: wipe check (hook DLL)

`[general] wipe=1` in scrapdecomp.ini (`DIFFTEST_WIPE=1 tools/test/difftest.sh`) fills the rest of every hooked
original with int3 after the hook jump. A jump into the middle of replaced code stops the game, and the log says
`wipe: original code reached at rva ...`.

## Stage 1: loader with the data of the original

`ScrapC.exe` (`src/standalone/main.c`) has the base 0x400000 of the original. `cmake/ScrapC.ld` puts the
uninitialized section `.orig` at rva 0x1000, as large as the original image, and the own code after it (under Wine
the low memory is full of mappings, so the room must be part of the exe). At start it reads the user's Scrap.exe
1.6.1 from game/Bin, copies its sections to their rvas (the rvas of `GAME_DATA` stay valid, no relocation), points
its own resource directory to the original's, fills the import table with GetProcAddress (dinput8 from the system
folder), gives the original its TLS block (the MSVC thread init epoch), installs the hooks and runs the entry point
of the original. Library code (zlib, libpng, libjpeg and the ASE SDK below rva 0x05f6d8; Python 1.5.2 at
0x2d6010..0x313642, the MSVC CRT and D3DX8 above) still runs from the copy.

`DIFFTEST_STANDALONE=1 tools/test/difftest.sh` runs the hooked side as ScrapC.exe. Since 2026-10-28 all three
scenarios match.

`tools/standalone/inventory.py` lists what is still missing:

- code pointers in the data sections (vtables, method tables) that point to game code without a C version: the
  frame of CMotorApp (kept) and the C++ catch blocks of the GOG code in the exception tables (their functions are C)
- the `SD_IMPORT`s of game code: the timers with busy waits

### Stage 1b: the whole game code int3

`[general] wipe=2` (`DIFFTEST_WIPE=2`) fills the whole game and engine code (rva 0x05f6d8..0x2d6010) with int3
before the hooks are written, except the functions of the `keep` table in `src/core/hook.c` (the frame with its
limiter, the device reset with Sleep and the timers with busy waits). Since 2026-10-28
`DIFFTEST_WIPE=2 DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios. The tiny methods
share the stubs of `src/game/Global/Stubs.c`, the Winsock thunks are C (`src/game/Net/Wsock.c`), the catch blocks
of the GOG code are done without C++ exceptions (`src/game/Global/Gog.c`).

What `tools/standalone/uncovered.py` still lists is code that nothing reaches: no call, jump or pointer of the exe
(code or data) and no rva in the C points to it. These are functions the compiler kept after it inlined every call,
the jump thunks of unused imports and pieces of hooked functions behind their hook size (only the original body
jumps there), besides the switch and byte index tables.

`wipe=3` is a probe for this work: the same int3, but each trap logs its rva and `[esp]` (the caller when the trap is
the start of a function) and copies the piece of original code there back, so one run lists every piece of game
code without a C version that the run reaches.

## Stage 2: real libraries

Python 1.5.2 from source (python.org py152.tgz), UCRT for the `crt_*` imports, zlib 1.1.3, libpng 1.0.5, IJG
libjpeg, D3DX8. The `GAME_DATA` addresses of library data (`include/scrap/python.h`: `PyString_Type`, `Py_None`
...) switch to the real symbols. Then the whole `.text` of the original is int3.

## Stage 3: data in C

The data sections move into C: vtables, tables, strings, then the globals with their types. After that the
original exe is no longer read.

## Stage 4: English names

The Spanish names of the original stay until the C is complete, then everything is translated.
