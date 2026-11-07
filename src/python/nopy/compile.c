/*
 * Python/compile.c without the compiler (SCRAP_NO_PYTHON): the code object stays a type of the object library
 * (PyCode_Type, PyCode_New, PyCode_Addr2Line, Py_OptimizeFlag); PyNode_Compile gets another name, so a caller does
 * not link, and --gc-sections drops the compiler with it.
 */
#define PyNode_Compile Py_NoNodeCompile

#include "Python/compile.c"
