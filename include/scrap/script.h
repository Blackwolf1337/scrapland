/*
 * ScrapC's own versions of the scripts of the game packs (issue #2). The game runs its menus, HUD, characters and
 * missions from Python 1.5.2 scripts in the packs; they move to C one by one while the rest keeps running in Python.
 *
 * The loader: when the engine loads a pack module (Pack_PreloadLibrary), the module is made in C from its definition
 * (Script_ModuleMake, src/game/Script/ScriptModule.c) with the C functions ScrapC has for it under their script names.
 * The callbacks named in strings ("PCMenu.ChangeVideoMode") and other modules then reach the C versions. What a port
 * can say as data (the layout of a menu, the action class of a character) is a .def file (Script_RunFile): built into
 * ScrapC, a pack in mods/ can replace it (scrapc/<name>, e.g. scrapc/menus/video.def, scrapc/chars/betty.def).
 */
#ifndef SCRAP_SCRIPT_H
#define SCRAP_SCRIPT_H

#include <stdint.h>

#include "scrap/python.h"
#include "scrap/scriptnative.h"

/* the C functions of one pack module: a function table (stage 7e, scrap/scriptnative.h), or a PyMethodDef table not
 * converted yet (OBJLIB 7e2) */
typedef struct scriptModule_s {
	const char *name;		/* the module name of the script, PCMenu */
	PyMethodDef *methods;		/* put into the module under their names, the table ends with a NULL name */
	const portFn_t *fns;		/* the same as a function table (methods NULL) */
} scriptModule_t;

/* a row of such a table: def of a PyMethodDef table or fn of a function table (the other NULL) */
typedef struct scriptRow_s {
	PyMethodDef *def;
	const portFn_t *fn;
} scriptRow_t;
/* the name of row i of the tables methods or fns (the one that is not NULL), NULL at the end; *row the row */
const char *Script_Row(PyMethodDef *methods, const portFn_t *fns, int i, scriptRow_t *row);

/* the function object of a C function of the script tables (PyCFunction_New(def, self)); with SCRAP_NULL_CHECK set
 * a wrapper that reports a NULL return without an error (module names it in the report) */
PyObject *Script_CheckFunction(PyMethodDef *def, PyObject *self, const char *module);
/* with SCRAP_NULL_CHECK set: reports that a C function or helper fails without a Python error (kind names the
 * place), once per kind, function and call site */
void Script_CheckSilent(const char *kind);

struct nativeVal_s;
/*
 * a pack module made in C instead of running its .pyc (src/game/Script/ScriptModule.c): when the module has a C
 * definition (a .mod file), *module is the new module (in the module store, with its variables, functions and load
 * time calls in the order of the script) and the result 1; 0 when it has none; -1 when a statement failed (error set,
 * *module None)
 */
int Script_ModuleMake(const char *name, const char *file, struct nativeVal_s *module);
/*
 * The modules by name (src/game/Script/ScriptImport.c): the module store (sys.modules) and the import of 1.5.2 on it.
 * Only Pack_PreloadLibrary makes a pack module (Script_ModuleMake); an import never did (there is no .pyc reader), so
 * the order of the first loads is the engine's. A built-in module (sys, math, strop, __builtin__, the engine modules
 * Scrap, SScorer, ...) is made into the store at its first import.
 */
/* the start: the modules Py_Initialize made (__builtin__, sys, __main__, signal) into a new store: 0 when that failed */
byte Script_ImportStart(void);
/* the end: the store emptied and let go */
void Script_ImportEnd(void);
/* the store (a dict object: name -> module), the module __main__ and the builtins dict (the variables of __builtin__) */
const struct nativeVal_s *Script_Modules(void);
const struct nativeVal_s *Script_Main(void);
const struct nativeVal_s *Script_Builtins(void);
/* a variable of sys (PySys_GetObject: the module the start made, also when sys.modules has another): 1 with *out, 0
 * without an error when it has none; Script_SetSysVar sets it (PySys_SetObject; v NULL: deleted when it has one) */
byte Script_SysVar(const char *name, struct nativeVal_s *out);
void Script_SetSysVar(const char *name, const struct nativeVal_s *v);
/* `import name`: the module into *out (1); 0 with the ImportError of the import ("No module named <name>", also for
 * a pack module whose entry is None) */
byte Script_Module(const char *name, struct nativeVal_s *out);
/* `import name` in the module code whose globals are given (IMPORT_NAME: a package's globals look in the package
 * first, which no pack module is) */
byte Script_ModuleIn(const char *name, const struct nativeVal_s *globals, struct nativeVal_s *out);
/* the object stored under name (also None or a non-module a case put there): 1 with a copy in *out, 0 without an
 * error (*out None) when there is none */
