# Removing the Python object library

This is the plan for the second half of removing Python (see Removing Python in scripts.md). The interpreter stages leave a build where no Python code runs. The C ports of the game scripts still work on Python 1.5.2 objects: `PyObject` values, module dicts, the `Sf_` frame helpers, `PyMethodDef` tables and the engine's script API. This plan moves them to native C state and types, module by module, and ends with python152 out of the build.

The survey numbers are from branch objlib-0 (base script-loader f2f57b62). The worktrees of nopy-3 (172 level .mod files), nopy-s4 (Init, string, NewsPanel, PCMenu, CharsNPC) and nopy-6b (`TestExpr.c`, regress in the repo) add to them.

## Survey

### Size

103 files in `src/game` use `PyObject`: 60 in `src/game/Script`, 22 in `src/game/Script/levels`, 16 in `src/game/PyInterface`, 3 in `src/game/Global`, 1 in Scorer and 1 in World.

The ports are 51124 lines (`src/game/Script`) plus 29242 lines (levels). The engine script API is 10231 lines (`PyInterface`). It has 289 functions in 11 modules (Scrap 115, SScorer 35, SNet 25, SAI 22, SInput 20, SSound 17, SWeap 15, SVec 12, SLogic 12, SAct 10, SFX 6) and the entity handle type `cPyEntity` (6 methods, getattr, setattr).

Python API uses in `src/game` and `include/scrap`:

- `PyObject` 17364
- `PyArg_ParseTuple` 1481
- `PyArg_ParseTupleAndKeywords` 129
- `Py_XDECREF` 1125
- `Py_ReturnNone` 793
- `Py_BuildValue` 529
- `PyMethodDef` 504
- `Py_None` 478
- `Py_DECREF` 311
- `PyInt_FromLong` 304
- `PyString_FromString` 256
- `PyString_AsString` 145
- `PyErr_Clear` 115
- `PyCFunction` 113
- `PyString_Check` 109
- `PyTuple_New` 90
- `PySequence_GetItem` 87
- `Py_INCREF` 84
- `PyImport_ImportModule` 76
- `PyFloat_FromDouble` 76
- `PyErr_SetString` 69
- `PyList_New` 67
- `PyErr_Occurred` 61
- `PyObject_IsTrue` 58
- `PyObject_GetAttrString` 55
- `PyNumber_Add` 54

There are 268 distinct Py identifiers in all.

The port helpers on top of the API (`script.h`) are used this often: `Sf_Ask` 2362, `Sf_None` 1790, `Sf_Run` 1725, `Sf_Attr` 1293, `Sf_Import` 1264, `Sf_Item` 1051, `Sf_True` 955, `Sf_Int` 899, `Sf_Global` 857, `Sf_Compare` 750, `Sf_SetAttr` 708, `Sf_Build` 661, `Sf_Own` 590, `Script_Keep` 585, `Sf_SetGlobal` 557, `Script_Try` 540, `Sf_Entity` 536 and `Sf_Op` 480. The ports have 2809 method table entries and 708 `SF_ARGS` functions.

### Traffic by name

These numbers were counted on the preprocessed sources. Calls with literal module and function names are counted, so macros like `G(name)` are resolved. Calls through variables are not counted.

- The ports make 3561 calls into the engine modules (Scrap 1982, SScorer 416, SAI 174, SVec 164, SAct 158, SSound 117, SInput 92, SLogic 39, SNet 29, SWeap 16).
- They make 1686 calls into other ports and 636 calls into their own module (its globals, which a mission may replace). 134 calls go to library modules (`string`, `__builtin__`).
- Module variables are read 3221 times in the own module, 755 times in another port and 53 times in the engine. They are written 660 times in the own module and 557 times in another port.
- Data texts that name `Module.name` (`Sf_Calls` tables, `"%Module.name"`, `"^Module.Func:"`, callbacks) name other ports 1479 times, the own module 532 times and the engine 143 times.

The most called ports are listed below as calls and texts from other files, with the number of calling files in parentheses: Scene 847 (11), Speech 392 (32), MissionsFuncs 372 (28), SaveGame 238 (32), Menu (MenuBase) 181 (45), Parking 173 (18), Init (InitLevel) 166 (49), the HUD (MenuHud) 128 (30), CharsNPC 127 (16), Traffic 102 (20), Doors 97 (21) and MakeChar 97 (7).

Shared state is counted as variables read or written from other files: Init 165, Parking 151, Speech 115, Menu 78, Scorer 59, Traffic 57, CharsNPC 56, ShipMaker 48, HangarTab 47, RaceMaker 40, CharConversor 40, ShipEdit 36 and MissionsFuncs 36. The missions and super deals keep their state to themselves (0 to 4 foreign accesses each).

Nothing calls the pure callers from other files. They are the level files, NetModes, MenuMain, MenuMission, MenuNet, MenuVideo, Chat, InMap, Weapons and Characters.

### By role

