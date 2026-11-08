/*
 * ScrapC's own versions of the scripts of the game packs (issue #2). The game runs its menus, HUD, characters and
 * missions from Python 1.5.2 scripts in the packs; they move to C one by one while the rest keeps running in Python.
 *
 * The loader: when the engine loads a pack module (Pack_PreloadLibrary), Script_ModuleLoaded puts the C functions
 * ScrapC has for that module into it under their script names. The pack's own code, the callbacks named in strings
 * ("PCMenu.ChangeVideoMode") and other modules then reach the C versions. What a port can say as data (the layout
 * of a menu, the action class of a character) is a .def file (Script_RunFile): built into ScrapC, a pack in mods/
 * can replace it (scrapc/<name>, e.g. scrapc/menus/video.def, scrapc/chars/betty.def).
 */
#ifndef SCRAP_SCRIPT_H
#define SCRAP_SCRIPT_H

#include <stdint.h>

#include "scrap/python.h"

/* the C functions of one pack module */
typedef struct scriptModule_s {
	const char *name;		/* the module name of the script, PCMenu */
	PyMethodDef *methods;		/* put into the module under their names, the table ends with a NULL name */
} scriptModule_t;

/* a module of the packs is loaded: its C functions go into it */
void Script_ModuleLoaded(const char *name, PyObject *module);
/* the other names, list and tuple items and dict values of a module dict that hold old get f (old is the function
 * under key; a tuple is replaced by a new one), and so do the engine callbacks bound to old (at the end of
 * Script_ModuleLoaded) */
void Script_ReplaceRefs(PyObject *dict, PyObject *key, PyObject *old, PyObject *f);
/* the C functions of a table into the module under their names (and the other names the module bound the Python
 * function to); kept (a list or NULL) keeps the replaced Python functions */
void Script_Install(PyObject *module, PyMethodDef *methods, PyObject *kept);
/* the function object of a C function of the script tables (PyCFunction_New(def, self)); with SCRAP_NULL_CHECK set
 * a wrapper that reports a NULL return without an error (module names it in the report) */
PyObject *Script_CheckFunction(PyMethodDef *def, PyObject *self, const char *module);
/* with SCRAP_NULL_CHECK set: reports that a C function or helper fails without a Python error (kind names the
 * place), once per kind, function and call site */
void Script_CheckSilent(const char *kind);
/* with SCRAP_NULL_CHECK and SCRAP_NULL_CHECK_ENGINE=1: the functions of the engine modules into the same wrapper */
void Script_CheckEngine(void);
/* a module of the level that loads: the functions its data file has go into it (src/game/Script/Levels.c); kept
 * holds the replaced functions until the module loads again */
void Script_LevelModuleLoaded(const char *name, PyObject *module, PyObject *kept);

/*
 * a pack module made in C instead of running its .pyc (src/game/Script/ScriptModule.c): when the module has a C
 * definition (a .mod file), *module is the new module (in sys.modules, with its variables, functions and load time
 * calls in the order of the script) and the result 1; 0 when it has none; -1 when a statement failed (error set,
 * *module NULL)
 */
int Script_ModuleMake(const char *name, const char *file, PyObject **module);
/* text (NAME or NAME.NAME...) as module code loads it (LOAD_NAME in locals, which are the globals too, then the
 * builtins; LOAD_ATTR): a new reference, NULL with the error set */
PyObject *Script_ModuleName(PyObject *locals, const char *text);
/* the body of a for loop of module code: 1, or 0 with the error set */
typedef byte (*scriptForBody_t)(PyObject *locals, PyObject *item, void *ctx);
/*
 * for <var> in <seq>: <body> in module code, for the C hooks of the definitions (FOR_LOOP, STORE_NAME): item i of seq
 * up to the IndexError, each stored as var in locals before body runs; 1, or 0 with the error set
 */
byte Script_ModuleFor(PyObject *locals, const char *var, PyObject *seq, scriptForBody_t body, void *ctx);
/* the C hooks of the module definitions ("c <name>" lines, module_hooks in ScriptModule.c): 1, or 0 with the error
 * set; locals is the dict the names of the module code go to */
int Init_ModLoadChars(PyObject *module, PyObject *locals);
int Init_ModExtraExecs(PyObject *module, PyObject *locals);
int NewsPanel_ModSettings(PyObject *module, PyObject *locals);
int CharsNPC_ModSpawnPosInit(PyObject *module, PyObject *locals);
int CharsNPC_ModSpawnPosClass(PyObject *module, PyObject *locals);
int MapTreasure_ModCoins(PyObject *module, PyObject *locals);
/* before Py_Finalize: breaks the cycle of the CharSpawnPos class and its C __init__ (CharsNPC.c) */
void CharsNPC_Close(void);
/* the function name of module from the C tables (script_modules, the last table that has it), a new reference; NULL
 * without an error when no table has it */
PyObject *Script_TableFunction(const char *module, const char *name);
/* whether a table of script_modules has the function name of module */
byte Script_TableHas(const char *module, const char *name);
/* the dict writes of Script_Install of a table on a module made in C, with the values in place (the functions the
 * script lacks are added) */
void Script_TableReplay(PyObject *module, PyMethodDef *methods);
/* the dict writes of a replaced function on a module made in C: the other names, tuples and dict values that hold
 * cur (Script_ReplaceRefs) and name itself, with the values in place */
void Script_ReplaySet(PyObject *dict, const char *name, PyObject *cur);
/*
 * after a module made in C: the dict writes Script_ModuleLoaded makes after the .pyc, with the values in place, for
 * every table of script_modules, the level data and the level tables (the functions the script lacks, and the sets
 * that resize a 1.5.2 dict, which decide its key order); defs holds the functions the module code defined, by name
 */
