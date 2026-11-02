# scrapland

Recreation of Scrapland Remastered 1.6.1 (GOG build 52159, `Bin/Scrap.exe`) as C. The code is functionally equivalent C, not the original source. It is not bit-exact with the original: the game uses the platform libc and libm.

`ScrapC.exe` is a standalone exe built only from the C code in `src/` and real libraries. It needs the game files (the packs) and the FFmpeg DLLs of `tools/ffmpeg.sh`, but no code or DLL of the original. Sound goes through its own mixer (`src/game/Sound/Snd.c`, miniaudio for the output, stb_vorbis for the .ogg files) in place of FMOD 3.

Until 2026-11-01 the work was done with a hook DLL (the re3 method: a proxy `dinput8.dll` replaced original functions one by one). That state is kept in the tag `hooked-final`. The history of the stages is in `docs/standalone.md`.

## Goals

- Get the mod SDK back: headers with structs and offsets, Python script API, `.packed` tools, model and map tools.
- A readable C code base that builds and runs without the original exe.
- Later: a Linux port, see the roadmap.

## Rules

- The repo is private.
- Never commit game files. `game/` holds the innoextract output and is gitignored.
- A pre-commit hook in `tools/git/` rejects files over 5 MB. Enable it with `git config core.hooksPath tools/git`.

## Layout

| Path | Content |
|------|---------|
| `docs/` | `binary.md`, triage report of Scrap.exe; `ghidra.md`, Ghidra database and symbols; `packed.md`, the .packed archives; `standalone.md`, history of the stages to the standalone exe; `rename.md`, the English names of stage 4; `vulkan.md`, the Vulkan renderer |
| `symbols/` | `functions.tsv`, `globals.tsv`, `comments.tsv`, `rtti_classes.txt` (see `docs/ghidra.md`) |
| `tools/ghidra/` | `rebuild.sh`, `headless.sh`, export/import and naming scripts (Java), `include/` (parse stand-ins) |
| `third_party/python-1.5.2/` | Python 1.5.2 headers, `COPYRIGHT` |
| `tools/git/pre-commit` | size check hook |
| `tools/codecmp.sh` | compares two builds of the C code |
| `tools/test/` | `prefix.sh`, `wine.sh`, `headless.sh`, `install.sh` (copies `build/ScrapC.exe` and the FFmpeg DLLs to `game/Bin`), `mkgame.sh` (a game and prefix copy for a slot or worktree), `difftest.sh` (current ScrapC.exe against the reference ScrapC.exe, scenarios sdtest, sdcall, sdlevel), `vkcheck.sh` (a run with the Vulkan validation layer, prints the errors of ours) |
| `tools/ffmpeg.sh` | builds the FFmpeg DLLs and import libraries of the video player |
| `tools/mcp/` | the scrap MCP server, headless game tests for an AI assistant, see [tools/mcp/README.md](tools/mcp/README.md) |
| `shaders/` | GLSL of the renderer: the fixed function (`ff.*`), the present pass (`present.*`), the 30 pixel shaders in `shaders/ps` |
| `include/scrap/` | engine structs and Python headers |
| `src/` | `standalone/` (entry point `main.c`), `core/`, `game/`, `lib/`, `python/`; `core/testsyms.inc`, `testaddrs.inc` and `testfuncs.inc` keep the rvas of the original as test data only |
| `CMakeLists.txt`, `cmake/` | build of ScrapC.exe, mingw-w64 i686 toolchain file |

## Setup

```
innoextract -g -d game/ 'setup_scrapland_remastered_1.6.1_(52159).exe'
tools/test/prefix.sh                          # creates prefix/ (own Wine prefix, audio off)
```

## Build and run

The build needs `glslangValidator` (Arch package `glslang`) for the shaders and an FFmpeg built once with `tools/ffmpeg.sh`. cmake stops without it. Running needs a Vulkan 1.3 driver (under Wine: winevulkan).