(a) The engine's script interface is the 11 modules and `cPyEntity` above. Most functions are thin wrappers over C that already exists (`SNet.SendChatString` over `Net_SendChatString`, `SScorer.AddChatMsg` over `Scorer_AddChatMsg`, `Scrap.Get` over `GVar_Find`). Some keep logic inline (`GVarValue`, the two argument forms of `SScorer.Set`, the order of variable types in `PyEntity_GetAttr`).

(b) The C ports are 75 files that hold the C functions of 223 module names (`script_modules` in `Script.c`, `level_modules` in `Levels.c`; Map and MapSnd count once for all levels). Their state is in the module dicts. They call the engine and each other by name (`Sf_Ask`, `Script_Try`), with Python argument tuples.

(c) Loader and data are the .mod files (134 here, with 1541 `def`, 952 `let`, 159 `call`, 14 `if` and 129 aliases `let X = Func`), the .def files (246, with 670 `Module.Func` names and 111 callback lines), `Script_RunFile` with a dict of `$names`, `ScriptModule.c` (the .mod evaluator makes `PyObject` values), `Levels.c`, `Script_ReplaceRefs` and `cPy_Function_Rebind`.

(d) Saves are C strings already (`SaveVars.c`). The ports read and write them by name (`GetSaveVar` 246, `SetSaveVar` 312, `DelSaveVars` 54, `IncSaveVar` 22, `CreateSaveVar` 14, `Script_SaveVar` 99). They keep Python reprs in them (lists, numbers, quoted strings) plus the 22 code strings of the super deals and missions (`mf_codes`). 69 sites read values back (`Script_EvalValue`, `Script_EvalIn`, `Sf_Eval`, `Script_Literal`) and 316 sites make reprs.

(e) Callbacks by name: `cPy_Function` binds `"Module.Func"` to a Python object. This covers 51 `GVAR_SCRIPT_FUNCTION` engine variables, 9 input events, entity callbacks, action class events and scorer item callbacks. The ports set them with `GVar_SetStr` (10 literal sites), `Scrap.Set` by name (28 sites), `SScorer`/`SInput`/`Scrap` `SetOnCancel`, `Bind`, `SetInputFunc` and `SetCallFunc` (24 sites), and from the .def files. Scheduled functions keep a `PyObject` and an argument tuple (325 sites: `Sf_Later`, `Sf_Schedule`, `Script_Later`, `Script_LaterIn`). The engine already has a registry of C callbacks: `"!name"` (`cPy_NativeFunc`, parameters in a memory file).

(f) The test channel and the console. `TestChannel.c` has 1631 lines, 108 `PyObject` and 170 API uses. nopy-6b adds `TestExpr.c`, 2241 lines on `PyObject`. Expressions, check, dump, print and recorder use `PyObject`. The commands get, ent, ents, menu, click and con are plain C. The console (`ConsoleCmd.c`) is plain C apart from the Python fallback that stage 7 of the interpreter removal takes out.

(g) Networking: the online modes send texts made with repr and read with eval (Ffa 19 sites, Split 11, NetModes 3, Vehicles 3, Net 2, Weapons 1; `SNet.SendUsrString` 6 calls). Chat sends `"<receiver>\t<text>"`. `SNet` is called by name 29 times.

### Python semantics the ports rely on

