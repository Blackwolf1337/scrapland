/*
 * The MSVC math as C of its own (stage 2b of docs/standalone.md), the default of ScrapC.exe (src/standalone/crtmath.c
 * takes ucrtbase_ms.dll instead when it is there). The entries are own_<name> here. Written from the C standard, the
 * documented behavior and the observed output of ucrtbase 10.0.10586 (the static UCRT of the original), not from
 * Microsoft's code.
 *
 * What the runtime does and this file does too:
 * - sqrt is the x87 fsqrt in the precision of the caller's control word (24 bits in the game once Direct3D runs).
 * - atan2 and _CIatan2 are the x87 fpatan of |y| and |x| with 64 bit precision, pi minus that for a negative x, the
 *   sign of y; _CIatan2 returns the 80 bit value. fpatan is exact to the CPU, so the result is the original's on the
 *   same machine.
 * - _hypot is big * sqrt(1 + (small/big)^2) on the x87 with 64 bit precision, the square root rounded to a double.
 * - fmod is the x87 fprem, ldexp fscale.
 * - Those x87 functions round a result below DBL_MIN to 53 bits first and then cut it toward zero.
 * - The cdecl atan takes x87 paths for |x| < 2^-27 and |x| >= 1 / (the low part of pi/2) whose last operation rounds
 *   in the caller's precision; the SSE2 entry does not.
 * - sin and cos from 90112 on, tan from 12800 on, reduce with pi/2 of 66 bits (two pieces of 33 bits), so the result
 *   there is the function of that reduced argument. From 2^63 on the runtime's results are not a function of the
 *   argument that could be found; there this file takes the x87 remainder by its 64 bit pi/2.
 * The other functions give the correctly rounded result (double-double evaluation, error near 2^-100). The runtime's
 *   are within about 0.51 ulp (sinh and cosh 0.79) and not always correctly rounded; there the last bit differs.
 *
 * Domain and range errors go to the game's matherr (E3D_MathErr) with the type, name and arguments of the runtime,
 * and set errno like _set_errno_from_matherr when it returns 0. Underflow (a result below DBL_MIN of a nonzero value)
 * is reported with errno untouched.
 *
 * The tables are values of the functions rounded to double-double (MPFR, tools/standalone/libmtab.c).
 */
#include <stdint.h>
#include <string.h>

#include "scrap/crt.h"
#include "scrap/mathlib.h"

typedef struct {
	double hi, lo;
} dd_t;

static inline uint64_t Libm_Bits(double d) {
	uint64_t b;
	memcpy(&b, &d, 8);
	return b;
}

static inline double Libm_Double(uint64_t b) {
	double d;
	memcpy(&d, &b, 8);
	return d;
}

/* biased exponent */
static inline int Libm_Exp2(double d) {
	return (int)((Libm_Bits(d) >> 52) & 0x7ff);
}

static inline double Libm_Abs(double d) {
	return Libm_Double(Libm_Bits(d) & 0x7fffffffffffffffull);
}

static inline double Libm_SqrtSse(double x) {
	double r;
	__asm__("sqrtsd %1, %0" : "=x"(r) : "x"(x));
	return r;
}

/* round to an integer, ties to even */
static inline double Libm_Rint(double x) {
	double ax = Libm_Abs(x);
	if (!(ax < 0x1p52)) {
		return x;
	}
	double r = (ax + 0x1p52) - 0x1p52;
	return x < 0 ? -r : r;
}

static const double libm_inf = __builtin_inf();

/* ---- errors ---- */

enum { LIBM_DOMAIN = 1, LIBM_SING, LIBM_OVERFLOW, LIBM_UNDERFLOW };

static double Libm_Error(int type, const char *name, double a1, double a2, double ret) {
	crtMathExc_t e = { type, (char *)name, a1, a2, ret };

	if (!E3D_MathErr(&e)) {
		Crt_MathErrno(type);
	}
	return e.retval;
}

/* ---- double-double ---- */

static inline dd_t Dd_FastSum(double a, double b) {
	double s = a + b;
	return (dd_t){ s, b - (s - a) };
}

static inline dd_t Dd_Sum(double a, double b) {
	double s = a + b, bb = s - a;
	return (dd_t){ s, (a - (s - bb)) + (b - bb) };
}

static inline dd_t Dd_Prod(double a, double b) {
	double p = a * b;
	double ta = 134217729.0 * a, ah = ta - (ta - a), al = a - ah;
	double tb = 134217729.0 * b, bh = tb - (tb - b), bl = b - bh;
	return (dd_t){ p, ((ah * bh - p) + ah * bl + al * bh) + al * bl };
}

static inline dd_t Dd_Neg(dd_t a) {
	return (dd_t){ -a.hi, -a.lo };
}

static inline dd_t Dd_Add(dd_t a, dd_t b) {
	dd_t s = Dd_Sum(a.hi, b.hi), t = Dd_Sum(a.lo, b.lo);
	s = Dd_FastSum(s.hi, s.lo + t.hi);
	return Dd_FastSum(s.hi, s.lo + t.lo);
}

static inline dd_t Dd_AddD(dd_t a, double b) {
	dd_t s = Dd_Sum(a.hi, b);
	return Dd_FastSum(s.hi, s.lo + a.lo);
}

static inline dd_t Dd_Mul(dd_t a, dd_t b) {
	dd_t p = Dd_Prod(a.hi, b.hi);
	return Dd_FastSum(p.hi, p.lo + (a.hi * b.lo + a.lo * b.hi));
}

static inline dd_t Dd_MulD(dd_t a, double b) {
	dd_t p = Dd_Prod(a.hi, b);
	return Dd_FastSum(p.hi, p.lo + a.lo * b);
}

