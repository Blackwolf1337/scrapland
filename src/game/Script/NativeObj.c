/*
 * Native objects with identity (include/scrap/nativeobj.h, stage 9 of docs/objlib-plan.md): the reference-counted
 * lists, dicts, instances, classes, functions, methods and modules that native values hold as NV_REF, their
 * operations as Python 1.5.2 has them, and the bridge to the Python objects while Python is linked (OBJLIB 9: the
 * views, the identity table and the one crossing of native data).
 */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/pyentity.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

/* the kinds of a function object */
enum {
	NF_PORT,			/* a port function: ref */
	NF_CALLBACK,			/* a C callback: cb (a data function is one that looks its name up) */
	NF_BUILTIN,			/* a method of a list or dict: self and the index in obj_methods */
};

/* a C callback and its context, shared by the function object and by its C function object in Python (OBJLIB 9) */
typedef struct nativeFn_s {
	int refs;
	nativeCall_t call;
	nativeCallKw_t callkw;		/* the call with keywords, NULL when it takes none */
	void *ctx;
	void (*drop)(void *ctx);
	nativeVal_t self;		/* its __self__ (None: a function of the C table) */
	PyMethodDef def;		/* OBJLIB 9: the method definition of its C function object */
	char name[];
} nativeFn_t;

struct nativeObj_s {
	byte kind;			/* NO_ */
	byte func;			/* NO_FUNCTION: NF_ */
	byte born;			/* made as native data (0: made as the view of a Python object) */
	byte binds;			/* NO_FUNCTION: a class binds it to its instances (a function of Python code) */
	byte busy;			/* NO_LIST: being sorted */
	int refs;
	PyObject *py;			/* OBJLIB 9: its Python object (a reference held), NULL for native data that never crossed */
	nativeObj_t *next;		/* in its chain of the identity table */
	nativeVal_t data;		/* NO_LIST: the NV_LIST; NO_DICT: the NV_DICT; NO_CLASS: the NV_TUPLE of its bases */
	nativeObj_t *dict;		/* NO_INSTANCE, NO_CLASS: the attributes (a NO_DICT) */
	nativeObj_t *cls;		/* NO_INSTANCE: its class; NO_METHOD: the class of the method */
	nativeObj_t *self;		/* NO_METHOD: the instance (NULL: unbound); NF_BUILTIN: the list or dict */
	nativeObj_t *fn;		/* NO_METHOD: the function */
	char *name;			/* NO_CLASS, NO_FUNCTION: owned */
	const portRef_t *ref;		/* NF_PORT */
	nativeFn_t *cb;			/* NF_CALLBACK */
	int method;			/* NF_BUILTIN */
};

static unsigned long obj_made, obj_freed, obj_crossed;

/* ---- the identity table (OBJLIB 9): Python object -> its native object, chained by nativeObj_t.next ---- */

static nativeObj_t **obj_table;
static unsigned obj_tableSize, obj_tableCount;

static unsigned Obj_Slot(const PyObject *py, unsigned size) {
	uintptr_t x = (uintptr_t)py;
	return (unsigned)((x >> 4) ^ (x >> 13) ^ (x >> 22)) & (size - 1);
}

static nativeObj_t *Obj_TableFind(const PyObject *py) {
	if (!obj_tableSize) {
		return NULL;
	}
	nativeObj_t *o = obj_table[Obj_Slot(py, obj_tableSize)];
	while (o && o->py != py) {
		o = o->next;
	}
	return o;
}

static void Obj_TableAdd(nativeObj_t *o) {
	if (obj_tableCount >= obj_tableSize) {
		unsigned size = obj_tableSize ? obj_tableSize * 2 : 256;
		nativeObj_t **t = Mem_Alloc((uint32_t)(size * sizeof(*t)));
		memset(t, 0, size * sizeof(*t));
		for (unsigned i = 0; i < obj_tableSize; i++) {
			for (nativeObj_t *x = obj_table[i], *n; x; x = n) {
				n = x->next;
				unsigned s = Obj_Slot(x->py, size);
				x->next = t[s];
				t[s] = x;
			}
		}
		Mem_Free(obj_table);
		obj_table = t;
		obj_tableSize = size;
	}
	unsigned s = Obj_Slot(o->py, obj_tableSize);
	o->next = obj_table[s];
	obj_table[s] = o;
	obj_tableCount++;
}

static void Obj_TableRemove(nativeObj_t *o) {
	nativeObj_t **p = &obj_table[Obj_Slot(o->py, obj_tableSize)];
	while (*p != o) {
		p = &(*p)->next;
	}
	*p = o->next;
	o->next = NULL;
	obj_tableCount--;
}

/* ---- holders ---- */

static nativeObj_t *Obj_New(byte kind) {
	nativeObj_t *o = Mem_Alloc(sizeof(*o));
	memset(o, 0, sizeof(*o));
	o->kind = kind;
	o->born = 1;
	o->refs = 1;
	Native_SetNone(&o->data);
	obj_made++;
	return o;
}

static void Obj_SetRef(nativeVal_t *v, nativeObj_t *o) {
	Native_SetNone(v);
	v->type = NV_REF;
	v->ref = o;
}

static char *Obj_Text(const char *s) {
	size_t n = strlen(s) + 1;
	char *t = Mem_Alloc((uint32_t)n);
	memcpy(t, s, n);
	return t;
}

static void Obj_FnDrop(nativeFn_t *f) {
	if (f && --f->refs == 0) {
		if (f->drop) {
			f->drop(f->ctx);
		}
		Native_Free(&f->self);
		Mem_Free(f);
	}
}

/* what the native data of o holds is released (the object stays, as a view or to be freed) */
static void Obj_Clear(nativeObj_t *o) {
	nativeObj_t *dict = o->dict, *cls = o->cls, *self = o->self, *fn = o->fn;
	o->dict = o->cls = o->self = o->fn = NULL;
	Native_Free(&o->data);
	if (dict) {
		NObj_Drop(dict);
	}
	if (cls) {
		NObj_Drop(cls);
	}
	if (self) {
		NObj_Drop(self);
	}
	if (fn) {
		NObj_Drop(fn);
	}
}

nativeObj_t *NObj_Keep(nativeObj_t *o) {
	o->refs++;
	return o;
}

void NObj_Drop(nativeObj_t *o) {
	if (--o->refs > 0) {
		return;
	}
	/* held while its parts go (a part that holds the object again does not free it twice) */
	o->refs = 1 << 30;
	Obj_Clear(o);
	if (o->py) {
		Obj_TableRemove(o);
		Py_DECREF(o->py);
	}
	Obj_FnDrop(o->cb);
	Mem_Free(o->name);
	Mem_Free(o);
	obj_freed++;
}

int NObj_Kind(const nativeVal_t *v) {
	return v->type == NV_REF ? v->ref->kind : -1;
}

byte NObj_Is(const nativeVal_t *a, const nativeVal_t *b) {
	/* one Python object as a view and as an NV_OBJ */
	if ((a->type == NV_REF && b->type == NV_OBJ) || (a->type == NV_OBJ && b->type == NV_REF)) {
		return a->type == NV_REF ? a->ref->py == b->obj : b->ref->py == a->obj;
	}
	if (a->type != b->type) {
		return 0;
	}
	switch (a->type) {
	case NV_NONE:
		return 1;
	case NV_REF:
		return a->ref == b->ref;
	case NV_ENT:
		return a->ent == b->ent;
	case NV_OBJ:
		return a->obj == b->obj;
	}
	return 0;
}

void NObj_Counts(unsigned long *made, unsigned long *freed, unsigned long *views, unsigned long *crossed) {
	*made = obj_made;
	*freed = obj_freed;
	*views = obj_tableCount;
	*crossed = obj_crossed;
}

void NObj_ListViews(nativeBuf_t *b, int max) {
	for (unsigned i = 0; i < obj_tableSize && max > 0; i++) {
		for (const nativeObj_t *o = obj_table[i]; o && max > 0; o = o->next, max--) {
			char head[0x80];
			PyObject *r = PyObject_Repr(o->py);
			snprintf(head, sizeof(head), "view %s holders %d ", o->py->ob_type->tp_name, o->refs);
			Native_BufAdd(b, head, -1);
			if (r) {
				Native_BufAdd(b, PyString_AsString(r), PyString_Size(r) > 200 ? 200 : PyString_Size(r));
				Py_DECREF(r);
			}
			Native_ErrClear();
			Native_BufAdd(b, "\n", 1);
		}
	}
}

byte NObj_RefMode(void) {
	static int mode = -1;
	if (mode < 0) {
		const char *e = getenv("SCRAP_NATIVE_REF");
		mode = !(e && !strcmp(e, "0"));
	}
	return (byte)mode;
}

/* under SCRAP_NATIVE_COUNT=1 the program prints at its end the objects made and freed (what is left then is held by
 * the C state of the ports, which is never freed, or was lost) */
__attribute__((destructor)) static void Obj_AtExit(void) {
	const char *e = getenv("SCRAP_NATIVE_COUNT");
	if (e && !strcmp(e, "1")) {
		printf("native objects at exit: made %lu freed %lu left %lu views %u crossed %lu\n", obj_made, obj_freed,
			obj_made - obj_freed, obj_tableCount, obj_crossed);
		fflush(stdout);
	}
}

/* the address the repr of the object shows and its order among objects without a compare: the Python object's while
 * it has one, as a handle shows its face */
static uintptr_t Obj_Addr(const nativeObj_t *o) {
	return o->py ? (uintptr_t)o->py : (uintptr_t)o;
}

/* the Python object the operations of o are forwarded to (borrowed), NULL for native data. A native function keeps
 * its own behaviour after it crossed (its Python object is a C function object made for it), and so does the module
 * object of a pack module (its variables are the registry's) */
static PyObject *Obj_View(const nativeObj_t *o) {
	return o->born && (o->kind == NO_FUNCTION || o->kind == NO_MODULE) ? NULL : o->py;
}

/* the Python object an operation on v goes to (a new reference): the view's, an NV_OBJ, the face of an entity handle;
 * NULL without an error for a value the native code handles */
static PyObject *Obj_Forward(const nativeVal_t *v) {
	if (v->type == NV_REF) {
		PyObject *py = Obj_View(v->ref);
		Py_XINCREF(py);
		return py;
	}
	return v->type == NV_OBJ || v->type == NV_ENT ? Native_ToPy(v) : NULL;
}

/* ---- OBJLIB 9: Python values into native ones ---- */

static int Obj_PyKind(PyObject *o) {
	if (PyList_Check(o)) {
		return NO_LIST;
	}
	if (PyDict_Check(o)) {
		return NO_DICT;
	}
	if (PyInstance_Check(o)) {
		return NO_INSTANCE;
	}
	if (PyClass_Check(o)) {
		return NO_CLASS;
	}
	if (PyFunction_Check(o) || PyCFunction_Check(o)) {
		return NO_FUNCTION;
	}
	if (PyMethod_Check(o)) {
		return NO_METHOD;
	}
	return PyModule_Check(o) ? NO_MODULE : -1;
}

static PyObject *Obj_Tramp(PyObject *self, PyObject *args, PyObject *kw);

byte Native_FromPyRef(PyObject *o, nativeVal_t *v) {
	Native_SetNone(v);
	int kind = Obj_PyKind(o);
	if (kind < 0) {
		return 0;
	}
	nativeObj_t *n = Obj_TableFind(o);
	if (n) {
		NObj_Keep(n);
	} else {
		modVarRef_t mv;
		n = Obj_New((byte)kind);
		/* a pack module is native from the start: the registry has its variables, the module object is its key and
		 * what it gives to Python (OBJLIB 9: the module objects are Python's until stage 7e) */
		n->born = kind == NO_MODULE && ModVar_Of(o, &mv);
		n->binds = PyFunction_Check(o) != 0;
		if (PyCFunction_Check(o) && ((PyCFunctionObject *)o)->m_ml->ml_meth == (PyCFunction)(void (*)(void))Obj_Tramp) {
			/* the C function object of a native callback whose native object went (Python held it alone): the
			 * callback again, its call and context shared */
			nativeFn_t *f = PyCObject_AsVoidPtr(PyCFunction_GET_SELF(o));
			f->refs++;
			n->born = 1;
			n->func = NF_CALLBACK;
			n->cb = f;
			n->name = Obj_Text(f->name);
		}
		Py_INCREF(o);
		n->py = o;
		Obj_TableAdd(n);
	}
	Obj_SetRef(v, n);
	return 1;
}

void NObj_FromPy(PyObject *o, nativeVal_t *v) {
	if (PyTuple_Check(o)) {
		Native_SetEmpty(v, NV_TUPLE);
		for (int i = 0; i < PyTuple_GET_SIZE(o); i++) {
			NObj_FromPy(PyTuple_GET_ITEM(o, i), Native_Append(v));
		}
		return;
	}
	if (PyList_Check(o) || PyDict_Check(o) || PyEntity_Check(o) || !Native_FromPyAny(o, v)) {
		if (PyEntity_Check(o) || !Native_FromPyRef(o, v)) {
			Native_SetObj(v, o);
		}
	}
}

