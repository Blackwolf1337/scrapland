/*
 * ScrapC.exe, stage 1 of docs/standalone.md: copies the sections of the user's Scrap.exe 1.6.1 (next to this exe
 * in game/Bin) to their rvas in this module, fills their import table, gives them their TLS block, installs the
 * hooks of the C code (Game_Start, as the hook DLL does) and runs the entry point of the original. The C code
 * replaces the game and engine; the libraries linked into the original (Python 1.5.2, the MSVC CRT ...) still run
 * from the copy.
 *
 * This exe has the base 0x400000 of the original, so its code needs no relocation. cmake/ScrapC.ld puts the
 * uninitialized section .orig (orig_room below) at rva 0x1000, as large as the original image; the own code
 * follows it. Under Wine the low memory is full of mappings when the exe is elsewhere, so the room must be part of
 * the exe. The headers stay the exe's own, only the resource directory points to the one of the original (the
 * game loads its dialogs and icons from its instance, which is this module).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "scrap/game.h"

int Game_Dir(char *dir, size_t size);
int Game_IsBuild(const IMAGE_NT_HEADERS32 *nt);
int Game_Start(uintptr_t base, const char *dir);
void Log_Open(const char *dir);

#define ORIG_STR(x) ORIG_STR2(x)
#define ORIG_STR2(x) #x
__asm__(".section .orig,\"bw\"\n\t.globl _orig_room\n_orig_room:\n\t.space " ORIG_STR(GAME_SIZEOFIMAGE) " - 0x1000\n\t.text");
extern byte orig_room[];

/* mingw's TLS index of this exe; its TLS block is unused (gcc's __thread is emutls on mingw), so it is given to the
 * original */
extern ULONG _tls_index;

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

static byte *Orig_Read(const char *path, DWORD *size) {
	HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		Fatal("cannot open %s (%lu)", path, GetLastError());
	}
	*size = GetFileSize(f, NULL);
	byte *buf = VirtualAlloc(NULL, *size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	DWORD got;
	if (!buf || !ReadFile(f, buf, *size, &got, NULL) || got != *size) {
		Fatal("cannot read %s (%lu)", path, GetLastError());
	}
	CloseHandle(f);
	return buf;
}

static const IMAGE_NT_HEADERS32 *Orig_Headers(const byte *file, DWORD size) {
	const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)file;
	if (size < sizeof(*dos) || dos->e_magic != IMAGE_DOS_SIGNATURE
		|| (DWORD)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS32) > size) {
		Fatal("Scrap.exe: no PE file");
	}
	const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(file + dos->e_lfanew);
	if (!Game_IsBuild(nt)) {
		Fatal("Scrap.exe is not version 1.6.1 (see scrapdecomp.log)");
	}
	return nt;
}

static void Orig_Copy(byte *base, const byte *file, DWORD size, const IMAGE_NT_HEADERS32 *nt) {
	DWORD old;

	if (orig_room != base + 0x1000) {
		Fatal("the room for Scrap.exe is at %p, not at rva 0x1000", (void *)orig_room);
	}
	if (!VirtualProtect(base, GAME_SIZEOFIMAGE, PAGE_EXECUTE_READWRITE, &old)) {
		Fatal("VirtualProtect of the room failed (%lu)", GetLastError());
	}
	const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
	for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
		DWORD n = sec->SizeOfRawData < sec->Misc.VirtualSize ? sec->SizeOfRawData : sec->Misc.VirtualSize;
		if (sec->PointerToRawData + n > size || sec->VirtualAddress < 0x1000 || sec->VirtualAddress + n > GAME_SIZEOFIMAGE) {
			Fatal("Scrap.exe: section %.8s outside the file", sec->Name);
		}
		memcpy(base + sec->VirtualAddress, file + sec->PointerToRawData, n);
	}
	IMAGE_NT_HEADERS32 *own = (IMAGE_NT_HEADERS32 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
	own->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE] =
		nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
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

static void Orig_Imports(byte *base, const IMAGE_NT_HEADERS32 *nt) {
	const IMAGE_DATA_DIRECTORY *d = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	for (const IMAGE_IMPORT_DESCRIPTOR *imp = (const void *)(base + d->VirtualAddress); imp->Name; imp++) {
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

/* the TLS block of the original for this thread (the only one that runs its code), a copy of its template */
static void Orig_Tls(const IMAGE_NT_HEADERS32 *nt) {
	const IMAGE_DATA_DIRECTORY *d = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
	if (!d->VirtualAddress) {
		return;
	}
	const IMAGE_TLS_DIRECTORY32 *tls = Game_Ptr(d->VirtualAddress);
	DWORD n = tls->EndAddressOfRawData - tls->StartAddressOfRawData;
	byte *block = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n + tls->SizeOfZeroFill);
	if (!block) {
		Fatal("no memory for the TLS block");
	}
	memcpy(block, (const void *)(uintptr_t)tls->StartAddressOfRawData, n);
	void **slots;
	__asm__("movl %%fs:0x2c, %0" : "=r"(slots));
	if (!slots) {
		Fatal("this exe has no TLS array");
	}
	slots[_tls_index] = block;
	*(ULONG *)(uintptr_t)tls->AddressOfIndex = _tls_index;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
	char dir[MAX_PATH], path[MAX_PATH];
	DWORD size;

	(void)prev, (void)cmd, (void)show;
	if (!Game_Dir(dir, sizeof(dir))) {
		return 1;
	}
	Log_Open(dir);
	if (snprintf(path, sizeof(path), "%sScrap.exe", dir) >= (int)sizeof(path)) {
		Fatal("path too long: %s", dir);
	}
	byte *base = (byte *)inst;
	byte *file = Orig_Read(path, &size);
	const IMAGE_NT_HEADERS32 *nt = Orig_Headers(file, size);
	Orig_Copy(base, file, size, nt);
	DWORD entry = nt->OptionalHeader.AddressOfEntryPoint;
	Orig_Imports(base, nt);
	game_base = (uintptr_t)base;
	Orig_Tls(nt);
	VirtualFree(file, 0, MEM_RELEASE);
	Game_Start((uintptr_t)base, dir);

	Com_Printf("standalone: entry of the original at rva %06lx\n", entry);
	((void (*)(void))(base + entry))();
	return 0;
}
