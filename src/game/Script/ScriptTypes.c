/*
 * The object types of the Python 1.5.2 library in C (include/scrap/scriptlib.h, stage 7e, batch 7e4c): complex
 * (Objects/complexobject.c), xrange (rangeobject.c), slice and Ellipsis (sliceobject.c), buffer (bufferobject.c) and
 * file (fileobject.c) as C objects of the native object layer (scrap/nativeobj.h), with their reprs, attributes,
 * methods, sequence protocols, compares, hashes and error texts. The number methods of complex are in ScriptNumber.c.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "scrap/global.h"
#include "scrap/scriptlib.h"
#include "scrap/scriptmodes.h"

/* OBJLIB 9: the Python object of a C object of this file that crosses (a value of a module variable held in a tuple
 * kept for its identity): an object of the library's type with the same contents, the face of the native one */

/* ---- complex ---- */

typedef struct {
	double re, im;
} libComplex_t;

static void Complex_Drop(void *ctx) {
	Mem_Free(ctx);
}

/* complex_buf_repr */
static byte Complex_Repr(void *ctx, uintptr_t addr, nativeBuf_t *b, nativeRepr_t mode) {
	const libComplex_t *c = ctx;
	char buf[100];
	(void)addr;
	(void)mode;
	if (c->re == 0.0) {
		snprintf(buf, sizeof(buf), "%.12gj", c->im);
	} else {
		snprintf(buf, sizeof(buf), "(%.12g%+.12gj)", c->re, c->im);
	}
	Native_BufAdd(b, buf, -1);
	return 1;
}

static void Complex_Print(void *ctx, char *buf, int size) {
	nativeBuf_t b;
	Native_BufInit(&b);
	Complex_Repr(ctx, 0, &b, NR_REPR);
	Str_Copy(buf, b.p, size);
	Native_BufFree(&b);
}

static byte Complex_GetAttr(void *ctx, const char *name, nativeVal_t *out) {
	const libComplex_t *c = ctx;
	if (!strcmp(name, "real")) {
		Native_SetFloat(out, c->re);
		return 1;
	}
	if (!strcmp(name, "imag")) {
		Native_SetFloat(out, c->im);
		return 1;
	}
	if (!strcmp(name, "__members__")) {
		Native_SetEmpty(out, NV_LIST);
		Native_SetStr(Native_Append(out), "imag");
		Native_SetStr(Native_Append(out), "real");
		return 1;
	}
	return Native_Raise(NE_ATTRIBUTE, "%s", name);
}

static byte Complex_Conjugate(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const libComplex_t *c = ctx;
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Lib_NewComplex(out, c->re, -c->im);
	return 1;
}

/* the value as a complex number of complex_coerce (an int, long, float or complex): 1, 0 for any other value */
static byte Complex_Coerced(const nativeVal_t *v, libComplex_t *c) {
	double re, im;
	c->im = 0.0;
	switch (v->type) {
	case NV_INT:
		c->re = (double)v->i;
		return 1;
	case NV_FLOAT:
		c->re = v->f;
		return 1;
	case NV_LONG:
		c->re = Long_AsDouble(v);
		return 1;
	}
	if (Lib_ComplexOf(v, &re, &im)) {
		c->re = re;
		c->im = im;
		return 1;
	}
	return 0;
}

/* complex_compare after the coercion of a number; another value by the names of the types */
static int Complex_Compare(const nativeVal_t *a, const nativeVal_t *b) {
	libComplex_t i, j;
	if (!Complex_Coerced(a, &i) || !Complex_Coerced(b, &j)) {
		return Lib_Cmp(a, b);
	}
	if (i.re == j.re && i.im == j.im) {
		return 0;
	}
	if (i.re != j.re) {
		return i.re < j.re ? -1 : 1;
	}
	return i.im < j.im ? -1 : 1;
}

/* the top 32 and next 32 bits of a fraction, the exponent of complex_hash */
static long Complex_PartHash(double fractpart, double intpart) {
	int expo;
	fractpart = frexp(fractpart, &expo) * 2147483648.0;
	long hipart = (long)fractpart;
	fractpart = (fractpart - (double)hipart) * 2147483648.0;
	return hipart + (long)fractpart + (long)intpart + (expo << 15);
}

static byte Complex_Hash(void *ctx, long *out) {
	const libComplex_t *c = ctx;
	double intpart, fractpart = modf(c->re, &intpart);
	long x;
	if (fractpart == 0.0 && c->im == 0.0) {
		if (intpart > 0x7fffffffL || -intpart > 0x7fffffffL) {
			nativeVal_t l;
			Long_FromDouble(c->re, &l);
			byte ok = Native_Hash(&l, out);
			Native_Free(&l);
			return ok;
		}
		x = (long)intpart;
	} else {
		x = Complex_PartHash(fractpart, intpart);
		if (c->im != 0.0) {
			fractpart = modf(c->im, &intpart);
			x ^= Complex_PartHash(fractpart, intpart);
		}
	}
	*out = x == -1 ? -2 : x;
	return 1;
}

