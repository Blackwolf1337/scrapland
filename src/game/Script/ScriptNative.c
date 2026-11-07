/*
 * The native layer of the script ports (include/scrap/scriptnative.h, docs/objlib-plan.md): C values with the hash,
 * dict order, repr and literal reader of Python 1.5.2, the errors a native function raises, the engine reads with
 * their test seams and the mirror of module state in the module dict.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"

/* ---- values ---- */

/* the key of a free slot of a dict */
#define NV_FREE 0xff
/* the digits of a long int this file works with (base 2^15, 3840 bits): a longer one has no hash */
#define NATIVE_DIGITS 256

static void Native_Clear(nativeVal_t *v, byte type) {
	v->type = type;
	v->n = 0;
	v->max = 0;
	v->text = NULL;
	v->items = NULL;
}

void Native_SetNone(nativeVal_t *v) {
	Native_Clear(v, NV_NONE);
}

void Native_SetInt(nativeVal_t *v, long i) {
	Native_Clear(v, NV_INT);
	v->i = i;
}

void Native_SetFloat(nativeVal_t *v, double f) {
	Native_Clear(v, NV_FLOAT);
	v->f = f;
}

void Native_SetVec(nativeVal_t *v, double x, double y, double z) {
	Native_Clear(v, NV_VEC);
	v->v[0] = x;
	v->v[1] = y;
	v->v[2] = z;
}

void Native_SetStr(nativeVal_t *v, const char *s) {
	Native_Clear(v, NV_STR);
	Str_Copy(v->s, s, sizeof(v->s));
	v->n = (int)strlen(v->s);
}

void Native_SetText(nativeVal_t *v, const char *s, int len) {
	Native_Clear(v, NV_STR);
	char *t = v->s;
	if (len >= NATIVE_TEXT) {
		v->s[0] = '\0';
		t = v->text = Mem_Alloc((uint32_t)len + 1);
	}
	memcpy(t, s, (size_t)len);
	t[len] = '\0';
	v->n = len;
}

void Native_SetLong(nativeVal_t *v, const char *digits, int len) {
	Native_SetText(v, digits, len);
	v->type = NV_LONG;
}

const char *Native_Text(const nativeVal_t *v) {
	if (v->type != NV_STR && v->type != NV_LONG) {
		return "";
	}
	return v->text ? v->text : v->s;
}

void Native_SetEmpty(nativeVal_t *v, nativeType_t type) {
	Native_Clear(v, (byte)type);
}

nativeVal_t *Native_Append(nativeVal_t *list) {
	if (list->n == list->max) {
		list->max = list->max ? list->max * 2 : 4;
		list->items = Mem_Realloc(list->items, (uint32_t)(list->max * sizeof(nativeVal_t)));
	}
	nativeVal_t *v = &list->items[list->n++];
	Native_SetNone(v);
	return v;
}

void Native_Free(nativeVal_t *v) {
	if (v->type == NV_LIST || v->type == NV_TUPLE) {
		for (int i = 0; i < v->n; i++) {
			Native_Free(&v->items[i]);
		}
	} else if (v->type == NV_DICT) {
		for (int i = 0; i < v->max * 2; i++) {
			if (v->items[i].type != NV_FREE) {
				Native_Free(&v->items[i]);
			}
		}
	}
	Mem_Free(v->text);
	Mem_Free(v->items);
	Native_Clear(v, NV_NONE);
}

byte Native_IsStr(const nativeVal_t *v, const char *text) {
	return v->type == NV_STR && !strcmp(Native_Text(v), text);
}

/* a vector is the tuple of three floats the engine gives: its count and items as a tuple's (the item made in *tmp) */
static byte Native_SeqType(const nativeVal_t *v) {
	return v->type == NV_VEC ? NV_TUPLE : v->type;
}

static int Native_Count(const nativeVal_t *v) {
	return v->type == NV_VEC ? 3 : v->n;
}

static const nativeVal_t *Native_Item(const nativeVal_t *v, int i, nativeVal_t *tmp) {
	if (v->type == NV_VEC) {
		Native_SetFloat(tmp, v->v[i]);
		return tmp;
	}
	return &v->items[i];
}

/* the digits of a long int from its decimal text, the lowest first: their count, -1 for more than NATIVE_DIGITS */
static int Native_LongDigits(const char *text, unsigned short *dig) {
	int n = 0;
	for (const char *p = text + (*text == '-'); *p; p++) {
		unsigned long carry = (unsigned long)(*p - '0');
		for (int i = 0; i < n; i++) {
			carry += (unsigned long)dig[i] * 10;
			dig[i] = (unsigned short)(carry & 0x7fff);
			carry >>= 15;
		}
		if (carry) {
			if (n == NATIVE_DIGITS) {
				return -1;
			}
			dig[n++] = (unsigned short)carry;
		}
	}
	return n;
}

/* PyLong_AsDouble of the long int text (inf beyond the digits) */
static double Native_LongDouble(const char *text) {
	unsigned short dig[NATIVE_DIGITS];
	int n = Native_LongDigits(text, dig);
	double x = n < 0 ? HUGE_VAL : 0.0;
	while (--n >= 0) {
		x = x * 32768.0 + (double)dig[n];
	}
	return *text == '-' ? -x : x;
}

static byte Native_IsNum(const nativeVal_t *v) {
	return v->type == NV_INT || v->type == NV_FLOAT || v->type == NV_LONG;
}

/* numbers as PyObject_Compare coerces them: an int with a long as two longs, a float with any number as floats */
static byte Native_NumEqual(const nativeVal_t *a, const nativeVal_t *b) {
	if (a->type == NV_LONG || b->type == NV_LONG) {
		const nativeVal_t *l = a->type == NV_LONG ? a : b, *o = a->type == NV_LONG ? b : a;
		char t[0x20];
		switch (o->type) {
		case NV_LONG:
			return l->n == o->n && !memcmp(Native_Text(l), Native_Text(o), (size_t)l->n);
		case NV_INT:
			snprintf(t, sizeof(t), "%ld", o->i);
			return !strcmp(t, Native_Text(l));
		}
		return Native_LongDouble(Native_Text(l)) == o->f;
	}
	if (a->type == NV_INT && b->type == NV_INT) {
		return a->i == b->i;
	}
	return (a->type == NV_INT ? (double)a->i : a->f) == (b->type == NV_INT ? (double)b->i : b->f);
}

