/*
 * ScrapC.exe, the standalone exe of docs/standalone.md. Its room at rva 0x1000..0x55e000 holds the data of Scrap.exe
 * 1.6.1 at their rvas, built in by tools/standalone/datagen.py (stage 3a), so the original exe is not read at run
 * time. The code of the original is not built in, but for the pieces that are still read or run (room.h): the
 * rest of its range is int3 at start, the hooks of the C code (Game_Start, as the hook DLL does) jump over it.
 *
 * This exe has the base 0x400000 of the original, so the rvas of GAME_DATA stay valid, no relocation. Under Wine the
 * low memory is full of mappings when the exe is elsewhere, so the room must be part of the exe. The headers stay
 * the exe's own. The resources (the game loads its dialogs, icons and menus from its instance, which is this module)
 * are the exe's own too, src/standalone/scrap.rc of stage 3b3. The DLLs of the original are imports of this exe
 * (stage 3c1a, GAME_IMPORT of game.h), which the loader binds before the start, as it did for the original; only
 * dinput8 is loaded here, from the system folder.
 *
 * Stage 2a: the Python is the real library of src/python, linked into this exe; datagen.py turns the pointers of
 * the room into the original's Python into the real symbols. Stage 2b: the CRT functions are the UCRT of this exe or
 * C (src/standalone/crt.c and crtinit.c), the MSVC math is Microsoft's ucrtbase (crtmath.c, crtvec.c); the entries
 * of the original's CRT jump there. The process starts in Crt_Main of crtinit.c. With [general] dataprobe the probe
 * of dataprobe.c watches the data of the original from there on (stage 3b2).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "scrap/game.h"
#include "scrap/win32.h"
#include "room.h"

int Game_Dir(char *dir, size_t size);
int Game_Start(uintptr_t base, const char *dir);
[[noreturn]] void Crt_Main(void);
const char *Crt_MathInit(const char *dir);
int Game_Option(const char *key);
void Log_Open(const char *dir);
void DataProbe_Start(const char *dir);

/* the start of the room, rva 0x1000 (room.c) */
extern byte orig_room[];

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

/* the room writable and executable (the hooks), the code that is not built in int3 */
static void Orig_Room(byte *base) {
	static const struct {
		uint32_t lo, hi;
	} int3[] = ROOM_INT3;
	DWORD old;

	if (orig_room != base + 0x1000) {
		Fatal("the room for Scrap.exe is at %p, not at rva 0x1000", (void *)orig_room);
	}
	if (!VirtualProtect(base, GAME_SIZEOFIMAGE, PAGE_EXECUTE_READWRITE, &old)) {
		Fatal("VirtualProtect of the room failed (%lu)", GetLastError());
	}
	for (size_t i = 0; i < sizeof(int3) / sizeof(int3[0]); i++) {
		memset(base + int3[i].lo, 0xcc, int3[i].hi - int3[i].lo);
	}
	FlushInstructionCache(GetCurrentProcess(), base, GAME_SIZEOFIMAGE);
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
	byte *base = (byte *)inst;

	(void)prev, (void)cmd, (void)show;
	if (!Game_Dir(dir, sizeof(dir))) {
		return 1;
	}
	Log_Open(dir);
	Orig_Room(base);
	Orig_DInput8();
	game_base = (uintptr_t)base;
	const char *missing = Crt_MathInit(dir);
	if (missing) {
		Fatal("the MSVC math: cannot load %s", missing);
	}
	Game_Start((uintptr_t)base, dir);
	if (Game_Option("wipe") >= 3) {
		Com_Printf("probe: ScrapC.exe has no code of the original to run, wipe=3 stops at the first trap\n");
	}
	DataProbe_Start(dir);
	Crt_Main();
}
