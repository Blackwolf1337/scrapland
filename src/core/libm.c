/*
 * Math functions of the MSVC runtime linked into Scrap.exe. Not correctly rounded like sqrt, so the C code calls
 * the exe's versions to get the original results bit for bit. The _libm_sse2_*_precise entries take the argument and give
 * the result in xmm0, the _CI* intrinsics (atan2) take theirs on the x87 stack and return st(0); neither is cdecl,
 * hence the inline asm. eax, ecx, edx and xmm0-7 are caller saved there like in cdecl.
 *
 * ScrapC.exe calls the same entries of Microsoft's ucrtbase.dll and the helpers without an export in
 * src/standalone/crtvec.c, through the labels of include/scrap/crt.h (src/standalone/crtmath.c).
 */
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/mathlib.h"

#ifdef SD_STANDALONE
#define LIBM_FN(label, rva) ((void *)label)
#else
#define LIBM_FN(label, rva) Game_Ptr(rva)
#endif

#define LIBM_SSE2_ACOS LIBM_FN(crt_libm_sse2_acos_precise, 0x329f50)
#define LIBM_SSE2_COS LIBM_FN(crt_libm_sse2_cos_precise, 0x346fe0)
#define LIBM_SSE2_SIN LIBM_FN(crt_libm_sse2_sin_precise, 0x3471a0)
#define LIBM_SSE2_ATAN LIBM_FN(crt_libm_sse2_atan_precise, 0x347370)
#define LIBM_SSE2_EXP LIBM_FN(crt_libm_sse2_exp_precise, 0x34e950)
#define LIBM_SSE2_ASIN LIBM_FN(crt_libm_sse2_asin_precise, 0x350150)
#define LIBM_SSE2_POW LIBM_FN(crt_libm_sse2_pow_precise, 0x321440)
#define LIBM_SSE2_LOG LIBM_FN(crt_libm_sse2_log_precise, 0x34ec30)
#define CI_ATAN2 LIBM_FN(crt_CIatan2, 0x329f3a)
#define CI_FMOD LIBM_FN(crt_CIfmod, 0x32201a)
#define LIBM_VEC_EXP2 LIBM_FN(crt_vdecl_exp2, 0x314a00)
#define LIBM_VEC_COS2 LIBM_FN(crt_vdecl_cos2, 0x314930)
#define LIBM_VEC_SIN2 LIBM_FN(crt_vdecl_sin2, 0x314940)
#define CRT_FTOUI3 LIBM_FN(crt_ftoui3, 0x314310)
#define CRT_DTOUI3 LIBM_FN(crt_dtoui3, 0x3144f1)

static double Libm_Sse2(void *fn, double x) {
	double r;

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

/* _libm_sse2_atan_precise (its table holds pi / 2 and the series 1/3, 1/5, ...) */
double Game_Atan(double x) {
	return Libm_Sse2(LIBM_SSE2_ATAN, x);
}

double Game_Asin(double x) {
	return Libm_Sse2(LIBM_SSE2_ASIN, x);
}

double Game_Exp(double x) {
	return Libm_Sse2(LIBM_SSE2_EXP, x);
}

/* natural logarithm */
double Game_Log(double x) {
	return Libm_Sse2(LIBM_SSE2_LOG, x);
}

/* the exponent in xmm1 */
double Game_Pow(double x, double y) {
	double r;
	void *fn = LIBM_SSE2_POW;

	__asm__ volatile(
		"movsd %[x], %%xmm0\n\t"
		"movsd %[y], %%xmm1\n\t"
		"call *%[fn]\n\t"
		"movsd %%xmm0, %[r]\n\t"
		: [r] "=m"(r)
		: [x] "m"(x), [y] "m"(y), [fn] "r"(fn)
		: "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
	return r;
}

/* y is loaded first, x on top of it, as the exe's callers do */
double Game_Atan2(double y, double x) {
	double r;
	void *fn = CI_ATAN2;

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

/* the same with the result stored as a float straight from the x87 (fstp DWORD) */
float Game_Atan2F(double y, double x) {
	float r;
	void *fn = CI_ATAN2;

	__asm__ volatile(
		"fldl %[y]\n\t"
		"fldl %[x]\n\t"
		"call *%[fn]\n\t"
		"fstps %[r]\n\t"
		: [r] "=m"(r)
		: [y] "m"(y), [x] "m"(x), [fn] "r"(fn)
		: "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "st", "st(1)",
		"memory", "cc");
	return r;
}

/* _CIfmod: x loaded first, y on top of it, the fprem remainder of x / y */
double Game_Fmod(double x, double y) {
	double r;
	void *fn = CI_FMOD;

	__asm__ volatile(
		"fldl %[x]\n\t"
		"fldl %[y]\n\t"
		"call *%[fn]\n\t"
		"fstpl %[r]\n\t"
		: [r] "=m"(r)
		: [x] "m"(x), [y] "m"(y), [fn] "r"(fn)
		: "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "st", "st(1)",
		"memory", "cc");
	return r;
}

/* fld and fistp: a double to integer in the rounding mode of the x87, the inline conversion of MSVC */
int64_t X87_Fistp(double x) {
	int64_t r;

	__asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
	return r;
}

/* x87 fsincos of a float, both results rounded to float: the inline code of the original */
void X87_SinCos(float a, float *s, float *c) {
	long double sl, cl;

	__asm__("fsincos" : "=t"(cl), "=u"(sl) : "0"((long double)a));
	*c = (float)cl;
	*s = (float)sl;
}

/* the packed double functions the compiler calls for vectorised loops, argument and result in xmm0 */
static void Libm_Pd(void *fn, const double in[2], double out[2]) {
	__asm__ volatile(
		"movupd %[in], %%xmm0\n\t"
		"call *%[fn]\n\t"
		"movupd %%xmm0, %[out]\n\t"
		: [out] "=m"(*(double(*)[2])out)
		: [in] "m"(*(const double(*)[2])in), [fn] "r"(fn)
		: "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
}

void Game_ExpPd(const double in[2], double out[2]) {
	Libm_Pd(LIBM_VEC_EXP2, in, out);
}

void Game_SinPd(const double in[2], double out[2]) {
	Libm_Pd(LIBM_VEC_SIN2, in, out);
}

void Game_CosPd(const double in[2], double out[2]) {
	Libm_Pd(LIBM_VEC_COS2, in, out);
}

/* __ftoui3: a float in xmm0 to an unsigned int in eax, truncated, with the range checks of the runtime */
uint32_t Game_FtoUI(float x) {
	uint32_t r;
	void *fn = CRT_FTOUI3;

	__asm__ volatile(
		"movss %[x], %%xmm0\n\t"
		"call *%[fn]\n\t"
		: "=a"(r)
		: [x] "m"(x), [fn] "r"(fn)
		: "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
	return r;
}

/* __dtoui3: a double in xmm0 to an unsigned int in eax */
uint32_t Game_DtoUI(double x) {
	uint32_t r;
	void *fn = CRT_DTOUI3;

	__asm__ volatile(
		"movsd %[x], %%xmm0\n\t"
		"call *%[fn]\n\t"
		: "=a"(r)
		: [x] "m"(x), [fn] "r"(fn)
		: "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
	return r;
}
