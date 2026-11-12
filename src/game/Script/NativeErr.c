/*
 * The error state of the scripts in C (include/scrap/scriptnative.h, stage 7d of docs/objlib-plan.md): the error a
 * native function raises, its test, fetch and restore, and its print as the script printed its traceback
 * (PyErr_PrintEx of Python/pythonrun.c: there is no traceback without the interpreter, so the lines are the file and
 * line of a SyntaxError and "<type>: <value>").
 *
 * OBJLIB 7: at the end the boundary to the object library of Python 1.5.2, whose error functions
 * (src/python/errors.c) work on this state, and the sys module parts of the print (sys.last_type, sys.stdout's
 * softspace, the missing sys.stderr, SystemExit).
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/nativeobj.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"

static nativeErr_t ne_err = { .kind = NE_NONE };

static const char *const ne_names[NE_KINDS] = {
	[NE_INDEX] = "IndexError",
	[NE_VALUE] = "ValueError",
	[NE_TYPE] = "TypeError",
	[NE_ATTRIBUTE] = "AttributeError",
	[NE_SYSTEM] = "SystemError",
	[NE_OVERFLOW] = "OverflowError",
	[NE_ZERODIV] = "ZeroDivisionError",
	[NE_NAME] = "NameError",
	[NE_KEY] = "KeyError",
	[NE_RUNTIME] = "RuntimeError",
	[NE_IMPORT] = "ImportError",
	[NE_SYNTAX] = "SyntaxError",
	[NE_MEMORY] = "MemoryError",
	[NE_IO] = "IOError",
	[NE_EOF] = "EOFError",
	[NE_OS] = "OSError",
	[NE_FLOAT] = "FloatingPointError",
	[NE_ASSERTION] = "AssertionError",
	[NE_INTERRUPT] = "KeyboardInterrupt",
	[NE_NOTIMPL] = "NotImplementedError",
	[NE_SYSTEMEXIT] = "SystemExit",
	[NE_EXCEPTION] = "Exception",
	[NE_STANDARD] = "StandardError",
	[NE_ARITHMETIC] = "ArithmeticError",
	[NE_LOOKUP] = "LookupError",
	[NE_ENVIRONMENT] = "EnvironmentError",
};

const char *Native_ErrName(nativeError_t kind) {
	return kind >= 0 && kind < NE_OTHER ? ne_names[kind] : NULL;
}

/* the error kind with type (NE_OTHER) and value, both taken over (NULL: None); the old one freed after, as
 * PyErr_Restore frees it (its values may run code that looks at the state) */
static void NErr_Set(nativeError_t kind, nativeVal_t *type, nativeVal_t *value) {
	nativeErr_t old = ne_err;
	ne_err.kind = kind;
	Native_SetNone(&ne_err.type);
	Native_SetNone(&ne_err.value);
	if (type) {
		ne_err.type = *type;
	}
	if (value) {
		ne_err.value = *value;
	}
	Native_ErrFree(&old);
}

byte Native_Raise(nativeError_t kind, const char *fmt, ...) {
	char text[0x200];
	nativeVal_t v;
	va_list va;
	va_start(va, fmt);
	vsnprintf(text, sizeof(text), fmt, va);
	va_end(va);
	Native_SetText(&v, text, (int)strlen(text));
	NErr_Set(kind, NULL, &v);
	return 0;
}

byte Native_RaiseKey(const nativeVal_t *key) {
	return Native_RaiseVal(NE_KEY, key);
}

byte Native_RaiseVal(nativeError_t kind, const nativeVal_t *value) {
	nativeVal_t v;
	Native_SetNone(&v);
	if (value) {
		Native_Copy(&v, value);
	}
	NErr_Set(kind, NULL, &v);
	return 0;
}

static nativeError_t NErr_KindOf(const nativeVal_t *type);