byte Script_ModuleLoaded(const char *name, struct nativeVal_s *out);
/* the module name, made empty into the store when there is none or the entry is no module (PyImport_AddModule): 1
 * with *out, 0 with the error */
byte Script_ModuleAdd(const char *name, struct nativeVal_s *out);
/* the lookups by path since the start (reset: back to 0): pack modules found in the store, pack modules whose entry
 * is None, pack modules not loaded, other names, modules made from a .mod file (Script_ModuleMake), PreloadLibrary of
 * a loaded module */
enum { SMC_PACK, SMC_NONE, SMC_ABSENT, SMC_OTHER, SMC_ADDED, SMC_MADE, SMC_LOADED, SMC_COUNT };
void Script_ModuleCounts(unsigned long counts[SMC_COUNT], byte reset);
/* one more lookup of the kind (SMC_) */
void Script_ModuleCount(int kind);
/* text (NAME or NAME.NAME...) as module code loads it (LOAD_NAME in the namespace ns, which is the locals and the
 * globals, then the builtins; LOAD_ATTR): OBJLIB 7e4b: as a Python object (a new reference), NULL with the error set */
PyObject *Script_ModuleName(const struct nativeVal_s *ns, const char *text);
/* the body of a for loop of module code: 1, or 0 with the error set */
typedef byte (*scriptForBody_t)(const struct nativeVal_s *ns, PyObject *item, void *ctx);
/*
 * for <var> in <seq>: <body> in module code, for the C hooks of the definitions (FOR_LOOP, STORE_NAME): item i of seq
 * up to the IndexError, each stored as var in the namespace ns before body runs; 1, or 0 with the error set
 */
byte Script_ModuleFor(const struct nativeVal_s *ns, const char *var, PyObject *seq, scriptForBody_t body, void *ctx);
/* the C hooks of the module definitions ("c <name>" lines, module_hooks in ScriptModule.c): 1, or 0 with the error
 * set; module is the module (its variables are the names of the module code), body the dict object of the class body
 * the line stands in (NULL in the module code) */
int Init_ModLoadChars(const struct nativeVal_s *module, struct nativeVal_s *body);
int Init_ModExtraExecs(const struct nativeVal_s *module, struct nativeVal_s *body);
int NewsPanel_ModSettings(const struct nativeVal_s *module, struct nativeVal_s *body);
int CharsNPC_ModSpawnPosInit(const struct nativeVal_s *module, struct nativeVal_s *body);
int CharsNPC_ModSpawnPosClass(const struct nativeVal_s *module, struct nativeVal_s *body);
int MapTreasure_ModCoins(const struct nativeVal_s *module, struct nativeVal_s *body);
/* at the end of the program, before the variables of the modules go: breaks the cycle of each CharSpawnPos class and
 * the method of its C __init__ in its dict (CharsNPC.c) */
void CharsNPC_Close(void);
struct modVarRef_s;
struct nativeVal_s;
/* SCRAP_NULL_CHECK is set (the functions of the tables are the wrappers of Script_CheckFunction) */
byte Script_Checking(void);
/* the function object of a C function of the script tables into *out: the native function of its port ref
 * (NObj_NewPortFunc; Port_OfFn, Port_OfDef), else (a PyMethodDef row without a ref, SCRAP_NULL_CHECK) the view of
 * Script_CheckFunction's object; 1, or 0 with the error */
byte Script_FunctionOf(const scriptRow_t *row, const char *module, struct nativeVal_s *out);
/* the function name of module from the C tables (script_modules, the last table that has it) into *out: 1, 0 with
 * *out None (an error set when making it failed, none when no table has it) */
byte Script_TableFunction(const char *module, const char *name, struct nativeVal_s *out);
/* whether a table of script_modules has the function name of module */
byte Script_TableHas(const char *module, const char *name);
/* the dict writes the loader made after the .pyc for a table, on a module made in C with the values in place (the
 * functions the script lacks are added) */
void Script_TableReplay(const struct nativeVal_s *module, PyMethodDef *methods, const portFn_t *fns);
/* the dict writes the loader made after the .pyc for the function cur under name, on a module made in C with the
 * values in place: the other names, tuples and dict values that hold cur and name itself (they decide the key order of
 * the module's variables, scrap/modvars.h) */
void Script_ReplaySet(const struct modVarRef_s *ref, const char *name, const struct nativeVal_s *cur);
/*
 * after a module made in C: the dict writes the loader made after the .pyc, with the values in place, for
 * every table of script_modules, the level data and the level tables (the functions the script lacks, and the sets
 * that resize a 1.5.2 dict, which decide its key order); defs holds the functions the module code defined, by name
 */
