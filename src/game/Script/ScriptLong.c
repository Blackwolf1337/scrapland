/*
 * The long ints of the scripts in C (include/scrap/scriptlib.h, stage 7e, batch 7e4c): the operations of
 * Objects/longobject.c of Python 1.5.2 on NV_LONG values (a decimal text), with its digits of 15 bits, its algorithms
 * (x_divrem, the floor division of l_divmod, long_pow, the bitwise operations on the two's complement of negative
 * numbers) and its error texts, so results and errors are those of the build's Python. The carries are an unsigned
 * long and a long as in longintrepr.h (32 bits on Windows, 64 on Linux), as there.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/scriptlib.h"

#define L_SHIFT 15
#define L_BASE (1 << L_SHIFT)
#define L_MASK (L_BASE - 1)

typedef unsigned short digit;
typedef unsigned long twodigits;
typedef long stwodigits;

/* a long int: size digits (its sign the sign of the number, 0 for 0), the lowest first */
typedef struct {
	int size;
	digit *d;
} lnum_t;

#define L_ABS(x) ((x) < 0 ? -(x) : (x))

static void L_New(lnum_t *z, int size) {
	z->size = size;
	z->d = Mem_Alloc((uint32_t)((size_t)(size ? size : 1) * sizeof(digit)));
	memset(z->d, 0, (size_t)(size ? size : 1) * sizeof(digit));
}

static void L_Free(lnum_t *z) {
	z->d = Mem_Free(z->d);
	z->size = 0;
}

/* long_normalize */
static void L_Norm(lnum_t *v) {
	int j = L_ABS(v->size), i = j;
	while (i > 0 && v->d[i - 1] == 0) {
		--i;
	}
	if (i != j) {
		v->size = v->size < 0 ? -i : i;
	}
}

static void L_Copy(lnum_t *z, const lnum_t *a) {
	L_New(z, L_ABS(a->size));
	memcpy(z->d, a->d, (size_t)L_ABS(a->size) * sizeof(digit));
	z->size = a->size;
}

/* PyLong_FromLong */
static void L_FromLong(lnum_t *z, long ival) {
	unsigned long t = (unsigned long)ival;
	L_New(z, 5);
	if (ival < 0) {
		t = 0ul - t;
		z->size = -5;
	}
	for (int i = 0; i < 5; i++) {
		z->d[i] = (digit)(t & L_MASK);
		t >>= L_SHIFT;
	}
	L_Norm(z);
}

/* muladd1 into a new number */
static void L_MulAdd1(lnum_t *z, const lnum_t *a, unsigned n, unsigned extra) {
	int size = L_ABS(a->size), i;
	twodigits carry = extra;
	L_New(z, size + 1);
	for (i = 0; i < size; ++i) {
		carry += (twodigits)a->d[i] * n;
		z->d[i] = (digit)(carry & L_MASK);
		carry >>= L_SHIFT;
	}
	z->d[i] = (digit)carry;
	L_Norm(z);
}

/* divrem1: |a| / n, the remainder in *rem */
static void L_DivRem1(lnum_t *z, const lnum_t *a, unsigned n, digit *rem) {
	int size = L_ABS(a->size);
	twodigits r = 0;
	L_New(z, size);
	for (int i = size; --i >= 0;) {
		r = (r << L_SHIFT) + a->d[i];
		z->d[i] = (digit)(r / n);
		r %= n;
	}
	*rem = (digit)r;
	L_Norm(z);
}

/* the number of a value NV_INT or NV_LONG (other values: 0) */
static void L_Of(const nativeVal_t *v, lnum_t *z) {
	if (v->type != NV_LONG) {
		L_FromLong(z, v->type == NV_INT ? v->i : 0);
		return;
	}
	const char *s = Native_Text(v);
	byte neg = *s == '-';
	lnum_t t;
	L_New(z, 0);
	for (s += neg; *s >= '0' && *s <= '9'; s++) {
		L_MulAdd1(&t, z, 10, (unsigned)(*s - '0'));
		L_Free(z);
		*z = t;
	}
	if (neg && z->size) {
		z->size = -z->size;
	}
}

