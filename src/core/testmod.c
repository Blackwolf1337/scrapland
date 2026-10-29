/*
 * The builtin Python module SDTest for the differential tests (tools/test), on with [general] testmod=1 in
 * scrapdecomp.ini, also when the hooks are off. It calls functions of the exe by RVA, so a test script gets the
 * C version with the hooks on and the original with them off.
 *
 * The functions and data of the exe go by name (stage 3c0, docs/standalone.md): SDTest.Sym(name) is the rva of a name of
 * src/core/testsyms.inc (tools/test/testsyms.py writes it from the test scripts), and Call, Peek, Poke and ConvU take
 * such a name in place of the rva. In ScrapC.exe the rva of a name is the address of its C function or object minus
 * the load address (stage 3c2). SDTest.Base() is the load address of the exe (Scrap.exe or ScrapC.exe), so
 * SDTest.Base() + rva is the address the pointers of the game hold.
 * SDTest.Orig(v) is v with an address (or rva) of a C object or function of ScrapC.exe turned into the original's,
 * for the outputs (tools/test/py/sdlib.py prints every int through it); in the hook DLL v.
 * SDTest.PeekSpan(name, off, n) and PokeSpan(name, off, value, n): n bytes after a name in the original's layout
 * (several neighbouring globals at once, separate C objects in ScrapC.exe).
 *
 * SDTest.Call(rva, sig, args...): sig is the return type and then one character per argument.
 *   return: v nothing, i int, b byte (al), s char * (None for NULL), f float (st0)
 *   args:   i int, f float, s string, z string or None, b string copied into a buffer of BUF_SIZE bytes (rest 0)
 *           that comes back, p int and D double that are passed by pointer and come back, F a tuple of numbers
 *           passed as an array of floats (up to MAX_FLOATS) that comes back as a tuple
 * The result is a tuple: the return value, then the b, p, D and F arguments in their order. cdecl functions with up to
 * MAX_ARGS arguments of 4 bytes; "t" before sig calls a thiscall method, the first argument is this (up to
 * MAX_THIS_ARGS more).
 *
 * SDTest.Peek(rva, kind) reads a global of the exe: i int, b byte, d double, s the characters at rva, p the string a
 * pointer at rva points to (None for NULL); with '@' before the kind rva is an address (from a returned pointer).
 * SDTest.Poke(rva, value) writes an int.
 * SDTest.Proc(dll, name) is the function name of the loaded DLL dll as an rva for Call (its address minus the load
 * address of the exe), None when there is none: the Win32 functions for a test, in both runs the same.
 * SDTest.ConvU(rva, lo, hi, isdouble) calls a function that takes a float (isdouble 0, bits in lo) or double (bits in
 * lo and hi) in xmm0 and returns an unsigned int in eax, for the __ftoui3 and __dtoui3 conversions.
 * SDTest.OutFunc() is the address of a cdecl void(const char *) that collects text, SDTest.Out() returns and clears
 * that text (for the output callbacks of the engine).
 * SDTest.D3DLog(on) puts a proxy in place of the D3D device that writes its calls into that text (src/core/d3dlog.c),
 * SDTest.D3DLog(0) the device back.
 * SDTest.HeapOk() is -1 when every heap of the process passes HeapValidate, else the index of the first bad one (to
 * find the test call that breaks a heap).
 */
/* the rvas of the tests: Game_Ptr adds the load address (in ScrapC.exe to the C address minus it, TestMod_SymRva) */
#define SD_GAME_RVA

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#include "scrap/crt.h"
#include "scrap/python.h"

#define MAX_ARGS 16
#define MAX_THIS_ARGS 11
#define MAX_OUT 8
#define BUF_SIZE 1024
#define MAX_FLOATS 64

