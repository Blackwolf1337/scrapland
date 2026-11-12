/*
 * Native objects with identity (stage 9 of docs/objlib-plan.md, batch 9a: the mechanism). A nativeObj_t is a
 * reference-counted heap object of one of the kinds below; a native value holds one as the kind NV_REF
 * (scrap/scriptnative.h): Native_Copy shares the object, Native_Free releases it, the last holder frees it. They are
 * the native form of what the scripts hold by reference and change in place: lists and dicts several holders share,
 * class instances, classes, functions, bound methods and modules. Lists and dicts by value (NV_LIST, NV_DICT) stay for
 * temporaries and literals; NObj_Share turns one into an object when it gets a second holder. There is no cycle
 * collector (1.5.2 has none either): an object that holds itself is never freed.
 *
 * The operations are those of Python 1.5.2 for these types (Objects/listobject.c, dictobject.c, classobject.c,
 * methodobject.c, funcobject.c, and the object calls of abstract.c and ceval.c), with their error texts. A native
 * instance honours __init__ (the creation), __repr__, __str__ and __call__ and no other special method: the three
 * classes of the game data (VehiclesNPC.cShipAI, CharsNPC.CharSpawnPos, GateKeeper.GateKeeperType) define none but
 * the __init__ of CharSpawnPos, and the helper objects of the test cases (tc_obj) use __call__ and __repr__.
 *
 * OBJLIB 9, the bridge while Python is linked. An object is either native data (made by the functions below) or the
 * view of a Python object (made by Native_FromPyRef: it forwards every operation to that object). The identity table
 * maps a Python object to its one native object, and the native object names its Python object, so a value that goes
 * to Python and comes back is the same object on both sides:
 *   - A view holds one reference of its Python object while a native value holds the view. The table entry holds
 *     nothing. When the last native holder goes, the view leaves the table and drops the Python object; while Python
 *     alone holds the object there is no native one, and the next Native_FromPyRef makes a new view. Python never
 *     holds a native object, so neither side keeps the other alive in a ring.
 *   - Native data that must cross to Python (Native_ToPy of the NV_REF) is converted once: the Python object is made
 *     from it (a list with the items, a dict with the same table, an instance, a class, a C function object), the
 *     native data is freed, and from then on the Python object is the master and the native object its view (the
 *     types of 1.5.2 cannot be the view of native data). The value keeps its identity on the native side (the same
 *     nativeObj_t, the same hash); its repr shows the address of the Python object from then on, as a handle shows
 *     the one of its face.
 * A function keeps what it calls (the port ref, the callback) after it crossed, so its native call does not go through
 * Python.
 */
#ifndef SCRAP_NATIVEOBJ_H
#define SCRAP_NATIVEOBJ_H

#include "scrap/scriptnative.h"

typedef enum {
	NO_LIST,
	NO_DICT,			/* the table of 1.5.2, as NV_DICT: keys(), items() and repr in its slot order */
	NO_INSTANCE,			/* a class and an attribute dict */
	NO_CLASS,			/* a name, the base classes and a dict */
	NO_FUNCTION,			/* a port function (portRef_t), a data function called by module and name, a C callback
					 * with a context, a method of a list or dict bound to it; as a view also a Python
					 * function or C function */
	NO_METHOD,			/* a function of a class bound to an instance, or unbound */
	NO_MODULE,			/* the registry's module (a view of the module object while Python is linked) */
} nativeObjKind_t;

typedef struct nativeObj_s nativeObj_t;

/* a C callback as a function object: the n arguments in a, the result into *out (None when called): 1, or 0 with the
 * error raised */
typedef byte (*nativeCall_t)(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out);

/* ---- holders ---- */

/* one more holder: o */
nativeObj_t *NObj_Keep(nativeObj_t *o);
/* a holder less; the last one frees the object (a view leaves the identity table and drops its Python object) */
void NObj_Drop(nativeObj_t *o);
/* the kind of the object v holds (NO_), -1 for a value that is no NV_REF */
int NObj_Kind(const nativeVal_t *v);
/* a is b: both hold the same object (values of other kinds: None with None, the same entity handle, the same Python
 * object; numbers and texts are never the same object here) */
byte NObj_Is(const nativeVal_t *a, const nativeVal_t *b);
/* the objects made and freed since the start, the entries of the identity table now (the objects that have a Python
 * object: the views) and the objects of native data that crossed to Python since the start */
