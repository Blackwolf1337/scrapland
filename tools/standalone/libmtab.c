/*
 * Prints src/standalone/crtlibmtab.h, the tables and constants of the own MSVC math (crtlibm.c), from MPFR:
 * gcc -O2 -o libmtab tools/standalone/libmtab.c -lmpfr -lgmp && ./libmtab > src/standalone/crtlibmtab.h
 */
#include <mpfr.h>
#include <stdio.h>

static mpfr_t tmp;

/* v as hi = RN(v), lo = RN(v - hi) */
static void Dd(mpfr_t v, double *hi, double *lo) {
	*hi = mpfr_get_d(v, MPFR_RNDN);
	mpfr_sub_d(tmp, v, *hi, MPFR_RNDN);
	*lo = mpfr_get_d(tmp, MPFR_RNDN);
}

static void PrintDd(mpfr_t v) {
	double hi, lo;
	Dd(v, &hi, &lo);
	printf("{ %a, %a }", hi, lo);
}

/* the leading 'bits' bits of v (toward zero), removed from v */
static double Piece(mpfr_t v, int bits) {
	mpfr_t c;
	mpfr_init2(c, bits);
	mpfr_set(c, v, MPFR_RNDZ);
	double d = mpfr_get_d(c, MPFR_RNDN);
	mpfr_clear(c);
	mpfr_sub_d(v, v, d, MPFR_RNDN);
	return d;
}

static void DefineDd(const char *name, mpfr_t v) {
	printf("#define %s ((dd_t){ ", name);
	double hi, lo;
	Dd(v, &hi, &lo);
	printf("%a, %a })\n", hi, lo);
}

/* the coefficients 1/k (fact = 0) or 1/k! (fact = 1) for k = first, first + step, ...; with neg the signs alternate,
 * the first negative */
static void Series(const char *name, const char *what, int first, int step, int n, int fact, int neg) {
	mpfr_t v;
	mpfr_init(v);
	printf("/* %s */\nstatic const dd_t %s[%d] = {\n", what, name, n);
	for (int i = 0; i < n; i++) {
		int k = first + i * step;
		if (fact) {
			mpfr_fac_ui(v, k, MPFR_RNDN);
		} else {
			mpfr_set_ui(v, k, MPFR_RNDN);
		}
		mpfr_ui_div(v, 1, v, MPFR_RNDN);
		if (neg && !(i & 1)) {
			mpfr_neg(v, v, MPFR_RNDN);
		}
		printf("\t");
		PrintDd(v);
		printf(",\n");
	}
	printf("};\n\n");
	mpfr_clear(v);
}