/* long_format: the text of the number in base 8, 10 or 16, with the prefix of the base and the L when tail */
static void L_Format(const lnum_t *a, int base, byte tail, nativeBuf_t *out) {
	int size = L_ABS(a->size), bits = 0;
	for (int i = base; i > 1; i >>= 1) {
		++bits;
	}
	int room = 6 + (size * L_SHIFT + bits - 1) / bits;
	char *buf = Mem_Alloc((uint32_t)room + 1), *p = buf + room;
	*p = '\0';
	if (tail) {
		*--p = 'L';
	}
	if (a->size == 0) {
		*--p = '0';
	} else if ((base & (base - 1)) == 0) {
		twodigits temp = a->d[0];
		int bitsleft = L_SHIFT, last = size, basebits = 1, i = 0;
		for (int k = base; (k >>= 1) > 1;) {
			++basebits;
		}
		for (;;) {
			while (bitsleft >= basebits) {
				if (temp == 0 && i >= last - 1) {
					break;
				}
				int rem = (int)(temp & (twodigits)(base - 1));
				*--p = (char)(rem < 10 ? rem + '0' : rem + 'A' - 10);
				bitsleft -= basebits;
				temp >>= basebits;
			}
			if (++i >= last) {
				if (temp == 0) {
					break;
				}
				bitsleft = 99;
			} else {
				temp = ((twodigits)a->d[i] << bitsleft) | temp;
				bitsleft += L_SHIFT;
			}
		}
	} else {
		lnum_t cur, t;
		L_Copy(&cur, a);
		do {
			digit rem;
			L_DivRem1(&t, &cur, (unsigned)base, &rem);
			L_Free(&cur);
			cur = t;
			*--p = (char)(rem < 10 ? rem + '0' : rem + 'A' - 10);
		} while (cur.size != 0);
		L_Free(&cur);
	}
	if (base == 8) {
		if (size != 0) {
			*--p = '0';
		}
	} else if (base == 16) {
		*--p = 'x';
		*--p = '0';
	}
	if (a->size < 0) {
		*--p = '-';
	}
	Native_BufAdd(out, p, -1);
	Mem_Free(buf);
}

/* the number as a value: a long int, its decimal text (z is freed) */
static void L_Give(lnum_t *z, nativeVal_t *out) {
	nativeBuf_t b;
	Native_BufInit(&b);
	L_Format(z, 10, 0, &b);
	Native_SetLong(out, b.p, b.len);
	Native_BufFree(&b);
	L_Free(z);
}

/* x_add, x_sub: |a| + |b|, |a| - |b| */
static void L_XAdd(lnum_t *z, const lnum_t *a, const lnum_t *b) {
	int sa = L_ABS(a->size), sb = L_ABS(b->size), i;
	digit carry = 0;
	if (sa < sb) {
		const lnum_t *t = a;
		a = b;
		b = t;
		int s = sa;
		sa = sb;
		sb = s;
	}
	L_New(z, sa + 1);
	for (i = 0; i < sb; ++i) {
		carry = (digit)(carry + a->d[i] + b->d[i]);
		z->d[i] = carry & L_MASK;
		carry >>= L_SHIFT;
	}
	for (; i < sa; ++i) {
		carry = (digit)(carry + a->d[i]);
		z->d[i] = carry & L_MASK;
		carry >>= L_SHIFT;
	}
	z->d[i] = carry;
	L_Norm(z);
}

static void L_XSub(lnum_t *z, const lnum_t *a, const lnum_t *b) {
	int sa = L_ABS(a->size), sb = L_ABS(b->size), i, sign = 1;
	digit borrow = 0;
	if (sa < sb) {
		sign = -1;
		const lnum_t *t = a;
		a = b;
		b = t;
		int s = sa;
		sa = sb;
		sb = s;
	} else if (sa == sb) {
		i = sa;
		while (--i >= 0 && a->d[i] == b->d[i]) {
		}
		if (i < 0) {
			L_New(z, 0);
			return;
		}
		if (a->d[i] < b->d[i]) {
			sign = -1;
			const lnum_t *t = a;
			a = b;
			b = t;
		}
		sa = sb = i + 1;
	}
	L_New(z, sa);
	for (i = 0; i < sb; ++i) {
		borrow = (digit)(a->d[i] - b->d[i] - borrow);
		z->d[i] = borrow & L_MASK;
		borrow >>= L_SHIFT;
		borrow &= 1;
	}
	for (; i < sa; ++i) {
		borrow = (digit)(a->d[i] - borrow);
		z->d[i] = borrow & L_MASK;
		borrow >>= L_SHIFT;
	}
	if (sign < 0) {
		z->size = -z->size;
	}
	L_Norm(z);
}

