/*
 * Python module Scrap (method table Scrap_methods, rva 0x42c610): engine functions for the game scripts.
 * Original file: PyInterface\\ScrapPy.cpp
 */
#include "scrap/python.h"

/* levelpath GetLevelPath() : Obtiene el path del nivel actual */
PyObject *Scrap_GetLevelPath(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("s", g_pApp->levelPath);
}

/* money Scrap.GetMoney() : Devuelve el liquido disponible. */
PyObject *Scrap_GetMoney(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", g_pWorld->money);
}
