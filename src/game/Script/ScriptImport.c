/*
 * The module store and the import (stage 7e, batch 7e4a: Python's runtime replaced; include/scrap/script.h). Not in
 * the original.
 *
 * The store is sys.modules: a dict object of native values, name -> module (a case may put None or another object
 * there). A module is a native object (NObj_NewModule) with its variables in a dict object; a pack module's are the
 * registry's (scrap/modvars.h). Script_ImportStart makes the modules Py_Initialize made, in its order: __builtin__,
 * sys (whose modules is the store and path the path of the library), __main__ (__builtins__ the module __builtin__,
 * its variables the registry's, ModVar_Keep) and signal. The import of a name is that of 1.5.2 (import_module_ex,
 * load_next, import_submodule) without packages and files: the store's entry, else a built-in module made into the
 * store (the engine modules from their function tables, the modules of the library's table), else the ImportError
 * "No module named <name>" (there is no file to find: the pack modules are made by Pack_PreloadLibrary only).
 *
 * OBJLIB 7e4b: the built-in modules other than the engine's are made by their init functions in the library
 * (Py_LibModule, src/python/import.c, behind a thread state of the library's own, no interpreter): their variables
 * are taken over (the slot order of their dicts kept), the C functions there are views of the library's functions,
 * the numbers and texts keep their Python objects (the exception names of __builtin__, which the error state maps by
 * object). The functions of those that need an interpreter (a frame, the import, sys of the interpreter) are native
 * here: __builtin__'s __import__, globals, locals, vars, dir, reload and raw_input, sys's exc_info, settrace and
 * setprofile, imp's import functions.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/script.h"
#include "scrap/scriptapi.h"

/* sys.modules, and the modules __builtin__, sys (the one the start made: the interpreter's sysdict) and __main__ */
static nativeVal_t imp_store, imp_builtin, imp_sys, imp_main;
static unsigned long imp_counts[SMC_COUNT];

/* ---- the store ---- */

const nativeVal_t *Script_Modules(void) {
	return &imp_store;
}

byte Script_ModuleLoaded(const char *name, nativeVal_t *out) {
	nativeVal_t key;
	Native_SetNone(out);
	if (imp_store.type != NV_REF) {
		return 0;
	}
	Native_SetStr(&key, name);
	nativeVal_t *t = NObj_DictData(&imp_store);
	byte ok;
	if (t) {
		const nativeVal_t *x = Native_DictGet(t, &key);
		if (x) {
			Native_Copy(out, x);
		}
		ok = x != NULL;
	} else {
		/* OBJLIB 7e4b: the store crossed to Python */
		ok = NObj_GetItem(&imp_store, &key, out);
		if (!ok) {
			Native_ErrClear();
		}
	}
	Native_Free(&key);
	return ok;
}

/* store[name] = m */
static byte Imp_Store(const char *name, const nativeVal_t *m) {
	nativeVal_t key;
	Native_SetStr(&key, name);
	byte ok = NObj_SetItem(&imp_store, &key, m);
	Native_Free(&key);
	return ok;
}

/* ---- the built-in modules ---- */

/* a value of the library as the module keeps it (a number or text with its object: the same object goes back) */
static byte Imp_Conv(PyObject *o, nativeVal_t *v, int depth) {
	(void)depth;
	NObj_FromPyKept(o, v);
	return 1;
}

/* OBJLIB 7e4b: the module name of the library's table as its init function makes it, natively */
static byte Imp_Lib(const char *name, nativeVal_t *out) {
	PyObject *m = Py_LibModule(name);
	Native_SetNone(out);
	if (!m) {
		return 0;
	}
	NObj_NewModule(out, name);
	nativeVal_t *t = NObj_DictData(NObj_ModuleVars(out));
	Native_Free(t);
	Native_DictOfPy(PyModule_GetDict(m), t, Imp_Conv, 0);
	Py_DECREF(m);
	return 1;
}

/* the value of name in module m replaced by the native C function fn, in its slot */
static void Imp_Native(const nativeVal_t *m, const char *name, nativeCall_t fn) {
	nativeVal_t key, f;
	Native_SetStr(&key, name);
	nativeVal_t *slot = Native_DictGet(NObj_DictData(NObj_ModuleVars(m)), &key);
	CHECK(slot);
	NObj_NewCallback(&f, name, fn, NULL, NULL, 0);
	Native_Free(slot);
	*slot = f;
	Native_Free(&key);
}

