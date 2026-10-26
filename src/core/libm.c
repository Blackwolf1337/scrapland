/*
 * Math functions of the MSVC runtime linked into Scrap.exe. Not correctly rounded like sqrt, so the C code calls
 * the exe's versions to get the original results bit for bit. The __libm_sse2_* entries take the argument and give
 * the result in xmm0, the _CI* intrinsics (atan2) take theirs on the x87 stack and return st(0); neither is cdecl,
 * hence the inline asm. eax, ecx, edx and xmm0-7 are caller saved there like in cdecl.
 */
#include "scrap/game.h"
#include "scrap/mathlib.h"

#define LIBM_SSE2_ACOS 0x329f50
#define LIBM_SSE2_COS 0x346fe0
#define LIBM_SSE2_SIN 0x3471a0
#define CI_ATAN2 0x329f3a
#define LIBM_VEC_EXP2 0x314a00

static double Libm_Sse2(uint32_t rva, double x) {
	double r;
	void *fn = Game_Ptr(rva);

	__asm__ volatile(
		"movsd %[x], %%xmm0\n\t"
		"call *%[fn]\n\t"
		"movsd %%xmm0, %[r]\n\t"
		: [r] "=m"(r)
		: [x] "m"(x), [fn] "r"(fn)
		: "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
	return r;
}

double Game_Acos(double x) {
	return Libm_Sse2(LIBM_SSE2_ACOS, x);
}

double Game_Cos(double x) {
	return Libm_Sse2(LIBM_SSE2_COS, x);
}

double Game_Sin(double x) {
	return Libm_Sse2(LIBM_SSE2_SIN, x);
}

/* y is loaded first, x on top of it, as the exe's callers do */
double Game_Atan2(double y, double x) {
	double r;
	void *fn = Game_Ptr(CI_ATAN2);

	__asm__ volatile(
		"fldl %[y]\n\t"
		"fldl %[x]\n\t"
		"call *%[fn]\n\t"
		"fstpl %[r]\n\t"
		: [r] "=m"(r)
		: [y] "m"(y), [x] "m"(x), [fn] "r"(fn)
		: "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "st", "st(1)",
		"memory", "cc");
	return r;
}

/* x87 fsincos of a float, both results rounded to float: the inline code of the original */
void X87_SinCos(float a, float *s, float *c) {
	long double sl, cl;

	__asm__("fsincos" : "=t"(cl), "=u"(sl) : "0"((long double)a));
	*c = (float)cl;
	*s = (float)sl;
}

/* the packed double exp the compiler calls for vectorised loops, argument and result in xmm0 */
void Game_ExpPd(const double in[2], double out[2]) {
	void *fn = Game_Ptr(LIBM_VEC_EXP2);

	__asm__ volatile(
		"movupd %[in], %%xmm0\n\t"
		"call *%[fn]\n\t"
		"movupd %%xmm0, %[out]\n\t"
		: [out] "=m"(*(double(*)[2])out)
		: [in] "m"(*(const double(*)[2])in), [fn] "r"(fn)
		: "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
}
