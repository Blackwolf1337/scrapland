# Hook DLL and decompilation workflow

## What it is

`dinput8.dll` is a proxy for the system dinput8. Scrap.exe imports only `DirectInput8Create` from dinput8 and dinput8 is no KnownDLL, so the copy in `game/Bin` is loaded. Under Wine it needs `WINEDLLOVERRIDES=dinput8=n,b`. All five dinput8 exports are forwarded (`src/loader/dinput8.def`). The system DLL is loaded on the first call, not in DllMain.

## Startup

DllMain calls `Game_Init` (`src/core/game.c`). It checks the PE `TimeDateStamp` (0x61b304cc) and `SizeOfImage` (0x55e000) of the exe. Other exes (Dedicated.exe) get no hooks, only the forwarding. Then it reads `scrapdecomp.ini` next to the exe, resolves the imports and installs the hooks.

Log: `scrapdecomp.log` next to the exe, new at every start.

ASLR: everything uses RVAs (Ghidra address minus 0x400000) plus the load address. Under Wine the exe loads at 0x400000, but the code never relies on that.

## Build

```
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake && cmake --build build
```

i686-w64-mingw32-gcc, gnu23, `-Wall -Wextra`, ccache if found. The DLL links against UCRT (Windows 10 or Wine; Windows 7 needs the UCRT update).

## Calling into the exe

`src/imports.inc` lists functions as `SD_IMPORT(name, rva)` (cdecl) or `SD_IMPORT_STDCALL(name, argbytes, rva)`. `src/core/imports.c` turns each into a thunk: a global symbol with the original name and one `jmp [slot]` instruction, slot = RVA + load address. Stack and registers stay untouched, so the prototype from the normal header (Python.h, engine headers) decides the convention.

Global data of the exe: `GAME_DATA(type, rva)` in `include/scrap/game.h` (`g_pWorld`, `g_pApp`). `Py_None` is remapped to `_Py_NoneStruct` of the exe in `include/scrap/python.h`.

## Replacing functions

`src/hooks.inc` lists `SD_HOOK(name, rva, size)`. At startup each original gets a 5 byte `jmp rel32` to the C function (size must be at least 5). There is no trampoline, the C version replaces the original completely. A function is either imported or implemented, never both: the linker reports a duplicate symbol if a name is in both lists.

The return type must match what the original leaves in eax. MSVC often returns a bool as a full int (`mov eax,1`, `neg/sbb/inc`) and its callers test eax; a C function declared `byte` sets only al, so the upper bits are whatever was there (cFile::Exists broke the pack loading this way). Declare `int` unless the original only sets al. A function shorter than 5 bytes can be hooked when int3 padding follows it: give the size up to the next function.

## scrapdecomp.ini

Optional, next to Scrap.exe in `game/Bin`.

```
[general]
hooks=0            ; no hooks at all, only the dinput8 forwarding

[hooks]
Scrap_GetMoney=0   ; one hook off, the original runs
```

## Engine structs

`include/scrap/game.h` holds partial structs with explicit padding. Offsets are checked with `SD_OFFSET` / `SD_SIZE` (static_assert). Known so far:

| Struct | Global | Notes |
|--------|--------|-------|
| `cWorld` | `g_pWorld`, rva 0x467450 | made by `cWorld::cWorld` at rva 0x206db0; `numWeapons` at 0x1bb8, `money` at 0x2090 |
| `CWinApp` | `g_pApp`, rva 0x467ba0 | 0x90 bytes, allocated by the WinMain message loop at rva 0x20d8c0; `levelPath` at 0x34 |

## Python

`include/scrap/python.h` includes the vendored Python 1.5.2 headers (`third_party/python-1.5.2`) with the shared `third_party/python-1.5.2/config/config.h`. It checks `sizeof(PyObject) == 8` and `sizeof(PyTypeObject) == 0x6c`.

## Implemented so far

496 hooks in total (`tools/progress.py` prints the counts). The list is `src/hooks.inc`, the files are under `src/game/<original folder>/`.

- Script API: all 290 functions of the 11 Python modules (Scrap, SAct, SAI, SFX, SInput, SLogic, SNet, SScorer, SSound, SVec, SWeap) and the 6 cPyEntity methods.
- The rest of `PyInterface/`: the cPyEntity and cPyMenuEntity Python types (getattr, setattr, print, dealloc), cPy_Function (`Functions.cpp`, callbacks by name, "Module.func" or "!NativeName"), the module init functions, the Python block pools (`pymempool.cpp`), `PythonInit.cpp` (interpreter start and end, PreloadLibrary) and the 20 SAI SetState handlers.
- `FileSys/`: AbstractFile, File, FileMem, FileBlock, FilePak (see `docs/packed.md`).
- `Global/`: the Nto1Links instances used by cPyEntity.

## Workflow for one function

1. `tools/ghidra/headless.sh Decompile.java <out file> <name or rva> ...` prints Ghidra's decompiler output. It is a reading aid, never copied in unchecked.
2. Write the C version in the matching `src/` file with real types and named struct fields. Add needed struct fields with `SD_OFFSET` checks and needed exe functions to `src/imports.inc`.
3. Add `SD_HOOK` to `src/hooks.inc`.
4. Give the function and the globals found their names in `symbols/functions.tsv` and `symbols/globals.tsv` (source `USER_DEFINED`, signature as `rettype __cc(type name, ...)`). Apply with `tools/ghidra/headless.sh ImportSymbols.java symbols`, check with `ExportSymbols.java symbols` that the export matches.
5. Add checks to `tools/test/py/sdtest.py` if the function is reachable from Python, run `tools/test/difftest.sh`.
6. Commit.

## Testing

`tools/test/install.sh` copies `build/dinput8.dll` to `game/Bin`. It refuses while Scrap.exe or Dedicated.exe runs and copies to `.new` and renames.

`tools/test/difftest.sh` installs the DLL and runs `tools/test/py/sdtest.py` inside the game twice, with the C functions (`hooks=1`) and with the originals (`hooks=0`), hidden and muted through `tools/test/wine.sh`. It diffs the outputs.

- The script is started through the engine variable `PythonExecute` ("This will run a python command", run at startup): `-PythonExecute:execfile(...)`. The command line parser drops quotes and splits at spaces, so difftest.sh spells the file name with `chr()`.
- The game changes its working directory to the game root, so `sdtest.py` and `sdtest.out` live in `game/`. Python writes CRLF there.
- The script ends with a `done` line. difftest.sh then stops the game (`Scrap.Exit()` from the startup script did not quit reliably).
- Outputs and logs go to `build/difftest/`. One run of both passes takes about 40 seconds.

First run (2026-10-25): 6 checks (the three functions, with and without wrong arguments), identical. The game reaches the main menu with the DLL loaded (checked with a gamescope screenshot).

Screenshot of a hidden test run, while it runs (the file appears a moment later):

```
GAMESCOPE_WAYLAND_DISPLAY=gamescope-0 gamescopectl screenshot <file.png>
```

Check which `gamescope-N` socket belongs to the test run if several exist.

Not testable automatically: anything that needs a level loaded or input. Those are tested in the game by hand.
