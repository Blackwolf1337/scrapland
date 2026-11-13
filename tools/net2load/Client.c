/*
 * net2load fake clients: N client hosts in one process, each with its own address (127.x.y.z, so the ports stay
 * in one small range), its own identity key pair from the seed and its own frame phase. A client connects, joins
 * (signed JOIN, WELCOME), then sends input every frame and chat now and then until the measured window ends; then
 * QUIT, and the server's STATS (reliable, so after every earlier message) is compared with what arrived. The
 * filter hook of every host drops the given share of datagrams in both directions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "load.h"
#include "monocypher.h"

enum {
	CS_IDLE,
	CS_CONNECTING,
	CS_CONNECTED,					/* JOIN sent */
	CS_JOINED,
	CS_QUITTING,					/* QUIT sent, waits for STATS */
	CS_CLOSING,						/* Net2_Disconnect, waits for the event */
	CS_DONE,
	CS_FAILED
};

typedef struct Client {
	Net2Host *h;
	int state;
	uint32_t index;
	uint8_t sk[64], pk[32];
	uint32_t rng, lossRng, lossLimit;
	uint64_t connectStart, nextFrame, nextChat, closeTime, finTime;
	uint32_t tick, latTick;
	uint8_t frames[LOAD_REDUNDANT][LOAD_FRAME];
	int numFrames;
	uint16_t yaw;
	uint32_t inputsSent, chatSeq, snaps, events, eventBad, relays, sendFull;
	uint32_t snapRecent[16];		/* numbers of the last snapshots, by snaps % 16 */
	int connected, joined, welcomeBad, haveStats, quitSent, dropped, reason;
	uint32_t filtered;
	Net2Stats base, fin;
	uint32_t stats[LOAD_STAT_COUNT];
} Client;

typedef struct Clients {
	const LoadOpts *o;
	Client *c;
	int n;
	uint64_t now, period, markTime;
	uint32_t markDrops;
	LoadHist connect, join, lat, rtt;
	uint8_t buf[NET2_MAX_MESSAGE];
	FILE *timeline;
} Clients;

static Clients s_cl;

static int Client_Filter(void *ctx, int outgoing, const Net2Addr *addr, const uint8_t *data, int size) {
	Client *c = ctx;

	(void)outgoing;
	(void)addr;
	(void)data;
	(void)size;
	if (!c->lossLimit || Load_Random(&c->lossRng) >= c->lossLimit) {
		return 1;
	}
	c->filtered++;
	return 0;
}

static void Client_Send(Client *c, int channel, const uint8_t *data, int size, uint32_t *count) {
	int e = Net2_Send(c->h, 0, channel, data, size);
	if (e == NET2_OK) {
		if (count) {
			(*count)++;
		}
	} else if (e == NET2_E_FULL) {
		c->sendFull++;
	}
}

static void Client_Join(Clients *cs, Client *c) {
	uint8_t key[NET2_KEY_SIZE], text[LOAD_JOIN_TEXT], sig[64];
	Net2Writer w;

	Net2_ServerKey(c->h, key);
	Load_JoinText(text, c->index, key);
	crypto_eddsa_sign(sig, c->sk, text, sizeof(text));
	Net2W_Init(&w, cs->buf, LOAD_JOIN_SIZE);
	Net2W_U8(&w, LM_JOIN);
	Net2W_U32(&w, c->index);
	Net2W_Bytes(&w, c->pk, 32);
	Net2W_Bytes(&w, sig, 64);
	Client_Send(c, LOAD_CH_RELIABLE, cs->buf, w.pos, NULL);
}

