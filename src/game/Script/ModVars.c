/*
 * The registry of the module variables (include/scrap/modvars.h, stage 5 of docs/objlib-plan.md): the lookup by
 * module and name on the generated tables (ModVarTable.c), the overflow list of each module file, the values of the
 * variables (batch 9b1), the access by module name that the helpers of the ports use (batch 5b) and the access by
 * namespace dict of the module code, its C hooks and the name chains (batch 5c).
 *
 * The values: every module dict a module file was made into has its variables here, a dict object of native values
 * (mvSpace_t: name -> value, in the slot order the dict of 1.5.2 gives the same stores and deletes, so keys(), items()
 * and a from-import walk go as they went). A shared list, dict, instance, function or module is held as its NV_REF,
 * a number, text, None or tuple by value. The module dict itself keeps only what the module object needs of it (its
 * __name__, __doc__, __builtins__, __file__ as the setup wrote them) and serves as the key of the module: the
 * evaluators pass it as their globals, and ModVar_OfDict leads from it to the variables. A reader that needs a Python
 * object gets the one of the value (Native_ToPy: a view's own object, native data crossing once, a number or text
 * made once and kept with the value, nativeVal_t.obj); Python code that reads the module object's attributes comes
 * here through the module type (MV_PyGetattr), and its __dict__ is the module dict filled with the variables first
 * (MV_SyncDict). A dict without variables here (Scrap, __main__, a module a test made, a class body) holds its own
 * values as before.
 *
 * The names: the table's, and the overflow list of each module file. A name the table does not have is taken into the
 * overflow list when ModVar_Find meets it among the variables (a lookup, a write, the walk of ModVar_Names) and when
 * the module is made (ModVar_Making binds the dict before the module code, so the let lines of a mod's .mod text enter
 * at their store; ModVar_Made walks the variables after the module code: the names the load time calls, a from-import
 * and the loader put in). An overflow name stays when its value goes (a del, the module of the level gone): the list
 * has the names ever seen, the variables the values that are there now.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/script.h"

/* the overflow list of a module file: sorted by name, the names Mem_Alloc'd */
typedef struct {
	modVar_t *v;
	int n, cap;
} mvExtra_t;

/* the variables of one module dict (kept while the interpreter runs: a module object that another module still holds
 * after its name was given to a new one keeps its values; ModVar_Close frees them all) */
typedef struct mvSpace_s {
	PyObject *dict;			/* the module dict (a reference held: no other dict gets its address) */
	nativeVal_t vars;		/* a dict object: name -> value */
	struct mvSpace_s *next;
} mvSpace_t;

typedef struct {
	PyObject *dict;		/* the dict the module was made into last (only compared, never read) */
	mvSpace_t *space;	/* its variables while it is bound */
	mvExtra_t extra;
} mvFile_t;

/* one per entry of g_ModVarModules */
static mvFile_t *mv_files;
static mvSpace_t *mv_spaces;
/* the module type's own getattr and setattr (MV_PyGetattr wraps them) */
static getattrfunc mv_pyGetattr;
static setattrfunc mv_pySetattr;
/* the times Python code read a module's __dict__, which crossed all its variables (SCRAP_NATIVE_COUNT) */
static unsigned long mv_synced;

static mvFile_t *MV_File(int file) {
	if (!mv_files) {
		mv_files = Mem_Calloc((uint32_t)(sizeof(mvFile_t) * (size_t)g_ModVarModuleCount));
	}
	return &mv_files[file];
}

/* the first file of the module name, -1 when the registry has none */
static int MV_FirstFile(const char *module) {
	int lo = 0, hi = g_ModVarModuleCount;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		if (strcmp(g_ModVarModules[mid].module, module) < 0) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo < g_ModVarModuleCount && !strcmp(g_ModVarModules[lo].module, module) ? lo : -1;
}

/* the index of name in a sorted array of count entries, or -(the place to insert it) - 1 */
static int MV_Search(const modVar_t *vars, int count, const char *name) {
	int lo = 0, hi = count;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		int c = strcmp(vars[mid].name, name);
		if (c == 0) {
			return mid;
		}
		if (c < 0) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return -lo - 1;
}

/* the entry of name in the table or the overflow list of a file; NULL when neither has it */
static const modVar_t *MV_Known(int file, const char *name) {
	const modVarModule_t *t = &g_ModVarModules[file];
	int i = MV_Search(t->vars, t->count, name);
	if (i >= 0) {
		return &t->vars[i];
	}
	mvExtra_t *x = &MV_File(file)->extra;
	i = MV_Search(x->v, x->n, name);
	return i >= 0 ? &x->v[i] : NULL;
}

