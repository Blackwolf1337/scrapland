/*
 * The coordinator of a multiplayer world, see include/scrap/coord.h. One per process (the dedicated ScrapCoord, or a
 * game that hosts): its state is static.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/coord.h"
#include "scrap/netrep.h"

#define CO_CONNS NET2_MAX_CONNS
#define CO_PLAYERS 256				/* players online or in an instance */
#define CO_CHANNEL 0				/* reliable in the Net2 default layout */
#define CO_TIMEOUT 30000			/* ms without a packet (a client's level load polls seldom) */
#define CO_STARTING 2				/* instances starting at once */
#define CO_KICK_WAIT 10000			/* ms a join waits for the instance to let the player's old connection go */
#define CO_MSG 256
#define CO_RESTART_MS 5000		/* a level kept up starts again this long after it went down */

enum { CK_NONE, CK_NEW, CK_HELLO, CK_PLAYER, CK_INST };
enum { CI_DOWN, CI_STARTING, CI_UP, CI_STOPPING };

typedef struct CoConn {
	int kind;
	uint8_t nonce[COORD_CHALLENGE];
	char name[COORD_NAME + 1];
	int player, inst;
} CoConn;

typedef struct CoPlayer {
	int used;
	uint8_t key[NET2_KEY_SIZE];
	char hex[2 * NET2_KEY_SIZE + 1];
	char name[COORD_NAME + 1];
	int conn;						/* its coordinator connection, -1 */
	int inst;						/* the instance it plays in or goes to, -1 */
	int wantTicket;					/* a ticket for inst is due once it is up */
	uint64_t kickAt;				/* a join that waits for the old connection to go (ms), 0 */
	int dirty;
	uint64_t saved;					/* ms */
	int len;
	char text[COORD_STATE_MAX];
} CoPlayer;

typedef struct CoInst {
	char level[COORD_LEVEL + 1];
	uint16_t port;
	int keep;
	int state;
	CoordProc proc;
	uint8_t token[32];
	int conn;						/* its connection, -1 */
	int players;
	int wanted;						/* a player waits for it */
	uint64_t since;					/* ms of the last state change */
	uint64_t empty;					/* ms since it has no player, 0 */
} CoInst;

struct Coord {
	CoordConfig cfg;
	Net2Host *host;
	uint8_t seed[NET2_SEED_SIZE], pub[NET2_KEY_SIZE];
	uint64_t now, lastSave;
	char playersDir[COORD_PATH], logsDir[COORD_PATH], instFile[COORD_PATH];
	int ninst;
	CoInst inst[COORD_MAX_LEVELS];
	CoConn conn[CO_CONNS];
	CoPlayer pl[CO_PLAYERS];
	int online;
	int instFileDirty;
	uint64_t passes;
};

static Coord coord;
static int coord_used;

static void Co_Log(const char *fmt, ...) {
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (coord.cfg.log) {
		coord.cfg.log(buf);
	} else {
		fputs(buf, stdout);
		fflush(stdout);
	}
}

void Coord_DefaultConfig(CoordConfig *cfg) {
	memset(cfg, 0, sizeof(*cfg));
	cfg->port = NET2_DEFAULT_PORT;
	snprintf(cfg->name, sizeof(cfg->name), "Scrapland");
	snprintf(cfg->dataDir, sizeof(cfg->dataDir), "server");
	snprintf(cfg->gameDir, sizeof(cfg->gameDir), ".");
	cfg->maxPlayers = 64;
	cfg->pvp = 1;
	cfg->money = 2000;
}

/* the index of level in the list (case-insensitive), -1 */
static int Co_Level(const char *level) {
	for (int i = 0; i < coord.ninst; i++) {
		const char *a = coord.inst[i].level, *b = level;
		while (*a && *b && (*a | 0x20) == (*b | 0x20)) {
			a++, b++;
		}
		if (!*a && !*b) {
			return i;
		}
	}
	return -1;
}

/* a comma list: each name into fn */
static void Co_List(const char *list, void (*fn)(const char *name)) {
	char name[COORD_LEVEL + 1];
	while (*list) {
		int n = 0;
		while (list[n] && list[n] != ',') {
			n++;
		}
		int k = n > COORD_LEVEL ? COORD_LEVEL : n;
		memcpy(name, list, (size_t)k);
		name[k] = 0;
		if (k) {
			fn(name);
		}
		list += n + (list[n] == ',');
	}
}

static void Co_AddLevel(const char *name) {
	if (coord.ninst >= COORD_MAX_LEVELS || Co_Level(name) >= 0) {
		return;
	}
	CoInst *in = &coord.inst[coord.ninst];
	memset(in, 0, sizeof(*in));
	snprintf(in->level, sizeof(in->level), "%s", name);
	in->port = (uint16_t)(coord.cfg.instPort + coord.ninst);
	in->conn = -1;
	coord.ninst++;
}

static void Co_Keep(const char *name) {
	int i = Co_Level(name);
	if (i >= 0) {
		coord.inst[i].keep = 1;
	}
}