byte Native_Equal(const nativeVal_t *a, const nativeVal_t *b) {
	if (Native_IsNum(a) && Native_IsNum(b)) {
		return Native_NumEqual(a, b);
	}
	if (Native_SeqType(a) != Native_SeqType(b)) {
		return 0;
	}
	switch (Native_SeqType(a)) {
	case NV_NONE:
		return 1;
	case NV_STR:
		return a->n == b->n && !memcmp(Native_Text(a), Native_Text(b), (size_t)a->n);
	case NV_LIST:
	case NV_TUPLE:
		if (Native_Count(a) != Native_Count(b)) {
			return 0;
		}
		for (int i = 0; i < Native_Count(a); i++) {
			nativeVal_t ta, tb;
			if (!Native_Equal(Native_Item(a, i, &ta), Native_Item(b, i, &tb))) {
				return 0;
			}
		}
		return 1;
	case NV_DICT: {
		const nativeVal_t *k, *x, *y;
		int pos = 0;
		if (a->n != b->n) {
			return 0;
		}
		while (Native_DictNext(a, &pos, &k, &x)) {
			if (!(y = Native_DictGet(b, k)) || !Native_Equal(x, y)) {
				return 0;
			}
		}
		return 1;
	}
	}
	return 0;
}

/* ---- hash and dicts (dictobject.c of 1.5.2, without deleted keys) ---- */

/* long_hash over the digits: its "32-bit circular shift" on a C long, as the build's Python computes it */
static long Native_DigitHash(const unsigned short *dig, int n, byte neg) {
	long x = 0;
	while (--n >= 0) {
		x = (long)((((unsigned long)x << 15) & ~0x7ffful) | ((unsigned long)(x >> 17) & 0x7fff));
		x = (long)((unsigned long)x + dig[n]);
	}
	if (neg) {
		x = (long)(0ul - (unsigned long)x);
	}
	return x == -1 ? -2 : x;
}

/* float_hash: an integral float beyond 32 bits through the long int PyLong_FromDouble makes of it */
static long Native_FloatHash(double v) {
	double intpart, fractpart = modf(v, &intpart);
	unsigned long x;
	if (fractpart == 0.0) {
		if (intpart > 0x7fffffffL || -intpart > 0x7fffffffL) {
			unsigned short dig[NATIVE_DIGITS];
			int expo, n = 0;
			double frac = frexp(v < 0.0 ? -v : v, &expo);
			if (expo > 0) {
				n = (expo - 1) / 15 + 1;
				frac = ldexp(frac, (expo - 1) % 15 + 1);
				for (int i = n; --i >= 0;) {
					long bits = (long)frac;
					dig[i] = (unsigned short)bits;
					frac = ldexp(frac - (double)bits, 15);
				}
			}
			return Native_DigitHash(dig, n, v < 0.0);
		}
		x = (unsigned long)(long)intpart;
	} else {
		int expo;
		fractpart = frexp(fractpart, &expo) * 2147483648.0;
		long hipart = (long)fractpart;
		fractpart = (fractpart - (double)hipart) * 2147483648.0;
		x = (unsigned long)hipart + (unsigned long)(long)fractpart + (unsigned long)(long)intpart +
			((unsigned long)(long)expo << 15);
	}
	return (long)x == -1 ? -2 : (long)x;
}

byte Native_Hash(const nativeVal_t *v, long *out) {
	unsigned long x;
	switch (Native_SeqType(v)) {
	case NV_NONE:
		/* OBJLIB 9: PyObject_Hash of a type without hash and compare, the address of the object */
		*out = (long)(intptr_t)Py_None;
		return 1;
	case NV_INT:
		*out = v->i == -1 ? -2 : v->i;
		return 1;
	case NV_FLOAT:
		*out = Native_FloatHash(v->f);
		return 1;
	case NV_LONG: {
		unsigned short dig[NATIVE_DIGITS];
		int n = Native_LongDigits(Native_Text(v), dig);
		*out = Native_DigitHash(dig, n, *Native_Text(v) == '-');
		return n >= 0;
	}
	case NV_STR: {
		/* string_hash: the first byte (the 0 after an empty text) shifted, then every byte */
		const unsigned char *p = (const unsigned char *)Native_Text(v);
		x = (unsigned long)p[0] << 7;
		for (int i = 0; i < v->n; i++) {
			x = (1000003ul * x) ^ p[i];
		}
		x ^= (unsigned long)v->n;
		break;
	}
	case NV_TUPLE:
		x = 0x345678ul;
		for (int i = 0; i < Native_Count(v); i++) {
			nativeVal_t tmp;
			long y;
			if (!Native_Hash(Native_Item(v, i, &tmp), &y)) {
				return 0;
			}
			x = (1000003ul * x) ^ (unsigned long)y;
		}
		x ^= (unsigned long)Native_Count(v);
		break;
	default:
		return 0;
	}
	*out = (long)x == -1 ? -2 : (long)x;
	return 1;
}

/* the polynomial of a table of size slots (polys of dictobject.c) */
static unsigned Native_Poly(int size) {
	static const unsigned polys[] = { 4 + 3, 8 + 3, 16 + 3, 32 + 5, 64 + 3, 128 + 3, 256 + 29, 512 + 17, 1024 + 9,
		2048 + 5, 4096 + 83, 8192 + 27, 16384 + 43, 32768 + 3, 65536 + 45, 131072 + 9, 262144 + 39, 524288 + 39,
		1048576 + 9, 2097152 + 5, 4194304 + 3, 8388608 + 33, 16777216 + 27, 33554432 + 9, 67108864 + 71,
		134217728 + 39, 268435456 + 9, 536870912 + 5, 1073741824 + 83 };
	int i = 0;
	while ((4 << i) < size) {
		i++;
	}
	return polys[i];
}

/* the slot that holds a key equal to key (same hash and equal), else the free slot it goes to: lookdict */
static int Native_Look(const nativeVal_t *d, const nativeVal_t *key, long hash) {
	unsigned mask = (unsigned)d->max - 1, i = (unsigned)~hash & mask, j = i;
	unsigned incr = (unsigned)(((unsigned long)hash ^ ((unsigned long)hash >> 3)) & mask);
	if (!incr) {
		incr = mask;
	}
	for (;;) {
		const nativeVal_t *k = &d->items[2 * j];
		long h;
		if (k->type == NV_FREE || (Native_Hash(k, &h) && h == hash && Native_Equal(k, key))) {
			return (int)j;
		}
		/* cycle through GF(2^n) - {0} */
		j = (i + incr) & mask;
		incr <<= 1;
		if (incr > mask) {
			incr ^= Native_Poly(d->max);
		}
	}
}

/* a table of size free slots */
static void Native_Table(nativeVal_t *d, int size) {
	d->items = Mem_Alloc((uint32_t)(size * 2 * sizeof(nativeVal_t)));
	d->max = size;
	for (int i = 0; i < size; i++) {
		Native_Clear(&d->items[2 * i], NV_FREE);
		Native_Clear(&d->items[2 * i + 1], NV_NONE);
	}
}