/* the result r of a Python call (a new reference, given away) into *out: 1, or 0 with *out None when r is NULL (the
 * error is set) */
static byte Obj_Result(PyObject *r, nativeVal_t *out) {
	Native_SetNone(out);
	if (!r) {
		return 0;
	}
	NObj_FromPy(r, out);
	Py_DECREF(r);
	return 1;
}

/* a value that holds an NV_OBJ (the result of a port entry) in the form NObj_FromPy gives: in place */
static byte Obj_Norm(nativeVal_t *v) {
	if (!Native_Holds(v, 1u << NV_OBJ)) {
		return 1;
	}
	PyObject *py = Native_ToPy(v);
	Native_Free(v);
	return Obj_Result(py, v);
}

/* the items of the Python tuple, list or dict of a view as a native value by value (the items by NObj_FromPy): for the
 * operations that read a view next to native data */
static void Obj_Snap(PyObject *py, nativeVal_t *out) {
	if (PyList_Check(py)) {
		Native_SetEmpty(out, NV_LIST);
		for (int i = 0; i < PyList_GET_SIZE(py); i++) {
			NObj_FromPy(PyList_GET_ITEM(py, i), Native_Append(out));
		}
		return;
	}
	Native_SetEmpty(out, NV_DICT);
	PyObject *k, *x;
	int pos = 0;
	while (PyDict_Next(py, &pos, &k, &x)) {
		nativeVal_t nk, nx;
		NObj_FromPy(k, &nk);
		NObj_FromPy(x, &nx);
		Native_DictSet(out, &nk, &nx);
	}
}

/* v as a list (type NV_LIST) or dict (NV_DICT) of items to read: the value itself, the data of its object, or a
 * snapshot of its view in *tmp (Native_Free tmp after); NULL when v is none of them */
static const nativeVal_t *Obj_Items(const nativeVal_t *v, byte type, nativeVal_t *tmp) {
	Native_SetNone(tmp);
	if (v->type == type) {
		return v;
	}
	if (v->type != NV_REF || v->ref->kind != (type == NV_LIST ? NO_LIST : NO_DICT)) {
		return NULL;
	}
	if (!v->ref->py) {
		return &v->ref->data;
	}
	Obj_Snap(v->ref->py, tmp);
	return tmp;
}

/* ---- making objects ---- */

void NObj_NewList(nativeVal_t *out) {
	nativeObj_t *o = Obj_New(NO_LIST);
	Native_SetEmpty(&o->data, NV_LIST);
	Obj_SetRef(out, o);
}

void NObj_NewDict(nativeVal_t *out) {
	nativeObj_t *o = Obj_New(NO_DICT);
	Native_SetEmpty(&o->data, NV_DICT);
	Obj_SetRef(out, o);
}

void NObj_Share(nativeVal_t *v) {
	if (v->type != NV_LIST && v->type != NV_TUPLE && v->type != NV_DICT) {
		return;
	}
	if (v->type == NV_DICT) {
		/* the values in their slots; a key is hashable, so it holds no list or dict */
		const nativeVal_t *k, *x;
		int pos = 0;
		while (Native_DictNext(v, &pos, &k, &x)) {
			NObj_Share((nativeVal_t *)x);
		}
	} else {
		for (int i = 0; i < v->n; i++) {
			NObj_Share(&v->items[i]);
		}
	}
	if (v->type != NV_TUPLE) {
		nativeObj_t *o = Obj_New(v->type == NV_LIST ? NO_LIST : NO_DICT);
		o->data = *v;
		Obj_SetRef(v, o);
	}
}

static byte Obj_Norm(nativeVal_t *v);

/* a copy of v to keep in an object: shared (NObj_Share), a Python value in it in the form a view's operations give it
 * (a number by value, an object as its view: what the object holds is the same either way) */
static void Obj_Store(nativeVal_t *slot, const nativeVal_t *v) {
	Native_Copy(slot, v);
	if (!Obj_Norm(slot)) {
		Native_ErrClear();
	}
	NObj_Share(slot);
}

/* v holds an object of that kind */
static byte Obj_IsKind(const nativeVal_t *v, int kind) {
	return v->type == NV_REF && v->ref->kind == kind;
}

static nativeObj_t *Obj_NewDictObj(void) {
	nativeVal_t d;
	NObj_NewDict(&d);
	return d.ref;
}

/* d[name] of a dict object (native or a view): 1 with *out a copy, 0 when it has none */
static byte Obj_DictLook(const nativeObj_t *d, const char *name, nativeVal_t *out) {
	if (d->py) {
		PyObject *x = PyDict_GetItemString(d->py, (char *)name);
		if (!x) {
			return 0;
		}
		NObj_FromPy(x, out);
		return 1;
	}
	nativeVal_t key;
	Native_SetText(&key, name, (int)strlen(name));
	const nativeVal_t *x = Native_DictGet(&d->data, &key);
	Native_Free(&key);
	if (!x) {
		return 0;
	}
	Native_Copy(out, x);
	return 1;
}

/* d[name] = v: 1, 0 with the error (of the Python dict) */
static byte Obj_DictPut(nativeObj_t *d, const char *name, const nativeVal_t *v) {
	if (d->py) {
		PyObject *x = Native_ToPy(v);
		int r = x ? PyDict_SetItemString(d->py, (char *)name, x) : -1;
		Py_XDECREF(x);
		return r == 0;
	}
	nativeVal_t key, x;
	Native_SetText(&key, name, (int)strlen(name));
	Obj_Store(&x, v);
	return Native_DictSet(&d->data, &key, &x);
}

/* del d[name]: 1, 0 without an error when it has none */
static byte Obj_DictRemove(nativeObj_t *d, const char *name) {
	if (d->py) {
		if (PyDict_DelItemString(d->py, (char *)name) != 0) {
			Native_ErrClear();
			return 0;
		}
		return 1;
	}
	nativeVal_t key;
	Native_SetText(&key, name, (int)strlen(name));
	byte ok = Native_DictDel(&d->data, &key);
	Native_Free(&key);
	return ok;
}

/* the bases as a tuple of classes from a tuple or list (by value or an object): 1 with *out, 0 when it is none or an
 * item is no class (no error set) */
static byte Obj_Bases(const nativeVal_t *bases, nativeVal_t *out) {
	nativeVal_t tmp;
	const nativeVal_t *b = bases->type == NV_TUPLE ? bases : Obj_Items(bases, NV_LIST, &tmp);
	Native_SetEmpty(out, NV_TUPLE);
	byte ok = b != NULL;
	for (int i = 0; ok && i < b->n; i++) {
		ok = Obj_IsKind(&b->items[i], NO_CLASS);
		if (ok) {
			Native_Copy(Native_Append(out), &b->items[i]);
		}
	}
	if (bases->type != NV_TUPLE) {
		Native_Free(&tmp);
	}
	if (!ok) {
		Native_Free(out);
	}
	return ok;
}

byte NObj_NewClass(nativeVal_t *out, const char *name, const char *module, const nativeVal_t *bases, nativeVal_t *dict) {
	nativeVal_t b, x;
	Native_SetNone(out);
	Native_SetEmpty(&b, NV_TUPLE);
	if (bases && !Obj_Bases(bases, &b)) {
		if (dict) {
			Native_Free(dict);
		}
		return Native_Raise(NE_TYPE, "base is not a class object");
	}
	nativeObj_t *o = Obj_New(NO_CLASS);
	o->name = Obj_Text(name);
	o->data = b;
	if (dict && (dict->type == NV_DICT || Obj_IsKind(dict, NO_DICT))) {
		/* the dict of the class is that object */
		NObj_Share(dict);
		o->dict = dict->ref;
		Native_SetNone(dict);
	} else {
		o->dict = Obj_NewDictObj();
	}
	/* PyClass_New: __doc__ and __module__ when the body gave none */
	if (!Obj_DictLook(o->dict, "__doc__", &x)) {
		Native_SetNone(&x);
		Obj_DictPut(o->dict, "__doc__", &x);
	}
	Native_Free(&x);
	if (module && !Obj_DictLook(o->dict, "__module__", &x)) {
		Native_SetText(&x, module, (int)strlen(module));
		Obj_DictPut(o->dict, "__module__", &x);
	}
	Native_Free(&x);
	Obj_SetRef(out, o);
	return 1;
}

void NObj_NewPortFunc(nativeVal_t *out, const portRef_t *ref) {
	nativeObj_t *o = Obj_New(NO_FUNCTION);
	o->func = NF_PORT;
	o->ref = ref;
	o->name = Obj_Text(ref->name);
	Obj_SetRef(out, o);
}

void NObj_NewCallback(nativeVal_t *out, const char *name, nativeCall_t call, void *ctx, void (*drop)(void *ctx),
	byte binds) {
	size_t n = strlen(name) + 1;
	nativeFn_t *f = Mem_Alloc((uint32_t)(sizeof(*f) + n));
	memset(f, 0, sizeof(*f));
	f->refs = 1;
	f->call = call;
	f->ctx = ctx;
	f->drop = drop;
	Native_SetNone(&f->self);
	memcpy(f->name, name, n);
	nativeObj_t *o = Obj_New(NO_FUNCTION);
	o->func = NF_CALLBACK;
	o->binds = binds;
	o->cb = f;
	o->name = Obj_Text(name);
	Obj_SetRef(out, o);
}

void NObj_SetCallback(const nativeVal_t *f, const nativeVal_t *self, nativeCallKw_t callkw) {
	nativeFn_t *fn = f->ref->cb;
	Native_Free(&fn->self);
	Native_Copy(&fn->self, self);
	fn->callkw = callkw;
}

byte NObj_CallbackOf(const nativeVal_t *f, nativeCall_t *call, void **ctx) {
	if (f->type != NV_REF || f->ref->kind != NO_FUNCTION || f->ref->func != NF_CALLBACK) {
		return 0;
	}
	*call = f->ref->cb->call;
	*ctx = f->ref->cb->ctx;
	return 1;
}

void NObj_CallArg(const nativeVal_t *v, nativeVal_t *out) {
	if (v->type == NV_VEC) {
		Native_SetEmpty(out, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetFloat(Native_Append(out), v->v[i]);
		}
		return;
	}
	Obj_Store(out, v);
}

byte NObj_Native(const nativeVal_t *v) {
	return v->type == NV_REF && !Obj_View(v->ref);
}

const portRef_t *NObj_PortRef(const nativeVal_t *f) {
	return f->type == NV_REF && f->ref->kind == NO_FUNCTION && f->ref->func == NF_PORT ? f->ref->ref : NULL;
}

static byte Obj_CallPy(PyObject *f, const nativeVal_t *a, int n, nativeVal_t *out);

/* a data function: ctx the module name, then the function name; the global looked up and called as a value */
static byte Obj_DataCall(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *module = ctx, *name = module + strlen(module) + 1;
	PyObject *f = Script_Global(module, name);
	byte ok = f && Obj_CallPy(f, a, n, out);
	Py_XDECREF(f);
	return ok;
}

static void Obj_DataFree(void *ctx) {
	Mem_Free(ctx);
}

void NObj_NewDataFunc(nativeVal_t *out, const char *module, const char *name) {
	size_t m = strlen(module) + 1, n = strlen(name) + 1;
	char *ctx = Mem_Alloc((uint32_t)(m + n));
	memcpy(ctx, module, m);
	memcpy(ctx + m, name, n);
	NObj_NewCallback(out, name, Obj_DataCall, ctx, Obj_DataFree, 0);
}

byte NObj_NewMethod(nativeVal_t *out, const nativeVal_t *func, const nativeVal_t *self, const nativeVal_t *cls) {
	Native_SetNone(out);
	if (!Obj_IsKind(func, NO_FUNCTION) || (cls ? !Obj_IsKind(cls, NO_CLASS) : self != NULL) ||
		(self && !Obj_IsKind(self, NO_INSTANCE))) {
		return Native_Raise(NE_TYPE, "a method takes a function, an instance and a class");
	}
	nativeObj_t *o = Obj_New(NO_METHOD);
	o->fn = NObj_Keep(func->ref);
	o->cls = cls ? NObj_Keep(cls->ref) : NULL;
	o->self = self ? NObj_Keep(self->ref) : NULL;
	Obj_SetRef(out, o);
	return 1;
}

void NObj_SetMethodClass(const nativeVal_t *m, const nativeVal_t *cls) {
	nativeObj_t *o = m->ref, *old = o->cls;
	o->cls = cls ? NObj_Keep(cls->ref) : NULL;
	if (old) {
		NObj_Drop(old);
	}
}

byte NObj_Module(nativeVal_t *out, const char *name) {
	PyObject *m = Script_Module(name);
	Native_SetNone(out);
	if (!m) {
		return 0;
	}
	Native_FromPyRef(m, out);
	Py_DECREF(m);
	return 1;
}

/* ---- OBJLIB 9: native data crosses to Python ---- */

static PyObject *Obj_ToPy(nativeObj_t *o);