/* long_add, long_sub */
static void L_Add(lnum_t *z, const lnum_t *a, const lnum_t *b, byte sub) {
	byte nb = (b->size < 0) != sub;
	if (a->size < 0) {
		if (nb) {
			L_XAdd(z, a, b);
		} else {
			L_XSub(z, a, b);
		}
		z->size = -z->size;
	} else if (nb) {
		L_XSub(z, a, b);
	} else {
		L_XAdd(z, a, b);
	}
}

/* long_mul */
static void L_Mul(lnum_t *z, const lnum_t *a, const lnum_t *b) {
	int sa = L_ABS(a->size), sb = L_ABS(b->size);
	L_New(z, sa + sb);
	for (int i = 0; i < sa; ++i) {
		twodigits carry = 0, f = a->d[i];
		int j;
		for (j = 0; j < sb; ++j) {
			carry += z->d[i + j] + b->d[j] * f;
			z->d[i + j] = (digit)(carry & L_MASK);
			carry >>= L_SHIFT;
		}
		for (; carry != 0; ++j) {
			carry += z->d[i + j];
			z->d[i + j] = (digit)(carry & L_MASK);
			carry >>= L_SHIFT;
		}
	}
	if ((a->size < 0) != (b->size < 0)) {
		z->size = -z->size;
	}
	L_Norm(z);
}

/* x_divrem: |v1| / |w1| (w1 of 2 digits or more, |v1| >= |w1|), the remainder in *rem */
static void L_XDivRem(lnum_t *a, const lnum_t *v1, const lnum_t *w1, lnum_t *rem) {
	int size_w = L_ABS(w1->size);
	digit d = (digit)((twodigits)L_BASE / (w1->d[size_w - 1] + 1));
	lnum_t v, w;
	L_MulAdd1(&v, v1, d, 0);
	L_MulAdd1(&w, w1, d, 0);
	int size_v = L_ABS(v.size);
	L_New(a, size_v - size_w + 1);
	for (int j = size_v, k = a->size - 1; k >= 0; --j, --k) {
		digit vj = j >= size_v ? 0 : v.d[j];
		twodigits q;
		stwodigits carry = 0;
		int i;
		if (vj == w.d[size_w - 1]) {
			q = L_MASK;
		} else {
			q = (((twodigits)vj << L_SHIFT) + v.d[j - 1]) / w.d[size_w - 1];
		}
		while (w.d[size_w - 2] * q > ((((twodigits)vj << L_SHIFT) + v.d[j - 1] - q * w.d[size_w - 1]) << L_SHIFT) +
			v.d[j - 2]) {
			--q;
		}
		for (i = 0; i < size_w && i + k < size_v; ++i) {
			twodigits z = w.d[i] * q;
			digit zz = (digit)(z >> L_SHIFT);
			carry += (stwodigits)(v.d[i + k] - z + ((twodigits)zz << L_SHIFT));
			v.d[i + k] = (digit)(carry & L_MASK);
			carry = (carry >> L_SHIFT) - zz;
		}
		if (i + k < size_v) {
			carry += v.d[i + k];
			v.d[i + k] = 0;
		}
		if (carry == 0) {
			a->d[k] = (digit)q;
		} else {
			a->d[k] = (digit)(q - 1);
			carry = 0;
			for (i = 0; i < size_w && i + k < size_v; ++i) {
				carry += v.d[i + k] + w.d[i];
				v.d[i + k] = (digit)(carry & L_MASK);
				carry >>= L_SHIFT;
			}
		}
	}
	L_Norm(a);
	L_DivRem1(rem, &v, d, &d);
	L_Free(&v);
	L_Free(&w);
}

/* long_divrem: the quotient with the sign of a*b, the remainder with the sign of a; 0 with the ZeroDivisionError */
static byte L_DivRem(const lnum_t *a, const lnum_t *b, lnum_t *div, lnum_t *rem) {
	int sa = L_ABS(a->size), sb = L_ABS(b->size);
	if (sb == 0) {
		return Native_Raise(NE_ZERODIV, "long division or modulo");
	}
	if (sa < sb || (sa == sb && a->d[sa - 1] < b->d[sb - 1])) {
		L_New(div, 0);
		L_Copy(rem, a);
		return 1;
	}
	if (sb == 1) {
		digit r = 0;
		L_DivRem1(div, a, b->d[0], &r);
		L_FromLong(rem, (long)r);
	} else {
		L_XDivRem(div, a, b, rem);
	}
	if ((a->size < 0) != (b->size < 0)) {
		div->size = -div->size;
	}
	if (a->size < 0 && rem->size != 0) {
		rem->size = -rem->size;
	}
	return 1;
}