void Script_ModuleExtras(const char *name, PyObject *module, PyObject *defs);
/* the level that loads, lower case ("police"); 0 when Init.Path is not set yet */
byte Script_LevelName(char *out, int size);
/* for a module of the level made in C (src/game/Script/Levels.c): its data functions start over in their pool */
void Script_LevelModuleStart(const char *module);
/* the C function name of the level table of module in this level, NULL without an error if none */
PyObject *Script_LevelTableFunction(const char *module, const char *name);
/* the data function name of the level file of module in this level, NULL without an error if none */
PyObject *Script_LevelDataFunction(const char *module, const char *name);
/* the part of Script_ModuleExtras for the level data and the level tables of the module (defs as there) */
void Script_LevelModuleExtras(const char *name, PyObject *module, PyObject *defs);

/* CreateFixedInit(e) of the level scripts: the character e stands where it is (src/game/Script/levels/Orbit.c) */
PyObject *Level_FixedInit(PyObject *self, PyObject *args);
/* a step of a .def file that C does ("run <name> [arg ...]"), args the tuple of the arguments */
typedef struct scriptStep_s {
	const char *name;
	void (*fn)(int id, PyObject *args);
} scriptStep_t;

/*
 * runs the .def file name (scrapc/<name> of the packs, else builtin) for the scorer of player id: the lines before
 * the first section and those of section (NULL for a file without sections). steps are the C steps it may run (NULL
 * ends the table), vars a dict of the $names it may use (or NULL). 0 when it could not be read.
 */
byte Script_RunFile(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	PyObject *vars);

/*
 * Script_RunFile for a port that must stop where the script stops: a call line whose function raises, an entity line
 * the entity refuses (SystemError, as the attribute set of the handle) and a step that leaves an error set end the
 * file; 0 with the error left set for the caller (not printed)
 */
byte Script_RunFileTry(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	PyObject *vars);

/* the step "print <text>" of .def files: a line on the console, as a script's print */
void Script_StepPrint(int id, PyObject *args);

/* the last item of the .def file that runs ("" when none runs), for its steps */
const char *Script_CurrentItem(void);
/* a $name of the .def file that runs, for its steps (value given away) */
void Script_SetValue(const char *name, PyObject *value);

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

/* module.func(args) of a script, args as Py_BuildValue (a tuple, "(is)"); 0 when it failed (the error printed) */
byte Script_Call(const char *module, const char *func, const char *fmt, ...);
/* module.func(args) as a line of a script: 0 with the error left set for the caller (not printed), which then
 * stops and returns NULL as the script would */
byte Script_Try(const char *module, const char *func, const char *fmt, ...);
/* module.func(args) with the result: a new reference, NULL with the error set (not printed) */
/* obj.name(args) as a script calls it (the error of a missing attribute reads the same): the result, a new reference,
 * NULL with the error set */
PyObject *Script_Method(PyObject *obj, const char *name, const char *fmt, ...);
PyObject *Script_Ask(const char *module, const char *func, const char *fmt, ...);
/* the same with a tuple of arguments and a dict of keywords (NULL for none), both given away */
byte Script_CallKw(const char *module, const char *func, PyObject *args, PyObject *kw);
/* callable(args), args given away: the result (a new reference, NULL with the error set) */
PyObject *Script_Invoke(PyObject *callable, PyObject *args);
/* an integer variable of a script module (def when it is not there) */
int Script_GetInt(const char *module, const char *var, int def);
/* a string variable of a script module into out ("" when it is not there or not a string) */
const char *Script_GetString(const char *module, const char *var, char *out, int size);
/* sets a variable of a script module (value given away) and an integer one */
void Script_SetVar(const char *module, const char *var, PyObject *value);
void Script_SetInt(const char *module, const char *var, int value);
/* an attribute of a script module (a new reference, NULL with the error set) */
PyObject *Script_Attr(const char *module, const char *name);
/* a number variable of a script module (def when it is not there or no number) */
double Script_GetFloat(const char *module, const char *var, double def);

/* entity handles and values as a script uses them (0 or -1 with the error set): handle.var = v (v given away),
 * handle.var == text, handle.var[0:len(prefix)] == prefix, cmp(value, n) (-2 on an error), and
 * Scrap.AddScheduledFunc(Scrap.GetTime() + delay, func, args[, name]) (args given away; name NULL for none) */
byte Script_HSet(PyObject *e, const char *var, PyObject *v);
int Script_HIs(PyObject *e, const char *var, const char *text);
int Script_HPrefix(PyObject *e, const char *var, const char *prefix);
int Script_NumCmp(PyObject *a, long n);
byte Script_Later(double delay, PyObject *func, PyObject *args, const char *name);
byte Script_LaterIn(double delay, const char *module, const char *func, PyObject *args, const char *name);

/* script variables of entities by name, as the Python entity handle reads and writes them: the number of a variable
 * (int, else float; def when the entity or the variable is not there), its text (def likewise), and ent.var = v (v
 * given away; 0, or -1 with the message printed) */
