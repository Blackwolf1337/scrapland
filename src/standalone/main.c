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
#ifdef __linux__
#include <sys/personality.h>
#include <unistd.h>
#endif

#include "scrap/cmdquote.h"
#include "scrap/game.h"
#include "scrap/testdet.h"

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
/* the arguments joined with spaces, quoted for the game's parser (include/scrap/cmdquote.h): the command line the
 * game parses */
int main(int argc, char **argv) {
	static char cmd[4096];
	size_t n = 0;

#ifdef __linux__
	/* the deterministic test mode: the same addresses in every run (they show in the reprs of the script objects, and
	 * whatever is ordered by address follows them), so the program starts itself again without the random layout of
	 * its address space. Linux only */
	if (TestDet_On()) {
		char exe[1024];
		int p = personality(0xffffffff);
		ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);

		/* by its path, so the process keeps its name */
		if (len > 0 && p != -1 && !(p & ADDR_NO_RANDOMIZE) && personality(p | ADDR_NO_RANDOMIZE) != -1) {
			exe[len] = 0;
			execv(exe, argv);
		}
	}
#endif
	for (int i = 1; i < argc; i++) {
		if (i > 1 && n + 1 < sizeof(cmd)) {
			cmd[n++] = ' ';
		}
		int w = CmdQuote_Arg(argv[i], cmd + n, (int)(sizeof(cmd) - n));
		if (w < 0) {
			cmd[n] = 0;
			break;
		}
		n += (size_t)w;
	}
	return Main_Start(cmd);
}
#endif
