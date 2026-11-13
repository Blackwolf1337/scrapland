/*
 * The number protocol of the Python 1.5.2 library on native values (include/scrap/scriptlib.h, stage 7e, batch
 * 7e4c): the binary and unary operators and conversions of Objects/abstract.c with the coercions of object.c
 * (PyNumber_Coerce), the number methods of ints, long ints, floats and complex numbers (intobject.c, longobject.c
 * through ScriptLong.c, floatobject.c, complexobject.c), the operators of class instances (PyInstance_DoBinOp of
 * classobject.c: __coerce__, __add__ / __radd__ ...), the concat and repeat of the sequences, PyObject_Compare with the
 * values it gives, and the sequence protocol of the builtins.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/scriptlib.h"

#ifndef _WIN32
/* src/standalone/crtposix.c: the pow and fmod of the Windows runtime, as the library sees them (pyshim.h) */
double crt_ms_pow(double x, double y);
double crt_ms_fmod(double x, double y);
double crt_ms_hypot(double x, double y);
double crt_ms_atan2(double y, double x);
double crt_ms_exp(double x);
#define LIB_POW crt_ms_pow
#define LIB_FMOD crt_ms_fmod
#define LIB_HYPOT crt_ms_hypot
#define LIB_ATAN2 crt_ms_atan2
#define LIB_EXP crt_ms_exp
#else
#define LIB_POW pow
#define LIB_FMOD fmod
#define LIB_HYPOT hypot
#define LIB_ATAN2 atan2
#define LIB_EXP exp
#endif

/* the kinds of numbers in the order of their coercions */
enum {
	NK_NONE,
	NK_INT,
	NK_LONG,
	NK_FLOAT,
	NK_COMPLEX,
};

static int Num_Kind(const nativeVal_t *v) {
	double re, im;
	switch (v->type) {
	case NV_INT:
		return NK_INT;
	case NV_LONG:
		return NK_LONG;
	case NV_FLOAT:
		return NK_FLOAT;
	}
	return Lib_ComplexOf(v, &re, &im) ? NK_COMPLEX : NK_NONE;
}

/* a class instance (native or the view of a Python one) */
static byte Num_IsInstance(const nativeVal_t *v) {
	return (v->type == NV_REF || v->type == NV_OBJ) && !strcmp(Native_TypeName(v), "instance");
}

byte Lib_IsNumber(const nativeVal_t *v) {
	return Num_Kind(v) != NK_NONE || Num_IsInstance(v);
}

/* the value as a double (int, long, float) */
static double Num_Double(const nativeVal_t *v) {
	return v->type == NV_INT ? (double)v->i : v->type == NV_LONG ? Long_AsDouble(v) : v->f;
}

/* the value as a complex number (int, long, float, complex) */
static void Num_Complex(const nativeVal_t *v, double *re, double *im) {
	if (!Lib_ComplexOf(v, re, im)) {
		*re = Num_Double(v);
		*im = 0.0;
	}
}

/* ---- the compare (PyObject_Compare) ---- */

/* the last rule of PyObject_Compare: values of different types by the names of the types, a number before all others */
static int Num_TypeOrder(const nativeVal_t *a, const nativeVal_t *b) {
	return strcmp(Lib_IsNumber(a) ? "" : Native_TypeName(a), Lib_IsNumber(b) ? "" : Native_TypeName(b));
}

/* the items of a list or tuple (by value, a vector, a list object), NULL for any other value */
static const nativeVal_t *Num_Items(const nativeVal_t *v, nativeVal_t *tmp) {
	Native_SetNone(tmp);
	if (v->type == NV_LIST || v->type == NV_TUPLE) {
		return v;
	}
	if (v->type == NV_VEC) {
		Native_SetEmpty(tmp, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetFloat(Native_Append(tmp), v->v[i]);
		}
		return tmp;
	}
	return NObj_ListData(v);
}

/* the keys of a dict sorted by the compare (PyList_Sort) */
static void Num_SortedKeys(const nativeVal_t *d, nativeVal_t *keys) {
	const nativeVal_t *k, *x;
	int pos = 0;
	Native_SetEmpty(keys, NV_LIST);
	while (Native_DictNext(d, &pos, &k, &x)) {
		Native_Copy(Native_Append(keys), k);
	}
	/* an insertion sort: the keys of one dict are unequal, so the order is that of any sort */
	for (int i = 1; i < keys->n; i++) {
		nativeVal_t t = keys->items[i];
		int j = i;
		for (; j > 0 && Native_Compare(&t, &keys->items[j - 1]) < 0; j--) {
			keys->items[j] = keys->items[j - 1];
		}
		keys->items[j] = t;
	}
}