/* dictresize: the smallest power of 2 from 4 above minused, the keys put in again in slot order */
static void Native_Resize(nativeVal_t *d, int minused) {
	nativeVal_t *old = d->items;
	int oldMax = d->max, size = 4;
	while (size <= minused) {
		size <<= 1;
	}
	Native_Table(d, size);
	for (int i = 0; i < oldMax; i++) {
		long h;
		if (old[2 * i].type != NV_FREE && Native_Hash(&old[2 * i], &h)) {
			int j = Native_Look(d, &old[2 * i], h);
			d->items[2 * j] = old[2 * i];
			d->items[2 * j + 1] = old[2 * i + 1];
		}
	}
	Mem_Free(old);
}

byte Native_DictSet(nativeVal_t *d, nativeVal_t *key, nativeVal_t *value) {
	long hash;
	if (!Native_Hash(key, &hash)) {
		Native_Free(key);
		Native_Free(value);
		return 0;
	}
	/* PyDict_SetItem: from 2/3 filled the table grows first, also when the key is there already */
	if (d->n * 3 >= d->max * 2) {
		Native_Resize(d, d->n * 2);
	}
	nativeVal_t *k = &d->items[2 * Native_Look(d, key, hash)];
	if (k->type == NV_FREE) {
		*k = *key;
		d->n++;
	} else {
		Native_Free(key);
	}
	Native_Free(&k[1]);
	k[1] = *value;
	Native_SetNone(key);
	Native_SetNone(value);
	return 1;
}

nativeVal_t *Native_DictGet(const nativeVal_t *d, const nativeVal_t *key) {
	long hash;
	if (!d->max || !Native_Hash(key, &hash)) {
		return NULL;
	}
	nativeVal_t *k = &d->items[2 * Native_Look(d, key, hash)];
	return k->type == NV_FREE ? NULL : &k[1];
}

byte Native_DictNext(const nativeVal_t *d, int *pos, const nativeVal_t **key, const nativeVal_t **value) {
	while (*pos < d->max && d->items[2 * *pos].type == NV_FREE) {
		(*pos)++;
	}
	if (*pos >= d->max) {
		return 0;
	}
	*key = &d->items[2 * *pos];
	*value = &d->items[2 * *pos + 1];
	(*pos)++;
	return 1;
}

/* ---- texts of values ---- */

void Native_BufInit(nativeBuf_t *b) {
	b->p = b->fixed;
	b->len = 0;
	b->max = sizeof(b->fixed);
	b->fixed[0] = '\0';
}

void Native_BufAdd(nativeBuf_t *b, const char *s, int len) {
	if (len < 0) {
		len = (int)strlen(s);
	}
	if (b->len + len + 1 > b->max) {
		int max = b->max * 2;
		while (b->len + len + 1 > max) {
			max *= 2;
		}
		if (b->p == b->fixed) {
			b->p = Mem_Alloc((uint32_t)max);
			memcpy(b->p, b->fixed, (size_t)b->len);
		} else {
			b->p = Mem_Realloc(b->p, (uint32_t)max);
		}
		b->max = max;
	}
	memcpy(b->p + b->len, s, (size_t)len);
	b->len += len;
	b->p[b->len] = '\0';
}

void Native_BufFree(nativeBuf_t *b) {
	if (b->p != b->fixed) {
		Mem_Free(b->p);
	}
	Native_BufInit(b);
}

/* PyFloat_AsString (12 digits, ".0" after a text of digits alone), or the 17 digits of check() */
static void Native_FloatRepr(nativeBuf_t *b, double f, nativeRepr_t mode) {
	char t[0x40];
	if (mode == NR_CHECK) {
		snprintf(t, sizeof(t), "%.17g", f);
		Native_BufAdd(b, strstr(t, "nan") ? "nan" : t, -1);
		return;
	}
	snprintf(t, sizeof(t), "%.12g", f);
	const char *c = t + (*t == '-');
	while (isdigit((unsigned char)*c)) {
		c++;
	}
	Native_BufAdd(b, t, -1);
	if (!*c) {
		Native_BufAdd(b, ".0", 2);
	}
}

/* string_repr: single quotes, double ones when the text has a single and no double quote (strchr, so up to its first
 * 0 byte); the quote and the backslash escaped, a byte below 32 or from 127 as \ooo */
static void Native_TextRepr(nativeBuf_t *b, const char *s, int n) {
	char q = strchr(s, '\'') && !strchr(s, '"') ? '"' : '\'';
	int plain = 0;
	Native_BufAdd(b, &q, 1);
	for (int i = 0; i <= n; i++) {
		unsigned char c = i < n ? (unsigned char)s[i] : (unsigned char)q;
		if (i < n && c != q && c != '\\' && c >= ' ' && c < 0177) {
			plain++;
			continue;
		}
		Native_BufAdd(b, s + i - plain, plain);
		plain = 0;
		if (i < n) {
			char e[8];
			if (c == q || c == '\\') {
				e[0] = '\\';
				e[1] = (char)c;
				e[2] = '\0';
			} else {
				snprintf(e, sizeof(e), "\\%03o", c);
			}
			Native_BufAdd(b, e, -1);
		}
	}
	Native_BufAdd(b, &q, 1);
}

void Native_Repr(nativeBuf_t *b, const nativeVal_t *v, nativeRepr_t mode) {
	/* items of containers as repr; check() walks only tuples */
	nativeRepr_t sub = mode == NR_CHECK ? NR_CHECK : NR_REPR;
	char t[0x20];
	switch (v->type) {
	case NV_NONE:
		Native_BufAdd(b, "None", 4);
		return;
	case NV_INT:
		snprintf(t, sizeof(t), "%ld", v->i);
		Native_BufAdd(b, t, -1);
		return;
	case NV_FLOAT:
		Native_FloatRepr(b, v->f, mode);
		return;
	case NV_LONG:
		Native_BufAdd(b, Native_Text(v), v->n);
		Native_BufAdd(b, "L", 1);
		return;
	case NV_STR:
		if (mode == NR_STR) {
			Native_BufAdd(b, Native_Text(v), v->n);
		} else {
			Native_TextRepr(b, Native_Text(v), v->n);
		}
		return;
	case NV_VEC:
	case NV_TUPLE:
	case NV_LIST: {
		byte list = v->type == NV_LIST;
		Native_BufAdd(b, list ? "[" : "(", 1);
		for (int i = 0; i < Native_Count(v); i++) {
			nativeVal_t tmp;
			if (i) {
				Native_BufAdd(b, ", ", 2);
			}
			Native_Repr(b, Native_Item(v, i, &tmp), list ? NR_REPR : sub);
		}
		if (!list && Native_Count(v) == 1 && mode != NR_CHECK) {
			Native_BufAdd(b, ",", 1);
		}
		Native_BufAdd(b, list ? "]" : ")", 1);
		return;
	}
	case NV_DICT: {
		const nativeVal_t *k, *x;
		int pos = 0;
		byte first = 1;
		Native_BufAdd(b, "{", 1);
		while (Native_DictNext(v, &pos, &k, &x)) {
			if (!first) {
				Native_BufAdd(b, ", ", 2);
			}
			first = 0;
			Native_Repr(b, k, NR_REPR);
			Native_BufAdd(b, ": ", 2);
			Native_Repr(b, x, NR_REPR);
		}
		Native_BufAdd(b, "}", 1);
		return;
	}
	}
}

