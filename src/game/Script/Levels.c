/*
 * The scripts of the levels in C (issue #2): each level loads modules of the same names (Map, MapSnd, the missions)
 * from its own pack. The functions of a level module that are data are the sections of a file
 * data/levels/<level>/<module>.def ("section <Func> <params>"; a parameter with a default is name=value); when the
 * module loads in that level, each section becomes the function of that name. The functions the file does not have
 * stay Python, and so do the module variables and the module code.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/script.h"

typedef struct {
	const char *level;		/* the folder of the level in the packs, lower case: "police" */
	const char *module;		/* the module name: "MapSnd" */
	const char *name;		/* the file for Script_RunFile: "levels/police/mapsnd.def" */
	const char *text;		/* the built-in copy */
} levelFile_t;

#include "LevelFiles.inc"

/* the functions of level modules that are C (logic): a table per level and module, put in after the data */
typedef struct {
	const char *level;
	const char *module;
	PyMethodDef *methods;
} levelModule_t;

extern PyMethodDef g_MenuMapMethods[];

static const levelModule_t level_modules[] = {
	{ "menu", "Map", g_MenuMapMethods },
	{ NULL, NULL, NULL },
};

enum { LEVEL_FUNCS = 96, LEVEL_POOLS = 8, LEVEL_NAME = 0x40 };

/* the method definitions of the functions of one module name; reused when the module loads again */
typedef struct {
	char module[LEVEL_NAME];
	PyMethodDef defs[LEVEL_FUNCS];
	char names[LEVEL_FUNCS][LEVEL_NAME];
} levelPool_t;

static levelPool_t g_LevelPools[LEVEL_POOLS];

static levelPool_t *Level_Pool(const char *module) {
	for (int i = 0; i < LEVEL_POOLS; i++) {
		if (!strcmp(g_LevelPools[i].module, module)) {
			return &g_LevelPools[i];
		}
	}
	for (int i = 0; i < LEVEL_POOLS; i++) {
		if (!g_LevelPools[i].module[0]) {
			Str_Copy(g_LevelPools[i].module, module, sizeof(g_LevelPools[i].module));
			return &g_LevelPools[i];
		}
	}
	return NULL;
}

/* the folder of the level that loads: the last part of Init.Path in lower case ("Levels/Police" -> "police") */
static byte Level_Current(char *out, int size) {
	PyObject *path = Script_Attr("Init", "Path");
	if (!path || !PyString_Check(path)) {
		Py_XDECREF(path);
		PyErr_Clear();
		return 0;
	}
	const char *p = PyString_AsString(path), *slash = strrchr(p, '/');
	Str_Copy(out, slash ? slash + 1 : p, size);
	for (char *c = out; *c; c++) {
		if (*c >= 'A' && *c <= 'Z') {
			*c = (char)(*c - 'A' + 'a');
		}
	}
	Py_DECREF(path);
	return 1;
}

/* "run vecrot <name> <pos> <offset> <angles>": $name = SVec.Add(pos, SVec.Rotate3D(offset, angles)), the position of a
 * sound beside an element of the map */
static void Level_StepVecRot(int id, PyObject *args) {
	PyObject *name, *pos, *off, *ang;
	(void)id;
	if (!PyArg_ParseTuple(args, "SOOO", &name, &pos, &off, &ang)) {
		return;
	}
	PyObject *rot = Script_Ask("SVec", "Rotate3D", "(OO)", off, ang), *v = rot ? Script_Ask("SVec", "Add", "(OO)", pos, rot) : NULL;
	Py_XDECREF(rot);
	if (v) {
		Script_SetValue(PyString_AsString(name), v);
	}
}

static const scriptStep_t level_steps[] = {
	{ "vecrot", Level_StepVecRot },
	{ NULL, NULL },
};

/*
 * a function of a level module: self is (file index, section, parameter names, defaults dict); the arguments bound
 * like those of a Python function (the error texts of ceval), then the section runs with them as $names
 */
