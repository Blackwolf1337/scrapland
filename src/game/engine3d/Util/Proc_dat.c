/*
 * The value scanner of the engine3d data files, original source Util/Proc_dat.cpp (include/scrap/e3dutil.h).
 *
 * Proc_Scan(src, fmt, ...) reads values like sscanf: "&" then an optional "(type)", an optional precision ".n" or
 * ".*" and the conversion: b byte 0..255, c char -128..127, s short, w word, l long, d unsigned long, f float,
 * g double, t text up to ',' (blanks at its end dropped). A type turns an identifier into its number: the table
 * entries name a list of "IDENT value" lines or a converter. Blanks and ',' in fmt match blanks and ',' in src,
 * other characters themselves. The bad cases go to Err_Printf, most of them as a message box.
 */
#include <string.h>

#include "scrap/e3dutil.h"

#define PROC_TYPES 16

#define IS_BLANK(c) ((c) == '\t' || (c) == ' ')

/* tipo_nota: "C4", "A#3", "Eb-1": the MIDI number of the note */
static int Proc_Note(char *dest, const char *src, const void *type, int size) {
	static const int notes[] = { 9, 11, 0, 2, 4, 5, 7 };	/* A..G */
	int octave;

	CHECK(type == NULL);
	CHECK(size > 5);
	int letter = crt_toupper(*src);
	if (letter < 'A' || letter > 'G') {
		return -1;
	}
	int note = notes[letter - 'A'];
	const char *p = src + 1;
	while (!crt_isdigit(*p) && *p != '-') {
		switch (*p++) {
		case '#':
		case 'X':
		case 'x':
			note++;
			break;
		case 'B':
		case 'b':
			note--;
			break;
		default:
			return -1;
		}
	}
	if (crt_sscanf(p, "%d", &octave) != 1) {
		return -1;
	}
	crt_sprintf(dest, "%d", note + (octave * 3 + 3) * 4);
	return 0;
}

/* the value of name in a NULL ended list of "NAME value" lines, case-insensitive */
static int Proc_TypeLookup(char *dest, const char *name, const void *data, int size) {
	const char *const *list = data;
	size_t n = strlen(name);

	CHECK(list != NULL);
	for (; *list; list++) {
		size_t m = crt_strcspn(*list, " ");
		if (n == m && !crt_strnicmp(*list, name, m)) {
			crt_strncat(dest, *list + 1 + m, size - 1);
			return 0;
		}
	}
	return -1;
}

static char g_ProcBuf[1024];

static procrange_t g_ProcRangeChar = { .lo = -128, .hi = 127 };

static procrange_t g_ProcRangeByte = { .hi = 255 };

static procrange_t g_ProcRangeShort = { .lo = -32768, .hi = 32767 };

static procrange_t g_ProcRangeWord = { .hi = 65535 };

static void *ProcDat_YesNoNames[8];
static void *ProcDat_LoopNames[4];
static void *ProcDat_SignNames[7];
static void *ProcDat_MonoStereoNames[7];
static const char str_TipoSiNo[];
static const char str_TipoBucle[];
static const char str_TipoNota[];
static const char str_TipoSampleSigno[];
static const char str_TipoSampleMnst[];
static cProcType g_ProcTypes[16] = {
	{ .name = (void *)str_TipoSiNo, .data = (void *)ProcDat_YesNoNames },
	{ .name = (void *)str_TipoBucle, .data = (void *)ProcDat_LoopNames },
	{ .name = (void *)str_TipoNota, .convert = (void *)Proc_Note },
	{ .name = (void *)str_TipoSampleSigno, .data = (void *)ProcDat_SignNames },
	{ .name = (void *)str_TipoSampleMnst, .data = (void *)ProcDat_MonoStereoNames },
};