double Script_EntNum(const char *ent, const char *var, double def);
const char *Script_EntStr(const char *ent, const char *var, const char *def);
int Script_EntSet(const char *ent, const char *var, PyObject *v);
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
/* a % b of a script (the string format for a string) */
PyObject *Script_Mod(PyObject *a, PyObject *b);
/* o == "text" (a string equals only a string), o in ("text", ...) (the texts end with NULL) */
byte Script_Is(PyObject *o, const char *text);
byte Script_In(PyObject *o, const char *const *texts);
/* `if o:`, a == b: 1, 0, or -1 with the error set (also for NULL) */
int Script_True(PyObject *o);
int Script_Eq(PyObject *a, PyObject *b);
/* int(o), `o` (new references, NULL stays NULL) */
PyObject *Script_Int(PyObject *o);
PyObject *Script_Repr(PyObject *o);
/* the count of range(n): 0 with the error of range set */
byte Script_Range(PyObject *n, long *count);
/* item i of a for loop over seq: 1 with a new reference in *item, 0 at the end, -1 with the error set */
int Script_Next(PyObject *seq, int i, PyObject **item);
/* the print statement with its items, as ceval prints them: 0 with the error set */
byte Script_Print(PyObject *const *items, int n);
/* a, b, ... = v: n new references into out, 0 with the error set */
byte Script_Unpack(PyObject *v, int n, PyObject **out);
/* Scrap.GetLangStr(key) (key given away), Scrap.GetSaveVar(name), Scrap.GetEntity(name), Scrap.UsrEntity(0): new
 * references, NULL with the error set */
PyObject *Script_LangOf(PyObject *key);
PyObject *Script_SaveVar(const char *name);
PyObject *Script_Entity(PyObject *name);
PyObject *Script_Usr(void);
/* module.func(*args, **kw) as a script calls it (args and kw given away, kw may be NULL): the result, a new
 * reference, NULL with the error set (not printed) */
PyObject *Script_AskKw(const char *module, const char *func, PyObject *args, PyObject *kw);
/* `global last; last = target.name; target.name = name` in a function of the script module (name its global, the
 * chain of callbacks the net weapons and Vehicles put in front of Net's): 0 with the error set */
byte Script_Hook(const char *module, const char *target, const char *name, const char *last);
/* eval(text) of a value the game wrote (save and engine variables, network messages, event names, data lines) in
 * globals: the checks of builtin eval (TypeError, ValueError), then Script_Literal of the text after its leading
 * blanks; any other text raises Script_NotRead's SyntaxError, no Python runs. Script_EvalIn the same in the globals
 * of a script module. A new reference, NULL with the error set */
PyObject *Script_EvalValue(PyObject *globals, PyObject *text);
PyObject *Script_EvalIn(const char *module, PyObject *text);
/* the rest of Script_EvalValue for a text s (leading blanks skipped) that the literal reader did not take: a name
 * chain as eval runs it (its value or NameError), else Script_NotRead's SyntaxError, for two names or numbers with
 * blanks between them with the 1.5.2 parser's message and position. Also used by Native_EvalText. NULL with the error
 * set, or a new reference */
PyObject *Script_EvalOther(PyObject *globals, const char *s);
/* the error for a text of Python that no C reader takes and nothing runs: SyntaxError "invalid syntax" (the last
 * line of the traceback eval or exec print for it), noted in the gate log (SCRAP_PY_GATE, event "notread", where
 * names the reader); NULL */
PyObject *Script_NotRead(const char *where, const char *text);
/* what eval of s (after the blanks eval skips) in globals gives when s is a plain literal: an int, long or float in
 * decimal (with exponent), a string in single or double quotes (with escapes), None, a list, tuple or dict of those,
 * '-' before a number, blanks between (the constants shared and interned as the compiler makes them). A new
 * reference, else NULL without an error (anything else, also when the globals lack valid __builtins__) */
PyObject *Script_Literal(PyObject *globals, const char *s);
/* the globals give a frame valid builtins (a dict or a module in __builtins__), as exec and eval need */
byte Script_HasBuiltins(PyObject *globals);
/* the plain literal text in the globals of __main__ (Script_Literal), NULL without an error for any other text */
PyObject *Script_EvalMain(const char *text);
/* a string constant of len chars as the compiler makes it (interned when it has only name characters) */
PyObject *Script_StrConst(const char *s, int len);
/* s[0:len] is a Python 1.5.2 name (ASCII letters, digits and _, not a keyword) */
byte Script_IsName(const char *s, int len);
/* o is a string that between single quotes in a line of Python is itself again */
byte Script_IsQuotable(PyObject *o);
/* a NAME(.NAME)* text evaluated with the locals, the globals and their builtins (no blanks; each name may be followed
 * by subscripts [n] of a decimal int as repr writes it, "ShipEdit.editedShips[3].Model"): 1 with a new reference in
 * *out, -1 with the error of the lookup set (NameError, AttributeError, IndexError), 0 when text is not such a
 * chain */
int Script_NameChain(PyObject *locals, PyObject *globals, const char *text, PyObject **out);

/*
 * A ported script function line by line (src/game/Script/ScriptFrame.c). The frame keeps the temporary references
 * and the error state: every Sf_ helper returns a reference the frame keeps (borrowed by the caller) and takes its
 * arguments borrowed. After the first error each helper does nothing and returns NULL, -1 or 0, as the script stops
 * at the line that raised; Sf_Done releases the references and gives NULL with the error left set. Conditions are 1,
 * 0, or -1 after an error.
 */
#define SCRIPT_FRAME_REFS 160
typedef struct scriptFrame_s {
	PyObject *refs[SCRIPT_FRAME_REFS];
	int n;
	byte failed;
} scriptFrame_t;

/* the start of a ported function (self, args): the frame f and the arguments as PyArg_ParseTuple(args, ...) parses
 * them (a bad call returns NULL) */
#define SF_ARGS(...) \
	scriptFrame_t f = { 0 }; \
	(void)self; \
	if (!PyArg_ParseTuple(args, __VA_ARGS__)) { \
		return NULL; \
	}
