/*
 * Pack modules made in C (the way to a build without Python, stage 0). When the engine would load a pack module
 * (Pack_PreloadLibrary) and the module has a definition, the module is made here and its .pyc is not read: the module
 * object, its variables with the values and types of the script, its functions (the C tables and the level data,
 * in the order the old loader put them in), its classes and its load time calls, in the order of the script. Every
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
 * builtins), attributes, calls with keywords, subscripts and slices, + - * / %, comparisons, not, and, or. The values
 * are native (scrap/scriptnative.h, see "the values" below): a call of a port or engine function runs its entry, and
 * the value becomes a Python object where it goes into the module dict or into Python. Equal constants of one code
 * object (the module, a class body) are one object there and a text that looks like a name is interned (com_addconst,
 * PyCode_New). eval(s) of the definitions (the save variables) reads a plain literal with the reader of the saves,
 * anything else with the same parser in the namespace of the module; what that cannot read goes to Script_EvalValue
 * (the literals of builtin eval, else its SyntaxError, logged as "notread" by the gate, SCRAP_PY_GATE). No Python
 * runs.
 *
 * Off (the .pyc runs as before) with SCRAP_PACK_SCRIPTS=1 or SCRAP_CMOD=0.
 */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/filepak.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

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
	{ "MapTreasure", "Coins", MapTreasure_ModCoins },
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
	byte ok = 1;
	for (int i = 0; ok; i++) {
		PyObject *item;
		int n = Script_Next(seq, i, &item);
		if (n <= 0) {
			ok = n == 0;
			break;
		}
		ok = ModVar_DictSet(locals, var, item) == 0 && body(locals, item, ctx);
		Py_DECREF(item);
	}
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
	PyObject *defs;			/* the functions the module code defined, by name (Script_ModuleExtras) */
	modVarRef_t ref;		/* the module in the registry of the module variables (ModVar_Making): its globals */
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

/* the value of a branch not taken for the statements that make Python objects (import, def): None */
static PyObject *Mod_Skipped(void) {
	Py_INCREF(Py_None);
	return Py_None;
}

/* ---- the values ---- */

/*
 * The expressions are native values (scrap/scriptnative.h): the numbers, texts, None, tuples, lists and dicts of the
 * text are made in C, and the operators, comparisons, subscripts and slices of plain values run in C. What a name,
 * an attribute or a call of Python gives is taken as Mod_FromPy takes it: a number or a text as a native value, any
 * other object (a list or dict a module holds, a module, a function, a class, an entity) by reference as an NV_OBJ, so
 * that lists and dicts stay shared and change in place. An operation on a value that holds such an object or a dict
 * goes through Python (the same result and error). The stores and loads of the module's own names and the attributes
 * of pack modules go to the registry as native values (scrap/modvars.h, batch 9b1). A value becomes a Python object
 * where it goes into Python: the store into a class body (Mod_ToPy), the
 * call of a callable that is no port or engine function, the set of another object's attribute or item, the
 * operations through Python (Mod_ToPy). So that `is` and the objects stored stay the ones they were, a number or text
 * that came from a Python object keeps that object next to its value (nativeVal_t.obj, Mod_Keep), through copies too;
 * a value an operation made has none and becomes a new object.
 */

static PyObject *Mod_ToPy(modParse_t *m, const nativeVal_t *v);

/* plain: no NV_OBJ, no entity handle and no dict at any depth, the values the native operations take as Python takes
 * them */
static byte Mod_Plain(const nativeVal_t *v) {
	return !Native_Holds(v, NV_REFS | 1u << NV_DICT);
}

/* the number or text *v is the object o from here on (Mod_ToPy gives it back) */
static void Mod_Keep(nativeVal_t *v, PyObject *o) {
	Native_KeepPy(v, o);
}

/* o (borrowed) as the evaluator keeps it: None, an int, a float, a text or a long int as a native value (with o
 * kept), any other object by reference */
static void Mod_FromPy(PyObject *o, nativeVal_t *out) {
	if (o == Py_None) {
		Native_SetNone(out);
	} else if ((PyInt_Check(o) || PyFloat_Check(o) || PyString_Check(o) || PyLong_Check(o)) && Native_FromPyAny(o, out)) {
		Mod_Keep(out, o);
	} else {
		Native_SetObj(out, o);
	}
}

/* the result r of an operation through Python (a new reference, NULL with the error) into *out: 1, or 0 with the
 * error and *out None */
static byte Mod_Took(PyObject *r, nativeVal_t *out) {
	Native_SetNone(out);
	if (!r) {
		return 0;
	}
	Mod_FromPy(r, out);
	Py_DECREF(r);
	return 1;
}

/* the same constant: the same type (checked before) and value, a float by its bits (0.0 and -0.0, NaN) */
static byte Mod_Same(PyObject *a, PyObject *b) {
	if (PyFloat_Check(a)) {
		double x = PyFloat_AsDouble(a), y = PyFloat_AsDouble(b);
		return !memcmp(&x, &y, sizeof(x));
	}
	return PyObject_Compare(a, b) == 0;
}

/*
 * a number or a text as a Python object: the equal one of the same type this code object (the module, a class body)
 * made before it (com_addconst: equal constants are one object), else a new one, kept for the next; a text that looks
 * like a name is interned (Script_StrConst, as PyCode_New interns the constants)
 */
static PyObject *Mod_Leaf(modParse_t *m, const nativeVal_t *v) {
	PyObject *o = v->type == NV_INT ? PyInt_FromLong(v->i) : v->type == NV_FLOAT ? PyFloat_FromDouble(v->f) :
		v->type == NV_STR ? Script_StrConst(Native_Text(v), v->n) : Native_ToPy(v);
	if (!o) {
		return NULL;
	}
	for (int i = PyList_Size(m->consts); --i >= 0;) {
		PyObject *w = PyList_GetItem(m->consts, i);
		if (w->ob_type == o->ob_type && Mod_Same(w, o)) {
			Py_DECREF(o);
			Py_INCREF(w);
			return w;
		}
	}
	if (PyList_Append(m->consts, o) != 0) {
		Py_DECREF(o);
		return NULL;
	}
	return o;
}