static void Client_Message(Clients *cs, Client *c, const Net2Event *ev) {
	Net2Reader r;

	Net2R_Init(&r, ev->data, ev->size);
	int type = (int)Net2R_U8(&r);
	/* after STATS nothing more is counted: what arrives later was sent after the server's counts */
	if (c->state == CS_CLOSING) {
		return;
	}
	if (type == LM_SNAP && ev->channel == LOAD_CH_SEQUENCED) {
		Net2R_U32(&r);				/* server tick */
		c->snapRecent[c->snaps % 16] = Net2R_U32(&r);
		uint32_t lastTick = Net2R_U32(&r);
		uint32_t lastTime = Net2R_U32(&r);
		c->snaps++;
		/* the time from an input to the first snapshot that confirms it */
		if (!r.bad && lastTick > c->latTick) {
			c->latTick = lastTick;
			Hist_Add(&cs->lat, (uint32_t)(cs->now / 1000) - lastTime);
		}
		return;
	}
	if (ev->channel != LOAD_CH_RELIABLE) {
		return;
	}
	if (type == LM_WELCOME && c->state == CS_CONNECTED) {
		if (ev->size != cs->o->welcomeSize || !Net2R_U8(&r)) {
			c->welcomeBad = 1;
			return;
		}
		c->joined = 1;
		c->state = CS_JOINED;
		Hist_Add(&cs->join, (uint32_t)((cs->now - c->connectStart) / 1000));
		c->nextChat = cs->now + (uint64_t)(cs->o->chatEvery * 1e6 * (Load_Random(&c->rng) % 1000) / 1000.0);
	} else if (type == LM_EVENT) {
		c->eventBad += Net2R_U32(&r) != c->events;
		c->events++;
	} else if (type == LM_RELAY) {
		c->relays++;
	} else if (type == LM_STATS && c->state == CS_QUITTING) {
		for (int i = 0; i < LOAD_STAT_COUNT; i++) {
			c->stats[i] = Net2R_U32(&r);
		}
		c->haveStats = !r.bad;
		/*
		 * one Net2_Poll can bring the packet with STATS and later ones, and Net2_Receive hands out the snapshots
		 * before the reliable messages: drop the snapshots the server sent after its count
		 */
		uint32_t later = 0;
		while (later < 16 && later < c->snaps && c->snapRecent[(c->snaps - 1 - later) % 16] > c->stats[LS_SNAPS]) {
			later++;
		}
		c->snaps -= later;
		Net2_GetStats(c->h, 0, &c->fin);
		c->finTime = cs->now;
		Net2_Disconnect(c->h, 0, 0);
		c->state = CS_CLOSING;
		c->closeTime = cs->now;
	}
}

static void Client_Events(Clients *cs, Client *c) {
	Net2Event ev;

	while (Net2_Receive(c->h, &ev)) {
		if (ev.type == NET2_EV_CONNECT) {
			c->connected = 1;
			c->state = CS_CONNECTED;
			Hist_Add(&cs->connect, (uint32_t)((cs->now - c->connectStart) / 1000));
			Client_Join(cs, c);
		} else if (ev.type == NET2_EV_DISCONNECT) {
			c->reason = ev.reason;
			if (c->state == CS_CLOSING) {
				c->state = CS_DONE;
			} else {
				c->dropped = c->connected;
				c->state = CS_FAILED;
			}
		} else if (ev.type == NET2_EV_MESSAGE && ev.size > 0) {
			Client_Message(cs, c, &ev);
		}
	}
}

