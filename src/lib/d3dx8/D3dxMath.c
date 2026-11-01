/*
 * The D3DX 8.1 math the game calls (D3DX8 is linked statically into Scrap.exe, its code below rva 0x03e800).
 *
 * Each public D3DX function of the exe is a stub "jmp [slot]" into a table at rva 0x43da08, which the first call
 * fills for the CPU: the x87 versions first, then the 3DNow!, SSE2 or SSE versions over some of them,
 * unless the registry value Direct3D\DisableD3DXPSGP is set. These are the versions a CPU with SSE2 and without
 * 3DNow! gets (every CPU the game runs on today): SSE2 for normalize, transform, multiply, inverse and the rotations
 * about x and y, the x87 code for transpose, the rotation about z, yaw pitch roll and the quaternion matrix. The
 * float operations are the ones of that code, in its order; the reciprocal and reciprocal square root estimates
 * (rcpss, rsqrtss) are the same instructions, the packed code is written with SSE intrinsics where its lanes mix.
 * The x87 code is C on long double, the precision the control word gives, with floats where it stores to memory.
 *
 * The hooks sit on the public stubs, so the hook DLL runs these too.
 */
#include <string.h>
#include <xmmintrin.h>
#include <emmintrin.h>

#include "scrap/d3d8.h"

/* fld DWORD, fstp DWORD: the x87 copy of a float in the original (a signaling NaN becomes quiet) */
static inline float X87_PassF(float f) {
	float r;

	__asm__("flds %1\n\tfstps %0" : "=m"(r) : "m"(f));
	return r;
}

static inline float Sse_Lane(__m128 v, int i) {
	float f[4];

	_mm_storeu_ps(f, v);
	return f[i];
}

#define K4(a, b, c, d) { (int32_t)(a), (int32_t)(b), (int32_t)(c), (int32_t)(d) }
#define K1(a) K4(a, a, a, a)

typedef union {
	int32_t i[4];
	__m128 v;
} d3dxConst_t;

/* the constants of the SSE sine and cosine (rva 0x4400e0..0x4402e0, bit patterns) */
static const d3dxConst_t k_scOut = { K4(0, 0x3f800000, 0, 0x3f800000) };		/* 0x4400e0 */
static const d3dxConst_t k_scP1 = { K4(0x3f800000, 0xbf000000, 0x3f800000, 0xbf000000) };	/* 0x4400f0 */
static const d3dxConst_t k_scP2 = { K4(0xbe2aaaa3, 0x3d2aaaa5, 0xbe2aaaa3, 0x3d2aaaa5) };	/* 0x440100 */
static const d3dxConst_t k_scP3 = { K4(0x3c08839d, 0xbab6061a, 0x3c08839d, 0xbab6061a) };	/* 0x440110 */
static const d3dxConst_t k_scP4 = { K4(0xb94ca1f0, 0x37ccf5ce, 0xb94ca1f0, 0x37ccf5ce) };	/* 0x440120 */
static const d3dxConst_t k_two = { K1(2) };						/* 0x440130 */
static const d3dxConst_t k_one = { K1(1) };						/* 0x440140 */
static const d3dxConst_t k_mant = { K1(0x007fffff) };					/* 0x440150 */
static const d3dxConst_t k_round = { K1(0x4b000000) };					/* 0x440160, 2^23 */
static const d3dxConst_t k_pio2Parts = { K4(0x3fc90000, 0x39fda000, 0x33a22000, 0x2c34611a) };	/* 0x440170 */
static const d3dxConst_t k_2opi = { K1(0x3f22f983) };					/* 0x440180 */
static const d3dxConst_t k_sign = { K1(0x80000000) };					/* 0x440190 */
static const d3dxConst_t k_abs = { K1(0x7fffffff) };					/* 0x4401a0 */
static const d3dxConst_t k_onef = { K1(0x3f800000) };					/* 0x4401b0, 0x4401d0 */
static const d3dxConst_t k_halfNeg = { K1(0xbf000000) };				/* 0x4401c0 */
static const d3dxConst_t k_c24 = { K1(0x3d2aaaa5) };					/* 0x4401e0 */
static const d3dxConst_t k_s6 = { K1(0xbe2aaaa3) };					/* 0x4401f0 */
static const d3dxConst_t k_c720 = { K1(0xbab6061a) };					/* 0x440200 */
static const d3dxConst_t k_s120 = { K1(0x3c08839d) };					/* 0x440210 */
static const d3dxConst_t k_c8 = { K1(0x37ccf5ce) };					/* 0x440250 */
static const d3dxConst_t k_s7 = { K1(0xb94ca1f0) };					/* 0x440260 */
static const d3dxConst_t k_pio2_3 = { K1(0x2c34611a) };				/* 0x440280 */
static const d3dxConst_t k_pio2_2 = { K1(0x33a22000) };				/* 0x440290 */
static const d3dxConst_t k_pio2_1 = { K1(0x39fda000) };				/* 0x4402a0 */
static const d3dxConst_t k_pio2_0 = { K1(0x3fc90000) };				/* 0x4402b0 */

