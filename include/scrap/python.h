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

#undef Py_None
#define Py_None (&GAME_DATA(PyObject, 0x435f3c))	/* _Py_NoneStruct */

#endif