```
tools/ffmpeg.sh                               # once: FFmpeg into build/ffmpeg/install
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake && cmake --build build
tools/test/install.sh                         # copies build/ScrapC.exe to game/Bin
tools/test/wine.sh ScrapC.exe -fullscreen:0   # hidden (gamescope headless) and muted
```

`install.sh` also copies the four FFmpeg DLLs (`avcodec-61`, `avformat-61`, `avutil-59`, `swscale-8`) to `game/Bin`. A game install outside the repo needs them next to `ScrapC.exe` too.

`wine.sh` runs for `HEADLESS_TIMEOUT` seconds (default 300). Set `WINE_LOG=file` to keep the output.

Environment variables of the renderer (see `docs/vulkan.md`):

- `SCRAP_VK_DEVICE`: the GPU, as an index or as `vendor:device` in hex. `wine.sh` defaults to `1002:164e`.
- `SCRAP_VK_VALIDATION=1`: the Vulkan validation layer. `wine.sh` turns it on for the game only.
- `SCRAP_VK_TRACE=<seconds>`: the render calls of the first frame that starts that many seconds after the device is created go into `scrapdecomp.log`.
- `SCRAP_RD_DUMP=<dir>` with `SCRAP_RD_DUMP_EVERY=<n>`: every nth presented frame (default 100) as a BMP in `<dir>`.

## Test

The difftest compares the current ScrapC.exe with a frozen reference ScrapC.exe. The reference is built from the tag `ref-noexact` and put into `build-ref-noexact/`, which `DIFFTEST_REF` defaults to. The old `build-ref/` (tag `hooked-final`) is only for comparing with the state of the hook era. The script copies the reference to `ScrapRef.exe` in each slot.

```
tools/test/difftest.sh
```

The scenarios (sdtest, sdcall, sdlevel) run at the same time, each in a game of its own in `build/difftest/slot-<scenario>`. The outputs of the reference are cached by the hashes of the reference exe and the test scripts, so after the first run only the current build runs. `DIFFTEST_NOCACHE=1` forces a run of the reference. A full run takes about 40 seconds.

The current build renders with Vulkan, the reference with Direct3D 8 through Wine. The differences that follow from that are masked in `tools/test/py/sdlib.py`: `rdcall` (the calls on the device objects), the pixel shader handles in the D3D logs, and the driver name in `R_LastDevice`.

Against the old reference these differences are accepted: the text of NaN, "1d5" is no longer read as a number, `%.0f` rounds half to even, trig of huge arguments, and the float hash.

## Roadmap

- Plain C without rva macros (partly done). Done: the code has no rva arguments (the `GAME_*` macros take names only), all game functions are plain cdecl, read-only strings are literals, many globals are plain extern declarations, and the script variables are read as `GVar_X.value.i/.f/.s` (types in `include/scrap/gvar.h`). Open: moving data into its owner files, raw offsets to named fields, typed vtables.
- Drop the bit-exact emulation of the MSVC CRT and math (done 2026-11-02). The game uses the platform libc and libm. `crt_*` in `include/scrap/crt.h` is a thin wrapper over libc (`src/standalone/crt.c`) until the Linux port. The Direct3D device is created with `D3DCREATE_FPU_PRESERVE` so the x87 libm keeps double precision. The INT_MIN-safe integer division of `src/python/intobject.c` stays because gcc wraps `-(-x/y)`.
- A Vulkan-only renderer replacing Direct3D 8 (done 2026-11-02). Videos play through FFmpeg and the 30 pixel shaders and the fixed function are GLSL. See `docs/vulkan.md`.
- A Linux port with SDL3.

## Prior work

- ReScrap ScrapHacks: https://gitdab.com/ReScrap/ScrapHacks (`.packed` format, Kaitai specs, notes for the 2004 exe)
- ReScrap ScrapModLoader: https://gitdab.com/ReScrap/ScrapModLoader
- ReScrap ScrapMods: https://gitdab.com/ReScrap/ScrapMods
