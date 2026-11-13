/*
 * The module store and the import (stage 7e, batch 7e4a: Python's runtime replaced; include/scrap/script.h). Not in
 * the original.
 *
 * The store is sys.modules: a dict object of native values, name -> module (a case may put None or another object
 * there). A module is a native object (NObj_NewModule) with its variables in a dict object; a pack module's are the
 * registry's (scrap/modvars.h). Script_ImportStart makes the modules pythonrun.c Initialize made, in its order:
 * __builtin__, sys (whose modules is the store and path the path of the library), __main__ (__builtins__ the module
 * __builtin__, its variables the registry's, ModVar_Keep) and signal. The import of a name is that of 1.5.2
 * (import_module_ex, load_next, import_submodule) without packages and files: the store's entry, else a built-in module
 * made into the store (the engine modules from their function tables, the modules of the library's table), else the
 * ImportError "No module named <name>" (there is no file to find: the pack modules are made by Pack_PreloadLibrary
 * only).
 *
 * The other built-in modules are native too (batch 7e4c): __builtin__, math, strop and signal (scrap/scriptlib.h),
 * sys and imp here (sys with the file objects of stdin, stdout and stderr and the path of PC/getpathp.c). The
 * functions that need an interpreter (a frame, the import, sys of the interpreter) are the natives of the import:
 * __builtin__'s __import__, globals, locals, vars, dir, reload and raw_input, sys's exc_info, settrace and setprofile,
 * imp's import functions.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/script.h"
#include "scrap/scriptapi.h"
#include "scrap/scriptlib.h"
#include "scrap/scriptmodes.h"

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
	const nativeVal_t *x = Native_DictGet(NObj_DictData(&imp_store), &key);
	if (x) {
		Native_Copy(out, x);
	}
	byte ok = x != NULL;
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

/* the modules of the library (scrap/scriptlib.h) */
#define IMP_LIB(n) \
	static byte Imp_##n(const char *name, nativeVal_t *out) { \
		(void)name; \
		Lib_Make##n(out); \
		return 1; \
	}
IMP_LIB(Math)
IMP_LIB(Signal)
IMP_LIB(Strop)

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

/* the table of the built-in modules (src/python/config.c in its order, without the modules of the library that had
 * no user: errno, new, regex, _locale, marshal): the maker, NULL for the modules made at the start that cannot be made
 * again */
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
	{ "math", Imp_Math },
	{ "signal", Imp_Signal },
	{ "strop", Imp_Strop },
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

/* the copies of the variables of the built-in modules as they were made (import.c Import_FixupExtension), by table
 * index */
static nativeVal_t imp_copies[sizeof(imp_table) / sizeof(imp_table[0])];

/* FixupExtension: the variables of the built-in module m (table index i) kept as made */
static void Imp_Fixup(int i, const nativeVal_t *m) {
	Native_Free(&imp_copies[i]);
	Native_Copy(&imp_copies[i], NObj_DictData(NObj_ModuleVars(m)));
}

/* init_builtin: the built-in module name made into the store, or a module made before given its variables as made
 * again (import.c Import_FindExtension: the module of the store, or a new one, updated with the copy); 1 with *out, 0
 * with the error ("Cannot re-init internal module"), -1 without one when name is no built-in module */
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