/* name into the overflow list of a file (the caller knows it is not there) */
static const modVar_t *MV_AddExtra(int file, const char *name) {
	mvExtra_t *x = &MV_File(file)->extra;
	int at = -MV_Search(x->v, x->n, name) - 1;
	if (x->n == x->cap) {
		x->cap = x->cap ? x->cap * 2 : 16;
		x->v = Mem_Realloc(x->v, (uint32_t)(sizeof(modVar_t) * (size_t)x->cap));
	}
	int size = (int)strlen(name) + 1;
	char *copy = Mem_Alloc((uint32_t)size);
	Str_Copy(copy, name, size);
	memmove(&x->v[at + 1], &x->v[at], sizeof(modVar_t) * (size_t)(x->n - at));
	x->v[at] = (modVar_t){ copy, MVK_EXTRA, MVT_ANY, 0, NULL };
	x->n++;
	return &x->v[at];
}

/* ---- the values ---- */

/* the key of a name */
static void MV_Key(nativeVal_t *key, const char *name) {
	Native_SetText(key, name, (int)strlen(name));
}

/* the native table of ref's variables, or NULL with *py the Python dict that holds them (a dict without variables
 * here, or variables whose dict object crossed to Python) */
static nativeVal_t *MV_Table(const modVarRef_t *ref, PyObject **py) {
	nativeVal_t *t = ref->vars ? NObj_DictData(ref->vars) : NULL;
	*py = t ? NULL : ref->vars ? NObj_Py(ref->vars) : ref->dict;
	return t;
}

/* the value of name in a native table (borrowed, valid until the table changes), NULL when it has none */
static nativeVal_t *MV_Slot(nativeVal_t *t, const char *name) {
	nativeVal_t key;
	MV_Key(&key, name);
	nativeVal_t *x = Native_DictGet(t, &key);
	Native_Free(&key);
	return x;
}

/* a value of Python as a variable holds it: a number, text, None or tuple by value with the object kept (the same
 * object goes back to Python), an entity handle as its handle, a list, dict, instance, class, function, method or
 * module as its view, anything else as NV_OBJ. SCRAP_NATIVE_REF=0 too: the readers of the dict slot gave that form
 * (the channel's TX_FromPy), an NV_OBJ here shows its repr to tc_sdn and is no tuple to `is` */
static void MV_FromPy(PyObject *o, nativeVal_t *v) {
	NObj_FromPy(o, v);
	if (v->type != NV_OBJ && v->type != NV_REF) {
		Native_KeepPy(v, o);
	}
}

/* *out the form of v a variable holds: lists and dicts as objects (NObj_Share), a vector as the tuple of three floats
 * it is in Python, a tuple with its Python tuple and an entity handle with its Python object (one object for `is`, id
 * and hash, as the dict slot had it; native data in a tuple crosses);
 * SCRAP_NATIVE_REF=0: the value as Python made it, a value that holds native objects (there are no views then) as it
 * is. 0 with the error */
static byte MV_Form(const nativeVal_t *v, nativeVal_t *out) {
	if (!NObj_RefMode() && !Native_Holds(v, 1u << NV_REF)) {
		PyObject *o = Native_ToPy(v);
		Native_SetNone(out);
		if (!o) {
			return 0;
		}
		MV_FromPy(o, out);
		Py_DECREF(o);
		return 1;
	}
	if (v->type == NV_VEC) {
		Native_SetEmpty(out, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetFloat(Native_Append(out), v->v[i]);
		}
	} else {
		Native_Copy(out, v);
	}
	NObj_Share(out);
	if (out->type == NV_ENT && !Native_KeptPy(out)) {
		/* the handle's Python object lives while the variable holds it, as in the dict slot: its address is the one
		 * id, hash, repr and cmp of the handle show */
		PyObject *o = Native_ToPy(out);
		if (o) {
			Native_KeepPy(out, o);
			Py_DECREF(o);
		}
		Native_ErrClear();
	}
	if (out->type == NV_TUPLE && !Native_KeptPy(out)) {
		PyObject *o = Native_ToPy(out);
		if (!o) {
			Native_Free(out);
			return 0;
		}
		Native_KeepPy(out, o);
		Py_DECREF(o);
	}
	return 1;
}

/* the Python object of a variable's value (borrowed, kept by the value): an object's, native data crossing here; a
 * value by value gets its object made once */
static PyObject *MV_Py(nativeVal_t *s) {
	if (s->type == NV_OBJ) {
		return s->obj;
	}
	if (s->type == NV_REF) {
		PyObject *o = NObj_ToPy(s);
		Py_XDECREF(o);
		return o;
	}
	if (!s->obj) {
		PyObject *o = Native_ToPy(s);
		if (!o) {
			return NULL;
		}
		Native_KeepPy(s, o);
		Py_DECREF(o);
	}
	return s->obj;
}

/* name has a value now (a name of the table or a new one, which goes to the overflow list) */
static void MV_Seen(const modVarRef_t *ref, const char *name) {
	if (ref->file >= 0 && !MV_Known(ref->file, name)) {
		MV_AddExtra(ref->file, name);
	}
}

