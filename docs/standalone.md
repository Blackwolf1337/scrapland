# Standalone build

Goal: a `scrap.exe` built only from the C code of `src/` and real libraries, with no code of the original Scrap.exe.
It is reached in stages, each one runs the difftest.

Since stage 3c2 ScrapC.exe is a normal exe with no byte of the original, and neither ScrapC.exe nor its build needs
Scrap.exe. ScrapC.exe needs only the game files: the packs and fmod.dll of the game's DLLs in game/Bin (it imports
neither Galaxy.dll nor steam_api.dll since the store code was removed). It runs without ucrtbase_ms.dll, which only
gives the exact math under Wine. The original exe is needed only for the optional check (`cmake --build build --target
datacheck`, stage 3c2) and for the original run of the difftest.

## Stage 0: wipe check (hook DLL)

`[general] wipe=1` in scrapdecomp.ini (`DIFFTEST_WIPE=1 tools/test/difftest.sh`) fills the rest of every hooked
original with int3 after the hook jump. A jump into the middle of replaced code stops the game, and the log says
`wipe: original code reached at rva ...`.

## Stage 1: loader with the data of the original

Stage 3c2 removed the room, the fixed base and the switches described in stages 1 to 3c1, see stage 3c2.

`ScrapC.exe` (`src/standalone/main.c`) has the base 0x400000 of the original. `cmake/ScrapC.ld` puts the
uninitialized section `.orig` at rva 0x1000, as large as the original image, and the own code after it (under Wine
the low memory is full of mappings, so the room must be part of the exe). At start it reads the user's Scrap.exe
1.6.1 from game/Bin, copies its sections to their rvas (the rvas of `GAME_DATA` stay valid, no relocation), points
its own resource directory to the original's, fills the import table with GetProcAddress (dinput8 from the system
folder; until stage 3c1a), gives the original its TLS block (the MSVC thread init epoch), installs the hooks and runs the entry point
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
(`src/game/Global/Gog.c`, gone since the store code was removed, see below).

What `tools/standalone/uncovered.py` still lists is code that nothing reaches: no call, jump or pointer of the exe
(code or data) and no rva in the C points to it. These are functions the compiler kept after it inlined every call,
the jump thunks of unused imports and pieces of hooked functions behind their hook size (only the original body
jumps there), besides the switch and byte index tables.

`wipe=3` is a probe for this work: the same int3, but each trap logs its rva and `[esp]` (the caller when the trap is
the start of a function) and copies the piece of original code there back, so one run lists every piece of game
code without a C version that the run reaches.

The seven functions with Sleep or busy waits are C too, with intended behavior changes (each marked in the code
with a comment): the frame (`CEngineApp_Render`) has no Sleep loop as its frame limiter, `Engine_FrameDue` holds the
next frame back in the PeekMessage loop of `App_WinMain` (no own limiter with vsync); the device reset has no
Sleep(250); `Timer_GetQPFreq`, `Timer_MeasureQP` and `Timer_CountTicks` take QueryPerformanceFrequency instead of
measuring; `Tsc_CpuFreq` measures rdtsc against the performance counter between two calls; `Video_WaitKeysUp`
starts a hold under which the video loop and `Keyboard_Poll` ignore Escape, Space and Enter until they are up or
the time is over. `tools/test/py/sdcall_nosleep.py` checks them (some only in the hooked run).

A second round removed the other Sleep calls and blocking waits from the C, each marked "differs from the original
on purpose" in the code. `Splash_Show` (`ScrapEngWorld.c`) draws the splash again instead of Sleep(10). Its loop
stays, because every caller goes on right after it. In the light map compiler (`Bsp.c`), `BspTxf_Save` no longer
sleeps 10 s and retries a failed write: the next load finds the TXF missing and computes it again. `BspTxf_Load`
no longer waits up to 30 s for another computer of a distributed compile. It deletes that file and rebuilds it here.
In the fmod code (`MusicFmod.c`), voices open without FSOUND_NONBLOCKING, so `VoiceRemain` knows the length right
after `PlayVoice`, and `Voice_Wait` is empty. `Music_Stop` closes a stream that is still opening later, from
`Music_Update` (a list of 8 slots), because FSOUND_Stream_Close fails on it. `CWinApp_Frame` (`WinApp.c`) returns at
once while the app is inactive, instead of Sleep(250), so the PeekMessage loop then runs at full CPU. `CD3DApp_Run`
uses PeekMessageA instead of GetMessageA while inactive. `CInputDInput_ReadKeys` tries Acquire once per call
instead of spinning. `win32.h` no longer has `exe_Sleep`. `tools/test/py/sdcall_nosleep2.py` checks the voice and
music paths.

The three other blocking places run from the message loop now. `Transport_Wait` polls the select with a zero timeout,
and the rest of the dedicated server's frame runs from the PeekMessage loop of `App_WinMain` (`CApp_FrameResume`)
once a packet came or 1 s passed.

Until the store code was removed, the start also waited for the GOG sign-in from the message loop. `CWinApp_Init` now
runs `CApp_Init` directly, and the message loop only waits for its videos (`CWinApp_InitStep`). See "No store code
(GOG Galaxy, Steam)" below.

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

While the app waits (a video, the rest after a video), `App_WinMain` takes only the messages outside the keyboard and
mouse ranges, so the window can be moved or closed, but the input comes after the wait as in the original (it handled
no message then). A WM_QUIT during a video is kept until the wait is over, a line entered in the
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
print only written memory now (`Table_New` at rva 0x2c2c60, the trail points after `SetMaxTrails`).

What still differs from the exe: `sys.executable` names ScrapC.exe.

`tools/test/py/sdpy.py` (prefix `spy`) runs at the end of sdtest.py and compares the interpreter itself with the exe's:
int, long and float arithmetic at the edges (overflow checks, shifts, division, pow), `int()`, `long()` and `float()`
of strings, float formatting with all the % formats and str/repr (inf, nan, denormals, 1e16..1e22), strings and the
strop module, string formatting with width and precision, `hash()` and the key order of dicts, list.sort with compare
functions, cmp of mixed types, marshal bytes and round trips, the regex module, the math functions at their edges,
string.py of the packs, exception messages, eval and exec, and compile errors with their line numbers. Only the
ScrapC.exe run (`DIFFTEST_STANDALONE=1`) compares it; in the hook DLL both sides run the Python of the exe. What it
found in the Python itself is fixed in wrappers of `src/python` that include the vendored file. `intobject.c` uses the
exe's integer division also for the final modulo of `pow(x, y, z)` (the exe calls its i_divmod at rva 0x2de700, which
MSVC folded into one idiv, so `pow(-2147483648, 1, 7)` is 5 and not 2). `floatobject.c` and `complexobject.c` do on
the x87 stack what the exe does there: the product of the frexp result with 2**31 in float_hash and complex_hash, and
the quotient `1./powu()` of float_pow for a negative integer exponent. Direct3D sets the x87 precision to 24 bits when
it creates the device, so these are single precision in the game: `hash(0.1)` is 1717888640 and `10.0 ** -3` is
0.0010000000474974513. The x87 helpers are in `src/python/pyx87.h`. The other differences came from Wine's ucrtbase,
see stage 2b.

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
git). There is no Wine DLL override. Only the math comes from that DLL now. Without `ucrtbase_ms.dll` under Wine the
math comes from Wine's ucrtbase.

The number formatting and parsing are C of our own, they no longer come from the DLL. `src/standalone/crtfmt.c` is the
printf core (`__stdio_common_vsprintf`, `__stdio_common_vfprintf`) and holds the strerror texts.
`src/standalone/crtscan.c` is the scanf core (`__stdio_common_vsscanf`), `strtod` and `atof`. `src/standalone/crtnum.c`
(with `crtnum.h`) does the exact conversions between decimal and binary with a small fixed size big integer, integer
arithmetic only, so the x87 precision of the game does not change a result. `src/standalone/crt.c` calls them for calls
without a locale object. Calls with a locale object stay with the linked UCRT.

They were written from the documented behavior and the observed output of Microsoft's UCRT, not from its code. They
give what the original's static UCRT gives, on Windows and Wine, with or without `ucrtbase_ms.dll`. The reference is
UCRT 10.0.10586: the rich header of Scrap.exe names VS 2015 Update 3 (build 24215), and the `ucrtbase_ms.dll` from
`tools/test/getucrt.sh` is the same version. Later UCRTs changed some of it (printf rounds ties to even, strtod reads
long digit strings and some ties correctly), so ScrapC.exe follows 10.0.10586. fprintf formats straight into the FILE
of the linked UCRT, in pieces of 512 bytes. The text before an error stays written, like the runtime's. strtod and
scanf set the errno of ScrapC.exe directly. atof leaves errno alone, like the runtime's atof. strerror has the UCRT
texts for 0 to 42 (15, 26, 35, 37 and every other code give "Unknown error") and copies the text into a buffer of the
thread, like the runtime.

The runtime does some things that the C standard does not tell, and ScrapC.exe does them too:

- printf rounds the exact digits half away from zero (`%.0f` of 0.5 is 1, `%.2f` of 0.125 is 0.13). `%a` rounds up
  only when the first dropped hex digit is above 8. '#' with precision 0 puts the point after the first character and
  its digits, also in the inf and nan texts (`'%#.0e'` of inf is "i.nf", `%#.0a` of -inf is "-0x.inf"). The MSVCRT
  option (8) spells 1.#INF, 1.#QNAN, 1.#IND and 1.#SNAN and rounds them like digits (`%.2f` of inf is "1.#J"). An
  integer precision stops at 512 digits. A float precision above 163 takes a buffer of 2 * (precision + 349) bytes,
  and without one the runtime goes on with 163 digits and ENOMEM. The format is a state machine where a character that
  does not fit the state is written as it is (`%y` gives y). Only `%n`, the size N, a second size and L, w or T with an
  integer are invalid (EINVAL, -1).
