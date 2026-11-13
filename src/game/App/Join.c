/*
 * The client's side of a coordinator: the identity key, the join, the tickets, and the coordinator of a game that
 * hosts, see include/scrap/travel.h and include/scrap/coord.h; started by the command line or by the menus of the
 * worlds (Join_Start, Join_StartHost), ended by Join_Stop. Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/netrep.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/texts.h"
#include "scrap/travel.h"

#define JOIN_RETRY_MS 3000			/* the coordinator is tried again this often */
#define JOIN_TRIES 5				/* times before the first ticket */
#define JOIN_TIMEOUT 30000			/* ms without a packet (level loads) */
#define JOIN_MSG 256

enum { JOIN_OFF, JOIN_CONNECTING, JOIN_HELLO, JOIN_IN, JOIN_GONE };

static struct {
	byte active, host;
	byte menu;						/* started by the menus: a failure goes back to them instead of ending the game */
	char addr[64];
	uint16_t port;					/* of the coordinator, 0 -port */
	char name[COORD_NAME + 1];
	char hostLevel[COORD_LEVEL + 1];
	char hostBind[48];
	char hostKeep[256];
	char hostMode[16];				/* "" sandbox */
	int hostPvp, hostMax;			/* -1, 0: the coordinator's defaults */
	byte hostListed;				/* registered with the list server of scrapdecomp.ini */
	uint8_t serverKey[NET2_KEY_SIZE];	/* pinned (a world of the list server's) */
	byte hasServerKey;
	uint8_t seed[NET2_SEED_SIZE], pub[NET2_KEY_SIZE];
	byte haveKey;
	Net2Host *net;
	int phase;
	int tries;
	uint64_t retryAt;
	byte played;					/* a ticket came: the coordinator going away does not end the game */
	uint16_t instPort;
	Coord *coord;
	char deny[128];					/* the text of the coordinator's last deny */
	int ui;							/* JOIN_UI_* */
	char why[160];					/* the text of a JOIN_UI_FAILED */
} join = { .hostPvp = -1 };

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

/* a join that never got a ticket failed: back to the menus with the text, or the end of a game of the command line */
static void Join_Fail(const char *key, const char *arg) {
	char text[sizeof(join.why)];
	Texts_Format(text, sizeof(text), key, arg);
	Con_Errorf("join: %s\n", text);
	if (!join.menu) {
		join.phase = JOIN_GONE;
		Plat_Quit();
		return;
	}
	Join_Stop(text);
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
	/* the menu's mode, else -mode of this game (include/scrap/faction.h, include/scrap/coop.h) */
	Str_Copy(cfg.mode, join.hostMode[0] ? join.hostMode : Coop_Asked() ? "coop" : Faction_ModeName(), sizeof(cfg.mode));
	byte coop = !strcmp(cfg.mode, "coop");
	Plat_ConfigDir(dir, sizeof(dir));
	/* a coop world keeps its party's story apart from the free worlds' players */
	Str_PrintfQuiet(cfg.dataDir, sizeof(cfg.dataDir), coop ? "%shostcoop" : "%shost", dir);
	Str_Copy(cfg.gameDir, ".", sizeof(cfg.gameDir));
	CoordProc_Self(cfg.exe, sizeof(cfg.exe));
	cfg.port = join.port;
	/* the command line's -host on loopback unless -hostbind; the menus' on hostbind= of scrapdecomp.ini (any address
	 * without it) */
	Str_Copy(cfg.bind, join.hostBind[0] || join.menu ? join.hostBind : "127.0.0.1", sizeof(cfg.bind));
	Str_Copy(cfg.start, coop ? COORD_COOP_START : join.hostLevel[0] ? join.hostLevel : "DownTown", sizeof(cfg.start));
	Str_Copy(cfg.keep, join.hostKeep[0] ? join.hostKeep : cfg.start, sizeof(cfg.keep));
	Str_PrintfQuiet(cfg.name, sizeof(cfg.name), "%s's world", join.name[0] ? join.name : "Player");
	if (join.hostPvp >= 0) {
		cfg.pvp = join.hostPvp;
	}
	if (join.hostMax > 0) {
		cfg.maxPlayers = join.hostMax;
	}
	if (join.hostListed && Game_OptionStr("listserver", cfg.listAddr, sizeof(cfg.listAddr))) {
		cfg.listPort = (uint16_t)Game_OptionOr("listport", COORD_LIST_PORT);
	}
	cfg.stopInstances = 1;
	cfg.log = Join_Log;
	join.coord = Coord_Create(&cfg);
	if (!join.coord) {
		join.active = 0;
		return;
	}
	Coord_Loopback(cfg.bind, join.addr, sizeof(join.addr));
}

