/*
 * The exact decimal and binary conversions of the own printf, scanf and strtod of ScrapC.exe (src/standalone/crtnum.c,
 * used by crtfmt.c and crtscan.c). Integer arithmetic only, so the x87 precision and the SSE state of the game do not
 * change a result.
 */
#ifndef SCRAP_CRTNUM_H
#define SCRAP_CRTNUM_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* the most significant digits of a double (2^-1074 * (2^53 - 1) has 767) */
#define CRT_DIGITS_MAX 768

/* the decimal digits strtod and scanf read; the runtime drops the ones after them, nonzero or not */
#define CRT_PARSE_DIGITS 768

/* the status of Crt_RoundBits and Crt_DecimalBits */
enum {
	CRT_NUM_INEXACT = 1,
	CRT_NUM_TINY = 2,		/* the result is below the smallest normal number: a denormal or zero */
	CRT_NUM_OVERFLOW = 4
};

/* all decimal digits of |v| (finite): v = 0.d1d2..dn * 10^decpt, no leading or trailing zeros; 0 digits for zero */
int Crt_DoubleDigits(double v, char *digits, int *decpt);

/*
 * mant * 2^exp2, plus a little more when sticky, rounded half even: the bits of the magnitude as a double, or as a
 * float when single
 */
uint64_t Crt_RoundBits(uint64_t mant, int exp2, int sticky, int single, int *status);

/*
 * 0.d1d2..dn * 10^decpt (d1 not '0', n up to CRT_PARSE_DIGITS), plus a little more when sticky, correctly rounded, the
 * bits as Crt_RoundBits
 */
uint64_t Crt_DecimalBits(const char *digits, int n, int decpt, int sticky, int single, int *status);

/*
 * src/standalone/crtfmt.c and crtscan.c: __stdio_common_vsprintf, __stdio_common_vfprintf and __stdio_common_vsscanf
 * without a locale, strtod, atof and strerror
 */
int Crt_VFormat(uint64_t options, char *buf, size_t count, const char *fmt, va_list ap);
int Crt_VFormatFile(uint64_t options, FILE *f, const char *fmt, va_list ap);
int Crt_VScan(uint64_t options, const char *buf, size_t count, const char *fmt, va_list ap);
double Crt_Strtod(const char *s, char **end);
double Crt_Atof(const char *s);
const char *Crt_ErrorText(int code);

#endif