static byte Complex_Truth(void *ctx) {
	const libComplex_t *c = ctx;
	return c->re != 0.0 || c->im != 0.0;
}

static PyObject *Complex_ToPy(void *ctx) {
	const libComplex_t *c = ctx;
	return PyComplex_FromDoubles(c->re, c->im);
}

static const nativeCMethod_t complex_methods[] = {
	{ "conjugate", Complex_Conjugate },
	{ NULL, NULL },
};

const nativeCType_t lib_complexType = {
	.name = "complex",
	.getattr = Complex_GetAttr,
	.print = Complex_Print,
	.drop = Complex_Drop,
	.repr = Complex_Repr,
	.compare = Complex_Compare,
	.hash = Complex_Hash,
	.truth = Complex_Truth,
	.methods = complex_methods,
	.pytype = &PyComplex_Type,
	.toPy = Complex_ToPy,
};

void Lib_NewComplex(nativeVal_t *out, double re, double im) {
	libComplex_t *c = Mem_Alloc(sizeof(*c));
	c->re = re;
	c->im = im;
	NObj_NewCObj(out, &lib_complexType, c);
}

byte Lib_ComplexOf(const nativeVal_t *v, double *re, double *im) {
	const libComplex_t *c = NObj_CObjOf(v, &lib_complexType);
	if (!c) {
		return 0;
	}
	*re = c->re;
	*im = c->im;
	return 1;
}

/* ---- xrange ---- */

typedef struct {
	long start, step, len;
	int reps;
} libRange_t;

static void Range_Drop(void *ctx) {
	Mem_Free(ctx);
}

static byte Range_Repr(void *ctx, uintptr_t addr, nativeBuf_t *b, nativeRepr_t mode) {
	const libRange_t *r = ctx;
	char buf[100];
	(void)addr;
	(void)mode;
	snprintf(buf, sizeof(buf), "(xrange(%ld, %ld, %ld) * %d)", r->start, (long)((unsigned long)r->start +
		(unsigned long)r->len * (unsigned long)r->step), r->step, r->reps);
	Native_BufAdd(b, buf, -1);
	return 1;
}

/* range_print: the items */
static void Range_Print(void *ctx, char *buf, int size) {
	const libRange_t *r = ctx;
	nativeBuf_t b;
	char item[32];
	Native_BufInit(&b);
	Native_BufAdd(&b, "(", 1);
	for (int i = 0; i < r->reps; ++i) {
		for (long j = 0; j < r->len; ++j) {
			snprintf(item, sizeof(item), "%s%ld", j > 0 || i > 0 ? ", " : "", r->start + j * r->step);
			Native_BufAdd(&b, item, -1);
		}
	}
	Native_BufAdd(&b, r->len == 1 && r->reps == 1 ? ",)" : ")", -1);
	Str_Copy(buf, b.p, size);
	Native_BufFree(&b);
}

static int Range_Len(void *ctx) {
	const libRange_t *r = ctx;
	return (int)(r->len * r->reps);
}

static byte Range_Item(void *ctx, long i, nativeVal_t *out) {
	const libRange_t *r = ctx;
	if (i < 0 || i >= r->len * r->reps) {
		return Native_Raise(NE_INDEX, "xrange object index out of range");
	}
	Native_SetInt(out, r->start + (i % r->len) * r->step);
	return 1;
}

static byte Range_Slice(void *ctx, long low, long high, nativeVal_t *out) {
	const libRange_t *r = ctx;
	if (r->reps != 1) {
		return Native_Raise(NE_TYPE, "cannot slice a replicated xrange");
	}
	low = (int)low;
	high = (int)high;
	if (low < 0) {
		low = 0;
	} else if (low > r->len) {
		low = r->len;
	}
	if (high < 0) {
		high = 0;
	}
	if (high < low) {
		high = low;
	} else if (high > r->len) {
		high = r->len;
	}
	Lib_NewXRange(out, low * r->step + r->start, high - low, r->step, 1);
	return 1;
}

/* range_compare: the differences of the fields, cut to an int as the C function returns them */
static int Range_Compare(const nativeVal_t *a, const nativeVal_t *b) {
	const libRange_t *x = NObj_CObjOf(a, &lib_xrangeType), *y = NObj_CObjOf(b, &lib_xrangeType);
	if (!x || !y) {
		return Lib_Cmp(a, b);
	}
	if (x->start != y->start) {
		return (int)(x->start - y->start);
	}
	if (x->step != y->step) {
		return (int)(x->step - y->step);
	}
	if (x->len != y->len) {
		return (int)(x->len - y->len);
	}
	return x->reps - y->reps;
}