- strtod and scanf read the first 768 significant digits and drop the rest. A number with more than 1088 digits after
  the point is zero (ERANGE in strtod). An exact tie that the runtime reaches through its division of a two word
  number by a one word one rounds up (64976.431640625 read as a float, ties of doubles near 2^44). scanf converts to a
  float straight from the digits, with no double between. A number that stops in an incomplete state ("1e+", "0x")
  fails instead of being cut back. sscanf reads signed chars, so the byte 0xff reads as EOF and a byte above 0x7f is a
  negative wide character. Integers are read into 64 bits and saturate with ERANGE. scanf sets no ERANGE for floats.

Wine's own versions are the reason for the C of our own. They print denormals rounded wrong, read them wrong
(`float('1e-310')`), take "1d5" as a number but not "nan(ind)", give "-nan(ind)" for `float('-nan')` where the original
gives "-nan", print `'%#g' % 0.0` as 0.00000 and `'%#.0e'` of inf as "inf" (Microsoft's gives 'i.nf'), and have other
error texts ("Numerical result out of range" for "Result too large"). sdpy.py found these. A differential fuzz harness
outside git (a mingw exe that loads `ucrtbase_ms.dll` and compares) found no difference on 2026-10-30 in 2.56 million
printf cases (all option values 0 to 31, buffer sizes, flags, widths, precisions up to 1200, every size and
conversion, edge doubles and NaN payloads), 240000 fprintf cases, 2.56 million strtod and atof cases (long mantissas,
exact and near halfway cases, hex floats, inf and nan forms, junk tails), 1.12 million scanf cases (all options, sets,
widths, sizes, buffer sizes of the secure variant) and strerror of -5 to 200. Not matched: a precision of about 2^30 or
more, where the result depends on the free memory.

The difftests on 2026-10-30: with `ucrtbase_ms.dll` sdtest 9965, sdcall 29078 and sdlevel 19095 checks match. Without
it about 380 sdtest, 1030 sdcall and 115 sdlevel checks differ, all in math results (Wine's pow, atan, sin and the like
at the x87 precision of 24 bits that Direct3D sets, and the console lines of the math errors that follow). None differs
in formatting or parsing. `en3hash` of `tools/test/py/sdcall_eng3.py` returns a long now, and `exheff` of
`sdlevel_exhaust.py` passes the motor colors through `plain`: a texture hash and a color equal to the address of a C
object of the new ScrapC.exe printed as the original's address, since `sdlib.py` maps every int through
`SDTest.Orig`.

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
- the shader assembler `D3DXAssembleShader` (`src/lib/d3dx8/D3dxAsm.c`): a hand-written parser of the grammar of
  the Berkeley yacc parser in the exe (no copy of its tables; it runs the rules and reads the tokens in the same
  order, so errors match), its lexer and rules, the validation through d3d8.dll. The game assembles the 30
  pixel shaders of `bmp/*.psh` (ps.1.1); `tools/test/py/sdcall_d3dxasm.py` compares their tokens and the errors of
  bad texts.
- descomp_afin.cpp (`src/game/engine3d/Engine/AffineDecomp.c`): Shoemake's polar decomposition of Graphics Gems IV
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
  exactly, and 14 DDS files and the TGA with the requests of the game through `CTexture_CreateD3D`: format, size and
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
  CTable freed (6), a DList cleared with its free list (2)

The 10 initializers and 5 destructors that fit no pattern, and 9 destructors that belong to C written by hand (their
initializer or the function of their static), are C by hand in the files of their classes (the HAND table of the
tool): the costs of the Swarm, the Steam ticket callback,
the four pads, the chat lines, the cPy_Function of the sphere tests, the paths of the exe and the engine, the shader
switch and the identity and bias matrices, the box of `g_PaintAll`, the faces of CheckBBox, the trace caches of the
lights, and the destructors of g_ActionSets, g_MatList, g_FileLog, g_TexList and the function statics.

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
.data, .gfids, .tls, _RDATA, .data1 and .rsrc (until stage 3b3). The zero tail of .data, .bss_ste and the code range stay
uninitialized, .reloc is left out (fixed base).

Of .text only the pieces that are still read or run are built in:

- the data formats of dinput8.lib at 0x001000..0x002070 (the `c_dfDIKeyboard` and `c_dfDIMouse` in .rdata point there)
- the import thunks of the game in the CRT (`ORIG_CRT_THUNKS` of `origcrt.h`, until stage 3c1a)

Until stage 2d the dynamic initializers at 0x03e800..0x05f6d8 and, from 0x37b3d0 to the end of .text, the C++ unwind
funclets and the atexit destructors were built in too. The C reads no other data in .text: its `Game_Ptr` and
`GAME_DATA` rvas below 0x3a1000 are hooked functions or .bss_ste.

`cmake/ScrapC.ld` includes `room.ld`, which puts the sections `.orig0`, `.orig1` ... from rva 0x1000 to 0x55e000:
initialized ones where there are bytes, uninitialized ones without file data for the gaps (the code, the tail of
.data, .reloc). That is 890 KB of data of the original in the exe, 690 KB of it non-zero.

At start `main.c` fills the rest of .text with int3 (`ROOM_INT3` of `room.h`), fills the import table of the
original with GetProcAddress from its import directory (until stage 3c1a) and points its own resource directory to the original's
(until stage 3b3).
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

### Stage 3b3: the resources as .rc (done)

The game loads its dialogs, menus, accelerators and icons from its own module (`LoadAcceleratorsA` of table 200 in
`App_WinMain`, `LoadIconA` 101, `CreateDialogParamA` 153 of the scene dialog ...). ScrapC.exe has them as its own
resources now: `src/standalone/scrap.rc`, compiled by windres (object library `scrapres`, so the C options of
`scrapcode_sa` do not reach windres), with the binary ones in `src/standalone/res` (two .ico, one .bmp, the
manifest). `main.c` no longer touches the resource directory, and `datagen.py` leaves .rsrc out (`built_in`), so its
range of the room is uninitialized: room.bin went from 871291 to 660731 bytes.

`tools/standalone/rsrc.py -w` writes them from the .rsrc of the original; without `-w` it checks that the files in the
repository are what it would write. The original has 23 resources: one bitmap, 6 icon images in 2 groups, 3 menus,
7 dialogs (all DIALOG, none DIALOGEX), 3 accelerator tables and the manifest, in English (US) and Spanish (modern
sort). No string table, no version info, no cursor. The .rc keeps their types, ids and languages. Dialogs, menus and
accelerators are written out, the styles as the names of their bits (`WS_`, `DS_`, `BS_` and so on, `NOT` for the
`WS_CHILD | WS_VISIBLE` that windres adds to a `CONTROL` where the original lacks it). The predefined control
classes are numbers (`0x80 /* Button */`): windres writes a class name as a string, the original has the ordinal.
windres numbers the icon images 1, 2 ... in the order of the `ICON` lines, which are the ids of the original;
rsrc.py stops when that would not hold, or on anything it does not write exactly (DIALOGEX, MENUEX, creation data of
a control, named resources).

`tools/standalone/rsrccheck.py <Scrap.exe> <ScrapC.exe>` runs after each link of ScrapC.exe and compares the
resources by type, id and language. 22 are the same bytes; dialog 153 differs only in the class name of its tree
control, which windres writes in capitals (`SYSTREEVIEW32`, window classes are found without case), so it is compared
as a parsed template.

`tools/test/py/sdcall_rsrc.py` finds each resource with `FindResourceA` (size and a sum of its bytes), loads the
menus, accelerators and icons and creates each dialog with `CreateDialogParamA`, with its caption and the texts of its
controls. `CreateDialogParamA` of dialog 163 (a `RICHEDIT` control) returns NULL in both exes, the class is not
registered at that point. The sum goes over halves of words: `-2147483648 % 65521` gives another result in the real Python of
ScrapC.exe than in the original's, where the `-xi` of `i_divmod` in intobject.c overflows (MSVC wraps, GCC does
not have to).