/* a scripted input: buttons and turns change every few ticks, from the client's own random numbers */
static void Client_Input(Clients *cs, Client *c) {
	Net2Writer w;

	if (c->tick % 8 == 0) {
		c->yaw += (uint16_t)(Load_Random(&c->rng) % 4096);
	}
	c->tick++;
	memmove(c->frames[1], c->frames[0], LOAD_FRAME * (LOAD_REDUNDANT - 1));
	Net2W_Init(&w, c->frames[0], LOAD_FRAME);
	Net2W_U32(&w, c->tick);
	Net2W_U32(&w, (uint32_t)(cs->now / 1000));
	Net2W_U16(&w, (c->tick / 15) % 4 == 0 ? 1u : 0u);	/* fire half a second of every two */
	Net2W_U16(&w, c->yaw);
	Net2W_U16(&w, (uint16_t)(c->tick * 37));
	Net2W_U8(&w, 127);
	Net2W_U8(&w, (uint32_t)(int8_t)((c->tick / 30) % 2 ? 64 : -64));
	if (c->numFrames < LOAD_REDUNDANT) {
		c->numFrames++;
	}
	Net2W_Init(&w, cs->buf, cs->o->inputSize);
	Net2W_U8(&w, LM_INPUT);
	Net2W_U8(&w, (uint32_t)c->numFrames);
	Net2W_Bytes(&w, c->frames, LOAD_FRAME * c->numFrames);
	Net2W_Zeros(&w, cs->o->inputSize - w.pos);
	Client_Send(c, LOAD_CH_UNRELIABLE, cs->buf, w.pos, &c->inputsSent);

	if (cs->o->chatEvery > 0 && cs->now >= c->nextChat) {
		Net2W_Init(&w, cs->buf, cs->o->chatSize);
		Net2W_U8(&w, LM_CHAT);
		Net2W_U32(&w, c->chatSeq);
		for (int i = w.pos; i < cs->o->chatSize; i++) {
			cs->buf[i] = (uint8_t)('a' + (i + (int)c->chatSeq) % 26);
		}
		Client_Send(c, LOAD_CH_RELIABLE, cs->buf, cs->o->chatSize, &c->chatSeq);
		c->nextChat = cs->now + (uint64_t)(cs->o->chatEvery * 1e6 * (500 + Load_Random(&c->rng) % 1000) / 1000.0);
	}
}

static void Client_Frame(Clients *cs, Client *c) {
	uint64_t ms = cs->now / 1000;

	Net2_Poll(c->h, ms);
	Client_Events(cs, c);
	if (c->state == CS_JOINED) {
		Client_Input(cs, c);
	} else if (c->state == CS_QUITTING && !c->quitSent) {
		uint8_t q = LM_QUIT;
		Client_Send(c, LOAD_CH_RELIABLE, &q, 1, NULL);
		c->quitSent = 1;
	}
	Net2_Flush(c->h, ms);
}

static int Client_Create(Clients *cs, Client *c, int i, const Net2Addr *server, const Net2Addr *base,
	const uint8_t *pin) {
	const LoadOpts *o = cs->o;
	Net2Config cfg;
	Net2Addr at = *base;
	uint8_t seed[32];
	int err;

	c->index = (uint32_t)i;
	Load_Seed32(seed, "client", o->seed, c->index);
	crypto_eddsa_key_pair(c->sk, c->pk, seed);
	c->rng = (uint32_t)(o->seed * 2654435761u) ^ (c->index * 40503u + 1);
	c->lossRng = c->rng ^ 0xa5a5a5a5u;
	c->lossLimit = (uint32_t)(o->loss / 100.0 * 4294967295.0);
	Net2_DefaultConfig(&cfg, 0);
	if (server->family == NET2_AF_IPV4) {
		/* the next addresses of the base, all on the loopback for 127.0.0.0/8 */
		uint32_t a = ((uint32_t)base->ip[0] << 24 | (uint32_t)base->ip[1] << 16 | (uint32_t)base->ip[2] << 8
			| base->ip[3]) + (uint32_t)i;
		at.ip[0] = (uint8_t)(a >> 24);
		at.ip[1] = (uint8_t)(a >> 16);
		at.ip[2] = (uint8_t)(a >> 8);
		at.ip[3] = (uint8_t)a;
		cfg.port = (uint16_t)o->clientPort;
		cfg.bind4 = at;
		cfg.ipv6 = 0;
	} else {
		/* only ::1 is the IPv6 loopback: one port per client */
		cfg.port = o->clientPort ? (uint16_t)(o->clientPort + i) : 0;
		cfg.bind6 = at;
		cfg.ipv4 = 0;
	}
	cfg.appVersion = o->appVersion;
	cfg.rate = o->rate;
	cfg.serverKey = pin;
	cfg.filter = Client_Filter;
	cfg.filterCtx = c;
	cfg.alloc = Load_Alloc;
	cfg.free = Load_Free;
	c->h = Net2_Create(&cfg, &err);
	if (!c->h) {
		char s[80];
		Net2_AddrString(&at, s, sizeof(s));
		fprintf(stderr, "net2load: client %d on %s port %u: error %d\n", i, s, cfg.port, err);
		return 0;
	}
	return 1;
}

