/*
 * Native objects with identity (include/scrap/nativeobj.h, stage 9 of docs/objlib-plan.md): the reference-counted
 * lists, dicts, instances, classes, functions, methods and modules that native values hold as NV_REF, their
 * operations as Python 1.5.2 has them.
 */
#include <errno.h>
#include <limits.h>
#include <stddef.h>
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
	NF_BOUND,			/* a C entry bound to a value (a method of an entity handle): data the value, entry */
};

/* a C callback and its context */
typedef struct nativeFn_s {
	int refs;
	nativeCall_t call;
	nativeCallKw_t callkw;		/* the call with keywords, NULL when it takes none */
	void *ctx;
	void (*drop)(void *ctx);
	nativeVal_t self;		/* its __self__ (None: a function of the C table) */
	const char *doc;		/* its __doc__ (NULL: None) */
	char name[];
} nativeFn_t;

struct nativeObj_s {
	byte kind;			/* NO_ */
	byte func;			/* NO_FUNCTION: NF_ */
	byte binds;			/* NO_FUNCTION: a class binds it to its instances (a function of Python code) */
	byte busy;			/* NO_LIST: being sorted */
	int refs;
	nativeObj_t *next;		/* NO_TYPE: the next type object made */
	nativeVal_t data;		/* NO_LIST: the NV_LIST; NO_DICT: the NV_DICT; NO_CLASS: the NV_TUPLE of its bases */
	nativeObj_t *dict;		/* NO_INSTANCE, NO_CLASS: the attributes (a NO_DICT) */
	nativeObj_t *cls;		/* NO_INSTANCE: its class; NO_METHOD: the class of the method */
	nativeObj_t *self;		/* NO_METHOD: the instance (NULL: unbound); NF_BUILTIN: the list or dict */
	nativeObj_t *fn;		/* NO_METHOD: the function */
	char *name;			/* NO_CLASS, NO_FUNCTION: owned */
	const portRef_t *ref;		/* NF_PORT */
	nativeFn_t *cb;			/* NF_CALLBACK */
	int method;			/* NF_BUILTIN */
	portEntry_t entry;		/* NF_BOUND: called with data first */
	const nativeCType_t *ctype;	/* NO_COBJ: its type, and its object */
	void *ctx;
};

static unsigned long obj_made, obj_freed;

/* ---- holders ---- */

static nativeObj_t *Obj_New(byte kind) {
	nativeObj_t *o = Mem_Alloc(sizeof(*o));
	memset(o, 0, sizeof(*o));
	o->kind = kind;
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

/* what the native data of o holds is released */
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
	Obj_FnDrop(o->cb);
	if (o->kind == NO_COBJ && o->ctype->drop) {
		o->ctype->drop(o->ctx);
	}
	Mem_Free(o->name);
	Mem_Free(o);
	obj_freed++;
}

int NObj_Kind(const nativeVal_t *v) {
	return v->type == NV_REF ? v->ref->kind : -1;
}

byte NObj_Is(const nativeVal_t *a, const nativeVal_t *b) {
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
	}
	return 0;
}

void NObj_Counts(unsigned long *made, unsigned long *freed) {
	*made = obj_made;
	*freed = obj_freed;
}

/* under SCRAP_NATIVE_COUNT=1 the program prints at its end the objects made and freed (what is left then is held by
 * the C state of the ports, which is never freed, or was lost) */
__attribute__((destructor)) static void Obj_AtExit(void) {
	const char *e = getenv("SCRAP_NATIVE_COUNT");
	if (e && !strcmp(e, "1")) {
		printf("native objects at exit: made %lu freed %lu left %lu\n", obj_made, obj_freed, obj_made - obj_freed);
		fflush(stdout);
	}
}

/* v as a list (type NV_LIST) or dict (NV_DICT) of items to read: the value itself or the data of its object (*tmp
 * None, for the callers that free it after); NULL when v is none of them */
