/*
 * Exact conversions between decimal and binary doubles for the own printf, scanf and strtod of ScrapC.exe (stage 2b
 * of docs/standalone.md), written from scratch with a small fixed size big integer:
 *
 * - the digits of a double m * 2^e are those of the integer m * 2^e, or of m * 5^-e with the point moved e places
 * - a decimal string D * 10^k is D * 10^k when k >= 0, else the quotient D * 2^s / 10^-k with 64 bits kept and the
 *   remainder as sticky bit; either way it is rounded once, half even, so the result is the correctly rounded double
 *   or float
 *
 * No floating point arithmetic, so the x87 precision and the SSE state of the game do not change a result.
 */
#include <string.h>

#include "crtnum.h"

/* 5120 bits: the largest operands are about 3700 bits (769 digits against 10^1093) */
#define BIG_WORDS 160

typedef struct {
	int n;
	uint32_t w[BIG_WORDS];
} big_t;

static void Big_Set(big_t *b, uint64_t v) {
	b->w[0] = (uint32_t)v;
	b->w[1] = (uint32_t)(v >> 32);
	b->n = b->w[1] ? 2 : b->w[0] ? 1 : 0;
}

static void Big_Trim(big_t *b) {
	while (b->n && !b->w[b->n - 1]) {
		b->n--;
	}
}

/* b = b * m + add */
static void Big_MulAdd(big_t *b, uint32_t m, uint32_t add) {
	uint64_t carry = add;

	for (int i = 0; i < b->n; i++) {
		uint64_t t = (uint64_t)b->w[i] * m + carry;
		b->w[i] = (uint32_t)t;
		carry = t >> 32;
	}
	if (carry) {
		b->w[b->n++] = (uint32_t)carry;
	}
}

/* b *= base^e for base 5 or 10 */
static void Big_MulPow(big_t *b, uint32_t base, int e) {
	uint32_t step = base == 5 ? 1220703125u : 1000000000u;
	int stepExp = base == 5 ? 13 : 9;
	uint32_t m = 1;

	for (; e >= stepExp; e -= stepExp) {
		Big_MulAdd(b, step, 0);
	}
	while (e-- > 0) {
		m *= base;
	}
	if (m > 1) {
		Big_MulAdd(b, m, 0);
	}
}

/* b /= d, returns the remainder */
static uint32_t Big_DivSmall(big_t *b, uint32_t d) {
	uint32_t r = 0;

	for (int i = b->n - 1; i >= 0; i--) {
		uint32_t q;
#if defined(__i386__) || defined(__x86_64__)
		/* r < d, so the quotient fits */
		__asm__("divl %4" : "=a"(q), "=d"(r) : "a"(b->w[i]), "d"(r), "rm"(d));
#else
		uint64_t t = ((uint64_t)r << 32) | b->w[i];
		q = (uint32_t)(t / d);
		r = (uint32_t)(t % d);
#endif
		b->w[i] = q;
	}
	Big_Trim(b);
	return r;
}

static void Big_Shl(big_t *b, int bits) {
	int ws = bits / 32, bs = bits % 32;

	if (!b->n || bits <= 0) {
		return;
	}
	if (bs) {
		uint32_t carry = 0;
		for (int i = 0; i < b->n; i++) {
			uint32_t t = b->w[i];
			b->w[i] = (t << bs) | carry;
			carry = t >> (32 - bs);
		}
		if (carry) {
			b->w[b->n++] = carry;
		}
	}
	if (ws) {
		memmove(b->w + ws, b->w, (size_t)b->n * sizeof(b->w[0]));
		memset(b->w, 0, (size_t)ws * sizeof(b->w[0]));
		b->n += ws;
	}
}

static void Big_Shr1(big_t *b) {
	for (int i = 0; i < b->n; i++) {
		b->w[i] = (b->w[i] >> 1) | (i + 1 < b->n ? b->w[i + 1] << 31 : 0);
	}
	Big_Trim(b);
}

static int Big_BitLen(const big_t *b) {
	return b->n ? b->n * 32 - __builtin_clz(b->w[b->n - 1]) : 0;
}

static int Big_Cmp(const big_t *a, const big_t *b) {
	if (a->n != b->n) {
		return a->n < b->n ? -1 : 1;
	}
	for (int i = a->n - 1; i >= 0; i--) {
		if (a->w[i] != b->w[i]) {
			return a->w[i] < b->w[i] ? -1 : 1;
		}
	}
	return 0;
}

/* a -= b, a >= b */
static void Big_Sub(big_t *a, const big_t *b) {
	uint32_t borrow = 0;

	for (int i = 0; i < a->n; i++) {
		uint64_t t = (uint64_t)a->w[i] - (i < b->n ? b->w[i] : 0) - borrow;
		a->w[i] = (uint32_t)t;
		borrow = (uint32_t)(t >> 63);
	}
	Big_Trim(a);
}

/* the 64 bits of b from bit lo up, and whether a bit below lo is set */
static uint64_t Big_Bits(const big_t *b, int lo, int *sticky) {
	uint64_t r = 0;

	*sticky = 0;
	for (int i = 0; i < lo / 32 && i < b->n; i++) {
		*sticky |= b->w[i] != 0;
	}
	if (lo % 32 && lo / 32 < b->n) {
		*sticky |= (b->w[lo / 32] & ((1u << (lo % 32)) - 1)) != 0;
	}
	for (int k = 0; k < 3; k++) {
		int i = lo / 32 + k;
		uint64_t w = i < b->n ? b->w[i] : 0;
		int at = k * 32 - lo % 32;
		if (at < 0) {
			r |= w >> -at;
		} else if (at < 64) {
			r |= w << at;
		}
	}
	return r;
}

