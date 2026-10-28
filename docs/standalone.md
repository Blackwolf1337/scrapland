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
music paths.

The three other blocking places run from the message loop now. `Transport_Wait` polls the select with a zero timeout,
and the rest of the dedicated server's frame runs from the PeekMessage loop of `App_WinMain` (`CApp_FrameResume`)
once a packet came or 1 s passed.

The GOG sign-in: `CWinApp_Init` calls `Gog_ProcessData` once. When not signed in yet, it returns 1 and the message
loop calls `Gog_ProcessData` once per pass (`CWinApp_InitStep`), then runs the rest of the init (`CApp_Init`, the
player names, the console window). In the test prefix the sign-in takes about 2.7 million passes, so the difftest
runs this path. Closing the window before the sign-in ends the start like a failed Init (`App_WinMain` returns 0, as
the original did when Init returned 0). A failed `CApp_Init` run from the loop does the same.

The videos: `Video_RenderStream` sets up and starts the stream and returns. `Video_Step` plays it, once per pass of
the message loop: the sample Update with SSUPDATE_ASYNC and `CompletionStatus` polled without waiting, the Blts
without DDBLT_WAIT and tried again on the next pass while the surface is busy (DDERR_WASSTILLDRAWING). A key or the
pad stops it (a pending Update is ended with COMPSTAT_ABORT). At the end of the stream `Video_Step` runs the rest of
`Video_PlayPath` and plays the second name of an "a|b" list. `Video_PlayFile` and `Video_PlayPath` return
`VIDEO_PENDING` (-1) while a video plays, `Video_Result` has the result afterwards.

The callers go on from the loop in the order of the original. The videos of `Engine_Init` are steps
(`Engine_InitVideos`), after them come the rest of `CApp_Init` (`CApp_InitResume`) and of `CWinApp_Init`. After the
start video of `Engine_LevelEnd` come the rest of the level load and of the frame (`CApp_FrameResume`). After the
video of `Video_PlayStart` the start video name is cleared. `Engine_VideoWait` and `Engine_VideoResume` hold this in
ScrapEngMain.c.

While the app waits (the GOG sign-in, a video, the rest after a video), `App_WinMain` takes only the messages outside
the keyboard and mouse ranges, so the window can be moved or closed, but the input comes after the wait as in the
original (it handled no message then). A WM_QUIT during a video is kept until the wait is over, a line entered in the
console window runs after it, and WM_PAINT does not Present over a video.

`Scrap.VideoPlayer` does not block any more either. `Video_Play` calls the new `Video_PlayThen(name, done)`
(ScrapEngMain.c). It starts the video through `Video_PlayFile` and returns at once. The frame waits for the video
like for the other videos (ENGWAIT_PLAYER in `Engine_VideoResume`). `done`, if given, is called with the result when
the video ends, or at once when it did not start. The script now goes on right after the call, in the original it
went on after the video. As an addition, `Scrap.VideoPlayer(name[, cmd[, done]])` takes an optional third argument:
a Python callable that gets the result (0 not found, 1 played to the end or not opened, 2 stopped by a key or the
pad). The game's own scripts never call VideoPlayer. `Video_PlayFileWait` and the synchronous Update and DDBLT_WAIT
paths of the video code are gone, so no blocking wait is left in the C. `tools/test/py/sdcall_nowait.py` checks the
callback (only in the hooked run, the original takes two arguments).

`tools/test/py/sdcall_nowait.py` checks the paths that end at once: in the test prefix the stream of Video/MSE.vds
does not open (no XviD decoder, OpenFile fails in both runs). The stream itself (`Video_Step` with a real video) is
not tested in the difftest.

## Stage 2: real libraries

Python 1.5.2 from source (python.org py152.tgz, done in stage 2a), UCRT for the `crt_*` imports (stage 2b), zlib 1.1.3,
libpng 1.0.5, IJG libjpeg, D3DX8. The `GAME_DATA` addresses of library data (`include/scrap/python.h`:
`PyString_Type`, `Py_None` ...) switch to the real symbols. Then the whole `.text` of the original is int3. Except
for Python and the CRT, all of this is still to do.

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

### Stage 2b: the C runtime

The `crt_*` functions of `include/scrap/crt.h` are `SD_IMPORT_CRT` (and `SD_IMPORT_CRT_STDCALL`) in `src/imports.inc`:
a thunk in the hook DLL; in ScrapC.exe the C of `src/standalone/crt.c` and `crtinit.c`, mostly a pass-through to the
UCRT that mingw links (the same runtime family as the original's static CRT). `src/core/hook.c` also points the
entries of the original's CRT there (`src/standalone/crthooks.inc`: the `SD_IMPORT_CRT` rvas and the entries that
only the code of the copy and the tests reach, such as memcpy, memset, `_alloca_probe`, `_EH_prolog`, `_setjmp3`), so
the C, the Python library, the copy and the tests share one heap (the process heap, which the static CRT used too),
one set of FILE objects, errno, locale and rand state. What differs from a plain UCRT call is C:

- `_chdir` sets the variable `=X:` of the environment like MSVC (Wine's does not), so `_chdrive` comes back to it
- atexit and exit (the game's atexit functions, last registered first, then the UCRT exit), the thread-safe statics
  (one epoch in C, ScrapC.exe no longer gives the original its TLS block), the vector constructor and destructor
  iterators, the STL error helpers

The math comes from Microsoft's ucrtbase.dll, the same code as the static UCRT of the original
(`src/standalone/crtmath.c`). Wine's own ucrtbase differs in the last bits. ScrapC.exe loads `ucrtbase_ms.dll` from
its folder when it is there, otherwise the system `ucrtbase.dll`, which on Windows is Microsoft's and the one the exe
links. `tools/test/getucrt.sh` puts `ucrtbase_ms.dll` into game/Bin for the Wine tests (a Microsoft binary, never in
git). There is no Wine DLL override.

Each entry is a jmp through a slot that holds the export, so the SSE2 entries (xmm0, xmm1) and the `_CI` intrinsics
(x87 stack) keep their conventions. The entries:

- the 23 cdecl functions of `crt.h`, acos to tanh, with sqrt, fmod, floor, ceil, fabs, frexp, ldexp and modf
  (`SD_IMPORT_CRT`; the Python's math module holds their addresses)
- the SSE2 entries that `src/core/libm.c` calls. They are `_libm_sse2_*_precise` (the same bytes as the exe's
  entries), not `__libm_sse2_*`, which in ucrtbase report no errors
- `_CIatan2` and `_CIfmod` for libm.c, `_libm_sse2_sqrt_precise`, `_CIacos` and `_CIpow` for the copy

libm.c calls the labels of `crt.h` in ScrapC.exe, `crthooks.inc` points the entries of the copy to the same labels.
ucrtbase gets `_set_SSE2_enable` like the CRT initializer at rva 0x35fce1.

Domain and range errors: ucrtbase calls the user matherr and sets errno when it returns 0, like the static CRT
(`_umatherr` at rva 0x35657e). `Crt_MathErrMs` is registered with the loaded ucrtbase and calls the game's
`E3D_MathErr`, which logs "Math Error" and returns 0. Under Wine `ucrtbase_ms.dll` keeps an errno of its own, so the
handler also sets the errno of ScrapC.exe (EDOM for `_DOMAIN`, ERANGE for `_SING` and `_OVERFLOW`), which the Python
reads after its math calls. On Windows both are the same DLL. The "Math Error" lines of log.html are the same as the
original's in all three scenarios but for the sqrt lines of sdcall (below).

No DLL exports `__ftoui3`, `__dtoui3` and the vector cos, sin and exp of two doubles. They are C of our own in
`src/standalone/crtvec.c`, written from their documented behavior, with a short asm wrapper each for the register
convention (argument in xmm0, result in eax or xmm0):

- the conversions give the low 32 bits of the 64 bit truncation toward zero, so a negative or too large value wraps.
  A NaN, an infinity or a magnitude of 2^63 or more gives 0 and errno EDOM, like the runtime's invalid path.
  `tools/test/py/sdcall_crtconv.py` compares results and errno with the original for normal, wrapping, negative and
  special inputs (`SDTest.ConvU` passes the argument bits in xmm0)
- each lane of the vector functions is the scalar cos, sin or exp of ucrtbase, so a result can differ from the
  original's vector code in the last bit (accepted). The tests that reach them (`CBmpFlt_Exposure`, the lava cos
  table of `CFXTexLava`) match bit for bit all the same, so none compares a rounded value

ScrapC.exe starts in `Crt_Main` of `crtinit.c`, the entry point of the original is not called (`[general]
crtstart=0`, `DIFFTEST_CRTSTART=0` still runs it). Crt_Main sets what the CRT initializers of the `__xi` table set
(x87 precision 53 bits, the option words of the inline printf and scanf, the game's matherr for the UCRT the exe
links), runs the 1790 dynamic initializers of the game from the `__xc` table in order (the first entry is the CRT's)
and calls `App_WinMain` with the module, the command line of `_get_narrow_winmain_command_line` and the show mode of
the startup info.

`[general] wipecrt=1` (`DIFFTEST_WIPECRT=1`, ScrapC.exe only) fills the code of the original's CRT with int3,
`include/scrap/origcrt.h`: rva 0x313634 (operator delete, after the Python) up to the C++ unwind funclets of the game at
0x37b3d0, but for the import thunks of the game in it (fmod, avifil32, msvfw32 and ddraw at 0x3179c6..0x317b2e,
wsock32 and d3d8 at 0x3761d0..0x3761dc). The atexit destructors of the game's globals from 0x38cda0 stay. With
`wipe=3` the probe covers it. With the original's entry (`crtstart=0`) the CRT init itself runs, so `wipecrt=1` needs
the default start.

On 2026-10-28 `DIFFTEST_WIPE=2 DIFFTEST_WIPEPY=1 DIFFTEST_WIPECRT=1 DIFFTEST_STANDALONE=1 tools/test/difftest.sh`
matches in all three scenarios with no "original code reached" line, and the probe run (`DIFFTEST_WIPE=3`, the same
otherwise) logs no trap. No code of the original's CRT or Python runs any more.

GCC turns `(float)__builtin_sqrt((double)f)` of the game C into `sqrtf`, which it calls for the errno of a negative
argument. That call goes to the UCRT the exe links, so log.html shows "sqrtf: argument domain error: -3.000000" where
the original logs "sqrt: argument domain error: -nan(ind)" (sdcall). The checks match.

Code of the copy that calls the CRT itself (static scan, `src/standalone/crthooks.inc` points all of these to C or
ucrtbase):
the dynamic initializers (`_atexit` from 347 places, `new[]`, `memset`, the vector constructor iterator), the atexit
destructors (`delete`, `delete[]`, the vector destructor iterator, `fclose`), the M3D library
(`__security_check_cookie`, `_libm_sse2_sqrt_precise`), D3DX8 with libpng, libjpeg and zlib (`malloc`, `free`,
`_alloca_probe`, `_EH_prolog` from 26 places, `_setjmp3` and `longjmp`, `_ftol`, `_aullshr`, `_CIacos`, `_CIpow`,
`floor`, `atoi`, ctype, `setlocale`, `strncpy`, `_strdup`, `calloc`, `_snprintf`, `_vsnprintf`, `exit`) and the ASE
SDK.

### Stage 2c: D3DX8, ASE and the M3D math (in progress)

The start of the code of the original is not all library code: rva 0x001000..0x002070 holds the data formats of
dinput8.lib (`c_dfDIKeyboard` and friends, data in `.text`), rva 0x002070..0x03e800 the libraries (the ASE SDK,
D3DX8 with its zlib 1.1.3, libpng 1.0.5 and IJG libjpeg 6a), and rva 0x03e800..0x05f6d8 the 1784 dynamic
initializers of the game, which the CRT calls from its table at rva 0x3a15c4. "M3D" at 0x2d6010..0x2da770 is
Engine/descomp_afin.cpp.

`[general] wipelow=1` (`DIFFTEST_WIPELOW=1`, ScrapC.exe only) fills the libraries (0x002070..0x03e800) and
descomp_afin.cpp with int3; with `wipe=3` the probe covers them. The library code has no int3 padding between its
functions, so a probe piece there can span several functions.

Done in C, as hooks in both builds:

- the D3DX math the game calls (`src/lib/d3dx8/D3dxMath.c`). D3DX picks its code per CPU on the first call (rva
  0x017f06); the C has the versions of a CPU with SSE2 and without 3DNow!: SSE2 for normalize, transform, multiply,
  inverse and the rotations about x and y (with the same rcpss/rsqrtss estimates and its own sine and cosine), the
  x87 code for transpose, the rotation about z, yaw pitch roll and the quaternion matrix.
- the shader assembler `D3DXAssembleShader` (`src/lib/d3dx8/D3dxAsm.c`): the Berkeley yacc parser with the tables
  of the exe (`D3dxAsmTables.h`), its lexer and rules, the validation through d3d8.dll. The game assembles the 30
  pixel shaders of `bmp/*.psh` (ps.1.1); `tools/test/py/sdcall_d3dxasm.py` compares their tokens and the errors of
  bad texts.
- descomp_afin.cpp (`src/game/engine3d/Engine/DescompAfin.c`): Shoemake's polar decomposition of Graphics Gems IV
  and the two transform matrices.

Still from the copy: the ASE SDK and the texture functions of D3DX (`D3DXCreateTextureFromFileInMemoryEx`,
`D3DXCheckTextureRequirements`, `D3DXLoadSurfaceFromSurface`). The game data loads DDS through D3DX (DXT1, DXT3,
DXT5, A8R8G8B8, R5G6B5, A4R4G4B4, X1R5G5B5; many with one level where the texture wants mipmaps, so D3DX makes them,
for DXT with its own compressor) and BMP and TGA through its own code, then D3DXLoadSurfaceFromSurface. No PNG or JPG
is in the packs.

## Stage 3: data in C

The data sections move into C: vtables, tables, strings, then the globals with their types. After that the
original exe is no longer read.

## Stage 4: English names

The Spanish names of the original stay until the C is complete, then everything is translated.