static const nativeVal_t *Obj_Items(const nativeVal_t *v, byte type, nativeVal_t *tmp) {
	Native_SetNone(tmp);
	if (v->type == type) {
		return v;
	}
	if (v->type != NV_REF || v->ref->kind != (type == NV_LIST ? NO_LIST : NO_DICT)) {
		return NULL;
	}
	return &v->ref->data;
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

/* a copy of v to keep in an object: shared (NObj_Share) */
static void Obj_Store(nativeVal_t *slot, const nativeVal_t *v) {
	Native_Copy(slot, v);
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

/* d[name] of a dict object: 1 with *out a copy, 0 when it has none */
static byte Obj_DictLook(const nativeObj_t *d, const char *name, nativeVal_t *out) {
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

/* d[name] = v: 1, 0 with the error */
static byte Obj_DictPut(nativeObj_t *d, const char *name, const nativeVal_t *v) {
	nativeVal_t key, x;
	Native_SetText(&key, name, (int)strlen(name));
	Obj_Store(&x, v);
	return Native_DictSet(&d->data, &key, &x);
}

/* del d[name]: 1, 0 without an error when it has none */
static byte Obj_DictRemove(nativeObj_t *d, const char *name) {
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
	/* classobject.c Class_New: __doc__ and __module__ when the body gave none */
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

void NObj_SetCallbackDoc(const nativeVal_t *f, const char *doc) {
	f->ref->cb->doc = doc;
}

int NObj_Refs(const nativeVal_t *v) {
	return v->type == NV_REF ? v->ref->refs : 0;
}

uintptr_t NObj_Addr(const nativeVal_t *v) {
	return v->type == NV_REF ? (uintptr_t)v->ref : 0;
}

const nativeCType_t *NObj_CType(const nativeVal_t *v) {
	return v->type == NV_REF && v->ref->kind == NO_COBJ ? v->ref->ctype : NULL;
}

void *NObj_CObjOf(const nativeVal_t *v, const nativeCType_t *type) {
	return v->type == NV_REF && v->ref->kind == NO_COBJ && v->ref->ctype == type ? v->ref->ctx : NULL;
}

/* the type objects made, one per name, kept for good (chained by next) */
static nativeObj_t *obj_types;

void NObj_TypeNamed(nativeVal_t *out, const char *name) {
	nativeObj_t *o = obj_types;
	while (o && strcmp(o->name, name)) {
		o = o->next;
	}
	if (!o) {
		o = Obj_New(NO_TYPE);
		o->name = Obj_Text(name);
		o->next = obj_types;
		obj_types = o;
	}
	Obj_SetRef(out, NObj_Keep(o));
}

void NObj_TypeOf(const nativeVal_t *v, nativeVal_t *out) {
	NObj_TypeNamed(out, Native_TypeName(v));
}

void NObj_NewCObj(nativeVal_t *out, const nativeCType_t *type, void *ctx) {
	nativeObj_t *o = Obj_New(NO_COBJ);
	o->ctype = type;
	o->ctx = ctx;
	Obj_SetRef(out, o);
}

byte NObj_PrintText(const nativeVal_t *v, char *buf, int size) {
	if (v->type != NV_REF || v->ref->kind != NO_COBJ || !v->ref->ctype->print) {
		return 0;
	}
	v->ref->ctype->print(v->ref->ctx, buf, size);
	return 1;
}

void NObj_NewBound(nativeVal_t *out, const char *name, portEntry_t entry, const nativeVal_t *self) {
	nativeObj_t *o = Obj_New(NO_FUNCTION);
	o->func = NF_BOUND;
	o->entry = entry;
	o->name = Obj_Text(name);
	Native_Copy(&o->data, self);
	Obj_SetRef(out, o);
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
	return v->type == NV_REF;
}

const portRef_t *NObj_PortRef(const nativeVal_t *f) {
	return f->type == NV_REF && f->ref->kind == NO_FUNCTION && f->ref->func == NF_PORT ? f->ref->ref : NULL;
}

/* a data function: ctx the module name, then the function name; the global looked up and called as a value (the
 * arguments and the result as a call through Python hands them over) */
static byte Obj_DataCall(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *module = ctx, *name = module + strlen(module) + 1;
	nativeVal_t f;
	byte ok = ModVar_GlobalVal(module, name, &f) && Port_ValuesOf(PORT_VSITE(), &f, a, n, out, 1);
	Native_Free(&f);
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
	return Script_Module(name, out);
}

void NObj_NewModule(nativeVal_t *out, const char *name) {
	nativeObj_t *o = Obj_New(NO_MODULE);
	nativeVal_t k, v;
	o->name = Obj_Text(name);
	NObj_NewDict(&o->data);
	/* moduleobject.c Module_New: __name__, __doc__ None */
	Native_SetStr(&k, "__name__");
	Native_SetText(&v, name, (int)strlen(name));
	Native_DictSet(NObj_DictData(&o->data), &k, &v);
	Native_SetStr(&k, "__doc__");
	Native_SetNone(&v);
	Native_DictSet(NObj_DictData(&o->data), &k, &v);
	Obj_SetRef(out, o);
}

const nativeVal_t *NObj_ModuleVars(const nativeVal_t *m) {
	return Obj_IsKind(m, NO_MODULE) ? &m->ref->data : NULL;
}

const nativeVal_t *NObj_ListData(const nativeVal_t *l) {
	return l->type == NV_REF && l->ref->kind == NO_LIST ? &l->ref->data : NULL;
}

nativeVal_t *NObj_DictData(const nativeVal_t *d) {
	return d->type == NV_REF && d->ref->kind == NO_DICT ? &d->ref->data : NULL;
}

/* NObj_Plain of a value at a depth inside a container */
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
		out->id = 0;
		return 1;
	case NV_ENT:
	case NV_OBJ:
		return 0;
	case NV_REF:
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
		/* the table as it is, as the dict a call through Python handed back had its slots */
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

/* NObj_ArgOf of a tuple, list or dict by value that holds an object or a handle: what a call through Python handed
 * back: a list or dict as a new object (its plain items copied, its objects and handles themselves), a tuple as a
 * tuple of such items, a vector as the tuple of its floats */
static void Obj_ArgOfData(const nativeVal_t *v, nativeVal_t *out) {
	if (v->type == NV_TUPLE) {
		/* the same tuple: Python hands the tuple object itself back */
		Native_SetEmpty(out, NV_TUPLE);
		for (int i = 0; i < v->n; i++) {
			Obj_ArgOfData(&v->items[i], Native_Append(out));
		}
		out->id = v->id;
	} else if (v->type == NV_LIST || v->type == NV_DICT) {
		NObj_CallArg(v, out);
	} else if (v->type == NV_ENT || v->type == NV_REF || !Obj_Plain(v, out, 0)) {
		Native_Copy(out, v);
		out->id = 0;
	}
}

void NObj_ArgOf(const nativeVal_t *v, nativeVal_t *out) {
	if (v->type != NV_ENT && Obj_Plain(v, out, 0)) {
		return;
	}
	if (v->type == NV_ENT || v->type == NV_REF || v->type == NV_OBJ) {
		/* the same handle or object */
		Native_Copy(out, v);
		return;
	}
	Obj_ArgOfData(v, out);
}

/* ---- classes and instances (classobject.c) ---- */

static const char *Obj_ClassName(const nativeObj_t *c) {
	return c->name ? c->name : "?";
}

/* base is c or a base class of c at any depth (classobject.c Class_IsSubclass) */
static byte Obj_IsSubclass(const nativeObj_t *c, const nativeObj_t *base) {
	if (c == base) {
		return 1;
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
	return Obj_IsSubclass(inst->cls, cls);
}

/* class_lookup: the attribute name of the class c or of its bases, depth first: 1 with *out a copy and *found the
 * class that has it (held: NObj_Drop it), else 0 */
static byte Obj_ClassLook(nativeObj_t *c, const char *name, nativeVal_t *out, nativeObj_t **found) {
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

/* the function of a method object and its class as held native objects: 1 */
static byte Obj_MethodParts(nativeObj_t *m, nativeObj_t **fn, nativeObj_t **cls) {
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
	return Obj_IsKind(v, NO_INSTANCE) && Obj_InstLook(v->ref, name, out);
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
	if (!Obj_DictLook(c->dict, name, tmp) || tmp->type != NV_STR) {
		return NULL;
	}
	return Native_Text(tmp);
}

/* the lists and dicts whose repr runs (object.c ReprEnter's list) */
#define OBJ_REPR_MAX 256
static const nativeObj_t *obj_repring[OBJ_REPR_MAX];
static int obj_reprDepth;

byte NObj_Repr(nativeBuf_t *b, const nativeVal_t *v, nativeRepr_t mode) {
	if (v->type != NV_REF) {
		Native_Repr(b, v, mode);
		return 1;
	}
	nativeObj_t *o = v->ref;
	nativeVal_t tmp, f, r;
	const char *mod;
	switch (o->kind) {
	case NO_LIST:
	case NO_DICT:
		/* the items as the value by value prints them; one that holds itself as object.c ReprEnter cuts it */
		for (int i = 0; i < obj_reprDepth && i < OBJ_REPR_MAX; i++) {
			if (obj_repring[i] == o) {
				Native_BufAdd(b, o->kind == NO_LIST ? "[...]" : "{...}", 5);
				return 1;
			}
		}
		if (obj_reprDepth < OBJ_REPR_MAX) {
			obj_repring[obj_reprDepth] = o;
		}
		obj_reprDepth++;
		Native_Repr(b, &o->data, mode);
		obj_reprDepth--;
		return 1;
	case NO_INSTANCE: {
		/* instance_repr, and for str the __str__ of object.c Object_Str first */
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
		if (mod) {
			Obj_Addf(b, "<%.50s.%.50s instance at %lx>", mod, Obj_ClassName(o->cls), (long)(uintptr_t)o);
		} else {
			Obj_Addf(b, "<?.%.100s instance at %lx>", Obj_ClassName(o->cls), (long)(uintptr_t)o);
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
			Obj_Addf(b, "<class %.50s.%.50s at %lx>", mod, Obj_ClassName(o), (long)(uintptr_t)o);
		} else {
			Obj_Addf(b, "<class ?.%.100s at %lx>", Obj_ClassName(o), (long)(uintptr_t)o);
		}
		Native_Free(&tmp);
		return 1;
	case NO_FUNCTION:
		if (o->func == NF_BUILTIN) {
			/* meth_repr with a self */
			Obj_Addf(b, "<built-in method %.80s of %.80s object at %lx>", o->name,
				o->self->kind == NO_LIST ? "list" : "dictionary", (long)(uintptr_t)o->self);
		} else if (o->binds) {
			Obj_Addf(b, "<function %.100s at %lx>", o->name, (long)(uintptr_t)o);
		} else if (o->func == NF_BOUND) {
			Obj_Addf(b, "<built-in method %.80s of %.80s object at %lx>", o->name, Native_TypeName(&o->data),
				(long)Native_EntAddr(&o->data));
		} else if (o->func == NF_CALLBACK && o->cb->self.type != NV_NONE) {
			/* meth_repr with a self: the address of the self (a value by value has none: the callback's) */
			Obj_Addf(b, "<built-in method %.80s of %.80s object at %lx>", o->name, Native_TypeName(&o->cb->self),
				o->cb->self.type == NV_REF ? (long)(uintptr_t)o->cb->self.ref : (long)(uintptr_t)o->cb);
		} else {
			Obj_Addf(b, "<built-in function %.80s>", o->name);
		}
		return 1;
	case NO_TYPE:
		Obj_Addf(b, "<type '%.80s'>", o->name);
		return 1;
	case NO_COBJ:
		if (o->ctype->repr) {
			return o->ctype->repr(o->ctx, (uintptr_t)o, b, mode);
		}
		/* the repr of a type without repr and str */
		Obj_Addf(b, "<%.80s object at %lx>", o->ctype->name, (long)(uintptr_t)o);
		return 1;
	case NO_MODULE: {
		/* module_repr, the name and file of the module dict (its variables) */
		modVarRef_t mv = { .file = -1, .vars = &o->data };
		nativeVal_t name, file;
		ModVar_GetVal(&mv, "__name__", &name);
		ModVar_GetVal(&mv, "__file__", &file);
		const char *n = name.type == NV_STR ? Native_Text(&name) : "?";
		if (file.type == NV_STR) {
			Obj_Addf(b, "<module '%.50s' from '%.255s'>", n, Native_Text(&file));
		} else {
			Obj_Addf(b, "<module '%.80s' (built-in)>", n);
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
		}
		if (!o->self) {
			Obj_Addf(b, "<unbound method %.100s.%.100s>", o->cls ? Obj_ClassName(o->cls) : "?", fname);
		} else {
			Obj_Addf(b, "<method %.60s.%.60s of %.60s instance at %lx>", Obj_ClassName(o->cls), fname,
				Obj_ClassName(o->self->cls), (long)(uintptr_t)o->self);
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
		[NO_TYPE] = "type",
	};
	const nativeObj_t *o = v->ref;
	if (o->kind == NO_COBJ) {
		return o->ctype->name;
	}
	return o->kind == NO_FUNCTION && o->binds ? "function" : names[o->kind];
}

/* ---- truth, compare, hash ---- */

byte NObj_True(const nativeVal_t *v) {
	const nativeObj_t *o = v->ref;
	if (o->kind == NO_COBJ) {
		/* nb_nonzero, else sq_length */
		return o->ctype->truth ? o->ctype->truth(o->ctx) : o->ctype->len ? o->ctype->len(o->ctx) != 0 : 1;
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
		return Obj_AddrOrder((uintptr_t)a, (uintptr_t)b);
	}
	switch (a->func) {
	case NF_PORT:
		if (a->ref->entry == b->ref->entry) {
			return 0;
		}
		break;
	case NF_CALLBACK:
		/* one C function on one context (a function of the test channel looked up twice) */
		if (a->cb == b->cb || (a->cb->call == b->cb->call && a->cb->ctx == b->cb->ctx)) {
			return 0;
		}
		break;
	case NF_BOUND:
		if (Native_EntAddr(&a->data) != Native_EntAddr(&b->data)) {
			return Obj_AddrOrder(Native_EntAddr(&a->data), Native_EntAddr(&b->data));
		}
		if (a->entry == b->entry) {
			return 0;
		}
		break;
	default:
		if (a->self != b->self) {
			return Obj_AddrOrder((uintptr_t)a->self, (uintptr_t)b->self);
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
	/* an object of a type with its own compare (the library's: complex with the numbers, xrange, buffer) */
	for (int k = 0; k < 2; k++) {
		const nativeVal_t *x = k ? b : a;
		if (NObj_Kind(x) == NO_COBJ && x->ref->ctype->compare) {
			return Obj_Sign(x->ref->ctype->compare(a, b));
		}
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
	if (x->kind == NO_FUNCTION && y->kind == NO_FUNCTION) {
		return Obj_FuncCompare(x, y);
	}
	if (x->kind == NO_METHOD && y->kind == NO_METHOD) {
		/* instancemethod_compare */
		if (x->self != y->self) {
			return Obj_AddrOrder(x->self ? (uintptr_t)x->self : 0, y->self ? (uintptr_t)y->self : 0);
		}
		nativeVal_t fx, fy;
		Obj_SetRef(&fx, x->fn);
		Obj_SetRef(&fy, y->fn);
		return NObj_Compare(&fx, &fy);
	}
	/* objects without a compare: by their addresses */
	return Obj_AddrOrder((uintptr_t)x, (uintptr_t)y);
}

byte NObj_Equal(const nativeVal_t *a, const nativeVal_t *b) {
	return NObj_Compare(a, b) == 0;
}

byte NObj_HashQuiet(const nativeVal_t *v, long *out) {
	const nativeObj_t *o = v->ref;
	long x = 0, y;
	nativeVal_t t;
	switch (o->kind) {
	case NO_LIST:
	case NO_DICT:
		return 0;
	case NO_FUNCTION:
		/* meth_hash: the hash of self xor the address of the function */
		if (o->binds) {
			x = (long)(intptr_t)o;
		} else if (o->func == NF_PORT) {
			x = (long)(intptr_t)o->ref->entry;
		} else if (o->func == NF_CALLBACK) {
			x = (long)(intptr_t)o->cb->ctx ^ (long)(intptr_t)o->cb->call;
		} else if (o->func == NF_BOUND) {
			if (!Native_Hash(&o->data, &x)) {
				return 0;
			}
			x ^= (long)(intptr_t)o->entry;
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
	case NO_TYPE:
		/* the address of its module or type object, as Python hashes it */
		x = (long)(uintptr_t)o;
		break;
	case NO_COBJ:
		if (o->ctype->hash) {
			if (!o->ctype->hash(o->ctx, &x)) {
				Native_ErrClear();
				return 0;
			}
			break;
		}
		if (o->ctype->compare) {
			/* object.c Object_Hash of a type with a compare and no hash */
			return 0;
		}
		x = (long)(uintptr_t)o;
		break;
	default:
		x = (long)(intptr_t)o;
	}
	*out = x == -1 ? -2 : x;
	return 1;
}

byte NObj_Hash(const nativeVal_t *v, long *out) {
	if (NObj_Kind(v) == NO_COBJ && v->ref->ctype->hash) {
		/* the error of the type's hash */
		return v->ref->ctype->hash(v->ref->ctx, out);
	}
	if (Native_Hash(v, out)) {
		return 1;
	}
	return Native_Raise(NE_TYPE, "unhashable type");
}

/* ---- the methods of lists and dicts (listobject.c, dictobject.c) ---- */

/* the errors of getargs.c Arg_Parse (the old argument form: no arguments, one object, else their tuple) and of
 * errors.c Err_BadArgument */
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

/* the number of a method argument as the "i" of getargs.c Arg_Parse reads it (intobject.c Int_AsLong: a float cut): 0
 * with its error */
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

/* methodobject.c FindMethod of a native list or dict: the method bound to it, the list of the names for __methods__ */
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

/* getattr of a C object: methodobject.c FindMethod on the methods of its type (the names sorted for __methods__), then
 * the getattr of the type */
static byte Obj_CGetAttr(const nativeVal_t *v, const char *name, nativeVal_t *out) {
	const nativeObj_t *o = v->ref;
	const nativeCMethod_t *m = o->ctype->methods;
	if (m && !strcmp(name, "__methods__")) {
		nativeVal_t r;
		Native_SetEmpty(out, NV_LIST);
		for (; m->name; m++) {
			Native_SetStr(Native_Append(out), m->name);
		}
		NObj_Share(out);
		NObj_CallMethod(out, "sort", NULL, 0, &r);
		Native_Free(&r);
		return 1;
	}
	for (; m && m->name; m++) {
		if (!strcmp(m->name, name)) {
			NObj_NewCallback(out, m->name, m->fn, o->ctx, NULL, 0);
			NObj_SetCallback(out, v, NULL);
			return 1;
		}
	}
	if (o->ctype->getattr) {
		return o->ctype->getattr(o->ctx, name, out);
	}
	if (o->ctype->methods) {
		return Native_Raise(NE_ATTRIBUTE, "%s", name);
	}
	return Native_Raise(NE_ATTRIBUTE, "'%.50s' object has no attribute '%.400s'", o->ctype->name, name);
}

byte NObj_GetAttr(const nativeVal_t *v, const char *name, nativeVal_t *out) {
	modVarRef_t mv;
	if (ModVar_OfVal(v, &mv)) {
		/* a pack module: its variable in the registry */
		return ModVar_AttrVal(&mv, name, out);
	}
	if (v->type == NV_ENT) {
		return Native_EntGetAttr(v, name, out);
	}
	Native_SetNone(out);
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
			} else if (o->func == NF_PORT && o->ref->doc) {
				Native_SetText(out, o->ref->doc, (int)strlen(o->ref->doc));
			} else if (o->func == NF_CALLBACK && o->cb->doc) {
				Native_SetText(out, o->cb->doc, (int)strlen(o->cb->doc));
			}
			return 1;
		}
		if (!o->binds && !strcmp(name, "__self__")) {
			if (o->func == NF_BUILTIN) {
				Obj_SetRef(out, NObj_Keep(o->self));
			} else if (o->func == NF_CALLBACK) {
				Native_Copy(out, &o->cb->self);
			} else if (o->func == NF_BOUND) {
				Native_Copy(out, &o->data);
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
	case NO_COBJ:
		return Obj_CGetAttr(v, name, out);
	case NO_TYPE:
		/* type_getattr */
		if (!strcmp(name, "__name__")) {
			Native_SetText(out, o->name, (int)strlen(o->name));
			return 1;
		}
		if (!strcmp(name, "__doc__")) {
			/* no type of the game has a tp_doc */
			return 1;
		}
		if (!strcmp(name, "__members__")) {
			Native_SetEmpty(out, NV_LIST);
			Native_SetStr(Native_Append(out), "__doc__");
			Native_SetStr(Native_Append(out), "__name__");
			return 1;
		}
		break;
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
	if (Obj_IsKind(t, NO_COBJ)) {
		const nativeCType_t *ct = t->ref->ctype;
		if (!ct->setattr) {
			/* object.c Object_SetAttr of a type without tp_setattr */
			return Native_Raise(NE_TYPE, ct->getattr || ct->methods ? "object has read-only attributes" :
				"attribute-less object (assign or del)");
		}
		return ct->setattr(t->ref->ctx, name, v);
	}
	if (t->type == NV_ENT) {
		/* the attribute set of the handle (a script variable of the entity, the value as the handle's set takes it; a
		 * delete is a value of none of the forms); 0 with the message printed and no error when none takes it */
		nativeVal_t x;
		if (v) {
			NObj_ArgOf(v, &x);
		} else {
			Native_SetMissing(&x);
		}
		byte ok = Native_EntStore(Native_Ent(t), Native_EntName(t), name, &x);
		Native_Free(&x);
		return ok;
	}
	if (t->type == NV_LIST || t->type == NV_DICT || Obj_IsKind(t, NO_LIST) || Obj_IsKind(t, NO_DICT) ||
		Obj_IsKind(t, NO_FUNCTION) || Obj_IsKind(t, NO_TYPE)) {
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
	if (v->type != NV_REF) {
		return Native_Size(v);
	}
	if (v->ref->kind == NO_LIST || v->ref->kind == NO_DICT) {
		return v->ref->data.n;
	}
	if (v->ref->kind == NO_INSTANCE) {
		Obj_NoSpecial("__len__");
	} else if (v->ref->kind == NO_COBJ && v->ref->ctype->len) {
		return v->ref->ctype->len(v->ref->ctx);
	} else {
		Native_Raise(NE_TYPE, "len() of unsized object");
	}
	return -1;
}

byte NObj_GetItem(const nativeVal_t *v, const nativeVal_t *key, nativeVal_t *out) {
	Native_SetNone(out);
	if (v->type != NV_REF) {
		return Native_Subscript(v, key, out);
	}
	if (v->ref->kind == NO_LIST || v->ref->kind == NO_DICT) {
		return Native_Subscript(&v->ref->data, key, out);
	}
	if (v->ref->kind == NO_COBJ && v->ref->ctype->item) {
		/* object.c Object_GetItem of a sequence: an int index, the length added to a negative one */
		long i = key->type == NV_INT ? key->i : 0;
		if (key->type != NV_INT) {
			return Native_Raise(NE_TYPE, "sequence index must be integer");
		}
		if (i < 0 && v->ref->ctype->len) {
			int n = v->ref->ctype->len(v->ref->ctx);
			if (n < 0) {
				return 0;
			}
			i += n;
		}
		return v->ref->ctype->item(v->ref->ctx, i, out);
	}
	return v->ref->kind == NO_INSTANCE ? Obj_NoSpecial("__getitem__") : Native_Raise(NE_TYPE, "unsubscriptable object");
}

/* o[key] = x (x given) and del o[key] (x NULL) */
static byte Obj_SetItem(const nativeVal_t *v, const nativeVal_t *key, const nativeVal_t *x) {
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
		/* a sequence without item assignment (abstract.c Sequence_SetItem, abstract.c Sequence_DelItem) */
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

byte NObj_GetSlice(const nativeVal_t *v, long lo, long hi, nativeVal_t *out) {
	Native_SetNone(out);
	if (v->type != NV_REF) {
		return Native_Slice(v, lo, hi, out);
	}
	if (v->ref->kind == NO_LIST) {
		return Native_Slice(&v->ref->data, lo, hi, out);
	}
	if (v->ref->kind == NO_COBJ && v->ref->ctype->slice) {
		/* abstract.c Sequence_GetSlice: the length added once to a negative bound */
		if ((lo < 0 || hi < 0) && v->ref->ctype->len) {
			int n = v->ref->ctype->len(v->ref->ctx);
			if (n < 0) {
				return 0;
			}
			lo += lo < 0 ? n : 0;
			hi += hi < 0 ? n : 0;
		}
		return v->ref->ctype->slice(v->ref->ctx, lo, hi, out);
	}
	return v->ref->kind == NO_INSTANCE ? Obj_NoSpecial("__getslice__") : Native_Raise(NE_TYPE, "unsliceable object");
}

byte NObj_SetSlice(const nativeVal_t *v, long lo, long hi, const nativeVal_t *seq) {
	nativeObj_t *o = v->type == NV_REF ? v->ref : NULL;
	if (o && o->kind == NO_INSTANCE) {
		return Obj_NoSpecial(seq ? "__setslice__" : "__delslice__");
	}
	if (!o || o->kind != NO_LIST) {
		return Native_Raise(NE_TYPE, seq ? "object doesn't support slice assignment" :
			"object doesn't support slice deletion");
	}
	/* abstract.c Sequence_SetSlice adds the length to a negative bound once, list_ass_slice clamps */
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
	if (seq->type != NV_REF) {
		return Native_Contains(seq, x);
	}
	if (seq->ref->kind == NO_LIST) {
		return Native_Contains(&seq->ref->data, x);
	}
	if (seq->ref->kind == NO_COBJ && seq->ref->ctype->item) {
		/* cmp_member: the items until the IndexError */
		for (long i = 0;; i++) {
			nativeVal_t item;
			if (!seq->ref->ctype->item(seq->ref->ctx, i, &item)) {
				if (!Native_ErrIs(NE_INDEX)) {
					return -1;
				}
				Native_ErrClear();
				return 0;
			}
			int c = Native_Compare(x, &item);
			Native_Free(&item);
			if (c == 0) {
				return 1;
			}
		}
	}
	if (seq->ref->kind == NO_INSTANCE) {
		Obj_NoSpecial("__getitem__");
	} else {
		Native_Raise(NE_TYPE, "'in' or 'not in' needs sequence right argument");
	}
	return -1;
}

/* ---- calls ---- */

/* a bound C entry on its arguments: the value first, the arguments as a call through Python hands them to the entry
 * (Native_Pass), the result as it comes back from Python (NObj_CallArg) */
static byte Obj_CallBound(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t buf[1 + PORT_ARGS], *x = n < PORT_ARGS ? buf : Mem_Alloc((uint32_t)((size_t)(n + 1) * sizeof(*x))), r;
	byte ok = 1;
	int k = 1;
	Native_Copy(&x[0], &o->data);
	for (; ok && k <= n; k++) {
		ok = Native_Pass(&a[k - 1], &x[k]);
	}
	Native_SetNone(&r);
	ok = ok && o->entry(x, n + 1, &r);
	if (ok) {
		NObj_CallArg(&r, out);
	}
	Native_Free(&r);
	while (--k >= 0) {
		Native_Free(&x[k]);
	}
	if (x != buf) {
		Mem_Free(x);
	}
	return ok;
}

/* a native function on its arguments */
static byte Obj_CallFunc(nativeObj_t *o, const nativeVal_t *a, int n, nativeVal_t *out) {
	byte ok;
	switch (o->func) {
	case NF_PORT: {
		static portSite_t site = { .ref = &port_value, .global = 2, .file = __FILE__, .line = __LINE__ };
		if (n > PORT_ARGS) {
			/* more than a bound call packs: the entry on them as they are (its arity error) */
			return Port_KwCall(o->ref, a, n, NULL, out);
		}
		site.ref = o->ref;
		return Port_BoundCall(&site, NULL, a, n, out, 0);
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
	case NF_BOUND:
		return Obj_CallBound(o, a, n, out);
	}
	ok = obj_methods[o->method].fn(o->self, a, n, out);
	if (!ok) {
		Native_Free(out);
	}
	return ok;
}

/* a new instance of the native class c: classobject.c Instance_New */
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
	nativeVal_t call;
	Native_SetNone(out);
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
	case NO_MODULE:
	case NO_TYPE:
	case NO_COBJ:
		return Native_Raise(NE_TYPE, "call of non-function (type %s)", NObj_TypeName(f));
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

byte NObj_Callable(const nativeVal_t *v) {
	nativeVal_t call;
	if (v->type != NV_REF) {
		return 0;
	}
	switch (v->ref->kind) {
	case NO_FUNCTION:
	case NO_METHOD:
	case NO_CLASS:
		return 1;
	case NO_INSTANCE:
		if (!Obj_InstLook(v->ref, "__call__", &call)) {
			return 0;
		}
		Native_Free(&call);
		return 1;
	}
	return 0;
}

byte NObj_CallKw(const nativeVal_t *f, const nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out) {
	nativeVal_t call;
	Native_SetNone(out);
	if (!kw || !kw->n) {
		return NObj_Call(f, a, n, out);
	}
	nativeObj_t *o = f->type == NV_REF ? f->ref : NULL;
	if (o && o->kind == NO_FUNCTION && o->func == NF_PORT) {
		return Port_KwCall(o->ref, a, n, kw, out);
	}
	if (o && o->kind == NO_FUNCTION) {
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
		/* classobject.c Instance_New: the keywords go to __init__ */
		nativeVal_t inst, init, r;
		nativeObj_t *i = Obj_New(NO_INSTANCE);
		i->cls = NObj_Keep(o);
		i->dict = Obj_NewDictObj();
		Obj_SetRef(&inst, i);
		Native_SetNone(&init);
		Native_SetNone(&r);
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
	/* any other value is no callable (NObj_Call's TypeError) */
	return NObj_Call(f, a, n, out);
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
