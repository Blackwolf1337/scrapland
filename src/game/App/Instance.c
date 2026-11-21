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
#include "scrap/arena.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/global.h"
#include "scrap/inputtape.h"
#include "scrap/instance.h"
#include "scrap/logic.h"
#include "scrap/net2.h"
#include "scrap/netscene.h"
#include "scrap/places.h"
#include "scrap/platform.h"
#include "scrap/players.h"
#include "scrap/scorer.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/sputniks.h"
#include "scrap/testdet.h"
#include "scrap/travel.h"
#include "scrap/uichan.h"
#include "scrap/viewers.h"
#include "scrap/world.h"

#define INST_NS 1000000000ull
#define INST_PERIOD (INST_NS / INSTANCE_TICK_HZ)
#define INST_SLOTS 256				/* Net2ServerInfo counts players in a byte */
#define INST_LINE 256
#define INST_HIST 2000				/* buckets of 0.1 ms of the tick time histogram */
#define INST_NET_HIST 1024			/* buckets of INST_NET_STEP bytes of the bytes a client got in a second */
#define INST_NET_STEP 256
#define INST_JOIN_SECS 10			/* s after a connect that count as its join (the creates), not as steady load */

byte g_Instance;

static struct {
	byte opened;					/* the Net2 server was tried */
	Net2Host *host;
	uint16_t port;
	char id[NET2_INFO_NAME + 1];
	byte deal;						/* -deal: a group's super deal */
	char bind[48];					/* numeric IPv4 or IPv6 address, "" for any address of both families */
	int maxPlayers;
	byte pvp;
	int money;
	uint64_t ticks;					/* ticks run */
	uint64_t base;					/* Plat_RealNs when tick baseTick was due */
	uint64_t baseTick;
	uint64_t dropped;				/* ticks given up as backlog */
	uint64_t busyNs, busyMax;		/* run time of the ticks since the last status line */
	uint32_t busyTicks;
	uint32_t hist[INST_HIST + 1];	/* run time of the ticks since "ticks reset" by 0.1 ms, the last all longer */
	uint64_t histMax, histNs;
	uint32_t cpuHist[INST_HIST + 1];	/* the same of the CPU time of the main thread in the ticks */
	uint64_t cpuMax, cpuNs;
	uint64_t lastStatus, lastQuitCheck;	/* ns */
	int players;
	byte live[INST_SLOTS];
	uint64_t since[INST_SLOTS];		/* ms of the connect */
	/* "netload": the bytes each client got in each second (all channels, Net2's count with the headers) */
	uint64_t netAt;					/* ns of the last sample */
	uint64_t netSent[INST_SLOTS];	/* bytesSent at the last sample */
	uint64_t joinBytes[INST_SLOTS];	/* bytes of the join so far */
	uint32_t joinMax[INST_SLOTS];	/* most bytes of one second of the join */
	uint64_t joinAt[INST_SLOTS];	/* ms of the first sample as a watcher, 0 before */
	byte joinDone[INST_SLOTS];
	uint32_t netHist[INST_NET_HIST + 1];
	uint64_t netSum, netMax, netClientSecs;
	uint64_t joinSum, joinPeak, joins;
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
	/* a group's super deal (the coordinator's -deal:<type>,<library>,<map>,<reputation>): the world's save variables a
	 * single player save game has when its level loads (MissionsFuncs.StartSuperDeal), before the level's modules
	 * read them; the reputation of the player that started it */
	if (!crt_stricmp("deal", param)) {
		char type[64], lib[64], map[64], rep[16];
		rep[0] = 0;
		if (sscanf(ext, "%63[^,],%63[^,],%63[^,],%15s", type, lib, map, rep) >= 3) {
			SaveVar_Set("SuperDealType", type);
			SaveVar_Set("Mission.Library", lib);
			SaveVar_Set("Mission.Map", map);
			SaveVar_Set("BadReputation", rep[0] ? rep : "0");
			inst.deal = 1;
		}
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
	return Places_ParseParam(param, ext) || Rep_ParseParam(param, ext) || Coop_ParseParam(param, ext) ||
		Arena_ParseParam(param, ext) || Faction_ParseParam(param, ext);
}

byte Instance_PvP(void) {
	/* the PvP modes are fights between the players whatever -pvp says */
	return inst.pvp || Arena_On();
}

int Instance_StartMoney(void) {
	return inst.money;
}

uint16_t Instance_Port(void) {
	return inst.port;
}

double Instance_Time(void) {
	/* the solo instance on a tape of single player's frames: their times (include/scrap/inputtape.h) */
	if (InputTape_SoloFrames()) {
		return InputTape_SoloTime();
	}
	return (double)inst.ticks / INSTANCE_TICK_HZ;
}

byte Instance_Deal(void) {
	return inst.deal;
}

const char *Instance_Id(void) {
	return inst.id[0] ? inst.id : Instance_Level();
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
	if (g_InputTape == INPUTTAPE_SOLO) {
		/* the solo instance: no network, one player on the input tape */
		Rep_Open();
		Rep_SoloJoin();
		Con_Printf("instance %s: level %s, solo, %d ticks a second\n", inst.id, Instance_Level(), INSTANCE_TICK_HZ);
		return;
	}
	Net2_DefaultConfig(&cfg, 1);
	cfg.port = inst.port;
	cfg.maxConns = inst.maxPlayers;
	cfg.appVersion = INSTANCE_APP_VERSION;
	cfg.inboxSize = INSTANCE_CONN_BUFFER;
	cfg.outboxSize = INSTANCE_CONN_BUFFER;
	if (!Net2_SetBind(&cfg, inst.bind)) {
		Con_Errorf("instance: -bind needs a numeric address, not '%s'\n", inst.bind);
		return;
	}
	inst.host = Net2_Create(&cfg, &err);
	if (!inst.host) {
		Con_Errorf("instance: no Net2 server on port %d (error %d)\n", inst.port, err);
		return;
	}
	Instance_TestLink(inst.host, "instance");
	Instance_SetInfo();
	Rep_Open();
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
			inst.joinBytes[ev.conn] = 0;
			inst.joinMax[ev.conn] = 0;
			inst.joinDone[ev.conn] = 0;
			inst.joinAt[ev.conn] = 0;
			inst.netSent[ev.conn] = 0;
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
				Faction_Message(inst.host, ev.conn, ev.data, ev.size) ||
				Coop_Message(inst.host, ev.conn, ev.data, ev.size) ||
				Arena_Message(inst.host, ev.conn, ev.data, ev.size) ||
				Sputniks_Message(inst.host, ev.conn, ev.data, ev.size) ||
				Ui_Message(inst.host, ev.conn, ev.data, ev.size) ||
				NetScene_Message(inst.host, ev.conn, ev.data, ev.size))) {
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
		/* a coop client's level load can play the intro video first: its hello comes within its ticket's life */
		uint64_t hold = Coop_On() ? COORD_TICKET_LIFE : INSTANCE_TEST_HOLD * 1000u;
		if (inst.live[i] && !Rep_IsWatcher(i) && ms - inst.since[i] >= hold) {
			inst.since[i] = ms;
			Net2_Disconnect(inst.host, i, INSTANCE_DR_NOGAME);
		}
	}
}

