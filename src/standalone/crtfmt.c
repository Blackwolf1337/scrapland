/*
 * The printf core of ScrapC.exe (__stdio_common_vsprintf and __stdio_common_vfprintf without a locale, stage 2b of
 * docs/standalone.md) and the strerror texts, written from the documented behavior of the MSVC UCRT and its observed
 * output, not from its code. The UCRT of the original (10.0.10586) is the reference: the digits of a double are exact
 * for any precision (src/standalone/crtnum.c) and rounded half away from zero on the exact value (later UCRTs round
 * half to even); %a rounds up only above half a digit; the inf and nan texts go through '#' and the padding like
 * digits, and with the MSVCRT option the 1.#INF forms through the rounding too.
 *
 * The format is the state machine of the runtime: a character that does not fit the state is written as it is (%y
 * gives y), only %n, the near pointer size N, a size after a size (but for hh, ll and after a lone I) and L, w or T
 * with an integer are invalid (EINVAL, -1). A wide character without a byte (EILSEQ) writes nothing and the format goes
 * on; in a wide string it ends the format with -1.
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "crtnum.h"

/* the options of __stdio_common_vsprintf (_CRT_INTERNAL_PRINTF_*); LEGACY_WIDE_SPECIFIERS (4) changes nothing here */
#define PO_LEGACY_TERMINATION 1
#define PO_STANDARD_SNPRINTF 2
#define PO_LEGACY_MSVCRT 8
#define PO_THREE_DIGIT_EXP 0x10

enum { FL_MINUS = 1, FL_PLUS = 2, FL_SPACE = 4, FL_HASH = 8, FL_ZERO = 16 };

enum { SZ_NONE, SZ_HH, SZ_H, SZ_L, SZ_LL, SZ_BIGL, SZ_J, SZ_Z, SZ_T, SZ_W, SZ_TCHAR, SZ_I, SZ_I32, SZ_I64 };

typedef struct {
	char *buf;
	size_t count;
	size_t used;
	uint32_t total;	/* wraps like the int count of the runtime */
	int failed;		/* the buffer is full and the text goes on (not the standard snprintf behavior), or a write failed */
	int stdSnprintf;
	FILE *file;		/* fprintf: the text goes there in pieces of the chunk */
	int chunkLen;
	char chunk[512];
} fmtOut_t;

typedef struct {
	int flags;
	uint32_t width;	/* -INT_MIN from '*' is 2^31 */
	int prec;		/* -1 when not given */
	int size;
	char type;
} fmtSpec_t;

static void Out_Flush(fmtOut_t *o) {
	if (o->chunkLen && fwrite(o->chunk, 1, (size_t)o->chunkLen, o->file) != (size_t)o->chunkLen) {
		o->failed = 1;
	}
	o->chunkLen = 0;
}

static void Out_Put(fmtOut_t *o, const char *s, size_t n) {
	for (size_t i = 0; i < n && !(o->file && o->failed); i++) {
		if (o->file) {
			o->chunk[o->chunkLen++] = s[i];
			if (o->chunkLen == (int)sizeof(o->chunk)) {
				Out_Flush(o);
			}
		} else if (o->buf && o->used >= o->count) {
			if (!o->stdSnprintf) {
				o->failed = 1;
				return;
			}
		} else if (o->buf) {
			o->buf[o->used++] = s[i];
		}
		o->total++;
	}
}

static void Out_Repeat(fmtOut_t *o, char c, int64_t n) {
	for (; n > 0 && !o->failed; n--) {
		Out_Put(o, &c, 1);
	}
}

/* a string of a conversion: a full buffer fails it even when it is empty */
static void Out_Str(fmtOut_t *o, const char *s, size_t n) {
	if (o->buf && o->used >= o->count && !o->stdSnprintf) {
		o->failed = 1;
		return;
	}
	Out_Put(o, s, n);
}

