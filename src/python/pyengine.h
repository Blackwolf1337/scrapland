/*
 * What the engine added to the files of its Python 1.5.2 (src/python/*.c around the vendored sources).
 */
#ifndef SCRAP_PYENGINE_H
#define SCRAP_PYENGINE_H

int PyRun_ScrapString(const char *command);
void Py_FiniObjects(void);

/* the parts of Py_FiniObjects in the files whose statics they free */
void _PyDict_FiniDummy(void);
void _PyString_FiniInterned(void);
void _PyPath_FiniSearchPath(void);
void _PyClass_FiniNames(void);
void _PyList_FiniIndexErr(void);

/* frees what a static of the interpreter points to, without its reference count, and clears it */
#define PY_FINI_STATIC(p) \
	do { \
		if (p) { \
			free(p); \
			(p) = NULL; \
		} \
	} while (0)

#endif