On 2026-10-28 `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios, and so does the hook DLL
difftest.

### Stage 3b2: the data usage map (done)

`tools/standalone/datamap.txt` is the map of the data of the original: every built-in byte of stage 3a and the bss, in
rows of rva, size, section, used by and kind. `tools/standalone/datamap.py` writes it from a static scan and the logs
of the data probe. datagen.py builds in only the rows with a user (rounded out to 4 bytes, the room's sections align
to 4), the rest of the room stays zero.

The probe: `[general] dataprobe=1` or `2` (`DIFFTEST_DATAPROBE=1|2` with `DIFFTEST_STANDALONE=1`; ScrapC.exe only, the
hook DLL logs that it ignores it) in `src/standalone/dataprobe.c`. Right before `Crt_Main` it makes the pages of the
original's data PAGE_NOACCESS: all the data of stage 3a (`ROOM_PROBE` of room.h, also what the map leaves out), but
for the resources (Wine reads some of them on the unix side of a system call, where a closed page is an error of the
call, not an exception) and the page of the import address table. A vectored handler logs the first read and the
first write of each 4 byte unit with the eip, opens the page, single steps the access with the trap flag and closes
the page again. For an eip outside ScrapC.exe it logs the module and the first return address into ScrapC.exe on the
stack (one right after a call), the C function that called the UCRT or the DLL. `dataprobe=1` logs only the first
access of each page and leaves it open. With `dataprobe=2` a page that faulted `dataprobebudget` times (default 20000)
stays open from then on and is logged as retired. The log is `game/Bin/dataprobe.log`, the difftest copies it to
`build/difftest/dataprobe-<scenario>-c.log`. Not seen: an access of another thread while a page is open for a step,
and the accesses of a page after it retired.

The difftest runs take about 20 s per scenario. With `dataprobe=2` sdtest took 25 s, sdcall 30 s and sdlevel 39 s,
11 pages retired (3a3000, 3a9000, 3aa000, 3ac000, 3ba000, 3db000, 3dd000, 428000, 431000, 432000 and 444000: vtables,
engine variables, the strings of the hot classes). With a budget of 2000000 sdcall took 110 s and sdlevel did not end
in its 240 s.

The static side of `datamap.py`:

- the rvas in the code of ScrapC.exe: game_base plus an immediate in the data (`GAME_DATA`, `Game_Ptr`), from
  `objdump -d` and the .reloc of ScrapC.exe (a relocated immediate is an address of ScrapC.exe), with the width of the
  access; `lea`, `add $rva` and indexed accesses have an unknown extent
- the rvas in the named data of ScrapC.exe (tables of rvas such as `engineIni` or `s_PathData`) where the original or
  the code of ScrapC.exe points too; the 16 bit tables of the shader assembler and a few byte tables are left out
- the numbers of the test scripts that lie in the data, 4 bytes each
- the import directory with its names and the import address table (main.c), the resources (found by id at run time)
  and the import thunks of the game
- the C objects of SD_AT (C already)
- everything a pointer (.reloc) of used data leads to, but the 11 pointers into the original's Python (datagen.py
  sets the real symbols) and the pointers before a vtable to its complete object locator, unless the C or the probe
  read them

The data is cut into pieces at every target of a pointer of the original (from its code or data), every rva of the C
and the tests and the end of an access of known width, the end of each string the original points to (its NUL and the
zeros up to 4), each GUID of the GUID tables of uuid.lib and dxguid.lib, and the edges of the sections and the C
objects. The margins: a piece with a used byte is used as a whole, so is a page the probe retired. An access of
unknown extent also uses the pieces after it, up to a pointer target from the data, a GUID, a C object, the end of the
section or a piece that only other functions of the original pointed to (the original reached the fields of a struct
or the elements of an array one by one, which cut it at its own targets). A string ends with its NUL unless the access
is indexed (`Game_Ptr(0x3bcbc8 + 8 * index)`, the names "Pad1" .. "Pad4").

The kind of a piece: bss, code, resource, import, rtti (type descriptors, complete object locators, class
hierarchies), guid, library (only the code of the original's CRT, Python or D3DX8 or only their data points to it),
else by its bytes (string, float, double, pointer table, int table, zero, unknown for ints mixed with pointers).
Neighbours of the same kind and user are one row.

Regenerate the map with `datamap.py` (it takes `build/ScrapC.exe`, `build/standalone/objects.txt` and the logs of
probe runs of that same ScrapC.exe, `--why <rva>` prints the chain of pointers that keeps a piece). Without logs it
keeps the rows the probe used in the map before. After the link of ScrapC.exe the build runs `datamap.py --check`: an
rva of the C or the tests in data the map leaves out stops the build. A new C function that reads data the map
dropped needs a new map.

The result on 2026-10-28 (sdtest, sdcall and sdlevel with the default budget, sdtest and sdcall with 2000000):

- room.bin went from 871291 to 402774 bytes, the non-zero bytes from 670 KB to 351016. Built in per section (before
  in parentheses): .text 4580 (4580), .rdata 142226 (532510) besides the 20332 bytes of the C vtables, .data 45400
  (115200), .gfids 4 (1272), _RDATA 4 (4832), .tls and .data1 nothing (65 and 2272), .rsrc 210560 (210560). Without
  the resources 192 KB of the original's data are left.
- used per kind (of all bytes of that kind): strings 119984 of 237354 in .rdata and all 24531 in .data, most of them
  names and texts of the engine variables (`Init_GVar_*`); int tables 11197 of 20656, floats and doubles 2524 of 8548,
  pointer tables 5634 of 24402, imports 9863, the dinput8 formats in .text 4208, zero 1291 of 2305, unknown 3299 of
  43533, GUIDs 192 of 20912; of the library data the CRT 7294 of 119476 (the initializer table of `Crt_Main`), the
  Python 1672 of 70392 (hot pages only), D3DX8 4 of 36065, the RTTI 140 of 36690.
- the static scan alone keeps 397676 bytes, the map 422736 (both with the resources and the C objects). 22 KB of the
  difference is the margin of the 11 retired pages, 3 KB are units the probe saw and what the pointers in them lead to
  (strings the UCRT copies, the engine globals of `Render_Shader`).

The type objects of the engine at 0x428e18 and 0x428f78, behind 2 of the 11 fixed pointers, are used by the C
(`PyEntity_New`, `ScorerItem_PyNew` and the Python library through them) and stay. The module table at 0x428d58 with
the other 9 has no user (`src/python/config.c` is the module table now), it stays only because its page 0x428000 is
hot. Of the original's Python data only what lies on the hot pages 0x428000 and 0x432000 stays (with the bytes of
`PyString_Type` and `PyList_Type`, whose pointers lead to the real symbols). The RTTI is dead but for the type
descriptor of galaxy::api::IError that `Gog_CatchError` matches a thrown object against. Of the CRT data only the
table of the initializers (`Crt_Main`) is used, of D3DX8 4 bytes.

On 2026-10-28 `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios with the reduced room, and
so does the hook DLL difftest. The game logs (log.html) of ScrapC.exe are the same with the whole and the reduced room,
but for the lines after the end of the script (the game is stopped at a varying point). The probe runs of a reduced
ScrapC.exe log no access to the data the map leaves out.

### Stage 3b4: the strings (done)

`tools/standalone/strings.py -w` writes the strings of the original as C: `src/game/Global/Strings3a.c` ..
`Strings44.c` (one file per 64 KB of rvas), one SD_AT object per string, `const char` in .rdata, `char` in .data.
Without `-w` it prints the counts and checks that every `GAME_STR(name, rva)` names the object at its rva.

A string is a run of text bytes with its NUL, padded with zeros to the next multiple of 4, that starts at a multiple
of 4 in the game's rows of the map of the kinds string, int table, float, double or zero (the map cuts strings where
the C or an inlined strcmp or strcpy of the original reads them, and calls some of the pieces ints or floats). The
original or the C points to its start or into its first 4 bytes (MSVC inlines strcmp with a compare of the first 2
bytes and reads the rest from there). It is no string when the original reads it there with a float or double
instruction (SSE or x87), when the C reads it only as another type (GAME_DATA or a pointer cast of a type that is not
char), or when it is a single character in .data (the ints of a table). The 10 strings nothing points to (texts of
CHECKs and messages whose code is gone) count when they lie in string rows only and are plain text. The one literal with
NULs in it (the filter "RTF File", "*.rtf" ... of the file dialog of `ConsoleWin_SaveOutput`) stays one object. Zero
words after a string that nothing reads become part of its array (227 arrays have a size: in .rdata the padding before
the next string, in .data the rest of a char buffer such as the 128 bytes of `g_M3dIni`). The zero word at 0x3a3228
that the C reads as a string is the empty string `str_Empty`.

The names: `str_GVar_<name>`, `str_GVarDesc_<name>` and `str_GVarValue_<name>` for the name, the help text and the
default value (or the script function) of the engine variables of `StaticInit.inc`, `str_NativeFunc_<name>` for the
script functions of the C, `str_PyMethod_<module>_<name>` and `str_PyDoc_<module>_<name>` for the method tables of the
Python modules (`SD_PY_MODULE` and `cPyEntity_methods`), else `str_<rva>`. A literal the compiler shared keeps the
first name (in that order and the order of `StaticInit.inc`). The bytes stay exact: the build has
`-fexec-charset=CP1252` with UTF-8 sources, so the letters 0xa1..0xff of the Spanish texts are UTF-8 (the same code
points in Windows-1252), every other byte that is not ASCII text an octal escape of 3 digits (the 0xa0 of the
DOS code page in "v\240lido", the soft hyphen 0xad). datagen.py compares every byte with the original.

`GAME_STR(name, rva)` (`include/scrap/game.h`) is the string as `char *`: in ScrapC.exe the C object `name` (declared
in the macro, as `GAME_VFTABLE` does), in the hook DLL `Game_Ptr(rva)`. The 175 uses of the C (the `((const char
*)Game_Ptr(rva))` macros, the `EXE_STR` and `AA_STATUS` macros, the bare `Game_Ptr` of a string and the tables of rvas
of `Fmod_ErrorString` and `cMissileEntity_VarS`, which are local tables of pointers now) are `GAME_STR`. The generated
lines of `StaticInit.inc` know only the rvas: `SI_STR(rva)` of `StaticInit.c` is `GAME_STR_AT(rva)`, which pastes
`SD_STR_<rva>`, and `src/game/Global/StringsAt.h` (written by strings.py, included by StaticInit.c in ScrapC.exe only)
defines it for each string rva of `StaticInit.inc` as the `GAME_STR` of its object. One `Game_Ptr` of a string is
left: the `INIT_CALL_ARGS` line of `Init_cVirtualKeyboard_VirtualKeyboard` (initpat.py writes the address arguments
of its calls as `Game_Ptr`). The data that points to strings (pointer tables, method tables, the descriptors of the
engine variables) stays bytes of the original, datagen.py checks those pointers at the original targets.

Left out: the strings of the libraries (their rows are kinds `library *`) and the strings inside other structures,
for stage 3b5. In the built-in data that is a single one, the name of the RTTI type descriptor of galaxy::api::IError
at 0x44226c (24 bytes, `Gog_CatchError`). The string rows of the map left besides it, 278 bytes in .rdata and 199 in
.data, are floats and ints that look like text (strings.py prints these counts). The map itself is not written again:
datamap.txt still has these rows as kind string, and datagen.py leaves out what the C objects cover.

The result: 10131 strings in .rdata (239380 bytes) and 291 in .data (24728 bytes), 3895 of them named after their use,
4970 with a user in the map. room.bin went from 407070 to 142962 bytes, by the bytes of the strings. The log of
datagen.py:

    datagen.py:   .rdata    116118 bytes of the original (65742 non-zero), 259712 in 10429 C objects
    datagen.py:   .data      20988 bytes of the original (12599 non-zero), 24728 in 291 C objects

(before: .rdata 355498 bytes of the original, 20332 in the 298 vtables; .data 45716, no C object).