/*
 * The literal reader: Script_Literal of ScriptValue.c (which see for the rules of the tokenizer, parsenumber and
 * parsestr) on native values, the same texts taken. OBJLIB 6: Script_Literal goes over to this one when the data
 * paths are native (its constants are shared and interned as the compiler does, which Python code can see).
 */
#define NLIT_DEPTH 32

typedef struct {
	const char *p;
	int depth;
} nativeLit_t;

static byte NLit_Item(nativeLit_t *l, nativeVal_t *out);

static void NLit_Blanks(nativeLit_t *l) {
	while (*l->p == ' ' || *l->p == '\t') {
		l->p++;
	}
}

/* the end of a token: a blank, a comma, a colon, a closing bracket or the end of the text */
static byte NLit_End(char c) {
	return c == '\0' || c == ' ' || c == '\t' || c == ',' || c == ':' || c == ')' || c == ']' || c == '}';
}

static byte NLit_Digit(char c) {
	return c >= '0' && c <= '9';
}

/* 0, 123, 0.5, 12., 1e-05, 2.5E+12, 7L, negative after a '-' (as UNARY_NEGATIVE makes it) */
static byte NLit_Number(nativeLit_t *l, byte neg, nativeVal_t *out) {
	const char *s = l->p, *e = s;
	char buf[66];
	byte isFloat = 0, isLong = 0;
	if (*e == '0') {
		e++;
	} else {
		while (NLit_Digit(*e)) {
			e++;
		}
	}
	if (*e == 'l' || *e == 'L') {
		isLong = 1;
		e++;
	} else {
		if (*e == '.') {
			isFloat = 1;
			for (e++; NLit_Digit(*e); e++) {
			}
		}
		if ((*e == 'e' || *e == 'E') && (isFloat || *s != '0')) {
			const char *x = e + 1;
			if (*x == '+' || *x == '-') {
				x++;
			}
			if (!NLit_Digit(*x)) {
				return 0;
			}
			while (NLit_Digit(*x)) {
				x++;
			}
			isFloat = 1;
			e = x;
		}
	}
	if (!NLit_End(*e) || e - s >= 64) {
		return 0;
	}
	/* a '-' in front of the digits for a long int other than 0 */
	byte sign = isLong && neg && !(e - s == 2 && *s == '0');
	buf[0] = '-';
	memcpy(buf + sign, s, (size_t)(e - s));
	buf[sign + (e - s)] = '\0';
	l->p = e;
	if (isFloat) {
		Native_SetFloat(out, neg ? -atof(buf) : atof(buf));
	} else if (isLong) {
		Native_SetLong(out, buf, sign + (int)(e - s) - 1);
	} else {
		char *end;
		errno = 0;
		long x = strtol(buf, &end, 10);
		if (*end || errno) {
			return 0;
		}
		Native_SetInt(out, neg ? -x : x);
	}
	return 1;
}

/* a byte the tokenizer and parsestr keep as it is inside a string without escapes */
static byte NLit_Plain(unsigned char c) {
	return c >= 0x20 && c != 0x7f && c != '\\';
}

/* the escapes of parsestr in s[0:len] (no newline after a backslash) into out: its length */
static int NLit_Unescape(const char *s, int len, char *out) {
	const char *end = s + len;
	char *p = out;
	while (s < end) {
		if (*s != '\\') {
			*p++ = *s++;
			continue;
		}
		s++;
		switch (*s++) {
		case '\\': *p++ = '\\'; break;
		case '\'': *p++ = '\''; break;
		case '"': *p++ = '"'; break;
		case 'b': *p++ = '\b'; break;
		case 'f': *p++ = '\014'; break;
		case 't': *p++ = '\t'; break;
		case 'n': *p++ = '\n'; break;
		case 'r': *p++ = '\r'; break;
		case 'v': *p++ = '\013'; break;
		case 'a': *p++ = '\007'; break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
			int c = s[-1] - '0';
			if ('0' <= *s && *s <= '7') {
				c = (c << 3) + *s++ - '0';
				if ('0' <= *s && *s <= '7') {
					c = (c << 3) + *s++ - '0';
				}
			}
			*p++ = (char)c;
			break;
		}
		case 'x':
			/* every hex digit that follows, the last 8 bits kept */
			if (isxdigit((unsigned char)*s)) {
				unsigned int x = 0;
				do {
					int c = (unsigned char)*s++;
					x = (x << 4) & ~0xFu;
					x += isdigit(c) ? (unsigned int)(c - '0') : islower(c) ? (unsigned int)(10 + c - 'a') :
						(unsigned int)(10 + c - 'A');
				} while (isxdigit((unsigned char)*s));
				*p++ = (char)x;
				break;
			}
			/* fall through */
		default:
			*p++ = '\\';
			*p++ = s[-1];
			break;
		}
	}
	return (int)(p - out);
}

/* 'text' or "text", with backslash escapes */
static byte NLit_String(nativeLit_t *l, nativeVal_t *out) {
	char q = *l->p;
	const char *s = l->p + 1, *e = s;
	byte escapes = 0;
	if (s[0] == q && s[1] == q) {
		return 0;
	}
	while (*e != q) {
		if (*e == '\\' && (e[1] == '\\' || NLit_Plain((unsigned char)e[1]))) {
			/* the tokenizer takes the byte after a backslash as it is, a quote too */
			escapes = 1;
			e += 2;
			continue;
		}
		if (!NLit_Plain((unsigned char)*e)) {
			return 0;
		}
		e++;
	}
	if (!NLit_End(e[1])) {
		return 0;
	}
	l->p = e + 1;
	int len = (int)(e - s);
	if (!escapes) {
		Native_SetText(out, s, len);
		return 1;
	}
	char tmp[NATIVE_TEXT], *t = len <= (int)sizeof(tmp) ? tmp : Mem_Alloc((uint32_t)len);
	Native_SetText(out, t, NLit_Unescape(s, len, t));
	if (t != tmp) {
		Mem_Free(t);
	}
	return 1;
}

