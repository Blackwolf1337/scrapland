/*
 * The MSVC math in ScrapC.exe (stage 2b of docs/standalone.md). The original links the static UCRT; the same code is
 * Microsoft's ucrtbase.dll, which exports the cdecl functions, the SSE2 entries (_libm_sse2_*_precise, argument and result
 * in xmm0) and the _CI intrinsics (x87 stack). Crt_MathInit loads ucrtbase_ms.dll from the exe folder when it is
 * there (tools/test/getucrt.sh puts it there for the Wine tests: Wine's own ucrtbase differs in the last bits),
 * otherwise the system ucrtbase.dll, which on Windows is Microsoft's and the one this exe links. Only the math comes
 * from it; the number formatting and parsing are C of their own (src/standalone/crtfmt.c, crtscan.c). Each entry is
 * a jmp through a slot holding the export, so the registers of the SSE2 and _CI conventions pass through unchanged.
 * The C calls them by these labels (src/core/libm.c, the crt_* math of src/imports.inc).
 *
 * Domain and range errors: the runtime calls the user matherr (_umatherr, rva 0x35657e in the original) and sets
 * errno when it returns 0 (_set_errno_from_matherr, rva 0x35654f: _DOMAIN EDOM, _SING and _OVERFLOW ERANGE). The
 * game registers E3D_MathErr, which logs and returns 0. Here Crt_MathErrMs is registered with the loaded ucrtbase
 * and calls E3D_MathErr. When that ucrtbase is not the one this exe links (ucrtbase_ms.dll under Wine) it keeps its
 * own errno, so Crt_MathErrMs sets the errno of this exe too, which the Python reads after its math calls.
 *
 * The helpers that no DLL exports (__ftoui3, __dtoui3 and the vector cos, sin and exp of vectorised loops) are C of
 * their own in src/standalone/crtvec.c.
 *
 * Built with the CMake option SCRAP_OWN_MATH (SD_OWN_MATH) the entries are the C of src/standalone/crtlibm.c and no
 * DLL is loaded.
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

#ifdef SD_OWN_MATH

const char *Crt_MathInit(const char *dir) {
	(void)dir;
	Com_Printf("crt: the MSVC math of its own (crtlibm.c)\n");
	return NULL;
}

#else

/* label, export */
#define CRT_MATH_ENTRIES(X) \
	X(crt_acos, "acos") \
	X(crt_asin, "asin") \
	X(crt_atan, "atan") \
	X(crt_atan2, "atan2") \
	X(crt_ceil, "ceil") \
	X(crt_cos, "cos") \
	X(crt_cosh, "cosh") \
	X(crt_exp, "exp") \
	X(crt_fabs, "fabs") \
	X(crt_floor, "floor") \
	X(crt_fmod, "fmod") \
	X(crt_frexp, "frexp") \
	X(crt_hypot, "_hypot") \
	X(crt_ldexp, "ldexp") \
	X(crt_log, "log") \
	X(crt_log10, "log10") \
	X(crt_modf, "modf") \
	X(crt_pow, "pow") \
	X(crt_sin, "sin") \
	X(crt_sinh, "sinh") \
	X(crt_sqrt, "sqrt") \
	X(crt_tan, "tan") \
	X(crt_tanh, "tanh") \
	X(crt_libm_sse2_acos_precise, "_libm_sse2_acos_precise") \
	X(crt_libm_sse2_asin_precise, "_libm_sse2_asin_precise") \
	X(crt_libm_sse2_atan_precise, "_libm_sse2_atan_precise") \
	X(crt_libm_sse2_cos_precise, "_libm_sse2_cos_precise") \
	X(crt_libm_sse2_exp_precise, "_libm_sse2_exp_precise") \
	X(crt_libm_sse2_log_precise, "_libm_sse2_log_precise") \
	X(crt_libm_sse2_pow_precise, "_libm_sse2_pow_precise") \
	X(crt_libm_sse2_sin_precise, "_libm_sse2_sin_precise") \
	X(crt_CIatan2, "_CIatan2") \
	X(crt_CIfmod, "_CIfmod")

#define CRT_SLOT(label, name) void *label##_ms;
#define CRT_THUNK(label, name) \
	__asm__(".text\n\t.globl _" #label "\n\t.p2align 4\n_" #label ":\n\tjmp *_" #label "_ms\n");
CRT_MATH_ENTRIES(CRT_SLOT)
CRT_MATH_ENTRIES(CRT_THUNK)

static const struct {
	const char *name;
	void **slot;
} crt_math_exports[] = {
#define CRT_EXPORT(label, name) { name, &label##_ms },
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
	HMODULE m = NULL;

	if (snprintf(path, sizeof(path), "%sucrtbase_ms.dll", dir) < (int)sizeof(path)
		&& GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
		m = LoadLibraryA(path);
	} else {
		snprintf(path, sizeof(path), "ucrtbase.dll");
		m = LoadLibraryA(path);
	}
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

#endif