byte Native_RaiseObj(const nativeVal_t *type, const nativeVal_t *value) {
	nativeError_t kind = NErr_KindOf(type);
	nativeVal_t t, v;
	if (kind != NE_OTHER) {
		return Native_RaiseVal(kind, value);
	}
	Native_Copy(&t, type);
	Native_SetNone(&v);
	if (value) {
		Native_Copy(&v, value);
	}
	NErr_Set(NE_OTHER, &t, &v);
	return 0;
}

byte Native_Failed(void) {
	return ne_err.kind != NE_NONE;
}

byte Native_ErrIs(nativeError_t kind) {
	return ne_err.kind == kind;
}

void Native_ErrClear(void) {
	nativeErr_t e;
	Native_ErrFetch(&e);
	Native_ErrFree(&e);
}

void Native_ErrFetch(nativeErr_t *e) {
	*e = ne_err;
	ne_err.kind = NE_NONE;
	Native_SetNone(&ne_err.type);
	Native_SetNone(&ne_err.value);
}

void Native_ErrRestore(nativeErr_t *e) {
	if (e->kind == NE_NONE) {
		Native_ErrFree(e);
		Native_ErrClear();
		return;
	}
	NErr_Set(e->kind, &e->type, &e->value);
	e->kind = NE_NONE;
	Native_SetNone(&e->type);
	Native_SetNone(&e->value);
}

void Native_ErrFree(nativeErr_t *e) {
	e->kind = NE_NONE;
	Native_Free(&e->type);
	Native_Free(&e->value);
}

void Native_ErrType(const nativeErr_t *e, nativeVal_t *out) {
	if (e->kind == NE_OTHER) {
		Native_Copy(out, &e->type);
	} else if (e->kind == NE_NONE) {
		Native_SetNone(out);
	} else {
		Native_SetStr(out, ne_names[e->kind]);
	}
}

void Native_ErrCaught(nativeVal_t *type, nativeVal_t *value) {
	nativeErr_t e;
	Native_ErrFetch(&e);
	Native_ErrNormalize(&e);
	Native_ErrType(&e, type);
	*value = e.value;
	Native_SetNone(&e.value);
	Native_ErrFree(&e);
}

byte Native_ErrTypeStr(nativeBuf_t *b, const nativeErr_t *e) {
	if (e->kind != NE_OTHER) {
		Native_BufAdd(b, e->kind == NE_NONE ? "None" : ne_names[e->kind], -1);
		return 1;
	}
	return NObj_Repr(b, &e->type, NR_STR);
}

byte Native_ErrValueStr(nativeBuf_t *b, const nativeErr_t *e) {
	return NObj_Repr(b, &e->value, NR_STR);
}

/* cls is base or derived from it (PyClass_IsSubclass): both classes */
static byte NErr_IsSub(const nativeVal_t *cls, const nativeVal_t *base, int depth) {
	nativeVal_t bases, b;
	if (NObj_Is(cls, base)) {
		return 1;
	}
	if (depth > 32 || !NObj_GetAttr(cls, "__bases__", &bases)) {
		Native_ErrClear();
		return 0;
	}
	byte sub = 0;
	for (int i = 0; !sub && bases.type == NV_TUPLE && i < bases.n; i++) {
		Native_Copy(&b, &bases.items[i]);
		sub = NErr_IsSub(&b, base, depth + 1);
		Native_Free(&b);
	}
	Native_Free(&bases);
	return sub;
}

void Native_ErrNormalize(nativeErr_t *e) {
	/* PyErr_NormalizeException: only a class as the exception has an instance as its value */
	while (e->kind == NE_OTHER && NObj_Kind(&e->type) == NO_CLASS) {
		nativeVal_t inclass, r;
		Native_SetNone(&inclass);
		if (NObj_Kind(&e->value) == NO_INSTANCE && !NObj_GetAttr(&e->value, "__class__", &inclass)) {
			Native_ErrClear();
		}
		if (inclass.type != NV_NONE && NErr_IsSub(&inclass, &e->type, 0)) {
			/* a value of a derived class names the exception */
			Native_Free(&e->type);
			e->type = inclass;
			return;
		}
		Native_Free(&inclass);
		const nativeVal_t *args = &e->value;
		int n = e->value.type == NV_NONE ? 0 : 1;
		if (e->value.type == NV_TUPLE) {
			args = e->value.items;
			n = e->value.n;
		}
		if (NObj_Call(&e->type, args, n, &r)) {
			Native_Free(&e->value);
			e->value = r;
			return;
		}
		/* the error of the instantiation instead, normalized in turn */
		Native_ErrFree(e);
		Native_ErrFetch(e);
	}
}

