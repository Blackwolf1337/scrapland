/*
 * The MSVC runtime helpers that ucrtbase.dll does not export, written in C from their documented behavior (stage 2b
 * of docs/standalone.md), not copied from the original's instructions:
 *
 * - __ftoui3 and __dtoui3: a float or double in xmm0 to an unsigned 32 bit integer in eax, truncated toward zero.
 *   These are the helpers MSVC emits for a (unsigned)float cast. The value is the low 32 bits of the 64 bit
 *   truncation (so a negative or too large input wraps); a non-finite input or one of magnitude 2^63 or more is the
 *   "integer indefinite" of the conversion, eax 0, and sets errno EDOM like the runtime's error path.
 * - the packed cos, sin and exp of two doubles in xmm0 that the compiler emits for vectorised loops. Each lane is
 *   the scalar function of ucrtbase (crt_cos, crt_sin, crt_exp through crtmath.c), so the result is the MSVC scalar
 *   value, within about one ulp of the original's vector code. A last bit difference is accepted; the tests that
 *   reach these (CBmpFlt_Exposure, the lava cos table) match bit for bit all the same.
 *
 * The entries keep the conventions of the originals (argument and result in xmm0, eax for the conversions), so the C
 * of src/core/libm.c and the code of the copy reach them unchanged (src/standalone/crthooks.inc). A short asm wrapper
 * marshals the xmm register to and from the stack and calls the C below; it may clobber eax, ecx, edx and xmm0..7
 * like any cdecl call, and keeps ebx, esi, edi and ebp, as the callers of both conventions expect.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>

#include "scrap/crt.h"

#define TWO63 9223372036854775808.0

static uint32_t Crt_ToUI(double x) {
	if (!isfinite(x) || fabs(x) >= TWO63) {
		*crt_errno() = EDOM;
		return 0;
	}
	/* truncate toward zero into 64 bits, keep the low 32, as the runtime returns edx:eax */
	return (uint32_t)(uint64_t)(int64_t)x;
}

uint32_t Crt_FtoUI_C(float x) {
	return Crt_ToUI((double)x);
}

uint32_t Crt_DtoUI_C(double x) {
	return Crt_ToUI(x);
}

void Crt_VCos2_C(const double in[2], double out[2]) {
	out[0] = crt_cos(in[0]);
	out[1] = crt_cos(in[1]);
}

void Crt_VSin2_C(const double in[2], double out[2]) {
	out[0] = crt_sin(in[0]);
	out[1] = crt_sin(in[1]);
}

void Crt_VExp2_C(const double in[2], double out[2]) {
	out[0] = crt_exp(in[0]);
	out[1] = crt_exp(in[1]);
}

__asm__(
	".text\n\t"
	/* __ftoui3: the float in xmm0 low, result in eax */
	".globl _crt_ftoui3\n\t"
	".p2align 4\n"
	"_crt_ftoui3:\n\t"
	"subl $4, %esp\n\t"
	"movss %xmm0, (%esp)\n\t"
	"call _Crt_FtoUI_C\n\t"
	"addl $4, %esp\n\t"
	"ret\n"

	/* __dtoui3: the double in xmm0, result in eax */
	".globl _crt_dtoui3\n\t"
	".p2align 4\n"
	"_crt_dtoui3:\n\t"
	"subl $8, %esp\n\t"
	"movsd %xmm0, (%esp)\n\t"
	"call _Crt_DtoUI_C\n\t"
	"addl $8, %esp\n\t"
	"ret\n"

	/* the packed functions: the two doubles in xmm0, the two results back in xmm0 */
	".globl _crt_vdecl_cos2\n\t"
	".p2align 4\n"
	"_crt_vdecl_cos2:\n\t"
	"jmp _Crt_Vdecl2\n\t"
	".globl _crt_vdecl_sin2\n\t"
	".p2align 4\n"
	"_crt_vdecl_sin2:\n\t"
	"movl $_Crt_VSin2_C, %eax\n\t"
	"jmp _Crt_Vdecl2b\n\t"
	".globl _crt_vdecl_exp2\n\t"
	".p2align 4\n"
	"_crt_vdecl_exp2:\n\t"
	"movl $_Crt_VExp2_C, %eax\n\t"
	"jmp _Crt_Vdecl2b\n"

	"_Crt_Vdecl2:\n\t"
	"movl $_Crt_VCos2_C, %eax\n"
	"_Crt_Vdecl2b:\n\t"
	"pushl %ebp\n\t"
	"movl %esp, %ebp\n\t"
	"andl $-16, %esp\n\t"
	"subl $48, %esp\n\t"			/* the two arguments, out[2] at 16, in[2] at 32 */
	"movupd %xmm0, 32(%esp)\n\t"
	"leal 32(%esp), %ecx\n\t"
	"movl %ecx, (%esp)\n\t"
	"leal 16(%esp), %ecx\n\t"
	"movl %ecx, 4(%esp)\n\t"
	"call *%eax\n\t"
	"movupd 16(%esp), %xmm0\n\t"
	"movl %ebp, %esp\n\t"
	"popl %ebp\n\t"
	"ret\n");