On 2026-10-28 `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios, and so does the hook DLL
difftest.

### Stage 3c0: the tests by name (done)

The difftest scripts in `tools/test/py` reached the game by rva in about 12000 places (`SDTest.Call(rva, sig, ...)`,
`SDTest.Peek(rva, kind)`, `SDTest.Poke(rva, v)`, absolute addresses as `rva + 0x400000`). Stage 3c moves the data and
the functions of ScrapC.exe off the original rvas (normal C linkage), while the original run (`Scrap.exe` with
`hooks=0`) keeps them there. So the tests address the game by names, which resolve per build.

`src/core/testsyms.inc` is the table, generated and committed, sorted by key. A line is one of:

- `SD_TEST_FUNC(key, name, rva)`: a C function of `src/hooks.inc`
- `SD_TEST_CRT`: an entry of the C runtime of the original (the C of `src/standalone` in ScrapC.exe)
- `SD_TEST_OBJ`: a C object of `SD_AT`
- `SD_TEST_DATA`: data of the original, by a `GAME_DATA` or `Game_Ptr` define or a name of `symbols/globals.tsv`, with
  no C object yet

It holds only the names the scripts use: 6082 names (5090 functions, 10 CRT entries, 100 C objects, 882 data names) in
12265 uses. `src/core/testmod.c` compiles it in, so it works in the original run too. `SDTest.Sym(name)` returns the
rva of a name, and `Call`, `Peek`, `Poke` and `ConvU` take a name in place of the rva (a name the table lacks raises a
TypeError). The lookup is a binary search. One function, `TestMod_SymRva`, turns an entry into the rva: the rva of the
original in the hook DLL and, for now, in ScrapC.exe too. Stage 3c switches that one function in ScrapC.exe to the
address of the C function or object minus `game_base` (`Game_Ptr` adds it back), so the arithmetic of the tests (rva +
field offset, `0x400000 + rva`) keeps working. `sdlib.py` has `sym(name)`, which is `SDTest.Sym(name)`, and
`symva(name)`, which is `0x400000 + sym(name)`: the address as the pointers of the game hold it.

`tools/test/testsyms.py -w` rewrites the scripts and writes the table. Without `-w` it only reports and exits 1 when
scripts or table would change. `-v` lists the numbers that stay, `-l` the rewrites outside the first argument of SDTest
(tables, comparisons, arithmetic), which are the ones to look at by eye. It is idempotent. It reads the `SD_AT` objects
with their sizes from the object files in `build/standalone/objects.txt` (the COFF sections `.sd.<rva>`, as datagen.py
does), so it needs a build of ScrapC. After a merge that brings new tests or names: build ScrapC, run `-w`.

A function rva of `src/hooks.inc` becomes its C function name. A C function that backs several identical copies of the
original names the first copy by its name and the others `name@rva` (79 such keys). The CRT entries likewise
(`crt_memcpy` for 0x3197d0). A data rva becomes, in this order, the name of the `SD_AT` object there, the name of a
`GAME_DATA` or `Game_Ptr` define (the first that is not all capitals), else a name of globals.tsv (only USER_DEFINED and
IMPORTED, no RTTI). As the first argument of SDTest the name is bare (`'Str_Trim'`), elsewhere `sym('Str_Trim')`,
inside an object `sym('g_VirtualKeyboard') + 0x100`, as an absolute address `symva('cAI_vftable')`. The printed check
labels are strings and stay as they were.

A number is a candidate when it is hex, not an offset added to something else (`x + 0x24`), not an operand of
`* / % & | ^ << >> ~` and not after a unary minus. The first argument of SDTest, `0x400000 + n` and a Peek with an `@`
kind are certain addresses. Anywhere else the number must hit a name exactly or lie in an object of known size at a
multiple of 4, and globals.tsv is not used there (a color 0x80ff00 matched the RTTI of cEntity). A struct type or a
`Game_Ptr` array has no known size and reaches up to the next name, at most 0x1000 bytes.

What stays a number: 16 certain data rvas without a name (fields after a 4 byte global, such as 0x4a8314 after
`g_Watermark`, and the unnamed globals of `sdcall_render2.py`, `sdlevel_scorer2.py`, `sdlevel_scorer3.py`). Of the other
numbers in the image, 340 are in the code range without a function start, 51 in the data range without a name, 3
unaligned inside an object and 1 beyond 0x1000 into an object of unknown size, nearly all struct offsets, colors and
test values. Stage 3b5 (the globals as C objects) gives names to most of the data, a rerun picks them up.

`tools/test/sigcheck.py` (run by difftest.sh) checks that every name in the scripts is in the table. By name, the
signature must fit the C prototype (found in `include/` and `src/`, the definition before a declaration): thiscall with
`t` and its argument count, cdecl without `t` (at least its arguments with `...`), fastcall with one argument as `t`
(ecx), stdcall as `t` with a dummy this (the callee pops like thiscall). `vec3_t` by value counts 3 words, `double` and
64 bit ints 2, another struct by value is not counted. No global may be named `out`, `sym` or `symva`. It found 27
calls of 15 fastcall functions with one argument written as cdecl (`'vi'`, `'ii'`: `cInputAction_ctor`,
`MsvcString_Tidy`, `CTexture_dtor`, `CGameParticles_NumParticles` ...). They worked only because ecx happened to hold
the argument, and are `t` calls now with the same outputs.

`tools/standalone/datamap.py` resolves the names through the table when it scans the scripts for the rvas the tests use
(`numbers()` of testsyms.py: the rva of each name, and with a following `+ 0x..` also rva plus offset). The post-link
check of the build passes.

Left for stage 3c: about 10 check labels (`sdlevel_fxmisc.py`, `sdlevel_logic1.py`, `sdlevel_aichar.py` ...) format an
rva from a table with `%x`, they print the original rva only while `sym()` gives it. `isfn(p, rva)` of `sdlib.py` also
compares with the rva of the original.

On 2026-10-28 the hook DLL difftest and `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` match in all three scenarios
with the counts of before (sdtest 9965, sdcall 29078, sdlevel 19095 checks). The outputs of sdtest and sdcall are the
same bytes as before the rewrite. Of sdlevel 5 lines differ from before, in both runs alike: `SC2 cScorer_ctor` and 4
lines of `SC3` (`Get` of `WeaponIconIndex`, `UpgradeIconIndex`, `AmmoIconIndex` and `Weapon fields`) print memory no
code writes (the `Get` after a `Set` of 7 returns text bytes). It holds what the heap had there before, which depends
on the text of the scripts: with the old `sdlevel_scorer2.py` and `sdlevel_scorer3.py` and all other scripts rewritten,
the 4 SC3 lines are the old ones again.

### Stage 3b5: the globals and tables (done)

`tools/standalone/globals.py -w` writes `src/game/Global/Globals39.c` .. `Globals4e.c` (one file per 64 KB of rvas, 18
files) and `src/game/Global/GlobalsAt.h`, and rewrites the defines of the C. Without `-w` it prints what it would write
and its counts. It reads the C objects of strings.py and vtables.py and the symbols of the C functions from the build
(`-b`, default `build`), so the order after a change is: strings.py -w, build, globals.py -w, build. It is idempotent:
a second run writes the same files and rewrites nothing. After a merge that brings new `GAME_DATA` defines it is run
again. `tools/standalone/ctypeinfo.py` gives it the C types: it compiles one probe file with all headers of
`include/scrap` like scrapcode_sa (mingw-w64 i686, `-g`, `-fno-merge-debug-strings`) and reads the DWARF back from
`objdump --dwarf=info`: sizes, the members of structs and unions with offsets, bit fields, array lengths, the encoding
of base types. The same probe evaluates the counts in the comments of the arrays (`/* [9] */`, `/* 256, ... */`,
`/* MAX_FILEPATH */`, `/* 3 bodies */` after an `&GAME_DATA`).

What it reads are the claims of the C, 4883 of them: the defines of `GAME_DATA(type, rva)` (2614 defines of 2470 rvas,
20 of them of the form `(*(T *)Game_Ptr(rva))`), the pointer casts `((T *)Game_Ptr(rva))` (an array of what the cast
points to, 380 defines), the bare `Game_Ptr(rva)` of a GUID or data format (an address of unknown type), the objects
that the dynamic initializers of `StaticInit.inc` construct (1721 engine variables as `cGVar`, the native functions as
`cPy_NativeFunc`, the hash tables, vectors and arrays by the class of their constructor), the method tables of
`SD_PY_MODULE` (named `<module>_methods`), the uses of a define `M(rva) GAME_DATA(type, rva)` in its own file (the
doubles `K(rva)` of ScraplandScorer.c) and the `GAME_DATA` and `Game_Ptr` with a literal rva in the code. An array of
unknown length runs to the next claim it cannot hold as an element (a scalar of the element size is an element), the
next C object of the build, data that stays bytes, the end of its section or kept data, or a row of the map that no C
function of its first row uses. It never runs out of an object that holds it.

Each byte has one owner. The claims are sorted by rva, the larger first, then by source (a define in a header, a define
in a .c, the static initializers, the method tables, the rest). A claim inside an earlier object is a view of it: a
field of a struct (the value of an engine variable at +0x10 of its `cGVar`, the width of the viewport in `g_Viewport`),
an element of a table, a second type at the same rva. No claim overlaps the end of another one (the script function
variables needed their own type for that: `cGVarFunction` in scrap.h, a `cGVar` with its `cPy_Function` at +0x10, 0x24
bytes). Objects that would share a word (each SD_AT section covers whole words) become one group struct with the
objects as members and the bytes between them as filler members. There are 16 groups, all of byte flags such as
`g_UseVB` and `g_MeshD3DOn` at 0x431bac. The name of an object is the macro of the best claim at its start with its
type, else the name of an address the C takes there, else the first C function of the "used by" column of the map with
the rva (`cScraplandMainMenuScorer_Draw_3b11a0`), else `data_<rva>`. A name already used by C gets the rva appended.
That makes 3659 objects: 2874 of claims, 816 of rows of the map and 16 groups. 587 are `data_<rva>`, mostly constants
of the code of the original in .rdata that no C reads.

The data that no claim covers becomes objects per row of the map, word by word, typed by the kind of the rows:
`float`, `double`, `D3DGuid` for the GUIDs, `char` for text, `void *` for pointer tables, else `uint32_t`.

Every object gets its initializer from the bytes of the original, walked along its type: designated initializers for
structs and unions (the union member that holds the bytes), bit fields, char arrays as literals, floats and doubles as
literals that give the same bits (NaNs with their payload through `__builtin_nanf`), ints in decimal or hex. A pointer
of the original (.reloc) becomes the C object, C function or Python symbol at its target, through a char array alias
`sd_<name>` that `SD_SYM(name)` of shared.h declares (`SD_SYM_AS(name, label)` for a stdcall or fastcall name), so the
generated files need no prototypes: `.tp_dealloc = (void *)sd_PyEntity_Dealloc`,
`.ml_name = (void *)sd_str_PyMethod_SAct_CreateClass`. A pointer into data that stays bytes is `SD_ORIG(rva)`, the
fixed address in the room (ScrapC.exe has the base 0x400000). Only the two dinput8 data formats `c_dfDIKeyboard` and
`c_dfDIMouse` point there, to their object tables in .text. The tool stops on a nonzero byte in no member (padding) or
a pointer in no pointer field. There is none. Objects in the bss of the original (the zero tail of .data, .bss_ste)
have no initializer and use `SD_BSS(rva)` of shared.h instead of `SD_AT`. GCC gives a named section the flags of
initialized data, so the section name of `SD_BSS` ends the directive with the gas flags "bw" and comments out the ones
GCC appends. Their sections are uninitialized and take no room in ScrapC.exe.

The C forms in `include/scrap/game.h` are, in ScrapC.exe, the C object and, in the hook DLL, the data of the exe at the
rva as before:

- `GAME_GLOBAL(type, name, rva)`: the object (an lvalue, declared in the macro with its type)
- `GAME_ARRAY(type, name, rva)`: its first element (`type *`)
- `GAME_GLOBAL_IN(type, owner, off, rva)` and `GAME_ARRAY_IN`: a view at `off` in an object
- `GAME_PTR(owner, off, rva)`: an address (`void *`), and `GAME_PTR_AT(rva)` from the rva alone

The names are pasted, never macro expanded (they are the names of the defines that use them). The uses stay as they
were: `#define g_pWorld GAME_DATA(cWorld *, 0x467450)` became `#define g_pWorld GAME_GLOBAL(cWorld *, g_pWorld,
0x467450)`, `#define g_ViewportW GAME_DATA(int, 0x4ad7e8)` became `GAME_GLOBAL_IN(int, g_Viewport, 8, 0x4ad7e8)` and
`((uint32_t *)Game_Ptr(0x4ad7e0))` became `(GAME_ARRAY(uint32_t, g_Viewport, 0x4ad7e0))`. After the rewrite there are
834 `GAME_GLOBAL`, 1593 `GAME_GLOBAL_IN`, 293 `GAME_ARRAY`, 87 `GAME_ARRAY_IN` and 18 `GAME_PTR` defines. 187 defines
stay `GAME_DATA`: the 179 slots of the import address table (main.c fills it), the 9 of the Python of the original in
the hook DLL branch of python.h and `GOG_IERROR_TYPE` (RTTI; gone since the store code was removed, see below).
StaticInit.c reads its objects through `SI_OBJ(rva)`, which is `GAME_PTR_AT`: GlobalsAt.h defines `SD_DATA_<rva>` for
the 1840 rvas of StaticInit.inc. `SD_PY_MODULE` registers `<module>_methods`.