/* ---- the print ---- */

/* one write to sys.stderr of the script (the write of ConsoleErrorOutput: Scrap.ConsoleError, through its seam); none
 * while an error is set (PyFile_WriteString) */
static byte NErr_Write(const char *s, int len) {
	nativeVal_t t;
	if (Native_Failed()) {
		return 0;
	}
	Native_SetText(&t, s, len < 0 ? (int)strlen(s) : len);
	byte ok = Mode_ConsoleError(&t);
	Native_Free(&t);
	return ok;
}

/* the text item of a value that is a text or None (PyArg "z"): 1 with *s (NULL for None) */
static byte NErr_Z(const nativeVal_t *v, const char **s) {
	*s = v->type == NV_STR ? Native_Text(v) : NULL;
	return v->type == NV_STR || v->type == NV_NONE;
}

/* the lines of a SyntaxError of the old style (msg, (filename, lineno, offset, text)): its message into *msg (a copy)
 * after the lines; 0 when the value has another form (an instance: its attributes are not read, no case raises one) */
static byte NErr_SyntaxLines(const nativeVal_t *v, nativeVal_t *msg) {
	const nativeVal_t *at;
	const char *file, *text;
	char num[0x10];
	if (v->type != NV_TUPLE || v->n != 2 || (at = &v->items[1])->type != NV_TUPLE || at->n != 4 ||
		!NErr_Z(&at->items[0], &file) || at->items[1].type != NV_INT || at->items[2].type != NV_INT ||
		!NErr_Z(&at->items[3], &text)) {
		return 0;
	}
	int offset = (int)at->items[2].i;
	Native_Copy(msg, &v->items[0]);
	snprintf(num, sizeof(num), "%d", (int)at->items[1].i);
	NErr_Write("  File \"", -1);
	NErr_Write(file ? file : "<string>", -1);
	NErr_Write("\", line ", -1);
	NErr_Write(num, -1);
	NErr_Write("\n", -1);
	if (text) {
		if (offset > 0 && offset == (int)strlen(text)) {
			offset--;
		}
		for (;;) {
			const char *nl = strchr(text, '\n');
			if (!nl || nl - text >= offset) {
				break;
			}
			offset -= (int)(nl + 1 - text);
			text = nl + 1;
		}
		while (*text == ' ' || *text == '\t') {
			text++;
			offset--;
		}
		NErr_Write("    ", -1);
		NErr_Write(text, -1);
		if (!*text || text[strlen(text) - 1] != '\n') {
			NErr_Write("\n", -1);
		}
		NErr_Write("    ", -1);
		/* a write per blank, as pythonrun.c writes them */
		for (offset--; offset > 0; offset--) {
			NErr_Write(" ", -1);
		}
		NErr_Write("^\n", -1);
	}
	return 1;
}

/* the exception as PyErr_Print writes it: a class as "module.name" (no module for the standard ones, "<unknown>"
 * without __module__), else its str */
static byte NErr_WriteType(const nativeErr_t *e) {
	nativeVal_t a;
	nativeBuf_t b;
	byte ok;
	if (e->kind == NE_OTHER && NObj_Kind(&e->type) == NO_CLASS) {
		if (!NObj_GetAttr(&e->type, "__module__", &a)) {
			Native_ErrClear();
			ok = NErr_Write("<unknown>", -1);
		} else {
			const char *mod = a.type == NV_STR ? Native_Text(&a) : NULL;
			ok = !mod || !strcmp(mod, "exceptions") || (NErr_Write(mod, -1) && NErr_Write(".", -1));
			Native_Free(&a);
		}
		if (!ok) {
			return 0;
		}
		if (!NObj_GetAttr(&e->type, "__name__", &a)) {
			Native_ErrClear();
			return NErr_Write("<unknown>", -1);
		}
		Native_BufInit(&b);
		ok = NObj_Repr(&b, &a, NR_STR) && NErr_Write(b.p, b.len);
		Native_BufFree(&b);
		Native_Free(&a);
		return ok;
	}
	Native_BufInit(&b);
	ok = Native_ErrTypeStr(&b, e) && NErr_Write(b.p, b.len);
	Native_BufFree(&b);
	return ok;
}