void NObj_Counts(unsigned long *made, unsigned long *freed, unsigned long *views, unsigned long *crossed);
/* the objects of the identity table now (at most max), one line "view <type> holders <n> <repr>" each added to b: who
 * still holds a view (the test channel) */
void NObj_ListViews(nativeBuf_t *b, int max);

/* ---- making objects (each *out an NV_REF held once: Native_Free it) ---- */

/* an empty list or dict */
void NObj_NewList(nativeVal_t *out);
void NObj_NewDict(nativeVal_t *out);
/* a list or dict by value becomes an object in place (its items moved, not copied), and so do the lists and dicts
 * inside it at any depth (inside tuples too): the one conversion for a value that gets a second holder. Other values
 * stay as they are. Every store into an object (an item, an attribute, an argument of append) does it */
void NObj_Share(nativeVal_t *v);
/* class name(bases): dict (an NV_DICT by value, NULL for an empty one) is moved into the class; module (NULL: none) is
 * its __module__ when the dict has none, __doc__ None when it has none (PyClass_New). bases: NULL, or a tuple or list
 * of classes. 0 with the TypeError of a base that is no class ("base is not a class object") */
byte NObj_NewClass(nativeVal_t *out, const char *name, const char *module, const nativeVal_t *bases, nativeVal_t *dict);
/* the function object of a port function (repr "<built-in function Name>"; called through Port_BoundCall) */
void NObj_NewPortFunc(nativeVal_t *out, const portRef_t *ref);
/* a data function: the global name of module, looked up at each call (repr as a C function) */
void NObj_NewDataFunc(nativeVal_t *out, const char *module, const char *name);
/* a C callback as a function: call(ctx, ...) runs it, drop(ctx) (NULL: nothing) frees the context with the last
 * holder. binds 1: it behaves as a function of Python code (repr "<function name at ...>", a class binds it to its
 * instances as a method); 0: as a C function (repr "<built-in function name>", never bound) */
void NObj_NewCallback(nativeVal_t *out, const char *name, nativeCall_t call, void *ctx, void (*drop)(void *ctx),
	byte binds);
/* the method object of func (a function object) for cls: bound to the instance self, or unbound (self NULL). 0 with a
 * TypeError when func, self or cls is not of that kind */
byte NObj_NewMethod(nativeVal_t *out, const nativeVal_t *func, const nativeVal_t *self, const nativeVal_t *cls);
/* the module name as the scripts import it (OBJLIB 9: the view of its module object): 0 with the ImportError */
byte NObj_Module(nativeVal_t *out, const char *name);

/* ---- operations on a value (an NV_REF by its object; the kinds by value as Native_ of scriptnative.h has them, an
 * NV_OBJ and an entity handle through Python). Each: 1 with *out made (Native_Free it), 0 with the error raised ---- */

/* repr(v), str(v) or the check() form added to b (Native_Repr with the errors of __repr__ and __str__ kept) */
byte NObj_Repr(nativeBuf_t *b, const nativeVal_t *v, nativeRepr_t mode);
/* hash(v): 0 with the TypeError "unhashable type" (a list, a dict, a method of one), "unhashable instance" */
byte NObj_Hash(const nativeVal_t *v, long *out);
/* len(v): -1 with the error */
int NObj_Len(const nativeVal_t *v);
/* x in seq: 1 or 0, -1 with the error */
int NObj_Contains(const nativeVal_t *seq, const nativeVal_t *x);
/* getattr(o, name), setattr(o, name, v), delattr(o, name) */
byte NObj_GetAttr(const nativeVal_t *o, const char *name, nativeVal_t *out);
byte NObj_SetAttr(const nativeVal_t *o, const char *name, const nativeVal_t *v);
byte NObj_DelAttr(const nativeVal_t *o, const char *name);
/* o[key], o[key] = v, del o[key] */
byte NObj_GetItem(const nativeVal_t *o, const nativeVal_t *key, nativeVal_t *out);
byte NObj_SetItem(const nativeVal_t *o, const nativeVal_t *key, const nativeVal_t *v);
byte NObj_DelItem(const nativeVal_t *o, const nativeVal_t *key);
/* o[lo:hi] (an omitted bound: 0 and NOBJ_END), o[lo:hi] = seq, del o[lo:hi] (seq NULL) */
#define NOBJ_END 0x7fffffffL
byte NObj_GetSlice(const nativeVal_t *o, long lo, long hi, nativeVal_t *out);
byte NObj_SetSlice(const nativeVal_t *o, long lo, long hi, const nativeVal_t *seq);
/* f(a[0], ..., a[n - 1]): a function (a port function through Port_BoundCall), a method (the instance first), a
 * class (a new instance, __init__ called), an instance with __call__ */