void Api_Module(const char *name, const portFn_t *fns, nativeVal_t *out) {
	NObj_NewModule(out, name);
	nativeVal_t *t = NObj_DictData(NObj_ModuleVars(out));
	for (; fns->name; fns++) {
		const portRef_t *ref = Port_OfFn(name, fns);
		nativeVal_t key, f;
		CHECK(ref);
		Native_SetStr(&key, fns->name);
		NObj_NewPortFunc(&f, ref);
		Native_DictSet(t, &key, &f);
	}
}

static byte Imp_MakeImp(const char *name, nativeVal_t *out);

#define IMP_ENGINE(n) \
	static byte Imp_##n(const char *name, nativeVal_t *out) { \
		void Api_Make##n(nativeVal_t *out); \
		(void)name; \
		Api_Make##n(out); \
		return 1; \
	}
IMP_ENGINE(Scrap)
IMP_ENGINE(SInput)
IMP_ENGINE(SNet)
IMP_ENGINE(SWeap)
IMP_ENGINE(SScorer)
IMP_ENGINE(SVec)
IMP_ENGINE(SSound)
IMP_ENGINE(SAI)
IMP_ENGINE(SAct)
IMP_ENGINE(SLogic)
IMP_ENGINE(SFX)

/* the table of the built-in modules (src/python/config.c in its order): the maker, NULL for the modules made at the
 * start that cannot be made again */
static const struct {
	const char *name;
	byte (*make)(const char *name, nativeVal_t *out);
} imp_table[] = {
	{ "Scrap", Imp_Scrap },
	{ "SInput", Imp_SInput },
	{ "SNet", Imp_SNet },
	{ "SWeap", Imp_SWeap },
	{ "SScorer", Imp_SScorer },
	{ "SVec", Imp_SVec },
	{ "SSound", Imp_SSound },
	{ "SAI", Imp_SAI },
	{ "SAct", Imp_SAct },
	{ "SLogic", Imp_SLogic },
	{ "SFX", Imp_SFX },
	{ "errno", Imp_Lib },
	{ "math", Imp_Lib },
	{ "new", Imp_Lib },
	{ "regex", Imp_Lib },
	{ "signal", Imp_Lib },
	{ "strop", Imp_Lib },
	{ "_locale", Imp_Lib },
	{ "marshal", Imp_Lib },
	{ "imp", Imp_MakeImp },
	{ "__main__", NULL },
	{ "__builtin__", NULL },
	{ "sys", NULL },
};

#define IMP_TABLE ((int)(sizeof(imp_table) / sizeof(imp_table[0])))

/* the index of name in the table, -1 when it is no built-in module */
static int Imp_Builtin(const char *name) {
	for (int i = 0; i < IMP_TABLE; i++) {
		if (!strcmp(imp_table[i].name, name)) {
			return i;
		}
	}
	return -1;
}

/* the copies of the variables of the built-in modules as they were made (_PyImport_FixupExtension), by table index */
static nativeVal_t imp_copies[sizeof(imp_table) / sizeof(imp_table[0])];

/* FixupExtension: the variables of the built-in module m (table index i) kept as made */
static void Imp_Fixup(int i, const nativeVal_t *m) {
	Native_Free(&imp_copies[i]);
	Native_Copy(&imp_copies[i], NObj_DictData(NObj_ModuleVars(m)));
}

/* init_builtin: the built-in module name made into the store, or a module made before given its variables as made
 * again (_PyImport_FindExtension: the module of the store, or a new one, updated with the copy); 1 with *out, 0 with
 * the error ("Cannot re-init internal module"), -1 without one when name is no built-in module */
static int Imp_InitBuiltin(const char *name, nativeVal_t *out) {
	int i = Imp_Builtin(name);
	Native_SetNone(out);
	if (i < 0) {
		return -1;
	}
	if (imp_copies[i].type == NV_DICT) {
		if (!Script_ModuleLoaded(name, out) || NObj_Kind(out) != NO_MODULE) {
			Native_Free(out);
			NObj_NewModule(out, name);
			if (!Imp_Store(name, out)) {
				Native_Free(out);
				return 0;
			}
		}
		nativeVal_t *t = NObj_DictData(NObj_ModuleVars(out));
		if (t) {
			Native_DictMerge(t, &imp_copies[i], 0);
		}
		return 1;
	}
	if (!imp_table[i].make) {
		return Native_Raise(NE_IMPORT, "Cannot re-init internal module %.200s", name);
	}
	if (!imp_table[i].make(name, out) || !Imp_Store(name, out)) {
		Native_Free(out);
		return 0;
	}
	Imp_Fixup(i, out);
	return 1;
}