/* l_divmod: the floor division, the modulo with the sign of w */
static byte L_DivMod(const lnum_t *v, const lnum_t *w, lnum_t *div, lnum_t *mod) {
	if (!L_DivRem(v, w, div, mod)) {
		return 0;
	}
	if ((mod->size < 0 && w->size > 0) || (mod->size > 0 && w->size < 0)) {
		lnum_t t, one;
		L_Add(&t, mod, w, 0);
		L_Free(mod);
		*mod = t;
		L_FromLong(&one, 1);
		L_Add(&t, div, &one, 1);
		L_Free(&one);
		L_Free(div);
		*div = t;
	}
	return 1;
}

/* long_invert: -(x + 1) */
static void L_Invert(lnum_t *z, const lnum_t *v) {
	lnum_t one;
	L_FromLong(&one, 1);
	L_Add(z, v, &one, 0);
	L_Free(&one);
	z->size = -z->size;
}

/* PyLong_AsLong: 0 with the OverflowError */
static byte L_AsLong(const lnum_t *v, long *out) {
	unsigned long x = 0, prev;
	int i = L_ABS(v->size), sign = v->size < 0 ? -1 : 1;
	while (--i >= 0) {
		prev = x;
		x = (x << L_SHIFT) + v->d[i];
		if ((x >> L_SHIFT) != prev) {
			return Native_Raise(NE_OVERFLOW, "long int too long to convert");
		}
	}
	if ((long)x < 0 && (sign > 0 || (x << 1) != 0)) {
		return Native_Raise(NE_OVERFLOW, "long int too long to convert");
	}
	*out = (long)x * sign;
	return 1;
}

/* long_rshift and long_lshift by a count b */
static byte L_Shift(lnum_t *z, const lnum_t *a, const lnum_t *b, byte left) {
	long by;
	if (!left && a->size < 0) {
		lnum_t a1, a2;
		L_Invert(&a1, a);
		byte ok = L_Shift(&a2, &a1, b, 0);
		L_Free(&a1);
		if (ok) {
			L_Invert(z, &a2);
			L_Free(&a2);
		}
		return ok;
	}
	if (!L_AsLong(b, &by)) {
		return 0;
	}
	if (by < 0) {
		return Native_Raise(NE_VALUE, "negative shift count");
	}
	if (!left) {
		int wordshift = (int)(by / L_SHIFT), newsize = L_ABS(a->size) - wordshift;
		if (newsize <= 0) {
			L_New(z, 0);
			return 1;
		}
		int loshift = (int)(by % L_SHIFT), hishift = L_SHIFT - loshift;
		digit lomask = (digit)((1u << hishift) - 1), himask = (digit)(L_MASK ^ lomask);
		L_New(z, newsize);
		for (int i = 0, j = wordshift; i < newsize; i++, j++) {
			z->d[i] = (digit)((a->d[j] >> loshift) & lomask);
			if (i + 1 < newsize) {
				z->d[i] |= (digit)((a->d[j + 1] << hishift) & himask);
			}
		}
		L_Norm(z);
		return 1;
	}
	if ((long)(int)by != by) {
		return Native_Raise(NE_VALUE, "outrageous left shift count");
	}
	int wordshift = (int)by / L_SHIFT, remshift = (int)by - wordshift * L_SHIFT, oldsize = L_ABS(a->size);
	int newsize = oldsize + wordshift + (remshift ? 1 : 0);
	twodigits accum = 0;
	L_New(z, newsize);
	if (a->size < 0) {
		z->size = -z->size;
	}
	int i = wordshift;
	for (int j = 0; j < oldsize; i++, j++) {
		accum |= (twodigits)a->d[j] << remshift;
		z->d[i] = (digit)(accum & L_MASK);
		accum >>= L_SHIFT;
	}
	if (remshift) {
		z->d[newsize - 1] = (digit)accum;
	}
	L_Norm(z);
	return 1;
}

