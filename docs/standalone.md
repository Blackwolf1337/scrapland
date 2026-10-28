# Standalone build

Goal: a `scrap.exe` built only from the C code of `src/` and real libraries, with no code of the original Scrap.exe.
It is reached in stages, each one runs the difftest.

## Stage 0: wipe check (hook DLL)

`[general] wipe=1` in scrapdecomp.ini (`DIFFTEST_WIPE=1 tools/test/difftest.sh`) fills the rest of every hooked
original with int3 after the hook jump. A jump into the middle of replaced code stops the game, and the log says
`wipe: original code reached at rva ...`.

## Stage 1: loader with the data of the original

`ScrapC.exe` (`src/standalone/main.c`) has the base 0x400000 of the original. `cmake/ScrapC.ld` puts the
uninitialized section `.orig` at rva 0x1000, as large as the original image, and the own code after it (under Wine
the low memory is full of mappings, so the room must be part of the exe). At start it reads the user's Scrap.exe
1.6.1 from game/Bin, copies its sections to their rvas (the rvas of `GAME_DATA` stay valid, no relocation), points
its own resource directory to the original's, fills the import table with GetProcAddress (dinput8 from the system
folder), gives the original its TLS block (the MSVC thread init epoch), installs the hooks and runs the entry point
of the original. Library code (zlib, libpng, libjpeg and the ASE SDK below rva 0x05f6d8; the M3D math library at
0x2d6010..0x2da770, still called by the C through imports; Python 1.5.2 at 0x2da770..0x313634; the MSVC CRT from
its operator delete at 0x313634, and D3DX8) still runs from the copy.

`DIFFTEST_STANDALONE=1 tools/test/difftest.sh` runs the hooked side as ScrapC.exe. Since 2026-10-28 all three
scenarios match.

`tools/standalone/inventory.py` lists what is still missing:

- code pointers in the data sections (vtables, method tables) that point to game code without a C version: the
  C++ catch blocks of the GOG code in the exception tables (their functions are C)

### Stage 1b: the whole game code int3

`[general] wipe=2` (`DIFFTEST_WIPE=2`) fills the whole game and engine code (rva 0x05f6d8..0x2d6010) with int3
before the hooks are written. Since 2026-10-28 `DIFFTEST_WIPE=2 DIFFTEST_STANDALONE=1 tools/test/difftest.sh`
matches in all three scenarios. The tiny methods share the stubs of `src/game/Global/Stubs.c`, the Winsock thunks
are C (`src/game/Net/Wsock.c`), the catch blocks of the GOG code are done without C++ exceptions
(`src/game/Global/Gog.c`).

What `tools/standalone/uncovered.py` still lists is code that nothing reaches: no call, jump or pointer of the exe
(code or data) and no rva in the C points to it. These are functions the compiler kept after it inlined every call,
the jump thunks of unused imports and pieces of hooked functions behind their hook size (only the original body
jumps there), besides the switch and byte index tables.

`wipe=3` is a probe for this work: the same int3, but each trap logs its rva and `[esp]` (the caller when the trap is
the start of a function) and copies the piece of original code there back, so one run lists every piece of game
code without a C version that the run reaches.

The seven functions with Sleep or busy waits are C too, with intended behavior changes (each marked in the code
with a comment): the frame (`CMotorApp_Render`) has no Sleep loop as its frame limiter, `Motor_FrameDue` holds the
next frame back in the PeekMessage loop of `App_WinMain` (no own limiter with vsync); the device reset has no
Sleep(250); `Timer_GetQPFreq`, `Timer_MeasureQP` and `Timer_CountTicks` take QueryPerformanceFrequency instead of
measuring; `Tsc_CpuFreq` measures rdtsc against the performance counter between two calls; `Video_WaitKeysUp`
starts a hold under which the video loop and `Keyboard_Poll` ignore Escape, Space and Enter until they are up or
the time is over. `tools/test/py/sdcall_nosleep.py` checks them (some only in the hooked run).