/* once a second: the bytes each connection got since the last sample into the histogram of "netload"; the first
 * INST_JOIN_SECS of a connection are its join (the creates of the level), counted apart */
static void Instance_NetSample(uint64_t now, uint64_t ms) {
	Net2Stats st;

	if (!inst.host || now - inst.netAt < INST_NS) {
		return;
	}
	double secs = (double)(now - inst.netAt) / INST_NS;
	inst.netAt = now;
	for (int i = 0; i < INST_SLOTS; i++) {
		if (!inst.live[i] || !Net2_GetStats(inst.host, i, &st)) {
			continue;
		}
		if (st.bytesSent < inst.netSent[i]) {
			inst.netSent[i] = 0;
		}
		uint64_t delta = st.bytesSent - inst.netSent[i];
		uint32_t b = (uint32_t)((double)delta / secs);
		inst.netSent[i] = st.bytesSent;
		if (!Rep_IsWatcher(i)) {
			continue;
		}
		if (!inst.joinAt[i]) {
			inst.joinAt[i] = ms;
		}
		if (!inst.joinDone[i]) {
			inst.joinBytes[i] += delta;
			inst.joinMax[i] = b > inst.joinMax[i] ? b : inst.joinMax[i];
			if (ms - inst.joinAt[i] >= INST_JOIN_SECS * 1000u) {
				Con_Printf("instance %s: join of connection %d: %llu bytes in %d s, at most %u in a second\n", inst.id, i,
					(unsigned long long)inst.joinBytes[i], INST_JOIN_SECS, inst.joinMax[i]);
				inst.joinSum += inst.joinBytes[i];
				inst.joinPeak = inst.joinMax[i] > inst.joinPeak ? inst.joinMax[i] : inst.joinPeak;
				inst.joins++;
				inst.joinDone[i] = 1;
			}
			continue;
		}
		inst.netHist[b / INST_NET_STEP < INST_NET_HIST ? b / INST_NET_STEP : INST_NET_HIST]++;
		inst.netSum += b;
		inst.netMax = b > inst.netMax ? b : inst.netMax;
		inst.netClientSecs++;
	}
}

