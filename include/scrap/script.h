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

#include "scrap/python.h"

/* the C functions of one pack module */
typedef struct scriptModule_s {
	const char *name;		/* the module name of the script, PCMenu */
	PyMethodDef *methods;		/* put into the module under their names, the table ends with a NULL name */
} scriptModule_t;

/* a module of the packs is loaded: its C functions go into it */
void Script_ModuleLoaded(const char *name, PyObject *module);

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

/* the value of a Python expression (the save variables keep lists as their text), NULL when it fails */
PyObject *Script_Eval(const char *text);

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

/* the C video options of PCMenu (src/game/Script/MenuVideo.c) */
extern PyMethodDef g_MenuVideoMethods[];
/* the C options and controls menus of PCMenu (src/game/Script/MenuControls.c) */
extern PyMethodDef g_MenuControlsMethods[];
/* the C multiplayer menus of PCMenu (src/game/Script/MenuNet.c) */
extern PyMethodDef g_MenuNetMethods[];
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

#endif
