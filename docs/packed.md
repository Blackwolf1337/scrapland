# The .packed archives

Code: `src/game/FileSys/FilePak.c`, `include/scrap/filepak.h`. The ReScrap ScrapHacks project documents the same format for the 2004 exe (link in `README.md`).

## Format

All values are little endian.

| Field | Type | Notes |
|-------|------|-------|
| magic | uint32 | 0x4b504642, "BFPK" in file byte order |
| version | uint32 | 0 |
| count | uint32 | number of files |

Then `count` entries:

| Field | Type | Notes |
|-------|------|-------|
| name length | int32 | |
| name | char[length] | no terminator |
| size | uint32 | |
| offset | uint32 | absolute in the pack |

The file data follows the entry table. Names are relative paths. The engine looks them up lower case with `/` as separator. A wrong magic or version only logs "Wrong format" or "Wrong version", loading goes on.

## Mounting

Up to 128 packs are open at once (`g_PackFiles`, `cFile2` objects). A `cFile2` tracks its file position, so a read close behind the last one needs no seek: a gap of up to 32 bytes is skipped by reading, a larger one seeks and logs "PackFile seek".

One global hash index (457 slots) maps every name to pack number, offset and size. The hash is case-insensitive: shift by 4 bits per character, fold to 28 bits, modulo the slot count. A later pack replaces the entry of an earlier one with the same name, so later packs override.

At start the game mounts Data, Data00, Data01, Data02, Data03 and the language pack (english). The log shows "Pack: Data.packed" and so on.

## Reading files

`cFilePack::Open` prefixes the name with the current pack prefix (`g_PackName`, set by `Scrap.OpenPack`). Names with `../` or `..\` are rejected by CHECK. A name in the index is read from its pack. Any other name is read from disk and logs "WARNING at cFilePack::Open : File not in pak!". Only one cFilePack may be open at a time (`g_PackLocked`).

## Recording and building

When `Data.packed` is missing and no pack is open, the engine opens `filelist.2Bpack` (text, one path per line). From then on it reads every file from disk and appends its name to that list.

`Pack_Build` is a developer path. It reads such a list and can add more files by walking the directory. Mode 2 adds everything, with .dds only under `2D/`. Mode 3 adds Python files, `2D/Loading/`, `Sounds/Stream/` and `Music/`. Mode 4 adds `Sounds/Voices/`. Files ending in .sav .m3d .ini .scc .html .2bpack .packed are skipped. An exclusion and an inclusion list (`;` separated) are honoured. The header and entry table are written twice: once to reserve the room, once after the data with the real offsets. Then the game ends (it asks first in one configuration).

## Extraction

`Pack_Extract(dir)` (script function `Scrap.ExtractPack`) is meant to extract `dir + "Data.packed"` into `dir`. It walks the build list, which mounting does not fill. From the code, in a normal run it has nothing valid to walk, and it only works after a build in the same run. This is an observation from the code, not tested in the game.