/* the field: padding, prefix (sign, 0x), body */
static void Out_Field(fmtOut_t *o, const fmtSpec_t *s, const char *prefix, const char *body, int len) {
	int plen = (int)strlen(prefix);
	int64_t pad = (int64_t)s->width - plen - len;

	if (!(s->flags & (FL_MINUS | FL_ZERO))) {
		Out_Repeat(o, ' ', pad);
	}
	Out_Str(o, prefix, (size_t)plen);
	if ((s->flags & (FL_MINUS | FL_ZERO)) == FL_ZERO) {
		Out_Repeat(o, '0', pad);
	}
	Out_Str(o, body, (size_t)len);
	if (s->flags & FL_MINUS) {
		Out_Repeat(o, ' ', pad);
	}
}

static void Fmt_Integer(fmtOut_t *o, fmtSpec_t *s, uint64_t v, int negative) {
	/* the digits with the zeros of the precision, at most 512, and the 0 of %#o */
	char buf[520];
	char *digits = buf + sizeof(buf);
	char prefix[3] = "";
	int radix = s->type == 'o' ? 8 : (s->type == 'x' || s->type == 'X' || s->type == 'p') ? 16 : 10;
	const char *set = s->type == 'x' ? "0123456789abcdef" : "0123456789ABCDEF";
	int prec = s->prec;
	int nonzero = v != 0;

	if (prec < 0) {
		prec = 1;
	} else {
		s->flags &= ~FL_ZERO;
		if (prec > 512) {
			prec = 512;
		}
	}
	for (; v; v /= (uint64_t)radix) {
		*--digits = set[v % (uint64_t)radix];
	}
	while (buf + sizeof(buf) - digits < prec) {
		*--digits = '0';
	}
	if (negative) {
		prefix[0] = '-';
	} else if (s->type == 'd' || s->type == 'i') {
		if (s->flags & FL_PLUS) {
			prefix[0] = '+';
		} else if (s->flags & FL_SPACE) {
			prefix[0] = ' ';
		}
	}
	int n = (int)(buf + sizeof(buf) - digits);
	if (s->flags & FL_HASH) {
		if (radix == 8 && (!n || digits[0] != '0')) {
			*--digits = '0';
			n++;
		} else if (radix == 16 && nonzero) {
			prefix[0] = '0';
			prefix[1] = s->type;
		}
	}
	Out_Field(o, s, prefix, digits, n);
}

/* the decimal digits of a double for one conversion: value 0.d * 10^decpt, letters too for the MSVCRT specials */
typedef struct {
	char d[CRT_DIGITS_MAX + 2];
	int n;
	int decpt;
} fmtDigits_t;

/* keeps the first keep digits, rounded up when the next one is 5 or more: half away from zero on exact digits */
static void Dig_Round(fmtDigits_t *g, int keep) {
	if (keep >= g->n) {
		return;
	}
	if (keep < 0) {
		g->n = 0;
		return;
	}
	char next = g->d[keep];
	g->n = keep;
	if (next < '5') {
		return;
	}
	int i = keep - 1;
	while (i >= 0 && g->d[i] == '9') {
		i--;
	}
	if (i < 0) {
		g->d[0] = '1';
		g->n = 1;
		g->decpt++;
		return;
	}
	g->d[i]++;
	g->n = i + 1;
}

static char Dig_At(const fmtDigits_t *g, int i) {
	return i >= 0 && i < g->n ? g->d[i] : '0';
}

/* the text of a float conversion */
typedef struct {
	char small[512];
	char *p;
	size_t size, len;
} fmtText_t;

/*
 * The buffer for a precision: the small one holds 163 digits after the 309 of DBL_MAX; a larger precision takes a
 * block of the size the runtime asks for, 2 * (precision + 349) bytes, and without it goes on with 163 (ENOMEM).
 */
static int Text_Reserve(fmtText_t *t, int *prec) {
	uint64_t size = 2 * ((uint64_t)*prec + 349);

	t->p = t->small;
	t->size = sizeof(t->small);
	t->len = 0;
	if (*prec <= 163) {
		return 1;
	}
	if (size <= SIZE_MAX && (t->p = malloc((size_t)size))) {
		t->size = (size_t)size;
		return 1;
	}
	t->p = t->small;
	errno = ENOMEM;
	*prec = 163;
	return 0;
}

static void Text_Free(fmtText_t *t) {
	if (t->p != t->small) {
		free(t->p);
	}
}

