/*
 * Functions of Scrap.exe the C code calls: SD_IMPORT(name, rva) for __cdecl, SD_IMPORT_STDCALL(name, argbytes, rva)
 * for __stdcall. src/core/imports.c turns each into a thunk with the name of the original, so the prototype comes
 * from the normal header. A function that gets implemented in C moves to src/hooks.inc: the linker reports a
 * function that is in both lists.
 */

/* engine: Global/ */
SD_IMPORT(CheckFailed, 0x0bcdb0)	/* CHECK macro, include/scrap/global.h */

/* engine: Math/ */
SD_IMPORT(Vec3_Rotate, 0x15e3b0)
SD_IMPORT(Vec3_GetRotAng, 0x15df10)
SD_IMPORT(NormAng, 0x15e2e0)

/* Python 1.5.2 */
SD_IMPORT(PyArg_ParseTuple, 0x2f0560)	/* getargs.c */
SD_IMPORT(Py_BuildValue, 0x2f1b90)	/* modsupport.c */
SD_IMPORT(PyErr_Clear, 0x2efd30)	/* errors.c */
SD_IMPORT(PyTuple_New, 0x2decb0)	/* tupleobject.c */
SD_IMPORT(PyTuple_SetItem, 0x2ded50)	/* tupleobject.c */
SD_IMPORT(PyFloat_FromDouble, 0x2dfd70)	/* floatobject.c */
