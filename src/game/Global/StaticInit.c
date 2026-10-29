/*
 * The dynamic initializers of the game's globals (docs/standalone.md, stage 2d): the functions the CRT of the original
 * calls from its __xc table before WinMain (rva 0x03e800..0x05f6d8), and the atexit destructors they register (rva
 * 0x38c720..0x38f097). Each one is a hook. Crt_Main of ScrapC.exe walks the table of the original, so they run in its
 * order, and the destructors are registered with crt_atexit in the same order as the original did.
 *
 * Nearly all of them follow a few patterns: one macro per pattern below, one line per function in StaticInit.inc,
 * which tools/standalone/initpat.py makes from the code of the exe. The first argument of a line is the rva of the
 * function, the destructor an initializer registers comes right before it. The few that fit no pattern are in the
 * files of their classes (the HAND table of the tool). Names and descriptions are the strings of the exe.
 *
 * The hook DLL installs these hooks too. There the initializers never run (the exe ran them before it loaded the
 * DLL), but the destructors do, at the exit.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/archio.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/fonts.h"
#include "scrap/global.h"
#include "scrap/graphic2d.h"
#include "scrap/hashtable.h"
#include "scrap/input.h"
#include "scrap/material.h"
#include "scrap/musicfmod.h"
#include "scrap/net.h"
#include "scrap/pyfunction.h"
#include "scrap/scoreritems.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapparticles.h"
#include "scrap/groundportal.h"
#include "scrap/table.h"
#include "scrap/misc.h"

/* the objects of the data at the rvas of the lines: in ScrapC.exe the C objects of src/game/Global/Globals*.c through
 * GlobalsAt.h (GAME_PTR_AT), in the hook DLL the exe's own */
#define SI_OBJ(rva) GAME_PTR_AT(rva)
#define SI_GVAR(rva) ((cGVar *)SI_OBJ(rva))
#ifdef SD_STANDALONE
#include "GlobalsAt.h"
#include "StringsAt.h"
#endif

#define SI_STR(rva) GAME_STR_AT(rva)
#define SI_FLOAT(rva) (*(float *)SI_OBJ(rva))

/* ---- engine variables (cGVar): name, flags, description and the value ---- */

#define INIT_GVAR_INT(rva, fn, obj, name, flags, desc, value) \
	void fn(void) { \
		GVar_InitInt(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), value); \
	}
#define INIT_GVAR_FLOAT(rva, fn, obj, name, flags, desc, value) \
	void fn(void) { \
		GVar_InitFloat(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), value); \
	}
/* the value from a float of the data, read before the constructor */
#define INIT_GVAR_FLOAT_DATA(rva, fn, obj, name, flags, desc, src) \
	void fn(void) { \
		float v = SI_FLOAT(src); \
		GVar_InitFloat(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), v); \
	}
#define INIT_GVAR_FLOAT_MULDIV(rva, fn, obj, name, flags, desc, src, mul, div) \
	void fn(void) { \
		float v = SI_FLOAT(src) * SI_FLOAT(mul) / SI_FLOAT(div); \
		GVar_InitFloat(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), v); \
	}
#define INIT_GVAR_FLOAT_ADD(rva, fn, obj, name, flags, desc, src, add) \
	void fn(void) { \
		float v = SI_FLOAT(src) + SI_FLOAT(add); \
		GVar_InitFloat(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), v); \
	}
#define INIT_GVAR_STRING(rva, fn, fini, obj, name, flags, desc, value) \
	void fini(void); \
	void fn(void) { \
		GVar_InitString(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), SI_STR(value)); \
		crt_atexit(fini); \
	}
#define FINI_GVAR_STRING(rva, fn, obj) \
	void fn(void) { \
		GVar_FiniString(SI_GVAR(obj)); \
	}
/* func 0: no function */
#define INIT_GVAR_FUNCTION(rva, fn, fini, obj, name, flags, desc, func) \
	void fini(void); \
	void fn(void) { \
		GVar_InitFunction(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), (func) ? SI_STR(func) : NULL); \
		crt_atexit(fini); \
	}

