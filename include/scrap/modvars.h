/*
 * The registry of the module variables of the pack modules (stage 5 of docs/objlib-plan.md). Not in the original.
 *
 * Per module file one static table of the names its module code gives (src/game/Script/ModVarTable.c, made by
 * tools/script/modvars.py from the .mod files and the C state of the ports): name, kind, the types of the values the
 * .mod lines give, flags and the C state of the port that keeps the variable. A name the table does not have (a mod's
 * own let line, a name a C hook, a load time call, a port or a test puts into the module) goes to the overflow list of
 * the module the first time the registry sees it, so mods keep working.
 *
 * The value of every name is still the slot of the module dict (OBJLIB 5, until batch 5h). A read through the
 * registry reads the dict, a write writes it, so a write through a path that still reaches the dict (eval in a module,
 * a raw dict of a test, an in-place change of a list another holder made) is seen at once; a name that is new to the
 * registry is taken into the overflow list when a lookup, a write or a walk of the names meets it. The test channel
 * (src/game/Global/TestChannel.c, TestExpr.c) and, since batch 5b, the helpers of the ports by module name
 * (Script_Global, Script_SetGlobal, Script_Attr, Script_SetVar, Native_Import, Native_Put, the Mode_ helpers of
 * scrap/scriptmodes.h and the LvN_ wrappers on them) and, since batch 5c, the module code of the .mod files (its
 * stores, loads and deletes, from the start of the module code on: ModVar_Making), the C hooks of module code, the .def
 * runner (its "%Module.name", setvar and object lines), Script_NameChain and Script_GetAttr read and write module
 * variables only through here.
 *
 * The 107 variables with a C place (the place text of the table) keep their value in the dict slot too, and the C
 * place follows it: the port refreshes it where it reads the variable (Mode_Read, Native_Import at its entry points)
 * and writes the dict and the place together (Mode_Put, Native_Put). Not the other way round while the dict exists,
 * because the dict object has readers the C place cannot follow: lists and dicts are shared and changed in place by
 * other holders (Mode_DictPut, Mode_ListAddG, the cases), the ports and the cases eval texts in a module's dict
 * (Native_EvalText, Script_EvalIn), a case can put a value of any type there and the port's TypeError has to come
 * where it reads it, and the cases reach the dict as X.__dict__ and __globals__.
 */
#ifndef SCRAP_MODVARS_H
#define SCRAP_MODVARS_H

#include "scrap/scriptnative.h"

/* the kind of a name: the .mod line that makes it */
enum {
	MVK_VAR,		/* let x = <expr>, or a variable only a port's C state names */
	MVK_FUNC,		/* def f */
	MVK_MODULE,		/* import A */
	MVK_IMPORT,		/* from M import a */
	MVK_CLASS,		/* class C (only in the dict until stage 7) */
	MVK_SYS,		/* __builtins__, __doc__, __file__, __name__ of every module */
	MVK_EXTRA		/* the overflow list: a name the table does not have */
};

/* the types of a variable: the NV_ mask of the values the .mod lines give it */
#define MVT_NONE (1 << NV_NONE)
#define MVT_INT (1 << NV_INT)
#define MVT_FLOAT (1 << NV_FLOAT)
#define MVT_STR (1 << NV_STR)
#define MVT_LONG (1 << NV_LONG)
#define MVT_LIST (1 << NV_LIST)
#define MVT_TUPLE (1 << NV_TUPLE)
#define MVT_DICT (1 << NV_DICT)
#define MVT_ANY NV_ANY

/* flags of a name */
#define MVF_COND 1		/* only a line in a branch of an if gives it */
#define MVF_DEL 2		/* a del line of the module code removes it again */

typedef struct modVar_s {
	const char *name;
	byte kind;			/* MVK_ */
	unsigned types;			/* MVT_ mask, 0 for a name that is no variable */
	byte flags;			/* MVF_ */
	const char *place;		/* the C state of the port that keeps it ("Chat.c chat.sendID[4]"), NULL: the dict only */
} modVar_t;

/* the table of one module file */
typedef struct modVarModule_s {
	const char *module;
	const char *level;		/* the level folder in lower case, NULL for the file of every level */
	const modVar_t *vars;		/* sorted by name (strcmp) */
	int count;
} modVarModule_t;

/* sorted by module, then level (the file of every level first) */
extern const modVarModule_t g_ModVarModules[];
extern const int g_ModVarModuleCount;

/* a module of the registry as it is loaded now: valid while the module lives, keep it only for the call (or, made by
 * ModVar_Making, while its module code runs) */
typedef struct modVarRef_s {
	int file;			/* the index of its table in g_ModVarModules (-1: a module the registry does not have,
					 * its dict alone; only ModVar_RefOfDict and ModVar_Making make such a ref, ModVar_Get,
					 * Set and Del take it, ModVar_Find gives NULL for it) */
	PyObject *dict;			/* the module dict (borrowed) */
} modVarRef_t;

/* Script_ModuleMake: the module name was made from the .mod file of level (NULL: of every level) into dict; the names
 * of the dict the table does not have go to the overflow list */