int Crt_DoubleDigits(double v, char *digits, int *decpt) {
	uint64_t bits;
	char tmp[CRT_DIGITS_MAX + 16];
	int pos = sizeof(tmp);
	big_t b;

	memcpy(&bits, &v, sizeof(bits));
	int be = (int)(bits >> 52) & 0x7ff;
	uint64_t m = bits & ((1ull << 52) - 1);
	int e = -1074;
	if (be) {
		m |= 1ull << 52;
		e = be - 1075;
	}
	*decpt = 0;
	if (!m) {
		return 0;
	}
	int tz = __builtin_ctzll(m);
	m >>= tz;
	e += tz;
	Big_Set(&b, m);
	if (e >= 0) {
		Big_Shl(&b, e);
	} else {
		Big_MulPow(&b, 5, -e);
	}
	while (b.n) {
		uint32_t r = Big_DivSmall(&b, 1000000000);
		for (int i = 0; i < 9; i++) {
			tmp[--pos] = (char)('0' + r % 10);
			r /= 10;
		}
	}
	while (tmp[pos] == '0') {
		pos++;
	}
	int n = (int)sizeof(tmp) - pos;
	*decpt = n + (e < 0 ? e : 0);
	while (tmp[pos + n - 1] == '0') {
		n--;
	}
	memcpy(digits, tmp + pos, (size_t)n);
	return n;
}

uint64_t Crt_RoundBits(uint64_t mant, int exp2, int sticky, int single, int *status) {
	/* precision, smallest normal exponent, largest exponent */
	int p = single ? 24 : 53;
	int emin = single ? -126 : -1022;
	int emax = single ? 127 : 1023;
	uint64_t inf = (uint64_t)(2 * emax + 1) << (p - 1);

	*status = 0;
	if (!mant) {
		*status = sticky ? CRT_NUM_INEXACT | CRT_NUM_TINY : CRT_NUM_TINY;
		return 0;
	}
	int lz = __builtin_clzll(mant);
	mant <<= lz;
	exp2 -= lz;
	/* mant has its top bit set, the value is in [2^e, 2^(e+1)) */
	int e = exp2 + 63;
	if (e > emax) {
		*status = CRT_NUM_OVERFLOW | CRT_NUM_INEXACT;
		return inf;
	}
	int shift = e >= emin ? 64 - p : 64 - p + (emin - e);
	uint64_t kept, rest, half;
	if (shift >= 64) {
		kept = 0;
		half = 1ull << 63;
		if (shift > 64) {
			rest = 1;
			half = 2;
			sticky = 1;
		} else {
			rest = mant;
		}
	} else {
		kept = mant >> shift;
		rest = mant & ((1ull << shift) - 1);
		half = 1ull << (shift - 1);
	}
	if (rest || sticky) {
		*status |= CRT_NUM_INEXACT;
	}
	if (rest > half || (rest == half && (sticky || (kept & 1)))) {
		kept++;
	}
	/* a carry into the next binade or from the denormals into the normals is the right bit pattern by itself */
	uint64_t bits = (e >= emin ? (uint64_t)(e - emin) << (p - 1) : 0) + kept;
	if (bits >= inf) {
		*status |= CRT_NUM_OVERFLOW;
		bits = inf;
	} else if (bits < 1ull << (p - 1)) {
		*status |= CRT_NUM_TINY;
	}
	return bits;
}

uint64_t Crt_DecimalBits(const char *digits, int n, int decpt, int sticky, int single, int *status) {
	big_t a, p;
	uint64_t mant = 0;
	int exp2, lost;

	if (n <= 0) {
		*status = CRT_NUM_TINY;
		return 0;
	}
	/* 0.d * 10^311 > DBL_MAX, 0.d * 10^-324 < 2^-1075 */
	if (decpt > 310) {
		return Crt_RoundBits(1, 1100, 0, single, status);
	}
	if (decpt < -324) {
		return Crt_RoundBits(0, 0, 1, single, status);
	}
	a.n = 0;
	for (int i = 0; i < n;) {
		uint32_t chunk = 0, scale = 1;
		for (int j = 0; j < 9 && i < n; j++, i++) {
			chunk = chunk * 10 + (uint32_t)(digits[i] - '0');
			scale *= 10;
		}
		if (!a.n) {
			Big_Set(&a, chunk);
		} else {
			Big_MulAdd(&a, scale, chunk);
		}
	}
	if (sticky) {
		/* a digit 1 after all digits a parse keeps: no tie, rounds like any small rest */
		Big_MulPow(&a, 10, CRT_PARSE_DIGITS - n);
		Big_MulAdd(&a, 10, 1);
		n = CRT_PARSE_DIGITS + 1;
	}
	int k = decpt - n;
	if (k >= 0) {
		Big_MulPow(&a, 10, k);
		int bl = Big_BitLen(&a);
		if (bl <= 64) {
			mant = Big_Bits(&a, 0, &lost);
			exp2 = 0;
		} else {
			mant = Big_Bits(&a, bl - 64, &lost);
			exp2 = bl - 64;
		}
		return Crt_RoundBits(mant, exp2, lost, single, status);
	}
	Big_Set(&p, 1);
	Big_MulPow(&p, 10, -k);
	/* a * 2^s / p has 63 or 64 bits */
	int s = 63 + Big_BitLen(&p) - Big_BitLen(&a);
	if (s >= 0) {
		Big_Shl(&a, s);
	} else {
		Big_Shl(&p, -s);
	}
	Big_Shl(&p, 63);
	for (int i = 63; i >= 0; i--) {
		if (Big_Cmp(&a, &p) >= 0) {
			Big_Sub(&a, &p);
			mant |= 1ull << i;
		}
		Big_Shr1(&p);
	}
	return Crt_RoundBits(mant, -s, a.n != 0, single, status);
}
