/*
 * The helpers of the engine modules on native entries (include/scrap/scriptapi.h, stage 7 of docs/objlib-plan.md):
 * the results in the forms of Py_BuildValue (Python/modsupport.c of 1.5.2: countformat, do_mkvalue, do_mktuple) and
 * the entity handles as native values (NV_ENT, no Python object).
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/nativeobj.h"
#include "scrap/scriptapi.h"

/* the items of a format up to end (')' or the end of the text) at its top level, as countformat counts them */
static int Api_Count(const char *f, char end) {
	int count = 0, level = 0;
	for (; level > 0 || *f != end; f++) {
		CHECK(*f);
		if (*f == '(') {
			count += !level;
			level++;
		} else if (*f == ')') {
			level--;
		} else if (!level && *f != ' ' && *f != '\t' && *f != ',' && *f != ':') {
			count++;
		}
	}
	return count;
}

/* the blanks and commas between the items */
static void Api_Skip(const char **f) {
	while (**f == ' ' || **f == '\t' || **f == ',' || **f == ':') {
		(*f)++;
	}
}

static void Api_Item(nativeVal_t *v, const char **f, va_list *va);

/* the n items up to end as a tuple (do_mktuple), *f after end */
static void Api_Tuple(nativeVal_t *v, const char **f, va_list *va, char end, int n) {
	Native_SetEmpty(v, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Api_Item(Native_Append(v), f, va);
	}
	Api_Skip(f);
	CHECK(**f == end);
	if (end) {
		(*f)++;
	}
}

/* one item (do_mkvalue) */
static void Api_Item(nativeVal_t *v, const char **f, va_list *va) {
	Api_Skip(f);
	char c = *(*f)++;
	switch (c) {
	case '(':
		Api_Tuple(v, f, va, ')', Api_Count(*f, ')'));
		break;
	case 'i':
		Native_SetInt(v, va_arg(*va, int));
		break;
	case 'l':
		Native_SetInt(v, va_arg(*va, long));
		break;
	case 'd':
	case 'f':
		Native_SetFloat(v, va_arg(*va, double));
		break;
	case 's': {
		const char *s = va_arg(*va, const char *);
		if (s) {
			Native_SetText(v, s, (int)strlen(s));
		} else {
			Native_SetNone(v);
		}
		break;
	}
	default:
		CHECK(c == 'v');
		Native_Copy(v, va_arg(*va, const nativeVal_t *));
		break;
	}
}

byte Api_Build(nativeVal_t *out, const char *fmt, ...) {
	va_list va;
	int n = Api_Count(fmt, 0);
	va_start(va, fmt);
	if (!n) {
		Native_SetNone(out);
	} else if (n == 1) {
		Api_Item(out, &fmt, &va);
	} else {
		Api_Tuple(out, &fmt, &va, 0, n);
	}
	va_end(va);
	return 1;
}

byte Api_Handle(const char *name, nativeVal_t *out) {
	return Native_SetEnt(out, name);
}
