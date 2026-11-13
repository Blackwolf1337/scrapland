/*
 * The built-in modules math, strop and signal of the Python 1.5.2 library in C (include/scrap/scriptlib.h, stage 7e,
 * batch 7e4c): Modules/mathmodule.c on the math of the Windows runtime as the library sees it (crtposix.c on Linux),
 * stropmodule.c and signalmodule.c, with their argument forms (the old PyArg_Parse form of the functions of flag 0:
 * no argument, the one argument, else the tuple of all), results, docs and error texts, and the order of their dicts.
 * The functions of strop and math the ports call directly (PORT_CALL(string, find, ...), the twins declared in
 * scrap/scriptmodes.h) are the entries of the module functions.
 *
 * The other modules of the library's table are gone: errno, new, regex, _locale and marshal had no user (no game code,
 * no case; new and marshal made code objects for the interpreter, regex compiled patterns no script used).
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/scriptlib.h"
#include "scrap/scriptmodes.h"

#ifndef _WIN32
/* src/standalone/crtposix.c: the libm and strtod of the Windows runtime, as the library sees them (pyshim.h) */
double crt_ms_pow(double x, double y);
double crt_ms_exp(double x);
double crt_ms_ldexp(double x, int e);
double crt_ms_sinh(double x);
double crt_ms_cosh(double x);
double crt_ms_atan2(double y, double x);
double crt_ms_hypot(double x, double y);
double crt_ms_fmod(double x, double y);
double crt_ms_sqrt(double x);
double crt_ms_asin(double x);
#define LIB_POW crt_ms_pow
#define LIB_EXP crt_ms_exp
#define LIB_LDEXP crt_ms_ldexp
#define LIB_SINH crt_ms_sinh
#define LIB_COSH crt_ms_cosh
#define LIB_ATAN2 crt_ms_atan2
#define LIB_HYPOT crt_ms_hypot
#define LIB_FMOD crt_ms_fmod
#define LIB_SQRT crt_ms_sqrt
#define LIB_ASIN crt_ms_asin
#else
#define LIB_POW pow
#define LIB_EXP exp
#define LIB_LDEXP ldexp
#define LIB_SINH sinh
#define LIB_COSH cosh
#define LIB_ATAN2 atan2
#define LIB_HYPOT hypot
#define LIB_FMOD fmod
#define LIB_SQRT sqrt
#define LIB_ASIN asin
#endif

/* the docs of the functions and modules (the texts of the vendored sources) */
static const char math_acos_doc[] = "acos(x)\n\nReturn the arc cosine of x.";
static const char math_asin_doc[] = "asin(x)\n\nReturn the arc sine of x.";
static const char math_atan_doc[] = "atan(x)\n\nReturn the arc tangent of x.";
static const char math_atan2_doc[] = "atan2(y, x)\n\nReturn atan(y/x).";
static const char math_ceil_doc[] = "ceil(x)\n\nReturn the ceiling of x as a real.";
static const char math_cos_doc[] = "cos(x)\n\nReturn the cosine of x.";
static const char math_cosh_doc[] = "cosh(x)\n\nReturn the hyperbolic cosine of x.";
static const char math_exp_doc[] = "exp(x)\n\nReturn e raised to the power of x.";
static const char math_fabs_doc[] = "fabs(x)\n\nReturn the absolute value of the real x.";
static const char math_floor_doc[] = "floor(x)\n\nReturn the floor of x as a real.";
static const char math_fmod_doc[] = "fmod(x,y)\n\nReturn x % y.";
static const char math_frexp_doc[] = "frexp(x)\n\nReturn the matissa and exponent for x. The mantissa is positive.";
static const char math_hypot_doc[] = "hypot(x,y)\n\nReturn the Euclidean distance, sqrt(x*x + y*y).";
static const char math_ldexp_doc[] = "ldexp_doc(x, i)\n\nReturn x * (2**i).";
static const char math_log_doc[] = "log(x)\n\nReturn the natural logarithm of x.";
static const char math_log10_doc[] = "log10(x)\n\nReturn the base-10 logarithm of x.";
static const char math_modf_doc[] =
	"modf(x)\n\nReturn the fractional and integer parts of x. Both results carry the sign\n"
	"of x.  The integer part is returned as a real.";
static const char math_pow_doc[] = "pow(x,y)\n\nReturn x**y.";
static const char math_sin_doc[] = "sin(x)\n\nReturn the sine of x.";
static const char math_sinh_doc[] = "sinh(x)\n\nReturn the hyperbolic sine of x.";
static const char math_sqrt_doc[] = "sqrt(x)\n\nReturn the square root of x.";
static const char math_tan_doc[] = "tan(x)\n\nReturn the tangent of x.";
static const char math_tanh_doc[] = "tanh(x)\n\nReturn the hyperbolic tangent of x.";
static const char math_doc[] =
	"This module is always available.  It provides access to the\n"
	"mathematical functions defined by the C standard.";
static const char strop_doc[] =
	"Common string manipulations, optimized for speed.\n\nAlways use \"import string\" rather than referencing\n"
	"this module directly.";
static const char strop_atof_doc[] =
	"atof(s) -> float\n\nReturn the floating point number represented by the string s.";
static const char strop_atoi_doc[] =
	"atoi(s [,base]) -> int\n\nReturn the integer represented by the string s in the given\n"
	"base, which defaults to 10.  The string s must consist of one\n"
	"or more digits, possibly preceded by a sign.  If base is 0, it\n"
	"is chosen from the leading characters of s, 0 for octal, 0x or\n"
	"0X for hexadecimal.  If base is 16, a preceding 0x or 0X is\naccepted.";
static const char strop_atol_doc[] =
	"atol(s [,base]) -> long\n\nReturn the long integer represented by the string s in the\n"
	"given base, which defaults to 10.  The string s must consist\n"
	"of one or more digits, possibly preceded by a sign.  If base\n"
	"is 0, it is chosen from the leading characters of s, 0 for\n"
	"octal, 0x or 0X for hexadecimal.  If base is 16, a preceding\n"
	"0x or 0X is accepted.  A trailing L or l is not accepted,\nunless base is 0.";
static const char strop_capitalize_doc[] =
	"capitalize(s) -> string\n\nReturn a copy of the string s with only its first character\ncapitalized.";
static const char strop_count_doc[] =
	"count(s, sub[, start[, end]]) -> int\n\nReturn the number of occurrences of substring sub in string\n"
	"s[start:end].  Optional arguments start and end are\ninterpreted as in slice notation.";
