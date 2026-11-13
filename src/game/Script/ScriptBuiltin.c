/*
 * The module __builtin__ of the Python 1.5.2 library in C (include/scrap/scriptlib.h, stage 7e, batch 7e4c):
 * the functions of Python/bltinmodule.c with their argument forms (PyArg_ParseTuple: the arity texts named after the
 * function, the conversion errors), results, docs and errors, and the dict of _PyBuiltin_Init_1 in its order: the
 * functions, __doc__, the names of the standard exceptions (the objects the error state knows them by, OBJLIB 7),
 * None, Ellipsis and __debug__. There is no interpreter: eval, compile, execfile and input raise the SystemError of
 * the build without one; __import__, globals, locals, vars, dir, reload and raw_input are the ones of the import
 * (ScriptImport.c).
 *
 * map, filter, reduce and apply call through a libCall_t (the test channel's evaluator passes its own call), getattr,
 * hasattr, setattr and delattr through a getter and setter (the channel's twins pass theirs).
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/scriptlib.h"
#include "scrap/scriptmodes.h"

/* the docs of the functions and of the module (the texts of bltinmodule.c) */
static const char bi_abs_doc[] = "abs(number) -> number\n\nReturn the absolute value of the argument.";
static const char bi_apply_doc[] =
	"apply(function, args[, kwargs]) -> value\n\n"
	"Call a function with positional arguments taken from the tuple args,\n"
	"and keyword arguments taken from the optional dictionary kwargs.";
static const char bi_buffer_doc[] =
	"buffer(object [, offset[, size]) -> object\n\n"
	"Creates a new buffer object which references the given object.\n"
	"The buffer will reference a slice of the target object from the\n"
	"start of the object (or at the specified offset). The slice will\n"
	"extend to the end of the target object (or with the specified size).";
static const char bi_callable_doc[] =
	"callable(object) -> Boolean\n\nReturn whether the object is callable (i.e., some kind of function).\n"
	"Note that classes are callable, as are instances with a __call__() method.";
static const char bi_chr_doc[] =
	"chr(i) -> character\n\nReturn a string of one character with ordinal i; 0 <= i < 256.";
static const char bi_cmp_doc[] = "cmp(x, y) -> integer\n\nReturn negative if x<y, zero if x==y, positive if x>y.";
static const char bi_coerce_doc[] =
	"coerce(x, y) -> None or (x1, y1)\n\nWhen x and y can be coerced to values of the same type, return a tuple\n"
	"containing the coerced values.  When they can't be coerced, return None.";
static const char bi_complex_doc[] =
	"complex(real[, imag]) -> complex number\n\n"
	"Create a complex number from a real part and an optional imaginary part.\n"
	"This is equivalent to (real + imag*1j) where imag defaults to 0.";
static const char bi_delattr_doc[] =
	"delattr(object, name)\n\nDelete a named attribute on an object; delattr(x, 'y') is equivalent to\n"
	"``del x.y''.";
static const char bi_divmod_doc[] =
	"divmod(x, y) -> (div, mod)\n\nReturn the tuple ((x-x%y)/y, x%y).  Invariant: div*y + mod == x.";
static const char bi_filter_doc[] =
	"filter(function, sequence) -> list\n\n"
	"Return a list containing those items of sequence for which function(item)\n"
	"is true.  If function is None, return a list of items that are true.";
static const char bi_float_doc[] =
	"float(x) -> floating point number\n\nConvert a string or number to a floating point number, if possible.";
static const char bi_getattr_doc[] =
	"getattr(object, name[, default]) -> value\n\n"
	"Get a named attribute from an object; getattr(x, 'y') is equivalent to x.y.\n"
	"When a default argument is given, it is returned when the attribute doesn't\n"
	"exist; without it, an exception is raised in that case.";
static const char bi_hasattr_doc[] =
	"hasattr(object, name) -> Boolean\n\nReturn whether the object has an attribute with the given name.\n"
	"(This is done by calling getattr(object, name) and catching exceptions.)";
static const char bi_hash_doc[] =
	"hash(object) -> integer\n\nReturn a hash value for the object.  Two objects with the same value have\n"
	"the same hash value.  The reverse is not necessarily true, but likely.";
static const char bi_hex_doc[] =
	"hex(number) -> string\n\nReturn the hexadecimal representation of an integer or long integer.";
static const char bi_id_doc[] =
	"id(object) -> integer\n\nReturn the identity of an object.  This is guaranteed to be unique among\n"
	"simultaneously existing objects.  (Hint: it's the object's memory address.)";
static const char bi_intern_doc[] =
	"intern(string) -> string\n\n``Intern'' the given string.  This enters the string in the (global)\n"
	"table of interned strings whose purpose is to speed up dictionary lookups.\n"
	"Return the string itself or the previously interned string object with the\nsame value.";
static const char bi_int_doc[] =
	"int(x) -> integer\n\nConvert a string or number to an integer, if possible.\n"
	"A floating point argument will be truncated towards zero.";
static const char bi_isinstance_doc[] =
	"isinstance(object, class-or-type) -> Boolean\n\n"
	"Return whether an object is an instance of a class or of a subclass thereof.\n"
	"With a type as second argument, return whether that is the object's type.";
static const char bi_issubclass_doc[] =
	"issubclass(C, B) -> Boolean\n\nReturn whether class C is a subclass (i.e., a derived class) of class B.";
static const char bi_len_doc[] = "len(object) -> integer\n\nReturn the number of items of a sequence or mapping.";
static const char bi_list_doc[] =
	"list(sequence) -> list\n\nReturn a new list whose items are the same as those of the argument sequence.";
