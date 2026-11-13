/*
 * net2load reference server: a fixed tick (Poll, the tool's messages, events, snapshots, Flush) like a headless
 * game server, with fake snapshots of a set size to every joined client and chat relayed to all of them. Thread
 * CPU and wall time of every tick go into histograms; MARK of the client process starts the measured window.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "load.h"
#include "monocypher.h"

typedef struct Peer {
	int used, joined;
	uint32_t index;
	Net2Window inputs;				/* distinct ticks */
	uint32_t inputCount, lastTick, lastTime;
	uint32_t snaps, events, relays, chats, chatNext, chatBad, sendFull;
	uint64_t nextEvent;
	Net2Stats base;					/* at MARK */
} Peer;

typedef struct Server {
	const LoadOpts *o;
	Net2Host *h;
	Peer peer[NET2_MAX_CONNS];
	uint64_t now;
	LoadHist tickCpu, tickWall;		/* since MARK */
	LoadHist ivCpu;					/* since the last progress line */
	uint64_t markTime, markCpu;
	uint32_t markDrops;
	uint16_t port;
	uint32_t memTick;				/* /proc is read once per tick, not per STATS */
	uint32_t rssKb, hwmKb, sockDrops;
	uint32_t ticks, late, joins, joinBad, sendFull, conns, everJoined;
	uint32_t reasons[NET2_DR_APP + 1];
	uint64_t ivPktIn, ivPktOut, ivBytesIn, ivBytesOut;
	FILE *timeline;
} Server;

static Server s_srv;
static uint8_t s_buf[NET2_MAX_MESSAGE];
static volatile sig_atomic_t s_stop;

static void Server_Signal(int sig) {
	(void)sig;
	s_stop = 1;
}

static int Server_Send(Server *s, int conn, int channel, const uint8_t *data, int size) {
	int e = Net2_Send(s->h, conn, channel, data, size);
	if (e == NET2_E_FULL) {
		s->peer[conn].sendFull++;
		s->sendFull++;
	}
	return e == NET2_OK;
}

/* type, the header numbers, then a filler up to size */
static int Server_Build(uint8_t *buf, int size, int type, const uint32_t *nums, int count) {
	Net2Writer w;

	Net2W_Init(&w, buf, size);
	Net2W_U8(&w, (uint32_t)type);
	for (int i = 0; i < count; i++) {
		Net2W_U32(&w, nums[i]);
	}
	for (int i = w.pos; i < size; i++) {
		buf[i] = (uint8_t)(i * 7 + type);
	}
	return size;
}

static void Server_Join(Server *s, int conn, Net2Reader *r) {
	Peer *p = &s->peer[conn];
	uint8_t key[NET2_KEY_SIZE], text[LOAD_JOIN_TEXT];

	uint32_t index = Net2R_U32(r);
	const uint8_t *pk = Net2R_Bytes(r, 32);
	const uint8_t *sig = Net2R_Bytes(r, 64);
	int ok = !r->bad && !p->joined && Net2_ServerKey(s->h, key);
	if (ok) {
		Load_JoinText(text, index, key);
		ok = crypto_eddsa_check(sig, pk, text, sizeof(text)) == 0;
	}
	if (!ok) {
		s->joinBad++;
	} else {
		s->joins++;
		s->everJoined = 1;
		p->joined = 1;
		p->index = index;
	}
	s_buf[0] = LM_WELCOME;
	s_buf[1] = (uint8_t)ok;
	for (int i = 2; i < s->o->welcomeSize; i++) {
		s_buf[i] = (uint8_t)(i * 13);
	}
	Server_Send(s, conn, LOAD_CH_RELIABLE, s_buf, s->o->welcomeSize);
}

