/*
 * The built-in modules of the Python 1.5.2 library in C (stage 7e of docs/objlib-plan.md, batch 7e4c): the long ints
 * (src/game/Script/ScriptLong.c), the number protocol of abstract.c and the compare of object.c on native values
 * (ScriptNumber.c), the object types complex, xrange, slice, buffer, file and ellipsis (ScriptTypes.c), the modules
 * __builtin__ (ScriptBuiltin.c), math, strop and signal (ScriptLib.c) and sys (ScriptImport.c). Results, reprs and
 * error texts are those of the build's Python (Objects/, Modules/, Python/bltinmodule.c and sysmodule.c).
 */
#ifndef SCRAP_SCRIPTLIB_H
#define SCRAP_SCRIPTLIB_H

#include "scrap/nativeobj.h"

/* ---- long ints (NV_LONG; an NV_INT operand is taken as a long int) ---- */

/* a op b: + - * / % (floor division and modulo), 'd' divmod (a tuple), '<' '>' the shifts, & | ^: 1, or 0 with the
 * error (ZeroDivisionError "long division or modulo", ValueError "negative shift count", ...) */
byte Long_Op(int op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out);
/* pow(a, b[, c]) of long_pow (c NULL: none) */
byte Long_Pow(const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c, nativeVal_t *out);
/* -a, +a, ~a, abs(a) ('a') */
void Long_Unary(int op, const nativeVal_t *a, nativeVal_t *out);
void Long_FromLong(long l, nativeVal_t *out);
/* PyLong_FromDouble */
void Long_FromDouble(double d, nativeVal_t *out);
/* PyLong_AsDouble */
double Long_AsDouble(const nativeVal_t *v);
/* PyLong_AsLong: 0 with the OverflowError "long int too long to convert" */
byte Long_AsLong(const nativeVal_t *v, long *out);
/* long_format with the L: base 8 ("0...L"), 10, 16 ("0x...L", capital digits) */
void Long_Format(const nativeVal_t *v, int base, nativeBuf_t *out);
/* PyLong_FromString: blanks, a sign, blanks, the digits of base (0: by the prefix); *end after them. 0 with the
 * ValueError ("invalid base for long literal", "no digits in long int constant") */
byte Long_FromText(const char *s, int base, nativeVal_t *out, const char **end);

/* ---- the number protocol (abstract.c) and the compare (object.c) on any value ---- */

/* v op w of PyNumber_*: + - * / % 'p' (power) '<' '>' & | ^ 'd' (divmod); the coercions, the instance operators
 * (__coerce__, __add__ / __radd__ ...), the sequences (concat, repeat, % of a text) */
byte Lib_BinOp(int op, const nativeVal_t *v, const nativeVal_t *w, nativeVal_t *out);
/* pow(v, w, z) of PyNumber_Power (z NULL or None: two arguments) */
byte Lib_Power(const nativeVal_t *v, const nativeVal_t *w, const nativeVal_t *z, nativeVal_t *out);
/* -v, +v, ~v, abs(v) ('a') */
byte Lib_Unary(int op, const nativeVal_t *v, nativeVal_t *out);
/* PyNumber_Int, PyNumber_Long, PyNumber_Float (texts too) */
byte Lib_Int(const nativeVal_t *v, nativeVal_t *out);
byte Lib_Long(const nativeVal_t *v, nativeVal_t *out);
byte Lib_Float(const nativeVal_t *v, nativeVal_t *out);
/* hex(v), oct(v) of the number methods */
byte Lib_HexOct(const nativeVal_t *v, int base, nativeVal_t *out);
/* PyNumber_Coerce: *a and *b replaced by their coerced values (owned copies, Native_Free both); 1, 0 with the error
 * "number coercion failed" (the error only when raise; else 0 without one) */
byte Lib_Coerce(nativeVal_t *a, nativeVal_t *b, byte raise);
/* PyObject_Compare: the value 1.5.2 gives (the byte difference of texts, the length difference of lists ...), 0 for
 * a value equal; an error of an instance's __cmp__ is left set */