static dd_t Dd_Div(dd_t a, dd_t b) {
	double q1 = a.hi / b.hi;
	dd_t r = Dd_Add(a, Dd_Neg(Dd_MulD(b, q1)));
	double q2 = r.hi / b.hi;
	r = Dd_Add(r, Dd_Neg(Dd_MulD(b, q2)));
	return Dd_AddD(Dd_FastSum(q1, q2), r.hi / b.hi);
}

/* a > 0 */
static dd_t Dd_Sqrt(dd_t a) {
	double s = Libm_SqrtSse(a.hi);
	dd_t r = Dd_Add(a, Dd_Neg(Dd_Prod(s, s)));
	return Dd_FastSum(s, r.hi / (2.0 * s));
}

/* c[0] + x * (c[1] + x * (... c[n - 1])), the first nd coefficients in double-double, the small tail in double */
static dd_t Dd_Poly(const dd_t *c, int n, int nd, dd_t x) {
	double t = c[n - 1].hi;
	for (int i = n - 2; i >= nd; i--) {
		t = c[i].hi + x.hi * t;
	}
	dd_t p = { t, 0.0 };
	for (int i = nd - 1; i >= 0; i--) {
		p = Dd_Add(c[i], Dd_Mul(x, p));
	}
	return p;
}

/*
 * (y.hi + y.lo) * 2^e rounded to nearest even, also when the result is subnormal (y normalized, y.hi != 0). *tiny is
 * set when the result is below DBL_MIN.
 */
static double Dd_Scale(dd_t y, int e, int *tiny) {
	double h = y.hi + y.lo;
	int er = Libm_Exp2(h) + e;

	*tiny = 0;
	if (er >= 0x7ff) {
		return h > 0 ? libm_inf : -libm_inf;
	}
	if (er > 0) {
		return Libm_Double(Libm_Bits(h) + ((uint64_t)(int64_t)e << 52));
	}
	*tiny = 1;
	double sign = h < 0 ? -1.0 : 1.0;
	int s = e + 1074;
	if (s < -64) {
		return sign * 0.0;
	}
	/* in units of the smallest subnormal: a + b, a with the exponent of y.hi moved by s (exact) */
	double a = Libm_Double(Libm_Bits(Libm_Abs(y.hi)) + ((uint64_t)(int64_t)s << 52));
	double b = sign * Libm_Double(Libm_Bits(y.lo) + ((uint64_t)(int64_t)s << 52));
	if (y.lo == 0.0) {
		b = 0.0;
	}
	double n = Libm_Rint(a);
	if (n > a) {
		n -= 1.0;
	}
	double f = a - n;
	if (f > 0.5 || (f == 0.5 && (b > 0.0 || (b == 0.0 && ((uint64_t)n & 1))))) {
		n += 1.0;
	}
	return sign * (n * 0x1p-1074);
}

#include "crtlibmtab.h"

/* ---- exp and log kernels ---- */

/* exp(x) = y * 2^*e with y in [0.7, 1.5], |x.hi| < 1500 */
static dd_t Libm_ExpDd(dd_t x, int *e) {
	double kd = Libm_Rint(x.hi * LIBM_INV_LN2_128);
	int k = (int)kd;
	dd_t r = Dd_Sum(x.hi - kd * LIBM_LN2_128_1, x.lo);
	r = Dd_Add(r, Dd_Neg(Dd_Prod(kd, LIBM_LN2_128_2)));
	r = Dd_AddD(r, -kd * LIBM_LN2_128_3);
	/* expm1(r) = r + r^2 * (1/2! + r/3! + ... + r^8/10!) */
	dd_t q = Dd_Poly(libm_expcoef, 9, 4, r);
	dd_t em1 = Dd_Add(r, Dd_Mul(Dd_Mul(r, r), q));
	dd_t t = libm_exp2tab[k & 127];
	*e = k >> 7;
	return Dd_Add(t, Dd_Mul(t, em1));
}

/* log(x) for a finite x > 0 */
static dd_t Libm_LogDd(double x) {
	int e = 0;
	if (Libm_Exp2(x) == 0) {
		x *= 0x1p54;
		e = -54;
	}
	uint64_t b = Libm_Bits(x);
	e += (int)(b >> 52) - 1023;
	/* m in [0.75, 1.5) */
	double m = Libm_Double((b & 0x000fffffffffffffull) | 0x3ff0000000000000ull);
	if (m >= 1.5) {
		m *= 0.5;
		e++;
	}
	int i = (int)Libm_Rint((m - 0.75) * 128.0);
	/* z = m / c - 1 with the reciprocal of the table, exact */
	dd_t p = Dd_Prod(m, libm_logtab[i].inv);
	dd_t z = Dd_FastSum(p.hi - 1.0, p.lo);
	/* log1p(z) = z + z^2 * (-1/2 + z/3 - z^2/4 ...) */
	dd_t q = Dd_Poly(libm_log1pcoef, 12, 5, z);
	dd_t l = Dd_Add(z, Dd_Mul(Dd_Mul(z, z), q));
	l = Dd_Add(l, libm_logtab[i].logc);
	if (e) {
		l = Dd_Add(Dd_MulD(LIBM_LN2, (double)e), l);
	}
	return l;
}

/* ---- sin and cos kernel ---- */

/* sin and cos of r, |r| <= 0.8 */
static void Libm_SinCosDd(dd_t r, dd_t *s, dd_t *c) {
	int j = (int)Libm_Rint(r.hi * 64.0);
	int aj = j < 0 ? -j : j;
	dd_t d = Dd_FastSum(r.hi - (double)j * 0.015625, r.lo);
	dd_t z = Dd_Mul(d, d);
	/* sin(d) = d + d^3 * S(d^2), cos(d) = 1 + d^2 * C(d^2) */
	dd_t sd = Dd_Add(d, Dd_Mul(Dd_Mul(d, z), Dd_Poly(libm_sincoef, 5, 2, z)));
	dd_t cm = Dd_Mul(z, Dd_Poly(libm_coscoef, 6, 3, z));
	dd_t sj = libm_sincostab[aj][0], cj = libm_sincostab[aj][1];
	if (j < 0) {
		sj = Dd_Neg(sj);
	}
	/* sin(cj + d) = sj + (sj * cm + cj * sd), cos(cj + d) = cj + (cj * cm - sj * sd) */
	*s = Dd_Add(sj, Dd_Add(Dd_Mul(sj, cm), Dd_Mul(cj, sd)));
	*c = Dd_Add(cj, Dd_Add(Dd_Mul(cj, cm), Dd_Neg(Dd_Mul(sj, sd))));
}