static const char bi_long_doc[] =
	"long(x) -> long integer\n\nConvert a string or number to a long integer, if possible.\n"
	"A floating point argument will be truncated towards zero.";
static const char bi_map_doc[] =
	"map(function, sequence[, sequence, ...]) -> list\n\n"
	"Return a list of the results of applying the function to the items of\n"
	"the argument sequence(s).  If more than one sequence is given, the\n"
	"function is called with an argument list consisting of the corresponding\n"
	"item of each sequence, substituting None for missing values when not all\n"
	"sequences have the same length.  If the function is None, return a list of\n"
	"the items of the sequence (or a list of tuples if more than one sequence).";
static const char bi_max_doc[] =
	"max(sequence) -> value\nmax(a, b, c, ...) -> value\n\n"
	"With a single sequence argument, return its largest item.\n"
	"With two or more arguments, return the largest argument.";
static const char bi_min_doc[] =
	"min(sequence) -> value\nmin(a, b, c, ...) -> value\n\n"
	"With a single sequence argument, return its smallest item.\n"
	"With two or more arguments, return the smallest argument.";
static const char bi_oct_doc[] =
	"oct(number) -> string\n\nReturn the octal representation of an integer or long integer.";
static const char bi_open_doc[] =
	"open(filename[, mode[, buffering]]) -> file object\n\n"
	"Open a file.  The mode can be 'r', 'w' or 'a' for reading (default),\n"
	"writing or appending.  The file will be created if it doesn't exist\n"
	"when opened for writing or appending; it will be truncated when\n"
	"opened for writing.  Add a 'b' to the mode for binary files.\n"
	"Add a '+' to the mode to allow simultaneous reading and writing.\n"
	"If the buffering argument is given, 0 means unbuffered, 1 means line\n"
	"buffered, and larger numbers specify the buffer size.";
static const char bi_ord_doc[] = "ord(c) -> integer\n\nReturn the integer ordinal of a one character string.";
static const char bi_pow_doc[] =
	"pow(x, y[, z]) -> number\n\nWith two arguments, equivalent to x**y.  With three arguments,\n"
	"equivalent to (x**y) % z, but may be more efficient (e.g. for longs).";
static const char bi_range_doc[] =
	"range([start,] stop[, step]) -> list of integers\n\n"
	"Return a list containing an arithmetic progression of integers.\n"
	"range(i, j) returns [i, i+1, i+2, ..., j-1]; start (!) defaults to 0.\n"
	"When step is given, it specifies the increment (or decrement).\n"
	"For example, range(4) returns [0, 1, 2, 3].  The end point is omitted!\n"
	"These are exactly the valid indices for a list of 4 elements.";
static const char bi_reduce_doc[] =
	"reduce(function, sequence[, initial]) -> value\n\n"
	"Apply a function of two arguments cumulatively to the items of a sequence,\n"
	"from left to right, so as to reduce the sequence to a single value.\n"
	"For example, reduce(lambda x, y: x+y, [1, 2, 3, 4, 5]) calculates\n"
	"((((1+2)+3)+4)+5).  If initial is present, it is placed before the items\n"
	"of the sequence in the calculation, and serves as a default when the\nsequence is empty.";
static const char bi_repr_doc[] =
	"repr(object) -> string\n\nReturn the canonical string representation of the object.\n"
	"For most object types, eval(repr(object)) == object.";
static const char bi_round_doc[] =
	"round(number[, ndigits]) -> floating point number\n\n"
	"Round a number to a given precision in decimal digits (default 0 digits).\n"
	"This always returns a floating point number.  Precision may be negative.";
static const char bi_setattr_doc[] =
	"setattr(object, name, value)\n\nSet a named attribute on an object; setattr(x, 'y', v) is equivalent to\n"
	"``x.y = v''.";
static const char bi_slice_doc[] =
	"slice([start,] step[, stop]) -> slice object\n\n"
	"Create a slice object.  This is used for slicing by the Numeric extensions.";
static const char bi_str_doc[] =
	"str(object) -> string\n\nReturn a nice string representation of the object.\n"
	"If the argument is a string, the return value is the same object.";
static const char bi_tuple_doc[] =
	"tuple(sequence) -> list\n\nReturn a tuple whose items are the same as those of the argument sequence.\n"
	"If the argument is a tuple, the return value is the same object.";
static const char bi_type_doc[] = "type(object) -> type object\n\nReturn the type of the object.";
static const char bi_xrange_doc[] =
	"xrange([start,] stop[, step]) -> xrange object\n\n"
	"Like range(), but instead of returning a list, returns an object that\n"
	"generates the numbers in the range on demand.  This is slightly slower\n"
	"than range() but more memory efficient.";
static const char bi_builtin_doc[] =
	"Built-in functions, exceptions, and other objects.\n\n"
	"Noteworthy: None is the `nil' object; Ellipsis represents `...' in slices.";

/* the "NULL result without error in call_object" of a call that failed without an error: 0 */
static byte Bi_NullResult(void) {
	if (!Native_Failed()) {
		Native_Raise(NE_SYSTEM, "NULL result without error in call_object");
	}
	return 0;
}

/* ---- the calls of map, filter, reduce, apply ---- */

