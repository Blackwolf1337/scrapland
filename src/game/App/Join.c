/*
 * The client's side of a coordinator: the identity key, the join, the tickets, and the coordinator of a game that
 * hosts, see include/scrap/travel.h and include/scrap/coord.h. Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/netrep.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/travel.h"

#define JOIN_RETRY_MS 3000			/* the coordinator is tried again this often */
#define JOIN_TRIES 5				/* times before the first ticket */
#define JOIN_TIMEOUT 30000			/* ms without a packet (level loads) */
#define JOIN_MSG 256

enum { JOIN_OFF, JOIN_CONNECTING, JOIN_HELLO, JOIN_IN, JOIN_GONE };

static struct {
	byte active, host;
	char addr[64];
	char name[COORD_NAME + 1];
	char hostLevel[COORD_LEVEL + 1];
	char hostBind[48];
	char hostKeep[256];
	uint8_t seed[NET2_SEED_SIZE], pub[NET2_KEY_SIZE];
	byte haveKey;
	Net2Host *net;
	int phase;
	int tries;
	uint64_t retryAt;
	byte played;					/* a ticket came: the coordinator going away does not end the game */
	uint16_t instPort;
	Coord *coord;
} join;

byte Join_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("join", param)) {
		Str_Copy(join.addr, ext, sizeof(join.addr));
		join.active = 1;
		return 1;
	}
	if (!crt_stricmp("host", param)) {
		Str_Copy(join.hostLevel, ext, sizeof(join.hostLevel));
		Str_Copy(join.addr, "127.0.0.1", sizeof(join.addr));
		join.active = join.host = 1;
		return 1;
	}
	if (!crt_stricmp("hostbind", param)) {
		Str_Copy(join.hostBind, ext, sizeof(join.hostBind));
		return 1;
	}
	if (!crt_stricmp("hostkeep", param)) {
		Str_Copy(join.hostKeep, ext, sizeof(join.hostKeep));
		return 1;
	}
	if (!crt_stricmp("name", param)) {
		Str_Copy(join.name, ext, sizeof(join.name));
		return 1;
	}
	return 0;
}

byte Join_Active(void) {
	return join.active;
}

static uint64_t Join_Ms(void) {
	return Plat_RealNs() / 1000000;
}

/* the identity key: player.key in the config folder (made on the first start), or the file of SCRAP_PLAYER_KEY */
static byte Join_Key(void) {
	char dir[COORD_PATH], path[COORD_PATH + 16], hex[2 * NET2_KEY_SIZE + 1];
	int made;
	const char *own = getenv("SCRAP_PLAYER_KEY");

	if (own && *own) {
		Str_Copy(path, own, sizeof(path));
	} else {
		Plat_ConfigDir(dir, sizeof(dir));
		Str_PrintfQuiet(path, sizeof(path), "%splayer.key", dir);
	}
	if (!Coord_LoadSeed(path, join.seed, &made)) {
		Con_Errorf("join: cannot read or make the identity key %s\n", path);
		return 0;
	}
	Net2_KeyPublic(join.seed, join.pub);
	Coord_Hex(join.pub, NET2_KEY_SIZE, hex);
	Con_Printf("join: identity %s%s\n", hex, made ? " (new)" : "");
	return 1;
}

static void Join_Log(const char *text) {
	Con_Printf("%s", text);
}