void Script_ModuleExtras(const char *name, const struct nativeVal_s *module, const struct nativeVal_s *defs);
/* the level that loads, lower case ("police"); 0 when Init.Path is not set yet */
byte Script_LevelName(char *out, int size);
/* for a module of the level made in C (src/game/Script/Levels.c): its data functions start over in their pool */
void Script_LevelModuleStart(const char *module);
/* the C function name of the level table of module in this level into *out (Script_FunctionOf): 1, 0 with *out None
 * (an error set when making it failed) */
byte Script_LevelTableFunction(const char *module, const char *name, struct nativeVal_s *out);
/* the data function name of the level file of module in this level into *out: a native function (a callback whose
 * __self__ is the tuple (file index, section, parameter names, defaults); under SCRAP_NULL_CHECK the view of the
 * checked C function object); 1, 0 with *out None (an error set when making it failed) */
byte Script_LevelDataFunction(const char *module, const char *name, struct nativeVal_s *out);
/* the part of Script_ModuleExtras for the level data and the level tables of the module (defs as there) */
void Script_LevelModuleExtras(const char *name, const struct nativeVal_s *module, const struct nativeVal_s *defs);

/* the core of CreateFixedInit(e) of the level scripts (MapDecont.CreateBishopInit, GamblinDen's Map.CreateFixedInit):
 * the character e stands where it is (src/game/Script/levels/Orbit.c) */
struct nativeVal_s;
byte Level_FixedInitN(const struct nativeVal_s *e);
/* a step of a .def file that C does ("run <name> [arg ...]"): args the n positional values of the line as the runner
 * has them (borrowed; a list or dict of a $name is the object itself). A step that fails leaves its error set */
typedef struct scriptStep_s {
	const char *name;
	void (*fn)(int id, const struct nativeVal_s *args, int n);
} scriptStep_t;

/* the n values of a step as one list or tuple (type NV_LIST or NV_TUPLE) of copies in *out (Native_Free it); an
 * NV_OBJ stays the object it is. pass: as the value a call passes on instead (Native_Pass: what an adapter makes of
 * the tuple of the values): 1, or 0 with the error and *out None */
byte Script_StepValues(const struct nativeVal_s *args, int n, int type, byte pass, struct nativeVal_s *out);

/*
 * runs the .def file name (scrapc/<name> of the packs, else builtin) for the scorer of player id: the lines before
 * the first section and those of section (NULL for a file without sections). steps are the C steps it may run (NULL
 * ends the table), vars the dict object of the $names it may use (Script_DefVar; NULL or None: none; the lets of the
 * file go into it). 0 when it could not be read.
 */
byte Script_RunFile(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	const struct nativeVal_s *vars);
/* the $name name = v in the dict object *vars of a .def file (*vars None: a new one; v copied as a call through Python
 * hands it on, NObj_CallArg) */
void Script_DefVar(struct nativeVal_s *vars, const char *name, const struct nativeVal_s *v);

/*
 * Script_RunFile for a port that must stop where the script stops: a call line whose function raises, an entity line
 * the entity refuses (SystemError, as the attribute set of the handle) and a step that leaves an error set end the
 * file; 0 with the error left set for the caller (not printed)
 */
byte Script_RunFileTry(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	const struct nativeVal_s *vars);

/* OBJLIB 7: the calls of the call lines of .def files since the start (or the last reset) by path, for the test
 * channel's "native ports": through the registry (the port's or the engine's entry while the module attribute is its
 * own function), by name to a function of the registry that something else replaced, by name to any other function,
 * and the ones that failed; reset: back to 0 (the registry count is the site's, reset with the sites) */
void Script_DefCounts(unsigned long *registry, unsigned long *byName, unsigned long *other, unsigned long *errors,
	byte reset);

/* the step "print <text>" of .def files: a line on the console, as a script's print */
void Script_StepPrint(int id, const struct nativeVal_s *args, int n);

/* the last item of the .def file that runs ("" when none runs), for its steps */
const char *Script_CurrentItem(void);
/* a $name of the .def file that runs, for its steps (copied as Script_DefVar copies it) */
void Script_SetValue(const char *name, const struct nativeVal_s *value);

/* the scorer items of player id */
void Script_Add(int id, const char *item, const char *type);
void Script_SetStr(int id, const char *item, const char *var, const char *s);
void Script_SetNum(int id, const char *item, const char *var, double d);
/* "" when the item or the variable is not there; the string of the item, valid until it changes */
const char *Script_GetStr(int id, const char *item, const char *var);
double Script_GetNum(int id, const char *item, const char *var);
void Script_SetDefault(int id, const char *item);
/* the item is in the scorer of player id */
byte Script_HasItem(int id, const char *item);
/* SScorer.GetDefault: the item that gets the cursor ("" when none) */
const char *Script_GetDefault(int id);
/* SScorer.SetCursor: the item drawn as the cursor ("" for none) */
void Script_SetCursor(int id, const char *item);

