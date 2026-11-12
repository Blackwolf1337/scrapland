/*
 * The string module of the packs in C (scripts/python/string.pyc, the string.py of Python 1.5.2). Its module code is
 * data/modules/string.mod: the variables, and for most functions the C functions of the built-in module strop (the
 * script ends with `from strop import *`, which replaces its own Python versions). The functions string.py keeps in
 * Python are here: index, rindex, ljust, rjust, center, zfill and capwords. They use the operators of the script on
 * any object (len, -, *, +, the comparisons), so a wrong argument fails with the error of the line of string.py, and
 * load the other functions (find, rfind, split, capitalize, join) and the builtins as the script does (LOAD_GLOBAL:
 * the module, then the builtins).
 *
 * The functions work on values held by reference (OBJLIB 7 and 9: the operators of any object and the builtins are
 * Python's until the object library goes; the Mode_Ref helpers of scrap/scriptmodes.h): the entries take the
 * caller's objects as they are (the native calls; MODE_REFKW for keywords), so a function that gives its argument
 * back gives the same object.
 */
#include <limits.h>

#include "scrap/script.h"
#include "scrap/scriptmodes.h"

/* a global of string as its functions load it (the module, then the builtins) */
static byte Str_Global(const char *name, nativeVal_t *out) {
	execScope_t x = { .module = "string" };
	int found = Mode_EvalChain(&x, name, out);
	return found > 0 || (found < 0 && Native_Raise(NE_SYSTEM, "string: cannot load %.100s", name));
}

/* len(s) as a value: 0 with the error of a value without length */
static byte Str_Len(const nativeVal_t *s, nativeVal_t *out) {
	int len = Mode_RefSize(s);
	if (len < 0 && Native_Failed()) {
		return 0;
	}
	Native_SetInt(out, len);
	return 1;
}

/* width - len(s) */
static byte Str_Room(const nativeVal_t *width, const nativeVal_t *s, nativeVal_t *out) {
	nativeVal_t n;
	Native_SetNone(out);
	return Str_Len(s, &n) && Mode_RefOp('-', width, &n, out);
}

/* `n <= 0` of the padding functions: 1, 0, or -1 with the error set */
static int Str_None(const nativeVal_t *n) {
	nativeVal_t zero;
	Native_SetInt(&zero, 0);
	int c = Mode_RefCmp(n, &zero);
	return c == -2 ? -1 : c <= 0;
}

/* text * n */
static byte Str_Times(const char *text, const nativeVal_t *n, nativeVal_t *out) {
	nativeVal_t t;
	Native_SetStr(&t, text);
	return Mode_RefOp('*', &t, n, out);
}

/* f(a, b[, c, d]) of a function of the module, the result by reference */
static byte Str_Call(const nativeVal_t *f, nativeVal_t *out, int n, const nativeVal_t *a, const nativeVal_t *b,
	const nativeVal_t *c, const nativeVal_t *d) {
	const nativeVal_t args[4] = { *a, *b, n > 2 ? *c : *a, n > 3 ? *d : *a };
	return Mode_RefApply(f, args, n, out);
}

/* index(s, sub, i=0, last=None) and rindex: find (rfind) of the module, ValueError when the result is negative */
static byte Str_Index(const nativeVal_t *v, const byte *given, const char *find, nativeVal_t *out) {
	nativeVal_t zero, len, f = { 0 }, res = { 0 };
	const nativeVal_t *i = given[2] ? &v[2] : &zero, *last = &v[3];
	Native_SetInt(&zero, 0);
	if (!given[3] || Mode_RefIsNone(last)) {
		if (!Str_Len(&v[0], &len)) {
			return 0;
		}
		last = &len;
	}
	byte ok = Str_Global(find, &f) && Str_Call(&f, &res, 4, &v[0], &v[1], i, last);
	Native_Free(&f);
	int c = ok ? Mode_RefCmp(&res, &zero) : -2;
	if (c >= 0) {
		*out = res;
		return 1;
	}
	Native_Free(&res);
	return c == -2 ? 0 : Native_Raise(NE_VALUE, "substring not found in string.index");
}

MODE_REFKW(String, index, "OO|OO", "s", "sub", "i", "last") {
	return Str_Index(v, given, "find", out);
}

MODE_REFKW(String, rindex, "OO|OO", "s", "sub", "i", "last") {
	return Str_Index(v, given, "rfind", out);
}

/* the start of ljust, rjust and center: n = width - len(s); 1 with n, 0 when the function is done (ok: s itself as
 * the result when it is that long, else the error) */
static byte Str_Short(const nativeVal_t *s, const nativeVal_t *width, nativeVal_t *n, nativeVal_t *out, byte *ok) {
	int le = Str_Room(width, s, n) ? Str_None(n) : -1;
	if (le > 0) {
		Native_Copy(out, s);
	}
	*ok = le > 0;
	if (le) {
		Native_Free(n);
	}
	return !le;
}

/* ljust(s, width), rjust(s, width): s with blanks after (before) it up to width; s itself when it is that long */
static byte Str_Just(const nativeVal_t *s, const nativeVal_t *width, byte right, nativeVal_t *out) {
	nativeVal_t n, pad = { 0 };
	byte ok;
	if (!Str_Short(s, width, &n, out, &ok)) {
		return ok;
	}
	ok = Str_Times(" ", &n, &pad) && (right ? Mode_RefOp('+', &pad, s, out) : Mode_RefOp('+', s, &pad, out));
	Native_Free(&pad);
	Native_Free(&n);
	return ok;
}

