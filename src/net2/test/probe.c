/*
 * A small Net2 client for a running server (the headless instance of ScrapC): asks the server for its info, then
 * connects N clients, sends each a reliable and an unreliable message and expects them back (the instance echoes
 * until it has a game), keeps the connections for the given seconds and closes them. With "drop" the clients wait
 * instead for the server to drop them (the instance does after 30 s) and report its reason.
 *
 *   net2_probe <address[:port]> [clients 0..64] [seconds] [app version] [drop]
 *
 * Prints one line per step; exit code 0 when the server answered and every client connected, got its echoes and
 * left (or was dropped) as asked. An unreliable message goes every 50 ms. The loop runs on the real clock without
 * waiting, as net2_loopback does.
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

#define PROBE_MAX 64
#define PROBE_QUERY_MS 2000
#define PROBE_CONNECT_MS 6000
#define PROBE_CLOSE_MS 1000

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

typedef struct Client {
	Net2Host *h;
	int connected, gone, reason;
	int relBack, unrelBack;
	uint64_t sentAt, rtt, unrelAt;
	Net2Stats st;					/* the last while connected */
} Client;

static Client g_cli[PROBE_MAX];

static void Events(Client *c, uint64_t now) {
	Net2Event ev;

	while (Net2_Receive(c->h, &ev)) {
		if (ev.type == NET2_EV_CONNECT) {
			c->connected = 1;
		} else if (ev.type == NET2_EV_DISCONNECT) {
			c->gone = 1;
			c->reason = ev.reason;
		} else if (ev.type == NET2_EV_MESSAGE && ev.size == 5 && !memcmp(ev.data, "probe", 5)) {
			if (ev.channel == 0 && !c->relBack++) {
				c->rtt = now - c->sentAt;
			} else if (ev.channel == 2) {
				c->unrelBack++;
			}
		}
	}
}

static int Query(const Net2Addr *addr, uint32_t app) {
	Net2Config cfg;
	Net2Event ev;
	int err = 0, ok = 0;

	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = app;
	cfg.ipv6 = addr->family == NET2_AF_IPV6;
	cfg.ipv4 = !cfg.ipv6;
	Net2Host *h = Net2_Create(&cfg, &err);
	if (!h) {
		printf("query: no client host (error %d)\n", err);
		return 0;
	}
	uint64_t t0 = Now();
	Net2_Query(h, addr, t0);
	for (uint64_t now = t0; !ok && now - t0 < PROBE_QUERY_MS; now = Now()) {
		Net2_Poll(h, now);
		while (Net2_Receive(h, &ev)) {
			if (ev.type == NET2_EV_SERVERINFO) {
				printf("query: name '%s' map '%s' mode '%s' players %d/%d app %u version %u flags %d ping %u ms\n",
					ev.info.name, ev.info.map, ev.info.mode, ev.info.players, ev.info.maxPlayers, ev.info.appVersion,
					ev.info.version, ev.info.flags, ev.ping);
				ok = 1;
			}
		}
		Net2_Flush(h, now);
	}
	if (!ok) {
		printf("query: no answer in %d ms\n", PROBE_QUERY_MS);
	}
	Net2_Destroy(h);
	return ok;
}

int main(int argc, char **argv) {
	Net2Addr addr;
	Net2Config cfg;
	int err = 0, fails = 0;

	if (argc < 2 || !Net2_ParseAddr(argv[1], NET2_DEFAULT_PORT, &addr)) {
		printf("usage: net2_probe <address[:port]> [clients 0..%d] [seconds] [app version] [drop]\n", PROBE_MAX);
		return 2;
	}
	int n = argc > 2 ? atoi(argv[2]) : 0;
	int seconds = argc > 3 ? atoi(argv[3]) : 2;
	uint32_t app = argc > 4 ? (uint32_t)strtoul(argv[4], NULL, 10) : 1;
	int drop = argc > 5 && !strcmp(argv[5], "drop");
	n = n < 0 ? 0 : n > PROBE_MAX ? PROBE_MAX : n;
	if (!Query(&addr, app)) {
		return 1;
	}
	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = app;
	cfg.ipv6 = addr.family == NET2_AF_IPV6;
	cfg.ipv4 = !cfg.ipv6;
	uint64_t now = Now();
	for (int i = 0; i < n; i++) {
		g_cli[i].h = Net2_Create(&cfg, &err);
		if (!g_cli[i].h || Net2_Connect(g_cli[i].h, &addr, now) != NET2_OK) {
			printf("client %d: no connect (error %d)\n", i, err);
			return 1;
		}
	}
	/* connect, echo, hold (or wait for the drop), then leave */
	uint64_t t0 = now, holdEnd = 0, closeEnd = 0;
	int up = 0;
	for (;;) {
		now = Now();
		int connected = 0, echoed = 0, gone = 0;
		for (int i = 0; i < n; i++) {
			Client *c = &g_cli[i];
			Net2_Poll(c->h, now);
			Events(c, now);
			if (c->connected && !c->sentAt) {
				c->sentAt = now;
				Net2_Send(c->h, 0, 0, "probe", 5);
			}
			if (c->connected && !c->gone && !closeEnd && now - c->unrelAt >= 50) {
				c->unrelAt = now;
				Net2_Send(c->h, 0, 2, "probe", 5);
				Net2_GetStats(c->h, 0, &c->st);
			}
			connected += c->connected;
			echoed += c->relBack > 0 && c->unrelBack > 0;
			gone += c->gone;
			Net2_Flush(c->h, now);
		}
		if (!up && echoed == n) {
			up = 1;
			holdEnd = now + (uint64_t)seconds * 1000;
			printf("clients: %d connected and echoed in %llu ms\n", n, (unsigned long long)(now - t0));
			if (n && !Query(&addr, app)) {
				fails++;
			}
		}
		if (!up && now - t0 > PROBE_CONNECT_MS) {
			printf("clients: %d of %d connected, %d echoed after %d ms\n", connected, n, echoed, PROBE_CONNECT_MS);
			return 1;
		}
		if (up && drop && gone == n) {
			break;
		}
		if (up && !drop && !closeEnd && now >= holdEnd) {
			for (int i = 0; i < n; i++) {
				Net2_Disconnect(g_cli[i].h, 0, 0);
			}
			closeEnd = now + PROBE_CLOSE_MS;
		}
		if (closeEnd && now >= closeEnd) {
			break;
		}
		if (up && drop && now >= holdEnd + 5000) {
			printf("clients: %d of %d dropped by the server in time\n", gone, n);
			fails++;
			break;
		}
	}
	for (int i = 0; i < n; i++) {
		Client *c = &g_cli[i];
		Net2Stats st = c->st;
		printf("client %d: echo rtt %llu ms, %d unreliable echoes, %llu packets sent, %llu received", i,
			(unsigned long long)c->rtt, c->unrelBack, (unsigned long long)st.packetsSent,
			(unsigned long long)st.packetsRecv);
		if (drop) {
			printf(", dropped: %s (%d)", c->gone ? Net2_ReasonString(c->reason) : "no", c->reason);
			fails += !c->gone;
		}
		printf("\n");
		Net2_Destroy(c->h);
	}
	if (n && !drop && Query(&addr, app) == 0) {
		fails++;
	}
	return fails ? 1 : 0;
}