/* -host: the coordinator in this game, its instances children of it on loopback (or on -hostbind) */
static void Join_Host(void) {
	CoordConfig cfg;
	char dir[COORD_PATH];

	Coord_DefaultConfig(&cfg);
	Plat_ConfigDir(dir, sizeof(dir));
	Str_PrintfQuiet(cfg.dataDir, sizeof(cfg.dataDir), "%shost", dir);
	Str_Copy(cfg.gameDir, ".", sizeof(cfg.gameDir));
	CoordProc_Self(cfg.exe, sizeof(cfg.exe));
	cfg.port = Instance_Port();
	Str_Copy(cfg.bind, join.hostBind[0] ? join.hostBind : "127.0.0.1", sizeof(cfg.bind));
	Str_Copy(cfg.start, join.hostLevel[0] ? join.hostLevel : "DownTown", sizeof(cfg.start));
	Str_Copy(cfg.keep, join.hostKeep[0] ? join.hostKeep : cfg.start, sizeof(cfg.keep));
	Str_PrintfQuiet(cfg.name, sizeof(cfg.name), "%s's world", join.name[0] ? join.name : "Player");
	cfg.stopInstances = 1;
	cfg.log = Join_Log;
	join.coord = Coord_Create(&cfg);
	if (!join.coord) {
		Con_Errorf("join: the world could not be hosted\n");
		join.active = 0;
		return;
	}
	if (join.hostBind[0]) {
		Str_Copy(join.addr, join.hostBind, sizeof(join.addr));
	}
}

static void Join_Connect(uint64_t now) {
	Net2Config cfg;
	Net2Addr addr;
	int err = 0;

	join.retryAt = now + JOIN_RETRY_MS;
	if (!Net2_ParseAddr(join.addr, Instance_Port(), &addr)) {
		Con_Errorf("join: '%s' is not a numeric address\n", join.addr);
		join.phase = JOIN_GONE;
		return;
	}
	if (!join.net) {
		Net2_DefaultConfig(&cfg, 0);
		cfg.appVersion = INSTANCE_APP_VERSION;
		cfg.ipv4 = addr.family == NET2_AF_IPV4;
		cfg.ipv6 = addr.family == NET2_AF_IPV6;
		cfg.timeout = JOIN_TIMEOUT;
		join.net = Net2_Create(&cfg, &err);
		if (!join.net) {
			Con_Errorf("join: no Net2 client (error %d)\n", err);
			join.phase = JOIN_GONE;
			return;
		}
	}
	if (Net2_Connect(join.net, &addr, now) == NET2_OK) {
		join.phase = JOIN_CONNECTING;
		Con_Printf("join: connecting to the coordinator %s\n", join.addr);
	}
}

static void Join_Send(NetRepBuf *w) {
	if (!w->bad) {
		Net2_Send(join.net, 0, REP_CH_RELIABLE, w->p, w->size);
	}
}

static void Join_Challenge(NetRepReader *r) {
	uint8_t nonce[COORD_CHALLENGE], skey[NET2_KEY_SIZE], hash[64], sig[NET2_SIG_SIZE], buf[JOIN_MSG];
	char world[COORD_NAME + 1];
	NetRepBuf w;

	uint32_t version = NetRep_GetU16(r);
	CoordMsg_GetBytes(r, nonce, sizeof(nonce));
	NetRep_GetStr(r, world, sizeof(world));
	if (r->bad || version != COORD_VERSION || !Net2_ServerKey(join.net, skey)) {
		Con_Errorf("join: a bad challenge\n");
		return;
	}
	Coord_JoinHash(hash, nonce, skey, join.name);
	Net2_Sign(join.seed, hash, sizeof(hash), sig);
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_JOIN);
	CoordMsg_PutBytes(&w, join.pub, NET2_KEY_SIZE);
	CoordMsg_PutBytes(&w, sig, NET2_SIG_SIZE);
	NetRep_PutU8(&w, join.played ? COORD_JF_RESUME : 0);
	NetRep_PutU16(&w, join.instPort);
	Join_Send(&w);
	join.phase = JOIN_IN;
	Con_Printf("join: world %s, joining as %s%s\n", world, join.name, join.played ? " (back from the instance)" : "");
}

static void Join_Ticket(NetRepReader *r) {
	static uint8_t ticket[COORD_TICKET_MAX];
	char level[COORD_LEVEL + 1];

	uint32_t port = NetRep_GetU16(r);
	NetRep_GetStr(r, level, sizeof(level));
	int n = (int)NetRep_GetU16(r);
	if (n > (int)sizeof(ticket)) {
		r->bad = 1;
	}
	CoordMsg_GetBytes(r, ticket, r->bad ? 0 : n);
	if (r->bad || !port) {
		Con_Errorf("join: a bad ticket\n");
		return;
	}
	join.played = 1;
	join.instPort = (uint16_t)port;
	Con_Printf("join: ticket for %s (port %u)\n", level, port);
	Watch_Go(join.addr, (uint16_t)port, ticket, n, level);
}