/* ---- the import ---- */

/* import_submodule of one name at the top (fullname: no parent) or under the module parent: 1 with *out (None when
 * there is no such module), 0 with the error */
static byte Imp_Sub(const nativeVal_t *parent, const char *fullname, nativeVal_t *out) {
	if (Script_ModuleLoaded(fullname, out)) {
		return 1;
	}
	if (parent) {
		/* a module without __path__ is no package: nothing under it */
		nativeVal_t path;
		byte has = NObj_GetAttr(parent, "__path__", &path);
		Native_Free(&path);
		Native_ErrClear();
		if (!has) {
			return 1;
		}
		/* a package path to search: there are no files */
		return 1;
	}
	int r = Imp_InitBuiltin(fullname, out);
	return r != 0;
}

/* import name (dotted: the head, or with tail the last module), as import_module_ex without a parent package */
static byte Imp_Dotted(const char *name, byte tail, nativeVal_t *out) {
	char full[0x200];
	nativeVal_t head, cur, next;
	const char *p = name;
	int len = 0;
	Native_SetNone(out);
	if (!*name) {
		return Native_Raise(NE_VALUE, "Empty module name");
	}
	Native_SetNone(&head);
	Native_SetNone(&cur);
	while (p) {
		const char *dot = strchr(p, '.');
		int n = dot ? (int)(dot - p) : (int)strlen(p);
		if (n == 0) {
			Native_Free(&head);
			Native_Free(&cur);
			return Native_Raise(NE_VALUE, "Empty module name");
		}
		if (len + n + 2 >= (int)sizeof(full)) {
			Native_Free(&head);
			Native_Free(&cur);
			return Native_Raise(NE_VALUE, "Module name too long");
		}
		if (len) {
			full[len++] = '.';
		}
		memcpy(full + len, p, (size_t)n);
		len += n;
		full[len] = '\0';
		if (!Imp_Sub(head.type == NV_NONE ? NULL : &cur, full, &next)) {
			Native_Free(&head);
			Native_Free(&cur);
			return 0;
		}
		if (next.type == NV_NONE) {
			Native_Free(&head);
			Native_Free(&cur);
			return Native_Raise(NE_IMPORT, "No module named %.200s", p);
		}
		if (head.type == NV_NONE) {
			Native_Copy(&head, &next);
		}
		Native_Free(&cur);
		cur = next;
		p = dot ? dot + 1 : NULL;
	}
	if (tail) {
		*out = cur;
		Native_Free(&head);
	} else {
		*out = head;
		Native_Free(&cur);
	}
	return 1;
}

byte Script_Module(const char *name, nativeVal_t *out) {
	byte pack = ModVar_HasModule(name);
	if (!pack || !Script_ModuleLoaded(name, out)) {
		imp_counts[pack ? SMC_ABSENT : SMC_OTHER]++;
		return Imp_Dotted(name, 0, out);
	}
	if (out->type == NV_NONE) {
		/* what load_next of the import raises for an entry None */
		imp_counts[SMC_NONE]++;
		return Native_Raise(NE_IMPORT, "No module named %.200s", name);
	}
	imp_counts[SMC_PACK]++;
	return 1;
}

byte Script_ModuleIn(const char *name, const nativeVal_t *globals, nativeVal_t *out) {
	modVarRef_t ref;
	nativeVal_t n;
	byte package = 0;
	/* get_parent of the import: a package (__path__) or a dotted module name looks for name in the package first */
	if (globals && ModVar_RefOfNs(globals, &ref) && ModVar_GetVal(&ref, "__name__", &n)) {
		package = n.type == NV_STR && (strchr(Native_Text(&n), '.') || ModVar_Has(&ref, "__path__"));
		Native_Free(&n);
	}
	if (package) {
		imp_counts[SMC_OTHER]++;
		return Imp_Dotted(name, 0, out);
	}
	return Script_Module(name, out);
}