/* the docs of sys and imp (the texts of the vendored sources) */
static const char sys_doc[] =
	"This module provides access to some objects used or maintained by the\n"
	"interpreter and to functions that interact strongly with the interpreter.\n\nDynamic objects:\n\n"
	"argv -- command line arguments; argv[0] is the script pathname if known\n"
	"path -- module search path; path[0] is the script directory, else ''\n"
	"modules -- dictionary of loaded modules\n"
	"exitfunc -- you may set this to a function to be called when Python exits\n\n"
	"stdin -- standard input file object; used by raw_input() and input()\n"
	"stdout -- standard output file object; used by the print statement\n"
	"stderr -- standard error object; used for error messages\n"
	"  By assigning another file object (or an object that behaves like a file)\n"
	"  to one of these, it is possible to redirect all of the interpreter's I/O.\n\n"
	"last_type -- type of last uncaught exception\nlast_value -- value of last uncaught exception\n"
	"last_traceback -- traceback of last uncaught exception\n"
	"  These three are only available in an interactive session after a\n  traceback has been printed.\n\n"
	"exc_type -- type of exception currently being handled\n"
	"exc_value -- value of exception currently being handled\n"
	"exc_traceback -- traceback of exception currently being handled\n"
	"  The function exc_info() should be used instead of these three,\n  because it is thread-safe.\n\n"
	"Static objects:\n\nmaxint -- the largest supported integer (the smallest is -maxint-1)\n"
	"builtin_module_names -- tuple of module names built into this intepreter\n"
	"version -- the version of this interpreter\ncopyright -- copyright notice pertaining to this interpreter\n"
	"platform -- platform identifier\nexecutable -- pathname of this Python interpreter\n"
	"prefix -- prefix used to find the Python library\n"
	"exec_prefix -- prefix used to find the machine-specific Python library\n"
	"dllhandle -- [Windows only] integer handle of the Python DLL\n"
	"winver -- [Windows only] version number of the Python DLL\n__stdin__ -- the original stdin; don't use!\n"
	"__stdout__ -- the original stdout; don't use!\n__stderr__ -- the original stderr; don't use!\n\nFunctions:\n"
	"\nexc_info() -- return thread-safe information about the current exception\n"
	"exit() -- exit the interpreter by raising SystemExit\n"
	"getrefcount() -- return the reference count for an object (plus one :-)\n"
	"setcheckinterval() -- control how often the interpreter checks for events\n"
	"setprofile() -- set the global profiling function\nsettrace() -- set the global debug tracing function\n";
static const char sys_exit_doc[] =
	"exit([status])\n\nExit the interpreter by raising SystemExit(status).\n"
	"If the status is omitted or None, it defaults to zero (i.e., success).\n"
	"If the status numeric, it will be used as the system exit status.\n"
	"If it is another kind of object, it will be printed and the system\nexit status will be one (i.e., failure).";
static const char sys_getrefcount_doc[] =
	"getrefcount(object) -> integer\n\nReturn the current reference count for the object.  This includes the\n"
	"temporary reference in the argument list, so it is at least 2.";
static const char imp_doc[] =
	"This module provides the components needed to build your own\n"
	"__import__ function.  Undocumented functions are obsolete.\n";
static const char imp_get_magic_doc[] = "get_magic() -> string\nReturn the magic number for .pyc or .pyo files.";
static const char imp_get_suffixes_doc[] =
	"get_suffixes() -> [(suffix, mode, type), ...]\n"
	"Return a list of (suffix, mode, type) tuples describing the files\nthat find_module() looks for.";

/* ---- the natives of __builtin__, sys and imp ---- */

/* __import__(name[, globals[, locals[, fromlist]]]) */
byte Bi_Import(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("__import__", n, 1, 4) || !Native_ArgTextOf("__import__", 1, &a[0], 1)) {
		return 0;
	}
	byte tail = n > 3 && (a[3].type == NV_REF ? NObj_True(&a[3]) : Native_True(&a[3]));
	return Script_ModuleIn(Native_Text(&a[0]), n > 1 ? &a[1] : NULL, out) && (!tail || (Native_Free(out),
		Imp_Dotted(Native_Text(&a[0]), 1, out)));
}

/* there is no frame: the locals vars() asks for (ceval.c Eval_GetLocals) are none */
static byte Bi_NoLocals(void) {
	return Native_Raise(NE_SYSTEM, "no locals!?");
}

byte Bi_Globals(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Native_Arity(NULL, n, 0, 0) && Bi_NoLocals();
}