/* ---- atan kernel ---- */

/* atan(t), t >= 0 */
static dd_t Libm_AtanPos(dd_t t) {
	int inv = t.hi > 1.0;
	if (inv) {
		t = Dd_Div((dd_t){ 1.0, 0.0 }, t);
	}
	int j = (int)Libm_Rint(t.hi * 64.0);
	double c = (double)j * 0.015625;
	/* d = (t - c) / (1 + t c) */
	dd_t num = Dd_FastSum(t.hi - c, t.lo);
	dd_t d = j ? Dd_Div(num, Dd_AddD(Dd_MulD(t, c), 1.0)) : t;
	dd_t z = Dd_Mul(d, d);
	dd_t a = Dd_Add(d, Dd_Mul(Dd_Mul(d, z), Dd_Poly(libm_atancoef, 7, 3, z)));
	a = Dd_Add(libm_atantab[j], a);
	return inv ? Dd_Add(LIBM_PIO2, Dd_Neg(a)) : a;
}

static dd_t Libm_AtanDd(dd_t t) {
	return t.hi < 0 ? Dd_Neg(Libm_AtanPos(Dd_Neg(t))) : Libm_AtanPos(t);
}

/* a double-double to a double, no under- or overflow */
static inline double Dd_Round(dd_t a) {
	return a.hi + a.lo;
}

/* ---- helpers of the functions ---- */

static inline int Libm_IsNan(double x) {
	return (Libm_Bits(x) & 0x7fffffffffffffffull) > 0x7ff0000000000000ull;
}

static inline int Libm_Sign(double x) {
	return (int)(Libm_Bits(x) >> 63);
}

/* a NaN comes back quiet with its payload */
static inline double Libm_Quiet(double x) {
	return Libm_Double(Libm_Bits(x) | 0x0008000000000000ull);
}

/*
 * The NaN that sinh, cosh, tanh and the cdecl atan2 and fmod return for a NaN (observed): quiet, the low word lo of
 * the bits becomes (lo & 0xffe00000) + (lo >> 11)
 */
static double Libm_NanLow(double x) {
	uint64_t b = Libm_Bits(x) | 0x0008000000000000ull;
	uint32_t lo = (uint32_t)b;
	return Libm_Double((b & 0xffffffff00000000ull) | ((lo & 0xffe00000u) + (lo >> 11)));
}

/* of two NaNs the one with the larger quiet significand, the positive one when they are equal */
static double Libm_NanLarger(double a, double b) {
	uint64_t ma = Libm_Bits(a) << 12 | 1ull << 63, mb = Libm_Bits(b) << 12 | 1ull << 63;
	if (ma != mb) {
		return ma > mb ? a : b;
	}
	return Libm_Sign(a) ? b : a;
}

/*
 * A NaN argument of the cdecl atan2 and fmod: the NaN of an x87 operation on both (a signaling and a quiet one give
 * the quiet one, else the larger significand), changed like Libm_NanLow; two NaNs with a signaling one are a domain
 * error.
 */
static double Libm_NanPair(const char *name, double a, double b) {
	double r;
	if (Libm_IsNan(a) && Libm_IsNan(b)) {
		uint64_t ma = Libm_Bits(a) & 0x000fffffffffffffull, mb = Libm_Bits(b) & 0x000fffffffffffffull;
		int sa = !(ma & 0x0008000000000000ull), sb = !(mb & 0x0008000000000000ull);
		if (sa != sb) {
			r = sa ? b : a;
		} else {
			r = Libm_NanLarger(a, b);
		}
		r = Libm_NanLow(r);
		if (sa || sb) {
			return Libm_Error(LIBM_DOMAIN, name, a, b, r);
		}
		return r;
	}
	return Libm_NanLow(Libm_IsNan(a) ? a : b);
}

/*
 * An x87 result to a double. Below DBL_MIN the x87 functions of the runtime round to 53 bits (with the wide exponent)
 * and then cut toward zero.
 */
static double Libm_Chop(long double a) {
	unsigned short cw, cw53;
	__asm__ volatile("fnstcw %0" : "=m"(cw));
	cw53 = (cw & ~0x300) | 0x200;
	__asm__ volatile("fldcw %1\n\tfld1\n\tfmulp\n\tfldcw %2" : "+t"(a) : "m"(cw53), "m"(cw));
	double d = (double)a;
	if (Libm_Abs(d) <= 0x1p-1022 && d != 0.0 && (d < 0 ? (long double)d < a : (long double)d > a)) {
		d = Libm_Double(Libm_Bits(d) - 1);
	}
	return d;
}

/* the NaN of an invalid operation (-nan(ind)) */
#define LIBM_IND Libm_Double(0xfff8000000000000ull)

/* a double-double result to a double with the overflow and underflow reports of the runtime */
static double Libm_Result(dd_t y, int e, const char *name, double a1, double a2) {
	int tiny;
	double r = Dd_Scale(y, e, &tiny);
	if (Libm_Abs(r) == libm_inf) {
		return Libm_Error(LIBM_OVERFLOW, name, a1, a2, r);
	}
	if (tiny) {
		return Libm_Error(LIBM_UNDERFLOW, name, a1, a2, r);
	}
	return r;
}

/* ---- exp, log, log10, pow ---- */

