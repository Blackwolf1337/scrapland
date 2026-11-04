/*
 * The compiler support of the MSVC runtime in ScrapC.exe (stage 2b of docs/standalone.md): the start of the process
 * instead of the entry point of the original, atexit and exit, the guards of the thread-safe function statics and
 * the vector constructor and destructor iterators. The functions are SD_IMPORT_CRT in src/imports.inc, so the
 * entries of the original jump here too (src/core/hook.c).
 *
 * The entry of the original (: __security_init_cookie, then __scrt_common_main_seh at 0x31478c) runs
 * the C initializers of the __xi table with _initterm_e, the C++ initializers of the __xc table with _initterm and
 * then WinMain; exit() with its result runs the atexit functions, last registered first. Every __xi entry and the
 * first __xc one belong to the CRT: what they set that the game can see is set here (Crt_InitState). The other
 * __xc entries are the dynamic initializers of the game's globals, which Crt_Main calls from the C table of
 * crtxc.c in the same order.
 */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <corecrt_startup.h>
#endif

#include "scrap/app.h"
#include "scrap/crt.h"
#include "scrap/game.h"

/* the matherr of the game, which __scrt_common_main_seh registers with __setusermatherr */
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
	Com_Printf("crt: exit(%d), %d atexit functions\n", code, crt_onexit_count);
	Crt_RunOnexit();
	Com_Printf("crt: the atexit functions ran\n");
	exit(code);
}

/* _exit: no atexit functions, no flush */
[[noreturn]] void crt_exit_now(int code) {
	_exit(code);
}

/* ---- thread-safe statics (_Init_thread_header and _Init_thread_footer) ---- */

/*
 * The original keeps the epoch per thread in its TLS block (+0x20) and the global one both start at
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
		((void(*)(void *))ctor)(p);
		p = (byte *)p + size;
	}
}

/* last object first */
void __stdcall crt_eh_vector_dtor(void *p, uint32_t size, uint32_t count, void *dtor) {
	for (byte *q = (byte *)p + size * count; count; count--) {
		q -= size;
		((void(*)(void *))dtor)(q);
	}
}

/* ---- the start ---- */

/* what the CRT initializers of the __xi table set that the game sees (the matherr hook exists on Windows only) */
static void Crt_InitState(void) {
#ifdef _WIN32
	__setusermatherr((int (*)(struct _exception *))E3D_MathErr);
#endif
}

/* the dynamic initializers of the game's globals, in the order of the __xc table of the original (crtxc.c) */
extern void (*const crt_xc[])(void);
extern const int crt_xc_count;

static void Crt_InitGlobals(void) {
	for (int i = 0; i < crt_xc_count; i++) {
		crt_xc[i]();
	}
	Com_Printf("crt: %d initializers of the game run\n", crt_xc_count);
}

/* __scrt_common_main_seh without the CRT, with the command line after the program name: returns only through exit */
[[noreturn]] void Crt_Main(char *cmdLine) {
	Crt_InitState();
	Crt_InitGlobals();
	crt_exit(App_WinMain(NULL, NULL, cmdLine, 10));	/* SW_SHOWDEFAULT, the window is SDL's */
}