/* the string keys of the variables the file does not know yet into its overflow list */
static void MV_TakeNames(const modVarRef_t *ref) {
	nativeVal_t k, v;
	int pos = 0;
	while (ModVar_Next(ref, &pos, &k, &v)) {
		if (k.type == NV_STR && (int)strlen(Native_Text(&k)) == k.n) {
			MV_Seen(ref, Native_Text(&k));
		}
		Native_Free(&k);
		Native_Free(&v);
	}
}

/* ---- the variables of a module dict ---- */

static mvSpace_t *MV_SpaceOf(PyObject *dict) {
	for (mvSpace_t *s = mv_spaces; s; s = s->next) {
		if (s->dict == dict) {
			return s;
		}
	}
	return NULL;
}

/* key = value of the dict into the native table t (value as MV_FromPy takes it) */
static void MV_PutPy(nativeVal_t *t, PyObject *key, PyObject *value) {
	nativeVal_t k, v;
	MV_FromPy(key, &k);
	MV_FromPy(value, &v);
	if (!Native_DictSet(t, &k, &v)) {
		/* an unhashable key cannot be in a dict */
		Native_ErrClear();
	}
}

static byte MV_IsName(PyObject *key, const char *const *names, int n) {
	for (int i = 0; i < n; i++) {
		if (PyString_Check(key) && !strcmp(PyString_AsString(key), names[i])) {
			return 1;
		}
	}
	return 0;
}

static PyObject *MV_PyGetattr(PyObject *m, char *name);
static int MV_PySetattr(PyObject *m, char *name, PyObject *v);

/*
 * the variables of dict, made at its first binding with the stores the dict had then, in the order they came: __name__
 * and __doc__ (PyModule_New), what else is there, __builtins__ and __file__ (the setup of Script_ModuleMake). At a
 * binding for the module code the writes of that setup again (making: __builtins__ when it has none, __file__), which
 * may resize the table as they resized the dict
 */
static mvSpace_t *MV_Bind(PyObject *dict, byte making) {
	static const char *const first[] = { "__name__", "__doc__" }, *const last[] = { "__builtins__", "__file__" };
	mvSpace_t *s = MV_SpaceOf(dict);
	if (!mv_pyGetattr) {
		mv_pyGetattr = PyModule_Type.tp_getattr;
		mv_pySetattr = PyModule_Type.tp_setattr;
		PyModule_Type.tp_getattr = MV_PyGetattr;
		PyModule_Type.tp_setattr = MV_PySetattr;
	}
	if (s) {
		nativeVal_t *t = NObj_DictData(&s->vars);
		PyObject *b = PyDict_GetItemString(dict, "__builtins__"), *f = PyDict_GetItemString(dict, "__file__");
		PyObject *key;
		if (t && making && b && !MV_Slot(t, "__builtins__") && (key = PyString_FromString("__builtins__")) != NULL) {
			MV_PutPy(t, key, b);
			Py_DECREF(key);
		}
		if (t && making && f && (key = PyString_FromString("__file__")) != NULL) {
			MV_PutPy(t, key, f);
			Py_DECREF(key);
		}
		return s;
	}
	s = Mem_Calloc(sizeof(*s));
	Py_INCREF(dict);
	s->dict = dict;
	NObj_NewDict(&s->vars);
	s->next = mv_spaces;
	mv_spaces = s;
	nativeVal_t *t = NObj_DictData(&s->vars);
	PyObject *k, *v;
	int pos = 0;
	for (int i = 0; i < 2; i++) {
		if ((v = PyDict_GetItemString(dict, (char *)first[i])) != NULL && (k = PyString_FromString(first[i])) != NULL) {
			MV_PutPy(t, k, v);
			Py_DECREF(k);
		}
	}
	while (PyDict_Next(dict, &pos, &k, &v)) {
		if (!MV_IsName(k, first, 2) && !MV_IsName(k, last, 2)) {
			MV_PutPy(t, k, v);
		}
	}
	for (int i = 0; i < 2; i++) {
		if ((v = PyDict_GetItemString(dict, (char *)last[i])) != NULL && (k = PyString_FromString(last[i])) != NULL) {
			MV_PutPy(t, k, v);
			Py_DECREF(k);
		}
	}
	Native_ErrClear();
	return s;
}

static void MV_Made(const char *module, const char *level, PyObject *dict, byte making) {
	int first = MV_FirstFile(module);
	if (first < 0) {
		return;
	}
	int file = -1;
	for (int i = first; i < g_ModVarModuleCount && !strcmp(g_ModVarModules[i].module, module); i++) {
		const char *l = g_ModVarModules[i].level;
		/* only one file of a name holds a dict, so a dict at the address of an old one finds no stale file */
		MV_File(i)->dict = NULL;
		MV_File(i)->space = NULL;
		if (l == level || (l && level && !strcmp(l, level))) {
			file = i;
		}
	}
	if (file >= 0) {
		modVarRef_t ref;
		MV_File(file)->dict = dict;
		MV_File(file)->space = MV_Bind(dict, making);
		ref.file = file;
		ref.dict = dict;
		ref.vars = &MV_File(file)->space->vars;
		MV_TakeNames(&ref);
	}
}