byte Script_ModuleAdd(const char *name, nativeVal_t *out) {
	byte pack = ModVar_HasModule(name), made = !Script_ModuleLoaded(name, out) || NObj_Kind(out) != NO_MODULE;
	imp_counts[!pack ? SMC_OTHER : made ? SMC_ADDED : SMC_PACK]++;
	if (made) {
		Native_Free(out);
		NObj_NewModule(out, name);
		if (!Imp_Store(name, out)) {
			Native_Free(out);
			return 0;
		}
	}
	return 1;
}

void Script_ModuleCount(int kind) {
	imp_counts[kind]++;
}

void Script_ModuleCounts(unsigned long counts[SMC_COUNT], byte reset) {
	for (int i = 0; i < SMC_COUNT; i++) {
		counts[i] = imp_counts[i];
		if (reset) {
			imp_counts[i] = 0;
		}
	}
}

/* ---- the natives of __builtin__, sys and imp ---- */

/* __import__(name[, globals[, locals[, fromlist]]]) */
static byte Bi_Import(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)ctx;
	if (!Native_Arity("__import__", n, 1, 4) || !Native_ArgTextOf("__import__", 1, &a[0], 1)) {
		return 0;
	}
	byte tail = n > 3 && (a[3].type == NV_REF ? NObj_True(&a[3]) : Native_True(&a[3]));
	return Script_ModuleIn(Native_Text(&a[0]), n > 1 ? &a[1] : NULL, out) && (!tail || (Native_Free(out),
		Imp_Dotted(Native_Text(&a[0]), 1, out)));
}

/* there is no frame: the locals vars() asks for (PyEval_GetLocals) are none */
static byte Bi_NoLocals(void) {
	return Native_Raise(NE_SYSTEM, "no locals!?");
}

static byte Bi_Globals(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)ctx;
	(void)a;
	(void)out;
	return Native_Arity(NULL, n, 0, 0) && Bi_NoLocals();
}

/* vars([object]) */
static byte Bi_Vars(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)ctx;
	if (!Native_Arity("vars", n, 0, 1)) {
		return 0;
	}
	if (!n) {
		return Bi_NoLocals();
	}
	if (!NObj_GetAttr(&a[0], "__dict__", out)) {
		Native_ErrClear();
		return Native_Raise(NE_TYPE, "vars() argument must have __dict__ attribute");
	}
	return 1;
}

/* dir([object]): the keys of its __dict__ and the names of its __members__ and __methods__, sorted (none of a part that
 * fails); without an object the locals, which are none: NULL without an error */
static byte Bi_Dir(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	static const char *const lists[] = { "__members__", "__methods__" };
	nativeVal_t d, m, r;
	(void)ctx;
	Native_SetNone(out);
	if (!Native_Arity("dir", n, 0, 1)) {
		return 0;
	}
	if (!n) {
		return 0;
	}
	if (NObj_GetAttr(&a[0], "__dict__", &d)) {
		if (!NObj_CallMethod(&d, "keys", NULL, 0, out)) {
			Native_ErrClear();
		}
		Native_Free(&d);
	} else {
		Native_ErrClear();
	}
	if (out->type == NV_NONE) {
		Native_SetEmpty(out, NV_LIST);
	}
	NObj_Share(out);
	for (int k = 0; k < 2; k++) {
		if (!NObj_GetAttr(&a[0], lists[k], &m)) {
			Native_ErrClear();
			continue;
		}
		for (int i = 0;; i++) {
			nativeVal_t key, x;
			Native_SetInt(&key, i);
			if (!NObj_GetItem(&m, &key, &x)) {
				Native_ErrClear();
				break;
			}
			byte ok = NObj_CallMethod(out, "append", &x, 1, &r);
			Native_Free(&r);
			Native_Free(&x);
			if (!ok) {
				Native_Free(&m);
				Native_Free(out);
				return 0;
			}
		}
		Native_Free(&m);
	}
	byte ok = NObj_CallMethod(out, "sort", NULL, 0, &r);
	Native_Free(&r);
	if (!ok) {
		Native_Free(out);
	}
	return ok;
}