static PyObject *Mod_ToPyConv(const nativeVal_t *v, void *m) {
	return Mod_ToPy(m, v);
}

/* the value as a Python object (a new reference, NULL with the error): the object it holds or keeps, a number or
 * text of the text through Mod_Leaf, a dict with the table it has (its keys in the order PyDict_SetItem gave the
 * display) */
static PyObject *Mod_ToPy(modParse_t *m, const nativeVal_t *v) {
	PyObject *o;
	o = Native_KeptPy(v);
	if (o) {
		Py_INCREF(o);
		return o;
	}
	switch (v->type) {
	case NV_INT:
	case NV_FLOAT:
	case NV_STR:
	case NV_LONG:
		return Mod_Leaf(m, v);
	case NV_LIST:
	case NV_TUPLE:
		o = v->type == NV_LIST ? PyList_New(v->n) : PyTuple_New(v->n);
		for (int i = 0; o && i < v->n; i++) {
			PyObject *x = Mod_ToPy(m, &v->items[i]);
			if (!x) {
				Py_DECREF(o);
				return NULL;
			}
			if (v->type == NV_LIST) {
				PyList_SET_ITEM(o, i, x);
			} else {
				PyTuple_SET_ITEM(o, i, x);
			}
		}
		return o;
	case NV_DICT:
		return Native_ToPyDict(v, Mod_ToPyConv, m);
	}
	/* None, and the vector of an engine result as its adapter makes it */
	return Native_ToPy(v);
}

/* the numbers and texts in *v that keep no object get theirs now (Mod_Leaf): the constants of an eval, which are
 * those of its own code; 0 with the error */
static byte Mod_Pin(modParse_t *m, nativeVal_t *v) {
	if (Native_KeptPy(v)) {
		return 1;
	}
	if (v->type == NV_INT || v->type == NV_FLOAT || v->type == NV_STR || v->type == NV_LONG) {
		PyObject *leaf = Mod_Leaf(m, v);
		if (leaf) {
			Native_KeepPy(v, leaf);
			Py_DECREF(leaf);
		}
		return leaf != NULL;
	}
	if (v->type == NV_LIST || v->type == NV_TUPLE) {
		for (int i = 0; i < v->n; i++) {
			if (!Mod_Pin(m, &v->items[i])) {
				return 0;
			}
		}
	} else if (v->type == NV_DICT) {
		const nativeVal_t *k, *x;
		int pos = 0;
		while (Native_DictNext(v, &pos, &k, &x)) {
			/* the slots of v itself */
			if (!Mod_Pin(m, (nativeVal_t *)k) || !Mod_Pin(m, (nativeVal_t *)x)) {
				return 0;
			}
		}
	}
	return 1;
}

/* the int 0 or 1 of a comparison or a not: Py_False or Py_True (cmp_outcome) */
static void Mod_SetBool(nativeVal_t *out, int t) {
	Native_SetInt(out, t != 0);
	Mod_Keep(out, t ? Py_True : Py_False);
}

/* `if v:` (PyObject_IsTrue for an object, which can fail): 1, 0, -1 with the error */
static int Mod_Truth(const nativeVal_t *v) {
	return Native_IsObj(v) ? PyObject_IsTrue(Native_PyOf(v)) : Native_True(v);
}

/* ---- the scanner of constants ---- */

/* a number as parsenumber of compile.c reads it */
static byte Mod_Number(modParse_t *m, nativeVal_t *out) {
	const char *s = m->p, *e = s;
	char buf[128];
	byte hex = s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
	Native_SetNone(out);
	while (Mod_IsName(*e) || *e == '.' ||
		(!hex && (*e == '+' || *e == '-') && e > s && (e[-1] == 'e' || e[-1] == 'E'))) {
		e++;
	}
	if (e - s >= (int)sizeof(buf)) {
		Mod_Syntax(m, "number too long");
		return 0;
	}
	memcpy(buf, s, e - s);
	buf[e - s] = 0;
	m->p = e;
	size_t len = strlen(buf);
	char *end;
	if (buf[len - 1] == 'l' || buf[len - 1] == 'L') {
		/* a decimal long int in C; octal, hex and the ValueError of a bad one through PyLong_FromString (OBJLIB 9) */
		if (len > 1 && (buf[0] != '0' || len == 2) && strspn(buf, "0123456789") == len - 1) {
			Native_SetLong(out, buf, (int)len - 1);
			return 1;
		}
		return Mod_Took(PyLong_FromString(buf, NULL, 0), out);
	}
	errno = 0;
	long x = buf[0] == '0' ? (long)PyOS_strtoul(buf, &end, 0) : PyOS_strtol(buf, &end, 0);
	if (*end == 0) {
		if (errno != 0) {
			PyErr_SetString(PyExc_OverflowError, "integer literal too large");
			return 0;
		}
		Native_SetInt(out, x);
		return 1;
	}
	Native_SetFloat(out, atof(buf));
	return 1;
}

static int Mod_Hex(int c) {
	return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* one string in quotes as parsestr of compile.c reads it (no raw or triple quoted strings), added to b */
static byte Mod_String1(modParse_t *m, nativeBuf_t *b) {
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
					Native_BufAdd(b, "\\", 1);
					c = e;
				}
			}
			if (c < 0) {
				continue;
			}
		}
		char ch = (char)c;
		Native_BufAdd(b, &ch, 1);
	}
}

/* a string, adjacent strings joined (parsestrplus) */
static byte Mod_String(modParse_t *m, nativeVal_t *out) {
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_SetNone(out);
	do {
		if (!Mod_String1(m, &b)) {
			Native_BufFree(&b);
			return 0;
		}
		Mod_Blanks(m);
	} while (*m->p == '\'' || *m->p == '"');
	Native_SetText(out, b.p, b.len);
	Native_BufFree(&b);
	return 1;
}

/* ---- the evaluator ---- */

/*
 * Each part of an expression makes its value into *out (None first): 1, or 0 with the error set and *out None. In a
 * branch not taken (skip) the text is read and every value is None
 */
static byte Mod_Expr(modParse_t *m, nativeVal_t *out);

/* a module variable's value as the evaluator keeps it: a tuple with its Python tuple by reference, as Mod_FromPy takes
 * it, else as it is */
