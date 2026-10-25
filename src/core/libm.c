/*
 * Math functions of the MSVC runtime linked into Scrap.exe. Not correctly rounded like sqrt, so the C code calls
 * the exe's versions to get the original results bit for bit. These are the __libm_sse2_* entries: argument and
 * result in xmm0, not cdecl, hence the inline asm. eax, ecx, edx and xmm0-7 are caller saved there like in cdecl.
 */
#include "scrap/game.h"
#include "scrap/mathlib.h"

#define LIBM_SSE2_ACOS 0x329f50

double Game_Acos(double x) {
	double r;
	void *fn = Game_Ptr(LIBM_SSE2_ACOS);

	__asm__ volatile(
		"movsd %[x], %%xmm0\n\t"
		"call *%[fn]\n\t"
		"movsd %%xmm0, %[r]\n\t"
		: [r] "=m"(r)
		: [x] "m"(x), [fn] "r"(fn)
		: "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
	return r;
}
