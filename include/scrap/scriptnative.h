/*
 * The native layer of the script ports (removal of the Python 1.5.2 object library, stage 1 of docs/objlib-plan.md).
 * A port moved to native C keeps its state in C, calls the engine's C functions and works on C values; its
 * PyMethodDef entries stay as adapters (arguments in, the C call, the result and the error out) while Python is
 * linked. The values have the repr, str, hash, dict order and literal reader of 1.5.2 (checked by tools/test/litfuzz.sh
 * against the texts the interpreter gave, stored before it was removed).
 *
 * Transitional parts are marked OBJLIB <stage> where they are used (the stage of the plan that removes them): the
 * functions, classes and modules held as Python values (OBJLIB 5 (9c)), the generic calls of the seams of engine
 * functions a test replaces (OBJLIB 7), the Python errors behind Native_Raise (OBJLIB 7), the conversions to and from
 * Python objects (OBJLIB 9).
 */
#ifndef SCRAP_SCRIPTNATIVE_H
#define SCRAP_SCRIPTNATIVE_H

#include <stdarg.h>

#include "scrap/entity.h"
#include "scrap/python.h"

/* ---- values ---- */

/* the depth of the containers the literal reader and Native_FromPyAny take */
#define NATIVE_DEPTH 32

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
	NV_OBJ,				/* OBJLIB 7: a Python value of another type (a function, a class instance, a
					 * container that holds one or an entity handle), held by reference: only adapters make
					 * it (Native_Arg), the native functions refuse it with the error the script raised for
					 * it */
	NV_ENT,				/* an entity handle of the scripts (the cPyEntity of Python): a cEntityRef, the link in
					 * the ring of the entity and the name the handle was made with */
	NV_REF,				/* a native object with identity (scrap/nativeobj.h: a shared list or dict, an instance, a
					 * class, a function, a method, a module), held by reference: copies share the object */
} nativeType_t;

/* every type: a mask of nativeVar_t.types for a variable that takes any value */
#define NV_ANY (1 << NV_NONE | 1 << NV_INT | 1 << NV_FLOAT | 1 << NV_STR | 1 << NV_VEC | 1 << NV_LONG | 1 << NV_LIST | \
	1 << NV_TUPLE | 1 << NV_DICT | 1 << NV_OBJ | 1 << NV_ENT | 1 << NV_REF)
/* the kinds held by reference, with an identity of their own: a Python value, an entity handle and a native object */
#define NV_REFS (1u << NV_OBJ | 1u << NV_ENT | 1u << NV_REF)
/* the kinds that hold an object: a native object or its view (NV_REF) and a Python value of another type (NV_OBJ) */
#define NV_OBJS (1u << NV_OBJ | 1u << NV_REF)

/*
 * A value of the scripts in C: None, an int (a C long, as the int of Python 1.5.2), a float, a text, a vector (the
 * tuple of three floats an engine call gives), a long int, a list, a tuple or a dict of values, or an entity handle.
 *
 * A text holds any bytes (0 too): n of them, in s while n < NATIVE_TEXT, else in the owned text (s is then empty).
 * Lists and tuples own their n items. A dict is the hash table of 1.5.2 (dictobject.c): max slots (a power of 2 from
 * 4), in each the key and the value at items[2 * slot] and [2 * slot + 1], n of them used; the slots in order are the
 * order of keys(), items() and repr. Only the setters below make a value; a setter overwrites without freeing, so a
 * value that may own memory (a long text, a long int, a list, a tuple or a dict) goes through Native_Free first.
 *
 * An entity handle (NV_ENT) is the handle of the scripts without a Python object: the value holds a cEntityRef
 * (scrap/pyentity.h), copies share it and the last holder frees it. It behaves as the cPyEntity object did: the handle
 * of an entity that is gone stays (its entity NULL, its name kept); a handle is equal only to itself (two handles of one
 * entity are different values, two keys of a dict), hashes and orders by its address, is true, and its repr is
 * "<cPyEntity object at ...>". Native_Arg makes it from a cPyEntity, Native_ToPy gives the cPyEntity of the handle back
 * (the same object while Python holds one, else a new one on the same handle).
 *
 * A native object (NV_REF) is a list, dict, instance, class, function, method or module with an identity
 * (scrap/nativeobj.h): the value holds one reference of a nativeObj_t, copies share the object and the last holder
 * frees it. The functions here copy, free, compare, hash, print and convert it (Native_ToPy gives its Python object,
 * OBJLIB 9); its operations (attributes, items, calls, len) are the NObj_ functions of that header.
 */
struct cEntityRef;
struct nativeObj_s;

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
	PyObject *obj;			/* NV_OBJ: the value (a reference owned). A number or text may keep the Python object it
					 * came from here (the .mod evaluator, OBJLIB 5 (9c); a reference owned, copied and freed with
					 * the value, read by nobody else), else NULL */
	struct cEntityRef *ent;		/* NV_ENT: the handle (held once by this value), else NULL */
	struct nativeObj_s *ref;	/* NV_REF: the object (held once by this value), else NULL */
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
/* a new handle of the entity called name (two calls give two handles): 1, or 0 without an error and *v None when
 * there is no such entity */
byte Native_SetEnt(nativeVal_t *v, const char *name);
/* the entity of the handle v (NV_ENT), NULL when it is gone */
cEntity *Native_Ent(const nativeVal_t *v);
/* the name the handle v (NV_ENT) was made with */
const char *Native_EntName(const nativeVal_t *v);
/* what the print statement writes for a handle of the entity called name (the tp_print of cPyEntity): "Entity:
 * <name>", or the text of a handle whose entity is gone */
void Native_EntPrint(const char *name, char *buf, int size);
/* the text of NV_STR or NV_LONG (with its 0 after n bytes), "" for other types */
const char *Native_Text(const nativeVal_t *v);
/* an empty list (NV_LIST), tuple (NV_TUPLE) or dict (NV_DICT) */
void Native_SetEmpty(nativeVal_t *v, nativeType_t type);
/* a new item at the end of the list or tuple, None: fill it with a setter */
nativeVal_t *Native_Append(nativeVal_t *list);
/* *out a copy of v that owns its own memory (Native_Free it) */
void Native_Copy(nativeVal_t *out, const nativeVal_t *v);
/* frees what the value owns; it is None after */
void Native_Free(nativeVal_t *v);
/* the value is the text */
byte Native_IsStr(const nativeVal_t *v, const char *text);
/* a == b as Python 1.5.2 compares them: numbers by value (int, long and float with the coercions of 1.5.2), texts by
 * their bytes, None only with None, lists and tuples item by item, dicts by their keys and values; values of other
 * types are never equal (a text never equals a number, a vector is a tuple of three floats); an entity handle only
 * equals itself (a copy of the same handle); a native object as NObj_Equal compares it (a list object equals a list
 * by value with the same items) */
