/*
 * The builtin Python module SDTest for the differential tests (tools/test), on with [general] testmod=1 in
 * scrapdecomp.ini, also when the hooks are off. It calls functions of the exe by RVA, so a test script gets the
 * C version with the hooks on and the original with them off.
 *
 * SDTest.Call(rva, sig, args...): sig is the return type and then one character per argument.
 *   return: v nothing, i int, b byte (al), s char * (None for NULL), f float (st0)
 *   args:   i int, f float, s string, z string or None, b string copied into a buffer of BUF_SIZE bytes (rest 0)
 *           that comes back, p int and D double that are passed by pointer and come back
 * The result is a tuple: the return value, then the b, p and D arguments in their order. cdecl functions with up to
 * MAX_ARGS arguments of 4 bytes; "t" before sig calls a thiscall method, the first argument is this (up to
 * MAX_THIS_ARGS more, no float return).
 *
 * SDTest.Peek(rva, kind) reads a global of the exe: i int, b byte, d double, s the characters at rva, p the string a
 * pointer at rva points to (None for NULL); with '@' before the kind rva is an address (from a returned pointer).
 * SDTest.Poke(rva, value) writes an int.
 * SDTest.OutFunc() is the address of a cdecl void(const char *) that collects text, SDTest.Out() returns and clears
 * that text (for the output callbacks of the engine).
 */
#include <stdio.h>
#include <string.h>

#include "scrap/python.h"

#define MAX_ARGS 16
#define MAX_THIS_ARGS 6
#define MAX_OUT 4
#define BUF_SIZE 1024

#define PyImport_Inittab GAME_DATA(struct _inittab *, 0x4334f0)

