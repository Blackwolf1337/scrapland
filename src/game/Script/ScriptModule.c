/*
 * Pack modules made in C (the way to a build without Python, stage 0). When the engine would load a pack module
 * (Pack_PreloadLibrary) and the module has a definition, the module is made here and its .pyc is not read: the module
 * object, its variables with the values and types of the script, its functions (the C tables and the level data,
 * as Script_ModuleLoaded would put them in), its classes and its load time calls, in the order of the script. Every
 * other module, the C ports and the engine then see the same module.
 *
 * A definition is a .mod file: data/modules/<module>.mod, or data/levels/<level>/<module>.mod for a module of a level
 * (a level file wins), built in (ModuleFiles.inc, made by tools/modules/gen.py from the .pyc of the packs with
 * tools/modules/pyc2mod.py). A pack in mods/ with scrapc/modules/<module>.mod or scrapc/levels/<level>/<module>.mod
 * replaces it. One statement per line, # starts a comment line, a line goes on while a bracket is open:
 *	import A B ...			the modules A, B under their names (IMPORT_NAME, STORE_NAME)
 *	from M import a b | *		names of the module M (IMPORT_FROM)
 *	def f g ...			the functions: a level table (level_modules) wins over script_modules, which
 *					wins over the data of the level file; none is an error
 *	let x = <expr>			a variable (STORE_NAME)
 *	set <target>.a = <expr>		an attribute or item of another object (STORE_ATTR, STORE_SUBSCR), the value
 *	set <target>[k] = <expr>	first, as in ceval
 *	call <expr>			an expression whose value is dropped (a load time call)
 *	del x				(DELETE_NAME)
 *	if <expr> / else / end		the lines between only when the value is true
 *	class C[(B, ...)] ... end	a class, the lines between make its dict (a method through a C hook)
 *	c <name>			a C hook (module_hooks) for what the lines cannot say: a for loop of the
 *					module code (Script_ModuleFor), a method of a class
 * An <expr> is a Python expression as the module code had it: numbers (parsenumber: ints through strtol or strtoul,
 * L longs, floats through atof as marshal read them), strings in quotes with the escapes of parsestr, None, tuple,
 * list and dict displays (a dict value before its key, as com_dictmaker), names (the locals, the module, the
 * builtins), attributes, calls with keywords, subscripts and slices, + - * / %, comparisons, not, and, or. Equal
 * constants of one code object (the module, a class body, an eval) are one object and a constant that looks like a
 * name is interned (com_addconst, PyCode_New). eval(s) of the definitions (the save variables) is read by the same
 * parser in the namespace of the module; what it cannot read goes to the eval of Python (logged as "evalpy" by the
 * gate, SCRAP_PY_GATE).
 *
 * Off (the .pyc runs as before) with SCRAP_PACK_SCRIPTS=1 or SCRAP_CMOD=0.
 */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/filepak.h"
#include "scrap/script.h"

typedef struct {
	const char *level;		/* the level folder in lower case, NULL for a module of every level */
	const char *module;		/* the module name: "Sound" */
	const char *name;		/* the file for the packs (scrapc/<name>): "modules/sound.mod" */
	const char *text;		/* the built-in copy */
} moduleFile_t;

#include "ModuleFiles.inc"

typedef int (*moduleHook_t)(PyObject *module, PyObject *locals);

/* the C hooks of the definitions ("c <name>"): module, name, function (0 with an error set when it fails) */
static const struct {
	const char *module;
	const char *name;
	moduleHook_t fn;
} module_hooks[] = {
	{ "Init", "LoadChars", Init_ModLoadChars },
	{ "Init", "ExtraExecs", Init_ModExtraExecs },
	{ "NewsPanel", "Settings", NewsPanel_ModSettings },
	{ "CharsNPC", "SpawnPosInit", CharsNPC_ModSpawnPosInit },
	{ "CharsNPC", "SpawnPosClass", CharsNPC_ModSpawnPosClass },
	{ NULL, NULL, NULL },
};

PyObject *Script_ModuleName(PyObject *locals, const char *text) {
	PyObject *v;
	int found = Script_NameChain(locals, locals, text, &v);
	if (found == 0) {
		PyErr_Format(PyExc_SystemError, "module code: cannot load %.100s", text);
	}
	return found > 0 ? v : NULL;
}

byte Script_ModuleFor(PyObject *locals, const char *var, PyObject *seq, scriptForBody_t body, void *ctx) {
	PyObject *key = PyString_InternFromString((char *)var);
	byte ok = key != NULL;
	for (int i = 0; ok; i++) {
		PyObject *item;
		int n = Script_Next(seq, i, &item);
		if (n <= 0) {
			ok = n == 0;
			break;
		}
		ok = PyDict_SetItem(locals, key, item) == 0 && body(locals, item, ctx);
		Py_DECREF(item);
	}
	Py_XDECREF(key);
	return ok;
}

enum { MOD_NAME = 0x80, MOD_DEPTH = 64 };

/* Py_CLEAR of later Pythons */
#define MOD_CLEAR(o) do { PyObject *mod_tmp = (PyObject *)(o); (o) = NULL; Py_XDECREF(mod_tmp); } while (0)

typedef struct {
	const char *p;			/* where the parser is */
	const char *text;		/* the whole text, for the line of an error */
	const char *file;		/* the definition (or "eval") */
	const char *module;		/* the module name */
	PyObject *mod;			/* the module */
	PyObject *globals;		/* its dict */
	PyObject *locals;		/* the dict the names go to: the module's or a class body's */
	PyObject *consts;		/* the constants of this code object, a list */
	int depth;			/* open brackets: newlines are blanks inside them */
	int skip;			/* > 0: parse only (the branch not taken, the right side of and/or) */
	int nest;			/* expressions inside each other */
	byte syntax;			/* the error is one of the text, not of a value */
	char lastDef[MOD_NAME];		/* the last function the module code defined (the touch after the code) */
} modParse_t;