/* the items up to close (')' or ']', '\0' at the top) appended to the list: *commas tells whether a comma came */
static byte NLit_Items(nativeLit_t *l, char close, nativeVal_t *list, byte *commas) {
	*commas = 0;
	NLit_Blanks(l);
	while (*l->p != close) {
		if (!NLit_Item(l, Native_Append(list))) {
			return 0;
		}
		NLit_Blanks(l);
		if (*l->p == ',') {
			*commas = 1;
			l->p++;
			NLit_Blanks(l);
		} else if (*l->p != close) {
			return 0;
		}
	}
	return 1;
}

/* the list of items as eval makes it of them: (x) is x, (x,) and () are tuples */
static void NLit_Tuple(nativeVal_t *list, byte commas) {
	if (list->n == 1 && !commas) {
		nativeVal_t one = list->items[0];
		list->n = 0;
		Native_Free(list);
		*list = one;
	} else {
		list->type = NV_TUPLE;
	}
}

/* {key: value, ...}: each pair set as STORE_SUBSCR does (an unhashable key fails here and raises in eval) */
static byte NLit_Dict(nativeLit_t *l, nativeVal_t *out) {
	byte ok = 1;
	Native_SetEmpty(out, NV_DICT);
	l->p++;
	l->depth++;
	NLit_Blanks(l);
	while (ok && *l->p != '}') {
		nativeVal_t k, v;
		Native_SetNone(&k);
		Native_SetNone(&v);
		ok = NLit_Item(l, &k);
		if (ok) {
			NLit_Blanks(l);
			ok = *l->p == ':';
		}
		if (ok) {
			l->p++;
			NLit_Blanks(l);
			ok = NLit_Item(l, &v);
		}
		if (!ok) {
			Native_Free(&k);
			Native_Free(&v);
		} else if ((ok = Native_DictSet(out, &k, &v))) {
			NLit_Blanks(l);
			if (*l->p == ',') {
				l->p++;
				NLit_Blanks(l);
			} else if (*l->p != '}') {
				ok = 0;
			}
		}
	}
	l->depth--;
	return ok && NLit_End(*++l->p);
}

/* one value; on failure *out is left for the caller to free */
static byte NLit_Item(nativeLit_t *l, nativeVal_t *out) {
	char c = *l->p;
	if (c == '\'' || c == '"') {
		return NLit_String(l, out);
	}
	if (!strncmp(l->p, "None", 4) && NLit_End(l->p[4])) {
		l->p += 4;
		Native_SetNone(out);
		return 1;
	}
	if (NLit_Digit(c)) {
		return NLit_Number(l, 0, out);
	}
	if (c == '-') {
		l->p++;
		NLit_Blanks(l);
		return NLit_Digit(*l->p) && NLit_Number(l, 1, out);
	}
	if (c == '{' && l->depth < NLIT_DEPTH) {
		return NLit_Dict(l, out);
	}
	if ((c != '[' && c != '(') || l->depth >= NLIT_DEPTH) {
		return 0;
	}
	char close = c == '[' ? ']' : ')';
	byte commas;
	Native_SetEmpty(out, NV_LIST);
	l->p++;
	l->depth++;
	byte ok = NLit_Items(l, close, out, &commas);
	l->depth--;
	if (!ok || !NLit_End(*++l->p)) {
		return 0;
	}
	if (close == ')') {
		NLit_Tuple(out, commas);
	}
	return 1;
}

byte Native_Literal(const char *s, nativeVal_t *out) {
	nativeLit_t l = { .p = s, .depth = 0 };
	byte commas;
	Native_SetEmpty(out, NV_LIST);
	if (!s || *s == ' ' || *s == '\t' || *s == '\0' || !NLit_Items(&l, '\0', out, &commas)) {
		Native_Free(out);
		return 0;
	}
	/* a, b is a tuple, a alone is a */
	NLit_Tuple(out, commas);
	return 1;
}

/* ---- errors ---- */

byte Native_Raise(nativeError_t kind, const char *fmt, ...) {
	/* OBJLIB 7: the Python exception of the same name */
	static PyObject **const types[] = {
		[NE_INDEX] = &PyExc_IndexError,
		[NE_VALUE] = &PyExc_ValueError,
		[NE_TYPE] = &PyExc_TypeError,
		[NE_ATTRIBUTE] = &PyExc_AttributeError,
		[NE_SYSTEM] = &PyExc_SystemError,
	};
	char text[0x200];
	va_list va;
	va_start(va, fmt);
	vsnprintf(text, sizeof(text), fmt, va);
	va_end(va);
	PyErr_SetString(*types[kind], text);
	return 0;
}

byte Native_RaiseKey(const nativeVal_t *key) {
	/* OBJLIB 7: the KeyError of a dict carries the key itself */
	PyObject *k = Native_ToPy(key);
	if (k) {
		PyErr_SetObject(PyExc_KeyError, k);
		Py_DECREF(k);
	}
	return 0;
}

const char *Native_TypeName(const nativeVal_t *v) {
	static const char *const names[] = {
		[NV_NONE] = "None",
		[NV_INT] = "int",
		[NV_FLOAT] = "float",
		[NV_STR] = "string",
		[NV_VEC] = "tuple",
		[NV_LONG] = "long int",
		[NV_LIST] = "list",
		[NV_TUPLE] = "tuple",
		[NV_DICT] = "dictionary",
	};
	return names[v->type];
}

byte Native_ArgStr(const nativeVal_t *v, int arg, const char **out) {
	/* "s" of getargs.c */
	if (v->type != NV_STR) {
		return Native_Raise(NE_TYPE, "argument %d: expected string, %s found", arg, Native_TypeName(v));
	}
	if (strlen(Native_Text(v)) != (size_t)v->n) {
		return Native_Raise(NE_TYPE, "argument %d: expected string without null bytes, string found", arg);
	}
	*out = Native_Text(v);
	return 1;
}

byte Native_Upper(const nativeVal_t *in, nativeVal_t *out) {
	/* strop.upper ("t#": any text, 0 bytes too; islower and toupper of the C library) */
	if (in->type != NV_STR) {
		return Native_Raise(NE_TYPE, "read-only character buffer, %s", Native_TypeName(in));
	}
	const char *t = Native_Text(in);
	Native_SetText(out, t, in->n);
	char *o = out->text ? out->text : out->s;
	for (int i = 0; i < out->n; i++) {
		int c = (unsigned char)o[i];
		if (islower(c)) {
			o[i] = (char)toupper(c);
		}
	}
	return 1;
}

