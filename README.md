# scrapland

Recreation of Scrapland Remastered 1.6.1 (GOG build 52159, `Bin/Scrap.exe`) as C. The code is functionally equivalent C, not the original source. It is not bit-exact with the original: the game uses the platform libc and libm.

`ScrapC.exe` is a standalone exe built only from the C code in `src/` and real libraries. It needs the game files (packs, `fmod.dll`) in `game/Bin`, but no code of the original exe.

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
| `docs/` | `binary.md`, triage report of Scrap.exe; `ghidra.md`, Ghidra database and symbols; `packed.md`, the .packed archives; `standalone.md`, history of the stages to the standalone exe; `rename.md`, the English names of stage 4 |
| `symbols/` | `functions.tsv`, `globals.tsv`, `comments.tsv`, `rtti_classes.txt` (see `docs/ghidra.md`) |
| `tools/ghidra/` | `rebuild.sh`, `headless.sh`, export/import and naming scripts (Java), `include/` (parse stand-ins) |
| `third_party/python-1.5.2/` | Python 1.5.2 headers, `COPYRIGHT` |
| `tools/git/pre-commit` | size check hook |
| `tools/codecmp.sh` | compares two builds of the C code |
| `tools/test/` | `prefix.sh`, `wine.sh`, `headless.sh`, `install.sh` (copies `build/ScrapC.exe` to `game/Bin`), `difftest.sh` (current ScrapC.exe against the reference ScrapC.exe, scenarios sdtest, sdcall, sdlevel) |
| `include/scrap/` | engine structs and Python headers |
| `src/` | `standalone/` (entry point `main.c`), `core/`, `game/`, `lib/`, `python/`; `core/testsyms.inc`, `testaddrs.inc` and `testfuncs.inc` keep the rvas of the original as test data only |
| `CMakeLists.txt`, `cmake/` | build of ScrapC.exe, mingw-w64 i686 toolchain file |

## Setup

```
innoextract -g -d game/ 'setup_scrapland_remastered_1.6.1_(52159).exe'
tools/test/prefix.sh                          # creates prefix/ (own Wine prefix, audio off)
```

## Build and run

```
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake && cmake --build build
tools/test/install.sh                         # copies build/ScrapC.exe to game/Bin
tools/test/wine.sh ScrapC.exe -fullscreen:0   # hidden (gamescope headless) and muted
```

`wine.sh` runs for `HEADLESS_TIMEOUT` seconds (default 300). Set `WINE_LOG=file` to keep the output.

## Test

The difftest compares the current ScrapC.exe with a frozen reference ScrapC.exe. The reference is built from the tag `ref-noexact` and put into `build-ref-noexact/`. The old `build-ref/` (tag `hooked-final`) is only for comparing with the state of the hook era. The script copies it to `game/Bin/ScrapRef.exe`.

```
DIFFTEST_REF=build-ref-noexact/ScrapC.exe tools/test/difftest.sh
```

Against the old reference these differences are accepted: the text of NaN, "1d5" is no longer read as a number, `%.0f` rounds half to even, trig of huge arguments, and the float hash.

## Roadmap

- Plain C without rva macros (partly done). Done: the code has no rva arguments (the `GAME_*` macros take names only), all game functions are plain cdecl, read-only strings are literals, many globals are plain extern declarations, and the script variables are read as `GVar_X.value.i/.f/.s` (types in `include/scrap/gvar.h`). Open: moving data into its owner files, raw offsets to named fields, typed vtables.
- Drop the bit-exact emulation of the MSVC CRT and math (done 2026-11-02). The game uses the platform libc and libm. `crt_*` in `include/scrap/crt.h` is a thin wrapper over libc (`src/standalone/crt.c`) until the Linux port. The Direct3D device is created with `D3DCREATE_FPU_PRESERVE` so the x87 libm keeps double precision. The INT_MIN-safe integer division of `src/python/intobject.c` stays because gcc wraps `-(-x/y)`.
- A Vulkan-only renderer replacing Direct3D 8: videos through FFmpeg, shaders ported to GLSL.
- A Linux port with SDL3.

## Prior work

- ReScrap ScrapHacks: https://gitdab.com/ReScrap/ScrapHacks (`.packed` format, Kaitai specs, notes for the 2004 exe)
- ReScrap ScrapModLoader: https://gitdab.com/ReScrap/ScrapModLoader
- ReScrap ScrapMods: https://gitdab.com/ReScrap/ScrapMods