/* "netload": per client and second the mean, p50, p99 and max bytes since "netload reset", and the joins */
static void Instance_NetLoad(void) {
	uint64_t n = inst.netClientSecs, want50 = (n + 1) / 2, want99 = (uint64_t)((double)n * 0.99 + 0.999999), sum = 0;
	uint32_t p50 = 0, p99 = 0;
	for (int i = 0; i <= INST_NET_HIST && n; i++) {
		sum += inst.netHist[i];
		if (!p50 && sum >= want50) {
			p50 = i == INST_NET_HIST ? (uint32_t)inst.netMax : (uint32_t)(i + 1) * INST_NET_STEP;
		}
		if (sum >= want99) {
			p99 = i == INST_NET_HIST ? (uint32_t)inst.netMax : (uint32_t)(i + 1) * INST_NET_STEP;
			break;
		}
	}
	Con_Printf("instance %s: netload %llu client seconds, bytes a second per client avg %.0f p50 %u p99 %u max %llu; "
		"%llu joins, avg %.0f bytes in %d s, peak %llu in a second\n", inst.id, (unsigned long long)n,
		n ? (double)inst.netSum / (double)n : 0.0, p50, p99, (unsigned long long)inst.netMax,
		(unsigned long long)inst.joins, inst.joins ? (double)inst.joinSum / (double)inst.joins : 0.0, INST_JOIN_SECS,
		(unsigned long long)inst.joinPeak);
}

/* "census": the entities of the world by class and name without digits, the most first (the soak's growth checks) */
static void Instance_Census(void) {
	enum { CENSUS_MAX = 256, CENSUS_KEY = 64 };
	static char key[CENSUS_MAX][CENSUS_KEY];
	static int count[CENSUS_MAX];
	int n = 0, total = 0, other = 0;
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		char k[CENSUS_KEY];
		int len = Str_PrintfQuiet(k, sizeof(k), "%s:", Entity_GetClassName(e));
		for (const char *c = e->name ? e->name : "-"; *c && len < CENSUS_KEY - 1; c++) {
			if (*c < '0' || *c > '9') {
				k[len++] = *c;
			}
		}
		k[len < CENSUS_KEY ? len : CENSUS_KEY - 1] = 0;
		total++;
		int i = 0;
		while (i < n && strcmp(key[i], k)) {
			i++;
		}
		if (i == n) {
			if (n == CENSUS_MAX) {
				other++;
				continue;
			}
			Str_Copy(key[n], k, CENSUS_KEY);
			count[n++] = 0;
		}
		count[i]++;
	}
	Con_Printf("instance %s: census %d entities, %d kinds%s\n", inst.id, total, n, other ? " (more not listed)" : "");
	for (int shown = 0; shown < 24 && shown < n; shown++) {
		int best = shown;
		for (int i = shown + 1; i < n; i++) {
			best = count[i] > count[best] ? i : best;
		}
		char tk[CENSUS_KEY];
		int tc = count[best];
		Str_Copy(tk, key[best], CENSUS_KEY);
		Str_Copy(key[best], key[shown], CENSUS_KEY);
		count[best] = count[shown];
		Str_Copy(key[shown], tk, CENSUS_KEY);
		count[shown] = tc;
		Con_Printf("  census %5d %s\n", tc, tk);
	}
}

/* "spread [seed]": the players on foot or in their ships to points of the level's ground graph as far apart as the
 * graph allows (the load tests) */