static void Co_SetInfo(void) {
	Net2ServerInfo info;
	memset(&info, 0, sizeof(info));
	info.appVersion = 1;
	info.version = NET2_VERSION;
	info.players = (uint8_t)(coord.online > 255 ? 255 : coord.online);
	info.maxPlayers = (uint8_t)(coord.cfg.maxPlayers > 255 ? 255 : coord.cfg.maxPlayers);
	info.flags = COORD_INFO_WORLD | (coord.cfg.dedicated ? COORD_INFO_DEDICATED : 0);
	snprintf(info.name, sizeof(info.name), "%s", coord.cfg.name);
	snprintf(info.map, sizeof(info.map), "%.32s", coord.cfg.start);
	snprintf(info.mode, sizeof(info.mode), "%s", coord.cfg.mode[0] ? coord.cfg.mode : COORD_MODE);
	Net2_SetServerInfo(coord.host, &info);
}

/* ---- the instances file: the instances a coordinator after this one takes over ---- */

static void Co_WriteInstFile(void) {
	char buf[COORD_MAX_LEVELS * 128], hex[65];
	int n = 0;
	for (int i = 0; i < coord.ninst; i++) {
		CoInst *in = &coord.inst[i];
		if (in->state == CI_DOWN || !in->proc.pid) {
			continue;
		}
		Coord_Hex(in->token, 32, hex);
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s %u %u %s\n", in->level, in->port, in->proc.pid, hex);
	}
	if (!Coord_WriteAtomic(coord.instFile, buf, n, NULL)) {
		Co_Log("coordinator: cannot write %s\n", coord.instFile);
	}
	coord.instFileDirty = 0;
}

static void Co_ReadInstFile(void) {
	char buf[COORD_MAX_LEVELS * 128], level[COORD_LEVEL + 1], hex[80];
	unsigned port, pid;
	if (Coord_ReadFile(coord.instFile, buf, sizeof(buf)) < 0) {
		return;
	}
	for (char *line = buf; *line;) {
		char *end = strchr(line, '\n');
		if (end) {
			*end = 0;
		}
		if (sscanf(line, "%48s %u %u %79s", level, &port, &pid, hex) == 4) {
			int i = Co_Level(level);
			CoInst *in = i >= 0 ? &coord.inst[i] : NULL;
			if (in && in->port == port && Coord_Unhex(hex, in->token, 32) && CoordProc_Adopt(&in->proc, pid)) {
				in->state = CI_STARTING;
				in->since = coord.now;
				Co_Log("coordinator: instance %s (pid %u, port %u) from before, waiting for it\n", in->level, pid, port);
			} else if (in) {
				memset(in->token, 0, sizeof(in->token));
			}
		}
		if (!end) {
			break;
		}
		line = end + 1;
	}
	coord.instFileDirty = 1;
}

/* ---- the instances ---- */

static void Co_SetState(CoInst *in, int state) {
	in->state = state;
	in->since = coord.now;
	coord.instFileDirty = 1;
}

static void Co_Start(CoInst *in) {
	char exe[COORD_PATH + 16], cwd[COORD_PATH + 8], logPath[COORD_PATH + 64], token[65], key[65];
	char a[11][96];
	char extra[256];
	const char *argv[40];
	int n = 0;

	if (coord.cfg.exe[0]) {
		snprintf(exe, sizeof(exe), "%s", coord.cfg.exe);
	} else {
#ifdef _WIN32
		snprintf(exe, sizeof(exe), "%s/Bin/ScrapC.exe", coord.cfg.gameDir);
#else
		snprintf(exe, sizeof(exe), "%s/Bin/ScrapC", coord.cfg.gameDir);
#endif
	}
	snprintf(cwd, sizeof(cwd), "%s/Bin", coord.cfg.gameDir);
	snprintf(logPath, sizeof(logPath), "%s/%s.log", coord.logsDir, in->level);
	if (!Net2_Random(in->token, sizeof(in->token))) {
		Co_Log("coordinator: no random numbers\n");
		return;
	}
	Coord_Hex(in->token, 32, token);
	Coord_Hex(coord.pub, NET2_KEY_SIZE, key);
	argv[n++] = exe;
	snprintf(a[0], sizeof(a[0]), "-instance:%s", in->level);
	snprintf(a[1], sizeof(a[1]), "-port:%u", in->port);
	snprintf(a[2], sizeof(a[2]), "-id:%s", in->level);
	snprintf(a[3], sizeof(a[3]), "-coord:%s", coord.cfg.bind[0] ? coord.cfg.bind : "127.0.0.1");
	snprintf(a[4], sizeof(a[4]), "-coordport:%u", Net2_LocalPort(coord.host, NET2_AF_IPV4) ? Net2_LocalPort(coord.host,
		NET2_AF_IPV4) : coord.cfg.port);
	snprintf(a[5], sizeof(a[5]), "-coordkey:%s", key);
	snprintf(a[6], sizeof(a[6]), "-pvp:%d", coord.cfg.pvp);
	snprintf(a[7], sizeof(a[7]), "-money:%d", coord.cfg.money);
	snprintf(a[8], sizeof(a[8]), "-maxplayers:%d", coord.cfg.maxPlayers);
	for (int i = 0; i < 9; i++) {
		argv[n++] = a[i];
	}
	if (coord.cfg.bind[0]) {
		snprintf(a[9], sizeof(a[9]), "-bind:%s", coord.cfg.bind);
		argv[n++] = a[9];
	}
	if (coord.cfg.mode[0]) {
		snprintf(a[10], sizeof(a[10]), "-mode:%s", coord.cfg.mode);
		argv[n++] = a[10];
	}
	snprintf(extra, sizeof(extra), "%s", coord.cfg.instArgs);
	for (char *p = strtok(extra, " "); p && n < 38; p = strtok(NULL, " ")) {
		argv[n++] = p;
	}
	argv[n] = NULL;
	if (!CoordProc_Start(&in->proc, exe, argv, cwd, logPath, COORD_TOKEN_ENV, token)) {
		Co_Log("coordinator: cannot start %s for %s\n", exe, in->level);
		Co_SetState(in, CI_DOWN);
		in->since = coord.now;
		return;
	}
	Net2_Wipe(token, sizeof(token));
	Co_SetState(in, CI_STARTING);
	in->empty = 0;
	Co_Log("coordinator: instance %s starting (pid %u, port %u, log %s)\n", in->level, in->proc.pid, in->port, logPath);
}