/* ---- the registry of C functions for the scripts ---- */

#define INIT_NATIVE_FUNC(rva, fn, obj, name, func, cfunc) \
	void fn(void) { \
		cPy_NativeFunc_ctor(SI_OBJ(obj), SI_STR(name), (nativefunc_t)GAME_FUNC(cfunc, func)); \
	}

/* ---- objects with a constructor of the C ---- */

/* only the destructor: the object is all data */
#define INIT_ATEXIT(rva, fn, fini) \
	void fini(void); \
	void fn(void) { \
		crt_atexit(fini); \
	}
#define INIT_OBJECT(rva, fn, fini, obj, ctor) \
	void fini(void); \
	void fn(void) { \
		ctor(SI_OBJ(obj)); \
		crt_atexit(fini); \
	}
#define INIT_OBJECT_ARGS(rva, fn, fini, obj, ctor, ...) \
	void fini(void); \
	void fn(void) { \
		ctor(SI_OBJ(obj), __VA_ARGS__); \
		crt_atexit(fini); \
	}
/* without destructor */
#define INIT_CALL(rva, fn, obj, ctor) \
	void fn(void) { \
		ctor(SI_OBJ(obj)); \
	}
#define INIT_CALL_ARGS(rva, fn, obj, ctor, ...) \
	void fn(void) { \
		ctor(SI_OBJ(obj), __VA_ARGS__); \
	}
#define INIT_JUMP(rva, fn, func) \
	void fn(void) { \
		func(); \
	}
/* an array through the vector constructor iterator of the CRT */
#define INIT_VECTOR(rva, fn, fini, obj, size, count, ctor, dtor) \
	void fini(void); \
	void fn(void) { \
		crt_eh_vector_ctor(SI_OBJ(obj), size, count, (void *)ctor, (void *)dtor); \
		crt_atexit(fini); \
	}
/* an array, the constructor in a loop of the initializer */
#define INIT_ARRAY(rva, fn, obj, size, count, ctor) \
	void fn(void) { \
		for (int i = 0; i < (count); i++) { \
			ctor((void *)((byte *)SI_OBJ(obj) + i * (size))); \
		} \
	}
#define INIT_VECTOR3(rva, fn, obj, vx, vy, vz) \
	void fn(void) { \
		vec3_t *v = SI_OBJ(obj); \
		v->x = vx; \
		v->y = vy; \
		v->z = vz; \
	}
/* the slots of a HashTable whose numSlots is data: new[], cleared, then locked and caseSensitive */
#define INIT_HASHTABLE(rva, fn, fini, table, count, size, clear, lock, cs) \
	void fini(void); \
	void fn(void) { \
		HashTable *t = SI_OBJ(table); \
		t->slots = New_Array(count, size); \
		memset(t->slots, 0, clear); \
		t->locked = lock; \
		t->caseSensitive = cs; \
		crt_atexit(fini); \
	}

/* ---- destructors ---- */

#define FINI_CALL(rva, fn, obj, dtor) \
	void fn(void) { \
		dtor(SI_OBJ(obj)); \
	}
#define FINI_CALL2(rva, fn, obj1, dtor1, obj2, dtor2) \
	void fn(void) { \
		dtor1(SI_OBJ(obj1)); \
		dtor2(SI_OBJ(obj2)); \
	}
#define FINI_VECTOR(rva, fn, obj, size, count, dtor) \
	void fn(void) { \
		crt_eh_vector_dtor(SI_OBJ(obj), size, count, (void *)dtor); \
	}
/* a DList: its nodes by its own function, then the free list */
#define FINI_DLIST(rva, fn, obj, clear, size) \
	void fn(void) { \
		clear(SI_OBJ(obj)); \
		DList_FreePool(SI_OBJ(obj), size); \
	}
#define FINI_TABLE(rva, fn, obj) \
	void fn(void) { \
		Table_Free(SI_OBJ(obj)); \
	}

#include "StaticInit.inc"