/* dict_compare (without NEWCMP): the lengths, then the sorted keys and their values */
static int Num_DictCompare(const nativeVal_t *a, const nativeVal_t *b) {
	nativeVal_t ak, bk;
	if (a->n != b->n || !a->n) {
		return a->n < b->n ? -1 : a->n > b->n;
	}
	Num_SortedKeys(a, &ak);
	Num_SortedKeys(b, &bk);
	int res = 0;
	for (int i = 0; i < ak.n && !res; i++) {
		res = Lib_Cmp(&ak.items[i], &bk.items[i]);
		if (!res) {
			res = Lib_Cmp(Native_DictGet(a, &ak.items[i]), Native_DictGet(b, &bk.items[i]));
		}
	}
	Native_Free(&ak);
	Native_Free(&bk);
	return res;
}

int Lib_Cmp(const nativeVal_t *a, const nativeVal_t *b) {
	nativeVal_t ta, tb;
	if (NObj_Is(a, b)) {
		return 0;
	}
	for (int k = 0; k < 2; k++) {
		/* the compare of a type of its own: two of its objects, a complex number with a number */
		const nativeCType_t *t = NObj_CType(k ? b : a);
		if (t && t->compare && (NObj_CType(k ? a : b) == t || (Num_Kind(a) != NK_NONE && Num_Kind(b) != NK_NONE))) {
			return t->compare(a, b);
		}
	}
	if (Num_IsInstance(a) || Num_IsInstance(b)) {
		/* without __cmp__: by the addresses */
		return NObj_Compare(a, b);
	}
	int ka = Num_Kind(a), kb = Num_Kind(b);
	if ((ka != NK_NONE || a->type == NV_NONE) && (kb != NK_NONE || b->type == NV_NONE)) {
		/* coerced: a float with any number as floats, an int with a long as longs; None with a number by the sign as
		 * the test channel's native cmp gave it */
		return Native_Compare(a, b);
	}
	const char *na = Native_TypeName(a), *nb = Native_TypeName(b);
	if (strcmp(na, nb)) {
		return Num_TypeOrder(a, b);
	}
	if (a->type == NV_STR && b->type == NV_STR) {
		/* string_compare: the first bytes, then memcmp, then the lengths */
		int min = a->n < b->n ? a->n : b->n;
		if (min > 0) {
			const unsigned char *x = (const unsigned char *)Native_Text(a), *y = (const unsigned char *)Native_Text(b);
			int c = x[0] - y[0];
			if (c == 0) {
				c = memcmp(x, y, (size_t)min);
			}
			if (c != 0) {
				return c;
			}
		}
		return a->n < b->n ? -1 : a->n > b->n;
	}
	const nativeVal_t *la = Num_Items(a, &ta), *lb = la ? Num_Items(b, &tb) : NULL;
	if (la && lb) {
		/* list_compare, tuplecompare: the first unequal items, else the difference of the lengths */
		int c = 0;
		for (int i = 0; i < la->n && i < lb->n && !c; i++) {
			c = Lib_Cmp(&la->items[i], &lb->items[i]);
		}
		if (!c) {
			c = la->n - lb->n;
		}
		Native_Free(&ta);
		Native_Free(&tb);
		return c;
	}
	if (la) {
		Native_Free(&ta);
	}
	const nativeVal_t *da = a->type == NV_DICT ? a : NObj_DictData(a), *db = b->type == NV_DICT ? b : NObj_DictData(b);
	if (da && db) {
		return Num_DictCompare(da, db);
	}
	return Native_Compare(a, b);
}

/* ---- the coercion ---- */

byte Lib_Coerce(nativeVal_t *a, nativeVal_t *b, byte raise) {
	int ka = Num_Kind(a), kb = Num_Kind(b);
	if (Num_IsInstance(a) || Num_IsInstance(b)) {
		/* instance_coerce without __coerce__: as they are */
		return 1;
	}
	if (!strcmp(Native_TypeName(a), Native_TypeName(b))) {
		return 1;
	}
	if (ka == NK_NONE || kb == NK_NONE) {
		return raise && Native_Raise(NE_TYPE, "number coercion failed");
	}
	/* the lower one to the kind of the higher one (long_coerce, float_coerce, complex_coerce) */
	nativeVal_t *lo = ka < kb ? a : b;
	int k = ka < kb ? kb : ka;
	nativeVal_t t;
	if (k == NK_LONG) {
		Long_FromLong(lo->i, &t);
	} else if (k == NK_FLOAT) {
		Native_SetFloat(&t, Num_Double(lo));
	} else {
		Lib_NewComplex(&t, Num_Double(lo), 0.0);
	}
	Native_Free(lo);
	*lo = t;
	return 1;
}

/* ---- ints ---- */

/* i_divmod */
static byte Int_DivMod(long x, long y, long *div, long *mod) {
	long d;
	if (y == 0) {
		return Native_Raise(NE_ZERODIV, "integer division or modulo");
	}
	/* the negations wrap as the library's -fwrapv build makes them */
	unsigned long ux = x < 0 ? 0ul - (unsigned long)x : (unsigned long)x, uy = y < 0 ? 0ul - (unsigned long)y :
		(unsigned long)y;
	d = (long)(ux / uy);
	if ((x < 0) != (y < 0)) {
		d = (long)(0ul - (unsigned long)d);
	}
	long m = (long)((unsigned long)x - (unsigned long)d * (unsigned long)y);
	if ((m < 0 && y > 0) || (m > 0 && y < 0)) {
		m = (long)((unsigned long)m + (unsigned long)y);
		d = (long)((unsigned long)d - 1);
	}
	*div = d;
	*mod = m;
	return 1;
}

