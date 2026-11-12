/*
 * cPy_Function: callbacks by name for the engine and the scripts. Original source: PyInterface/Functions.cpp.
 * A function is a Python function (in ScrapC a function value, PYF_VALUE, parameters in a tuple value), a C function
 * of the registry (PYF_NATIVE, parameters written into a cFileMem) or, in ScrapC, the native entry of a port function
 * (PYF_ENTRY, stage 4 of docs/objlib-plan.md: "Module.Func" names a port function, parameters as native values). The
 * parameter files and blocks are reused through free lists.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/modvars.h"
#include "scrap/nativeobj.h"
#include "scrap/pyfunction.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

byte Py_IsReady(void) {
	return g_PythonReady;
}

/* memory files of C function parameters for reuse; the first dword of the buffer links them */
static cFileMem *g_FreeParamFiles;

/* a parameter file from the free list or a new one with a 0x200 byte buffer */
static cFileMem *ParamFile_Get(void) {
	cFileMem *f = g_FreeParamFiles;
	if (f) {
		g_FreeParamFiles = *(cFileMem **)f->buffer;
		return f;
	}
	void *mem = operator_new(sizeof(cFileMem));
	if (!mem) {
		return NULL;
	}
	return cFileMem_ctor(mem, Mem_Alloc(0x200), 0x200, 1);
}

static void ParamFile_Free(cFileMem *f) {
	*(cFileMem **)f->buffer = g_FreeParamFiles;
	g_FreeParamFiles = f;
}

/* the parameters of a port entry (PYF_ENTRY): native values as the adapter's Mode_Args made them of the Python list */
typedef struct cPy_EntryParams {
	nativeVal_t a[PORT_ARGS];
	int n;
	struct cPy_EntryParams *next;	/* in the free list */
} cPy_EntryParams;

static cPy_EntryParams *g_FreeEntryParams;

static cPy_EntryParams *EntryParams_Get(void) {
	cPy_EntryParams *p = g_FreeEntryParams;
	if (p) {
		g_FreeEntryParams = p->next;
	} else {
		p = Mem_Alloc(sizeof(*p));
	}
	p->n = 0;
	return p;
}

/* the values freed, the block back to the free list */
static void EntryParams_Free(cPy_EntryParams *p) {
	for (int i = 0; i < p->n; i++) {
		Native_Free(&p->a[i]);
	}
	p->next = g_FreeEntryParams;
	g_FreeEntryParams = p;
}

/* the calls of Execute by kind: value, C function, port entry (the test channel) */
static unsigned long g_PyFunctionCalls[3];

/*
 * The function of the sphere tests (g_ChTypeSphereFunc, g_DoorSphereFunc and the others): its static
 * initializer has the constructor inlined without the fields that are 0 in the data, it only links it
 * into the ring. The atexit destructor is.
 */
extern cPy_Function g_ChTypeSphereFunc;
#define g_SphereFunc ((&g_ChTypeSphereFunc))

static void PyFunction_FiniSphereFunc(void) {
	cPy_Function_dtor(g_SphereFunc);
}

static cPy_Function *g_PyFunctions;

void PyFunction_InitSphereFunc(void) {
	cPy_Function *head = g_PyFunctions, *self = g_SphereFunc;

	if (!head) {
		self->next = self;
		self->prev = self;
	} else {
		self->prev = head->prev;
		self->next = head;
		head->prev->next = self;
		head->prev = self;
	}
	g_PyFunctions = self;
	crt_atexit(PyFunction_FiniSphereFunc);
}

cPy_Function *cPy_Function_ctor(cPy_Function *self) {
	cPy_Function *head = g_PyFunctions;

	self->func = NULL;
	self->params = NULL;
	self->flags = 0;
	g_PyFunctions = self;
	if (!head) {
		self->next = self;
		self->prev = self;
		return self;
	}
	self->prev = head->prev;
	self->next = head;
	head->prev->next = self;
	head->prev = self;
	return self;
}

