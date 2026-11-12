/*
 * PyInterface/Functions.cpp: cPy_Function, a callback by name. "Module.function" binds what the module holds under
 * that name: the native entry of a port function (PYF_ENTRY, the registry of stage 4 of docs/objlib-plan.md:
 * Port_CallbackOf), else the value itself (PYF_VALUE: a native callback, a lambda or recorder of the test channel, a
 * class, an instance, a method, an engine module function, everything under SCRAP_PORT_SLOW=1), called natively
 * (Port_CallKept). "!name" binds a C function of the engine registry (cPy_NativeFunc). Parameters go into a block of
 * native values (PYF_ENTRY), a tuple value (PYF_VALUE) or, for C functions, a memory file. Every cPy_Function is in
 * one ring (g_PyFunctions).
 */
#ifndef SCRAP_PYFUNCTION_H
#define SCRAP_PYFUNCTION_H

#include "scrap/filesys.h"
#include "scrap/python.h"

#define PYF_VALUE 0x1			/* ScrapC: a function value (the Python function of the original) */
#define PYF_NATIVE 0x2
#define PYF_BLOCKED 0x4			/* while Execute runs */
#define PYF_ENTRY 0x8			/* ScrapC: the native entry of a port function */

typedef void (*nativefunc_t)(cAbstractFile *params);

typedef struct cPy_Function {
	struct cPy_Function *next;
	struct cPy_Function *prev;
	/* nativeVal_t * (PYF_VALUE: a copy of the value, owned; the original borrowed the Python function from the module
	 * dictionary), nativefunc_t or portSite_t * (PYF_ENTRY: its ref and the counters of the callback) */
	void *func;
	/* nativeVal_t * (PYF_VALUE: an NV_TUPLE, owned), cFileMem * or cPy_EntryParams * (PYF_ENTRY, from a free list) */
	void *params;
	byte flags;			/* PYF_ */
	byte pad_11[3];
} cPy_Function;

/* an entry of the registry of C functions, a binary tree by name (static objects of the engine) */
typedef struct cPy_NativeFunc {
	const char *name;
	nativefunc_t func;
	struct cPy_NativeFunc *greater;	/* names after this one (strcmp) */
	struct cPy_NativeFunc *less;
} cPy_NativeFunc;

/* the name of a registered C function, NULL when it is none (ScrapC only, for SAct.DumpEvents) */
const char *cPy_NativeFunc_Name(nativefunc_t func);

/* a static entry, inserted into g_NativeFuncs */
cPy_NativeFunc *cPy_NativeFunc_ctor(cPy_NativeFunc *self, const char *name, nativefunc_t func);

extern byte g_PythonReady;

cPy_Function *cPy_Function_ctor(cPy_Function *self);
void cPy_Function_dtor(cPy_Function *self);
/* 1 if set; also 1 for an empty name (the function is cleared then) and while Python is not running */
byte cPy_Function_SetFunc(cPy_Function *self, const char *name);
int cPy_Function_IsValid(cPy_Function *self);
/* ScrapC: the name of the function as its __name__ gives it (PYF_VALUE and PYF_ENTRY, without the module), "!native"
 * for a C function of the registry, "" without one or when it has no name (a static buffer until the next call) */
const char *cPy_Function_Name(cPy_Function *self);
/* binds the C function func (NULL: only cleared); always 1 */
byte cPy_Function_SetNative(cPy_Function *self, nativefunc_t func);
void cPy_Function_AddParamI(cPy_Function *self, int v);
void cPy_Function_AddParamF(cPy_Function *self, float v);
void cPy_Function_AddParamS(cPy_Function *self, const char *v);
/* calls the function; the int result of a function value or a port entry, else 0. clear drops function and
 * parameters first. A failure of a function value or a port entry is printed (Native_ErrPrint). */
int cPy_Function_Execute(cPy_Function *self, byte clear);
/* clears every function of the ring and frees the parameter files */
void cPy_Function_ClearAll(void);
/* ScrapC: the calls of Execute by kind since the start or the last reset (the test channel's native ports) */
void cPy_Function_Counts(unsigned long *value, unsigned long *native, unsigned long *entry, byte reset);

byte Py_IsReady(void);

#endif
