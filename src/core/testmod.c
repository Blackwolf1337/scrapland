/*
 * The builtin Python module SDTest for the differential tests (tools/test), on with [general] testmod=1 in
 * scrapdecomp.ini. The difftest runs the same scripts on the current ScrapC.exe and on a reference build
 * (DIFFTEST_REF) and compares the outputs.
 *
 * The functions and data go by name: SDTest.Sym(name) is the rva of a name of src/core/testsyms.inc (the address of
 * its C function or object minus the load address), and Call, Peek and Poke take such a name in place of the
 * rva. SDTest.Base() is the load address of the exe, so SDTest.Base() + rva is the address the pointers of the game
 * hold. A renamed C function or object needs its new name in testsyms.inc, testaddrs.inc or testfuncs.inc.
 * SDTest.Orig(v) is v with an address (or rva) of a C object or function turned into the one of the original
 * Scrap.exe, for the outputs (tools/test/py/sdlib.py prints every int through it), so they do not depend on where
 * the linker put things.
 * SDTest.PeekSpan(name, off, n) and PokeSpan(name, off, value, n): n bytes after a name in the original's layout
 * (several neighbouring globals of the original at once, separate C objects here).
 *
 * SDTest.Call(rva, sig, args...): sig is the return type and then one character per argument.
 *   return: v nothing, i int, b byte (al), s char * (None for NULL), f float (st0)
 *   args:   i int, f float, s string, z string or None, b string copied into a buffer of BUF_SIZE bytes (rest 0)
 *           that comes back, p int and D double that are passed by pointer and come back, F a tuple of numbers
 *           passed as an array of floats (up to MAX_FLOATS) that comes back as a tuple
 * The result is a tuple: the return value, then the b, p, D and F arguments in their order. cdecl functions with up to
 * MAX_ARGS arguments of 4 bytes; "t" before sig calls a method, the first argument is this (up to
 * MAX_THIS_ARGS more).
 *
 * SDTest.Peek(rva, kind) reads a global of the exe: i int, b byte, d double, s the characters at rva, p the string a
 * pointer at rva points to (None for NULL); with '@' before the kind rva is an address (from a returned pointer).
 * SDTest.Poke(rva, value) writes an int.
 * SDTest.Proc(dll, name) is the function name of the loaded DLL dll as an rva for Call (its address minus the load
 * address of the exe), None when there is none: the Win32 functions for a test, in both runs the same.
 * SDTest.OutFunc() is the address of a cdecl void(const char *) that collects text, SDTest.Out() returns and clears
 * that text (for the output callbacks of the engine).
 * SDTest.D3DLog(on) puts a proxy in place of the D3D device that writes its calls into that text (src/core/d3dlog.c),
 * SDTest.D3DLog(0) the device back.
 * SDTest.HeapOk() is -1 when every heap of the process passes HeapValidate, else the index of the first bad one (to
 * find the test call that breaks a heap).
 */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#include "scrap/crt.h"
#include "scrap/python.h"

/* the load address of the exe (src/core/game.c): the rvas of the tests are C addresses minus it (TestMod_SymRva) */
extern uintptr_t game_base;

static inline void *Game_Ptr(uint32_t rva) {
	return (void *)(game_base + rva);
}

#define MAX_ARGS 16
#define MAX_THIS_ARGS 11
#define MAX_OUT 8
#define BUF_SIZE 1024
#define MAX_FLOATS 64