/* ---- errors ---- */

static int Mod_Line(modParse_t *m) {
	int line = 1;
	for (const char *s = m->text; s < m->p && *s; s++) {
		line += *s == '\n';
	}
	return line;
}

static PyObject *Mod_Syntax(modParse_t *m, const char *what) {
	if (!PyErr_Occurred()) {
		char near[24];
		int n = 0;
		while (n < (int)sizeof(near) - 1 && m->p[n] && m->p[n] != '\n') {
			near[n] = m->p[n];
			n++;
		}
		near[n] = 0;
		PyErr_Format(PyExc_SyntaxError, "%.100s:%d: %.100s at '%.24s'", m->file, Mod_Line(m), what, near);
	}
	m->syntax = 1;
	return NULL;
}

/* ---- the scanner ---- */

static void Mod_Blanks(modParse_t *m) {
	for (;;) {
		char c = *m->p;
		if (c == ' ' || c == '\t' || c == '\r' || (m->depth > 0 && c == '\n')) {
			m->p++;
		} else if (c == '#' && m->depth > 0) {
			while (*m->p && *m->p != '\n') {
				m->p++;
			}
		} else {
			return;
		}
	}
}

static byte Mod_IsName(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9');
}

/* the word at p (a name) into out, advancing; 0 when there is none */
static byte Mod_Word(modParse_t *m, char *out, int size) {
	Mod_Blanks(m);
	const char *s = m->p;
	if (!Mod_IsName(*s) || (*s >= '0' && *s <= '9')) {
		return 0;
	}
	int n = 0;
	while (Mod_IsName(s[n])) {
		n++;
	}
	if (n >= size) {
		return 0;
	}
	memcpy(out, s, n);
	out[n] = 0;
	m->p = s + n;
	return 1;
}

/* the keyword kw follows (a whole word): advance past it */
static byte Mod_Keyword(modParse_t *m, const char *kw) {
	Mod_Blanks(m);
	size_t n = strlen(kw);
	if (strncmp(m->p, kw, n) || Mod_IsName(m->p[n])) {
		return 0;
	}
	m->p += n;
	return 1;
}

/* the operator op follows: advance past it */
static byte Mod_Op(modParse_t *m, const char *op) {
	Mod_Blanks(m);
	size_t n = strlen(op);
	if (strncmp(m->p, op, n)) {
		return 0;
	}
	m->p += n;
	return 1;
}

/* the value of a branch not taken: None, nothing is evaluated */
static PyObject *Mod_Skipped(void) {
	Py_INCREF(Py_None);
	return Py_None;
}

/* v (a new constant) or the equal constant of the same type this code object made before it (com_addconst) */
static PyObject *Mod_Const(modParse_t *m, PyObject *v) {
	if (!v || m->skip) {
		return v;
	}
	for (int i = PyList_Size(m->consts); --i >= 0;) {
		PyObject *w = PyList_GetItem(m->consts, i);
		if (v->ob_type == w->ob_type && PyObject_Compare(v, w) == 0) {
			Py_DECREF(v);
			Py_INCREF(w);
			return w;
		}
	}
	if (PyErr_Occurred() || PyList_Append(m->consts, v) != 0) {
		Py_DECREF(v);
		return NULL;
	}
	return v;
}

/* a number as parsenumber of compile.c reads it */
static PyObject *Mod_Number(modParse_t *m) {
	const char *s = m->p, *e = s;
	char buf[128];
	byte hex = s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
	while (Mod_IsName(*e) || *e == '.' ||
		(!hex && (*e == '+' || *e == '-') && e > s && (e[-1] == 'e' || e[-1] == 'E'))) {
		e++;
	}
	if (e - s >= (int)sizeof(buf)) {
		return Mod_Syntax(m, "number too long");
	}
	memcpy(buf, s, e - s);
	buf[e - s] = 0;
	m->p = e;
	char *last = buf + strlen(buf) - 1, *end;
	if (*last == 'l' || *last == 'L') {
		return Mod_Const(m, PyLong_FromString(buf, NULL, 0));
	}
	errno = 0;
	long x = buf[0] == '0' ? (long)PyOS_strtoul(buf, &end, 0) : PyOS_strtol(buf, &end, 0);
	if (*end == 0) {
		if (errno != 0) {
			PyErr_SetString(PyExc_OverflowError, "integer literal too large");
			return NULL;
		}
		return Mod_Const(m, PyInt_FromLong(x));
	}
	return Mod_Const(m, PyFloat_FromDouble(atof(buf)));
}