/* creates the parameter file of a C function; 1 if it made one */
static byte cPy_Function_NativeParams(cPy_Function *self) {
	if (!(self->flags & PYF_NATIVE) || self->params) {
		return 0;
	}
	cFileMem *f = ParamFile_Get();
	self->params = f;
	f->buffer[0] = 0;
	VFile_Open(f, "", 7);
	return 1;
}

/* a new value (None), for the function and the parameters of PYF_VALUE */
static nativeVal_t *Value_New(void) {
	nativeVal_t *v = Mem_Alloc(sizeof(*v));
	Native_SetNone(v);
	return v;
}

static void Value_Delete(nativeVal_t *v) {
	Native_Free(v);
	Mem_Free(v);
}

/* the place of the next parameter of a function value, in its tuple (made at the first one) */
static nativeVal_t *cPy_Function_ValueParam(cPy_Function *self) {
	nativeVal_t *t = self->params;
	if (!t) {
		self->params = t = Value_New();
		Native_SetEmpty(t, NV_TUPLE);
	}
	return Native_Append(t);
}

/* drops the parameters: the tuple, or the file or the block goes back to its free list */
static void cPy_Function_ReleaseParams(cPy_Function *self) {
	if (self->flags & PYF_VALUE) {
		if (!self->params) {
			return;
		}
		Value_Delete(self->params);
	} else if (self->flags & PYF_ENTRY) {
		if (!self->params) {
			return;
		}
		EntryParams_Free(self->params);
	} else {
		if (!(self->flags & PYF_NATIVE) || !self->params) {
			return;
		}
		cFileMem *f = self->params;
		VFile_Close(f);
		ParamFile_Free(f);
	}
	self->params = NULL;
}

/* the binding of a Python function as the original clears it (the parameter list released); here the value and its
 * tuple */
static void cPy_Function_ClearValue(cPy_Function *self) {
	if (self->flags & PYF_VALUE) {
		if (self->func) {
			Value_Delete(self->func);
			self->func = NULL;
		}
		cPy_Function_ReleaseParams(self);
		self->flags &= ~PYF_VALUE;
	}
}

void cPy_Function_Clear(cPy_Function *self) {
	if (self->flags & PYF_VALUE) {
		cPy_Function_ClearValue(self);
		return;
	}
	cPy_Function_ReleaseParams(self);
	self->flags &= ~(PYF_NATIVE | PYF_ENTRY);
	self->func = NULL;
}

void cPy_Function_dtor(cPy_Function *self) {
	cPy_Function_ReleaseParams(self);
	cPy_Function_Clear(self);
	if (!g_pApp) {
		return;
	}
	if (g_PyFunctions == self) {
		g_PyFunctions = self->next;
	}
	self->prev->next = self->next;
	self->next->prev = self->prev;
	if (g_PyFunctions == self) {
		CHECK(self->next == self->prev);
		CHECK(self->next == self);
		g_PyFunctions = NULL;
	}
}

/* binds a C function; its old parameter file goes to the free list without Close. Always 1. */
byte cPy_Function_SetNative(cPy_Function *self, nativefunc_t func) {
	cPy_Function_Clear(self);
	if (func) {
		cFileMem *f = self->params;
		if (f) {
			ParamFile_Free(f);
			self->params = NULL;
		}
		self->flags |= PYF_NATIVE;
		self->func = (void *)func;
	}
	return 1;
}

/*
 * binds module.func; only a binding of a value is cleared before (the original: of a Python function). The binding is
 * what the module holds at this time, read as a native value (ModVar_GetVal: no crossing); when that is a port
 * function, its native entry (PYF_ENTRY), else a copy of the value (PYF_VALUE).
 */
