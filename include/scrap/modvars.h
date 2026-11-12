/*
 * The registry of the module variables of the pack modules (stage 5 of docs/objlib-plan.md). Not in the original.
 *
 * Per module file one static table of the names its module code gives (src/game/Script/ModVarTable.c, made by
 * tools/script/modvars.py from the .mod files and the C state of the ports): name, kind, the types of the values the
 * .mod lines give, flags and the C state of the port that keeps the variable. A name the table does not have (a mod's
 * own let line, a name a C hook, a load time call, a port or a test puts into the module) goes to the overflow list of
 * the module the first time the registry sees it, so mods keep working.
 *
 * The values (batch 9b1, 7e4a): a module is a native object (NObj_NewModule) whose variables are a dict object of
 * native values: shared lists, dicts, instances, functions and modules as their NV_REF, numbers, texts, None and tuples
 * by value, in the slot order of the dict of 1.5.2, so keys(), items(), a from-import walk and the writes that resize
 * it go as they went. Those variables are the key of the module here (ModVar_OfVal leads from a module value to its
 * table and overflow list, ModVar_RefOfNs from a namespace: a module or a dict object). The test channel
 * (src/game/Global/TestChannel.c, TestExpr.c: names, __globals__, X.__dict__, dump), the helpers of the ports by
 * module name (Script_Global, Script_SetGlobal, Script_Attr, Script_SetVar, the Mode_ helpers of scrap/scriptmodes.h
 * and the LvN_ wrappers on them), the module code of the .mod files (its stores, loads and deletes, from the start of
 * the module code on: ModVar_Making, and its from-import), the C hooks of module code, the writes of the loader
 * (Script_ReplaySet), the .def runner, Script_NameChain and the getattr and setattr of modules read and write module
 * variables only through here. The variables of __main__ are here too (ModVar_Keep). OBJLIB 7e4b: a reader that needs
 * a Python object gets the one of the value (Native_ToPy, kept with the value), and the face of a module that crossed
 * to Python reaches its variables through the getattr and setattr of the module type (ModVar_FaceHook).
 *
 * The variables with a C place (the place text of the table: the port that reads and writes them) have no C copy: the
 * port reads the value here where the script reads it and writes it here where the script writes it, so the value
 * here is the only one (other holders change the shared lists and dicts in place, and a case can put a value of any
 * type there: the port's TypeError comes where it reads it).
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

/* a module of the registry as it is loaded now, or any module or namespace dict: valid while the module lives, keep it
 * only for the call (or, made by ModVar_Making, while its module code runs) */
typedef struct modVarRef_s {
	int file;			/* the index of its table in g_ModVarModules (-1: a module the registry has no table
					 * of, or a namespace dict; ModVar_Find gives NULL for it) */
	const nativeVal_t *vars;	/* its variables: the module's dict object (NObj_ModuleVars), or the namespace dict */
} modVarRef_t;

/* Script_ModuleMake: the module (NObj_NewModule) was made from the .mod file of level (NULL: of every level); the names
 * of its variables the table does not have go to the overflow list */
void ModVar_Made(const char *module, const char *level, const nativeVal_t *mod);
/* Script_ModuleMake before the module code: the same, so the module code's own stores (the let lines of the .mod text,
 * built in or of a mod pack) and loads go through the registry; *ref the module as the evaluator keeps it, *was the
 * binding it replaced, for ModVar_Unmade when the module code fails */
void ModVar_Making(const char *module, const char *level, const nativeVal_t *mod, modVarRef_t *ref, modVarRef_t *was);
/* the module code failed: the binding of the module's files as before ModVar_Making */
void ModVar_Unmade(const char *module, const modVarRef_t *was);
/* the module __main__ (no .mod file): kept here from now on, its variables those of a module without a table */
void ModVar_Keep(const nativeVal_t *mod);
/* at the end, before the module store goes: the variables of every module set to None as the module cleanup of
 * Py_Finalize cleared a module dict, then the modules let go */
void ModVar_Close(void);
/* the ref of a module value (any module: the pack module of a .mod file has its table, file >= 0): 1 with *ref, 0
 * (no error set) for a value that is no module */
byte ModVar_OfVal(const nativeVal_t *v, modVarRef_t *ref);
/* the same for a module name (only when it is in the store) */
byte ModVar_Named(const char *module, modVarRef_t *ref);
/* the ref of a namespace (the globals of an evaluator): a module (ModVar_OfVal), or a dict object (file -1); 0 for any
 * other value */
byte ModVar_RefOfNs(const nativeVal_t *ns, modVarRef_t *ref);
/* the entry of a name: of the table, or of the overflow list (the name of the dict taken in); NULL when the module
 * has no such name now */
const modVar_t *ModVar_Find(const modVarRef_t *ref, const char *name);
/* the module has a value of name now */
byte ModVar_Has(const modVarRef_t *ref, const char *name);
/* the value of a variable: 1 with *out a copy (Native_Free it), 0 without an error and *out None when it has none */
byte ModVar_GetVal(const modVarRef_t *ref, const char *name, nativeVal_t *out);
/* a variable set to v (copied; a list or dict by value becomes an object, NObj_Share): 0, -1 with the error set */
int ModVar_SetVal(const modVarRef_t *ref, const char *name, const nativeVal_t *v);
/* the next variable in the slot order of the dict (*pos 0 at the start, as PyDict_Next): 1 with copies in *key and
 * *value (Native_Free both), 0 at the end */