/* o (a new reference or NULL with the error set) into the frame */
PyObject *Sf_Keep(scriptFrame_t *f, PyObject *o);
/* ok, else the frame fails (the error set); the frame is still good */
byte Sf_Check(scriptFrame_t *f, int ok);
/* the references kept after mark (f->n before a loop turn) */
void Sf_Release(scriptFrame_t *f, int mark);
/* the references kept after mark released, except the n values of keep (kept by the frame, NULL skipped): the
 * values a loop turn carries to the next one */
void Sf_ReleaseKeep(scriptFrame_t *f, int mark, PyObject *const *keep, int n);
/* a bare `except:` after the lines of a `try:` block: 1 when one of them raised (the error is cleared and the frame
 * goes on), else 0 */
byte Sf_Except(scriptFrame_t *f);
/* the end of the function: result (a new reference) or NULL when the frame failed; Sf_Return(o) with o borrowed,
 * Sf_None for None */
PyObject *Sf_Done(scriptFrame_t *f, PyObject *result);
PyObject *Sf_Return(scriptFrame_t *f, PyObject *o);
PyObject *Sf_None(scriptFrame_t *f);
/* raise exc(msg): 0 */
byte Sf_Raise(scriptFrame_t *f, PyObject *exc, const char *msg);
/* raise "text" (a string exception of Python 1.5.2, the value None): 0 */
byte Sf_RaiseText(scriptFrame_t *f, const char *text);
/* values: a borrowed o kept, an int, a float, a string, Py_BuildValue */
PyObject *Sf_Ref(scriptFrame_t *f, PyObject *o);
PyObject *Sf_Int(scriptFrame_t *f, long i);
PyObject *Sf_Float(scriptFrame_t *f, double d);
PyObject *Sf_Str(scriptFrame_t *f, const char *s);
PyObject *Sf_Build(scriptFrame_t *f, const char *fmt, ...);
/* import name; a global of a module as its functions read it (NameError when missing), global name = v, del name;
 * module.name and module.name = v as another script does them */
byte Sf_Import(scriptFrame_t *f, const char *name);
PyObject *Sf_Global(scriptFrame_t *f, const char *module, const char *name);
byte Sf_SetGlobal(scriptFrame_t *f, const char *module, const char *name, PyObject *v);
byte Sf_DelGlobal(scriptFrame_t *f, const char *module, const char *name);
PyObject *Sf_Mod(scriptFrame_t *f, const char *module, const char *name);
/* `from module import name` (ImportError "cannot import name" when the module has no such name) */
PyObject *Sf_From(scriptFrame_t *f, const char *module, const char *name);
byte Sf_SetMod(scriptFrame_t *f, const char *module, const char *name, PyObject *v);
/* o.name, o.name = v, o[i], o[key], o[key] = v, del o[key], o[lo:hi] */
PyObject *Sf_Attr(scriptFrame_t *f, PyObject *o, const char *name);
byte Sf_SetAttr(scriptFrame_t *f, PyObject *o, const char *name, PyObject *v);
byte Sf_SetAttrI(scriptFrame_t *f, PyObject *o, const char *name, long i);
byte Sf_SetAttrS(scriptFrame_t *f, PyObject *o, const char *name, const char *s);
PyObject *Sf_Item(scriptFrame_t *f, PyObject *o, long i);
PyObject *Sf_GetItem(scriptFrame_t *f, PyObject *o, PyObject *key);
byte Sf_SetItem(scriptFrame_t *f, PyObject *o, PyObject *key, PyObject *v);
byte Sf_DelItem(scriptFrame_t *f, PyObject *o, PyObject *key);
PyObject *Sf_Slice(scriptFrame_t *f, PyObject *o, int lo, int hi);
/* a op b with op one of + - * / %, a op int; a + b + ... of fmt items ('s' a C string, 'O' an object); `o` */
PyObject *Sf_Op(scriptFrame_t *f, PyObject *a, char op, PyObject *b);
PyObject *Sf_OpI(scriptFrame_t *f, PyObject *a, char op, long b);
PyObject *Sf_Cat(scriptFrame_t *f, const char *fmt, ...);
PyObject *Sf_Repr(scriptFrame_t *f, PyObject *o);
PyObject *Sf_ReprI(scriptFrame_t *f, long i);
/* int(o), float(o) */
PyObject *Sf_ToInt(scriptFrame_t *f, PyObject *o);
PyObject *Sf_ToFloat(scriptFrame_t *f, PyObject *o);
/* `if o:`, a op b with op "==", "!=", "<", "<=", ">", ">=", a == "text", a == i, x in seq, x in ("text", ..., NULL),
 * len(o) */
int Sf_True(scriptFrame_t *f, PyObject *o);
int Sf_Compare(scriptFrame_t *f, PyObject *a, const char *op, PyObject *b);
int Sf_EqS(scriptFrame_t *f, PyObject *a, const char *s);
int Sf_EqI(scriptFrame_t *f, PyObject *a, long i);
int Sf_In(scriptFrame_t *f, PyObject *x, PyObject *seq);
int Sf_InS(scriptFrame_t *f, PyObject *x, const char *const *texts);
int Sf_Len(scriptFrame_t *f, PyObject *o);
/* item i of `for x in seq` (as ceval loops: NULL at the end without an error, or with the error of the frame) */
PyObject *Sf_Next(scriptFrame_t *f, PyObject *seq, int i);
/* callable(args) (args given away), callable(Py_BuildValue args), module.func(args), o.name(args) */
PyObject *Sf_CallArgs(scriptFrame_t *f, PyObject *callable, PyObject *args);
PyObject *Sf_Call(scriptFrame_t *f, PyObject *callable, const char *fmt, ...);
/* callable(args, **kw) (args given away, kw a dict) */
PyObject *Sf_CallKw(scriptFrame_t *f, PyObject *callable, PyObject *args, PyObject *kw);
PyObject *Sf_Ask(scriptFrame_t *f, const char *module, const char *func, const char *fmt, ...);
/* func(args) of a ported module as its own functions call it: the global func of module (NameError when missing,
 * a function a mission put there is called) */
