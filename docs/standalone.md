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
its operator delete at 0x313634, and D3DX8) still runs from the copy. Since stage 3a the data is built into
ScrapC.exe and the original exe is no longer read (below).

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
C now (mostly `Py_None`). `Orig_PyPointers` in `src/standalone/main.c` walked the .reloc of the original at start
(since stage 3a `tools/standalone/datagen.py` does it at build time).
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
crtstart=0`, `DIFFTEST_CRTSTART=0` ran it until stage 3a removed it). Crt_Main sets what the CRT initializers of the `__xi` table set
(x87 precision 53 bits, the option words of the inline printf and scanf, the game's matherr for the UCRT the exe
links), runs the 1790 dynamic initializers of the game from the `__xc` table in order (the first entry is the CRT's)
and calls `App_WinMain` with the module, the command line of `_get_narrow_winmain_command_line` and the show mode of
the startup info.

`[general] wipecrt=1` (`DIFFTEST_WIPECRT=1`, ScrapC.exe only) fills the code of the original's CRT with int3,
`include/scrap/origcrt.h`: rva 0x313634 (operator delete, after the Python) up to the C++ unwind funclets of the game at
0x37b3d0, but for the import thunks of the game in it (fmod, avifil32, msvfw32 and ddraw at 0x3179c6..0x317b2e,
wsock32 and d3d8 at 0x3761d0..0x3761dc). The atexit destructors of the game's globals from 0x38c720 are game code
(stage 2d). With
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

### Stage 2c: D3DX8, ASE and the M3D math (done)

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

Stage 2c2 wrote the rest from scratch, from the documented behavior and the observed results, not from the code of
the copy. These are tested by what they do, not bit for bit:

- the texture functions of D3DX (`src/lib/d3dx8/D3dxTex.c`): `D3DXCheckTextureRequirements`,
  `D3DXCreateTextureFromFileInMemoryEx` and `D3DXLoadSurfaceFromSurface`. The game loads DDS through D3DX (DXT1,
  DXT3, DXT5, A8R8G8B8, R5G6B5, A4R4G4B4, X1R5G5B5) and one TGA, `bmp/norm1.alpha.psa` (its extension is not one the
  game's own reader takes). BMP and TGA go through the game's code, then `D3DXLoadSurfaceFromSurface`. No PNG or JPG
  is in the packs, so the loader reads DDS and TGA.
- The closest format follows the D3DX rules (missing channels weigh most, then fewer bits, then extra channels, then
  more bits; the smaller pixel on a tie). P8 asked for a texture becomes X8R8G8B8, as D3DX did on this device.
- The levels of a file are its mip count, with or without the flag of the count (as D3DX). A level the file has is
  copied in the same format, else converted; the missing ones are box filtered from the level above in the format
  of the texture, DXT through our own encoder (a range fit on the bounding box of the block).
- Pixels move as float RGBA. The conversions between the uncompressed formats give the bytes of D3DX, also the
  signed ones: s = 2x - 1 stored as trunc(s * 2^(n-1) + 0.5), so 255 wraps to the lowest value as in D3DX.
- What differs: the DXT blocks D3DX encodes, the DXT levels below 4 pixels (D3DX encodes them again, ours copies the
  file's blocks), the made levels (D3DX rounds them in other steps, its signed levels drift), dithering
  (`D3DX_FILTER_DITHER` is ignored) and the scaling filters, which the game never needs (no size changes on this
  device).
- `tools/test/py/sdcall_d3dxtex.py` (prefix `dxt`) checks the requirements, the conversions and the color key
  exactly, and 14 DDS files and the TGA with the requests of the game through `CTextura_CreateD3D`: format, size and
  levels exactly, the file's levels as a checksum, the converted and made levels by their average color within a
  tolerance. The device calls of a texture are the same in the D3D log.
- the ASE SDK (`src/lib/ase/Ase.c`): a UDP socket on the game port + 123 (the next free one of 64) answers the
  All-Seeing Eye queries 's' (status, rules, players from the game's callbacks), 'g' and the master's 'p'. The
  masters are gone and resolving them would block, so there is no heartbeat and no 'v'.
  `tools/test/py/sdlevel_ase.py` (prefix `ase`) queries it on the loopback in the level and compares the answers.

All of these are hooks in both builds; the hook DLL difftest matches with them. In ScrapC.exe no code of the range
0x002070..0x03e800 runs now: on 2026-10-28 the probe (`DIFFTEST_WIPE=3 DIFFTEST_WIPEPY=1 DIFFTEST_WIPELOW=1
DIFFTEST_STANDALONE=1`) logged no trap, and `DIFFTEST_WIPE=2 DIFFTEST_WIPEPY=1 DIFFTEST_WIPELOW=1
DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios. The dynamic initializers at
0x03e800..0x05f6d8 are game code and stay outside wipelow.