static PyObject *Obj_Conv(const nativeVal_t *v, void *ctx) {
	(void)ctx;
	return Native_ToPy(v);
}

/* the C function object of a callback calls it on the arguments as NObj_FromPy gives them (the keywords as (name,
 * value) pairs in the order of the dict, for a callback that takes them) */
static PyObject *Obj_Tramp(PyObject *self, PyObject *args, PyObject *kw) {
	nativeFn_t *f = PyCObject_AsVoidPtr(self);
	int n = PyTuple_Size(args);
	/* the keywords only when the C function object was made with METH_KEYWORDS (else Python passes two arguments) */
	kw = f->def.ml_flags & METH_KEYWORDS ? kw : NULL;
	nativeVal_t *a = Mem_Alloc((uint32_t)((size_t)(n + 1) * sizeof(*a))), r, pairs;
	for (int i = 0; i < n; i++) {
		NObj_FromPy(PyTuple_GET_ITEM(args, i), &a[i]);
	}
	Native_SetNone(&r);
	Native_SetEmpty(&pairs, NV_LIST);
	if (kw) {
		PyObject *k, *x;
		int pos = 0;
		while (PyDict_Next(kw, &pos, &k, &x)) {
			nativeVal_t *pair = Native_Append(&pairs);
			Native_SetEmpty(pair, NV_TUPLE);
			NObj_FromPy(k, Native_Append(pair));
			NObj_FromPy(x, Native_Append(pair));
		}
	}
	byte ok = f->callkw && kw ? f->callkw(f->ctx, a, n, &pairs, &r) : f->call(f->ctx, a, n, &r);
	Native_Free(&pairs);
	for (int i = 0; i < n; i++) {
		Native_Free(&a[i]);
	}
	Mem_Free(a);
	PyObject *res = ok ? Native_ToPy(&r) : NULL;
	Native_Free(&r);
	if (!ok && !Native_Failed()) {
		Native_Raise(NE_SYSTEM, "NULL result without error in call_object");
	}
	return res;
}

static void Obj_TrampFree(void *p) {
	Obj_FnDrop(p);
}

/* the method definition of a port function for a C function object of its own: one per ref, kept for good (a C
 * function object points at it as long as it lives) */
static PyMethodDef *Obj_PortDef(const portRef_t *ref) {
	typedef struct portDef_s {
		const portRef_t *ref;
		PyMethodDef def;
		struct portDef_s *next;
	} portDef_t;
	static portDef_t *defs;
	for (portDef_t *d = defs; d; d = d->next) {
		if (d->ref == ref) {
			return &d->def;
		}
	}
	portDef_t *d = Mem_Alloc(sizeof(*d));
	memset(d, 0, sizeof(*d));
	d->ref = ref;
	d->def.ml_name = (char *)ref->name;
	d->def.ml_meth = ref->adapter;
	d->def.ml_flags = METH_VARARGS | (ref->kw ? METH_KEYWORDS : 0);
	d->next = defs;
	defs = d;
	return &d->def;
}

/* the Python object of a native function */
static PyObject *Obj_FuncToPy(nativeObj_t *o) {
	switch (o->func) {
	case NF_PORT: {
		/* the function object the module holds while it is the port's own and no other native object is its view */
		modVarRef_t mv;
		nativeVal_t cur;
		PyObject *f = NULL;
		if (ModVar_Named(o->ref->module, &mv) && ModVar_GetVal(&mv, o->ref->name, &cur)) {
			/* the object the module's value has (no conversion: the value may be this function itself) */
			f = NObj_Py(&cur);
			Py_XINCREF(f);
			Native_Free(&cur);
		}
		if (f && Port_IsOwn(o->ref, f) && !Obj_TableFind(f)) {
			return f;
		}
		Py_XDECREF(f);
		return PyCFunction_New(Obj_PortDef(o->ref), NULL);
	}
	case NF_CALLBACK: {
		nativeFn_t *f = o->cb;
		f->def.ml_name = f->name;
		f->def.ml_meth = (PyCFunction)(void (*)(void))Obj_Tramp;
		f->def.ml_flags = METH_VARARGS | (f->callkw ? METH_KEYWORDS : 0);
		PyObject *self = PyCObject_FromVoidPtr(f, Obj_TrampFree);
		if (!self) {
			return NULL;
		}
		f->refs++;
		PyObject *r = PyCFunction_New(&f->def, self);
		Py_DECREF(self);
		return r;
	}
	}
	/* a method of a list or dict: the one of the Python object */
	PyObject *self = Obj_ToPy(o->self);
	PyObject *r = self ? PyObject_GetAttrString(self, o->name) : NULL;
	Py_XDECREF(self);
	return r;
}

/* the tuple of the Python classes of the bases */
static PyObject *Obj_BasesToPy(const nativeVal_t *bases) {
	PyObject *t = PyTuple_New(bases->n);
	for (int i = 0; t && i < bases->n; i++) {
		PyObject *b = Native_ToPy(&bases->items[i]);
		if (!b) {
			Py_DECREF(t);
			return NULL;
		}
		PyTuple_SET_ITEM(t, i, b);
	}
	return t;
}

/* o becomes the view of the new Python object py (given away) */
static void Obj_Crossed(nativeObj_t *o, PyObject *py) {
	o->py = py;
	Obj_TableAdd(o);
	obj_crossed++;
}

/* a crossing that failed: the Python object is dropped, o is native data as before */
static void Obj_Uncross(nativeObj_t *o) {
	PyObject *py = o->py;
	Obj_TableRemove(o);
	o->py = NULL;
	obj_crossed--;
	Py_DECREF(py);
}

/* the Python object of o (a new reference), made from its native data the first time; NULL with the error */
static PyObject *Obj_ToPy(nativeObj_t *o) {
	PyObject *py = NULL, *a = NULL, *b = NULL, *c = NULL;
	if (o->py) {
		Py_INCREF(o->py);
		return o->py;
	}
	switch (o->kind) {
	case NO_LIST:
		/* the list is known before its items are made: one that holds itself finds it */
		py = PyList_New(o->data.n);
		if (!py) {
			return NULL;
		}
		Obj_Crossed(o, py);
		for (int i = 0; i < o->data.n; i++) {
			PyObject *x = Native_ToPy(&o->data.items[i]);
			if (!x) {
				/* the items left stay NULL in the Python list: it is dropped with the error */
				Obj_Uncross(o);
				return NULL;
			}
			PyList_SET_ITEM(py, i, x);
		}
		break;
	case NO_DICT:
		/* the same for a dict, with the table of the native one */
		py = PyDict_New();
		if (!py) {
			return NULL;
		}
		Obj_Crossed(o, py);
		if (!Native_FillPyDict(py, &o->data, Obj_Conv, NULL)) {
			Obj_Uncross(o);
			return NULL;
		}
		break;
	case NO_INSTANCE: {
		/* the instance as PyInstance_New makes it, without the call of __init__; it is known before its dict is made
		 * (an attribute that holds the instance finds it) */
		a = Obj_ToPy(o->cls);
		b = a ? PyDict_New() : NULL;
		PyInstanceObject *inst = b ? PyObject_NEW(PyInstanceObject, &PyInstance_Type) : NULL;
		if (!inst) {
			Py_XDECREF(a);
			Py_XDECREF(b);
			return NULL;
		}
		inst->in_class = (PyClassObject *)a;
		inst->in_dict = b;
		Obj_Crossed(o, (PyObject *)inst);
		c = Obj_ToPy(o->dict);
		if (!c) {
			Obj_Uncross(o);
			return NULL;
		}
		inst->in_dict = c;
		Py_DECREF(b);
		break;
	}
	case NO_CLASS:
		/* the same for a class: made with an empty dict, then given its own */
		a = Obj_BasesToPy(&o->data);
		b = a ? PyDict_New() : NULL;
		c = b ? PyString_FromString(o->name) : NULL;
		py = c ? PyClass_New(a, b, c) : NULL;
		Py_XDECREF(a);
		Py_XDECREF(b);
		Py_XDECREF(c);
		if (!py) {
			return NULL;
		}
		Obj_Crossed(o, py);
		c = Obj_ToPy(o->dict);
		if (!c || PyObject_SetAttrString(py, "__dict__", c) != 0) {
			Py_XDECREF(c);
			Obj_Uncross(o);
			return NULL;
		}
		Py_DECREF(c);
		break;
	case NO_FUNCTION:
		py = Obj_FuncToPy(o);
		if (!py) {
			return NULL;
		}
		if (o->py) {
			/* made on the way (a method of a list that holds the method) */
			Py_DECREF(py);
		} else {
			Obj_Crossed(o, py);
		}
		/* a function keeps what it calls */
		Py_INCREF(o->py);
		return o->py;
	case NO_METHOD:
		a = Obj_ToPy(o->fn);
		b = a && o->self ? Obj_ToPy(o->self) : NULL;
		c = !a || (!b && o->self) ? NULL : o->cls ? Obj_ToPy(o->cls) : (Py_INCREF(Py_None), Py_None);
		py = c ? PyMethod_New(a, b, c) : NULL;
		Py_XDECREF(a);
		Py_XDECREF(b);
		Py_XDECREF(c);
		if (!py) {
			return NULL;
		}
		if (o->py) {
			/* made on the way (an instance that holds its own method) */
			Py_DECREF(py);
			Py_INCREF(o->py);
			return o->py;
		}
		Obj_Crossed(o, py);
		break;
	default:
		Native_Raise(NE_SYSTEM, "a native module without its Python object");
		return NULL;
	}
	/* the Python object is the master from here on */
	Obj_Clear(o);
	Py_INCREF(o->py);
	return o->py;
}

PyObject *NObj_ToPy(const nativeVal_t *v) {
	return Obj_ToPy(v->ref);
}

PyObject *NObj_Py(const nativeVal_t *v) {
	return v->type == NV_REF ? v->ref->py : v->type == NV_OBJ ? v->obj : NULL;
}

nativeVal_t *NObj_DictData(const nativeVal_t *d) {
	return d->type == NV_REF && d->ref->kind == NO_DICT && !d->ref->py ? &d->ref->data : NULL;
}

/* NObj_Plain of a value at a depth inside a container (as Native_FromPyAt) */
static byte Obj_Plain(const nativeVal_t *v, nativeVal_t *out, int depth) {
	const nativeVal_t *k, *x;
	int pos = 0;
	Native_SetNone(out);
	switch (v->type) {
	case NV_NONE:
	case NV_INT:
	case NV_FLOAT:
	case NV_STR:
	case NV_LONG:
		Native_Copy(out, v);
		Py_XDECREF(out->obj);
		out->obj = NULL;
		return 1;
	case NV_OBJ:
		return Native_FromPyAt(v->obj, out, depth);
	case NV_ENT:
		return 0;
	case NV_REF:
		if (v->ref->py) {
			return Native_FromPyAt(v->ref->py, out, depth);
		}
		if (v->ref->kind != NO_LIST && v->ref->kind != NO_DICT) {
			return 0;
		}
		v = &v->ref->data;
		break;
	}
	if (depth >= NATIVE_DEPTH) {
		return 0;
	}
	switch (v->type) {
	case NV_VEC:
		Native_SetEmpty(out, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetFloat(Native_Append(out), v->v[i]);
		}
		return 1;
	case NV_LIST:
	case NV_TUPLE:
		Native_SetEmpty(out, v->type);
		for (int i = 0; i < v->n; i++) {
			if (!Obj_Plain(&v->items[i], Native_Append(out), depth + 1)) {
				Native_Free(out);
				return 0;
			}
		}
		return 1;
	case NV_DICT:
		/* the table as it is: Python's dict made from it has it, and Native_FromPyIn copies the slots of that */
		Native_Copy(out, v);
		while (Native_DictNext(out, &pos, &k, &x)) {
			nativeVal_t pk, px;
			if (!Obj_Plain(k, &pk, depth + 1)) {
				Native_Free(out);
				return 0;
			}
			if (!Obj_Plain(x, &px, depth + 1)) {
				Native_Free(&pk);
				Native_Free(out);
				return 0;
			}
			/* the slots of out itself */
			Native_Free((nativeVal_t *)k);
			*(nativeVal_t *)k = pk;
			Native_Free((nativeVal_t *)x);
			*(nativeVal_t *)x = px;
		}
		return 1;
	}
	return 0;
}

byte NObj_Plain(const nativeVal_t *v, nativeVal_t *out) {
	return Obj_Plain(v, out, 0);
}

void NObj_ArgOf(const nativeVal_t *v, nativeVal_t *out) {
	if (v->type != NV_ENT && Obj_Plain(v, out, 0)) {
		return;
	}
	if (v->type == NV_OBJ) {
		Native_SetObj(out, v->obj);
		return;
	}
	if (v->type == NV_ENT || (v->type == NV_REF && NObj_RefMode())) {
		/* the same handle or object */
		Native_Copy(out, v);
		Py_XDECREF(out->obj);
		out->obj = NULL;
		return;
	}
	/* a tuple, list or dict that holds an object or a handle: as Python has it (a tuple keeps its object) */
	PyObject *o = v->type == NV_TUPLE && v->obj && PyTuple_Check(v->obj) ? (Py_INCREF(v->obj), v->obj) : Native_ToPy(v);
	if (o) {
		Native_SetObj(out, o);
		Py_DECREF(o);
	} else {
		Native_ErrClear();
	}
}