static void Server_Input(Peer *p, Net2Reader *r) {
	int count = (int)Net2R_U8(r);

	for (int i = 0; i < count && i < LOAD_REDUNDANT; i++) {
		uint32_t tick = Net2R_U32(r);
		uint32_t time = Net2R_U32(r);
		Net2R_Bytes(r, LOAD_FRAME - 8);
		if (r->bad) {
			return;
		}
		if (Net2Win_Check(&p->inputs, tick)) {
			Net2Win_Mark(&p->inputs, tick);
			p->inputCount++;
			if (tick > p->lastTick) {
				p->lastTick = tick;
				p->lastTime = time;
			}
		}
	}
}

static void Server_Chat(Server *s, int conn, Net2Reader *r, int size) {
	Peer *p = &s->peer[conn];
	uint32_t seq = Net2R_U32(r);

	p->chatBad += seq != p->chatNext;
	p->chatNext = seq + 1;
	p->chats++;
	if (!s->o->relay) {
		return;
	}
	uint32_t nums[2] = { p->index, seq };
	int n = Server_Build(s_buf, size < 9 ? 9 : size, LM_RELAY, nums, 2);
	for (int i = 0; i < s->o->maxConns; i++) {
		if (s->peer[i].joined && Server_Send(s, i, LOAD_CH_RELIABLE, s_buf, n)) {
			s->peer[i].relays++;
		}
	}
}

static void Server_Mark(Server *s) {
	Hist_Clear(&s->tickCpu);
	Hist_Clear(&s->tickWall);
	s->ticks = 0;
	s->late = 0;
	s->markTime = s->now;
	s->markCpu = Load_ProcessCpu();
	s->markDrops = Load_SockDrops(s->port);
	s->memTick = 0;
	for (int i = 0; i < s->o->maxConns; i++) {
		if (s->peer[i].used) {
			Net2_GetStats(s->h, i, &s->peer[i].base);
		}
	}
}

static void Server_Stats(Server *s, int conn) {
	Peer *p = &s->peer[conn];
	uint32_t v[LOAD_STAT_COUNT];
	Net2Stats st;
	Net2Writer w;

	memset(v, 0, sizeof(v));
	Net2_GetStats(s->h, conn, &st);
	v[LS_INPUTS] = p->inputCount;
	v[LS_SNAPS] = p->snaps;
	v[LS_EVENTS] = p->events;
	v[LS_RELAYS] = p->relays;
	v[LS_CHATS] = p->chats;
	v[LS_CHATBAD] = p->chatBad;
	v[LS_PKT_OUT] = (uint32_t)(st.packetsSent - p->base.packetsSent);
	v[LS_PKT_IN] = (uint32_t)(st.packetsRecv - p->base.packetsRecv);
	v[LS_KB_OUT] = (uint32_t)((st.bytesSent - p->base.bytesSent) / 1024);
	v[LS_KB_IN] = (uint32_t)((st.bytesRecv - p->base.bytesRecv) / 1024);
	v[LS_RESENT] = (uint32_t)(st.resent - p->base.resent);
	v[LS_DROPPED] = st.dropped - p->base.dropped;
	v[LS_RTT] = st.rtt;
	v[LS_SENDFULL] = p->sendFull;
	v[LS_WINDOW_MS] = (uint32_t)((s->now - s->markTime) / 1000);
	v[LS_TICKS] = s->ticks;
	v[LS_LATE] = s->late;
	v[LS_TICK_AVG] = Hist_Avg(&s->tickCpu);
	v[LS_TICK_P50] = Hist_Pct(&s->tickCpu, 50);
	v[LS_TICK_P99] = Hist_Pct(&s->tickCpu, 99);
	v[LS_TICK_MAX] = s->tickCpu.max;
	v[LS_WALL_P99] = Hist_Pct(&s->tickWall, 99);
	v[LS_WALL_MAX] = s->tickWall.max;
	uint64_t wall = s->now - s->markTime;
	v[LS_CPU_PERMILLE] = wall ? (uint32_t)((Load_ProcessCpu() - s->markCpu) * 1000 / wall) : 0;
	if (s->memTick != s->ticks + 1) {
		Load_Memory(&s->rssKb, &s->hwmKb);
		s->sockDrops = Load_SockDrops(s->port) - s->markDrops;
		s->memTick = s->ticks + 1;
	}
	v[LS_RSS_KB] = s->rssKb;
	v[LS_HWM_KB] = s->hwmKb;
	v[LS_NET2_KB] = (uint32_t)(load_memUsed / 1024);
	v[LS_NET2_PEAK_KB] = (uint32_t)(load_memPeak / 1024);
	v[LS_CONNS] = s->conns;
	v[LS_JOINS] = s->joins;
	v[LS_JOINBAD] = s->joinBad;
	v[LS_SOCK_DROPS] = s->sockDrops;
	Net2W_Init(&w, s_buf, sizeof(s_buf));
	Net2W_U8(&w, LM_STATS);
	for (int i = 0; i < LOAD_STAT_COUNT; i++) {
		Net2W_U32(&w, v[i]);
	}
	Server_Send(s, conn, LOAD_CH_RELIABLE, s_buf, w.pos);
}