/* one "&" field: *pfmt is at the '&'; both pointers move past what was read */
static int Proc_ReadField(va_list *ap, const char **psrc, const char **pfmt) {
	const char *src = *psrc;
	const char *p = *pfmt;
	int prec = -1;
	const void *data = NULL;
	proctype_t convert = NULL;
	char type[36];
	char ident[36];
	long v;
	double d;
	int n;

	CHECK(*p == '&');
	p++;
	type[0] = '\0';
	ident[0] = '\0';
	if (!*p) {
		return -1;
	}
	if (*p == '(') {
		p++;
		if (!*p) {
			return -1;
		}
		unsigned i = 0;
		for (; *p != ')' && i < 0x20; i++) {
			type[i] = *p++;
			if (!*p) {
				return -1;
			}
		}
		if (i >= 0x21) {
			crt_report_rangecheckfailure();
		}
		type[i] = '\0';
		if (*p != ')' || !*++p) {
			return -1;
		}
		if (type[0]) {
			int j = 0;
			for (; j < PROC_TYPES; j++) {
				if (g_ProcTypes[j].name && !strcmp(g_ProcTypes[j].name, type)) {
					data = g_ProcTypes[j].data;
					convert = g_ProcTypes[j].convert;
					break;
				}
			}
			if (j == PROC_TYPES) {
				Err_Printf(1, "procesa_datos(): Tipo no v\xe1lido (%s)", type);
			}
		}
	}
	if (*p == '.') {
		p++;
		if (!*p) {
			return -1;
		}
		int len;
		if (*p == '*') {
			prec = va_arg(*ap, int);
			len = 1;
		} else if (crt_sscanf(p, "%d%n", &prec, &len) != 1) {
			CHECK(0 && "num_campos == 1");
		}
		p += len;
		if (!*p) {
			return -1;
		}
	}

	if (*p == 't') {
		char *dest = va_arg(*ap, char *);
		int len = 0;
		if (prec < 0) {
			while (src[len] && src[len] != ',') {
				len++;
			}
		} else {
			while (len < prec && src[len] && src[len] != ',') {
				len++;
			}
		}
		while (len > 0 && IS_BLANK(src[len - 1])) {
			len--;
		}
		if (dest) {
			memcpy(dest, src, len);
			dest[len] = '\0';
		}
		*psrc = src + len;
		*pfmt = p + 1;
		return 0;
	}

	if (crt_isalpha(*src) || *src == '_') {
		unsigned i = 0;
		for (; (crt_isalnum(*src) || *src == '_') && i < 0x20; i++) {
			ident[i] = *src++;
		}
		if (i >= 0x21) {
			crt_report_rangecheckfailure();
		}
		ident[i] = '\0';
		while (crt_isalnum(*src) || *src == '_') {
			src++;
		}
	}
	const char *num = src;
	if (type[0] && ident[0]) {
		g_ProcBuf[0] = '\0';
		if ((convert ? convert : Proc_TypeLookup)(g_ProcBuf, ident, data, 0x400) != 0) {
			Err_Printf(0, "Identificador no v\xe1lido: %s", ident);
			return -1;
		}
		num = g_ProcBuf;
	}

	int ok;
	switch (*p) {
	case 'b':
	case 'c':
	case 'l':
	case 's':
	case 'w':
		ok = crt_sscanf(num, "%ld%n", &v, &n) == 1;
		break;
	case 'd':
		ok = crt_sscanf(num, "%lu%n", &v, &n) == 1;
		break;
	case 'f':
	case 'g':
		ok = crt_sscanf(num, "%lg%n", &d, &n) == 1;
		break;
	default:
		ok = 0;
		break;
	}
	if (!ok) {
		Err_Printf(0, "N\xfamero no v\xe1lido: %s", num);
		return -1;
	}
	if (num == src) {
		src += n;
	}

	const procrange_t *range = NULL;
	switch (*p) {
	case 'b':
		range = &g_ProcRangeByte;
		break;
	case 'c':
		range = &g_ProcRangeChar;
		break;
	case 's':
		range = &g_ProcRangeShort;
		break;
	case 'w':
		range = &g_ProcRangeWord;
		break;
	}
	if (range && (v < range->lo || v > range->hi)) {
		Err_Printf(0, "N\xfamero fuera de rango: %ld  (rango v\xe1lido: %ld .. %ld)", v, (long)range->lo, (long)range->hi);
		return -1;
	}

	switch (*p) {
	case 'b':
	case 'c': {
		char *out = va_arg(*ap, char *);
		if (out) {
			*out = (char)v;
		}
		break;
	}
	case 'd':
	case 'l': {
		int32_t *out = va_arg(*ap, int32_t *);	/* the long of the original: 32 bits */
		if (out) {
			*out = (int32_t)v;
		}
		break;
	}
	case 'f': {
		float *out = va_arg(*ap, float *);
		if (out) {
			*out = (float)d;
		}
		break;
	}
	case 'g': {
		double *out = va_arg(*ap, double *);
		if (out) {
			*out = d;
		}
		break;
	}
	case 's':
	case 'w': {
		int16_t *out = va_arg(*ap, int16_t *);
		if (out) {
			*out = (int16_t)v;
		}
		break;
	}
	}
	*psrc = src;
	*pfmt = p + 1;
	return 0;
}

