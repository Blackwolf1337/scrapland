/*
 * The Python 1.5.2 interpreter linked into Scrap.exe. Declarations from third_party/python-1.5.2/Include (built
 * with MSVC 32 bit there, same layout here). In the hook DLL the functions are thunks into the exe (SD_IMPORT_PY in
 * src/imports.inc) and the data the exe owns is mapped with GAME_DATA; ScrapC.exe (SD_STANDALONE) links the real
 * library of src/python and uses its symbols.
 */
#ifndef SCRAP_PYTHON_H
#define SCRAP_PYTHON_H

#include "scrap/game.h"

#include <Python.h>
#include <compile.h>

/* the exe has the function form of ceval.c, the header only the macro */
#undef PyEval_CallObject
PyObject *PyEval_CallObject(PyObject *func, PyObject *args);

/* malloc, realloc and free of the Python library are those of the script pool (scrap/mempool.h). The PyMem_ and
 * PyObject_NEW/DEL macros of the headers would call the DLL's CRT instead, so the C code uses these and _PyObject_New */
#include "scrap/mempool.h"

/* None with a new reference, the result of a method that returns nothing */
static inline PyObject *Py_ReturnNone(void) {
	Py_INCREF(Py_None);
	return Py_None;
}

/* OBJLIB 7e4b: the built-in module name of the library as its init function makes it, without an interpreter (a new
 * reference, NULL with the error: ImportError for a name the table does not have), and sys.path as Py_Initialize made
 * it (a new list, src/python/import.c) */
PyObject *Py_LibModule(const char *name);
PyObject *Py_LibPath(void);


#endif