static PyObject *Level_Call(PyObject *self, PyObject *args, PyObject *kw) {
	int file = (int)PyInt_AsLong(PyTuple_GetItem(self, 0));
	PyObject *section = PyTuple_GetItem(self, 1), *params = PyTuple_GetItem(self, 2), *defs = PyTuple_GetItem(self, 3);
	int np = PyTuple_Size(params), na = PyTuple_Size(args);
	if (np == 0 && (na > 0 || (kw && PyDict_Size(kw) > 0))) {
		PyErr_SetString(PyExc_TypeError, "no arguments expected");
		return NULL;
	}
	if (na > np) {
		char msg[0x80];
		Str_Printf(msg, sizeof(msg), "too many arguments; expected %d, got %d", np, na);
		PyErr_SetString(PyExc_TypeError, msg);
		return NULL;
	}
	PyObject *vars = PyDict_New();
	if (!vars) {
		return NULL;
	}
	for (int i = 0; i < na; i++) {
		PyDict_SetItem(vars, PyTuple_GetItem(params, i), PyTuple_GetItem(args, i));
	}
	if (kw) {
		PyObject *key, *value;
		int pos = 0;
		while (PyDict_Next(kw, &pos, &key, &value)) {
			int known = 0;
			for (int i = 0; i < np && !known; i++) {
				known = PyObject_Compare(key, PyTuple_GetItem(params, i)) == 0;
			}
			const char *err = !known ? "unexpected keyword argument: %.400s" : PyDict_GetItem(vars, key) ? "keyword parameter redefined" : NULL;
			if (err) {
				char msg[0x80];
				Str_Printf(msg, sizeof(msg), err, PyString_Check(key) ? PyString_AsString(key) : "?");
				PyErr_SetString(PyExc_TypeError, msg);
				Py_DECREF(vars);
				return NULL;
			}
			PyDict_SetItem(vars, key, value);
		}
	}
	for (int i = 0; i < np; i++) {
		PyObject *p = PyTuple_GetItem(params, i);
		if (PyDict_GetItem(vars, p)) {
			continue;
		}
		PyObject *d = PyDict_GetItem(defs, p);
		if (!d) {
			char msg[0x80];
			Str_Printf(msg, sizeof(msg), "not enough arguments; expected %d, got %d", np - PyDict_Size(defs), i);
			PyErr_SetString(PyExc_TypeError, msg);
			Py_DECREF(vars);
			return NULL;
		}
		PyDict_SetItem(vars, p, d);
	}
	const levelFile_t *lf = &level_files[file];
	PyObject *mod = PyString_FromString((char *)lf->module);
	if (mod) {
		PyDict_SetItemString(vars, "__module__", mod);
		Py_DECREF(mod);
	}
	byte ok = Script_RunFileTry(0, lf->name, PyString_AsString(section), lf->text, level_steps, vars);
	Py_DECREF(vars);
	return ok ? Py_ReturnNone() : NULL;
}

/* the value of a default parameter of a section line, as the .def parser reads a value (a new reference) */
static PyObject *Level_Default(const char *text) {
	char buf[LEVEL_NAME + 8];
	Str_Printf(buf, sizeof(buf), "(%s,)", text);
	PyObject *t = Script_Eval(buf);
	PyObject *v = t && PyTuple_Check(t) && PyTuple_Size(t) == 1 ? PyTuple_GetItem(t, 0) : NULL;
	Py_XINCREF(v);
	Py_XDECREF(t);
	return v;
}

/* the sections of file i into functions of the module (only those the script has, under their names) */
static void Level_Install(int i, PyObject *module, levelPool_t *pool, PyObject *kept) {
	const char *p = level_files[i].text;
	PyObject *dict = PyModule_GetDict(module);
	int n = 0;
	while (*p && n < LEVEL_FUNCS) {
		const char *eol = strchr(p, '\n');
		int len = eol ? (int)(eol - p) : (int)strlen(p);
		if (len > 8 && !strncmp(p, "section ", 8)) {
			char line[0x200], *tok[24];
			int nt = 0;
			Str_Copy(line, p + 8, len - 8 + 1 < (int)sizeof(line) ? len - 8 + 1 : (int)sizeof(line));
			for (char *s = strtok(line, " \t\r"); s && nt < 24; s = strtok(NULL, " \t\r")) {
				tok[nt++] = s;
			}
			PyObject *old = nt ? PyDict_GetItemString(dict, tok[0]) : NULL;
			if (old && PyFunction_Check(old)) {
				PyObject *params = PyTuple_New(nt - 1), *defs = PyDict_New();
				for (int k = 1; params && defs && k < nt; k++) {
					char *eq = strchr(tok[k], '=');
					if (eq) {
						*eq = 0;
						PyObject *d = Level_Default(eq + 1);
						if (d) {
							PyDict_SetItemString(defs, tok[k], d);
							Py_DECREF(d);
						}
					}
					PyTuple_SET_ITEM(params, k - 1, PyString_FromString(tok[k]));
				}
				PyObject *self = params && defs ? Py_BuildValue("(isOO)", i, tok[0], params, defs) : NULL;
				Py_XDECREF(params);
				Py_XDECREF(defs);
				Str_Copy(pool->names[n], tok[0], LEVEL_NAME);
				PyMethodDef *def = &pool->defs[n];
				def->ml_name = pool->names[n];
				def->ml_meth = (PyCFunction)(void (*)(void))Level_Call;
				def->ml_flags = METH_VARARGS | METH_KEYWORDS;
				def->ml_doc = NULL;
				PyObject *f = self ? PyCFunction_New(def, self) : NULL;
				Py_XDECREF(self);
				if (f) {
					if (kept) {
						PyList_Append(kept, old);
					}
					PyDict_SetItemString(dict, tok[0], f);
					Py_DECREF(f);
					n++;
				}
			}
		}
		p += len + (eol != NULL);
	}
	PyErr_Clear();
	Con_Verbosef("Script: %d functions of %s are data in %s\n", n, level_files[i].module, level_files[i].name);
}

void Script_LevelModuleLoaded(const char *name, PyObject *module, PyObject *kept) {
	char level[LEVEL_NAME];
	if (!Level_Current(level, sizeof(level))) {
		return;
	}
	for (size_t i = 0; i < sizeof(level_files) / sizeof(level_files[0]); i++) {
		if (!strcmp(level_files[i].level, level) && !strcmp(level_files[i].module, name)) {
			levelPool_t *pool = Level_Pool(name);
			if (pool) {
				Level_Install((int)i, module, pool, kept);
			}
			break;
		}
	}
	for (const levelModule_t *lm = level_modules; lm->level; lm++) {
		if (!strcmp(lm->level, level) && !strcmp(lm->module, name)) {
			Script_Install(module, lm->methods, kept);
		}
	}
}