static byte Range_ToList(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	const libRange_t *r = ctx;
	(void)a;
	if (n) {
		/* PyArg_Parse(args, "") */
		return Native_Raise(NE_TYPE, "function requires no arguments");
	}
	Native_SetEmpty(out, NV_LIST);
	for (long j = 0; j < r->len * r->reps; ++j) {
		Native_SetInt(Native_Append(out), r->start + (j % r->len) * r->step);
	}
	return 1;
}

static PyObject *Range_ToPy(void *ctx) {
	const libRange_t *r = ctx;
	return PyRange_New(r->start, r->len, r->step, r->reps);
}

static const nativeCMethod_t range_methods[] = {
	{ "tolist", Range_ToList },
	{ NULL, NULL },
};

const nativeCType_t lib_xrangeType = {
	.name = "xrange",
	.print = Range_Print,
	.drop = Range_Drop,
	.repr = Range_Repr,
	.len = Range_Len,
	.item = Range_Item,
	.slice = Range_Slice,
	.compare = Range_Compare,
	.methods = range_methods,
	.pytype = &PyRange_Type,
	.toPy = Range_ToPy,
};

void Lib_NewXRange(nativeVal_t *out, long start, long len, long step, int reps) {
	libRange_t *r = Mem_Alloc(sizeof(*r));
	r->start = start;
	r->len = len;
	r->step = step;
	r->reps = reps;
	NObj_NewCObj(out, &lib_xrangeType, r);
}

/* repeat of an xrange (range_repeat): 1, 0 for a value that is no xrange */
byte Lib_RangeRepeat(const nativeVal_t *v, long n, nativeVal_t *out) {
	const libRange_t *r = NObj_CObjOf(v, &lib_xrangeType);
	if (!r) {
		return 0;
	}
	if (n < 0) {
		Lib_NewXRange(out, 0, 0, 1, 1);
	} else if (n == 1) {
		Native_Copy(out, v);
	} else {
		Lib_NewXRange(out, r->start, r->len, r->step, (int)(r->reps * n));
	}
	return 1;
}

/* ---- slice ---- */

typedef struct {
	nativeVal_t start, stop, step;
} libSlice_t;

static void Slice_Drop(void *ctx) {
	libSlice_t *s = ctx;
	Native_Free(&s->start);
	Native_Free(&s->stop);
	Native_Free(&s->step);
	Mem_Free(s);
}

static byte Slice_Repr(void *ctx, uintptr_t addr, nativeBuf_t *b, nativeRepr_t mode) {
	const libSlice_t *s = ctx;
	(void)addr;
	(void)mode;
	Native_BufAdd(b, "slice(", -1);
	byte ok = NObj_Repr(b, &s->start, NR_REPR);
	Native_BufAdd(b, ", ", -1);
	ok = ok && NObj_Repr(b, &s->stop, NR_REPR);
	Native_BufAdd(b, ", ", -1);
	ok = ok && NObj_Repr(b, &s->step, NR_REPR);
	Native_BufAdd(b, ")", 1);
	return ok;
}

static byte Slice_GetAttr(void *ctx, const char *name, nativeVal_t *out) {
	const libSlice_t *s = ctx;
	if (!strcmp(name, "start")) {
		Native_Copy(out, &s->start);
		return 1;
	}
	if (!strcmp(name, "stop")) {
		Native_Copy(out, &s->stop);
		return 1;
	}
	if (!strcmp(name, "step")) {
		Native_Copy(out, &s->step);
		return 1;
	}
	if (!strcmp(name, "__members__")) {
		Native_SetEmpty(out, NV_LIST);
		Native_SetStr(Native_Append(out), "start");
		Native_SetStr(Native_Append(out), "stop");
		Native_SetStr(Native_Append(out), "step");
		return 1;
	}
	return Native_Raise(NE_ATTRIBUTE, "%s", name);
}

static PyObject *Slice_ToPy(void *ctx) {
	const libSlice_t *s = ctx;
	PyObject *a = Native_ToPy(&s->start), *b = a ? Native_ToPy(&s->stop) : NULL, *c = b ? Native_ToPy(&s->step) : NULL;
	PyObject *r = c ? PySlice_New(a, b, c) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(b);
	Py_XDECREF(c);
	return r;
}

const nativeCType_t lib_sliceType = {
	.name = "slice",
	.getattr = Slice_GetAttr,
	.drop = Slice_Drop,
	.repr = Slice_Repr,
	.pytype = &PySlice_Type,
	.toPy = Slice_ToPy,
};