PyObject *Sf_Own(scriptFrame_t *f, const char *module, const char *func, const char *fmt, ...);
PyObject *Sf_Method(scriptFrame_t *f, PyObject *o, const char *name, const char *fmt, ...);
/* Scrap.GetEntity(name), Scrap.GetLangStr(key), Scrap.GetTime() */
PyObject *Sf_Entity(scriptFrame_t *f, PyObject *name);
PyObject *Sf_Lang(scriptFrame_t *f, PyObject *key);
PyObject *Sf_Time(scriptFrame_t *f);
/* Scrap.AddScheduledFunc(time, func, args[, name]) and the same at Scrap.GetTime() + delay (name NULL for none) */
byte Sf_Schedule(scriptFrame_t *f, PyObject *time, PyObject *func, PyObject *args, const char *name);
byte Sf_Later(scriptFrame_t *f, PyObject *delay, PyObject *func, PyObject *args, const char *name);
/* print with fmt items ('s' a C string, 'O' an object), a, b = v, exec code in a function of module (locals a dict
 * of its variables: a line in the forms the game execs runs in C, "import M", calls of names and attributes with
 * literal arguments, an attribute set to a literal, split by ';'; STAGE 7: any other line still runs in Python) */
byte Sf_Print(scriptFrame_t *f, const char *fmt, ...);
byte Sf_Unpack(scriptFrame_t *f, PyObject *v, int n, PyObject **out);
byte Sf_Exec(scriptFrame_t *f, PyObject *code, const char *module, PyObject *locals);
/* eval(text) in a function of module: Script_EvalValue in the module's globals (a literal, else SyntaxError) */
PyObject *Sf_Eval(scriptFrame_t *f, PyObject *text, const char *module);
/* exec("import " + name) in a function of module (locals: the dict of its variables, the import is stored there);
 * a name that is not a plain module name goes to Sf_Exec */
byte Sf_ImportTo(scriptFrame_t *f, const char *module, PyObject *locals, PyObject *name);
/* exec("import " + target + ";" + target + "." + func + "()") in a function of module (locals as for Sf_ImportTo),
 * func a string; one that is not a name goes to Sf_Exec (its SyntaxError) */
byte Sf_ImportCall(scriptFrame_t *f, const char *module, PyObject *locals, const char *target, PyObject *func);
/* callable(Py_BuildValue args) as a statement of a script: the result is dropped at once (no reference kept) */
byte Sf_Line(scriptFrame_t *f, PyObject *callable, const char *fmt, ...);
/* the C string of a text of the script, an item name: another value raises the TypeError of the name argument of
 * the SScorer functions; NULL after an error */
const char *Sf_Text(scriptFrame_t *f, PyObject *o);
/* SScorer.Set(id, item, var, v) as a line of a script: a text, a number or (any other value) the engine function
 * with its TypeError; the same with a C number and a C text; 0 after an error */
byte Sf_ScorerSet(scriptFrame_t *f, int id, const char *item, const char *var, PyObject *v);
byte Sf_ScorerNum(scriptFrame_t *f, int id, const char *item, const char *var, double d);
byte Sf_ScorerStr(scriptFrame_t *f, int id, const char *item, const char *var, const char *s);
/* SScorer.Get(id, item, var) as a number (0 when the item or the variable is not there), nothing after an error */
double Sf_ScorerGet(scriptFrame_t *f, int id, const char *item, const char *var);
/* SScorer.SetDefault(id, item), nothing after an error */
byte Sf_ScorerDefault(scriptFrame_t *f, int id, const char *item);
/* module.func(Py_BuildValue args) as a statement: the result dropped at once */
byte Sf_Run(scriptFrame_t *f, const char *module, const char *func, const char *fmt, ...);
/*
 * the frame's calls of port functions through their refs (Port_Ask, include/scrap/scriptnative.h): fmt and the
 * arguments of PORT_CALL (v, i a long, d, s, O; keywords after ';'), not of Py_BuildValue. Nothing after an error.
 *   SF_CALL(f, Module, Func, fmt, ...)    Sf_Ask: the result kept by the frame
 *   SF_OWN(f, Module, Func, fmt, ...)     Sf_Own: through the module's global
 *   SF_RUN(f, Module, Func, fmt, ...)     Sf_Run: as a statement, the result dropped at once
 *   SF_LCALL, SF_LOWN, SF_LRUN            the same for a module of a level (PORT_LEVEL)
 */