/* ---- classes and instances (classobject.c) ---- */

static const char *Obj_ClassName(const nativeObj_t *c) {
	return c->name ? c->name : "?";
}

/* base is c or a base class of c at any depth (PyClass_IsSubclass) */
static byte Obj_IsSubclass(const nativeObj_t *c, const nativeObj_t *base) {
	if (c == base) {
		return 1;
	}
	if (c->py) {
		return base->py && PyClass_IsSubclass(c->py, base->py);
	}
	for (int i = 0; i < c->data.n; i++) {
		if (Obj_IsSubclass(c->data.items[i].ref, base)) {
			return 1;
		}
	}
	return 0;
}

/* the instance is one of the class cls or of a class derived from it */
static byte Obj_InstanceOf(const nativeObj_t *inst, const nativeObj_t *cls) {
	if (inst->py) {
		return cls->py && PyClass_IsSubclass((PyObject *)((PyInstanceObject *)inst->py)->in_class, cls->py);
	}
	return Obj_IsSubclass(inst->cls, cls);
}

/* class_lookup: the attribute name of the class c or of its bases, depth first: 1 with *out a copy and *found the
 * class that has it (held: NObj_Drop it), else 0 */
static byte Obj_ClassLook(nativeObj_t *c, const char *name, nativeVal_t *out, nativeObj_t **found) {
	if (c->py) {
		/* a class that crossed to Python: its dict and bases there */
		PyClassObject *pc = (PyClassObject *)c->py;
		PyObject *x = PyDict_GetItemString(pc->cl_dict, (char *)name);
		if (x) {
			NObj_FromPy(x, out);
			*found = NObj_Keep(c);
			return 1;
		}
		for (int i = 0; i < PyTuple_Size(pc->cl_bases); i++) {
			nativeVal_t base;
			if (Native_FromPyRef(PyTuple_GET_ITEM(pc->cl_bases, i), &base)) {
				byte ok = base.ref->kind == NO_CLASS && Obj_ClassLook(base.ref, name, out, found);
				Native_Free(&base);
				if (ok) {
					return 1;
				}
			}
		}
		return 0;
	}
	if (Obj_DictLook(c->dict, name, out)) {
		*found = NObj_Keep(c);
		return 1;
	}
	for (int i = 0; i < c->data.n; i++) {
		if (Obj_ClassLook(c->data.items[i].ref, name, out, found)) {
			return 1;
		}
	}
	return 0;
}

/* *v the method object of the function fn for cls, bound to self (NULL: unbound); fn, self and cls are kept */
static void Obj_Method(nativeVal_t *v, nativeObj_t *fn, nativeObj_t *self, nativeObj_t *cls) {
	nativeObj_t *o = Obj_New(NO_METHOD);
	o->fn = NObj_Keep(fn);
	o->cls = NObj_Keep(cls);
	o->self = self ? NObj_Keep(self) : NULL;
	Obj_SetRef(v, o);
}

/* the function of a method object and its class as held native objects (of a view: the views of its parts): 1 */
static byte Obj_MethodParts(nativeObj_t *m, nativeObj_t **fn, nativeObj_t **cls) {
	if (m->py) {
		nativeVal_t f, c;
		byte ok = Native_FromPyRef(PyMethod_GET_FUNCTION(m->py), &f);
		if (ok && !Native_FromPyRef(PyMethod_GET_CLASS(m->py), &c)) {
			Native_Free(&f);
			ok = 0;
		}
		if (!ok) {
			return 0;
		}
		*fn = f.ref;
		*cls = c.ref;
		return 1;
	}
	if (!m->cls) {
		/* an unbound method whose class is not made yet (NObj_NewMethod) */
		return 0;
	}
	*fn = NObj_Keep(m->fn);
	*cls = NObj_Keep(m->cls);
	return 1;
}

/* a value found in the class found, as the attribute of the instance inst (NULL: of the class) gives it: a function
 * of Python code becomes a method, a method of a class the instance is of is bound to the instance; in place */
static void Obj_Bind(nativeVal_t *v, nativeObj_t *inst, nativeObj_t *found) {
	if (v->type != NV_REF) {
		return;
	}
	nativeObj_t *x = v->ref;
	if (x->kind == NO_FUNCTION && x->binds) {
		nativeVal_t m;
		Obj_Method(&m, x, inst, found);
		Native_Free(v);
		*v = m;
	} else if (x->kind == NO_METHOD && inst) {
		nativeObj_t *fn, *cls;
		if (!Obj_MethodParts(x, &fn, &cls)) {
			return;
		}
		if (cls->kind == NO_CLASS && fn->kind == NO_FUNCTION && Obj_IsSubclass(found, cls)) {
			nativeVal_t m;
			Obj_Method(&m, fn, inst, cls);
			Native_Free(v);
			*v = m;
		}
		NObj_Drop(fn);
		NObj_Drop(cls);
	}
}

/* instance_getattr1: 1 with *out, 0 without an error when the instance has no such attribute */
static byte Obj_InstLook(nativeObj_t *o, const char *name, nativeVal_t *out) {
	if (name[0] == '_' && name[1] == '_') {
		if (!strcmp(name, "__dict__")) {
			Obj_SetRef(out, NObj_Keep(o->dict));
			return 1;
		}
		if (!strcmp(name, "__class__")) {
			Obj_SetRef(out, NObj_Keep(o->cls));
			return 1;
		}
	}
	if (Obj_DictLook(o->dict, name, out)) {
		return 1;
	}
	nativeObj_t *found;
	if (!Obj_ClassLook(o->cls, name, out, &found)) {
		return 0;
	}
	Obj_Bind(out, o, found);
	NObj_Drop(found);
	return 1;
}

/* the special method name of a value (an instance's attribute): 1 with *out, 0 without an error */
static byte Obj_Special(const nativeVal_t *v, const char *name, nativeVal_t *out) {
	return Obj_IsKind(v, NO_INSTANCE) && !v->ref->py && Obj_InstLook(v->ref, name, out);
}

/* ---- texts ---- */

static void Obj_Addf(nativeBuf_t *b, const char *fmt, ...) {
	char t[0x200];
	va_list va;
	va_start(va, fmt);
	vsnprintf(t, sizeof(t), fmt, va);
	va_end(va);
	Native_BufAdd(b, t, -1);
}

/* the text of a value of a class dict when it is a text, else NULL */
static const char *Obj_ClassText(const nativeObj_t *c, const char *name, nativeVal_t *tmp) {
	Native_SetNone(tmp);
	if (c->py || !Obj_DictLook(c->dict, name, tmp) || tmp->type != NV_STR) {
		return NULL;
	}
	return Native_Text(tmp);
}

/* the repr or str of a Python object added to b: 1, 0 with the error */
static byte Obj_PyRepr(nativeBuf_t *b, PyObject *py, nativeRepr_t mode) {
	PyObject *r = mode == NR_STR ? PyObject_Str(py) : PyObject_Repr(py);
	if (!r) {
		return 0;
	}
	Native_BufAdd(b, PyString_AsString(r), PyString_Size(r));
	Py_DECREF(r);
	return 1;
}

byte NObj_Repr(nativeBuf_t *b, const nativeVal_t *v, nativeRepr_t mode) {
	if (v->type != NV_REF) {
		Native_Repr(b, v, mode);
		return 1;
	}
	nativeObj_t *o = v->ref;
	nativeVal_t tmp, f, r;
	const char *mod;
	if (Obj_View(o)) {
		return Obj_PyRepr(b, o->py, mode);
	}
	switch (o->kind) {
	case NO_LIST:
	case NO_DICT:
		/* the items as the value by value prints them */
		Native_Repr(b, &o->data, mode);
		return 1;
	case NO_INSTANCE: {
		/* instance_repr, and for str the __str__ of PyObject_Str first */
		byte special = (mode == NR_STR && Obj_Special(v, "__str__", &f)) || Obj_Special(v, "__repr__", &f);
		if (special) {
			byte ok = NObj_Call(&f, NULL, 0, &r);
			Native_Free(&f);
			if (ok && r.type != NV_STR) {
				Native_Raise(NE_TYPE, "__repr__ returned non-string (type %.80s)", Native_TypeName(&r));
				ok = 0;
			}
			if (ok) {
				Native_BufAdd(b, Native_Text(&r), r.n);
			}
			Native_Free(&r);
			return ok;
		}
		mod = Obj_ClassText(o->cls, "__module__", &tmp);
		if (o->cls->py) {
			/* an instance of a class that crossed: the names from the Python class */
			PyClassObject *pc = (PyClassObject *)o->cls->py;
			PyObject *m = PyDict_GetItemString(pc->cl_dict, "__module__");
			const char *cn = pc->cl_name && PyString_Check(pc->cl_name) ? PyString_AsString(pc->cl_name) : "?";
			if (m && PyString_Check(m)) {
				Obj_Addf(b, "<%.50s.%.50s instance at %lx>", PyString_AsString(m), cn, (long)Obj_Addr(o));
			} else {
				Obj_Addf(b, "<?.%.100s instance at %lx>", cn, (long)Obj_Addr(o));
			}
		} else if (mod) {
			Obj_Addf(b, "<%.50s.%.50s instance at %lx>", mod, Obj_ClassName(o->cls), (long)Obj_Addr(o));
		} else {
			Obj_Addf(b, "<?.%.100s instance at %lx>", Obj_ClassName(o->cls), (long)Obj_Addr(o));
		}
		Native_Free(&tmp);
		return 1;
	}
	case NO_CLASS:
		mod = Obj_ClassText(o, "__module__", &tmp);
		if (mode == NR_STR && o->name) {
			/* class_str: module.name */
			if (mod) {
				Native_BufAdd(b, mod, tmp.n);
				Native_BufAdd(b, ".", 1);
			}
			Native_BufAdd(b, o->name, -1);
		} else if (mod) {
			Obj_Addf(b, "<class %.50s.%.50s at %lx>", mod, Obj_ClassName(o), (long)Obj_Addr(o));
		} else {
			Obj_Addf(b, "<class ?.%.100s at %lx>", Obj_ClassName(o), (long)Obj_Addr(o));
		}
		Native_Free(&tmp);
		return 1;
	case NO_FUNCTION:
		if (o->func == NF_BUILTIN) {
			/* meth_repr with a self */
			Obj_Addf(b, "<built-in method %.80s of %.80s object at %lx>", o->name,
				o->self->kind == NO_LIST ? "list" : "dictionary", (long)Obj_Addr(o->self));
		} else if (o->binds) {
			Obj_Addf(b, "<function %.100s at %lx>", o->name, (long)Obj_Addr(o));
		} else if (o->func == NF_CALLBACK && o->cb->self.type != NV_NONE) {
			/* meth_repr with a self: the address of the callback for the self's (a value by value has none) */
			Obj_Addf(b, "<built-in method %.80s of %.80s object at %lx>", o->name, Native_TypeName(&o->cb->self),
				(long)(uintptr_t)o->cb);
		} else {
			Obj_Addf(b, "<built-in function %.80s>", o->name);
		}
		return 1;
	case NO_MODULE: {
		/* module_repr, the name and file of the module dict (the registry's variables) */
		modVarRef_t mv;
		nativeVal_t name, file;
		Native_SetNone(&name);
		Native_SetNone(&file);
		if (ModVar_Of(o->py, &mv)) {
			ModVar_GetVal(&mv, "__name__", &name);
			ModVar_GetVal(&mv, "__file__", &file);
		}
		const char *n = name.type == NV_STR ? Native_Text(&name) : "?";
		if (file.type == NV_STR) {
			Obj_Addf(b, "<module '%.50s' from '%.80s'>", n, Native_Text(&file));
		} else {
			Obj_Addf(b, "<module '%.80s'>", n);
		}
		Native_Free(&name);
		Native_Free(&file);
		return 1;
	}
	case NO_METHOD: {
		/* instancemethod_repr */
		char fname[0x80] = "?";
		if (o->fn->name) {
			Str_Copy(fname, o->fn->name, sizeof(fname));
		} else if (o->fn->py) {
			PyObject *n = PyObject_GetAttrString(o->fn->py, "__name__");
			if (n && PyString_Check(n)) {
				Str_Copy(fname, PyString_AsString(n), sizeof(fname));
			}
			Py_XDECREF(n);
			Native_ErrClear();
		}
		if (!o->self) {
			Obj_Addf(b, "<unbound method %.100s.%.100s>", o->cls ? Obj_ClassName(o->cls) : "?", fname);
		} else if (o->self->py) {
			PyObject *n = ((PyInstanceObject *)o->self->py)->in_class->cl_name;
			Obj_Addf(b, "<method %.60s.%.60s of %.60s instance at %lx>", Obj_ClassName(o->cls), fname,
				n && PyString_Check(n) ? PyString_AsString(n) : "?", (long)Obj_Addr(o->self));
		} else {
			Obj_Addf(b, "<method %.60s.%.60s of %.60s instance at %lx>", Obj_ClassName(o->cls), fname,
				Obj_ClassName(o->self->cls), (long)Obj_Addr(o->self));
		}
		return 1;
	}
	}
	return 1;
}