/* int_pow (z 0: none) */
static byte Int_Pow(long iv, long iw, long iz, byte hasz, nativeVal_t *out) {
	long temp = iv, ix = 1, prev;
	if (iw < 0) {
		return Native_Raise(NE_VALUE, "integer to the negative power");
	}
	if (hasz && iz == 0) {
		return Native_Raise(NE_VALUE, "pow(x, y, z) with z==0");
	}
	while (iw > 0) {
		prev = ix;
		if (iw & 1) {
			ix = (long)((unsigned long)ix * (unsigned long)temp);
			if (temp == 0) {
				break;
			}
			if ((temp == -1 ? (long)(0ul - (unsigned long)ix) : ix / temp) != prev) {
				return Native_Raise(NE_OVERFLOW, "integer pow()");
			}
		}
		iw >>= 1;
		if (iw == 0) {
			break;
		}
		prev = temp;
		temp = (long)((unsigned long)temp * (unsigned long)temp);
		if (prev != 0 && (prev == -1 ? (long)(0ul - (unsigned long)temp) : temp / prev) != prev) {
			return Native_Raise(NE_OVERFLOW, "integer pow()");
		}
		if (iz) {
			ix = iz == -1 ? 0 : ix % iz;
			temp = iz == -1 ? 0 : temp % iz;
		}
	}
	if (iz) {
		long d;
		Int_DivMod(ix, iz, &d, &ix);
	}
	Native_SetInt(out, ix);
	return 1;
}

/* int op int of intobject.c for << >> & ^ | */
static byte Int_Bits(int op, long a, long b, nativeVal_t *out) {
	switch (op) {
	case '<':
	case '>':
		if (b < 0) {
			return Native_Raise(NE_VALUE, "negative shift count");
		}
		if (a == 0 || b == 0) {
			Native_SetInt(out, a);
		} else if (b >= (long)(8 * sizeof(long))) {
			Native_SetInt(out, op == '<' ? 0 : a < 0 ? -1 : 0);
		} else if (op == '<') {
			Native_SetInt(out, (long)((unsigned long)a << b));
		} else {
			Native_SetInt(out, a < 0 ? ~(~a >> b) : a >> b);
		}
		return 1;
	case '&':
		Native_SetInt(out, a & b);
		return 1;
	case '^':
		Native_SetInt(out, a ^ b);
		return 1;
	}
	Native_SetInt(out, a | b);
	return 1;
}

/* the pair (d, m) as a tuple */
static void Num_Pair(nativeVal_t *d, nativeVal_t *m, nativeVal_t *out) {
	Native_SetEmpty(out, NV_TUPLE);
	*Native_Append(out) = *d;
	*Native_Append(out) = *m;
}

/* ---- floats ---- */

/* float_pow (z NULL: none) */
static byte Float_Pow(double iv, double iw, const double *z, nativeVal_t *out) {
	double ix;
	long intw = (long)iw;
	if (iw == (double)intw && -10000 < intw && intw < 10000) {
		if (intw == 0) {
			ix = 1.0;
			if (z) {
				ix = LIB_FMOD(1.0, *z);
				if (ix != 0 && *z < 0) {
					ix += *z;
				}
			}
			Native_SetFloat(out, ix);
			return 1;
		}
		errno = 0;
		/* powu */
		double r = 1.0, p = iv;
		long n = intw > 0 ? intw : -intw;
		for (long mask = 1; mask > 0 && n >= mask; mask <<= 1) {
			if (n & mask) {
				r *= p;
			}
			p *= p;
		}
		ix = intw > 0 ? r : 1.0 / r;
	} else {
		if (iv == 0.0) {
			if (iw < 0.0) {
				return Native_Raise(NE_VALUE, "0.0 to a negative power");
			}
			Native_SetFloat(out, 0.0);
			return 1;
		}
		if (iv < 0.0) {
			return Native_Raise(NE_VALUE, "negative number to a float power");
		}
		errno = 0;
		ix = LIB_POW(iv, iw);
	}
	if (errno == 0 && !(-HUGE_VAL <= ix && ix <= HUGE_VAL)) {
		errno = ERANGE;
	}
	if (errno != 0) {
		/* PyErr_SetFromErrno(OverflowError) */
		nativeVal_t v;
		const char *text = strerror(errno);
		Native_SetEmpty(&v, NV_TUPLE);
		Native_SetInt(Native_Append(&v), errno);
		Native_SetText(Native_Append(&v), text, (int)strlen(text));
		Native_RaiseVal(NE_OVERFLOW, &v);
		Native_Free(&v);
		return 0;
	}
	if (z) {
		ix = LIB_FMOD(ix, *z);
		if (ix != 0 && ((iv < 0 && *z > 0) || (iv > 0 && *z < 0))) {
			ix += *z;
		}
	}
	Native_SetFloat(out, ix);
	return 1;
}

