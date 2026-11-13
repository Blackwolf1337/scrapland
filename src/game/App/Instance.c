/*
 * The headless instance of a level, see include/scrap/instance.h. Not in the original. The main loop of WinApp.c
 * calls Instance_Pass in place of its frame; the window, render device, sound and input are off through
 * App_Headless (include/scrap/game.h) as for the dedicated server, while the application flags stay those of a
 * single player game, so the level scripts start traffic, police and characters.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/net2.h"
#include "scrap/platform.h"
#include "scrap/players.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/travel.h"
#include "scrap/world.h"

#define INST_NS 1000000000ull
#define INST_PERIOD (INST_NS / INSTANCE_TICK_HZ)
#define INST_SLOTS 256				/* Net2ServerInfo counts players in a byte */
#define INST_LINE 256

byte g_Instance;

static struct {
	byte opened;					/* the Net2 server was tried */
	Net2Host *host;
	uint16_t port;
	char id[NET2_INFO_NAME + 1];
	char bind[48];					/* numeric IPv4 address, "" for any address of both families */
	int maxPlayers;
	byte pvp;
	int money;
	uint64_t ticks;					/* ticks run */
	uint64_t base;					/* Plat_RealNs when tick baseTick was due */
	uint64_t baseTick;
	uint64_t dropped;				/* ticks given up as backlog */
	uint64_t busyNs, busyMax;		/* run time of the ticks since the last status line */
	uint32_t busyTicks;
	uint64_t lastStatus, lastQuitCheck;	/* ns */
	int players;
	byte live[INST_SLOTS];
	uint64_t since[INST_SLOTS];		/* ms of the connect */
	byte stdinEnd;
	char line[INST_LINE];
	int lineLen;
} inst = { .port = NET2_DEFAULT_PORT, .maxPlayers = INSTANCE_MAX_PLAYERS, .pvp = 1, .money = INSTANCE_START_MONEY };

byte Instance_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("port", param)) {
		int p = crt_atoi(ext);
		if (p > 0 && p < 65536) {
			inst.port = (uint16_t)p;
		}
		return 1;
	}
	if (!crt_stricmp("id", param)) {
		Str_Copy(inst.id, ext, sizeof(inst.id));
		return 1;
	}
	if (!crt_stricmp("bind", param)) {
		Str_Copy(inst.bind, ext, sizeof(inst.bind));
		return 1;
	}
	if (!crt_stricmp("maxplayers", param)) {
		int n = crt_atoi(ext);
		inst.maxPlayers = n < 1 ? 1 : n >= INST_SLOTS ? INST_SLOTS - 1 : n;
		return 1;
	}
	if (!crt_stricmp("pvp", param)) {
		inst.pvp = crt_atoi(ext) != 0;
		return 1;
	}
	if (!crt_stricmp("money", param)) {
		inst.money = crt_atoi(ext);
		return 1;
	}
	return Rep_ParseParam(param, ext) || Faction_ParseParam(param, ext);
}

byte Instance_PvP(void) {
	return inst.pvp;
}

int Instance_StartMoney(void) {
	return inst.money;
}

uint16_t Instance_Port(void) {
	return inst.port;
}

double Instance_Time(void) {
	return (double)inst.ticks / INSTANCE_TICK_HZ;
}

const char *Instance_Level(void) {
	const char *p = g_pApp->levelPath ? g_pApp->levelPath : "";
	const char *s = strrchr(p, '/');
	return s ? s + 1 : p;
}

static void Instance_SetInfo(void) {
	Net2ServerInfo info;

	memset(&info, 0, sizeof(info));
	info.appVersion = INSTANCE_APP_VERSION;
	info.version = NET2_VERSION;
	info.players = (uint8_t)inst.players;
	info.maxPlayers = (uint8_t)inst.maxPlayers;
	Str_Copy(info.name, inst.id, sizeof(info.name));
	Str_Copy(info.map, Instance_Level(), sizeof(info.map));
	Str_Copy(info.mode, "instance", sizeof(info.mode));
	Net2_SetServerInfo(inst.host, &info);
}

static void Instance_Open(uint64_t now) {
	Net2Config cfg;
	int err = 0;

	inst.opened = 1;
	inst.base = now;
	inst.lastStatus = now;
	inst.lastQuitCheck = now;
	if (!inst.id[0]) {
		Str_Copy(inst.id, Instance_Level(), sizeof(inst.id));
	}
	Net2_DefaultConfig(&cfg, 1);
	cfg.port = inst.port;
	cfg.maxConns = inst.maxPlayers;
	cfg.appVersion = INSTANCE_APP_VERSION;
	cfg.inboxSize = INSTANCE_CONN_BUFFER;
	cfg.outboxSize = INSTANCE_CONN_BUFFER;
	if (inst.bind[0]) {
		cfg.ipv6 = 0;
		if (!Net2_ParseAddr(inst.bind, 0, &cfg.bind4) || cfg.bind4.family != NET2_AF_IPV4) {
			Con_Errorf("instance: -bind needs a numeric IPv4 address, not '%s'\n", inst.bind);
			return;
		}
	}
	inst.host = Net2_Create(&cfg, &err);
	if (!inst.host) {
		Con_Errorf("instance: no Net2 server on port %d (error %d)\n", inst.port, err);
		return;
	}
	Instance_TestLink(inst.host, "instance");
	Instance_SetInfo();
	Con_Printf("instance %s: level %s, Net2 server on %s port %d, %d players, %d ticks a second\n", inst.id,
		Instance_Level(), inst.bind[0] ? inst.bind : "any address", inst.port, inst.maxPlayers, INSTANCE_TICK_HZ);
}

