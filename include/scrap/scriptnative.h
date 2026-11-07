/*
 * The native layer of the script ports (removal of the Python 1.5.2 object library, stage 1 of docs/objlib-plan.md).
 * A port moved to native C keeps its state in C, calls the engine's C functions and works on C values; its
 * PyMethodDef entries stay as adapters (arguments in, the C call, the result and the error out) while Python is
 * linked. The values have the repr, str, hash, dict order and literal reader of 1.5.2 (checked against the linked
 * interpreter by tools/test/litfuzz.sh).
 *
 * Transitional parts are marked OBJLIB <stage> where they are used (the stage of the plan that removes them): the
 * mirror of module state in the module dict (OBJLIB 5), the seams of engine functions a test replaces (OBJLIB 7),
 * the Python errors behind Native_Raise (OBJLIB 7), the conversions to and from Python objects (OBJLIB 9).
 */
#ifndef SCRAP_SCRIPTNATIVE_H
#define SCRAP_SCRIPTNATIVE_H

#include "scrap/entity.h"
#include "scrap/python.h"

/* ---- values ---- */

/* the longest text a native value holds in place (with its 0); a longer one is in an owned buffer */
#define NATIVE_TEXT 0x80

typedef enum {
	NV_NONE,
	NV_INT,
	NV_FLOAT,
	NV_STR,
	NV_VEC,
	NV_LONG,			/* a long int of any size, as its decimal text ("-12": a sign, no L) */
	NV_LIST,
	NV_TUPLE,
	NV_DICT,
} nativeType_t;

/*
 * A value of the scripts in C: None, an int (a C long, as the int of Python 1.5.2), a float, a text, a vector (the
 * tuple of three floats an engine call gives), a long int, a list, a tuple or a dict of values.
 *
 * A text holds any bytes (0 too): n of them, in s while n < NATIVE_TEXT, else in the owned text (s is then empty).
 * Lists and tuples own their n items. A dict is the hash table of 1.5.2 (dictobject.c): max slots (a power of 2 from
 * 4), in each the key and the value at items[2 * slot] and [2 * slot + 1], n of them used; the slots in order are the
 * order of keys(), items() and repr. Only the setters below make a value; a setter overwrites without freeing, so a
 * value that may own memory (a long text, a long int, a list, a tuple or a dict) goes through Native_Free first.
 */
typedef struct nativeVal_s {
	byte type;			/* NV_ */
	long i;
	double f;
	double v[3];
	char s[NATIVE_TEXT];
	int n;				/* NV_STR, NV_LONG: the length of the text; NV_LIST, NV_TUPLE: the items; NV_DICT: the
					 * keys */
	int max;			/* NV_LIST, NV_TUPLE: the room for items; NV_DICT: the slots */
	char *text;			/* NV_STR, NV_LONG: a text of NATIVE_TEXT or more bytes (owned), else NULL */
	struct nativeVal_s *items;	/* NV_LIST, NV_TUPLE, NV_DICT (owned), else NULL */
} nativeVal_t;

void Native_SetNone(nativeVal_t *v);
void Native_SetInt(nativeVal_t *v, long i);
void Native_SetFloat(nativeVal_t *v, double f);
void Native_SetVec(nativeVal_t *v, double x, double y, double z);
/* the text s (cut to NATIVE_TEXT - 1 characters) */
void Native_SetStr(nativeVal_t *v, const char *s);
/* the len bytes at s (any length, 0 bytes too) */
void Native_SetText(nativeVal_t *v, const char *s, int len);
/* the long int of a decimal text (an optional '-', digits without leading zeros); its value is not checked */
void Native_SetLong(nativeVal_t *v, const char *digits, int len);
/* the text of NV_STR or NV_LONG (with its 0 after n bytes), "" for other types */
const char *Native_Text(const nativeVal_t *v);
/* an empty list (NV_LIST), tuple (NV_TUPLE) or dict (NV_DICT) */
void Native_SetEmpty(nativeVal_t *v, nativeType_t type);
/* a new item at the end of the list or tuple, None: fill it with a setter */
nativeVal_t *Native_Append(nativeVal_t *list);
/* frees what the value owns; it is None after */
void Native_Free(nativeVal_t *v);
/* the value is the text */
byte Native_IsStr(const nativeVal_t *v, const char *text);
/* a == b as Python 1.5.2 compares them: numbers by value (int, long and float with the coercions of 1.5.2), texts by
 * their bytes, None only with None, lists and tuples item by item, dicts by their keys and values; values of other
 * types are never equal (a text never equals a number, a vector is a tuple of three floats) */