/* ---- complex numbers ---- */

/* c_quot: errno EDOM for a zero divisor */
static void Cx_Quot(double ar, double ai, double br, double bi, double *r, double *i) {
	double d = br * br + bi * bi;
	if (d == 0.0) {
		errno = EDOM;
	}
	*r = (ar * br + ai * bi) / d;
	*i = (ai * br - ar * bi) / d;
}

static void Cx_Prod(double ar, double ai, double br, double bi, double *r, double *i) {
	*r = ar * br - ai * bi;
	*i = ar * bi + ai * br;
}

/* c_pow */
static void Cx_Pow(double ar, double ai, double br, double bi, double *r, double *i) {
	if (br == 0.0 && bi == 0.0) {
		*r = 1.0;
		*i = 0.0;
	} else if (ar == 0.0 && ai == 0.0) {
		if (bi != 0.0 || br < 0.0) {
			errno = ERANGE;
		}
		*r = 0.0;
		*i = 0.0;
	} else {
		double vabs = LIB_HYPOT(ar, ai), len = LIB_POW(vabs, br), at = LIB_ATAN2(ai, ar), phase = at * br;
		if (bi != 0.0) {
			len /= LIB_EXP(at * bi);
			phase += bi * log(vabs);
		}
		*r = len * cos(phase);
		*i = len * sin(phase);
	}
}

/* c_powu, c_powi */
static void Cx_PowI(double xr, double xi, long n, double *r, double *i) {
	if (n > 100 || n < -100) {
		Cx_Pow(xr, xi, (double)n, 0.0, r, i);
		return;
	}
	double rr = 1.0, ri = 0.0, pr = xr, pi = xi, t1, t2;
	long m = n > 0 ? n : -n;
	for (long mask = 1; mask > 0 && m >= mask; mask <<= 1) {
		if (m & mask) {
			Cx_Prod(rr, ri, pr, pi, &t1, &t2);
			rr = t1;
			ri = t2;
		}
		Cx_Prod(pr, pi, pr, pi, &t1, &t2);
		pr = t1;
		pi = t2;
	}
	if (n > 0) {
		*r = rr;
		*i = ri;
	} else {
		Cx_Quot(1.0, 0.0, rr, ri, r, i);
	}
}

/* v op w of two complex numbers: 1, 0 with the error, -1 for an operator complex numbers have not */
static int Cx_Op(int op, const nativeVal_t *v, const nativeVal_t *w, nativeVal_t *out) {
	double ar, ai, br, bi, r = 0.0, i = 0.0;
	Num_Complex(v, &ar, &ai);
	Num_Complex(w, &br, &bi);
	errno = 0;
	switch (op) {
	case '+':
		r = ar + br;
		i = ai + bi;
		break;
	case '-':
		r = ar - br;
		i = ai - bi;
		break;
	case '*':
		Cx_Prod(ar, ai, br, bi, &r, &i);
		break;
	case '/':
		Cx_Quot(ar, ai, br, bi, &r, &i);
		if (errno == EDOM) {
			return Native_Raise(NE_ZERODIV, "complex division");
		}
		break;
	case '%':
	case 'd': {
		double dr, di, pr, pi;
		Cx_Quot(ar, ai, br, bi, &dr, &di);
		if (errno == EDOM) {
			return Native_Raise(NE_ZERODIV, op == '%' ? "complex remainder" : "complex divmod()");
		}
		dr = floor(dr);
		di = 0.0;
		Cx_Prod(br, bi, dr, di, &pr, &pi);
		r = ar - pr;
		i = ai - pi;
		if (op == 'd') {
			nativeVal_t d, m;
			Lib_NewComplex(&d, dr, di);
			Lib_NewComplex(&m, r, i);
			Num_Pair(&d, &m, out);
			return 1;
		}
		break;
	}
	case 'p': {
		long n = (long)br;
		if (bi == 0.0 && br == (double)n) {
			Cx_PowI(ar, ai, n, &r, &i);
		} else {
			Cx_Pow(ar, ai, br, bi, &r, &i);
		}
		if (errno == ERANGE) {
			return Native_Raise(NE_VALUE, "0.0 to a negative or complex power");
		}
		break;
	}
	default:
		return -1;
	}
	Lib_NewComplex(out, r, i);
	return 1;
}

/* ---- the operators ---- */

static const char *Num_OpText(int op) {
	switch (op) {
	case 'p':
		return "**";
	case '<':
		return "<<";
	case '>':
		return ">>";
	case 'd':
		return "divmod()";
	case '+':
		return "+";
	case '-':
		return "-";
	case '*':
		return "*";
	case '/':
		return "/";
	case '%':
		return "%";
	case '&':
		return "&";
	case '^':
		return "^";
	}
	return "|";
}