static const char strop_expandtabs_doc[] =
	"expandtabs(string, [tabsize]) -> string\n\n"
	"Expand tabs in a string, i.e. replace them by one or more spaces,\n"
	"depending on the current column and the given tab size (default 8).\n"
	"The column number is reset to zero after each newline occurring in the\n"
	"string.  This doesn't understand other non-printing characters.";
static const char strop_find_doc[] =
	"find(s, sub [,start [,end]]) -> in\n\nReturn the lowest index in s where substring sub is found,\n"
	"such that sub is contained within s[start,end].  Optional\n"
	"arguments start and end are interpreted as in slice notation.\n\nReturn -1 on failure.";
static const char strop_joinfields_doc[] =
	"join(list [,sep]) -> string\njoinfields(list [,sep]) -> string\n\n"
	"Return a string composed of the words in list, with\n"
	"intervening occurences of sep.  Sep defaults to a single\nspace.\n\n(join and joinfields are synonymous)";
static const char strop_lstrip_doc[] =
	"lstrip(s) -> string\n\nReturn a copy of the string s with leading whitespace removed.";
static const char strop_lower_doc[] = "lower(s) -> string\n\nReturn a copy of the string s converted to lowercase.";
static const char strop_maketrans_doc[] =
	"maketrans(frm, to) -> string\n\nReturn a translation table (a string of 256 bytes long)\n"
	"suitable for use in string.translate.  The strings frm and to\nmust be of the same length.";
static const char strop_replace_doc[] =
	"replace (str, old, new[, maxsplit]) -> string\n\n"
	"Return a copy of string str with all occurrences of substring\n"
	"old replaced by new. If the optional argument maxsplit is\n"
	"given, only the first maxsplit occurrences are replaced.";
static const char strop_rfind_doc[] =
	"rfind(s, sub [,start [,end]]) -> int\n\nReturn the highest index in s where substring sub is found,\n"
	"such that sub is contained within s[start,end].  Optional\n"
	"arguments start and end are interpreted as in slice notation.\n\nReturn -1 on failure.";
static const char strop_rstrip_doc[] =
	"rstrip(s) -> string\n\nReturn a copy of the string s with trailing whitespace removed.";
static const char strop_splitfields_doc[] =
	"split(str [,sep [,maxsplit]]) -> list of strings\nsplitfields(str [,sep [,maxsplit]]) -> list of strings\n\n"
	"Return a list of the words in the string s, using sep as the\n"
	"delimiter string.  If maxsplit is nonzero, splits into at most\n"
	"maxsplit words If sep is not specified, any whitespace string\nis a separator.  Maxsplit defaults to 0.\n\n"
	"(split and splitfields are synonymous)";
static const char strop_strip_doc[] =
	"strip(s) -> string\n\nReturn a copy of the string s with leading and trailing\nwhitespace removed.";
static const char strop_swapcase_doc[] =
	"swapcase(s) -> string\n\nReturn a copy of the string s with upper case characters\n"
	"converted to lowercase and vice versa.";
static const char strop_translate_doc[] =
	"translate(s,table [,deletechars]) -> string\n\n"
	"Return a copy of the string s, where all characters occurring\n"
	"in the optional argument deletechars are removed, and the\n"
	"remaining characters have been mapped through the given\n"
	"translation table, which must be a string of length 256.";
static const char strop_upper_doc[] = "upper(s) -> string\n\nReturn a copy of the string s converted to uppercase.";
static const char signal_doc[] =
	"This module provides mechanisms to use signal handlers in Python.\n\nFunctions:\n\n"
	"alarm() -- cause SIGALRM after a specified time [Unix only]\nsignal() -- set the action for a given signal\n"
	"getsignal() -- get the signal action for a given signal\n"
	"pause() -- wait until a signal arrives [Unix only]\ndefault_int_handler() -- default SIGINT handler\n\n"
	"Constants:\n\nSIG_DFL -- used to refer to the system default handler\nSIG_IGN -- used to ignore the signal\n"
	"NSIG -- number of defined signals\n\nSIGINT, SIGTERM, etc. -- signal numbers\n\n*** IMPORTANT NOTICE ***\n"
	"A signal handler function is called with two arguments:\n"
	"the first is the signal number, the second is the interrupted stack frame.";
static const char signal_signal_doc[] =
	"signal(sig, action) -> action\n\nSet the action for the given signal.  The action can be SIG_DFL,\n"
	"SIG_IGN, or a callable Python object.  The previous action is\n"
	"returned.  See getsignal() for possible return values.\n\n*** IMPORTANT NOTICE ***\n"
	"A signal handler function is called with two arguments:\n"
	"the first is the signal number, the second is the interrupted stack frame.";
static const char signal_getsignal_doc[] =
	"getsignal(sig) -> action\n\nReturn the current action for the given signal.  The return value can be:\n"
	"SIG_IGN -- if the signal is being ignored\nSIG_DFL -- if the default action for the signal is in effect\n"
	"None -- if an unknown handler is in effect\nanything else -- the callable Python object used as a handler\n";
static const char signal_default_doc[] =
	"default_int_handler(...)\n\nThe default handler for SIGINT instated by Python.\nIt raises KeyboardInterrupt.";

/* ---- arguments ---- */

/* "t#" (chars 1) and "s#" (chars 0) of getargs.c: the bytes of a text or buffer, any bytes */
static byte Lib_Bytes(const nativeVal_t *v, int arg, byte chars, const char **s, int *len) {
	if (v->type == NV_STR) {
		*s = Native_Text(v);
		*len = v->n;
		return 1;
	}
	if (Lib_BufferOf(v, s, len)) {
		return 1;
	}
	return Native_Raise(NE_TYPE, "argument %d: expected read-only %sbuffer, %s found", arg, chars ? "character " : "",
		Native_TypeName(v));
}

/* "t#" of the old argument form (PyArg_Parse): none, the argument, or the tuple of several */
static byte Lib_OldBytes(const nativeVal_t *a, int n, const char **s, int *len) {
	if (n == 0) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	if (n == 1 && (a[0].type == NV_STR || Lib_BufferOf(&a[0], s, len))) {
		if (a[0].type == NV_STR) {
			*s = Native_Text(&a[0]);
			*len = a[0].n;
		}
		return 1;
	}
	return Native_Raise(NE_TYPE, "read-only character buffer, %s", n > 1 ? "tuple" : Native_TypeName(&a[0]));
}