/* the quit file the instance looks for once a second (include/scrap/instance.h) */
static void Co_Stop(CoInst *in, const char *why) {
	char path[COORD_PATH + 64];
	snprintf(path, sizeof(path), "%s/instance_%s.quit", coord.cfg.gameDir, in->level);
	FILE *f = fopen(path, "w");
	if (f) {
		fclose(f);
	}
	Co_SetState(in, CI_STOPPING);
	Co_Log("coordinator: instance %s stopping (%s)\n", in->level, why);
}

static void Co_Send(int conn, NetRepBuf *w) {
	if (!w->bad && conn >= 0) {
		Net2_Send(coord.host, conn, CO_CHANNEL, w->p, w->size);
	}
}

/* ---- the players ---- */

static int Co_FindPlayer(const uint8_t key[NET2_KEY_SIZE]) {
	for (int i = 0; i < CO_PLAYERS; i++) {
		if (coord.pl[i].used && !memcmp(coord.pl[i].key, key, NET2_KEY_SIZE)) {
			return i;
		}
	}
	return -1;
}

static void Co_Save(CoPlayer *p) {
	if (!CoordStore_Save(coord.playersDir, p->key, p->text, p->len)) {
		Co_Log("coordinator: cannot save player %s (%.8s)\n", p->name, p->hex);
		return;
	}
	p->dirty = 0;
	p->saved = coord.now;
}

/* the player of key in the table, its progress loaded from the store when it was not there: -1 no room or store */
static int Co_Player(const uint8_t key[NET2_KEY_SIZE], int *loaded) {
	int i = Co_FindPlayer(key);
	*loaded = 0;
	if (i >= 0) {
		return i;
	}
	for (i = 0; i < CO_PLAYERS && coord.pl[i].used; i++) {
	}
	if (i == CO_PLAYERS) {
		return -1;
	}
	CoPlayer *p = &coord.pl[i];
	memset(p, 0, sizeof(*p));
	memcpy(p->key, key, NET2_KEY_SIZE);
	Coord_Hex(key, NET2_KEY_SIZE, p->hex);
	p->conn = -1;
	p->inst = -1;
	int r = CoordStore_Load(coord.playersDir, key, p->text, &p->len, sizeof(p->text));
	if (r == COORD_LD_IO) {
		Co_Log("coordinator: cannot read the progress of %.8s\n", p->hex);
		return -1;
	}
	if (r == COORD_LD_BACKUP) {
		Co_Log("coordinator: the progress file of %.8s was bad, the one before it was taken\n", p->hex);
	} else if (r == COORD_LD_CORRUPT) {
		Co_Log("coordinator: the progress files of %.8s were bad (kept as .corrupt), new progress\n", p->hex);
	}
	CoordKv_Get(p->text, p->len, "name", p->name, sizeof(p->name));
	p->used = 1;
	p->dirty = r == COORD_LD_BACKUP || r == COORD_LD_CORRUPT;
	*loaded = r == COORD_LD_NEW ? 2 : 1;
	return i;
}

static void Co_Release(int i) {
	CoPlayer *p = &coord.pl[i];
	if (p->dirty) {
		Co_Save(p);
	}
	if (p->conn < 0 && p->inst < 0 && !p->dirty) {
		p->used = 0;
	}
}

static void Co_Deny(int conn, int reason, const char *text) {
	uint8_t buf[CO_MSG];
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_DENY);
	NetRep_PutU8(&w, (uint32_t)reason);
	NetRep_PutStr(&w, text);
	Co_Send(conn, &w);
	Net2_Disconnect(coord.host, conn, NET2_DR_APP + reason);
}

static void Co_Notice(int conn, const char *text) {
	uint8_t buf[CO_MSG];
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_NOTICE);
	NetRep_PutStr(&w, text);
	Co_Send(conn, &w);
}