void ModVar_Made(const char *module, const char *level, PyObject *dict) {
	MV_Made(module, level, dict, 0);
}

void ModVar_Making(const char *module, const char *level, PyObject *dict, modVarRef_t *ref, modVarRef_t *was) {
	int first = MV_FirstFile(module);
	was->file = -1;
	was->dict = NULL;
	was->vars = NULL;
	for (int i = first; first >= 0 && i < g_ModVarModuleCount && !strcmp(g_ModVarModules[i].module, module); i++) {
		if (MV_File(i)->dict) {
			was->file = i;
			was->dict = MV_File(i)->dict;
			was->vars = &MV_File(i)->space->vars;
		}
	}
	MV_Made(module, level, dict, 1);
	ModVar_RefOfDict(dict, ref);
}

void ModVar_Unmade(const char *module, const modVarRef_t *was) {
	int first = MV_FirstFile(module);
	for (int i = first; first >= 0 && i < g_ModVarModuleCount && !strcmp(g_ModVarModules[i].module, module); i++) {
		MV_File(i)->dict = NULL;
		MV_File(i)->space = NULL;
	}
	if (was->file >= 0) {
		MV_File(was->file)->dict = was->dict;
		MV_File(was->file)->space = MV_SpaceOf(was->dict);
	}
}

/* the variables of a module set to None as _PyModule_Clear sets the values of its dict (PyImport_Cleanup of
 * Py_Finalize): the names with a single underscore first, then every name but __builtins__ */
static void MV_Clear(mvSpace_t *s) {
	modVarRef_t ref = { .file = -1, .dict = s->dict, .vars = &s->vars };
	for (int pass = 0; pass < 2; pass++) {
		nativeVal_t k, v, none;
		int pos = 0;
		while (ModVar_Next(&ref, &pos, &k, &v)) {
			/* a key with a NUL in it has no name to set it by (no module code makes one) */
			const char *n = k.type == NV_STR && (int)strlen(Native_Text(&k)) == k.n ? Native_Text(&k) : NULL;
			if (n && v.type != NV_NONE && (pass ? strcmp(n, "__builtins__") != 0 : n[0] == '_' && n[1] != '_')) {
				Native_SetNone(&none);
				ModVar_SetVal(&ref, n, &none);
			}
			Native_Free(&k);
			Native_Free(&v);
		}
	}
	Native_ErrClear();
}

void ModVar_Close(void) {
	const char *e = getenv("SCRAP_NATIVE_COUNT");
	if (e && !strcmp(e, "1")) {
		nativeBuf_t b;
		Native_BufInit(&b);
		ModVar_Report(&b);
		fputs(b.p, stdout);
		fflush(stdout);
		Native_BufFree(&b);
	}
	for (mvSpace_t *s = mv_spaces; s; s = s->next) {
		MV_Clear(s);
	}
	while (mv_spaces) {
		mvSpace_t *s = mv_spaces;
		mv_spaces = s->next;
		Native_Free(&s->vars);
		Py_DECREF(s->dict);
		Mem_Free(s);
	}
	for (int i = 0; mv_files && i < g_ModVarModuleCount; i++) {
		mv_files[i].dict = NULL;
		mv_files[i].space = NULL;
	}
}

byte ModVar_OfDict(PyObject *dict, modVarRef_t *ref) {
	if (!dict || !PyDict_Check(dict)) {
		return 0;
	}
	PyObject *name = PyDict_GetItemString(dict, "__name__");
	PyObject *file = PyDict_GetItemString(dict, "__file__");
	if (!name || !PyString_Check(name) || !file || !PyString_Check(file) ||
		strncmp(PyString_AsString(file), "PACK:", 5)) {
		return 0;
	}
	const char *module = PyString_AsString(name);
	int first = MV_FirstFile(module);
	for (int i = first; first >= 0 && i < g_ModVarModuleCount && !strcmp(g_ModVarModules[i].module, module); i++) {
		if (MV_File(i)->dict == dict) {
			ref->file = i;
			ref->dict = dict;
			ref->vars = &MV_File(i)->space->vars;
			return 1;
		}
	}
	/* a module object whose name another one has now, or whose module code failed: its variables without a table */
	mvSpace_t *s = MV_SpaceOf(dict);
	if (s) {
		ref->file = -1;
		ref->dict = dict;
		ref->vars = &s->vars;
		return 1;
	}
	return 0;
}

byte ModVar_Of(PyObject *module, modVarRef_t *ref) {
	return module && PyModule_Check(module) && ModVar_OfDict(PyModule_GetDict(module), ref);
}

byte ModVar_Named(const char *module, modVarRef_t *ref) {
	return ModVar_Of(Script_ModuleLoaded(module), ref);
}