/* the optional ints start and end of find, rfind and count ("|ii") */
static byte Lib_Bounds(const nativeVal_t *a, int n, int first, int len, int *i, int *last) {
	*i = 0;
	*last = INT_MAX;
	if ((n > first && !Mode_ArgInt(&a[first], i)) || (n > first + 1 && !Mode_ArgInt(&a[first + 1], last))) {
		return 0;
	}
	if (*last > len) {
		*last = len;
	}
	if (*last < 0) {
		*last += len;
	}
	if (*last < 0) {
		*last = 0;
	}
	if (*i < 0) {
		*i += len;
	}
	if (*i < 0) {
		*i = 0;
	}
	return 1;
}

/* ---- strop ---- */

/* find(s, sub [,start [,end]]) */
static byte Strop_Find(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s, *sub;
	int len, m, i, last;
	if (!Mode_Arity(n, 2, 4) || !Lib_Bytes(&a[0], 1, 1, &s, &len) || !Lib_Bytes(&a[1], 2, 1, &sub, &m) ||
		!Lib_Bounds(a, n, 2, len, &i, &last)) {
		return 0;
	}
	if (m == 0 && i <= last) {
		Native_SetInt(out, i);
		return 1;
	}
	for (last -= m; i <= last; ++i) {
		if (s[i] == sub[0] && (m == 1 || !memcmp(&s[i + 1], &sub[1], (size_t)(m - 1)))) {
			Native_SetInt(out, i);
			return 1;
		}
	}
	Native_SetInt(out, -1);
	return 1;
}

/* rfind(s, sub [,start [,end]]) */
static byte Strop_RFind(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s, *sub;
	int len, m, i, last;
	if (!Mode_Arity(n, 2, 4) || !Lib_Bytes(&a[0], 1, 1, &s, &len) || !Lib_Bytes(&a[1], 2, 1, &sub, &m) ||
		!Lib_Bounds(a, n, 2, len, &i, &last)) {
		return 0;
	}
	if (m == 0 && i <= last) {
		Native_SetInt(out, last);
		return 1;
	}
	for (int j = last - m; j >= i; --j) {
		if (s[j] == sub[0] && (m == 1 || !memcmp(&s[j + 1], &sub[1], (size_t)(m - 1)))) {
			Native_SetInt(out, j);
			return 1;
		}
	}
	Native_SetInt(out, -1);
	return 1;
}

/* count(s, sub[, start[, end]]) */
static byte Strop_Count(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s, *sub;
	int len, m, i, last, r = 0;
	if (!Mode_Arity(n, 2, 4) || !Lib_Bytes(&a[0], 1, 1, &s, &len) || !Lib_Bytes(&a[1], 2, 1, &sub, &m) ||
		!Lib_Bounds(a, n, 2, len, &i, &last)) {
		return 0;
	}
	int end = last + 1 - m;
	if (m == 0) {
		Native_SetInt(out, end - i);
		return 1;
	}
	while (i < end) {
		if (!memcmp(s + i, sub, (size_t)m)) {
			r++;
			i += m;
		} else {
			i++;
		}
	}
	Native_SetInt(out, r);
	return 1;
}

/* do_strip: 1 the left, 2 the right, 3 both */
static byte Strop_Strip(const nativeVal_t *a, int n, nativeVal_t *out, int sides) {
	const char *s;
	int len, i = 0, j;
	if (!Lib_OldBytes(a, n, &s, &len)) {
		return 0;
	}
	if (sides & 1) {
		while (i < len && isspace((unsigned char)s[i])) {
			i++;
		}
	}
	j = len;
	if (sides & 2) {
		do {
			j--;
		} while (j >= i && isspace((unsigned char)s[j]));
		j++;
	}
	Native_SetText(out, s + i, j - i);
	return 1;
}

static byte Strop_LStrip(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Strop_Strip(a, n, out, 1);
}

static byte Strop_RStrip(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Strop_Strip(a, n, out, 2);
}

static byte Strop_StripBoth(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Strop_Strip(a, n, out, 3);
}

/* lower, upper, swapcase and capitalize: the case of each byte (how: 'l', 'u', 's', 'c') */
static byte Strop_Case(const nativeVal_t *a, int n, nativeVal_t *out, char how) {
	const char *s;
	int len;
	if (!Lib_OldBytes(a, n, &s, &len)) {
		return 0;
	}
	Native_SetText(out, s, len);
	char *o = out->text ? out->text : out->s;
	for (int i = 0; i < len; i++) {
		int c = (unsigned char)o[i];
		char k = how == 'c' ? (i == 0 ? 'u' : 'l') : how;
		if ((k == 'u' || k == 's') && islower(c)) {
			o[i] = (char)toupper(c);
		} else if ((k == 'l' || k == 's') && isupper(c)) {
			o[i] = (char)tolower(c);
		}
	}
	return 1;
}

static byte Strop_Lower(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Strop_Case(a, n, out, 'l');
}

static byte Strop_Upper(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Strop_Case(a, n, out, 'u');
}

static byte Strop_SwapCase(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Strop_Case(a, n, out, 's');
}

static byte Strop_Capitalize(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Strop_Case(a, n, out, 'c');
}

/* expandtabs(string, [tabsize]) */
static byte Strop_ExpandTabs(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s;
	int len, tabsize = 8, col = 0;
	nativeBuf_t b;
	if (!Mode_Arity(n, 1, 2) || !Lib_Bytes(&a[0], 1, 0, &s, &len) || (n > 1 && !Mode_ArgInt(&a[1], &tabsize))) {
		return 0;
	}
	if (tabsize < 1) {
		return Native_Raise(NE_VALUE, "tabsize must be at least 1");
	}
	Native_BufInit(&b);
	for (int i = 0; i < len; i++) {
		if (s[i] == '\t') {
			for (int j = tabsize - (col % tabsize); j > 0; j--, col++) {
				Native_BufAdd(&b, " ", 1);
			}
		} else {
			Native_BufAdd(&b, &s[i], 1);
			col = s[i] == '\n' ? 0 : col + 1;
		}
	}
	Native_SetText(out, b.p ? b.p : "", b.len);
	Native_BufFree(&b);
	return 1;
}