byte Native_EvalText(const char *module, const nativeVal_t *text, nativeVal_t *out) {
	/* Script_EvalValue: the checks of builtin eval, then the literal after the leading blanks */
	if (text->type != NV_STR) {
		return Native_Raise(NE_TYPE, "eval() argument 1 must be string or code object");
	}
	const char *s = Native_Text(text);
	if (strlen(s) != (size_t)text->n) {
		return Native_Raise(NE_VALUE, "embedded '\\0' in string arg");
	}
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	if (Native_Literal(s, out)) {
		return 1;
	}
	/* OBJLIB 6: the error of a text no reader takes (and its note in the gate log) */
	(void)module;
	Script_NotRead("eval", s);
	return 0;
}

byte Native_Index(long i, int count, int *out) {
	if (i < 0) {
		i += count;
	}
	if (i < 0 || i >= count) {
		return Native_Raise(NE_INDEX, "list index out of range");
	}
	*out = (int)i;
	return 1;
}

byte Native_IntOfText(const char *text, long *out) {
	/* int_from_string of abstract.c over PyOS_strtol (mystrtoul.c), base 10 */
	const char *s = text;
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	const char *p = s;
	char sign = *p;
	if (sign == '+' || sign == '-') {
		p++;
	}
	unsigned long result = 0;
	byte range = 0;
	for (; isdigit((unsigned char)*p); p++) {
		unsigned long c = (unsigned long)(*p - '0'), prev = result;
		result = result * 10 + c;
		if ((long)(result - c) / 10 != (long)prev) {
			range = 1;
		}
	}
	const char *end = p;
	if (range) {
		result = ~0UL;
	}
	/* a result that looks negative overflows, except the largest negative long (without the overflow of -v) */
	byte lowest = sign == '-' && result == (unsigned long)LONG_MAX + 1;
	long v;
	if (result > (unsigned long)LONG_MAX && !lowest) {
		range = 1;
		v = 0x7fffffff;
	} else {
		v = lowest ? LONG_MIN : (long)result;
	}
	if (sign == '-' && !lowest) {
		v = -v;
	}
	if (end == s || !isdigit((unsigned char)end[-1])) {
		return Native_Raise(NE_VALUE, "invalid literal for int(): %.200s", s);
	}
	while (*end && isspace((unsigned char)*end)) {
		end++;
	}
	if (*end) {
		return Native_Raise(NE_VALUE, "invalid literal for int(): %.200s", s);
	}
	if (range) {
		return Native_Raise(NE_VALUE, "int() literal too large: %.200s", s);
	}
	*out = v;
	return 1;
}

/* ---- the engine ---- */

byte Native_NullResult(void) {
	return Native_Raise(NE_SYSTEM, "NULL result without error in call_object");
}

byte Native_EntVar(cEntity *e, const char *ref, const char *name, nativeVal_t *out) {
	/* PyEntity_GetAttr: the values start at 0, the types in this order */
	int i = 0;
	double d = 0.0, x = 0.0, y = 0.0, z = 0.0;
	char *s = NULL;
	if (!e) {
		Con_Errorf("'%s'->%s : Entity invalid.\n", ref, name);
		return Native_Raise(NE_SYSTEM, "error return without exception set");
	}
	if (Entity_VarI(e, name, &i, 0)) {
		Native_SetInt(out, i);
	} else if (Entity_VarF(e, name, &d, 0)) {
		Native_SetFloat(out, d);
	} else if (Entity_VarV(e, name, &x, &y, &z, 0)) {
		Native_SetVec(out, x, y, z);
	} else if (Entity_VarS(e, name, &s, 0)) {
		if (s) {
			Native_SetStr(out, s);
		} else {
			Native_SetNone(out);
		}
	} else {
		/* no variable: the handle looks for a method of that name (none is a variable name of the ports) */
		Con_Errorf("'%s'->%s : Member invalid.\n", ref, name);
		return Native_Raise(NE_ATTRIBUTE, "%s", name);
	}
	return 1;
}

/* the number of a value as the "i" (int 0) or "d" (int 1) of PyArg_Parse reads it (PyInt_AsLong and PyFloat_AsDouble
 * of 1.5.2 through the number methods, no range check for "i"): 0 when it reads none */
static byte Native_ArgNum(const nativeVal_t *v, byte asDouble, double *d, int *i) {
	switch (v->type) {
	case NV_INT:
		*i = (int)v->i;
		*d = (double)v->i;
		return 1;
	case NV_FLOAT:
		if (!asDouble && (v->f < (double)LONG_MIN || v->f > (double)LONG_MAX)) {
			/* float_int: "float too large to convert" */
			return 0;
		}
		*i = (int)(long)v->f;
		*d = v->f;
		return 1;
	case NV_LONG: {
		/* PyLong_AsLong and PyLong_AsDouble: the value must fit */
		errno = 0;
		const char *t = Native_Text(v);
		long l = strtol(t, NULL, 10);
		if (!asDouble && errno == ERANGE) {
			return 0;
		}
		*i = (int)l;
		/* PyLong_AsDouble of 1.5.2 has no overflow check (inf) */
		*d = strtod(t, NULL);
		return 1;
	}
	}
	return 0;
}

byte Native_ArgDouble(const nativeVal_t *v, double *out) {
	int i;
	if (!Native_ArgNum(v, 1, out, &i)) {
		/* PyFloat_AsDouble of a value without number methods */
		return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	}
	return 1;
}

byte Native_EntSet(cEntity *e, const char *ref, const char *name, const nativeVal_t *v) {
	/* PyEntity_SetVar: the value is tried as int, float, vector and text, each form that parses goes to the
	 * variable of that type until one takes it */
	byte parsed = 0;
	double d, x[3];
	int i;
	if (!e) {
		Con_Errorf("'%s'->%s : Entity invalid.\n", ref, name);
		return Native_Raise(NE_SYSTEM, "error return without exception set");
	}
	if (Native_ArgNum(v, 0, &d, &i)) {
		parsed = 1;
		if (Entity_VarI(e, name, &i, 1)) {
			return 1;
		}
	}
	if (Native_ArgNum(v, 1, &d, &i)) {
		parsed = 1;
		if (Entity_VarF(e, name, &d, 1)) {
			return 1;
		}
	}
	byte vec = v->type == NV_VEC;
	if (vec) {
		x[0] = v->v[0];
		x[1] = v->v[1];
		x[2] = v->v[2];
	} else if (v->type == NV_TUPLE && v->n == 3) {
		vec = 1;
		for (int k = 0; vec && k < 3; k++) {
			vec = Native_ArgNum(&v->items[k], 1, &x[k], &i);
		}
	}
	if (vec) {
		parsed = 1;
		if (Entity_VarV(e, name, &x[0], &x[1], &x[2], 1)) {
			return 1;
		}
	}
	/* "s": a text without 0 bytes */
	if (v->type == NV_STR && strlen(Native_Text(v)) == (size_t)v->n) {
		char *s = (char *)Native_Text(v);
		if (Entity_VarS(e, name, &s, 1)) {
			return 1;
		}
	} else if (!parsed) {
		Con_Errorf("'%s'->%s : Invalid type.\n", ref, name);
		return Native_Raise(NE_SYSTEM, "error return without exception set");
	}
	Con_Errorf("'%s'->%s : Member invalid.\n", ref, name);
	return Native_Raise(NE_SYSTEM, "error return without exception set");
}

