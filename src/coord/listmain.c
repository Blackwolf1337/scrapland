/*
 * ScrapList: the list server of the worlds (include/scrap/coordlist.h). Not in the original.
 *
 *   ScrapList [-port:28090] [-bind:<IPv4|IPv6>] [-ttl:<s>] [-peraddr:<n>] [-conns:<n>] [-key:<seed file>]
 *
 * Coordinators register with heartbeats, clients ask for the listed worlds. -key keeps the identity of the list
 * server in a seed file (made when missing), else it has a new one at every start. Lines on the standard input:
 * status, quit. SIGINT and SIGTERM end it like quit.
 */
#ifndef _WIN32
#include <signal.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/coordlist.h"
#include "scrap/netrep.h"

#define LM_WAIT_MS 5				/* the network is read this often */
#define LM_CHANNEL 0
#define LM_QUERY_MS 5000			/* a client is closed this long after its pages */
#define LM_PROBES 8					/* server queries per pass at most */
#define LM_CONNS 256

typedef struct LmConn {
	int used;
	uint8_t nonce[COORD_CHALLENGE];
	uint64_t closeAt;				/* ms, 0 never (a coordinator) */
} LmConn;

static volatile int lm_quit;
static ListReg lm_reg;
static LmConn lm_conn[NET2_MAX_CONNS];

#ifndef _WIN32
static void Lm_Signal(int sig) {
	(void)sig;
	lm_quit = 1;
}
#endif

static void Lm_Log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void Lm_Log(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	fflush(stdout);
}

static void Lm_Status(void) {
	char addr[64], hex[2 * NET2_KEY_SIZE + 1];
	int n = 0;
	for (int i = 0; i < LIST_MAX; i++) {
		const ListEntry *e = &lm_reg.e[i];
		if (e->used) {
			Net2_AddrString(&e->w.addr, addr, sizeof(addr));
			Coord_Hex(e->w.key, NET2_KEY_SIZE, hex);
			Lm_Log("  %s at %s (%.8s): %s, %s, %u/%u%s\n", e->w.name, addr, hex, e->w.map, e->w.mode, e->w.players,
				e->w.maxPlayers, e->verified ? "" : ", not answered yet");
			n++;
		}
	}
	Lm_Log("list: %d worlds, %d listed\n", n, ListReg_Count(&lm_reg));
}

static void Lm_Beat(Net2Host *h, int conn, const uint8_t *data, int size, uint64_t now) {
	uint8_t key[NET2_KEY_SIZE], buf[8];
	ListBeat b;
	Net2Addr peer;
	NetRepBuf w;
	int isNew = 0;

	int result = Net2_ServerKey(h, key) && Net2_PeerAddr(h, conn, &peer) ?
		List_ReadBeat(data, size, lm_conn[conn].nonce, key, &b) : LIST_E_BAD;
	if (result == LIST_OK) {
		result = ListReg_Beat(&lm_reg, &b, lm_conn[conn].nonce, &peer, now, &isNew);
		lm_conn[conn].closeAt = 0;
	}
	if (isNew) {
		char addr[64];
		Net2_AddrString(&peer, addr, sizeof(addr));
		Lm_Log("list: %s from %s, port %u: waits for its answer\n", b.w.name, addr, b.w.addr.port);
	} else if (result != LIST_OK) {
		Lm_Log("list: a heartbeat on connection %d refused (%d)\n", conn, result);
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, LIST_MSG_ANSWER);
	NetRep_PutU8(&w, (uint32_t)result);
	NetRep_PutU16(&w, lm_reg.ttl / 1000u);
	Net2_Send(h, conn, LM_CHANNEL, buf, w.size);
}

static void Lm_Query(Net2Host *h, int conn, uint64_t now) {
	static uint8_t page[NET2_MAX_MESSAGE];
	int first = 0, total = ListReg_Count(&lm_reg);
	do {
		int count = 0;
		int n = ListReg_WritePage(&lm_reg, first, page, sizeof(page), &count);
		if (n <= 0 || Net2_Send(h, conn, LM_CHANNEL, page, n) != NET2_OK) {
			break;
		}
		first += count;
		if (!count) {
			break;
		}
	} while (first < total);
	lm_conn[conn].closeAt = now + LM_QUERY_MS;
}