void Lib_NewSlice(nativeVal_t *out, const nativeVal_t *start, const nativeVal_t *stop, const nativeVal_t *step) {
	libSlice_t *s = Mem_Alloc(sizeof(*s));
	Native_SetNone(&s->start);
	Native_SetNone(&s->stop);
	Native_SetNone(&s->step);
	if (start) {
		Native_Copy(&s->start, start);
	}
	if (stop) {
		Native_Copy(&s->stop, stop);
	}
	if (step) {
		Native_Copy(&s->step, step);
	}
	NObj_NewCObj(out, &lib_sliceType, s);
}

/* ---- Ellipsis ---- */

static byte Ellipsis_Repr(void *ctx, uintptr_t addr, nativeBuf_t *b, nativeRepr_t mode) {
	(void)ctx;
	(void)addr;
	(void)mode;
	Native_BufAdd(b, "Ellipsis", -1);
	return 1;
}

static PyObject *Ellipsis_ToPy(void *ctx) {
	(void)ctx;
	Py_INCREF(Py_Ellipsis);
	return Py_Ellipsis;
}

const nativeCType_t lib_ellipsisType = {
	.name = "ellipsis",
	.repr = Ellipsis_Repr,
	.toPy = Ellipsis_ToPy,
};

void Lib_Ellipsis(nativeVal_t *out) {
	static nativeVal_t ellipsis;
	if (ellipsis.type != NV_REF) {
		NObj_NewCObj(&ellipsis, &lib_ellipsisType, NULL);
	}
	Native_Copy(out, &ellipsis);
}

/* ---- buffer ---- */

typedef struct {
	nativeVal_t base;		/* the text the buffer reads */
	int offset, size;
} libBuffer_t;

static void Buffer_Drop(void *ctx) {
	libBuffer_t *b = ctx;
	Native_Free(&b->base);
	Mem_Free(b);
}

static const char *Buffer_Ptr(const libBuffer_t *b) {
	return Native_Text(&b->base) + b->offset;
}

static byte Buffer_Repr(void *ctx, uintptr_t addr, nativeBuf_t *out, nativeRepr_t mode) {
	const libBuffer_t *b = ctx;
	char buf[300];
	(void)mode;
	snprintf(buf, sizeof(buf), "<read-only buffer for %lx, ptr %lx, size %d at %lx>", (long)(uintptr_t)&b->base,
		(long)(uintptr_t)Buffer_Ptr(b), b->size, (long)addr);
	Native_BufAdd(out, buf, -1);
	return 1;
}

/* the print statement writes its str: the bytes */
static void Buffer_Print(void *ctx, char *buf, int size) {
	const libBuffer_t *b = ctx;
	int n = b->size < size - 1 ? b->size : size - 1;
	memcpy(buf, Buffer_Ptr(b), (size_t)n);
	buf[n] = '\0';
}

static int Buffer_Len(void *ctx) {
	return ((libBuffer_t *)ctx)->size;
}

static byte Buffer_Item(void *ctx, long i, nativeVal_t *out) {
	const libBuffer_t *b = ctx;
	if (i < 0 || i >= b->size) {
		return Native_Raise(NE_INDEX, "buffer index out of range");
	}
	Native_SetText(out, Buffer_Ptr(b) + i, 1);
	return 1;
}

static byte Buffer_Slice(void *ctx, long left, long right, nativeVal_t *out);

/* buffer_compare of two buffers; another value by the names of the types */
static int Buffer_Compare(const nativeVal_t *a, const nativeVal_t *b) {
	const libBuffer_t *x = NObj_CObjOf(a, &lib_bufferType), *y = NObj_CObjOf(b, &lib_bufferType);
	if (!x || !y) {
		return Lib_Cmp(a, b);
	}
	int min = x->size < y->size ? x->size : y->size;
	if (min > 0) {
		int c = memcmp(Buffer_Ptr(x), Buffer_Ptr(y), (size_t)min);
		if (c != 0) {
			return c;
		}
	}
	return x->size < y->size ? -1 : x->size > y->size;
}

/* buffer_hash of a read-only buffer: the hash of its bytes as a text */
static byte Buffer_Hash(void *ctx, long *out) {
	const libBuffer_t *b = ctx;
	nativeVal_t t;
	Native_SetText(&t, Buffer_Ptr(b), b->size);
	byte ok = Native_Hash(&t, out);
	Native_Free(&t);
	return ok;
}

static PyObject *Buffer_ToPy(void *ctx) {
	const libBuffer_t *b = ctx;
	PyObject *base = Native_ToPy(&b->base), *r = base ? PyBuffer_FromObject(base, b->offset, b->size) : NULL;
	Py_XDECREF(base);
	return r;
}