double own_exp(double x) {
	double ax = Libm_Abs(x);
	if (!(ax < libm_inf)) {
		if (Libm_IsNan(x)) {
			return Libm_Quiet(x);
		}
		return x > 0 ? x : 0.0;
	}
	if (ax < 0x1p-54) {
		return 1.0 + x;
	}
	if (x > 710.0) {
		return Libm_Error(LIBM_OVERFLOW, "exp", x, x, libm_inf);
	}
	if (x < -746.0) {
		return Libm_Error(LIBM_UNDERFLOW, "exp", x, x, 0.0);
	}
	int e;
	dd_t y = Libm_ExpDd((dd_t){ x, 0.0 }, &e);
	return Libm_Result(y, e, "exp", x, x);
}

/* log and log10: zero is a singularity (-inf), a negative argument a domain error */
static double Libm_LogCommon(double x, const char *name, int ten) {
	if (Libm_IsNan(x)) {
		return Libm_Quiet(x);
	}
	if (x == 0.0) {
		return Libm_Error(LIBM_SING, name, x, x, -libm_inf);
	}
	if (x < 0.0) {
		return Libm_Error(LIBM_DOMAIN, name, x, x, LIBM_IND);
	}
	if (x == libm_inf) {
		return x;
	}
	dd_t l = Libm_LogDd(x);
	if (ten) {
		l = Dd_Mul(l, LIBM_INV_LN10);
	}
	return Dd_Round(l);
}

double own_log(double x) {
	return Libm_LogCommon(x, "log", 0);
}

double own_log10(double x) {
	return Libm_LogCommon(x, "log10", 1);
}

/* 0 for no integer, 1 for an odd and 2 for an even integer */
static int Libm_IntKind(double y) {
	double ay = Libm_Abs(y);
	if (ay >= 0x1p53) {
		return ay < libm_inf ? 2 : 0;
	}
	if (Libm_Rint(ay) != ay) {
		return 0;
	}
	return Libm_Rint(ay * 0.5) == ay * 0.5 ? 2 : 1;
}

double own_pow(double x, double y) {
	double ax = Libm_Abs(x), ay = Libm_Abs(y);
	if (y == 0.0) {
		return 1.0;
	}
	if (Libm_IsNan(x) || Libm_IsNan(y)) {
		if (x == 1.0) {
			return 1.0;
		}
		return Libm_IsNan(x) ? Libm_Quiet(x) : Libm_Quiet(y);
	}
	int ik = Libm_IntKind(y);
	int neg = Libm_Sign(x) && ik == 1;
	if (ay == libm_inf) {
		if (ax == 1.0) {
			return 1.0;
		}
		return (ax < 1.0) == (y < 0) ? libm_inf : 0.0;
	}
	if (ax == libm_inf) {
		double r = y > 0 ? libm_inf : 0.0;
		return neg ? -r : r;
	}
	if (x == 0.0) {
		if (y < 0) {
			return Libm_Error(LIBM_SING, "pow", x, y, neg ? -libm_inf : libm_inf);
		}
		return neg ? -0.0 : 0.0;
	}
	if (x < 0 && !ik) {
		return Libm_Error(LIBM_DOMAIN, "pow", x, y, LIBM_IND);
	}
	if (ax == 1.0) {
		return neg ? -1.0 : 1.0;
	}
	dd_t l = Libm_LogDd(ax);
	double est = l.hi * y;
	if (est > 710.0) {
		return Libm_Error(LIBM_OVERFLOW, "pow", x, y, neg ? -libm_inf : libm_inf);
	}
	if (est < -746.0) {
		return Libm_Error(LIBM_UNDERFLOW, "pow", x, y, neg ? -0.0 : 0.0);
	}
	/* a huge y would overflow the splitting of the exact product */
	double ys = y;
	if (ay > 0x1p900) {
		l = (dd_t){ l.hi * 0x1p100, l.lo * 0x1p100 };
		ys *= 0x1p-100;
	}
	int e;
	dd_t r = Libm_ExpDd(Dd_MulD(l, ys), &e);
	return Libm_Result(neg ? Dd_Neg(r) : r, e, "pow", x, y);
}

/* ---- sin, cos, tan ---- */

/*
 * x = k * pi/2 + r with |r| about pi/4 at most; returns k mod 4. Below medium (sin and cos 90112, tan 12800) the
 * reduction is exact, from there on it takes pi/2 of 66 bits like the runtime.
 */
static int Libm_Reduce(double x, double medium, dd_t *r) {
	double ax = Libm_Abs(x);
	if (ax <= 0x1.921fb54442d18p-1) {
		*r = (dd_t){ x, 0.0 };
		return 0;
	}
	double kd = Libm_Rint(x * LIBM_2_PI);
	if (ax < medium) {
		dd_t t = Dd_Sum(x - kd * LIBM_PIO2_1, -kd * LIBM_PIO2_2);
		t = Dd_AddD(t, -kd * LIBM_PIO2_3);
		t = Dd_AddD(t, -kd * LIBM_PIO2_4);
		*r = Dd_Add(t, Dd_Neg(Dd_Prod(kd, LIBM_PIO2_5)));
		return (int)kd & 3;
	}
	if (ax < 0x1p63) {
		/* x - k * (P1 + P2) exactly, P1 + P2 the 66 bit pi/2 (2.02e-21 below it); k again when the product of
		 * x and 2/pi was off by a unit or more */
		int64_t k = (int64_t)kd;
		for (;;) {
			/* k in two parts that are exact doubles */
			double kh = (double)(k & ~(int64_t)0x1fffff), kl = (double)(k & 0x1fffff);
			dd_t t = { x, 0.0 };
			t = Dd_Add(t, Dd_Neg(Dd_Prod(kh, LIBM_PIO2_1)));
			t = Dd_Add(t, Dd_Neg(Dd_Prod(kl, LIBM_PIO2_1)));
			t = Dd_Add(t, Dd_Neg(Dd_Prod(kh, LIBM_PIO2_2)));
			*r = Dd_Add(t, Dd_Neg(Dd_Prod(kl, LIBM_PIO2_2)));
			if (Libm_Abs(r->hi) < 0.79) {
				return (int)(k & 3);
			}
			k += (int64_t)Libm_Rint(r->hi * LIBM_2_PI);
		}
	}
	/* the runtime's results are not a function of x here; the x87 remainder by its pi/2 */
	static const long double pio2 = 0xc.90fdaa22168c235p-3L;
	long double rem;
	unsigned short sw;
	__asm__ volatile("1:\n\tfprem1\n\tfnstsw %%ax\n\ttestb $4, %%ah\n\tjnz 1b\n\tmovw %%ax, %1" : "=t"(rem), "=m"(sw)
		: "0"((long double)x), "u"(pio2) : "eax", "cc");
	double hi = (double)rem;
	*r = (dd_t){ hi, (double)(rem - hi) };
	/* the low bits of the quotient's magnitude: C3 bit 1, C1 bit 0 */
	int q = (sw >> 14 & 1) << 1 | (sw >> 9 & 1);
	return x < 0 ? -q & 3 : q;
}