/* the special method names of an operator of PyNumber_* */
static void Num_OpNames(int op, const char **name, const char **rname) {
	static const struct {
		int op;
		const char *name, *rname;
	} names[] = {
		{ '+', "__add__", "__radd__" },
		{ '-', "__sub__", "__rsub__" },
		{ '*', "__mul__", "__rmul__" },
		{ '/', "__div__", "__rdiv__" },
		{ '%', "__mod__", "__rmod__" },
		{ 'd', "__divmod__", "__rdivmod__" },
		{ 'p', "__pow__", "__rpow__" },
		{ '<', "__lshift__", "__rlshift__" },
		{ '>', "__rshift__", "__rrshift__" },
		{ '&', "__and__", "__rand__" },
		{ '^', "__xor__", "__rxor__" },
		{ '|', "__or__", "__ror__" },
	};
	for (size_t k = 0; k < sizeof(names) / sizeof(names[0]); k++) {
		if (names[k].op == op) {
			*name = names[k].name;
			*rname = names[k].rname;
			return;
		}
	}
	*name = *rname = "";
}

/* halfbinop of classobject.c: 1 to try the other half, 0 with the result, -1 with the error */
static int Num_HalfBinOp(int op, const nativeVal_t *v, const nativeVal_t *w, const char *name, byte swapped,
	nativeVal_t *out) {
	nativeVal_t f, coerced, arg;
	const nativeVal_t *v1 = v, *w1 = w;
	if (!Num_IsInstance(v)) {
		return 1;
	}
	Native_SetNone(&coerced);
	if (!NObj_GetAttr(v, "__coerce__", &f)) {
		Native_ErrClear();
	} else {
		Native_Copy(&arg, w);
		byte ok = NObj_Call(&f, &arg, 1, &coerced);
		Native_Free(&arg);
		Native_Free(&f);
		if (!ok) {
			return -1;
		}
		if (coerced.type == NV_NONE) {
			return 1;
		}
		if (coerced.type != NV_TUPLE || coerced.n != 2) {
			Native_Free(&coerced);
			return Native_Raise(NE_TYPE, "coercion should return None or 2-tuple"), -1;
		}
		v1 = &coerced.items[0];
		w1 = &coerced.items[1];
		if (!NObj_Is(v1, v) && !Num_IsInstance(v1) && !Num_IsInstance(w1)) {
			byte ok = swapped ? Lib_BinOp(op, w1, v1, out) : Lib_BinOp(op, v1, w1, out);
			Native_Free(&coerced);
			return ok ? 0 : -1;
		}
	}
	if (!NObj_GetAttr(v1, name, &f)) {
		Native_Free(&coerced);
		if (!Native_ErrIs(NE_ATTRIBUTE)) {
			return -1;
		}
		Native_ErrClear();
		return 1;
	}
	Native_Copy(&arg, w1);
	byte ok = NObj_Call(&f, &arg, 1, out);
	Native_Free(&arg);
	Native_Free(&f);
	Native_Free(&coerced);
	return ok ? 0 : -1;
}

/* PyInstance_DoBinOp */
static byte Num_InstanceOp(int op, const nativeVal_t *v, const nativeVal_t *w, nativeVal_t *out) {
	const char *name, *rname;
	Num_OpNames(op, &name, &rname);
	int r = Num_HalfBinOp(op, v, w, name, 0, out);
	if (r <= 0) {
		return r == 0;
	}
	r = Num_HalfBinOp(op, w, v, rname, 1, out);
	if (r <= 0) {
		return r == 0;
	}
	return Native_Raise(NE_TYPE, "%s nor %s defined for these operands", name, rname);
}

/* the sequence a value is for the operators: 1 a text, list or tuple (by value, a list object), 2 an xrange, 3 a
 * buffer, 0 none */
static int Num_Seq(const nativeVal_t *v) {
	if (v->type == NV_STR || v->type == NV_LIST || v->type == NV_TUPLE || v->type == NV_VEC || NObj_Kind(v) == NO_LIST) {
		return 1;
	}
	if (NObj_CObjOf(v, &lib_xrangeType)) {
		return 2;
	}
	return NObj_CObjOf(v, &lib_bufferType) ? 3 : 0;
}

/* sq_concat and sq_repeat of a sequence of Num_Seq */
static byte Num_SeqOp(int op, int seq, const nativeVal_t *v, const nativeVal_t *w, nativeVal_t *out) {
	nativeVal_t tv, tw;
	if (op == '*' && w->type != NV_INT) {
		return Native_Raise(NE_TYPE, "can't multiply sequence with non-int");
	}
	if (seq == 2) {
		return op == '*' ? Lib_RangeRepeat(v, w->i, out) : Native_Raise(NE_TYPE, "cannot concatenate xrange objects");
	}
	if (seq == 3) {
		return Lib_BufferOp(op, v, w, out) > 0;
	}
	/* a list object (or the view of a list) as its items, as list_concat and list_repeat give a new list */
	byte obj = NObj_Kind(v) == NO_LIST;
	Native_SetNone(&tv);
	Native_SetNone(&tw);
	if (obj) {
		if (!Lib_SeqList(v, NV_LIST, &tv)) {
			return 0;
		}
		v = &tv;
	}
	if (op == '+' && NObj_Kind(w) == NO_LIST) {
		if (!Lib_SeqList(w, NV_LIST, &tw)) {
			Native_Free(&tv);
			return 0;
		}
		w = &tw;
	}
	byte ok = Native_Arith((char)op, v, w, out);
	Native_Free(&tv);
	Native_Free(&tw);
	if (ok && obj) {
		NObj_Share(out);
	}
	return ok;
}