static void Co_SendTicket(CoPlayer *p) {
	static CoordTicket t;
	static uint8_t buf[COORD_TICKET_MAX + 64], tk[COORD_TICKET_MAX];
	CoInst *in = &coord.inst[p->inst];
	NetRepBuf w;

	memset(&t, 0, sizeof(t));
	memcpy(t.key, p->key, NET2_KEY_SIZE);
	snprintf(t.level, sizeof(t.level), "%s", in->level);
	CoordKv_Get(p->text, p->len, "entrance", t.entrance, sizeof(t.entrance));
	CoordKv_Get(p->text, p->len, "door", t.door, sizeof(t.door));
	t.issued = Coord_UnixMs();
	t.expires = t.issued + COORD_TICKET_LIFE;
	Net2_Random(t.nonce, sizeof(t.nonce));
	memcpy(t.state, p->text, (size_t)p->len);
	t.stateLen = p->len;
	int n = Coord_TicketSeal(&t, coord.seed, tk, sizeof(tk));
	if (!n) {
		Co_Log("coordinator: the ticket of %s does not fit\n", p->name);
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_TICKET);
	NetRep_PutU16(&w, in->port);
	NetRep_PutStr(&w, in->level);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, tk, n);
	Co_Send(p->conn, &w);
	p->wantTicket = 0;
	Co_Log("coordinator: ticket for %s (%.8s) to %s port %u%s%s\n", p->name, p->hex, in->level, in->port,
		t.entrance[0] ? " at " : "", t.entrance);
}

/* the player goes to the instance of level i: a ticket once it is up */
static void Co_Send2Level(CoPlayer *p, int i) {
	p->inst = i;
	p->wantTicket = 1;
	coord.inst[i].wanted = 1;
}

static void Co_Hello(int conn, NetRepReader *r) {
	uint8_t buf[CO_MSG];
	NetRepBuf w;
	CoConn *c = &coord.conn[conn];

	uint32_t version = NetRep_GetU16(r);
	NetRep_GetStr(r, c->name, sizeof(c->name));
	if (r->bad || version != COORD_VERSION) {
		Co_Deny(conn, COORD_DENY_VERSION, "another version of the game");
		return;
	}
	for (char *s = c->name; *s; s++) {
		if ((unsigned char)*s < 32 || *s == 127) {
			*s = '_';
		}
	}
	if (!c->name[0]) {
		snprintf(c->name, sizeof(c->name), "Player");
	}
	if (!Net2_Random(c->nonce, sizeof(c->nonce))) {
		Co_Deny(conn, COORD_DENY_STORE, "no random numbers");
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_CHALLENGE);
	NetRep_PutU16(&w, COORD_VERSION);
	CoordMsg_PutBytes(&w, c->nonce, sizeof(c->nonce));
	NetRep_PutStr(&w, coord.cfg.name);
	Co_Send(conn, &w);
	c->kind = CK_HELLO;
}

static void Co_SendKick(int inst, const uint8_t key[NET2_KEY_SIZE], int reason) {
	uint8_t buf[64];
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_KICK);
	CoordMsg_PutBytes(&w, key, NET2_KEY_SIZE);
	NetRep_PutU8(&w, (uint32_t)reason);
	if (inst >= 0) {
		Co_Send(coord.inst[inst].conn, &w);
	}
}

static void Co_Join(int conn, NetRepReader *r) {
	uint8_t key[NET2_KEY_SIZE], sig[NET2_SIG_SIZE], hash[64], skey[NET2_KEY_SIZE];
	CoConn *c = &coord.conn[conn];
	int loaded;

	CoordMsg_GetBytes(r, key, NET2_KEY_SIZE);
	CoordMsg_GetBytes(r, sig, NET2_SIG_SIZE);
	uint32_t flags = NetRep_GetU8(r);
	uint32_t port = NetRep_GetU16(r);
	Net2_ServerKey(coord.host, skey);
	Coord_JoinHash(hash, c->nonce, skey, c->name);
	if (r->bad || !Net2_Verify(key, hash, sizeof(hash), sig)) {
		Co_Deny(conn, COORD_DENY_SIGNATURE, "the signature of the join is wrong");
		return;
	}
	if (coord.online >= coord.cfg.maxPlayers) {
		Co_Deny(conn, COORD_DENY_FULL, "the world is full");
		return;
	}
	int i = Co_Player(key, &loaded);
	if (i < 0) {
		Co_Deny(conn, COORD_DENY_STORE, "the progress could not be read");
		return;
	}
	CoPlayer *p = &coord.pl[i];
	if (p->conn >= 0) {
		Co_Deny(conn, COORD_DENY_PLAYING, "this player is in the world already");
		return;
	}
	p->conn = conn;
	memcpy(p->name, c->name, sizeof(p->name));
	CoordKv_Set(p->text, &p->len, sizeof(p->text), "name", p->name);
	c->kind = CK_PLAYER;
	c->player = i;
	coord.online++;
	Co_SetInfo();
	Co_Log("coordinator: %s (%.8s) joins, %s%s\n", p->name, p->hex, loaded == 2 ? "new player" : "progress loaded",
		flags & COORD_JF_RESUME ? ", back from its instance" : "");
	if (flags & COORD_JF_RESUME) {
		/* the coordinator came back: the client stays where it is (its instance reports it) */
		for (int k = 0; k < coord.ninst && p->inst < 0; k++) {
			if (coord.inst[k].port == port) {
				p->inst = k;
			}
		}
		return;
	}
	if (p->inst >= 0 && !p->wantTicket) {
		/* still in an instance without a client (it left uncleanly): that connection goes first */
		Co_SendKick(p->inst, p->key, COORD_DENY_KICK);
		p->kickAt = coord.now;
		return;
	}
	if (p->inst >= 0) {
		return;			/* a travel's ticket waits for its instance */
	}
	char level[COORD_LEVEL + 1];
	CoordKv_Get(p->text, p->len, "level", level, sizeof(level));
	int k = Co_Level(level);
	if (k < 0) {
		k = Co_Level(coord.cfg.start);
		CoordKv_Set(p->text, &p->len, sizeof(p->text), "entrance", "");
		CoordKv_Set(p->text, &p->len, sizeof(p->text), "door", "");
	}
	if (k < 0) {
		Co_Deny(conn, COORD_DENY_LEVEL, "no level to start in");
		return;
	}
	Co_Send2Level(p, k);
}