byte Lib_KwPairs(const nativeVal_t *kw, nativeVal_t *pairs) {
	nativeVal_t items;
	Native_SetEmpty(pairs, NV_LIST);
	if (kw->type == NV_DICT) {
		const nativeVal_t *k, *v;
		int pos = 0;
		while (Native_DictNext(kw, &pos, &k, &v)) {
			nativeVal_t *pair = Native_Append(pairs);
			Native_SetEmpty(pair, NV_TUPLE);
			Native_Copy(Native_Append(pair), k);
			Native_Copy(Native_Append(pair), v);
		}
		return 1;
	}
	if (!NObj_CallMethod(kw, "items", NULL, 0, &items)) {
		return 0;
	}
	byte ok = Lib_SeqList(&items, NV_LIST, pairs);
	Native_Free(&items);
	return ok;
}

byte Lib_IsDict(const nativeVal_t *v) {
	return v->type == NV_DICT || NObj_Kind(v) == NO_DICT;
}

byte Lib_HasKw(const nativeVal_t *kw) {
	return kw && Lib_IsDict(kw) && (kw->type == NV_DICT ? kw->n > 0 : NObj_Len(kw) > 0);
}

/* the call of the module's own functions: PyEval_CallObjectWithKeywords */
static byte Bi_Call(const nativeVal_t *f, nativeVal_t *a, int n, const nativeVal_t *kw, nativeVal_t *out) {
	nativeVal_t pairs;
	for (int i = 0; i < n; i++) {
		NObj_Share(&a[i]);
	}
	if (!Lib_HasKw(kw)) {
		return NObj_Call(f, a, n, out) || Bi_NullResult();
	}
	if (!Lib_KwPairs(kw, &pairs)) {
		return 0;
	}
	byte ok = NObj_CallKw(f, a, n, &pairs, out) || Bi_NullResult();
	Native_Free(&pairs);
	return ok;
}

byte Lib_Map(libCall_t call, nativeVal_t *a, int n, nativeVal_t *out) {
	if (n < 2) {
		return Native_Raise(NE_TYPE, "map() requires at least two args");
	}
	if (a[0].type == NV_NONE && n == 2) {
		/* map(None, S) is list(S) */
		return Lib_IsSeq(&a[1]) ? Lib_SeqList(&a[1], NV_LIST, out) : Native_Raise(NE_TYPE,
			"list() argument must be a sequence");
	}
	for (int i = 1; i < n; i++) {
		if (!Lib_IsSeq(&a[i])) {
			return Native_Raise(NE_TYPE, "argument %d to map() must be a sequence object", i + 1);
		}
	}
	Native_SetEmpty(out, NV_LIST);
	for (int i = 0;; i++) {
		nativeVal_t args, value;
		byte any = 0;
		Native_SetEmpty(&args, NV_TUPLE);
		for (int j = 1; j < n; j++) {
			int len = Lib_SeqLen(&a[j]);
			nativeVal_t *item = Native_Append(&args);
			if (len < 0 || (i < len && !Lib_SeqItem(&a[j], i, item))) {
				Native_Free(&args);
				Native_Free(out);
				return 0;
			}
			any |= i < len;
		}
		if (!any) {
			Native_Free(&args);
			return 1;
		}
		if (a[0].type == NV_NONE) {
			value = args;
		} else {
			byte ok = call(&a[0], args.items, args.n, NULL, &value);
			Native_Free(&args);
			if (!ok) {
				Native_Free(out);
				return 0;
			}
		}
		*Native_Append(out) = value;
	}
}

/* func(item) is true (func None: the item is): 1, 0, -1 with the error */
static int Bi_Good(libCall_t call, nativeVal_t *func, const nativeVal_t *item) {
	nativeVal_t arg, r;
	if (func->type == NV_NONE) {
		return Native_True(item);
	}
	Native_Copy(&arg, item);
	byte ok = call(func, &arg, 1, NULL, &r);
	Native_Free(&arg);
	int t = ok ? Native_True(&r) : -1;
	Native_Free(&r);
	return t;
}

byte Lib_Filter(libCall_t call, nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("filter", n, 2, 2)) {
		return 0;
	}
	nativeVal_t *seq = &a[1];
	if (seq->type == NV_STR) {
		/* filterstring: no character is false */
		nativeBuf_t b;
		if (a[0].type == NV_NONE) {
			Native_Copy(out, seq);
			return 1;
		}
		Native_BufInit(&b);
		for (int i = 0; i < seq->n; i++) {
			nativeVal_t c;
			Native_SetText(&c, Native_Text(seq) + i, 1);
			int t = Bi_Good(call, &a[0], &c);
			Native_Free(&c);
			if (t < 0) {
				Native_BufFree(&b);
				return 0;
			}
			if (t) {
				Native_BufAdd(&b, Native_Text(seq) + i, 1);
			}
		}
		Native_SetText(out, b.p, b.len);
		Native_BufFree(&b);
		return 1;
	}
	if (!Lib_IsSeq(seq)) {
		return Native_Raise(NE_TYPE, "argument 2 to filter() must be a sequence type");
	}
	/* filtertuple gives a tuple, the rest a list */
	byte tuple = seq->type == NV_TUPLE || seq->type == NV_VEC;
	Native_SetEmpty(out, tuple ? NV_TUPLE : NV_LIST);
	for (int i = 0;; i++) {
		nativeVal_t item;
		int len = Lib_SeqLen(seq);
		if (len < 0) {
			Native_Free(out);
			return 0;
		}
		if (i >= len) {
			return 1;
		}
		if (!Lib_SeqItem(seq, i, &item)) {
			Native_Free(out);
			return 0;
		}
		int t = Bi_Good(call, &a[0], &item);
		if (t > 0) {
			*Native_Append(out) = item;
		} else {
			Native_Free(&item);
			if (t < 0) {
				Native_Free(out);
				return 0;
			}
		}
	}
}