static byte NErr_PrintStart(nativeErr_t *e);
static void NErr_Exit(nativeErr_t *e);

void Native_ErrPrint(void) {
	nativeErr_t e;
	nativeVal_t msg;
	nativeBuf_t s;
	Native_ErrFetch(&e);
	Native_ErrNormalize(&e);
	if (e.kind == NE_NONE) {
		return;
	}
	if (e.kind == NE_SYSTEMEXIT) {
		NErr_Exit(&e);
	}
	if (!NErr_PrintStart(&e)) {
		fprintf(stderr, "lost sys.stderr\n");
		Native_ErrFree(&e);
		return;
	}
	fflush(stdout);
	byte ok = 1;
	if (e.kind == NE_SYNTAX && NErr_SyntaxLines(&e.value, &msg)) {
		Native_Free(&e.value);
		e.value = msg;
		ok = !Native_Failed();
	}
	ok = ok && NErr_WriteType(&e);
	if (ok && e.value.type != NV_NONE) {
		/* the colon only before a str that is not empty */
		Native_BufInit(&s);
		ok = Native_ErrValueStr(&s, &e) && (!s.len || NErr_Write(": ", -1)) && NErr_Write(s.p, s.len);
		Native_BufFree(&s);
	}
	if (ok) {
		ok = NErr_Write("\n", -1);
	}
	Native_ErrFree(&e);
	if (!ok) {
		Native_ErrClear();
	}
}

/* ---- OBJLIB 7: the boundary to Python's error state and sys ---- */

static PyObject **const ne_py[NE_KINDS] = {
	[NE_INDEX] = &PyExc_IndexError,
	[NE_VALUE] = &PyExc_ValueError,
	[NE_TYPE] = &PyExc_TypeError,
	[NE_ATTRIBUTE] = &PyExc_AttributeError,
	[NE_SYSTEM] = &PyExc_SystemError,
	[NE_OVERFLOW] = &PyExc_OverflowError,
	[NE_ZERODIV] = &PyExc_ZeroDivisionError,
	[NE_NAME] = &PyExc_NameError,
	[NE_KEY] = &PyExc_KeyError,
	[NE_RUNTIME] = &PyExc_RuntimeError,
	[NE_IMPORT] = &PyExc_ImportError,
	[NE_SYNTAX] = &PyExc_SyntaxError,
	[NE_MEMORY] = &PyExc_MemoryError,
	[NE_IO] = &PyExc_IOError,
	[NE_EOF] = &PyExc_EOFError,
	[NE_OS] = &PyExc_OSError,
	[NE_FLOAT] = &PyExc_FloatingPointError,
	[NE_ASSERTION] = &PyExc_AssertionError,
	[NE_INTERRUPT] = &PyExc_KeyboardInterrupt,
	[NE_NOTIMPL] = &PyExc_NotImplementedError,
	[NE_SYSTEMEXIT] = &PyExc_SystemExit,
	[NE_EXCEPTION] = &PyExc_Exception,
	[NE_STANDARD] = &PyExc_StandardError,
	[NE_ARITHMETIC] = &PyExc_ArithmeticError,
	[NE_LOOKUP] = &PyExc_LookupError,
	[NE_ENVIRONMENT] = &PyExc_EnvironmentError,
};