static void Mod_Var(nativeVal_t *v) {
	PyObject *t = v->type == NV_TUPLE ? Native_KeptPy(v) : NULL;
	if (t) {
		Py_INCREF(t);
		Native_Free(v);
		Native_SetObj(v, t);
		Py_DECREF(t);
	}
}

/* LOAD_NAME: the locals (a class body's), the module (through the registry), the builtins; NameError with the name */
static byte Mod_Load(modParse_t *m, const char *name, nativeVal_t *out) {
	PyObject *v = m->locals != m->globals ? PyDict_GetItemString(m->locals, (char *)name) : NULL;
	Native_SetNone(out);
	if (!v && ModVar_GetVal(&m->ref, name, out)) {
		Mod_Var(out);
		return 1;
	}
	if (!v) {
		PyObject *b = ModVar_Get(&m->ref, "__builtins__");
		b = b && PyModule_Check(b) ? PyModule_GetDict(b) : b;
		v = b && PyDict_Check(b) ? PyDict_GetItemString(b, (char *)name) : NULL;
	}
	if (!v) {
		PyObject *n = PyString_FromString(name);
		PyErr_SetObject(PyExc_NameError, n);
		Py_XDECREF(n);
		return 0;
	}
	Mod_FromPy(v, out);
	return 1;
}

/* the items up to close into a new list (*comma: a comma came) */
static byte Mod_Items(modParse_t *m, char close, byte *comma, nativeVal_t *out) {
	Native_SetEmpty(out, NV_LIST);
	*comma = 0;
	m->depth++;
	Mod_Blanks(m);
	while (*m->p != close) {
		byte ok = Mod_Expr(m, Native_Append(out));
		if (ok && Mod_Op(m, ",")) {
			*comma = 1;
			Mod_Blanks(m);
		} else if (ok && *m->p != close) {
			Mod_Syntax(m, "',' or a closing bracket expected");
			ok = 0;
		}
		if (!ok) {
			Native_Free(out);
			m->depth--;
			return 0;
		}
	}
	m->depth--;
	m->p++;
	return 1;
}

/* d[k] = v of a display (k and v freed): in the native dict, or once a key is a value the native table does not take
 * as Python does (an object, an unhashable value) in the Python dict *py made from it with its table (its error) */
static byte Mod_DictPut(modParse_t *m, nativeVal_t *d, PyObject **py, nativeVal_t *k, nativeVal_t *v) {
	long h;
	if (!*py && (Native_Holds(k, NV_REFS) || !Native_Hash(k, &h))) {
		*py = Mod_ToPy(m, d);
		if (!*py) {
			return 0;
		}
	}
	if (!*py) {
		Native_DictSet(d, k, v);
		return 1;
	}
	PyObject *pk = Mod_ToPy(m, k), *pv = pk ? Mod_ToPy(m, v) : NULL;
	byte ok = pv && PyDict_SetItem(*py, pk, pv) == 0;
	Py_XDECREF(pk);
	Py_XDECREF(pv);
	return ok;
}

static byte Mod_Dict(modParse_t *m, nativeVal_t *out) {
	PyObject *py = NULL;
	byte ok = 1;
	Native_SetEmpty(out, NV_DICT);
	m->depth++;
	Mod_Blanks(m);
	while (ok && *m->p != '}') {
		/* com_dictmaker: the value is made before the key */
		const char *key = m->p;
		nativeVal_t k, v;
		m->skip++;
		ok = Mod_Expr(m, &k);
		m->skip--;
		Native_Free(&k);
		if (!ok || !Mod_Op(m, ":")) {
			if (ok) {
				Mod_Syntax(m, "':' expected");
			}
			ok = 0;
			break;
		}
		ok = Mod_Expr(m, &v);
		const char *after = m->p;
		m->p = key;
		Native_SetNone(&k);
		ok = ok && Mod_Expr(m, &k);
		m->p = after;
		if (ok && !m->skip) {
			ok = Mod_DictPut(m, out, &py, &k, &v);
		}
		Native_Free(&k);
		Native_Free(&v);
		if (ok && !Mod_Op(m, ",") && (Mod_Blanks(m), *m->p != '}')) {
			Mod_Syntax(m, "',' or '}' expected");
			ok = 0;
		}
		if (ok) {
			Mod_Blanks(m);
		}
	}
	m->depth--;
	if (!ok) {
		Native_Free(out);
		Py_XDECREF(py);
		return 0;
	}
	m->p++;
	if (py) {
		Native_Free(out);
		Native_SetObj(out, py);
		Py_DECREF(py);
	}
	return 1;
}

static byte Mod_Atom(modParse_t *m, nativeVal_t *out) {
	Mod_Blanks(m);
	char c = *m->p, name[MOD_NAME];
	Native_SetNone(out);
	if (c == '\'' || c == '"') {
		return Mod_String(m, out);
	}
	if ((c >= '0' && c <= '9') || (c == '.' && m->p[1] >= '0' && m->p[1] <= '9')) {
		return Mod_Number(m, out);
	}
	if (c == '(' || c == '[') {
		byte comma;
		m->p++;
		if (!Mod_Items(m, c == '(' ? ')' : ']', &comma, out)) {
			return 0;
		}
		if (c == '[') {
			return 1;
		}
		/* (x) is x, (x,) and () are tuples */
		if (out->n == 1 && !comma) {
			nativeVal_t x = out->items[0];
			out->n = 0;
			Native_Free(out);
			*out = x;
		} else {
			out->type = NV_TUPLE;
		}
		return 1;
	}
	if (c == '{') {
		m->p++;
		return Mod_Dict(m, out);
	}
	if (!Mod_Word(m, name, sizeof(name))) {
		Mod_Syntax(m, "a value expected");
		return 0;
	}
	if (!strcmp(name, "None") || m->skip) {
		return 1;
	}
	return Mod_Load(m, name, out);
}

/* the builtin eval with one string: the definitions read the save variables through it */
static byte Mod_IsEval(const nativeVal_t *f, const nativeVal_t *args, const nativeVal_t *kw) {
	if (!Native_IsObj(f) || kw->n || args->n != 1 || args->items[0].type != NV_STR) {
		return 0;
	}
	PyObject *o = Native_PyOf(f);
	return PyCFunction_Check(o) && !strcmp(((PyCFunctionObject *)o)->m_ml->ml_name, "eval") &&
		((PyCFunctionObject *)o)->m_self == NULL;
}