/* join(list [,sep]) and joinfields */
static byte Strop_Join(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *sep = " ";
	int seplen = 1;
	nativeBuf_t b;
	if (!Mode_Arity(n, 1, 2) || (n > 1 && !Lib_Bytes(&a[1], 2, 1, &sep, &seplen))) {
		return 0;
	}
	int len = NObj_Len(&a[0]);
	if (len < 0) {
		return 0;
	}
	if (len == 1) {
		/* the one item itself */
		if (!Lib_SeqItem(&a[0], 0, out)) {
			return 0;
		}
		if (out->type != NV_STR) {
			Native_Free(out);
			return Native_Raise(NE_TYPE, "first argument must be sequence of strings");
		}
		return 1;
	}
	if (!Lib_IsSeq(&a[0]) && NObj_ListData(&a[0]) == NULL) {
		return Native_Raise(NE_TYPE, "first argument must be a sequence");
	}
	Native_BufInit(&b);
	for (int i = 0; i < len; i++) {
		nativeVal_t item;
		if (!Lib_SeqItem(&a[0], i, &item) || item.type != NV_STR) {
			Native_Free(&item);
			Native_BufFree(&b);
			Native_ErrClear();
			return Native_Raise(NE_TYPE, "first argument must be sequence of strings");
		}
		if (i > 0) {
			Native_BufAdd(&b, sep, seplen);
		}
		Native_BufAdd(&b, Native_Text(&item), item.n);
		Native_Free(&item);
	}
	Native_SetText(out, b.p ? b.p : "", b.len);
	Native_BufFree(&b);
	return 1;
}

/* mymemfind */
static int Lib_MemFind(const char *mem, int len, const char *pat, int plen) {
	len -= plen;
	for (int i = 0; i <= len; i++) {
		if (mem[i] == pat[0] && (plen == 1 || !memcmp(&mem[i + 1], &pat[1], (size_t)(plen - 1)))) {
			return i;
		}
	}
	return -1;
}

/* replace (str, old, new[, maxsplit]): mymemreplace (maxsplit 0 or below: all) */
static byte Strop_Replace(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *str, *pat, *sub;
	int len, plen, slen, count = 0;
	if (!Mode_Arity(n, 3, 4) || !Lib_Bytes(&a[0], 1, 1, &str, &len) || !Lib_Bytes(&a[1], 2, 1, &pat, &plen) ||
		!Lib_Bytes(&a[2], 3, 1, &sub, &slen) || (n > 3 && !Mode_ArgInt(&a[3], &count))) {
		return 0;
	}
	if (plen <= 0) {
		return Native_Raise(NE_VALUE, "empty pattern string");
	}
	/* mymemcnt: the number of places (none for an empty text or a longer pattern) */
	int found = 0;
	if (len && plen <= len) {
		for (int rest = len, at = 0, off; rest >= 0 && (off = Lib_MemFind(str + at, rest, pat, plen)) != -1; found++) {
			at += off + plen;
			rest -= off + plen;
		}
	}
	if (count > 0 && found > count) {
		found = count;
	}
	if (!found) {
		Native_SetText(out, str, len);
		return 1;
	}
	nativeBuf_t b;
	Native_BufInit(&b);
	while (len > 0) {
		int off = Lib_MemFind(str, len, pat, plen);
		if (off == -1) {
			break;
		}
		Native_BufAdd(&b, str, off);
		Native_BufAdd(&b, sub, slen);
		str += off + plen;
		len -= off + plen;
		if (--count == 0) {
			break;
		}
	}
	if (len > 0) {
		Native_BufAdd(&b, str, len);
	}
	Native_SetText(out, b.p ? b.p : "", b.len);
	Native_BufFree(&b);
	return 1;
}

/* atoi(s [,base]): PyOS_strtol (PyOS_strtoul for base 0 and a leading 0) on the text after its blanks */
static byte Strop_Atoi(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s;
	char *end;
	int base = 10;
	if (!Mode_Parse(a, n, "s|i", &s, &base)) {
		return 0;
	}
	if ((base != 0 && base < 2) || base > 36) {
		return Native_Raise(NE_VALUE, "invalid base for atoi()");
	}
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	errno = 0;
	long x = base == 0 && s[0] == '0' ? (long)PyOS_strtoul((char *)s, &end, base) : PyOS_strtol((char *)s, &end, base);
	byte bad = end == s || !isalnum((unsigned char)end[-1]);
	while (!bad && *end && isspace((unsigned char)*end)) {
		end++;
	}
	if (bad || *end) {
		return Native_Raise(NE_VALUE, "invalid literal for atoi(): %.200s", s);
	}
	if (errno) {
		return Native_Raise(NE_VALUE, "atoi() literal too large: %.200s", s);
	}
	Native_SetInt(out, x);
	return 1;
}

/* atol(s [,base]): PyLong_FromString on the text after its blanks, an L for base 0 */
static byte Strop_Atol(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s, *end;
	int base = 10;
	if (!Mode_Parse(a, n, "s|i", &s, &base)) {
		return 0;
	}
	if ((base != 0 && base < 2) || base > 36) {
		return Native_Raise(NE_VALUE, "invalid base for atol()");
	}
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	if (s[0] == '\0') {
		return Native_Raise(NE_VALUE, "empty string for atol()");
	}
	if (!Long_FromText(s, base, out, &end)) {
		return 0;
	}
	if (base == 0 && (*end == 'l' || *end == 'L')) {
		end++;
	}
	while (*end && isspace((unsigned char)*end)) {
		end++;
	}
	if (*end != '\0') {
		Native_Free(out);
		return Native_Raise(NE_VALUE, "invalid literal for atol(): %.200s", s);
	}
	return 1;
}

/* atof(s): strtod of the Windows runtime (crt_strtod) on the text after its blanks */
static byte Strop_Atof(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s;
	char *end;
	if (!Mode_Parse(a, n, "s", &s)) {
		return 0;
	}
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	if (!*s) {
		return Native_Raise(NE_VALUE, "empty string for atof()");
	}
	errno = 0;
	double x = crt_strtod(s, &end);
	while (*end && isspace((unsigned char)*end)) {
		end++;
	}
	if (*end) {
		return Native_Raise(NE_VALUE, "invalid literal for atof(): %.200s", s);
	}
	if (errno) {
		return Native_Raise(NE_VALUE, "atof() literal too large: %.200s", s);
	}
	Native_SetFloat(out, x);
	return 1;
}