static void Co_Refuse(int conn, const uint8_t key[NET2_KEY_SIZE], const char *text) {
	uint8_t buf[CO_MSG];
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_REFUSE);
	CoordMsg_PutBytes(&w, key, NET2_KEY_SIZE);
	NetRep_PutStr(&w, text);
	Co_Send(conn, &w);
}

static void Co_State(int conn, NetRepReader *r) {
	static char state[COORD_STATE_MAX];
	uint8_t key[NET2_KEY_SIZE];
	char level[COORD_LEVEL + 1], entrance[COORD_LEVEL + 1], door[COORD_LEVEL + 1];
	int from = coord.conn[conn].inst, loaded;

	uint32_t reason = NetRep_GetU8(r);
	CoordMsg_GetBytes(r, key, NET2_KEY_SIZE);
	NetRep_GetStr(r, level, sizeof(level));
	NetRep_GetStr(r, entrance, sizeof(entrance));
	NetRep_GetStr(r, door, sizeof(door));
	uint32_t n = NetRep_GetU16(r);
	if (n > sizeof(state)) {
		r->bad = 1;
	}
	CoordMsg_GetBytes(r, state, r->bad ? 0 : (int)n);
	if (r->bad || !CoordKv_Valid(state, (int)n)) {
		Co_Log("coordinator: bad state message from %s\n", coord.inst[from].level);
		return;
	}
	int i = Co_Player(key, &loaded);
	if (i < 0) {
		return;
	}
	CoPlayer *p = &coord.pl[i];
	if (reason == COORD_SR_JOIN) {
		p->inst = from;
		p->wantTicket = 0;
	} else if (p->inst < 0 && reason == COORD_SR_REPORT) {
		p->inst = from;			/* the coordinator came back: where the player is */
	} else if (p->inst != from) {
		return;					/* a report of an instance the player left */
	}
	CoordKv_Merge(p->text, &p->len, sizeof(p->text), state, (int)n);
	if (p->name[0]) {
		CoordKv_Set(p->text, &p->len, sizeof(p->text), "name", p->name);
	}
	CoordKv_Set(p->text, &p->len, sizeof(p->text), "level", coord.inst[from].level);
	p->dirty = 1;
	switch (reason) {
	case COORD_SR_JOIN:
		Co_Log("coordinator: %s (%.8s) is in %s\n", p->name, p->hex, coord.inst[from].level);
		break;
	case COORD_SR_LEAVE:
		Co_Log("coordinator: %s (%.8s) left %s\n", p->name, p->hex, coord.inst[from].level);
		p->inst = -1;
		Co_Save(p);
		if (p->kickAt && p->conn >= 0) {
			/* the join that waited for the old connection */
			p->kickAt = 0;
			Co_Send2Level(p, from);
		}
		Co_Release(i);
		break;
	case COORD_SR_TRAVEL: {
		int k = Co_Level(level);
		if (k < 0) {
			Co_Log("coordinator: %s (%.8s) cannot go to %s (not a level of this world)\n", p->name, p->hex, level);
			Co_Refuse(conn, key, "that level is closed in this world");
			if (p->conn >= 0) {
				Co_Notice(p->conn, "that level is closed in this world");
			}
			break;
		}
		CoordKv_Set(p->text, &p->len, sizeof(p->text), "level", coord.inst[k].level);
		CoordKv_Set(p->text, &p->len, sizeof(p->text), "entrance", entrance);
		CoordKv_Set(p->text, &p->len, sizeof(p->text), "door", door);
		Co_Save(p);
		Co_Log("coordinator: %s (%.8s) travels from %s to %s (%s, %s)\n", p->name, p->hex, coord.inst[from].level,
			coord.inst[k].level, entrance[0] ? entrance : "spawn", door[0] ? door : "-");
		Co_Send2Level(p, k);
		break;
	}
	}
}