const char *NObj_TypeName(const nativeVal_t *v) {
	static const char *const names[] = {
		[NO_LIST] = "list",
		[NO_DICT] = "dictionary",
		[NO_INSTANCE] = "instance",
		[NO_CLASS] = "class",
		[NO_FUNCTION] = "builtin_function_or_method",
		[NO_METHOD] = "instance method",
		[NO_MODULE] = "module",
	};
	const nativeObj_t *o = v->ref;
	if (Obj_View(o)) {
		return o->py->ob_type->tp_name;
	}
	return o->kind == NO_FUNCTION && o->binds ? "function" : names[o->kind];
}

/* ---- truth, compare, hash ---- */

byte NObj_True(const nativeVal_t *v) {
	const nativeObj_t *o = v->ref;
	if (Obj_View(o)) {
		int t = PyObject_IsTrue(o->py);
		Native_ErrClear();
		return t > 0;
	}
	/* a list or dict by its length; an instance is true (no __nonzero__ and __len__ of native instances) */
	return o->kind == NO_LIST || o->kind == NO_DICT ? o->data.n != 0 : 1;
}

static int Obj_Sign(long c) {
	return c < 0 ? -1 : c > 0;
}

static int Obj_AddrOrder(uintptr_t a, uintptr_t b) {
	return a == b ? 0 : a < b ? -1 : 1;
}

/* two native function objects (meth_compare; a function of Python code by its address) */
static int Obj_FuncCompare(const nativeObj_t *a, const nativeObj_t *b) {
	if (a->binds || b->binds || a->func != b->func) {
		return Obj_AddrOrder(Obj_Addr(a), Obj_Addr(b));
	}
	switch (a->func) {
	case NF_PORT:
		if (a->ref->adapter == b->ref->adapter) {
			return 0;
		}
		break;
	case NF_CALLBACK:
		if (a->cb == b->cb) {
			return 0;
		}
		break;
	default:
		if (a->self != b->self) {
			return Obj_AddrOrder(Obj_Addr(a->self), Obj_Addr(b->self));
		}
		if (a->method == b->method) {
			return 0;
		}
	}
	return strcmp(a->name, b->name) < 0 ? -1 : 1;
}

/* the name a value has in the order of values of different types: the numbers before all others */
static const char *Obj_OrderName(const nativeVal_t *v) {
	return v->type == NV_INT || v->type == NV_FLOAT || v->type == NV_LONG ? "" : Native_TypeName(v);
}

int NObj_Compare(const nativeVal_t *a, const nativeVal_t *b) {
	nativeVal_t ta, tb;
	int c;
	if (a->type == NV_REF && b->type == NV_REF && a->ref == b->ref) {
		return 0;
	}
	/* two Python objects, each a view or an NV_OBJ (the same object can come as either), as Python compares them */
	PyObject *pa = a->type == NV_REF ? Obj_View(a->ref) : a->type == NV_OBJ ? a->obj : NULL;
	PyObject *pb = b->type == NV_REF ? Obj_View(b->ref) : b->type == NV_OBJ ? b->obj : NULL;
	if (pa && pb) {
		c = PyObject_Compare(pa, pb);
		Native_ErrClear();
		return Obj_Sign(c);
	}
	/* a list with a list and a dict with a dict by their items, by value or as objects */
	for (int pass = 0; pass < 2; pass++) {
		byte type = pass ? NV_DICT : NV_LIST;
		const nativeVal_t *x = Obj_Items(a, type, &ta), *y = x ? Obj_Items(b, type, &tb) : NULL;
		if (y) {
			/* the items of two different objects, never the two NV_REF again */
			c = Native_Compare(x, y);
			Native_Free(&ta);
			Native_Free(&tb);
			return c;
		}
		if (x) {
			Native_Free(&ta);
		}
	}
	c = strcmp(Obj_OrderName(a), Obj_OrderName(b));
	if (c || a->type != NV_REF || b->type != NV_REF) {
		/* by the names of the types (an instance and a value of another type compare by their addresses in 1.5.2,
		 * which a value by value does not have) */
		return c ? Obj_Sign(c) : a->type == NV_REF ? 1 : b->type == NV_REF ? -1 : Native_Compare(a, b);
	}
	const nativeObj_t *x = a->ref, *y = b->ref;
	if (x->kind == NO_FUNCTION && y->kind == NO_FUNCTION && !Obj_View(x) && !Obj_View(y)) {
		return Obj_FuncCompare(x, y);
	}
	if (x->kind == NO_METHOD && y->kind == NO_METHOD && !x->py && !y->py) {
		/* instancemethod_compare */
		if (x->self != y->self) {
			return Obj_AddrOrder(x->self ? Obj_Addr(x->self) : 0, y->self ? Obj_Addr(y->self) : 0);
		}
		nativeVal_t fx, fy;
		Obj_SetRef(&fx, x->fn);
		Obj_SetRef(&fy, y->fn);
		return NObj_Compare(&fx, &fy);
	}
	/* objects without a compare: by their addresses */
	return Obj_AddrOrder(Obj_Addr(x), Obj_Addr(y));
}

byte NObj_Equal(const nativeVal_t *a, const nativeVal_t *b) {
	return NObj_Compare(a, b) == 0;
}

byte NObj_HashQuiet(const nativeVal_t *v, long *out) {
	const nativeObj_t *o = v->ref;
	long x = 0, y;
	nativeVal_t t;
	if (!o->born) {
		/* a view from the start: the hash of the Python object (the address, or its __hash__) */
		x = PyObject_Hash(o->py);
		if (x == -1) {
			Native_ErrClear();
			return 0;
		}
		*out = x;
		return 1;
	}
	/* native data keeps the hash of the native object when it crosses */
	switch (o->kind) {
	case NO_LIST:
	case NO_DICT:
		return 0;
	case NO_FUNCTION:
		/* meth_hash: the hash of self xor the address of the function */
		if (o->binds) {
			x = (long)(intptr_t)o;
		} else if (o->func == NF_PORT) {
			x = (long)(intptr_t)o->ref->adapter;
		} else if (o->func == NF_CALLBACK) {
			x = (long)(intptr_t)o->cb;
		} else {
			return 0;
		}
		break;
	case NO_METHOD:
		/* instancemethod_hash */
		if (o->self) {
			Obj_SetRef(&t, o->self);
		} else {
			Native_SetNone(&t);
		}
		if (!Native_Hash(&t, &x)) {
			return 0;
		}
		Obj_SetRef(&t, o->fn);
		if (!Native_Hash(&t, &y)) {
			return 0;
		}
		x ^= y;
		break;
	case NO_MODULE:
		/* the address of its module object, as Python hashes it */
		x = (long)Obj_Addr(o);
		break;
	default:
		x = (long)(intptr_t)o;
	}
	*out = x == -1 ? -2 : x;
	return 1;
}

byte NObj_Hash(const nativeVal_t *v, long *out) {
	PyObject *py = Obj_Forward(v);
	if (py) {
		*out = PyObject_Hash(py);
		Py_DECREF(py);
		return *out != -1;
	}
	if (Native_Hash(v, out)) {
		return 1;
	}
	return Native_Raise(NE_TYPE, "unhashable type");
}

/* ---- the methods of lists and dicts (listobject.c, dictobject.c) ---- */

/* the errors of PyArg_Parse (the old argument form: no arguments, one object, else their tuple) and of
 * PyErr_BadArgument */
static byte Obj_NeedArg(void) {
	return Native_Raise(NE_TYPE, "function requires at least one argument");
}

static byte Obj_NoArgs(int n) {
	return n == 0 || Native_Raise(NE_TYPE, "function requires no arguments");
}

static byte Obj_BadArg(void) {
	return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
}

/* the one argument object of a method of the old form: a[0], or the tuple of the n arguments in *tmp (Native_Free
 * tmp after) */
static const nativeVal_t *Obj_OneArg(const nativeVal_t *a, int n, nativeVal_t *tmp) {
	Native_SetNone(tmp);
	if (n == 1) {
		return a;
	}
	Native_SetEmpty(tmp, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_Copy(Native_Append(tmp), &a[i]);
	}
	return tmp;
}

/* a list that is being sorted takes no change */
static byte Obj_ListFree(const nativeObj_t *o) {
	return !o->busy || Native_Raise(NE_TYPE, "a list cannot be modified while it is being sorted");
}

/* ins1: v before the item where (clamped) */
static void Obj_ListInsert(nativeObj_t *o, long where, const nativeVal_t *v) {
	nativeVal_t *list = &o->data, x;
	Obj_Store(&x, v);
	int n = list->n;
	Native_Append(list);
	where = where < 0 ? 0 : where > n ? n : where;
	memmove(&list->items[where + 1], &list->items[where], (size_t)(n - where) * sizeof(nativeVal_t));
	list->items[where] = x;
}

/* list_ass_slice with the bounds clamped: the items lo to hi replaced by copies of the count items at src (which
 * are not in this list) */
static void Obj_ListSplice(nativeObj_t *o, long lo, long hi, const nativeVal_t *src, int count) {
	nativeVal_t *list = &o->data;
	int n = list->n, d = count - (int)(hi - lo);
	nativeVal_t *old = Mem_Alloc((uint32_t)((size_t)(hi - lo + 1) * sizeof(nativeVal_t)));
	memcpy(old, &list->items[lo], (size_t)(hi - lo) * sizeof(nativeVal_t));
	for (int i = 0; i < d; i++) {
		Native_Append(list);
	}
	memmove(&list->items[hi + d], &list->items[hi], (size_t)(n - hi) * sizeof(nativeVal_t));
	list->n = n + d;
	for (int i = 0; i < count; i++) {
		Obj_Store(&list->items[lo + i], &src[i]);
	}
	/* the old items go when the list is whole again */
	for (int i = 0; i < hi - lo; i++) {
		Native_Free(&old[i]);
	}
	Mem_Free(old);
}

/* the number of a method argument as the "i" of PyArg_Parse reads it (PyInt_AsLong: a float cut): 0 with its error */
static byte Obj_ArgLong(const nativeVal_t *v, long *out) {
	char *end;
	switch (v->type) {
	case NV_INT:
		*out = v->i;
		return 1;
	case NV_FLOAT:
		*out = (long)v->f;
		return 1;
	case NV_LONG:
		errno = 0;
		*out = strtol(Native_Text(v), &end, 10);
		return (!errno && !*end) || Native_Raise(NE_OVERFLOW, "long int too long to convert");
	}
	return Obj_BadArg();
}

/* the error of an argument of the old form (convertsimple without an argument number): "<expected>, <type>" */
static byte Obj_Expected(const char *expected, const nativeVal_t *v) {
	return Native_Raise(NE_TYPE, "%.50s, %.50s", expected, Native_TypeName(v));
}

static byte Obj_ListAppend(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t tmp;
	(void)out;
	if (!n) {
		return Obj_NeedArg();
	}
	if (!Obj_ListFree(o)) {
		return 0;
	}
	Obj_ListInsert(o, o->data.n, Obj_OneArg(a, n, &tmp));
	Native_Free(&tmp);
	return 1;
}

static byte Obj_ListInsertM(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* "(iO)" */
	nativeVal_t tmp;
	long where;
	(void)out;
	if (!n) {
		return Obj_NeedArg();
	}
	/* converttuple: the one argument is any sequence of two items */
	const nativeVal_t *t = Obj_OneArg(a, n, &tmp);
	nativeVal_t snap;
	const nativeVal_t *seq = t->type == NV_TUPLE ? t : Obj_Items(t, NV_LIST, &snap);
	int count = seq ? seq->n : t->type == NV_VEC ? 3 : t->type == NV_STR ? t->n : -1;
	byte ok;
	if (count < 0 && Obj_IsKind(t, NO_INSTANCE)) {
		ok = Native_Raise(NE_ATTRIBUTE, "__len__");
	} else if (count < 0) {
		ok = Obj_Expected("2-sequence", t);
	} else if (count != 2) {
		ok = Native_Raise(NE_TYPE, "2-sequence, %d-sequence", count);
	} else if (!seq) {
		/* a text of two characters: the first is no number */
		ok = Obj_BadArg();
	} else {
		ok = Obj_ArgLong(&seq->items[0], &where) && Obj_ListFree(o);
		if (ok) {
			Obj_ListInsert(o, where, &seq->items[1]);
		}
	}
	if (seq && seq != t) {
		Native_Free(&snap);
	}
	Native_Free(&tmp);
	return ok;
}