byte ModVar_Next(const modVarRef_t *ref, int *pos, nativeVal_t *key, nativeVal_t *value);
/* the variables as a dict object (module.__dict__ and the globals of the channel's lines): the registry's, or the view
 * of a dict without variables here (Native_Free it) */
void ModVar_Space(const modVarRef_t *ref, nativeVal_t *out);
/* OBJLIB 9: the value of a variable as a Python object (borrowed, kept by the value; native data crosses here), NULL
 * without an error when it has none */
PyObject *ModVar_Get(const modVarRef_t *ref, const char *name);
/* OBJLIB 9: a variable set to the Python object v (as the dict's SetItemString): 0, -1 with the error set */
int ModVar_Set(const modVarRef_t *ref, const char *name, PyObject *v);
/* a variable deleted (as the dict's DelItemString): 0, -1 with KeyError set when it has no value */
int ModVar_Del(const modVarRef_t *ref, const char *name);
/* OBJLIB 7e4b: the module's attribute name as getattr gives it (module_getattr of 1.5.2: __dict__ the dict object of
 * the variables, which crosses, else the variable or AttributeError(name)): a new reference, NULL with the error set */
PyObject *ModVar_Attr(const modVarRef_t *ref, const char *name);
/* OBJLIB 7e4b: the module's attribute set (v NULL: deleted) as setattr and delattr do it (module_setattr: TypeError for
 * __dict__, AttributeError "delete non-existing module attribute"): 0, -1 with the error set */
int ModVar_SetAttr(const modVarRef_t *ref, const char *name, PyObject *v);
/* the same on native values: 1 with *out (__dict__: ModVar_Space), 0 with the error */
byte ModVar_AttrVal(const modVarRef_t *ref, const char *name, nativeVal_t *out);
byte ModVar_SetAttrVal(const modVarRef_t *ref, const char *name, const nativeVal_t *v);
/* the names that have a value now, sorted (strcmp): the table's and the overflow list's, the new names taken in first
 * (a ref without a table: the string keys). *names is Mem_Alloc'd (Mem_Free it, NULL when there are none), the texts
 * valid while the module keeps the names; the count */
int ModVar_Names(const modVarRef_t *ref, const char ***names);
/* the name of the module of a ref */
const char *ModVar_ModuleName(const modVarRef_t *ref);
/* whether the registry has a .mod file (of any level) of the module name: a pack module (Script_Module) */
byte ModVar_HasModule(const char *module);

/* what the variables of every module hold now, counted by kind (native data, views, crossed), as lines added to b:
 * the measure of stage 9b (the channel's "native vars"; SCRAP_NATIVE_COUNT=1 prints it at the end) */
void ModVar_Report(nativeBuf_t *b);

/* ---- by module name: the module imported as the scripts import it (Script_Module, its error), a pack module through
 * the registry, another module (Scrap, a pack module while its code runs) by its dict ---- */

/* the variable name of module as its code reads a global: a new reference, NULL when it has none (no error set, the
 * caller raises NameError or ImportError) or when the import failed (*imported 0, its error set) */
PyObject *ModVar_Lookup(const char *module, const char *name, byte *imported);
/* the same on native values: 1 with *out, 0 with the error of the import or NameError(name) */
byte ModVar_GlobalVal(const char *module, const char *name, nativeVal_t *out);
/* the global name of module set to v (not stolen): 0, -1 with the error set */
int ModVar_SetGlobal(const char *module, const char *name, PyObject *v);
/* the same for a native value: 1, 0 with the error */
byte ModVar_SetGlobalVal(const char *module, const char *name, const nativeVal_t *v);
/* the global name of module deleted: 0, -1 with the error of the import or NameError(name) */
int ModVar_DelGlobal(const char *module, const char *name);
/* getattr(module, name) of the imported module: a new reference, NULL with the error set */
PyObject *ModVar_ModAttr(const char *module, const char *name);
/* setattr(module, name, v) of the imported module (v not stolen, NULL: delattr): 0, -1 with the error set */
int ModVar_SetModAttr(const char *module, const char *name, PyObject *v);
/* the same on native values (v NULL: delattr): 1 (with *out), 0 with the error */
byte ModVar_ModAttrVal(const char *module, const char *name, nativeVal_t *out);
byte ModVar_SetModAttrVal(const char *module, const char *name, const nativeVal_t *v);
/* OBJLIB 7e4b: getattr and setattr of any Python object (v NULL: delattr), the face of a module through the registry */
PyObject *ModVar_ObjAttr(PyObject *o, const char *name);
int ModVar_SetObjAttr(PyObject *o, const char *name, PyObject *v);
/* OBJLIB 7e4b: the getattr and setattr of the module type go to the native module of a face (NObj_ToPy of a module) */
void ModVar_FaceHook(void);

#endif