byte ModVar_OfVal(const nativeVal_t *v, modVarRef_t *ref) {
	PyObject *o = NObj_Py(v);
	return o && ModVar_Of(o, ref);
}

void ModVar_RefOfDict(PyObject *dict, modVarRef_t *ref) {
	if (!ModVar_OfDict(dict, ref)) {
		ref->file = -1;
		ref->dict = dict;
		ref->vars = NULL;
	}
}

const char *ModVar_ModuleName(const modVarRef_t *ref) {
	return g_ModVarModules[ref->file].module;
}

byte ModVar_HasModule(const char *module) {
	return MV_FirstFile(module) >= 0;
}

const modVar_t *ModVar_Find(const modVarRef_t *ref, const char *name) {
	if (ref->file < 0) {
		return NULL;
	}
	const modVar_t *e = MV_Known(ref->file, name);
	if (!e && ModVar_Has(ref, name)) {
		e = MV_AddExtra(ref->file, name);
	}
	return e;
}

byte ModVar_Has(const modVarRef_t *ref, const char *name) {
	PyObject *py;
	nativeVal_t *t = MV_Table(ref, &py);
	return t ? MV_Slot(t, name) != NULL : PyDict_GetItemString(py, (char *)name) != NULL;
}

byte ModVar_GetVal(const modVarRef_t *ref, const char *name, nativeVal_t *out) {
	PyObject *py, *o;
	nativeVal_t *t = MV_Table(ref, &py), *s;
	Native_SetNone(out);
	if (!t) {
		if ((o = PyDict_GetItemString(py, (char *)name)) == NULL) {
			return 0;
		}
		MV_FromPy(o, out);
	} else if ((s = MV_Slot(t, name)) != NULL) {
		Native_Copy(out, s);
	} else {
		return 0;
	}
	MV_Seen(ref, name);
	return 1;
}

int ModVar_SetVal(const modVarRef_t *ref, const char *name, const nativeVal_t *v) {
	PyObject *py;
	nativeVal_t *t = MV_Table(ref, &py), key, x;
	if (!t) {
		PyObject *o = Native_ToPy(v);
		int r = o ? PyDict_SetItemString(py, (char *)name, o) : -1;
		Py_XDECREF(o);
		if (r == 0) {
			MV_Seen(ref, name);
		}
		return r;
	}
	if (!MV_Form(v, &x)) {
		return -1;
	}
	MV_Key(&key, name);
	Native_DictSet(t, &key, &x);
	MV_Seen(ref, name);
	return 0;
}

PyObject *ModVar_Get(const modVarRef_t *ref, const char *name) {
	PyObject *py, *o;
	nativeVal_t *t = MV_Table(ref, &py), *s;
	if (!t) {
		o = PyDict_GetItemString(py, (char *)name);
	} else {
		o = (s = MV_Slot(t, name)) != NULL ? MV_Py(s) : NULL;
	}
	if (o) {
		MV_Seen(ref, name);
	}
	return o;
}

int ModVar_Set(const modVarRef_t *ref, const char *name, PyObject *v) {
	PyObject *py;
	nativeVal_t *t = MV_Table(ref, &py), key, x;
	if (!t) {
		if (PyDict_SetItemString(py, (char *)name, v) != 0) {
			return -1;
		}
	} else {
		MV_FromPy(v, &x);
		MV_Key(&key, name);
		Native_DictSet(t, &key, &x);
	}
	MV_Seen(ref, name);
	return 0;
}

int ModVar_Del(const modVarRef_t *ref, const char *name) {
	/* the name stays in the table or the overflow list */
	PyObject *py;
	nativeVal_t *t = MV_Table(ref, &py), key;
	if (!t) {
		return PyDict_DelItemString(py, (char *)name);
	}
	MV_Key(&key, name);
	byte ok = Native_DictDel(t, &key);
	if (!ok) {
		/* PyDict_DelItemString's KeyError */
		Native_RaiseKey(&key);
	}
	Native_Free(&key);
	if (!ok) {
		return -1;
	}
	return 0;
}

byte ModVar_Next(const modVarRef_t *ref, int *pos, nativeVal_t *key, nativeVal_t *value) {
	PyObject *py, *k, *v;
	nativeVal_t *t = MV_Table(ref, &py);
	const nativeVal_t *nk, *nv;
	Native_SetNone(key);
	Native_SetNone(value);
	if (!t) {
		if (!PyDict_Next(py, pos, &k, &v)) {
			return 0;
		}
		MV_FromPy(k, key);
		MV_FromPy(v, value);
		return 1;
	}
	if (!Native_DictNext(t, pos, &nk, &nv)) {
		return 0;
	}
	Native_Copy(key, nk);
	Native_Copy(value, nv);
	return 1;
}

void ModVar_Space(const modVarRef_t *ref, nativeVal_t *out) {
	if (ref->vars) {
		Native_Copy(out, ref->vars);
	} else if (!Native_FromPyRef(ref->dict, out)) {
		Native_SetObj(out, ref->dict);
	}
}

