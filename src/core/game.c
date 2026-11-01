/*
 * Startup settings: scrapdecomp.ini next to the exe ([general] switches) and the test module. ScrapC.exe calls
 * Game_Start from its WinMain (src/standalone/main.c).
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "scrap/game.h"

uintptr_t game_base;

static char ini_path[MAX_PATH];

int Game_Option(const char *key);
int Game_OptionOr(const char *key, int def);
void TestMod_Register(void);

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

/* base is the module of the exe, for the test module; testmod=1 adds SDTest (src/core/testmod.c) */
int Game_Start(uintptr_t base, const char *dir) {
	if (snprintf(ini_path, sizeof(ini_path), "%sscrapdecomp.ini", dir) >= (int)sizeof(ini_path)) {
		return 0;
	}
	game_base = base;
	Com_Printf("ScrapC.exe at %08x\n", (unsigned)game_base);
	if (Game_Option("testmod")) {
		TestMod_Register();
	}
	return 1;
}