byte Lib_Reduce(libCall_t call, nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t r;
	byte has = n > 2;
	if (!Native_Arity("reduce", n, 2, 3)) {
		return 0;
	}
	if (!Lib_IsSeq(&a[1])) {
		return Native_Raise(NE_TYPE, "2nd argument to reduce() must be a sequence object");
	}
	Native_SetNone(&r);
	if (has) {
		Native_Copy(&r, &a[2]);
	}
	for (int i = 0;; i++) {
		/* the items until the IndexError, as builtin_reduce reads them */
		nativeVal_t args[2];
		if (!Lib_SeqItem(&a[1], i, &args[1])) {
			if (!Native_ErrIs(NE_INDEX)) {
				Native_Free(&r);
				return 0;
			}
			Native_ErrClear();
			break;
		}
		if (!has) {
			r = args[1];
			has = 1;
			continue;
		}
		args[0] = r;
		byte ok = call(&a[0], args, 2, NULL, &r);
		Native_Free(&args[0]);
		Native_Free(&args[1]);
		if (!ok) {
			return 0;
		}
	}
	if (!has) {
		return Native_Raise(NE_TYPE, "reduce of empty sequence with no initial value");
	}
	*out = r;
	return 1;
}

byte Lib_Apply(libCall_t call, nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t args;
	if (!Native_Arity("apply", n, 1, 3)) {
		return 0;
	}
	Native_SetEmpty(&args, NV_TUPLE);
	if (n > 1 && a[1].type != NV_TUPLE) {
		if (!Lib_IsSeq(&a[1])) {
			return Native_Raise(NE_TYPE, "apply() 2nd argument must be a sequence");
		}
		if (!Lib_SeqList(&a[1], NV_TUPLE, &args)) {
			return 0;
		}
	} else if (n > 1) {
		Native_Copy(&args, &a[1]);
	}
	if (n > 2 && !Lib_IsDict(&a[2])) {
		Native_Free(&args);
		return Native_Raise(NE_TYPE, "apply() 3rd argument must be dictionary");
	}
	byte ok = call(&a[0], args.items, args.n, n > 2 && Lib_HasKw(&a[2]) ? &a[2] : NULL, out);
	Native_Free(&args);
	return ok;
}

/* ---- attributes ---- */

byte Lib_GetAttrOf(libGetAttr_t get, const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("getattr", n, 2, 3) || !Native_ArgTextOf("getattr", 2, &a[1], 0)) {
		return 0;
	}
	if (get(&a[0], Native_Text(&a[1]), out)) {
		return 1;
	}
	if (n > 2) {
		Native_ErrClear();
		Native_Copy(out, &a[2]);
		return 1;
	}
	return Bi_NullResult();
}

byte Lib_HasAttrOf(libGetAttr_t get, const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t v;
	if (!Native_Arity("hasattr", n, 2, 2) || !Native_ArgTextOf("hasattr", 2, &a[1], 0)) {
		return 0;
	}
	byte ok = get(&a[0], Native_Text(&a[1]), &v);
	Native_Free(&v);
	if (!ok) {
		Native_ErrClear();
	}
	Native_SetInt(out, ok);
	return 1;
}

byte Lib_SetAttrOf(libSetAttr_t set, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	if (!Native_Arity("setattr", n, 3, 3) || !Native_ArgTextOf("setattr", 2, &a[1], 0)) {
		return 0;
	}
	return set(&a[0], Native_Text(&a[1]), &a[2]) || Bi_NullResult();
}

byte Lib_DelAttrOf(libSetAttr_t set, const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	if (!Native_Arity("delattr", n, 2, 2) || !Native_ArgTextOf("delattr", 2, &a[1], 0)) {
		return 0;
	}
	return set(&a[0], Native_Text(&a[1]), NULL) || Bi_NullResult();
}

/* the setter of the module: setattr (v given), delattr */
static byte Bi_Set(const nativeVal_t *o, const char *name, const nativeVal_t *v) {
	return v ? NObj_SetAttr(o, name, v) : NObj_DelAttr(o, name);
}

/* ---- sequences ---- */

/* min (sign -1) and max (1) */
byte Lib_MinMax(const nativeVal_t *a, int n, nativeVal_t *out, int sign) {
	nativeVal_t args, w;
	const nativeVal_t *seq = &args;
	byte has = 0;
	if (n > 1) {
		Native_SetNone(&args);
		args.type = NV_TUPLE;
		args.items = (nativeVal_t *)a;
		args.n = n;
	} else if (!Native_Arity("min/max", n, 1, 1)) {
		return 0;
	} else if (!Lib_IsSeq(a)) {
		return Native_Raise(NE_TYPE, "min() or max() of non-sequence");
	} else {
		seq = a;
	}
	Native_SetNone(&w);
	for (int i = 0;; i++) {
		/* the items until the IndexError, as min_max reads them */
		nativeVal_t x;
		if (n > 1 ? i >= n : 0) {
			break;
		}
		if (!(n > 1 ? (Native_Copy(&x, &a[i]), 1) : Lib_SeqItem(seq, i, &x))) {
			if (!Native_ErrIs(NE_INDEX)) {
				Native_Free(&w);
				return 0;
			}
			Native_ErrClear();
			break;
		}
		if (!has || Native_Compare(&x, &w) * sign > 0) {
			Native_Free(&w);
			w = x;
			has = 1;
		} else {
			Native_Free(&x);
		}
	}
	if (!has) {
		return Native_Raise(NE_VALUE, "min() or max() of empty sequence");
	}
	*out = w;
	return 1;
}