Around it, the types of ten objects moved from their .c files into headers (the BMP, TGA and TEX headers of Bmp.c into
bmp.h, `AviBih` into rendertarget.h, `traceCell_t` into ground.h, `procrange_t` and `cProcType` into e3dutil.h,
`cSaveVar` and `cSteamTicketCallback` into scrap.h; the latter is gone since the store code was removed, see below).
`g_LastError` is a `uint32_t` instead of DWORD, `BlendMode_GetSrc` and `GetDst` have one prototype type in scrapmain.h
and scrapmaterial.h, and the inline of bsp.h is `Bsp_Vec3Equal` (it had the name of the C function `Vec3_Equal` of
ICharControl.c). datagen.py accepts pointers with an offset into another
object and stdcall and fastcall names of C functions. It puts the C objects of `SD_BSS` into uninitialized sections of
their own (a unit joins the group before it when it starts in its last page, else data with data below MERGE, bss with
bss). strings.py no longer reads the generated globals as uses (it would see their rvas) and knows the new forms.

`datamap.py --relabel` changes the map for this: it gives the rows in the C++ exception tables of MSVC (each FuncInfo
with its unwind map, try block map, handler arrays and ES type list, found by its magic number 0x1993052x) the kind
`eh` and the pointers to the RTTI before the vtables (the complete object locators) the kind `rtti`, and leaves the rest
as it was. A full run would see the strings and globals as C objects, which strings.py and globals.py read as the data
they make. A full run classifies them the same way (`eh_ranges`, `kinds`). `eh` is droppable like the libraries and the
RTTI, because only the code of the original read it, and that is int3: 63448 bytes in 2249 rows, none used. Five
"strings" of stage 3b4 were ints of those tables ("@" and a tab), so Strings41.c is gone.

What stays bytes of the original (room.bin went from 142962 to 23998 bytes):

- .text: the data formats of dinput8.lib (4208 bytes) and the import thunks of the game in the CRT (372)
- .rdata: the import directory, the import address table and the import names (9863 bytes, with 195 bytes of padding
  between the names in words shared with them), the data of the CRT that the C still uses (7292, the initializer table
  of `Crt_Main`), the Python of the original on the hot pages (200), D3DX8 (4), the pointers to the RTTI before the
  vtables that are used (232)
- .data: the Python of the original (1472, with the 16 bytes at 0x432b50 in its range of origpython.h), the RTTI (140,
  the type descriptor of galaxy::api::IError that `Gog_CatchError` reads)
- _RDATA: 4 bytes of the CRT

The log of datagen.py:

    datagen.py: 14 sections, 23998 bytes of data, 11 pointers into the Python lead to the real one, 14374 C objects
    datagen.py:   .text       4580 bytes of the original (2138 non-zero), 0 in 0 C objects
    datagen.py:   .bss_ste       0 bytes of the original (0 non-zero), 66240 in 22 C objects (66240 of them bss in 22)
    datagen.py:   .rdata     17786 bytes of the original (13876 non-zero), 293552 in 11104 C objects
    datagen.py:   .data       1628 bytes of the original (992 non-zero), 711420 in 3245 C objects (667320 of them bss in 2617)
    datagen.py:   .gfids         0 bytes of the original (0 non-zero), 1272 in 3 C objects
    datagen.py:   _RDATA         4 bytes of the original (3 non-zero), 0 in 0 C objects

(before: .rdata 116118 bytes of the original, .data 20988, no C object in .bss_ste, the bss and .gfids).

