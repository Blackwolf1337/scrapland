/*
 * ScrapC.exe, the standalone exe of docs/standalone.md: a normal exe of the C code of src/ and real libraries, with no
 * byte of Scrap.exe in it. It needs only the game files, not the original exe (stage 3c2). Its data are the C objects
 * of src/game/Global (stage 3b), placed by the linker like any other global, its resources the ones of
 * src/standalone/scrap.rc (stage 3b3). The DLLs of the original are imports of this exe (stage 3c1a, GAME_IMPORT of
 * game.h), which the loader binds before the start, as it did for the original; only dinput8 is loaded here, from the
 * system folder. It has the default base and dynamic base (ASLR), nothing depends on the base 0x400000 of the
 * original.
 *
 * Stage 2a: the Python is the real library of src/python, linked into this exe. Stage 2b: the CRT functions are the
 * UCRT of this exe or C (src/standalone/crt.c and crtinit.c), the MSVC math is Microsoft's ucrtbase (crtmath.c,
 * crtvec.c). The process starts in Crt_Main of crtinit.c.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "scrap/game.h"
#include "scrap/win32.h"

int Game_Dir(char *dir, size_t size);
int Game_Start(uintptr_t base, const char *dir);
[[noreturn]] void Crt_Main(void);
const char *Crt_MathInit(const char *dir);
void Log_Open(const char *dir);

static void Fatal(const char *fmt, ...) __attribute__((format(printf, 1, 2), noreturn));

static void Fatal(const char *fmt, ...) {
	char buf[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Com_Printf("%s\n", buf);
	MessageBoxA(NULL, buf, "ScrapC", MB_OK | MB_ICONERROR);
	ExitProcess(1);
}

dinput8create_t sd_DirectInput8Create;

/*
 * DirectInput8Create of the dinput8 in the system folder: the dinput8.dll in game/Bin is the hook DLL, which would
 * write the log again. The original imported it, so it is loaded before anything of the game runs.
 */
static void Orig_DInput8(void) {
	char sys[MAX_PATH], path[MAX_PATH];

	UINT n = GetSystemDirectoryA(sys, sizeof(sys));
	if (n == 0 || n >= sizeof(sys) || snprintf(path, sizeof(path), "%s\\dinput8.dll", sys) >= (int)sizeof(path)) {
		Fatal("no path of the system dinput8.dll");
	}
	HMODULE m = LoadLibraryA(path);
	if (!m) {
		Fatal("cannot load %s (%lu)", path, GetLastError());
	}
	sd_DirectInput8Create = (dinput8create_t)(void *)GetProcAddress(m, "DirectInput8Create");
	if (!sd_DirectInput8Create) {
		Fatal("%s: no DirectInput8Create", path);
	}
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
	char dir[MAX_PATH];

	(void)prev, (void)cmd, (void)show;
	if (!Game_Dir(dir, sizeof(dir))) {
		return 1;
	}
	Log_Open(dir);
	Orig_DInput8();
	const char *missing = Crt_MathInit(dir);
	if (missing) {
		Fatal("the MSVC math: cannot load %s", missing);
	}
	Game_Start((uintptr_t)inst, dir);
	Crt_Main();
}
