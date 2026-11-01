/*
 * The MSVC math in ScrapC.exe (stage 2b of docs/standalone.md): the cdecl functions, the SSE2 entries
 * (_libm_sse2_*_precise, argument and result in xmm0) and the _CI intrinsics (x87 stack). By default they are the C of
 * src/standalone/crtlibm.c. When ucrtbase_ms.dll is in the exe folder (tools/test/getucrt.sh puts it there for the
 * Wine tests) Crt_MathInit takes them from it instead: Microsoft's code, the same as the static UCRT of the original,
 * exact in the last bits where crtlibm.c is not. Each entry is a jmp through a slot, so the registers of the SSE2 and
 * _CI conventions pass through unchanged. The C calls them by these labels (src/core/libm.c, the crt_* math of
 * src/imports.inc).
 *
 * Domain and range errors: the runtime calls the user matherr (_umatherr, rva 0x35657e in the original) and sets
 * errno when it returns 0 (_set_errno_from_matherr, rva 0x35654f: _DOMAIN EDOM, _SING and _OVERFLOW ERANGE). The
 * game registers E3D_MathErr, which logs and returns 0. Here Crt_MathErrMs is registered with the loaded ucrtbase
 * and calls E3D_MathErr. When that ucrtbase is not the one this exe links (ucrtbase_ms.dll under Wine) it keeps its
 * own errno, so Crt_MathErrMs sets the errno of this exe too, which the Python reads after its math calls.
 *
 * The helpers that no DLL exports (__ftoui3, __dtoui3 and the vector cos, sin and exp of vectorised loops) are C of
 * their own in src/standalone/crtvec.c.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>

#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/mathlib.h"

/* _set_errno_from_matherr: _DOMAIN EDOM, _SING and _OVERFLOW ERANGE */
void Crt_MathErrno(int type) {
	if (type == 1) {
		*crt_errno() = EDOM;
	} else if (type == 2 || type == 3) {
		*crt_errno() = ERANGE;
	}
}

/* label, export */
#define CRT_MATH_ENTRIES(X) \
	X(acos, "acos") \
	X(asin, "asin") \
	X(atan, "atan") \
	X(atan2, "atan2") \
	X(ceil, "ceil") \
	X(cos, "cos") \
	X(cosh, "cosh") \
	X(exp, "exp") \
	X(fabs, "fabs") \
	X(floor, "floor") \
	X(fmod, "fmod") \
	X(frexp, "frexp") \
	X(hypot, "_hypot") \
	X(ldexp, "ldexp") \
	X(log, "log") \
	X(log10, "log10") \
	X(modf, "modf") \
	X(pow, "pow") \
	X(sin, "sin") \
	X(sinh, "sinh") \
	X(sqrt, "sqrt") \
	X(tan, "tan") \
	X(tanh, "tanh") \
	X(libm_sse2_acos_precise, "_libm_sse2_acos_precise") \
	X(libm_sse2_asin_precise, "_libm_sse2_asin_precise") \
	X(libm_sse2_atan_precise, "_libm_sse2_atan_precise") \
	X(libm_sse2_cos_precise, "_libm_sse2_cos_precise") \
	X(libm_sse2_exp_precise, "_libm_sse2_exp_precise") \
	X(libm_sse2_log_precise, "_libm_sse2_log_precise") \
	X(libm_sse2_pow_precise, "_libm_sse2_pow_precise") \
	X(libm_sse2_sin_precise, "_libm_sse2_sin_precise") \
	X(CIatan2, "_CIatan2") \
	X(CIfmod, "_CIfmod")

/* the slots hold the own C of src/standalone/crtlibm.c until ucrtbase_ms.dll replaces them */
#define CRT_OWN(label, name) void own_##label(void);
#define CRT_SLOT(label, name) void *crt_##label##_ms = (void *)own_##label;
#define CRT_THUNK(label, name) \
	__asm__(".text\n\t.globl _crt_" #label "\n\t.p2align 4\n_crt_" #label ":\n\tjmp *_crt_" #label "_ms\n");
CRT_MATH_ENTRIES(CRT_OWN)
CRT_MATH_ENTRIES(CRT_SLOT)
CRT_MATH_ENTRIES(CRT_THUNK)

static const struct {
	const char *name;
	void **slot;
} crt_math_exports[] = {
#define CRT_EXPORT(label, name) { name, &crt_##label##_ms },
	CRT_MATH_ENTRIES(CRT_EXPORT)
#undef CRT_EXPORT
};

/* the loaded ucrtbase keeps an errno of its own (it is not the one this exe links) */
static int crt_math_apart;

static int Crt_MathErrMs(crtMathExc_t *e) {
	int r = E3D_MathErr(e);
	if (!r && crt_math_apart) {
		Crt_MathErrno(e->type);
	}
	return r;
}

const char *Crt_MathInit(const char *dir) {
	static char path[MAX_PATH];

	if (snprintf(path, sizeof(path), "%sucrtbase_ms.dll", dir) >= (int)sizeof(path)
		|| GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
		Com_Printf("crt: the MSVC math of its own (crtlibm.c)\n");
		return NULL;
	}
	HMODULE m = LoadLibraryA(path);
	if (!m) {
		return path;
	}
	for (size_t i = 0; i < sizeof(crt_math_exports) / sizeof(crt_math_exports[0]); i++) {
		if (!(*crt_math_exports[i].slot = (void *)GetProcAddress(m, crt_math_exports[i].name))) {
			return crt_math_exports[i].name;
		}
	}
	void (*setmatherr)(int (*)(crtMathExc_t *)) = (void *)GetProcAddress(m, "__setusermatherr");
	int (*sse2)(int) = (void *)GetProcAddress(m, "_set_SSE2_enable");
	int *(*err)(void) = (void *)GetProcAddress(m, "_errno");
	if (!setmatherr || !sse2 || !err) {
		return "__setusermatherr, _set_SSE2_enable or _errno";
	}
	crt_math_apart = err() != crt_errno();
	setmatherr(Crt_MathErrMs);
	/* the CRT initializer at rva 0x35fce1: the SSE2 code of the math functions when the CPU has SSE2 */
	sse2(IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE));
	Com_Printf("crt: the MSVC math from %s%s\n", path, crt_math_apart ? " (its errno is set in this exe too)" : "");
	return NULL;
}
