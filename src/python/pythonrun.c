/*
 * Python/pythonrun.c with what the engine added to it in Scrap.exe: Py_FiniObjects, which uses statics of the file.
 * There is no interpreter: the runners of code (PyRun_*, Py_CompileString, PyParser_Simple*) are not built (NOPY_DROP)
 * and with them the parse, compile and run helpers of the file; Py_Finalize has no grammar to free.
 */
#include "Python.h"
#include "node.h"
#include "pyengine.h"

#define PyRun_AnyFile(args...) nopy_PyRun_AnyFile(args)
#define PyRun_InteractiveLoop(args...) nopy_PyRun_InteractiveLoop(args)
#define PyRun_InteractiveOne(args...) nopy_PyRun_InteractiveOne(args)
#define PyRun_SimpleFile(args...) nopy_PyRun_SimpleFile(args)
#define PyRun_SimpleString(args...) nopy_PyRun_SimpleString(args)
#define PyRun_String(args...) nopy_PyRun_String(args)
#define PyRun_File(args...) nopy_PyRun_File(args)
#define Py_CompileString(args...) nopy_Py_CompileString(args)
#define PyParser_SimpleParseFile(args...) nopy_PyParser_SimpleParseFile(args)
#define PyParser_SimpleParseString(args...) nopy_PyParser_SimpleParseString(args)
NOPY_DROP(int, PyRun_AnyFile);
NOPY_DROP(int, PyRun_InteractiveLoop);
NOPY_DROP(int, PyRun_InteractiveOne);
NOPY_DROP(int, PyRun_SimpleFile);
NOPY_DROP(int, PyRun_SimpleString);
NOPY_DROP(PyObject *, PyRun_String);
NOPY_DROP(PyObject *, PyRun_File);
NOPY_DROP(PyObject *, Py_CompileString);
NOPY_DROP(node *, PyParser_SimpleParseFile);
NOPY_DROP(node *, PyParser_SimpleParseString);
#define PyGrammar_RemoveAccelerators(g) ((void)0)

#include "Python/pythonrun.c"

/*
 * After Py_Finalize, before the engine deletes its Python pool: frees the objects that static pointers of the
 * interpreter keep (without their reference counts, the interpreter is gone). The exe also frees the names in the
 * function statics of classobject.c and the "__path__" of get_parent in import.c; those are not reachable from
 * here and stay in the pool, which is deleted right after (they are pool blocks, small strings).
 */
void Py_FiniObjects(void) {
	_PyDict_FiniDummy();
	_PyString_FiniInterned();
	_PyPath_FiniSearchPath();
	_PyClass_FiniNames();
	_PyList_FiniIndexErr();
}
