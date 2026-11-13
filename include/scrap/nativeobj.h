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
					 * with a context, a method of a list or dict bound to it */
	NO_METHOD,			/* a function of a class bound to an instance, or unbound */
	NO_MODULE,			/* a module: its name and its variables (a dict object; a pack module's are the
					 * registry's, scrap/modvars.h) */
	NO_TYPE,			/* a type object (what type() gives): one per type name, kept for good */
	NO_COBJ,			/* a C object of the engine (a scorer item): its type's functions on its context */
} nativeObjKind_t;

typedef struct nativeObj_s nativeObj_t;

/* the type of a C object (NO_COBJ): the name its repr ("<name object at ...>"), type() and errors show, its getattr
 * (1 with *out; 0 with the error raised, or without one as the C type's getattr gives NULL; NULL: none), its setattr
 * (NULL: none; v NULL:
 * delattr; 1, or 0 as getattr), the text the print statement writes for it (tp_print; NULL: str) and the drop of its
 * context with the last holder (NULL: nothing) */
/* a method of a C object type: called with the object's context (a bound method object per lookup, equal for one
 * method of one object) */
typedef struct nativeCMethod_s {
	const char *name;
	byte (*fn)(void *ctx, const struct nativeVal_s *a, int n, struct nativeVal_s *out);
} nativeCMethod_t;

typedef struct nativeCType_s {
	const char *name;
	byte (*getattr)(void *ctx, const char *name, nativeVal_t *out);
	byte (*setattr)(void *ctx, const char *name, const nativeVal_t *v);
	void (*print)(void *ctx, char *buf, int size);
	void (*drop)(void *ctx);
	/* the protocols of the object types of the library (batch 7e4c; NULL: none): repr and str (addr the address the
	 * object shows; NULL: "<name object at ...>"), sq_length (-1 with the error), sq_item of an index the length made
	 * non-negative, sq_slice of bounds made so, the compare of a and b where one is of the type (the value 1.5.2 gives,
	 * not only its sign), tp_hash (0 with the error; NULL: the address), the truth (NULL: by sq_length, else true) */
	byte (*repr)(void *ctx, uintptr_t addr, nativeBuf_t *b, nativeRepr_t mode);
	int (*len)(void *ctx);
	byte (*item)(void *ctx, long i, nativeVal_t *out);
	byte (*slice)(void *ctx, long lo, long hi, nativeVal_t *out);
	int (*compare)(const nativeVal_t *a, const nativeVal_t *b);
	byte (*hash)(void *ctx, long *out);
	byte (*truth)(void *ctx);
	/* the methods (looked up before the getattr: __methods__, then the names), NULL or ended by a NULL name; a
	 * type without getattr and methods has no attributes ("'name' object has no attribute ..."), one without
	 * setattr read-only ones */
	const nativeCMethod_t *methods;
} nativeCType_t;

/* a C callback as a function object: the n arguments in a, the result into *out (None when called): 1, or 0 with the
 * error raised */
typedef byte (*nativeCall_t)(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out);
/* the same with the keywords: kw an NV_LIST of (name, value) tuples in the order given, NULL when there are none */
typedef byte (*nativeCallKw_t)(void *ctx, const nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out);

/* ---- holders ---- */

/* one more holder: o */
nativeObj_t *NObj_Keep(nativeObj_t *o);
/* a holder less; the last one frees the object */
void NObj_Drop(nativeObj_t *o);
/* the kind of the object v holds (NO_), -1 for a value that is no NV_REF */
int NObj_Kind(const nativeVal_t *v);
/* a is b: both hold the same object (values of other kinds: None with None, the same entity handle; numbers and
 * texts are never the same object here) */
byte NObj_Is(const nativeVal_t *a, const nativeVal_t *b);
/* the objects made and freed since the start */
void NObj_Counts(unsigned long *made, unsigned long *freed);

/* ---- making objects (each *out an NV_REF held once: Native_Free it) ---- */

/* an empty list or dict */
void NObj_NewList(nativeVal_t *out);
void NObj_NewDict(nativeVal_t *out);
/* a list or dict by value becomes an object in place (its items moved, not copied), and so do the lists and dicts
 * inside it at any depth (inside tuples too): the one conversion for a value that gets a second holder. Other values
 * stay as they are. Every store into an object (an item, an attribute, an argument of append) does it */
void NObj_Share(nativeVal_t *v);
/* class name(bases): dict (an NV_DICT by value, NULL for an empty one) is moved into the class; module (NULL: none) is
 * its __module__ when the dict has none, __doc__ None when it has none (classobject.c Class_New). bases: NULL, or a
 * tuple or list of classes. 0 with the TypeError of a base that is no class ("base is not a class object") */
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
/* a callback function made by NObj_NewCallback (binds 0) gets a __self__ (copied; not None: its repr is that of a
 * C function bound to an object of that type, as a function of a C table with a self) and callkw, its call when
 * keywords are given (NULL: it takes none) */
void NObj_SetCallback(const nativeVal_t *f, const nativeVal_t *self, nativeCallKw_t callkw);
/* a C entry bound to the entity handle self (copied), as a method of a C type: repr "<built-in method name of
 * cPyEntity object at ...>", equal for one entry on one handle, __self__ the handle; its call is entry(self,
 * arguments...) (the arguments as Native_Pass hands them over, no keywords) */
