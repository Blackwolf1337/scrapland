# Platforms and 64 bit

ScrapC.exe builds for Windows and Linux, 64 bit, from the same sources. CMake refuses a 32-bit build (`CMAKE_SIZEOF_VOID_P` 4 is a fatal error). This file describes the platform layer, the rules the 64-bit port taught and how the builds are tested.

## Builds

| Folder | Toolchain file | Target |
|--------|----------------|--------|
| `build-win64` | `cmake/mingw-x86_64.cmake` | Windows x86_64 |
| `build-linux64` | `cmake/linux-x86_64.cmake` | Linux x86_64 |
| `build-linux64-asan` | `cmake/linux-x86_64.cmake` | Linux x86_64 with `CMAKE_C_FLAGS=-fsanitize=address -fno-omit-frame-pointer`, for the level test |

The i686 builds and the i686 difftest are retired. The tag `difftest-final` keeps the last state with them. The Linux binary is position independent again, the fixed load address 0x400000 was only for the test scripts.

## Platform layer

`include/scrap/platform.h` and `src/game/App/Platform.c` sit on SDL3 3.4.18. `tools/sdl3.sh [build dir]` builds the x86_64 SDL3.dll for mingw (default `build-win64/sdl3`), Linux uses the system SDL3. The layer provides:

- The window (`SDL_WINDOW_VULKAN`) and the event pump that replaced the Win32 message loop, with focus, resize and minimize.
- Text input, converted from UTF-8 to CP1252.
- The keyboard. One SDL scancode table gives the VK codes and the DirectInput codes of the game.
- The relative mouse.
- Gamepads, presented as `XINPUT_STATE`.
- Timers. `Plat_Counter` is a 10 MHz counter, `Plat_Ticks` counts milliseconds.
- Message boxes through `SDL_ShowSimpleMessageBox`. With `SCRAP_NO_MSGBOX=1` they are logged instead, for headless tests.
- The Vulkan instance extensions and the surface, from SDL.

Paths come from the SDL base path and pref path. On Linux the config and the saves go to the SDL user data folder (`XDG_DATA_HOME`). The install language is read from a config file instead of the registry.

Sound is an own mixer, `src/game/Sound/Snd.c`, with miniaudio for the output and stb_vorbis for decoding. `SCRAP_SND_NULL=1` picks the null device. `SCRAP_SND_WAV=<file>` also writes the mix into that file as a 32-bit float stereo WAV at 44100 Hz, for tests (it works with the null device too, so a headless run records what the game plays).

Networking uses BSD sockets with a thin Winsock shim in `src/game/Net/Sock.c`.

The `crt_*` layer (`include/scrap/crt.h`) is split in three files. `src/standalone/crt.c` is shared and holds an MSVC-compatible `rand`, so the random sequences of the game match the original. `crtwin.c` and `crtposix.c` hold the rest.

## Rules for 64 bit

These are for anyone who changes structs or adds code.

The layout checks `SD_OFFSET`, `SD_SIZE` and `SD_32BIT` (1525 checks of the 32-bit layout) are gone. The offset comments in the structs stay as a reference to the original, they are not checked.

Never use a literal size or offset for something that holds a pointer. Use `sizeof` and `offsetof`. The port found these bugs:

- object sizes in `*_SIZE` macros and allocation tables
- `Table_*` helpers named by element size (`Table_AllocN`, `Table_ResizeN` and `Table_GetN` take the size now)
- byte arrays sized for 32 bit that held structs with pointers (they are typed fields now)
- mirror structs with 32-bit padding
- `+ 0x..` offsets into objects, also through `uintptr_t`
- data initializers like `sd_X + 16`
- name and offset tables of script variables (they use `offsetof` now)

Never keep a pointer in an int. Use `intptr_t` or the pointer type. A typical symptom in the debugger is a pointer whose upper half is 1, from an int written at a 32-bit offset.

The vtables are typed `*Vtbl` structs with designated initializers, so the compiler checks every slot. This matters on x86_64: float arguments travel in other registers than integers, so a slot implemented with `int` where the caller passes `float` would shift all later arguments. On i386 every argument is a stack word, so the same mistake went unnoticed. GCC now reports such a mismatch.

Calls go through wrapper functions named `Class_Slot(obj, ...)`, static inline and placed after each vtable type in its header. A wrapper reads the table with `VT(type, obj)` and calls the typed slot, so the slot signature is checked at every call as well. Classes whose vtable had only one implementation have no vtable and are called directly.

`long` differs between the 64-bit targets. Linux x86_64 is LP64 (`long` is 64 bits), Windows x64 is LLP64 (`long` is 32 bits). The Python 1.5.2 config follows that (`src/python/config.h`): Python ints are 64 bits on Linux x86_64 and 32 bits on Windows x64. The pointer-to-long casts of Python itself are only in repr strings and hashes.

Save games are name/value strings plus a count, so they do not depend on the pointer size.

## Testing on Linux

`tools/test/native.sh` runs the Linux build headless in gamescope. `SCRAP_BUILD` selects the build folder (default `build-linux64`), `SCRAP_GAME_DIR` another game folder.

## Testing 64 bit

`tools/test/levels.sh` runs every playable level (29, Nano has no map in the packs) in `build-linux64-asan` headless for `LEVELS_TIME` seconds (default 120) and prints one line per level. A level fails on an ASan error, a fatal error (the message box text in `scrapdecomp.log`) or a Python traceback. The exit code is 0 when all pass. Levels can be given as arguments. The levels run in parallel, `LEVELS_JOBS` at the same time (default 4). Worker n > 0 uses its own game folder `<game>-<n>` (`native.sh` makes it when it is missing). The log of a level is in the game folder of its worker, and the lines still come in the order of the levels. All 29 levels with 60 s each and 6 jobs take about 6 minutes instead of about 45.

`tools/test/golden.sh` runs `tools/test/golden/sdtest.py` (the script functions of the game and the Python interpreter in `sdpy.py`, 9961 checks, in the main menu) on the Linux build and compares the output with `tools/test/golden/sdtest.out`. `GOLDEN_UPDATE=1` stores a new expected output. The stored output was checked against the frozen i686 reference before it was stored. It equals it except where 64 bit differs on purpose: Python ints are 64 bits on Linux x86_64 (overflows, `%x` of negative numbers, literal sizes, string hashes), the text of NaN, and there is no SDTest module.

A failed `CHECK` of the game traps, which shows as an `UNKNOWN SIGNAL` at address 0x1. To debug, run gdb only as a child of `tools/test/headless.sh`, because ptrace of other processes is blocked in the container. A hardware watchpoint on a corrupted field finds the writer.

The scrap MCP server (`tools/mcp`) launches every build with the `exe` argument: `native64` (default), `native64asan`, `c64` (`build-win64` under Wine) and `ref` (the frozen reference exe, for visual comparisons). Its slots are in `build-mcp/`.

## State on 2026-11-02

All 30 levels run ASan clean on Linux x86_64. On Windows x64 under Wine they stay alive without new script errors. Video and multiplayer work.

Not tested: a full light map compile. It only runs when the light map cache of a level is missing.
