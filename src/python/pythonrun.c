/*
 * Python/pythonrun.c with what the engine added to it in Scrap.exe: PyRun_ScrapString, the PyRun_SimpleString of
 * the console and the startup (rva 0x2ec790), and Py_FiniObjects (rva 0x2ebce0). Both use statics of the file.
 */
#include "Python/pythonrun.c"
#include "pyengine.h"

/* rva 0x4e7b08 */
static char scrap_input[0x800];

/*
 * The command without its CRs and with a newline, parsed as single_input in a static buffer, run in __main__ under
 * the file name "Scrap input". 0 for an empty or too long command (nothing runs), -1 after an error (printed).
 */
int PyRun_ScrapString(const char *command) {
	size_t len = strlen(command);
	size_t n = 0;
	node *nd;
	perrdetail err;
	PyObject *m, *d, *v;

	if (len == 0 || len + 3 > sizeof(scrap_input))
		return 0;
	for (; *command; command++) {
		if (*command != '\r')
			scrap_input[n++] = *command;
	}
	scrap_input[n++] = '\n';
	if (n >= sizeof(scrap_input))
		crt_report_rangecheckfailure();
	/* one 16 bit store in the exe */
	scrap_input[n] = '\0';
	scrap_input[n + 1] = (char)0xff;
	nd = PyParser_ParseString(scrap_input, &_PyParser_Grammar, Py_single_input, &err);
	if (nd == NULL) {
		err_input(&err);
		PyErr_Print();
		return -1;
	}
	m = PyImport_AddModule("__main__");
	if (m == NULL)
		return -1;
	d = PyModule_GetDict(m);
	v = run_node(nd, "Scrap input", d, d);
	if (v == NULL) {
		PyErr_Print();
		return -1;
	}
	Py_DECREF(v);
	if (Py_FlushLine())
		PyErr_Clear();
	return 0;
}

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