/* a text of the language ("" when it is not there) */
const char *Script_Lang(const char *key);

/* module.func(args) as a line of a script: 0 with the error left set for the caller (not printed), which then
 * stops and returns NULL as the script would */
byte Script_Try(const char *module, const char *func, const char *fmt, ...);
/* module.func(args) with the result: a new reference, NULL with the error set (not printed) */
/* obj.name(args) as a script calls it (the error of a missing attribute reads the same): the result, a new reference,
 * NULL with the error set */
PyObject *Script_Method(PyObject *obj, const char *name, const char *fmt, ...);
PyObject *Script_Ask(const char *module, const char *func, const char *fmt, ...);
/* callable(args), args given away: the result (a new reference, NULL with the error set) */
PyObject *Script_Invoke(PyObject *callable, PyObject *args);
/* an integer variable of a script module (def when it is not there) */
int Script_GetInt(const char *module, const char *var, int def);
/* a string variable of a script module into out ("" when it is not there or not a string) */
const char *Script_GetString(const char *module, const char *var, char *out, int size);
/* sets a variable of a script module (value given away) and an integer one */
void Script_SetVar(const char *module, const char *var, PyObject *value);
void Script_SetInt(const char *module, const char *var, int value);
/* an attribute of a script module (a new reference, NULL with the error set); a pack module through the variable
 * registry (scrap/modvars.h), as Script_Global, Script_SetGlobal and Script_SetVar */
PyObject *Script_Attr(const char *module, const char *name);
/* a number variable of a script module (def when it is not there or no number) */
double Script_GetFloat(const char *module, const char *var, double def);

/* entity handles and values as a script uses them (0 or -1 with the error set): handle.var = v (v given away),
 * handle.var == text, handle.var[0:len(prefix)] == prefix, cmp(value, n) (-2 on an error) */
byte Script_HSet(PyObject *e, const char *var, PyObject *v);
int Script_HIs(PyObject *e, const char *var, const char *text);
int Script_HPrefix(PyObject *e, const char *var, const char *prefix);
int Script_NumCmp(PyObject *a, long n);

/* script variables of entities by name, as the entity handle reads them: the number of a variable (int, else float;
 * def when the entity or the variable is not there) and its text (def likewise) */
double Script_EntNum(const char *ent, const char *var, double def);
const char *Script_EntStr(const char *ent, const char *var, const char *def);
/* `if ent.var:` of a script (0 when the entity or the variable is not there) */
byte Script_EntTrue(const char *ent, const char *var);

/* the arguments of a menu callback: the player and the item (empty when the callback got none) */
byte Script_Args(PyObject *args, int *id, const char **control);

/* ---- Python values as the scripts use them (src/game/Script/ScriptValue.c) ---- */

/* the temporary references of a C function, released together by Script_Done */
#define SCRIPT_REFS 48
typedef struct scriptRefs_s {
	PyObject *o[SCRIPT_REFS];
	int n;
} scriptRefs_t;
/* o into the references of r (NULL passes through) */
PyObject *Script_Keep(scriptRefs_t *r, PyObject *o);
/* releases the references of r, then result */
PyObject *Script_Done(scriptRefs_t *r, PyObject *result);
/* a new reference of o (NULL stays NULL) */
PyObject *Script_Ref(PyObject *o);
/* a global of a module as a function of its script reads it: a new reference, a NameError with the name when it is
 * missing (a deleted variable) */
PyObject *Script_Global(const char *module, const char *name);
/* `global name; name = v` (v given away): 0 with the error set */
byte Script_SetGlobal(const char *module, const char *name, PyObject *v);
/* the import line of a function (the names end with NULL): 0 with the error set */
byte Script_Import(const char *const *names);
/* o[i] as a script subscripts (a new reference) */
PyObject *Script_Index(PyObject *o, int i);
/* o.name as a script reads it (a new reference, NULL with the error set; o NULL stays NULL): an attribute the engine
 * fails to give without an exception (the handle of a deleted entity) raises ceval's "SystemError: error return
 * without exception set", as the line of the script does */
PyObject *Script_GetAttr(PyObject *o, const char *name);
/* NULL with ceval's SystemError set unless an error is set already: the return of a C function where an engine call
 * failed without an exception */
PyObject *Script_Fail(void);
/* callable(*args, **kw) from C (kw may be NULL; nothing given away): the C function making the call counts toward
 * the recursion depth of the interpreter for the time of the call, as the frame of the Python function it replaces
 * does in eval_code2, so a cycle through C functions raises "RuntimeError: Maximum recursion depth exceeded" like
 * the scripts (also when the C stack runs short first) */