static void Client_Progress(Clients *cs, uint64_t start, const char *phase) {
	int counts[CS_FAILED + 1] = { 0 };
	uint64_t rttSum = 0;
	uint32_t rttN = 0, rss, hwm;
	Net2Stats st;

	for (int i = 0; i < cs->n; i++) {
		Client *c = &cs->c[i];
		counts[c->state]++;
		if (c->state == CS_JOINED && Net2_GetStats(c->h, 0, &st)) {
			rttSum += st.rtt;
			rttN++;
		}
	}
	Load_Memory(&rss, &hwm);
	double t = (double)(cs->now - start) / 1e6;
	uint32_t rtt = rttN ? (uint32_t)(rttSum / rttN) : 0;
	printf("cli t=%.0f %s: connecting=%d joining=%d joined=%d quitting=%d done=%d failed=%d rtt=%u "
		"lat_p99=%u rss=%uKB net2=%zuKB\n", t, phase, counts[CS_IDLE] + counts[CS_CONNECTING], counts[CS_CONNECTED],
		counts[CS_JOINED], counts[CS_QUITTING] + counts[CS_CLOSING], counts[CS_DONE], counts[CS_FAILED], rtt,
		Hist_Pct(&cs->lat, 99), rss, load_memUsed / 1024);
	fflush(stdout);
	if (cs->timeline) {
		fprintf(cs->timeline, "cli,%.1f,%s,%d,%d,%d,%u,%u,%u,%zu\n", t, phase, counts[CS_JOINED],
			counts[CS_FAILED], counts[CS_DONE], rtt, Hist_Pct(&cs->lat, 99), rss, load_memUsed / 1024);
		fflush(cs->timeline);
	}
}

