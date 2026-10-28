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
of the original. Library code (Python 1.5.2 below rva 0x05f6d8, the MSVC CRT and other libraries from 0x2d6010)
still runs from the copy.

`DIFFTEST_STANDALONE=1 tools/test/difftest.sh` runs the hooked side as ScrapC.exe. Since 2026-10-28 all three
scenarios match.

`tools/standalone/inventory.py` lists what is still missing:

- code pointers in the data sections (vtables, method tables) that point to game code without a C version
  (tiny stubs under 5 bytes, never hooked)
- the `SD_IMPORT`s of game code: Winsock thunks, Steam and GOG stubs

The seven functions with Sleep or busy waits are C too, with intended behavior changes (each marked in the code
with a comment): the frame (`CMotorApp_Render`) has no Sleep loop as its frame limiter, `Motor_FrameDue` holds the
next frame back in the PeekMessage loop of `App_WinMain` (no own limiter with vsync); the device reset has no
Sleep(250); `Timer_GetQPFreq`, `Timer_MeasureQP` and `Timer_CountTicks` take QueryPerformanceFrequency instead of
measuring; `Tsc_CpuFreq` measures rdtsc against the performance counter between two calls; `Video_WaitKeysUp`
starts a hold under which the video loop and `Keyboard_Poll` ignore Escape, Space and Enter until they are up or
the time is over. `tools/test/py/sdcall_nosleep.py` checks them (some only in the hooked run).

## Stage 2: real libraries

Python 1.5.2 from source (python.org py152.tgz), UCRT for the `crt_*` imports, zlib 1.1.3, libpng 1.0.5, IJG
libjpeg, D3DX8. The `GAME_DATA` addresses of library data (`include/scrap/python.h`: `PyString_Type`, `Py_None`
...) switch to the real symbols. Then the whole `.text` of the original is int3.

## Stage 3: data in C

The data sections move into C: vtables, tables, strings, then the globals with their types. After that the
original exe is no longer read.

## Stage 4: English names

The Spanish names of the original stay until the C is complete, then everything is translated.