#define I(v) _mm_castps_si128(v)
#define F(v) _mm_castsi128_ps(v)

/*
 * rva 0x03a480: cosine and sine of the angle in all four lanes, in lanes 0 and 1 (2 and 3 the same). The lanes of
 * the reduction subtract the four parts of pi / 2 in a rotated order, like the original.
 */
static __m128 D3dx_SinCosSS(__m128 x0) {
	__m128 x1 = x0, x2, x3, x4, x5, x7;

	x0 = F(_mm_and_si128(I(x0), I(k_abs.v)));
	x1 = F(_mm_and_si128(I(x1), I(k_sign.v)));
	x2 = x0;
	x0 = _mm_mul_ps(x0, k_2opi.v);
	x4 = k_pio2Parts.v;
	x0 = _mm_add_ps(x0, k_round.v);
	x3 = x0;
	x0 = _mm_sub_ps(x0, k_round.v);
	x4 = _mm_mul_ps(x4, x0);
	x2 = _mm_sub_ps(x2, x4);
	x4 = _mm_shuffle_ps(x4, x4, 0x39);
	x2 = _mm_sub_ps(x2, x4);
	x4 = _mm_shuffle_ps(x4, x4, 0x39);
	x2 = _mm_sub_ps(x2, x4);
	x4 = _mm_shuffle_ps(x4, x4, 0x39);
	x2 = _mm_sub_ps(x2, x4);
	x4 = _mm_mul_ps(x2, x2);
	x3 = _mm_and_ps(x3, k_mant.v);
	x0 = _mm_and_ps(x3, k_one.v);
	x7 = F(_mm_add_epi32(I(x3), I(x0)));
	x3 = F(_mm_sub_epi32(I(x3), I(x0)));
	x3 = F(_mm_slli_epi32(_mm_and_si128(I(x3), I(k_two.v)), 30));
	x7 = F(_mm_slli_epi32(_mm_and_si128(I(x7), I(k_two.v)), 30));
	x3 = F(_mm_xor_si128(I(x3), I(x1)));
	x0 = F(_mm_slli_epi32(I(x0), 30));
	x0 = _mm_cmpneq_ps(x0, _mm_setzero_ps());
	x5 = _mm_mul_ps(k_scP4.v, x4);
	x5 = _mm_add_ps(x5, k_scP3.v);
	x5 = _mm_mul_ps(x5, x4);
	x5 = _mm_add_ps(x5, k_scP2.v);
	x5 = _mm_mul_ps(x5, x4);
	x5 = _mm_add_ps(x5, k_scP1.v);
	x2 = _mm_unpacklo_ps(x2, x4);
	x5 = _mm_mul_ps(x5, x2);
	x5 = _mm_add_ps(x5, k_scOut.v);
	x2 = _mm_andnot_ps(x0, x5);
	x0 = _mm_and_ps(x0, x5);
	x2 = _mm_shuffle_ps(x2, x2, 0xb1);
	x0 = _mm_or_ps(x0, x2);
	x7 = _mm_unpacklo_ps(x7, x3);
	return _mm_xor_ps(x0, x7);
}