static void Text_Add(fmtText_t *t, char c) {
	if (t->len + 1 < t->size) {
		t->p[t->len++] = c;
	}
	t->p[t->len] = '\0';
}

static void Text_Str(fmtText_t *t, const char *s) {
	while (*s) {
		Text_Add(t, *s++);
	}
}

/* the exponent with its sign and at least min digits */
static void Text_Exp(fmtText_t *t, char e, int x, int min) {
	char tmp[8];
	int n = 0;

	Text_Add(t, e);
	Text_Add(t, x < 0 ? '-' : '+');
	if (x < 0) {
		x = -x;
	}
	do {
		tmp[n++] = (char)('0' + x % 10);
		x /= 10;
	} while (x);
	while (n < min) {
		tmp[n++] = '0';
	}
	while (n) {
		Text_Add(t, tmp[--n]);
	}
}

static void Text_Fixed(fmtText_t *t, const fmtDigits_t *g, int prec) {
	if (g->decpt <= 0) {
		Text_Add(t, '0');
	}
	for (int i = 0; i < g->decpt; i++) {
		Text_Add(t, Dig_At(g, i));
	}
	if (prec > 0) {
		Text_Add(t, '.');
	}
	for (int i = 0; i < prec; i++) {
		Text_Add(t, Dig_At(g, g->decpt + i));
	}
}

static void Text_Exponent(fmtText_t *t, const fmtDigits_t *g, int prec, char e, int min) {
	Text_Add(t, Dig_At(g, 0));
	if (prec > 0) {
		Text_Add(t, '.');
	}
	for (int i = 1; i <= prec; i++) {
		Text_Add(t, Dig_At(g, i));
	}
	Text_Exp(t, e, g->n ? g->decpt - 1 : 0, min);
}

/*
 * %a of a finite double after the 0x: the leading digit, prec hex digits, p and the exponent. A shorter mantissa is
 * rounded up only when the first digit that is dropped is above 8 (so 0x1.8 and 0x1.88 give 0x1 at precision 0).
 */
static void Text_Hex(fmtText_t *t, uint64_t bits, int prec, int upper) {
	const char *set = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int be = (int)(bits >> 52) & 0x7ff;
	uint64_t m = bits & ((1ull << 52) - 1);
	int lead = be ? 1 : 0;
	int x = !be && !m ? 0 : be ? be - 1023 : -1022;

	if (prec < 13) {
		int shift = 52 - 4 * prec;
		uint64_t next = (m >> (shift - 4)) & 15;
		m >>= shift;
		if (next > 8) {
			m++;
		}
		if (m >> (4 * prec)) {
			lead++;
			m &= (1ull << (4 * prec)) - 1;
		}
		m <<= shift;
	}
	Text_Add(t, set[lead]);
	if (prec > 0) {
		Text_Add(t, '.');
	}
	for (int i = 0; i < prec; i++) {
		Text_Add(t, i < 13 ? set[(m >> (48 - 4 * i)) & 15] : '0');
	}
	Text_Exp(t, upper ? 'P' : 'p', x, 1);
}

/* '#' with precision 0: a point after the first character and the digits that follow it */
static void Text_ForcePoint(fmtText_t *t) {
	size_t i = 0;

	if (t->p[0] != 'e' && t->p[0] != 'E') {
		do {
			i++;
		} while (t->p[i] >= '0' && t->p[i] <= '9');
	}
	if (t->len + 1 >= t->size) {
		return;
	}
	memmove(t->p + i + 1, t->p + i, t->len - i + 1);
	t->p[i] = '.';
	t->len++;
}

/* %g without '#': the zeros at the end of the fraction, and the point when nothing is left after it */
static void Text_CropZeros(fmtText_t *t) {
	char *p = strchr(t->p, '.');

	if (!p) {
		return;
	}
	char *stop = p + 1;
	while (*stop && *stop != 'e' && *stop != 'E') {
		stop++;
	}
	char *q = stop - 1;
	while (*q == '0') {
		q--;
	}
	if (*q == '.') {
		q--;
	}
	memmove(q + 1, stop, strlen(stop) + 1);
	t->len = strlen(t->p);
}