static byte Mod_Eval(modParse_t *outer, const nativeVal_t *text, nativeVal_t *out);

/*
 * f(args, keywords) when not skipped: a port or engine function (Port_Own: the value is still the port's own C
 * function, not a recorder or another value a case put there) through its entry on the native values, as its adapter
 * runs it; any other callable, a call with keywords and one with more arguments than an entry takes as before
 * through Python (Script_CallObject)
 */
static byte Mod_Invoke(modParse_t *m, const nativeVal_t *f, nativeVal_t *args, const nativeVal_t *kw, nativeVal_t *out) {
	if (Native_IsObj(f) && !kw->n && args->n <= PORT_ARGS && Port_Own(Native_PyOf(f))) {
		/* a dict goes as the Python dict with the table of its display (Native_Pass would lay it out anew) */
		for (int i = 0; i < args->n; i++) {
			if (Native_Holds(&args->items[i], 1u << NV_DICT)) {
				PyObject *o = Mod_ToPy(m, &args->items[i]);
				if (!o) {
					Native_SetNone(out);
					return 0;
				}
				Native_Free(&args->items[i]);
				Native_SetObj(&args->items[i], o);
				Py_DECREF(o);
			}
		}
		return Port_Values(PORT_VSITE(), Native_PyOf(f), args->items, args->n, out, NULL);
	}
	PyObject *pf = Mod_ToPy(m, f), *t = pf ? PyTuple_New(args->n) : NULL, *pkw = NULL;
	byte ok = t != NULL;
	for (int i = 0; ok && i < args->n; i++) {
		PyObject *x = Mod_ToPy(m, &args->items[i]);
		ok = x != NULL;
		if (ok) {
			PyTuple_SET_ITEM(t, i, x);
		}
	}
	/* the keywords in their order, a name given twice the last value */
	for (int i = 0; ok && i < kw->n; i++) {
		const nativeVal_t *pair = &kw->items[i];
		PyObject *k = PyString_InternFromString((char *)Native_Text(&pair->items[0]));
		PyObject *x = k ? Mod_ToPy(m, &pair->items[1]) : NULL;
		if (x && !pkw) {
			pkw = PyDict_New();
		}
		ok = pkw && x && PyDict_SetItem(pkw, k, x) == 0;
		Py_XDECREF(k);
		Py_XDECREF(x);
	}
	PyObject *r = ok ? Script_CallObject(pf, t, pkw) : NULL;
	Py_XDECREF(pf);
	Py_XDECREF(t);
	Py_XDECREF(pkw);
	return Mod_Took(r, out);
}

static byte Mod_Call(modParse_t *m, const nativeVal_t *f, nativeVal_t *out) {
	/* the positional values, and the keywords as (name, value) in their order */
	nativeVal_t args, kw;
	byte ok = 1;
	Native_SetNone(out);
	Native_SetEmpty(&args, NV_LIST);
	Native_SetEmpty(&kw, NV_LIST);
	m->depth++;
	Mod_Blanks(m);
	while (ok && *m->p != ')') {
		const char *at = m->p;
		char name[MOD_NAME];
		nativeVal_t *slot;
		if (Mod_Word(m, name, sizeof(name)) && Mod_Op(m, "=") && *m->p != '=') {
			nativeVal_t *pair = Native_Append(&kw);
			Native_SetEmpty(pair, NV_TUPLE);
			Native_SetStr(Native_Append(pair), name);
			slot = Native_Append(pair);
		} else {
			m->p = at;
			slot = Native_Append(&args);
		}
		ok = Mod_Expr(m, slot);
		if (ok && !Mod_Op(m, ",") && (Mod_Blanks(m), *m->p != ')')) {
			Mod_Syntax(m, "',' or ')' expected");
			ok = 0;
		}
		if (ok) {
			Mod_Blanks(m);
		}
	}
	m->depth--;
	if (ok) {
		m->p++;
		if (!m->skip) {
			ok = Mod_IsEval(f, &args, &kw) ? Mod_Eval(m, &args.items[0], out) : Mod_Invoke(m, f, &args, &kw, out);
		}
	}
	Native_Free(&args);
	Native_Free(&kw);
	return ok;
}

/* slice_index of ceval: an int index (v NULL: not given), else TypeError */
static byte Mod_SliceIndex(const nativeVal_t *v, int *pi) {
	if (v == NULL) {
		return 1;
	}
	if (v->type != NV_INT) {
		PyErr_SetString(PyExc_TypeError, "slice index must be int");
		return 0;
	}
	long x = v->i;
	*pi = x > INT_MAX ? INT_MAX : x < -INT_MAX ? -INT_MAX : (int)x;
	return 1;
}

/* o[lo:hi] (PySequence_GetSlice) and o[k] (PyObject_GetItem): in C on plain values, else through Python */
static byte Mod_Get(modParse_t *m, const nativeVal_t *o, const nativeVal_t *k, int lo, int hi, nativeVal_t *out) {
	if (Mod_Plain(o) && (!k || Mod_Plain(k))) {
		return k ? Native_Subscript(o, k, out) : Native_Slice(o, lo, hi, out);
	}
	PyObject *po = Mod_ToPy(m, o), *pk = po && k ? Mod_ToPy(m, k) : NULL;
	PyObject *r = !po || (k && !pk) ? NULL : k ? PyObject_GetItem(po, pk) : PySequence_GetSlice(po, lo, hi);
	Py_XDECREF(po);
	Py_XDECREF(pk);
	return Mod_Took(r, out);
}

