/*
 * The engine variables (gvars, Scrap.Get/Set/Def/Des), Global/Gvars.cpp: the types only, so any header can declare
 * its variables (extern cGVar GVar_X) and name their values (GVar_X.value.i). The functions are in scrap.h.
 */
#ifndef SCRAP_GVAR_H
#define SCRAP_GVAR_H

#include "scrap/shared.h"

enum { GVAR_STRING = 1, GVAR_INT = 2, GVAR_FLOAT = 4, GVAR_FUNCTION = 5, GVAR_SCRIPT_FUNCTION = 6 };

/* the class cGvar, 0x10 bytes: the part before the value (the value and the default are of the typed variables,
 * classes without RTTI) */
typedef struct cGvar {
	struct cGVar *next;		/* 0x00 g_GVars */
	char *name;			/* 0x04 */
	char *description;		/* 0x08 */
	byte flags;			/* 0x0c bit 0: scripts may not write it */
	byte type;			/* 0x0d GVAR_ */
	byte pad_e[2];
} cGvar;
SD_SIZE(cGvar, 0x10);

/*
 * an engine variable (Global/Gvars.cpp). The pointers in g_GVars point 4 bytes into the object: a GVAR_FUNCTION
 * variable has a vtable before it (cGVarListVtbl) that saves and loads its lines of the config file.
 */
typedef struct cGVar {
	cGvar base;			/* 0x00 */
	union {				/* 0x10 current value */
		char *s;		/* GVAR_STRING: first member of a string object, cString_Set writes it */
		int i;
		float f;
	} value;
	union {				/* 0x14 default value */
		char *s;
		int i;
		float f;
	} def;
} cGVar;
SD_OFFSET(cGVar, def, 0x14);
/* GVAR_SCRIPT_FUNCTION keeps a cPy_Function at +0x10 instead of value/def */
#define GVar_Function(v) ((cPy_Function *)&(v)->value)
SD_OFFSET(cGvar, type, 0x0d);
SD_OFFSET(cGVar, value, 0x10);

#endif
