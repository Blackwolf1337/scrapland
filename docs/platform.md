# Platforms and 64 bit

ScrapC.exe builds for Windows and Linux, 32 and 64 bit, from the same sources. This file describes the platform layer, the rules the 64-bit port taught and how the builds are tested.

## Builds

| Folder | Toolchain file | Target |
|--------|----------------|--------|
| `build` | `cmake/mingw-i686.cmake` | Windows i686 |
| `build-win64` | `cmake/mingw-x86_64.cmake` | Windows x86_64 |
| `build-linux` | `cmake/linux-i686.cmake` | Linux i686 |
| `build-linux64` | `cmake/linux-x86_64.cmake` | Linux x86_64 |

The 64-bit builds are the ones to play. The i686 builds stay because the difftest needs them. `SDTest.Call` passes all arguments as 32-bit words, and only the i386 calling conventions take that. On the 64-bit builds `SDTest.Call` and `SDTest.D3DLog` raise an error.

## Platform layer

`include/scrap/platform.h` and `src/game/App/Platform.c` sit on SDL3 3.4.18. `tools/sdl3.sh` builds SDL3.dll for mingw, Linux uses the system SDL3. The layer provides:

- The window (`SDL_WINDOW_VULKAN`) and the event pump that replaced the Win32 message loop, with focus, resize and minimize.
- Text input, converted from UTF-8 to CP1252.
- The keyboard. One SDL scancode table gives the VK codes and the DirectInput codes of the game.
- The relative mouse.
- Gamepads, presented as `XINPUT_STATE`.
- Timers. `Plat_Counter` is a 10 MHz counter, `Plat_Ticks` counts milliseconds.
- Message boxes through `SDL_ShowSimpleMessageBox`. With `SCRAP_NO_MSGBOX=1` they are logged instead, for headless tests.
- The Vulkan instance extensions and the surface, from SDL.

Paths come from the SDL base path and pref path. On Linux the config and the saves go to the SDL user data folder (`XDG_DATA_HOME`). The install language is read from a config file instead of the registry.

Sound is an own mixer, `src/game/Sound/Snd.c`, with miniaudio for the output and stb_vorbis for decoding. `SCRAP_SND_NULL=1` picks the null device.

Networking uses BSD sockets with a thin Winsock shim in `src/game/Net/Sock.c`.

The `crt_*` layer (`include/scrap/crt.h`) is split in three files. `src/standalone/crt.c` is shared and holds an MSVC-compatible `rand`, so the random sequences of the game match the original. `crtwin.c` and `crtposix.c` hold the rest.

## Rules for 64 bit

These are for anyone who changes structs or adds code.

`SD_OFFSET` and `SD_SIZE` (`include/scrap/shared.h`) check the layout of the original only when `SD_32BIT` is 1. On 64 bit they are skipped. Every 64-bit fix keeps the 32-bit layout, so the difftest stays valid.

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

`long` differs between the 64-bit targets. Linux x86_64 is LP64 (`long` is 64 bits), Windows x64 is LLP64 (`long` is 32 bits). The Python 1.5.2 config follows that (`src/python/config.h`): Python ints are 64 bits on Linux x86_64 and 32 bits on Windows x64. The pointer-to-long casts of Python itself are only in repr strings and hashes.

The asm name prefix `SD_CPFX` is an underscore only on 32-bit Windows.

Save games are name/value strings plus a count, so they work between 32 and 64 bit. An i686 server and an x86_64 client play together.

## Testing on Linux

`tools/test/native.sh` runs the Linux build headless in gamescope. `SCRAP_BUILD=build-linux64` selects the 64-bit build, `SCRAP_GAME_DIR` another game folder.

`DIFFTEST_NATIVE=1 tools/test/difftest.sh` compares the Linux i686 build with the Wine reference. It accepts only the checks listed in `tools/test/native-accepted/<scenario>.txt`.

## Testing 64 bit

The 64-bit builds have no difftest. They are checked with AddressSanitizer runs, one per level. The build folder is `build-linux64-asan`, configured with `CMAKE_C_FLAGS=-fsanitize=address -fno-omit-frame-pointer`. Run with `ASAN_OPTIONS=detect_leaks=0` and `-imap:Levels/<name>`.

A failed `CHECK` of the game traps, which shows as an `UNKNOWN SIGNAL` at address 0x1. To debug, run gdb only as a child of `tools/test/headless.sh`, because ptrace of other processes is blocked in the container. A hardware watchpoint on a corrupted field finds the writer.

The scrap MCP server (`tools/mcp`) launches every build with the `exe` argument: `c` (Windows i686), `c64` (Windows x64), `native` (Linux i686), `native64` and `native64asan`.

## State on 2026-11-02

All 30 levels run ASan clean on Linux x86_64. On Windows x64 under Wine they stay alive without new script errors. Video and multiplayer work.

Not tested: a full light map compile. It only runs when the light map cache of a level is missing.