/* long_bitwise: & | ^ on the two's complement */
static void L_Bitwise(lnum_t *z, const lnum_t *a0, int op, const lnum_t *b0) {
	digit maska = 0, maskb = 0;
	lnum_t a, b;
	int negz = 0;
	if (a0->size < 0) {
		L_Invert(&a, a0);
		maska = L_MASK;
	} else {
		L_Copy(&a, a0);
	}
	if (b0->size < 0) {
		L_Invert(&b, b0);
		maskb = L_MASK;
	} else {
		L_Copy(&b, b0);
	}
	switch (op) {
	case '^':
		if (maska != maskb) {
			maska ^= L_MASK;
			negz = 1;
		}
		break;
	case '&':
		if (maska && maskb) {
			op = '|';
			maska ^= L_MASK;
			maskb ^= L_MASK;
			negz = 1;
		}
		break;
	case '|':
		if (maska || maskb) {
			op = '&';
			maska ^= L_MASK;
			maskb ^= L_MASK;
			negz = 1;
		}
		break;
	}
	int sa = a.size, sb = b.size;
	int sz = op == '&' ? (maska ? sb : maskb ? sa : (sa < sb ? sa : sb)) : (sa > sb ? sa : sb);
	L_New(z, sz);
	for (int i = 0; i < sz; ++i) {
		digit da = (digit)((i < sa ? a.d[i] : 0) ^ maska), db = (digit)((i < sb ? b.d[i] : 0) ^ maskb);
		z->d[i] = (digit)(op == '&' ? da & db : op == '|' ? da | db : da ^ db);
	}
	L_Free(&a);
	L_Free(&b);
	L_Norm(z);
	if (negz) {
		lnum_t t;
		L_Invert(&t, z);
		L_Free(z);
		*z = t;
	}
}

/* long_pow, c NULL for none */
static byte L_Pow(lnum_t *z, const lnum_t *a0, const lnum_t *b, const lnum_t *c) {
	if (b->size < 0) {
		return Native_Raise(NE_VALUE, "long integer to the negative power");
	}
	lnum_t a, t, div, mod;
	L_FromLong(z, 1);
	L_Copy(&a, a0);
	for (int i = 0; i < b->size; ++i) {
		digit bi = b->d[i];
		for (int j = 0; j < L_SHIFT; ++j) {
			if (bi & 1) {
				L_Mul(&t, z, &a);
				L_Free(z);
				if (c) {
					byte ok = L_DivMod(&t, c, &div, &mod);
					L_Free(&t);
					if (!ok) {
						L_Free(&a);
						return 0;
					}
					L_Free(&div);
					t = mod;
				}
				*z = t;
			}
			bi >>= 1;
			if (bi == 0 && i + 1 == b->size) {
				break;
			}
			L_Mul(&t, &a, &a);
			L_Free(&a);
			if (c) {
				byte ok = L_DivMod(&t, c, &div, &mod);
				L_Free(&t);
				if (!ok) {
					L_Free(z);
					return 0;
				}
				L_Free(&div);
				t = mod;
			}
			a = t;
		}
	}
	L_Free(&a);
	if (c) {
		byte ok = L_DivMod(z, c, &div, &mod);
		L_Free(z);
		if (!ok) {
			return 0;
		}
		L_Free(&div);
		*z = mod;
	}
	return 1;
}

/* ---- the operations on values ---- */

byte Long_Op(int op, const nativeVal_t *a, const nativeVal_t *b, nativeVal_t *out) {
	lnum_t x, y, z, w;
	byte ok = 1;
	Native_SetNone(out);
	L_Of(a, &x);
	L_Of(b, &y);
	switch (op) {
	case '+':
	case '-':
		L_Add(&z, &x, &y, op == '-');
		break;
	case '*':
		L_Mul(&z, &x, &y);
		break;
	case '/':
	case '%':
	case 'd':
		ok = L_DivMod(&x, &y, &z, &w);
		if (ok && op == '%') {
			L_Free(&z);
			z = w;
		} else if (ok && op == 'd') {
			nativeVal_t q, r;
			L_Give(&z, &q);
			L_Give(&w, &r);
			Native_SetEmpty(out, NV_TUPLE);
			*Native_Append(out) = q;
			*Native_Append(out) = r;
		} else if (ok) {
			L_Free(&w);
		}
		break;
	case '<':
	case '>':
		ok = L_Shift(&z, &x, &y, op == '<');
		break;
	default:
		L_Bitwise(&z, &x, op, &y);
		break;
	}
	L_Free(&x);
	L_Free(&y);
	if (ok && op != 'd') {
		L_Give(&z, out);
	}
	return ok;
}

