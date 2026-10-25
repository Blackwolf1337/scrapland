# scrapland

Decompilation of Scrapland Remastered 1.6.1 (GOG build 52159, `Bin/Scrap.exe`) into C.

The work is done incrementally with a hook DLL, the re3 method: a proxy `dinput8.dll` replaces original functions one by one, so the game stays playable at every step. The decompiled code is functionally equivalent C, not the original source.

## Goals

- Get the mod SDK back: headers with structs and offsets, hook DLL, Python script API, `.packed` tools, model and map tools.
- Later: a standalone exe, then a native Linux port (SDL3, DXVK-native or an own renderer).

## Rules

- The repo is private.
- Never commit game files. `game/` holds the innoextract output and is gitignored.
- A pre-commit hook in `tools/git/` rejects files over 5 MB. Enable it with `git config core.hooksPath tools/git`.

## Layout

| Path | Content |
|------|---------|
| `docs/` | `binary.md`, triage report of Scrap.exe; `ghidra.md`, Ghidra database and symbols; `hookdll.md`, hook DLL and decompilation workflow; `packed.md`, the .packed archives |
| `symbols/` | `functions.tsv`, `globals.tsv`, `comments.tsv`, `srcstrings.tsv`, `rtti_classes.txt` (see `docs/ghidra.md`) |
| `tools/ghidra/` | `rebuild.sh`, `headless.sh`, export/import and naming scripts (Java), `include/` (parse stand-ins) |
| `third_party/python-1.5.2/` | Python 1.5.2 headers, `COPYRIGHT` |
| `tools/srcstrings.py` | source string table of the static libraries |
| `tools/git/pre-commit` | size check hook |
| `tools/test/` | `prefix.sh`, `wine.sh`, `headless.sh`, `install.sh` (copies the DLL to `game/Bin`), `difftest.sh` (C functions against the originals) |
| `include/scrap/` | engine structs and Python headers of the hook DLL |
| `src/` | hook DLL: `loader/`, `core/`, `py/`, `imports.inc`, `hooks.inc` |
| `CMakeLists.txt`, `cmake/` | build of the hook DLL, mingw-w64 i686 toolchain file |

## Setup

```
innoextract -g -d game/ 'setup_scrapland_remastered_1.6.1_(52159).exe'
tools/test/prefix.sh                          # creates prefix/ (own Wine prefix, audio off)
tools/test/wine.sh Scrap.exe -fullscreen:0    # hidden (gamescope headless) and muted
```

`wine.sh` runs for `HEADLESS_TIMEOUT` seconds (default 300). Set `WINE_LOG=file` to keep the output.

## Build

```
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake && cmake --build build
tools/test/difftest.sh                        # installs the DLL, compares C functions with the originals
```

Details in `docs/hookdll.md`.

## Prior work

- ReScrap ScrapHacks: https://gitdab.com/ReScrap/ScrapHacks (`.packed` format, Kaitai specs, notes for the 2004 exe)
- ReScrap ScrapModLoader: https://gitdab.com/ReScrap/ScrapModLoader
- ReScrap ScrapMods: https://gitdab.com/ReScrap/ScrapMods