static byte cPy_Function_SetValue(cPy_Function *self, const char *module, const char *func) {
	modVarRef_t ref;
	nativeVal_t v;
	cPy_Function_ClearValue(self);
	if (!module || !func) {
		return 0;
	}
	nativeVal_t m;
	if (!Script_ModuleAdd(module, &m)) {
		Con_Warnf("cPy_Function::SetFunc: Module does not exist.\n '%s'.'%s'\n", module, func);
	} else {
		/* the store holds the module while the ref is used */
		ModVar_OfVal(&m, &ref);
		Native_Free(&m);
		if (!ModVar_GetVal(&ref, func, &v)) {
			Con_Warnf("cPy_Function::SetFunc: No function in module.\n '%s'.'%s'\n", module, func);
		} else if (!NObj_Callable(&v)) {
			Con_Warnf("cPy_Function::SetFunc: Element found in module but isn\xb4t callable.\n '%s'.'%s' \n",
				module, func);
		} else {
			portSite_t *site = Port_CallbackOf(module, func, &v);
			if (site) {
				self->func = site;
				self->flags |= PYF_ENTRY;
				Native_Free(&v);
			} else {
				self->func = Value_New();
				*(nativeVal_t *)self->func = v;
				self->flags |= PYF_VALUE;
			}
			return 1;
		}
		Native_Free(&v);
	}
	cPy_Function_Clear(self);
	return 0;
}

static cPy_NativeFunc *g_NativeFuncs;

/* the name of a registered C function (a walk of the tree), NULL when it is none */
static const char *NativeFunc_Find(const cPy_NativeFunc *n, nativefunc_t func) {
	for (; n; n = n->greater) {
		if (n->func == func) {
			return n->name;
		}
		const char *found = NativeFunc_Find(n->less, func);
		if (found) {
			return found;
		}
	}
	return NULL;
}

const char *cPy_NativeFunc_Name(nativefunc_t func) {
	return NativeFunc_Find(g_NativeFuncs, func);
}

byte cPy_Function_SetFunc(cPy_Function *self, const char *name) {
	char module[0x100];

	cPy_Function_Clear(self);
	if (!name || !*name) {
		return 1;
	}
	if (*name == '!') {
		for (cPy_NativeFunc *n = g_NativeFuncs; n;) {
			int res = strcmp(n->name, name + 1);
			if (!res) {
				return cPy_Function_SetNative(self, n->func);
			}
			n = res < 0 ? n->greater : n->less;
		}
		return 0;
	}
	const char *dot = strchr(name, '.');
	if (!dot) {
		Con_Warnf("cPy_Function::SetFunc: Incorrect function format\n '%s'", name);
		return 0;
	}
	size_t len = (size_t)(dot - name);
	/* the original copies first and checks after, a module name of 0x100 or more ends the game */
	if (len >= sizeof(module)) {
		crt_report_rangecheckfailure();
	}
	memcpy(module, name, len);
	module[len] = 0;
	if (!Py_IsReady()) {
		return 1;
	}
	return cPy_Function_SetValue(self, module, dot + 1);
}

int cPy_Function_IsValid(cPy_Function *self) {
	return (self->flags & (PYF_VALUE | PYF_NATIVE | PYF_ENTRY)) != 0;
}

const char *cPy_Function_Name(cPy_Function *self) {
	static char name[256];

	name[0] = 0;
	if (self->flags & PYF_NATIVE) {
		snprintf(name, sizeof(name), "!native");
	} else if (self->flags & PYF_ENTRY) {
		/* the __name__ of the C function, without the module (not a Python function) */
		snprintf(name, sizeof(name), "%s", ((portSite_t *)self->func)->ref->name);
	} else if ((self->flags & PYF_VALUE) && self->func) {
		/* __name__ (no Python function is left, so never "module.function") */
		nativeVal_t fn;
		if (NObj_GetAttr(self->func, "__name__", &fn) && fn.type == NV_STR) {
			snprintf(name, sizeof(name), "%s", Native_Text(&fn));
		}
		Native_Free(&fn);
		Native_ErrClear();
	}
	return name;
}