/* tuple(v) and list(v): PySequence_Tuple, PySequence_List */
byte Lib_Seq(const nativeVal_t *a, int n, nativeVal_t *out, nativeType_t type) {
	if (!Native_Arity(type == NV_TUPLE ? "tuple" : "list", n, 1, 1)) {
		return 0;
	}
	if (type == NV_TUPLE && a->type == NV_TUPLE) {
		Native_Copy(out, a);
		return 1;
	}
	if (!Lib_IsSeq(a)) {
		return Native_Raise(NE_TYPE, type == NV_TUPLE ? "tuple() argument must be a sequence" :
			"list() argument must be a sequence");
	}
	return Lib_SeqList(a, type, out);
}

/* get_len_of_range */
static unsigned long Bi_RangeLen(long lo, long hi, long step) {
	if (step > 0 && lo < hi) {
		return ((unsigned long)hi - (unsigned long)lo - 1) / (unsigned long)step + 1;
	}
	if (step < 0 && hi < lo) {
		return ((unsigned long)lo - (unsigned long)hi - 1) / (0ul - (unsigned long)step) + 1;
	}
	return 0;
}

/* the bounds of range and xrange ("l" or "ll|l" with the message of the function) */
static byte Bi_RangeArgs(const char *fname, const nativeVal_t *a, int n, long *lo, long *hi, long *step) {
	long *bound[3] = { hi, NULL, NULL };
	*lo = 0;
	*hi = 0;
	*step = 1;
	if (n < 1 || n > 3) {
		return Native_Raise(NE_TYPE, "%s() requires 1-3 int arguments", fname);
	}
	if (n > 1) {
		bound[0] = lo;
		bound[1] = hi;
		bound[2] = step;
	}
	for (int i = 0; i < n; i++) {
		if (!Lib_AsLong(&a[i], bound[i])) {
			return 0;
		}
	}
	if (*step == 0) {
		return Native_Raise(NE_VALUE, "zero step for %s()", fname);
	}
	return 1;
}

byte Lib_Range(const nativeVal_t *a, int n, nativeVal_t *out) {
	long lo, hi, step;
	if (!Bi_RangeArgs("range", a, n, &lo, &hi, &step)) {
		return 0;
	}
	unsigned long count = Bi_RangeLen(lo, hi, step);
	if (count > (unsigned long)INT_MAX) {
		return Native_Raise(NE_OVERFLOW, "range() has too many items");
	}
	Native_SetEmpty(out, NV_LIST);
	for (unsigned long i = 0; i < count; i++, lo += step) {
		Native_SetInt(Native_Append(out), lo);
	}
	return 1;
}

static byte Bi_XRange(const nativeVal_t *a, int n, nativeVal_t *out) {
	long lo, hi, step;
	if (!Bi_RangeArgs("xrange", a, n, &lo, &hi, &step)) {
		return 0;
	}
	unsigned long count = Bi_RangeLen(lo, hi, step);
	if (count > (unsigned long)LONG_MAX) {
		return Native_Raise(NE_OVERFLOW, "xrange() has more than sys.maxint items");
	}
	Lib_NewXRange(out, lo, (long)count, step, 1);
	return 1;
}

/* ---- the functions of the module ---- */

static byte Bi_Abs(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Native_Arity("abs", n, 1, 1) && Lib_Unary('a', a, out);
}

static byte Bi_Buffer(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *ob;
	int offset = 0, size = -1;
	return Mode_Parse(a, n, "O|ii:buffer", &ob, &offset, &size) && Lib_NewBuffer(ob, offset, size, out);
}

static byte Bi_Callable(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("callable", n, 1, 1)) {
		return 0;
	}
	Native_SetInt(out, NObj_Callable(a));
	return 1;
}

static byte Bi_Chr(const nativeVal_t *a, int n, nativeVal_t *out) {
	long x;
	if (!Mode_Parse(a, n, "l:chr", &x)) {
		return 0;
	}
	if (x < 0 || x >= 256) {
		return Native_Raise(NE_VALUE, "chr() arg not in range(256)");
	}
	char c = (char)x;
	Native_SetText(out, &c, 1);
	return 1;
}

static byte Bi_Cmp(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("cmp", n, 2, 2)) {
		return 0;
	}
	int c = Lib_Cmp(&a[0], &a[1]);
	if (Native_Failed()) {
		return 0;
	}
	Native_SetInt(out, c);
	return 1;
}

static byte Bi_Coerce(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t v, w;
	if (!Native_Arity("coerce", n, 2, 2)) {
		return 0;
	}
	Native_Copy(&v, &a[0]);
	Native_Copy(&w, &a[1]);
	if (!Lib_Coerce(&v, &w, 1)) {
		Native_Free(&v);
		Native_Free(&w);
		return 0;
	}
	Native_SetEmpty(out, NV_TUPLE);
	*Native_Append(out) = v;
	*Native_Append(out) = w;
	return 1;
}

/* eval, compile, execfile and input: no interpreter (before they look at their arguments) */
#define BI_NOINTERP(fn, name) \
	static byte fn(const nativeVal_t *a, int n, nativeVal_t *out) { \
		(void)a; \
		(void)n; \
		(void)out; \
		return Native_Raise(NE_SYSTEM, "%s: no Python interpreter in this build", name); \
	}
BI_NOINTERP(Bi_Compile, "compile")
BI_NOINTERP(Bi_Eval, "eval")
BI_NOINTERP(Bi_ExecFile, "execfile")
BI_NOINTERP(Bi_Input, "input")