static void Server_Events(Server *s) {
	Net2Event ev;
	Net2Reader r;

	while (Net2_Receive(s->h, &ev)) {
		Peer *p = &s->peer[ev.conn];
		if (ev.type == NET2_EV_CONNECT) {
			memset(p, 0, sizeof(*p));
			p->used = 1;
			p->nextEvent = s->now + (uint64_t)((uint32_t)ev.conn * 2654435761u % 1000000);
			if (s->markTime) {
				Net2_GetStats(s->h, ev.conn, &p->base);
			}
			s->conns++;
		} else if (ev.type == NET2_EV_DISCONNECT) {
			s->reasons[ev.reason > NET2_DR_APP ? NET2_DR_APP : ev.reason]++;
			memset(p, 0, sizeof(*p));
			s->conns--;
		} else if (ev.type == NET2_EV_MESSAGE && ev.size > 0) {
			Net2R_Init(&r, ev.data, ev.size);
			int type = (int)Net2R_U8(&r);
			if (ev.channel == LOAD_CH_UNRELIABLE && type == LM_INPUT) {
				Server_Input(p, &r);
			} else if (ev.channel != LOAD_CH_RELIABLE) {
				continue;
			} else if (type == LM_JOIN) {
				Server_Join(s, ev.conn, &r);
			} else if (type == LM_CHAT && p->joined) {
				Server_Chat(s, ev.conn, &r, ev.size);
			} else if (type == LM_MARK) {
				Server_Mark(s);
			} else if (type == LM_QUIT) {
				Server_Stats(s, ev.conn);
			}
		}
	}
}

/* the events go out with the snapshots */
static void Server_Snapshots(Server *s, uint32_t tick) {
	const LoadOpts *o = s->o;

	for (int i = 0; i < o->maxConns; i++) {
		Peer *p = &s->peer[i];
		if (!p->joined) {
			continue;
		}
		uint32_t nums[5] = { tick, p->snaps + 1, p->lastTick, p->lastTime, p->inputCount };
		Server_Build(s_buf, o->snapSize, LM_SNAP, nums, 5);
		if (Server_Send(s, i, LOAD_CH_SEQUENCED, s_buf, o->snapSize)) {
			p->snaps++;
		}
		if (o->eventHz > 0 && s->now >= p->nextEvent) {
			uint32_t seq = p->events;
			Server_Build(s_buf, o->eventSize, LM_EVENT, &seq, 1);
			if (Server_Send(s, i, LOAD_CH_RELIABLE, s_buf, o->eventSize)) {
				p->events++;
			}
			p->nextEvent += (uint64_t)(1e6 / o->eventHz);
		}
	}
}

