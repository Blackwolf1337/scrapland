/*
 * watchprobe: watchers without the game for a running instance (ScrapC -instance), for load runs: each client says
 * hello, takes the snapshots into a NetRepView (as the watcher does, without proxies), acks them and sends a view point
 * that circles the welcome's view point (radius and phase per client). At the end one line per client: bytes and
 * messages received per second, messages not taken, entities held, and the totals.
 *
 *   watchprobe <address[:port]> [clients 1..256] [seconds] [rate bytes/s] [circle radius units]
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "scrap/net2.h"
#include "scrap/netrep.h"

#define PROBE_MAX 256
#define PROBE_APP_VERSION 1			/* INSTANCE_APP_VERSION */

typedef struct Probe {
	Net2Host *host;
	NetRepView *view;
	int up, welcomed, gone;
	float center[3];
	uint64_t bytes, msgs, notTaken;
	uint64_t lastAck;
	int taken;
} Probe;

static Probe g_probe[PROBE_MAX];

static uint64_t NowMs(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

/* a wait of the tool itself (not game code): the clients are polled every 2 ms */
static void Nap(void) {
	struct timespec ts = { 0, 2000000 };
	nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
	Net2Addr addr;
	if (argc < 2 || !Net2_ParseAddr(argv[1], NET2_DEFAULT_PORT, &addr)) {
		fprintf(stderr, "usage: watchprobe <address[:port]> [clients 1..%d] [seconds] [rate] [radius]\n", PROBE_MAX);
		return 2;
	}
	int n = argc > 2 ? atoi(argv[2]) : 1;
	int secs = argc > 3 ? atoi(argv[3]) : 30;
	uint32_t rate = argc > 4 ? (uint32_t)atoi(argv[4]) : NETREP_RATE_DEFAULT;
	float radius = argc > 5 ? (float)atof(argv[5]) : 20000.0f;
	n = n < 1 ? 1 : n > PROBE_MAX ? PROBE_MAX : n;
	uint64_t start = NowMs();
	for (int i = 0; i < n; i++) {
		Net2Config cfg;
		int err = 0;
		Net2_DefaultConfig(&cfg, 0);
		cfg.appVersion = PROBE_APP_VERSION;
		cfg.ipv4 = addr.family == NET2_AF_IPV4;
		cfg.ipv6 = addr.family == NET2_AF_IPV6;
		g_probe[i].host = Net2_Create(&cfg, &err);
		g_probe[i].view = NetRepCli_Create();
		if (!g_probe[i].host || !g_probe[i].view || Net2_Connect(g_probe[i].host, &addr, start) != NET2_OK) {
			fprintf(stderr, "client %d: no host (error %d)\n", i, err);
			return 1;
		}
	}
	uint64_t measureFrom = 0;
	uint64_t bytesAt[PROBE_MAX], msgsAt[PROBE_MAX];
	for (;;) {
		uint64_t now = NowMs();
		if (now - start >= (uint64_t)secs * 1000u) {
			break;
		}
		/* the measured part starts 5 s in, after the joins */
		if (!measureFrom && now - start >= 5000) {
			measureFrom = now;
			for (int i = 0; i < n; i++) {
				bytesAt[i] = g_probe[i].bytes;
				msgsAt[i] = g_probe[i].msgs;
			}
		}
		for (int i = 0; i < n; i++) {
			Probe *p = &g_probe[i];
			Net2Event ev;
			Net2_Poll(p->host, now);
			while (Net2_Receive(p->host, &ev)) {
				if (ev.type == NET2_EV_CONNECT) {
					uint8_t buf[16];
					int k = NetRep_WriteHello(buf, sizeof(buf), rate);
					Net2_Send(p->host, 0, 0, buf, k);
					p->up = 1;
				} else if (ev.type == NET2_EV_DISCONNECT) {
					fprintf(stderr, "client %d: disconnected (%s)\n", i, Net2_ReasonString(ev.reason));
					p->gone = 1;
				} else if (ev.type == NET2_EV_MESSAGE && ev.size > 0) {
					if (ev.channel == 0 && ev.data[0] == NETREP_MSG_WELCOME) {
						uint32_t tick;
						char level[128];
						if (NetRep_ReadWelcome(ev.data, ev.size, &tick, p->center, level, sizeof(level))) {
							p->welcomed = 1;
						}
					} else if (ev.channel == 2 && ev.data[0] == NETREP_MSG_SNAP && p->welcomed) {
						p->msgs++;
						p->bytes += (uint64_t)ev.size;
						if (NetRepCli_Message(p->view, ev.data, ev.size, NULL)) {
							p->taken = 1;
						} else {
							p->notTaken++;
						}
					}
				}
			}
			uint16_t newest;
			uint32_t bits;
			if (p->welcomed && (p->taken || now - p->lastAck >= 50) && NetRepCli_Ack(p->view, &newest, &bits)) {
				uint8_t buf[32];
				float a = (float)(now - start) / 1000.0f * 0.2f + (float)i * 6.2831853f / (float)n;
				float v[3] = { p->center[0] + radius * cosf(a), p->center[1], p->center[2] + radius * sinf(a) };
				int k = NetRep_WriteAck(buf, sizeof(buf), newest, bits, v);
				Net2_Send(p->host, 0, 2, buf, k);
				p->taken = 0;
				p->lastAck = now;
			}
			Net2_Flush(p->host, now);
		}
		Nap();
	}
	double span = measureFrom ? (double)(NowMs() - measureFrom) / 1000.0 : 1.0;
	uint64_t totalBytes = 0;
	int ok = 0;
	for (int i = 0; i < n; i++) {
		Probe *p = &g_probe[i];
		int held = 0;
		for (int s = 0; s < NETREP_MAX_ENTS; s++) {
			held += NetRepCli_State(p->view, s, NULL, NULL) != NULL;
		}
		uint64_t b = measureFrom ? p->bytes - bytesAt[i] : p->bytes;
		uint64_t m = measureFrom ? p->msgs - msgsAt[i] : p->msgs;
		totalBytes += b;
		ok += p->welcomed && !p->gone;
		printf("client %d: %.0f bytes/s, %.1f messages/s, %llu not taken, %d entities, %s\n", i, (double)b / span,
			(double)m / span, (unsigned long long)p->notTaken, held, p->gone ? "gone" : p->welcomed ? "ok" : "no welcome");
		Net2_Destroy(p->host);
		NetRepCli_Destroy(p->view);
	}
	printf("watchprobe: %d of %d clients ok, %.0f bytes/s in total (payload, without Net2 overhead)\n", ok, n,
		(double)totalBytes / span);
	return ok == n ? 0 : 1;
}
