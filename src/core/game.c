/*
 * Startup settings: scrapdecomp.ini next to the exe ([general] switches) and the test module. ScrapC calls
 * Game_Start from its main (src/standalone/main.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/game.h"
#include "scrap/platform.h"

uintptr_t game_base;

static char ini_path[1024];

int Game_Option(const char *key);
int Game_OptionOr(const char *key, int def);
void TestMod_Register(void);

int Game_Option(const char *key) {
	return Game_OptionOr(key, 0);
}

/* key=number of the [general] section of the ini (what GetPrivateProfileInt read), def without it */
int Game_OptionOr(const char *key, int def) {
	char line[256];
	int general = 0;
	size_t n = strlen(key);
	FILE *f = fopen(ini_path, "r");

	if (!f) {
		return def;
	}
	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t') {
			p++;
		}
		if (*p == '[') {
			general = !strncmp(p, "[general]", 9);
		} else if (general && !strncmp(p, key, n) && (p[n] == '=' || p[n] == ' ')) {
			p = strchr(p, '=');
			if (p) {
				def = atoi(p + 1);
			}
			break;
		}
	}
	fclose(f);
	return def;
}

/* the folder of the exe with its separator */
int Game_Dir(char *dir, size_t size) {
	Plat_ExePath(dir, (int)size);
	char *slash = strrchr(dir, '/');
	char *back = strrchr(dir, '\\');
	if (back > slash) {
		slash = back;
	}
	if (!slash) {
		return 0;
	}
	slash[1] = 0;
	return 1;
}

int Game_Start(uintptr_t base, const char *dir) {
	if (snprintf(ini_path, sizeof(ini_path), "%sscrapdecomp.ini", dir) >= (int)sizeof(ini_path)) {
		return 0;
	}
	game_base = base;
	Com_Printf("ScrapC at %08x\n", (unsigned)game_base);
	if (Game_Option("testmod")) {
		TestMod_Register();
	}
	return 1;
}
