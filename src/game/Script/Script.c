/*
 * The script loader of ScrapC (include/scrap/script.h): the C functions of pack modules, the helpers they share and
 * the .menu files.
 *
 * A .menu file is a list of lines, # starts a comment:
 *	style <name>			the attribute lines that follow belong to the style, until the next command
 *	item <name> <type> [style ...]	SScorer.Add, then the attributes of the styles
 *	<Attr> <value>			SScorer.Set of the last item (or of the style being defined)
 *	use <style>			the attributes of a style onto the last item, at this point
 *	set <item> <Attr> <value>	SScorer.Set of another item
 *	lr <a> <b>, ud <a> <b>		Menu.LinkLR, Menu.LinkUD
 *	default <item>			SScorer.SetDefault
 *	call <Module.Func> [arg ...]	a function of a script; $id is the player
 *	run <step>			a step the C code of the menu does
 * A value is a number, a word or a "quoted text"; {Key} in a text is the language text Key, @Item.Var the number
 * of a variable of another item.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"

#define SCRIPT_MAX_FILE 0x10000
#define SCRIPT_MAX_TOKENS 16
#define SCRIPT_MAX_TEXT 0x200
#define SCRIPT_MAX_STYLES 32

static const scriptModule_t script_modules[] = {
	{ "PCMenu", g_MenuVideoMethods },
};

void Script_ModuleLoaded(const char *name, PyObject *module) {
	/* SCRAP_PACK_SCRIPTS=1: the scripts of the packs only (to compare a port with the original) */
	const char *only = getenv("SCRAP_PACK_SCRIPTS");
	if (only && *only == '1') {
		return;
	}
	for (size_t i = 0; i < sizeof(script_modules) / sizeof(script_modules[0]); i++) {
		const scriptModule_t *sm = &script_modules[i];
		if (strcmp(sm->name, name)) {
			continue;
		}
		PyObject *dict = PyModule_GetDict(module);
		for (PyMethodDef *m = sm->methods; m->ml_name; m++) {
			PyObject *f = PyCFunction_New(m, NULL);
			if (f) {
				PyDict_SetItemString(dict, m->ml_name, f);
				Py_DECREF(f);
			}
		}
		Con_Verbosef("Script: the C functions of %s are in place\n", name);
	}
}

/* ---- the scorer of a player ---- */

static cScorerItem *Script_Item(int id, const char *item) {
	cScorer *sc = Scorer_Get(id);
	return sc ? cScorer_FindItem(sc, item) : NULL;
}

void Script_Add(int id, const char *item, const char *type) {
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_AddItem(sc, item, type, 0);
	}
}

void Script_SetStr(int id, const char *item, const char *var, const char *s) {
	cScorerItem *it = Script_Item(id, item);
	char *v = (char *)s;
	if (it) {
		ScorerItem_Var(it, var, 1, &v, NULL, NULL);
	}
}

void Script_SetNum(int id, const char *item, const char *var, double d) {
	cScorerItem *it = Script_Item(id, item);
	int i = (int)d;
	if (it) {
		ScorerItem_Var(it, var, 1, NULL, &d, &i);
	}
}

const char *Script_GetStr(int id, const char *item, const char *var) {
	cScorerItem *it = Script_Item(id, item);
	char *s = NULL;
	double d;
	int i;
	if (it && ScorerItem_Var(it, var, 0, &s, &d, &i) == 1 && s) {
		return s;
	}
	return "";
}

double Script_GetNum(int id, const char *item, const char *var) {
	cScorerItem *it = Script_Item(id, item);
	char *s;
	double d = 0.0;
	int i = 0;
	if (!it) {
		return 0.0;
	}
	switch (ScorerItem_Var(it, var, 0, &s, &d, &i)) {
	case 2:
		return d;
	case 3:
		return i;
	}
	return 0.0;
}

void Script_SetDefault(int id, const char *item) {
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_SetDefault(sc, item);
	}
}

const char *Script_Lang(const char *key) {
	const char *s = Lang_GetStr(key);
	if (!s) {
		Con_Warnf("STRING NOT FOUND: '%s'\n", key);
		return "";
	}
	return s;
}

/* ---- the scripts ---- */

static PyObject *Script_Func(const char *module, const char *func) {
	PyObject *m = PyImport_ImportModule((char *)module);
	if (!m) {
		return NULL;
	}
	PyObject *f = PyObject_GetAttrString(m, (char *)func);
	Py_DECREF(m);
	return f;
}

byte Script_CallKw(const char *module, const char *func, PyObject *args, PyObject *kw) {
	PyObject *f = Script_Func(module, func);
	PyObject *r = NULL;
	if (f && args) {
		r = PyEval_CallObjectWithKeywords(f, args, kw);
	}
	Py_XDECREF(f);
	Py_XDECREF(args);
	Py_XDECREF(kw);
	if (!r) {
		Con_Warnf("Script: %s.%s failed\n", module, func);
		PyErr_Print();
		return 0;
	}
	Py_DECREF(r);
	return 1;
}

