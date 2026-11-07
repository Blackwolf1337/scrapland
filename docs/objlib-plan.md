# Removing the Python object library

Technical outline of the second half of the Python removal (see Removing Python in scripts.md). The interpreter stages
leave a build where no Python code runs. The C ports of the game scripts still work on Python 1.5.2 objects:
PyObject values, module dicts, the Sf_ frame helpers, PyMethodDef tables and the engine's script API. This plan moves
them to native C state and types, module by module, and ends with python152 out of the build. Survey numbers are from
branch objlib-0 (base script-loader f2f57b62). The worktrees of nopy-3 (172 level .mod files), nopy-s4 (Init, string,
NewsPanel, PCMenu, CharsNPC) and nopy-6b (TestExpr.c, regress in the repo) add to it.

## Survey

### Size

- 103 files in src/game use PyObject: 60 in src/game/Script, 22 in src/game/Script/levels, 16 in
  src/game/PyInterface, 3 in src/game/Global, 1 in Scorer, 1 in World.
- The ports are 51124 lines (src/game/Script) plus 29242 lines (levels). The engine script API is 10231 lines
  (PyInterface): 289 functions in 11 modules (Scrap 115, SScorer 35, SNet 25, SAI 22, SInput 20, SSound 17, SWeap 15,
  SVec 12, SLogic 12, SAct 10, SFX 6) and the entity handle type cPyEntity (6 methods, getattr, setattr).
- Python API uses in src/game and include/scrap: PyObject 17364, PyArg_ParseTuple 1481, PyArg_ParseTupleAndKeywords
  129, Py_XDECREF 1125, Py_ReturnNone 793, Py_BuildValue 529, PyMethodDef 504, Py_None 478, Py_DECREF 311,
  PyInt_FromLong 304, PyString_FromString 256, PyString_AsString 145, PyErr_Clear 115, PyCFunction 113,
  PyString_Check 109, PyTuple_New 90, PySequence_GetItem 87, Py_INCREF 84, PyImport_ImportModule 76,
  PyFloat_FromDouble 76, PyErr_SetString 69, PyList_New 67, PyErr_Occurred 61, PyObject_IsTrue 58,
  PyObject_GetAttrString 55, PyNumber_Add 54. 268 distinct Py identifiers in all.
- The port helpers on top of it (script.h): Sf_Ask 2362, Sf_None 1790, Sf_Run 1725, Sf_Attr 1293, Sf_Import 1264,
  Sf_Item 1051, Sf_True 955, Sf_Int 899, Sf_Global 857, Sf_Compare 750, Sf_SetAttr 708, Sf_Build 661, Sf_Own 590,
  Script_Keep 585, Sf_SetGlobal 557, Script_Try 540, Sf_Entity 536, Sf_Op 480. The ports have 2809 method table
  entries and 708 SF_ARGS functions.

### Traffic by name

Counted on the preprocessed sources (calls with literal module and function names, so macros like G(name) are
resolved; calls through variables are not counted).

- Calls into the engine modules: 3561 (Scrap 1982, SScorer 416, SAI 174, SVec 164, SAct 158, SSound 117, SInput 92,
  SLogic 39, SNet 29, SWeap 16).
- Calls into other ports: 1686. Calls into the own module (its globals, which a mission may replace): 636.
  Library modules (string, __builtin__): 134.
- Module variables read: own module 3221, another port's 755, engine 53. Written: own 660, another port's 557.
- Data texts naming Module.name (Sf_Calls tables, "%Module.name", "^Module.Func:", callbacks): other ports 1479,
  own 532, engine 143.
- The most called ports (calls and texts from other files, number of calling files): Scene 847 (11), Speech 392
  (32), MissionsFuncs 372 (28), SaveGame 238 (32), Menu (MenuBase) 181 (45), Parking 173 (18), Init (InitLevel) 166
  (49), the HUD (MenuHud) 128 (30), CharsNPC 127 (16), Traffic 102 (20), Doors 97 (21), MakeChar 97 (7).
- Shared state (variables read or written from other files): Init 165, Parking 151, Speech 115, Menu 78, Scorer 59,
  Traffic 57, CharsNPC 56, ShipMaker 48, HangarTab 47, RaceMaker 40, CharConversor 40, ShipEdit 36, MissionsFuncs
  36. The missions and super deals keep their state to themselves (0 to 4 foreign accesses each).
- Pure callers (no other file calls them): the level files, NetModes, MenuMain, MenuMission, MenuNet, MenuVideo,
  Chat, InMap, Weapons, Characters.

