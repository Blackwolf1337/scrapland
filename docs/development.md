# Development notes

ScrapC is functionally equivalent C, not the original source. It is not bit-exact with the original: the game uses the platform libc and libm. `ScrapC.exe` is built only from the C code in `src/` and real libraries. It needs the game files (the packs) and SDL3 and FFmpeg, but no code or DLL of the original. The platform layer is SDL3 and the renderer is Vulkan only. Sound goes through its own mixer (`src/game/Sound/Snd.c`, miniaudio for the output, stb_vorbis for the .ogg files).

Until 2026-11-01 the work was done with a hook DLL (the re3 method: a proxy `dinput8.dll` replaced original functions one by one). That state is kept in the tag `hooked-final`. The history of the stages is in [standalone.md](standalone.md).

## Goals

- Get the mod SDK back: headers with structs and offsets, Python script API, `.packed` tools, model and map tools.
- A readable C code base that builds and runs without the original exe.
- Builds for Windows and Linux, 64 bit, from the same sources.

## Rules

- Never commit game files. `game/` holds the innoextract output and is gitignored.
- A pre-commit hook in `tools/git/` rejects files over 5 MB. Enable it with `git config core.hooksPath tools/git`.

## Layout

| Path | Content |
|------|---------|
| `docs/` | see [README.md](README.md) |
| `symbols/` | `functions.tsv`, `globals.tsv`, `comments.tsv`, `rtti_classes.txt` (see [ghidra.md](ghidra.md)) |
| `tools/ghidra/` | `rebuild.sh`, `headless.sh`, export/import and naming scripts (Java), `include/` (parse stand-ins) |
| `third_party/python-1.5.2/` | Python 1.5.2 headers, `COPYRIGHT` |
| `tools/git/pre-commit` | size check hook |
| `tools/codecmp.sh` | compares two builds of the C code |
| `tools/test/` | `prefix.sh`, `wine.sh`, `headless.sh`, `install.sh` (copies `build-win64/ScrapC.exe` and the FFmpeg and SDL3 DLLs to `game/Bin`), `mkgame.sh` (a game and prefix copy for a slot or worktree), `golden.sh` (the script functions of the game through the test channel against a stored output), `litfuzz.sh` (the native literal reader against the stored texts of 1.5.2), `levels.sh` (every playable level in the ASan build), `native.sh` (runs the Linux build headless), `vkcheck.sh` (a run with the Vulkan validation layer, prints the errors of ours) |
| `tools/ffmpeg.sh` | builds FFmpeg (DLLs or shared libraries) for the video player |
| `tools/sdl3.sh` | builds SDL3 for the Windows 64 bit build |
| `tools/mcp/` | the scrap MCP server, headless game tests for an AI assistant, see [tools/mcp/README.md](../tools/mcp/README.md) |
| `shaders/` | GLSL of the renderer: the fixed function (`ff.*`), the present pass (`present.*`), the 30 pixel shaders in `shaders/ps` |
| `include/scrap/` | engine structs and Python headers |
| `src/` | `standalone/` (entry point `main.c`), `core/`, `game/`, `lib/`, `python/` |
| `CMakeLists.txt`, `cmake/` | build of ScrapC.exe, toolchain files for mingw-w64 x86_64 and Linux x86_64 |

## Vtables and calls

The vtables are typed structs with designated initializers next to their classes, so GCC checks every slot. This matters for 64 bit, where float and integer arguments travel in different registers.

Code calls a slot through a named function, `Class_Slot(obj, ...)`, for example `Entity_GetPos(e, &p)` and `AIChar_SetState(ai, AIC_GO_TO, ...)`. These are static inline functions right after each vtable type in its header. They take the object as `void *` and read the table with `VT(type, obj)` (`include/scrap/shared.h`), so call sites need no `base.base` chains or casts. A class whose vtable had only one implementation has no vtable any more, its methods are plain calls (`cBinaryHeap_Push`, `CModel_Free`). AI states and events, the logic states of the character classes, model render effects, trail types and the input actions are enums (`AIC_`, `GENCHAR_`, `MODEL_EFFECT_`, `TRAIL_`, `ACTSET_` and so on).

## Roadmap

- Plain C without rva macros (done except raw offsets). The code has no rva arguments and no `GAME_*` macros, all game functions are plain cdecl, read-only strings are literals, every vtable is a typed struct with designated initializers next to its class, and the script variables are read as `GVar_X.value.i/.f/.s` (types in `include/scrap/gvar.h`). The data (the old `Globals*.c` and `Strings*.c`) lives in the files that use it, as plain extern objects named after their use or text. Still open: raw offsets and pad arrays to named fields. This is partly done, and some large pad arrays are left on purpose.
- Drop the bit-exact emulation of the MSVC CRT and math (done 2026-11-02). The game uses the platform libc and libm. `crt_*` in `include/scrap/crt.h` is a thin wrapper over libc (`src/standalone/crt.c`) for the builds on other platforms. The Direct3D device is created with `D3DCREATE_FPU_PRESERVE` so the x87 libm keeps double precision. The INT_MIN-safe integer division of `src/python/intobject.c` stays because gcc wraps `-(-x/y)`.
- A Vulkan-only renderer replacing Direct3D 8 (done 2026-11-02). Videos play through FFmpeg and the 30 pixel shaders and the fixed function are GLSL. See [vulkan.md](vulkan.md).
- Readable calls and names (done, merged into main on 2026-11-03 and 2026-11-04). Every call through a vtable slot is a `Class_Slot(obj, ...)` function, 30 vtable types with a single implementation are gone (direct calls), unnamed slots got names, and state and action numbers became enums.
- The i686 builds and the difftest are retired. The tag `difftest-final` keeps the last state with them.

## Scripts

The menus, HUD, characters and missions still run from the Python scripts of the packs, and are moved to C module by module. See [scripts.md](scripts.md).
