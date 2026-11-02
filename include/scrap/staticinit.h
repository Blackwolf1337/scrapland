/*
 * The dynamic initializers of the globals of the game: the functions the CRT of the original called from its __xc
 * table before WinMain, and the atexit destructors they register. Crt_Main calls them in the order of the original
 * (src/standalone/crtxc.inc); each file defines the ones of its globals with these macros, the destructor an
 * initializer registers right before it. Names and descriptions are the strings of the exe.
 */
#ifndef SCRAP_STATICINIT_H
#define SCRAP_STATICINIT_H

#include <string.h>

#include "scrap/crt.h"
#include "scrap/gvar.h"
#include "scrap/hashtable.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"
#include "scrap/table.h"
#include "scrap/dlist.h"

#define SI_GVAR(p) ((cGVar *)(p))
#define SI_FLOAT(p) (*(float *)(p))

/* ---- engine variables (cGVar): name, flags, description and the value ---- */

#define INIT_GVAR_INT(fn, obj, name, flags, desc, value) \
	void fn(void) { \
		GVar_InitInt(SI_GVAR(obj), name, flags, desc, value); \
	}
#define INIT_GVAR_FLOAT(fn, obj, name, flags, desc, value) \
	void fn(void) { \
		GVar_InitFloat(SI_GVAR(obj), name, flags, desc, value); \
	}
/* the value from a float of the data, read before the constructor */
#define INIT_GVAR_FLOAT_DATA(fn, obj, name, flags, desc, src) \
	void fn(void) { \
		float v = SI_FLOAT(src); \
		GVar_InitFloat(SI_GVAR(obj), name, flags, desc, v); \
	}
#define INIT_GVAR_FLOAT_MULDIV(fn, obj, name, flags, desc, src, mul, div) \
	void fn(void) { \
		float v = SI_FLOAT(src) * SI_FLOAT(mul) / SI_FLOAT(div); \
		GVar_InitFloat(SI_GVAR(obj), name, flags, desc, v); \
	}
#define INIT_GVAR_FLOAT_ADD(fn, obj, name, flags, desc, src, add) \
	void fn(void) { \
		float v = SI_FLOAT(src) + SI_FLOAT(add); \
		GVar_InitFloat(SI_GVAR(obj), name, flags, desc, v); \
	}
#define INIT_GVAR_STRING(fn, fini, obj, name, flags, desc, value) \
	void fini(void); \
	void fn(void) { \
		GVar_InitString(SI_GVAR(obj), name, flags, desc, value); \
		crt_atexit(fini); \
	}
#define FINI_GVAR_STRING(fn, obj) \
	void fn(void) { \
		GVar_FiniString(SI_GVAR(obj)); \
	}
/* func NULL: no function */
#define INIT_GVAR_FUNCTION(fn, fini, obj, name, flags, desc, func) \
	void fini(void); \
	void fn(void) { \
		GVar_InitFunction(SI_GVAR(obj), name, flags, desc, func); \
		crt_atexit(fini); \
	}

/* ---- the registry of C functions for the scripts ---- */

#define INIT_NATIVE_FUNC(fn, obj, name, cfunc) \
	void fn(void) { \
		cPy_NativeFunc_ctor((obj), name, (nativefunc_t)cfunc); \
	}

/* ---- objects with a constructor of the C ---- */

/* only the destructor: the object is all data */
#define INIT_ATEXIT(fn, fini) \
	void fini(void); \
	void fn(void) { \
		crt_atexit(fini); \
	}
#define INIT_OBJECT(fn, fini, obj, ctor) \
	void fini(void); \
	void fn(void) { \
		ctor((obj)); \
		crt_atexit(fini); \
	}
#define INIT_OBJECT_ARGS(fn, fini, obj, ctor, ...) \
	void fini(void); \
	void fn(void) { \
		ctor((obj), __VA_ARGS__); \
		crt_atexit(fini); \
	}
/* without destructor */
#define INIT_CALL(fn, obj, ctor) \
	void fn(void) { \
		ctor((obj)); \
	}
#define INIT_CALL_ARGS(fn, obj, ctor, ...) \
	void fn(void) { \
		ctor((obj), __VA_ARGS__); \
	}
#define INIT_JUMP(fn, func) \
	void fn(void) { \
		func(); \
	}
/* an array through the vector constructor iterator of the CRT */
#define INIT_VECTOR(fn, fini, obj, size, count, ctor, dtor) \
	void fini(void); \
	void fn(void) { \
		crt_eh_vector_ctor((obj), size, count, (void *)ctor, (void *)dtor); \
		crt_atexit(fini); \
	}
/* an array, the constructor in a loop of the initializer */
#define INIT_ARRAY(fn, obj, size, count, ctor) \
	void fn(void) { \
		for (int i = 0; i < (count); i++) { \
			ctor((void *)((byte *)(obj) + i * (size))); \
		} \
	}
#define INIT_VECTOR3(fn, obj, vx, vy, vz) \
	void fn(void) { \
		vec3_t *v = (obj); \
		v->x = vx; \
		v->y = vy; \
		v->z = vz; \
	}
/* the slots of a HashTable whose numSlots is data: new[], cleared, then locked and caseSensitive */
#define INIT_HASHTABLE(fn, fini, table, count, size, clear, lock, cs) \
	void fini(void); \
	void fn(void) { \
		HashTable *t = (table); \
		(void)(size), (void)(clear);	/* the 32 bit slot size and bytes of the original's call */ \
		t->slots = New_Array(count, sizeof(*t->slots)); \
		memset(t->slots, 0, (count) * sizeof(*t->slots)); \
		t->locked = lock; \
		t->caseSensitive = cs; \
		crt_atexit(fini); \
	}

/* ---- destructors ---- */

#define FINI_CALL(fn, obj, dtor) \
	void fn(void) { \
		dtor((obj)); \
	}
#define FINI_CALL2(fn, obj1, dtor1, obj2, dtor2) \
	void fn(void) { \
		dtor1((obj1)); \
		dtor2((obj2)); \
	}
#define FINI_VECTOR(fn, obj, size, count, dtor) \
	void fn(void) { \
		crt_eh_vector_dtor((obj), size, count, (void *)dtor); \
	}
/* a DList: its nodes by its own function, then the free list */
#define FINI_DLIST(fn, obj, clear, size) \
	void fn(void) { \
		clear((obj)); \
		DList_FreePool((obj), size); \
	}
#define FINI_TABLE(fn, obj) \
	void fn(void) { \
		Table_Free((obj)); \
	}

#endif