/* the summary over all clients, and the CSV line */
static void Client_Report(Clients *cs, uint64_t cpuUs, uint64_t wallUs) {
	const LoadOpts *o = cs->o;
	uint32_t connected = 0, joined = 0, failedConnect = 0, dropped = 0, noStats = 0, welcomeBad = 0;
	uint32_t eventBad = 0, eventMiss = 0, relayMiss = 0, chatBad = 0, sendFull = 0, srvSendFull = 0;
	uint64_t inSent = 0, inGot = 0, snapSent = 0, snapGot = 0, filtered = 0, resent = 0, srvResent = 0;
	uint64_t pktOut = 0, pktIn = 0, bytesOut = 0, bytesIn = 0, srvPktIn = 0, srvPktOut = 0, srvKbIn = 0;
	uint64_t srvKbOut = 0, srvDropped = 0, srvSockDrops = 0;
	double winSum = 0, lossSum = 0;
	uint32_t reasons[NET2_DR_APP + 1] = { 0 };
	const uint32_t *g = NULL;
	uint32_t rss, hwm;

	Hist_Clear(&cs->rtt);
	for (int i = 0; i < cs->n; i++) {
		Client *c = &cs->c[i];
		connected += (uint32_t)c->connected;
		joined += (uint32_t)c->joined;
		failedConnect += (uint32_t)!c->connected;
		dropped += (uint32_t)c->dropped;
		welcomeBad += (uint32_t)c->welcomeBad;
		sendFull += c->sendFull;
		filtered += c->filtered;
		if (c->state == CS_FAILED) {
			reasons[c->reason > NET2_DR_APP ? NET2_DR_APP : c->reason]++;
		}
		if (!c->joined) {
			continue;
		}
		if (!c->haveStats) {
			noStats++;
			continue;
		}
		const uint32_t *v = c->stats;
		if (!g || v[LS_WINDOW_MS] > g[LS_WINDOW_MS]) {
			g = v;
		}
		inSent += c->inputsSent;
		inGot += v[LS_INPUTS];
		snapSent += v[LS_SNAPS];
		snapGot += c->snaps;
		eventBad += c->eventBad;
		eventMiss += c->events != v[LS_EVENTS];
		relayMiss += c->relays != v[LS_RELAYS];
		chatBad += v[LS_CHATBAD] + (v[LS_CHATS] != c->chatSeq);
		srvSendFull += v[LS_SENDFULL];
		srvResent += v[LS_RESENT];
		srvPktIn += v[LS_PKT_IN];
		srvPktOut += v[LS_PKT_OUT];
		srvKbIn += v[LS_KB_IN];
		srvKbOut += v[LS_KB_OUT];
		srvDropped += v[LS_DROPPED];
		Hist_Add(&cs->rtt, c->fin.rtt);
		lossSum += (double)c->fin.loss;
		/* traffic of the measured window: from MARK to the STATS answer */
		double win = (double)(c->finTime - cs->markTime) / 1e6;
		if (win > 0) {
			winSum += win;
			pktOut += c->fin.packetsSent - c->base.packetsSent;
			pktIn += c->fin.packetsRecv - c->base.packetsRecv;
			bytesOut += c->fin.bytesSent - c->base.bytesSent;
			bytesIn += c->fin.bytesRecv - c->base.bytesRecv;
			resent += c->fin.resent - c->base.resent;
		}
	}
	uint32_t withStats = joined - noStats;
	double win = withStats ? winSum / withStats : 0;		/* average window of a client */
	double perC = withStats && win > 0 ? 1.0 / (withStats * win) : 0;
	double srvWin = g ? g[LS_WINDOW_MS] / 1000.0 : 0;
	double inLoss = inSent ? 100.0 * (1.0 - (double)inGot / (double)inSent) : 0;
	double snapLoss = snapSent ? 100.0 * (1.0 - (double)snapGot / (double)snapSent) : 0;
	if (g) {
		srvSockDrops = g[LS_SOCK_DROPS];
	}
	uint32_t cliSockDrops = Load_SockDrops((uint16_t)o->clientPort) - cs->markDrops;
	uint32_t integrity = eventBad + eventMiss + relayMiss + chatBad + welcomeBad;
	Load_Memory(&rss, &hwm);

	printf("== %s: %d clients, loss %.1f%%, window %.1f s\n", o->label, cs->n, o->loss, win);
	printf("connect: %u/%d, failed %u, joined %u, dropped %u, no stats %u; connect ms p50=%u p90=%u p99=%u max=%u; "
		"join ms p50=%u p99=%u max=%u\n", connected, cs->n, failedConnect, joined, dropped, noStats,
		Hist_Pct(&cs->connect, 50), Hist_Pct(&cs->connect, 90), Hist_Pct(&cs->connect, 99), cs->connect.max,
		Hist_Pct(&cs->join, 50), Hist_Pct(&cs->join, 99), cs->join.max);
	printf("rtt ms avg=%u p99=%u max=%u; input->snapshot ms p50=%u p99=%u max=%u; net2 loss c>s %.2f%%\n",
		Hist_Avg(&cs->rtt), Hist_Pct(&cs->rtt, 99), cs->rtt.max, Hist_Pct(&cs->lat, 50), Hist_Pct(&cs->lat, 99),
		cs->lat.max, withStats ? 100.0 * lossSum / withStats : 0.0);
	printf("lost after redundancy: input %.2f%%, snapshots %.2f%%; filter dropped %llu datagrams; socket drops "
		"server %llu clients %u; net2 dropped on the server %llu\n", inLoss, snapLoss, (unsigned long long)filtered,
		(unsigned long long)srvSockDrops, cliSockDrops, (unsigned long long)srvDropped);
	printf("per client: c>s %.0f B/s %.1f pkt/s, s>c %.0f B/s %.1f pkt/s; resent c>s %llu s>c %llu; send full "
		"client %u server %u\n", (double)bytesOut * perC, (double)pktOut * perC, (double)bytesIn * perC,
		(double)pktIn * perC, (unsigned long long)resent, (unsigned long long)srvResent, sendFull, srvSendFull);
	printf("reliable integrity errors %u (event order %u, event count %u, relay count %u, chat %u, welcome %u)\n",
		integrity, eventBad, eventMiss, relayMiss, chatBad, welcomeBad);
	if (g) {
		printf("server: pkt/s in=%.0f out=%.0f, KB/s in=%.0f out=%.0f, tick cpu us avg=%u p50=%u p99=%u max=%u, "
			"wall p99=%u max=%u, late %u of %u, cpu %.1f%%, rss %uKB hwm %uKB, net2 %uKB peak %uKB\n",
			srvWin > 0 ? (double)srvPktIn / srvWin : 0, srvWin > 0 ? (double)srvPktOut / srvWin : 0,
			srvWin > 0 ? (double)srvKbIn / srvWin : 0, srvWin > 0 ? (double)srvKbOut / srvWin : 0, g[LS_TICK_AVG],
			g[LS_TICK_P50], g[LS_TICK_P99], g[LS_TICK_MAX], g[LS_WALL_P99], g[LS_WALL_MAX], g[LS_LATE],
			g[LS_TICKS], g[LS_CPU_PERMILLE] / 10.0, g[LS_RSS_KB], g[LS_HWM_KB], g[LS_NET2_KB],
			g[LS_NET2_PEAK_KB]);
	}
	printf("client process: cpu %.1f%%, rss %uKB hwm %uKB, net2 peak %zuKB\n",
		wallUs ? 100.0 * (double)cpuUs / (double)wallUs : 0.0, rss, hwm, load_memPeak / 1024);
	printf("client failures:");
	for (int i = 0; i <= NET2_DR_APP; i++) {
		if (reasons[i]) {
			printf(" %s=%u", Net2_ReasonString(i), reasons[i]);
		}
	}
	printf("\n");
	fflush(stdout);

	if (!o->csv) {
		return;
	}
	FILE *f = fopen(o->csv, "a+");
	if (!f) {
		fprintf(stderr, "net2load: cannot write %s\n", o->csv);
		return;
	}
	fseek(f, 0, SEEK_END);
	if (ftell(f) == 0) {
		fputs("label,clients,loss_pct,window_s,snap_size,input_hz,connected,failed,joined,dropped,no_stats,"
			"connect_p50_ms,connect_p90_ms,connect_p99_ms,connect_max_ms,join_p50_ms,join_p99_ms,join_max_ms,"
			"rtt_avg_ms,rtt_p99_ms,rtt_max_ms,lat_p50_ms,lat_p99_ms,lat_max_ms,net2_loss_pct,input_lost_pct,"
			"snap_lost_pct,c2s_Bps,c2s_pps,s2c_Bps,s2c_pps,resent_c2s,resent_s2c,sendfull_cli,sendfull_srv,"
			"integrity_errors,srv_pps_in,srv_pps_out,srv_KBps_in,srv_KBps_out,srv_tick_avg_us,srv_tick_p50_us,"
			"srv_tick_p99_us,srv_tick_max_us,srv_wall_p99_us,srv_wall_max_us,srv_late,srv_ticks,srv_cpu_pct,"
			"srv_rss_kb,srv_hwm_kb,srv_net2_kb,srv_net2_peak_kb,cli_cpu_pct,cli_rss_kb,cli_hwm_kb,"
			"cli_net2_peak_kb,srv_sock_drops,cli_sock_drops,srv_net2_dropped,filtered\n", f);
	}
	uint32_t z[LOAD_STAT_COUNT] = { 0 };
	if (!g) {
		g = z;
	}
	fprintf(f, "%s,%d,%.1f,%.1f,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.2f,%.3f,%.3f,"
		"%.0f,%.1f,%.0f,%.1f,%llu,%llu,%u,%u,%u,%.0f,%.0f,%.0f,%.0f,%u,%u,%u,%u,%u,%u,%u,%u,%.1f,%u,%u,%u,%u,"
		"%.1f,%u,%u,%zu,%llu,%u,%llu,%llu\n", o->label, cs->n, o->loss, win, o->snapSize, o->inputHz, connected, failedConnect,
		joined, dropped, noStats, Hist_Pct(&cs->connect, 50), Hist_Pct(&cs->connect, 90),
		Hist_Pct(&cs->connect, 99), cs->connect.max, Hist_Pct(&cs->join, 50), Hist_Pct(&cs->join, 99),
		cs->join.max, Hist_Avg(&cs->rtt), Hist_Pct(&cs->rtt, 99), cs->rtt.max, Hist_Pct(&cs->lat, 50),
		Hist_Pct(&cs->lat, 99), cs->lat.max, withStats ? 100.0 * lossSum / withStats : 0.0, inLoss, snapLoss,
		(double)bytesOut * perC, (double)pktOut * perC, (double)bytesIn * perC, (double)pktIn * perC,
		(unsigned long long)resent, (unsigned long long)srvResent, sendFull, srvSendFull, integrity,
		srvWin > 0 ? (double)srvPktIn / srvWin : 0, srvWin > 0 ? (double)srvPktOut / srvWin : 0,
		srvWin > 0 ? (double)srvKbIn / srvWin : 0, srvWin > 0 ? (double)srvKbOut / srvWin : 0, g[LS_TICK_AVG],
		g[LS_TICK_P50], g[LS_TICK_P99], g[LS_TICK_MAX], g[LS_WALL_P99], g[LS_WALL_MAX], g[LS_LATE], g[LS_TICKS],
		g[LS_CPU_PERMILLE] / 10.0, g[LS_RSS_KB], g[LS_HWM_KB], g[LS_NET2_KB], g[LS_NET2_PEAK_KB],
		wallUs ? 100.0 * (double)cpuUs / (double)wallUs : 0.0, rss, hwm, load_memPeak / 1024,
		(unsigned long long)srvSockDrops, cliSockDrops, (unsigned long long)srvDropped, (unsigned long long)filtered);
	fclose(f);
}