/*
 * A parameter past PORT_ARGS (an entry takes no more; no callback of the engine passes more): the binding becomes the
 * port function as a value (PYF_VALUE) with the parameters as its tuple, so the call is the call of the function
 * object with its errors (the arity error of its adapter).
 */
static void cPy_Function_EntryToValue(cPy_Function *self) {
	portSite_t *site = self->func;
	cPy_EntryParams *p = self->params;
	nativeVal_t *t = Value_New();
	Native_SetEmpty(t, NV_TUPLE);
	for (int i = 0; i < p->n; i++) {
		Native_Copy(Native_Append(t), &p->a[i]);
	}
	EntryParams_Free(p);
	self->func = Value_New();
	NObj_NewPortFunc(self->func, site->ref);
	self->params = t;
	self->flags = (self->flags & ~PYF_ENTRY) | PYF_VALUE;
}

/* the place of the next parameter: in the block of a port entry or the tuple of a value; NULL for a C function or
 * without a binding */
static nativeVal_t *cPy_Function_Param(cPy_Function *self) {
	if (self->flags & PYF_ENTRY) {
		cPy_EntryParams *p = self->params;
		if (!p) {
			self->params = p = EntryParams_Get();
		}
		if (p->n < PORT_ARGS) {
			return &p->a[p->n++];
		}
		cPy_Function_EntryToValue(self);
	}
	return self->flags & PYF_VALUE ? cPy_Function_ValueParam(self) : NULL;
}

void cPy_Function_AddParamI(cPy_Function *self, int v) {
	nativeVal_t *a = cPy_Function_Param(self);
	if (a) {
		Native_SetInt(a, v);
	} else if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile *f = self->params;
		VFile_Write(f, &v, sizeof(v));
	}
}

void cPy_Function_AddParamF(cPy_Function *self, float v) {
	nativeVal_t *a = cPy_Function_Param(self);
	if (a) {
		Native_SetFloat(a, v);
	} else if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile *f = self->params;
		VFile_Write(f, &v, sizeof(v));
	}
}

void cPy_Function_AddParamS(cPy_Function *self, const char *v) {
	if (!(self->flags & (PYF_VALUE | PYF_NATIVE | PYF_ENTRY))) {
		return;
	}
	if (!v) {
		v = "";
	}
	nativeVal_t *a = cPy_Function_Param(self);
	if (a) {
		Native_SetText(a, v, (int)strlen(v));
	} else if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile_WriteString(self->params, v);
	}
}

/*
 * The call of a port entry (site, the parameters a, n) as PyEval_CallObject called its adapter (Mode_Args, the entry,
 * Mode_Out), with the depth check of the direct calls (Port_BoundCall), or of a function value f natively
 * (Port_CallKept): the int result, else 0; a failure printed as the Python call printed it. The result of an entry
 * goes through the conversions of the adapter and the int check (Native_Pass: they can fail). Not inlined: the result
 * value stays off the frame of cPy_Function_Execute, which is on the stack during a call.
 */
static __attribute__((noinline)) int cPy_Function_Call(portSite_t *site, const nativeVal_t *f, const nativeVal_t *a,
	int n) {
	nativeVal_t res;
	int result = 0;

	if (!Port_CallKept(site, f, a, n, &res)) {
		Native_ErrPrint();
	} else if (res.type == NV_INT) {
		result = (int)res.i;
	}
	Native_Free(&res);
	return result;
}

/*
 * With clear a C function gets NULL as its parameter file: the original reads the field again after the clear.
 * A function value still gets the tuple made before, a port entry the parameters added before.
 */
