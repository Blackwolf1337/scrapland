/*
 * Net2 over real UDP sockets on the loopback: one server on 127.0.0.1 and ::1 port 29000, N clients (even ones
 * IPv4, odd ones IPv6) on ports 29001.., all in this process, with a filter that drops a share of the datagrams
 * both ways. The clients send input (unreliable) every frame and reliable messages up to 50 KB; the server sends
 * snapshots (sequenced) and reliable messages to every client. Then everything is checked, the clients
 * disconnect, and the server must see them go.
 *
 *   net2_loopback [clients 1..16] [seconds] [loss percent]
 *
 * The loop runs frames on the real clock without waiting (a frame per millisecond tick, the loop spins between).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "scrap/net2.h"
#include "testmsg.h"

#define LB_PORT 29000
#define LB_MAX_CLIENTS 16
#define LB_INPUT_IDS 65536

static uint64_t Now(void) {
#ifdef _WIN32
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&c);
	return (uint64_t)(c.QuadPart / (f.QuadPart / 1000));
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

static uint32_t g_rng = 0x9e3779b9;
static int g_loss;

static int Filter(void *ctx, int outgoing, const Net2Addr *addr, const uint8_t *data, int size) {
	(void)ctx;
	(void)addr;
	(void)data;
	(void)size;
	if (!outgoing) {
		return 1;					/* every datagram passes the filter of its sender once */
	}
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 17;
	g_rng ^= g_rng << 5;
	return (int)(g_rng % 100) >= g_loss;
}

typedef struct Side {
	uint32_t relSent, relRecv;
	int relBad;
	uint32_t seqLast;
	int seqBad, seqCount;
	uint32_t inputSent;
	int inputDup, inputCount;
	int connected, disconnects, reason;
	uint64_t relTime;
} Side;

static Side g_cli[LB_MAX_CLIENTS];		/* what client i sent and got */
static Side g_srv[LB_MAX_CLIENTS];		/* what the server sent to and got from client i */
static int g_srvConn[LB_MAX_CLIENTS];	/* server connection of client i, -1 none */
static uint8_t g_inputSeen[LB_MAX_CLIENTS][LB_INPUT_IDS / 8];
static uint8_t g_buf[NET2_MAX_MESSAGE];
static Net2Stats g_stats[LB_MAX_CLIENTS];	/* of each client, before it disconnects */
static int g_infos;
static int g_have6;

static int ClientOfConn(int conn) {
	for (int i = 0; i < LB_MAX_CLIENTS; i++) {
		if (g_srvConn[i] == conn) {
			return i;
		}
	}
	return -1;
}

static void ServerEvents(Net2Host *srv, Net2Host **cli, int n) {
	Net2Event ev;

	while (Net2_Receive(srv, &ev)) {
		int i = ClientOfConn(ev.conn);
		if (ev.type == NET2_EV_CONNECT) {
			/* which client: by its port */
			for (int k = 0; k < n; k++) {
				if (ev.addr.port == Net2_LocalPort(cli[k], k % 2 && g_have6 ? NET2_AF_IPV6 : NET2_AF_IPV4)) {
					g_srvConn[k] = ev.conn;
					g_srv[k].connected++;
				}
			}
		} else if (ev.type == NET2_EV_DISCONNECT && i >= 0) {
			g_srv[i].disconnects++;
			g_srv[i].reason = ev.reason;
			g_srvConn[i] = -1;
		} else if (ev.type == NET2_EV_MESSAGE && i >= 0) {
			Side *s = &g_srv[i];
			if (ev.channel == 0) {
				s->relBad += !Msg_Check(ev.data, ev.size, s->relRecv);
				s->relRecv++;
			} else if (ev.channel == 2 && ev.size == 4) {
				uint32_t id = Msg_U32(ev.data) % LB_INPUT_IDS;
				if (g_inputSeen[i][id >> 3] & (1 << (id & 7))) {
					s->inputDup++;
				}
				g_inputSeen[i][id >> 3] |= (uint8_t)(1 << (id & 7));
				s->inputCount++;
			}
		}
	}
}

