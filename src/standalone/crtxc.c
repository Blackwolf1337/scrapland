/*
 * The dynamic initializers of the game's globals for Crt_Main (crtinit.c), in the order of the __xc table of the
 * original (docs/standalone.md, stages 2d and 3c1b). crtxc.inc is written by tools/standalone/xctable.py.
 *
 * Only shared.h, as in src/core/hook.c: the C files declare the initializers with their own prototypes, here only
 * the addresses are needed. All of them are cdecl void(void).
 */
#include "scrap/shared.h"

#define CRT_XC(name, rva) extern void name(void);
#include "crtxc.inc"
#undef CRT_XC

void (*const crt_xc[])(void) = {
#define CRT_XC(name, rva) name,
#include "crtxc.inc"
#undef CRT_XC
};

const int crt_xc_count = (int)(sizeof(crt_xc) / sizeof(crt_xc[0]));