void NObj_NewBound(nativeVal_t *out, const char *name, portEntry_t entry, const nativeVal_t *self);
/* the __doc__ of a callback function made by NObj_NewCallback (a static text, NULL: None) */
void NObj_SetCallbackDoc(const nativeVal_t *f, const char *doc);
/* the holders of the object v holds (sys.getrefcount), 0 for a value that is no NV_REF */
int NObj_Refs(const nativeVal_t *v);
/* the address the repr of the object v holds shows, 0 for a value that is no NV_REF */
uintptr_t NObj_Addr(const nativeVal_t *v);
/* the type of the C object a value holds, NULL for any other value */
const nativeCType_t *NObj_CType(const nativeVal_t *v);
/* the C object of type a value holds: its context, NULL for any other value */
void *NObj_CObjOf(const nativeVal_t *v, const nativeCType_t *type);
/* the C callback and context of a callback function: 1, 0 for any other value */
byte NObj_CallbackOf(const nativeVal_t *f, nativeCall_t *call, void **ctx);
/* v as a call through Python handed it to a callee: a copy with its lists and dicts as new objects (NObj_Share), a
 * vector as the tuple of its floats */
void NObj_CallArg(const nativeVal_t *v, nativeVal_t *out);
/* v holds an object (an NV_REF) */
byte NObj_Native(const nativeVal_t *v);
/* the port ref of a port function made by NObj_NewPortFunc, NULL for any other value */
const portRef_t *NObj_PortRef(const nativeVal_t *f);
/* the method object of func (a function object) for cls: bound to the instance self, or unbound (self NULL). 0 with a
 * TypeError when func, self or cls is not of that kind */
byte NObj_NewMethod(nativeVal_t *out, const nativeVal_t *func, const nativeVal_t *self, const nativeVal_t *cls);
/* cls NULL and self NULL above: an unbound method whose class comes later (a class body that holds a method of its
 * own class, CharsNPC.CharSpawnPos.__init__); NObj_SetMethodClass gives it (the class and its dict then hold each
 * other: NObj_SetMethodClass(m, NULL) breaks that again) */
void NObj_SetMethodClass(const nativeVal_t *m, const nativeVal_t *cls);
/* the type object of that name (type(): repr "<type 'name'>", __name__, __doc__, compared and hashed by identity),
 * one per name; NObj_TypeOf: the type of v (by Native_TypeName) */
void NObj_TypeNamed(nativeVal_t *out, const char *name);
void NObj_TypeOf(const nativeVal_t *v, nativeVal_t *out);
/* a new C object of type on its context ctx */
void NObj_NewCObj(nativeVal_t *out, const nativeCType_t *type, void *ctx);
/* the text the print statement writes for v when its type has one (a C object with print): 1 with the text in buf, 0
 * for any other value */
byte NObj_PrintText(const nativeVal_t *v, char *buf, int size);
/* the module name as the scripts import it (Script_Module): 0 with the ImportError */
byte NObj_Module(nativeVal_t *out, const char *name);
/* a new module name (as 1.5.2's module_new: its variables __name__ and __doc__ None), in no module store */
void NObj_NewModule(nativeVal_t *out, const char *name);
/* the variables of the module m (a dict object, held by the module: the pointer is the module's key while it lives),
 * NULL for any other value */
const nativeVal_t *NObj_ModuleVars(const nativeVal_t *m);

/* ---- operations on a value (an NV_REF by its object; the kinds by value as Native_ of scriptnative.h has them, an
 * entity handle as its handle). Each: 1 with *out made (Native_Free it), 0 with the error raised ---- */

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
/* callable(v): a function, a method, a class, an instance that has __call__; 0 for anything else */
byte NObj_Callable(const nativeVal_t *v);
/* f(a..., **kw) with kw as nativeCallKw_t has them (NULL or empty: NObj_Call): a callback with its callkw, a class,
 * instance or method, a port function (its keyword entry); a native function without callkw raises "this function takes
 * no keyword arguments" */
byte NObj_CallKw(const nativeVal_t *f, const nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out);

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
 * method", "module", "type" */
const char *NObj_TypeName(const nativeVal_t *v);

/* the table of a dict object (NV_DICT, changed in place by the caller with the Native_Dict functions); NULL for
 * another value */
nativeVal_t *NObj_DictData(const nativeVal_t *d);
/* the items of a list object (an NV_LIST by value, changed in place by the caller); NULL for another value */
const nativeVal_t *NObj_ListData(const nativeVal_t *l);
/* the plain data of v: numbers, texts, None, and lists, tuples and dicts (by value or objects) of them by value, 32
 * deep (a dict with its table); 1 with *out (Native_Free it), 0 without an error and *out None when v holds another
 * value (an instance, a function, a module, an entity handle) */
byte NObj_Plain(const nativeVal_t *v, nativeVal_t *out);
/* v as a call through Python handed it back: NObj_Plain, else the object or handle itself, a tuple, list or dict by
 * value that holds objects or handles (a list or dict as a new object, a tuple of such items) */
void NObj_ArgOf(const nativeVal_t *v, nativeVal_t *out);

#endif