static void ClientEvents(Net2Host *h, int i) {
	Net2Event ev;
	Side *s = &g_cli[i];

	while (Net2_Receive(h, &ev)) {
		if (ev.type == NET2_EV_CONNECT) {
			s->connected++;
		} else if (ev.type == NET2_EV_DISCONNECT) {
			s->disconnects++;
			s->reason = ev.reason;
		} else if (ev.type == NET2_EV_SERVERINFO) {
			g_infos++;
		} else if (ev.type == NET2_EV_MESSAGE && ev.channel == 0) {
			s->relBad += !Msg_Check(ev.data, ev.size, s->relRecv);
			s->relRecv++;
		} else if (ev.type == NET2_EV_MESSAGE && ev.channel == 1 && ev.size == 4) {
			uint32_t v = Msg_U32(ev.data);
			s->seqBad += v <= s->seqLast;
			s->seqLast = v;
			s->seqCount++;
		}
	}
}

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 8;
	int seconds = argc > 2 ? atoi(argv[2]) : 3;
	g_loss = argc > 3 ? atoi(argv[3]) : 10;
	Net2Host *cli[LB_MAX_CLIENTS];
	Net2Config cfg;
	Net2Addr srv4, srv6;
	int err, fails = 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	if (n < 1 || n > LB_MAX_CLIENTS || seconds < 1 || g_loss < 0 || g_loss > 50) {
		printf("usage: net2_loopback [clients 1..%d] [seconds] [loss 0..50]\n", LB_MAX_CLIENTS);
		return 2;
	}
	Net2_DefaultConfig(&cfg, 1);
	cfg.port = LB_PORT;
	cfg.maxConns = n;
	cfg.appVersion = 1;
	cfg.filter = Filter;
	Net2_ParseAddr("127.0.0.1", 0, &cfg.bind4);
	Net2_ParseAddr("::1", 0, &cfg.bind6);
	Net2Host *srv = Net2_Create(&cfg, &err);
	if (!srv) {
		printf("server: Net2_Create failed (%d)\n", err);
		return 1;
	}
	int have6 = g_have6 = Net2_LocalPort(srv, NET2_AF_IPV6) == LB_PORT;
	printf("server on 127.0.0.1:%u%s, %d clients, %d s, %d%% loss each way\n", Net2_LocalPort(srv, NET2_AF_IPV4),
		have6 ? " and [::1]" : " (no IPv6)", n, seconds, g_loss);
	Net2ServerInfo info;
	memset(&info, 0, sizeof(info));
	snprintf(info.name, sizeof(info.name), "loopback");
	snprintf(info.map, sizeof(info.map), "DownTown");
	info.maxPlayers = (uint8_t)n;
	Net2_SetServerInfo(srv, &info);

	Net2_ParseAddr("127.0.0.1", LB_PORT, &srv4);
	Net2_ParseAddr("::1", LB_PORT, &srv6);
	uint64_t now = Now();
	for (int i = 0; i < n; i++) {
		int v6 = i % 2 && have6;
		Net2_DefaultConfig(&cfg, 0);
		cfg.port = (uint16_t)(LB_PORT + 1 + i);
		cfg.ipv4 = !v6;
		cfg.ipv6 = v6;
		cfg.appVersion = 1;
		cfg.filter = Filter;
		Net2_ParseAddr(v6 ? "::1" : "127.0.0.1", 0, v6 ? &cfg.bind6 : &cfg.bind4);
		cli[i] = Net2_Create(&cfg, &err);
		if (!cli[i] || Net2_Connect(cli[i], v6 ? &srv6 : &srv4, now) != NET2_OK) {
			printf("client %d: create or connect failed (%d)\n", i, err);
			return 1;
		}
		g_srvConn[i] = -1;
	}
	uint64_t start = now, last = 0, snap = 0, chat = 0, query = 0;
	uint64_t sendEnd = start + (uint64_t)seconds * 1000, end = sendEnd + 5000;
	uint32_t snapCount = 0;
	int closing = 0;
	for (;;) {
		now = Now();
		if (now == last) {
			continue;
		}
		last = now;
		int sending = now < sendEnd;

		Net2_Poll(srv, now);
		ServerEvents(srv, cli, n);
		if (sending && now - snap >= 50) {
			snap = now;
			snapCount++;
			for (int i = 0; i < n; i++) {
				if (g_srvConn[i] >= 0) {
					uint8_t s[4];
					Msg_PutU32(s, snapCount);
					Net2_Send(srv, g_srvConn[i], 1, s, 4);
				}
			}
		}
		if (sending && now - chat >= 150) {
			chat = now;
			for (int i = 0; i < n; i++) {
				Side *s = &g_srv[i];
				int size = Msg_Reliable(g_buf, s->relSent, 20 + (int)(s->relSent % 9) * 300);
				if (g_srvConn[i] >= 0 && Net2_Send(srv, g_srvConn[i], 0, g_buf, size) == NET2_OK) {
					s->relSent++;
				}
			}
		}
		Net2_Flush(srv, now);

		if (sending && !g_infos && now - query >= 500) {
			query = now;
			Net2_Query(cli[0], &srv4, now);
		}
		for (int i = 0; i < n; i++) {
			Side *s = &g_cli[i];
			Net2_Poll(cli[i], now);
			ClientEvents(cli[i], i);
			if (sending && Net2_IsConnected(cli[i], 0)) {
				uint8_t in[4];
				Msg_PutU32(in, s->inputSent++);
				Net2_Send(cli[i], 0, 2, in, 4);
				if (now - s->relTime >= 100) {
					s->relTime = now;
					int size = s->relSent % 10 == 5 ? 50000 : 8 + (int)(s->relSent % 13) * 97;
					size = Msg_Reliable(g_buf, s->relSent, size);
					if (Net2_Send(cli[i], 0, 0, g_buf, size) == NET2_OK) {
						s->relSent++;
					}
				}
			}
			Net2_Flush(cli[i], now);
		}

		if (!sending && !closing) {
			int done = 1;
			for (int i = 0; i < n; i++) {
				done &= g_srv[i].relRecv == g_cli[i].relSent && g_cli[i].relRecv == g_srv[i].relSent;
			}
			if (done || now >= end - 2000) {
				for (int i = 0; i < n; i++) {
					Net2_GetStats(cli[i], 0, &g_stats[i]);
					Net2_Disconnect(cli[i], 0, NET2_DR_APP + 1);
				}
				closing = 1;
			}
		}
		if (closing) {
			int gone = 1;
			for (int i = 0; i < n; i++) {
				gone &= g_srv[i].disconnects && g_cli[i].disconnects;
			}
			if (gone) {
				break;
			}
		}
		if (now >= end) {
			printf("time out at the end\n");
			fails++;
			break;
		}
	}

	for (int i = 0; i < n; i++) {
		Side *c = &g_cli[i], *s = &g_srv[i];
		printf("client %2d %s: connects %d/%d, reliable up %u/%u down %u/%u, snapshots %d, input %d/%u, "
			"disconnect %s\n", i, i % 2 && have6 ? "v6" : "v4", c->connected, s->connected, s->relRecv, c->relSent,
			c->relRecv, s->relSent, c->seqCount, s->inputCount, c->inputSent, Net2_ReasonString(s->reason));
		int ok = c->connected == 1 && s->connected == 1 && s->relRecv == c->relSent && c->relRecv == s->relSent
			&& !c->relBad && !s->relBad && !c->seqBad && !s->inputDup && c->seqCount > (int)c->inputSent / 100
			&& s->inputCount > (int)c->inputSent / 2 && c->relSent >= (uint32_t)seconds * 4
			&& (s->reason == NET2_DR_APP + 1 || s->reason == NET2_DR_TIMEOUT) && c->reason == NET2_DR_APP + 1;
		Net2Stats *st = &g_stats[i];
		printf("          rtt %u ms (var %u), loss %.3f, sent %llu packets %llu bytes, lost %llu, bad %u, dropped %u\n",
			st->rtt, st->rttVar, (double)st->loss, (unsigned long long)st->packetsSent,
			(unsigned long long)st->bytesSent, (unsigned long long)st->packetsLost, st->bad, st->dropped);
		if (!ok) {
			printf("client %d: FAIL\n", i);
			fails++;
		}
	}
	printf("query answers: %d\n", g_infos);
	fails += g_infos < 1;
	for (int i = 0; i < n; i++) {
		Net2_Destroy(cli[i]);
	}
	Net2_Destroy(srv);
	printf("net2_loopback: %s (%.1f s)\n", fails ? "FAILED" : "ok", (double)(Now() - start) / 1000.0);
	return fails != 0;
}