/* split(str [,sep [,maxsplit]]) and splitfields ("t#|z#i") */
static byte Strop_Split(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *s, *sep = NULL;
	int len, m = 0, maxsplit = 0;
	if (!Mode_Arity(n, 1, 3) || !Lib_Bytes(&a[0], 1, 1, &s, &len)) {
		return 0;
	}
	if (n > 1 && a[1].type != NV_NONE) {
		if (a[1].type == NV_STR) {
			sep = Native_Text(&a[1]);
			m = a[1].n;
		} else if (!Lib_BufferOf(&a[1], &sep, &m)) {
			return Native_Raise(NE_TYPE, "argument 2: expected read-only buffer, %s found", Native_TypeName(&a[1]));
		}
	}
	if (n > 2 && !Mode_ArgInt(&a[2], &maxsplit)) {
		return 0;
	}
	return Native_SplitText(s, len, sep, m, maxsplit, out);
}

/* maketrans(frm, to) */
static byte Strop_MakeTrans(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *from, *to;
	int fromlen, tolen;
	unsigned char c[256];
	if (!Mode_Arity(n, 2, 2) || !Lib_Bytes(&a[0], 1, 1, &from, &fromlen) || !Lib_Bytes(&a[1], 2, 1, &to, &tolen)) {
		return 0;
	}
	if (fromlen != tolen) {
		return Native_Raise(NE_VALUE, "maketrans arguments must have same length");
	}
	for (int i = 0; i < 256; i++) {
		c[i] = (unsigned char)i;
	}
	for (int i = 0; i < fromlen; i++) {
		c[(unsigned char)from[i]] = (unsigned char)to[i];
	}
	Native_SetText(out, (const char *)c, 256);
	return 1;
}

/* translate(s,table [,deletechars]) ("St#|t#") */
static byte Strop_Translate(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *table, *del = NULL;
	int tablen, dellen = 0, trans[256];
	if (!Mode_Arity(n, 2, 3)) {
		return 0;
	}
	if (a[0].type != NV_STR) {
		return Native_Raise(NE_TYPE, "argument 1: expected string, %s found", Native_TypeName(&a[0]));
	}
	if (!Lib_Bytes(&a[1], 2, 1, &table, &tablen) || (n > 2 && !Lib_Bytes(&a[2], 3, 1, &del, &dellen))) {
		return 0;
	}
	if (tablen != 256) {
		return Native_Raise(NE_VALUE, "translation table must be 256 characters long");
	}
	for (int i = 0; i < 256; i++) {
		trans[i] = (unsigned char)table[i];
	}
	for (int i = 0; i < dellen; i++) {
		trans[(unsigned char)del[i]] = -1;
	}
	nativeBuf_t b;
	Native_BufInit(&b);
	const char *s = Native_Text(&a[0]);
	for (int i = 0; i < a[0].n; i++) {
		int c = trans[(unsigned char)s[i]];
		if (c != -1) {
			char ch = (char)c;
			Native_BufAdd(&b, &ch, 1);
		}
	}
	Native_SetText(out, b.p ? b.p : "", b.len);
	Native_BufFree(&b);
	return 1;
}

static const libFn_t strop_fns[] = {
	{ "atof", Strop_Atof, strop_atof_doc },
	{ "atoi", Strop_Atoi, strop_atoi_doc },
	{ "atol", Strop_Atol, strop_atol_doc },
	{ "capitalize", Strop_Capitalize, strop_capitalize_doc },
	{ "count", Strop_Count, strop_count_doc },
	{ "expandtabs", Strop_ExpandTabs, strop_expandtabs_doc },
	{ "find", Strop_Find, strop_find_doc },
	{ "join", Strop_Join, strop_joinfields_doc },
	{ "joinfields", Strop_Join, strop_joinfields_doc },
	{ "lstrip", Strop_LStrip, strop_lstrip_doc },
	{ "lower", Strop_Lower, strop_lower_doc },
	{ "maketrans", Strop_MakeTrans, strop_maketrans_doc },
	{ "replace", Strop_Replace, strop_replace_doc },
	{ "rfind", Strop_RFind, strop_rfind_doc },
	{ "rstrip", Strop_RStrip, strop_rstrip_doc },
	{ "split", Strop_Split, strop_splitfields_doc },
	{ "splitfields", Strop_Split, strop_splitfields_doc },
	{ "strip", Strop_StripBoth, strop_strip_doc },
	{ "swapcase", Strop_SwapCase, strop_swapcase_doc },
	{ "translate", Strop_Translate, strop_translate_doc },
	{ "upper", Strop_Upper, strop_upper_doc },
	{ NULL, NULL, NULL },
};

void Lib_MakeStrop(nativeVal_t *out) {
	nativeVal_t s;
	char buf[256];
	static int (*const classes[])(int) = { isspace, islower, isupper };
	static const char *const names[] = { "whitespace", "lowercase", "uppercase" };
	Lib_Module(out, "strop", strop_fns, strop_doc);
	for (int k = 0; k < 3; k++) {
		int n = 0;
		for (int c = 0; c < 256; c++) {
			if (classes[k](c)) {
				buf[n++] = (char)c;
			}
		}
		Native_SetText(&s, buf, n);
		Lib_Put(out, names[k], &s);
		Native_Free(&s);
	}
}

/* ---- math ---- */

/* the result of a function of math: CHECK and math_error of mathmodule.c (a NaN is a range error) */
static byte Math_Out(double x, nativeVal_t *out) {
	if (errno == 0 && !(-HUGE_VAL <= x && x <= HUGE_VAL)) {
		errno = ERANGE;
	}
	if (errno == EDOM) {
		return Native_Raise(NE_VALUE, "math domain error");
	}
	if (errno == ERANGE) {
		return Native_Raise(NE_OVERFLOW, "math range error");
	}
	if (errno) {
		/* the (errno, text) value of PyErr_SetFromErrno */
		nativeVal_t v;
		const char *text = strerror(errno);
		Native_SetEmpty(&v, NV_TUPLE);
		Native_SetInt(Native_Append(&v), errno);
		Native_SetText(Native_Append(&v), text, (int)strlen(text));
		Native_RaiseVal(NE_VALUE, &v);
		Native_Free(&v);
		return 0;
	}
	Native_SetFloat(out, x);
	return 1;
}

