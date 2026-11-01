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
#define g_ProcTypes (GAME_ARRAY(cProcType, g_ProcTypes))	/* tipo_si_no, tipo_bucle, tipo_nota, ... */
#define g_ProcBuf (GAME_ARRAY(char, g_ProcBuf))	/* 0x400, the number of an identifier */

#define g_ProcRangeChar GAME_GLOBAL(procrange_t, g_ProcRangeChar)
#define g_ProcRangeByte GAME_GLOBAL(procrange_t, g_ProcRangeByte)
#define g_ProcRangeShort GAME_GLOBAL(procrange_t, g_ProcRangeShort)
#define g_ProcRangeWord GAME_GLOBAL(procrange_t, g_ProcRangeWord)

#define IS_BLANK(c) ((c) == '\t' || (c) == ' ')

/* tipo_nota: "C4", "A#3", "Eb-1": the MIDI number of the note */
int Proc_Note(char *dest, const char *src, const void *type, int size) {
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
int Proc_TypeLookup(char *dest, const char *name, const void *data, int size) {
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

/* one "&" field: *pfmt is at the '&'; both pointers move past what was read */
int Proc_ReadField(va_list *ap, const char **psrc, const char **pfmt) {
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
		long *out = va_arg(*ap, long *);
		if (out) {
			*out = v;
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

/* a new type of identifiers; -1 if the name is taken or the table full */
int Proc_DefineType(const char *ident, const void *defin) {
	cProcType *free = NULL;

	CHECK(ident && defin);
	for (int j = 0; j < PROC_TYPES; j++) {
		cProcType *t = &g_ProcTypes[j];
		if (!t->name) {
			if (!free) {
				free = t;
			}
		} else if (!strcmp(t->name, ident)) {
			return -1;
		}
	}
	if (!free) {
		return -1;
	}
	free->convert = NULL;
	free->name = ident;
	free->data = defin;
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