typedef uint32_t (*call_int_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef float (*call_float_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

#define THIS_TYPES(name, ret, conv) \
	typedef ret(conv *name##0_t)(uint32_t); \
	typedef ret(conv *name##1_t)(uint32_t, uint32_t); \
	typedef ret(conv *name##2_t)(uint32_t, uint32_t, uint32_t); \
	typedef ret(conv *name##3_t)(uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(conv *name##4_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(conv *name##5_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(conv *name##6_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(conv *name##7_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t); \
	typedef ret(conv *name##8_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t, uint32_t); \
	typedef ret(conv *name##9_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t, uint32_t, uint32_t); \
	typedef ret(conv *name##10_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t, uint32_t, uint32_t, uint32_t); \
	typedef ret(conv *name##11_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
		uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
THIS_TYPES(this, uint32_t, )
THIS_TYPES(thisf, float, )
/* the callbacks of Windows, COM and FMOD and the D3DX functions keep __stdcall: the tests call them with "t" and a
 * dummy this, which works as a thiscall call (this in ecx, the callee pops the rest) */
THIS_TYPES(std, uint32_t, __thiscall)
THIS_TYPES(stdf, float, __thiscall)

/* a method with the exact count of its arguments */
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

static uint32_t Call_Std(void *fn, const uint32_t *a, int n) {
	CALL_THIS(std, fn, a, n)
}

static float Call_StdFloat(void *fn, const uint32_t *a, int n) {
	CALL_THIS(stdf, fn, a, n)
}

/* the __stdcall functions of the exe (its decorated names _name@n; the label without the decoration resolves through
 * --enable-stdcall-fixup): the callbacks of Windows, COM and FMOD, the import thunks and D3DX */
#define TEST_STDCALL(name) extern char sds_##name[] __asm__("_" #name);
#define TEST_STDCALLS \
	TEST_STDCALL(App_ExceptionFilter) TEST_STDCALL(App_WinMain) TEST_STDCALL(App_WndProc) TEST_STDCALL(Bsp_DlgProc) \
	TEST_STDCALL(crt_eh_vector_ctor) \
	TEST_STDCALL(crt_eh_vector_dtor) TEST_STDCALL(D3DApp_WndProc) \
	TEST_STDCALL(D3DX_CheckTextureRequirements) \
	TEST_STDCALL(D3DX_CreateTextureFromFileInMemoryEx) TEST_STDCALL(D3DX_LoadSurfaceFromSurface) \
	TEST_STDCALL(D3DX_MatrixInverse) TEST_STDCALL(D3DX_MatrixMultiply) TEST_STDCALL(D3DX_MatrixRotationQuaternion) \
	TEST_STDCALL(D3DX_MatrixRotationX) TEST_STDCALL(D3DX_MatrixRotationY) TEST_STDCALL(D3DX_MatrixRotationYPR) \
	TEST_STDCALL(D3DX_MatrixRotationZ) TEST_STDCALL(D3DX_MatrixTranspose) TEST_STDCALL(D3DX_QuaternionRotationYPR) \
	TEST_STDCALL(D3DX_Vec3Normalize) TEST_STDCALL(D3DX_Vec3Transform) TEST_STDCALL(DbgWin_DlgProc) \
	TEST_STDCALL(Engine_SceneDlgProc) \
	TEST_STDCALL(Wsock_bind) \
	TEST_STDCALL(Wsock_closesocket) TEST_STDCALL(Wsock_gethostbyaddr) TEST_STDCALL(Wsock_gethostbyname) \
	TEST_STDCALL(Wsock_htonl) TEST_STDCALL(Wsock_htons) TEST_STDCALL(Wsock_ioctlsocket) TEST_STDCALL(Wsock_ntohs) \
	TEST_STDCALL(Wsock_recvfrom) TEST_STDCALL(Wsock_select) TEST_STDCALL(Wsock_sendto) TEST_STDCALL(Wsock_setsockopt) \
	TEST_STDCALL(Wsock_socket) TEST_STDCALL(Wsock_WSACleanup) TEST_STDCALL(Wsock_WSAStartup)
TEST_STDCALLS
#undef TEST_STDCALL
#define TEST_STDCALL(name) sds_##name,
static const char *const testStdcalls[] = { TEST_STDCALLS };
#undef TEST_STDCALL

/* also every function outside the exe: the methods of COM objects and the Win32 functions of SDTest.Proc */
static int TestMod_IsStdcall(const void *fn) {
	const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)game_base;
	const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(game_base + dos->e_lfanew);
	if ((uintptr_t)fn < game_base || (uintptr_t)fn >= game_base + nt->OptionalHeader.SizeOfImage) {
		return 1;
	}
	for (size_t i = 0; i < sizeof(testStdcalls) / sizeof(testStdcalls[0]); i++) {
		if (testStdcalls[i] == fn) {
			return 1;
		}
	}
	return 0;
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
 * The table of the names, sorted by name (strcmp order). An entry holds the address of the C function or object (plus
 * the offset of a view inside the object), taken from its assembler label, so no prototype is needed.
 */
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
 * of src/core/testfuncs.inc with the rva of the first original it backs: the way from an address of ScrapC.exe to the
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
#define SD_TEST_FADDR(name, rva) extern char sda_##name[] __asm__("_" #name);
#include "testfuncs.inc"
#undef SD_TEST_FADDR

static const testaddr_t testObjs[] = {
#define SD_TEST_ADDR(name, rva, size) { sda_##name, size, rva },
#include "testaddrs.inc"
#undef SD_TEST_ADDR
};

static const testaddr_t testFuncs[] = {
#define SD_TEST_FADDR(name, rva) { sda_##name, 0, rva },
#include "testfuncs.inc"
#undef SD_TEST_FADDR
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
 * of the game prints this.
 */
static uint32_t TestMod_Orig(uint32_t v) {
	/* the rva form for data only: small numbers hit the starts of the functions (aligned to 16) too often. It goes
	 * first: the tests print the pointers to data as rvas (p - 0x400000), and the rva of a C object can be the address
	 * of a C function too */
	uint32_t r = TestMod_OrigRva((uintptr_t)v + game_base);
	if (r >= TEST_DATA_RVA) {
		return r;
	}
	r = TestMod_OrigRva(v);
	return r ? 0x400000 + r : v;
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
	if (thiscall && TestMod_IsStdcall(fn)) {
		ret = sig[0] == 'f' ? PyFloat_FromDouble(Call_StdFloat(fn, a, n)) : SDTest_Result(sig[0], Call_Std(fn, a, n));
	} else if (thiscall && sig[0] == 'f') {
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
		/* a TypeError through the parser */
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
