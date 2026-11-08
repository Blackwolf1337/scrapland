/*
 * Python/compile.c without the compiler: the code object stays a type of the object library (PyCode_Type,
 * PyCode_New, PyCode_Addr2Line, Py_OptimizeFlag); PyNode_Compile is not built (NOPY_DROP), so the compiler drops with
 * it.
 */
#include "Python.h"
#include "compile.h"
#include "node.h"
#include "pyengine.h"

#define PyNode_Compile(args...) nopy_PyNode_Compile(args)
NOPY_DROP(PyCodeObject *, PyNode_Compile);

#include "Python/compile.c"
