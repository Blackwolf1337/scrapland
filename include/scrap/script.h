/*
 * ScrapC's own versions of the scripts of the game packs (issue #2). The game runs its menus, HUD, characters and
 * missions from Python 1.5.2 scripts in the packs; they move to C one by one while the rest keeps running in Python.
 *
 * The loader: when the engine loads a pack module (Pack_PreloadLibrary), Script_ModuleLoaded puts the C functions
 * ScrapC has for that module into it under their script names. The pack's own code, the callbacks named in strings
 * ("PCMenu.ChangeVideoMode") and other modules then reach the C versions. The layout of a menu is a .menu file
 * (Script_RunMenu): built into ScrapC, a pack in mods/ can replace it (scrapc/<name>, e.g. scrapc/menus/video.menu).
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

/* a step of a .menu file that C does ("run <name> [arg ...]"), args the tuple of the arguments */
typedef struct scriptStep_s {
	const char *name;
	void (*fn)(int id, PyObject *args);
} scriptStep_t;

/*
 * runs the .menu file name (scrapc/<name> of the packs, else builtin) for the scorer of player id: the lines before
 * the first section and those of section (NULL for a file without sections). steps are the C steps it may run (NULL
 * ends the table), vars a dict of the $names it may use (or NULL). 0 when it could not be read.
 */
byte Script_RunMenu(int id, const char *name, const char *section, const char *builtin, const scriptStep_t *steps,
	PyObject *vars);

/* the last item of the .menu file that runs ("" when none runs), for its steps */
const char *Script_CurrentItem(void);
/* a $name of the .menu file that runs, for its steps (value given away) */
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
/* the value of a Python expression (the save variables keep lists as their text), NULL when it fails */
PyObject *Script_Eval(const char *text);

/* the arguments of a menu callback: the player and the item (empty when the callback got none) */
byte Script_Args(PyObject *args, int *id, const char **control);

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

#endif