/* which = 0 sin, 1 cos, 2 tan */
static double Libm_Trig(double x, int which, const char *name) {
	if (!(Libm_Abs(x) < libm_inf)) {
		if (Libm_IsNan(x)) {
			return Libm_Quiet(x);
		}
		return Libm_Error(LIBM_DOMAIN, name, x, 0.0, LIBM_IND);
	}
	if (Libm_Abs(x) < 0x1p-27 && which != 1) {
		/* sin(x) and tan(x) round to x */
		return x;
	}
	dd_t r, s, c;
	int q = Libm_Reduce(x, which == 2 ? 12800.0 : 90112.0, &r);
	Libm_SinCosDd(r, &s, &c);
	switch (which) {
	case 0:
		return Dd_Round(q == 0 ? s : q == 1 ? c : q == 2 ? Dd_Neg(s) : Dd_Neg(c));
	case 1:
		return Dd_Round(q == 0 ? c : q == 1 ? Dd_Neg(s) : q == 2 ? Dd_Neg(c) : s);
	default:
		return Dd_Round(q & 1 ? Dd_Neg(Dd_Div(c, s)) : Dd_Div(s, c));
	}
}

double own_sin(double x) {
	return Libm_Trig(x, 0, "sin");
}

double own_cos(double x) {
	return Libm_Trig(x, 1, "cos");
}

double own_tan(double x) {
	return Libm_Trig(x, 2, "tan");
}

/* ---- atan, asin, acos ---- */

/* the atan of the SSE2 code */
double Libm_Atan(double x) {
	if (Libm_IsNan(x)) {
		return Libm_Quiet(x);
	}
	if (Libm_Abs(x) < 0x1p-27) {
		return x;
	}
	if (Libm_Abs(x) > 0x1p60) {
		return Libm_Sign(x) ? -LIBM_PIO2.hi : LIBM_PIO2.hi;
	}
	return Dd_Round(Libm_AtanDd((dd_t){ x, 0.0 }));
}

/*
 * x rounded to the caller's x87 precision: a subnormal x to nearest even, a normal one as x plus a positive amount
 * below every rounding step (a tie goes up)
 */
static double Libm_X87Tiny(double x) {
	long double c = Libm_Exp2(x) ? 0x1p-16000L : 0.0L;
	double d;
	__asm__ volatile("fldl %1\n\tfldt %2\n\tfaddp\n\tfstpl %0" : "=m"(d) : "m"(x), "m"(c) : "st");
	return d;
}

/* pi/2 (high + low part) added in the caller's x87 precision */
static double Libm_X87Pio2(int neg) {
	static const dd_t pio2 = LIBM_PIO2;
	double d;
	__asm__ volatile("fldl %1\n\tfaddl %2\n\tfstpl %0" : "=m"(d) : "m"(pio2.hi), "m"(pio2.lo) : "st");
	return neg ? -d : d;
}

/* the cdecl atan: x87 paths for tiny arguments and those above 1 / (the low part of pi/2) */
double own_atan(double x) {
	double ax = Libm_Abs(x);
	if (ax < 0x1p-27) {
		return x == 0.0 ? x : Libm_X87Tiny(x);
	}
	if (ax >= 0x1.d02967c31cdb5p+53 && !Libm_IsNan(x)) {
		return Libm_X87Pio2(Libm_Sign(x));
	}
	return Libm_Atan(x);
}

/* sqrt(1 - x^2) for |x| <= 1 */
static dd_t Libm_Cofactor(double x) {
	double ax = Libm_Abs(x);
	dd_t w;
	if (ax < 0.5) {
		w = Dd_AddD(Dd_Neg(Dd_Prod(x, x)), 1.0);
	} else {
		w = Dd_MulD(Dd_Sum(1.0, ax), 1.0 - ax);
	}
	return w.hi == 0.0 ? w : Dd_Sqrt(w);
}

double own_asin(double x) {
	double ax = Libm_Abs(x);
	if (Libm_IsNan(x)) {
		return Libm_Quiet(x);
	}
	if (!(ax <= 1.0)) {
		return Libm_Error(LIBM_DOMAIN, "asin", x, x, LIBM_IND);
	}
	if (ax < 0x1p-27) {
		return x;
	}
	if (ax == 1.0) {
		return x * LIBM_PIO2.hi;
	}
	dd_t s = Libm_Cofactor(x), a;
	if (ax <= 0.7) {
		a = Libm_AtanDd(Dd_Div((dd_t){ x, 0.0 }, s));
	} else {
		a = Dd_Add(LIBM_PIO2, Dd_Neg(Libm_AtanPos(Dd_Div(s, (dd_t){ ax, 0.0 }))));
		if (x < 0) {
			a = Dd_Neg(a);
		}
	}
	return Dd_Round(a);
}