PyObject *Script_CallObject(PyObject *callable, PyObject *args, PyObject *kw);
/* the depth check of Script_CallObject for a call from C that does not go through the interpreter (the direct calls
 * between ports, OBJLIB 7): frame is the caller's __builtin_frame_address(0). 1 with the call counted (Script_Leave
 * after it), else 0 with the RuntimeError */
byte Script_Enter(uintptr_t frame);
void Script_Leave(void);
/* a + b (a given away), a + "text" (a given away), "text" + b: new references, NULL stays NULL */
PyObject *Script_Plus(PyObject *a, PyObject *b);
PyObject *Script_PlusS(PyObject *a, const char *text);
PyObject *Script_SPlus(const char *text, PyObject *b);
/* `if o:`, a == b: 1, 0, or -1 with the error set (also for NULL) */
int Script_True(PyObject *o);
int Script_Eq(PyObject *a, PyObject *b);
/* `o` (a new reference, NULL stays NULL) */
PyObject *Script_Repr(PyObject *o);
/* item i of a for loop over seq: 1 with a new reference in *item, 0 at the end, -1 with the error set */
int Script_Next(PyObject *seq, int i, PyObject **item);
/* the print statement with its items, as ceval prints them to sys.stdout (PRINT_ITEM, PRINT_NEWLINE): 0 with the
 * error set */
byte Script_Print(const struct nativeVal_s *const *items, int n);
/* the parts of the print on the file objects of the scripts (an instance with write and softspace, sys.stdout;
 * fileobject.c): PyFile_SoftSpace (the old flag), PyFile_WriteObject (str of v when raw, else its repr, to f.write),
 * PyFile_WriteString (nothing while an error is set), Py_FlushLine (the newline sys.stdout's soft space owes);
 * Script_WriteOut v to sys.stdout. Each 1, or 0 with the error */
int Script_SoftSpace(const struct nativeVal_s *f, int newflag);
byte Script_FileWrite(const struct nativeVal_s *f, const struct nativeVal_s *v, byte raw);
byte Script_FileWriteText(const struct nativeVal_s *f, const char *s);
byte Script_FlushLine(void);
byte Script_WriteOut(const struct nativeVal_s *v, byte raw);
/* Scrap.GetEntity(name), Scrap.UsrEntity(0): new references, NULL with the error set */
PyObject *Script_Entity(PyObject *name);
PyObject *Script_Usr(void);
/* `global last; last = target.name; target.name = name` in a function of the script module (name its global, the
 * chain of callbacks the net weapons and Vehicles put in front of Net's): 0 with the error set */
byte Script_Hook(const char *module, const char *target, const char *name, const char *last);
/* eval(text) of a value the game wrote (save and engine variables, network messages, event names, data lines) in the
 * namespace globals (a module or a dict object): the checks of builtin eval (TypeError, ValueError), then the literal
 * of the text after its leading blanks (Native_LiteralNone, None as LOAD_NAME finds it, when the globals give valid
 * __builtins__); any other text raises Script_NotRead's SyntaxError, no Python runs. Script_EvalIn the same in the
 * globals of a script module. 1 with *out, 0 with the error */
byte Script_EvalText(const struct nativeVal_s *globals, const struct nativeVal_s *text, struct nativeVal_s *out);
byte Script_EvalIn(const char *module, const struct nativeVal_s *text, struct nativeVal_s *out);
/* the rest of Script_EvalText for a text s (leading blanks skipped) that the literal reader did not take: a name chain
 * as eval runs it (its value or NameError), else Script_NotRead's SyntaxError, for two names or numbers with blanks
 * between them with the 1.5.2 parser's message and position. Also used by Native_EvalText. 1 with *out, 0 with the
 * error */
byte Script_EvalOther(const struct nativeVal_s *globals, const char *s, struct nativeVal_s *out);
/* a line in the log of the gate of the pack modules (SCRAP_PY_GATE, PythonInit.c): the level, what, the module name, a
 * detail; nothing when it is off */
void Script_GateNote(const char *what, const char *name, const char *detail);
/* the error for a text of Python that no C reader takes and nothing runs: SyntaxError "invalid syntax" (the last
 * line of the traceback eval or exec print for it), noted in the gate log (SCRAP_PY_GATE, event "notread", where
 * names the reader); NULL */