void ModVar_Made(const char *module, const char *level, PyObject *dict);
/* Script_ModuleMake before the module code: the same, so the module code's own stores (the let lines of the .mod text,
 * built in or of a mod pack) and loads go through the registry; *ref the module as the evaluator keeps it (ModVar_
 * RefOfDict), *was the binding it replaced, for ModVar_Unmade when the module code fails */
void ModVar_Making(const char *module, const char *level, PyObject *dict, modVarRef_t *ref, modVarRef_t *was);
/* the module code failed: the binding of the module's files as before ModVar_Making */
void ModVar_Unmade(const char *module, const modVarRef_t *was);
/* the registry module of a module object (the pack module of that name in sys.modules, made from a .mod file): 1
 * with *ref, else 0 (no error set) */
byte ModVar_Of(PyObject *module, modVarRef_t *ref);
/* the same for a module dict */
byte ModVar_OfDict(PyObject *dict, modVarRef_t *ref);
/* the same for a module name (only when it is loaded) */
byte ModVar_Named(const char *module, modVarRef_t *ref);
/* the ref of a namespace dict: its registry module (ModVar_OfDict), else the dict alone (file -1) */
void ModVar_RefOfDict(PyObject *dict, modVarRef_t *ref);
/* the entry of a name: of the table, or of the overflow list (the name of the dict taken in); NULL when the module
 * has no such name now */
const modVar_t *ModVar_Find(const modVarRef_t *ref, const char *name);
/* the value of a variable (borrowed), NULL without an error when it has none */
PyObject *ModVar_Get(const modVarRef_t *ref, const char *name);
/* a variable set (as the dict's SetItemString): 0, -1 with the error set */
int ModVar_Set(const modVarRef_t *ref, const char *name, PyObject *v);
/* a variable deleted (as the dict's DelItemString): 0, -1 with KeyError set when it has no value */
int ModVar_Del(const modVarRef_t *ref, const char *name);
/* the module's attribute name as getattr gives it (module_getattr of 1.5.2: __dict__ the dict, else the variable or
 * AttributeError(name)): a new reference, NULL with the error set */
PyObject *ModVar_Attr(const modVarRef_t *ref, const char *name);
/* the module's attribute set (v NULL: deleted) as setattr and delattr do it (module_setattr: TypeError for __dict__,
 * AttributeError "delete non-existing module attribute"): 0, -1 with the error set */
int ModVar_SetAttr(const modVarRef_t *ref, const char *name, PyObject *v);
/* the names that have a value now, sorted (strcmp): the table's and the overflow list's, the dict's new names taken
 * in first. *names is Mem_Alloc'd (Mem_Free it, NULL when there are none); the count */
int ModVar_Names(const modVarRef_t *ref, const char ***names);
/* the name of the module of a ref */
const char *ModVar_ModuleName(const modVarRef_t *ref);

/* ---- by module name: the module imported as the scripts import it (PyImport_ImportModule, its error), a pack module
 * through the registry, another module (Scrap, __main__, a pack module while its code runs) by its dict ---- */

/* the variable name of module as its code reads a global: a new reference, NULL when it has none (no error set, the
 * caller raises NameError or ImportError) or when the import failed (*imported 0, its error set) */
PyObject *ModVar_Lookup(const char *module, const char *name, byte *imported);
/* the global name of module set to v (not stolen): 0, -1 with the error set */
int ModVar_SetGlobal(const char *module, const char *name, PyObject *v);
/* the global name of module deleted: 0, -1 with the error of the import or NameError(name) */
int ModVar_DelGlobal(const char *module, const char *name);
/* getattr(module, name) of the imported module: a new reference, NULL with the error set */
PyObject *ModVar_ModAttr(const char *module, const char *name);
/* setattr(module, name, v) of the imported module (v not stolen, NULL: delattr): 0, -1 with the error set */
int ModVar_SetModAttr(const char *module, const char *name, PyObject *v);
/* getattr and setattr of any object (v NULL: delattr), a pack module through the registry */
PyObject *ModVar_ObjAttr(PyObject *o, const char *name);
int ModVar_SetObjAttr(PyObject *o, const char *name, PyObject *v);

/* ---- by namespace dict (batch 5c): the globals and locals of the .mod evaluator, of Script_NameChain (the .def
 * runner's code strings, the save code strings, the names of Script_Literal) and of the C hooks of module code; a pack
 * module's dict through the registry, another dict (a class body, __main__, the $names of a .def file) as it is ---- */

/* the name of the dict (LOAD_NAME's look into one dict): borrowed, NULL without an error when it has none */
PyObject *ModVar_DictGet(PyObject *dict, const char *name);
/* name = v in the dict (STORE_NAME, v not stolen): 0, -1 with the error set */
int ModVar_DictSet(PyObject *dict, const char *name, PyObject *v);
/* del name in the dict (DELETE_NAME): 0, -1 with KeyError set when it has no value (the caller raises NameError) */
int ModVar_DictDel(PyObject *dict, const char *name);

#endif