static void Server_Progress(Server *s, uint64_t start, uint64_t *last) {
	uint64_t pin = 0, pout = 0, bin = 0, bout = 0;
	uint32_t rss, hwm, joined = 0;
	Net2Stats st;

	for (int i = 0; i < s->o->maxConns; i++) {
		if (s->peer[i].used && Net2_GetStats(s->h, i, &st)) {
			pin += st.packetsRecv;
			pout += st.packetsSent;
			bin += st.bytesRecv;
			bout += st.bytesSent;
			joined += (uint32_t)s->peer[i].joined;
		}
	}
	double dt = (double)(s->now - *last) / 1e6;
	Load_Memory(&rss, &hwm);
	/* totals of the live connections, so a disconnect makes a step down: clamp */
	double rin = pin > s->ivPktIn ? (double)(pin - s->ivPktIn) / dt : 0;
	double rout = pout > s->ivPktOut ? (double)(pout - s->ivPktOut) / dt : 0;
	double kin = bin > s->ivBytesIn ? (double)(bin - s->ivBytesIn) / dt / 1024 : 0;
	double kout = bout > s->ivBytesOut ? (double)(bout - s->ivBytesOut) / dt / 1024 : 0;
	double t = (double)(s->now - start) / 1e6;
	printf("srv t=%.0f conns=%u joined=%u tick_cpu_us avg=%u p99=%u max=%u late=%u pkt/s in=%.0f out=%.0f "
		"KB/s in=%.1f out=%.1f rss=%uKB net2=%zuKB sock_drops=%u\n", t, s->conns, joined, Hist_Avg(&s->ivCpu),
		Hist_Pct(&s->ivCpu, 99), s->ivCpu.max, s->late, rin, rout, kin, kout, rss, load_memUsed / 1024,
		Load_SockDrops(s->port));
	fflush(stdout);
	if (s->timeline) {
		fprintf(s->timeline, "srv,%.1f,%u,%u,%u,%u,%u,%u,%.0f,%.0f,%.1f,%.1f,%u,%zu\n", t, s->conns, joined,
			Hist_Avg(&s->ivCpu), Hist_Pct(&s->ivCpu, 99), s->ivCpu.max, s->late, rin, rout, kin, kout, rss,
			load_memUsed / 1024);
		fflush(s->timeline);
	}
	Hist_Clear(&s->ivCpu);
	s->ivPktIn = pin;
	s->ivPktOut = pout;
	s->ivBytesIn = bin;
	s->ivBytesOut = bout;
	*last = s->now;
}