/* the number method op of two numbers of one kind (after the coercion) */
static byte Num_Method(int op, int kind, const nativeVal_t *v, const nativeVal_t *w, nativeVal_t *out) {
	switch (kind) {
	case NK_INT:
		if (strchr("+-*/%", op)) {
			return Native_Arith((char)op, v, w, out);
		}
		if (op == 'd') {
			long d, m;
			nativeVal_t x, y;
			if (!Int_DivMod(v->i, w->i, &d, &m)) {
				return 0;
			}
			Native_SetInt(&x, d);
			Native_SetInt(&y, m);
			Num_Pair(&x, &y, out);
			return 1;
		}
		if (op == 'p') {
			return Int_Pow(v->i, w->i, 0, 0, out);
		}
		return Int_Bits(op, v->i, w->i, out);
	case NK_LONG:
		return op == 'p' ? Long_Pow(v, w, NULL, out) : Long_Op(op, v, w, out);
	case NK_FLOAT:
		if (strchr("+-*/%", op)) {
			return Native_Arith((char)op, v, w, out);
		}
		if (op == 'd') {
			/* float_divmod */
			double vx = v->f, wx = w->f;
			nativeVal_t x, y;
			if (wx == 0.0) {
				return Native_Raise(NE_ZERODIV, "float divmod()");
			}
			double mod = LIB_FMOD(vx, wx), div = (vx - mod) / wx;
			if (wx * mod < 0) {
				mod += wx;
				div -= 1.0;
			}
			Native_SetFloat(&x, div);
			Native_SetFloat(&y, mod);
			Num_Pair(&x, &y, out);
			return 1;
		}
		if (op == 'p') {
			return Float_Pow(v->f, w->f, NULL, out);
		}
		break;
	case NK_COMPLEX: {
		int r = Cx_Op(op, v, w, out);
		if (r >= 0) {
			return (byte)r;
		}
		break;
	}
	}
	return Native_Raise(NE_TYPE, "bad operand type(s) for %s", Num_OpText(op));
}

byte Lib_BinOp(int op, const nativeVal_t *v, const nativeVal_t *w, nativeVal_t *out) {
	nativeVal_t a, b;
	Native_SetNone(out);
	if (op == '%' && v->type == NV_STR) {
		return Native_Format(v, w, out);
	}
	if (Num_IsInstance(v) || Num_IsInstance(w)) {
		return Num_InstanceOp(op, v, w, out);
	}
	if (op == 'p') {
		return Lib_Power(v, w, NULL, out);
	}
	int seq = Num_Seq(v);
	if (op == '+' && seq) {
		return Num_SeqOp(op, seq, v, w, out);
	}
	if (op == '*' && Num_Kind(v) != NK_NONE && (Num_Seq(w) || Num_IsInstance(w))) {
		/* number * sequence: sequence * number */
		const nativeVal_t *t = v;
		v = w;
		w = t;
		seq = Num_Seq(v);
	}
	if (Num_Kind(v) != NK_NONE) {
		Native_Copy(&a, v);
		Native_Copy(&b, w);
		byte ok = Lib_Coerce(&a, &b, 1) && Num_Method(op, Num_Kind(&a), &a, &b, out);
		Native_Free(&a);
		Native_Free(&b);
		return ok;
	}
	if (op == '*' && seq) {
		return Num_SeqOp(op, seq, v, w, out);
	}
	return Native_Raise(NE_TYPE, "bad operand type(s) for %s", Num_OpText(op));
}

byte Lib_Power(const nativeVal_t *v, const nativeVal_t *w, const nativeVal_t *z, nativeVal_t *out) {
	nativeVal_t a, b, c;
	Native_SetNone(out);
	if (!z || z->type == NV_NONE) {
		/* do_pow */
		if (Num_IsInstance(v) || Num_IsInstance(w)) {
			return Num_InstanceOp('p', v, w, out);
		}
		if (!Lib_IsNumber(v) || !Lib_IsNumber(w)) {
			return Native_Raise(NE_TYPE, "pow(x, y) requires numeric arguments");
		}
		Native_Copy(&a, v);
		Native_Copy(&b, w);
		byte ok = Lib_Coerce(&a, &b, 1) && Num_Method('p', Num_Kind(&a), &a, &b, out);
		Native_Free(&a);
		Native_Free(&b);
		return ok;
	}
	if (Num_IsInstance(v)) {
		/* instance_pow of the ternary form */
		nativeVal_t f, args[2];
		if (!NObj_GetAttr(v, "__pow__", &f)) {
			return 0;
		}
		Native_Copy(&args[0], w);
		Native_Copy(&args[1], z);
		byte ok = NObj_Call(&f, args, 2, out);
		Native_Free(&args[0]);
		Native_Free(&args[1]);
		Native_Free(&f);
		return ok;
	}
	if (!Lib_IsNumber(v) || !Lib_IsNumber(z) || !Lib_IsNumber(w)) {
		return Native_Raise(NE_TYPE, "pow(x, y, z) requires numeric arguments");
	}
	/* v and w coerced, v with z, w with that z */
	Native_Copy(&a, v);
	Native_Copy(&b, w);
	Native_Copy(&c, z);
	byte ok = Lib_Coerce(&a, &b, 1) && Lib_Coerce(&a, &c, 1) && Lib_Coerce(&b, &c, 1);
	if (ok) {
		switch (Num_Kind(&a)) {
		case NK_INT:
			ok = Int_Pow(a.i, b.i, c.i, 1, out);
			break;
		case NK_LONG:
			ok = Long_Pow(&a, &b, &c, out);
			break;
		case NK_FLOAT:
			ok = Float_Pow(a.f, b.f, &c.f, out);
			break;
		default:
			ok = Native_Raise(NE_VALUE, "complex modulo");
			break;
		}
	}
	Native_Free(&a);
	Native_Free(&b);
	Native_Free(&c);
	return ok;
}