/* rva 0x03a570: sine and cosine of four angles */
static void D3dx_SinCosPS(__m128 x0, __m128 *sinOut, __m128 *cosOut) {
	__m128 x1 = x0, x2, x3, x4, x5, x6, x7;

	x0 = F(_mm_and_si128(I(x0), I(k_abs.v)));
	x1 = F(_mm_and_si128(I(x1), I(k_sign.v)));
	x2 = x0;
	x0 = _mm_mul_ps(x0, k_2opi.v);
	x0 = _mm_add_ps(x0, k_round.v);
	x3 = x0;
	x0 = _mm_sub_ps(x0, k_round.v);
	x4 = _mm_mul_ps(k_pio2_0.v, x0);
	x5 = _mm_mul_ps(k_pio2_1.v, x0);
	x6 = _mm_mul_ps(k_pio2_2.v, x0);
	x7 = _mm_mul_ps(k_pio2_3.v, x0);
	x2 = _mm_sub_ps(x2, x4);
	x2 = _mm_sub_ps(x2, x5);
	x2 = _mm_sub_ps(x2, x6);
	x2 = _mm_sub_ps(x2, x7);
	x4 = _mm_mul_ps(x2, x2);
	x3 = _mm_and_ps(x3, k_mant.v);
	x0 = _mm_and_ps(x3, k_one.v);
	x7 = F(_mm_add_epi32(I(x3), I(x0)));
	x3 = F(_mm_sub_epi32(I(x3), I(x0)));
	x3 = F(_mm_slli_epi32(_mm_and_si128(I(x3), I(k_two.v)), 30));
	x7 = F(_mm_slli_epi32(_mm_and_si128(I(x7), I(k_two.v)), 30));
	x3 = F(_mm_xor_si128(I(x3), I(x1)));
	x0 = F(_mm_slli_epi32(I(x0), 30));
	x0 = _mm_cmpneq_ps(x0, _mm_setzero_ps());
	x5 = _mm_mul_ps(k_s7.v, x4);
	x6 = _mm_mul_ps(k_c8.v, x4);
	x5 = _mm_add_ps(x5, k_s120.v);
	x6 = _mm_add_ps(x6, k_c720.v);
	x5 = _mm_mul_ps(x5, x4);
	x6 = _mm_mul_ps(x6, x4);
	x5 = _mm_add_ps(x5, k_s6.v);
	x6 = _mm_add_ps(x6, k_c24.v);
	x5 = _mm_mul_ps(x5, x4);
	x6 = _mm_mul_ps(x6, x4);
	x5 = _mm_add_ps(x5, k_onef.v);
	x6 = _mm_add_ps(x6, k_halfNeg.v);
	x5 = _mm_mul_ps(x5, x2);
	x6 = _mm_mul_ps(x6, x4);
	x6 = _mm_add_ps(x6, k_onef.v);
	x1 = _mm_or_ps(_mm_andnot_ps(x0, x5), _mm_and_ps(x0, x6));
	x0 = _mm_or_ps(_mm_and_ps(x0, x5), _mm_andnot_ps(x0, x6));
	*cosOut = _mm_xor_ps(x0, x7);
	*sinOut = _mm_xor_ps(x1, x3);
}

#undef I
#undef F

/* rva 0x02b490 (SSE2): zero below a squared length of 2^-46, else one Newton step on the estimate */
vec3_t *__stdcall D3DX_Vec3Normalize(vec3_t *out, const vec3_t *v) {
	float x = v->x, y = v->y, z = v->z;
	float len2 = (x * x + y * y) + z * z;

	if (0x1p-46f > len2) {
		out->x = 0.0f;
		out->y = 0.0f;
		out->z = 0.0f;
		return out;
	}
	float r = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(len2)));
	float k = (0.5f * r) * (3.0f - (len2 * r) * r);
	out->x = k * x;
	out->y = k * y;
	out->z = k * z;
	return out;
}