static void Join_Connect(uint64_t now) {
	Net2Config cfg;
	Net2Addr addr;
	int err = 0;

	join.retryAt = now + JOIN_RETRY_MS;
	if (!Net2_ParseAddr(join.addr, join.port, &addr)) {
		Join_Fail("MP_BadAddress", join.addr);
		return;
	}
	if (!join.net) {
		Net2_DefaultConfig(&cfg, 0);
		cfg.appVersion = INSTANCE_APP_VERSION;
		cfg.ipv4 = addr.family == NET2_AF_IPV4;
		cfg.ipv6 = addr.family == NET2_AF_IPV6;
		cfg.timeout = JOIN_TIMEOUT;
		cfg.serverKey = join.hasServerKey ? join.serverKey : NULL;
		join.net = Net2_Create(&cfg, &err);
		if (!join.net) {
			Con_Errorf("join: no Net2 client (error %d)\n", err);
			Join_Fail("MP_NotReached", "");
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
	join.ui = JOIN_UI_PLAYING;
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
		case NET2_EV_DISCONNECT: {
			const char *why = ev.reason > NET2_DR_APP ? Coord_DenyText(ev.reason - NET2_DR_APP) :
				Net2_ReasonString(ev.reason);
			Con_Printf("join: coordinator %s (%s)\n", join.phase == JOIN_CONNECTING ? "not reached" : "lost", why);
			if (!join.played && (ev.reason >= NET2_DR_APP || ev.reason == NET2_DR_KEY || ++join.tries >= JOIN_TRIES)) {
				if (ev.reason >= NET2_DR_APP) {
					Join_Fail("MP_Refused", join.deny[0] ? join.deny : why);
				} else if (ev.reason == NET2_DR_KEY) {
					Join_Fail("MP_Lost", why);
				} else {
					Join_Fail("MP_NotReached", "");
				}
				return;
			}
			join.tries += join.played;
			join.phase = ev.reason >= NET2_DR_APP ? JOIN_GONE : JOIN_OFF;
			join.retryAt = now + JOIN_RETRY_MS;
			break;
		}
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
				Str_Copy(join.deny, text, sizeof(join.deny));
			} else if (ev.data[0] == COORD_MSG_NOTICE) {
				NetRep_GetStr(&r, text, sizeof(text));
				Con_Printf("join: %s\n", text);
			} else if (ev.data[0] == COORD_MSG_CUE) {
				int kind = (int)NetRep_GetU8(&r);
				NetRep_GetStr(&r, text, sizeof(text));
				if (!r.bad) {
					Coop_Cue(kind, text);
				}
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
	if (Coop_ClientEnded()) {
		Join_Stop("");
		return;
	}
	if (!join.haveKey) {
		join.haveKey = 1;
		if (!join.port) {
			join.port = Instance_Port();
		}
		if (!join.name[0]) {
			const char *n = getenv("SCRAP_PLAYER_NAME");
			Str_Copy(join.name, n && *n ? n : "Player", sizeof(join.name));
		}
		if (!Join_Key()) {
			Join_Fail("MP_HostFailed", "");
			return;
		}
		if (join.host) {
			Join_Host();
			if (!join.active) {
				Join_Fail("MP_HostFailed", "");
				return;
			}
		}
	}
	if (join.coord) {
		Coord_Pass(join.coord, now);
	}
	/* neither the instance nor the coordinator (it refused the player, or did not answer a few times): the world is
	 * gone for this player */
	if (join.played && (join.phase == JOIN_GONE || join.tries >= JOIN_TRIES) && !Watch_Alive()) {
		char text[sizeof(join.why)];
		if (join.phase == JOIN_GONE && join.deny[0]) {
			Texts_Format(text, sizeof(text), "MP_Refused", join.deny);
		} else {
			Str_Copy(text, Script_Lang("MP_WorldGone"), sizeof(text));
		}
		Join_Stop(text);
		return;
	}
	if (join.phase == JOIN_OFF && now >= join.retryAt) {
		Join_Connect(now);
	}
	if (join.active) {
		Join_Net(now);
	}
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

/* a new join or host of the menus: the state of the last one gone */
static void Join_Begin(const char *name) {
	if (join.active) {
		Join_Stop(NULL);
	}
	join.host = 0;
	join.hostMode[0] = 0;
	join.hostPvp = -1;
	join.hostMax = 0;
	join.hostListed = 0;
	join.hostKeep[0] = 0;
	join.hasServerKey = 0;
	join.deny[0] = 0;
	join.why[0] = 0;
	Str_Copy(join.name, name && *name ? name : "Player", sizeof(join.name));
	join.active = join.menu = 1;
	join.ui = JOIN_UI_WAIT;
	g_Play = g_Watch = 1;
}

byte Join_Start(const char *addr, uint16_t port, const char *name, const uint8_t *serverKey) {
	Join_Begin(name);
	Str_Copy(join.addr, addr, sizeof(join.addr));
	join.port = port;
	if (serverKey) {
		memcpy(join.serverKey, serverKey, NET2_KEY_SIZE);
		join.hasServerKey = 1;
	}
	Con_Printf("join: from the menu to %s port %u as %s\n", addr, port, join.name);
	return 1;
}

byte Join_StartHost(const JoinHostOpts *o, const char *name) {
	Join_Begin(name);
	join.host = 1;
	join.port = o->port;
	Str_Copy(join.hostLevel, o->level, sizeof(join.hostLevel));
	Str_Copy(join.hostMode, o->mode, sizeof(join.hostMode));
	join.hostPvp = o->pvp;
	join.hostMax = o->maxPlayers;
	join.hostListed = o->listed;
	if (!Game_OptionStr("hostbind", join.hostBind, sizeof(join.hostBind))) {
		join.hostBind[0] = 0;
	}
	Con_Printf("join: hosting %s (%s) on port %u as %s\n", o->level, o->mode[0] ? o->mode : COORD_MODE, o->port,
		join.name);
	return 1;
}

void Join_Stop(const char *why) {
	byte inWorld = join.played;
	Join_Close();
	Watch_Stop();
	join.active = join.host = join.menu = 0;
	join.phase = JOIN_OFF;
	join.tries = 0;
	join.retryAt = 0;
	join.played = 0;
	join.instPort = 0;
	join.haveKey = 0;
	join.hasServerKey = 0;
	g_Play = g_Watch = 0;
	Str_Copy(join.why, why ? why : "", sizeof(join.why));
	join.ui = join.why[0] ? JOIN_UI_FAILED : JOIN_UI_IDLE;
	Con_Printf("join: ended%s%s\n", join.why[0] ? ": " : "", join.why);
	if (g_HeadlessPlay) {
		Plat_Quit();
		return;
	}
	if (inWorld) {
		SaveVars_Delete("");
		CWinApp_LoadLevel(g_pApp, "Levels/Menu");
	}
}

void Join_Command(const char *line) {
	while (*line == ' ') {
		line++;
	}
	if (!join.coord || !Coord_Command(join.coord, line)) {
		Con_Printf("coord: %s\n", join.coord ? "unknown command (status, save, kick, start, stop, inst)" :
			"this game hosts no world");
	}
}

int Join_UiState(const char **why) {
	if (why) {
		*why = join.why;
	}
	return join.ui;
}

void Join_UiClear(void) {
	join.ui = join.active ? join.ui : JOIN_UI_IDLE;
	join.why[0] = 0;
}