### Stage 2d: the dynamic initializers (done)

The 1784 functions at rva 0x03e800..0x05f6d8 are the dynamic initializers of the game's globals, which the CRT calls
from its `__xc` table (0x3a15bc..0x3a31bc, 1791 entries, the first is the CRT's; 6 more initializers lie in the game
code from 0x05f6e0 and were done in stage 1b). They register 347 atexit destructors, which lie at the end of the code,
rva 0x38c720..0x38f097, together with the destructors of 6 function statics that game code registers (353 in all).
Every one of them is C now and a hook. Crt_Main still walks the table of the original, so the initializers run in its
order through the hook jumps, and the destructors are registered with `crt_atexit` in the same order.

`tools/standalone/initpat.py` disassembles them (capstone) and matches them against templates with the addresses and
immediates as parameters, with checks of the offsets and jump targets. A match gives the object, strings, values,
constructor and destructor. With `-w` it writes `src/game/Global/StaticInit.inc` (one macro line per function, in the
order of the table, each destructor before the initializer that registers it), the SD_HOOK lines below the 2d marker
of `src/hooks.inc` (the block is rewritten) and the rows of `symbols/functions.tsv` (USER_DEFINED,
`void __cdecl(void)`). `src/game/Global/StaticInit.c` has the macros, which call the C constructors and destructors;
the names come from the variable names (`Init_GVar_<name>`, `Init_NativeFunc_<name>`), the names of the globals in the
C or the class and the address. The patterns:

- engine variables: 1237 with a constant int or float, 7 with a float from the data (also times a factor over a
  divisor, or plus a term), 242 strings (two cString, freed by the destructor), 60 script functions (a cPy_Function)
- 175 entries of the registry of C functions for the scripts (`cPy_NativeFunc_ctor`, no destructor)
- objects with a constructor of the C and a destructor (20), the vector constructor iterator (6), a constructor in a
  loop (2), a constructor without destructor (8), the slots of a HashTable (4), a vector of three floats (2), only
  the destructor (11)
- destructors: a call of the C destructor (90), two (243, mostly the strings), the vector destructor iterator (7), a
  CTabla freed (6), a DList cleared with its free list (2)

The 10 initializers and 5 destructors that fit no pattern, and 9 destructors that belong to C written by hand (their
initializer or the function of their static), are C by hand in the files of their classes (the HAND table of the
tool): the costs of the Swarm, the Steam ticket callback,
the four pads, the chat lines, the cPy_Function of the sphere tests, the paths of the exe and the engine, the shader
switch and the identity and bias matrices, the box of `g_PintarTodas`, the faces of CheckBBox, the trace caches of the
lights, and the destructors of g_ActionSets, g_ListaMat, g_FileLog, g_TexList and the function statics.

`[general] wipeinit=1` (`DIFFTEST_WIPEINIT=1`, ScrapC.exe only; the exe of the hook DLL ran its initializers before
the hooks) fills both ranges with int3, and the probe of `wipe=3` covers them. The hook DLL installs the hooks too,
there only the destructors run, at the exit. ScrapC.exe logs the number of atexit functions at the exit
("crt: exit(0), 349 atexit functions" when the game is quit from a script; all of them ran with every switch on).