/* rva 0x02b180 (SSE2): v * m divided by w, w from the reciprocal estimate and one Newton step (TransformCoord) */
vec3_t *__stdcall D3DX_Vec3Transform(vec3_t *out, const vec3_t *v, const matrix_t *m) {
	const float *a = m->m;
	float r[4];

	for (int i = 0; i < 4; i++) {
		r[i] = (v->x * a[i] + v->y * a[4 + i]) + (v->z * a[8 + i] + a[12 + i]);
	}
	float e = _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(r[3])));
	float k = (e + e) - (e * r[3]) * e;
	out->x = r[0] * k;
	out->y = r[1] * k;
	out->z = r[2] * k;
	return out;
}

/* rva 0x02b340 (SSE2); out may be a or b */
matrix_t *__stdcall D3DX_MatrixMultiply(matrix_t *out, const matrix_t *a, const matrix_t *b) {
	matrix_t r;

	for (int i = 0; i < 4; i++) {
		const float *ai = a->m + 4 * i;
		for (int j = 0; j < 4; j++) {
			r.m[4 * i + j] = ((ai[0] * b->m[j] + ai[1] * b->m[4 + j]) + ai[2] * b->m[8 + j]) + ai[3] * b->m[12 + j];
		}
	}
	memcpy(out, &r, sizeof(r));
	return out;
}

/* rva 0x007a07 (x87): one of each pair of elements goes through the FPU, the other and the diagonal as integers */
matrix_t *__stdcall D3DX_MatrixTranspose(matrix_t *out, const matrix_t *m) {
	static const byte pairs[5][2] = { { 2, 8 }, { 3, 12 }, { 6, 9 }, { 7, 13 }, { 11, 14 } };
	float m1 = m->m[1], m4 = m->m[4];

	out->m[1] = X87_PassF(m4);
	out->m[4] = X87_PassF(m1);
	for (int i = 0; i < 5; i++) {
		uint32_t lo;
		memcpy(&lo, &m->m[pairs[i][1]], 4);
		float hi = m->m[pairs[i][0]];
		memcpy(&out->m[pairs[i][0]], &lo, 4);
		out->m[pairs[i][1]] = X87_PassF(hi);
	}
	if (out != m) {
		static const byte diag[4] = { 0, 5, 10, 15 };
		for (int i = 0; i < 4; i++) {
			memcpy(&out->m[diag[i]], &m->m[diag[i]], 4);
		}
	}
	return out;
}

/*
 * rva 0x02b6e0 (SSE2): the inverse by Cramer's rule of Intel's SIMD code (the transposed cofactors from 2x2
 * products), the reciprocal of the determinant from the estimate and one Newton step. NULL (out untouched) when the
 * determinant is 0 or NaN; det gets it otherwise.
 */
