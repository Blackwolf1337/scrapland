/*
 * Functions of Scrap.exe the C code calls: SD_IMPORT(name, rva) for __cdecl, SD_IMPORT_STDCALL(name, argbytes, rva)
 * for __stdcall. src/core/imports.c turns each into a thunk with the name of the original, so the prototype comes
 * from the normal header. A function that gets implemented in C moves to src/hooks.inc: the linker reports a
 * function that is in both lists.
 */

/* Python 1.5.2 */
SD_IMPORT(PyArg_ParseTuple, 0x2f0560)	/* getargs.c */
SD_IMPORT(Py_BuildValue, 0x2f1b90)	/* modsupport.c */
