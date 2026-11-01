# scrapland

Recreation of Scrapland Remastered 1.6.1 (GOG build 52159, `Bin/Scrap.exe`) as C. The code is functionally equivalent C, not the original source.

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

The difftest compares the current ScrapC.exe with a frozen reference ScrapC.exe. The reference is built from the tag `hooked-final` and put into `build-ref/`. The script copies it to `game/Bin/ScrapRef.exe`.

```
DIFFTEST_REF=build-ref/ScrapC.exe tools/test/difftest.sh
```

## Roadmap

- Plain C without rva macros (branch `plain-c`).
- Drop the bit-exact emulation of the MSVC CRT and math.
- A Vulkan-only renderer replacing Direct3D 8: videos through FFmpeg, shaders ported to GLSL.
- A Linux port with SDL3.

## Prior work

- ReScrap ScrapHacks: https://gitdab.com/ReScrap/ScrapHacks (`.packed` format, Kaitai specs, notes for the 2004 exe)
- ReScrap ScrapModLoader: https://gitdab.com/ReScrap/ScrapModLoader
- ReScrap ScrapMods: https://gitdab.com/ReScrap/ScrapMods
