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
	NV_OBJ,				/* OBJLIB 7: a Python value of another type (an entity handle, a function, a class
					 * instance), held by reference: only adapters make it (Native_Arg), the native
					 * functions refuse it with the error the script raised for it */
} nativeType_t;

/* every type: a mask of nativeVar_t.types for a variable that takes any value */
#define NV_ANY (1 << NV_NONE | 1 << NV_INT | 1 << NV_FLOAT | 1 << NV_STR | 1 << NV_VEC | 1 << NV_LONG | 1 << NV_LIST | \
	1 << NV_TUPLE | 1 << NV_DICT | 1 << NV_OBJ)

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
	PyObject *obj;			/* NV_OBJ: the value (a reference owned), else NULL */
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
/* *out a copy of v that owns its own memory (Native_Free it) */
void Native_Copy(nativeVal_t *out, const nativeVal_t *v);
/* frees what the value owns; it is None after */
void Native_Free(nativeVal_t *v);
/* the value is the text */
byte Native_IsStr(const nativeVal_t *v, const char *text);
/* a == b as Python 1.5.2 compares them: numbers by value (int, long and float with the coercions of 1.5.2), texts by
 * their bytes, None only with None, lists and tuples item by item, dicts by their keys and values; values of other
 * types are never equal (a text never equals a number, a vector is a tuple of three floats) */
byte Native_Equal(const nativeVal_t *a, const nativeVal_t *b);
/* hash(v) of Python 1.5.2 (string_hash, int_hash, long_hash, float_hash, tuplehash; None by its address, OBJLIB 9):
 * 1 with *out, 0 for a list or dict (unhashable, in a tuple too) and a long int of 3840 bits or more */
byte Native_Hash(const nativeVal_t *v, long *out);
/* d[key] = value as PyDict_SetItem does it (the table grows at the same moment, an equal key keeps the old key and
 * gets the new value): key and value are taken over (moved into the dict, or freed); 0 when the key has no hash */
byte Native_DictSet(nativeVal_t *d, nativeVal_t *key, nativeVal_t *value);
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
 * different types by the names of the types with the numbers below all others, two NV_OBJ by Python). Two different
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

/* the kinds of error a native function raises; while Python is linked each is the Python exception of that name */
typedef enum {
	NE_INDEX,
	NE_VALUE,
	NE_TYPE,
	NE_ATTRIBUTE,
	NE_SYSTEM,
	NE_OVERFLOW,
	NE_ZERODIV,			/* ZeroDivisionError */
	NE_NAME,
} nativeError_t;

/* raises the error kind with the text (printf form); 0, for `return Native_Raise(...)` in a function that returns
 * 1 for success. OBJLIB 7: the Python exception now, a C error state after Python */
byte Native_Raise(nativeError_t kind, const char *fmt, ...);
/* the KeyError of d[key] (the key is the value of the error): 0 */
byte Native_RaiseKey(const nativeVal_t *key);
/* the name of the Python type of the value, as the errors of 1.5.2 write it ("None", "int", "string", "tuple", ...;
 * the type name of an NV_OBJ, "cPyEntity") */
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
/* entity.name as the script handle reads it: an int, float, vector or text variable (a text variable without a text
 * is None). 0 when the entity has no such variable: the console line of the handle and its AttributeError; e NULL is
 * the handle of an entity that is gone (its console line and ceval's SystemError) */
byte Native_EntVar(cEntity *e, const char *ref, const char *name, nativeVal_t *out);
/* entity.name = v as the script handle sets it (PyEntity_SetVar: v as an int, float, vector and text, in this order,
 * each form it reads as goes to the variable of that type until one takes it); 0 when none takes it or e is NULL
 * (the handle of an entity that is gone): the console line of the handle and ceval's SystemError, as the line of the
 * script */
byte Native_EntSet(cEntity *e, const char *ref, const char *name, const nativeVal_t *v);

/*
 * OBJLIB 7, the seam of an engine function that a test case replaces (fc-chat records SNet.SendChatString and
 * swaps Scrap.GetNetFlags): 0 when module.name is still the engine's own C function (the port calls the C directly),
 * 1 when something else is there (*hook a new reference: the port calls it as the script did), -1 with the error of
 * the lookup set
 */
int Native_Seam(const char *module, const char *name, PyObject **hook);
/*
 * OBJLIB 3, 4 and 7: calls of Python callables with native arguments, for the calls between ports by name, the
 * callbacks and the seams. fmt has one letter per argument: v a const nativeVal_t *, i a long, d a double, s a text,
 * O a PyObject * (borrowed). The result goes into *out (Native_Free it; NULL drops it): any native value, else a
 * TypeError. 0 with the error of the call.
 */
byte Native_Call(PyObject *callable, nativeVal_t *out, const char *fmt, ...);
/* the same with the result as it is (a new reference, NULL with the error): an entity handle or another object */
PyObject *Native_CallObj(PyObject *callable, const char *fmt, ...);
/* the same with a new reference (a hook of Native_Seam) that the call takes over */
byte Native_HookCall(PyObject *hook, nativeVal_t *out, const char *fmt, ...);
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
/* Scrap.Set(name, v): a text (without 0 bytes) to a text variable, a number to a number variable; 0 with the
 * TypeError of its arguments for other values ("illegal argument type for built-in operation") or Native_NullResult
 * when the variable is not there or of the other type. OBJLIB 7: through the seam (fc-om-dt records Scrap.Set) */
byte Native_GVarSet(const char *name, const nativeVal_t *v);
/* SScorer.Set(id, item, var, v) with a text or a number (nothing when the item is not there); 0 with the TypeError
 * of its arguments for other values. OBJLIB 7: through the seam (fc-credits2 records SScorer.Set) */
byte Native_ScorerSet(int id, const char *item, const char *var, const nativeVal_t *v);
/* Scrap.GetNetFlags() == (0, 0, 0), one call through the seam: 1 or 0, -1 with the error set */
int Native_NetOff(void);
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
	unsigned types;			/* the NV_ types the C takes (1 << NV_STR | ...); with NV_LONG, NV_LIST, NV_TUPLE or
					 * NV_DICT among them any value of the types (texts of any length too), else the short
					 * forms of Native_FromPy */
} nativeVar_t;

/* an argument of an adapter as a native value (Native_Free it): the native types as Native_FromPyAny takes them, any
 * other value (an entity handle, a function, a class instance, or a container that holds one) as NV_OBJ (OBJLIB 7);
 * always 1 */
byte Native_Arg(PyObject *o, nativeVal_t *v);
/* the variables of the table (ends with a NULL name) from the dict of module into C: 0 with the error set when one
 * is missing or of a form the C does not take (TypeError naming it) */
byte Native_Import(const char *module, const nativeVar_t *vars);
/* Native_Import for variables a function of the script reads as its globals: a missing one is the NameError of the
 * script ("global name"), not the AttributeError of the module attribute */
byte Native_ImportGlobals(const char *module, const nativeVar_t *vars);
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