On 2026-10-28 the probe run (`DIFFTEST_WIPE=3 DIFFTEST_WIPEPY=1 DIFFTEST_WIPECRT=1 DIFFTEST_WIPELOW=1
DIFFTEST_WIPEINIT=1 DIFFTEST_STANDALONE=1`) logged no trap, and `DIFFTEST_WIPE=2 DIFFTEST_WIPEPY=1 DIFFTEST_WIPECRT=1
DIFFTEST_WIPELOW=1 DIFFTEST_WIPEINIT=1 DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios,
and so does the hook DLL difftest. The difftest ends the game with `wineserver -k`, so it does not run the
destructors.

## Stage 3: data in C

The data sections move into C: vtables, tables, strings, then the globals with their types. Stage 3a builds the data
of the original into ScrapC.exe at its rvas, so the original exe is no longer read at run time. Stage 3b gives it
typed C definitions at the same addresses, region by region, stage 3c normal C linkage without fixed addresses.

### Stage 3a: the data built in

The build of ScrapC.exe needs Scrap.exe 1.6.1 (CMake cache path `SCRAP_EXE`, default `game/Bin/Scrap.exe`).
ScrapC.exe itself needs only the game files, it no longer opens the original exe.

`tools/standalone/datagen.py` checks the timestamp and SizeOfImage like `Game_IsBuild` and writes `room.c`, `room.ld`,
`room.h` and `room.bin` into `build/standalone` (generated, not in the repository). Built in at their rvas are .rdata,
.data, .gfids, .tls, _RDATA, .data1 and .rsrc. The zero tail of .data, .bss_ste and the code range stay
uninitialized, .reloc is left out (fixed base).

Of .text only the pieces that are still read or run are built in:

- the data formats of dinput8.lib at 0x001000..0x002070 (the `c_dfDIKeyboard` and `c_dfDIMouse` in .rdata point there)
- the import thunks of the game in the CRT (`ORIG_CRT_THUNKS` of `origcrt.h`)

Until stage 2d the dynamic initializers at 0x03e800..0x05f6d8 and, from 0x37b3d0 to the end of .text, the C++ unwind
funclets and the atexit destructors were built in too. The C reads no other data in .text: its `Game_Ptr` and
`GAME_DATA` rvas below 0x3a1000 are hooked functions or .bss_ste.

`cmake/ScrapC.ld` includes `room.ld`, which puts the sections `.orig0`, `.orig1` ... from rva 0x1000 to 0x55e000:
initialized ones where there are bytes, uninitialized ones without file data for the gaps (the code, the tail of
.data, .reloc). That is 890 KB of data of the original in the exe, 690 KB of it non-zero.

At start `main.c` fills the rest of .text with int3 (`ROOM_INT3` of `room.h`), fills the import table of the
original with GetProcAddress from its import directory and points its own resource directory to the original's.
The TLS directory of the original is built in with .rdata and .tls, but nothing reads it any more (`crtinit.c` keeps
the epoch of the thread-safe statics). `Orig_Read`, the section copy and `Orig_PyPointers` are gone.

The pointers of the built-in data into the original's Python are symbol references in `room.c`, which the linker
sets: `datagen.py` holds the table of real symbols (`pymap.py -c` prints it), a target without one stops the build.
11 pointers are left (the init functions in the module table at 0x428d58 and the `ob_type` of the two engine type
objects), the others of stage 2a were in game code that is int3 now.

ScrapC.exe now runs as with `wipe=2`, `wipepy=1`, `wipecrt=1` and `wipelow=1` in any case. The switches still run and
log, they find int3 there already. The handler that logs "original code reached" is always on in ScrapC.exe. With
`wipe=3` the probe has no original code to copy back in ScrapC.exe, so it stops at the first trap: probe runs work
in the hook DLL only. `crtstart=0` is gone, the CRT of the original is not built in. `hooks=0` or a hook switched off
in scrapdecomp.ini leaves int3 in ScrapC.exe.

On 2026-10-28 `DIFFTEST_WIPE=2 DIFFTEST_WIPEPY=1 DIFFTEST_WIPECRT=1 DIFFTEST_WIPELOW=1 DIFFTEST_STANDALONE=1
tools/test/difftest.sh` matches in all three scenarios with no "original code reached" line, and so does the hook
DLL difftest. A run of ScrapC.exe in each scenario with `game/Bin/Scrap.exe` renamed gives the output of the
original but for the checks of that file itself (`Scrap.FileExist(Bin/Scrap.exe)`, `File_Access` and `File_Stat` of
it).

### Stage 3b: typed C at the same addresses

The data moves into C region by region, each object at the rva of the original, so the rvas of `GAME_DATA`,
`Game_Ptr` and the tests stay valid. `SD_AT(rva)` (`include/scrap/shared.h`, only with `SD_STANDALONE`) puts a C
object into a section `.sd.<rva>` of its own:

    void (*const cAI_vftable[14])(void) SD_AT(0x3a3f18) = { ... };

It is aligned to 4 (GCC would align larger arrays to 32), so it must start at a multiple of 4, and it covers its size
rounded up to 4, because the assembler pads every PE section to that. Neighbours smaller than that go into one
struct. SD_AT objects belong in the sources of `scrapcode_sa` (src/core, src/game, src/lib), the hook DLL keeps the
exe's own data.

`tools/standalone/datagen.py` learns the objects from the object files themselves: CMake writes the list of the
objects of `scrapcode_sa` into `build/standalone/objects.txt` (`file(GENERATE)`), and datagen.py reads the section
table, the contents and the relocations of each `.sd.` section (COFF). So there is no second link pass, no table to
keep in step with the C and no generated header. The order is: compile `scrapcode_sa`, run datagen.py, link
ScrapC.exe (it links the objects, so they are built first). datagen.py depends on these objects and runs again when
one of them changes; it always writes its outputs, so a build without changes does not run it. `room.c` carries the
SHA-1 of `room.bin`, because ccache does not hash the data of `.incbin`.

In `room.ld` each section `.orig<k>` lists its parts at their offsets (`. = offset`): the runs of original bytes of
`room.c` (input sections `.room.<rva>`) and the C objects, each followed by an `ASSERT` of its end. `ScrapC.ld`
collects any `.sd.` section left over into `.sd.unplaced` and stops the link when it is not empty (an SD_AT object
outside `scrapcode_sa`).

datagen.py stops the build when a C object

- lies outside the data (in .text, .rsrc or .reloc) or on the import directory or the import address table, which
  `main.c` fills
- overlaps another one or is not at a multiple of its alignment
- does not hold what the original holds there: every byte must be the same, but for pointers. Where the original has
  a pointer (.reloc), the C object must have one too, to the C function the hook of the original target jumps to
  (`src/hooks.inc`, the CRT entries of `imports.inc` and `crthooks.inc`), to the real Python symbol, or to another C
  object at the original target. An absolute address without a relocation (a fixed rva of the room) is compared as
  bytes. A bss object needs zeros in the original.

The build logs what is left of the original per section: its bytes built in (and how many of them are not zero) and
what the C objects cover.

`GAME_VFTABLE(name, rva)` (`include/scrap/game.h`) is the address of such an object: in ScrapC.exe the C object
`name` (declared in the macro, so no header lists them), in the hook DLL `Game_Ptr(rva)`.

### Stage 3b1: the vtables (done)

`tools/standalone/vtables.py -w` writes `src/game/Global/Vtables.c`. It finds the vtables from the RTTI of the exe:
every pointer in .rdata to a complete object locator (signature 0, a type descriptor in .data) is followed by a
vtable, whose slots are the relocated pointers into .text after it. Nothing points into the middle of one. The
other code pointers in .rdata are tables of the CRT (the initializers, the exception and unwind tables, the catch
blocks of the GOG code) and the D3DX8 interfaces, which have no RTTI. Each class has one vtable (all locators have
offset 0). The objects are named after the class of the type descriptor
(`cWorld_vftable`, namespaces joined by `_` as in `gog_GoGScrap_AuthListener_vftable`, the 7 templates by hand), each
slot is the C function its hook jumps to (`crt_purecall` for `_purecall`).

298 vtables of game classes with 5083 slots (20332 bytes) are C now, room.bin went from 891623 to 871291 bytes. A
virtual call of ScrapC.exe goes straight to the C, not through the hook jump at the original rva, so a hook switched
off in scrapdecomp.ini still leaves int3 at its rva but no longer stops its virtual calls.

Every slot has a C function, none points to an unhooked rva. vtables.py stops on such a slot; the choice would then
be a stub, a library function or a remaining jump thunk (the rva of the original), per slot.

What stays bytes of the original:

- the vtables of the C++ runtime in the copy: `type_info`, the 11 std exceptions and the 6 name nodes of its
  undecorator (18 vtables). The C throws nothing and calls none of them
- the RTTI itself: the complete object locator before each vtable, the type descriptors and the class hierarchies
- the D3DX8 interfaces, no C calls them

The 292 vtable defines of the C (`CAI_VFTABLE`, `VT_FXSPARKS`, `cFile_vftable`, `g_CAppVftable` ...) are
`GAME_VFTABLE` now, pointers instead of rvas: `Game_Ptr(CAI_VFTABLE)` became `CAI_VFTABLE`, and the 17 helpers that
took the rva of a vtable take the pointer. vtables.py checks that each `GAME_VFTABLE` names the vtable at its rva. The
hook DLL behaves as before, only those helpers add the load address at the caller now.

A test that compares a vtable slot with an rva of the original sees the C function in ScrapC.exe. `isfn(p, rva)` of
`tools/test/py/sdlib.py` accepts both (the C function is the target of the hook jump at the rva); the check of the fake
entity in `sdlevel_efxchar.py` uses it.

On 2026-10-28 `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios, and so does the hook DLL
difftest.

## Stage 4: English names

The Spanish names of the original stay until the C is complete, then everything is translated.