static void Lm_Net(Net2Host *h, uint64_t now) {
	Net2Event ev;
	Net2_Poll(h, now);
	while (Net2_Receive(h, &ev)) {
		LmConn *c = ev.conn >= 0 && ev.conn < NET2_MAX_CONNS ? &lm_conn[ev.conn] : NULL;
		switch (ev.type) {
		case NET2_EV_CONNECT: {
			uint8_t buf[4 + COORD_CHALLENGE];
			NetRepBuf w;
			if (!c || !Net2_Random(c->nonce, sizeof(c->nonce))) {
				Net2_Disconnect(h, ev.conn, NET2_DR_LOCAL);
				break;
			}
			c->used = 1;
			c->closeAt = now + LM_QUERY_MS * 4;
			NetRepBuf_Init(&w, buf, sizeof(buf));
			NetRep_PutU8(&w, LIST_MSG_CHALLENGE);
			NetRep_PutU16(&w, LIST_VERSION);
			CoordMsg_PutBytes(&w, c->nonce, COORD_CHALLENGE);
			Net2_Send(h, ev.conn, LM_CHANNEL, buf, w.size);
			break;
		}
		case NET2_EV_DISCONNECT:
			if (c) {
				memset(c, 0, sizeof(*c));
			}
			break;
		case NET2_EV_MESSAGE:
			if (!c || !c->used || ev.channel != LM_CHANNEL || ev.size < 1) {
				break;
			}
			if (ev.data[0] == LIST_MSG_BEAT) {
				Lm_Beat(h, ev.conn, ev.data, ev.size, now);
			} else if (ev.data[0] == LIST_MSG_QUERY) {
				Lm_Query(h, ev.conn, now);
			}
			break;
		case NET2_EV_SERVERINFO: {
			int before = ListReg_Count(&lm_reg);
			if (ListReg_Answer(&lm_reg, &ev.addr, &ev.info) && ListReg_Count(&lm_reg) > before) {
				char addr[64];
				Net2_AddrString(&ev.addr, addr, sizeof(addr));
				Lm_Log("list: listed %s at %s (%s, %u/%u)\n", ev.info.name, addr, ev.info.mode, ev.info.players,
					ev.info.maxPlayers);
			}
			break;
		}
		}
	}
	Net2Addr a;
	for (int i = 0; i < LM_PROBES && ListReg_NextProbe(&lm_reg, now, &a); i++) {
		Net2_Query(h, &a, now);
	}
	int gone = ListReg_Expire(&lm_reg, now);
	if (gone) {
		Lm_Log("list: %d worlds expired, %d listed\n", gone, ListReg_Count(&lm_reg));
	}
	for (int i = 0; i < NET2_MAX_CONNS; i++) {
		if (lm_conn[i].used && lm_conn[i].closeAt && now >= lm_conn[i].closeAt) {
			Net2_Disconnect(h, i, NET2_DR_LOCAL);
			memset(&lm_conn[i], 0, sizeof(lm_conn[i]));
		}
	}
	Net2_Flush(h, now);
}

int main(int argc, char **argv) {
	Net2Config cfg;
	char line[128], bind[64] = "", keyPath[COORD_PATH] = "";
	uint8_t seed[NET2_SEED_SIZE];
	int ttl = LIST_TTL_SECS, perAddr = LIST_PER_ADDR, conns = LM_CONNS, port = COORD_LIST_PORT, err = 0;
	const char *v;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if ((v = CoordCli_Arg(a, "port"))) {
			port = atoi(v);
		} else if ((v = CoordCli_Arg(a, "bind"))) {
			snprintf(bind, sizeof(bind), "%s", v);
		} else if ((v = CoordCli_Arg(a, "ttl"))) {
			ttl = atoi(v);
		} else if ((v = CoordCli_Arg(a, "peraddr"))) {
			perAddr = atoi(v);
		} else if ((v = CoordCli_Arg(a, "conns"))) {
			conns = atoi(v);
		} else if ((v = CoordCli_Arg(a, "key"))) {
			snprintf(keyPath, sizeof(keyPath), "%s", v);
		} else {
			fprintf(stderr, "ScrapList: unknown argument %s\n", a);
			return 2;
		}
	}
	Net2_DefaultConfig(&cfg, 1);
	cfg.port = (uint16_t)port;
	cfg.appVersion = LIST_VERSION;
	cfg.maxConns = conns < 1 ? 1 : conns > NET2_MAX_CONNS ? NET2_MAX_CONNS : conns;
	if (!Net2_SetBind(&cfg, bind)) {
		fprintf(stderr, "ScrapList: -bind needs a numeric address, not '%s'\n", bind);
		return 2;
	}
	if (keyPath[0]) {
		int made;
		if (!Coord_LoadSeed(keyPath, seed, &made)) {
			fprintf(stderr, "ScrapList: cannot read or make %s\n", keyPath);
			return 1;
		}
		cfg.identitySeed = seed;
	}
	ListReg_Init(&lm_reg, (uint32_t)(ttl > 0 ? ttl : LIST_TTL_SECS) * 1000u, perAddr);
	Net2Host *h = Net2_Create(&cfg, &err);
	if (!h) {
		fprintf(stderr, "ScrapList: no Net2 server on port %d (error %d)\n", port, err);
		return 1;
	}
	uint8_t key[NET2_KEY_SIZE];
	char hex[2 * NET2_KEY_SIZE + 1];
	Net2_ServerKey(h, key);
	Coord_Hex(key, NET2_KEY_SIZE, hex);
	Lm_Log("list server: port %d on %s, worlds listed %d s after their heartbeat, key %s\n", port,
		bind[0] ? bind : "any address", ttl, hex);
#ifndef _WIN32
	signal(SIGINT, Lm_Signal);
	signal(SIGTERM, Lm_Signal);
#endif
	while (!lm_quit) {
		Lm_Net(h, CoordCli_Ms());
		int r = CoordCli_Line(line, sizeof(line), LM_WAIT_MS);
		if (r > 0) {
			if (!strcmp(line, "quit") || !strcmp(line, "exit")) {
				break;
			}
			if (!strcmp(line, "status")) {
				Lm_Status();
			} else if (line[0]) {
				Lm_Log("list: unknown command '%s' (status, quit)\n", line);
			}
		} else if (r < 0) {
			CoordCli_Wait(LM_WAIT_MS);
		}
	}
	Net2_Destroy(h);
	Net2_Wipe(seed, sizeof(seed));
	Lm_Log("list server: closed\n");
	return 0;
}
