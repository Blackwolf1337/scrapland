/*
 * ScrapC.exe, the standalone exe of docs/standalone.md. Its room at rva 0x1000..0x55e000 holds the data of Scrap.exe
 * 1.6.1 at their rvas, built in by tools/standalone/datagen.py (stage 3a), so the original exe is not read at run
 * time. The code of the original is not built in, but for the pieces that are still read or run (room.h): the
 * rest of its range is int3 at start, the hooks of the C code (Game_Start, as the hook DLL does) jump over it.
 *
 * This exe has the base 0x400000 of the original, so the rvas of GAME_DATA stay valid, no relocation. Under Wine the
 * low memory is full of mappings when the exe is elsewhere, so the room must be part of the exe. The headers stay
 * the exe's own, only the resource directory points to the one of the original (the game loads its dialogs and
 * icons from its instance, which is this module). The import table of the original is filled with GetProcAddress.
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

/* the room writable and executable (the import table, the hooks), the code that is not built in int3 */
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
	IMAGE_NT_HEADERS32 *own = (IMAGE_NT_HEADERS32 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
	own->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress = ROOM_RESOURCE_RVA;
	own->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = ROOM_RESOURCE_SIZE;
}

/* dinput8 from the system folder: the dinput8.dll in game/Bin is the hook DLL, which would write the log again */
static HMODULE Orig_LoadDll(const char *dll) {
	char sys[MAX_PATH], path[MAX_PATH];

	if (_stricmp(dll, "dinput8.dll")) {
		return LoadLibraryA(dll);
	}
	UINT n = GetSystemDirectoryA(sys, sizeof(sys));
	if (n == 0 || n >= sizeof(sys) || snprintf(path, sizeof(path), "%s\\%s", sys, dll) >= (int)sizeof(path)) {
		return NULL;
	}
	return LoadLibraryA(path);
}

static void Orig_Imports(byte *base) {
	for (const IMAGE_IMPORT_DESCRIPTOR *imp = (const void *)(base + ROOM_IMPORT_RVA); imp->Name; imp++) {
		const char *dll = (const char *)base + imp->Name;
		HMODULE m = Orig_LoadDll(dll);
		if (!m) {
			Fatal("cannot load %s (%lu)", dll, GetLastError());
		}
		const IMAGE_THUNK_DATA32 *names = (const void *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk
			: imp->FirstThunk));
		IMAGE_THUNK_DATA32 *iat = (void *)(base + imp->FirstThunk);
		for (; names->u1.AddressOfData; names++, iat++) {
			int ord = IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal);
			const char *what = ord ? (const char *)(uintptr_t)IMAGE_ORDINAL32(names->u1.Ordinal)
				: (const char *)((const IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name;
			FARPROC fn = GetProcAddress(m, what);
			if (!fn) {
				if (ord) {
					Fatal("%s: no ordinal %u", dll, (unsigned)IMAGE_ORDINAL32(names->u1.Ordinal));
				}
				Fatal("%s: no %s", dll, what);
			}
			iat->u1.Function = (DWORD)(uintptr_t)fn;
		}
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
	Orig_Imports(base);
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