/* an instance's unary special method (instance_neg ...): getattr, call */
static byte Num_InstanceUnary(const nativeVal_t *v, const char *name, nativeVal_t *out) {
	nativeVal_t f;
	if (!NObj_GetAttr(v, name, &f)) {
		return 0;
	}
	byte ok = NObj_Call(&f, NULL, 0, out);
	Native_Free(&f);
	return ok;
}

byte Lib_Unary(int op, const nativeVal_t *v, nativeVal_t *out) {
	static const char *const names[] = { "__neg__", "__pos__", "__invert__", "__abs__" };
	static const char *const errs[] = { "bad operand type for unary -", "bad operand type for unary +",
		"bad operand type for unary ~", "bad operand type for abs()" };
	int k = op == '-' ? 0 : op == '+' ? 1 : op == '~' ? 2 : 3;
	double re, im;
	Native_SetNone(out);
	if (Num_IsInstance(v)) {
		return Num_InstanceUnary(v, names[k], out);
	}
	switch (Num_Kind(v)) {
	case NK_INT:
		if (k == 2) {
			Native_SetInt(out, ~v->i);
			return 1;
		}
		if (k == 1 || (k == 3 && v->i >= 0)) {
			Native_SetInt(out, v->i);
			return 1;
		}
		if (v->i == LONG_MIN) {
			return Native_Raise(NE_OVERFLOW, "integer negation");
		}
		Native_SetInt(out, -v->i);
		return 1;
	case NK_LONG:
		Long_Unary(k == 0 ? '-' : k == 1 ? '+' : k == 2 ? '~' : 'a', v, out);
		return 1;
	case NK_FLOAT:
		if (k == 2) {
			break;
		}
		Native_SetFloat(out, k == 0 ? -v->f : k == 1 || v->f >= 0 ? v->f : -v->f);
		return 1;
	case NK_COMPLEX:
		Lib_ComplexOf(v, &re, &im);
		if (k == 2) {
			break;
		}
		if (k == 3) {
			Native_SetFloat(out, LIB_HYPOT(re, im));
		} else if (k == 0) {
			Lib_NewComplex(out, -re, -im);
		} else {
			Native_Copy(out, v);
		}
		return 1;
	}
	return Native_Raise(NE_TYPE, "%s", errs[k]);
}

/* the nb_int, nb_long, nb_float conversion (which 0, 1, 2) of a value that is no text */
static byte Num_Convert(int which, const nativeVal_t *v, nativeVal_t *out) {
	static const char *const names[] = { "__int__", "__long__", "__float__" };
	static const char *const cx[] = { "can't convert complex to int; use e.g. int(abs(z))",
		"can't convert complex to long; use e.g. long(abs(z))", "can't convert complex to float; use e.g. abs(z)" };
	static const char *const errs[] = { "object can't be converted to int", "object can't be converted to long",
		"object can't be converted to float" };
	Native_SetNone(out);
	if (Num_IsInstance(v)) {
		return Num_InstanceUnary(v, names[which], out);
	}
	switch (Num_Kind(v)) {
	case NK_INT:
		if (which == 1) {
			Long_FromLong(v->i, out);
		} else if (which == 2) {
			Native_SetFloat(out, (double)v->i);
		} else {
			Native_SetInt(out, v->i);
		}
		return 1;
	case NK_LONG:
		if (which == 0) {
			long l;
			if (!Long_AsLong(v, &l)) {
				return 0;
			}
			Native_SetInt(out, l);
		} else if (which == 2) {
			Native_SetFloat(out, Long_AsDouble(v));
		} else {
			Native_Copy(out, v);
		}
		return 1;
	case NK_FLOAT:
		if (which == 1) {
			Long_FromDouble(v->f, out);
			return 1;
		}
		return which == 0 ? Native_ToInt(v, out) : (Native_SetFloat(out, v->f), 1);
	case NK_COMPLEX:
		return Native_Raise(NE_TYPE, "%s", cx[which]);
	}
	return Native_Raise(NE_TYPE, "%s", errs[which]);
}

