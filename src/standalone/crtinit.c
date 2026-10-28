/*
 * The compiler support of the MSVC runtime in ScrapC.exe (stage 2b of docs/standalone.md): the start of the process
 * instead of the entry point of the original, atexit and exit, the guards of the thread-safe function statics and
 * the vector constructor and destructor iterators. The functions are SD_IMPORT_CRT in src/imports.inc, so the
 * entries of the original jump here too (src/core/hook.c).
 *
 * The entry of the original (rva 0x314919: __security_init_cookie, then __scrt_common_main_seh at 0x31478c) runs
 * the C initializers of the __xi table with _initterm_e, the C++ initializers of the __xc table with _initterm and
 * then WinMain; exit() with its result runs the atexit functions, last registered first. Crt_Main does the same
 * with the tables of the original: every __xi entry and the first __xc one belong to the CRT, the other __xc entries
 * are the dynamic initializers of the game's globals. What the CRT ones set that the game can see is set here.
 */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <windows.h>
#include <corecrt_startup.h>

#include "scrap/app.h"
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/origcrt.h"

/* the bounds the entry code pushes for _initterm_e (rva 0x3147d2) and _initterm (rva 0x3147f8) */
#define CRT_XI_A 0x3a31c0
#define CRT_XI_Z 0x3a31e4
#define CRT_XC_A 0x3a15bc
#define CRT_XC_Z 0x3a31bc

/* the matherr of the game, which __scrt_common_main_seh registers with __setusermatherr (rva 0x23aae0) */
int E3D_MathErr(void *exc);

/* ---- atexit and exit ---- */

/* the onexit table of the original grows on the heap; 2048 entries are far more than the 354 call sites */
static void (*crt_onexit[2048])(void);
static int crt_onexit_count;

int crt_atexit(void (*func)(void)) {
	if (crt_onexit_count == (int)(sizeof(crt_onexit) / sizeof(crt_onexit[0]))) {
		return -1;
	}
	crt_onexit[crt_onexit_count++] = func;
	return 0;
}

/* the atexit functions, last registered first; one registered while they run is run as well */
static void Crt_RunOnexit(void) {
	while (crt_onexit_count > 0) {
		crt_onexit[--crt_onexit_count]();
	}
}

[[noreturn]] void crt_exit(int code) {
	Crt_RunOnexit();
	exit(code);
}

/* _exit: no atexit functions, no flush */
[[noreturn]] void crt_exit_now(int code) {
	_exit(code);
}

/* ---- thread-safe statics (_Init_thread_header and _Init_thread_footer) ---- */

/*
 * The original keeps the epoch per thread in its TLS block (+0x20) and the global one at rva 0x43cfe8, both start at
 * INT_MIN; a static is made when its guard is above the epoch of the thread. Only the main thread runs game code, so
 * one epoch serves here, and the critical section and the wait of the original for a static another thread is still
 * making are left out (the guard is -1 then).
 */
int crt_init_epoch = INT_MIN;
static int crt_init_global_epoch = INT_MIN;

void crt_Init_thread_header(int *guard) {
	if (*guard == 0) {
		*guard = -1;
	} else {
		crt_init_epoch = crt_init_global_epoch;
	}
}

void crt_Init_thread_footer(int *guard) {
	*guard = ++crt_init_global_epoch;
	crt_init_epoch = crt_init_global_epoch;
}

/* ---- the vector iterators of new[] and delete[] for objects (no C++ exceptions here, so no unwind) ---- */

void __stdcall crt_eh_vector_ctor(void *p, uint32_t size, uint32_t count, void *ctor, void *dtor) {
	(void)dtor;
	for (; count; count--) {
		((void(__thiscall *)(void *))ctor)(p);
		p = (byte *)p + size;
	}
}

/* last object first */
void __stdcall crt_eh_vector_dtor(void *p, uint32_t size, uint32_t count, void *dtor) {
	for (byte *q = (byte *)p + size * count; count; count--) {
		q -= size;
		((void(__thiscall *)(void *))dtor)(q);
	}
}

/* operator delete(void *, size_t) of the runtime: the engine's operator delete (rva 0x1334e0) */
void operator_delete_sized(void *p, uint32_t size) {
	(void)size;
	operator_delete(p);
}

/* ---- the start ---- */

/* what the CRT initializers of the __xi table set that the game sees */
static void Crt_InitState(void) {
	/* _initialize_default_precision (rva 0x314f21): the x87 rounds to 53 bits */
	_controlfp_s(NULL, _PC_53, _MCW_PC);
	/* __scrt_initialize_default_local_stdio_options (rva 0x314f44, again at 0x37b3a8): the option words of the
	 * inline printf and scanf get _CRT_INTERNAL_PRINTF_LEGACY_WIDE_SPECIFIERS and the scanf one */
	*crt_local_printf_options() |= 4;
	*crt_local_scanf_options() |= 2;
	__setusermatherr((int (*)(struct _exception *))E3D_MathErr);
}

/* the dynamic initializers of the game's globals, in the order of the __xc table */
static void Crt_InitGlobals(void) {
	const uint32_t *xi = Game_Ptr(CRT_XI_A), *xc = Game_Ptr(CRT_XC_A);
	int n = 0;

	for (; xi < (const uint32_t *)Game_Ptr(CRT_XI_Z); xi++) {
		if (*xi && (*xi - 0x400000 < ORIG_CRT_CODE_START || *xi - 0x400000 >= ORIG_CRT_CODE_END)) {
			Com_Printf("crt: the __xi initializer at rva %06x is not the CRT's\n", (unsigned)(*xi - 0x400000));
		}
	}
	for (; xc < (const uint32_t *)Game_Ptr(CRT_XC_Z); xc++) {
		uint32_t rva = *xc - 0x400000;
		if (!*xc || (rva >= ORIG_CRT_CODE_START && rva < ORIG_CRT_CODE_END)) {
			continue;
		}
		((void (*)(void))Game_Ptr(rva))();
		n++;
	}
	Com_Printf("crt: %d initializers of the game run\n", n);
}

/* __scrt_get_show_window_mode (rva 0x314d21) */
static int Crt_ShowMode(void) {
	STARTUPINFOW si;

	GetStartupInfoW(&si);
	return si.dwFlags & STARTF_USESHOWWINDOW ? si.wShowWindow : SW_SHOWDEFAULT;
}

/* __scrt_common_main_seh without the CRT: returns only through exit */
[[noreturn]] void Crt_Main(void) {
	Crt_InitState();
	Crt_InitGlobals();
	crt_exit(App_WinMain(Game_Ptr(0), NULL, _get_narrow_winmain_command_line(), Crt_ShowMode()));
}