PyObject *Script_NotRead(const char *where, const char *text);
/* the globals give a frame valid builtins (a dict or a module in __builtins__), as exec and eval need */
byte Script_HasBuiltins(const struct nativeVal_s *globals);
/* a string constant of len chars as the compiler makes it (interned when it has only name characters) */
PyObject *Script_StrConst(const char *s, int len);
/* s[0:len] is a Python 1.5.2 name (ASCII letters, digits and _, not a keyword) */
byte Script_IsName(const char *s, int len);
/* o is a string that between single quotes in a line of Python is itself again; the same for the n bytes of s */
byte Script_IsQuotable(PyObject *o);
byte Script_IsQuotableText(const char *s, int n);
/* a NAME(.NAME)* text evaluated with the locals, the globals and their builtins (namespaces: a module or a dict
 * object; no blanks; each name may be followed by subscripts [n] of a decimal int as repr writes it,
 * "ShipEdit.editedShips[3].Model"): 1 with *out, -1 with the error of the lookup set (NameError, AttributeError,
 * IndexError), 0 when text is not such a chain or the globals have no valid builtins */
int Script_NameChain(const struct nativeVal_s *locals, const struct nativeVal_s *globals, const char *text,
	struct nativeVal_s *out);

/* the C video options of PCMenu (src/game/Script/MenuVideo.c) */
extern const portFn_t g_MenuVideoMethods[];
/* the C options and controls menus of PCMenu (src/game/Script/MenuControls.c) */
extern const portFn_t g_MenuControlsMethods[];
/* the C multiplayer menus of PCMenu (src/game/Script/MenuNet.c) */
extern const portFn_t g_MenuNetMethods[];
/* the helpers of pcmenu.py on the variables of its video menu (src/game/Script/MenuVideoVars.c) */
extern const portFn_t g_MenuVideoVarsMethods[];
/* the C building blocks of the Menu module (src/game/Script/MenuBase.c) */
extern const portFn_t g_MenuBaseMethods[];
/* the C main menus of the Menu module (src/game/Script/MenuMain.c) */
extern const portFn_t g_MenuMainMethods[];
/* the C mission info pages of the Menu module (src/game/Script/MenuMission.c) */
extern const portFn_t g_MenuMissionMethods[];
/* the C HUD: the Scorer, CharScorer and RacerScorer modules (src/game/Script/MenuHud.c) */
extern const portFn_t g_ScorerMethods[];
extern const portFn_t g_CharScorerMethods[];
extern const portFn_t g_RacerScorerMethods[];
/* the C Chars module (src/game/Script/Chars.c) */
extern const portFn_t g_CharsMethods[];
/* the C MakeChar module (src/game/Script/MakeChar.c) */
extern const portFn_t g_MakeCharMethods[];
/* the C CharAct module (src/game/Script/CharAct.c) */
extern const portFn_t g_CharActMethods[];
/* the C Speech module (src/game/Script/Speech.c) */
extern const portFn_t g_SpeechMethods[];
/* the C SaveGame module (src/game/Script/SaveGame.c) */
extern const portFn_t g_SaveGameMethods[];
/* the C Fx module (src/game/Script/Fx.c, data/fx/fx.def) */
extern const portFn_t g_FxMethods[];
/* the C Items module (src/game/Script/Items.c) */
extern const portFn_t g_ItemsMethods[];
extern const portFn_t g_NetMethods[];
/* the online game modes (src/game/Script/NetModes.c) */
extern const portFn_t g_NDeathMatchMethods[];
extern const portFn_t g_NDeathMatchBotsMethods[];
extern const portFn_t g_NTeamDMMethods[];
extern const portFn_t g_NTeamFlagMethods[];
extern const portFn_t g_NFlagHuntMethods[];
extern const portFn_t g_NPostmanMethods[];
extern const portFn_t g_NTeamPostMethods[];
extern const portFn_t g_WeaponsMethods[];
extern const portFn_t g_VulcanMethods[];
extern const portFn_t g_DevastatorMethods[];
extern const portFn_t g_TeslaMethods[];
extern const portFn_t g_ATPCMethods[];
extern const portFn_t g_LaserMethods[];
extern const portFn_t g_SwarmMethods[];
extern const portFn_t g_InfernoMethods[];
extern const portFn_t g_CloudMethods[];
extern const portFn_t g_EMIMethods[];
extern const portFn_t g_SonicMethods[];
extern const portFn_t g_SputnikInterfaceMethods[];
/* the ship editor (src/game/Script/ShipEdit.c, HangarTab.c, data/menus/shipedit.def) */
extern const portFn_t g_ShipEditMethods[];
extern const portFn_t g_HangarTabMethods[];
extern const portFn_t g_VehiclesMethods[];
extern const portFn_t g_VehiclesNPCMethods[];
extern const portFn_t g_SplitMethods[];
extern const portFn_t g_SDeathMatchMethods[];
extern const portFn_t g_SFlagHuntMethods[];
extern const portFn_t g_SPostmanMethods[];
extern const portFn_t g_SRaceMethods[];
/* the functions of the Init module (src/game/Script/InitLevel.c) */
extern const portFn_t g_InitMethods[];
/* the functions string.py keeps in Python (src/game/Script/String.c) */
extern const portFn_t g_StringMethods[];
/* the sound scripts of the characters (src/game/Script/Sounds.c, data/sound/) */
extern const portFn_t g_BankDirectorSoundMethods[];
extern const portFn_t g_BankMasterSoundMethods[];
extern const portFn_t g_BertoSoundMethods[];
extern const portFn_t g_BettySoundMethods[];
extern const portFn_t g_BishopSoundMethods[];
extern const portFn_t g_BossSoundMethods[];
extern const portFn_t g_CrazyGamblerSoundMethods[];
extern const portFn_t g_DesktopSoundMethods[];
extern const portFn_t g_DtritusSoundMethods[];
extern const portFn_t g_FunctionarySoundMethods[];
extern const portFn_t g_GateKeeperSoundMethods[];
extern const portFn_t g_GearSoundMethods[];
extern const portFn_t g_HumanSoundMethods[];
extern const portFn_t g_HumphreySoundMethods[];
extern const portFn_t g_MaintenanceSoundMethods[];
extern const portFn_t g_MayorSoundMethods[];
extern const portFn_t g_MercenarySoundMethods[];
extern const portFn_t g_MessengerSoundMethods[];
extern const portFn_t g_NurseSoundMethods[];
extern const portFn_t g_PoliceBossSoundMethods[];
extern const portFn_t g_PoliceSoundMethods[];
extern const portFn_t g_RustySoundMethods[];
extern const portFn_t g_SebastianSoundMethods[];
extern const portFn_t g_SentinelSoundMethods[];
extern const portFn_t g_SputnikJunkyardSoundMethods[];
extern const portFn_t g_SputnikSoundMethods[];
extern const portFn_t g_TestCSoundMethods[];
/* the C CharsNPC module (src/game/Script/CharsNPC.c) */
extern const portFn_t g_CharsNPCMethods[];
/* the C parts of the character scripts, their Init runs data/chars/<name>.def (src/game/Script/Characters.c) */
extern const portFn_t g_BettyMethods[];
extern const portFn_t g_BankDirectorMethods[];
extern const portFn_t g_BossMethods[];
extern const portFn_t g_CrazyGamblerMethods[];
extern const portFn_t g_HumphreyMethods[];
extern const portFn_t g_MaintenanceMethods[];
extern const portFn_t g_MayorMethods[];
extern const portFn_t g_RustyMethods[];
extern const portFn_t g_SebastianMethods[];
extern const portFn_t g_SentinelMethods[];
extern const portFn_t g_SputnikJunkyardMethods[];
extern const portFn_t g_BishopMethods[];
extern const portFn_t g_FunctionaryMethods[];
extern const portFn_t g_GearMethods[];
extern const portFn_t g_BankMasterMethods[];
extern const portFn_t g_BertoMethods[];
extern const portFn_t g_NurseMethods[];
extern const portFn_t g_DesktopMethods[];
extern const portFn_t g_PoliceMethods[];
extern const portFn_t g_GateKeeperMethods[];
extern const portFn_t g_DtritusMethods[];
extern const portFn_t g_SputnikMethods[];
extern const portFn_t g_PoliceBossMethods[];
extern const portFn_t g_MessengerMethods[];
extern const portFn_t g_HumanMethods[];
extern const portFn_t g_MercenaryAMethods[];
extern const portFn_t g_MercenaryBMethods[];
extern const portFn_t g_MercenaryCMethods[];
extern const portFn_t g_MercenaryLMethods[];