/* the module dict with the variables as Python objects, for Python code that reads __dict__ (dir() of a module, the
 * channel's fallbacks to Python builtins): every variable crosses; the dict object of the variables when it crossed
 * itself (a new reference) */
static PyObject *MV_SyncDict(const modVarRef_t *ref) {
	PyObject *py, *keys;
	nativeVal_t *t = MV_Table(ref, &py);
	const nativeVal_t *k, *v;
	int pos = 0;
	if (!t) {
		Py_INCREF(py);
		return py;
	}
	mv_synced++;
	while (Native_DictNext(t, &pos, &k, &v)) {
		PyObject *pk = MV_Py((nativeVal_t *)k), *pv = pk ? MV_Py((nativeVal_t *)v) : NULL;
		if (!pv || PyDict_SetItem(ref->dict, pk, pv) != 0) {
			Native_ErrClear();
		}
	}
	keys = PyDict_Keys(ref->dict);
	for (int i = 0; keys && i < PyList_GET_SIZE(keys); i++) {
		nativeVal_t nk;
		MV_FromPy(PyList_GET_ITEM(keys, i), &nk);
		if (!Native_DictGet(t, &nk)) {
			PyDict_DelItem(ref->dict, PyList_GET_ITEM(keys, i));
		}
		Native_Free(&nk);
	}
	Py_XDECREF(keys);
	Native_ErrClear();
	Py_INCREF(ref->dict);
	return ref->dict;
}

PyObject *ModVar_Attr(const modVarRef_t *ref, const char *name) {
	if (!strcmp(name, "__dict__")) {
		if (ref->vars) {
			return MV_SyncDict(ref);
		}
		Py_INCREF(ref->dict);
		return ref->dict;
	}
	PyObject *v = ModVar_Get(ref, name);
	if (!v) {
		Native_Raise(NE_ATTRIBUTE, "%s", name);
		return NULL;
	}
	Py_INCREF(v);
	return v;
}

int ModVar_SetAttr(const modVarRef_t *ref, const char *name, PyObject *v) {
	if (name[0] == '_' && !strcmp(name, "__dict__")) {
		Native_Raise(NE_TYPE, "read-only special attribute");
		return -1;
	}
	if (v) {
		return ModVar_Set(ref, name, v);
	}
	if (ModVar_Del(ref, name) != 0) {
		Native_Raise(NE_ATTRIBUTE, "delete non-existing module attribute");
		return -1;
	}
	return 0;
}

byte ModVar_AttrVal(const modVarRef_t *ref, const char *name, nativeVal_t *out) {
	if (!strcmp(name, "__dict__")) {
		ModVar_Space(ref, out);
		return 1;
	}
	return ModVar_GetVal(ref, name, out) || Native_Raise(NE_ATTRIBUTE, "%s", name);
}

byte ModVar_SetAttrVal(const modVarRef_t *ref, const char *name, const nativeVal_t *v) {
	if (name[0] == '_' && !strcmp(name, "__dict__")) {
		return Native_Raise(NE_TYPE, "read-only special attribute");
	}
	if (v) {
		return ModVar_SetVal(ref, name, v) == 0;
	}
	if (ModVar_Del(ref, name) != 0) {
		Native_ErrClear();
		return Native_Raise(NE_ATTRIBUTE, "delete non-existing module attribute");
	}
	return 1;
}