byte Long_Pow(const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c, nativeVal_t *out) {
	lnum_t x, y, m, z;
	Native_SetNone(out);
	L_Of(a, &x);
	L_Of(b, &y);
	if (c) {
		L_Of(c, &m);
	}
	byte ok = L_Pow(&z, &x, &y, c ? &m : NULL);
	L_Free(&x);
	L_Free(&y);
	if (c) {
		L_Free(&m);
	}
	if (ok) {
		L_Give(&z, out);
	}
	return ok;
}

void Long_Unary(int op, const nativeVal_t *a, nativeVal_t *out) {
	lnum_t x, z;
	L_Of(a, &x);
	if (op == '~') {
		L_Invert(&z, &x);
		L_Free(&x);
	} else {
		z = x;
		if (op == '-' || (op == 'a' && z.size < 0)) {
			z.size = -z.size;
		}
	}
	L_Give(&z, out);
}

void Long_FromLong(long l, nativeVal_t *out) {
	char t[0x20];
	snprintf(t, sizeof(t), "%ld", l);
	Native_SetLong(out, t, (int)strlen(t));
}

void Long_FromDouble(double dval, nativeVal_t *out) {
	lnum_t z;
	int expo;
	byte neg = dval < 0.0;
	if (neg) {
		dval = -dval;
	}
	double frac = frexp(dval, &expo);
	if (expo <= 0) {
		Native_SetLong(out, "0", 1);
		return;
	}
	int ndig = (expo - 1) / L_SHIFT + 1;
	L_New(&z, ndig);
	frac = ldexp(frac, (expo - 1) % L_SHIFT + 1);
	for (int i = ndig; --i >= 0;) {
		long bits = (long)frac;
		z.d[i] = (digit)bits;
		frac = ldexp(frac - (double)bits, L_SHIFT);
	}
	if (neg) {
		z.size = -z.size;
	}
	L_Give(&z, out);
}

double Long_AsDouble(const nativeVal_t *v) {
	lnum_t x;
	double r = 0.0;
	L_Of(v, &x);
	for (int i = L_ABS(x.size); --i >= 0;) {
		r = r * (double)L_BASE + (double)x.d[i];
	}
	if (x.size < 0) {
		r = -r;
	}
	L_Free(&x);
	return r;
}

byte Long_AsLong(const nativeVal_t *v, long *out) {
	lnum_t x;
	L_Of(v, &x);
	byte ok = L_AsLong(&x, out);
	L_Free(&x);
	return ok;
}

void Long_Format(const nativeVal_t *v, int base, nativeBuf_t *out) {
	lnum_t x;
	L_Of(v, &x);
	L_Format(&x, base, 1, out);
	L_Free(&x);
}

byte Long_FromText(const char *s, int base, nativeVal_t *out, const char **end) {
	lnum_t z, t;
	byte neg = 0;
	Native_SetNone(out);
	if ((base != 0 && base < 2) || base > 36) {
		return Native_Raise(NE_VALUE, "invalid base for long literal");
	}
	/* PyLong_FromString: blanks, a sign, blanks, the prefix of the base */
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	if (*s == '+') {
		s++;
	} else if (*s == '-') {
		s++;
		neg = 1;
	}
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	if (base == 0) {
		base = s[0] != '0' ? 10 : s[1] == 'x' || s[1] == 'X' ? 16 : 8;
	}
	if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
	}
	const char *start = s;
	L_New(&z, 0);
	for (;; s++) {
		int k = -1;
		if (*s <= '9') {
			k = *s - '0';
		} else if (*s >= 'a') {
			k = *s - 'a' + 10;
		} else if (*s >= 'A') {
			k = *s - 'A' + 10;
		}
		if (k < 0 || k >= base) {
			break;
		}
		L_MulAdd1(&t, &z, (unsigned)base, (unsigned)k);
		L_Free(&z);
		z = t;
	}
	if (s == start) {
		L_Free(&z);
		return Native_Raise(NE_VALUE, "no digits in long int constant");
	}
	if (neg && z.size) {
		z.size = -z.size;
	}
	*end = s;
	L_Give(&z, out);
	return 1;
}