static int Fmt_Float(fmtOut_t *o, fmtSpec_t *s, double v, uint64_t options) {
	fmtDigits_t g;
	fmtText_t t;
	uint64_t bits;
	char type = s->type;
	int upper = type == 'E' || type == 'F' || type == 'G' || type == 'A';
	int hex = type == 'a' || type == 'A';
	int legacy = (options & PO_LEGACY_MSVCRT) != 0;
	int prec = s->prec;

	memcpy(&bits, &v, sizeof(bits));
	int negative = (int)(bits >> 63);
	int special = ((bits >> 52) & 0x7ff) == 0x7ff;
	uint64_t payload = bits & ((1ull << 52) - 1);
	if (prec < 0) {
		prec = hex ? 13 : 6;
	}
	Text_Reserve(&t, &prec);
	if (negative) {
		Text_Add(&t, '-');
	}
	if (special && !legacy) {
		if (!payload) {
			Text_Str(&t, upper ? "INF" : "inf");
		} else if (!(payload >> 51)) {
			Text_Str(&t, upper ? "NAN(SNAN)" : "nan(snan)");
		} else if (negative && payload == 1ull << 51) {
			Text_Str(&t, upper ? "NAN(IND)" : "nan(ind)");
		} else {
			Text_Str(&t, upper ? "NAN" : "nan");
		}
	} else if (hex && !special) {
		Text_Hex(&t, bits, prec, upper);
	} else {
		if (special) {
			/* the MSVCRT spellings go through the digits like a number, rounding included: %.2f of inf is 1.#J */
			const char *str = !payload ? "1#INF"
				: !(payload >> 51) ? "1#SNAN"
				: negative && payload == 1ull << 51 ? "1#IND"
				: "1#QNAN";
			g.n = (int)strlen(str);
			memcpy(g.d, str, (size_t)g.n);
			g.decpt = 1;
		} else {
			g.n = Crt_DoubleDigits(v, g.d, &g.decpt);
		}
		char e = upper ? 'E' : 'e';
		int min = (options & PO_THREE_DIGIT_EXP) ? 3 : 2;
		if (hex) {
			Dig_Round(&g, prec + 1);
			Text_Exponent(&t, &g, prec, upper ? 'P' : 'p', 1);
		} else if (type == 'f' || type == 'F') {
			Dig_Round(&g, g.decpt + prec);
			Text_Fixed(&t, &g, prec);
		} else if (type == 'e' || type == 'E') {
			Dig_Round(&g, prec + 1);
			Text_Exponent(&t, &g, prec, e, min);
		} else {
			int p = prec ? prec : 1;
			Dig_Round(&g, p);
			int x = g.decpt - 1;
			if (x < -4 || x >= p) {
				Text_Exponent(&t, &g, p - 1, e, min);
			} else {
				Text_Fixed(&t, &g, p - g.decpt);
			}
		}
	}
	int point = (s->flags & FL_HASH) && s->prec == 0;
	if (type == 'g' || type == 'G') {
		/* %#g of a number keeps a point, the inf and nan texts stay as they are */
		point = (s->flags & FL_HASH) && (!special || legacy) && !strchr(t.p, '.');
		if (!(s->flags & FL_HASH)) {
			Text_CropZeros(&t);
		}
	}
	if (point) {
		Text_ForcePoint(&t);
	}
	char prefix[4];
	int pl = 0;
	const char *body = t.p;
	if (*body == '-') {
		prefix[pl++] = '-';
		body++;
	} else if (s->flags & FL_PLUS) {
		prefix[pl++] = '+';
	} else if (s->flags & FL_SPACE) {
		prefix[pl++] = ' ';
	}
	/* the 0x of %a; a negative inf or nan gets it too when '#' forces the point (-0x.inf) */
	if (hex && (!special || legacy || (negative && point))) {
		prefix[pl++] = '0';
		prefix[pl++] = upper ? 'X' : 'x';
	}
	prefix[pl] = '\0';
	Out_Field(o, s, prefix, body, (int)strlen(body));
	Text_Free(&t);
	return 0;
}

/* a wide character in the C locale: one byte up to 255 */
static int Fmt_WideByte(unsigned c, char *out) {
	if (c > 255) {
		return 0;
	}
	*out = (char)c;
	return 1;
}

