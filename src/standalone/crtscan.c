/*
 * The scanf core, strtod and atof of ScrapC.exe (__stdio_common_vsscanf without a locale, stage 2b of
 * docs/standalone.md), written from the documented behavior of the MSVC UCRT and its observed output, not from its
 * code. strtod and the float conversions of scanf share one parser; the decimal and hexadecimal digits become the
 * correctly rounded double or float in src/standalone/crtnum.c (a float straight from the digits, no double between).
 *
 * What the runtime does that a plain reading of the C standard does not tell:
 * - strtod and scanf read the first 768 significant digits and drop the others, nonzero or not; a number with more than
 *   1088 digits after the point (after the dropping and without the zeros at its end) is zero, an underflow; an exact
 *   tie the runtime reaches through its division of two words by one rounds up (Num_TieUp)
 * - sscanf reads its input as signed chars: a byte above 0x7f becomes a negative wide character (0xe9 is L'\xffe9'),
 *   and the byte 0xff reads as EOF. A number or a literal that stops at it keeps it read, the end of a string and a
 *   whitespace skip give it back unless it is the last byte of the input
 * - a number that stops in an incomplete state ("1e+", "0x", "inf" followed by "ini") fails, it is not cut back; a
 *   failure is an input failure (EOF) when the next character, width or not, is the end of the input or 0xff
 * - EOF is also the result when the input ends with the format and no conversion (%n and suppressed ones count) ran
 * - integers are read into 64 bits: d and i saturate at the signed limits, u, o and x at the unsigned one, with ERANGE;
 *   the result is then cut to the size
 * - %I that is not followed by 32, 64 or an integer conversion is itself an %i
 * - scanf sets no ERANGE for floats; strtod does for an overflow and for a nonzero input that gives zero
 */
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "crtnum.h"

/*
 * The options of __stdio_common_vsscanf: _CRT_INTERNAL_SCANF_SECURECRT (%s, %c and %[ take a buffer size, an invalid
 * format sets EINVAL) and _CRT_INTERNAL_SCANF_LEGACY_MSVCRT_COMPATIBILITY (%c takes what is left of the input).
 */
#define SO_SECURE 1
#define SO_LEGACY_MSVCRT 4

#define IN_EOF (-1)

typedef struct {
	const unsigned char *p;
	const unsigned char *end;	/* NULL: up to the NUL */
	unsigned width;				/* characters a conversion may still take */
	int limited;				/* width counts */
	int count;					/* characters taken, for %n */
	int ff;						/* sscanf: a 0xff that a number reads stays read */
} scanIn_t;

static int In_AtEnd(const scanIn_t *in, const unsigned char *p) {
	return (in->end && p >= in->end) || !*p;
}

/* the next character, IN_EOF at the end of the input or of the width */
static int In_Peek(const scanIn_t *in) {
	return (in->limited && !in->width) || In_AtEnd(in, in->p) ? IN_EOF : *in->p;
}

static void In_Next(scanIn_t *in) {
	in->p++;
	in->count++;
	if (in->limited) {
		in->width--;
	}
}

/* sscanf: the next character reads as EOF */
static int In_EofNext(const scanIn_t *in) {
	return In_AtEnd(in, in->p) || *in->p == 0xff;
}

/* a 0xff that a reading gives back as EOF stays read (it counts for %n, not for the width) */
static void In_KeepFF(scanIn_t *in) {
	in->p++;
	in->count++;
}

/* a 0xff that stopped a reading stays read when always, else only when it is the last byte */
static void In_DropFF(scanIn_t *in, int always) {
	if (In_Peek(in) == 0xff && (always || In_AtEnd(in, in->p + 1))) {
		In_KeepFF(in);
	}
}

#define IN_FF (-2)

/* the character a number reads; a 0xff (sscanf) is taken and matches nothing */
static int In_Look(scanIn_t *in) {
	int c = In_Peek(in);

	if (c == 0xff && in->ff) {
		In_KeepFF(in);
		return IN_FF;
	}
	return c;
}