/* complex_from_string */
static byte Bi_ComplexText(const nativeVal_t *v, nativeVal_t *out) {
	const char *start = Native_Text(v), *s = start;
	char *end;
	double x = 0.0, y = 0.0, z = -1.0;
	byte got_re = 0, got_im = 0, done = 0, error = 0;
	int sign = 1;
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	if (s[0] == '\0') {
		return Native_Raise(NE_VALUE, "empty string for complex()");
	}
	do {
		switch (*s) {
		case '\0':
			if (s - start != v->n) {
				return Native_Raise(NE_VALUE, "null byte in argument for complex()");
			}
			if (!done) {
				error = 1;
			}
			break;
		case '-':
			sign = -1;
			/* fall through */
		case '+':
			if (done) {
				error = 1;
			}
			s++;
			if (*s == '\0' || *s == '+' || *s == '-' || isspace((unsigned char)*s)) {
				error = 1;
			}
			break;
		case 'J':
		case 'j':
			if (got_im || done) {
				error = 1;
				break;
			}
			y = z < 0.0 ? sign : sign * z;
			got_im = 1;
			s++;
			if (*s != '+' && *s != '-') {
				done = 1;
			}
			break;
		default:
			if (isspace((unsigned char)*s)) {
				while (*s && isspace((unsigned char)*s)) {
					s++;
				}
				if (s[0] != '\0') {
					error = 1;
				} else {
					done = 1;
				}
				break;
			}
			if (done || !(*s == '.' || isdigit((unsigned char)*s))) {
				error = 1;
				break;
			}
			errno = 0;
			z = crt_strtod(s, &end);
			if (errno != 0) {
				return Native_Raise(NE_VALUE, "float() out of range: %.150s", s);
			}
			s = end;
			if (*s == 'J' || *s == 'j') {
				break;
			}
			if (got_re) {
				error = 1;
				break;
			}
			x = sign * z;
			got_re = 1;
			if (got_im) {
				done = 1;
			}
			z = -1.0;
			sign = 1;
			break;
		}
	} while (*s != '\0' && !error);
	if (error) {
		return Native_Raise(NE_VALUE, "malformed string for complex()");
	}
	Lib_NewComplex(out, x, y);
	return 1;
}

/* one argument of complex(): a complex as it is, another number by its float (nb_float) */
static byte Bi_ComplexPart(const nativeVal_t *v, double *re, double *im) {
	nativeVal_t f;
	if (Lib_ComplexOf(v, re, im)) {
		return 1;
	}
	if (!Lib_Float(v, &f)) {
		return 0;
	}
	*re = f.f;
	*im = 0.0;
	Native_Free(&f);
	return 1;
}

static byte Bi_Complex(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *r, *i = NULL;
	double rr, ri, ir = 0.0, ii = 0.0;
	if (!Mode_Parse(a, n, "O|O:complex", &r, &i)) {
		return 0;
	}
	if (r->type == NV_STR) {
		return Bi_ComplexText(r, out);
	}
	if (!Lib_IsNumber(r) || (i && !Lib_IsNumber(i))) {
		return Native_Raise(NE_TYPE, "complex() argument can't be converted to complex");
	}
	if (!Bi_ComplexPart(r, &rr, &ri) || (i && !Bi_ComplexPart(i, &ir, &ii))) {
		return 0;
	}
	Lib_NewComplex(out, rr - ii, ri + ir);
	return 1;
}

static byte Bi_DelAttr(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_DelAttrOf(Bi_Set, a, n, out);
}

static byte Bi_DivMod(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Native_Arity("divmod", n, 2, 2) && Lib_BinOp('d', &a[0], &a[1], out);
}

static byte Bi_Float(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Native_Arity("float", n, 1, 1) && Lib_Float(a, out);
}

static byte Bi_GetAttr(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_GetAttrOf(NObj_GetAttr, a, n, out);
}

static byte Bi_HasAttr(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_HasAttrOf(NObj_GetAttr, a, n, out);
}

static byte Bi_Hash(const nativeVal_t *a, int n, nativeVal_t *out) {
	long x;
	if (!Native_Arity("hash", n, 1, 1)) {
		return 0;
	}
	if (a->type == NV_ENT) {
		/* the address of the cPyEntity, the one its repr and id show (a dict keys a handle by its ref) */
		x = (long)Native_EntAddr(a);
		Native_SetInt(out, x == -1 ? -2 : x);
		return 1;
	}
	if (!NObj_Hash(a, &x)) {
		return 0;
	}
	Native_SetInt(out, x);
	return 1;
}

static byte Bi_Hex(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Native_Arity("hex", n, 1, 1) && Lib_HexOct(a, 16, out);
}

/* id(object): the address of the object (a value by value has none: the address of its copy, as a new object's) */
static byte Bi_Id(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("id", n, 1, 1)) {
		return 0;
	}
	uintptr_t x = a->type == NV_REF ? NObj_Addr(a) : a->type == NV_ENT ? Native_EntAddr(a) :
		a->type == NV_OBJ ? (uintptr_t)a->obj : (uintptr_t)a;
	Native_SetInt(out, (long)x);
	return 1;
}

static byte Bi_Intern(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *s;
	if (!Mode_Parse(a, n, "S", &s)) {
		return 0;
	}
	Native_Copy(out, s);
	return 1;
}

static byte Bi_Int(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Native_Arity("int", n, 1, 1) && Lib_Int(a, out);
}

/* PyClass_IsSubclass: cls is base or one of its bases is */
static byte Bi_IsSubclass(const nativeVal_t *cls, const nativeVal_t *base, int depth) {
	nativeVal_t bases;
	if (NObj_Is(cls, base)) {
		return 1;
	}
	if (NObj_Kind(cls) != NO_CLASS || depth > NATIVE_DEPTH || !NObj_GetAttr(cls, "__bases__", &bases)) {
		Native_ErrClear();
		return 0;
	}
	byte r = 0;
	for (int i = 0; !r && bases.type == NV_TUPLE && i < bases.n; i++) {
		r = Bi_IsSubclass(&bases.items[i], base, depth + 1);
	}
	Native_Free(&bases);
	return r;
}