byte Script_Call(const char *module, const char *func, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	return Script_CallKw(module, func, args, NULL);
}

int Script_GetInt(const char *module, const char *var, int def) {
	PyObject *v = Script_Func(module, var);
	int r = def;
	if (v && PyInt_Check(v)) {
		r = (int)PyInt_AsLong(v);
	} else if (v && PyFloat_Check(v)) {
		r = (int)PyFloat_AsDouble(v);
	}
	if (!v) {
		PyErr_Clear();
	}
	Py_XDECREF(v);
	return r;
}

byte Script_Args(PyObject *args, int *id, const char **control) {
	PyObject *c = NULL;
	if (!PyArg_ParseTuple(args, "i|O", id, &c)) {
		return 0;
	}
	*control = c && PyString_Check(c) ? PyString_AsString(c) : "";
	return 1;
}

/* ---- the .menu files ---- */

typedef struct {
	char *tok[SCRIPT_MAX_TOKENS];
	int n;
} scriptLine_t;

typedef struct {
	char name[0x40];
	const char *start;		/* the first attribute line of the style */
	const char *end;
} scriptStyle_t;

typedef struct {
	int id;
	const scriptStep_t *steps;
	char item[0x80];		/* the last item */
	scriptStyle_t styles[SCRIPT_MAX_STYLES];
	int numStyles;
	const char *name;		/* the file, for the warnings */
	int line;
} scriptMenu_t;

/* the tokens of one line into buf (a word, a "quoted text", up to # outside quotes); the next line */
static const char *Menu_Tokens(const char *p, char *buf, int size, scriptLine_t *l) {
	char *o = buf;
	char *end = buf + size - 1;

	l->n = 0;
	while (*p && *p != '\n') {
		while (*p == ' ' || *p == '\t' || *p == '\r') {
			p++;
		}
		if (!*p || *p == '\n' || *p == '#') {
			break;
		}
		if (l->n < SCRIPT_MAX_TOKENS) {
			l->tok[l->n++] = o;
		}
		if (*p == '"') {
			for (p++; *p && *p != '"' && *p != '\n'; p++) {
				if (o < end) {
					*o++ = *p;
				}
			}
			if (*p == '"') {
				p++;
			}
		} else {
			for (; *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'; p++) {
				if (o < end) {
					*o++ = *p;
				}
			}
		}
		if (o < end) {
			*o++ = 0;
		}
	}
	*end = 0;
	while (*p && *p != '\n') {
		p++;
	}
	return *p ? p + 1 : p;
}

/* the text s with {Key} replaced by the language texts */
static const char *Menu_Text(const char *s, char *out, int size) {
	int n = 0;
	while (*s && n < size - 1) {
		const char *close = *s == '{' ? strchr(s, '}') : NULL;
		if (close && close - s < 0x40) {
			char key[0x40];
			Str_Copy(key, s + 1, (int)(close - s));
			for (const char *t = Script_Lang(key); *t && n < size - 1; t++) {
				out[n++] = *t;
			}
			s = close + 1;
		} else {
			out[n++] = *s++;
		}
	}
	out[n] = 0;
	return out;
}

/* a number token: 1 and its value */
static byte Menu_Number(const char *s, double *d) {
	char *e;
	if (!*s) {
		return 0;
	}
	*d = strtod(s, &e);
	return *e == 0;
}

static void Menu_Set(scriptMenu_t *m, const char *item, const char *var, const char *value) {
	double d;
	const char *dot = value[0] == '@' ? strchr(value, '.') : NULL;
	if (dot && dot - value < 0x80) {
		char from[0x80];
		Str_Copy(from, value + 1, (int)(dot - value));
		Script_SetNum(m->id, item, var, Script_GetNum(m->id, from, dot + 1));
	} else if (Menu_Number(value, &d)) {
		Script_SetNum(m->id, item, var, d);
	} else {
		char text[SCRIPT_MAX_TEXT];
		Script_SetStr(m->id, item, var, Menu_Text(value, text, sizeof(text)));
	}
}

/* the attribute lines of a style onto the item */
static void Menu_ApplyStyle(scriptMenu_t *m, const char *name) {
	for (int i = 0; i < m->numStyles; i++) {
		scriptStyle_t *st = &m->styles[i];
		if (strcmp(st->name, name)) {
			continue;
		}
		for (const char *p = st->start; p < st->end;) {
			char buf[SCRIPT_MAX_TEXT];
			scriptLine_t l;
			p = Menu_Tokens(p, buf, sizeof(buf), &l);
			if (l.n >= 2) {
				Menu_Set(m, m->item, l.tok[0], l.tok[1]);
			}
		}
		return;
	}
	Con_Warnf("%s:%d: no style %s\n", m->name, m->line, name);
}