const nativeCType_t lib_bufferType = {
	.name = "buffer",
	.print = Buffer_Print,
	.drop = Buffer_Drop,
	.repr = Buffer_Repr,
	.len = Buffer_Len,
	.item = Buffer_Item,
	.slice = Buffer_Slice,
	.compare = Buffer_Compare,
	.hash = Buffer_Hash,
	.pytype = &PyBuffer_Type,
	.toPy = Buffer_ToPy,
};

/* a buffer of len bytes of base from offset */
static void Buffer_New(const nativeVal_t *base, int offset, int size, nativeVal_t *out) {
	libBuffer_t *b = Mem_Alloc(sizeof(*b));
	Native_Copy(&b->base, base);
	b->offset = offset;
	b->size = size;
	NObj_NewCObj(out, &lib_bufferType, b);
}

/* buffer_slice: the buffer itself for the whole, else the text */
static byte Buffer_Slice(void *ctx, long left, long right, nativeVal_t *out) {
	const libBuffer_t *b = ctx;
	left = left < 0 ? 0 : (int)left;
	right = right < 0 ? 0 : right > b->size ? b->size : (int)right;
	if (left == 0 && right == b->size) {
		Buffer_New(&b->base, b->offset, b->size, out);
		return 1;
	}
	if (right < left) {
		right = left;
	}
	Native_SetText(out, Buffer_Ptr(b) + left, (int)(right - left));
	return 1;
}

byte Lib_BufferOf(const nativeVal_t *v, const char **p, int *len) {
	const libBuffer_t *b = NObj_CObjOf(v, &lib_bufferType);
	if (b) {
		*p = Buffer_Ptr(b);
		*len = b->size;
		return 1;
	}
	return 0;
}

byte Lib_NewBuffer(const nativeVal_t *obj, int offset, int size, nativeVal_t *out) {
	const libBuffer_t *inner = NObj_CObjOf(obj, &lib_bufferType);
	Native_SetNone(out);
	if (obj->type != NV_STR && !inner) {
		return Native_Raise(NE_TYPE, "buffer object expected");
	}
	if (offset < 0) {
		return Native_Raise(NE_VALUE, "offset must be zero or positive");
	}
	int count = inner ? inner->size : obj->n;
	if (size < 0) {
		size = count;
	}
	if (offset > count) {
		offset = count;
	}
	if (offset + size > count) {
		size = count - offset;
	}
	if (inner) {
		Buffer_New(&inner->base, inner->offset + offset, size, out);
	} else {
		Buffer_New(obj, offset, size, out);
	}
	return 1;
}

/* buffer + other (buffer_concat), buffer * n (buffer_repeat): 1, 0 with the error; -1 when v is no buffer */
int Lib_BufferOp(int op, const nativeVal_t *v, const nativeVal_t *w, nativeVal_t *out) {
	const libBuffer_t *b = NObj_CObjOf(v, &lib_bufferType);
	const char *p2;
	int count;
	if (!b) {
		return -1;
	}
	if (op == '*') {
		nativeBuf_t t;
		long n = w->i < 0 ? 0 : w->i;
		Native_BufInit(&t);
		while (n-- > 0) {
			Native_BufAdd(&t, Buffer_Ptr(b), b->size);
		}
		Native_SetText(out, t.p, t.len);
		Native_BufFree(&t);
		return 1;
	}
	if (w->type == NV_STR) {
		p2 = Native_Text(w);
		count = w->n;
	} else if (!Lib_BufferOf(w, &p2, &count)) {
		return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	}
	if (b->size == 0) {
		Native_Copy(out, w);
		return 1;
	}
	if (count == 0) {
		Native_Copy(out, v);
		return 1;
	}
	nativeBuf_t t;
	Native_BufInit(&t);
	Native_BufAdd(&t, Buffer_Ptr(b), b->size);
	Native_BufAdd(&t, p2, count);
	Native_SetText(out, t.p, t.len);
	Native_BufFree(&t);
	return 1;
}

/* ---- file ---- */

typedef struct {
	FILE *fp;
	byte close;			/* fclose at the close (not stdin, stdout, stderr) */
	int softspace;
	char name[0x108];
	char mode[0x10];
} libFile_t;

byte Lib_IOError(const char *name) {
	nativeVal_t v;
	const char *text = strerror(errno);
	int e = errno;
	Native_SetEmpty(&v, NV_TUPLE);
	Native_SetInt(Native_Append(&v), e);
	Native_SetText(Native_Append(&v), text, (int)strlen(text));
	if (name) {
		Native_SetText(Native_Append(&v), name, (int)strlen(name));
	}
	Native_RaiseVal(NE_IO, &v);
	Native_Free(&v);
	return 0;
}