A second round removed the other Sleep calls and blocking waits from the C, each marked "differs from the original
on purpose" in the code. `Splash_Show` (`ScrapEngWorld.c`) draws the splash again instead of Sleep(10). Its loop
stays, because every caller goes on right after it. In the light map compiler (`Bsp.c`), `BspTxf_Guardar` no longer
sleeps 10 s and retries a failed write: the next load finds the TXF missing and computes it again. `BspTxf_Cargar`
no longer waits up to 30 s for another computer of a distributed compile. It deletes that file and rebuilds it here.
In the fmod code (`MusicFmod.c`), voices open without FSOUND_NONBLOCKING, so `VoiceRemain` knows the length right
after `PlayVoice`, and `Voice_Wait` is empty. `Music_Stop` closes a stream that is still opening later, from
`Music_Update` (a list of 8 slots), because FSOUND_Stream_Close fails on it. `CWinApp_Frame` (`WinApp.c`) returns at
once while the app is inactive, instead of Sleep(250), so the PeekMessage loop then runs at full CPU. `CD3DApp_Run`
uses PeekMessageA instead of GetMessageA while inactive. `CEntradaDInput_ReadKeys` tries Acquire once per call
instead of spinning. `win32.h` no longer has `exe_Sleep`. `tools/test/py/sdcall_nosleep2.py` checks the voice and
music paths. Three places still block, each with a plan:

- the GOG sign-in wait in `CWinApp_Init` (`while (!g_GogReady)`). Plan: finish the init from `CWinApp_Frame` once
  signed in.
- `Transport_Wait`, a select with a 1 s timeout that throttles the dedicated server. Plan: timeout 0 plus a frame
  gate, waiting for the user's decision.
- the video playback in `Video_RenderStream` (synchronous sample Update, DDBLT_WAIT). Plan: SSUPDATE_ASYNC, polled
  once per frame.

## Stage 2: real libraries

Python 1.5.2 from source (python.org py152.tgz, done in stage 2a), UCRT for the `crt_*` imports, zlib 1.1.3,
libpng 1.0.5, IJG libjpeg, D3DX8. The `GAME_DATA` addresses of library data (`include/scrap/python.h`:
`PyString_Type`, `Py_None` ...) switch to the real symbols. Then the whole `.text` of the original is int3. Except
for Python, all of this is still to do.

### Stage 2a: the real Python

`ScrapC.exe` links a real Python 1.5.2. It is built from the unchanged sources of `third_party/python-1.5.2` (CMake
target `python152`, `-std=gnu89`, `NDEBUG`). `src/python/config.h` is derived from the MSVC part of `PC/config.h`
with the choices of the build in Scrap.exe:

- static, no `MS_COREDLL` (the `_PyObject_New` of the exe takes only the type, `sys` has no `dllhandle` or `winver`)
- no `WITH_THREAD` (the exe has no thread module and no `PyEval_AcquireThread`)
- `staticforward` is `static`, `MS_WIN32` and `MS_WINDOWS` are set
- `COMPILER` is "[MSC 32 bit (Intel)]" and `PLATFORM` is "win32"

The build date and time of the exe ("Nov 24 2021", "15:23:52") are set for `getbuildinfo.c`, so `sys.version` reads
like the original. `src/python/config.c` is the module table of the exe (rva 0x428d58) in its order: the 11 engine
modules, then errno, math, new, regex, signal, strop, _locale, marshal, imp, `__main__`, `__builtin__`, sys. The
errno and signal modules have the same names and values as the exe's (checked against its init code).

`src/python/pyshim.h` is force-included into every Python source. It makes the library use what the Python of the
exe used. `malloc`, `realloc` and `free` are the engine's Python pool (`PyPool_Malloc` and friends, as in the exe,
where all 124 call sites go to the pool). The inline stdio functions of the UCRT headers (`sprintf`, `vsprintf`,
`fprintf`, `vfprintf`, `printf`, `sscanf`) are the C versions in `src/game/Global/StdioInline.c`. Every other libc
function that is not pure string work (FILE functions, `errno`, `getenv`, `signal`, locale, ctype, `strtod` and
`atof`, `stat`) goes to the C runtime of the original through `crt_*` imports, so FILE objects, `errno` and the
float formatting and parsing are the exe's.

Math is split in two. Direct calls use the SSE2 entries that MSVC calls (`Game_Sin`, `Game_Pow`, `Game_Fmod` ... of
`src/core/libm.c`). The address of a function (the tables of the math module) is the cdecl function of the runtime.
One macro pair does both: `#define sin crt_sin` together with a function-like `crt_sin(x)` that becomes
`Game_Sin(x)`, while a bare `sin` stays `crt_sin`.