static byte Obj_ListExtend(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t tmp, copy;
	(void)out;
	if (n != 1) {
		return Port_Arity(n, 1);
	}
	const nativeVal_t *b = Obj_Items(a, NV_LIST, &tmp);
	if (!b) {
		return Native_Raise(NE_TYPE, "list.extend() argument must be a list");
	}
	if (!Obj_ListFree(o)) {
		Native_Free(&tmp);
		return 0;
	}
	/* a.extend(a): from a copy */
	Native_Copy(&copy, b);
	Obj_ListSplice(o, o->data.n, o->data.n, copy.items, copy.n);
	Native_Free(&copy);
	Native_Free(&tmp);
	return 1;
}

static byte Obj_ListPop(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	long i = -1;
	if (n > 1) {
		return Native_Raise(NE_TYPE, "function requires at most 1 argument; %d given", n);
	}
	if (n && !Obj_ArgLong(a, &i)) {
		return 0;
	}
	if (!o->data.n) {
		return Native_Raise(NE_INDEX, "pop from empty list");
	}
	if (i < 0) {
		i += o->data.n;
	}
	if (i < 0 || i >= o->data.n) {
		return Native_Raise(NE_INDEX, "pop index out of range");
	}
	if (!Obj_ListFree(o)) {
		return 0;
	}
	Native_Copy(out, &o->data.items[i]);
	Obj_ListSplice(o, i, i + 1, NULL, 0);
	return 1;
}

/* the index of the first item equal to the argument x, -1 when there is none */
static int Obj_ListFind(const nativeObj_t *o, const nativeVal_t *x) {
	for (int i = 0; i < o->data.n; i++) {
		if (!Native_Compare(&o->data.items[i], x)) {
			return i;
		}
	}
	return -1;
}

static byte Obj_ListRemove(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t tmp;
	(void)out;
	if (!n) {
		return Obj_BadArg();
	}
	int i = Obj_ListFind(o, Obj_OneArg(a, n, &tmp));
	Native_Free(&tmp);
	if (i < 0) {
		return Native_Raise(NE_VALUE, "list.remove(x): x not in list");
	}
	if (!Obj_ListFree(o)) {
		return 0;
	}
	Obj_ListSplice(o, i, i + 1, NULL, 0);
	return 1;
}

static byte Obj_ListIndex(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t tmp;
	if (!n) {
		return Obj_BadArg();
	}
	int i = Obj_ListFind(o, Obj_OneArg(a, n, &tmp));
	Native_Free(&tmp);
	if (i < 0) {
		return Native_Raise(NE_VALUE, "list.index(x): x not in list");
	}
	Native_SetInt(out, i);
	return 1;
}

static byte Obj_ListCount(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t tmp;
	int count = 0;
	if (!n) {
		return Obj_BadArg();
	}
	const nativeVal_t *x = Obj_OneArg(a, n, &tmp);
	for (int i = 0; i < o->data.n; i++) {
		count += !Native_Compare(&o->data.items[i], x);
	}
	Native_Free(&tmp);
	Native_SetInt(out, count);
	return 1;
}

static byte Obj_ListReverse(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (n) {
		return Obj_BadArg();
	}
	if (!Obj_ListFree(o)) {
		return 0;
	}
	for (int i = 0, j = o->data.n - 1; i < j; i++, j--) {
		nativeVal_t t = o->data.items[i];
		o->data.items[i] = o->data.items[j];
		o->data.items[j] = t;
	}
	return 1;
}

/* docompare: cmp(x, y), or the compare function called (its result an int): 0 with the error */
static byte Obj_SortCompare(const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *compare, int *k) {
	if (!compare) {
		*k = Native_Compare(x, y);
		return 1;
	}
	nativeVal_t args[2] = { *x, *y }, r;
	byte ok = NObj_Call(compare, args, 2, &r);
	if (ok && r.type != NV_INT) {
		ok = Native_Raise(NE_TYPE, "comparison function should return int");
	}
	*k = ok ? Obj_Sign(r.i) : 0;
	Native_Free(&r);
	return ok;
}

static byte Obj_ListSort(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	/* binarysort of listobject.c (what its samplesort does below 100 items; a longer list is sorted the same way
	 * here, which can order equal items otherwise than 1.5.2) */
	nativeVal_t tmp, *lo = o->data.items;
	int count = o->data.n;
	(void)out;
	if (!Obj_ListFree(o)) {
		return 0;
	}
	const nativeVal_t *compare = n ? Obj_OneArg(a, n, &tmp) : NULL;
	byte ok = 1;
	o->busy = 1;
	for (int start = 1; ok && start < count; start++) {
		int l = 0, r = start, k;
		nativeVal_t pivot = lo[start];
		do {
			int p = l + ((r - l) >> 1);
			ok = Obj_SortCompare(&pivot, &lo[p], compare, &k);
			if (k < 0) {
				r = p;
			} else {
				l = p + 1;
			}
		} while (ok && l < r);
		if (ok) {
			memmove(&lo[l + 1], &lo[l], (size_t)(start - l) * sizeof(nativeVal_t));
			lo[l] = pivot;
		}
	}
	o->busy = 0;
	if (n) {
		Native_Free(&tmp);
	}
	return ok;
}

static byte Obj_DictKeys(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *k, *x;
	int pos = 0;
	(void)a;
	if (!Obj_NoArgs(n)) {
		return 0;
	}
	Native_SetEmpty(out, NV_LIST);
	while (Native_DictNext(&o->data, &pos, &k, &x)) {
		Native_Copy(Native_Append(out), k);
	}
	return 1;
}

static byte Obj_DictValues(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *k, *x;
	int pos = 0;
	(void)a;
	if (!Obj_NoArgs(n)) {
		return 0;
	}
	Native_SetEmpty(out, NV_LIST);
	while (Native_DictNext(&o->data, &pos, &k, &x)) {
		Native_Copy(Native_Append(out), x);
	}
	return 1;
}

static byte Obj_DictItems(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *k, *x;
	int pos = 0;
	(void)a;
	if (!Obj_NoArgs(n)) {
		return 0;
	}
	Native_SetEmpty(out, NV_LIST);
	while (Native_DictNext(&o->data, &pos, &k, &x)) {
		nativeVal_t *pair = Native_Append(out);
		Native_SetEmpty(pair, NV_TUPLE);
		Native_Copy(Native_Append(pair), k);
		Native_Copy(Native_Append(pair), x);
	}
	return 1;
}

static byte Obj_DictHasKey(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t tmp;
	long h;
	if (!n) {
		return Obj_NeedArg();
	}
	const nativeVal_t *key = Obj_OneArg(a, n, &tmp);
	byte ok = NObj_Hash(key, &h);
	if (ok) {
		Native_SetInt(out, Native_DictGet(&o->data, key) != NULL);
	}
	Native_Free(&tmp);
	return ok;
}

static byte Obj_DictGetM(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	long h;
	if (n < 1) {
		return Native_Raise(NE_TYPE, "function requires at least 1 argument; %d given", n);
	}
	if (n > 2) {
		return Native_Raise(NE_TYPE, "function requires at most 2 arguments; %d given", n);
	}
	if (!NObj_Hash(&a[0], &h)) {
		return 0;
	}
	const nativeVal_t *x = Native_DictGet(&o->data, &a[0]);
	if (x) {
		Native_Copy(out, x);
	} else if (n == 2) {
		Native_Copy(out, &a[1]);
	}
	return 1;
}

static byte Obj_DictUpdate(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t tmp, arg;
	const nativeVal_t *k, *x;
	int pos = 0;
	(void)out;
	if (!n) {
		return Obj_NeedArg();
	}
	const nativeVal_t *t = Obj_OneArg(a, n, &arg);
	const nativeVal_t *other = Obj_Items(t, NV_DICT, &tmp);
	if (!other) {
		Obj_Expected("dictionary", t);
		Native_Free(&arg);
		return 0;
	}
	/* d.update(d) changes nothing */
	if (other != &o->data) {
		Native_DictMerge(&o->data, other, 0);
		/* a list or dict by value that came with a dict by value is held by this dict now */
		while (Native_DictNext(&o->data, &pos, &k, &x)) {
			NObj_Share((nativeVal_t *)x);
		}
	}
	Native_Free(&tmp);
	Native_Free(&arg);
	return 1;
}

static byte Obj_DictClear(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Obj_NoArgs(n)) {
		return 0;
	}
	nativeVal_t old = o->data;
	Native_SetEmpty(&o->data, NV_DICT);
	Native_Free(&old);
	return 1;
}

static byte Obj_DictCopy(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	if (!Obj_NoArgs(n)) {
		return 0;
	}
	Native_SetEmpty(out, NV_DICT);
	Native_DictMerge(out, &o->data, 1);
	return 1;
}

/* the methods by type, each type sorted by name (the order of __methods__) */
static const struct {
	byte kind;
	const char *name;
	byte (*fn)(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out);
	const char *doc;		/* __doc__ (the list methods of 1.5.2 have one) */
} obj_methods[] = {
	{ NO_LIST, "append", Obj_ListAppend, "L.append(object) -- append object to end" },
	{ NO_LIST, "count", Obj_ListCount, "L.count(value) -> integer -- return number of occurrences of value" },
	{ NO_LIST, "extend", Obj_ListExtend, "L.extend(list) -- extend list by appending list elements" },
	{ NO_LIST, "index", Obj_ListIndex, "L.index(value) -> integer -- return index of first occurrence of value" },
	{ NO_LIST, "insert", Obj_ListInsertM, "L.insert(index, object) -- insert object before index" },
	{ NO_LIST, "pop", Obj_ListPop, "L.pop([index]) -> item -- remove and return item at index (default last)" },
	{ NO_LIST, "remove", Obj_ListRemove, "L.remove(value) -- remove first occurrence of value" },
	{ NO_LIST, "reverse", Obj_ListReverse, "L.reverse() -- reverse *IN PLACE*" },
	{ NO_LIST, "sort", Obj_ListSort, "L.sort([cmpfunc]) -- sort *IN PLACE*; if given, cmpfunc(x, y) -> -1, 0, 1" },
	{ NO_DICT, "clear", Obj_DictClear, NULL },
	{ NO_DICT, "copy", Obj_DictCopy, NULL },
	{ NO_DICT, "get", Obj_DictGetM, NULL },
	{ NO_DICT, "has_key", Obj_DictHasKey, NULL },
	{ NO_DICT, "items", Obj_DictItems, NULL },
	{ NO_DICT, "keys", Obj_DictKeys, NULL },
	{ NO_DICT, "update", Obj_DictUpdate, NULL },
	{ NO_DICT, "values", Obj_DictValues, NULL },
};
#define OBJ_METHODS ((int)(sizeof(obj_methods) / sizeof(obj_methods[0])))

/* Py_FindMethod of a native list or dict: the method bound to it, the list of the names for __methods__ */
static byte Obj_FindMethod(nativeObj_t *o, const char *name, nativeVal_t *out) {
	if (!strcmp(name, "__methods__")) {
		Native_SetEmpty(out, NV_LIST);
		for (int i = 0; i < OBJ_METHODS; i++) {
			if (obj_methods[i].kind == o->kind) {
				Native_SetStr(Native_Append(out), obj_methods[i].name);
			}
		}
		return 1;
	}
	for (int i = 0; i < OBJ_METHODS; i++) {
		if (obj_methods[i].kind == o->kind && !strcmp(obj_methods[i].name, name)) {
			nativeObj_t *m = Obj_New(NO_FUNCTION);
			m->func = NF_BUILTIN;
			m->method = i;
			m->self = NObj_Keep(o);
			m->name = Obj_Text(name);
			Obj_SetRef(out, m);
			return 1;
		}
	}
	return Native_Raise(NE_ATTRIBUTE, "%s", name);
}

/* ---- attributes ---- */

/* getattr of a Python object (a pack module through the registry of the module variables, as the channel reads it) */
static byte Obj_PyGetAttr(PyObject *py, const char *name, nativeVal_t *out) {
	PyObject *r = ModVar_ObjAttr(py, name);
	Py_DECREF(py);
	return Obj_Result(r, out);
}

