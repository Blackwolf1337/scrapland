/*
 * The Python 1.5.2 interpreter linked into Scrap.exe. Declarations from third_party/python-1.5.2/Include (built
 * with MSVC 32 bit there, same layout here); the functions are thunks into the exe (src/imports.inc), the data
 * the exe owns is mapped with GAME_DATA.
 */
#ifndef SCRAP_PYTHON_H
#define SCRAP_PYTHON_H

#include "scrap/game.h"

#include <Python.h>

static_assert(sizeof(PyObject) == 8, "PyObject: ob_refcnt, ob_type");
static_assert(sizeof(PyTypeObject) == 0x6c, "PyTypeObject of 1.5.2 without COUNT_ALLOCS");

/* type objects of the exe, for PyList_Check, PyString_Check, ... */
#define PyList_Type GAME_DATA(PyTypeObject, 0x432ff0)
#define PyString_Type GAME_DATA(PyTypeObject, 0x432b90)
#define PyInt_Type GAME_DATA(PyTypeObject, 0x433128)
#define PyFloat_Type GAME_DATA(PyTypeObject, 0x433288)

/* the exe has the function form of ceval.c (rva 0x2ee850), the header only the macro */
#undef PyEval_CallObject
PyObject *PyEval_CallObject(PyObject *func, PyObject *args);

/*
 * malloc, realloc and free of the interpreter: the engine counts them in its "Python" memory pool (rva 0x1935f0,
 * 0x193620, 0x1935c0). The PyMem_ and PyObject_NEW/DEL macros of the headers would call the DLL's CRT instead,
 * so the C code uses these and _PyObject_New.
 */
void *PyPool_Malloc(size_t size);
void *PyPool_Realloc(void *p, size_t size);
void PyPool_Free(void *p);

/* init<name> of an engine module: registers the method table of the exe at rva (PYTHON_API_VERSION 1007) */
#define SD_PY_MODULE(name, rva) \
	void init##name(void) { \
		Py_InitModule(#name, &GAME_DATA(PyMethodDef, rva)); \
	}

#undef Py_None
#define Py_None (&GAME_DATA(PyObject, 0x435f3c))	/* _Py_NoneStruct */

#endif