/* the start of an indoor map (src/game/Script/InMap.c) and the desks of the functionaries
 * (src/game/Script/Bureaucracy.c) */
extern const portFn_t g_InMapMethods[];
extern const portFn_t g_BureaucracyMethods[];
/* the chat of a network game (src/game/Script/Chat.c) and the credits text (src/game/Script/CreditsText.c) */
extern const portFn_t g_ChatMethods[];
extern const portFn_t g_CreditsTextMethods[];
/* the base of the online game modes (src/game/Script/Ffa.c) */
extern const portFn_t g_FfaMethods[];

/* the mission scripts: the traffic and the police (src/game/Script/Traffic.c), the phone cabs and Dtritus' desktop
 * (src/game/Script/PhoneCab.c), the metro (src/game/Script/Metro.c) and the challenges (src/game/Script/Challenge.c) */
extern const portFn_t g_TrafficMethods[];
extern const portFn_t g_OutPoliceMethods[];
extern const portFn_t g_InPoliceMethods[];
extern const portFn_t g_PhoneCabMethods[];
extern const portFn_t g_DTritusDesktopMethods[];
extern const portFn_t g_MetroMethods[];
extern const portFn_t g_ChallengeMethods[];
/* the options of a challenge against foe of the kind type ("Race", "Combat") on the entity ch, with the rewards or
 * the lives they cost (Challenge.SetChallengeOptions, PhoneCab.SetRevengeOptions): 0 with the error set */