static byte Bi_IsInstance(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t t;
	if (!Native_Arity(NULL, n, 2, 2)) {
		return 0;
	}
	if (NObj_Kind(&a[1]) == NO_TYPE) {
		NObj_TypeOf(&a[0], &t);
		Native_SetInt(out, NObj_Is(&t, &a[1]));
		Native_Free(&t);
		return 1;
	}
	if (NObj_Kind(&a[1]) != NO_CLASS) {
		return Native_Raise(NE_TYPE, "second argument must be a class");
	}
	byte r = 0;
	if (NObj_Kind(&a[0]) == NO_INSTANCE && NObj_GetAttr(&a[0], "__class__", &t)) {
		r = Bi_IsSubclass(&t, &a[1], 0);
		Native_Free(&t);
	}
	Native_ErrClear();
	Native_SetInt(out, r);
	return 1;
}

static byte Bi_IsSubclassFn(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity(NULL, n, 2, 2)) {
		return 0;
	}
	if (NObj_Kind(&a[0]) != NO_CLASS || NObj_Kind(&a[1]) != NO_CLASS) {
		return Native_Raise(NE_TYPE, "arguments must be classes");
	}
	Native_SetInt(out, Bi_IsSubclass(&a[0], &a[1], 0));
	return 1;
}

static byte Bi_Len(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("len", n, 1, 1)) {
		return 0;
	}
	int len = NObj_Len(a);
	if (len < 0) {
		return 0;
	}
	Native_SetInt(out, len);
	return 1;
}

static byte Bi_List(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_Seq(a, n, out, NV_LIST);
}

static byte Bi_Long(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Native_Arity("long", n, 1, 1) && Lib_Long(a, out);
}

/* the arguments of a builtin that calls as copies its call may take over */
static byte Bi_CallerOf(byte (*fn)(libCall_t, nativeVal_t *, int, nativeVal_t *), const nativeVal_t *a, int n,
	nativeVal_t *out) {
	nativeVal_t *c = n ? Mem_Alloc((uint32_t)((size_t)n * sizeof(*c))) : NULL;
	for (int k = 0; k < n; k++) {
		Native_Copy(&c[k], &a[k]);
	}
	byte ok = fn(Bi_Call, c, n, out);
	for (int k = 0; k < n; k++) {
		Native_Free(&c[k]);
	}
	Mem_Free(c);
	return ok;
}

static byte Bi_ApplyFn(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Bi_CallerOf(Lib_Apply, a, n, out);
}

static byte Bi_FilterFn(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Bi_CallerOf(Lib_Filter, a, n, out);
}

static byte Bi_MapFn(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Bi_CallerOf(Lib_Map, a, n, out);
}

static byte Bi_Max(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_MinMax(a, n, out, 1);
}

static byte Bi_Min(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_MinMax(a, n, out, -1);
}

static byte Bi_Oct(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Native_Arity("oct", n, 1, 1) && Lib_HexOct(a, 8, out);
}

static byte Bi_Open(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *name, *mode = "r";
	int bufsize = -1;
	return Mode_Parse(a, n, "s|si:open", &name, &mode, &bufsize) && Lib_Open(name, mode, bufsize, out);
}

static byte Bi_Ord(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("ord", n, 1, 1)) {
		return 0;
	}
	if (a->type != NV_STR || a->n != 1) {
		return Native_Raise(NE_TYPE, "ord, argument 1: expected char, %s found", Native_TypeName(a));
	}
	Native_SetInt(out, (unsigned char)Native_Text(a)[0]);
	return 1;
}

static byte Bi_Pow(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Native_Arity("pow", n, 2, 3) && Lib_Power(&a[0], &a[1], n > 2 ? &a[2] : NULL, out);
}

static byte Bi_Range(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_Range(a, n, out);
}

static byte Bi_ReduceFn(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Bi_CallerOf(Lib_Reduce, a, n, out);
}

/* repr and str as a text value */
static byte Bi_Text(const nativeVal_t *a, int n, nativeVal_t *out, const char *fname, nativeRepr_t mode) {
	nativeBuf_t b;
	if (!Native_Arity(fname, n, 1, 1)) {
		return 0;
	}
	Native_BufInit(&b);
	byte ok = NObj_Repr(&b, a, mode);
	if (ok) {
		Native_SetText(out, b.p, b.len);
	}
	Native_BufFree(&b);
	return ok;
}

static byte Bi_Repr(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Bi_Text(a, n, out, "repr", NR_REPR);
}

static byte Bi_Str(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Bi_Text(a, n, out, "str", NR_STR);
}

/* round(x, digits) of bltinmodule.c */
double Lib_Round(double x, int digits) {
	double f = 1.0;
	for (int i = digits < 0 ? -digits : digits; --i >= 0;) {
		f = f * 10.0;
	}
	x = digits < 0 ? x / f : x * f;
	x = x >= 0.0 ? floor(x + 0.5) : ceil(x - 0.5);
	return digits < 0 ? x * f : x / f;
}

static byte Bi_Round(const nativeVal_t *a, int n, nativeVal_t *out) {
	double x;
	int digits = 0;
	if (!Mode_Parse(a, n, "d|i:round", &x, &digits)) {
		return 0;
	}
	Native_SetFloat(out, Lib_Round(x, digits));
	return 1;
}