/* PyErr_SetFromErrno(IOError) and clearerr of a failed stream call */
static byte File_Error(libFile_t *f) {
	Lib_IOError(NULL);
	clearerr(f->fp);
	return 0;
}

static byte File_Closed(void) {
	return Native_Raise(NE_VALUE, "I/O operation on closed file");
}

/* PyArg_NoArgs */
static byte File_NoArgs(int n) {
	return n == 0 || Native_Raise(NE_TYPE, "function requires no arguments");
}

static void File_Drop(void *ctx) {
	libFile_t *f = ctx;
	if (f->fp && f->close) {
		fclose(f->fp);
	}
	Mem_Free(f);
}

static byte File_Repr(void *ctx, uintptr_t addr, nativeBuf_t *b, nativeRepr_t mode) {
	const libFile_t *f = ctx;
	char buf[300];
	(void)mode;
	snprintf(buf, sizeof(buf), "<%s file '%.256s', mode '%.10s' at %lx>", f->fp ? "open" : "closed", f->name, f->mode,
		(long)addr);
	Native_BufAdd(b, buf, -1);
	return 1;
}

static byte File_GetAttr(void *ctx, const char *name, nativeVal_t *out) {
	const libFile_t *f = ctx;
	if (!strcmp(name, "closed")) {
		Native_SetInt(out, f->fp == NULL);
		return 1;
	}
	if (!strcmp(name, "__members__")) {
		static const char *const members[] = { "closed", "mode", "name", "softspace" };
		Native_SetEmpty(out, NV_LIST);
		for (int i = 0; i < 4; i++) {
			Native_SetStr(Native_Append(out), members[i]);
		}
		return 1;
	}
	if (!strcmp(name, "softspace")) {
		Native_SetInt(out, f->softspace);
		return 1;
	}
	if (!strcmp(name, "mode")) {
		Native_SetStr(out, f->mode);
		return 1;
	}
	if (!strcmp(name, "name")) {
		Native_SetStr(out, f->name);
		return 1;
	}
	return Native_Raise(NE_ATTRIBUTE, "%s", name);
}

/* file_setattr: PyMember_Set of softspace (T_INT), mode, name and closed read-only */
static byte File_SetAttr(void *ctx, const char *name, const nativeVal_t *v) {
	libFile_t *f = ctx;
	if (!v) {
		return Native_Raise(NE_ATTRIBUTE, "can't delete file attributes");
	}
	if (!strcmp(name, "mode") || !strcmp(name, "name") || !strcmp(name, "closed")) {
		return Native_Raise(NE_TYPE, "readonly attribute");
	}
	if (!strcmp(name, "softspace")) {
		if (v->type != NV_INT) {
			return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
		}
		f->softspace = (int)v->i;
		return 1;
	}
	return Native_Raise(NE_ATTRIBUTE, "%s", name);
}

static byte File_Close(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	int sts = 0;
	(void)a;
	if (!File_NoArgs(n)) {
		return 0;
	}
	if (f->fp) {
		if (f->close) {
			errno = 0;
			sts = fclose(f->fp);
		}
		f->fp = NULL;
	}
	if (sts == EOF) {
		return Lib_IOError(NULL);
	}
	if (sts != 0) {
		Native_SetInt(out, sts);
	}
	return 1;
}

/* an offset as file_seek and file_truncate take it (PyInt_AsLong) */
static byte File_Offset(const nativeVal_t *v, long *out) {
	nativeVal_t x;
	byte ok;
	if (v->type == NV_INT) {
		*out = v->i;
		return 1;
	}
	if (v->type == NV_LONG) {
		return Long_AsLong(v, out);
	}
	ok = v->type == NV_FLOAT ? Lib_Int(v, &x) : Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	if (ok) {
		*out = x.i;
		Native_Free(&x);
	}
	return ok;
}

static byte File_Seek(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	const nativeVal_t *off;
	int whence = 0;
	long offset;
	(void)out;
	if (!f->fp) {
		return File_Closed();
	}
	if (!Mode_Parse(a, n, "O|i", &off, &whence) || !File_Offset(off, &offset)) {
		return 0;
	}
	errno = 0;
	if (fseek(f->fp, offset, whence) != 0) {
		return File_Error(f);
	}
	return 1;
}

#ifdef _WIN32
/* file_truncate (HAVE_FTRUNCATE of the Windows build: _chsize) */
static byte File_Truncate(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	const nativeVal_t *size = NULL;
	long newsize;
	(void)out;
	if (!f->fp) {
		return File_Closed();
	}
	if (!Mode_Parse(a, n, "|O", &size)) {
		return 0;
	}
	if (size) {
		if (!File_Offset(size, &newsize)) {
			return 0;
		}
	} else {
		errno = 0;
		newsize = ftell(f->fp);
		if (newsize == -1) {
			return File_Error(f);
		}
	}
	errno = 0;
	if (fflush(f->fp) != 0 || _chsize(_fileno(f->fp), newsize) != 0) {
		return File_Error(f);
	}
	return 1;
}
#endif

