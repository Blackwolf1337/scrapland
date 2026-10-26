/*
 * The builtin Python module SDTest for the differential tests (tools/test), on with [general] testmod=1 in
 * scrapdecomp.ini, also when the hooks are off. It calls functions of the exe by RVA, so a test script gets the
 * C version with the hooks on and the original with them off.
 *
 * SDTest.Call(rva, sig, args...): sig is the return type and then one character per argument.
 *   return: v nothing, i int, b byte (al), s char * (None for NULL), f float (st0)
 *   args:   i int, f float, s string, z string or None, b string copied into a buffer of BUF_SIZE bytes (rest 0)
 *           that comes back, p int that is passed by pointer and comes back
 * The result is a tuple: the return value, then the b and p arguments in their order. Only cdecl functions with
 * up to MAX_ARGS arguments of 4 bytes.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/python.h"

#define MAX_ARGS 16
#define MAX_OUT 4
#define BUF_SIZE 1024

#define PyImport_Inittab GAME_DATA(struct _inittab *, 0x4334f0)

typedef uint32_t (*call_int_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef float (*call_float_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

static char bufs[MAX_OUT][BUF_SIZE];
static int ints[MAX_OUT];

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
	int n = (int)strlen(sig) - 1;
	if (n < 0 || n > MAX_ARGS) {
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
		default:
			return NULL;
		}
	}

	void *fn = Game_Ptr((uint32_t)rva);
	PyObject *ret;
	if (sig[0] == 'f') {
		float f = ((call_float_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11],
			a[12], a[13], a[14], a[15]);
		ret = PyFloat_FromDouble(f);
	} else {
		uint32_t r = ((call_int_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11],
			a[12], a[13], a[14], a[15]);
		switch (sig[0]) {
		case 'i':
			ret = PyInt_FromLong((int32_t)r);
			break;
		case 'b':
			ret = PyInt_FromLong(r & 0xff);
			break;
		case 's':
			ret = r ? PyString_FromString((const char *)(uintptr_t)r) : NULL;
			break;
		default:
			ret = NULL;
			break;
		}
	}
	if (!ret) {
		ret = Py_None;
		Py_INCREF(ret);
	}

	PyObject *t = PyTuple_New(1 + nOut);
	PyTuple_SetItem(t, 0, ret);
	for (int i = 0; i < nOut; i++) {
		PyTuple_SetItem(t, 1 + i, outKind[i] == 'b' ? PyString_FromString(bufs[i]) : PyInt_FromLong(ints[i]));
	}
	return t;
}

static PyMethodDef sdtest_methods[] = {
	{ "Call", SDTest_Call, 1, NULL },
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