int main(void) {
	mpfr_set_default_prec(600);
	mpfr_init(tmp);
	mpfr_t v, w, pi, ln2;
	mpfr_inits(v, w, pi, ln2, (mpfr_ptr)0);
	mpfr_const_pi(pi, MPFR_RNDN);
	mpfr_const_log2(ln2, MPFR_RNDN);

	printf("/* The tables of src/standalone/crtlibm.c, made by tools/standalone/libmtab.c (MPFR) */\n\n");

	mpfr_div_ui(v, ln2, 128, MPFR_RNDN);
	printf("/* ln2/128 in pieces: 32 bits (k * piece exact for |k| < 2^21), 53, 53 */\n");
	printf("#define LIBM_LN2_128_1 %a\n", Piece(v, 32));
	printf("#define LIBM_LN2_128_2 %a\n", Piece(v, 53));
	printf("#define LIBM_LN2_128_3 %a\n", Piece(v, 53));
	mpfr_ui_div(v, 128, ln2, MPFR_RNDN);
	printf("#define LIBM_INV_LN2_128 %a\n", mpfr_get_d(v, MPFR_RNDN));
	DefineDd("LIBM_LN2", ln2);
	mpfr_set_ui(v, 10, MPFR_RNDN);
	mpfr_log(v, v, MPFR_RNDN);
	mpfr_ui_div(v, 1, v, MPFR_RNDN);
	DefineDd("LIBM_INV_LN10", v);
	mpfr_div_ui(v, pi, 2, MPFR_RNDN);
	DefineDd("LIBM_PIO2", v);
	DefineDd("LIBM_PI", pi);
	mpfr_ui_div(v, 2, pi, MPFR_RNDN);
	printf("#define LIBM_2_PI %a\n", mpfr_get_d(v, MPFR_RNDN));
	printf("/* pi/2 in pieces: four of 32 bits (k * piece exact for |k| < 2^21), 53 */\n");
	mpfr_div_ui(v, pi, 2, MPFR_RNDN);
	for (int i = 1; i <= 5; i++) {
		printf("#define LIBM_PIO2_%d %a\n", i, Piece(v, i < 5 ? 32 : 53));
	}
	printf("\n");

	printf("/* 2^(j/128) */\nstatic const dd_t libm_exp2tab[128] = {\n");
	for (int j = 0; j < 128; j++) {
		mpfr_set_si(v, j, MPFR_RNDN);
		mpfr_div_ui(v, v, 128, MPFR_RNDN);
		mpfr_ui_pow(v, 2, v, MPFR_RNDN);
		printf("\t");
		PrintDd(v);
		printf(",\n");
	}
	printf("};\n\n");

	printf("/* c = 0.75 + i/128: the reciprocal r = RN(1/c) and -log(r) */\nstatic const struct {\n\tdouble inv;\n"
		"\tdd_t logc;\n} libm_logtab[97] = {\n");
	for (int i = 0; i <= 96; i++) {
		double inv = 1.0 / (0.75 + i / 128.0);
		mpfr_set_d(v, inv, MPFR_RNDN);
		mpfr_log(v, v, MPFR_RNDN);
		mpfr_neg(v, v, MPFR_RNDN);
		printf("\t{ %a, ", inv);
		PrintDd(v);
		printf(" },\n");
	}
	printf("};\n\n");

	printf("/* sin(j/64) and cos(j/64) */\nstatic const dd_t libm_sincostab[52][2] = {\n");
	for (int j = 0; j < 52; j++) {
		mpfr_set_si(w, j, MPFR_RNDN);
		mpfr_div_ui(w, w, 64, MPFR_RNDN);
		mpfr_t s, c;
		mpfr_inits(s, c, (mpfr_ptr)0);
		mpfr_sin_cos(s, c, w, MPFR_RNDN);
		printf("\t{ ");
		PrintDd(s);
		printf(", ");
		PrintDd(c);
		printf(" },\n");
		mpfr_clears(s, c, (mpfr_ptr)0);
	}
	printf("};\n\n");

	printf("/* atan(j/64) */\nstatic const dd_t libm_atantab[65] = {\n");
	for (int j = 0; j <= 64; j++) {
		mpfr_set_si(v, j, MPFR_RNDN);
		mpfr_div_ui(v, v, 64, MPFR_RNDN);
		mpfr_atan(v, v, MPFR_RNDN);
		printf("\t");
		PrintDd(v);
		printf(",\n");
	}
	printf("};\n\n");

	Series("libm_expcoef", "1/2!, 1/3!, ... 1/10!", 2, 1, 9, 1, 0);
	Series("libm_log1pcoef", "-1/2, 1/3, -1/4, ... 1/13", 2, 1, 12, 0, 1);
	Series("libm_sincoef", "-1/3!, 1/5!, ... -1/11!", 3, 2, 5, 1, 1);
	Series("libm_coscoef", "-1/2!, 1/4!, ... 1/12!", 2, 2, 6, 1, 1);
	Series("libm_atancoef", "-1/3, 1/5, ... -1/15", 3, 2, 7, 0, 1);
	Series("libm_sinhcoef", "1/3!, 1/5!, ... 1/21!", 3, 2, 10, 1, 0);
	Series("libm_coshcoef", "1/2!, 1/4!, ... 1/20!", 2, 2, 10, 1, 0);
	return 0;
}