static void Menu_DoCall(scriptMenu_t *m, scriptLine_t *l) {
	char module[0x40], text[SCRIPT_MAX_TEXT];
	const char *dot = strchr(l->tok[1], '.');
	if (!dot || dot - l->tok[1] >= (int)sizeof(module)) {
		Con_Warnf("%s:%d: call needs Module.Function\n", m->name, m->line);
		return;
	}
	Str_Copy(module, l->tok[1], (int)(dot - l->tok[1]) + 1);
	PyObject *args = PyTuple_New(l->n - 2);
	for (int i = 2; args && i < l->n; i++) {
		const char *t = l->tok[i];
		double d;
		PyObject *v;
		if (!strcmp(t, "$id")) {
			v = PyInt_FromLong(m->id);
		} else if (Menu_Number(t, &d)) {
			v = strchr(t, '.') ? PyFloat_FromDouble(d) : PyInt_FromLong((long)d);
		} else {
			v = PyString_FromString((char *)Menu_Text(t, text, sizeof(text)));
		}
		PyTuple_SetItem(args, i - 2, v);
	}
	Script_CallKw(module, dot + 1, args, NULL);
}

static void Menu_Line(scriptMenu_t *m, scriptLine_t *l, scriptStyle_t **style, const char *next) {
	const char *cmd = l->tok[0];

	if (*style && strcmp(cmd, "style") && strcmp(cmd, "item") && strcmp(cmd, "set") && strcmp(cmd, "lr") &&
		strcmp(cmd, "ud") && strcmp(cmd, "default") && strcmp(cmd, "call") && strcmp(cmd, "run") && strcmp(cmd, "use")) {
		(*style)->end = next;	/* an attribute line of the style */
		return;
	}
	*style = NULL;
	if (!strcmp(cmd, "style") && l->n == 2) {
		if (m->numStyles >= SCRIPT_MAX_STYLES) {
			Con_Warnf("%s:%d: too many styles\n", m->name, m->line);
			return;
		}
		*style = &m->styles[m->numStyles++];
		Str_Copy((*style)->name, l->tok[1], sizeof((*style)->name));
		(*style)->start = (*style)->end = next;
	} else if (!strcmp(cmd, "item") && l->n >= 3) {
		Script_Add(m->id, l->tok[1], l->tok[2]);
		Str_Copy(m->item, l->tok[1], sizeof(m->item));
		for (int i = 3; i < l->n; i++) {
			Menu_ApplyStyle(m, l->tok[i]);
		}
	} else if (!strcmp(cmd, "use") && l->n == 2 && m->item[0]) {
		Menu_ApplyStyle(m, l->tok[1]);
	} else if (!strcmp(cmd, "set") && l->n == 4) {
		Menu_Set(m, l->tok[1], l->tok[2], l->tok[3]);
	} else if ((!strcmp(cmd, "lr") || !strcmp(cmd, "ud")) && l->n == 3) {
		Script_Call("Menu", cmd[0] == 'l' ? "LinkLR" : "LinkUD", "(iss)", m->id, l->tok[1], l->tok[2]);
	} else if (!strcmp(cmd, "default") && l->n == 2) {
		Script_SetDefault(m->id, l->tok[1]);
	} else if (!strcmp(cmd, "call") && l->n >= 2) {
		Menu_DoCall(m, l);
	} else if (!strcmp(cmd, "run") && l->n == 2) {
		for (const scriptStep_t *s = m->steps; s && s->name; s++) {
			if (!strcmp(s->name, l->tok[1])) {
				s->fn(m->id);
				return;
			}
		}
		Con_Warnf("%s:%d: no step %s\n", m->name, m->line, l->tok[1]);
	} else if (l->n == 2 && m->item[0]) {
		Menu_Set(m, m->item, cmd, l->tok[1]);
	} else {
		Con_Warnf("%s:%d: what is '%s'?\n", m->name, m->line, cmd);
	}
}

byte Script_RunMenu(int id, const char *name, const char *builtin, const scriptStep_t *steps) {
	static char file[SCRIPT_MAX_FILE];
	char path[0x100];
	const char *text = builtin;

	Str_Printf(path, sizeof(path), "scrapc/%s", name);
	int size = Pack_ReadFile(path, file, sizeof(file) - 1);
	if (size >= 0) {
		file[size] = 0;
		text = file;
		Con_Verbosef("Script: %s from the packs\n", path);
	}
	if (!text) {
		Con_Warnf("Script: no %s\n", name);
		return 0;
	}
	scriptMenu_t m = { .id = id, .steps = steps, .name = name };
	scriptStyle_t *style = NULL;
	for (const char *p = text; *p;) {
		char buf[SCRIPT_MAX_TEXT];
		scriptLine_t l;
		const char *next = Menu_Tokens(p, buf, sizeof(buf), &l);
		m.line++;
		if (l.n) {
			Menu_Line(&m, &l, &style, next);
		}
		p = next;
	}
	return 1;
}