int Client_Run(const LoadOpts *o) {
	Clients *cs = &s_cl;
	Net2Addr server, base;
	uint8_t pin[NET2_KEY_SIZE];
	int ok = 1;

	if (!Net2_ParseAddr(o->bind, 29300, &server) || !Net2_ParseAddr(o->clientBase, 0, &base)
		|| base.family != server.family) {
		fprintf(stderr, "net2load: bad addresses %s / %s (same family)\n", o->bind, o->clientBase);
		return 2;
	}
	if (o->pin) {
		uint8_t seed[32], sk[64];
		Load_Seed32(seed, "server", o->seed, 0);
		crypto_eddsa_key_pair(sk, pin, seed);
		crypto_wipe(sk, sizeof(sk));
	}
	cs->o = o;
	cs->n = o->clients;
	cs->period = 1000000 / (uint64_t)o->inputHz;
	cs->c = calloc((size_t)cs->n, sizeof(Client));	/* the client count is a run option */
	if (!cs->c) {
		return 1;
	}
	if (o->timeline) {
		cs->timeline = fopen(o->timeline, "a");
	}
	for (int i = 0; i < cs->n && ok; i++) {
		ok = Client_Create(cs, &cs->c[i], i, &server, &base, o->pin ? pin : NULL);
	}
	if (!ok) {
		for (int i = 0; i < cs->n; i++) {
			Net2_Destroy(cs->c[i].h);
		}
		free(cs->c);
		return 1;
	}

	uint64_t start = Load_Now(), ready = 0, quitAt = 0, cpu0 = 0, cpuQuit = 0, last = start;
	uint64_t ramp = (uint64_t)(o->ramp * 1000), interval = (uint64_t)(o->interval * 1e6);
	uint64_t drain = (uint64_t)(o->drain * 1e6);
	const char *phase = "connect";
	for (;;) {
		uint64_t now = Load_Now(), wake = now + 2000;
		int pending = 0, live = 0;
		cs->now = now;
		for (int i = 0; i < cs->n; i++) {
			Client *c = &cs->c[i];
			if (c->state == CS_IDLE) {
				uint64_t at = start + ramp * (uint64_t)i;
				if (now < at) {
					wake = at < wake ? at : wake;
					pending++;
					continue;
				}
				c->connectStart = now;
				/* the frames of the clients spread over one period */
				c->nextFrame = now + cs->period * (uint64_t)i / (uint64_t)cs->n;
				c->state = Net2_Connect(c->h, &server, now / 1000) == NET2_OK ? CS_CONNECTING : CS_FAILED;
			}
			if (c->state >= CS_DONE) {
				continue;
			}
			live++;
			pending += c->state < CS_JOINED;
			if (now >= c->nextFrame) {
				Client_Frame(cs, c);
				c->nextFrame += cs->period;
				if (c->nextFrame <= now) {
					c->nextFrame = now + cs->period;
				}
			}
			if (c->state == CS_CLOSING && now - c->closeTime > 3000000) {
				c->state = CS_DONE;	/* the disconnect event never came */
			}
			wake = c->nextFrame < wake ? c->nextFrame : wake;
		}

		if (!ready && (pending == 0 || now - start >= (uint64_t)(o->connectWait * 1e6))) {
			ready = now;
			phase = "warmup";
		}
		if (ready && !cs->markTime && now - ready >= (uint64_t)(o->warmup * 1e6)) {
			/* the measured window starts: MARK through one joined client, baselines of all */
			for (int i = 0; i < cs->n; i++) {
				Client *c = &cs->c[i];
				if (c->state == CS_JOINED) {
					if (!cs->markTime) {
						uint8_t m = LM_MARK;
						Client_Send(c, LOAD_CH_RELIABLE, &m, 1, NULL);
						cs->markTime = now;
					}
					Net2_GetStats(c->h, 0, &c->base);
				}
			}
			if (!cs->markTime) {
				cs->markTime = now;	/* nobody joined; the run ends now */
			}
			Hist_Clear(&cs->lat);
			cpu0 = Load_ProcessCpu();
			cs->markDrops = Load_SockDrops((uint16_t)o->clientPort);
			phase = "run";
		}
		if (cs->markTime && !quitAt && now - cs->markTime >= (uint64_t)(o->seconds * 1e6)) {
			quitAt = now;
			cpuQuit = Load_ProcessCpu();
			phase = "quit";
			for (int i = 0; i < cs->n; i++) {
				Client *c = &cs->c[i];
				if (c->state == CS_JOINED) {
					c->state = CS_QUITTING;
				} else if (c->state < CS_JOINED && c->state != CS_IDLE) {
					Net2_Disconnect(c->h, 0, 0);
					c->state = CS_CLOSING;
					c->closeTime = now;
				}
			}
		}
		if (quitAt && now - quitAt >= drain) {
			for (int i = 0; i < cs->n; i++) {
				Client *c = &cs->c[i];
				if (c->state == CS_QUITTING) {
					Net2_GetStats(c->h, 0, &c->fin);
					Net2_Disconnect(c->h, 0, 0);
					c->state = CS_CLOSING;
					c->closeTime = now;
				}
			}
		}
		if (interval && now - last >= interval) {
			Client_Progress(cs, start, phase);
			last = now;
		}
		if ((quitAt && live == 0) || (quitAt && now - quitAt >= drain + 5000000)) {
			break;
		}
		now = Load_Now();
		if (wake > now) {
			Load_Wait(wake - now);
		}
	}
	Client_Report(cs, cpuQuit - cpu0, quitAt - cs->markTime);
	int failures = 0;
	for (int i = 0; i < cs->n; i++) {
		failures += cs->c[i].state == CS_FAILED || !cs->c[i].haveStats;
		Net2_Destroy(cs->c[i].h);
	}
	printf("cli net2 memory after destroy: %zu bytes\n", load_memUsed);
	if (cs->timeline) {
		fclose(cs->timeline);
	}
	free(cs->c);
	return failures ? 1 : 0;
}