static int MV_CompareNames(const void *a, const void *b) {
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int ModVar_Names(const modVarRef_t *ref, const char ***names) {
	int n = 0;
	if (ref->file < 0) {
		/* the string keys of the dict, as texts that live while the module has them */
		PyObject *py, *k, *v;
		int pos = 0;
		nativeVal_t *t = MV_Table(ref, &py);
		const nativeVal_t *nk, *nv;
		int cap = t ? t->n : PyDict_Size(py);
		*names = cap ? Mem_Alloc((uint32_t)(sizeof(const char *) * (size_t)cap)) : NULL;
		while (n < cap && (t ? Native_DictNext(t, &pos, &nk, &nv) : PyDict_Next(py, &pos, &k, &v))) {
			if (t && nk->type == NV_STR && (int)strlen(Native_Text(nk)) == nk->n) {
				(*names)[n++] = Native_Text(nk);
			} else if (!t && PyString_Check(k)) {
				(*names)[n++] = PyString_AsString(k);
			}
		}
		if (n) {
			qsort((void *)*names, (size_t)n, sizeof(const char *), MV_CompareNames);
		}
	} else {
		const modVarModule_t *t = &g_ModVarModules[ref->file];
		mvExtra_t *x = &MV_File(ref->file)->extra;
		int i = 0, j = 0;
		MV_TakeNames(ref);
		*names = t->count + x->n ? Mem_Alloc((uint32_t)(sizeof(const char *) * (size_t)(t->count + x->n))) : NULL;
		/* the table and the overflow list merged, both sorted */
		while (i < t->count || j < x->n) {
			const char *name = j >= x->n || (i < t->count && strcmp(t->vars[i].name, x->v[j].name) < 0) ?
				t->vars[i++].name : x->v[j++].name;
			if (ModVar_Has(ref, name)) {
				(*names)[n++] = name;
			}
		}
	}
	if (!n) {
		*names = Mem_Free(*names);
	}
	return n;
}

/* ---- by module name: the one path of the helpers of the ports (Script_Global, Script_Attr, the Mode_ helpers) ---- */

/* the ref of a module object: its variables here (a registry module, or one that had its name), else (Scrap,
 * __main__, a module of the C or of a test) its dict alone (file -1, vars NULL); 0 for an object in sys.modules that
 * is no module */
static byte MV_Ref(PyObject *m, modVarRef_t *ref) {
	if (ModVar_Of(m, ref)) {
		return 1;
	}
	if (!PyModule_Check(m)) {
		return 0;
	}
	ref->file = -1;
	ref->dict = PyModule_GetDict(m);
	ref->vars = NULL;
	return 1;
}

byte ModVar_RefOf(PyObject *m, modVarRef_t *ref) {
	return m && MV_Ref(m, ref);
}

PyObject *ModVar_Lookup(const char *module, const char *name, byte *imported) {
	PyObject *m = Script_Module(module);
	modVarRef_t ref;
	PyObject *v = m && MV_Ref(m, &ref) ? ModVar_Get(&ref, name) : NULL;
	*imported = m != NULL;
	Py_XINCREF(v);
	Py_XDECREF(m);
	return v;
}

byte ModVar_GlobalVal(const char *module, const char *name, nativeVal_t *out) {
	PyObject *m = Script_Module(module);
	modVarRef_t ref;
	byte ok = m && MV_Ref(m, &ref) && ModVar_GetVal(&ref, name, out);
	Py_XDECREF(m);
	if (!ok) {
		Native_SetNone(out);
	}
	return ok || (m && Native_Raise(NE_NAME, "%s", name));
}

int ModVar_SetGlobal(const char *module, const char *name, PyObject *v) {
	PyObject *m = Script_Module(module);
	modVarRef_t ref;
	int r = -1;
	if (m && MV_Ref(m, &ref)) {
		r = ModVar_Set(&ref, name, v);
	} else if (m) {
		/* PyModule_GetDict's SystemError */
		Native_Raise(NE_SYSTEM, "bad argument to internal function");
	}
	Py_XDECREF(m);
	return r;
}

byte ModVar_SetGlobalVal(const char *module, const char *name, const nativeVal_t *v) {
	PyObject *m = Script_Module(module);
	modVarRef_t ref;
	byte ok = 0;
	if (m && MV_Ref(m, &ref)) {
		ok = ModVar_SetVal(&ref, name, v) == 0;
	} else if (m) {
		Native_Raise(NE_SYSTEM, "bad argument to internal function");
	}
	Py_XDECREF(m);
	return ok;
}

int ModVar_DelGlobal(const char *module, const char *name) {
	PyObject *m = Script_Module(module);
	modVarRef_t ref;
	int r = m ? 0 : -1;
	if (m && (!MV_Ref(m, &ref) || ModVar_Del(&ref, name) != 0)) {
		Native_ErrClear();
		Native_Raise(NE_NAME, "%s", name);
		r = -1;
	}
	Py_XDECREF(m);
	return r;
}

PyObject *ModVar_ObjAttr(PyObject *o, const char *name) {
	modVarRef_t ref;
	return ModVar_Of(o, &ref) ? ModVar_Attr(&ref, name) : PyObject_GetAttrString(o, (char *)name);
}

int ModVar_SetObjAttr(PyObject *o, const char *name, PyObject *v) {
	modVarRef_t ref;
	return ModVar_Of(o, &ref) ? ModVar_SetAttr(&ref, name, v) : PyObject_SetAttrString(o, (char *)name, v);
}

PyObject *ModVar_ModAttr(const char *module, const char *name) {
	PyObject *m = Script_Module(module);
	PyObject *v = m ? ModVar_ObjAttr(m, name) : NULL;
	Py_XDECREF(m);
	return v;
}

int ModVar_SetModAttr(const char *module, const char *name, PyObject *v) {
	PyObject *m = Script_Module(module);
	int r = m ? ModVar_SetObjAttr(m, name, v) : -1;
	Py_XDECREF(m);
	return r;
}

byte ModVar_ModAttrVal(const char *module, const char *name, nativeVal_t *out) {
	PyObject *m = Script_Module(module);
	modVarRef_t ref;
	byte ok = 0;
	Native_SetNone(out);
	if (m && ModVar_Of(m, &ref)) {
		ok = ModVar_AttrVal(&ref, name, out);
	} else if (m) {
		PyObject *v = PyObject_GetAttrString(m, (char *)name);
		if (v) {
			MV_FromPy(v, out);
			Py_DECREF(v);
			ok = 1;
		}
	}
	Py_XDECREF(m);
	return ok;
}

byte ModVar_SetModAttrVal(const char *module, const char *name, const nativeVal_t *v) {
	PyObject *m = Script_Module(module);
	modVarRef_t ref;
	byte ok = 0;
	if (m && ModVar_Of(m, &ref)) {
		ok = ModVar_SetAttrVal(&ref, name, v);
	} else if (m) {
		PyObject *x = v ? Native_ToPy(v) : NULL;
		ok = (!v || x) && PyObject_SetAttrString(m, (char *)name, x) == 0;
		Py_XDECREF(x);
	}
	Py_XDECREF(m);
	return ok;
}

/* ---- Python code on the module objects: getattr and setattr of the module type through the registry ---- */

static PyObject *MV_PyGetattr(PyObject *m, char *name) {
	modVarRef_t ref;
	return ModVar_Of(m, &ref) ? ModVar_Attr(&ref, name) : mv_pyGetattr(m, name);
}

static int MV_PySetattr(PyObject *m, char *name, PyObject *v) {
	modVarRef_t ref;
	return ModVar_Of(m, &ref) ? ModVar_SetAttr(&ref, name, v) : mv_pySetattr(m, name, v);
}

/* ---- by namespace dict (batch 5c): the .mod evaluator, Script_NameChain, the C hooks of module code ---- */

PyObject *ModVar_DictGet(PyObject *dict, const char *name) {
	modVarRef_t ref;
	ModVar_RefOfDict(dict, &ref);
	return ModVar_Get(&ref, name);
}

int ModVar_DictSet(PyObject *dict, const char *name, PyObject *v) {
	modVarRef_t ref;
	ModVar_RefOfDict(dict, &ref);
	return ModVar_Set(&ref, name, v);
}

int ModVar_DictDel(PyObject *dict, const char *name) {
	modVarRef_t ref;
	ModVar_RefOfDict(dict, &ref);
	return ModVar_Del(&ref, name);
}

/* ---- the measure: what the variables hold (SCRAP_NATIVE_COUNT=1 at the end, the channel's "native vars") ---- */

/* the kinds of values counted: by value, entity handle, NV_OBJ, then the NO_ kinds of objects */
enum { MVC_PLAIN, MVC_ENT, MVC_OBJ, MVC_KINDS };
#define MVC_OBJKINDS 7

typedef struct {
	unsigned long other[MVC_KINDS];
	unsigned long obj[MVC_OBJKINDS][3];	/* per NO_ kind: native data, view of a Python object, crossed */
} mvCounts_t;

/* each value of v at any depth inside tuples counted */
static void MV_Count(const nativeVal_t *v, mvCounts_t *c) {
	if (v->type == NV_TUPLE) {
		for (int i = 0; i < v->n; i++) {
			MV_Count(&v->items[i], c);
		}
		return;
	}
	if (v->type == NV_REF && NObj_Kind(v) < MVC_OBJKINDS) {
		c->obj[NObj_Kind(v)][NObj_State(v)]++;
	} else {
		c->other[v->type == NV_OBJ ? MVC_OBJ : v->type == NV_ENT ? MVC_ENT : MVC_PLAIN]++;
	}
}

void ModVar_Report(nativeBuf_t *b) {
	static const char *const kinds[MVC_OBJKINDS] = { "lists", "dicts", "instances", "classes", "functions", "methods",
		"modules" };
	mvCounts_t c;
	unsigned long spaces = 0, vars = 0, crossed = 0, builtins = 0;
	char line[0x100];
	memset(&c, 0, sizeof(c));
	for (mvSpace_t *s = mv_spaces; s; s = s->next) {
		nativeVal_t *t = NObj_DictData(&s->vars);
		const nativeVal_t *k, *v;
		int pos = 0;
		spaces++;
		if (!t) {
			crossed++;
			continue;
		}
		while (Native_DictNext(t, &pos, &k, &v)) {
			vars++;
			if (k->type == NV_STR && !strcmp(Native_Text(k), "__builtins__")) {
				/* the builtins dict of Python every module holds, counted apart */
				builtins++;
				continue;
			}
			MV_Count(v, &c);
		}
	}
	snprintf(line, sizeof(line), "module variables %lu in %lu modules (%lu crossed, %lu __dict__ reads by Python): by "
		"value %lu, entity handles %lu, Python objects %lu, __builtins__ %lu\n", vars, spaces, crossed, mv_synced,
		c.other[MVC_PLAIN], c.other[MVC_ENT], c.other[MVC_OBJ], builtins);
	Native_BufAdd(b, line, -1);
	for (int i = 0; i < MVC_OBJKINDS; i++) {
		snprintf(line, sizeof(line), "module variables %s native %lu views %lu crossed %lu\n", kinds[i], c.obj[i][0],
			c.obj[i][1], c.obj[i][2]);
		Native_BufAdd(b, line, -1);
	}
}