On 2026-10-28 `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios (sdtest 1487 checks, sdcall
29078, sdlevel 19095), and so does the hook DLL difftest.

### Stage 3c1a: the imports of ScrapC.exe (done)

ScrapC.exe calls the DLLs of the game through imports of its own. The import directory, the import names and the import
address table of the original (10058 bytes with the padding between the names) are no longer built in, and `main.c` no
longer fills them (`Orig_Imports` is gone).

The 176 defines of DLL functions (`exe_MessageBoxA`, `g_GalaxyInit`, `g_SteamAPI_GetHSteamUser` ...) use the forms of
`include/scrap/game.h`:

- `GAME_IMPORT(type, name, argbytes, rva)`: a stdcall function, `#define exe_MessageBoxA GAME_IMPORT(messagebox_t,
  MessageBoxA, 16, 0x3a142c)`
- `GAME_IMPORT_CDECL(type, name, rva)`: a cdecl function (steam_api, `wsprintfA`)
- `GAME_IMPORT_SYM(type, name, sym, rva)`: any other symbol, the C++ names of Galaxy.dll
  (`"?Init@api@galaxy@@YAXABUInitOptions@12@@Z"`; gone since the store code was removed, see below)

In ScrapC.exe each one is the cell of its own import address table: an extern with the label `__imp_<symbol>` of the
import library, declared in the macro (quoted for the assembler, which takes no `?` in a bare name). The call is
`call *__imp__MessageBoxA@16`, through an import slot as in the original. In the hook DLL the form is the slot of the
exe at rva, as before. The types stay the game's own (win32.h needs no windows.h). A wrong argbytes is an undefined
symbol at the link.

The import libraries: mingw-w64 for kernel32, user32, gdi32, winmm, advapi32, shell32, ole32, comctl32, comdlg32, ddraw,
d3d8, wsock32, msvfw32, avifil32 and xinput9_1_0. For fmod.dll, Galaxy.dll and steam_api.dll, which mingw-w64 has not,
`src/standalone/fmod.def`, `Galaxy.def` and `steam_api.def` list the functions the original imports from them, and
dlltool makes the libraries in build/implib (the last two are gone since the store code was removed, see below).
fmod.dll exports the stdcall names with their underscore (`_FSOUND_Init@12`), so its .def lines are
`FSOUND_Init@12 == _FSOUND_Init@12`: the C symbol and the import name are the same. The original imports wsock32 (15
functions), msvfw32 (`VideoForWindowsVersion`) and comctl32 (`InitCommonControls`) by ordinal, ScrapC.exe by name, the
same functions.

The 47 FSOUND functions of `src/imports.inc` pointed at the jump thunks of the exe (0x3179c6..0x317b2e). They are
`SD_IMPORT_DLL_STDCALL` now: a thunk in the hook DLL as before, nothing in ScrapC.exe, where `_FSOUND_Init@12` is the
function of the import library. Nothing jumps to the thunks of the original any more, so `ORIG_CRT_THUNKS` is gone: the
pieces of .text that datagen.py builds in are the dinput8 data formats only, and `wipecrt=1` fills the whole range of
the CRT.

What stays as it was:

- dinput8 is no import. ScrapC.exe runs in game/Bin, where dinput8.dll is the hook DLL. `Orig_DInput8` of `main.c`
  loads the one of the system folder at the start of WinMain, before anything of the game runs, and sets
  `sd_DirectInput8Create`, which is `exe_DirectInput8Create` in ScrapC.exe (win32.h, one define for IDevice.c and
  Input.c).
- fmod.dll, Galaxy.dll and steam_api.dll are found in game/Bin, the directory of the exe, first in the search order, as
  for the original (Galaxy.dll and steam_api.dll are no longer imported, see below).
- The original has no delay load (its delay import directory is empty), neither has ScrapC.exe.
- The order of the loads. The loader binds the imports of ScrapC.exe before its start, as it did for the original
  (before this stage `main.c` loaded them in WinMain). ld sorts the import directory by the path of the import library,
  so the libraries of the game's DLLs are linked as `build/implib/<n>-lib<name>.a` (dlltool output or a link to the
  mingw-w64 library), numbered in the order of the original: XINPUT9_1_0, WINMM, WSOCK32, Galaxy, steam_api, fmod,
  MSVFW32, AVIFIL32, DDRAW, COMCTL32, d3d8 (Galaxy and steam_api are gone since the store code was removed, see
  below). kernel32, user32, gdi32, advapi32, shell32, comdlg32 and ole32 are default libraries of the toolchain and
  come before them, with the UCRT (a second path of one of them would split its import descriptor). In the original
  kernel32 .. shell32 came after steam_api, comdlg32 and ole32 last, and dinput8 third.

The map: the kind `import` is droppable (`datamap.py`), the import rows have no user any more, the 195 bytes of zeros
between the import names (only `Orig_Imports` read them, in words) are kind `import` too, and the rows of the jump
thunks in .text are gone. The scan no longer seeds the imports. `dataprobe.c` watches the page of the import address
table like the others (0x3a1000, the initializer table of the CRT starts on it). datagen.py no longer writes
`ROOM_IMPORT_RVA` and `ROOM_IAT_*` into room.h and no longer refuses C objects on the import directory or the import
address table.

The test of the resources (`sdcall_rsrc.py`) called the Win32 functions through the import table of the exe (a Peek of
the slot). It takes them from `SDTest.Proc(dll, name)` now (`src/core/testmod.c`): the address of a function of a
loaded DLL as an rva for `Call`, the same in both runs. The 10 `exe_` names left `testsyms.inc`.

What is left of the original, room.bin went from 23998 to 13568 bytes:

- .text: the data formats of dinput8.lib (4208 bytes)
- .rdata: the initializer table of `Crt_Main` (7292), the Python of the original on the hot pages (200), D3DX8 (4), the
  pointers to the RTTI before the vtables that are used (232)
- .data: the Python of the original (1472, with the 16 bytes at 0x432b50), the RTTI (140, the type descriptor of
  galaxy::api::IError that `Gog_CatchError` reads)
- _RDATA: 4 bytes of the CRT

The log of datagen.py:

    datagen.py: 10 sections, 13568 bytes of data, 11 pointers into the Python lead to the real one, 14374 C objects
    datagen.py:   .text       4208 bytes of the original (1829 non-zero), 0 in 0 C objects
    datagen.py:   .bss_ste       0 bytes of the original (0 non-zero), 66240 in 22 C objects (66240 of them bss in 22)
    datagen.py:   .rdata      7728 bytes of the original (5604 non-zero), 293552 in 11104 C objects
    datagen.py:   .data       1628 bytes of the original (992 non-zero), 711420 in 3245 C objects (667320 of them bss in 2617)
    datagen.py:   .gfids         0 bytes of the original (0 non-zero), 1272 in 3 C objects
    datagen.py:   _RDATA         4 bytes of the original (3 non-zero), 0 in 0 C objects

On 2026-10-28 `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios (sdtest 9965 checks, sdcall
29078, sdlevel 19095), and so does the hook DLL difftest. `ScrapC.exe -dedicated` starts the server (port 28086, the
level loads) with the same log.html as `Scrap.exe -dedicated` but for the time in its head line: ScrapC.exe writes it an
hour earlier, as the build before this stage did.

### Stage 3c1b: the last data of the original (done)

Of the 24 KB that stayed bytes after stage 3b5, stage 3c1a took the import part, and this stage the rest: nothing of the
original is built in any more (room.bin is empty), every byte the room holds is a C object. The C of ScrapC.exe computes
no address from an rva of the original. The hook DLL works by rva as before: every form below is
`Game_Ptr(rva)` there.