/* %s, %S and %Z; a wide string is padded by its count of characters and fails at the first one without a byte */
static int Fmt_String(fmtOut_t *o, const fmtSpec_t *s, const void *str, int wide, int counted) {
	int len = 0;

	if (counted) {
		/* ANSI_STRING or UNICODE_STRING: the length in bytes, no precision */
		const struct {
			unsigned short length, maximum;
			const void *buffer;
		} *c = str;
		str = NULL;
		if (c && c->buffer) {
			str = c->buffer;
			len = wide ? c->length / 2 : c->length;
		}
	}
	if (!str) {
		str = wide ? (const void *)L"(null)" : "(null)";
		len = 6;
		if (!counted && s->prec >= 0 && s->prec < len) {
			len = s->prec;
		}
		counted = 1;
	}
	if (!counted) {
		while (s->prec < 0 || len < s->prec) {
			if (wide ? !((const wchar_t *)str)[len] : !((const char *)str)[len]) {
				break;
			}
			len++;
		}
	}
	if (!wide) {
		Out_Field(o, s, "", str, len);
		return 0;
	}
	int64_t pad = (int64_t)s->width - len;
	if (!(s->flags & FL_MINUS)) {
		Out_Repeat(o, (s->flags & FL_ZERO) ? '0' : ' ', pad);
	}
	Out_Str(o, "", 0);
	for (int i = 0; i < len; i++) {
		char ch;
		if (!Fmt_WideByte(((const wchar_t *)str)[i], &ch)) {
			errno = EILSEQ;
			return -1;
		}
		Out_Str(o, &ch, 1);
	}
	if (s->flags & FL_MINUS) {
		Out_Repeat(o, ' ', pad);
	}
	return 0;
}

enum { ST_NORMAL, ST_PERCENT, ST_FLAG, ST_WIDTH, ST_DOT, ST_PRECIS, ST_SIZE };

/* one conversion; -1 on an error that ends the format */
static int Fmt_Convert(fmtOut_t *o, fmtSpec_t *s, va_list *ap, uint64_t options) {
	int narrowSize = s->size == SZ_H || s->size == SZ_TCHAR;
	int wideSize = s->size == SZ_L || s->size == SZ_W;

	switch (s->type) {
	case 'd':
	case 'i':
	case 'u':
	case 'o':
	case 'x':
	case 'X': {
		uint64_t v;
		int negative = 0;
		int sign = s->type == 'd' || s->type == 'i';
		if (s->size == SZ_LL || s->size == SZ_I64 || s->size == SZ_J) {
			v = va_arg(*ap, uint64_t);
			if (sign && (int64_t)v < 0) {
				negative = 1;
				v = 0 - v;
			}
		} else {
			uint32_t w = va_arg(*ap, uint32_t);
			if (s->size == SZ_HH) {
				w = sign ? (uint32_t)(int32_t)(int8_t)w : (uint8_t)w;
			} else if (s->size == SZ_H) {
				w = sign ? (uint32_t)(int32_t)(int16_t)w : (uint16_t)w;
			}
			v = w;
			if (sign && (int32_t)w < 0) {
				negative = 1;
				v = 0 - (uint64_t)(int64_t)(int32_t)w;
			}
		}
		Fmt_Integer(o, s, v, negative);
		return 0;
	}
	case 'p':
		/* 8 digits, no '#' prefix */
		s->prec = 8;
		s->flags &= ~FL_HASH;
		s->type = 'X';
		Fmt_Integer(o, s, (uint32_t)(uintptr_t)va_arg(*ap, void *), 0);
		return 0;
	case 'c':
	case 'C': {
		int wide = s->type == 'c' ? wideSize : !narrowSize;
		int c = va_arg(*ap, int);
		char ch = (char)c;
		if (wide && !Fmt_WideByte((unsigned short)c, &ch)) {
			/* nothing is written, the format goes on */
			errno = EILSEQ;
			return 0;
		}
		s->prec = -1;
		Out_Field(o, s, "", &ch, 1);
		return 0;
	}
	case 's':
	case 'S':
		return Fmt_String(o, s, va_arg(*ap, const void *), s->type == 's' ? wideSize : !narrowSize, 0);
	case 'Z':
		return Fmt_String(o, s, va_arg(*ap, const void *), !narrowSize, 1);
	default:
		return Fmt_Float(o, s, va_arg(*ap, double), options);
	}
}

