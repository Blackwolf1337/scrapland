# Standalone build

Goal: a `scrap.exe` built only from the C code of `src/` and real libraries, with no code of the original Scrap.exe.
It is reached in stages, each one runs the difftest.

## Stage 0: wipe check (hook DLL)

`[general] wipe=1` in scrapdecomp.ini (`DIFFTEST_WIPE=1 tools/test/difftest.sh`) fills the rest of every hooked
original with int3 after the hook jump. A jump into the middle of replaced code stops the game, and the log says
`wipe: original code reached at rva ...`.

## Stage 1: loader with the data of the original

`scrap.exe` is linked at an image base away from 0x400000. It reads the user's Scrap.exe 1.6.1, maps its headers
and sections at 0x400000 (the rvas of `GAME_DATA` stay valid, no relocation needed), fills the game code with int3,
writes the hook jumps, fills the import table with GetProcAddress and calls the C WinMain. Library code (Python
1.5.2 below rva 0x05f6d8, the MSVC CRT and other libraries from 0x2d6010) still runs from the mapped original.

`tools/standalone/inventory.py` lists what is still missing:

- code pointers in the data sections (vtables, method tables) that point to game code without a C version
  (tiny stubs under 5 bytes, never hooked)
- the `SD_IMPORT`s of game code: Winsock thunks, Steam and GOG stubs, the timers with busy waits

## Stage 2: real libraries

Python 1.5.2 from source (python.org py152.tgz), UCRT for the `crt_*` imports, zlib 1.1.3, libpng 1.0.5, IJG
libjpeg, D3DX8. The `GAME_DATA` addresses of library data (`include/scrap/python.h`: `PyString_Type`, `Py_None`
...) switch to the real symbols. Then the whole `.text` of the original is int3.

## Stage 3: data in C

The data sections move into C: vtables, tables, strings, then the globals with their types. After that the
original exe is no longer read.

## Stage 4: English names

The Spanish names of the original stay until the C is complete, then everything is translated.