static byte String_ljust_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return n == 2 ? Str_Just(&a[0], &a[1], 0, out) : Port_Arity(n, 2);
}

static byte String_rjust_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return n == 2 ? Str_Just(&a[0], &a[1], 1, out) : Port_Arity(n, 2);
}

/* `if v:` of a value made by an operator (released): 1, 0, -1 with the error (made: the operator gave it) */
static int Str_True(byte made, nativeVal_t *v) {
	int t = made ? Mode_Truth(v) : -1;
	Native_Free(v);
	return t;
}

/* center(s, width): half the blanks before s (one more when both n and width are odd), the rest after it */
static byte Str_Center(const nativeVal_t *s, const nativeVal_t *width, nativeVal_t *out) {
	nativeVal_t n, two, one, half = { 0 }, r = { 0 }, left = { 0 }, mid = { 0 }, rest = { 0 }, right = { 0 };
	byte ok;
	if (!Str_Short(s, width, &n, out, &ok)) {
		return ok;
	}
	Native_SetInt(&two, 2);
	Native_SetInt(&one, 1);
	/* if n%2 and width%2: half = half+1 */
	int odd = Mode_RefOp('/', &n, &two, &half) ? Str_True(Mode_RefOp('%', &n, &two, &r), &r) : -1;
	if (odd > 0) {
		odd = Str_True(Mode_RefOp('%', width, &two, &r), &r);
		if (odd > 0) {
			odd = Mode_RefOp('+', &half, &one, &r) ? 0 : -1;
			Native_Free(&half);
			half = r;
		}
	}
	ok = odd >= 0 && Str_Times(" ", &half, &left) && Mode_RefOp('+', &left, s, &mid) && Mode_RefOp('-', &n, &half, &rest) &&
		Str_Times(" ", &rest, &right) && Mode_RefOp('+', &mid, &right, out);
	Native_Free(&right);
	Native_Free(&rest);
	Native_Free(&mid);
	Native_Free(&left);
	Native_Free(&half);
	Native_Free(&n);
	return ok;
}

static byte String_center_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return n == 2 ? Str_Center(&a[0], &a[1], out) : Port_Arity(n, 2);
}

/* zfill(x, width): the string (or the repr of x) with zeros after its sign up to width */
static byte Str_Zfill(const nativeVal_t *x, const nativeVal_t *width, nativeVal_t *out) {
	nativeVal_t s = { 0 }, n, plain, sign, first = { 0 }, tail = { 0 }, room = { 0 }, zeros = { 0 }, head = { 0 };
	Mode_RefPlain(x, &plain);
	byte text = plain.type == NV_STR;
	Native_Free(&plain);
	if (text) {
		Native_Copy(&s, x);
	} else if (!Mode_RefRepr(x, &s)) {
		return 0;
	}
	int c = Str_Len(&s, &n) ? Mode_RefCmp(&n, width) : -2;
	if (c == -2 || c >= 0) {
		if (c != -2) {
			*out = s;
			return 1;
		}
		Native_Free(&s);
		return 0;
	}
	Native_SetStr(&sign, "");
	byte ok = Mode_RefItem(&s, 0, &first);
	if (ok) {
		Mode_RefPlain(&first, &plain);
		const char *f = plain.type == NV_STR ? Native_Text(&plain) : "";
		if ((f[0] == '-' || f[0] == '+') && f[1] == 0) {
			Native_Copy(&sign, &first);
			ok = Mode_RefSlice(&s, 1, INT_MAX, &tail);
			Native_Free(&s);
			s = tail;
		}
		Native_Free(&plain);
	}
	ok = ok && Mode_RefOp('-', width, &n, &room) && Str_Times("0", &room, &zeros) && Mode_RefOp('+', &sign, &zeros, &head) &&
		Mode_RefOp('+', &head, &s, out);
	Native_Free(&head);
	Native_Free(&zeros);
	Native_Free(&room);
	Native_Free(&first);
	Native_Free(&sign);
	Native_Free(&s);
	return ok;
}

static byte String_zfill_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return n == 2 ? Str_Zfill(&a[0], &a[1], out) : Port_Arity(n, 2);
}

/* capwords(s, sep=None): join(map(capitalize, split(s, sep)), sep or ' ') with the functions of the module */
MODE_REFKW(String, capwords, "O|O", "s", "sep") {
	nativeVal_t join = { 0 }, map = { 0 }, cap = { 0 }, split = { 0 }, words = { 0 }, caps = { 0 }, blank;
	byte ok = Str_Global("join", &join) && Str_Global("map", &map) && Str_Global("capitalize", &cap) &&
		Str_Global("split", &split) && Str_Call(&split, &words, 2, &v[0], &v[1], NULL, NULL) &&
		Str_Call(&map, &caps, 2, &cap, &words, NULL, NULL);
	int t = ok ? Mode_Truth(&v[1]) : -1;
	Native_SetStr(&blank, " ");
	ok = t >= 0 && Str_Call(&join, out, 2, &caps, t ? &v[1] : &blank, NULL, NULL);
	Native_Free(&caps);
	Native_Free(&words);
	Native_Free(&split);
	Native_Free(&cap);
	Native_Free(&map);
	Native_Free(&join);
	return ok;
}

const portFn_t g_StringMethods[] = {
	PORT_FNK(index, String_index),
	PORT_FNK(rindex, String_rindex),
	PORT_FN(ljust, String_ljust),
	PORT_FN(rjust, String_rjust),
	PORT_FN(center, String_center),
	PORT_FN(zfill, String_zfill),
	PORT_FNK(capwords, String_capwords),
	PORT_FNEND
};

PORTS(String)
