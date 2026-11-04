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

/* a step of a .menu file that C does ("run <name>") */
typedef struct scriptStep_s {
	const char *name;
	void (*fn)(int id);
} scriptStep_t;

/*
 * runs the .menu file name (scrapc/<name> of the packs, else builtin) for the scorer of player id; steps are the
 * C steps it may run (NULL ends the table). 0 when it could not be read.
 */
byte Script_RunMenu(int id, const char *name, const char *builtin, const scriptStep_t *steps);

/* the scorer items of player id */
void Script_Add(int id, const char *item, const char *type);
void Script_SetStr(int id, const char *item, const char *var, const char *s);
void Script_SetNum(int id, const char *item, const char *var, double d);
/* "" when the item or the variable is not there; the string of the item, valid until it changes */
const char *Script_GetStr(int id, const char *item, const char *var);
double Script_GetNum(int id, const char *item, const char *var);
void Script_SetDefault(int id, const char *item);

/* a text of the language ("" when it is not there) */
const char *Script_Lang(const char *key);

/* module.func(args) of a script, args as Py_BuildValue (a tuple, "(is)"); 0 when it failed (the error printed) */
byte Script_Call(const char *module, const char *func, const char *fmt, ...);
/* the same with a tuple of arguments and a dict of keywords (NULL for none), both given away */
byte Script_CallKw(const char *module, const char *func, PyObject *args, PyObject *kw);
/* an integer variable of a script module (def when it is not there) */
int Script_GetInt(const char *module, const char *var, int def);

/* the arguments of a menu callback: the player and the item (empty when the callback got none) */
byte Script_Args(PyObject *args, int *id, const char **control);

/* the C video options of PCMenu (src/game/Script/MenuVideo.c) */
extern PyMethodDef g_MenuVideoMethods[];

#endif