byte NObj_Call(const nativeVal_t *f, const nativeVal_t *a, int n, nativeVal_t *out);
/* o.name(a[0], ..., a[n - 1]): NObj_GetAttr, then NObj_Call (the one lookup of the list and dict methods: append,
 * insert, extend, pop, remove, index, count, reverse, sort; keys, values, items, has_key, get, update, clear, copy) */
byte NObj_CallMethod(const nativeVal_t *o, const char *name, const nativeVal_t *a, int n, nativeVal_t *out);

/* ---- the hooks of the value functions (ScriptNative.c calls them for an NV_REF) ---- */

/* Native_Equal and Native_Compare when a or b is an NV_REF (the other may be of any kind: a list object equals a list
 * by value with the same items) */
byte NObj_Equal(const nativeVal_t *a, const nativeVal_t *b);
int NObj_Compare(const nativeVal_t *a, const nativeVal_t *b);
/* Native_Hash: 1 with *out, 0 without an error for an unhashable object */
byte NObj_HashQuiet(const nativeVal_t *v, long *out);
/* Native_True */
byte NObj_True(const nativeVal_t *v);
/* Native_TypeName: "list", "dictionary", "instance", "class", "function", "builtin_function_or_method", "instance
 * method", "module" */
const char *NObj_TypeName(const nativeVal_t *v);

/* ---- OBJLIB 9: the bridge ---- */

/* the NV_REF of a Python list, dict, instance, class, function, C function, method or module: its native object when
 * it has one, else a new view (1, Native_Free *v); 0 without an error and *v None for an object of another type */
byte Native_FromPyRef(PyObject *o, nativeVal_t *v);
/* a Python value as the operations of a view give it: a number, text or None by value, a tuple by value with its
 * items by this rule, an entity handle as NV_ENT, an object Native_FromPyRef takes as its NV_REF (a list or dict of
 * plain values too: it keeps its identity), anything else as NV_OBJ */
void NObj_FromPy(PyObject *o, nativeVal_t *v);
/* Native_ToPy of an NV_REF: its Python object (a new reference), made from the native data at the first crossing
 * (see the head comment); NULL with the error */
PyObject *NObj_ToPy(const nativeVal_t *v);
/* the Python object behind the value now (borrowed): the one of a view, of an NV_OBJ; NULL for native data that never
 * crossed and the kinds by value (no conversion, unlike Native_PyOf) */
PyObject *NObj_Py(const nativeVal_t *v);
/* the table of a dict object of native data (NV_DICT, changed in place by the caller with the Native_Dict functions);
 * NULL for a view, a dict that crossed, another value */
nativeVal_t *NObj_DictData(const nativeVal_t *d);
/* the items of a list object of native data (an NV_LIST by value, changed in place by the caller); NULL for a view,
 * a list that crossed, another value */
const nativeVal_t *NObj_ListData(const nativeVal_t *l);
/* Native_FromPyAny(Native_ToPy(v)) without the crossing of native data: numbers, texts, None, and lists, tuples and
 * dicts (by value, objects, views) of them by value, 32 deep (a dict with its table); 1 with *out (Native_Free it),
 * 0 without an error and *out None when v holds another value (an instance, a function, a module, an entity handle) */
byte NObj_Plain(const nativeVal_t *v, nativeVal_t *out);
/* Native_Arg(Native_ToPy(v)) the same way: NObj_Plain, else the object or handle itself (an NV_OBJ as Native_SetObj
 * gives it, a tuple that holds an object as its Python tuple) */
void NObj_ArgOf(const nativeVal_t *v, nativeVal_t *out);
/* 1 by default: Native_SetObj and Native_Arg give the NV_REF view of an object Native_FromPyRef takes. 0 under
 * SCRAP_NATIVE_REF=0 (read once): the NV_OBJ of before batch 9a2, kept one more round for comparison */
byte NObj_RefMode(void);
/* the measure of what stays native: 0 native data, 1 the view of an object Python made, 2 native data that crossed to
 * Python (now a view too); -1 for a value that is no NV_REF */
int NObj_State(const nativeVal_t *v);

#endif