static byte Bi_SetAttr(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_SetAttrOf(Bi_Set, a, n, out);
}

static byte Bi_Slice(const nativeVal_t *a, int n, nativeVal_t *out) {
	const nativeVal_t *start = NULL, *stop = NULL, *step = NULL;
	if (!Mode_Parse(a, n, "O|OO:slice", &start, &stop, &step)) {
		return 0;
	}
	if (!stop) {
		stop = start;
		start = NULL;
	}
	Lib_NewSlice(out, start, stop, step);
	return 1;
}

static byte Bi_Tuple(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Lib_Seq(a, n, out, NV_TUPLE);
}

static byte Bi_Type(const nativeVal_t *a, int n, nativeVal_t *out) {
	if (!Native_Arity("type", n, 1, 1)) {
		return 0;
	}
	NObj_TypeOf(a, out);
	return 1;
}

static const libFn_t bi_fns[] = {
	{ "__import__", Bi_Import, NULL },
	{ "abs", Bi_Abs, bi_abs_doc },
	{ "apply", Bi_ApplyFn, bi_apply_doc },
	{ "buffer", Bi_Buffer, bi_buffer_doc },
	{ "callable", Bi_Callable, bi_callable_doc },
	{ "chr", Bi_Chr, bi_chr_doc },
	{ "cmp", Bi_Cmp, bi_cmp_doc },
	{ "coerce", Bi_Coerce, bi_coerce_doc },
	{ "compile", Bi_Compile, NULL },
	{ "complex", Bi_Complex, bi_complex_doc },
	{ "delattr", Bi_DelAttr, bi_delattr_doc },
	{ "dir", Bi_Dir, NULL },
	{ "divmod", Bi_DivMod, bi_divmod_doc },
	{ "eval", Bi_Eval, NULL },
	{ "execfile", Bi_ExecFile, NULL },
	{ "filter", Bi_FilterFn, bi_filter_doc },
	{ "float", Bi_Float, bi_float_doc },
	{ "getattr", Bi_GetAttr, bi_getattr_doc },
	{ "globals", Bi_Globals, NULL },
	{ "hasattr", Bi_HasAttr, bi_hasattr_doc },
	{ "hash", Bi_Hash, bi_hash_doc },
	{ "hex", Bi_Hex, bi_hex_doc },
	{ "id", Bi_Id, bi_id_doc },
	{ "input", Bi_Input, NULL },
	{ "intern", Bi_Intern, bi_intern_doc },
	{ "int", Bi_Int, bi_int_doc },
	{ "isinstance", Bi_IsInstance, bi_isinstance_doc },
	{ "issubclass", Bi_IsSubclassFn, bi_issubclass_doc },
	{ "len", Bi_Len, bi_len_doc },
	{ "list", Bi_List, bi_list_doc },
	{ "locals", Bi_Globals, NULL },
	{ "long", Bi_Long, bi_long_doc },
	{ "map", Bi_MapFn, bi_map_doc },
	{ "max", Bi_Max, bi_max_doc },
	{ "min", Bi_Min, bi_min_doc },
	{ "oct", Bi_Oct, bi_oct_doc },
	{ "open", Bi_Open, bi_open_doc },
	{ "ord", Bi_Ord, bi_ord_doc },
	{ "pow", Bi_Pow, bi_pow_doc },
	{ "range", Bi_Range, bi_range_doc },
	{ "raw_input", Bi_RawInput, NULL },
	{ "reduce", Bi_ReduceFn, bi_reduce_doc },
	{ "reload", Bi_Reload, NULL },
	{ "repr", Bi_Repr, bi_repr_doc },
	{ "round", Bi_Round, bi_round_doc },
	{ "setattr", Bi_SetAttr, bi_setattr_doc },
	{ "slice", Bi_Slice, bi_slice_doc },
	{ "str", Bi_Str, bi_str_doc },
	{ "tuple", Bi_Tuple, bi_tuple_doc },
	{ "type", Bi_Type, bi_type_doc },
	{ "vars", Bi_Vars, NULL },
	{ "xrange", Bi_XRange, bi_xrange_doc },
	{ NULL, NULL, NULL },
};

const libFn_t *Lib_Builtin(const char *name) {
	for (const libFn_t *row = bi_fns; row->name; row++) {
		if (!strcmp(row->name, name)) {
			return row;
		}
	}
	return NULL;
}

void Lib_MakeBuiltin(nativeVal_t *out) {
	/* the standard exceptions in the order of initerrors: the texts of the leaf ones, then the tuples */
	static const char *const excs[] = { "AssertionError", "AttributeError", "EOFError", "FloatingPointError", "IOError",
		"OSError", "ImportError", "IndexError", "KeyError", "KeyboardInterrupt", "MemoryError", "NameError",
		"OverflowError", "RuntimeError", "NotImplementedError", "SyntaxError", "SystemError", "SystemExit",
		"TypeError", "ValueError", "ZeroDivisionError", "LookupError", "ArithmeticError", "EnvironmentError",
		"StandardError", "Exception" };
	nativeVal_t v;
	Lib_Module(out, "__builtin__", bi_fns, bi_builtin_doc);
	for (size_t k = 0; k < sizeof(excs) / sizeof(excs[0]); k++) {
		Native_ErrObject(excs[k], &v);
		Lib_Put(out, excs[k], &v);
		Native_Free(&v);
	}
	Lib_Put(out, "None", &v);
	Lib_Ellipsis(&v);
	Lib_Put(out, "Ellipsis", &v);
	Native_Free(&v);
	Native_SetInt(&v, 1);
	Lib_Put(out, "__debug__", &v);
}