byte NObj_GetAttr(const nativeVal_t *v, const char *name, nativeVal_t *out) {
	modVarRef_t mv;
	if (ModVar_OfVal(v, &mv)) {
		/* a pack module: its variable in the registry */
		return ModVar_AttrVal(&mv, name, out);
	}
	PyObject *py = Obj_Forward(v);
	Native_SetNone(out);
	if (py) {
		return Obj_PyGetAttr(py, name, out);
	}
	if (v->type == NV_LIST || v->type == NV_DICT) {
		/* a temporary: its method is bound to an object of its own */
		nativeVal_t tmp;
		Obj_Store(&tmp, v);
		byte ok = Obj_FindMethod(tmp.ref, name, out);
		Native_Free(&tmp);
		return ok;
	}
	if (v->type != NV_REF) {
		return Native_Raise(NE_ATTRIBUTE, "'%.50s' object has no attribute '%.400s'", Native_TypeName(v), name);
	}
	nativeObj_t *o = v->ref, *found;
	switch (o->kind) {
	case NO_LIST:
	case NO_DICT:
		return Obj_FindMethod(o, name, out);
	case NO_INSTANCE:
		if (Obj_InstLook(o, name, out)) {
			return 1;
		}
		break;
	case NO_CLASS:
		/* class_getattr */
		if (!strcmp(name, "__dict__")) {
			Obj_SetRef(out, NObj_Keep(o->dict));
			return 1;
		}
		if (!strcmp(name, "__bases__")) {
			Native_Copy(out, &o->data);
			return 1;
		}
		if (!strcmp(name, "__name__")) {
			if (o->name) {
				Native_SetText(out, o->name, (int)strlen(o->name));
			}
			return 1;
		}
		if (Obj_ClassLook(o, name, out, &found)) {
			if (Obj_IsKind(out, NO_FUNCTION) && out->ref->binds) {
				Obj_Bind(out, NULL, found);
			}
			NObj_Drop(found);
			return 1;
		}
		break;
	case NO_FUNCTION:
		/* meth_getattr; of a function of Python code its name and doc */
		if (!strcmp(name, "__name__") || (o->binds && !strcmp(name, "func_name"))) {
			Native_SetText(out, o->name, (int)strlen(o->name));
			return 1;
		}
		if (!strcmp(name, "__doc__") || (o->binds && !strcmp(name, "func_doc"))) {
			if (o->func == NF_BUILTIN && !o->binds && obj_methods[o->method].doc) {
				Native_SetText(out, obj_methods[o->method].doc, (int)strlen(obj_methods[o->method].doc));
			}
			return 1;
		}
		if (!o->binds && !strcmp(name, "__self__")) {
			if (o->func == NF_BUILTIN) {
				Obj_SetRef(out, NObj_Keep(o->self));
			} else if (o->func == NF_CALLBACK) {
				Native_Copy(out, &o->cb->self);
			}
			return 1;
		}
		if (!o->binds && !strcmp(name, "__members__")) {
			Native_SetEmpty(out, NV_LIST);
			Native_SetStr(Native_Append(out), "__doc__");
			Native_SetStr(Native_Append(out), "__name__");
			Native_SetStr(Native_Append(out), "__self__");
			return 1;
		}
		break;
	case NO_METHOD: {
		/* instancemethod_getattr */
		nativeVal_t f;
		if (!strcmp(name, "__name__") || !strcmp(name, "__doc__")) {
			Obj_SetRef(&f, o->fn);
			return NObj_GetAttr(&f, name, out);
		}
		if (!strcmp(name, "im_func")) {
			Obj_SetRef(out, NObj_Keep(o->fn));
			return 1;
		}
		if (!strcmp(name, "im_self")) {
			if (o->self) {
				Obj_SetRef(out, NObj_Keep(o->self));
			}
			return 1;
		}
		if (!strcmp(name, "im_class")) {
			if (o->cls) {
				Obj_SetRef(out, NObj_Keep(o->cls));
			}
			return 1;
		}
		break;
	}
	}
	return Native_Raise(NE_ATTRIBUTE, "%s", name);
}

/* a text is a name of the form __x__ */
static byte Obj_Dunder(const char *name) {
	size_t n = strlen(name);
	return n >= 4 && name[0] == '_' && name[1] == '_' && name[n - 1] == '_' && name[n - 2] == '_';
}

/* setattr (v given) and delattr (v NULL) */
static byte Obj_SetAttr(const nativeVal_t *t, const char *name, const nativeVal_t *v) {
	modVarRef_t mv;
	if (ModVar_OfVal(t, &mv)) {
		return ModVar_SetAttrVal(&mv, name, v);
	}
	PyObject *py = Obj_Forward(t);
	if (py) {
		PyObject *x = v ? Native_ToPy(v) : NULL;
		int r = !v || x ? ModVar_SetObjAttr(py, name, x) : -1;
		Py_XDECREF(x);
		Py_DECREF(py);
		return r == 0;
	}
	if (t->type == NV_LIST || t->type == NV_DICT || Obj_IsKind(t, NO_LIST) || Obj_IsKind(t, NO_DICT) ||
		Obj_IsKind(t, NO_FUNCTION)) {
		/* a type with a getattr and no setattr; a function of Python code has none of these names */
		if (Obj_IsKind(t, NO_FUNCTION) && t->ref->binds) {
			return Native_Raise(NE_ATTRIBUTE, "%s", name);
		}
		return Native_Raise(NE_TYPE, "object has read-only attributes");
	}
	if (!Obj_IsKind(t, NO_INSTANCE) && !Obj_IsKind(t, NO_CLASS)) {
		return Native_Raise(NE_TYPE, "attribute-less object (assign or del)");
	}
	nativeObj_t *o = t->ref;
	byte special = Obj_Dunder(name);
	if (o->kind == NO_INSTANCE) {
		/* instance_setattr */
		if (special && !strcmp(name, "__dict__")) {
			if (!v || (v->type != NV_DICT && !Obj_IsKind(v, NO_DICT))) {
				return Native_Raise(NE_TYPE, "__dict__ must be set to a dictionary");
			}
			nativeVal_t d;
			Obj_Store(&d, v);
			NObj_Drop(o->dict);
			o->dict = d.ref;
			return 1;
		}
		if (special && !strcmp(name, "__class__")) {
			if (!v || !Obj_IsKind(v, NO_CLASS)) {
				return Native_Raise(NE_TYPE, "__class__ must be set to a class");
			}
			nativeObj_t *old = o->cls;
			o->cls = NObj_Keep(v->ref);
			NObj_Drop(old);
			return 1;
		}
		if (!v) {
			return Obj_DictRemove(o->dict, name) || Native_Raise(NE_ATTRIBUTE, "delete non-existing instance attribute");
		}
		return Obj_DictPut(o->dict, name, v);
	}
	/* class_setattr */
	if (special && !strcmp(name, "__dict__")) {
		if (!v || (v->type != NV_DICT && !Obj_IsKind(v, NO_DICT))) {
			return Native_Raise(NE_TYPE, "__dict__ must be a dictionary object");
		}
		nativeVal_t d;
		Obj_Store(&d, v);
		NObj_Drop(o->dict);
		o->dict = d.ref;
		return 1;
	}
	if (special && !strcmp(name, "__bases__")) {
		nativeVal_t b;
		if (!v || v->type != NV_TUPLE) {
			return Native_Raise(NE_TYPE, "__bases__ must be a tuple object");
		}
		if (!Obj_Bases(v, &b)) {
			return Native_Raise(NE_TYPE, "__bases__ items must be classes");
		}
		for (int i = 0; i < b.n; i++) {
			if (Obj_IsSubclass(b.items[i].ref, o)) {
				Native_Free(&b);
				return Native_Raise(NE_TYPE, "a __bases__ item causes an inheritance cycle");
			}
		}
		nativeVal_t old = o->data;
		o->data = b;
		Native_Free(&old);
		return 1;
	}
	if (special && !strcmp(name, "__name__")) {
		if (!v || v->type != NV_STR) {
			return Native_Raise(NE_TYPE, "__name__ must be a string object");
		}
		if (strlen(Native_Text(v)) != (size_t)v->n) {
			return Native_Raise(NE_TYPE, "__name__ must not contain null bytes");
		}
		Mem_Free(o->name);
		o->name = Obj_Text(Native_Text(v));
		return 1;
	}
	if (!v) {
		return Obj_DictRemove(o->dict, name) || Native_Raise(NE_ATTRIBUTE, "delete non-existing class attribute");
	}
	return Obj_DictPut(o->dict, name, v);
}

byte NObj_SetAttr(const nativeVal_t *o, const char *name, const nativeVal_t *v) {
	return Obj_SetAttr(o, name, v);
}

byte NObj_DelAttr(const nativeVal_t *o, const char *name) {
	return Obj_SetAttr(o, name, NULL);
}

/* ---- items, slices, len, in ---- */

/* the AttributeError of the special method a native instance does not have: 0 */
static byte Obj_NoSpecial(const char *name) {
	return Native_Raise(NE_ATTRIBUTE, "%s", name);
}

int NObj_Len(const nativeVal_t *v) {
	PyObject *py = Obj_Forward(v);
	if (py) {
		int n = PyObject_Length(py);
		Py_DECREF(py);
		return n;
	}
	if (v->type != NV_REF) {
		return Native_Size(v);
	}
	if (v->ref->kind == NO_LIST || v->ref->kind == NO_DICT) {
		return v->ref->data.n;
	}
	if (v->ref->kind == NO_INSTANCE) {
		Obj_NoSpecial("__len__");
	} else {
		Native_Raise(NE_TYPE, "len() of unsized object");
	}
	return -1;
}

byte NObj_GetItem(const nativeVal_t *v, const nativeVal_t *key, nativeVal_t *out) {
	PyObject *py = Obj_Forward(v);
	Native_SetNone(out);
	if (py) {
		PyObject *k = Native_ToPy(key);
		PyObject *r = k ? PyObject_GetItem(py, k) : NULL;
		Py_XDECREF(k);
		Py_DECREF(py);
		return Obj_Result(r, out);
	}
	if (v->type != NV_REF) {
		return Native_Subscript(v, key, out);
	}
	if (v->ref->kind == NO_LIST || v->ref->kind == NO_DICT) {
		return Native_Subscript(&v->ref->data, key, out);
	}
	return v->ref->kind == NO_INSTANCE ? Obj_NoSpecial("__getitem__") : Native_Raise(NE_TYPE, "unsubscriptable object");
}

/* o[key] = x (x given) and del o[key] (x NULL) */
static byte Obj_SetItem(const nativeVal_t *v, const nativeVal_t *key, const nativeVal_t *x) {
	PyObject *py = Obj_Forward(v);
	if (py) {
		PyObject *k = Native_ToPy(key), *px = k && x ? Native_ToPy(x) : NULL;
		int r = !k || (x && !px) ? -1 : x ? PyObject_SetItem(py, k, px) : PyObject_DelItem(py, k);
		Py_XDECREF(k);
		Py_XDECREF(px);
		Py_DECREF(py);
		return r == 0;
	}
	nativeObj_t *o = v->type == NV_REF ? v->ref : NULL;
	if (o && o->kind == NO_LIST) {
		if (key->type != NV_INT) {
			return Native_Raise(NE_TYPE, "sequence index must be integer");
		}
		long i = key->i < 0 ? key->i + o->data.n : key->i;
		if (i < 0 || i >= o->data.n) {
			return Native_Raise(NE_INDEX, "list assignment index out of range");
		}
		if (!Obj_ListFree(o)) {
			return 0;
		}
		if (x) {
			nativeVal_t old = o->data.items[i];
			Obj_Store(&o->data.items[i], x);
			Native_Free(&old);
		} else {
			Obj_ListSplice(o, i, i + 1, NULL, 0);
		}
		return 1;
	}
	if (o && o->kind == NO_DICT) {
		long h;
		if (!NObj_Hash(key, &h)) {
			return 0;
		}
		if (!x) {
			return Native_DictDel(&o->data, key) || Native_RaiseKey(key);
		}
		nativeVal_t nk, nx;
		Native_Copy(&nk, key);
		Obj_Store(&nx, x);
		return Native_DictSet(&o->data, &nk, &nx);
	}
	if (o && o->kind == NO_INSTANCE) {
		return Obj_NoSpecial(x ? "__setitem__" : "__delitem__");
	}
	if (v->type == NV_LIST || v->type == NV_DICT) {
		/* a temporary: the store goes to an object of its own */
		nativeVal_t tmp;
		Obj_Store(&tmp, v);
		byte ok = Obj_SetItem(&tmp, key, x);
		Native_Free(&tmp);
		return ok;
	}
	if (v->type == NV_STR || v->type == NV_TUPLE || v->type == NV_VEC) {
		/* a sequence without item assignment (PySequence_SetItem, PySequence_DelItem) */
		if (key->type != NV_INT) {
			return Native_Raise(NE_TYPE, "sequence index must be integer");
		}
		return Native_Raise(NE_TYPE, x ? "object doesn't support item assignment" : "object doesn't support item deletion");
	}
	return Native_Raise(NE_TYPE, x ? "object does not support item assignment" : "object does not support item deletion");
}

byte NObj_SetItem(const nativeVal_t *o, const nativeVal_t *key, const nativeVal_t *v) {
	return Obj_SetItem(o, key, v);
}

byte NObj_DelItem(const nativeVal_t *o, const nativeVal_t *key) {
	return Obj_SetItem(o, key, NULL);
}

static int Obj_Bound(long i) {
	return i > INT_MAX ? INT_MAX : i < INT_MIN ? INT_MIN : (int)i;
}

byte NObj_GetSlice(const nativeVal_t *v, long lo, long hi, nativeVal_t *out) {
	PyObject *py = Obj_Forward(v);
	Native_SetNone(out);
	if (py) {
		PyObject *r = PySequence_GetSlice(py, Obj_Bound(lo), Obj_Bound(hi));
		Py_DECREF(py);
		return Obj_Result(r, out);
	}
	if (v->type != NV_REF) {
		return Native_Slice(v, lo, hi, out);
	}
	if (v->ref->kind == NO_LIST) {
		return Native_Slice(&v->ref->data, lo, hi, out);
	}
	return v->ref->kind == NO_INSTANCE ? Obj_NoSpecial("__getslice__") : Native_Raise(NE_TYPE, "unsliceable object");
}

