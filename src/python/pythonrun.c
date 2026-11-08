/*
 * Python/pythonrun.c with what the engine added to it in Scrap.exe: PyRun_ScrapString, the PyRun_SimpleString of
 * the console and the startup, and Py_FiniObjects. Both use statics of the file.
 */
/*
 * Without the interpreter (src/python/nopy) the runners of code get other names, so a caller does not link, and
 * --gc-sections drops them; what they would parse is the SyntaxError of an empty parse, a compile or a run of code
 * raises; Py_Finalize has no grammar to free.
 */
#define PyRun_AnyFile Py_NoRunAnyFile
#define PyRun_InteractiveLoop Py_NoRunInteractiveLoop
#define PyRun_InteractiveOne Py_NoRunInteractiveOne
#define PyRun_SimpleFile Py_NoRunSimpleFile
#define PyRun_SimpleString Py_NoRunSimpleString
#define PyRun_String Py_NoRunString
#define PyRun_File Py_NoRunFile
#define Py_CompileString Py_NoCompileString
#define PyParser_SimpleParseFile Py_NoParseFile
#define PyParser_SimpleParseString Py_NoParseString
#include "Python.h"
#include "grammar.h"
#include "node.h"
#include "parsetok.h"
#include "errcode.h"
#include "pyengine.h"

static node *Py_NoParse(perrdetail *err) {
	err->error = E_SYNTAX;
	err->filename = NULL;
	err->lineno = 0;
	err->offset = 0;
	err->text = NULL;
	return NULL;
}

#define PyParser_ParseString(s, g, start, err) ((void)(s), (void)(start), Py_NoParse(err))
#define PyParser_ParseFile(fp, file, g, start, ps1, ps2, err) ((void)(fp), (void)(file), (void)(start), (void)(ps1), \
	(void)(ps2), Py_NoParse(err))
#define PyNode_Compile(n, file) ((void)(n), (PyCodeObject *)Py_NoInterpreter(PyExc_SystemError, file))
#define PyNode_Free(n) ((void)(n))
#define PyEval_EvalCode(co, g, l) ((void)(co), (void)(g), (void)(l), \
	(PyObject *)Py_NoInterpreter(PyExc_SystemError, "code"))
#define PyGrammar_RemoveAccelerators(g) ((void)0)

#include "Python/pythonrun.c"
#include "pyengine.h"


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