/* vars([object]) */
byte Bi_Vars(const nativeVal_t *a, int n, nativeVal_t *out) {
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
byte Bi_Dir(const nativeVal_t *a, int n, nativeVal_t *out) {
	static const char *const lists[] = { "__members__", "__methods__" };
	nativeVal_t d, m, r;
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
byte Bi_Reload(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t name, cur;
	modVarRef_t ref;
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
byte Bi_RawInput(const nativeVal_t *a, int n, nativeVal_t *out) {
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
static byte Sys_ExcInfo(const nativeVal_t *a, int n, nativeVal_t *out) {
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
static byte Sys_SetTrace(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	Native_SetNone(out);
	return 1;
}

/* sys.setcheckinterval(n): there is no bytecode loop to check */
static byte Sys_SetCheckInterval(const nativeVal_t *a, int n, nativeVal_t *out) {
	int k;
	Native_SetNone(out);
	return Native_Arity(NULL, n, 1, 1) && Native_ArgInt(&a[0], &k);
}

/* imp.new_module(name) */
static byte Imp_NewModule(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	if (!Native_Arity(NULL, n, 1, 1) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	NObj_NewModule(out, name);
	return 1;
}

/* imp.is_builtin(name): 1, -1 for one that cannot be made again, 0 */
static byte Imp_IsBuiltin(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	if (!Native_Arity(NULL, n, 1, 1) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	int i = Imp_Builtin(name);
	Native_SetInt(out, i < 0 ? 0 : imp_table[i].make ? 1 : -1);
	return 1;
}

/* imp.init_builtin(name): the module, None for no built-in one */
static byte Imp_InitBuiltinFn(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	if (!Native_Arity(NULL, n, 1, 1) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	return Imp_InitBuiltin(name, out) != 0;
}

/* imp.find_module(name[, path]): a built-in module, else no file */
static byte Imp_FindModule(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
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
static byte Imp_Load(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	(void)out;
	if (!Native_Arity(NULL, n, 2, 4) || !Native_ArgStr(&a[0], 1, &name)) {
		return 0;
	}
	return Native_Raise(NE_IMPORT, "No module named %.200s", name);
}

/* the frozen modules of Python/frozen.c: their names and sizes (a package negative) */
static const struct {
	const char *name;
	int size;
} imp_frozen[] = {
	{ "__hello__", 90 },
	{ "__phello__", -90 },
	{ "__phello__.spam", 90 },
};

/* the size of the frozen module name, 0 for none */
static int Imp_Frozen(const char *name) {
	for (size_t i = 0; i < sizeof(imp_frozen) / sizeof(imp_frozen[0]); i++) {
		if (!strcmp(imp_frozen[i].name, name)) {
			return imp_frozen[i].size;
		}
	}
	return 0;
}

/* imp.is_frozen(name) */
static byte Imp_IsFrozen(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	Native_SetInt(out, Imp_Frozen(name));
	return 1;
}

/* imp.get_frozen_object(name): the code object is unmarshalled for the interpreter there is not */
static byte Imp_GetFrozen(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	(void)out;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	if (!Imp_Frozen(name)) {
		return Native_Raise(NE_IMPORT, "No such frozen object named %.200s", name);
	}
	return Native_Raise(NE_IMPORT, "frozen module: no Python interpreter in this build");
}

/* imp.init_frozen(name): None for no frozen module, else the code would be run */
static byte Imp_InitFrozen(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	if (!Imp_Frozen(name)) {
		Native_SetNone(out);
		return 1;
	}
	return Native_Raise(NE_IMPORT, "frozen module: no Python interpreter in this build");
}

/* imp.get_magic() */
static byte Imp_GetMagic(const nativeVal_t *a, int n, nativeVal_t *out) {
	/* MAGIC of import.c: 20121 and "\r\n" */
	static const char magic[4] = { (char)0x99, 0x4e, '\r', '\n' };
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Native_SetText(out, magic, 4);
	return 1;
}

/* imp.get_suffixes(): import.c Import_Filetab (the extensions of the Windows build, none on Linux) */
static byte Imp_GetSuffixes(const nativeVal_t *a, int n, nativeVal_t *out) {
	static const struct {
		const char *suffix, *mode;
		int type;
	} tab[] = {
#ifdef _WIN32
		{ ".pyd", "rb", 3 },
		{ ".dll", "rb", 3 },
#endif
		{ ".py", "r", 1 },
		{ ".pyc", "rb", 2 },
	};
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Native_SetEmpty(out, NV_LIST);
	for (size_t i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
		nativeVal_t *item = Native_Append(out);
		Native_SetEmpty(item, NV_TUPLE);
		Native_SetStr(Native_Append(item), tab[i].suffix);
		Native_SetStr(Native_Append(item), tab[i].mode);
		Native_SetInt(Native_Append(item), tab[i].type);
	}
	return 1;
}

/* imp_methods of import.c (the functions native since 7e4a without their docs) */
static const libFn_t imp_fns[] = {
	{ "find_module", Imp_FindModule, NULL },
	{ "get_magic", Imp_GetMagic, imp_get_magic_doc },
	{ "get_suffixes", Imp_GetSuffixes, imp_get_suffixes_doc },
	{ "load_module", Imp_Load, NULL },
	{ "new_module", Imp_NewModule, NULL },
	{ "get_frozen_object", Imp_GetFrozen, NULL },
	{ "init_builtin", Imp_InitBuiltinFn, NULL },
	{ "init_frozen", Imp_InitFrozen, NULL },
	{ "is_builtin", Imp_IsBuiltin, NULL },
	{ "is_frozen", Imp_IsFrozen, NULL },
	{ "load_compiled", Imp_Load, NULL },
	{ "load_dynamic", Imp_Load, NULL },
	{ "load_package", Imp_Load, NULL },
	{ "load_source", Imp_Load, NULL },
	{ NULL, NULL, NULL },
};

static byte Imp_MakeImp(const char *name, nativeVal_t *out) {
	static const char *const consts[] = { "SEARCH_ERROR", "PY_SOURCE", "PY_COMPILED", "C_EXTENSION", "PY_RESOURCE",
		"PKG_DIRECTORY", "C_BUILTIN", "PY_FROZEN", "PY_CODERESOURCE" };
	nativeVal_t v;
	Lib_Module(out, name, imp_fns, imp_doc);
	for (size_t i = 0; i < sizeof(consts) / sizeof(consts[0]); i++) {
		Native_SetInt(&v, (long)i);
		Lib_Put(out, consts[i], &v);
	}
	return 1;
}

/* ---- sys ---- */

/* sys.exit([status]): SystemExit with the old argument form (None, the one argument, the tuple of several) */
static byte Sys_Exit(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t v;
	(void)out;
	if (n == 1) {
		return Native_RaiseVal(NE_SYSTEMEXIT, &a[0]);
	}
	Native_SetNone(&v);
	if (n > 1) {
		Native_SetEmpty(&v, NV_TUPLE);
		for (int i = 0; i < n; i++) {
			Native_Copy(Native_Append(&v), &a[i]);
		}
	}
	Native_RaiseVal(NE_SYSTEMEXIT, &v);
	Native_Free(&v);
	return 0;
}

/* sys.getrefcount(object) ("O" of the old form): the holders of an object and the argument; a value by value is a
 * temporary object held by the call and its argument */
static byte Sys_GetRefCount(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!n) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	Native_SetInt(out, n == 1 && a[0].type == NV_REF ? NObj_Refs(&a[0]) + 1 : 2);
	return 1;
}

static const libFn_t sys_fns[] = {
	{ "exc_info", Sys_ExcInfo, NULL },
	{ "exit", Sys_Exit, sys_exit_doc },
	{ "getrefcount", Sys_GetRefCount, sys_getrefcount_doc },
	{ "setcheckinterval", Sys_SetCheckInterval, NULL },
	{ "setprofile", Sys_SetTrace, NULL },
	{ "settrace", Sys_SetTrace, NULL },
	{ NULL, NULL, NULL },
};

#ifdef _WIN32
#define SYS_SEP '\\'
#define SYS_DELIM ';'

/* getpythonregpath of PC/getpathp.c: the PythonPath of the registry under keyBase (the version of a static build is
 * empty), NULL when there is none (a block of Mem_Alloc otherwise) */
static char *Sys_RegPath(HKEY keyBase) {
	HKEY key = 0;
	DWORD nameSize = 0, dataSize = 0, numEntries = 0;
	char *retval = NULL;
	if (RegOpenKeyA(keyBase, "Software\\Python\\PythonCore\\\\PythonPath", &key) == ERROR_SUCCESS) {
		RegQueryInfoKeyA(key, NULL, NULL, NULL, NULL, NULL, NULL, &numEntries, &nameSize, &dataSize, NULL, NULL);
	}
	if (numEntries) {
		char sub[MAX_PATH + 1];
		int off = 0;
		for (DWORD index = 0;; index++) {
			LONG reqd = 0;
			if (RegEnumKeyA(key, index, sub, MAX_PATH + 1) || RegQueryValueA(key, sub, NULL, &reqd)) {
				break;
			}
			dataSize += (DWORD)reqd + 1;
		}
		char *data = Mem_Alloc(dataSize + 1);
		data[0] = '\0';
		for (DWORD index = 0;; index++) {
			LONG reqd = (LONG)dataSize;
			if (RegEnumKeyA(key, index, sub, MAX_PATH + 1) || RegQueryValueA(key, sub, data + off, &reqd)) {
				break;
			}
			if (reqd > 1) {
				int adjust = (int)strlen(data + off);
				dataSize -= (DWORD)adjust;
				off += adjust;
				data[off++] = ';';
				data[off] = '\0';
				dataSize--;
			}
		}
		LONG size = (LONG)dataSize;
		if (RegQueryValueA(key, "", data + off, &size) == ERROR_SUCCESS && strlen(data)) {
			retval = data;
		} else {
			Mem_Free(data);
		}
	}
	if (key) {
		RegCloseKey(key);
	}
	return retval;
}
#else
#define SYS_SEP '/'
#define SYS_DELIM ':'
#endif

static const char sys_sep[2] = { SYS_SEP, 0 }, sys_delim[2] = { SYS_DELIM, 0 };

/* calculate_path of PC/getpathp.c: the module search path (the PYTHONPATH of the environment, the registry's on
 * Windows, the PYTHONPATH of the build with "." for the home, the folder of the program), the prefix (where
 * lib\string.py is found from there) and the program (the exe on Windows, else "python" on the PATH) */
static void Sys_Paths(nativeBuf_t *path, char *prefix, char *prog, int size) {
	static const char pythonpath[] = ".\\DLLs;.\\lib;.\\lib\\plat-win;.\\lib\\lib-tk";
	char dir[0x400];
	prefix[0] = prog[0] = '\0';
#ifdef _WIN32
	if (!GetModuleFileNameA(NULL, prog, (DWORD)size)) {
		prog[0] = '\0';
	}
#else
	/* get_progpath: "python" in the first folder of the PATH that has one */
	const char *env = getenv("PATH");
	for (const char *p = env; p;) {
		const char *delim = strchr(p, SYS_DELIM);
		int len = delim ? (int)(delim - p) : (int)strlen(p);
		struct stat st;
		snprintf(prog, (size_t)size, "%.*s%spython", len, p, len && p[len - 1] != SYS_SEP ? sys_sep : "");
		if (stat(prog, &st) == 0) {
			break;
		}
		prog[0] = '\0';
		p = delim ? delim + 1 : NULL;
	}
#endif
	Str_Copy(dir, prog, sizeof(dir));
	/* reduce: the folder (the last separator cut) */
	int i = (int)strlen(dir);
	while (i > 0 && dir[i] != SYS_SEP) {
		--i;
	}
	dir[i] = '\0';
	const char *home = getenv("PYTHONHOME");
	if (!home || !*home) {
		/* search_for_prefix: from the folder of the program up */
		home = NULL;
		Str_Copy(prefix, dir, size);
		do {
			char probe[0x400];
			struct stat st;
			/* join and ismodule: the landmark, or its .pyc */
			snprintf(probe, sizeof(probe), "%s%slib\\string.py", prefix, prefix[0] && prefix[strlen(prefix) - 1] !=
				SYS_SEP ? sys_sep : "");
			byte found = stat(probe, &st) == 0;
			if (!found) {
				snprintf(probe + strlen(probe), sizeof(probe) - strlen(probe), "c");
				found = stat(probe, &st) == 0;
			}
			if (found) {
				home = prefix;
				break;
			}
			int k = (int)strlen(prefix);
			while (k > 0 && prefix[k] != SYS_SEP) {
				--k;
			}
			prefix[k] = '\0';
		} while (prefix[0]);
	} else {
		Str_Copy(prefix, home, size);
	}
	const char *envpath = getenv("PYTHONPATH");
	if (envpath && *envpath) {
		Native_BufAdd(path, envpath, -1);
		Native_BufAdd(path, sys_delim, 1);
	}
#ifdef _WIN32
	char *machinepath = Sys_RegPath(HKEY_LOCAL_MACHINE), *userpath = Sys_RegPath(HKEY_CURRENT_USER);
	if (userpath) {
		Native_BufAdd(path, userpath, -1);
		Native_BufAdd(path, ";", 1);
		Mem_Free(userpath);
	}
	if (machinepath) {
		Native_BufAdd(path, machinepath, -1);
		Native_BufAdd(path, ";", 1);
		Mem_Free(machinepath);
	}
#endif
	if (!home) {
		Native_BufAdd(path, pythonpath, -1);
	} else {
		/* each part that starts with ".\" with the home for the "." */
		for (const char *p = pythonpath;;) {
			const char *q = strchr(p, SYS_DELIM);
			int k = q ? (int)(q - p) : (int)strlen(p);
			if (p[0] == '.' && p[1] == SYS_SEP) {
				Native_BufAdd(path, home, -1);
				p++;
				k--;
			}
			Native_BufAdd(path, p, k);
			if (!q) {
				break;
			}
			Native_BufAdd(path, sys_delim, 1);
			p = q + 1;
		}
	}
	Native_BufAdd(path, sys_delim, 1);
	Native_BufAdd(path, dir, -1);
}

/* the module sys of sysmodule.c Sys_Init (its functions, the files of stdin, stdout and stderr, the version, the
 * platform, the paths, the names of the built-in modules) */
static void Sys_Make(nativeVal_t *out) {
	static const char *const files[] = { "stdin", "stdout", "stderr", "__stdin__", "__stdout__", "__stderr__" };
	char prefix[0x400], prog[0x400];
	nativeVal_t f[3], v, names;
	nativeBuf_t path;
	Lib_Module(out, "sys", sys_fns, sys_doc);
	for (int i = 0; i < 3; i++) {
		Lib_StdFile(&f[i], i);
	}
	for (int i = 0; i < 6; i++) {
		Lib_Put(out, files[i], &f[i % 3]);
	}
	for (int i = 0; i < 3; i++) {
		Native_Free(&f[i]);
	}
	Native_BufInit(&path);
	Sys_Paths(&path, prefix, prog, sizeof(prefix));
	Native_SetStr(&v, "1.5.2 (#0, Nov 24 2021, 15:23:52) [MSC 32 bit (Intel)]");
	Lib_Put(out, "version", &v);
	Native_SetInt(&v, 0x010502f0);
	Lib_Put(out, "hexversion", &v);
	Native_SetStr(&v, "Copyright 1991-1995 Stichting Mathematisch Centrum, Amsterdam");
	Lib_Put(out, "copyright", &v);
	Native_SetStr(&v, "win32");
	Lib_Put(out, "platform", &v);
	Native_SetText(&v, prog, (int)strlen(prog));
	Lib_Put(out, "executable", &v);
	Native_Free(&v);
	Native_SetText(&v, prefix, (int)strlen(prefix));
	Lib_Put(out, "prefix", &v);
	Lib_Put(out, "exec_prefix", &v);
	Native_Free(&v);
	Native_SetInt(&v, LONG_MAX);
	Lib_Put(out, "maxint", &v);
	/* the names of the table, sorted */
	Native_SetEmpty(&names, NV_LIST);
	for (int i = 0; i < IMP_TABLE; i++) {
		int k = names.n;
		Native_Append(&names);
		for (; k > 0 && strcmp(Native_Text(&names.items[k - 1]), imp_table[i].name) > 0; k--) {
			names.items[k] = names.items[k - 1];
		}
		Native_SetStr(&names.items[k], imp_table[i].name);
	}
	names.type = NV_TUPLE;
	Lib_Put(out, "builtin_module_names", &names);
	Native_Free(&names);
	Lib_Put(out, "modules", &imp_store);
	/* the path as pythonrun.c Initialize made it from the search path (makepathobject) */
	Native_SetEmpty(&v, NV_LIST);
	for (const char *p = path.p;;) {
		const char *q = strchr(p, SYS_DELIM);
		int k = q ? (int)(q - p) : (int)strlen(p);
		Native_SetText(Native_Append(&v), p, k);
		if (!q) {
			break;
		}
		p = q + 1;
	}
	Native_BufFree(&path);
	Lib_Put(out, "path", &v);
	Native_Free(&v);
}

/* ---- the start and the end ---- */

const nativeVal_t *Script_Main(void) {
	return &imp_main;
}

const nativeVal_t *Script_Builtins(void) {
	return NObj_ModuleVars(&imp_builtin);
}

byte Script_ImportStart(void) {
	nativeVal_t signal;
	NObj_NewDict(&imp_store);
	Lib_MakeBuiltin(&imp_builtin);
	Imp_Store("__builtin__", &imp_builtin);
	Imp_Fixup(Imp_Builtin("__builtin__"), &imp_builtin);
	Sys_Make(&imp_sys);
	Imp_Store("sys", &imp_sys);
	Imp_Fixup(Imp_Builtin("sys"), &imp_sys);
	/* initmain */
	NObj_NewModule(&imp_main, "__main__");
	Lib_Put(&imp_main, "__builtins__", &imp_builtin);
	Imp_Store("__main__", &imp_main);
	ModVar_Keep(&imp_main);
	/* initsigs */
	Lib_MakeSignal(&signal);
	Imp_Store("signal", &signal);
	Imp_Fixup(Imp_Builtin("signal"), &signal);
	Native_Free(&signal);
	return 1;
}

void Script_ImportEnd(void) {
	nativeVal_t r;
	/* import.c Import_Cleanup: the store emptied (sys holds it) */
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