static byte File_Tell(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	(void)a;
	if (!f->fp) {
		return File_Closed();
	}
	if (!File_NoArgs(n)) {
		return 0;
	}
	errno = 0;
	long offset = ftell(f->fp);
	if (offset == -1) {
		return File_Error(f);
	}
	Native_SetInt(out, offset);
	return 1;
}

static byte File_Fileno(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	(void)a;
	if (!f->fp) {
		return File_Closed();
	}
	if (!File_NoArgs(n)) {
		return 0;
	}
	Native_SetInt(out, fileno(f->fp));
	return 1;
}

static byte File_Flush(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	(void)a;
	(void)out;
	if (!f->fp) {
		return File_Closed();
	}
	if (!File_NoArgs(n)) {
		return 0;
	}
	errno = 0;
	if (fflush(f->fp) != 0) {
		return File_Error(f);
	}
	return 1;
}

static byte File_IsAtty(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	(void)a;
	if (!f->fp) {
		return File_Closed();
	}
	if (!File_NoArgs(n)) {
		return 0;
	}
	Native_SetInt(out, isatty(fileno(f->fp)));
	return 1;
}

static byte File_Read(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	long requested = -1;
	char chunk[8192];
	nativeBuf_t b;
	if (!f->fp) {
		return File_Closed();
	}
	if (!Mode_Parse(a, n, "|l", &requested)) {
		return 0;
	}
	Native_BufInit(&b);
	for (;;) {
		size_t want = requested < 0 || requested - b.len > (long)sizeof(chunk) ? sizeof(chunk) :
			(size_t)(requested - b.len);
		if (!want) {
			break;
		}
		errno = 0;
		size_t got = fread(chunk, 1, want, f->fp);
		if (got == 0) {
			if (!ferror(f->fp)) {
				break;
			}
			Native_BufFree(&b);
			return File_Error(f);
		}
		Native_BufAdd(&b, chunk, (int)got);
		if (got < want && requested >= 0) {
			break;
		}
	}
	Native_SetText(out, b.p ? b.p : "", b.len);
	Native_BufFree(&b);
	return 1;
}

/* getline of fileobject.c: a line with its newline, at most max bytes (0: any length) */
static void File_Line(libFile_t *f, int max, nativeVal_t *out) {
	nativeBuf_t b;
	int c;
	Native_BufInit(&b);
	for (;;) {
		if ((c = getc(f->fp)) == EOF) {
			clearerr(f->fp);
			break;
		}
		char ch = (char)c;
		Native_BufAdd(&b, &ch, 1);
		if (ch == '\n' || (max > 0 && b.len >= max)) {
			break;
		}
	}
	Native_SetText(out, b.p ? b.p : "", b.len);
	Native_BufFree(&b);
}

static byte File_ReadLine(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	int max = -1;
	if (!f->fp) {
		return File_Closed();
	}
	if (!Mode_Parse(a, n, "|i", &max)) {
		return 0;
	}
	if (max == 0) {
		Native_SetStr(out, "");
		return 1;
	}
	File_Line(f, max < 0 ? 0 : max, out);
	return 1;
}

static byte File_ReadLines(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	long sizehint = 0;
	char chunk[8192];
	nativeBuf_t part;
	size_t total = 0;
	if (!f->fp) {
		return File_Closed();
	}
	if (!Mode_Parse(a, n, "|l", &sizehint)) {
		return 0;
	}
	Native_SetEmpty(out, NV_LIST);
	Native_BufInit(&part);
	for (;;) {
		errno = 0;
		size_t got = fread(chunk, 1, sizeof(chunk), f->fp);
		if (got == 0) {
			sizehint = 0;
			if (!ferror(f->fp)) {
				break;
			}
			Native_BufFree(&part);
			Native_Free(out);
			return File_Error(f);
		}
		total += got;
		for (size_t i = 0; i < got; i++) {
			Native_BufAdd(&part, &chunk[i], 1);
			if (chunk[i] == '\n') {
				Native_SetText(Native_Append(out), part.p, part.len);
				Native_BufFree(&part);
				Native_BufInit(&part);
			}
		}
		if (sizehint > 0 && total >= (size_t)sizehint) {
			break;
		}
	}
	if (part.len) {
		if (sizehint > 0) {
			/* the last line completed */
			nativeVal_t rest;
			File_Line(f, 0, &rest);
			Native_BufAdd(&part, Native_Text(&rest), rest.n);
			Native_Free(&rest);
		}
		Native_SetText(Native_Append(out), part.p, part.len);
	}
	Native_BufFree(&part);
	return 1;
}

