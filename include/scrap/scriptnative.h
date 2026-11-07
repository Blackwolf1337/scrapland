/*
 * The first pieces of the native layer of the script ports (removal of the Python 1.5.2 object library, plan in
 * docs/objlib-plan.md). A port moved to native C keeps its state in C, calls the engine's C functions and works on
 * C values; its PyMethodDef entries stay as adapters (arguments in, the C call, the result and the error out) while
 * Python is linked.
 *
 * Transitional parts are marked OBJLIB <stage> where they are used (the stage of the plan that removes them): the
 * mirror of module state in the module dict (OBJLIB 5), the seams of engine functions a test replaces (OBJLIB 7),
 * the Python errors behind Native_Raise (OBJLIB 7).
 */
#ifndef SCRAP_SCRIPTNATIVE_H
#define SCRAP_SCRIPTNATIVE_H

#include "scrap/entity.h"
#include "scrap/python.h"

/* ---- values ---- */

/* the longest text a native value holds (with its 0) */
#define NATIVE_TEXT 0x80

typedef enum {
	NV_NONE,
	NV_INT,
	NV_FLOAT,
	NV_STR,
	NV_VEC,
} nativeType_t;

/* a value of the scripts in C: None, an int (a C long, as the int of Python 1.5.2), a float, a text or a vector */
typedef struct nativeVal_s {
	byte type;			/* NV_ */
	long i;
	double f;
	double v[3];
	char s[NATIVE_TEXT];
} nativeVal_t;

void Native_SetNone(nativeVal_t *v);
void Native_SetInt(nativeVal_t *v, long i);
void Native_SetFloat(nativeVal_t *v, double f);
/* the text s (cut to NATIVE_TEXT - 1 characters) */
void Native_SetStr(nativeVal_t *v, const char *s);
/* the value is the text */
byte Native_IsStr(const nativeVal_t *v, const char *text);
/* a == b as Python 1.5.2 compares them: numbers by value, texts by content, None only with None; a text never equals
 * a number */
byte Native_Equal(const nativeVal_t *a, const nativeVal_t *b);

/* ---- errors ---- */

/* the kinds of error a native function raises; while Python is linked each is the Python exception of that name */
typedef enum {
	NE_INDEX,
	NE_VALUE,
	NE_TYPE,
	NE_ATTRIBUTE,
	NE_SYSTEM,
} nativeError_t;

/* raises the error kind with the text (printf form); 0, for `return Native_Raise(...)` in a function that returns
 * 1 for success. OBJLIB 7: the Python exception now, a C error state after Python */
byte Native_Raise(nativeError_t kind, const char *fmt, ...);
/* i as an index of a list of count items as Python indexes it (a negative i counts from the end): 1 with *out, else
 * 0 with the IndexError of a list */
byte Native_Index(long i, int count, int *out);
/* int(text) of Python 1.5.2 on a text (blanks around it, a sign, decimal digits): 1 with *out, else 0 with its
 * ValueError ("invalid literal for int(): ...", "int() literal too large: ...") */
byte Native_IntOfText(const char *text, long *out);

/* ---- the engine ---- */

/* entity.name as the script handle reads it: an int, float, vector or text variable (a text variable without a text
 * is None). 0 when the entity has no such variable: the console line of the handle and its AttributeError */
byte Native_EntVar(cEntity *e, const char *ref, const char *name, nativeVal_t *out);

/*
 * OBJLIB 7, the seam of an engine function that a test case replaces (fc-chat records SNet.SendChatString and
 * swaps Scrap.GetNetFlags): 0 when module.name is still the engine's own C function (the port calls the C directly),
 * 1 when something else is there (*hook a new reference: the port calls it as the script did), -1 with the error of
 * the lookup set
 */
int Native_Seam(const char *module, const char *name, PyObject **hook);
/* Scrap.GetNetFlags()[i] (0 client, 1 server, 2 dedicated): 1 or 0, -1 with the error set (through the seam) */
int Native_NetFlag(int i);

/* ---- module state mirrored into the module dict (OBJLIB 5) ---- */

/*
 * A variable of a module kept in C. While other code (the module code of the .mod file, the scripts' readers of
 * other modules, the test cases) still reads and writes the module dict, the dict keeps the value between calls:
 * Native_Import at the start of every entry point of the module takes the dict values into C, Native_Put writes a
 * C change through to the dict at once (a list is changed in place, so references to it see the change).
 */
typedef struct nativeVar_s {
	const char *name;		/* the variable of the module */
	nativeVal_t *values;		/* the C value, or the count items of a list */
	int count;			/* 0: a plain variable; else a list of exactly count items */
	unsigned types;			/* the NV_ types the C takes (1 << NV_STR | ...) */
} nativeVar_t;

/* the variables of the table (ends with a NULL name) from the dict of module into C: 0 with the error set when one
 * is missing or of a form the C does not take (TypeError naming it) */
byte Native_Import(const char *module, const nativeVar_t *vars);
/* the C value of var (index: the item of a list, ignored for a plain variable) into the dict of module: 0 with the
 * error set */
byte Native_Put(const char *module, const nativeVar_t *var, int index);

/* the value as a new Python object (NULL with the error set): for adapters and mirrors only */
PyObject *Native_ToPy(const nativeVal_t *v);
/* a Python value into *v when its type is one of types (NV_ mask): 1, else 0 without an error */
byte Native_FromPy(PyObject *o, unsigned types, nativeVal_t *v);

#endif