The engine changed three files of its Python. The vendored files stay as they are: `src/python` has files that
include the vendored file and add the change around it.

- `import.c`: once the startup imports are done (`g_PyPackImport`), reading or writing a .pyc prints
  "WARNING: import %s (read)" or "(wrote)" with "the library was not preloaded". Macros on the one
  `PyMarshal_ReadObjectFromFile` call and the one `fflush` put the warnings at the same places as in the exe, also
  with -v.
- `pythonrun.c`: `PyRun_ScrapString` is the `PyRun_SimpleString` of the console and the startup. It drops CR, adds
  a newline, parses as `single_input` in a static buffer and runs under the file name "Scrap input".
  `Py_FiniObjects` frees the objects of static pointers after `Py_Finalize`, before the engine deletes its pool.
- `dictobject.c`, `stringobject.c`, `listobject.c`, `classobject.c` and `getpathp.c`: the statics that
  `Py_FiniObjects` frees.

The exe also frees the interned names held in function statics of `classobject.c` and the "__path__" of
`get_parent` in `import.c`. They cannot be reached from outside the vendored files and stay in the pool, which is
deleted right after (small pool blocks, nothing visible).

The shared C is built twice. CMake makes the object library `scrapcode` for the hook DLL (unchanged: the Python
functions are thunks into the exe) and `scrapcode_sa` with `SD_STANDALONE` for ScrapC.exe. The Python functions in
`src/imports.inc` are `SD_IMPORT_PY`: a thunk in the DLL, none in ScrapC.exe, which links the library.
`include/scrap/python.h` maps the Python data of the exe with `GAME_DATA` only without `SD_STANDALONE` (the type
objects, `_Py_NoneStruct`, `Py_VerboseFlag`, `PyImport_Inittab`, `g_PyPackImport`). ScrapC.exe uses the real symbols.

The copy of the original still holds pointers into the Python code and data: the `ob_type` of the two engine type
objects at rva 0x428e18 and 0x428f78, the init functions in the module table of the exe, and the game code that is
C now (mostly `Py_None`). `Orig_PyPointers` in `src/standalone/main.c` walks the .reloc of the original at start.
Every pointer outside the Python (data, game code, CRT and library code) that leads into the Python code or data is
set to the real symbol (371 pointers, 18 targets). A target without a real symbol stops the start. This was chosen
over defining these objects in C: the scan finds every such pointer, also ones nobody listed, and the type objects
and tables of the engine stay the exe's data until stage 3 moves all data into C at once. No CRT or other library
code points into the Python.

`tools/standalone/pymap.py` names the Python of the exe in the names of the vendored sources: functions from
`symbols/functions.tsv` and by their string literals, type objects by `tp_name`, None and Ellipsis by their type,
flags by the code next to their strings. With `-c` it prints the table of `main.c`. The ranges are in
`include/scrap/origpython.h`: code 0x2da770..0x313634, data 0x432b50..0x43cfd8 and 0x4e73c0..0x4e8aa8, and the
common symbols 0x4ed3a0..0x4ed540.

`[general] wipepy=1` (`DIFFTEST_WIPEPY=1`, ScrapC.exe only, the DLL ignores it with a log line) fills the code of
the original's Python with int3. With `wipe=3` the probe covers it too. On 2026-10-28 the probe run (`wipe=3`,
`wipepy=1`) logged no trap in any scenario. `DIFFTEST_WIPE=2 DIFFTEST_WIPEPY=1 DIFFTEST_STANDALONE=1
tools/test/difftest.sh` matches in all three scenarios, and so does the hook DLL difftest. Two tests had printed
memory as `new[]` left it, which holds the address of the string type object of the Python and so differs now. They
print only written memory now (`Tabla_New` at rva 0x2c2c60, the trail points after `SetMaxTrails`).

What still differs from the exe: `sys.executable` names ScrapC.exe.

## Stage 3: data in C

The data sections move into C: vtables, tables, strings, then the globals with their types. After that the
original exe is no longer read.

## Stage 4: English names

The Spanish names of the original stay until the C is complete, then everything is translated.