byte Lib_AsLong(const nativeVal_t *v, long *out) {
	nativeVal_t r;
	if (v->type == NV_INT) {
		*out = v->i;
		return 1;
	}
	if (!Lib_IsNumber(v)) {
		return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	}
	if (!Num_Convert(0, v, &r)) {
		return 0;
	}
	byte ok = r.type == NV_INT || Native_Raise(NE_TYPE, "nb_int should return int object");
	*out = r.i;
	Native_Free(&r);
	return ok;
}

byte Lib_AsDouble(const nativeVal_t *v, double *out) {
	nativeVal_t r;
	if (v->type == NV_FLOAT) {
		*out = v->f;
		return 1;
	}
	if (!Lib_IsNumber(v)) {
		return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	}
	if (!Num_Convert(2, v, &r)) {
		return 0;
	}
	byte ok = r.type == NV_FLOAT || Native_Raise(NE_TYPE, "nb_float should return float object");
	*out = r.f;
	Native_Free(&r);
	return ok;
}

byte Lib_Int(const nativeVal_t *v, nativeVal_t *out) {
	return v->type == NV_STR ? Native_ToInt(v, out) : Num_Convert(0, v, out);
}

byte Lib_Float(const nativeVal_t *v, nativeVal_t *out) {
	return v->type == NV_STR ? Native_ToFloat(v, out) : Num_Convert(2, v, out);
}

byte Lib_Long(const nativeVal_t *v, nativeVal_t *out) {
	if (v->type != NV_STR) {
		return Num_Convert(1, v, out);
	}
	/* long_from_string: the digits in base 10, an L, blanks */
	const char *s = Native_Text(v), *end;
	if (!Long_FromText(s, 10, out, &end)) {
		Native_ErrClear();
		return Native_Raise(NE_VALUE, "invalid literal for long(): %.200s", s);
	}
	if (*end == 'L' || *end == 'l') {
		end++;
	}
	while (*end && isspace((unsigned char)*end)) {
		end++;
	}
	if (*end != '\0') {
		Native_Free(out);
		return Native_Raise(NE_VALUE, "invalid literal for long(): %.200s", s);
	}
	if (end != s + v->n) {
		Native_Free(out);
		return Native_Raise(NE_VALUE, "null byte in argument for long()");
	}
	return 1;
}

byte Lib_HexOct(const nativeVal_t *v, int base, nativeVal_t *out) {
	char buf[100];
	Native_SetNone(out);
	if (Num_IsInstance(v)) {
		return Num_InstanceUnary(v, base == 16 ? "__hex__" : "__oct__", out);
	}
	if (v->type == NV_INT) {
		if (base == 16) {
			snprintf(buf, sizeof(buf), "0x%lx", v->i);
		} else if (v->i == 0) {
			Str_Copy(buf, "0", sizeof(buf));
		} else {
			snprintf(buf, sizeof(buf), "0%lo", v->i);
		}
		Native_SetStr(out, buf);
		return 1;
	}
	if (v->type == NV_LONG) {
		nativeBuf_t b;
		Native_BufInit(&b);
		Long_Format(v, base, &b);
		Native_SetText(out, b.p, b.len);
		Native_BufFree(&b);
		return 1;
	}
	return Native_Raise(NE_TYPE, base == 16 ? "hex() argument can't be converted to hex" :
		"oct() argument can't be converted to oct");
}

/* ---- the sequence protocol of the builtins ---- */

byte Lib_IsSeq(const nativeVal_t *v) {
	switch (v->type) {
	case NV_STR:
	case NV_TUPLE:
	case NV_LIST:
	case NV_VEC:
		return 1;
	case NV_REF:
		if (NObj_CType(v)) {
			return NObj_CType(v)->item != NULL;
		}
		return NObj_Kind(v) == NO_LIST || NObj_Kind(v) == NO_INSTANCE ||
			(NObj_Kind(v) != NO_DICT && Num_IsInstance(v));
	}
	return 0;
}

int Lib_SeqLen(const nativeVal_t *v) {
	int n;
	if (v->type == NV_REF) {
		return NObj_Len(v);
	}
	return Native_Len(v, &n) ? n : -1;
}

byte Lib_SeqItem(const nativeVal_t *v, int i, nativeVal_t *out) {
	if (v->type == NV_REF) {
		nativeVal_t key;
		Native_SetInt(&key, i);
		return NObj_GetItem(v, &key, out);
	}
	return Native_SeqItem(v, i, out);
}

byte Lib_SeqList(const nativeVal_t *v, nativeType_t type, nativeVal_t *out) {
	Native_SetEmpty(out, type);
	for (int i = 0;; i++) {
		int n = Lib_SeqLen(v);
		if (n < 0) {
			Native_Free(out);
			return 0;
		}
		if (i >= n) {
			return 1;
		}
		if (!Lib_SeqItem(v, i, Native_Append(out))) {
			Native_Free(out);
			return 0;
		}
	}
}