byte Native_Equal(const nativeVal_t *a, const nativeVal_t *b);
/* hash(v) of Python 1.5.2 (string_hash, int_hash, long_hash, float_hash, tuplehash; None by its address while Python
 * is linked): 1 with *out, 0 for a list or dict (unhashable) */
byte Native_Hash(const nativeVal_t *v, long *out);
/* d[key] = value as PyDict_SetItem does it (the table grows at the same moment, an equal key keeps the old key and
 * gets the new value): key and value are taken over (moved into the dict, or freed); 0 when the key cannot be
 * hashed (a list or dict, in a tuple too) */
byte Native_DictSet(nativeVal_t *d, nativeVal_t *key, nativeVal_t *value);
/* the value of the key in the dict, NULL when it has none */
nativeVal_t *Native_DictGet(const nativeVal_t *d, const nativeVal_t *key);
/* the next key and value of the dict in the order of 1.5.2 from *pos (0 at the start), as PyDict_Next: 0 at the end */
byte Native_DictNext(const nativeVal_t *d, int *pos, const nativeVal_t **key, const nativeVal_t **value);

/* ---- texts of values ---- */

/* a growing text: in place while it is short, then owned (Native_BufFree); never copied */
typedef struct nativeBuf_s {
	char *p;			/* the text with its 0 */
	int len, max;
	char fixed[NATIVE_TEXT * 2];
} nativeBuf_t;

void Native_BufInit(nativeBuf_t *b);
void Native_BufAdd(nativeBuf_t *b, const char *s, int len);
void Native_BufFree(nativeBuf_t *b);

typedef enum {
	NR_REPR,			/* repr() and `v` of 1.5.2 */
	NR_STR,				/* str() of 1.5.2: a text as it is at the top, the rest as repr */
	NR_CHECK,			/* check() of the golden test (tools/test/golden/sdlib.py fmt, the channel's check): floats
					 * with 17 digits ("nan" for every NaN), tuples walked without the comma of a 1-tuple, the
					 * rest as repr */
} nativeRepr_t;

/* the text of the value added to b. Floats in repr and str have 12 digits as PyFloat_AsString writes them (".0"
 * after an integral number, inf and nan as the C library prints them), texts the quotes and escapes of string_repr,
 * long ints an L, dicts their keys in the slot order of 1.5.2 */
void Native_Repr(nativeBuf_t *b, const nativeVal_t *v, nativeRepr_t mode);
/* what eval of s gives when s is a plain literal of the forms Script_Literal (ScriptValue.c) reads: ints, longs and
 * floats in decimal (exponents too), texts in single or double quotes with the escapes of parsestr, None, lists,
 * tuples and dicts of those, '-' before a number, blanks between, a top level tuple without brackets. 1 with *out
 * (Native_Free it), else 0 without an error and *out None (any other text: names, octal and hex numbers, raw, triple
 * quoted or adjacent texts, control characters, newlines, comments, nesting deeper than 32, unhashable keys, a
 * leading blank). None is the constant: no script binds the name None */
byte Native_Literal(const char *s, nativeVal_t *out);

/* ---- errors ---- */

/* the kinds of error a native function raises; while Python is linked each is the Python exception of that name */
typedef enum {
	NE_INDEX,
	NE_VALUE,
	NE_TYPE,
	NE_ATTRIBUTE,
	NE_SYSTEM,
} nativeError_t;