/* the next reading of the position of c: c again, unless it was a 0xff that is gone */
static int In_Again(scanIn_t *in, int c) {
	return c == IN_FF ? In_Look(in) : c;
}

/* takes the character and reads the next */
static int In_Take(scanIn_t *in) {
	In_Next(in);
	return In_Look(in);
}

static int In_Space(int c) {
	return c == ' ' || (c >= '\t' && c <= '\r');
}

static int Lower(int c) {
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int HexValue(int c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	c = Lower(c);
	return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/* ---- floating point numbers ---- */

enum { NUM_ZERO, NUM_DEC, NUM_HEX, NUM_INF, NUM_NAN, NUM_IND, NUM_SNAN };

typedef struct {
	int negative;
	int kind;
	int n, decpt;				/* NUM_DEC: 0.digits * 10^decpt */
	uint64_t mant;				/* NUM_HEX: mant * 2^exp2, and sticky for nonzero hex digits after it */
	int exp2, sticky;
	char digits[CRT_PARSE_DIGITS];
} numVal_t;

/* matches the rest of a word case-insensitively, *c is its first character; 0 when one differs */
static int Num_Word(scanIn_t *in, int *c, const char *rest) {
	for (; *rest; rest++) {
		if (Lower(*c) != *rest) {
			return 0;
		}
		*c = In_Take(in);
	}
	return 1;
}

/* a decimal exponent after e or p, saturated; 0 when there is no digit. *c is the character after the e or p */
static int Num_Exponent(scanIn_t *in, int *c, int *exp) {
	int negative = 0, e = 0;

	if (*c == '+' || *c == '-') {
		negative = *c == '-';
		*c = In_Take(in);
	}
	if (*c < '0' || *c > '9') {
		return 0;
	}
	for (; *c >= '0' && *c <= '9'; *c = In_Take(in)) {
		if (e < 100000000) {
			e = e * 10 + (*c - '0');
		}
	}
	*exp = negative ? -e : e;
	return 1;
}

static int Clamp(int64_t v) {
	return v > 100000000 ? 100000000 : v < -100000000 ? -100000000 : (int)v;
}

/*
 * Reads a number: [sign] digits [. digits] [e exp], 0x hexdigits [. hexdigits] [p exp], inf, infinity, nan,
 * nan(chars). Returns the count of characters of the longest complete number in it (0: none) and sets *incomplete
 * when the reading stopped in the middle of one. Each character the runtime reads is one In_Look or In_Take here
 * (In_Again where it reads the same position again: the check for the exponent, a second look after inf and nan, the
 * digits after the check for 0x), so sscanf loses the same 0xff bytes.
 */
static int Num_Scan(scanIn_t *in, numVal_t *v, int *incomplete) {
	int start = in->count, good = 0, digits = 0;
	int c = In_Look(in);

	memset(v, 0, offsetof(numVal_t, digits));
	*incomplete = 1;
	if (c == '+' || c == '-') {
		v->negative = c == '-';
		c = In_Take(in);
	}
	if (Lower(c) == 'i') {
		c = In_Take(in);
		if (!Num_Word(in, &c, "nf")) {
			return 0;
		}
		v->kind = NUM_INF;
		good = in->count - start;
		if (Lower(c) == 'i') {
			c = In_Take(in);
			if (!Num_Word(in, &c, "nity")) {
				return good;
			}
			good = in->count - start;
		} else {
			/* a second look after inf */
			In_Again(in, c);
		}
		*incomplete = 0;
		return good;
	}
	if (Lower(c) == 'n') {
		char tag[8];
		int len = 0;
		c = In_Take(in);
		if (!Num_Word(in, &c, "an")) {
			return 0;
		}
		v->kind = NUM_NAN;
		good = in->count - start;
		if (c == '(') {
			for (c = In_Take(in); (c >= '0' && c <= '9') || (Lower(c) >= 'a' && Lower(c) <= 'z') || c == '_'; c = In_Take(in)) {
				if (len < (int)sizeof(tag) - 1) {
					tag[len] = (char)Lower(c);
				}
				len++;
			}
			if (c != ')') {
				return good;
			}
			In_Next(in);
			good = in->count - start;
			tag[len < (int)sizeof(tag) ? len : (int)sizeof(tag) - 1] = '\0';
			if (len == 3 && !strcmp(tag, "ind")) {
				v->kind = NUM_IND;
			} else if (len == 4 && !strcmp(tag, "snan")) {
				v->kind = NUM_SNAN;
			}
			/* a look after the ) of these two only */
			if (v->kind != NUM_NAN) {
				In_Look(in);
			}
		} else {
			In_Again(in, c);
		}
		*incomplete = 0;
		return good;
	}
	if (c == '0') {
		good = in->count + 1 - start;
		c = In_Take(in);
		if (Lower(c) == 'x') {
			int kept = 0, d;
			for (c = In_Take(in); (d = HexValue(c)) >= 0; c = In_Take(in)) {
				digits++;
				if (!kept && !d) {
					continue;
				}
				if (kept < 16) {
					v->mant = v->mant * 16 + (uint64_t)d;
					kept++;
				} else {
					v->sticky |= d != 0;
					v->exp2 = Clamp((int64_t)v->exp2 + 4);
				}
			}
			if (c == '.') {
				for (c = In_Take(in); (d = HexValue(c)) >= 0; c = In_Take(in)) {
					digits++;
					if (!kept && !d) {
						v->exp2 = Clamp((int64_t)v->exp2 - 4);
					} else if (kept < 16) {
						v->mant = v->mant * 16 + (uint64_t)d;
						kept++;
						v->exp2 -= 4;
					} else {
						v->sticky |= d != 0;
					}
				}
			}
			if (!digits) {
				return good;
			}
			v->kind = v->mant || v->sticky ? NUM_HEX : NUM_ZERO;
			good = in->count - start;
			*incomplete = 0;
			if (Lower(c = In_Again(in, c)) == 'p') {
				int e;
				*incomplete = 1;
				c = In_Take(in);
				if (!Num_Exponent(in, &c, &e)) {
					return good;
				}
				v->exp2 = Clamp((int64_t)v->exp2 + e);
				good = in->count - start;
				*incomplete = 0;
			}
			return good;
		}
		/* the digits go on after the check for the x */
		digits = 1;
		c = In_Again(in, c);
	} else if ((c < '0' || c > '9') && c != '.') {
		return 0;
	}
	for (; c >= '0' && c <= '9'; c = In_Take(in)) {
		digits++;
		if (!v->n && c == '0') {
			continue;
		}
		if (v->n < CRT_PARSE_DIGITS) {
			v->digits[v->n++] = (char)c;
		}
		v->decpt = Clamp((int64_t)v->decpt + 1);
	}
	if (c == '.') {
		for (c = In_Take(in); c >= '0' && c <= '9'; c = In_Take(in)) {
			digits++;
			if (!v->n && c == '0') {
				v->decpt = Clamp((int64_t)v->decpt - 1);
			} else if (v->n < CRT_PARSE_DIGITS) {
				v->digits[v->n++] = (char)c;
			}
		}
	}
	if (!digits) {
		return 0;
	}
	v->kind = v->n ? NUM_DEC : NUM_ZERO;
	good = in->count - start;
	*incomplete = 0;
	if (Lower(c = In_Again(in, c)) == 'e') {
		int e;
		*incomplete = 1;
		c = In_Take(in);
		if (!Num_Exponent(in, &c, &e)) {
			return good;
		}
		v->decpt = Clamp((int64_t)v->decpt + e);
		good = in->count - start;
		*incomplete = 0;
	}
	while (v->n && v->digits[v->n - 1] == '0') {
		v->n--;
	}
	return good;
}

/*
 * The runtime's division of a two word number by a one word one leaves the remainder as nonzero, so a tie it reaches
 * that way rounds up: a number with an integer part of fewer bits than the precision plus one (rf bits short) and one
 * to nine digits after the point that need more than 32 bits once shifted by rf (64976.431640625 to a float).
 */
static int Num_TieUp(const numVal_t *v, int single) {
	int k = v->n - v->decpt;
	uint64_t ip = 0;
	uint32_t frac = 0;

	if (v->decpt < 1 || v->decpt > 19 || k < 1 || k > 9) {
		return 0;
	}
	for (int i = 0; i < v->decpt; i++) {
		ip = ip * 10 + (uint64_t)(v->digits[i] - '0');
	}
	for (int i = v->decpt; i < v->n; i++) {
		frac = frac * 10 + (uint32_t)(v->digits[i] - '0');
	}
	int rf = (single ? 25 : 54) - (64 - __builtin_clzll(ip));
	return rf > 0 && rf + 32 - __builtin_clz(frac) > 32;
}

/* the bits of the double, or of the float when single; *status as Crt_RoundBits, 0 for inf and nan */
static uint64_t Num_Bits(const numVal_t *v, int single, int *status) {
	uint64_t sign = v->negative ? (single ? 1ull << 31 : 1ull << 63) : 0;
	uint64_t bits;

	*status = 0;
	switch (v->kind) {
	case NUM_DEC:
		if (v->n - v->decpt > 1088) {
			/* more digits after the point than the runtime holds: it gives zero, an underflow */
			*status = CRT_NUM_INEXACT | CRT_NUM_TINY;
			bits = 0;
		} else {
			bits = Crt_DecimalBits(v->digits, v->n, v->decpt, Num_TieUp(v, single), single, status);
		}
		break;
	case NUM_HEX:
		bits = Crt_RoundBits(v->mant, v->exp2, v->sticky, single, status);
		break;
	case NUM_INF:
		bits = single ? 0x7f800000 : 0x7ff0000000000000ull;
		break;
	case NUM_NAN:
		/* the nan constants of the runtime: all payload bits, the indefinite, a signaling one with payload 1 */
		bits = single ? 0x7fffffff : 0x7fffffffffffffffull;
		break;
	case NUM_IND:
		return single ? 0xffc00000 : 0xfff8000000000000ull;
	case NUM_SNAN:
		bits = single ? 0x7f800001 : 0x7ff0000000000001ull;
		break;
	default:
		bits = 0;
		break;
	}
	return bits | sign;
}

double Crt_Strtod(const char *s, char **end) {
	scanIn_t in = { (const unsigned char *)s, NULL, 0, 0, 0, 0 };
	numVal_t v;
	int incomplete, status;
	double r;

	while (In_Space(In_Peek(&in))) {
		In_Next(&in);
	}
	const char *start = (const char *)in.p;
	int good = Num_Scan(&in, &v, &incomplete);
	if (end) {
		*end = (char *)(good ? start + good : s);
	}
	if (!good) {
		return 0.0;
	}
	uint64_t bits = Num_Bits(&v, 0, &status);
	if ((status & CRT_NUM_OVERFLOW) || ((status & CRT_NUM_INEXACT) && !(bits << 1))) {
		errno = ERANGE;
	}
	memcpy(&r, &bits, sizeof(r));
	return r;
}

/* strtod without the end and without touching errno */
double Crt_Atof(const char *s) {
	int saved = errno;
	double r = Crt_Strtod(s, NULL);

	errno = saved;
	return r;
}

/* ---- scanf ---- */

enum { SZ_NONE, SZ_HH, SZ_H, SZ_L, SZ_LL, SZ_BIGL, SZ_I32, SZ_I64, SZ_J, SZ_Z, SZ_T, SZ_W, SZ_TCHAR, SZ_I };

enum { FAIL_NONE, FAIL_MATCH, FAIL_INPUT };

/* a conversion stopped: an input failure when the input ends at this point (also past the width) */
static int Scan_Fail(const scanIn_t *in) {
	return In_EofNext(in) ? FAIL_INPUT : FAIL_MATCH;
}

/* skips white space (not counted in a width) */
static void Scan_SkipSpace(scanIn_t *in) {
	int limited = in->limited;

	in->limited = 0;
	while (In_Space(In_Peek(in))) {
		In_Next(in);
	}
	In_DropFF(in, 0);
	in->limited = limited;
}

static void Store_Int(void *p, uint64_t v, int size) {
	if (size == SZ_HH) {
		*(uint8_t *)p = (uint8_t)v;
	} else if (size == SZ_H) {
		*(uint16_t *)p = (uint16_t)v;
	} else if (size == SZ_LL || size == SZ_I64 || size == SZ_J) {
		memcpy(p, &v, sizeof(v));
	} else {
		*(uint32_t *)p = (uint32_t)v;
	}
}

/* an integer of the conversion type into *out; 0 when there is no digit. The reads are those of Num_Scan */
static int Scan_Integer(scanIn_t *in, char type, uint64_t *out) {
	int base = type == 'o' ? 8 : (type == 'x' || type == 'X' || type == 'p') ? 16 : type == 'i' ? 0 : 10;
	int negative = 0, digits = 0, overflow = 0, d;
	uint64_t mag = 0;
	int c = In_Look(in);

	if (c == '+' || c == '-') {
		negative = c == '-';
		c = In_Take(in);
	}
	if ((base == 16 || base == 0) && c == '0') {
		c = In_Take(in);
		digits = 1;
		if (Lower(c) == 'x') {
			c = In_Take(in);
			digits = 0;
			base = 16;
		} else {
			if (!base) {
				base = 8;
			}
			c = In_Again(in, c);
		}
	} else if (!base) {
		base = 10;
	}
	for (; (d = HexValue(c)) >= 0 && d < base; c = In_Take(in)) {
		if (mag > (UINT64_MAX - (uint64_t)d) / (uint64_t)base) {
			overflow = 1;
		}
		mag = mag * (uint64_t)base + (uint64_t)d;
		digits++;
	}
	if (!digits) {
		return 0;
	}
	if (type == 'd' || type == 'i') {
		if (negative && (overflow || mag > 1ull << 63)) {
			errno = ERANGE;
			mag = 1ull << 63;
		} else if (!negative && (overflow || mag > INT64_MAX)) {
			errno = ERANGE;
			mag = INT64_MAX;
		} else if (negative) {
			mag = 0 - mag;
		}
	} else if (overflow) {
		errno = ERANGE;
		mag = UINT64_MAX;
	} else if (negative) {
		mag = 0 - mag;
	}
	*out = mag;
	return 1;
}

/* the 256 bits of a %[ set; the format pointer ends after the ], NULL when there is none */
static const unsigned char *Scan_Set(const unsigned char *f, uint8_t set[32]) {
	int negate = 0;

	memset(set, 0, 32);
	if (*f == '^') {
		negate = 1;
		f++;
	}
	if (*f == ']') {
		set[']' >> 3] |= 1 << (']' & 7);
		f++;
	}
	for (; *f != ']'; f++) {
		if (!*f) {
			return NULL;
		}
		unsigned lo = *f, hi = *f;
		if (f[1] == '-' && f[2] && f[2] != ']') {
			hi = f[2];
			f += 2;
			if (lo > hi) {
				unsigned t = lo;
				lo = hi;
				hi = t;
			}
		}
		for (unsigned c = lo; c <= hi; c++) {
			set[c >> 3] |= 1 << (c & 7);
		}
	}
	if (negate) {
		for (int i = 0; i < 32; i++) {
			set[i] = (uint8_t)~set[i];
		}
	}
	return f + 1;
}

/* the sizes a conversion takes (the others make the format invalid) */
static int Scan_SizeOk(char type, int size) {
	switch (type) {
	case 'c':
	case 'C':
	case 's':
	case 'S':
		return size == SZ_NONE || size == SZ_H || size == SZ_L || size == SZ_LL || size == SZ_BIGL || size == SZ_W
			|| size == SZ_TCHAR;
	case '[':
		return size == SZ_NONE || size == SZ_L || size == SZ_W || size == SZ_TCHAR;
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	case 'a':
	case 'A':
		return size == SZ_NONE || size == SZ_L || size == SZ_BIGL || size == SZ_W;
	case 'p':
		return 1;
	default:
		return size != SZ_BIGL && size != SZ_TCHAR;
	}
}

/* %c, %s and %[ (set): narrow or wide characters, with a buffer size in the secure variant; the failure */
static int Scan_Chars(scanIn_t *in, const uint8_t *set, char type, int size, int suppress, uint64_t options,
	va_list *args, int *assigned) {
	int wide = size == SZ_L || size == SZ_LL || size == SZ_BIGL || size == SZ_W
		|| ((type == 'C' || type == 'S') && size != SZ_H);
	int terminate = type != 'c' && type != 'C';
	unsigned char *dst = NULL;
	uint32_t room = UINT32_MAX, stored = 0;

	if (!suppress) {
		dst = va_arg(*args, unsigned char *);
		if (options & SO_SECURE) {
			room = va_arg(*args, uint32_t);
			/* the legacy variant only finds out at the first character */
			if (!room && !(options & SO_LEGACY_MSVCRT)) {
				errno = ENOMEM;
				return Scan_Fail(in);
			}
		}
	}
	if (dst && !room) {
		/* the legacy variant with no room takes the next character, whatever it is, and fails */
		if (!In_AtEnd(in, in->p)) {
			In_Next(in);
		}
		memset(dst, 0, wide ? 2 : 1);
		errno = ENOMEM;
		return Scan_Fail(in);
	}
	if (!terminate && !in->limited) {
		in->width = 1;
		in->limited = 1;
	}
	for (;;) {
		int c = In_Peek(in);
		if (c == IN_EOF || c == 0xff || (set ? !(set[c >> 3] & (1 << (c & 7))) : terminate && In_Space(c))) {
			break;
		}
		In_Next(in);
		if (dst) {
			if (stored + (uint32_t)terminate >= room) {
				/* no room: the buffer is emptied, the input after the character decides EOF */
				memset(dst, 0, wide ? 2 : 1);
				errno = ENOMEM;
				return Scan_Fail(in);
			}
			if (wide) {
				uint16_t w = (uint16_t)(int16_t)(int8_t)c;
				memcpy(dst + 2 * stored, &w, 2);
			} else {
				dst[stored] = (unsigned char)c;
			}
		}
		stored++;
	}
	int shortRead = !terminate && in->width;
	In_DropFF(in, 0);
	if (shortRead && (!stored || !(options & SO_LEGACY_MSVCRT))) {
		/* %c ran out of input before its count */
		return FAIL_INPUT;
	}
	if (!stored) {
		return Scan_Fail(in);
	}
	if (dst) {
		if (terminate) {
			memset(dst + (wide ? 2 * stored : stored), 0, wide ? 2 : 1);
		}
		(*assigned)++;
	}
	return FAIL_NONE;
}

int Crt_VScan(uint64_t options, const char *buf, size_t count, const char *fmt, va_list ap) {
	const unsigned char *end = count == (size_t)-1 ? NULL : (const unsigned char *)buf + count;
	scanIn_t in = { (const unsigned char *)buf, end, 0, 0, 0, 1 };
	const unsigned char *f = (const unsigned char *)fmt;
	int assigned = 0, done = 0, fail = FAIL_NONE, invalid = 0;
	va_list args;

	if (!buf || !fmt) {
		errno = EINVAL;
		return -1;
	}
	va_copy(args, ap);
	while (*f && fail == FAIL_NONE && !invalid) {
		in.limited = 0;
		if (In_Space(*f)) {
			while (In_Space(*f)) {
				f++;
			}
			Scan_SkipSpace(&in);
			continue;
		}
		if (*f != '%' || f[1] == '%') {
			if (*f == '%') {
				f++;
			}
			int c = In_Peek(&in);
			if (c == IN_EOF) {
				fail = FAIL_INPUT;
			} else if (c == 0xff || c != *f) {
				In_DropFF(&in, 1);
				fail = Scan_Fail(&in);
			} else {
				In_Next(&in);
				f++;
			}
			continue;
		}
		f++;
		int suppress = 0, size = SZ_NONE;
		uint32_t width = 0;
		if (*f == '*') {
			suppress = 1;
			f++;
		}
		if (*f >= '1' && *f <= '9') {
			while (*f >= '0' && *f <= '9') {
				width = width * 10 + (uint32_t)(*f++ - '0');
			}
		}
		switch (*f) {
		case 'h':
			size = f[1] == 'h' ? SZ_HH : SZ_H;
			f += size == SZ_HH ? 2 : 1;
			break;
		case 'l':
			size = f[1] == 'l' ? SZ_LL : SZ_L;
			f += size == SZ_LL ? 2 : 1;
			break;
		case 'L':
			size = SZ_BIGL;
			f++;
			break;
		case 'j':
			size = SZ_J;
			f++;
			break;
		case 'z':
			size = SZ_Z;
			f++;
			break;
		case 't':
			size = SZ_T;
			f++;
			break;
		case 'w':
			size = SZ_W;
			f++;
			break;
		case 'T':
			size = SZ_TCHAR;
			f++;
			break;
		case 'I':
			if (f[1] == '3' && f[2] == '2') {
				size = SZ_I32;
				f += 3;
			} else if (f[1] == '6' && f[2] == '4') {
				size = SZ_I64;
				f += 3;
			} else if (f[1] && strchr("diouxX", f[1])) {
				size = SZ_I;
				f++;
			}
			break;
		}
		/* a lone I is an %i after the other sizes */
		char type = *f == 'I' ? 'i' : (char)*f;
		uint8_t set[32];
		if (!type || !strchr("diouxXpneEfFgGaAcCsS[", type) || !Scan_SizeOk(type, size)) {
			invalid = 1;
			break;
		}
		f++;
		if (type == '[' && !(f = Scan_Set(f, set))) {
			invalid = 1;
			break;
		}
		in.width = width;
		in.limited = width != 0;
		if (type == 'n') {
			if (!suppress) {
				Store_Int(va_arg(args, void *), (uint64_t)(int64_t)in.count, size);
			}
			done++;
			continue;
		}
		if (type != 'c' && type != 'C' && type != '[') {
			Scan_SkipSpace(&in);
			/* %s first takes its buffer and size */
			if (In_AtEnd(&in, in.p) && type != 's' && type != 'S') {
				fail = FAIL_INPUT;
				break;
			}
		}
		if (strchr("diouxXp", type)) {
			uint64_t v;
			if (!Scan_Integer(&in, type, &v)) {
				fail = Scan_Fail(&in);
			} else if (!suppress) {
				Store_Int(va_arg(args, void *), v, type == 'p' ? SZ_NONE : size);
				assigned++;
			}
		} else if (strchr("eEfFgGaA", type)) {
			numVal_t v;
			int incomplete, status;
			int good = Num_Scan(&in, &v, &incomplete);
			if (!good || incomplete) {
				fail = Scan_Fail(&in);
			} else if (!suppress) {
				int dbl = size == SZ_L || size == SZ_BIGL;
				uint64_t bits = Num_Bits(&v, !dbl, &status);
				void *p = va_arg(args, void *);
				if (dbl) {
					memcpy(p, &bits, 8);
				} else {
					*(uint32_t *)p = (uint32_t)bits;
				}
				assigned++;
			}
		} else {
			fail = Scan_Chars(&in, type == '[' ? set : NULL, type, size, suppress, options, &args, &assigned);
		}
		if (fail == FAIL_NONE) {
			done++;
		}
	}
	va_end(args);
	if (invalid && (options & SO_SECURE)) {
		errno = EINVAL;
	}
	if (!done && (fail == FAIL_INPUT || (fail == FAIL_NONE && In_EofNext(&in)))) {
		return -1;
	}
	return assigned;
}
