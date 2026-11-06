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

/*
 * malloc, realloc and free of the interpreter: the engine counts them in its "Python" memory pool (
 * 0x193620, 0x1935c0). The PyMem_ and PyObject_NEW/DEL macros of the headers would call the DLL's CRT instead,
 * so the C code uses these and _PyObject_New.
 */
void *PyPool_Malloc(size_t size);
void *PyPool_Realloc(void *p, size_t size);
void PyPool_Free(void *p);

/* init<name> of an engine module: registers the method table of the exe at rva (PYTHON_API_VERSION 1007), in
 * ScrapC.exe the C object <name>_methods (in the file of the module) */
#define SD_PY_MODULE(name, rva) \
	void init##name(void) { \
		Py_InitModule(#name, name##_methods); \
	}

/* None with a new reference, the result of a method that returns nothing */
static inline PyObject *Py_ReturnNone(void) {
	Py_INCREF(Py_None);
	return Py_None;
}

/* the engine's flag in its import.c (src/python/import.c) */
extern int g_PyPackImport;
/* told of every .py or .pyc the import of Python reads (not the preloaded modules), 0 refuses it with ImportError set
 * (the gate of PythonInit.c, SCRAP_PY_GATE) */
extern int (*g_PyImportGate)(const char *path);
/* a line in the log of the gate (SCRAP_PY_GATE): the level, what, the module name, a detail; nothing when it is off */
void Py_GateNote(const char *what, const char *name, const char *detail);

/* ScrapC only, SCRAP_PY_PROFILE=<file>: the code objects that run, with counts (src/game/PyInterface/PyProfile.c);
 * Start after Py_Initialize, End after Py_Finalize. Nothing happens without the variable. */
void PyProfile_Start(void);
void PyProfile_End(void);

#endif