int Lib_Cmp(const nativeVal_t *a, const nativeVal_t *b);
/* the value has the number methods (tp_as_number): ints, longs, floats, complex, instances */
byte Lib_IsNumber(const nativeVal_t *v);
/* PyInt_AsLong and PyFloat_AsDouble: a number through its nb_int, nb_float (an instance's __int__, the TypeError of a
 * complex), else the TypeError "illegal argument type for built-in operation"; 0 with the error */
byte Lib_AsLong(const nativeVal_t *v, long *out);
byte Lib_AsDouble(const nativeVal_t *v, double *out);

/* the sequence protocol of the builtins (PySequence_Check, PySequence_Length, PySequence_GetItem): 1 for a text, tuple,
 * list, vector, list object, instance (its specials raise), xrange, buffer, a Python sequence; 0 for anything else */
byte Lib_IsSeq(const nativeVal_t *v);
int Lib_SeqLen(const nativeVal_t *v);
byte Lib_SeqItem(const nativeVal_t *v, int i, nativeVal_t *out);
/* the items as a list or tuple by value (PySequence_List, PySequence_Tuple): 0 with the error */
byte Lib_SeqList(const nativeVal_t *v, nativeType_t type, nativeVal_t *out);

/* ---- the object types of the library ---- */

extern const nativeCType_t lib_complexType, lib_xrangeType, lib_sliceType, lib_bufferType, lib_fileType,
	lib_ellipsisType;

void Lib_NewComplex(nativeVal_t *out, double re, double im);
/* the parts of a complex value: 1, 0 for any other value */
byte Lib_ComplexOf(const nativeVal_t *v, double *re, double *im);
/* PyRange_New */
void Lib_NewXRange(nativeVal_t *out, long start, long len, long step, int reps);
/* PySlice_New (NULL: None) */
void Lib_NewSlice(nativeVal_t *out, const nativeVal_t *start, const nativeVal_t *stop, const nativeVal_t *step);
/* buffer(obj, offset, size) of PyBuffer_FromObject (size -1: to the end) */
byte Lib_NewBuffer(const nativeVal_t *obj, int offset, int size, nativeVal_t *out);
/* the text of a buffer object: its bytes; 1, 0 for any other value */
byte Lib_BufferOf(const nativeVal_t *v, const char **p, int *len);
/* the singleton Ellipsis */
void Lib_Ellipsis(nativeVal_t *out);
/* open(name, mode, bufsize) of fileobject.c: 0 with the IOError (errno, text, name) */
byte Lib_Open(const char *name, const char *mode, int bufsize, nativeVal_t *out);
/* the file object of stdin, stdout or stderr ("<stdin>", ...), closed by nothing */
void Lib_StdFile(nativeVal_t *out, int which);
/* the IOError of errno with the file name (NULL: none): (errno, text[, name]) */
byte Lib_IOError(const char *name);

/* ---- the modules ---- */

/* a function of a module: its name, its entry and its __doc__ (NULL: None) */
typedef struct {
	const char *name;
	portEntry_t fn;
	const char *doc;
} libFn_t;

/* the call of a callable by a builtin that calls (map, filter, reduce, apply): f(a[0..n-1], **kw), kw NULL or a dict
 * (by value or object) of keywords; the values of a may be taken */
typedef byte (*libCall_t)(const nativeVal_t *f, nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out);
/* map, filter, reduce and apply with the calls of call (the test channel passes its own), on n arguments it may take */
byte Lib_Map(libCall_t call, nativeVal_t *a, int n, nativeVal_t *out);
byte Lib_Filter(libCall_t call, nativeVal_t *a, int n, nativeVal_t *out);
byte Lib_Reduce(libCall_t call, nativeVal_t *a, int n, nativeVal_t *out);
byte Lib_Apply(libCall_t call, nativeVal_t *a, int n, nativeVal_t *out);
/* the keywords of a call (a dict by value or object) as the (name, value) tuples of nativeCallKw_t: 0 with the error */
byte Lib_KwPairs(const nativeVal_t *kw, nativeVal_t *pairs);
/* a dict by value or a dict object; kw is a dict with keywords (a call with an empty dict has none) */
byte Lib_IsDict(const nativeVal_t *v);
byte Lib_HasKw(const nativeVal_t *kw);
/* getattr, hasattr, setattr and delattr through get and set (set v NULL: delattr) as the builtins (the test channel
 * passes its own) */
