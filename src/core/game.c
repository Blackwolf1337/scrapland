/*
 * Startup: checks that the image is the exe this code was made for, resolves the imports and installs the hooks.
 * Game_Init runs inside Scrap.exe from DllMain (so only kernel32 work, no LoadLibrary, no threads). ScrapC.exe calls
 * Game_Start from its WinMain (src/standalone/main.c) for the settings and the test module, it hooks nothing.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "scrap/game.h"

uintptr_t game_base;

static char ini_path[MAX_PATH];

int Imports_Resolve(uintptr_t base);
void Hook_InstallAll(int wipe);
void Log_Open(const char *dir);
int Hook_Enabled(const char *name);
int Game_Option(const char *key);
int Game_OptionOr(const char *key, int def);
void TestMod_Register(void);

int Hook_Enabled(const char *name) {
	return GetPrivateProfileIntA("hooks", name, 1, ini_path) != 0;
}

/* a switch of the [general] section, 0 when it is not there */
int Game_Option(const char *key) {
	return Game_OptionOr(key, 0);
}

/* the same with def when it is not there */
int Game_OptionOr(const char *key, int def) {
	return GetPrivateProfileIntA("general", key, def, ini_path);
}

/* the folder of the running exe with its trailing backslash */
int Game_Dir(char *dir, size_t size) {
	DWORD n = GetModuleFileNameA(NULL, dir, (DWORD)size);
	if (n == 0 || n >= size) {
		return 0;
	}
	char *slash = strrchr(dir, '\\');
	if (slash) {
		slash[1] = '\0';
	}
	return 1;
}

int Game_IsBuild(const IMAGE_NT_HEADERS32 *nt) {
	if (nt->FileHeader.TimeDateStamp != GAME_TIMESTAMP || nt->OptionalHeader.SizeOfImage != GAME_SIZEOFIMAGE) {
		Com_Printf("not Scrap.exe 1.6.1 (timestamp %08lx, size %08lx): no hooks\n",
			nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
		return 0;
	}
	return 1;
}

#ifdef SD_STANDALONE
/*
 * ScrapC.exe (src/standalone/main.c): base is its own module. There is no exe to hook (stage 3c2), so hooks and wipe of
 * scrapdecomp.ini do nothing here; testmod=1 still adds SDTest, whose names are the C functions and objects
 * (src/core/testmod.c).
 */
int Game_Start(uintptr_t base, const char *dir) {
	if (snprintf(ini_path, sizeof(ini_path), "%sscrapdecomp.ini", dir) >= (int)sizeof(ini_path)) {
		return 0;
	}
	game_base = base;
	Com_Printf("ScrapC.exe at %08x\n", (unsigned)game_base);
	if (Game_Option("wipe")) {
		Com_Printf("wipe: only in the hook DLL, ScrapC.exe has no code of the original\n");
	}
	if (Game_Option("testmod")) {
		TestMod_Register();
	}
	return 1;
}
#else
int Game_Start(uintptr_t base, const char *dir) {
	if (snprintf(ini_path, sizeof(ini_path), "%sscrapdecomp.ini", dir) >= (int)sizeof(ini_path)) {
		return 0;
	}
	int hooks = GetPrivateProfileIntA("general", "hooks", 1, ini_path);
	int testmod = GetPrivateProfileIntA("general", "testmod", 0, ini_path);
	if (!hooks) {
		Com_Printf("hooks switched off in scrapdecomp.ini\n");
		if (!testmod) {
			return 0;
		}
	}
	game_base = base;
	Com_Printf("Scrap.exe 1.6.1 at %08x, %d imports\n", (unsigned)game_base, Imports_Resolve(game_base));
	if (testmod) {
		TestMod_Register();
	}
	if (hooks) {
		Hook_InstallAll(GetPrivateProfileIntA("general", "wipe", 0, ini_path));
	}
	return hooks;
}
#endif

int Game_Init(void) {
	char dir[MAX_PATH];

	if (!Game_Dir(dir, sizeof(dir))) {
		return 0;
	}
	Log_Open(dir);
	HMODULE exe = GetModuleHandleA(NULL);
	const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)exe;
	if (!Game_IsBuild((const IMAGE_NT_HEADERS32 *)((const byte *)exe + dos->e_lfanew))) {
		return 0;
	}
	return Game_Start((uintptr_t)exe, dir);
}