double own_acos(double x) {
	double ax = Libm_Abs(x);
	if (Libm_IsNan(x)) {
		return Libm_Quiet(x);
	}
	if (!(ax <= 1.0)) {
		return Libm_Error(LIBM_DOMAIN, "acos", x, x, LIBM_IND);
	}
	if (x == 1.0) {
		return 0.0;
	}
	if (ax < 0x1p-60) {
		return LIBM_PIO2.hi;
	}
	dd_t s = Libm_Cofactor(x), a;
	/* acos(|x|) = atan(s / |x|), pi minus that for a negative x */
	if (s.hi <= ax) {
		a = Libm_AtanPos(Dd_Div(s, (dd_t){ ax, 0.0 }));
	} else {
		a = Dd_Add(LIBM_PIO2, Dd_Neg(Libm_AtanPos(Dd_Div((dd_t){ ax, 0.0 }, s))));
	}
	if (x < 0) {
		a = Dd_Add(LIBM_PI, Dd_Neg(a));
	}
	return Dd_Round(a);
}

/* ---- atan2: the x87 fpatan ---- */

/* fpatan(|y|, |x|) with 64 bit precision, pi minus that for a negative x, the sign of y */
static long double Libm_Atan2X(double y, double x) {
	static const long double pi = 0xc.90fdaa22168c235p-2L;
	unsigned short cw, cw64;
	long double a;
	__asm__ volatile("fnstcw %0" : "=m"(cw));
	cw64 = cw | 0x300;
	__asm__ volatile("fldcw %3\n\tfpatan" : "=t"(a) : "0"((long double)Libm_Abs(x)), "u"((long double)Libm_Abs(y)),
		"m"(cw64) : "st(1)");
	if (Libm_Sign(x)) {
		a = pi - a;
	}
	__asm__ volatile("fldcw %1" : "+t"(a) : "m"(cw));
	return Libm_Sign(y) ? -a : a;
}

long double Libm_CIAtan2(double y, double x) {
	if (Libm_IsNan(x) || Libm_IsNan(y)) {
		/* the x87 picks the NaN (the one with the larger significand) */
		long double a;
		__asm__("fpatan" : "=t"(a) : "0"((long double)x), "u"((long double)y) : "st(1)");
		return a;
	}
	long double a = Libm_Atan2X(y, x);
	double d = Libm_Chop(a);
	if (Libm_Abs(d) < 0x1p-1022 && a != 0.0L) {
		return Libm_Error(LIBM_UNDERFLOW, "atan2", y, x, d);
	}
	return a;
}

double own_atan2(double y, double x) {
	if (Libm_IsNan(x) || Libm_IsNan(y)) {
		return Libm_NanPair("atan2", y, x);
	}
	return (double)Libm_CIAtan2(y, x);
}

/* ---- sinh, cosh, tanh ---- */

/* 1/y * 2^(-2e): exp(-|x|) on the scale of exp(|x|) = y * 2^e */
static dd_t Libm_HypInv(dd_t y, int e) {
	dd_t inv = Dd_Div((dd_t){ 1.0, 0.0 }, y);
	double s = Libm_Double((uint64_t)(1023 - 2 * e) << 52);
	return (dd_t){ inv.hi * s, inv.lo * s };
}

double own_sinh(double x) {
	double ax = Libm_Abs(x);
	if (!(ax < libm_inf)) {
		return Libm_IsNan(x) ? Libm_NanLow(x) : x;
	}
	if (ax < 0x1p-27) {
		return x;
	}
	if (ax >= 711.0) {
		return Libm_Error(LIBM_OVERFLOW, "sinh", x, 0.0, x > 0 ? libm_inf : -libm_inf);
	}
	dd_t r;
	int e = 0;
	if (ax < 0.25) {
		dd_t xx = { ax, 0.0 }, z = Dd_Mul(xx, xx);
		r = Dd_Add(xx, Dd_Mul(Dd_Mul(xx, z), Dd_Poly(libm_sinhcoef, 10, 5, z)));
	} else {
		/* (exp(|x|) - exp(-|x|)) / 2 */
		dd_t y = Libm_ExpDd((dd_t){ ax, 0.0 }, &e);
		r = ax < 40.0 ? Dd_Add(y, Dd_Neg(Libm_HypInv(y, e))) : y;
		e--;
	}
	int tiny;
	double d = Dd_Scale(x < 0 ? Dd_Neg(r) : r, e, &tiny);
	if (Libm_Abs(d) == libm_inf) {
		return Libm_Error(LIBM_OVERFLOW, "sinh", x, 0.0, d);
	}
	return d;
}

double own_cosh(double x) {
	double ax = Libm_Abs(x);
	if (!(ax < libm_inf)) {
		return Libm_IsNan(x) ? Libm_NanLow(x) : ax;
	}
	if (ax < 0x1p-27) {
		return 1.0;
	}
	if (ax >= 711.0) {
		return Libm_Error(LIBM_OVERFLOW, "cosh", x, 0.0, libm_inf);
	}
	dd_t r;
	int e = 0;
	if (ax < 0.25) {
		dd_t z = Dd_Prod(ax, ax);
		r = Dd_AddD(Dd_Mul(z, Dd_Poly(libm_coshcoef, 10, 5, z)), 1.0);
	} else {
		dd_t y = Libm_ExpDd((dd_t){ ax, 0.0 }, &e);
		r = ax < 40.0 ? Dd_Add(y, Libm_HypInv(y, e)) : y;
		e--;
	}
	int tiny;
	double d = Dd_Scale(r, e, &tiny);
	if (d == libm_inf) {
		return Libm_Error(LIBM_OVERFLOW, "cosh", x, 0.0, d);
	}
	return d;
}