The initializer table. `tools/standalone/xctable.py -w` writes `src/standalone/crtxc.inc` from the `__xc` table of the
original (0x3a15bc..0x3a31bc): one `CRT_XC(name, rva)` line per initializer in its order, the C function the hook at the
rva jumps to (1790 of them; the zero entries and the CRT's own are left out). Without `-w` it checks the file.
`src/standalone/crtxc.c` makes the table of function pointers, and `Crt_Main` calls it. The `__xi` entries all belong
to the CRT, `Crt_InitState` sets what they set. `App_WinMain` gets `GetModuleHandleA(NULL)`.

The dinput8 data formats. The game passes `c_dfDIKeyboard` and `c_dfDIMouse` to SetDataFormat. Those of mingw's
libdinput8 hold the same objects as the original's (its 4.2 KB at 0x001000..0x002070 in .text): the same header, and
for all 256 and 7 objects the same GUID, offset, type and flags. ScrapC.exe links them (`-ldinput8 -ldxguid`, written
as flags: `dinput8` alone is the target of the hook DLL), `GAME_DF_KEYBOARD` and `GAME_DF_MOUSE` of game.h are those in
ScrapC.exe and the exe's in the hook DLL. `TEXT_KEEP` of sdexe.py is gone (no piece of .text is built in), datagen.py
starts the room with an uninitialized section when the first C object lies behind rva 0x1000.

The IError type descriptor is a C copy in `Gog.c` for both builds: `Msvc_CatchAs` compares the descriptor pointer and
the decorated name, the descriptors of Galaxy.dll are its own, so only the name counts (the copy is gone since the
store code was removed, see below).

The rest was read by nobody. `datamap.py --relabel` now also drops the users that are gone (`relabel_3c1b`, idempotent,
the same result from the map of stage 3b5 or 3c1a):

- the rows of `GONE_3C1B`: `Crt_Main` in the initializer table, the data formats (kind `library dinput8` now, also the
  two DIDATAFORMAT at 0x3cfbec and 0x3cfdf4; the rest of the old row at 0x3cfc04 is game data without a user), and
  `Gog_CatchError` in the type descriptor
- in the data of the libraries, the RTTI and the exception tables: the margins of the probe (a retired page), the
  numbers of the tests and of the tables of the C, which only look like rvas there (`0x400000`, a color 0x404040, the
  absolute address of a function through `symva`). Their code is int3, the C reads none of it. The static scan of
  datamap.py leaves such numbers out too (`LibraryData`), and `numbers()` of testsyms.py gives the rva of a `symva` name
  instead of its address
- the pointers to the RTTI that rows of other kinds held (kind `rtti` now): 56 before the vtables, nothing reads
  `vtable[-1]`, the C has no RTTI, and the 2 of a base class array at 0x410064
- the rows in the Python data of origpython.h are kind `library python` (the 16 bytes at 0x432b50)
- then, to a fixpoint, the `ptr` users from data that is no longer kept (the strings and type objects behind the module
  table and `PyString_Type` of the original's Python, `type_info::vftable` behind the type descriptors)

datagen.py stops the build when it would build in a byte of the original that is no C object.

The address uses of the C:

- `GAME_FUNC(name, rva)` of game.h is the address of a function of the exe: in ScrapC.exe the C function `name` (an asm
  label reference, so no prototype is needed; stdcall and fastcall names get their decoration from
  `--enable-stdcall-fixup`). The 215 `Game_Ptr` of function rvas (constructors and destructors for the vector
  iterators, natives of the scripts, window and dialog procedures, callbacks, qsort compares) are `GAME_FUNC` now.
  `New_Objects`, `Delete_Objects`, `Delete_VectorPart`, `Table_NewObjs` and `DeleteVecArray` take the functions instead
  of rvas, `WEAP_NATIVE` is gone, the tables of debris effects (`cDebris_SetFX`) and scorer items (`cScorer_AddItem`)
  hold `GAME_FUNC` (382 in all). `INIT_NATIVE_FUNC` of StaticInit.inc has the C function as a sixth argument (initpat.py writes it),
  the string argument of `INIT_CALL_ARGS` is `GAME_STR_AT` (strings.py defines it in StringsAt.h).
- the inline `GAME_DATA`, `Game_Ptr` and the `K(rva)` doubles of ScraplandScorer.c are `GAME_GLOBAL`, `GAME_ARRAY`, the
  `_IN` views and `GAME_PTR`. globals.py rewrites the uses in the code now too, not only the defines (each inline claim
  has its text span, and it is left alone when the claim has no object), and an rva may be the name of a numeric define
  (`GAME_ARRAY(uint32_t, owner, FXV_RAMP_FIRE)`), which the rewrite keeps.
- the helpers that took the rva of a ramp, a color table or a guard (`FXC_Ramp`, `FXV_Ramp`, `Press_Ramp`, `FXS_RAMP`,
  `Particles_Emit`, `Sirens_Flash`, `VME_Colors`, `VME_StaticBegin`, `VME_ExhaustLine`) take the pointer, and the
  `Game_Ptr(base + k * 0x24)` of a random ramp is `(uint32_t *)Game_Ptr(base) + 9 * k` before the rewrite.
- the static tables of rvas are local tables of addresses in their functions (the variables of m3d.ini in
  `Engine_LoadVarsIni`, the stores of `CircuitScorer_InitPaths`, the checkboxes of `Bsp_DlgProc`, the font globals, the LOD
  groups, the light colors of `Light_SetDefault`); `txfKind_t` of BmpFlt.c names its header by the version.
- `__security_check_cookie` of crt.c compares with `crt_security_cookie` (a define of crt.h, the C object at 0x43cfdc).
- the unused import `ActionSet_IsNamed` is gone (`cString_EqStr` hooks that rva).

The new claims changed some objects of globals.py, all checked by datagen.py against the bytes: the color tables of
the exhaust are `uint32_t[10]` (the float before each of them was its first color), the source rows of the circuit
paths are 16 byte objects, the ramp tables split where the C takes a ramp, the guards of the function statics of
VehicleMotorsEffects.c are `int` objects. An address the C takes inside an array of known length is a view of it (the
stores into `g_CircuitPaths`). A pointer of the original into data that stays bytes is an error of globals.py now,
`SD_ORIG` is gone.

The checks. In ScrapC.exe (game.h), `Game_Ptr` exists only where `SD_GAME_RVA` is defined (the rva lookup of
testmod.c, which stage 3c switches), and `GAME_DATA` not at all (after stage 3c1a only the hook DLL branches of
python.h and win32.h use it); any other use does not compile. `tools/standalone/rvacheck.py` runs after
the link of ScrapC.exe: every `GAME_FUNC` and `INIT_NATIVE_FUNC` names the C function of the hook at its rva,
`game_base` appears only in the hook installer, testmod.c, game.c, the room code of main.c and dataprobe.c, and
`SD_ORIG` nowhere.

Five checks of the tests compared a stored function pointer with the address of the exe (`sdlevel_weap2.py`,
`sdlevel_scorer3.py`, `sdlevel_net.py`, `sdcall_input.py`, `sdcall_restb.py`). ScrapC.exe stores the C function now,
so they use `isfn` of sdlib.py; their lines print 1 in place of the address.

The log of datagen.py, with stage 3c1a:

    datagen.py: 12 sections, 0 bytes of data, 2 pointers into the Python lead to the real one, 14435 C objects
    datagen.py:   .bss_ste       0 bytes of the original (0 non-zero), 66240 in 22 C objects (66240 of them bss in 22)
    datagen.py:   .rdata         0 bytes of the original (0 non-zero), 293504 in 11119 C objects
    datagen.py:   .data          0 bytes of the original (0 non-zero), 711420 in 3291 C objects (667320 of them bss in 2628)
    datagen.py:   .gfids         0 bytes of the original (0 non-zero), 1272 in 3 C objects

(after stage 3c1a: .text 4208, .rdata 7728, .data 1628, _RDATA 4). The 2 pointers into the Python lie in C objects (the
`ob_type` of `cPyEntity_Type` and `cPyMenuEntity_Type`), which hold the real symbols.

On 2026-10-28 `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios (sdtest 9965 checks, sdcall
29078, sdlevel 19095), and so does the hook DLL difftest.

### Stage 3c2: ScrapC.exe without the room (done)

The room is gone. ScrapC.exe is a normal exe: the linker places everything, and no byte of the original is built in.

Linkage. `SD_AT(rva)` and `SD_BSS(rva)` of `include/scrap/shared.h` are plain definitions in ScrapC.exe now
(`__attribute__((used))`, initialized or bss). The linker places the 14378 C objects of src/game/Global (Globals*.c,
Strings*.c, Vtables.c) like any other global, and the `GAME_*` macros of game.h give them as before. There is no room,
no `cmake/ScrapC.ld`, and no `room.ld`, `room.c`, `room.h` or `room.bin`.

ScrapC.exe has the default base of ld (0x400000) with dynamic base (`-Wl,--dynamicbase`, DllCharacteristics 0x140:
DYNAMIC_BASE and NX_COMPAT). Under Wine it loads at 0x400000 (the log line "ScrapC.exe at 00400000"). The test scripts
take 0x400000 in many places, as they do for Scrap.exe, which has dynamic base too, and sdlib.py writes a line into the
output when `SDTest.Base()` is another.

Gone from ScrapC.exe:

- the room code of main.c (`Orig_Room`, the int3 fill)
- the hook installer and the thunks of the imports (src/core/hook.c and src/core/imports.c are built into the hook DLL
  only)
- the switches wipepy, wipecrt, wipelow, wipeinit and dataprobe with `src/standalone/dataprobe.c`, and the code-gone trap

`Game_Start` of game.c in ScrapC.exe only reads scrapdecomp.ini, sets `game_base` to its own module and adds SDTest with
testmod=1. wipe is a switch of the hook DLL only (stages 0 and 1b), and difftest.sh has only DIFFTEST_WIPE and
DIFFTEST_STANDALONE now.

The entries of the original's CRT that only the code of the copy reached are gone from src/standalone/crt.c and
crtmath.c (`_alloca_probe`, `_EH_prolog`, `_setjmp3`, `longjmp`, `_ftol`, `_aullshr`, `__security_check_cookie`,
`strncpy`, `_finite`, `calloc`, `_vsnprintf`, `_snprintf`, the sized `operator delete[]`, `_CIacos`, `_CIpow`,
`_libm_sse2_sqrt_precise`). `src/standalone/crthooks.inc` stays as a table of names: no C includes it, the tools read it
to name an rva of the original's CRT (sdexe.py `c_names`, rvacheck.py, testsyms.py, initpat.py).

ScrapC.exe has a read-only .rdata now (the room was writable as a whole). globals.py no longer gives an object outside
the original's .rdata the const of a claim (`GAME_ARRAY(const byte, ...)`): `g_RSqrtTable`, `g_MatIdentity`,
`g_Frustum` and others in the original's .data and bss were written into const objects.

Tables that are one object now. The 3c1b layout had split tables where the C took an address inside them, and in the
room the C reached the rest through the neighbouring objects. They were found by a scan of the indexed accesses of the
original's code (capstone) and of the C (the relocations of the object files) against the sizes of the C objects, and
by the difftest.

globals.py: an array of a known length (a count in the comment of its claim) takes every claim inside it as a view, also
a smaller array at its start. Before, the next claim cut it: `g_BspColors` became 4 bytes, `g_CircuitFlares` 3 of 4,
`g_KeyNameBuf` 16 of 40 bytes, and the vector of 2 cStaticBuffer of `cStaticBuffer_PyStaticBuffer` 1.

Counts in the C:

- the ramp tables the C picks a ramp of 9 colors from at random (`NURSE_RAMPS_STARS` and `NURSE_RAMPS_SPARKS`, which had
  `[2][9]` read as 2, `RUSTY_RAMPS`, `VS_RAMP_FIRE`, and table defines with a count for the FX ramps of FXChar.c,
  FXCharActions.c, FXScene.c, FXVehicle.c and FXWeapons.c, such as `FXV_FIRE_RAMPS` and `FXATPC_GLOW_RAMPS`)
- the exhaust colors at 0x3b5914
- `g_EngCallbacks` (4: entries 2 and 3 are `g_LoadingDraw2` and `g_LoadingDraw`, which `Engine_SetCallback` writes)
- `g_InputCursor` (the POINT of GetCursorPos)
- `g_StageTex` and `g_StageTexFrame` (8), `g_PSHandles` (32), `g_ProfTicks` (32)
- `g_RenderCounts` (10, the frame clears 10)
- `g_BspCacheStats` (3, the third is `g_CollisionTestsMax`)
- `g_ShaderTab` (32, `Shader_Find` walks it to the NULL name)

Changes in the C where the original reached a separate global through a table. `Render_IniD3D` writes the six default
materials (`g_MatWhite` .. `g_MatGround`) through a table of their addresses instead of `(&g_MatWhite)[i]`.
`Mat_ConvertBlend` reads the legacy blend modes 1..6 from `g_BlendTable[mode - 1]` (the original indexed from the last
word of `g_MatList` before it). `MainMenu_Sparks` reads the spark waits for k >= 9 where the original read past the
letter lengths.

One case is left as it is. `Str_WrapText` of the message scorer may write up to 0xfa line pointers into `g_MsgLines`,
which holds 20. With more than 20 lines the original wrote over `g_LabelText` and the hint globals after it, and
ScrapC.exe writes over whatever the linker put after `g_MsgLines`. No message of the game has that many lines.

The check against the original (optional). `cmake --build build --target datacheck` runs
`tools/standalone/datagen.py --check <SCRAP_EXE> build/standalone/datacheck.txt` and `rsrccheck.py <SCRAP_EXE>
ScrapC.exe`. SCRAP_EXE is the CMake cache path (default game/Bin/Scrap.exe). The normal build needs neither, and a clean
build with `-DSCRAP_EXE=/nonexistent/Scrap.exe` succeeds.

The check build has the object library `scrapdata_check` (EXCLUDE_FROM_ALL). It compiles Globals*.c, Strings*.c and
Vtables.c again with `SD_DATACHECK`, which puts every object into a section `.sd.<rva>` of its own as in stage 3b.
datagen.py reads them as before. It stops when an object lies outside the data, overlaps another, is not at a multiple
of its alignment, holds other bytes or pointers than the original (the same rules as stage 3b), or when a byte the map
of stage 3b2 keeps is no C object. It writes nothing. The log on 2026-10-29:

    datagen.py: 14378 C objects match Scrap.exe 1.6.1, 2 pointers into the Python lead to the real one

with .bss_ste 66240 bytes in 22 C objects, .rdata 293504 in 11118, .data 711612 in 3235 (667512 of them bss in 2585) and
.gfids 1272 in 3. rsrccheck.py: 23 resources match.

`build/standalone/objects.txt` lists the objects of scrapcode_sa and of scrapdata_check. The generators (globals.py,
strings.py, vtables.py, testsyms.py, datamap.py) read the .sd sections from the check objects, so build the target
scrapdata_check before running them. The post-link check `datamap.py --check` of stage 3b2 is gone (there is no built-in
data to map); datamap.py stays for the generators. rvacheck.py still runs after each link, and `game_base` is allowed
only in game.h, game.c, hook.c and testmod.c.

The tests. `TestMod_SymRva` gives in ScrapC.exe the address of the C function or object (plus the offset of a view
inside it) minus the load address. The table of testmod.c takes the address from the assembler label like `GAME_FUNC`.
In the hook DLL it gives the rva of the original as before. testsyms.inc writes data names as `SD_TEST_DATA(key, owner,
off, rva)`: the C object that holds them and the offset. testsyms.py stops when a data name lies in no C object, or when
an offset after a name in a script (`sym('g_X') + 0x10`) leaves the object.

`SDTest.Base()` is the load address. `SDTest.Orig(v)` turns the start of a C object or C function of ScrapC.exe
(absolute, or as an rva for data) into the original's address. It uses `src/core/testaddrs.inc` (every C object with its
rva and size, written by `testsyms.py -w` from the check build) and src/hooks.inc (a C function that backs several
copies names the first). In the hook DLL it returns v. `fmt` of sdlib.py prints every int through it, so vtable
pointers, string pointers and table pointers in the outputs compare with the original run. Only starts count, and the
rva form only for data: colors and packed numbers hit the inside of objects and the starts of the functions too often.
The rva form is tried first: the tests print pointers to data as `p - 0x400000`, and such a value can also be the
address of a C function (the rva of `g_ExePath` was one). The guess depends on where the linker puts things: a number
that a test prints and that hits the start of a C object or function after a change of the layout shows up as a
difference of the difftest, and the test then marks it with `plain()`.
`plain(v)` of sdlib.py marks values that hold no address (the pixels of the bitmap tests, trail vertices and indices, a
hash of sdlevel_bspcalc.py, particle data of sdlevel_efxmisc.py): their ints become longs, which `fmt` prints as they
are.

`SDTest.PeekSpan(name, off, n)` and `SDTest.PokeSpan(name, off, value, n)` read and write n bytes (1..4) after a name in
the layout of the original, byte by byte through the C objects there. They are for the tests that read or wrote a word
over byte flags of separate objects (`g_TileOn`, `g_UseShaders`, `g_M3DUseOpacity`, `g_PointsOn`).

The tests that read or wrote several neighbouring globals as one block read them one by one now (the domination zones,
the mission scorer words, the radar, the music volumes, `g_DbgWnd` .. `g_DbgButtonRect`, `g_Screens` and `g_Models`,
`g_VCacheVert` and `g_VCacheAge`, the XBOX vertex shaders, the default materials and others), with tables of names in
place of `sym('A') + 4 * i`. `Render_PS0 + 0x60 * i` (the nine shader functions one after the other in the original) is
a table of names. The check labels that printed the rva of a function (`%x` or `fmt` of a `sym()`: the loops of
sdlevel_logic1..4, sdlevel_fxmisc.py, `Table_Alloc`, `Array_Read`, the Stub functions and others, about 50 loops) go over
the names and print them. `SDTest.Call` takes a name, and testsyms.py adds a name of the table that a script holds as a plain
string. `isfn(p, rva)` of sdlib.py compares p with 0x400000 + rva, which is the C function in ScrapC.exe
(sym gives its address there).

On 2026-10-29 `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` matches in all three scenarios: sdtest 9965 checks (116
raise an exception in both), sdcall 29078 (0), sdlevel 19095 (89). So does the hook DLL difftest, with the same counts
and 9957 of 9957 hooks installed. `ScrapC.exe -dedicated` starts the server (port 28086 bound, the AI graph loads) with
the same log.html as `Scrap.exe -dedicated` but for the time in its head line (an hour earlier, as before).

The size of ScrapC.exe was 24423137 bytes before (stage 3c1b) and is 24701035 after, with the debug information;
stripped 5432846 and 5265934 bytes. SizeOfImage went from 0xa62000 to 0x620000 (stripped exes): the room of 0x55e000 is
gone, and the .reloc of the dynamic base (about 180 KB) and the address table of `SDTest.Orig` came in.

## Stage 4: English names

Stage 4 translated the Spanish names of our C to English: 2625 identifiers of `src/` and `include/` and 81 files, with
the uses in the tests, the tools, `symbols/*.tsv`, `datamap.txt` and the docs. The tool is `tools/rename`, the
decisions, the tables and the proof are in `docs/rename.md`. String literals, RTTI names, engine variable names and the
paths of the original in comments did not change.

The generators `vtables.py`, `strings.py` and `initpat.py` map the names they derive from the exe through `english()`
now. `strings.py` and `initpat.py` were not idempotent and are fixed: `strings.py` read the rvas of
`src/core/testaddrs.inc` as uses of the C, and `initpat.py` knew only the defines before stage 3b5 for the names of
objects. All generators with `-w` change nothing now.

The results on 2026-10-29. `objcmp.py` finds all 882 objects of the build before the rename the same after it but for
the names (the only difference is the order of the table of `testmod.c`). `DIFFTEST_STANDALONE=1` and the hook DLL
difftest match with the counts of before: sdtest 9965 checks (116 raise an exception in both), sdcall 29078 (0),
sdlevel 19095 (89), and 9957 of 9957 hooks installed. `datacheck` passes: 14378 C objects match Scrap.exe 1.6.1 and 23
resources match.

## No store code (GOG Galaxy, Steam)

Store and account features are not wanted in the game, so all GOG Galaxy and Steam code is gone from the C (2026-11-01).
Deleted are `src/game/Global/Gog.c` (the Galaxy init and the sign-in listener, the MSVC `std::string` and vector helpers
it used, the SEH catch of `galaxy::api::IError`, `Gog_GetID`, `Gog_GetLanguage`, `Achievement_Unlock`,
`Gog_ProcessData`), `src/game/Global/Steam.c` (the steam_api context, the auth ticket, the Steam ID and the do-nothing
Steam stubs of the GOG build), `src/standalone/Galaxy.def` and `src/standalone/steam_api.def`. That is 63 hooks fewer:
9894 instead of 9957. The import library loop of CMake has no Galaxy and steam_api any more, so ScrapC.exe imports
neither Galaxy.dll nor steam_api.dll (checked with `objdump -p`) and needs only fmod.dll of the game's DLLs.

Gone with them: the form `GAME_IMPORT_SYM` of `include/scrap/game.h` (only the C++ names of Galaxy.dll used it),
`exe_RtlUnwind`, `exe_GalaxyFriends`, `cSteamTicketCallback`, `GOG_IERROR_TYPE` and five CRT entries only the store code
used (`crt_Xlength_error`, `crt_Xout_of_range`, `crt_invalid_parameter_noinfo_noreturn`,
`crt_Throw_bad_array_new_length`, `crt_ui64toa`).

The start. `CWinApp_Init` has no Steam or GOG init and no sign-in, so the wait state for the sign-in in the message loop
is gone. `CWinApp_InitStep` only waits for the videos of `CApp_Init`. Closing the window during that wait now always
quits after the wait. `CWinApp_Frame` runs no GOG or Steam callbacks. The field of `CWinApp` at 0x84 (the vector of the
GOG listeners) is `unused_84`, the size stays 0x90.

- The language is the LANGUAGE value of `HKLM\Software\MercurySteam Entertainment\Scrapland` as before, otherwise
  english, then the fallbacks of `Lang_Set` (`g_LangDefault`, Strings.txt). The original took the GOG language over the
  registry one, english in a normal install. The default of the engine variable `SelectedLanguage` ("French") is not
  used any more.
- The player name stays the one of the config. The original took the GOG or Steam persona name.
- `App_GetConfigDir` is My Documents\MercurySteam\Scrapland\ without the subfolder of the GOG id.

The Python API keeps every `Scrap.*` name. `GetGogID`, `GetSteamID`, `GetSteamTicket` and `IsSteam` return None,
`OpenStore` does nothing, `IsApp` returns 1 and `Achievement` does nothing. The argument checks and the error returns
are as before.

The hook DLL: Scrap.exe keeps its own Galaxy and Steam code, but no C calls it, and the hooked `CWinApp_Init` does not
sign in either.

The data. `tools/standalone/datamap.py` has a list `GONE_NOSTORE` next to `GONE_3C1B`, applied by `datamap.py
--relabel`. The data only the store code read gets the kind `library store` without users (droppable like the
libraries): the vtables of the Galaxy listeners and the Steam callbacks, the interface version strings of steam_api,
the hex digits of the ticket, the client id and secret of Galaxy, "ACHIEVEMENT_", "Failed to Auth: %s", the texts of
the STL errors, the steam_api context and the ticket callback object. The name in the type descriptor of
`galaxy::api::IError` is kind `rtti` now, and the bss rows of the ticket, the Steam ID, `g_GogReady` and `g_IsSteam`
lost their users.

strings.py, globals.py and vtables.py then wrote 48 C objects fewer: 30 strings, 10 globals and 8 vtables (290 vtables
with 5060 slots instead of 298 with 5083). vtables.py leaves out the classes of the store code (regex `STORE`).
initpat.py and xctable.py leave out the initializer of the Steam ticket callback and its atexit destructor (`DROPPED`),
so `src/standalone/crtxc.inc` has 1789 initializers instead of 1790. All generators are idempotent again.

Two test changes besides the removed checks:

- `sdlevel_ase.py` calls `Transport_WSAStartup` before `Ase_Initialize`. In Scrap.exe Galaxy.dll had already started
  Winsock, and ScrapC.exe loads no Galaxy.dll. The game itself starts Winsock in `Transport_Startup` before its server
  starts the ASE.
- `sdcall_multisprite.py` marks the packed coordinates of the sprite tables with `plain()`: one of them hit the start
  of a C object after the layout change.

The checks of the deleted functions left `sdcall_resta.py`, `sdcall_sa1b.py` and `sdcall_nowait.py`.

The results on 2026-11-01. `DIFFTEST_STANDALONE=1 tools/test/difftest.sh` and the hook DLL difftest match in all three
scenarios: sdtest 9965 checks (116 raise an exception in both), sdcall 28986 (0, it was 29078 before the checks of the
deleted functions left), sdlevel 19095 (89), and 9894 of 9894 hooks installed. `datacheck` passes: 14330 C objects match
Scrap.exe 1.6.1 (it was 14378) and 23 resources match. A run of ScrapC.exe with Galaxy.dll and steam_api.dll moved out
of game/Bin gives the same sdtest and sdlevel outputs as Scrap.exe. In the test prefix the sign-in of the original took
about 2.7 million passes of the message loop, the C runs no such path now.