struct portSite_s;
PyObject *Sf_Port(scriptFrame_t *f, struct portSite_s *site, const char *fmt, ...);
byte Sf_PortRun(scriptFrame_t *f, struct portSite_s *site, const char *fmt, ...);
#define SF_CALL(f, module, name, ...) Sf_Port(f, PORT_SITE(port_##module##_##name, 0), __VA_ARGS__)
#define SF_OWN(f, module, name, ...) Sf_Port(f, PORT_SITE(port_##module##_##name, 1), __VA_ARGS__)
#define SF_RUN(f, module, name, ...) Sf_PortRun(f, PORT_SITE(port_##module##_##name, 0), __VA_ARGS__)
#define SF_LCALL(f, module, name, ...) Sf_Port(f, PORT_SITE(PORT_LREF_(PORT_LEVEL, module##_##name), 0), __VA_ARGS__)
#define SF_LOWN(f, module, name, ...) Sf_Port(f, PORT_SITE(PORT_LREF_(PORT_LEVEL, module##_##name), 1), __VA_ARGS__)
#define SF_LRUN(f, module, name, ...) Sf_PortRun(f, PORT_SITE(PORT_LREF_(PORT_LEVEL, module##_##name), 0), __VA_ARGS__)
/*
 * the tables of the level scripts (scenes, ship profiles): a value as text (len -1 for the whole text) is an int
 * ("205"), a float ("0.7"), "%Module.name" (an attribute of a module), "^Module.Func:text" (Module.Func("text")),
 * "'text" (always a string) or else a string; Sf_Calls runs lines "Module.Func|value|..." ("@Func" a global of module,
 * as its own functions call it) in order as statements (the table ends with NULL); Sf_SetAttrs sets "name=value"
 * pairs as o.name = value
 */
PyObject *Sf_Value(scriptFrame_t *f, const char *module, const char *text, int len);
byte Sf_Calls(scriptFrame_t *f, const char *module, const char *const *lines);
byte Sf_SetAttrs(scriptFrame_t *f, PyObject *o, const char *module, const char *const *pairs);
/* Speech.PlayerName = usr; Speech.NPCName = npc. Speech.OnEndOfSpeech = fn; Speech.OnEndOfSpeechParams = params
 * (NULL for ()). Speech.NISpeech(who) (who NULL: no argument); Speech.AutoAddNI(key); Speech.RunNI() */
byte Sf_SpeechNames(scriptFrame_t *f, PyObject *usr, PyObject *npc);
byte Sf_SpeechEnd(scriptFrame_t *f, PyObject *fn, PyObject *params);
byte Sf_NISpeech(scriptFrame_t *f, const char *who, const char *key);
/* Scrap.GetEntity(name).var = v, a text, an int (v borrowed) */
byte Sf_EntSet(scriptFrame_t *f, const char *name, const char *var, PyObject *v);
byte Sf_EntSetS(scriptFrame_t *f, const char *name, const char *var, const char *s);
byte Sf_EntSetI(scriptFrame_t *f, const char *name, const char *var, long i);
/* Menu.DrawScorer(0, SInput.GetActionSet()) */
byte Sf_DrawScorer(scriptFrame_t *f);
/* a text of a mission scorer as the missions set it: SScorer.Add(id, item, type) (type NULL: none), X, Y, Font,
 * Effect (NULL: none), Text, Align, the colour Scorer.MissionScorerText_Color and Visible (NULL: none) */
byte Sf_MissionText(scriptFrame_t *f, PyObject *id, const char *item, const char *type, PyObject *x, PyObject *y,
	const char *font, const char *effect, PyObject *text, const char *align, PyObject *visible);
/* a 30 x 19 sprite behind a number of a mission scorer: Add, X, Y, ScaleX, ScaleY, Unit 1, Alpha, Red, Green, Blue */
byte Sf_MissionSprite(scriptFrame_t *f, PyObject *id, const char *item, PyObject *x, PyObject *y, int alpha, int r, int g, int b);