byte Native_GetEntity(const nativeVal_t *name, cEntity **out) {
	const char *s;
	if (!Native_ArgStr(name, 1, &s)) {
		return 0;
	}
	*out = World_FindEntity(s);
	return 1;
}

int Native_Seam(const char *module, const char *name, PyObject **hook) {
	PyObject *f = Script_Attr(module, name);
	if (!f) {
		return -1;
	}
	/* the engine's own: a C function of the module table (no self; the recorder of the test channel has one) */
	if (PyCFunction_Check(f) && !PyCFunction_GET_SELF(f) && !strcmp(((PyCFunctionObject *)f)->m_ml->ml_name, name)) {
		Py_DECREF(f);
		return 0;
	}
	*hook = f;
	return 1;
}

byte Native_GVarGet(const char *name, nativeVal_t *out) {
	/* GVarValue of Scrap.Get */
	cGVar *v = GVar_Find(name);
	if (!v) {
		Con_Errorf("ERROR: '%s' global variable not defined\n", name);
		return Native_NullResult();
	}
	switch (v->base.type) {
	case GVAR_STRING:
		/* Py_BuildValue "s" of NULL is None */
		if (v->value.s) {
			Native_SetText(out, v->value.s, (int)strlen(v->value.s));
		} else {
			Native_SetNone(out);
		}
		return 1;
	case GVAR_INT:
		Native_SetInt(out, v->value.i);
		return 1;
	case GVAR_FLOAT:
		Native_SetFloat(out, (double)v->value.f);
		return 1;
	case GVAR_FUNCTION:
		Native_SetStr(out, "<function>");
		return 1;
	case GVAR_SCRIPT_FUNCTION:
		Native_SetStr(out, "<Script function>");
		return 1;
	}
	CHECK(0);
	return 0;
}

byte Native_GVarSet(const char *name, const nativeVal_t *v) {
	/* Scrap.Set: "ss", else "sd"; a variable of another type or no variable gives NULL without an exception */
	double d;
	int r;
	if (v->type == NV_STR && strlen(Native_Text(v)) == (size_t)v->n) {
		r = GVar_SetStr(name, Native_Text(v));
	} else if (Native_ArgDouble(v, &d)) {
		r = GVar_SetNum(name, d);
	} else {
		return 0;
	}
	return r < 0 ? Native_NullResult() : 1;
}

byte Native_ScorerSet(int id, const char *item, const char *var, const nativeVal_t *v) {
	/* SScorer.Set: "isss", else "issd" */
	double d;
	if (v->type == NV_STR && strlen(Native_Text(v)) == (size_t)v->n) {
		Script_SetStr(id, item, var, Native_Text(v));
	} else if (Native_ArgDouble(v, &d)) {
		Script_SetNum(id, item, var, d);
	} else {
		return 0;
	}
	return 1;
}

int Native_NetOff(void) {
	PyObject *hook;
	int s = Native_Seam("Scrap", "GetNetFlags", &hook);
	if (s == 0) {
		return (g_pApp->flags & 7) == 0;
	}
	if (s < 0) {
		return -1;
	}
	/* GetNetFlags() == (0, 0, 0) with what the test put there */
	PyObject *flags = Script_Invoke(hook, PyTuple_New(0));
	Py_DECREF(hook);
	PyObject *off = flags ? Py_BuildValue("(iii)", 0, 0, 0) : NULL;
	int r = off ? Script_Eq(flags, off) : -1;
	Py_XDECREF(off);
	Py_XDECREF(flags);
	return r;
}

int Native_NetFlag(int i) {
	PyObject *hook;
	int s = Native_Seam("Scrap", "GetNetFlags", &hook);
	if (s == 0) {
		return (int)(g_pApp->flags >> i & 1);
	}
	if (s < 0) {
		return -1;
	}
	PyObject *flags = Script_Invoke(hook, PyTuple_New(0));
	Py_DECREF(hook);
	PyObject *f = flags ? PySequence_GetItem(flags, i) : NULL;
	Py_XDECREF(flags);
	int r = f ? PyObject_IsTrue(f) : -1;
	Py_XDECREF(f);
	return r;
}

/* ---- module state mirrored into the module dict ---- */

PyObject *Native_ToPy(const nativeVal_t *v) {
	PyObject *o = NULL;
	switch (v->type) {
	case NV_INT:
		return PyInt_FromLong(v->i);
	case NV_FLOAT:
		return PyFloat_FromDouble(v->f);
	case NV_STR:
		return PyString_FromStringAndSize((char *)Native_Text(v), v->n);
	case NV_LONG:
		return PyLong_FromString((char *)Native_Text(v), NULL, 10);
	case NV_VEC:
		return Py_BuildValue("(ddd)", v->v[0], v->v[1], v->v[2]);
	case NV_LIST:
	case NV_TUPLE:
		o = v->type == NV_LIST ? PyList_New(v->n) : PyTuple_New(v->n);
		for (int i = 0; o && i < v->n; i++) {
			PyObject *x = Native_ToPy(&v->items[i]);
			if (!x || (v->type == NV_LIST ? PyList_SetItem(o, i, x) : PyTuple_SetItem(o, i, x)) != 0) {
				Py_DECREF(o);
				o = NULL;
			}
		}
		return o;
	case NV_DICT: {
		const nativeVal_t *k, *x;
		int pos = 0;
		o = PyDict_New();
		while (o && Native_DictNext(v, &pos, &k, &x)) {
			PyObject *pk = Native_ToPy(k), *px = pk ? Native_ToPy(x) : NULL;
			if (!px || PyDict_SetItem(o, pk, px) != 0) {
				Py_DECREF(o);
				o = NULL;
			}
			Py_XDECREF(pk);
			Py_XDECREF(px);
		}
		return o;
	}
	}
	return Py_ReturnNone();
}

