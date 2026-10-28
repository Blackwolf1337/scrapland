/*
 * cPy_Function: callbacks by name for the engine and the scripts. Original source: PyInterface/Functions.cpp.
 * A function is either a Python function (PYF_PYTHON, parameters in a Python list) or a C function of the
 * registry (PYF_NATIVE, parameters written into a cFileMem). The parameter files are reused through a free list.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/pyfunction.h"

byte Py_IsReady(void) {
	return g_PythonReady;
}

/* a parameter file from the free list or a new one with a 0x200 byte buffer */
cFileMem *ParamFile_Get(void) {
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

/*
 * The function of the sphere tests (g_ChTypeSphereFunc, g_DoorSphereFunc and the others at rva 0x46679c): its static
 * initializer (rva 0x0554d0) has the constructor inlined without the fields that are 0 in the data, it only links it
 * into the ring. The atexit destructor is rva 0x38e3a0.
 */
#define g_SphereFunc ((cPy_Function *)Game_Ptr(0x46679c))

void PyFunction_FiniSphereFunc(void) {
	cPy_Function_dtor(g_SphereFunc);
}

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

cPy_Function *__thiscall cPy_Function_ctor(cPy_Function *self) {
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
byte __thiscall cPy_Function_NativeParams(cPy_Function *self) {
	if (!(self->flags & PYF_NATIVE) || self->params) {
		return 0;
	}
	cFileMem *f = ParamFile_Get();
	self->params = f;
	f->buffer[0] = 0;
	f->base.vftable->Open(&f->base, "", 7);
	return 1;
}

/* creates the parameter list of a Python function */
void __thiscall cPy_Function_PyParams(cPy_Function *self) {
	if ((self->flags & PYF_PYTHON) && !self->params) {
		self->params = PyList_New(0);
		CHECK(self->params);
	}
}

/* drops the parameters: the list, or the file goes back to the free list */
void __thiscall cPy_Function_ReleaseParams(cPy_Function *self) {
	if (self->flags & PYF_PYTHON) {
		PyObject *list = self->params;
		if (!list) {
			return;
		}
		Py_DECREF(list);
	} else {
		if (!(self->flags & PYF_NATIVE) || !self->params) {
			return;
		}
		cFileMem *f = self->params;
		f->base.vftable->Close(&f->base);
		ParamFile_Free(f);
	}
	self->params = NULL;
}

void __thiscall cPy_Function_ReleasePyParams(cPy_Function *self) {
	if ((self->flags & PYF_PYTHON) && self->params) {
		PyObject *list = self->params;
		Py_DECREF(list);
		self->params = NULL;
	}
}

/* the function of a Python binding is borrowed, only the parameter list is released */
void __thiscall cPy_Function_ClearPython(cPy_Function *self) {
	if (self->flags & PYF_PYTHON) {
		self->func = NULL;
		cPy_Function_ReleasePyParams(self);
		self->flags &= ~PYF_PYTHON;
	}
}

void __thiscall cPy_Function_Clear(cPy_Function *self) {
	if (self->flags & PYF_PYTHON) {
		cPy_Function_ClearPython(self);
		return;
	}
	cPy_Function_ReleaseParams(self);
	self->flags &= ~PYF_NATIVE;
	self->func = NULL;
}

void __thiscall cPy_Function_dtor(cPy_Function *self) {
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
byte __thiscall cPy_Function_SetNative(cPy_Function *self, nativefunc_t func) {
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

/* binds module.func; only a Python binding is cleared before */
byte __thiscall cPy_Function_SetPython(cPy_Function *self, const char *module, const char *func) {
	cPy_Function_ClearPython(self);
	if (!module || !func) {
		return 0;
	}
	PyObject *m = PyImport_AddModule((char *)module);
	if (!m) {
		Con_Warnf("cPy_Function::SetFunc: Module does not exist.\n '%s'.'%s'\n", module, func);
	} else {
		self->func = PyDict_GetItemString(PyModule_GetDict(m), (char *)func);
		if (!self->func) {
			Con_Warnf("cPy_Function::SetFunc: No function in module.\n '%s'.'%s'\n", module, func);
		} else if (!PyCallable_Check(self->func)) {
			Con_Warnf("cPy_Function::SetFunc: Element found in module but isn\xb4t callable.\n '%s'.'%s' \n",
				module, func);
		} else {
			self->flags |= PYF_PYTHON;
			return 1;
		}
	}
	cPy_Function_Clear(self);
	return 0;
}

byte __thiscall cPy_Function_SetFunc(cPy_Function *self, const char *name) {
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

int __thiscall cPy_Function_IsValid(cPy_Function *self) {
	return (self->flags & (PYF_PYTHON | PYF_NATIVE)) != 0;
}

void __thiscall cPy_Function_AddParamI(cPy_Function *self, int v) {
	if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile *f = self->params;
		f->vftable->Write(f, &v, sizeof(v));
	} else if (self->flags & PYF_PYTHON) {
		cPy_Function_PyParams(self);
		PyObject *o = PyInt_FromLong(v);
		PyList_Append(self->params, o);
		Py_DECREF(o);
	}
}

void __thiscall cPy_Function_AddParamF(cPy_Function *self, float v) {
	if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile *f = self->params;
		f->vftable->Write(f, &v, sizeof(v));
	} else if (self->flags & PYF_PYTHON) {
		cPy_Function_PyParams(self);
		PyObject *o = PyFloat_FromDouble(v);
		PyList_Append(self->params, o);
		Py_DECREF(o);
	}
}

void __thiscall cPy_Function_AddParamS(cPy_Function *self, const char *v) {
	if (!(self->flags & (PYF_PYTHON | PYF_NATIVE))) {
		return;
	}
	if (!v) {
		v = "";
	}
	if (self->flags & PYF_NATIVE) {
		cPy_Function_NativeParams(self);
		cAbstractFile_WriteString(self->params, v);
	} else {
		cPy_Function_PyParams(self);
		PyObject *o = PyString_FromString((char *)v);
		PyList_Append(self->params, o);
		Py_DECREF(o);
	}
}

/*
 * With clear a C function gets NULL as its parameter file: the original reads the field again after the clear.
 * A Python function still gets the tuple made before.
 */
int __thiscall cPy_Function_Execute(cPy_Function *self, byte clear) {
	int result = 0;

	CHECK(!(self->flags & PYF_BLOCKED));
	self->flags |= PYF_BLOCKED;
	if (self->flags & PYF_PYTHON) {
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
					PyErr_Print();
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
		cPy_Function_NativeParams(self);
		cAbstractFile *f = self->params;
		f->vftable->Seek(f, 0);
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
		f->base.vftable->Destroy(&f->base, 1);
		freed++;
	}
	Con_Printf("%d Functions %d cleared\n", n, freed);
}

/* registry of C functions: inserted into the tree on construction (static objects) */
void __thiscall cPy_NativeFunc_Insert(cPy_NativeFunc *root, cPy_NativeFunc *node) {
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

cPy_NativeFunc *__thiscall cPy_NativeFunc_ctor(cPy_NativeFunc *self, const char *name, nativefunc_t func) {
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