/* the C video options of PCMenu (src/game/Script/MenuVideo.c) */
extern PyMethodDef g_MenuVideoMethods[];
/* the C options and controls menus of PCMenu (src/game/Script/MenuControls.c) */
extern PyMethodDef g_MenuControlsMethods[];
/* the C multiplayer menus of PCMenu (src/game/Script/MenuNet.c) */
extern PyMethodDef g_MenuNetMethods[];
/* the helpers of pcmenu.py on the variables of its video menu (src/game/Script/MenuVideoVars.c) */
extern PyMethodDef g_MenuVideoVarsMethods[];
/* the C building blocks of the Menu module (src/game/Script/MenuBase.c) */
extern PyMethodDef g_MenuBaseMethods[];
/* the C main menus of the Menu module (src/game/Script/MenuMain.c) */
extern PyMethodDef g_MenuMainMethods[];
/* the C mission info pages of the Menu module (src/game/Script/MenuMission.c) */
extern PyMethodDef g_MenuMissionMethods[];
/* the C HUD: the Scorer, CharScorer and RacerScorer modules (src/game/Script/MenuHud.c) */
extern PyMethodDef g_ScorerMethods[];
extern PyMethodDef g_CharScorerMethods[];
extern PyMethodDef g_RacerScorerMethods[];
/* the C Chars module (src/game/Script/Chars.c) */
extern PyMethodDef g_CharsMethods[];
/* the C MakeChar module (src/game/Script/MakeChar.c) */
extern PyMethodDef g_MakeCharMethods[];
/* the C CharAct module (src/game/Script/CharAct.c) */
extern PyMethodDef g_CharActMethods[];
/* the C Speech module (src/game/Script/Speech.c) */
extern PyMethodDef g_SpeechMethods[];
/* the C SaveGame module (src/game/Script/SaveGame.c) */
extern PyMethodDef g_SaveGameMethods[];
/* the C Fx module (src/game/Script/Fx.c, data/fx/fx.def) */
extern PyMethodDef g_FxMethods[];
/* the C Items module (src/game/Script/Items.c) */
extern PyMethodDef g_ItemsMethods[];
extern PyMethodDef g_NetMethods[];
/* the online game modes (src/game/Script/NetModes.c) */
extern PyMethodDef g_NDeathMatchMethods[];
extern PyMethodDef g_NDeathMatchBotsMethods[];
extern PyMethodDef g_NTeamDMMethods[];
extern PyMethodDef g_NTeamFlagMethods[];
extern PyMethodDef g_NFlagHuntMethods[];
extern PyMethodDef g_NPostmanMethods[];
extern PyMethodDef g_NTeamPostMethods[];
extern PyMethodDef g_WeaponsMethods[];
extern PyMethodDef g_VulcanMethods[];
extern PyMethodDef g_DevastatorMethods[];
extern PyMethodDef g_TeslaMethods[];
extern PyMethodDef g_ATPCMethods[];
extern PyMethodDef g_LaserMethods[];
extern PyMethodDef g_SwarmMethods[];
extern PyMethodDef g_InfernoMethods[];
extern PyMethodDef g_CloudMethods[];
extern PyMethodDef g_EMIMethods[];
extern PyMethodDef g_SonicMethods[];
extern PyMethodDef g_SputnikInterfaceMethods[];
/* the ship editor (src/game/Script/ShipEdit.c, HangarTab.c, data/menus/shipedit.def) */
extern PyMethodDef g_ShipEditMethods[];
extern PyMethodDef g_HangarTabMethods[];
extern PyMethodDef g_VehiclesMethods[];
extern PyMethodDef g_VehiclesNPCMethods[];
extern PyMethodDef g_SplitMethods[];
extern PyMethodDef g_SDeathMatchMethods[];
extern PyMethodDef g_SFlagHuntMethods[];
extern PyMethodDef g_SPostmanMethods[];
extern PyMethodDef g_SRaceMethods[];
/* the functions of the Init module (src/game/Script/InitLevel.c) */
extern PyMethodDef g_InitMethods[];
/* the functions string.py keeps in Python (src/game/Script/String.c) */
extern PyMethodDef g_StringMethods[];
/* the sound scripts of the characters (src/game/Script/Sounds.c, data/sound/) */
extern PyMethodDef g_BankDirectorSoundMethods[];
extern PyMethodDef g_BankMasterSoundMethods[];
extern PyMethodDef g_BertoSoundMethods[];
extern PyMethodDef g_BettySoundMethods[];
extern PyMethodDef g_BishopSoundMethods[];
extern PyMethodDef g_BossSoundMethods[];
extern PyMethodDef g_CrazyGamblerSoundMethods[];
extern PyMethodDef g_DesktopSoundMethods[];
extern PyMethodDef g_DtritusSoundMethods[];
extern PyMethodDef g_FunctionarySoundMethods[];
extern PyMethodDef g_GateKeeperSoundMethods[];
extern PyMethodDef g_GearSoundMethods[];
extern PyMethodDef g_HumanSoundMethods[];
extern PyMethodDef g_HumphreySoundMethods[];
extern PyMethodDef g_MaintenanceSoundMethods[];
extern PyMethodDef g_MayorSoundMethods[];
extern PyMethodDef g_MercenarySoundMethods[];
extern PyMethodDef g_MessengerSoundMethods[];
extern PyMethodDef g_NurseSoundMethods[];
extern PyMethodDef g_PoliceBossSoundMethods[];
extern PyMethodDef g_PoliceSoundMethods[];
extern PyMethodDef g_RustySoundMethods[];
extern PyMethodDef g_SebastianSoundMethods[];
extern PyMethodDef g_SentinelSoundMethods[];
extern PyMethodDef g_SputnikJunkyardSoundMethods[];
extern PyMethodDef g_SputnikSoundMethods[];
extern PyMethodDef g_TestCSoundMethods[];
/* the C CharsNPC module (src/game/Script/CharsNPC.c) */
extern PyMethodDef g_CharsNPCMethods[];
/* the C parts of the character scripts, their Init runs data/chars/<name>.def (src/game/Script/Characters.c) */
extern PyMethodDef g_BettyMethods[];
extern PyMethodDef g_BankDirectorMethods[];
extern PyMethodDef g_BossMethods[];
extern PyMethodDef g_CrazyGamblerMethods[];
extern PyMethodDef g_HumphreyMethods[];
extern PyMethodDef g_MaintenanceMethods[];
extern PyMethodDef g_MayorMethods[];
extern PyMethodDef g_RustyMethods[];
extern PyMethodDef g_SebastianMethods[];
extern PyMethodDef g_SentinelMethods[];
extern PyMethodDef g_SputnikJunkyardMethods[];
extern PyMethodDef g_BishopMethods[];
extern PyMethodDef g_FunctionaryMethods[];
extern PyMethodDef g_GearMethods[];
extern PyMethodDef g_BankMasterMethods[];
extern PyMethodDef g_BertoMethods[];
extern PyMethodDef g_NurseMethods[];
extern PyMethodDef g_DesktopMethods[];
extern PyMethodDef g_PoliceMethods[];
extern PyMethodDef g_GateKeeperMethods[];
extern PyMethodDef g_DtritusMethods[];
extern PyMethodDef g_SputnikMethods[];
extern PyMethodDef g_PoliceBossMethods[];
extern PyMethodDef g_MessengerMethods[];
extern PyMethodDef g_HumanMethods[];
extern PyMethodDef g_MercenaryAMethods[];
extern PyMethodDef g_MercenaryBMethods[];
extern PyMethodDef g_MercenaryCMethods[];
extern PyMethodDef g_MercenaryLMethods[];

/* the start of an indoor map (src/game/Script/InMap.c) and the desks of the functionaries
 * (src/game/Script/Bureaucracy.c) */