void Instance_TestLink(Net2Host *host, const char *who) {
	const char *l = getenv("SCRAP_NET2_LOSS"), *d = getenv("SCRAP_NET2_DELAY"), *j = getenv("SCRAP_NET2_JITTER");
	float loss = l ? (float)crt_atof(l) : 0.0f;
	int delay = d ? crt_atoi(d) : 0, jitter = j ? crt_atoi(j) : 0;
	if (loss > 0.0f || delay > 0 || jitter > 0) {
		Net2_SetTestLink(host, loss, (uint32_t)(delay > 0 ? delay : 0), (uint32_t)(jitter > 0 ? jitter : 0),
			(uint32_t)Plat_RealNs() | 1u);
		Con_Printf("%s: test link, %.1f%% loss, %d ms delay, %d ms jitter\n", who, (double)loss, delay, jitter);
	}
}

Net2Host *Instance_Host(void) {
	return inst.host;
}

void Instance_Close(void) {
	Travel_InstanceClose();
	Rep_Close();
	if (inst.host) {
		Net2_Destroy(inst.host);
		inst.host = NULL;
	}
}

/* the events of the clients: watchers go to the replication; other (test) connections are counted, echoed and
 * dropped after INSTANCE_TEST_HOLD s */
static void Instance_NetIn(uint64_t ms) {
	Net2Event ev;
	char addr[64];

	Net2_Poll(inst.host, ms);
	while (Net2_Receive(inst.host, &ev)) {
		if (ev.conn < 0 || ev.conn >= INST_SLOTS) {
			continue;
		}
		switch (ev.type) {
		case NET2_EV_CONNECT:
			Net2_AddrString(&ev.addr, addr, sizeof(addr));
			Con_Printf("instance %s: connect %d from %s\n", inst.id, ev.conn, addr);
			inst.live[ev.conn] = 1;
			inst.since[ev.conn] = ms;
			inst.players++;
			Instance_SetInfo();
			break;
		case NET2_EV_DISCONNECT:
			Con_Printf("instance %s: disconnect %d (%s)\n", inst.id, ev.conn, Net2_ReasonString(ev.reason));
			Rep_Disconnect(ev.conn);
			Travel_Drop(ev.conn);
			if (inst.live[ev.conn]) {
				inst.live[ev.conn] = 0;
				inst.players--;
				Instance_SetInfo();
			}
			break;
		case NET2_EV_MESSAGE:
			if (ev.channel == REP_CH_RELIABLE && (Travel_Message(inst.host, ev.conn, ev.data, ev.size) ||
				Faction_Message(inst.host, ev.conn, ev.data, ev.size))) {
				break;
			}
			if (ev.channel == REP_CH_RELIABLE && ev.size > 0 && ev.data[0] == NETREP_MSG_HELLO &&
				!Travel_HelloOk(ev.conn)) {
				Con_Printf("instance %s: connection %d without a ticket\n", inst.id, ev.conn);
				Net2_Disconnect(inst.host, ev.conn, NET2_DR_APP + COORD_DENY_TICKET);
				break;
			}
			if (!Rep_Message(inst.host, ev.conn, ev.channel, ev.data, ev.size)) {
				Net2_Send(inst.host, ev.conn, ev.channel, ev.data, ev.size);
			}
			break;
		}
	}
	for (int i = 0; i < INST_SLOTS; i++) {
		if (inst.live[i] && !Rep_IsWatcher(i) && ms - inst.since[i] >= INSTANCE_TEST_HOLD * 1000u) {
			inst.since[i] = ms;
			Net2_Disconnect(inst.host, i, INSTANCE_DR_NOGAME);
		}
	}
}

static void Instance_Status(void) {
	double avg = inst.busyTicks ? (double)inst.busyNs / inst.busyTicks / 1e6 : 0.0;

	Con_Printf("instance %s: tick %llu, tick ms avg %.2f max %.2f, %llu dropped, %d players\n", inst.id,
		(unsigned long long)inst.ticks, avg, (double)inst.busyMax / 1e6, (unsigned long long)inst.dropped,
		inst.players);
	Rep_Status((double)inst.busyTicks / INSTANCE_TICK_HZ);
	inst.busyNs = inst.busyMax = 0;
	inst.busyTicks = 0;
}

