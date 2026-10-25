/*
 * Python module SWeap (method table SWeap_methods, rva 0x430128): weapons.
 */
#include "scrap/python.h"

/* GetNWeap() : Obtiene el numero de armas en el juego */
PyObject *SWeap_GetNWeap(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", g_pWorld->numWeapons);
}