/* the format state machine: a character that does not fit the state is written as it is; 1 on an error */
static int Fmt_Run(fmtOut_t *out, uint64_t options, const char *fmt, va_list ap) {
	fmtOut_t o = *out;
	int legacy = (options & PO_LEGACY_MSVCRT) != 0;
	int state = ST_NORMAL, star = 0, bad = 0;
	fmtSpec_t s = { 0, 0, -1, SZ_NONE, 0 };
	va_list args;

	va_copy(args, ap);
	for (const char *p = fmt; *p && !o.failed && !bad; p++) {
		char c = *p;
		if (state == ST_NORMAL) {
			if (c == '%') {
				state = ST_PERCENT;
				s = (fmtSpec_t) { 0, 0, -1, SZ_NONE, 0 };
			} else {
				Out_Put(&o, p, 1);
			}
			continue;
		}
		if (c == 'n' || (c == 'N' && !legacy)) {
			/* %n (the count output is off in the UCRT) and the near pointer size */
			errno = EINVAL;
			bad = 1;
			break;
		}
		if ((state == ST_PERCENT || state == ST_FLAG) && strchr("-+ #0", c)) {
			s.flags |= c == '-' ? FL_MINUS : c == '+' ? FL_PLUS : c == ' ' ? FL_SPACE : c == '#' ? FL_HASH : FL_ZERO;
			state = ST_FLAG;
			continue;
		}
		if (c == '*' && (state == ST_PERCENT || state == ST_FLAG || state == ST_DOT)) {
			int v = va_arg(args, int);
			if (state == ST_DOT) {
				s.prec = v < 0 ? -1 : v;
				state = ST_PRECIS;
			} else {
				s.width = (uint32_t)v;
				if (v < 0) {
					s.flags |= FL_MINUS;
					s.width = 0u - s.width;
				}
				state = ST_WIDTH;
			}
			star = 1;
			continue;
		}
		if (c >= '0' && c <= '9' && state <= ST_PRECIS) {
			/* a run of digits, also after '*', gives the whole value */
			int v = state >= ST_DOT ? s.prec : (int)s.width;
			if (star || state == ST_PERCENT || state == ST_FLAG || state == ST_DOT) {
				v = 0;
			}
			if (v > (INT_MAX - (c - '0')) / 10) {
				errno = ERANGE;
				bad = 1;
				break;
			}
			v = v * 10 + (c - '0');
			if (state >= ST_DOT) {
				s.prec = v;
			} else {
				s.width = (uint32_t)v;
			}
			state = state >= ST_DOT ? ST_PRECIS : ST_WIDTH;
			star = 0;
			continue;
		}
		star = 0;
		if (c == '.' && state <= ST_WIDTH) {
			s.prec = 0;
			state = ST_DOT;
			continue;
		}
		if (strchr("hlLIjztwT", c) || (legacy && (c == 'F' || c == 'N'))) {
			if (c == 'F' || c == 'N') {
				state = ST_SIZE;
				continue;
			}
			int size = SZ_NONE;
			if (s.size != SZ_NONE && s.size != SZ_I && !(c == 'h' && s.size == SZ_H) && !(c == 'l' && s.size == SZ_L)) {
				errno = EINVAL;
				bad = 1;
				break;
			}
			switch (c) {
			case 'h':
				size = s.size == SZ_H ? SZ_HH : SZ_H;
				break;
			case 'l':
				size = s.size == SZ_L ? SZ_LL : SZ_L;
				break;
			case 'L':
				size = SZ_BIGL;
				break;
			case 'j':
				size = SZ_J;
				break;
			case 'z':
				size = SZ_Z;
				break;
			case 't':
				size = SZ_T;
				break;
			case 'w':
				size = SZ_W;
				break;
			case 'T':
				size = SZ_TCHAR;
				break;
			case 'I':
				size = SZ_I;
				if (p[1] == '3' && p[2] == '2') {
					size = SZ_I32;
					p += 2;
				} else if (p[1] == '6' && p[2] == '4') {
					size = SZ_I64;
					p += 2;
				}
				break;
			}
			s.size = size;
			state = ST_SIZE;
			continue;
		}
		if (strchr("diouxXcCsSZpeEfgGaA", c) || (c == 'F' && !legacy)) {
			s.type = c;
			if (strchr("diouxX", c) && (s.size == SZ_BIGL || s.size == SZ_W || s.size == SZ_TCHAR)) {
				errno = EINVAL;
				bad = 1;
				break;
			}
			if (Fmt_Convert(&o, &s, &args, options) < 0) {
				bad = 1;
			}
			state = ST_NORMAL;
			continue;
		}
		Out_Put(&o, p, 1);
		state = ST_NORMAL;
	}
	va_end(args);
	*out = o;
	return bad;
}