static void Co_Inst(int conn, NetRepReader *r) {
	uint8_t token[32];
	char id[COORD_NAME + 1], level[COORD_LEVEL + 1];

	uint32_t version = NetRep_GetU16(r);
	CoordMsg_GetBytes(r, token, sizeof(token));
	NetRep_GetStr(r, id, sizeof(id));
	NetRep_GetStr(r, level, sizeof(level));
	uint32_t port = NetRep_GetU16(r);
	uint32_t pid = NetRep_GetU32(r);
	int i = Co_Level(level);
	CoInst *in = i >= 0 ? &coord.inst[i] : NULL;
	if (r->bad || version != COORD_VERSION || !in || in->port != port || in->state == CI_DOWN ||
		memcmp(in->token, token, sizeof(token))) {
		Co_Log("coordinator: an instance (%s, port %u) with a wrong token\n", level, port);
		Net2_Disconnect(coord.host, conn, NET2_DR_APP + COORD_DENY_TICKET);
		return;
	}
	if (in->conn >= 0 && in->conn != conn) {
		Net2_Disconnect(coord.host, in->conn, NET2_DR_APP + COORD_DENY_KICK);
		coord.conn[in->conn].kind = CK_NONE;
	}
	coord.conn[conn].kind = CK_INST;
	coord.conn[conn].inst = i;
	in->conn = conn;
	if (in->state == CI_STARTING) {
		Co_SetState(in, CI_UP);
	}
	if (!in->proc.pid && pid) {
		CoordProc_Adopt(&in->proc, pid);
	}
	Co_Log("coordinator: instance %s up (pid %u, port %u)\n", in->level, pid, in->port);
}

static void Co_Disconnect(int conn) {
	CoConn *c = &coord.conn[conn];
	if (c->kind == CK_PLAYER) {
		CoPlayer *p = &coord.pl[c->player];
		Co_Log("coordinator: %s (%.8s) left the coordinator%s\n", p->name, p->hex, p->inst >= 0 ? "" : " and the world");
		p->conn = -1;
		p->kickAt = 0;
		coord.online--;
		Co_SetInfo();
		Co_Release(c->player);
	} else if (c->kind == CK_INST) {
		CoInst *in = &coord.inst[c->inst];
		if (in->conn == conn) {
			in->conn = -1;
			if (in->state == CI_UP) {
				Co_SetState(in, CI_STARTING);	/* it connects again or its process ends */
			}
			Co_Log("coordinator: instance %s lost\n", in->level);
		}
	}
	memset(c, 0, sizeof(*c));
}

static void Co_Net(void) {
	Net2Event ev;
	NetRepReader r;

	Net2_Poll(coord.host, coord.now);
	while (Net2_Receive(coord.host, &ev)) {
		if (ev.conn < 0 || ev.conn >= CO_CONNS) {
			continue;
		}
		CoConn *c = &coord.conn[ev.conn];
		switch (ev.type) {
		case NET2_EV_CONNECT:
			memset(c, 0, sizeof(*c));
			c->kind = CK_NEW;
			c->player = c->inst = -1;
			break;
		case NET2_EV_DISCONNECT:
			Co_Disconnect(ev.conn);
			break;
		case NET2_EV_MESSAGE:
			if (ev.channel != CO_CHANNEL || ev.size < 1) {
				break;
			}
			NetRepReader_Init(&r, ev.data + 1, ev.size - 1);
			if (c->kind == CK_NEW && ev.data[0] == COORD_MSG_HELLO) {
				Co_Hello(ev.conn, &r);
			} else if (c->kind == CK_HELLO && ev.data[0] == COORD_MSG_JOIN) {
				Co_Join(ev.conn, &r);
			} else if (c->kind == CK_NEW && ev.data[0] == COORD_MSG_INST) {
				Co_Inst(ev.conn, &r);
			} else if (c->kind == CK_INST && ev.data[0] == COORD_MSG_STATE) {
				Co_State(ev.conn, &r);
			}
			break;
		}
	}
}

/* the processes, the starts, the ends of empty instances */
static void Co_Instances(void) {
	int starting = 0;
	for (int i = 0; i < coord.ninst; i++) {
		coord.inst[i].players = 0;
		coord.inst[i].wanted = 0;
		starting += coord.inst[i].state == CI_STARTING;
	}
	for (int k = 0; k < CO_PLAYERS; k++) {
		CoPlayer *p = &coord.pl[k];
		if (p->used && p->inst >= 0) {
			coord.inst[p->inst].players++;
			coord.inst[p->inst].wanted |= p->wantTicket;
		}
	}
	uint64_t grace = (uint64_t)(coord.cfg.grace ? coord.cfg.grace : COORD_GRACE_SECS) * 1000u;
	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < coord.ninst; i++) {
			CoInst *in = &coord.inst[i];
			int alive = in->state != CI_DOWN && CoordProc_Alive(&in->proc);
			if (pass == 1) {
				/* the starts: the levels players wait for first, then those kept up */
				if (in->state == CI_DOWN && starting < CO_STARTING && (in->wanted || (in->keep && coord.now - in->since >
					CO_RESTART_MS))) {
					Co_Start(in);
					starting += in->state == CI_STARTING;
				}
				continue;
			}
			switch (in->state) {
			case CI_STARTING:
				if (!alive) {
					Co_Log("coordinator: instance %s ended while starting (see %s/%s.log)\n", in->level, coord.logsDir,
						in->level);
					Co_SetState(in, CI_DOWN);
				} else if (coord.now - in->since > COORD_START_SECS * 1000u) {
					Co_Stop(in, "it did not connect");
				}
				break;
			case CI_UP:
				if (!alive) {
					Co_Log("coordinator: instance %s ended\n", in->level);
					if (in->conn >= 0) {
						Net2_Disconnect(coord.host, in->conn, NET2_DR_APP + COORD_DENY_KICK);
						coord.conn[in->conn].kind = CK_NONE;
						in->conn = -1;
					}
					Co_SetState(in, CI_DOWN);
					break;
				}
				in->empty = in->players ? 0 : in->empty ? in->empty : coord.now;
				if (!in->keep && in->empty && coord.now - in->empty > grace) {
					Co_Stop(in, "no player");
				}
				break;
			case CI_STOPPING:
				if (!alive) {
					Co_Log("coordinator: instance %s stopped\n", in->level);
					Co_SetState(in, CI_DOWN);
				} else if (coord.now - in->since > COORD_STOP_SECS * 1000u) {
					Co_Log("coordinator: instance %s killed\n", in->level);
					CoordProc_Kill(&in->proc);
				}
				break;
			}
		}
	}
	/* the tickets of the players whose instance is up */
	for (int k = 0; k < CO_PLAYERS; k++) {
		CoPlayer *p = &coord.pl[k];
		if (!p->used || p->conn < 0) {
			continue;
		}
		if (p->kickAt && coord.now - p->kickAt > CO_KICK_WAIT) {
			/* the instance did not let it go: the ticket anyway (a second connection of the key replaces the old one) */
			p->kickAt = 0;
			p->wantTicket = 1;
		}
		if (p->wantTicket && p->inst >= 0 && coord.inst[p->inst].state == CI_UP && coord.inst[p->inst].conn >= 0) {
			Co_SendTicket(p);
		}
	}
}