typedef byte (*libGetAttr_t)(const nativeVal_t *o, const char *name, nativeVal_t *out);
typedef byte (*libSetAttr_t)(const nativeVal_t *o, const char *name, const nativeVal_t *v);
byte Lib_GetAttrOf(libGetAttr_t get, const nativeVal_t *a, int n, nativeVal_t *out);
byte Lib_HasAttrOf(libGetAttr_t get, const nativeVal_t *a, int n, nativeVal_t *out);
byte Lib_SetAttrOf(libSetAttr_t set, const nativeVal_t *a, int n, nativeVal_t *out);
byte Lib_DelAttrOf(libSetAttr_t set, const nativeVal_t *a, int n, nativeVal_t *out);
/* min (sign -1) and max (1), tuple and list (type NV_TUPLE, NV_LIST), range of the builtins */
byte Lib_MinMax(const nativeVal_t *a, int n, nativeVal_t *out, int sign);
byte Lib_Seq(const nativeVal_t *a, int n, nativeVal_t *out, nativeType_t type);
byte Lib_Range(const nativeVal_t *a, int n, nativeVal_t *out);
/* round(x, digits) of bltinmodule.c */
double Lib_Round(double x, int digits);
/* the row of the builtin name of __builtin__, NULL for none */
const libFn_t *Lib_Builtin(const char *name);
/* the builtins of the import (ScriptImport.c): __import__, globals and locals, vars, dir, reload, raw_input */
byte Bi_Import(const nativeVal_t *a, int n, nativeVal_t *out);
byte Bi_Globals(const nativeVal_t *a, int n, nativeVal_t *out);
byte Bi_Vars(const nativeVal_t *a, int n, nativeVal_t *out);
byte Bi_Dir(const nativeVal_t *a, int n, nativeVal_t *out);
byte Bi_Reload(const nativeVal_t *a, int n, nativeVal_t *out);
byte Bi_RawInput(const nativeVal_t *a, int n, nativeVal_t *out);
/* OBJLIB 7: the standard exception name as the error state knows it (the text or tuple of __builtin__ with the object
 * of the library, src/game/Script/NativeErr.c), None for another name */
void Native_ErrObject(const char *name, nativeVal_t *out);
/* a function of a table of Lib_Module (a builtin of __builtin__ ...): its row, NULL for any other value */
const libFn_t *Lib_FnOf(const nativeVal_t *f);

/* the modules as their init functions made them (their dicts in the slot order of 1.5.2) */
void Lib_MakeBuiltin(nativeVal_t *out);
void Lib_MakeMath(nativeVal_t *out);
void Lib_MakeStrop(nativeVal_t *out);
void Lib_MakeSignal(nativeVal_t *out);
/* a module name whose dict gets __name__, __doc__ (doc NULL: None) and the functions of table in order, as
 * Py_InitModule4 makes it */
void Lib_Module(nativeVal_t *out, const char *name, const libFn_t *table, const char *doc);
/* name = v into the dict of module m (PyDict_SetItemString) */
void Lib_Put(const nativeVal_t *m, const char *name, const nativeVal_t *v);
/* xrange * n (range_repeat): 1, 0 for a value that is no xrange */
byte Lib_RangeRepeat(const nativeVal_t *v, long n, nativeVal_t *out);
/* buffer + other (buffer_concat), buffer * n (an int, buffer_repeat): 1, 0 with the error, -1 when v is no buffer */
int Lib_BufferOp(int op, const nativeVal_t *v, const nativeVal_t *w, nativeVal_t *out);

#endif