/* reload(module): a built-in module made again, else the module has no file to read */
static byte Bi_Reload(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t name, cur;
	modVarRef_t ref;
	(void)ctx;
	Native_SetNone(out);
	if (!Native_Arity("reload", n, 1, 1)) {
		return 0;
	}
	if (!ModVar_OfVal(&a[0], &ref)) {
		return Native_Raise(NE_TYPE, "reload() argument must be module");
	}
	if (!ModVar_GetVal(&ref, "__name__", &name) || name.type != NV_STR) {
		Native_Free(&name);
		return Native_Raise(NE_SYSTEM, "nameless module");
	}
	byte ok = Script_ModuleLoaded(Native_Text(&name), &cur) && NObj_Is(&cur, &a[0]);
	Native_Free(&cur);
	if (!ok) {
		Native_Raise(NE_IMPORT, "reload(): module %.200s not in sys.modules", Native_Text(&name));
	} else {
		int r = Imp_InitBuiltin(Native_Text(&name), out);
		ok = r > 0;
		if (r < 0) {
			Native_Raise(NE_IMPORT, "No module named %.200s", Native_Text(&name));
		}
	}
	Native_Free(&name);
	return ok;
}

/* raw_input([prompt]): the prompt to sys.stdout, then the line of stdin, which has none */
static byte Bi_RawInput(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)ctx;
	(void)out;
	if (!Native_Arity("raw_input", n, 0, 1)) {
		return 0;
	}
	if (n && !Script_WriteOut(&a[0], 1)) {
		return 0;
	}
	return Native_Raise(NE_EOF, "EOF when reading a line");
}

/* sys.exc_info(): no exception is being handled (there is no except clause) */
static byte Sys_ExcInfo(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)ctx;
	(void)a;
	if (n) {
		return Native_Raise(NE_TYPE, "function requires no arguments");
	}
	Native_SetEmpty(out, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		Native_Append(out);
	}
	return 1;
}

/* sys.settrace(f), sys.setprofile(f): no Python code runs to trace */
static byte Sys_SetTrace(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)ctx;
	(void)a;
	(void)n;
	Native_SetNone(out);
	return 1;
}

/* sys.setcheckinterval(n): there is no bytecode loop to check */
static byte Sys_SetCheckInterval(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	int k;
	(void)ctx;
	Native_SetNone(out);
	return Native_Arity(NULL, n, 1, 1) && Native_ArgInt(&a[0], &k);
}