byte Native_Equal(const nativeVal_t *a, const nativeVal_t *b);
/* hash(v) of Python 1.5.2 (string_hash, int_hash, long_hash, float_hash, tuplehash; None by its address, OBJLIB 9; an
 * entity handle by the address of the handle, a native object by NObj_HashQuiet):
 * 1 with *out, 0 for a list or dict (unhashable, in a tuple too) and a long int of 3840 bits or more */
byte Native_Hash(const nativeVal_t *v, long *out);
/* d[key] = value as PyDict_SetItem does it (the table grows at the same moment, an equal key keeps the old key and
 * gets the new value): key and value are taken over (moved into the dict, or freed); 0 when the key has no hash */
byte Native_DictSet(nativeVal_t *d, nativeVal_t *key, nativeVal_t *value);
/* d.update(other) (copy 0) and the table of other.copy() into the empty dict d (copy 1), as dict_update and dict_copy
 * of dictobject.c lay the keys out: one resize at the start (for 3/2 of the keys), then every key and value of other
 * in its slot order put in without a further growth check; copies of the keys and values. other is not d */
void Native_DictMerge(nativeVal_t *d, const nativeVal_t *other, byte copy);
/* the value of the key in the dict, NULL when it has none */
nativeVal_t *Native_DictGet(const nativeVal_t *d, const nativeVal_t *key);
/* del d[key] as PyDict_DelItem does it (the slot stays as a deleted one, the order of the others stays): 0 when the
 * dict has no such key (no error set) */
byte Native_DictDel(nativeVal_t *d, const nativeVal_t *key);
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
/* len bytes of s, all up to its 0 for a negative len */
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

/* ---- operations of the scripts ---- */

/*
 * The operators and built-in conversions of Python 1.5.2 on native values, with the errors of Objects/ (the TypeError
 * of a type that does not take the operation, OverflowError, ZeroDivisionError, IndexError, KeyError). Each returns
 * 1 with *out made (Native_Free it), else 0 with the error raised. A vector is a tuple of three floats in all of
 * them. Long ints compare, convert, format, add and subtract, but do not multiply or divide (a TypeError of its own:
 * no script makes them).
 */

/* cmp(a, b) of 1.5.2 as -1, 0 or 1 (PyObject_Compare: numbers coerced, other types of one kind by their values,
 * different types by the names of the types with the numbers below all others, two NV_OBJ by Python, two entity
 * handles by their addresses). Two different
 * dicts of the same length are only told apart (1, not the order of 1.5.2) */
int Native_Compare(const nativeVal_t *a, const nativeVal_t *b);
/* a op b for op '+', '-', '*', '/', '%' (PyNumber_Add and the others, the concatenation and repetition of texts,
 * lists and tuples, the division and modulo rounding down; '%' on a text is Native_Format) */