double own_tanh(double x) {
	double ax = Libm_Abs(x);
	if (Libm_IsNan(x)) {
		return Libm_NanLow(x);
	}
	if (ax < 0x1p-27) {
		return x;
	}
	if (ax > 23.0) {
		return x > 0 ? 1.0 : -1.0;
	}
	dd_t t;
	if (ax < 0.25) {
		dd_t xx = { ax, 0.0 }, z = Dd_Mul(xx, xx);
		dd_t sh = Dd_Add(xx, Dd_Mul(Dd_Mul(xx, z), Dd_Poly(libm_sinhcoef, 10, 5, z)));
		dd_t ch = Dd_AddD(Dd_Mul(z, Dd_Poly(libm_coshcoef, 10, 5, z)), 1.0);
		t = Dd_Div(sh, ch);
	} else {
		/* (E - 1) / (E + 1), E = exp(2|x|) */
		int e;
		dd_t y = Libm_ExpDd((dd_t){ 2.0 * ax, 0.0 }, &e);
		double s = Libm_Double((uint64_t)(1023 + e) << 52);
		dd_t big = { y.hi * s, y.lo * s };
		t = Dd_Div(Dd_AddD(big, -1.0), Dd_AddD(big, 1.0));
	}
	double d = Dd_Round(t);
	return x < 0 ? -d : d;
}

/* ---- sqrt and hypot: the x87 ---- */

/* fsqrt in the caller's precision */
double own_sqrt(double x) {
	if (Libm_IsNan(x)) {
		/* the runtime calls matherr with type 0 for a NaN */
		return Libm_Error(0, "sqrt", x, 0.0, Libm_Quiet(x));
	}
	if (x < 0.0) {
		return Libm_Error(LIBM_DOMAIN, "sqrt", x, 0.0, LIBM_IND);
	}
	double r;
	__asm__ volatile("fldl %1\n\tfsqrt\n\tfstpl %0" : "=m"(r) : "m"(x) : "st");
	return r;
}

/* big * sqrt(1 + (small/big)^2) with 64 bit precision, the square root rounded to a double */
static long double Libm_HypotX87(double big, double small) {
	unsigned short cw, cw64;
	double s;
	long double r;
	__asm__ volatile(
		"fnstcw %[cw]\n\t"
		"movzwl %[cw], %%eax\n\t"
		"orl $0x300, %%eax\n\t"
		"movw %%ax, %[cw64]\n\t"
		"fldcw %[cw64]\n\t"
		"fldl %[sm]\n\t"
		"fdivl %[bg]\n\t"
		"fmul %%st(0), %%st(0)\n\t"
		"fld1\n\t"
		"faddp\n\t"
		"fsqrt\n\t"
		"fstpl %[s]\n\t"
		"fldl %[s]\n\t"
		"fmull %[bg]\n\t"
		"fldcw %[cw]\n\t"
		: "=t"(r), [s] "=m"(s), [cw] "=m"(cw), [cw64] "=m"(cw64)
		: [sm] "m"(small), [bg] "m"(big)
		: "eax", "cc");
	return r;
}

/* a / b with 64 bit precision, rounded to a double */
static double Libm_DivX87(double a, double b) {
	unsigned short cw, cw64;
	double r;
	__asm__ volatile("fnstcw %0" : "=m"(cw));
	cw64 = cw | 0x300;
	__asm__ volatile("fldcw %1\n\tfldl %2\n\tfdivl %3\n\tfstpl %0\n\tfldcw %4" : "=m"(r) : "m"(cw64), "m"(a), "m"(b),
		"m"(cw) : "st");
	return r;
}

/* the matherr arguments are x and y divided by the larger of them */
double own_hypot(double x, double y) {
	double ax = Libm_Abs(x), ay = Libm_Abs(y);
	if (ax == libm_inf || ay == libm_inf) {
		return libm_inf;
	}
	if (Libm_IsNan(x) || Libm_IsNan(y)) {
		if (Libm_IsNan(x) && Libm_IsNan(y)) {
			return Libm_Quiet(Libm_NanLarger(x, y));
		}
		return Libm_IsNan(x) ? Libm_Quiet(x) : Libm_Quiet(y);
	}
	double big = ax > ay ? ax : ay, small = ax > ay ? ay : ax;
	double r = small == 0.0 ? big : Libm_Chop(Libm_HypotX87(big, small));
	if ((r < 0x1p-1022 && r != 0.0) || r == libm_inf) {
		/* x < 0 ? -x : x keeps -0; the quotients with 64 bit precision */
		double a1 = Libm_DivX87(x < 0 ? -x : x, big), a2 = Libm_DivX87(y < 0 ? -y : y, big);
		return Libm_Error(r == libm_inf ? LIBM_OVERFLOW : LIBM_UNDERFLOW, "_hypot", a1, a2, r);
	}
	return r;
}

/* ---- fmod: the x87 fprem ---- */

static double Libm_Fprem(double x, double y) {
	long double r;
	__asm__ volatile("1:\n\tfprem\n\tfnstsw %%ax\n\ttestb $4, %%ah\n\tjnz 1b" : "=t"(r) : "0"((long double)x),
		"u"((long double)y) : "eax", "cc");
	return (double)r;
}

/* plain = 0: _CIfmod, a NaN argument gives the one fprem picks; 1: fmod */
static double Libm_FmodCommon(double x, double y, int plain) {
	if (Libm_IsNan(x) || Libm_IsNan(y)) {
		return plain ? Libm_NanPair("fmod", x, y) : Libm_Fprem(x, y);
	}
	if (y == 0.0 || Libm_Abs(x) == libm_inf) {
		return Libm_Error(LIBM_DOMAIN, "fmod", x, y, LIBM_IND);
	}
	if (Libm_Abs(y) == libm_inf) {
		return x;
	}
	return Libm_Fprem(x, y);
}

double own_fmod(double x, double y) {
	return Libm_FmodCommon(x, y, 1);
}