static void Join_Net(uint64_t now) {
	Net2Event ev;
	NetRepReader r;
	char text[128];

	if (!join.net) {
		return;
	}
	Net2_Poll(join.net, now);
	while (Net2_Receive(join.net, &ev)) {
		switch (ev.type) {
		case NET2_EV_CONNECT: {
			uint8_t buf[JOIN_MSG];
			NetRepBuf w;
			NetRepBuf_Init(&w, buf, sizeof(buf));
			NetRep_PutU8(&w, COORD_MSG_HELLO);
			NetRep_PutU16(&w, COORD_VERSION);
			NetRep_PutStr(&w, join.name);
			Join_Send(&w);
			join.phase = JOIN_HELLO;
			join.tries = 0;
			break;
		}
		case NET2_EV_DISCONNECT:
			Con_Printf("join: coordinator %s (%s)\n", join.phase == JOIN_CONNECTING ? "not reached" : "lost",
				Net2_ReasonString(ev.reason));
			if (ev.reason >= NET2_DR_APP || (!join.played && ++join.tries >= JOIN_TRIES)) {
				join.phase = JOIN_GONE;
				if (!join.played) {
					Plat_Quit();
				}
			} else {
				join.phase = JOIN_OFF;
				join.retryAt = now + JOIN_RETRY_MS;
			}
			break;
		case NET2_EV_MESSAGE:
			if (ev.channel != REP_CH_RELIABLE || ev.size < 1) {
				break;
			}
			NetRepReader_Init(&r, ev.data + 1, ev.size - 1);
			if (ev.data[0] == COORD_MSG_CHALLENGE && join.phase == JOIN_HELLO) {
				Join_Challenge(&r);
			} else if (ev.data[0] == COORD_MSG_TICKET && join.phase == JOIN_IN) {
				Join_Ticket(&r);
			} else if (ev.data[0] == COORD_MSG_DENY) {
				int reason = (int)NetRep_GetU8(&r);
				NetRep_GetStr(&r, text, sizeof(text));
				Con_Errorf("join: refused: %s (%d)\n", text, reason);
			} else if (ev.data[0] == COORD_MSG_NOTICE) {
				NetRep_GetStr(&r, text, sizeof(text));
				Con_Printf("join: %s\n", text);
			}
			break;
		}
	}
	Net2_Flush(join.net, now);
}

void Join_PreFrame(void) {
	uint64_t now = Join_Ms();

	if (!join.active) {
		return;
	}
	if (!join.haveKey) {
		join.haveKey = 1;
		if (!join.name[0]) {
			const char *n = getenv("SCRAP_PLAYER_NAME");
			Str_Copy(join.name, n && *n ? n : "Player", sizeof(join.name));
		}
		if (!Join_Key()) {
			join.active = 0;
			Plat_Quit();
			return;
		}
		if (join.host) {
			Join_Host();
			if (!join.active) {
				Plat_Quit();
				return;
			}
		}
	}
	if (join.coord) {
		Coord_Pass(join.coord, now);
	}
	if (join.phase == JOIN_OFF && now >= join.retryAt) {
		Join_Connect(now);
	}
	Join_Net(now);
}

void Join_Idle(void) {
	if (!join.active) {
		return;
	}
	uint64_t now = Join_Ms();
	if (join.coord) {
		Coord_Pass(join.coord, now);
	}
	if (join.net) {
		Net2_Poll(join.net, now);
		Net2_Flush(join.net, now);
	}
	Watch_Idle();
}

void Join_Close(void) {
	if (join.net) {
		Net2_Destroy(join.net);
		join.net = NULL;
	}
	if (join.coord) {
		Coord_Destroy(join.coord);
		join.coord = NULL;
	}
	Net2_Wipe(join.seed, sizeof(join.seed));
}