static byte Mod_Subscript(modParse_t *m, const nativeVal_t *o, nativeVal_t *out) {
	nativeVal_t lo, hi;
	byte haveLo = 0, haveHi = 0, slice = 0, ok = 1;
	Native_SetNone(&lo);
	Native_SetNone(&hi);
	Native_SetNone(out);
	m->depth++;
	Mod_Blanks(m);
	if (*m->p != ':') {
		haveLo = Mod_Expr(m, &lo);
	}
	if ((haveLo || *m->p == ':') && Mod_Op(m, ":")) {
		slice = 1;
		Mod_Blanks(m);
		if (*m->p != ']') {
			haveHi = Mod_Expr(m, &hi);
			if (!haveHi) {
				Native_Free(&lo);
				m->depth--;
				return 0;
			}
		}
	}
	m->depth--;
	if ((!haveLo && !slice) || !Mod_Op(m, "]")) {
		Native_Free(&lo);
		Native_Free(&hi);
		if (haveLo || slice) {
			Mod_Syntax(m, "']' expected");
		}
		return 0;
	}
	if (m->skip) {
		/* None */
	} else if (slice) {
		int ilow = 0, ihigh = INT_MAX;
		ok = Mod_SliceIndex(haveLo ? &lo : NULL, &ilow) && Mod_SliceIndex(haveHi ? &hi : NULL, &ihigh) &&
			Mod_Get(m, o, NULL, ilow, ihigh, out);
	} else {
		ok = Mod_Get(m, o, &lo, 0, 0, out);
	}
	Native_Free(&lo);
	Native_Free(&hi);
	return ok;
}

/* o.name (Script_GetAttr; a pack module's variable in the registry) */
static byte Mod_Attr(modParse_t *m, const nativeVal_t *o, const char *name, nativeVal_t *out) {
	modVarRef_t mv;
	if (ModVar_OfVal(o, &mv)) {
		byte ok = ModVar_AttrVal(&mv, name, out);
		Mod_Var(out);
		return ok;
	}
	PyObject *po = Mod_ToPy(m, o), *r = po ? Script_GetAttr(po, name) : NULL;
	Py_XDECREF(po);
	return Mod_Took(r, out);
}

/* an atom and its attributes, calls and subscripts */
static byte Mod_Postfix(modParse_t *m, nativeVal_t *out) {
	if (!Mod_Atom(m, out)) {
		return 0;
	}
	for (;;) {
		nativeVal_t r;
		byte ok;
		Mod_Blanks(m);
		Native_SetNone(&r);
		if (*m->p == '.' && !(m->p[1] >= '0' && m->p[1] <= '9')) {
			char name[MOD_NAME];
			m->p++;
			if (!Mod_Word(m, name, sizeof(name))) {
				Native_Free(out);
				Mod_Syntax(m, "a name expected after '.'");
				return 0;
			}
			ok = m->skip || Mod_Attr(m, out, name, &r);
		} else if (*m->p == '(') {
			m->p++;
			ok = Mod_Call(m, out, &r);
		} else if (*m->p == '[') {
			m->p++;
			ok = Mod_Subscript(m, out, &r);
		} else {
			return 1;
		}
		Native_Free(out);
		*out = r;
		if (!ok) {
			return 0;
		}
	}
}

/* -v and +v: ints and floats in C; long ints, the OverflowError of -LONG_MIN and the TypeError of a value without the
 * operation through Python (PyNumber_Negative, PyNumber_Positive) */
static byte Mod_Unary(modParse_t *m, nativeVal_t *out) {
	char op = Mod_Op(m, "-") ? '-' : Mod_Op(m, "+") ? '+' : 0;
	if (!op) {
		return Mod_Postfix(m, out);
	}
	if (!Mod_Unary(m, out)) {
		return 0;
	}
	if (m->skip) {
		Native_Free(out);
	} else if (op == '+' && (out->type == NV_INT || out->type == NV_FLOAT)) {
		/* int_pos, float_pos: the value itself */
	} else if (out->type == NV_INT && out->i != LONG_MIN) {
		long i = out->i;
		Native_Free(out);
		Native_SetInt(out, -i);
	} else if (out->type == NV_FLOAT) {
		double f = out->f;
		Native_Free(out);
		Native_SetFloat(out, -f);
	} else {
		PyObject *v = Mod_ToPy(m, out), *r = !v ? NULL : op == '-' ? PyNumber_Negative(v) : PyNumber_Positive(v);
		Py_XDECREF(v);
		Native_Free(out);
		return Mod_Took(r, out);
	}
	return 1;
}

/* a op b into *a (b made: b was made without an error; both freed): Native_Arith on plain values (long ints only add
 * and subtract there), else PyNumber_Add and the others */
static byte Mod_Binary(modParse_t *m, nativeVal_t *a, char op, nativeVal_t *b, byte made) {
	nativeVal_t r;
	byte ok = made;
	Native_SetNone(&r);
	if (ok && !m->skip) {
		if (Mod_Plain(a) && Mod_Plain(b) &&
			(op == '+' || op == '-' || !(Native_Holds(a, 1u << NV_LONG) || Native_Holds(b, 1u << NV_LONG)))) {
			ok = Native_Arith(op, a, b, &r);
		} else {
			PyObject *pa = Mod_ToPy(m, a), *pb = pa ? Mod_ToPy(m, b) : NULL, *pr = NULL;
			if (pb) {
				pr = op == '+' ? PyNumber_Add(pa, pb) : op == '-' ? PyNumber_Subtract(pa, pb) :
					op == '*' ? PyNumber_Multiply(pa, pb) : op == '/' ? PyNumber_Divide(pa, pb) : PyNumber_Remainder(pa, pb);
			}
			Py_XDECREF(pa);
			Py_XDECREF(pb);
			ok = Mod_Took(pr, &r);
		}
	}
	Native_Free(a);
	Native_Free(b);
	*a = r;
	return ok;
}

static byte Mod_Term(modParse_t *m, nativeVal_t *out) {
	if (!Mod_Unary(m, out)) {
		return 0;
	}
	for (;;) {
		nativeVal_t b;
		Mod_Blanks(m);
		char op = *m->p;
		if ((op != '*' && op != '/' && op != '%') || m->p[1] == '*') {
			return 1;
		}
		m->p++;
		byte made = Mod_Unary(m, &b);
		if (!Mod_Binary(m, out, op, &b, made)) {
			return 0;
		}
	}
}