- Errors are Python exceptions that stop the function where the script stopped (`Script_Try`, `Sf_` frames, `Sf_Except` with 9 try blocks, `PyErr_Print` 29). They carry the texts of the interpreter: the TypeError of `PyArg`, IndexError, the NameError of a deleted global, "error return without exception set", and the recursion limit through `Script_CallObject`.
- Dict iteration follows the 1.5.2 hash order: "keys" 31 and `.keys()` 5 in the ports and the .mod files, `PyDict_Next` 11, `PyDict_Keys` 3.
- Identity and sharing matter. Lists are held by several modules or put into entities (the `EntityList` of Bureaucracy), and lists are changed in place.
- Number rules: an int is a C long (64 bit on Linux, 32 bit on Windows, like the build's Python). Int division and `%` round down. Float repr and str both have 12 significant digits (`PyFloat_AsString`, with ".0" after an integral number). The 17 digits are the form of the golden test's `check()` and of the channel's dump. Mixed types compare by type name.
- Classes and instances: `PyClass_New` 6, instances 3 sites, `PyFunction_Check` 6. Keyword functions: 108.
- Functions can be replaced: 590 `Sf_Own` calls (a mission may put another function under the name), `Script_Hook` (5 chains that the net weapons and Vehicles put in front of the callbacks of Net) and the 129 .mod aliases.

### What must stay

- Save files. The text of every save variable the ports write stays byte for byte, and old saves of the original game keep loading (the literal reader and the 22 code strings stay).
- `Scrap.cfg`. It holds engine variables, including script function names `"Module.Func"`.
- Network. The message texts of the online modes and of the chat stay byte for byte, so ScrapC talks to the original exe. The multiplayer rework may change that later, on its own plan.
- Mods. The .def and .mod files in mods/ (scrapc/...) keep their formats and names. Module and function names used in data (`Module.Func`) stay as the script names.
- Test data. The regress baselines, golden's stored output and the sweep tables compare text. The C must print values the way Python 1.5.2 repr does as long as these stores are the reference.

### Tests and the object library

The checks themselves observe Python objects. 414 case files in rd/ read and write module variables (`Init._mod`, setattr, `Module.var[i] = x`). They replace engine functions with recorders (`_sd` swaps `Scrap.AddScheduledFunc` in 2913 case lines, `_bhooks` 273, `Scrap.GetNetFlags` 8, `SAI.GetNearestItemLife` 6) and port functions (77 recorder sites, among them `Speech.AddRemoteCom`, `MissionsFuncs.OnSuperDealFinish` and `RaceMaker.ShowSystemMessage`).

A port that calls the engine in C and keeps its state in C must stay visible to these cases. Two transitional pieces do that (see the pilot). The module state is mirrored into the dict, and engine functions that a case replaces are called through a seam that sees the replacement.

## Stages

Each stage keeps behaviour. Transitional code carries a comment marker `OBJLIB <n>` with the number of the stage that removes it, as `STAGE 7` marked the interpreter sites. The stages start after nopy-3, nopy-s4 and nopy-6b are merged, because they touch `ScriptModule.c`, `Script.c`, `script.h` and the channel.

### Stage 1: the native layer

After this stage there are `include/scrap/scriptnative.h` and `src/game/Script/ScriptNative.c` with the native value, the error convention, the engine helpers with seams and the dict mirror. The Python 1.5.2 repr and literal reader work on native values (the C texts of `Script_Literal` and repr without `PyObject`), and the channel reads native values. The pilot did the first part (values, errors, seams, mirror) for Chat.

- The value is `nativeVal_t` (None, int as C long, float, text, vector). Lists and tuples of values come with the repr and literal reader. Texts are fixed buffers (`NATIVE_TEXT`) where the texts of the scripts are short, and owned buffers otherwise.
- `Native_Raise(kind, text)` reports errors and returns 0. While Python is linked it sets the Python exception of that name, so the adapters keep every error text. `Native_Index` and `Native_IntOfText` give the list index and `int()` rules of 1.5.2.
- The seams are marked OBJLIB 7. `Native_Seam(module, name)` tells whether `module.name` is still the engine's own C function. If a test put something else there, the port calls that the old way. The game itself never replaces engine functions.
- The mirror is marked OBJLIB 5. It is a table of the module's variables (`nativeVar_t`). `Native_Import` at the entry points takes the dict values into C, and `Native_Put` writes C changes through at once (lists in place). The .mod file still sets the initial values, and other ports and the cases still read and write the dict.
- The size is small: one file and a header plus the repr and literal reader, about 1000 lines with tests.

Branch objlib-1 did the second part. The value now also has long ints (as decimal text), lists, tuples and dicts, and texts of any length with 0 bytes (owned buffers from `NATIVE_TEXT` bytes on). The dict is the hash table of 1.5.2 (string, int, long, float and tuple hash, `lookdict` probing with the polynomials, growth at 2/3), so keys come out in the order 1.5.2 gives. `Native_Repr` writes repr, str and the `check()` form. `Native_Literal` reads every form `Script_Literal` reads. `Native_ToPy` and `Native_FromPyAny` convert whole values for adapters. The channel reads and prints native values with the command `native`. The check is `tools/test/litfuzz.sh`: 5 seeds of 3200 random texts on the nopy-4 corpus plus 5 on a wider corpus (33780 texts, 15279 taken, and the reprs of those again), plus 77 repr cases, all with 0 differences in text, type and value, in build-linux64 and under ASan with no reports.

Known limits left for later stages:

- The dict has no delete yet. A Python dict with deleted keys keeps its order when converted, but grows from fewer used slots than Python's.
- `Native_ToPy` of a dict inserts in slot order, so Python's table can order two colliding keys differently. This matters at stage 6, when `Script_Literal` goes over to `Native_Literal` (marked OBJLIB 6).
- The literal reader repeats the token rules of `Script_Literal` in `ScriptValue.c` until then.
- The hash of None is the address of Python's None (OBJLIB 9), and the size of a Python dict is read from the struct of the vendored `dictobject.c` (OBJLIB 9).
- A long int of 3840 bits or more has no hash.
- How win64 prints inf and nan was not compared, the fuzz runs on Linux.

### Stage 2: native module cores

After this stage every port is typed C inside. Its functions take C arguments, keep module state in a static struct, call the engine's C functions (3561 call sites) and return C values and `Native_Raise` errors. The `PyMethodDef` entries are adapters: `PyArg_ParseTuple`, `Native_Import`, the C call, and the result as a Python object. Calls between ports stay by name, because the adapters are the boundary. Imports, `Script_RunFile` and the calls of other ports are marked OBJLIB 3 or 6.

- The order is free, because each module is independent at this stage. The work starts with the leaves and pure callers (Chat is done, then Net, InMap, Fx, Bureaucracy, Metro, CreditsText, CharAct, the sound and character tables, the menus, the level files). The hubs follow (Speech, MissionsFuncs, SaveGame, Menu, Init, Parking, RaceMaker, Scene, Traffic, Doors).
- The engine's C entry points get a header each where they are missing. Most exist (`GVar_*`, `Scorer_*`, `Input_*`, `Net_*`, `Entity_Var*`, `World_FindEntity`, `SaveVar_*`).
- The Python number and string rules that matter for results are kept in helpers (`Native_IntOfText`, floor division, repr of floats) instead of per site.
- The `Sf_` frame and `Script_` value helpers lose callers module by module. `ScriptFrame.c` and `ScriptValue.c` go at the end of stage 3.
- The size is the bulk of the work: 80000 lines of ports. The pilot took Chat (482 lines) in one step with its checks. Parallel module agents fit here, one module file per agent, each with its own worktree and slots.

#### State of stage 2 (2026-11-07)

These modules are native C inside now, with adapters in the `PyMethodDef` tables.

Merged into script-loader:

- Chat (the pilot).
- Batch A: InMap, Fx, Bureaucracy, CreditsText, Metro, Characters, Sounds.
- Batch B: Net, NetModes with the N* modes, Ffa, Split with the S* modes.
- Batch C: Weapons and the ten weapon modules, Items, MisItems, CrazyWing, PhoneCab, DTritusDesktop, HullTab.
- Batch D: the six super deal modes in `SuperDeals.c`.
- Batch E: WeaponsTab, EnginesTab, InPolice, OutPolice, InTraffic.
- Batch F: the levels OM, Small, QF, ABIndoor, Fake, Menu, AB, Junkyard, LevelCommon, Temple, GDB and Bank.

Done on branches and being merged now:

- Batch G: Orbit, Townhall, GamblinDen, Scrapyard.
- Batch H: Commercial, Police, Press.
- Batch I: DownTown, Outskirts.

Running: batch J with Traffic, OutMap, Challenge, CharConversor, RaceMaker and Doors (with Elevator and Teleport).

Left are the big hubs: Init, InitLevel, Parking, Speech, the Menu modules, Scene, MissionsFuncs, SaveGame, CharsNPC and Alarm. They have the most shared state, so they are the hard part of the stage.

#### Where the shared code lives

- `ScriptNative.c`: the native values, the Python operators (compare, `+`, `%`, `int()`, slices, `in`, `len`, unpack, with the 1.5.2 error texts) and the calls into the engine and the seams.
- `ScriptModes.c`: the helpers for modes and levels (`Mode_` functions for globals, lists, dicts, handles, keyword calls and seams). Each batch has its own section at the end of the file.
- `levels/Levels.h` and `levels/LevelCommon.c`: the `LvN_` helpers of the level files. Several of them are `#define`s of the `Mode_` ones now.

A helper that only one file needs stays a static function in that file. A second user moves it to the shared files.

#### Rules the batches learned

- A variable of another module that can hold a module or any other object must be read as `NV_OBJ` or by its truth. Batch C read `Init.SuperDealModule` as a native value in MisItems. During a super deal it holds the deal's module, so every ship that died below the atmosphere raised a TypeError and dropped no box. Batch D found it with the regress of the Domination case. The fix takes the truth of the object, as `Sf_True` did.
- A list that holds entity handles arrives as one `NV_OBJ`. Walk it with `Mode_Loop` or `Mode_Elems`. `Native_ForCount` refuses it, and a tuple of handles (`CreateFakeChar`) needs `Mode_Elems` before `Mode_Unpack`.
- `Mode_VGet` on a Python object that is no entity handle fails when the attribute holds a function, an instance or a handle. Use `Mode_VObj` there (AI profiles, `CharRes` objects).
- Long ints in `*`, `/` and `%` go through `Mode_Arith`. The plain C operators give other results.
- Native locals that a failed call earlier in the same condition can leave unset start as `{ 0 }` or None. `Native_Free` on an uninitialized value crashed Bank, and Scrapyard crashed at `Ambush.IsValidPos` the same way. Batch H fixed two more failure paths of this kind.
- `Script_SetGlobal` takes over the reference it gets. Releasing the value after it crashed AB.
- Values that `Mode_G` and `Mode_VGet` return are not freed by them. Free them before the next read into the same slot, or use `Mode_Read`.
- Read globals and engine calls in the order the script does. Python evaluates `GetTime() + Rand()` before the function name, so a C argument order can change which error comes first.
- A `try` block clears the Python error for the lines of the script's `try` only, not for earlier ones.
- Engine functions that a case replaces or records go through a seam (`SSound.LoadSound` through `Mode_LoadSoundSeam`). A direct engine call hides them from the case. `grep` the cases for the functions a module calls before choosing seam or direct call.
- Slot tags that start with `p` make `rd/msw-run.py` run the pack scripts. Base runs of C builds use `c` slots, or the base side is a pack run.
- Parallel batches add shared helpers as helper-only commits in their own section of `ScriptModes.c` or `ScriptNative.c`, and each commit builds alone. The next batch takes them by cherry-pick. Check `git log <base>..objlib-2<x> -- ScriptModes.* ScriptNative.*` before writing a helper.
- Forked sub-agents that write the same worktree caused trouble (half converted files, held slots). Batches work alone, or the forks get one file each and their own slots.

#### How the batches were checked

Each batch compared a build of its base commit against the build after it. The checks were:

- Raw replies of the regress cases of the touched modules, base against new, with floats and heap addresses masked. The pack baselines are loose, so the raw replies are the main evidence. What differs must be live state (entity numbers, AI states, random traffic models, idle animations, timing), and a rerun of the base binary must differ the same way.
- Edge files `rd/fc-ol2<batch>-<name>.txt` for the paths the stored cases miss: odd argument types, long ints, 0 bytes in texts, deleted and odd module values, recorded or swapped engine functions, arity and keyword errors. Base and new must be identical line by line.
- Mission starts (`rd/msw-run.py`) for the missions and super deals of the batch, base against new, exact lines equal apart from live entities and random values.
- Saves both ways: a save of each build loads in the other, and the states match.
- The online pair test (`rd/r8g.py`) where a module runs online (Fx, Net, Ffa, Split, NetModes, Chat, the net weapons).
- Golden (9961 checks), sweep (29 of 29), ASan runs of the cases and edge files (0 reports), and 0 warnings in build-linux64, build-linux64-asan, build-win64 and build-py64.

Things the checks could not cover are listed in the batch hand-backs: levels that do not start in either build (Nano, TucZone), `Mission_Demo_HumphreyRace` (no .pyc in the pack), and engine fatal checks that end the game in both builds. These were checked by compile and review only.

### Stage 3: direct calls between ports

After this stage the 1686 by-name calls between ports are C calls of the typed cores. Functions that a mission, a mod .mod line or another port replaces at run time become explicit C hook pointers in the module struct. They come from the `Sf_Own` sites, `Script_Hook`, the .mod aliases and the recorders of the cases. After this stage only data and engine callbacks call by name.

The chosen design is the port seam described under "Design of stage 3" below, not hook pointers in the module structs.

- The order is callees before callers. The hubs come first (Speech, MissionsFuncs, SaveGame, Menu, Init, Parking, Scene), and the level files and NetModes come last.
- The recorders of the cases on port functions (77) need hook pointers that can be set from the channel.
- The size is medium. The work is mostly mechanical once stage 2 is done.

#### Design of stage 3 (batch 0, 2026-11-08)

Stage 3 does not use hook pointers in the module structs. The chosen design is the port seam. A call site names the function it calls through a macro, and a runtime check decides per call whether the C entry runs or the old call through Python runs.

The call forms are macros in `include/scrap/scriptnative.h`:

- `PORT_CALL(Module, Func, out, fmt, ...)` calls `Module.Func` through the module attribute, as `Native_CallMod` did.
- `PORT_OWN(Module, Func, out, fmt, ...)` calls through the global of the module, as `Native_CallGlobal` did. A deleted global gives the NameError "global name" as before.
- `PORT_LCALL` and `PORT_LOWN` do the same for a module of a level (Map, Mission_X). Their ref is named by a `PORT_LEVEL` define in the file, the level of its `Levels.c` entry (for example temple).

The `fmt` string and the arguments stay those of `Native_Call` (v, i, d, s, O). Each site expands to a static `portSite_t` (a C23 static compound literal) and a call of `Port_Invoke` (`src/game/Script/ScriptPort.c`, declared in `include/scrap/scriptnative.h`).

`Port_Invoke` looks the attribute or the global up as before, with the same import and the same AttributeError or NameError. When the result is a C function without self whose `ml_meth` is the adapter of the ref, the fast path runs: the native entry on packed arguments (OBJLIB 5). In every other case the slow path runs (OBJLIB 7): `Native_CallVa`, the exact call through Python of today, with the same `va` arguments. A recorder, an alias, a function of a mission or a mod, and a deleted or non-callable attribute behave as before. At stage 5 the variable registry can set a hook pointer and the lookup goes. At stage 7 the slow path goes.

On the fast path the arguments are packed as the adapter would get them: "v" with `Native_Pass`, "O" with `Native_Arg`, "i", "d" and "s" directly. The depth check of `Script_CallObject` (recursion limit and stack floor, same RuntimeError) is shared through the new `Script_Enter` and `Script_Leave` in `ScriptValue.c`. An entry that returns 0 without an error gives the SystemError "NULL result without error in call_object", as `PyEval_CallObjectWithKeywords` does. The result goes through `Native_Pass` too, also when the caller drops it.

An entry has the signature `byte <adapter>_E(const nativeVal_t *a, int n, nativeVal_t *out)`. `MODE_FN` and `LV_FN` make it next to the adapter, and the adapter is `Mode_Args` plus the entry. The entry checks the count with the TypeError text of `PyArg_ParseTuple` ("function requires exactly N argument(s); M given", `Port_Arity`). The const signature built with 0 warnings, because all cores already take const arguments. Hand adapters (phase A) get a hand-written entry with the same name pattern and form.

A `portRef_t` holds module, name, adapter and entry. The refs do not come from `MODE_FN`, as the plan first said, for three reasons. The C names of 795 adapters differ from their Python names (InitPy, InitA). One adapter serves several modules or names (SuperDeals, Weapons, Press, Alarm). Level files repeat module names (Map, Mission_TalkToDeepThroat in temple and scrapyard). So `tools/script/ports.py` makes the refs from the `PyMethodDef` tables of the preprocessed sources (`compile_commands.json` of build-linux64), their registrations in `Script.c` and `Levels.c`, and the entries it finds. It writes `include/scrap/scriptports.h`: one `PORT_DECL` line per ref with the argument count of the entry, and a `PORTS_<file>` list per file. Each file with refs ends with a `PORTS(<file>)` line that defines its refs where the static adapter and entry are visible. There are 1723 refs in 51 files now. A name that is in two tables of one module gets no ref (`Vulcan.Init` and `LoadResources` in `Weapons.c`). `scriptnative.h` includes `scriptports.h`, so a caller needs no new include. The option `--check` tests that the header is up to date and every `PORTS` line is there, and `--lines` adds the missing `PORTS` lines. A merge conflict in the header is resolved by running the tool again.

`tools/script/portcalls.py` reports and rewrites the sites. By default it lists per file the by-name calls left by kind (port, own, global, noentry = a C table function without an entry yet, engine, lib, other), the `PORT` sites and the OBJLIB 3 marks, and then runs the arity check (the count of the `fmt` at each site against the count of the entry). After batch 0 the totals are: port 438, own 71, global 610, noentry 546, engine 401, lib 43, other 423, PORT 41, OBJLIB 3 marks 229, and 0 arity mismatches. `sites Mod.Func` lists every site of a function. `rewrite Mod.Func [files]` rewrites the plain sites (`LV_CALL`, `LV_OWN`, `Native_CallMod`, `Native_CallGlobal` and file aliases with a literal or fixed module such as `CALL`, `OWN`, `SM_OWN`). `wrap [files]` prints the functions for the fol3 recorder.

`Native_Pass` (`ScriptNative.c`) gives without Python what `Native_ToPy` and `Native_Arg` give. Vectors become tuples of floats, and dicts are built again in their slot order, so colliding keys and dicts with deleted keys come out exactly as the round trip gives them. The exception the plan expected does not happen. Values that hold an `NV_OBJ`, long ints not in plain digits, and containers nested deeper than `Native_FromPyAny` reads (32) take the Python way (OBJLIB 9). `litfuzz.sh` checks the packer: it packs every taken value and every repr case through the new channel function `_native_pack`, as it is and with its 3-float tuples as vectors, plus values with objects or deep nesting. There were 60470 packs on build-linux64 (5 seeds of 3200, plain and wide) and 9608 under ASan, with 0 differences and 0 ASan reports. A packer broken on purpose (vectors kept, dicts copied) gave 17 differences, so the check sees such faults.

Two pieces help to debug (OBJLIB 7). `SCRAP_PORT_SLOW=1` sends every call the slow way, so the same binary can be compared with itself (the regress runner passes the environment to the game). The counters per call site (fast, slow, errors) are listed by the channel command `native ports`, and `native ports reset` zeroes them. For the arguments, `helpers/fol3.py` and `channel/fol3.tc` have `_pton(names)`, which wraps port functions in a pass-through recorder (name, arguments, result or 'error'), `_ptlog()` and `_ptoff()`. Modules that are not loaded are skipped. A wrapped function makes the sites take the slow path, so fol3 checks the arguments the converted sites pass, base against new.

The pilot is `MissionsFuncs.SetMissionTarget` at all 41 sites in 13 files (`ScriptModes.c`, `LevelCommon.c`, `SuperDeals.c` and ten level files), rewritten with `portcalls.py rewrite`. The checks ran on the RTX with the channel harness: 43 regress cases near the sites plus the edge file `fc-ol3-0` (Temple, kept in the scratchpad of batch 0). The counters show 36 of the 41 sites reached by the cases (38 with the edge file), each with only fast calls in the plain run and only slow calls under `SCRAP_PORT_SLOW=1`, with the same totals; the slow calls of the plain run are the cases that record SetMissionTarget. Raw replies, base against new and new against slow: 28 cases identical, the other lines are live state (idle animations, entity numbers, AI timing, random taunts). Two lines kept their split in the first reruns (fc-r6b-cm3 TakeData, fc-r7d-sy6 OnStupidMercDeath); five more runs of each binary gave both outcomes in base, new and slow. The edge file (attribute deleted, None, an int, lambdas of the wrong arity, a raising lambda, other C functions of the port and of the mission, a recorder, fol3 on two functions and a missing module, arity errors of the caller) is identical line by line base against new and new against slow, on both harnesses. The sites ABIndoor.c 147 and DownTown.c 285 and 2035 were not reached and are checked by compile and review only. Golden gives 9961, and the four builds have 0 warnings.

For the next batches, phase A writes the hand entries and runs `ports.py`. Phase B uses `portcalls.py rewrite` for the plain sites and converts the rest by hand. Keyword calls, `Script_Try` and the name helpers (`LvN_OnEnd`, `LvN_ScorerCallG`) still need a form (B1). A file with level-module own calls needs `#define PORT_LEVEL <level>` first. `Small.c` serves three levels and needs care.

### Stage 4: callbacks by name through a C registry

After this stage there is one C table of named functions: module and name to a C adapter on native arguments, for the engine modules and the ports. `cPy_Function` binds `"Module.Func"` to its entry, with the `"!name"` registry of the engine as the model. The scheduler keeps an entry and native arguments. Menu and input callbacks, `SetOnCancel`, scorer item callbacks, entity and action class callbacks, the 51 engine function variables and the 9 input events all go through the table. The names stay `"Module.Func"`, because saves, `Scrap.cfg`, data and mods use them.

- `Script_ReplaceRefs` and `cPy_Function_Rebind` become unnecessary, because the registry knows the C function from the start.
- The size is medium. The scheduler and `cPy_Function` are small. The work is the argument conventions of each callback.

### Stage 5: module variables without dicts

After this stage a C registry of module variables (module, name, type, place) replaces the dict mirrors. Everything that reads variables by name uses it: the .mod `let` lines fill the C variables (mods keep working), the .def `$` and `"%Module.name"`, `Script_NameChain`, the save code strings, and the channel (dump, get, set, and the case lines that read and write module variables). `Native_Import` and `Native_Put` go.

- The channel needs to be on the registry first, because the cases read the variables there.
- The size is medium. The hubs with shared state (Init, Parking, Speech, Menu, Scorer, Traffic, CharsNPC) are the work.

### Stage 6: data paths native

After this stage the .mod evaluator (`Mod_Eval`, `Script_Literal`) makes native values. `Script_RunFile` takes native `$names` and native steps. Module loading is the C module table, with no `PyImport`. The save variables are written with the C repr and read with the C literal reader (same text). The network message texts of the online modes are made and read in C (same bytes). `Lang_GetEntries` and `SaveGames_GetList` return C arrays.

- The checks are saves both ways (a save of the build before the stage loads after it and the other way round, with the files equal apart from `GameplayTime`), one original save on the PC, and the online pair test (`rd/r8g.py`) against the stored runs.
- The size is medium.

### Stage 7: the engine script API tables go

After this stage no `PyMethodDef` is left. The engine modules (`ScrapPy.c` and the others) become entries of the stage 4 registry on native arguments, used by the channel and by data callbacks. `cPyEntity` goes, and entities are names or entity refs in native values. The adapters of the ports go, and with them the Python-only errors (the TypeError of argument parsing, arity, the NameError of deleted globals, the recursion limit). `Native_Raise` becomes a C error state that prints the message where the script printed its traceback. The seams become C hook pointers that the channel recorder sets.

- Baseline lines that only showed Python-only errors are accepted as changed (see the decisions).
- The size is medium: 289 engine functions, each a small adapter already.

### Stage 8: test channel and tools native

After this stage the channel parses its expressions into native values and calls the registry (`TestExpr` on `nativeVal`). It answers in the 1.5.2 repr text. Dump and get read the variable registry, and recorders set C hooks. The regress cases are channel cases (`tochannel.py` of nopy-6b). The 1481 API checks of golden run as channel checks on the registry. The interpreter checks of `sdpy.py` are gone with stage 7 of the interpreter removal.

- Parts of it come earlier: stage 5 needs dump and get on the registry, and stage 7 needs recorders on C hooks.

### Stage 9: python152 out of the build

After this stage `src/python` and `third_party/python-1.5.2` are out of `CMakeLists.txt`, `python.h`, `PyPool` and `pymempool.c` are deleted, and no Py identifier is left in `src/game` or `include/scrap`. The link proves it. Golden, sweep, levels, regress, saves and the online pair test are the final check.

## Checks per stage

- regress (`rd/regress`, later `tools/test/regress`): run the cases of the touched modules after each module, and the full run per group of modules. Where the pack baselines are loose (masks, wildcards), compare the raw C replies before and after on the same slot. The pilot's replies were identical line by line.
- golden: 9961 in the reference build (`build-py64`), with 1481 API checks and the interpreter checks. In the default build it is 1481 now. After step B of the interpreter removal it is the API part only.
- sweep 29 of 29 (action tables after each level start) and `levels.sh` 29 of 29 under ASan.
- Mission starts: the mission start sweep of round 8 (`missionmaker.py` starts) for stages 2 and 3 on mission modules.
- Saves both ways for every stage that touches a save writer or reader (stage 2 on SaveGame and the missions, and stage 6).
- The online pair test (`rd/r8g.py`, real server and client) for Ffa, NetModes, Net, Chat, Split and the net weapons.
- Edge cases per module: a case file with the paths the stored cases miss (bad arguments, negative indices, odd values the cases can put into module variables), run on the build before and after (`rd/fc-ol0-chat.txt` for Chat).
- 0 new warnings in build-linux64, build-linux64-asan, build-win64 and build-py64.

## What the test channel needs

For stages 1 to 6 it needs nothing new. The channel's `py` command and the Python harness see the mirrored module variables and the seams.

For stage 5 it needs dump, get and set of module variables through the variable registry, and a native form of the case helpers that read module variables (`Init._mod`, `_sdmod`).

For stage 7 it needs recorders on the C hooks of the seams and of the port hook pointers, recording native arguments in the same repr text. The `_sd` scheduler recorder is the biggest user.

For stage 8 it needs expressions on native values, names through the call registry, and answers in the 1.5.2 repr text.

## Decisions

These six decisions are taken for now. The user can still change any of them.

1. Python-only errors. The adapters keep every Python error text until stage 7. Then the errors that only a test, the channel or a broken mod can cause (wrong argument types, arity, a deleted module global, the recursion limit, "error return without exception set") become console messages, and the baseline lines that show them are accepted as changed. Keeping them would mean keeping a Python-like type system.
2. Dict order. Where the scripts iterate a dict, the C keeps the order 1.5.2 gives as a fixed list. The order depends on the string hash and the table size, not on anything at run time. The lists are checked against the current dumps. Sorting instead would change behaviour in places (spawn order, menu order).
3. Network texts. The online and chat messages stay byte for byte, so ScrapC and the original exe play together. This holds until the multiplayer rework decides otherwise.
4. Golden after stage 7. The 1481 API checks call the engine's script functions. They stay as channel checks on the native registry with the same expected values, so golden stays a test of the engine.
5. Integer width. Python ints are C long, 64 bit on Linux and 32 bit on Windows, and the native code keeps that during the move, so results stay equal to the baselines. A switch to the original's 32 bit can follow in one later commit if wanted.
6. Parallel agents. Stage 2 splits by module file. After stage 1 is merged, 2 to 4 module agents work in parallel, each with its own worktree, build dirs and slots, like the script port rounds.

## The pilot: Chat

Chat (`src/game/Script/Chat.c`, 482 lines, `scripts/scorer/chat.py`) has the pieces every module has: module state (`SendID`, a list of four receivers, and `MyShip`), engine calls, network texts, callbacks by name and a .def file. No other port reads its state, and no other agent works on it.

- State is the struct `chat` (`nativeVal_t sendID[4]`, `myShip`), mirrored into the module dict through a `nativeVar_t` table. The adapters that read `SendID` take it in, and every C change is written through.
- These engine calls are in C: `World_FindEntity`, `Entity_IsType`, `Entity_Var*` (Descriptor through `Native_EntVar`), `GVar_GetStr`, `Input_Bind`, `cPlayerInput_SetInputFunc`, `Scorer_Get` and `cPy_Function_SetFunc`, `Script_SetStr` and `Script_SetNum`, `Con_Print`. These go through seams, because fc-chat records or swaps them: `SNet.SendChatString`, `SScorer.AddChatMsg`, `Scrap.GetNetFlags`, `SNet.GetMyClientShip`.
- These stay by name and are marked: `Menu.BackToGame` (OBJLIB 3), the imports of Menu and string, and the `$names` dict of `chat.def` (OBJLIB 6).
- The adapters and the core raise the same exceptions with the same texts as before: the IndexError of `SendID`, the `int()` of the receiver number, the TypeError of string `+` and of `string.split`, and "unsliceable object".
- Known differences exist only for values that Python code can put into the module. A `SendID` item that is no text, a `SendID` that is no list of four, a text over 127 characters, or a `MyShip` that is no number, text or None raise a TypeError at the entry instead of failing later in the script's way. A line over 511 characters is cut (the network takes 128).

The checks of the pilot:

- fc-chat (the stored pack baselines) passes, and the raw replies are identical to the build before.
- fc-r8t-chat and the edge file `rd/fc-ol0-chat.txt` are identical line by line between the build before and after. The edge file has 55 lines: Car Descriptor, negative ids, `int()` forms, non-text arguments, and flags and client ship swapped. The same runs under ASan give 0 reports.
- Sweep gives 29 of 29, golden gives 9961, and there are 0 warnings.
- The online pair test (`rd/r8g.py`, DeathMatch, a real server and client) reaches the native paths that the cases record. The chat steps (to all from each side, a private line) give the same results and the same chat lines in both game logs before and after. The other differing lines of that run are in the join and spectate steps, the timing races that round 8g reported.
