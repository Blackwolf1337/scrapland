/*
 * ScrapC, the standalone game: a normal program of the C code of src/ and real libraries, with no byte of Scrap.exe in
 * it, only the game files. Windows (mingw-w64) and Linux start here; the process goes on in Crt_Main of crtinit.c
 * with the command line after the program name, as WinMain had it.
 */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "scrap/game.h"

int Game_Dir(char *dir, size_t size);
int Game_Start(const char *dir);
[[noreturn]] void Crt_Main(char *cmdLine);
void Log_Open(const char *dir);

static int Main_Start(char *cmdLine) {
	char dir[1024];

	if (!Game_Dir(dir, sizeof(dir))) {
		return 1;
	}
	Log_Open(dir);
	Game_Start(dir);
	Crt_Main(cmdLine);
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
	(void)inst, (void)prev, (void)show;
	return Main_Start(cmd);
}
#else
/* the arguments joined with spaces, the ones with a space quoted: the command line the game parses */
int main(int argc, char **argv) {
	static char cmd[4096];
	size_t n = 0;

	for (int i = 1; i < argc; i++) {
		byte quote = strchr(argv[i], ' ') != NULL;
		int w = snprintf(cmd + n, sizeof(cmd) - n, "%s%s%s%s", i > 1 ? " " : "", quote ? "\"" : "", argv[i],
			quote ? "\"" : "");
		if (w < 0 || (size_t)w >= sizeof(cmd) - n) {
			break;
		}
		n += (size_t)w;
	}
	return Main_Start(cmd);
}
#endif
