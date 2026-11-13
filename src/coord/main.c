/*
 * ScrapCoord: the dedicated coordinator of a multiplayer world (include/scrap/coord.h). Not in the original.
 *
 *   ScrapCoord [-port:28100] [-bind:<IPv4>] [-name:<world>] [-data:<folder>] [-gamedir:<game folder>] [-exe:<ScrapC>]
 *              [-levels:<comma list>] [-keep:<comma list>] [-start:<level>] [-maxplayers:64] [-pvp:1] [-money:2000]
 *              [-grace:<s>] [-instargs:"<more instance arguments>"] [-stopinstances]
 *
 * Lines on the standard input: status, save, kick <name or key>, start <level> (keep it up), stop <level>, quit.
 * SIGINT and SIGTERM end it like quit: the players are saved; the instances keep running for the next coordinator
 * (which takes them over) unless -stopinstances.
 */
#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/coord.h"

#define MAIN_WAIT_MS 5				/* the network is read this often */

static volatile int main_quit;

#ifndef _WIN32
static void Main_Signal(int sig) {
	(void)sig;
	main_quit = 1;
}
#endif

static uint64_t Main_Ms(void) {
#ifdef _WIN32
	return GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

/* a line of the standard input when one is complete, waiting up to ms for it: 1 with a line, -1 at its end */
static int Main_Line(char *line, int cap, int ms) {
	static char buf[512];
	static int len, eof;
	for (;;) {
		char *nl = memchr(buf, '\n', (size_t)len);
		if (nl) {
			int n = (int)(nl - buf);
			int k = n < cap - 1 ? n : cap - 1;
			memcpy(line, buf, (size_t)k);
			line[k] = 0;
			if (k && line[k - 1] == '\r') {
				line[k - 1] = 0;
			}
			memmove(buf, nl + 1, (size_t)(len - n - 1));
			len -= n + 1;
			return 1;
		}
		if (eof) {
			return -1;
		}
		if (len == (int)sizeof(buf)) {
			len = 0;
		}
#ifdef _WIN32
		HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
		DWORD got = 0, avail = 0;
		if (GetFileType(in) == FILE_TYPE_PIPE) {
			if (!PeekNamedPipe(in, NULL, 0, NULL, &avail, NULL)) {
				eof = 1;
				continue;
			}
			if (!avail) {
				Sleep((DWORD)ms);
				return 0;
			}
		} else if (WaitForSingleObject(in, (DWORD)ms) != WAIT_OBJECT_0) {
			return 0;
		}
		if (!ReadFile(in, buf + len, (DWORD)(sizeof(buf) - (size_t)len), &got, NULL) || !got) {
			eof = 1;
			continue;
		}
		len += (int)got;
#else
		struct pollfd p = { 0, POLLIN, 0 };
		if (poll(&p, 1, ms) <= 0) {
			return 0;
		}
		ssize_t got = read(0, buf + len, sizeof(buf) - (size_t)len);
		if (got <= 0) {
			eof = 1;
			continue;
		}
		len += (int)got;
#endif
		ms = 0;
	}
}

/* -name:value */
static const char *Main_Arg(const char *a, const char *name) {
	size_t n = strlen(name);
	return a[0] == '-' && !strncmp(a + 1, name, n) && a[n + 1] == ':' ? a + n + 2 : NULL;
}

int main(int argc, char **argv) {
	CoordConfig cfg;
	char line[256];
	const char *v;

	Coord_DefaultConfig(&cfg);
	cfg.dedicated = 1;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if ((v = Main_Arg(a, "port"))) {
			cfg.port = (uint16_t)atoi(v);
		} else if ((v = Main_Arg(a, "instport"))) {
			cfg.instPort = (uint16_t)atoi(v);
		} else if ((v = Main_Arg(a, "bind"))) {
			snprintf(cfg.bind, sizeof(cfg.bind), "%s", v);
		} else if ((v = Main_Arg(a, "name"))) {
			snprintf(cfg.name, sizeof(cfg.name), "%s", v);
		} else if ((v = Main_Arg(a, "data"))) {
			snprintf(cfg.dataDir, sizeof(cfg.dataDir), "%s", v);
		} else if ((v = Main_Arg(a, "gamedir"))) {
			snprintf(cfg.gameDir, sizeof(cfg.gameDir), "%s", v);
		} else if ((v = Main_Arg(a, "exe"))) {
			snprintf(cfg.exe, sizeof(cfg.exe), "%s", v);
		} else if ((v = Main_Arg(a, "levels"))) {
			snprintf(cfg.levels, sizeof(cfg.levels), "%s", v);
		} else if ((v = Main_Arg(a, "keep"))) {
			snprintf(cfg.keep, sizeof(cfg.keep), "%s", v);
		} else if ((v = Main_Arg(a, "start"))) {
			snprintf(cfg.start, sizeof(cfg.start), "%s", v);
		} else if ((v = Main_Arg(a, "maxplayers"))) {
			cfg.maxPlayers = atoi(v);
		} else if ((v = Main_Arg(a, "pvp"))) {
			cfg.pvp = atoi(v) != 0;
		} else if ((v = Main_Arg(a, "money"))) {
			cfg.money = atoi(v);
		} else if ((v = Main_Arg(a, "grace"))) {
			cfg.grace = atoi(v);
		} else if ((v = Main_Arg(a, "instargs"))) {
			snprintf(cfg.instArgs, sizeof(cfg.instArgs), "%s", v);
		} else if (!strcmp(a, "-stopinstances")) {
			cfg.stopInstances = 1;
		} else {
			fprintf(stderr, "ScrapCoord: unknown argument %s\n", a);
			return 2;
		}
	}
	if (cfg.maxPlayers < 1) {
		cfg.maxPlayers = 1;
	}
#ifndef _WIN32
	signal(SIGINT, Main_Signal);
	signal(SIGTERM, Main_Signal);
	signal(SIGPIPE, SIG_IGN);
#endif
	Coord *c = Coord_Create(&cfg);
	if (!c) {
		return 1;
	}
	while (!main_quit) {
		Coord_Pass(c, Main_Ms());
		int r = Main_Line(line, sizeof(line), MAIN_WAIT_MS);
		if (r > 0) {
			if (!strcmp(line, "quit") || !strcmp(line, "exit")) {
				break;
			}
			if (line[0] && !Coord_Command(c, line)) {
				printf("coordinator: unknown command '%s' (status, save, kick, start, stop, quit)\n", line);
				fflush(stdout);
			}
		} else if (r < 0) {
#ifdef _WIN32
			Sleep(MAIN_WAIT_MS);
#else
			poll(NULL, 0, MAIN_WAIT_MS);
#endif
		}
	}
	Coord_Destroy(c);
	return 0;
}