/* imp.new_module(name) */
static byte Imp_NewModule(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	(void)ctx;
	if (!Native_Arity(NULL, n, 1, 1) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	NObj_NewModule(out, name);
	return 1;
}

/* imp.is_builtin(name): 1, -1 for one that cannot be made again, 0 */
static byte Imp_IsBuiltin(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	(void)ctx;
	if (!Native_Arity(NULL, n, 1, 1) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	int i = Imp_Builtin(name);
	Native_SetInt(out, i < 0 ? 0 : imp_table[i].make ? 1 : -1);
	return 1;
}

/* imp.init_builtin(name): the module, None for no built-in one */
static byte Imp_InitBuiltinFn(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	(void)ctx;
	if (!Native_Arity(NULL, n, 1, 1) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	return Imp_InitBuiltin(name, out) != 0;
}

/* imp.find_module(name[, path]): a built-in module, else no file */
static byte Imp_FindModule(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	(void)ctx;
	if (!Native_Arity(NULL, n, 1, 2) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	if ((n < 2 || a[1].type == NV_NONE) && Imp_Builtin(name) >= 0) {
		nativeVal_t *desc;
		Native_SetEmpty(out, NV_TUPLE);
		Native_Append(out);
		Native_SetStr(Native_Append(out), name);
		Native_SetEmpty(desc = Native_Append(out), NV_TUPLE);
		Native_SetStr(Native_Append(desc), "");
		Native_SetStr(Native_Append(desc), "");
		Native_SetInt(Native_Append(desc), 6);
		return 1;
	}
	return Native_Raise(NE_IMPORT, "No module named %.200s", name);
}

/* imp.load_compiled, load_dynamic, load_package, load_source, load_module: there is no file to load */
static byte Imp_Load(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	(void)ctx;
	(void)out;
	if (!Native_Arity(NULL, n, 2, 4) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	return Native_Raise(NE_IMPORT, "No module named %.200s", name);
}

static byte Imp_MakeImp(const char *name, nativeVal_t *out) {
	static const char *const loads[] = { "load_compiled", "load_dynamic", "load_module", "load_package",
		"load_source" };
	if (!Imp_Lib(name, out)) {
		return 0;
	}
	Imp_Native(out, "new_module", Imp_NewModule);
	Imp_Native(out, "is_builtin", Imp_IsBuiltin);
	Imp_Native(out, "init_builtin", Imp_InitBuiltinFn);
	Imp_Native(out, "find_module", Imp_FindModule);
	for (size_t i = 0; i < sizeof(loads) / sizeof(loads[0]); i++) {
		Imp_Native(out, loads[i], Imp_Load);
	}
	return 1;
}

/* ---- the start and the end ---- */

const nativeVal_t *Script_Main(void) {
	return &imp_main;
}

const nativeVal_t *Script_Builtins(void) {
	return NObj_ModuleVars(&imp_builtin);
}

/* name = v into the variables of module m (a new key: the dict's insert) */
static void Imp_Put(const nativeVal_t *m, const char *name, const nativeVal_t *v) {
	nativeVal_t key, x;
	Native_SetStr(&key, name);
	Native_Copy(&x, v);
	Native_DictSet(NObj_DictData(NObj_ModuleVars(m)), &key, &x);
}

byte Script_ImportStart(void) {
	static const char *const bi[] = { "__import__", "globals", "locals", "vars", "dir", "reload", "raw_input" };
	static const nativeCall_t biFn[] = { Bi_Import, Bi_Globals, Bi_Globals, Bi_Vars, Bi_Dir, Bi_Reload, Bi_RawInput };
	nativeVal_t sys, path, signal;
	NObj_NewDict(&imp_store);
	if (!Imp_Lib("__builtin__", &imp_builtin)) {
		return 0;
	}
	for (size_t i = 0; i < sizeof(bi) / sizeof(bi[0]); i++) {
		Imp_Native(&imp_builtin, bi[i], biFn[i]);
	}
	Imp_Store("__builtin__", &imp_builtin);
	Imp_Fixup(Imp_Builtin("__builtin__"), &imp_builtin);
	if (!Imp_Lib("sys", &sys)) {
		return 0;
	}
	Imp_Native(&sys, "exc_info", Sys_ExcInfo);
	Imp_Native(&sys, "settrace", Sys_SetTrace);
	Imp_Native(&sys, "setprofile", Sys_SetTrace);
	Imp_Native(&sys, "setcheckinterval", Sys_SetCheckInterval);
	Imp_Store("sys", &sys);
	Imp_Fixup(Imp_Builtin("sys"), &sys);
	Imp_Put(&sys, "modules", &imp_store);
	/* OBJLIB 7e4b: the path of the library (getpathp.c) */
	PyObject *p = Py_LibPath();
	if (!p || !Native_FromPyAny(p, &path)) {
		Py_XDECREF(p);
		Native_Free(&sys);
		return 0;
	}
	Py_DECREF(p);
	NObj_Share(&path);
	Imp_Put(&sys, "path", &path);
	Native_Free(&path);
	imp_sys = sys;
	/* initmain */
	NObj_NewModule(&imp_main, "__main__");
	Imp_Put(&imp_main, "__builtins__", &imp_builtin);
	Imp_Store("__main__", &imp_main);
	ModVar_Keep(&imp_main);
	/* initsigs */
	if (!Imp_Lib("signal", &signal)) {
		return 0;
	}
	Imp_Store("signal", &signal);
	Imp_Fixup(Imp_Builtin("signal"), &signal);
	Native_Free(&signal);
	return 1;
}

void Script_ImportEnd(void) {
	nativeVal_t r;
	/* PyImport_Cleanup: the store emptied (sys holds it) */
	if (NObj_CallMethod(&imp_store, "clear", NULL, 0, &r)) {
		Native_Free(&r);
	}
	Native_ErrClear();
	Native_Free(&imp_main);
	Native_Free(&imp_builtin);
	Native_Free(&imp_sys);
	Native_Free(&imp_store);
	for (int i = 0; i < IMP_TABLE; i++) {
		Native_Free(&imp_copies[i]);
	}
}

byte Script_SysVar(const char *name, nativeVal_t *out) {
	modVarRef_t ref;
	Native_SetNone(out);
	return ModVar_OfVal(&imp_sys, &ref) && ModVar_GetVal(&ref, name, out);
}

void Script_SetSysVar(const char *name, const nativeVal_t *v) {
	modVarRef_t ref;
	if (!ModVar_OfVal(&imp_sys, &ref)) {
		return;
	}
	if (v) {
		ModVar_SetVal(&ref, name, v);
	} else if (ModVar_Has(&ref, name)) {
		ModVar_Del(&ref, name);
	}
	Native_ErrClear();
}