/* raises the error kind with the text (printf form); 0, for `return Native_Raise(...)` in a function that returns
 * 1 for success. OBJLIB 7: the Python exception now, a C error state after Python */
byte Native_Raise(nativeError_t kind, const char *fmt, ...);
/* i as an index of a list of count items as Python indexes it (a negative i counts from the end): 1 with *out, else
 * 0 with the IndexError of a list */
byte Native_Index(long i, int count, int *out);
/* int(text) of Python 1.5.2 on a text (blanks around it, a sign, decimal digits): 1 with *out, else 0 with its
 * ValueError ("invalid literal for int(): ...", "int() literal too large: ...") */
byte Native_IntOfText(const char *text, long *out);

/* ---- the engine ---- */

/* entity.name as the script handle reads it: an int, float, vector or text variable (a text variable without a text
 * is None). 0 when the entity has no such variable: the console line of the handle and its AttributeError */
byte Native_EntVar(cEntity *e, const char *ref, const char *name, nativeVal_t *out);

/*
 * OBJLIB 7, the seam of an engine function that a test case replaces (fc-chat records SNet.SendChatString and
 * swaps Scrap.GetNetFlags): 0 when module.name is still the engine's own C function (the port calls the C directly),
 * 1 when something else is there (*hook a new reference: the port calls it as the script did), -1 with the error of
 * the lookup set
 */
int Native_Seam(const char *module, const char *name, PyObject **hook);
/* Scrap.GetNetFlags()[i] (0 client, 1 server, 2 dedicated): 1 or 0, -1 with the error set (through the seam) */
int Native_NetFlag(int i);

/* ---- module state mirrored into the module dict (OBJLIB 5) ---- */

/*
 * A variable of a module kept in C. While other code (the module code of the .mod file, the scripts' readers of
 * other modules, the test cases) still reads and writes the module dict, the dict keeps the value between calls:
 * Native_Import at the start of every entry point of the module takes the dict values into C, Native_Put writes a
 * C change through to the dict at once (a list is changed in place, so references to it see the change).
 */
typedef struct nativeVar_s {
	const char *name;		/* the variable of the module */
	nativeVal_t *values;		/* the C value, or the count items of a list */
	int count;			/* 0: a plain variable; else a list of exactly count items */
	unsigned types;			/* the NV_ types the C takes (1 << NV_STR | ...) */
} nativeVar_t;

/* the variables of the table (ends with a NULL name) from the dict of module into C: 0 with the error set when one
 * is missing or of a form the C does not take (TypeError naming it) */
byte Native_Import(const char *module, const nativeVar_t *vars);
/* the C value of var (index: the item of a list, ignored for a plain variable) into the dict of module: 0 with the
 * error set */
byte Native_Put(const char *module, const nativeVar_t *var, int index);

/* the value as a new Python object (NULL with the error set): for adapters and mirrors only. A dict gets its keys in
 * slot order; Python's table can order them otherwise when two keys collide (OBJLIB 6: the literal reader of the
 * saves keeps Script_Literal until then) */
PyObject *Native_ToPy(const nativeVal_t *v);
/* a Python value into *v when its type is one of types (NV_ mask of None, int, float, a text shorter than
 * NATIVE_TEXT without 0 bytes, a 3-tuple of numbers as a vector): 1, else 0 without an error */
byte Native_FromPy(PyObject *o, unsigned types, nativeVal_t *v);
/* any value of the native types (None, int, long, float, text, and lists, tuples and dicts of them, 32 deep; a dict
 * in the slots Python has it in) into *v: 1 (Native_Free it), else 0 without an error and *v None */
byte Native_FromPyAny(PyObject *o, nativeVal_t *v);

/* ---- the test channel ---- */

/* the answer of the channel's command "native <arg>" (src/game/Global/TestNative.c) added to out */
void TestNative_Command(nativeBuf_t *out, const char *arg);

#endif