struct nativeVal_s;
byte Challenge_SetOptions(const struct nativeVal_s *ch, const struct nativeVal_s *type, const struct nativeVal_s *foe);
/* the missions (issue #2 step 8a): the alarm and the news panels (src/game/Script/Alarm.c), the doors, elevators and
 * teleporters (src/game/Script/Doors.c), the items and the crazy wind (src/game/Script/MisItems.c), the mission flow
 * (src/game/Script/MissionsFuncs.c) */
extern const portFn_t g_AlarmMethods[];
extern const portFn_t g_NewsPanelMethods[];
extern const portFn_t g_DoorsMethods[];
extern const portFn_t g_ElevatorMethods[];
extern const portFn_t g_TeleportMethods[];
extern const portFn_t g_MisItemsMethods[];
extern const portFn_t g_CrazyWingMethods[];
extern const portFn_t g_MissionsFuncsMethods[];
/* the cut scenes (src/game/Script/Scene.c), the character conversors and the possession
 * (src/game/Script/CharConversor.c), the races and combats of the challenges (src/game/Script/RaceMaker.c) */
extern const portFn_t g_SceneMethods[];
extern const portFn_t g_CharConversorMethods[];
extern const portFn_t g_RaceMakerMethods[];

/* the general sounds: the sound files of a level, the ship and damage sounds, the point sounds of the maps and the door
 * sounds (src/game/Script/GameSounds.c, data/sound/sound.def, outsound.def, doorsound.def) */
extern const portFn_t g_SoundMethods[];
extern const portFn_t g_OutSoundMethods[];
extern const portFn_t g_DoorSoundMethods[];
/* the start of an outdoor level and its spawn points, the fake traffic indoors (src/game/Script/OutMap.c) */
extern const portFn_t g_OutMapMethods[];
extern const portFn_t g_InTrafficMethods[];
/* the ship tables and SetShipParams (src/game/Script/ShipMaker.c, data/vehicles/shipmaker.def), the player's ship,
 * boarding, parking and the parked ships, the speed rings (src/game/Script/Parking.c) */
extern const portFn_t g_ShipMakerMethods[];
extern const portFn_t g_ParkingMethods[];
extern const portFn_t g_AcceleratorMethods[];
/* the game modes of the single player super deals (src/game/Script/SuperDeals.c) */
extern const portFn_t g_DeathMatchMethods[];
extern const portFn_t g_TeamDeathMatchMethods[];
extern const portFn_t g_VersusMethods[];
extern const portFn_t g_CTFMethods[];
extern const portFn_t g_DominationMethods[];
extern const portFn_t g_MortalRaceMethods[];
/* the tabs of the ship editor: engines, weapons and hull (src/game/Script/EnginesTab.c, WeaponsTab.c, HullTab.c,
 * data/menus/enginestab.def, weaponstab.def, hulltab.def) */
extern const portFn_t g_EnginesTabMethods[];
extern const portFn_t g_WeaponsTabMethods[];
extern const portFn_t g_HullTabMethods[];

/* the gameplay functions of the character modules (src/game/Script/CharFuncs.c), next to their Init tables */
extern const portFn_t g_FunctionaryFuncs[];
extern const portFn_t g_RustyFuncs[];
extern const portFn_t g_BossFuncs[];
extern const portFn_t g_MayorFuncs[];
extern const portFn_t g_MaintenanceFuncs[];
extern const portFn_t g_SputnikJunkyardFuncs[];
extern const portFn_t g_CrazyGamblerFuncs[];
extern const portFn_t g_GearFuncs[];
extern const portFn_t g_PoliceFuncs[];
extern const portFn_t g_BishopFuncs[];
extern const portFn_t g_MessengerFuncs[];
/* the siren actions and missile events of Gear and Police from their files (src/game/Script/Characters.c) */
extern const portFn_t g_GearDefFuncs[];
extern const portFn_t g_PoliceDefFuncs[];
/* Sputnik's ship functions (src/game/Script/Sputnik.c) and the gatekeepers (src/game/Script/GateKeeper.c) */
extern const portFn_t g_SputnikFuncs[];
extern const portFn_t g_GateKeeperFuncs[];

#endif
