/*
 * PyInterface/Functions.cpp: cPy_Function, a callback by name. "Module.function" binds a Python function,
 * "!name" a C function of the engine registry (cPy_NativeFunc). Parameters go into a Python list or, for C
 * functions, into a memory file. Every cPy_Function is in one ring (g_PyFunctions).
 */
#ifndef SCRAP_PYFUNCTION_H
#define SCRAP_PYFUNCTION_H

#include "scrap/filesys.h"
#include "scrap/python.h"

#define PYF_PYTHON 0x1
#define PYF_NATIVE 0x2
#define PYF_BLOCKED 0x4			/* while Execute runs */

typedef void (*nativefunc_t)(cAbstractFile *params);

typedef struct cPy_Function {
	struct cPy_Function *next;
	struct cPy_Function *prev;
	/* PyObject * (borrowed from the module dictionary) or nativefunc_t */
	void *func;
	/* PyObject * list or cFileMem * */
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

/* a static entry, inserted into g_NativeFuncs */
cPy_NativeFunc *cPy_NativeFunc_ctor(cPy_NativeFunc *self, const char *name, nativefunc_t func);

extern byte g_PythonReady;

cPy_Function *cPy_Function_ctor(cPy_Function *self);
void cPy_Function_dtor(cPy_Function *self);
/* 1 if set; also 1 for an empty name (the function is cleared then) and while Python is not running */
byte cPy_Function_SetFunc(cPy_Function *self, const char *name);
int cPy_Function_IsValid(cPy_Function *self);
/* ScrapC: "module.function" of a Python function, "!native" for a C function, "" without one (a static buffer until
 * the next call) */
const char *cPy_Function_Name(cPy_Function *self);
/* binds the C function func (NULL: only cleared); always 1 */
byte cPy_Function_SetNative(cPy_Function *self, nativefunc_t func);
void cPy_Function_AddParamI(cPy_Function *self, int v);
void cPy_Function_AddParamF(cPy_Function *self, float v);
void cPy_Function_AddParamS(cPy_Function *self, const char *v);
/* calls the function; the int result of a Python function, else 0. clear drops function and parameters first */
int cPy_Function_Execute(cPy_Function *self, byte clear);
/* clears every function of the ring and frees the parameter files */
void cPy_Function_ClearAll(void);

byte Py_IsReady(void);

#endif