/* PyArg_Parse(args, "d") of the old form: no argument, the one, else the tuple (which PyFloat_AsDouble refuses) */
static byte Math_Arg1(const nativeVal_t *a, int n, double *x) {
	nativeVal_t t;
	if (!n) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	Native_SetEmpty(&t, NV_TUPLE);
	byte ok = Native_ArgDouble(n == 1 ? &a[0] : &t, x);
	Native_Free(&t);
	return ok;
}

/* PyArg_Parse(args, "(dd)"): the arguments (or the one value) as a sequence of two numbers */
static byte Math_Arg2(const nativeVal_t *a, int n, double x[2]) {
	nativeVal_t t, tmp;
	if (!n) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	/* the arguments as the tuple of old arguments (a view of them, not freed) */
	Native_SetNone(&t);
	t.type = NV_TUPLE;
	t.items = (nativeVal_t *)a;
	t.n = n;
	const nativeVal_t *seq = n == 1 ? &a[0] : &t;
	int len = seq->type == NV_STR || seq->type == NV_LIST || seq->type == NV_TUPLE ? seq->n : seq->type == NV_VEC ? 3 : -1;
	if (len < 0) {
		return Native_Raise(NE_TYPE, "2-sequence, %s", Native_TypeName(seq));
	}
	if (len != 2) {
		return Native_Raise(NE_TYPE, "2-sequence, %d-sequence", len);
	}
	for (int i = 0; i < 2; i++) {
		Native_SetNone(&tmp);
		const nativeVal_t *item = Native_GetItem(seq, i, 0, &tmp);
		byte ok = item && Native_ArgDouble(item, &x[i]);
		Native_Free(&tmp);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* math_1 */
static byte Math_1(const nativeVal_t *a, int n, nativeVal_t *out, double (*fn)(double)) {
	double x;
	if (!Math_Arg1(a, n, &x)) {
		return 0;
	}
	errno = 0;
	return Math_Out(fn(x), out);
}

/* math_2 */
static byte Math_2(const nativeVal_t *a, int n, nativeVal_t *out, double (*fn)(double, double)) {
	double x[2];
	if (!Math_Arg2(a, n, x)) {
		return 0;
	}
	errno = 0;
	return Math_Out(fn(x[0], x[1]), out);
}

static double Math_Fabs(double x) {
	return fabs(x);
}

static double Math_Sqrt(double x) {
	return LIB_SQRT(x);
}

static double Math_Asin(double x) {
	return LIB_ASIN(x);
}

static double Math_Exp(double x) {
	return LIB_EXP(x);
}

static double Math_Sinh(double x) {
	return LIB_SINH(x);
}

static double Math_Cosh(double x) {
	return LIB_COSH(x);
}

static double Math_Fmod(double x, double y) {
	return LIB_FMOD(x, y);
}

static double Math_Atan2(double y, double x) {
	return LIB_ATAN2(y, x);
}

static double Math_Hypot(double x, double y) {
	return LIB_HYPOT(x, y);
}

static double Math_Pow(double x, double y) {
	return LIB_POW(x, y);
}

#define MATH_FUNC1(name, fn) \
	static byte Math_##name(const nativeVal_t *a, int n, nativeVal_t *out) { \
		return Math_1(a, n, out, fn); \
	}
#define MATH_FUNC2(name, fn) \
	static byte Math_##name(const nativeVal_t *a, int n, nativeVal_t *out) { \
		return Math_2(a, n, out, fn); \
	}
MATH_FUNC1(AcosF, acos)
MATH_FUNC1(AsinF, Math_Asin)
MATH_FUNC1(AtanF, atan)
MATH_FUNC2(Atan2F, Math_Atan2)
MATH_FUNC1(CeilF, ceil)
MATH_FUNC1(CosF, cos)
MATH_FUNC1(CoshF, Math_Cosh)
MATH_FUNC1(ExpF, Math_Exp)
MATH_FUNC1(FabsF, Math_Fabs)
MATH_FUNC1(FloorF, floor)
MATH_FUNC2(FmodF, Math_Fmod)
MATH_FUNC2(HypotF, Math_Hypot)
MATH_FUNC1(LogF, log)
MATH_FUNC1(Log10F, log10)
MATH_FUNC2(PowF, Math_Pow)
MATH_FUNC1(SinF, sin)
MATH_FUNC1(SinhF, Math_Sinh)
MATH_FUNC1(SqrtF, Math_Sqrt)
MATH_FUNC1(TanF, tan)
MATH_FUNC1(TanhF, tanh)

/* the pair (x, y) as a tuple */
static void Math_Pair(nativeVal_t *x, nativeVal_t *y, nativeVal_t *out) {
	Native_SetEmpty(out, NV_TUPLE);
	*Native_Append(out) = *x;
	*Native_Append(out) = *y;
}

static byte Math_Frexp(const nativeVal_t *a, int n, nativeVal_t *out) {
	double x;
	int i;
	nativeVal_t m, e;
	if (!Math_Arg1(a, n, &x)) {
		return 0;
	}
	errno = 0;
	x = frexp(x, &i);
	if (!Math_Out(x, &m)) {
		return 0;
	}
	Native_SetInt(&e, i);
	Math_Pair(&m, &e, out);
	return 1;
}

static byte Math_Ldexp(const nativeVal_t *a, int n, nativeVal_t *out) {
	double x[2];
	if (!Math_Arg2(a, n, x)) {
		return 0;
	}
	errno = 0;
	return Math_Out(LIB_LDEXP(x[0], (int)x[1]), out);
}

static byte Math_Modf(const nativeVal_t *a, int n, nativeVal_t *out) {
	double x, y;
	nativeVal_t f, i;
	if (!Math_Arg1(a, n, &x)) {
		return 0;
	}
	errno = 0;
	x = modf(x, &y);
	if (!Math_Out(x, &f)) {
		return 0;
	}
	Native_SetFloat(&i, y);
	Math_Pair(&f, &i, out);
	return 1;
}

