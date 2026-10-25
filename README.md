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
| `docs/` | `binary.md`, triage report of Scrap.exe |
| `symbols/` | `rtti_classes.txt`, 321 MSVC RTTI type names from Scrap.exe |
| `tools/git/pre-commit` | size check hook |
| `tools/test/` | `prefix.sh`, `wine.sh`, `headless.sh` |

## Setup

```
innoextract -g -d game/ 'setup_scrapland_remastered_1.6.1_(52159).exe'
tools/test/prefix.sh                          # creates prefix/ (own Wine prefix, audio off)
tools/test/wine.sh Scrap.exe -fullscreen:0    # hidden (gamescope headless) and muted
```

`wine.sh` runs for `HEADLESS_TIMEOUT` seconds (default 300). Set `WINE_LOG=file` to keep the output.

## Prior work

- ReScrap ScrapHacks: https://gitdab.com/ReScrap/ScrapHacks (`.packed` format, Kaitai specs, notes for the 2004 exe)
- ReScrap ScrapModLoader: https://gitdab.com/ReScrap/ScrapModLoader
- ReScrap ScrapMods: https://gitdab.com/ReScrap/ScrapMods