matrix_t *__stdcall D3DX_MatrixInverse(matrix_t *out, float *det, const matrix_t *m) {
	const float *p = m->m;
	__m128 x0, x1, x2, x3, x4, x5, x6, x7;
	__m128 s10, s20, s30, s40, s50, s60, s70, s80, s90, sa0, sb0, sc0, sd0, se0;

	x0 = _mm_loadh_pi(_mm_loadl_pi(_mm_setzero_ps(), (const __m64 *)(p + 2)), (const __m64 *)(p + 6));
	x4 = _mm_loadh_pi(_mm_loadl_pi(_mm_setzero_ps(), (const __m64 *)(p + 10)), (const __m64 *)(p + 14));
	x3 = _mm_loadh_pi(_mm_loadl_pi(_mm_setzero_ps(), (const __m64 *)(p + 8)), (const __m64 *)(p + 12));
	x1 = _mm_loadh_pi(_mm_loadl_pi(_mm_setzero_ps(), (const __m64 *)(p + 0)), (const __m64 *)(p + 4));
	x5 = x0;
	x0 = _mm_shuffle_ps(x0, x4, 0x88);
	x4 = _mm_shuffle_ps(x4, x5, 0xdd);
	x5 = _mm_mul_ps(x4, x0);
	x5 = _mm_shuffle_ps(x5, x5, 0xb1);
	x6 = x5;
	x5 = _mm_shuffle_ps(x5, x5, 0x4e);
	x7 = x3;
	x3 = _mm_shuffle_ps(x3, x1, 0xdd);
	x1 = _mm_shuffle_ps(x1, x7, 0x88);
	x7 = x3;
	x3 = _mm_mul_ps(x3, x5);
	x2 = _mm_setzero_ps();
	x5 = _mm_mul_ps(x5, x1);
	s30 = x5;
	x5 = x7;
	x7 = _mm_mul_ps(x7, x6);
	x6 = _mm_mul_ps(x6, x1);
	x3 = _mm_sub_ps(x3, x7);
	x7 = x5;
	x5 = _mm_mul_ps(x5, x0);
	x0 = _mm_shuffle_ps(x0, x0, 0x4e);
	x5 = _mm_shuffle_ps(x5, x5, 0xb1);
	s40 = x6;
	x6 = _mm_mul_ps(x4, x5);
	x6 = _mm_add_ps(x6, x3);
	x3 = x5;
	x5 = _mm_shuffle_ps(x5, x5, 0x4e);
	x3 = _mm_mul_ps(x3, x1);
	s50 = x3;
	x3 = _mm_mul_ps(x4, x5);
	x5 = _mm_mul_ps(x5, x1);
	x6 = _mm_sub_ps(x6, x3);
	x3 = x7;
	x7 = _mm_shuffle_ps(x7, x7, 0x4e);
	x7 = _mm_mul_ps(x7, x4);
	x7 = _mm_shuffle_ps(x7, x7, 0xb1);
	s60 = x5;
	x5 = x0;
	x0 = _mm_mul_ps(x0, x7);
	x0 = _mm_add_ps(x0, x6);
	x6 = x7;
	x7 = _mm_shuffle_ps(x7, x7, 0x4e);
	x6 = _mm_mul_ps(x6, x1);
	s70 = x4;
	x4 = x5;
	x5 = _mm_mul_ps(x5, x7);
	x7 = _mm_mul_ps(x7, x1);
	x0 = _mm_sub_ps(x0, x5);
	x7 = _mm_sub_ps(x7, x6);
	x5 = x1;
	x1 = _mm_mul_ps(x1, x0);
	x7 = _mm_shuffle_ps(x7, x7, 0x4e);
	x6 = x1;
	x1 = _mm_shuffle_ps(x1, x1, 0x4e);
	s20 = x0;
	x1 = _mm_add_ps(x1, x6);
	x0 = x1;
	x1 = _mm_shuffle_ps(x1, x1, 0xb1);
	x1 = _mm_add_ss(x1, x0);
	s10 = x1;
	x0 = x5;
	x5 = _mm_mul_ps(x5, x3);
	x5 = _mm_shuffle_ps(x5, x5, 0xb1);
	x6 = x5;
	x5 = _mm_shuffle_ps(x5, x5, 0x4e);
	s80 = x4;
	x4 = s70;
	s90 = x3;
	x3 = _mm_mul_ps(x4, x6);
	x3 = _mm_add_ps(x3, x7);
	x7 = _mm_mul_ps(x4, x5);
	x7 = _mm_sub_ps(x7, x3);
	x3 = _mm_mul_ps(x4, x0);
	x3 = _mm_shuffle_ps(x3, x3, 0xb1);
	sa0 = x5;
	x5 = s90;
	sb0 = x6;
	x6 = x5;
	x5 = _mm_mul_ps(x5, x3);
	x5 = _mm_add_ps(x5, x7);
	x7 = x3;
	x3 = _mm_shuffle_ps(x3, x3, 0x4e);
	sc0 = x4;
	x4 = _mm_mul_ps(x6, x3);
	sd0 = x6;
	x6 = _mm_sub_ps(x5, x4);
	x4 = _mm_sub_ps(s30, s40);
	x4 = _mm_shuffle_ps(x4, x4, 0x4e);
	x5 = s80;
	x7 = _mm_mul_ps(x7, x5);
	x3 = _mm_mul_ps(x3, x5);
	x4 = _mm_sub_ps(x4, x7);
	x0 = _mm_mul_ps(x0, x5);
	x4 = _mm_add_ps(x4, x3);
	x0 = _mm_shuffle_ps(x0, x0, 0xb1);
	x3 = x0;
	x0 = _mm_shuffle_ps(x0, x0, 0x4e);
	x7 = sc0;
	se0 = x6;
	x6 = _mm_mul_ps(x7, x0);
	x7 = _mm_mul_ps(x7, x3);
	x4 = _mm_add_ps(x4, x7);
	x4 = _mm_sub_ps(x4, x6);
	x6 = _mm_sub_ps(s60, s50);
	x6 = _mm_shuffle_ps(x6, x6, 0x4e);
	x7 = _mm_mul_ps(sb0, x5);
	x7 = _mm_sub_ps(x7, x6);
	x5 = _mm_mul_ps(x5, sa0);
	x7 = _mm_sub_ps(x7, x5);
	x5 = sd0;
	x3 = _mm_mul_ps(x3, x5);
	x5 = _mm_mul_ps(x5, x0);
	x7 = _mm_sub_ps(x7, x3);
	x2 = _mm_add_ps(x5, x7);
	x6 = se0;

	float d = _mm_cvtss_f32(s10);
	if (d == 0.0f || __builtin_isnan(d)) {
		return NULL;
	}
	if (det) {
		*det = d;
	}
	x0 = s10;
	x1 = s20;
	x3 = x0;
	x0 = _mm_rcp_ss(x0);
	x5 = x0;
	x0 = _mm_mul_ss(x0, x0);
	x3 = _mm_mul_ss(x3, x0);
	x5 = _mm_add_ss(x5, x5);
	x5 = _mm_sub_ss(x5, x3);
	x5 = _mm_shuffle_ps(x5, x5, 0);
	_mm_storeu_ps(out->m, _mm_mul_ps(x1, x5));
	_mm_storeu_ps(out->m + 4, _mm_mul_ps(x4, x5));
	_mm_storeu_ps(out->m + 8, _mm_mul_ps(x6, x5));
	_mm_storeu_ps(out->m + 12, _mm_mul_ps(x5, x2));
	return out;
}