void Instance_Line(char *line) {
	Str_TrimBlanks(line);
	if (!line[0]) {
		return;
	}
	if (!crt_stricmp(line, "quit") || !crt_stricmp(line, "exit")) {
		Con_Printf("instance %s: quit\n", inst.id);
		Plat_Quit();
		return;
	}
	if (!crt_stricmp(line, "status")) {
		Instance_Status();
		return;
	}
	/* tests: "alarm <player> <level>" sets one player's alarm (include/scrap/players.h), "money <player> <n>" its
	 * money, "use <player> <entity> [<option>]" makes it use an entity (include/scrap/travel.h), "goto <player>
	 * <entity> [<dx> <dz>]" puts it next to one, "switch <player>" gets it into the nearest parked ship or out of its ship, "call
	 * <player>" calls a ship beside it */
	int player, money;
	float level;
	char ent[64], opt[64];
	if (sscanf(line, "money %d %d", &player, &money) == 2) {
		Players_Bind(player);
		g_pWorld->money = money;
		Players_BindTurn();
		Con_Printf("instance %s: money of player %d %d\n", inst.id, player, Players_Money(player));
		return;
	}
	opt[0] = 0;
	if (sscanf(line, "use %d %63s %63[^\n]", &player, ent, opt) >= 2) {
		Travel_TestUse(player, ent, opt);
		return;
	}
	float dx = 300.0f, dz = 0.0f;
	if (sscanf(line, "goto %d %63s %f %f", &player, ent, &dx, &dz) >= 2) {
		Travel_TestGoto(player, ent, dx, dz);
		return;
	}
	if (sscanf(line, "switch %d", &player) == 1 || sscanf(line, "call %d", &player) == 1) {
		Rep_TestSwitch(player, line[0] == 'c');
		return;
	}
	if (sscanf(line, "alarm %d %f", &player, &level) == 2) {
		Players_Bind(player);
		Alarm_Set(&g_pWorld->alarm, level);
		Con_Printf("instance %s: alarm of player %d %.2f%s\n", inst.id, player, (double)g_pWorld->alarm.value,
			g_pWorld->alarm.on ? " on" : "");
		Players_BindTurn();
		return;
	}
	if (Rep_Command(line) || Faction_Command(line)) {
		return;
	}
	Console_Execute(line);
}

static void Instance_Commands(uint64_t now) {
	char buf[INST_LINE];
	char path[NET2_INFO_NAME + 32];

	while (!inst.stdinEnd) {
		int n = Plat_ReadStdin(buf, sizeof(buf));
		if (n < 0) {
			inst.stdinEnd = 1;
		}
		if (n <= 0) {
			break;
		}
		for (int i = 0; i < n; i++) {
			if (buf[i] == '\n' || buf[i] == '\r') {
				inst.line[inst.lineLen] = 0;
				inst.lineLen = 0;
				Instance_Line(inst.line);
			} else if (inst.lineLen < INST_LINE - 1) {
				inst.line[inst.lineLen++] = buf[i];
			}
		}
	}
	if (now - inst.lastQuitCheck >= INST_NS) {
		inst.lastQuitCheck = now;
		Str_PrintfQuiet(path, sizeof(path), "instance_%s.quit", inst.id);
		if (!remove(path)) {
			Con_Printf("instance %s: quit file\n", inst.id);
			Plat_Quit();
		}
	}
}

void Instance_Pass(void) {
	uint64_t now = Plat_RealNs();

	if (!inst.opened) {
		Instance_Open(now);
	}
	uint64_t due = inst.base + (inst.ticks + 1 - inst.baseTick) * INST_PERIOD;
	if (now < due) {
		/* the sockets emptied a few times between the ticks, so their buffers do not overflow */
		if (inst.host) {
			Net2_Poll(inst.host, now / 1000000);
		}
		uint64_t wait = (due - now + 999999) / 1000000;
		Plat_WaitEvent((int)(wait > INSTANCE_POLL_MS ? INSTANCE_POLL_MS : wait));
		return;
	}
	if (now - due > INSTANCE_MAX_BEHIND * INST_PERIOD) {
		inst.dropped += (now - due) / INST_PERIOD;
		inst.base = now - INST_PERIOD;
		inst.baseTick = inst.ticks;
	}
	inst.ticks++;
	Instance_Commands(now);
	Travel_InstancePass(now / 1000000);
	if (inst.host) {
		Instance_NetIn(now / 1000000);
	}
	Rep_PreFrame(inst.ticks);
	App_Frame(g_pApp);
	Faction_Tick(inst.host, inst.ticks);
	if (inst.host) {
		Rep_Tick(inst.host, inst.ticks);
	}
	uint64_t end = Plat_RealNs();
	if (inst.host) {
		Net2_Flush(inst.host, end / 1000000);
	}
	uint64_t busy = end - now;
	inst.busyNs += busy;
	inst.busyMax = busy > inst.busyMax ? busy : inst.busyMax;
	inst.busyTicks++;
	if (end - inst.lastStatus >= INSTANCE_STATUS_SECS * INST_NS) {
		inst.lastStatus = end;
		Instance_Status();
	}
}