static const char *Proc_SkipBlanks(const char *s) {
	while (IS_BLANK(*s)) {
		s++;
	}
	return s;
}

/* the number of values read */
int Proc_Scan(const char *src, const char *fmt, ...) {
	va_list ap;
	int count = 0;

	CHECK(src != NULL);
	CHECK(fmt != NULL);
	va_start(ap, fmt);
	src = Proc_SkipBlanks(src);
	while (*fmt && *src) {
		switch (*fmt) {
		case '\t':
		case ' ':
			if (!IS_BLANK(*src)) {
				goto done;
			}
			src = Proc_SkipBlanks(src);
			fmt++;
			break;
		case '&':
			if (Proc_ReadField(&ap, &src, &fmt) != 0) {
				goto done;
			}
			count++;
			break;
		case ',':
			src = Proc_SkipBlanks(src);
			if (*src != ',') {
				goto done;
			}
			fmt++;
			src = Proc_SkipBlanks(src + 1);
			break;
		default:
			if (*src != *fmt) {
				goto done;
			}
			src++;
			fmt++;
			break;
		}
	}
done:
	va_end(ap);
	return count;
}

static const char str_NO0[] = "NO 0";
static const char str_OFF0[] = "OFF 0";
static const char str_FALSE0[] = "FALSE 0";
static const char str_SI1[] = "SI 1";
static const char str_ON1[] = "ON 1";
static const char str_YES1[] = "YES 1";
static const char str_TRUE1[] = "TRUE 1";
static const char str_LOCAL1[] = "LOCAL 1";
static const char str_GLOBAL2[] = "GLOBAL 2";
static const char str_U0[] = "U 0";
static const char str_SINSIGNO0[] = "SIN_SIGNO 0";
static const char str_UNSIGNED0[] = "UNSIGNED 0";
static const char str_S1[] = "S 1";
static const char str_CONSIGNO1[] = "CON_SIGNO 1";
static const char str_SIGNED1[] = "SIGNED 1";
static const char str_MN0[] = "MN 0";
static const char str_MONO0[] = "MONO 0";
static const char str_ST1[] = "ST 1";
static const char str_ESTEREO1[] = "ESTEREO 1";
static const char str_STEREO1[] = "STEREO 1";
static const char str_TipoSiNo[] = "tipo_si_no";
static const char str_TipoBucle[] = "tipo_bucle";
static const char str_TipoNota[] = "tipo_nota";
static const char str_TipoSampleSigno[] = "tipo_sample_signo";
static const char str_TipoSampleMnst[] = "tipo_sample_mnst";

static void *ProcDat_YesNoNames[8] = {
	(void *)str_NO0, (void *)str_OFF0, (void *)str_FALSE0, (void *)str_SI1,
	(void *)str_ON1, (void *)str_YES1, (void *)str_TRUE1,
};

static void *ProcDat_LoopNames[4] = { (void *)str_NO0, (void *)str_LOCAL1, (void *)str_GLOBAL2 };

static void *ProcDat_SignNames[7] = {
	(void *)str_U0, (void *)str_SINSIGNO0, (void *)str_UNSIGNED0, (void *)str_S1,
	(void *)str_CONSIGNO1, (void *)str_SIGNED1,
};

static void *ProcDat_MonoStereoNames[7] = { (void *)str_MN0, (void *)str_MONO0, (void *)str_ST1, (void *)str_ESTEREO1, (void *)str_STEREO1 };