static void D3dx_Rows(matrix_t *out, const float *r0, const float *r1, const float *r2) {
	memcpy(out->m, r0, 16);
	memcpy(out->m + 4, r1, 16);
	memcpy(out->m + 8, r2, 16);
	out->m[12] = 0.0f;
	out->m[13] = 0.0f;
	out->m[14] = 0.0f;
	out->m[15] = 1.0f;
}

/* rva 0x02bbb0 (SSE2) */
matrix_t *__stdcall D3DX_MatrixRotationX(matrix_t *out, float ang) {
	__m128 sc = D3dx_SinCosSS(_mm_set1_ps(ang));
	float c = Sse_Lane(sc, 0), s = Sse_Lane(sc, 1);
	const float r0[4] = { 1.0f, 0.0f, 0.0f, 0.0f }, r1[4] = { 0.0f, c, s, 0.0f }, r2[4] = { 0.0f, -s, c, 0.0f };

	D3dx_Rows(out, r0, r1, r2);
	return out;
}

/* rva 0x02bc30 (SSE2) */
matrix_t *__stdcall D3DX_MatrixRotationY(matrix_t *out, float ang) {
	__m128 sc = D3dx_SinCosSS(_mm_set1_ps(ang));
	float c = Sse_Lane(sc, 0), s = Sse_Lane(sc, 1);
	const float r0[4] = { c, 0.0f, -s, 0.0f }, r1[4] = { 0.0f, 1.0f, 0.0f, 0.0f }, r2[4] = { s, 0.0f, c, 0.0f };

	D3dx_Rows(out, r0, r1, r2);
	return out;
}