typedef uint32_t (*call_int_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef float (*call_float_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

#define THIS_TYPES(name, ret) \
	typedef ret(__thiscall *name##0_t)(uint32_t); \
	typedef ret(__thiscall *name##1_t)(uint32_t, uint32_t); \
	typedef ret(__thiscall *name##2_t)(uint32_t, uint32_t, uint32_t); \
	typedef ret(__thiscall *name##3_t)(uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(__thiscall *name##4_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(__thiscall *name##5_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(__thiscall *name##6_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(__thiscall *name##7_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t); \
	typedef ret(__thiscall *name##8_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t, uint32_t); \
	typedef ret(__thiscall *name##9_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t, uint32_t, uint32_t); \
	typedef ret(__thiscall *name##10_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(__thiscall *name##11_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
THIS_TYPES(this, uint32_t)
THIS_TYPES(thisf, float)

/* the callee pops its arguments, so the call needs the exact count */
#define CALL_THIS(name, fn, a, n) \
	switch (n) { \
	case 1: \
		return ((name##0_t)fn)(a[0]); \
	case 2: \
		return ((name##1_t)fn)(a[0], a[1]); \
	case 3: \
		return ((name##2_t)fn)(a[0], a[1], a[2]); \
	case 4: \
		return ((name##3_t)fn)(a[0], a[1], a[2], a[3]); \
	case 5: \
		return ((name##4_t)fn)(a[0], a[1], a[2], a[3], a[4]); \
	case 6: \
		return ((name##5_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5]); \
	case 7: \
		return ((name##6_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6]); \
	case 8: \
		return ((name##7_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]); \
	case 9: \
		return ((name##8_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]); \
	case 10: \
		return ((name##9_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]); \
	case 11: \
		return ((name##10_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10]); \
	default: \
		return ((name##11_t)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]); \
	}

static uint32_t Call_This(void *fn, const uint32_t *a, int n) {
	CALL_THIS(this, fn, a, n)
}

static float Call_ThisFloat(void *fn, const uint32_t *a, int n) {
	CALL_THIS(thisf, fn, a, n)
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

/*
 * The table of the names, sorted by name (strcmp order). In the hook DLL an entry holds the rva of the original, so the
 * run with hooks=0 reaches the original and the hooked run the C through the hook at that rva. In ScrapC.exe it holds
 * the address of the C function or object (plus the offset of a view inside the object), taken from its assembler
 * label like GAME_FUNC of game.h does, so no prototype is needed (stage 3c2).
 */
#ifdef SD_STANDALONE
typedef struct {
	const char *name;
	const char *addr;
	uint32_t rva;		/* of the original */
} testsym_t;

#define SD_TEST_FUNC(key, name, rva) extern char sdt_##name[] __asm__("_" #name);
#define SD_TEST_CRT(key, name, rva) extern char sdt_##name[] __asm__("_" #name);
#define SD_TEST_OBJ(key, name, rva) extern char sdt_##name[] __asm__("_" #name);
#define SD_TEST_DATA(key, owner, off, rva) extern char sdt_##owner[] __asm__("_" #owner);
#include "testsyms.inc"
#undef SD_TEST_FUNC
#undef SD_TEST_CRT
#undef SD_TEST_OBJ
#undef SD_TEST_DATA

static const testsym_t testSyms[] = {
#define SD_TEST_FUNC(key, name, rva) { key, sdt_##name, rva },
#define SD_TEST_CRT(key, name, rva) { key, sdt_##name, rva },
#define SD_TEST_OBJ(key, name, rva) { key, sdt_##name, rva },
#define SD_TEST_DATA(key, owner, off, rva) { key, sdt_##owner + (off), rva },
#include "testsyms.inc"
#undef SD_TEST_FUNC
#undef SD_TEST_CRT
#undef SD_TEST_OBJ
#undef SD_TEST_DATA
};

/* the address of the C minus the load address of ScrapC.exe, which Game_Ptr adds back */
static uint32_t TestMod_SymRva(const testsym_t *s) {
	return (uint32_t)((uintptr_t)s->addr - game_base);
}

/*
 * Every C object with the rva and size of the original (src/core/testaddrs.inc, sorted by rva) and every C function
 * of src/hooks.inc with the rva of the first original it backs: the way from an address of ScrapC.exe to the
 * original's (SDTest.Orig) and from an rva of the original to the C object there (SDTest.PeekSpan, PokeSpan).
 */
typedef struct {
	const char *addr;
	uint32_t size;		/* 0 for a function */
	uint32_t rva;
} testaddr_t;

#define SD_TEST_ADDR(name, rva, size) extern char sda_##name[] __asm__("_" #name);
#include "testaddrs.inc"
#undef SD_TEST_ADDR
#define SD_HOOK(name, rva, size) extern char sda_##name[] __asm__("_" #name);
#include "hooks.inc"
#undef SD_HOOK

static const testaddr_t testObjs[] = {
#define SD_TEST_ADDR(name, rva, size) { sda_##name, size, rva },
#include "testaddrs.inc"
#undef SD_TEST_ADDR
};

static const testaddr_t testFuncs[] = {
#define SD_HOOK(name, rva, size) { sda_##name, 0, rva },
#include "hooks.inc"
#undef SD_HOOK
};

#define TEST_OBJS ((int)(sizeof(testObjs) / sizeof(testObjs[0])))
#define TEST_FUNCS ((int)(sizeof(testFuncs) / sizeof(testFuncs[0])))
/* the start of the original's data (.bss_ste), below it the code */
#define TEST_DATA_RVA 0x390000

/* both tables by the address in ScrapC.exe, made at the first use */
static const testaddr_t *byAddr[TEST_OBJS + TEST_FUNCS];

static int TestMod_AddrCompare(const void *a, const void *b) {
	const testaddr_t *x = *(const testaddr_t *const *)a, *y = *(const testaddr_t *const *)b;

	if (x->addr != y->addr) {
		return (uintptr_t)x->addr < (uintptr_t)y->addr ? -1 : 1;
	}
	return x < y ? -1 : x > y;	/* the first copy of a function */
}

/* the rva of the original for the address a of ScrapC.exe, 0 for none. Only the start of an object or function: the
 * colors and packed numbers the tests print would hit the inside of the objects too often */
static uint32_t TestMod_OrigRva(uintptr_t a) {
	static int sorted;
	int lo = 0, hi = TEST_OBJS + TEST_FUNCS - 1, found = -1;

	if (!sorted) {
		for (int i = 0; i < TEST_OBJS; i++) {
			byAddr[i] = &testObjs[i];
		}
		for (int i = 0; i < TEST_FUNCS; i++) {
			byAddr[TEST_OBJS + i] = &testFuncs[i];
		}
		qsort(byAddr, TEST_OBJS + TEST_FUNCS, sizeof(byAddr[0]), TestMod_AddrCompare);
		sorted = 1;
	}
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if ((uintptr_t)byAddr[mid]->addr < a) {
			lo = mid + 1;
		} else {
			if ((uintptr_t)byAddr[mid]->addr == a) {
				found = mid;
			}
			hi = mid - 1;
		}
	}
	return found >= 0 ? byAddr[found]->rva : 0;
}

/*
 * SDTest.Orig(v): an address of ScrapC.exe as the original has it, for the outputs of the tests: v as an absolute
 * address (0x400000 + the rva of the original) or as an rva (v plus the load address), else v itself. Neither form is
 * the same in ScrapC.exe since stage 3c2, so a test that prints a pointer to a vtable, a string, a table or a function
 * of the game prints this. In the hook DLL v itself.
 */
static uint32_t TestMod_Orig(uint32_t v) {
	uint32_t r = TestMod_OrigRva(v);
	if (r) {
		return 0x400000 + r;
	}
	/* the rva form for data only: small numbers hit the starts of the functions (aligned to 16) too often */
	r = TestMod_OrigRva((uintptr_t)v + game_base);
	return r && r >= TEST_DATA_RVA ? r : v;
}

/* the byte of ScrapC.exe at the rva of the original's data, NULL where no C object is (data nothing uses) */
static byte *TestMod_OrigByte(uint32_t rva) {
	int lo = 0, hi = TEST_OBJS - 1;

	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		const testaddr_t *o = &testObjs[mid];
		if (rva < o->rva) {
			hi = mid - 1;
		} else if (rva >= o->rva + o->size) {
			lo = mid + 1;
		} else {
			return (byte *)o->addr + (rva - o->rva);
		}
	}
	return NULL;
}
#else
typedef struct {
	const char *name;
	uint32_t rva;
} testsym_t;

static const testsym_t testSyms[] = {
#define SD_TEST_FUNC(key, name, rva) { key, rva },
#define SD_TEST_CRT(key, name, rva) { key, rva },
#define SD_TEST_OBJ(key, name, rva) { key, rva },
#define SD_TEST_DATA(key, owner, off, rva) { key, rva },
#include "testsyms.inc"
#undef SD_TEST_FUNC
#undef SD_TEST_CRT
#undef SD_TEST_OBJ
#undef SD_TEST_DATA
};

/* the rva of the original */
static uint32_t TestMod_SymRva(const testsym_t *s) {
	return s->rva;
}

/* the addresses are the original's already */
static uint32_t TestMod_Orig(uint32_t v) {
	return v;
}

static byte *TestMod_OrigByte(uint32_t rva) {
	return (byte *)Game_Ptr(rva);
}
#endif

static PyObject *SDTest_Orig(PyObject *self, PyObject *args) {
	(void)self;
	int v;

	if (!PyArg_ParseTuple(args, "i", &v)) {
		return NULL;
	}
	return PyInt_FromLong((int32_t)TestMod_Orig((uint32_t)v));
}

static const testsym_t *TestMod_FindSym(const char *name) {
	int lo = 0, hi = (int)(sizeof(testSyms) / sizeof(testSyms[0])) - 1;

	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		int c = strcmp(name, testSyms[mid].name);
		if (!c) {
			return &testSyms[mid];
		}
		if (c < 0) {
			hi = mid - 1;
		} else {
			lo = mid + 1;
		}
	}
	return NULL;
}

/* an rva argument: an int, or a name of testsyms.inc (a TypeError for another name) */
static int TestMod_ParseRva(PyObject *o, int *rva) {
	if (o && PyString_Check(o)) {
		const testsym_t *s = TestMod_FindSym(PyString_AsString(o));
		if (s) {
			*rva = (int)TestMod_SymRva(s);
			return 1;
		}
		return PyArg_Parse(o, "i;SDTest: a name that src/core/testsyms.inc does not have", rva);
	}
	return o && PyArg_Parse(o, "i", rva);
}

static char bufs[MAX_OUT][BUF_SIZE];
static int ints[MAX_OUT];
static double doubles[MAX_OUT];
static float floats[MAX_OUT][MAX_FLOATS];
static int nFloats[MAX_OUT];
static char out[0x40000];
static size_t outLen;

static PyObject *SDTest_Call(PyObject *self, PyObject *args) {
	(void)self;
	int rva;
	const char *sig;
	uint32_t a[MAX_ARGS] = { 0 };
	char outKind[MAX_OUT];
	int nOut = 0;

	if (!TestMod_ParseRva(PyTuple_GetItem(args, 0), &rva) || !PyArg_Parse(PyTuple_GetItem(args, 1), "s", &sig)) {
		return NULL;
	}
	int thiscall = sig[0] == 't';
	sig += thiscall;
	int n = (int)strlen(sig) - 1;
	if (n < 0 || n > MAX_ARGS || (thiscall && (n < 1 || n > MAX_THIS_ARGS + 1))) {
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
		case 'F': {
			int len = (int)PyTuple_GET_SIZE(o);
			if (nOut == MAX_OUT || len > MAX_FLOATS) {
				return NULL;
			}
			memset(floats[nOut], 0, sizeof(floats[nOut]));
			for (int k = 0; k < len; k++) {
				if (!PyArg_Parse(PyTuple_GetItem(o, k), "f", &floats[nOut][k])) {
					return NULL;
				}
			}
			nFloats[nOut] = len;
			a[i] = (uint32_t)(uintptr_t)floats[nOut];
			outKind[nOut++] = 'F';
			break;
		}
		default:
			return NULL;
		}
	}

	void *fn = Game_Ptr((uint32_t)rva);
	PyObject *ret;
	if (thiscall && sig[0] == 'f') {
		ret = PyFloat_FromDouble(Call_ThisFloat(fn, a, n));
	} else if (thiscall) {
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
		case 'F':
			o = PyTuple_New(nFloats[i]);
			for (int k = 0; k < nFloats[i]; k++) {
				PyTuple_SetItem(o, k, PyFloat_FromDouble(floats[i][k]));
			}
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
	PyObject *at;
	int rva;
	const char *kind;

	if (!PyArg_ParseTuple(args, "Os", &at, &kind) || !TestMod_ParseRva(at, &rva)) {
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

/*
 * SDTest.PeekSpan(name, off, n) and SDTest.PokeSpan(name, off, value, n): n bytes (1..4, little endian) from off after
 * the name in the layout of the original, byte by byte through the C objects there in ScrapC.exe (stage 3c2: the
 * neighbours of the original are separate objects), the bytes no object holds read as 0 and are not written. For the
 * tests that read or write several neighbouring globals at once, such as a word over four byte flags.
 */
static int TestMod_Span(PyObject *name, int off, int n, uint32_t *rva) {
	const testsym_t *s = PyString_Check(name) ? TestMod_FindSym(PyString_AsString(name)) : NULL;

	if (!s || n < 1 || n > 4) {
		int x;
		/* a TypeError through the parser, the only error path the Python of the exe gives the hook DLL */
		PyArg_Parse(PyString_FromString(""), "i;SDTest: a name of src/core/testsyms.inc and 1..4 bytes", &x);
		return 0;
	}
	*rva = s->rva + (uint32_t)off;
	return 1;
}

static PyObject *SDTest_PeekSpan(PyObject *self, PyObject *args) {
	(void)self;
	PyObject *name;
	int off, n;
	uint32_t rva, v = 0;

	if (!PyArg_ParseTuple(args, "Oii", &name, &off, &n) || !TestMod_Span(name, off, n, &rva)) {
		return NULL;
	}
	for (int i = 0; i < n; i++) {
		const byte *b = TestMod_OrigByte(rva + (uint32_t)i);
		v |= (uint32_t)(b ? *b : 0) << (8 * i);
	}
	return PyInt_FromLong((int32_t)v);
}

static PyObject *SDTest_PokeSpan(PyObject *self, PyObject *args) {
	(void)self;
	PyObject *name;
	int off, value, n;
	uint32_t rva;

	if (!PyArg_ParseTuple(args, "Oiii", &name, &off, &value, &n) || !TestMod_Span(name, off, n, &rva)) {
		return NULL;
	}
	for (int i = 0; i < n; i++) {
		byte *b = TestMod_OrigByte(rva + (uint32_t)i);
		if (b) {
			*b = (byte)((uint32_t)value >> (8 * i));
		}
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *SDTest_Poke(PyObject *self, PyObject *args) {
	(void)self;
	PyObject *at;
	int rva, v;

	if (!PyArg_ParseTuple(args, "Oi", &at, &v) || !TestMod_ParseRva(at, &rva)) {
		return NULL;
	}
	*(int32_t *)Game_Ptr((uint32_t)rva) = v;
	Py_INCREF(Py_None);
	return Py_None;
}

/*
 * Calls a function of the exe that takes a float or double in xmm0 and returns in eax (the __ftoui3 and __dtoui3
 * conversions at rva 0x314310 and 0x3144f1, src/standalone/crtvec.c): lo and hi are the bits of the argument (lo
 * alone for a float), signed so Python 1.5.2 can pass the top bit. The asm keeps the bits exact, so a NaN or an
 * out of range value reaches the conversion as the test means it.
 */
uint32_t TestMod_ConvCall(void *fn, uint32_t lo, uint32_t hi, int isdouble);
__asm__(
	".text\n\t"
	".globl _TestMod_ConvCall\n"
	"_TestMod_ConvCall:\n\t"
	"movl 4(%esp), %eax\n\t"
	"cmpl $0, 16(%esp)\n\t"
	"je 1f\n\t"
	"movsd 8(%esp), %xmm0\n\t"
	"call *%eax\n\t"
	"ret\n"
	"1:\n\t"
	"movss 8(%esp), %xmm0\n\t"
	"call *%eax\n\t"
	"ret\n");

static PyObject *SDTest_ConvU(PyObject *self, PyObject *args) {
	(void)self;
	PyObject *at;
	int rva, lo, hi, isdouble;

	if (!PyArg_ParseTuple(args, "Oiii", &at, &lo, &hi, &isdouble) || !TestMod_ParseRva(at, &rva)) {
		return NULL;
	}
	/* errno through crt_errno is the one the conversion sets: the exe's static CRT with the hooks off, the linked
	 * UCRT in ScrapC.exe, each the one its __ftoui3 touches. Reset it so each call stands on its own. */
	*crt_errno() = 0;
	uint32_t r = TestMod_ConvCall(Game_Ptr((uint32_t)rva), (uint32_t)lo, (uint32_t)hi, isdouble);
	return Py_BuildValue("ii", (int)r, *crt_errno());
}

static PyObject *SDTest_Sym(PyObject *self, PyObject *args) {
	(void)self;
	PyObject *name;
	int rva;

	if (!PyArg_ParseTuple(args, "S", &name) || !TestMod_ParseRva(name, &rva)) {
		return NULL;
	}
	return PyInt_FromLong(rva);
}

static PyObject *SDTest_Base(PyObject *self, PyObject *args) {
	(void)self;

	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return PyInt_FromLong((long)game_base);
}

static PyObject *SDTest_Proc(PyObject *self, PyObject *args) {
	(void)self;
	const char *dll, *name;

	if (!PyArg_ParseTuple(args, "ss", &dll, &name)) {
		return NULL;
	}
	HMODULE m = GetModuleHandleA(dll);
	FARPROC fn = m ? GetProcAddress(m, name) : NULL;
	if (!fn) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return PyInt_FromLong((long)((uintptr_t)fn - game_base));
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

void D3DLog_Set(void (*out)(const char *s));

static PyObject *SDTest_D3DLog(PyObject *self, PyObject *args) {
	(void)self;
	int on;

	if (!PyArg_ParseTuple(args, "i", &on)) {
		return NULL;
	}
	D3DLog_Set(on ? SDTest_Collect : NULL);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *SDTest_HeapOk(PyObject *self, PyObject *args) {
	(void)self;
	HANDLE heaps[64];

	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	DWORD n = GetProcessHeaps(64, heaps);
	if (n > 64) {
		n = 64;
	}
	for (DWORD i = 0; i < n; i++) {
		if (!HeapValidate(heaps[i], 0, NULL)) {
			return Py_BuildValue("i", (int)i);
		}
	}
	return Py_BuildValue("i", -1);
}

static PyMethodDef sdtest_methods[] = {
	{ "Call", SDTest_Call, 1, NULL },
	{ "Peek", SDTest_Peek, 1, NULL },
	{ "Poke", SDTest_Poke, 1, NULL },
	{ "ConvU", SDTest_ConvU, 1, NULL },
	{ "Sym", SDTest_Sym, 1, NULL },
	{ "Base", SDTest_Base, 1, NULL },
	{ "Orig", SDTest_Orig, 1, NULL },
	{ "PeekSpan", SDTest_PeekSpan, 1, NULL },
	{ "PokeSpan", SDTest_PokeSpan, 1, NULL },
	{ "Proc", SDTest_Proc, 1, NULL },
	{ "OutFunc", SDTest_OutFunc, 1, NULL },
	{ "Out", SDTest_Out, 1, NULL },
	{ "D3DLog", SDTest_D3DLog, 1, NULL },
	{ "HeapOk", SDTest_HeapOk, 1, NULL },
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