/* the kind whose exception object o is (string exceptions match by identity), NE_OTHER for any other */
static nativeError_t NErr_KindOfPy(PyObject *o) {
	for (int k = 0; o && k < NE_OTHER; k++) {
		if (*ne_py[k] == o) {
			return (nativeError_t)k;
		}
	}
	return NE_OTHER;
}

/* the kind of a value raised as the exception: a standard one only as its own object (an object of another type or a
 * text made anew is no standard exception) */
static nativeError_t NErr_KindOf(const nativeVal_t *type) {
	return NErr_KindOfPy(Native_KeptPy(type));
}

/* the native value of an object of Python's error state; a number, text, tuple or handle keeps the object, so the way
 * back gives the same one */
static void NErr_FromPy(PyObject *o, nativeVal_t *v) {
	NObj_FromPy(o, v);
	if (v->type != NV_OBJ && v->type != NV_REF) {
		Native_KeepPy(v, o);
	}
}

/* the Python object of a value of the error (borrowed: kept with the value) */
static PyObject *NErr_Py(nativeVal_t *v) {
	PyObject *o = Native_KeptPy(v);
	if (!o && v->type == NV_REF) {
		o = Native_PyOf(v);
	}
	if (!o) {
		o = Native_ToPy(v);
		if (!o) {
			return Py_None;
		}
		Native_KeepPy(v, o);
		Py_DECREF(o);
	}
	return o;
}

void Py_ErrSet(PyObject *type, PyObject *value, PyObject *tb) {
	nativeError_t kind = type ? NErr_KindOfPy(type) : NE_NONE;
	nativeVal_t t, v;
	Native_SetNone(&t);
	Native_SetNone(&v);
	if (kind == NE_OTHER) {
		NErr_FromPy(type, &t);
	}
	if (kind != NE_NONE && value) {
		NErr_FromPy(value, &v);
	}
	Py_XDECREF(type);
	Py_XDECREF(value);
	Py_XDECREF(tb);
	NErr_Set(kind, &t, &v);
}

PyObject *Py_ErrType(void) {
	if (ne_err.kind == NE_NONE) {
		return NULL;
	}
	return ne_err.kind == NE_OTHER ? NErr_Py(&ne_err.type) : *ne_py[ne_err.kind];
}

void Py_ErrTake(PyObject **type, PyObject **value, PyObject **tb) {
	*type = Py_ErrType();
	*value = NULL;
	*tb = NULL;
	if (*type) {
		Py_INCREF(*type);
		*value = NErr_Py(&ne_err.value);
		Py_INCREF(*value);
	}
	Native_ErrClear();
}

/* sys.last_type, sys.last_value and (deleted) sys.last_traceback, sys.stdout's soft space ended: 0 when there is no
 * sys.stderr */
static byte NErr_PrintStart(nativeErr_t *e) {
	nativeVal_t t;
	if (e->kind == NE_OTHER) {
		Native_Copy(&t, &e->type);
	} else {
		/* the name of the standard exception as __builtin__ has it (OBJLIB 7e4b: with its object) */
		NObj_FromPyKept(*ne_py[e->kind], &t);
	}
	Script_SetSysVar("last_type", &t);
	Native_Free(&t);
	Script_SetSysVar("last_value", &e->value);
	Script_SetSysVar("last_traceback", NULL);
	if (!Script_SysVar("stderr", &t)) {
		return 0;
	}
	Native_Free(&t);
	if (!Script_FlushLine()) {
		Native_ErrClear();
	}
	return 1;
}

/* SystemExit: the program ends with the code of the value (None 0, an int itself, else 1 after its str) */
static void NErr_Exit(nativeErr_t *e) {
	nativeBuf_t b;
	if (!Script_FlushLine()) {
		Native_ErrClear();
	}
	fflush(stdout);
	if (e->value.type == NV_NONE) {
		exit(0);
	}
	if (e->value.type == NV_INT) {
		exit((int)e->value.i);
	}
	Native_BufInit(&b);
	if (NObj_Repr(&b, &e->value, NR_STR)) {
		fwrite(b.p, 1, (size_t)b.len, stderr);
	}
	fprintf(stderr, "\n");
	exit(1);
}