byte Native_Arith(char op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
/* format % args (PyString_Format: flags, width, precision, the conversions s d i u o x X e E f g G c %, "%(key)"
 * with a dict) */
byte Native_Format(const nativeVal_t *format, const nativeVal_t *args, nativeVal_t *out);
/* int(v) and float(v) (PyNumber_Int, PyNumber_Float: numbers and texts) */
byte Native_ToInt(const nativeVal_t *v, nativeVal_t *out);
byte Native_ToFloat(const nativeVal_t *v, nativeVal_t *out);
/* len(v) of the script (PyObject_Length: a dict too): -1 with the TypeError of a value without length */
int Native_Size(const nativeVal_t *v);
/* v[i] of a text, list or tuple as a copy (PySequence_GetItem through Native_GetItem) */
byte Native_SeqItem(const nativeVal_t *v, long i, nativeVal_t *out);
/* v[key] as a copy (PyObject_GetItem: a dict by any key, a sequence by an int) */
byte Native_Subscript(const nativeVal_t *v, const nativeVal_t *key, nativeVal_t *out);
/* v[lo:hi] of a text, list or tuple (PySequence_GetSlice) */
byte Native_Slice(const nativeVal_t *v, long lo, long hi, nativeVal_t *out);
/* x in seq (PySequence_Contains): 1 or 0, -1 with the error */
int Native_Contains(const nativeVal_t *seq, const nativeVal_t *x);
/* the items a for loop over seq takes (Native_SeqItem each): -1 with the TypeError of a value that is no sequence */
int Native_ForCount(const nativeVal_t *seq);

/* ---- errors ---- */

/*
 * The error state of the scripts (src/game/Script/NativeErr.c, stage 7d of docs/objlib-plan.md): one error at a time,
 * its kind, the exception it was raised as when that is no kind (NE_OTHER: a string exception of a test, a class) and
 * its value (the text of Native_Raise, the key of a KeyError, any value of a raise). The kinds are the standard
 * exceptions of 1.5.2, which are strings in the game (no exceptions.py: str of the type is its name). A function that
 * fails raises and returns 0 (or NULL, -1); the caller passes the failure up, drops it (Native_ErrClear) or prints it
 * where the script printed its traceback (Native_ErrPrint).
 *
 * OBJLIB 7: Python's error state is this one. PyErr_Restore, PyErr_Occurred and PyErr_Fetch of the object library
 * (src/python/errors.c) come here through Py_ErrSet, Py_ErrType and Py_ErrTake, the boundary that converts the values
 * (a Python exception that is a standard one becomes its kind, its value the native value of its object, which keeps
 * the object for the way back).
 */
typedef enum {
	NE_NONE = -1,			/* no error */
	NE_INDEX,
	NE_VALUE,
	NE_TYPE,
	NE_ATTRIBUTE,
	NE_SYSTEM,
	NE_OVERFLOW,
	NE_ZERODIV,			/* ZeroDivisionError */
	NE_NAME,
	NE_KEY,
	NE_RUNTIME,
	NE_IMPORT,
	NE_SYNTAX,
	NE_MEMORY,
	NE_IO,
	NE_EOF,
	NE_OS,
	NE_FLOAT,			/* FloatingPointError */
	NE_ASSERTION,
	NE_INTERRUPT,			/* KeyboardInterrupt */
	NE_NOTIMPL,			/* NotImplementedError */
	NE_SYSTEMEXIT,
	NE_EXCEPTION,			/* the bases of the others */
	NE_STANDARD,
	NE_ARITHMETIC,
	NE_LOOKUP,
	NE_ENVIRONMENT,
	NE_OTHER,			/* raised as another exception: nativeErr_t.type */
	NE_KINDS
} nativeError_t;

/* an error taken out of the state (Native_ErrFetch) */
typedef struct {
	nativeError_t kind;
	nativeVal_t type;		/* NE_OTHER: what was raised as the exception, else None */
	nativeVal_t value;		/* the value of the exception (None for none) */
} nativeErr_t;

/* raises the error kind with the text (printf form, 511 characters at most); 0, for `return Native_Raise(...)` in a
 * function that returns 1 for success */
byte Native_Raise(nativeError_t kind, const char *fmt, ...);
/* the KeyError of d[key] (the key is the value of the error): 0 */
byte Native_RaiseKey(const nativeVal_t *key);
/* raise type, value of the script (value NULL: None): a standard exception as its kind (the same object, OBJLIB 7:
 * Python's string), anything else as NE_OTHER: 0 */
byte Native_RaiseObj(const nativeVal_t *type, const nativeVal_t *value);
/* raises the kind with value (NULL: None): 0 */
byte Native_RaiseVal(nativeError_t kind, const nativeVal_t *value);
/* an error is set */
byte Native_Failed(void);
/* the error set is of kind (PyErr_ExceptionMatches of a standard exception) */
byte Native_ErrIs(nativeError_t kind);
/* the error dropped */
void Native_ErrClear(void);
/* the error taken into *e (kind NE_NONE when there is none) and cleared; Native_ErrRestore sets it again (or clears
 * with NE_NONE) and takes the values over, Native_ErrFree frees an error not restored */
void Native_ErrFetch(nativeErr_t *e);
void Native_ErrRestore(nativeErr_t *e);
void Native_ErrFree(nativeErr_t *e);
/* e normalized as PyErr_NormalizeException does it: a class as the exception gets an instance of it as the value
 * (the class called with the value), or the class of an instance of a derived class as the exception */
void Native_ErrNormalize(nativeErr_t *e);
/* the error printed as PyErr_Print printed the traceback of a script (sys.last_type and sys.last_value set, the type,
 * ": " and the str of the value written to the error console through Scrap.ConsoleError, the file and line of a
 * SyntaxError with its tuple) and cleared; SystemExit ends the program as there */
void Native_ErrPrint(void);
/* the name of a standard kind ("TypeError"), NULL for NE_OTHER and NE_NONE */
const char *Native_ErrName(nativeError_t kind);
/* the exception of e as the script sees it (sys.exc_type): the object of a standard kind (OBJLIB 7: the string of
 * Python, so `is` holds), a copy of the type of NE_OTHER (None for no error) */
void Native_ErrType(const nativeErr_t *e, nativeVal_t *out);
/* the error as an except clause takes it (sys.exc_type and sys.exc_value: normalized, Native_ErrType and the value),
 * cleared; None and None for no error */
void Native_ErrCaught(nativeVal_t *type, nativeVal_t *value);
/* str() of the exception and of the value of e added to b ("TypeError", "function requires ..."): 0 with the error of
 * a str that fails */
byte Native_ErrTypeStr(nativeBuf_t *b, const nativeErr_t *e);
byte Native_ErrValueStr(nativeBuf_t *b, const nativeErr_t *e);
/* OBJLIB 7, the boundary for src/python/errors.c: PyErr_Restore (the references taken over; type NULL clears),
 * PyErr_Occurred (borrowed, NULL for none) and PyErr_Fetch (new references, cleared) on this state */
void Py_ErrSet(PyObject *type, PyObject *value, PyObject *tb);
PyObject *Py_ErrType(void);
void Py_ErrTake(PyObject **type, PyObject **value, PyObject **tb);
/* the name of the Python type of the value, as the errors of 1.5.2 write it ("None", "int", "string", "tuple", ...;
 * the type name of an NV_OBJ; "cPyEntity" for an entity handle) */
const char *Native_TypeName(const nativeVal_t *v);
/* len(seq) as PySequence_Length takes it (a list, tuple, text or vector): 1 with *n, else 0 with its TypeError */
byte Native_Len(const nativeVal_t *seq, int *n);
/* seq[i] as PySequence_GetItem (subscript 0) or the subscript of a script (subscript 1: a dict looks the int up)
 * gives it, a negative i from the end: the item (a character of a text and a number of a vector in tmp: Native_Free
 * tmp when the result is tmp), NULL with the error ("unindexable object", "unsubscriptable object", "tuple index out
 * of range", the KeyError) */
const nativeVal_t *Native_GetItem(const nativeVal_t *seq, long i, byte subscript, nativeVal_t *tmp);
/* the text of argument arg (from 1) of an engine function as its "s" reads it: 1 with *out, else 0 with the TypeError
 * of getargs.c ("argument 1: expected string, int found", "... string without null bytes ...") */
byte Native_ArgStr(const nativeVal_t *v, int arg, const char **out);
/* string.upper(v) (strop.upper: islower and toupper on every byte): 1 with *out (Native_Free it), else 0 with the
 * TypeError of a value that is no text ("read-only character buffer, int") */
byte Native_Upper(const nativeVal_t *in, nativeVal_t *out);
/* string.split(s, sep) (strop.split: sep NULL splits at blanks): 1 with the list in *out (Native_Free it), else 0 with
 * the TypeError of a value that is no text or the ValueError of an empty sep */
byte Native_Split(const nativeVal_t *s, const char *sep, nativeVal_t *out);
/* the same on the len bytes of t with a separator of n bytes (NULL: blanks) and at most maxsplit splits (0: all): the
 * list in *out, or 0 with the ValueError of an empty separator */
byte Native_SplitText(const char *t, int len, const char *sep, int n, int maxsplit, nativeVal_t *out);
/* the number of argument v of an engine function as its "d" or "f" reads it: 1 with *out, else 0 with the TypeError
 * of PyFloat_AsDouble ("illegal argument type for built-in operation") */
byte Native_ArgDouble(const nativeVal_t *v, double *out);
/* eval(text) of a value the game wrote, as Script_EvalIn(module, text) reads it: the checks of builtin eval (a text
 * without 0 bytes), then Native_Literal of the text after its leading blanks; 1 with *out (Native_Free it), else 0
 * with the error (SyntaxError of Script_NotRead for a text no reader takes) */
byte Native_EvalText(const char *module, const nativeVal_t *text, nativeVal_t *out);
/* i as an index of a list of count items as Python indexes it (a negative i counts from the end): 1 with *out, else
 * 0 with the IndexError of a list */
byte Native_Index(long i, int count, int *out);
/* int(text) of Python 1.5.2 on a text (blanks around it, a sign, decimal digits): 1 with *out, else 0 with its
 * ValueError ("invalid literal for int(): ...", "int() literal too large: ...") */
byte Native_IntOfText(const char *text, long *out);

/* ---- the engine ---- */

/* the error of an engine function that gives NULL without an exception, as a call from C reports it
 * (PyEval_CallObjectWithKeywords, "NULL result without error in call_object"): 0 */
byte Native_NullResult(void);
/* the number of argument v of an engine function as its "i" reads it (no range check, a float cut): 1 with *out,
 * else 0 with the TypeError of PyInt_AsLong ("illegal argument type for built-in operation") */
byte Native_ArgInt(const nativeVal_t *v, int *out);
/* `if v:` of a script: 0, 0.0, "", None and empty lists, tuples and dicts are false */
byte Native_True(const nativeVal_t *v);
/* SAct.GetClass(name): the class becomes the current one (1), or there is none (0) */
byte Native_ActClass(const char *name);
/* SAct.GetAction(name): the action of the current class becomes the current one (a debug line without a class) */
void Native_ActAction(const char *name);
/* SAct.GetAct(name): the variable of the current action (None and a debug line when there is no action or no such
 * variable) */
void Native_ActVar(const char *name, nativeVal_t *out);
/* SAct.SetAct(name, v) with a text or a number (a debug line when there is no action or v is neither, no error) */
void Native_ActSet(const char *name, const nativeVal_t *v);
/* SAct.SetCls(name, v) with a text or a number: 0 with Native_NullResult when there is no current class (and the
 * console line), with the TypeError of its arguments when v is neither */
byte Native_ClsSet(const char *name, const nativeVal_t *v);
/* the script variable name of entity e as its handle reads it (the core of the attribute get): an int, float, vector
 * or text variable, tried in this order (a text of any length; a text variable without a text is None): 1 with *out
 * (Native_Free it), 0 when the entity has no such variable (*out untouched) */
byte Native_EntRead(cEntity *e, const char *name, nativeVal_t *out);
/* entity.name as the script handle reads it (Native_EntRead; a text cut to the short form, NATIVE_TEXT - 1
 * characters). 0 when the entity has no such variable: the console line of the handle and its AttributeError; e NULL
 * is the handle of an entity that is gone (its console line and ceval's SystemError) */
byte Native_EntVar(cEntity *e, const char *ref, const char *name, nativeVal_t *out);
/* the core of the attribute set of a handle: v as an int, float, vector and text, in this order, each form it reads
 * as goes to the variable of that type until one takes it (1); 0 with the console line of the handle (ref names the
 * entity in it) when e is NULL (the entity is gone), v has none of the forms or no variable takes it */
byte Native_EntStore(cEntity *e, const char *ref, const char *name, const nativeVal_t *v);
/* entity.name = v as the script handle sets it (Native_EntStore); 0 when none takes it or e is NULL (the handle of
 * an entity that is gone): the console line of the handle and ceval's SystemError, as the line of the script */
byte Native_EntSet(cEntity *e, const char *ref, const char *name, const nativeVal_t *v);

/*
 * The seams of the engine functions a test case replaces (fc-chat records SNet.SendChatString and swaps
 * Scrap.GetNetFlags), as C hooks (ScriptPort.c, stage 7d). Each module.name a port asks for has an entry: the engine's
 * own function, a hook (what the case put there, and for a function of the test channel, a recorder or a lambda, its
 * C function and record, so the call does not go through an object of Python) or deleted. The entry is made at the
 * first question from what the module holds then; the test channel's stores of module attributes keep it in step
 * (Native_HookStore), and the entries go when the modules are made again for the next level (Native_HookClear). Native_Seam: 0 for the engine's own function (the port calls the C directly), 1 with *hook (a
 * copy that Native_HookCall takes over or Native_HookFree drops), -1 with the AttributeError of a deleted one (or the
 * error of the first lookup).
 */
typedef struct {
	nativeVal_t f;			/* what is there */
	byte (*call)(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out);	/* a channel function, else NULL */
	void *ctx;
} nativeHook_t;
int Native_Seam(const char *module, const char *name, nativeHook_t *hook);
/* the call of a hook (taken over) with the letters of Native_Call; the result into *out (NULL: dropped) in the form
 * the script got it (Native_Pass). A channel function runs in C on the arguments as its adapter got them (Port_Pack,
 * the depth check of the calls), any other callable through Python as before (OBJLIB 7). 0 with the error */
byte Native_HookCall(nativeHook_t *hook, nativeVal_t *out, const char *fmt, ...);
void Native_HookFree(nativeHook_t *hook);
/* o.name = v (v NULL: del o.name) was done: when o is a module and module.name has a seam entry, it follows */
void Native_HookStore(const nativeVal_t *o, const char *name, const nativeVal_t *v);
/* every seam entry dropped: the modules are made again (Python_Close before the next level) */
void Native_HookClear(void);
/* the test channel's function that tells its own functions: 1 with the C function and record of f */
void Native_HookFinder(byte (*find)(const nativeVal_t *f, nativeHook_t *hook));
/*
 * OBJLIB 7: calls of Python callables with native arguments, for the calls of names known at run time and the seams
 * (a value that may hold a port function goes through Port_Value instead). fmt has one letter per argument: v a const
 * nativeVal_t *, i a long, d a double, s a text (NULL: None), O a PyObject * (borrowed; P the same, see PORT_CALL), r a
 * const nativeVal_t * that holds an object the callee keeps or changes (a native entry gets the object itself, as P
 * gives a Python object; v would give a copy of a list or dict of plain values, as Native_Arg makes it). The
 * result goes into *out (Native_Free it; NULL drops it) as Native_Arg makes it. 0 with the error of the call.
 */
byte Native_Call(PyObject *callable, nativeVal_t *out, const char *fmt, ...);
/* the same with the result as it is (a new reference, NULL with the error): an entity handle or another object */
PyObject *Native_CallObj(PyObject *callable, const char *fmt, ...);
/* Native_Call on a va_list with a reference that the call takes over (the slow path of Port_Invoke) */
byte Native_CallVa(PyObject *callable, nativeVal_t *out, const char *fmt, va_list va);
/* the most arguments of one mini format (Native_ReadArgs) */
#define NATIVE_ARGS 16
/* one argument of the mini format as read from the arguments of the call: type the letter, then its value */
typedef struct {
	char type;
	const nativeVal_t *v;
	long i;
	double d;
	const char *s;
	PyObject *o;
} nativeArg_t;
/* the arguments of fmt (at most NATIVE_ARGS) read from va into a: their count */
int Native_ReadArgs(const char *fmt, va_list va, nativeArg_t *a);
/* one argument as Native_Call passes it: a new reference (a NULL text gives None, as for Py_BuildValue; a NULL object
 * gives NULL without an error), NULL with the error of the conversion */
PyObject *Native_ArgPy(const nativeArg_t *a);
/* the tuple of n arguments (NULL as for one of them) */
PyObject *Native_PyArgs(const nativeArg_t *a, int n);
/* module.name(...) through the module attribute (an AttributeError when it is not there) */
byte Native_CallMod(const char *module, const char *name, nativeVal_t *out, const char *fmt, ...);
/* name(...) as a function of the module calls a global of its own (the NameError of the script when it is gone) */
byte Native_CallGlobal(const char *module, const char *name, nativeVal_t *out, const char *fmt, ...);
/* Scrap.GetEntity(name): 1 with the entity in *out (NULL when there is none: the script gets None), 0 with the
 * TypeError of its argument for a value that is no text */
byte Native_GetEntity(const nativeVal_t *name, cEntity **out);
/* Scrap.Get(name): the engine variable as the script gets it (a text, an int, a float, "<function>" or "<Script
 * function>"); 0 for an unknown name (the console line of Scrap.Get and Native_NullResult) */
byte Native_GVarGet(const char *name, nativeVal_t *out);
/* Scrap.GetLangEntries(name, n) (n < 0: not given, 3) as a list of texts; 0 with the TypeError of its argument for a
 * value that is no text. through the seam (fc-ol2p-charsnpc swaps it), the hook called with the arguments
 * the caller gave */
byte Native_LangEntries(const nativeVal_t *name, int n, nativeVal_t *out);
/* Scrap.Set(name, v): a text (without 0 bytes) to a text variable, a number to a number variable; 0 with the
 * TypeError of its arguments for other values ("illegal argument type for built-in operation") or Native_NullResult
 * when the variable is not there or of the other type. through the seam (fc-om-dt records Scrap.Set) */
byte Native_GVarSet(const char *name, const nativeVal_t *v);
/* the core of Scrap.Set without the seam (its own entry): the result of GVar_SetStr or GVar_SetNum (1 set, 0 read
 * only, -1 no such variable or one of the other type, with the console line), -2 with the TypeError of a value that is
 * neither a text without 0 bytes nor a number */
int Native_GVarStore(const char *name, const nativeVal_t *v);
/* SScorer.Set(id, item, var, v) with a text or a number (nothing when the item is not there); 0 with the TypeError
 * of its arguments for other values. through the seam (fc-credits2 records SScorer.Set) */
byte Native_ScorerSet(int id, const char *item, const char *var, const nativeVal_t *v);
/* the core of SScorer.Set without the seam (its own entry): 1 with *r the result of the item's Var, 0 when the player
 * has no scorer or the item is not there, -1 with the TypeError of a value that is neither a text without 0 bytes nor
 * a number */
int Native_ScorerStore(int id, const char *item, const char *var, const nativeVal_t *v, int *r);
/* Scrap.GetNetFlags() == (0, 0, 0), one call through the seam: 1 or 0, -1 with the error set */
int Native_NetOff(void);
/* Scrap.GetNetFlags()[i] (0 client, 1 server, 2 dedicated): 1 or 0, -1 with the error set (through the seam) */
int Native_NetFlag(int i);

/* ---- module variables a port reads in the forms its C takes ---- */

/* the variable name of module (global 1: as a function of the script reads its global, a missing one the NameError of
 * the script; 0: the module's attribute, AttributeError) into *out (what it owned is freed) in the form the C takes:
 * types the NV_ types it takes (1 << NV_STR | ...; with NV_LONG, NV_LIST, NV_TUPLE or NV_DICT among them any value of
 * those types, texts of any length too, else the short forms of Native_FromPy; with NV_OBJ the object itself). 0 with
 * the error, a TypeError naming the variable for a value of another form */
byte Native_VarTake(const char *module, const char *name, byte global, unsigned types, nativeVal_t *out);
/* the same for the attribute name that holds a list of exactly count items: each into out[i] */
byte Native_VarItems(const char *module, const char *name, int count, unsigned types, nativeVal_t *out);
/* the item index of the list attribute name of module set to v (in place): 0 with the error */
byte Native_VarSetItem(const char *module, const char *name, int index, const nativeVal_t *v);

/* an argument of an adapter as a native value (Native_Free it): the native types as Native_FromPyAny takes them, an
 * entity handle (cPyEntity) as NV_ENT, any other value (a function, a class instance, or a container that holds one
 * or a handle: the container keeps its identity) as Native_SetObj gives it: the NV_REF view of a list, dict, instance,
 * class, function, method or module, else NV_OBJ (a tuple that holds objects); always 1. SCRAP_NATIVE_REF=0 gives the
 * NV_OBJ of before for all of them (one more round, for comparison) */
byte Native_Arg(PyObject *o, nativeVal_t *v);
/* the value as a new Python object (NULL with the error set): for adapters and mirrors only. A dict gets its keys in
 * slot order; Python's table can order them otherwise when two keys collide (OBJLIB 6: the literal reader of the
 * saves keeps Script_Literal until then). A native object gives its Python object (NObj_ToPy: the same one each
 * time) */
PyObject *Native_ToPy(const nativeVal_t *v);
/* a Python value into *v when its type is one of types (NV_ mask of None, int, float, a text shorter than
 * NATIVE_TEXT without 0 bytes, a 3-tuple of numbers as a vector): 1, else 0 without an error */
byte Native_FromPy(PyObject *o, unsigned types, nativeVal_t *v);
/* any value of the native types (None, int, long, float, text, and lists, tuples and dicts of them, 32 deep; a dict
 * in the slots Python has it in) into *v: 1 (Native_Free it), else 0 without an error and *v None */
byte Native_FromPyAny(PyObject *o, nativeVal_t *v);
/* the same for a value at that depth inside a container (the containers stop at NATIVE_DEPTH as there) */
byte Native_FromPyAt(PyObject *o, nativeVal_t *v, int depth);

/*
 * OBJLIB 9: *out the value an adapter's Native_Arg gets when a caller passes v through Python (Native_ToPy and back),
 * made without Python: a copy with vectors as tuples of floats and dicts laid out anew in their slot order (an entity
 * handle is the same handle). A value that holds an NV_OBJ or a native object, a container that holds an entity handle
 * (Native_Arg gives it back as one NV_OBJ), a long int not in its plain digits, or containers nested deeper than
 * Native_FromPyAny reads goes the Python way. 1 (Native_Free *out), else 0 with the error of Native_ToPy and *out None. The packer of the
 * direct calls between ports and the form their results take
 */
byte Native_Pass(const nativeVal_t *v, nativeVal_t *out);

/* ---- direct calls between ports (stage 3 of docs/objlib-plan.md) ---- */

/*
 * A port function that another port calls by its module and name is called directly when the module attribute still
 * holds the port's own C function (the seam, OBJLIB 5 (9c: functions as Python values in the
 * registry)), else the old way through Python with the same arguments (the slow path, OBJLIB 7): a recorder of the
 * test cases, an alias, a mission's or a mod's function there gets the call, and a deleted or non-callable attribute
 * raises the error it raised.
 *
 *   PORT_CALL(Module, Func, out, fmt, ...)   Module.Func(...) through the module attribute (Native_CallMod)
 *   PORT_OWN(Module, Func, out, fmt, ...)    Func(...) through the module's global, as a function of the module calls
 *                                            its own (Native_CallGlobal: the NameError of a deleted global)
 *   PORT_LCALL, PORT_LOWN                    the same for a module of a level (Map, Mission_X): its ref is named by
 *                                            the file's PORT_LEVEL (the level of its Levels.c entry, e.g. temple; a
 *                                            file with the modules of several levels defines it again before each
 *                                            part, #undef first, or names the level at the call)
 *   PORT_XCALL(level, Module, Func, ...)     PORT_LCALL and PORT_LOWN with the level named at the call
 *   PORT_XOWN(level, Module, Func, ...)
 *   PORT_ASK(Module, Func, fmt, ...)         Script_Ask: the result as the Python object the call gives (a new
 *   PORT_LASK, PORT_OWNASK, PORT_LOWNASK     reference, NULL with the error), the fast path the entry's result as
 *                                            its adapter returns it
 *
 * fmt and the arguments are those of Native_Call (v, i, d, s, O; a NULL s is None); out and the result as there. The
 * ref of Module.Func comes from include/scrap/scriptports.h (tools/script/ports.py makes it from the PyMethodDef
 * tables).
 *
 * Keyword arguments follow the positional letters after a ';', as name=letter separated by ',', their values after
 * the positional ones: PORT_CALL(Speech, AutoAddNI, NULL, "s;HeadName=s", key, "DeepThroat") is
 * Speech.AutoAddNI(key, HeadName = "DeepThroat"). The slow path passes them as the dict of keywords, in this order
 * (Mode_CallKw); the fast path puts each one into the slot of the entry its name has (the names of the ref, from the
 * <adapter>_N list of a keyword entry: MODE_KW, Mode_KwEntry), the slots between not given (Mode_KwSkip). A call the
 * entry cannot take that way (a ref without names, a name it does not have, a slot given twice) goes the slow way and
 * gets the error of the adapter.
 *
 * The letter P passes a PyObject * as the object itself: the slow path as O, the fast path as an NV_OBJ that holds it
 * (O there gives what Native_Arg makes of it: a copy of a pure list). It is for the entries that take an NV_OBJ to keep
 * the caller's object (MissionsFuncs.SetMultipleMissionTargets). A ref of an entry on Python objects (MODE_PYENTRY,
 * portRef_t.py) always takes the slow path: the adapter gets the objects themselves only that way.
 */

/* the most arguments of a port function (Mode_Args) */
#define PORT_ARGS 11

/* the native entry of a port function: its n arguments in a, the result into *out (None when called, left None for
 * a function that returns None): 1, or 0 with the error raised. MODE_FN and LV_FN make it as <adapter>_E, a hand
 * adapter has one of the same name and form; the adapter is the arguments as native values plus the entry */
typedef byte (*portEntry_t)(const nativeVal_t *a, int n, nativeVal_t *out);

/* a function of a port table: module.name, the adapter its table entry holds and its entry */
typedef struct {
	const char *module;
	const char *name;
	PyCFunction adapter;		/* OBJLIB 5 (7e): what the attribute holds while it is the port's own function */
	portEntry_t entry;
	char **kw;			/* the names of the entry's slots for keyword calls, NULL for none */
	byte py;			/* OBJLIB 7: the entry calls the adapter on Python objects (MODE_PYENTRY): the slow way */
} portRef_t;

/* a call site of PORT_CALL or PORT_OWN (one static per site) and the counts of the paths it took (OBJLIB 7) */
typedef struct portSite_s {
	const portRef_t *ref;
	byte global;			/* PORT_OWN (1), a call of a value (2: PORT_VSITE) */
	byte listed;			/* in the list of Port_Sites */
	const char *file;
	int line;
	unsigned long fast, slow, errors;
	struct portSite_s *next;
} portSite_t;

/* module.name (global 0) or the global name of module (global 1) is still the port's own function of ref: 0, or 1
 * with *hook a new reference to what is there (always 1 under SCRAP_PORT_SLOW=1, OBJLIB 7), -1 with the error of
 * the lookup (the import, AttributeError, NameError "global name") */
int Port_Seam(const portRef_t *ref, byte global, PyObject **hook);
/* the call of a site: the seam, then the entry on the arguments packed with Native_Pass and the depth check of
 * Script_CallObject, or the call through Python of Native_HookCall */
byte Port_Invoke(portSite_t *site, nativeVal_t *out, const char *fmt, ...);
/* Port_Invoke with the result as the Python object the call gives (a new reference, NULL with the error) */
PyObject *Port_Ask(portSite_t *site, const char *fmt, ...);
PyObject *Port_AskVa(portSite_t *site, const char *fmt, va_list va);
/* the TypeError of PyArg_ParseTuple for n arguments to a function of nargs: 0 */
byte Port_Arity(int n, int nargs);
/* the sites that ran since the start, the last one first (OBJLIB 7: the counters of the test channel) */
portSite_t *Port_Sites(void);

/*
 * The registry of the port functions by module and name (stage 4 of docs/objlib-plan.md): g_PortTable holds every ref
 * sorted by module and name (strcmp), made by tools/script/ports.py with the refs (src/game/Script/PortTable.c). A
 * name served by the modules of several levels (Map, Mission_X) has a ref per level, after each other.
 */
extern const portRef_t *const g_PortTable[];
/* the first ref of module.name (binary search), NULL when there is none */
const portRef_t *Port_Find(const char *module, const char *name);
/* the port's own function of ref: f is the C function of its table entry (the adapter: Mission_X.Init and
 * Mission_Y.Init share the name), without self (a recorder of the test channel has one); never under
 * SCRAP_PORT_SLOW=1 (OBJLIB 7). The test of the seam (Port_Seam) and of the callbacks (Port_Callback). */
byte Port_IsOwn(const portRef_t *ref, PyObject *f);
/* a callback bound by name (cPy_Function PYF_ENTRY): the ref of module.name whose own function f is (Port_IsOwn, not
 * a ref on Python objects, and the function's own name that one: cPy_Function_Name gives it), as the site that
 * counts its calls (fast: the calls, errors: the failed ones; listed in Port_Callbacks once bound, file "callback");
 * NULL when f is none of them */
portSite_t *Port_Callback(const char *module, const char *name, PyObject *f);
/* the callbacks bound since the start, the last one first (OBJLIB 7: the counters of the test channel) */
portSite_t *Port_Callbacks(void);
/* OBJLIB 7: the C function object (borrowed) the site of Port_Callback was first bound to: the same adapter, for the
 * Python call of a callback given more parameters than an entry takes (PORT_ARGS) */
PyObject *Port_CallbackFunc(const portSite_t *site);
/* the entry of ref on n native arguments a, with the depth check of Script_CallObject (Script_Enter), the result into
 * *r (None when it fails; Native_Free it either way): 1, or 0 with the error set (an entry that fails without one
 * gives the SystemError of call_object) */
byte Port_Entry(const portRef_t *ref, const nativeVal_t *a, int n, nativeVal_t *r);
/* a function given to the scheduler (cScheduledFunc, stage 4): the ref of Port_Own (by the adapter and the function's
 * own name), as the site that counts its scheduled calls (listed in Port_ScheduledSites once scheduled, file
 * "scheduled"); NULL when Port_Own finds none (a recorder, a lambda, a bound method, an engine function, a ref on
 * Python objects, everything under SCRAP_PORT_SLOW=1) */
portSite_t *Port_Scheduled(PyObject *f);
/* the functions scheduled as entries since the start, the last one first (OBJLIB 7: the counters of the test
 * channel) */
portSite_t *Port_ScheduledSites(void);
/* the call of a bound site (a callback, a scheduled call, a value that holds a port function: Port_Value) as the
 * Python call of its adapter on n arguments: the letters x, each one as the adapter gets it ('v' a native value
 * through Native_Pass, 'O' an object through Native_Arg, as Port_Invoke packs them), or with x NULL the native values
 * v through Native_Pass; the entry (Port_Entry), the result through the conversions of the adapter (Native_Pass) into
 * *res (Native_Free it either way; NULL: dropped), or with pyout what the adapter returns as the Python object (a new
 * reference, NULL with the error; res unused). Counts the call (fast) and a failure (errors). 1, or 0 with the error
 * set */
byte Port_BoundCall(portSite_t *site, const nativeArg_t *x, const nativeVal_t *v, int n, nativeVal_t *res,
	PyObject **pyout);

/*
 * Values that hold port functions (stage 4): a function a module variable, a table, a from import or a parameter
 * holds, called as the value it is. Port_Own finds the ref of the function itself in any module, by its adapter and
 * its own name (__name__: an alias in a table under another name is the ref of that name), so a variable that holds
 * another module's function (Net.ClientCreate = Ffa.ClientCreate) is the entry of that function, as the Python call
 * would run its adapter. NULL for any other value (a recorder, a lambda, a bound method, a class, an engine function),
 * a ref on Python objects (MODE_PYENTRY) and under SCRAP_PORT_SLOW=1 (OBJLIB 7).
 */
const portRef_t *Port_Own(PyObject *f);
/*
 * The call of a value f: the entry of the port function f is (Port_Own; the arguments packed as its adapter gets them,
 * the depth check of Port_Entry), else the call through Python of Native_Call (OBJLIB 7: the same object). f NULL
 * fails with the error its lookup set. Port_Value takes the letters of Native_Call (no keywords), Port_Values n native
 * values (more than PORT_ARGS go through Python) with the result as Native_Arg takes it into *out (NULL: dropped) or,
 * with pyout, as the Python object (a new reference, NULL with the error). The site (PORT_VSITE, one per call site)
 * counts the calls: fast through the entry, slow through Python, and names the port function it called last
 * ("value.?" before; listed by the channel's "native ports" as "value").
 */
byte Port_Value(portSite_t *site, PyObject *f, nativeVal_t *out, const char *fmt, ...);
byte Port_ValueVa(portSite_t *site, PyObject *f, nativeVal_t *out, const char *fmt, va_list va);
byte Port_Values(portSite_t *site, PyObject *f, const nativeVal_t *a, int n, nativeVal_t *out, PyObject **pyout);
/* the ref a value site names before its first call through an entry */
extern const portRef_t port_value;
#define PORT_VSITE() (&(static portSite_t){ .ref = &port_value, .global = 2, .file = __FILE__, .line = __LINE__ })
#define PORT_VALUE(f, out, ...) Port_Value(PORT_VSITE(), f, out, __VA_ARGS__)

/* the ref names: the module and the function pasted at once, in the macro the site writes, so that a module whose
 * name is also a macro of the file (#define CTF "CTF") is not replaced first */
#define PORT_REF(module, name) port_##module##_##name
#define PORT_LREF(module, name) PORT_LREF_(PORT_LEVEL, module##_##name)
#define PORT_XREF(level, module, name) PORT_LREF_(level, module##_##name)
#define PORT_LREF_(level, func) PORT_LREF__(level, func)
#define PORT_LREF__(level, func) port_##level##_##func
#define PORT_SITE(pref, own) \
	(&(static portSite_t){ .ref = &(pref), .global = (own), .file = __FILE__, .line = __LINE__ })
#define PORT_CALL(module, name, out, ...) Port_Invoke(PORT_SITE(port_##module##_##name, 0), out, __VA_ARGS__)
#define PORT_OWN(module, name, out, ...) Port_Invoke(PORT_SITE(port_##module##_##name, 1), out, __VA_ARGS__)
#define PORT_LCALL(module, name, out, ...) \
	Port_Invoke(PORT_SITE(PORT_LREF_(PORT_LEVEL, module##_##name), 0), out, __VA_ARGS__)
#define PORT_LOWN(module, name, out, ...) \
	Port_Invoke(PORT_SITE(PORT_LREF_(PORT_LEVEL, module##_##name), 1), out, __VA_ARGS__)
#define PORT_XCALL(level, module, name, out, ...) \
	Port_Invoke(PORT_SITE(PORT_LREF_(level, module##_##name), 0), out, __VA_ARGS__)
#define PORT_XOWN(level, module, name, out, ...) \
	Port_Invoke(PORT_SITE(PORT_LREF_(level, module##_##name), 1), out, __VA_ARGS__)
#define PORT_ASK(module, name, ...) Port_Ask(PORT_SITE(port_##module##_##name, 0), __VA_ARGS__)
#define PORT_OWNASK(module, name, ...) Port_Ask(PORT_SITE(port_##module##_##name, 1), __VA_ARGS__)
#define PORT_LASK(module, name, ...) Port_Ask(PORT_SITE(PORT_LREF_(PORT_LEVEL, module##_##name), 0), __VA_ARGS__)
#define PORT_LOWNASK(module, name, ...) Port_Ask(PORT_SITE(PORT_LREF_(PORT_LEVEL, module##_##name), 1), __VA_ARGS__)

/* the refs, as include/scrap/scriptports.h lists them: PORT_DECL declares one, PORT_DEF defines it (the list of a
 * file, PORTS(file), at the end of the file of the table, after the adapters and entries); PORT_DECLL and PORT_DEFL
 * for the modules of a level (the cast through void (*)(void) as in the tables: a keyword adapter has three
 * parameters); PORT_DEFK and PORT_DEFLK for a keyword entry with the names of its slots (adapter_N), PORT_DEFP and
 * PORT_DEFLP for an entry on Python objects (Mode_PyEntry) */
#define PORT_DECL(module, name) extern portRef_t port_##module##_##name;
#define PORT_DECLL(level, module, name) extern portRef_t port_##level##_##module##_##name;
#define PORT_DEF(module, name, adapter) \
	portRef_t port_##module##_##name = { #module, #name, (PyCFunction)(void (*)(void))adapter, adapter##_E, NULL, 0 };
#define PORT_DEFL(level, module, name, adapter) \
	portRef_t port_##level##_##module##_##name = { #module, #name, (PyCFunction)(void (*)(void))adapter, adapter##_E, \
		NULL, 0 };
#define PORT_DEFK(module, name, adapter) \
	portRef_t port_##module##_##name = { #module, #name, (PyCFunction)(void (*)(void))adapter, adapter##_E, adapter##_N, \
		0 };
#define PORT_DEFLK(level, module, name, adapter) \
	portRef_t port_##level##_##module##_##name = { #module, #name, (PyCFunction)(void (*)(void))adapter, adapter##_E, \
		adapter##_N, 0 };
#define PORT_DEFP(module, name, adapter) \
	portRef_t port_##module##_##name = { #module, #name, (PyCFunction)(void (*)(void))adapter, adapter##_E, NULL, 1 };
#define PORT_DEFLP(level, module, name, adapter) \
	portRef_t port_##level##_##module##_##name = { #module, #name, (PyCFunction)(void (*)(void))adapter, adapter##_E, \
		NULL, 1 };
#define PORTS(file) PORTS_##file

/* ---- the lines the game execs (src/game/Script/ScriptExec.c) ---- */

/*
 * exec(text) in a function of a module, for the texts the game makes or keeps in data (the scene generic lines, the
 * sound lines, the save code strings, the lines of the hangar, the characters, the elevators and the news panels):
 * statements split by ';', "import NAME", call chains NAME(.NAME | (args))* with literal arguments, an attribute set
 * to a literal or to two literals joined by * or /. Any other text raises Script_NotRead's SyntaxError and nothing of
 * it runs. The scope holds the module and the variables of the function the text can name (the locals of exec): the
 * native values the caller gives (borrowed, they outlive the scope) and the names an import bound.
 */
#define EXEC_NAME 0x40
#define EXEC_VARS 32

typedef struct {
	char name[EXEC_NAME];
	const nativeVal_t *v;		/* NULL: the module of that name (an import) */
} execVar_t;

typedef struct {
	const char *module;		/* the module whose function execs: its globals */
	execVar_t vars[EXEC_VARS];
	int n;
} execScope_t;

/* the variable name of the function is v (v NULL: the module name, as an import of the function left it) */
void Exec_Var(execScope_t *x, const char *name, const nativeVal_t *v);
/* the function has a variable name */
byte Exec_Bound(execScope_t *x, const char *name);
/* exec(code): 1, or 0 with the error set (the TypeError of a code that is no text, the ValueError of a 0 byte, the
 * SyntaxError of a text the reader does not take, the error of the statement that failed) */
byte Exec_Text(execScope_t *x, const nativeVal_t *code);
/* `import name` of a plain module name, bound in the variables */
byte Exec_Import(execScope_t *x, const char *name);
/* exec("import " + name): a module name directly, any other value through Exec_Text (the TypeError of the +, the
 * SyntaxError) */
byte Exec_ImportText(execScope_t *x, const nativeVal_t *name);
/* exec("import " + target + ";" + target + "." + func + "()"): directly when func is a name, else through Exec_Text */
byte Exec_ImportCall(execScope_t *x, const char *target, const nativeVal_t *func);
/* a + b + ... (n values, const nativeVal_t *) as such a function builds the text of its line: the + of the script,
 * also for an object (OBJLIB 7: an instance with __add__ or __radd__). 1 with *out (Native_Free it either way) */
byte Exec_Cat(nativeVal_t *out, int n, ...);
/* the statement name.func(a[0], ..., a[n - 1]) with name a variable of the function */
byte Exec_Call(execScope_t *x, const char *name, const char *func, const nativeVal_t *a, int n);

/* ---- the test channel ---- */

/* the answer of the channel's command "native <arg>" (src/game/Global/TestNative.c) added to out */
void TestNative_Command(nativeBuf_t *out, const char *arg);

/* ---- the native level files of batch G ---- */

/* Scrap.Def(name): the default value of the engine variable, as Native_GVarGet gives the value */
byte Native_GVarDef(const char *name, nativeVal_t *out);

/* ---- the values of the .mod evaluator (stage 6) ---- */

/* *v the value that holds the Python object o itself (a new reference): the NV_REF view of a list, dict, instance,
 * class, function, method or module (scrap/nativeobj.h), an entity handle (cPyEntity) as the NV_ENT of its handle, an
 * object of another type as NV_OBJ (OBJLIB 9) */
void Native_SetObj(nativeVal_t *v, PyObject *o);
/*
 * The accessors of the users (no file but the native layer tests NV_OBJ or reads nativeVal_t.obj):
 * Native_IsObj: v holds an object, a native one or a view (NV_REF) or a Python value (NV_OBJ, also the missing
 * argument below). Native_PyOf: the Python object behind such a value while Python exists (borrowed, valid while the
 * value lives; native data crosses to Python for it, NULL with the error when that fails), NULL for any other value.
 */
byte Native_IsObj(const nativeVal_t *v);
PyObject *Native_PyOf(const nativeVal_t *v);
/* the argument of a keyword entry that is not given (an NV_OBJ without an object) */
void Native_SetMissing(nativeVal_t *v);
byte Native_IsMissing(const nativeVal_t *v);
/* OBJLIB 5 (9c): the Python object a number or text came from, kept with the value (the constants of the .mod
 * evaluator): set (a new reference) and read (borrowed, NULL for none; of an NV_OBJ its object) */
void Native_KeepPy(nativeVal_t *v, PyObject *o);
PyObject *Native_KeptPy(const nativeVal_t *v);
/* v or a value in it (an item, a key, a value of a dict, at any depth) has one of types (a mask of 1 << NV_) */
byte Native_Holds(const nativeVal_t *v, unsigned types);
/* eval(text) when text is a plain literal: a text without 0 bytes, Native_Literal after its leading blanks (as the
 * readers of the save texts take it first). 1 with *out (Native_Free it), else 0 without an error and *out None */
byte Native_LiteralText(const nativeVal_t *text, nativeVal_t *out);
/*
 * OBJLIB 9: the dict d as a new Python dict with the same table, each key and value (made by conv: a new reference,
 * NULL with the error) in the slot it has in d, so that its keys come in the order 1.5.2 gives when the keys are set
 * in the order d was made (Native_ToPy sets them in slot order, which can place two colliding keys otherwise). A dict
 * with deleted slots, or one whose keys Python's lookdict would not find in their slots, is made in slot order. NULL
 * with the error (an unhashable key)
 */
PyObject *Native_ToPyDict(const nativeVal_t *d, PyObject *(*conv)(const nativeVal_t *v, void *ctx), void *ctx);
/* the same into the empty Python dict o (a dict that is known before its items are made: a native dict object that
 * holds itself): 1, or 0 with the error (o holds what was made until then) */
byte Native_FillPyDict(PyObject *o, const nativeVal_t *d, PyObject *(*conv)(const nativeVal_t *v, void *ctx), void *ctx);

#include "scrap/scriptports.h"

#endif