byte NObj_SetSlice(const nativeVal_t *v, long lo, long hi, const nativeVal_t *seq) {
	PyObject *py = Obj_Forward(v);
	if (py) {
		PyObject *s = seq ? Native_ToPy(seq) : NULL;
		int r = seq && !s ? -1 : seq ? PySequence_SetSlice(py, Obj_Bound(lo), Obj_Bound(hi), s) :
			PySequence_DelSlice(py, Obj_Bound(lo), Obj_Bound(hi));
		Py_XDECREF(s);
		Py_DECREF(py);
		return r == 0;
	}
	nativeObj_t *o = v->type == NV_REF ? v->ref : NULL;
	if (o && o->kind == NO_INSTANCE) {
		return Obj_NoSpecial(seq ? "__setslice__" : "__delslice__");
	}
	if (!o || o->kind != NO_LIST) {
		return Native_Raise(NE_TYPE, seq ? "object doesn't support slice assignment" :
			"object doesn't support slice deletion");
	}
	/* PySequence_SetSlice adds the length to a negative bound once, list_ass_slice clamps */
	long n = o->data.n;
	lo = lo < 0 ? lo + n : lo;
	hi = hi < 0 ? hi + n : hi;
	lo = lo < 0 ? 0 : lo > n ? n : lo;
	hi = hi < lo ? lo : hi > n ? n : hi;
	nativeVal_t tmp, copy;
	const nativeVal_t *src = seq ? Obj_Items(seq, NV_LIST, &tmp) : NULL;
	if (seq && !src) {
		return Obj_BadArg();
	}
	if (!Obj_ListFree(o)) {
		if (seq) {
			Native_Free(&tmp);
		}
		return 0;
	}
	if (!seq) {
		Obj_ListSplice(o, lo, hi, NULL, 0);
		return 1;
	}
	/* a[i:j] = a: from a copy */
	Native_Copy(&copy, src);
	Obj_ListSplice(o, lo, hi, copy.items, copy.n);
	Native_Free(&copy);
	Native_Free(&tmp);
	return 1;
}

int NObj_Contains(const nativeVal_t *seq, const nativeVal_t *x) {
	PyObject *py = Obj_Forward(seq);
	if (py) {
		/* cmp_member of ceval.c */
		PySequenceMethods *sq = py->ob_type->tp_as_sequence;
		PyObject *px = sq && sq->sq_item ? Native_ToPy(x) : NULL;
		int r = -1;
		if (!sq || !sq->sq_item) {
			Native_Raise(NE_TYPE, "'in' or 'not in' needs sequence right argument");
		}
		for (int i = 0; px && r < 0; i++) {
			PyObject *item = sq->sq_item(py, i);
			if (!item) {
				if (Native_ErrIs(NE_INDEX)) {
					Native_ErrClear();
					r = 0;
				}
				break;
			}
			int c = PyObject_Compare(px, item);
			Py_DECREF(item);
			if (c == 0) {
				r = 1;
			} else if (Native_Failed()) {
				break;
			}
		}
		Py_XDECREF(px);
		Py_DECREF(py);
		return r;
	}
	if (seq->type != NV_REF) {
		return Native_Contains(seq, x);
	}
	if (seq->ref->kind == NO_LIST) {
		return Native_Contains(&seq->ref->data, x);
	}
	if (seq->ref->kind == NO_INSTANCE) {
		Obj_NoSpecial("__getitem__");
	} else {
		Native_Raise(NE_TYPE, "'in' or 'not in' needs sequence right argument");
	}
	return -1;
}

/* ---- calls ---- */

/* the call of a Python object on native arguments, as the value calls of the ports make it (the entry of a port's own
 * function, else through Python), the result by NObj_FromPy */
static byte Obj_CallPy(PyObject *f, const nativeVal_t *a, int n, nativeVal_t *out) {
	PyObject *r = NULL;
	Native_SetNone(out);
	Port_Values(PORT_VSITE(), f, a, n, NULL, &r);
	return Obj_Result(r, out);
}

/* a native function on its arguments */
static byte Obj_CallFunc(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	byte ok;
	switch (o->func) {
	case NF_PORT: {
		static portSite_t site = { .ref = &port_value, .global = 2, .file = __FILE__, .line = __LINE__ };
		if (n > PORT_ARGS) {
			/* more than an entry takes: the adapter through Python */
			PyObject *f = Obj_ToPy(o);
			ok = f && Obj_CallPy(f, a, n, out);
			Py_XDECREF(f);
			return ok;
		}
		site.ref = o->ref;
		return Port_BoundCall(&site, NULL, a, n, out, NULL) && Obj_Norm(out);
	}
	case NF_CALLBACK:
		/* the depth check of a call, as Port_Entry has it */
		if (!Script_Enter((uintptr_t)__builtin_frame_address(0))) {
			return 0;
		}
		ok = o->cb->call(o->cb->ctx, a, n, out);
		Script_Leave();
		if (!ok) {
			Native_Free(out);
			if (!Native_Failed()) {
				Native_Raise(NE_SYSTEM, "NULL result without error in call_object");
			}
		}
		return ok;
	}
	ok = obj_methods[o->method].fn(o->self, a, n, out);
	if (!ok) {
		Native_Free(out);
	}
	return ok;
}

/* a new instance of the native class c: PyInstance_New */
static byte Obj_NewInstance(nativeObj_t *c, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t inst, init, r;
	nativeObj_t *o = Obj_New(NO_INSTANCE);
	o->cls = NObj_Keep(c);
	o->dict = Obj_NewDictObj();
	Obj_SetRef(&inst, o);
	byte ok = 1;
	if (!Obj_InstLook(o, "__init__", &init)) {
		if (n) {
			ok = Native_Raise(NE_TYPE, "this constructor takes no arguments");
		}
	} else {
		ok = NObj_Call(&init, a, n, &r);
		if (ok && r.type != NV_NONE) {
			ok = Native_Raise(NE_TYPE, "__init__() should return None");
		}
		Native_Free(&init);
		Native_Free(&r);
	}
	if (!ok) {
		Native_Free(&inst);
		return 0;
	}
	*out = inst;
	return 1;
}

byte NObj_Call(const nativeVal_t *f, const nativeVal_t *a, int n, nativeVal_t *out) {
	PyObject *py = Obj_Forward(f);
	nativeVal_t call;
	Native_SetNone(out);
	if (py) {
		byte ok = Obj_CallPy(py, a, n, out);
		Py_DECREF(py);
		return ok;
	}
	if (f->type != NV_REF || f->ref->kind == NO_LIST || f->ref->kind == NO_DICT) {
		return Native_Raise(NE_TYPE, "call of non-function (type %s)", Native_TypeName(f));
	}
	nativeObj_t *o = f->ref;
	switch (o->kind) {
	case NO_FUNCTION:
		return Obj_CallFunc(o, a, n, out);
	case NO_CLASS:
		return Obj_NewInstance(o, a, n, out);
	case NO_INSTANCE: {
		if (!Obj_InstLook(o, "__call__", &call)) {
			return Native_Raise(NE_ATTRIBUTE, "no __call__ method defined");
		}
		byte ok = NObj_Call(&call, a, n, out);
		Native_Free(&call);
		return ok;
	}
	}
	/* a method: the instance first; unbound, the first argument is an instance of its class */
	nativeVal_t fn;
	Obj_SetRef(&fn, o->fn);
	if (!o->self) {
		if (n < 1 || !o->cls || !Obj_IsKind(&a[0], NO_INSTANCE) || !Obj_InstanceOf(a[0].ref, o->cls)) {
			return Native_Raise(NE_TYPE, "unbound method must be called with class instance 1st argument");
		}
		return NObj_Call(&fn, a, n, out);
	}
	nativeVal_t *args = Mem_Alloc((uint32_t)((size_t)(n + 1) * sizeof(*args)));
	Obj_SetRef(&args[0], o->self);
	for (int i = 0; i < n; i++) {
		args[i + 1] = a[i];
	}
	byte ok = NObj_Call(&fn, args, n + 1, out);
	Mem_Free(args);
	return ok;
}

/* the keyword pairs as the dict a call through Python takes (NULL with the error) */
static PyObject *Obj_KwToPy(const nativeVal_t *kw) {
	PyObject *d = PyDict_New();
	for (int i = 0; d && i < kw->n; i++) {
		const nativeVal_t *pair = &kw->items[i];
		PyObject *k = Native_ToPy(&pair->items[0]), *x = k ? Native_ToPy(&pair->items[1]) : NULL;
		int r = x ? PyDict_SetItem(d, k, x) : -1;
		Py_XDECREF(k);
		Py_XDECREF(x);
		if (r != 0) {
			Py_DECREF(d);
			d = NULL;
		}
	}
	return d;
}

byte NObj_CallKw(const nativeVal_t *f, const nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out) {
	nativeVal_t call;
	Native_SetNone(out);
	if (!kw || !kw->n) {
		return NObj_Call(f, a, n, out);
	}
	nativeObj_t *o = f->type == NV_REF && !Obj_View(f->ref) ? f->ref : NULL;
	if (o && o->kind == NO_FUNCTION && o->func != NF_PORT) {
		if (o->func != NF_CALLBACK || !o->cb->callkw) {
			return Native_Raise(NE_TYPE, "this function takes no keyword arguments");
		}
		if (!Script_Enter((uintptr_t)__builtin_frame_address(0))) {
			return 0;
		}
		byte ok = o->cb->callkw(o->cb->ctx, a, n, kw, out);
		Script_Leave();
		if (!ok) {
			Native_Free(out);
			if (!Native_Failed()) {
				Native_Raise(NE_SYSTEM, "NULL result without error in call_object");
			}
		}
		return ok;
	}
	if (o && o->kind == NO_INSTANCE) {
		if (!Obj_InstLook(o, "__call__", &call)) {
			return Native_Raise(NE_ATTRIBUTE, "no __call__ method defined");
		}
		byte ok = NObj_CallKw(&call, a, n, kw, out);
		Native_Free(&call);
		return ok;
	}
	if (o && o->kind == NO_CLASS) {
		/* PyInstance_New: the keywords go to __init__ */
		nativeVal_t inst, init, r;
		nativeObj_t *i = Obj_New(NO_INSTANCE);
		i->cls = NObj_Keep(o);
		i->dict = Obj_NewDictObj();
		Obj_SetRef(&inst, i);
		byte ok = Obj_InstLook(i, "__init__", &init) ? NObj_CallKw(&init, a, n, kw, &r) :
			Native_Raise(NE_TYPE, "this constructor takes no arguments");
		if (ok && r.type != NV_NONE) {
			ok = Native_Raise(NE_TYPE, "__init__() should return None");
		}
		Native_Free(&init);
		Native_Free(&r);
		if (!ok) {
			Native_Free(&inst);
			return 0;
		}
		*out = inst;
		return 1;
	}
	if (o && o->kind == NO_METHOD) {
		nativeVal_t fn;
		Obj_SetRef(&fn, o->fn);
		if (!o->self) {
			if (n < 1 || !o->cls || !Obj_IsKind(&a[0], NO_INSTANCE) || !Obj_InstanceOf(a[0].ref, o->cls)) {
				return Native_Raise(NE_TYPE, "unbound method must be called with class instance 1st argument");
			}
			return NObj_CallKw(&fn, a, n, kw, out);
		}
		nativeVal_t *args = Mem_Alloc((uint32_t)((size_t)(n + 1) * sizeof(*args)));
		Obj_SetRef(&args[0], o->self);
		for (int k = 0; k < n; k++) {
			args[k + 1] = a[k];
		}
		byte ok = NObj_CallKw(&fn, args, n + 1, kw, out);
		Mem_Free(args);
		return ok;
	}
	/* a port function, a Python object: through Python (the port's adapter takes the keywords) */
	PyObject *py = Native_ToPy(f), *t = py ? PyTuple_New(n) : NULL, *d = t ? Obj_KwToPy(kw) : NULL;
	for (int k = 0; d && k < n; k++) {
		PyObject *x = Native_ToPy(&a[k]);
		if (!x) {
			Py_DECREF(d);
			d = NULL;
			break;
		}
		PyTuple_SET_ITEM(t, k, x);
	}
	PyObject *r = d ? Script_CallObject(py, t, d) : NULL;
	Py_XDECREF(py);
	Py_XDECREF(t);
	Py_XDECREF(d);
	return Obj_Result(r, out);
}

byte NObj_CallMethod(const nativeVal_t *o, const char *name, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t f;
	Native_SetNone(out);
	if (!NObj_GetAttr(o, name, &f)) {
		return 0;
	}
	byte ok = NObj_Call(&f, a, n, out);
	Native_Free(&f);
	return ok;
}