static void Co_SaveAll(int all) {
	for (int i = 0; i < CO_PLAYERS; i++) {
		if (coord.pl[i].used && coord.pl[i].dirty && (all || coord.now - coord.pl[i].saved >= 1000u)) {
			Co_Save(&coord.pl[i]);
		}
	}
	coord.lastSave = coord.now;
}

Coord *Coord_Create(const CoordConfig *cfg) {
	char seedPath[COORD_PATH + 32];
	int made;

	if (coord_used) {
		return NULL;
	}
	memset(&coord, 0, sizeof(coord));
	coord.cfg = *cfg;
	CoordConfig *c = &coord.cfg;
	if (!c->instPort) {
		c->instPort = (uint16_t)(c->port + NET2_LAN_PORTS);
	}
	if (!c->start[0]) {
		snprintf(c->start, sizeof(c->start), "DownTown");
	}
	Co_List(c->levels[0] ? c->levels : COORD_CITY, Co_AddLevel);
	Co_List(c->start, Co_AddLevel);
	Co_List(c->keep[0] ? c->keep : COORD_CITY, Co_Keep);
	if (snprintf(coord.playersDir, sizeof(coord.playersDir), "%s/players", c->dataDir) >= (int)sizeof(coord.playersDir) ||
		snprintf(coord.logsDir, sizeof(coord.logsDir), "%s/logs", c->dataDir) >= (int)sizeof(coord.logsDir) ||
		snprintf(coord.instFile, sizeof(coord.instFile), "%s/instances.txt", c->dataDir) >= (int)sizeof(coord.instFile) ||
		snprintf(seedPath, sizeof(seedPath), "%s/coordinator.key", c->dataDir) >= (int)sizeof(seedPath)) {
		Co_Log("coordinator: the data folder's path is too long\n");
		return NULL;
	}
	if (!Coord_MkDir(c->dataDir) || !Coord_MkDir(coord.playersDir) || !Coord_MkDir(coord.logsDir)) {
		Co_Log("coordinator: cannot make the data folder %s\n", c->dataDir);
		return NULL;
	}
	if (!Coord_LoadSeed(seedPath, coord.seed, &made)) {
		Co_Log("coordinator: cannot read or make %s\n", seedPath);
		return NULL;
	}
	Net2_KeyPublic(coord.seed, coord.pub);
	Net2Config nc;
	int err = 0;
	Net2_DefaultConfig(&nc, 1);
	nc.port = c->port;
	nc.maxConns = c->maxPlayers + COORD_MAX_LEVELS + 16 > CO_CONNS ? CO_CONNS : c->maxPlayers + COORD_MAX_LEVELS + 16;
	nc.appVersion = 1;
	nc.timeout = CO_TIMEOUT;
	nc.identitySeed = coord.seed;
	if (c->bind[0]) {
		nc.ipv6 = 0;
		if (!Net2_ParseAddr(c->bind, 0, &nc.bind4) || nc.bind4.family != NET2_AF_IPV4) {
			Co_Log("coordinator: -bind needs a numeric IPv4 address, not '%s'\n", c->bind);
			return NULL;
		}
	}
	coord.host = Net2_Create(&nc, &err);
	if (!coord.host) {
		Co_Log("coordinator: no Net2 server on port %u (error %d)\n", c->port, err);
		return NULL;
	}
	coord_used = 1;
	for (int i = 0; i < CO_CONNS; i++) {
		coord.conn[i].player = coord.conn[i].inst = -1;
	}
	Co_SetInfo();
	Co_ReadInstFile();
	char hex[65];
	Coord_Hex(coord.pub, NET2_KEY_SIZE, hex);
	Co_Log("coordinator %s: port %u on %s, instances from port %u, %d levels, data %s, key %s%s\n", c->name, c->port,
		c->bind[0] ? c->bind : "any address", c->instPort, coord.ninst, c->dataDir, hex, made ? " (new)" : "");
	return &coord;
}

