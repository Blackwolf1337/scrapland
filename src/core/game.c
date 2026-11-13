/*
 * Startup settings: scrapdecomp.ini next to the exe ([general] switches). ScrapC calls Game_Start from its main
 * (src/standalone/main.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/game.h"
#include "scrap/platform.h"

static char ini_path[1024];

int Game_Option(const char *key) {
	return Game_OptionOr(key, 0);
}

int Game_OptionStr(const char *key, char *out, int size) {
	char line[256];
	int general = 0, found = 0;
	size_t n = strlen(key);
	FILE *f = fopen(ini_path, "r");

	out[0] = 0;
	if (!f) {
		return 0;
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
				p++;
				while (*p == ' ' || *p == '\t') {
					p++;
				}
				size_t len = strcspn(p, "\r\n");
				while (len && (p[len - 1] == ' ' || p[len - 1] == '\t')) {
					len--;
				}
				snprintf(out, (size_t)size, "%.*s", (int)len, p);
				found = 1;
			}
			break;
		}
	}
	fclose(f);
	return found;
}

/* key=number of the [general] section of the ini (what GetPrivateProfileInt read), def without it */
int Game_OptionOr(const char *key, int def) {
	char value[64];
	return Game_OptionStr(key, value, sizeof(value)) ? atoi(value) : def;
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

int Game_Start(const char *dir) {
	if (snprintf(ini_path, sizeof(ini_path), "%sscrapdecomp.ini", dir) >= (int)sizeof(ini_path)) {
		return 0;
	}
	return 1;
}