static int Mod_Hex(int c) {
	return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* room for two more characters in the string buffer */
static byte Mod_Room(char **buf, int len, int *cap) {
	if (len + 2 < *cap) {
		return 1;
	}
	char *b = realloc(*buf, *cap * 2);
	if (!b) {
		PyErr_NoMemory();
		return 0;
	}
	*buf = b;
	*cap *= 2;
	return 1;
}

/* one string in quotes as parsestr of compile.c reads it (no raw or triple quoted strings), appended to buf */
static byte Mod_String1(modParse_t *m, char **buf, int *len, int *cap) {
	char q = *m->p++;
	for (;;) {
		int c = (unsigned char)*m->p;
		if (c == 0 || c == '\n') {
			Mod_Syntax(m, "string without its end");
			return 0;
		}
		m->p++;
		if (c == q) {
			return 1;
		}
		if (c == '\\') {
			int e = (unsigned char)*m->p++;
			switch (e) {
			case '\n': c = -1; break;
			case '\\': case '\'': case '"': c = e; break;
			case 'b': c = '\b'; break;
			case 'f': c = '\014'; break;
			case 't': c = '\t'; break;
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 'v': c = '\013'; break;
			case 'a': c = '\007'; break;
			case 'x':
				if (Mod_Hex((unsigned char)*m->p) >= 0) {
					unsigned x = 0;
					while (Mod_Hex((unsigned char)*m->p) >= 0) {
						x = ((x << 4) & ~0xFu) + (unsigned)Mod_Hex((unsigned char)*m->p++);
					}
					c = (int)(x & 0xff);
					break;
				}
				/* fall through */
			default:
				if (e >= '0' && e <= '7') {
					c = e - '0';
					if (*m->p >= '0' && *m->p <= '7') {
						c = (c << 3) + *m->p++ - '0';
						if (*m->p >= '0' && *m->p <= '7') {
							c = (c << 3) + *m->p++ - '0';
						}
					}
					c &= 0xff;
				} else {
					/* an unknown escape stays as it is: the backslash and the character */
					if (!Mod_Room(buf, *len, cap)) {
						return 0;
					}
					(*buf)[(*len)++] = '\\';
					c = e;
				}
			}
			if (c < 0) {
				continue;
			}
		}
		if (!Mod_Room(buf, *len, cap)) {
			return 0;
		}
		(*buf)[(*len)++] = (char)c;
	}
}

/* a string, adjacent strings joined (parsestrplus) */
static PyObject *Mod_String(modParse_t *m) {
	int len = 0, cap = 256;
	char *buf = malloc(cap);
	if (!buf) {
		return PyErr_NoMemory();
	}
	do {
		if (!Mod_String1(m, &buf, &len, &cap)) {
			free(buf);
			return NULL;
		}
		Mod_Blanks(m);
	} while (*m->p == '\'' || *m->p == '"');
	PyObject *v = Mod_Const(m, Script_StrConst(buf, len));
	free(buf);
	return v;
}

/* ---- the evaluator ---- */

static PyObject *Mod_Expr(modParse_t *m);

/* LOAD_NAME: the locals, the module, the builtins; NameError with the name */
static PyObject *Mod_Load(modParse_t *m, const char *name) {
	PyObject *v = PyDict_GetItemString(m->locals, (char *)name);
	if (!v) {
		v = PyDict_GetItemString(m->globals, (char *)name);
	}
	if (!v) {
		PyObject *b = PyDict_GetItemString(m->globals, "__builtins__");
		b = b && PyModule_Check(b) ? PyModule_GetDict(b) : b;
		v = b && PyDict_Check(b) ? PyDict_GetItemString(b, (char *)name) : NULL;
	}
	if (!v) {
		PyObject *n = PyString_FromString(name);
		PyErr_SetObject(PyExc_NameError, n);
		Py_XDECREF(n);
		return NULL;
	}
	Py_INCREF(v);
	return v;
}

/* the items up to close into a new list (*comma: a comma came); for a dict the pairs, the value made first */
static PyObject *Mod_Items(modParse_t *m, char close, byte *comma) {
	PyObject *list = PyList_New(0);
	*comma = 0;
	m->depth++;
	Mod_Blanks(m);
	while (list && *m->p != close) {
		PyObject *v = Mod_Expr(m);
		if (!v || PyList_Append(list, v) != 0) {
			Py_XDECREF(v);
			Py_DECREF(list);
			m->depth--;
			return NULL;
		}
		Py_DECREF(v);
		if (Mod_Op(m, ",")) {
			*comma = 1;
			Mod_Blanks(m);
		} else if (*m->p != close) {
			Py_DECREF(list);
			m->depth--;
			return Mod_Syntax(m, "',' or a closing bracket expected");
		}
	}
	m->depth--;
	if (list) {
		m->p++;
	}
	return list;
}

static PyObject *Mod_Dict(modParse_t *m) {
	PyObject *d = PyDict_New();
	m->depth++;
	Mod_Blanks(m);
	while (d && *m->p != '}') {
		/* com_dictmaker: the value is made before the key */
		const char *key = m->p;
		m->skip++;
		PyObject *k = Mod_Expr(m);
		m->skip--;
		Py_XDECREF(k);
		if (!k || !Mod_Op(m, ":")) {
			Py_DECREF(d);
			m->depth--;
			return k ? Mod_Syntax(m, "':' expected") : NULL;
		}
		PyObject *v = Mod_Expr(m);
		const char *after = m->p;
		m->p = key;
		k = v ? Mod_Expr(m) : NULL;
		m->p = after;
		if (!k || (!m->skip && PyDict_SetItem(d, k, v) != 0)) {
			Py_XDECREF(k);
			Py_XDECREF(v);
			Py_DECREF(d);
			m->depth--;
			return NULL;
		}
		Py_DECREF(k);
		Py_DECREF(v);
		if (!Mod_Op(m, ",") && (Mod_Blanks(m), *m->p != '}')) {
			Py_DECREF(d);
			m->depth--;
			return Mod_Syntax(m, "',' or '}' expected");
		}
		Mod_Blanks(m);
	}
	m->depth--;
	if (d) {
		m->p++;
	}
	return d;
}

static PyObject *Mod_Atom(modParse_t *m) {
	Mod_Blanks(m);
	char c = *m->p, name[MOD_NAME];
	if (c == '\'' || c == '"') {
		return Mod_String(m);
	}
	if ((c >= '0' && c <= '9') || (c == '.' && m->p[1] >= '0' && m->p[1] <= '9')) {
		return Mod_Number(m);
	}
	if (c == '(' || c == '[') {
		byte comma;
		m->p++;
		PyObject *list = Mod_Items(m, c == '(' ? ')' : ']', &comma);
		if (!list || c == '[') {
			return list;
		}
		/* (x) is x, (x,) and () are tuples */
		PyObject *v = PyList_Size(list) == 1 && !comma ? PyList_GetItem(list, 0) : NULL;
		Py_XINCREF(v);
		v = v ? v : PyList_AsTuple(list);
		Py_DECREF(list);
		return v;
	}
	if (c == '{') {
		m->p++;
		return Mod_Dict(m);
	}
	if (!Mod_Word(m, name, sizeof(name))) {
		return Mod_Syntax(m, "a value expected");
	}
	if (!strcmp(name, "None")) {
		return Mod_Skipped();
	}
	return m->skip ? Mod_Skipped() : Mod_Load(m, name);
}

/* the builtin eval with one string: the definitions read the save variables through it */
static byte Mod_IsEval(PyObject *f, PyObject *args, PyObject *kw) {
	return PyCFunction_Check(f) && !strcmp(((PyCFunctionObject *)f)->m_ml->ml_name, "eval") &&
		((PyCFunctionObject *)f)->m_self == NULL && !kw && PyTuple_Size(args) == 1 &&
		PyString_Check(PyTuple_GET_ITEM(args, 0));
}

static PyObject *Mod_Eval(modParse_t *outer, PyObject *text);

static PyObject *Mod_Call(modParse_t *m, PyObject *f) {
	PyObject *args = PyList_New(0), *kw = NULL;
	m->depth++;
	Mod_Blanks(m);
	while (args && *m->p != ')') {
		const char *at = m->p;
		char name[MOD_NAME];
		PyObject *key = NULL;
		if (Mod_Word(m, name, sizeof(name)) && Mod_Op(m, "=") && *m->p != '=') {
			key = PyString_InternFromString(name);
		} else {
			m->p = at;
		}
		PyObject *v = Mod_Expr(m);
		if (v && key) {
			if (!kw) {
				kw = PyDict_New();
			}
			if (!kw || PyDict_SetItem(kw, key, v) != 0) {
				MOD_CLEAR(v);
			}
		} else if (v && PyList_Append(args, v) != 0) {
			MOD_CLEAR(v);
		}
		Py_XDECREF(key);
		if (!v) {
			MOD_CLEAR(args);
			break;
		}
		Py_DECREF(v);
		if (!Mod_Op(m, ",") && (Mod_Blanks(m), *m->p != ')')) {
			MOD_CLEAR(args);
			Mod_Syntax(m, "',' or ')' expected");
			break;
		}
		Mod_Blanks(m);
	}
	m->depth--;
	PyObject *r = NULL;
	if (args) {
		m->p++;
		if (m->skip) {
			r = Mod_Skipped();
		} else {
			PyObject *t = PyList_AsTuple(args);
			if (t && Mod_IsEval(f, t, kw)) {
				r = Mod_Eval(m, PyTuple_GET_ITEM(t, 0));
			} else if (t) {
				r = Script_CallObject(f, t, kw);
			}
			Py_XDECREF(t);
		}
	}
	Py_XDECREF(args);
	Py_XDECREF(kw);
	return r;
}

/* slice_index of ceval: an int index, else TypeError */
static byte Mod_SliceIndex(PyObject *v, int *pi) {
	if (v == NULL) {
		return 1;
	}
	if (!PyInt_Check(v)) {
		PyErr_SetString(PyExc_TypeError, "slice index must be int");
		return 0;
	}
	long x = PyInt_AsLong(v);
	*pi = x > INT_MAX ? INT_MAX : x < -INT_MAX ? -INT_MAX : (int)x;
	return 1;
}

static PyObject *Mod_Subscript(modParse_t *m, PyObject *o) {
	PyObject *lo = NULL, *hi = NULL, *r = NULL;
	byte slice = 0;
	m->depth++;
	Mod_Blanks(m);
	if (*m->p != ':') {
		lo = Mod_Expr(m);
	}
	if ((lo || *m->p == ':') && Mod_Op(m, ":")) {
		slice = 1;
		Mod_Blanks(m);
		if (*m->p != ']') {
			hi = Mod_Expr(m);
			if (!hi) {
				Py_XDECREF(lo);
				m->depth--;
				return NULL;
			}
		}
	}
	m->depth--;
	if ((!lo && !slice) || !Mod_Op(m, "]")) {
		Py_XDECREF(lo);
		Py_XDECREF(hi);
		return lo || slice ? Mod_Syntax(m, "']' expected") : NULL;
	}
	if (m->skip) {
		r = Mod_Skipped();
	} else if (slice) {
		int ilow = 0, ihigh = INT_MAX;
		if (Mod_SliceIndex(lo, &ilow) && Mod_SliceIndex(hi, &ihigh)) {
			r = PySequence_GetSlice(o, ilow, ihigh);
		}
	} else {
		r = PyObject_GetItem(o, lo);
	}
	Py_XDECREF(lo);
	Py_XDECREF(hi);
	return r;
}

/* an atom and its attributes, calls and subscripts */
static PyObject *Mod_Postfix(modParse_t *m) {
	PyObject *v = Mod_Atom(m);
	while (v) {
		Mod_Blanks(m);
		PyObject *r;
		if (*m->p == '.' && !(m->p[1] >= '0' && m->p[1] <= '9')) {
			char name[MOD_NAME];
			m->p++;
			if (!Mod_Word(m, name, sizeof(name))) {
				Py_DECREF(v);
				return Mod_Syntax(m, "a name expected after '.'");
			}
			r = m->skip ? Mod_Skipped() : Script_GetAttr(v, name);
		} else if (*m->p == '(') {
			m->p++;
			r = Mod_Call(m, v);
		} else if (*m->p == '[') {
			m->p++;
			r = Mod_Subscript(m, v);
		} else {
			return v;
		}
		Py_DECREF(v);
		v = r;
	}
	return NULL;
}

static PyObject *Mod_Unary(modParse_t *m) {
	if (Mod_Op(m, "-")) {
		PyObject *v = Mod_Unary(m), *r = v && !m->skip ? PyNumber_Negative(v) : v ? Mod_Skipped() : NULL;
		Py_XDECREF(v);
		return r;
	}
	if (Mod_Op(m, "+")) {
		PyObject *v = Mod_Unary(m), *r = v && !m->skip ? PyNumber_Positive(v) : v ? Mod_Skipped() : NULL;
		Py_XDECREF(v);
		return r;
	}
	return Mod_Postfix(m);
}

static PyObject *Mod_Binary(modParse_t *m, PyObject *a, char op, PyObject *b) {
	PyObject *r = NULL;
	if (a && b) {
		if (m->skip) {
			r = Mod_Skipped();
		} else if (op == '+') {
			r = PyNumber_Add(a, b);
		} else if (op == '-') {
			r = PyNumber_Subtract(a, b);
		} else if (op == '*') {
			r = PyNumber_Multiply(a, b);
		} else if (op == '/') {
			r = PyNumber_Divide(a, b);
		} else {
			r = PyNumber_Remainder(a, b);
		}
	}
	Py_XDECREF(a);
	Py_XDECREF(b);
	return r;
}

static PyObject *Mod_Term(modParse_t *m) {
	PyObject *v = Mod_Unary(m);
	for (;;) {
		Mod_Blanks(m);
		char op = *m->p;
		if (!v || (op != '*' && op != '/' && op != '%') || m->p[1] == '*') {
			return v;
		}
		m->p++;
		v = Mod_Binary(m, v, op, Mod_Unary(m));
	}
}

static PyObject *Mod_Sum(modParse_t *m) {
	PyObject *v = Mod_Term(m);
	for (;;) {
		Mod_Blanks(m);
		char op = *m->p;
		if (!v || (op != '+' && op != '-')) {
			return v;
		}
		m->p++;
		v = Mod_Binary(m, v, op, Mod_Term(m));
	}
}

/* a comparison as cmp_outcome of ceval makes it (an int 0 or 1, Py_False or Py_True) */
static PyObject *Mod_Compare(modParse_t *m) {
	static const char *const ops[] = { "<=", ">=", "==", "!=", "<>", "<", ">", NULL };
	PyObject *a = Mod_Sum(m);
	if (!a) {
		return NULL;
	}
	int op = -1;
	for (int i = 0; ops[i] && op < 0; i++) {
		if (Mod_Op(m, ops[i])) {
			op = i;
		}
	}
	byte not = 0, in = 0, is = 0;
	if (op < 0) {
		const char *at = m->p;
		if (Mod_Keyword(m, "not") && Mod_Keyword(m, "in")) {
			not = in = 1;
		} else {
			m->p = at;
			if (Mod_Keyword(m, "in")) {
				in = 1;
			} else if (Mod_Keyword(m, "is")) {
				is = 1;
				not = Mod_Keyword(m, "not");
			} else {
				return a;
			}
		}
	}
	PyObject *b = Mod_Sum(m);
	if (!b || m->skip) {
		Py_DECREF(a);
		return b;
	}
	int res = 0;
	if (is) {
		res = (a == b) != not;
	} else if (in) {
		res = PySequence_Contains(b, a);
		res = res < 0 ? -1 : res != not;
	} else {
		int cmp = PyObject_Compare(a, b);
		if (cmp && PyErr_Occurred()) {
			res = -1;
		} else {
			switch (op) {
			case 0: res = cmp <= 0; break;
			case 1: res = cmp >= 0; break;
			case 2: res = cmp == 0; break;
			case 3: case 4: res = cmp != 0; break;
			case 5: res = cmp < 0; break;
			default: res = cmp > 0; break;
			}
		}
	}
	Py_DECREF(a);
	Py_DECREF(b);
	if (res < 0) {
		return NULL;
	}
	PyObject *r = res ? Py_True : Py_False;
	Py_INCREF(r);
	return r;
}

static PyObject *Mod_Not(modParse_t *m) {
	if (!Mod_Keyword(m, "not")) {
		return Mod_Compare(m);
	}
	PyObject *v = Mod_Not(m);
	if (!v || m->skip) {
		return v;
	}
	int t = PyObject_IsTrue(v);
	Py_DECREF(v);
	if (t < 0) {
		return NULL;
	}
	PyObject *r = t ? Py_False : Py_True;
	Py_INCREF(r);
	return r;
}

/* a and b, a or b: the right side is made only when the left one does not decide (JUMP_IF_FALSE, JUMP_IF_TRUE) */
static PyObject *Mod_Logic(modParse_t *m, PyObject *(*side)(modParse_t *), const char *kw, int stopWhen) {
	PyObject *v = side(m);
	while (v && Mod_Keyword(m, kw)) {
		int t = m->skip ? 0 : PyObject_IsTrue(v);
		if (t < 0) {
			Py_DECREF(v);
			return NULL;
		}
		if (!m->skip && t == stopWhen) {
			m->skip++;
			PyObject *r = side(m);
			m->skip--;
			if (!r) {
				Py_DECREF(v);
				return NULL;
			}
			Py_DECREF(r);
		} else {
			Py_DECREF(v);
			v = side(m);
		}
	}
	return v;
}

static PyObject *Mod_And(modParse_t *m) {
	return Mod_Logic(m, Mod_Not, "and", 0);
}

static PyObject *Mod_Expr(modParse_t *m) {
	if (m->nest >= MOD_DEPTH) {
		return Mod_Syntax(m, "nested too deep");
	}
	m->nest++;
	PyObject *v = Mod_Logic(m, Mod_And, "or", 1);
	m->nest--;
	return v;
}

/* eval(text) in the namespace of the module (the save variables): this parser, else the eval of Python */
static PyObject *Mod_Eval(modParse_t *outer, PyObject *text) {
	const char *s = PyString_AsString(text);
	modParse_t m = { .p = s, .text = s, .file = "eval", .module = outer->module, .mod = outer->mod,
		.globals = outer->globals, .locals = outer->locals, .consts = PyList_New(0) };
	if (!m.consts) {
		return NULL;
	}
	m.depth = 1;
	PyObject *v = Mod_Expr(&m);
	Mod_Blanks(&m);
	if (v && *m.p) {
		MOD_CLEAR(v);
		Mod_Syntax(&m, "the end expected");
	}
	Py_DECREF(m.consts);
	if (!v && m.syntax) {
		PyErr_Clear();
		Py_GateNote("evalpy", outer->module, s);
		v = PyRun_String((char *)s, Py_eval_input, outer->globals, outer->locals);
	}
	return v;
}

/* ---- the statements ---- */

/* the end of the line: only blanks may follow */
static byte Mod_EndLine(modParse_t *m) {
	Mod_Blanks(m);
	if (*m->p && *m->p != '\n') {
		Mod_Syntax(m, "the end of the line expected");
		return 0;
	}
	if (*m->p) {
		m->p++;
	}
	return 1;
}

/* name = v into the locals (STORE_NAME); v is given away */
static byte Mod_Store(modParse_t *m, const char *name, PyObject *v) {
	if (!v) {
		return 0;
	}
	byte ok = 1;
	if (!m->skip) {
		PyObject *key = PyString_InternFromString(name);
		ok = key && PyDict_SetItem(m->locals, key, v) == 0;
		Py_XDECREF(key);
	}
	Py_DECREF(v);
	return ok;
}

/* the function name of the module: a level table, script_modules, the level data */
static PyObject *Mod_Function(modParse_t *m, const char *name) {
	PyObject *f = Script_LevelTableFunction(m->module, name);
	if (!f && !PyErr_Occurred()) {
		f = Script_TableFunction(m->module, name);
	}
	if (!f && !PyErr_Occurred()) {
		f = Script_LevelDataFunction(m->module, name);
	}
	if (!f && !PyErr_Occurred()) {
		PyErr_Format(PyExc_SystemError, "%.100s:%d: %.60s.%.60s has no C definition", m->file, Mod_Line(m), m->module,
			name);
	}
	return f;
}

static byte Mod_Import(modParse_t *m) {
	char name[MOD_NAME];
	while (Mod_Word(m, name, sizeof(name))) {
		PyObject *v = m->skip ? Mod_Skipped() : PyImport_ImportModuleEx(name, m->globals, m->locals, NULL);
		if (!Mod_Store(m, name, v)) {
			return 0;
		}
	}
	return Mod_EndLine(m);
}

/* from M import a b, from M import * (import_from of ceval: * takes the names without a leading _) */
static byte Mod_From(modParse_t *m) {
	char name[MOD_NAME];
	if (!Mod_Word(m, name, sizeof(name)) || !Mod_Keyword(m, "import")) {
		return Mod_Syntax(m, "from <module> import <names>"), 0;
	}
	PyObject *mod = m->skip ? NULL : PyImport_ImportModuleEx(name, m->globals, m->locals, NULL);
	if (!mod && !m->skip) {
		return 0;
	}
	PyObject *d = mod ? PyModule_GetDict(mod) : NULL;
	byte ok = 1;
	if (Mod_Op(m, "*")) {
		PyObject *k, *v;
		int pos = 0;
		while (ok && d && PyDict_Next(d, &pos, &k, &v)) {
			if (PyString_Check(k) && PyString_AsString(k)[0] != '_') {
				ok = PyDict_SetItem(m->locals, k, v) == 0;
			}
		}
	} else {
		while (ok && Mod_Word(m, name, sizeof(name))) {
			if (!d) {
				continue;
			}
			PyObject *v = PyDict_GetItemString(d, name);
			if (!v) {
				PyErr_Format(PyExc_ImportError, "cannot import name %.200s", name);
				ok = 0;
			} else {
				Py_INCREF(v);
				ok = Mod_Store(m, name, v);
			}
		}
	}
	Py_XDECREF(mod);
	return ok && Mod_EndLine(m);
}

static byte Mod_Def(modParse_t *m) {
	char name[MOD_NAME];
	while (Mod_Word(m, name, sizeof(name))) {
		if (!Mod_Store(m, name, m->skip ? Mod_Skipped() : Mod_Function(m, name))) {
			return 0;
		}
		if (!m->skip && m->locals == m->globals) {
			Str_Copy(m->lastDef, name, sizeof(m->lastDef));
		}
	}
	return Mod_EndLine(m);
}

static byte Mod_Let(modParse_t *m) {
	char name[MOD_NAME];
	if (!Mod_Word(m, name, sizeof(name)) || !Mod_Op(m, "=")) {
		return Mod_Syntax(m, "let <name> = <value>"), 0;
	}
	return Mod_Store(m, name, Mod_Expr(m)) && Mod_EndLine(m);
}

/*
 * set <target> = <value>: the value first, then the object of the target and its key (STORE_ATTR, STORE_SUBSCR); the
 * target is an expression whose last part is an attribute or a subscript
 */
static byte Mod_Set(modParse_t *m) {
	const char *target = m->p, *last = NULL;
	/* the parts of the target without making them: where the last one starts */
	m->skip++;
	PyObject *o = Mod_Atom(m);
	while (o) {
		Mod_Blanks(m);
		if (*m->p != '.' && *m->p != '(' && *m->p != '[') {
			break;
		}
		last = m->p;
		PyObject *r = *m->p == '.' ? (m->p++, Mod_Word(m, (char[MOD_NAME]){ 0 }, MOD_NAME) ? Mod_Skipped() : NULL) :
			*m->p == '(' ? (m->p++, Mod_Call(m, o)) : (m->p++, Mod_Subscript(m, o));
		Py_DECREF(o);
		o = r;
	}
	m->skip--;
	Py_XDECREF(o);
	if (!o || !last || *last == '(' || !Mod_Op(m, "=")) {
		return Mod_Syntax(m, "set <object>.<name> = <value> or set <object>[<key>] = <value>"), 0;
	}
	PyObject *v = Mod_Expr(m);
	const char *end = m->p;
	if (!v) {
		return 0;
	}
	/* the object: the target up to its last part, made now */
	char *head = malloc(last - target + 1);
	if (!head) {
		Py_DECREF(v);
		PyErr_NoMemory();
		return 0;
	}
	memcpy(head, target, last - target);
	head[last - target] = 0;
	modParse_t h = *m;
	h.p = h.text = head;
	o = Mod_Expr(&h);
	free(head);
	byte ok = 0;
	m->p = last;
	if (o && *last == '.') {
		char name[MOD_NAME];
		m->p++;
		Mod_Word(m, name, sizeof(name));
		ok = m->skip || PyObject_SetAttrString(o, name, v) == 0;
	} else if (o) {
		m->p++;
		m->depth++;
		PyObject *k = Mod_Expr(m);
		m->depth--;
		ok = k && (m->skip || PyObject_SetItem(o, k, v) == 0);
		Py_XDECREF(k);
	}
	Py_XDECREF(o);
	Py_DECREF(v);
	m->p = end;
	return ok && Mod_EndLine(m);
}

static byte Mod_Call1(modParse_t *m) {
	PyObject *v = Mod_Expr(m);
	Py_XDECREF(v);
	return v && Mod_EndLine(m);
}

static byte Mod_Del(modParse_t *m) {
	char name[MOD_NAME];
	if (!Mod_Word(m, name, sizeof(name))) {
		return Mod_Syntax(m, "del <name>"), 0;
	}
	if (!m->skip && PyDict_DelItemString(m->locals, name) != 0) {
		PyErr_Clear();
		PyObject *n = PyString_FromString(name);
		PyErr_SetObject(PyExc_NameError, n);
		Py_XDECREF(n);
		return 0;
	}
	return Mod_EndLine(m);
}

static byte Mod_Hook(modParse_t *m) {
	char name[MOD_NAME];
	if (!Mod_Word(m, name, sizeof(name)) || !Mod_EndLine(m)) {
		return Mod_Syntax(m, "c <hook>"), 0;
	}
	for (int i = 0; module_hooks[i].fn; i++) {
		if (!strcmp(module_hooks[i].module, m->module) && !strcmp(module_hooks[i].name, name)) {
			return m->skip || module_hooks[i].fn(m->mod, m->locals);
		}
	}
	PyErr_Format(PyExc_SystemError, "%.100s:%d: no C hook %.60s", m->file, Mod_Line(m), name);
	return 0;
}

static int Mod_Block(modParse_t *m, char *stop, int size);

/* if <expr> ... [else ...] end */
static byte Mod_If(modParse_t *m) {
	PyObject *v = Mod_Expr(m);
	if (!v || !Mod_EndLine(m)) {
		Py_XDECREF(v);
		return 0;
	}
	int t = m->skip ? 0 : PyObject_IsTrue(v);
	Py_DECREF(v);
	if (t < 0) {
		return 0;
	}
	char stop[16];
	int skipped = !t;
	m->skip += skipped;
	int r = Mod_Block(m, stop, sizeof(stop));
	m->skip -= skipped;
	if (r && !strcmp(stop, "else")) {
		skipped = !!t;
		m->skip += skipped;
		r = Mod_Block(m, stop, sizeof(stop));
		m->skip -= skipped;
	}
	if (r && strcmp(stop, "end")) {
		return Mod_Syntax(m, "end expected"), 0;
	}
	return r;
}

/* class C[(B, ...)] ... end: the bases, the body into a new dict (its own constants), PyClass_New as build_class */
static byte Mod_Class(modParse_t *m) {
	char name[MOD_NAME];
	if (!Mod_Word(m, name, sizeof(name))) {
		return Mod_Syntax(m, "class <name>"), 0;
	}
	PyObject *bases = NULL;
	Mod_Blanks(m);
	if (*m->p == '(') {
		byte comma;
		m->p++;
		PyObject *list = Mod_Items(m, ')', &comma);
		bases = list ? PyList_AsTuple(list) : NULL;
		Py_XDECREF(list);
	} else {
		bases = PyTuple_New(0);
	}
	if (!bases || !Mod_EndLine(m)) {
		Py_XDECREF(bases);
		return 0;
	}
	modParse_t body = *m;
	body.locals = PyDict_New();
	body.consts = PyList_New(0);
	char stop[16];
	int r = body.locals && body.consts && Mod_Block(&body, stop, sizeof(stop));
	m->p = body.p;
	if (r && strcmp(stop, "end")) {
		r = 0;
		Mod_Syntax(m, "end expected");
	}
	PyObject *cls = NULL;
	if (r && !m->skip) {
		for (int i = 0; i < PyTuple_Size(bases); i++) {
			if (!PyClass_Check(PyTuple_GET_ITEM(bases, i))) {
				PyErr_SetString(PyExc_TypeError, "base is not a class object");
			}
		}
		/* PyClass_New takes __module__ from the globals of the frame that runs, which is not this module's here */
		PyObject *modname = PyDict_GetItemString(m->globals, "__name__");
		if (!PyErr_Occurred() && !PyDict_GetItemString(body.locals, "__doc__")) {
			PyDict_SetItemString(body.locals, "__doc__", Py_None);
		}
		if (!PyErr_Occurred() && modname && !PyDict_GetItemString(body.locals, "__module__")) {
			PyDict_SetItemString(body.locals, "__module__", modname);
		}
		PyObject *n = PyErr_Occurred() ? NULL : PyString_InternFromString(name);
		cls = n ? PyClass_New(bases, body.locals, n) : NULL;
		Py_XDECREF(n);
		r = cls != NULL;
	}
	Py_XDECREF(body.locals);
	Py_XDECREF(body.consts);
	Py_DECREF(bases);
	return r && (m->skip || Mod_Store(m, name, cls));
}

/*
 * the statements up to else, end or the end of the text: 1, the word that stopped them in stop ("" at the end of the
 * text); 0 with an error set when one failed
 */
static int Mod_Block(modParse_t *m, char *stop, int size) {
	static const struct {
		const char *word;
		byte (*fn)(modParse_t *m);
	} stmts[] = {
		{ "import", Mod_Import }, { "from", Mod_From }, { "def", Mod_Def }, { "let", Mod_Let }, { "set", Mod_Set },
		{ "call", Mod_Call1 }, { "del", Mod_Del }, { "if", Mod_If }, { "class", Mod_Class }, { "c", Mod_Hook },
		{ NULL, NULL },
	};
	for (;;) {
		Mod_Blanks(m);
		if (*m->p == '#') {
			while (*m->p && *m->p != '\n') {
				m->p++;
			}
		}
		if (*m->p == '\n') {
			m->p++;
			continue;
		}
		if (!*m->p) {
			stop[0] = 0;
			return 1;
		}
		char word[16];
		if (!Mod_Word(m, word, sizeof(word))) {
			return Mod_Syntax(m, "a statement expected"), 0;
		}
		if (!strcmp(word, "else") || !strcmp(word, "end")) {
			Str_Copy(stop, word, size);
			return Mod_EndLine(m);
		}
		int i = 0;
		while (stmts[i].word && strcmp(stmts[i].word, word)) {
			i++;
		}
		if (!stmts[i].word) {
			return Mod_Syntax(m, "unknown statement"), 0;
		}
		if (!stmts[i].fn(m)) {
			if (!PyErr_Occurred()) {
				PyErr_Format(PyExc_SystemError, "%.100s:%d: %s failed", m->file, Mod_Line(m), word);
			}
			return 0;
		}
	}
}

/* ---- the loader ---- */

static byte Mod_On(void) {
	const char *e = getenv("SCRAP_PACK_SCRIPTS");
	if (e && *e == '1') {
		return 0;
	}
	e = getenv("SCRAP_CMOD");
	return !(e && *e == '0');
}

/* the definition of module name: the level's, else the one of every level */
static const moduleFile_t *Mod_Find(const char *name) {
	char level[MOD_NAME];
	byte inLevel = Script_LevelName(level, sizeof(level));
	const moduleFile_t *any = NULL;
	for (size_t i = 0; i < sizeof(module_files) / sizeof(module_files[0]); i++) {
		const moduleFile_t *f = &module_files[i];
		if (strcmp(f->module, name)) {
			continue;
		}
		if (f->level && inLevel && !strcmp(f->level, level)) {
			return f;
		}
		if (!f->level) {
			any = f;
		}
	}
	return any;
}

int Script_ModuleMake(const char *name, const char *file, PyObject **module) {
	*module = NULL;
	if (!Mod_On()) {
		return 0;
	}
	const moduleFile_t *mf = Mod_Find(name);
	if (!mf) {
		return 0;
	}
	/* the copy of a mod pack (scrapc/<name>) wins over the built-in one */
	char path[0x100], packName[0x40];
	int offset, size;
	char *own = NULL;
	const char *text = mf->text;
	Str_Printf(path, sizeof(path), "scrapc/%s", mf->name);
	if (Pack_GetFileInfo(path, packName, &offset, &size) && (own = malloc(size + 1)) != NULL) {
		if (Pack_ReadFile(path, own, size) == size) {
			own[size] = 0;
			text = own;
			Con_Verbosef("Script: %s from the packs\n", path);
		}
	}
	PyObject *m = PyImport_AddModule((char *)name);
	PyObject *d = m ? PyModule_GetDict(m) : NULL;
	modParse_t mp = { .p = text, .text = text, .file = mf->name, .module = name, .mod = m, .globals = d, .locals = d,
		.consts = PyList_New(0) };
	int ok = d && mp.consts;
	/* as PyImport_ExecCodeModuleEx: the builtins and __file__ before the module code */
	if (ok && !PyDict_GetItemString(d, "__builtins__")) {
		ok = PyDict_SetItemString(d, "__builtins__", PyEval_GetBuiltins()) == 0;
	}
	if (ok) {
		char fpath[0x100];
		Str_Printf(fpath, sizeof(fpath), "PACK: %s", file);
		PyObject *v = PyString_FromString(fpath);
		if (v) {
			PyDict_SetItemString(d, "__file__", v);
			Py_DECREF(v);
		}
		PyErr_Clear();
		Script_LevelModuleStart(name);
		char stop[16];
		ok = Mod_Block(&mp, stop, sizeof(stop));
		if (ok && stop[0]) {
			ok = 0;
			Mod_Syntax(&mp, "else or end without if");
		}
	}
	Py_XDECREF(mp.consts);
	free(own);
	if (!ok) {
		return -1;
	}
	/* the writes the loader makes after the module code (the functions of the C tables the script does not have, the
	 * sets that may resize the dict), as after the .pyc */
	Script_ModuleExtras(name, m);
	/*
	 * after the .pyc the loader sets every function of the module again (its C or data version); a dict whose fill
	 * reached 2/3 grows at that first store, which changes the order of its keys: one store of a function the code
	 * defined does the same (Mission_StupidMerc and Mission_DefeatCrazyGambler3 have 43 keys in a table of 64)
	 */
	if (mp.lastDef[0]) {
		PyObject *f = PyDict_GetItemString(d, mp.lastDef);
		if (f) {
			Py_INCREF(f);
			PyDict_SetItemString(d, mp.lastDef, f);
			Py_DECREF(f);
		}
		PyErr_Clear();
	}
	Py_INCREF(m);
	*module = m;
	Con_Verbosef("Script: %s made in C from %s\n", name, mf->name);
	return 1;
}
