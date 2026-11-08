/*
 * The registry of the module variables (include/scrap/modvars.h, stage 5 of docs/objlib-plan.md): the lookup by
 * module and name on the generated tables (ModVarTable.c), the overflow list of each module file and, in batch 5a,
 * the values in the slots of the module dict (OBJLIB 5).
 *
 * How the registry and the dict stay in step while the dict holds the values: every read and write here goes to the
 * dict, so the registry never keeps a value of its own. What it keeps are names: the table's, and the overflow list of
 * each module file. A name the table does not have is taken into the overflow list when ModVar_Find meets it in the
 * dict (a lookup, a write here, the walk of ModVar_Names) and when the module is made (ModVar_Made walks the dict
 * after the module code: the names of a mod's .mod text, of the C hooks and of the load time calls). An overflow name
 * stays when its value goes (a del, the module of the level gone): the list has the names ever seen, the dict the
 * values that are there now.
 */
#include <string.h>

#include "scrap/global.h"
#include "scrap/modvars.h"

/* the overflow list of a module file: sorted by name, the names Mem_Alloc'd */
typedef struct {
	modVar_t *v;
	int n, cap;
} mvExtra_t;

typedef struct {
	PyObject *dict;		/* the dict the module was made into last (only compared, never read) */
	mvExtra_t extra;
} mvFile_t;

/* one per entry of g_ModVarModules */
static mvFile_t *mv_files;

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

/* the string keys of the dict the file does not know yet into its overflow list */
static void MV_TakeNames(int file, PyObject *dict) {
	int pos = 0;
	PyObject *k, *v;
	while (PyDict_Next(dict, &pos, &k, &v)) {
		if (PyString_Check(k) && !MV_Known(file, PyString_AsString(k))) {
			MV_AddExtra(file, PyString_AsString(k));
		}
	}
}

void ModVar_Made(const char *module, const char *level, PyObject *dict) {
	int first = MV_FirstFile(module);
	if (first < 0) {
		return;
	}
	int file = -1;
	for (int i = first; i < g_ModVarModuleCount && !strcmp(g_ModVarModules[i].module, module); i++) {
		const char *l = g_ModVarModules[i].level;
		/* only one file of a name holds a dict, so a dict at the address of an old one finds no stale file */
		MV_File(i)->dict = NULL;
		if (l == level || (l && level && !strcmp(l, level))) {
			file = i;
		}
	}
	if (file >= 0) {
		MV_File(file)->dict = dict;
		MV_TakeNames(file, dict);
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
			return 1;
		}
	}
	return 0;
}

byte ModVar_Of(PyObject *module, modVarRef_t *ref) {
	return module && PyModule_Check(module) && ModVar_OfDict(PyModule_GetDict(module), ref);
}

byte ModVar_Named(const char *module, modVarRef_t *ref) {
	return ModVar_Of(PyDict_GetItemString(PyImport_GetModuleDict(), (char *)module), ref);
}

const char *ModVar_ModuleName(const modVarRef_t *ref) {
	return g_ModVarModules[ref->file].module;
}

const modVar_t *ModVar_Find(const modVarRef_t *ref, const char *name) {
	const modVar_t *e = MV_Known(ref->file, name);
	if (!e && PyDict_GetItemString(ref->dict, (char *)name)) {
		e = MV_AddExtra(ref->file, name);
	}
	return e;
}

PyObject *ModVar_Get(const modVarRef_t *ref, const char *name) {
	/* OBJLIB 5: the slot of the dict */
	return ModVar_Find(ref, name) ? PyDict_GetItemString(ref->dict, (char *)name) : NULL;
}

int ModVar_Set(const modVarRef_t *ref, const char *name, PyObject *v) {
	/* OBJLIB 5: the slot of the dict */
	if (PyDict_SetItemString(ref->dict, (char *)name, v) != 0) {
		return -1;
	}
	ModVar_Find(ref, name);
	return 0;
}

int ModVar_Del(const modVarRef_t *ref, const char *name) {
	/* OBJLIB 5: the slot of the dict; the name stays in the table or the overflow list */
	return PyDict_DelItemString(ref->dict, (char *)name);
}

PyObject *ModVar_Attr(const modVarRef_t *ref, const char *name) {
	if (!strcmp(name, "__dict__")) {
		Py_INCREF(ref->dict);
		return ref->dict;
	}
	PyObject *v = ModVar_Get(ref, name);
	if (!v) {
		PyErr_SetString(PyExc_AttributeError, (char *)name);
		return NULL;
	}
	Py_INCREF(v);
	return v;
}

int ModVar_SetAttr(const modVarRef_t *ref, const char *name, PyObject *v) {
	if (name[0] == '_' && !strcmp(name, "__dict__")) {
		PyErr_SetString(PyExc_TypeError, "read-only special attribute");
		return -1;
	}
	if (v) {
		return ModVar_Set(ref, name, v);
	}
	if (ModVar_Del(ref, name) != 0) {
		PyErr_SetString(PyExc_AttributeError, "delete non-existing module attribute");
		return -1;
	}
	return 0;
}

int ModVar_Names(const modVarRef_t *ref, const char ***names) {
	const modVarModule_t *t = &g_ModVarModules[ref->file];
	mvExtra_t *x = &MV_File(ref->file)->extra;
	MV_TakeNames(ref->file, ref->dict);
	int n = 0, i = 0, j = 0;
	*names = t->count + x->n ? Mem_Alloc((uint32_t)(sizeof(const char *) * (size_t)(t->count + x->n))) : NULL;
	/* the table and the overflow list merged, both sorted */
	while (i < t->count || j < x->n) {
		const char *name = j >= x->n || (i < t->count && strcmp(t->vars[i].name, x->v[j].name) < 0) ?
			t->vars[i++].name : x->v[j++].name;
		if (PyDict_GetItemString(ref->dict, (char *)name)) {
			(*names)[n++] = name;
		}
	}
	if (!n) {
		*names = Mem_Free(*names);
	}
	return n;
}