typedef uint32_t (*call_int_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef float (*call_float_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

typedef uint32_t(__thiscall *this0_t)(uint32_t);
typedef uint32_t(__thiscall *this1_t)(uint32_t, uint32_t);
typedef uint32_t(__thiscall *this2_t)(uint32_t, uint32_t, uint32_t);
typedef uint32_t(__thiscall *this3_t)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t(__thiscall *this4_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t(__thiscall *this5_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t(__thiscall *this6_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* the callee pops its arguments, so the call needs the exact count */
static uint32_t Call_This(void *fn, const uint32_t *a, int n) {
	switch (n) {
	case 1:
		return ((this0_t)fn)(a[0]);
	case 2:
		return ((this1_t)fn)(a[0], a[1]);
	case 3:
		return ((this2_t)fn)(a[0], a[1], a[2]);
	case 4:
		return ((this3_t)fn)(a[0], a[1], a[2], a[3]);
	case 5:
		return ((this4_t)fn)(a[0], a[1], a[2], a[3], a[4]);
	case 6:
		return ((this5_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5]);
	default:
		return ((this6_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
	}
}

/* the return value as the first character of sig says, NULL for None */
static PyObject *SDTest_Result(char kind, uint32_t r) {
	switch (kind) {
	case 'i':
		return PyInt_FromLong((int32_t)r);
	case 'b':
		return PyInt_FromLong(r & 0xff);
	case 's':
		return r ? PyString_FromString((const char *)(uintptr_t)r) : NULL;
	default:
		return NULL;
	}
}

static char bufs[MAX_OUT][BUF_SIZE];
static int ints[MAX_OUT];
static double doubles[MAX_OUT];
static char out[0x4000];
static size_t outLen;

static PyObject *SDTest_Call(PyObject *self, PyObject *args) {
	(void)self;
	int rva;
	const char *sig;
	uint32_t a[MAX_ARGS] = { 0 };
	char outKind[MAX_OUT];
	int nOut = 0;

	if (!PyArg_Parse(PyTuple_GetItem(args, 0), "i", &rva) || !PyArg_Parse(PyTuple_GetItem(args, 1), "s", &sig)) {
		return NULL;
	}
	int thiscall = sig[0] == 't';
	sig += thiscall;
	int n = (int)strlen(sig) - 1;
	if (n < 0 || n > MAX_ARGS || (thiscall && (n < 1 || n > MAX_THIS_ARGS + 1 || sig[0] == 'f'))) {
		return NULL;
	}
	for (int i = 0; i < n; i++) {
		PyObject *o = PyTuple_GetItem(args, 2 + i);
		const char *s;
		int v;
		float f;

		if (!o) {
			return NULL;
		}
		switch (sig[1 + i]) {
		case 'i':
			if (!PyArg_Parse(o, "i", &v)) {
				return NULL;
			}
			a[i] = (uint32_t)v;
			break;
		case 'f':
			if (!PyArg_Parse(o, "f", &f)) {
				return NULL;
			}
			memcpy(&a[i], &f, sizeof(f));
			break;
		case 's':
		case 'z':
			if (!PyArg_Parse(o, sig[1 + i] == 's' ? "s" : "z", &s)) {
				return NULL;
			}
			a[i] = (uint32_t)(uintptr_t)s;
			break;
		case 'b':
			if (nOut == MAX_OUT || !PyArg_Parse(o, "s", &s)) {
				return NULL;
			}
			memset(bufs[nOut], 0, BUF_SIZE);
			snprintf(bufs[nOut], BUF_SIZE, "%s", s);
			a[i] = (uint32_t)(uintptr_t)bufs[nOut];
			outKind[nOut++] = 'b';
			break;
		case 'p':
			if (nOut == MAX_OUT || !PyArg_Parse(o, "i", &ints[nOut])) {
				return NULL;
			}
			a[i] = (uint32_t)(uintptr_t)&ints[nOut];
			outKind[nOut++] = 'p';
			break;
		case 'D':
			if (nOut == MAX_OUT || !PyArg_Parse(o, "d", &doubles[nOut])) {
				return NULL;
			}
			a[i] = (uint32_t)(uintptr_t)&doubles[nOut];
			outKind[nOut++] = 'D';
			break;
		default:
			return NULL;
		}
	}

	void *fn = Game_Ptr((uint32_t)rva);
	PyObject *ret;
	if (thiscall) {
		ret = SDTest_Result(sig[0], Call_This(fn, a, n));
	} else if (sig[0] == 'f') {
		float f = ((call_float_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11],
			a[12], a[13], a[14], a[15]);
		ret = PyFloat_FromDouble(f);
	} else {
		ret = SDTest_Result(sig[0], ((call_int_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10],
			a[11], a[12], a[13], a[14], a[15]));
	}
	if (!ret) {
		ret = Py_None;
		Py_INCREF(ret);
	}

	PyObject *t = PyTuple_New(1 + nOut);
	PyTuple_SetItem(t, 0, ret);
	for (int i = 0; i < nOut; i++) {
		PyObject *o;
		switch (outKind[i]) {
		case 'b':
			o = PyString_FromString(bufs[i]);
			break;
		case 'D':
			o = PyFloat_FromDouble(doubles[i]);
			break;
		default:
			o = PyInt_FromLong(ints[i]);
			break;
		}
		PyTuple_SetItem(t, 1 + i, o);
	}
	return t;
}

static PyObject *SDTest_Peek(PyObject *self, PyObject *args) {
	(void)self;
	int rva;
	const char *kind;

	if (!PyArg_ParseTuple(args, "is", &rva, &kind)) {
		return NULL;
	}
	const void *p = Game_Ptr((uint32_t)rva);
	if (kind[0] == '@') {
		p = (const void *)(uintptr_t)(uint32_t)rva;
		kind++;
	}
	switch (kind[0]) {
	case 'i':
		return PyInt_FromLong(*(const int32_t *)p);
	case 'b':
		return PyInt_FromLong(*(const byte *)p);
	case 'd':
		return PyFloat_FromDouble(*(const double *)p);
	case 's':
		return PyString_FromString(p);
	case 'p':
		if (*(const char *const *)p) {
			return PyString_FromString(*(const char *const *)p);
		}
		break;
	default:
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *SDTest_Poke(PyObject *self, PyObject *args) {
	(void)self;
	int rva, v;

	if (!PyArg_ParseTuple(args, "ii", &rva, &v)) {
		return NULL;
	}
	*(int32_t *)Game_Ptr((uint32_t)rva) = v;
	Py_INCREF(Py_None);
	return Py_None;
}

static void SDTest_Collect(const char *s) {
	size_t n = strlen(s);
	if (n > sizeof(out) - 1 - outLen) {
		n = sizeof(out) - 1 - outLen;
	}
	memcpy(out + outLen, s, n);
	outLen += n;
	out[outLen] = '\0';
}

static PyObject *SDTest_OutFunc(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return PyInt_FromLong((long)(uintptr_t)SDTest_Collect);
}

static PyObject *SDTest_Out(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	PyObject *r = PyString_FromString(out);
	outLen = 0;
	out[0] = '\0';
	return r;
}

static PyMethodDef sdtest_methods[] = {
	{ "Call", SDTest_Call, 1, NULL },
	{ "Peek", SDTest_Peek, 1, NULL },
	{ "Poke", SDTest_Poke, 1, NULL },
	{ "OutFunc", SDTest_OutFunc, 1, NULL },
	{ "Out", SDTest_Out, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

static void initSDTest(void) {
	Py_InitModule("SDTest", sdtest_methods);
}

/* the exe's builtin modules and SDTest; Py_Initialize reads the table through PyImport_Inittab */
static struct _inittab inittab[64];

void TestMod_Register(void) {
	int n = 0;

	for (const struct _inittab *p = PyImport_Inittab; p->name && n < (int)(sizeof(inittab) / sizeof(inittab[0])) - 2;
		p++) {
		inittab[n++] = *p;
	}
	inittab[n].name = "SDTest";
	inittab[n].initfunc = initSDTest;
	PyImport_Inittab = inittab;
}
