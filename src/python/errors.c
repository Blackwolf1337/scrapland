/*
 * Python/errors.c on the error state of the engine (src/game/Script/NativeErr.c, OBJLIB 7): the exception is not kept
 * in the thread state but in the C error of the scripts, so an error raised in C is the pending exception of Python
 * and the other way round. PyErr_Restore, PyErr_Occurred and PyErr_Fetch are the boundary (Py_ErrSet, Py_ErrType,
 * Py_ErrTake); the other functions are those of 1.5.2 on them.
 */
#include "Python.h"
#include "pyengine.h"

#include <errno.h>
#include <stdarg.h>

void PyErr_Restore(PyObject *type, PyObject *value, PyObject *traceback) {
	Py_ErrSet(type, value, traceback);
}

void PyErr_SetObject(PyObject *exception, PyObject *value) {
	Py_XINCREF(exception);
	Py_XINCREF(value);
	PyErr_Restore(exception, value, (PyObject *)NULL);
}

void PyErr_SetNone(PyObject *exception) {
	PyErr_SetObject(exception, (PyObject *)NULL);
}

void PyErr_SetString(PyObject *exception, const char *string) {
	PyObject *value = PyString_FromString(string);
	PyErr_SetObject(exception, value);
	Py_XDECREF(value);
}

PyObject *PyErr_Occurred(void) {
	return Py_ErrType();
}

int PyErr_GivenExceptionMatches(PyObject *err, PyObject *exc) {
	if (PyTuple_Check(exc)) {
		int i, n = PyTuple_Size(exc);
		for (i = 0; i < n; i++) {
			if (PyErr_GivenExceptionMatches(err, PyTuple_GET_ITEM(exc, i)))
				return 1;
		}
		return 0;
	}
	/* err might be an instance, so check its class */
	if (PyInstance_Check(err))
		err = (PyObject *)((PyInstanceObject *)err)->in_class;
	if (PyClass_Check(err) && PyClass_Check(exc))
		return PyClass_IsSubclass(err, exc);
	return err == exc;
}

int PyErr_ExceptionMatches(PyObject *exc) {
	return PyErr_GivenExceptionMatches(PyErr_Occurred(), exc);
}

/* if the type is a class, the value becomes an instance of it */
void PyErr_NormalizeException(PyObject **exc, PyObject **val, PyObject **tb) {
	PyObject *type = *exc, *value = *val, *inclass = NULL;

	/* PyErr_SetNone leaves the value NULL */
	if (!value) {
		value = Py_None;
		Py_INCREF(value);
	}
	if (PyInstance_Check(value))
		inclass = (PyObject *)((PyInstanceObject *)value)->in_class;
	if (PyClass_Check(type)) {
		if (!inclass || !PyClass_IsSubclass(inclass, type)) {
			PyObject *args, *res;
			if (value == Py_None)
				args = Py_BuildValue("()");
			else if (PyTuple_Check(value)) {
				Py_INCREF(value);
				args = value;
			} else
				args = Py_BuildValue("(O)", value);
			if (args == NULL)
				goto finally;
			res = PyEval_CallObject(type, args);
			Py_DECREF(args);
			if (res == NULL)
				goto finally;
			Py_DECREF(value);
			value = res;
		} else if (inclass != type) {
			/* the class of the instance names the exception */
			Py_DECREF(type);
			type = inclass;
			Py_INCREF(type);
		}
	}
	*exc = type;
	*val = value;
	return;
finally:
	Py_DECREF(type);
	Py_DECREF(value);
	Py_XDECREF(*tb);
	PyErr_Fetch(exc, val, tb);
	PyErr_NormalizeException(exc, val, tb);
}

void PyErr_Fetch(PyObject **p_type, PyObject **p_value, PyObject **p_traceback) {
	Py_ErrTake(p_type, p_value, p_traceback);
}

void PyErr_Clear(void) {
	PyErr_Restore(NULL, NULL, NULL);
}

int PyErr_BadArgument(void) {
	PyErr_SetString(PyExc_TypeError, "illegal argument type for built-in operation");
	return 0;
}

PyObject *PyErr_NoMemory(void) {
	if (PyExc_MemoryErrorInst)
		PyErr_SetObject(PyExc_MemoryError, PyExc_MemoryErrorInst);
	else
		PyErr_SetNone(PyExc_MemoryError);
	return NULL;
}

PyObject *PyErr_SetFromErrnoWithFilename(PyObject *exc, char *filename) {
	PyObject *v;
	int i = errno;
	char *s = i == 0 ? "Error" : strerror(i);
#ifdef EINTR
	if (i == EINTR && PyErr_CheckSignals())
		return NULL;
#endif
	if (filename != NULL && Py_UseClassExceptionsFlag)
		v = Py_BuildValue("(iss)", i, s, filename);
	else
		v = Py_BuildValue("(is)", i, s);
	if (v != NULL) {
		PyErr_SetObject(exc, v);
		Py_DECREF(v);
	}
	return NULL;
}

PyObject *PyErr_SetFromErrno(PyObject *exc) {
	return PyErr_SetFromErrnoWithFilename(exc, NULL);
}

void PyErr_BadInternalCall(void) {
	PyErr_SetString(PyExc_SystemError, "bad argument to internal function");
}

PyObject *PyErr_Format(PyObject *exception, const char *format, ...) {
	va_list vargs;
	char buffer[500];

	va_start(vargs, format);
	vsnprintf(buffer, sizeof(buffer), format, vargs);
	va_end(vargs);
	PyErr_SetString(exception, buffer);
	return NULL;
}

/* string exceptions in the game (Py_UseClassExceptionsFlag without exceptions.py): the name; a class when the base
 * is one */
PyObject *PyErr_NewException(char *name, PyObject *base, PyObject *dict) {
	char *dot = strrchr(name, '.');
	PyObject *modulename = NULL, *classname = NULL, *mydict = NULL, *bases = NULL, *result = NULL;

	if (dot == NULL) {
		PyErr_SetString(PyExc_SystemError, "PyErr_NewException: name must be module.class");
		return NULL;
	}
	if (base == NULL)
		base = PyExc_Exception;
	if (!PyClass_Check(base))
		return PyString_FromString(name);
	if (dict == NULL) {
		dict = mydict = PyDict_New();
		if (dict == NULL)
			goto failure;
	}
	if (PyDict_GetItemString(dict, "__module__") == NULL) {
		modulename = PyString_FromStringAndSize(name, (int)(dot - name));
		if (modulename == NULL)
			goto failure;
		if (PyDict_SetItemString(dict, "__module__", modulename) != 0)
			goto failure;
	}
	classname = PyString_FromString(dot + 1);
	if (classname == NULL)
		goto failure;
	bases = Py_BuildValue("(O)", base);
	if (bases == NULL)
		goto failure;
	result = PyClass_New(bases, dict, classname);
failure:
	Py_XDECREF(bases);
	Py_XDECREF(mydict);
	Py_XDECREF(classname);
	Py_XDECREF(modulename);
	return result;
}
