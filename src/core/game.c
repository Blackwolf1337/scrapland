/*
 * Startup inside Scrap.exe: checks that the process is the exe this code was made for, resolves the imports and
 * installs the hooks. Called from DllMain, so only kernel32 work here (no LoadLibrary, no threads).
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "scrap/game.h"

uintptr_t game_base;

static char ini_path[MAX_PATH];

int Imports_Resolve(uintptr_t base);
void Hook_InstallAll(void);
void Log_Open(const char *dir);
int Hook_Enabled(const char *name);

int Hook_Enabled(const char *name) {
	return GetPrivateProfileIntA("hooks", name, 1, ini_path) != 0;
}

int Game_Init(void) {
	HMODULE exe = GetModuleHandleA(NULL);
	char dir[MAX_PATH];

	DWORD n = GetModuleFileNameA(exe, dir, sizeof(dir));
	if (n == 0 || n >= sizeof(dir)) {
		return 0;
	}
	char *slash = strrchr(dir, '\\');
	if (slash) {
		slash[1] = '\0';
	}
	if (snprintf(ini_path, sizeof(ini_path), "%sscrapdecomp.ini", dir) >= (int)sizeof(ini_path)) {
		return 0;
	}
	Log_Open(dir);

	const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)exe;
	const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)((const byte *)exe + dos->e_lfanew);
	if (nt->FileHeader.TimeDateStamp != GAME_TIMESTAMP || nt->OptionalHeader.SizeOfImage != GAME_SIZEOFIMAGE) {
		Com_Printf("not Scrap.exe 1.6.1 (timestamp %08lx, size %08lx): no hooks\n",
			nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
		return 0;
	}
	if (!GetPrivateProfileIntA("general", "hooks", 1, ini_path)) {
		Com_Printf("hooks switched off in scrapdecomp.ini\n");
		return 0;
	}
	game_base = (uintptr_t)exe;
	Com_Printf("Scrap.exe 1.6.1 at %08x, %d imports\n", (unsigned)game_base, Imports_Resolve(game_base));
	Hook_InstallAll();
	return 1;
}