long double Libm_CIFmod(double x, double y) {
	return Libm_FmodCommon(x, y, 0);
}

/* ---- the exact functions ---- */

double own_fabs(double x) {
	return Libm_IsNan(x) ? Libm_Quiet(x) : Libm_Abs(x);
}

/* floor (up = 0) and ceil (up = 1) */
static double Libm_ToInt(double x, int up) {
	if (Libm_IsNan(x)) {
		return Libm_Quiet(x);
	}
	if (!(Libm_Abs(x) < 0x1p52)) {
		return x;
	}
	double r = Libm_Rint(x);
	if (up ? r < x : r > x) {
		r += up ? 1.0 : -1.0;
	}
	/* a zero result keeps the sign of x (ceil(-0.5) is -0) */
	return r == 0.0 ? (Libm_Sign(x) ? -0.0 : 0.0) : r;
}

double own_floor(double x) {
	return Libm_ToInt(x, 0);
}

double own_ceil(double x) {
	return Libm_ToInt(x, 1);
}

double own_frexp(double x, int *e) {
	if (!(Libm_Abs(x) < libm_inf)) {
		*e = -1;
		return Libm_IsNan(x) ? Libm_Quiet(x) : x;
	}
	if (x == 0.0) {
		*e = 0;
		return x;
	}
	int k = 0;
	if (Libm_Exp2(x) == 0) {
		x *= 0x1p54;
		k = -54;
	}
	uint64_t b = Libm_Bits(x);
	*e = k + (int)((b >> 52) & 0x7ff) - 1022;
	return Libm_Double((b & 0x800fffffffffffffull) | 0x3fe0000000000000ull);
}

double own_ldexp(double x, int n) {
	if (!(Libm_Abs(x) < libm_inf) || x == 0.0) {
		return Libm_IsNan(x) ? Libm_Quiet(x) : x;
	}
	int e;
	own_frexp(x, &e);
	/* x = m * 2^e with m in [0.5, 1) */
	int64_t t = (int64_t)e + n;
	if (t > 1024) {
		return Libm_Error(LIBM_OVERFLOW, "ldexp", x, (double)n, Libm_Sign(x) ? -libm_inf : libm_inf);
	}
	if (t < -1100) {
		return Libm_Error(LIBM_UNDERFLOW, "ldexp", x, (double)n, Libm_Sign(x) ? -0.0 : 0.0);
	}
	/* x * 2^n exactly on the x87, cut toward zero below DBL_MIN */
	long double v;
	__asm__("fscale" : "=t"(v) : "0"((long double)x), "u"((long double)n));
	double r = Libm_Chop(v);
	if (Libm_Abs(r) == libm_inf) {
		return Libm_Error(LIBM_OVERFLOW, "ldexp", x, (double)n, r);
	}
	if (Libm_Abs(r) < 0x1p-1022) {
		return Libm_Error(LIBM_UNDERFLOW, "ldexp", x, (double)n, r);
	}
	return r;
}

double own_modf(double x, double *ip) {
	if (Libm_IsNan(x)) {
		*ip = Libm_Quiet(x);
		return *ip;
	}
	double i = Libm_ToInt(Libm_Abs(x), 0);
	*ip = Libm_Sign(x) ? -i : i;
	double f = Libm_Abs(x) == libm_inf ? 0.0 : Libm_Abs(x) - i;
	return Libm_Sign(x) ? -f : f;
}


/* ---- the entries with conventions of their own ---- */

/*
 * The SSE2 entries take the argument in xmm0 (pow the exponent in xmm1) and return the result in xmm0, the _CI
 * intrinsics take theirs on the x87 stack (_CIatan2: x on top of y, _CIfmod: y on top of x) and return st(0). Each
 * moves them to the stack for the C above; eax, ecx, edx and xmm0..7 may change, as in a cdecl call.
 */
#define LIBM_SSE2_ENTRY(label, func) \
	__asm__(".text\n\t.globl _" #label "\n\t.p2align 4\n_" #label ":\n\t" \
		"pushl %ebp\n\tmovl %esp, %ebp\n\tandl $-16, %esp\n\tsubl $16, %esp\n\t" \
		"movsd %xmm0, (%esp)\n\tmovsd %xmm1, 8(%esp)\n\t" \
		"call _" #func "\n\t" \
		"fstpl (%esp)\n\tmovsd (%esp), %xmm0\n\t" \
		"movl %ebp, %esp\n\tpopl %ebp\n\tret\n");

LIBM_SSE2_ENTRY(own_libm_sse2_acos_precise, own_acos)
LIBM_SSE2_ENTRY(own_libm_sse2_asin_precise, own_asin)
LIBM_SSE2_ENTRY(own_libm_sse2_atan_precise, Libm_Atan)
LIBM_SSE2_ENTRY(own_libm_sse2_cos_precise, own_cos)
LIBM_SSE2_ENTRY(own_libm_sse2_exp_precise, own_exp)
LIBM_SSE2_ENTRY(own_libm_sse2_log_precise, own_log)
LIBM_SSE2_ENTRY(own_libm_sse2_pow_precise, own_pow)
LIBM_SSE2_ENTRY(own_libm_sse2_sin_precise, own_sin)

#define LIBM_CI_ENTRY(label, func) \
	__asm__(".text\n\t.globl _" #label "\n\t.p2align 4\n_" #label ":\n\t" \
		"pushl %ebp\n\tmovl %esp, %ebp\n\tandl $-16, %esp\n\tsubl $16, %esp\n\t" \
		"fstpl 8(%esp)\n\tfstpl (%esp)\n\t" \
		"call _" #func "\n\t" \
		"movl %ebp, %esp\n\tpopl %ebp\n\tret\n");

LIBM_CI_ENTRY(own_CIatan2, Libm_CIAtan2)
LIBM_CI_ENTRY(own_CIfmod, Libm_CIFmod)