static byte Mod_Sum(modParse_t *m, nativeVal_t *out) {
	if (!Mod_Term(m, out)) {
		return 0;
	}
	for (;;) {
		nativeVal_t b;
		Mod_Blanks(m);
		char op = *m->p;
		if (op != '+' && op != '-') {
			return 1;
		}
		m->p++;
		byte made = Mod_Term(m, &b);
		if (!Mod_Binary(m, out, op, &b, made)) {
			return 0;
		}
	}
}

/* a is b (the objects themselves), a in b (PySequence_Contains), cmp(a, b) (PyObject_Compare): 1, 0, -1 with the
 * error; not is the negation of is and in. Plain values in C (Native_Contains, Native_Compare), the rest and `is`
 * on the values as Python objects */
static int Mod_Relation(modParse_t *m, const nativeVal_t *a, const nativeVal_t *b, byte is, byte in, byte not, int *cmp) {
	if (!is && Mod_Plain(a) && Mod_Plain(b)) {
		if (in) {
			int r = Native_Contains(b, a);
			return r < 0 ? -1 : r != not;
		}
		*cmp = Native_Compare(a, b);
		return 1;
	}
	PyObject *pa = Mod_ToPy(m, a), *pb = pa ? Mod_ToPy(m, b) : NULL;
	int r = -1;
	if (pb && is) {
		r = (pa == pb) != not;
	} else if (pb && in) {
		r = PySequence_Contains(pb, pa);
		r = r < 0 ? -1 : r != not;
	} else if (pb) {
		*cmp = PyObject_Compare(pa, pb);
		r = *cmp && PyErr_Occurred() ? -1 : 1;
	}
	Py_XDECREF(pa);
	Py_XDECREF(pb);
	return r;
}

/* a comparison as cmp_outcome of ceval makes it (an int 0 or 1, Py_False or Py_True) */
static byte Mod_Compare(modParse_t *m, nativeVal_t *out) {
	static const char *const ops[] = { "<=", ">=", "==", "!=", "<>", "<", ">", NULL };
	if (!Mod_Sum(m, out)) {
		return 0;
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
				return 1;
			}
		}
	}
	nativeVal_t b;
	byte made = Mod_Sum(m, &b);
	if (!made || m->skip) {
		Native_Free(out);
		*out = b;
		return made;
	}
	int cmp = 0, res = Mod_Relation(m, out, &b, is, in, not, &cmp);
	if (res > 0 && !is && !in) {
		switch (op) {
		case 0: res = cmp <= 0; break;
		case 1: res = cmp >= 0; break;
		case 2: res = cmp == 0; break;
		case 3: case 4: res = cmp != 0; break;
		case 5: res = cmp < 0; break;
		default: res = cmp > 0; break;
		}
	}
	Native_Free(out);
	Native_Free(&b);
	if (res < 0) {
		return 0;
	}
	Mod_SetBool(out, res);
	return 1;
}

static byte Mod_Not(modParse_t *m, nativeVal_t *out) {
	if (!Mod_Keyword(m, "not")) {
		return Mod_Compare(m, out);
	}
	byte ok = Mod_Not(m, out);
	if (!ok || m->skip) {
		return ok;
	}
	int t = Mod_Truth(out);
	Native_Free(out);
	if (t < 0) {
		return 0;
	}
	Mod_SetBool(out, !t);
	return 1;
}

/* a and b, a or b: the right side is made only when the left one does not decide (JUMP_IF_FALSE, JUMP_IF_TRUE) */
static byte Mod_Logic(modParse_t *m, byte (*side)(modParse_t *, nativeVal_t *), const char *kw, int stopWhen,
	nativeVal_t *out) {
	if (!side(m, out)) {
		return 0;
	}
	while (Mod_Keyword(m, kw)) {
		int t = m->skip ? 0 : Mod_Truth(out);
		if (t < 0) {
			Native_Free(out);
			return 0;
		}
		if (!m->skip && t == stopWhen) {
			nativeVal_t r;
			m->skip++;
			byte ok = side(m, &r);
			m->skip--;
			Native_Free(&r);
			if (!ok) {
				Native_Free(out);
				return 0;
			}
		} else {
			Native_Free(out);
			if (!side(m, out)) {
				return 0;
			}
		}
	}
	return 1;
}

static byte Mod_And(modParse_t *m, nativeVal_t *out) {
	return Mod_Logic(m, Mod_Not, "and", 0, out);
}

static byte Mod_Expr(modParse_t *m, nativeVal_t *out) {
	Native_SetNone(out);
	if (m->nest >= MOD_DEPTH) {
		Mod_Syntax(m, "nested too deep");
		return 0;
	}
	m->nest++;
	byte ok = Mod_Logic(m, Mod_And, "or", 1, out);
	m->nest--;
	return ok;
}

/*
 * eval(text) in the namespace of the module (the save variables): a plain literal through the reader of the saves
 * (Native_LiteralText, as Mode_EvalSave), else this parser, else the eval of Python (Script_EvalValue: the ValueError
 * of a text with a 0 byte, its literals, its SyntaxError). The constants are those of the eval's own code (Mod_Pin)
 */