int Server_Run(const LoadOpts *o) {
	Server *s = &s_srv;
	Net2Config cfg;
	Net2Addr at;
	uint8_t seed[32], key[NET2_KEY_SIZE];
	char addr[80];
	int err;

	if (!Net2_ParseAddr(o->bind, 29300, &at)) {
		fprintf(stderr, "net2load: bad address %s\n", o->bind);
		return 2;
	}
	Net2_DefaultConfig(&cfg, 1);
	cfg.port = at.port;
	cfg.ipv4 = at.family == NET2_AF_IPV4;
	cfg.ipv6 = at.family == NET2_AF_IPV6;
	cfg.bind4 = at;
	cfg.bind6 = at;
	cfg.maxConns = o->maxConns;
	cfg.appVersion = o->appVersion;
	cfg.rate = o->rate;
	if (o->sockBuffer >= 0) {
		cfg.sockBuffer = (uint32_t)o->sockBuffer;
	}
	Load_Seed32(seed, "server", o->seed, 0);
	cfg.identitySeed = seed;
	cfg.alloc = Load_Alloc;
	cfg.free = Load_Free;
	s->o = o;
	s->h = Net2_Create(&cfg, &err);
	if (!s->h) {
		fprintf(stderr, "net2load: server on %s: error %d\n", o->bind, err);
		return 1;
	}
	if (o->timeline) {
		s->timeline = fopen(o->timeline, "a");
	}
	Net2_ServerKey(s->h, key);
	s->port = at.port;
	Net2_AddrString(&at, addr, sizeof(addr));
	printf("srv listening on %s, key %02x%02x%02x%02x.., tick %d Hz, snapshots %d Hz of %d bytes, events %.1f Hz\n",
		addr, key[0], key[1], key[2], key[3], o->tickHz, o->snapHz, o->snapSize, o->eventHz);
	fflush(stdout);
	signal(SIGINT, Server_Signal);
	signal(SIGTERM, Server_Signal);

	uint64_t period = 1000000 / (uint64_t)o->tickHz, snapPeriod = 1000000 / (uint64_t)o->snapHz;
	uint64_t pollPeriod = o->pollHz > o->tickHz ? 1000000 / (uint64_t)o->pollHz : 0;
	uint64_t start = Load_Now(), next = start, nextSnap = start, nextPoll = start, last = start;
	uint64_t interval = (uint64_t)(o->interval * 1e6);
	uint32_t tick = 0;
	s->markTime = 0;
	s->markCpu = Load_ProcessCpu();
	while (!s_stop) {
		uint64_t now = Load_Now();
		if (now < next) {
			if (pollPeriod && now >= nextPoll) {
				Net2_Poll(s->h, now / 1000);
				nextPoll = now + pollPeriod;
			}
			uint64_t until = pollPeriod && nextPoll < next ? nextPoll : next;
			Load_Wait(until > now ? (until - now < 2000 ? until - now : 2000) : 0);
			continue;
		}
		if (now - next > period) {
			s->late++;
		}
		s->now = now;
		uint64_t cpu0 = Load_ThreadCpu();
		Net2_Poll(s->h, now / 1000);
		Server_Events(s);
		if (now >= nextSnap) {
			Server_Snapshots(s, tick);
			while (nextSnap <= now) {
				nextSnap += snapPeriod;
			}
		}
		Net2_Flush(s->h, now / 1000);
		uint32_t cpu = (uint32_t)(Load_ThreadCpu() - cpu0);
		Hist_Add(&s->tickCpu, cpu);
		Hist_Add(&s->ivCpu, cpu);
		Hist_Add(&s->tickWall, (uint32_t)(Load_Now() - now));
		s->ticks++;
		tick++;
		next += period;
		if (now > next + 10 * period) {
			next = now + period;	/* stalled: start again from here instead of catching up */
		}
		if (interval && now - last >= interval) {
			Server_Progress(s, start, &last);
		}
		if (o->seconds > 0 && now - start >= (uint64_t)(o->seconds * 1e6)) {
			break;
		}
		if (o->exitEmpty && s->everJoined && s->conns == 0) {
			break;
		}
	}
	s->now = Load_Now();
	uint32_t rss, hwm;
	Load_Memory(&rss, &hwm);
	printf("srv done: %u ticks since mark, tick_cpu_us avg=%u p50=%u p99=%u max=%u, wall p99=%u max=%u, late=%u, "
		"joins=%u bad=%u, send_full=%u, rss=%uKB hwm=%uKB, net2 peak=%zuKB\n", s->ticks, Hist_Avg(&s->tickCpu),
		Hist_Pct(&s->tickCpu, 50), Hist_Pct(&s->tickCpu, 99), s->tickCpu.max, Hist_Pct(&s->tickWall, 99),
		s->tickWall.max, s->late, s->joins, s->joinBad, s->sendFull, rss, hwm, load_memPeak / 1024);
	printf("srv disconnects:");
	for (int i = 0; i <= NET2_DR_APP; i++) {
		if (s->reasons[i]) {
			printf(" %s=%u", Net2_ReasonString(i), s->reasons[i]);
		}
	}
	printf("\n");
	Net2_Destroy(s->h);
	if (s->timeline) {
		fclose(s->timeline);
	}
	printf("srv net2 memory after destroy: %zu bytes\n", load_memUsed);
	return 0;
}