### By role

(a) The engine's script interface: the 11 modules and cPyEntity above. Most functions are thin wrappers over C that
already exists (SNet.SendChatString over Net_SendChatString, SScorer.AddChatMsg over Scorer_AddChatMsg, Scrap.Get
over GVar_Find). Some keep logic inline (GVarValue, SScorer.Set's two argument forms, PyEntity_GetAttr's order of
variable types).

(b) The C ports: 75 files hold the C functions of 223 module names (script_modules in Script.c, level_modules in Levels.c; Map and MapSnd count once for all levels). Their state is in
the module dicts. They call the engine and each other by name (Sf_Ask, Script_Try), with Python argument tuples.

(c) Loader and data: .mod files (134 here, 1541 def, 952 let, 159 call, 14 if, 129 aliases "let X = Func"), .def files
(246, 670 Module.Func names, 111 callback lines), Script_RunFile with a dict of $names, ScriptModule.c (the .mod
evaluator makes PyObject values), Levels.c, Script_ReplaceRefs, cPy_Function_Rebind.

(d) Saves: save variables are C strings already (SaveVars.c). The ports read and write them by name (GetSaveVar
246, SetSaveVar 312, DelSaveVars 54, IncSaveVar 22, CreateSaveVar 14, Script_SaveVar 99) and keep Python reprs in
them (lists, numbers, quoted strings) plus the 22 code strings of the super deals and missions (mf_codes). 69 sites
read values back (Script_EvalValue, Script_EvalIn, Sf_Eval, Script_Literal), 316 make reprs.

(e) Callbacks by name: cPy_Function binds "Module.Func" to a Python object (51 GVAR_SCRIPT_FUNCTION engine
variables, 9 input events, entity callbacks, action class events, scorer item callbacks). The ports set them with
GVar_SetStr (10 literal sites), Scrap.Set by name (28), SScorer/SInput/Scrap SetOnCancel, Bind, SetInputFunc,
SetCallFunc (24), and from the .def files. Scheduled functions keep a PyObject and an argument tuple (325 sites:
Sf_Later, Sf_Schedule, Script_Later, Script_LaterIn). The engine already has a registry of C callbacks: "!name"
(cPy_NativeFunc, parameters in a memory file).

(f) Test channel and console: TestChannel.c (1631 lines, 108 PyObject, 170 API uses; nopy-6b adds TestExpr.c, 2241
lines on PyObject). Expressions, check, dump, print and recorder use PyObject; get, ent, ents, menu, click and con
are plain C. The console (ConsoleCmd.c) is plain C apart from the Python fallback that stage 7 removes.

(g) Networking: the online modes send texts made with repr and read with eval (Ffa 19 sites, Split 11, NetModes 3,
Vehicles 3, Net 2, Weapons 1; SNet.SendUsrString 6 calls). Chat sends "<receiver>\t<text>". SNet by name: 29 calls.

### Python semantics the ports rely on

- Errors as Python exceptions that stop the function where the script stopped (Script_Try, Sf_ frames, Sf_Except 9
  try blocks, PyErr_Print 29), with the texts of the interpreter (TypeError of PyArg, IndexError, NameError of a
  deleted global, "error return without exception set", the recursion limit through Script_CallObject).
- Dict iteration in 1.5.2 hash order: "keys" 31 and .keys() 5 in the ports and the .mod files, PyDict_Next 11,
  PyDict_Keys 3.
