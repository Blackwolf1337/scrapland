/*
 * ScrapCoord: the dedicated coordinator of a multiplayer world (include/scrap/coord.h). Not in the original.
 *
 *   ScrapCoord [-port:28100] [-bind:<IPv4|IPv6>] [-name:<world>] [-data:<folder>] [-gamedir:<game folder>]
 *              [-exe:<ScrapC>] [-levels:<comma list>] [-keep:<comma list>] [-start:<level>] [-maxplayers:64] [-pvp:1]
 *              [-money:2000] [-grace:<s>] [-instargs:"<more instance arguments>"] [-stopinstances]
 *              [-mode:sandbox|freeroam] [-list:<list server address> [-listport:28090] [-listpublic:<address>]]
 *
 * Lines on the standard input: status, save, kick <name or key>, start <level> (keep it up), stop <level>, quit.
 * SIGINT and SIGTERM end it like quit: the players are saved; the instances keep running for the next coordinator
 * (which takes them over) unless -stopinstances.
 */
#ifndef _WIN32
#include <signal.h>
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

int main(int argc, char **argv) {
	CoordConfig cfg;
	char line[256];
	const char *v;

	Coord_DefaultConfig(&cfg);
	cfg.dedicated = 1;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if ((v = CoordCli_Arg(a, "port"))) {
			cfg.port = (uint16_t)atoi(v);
		} else if ((v = CoordCli_Arg(a, "instport"))) {
			cfg.instPort = (uint16_t)atoi(v);
		} else if ((v = CoordCli_Arg(a, "bind"))) {
			snprintf(cfg.bind, sizeof(cfg.bind), "%s", v);
		} else if ((v = CoordCli_Arg(a, "name"))) {
			snprintf(cfg.name, sizeof(cfg.name), "%s", v);
		} else if ((v = CoordCli_Arg(a, "data"))) {
			snprintf(cfg.dataDir, sizeof(cfg.dataDir), "%s", v);
		} else if ((v = CoordCli_Arg(a, "gamedir"))) {
			snprintf(cfg.gameDir, sizeof(cfg.gameDir), "%s", v);
		} else if ((v = CoordCli_Arg(a, "exe"))) {
			snprintf(cfg.exe, sizeof(cfg.exe), "%s", v);
		} else if ((v = CoordCli_Arg(a, "levels"))) {
			snprintf(cfg.levels, sizeof(cfg.levels), "%s", v);
		} else if ((v = CoordCli_Arg(a, "keep"))) {
			snprintf(cfg.keep, sizeof(cfg.keep), "%s", v);
		} else if ((v = CoordCli_Arg(a, "start"))) {
			snprintf(cfg.start, sizeof(cfg.start), "%s", v);
		} else if ((v = CoordCli_Arg(a, "maxplayers"))) {
			cfg.maxPlayers = atoi(v);
		} else if ((v = CoordCli_Arg(a, "pvp"))) {
			cfg.pvp = atoi(v) != 0;
		} else if ((v = CoordCli_Arg(a, "money"))) {
			cfg.money = atoi(v);
		} else if ((v = CoordCli_Arg(a, "grace"))) {
			cfg.grace = atoi(v);
		} else if ((v = CoordCli_Arg(a, "instargs"))) {
			snprintf(cfg.instArgs, sizeof(cfg.instArgs), "%s", v);
		} else if ((v = CoordCli_Arg(a, "mode"))) {
			snprintf(cfg.mode, sizeof(cfg.mode), "%s", v);
		} else if ((v = CoordCli_Arg(a, "list"))) {
			snprintf(cfg.listAddr, sizeof(cfg.listAddr), "%s", v);
		} else if ((v = CoordCli_Arg(a, "listport"))) {
			cfg.listPort = (uint16_t)atoi(v);
		} else if ((v = CoordCli_Arg(a, "listpublic"))) {
			snprintf(cfg.listPublic, sizeof(cfg.listPublic), "%s", v);
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
		Coord_Pass(c, CoordCli_Ms());
		int r = CoordCli_Line(line, sizeof(line), MAIN_WAIT_MS);
		if (r > 0) {
			if (!strcmp(line, "quit") || !strcmp(line, "exit")) {
				break;
			}
			if (line[0] && !Coord_Command(c, line)) {
				printf("coordinator: unknown command '%s' (status, save, kick, start, stop, quit)\n", line);
				fflush(stdout);
			}
		} else if (r < 0) {
			CoordCli_Wait(MAIN_WAIT_MS);
		}
	}
	Coord_Destroy(c);
	return 0;
}