extern PyMethodDef g_InMapMethods[];
extern PyMethodDef g_BureaucracyMethods[];
/* the chat of a network game (src/game/Script/Chat.c) and the credits text (src/game/Script/CreditsText.c) */
extern PyMethodDef g_ChatMethods[];
extern PyMethodDef g_CreditsTextMethods[];
/* the base of the online game modes (src/game/Script/Ffa.c) */
extern PyMethodDef g_FfaMethods[];

/* the mission scripts: the traffic and the police (src/game/Script/Traffic.c), the phone cabs and Dtritus' desktop
 * (src/game/Script/PhoneCab.c), the metro (src/game/Script/Metro.c) and the challenges (src/game/Script/Challenge.c) */
extern PyMethodDef g_TrafficMethods[];
extern PyMethodDef g_OutPoliceMethods[];
extern PyMethodDef g_InPoliceMethods[];
extern PyMethodDef g_PhoneCabMethods[];
extern PyMethodDef g_DTritusDesktopMethods[];
extern PyMethodDef g_MetroMethods[];
extern PyMethodDef g_ChallengeMethods[];
/* the options of a challenge against foe of the kind type ("Race", "Combat") on the entity ch, with the rewards or
 * the lives they cost (Challenge.SetChallengeOptions, PhoneCab.SetRevengeOptions): 0 with the error set */
struct nativeVal_s;
byte Challenge_SetOptions(const struct nativeVal_s *ch, const struct nativeVal_s *type, const struct nativeVal_s *foe);
/* the missions (issue #2 step 8a): the alarm and the news panels (src/game/Script/Alarm.c), the doors, elevators and
 * teleporters (src/game/Script/Doors.c), the items and the crazy wind (src/game/Script/MisItems.c), the mission flow
 * (src/game/Script/MissionsFuncs.c) */
extern PyMethodDef g_AlarmMethods[];
extern PyMethodDef g_NewsPanelMethods[];
extern PyMethodDef g_DoorsMethods[];
extern PyMethodDef g_ElevatorMethods[];
extern PyMethodDef g_TeleportMethods[];
extern PyMethodDef g_MisItemsMethods[];
extern PyMethodDef g_CrazyWingMethods[];
extern PyMethodDef g_MissionsFuncsMethods[];
/* the cut scenes (src/game/Script/Scene.c), the character conversors and the possession
 * (src/game/Script/CharConversor.c), the races and combats of the challenges (src/game/Script/RaceMaker.c) */
extern PyMethodDef g_SceneMethods[];
extern PyMethodDef g_CharConversorMethods[];
extern PyMethodDef g_RaceMakerMethods[];

/* the general sounds: the sound files of a level, the ship and damage sounds, the point sounds of the maps and the door
 * sounds (src/game/Script/GameSounds.c, data/sound/sound.def, outsound.def, doorsound.def) */
extern PyMethodDef g_SoundMethods[];
extern PyMethodDef g_OutSoundMethods[];
extern PyMethodDef g_DoorSoundMethods[];
/* the start of an outdoor level and its spawn points, the fake traffic indoors (src/game/Script/OutMap.c) */
extern PyMethodDef g_OutMapMethods[];
extern PyMethodDef g_InTrafficMethods[];
/* the ship tables and SetShipParams (src/game/Script/ShipMaker.c, data/vehicles/shipmaker.def), the player's ship,
 * boarding, parking and the parked ships, the speed rings (src/game/Script/Parking.c) */
extern PyMethodDef g_ShipMakerMethods[];
extern PyMethodDef g_ParkingMethods[];
extern PyMethodDef g_AcceleratorMethods[];
/* the game modes of the single player super deals (src/game/Script/SuperDeals.c) */
extern PyMethodDef g_DeathMatchMethods[];
extern PyMethodDef g_TeamDeathMatchMethods[];
extern PyMethodDef g_VersusMethods[];
extern PyMethodDef g_CTFMethods[];
extern PyMethodDef g_DominationMethods[];
extern PyMethodDef g_MortalRaceMethods[];
/* the tabs of the ship editor: engines, weapons and hull (src/game/Script/EnginesTab.c, WeaponsTab.c, HullTab.c,
 * data/menus/enginestab.def, weaponstab.def, hulltab.def) */
extern PyMethodDef g_EnginesTabMethods[];
extern PyMethodDef g_WeaponsTabMethods[];
extern PyMethodDef g_HullTabMethods[];

/* the gameplay functions of the character modules (src/game/Script/CharFuncs.c), next to their Init tables */
extern PyMethodDef g_FunctionaryFuncs[];
extern PyMethodDef g_RustyFuncs[];
extern PyMethodDef g_BossFuncs[];
extern PyMethodDef g_MayorFuncs[];
extern PyMethodDef g_MaintenanceFuncs[];
extern PyMethodDef g_SputnikJunkyardFuncs[];
extern PyMethodDef g_CrazyGamblerFuncs[];
extern PyMethodDef g_GearFuncs[];
extern PyMethodDef g_PoliceFuncs[];
extern PyMethodDef g_BishopFuncs[];
extern PyMethodDef g_MessengerFuncs[];
/* the siren actions and missile events of Gear and Police from their files (src/game/Script/Characters.c) */
extern PyMethodDef g_GearDefFuncs[];
extern PyMethodDef g_PoliceDefFuncs[];
/* Sputnik's ship functions (src/game/Script/Sputnik.c) and the gatekeepers (src/game/Script/GateKeeper.c) */
extern PyMethodDef g_SputnikFuncs[];
extern PyMethodDef g_GateKeeperFuncs[];

#endif