/* rva 0x00843a (x87): fsincos */
matrix_t *__stdcall D3DX_MatrixRotationZ(matrix_t *out, float ang) {
	float s, c;

	X87_SinCos(ang, &s, &c);
	const float r0[4] = { c, s, 0.0f, 0.0f }, r1[4] = { -s, c, 0.0f, 0.0f }, r2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	D3dx_Rows(out, r0, r1, r2);
	return out;
}

/* rva 0x02c3d0 (SSE2): the half angles (roll, pitch, yaw) through the packed sine and cosine */
quat_t *__stdcall D3DX_QuaternionRotationYPR(quat_t *out, float yaw, float pitch, float roll) {
	__m128 sn, cs;

	D3dx_SinCosPS(_mm_mul_ps(_mm_setr_ps(roll, pitch, yaw, 0.0f), _mm_set1_ps(0.5f)), &sn, &cs);
	float s[4], c[4];
	_mm_storeu_ps(s, sn);
	_mm_storeu_ps(c, cs);
	out->x = ((c[0] * c[2]) * s[1]) + ((s[0] * s[2]) * c[1]);
	/* the sum of a negated product (xorps), not a subtraction: the sign of a NaN result differs */
	out->y = ((c[0] * s[2]) * c[1]) + Float_Keep(-((s[0] * c[2]) * s[1]));
	out->z = ((s[0] * c[2]) * c[1]) + Float_Keep(-((c[0] * s[2]) * s[1]));
	out->w = ((c[0] * c[2]) * c[1]) + ((s[0] * s[2]) * s[1]);
	return out;
}

/* rva 0x0085f1 (x87): the factors 2x and 2y stored as floats, 2z kept on the FPU stack */
matrix_t *__stdcall D3DX_MatrixRotationQuaternion(matrix_t *out, const quat_t *q) {
	const long double two = 2.0f;
	float x2 = q->x * two;
	float y2 = q->y * two;
	long double z2 = two * q->z;
	long double xw = x2 * (long double)q->w;
	long double yw = y2 * (long double)q->w;
	long double zw = z2 * q->w;
	float xx = x2 * (long double)q->x;
	float xy = y2 * (long double)q->x;
	float xz = z2 * q->x;
	float yy = y2 * (long double)q->y;
	float yz = z2 * q->y;
	long double zz = z2 * q->z;
	long double xx1 = 1.0L - xx;
	float xx1f = xx1;

	out->m[0] = (1.0L - yy) - zz;
	out->m[1] = xy + zw;
	out->m[2] = xz - yw;
	out->m[3] = 0.0f;
	out->m[4] = xy - zw;
	out->m[5] = xx1 - zz;
	out->m[6] = yz + xw;
	out->m[7] = 0.0f;
	out->m[8] = xz + yw;
	out->m[9] = yz - xw;
	out->m[10] = xx1f - (long double)yy;
	out->m[11] = 0.0f;
	out->m[12] = 0.0f;
	out->m[13] = 0.0f;
	out->m[14] = 0.0f;
	out->m[15] = 1.0f;
	return out;
}

/* rva 0x00a780 (x87): the quaternion of the angles (passed on through the FPU) as a matrix */
matrix_t *__stdcall D3DX_MatrixRotationYPR(matrix_t *out, float yaw, float pitch, float roll) {
	quat_t q;

	D3DX_QuaternionRotationYPR(&q, X87_PassF(yaw), X87_PassF(pitch), X87_PassF(roll));
	D3DX_MatrixRotationQuaternion(out, &q);
	return out;
}