/* the old argument form of a method of one argument "s#" or "w#" (none: NULL; several: their tuple) */
static byte File_OldText(const nativeVal_t *a, int n, const char *what, const char **p, int *len) {
	if (n == 0) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	if (n == 1 && a[0].type == NV_STR) {
		*p = Native_Text(&a[0]);
		*len = a[0].n;
		return 1;
	}
	if (n == 1 && Lib_BufferOf(&a[0], p, len)) {
		return 1;
	}
	return Native_Raise(NE_TYPE, "%s, %s", what, n > 1 ? "tuple" : Native_TypeName(&a[0]));
}

static byte File_ReadInto(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	(void)out;
	if (!f->fp) {
		return File_Closed();
	}
	/* "w#": no value here is a read-write buffer */
	if (n == 0) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	return Native_Raise(NE_TYPE, "read-write buffer, %s", n > 1 ? "tuple" : Native_TypeName(&a[0]));
}

static byte File_Write(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	const char *s = NULL;
	int len = 0;
	(void)out;
	if (!f->fp) {
		return File_Closed();
	}
	if (!File_OldText(a, n, "read-only buffer", &s, &len)) {
		return 0;
	}
	f->softspace = 0;
	errno = 0;
	if (fwrite(s, 1, (size_t)len, f->fp) != (size_t)len) {
		return File_Error(f);
	}
	return 1;
}

static byte File_WriteLines(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	libFile_t *f = ctx;
	(void)out;
	if (!f->fp) {
		return File_Closed();
	}
	const nativeVal_t *list = n == 1 ? (a[0].type == NV_LIST ? &a[0] : NObj_ListData(&a[0])) : NULL;
	if (!list) {
		return Native_Raise(NE_TYPE, "writelines() requires list of strings");
	}
	f->softspace = 0;
	errno = 0;
	for (int i = 0; i < list->n; i++) {
		const nativeVal_t *line = &list->items[i];
		if (line->type != NV_STR) {
			return Native_Raise(NE_TYPE, "writelines() requires list of strings");
		}
		if (fwrite(Native_Text(line), 1, (size_t)line->n, f->fp) != (size_t)line->n) {
			return File_Error(f);
		}
	}
	return 1;
}

static const nativeCMethod_t file_methods[] = {
	{ "readline", File_ReadLine },
	{ "read", File_Read },
	{ "write", File_Write },
	{ "fileno", File_Fileno },
	{ "seek", File_Seek },
#ifdef _WIN32
	{ "truncate", File_Truncate },
#endif
	{ "tell", File_Tell },
	{ "readinto", File_ReadInto },
	{ "readlines", File_ReadLines },
	{ "writelines", File_WriteLines },
	{ "flush", File_Flush },
	{ "close", File_Close },
	{ "isatty", File_IsAtty },
	{ NULL, NULL },
};

/* the face shares the stream; the native object closes it */
static PyObject *File_ToPy(void *ctx) {
	libFile_t *f = ctx;
	return PyFile_FromFile(f->fp, f->name, f->mode, NULL);
}

const nativeCType_t lib_fileType = {
	.name = "file",
	.getattr = File_GetAttr,
	.setattr = File_SetAttr,
	.drop = File_Drop,
	.repr = File_Repr,
	.methods = file_methods,
	.pytype = &PyFile_Type,
	.toPy = File_ToPy,
};

static libFile_t *File_New(FILE *fp, const char *name, const char *mode, byte close, nativeVal_t *out) {
	libFile_t *f = Mem_Alloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	f->fp = fp;
	f->close = close;
	Str_Copy(f->name, name, sizeof(f->name));
	Str_Copy(f->mode, mode, sizeof(f->mode));
	NObj_NewCObj(out, &lib_fileType, f);
	return f;
}

byte Lib_Open(const char *name, const char *mode, int bufsize, nativeVal_t *out) {
	Native_SetNone(out);
	FILE *fp = fopen(name, mode);
	if (!fp) {
		return Lib_IOError(name);
	}
	File_New(fp, name, mode, 1, out);
	/* PyFile_SetBufSize */
	if (bufsize >= 0) {
		int type = bufsize == 0 ? _IONBF : bufsize == 1 ? _IOLBF : _IOFBF;
		setvbuf(fp, NULL, type, bufsize == 1 ? BUFSIZ : (size_t)bufsize);
	}
	return 1;
}

void Lib_StdFile(nativeVal_t *out, int which) {
	static const char *const names[] = { "<stdin>", "<stdout>", "<stderr>" };
	File_New(which == 0 ? stdin : which == 1 ? stdout : stderr, names[which], which ? "w" : "r", 0, out);
}