byte Native_FromPy(PyObject *o, unsigned types, nativeVal_t *v) {
	double x, y, z;
	if (o == Py_None && types & 1 << NV_NONE) {
		Native_SetNone(v);
	} else if (PyInt_Check(o) && types & 1 << NV_INT) {
		Native_SetInt(v, PyInt_AsLong(o));
	} else if (PyFloat_Check(o) && types & 1 << NV_FLOAT) {
		Native_SetFloat(v, PyFloat_AsDouble(o));
	} else if (PyString_Check(o) && types & 1 << NV_STR && PyString_Size(o) < NATIVE_TEXT &&
		strlen(PyString_AsString(o)) == (size_t)PyString_Size(o)) {
		Native_SetStr(v, PyString_AsString(o));
	} else if (PyTuple_Check(o) && PyTuple_Size(o) == 3 && types & 1 << NV_VEC &&
		PyArg_ParseTuple(o, "ddd", &x, &y, &z)) {
		Native_SetVec(v, x, y, z);
	} else {
		PyErr_Clear();
		return 0;
	}
	return 1;
}

/* OBJLIB 9: the head of a dict of the vendored dictobject.c (the struct is private there): its table size */
typedef struct {
	PyObject_HEAD
	int ma_fill;
	int ma_used;
	int ma_size;
	int ma_poly;
	void *ma_table;
} nativePyDict_t;

static byte Native_FromPyIn(PyObject *o, nativeVal_t *v, int depth) {
	Native_SetNone(v);
	if (o == Py_None) {
		return 1;
	}
	if (PyInt_Check(o)) {
		Native_SetInt(v, PyInt_AsLong(o));
		return 1;
	}
	if (PyFloat_Check(o)) {
		Native_SetFloat(v, PyFloat_AsDouble(o));
		return 1;
	}
	if (PyString_Check(o)) {
		Native_SetText(v, PyString_AsString(o), PyString_Size(o));
		return 1;
	}
	if (PyLong_Check(o)) {
		/* the digits of its repr, without the L */
		PyObject *r = PyObject_Repr(o);
		if (r) {
			Native_SetLong(v, PyString_AsString(r), PyString_Size(r) - 1);
			Py_DECREF(r);
		}
		PyErr_Clear();
		return r != NULL;
	}
	if (depth >= NLIT_DEPTH) {
		return 0;
	}
	byte ok = 1;
	if (PyList_Check(o) || PyTuple_Check(o)) {
		Native_SetEmpty(v, PyList_Check(o) ? NV_LIST : NV_TUPLE);
		for (int i = 0; ok && i < PySequence_Length(o); i++) {
			ok = Native_FromPyIn(PyList_Check(o) ? PyList_GetItem(o, i) : PyTuple_GetItem(o, i), Native_Append(v),
				depth + 1);
		}
	} else if (PyDict_Check(o)) {
		/* each key and value in the slot Python has it in */
		int pos = 0;
		PyObject *k, *x;
		Native_SetEmpty(v, NV_DICT);
		if (((nativePyDict_t *)o)->ma_size) {
			Native_Table(v, ((nativePyDict_t *)o)->ma_size);
		}
		while (ok && PyDict_Next(o, &pos, &k, &x)) {
			nativeVal_t *slot = &v->items[2 * (pos - 1)];
			v->n++;
			ok = Native_FromPyIn(k, slot, depth + 1) && Native_FromPyIn(x, slot + 1, depth + 1);
		}
	} else {
		ok = 0;
	}
	if (!ok) {
		Native_Free(v);
	}
	return ok;
}

byte Native_FromPyAny(PyObject *o, nativeVal_t *v) {
	return Native_FromPyIn(o, v, 0);
}

/* a value of the mirror from Python into *v (what *v owned is freed first): the short forms of Native_FromPy, and
 * with a long int, list, tuple or dict among the types any value of those types (texts of any length too) */
static byte Native_Take(PyObject *o, unsigned types, nativeVal_t *v) {
	Native_Free(v);
	if (Native_FromPy(o, types, v)) {
		return 1;
	}
	if (!(types & (1 << NV_LONG | 1 << NV_LIST | 1 << NV_TUPLE | 1 << NV_DICT)) || !Native_FromPyAny(o, v)) {
		return 0;
	}
	if (types & 1 << v->type) {
		return 1;
	}
	Native_Free(v);
	return 0;
}

/* the variables from the dict of module: a missing one is the AttributeError of a module attribute (global 0) or the
 * NameError of a global the script reads (global 1) */
static byte Native_ImportBy(const char *module, const nativeVar_t *vars, byte global) {
	for (const nativeVar_t *var = vars; var->name; var++) {
		PyObject *o = global ? Script_Global(module, var->name) : Script_Attr(module, var->name);
		if (!o) {
			return 0;
		}
		byte ok = 1;
		if (!var->count) {
			ok = Native_Take(o, var->types, var->values);
		} else if (!PyList_Check(o) || PyList_Size(o) != var->count) {
			ok = 0;
		} else {
			for (int i = 0; ok && i < var->count; i++) {
				ok = Native_Take(PyList_GetItem(o, i), var->types, &var->values[i]);
			}
		}
		Py_DECREF(o);
		if (!ok) {
			/* a value only Python code can have put there (the C writes the forms it takes) */
			return Native_Raise(NE_TYPE, var->count ? "%s.%s: not a list of %d values the C module takes" :
				"%s.%s: not a value the C module takes", module, var->name, var->count);
		}
	}
	return 1;
}

byte Native_Arg(PyObject *o, nativeVal_t *v) {
	return Native_FromPyAny(o, v) || Native_Raise(NE_TYPE, "a value of type %.50s has no native form", o->ob_type->tp_name);
}

byte Native_Import(const char *module, const nativeVar_t *vars) {
	return Native_ImportBy(module, vars, 0);
}

byte Native_ImportGlobals(const char *module, const nativeVar_t *vars) {
	return Native_ImportBy(module, vars, 1);
}

byte Native_Put(const char *module, const nativeVar_t *var, int index) {
	const nativeVal_t *v = var->count ? &var->values[index] : var->values;
	PyObject *o = Native_ToPy(v);
	if (!o) {
		return 0;
	}
	int r;
	if (var->count) {
		PyObject *list = Script_Attr(module, var->name);
		r = list ? PySequence_SetItem(list, index, o) : -1;
		Py_XDECREF(list);
	} else {
		PyObject *m = PyImport_ImportModule((char *)module);
		r = m ? PyObject_SetAttrString(m, (char *)var->name, o) : -1;
		Py_XDECREF(m);
	}
	Py_DECREF(o);
	return r == 0;
}
