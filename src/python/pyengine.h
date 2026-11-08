/*
 * What the engine added to the files of its Python 1.5.2 (src/python/*.c around the vendored sources).
 */
#ifndef SCRAP_PYENGINE_H
#define SCRAP_PYENGINE_H

void Py_FiniObjects(void);

/* sets exc "<what>: no Python interpreter in this build" unless an error is set already; NULL (src/python/nopy) */
void *Py_NoInterpreter(PyObject *exc, const char *what);
/* the same as an expression of type t that the compiler knows is NULL, so the code behind a check of it drops */
#define Py_NOINTERP(t, exc, what) (Py_NoInterpreter(exc, what), (t)NULL)

/*
 * A function of a vendored file that is not built: "#define name(args...) nopy_##name(args)" and NOPY_DROP(type,
 * name) before the file is included. Its definition (K&R) and its calls in the file are then of the static
 * nopy_<name> (a K&R definition after a static declaration is static too), which nothing calls: the compiler drops it
 * with the statics only it reaches. A caller in another file does not link (the prototype of the header does not
 * expand the macro, the name stays undefined).
 */
#define NOPY_DROP(type, name) static type nopy_##name() __attribute__((unused))

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