static byte Mod_Eval(modParse_t *outer, const nativeVal_t *text, nativeVal_t *out) {
	const char *s = Native_Text(text);
	if ((int)strlen(s) == text->n) {
		modParse_t m = { .p = s, .text = s, .file = "eval", .module = outer->module, .mod = outer->mod,
			.globals = outer->globals, .locals = outer->locals, .consts = PyList_New(0), .ref = outer->ref };
		if (!m.consts) {
			Native_SetNone(out);
			return 0;
		}
		m.depth = 1;
		byte ok = Native_LiteralText(text, out), literal = ok;
		if (!literal) {
			ok = Mod_Expr(&m, out);
			Mod_Blanks(&m);
			if (ok && *m.p) {
				Native_Free(out);
				Mod_Syntax(&m, "the end expected");
				ok = 0;
			}
		}
		if (ok && !Mod_Pin(&m, out)) {
			Native_Free(out);
			ok = 0;
		}
		Py_DECREF(m.consts);
		if (ok || literal || !m.syntax) {
			return ok;
		}
		PyErr_Clear();
	}
	PyObject *t = Native_ToPy(text), *r = t ? Script_EvalValue(outer->globals, t) : NULL;
	Py_XDECREF(t);
	return Mod_Took(r, out);
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

/* name = v into the locals (STORE_NAME): the module's through the registry, a class body's dict (an interned key
 * either way, as the compiler's names); v is not stolen */
static byte Mod_Put(modParse_t *m, const char *name, PyObject *v) {
	return (m->locals == m->globals ? ModVar_Set(&m->ref, name, v) :
		PyDict_SetItemString(m->locals, (char *)name, v)) == 0;
}

/* the same when not skipped; v is given away */
static byte Mod_Store(modParse_t *m, const char *name, PyObject *v) {
	if (!v) {
		return 0;
	}
	byte ok = m->skip || Mod_Put(m, name, v);
	Py_DECREF(v);
	return ok;
}

/* name = v (not freed) in the locals: a native value into the module's variables, an object into a class body's dict
 * (Mod_ToPy, OBJLIB 9) */
static byte Mod_PutVal(modParse_t *m, const char *name, const nativeVal_t *v) {
	if (m->locals == m->globals && NObj_RefMode()) {
		/* the numbers and texts as the objects of this code's constants (Mod_Leaf): `is` finds them */
		nativeVal_t c;
		Native_Copy(&c, v);
		byte ok = Mod_Pin(m, &c) && ModVar_SetVal(&m->ref, name, &c) == 0;
		Native_Free(&c);
		return ok;
	}
	PyObject *o = Mod_ToPy(m, v);
	byte ok = o && Mod_Put(m, name, o);
	Py_XDECREF(o);
	return ok;
}

/* the same for a value of the evaluator when not skipped (freed) */
static byte Mod_StoreVal(modParse_t *m, const char *name, nativeVal_t *v) {
	byte ok = m->skip || Mod_PutVal(m, name, v);
	Native_Free(v);
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
		PyObject *v = m->skip ? Mod_Skipped() : Script_ModuleIn(name, m->globals);
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
	PyObject *mod = m->skip ? NULL : Script_ModuleIn(name, m->globals);
	if (!mod && !m->skip) {
		return 0;
	}
	modVarRef_t from;
	byte has = mod && ModVar_RefOf(mod, &from), ok = 1;
	nativeVal_t k, v;
	if (Mod_Op(m, "*")) {
		int pos = 0;
		/* the walk of the module's variables in the slot order of its dict (the order of the stores) */
		while (ok && has && ModVar_Next(&from, &pos, &k, &v)) {
			if (k.type == NV_STR && Native_Text(&k)[0] != '_') {
				ok = Mod_PutVal(m, Native_Text(&k), &v);
			}
			Native_Free(&k);
			Native_Free(&v);
		}
	} else {
		while (ok && Mod_Word(m, name, sizeof(name))) {
			if (!has) {
				continue;
			}
			if (!ModVar_GetVal(&from, name, &v)) {
				PyErr_Format(PyExc_ImportError, "cannot import name %.200s", name);
				ok = 0;
			} else {
				ok = m->skip || Mod_PutVal(m, name, &v);
				Native_Free(&v);
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
		if (!m->skip && m->locals == m->globals &&
			PyDict_SetItemString(m->defs, name, ModVar_Get(&m->ref, name)) != 0) {
			return 0;
		}
	}
	return Mod_EndLine(m);
}

static byte Mod_Let(modParse_t *m) {
	char name[MOD_NAME];
	nativeVal_t v;
	if (!Mod_Word(m, name, sizeof(name)) || !Mod_Op(m, "=")) {
		return Mod_Syntax(m, "let <name> = <value>"), 0;
	}
	return Mod_Expr(m, &v) && Mod_StoreVal(m, name, &v) && Mod_EndLine(m);
}

/* o.name = v (STORE_ATTR through the registry for a pack module) and o[k] = v (STORE_SUBSCR) on the objects */
static byte Mod_SetOn(modParse_t *m, const nativeVal_t *o, const char *name, const nativeVal_t *k, const nativeVal_t *v) {
	modVarRef_t mv;
	if (!k && NObj_RefMode() && ModVar_OfVal(o, &mv)) {
		return ModVar_SetAttrVal(&mv, name, v);
	}
	PyObject *pv = Mod_ToPy(m, v), *po = pv ? Mod_ToPy(m, o) : NULL, *pk = po && k ? Mod_ToPy(m, k) : NULL;
	byte ok = po && (k ? pk && PyObject_SetItem(po, pk, pv) == 0 : ModVar_SetObjAttr(po, name, pv) == 0);
	Py_XDECREF(pv);
	Py_XDECREF(po);
	Py_XDECREF(pk);
	return ok;
}

/*
 * set <target> = <value>: the value first, then the object of the target and its key (STORE_ATTR, STORE_SUBSCR); the
 * target is an expression whose last part is an attribute or a subscript
 */
static byte Mod_Set(modParse_t *m) {
	const char *target = m->p, *last = NULL;
	nativeVal_t o, v, k;
	/* the parts of the target without making them: where the last one starts */
	m->skip++;
	byte ok = Mod_Atom(m, &o);
	while (ok) {
		nativeVal_t r;
		Mod_Blanks(m);
		if (*m->p != '.' && *m->p != '(' && *m->p != '[') {
			break;
		}
		last = m->p++;
		Native_SetNone(&r);
		ok = *last == '.' ? Mod_Word(m, (char[MOD_NAME]){ 0 }, MOD_NAME) : *last == '(' ? Mod_Call(m, &o, &r) :
			Mod_Subscript(m, &o, &r);
		Native_Free(&o);
		o = r;
	}
	m->skip--;
	Native_Free(&o);
	if (!ok || !last || *last == '(' || !Mod_Op(m, "=")) {
		return Mod_Syntax(m, "set <object>.<name> = <value> or set <object>[<key>] = <value>"), 0;
	}
	if (!Mod_Expr(m, &v)) {
		return 0;
	}
	const char *end = m->p;
	/* the object: the target up to its last part, made now */
	nativeBuf_t head;
	Native_BufInit(&head);
	Native_BufAdd(&head, target, (int)(last - target));
	modParse_t h = *m;
	h.p = h.text = head.p;
	ok = Mod_Expr(&h, &o);
	Native_BufFree(&head);
	m->p = last;
	if (ok && *last == '.') {
		char name[MOD_NAME];
		m->p++;
		Mod_Word(m, name, sizeof(name));
		ok = m->skip || Mod_SetOn(m, &o, name, NULL, &v);
	} else if (ok) {
		m->p++;
		m->depth++;
		ok = Mod_Expr(m, &k);
		m->depth--;
		ok = ok && (m->skip || Mod_SetOn(m, &o, NULL, &k, &v));
		Native_Free(&k);
	}
	Native_Free(&o);
	Native_Free(&v);
	m->p = end;
	return ok && Mod_EndLine(m);
}

static byte Mod_Call1(modParse_t *m) {
	nativeVal_t v;
	byte ok = Mod_Expr(m, &v);
	Native_Free(&v);
	return ok && Mod_EndLine(m);
}

static byte Mod_Del(modParse_t *m) {
	char name[MOD_NAME];
	if (!Mod_Word(m, name, sizeof(name))) {
		return Mod_Syntax(m, "del <name>"), 0;
	}
	if (!m->skip &&
		(m->locals == m->globals ? ModVar_Del(&m->ref, name) : PyDict_DelItemString(m->locals, name)) != 0) {
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
	nativeVal_t v;
	if (!Mod_Expr(m, &v) || !Mod_EndLine(m)) {
		Native_Free(&v);
		return 0;
	}
	int t = m->skip ? 0 : Mod_Truth(&v);
	Native_Free(&v);
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
		nativeVal_t list;
		m->p++;
		if (Mod_Items(m, ')', &comma, &list)) {
			list.type = NV_TUPLE;
			bases = Mod_ToPy(m, &list);
			Native_Free(&list);
		}
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
		PyObject *modname = ModVar_Get(&m->ref, "__name__");
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

/* the lookups and makes by path (Script_ModuleCounts) */
static unsigned long mod_counts[SMC_COUNT];

static PyObject *Mod_Add(const char *name, byte *made);

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
	byte fresh;
	PyObject *m = Mod_Add(name, &fresh);
	PyObject *d = m ? PyModule_GetDict(m) : NULL;
	modParse_t mp = { .p = text, .text = text, .file = mf->name, .module = name, .mod = m, .globals = d, .locals = d,
		.consts = PyList_New(0), .defs = PyDict_New() };
	modVarRef_t was;
	int ok = d && mp.consts && mp.defs, bound = 0;
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
		/* the module code stores and loads its names through the registry */
		ModVar_Making(name, mf->level, d, &mp.ref, &was);
		bound = 1;
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
	if (ok) {
		/* the writes the loader makes after the module code (the functions of the C tables the script does not have,
		 * the sets that may resize the dict), as after the .pyc */
		Script_ModuleExtras(name, m, mp.defs);
		/* the names of the module for the registry of the module variables (a mod's own, the hooks' too) */
		ModVar_Made(name, mf->level, d);
	} else if (bound) {
		ModVar_Unmade(name, &was);
	}
	Py_XDECREF(mp.defs);
	if (!ok) {
		return -1;
	}
	mod_counts[SMC_MADE]++;
	Py_INCREF(m);
	*module = m;
	Con_Verbosef("Script: %s made in C from %s\n", name, mf->name);
	return 1;
}

/* ---- the modules by name (batch 6b) ---- */

/* sys.modules: the store of the module objects until stage 7 */
static PyObject *Mod_Modules(void) {
	return PyImport_GetModuleDict();
}

PyObject *Script_ModuleLoaded(const char *name) {
	return PyDict_GetItemString(Mod_Modules(), (char *)name);
}

PyObject *Script_Module(const char *name) {
	byte pack = ModVar_HasModule(name);
	PyObject *m = pack ? Script_ModuleLoaded(name) : NULL;
	if (!m) {
		mod_counts[pack ? SMC_ABSENT : SMC_OTHER]++;
		return PyImport_ImportModule((char *)name);
	}
	if (m == Py_None) {
		/* what load_next of the import raises for an entry None */
		mod_counts[SMC_NONE]++;
		PyErr_Format(PyExc_ImportError, "No module named %.200s", name);
		return NULL;
	}
	mod_counts[SMC_PACK]++;
	Py_INCREF(m);
	return m;
}

PyObject *Script_ModuleIn(const char *name, PyObject *globals) {
	PyObject *n = globals && PyDict_Check(globals) ? PyDict_GetItemString(globals, "__name__") : NULL;
	/* get_parent of the import: a package (__path__) or a dotted module name looks for name in the package first */
	if (n && PyString_Check(n) && (PyDict_GetItemString(globals, "__path__") || strchr(PyString_AsString(n), '.'))) {
		mod_counts[SMC_OTHER]++;
		return PyImport_ImportModuleEx((char *)name, globals, NULL, NULL);
	}
	return Script_Module(name);
}

/* PyImport_AddModule of a pack module: the module in sys.modules, a new empty one when there is none or the entry is
 * no module (borrowed); *made 1 for a new one */
static PyObject *Mod_Add(const char *name, byte *made) {
	PyObject *m = Script_ModuleLoaded(name);
	*made = !m || !PyModule_Check(m);
	if (!*made) {
		return m;
	}
	m = PyModule_New((char *)name);
	if (m && PyDict_SetItemString(Mod_Modules(), (char *)name, m) != 0) {
		Py_DECREF(m);
		return NULL;
	}
	/* the reference of sys.modules keeps it */
	Py_XDECREF(m);
	return m;
}

PyObject *Script_ModuleAdd(const char *name) {
	if (!ModVar_HasModule(name)) {
		mod_counts[SMC_OTHER]++;
		return PyImport_AddModule((char *)name);
	}
	byte made;
	PyObject *m = Mod_Add(name, &made);
	mod_counts[made ? SMC_ADDED : SMC_PACK]++;
	return m;
}

void Script_ModuleCountLoaded(void) {
	mod_counts[SMC_LOADED]++;
}

void Script_ModuleCounts(unsigned long counts[SMC_COUNT], byte reset) {
	for (int i = 0; i < SMC_COUNT; i++) {
		counts[i] = mod_counts[i];
		if (reset) {
			mod_counts[i] = 0;
		}
	}
}