int Crt_VFormat(uint64_t options, char *buf, size_t count, const char *fmt, va_list ap) {
	fmtOut_t o = { buf, count, 0, 0, 0, (options & PO_STANDARD_SNPRINTF) != 0, NULL, 0, { 0 } };

	if (!fmt || (count && !buf)) {
		errno = EINVAL;
		return -1;
	}
	int bad = Fmt_Run(&o, options, fmt, ap);
	if (!buf) {
		return bad ? -1 : (int)o.total;
	}
	if (options & PO_LEGACY_TERMINATION) {
		/* _vsnprintf: -1 when it does not fit, no terminator when it fits exactly */
		if (o.used < count) {
			buf[o.used] = '\0';
		}
		return bad || o.failed || (int64_t)(int)o.total > (int64_t)count ? -1 : (int)o.total;
	}
	if (o.stdSnprintf) {
		if (!count || bad) {
			if (count) {
				buf[0] = '\0';
			}
			return bad ? -1 : (int)o.total;
		}
		buf[o.total < count ? o.total : count - 1] = '\0';
		return (int)o.total;
	}
	if (!count) {
		return -1;
	}
	if (o.used == count) {
		buf[count - 1] = '\0';
		return -2;
	}
	buf[o.used] = '\0';
	return bad ? -1 : (int)o.total;
}

/* fprintf: the text as far as it goes before an error, the count or -1 */
int Crt_VFormatFile(uint64_t options, FILE *f, const char *fmt, va_list ap) {
	fmtOut_t o = { NULL, 0, 0, 0, 0, 1, f, 0, { 0 } };

	if (!fmt || !f) {
		errno = EINVAL;
		return -1;
	}
	int bad = Fmt_Run(&o, options, fmt, ap);
	if (!o.failed) {
		Out_Flush(&o);
	}
	return bad || o.failed ? -1 : (int)o.total;
}

/* the texts of strerror; the codes between and above them are "Unknown error" */
static const char *const fmt_errorTexts[] = {
	"No error",
	"Operation not permitted",
	"No such file or directory",
	"No such process",
	"Interrupted function call",
	"Input/output error",
	"No such device or address",
	"Arg list too long",
	"Exec format error",
	"Bad file descriptor",
	"No child processes",
	"Resource temporarily unavailable",
	"Not enough space",
	"Permission denied",
	"Bad address",
	NULL,
	"Resource device",
	"File exists",
	"Improper link",
	"No such device",
	"Not a directory",
	"Is a directory",
	"Invalid argument",
	"Too many open files in system",
	"Too many open files",
	"Inappropriate I/O control operation",
	NULL,
	"File too large",
	"No space left on device",
	"Invalid seek",
	"Read-only file system",
	"Too many links",
	"Broken pipe",
	"Domain error",
	"Result too large",
	NULL,
	"Resource deadlock avoided",
	NULL,
	"Filename too long",
	"No locks available",
	"Function not implemented",
	"Directory not empty",
	"Illegal byte sequence",
};

const char *Crt_ErrorText(int code) {
	const char *s = NULL;

	if (code >= 0 && code < (int)(sizeof(fmt_errorTexts) / sizeof(fmt_errorTexts[0]))) {
		s = fmt_errorTexts[code];
	}
	return s ? s : "Unknown error";
}
