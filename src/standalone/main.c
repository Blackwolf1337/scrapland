/*
 * ScrapC.exe, the standalone exe of docs/standalone.md: a normal exe of the C code of src/ and real libraries, with no
 * byte of Scrap.exe in it. It needs only the game files, not the original exe (stage 3c2). Its data are the C objects
 * of src/game/Global (stage 3b), placed by the linker like any other global, its resources the ones of
 * src/standalone/scrap.rc (stage 3b3). The DLLs of the original are imports of this exe (stage 3c1a, GAME_IMPORT of
 * game.h), which the loader binds before the start, as it did for the original. It has the default base and dynamic base (ASLR), nothing depends on the base 0x400000 of the
 * original.
 *
 * Stage 2a: the Python is the real library of src/python, linked into this exe. Stage 2b: the CRT functions are the
 * UCRT of this exe or C (src/standalone/crt.c and crtinit.c), the MSVC math is Microsoft's ucrtbase (crtmath.c,
 * crtvec.c). The process starts in Crt_Main of crtinit.c.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "scrap/game.h"
#include "scrap/win32.h"

int Game_Dir(char *dir, size_t size);
int Game_Start(uintptr_t base, const char *dir);
[[noreturn]] void Crt_Main(void);
void Log_Open(const char *dir);

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
	char dir[MAX_PATH];

	(void)prev, (void)cmd, (void)show;
	if (!Game_Dir(dir, sizeof(dir))) {
		return 1;
	}
	Log_Open(dir);
	Game_Start((uintptr_t)inst, dir);
	Crt_Main();
}