void Coord_Pass(Coord *c, uint64_t now) {
	if (!c) {
		return;
	}
	if (!coord.passes++) {
		/* the instances taken over wait for their connection from now on */
		for (int i = 0; i < coord.ninst; i++) {
			coord.inst[i].since = now;
		}
		coord.lastSave = now;
	}
	coord.now = now;
	Co_Net();
	Co_Instances();
	uint64_t every = (uint64_t)(coord.cfg.saveSecs ? coord.cfg.saveSecs : COORD_SAVE_SECS) * 1000u;
	if (now - coord.lastSave >= every) {
		Co_SaveAll(0);
	}
	if (coord.instFileDirty) {
		Co_WriteInstFile();
	}
	Net2_Flush(coord.host, now);
}

void Coord_Status(Coord *c) {
	static const char *const states[] = { "down", "starting", "up", "stopping" };
	if (!c) {
		return;
	}
	Co_Log("coordinator %s: %d players online\n", coord.cfg.name, coord.online);
	for (int i = 0; i < coord.ninst; i++) {
		CoInst *in = &coord.inst[i];
		if (in->state != CI_DOWN || in->keep) {
			Co_Log("  %s: %s, port %u, pid %u, %d players%s\n", in->level, states[in->state], in->port, in->proc.pid,
				in->players, in->keep ? ", kept up" : "");
		}
	}
	for (int k = 0; k < CO_PLAYERS; k++) {
		CoPlayer *p = &coord.pl[k];
		if (p->used) {
			char money[32];
			CoordKv_Get(p->text, p->len, "money", money, sizeof(money));
			Co_Log("  player %s (%.8s): %s, %s, money %s%s\n", p->name, p->hex, p->conn >= 0 ? "online" : "offline",
				p->inst >= 0 ? coord.inst[p->inst].level : "no instance", money[0] ? money : "-",
				p->wantTicket ? ", ticket due" : "");
		}
	}
}

int Coord_Command(Coord *c, const char *line) {
	char arg[80];
	if (!c) {
		return 0;
	}
	if (!strcmp(line, "status")) {
		Coord_Status(c);
		return 1;
	}
	if (!strcmp(line, "save")) {
		Co_SaveAll(1);
		Co_Log("coordinator: saved\n");
		return 1;
	}
	if (sscanf(line, "stop %79s", arg) == 1 || sscanf(line, "start %79s", arg) == 1) {
		int i = Co_Level(arg);
		if (i < 0) {
			Co_Log("coordinator: no level %s\n", arg);
		} else if (line[2] == 'o' && coord.inst[i].state != CI_DOWN) {
			coord.inst[i].keep = 0;
			Co_Stop(&coord.inst[i], "asked");
		} else if (line[2] == 'a') {
			coord.inst[i].keep = 1;
		}
		return 1;
	}
	int at = 0;
	if (sscanf(line, "inst %79s %n", arg, &at) == 1 && at > 0) {
		uint8_t buf[CO_MSG];
		NetRepBuf w;
		int i = Co_Level(arg);
		if (i < 0 || coord.inst[i].conn < 0) {
			Co_Log("coordinator: no instance %s up\n", arg);
			return 1;
		}
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_PutU8(&w, COORD_MSG_CONSOLE);
		NetRep_PutStr(&w, line + at);
		Co_Send(coord.inst[i].conn, &w);
		return 1;
	}
	if (sscanf(line, "kick %79s", arg) == 1) {
		for (int k = 0; k < CO_PLAYERS; k++) {
			CoPlayer *p = &coord.pl[k];
			if (p->used && (!strcmp(p->name, arg) || !strncmp(p->hex, arg, strlen(arg)))) {
				Co_SendKick(p->inst, p->key, COORD_DENY_KICK);
				if (p->conn >= 0) {
					Co_Deny(p->conn, COORD_DENY_KICK, "kicked");
				}
				Co_Log("coordinator: %s kicked\n", p->name);
			}
		}
		return 1;
	}
	return 0;
}

void Coord_PublicKey(const Coord *c, uint8_t pub[NET2_KEY_SIZE]) {
	(void)c;
	memcpy(pub, coord.pub, NET2_KEY_SIZE);
}

uint16_t Coord_Port(const Coord *c) {
	(void)c;
	return Net2_LocalPort(coord.host, NET2_AF_IPV4);
}

void Coord_Destroy(Coord *c) {
	if (!c) {
		return;
	}
	Co_SaveAll(1);
	if (coord.cfg.stopInstances) {
		for (int i = 0; i < coord.ninst; i++) {
			if (coord.inst[i].state != CI_DOWN) {
				Co_Stop(&coord.inst[i], "the coordinator ends");
			}
		}
		Co_WriteInstFile();
	}
	Net2_Destroy(coord.host);
	coord.host = NULL;
	Net2_Wipe(coord.seed, sizeof(coord.seed));
	Co_Log("coordinator: closed\n");
	coord_used = 0;
}