static void Instance_Spread(unsigned seed) {
	vec3_t at[PLAYERS_MAX];
	int n = Viewers_Spread(PLAYERS_MAX, seed, at, NULL), k = 0;
	for (int i = 0; i < PLAYERS_MAX && k < n; i++) {
		cEntity *e = Players_Get(i);
		if (!e) {
			continue;
		}
		byte ship = NetPl_Kind(e) == NETREP_SHIP;
		e->pos = (vec3_t){ at[k].x, at[k].y + (ship ? 1500.0f : 100.0f), at[k].z };
		k++;
		if (ship) {
			Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
			Obj(e)->rotVel = (vec3_t){ 0.0f, 0.0f, 0.0f };
		}
		Rep_Placed(i);
	}
	Con_Printf("instance %s: %d players spread\n", inst.id, k);
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

/* the tick time of the share q (0..1) of the ticks in a histogram, ms (the upper bound of its bucket) */
static double Instance_TickPart(const uint32_t *hist, uint64_t max, uint64_t n, double q) {
	uint64_t want = (uint64_t)((double)n * q + 0.999999), sum = 0;
	for (int i = 0; i <= INST_HIST; i++) {
		sum += hist[i];
		if (sum >= want && sum) {
			return i == INST_HIST ? (double)max / 1e6 : (i + 1) * 0.1;
		}
	}
	return 0.0;
}

/* "ticks": tick count, p50, p99, max and mean of the run times since "ticks reset" (the real time, then the CPU time
 * of the main thread: on a busy machine the difference is the wait for a CPU) */
static void Instance_Ticks(void) {
	uint64_t n = 0;
	for (int i = 0; i <= INST_HIST; i++) {
		n += inst.hist[i];
	}
	Con_Printf("instance %s: ticks %llu p50 %.1f p99 %.1f max %.2f avg %.2f ms, cpu p50 %.1f p99 %.1f max %.2f avg %.2f ms, "
		"%llu dropped, %d viewers, %d leaders\n", inst.id, (unsigned long long)n,
		Instance_TickPart(inst.hist, inst.histMax, n, 0.5), Instance_TickPart(inst.hist, inst.histMax, n, 0.99),
		(double)inst.histMax / 1e6, n ? (double)inst.histNs / (double)n / 1e6 : 0.0,
		Instance_TickPart(inst.cpuHist, inst.cpuMax, n, 0.5), Instance_TickPart(inst.cpuHist, inst.cpuMax, n, 0.99),
		(double)inst.cpuMax / 1e6, n ? (double)inst.cpuNs / (double)n / 1e6 : 0.0, (unsigned long long)inst.dropped,
		Viewers_Count(), Viewers_Leaders());
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
	if (!crt_stricmp(line, "ticks")) {
		Instance_Ticks();
		return;
	}
	if (!crt_stricmp(line, "netload")) {
		Instance_NetLoad();
		return;
	}
	if (!crt_stricmp(line, "netload reset")) {
		memset(inst.netHist, 0, sizeof(inst.netHist));
		inst.netSum = inst.netMax = inst.netClientSecs = 0;
		inst.joinSum = inst.joinPeak = inst.joins = 0;
		Con_Printf("instance %s: netload reset\n", inst.id);
		return;
	}
	if (!crt_stricmp(line, "census")) {
		Instance_Census();
		return;
	}
	unsigned spreadSeed = 1;
	if (!strncmp(line, "spread", 6) && (!line[6] || sscanf(line + 6, "%u", &spreadSeed) == 1)) {
		Instance_Spread(spreadSeed);
		return;
	}
	if (!crt_stricmp(line, "ticks reset")) {
		memset(inst.hist, 0, sizeof(inst.hist));
		memset(inst.cpuHist, 0, sizeof(inst.cpuHist));
		inst.histMax = inst.histNs = inst.cpuMax = inst.cpuNs = 0;
		inst.dropped = 0;
		Con_Printf("instance %s: ticks reset\n", inst.id);
		return;
	}
	if (Viewers_Command(line) || Traffic_Command(line)) {
		return;
	}
	/* tests: "alarm <player> <level>" sets one player's alarm (include/scrap/players.h), "money <player> <n>" its
	 * money, "use <player> <entity> [<option>]" makes it use an entity (include/scrap/travel.h), "goto <player>
	 * <entity> [<dx> <dz>]" puts it next to one (a ship level and standing), "switch <player>" gets it into the
	 * nearest parked ship or out of its ship, "call <player>" calls a ship beside it, "msg <player> <text>" shows a
	 * message on its HUD (include/scrap/pres.h) */
	int player, money;
	float level;
	char ent[64], opt[64];
	if (sscanf(line, "money %d %d", &player, &money) == 2) {
		int was = Players_Enter(player);
		*Players_CurMoney() = money;
		Players_Leave(was);
		Con_Printf("instance %s: money of player %d %d\n", inst.id, player, Players_Money(player));
		return;
	}
	opt[0] = 0;
	if (sscanf(line, "use %d %63s %63[^\n]", &player, ent, opt) >= 2) {
		Travel_TestUse(player, ent, opt);
		return;
	}
	if (sscanf(line, "kill %d %63s", &player, ent) == 2) {
		Travel_TestKill(player, ent);
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
	if (sscanf(line, "msg %d %63[^\n]", &player, opt) == 2) {
		int was = Players_Enter(player);
		Scorer_SetMsgText(opt, 5.0f, "Horatio");
		Players_Leave(was);
		return;
	}
	if (sscanf(line, "alarm %d %f", &player, &level) == 2) {
		int was = Players_Enter(player);
		Alarm_Set(Players_CurAlarm(), level);
		Con_Printf("instance %s: alarm of player %d %.2f%s\n", inst.id, player, (double)Players_CurAlarm()->value,
			Players_CurAlarm()->on ? " on" : "");
		Players_Leave(was);
		return;
	}
	if (Rep_Command(line) || Faction_Command(line) || Coop_Command(line) || Arena_Command(line) ||
		Places_Command(line)) {
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

/* the solo instance runs on the clock of the deterministic test mode (when it is on), the others on the real one */
static byte Instance_DetClock(void) {
	return g_InputTape == INPUTTAPE_SOLO && TestDet_On();
}

/* the clock of the ticks in ns */
static uint64_t Instance_Ns(void) {
	return Instance_DetClock() ? (uint64_t)TestDet_Counter() * 100u : Plat_RealNs();
}

void Instance_Pass(void) {
	uint64_t now = Instance_Ns();

	if (!inst.opened) {
		Instance_Open(now);
	}
	uint64_t due = inst.base + (inst.ticks + 1 - inst.baseTick) * INST_PERIOD;
	/* the solo instance on a tape of single player's frames: a tick per frame of the tape, each pass */
	if (InputTape_SoloFrames()) {
		due = now;
	}
	if (now < due) {
		/* the sockets emptied a few times between the ticks, so their buffers do not overflow */
		if (inst.host) {
			Net2_Poll(inst.host, now / 1000000);
		}
		uint64_t wait = (due - now + 999999) / 1000000;
		/* the clock of the deterministic test mode goes on by a step per pass */
		if (!Instance_DetClock()) {
			Plat_WaitEvent((int)(wait > INSTANCE_POLL_MS ? INSTANCE_POLL_MS : wait));
		}
		return;
	}
	if (now - due > INSTANCE_MAX_BEHIND * INST_PERIOD) {
		inst.dropped += (now - due) / INST_PERIOD;
		inst.base = now - INST_PERIOD;
		inst.baseTick = inst.ticks;
	}
	inst.ticks++;
	uint64_t cpu0 = Plat_ThreadCpuNs();
	Instance_Commands(now);
	Instance_NetSample(now, now / 1000000);
	Travel_InstancePass(now / 1000000);
	if (inst.host) {
		Instance_NetIn(now / 1000000);
	}
	Rep_PreFrame(inst.ticks);
	App_Frame(g_pApp);
	Faction_Tick(inst.host, inst.ticks);
	Coop_Pass();
	if (g_TalkModels) {
		/* nothing draws the talks here: the world's and each player's run down (include/scrap/scorer.h) */
		Scorer_TalkTickPlayers(1.0f / INSTANCE_TICK_HZ);
	}
	Arena_Tick(inst.host, inst.ticks);
	if (inst.host) {
		Rep_Tick(inst.host, inst.ticks);
	}
	uint64_t end = Instance_Ns();
	uint64_t cpu = Plat_ThreadCpuNs() - cpu0;
	if (inst.host) {
		Net2_Flush(inst.host, end / 1000000);
	}
	uint64_t busy = end - now;
	inst.busyNs += busy;
	inst.busyMax = busy > inst.busyMax ? busy : inst.busyMax;
	inst.busyTicks++;
	inst.hist[busy / 100000 < INST_HIST ? busy / 100000 : INST_HIST]++;
	inst.histMax = busy > inst.histMax ? busy : inst.histMax;
	inst.histNs += busy;
	inst.cpuHist[cpu / 100000 < INST_HIST ? cpu / 100000 : INST_HIST]++;
	inst.cpuMax = cpu > inst.cpuMax ? cpu : inst.cpuMax;
	inst.cpuNs += cpu;
	if (end - inst.lastStatus >= INSTANCE_STATUS_SECS * INST_NS) {
		inst.lastStatus = end;
		Instance_Status();
	}
}