int cPy_Function_Execute(cPy_Function *self, byte clear) {
	int result = 0;

	CHECK(!(self->flags & PYF_BLOCKED));
	self->flags |= PYF_BLOCKED;
	if (self->flags & PYF_ENTRY) {
		g_PyFunctionCalls[2]++;
		if (!Py_IsReady()) {
			Con_Warnf("WARNING : Impossible to execute \n Python closed!.\n");
		} else {
			portSite_t *site = self->func;
			/* the call owns the parameters, as the Python call owns its tuple: the callback may bind this function
			 * again or add parameters (dropped after the call, as the list was) */
			cPy_EntryParams *p = self->params;
			self->params = NULL;
			if (clear) {
				cPy_Function_Clear(self);
			}
			result = cPy_Function_Call(site, NULL, p ? p->a : NULL, p ? p->n : 0);
			if (p) {
				EntryParams_Free(p);
			}
		}
	} else if (self->flags & PYF_VALUE) {
		g_PyFunctionCalls[0]++;
		if (!Py_IsReady()) {
			Con_Warnf("WARNING : Impossible to execute \n Python closed!.\n");
		} else {
			/* the call holds the function (the callback may bind this function again) and owns the parameters, as
			 * the Python call held its function and owned its tuple */
			nativeVal_t f;
			nativeVal_t *args = self->params;
			Native_Copy(&f, self->func);
			self->params = NULL;
			if (clear) {
				cPy_Function_Clear(self);
			}
			result = cPy_Function_Call(NULL, &f, args ? args->items : NULL, args ? args->n : 0);
			Native_Free(&f);
			if (args) {
				Value_Delete(args);
			}
		}
	} else if (self->flags & PYF_NATIVE) {
		g_PyFunctionCalls[1]++;
		cPy_Function_NativeParams(self);
		cAbstractFile *f = self->params;
		VFile_Seek(f, 0);
		nativefunc_t func = (nativefunc_t)self->func;
		if (clear) {
			cPy_Function_Clear(self);
		}
		func(self->params);
	}
	cPy_Function_ReleaseParams(self);
	self->flags &= ~PYF_BLOCKED;
	return result;
}

void cPy_Function_ClearAll(void) {
	int n = 0, freed = 0;

	for (cPy_Function *f = g_PyFunctions; f; n++) {
		cPy_Function_Clear(f);
		f = f->next;
		if (f == g_PyFunctions) {
			break;
		}
	}
	for (cFileMem *f = g_FreeParamFiles; f; f = g_FreeParamFiles) {
		g_FreeParamFiles = *(cFileMem **)f->buffer;
		VFile_Destroy(f, 1);
		freed++;
	}
	/* the blocks of the port entries too (not in the count of the original's line) */
	for (cPy_EntryParams *p = g_FreeEntryParams; p; p = g_FreeEntryParams) {
		g_FreeEntryParams = p->next;
		Mem_Free(p);
	}
	Con_Printf("%d Functions %d cleared\n", n, freed);
}

void cPy_Function_Counts(unsigned long *value, unsigned long *native, unsigned long *entry, byte reset) {
	*value = g_PyFunctionCalls[0];
	*native = g_PyFunctionCalls[1];
	*entry = g_PyFunctionCalls[2];
	if (reset) {
		memset(g_PyFunctionCalls, 0, sizeof(g_PyFunctionCalls));
	}
}

/* registry of C functions: inserted into the tree on construction (static objects) */
static void cPy_NativeFunc_Insert(cPy_NativeFunc *root, cPy_NativeFunc *node) {
	cPy_NativeFunc **link = &root;
	do {
		int res = strcmp(node->name, (*link)->name);
		if (!res) {
			CHECK(res != 0);
		}
		link = res > 0 ? &(*link)->greater : &(*link)->less;
	} while (*link);
	*link = node;
}

cPy_NativeFunc *cPy_NativeFunc_ctor(cPy_NativeFunc *self, const char *name, nativefunc_t func) {
	self->name = name;
	self->func = func;
	self->greater = NULL;
	self->less = NULL;
	if (!g_NativeFuncs) {
		g_NativeFuncs = self;
	} else {
		cPy_NativeFunc_Insert(g_NativeFuncs, self);
	}
	return self;
}
