/*
 * cPy_Function: callbacks by name for the engine and the scripts. Original source: PyInterface/Functions.cpp.
 * A function is a Python function (PYF_PYTHON, parameters in a Python list), a C function of the registry
 * (PYF_NATIVE, parameters written into a cFileMem) or, in ScrapC, the native entry of a port function (PYF_ENTRY,
 * stage 4 of docs/objlib-plan.md: "Module.Func" names the port's own C function, parameters as native values). The
 * parameter files and blocks are reused through free lists.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/modvars.h"
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

/* the parameters of a port entry (PYF_ENTRY): native values as the adapter's Mode_Args makes them of the Python list */
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

/* OBJLIB 7: the calls of Execute by kind (the test channel) */
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

/* creates the parameter list of a Python function */
static void cPy_Function_PyParams(cPy_Function *self) {
	if ((self->flags & PYF_PYTHON) && !self->params) {
		self->params = PyList_New(0);
		CHECK(self->params);
	}
}

/* drops the parameters: the list, or the file or the block goes back to its free list */
static void cPy_Function_ReleaseParams(cPy_Function *self) {
	if (self->flags & PYF_PYTHON) {
		PyObject *list = self->params;
		if (!list) {
			return;
		}
		Py_DECREF(list);
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

static void cPy_Function_ReleasePyParams(cPy_Function *self) {
	if ((self->flags & PYF_PYTHON) && self->params) {
		PyObject *list = self->params;
		Py_DECREF(list);
		self->params = NULL;
	}
}

/* the function of a Python binding is borrowed, only the parameter list is released */
static void cPy_Function_ClearPython(cPy_Function *self) {
	if (self->flags & PYF_PYTHON) {
		self->func = NULL;
		cPy_Function_ReleasePyParams(self);
		self->flags &= ~PYF_PYTHON;
	}
}

void cPy_Function_Clear(cPy_Function *self) {
	if (self->flags & PYF_PYTHON) {
		cPy_Function_ClearPython(self);
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
 * binds module.func; only a Python binding is cleared before. The binding is what the module holds at this time; when
 * that is the port's own C function of module.func, its native entry (PYF_ENTRY), else the object (PYF_PYTHON).
 */
static byte cPy_Function_SetPython(cPy_Function *self, const char *module, const char *func) {
	cPy_Function_ClearPython(self);
	if (!module || !func) {
		return 0;
	}
	PyObject *m = Script_ModuleAdd(module);
	if (!m) {
		Con_Warnf("cPy_Function::SetFunc: Module does not exist.\n '%s'.'%s'\n", module, func);
	} else {
		self->func = ModVar_DictGet(PyModule_GetDict(m), func);
		if (!self->func) {
			Con_Warnf("cPy_Function::SetFunc: No function in module.\n '%s'.'%s'\n", module, func);
		} else if (!PyCallable_Check(self->func)) {
			Con_Warnf("cPy_Function::SetFunc: Element found in module but isn\xb4t callable.\n '%s'.'%s' \n",
				module, func);
		} else {
			portSite_t *site = Port_Callback(module, func, self->func);
			if (site) {
				self->func = site;
				self->flags |= PYF_ENTRY;
			} else {
				self->flags |= PYF_PYTHON;
			}
			return 1;
		}
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
	return cPy_Function_SetPython(self, module, dot + 1);
}

int cPy_Function_IsValid(cPy_Function *self) {
	return (self->flags & (PYF_PYTHON | PYF_NATIVE | PYF_ENTRY)) != 0;
}

const char *cPy_Function_Name(cPy_Function *self) {
	static char name[256];

	name[0] = 0;
	if (self->flags & PYF_NATIVE) {
		snprintf(name, sizeof(name), "!native");
	} else if (self->flags & PYF_ENTRY) {
		/* the __name__ of the C function, without the module (not a Python function) */
		snprintf(name, sizeof(name), "%s", ((portSite_t *)self->func)->ref->name);
	} else if ((self->flags & PYF_PYTHON) && self->func) {
		PyObject *f = self->func;
		PyObject *fn = PyObject_GetAttrString(f, "__name__");
		PyObject *mod = PyFunction_Check(f) ? PyDict_GetItemString(PyFunction_GetGlobals(f), "__name__") : NULL;
		if (fn && PyString_Check(fn)) {
			snprintf(name, sizeof(name), "%s%s%s", mod && PyString_Check(mod) ? PyString_AsString(mod) : "",
				mod && PyString_Check(mod) ? "." : "", PyString_AsString(fn));
		}
		Py_XDECREF(fn);
		Native_ErrClear();
	}
	return name;
}

/*
 * OBJLIB 7: a parameter past PORT_ARGS (an entry takes no more; no callback of the engine passes more): the binding
 * goes back to the port's C function object (Port_CallbackFunc: the same adapter as the object bound at SetFunc),
 * PYF_PYTHON with the parameters as its list (what Mode_Args makes of them), so the call is the Python call of the
 * original with its errors.
 */
static void cPy_Function_EntryToPython(cPy_Function *self) {
	portSite_t *site = self->func;
	cPy_EntryParams *p = self->params;
	PyObject *list = PyList_New(0);
	CHECK(list);
	for (int i = 0; i < p->n; i++) {
		PyObject *o = Native_ToPy(&p->a[i]);
		CHECK(o);
		PyList_Append(list, o);
		Py_DECREF(o);
	}
	EntryParams_Free(p);
	self->func = Port_CallbackFunc(site);
	self->params = list;
	self->flags = (self->flags & ~PYF_ENTRY) | PYF_PYTHON;
}

/* the place of the next parameter of a port entry; NULL when the binding went back to Python with it */
static nativeVal_t *cPy_Function_EntryParam(cPy_Function *self) {
	cPy_EntryParams *p = self->params;
	if (!p) {
		self->params = p = EntryParams_Get();
	}
	if (p->n == PORT_ARGS) {
		cPy_Function_EntryToPython(self);
		return NULL;
	}
	return &p->a[p->n++];
}

void cPy_Function_AddParamI(cPy_Function *self, int v) {
	nativeVal_t *a;
	if ((self->flags & PYF_ENTRY) && (a = cPy_Function_EntryParam(self)) != NULL) {
		Native_SetInt(a, v);
	} else if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile *f = self->params;
		VFile_Write(f, &v, sizeof(v));
	} else if (self->flags & PYF_PYTHON) {
		cPy_Function_PyParams(self);
		PyObject *o = PyInt_FromLong(v);
		PyList_Append(self->params, o);
		Py_DECREF(o);
	}
}

void cPy_Function_AddParamF(cPy_Function *self, float v) {
	nativeVal_t *a;
	if ((self->flags & PYF_ENTRY) && (a = cPy_Function_EntryParam(self)) != NULL) {
		Native_SetFloat(a, v);
	} else if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile *f = self->params;
		VFile_Write(f, &v, sizeof(v));
	} else if (self->flags & PYF_PYTHON) {
		cPy_Function_PyParams(self);
		PyObject *o = PyFloat_FromDouble(v);
		PyList_Append(self->params, o);
		Py_DECREF(o);
	}
}

void cPy_Function_AddParamS(cPy_Function *self, const char *v) {
	nativeVal_t *a;
	if (!(self->flags & (PYF_PYTHON | PYF_NATIVE | PYF_ENTRY))) {
		return;
	}
	if (!v) {
		v = "";
	}
	if ((self->flags & PYF_ENTRY) && (a = cPy_Function_EntryParam(self)) != NULL) {
		Native_SetText(a, v, (int)strlen(v));
	} else if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile_WriteString(self->params, v);
	} else if (self->flags & PYF_PYTHON) {
		cPy_Function_PyParams(self);
		PyObject *o = PyString_FromString((char *)v);
		PyList_Append(self->params, o);
		Py_DECREF(o);
	}
}

/*
 * The call of a port entry as PyEval_CallObject called its adapter (Mode_Args, the entry, Mode_Out), with the depth
 * check of the direct calls (Port_BoundCall): the int result, else 0; a failure printed as the Python call printed it.
 * The result goes through the conversions of the adapter and the int check (Native_Pass: they can fail). Not inlined:
 * the result value stays off the frame of cPy_Function_Execute, which is on the stack during a Python call.
 */
static __attribute__((noinline)) int cPy_Function_CallEntry(portSite_t *site, const cPy_EntryParams *p) {
	nativeVal_t res;
	int result = 0;

	if (!Port_BoundCall(site, NULL, p ? p->a : NULL, p ? p->n : 0, &res, NULL)) {
		Native_ErrPrint();
	} else if (res.type == NV_INT) {
		result = (int)res.i;
	}
	Native_Free(&res);
	return result;
}

/*
 * With clear a C function gets NULL as its parameter file: the original reads the field again after the clear.
 * A Python function still gets the tuple made before, a port entry the parameters added before.
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
			result = cPy_Function_CallEntry(site, p);
			if (p) {
				EntryParams_Free(p);
			}
		}
	} else if (self->flags & PYF_PYTHON) {
		g_PyFunctionCalls[0]++;
		if (!Py_IsReady()) {
			Con_Warnf("WARNING : Impossible to execute \n Python closed!.\n");
		} else {
			PyObject *func = self->func;
			PyObject *args = self->params ? PyList_AsTuple(self->params) : PyTuple_New(0);
			if (clear) {
				cPy_Function_Clear(self);
			}
			if (!func || !args) {
				Con_Warnf("WARNING : No pyton function preloaded : \n");
			} else {
				PyObject *r = PyEval_CallObject(func, args);
				if (!r) {
					Native_ErrPrint();
				} else {
					if (PyInt_Check(r)) {
						result = PyInt_AsLong(r);
					}
					Py_DECREF(r);
				}
				Py_DECREF(args);
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

void cPy_Function_Counts(unsigned long *python, unsigned long *native, unsigned long *entry, byte reset) {
	*python = g_PyFunctionCalls[0];
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