static const libFn_t math_fns[] = {
	{ "acos", Math_AcosF, math_acos_doc },
	{ "asin", Math_AsinF, math_asin_doc },
	{ "atan", Math_AtanF, math_atan_doc },
	{ "atan2", Math_Atan2F, math_atan2_doc },
	{ "ceil", Math_CeilF, math_ceil_doc },
	{ "cos", Math_CosF, math_cos_doc },
	{ "cosh", Math_CoshF, math_cosh_doc },
	{ "exp", Math_ExpF, math_exp_doc },
	{ "fabs", Math_FabsF, math_fabs_doc },
	{ "floor", Math_FloorF, math_floor_doc },
	{ "fmod", Math_FmodF, math_fmod_doc },
	{ "frexp", Math_Frexp, math_frexp_doc },
	{ "hypot", Math_HypotF, math_hypot_doc },
	{ "ldexp", Math_Ldexp, math_ldexp_doc },
	{ "log", Math_LogF, math_log_doc },
	{ "log10", Math_Log10F, math_log10_doc },
	{ "modf", Math_Modf, math_modf_doc },
	{ "pow", Math_PowF, math_pow_doc },
	{ "sin", Math_SinF, math_sin_doc },
	{ "sinh", Math_SinhF, math_sinh_doc },
	{ "sqrt", Math_SqrtF, math_sqrt_doc },
	{ "tan", Math_TanF, math_tan_doc },
	{ "tanh", Math_TanhF, math_tanh_doc },
	{ NULL, NULL, NULL },
};

void Lib_MakeMath(nativeVal_t *out) {
	nativeVal_t v;
	Lib_Module(out, "math", math_fns, math_doc);
	Native_SetFloat(&v, atan(1.0) * 4.0);
	Lib_Put(out, "pi", &v);
	Native_SetFloat(&v, LIB_EXP(1.0));
	Lib_Put(out, "e", &v);
}

/* the twins of the ports (declared in scrap/scriptmodes.h, no seam): the entries of strop and math */
portRef_t port_string_find = { .module = "string", .name = "find", .entry = Strop_Find, .twin = 1 };
portRef_t port_string_replace = { .module = "string", .name = "replace", .entry = Strop_Replace, .twin = 1 };
portRef_t port_string_atoi = { .module = "string", .name = "atoi", .entry = Strop_Atoi, .twin = 1 };
portRef_t port_string_atof = { .module = "string", .name = "atof", .entry = Strop_Atof, .twin = 1 };
portRef_t port_string_split = { .module = "string", .name = "split", .entry = Strop_Split, .twin = 1 };
portRef_t port_string_splitfields = { .module = "string", .name = "splitfields", .entry = Strop_Split, .twin = 1 };
portRef_t port_math_sqrt = { .module = "math", .name = "sqrt", .entry = Math_SqrtF, .twin = 1 };
portRef_t port_math_fabs = { .module = "math", .name = "fabs", .entry = Math_FabsF, .twin = 1 };
portRef_t port_math_fmod = { .module = "math", .name = "fmod", .entry = Math_FmodF, .twin = 1 };

/* ---- signal ---- */

#ifndef NSIG
#define NSIG 23
#endif

/* the action of each signal as the module has it (SIG_DFL and SIG_IGN as the ints 0 and 1, a callable, None for a
 * handler of the game's) */
static nativeVal_t signal_handlers[NSIG];

/* signal_handler: no Python code runs to call the action, the signal is only taken */
static void Signal_Taken(int sig) {
	(void)signal(sig, Signal_Taken);
}

/* the int of the old argument form "i" */
static byte Signal_Num(const nativeVal_t *a, int n, int *sig) {
	nativeVal_t t;
	if (!n) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	Native_SetEmpty(&t, NV_TUPLE);
	byte ok = Mode_ArgInt(n == 1 ? &a[0] : &t, sig);
	Native_Free(&t);
	return ok && ((*sig >= 1 && *sig < NSIG) || Native_Raise(NE_VALUE, "signal number out of range"));
}

/* signal(sig, action) -> action ("(iO)" of the old form) */
static byte Signal_Signal(const nativeVal_t *a, int n, nativeVal_t *out) {
	int sig;
	if (!n) {
		return Native_Raise(NE_TYPE, "function requires at least one argument");
	}
	if (n != 2) {
		return n == 1 ? Native_Raise(NE_TYPE, "2-sequence, %s", Native_TypeName(&a[0])) :
			Native_Raise(NE_TYPE, "2-sequence, %d-sequence", n);
	}
	if (!Mode_ArgInt(&a[0], &sig)) {
		return 0;
	}
	if (sig < 1 || sig >= NSIG) {
		return Native_Raise(NE_VALUE, "signal number out of range");
	}
	void (*func)(int);
	if (a[1].type == NV_INT && a[1].i == (long)(intptr_t)SIG_IGN) {
		func = SIG_IGN;
	} else if (a[1].type == NV_INT && a[1].i == (long)(intptr_t)SIG_DFL) {
		func = SIG_DFL;
	} else if (!NObj_Callable(&a[1])) {
		return Native_Raise(NE_TYPE, "signal handler must be signal.SIG_IGN, signal.SIG_DFL, or a callable object");
	} else {
		func = Signal_Taken;
	}
	if (signal(sig, func) == SIG_ERR) {
		/* PyErr_SetFromErrno(RuntimeError) */
		nativeVal_t v;
		const char *text = strerror(errno);
		Native_SetEmpty(&v, NV_TUPLE);
		Native_SetInt(Native_Append(&v), errno);
		Native_SetText(Native_Append(&v), text, (int)strlen(text));
		Native_RaiseVal(NE_RUNTIME, &v);
		Native_Free(&v);
		return 0;
	}
	*out = signal_handlers[sig];
	Native_Copy(&signal_handlers[sig], &a[1]);
	NObj_Share(&signal_handlers[sig]);
	return 1;
}

static byte Signal_GetSignal(const nativeVal_t *a, int n, nativeVal_t *out) {
	int sig;
	if (!Signal_Num(a, n, &sig)) {
		return 0;
	}
	Native_Copy(out, &signal_handlers[sig]);
	return 1;
}

static byte Signal_Default(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	return Native_RaiseVal(NE_INTERRUPT, NULL);
}

static const libFn_t signal_fns[] = {
	{ "signal", Signal_Signal, signal_signal_doc },
	{ "getsignal", Signal_GetSignal, signal_getsignal_doc },
	{ "default_int_handler", Signal_Default, signal_default_doc },
	{ NULL, NULL, NULL },
};