- Identity and sharing: lists held by several modules or put into entities (Bureaucracy's EntityList), lists changed
  in place.
- Number rules: int is a C long (64 bit on Linux, 32 bit on Windows, as the build's Python), int division and %
  rounding down, float repr with 17 digits, str with 12, compare of mixed types by type name.
- Classes and instances: PyClass_New 6, instances 3 sites, PyFunction_Check 6. Keyword functions: 108.
- Replaceable functions: 590 Sf_Own calls (a mission may put another function under the name), Script_Hook (5
  chains the net weapons and Vehicles put in front of Net's callbacks), the 129 .mod aliases.

### What must stay

- Save files: the text of every save variable the ports write stays byte for byte, and old saves of the original
  game keep loading (the literal reader and the 22 code strings stay).
- Scrap.cfg: engine variables, including script function names "Module.Func".
- Network: the message texts of the online modes and of the chat stay byte for byte, so ScrapC talks to the
  original exe. The multiplayer rework may change that later, on its own plan.
- Mods: .def and .mod files in mods/ (scrapc/...) keep their formats and names. Module and function names used in
  data (Module.Func) stay as the script names.
- Test data: the regress baselines, golden's stored output and the sweep tables compare text. The C must print
  values the way Python 1.5.2 repr does as long as these stores are the reference.

### Tests and the object library

The checks themselves observe Python objects. 414 case files in rd/ read and write module variables
(Init._mod, setattr, Module.var[i] = x), replace engine functions with recorders (_sd swaps
Scrap.AddScheduledFunc in 2913 case lines, _bhooks 273, Scrap.GetNetFlags 8, SAI.GetNearestItemLife 6) and port
functions (77 recorder sites, Speech.AddRemoteCom, MissionsFuncs.OnSuperDealFinish, RaceMaker.ShowSystemMessage).
A port that calls the engine in C and keeps its state in C must stay visible to them. Two transitional pieces do
that (see the pilot): the module state is mirrored into the dict, and engine functions that a case replaces are
called through a seam that sees the replacement.

## Stages

Each stage keeps behaviour. Transitional code carries a comment marker OBJLIB <n> with the stage that removes it, as
STAGE 7 marked the interpreter sites. The stages start after nopy-3, nopy-s4 and nopy-6b are merged (they touch
ScriptModule.c, Script.c, script.h and the channel).

### Stage 1: the native layer

End state: include/scrap/scriptnative.h and src/game/Script/ScriptNative.c with the native value, the error
convention, the engine helpers with seams and the dict mirror; the Python 1.5.2 repr and literal reader for native
values (the C texts of Script_Literal and repr without PyObject); the channel reads native values. The pilot did the
first part (values, errors, seams, mirror) for Chat.

- Value: nativeVal_t (None, int as C long, float, text, vector); lists and tuples of values come with the repr and
  literal reader. Texts are fixed buffers (NATIVE_TEXT) where the scripts' texts are short, else owned buffers.
- Errors: Native_Raise(kind, text) returns 0. While Python is linked it sets the Python exception of that name, so
  the adapters keep every error text. Native_Index and Native_IntOfText give the list index and int() rules of 1.5.2.
- Seams (OBJLIB 7): Native_Seam(module, name) tells whether module.name is still the engine's own C function. If a
  test put something else there, the port calls that the old way. The game itself never replaces engine functions.
- Mirror (OBJLIB 5): a table of the module's variables (nativeVar_t); Native_Import at the entry points takes the
  dict values into C, Native_Put writes C changes through at once (lists in place). The .mod file still sets the
  initial values, other ports and the cases still read and write the dict.
- Size: small, one file and a header plus the repr and literal reader (about 1000 lines with tests).

### Stage 2: native module cores

End state: every port is typed C inside. Its functions take C arguments, keep module state in a static struct, call
the engine's C functions (3561 call sites), and return C values and Native_Raise errors. The PyMethodDef entries are
adapters: PyArg_ParseTuple, Native_Import, the C call, the result as a Python object. Calls between ports stay by name
(the adapters are the boundary). Imports, Script_RunFile and the calls of other ports are marked OBJLIB 3 or 6.

- Order: free (each module is independent at this stage). Start with the leaves and pure callers (Chat done, Net,
  InMap, Fx, Bureaucracy, Metro, CreditsText, CharAct, the sound and character tables, the menus, the level files),
  then the hubs (Speech, MissionsFuncs, SaveGame, Menu, Init, Parking, RaceMaker, Scene, Traffic, Doors).
- The engine's C entry points get a header each where they are missing (most exist: GVar_*, Scorer_*, Input_*,
  Net_*, Entity_Var*, World_FindEntity, SaveVar_*).
- Python number and string rules that matter for results are kept in helpers (Native_IntOfText, floor division,
  repr of floats) instead of per site.
- The Sf_ frame and Script_ value helpers lose callers module by module; ScriptFrame.c and ScriptValue.c go at the
  end of stage 3.
- Size: the bulk of the work. 80000 lines of ports; the pilot took Chat (482 lines) in one step with its checks.
  Parallel module agents fit here (one module file per agent, own worktree and slots).

### Stage 3: direct calls between ports

End state: the 1686 by-name calls between ports are C calls of the typed cores. Functions that a mission, a mod
.mod line or another port replaces at run time become explicit C hook pointers in the module struct (from the Sf_Own
sites, Script_Hook, the .mod aliases and the recorders of the cases). After it only data and engine callbacks call
by name.

- Order: callees before callers. Hubs first now (Speech, MissionsFuncs, SaveGame, Menu, Init, Parking, Scene), the
  level files and NetModes last.
- Recorders of the cases on port functions (77) need the hook pointers to be settable from the channel.
- Size: medium. Mostly mechanical once stage 2 is done.

### Stage 4: callbacks by name through a C registry

End state: one C table of named functions, module and name to a C adapter on native arguments, for the engine
modules and the ports. cPy_Function binds "Module.Func" to its entry (the "!name" registry of the engine is the
model); the scheduler keeps an entry and native arguments; menu and input callbacks, SetOnCancel, scorer item
callbacks, entity and action class callbacks, the 51 engine function variables and the 9 input events go through it.
The names stay "Module.Func" (saves, Scrap.cfg, data and mods use them).

- Script_ReplaceRefs and cPy_Function_Rebind become unnecessary (the registry knows the C function from the start).
- Size: medium. The scheduler and cPy_Function are small; the work is the argument conventions of each callback.

### Stage 5: module variables without dicts

End state: a C registry of module variables (module, name, type, place) replaces the dict mirrors. Readers through
names use it: the .mod "let" lines fill the C variables (mods keep working), .def "$" and "%Module.name",
Script_NameChain, the save code strings, and the channel (dump, get, set, the case lines that read and write module
variables). Native_Import and Native_Put go.

- Needs the channel on the registry first, because the cases read the variables there.
- Size: medium. The hubs with shared state (Init, Parking, Speech, Menu, Scorer, Traffic, CharsNPC) are the work.

### Stage 6: data paths native

End state: the .mod evaluator (Mod_Eval, Script_Literal) makes native values, Script_RunFile takes native $names and
native steps, module loading is the C module table (no PyImport), the save variables are written with the C repr and
read with the C literal reader (same text), the network message texts of the online modes are made and read in C
(same bytes), Lang_GetEntries and SaveGames_GetList return C arrays.

- Checks: saves both ways (a save of the build before the stage loads after it and the other way round, the files
  equal apart from GameplayTime), one original save on the PC, the online pair test (rd/r8g.py) against the stored
  runs.
- Size: medium.

### Stage 7: the engine script API tables go

End state: no PyMethodDef left. The engine modules (ScrapPy.c and the others) become entries of the stage 4 registry
on native arguments, used by the channel and by data callbacks. cPyEntity goes (entities are names or entity refs in
native values). The adapters of the ports go, and with them the Python-only errors (TypeError of argument parsing,
arity, NameError of deleted globals, the recursion limit). Native_Raise becomes a C error state that prints the
message where the script printed its traceback. The seams become C hook pointers that the channel recorder sets.

- Baselines: lines that only showed Python-only errors are accepted as changed (see decisions).
- Size: medium (289 engine functions, each a small adapter already).

### Stage 8: test channel and tools native

End state: the channel parses its expressions into native values and calls the registry (TestExpr on nativeVal),
answers in the 1.5.2 repr text, dump and get read the variable registry, recorders set C hooks. The regress cases are
channel cases (tochannel.py of nopy-6b), golden's 1481 API checks run as channel checks on the registry. sdpy.py's
interpreter checks are gone with stage 7 of the interpreter removal.

- Parts of it come earlier: stage 5 needs dump and get on the registry, stage 7 needs recorders on C hooks.

### Stage 9: python152 out of the build

End state: src/python and third_party/python-1.5.2 out of CMakeLists.txt, python.h, PyPool and pymempool.c deleted,
no Py identifier in src/game or include/scrap. The link proves it. Golden, sweep, levels, regress, saves and the
online pair test as the final check.

## Checks per stage

- regress (rd/regress, later tools/test/regress): the cases of the touched modules after each module, the full run
  per group of modules. Compare the raw C replies before and after on the same slot where the pack baselines are
  loose (masks, wildcards): the pilot's replies were identical line by line.
- golden: 9961 now (1481 API checks with the interpreter checks); after stage 7 of the interpreter removal the API
  part only.
- sweep 29 of 29 (action tables after each level start), levels.sh 29 of 29 under ASan.
- Mission starts: the mission start sweep of round 8 (missionmaker.py starts) for stages 2 and 3 on mission modules.
- Saves both ways for every stage that touches a save writer or reader (stages 2 on SaveGame and the missions, 6).
- The online pair test (rd/r8g.py, real server and client) for Ffa, NetModes, Net, Chat, Split and the net weapons.
- Edge cases per module: a case file with the paths the stored cases miss (bad arguments, negative indices, odd
  values the cases can put into module variables), run on the build before and after (rd/fc-ol0-chat.txt for Chat).
- 0 new warnings in build-linux64, build-linux64-asan, build-win64 and build-nopy64.

## What the test channel needs

- Now (stages 1 to 6): nothing new. The channel's py command and the Python harness see the mirrored module
  variables and the seams.
- For stage 5: dump, get and set of module variables through the variable registry, and a native form of the case
  helpers that read module variables (Init._mod, _sdmod).
- For stage 7: recorders on the C hooks of the seams and of the port hook pointers, recording native arguments in the
  same repr text (the _sd scheduler recorder is the biggest user).
- For stage 8: expressions on native values, names through the call registry, answers in the 1.5.2 repr text.

## Decisions for the user

1. Python-only errors. The adapters keep every Python error text until stage 7. Then errors that only a test, the
   channel or a broken mod can cause (wrong argument types, arity, a deleted module global, the recursion limit,
   "error return without exception set") become console messages, and the baseline lines that show them are accepted
   as changed. Recommendation: yes, drop them at stage 7. Keeping them would mean keeping a Python-like type system.
2. Dict order. Where the scripts iterate a dict, the C keeps the order 1.5.2 gives as a fixed list (it depends on the
   string hash and the table size, not on anything at run time). Recommendation: fixed lists, checked against the
   current dumps. Sorting instead would change behaviour in places (spawn order, menu order).
3. Network texts. Keep the online and chat messages byte for byte so ScrapC and the original exe play together.
   Recommendation: keep them until the multiplayer rework decides otherwise.
4. Golden after stage 7. The 1481 API checks call the engine's script functions. Recommendation: keep them as
   channel checks on the native registry (same expected values), so golden stays a test of the engine.
5. Integer width. Python ints are C long, 64 bit on Linux and 32 bit on Windows, and the native code keeps that.
   Recommendation: keep long during the move (results stay equal to the baselines), switch to the original's 32 bit
   in one later commit if wanted.
6. Parallel agents. Stage 2 splits by module file. Recommendation: 2 to 4 module agents after stage 1 is merged,
   each with its own worktree, build dirs and slots, like the script port rounds.

## The pilot: Chat

Chat (src/game/Script/Chat.c, 482 lines, scripts/scorer/chat.py) has the pieces every module has: module state
(SendID, a list of four receivers, and MyShip), engine calls, network texts, callbacks by name and a .def file. No
other port reads its state, and no other agent works on it.

- State: the struct chat (nativeVal_t sendID[4], myShip), mirrored into the module dict through a nativeVar_t table.
  The adapters that read SendID take it in; every C change is written through.
- Engine in C: World_FindEntity, Entity_IsType, Entity_Var* (Descriptor through Native_EntVar), GVar_GetStr,
  Input_Bind, cPlayerInput_SetInputFunc, Scorer_Get and cPy_Function_SetFunc, Script_SetStr and Script_SetNum,
  Con_Print. Through seams, because fc-chat records or swaps them: SNet.SendChatString, SScorer.AddChatMsg,
  Scrap.GetNetFlags, SNet.GetMyClientShip.
- Kept by name and marked: Menu.BackToGame (OBJLIB 3), the imports of Menu and string and the $names dict of
  chat.def (OBJLIB 6).
- Errors: the adapters and the core raise the same exceptions with the same texts as before (IndexError of SendID,
  int() of the receiver number, the TypeError of string + and of string.split, "unsliceable object").
- Known differences, only for values Python code can put into the module: a SendID item that is no text, a SendID
  that is no list of four, a text over 127 characters, or a MyShip that is no number, text or None raise a TypeError
  at the entry instead of failing later in the script's way. A line over 511 characters is cut (the network takes
  128).
- Checks: fc-chat (the stored pack baselines: pass; the raw replies identical to the build before), fc-r8t-chat
  and the edge file rd/fc-ol0-chat.txt (55 lines: Car Descriptor, negative ids, int() forms, non-text arguments,
  flags and client ship swapped) identical line by line between the build before and after, the same under ASan
  with 0 reports, sweep 29 of 29, golden 9961, 0 warnings. The online pair test (rd/r8g.py, DeathMatch, a real
  server and client) reaches the native paths the cases record: the chat steps (to all from each side, a private
  line) give the same results and the same chat lines in both game logs before and after. The other differing
  lines of that run are in the join and spectate steps, the timing races round 8g reported.