void Lib_MakeSignal(nativeVal_t *out) {
	static const struct {
		const char *name;
		int sig;
	} sigs[] = {
#ifdef SIGHUP
		{ "SIGHUP", SIGHUP },
#endif
#ifdef SIGINT
		{ "SIGINT", SIGINT },
#endif
#ifdef SIGQUIT
		{ "SIGQUIT", SIGQUIT },
#endif
#ifdef SIGILL
		{ "SIGILL", SIGILL },
#endif
#ifdef SIGTRAP
		{ "SIGTRAP", SIGTRAP },
#endif
#ifdef SIGIOT
		{ "SIGIOT", SIGIOT },
#endif
#ifdef SIGABRT
		{ "SIGABRT", SIGABRT },
#endif
#ifdef SIGEMT
		{ "SIGEMT", SIGEMT },
#endif
#ifdef SIGFPE
		{ "SIGFPE", SIGFPE },
#endif
#ifdef SIGKILL
		{ "SIGKILL", SIGKILL },
#endif
#ifdef SIGBUS
		{ "SIGBUS", SIGBUS },
#endif
#ifdef SIGSEGV
		{ "SIGSEGV", SIGSEGV },
#endif
#ifdef SIGSYS
		{ "SIGSYS", SIGSYS },
#endif
#ifdef SIGPIPE
		{ "SIGPIPE", SIGPIPE },
#endif
#ifdef SIGALRM
		{ "SIGALRM", SIGALRM },
#endif
#ifdef SIGTERM
		{ "SIGTERM", SIGTERM },
#endif
#ifdef SIGUSR1
		{ "SIGUSR1", SIGUSR1 },
#endif
#ifdef SIGUSR2
		{ "SIGUSR2", SIGUSR2 },
#endif
#ifdef SIGCLD
		{ "SIGCLD", SIGCLD },
#endif
#ifdef SIGCHLD
		{ "SIGCHLD", SIGCHLD },
#endif
#ifdef SIGPWR
		{ "SIGPWR", SIGPWR },
#endif
#ifdef SIGIO
		{ "SIGIO", SIGIO },
#endif
#ifdef SIGURG
		{ "SIGURG", SIGURG },
#endif
#ifdef SIGWINCH
		{ "SIGWINCH", SIGWINCH },
#endif
#ifdef SIGPOLL
		{ "SIGPOLL", SIGPOLL },
#endif
#ifdef SIGSTOP
		{ "SIGSTOP", SIGSTOP },
#endif
#ifdef SIGTSTP
		{ "SIGTSTP", SIGTSTP },
#endif
#ifdef SIGCONT
		{ "SIGCONT", SIGCONT },
#endif
#ifdef SIGTTIN
		{ "SIGTTIN", SIGTTIN },
#endif
#ifdef SIGTTOU
		{ "SIGTTOU", SIGTTOU },
#endif
#ifdef SIGVTALRM
		{ "SIGVTALRM", SIGVTALRM },
#endif
#ifdef SIGPROF
		{ "SIGPROF", SIGPROF },
#endif
#ifdef SIGXCPU
		{ "SIGXCPU", SIGXCPU },
#endif
#ifdef SIGXFSZ
		{ "SIGXFSZ", SIGXFSZ },
#endif
	};
	nativeVal_t v, intHandler, key;
	Lib_Module(out, "signal", signal_fns, signal_doc);
	Native_SetInt(&v, (long)(intptr_t)SIG_DFL);
	Lib_Put(out, "SIG_DFL", &v);
	Native_SetInt(&v, (long)(intptr_t)SIG_IGN);
	Lib_Put(out, "SIG_IGN", &v);
	Native_SetInt(&v, NSIG);
	Lib_Put(out, "NSIG", &v);
	Native_SetStr(&key, "default_int_handler");
	Native_Copy(&intHandler, Native_DictGet(NObj_DictData(NObj_ModuleVars(out)), &key));
	Native_Free(&key);
	/* the actions there are (initsignal without sigaction: each one swapped for SIG_IGN and back) */
	for (int i = 1; i < NSIG; i++) {
		void (*t)(int) = signal(i, SIG_IGN);
		signal(i, t);
		Native_Free(&signal_handlers[i]);
		if (t == SIG_DFL) {
			Native_SetInt(&signal_handlers[i], (long)(intptr_t)SIG_DFL);
		} else if (t == SIG_IGN) {
			Native_SetInt(&signal_handlers[i], (long)(intptr_t)SIG_IGN);
		}
	}
	if (signal_handlers[SIGINT].type == NV_INT && signal_handlers[SIGINT].i == (long)(intptr_t)SIG_DFL) {
		Native_Free(&signal_handlers[SIGINT]);
		Native_Copy(&signal_handlers[SIGINT], &intHandler);
		signal(SIGINT, Signal_Taken);
	}
	Native_Free(&intHandler);
	for (size_t k = 0; k < sizeof(sigs) / sizeof(sigs[0]); k++) {
		Native_SetInt(&v, sigs[k].sig);
		Lib_Put(out, sigs[k].name, &v);
	}
}

/* ---- the modules ---- */

/* the call of a row of a table: its entry */
static byte Lib_RowCall(void *ctx, const nativeVal_t *a, int n, nativeVal_t *out) {
	return ((const libFn_t *)ctx)->fn(a, n, out);
}

const libFn_t *Lib_FnOf(const nativeVal_t *f) {
	nativeCall_t call;
	void *ctx;
	return NObj_CallbackOf(f, &call, &ctx) && call == Lib_RowCall ? ctx : NULL;
}

void Lib_Put(const nativeVal_t *m, const char *name, const nativeVal_t *v) {
	nativeVal_t key, x;
	Native_SetStr(&key, name);
	Native_Copy(&x, v);
	NObj_Share(&x);
	Native_DictSet(NObj_DictData(NObj_ModuleVars(m)), &key, &x);
}

void Lib_Module(nativeVal_t *out, const char *name, const libFn_t *table, const char *doc) {
	nativeVal_t f, d;
	/* PyModule_New (__name__, __doc__ None), the functions, then __doc__ (Py_InitModule4) */
	NObj_NewModule(out, name);
	for (const libFn_t *row = table; row->name; row++) {
		/* the rows of one C function on the first one's: equal function objects (meth_compare) */
		const libFn_t *first = table;
		while (first->fn != row->fn) {
			first++;
		}
		NObj_NewCallback(&f, row->name, Lib_RowCall, (void *)first, NULL, 0);
		NObj_SetCallbackDoc(&f, row->doc);
		Lib_Put(out, row->name, &f);
		Native_Free(&f);
	}
	if (doc) {
		Native_SetText(&d, doc, (int)strlen(doc));
		Lib_Put(out, "__doc__", &d);
		Native_Free(&d);
	}
}
