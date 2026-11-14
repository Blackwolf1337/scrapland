/*
 * Unit test of NetRep (include/scrap/netrep.h): the varints, the quantisation, the fields against a base, the interest
 * grid against a brute force search, the priorities, and a server and clients over a link with loss, duplicates and
 * reordering (both ways), with entities that move, die, are born into reused slots, and views that move: every
 * message within the size, no record without its base, the budget kept, and after a quiet phase each client holds
 * exactly the entities of its interest with the server's states and looks. Then random bytes as messages.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/netrep.h"

static int g_checks, g_fails;

#define CHECK(c) do { g_checks++; if (!(c)) { g_fails++; if (g_fails < 30) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint64_t g_rng = 0x9e3779b97f4a7c15ull;

static uint32_t Rnd(void) {
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 16);
}

static float RndF(float lo, float hi) {
	return lo + (hi - lo) * (float)(Rnd() & 0xffffff) / 16777216.0f;
}

static void Test_Varints(void) {
	uint8_t buf[64];
	static const uint32_t u[] = { 0, 1, 127, 128, 255, 16383, 16384, 0x7fffffff, 0xffffffff };
	static const int32_t sv[] = { 0, 1, -1, 63, -64, 64, -65, 2147483647, -2147483647 - 1 };
	for (size_t i = 0; i < sizeof(u) / sizeof(u[0]); i++) {
		NetRepBuf w;
		NetRepReader r;
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_PutVar(&w, u[i]);
		NetRep_PutSVar(&w, sv[i]);
		NetRepReader_Init(&r, buf, w.size);
		CHECK(NetRep_GetVar(&r) == u[i]);
		CHECK(NetRep_GetSVar(&r) == sv[i]);
		CHECK(!r.bad && r.pos == w.size);
	}
	/* a writer without room and a reader past the end */
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, 2);
	NetRep_PutU32(&w, 1);
	CHECK(w.bad);
	NetRepReader r;
	NetRepReader_Init(&r, buf, 1);
	NetRep_GetU16(&r);
	CHECK(r.bad);
	/* a varint that never ends */
	memset(buf, 0xff, 8);
	NetRepReader_Init(&r, buf, 8);
	NetRep_GetVar(&r);
	CHECK(r.bad);
}

static void Test_Quant(void) {
	for (int i = 0; i < 10000; i++) {
		float p = RndF(-300000.0f, 300000.0f);
		CHECK(fabsf(NetRep_UPos(NetRep_QPos(p)) - p) <= 0.5f / NETREP_POS_SCALE + fabsf(p) * 1e-7f + 0.01f);
		float a = RndF(-20.0f, 20.0f);
		float u = NetRep_UAng(NetRep_QAng(a));
		float d = remainderf(u - a, 6.2831853f);
		CHECK(fabsf(d) < 0.0002f);
	}
	CHECK(NetRep_QCtrl(2.0f) == 127 && NetRep_QCtrl(-2.0f) == -127 && NetRep_QCtrl(0.5f) == 64);
	CHECK(NetRep_QPos(NAN) < 0);
}

static void RandState(NetRepState *s) {
	memset(s, 0, sizeof(*s));
	for (int i = 0; i < 3; i++) {
		s->pos[i] = (int32_t)Rnd() - (int32_t)(Rnd() & 0x7fff);
		s->vel[i] = (int32_t)(Rnd() % 40000) - 20000;
		s->ang[i] = (uint16_t)Rnd();
	}
	s->view[0] = (uint16_t)Rnd();
	s->view[1] = (uint16_t)Rnd();
	s->life = (int16_t)Rnd();
	s->flags = (uint8_t)Rnd();
	s->ctrl[0] = (int8_t)(Rnd() % 255 - 127);
	s->ctrl[1] = (int8_t)(Rnd() % 255 - 127);
	s->dt = (uint16_t)Rnd();
	for (int l = 0; l < NETREP_LAYERS; l++) {
		s->act[l] = (uint16_t)Rnd();
		s->actTime[l] = Rnd() * 3u;
	}
	s->weapon = (uint8_t)Rnd();
}

static void Test_Fields(void) {
	uint8_t buf[256];
	for (int i = 0; i < 20000; i++) {
		NetRepState a, b, out;
		RandState(&a);
		b = a;
		/* change some fields only */
		NetRepState c;
		RandState(&c);
		uint32_t pick = Rnd() & NETREP_F_ALL;
		if (pick & NETREP_F_POS) memcpy(b.pos, c.pos, sizeof(b.pos));
		if (pick & NETREP_F_ANG) memcpy(b.ang, c.ang, sizeof(b.ang));
		if (pick & NETREP_F_VEL) memcpy(b.vel, c.vel, sizeof(b.vel));
		if (pick & NETREP_F_DT) b.dt = c.dt;
		if (pick & NETREP_F_LIFE) b.life = c.life;
		if (pick & NETREP_F_FLAGS) b.flags = c.flags;
		if (pick & NETREP_F_CTRL) memcpy(b.ctrl, c.ctrl, sizeof(b.ctrl));
		if (pick & NETREP_F_ACT0) { b.act[0] = c.act[0]; b.actTime[0] = c.actTime[0]; }
		if (pick & NETREP_F_ACT1) { b.act[1] = c.act[1]; b.actTime[1] = c.actTime[1]; }
		if (pick & NETREP_F_VIEW) memcpy(b.view, c.view, sizeof(b.view));
		if (pick & NETREP_F_WEAP) b.weapon = c.weapon;
		uint32_t mask = NetRep_Diff(&a, &b);
		CHECK((mask & ~pick) == 0);
		NetRepBuf w;
		NetRepReader r;
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_WriteFields(&w, &a, &b, mask);
		CHECK(!w.bad);
		NetRepReader_Init(&r, buf, w.size);
		CHECK(NetRep_ReadFields(&r, &a, &out, mask));
		CHECK(r.pos == w.size);
		CHECK(!memcmp(&out, &b, sizeof(out)) || !NetRep_Diff(&out, &b));
		CHECK(NetRep_Diff(&out, &b) == 0);
		/* a cut record */
		if (w.size > 1) {
			NetRepReader_Init(&r, buf, w.size - 1);
			CHECK(!NetRep_ReadFields(&r, &a, &out, mask));
		}
	}
	/* a small move costs few bytes */
	NetRepState a, b;
	RandState(&a);
	b = a;
	b.pos[0] += 40;
	b.pos[2] -= 12;
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_WriteFields(&w, &a, &b, NetRep_Diff(&a, &b));
	CHECK(w.size == 3);
}

static void Test_Grid(void) {
	static NetRepGrid g;
	static float px[NETREP_MAX_ENTS], pz[NETREP_MAX_ENTS];
	static uint16_t out[NETREP_MAX_ENTS];
	for (int round = 0; round < 50; round++) {
		float cell = RndF(500.0f, 20000.0f);
		int n = (int)(Rnd() % NETREP_MAX_ENTS);
		NetRepGrid_Clear(&g, cell);
		for (int i = 0; i < n; i++) {
			px[i] = RndF(-200000.0f, 200000.0f);
			pz[i] = RndF(-200000.0f, 200000.0f);
			NetRepGrid_Add(&g, i, px[i], pz[i]);
		}
		for (int q = 0; q < 20; q++) {
			float x = RndF(-250000.0f, 250000.0f), z = RndF(-250000.0f, 250000.0f), rad = RndF(0.0f, 120000.0f);
			int m = NetRepGrid_Query(&g, x, z, rad, out, NETREP_MAX_ENTS);
			static uint8_t seen[NETREP_MAX_ENTS];
			memset(seen, 0, sizeof(seen));
			int dup = 0;
			for (int i = 0; i < m; i++) {
				dup |= seen[out[i]];
				seen[out[i]] = 1;
			}
			CHECK(!dup);
			/* everything within the radius is there; nothing beyond the square of cells */
			for (int i = 0; i < n; i++) {
				float dx = px[i] - x, dz = pz[i] - z;
				if (dx * dx + dz * dz <= rad * rad) {
					CHECK(seen[i]);
				}
				if (seen[i]) {
					CHECK(fabsf(dx) <= rad + cell && fabsf(dz) <= rad + cell);
				}
			}
		}
	}
}

static void Test_Priority(void) {
	float r = NETREP_RADIUS;
	CHECK(NetRep_Priority(100.0f, r, 50.0f, NETREP_OP_DELTA) > NetRep_Priority(30000.0f, r, 50.0f, NETREP_OP_DELTA));
	CHECK(NetRep_Priority(5000.0f, r, 2000.0f, NETREP_OP_DELTA) > NetRep_Priority(5000.0f, r, 10.0f, NETREP_OP_DELTA));
	CHECK(NetRep_Priority(5000.0f, r, 0.0f, NETREP_OP_CREATE) > NetRep_Priority(5000.0f, r, 10.0f, NETREP_OP_DELTA));
	CHECK(NetRep_Priority(r * 3.0f, r, 0.0f, NETREP_OP_DELTA) > 0.0f);
	NetRepCand c[200];
	for (int i = 0; i < 200; i++) {
		c[i].slot = (uint16_t)(199 - i);
		c[i].prio = (float)(Rnd() % 7);
		c[i].op = 0;
	}
	NetRep_SortCands(c, 200);
	for (int i = 1; i < 200; i++) {
		CHECK(c[i - 1].prio > c[i].prio || (c[i - 1].prio == c[i].prio && c[i - 1].slot < c[i].slot));
	}
}

/* ---- server and clients over a bad link ---- */

#define SIM_ENTS 600
#define SIM_CLIENTS 3
#define SIM_QUEUE 4096

typedef struct SimEnt {
	int alive;
	int slot;
	int kind, look;
	float p[3], v[3];
	NetRepState st;
} SimEnt;

typedef struct SimPkt {
	int to;							/* client, or -1 - client for an ack to the server */
	int due;						/* snapshot it arrives */
	int size;
	uint8_t data[NETREP_MSG_MAX];
} SimPkt;

typedef struct SimClient {
	NetRepClient *srv;
	NetRepView *view;
	float vp[3];
	int creates, removes, updates;
	int bad;
	int ghostLook;					/* a create with another look than the server's */
} SimClient;

static SimEnt g_ent[SIM_ENTS];
static int g_slotEnt[NETREP_MAX_ENTS];	/* the entity of a slot, -1 */
static SimClient g_cli[SIM_CLIENTS];
static SimPkt g_q[SIM_QUEUE];
static int g_nq;
static NetRepServer *g_srv;
static const char *g_lookNames[] = { "ship:SPOLI1|Pilot|m1|m2", "ship:TRAFFIC2", "char:Police", "char:Functionary",
	"char:Messenger", "ship:SMERC3|Merc" };

typedef struct SimCtx {
	SimClient *c;
} SimCtx;

static void Ev_Create(void *ctx, int slot, int kind, const char *look, uint32_t tick, const NetRepState *st) {
	SimClient *c = ctx;
	(void)tick;
	(void)st;
	c->creates++;
	int e = g_slotEnt[slot];
	if (e >= 0 && (strcmp(look, g_lookNames[g_ent[e].look]) || kind != g_ent[e].kind)) {
		c->ghostLook++;
	}
}

static void Ev_Update(void *ctx, int slot, uint32_t tick, const NetRepState *st) {
	(void)slot;
	(void)tick;
	(void)st;
	((SimClient *)ctx)->updates++;
}

static void Ev_Remove(void *ctx, int slot) {
	(void)slot;
	((SimClient *)ctx)->removes++;
}

static void Sim_Push(int to, int due, const uint8_t *data, int size) {
	if (g_nq >= SIM_QUEUE) {
		return;
	}
	SimPkt *p = &g_q[g_nq++];
	p->to = to;
	p->due = due;
	p->size = size;
	memcpy(p->data, data, (size_t)size);
}

static void Sim_Born(int i, int snap) {
	SimEnt *e = &g_ent[i];
	int slot = NetRepSrv_FreeSlot(g_srv);
	if (slot < 0) {
		return;
	}
	(void)snap;
	e->alive = 1;
	e->slot = slot;
	e->look = (int)(Rnd() % 6);
	e->kind = g_lookNames[e->look][0] == 's' ? NETREP_SHIP : NETREP_CHAR;
	for (int k = 0; k < 3; k++) {
		e->p[k] = RndF(-90000.0f, 90000.0f);
		e->v[k] = RndF(-3000.0f, 3000.0f);
	}
	e->p[1] = RndF(0.0f, 5000.0f);
	g_slotEnt[slot] = i;
}

/* one run: loss and duplicates in percent, delays up to maxDelay snapshots, snapshots of motion, then a quiet phase */
static void Sim_Run(int lossPct, int dupPct, int maxDelay, int snaps, uint32_t rate) {
	static uint8_t msgs[NETREP_MAX_MSGS][NETREP_MSG_MAX];
	int sizes[NETREP_MAX_MSGS];
	int maxMsg = 0, overBudget = 0;
	g_srv = NetRepSrv_Create();
	CHECK(g_srv != NULL);
	memset(g_ent, 0, sizeof(g_ent));
	for (int i = 0; i < NETREP_MAX_ENTS; i++) {
		g_slotEnt[i] = -1;
	}
	g_nq = 0;
	for (int i = 0; i < 6; i++) {
		CHECK(NetRepSrv_Look(g_srv, g_lookNames[i]) == i);
	}
	CHECK(NetRepSrv_Look(g_srv, g_lookNames[2]) == 2);
	for (int c = 0; c < SIM_CLIENTS; c++) {
		memset(&g_cli[c], 0, sizeof(g_cli[c]));
		g_cli[c].srv = NetRepSrv_AddClient(g_srv, rate);
		g_cli[c].view = NetRepCli_Create();
		g_cli[c].vp[0] = RndF(-50000.0f, 50000.0f);
		g_cli[c].vp[2] = RndF(-50000.0f, 50000.0f);
	}
	for (int i = 0; i < SIM_ENTS; i++) {
		Sim_Born(i, 0);
	}
	int quietFrom = snaps;
	int total = snaps + 400;
	for (int t = 1; t <= total; t++) {
		int quiet = t > quietFrom;
		/* the world moves; some die, some are born */
		uint32_t snap = NetRepSrv_Begin(g_srv, (uint32_t)t * 3 / 2);
		for (int i = 0; i < SIM_ENTS; i++) {
			SimEnt *e = &g_ent[i];
			if (!quiet && e->alive && Rnd() % 1000 < 3) {
				e->alive = 0;
				g_slotEnt[e->slot] = -1;
			} else if (!quiet && !e->alive && Rnd() % 1000 < 20) {
				Sim_Born(i, (int)snap);
			}
			if (!e->alive) {
				continue;
			}
			if (!quiet && (i % 3) != 0) {
				for (int k = 0; k < 3; k++) {
					e->p[k] += e->v[k] * 0.05f;
				}
				if (Rnd() % 50 == 0) {
					e->v[0] = RndF(-3000.0f, 3000.0f);
				}
			}
			NetRepState *s = &e->st;
			memset(s, 0, sizeof(*s));
			for (int k = 0; k < 3; k++) {
				s->pos[k] = NetRep_QPos(e->p[k]);
				s->vel[k] = quiet || (i % 3) == 0 ? 0 : NetRep_QVel(e->v[k]);
			}
			s->ang[1] = NetRep_QAng(atan2f(e->v[0], e->v[2]));
			s->life = (int16_t)(100 - i % 50);
			s->flags = NETREP_FL_ACTIVE;
			s->dt = (uint16_t)((i * 7) % 50);
			if (e->kind == NETREP_CHAR) {
				s->act[0] = (uint16_t)(1 + ((quiet ? quietFrom : t) / 40 + i) % 5);
				s->actTime[0] = quiet ? 500 : (uint32_t)((t * 50) % 3000);
			}
			NetRepSrv_Set(g_srv, e->slot, e->kind, e->look, s);
		}
		NetRepSrv_End(g_srv);
		/* the clients' views move */
		for (int c = 0; c < SIM_CLIENTS; c++) {
			SimClient *cl = &g_cli[c];
			if (!quiet) {
				cl->vp[0] += 400.0f * sinf((float)t * 0.01f + (float)c);
				cl->vp[2] += 400.0f * cosf((float)t * 0.013f + (float)c);
			}
			NetRepSrv_SetView(cl->srv, cl->vp[0], cl->vp[1], cl->vp[2], 0.0f);
			int n = NetRepSrv_Build(g_srv, cl->srv, msgs, sizes, NETREP_MAX_MSGS);
			int bytes = 0;
			for (int m = 0; m < n; m++) {
				CHECK(sizes[m] <= NETREP_MSG_MAX && sizes[m] > NETREP_MSG_HEADER);
				maxMsg = sizes[m] > maxMsg ? sizes[m] : maxMsg;
				bytes += sizes[m] + NETREP_MSG_OVERHEAD;
				int lost = !quiet && (int)(Rnd() % 100) < lossPct;
				int delay = quiet ? 1 : 1 + (int)(Rnd() % (unsigned)(maxDelay + 1));
				if (!lost) {
					Sim_Push(c, t + delay, msgs[m], sizes[m]);
				}
				if (!quiet && (int)(Rnd() % 100) < dupPct) {
					Sim_Push(c, t + 1 + (int)(Rnd() % (unsigned)(maxDelay + 2)), msgs[m], sizes[m]);
				}
			}
			/* two snapshots' budget plus one record may go out at once */
			if (bytes > (int)(rate / NETREP_SNAP_HZ) * 2 + 400) {
				overBudget++;
			}
		}
		/* deliver what is due, in the order of the queue (the delays reorder) */
		int k = 0;
		for (int i = 0; i < g_nq; i++) {
			SimPkt *p = &g_q[i];
			if (p->due > t) {
				g_q[k++] = *p;
				continue;
			}
			if (p->to >= 0) {
				SimClient *cl = &g_cli[p->to];
				NetRepEvents ev = { cl, Ev_Create, Ev_Update, Ev_Remove };
				/* late duplicates and reordered messages may name a base that is gone: not taken, not acked */
				if (!NetRepCli_Message(cl->view, p->data, p->size, &ev) && t > quietFrom + 20) {
					cl->bad++;
				}
			} else {
				SimClient *cl = &g_cli[-1 - p->to];
				uint16_t newest;
				uint32_t bits;
				float vp[3];
				CHECK(NetRep_ReadAck(p->data, p->size, &newest, &bits, vp));
				NetRepSrv_Ack(g_srv, cl->srv, newest, bits);
			}
		}
		g_nq = k;
		/* the acks go back, through the same link */
		for (int c = 0; c < SIM_CLIENTS; c++) {
			uint16_t newest;
			uint32_t bits;
			uint8_t buf[32];
			if (NetRepCli_Ack(g_cli[c].view, &newest, &bits)) {
				int n = NetRep_WriteAck(buf, sizeof(buf), newest, bits, g_cli[c].vp);
				CHECK(n == 19);
				if (quiet || (int)(Rnd() % 100) >= lossPct) {
					Sim_Push(-1 - c, t + (quiet ? 1 : 1 + (int)(Rnd() % (unsigned)(maxDelay + 1))), buf, n);
				}
			}
		}
	}
	CHECK(overBudget == 0);
	/* after the quiet phase: each client holds exactly its interest, with the server's states */
	for (int c = 0; c < SIM_CLIENTS; c++) {
		SimClient *cl = &g_cli[c];
		int want = 0, have = 0, wrong = 0, ghosts = 0;
		for (int slot = 0; slot < NETREP_MAX_ENTS; slot++) {
			int kind, look;
			const NetRepState *st = NetRepCli_State(cl->view, slot, &kind, &look);
			int e = g_slotEnt[slot];
			int in = 0;
			if (e >= 0) {
				float dx = g_ent[e].p[0] - cl->vp[0], dy = g_ent[e].p[1] - cl->vp[1], dz = g_ent[e].p[2] - cl->vp[2];
				float d = sqrtf(dx * dx + dy * dy + dz * dz);
				in = d <= NETREP_RADIUS ? 1 : d <= NETREP_RADIUS * NETREP_KEEP ? 2 : 0;
			}
			want += in == 1;
			if (st) {
				have++;
				if (!in) {
					ghosts++;
				} else if (NetRep_Diff(st, &g_ent[e].st) || kind != g_ent[e].kind ||
					strcmp(NetRepCli_LookString(cl->view, look), g_lookNames[g_ent[e].look])) {
					wrong++;
				}
			} else if (in == 1) {
				wrong++;
			}
			/* the server's base is the client's state */
			const NetRepState *b = NetRepSrv_Base(cl->srv, slot);
			if (st && b) {
				CHECK(NetRep_Diff(st, b) == 0);
			}
		}
		const NetRepClientStats *ss = NetRepSrv_Stats(cl->srv);
		printf("  loss %d%% dup %d%% delay %d rate %u client %d: interest %d, holds %d, wrong %d, ghosts %d, bad msgs %d, "
			"creates %d removes %d updates %d; server: %u snaps %u msgs %u records (%u creates %u fulls %u removes), "
			"%.0f bytes/s, max snapshot %u bytes %u records, %u acked, %u waited\n", lossPct, dupPct, maxDelay, rate, c,
			want, have, wrong, ghosts, cl->bad, cl->creates, cl->removes, cl->updates, ss->snaps, ss->msgs, ss->records,
			ss->creates, ss->fulls, ss->removes, (double)ss->bytes * NETREP_SNAP_HZ / ss->snaps, ss->maxSnapBytes,
			ss->maxRecords, ss->acked, ss->skipped);
		CHECK(wrong == 0);
		CHECK(ghosts == 0);
		CHECK(cl->bad == 0);
		CHECK(cl->ghostLook == 0);
		CHECK(want > 0);
	}
	CHECK(maxMsg <= NETREP_MSG_MAX);
	for (int c = 0; c < SIM_CLIENTS; c++) {
		NetRepCli_Destroy(g_cli[c].view);
		NetRepSrv_RemoveClient(g_srv, g_cli[c].srv);
	}
	NetRepSrv_Destroy(g_srv);
}

/* random and cut messages: no crash, nothing taken from garbage */
static void Test_Garbage(void) {
	NetRepView *v = NetRepCli_Create();
	uint8_t buf[NETREP_MSG_MAX];
	int taken = 0;
	for (int i = 0; i < 200000; i++) {
		int n = (int)(Rnd() % 200);
		for (int k = 0; k < n; k++) {
			buf[k] = (uint8_t)Rnd();
		}
		if (n > 0 && (i & 1)) {
			buf[0] = NETREP_MSG_SNAP;
		}
		taken += NetRepCli_Message(v, buf, n, NULL);
	}
	/* an empty snapshot message is fine; garbage with records hardly ever */
	printf("  garbage: %d of 200000 taken\n", taken);
	CHECK(taken < 2000);
	uint16_t a;
	uint32_t b;
	float vp[3];
	char level[64];
	uint32_t tick, rate;
	CHECK(!NetRep_ReadAck(buf, 3, &a, &b, vp));
	CHECK(!NetRep_ReadWelcome(buf, 5, &tick, vp, level, sizeof(level)));
	{
		int flags;
		int n = NetRep_WriteHello(buf, sizeof(buf), 12345, NETREP_HELLO_PLAY);
		CHECK(NetRep_ReadHello(buf, n, &rate, &flags) && rate == 12345 && flags == NETREP_HELLO_PLAY);
		CHECK(!NetRep_ReadHello(buf, n - 1, &rate, &flags));
		float view[3] = { 1.0f, -2.0f, 300000.0f };
		n = NetRep_WriteWelcome(buf, sizeof(buf), 777, view, "DownTown");
		CHECK(NetRep_ReadWelcome(buf, n, &tick, vp, level, sizeof(level)) && tick == 777 && !strcmp(level, "DownTown"));
		CHECK(vp[0] == 1.0f && vp[1] == -2.0f && vp[2] == 300000.0f);
	}
	NetRepCli_Destroy(v);
}

/* the input of a tick from a seed, for comparisons */
static NetRepInput Test_MakeInput(uint32_t tick) {
	NetRepInput in;
	memset(&in, 0, sizeof(in));
	in.tick = tick;
	in.move[0] = (int8_t)(tick * 7);
	in.move[1] = (int8_t)-(int)(tick % 128);
	in.move[2] = (int8_t)(tick % 3 - 1);
	in.rot[0] = (int16_t)(tick * 1013);
	in.rot[1] = (int16_t)-(int)(tick * 37);
	in.buttons = (uint16_t)(tick * 0x55);
	in.weapon = (uint8_t)(tick % 7);
	in.slot = (uint16_t)(tick % 2048);
	return in;
}

static int Test_SameInput(const NetRepInput *a, const NetRepInput *b) {
	return a->tick == b->tick && !memcmp(a->move, b->move, 3) && a->rot[0] == b->rot[0] && a->rot[1] == b->rot[1] &&
		a->buttons == b->buttons && a->weapon == b->weapon && a->slot == b->slot;
}

/* a missile's state (Replicate.c): its bees in life, the slot + 1 of its shooter in view[0]; a hit costs a few bytes
 * and its end (bees and the active flag) reads back */
static void Test_MissileState(void) {
	uint8_t buf[64];
	NetRepState a, b, out;
	memset(&a, 0, sizeof(a));
	a.pos[0] = 4000;
	a.vel[2] = -12000;
	a.life = 40;
	a.view[0] = 2048;
	a.flags = NETREP_FL_ACTIVE;
	b = a;
	b.life = 33;
	uint32_t mask = NetRep_Diff(&a, &b);
	CHECK(mask == NETREP_F_LIFE);
	NetRepBuf w;
	NetRepReader r;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_WriteFields(&w, &a, &b, mask);
	CHECK(!w.bad && w.size <= 2);
	b.life = 0;
	b.flags = 0;
	mask = NetRep_Diff(&a, &b);
	CHECK(mask == (NETREP_F_LIFE | NETREP_F_FLAGS));
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_WriteFields(&w, &a, &b, mask);
	NetRepReader_Init(&r, buf, w.size);
	CHECK(NetRep_ReadFields(&r, &a, &out, mask) && out.life == 0 && !(out.flags & NETREP_FL_ACTIVE) &&
		out.view[0] == 2048 && !NetRep_Diff(&out, &b));
}

static void Test_PlayerMessages(void) {
	uint8_t buf[256];
	float view[3] = { 10.0f, -20.0f, 30.0f }, vp[3];
	NetRepInput in[NETREP_INPUT_REDUNDANCY], out[NETREP_INPUT_REDUNDANCY];
	uint16_t newest;
	uint32_t bits, shown, tick;
	int n, player, slot, kind;

	float phys[NETREP_PHYS_MAX] = { 30701.75f, -13300.0f, 1e-30f, 3420.0f }, got[NETREP_PHYS_MAX];
	int nphys;
	n = NetRep_WritePlayer(buf, sizeof(buf), 63, 2047, NETREP_SHIP, 123456, phys, 4);
	CHECK(NetRep_ReadPlayer(buf, n, &player, &slot, &kind, &tick, got, &nphys) && player == 63 && slot == 2047 &&
		kind == NETREP_SHIP && tick == 123456 && nphys == 4 && !memcmp(phys, got, 4 * sizeof(float)));
	CHECK(!NetRep_ReadPlayer(buf, n - 1, &player, &slot, &kind, &tick, got, &nphys));
	n = NetRep_WritePlayer(buf, sizeof(buf), 1, NETREP_NO_SLOT, 0, 7, NULL, 0);
	CHECK(NetRep_ReadPlayer(buf, n, &player, &slot, &kind, &tick, got, &nphys) && slot == NETREP_NO_SLOT && !nphys);
	/* a death: no slot, the kind says why */
	n = NetRep_WritePlayer(buf, sizeof(buf), 9, NETREP_NO_SLOT, NETREP_DEAD, 77, NULL, 0);
	CHECK(NetRep_ReadPlayer(buf, n, &player, &slot, &kind, &tick, got, &nphys) && player == 9 &&
		slot == NETREP_NO_SLOT && kind == NETREP_DEAD && tick == 77 && !nphys);
	/* an ack of a watcher reads as one without input, with or without the count byte */
	n = NetRep_WriteAck(buf, sizeof(buf), 5, 0xf0f0f0f0u, view);
	CHECK(NetRep_ReadAckInput(buf, n, &newest, &bits, vp, out, 4, &kind, &shown) && kind == 0 && newest == 5);
	n = NetRep_WriteAckInput(buf, sizeof(buf), 6, 1, view, in, 0, 0);
	CHECK(NetRep_ReadAckInput(buf, n, &newest, &bits, vp, out, 4, &kind, &shown) && kind == 0 && newest == 6);
	for (uint32_t t = 1000; t < 1200; t += 13) {
		for (int i = 0; i < NETREP_INPUT_REDUNDANCY; i++) {
			in[i] = Test_MakeInput(t - (uint32_t)i * (1 + (t % 2)));
		}
		n = NetRep_WriteAckInput(buf, sizeof(buf), (uint16_t)t, t * 3, view, in, NETREP_INPUT_REDUNDANCY, t * 33);
		CHECK(n > 0 && n <= 88);
		int got;
		CHECK(NetRep_ReadAckInput(buf, n, &newest, &bits, vp, out, NETREP_INPUT_REDUNDANCY, &got, &shown));
		CHECK(got == NETREP_INPUT_REDUNDANCY && newest == (uint16_t)t && bits == t * 3 && shown == t * 33);
		CHECK(vp[0] == 10.0f && vp[1] == -20.0f && vp[2] == 30.0f);
		for (int i = 0; i < got; i++) {
			CHECK(Test_SameInput(&in[i], &out[i]));
		}
		CHECK(NetRep_ReadAck(buf, n, &newest, &bits, vp) && newest == (uint16_t)t);
		/* every cut is refused */
		for (int k = 20; k < n; k++) {
			CHECK(!NetRep_ReadAckInput(buf, k, &newest, &bits, vp, out, NETREP_INPUT_REDUNDANCY, &got, &shown));
		}
	}
	/* inputs out of order or of the same tick are not written */
	in[1] = in[0];
	CHECK(!NetRep_WriteAckInput(buf, sizeof(buf), 1, 0, view, in, 2, 0));
	CHECK(NetRep_QMove(1.0f) == 127 && NetRep_QMove(-3.0f) == -127 && NetRep_QMove(0.5f) == 64);
	CHECK(NetRep_QRot(1.0f) == 1024 && NetRep_QRot(100.0f) == 32767 && NetRep_QRot(-0.25f) == -256);
}

static void Test_Status(void) {
	uint8_t buf[128];
	NetRepStatus a, b;

	memset(&a, 0, sizeof(a));
	a.tick = 0xfffffff0u;
	a.money = -123456;
	a.alarm = 200;
	a.flags = NETREP_ST_ALARM;
	a.nammo = NETREP_AMMO_MAX;
	for (int i = 0; i < NETREP_AMMO_MAX; i++) {
		a.ammo[i] = (uint16_t)(i * 4099);
	}
	int n = NetRep_WriteStatus(buf, sizeof(buf), &a);
	CHECK(n > 0 && NetRep_ReadStatus(buf, n, &b) && !NetRep_StatusDiffers(&a, &b) && b.tick == a.tick);
	for (int k = 0; k < n; k++) {
		CHECK(!NetRep_ReadStatus(buf, k, &b));
	}
	CHECK(!NetRep_WriteStatus(buf, 10, &a));
	a.nammo = 0;
	a.money = 7;
	n = NetRep_WriteStatus(buf, sizeof(buf), &a);
	CHECK(n == 9 && NetRep_ReadStatus(buf, n, &b) && b.money == 7 && !b.nammo);
	b.ammo[0] = 9;
	CHECK(!NetRep_StatusDiffers(&a, &b));
	b.alarm = 1;
	CHECK(NetRep_StatusDiffers(&a, &b));
	buf[8] = NETREP_AMMO_MAX + 1;
	CHECK(!NetRep_ReadStatus(buf, n, &b));
}

static void Test_Shots(void) {
	uint8_t buf[NETREP_MSG_MAX];
	NetRepShot in[NETREP_SHOTS_MAX], out[NETREP_SHOTS_MAX];
	uint32_t tick;
	int n;

	for (int i = 0; i < NETREP_SHOTS_MAX; i++) {
		NetRepShot *x = &in[i];
		x->slot = (uint16_t)(i * 41 % NETREP_MAX_ENTS);
		x->weapon = (uint8_t)(i % 7);
		x->count = (uint8_t)(1 + i % 5);
		x->target = i % 3 ? (uint16_t)(i * 7) : NETREP_NO_SLOT;
		for (int k = 0; k < 3; k++) {
			x->pos[k] = (int32_t)(i * 100003 * (k + 1)) - 2000000;
			x->from[k] = x->pos[k] + (i % 2 ? 40000 : -7) * (k + 1);
		}
	}
	int size = NetRep_WriteShots(buf, sizeof(buf), 99, in, NETREP_SHOTS_MAX);
	CHECK(size > 0 && size <= NETREP_MSG_MAX);
	CHECK(NetRep_ReadShots(buf, size, &tick, out, NETREP_SHOTS_MAX, &n) && tick == 99 && n == NETREP_SHOTS_MAX);
	for (int i = 0; i < n; i++) {
		CHECK(!memcmp(&in[i].pos, &out[i].pos, sizeof(in[i].pos)) && !memcmp(&in[i].from, &out[i].from,
			sizeof(in[i].from)) && in[i].slot == out[i].slot && in[i].weapon == out[i].weapon &&
			in[i].count == out[i].count && in[i].target == out[i].target);
	}
	for (int k = 0; k < size; k++) {
		CHECK(!NetRep_ReadShots(buf, k, &tick, out, NETREP_SHOTS_MAX, &n));
	}
	CHECK(!NetRep_ReadShots(buf, size, &tick, out, 3, &n));
	CHECK(!NetRep_WriteShots(buf, sizeof(buf), 1, in, NETREP_SHOTS_MAX + 1));
	CHECK(!NetRep_WriteShots(buf, 40, 1, in, NETREP_SHOTS_MAX));
	size = NetRep_WriteShots(buf, sizeof(buf), 5, in, 0);
	CHECK(size == 6 && NetRep_ReadShots(buf, size, &tick, out, 1, &n) && n == 0);
}

/* the server's input buffer against a link with loss, duplicates, delay and jitter: what it runs is the client's
 * input of that tick whenever one of the redundant copies came in time, else the input before */
static void Test_InBuf(int lossPct, int delayTicks, int jitterTicks, int lead) {
	enum { TICKS = 3000, MAXQ = 4096 };
	static struct {
		uint32_t at;
		NetRepInput in[NETREP_INPUT_REDUNDANCY];
		int n;
	} q[MAXQ];
	static uint8_t arrived[TICKS + 64];
	NetRepInBuf b;
	int nq = 0, ran = 0, exact = 0, wrongRepeat = 0;
	NetRepInput last;

	memset(arrived, 0, sizeof(arrived));
	memset(&last, 0, sizeof(last));
	NetRepInBuf_Init(&b);
	for (uint32_t now = 1; now < TICKS; now++) {
		/* the client sends the inputs for tick now + lead and the ticks before */
		uint32_t t = now + (uint32_t)lead;
		int copies = 1 + (Rnd() % 100 < 5);
		for (int c = 0; c < copies && nq < MAXQ; c++) {
			if ((int)(Rnd() % 100) < lossPct) {
				continue;
			}
			q[nq].at = now + (uint32_t)delayTicks + (jitterTicks ? Rnd() % (uint32_t)(jitterTicks + 1) : 0);
			q[nq].n = NETREP_INPUT_REDUNDANCY;
			for (int i = 0; i < NETREP_INPUT_REDUNDANCY; i++) {
				q[nq].in[i] = Test_MakeInput(t - (uint32_t)i);
			}
			nq++;
		}
		/* the server takes what came, then runs tick now */
		for (int i = 0; i < nq;) {
			if (q[i].at > now) {
				i++;
				continue;
			}
			for (int k = 0; k < q[i].n; k++) {
				if (NetRepInBuf_Put(&b, &q[i].in[k], now - 1) && q[i].in[k].tick < TICKS + 64) {
					arrived[q[i].in[k].tick] = 1;
				}
			}
			q[i] = q[--nq];
		}
		NetRepInput x;
		int have = NetRepInBuf_Take(&b, now, &x);
		NetRepInput want = Test_MakeInput(now);
		ran++;
		CHECK(x.tick == now);
		if (have) {
			exact++;
			CHECK(arrived[now] && Test_SameInput(&x, &want));
		} else {
			NetRepInput rep = last;
			rep.tick = now;
			wrongRepeat += !Test_SameInput(&x, &rep);
		}
		last = x;
	}
	CHECK(wrongRepeat == 0);
	CHECK(b.got + 0u >= (uint32_t)exact);
	printf("  inbuf loss %d%% delay %d jitter %d lead %d: %d of %d ticks with their input, %u late, %u dup, %u "
		"missed, lead avg %.2f\n", lossPct, delayTicks, jitterTicks, lead, exact, ran, b.late, b.dup, b.missed,
		b.leadN ? (double)b.leadSum / b.leadN : 0.0);
	if (!lossPct && lead > delayTicks + jitterTicks) {
		CHECK(exact >= ran - lead - 2);
	}
	if (lossPct <= 10 && lead > delayTicks + jitterTicks) {
		CHECK(exact >= ran * 97 / 100);
	}
	if (lead < delayTicks) {
		CHECK(exact == 0 && b.late > 0);
	}
	/* a restart forgets the waiting inputs and repeats no input (a tick past the ones the run sent) */
	NetRepInput w = Test_MakeInput(TICKS + 20), x;
	CHECK(NetRepInBuf_Put(&b, &w, TICKS - 1));
	NetRepInBuf_Restart(&b);
	CHECK(!NetRepInBuf_Take(&b, TICKS, &x) && x.tick == TICKS && !x.buttons && !x.move[0] && !x.rot[0]);
	CHECK(!NetRepInBuf_Take(&b, TICKS + 20, &x) && !x.buttons);
	/* too far ahead and late inputs */
	NetRepInput far = Test_MakeInput(TICKS + NETREP_INPUT_AHEAD + 5);
	CHECK(!NetRepInBuf_Put(&b, &far, TICKS - 1));
	NetRepInput old = Test_MakeInput(TICKS - 10);
	CHECK(!NetRepInBuf_Put(&b, &old, TICKS - 1));
}

static void Test_Clock(void) {
	NetRepClock c;
	double rtt = 0.080;
	NetRepClock_Init(&c);
	/* the server's tick t is run at t / 30 s of the server's clock, which is local + 5.5 s; snapshots come
	 * one-way 40 ms later with 0..30 ms of jitter, every 2 or 1 ticks */
	double maxErr = 0.0;
	for (uint32_t t = 30; t < 3000; t += 1 + (t % 2)) {
		double send = (double)t / NETREP_TICK_HZ - 5.5;
		double now = send + rtt / 2 + (double)(Rnd() % 31) / 1000.0;
		NetRepClock_Sample(&c, t, now, rtt);
		if (t > 300) {
			double err = fabs(NetRepClock_Server(&c, now) - (now + 5.5));
			maxErr = err > maxErr ? err : maxErr;
		}
	}
	printf("  clock: largest error %.1f ms (jitter 0..30 ms)\n", maxErr * 1000.0);
	CHECK(maxErr < 0.035);
	/* the input tick: the server time half a round trip later plus the lead */
	double now = 100.0;
	uint32_t it = NetRepClock_InputTick(&c, now, rtt, 1.0 / NETREP_TICK_HZ);
	double sv = NetRepClock_Server(&c, now) + rtt / 2;
	CHECK(it >= (uint32_t)(sv * NETREP_TICK_HZ) && it <= (uint32_t)(sv * NETREP_TICK_HZ) + 2);
	/* a jump of the server (a stall) is taken at once */
	NetRepClock_Sample(&c, 100000, now, rtt);
	CHECK(fabs(NetRepClock_Server(&c, now) - (100000.0 / NETREP_TICK_HZ + rtt / 2)) < 1e-6);
}

/* the followed time: even steps for a target that jitters, the rate bound, a jump taken at once */
static void Test_Follow(void) {
	NetRepFollow f;
	NetRepFollow_Init(&f, 0.25);
	double t = 100.0, prev = 0.0, dt = 1.0 / 60.0;
	uint32_t seed = 7;
	for (int i = 0; i < 6000; i++) {
		t += dt;
		seed = seed * 1103515245u + 12345u;
		double jit = ((double)(seed >> 16 & 0xffff) / 65535.0 - 0.5) * 0.016;
		double v = NetRepFollow_Step(&f, t + jit, dt);
		if (i > 600) {
			double step = v - prev;
			CHECK(step > dt * (1.0 - NETREP_FOLLOW_RATE) - 1e-9 && step < dt * (1.0 + NETREP_FOLLOW_RATE) + 1e-9);
			CHECK(fabs(v - t) < 0.012);
		}
		prev = v;
	}
	/* a step of 0.1 s is closed at the rate bound, one of 1 s at once */
	t += 0.1;
	double v0 = NetRepFollow_Step(&f, t, 0.0);
	CHECK(fabs(v0 - (t - 0.1)) < 0.02);
	for (int i = 0; i < 300; i++) {
		t += dt;
		v0 = NetRepFollow_Step(&f, t, dt);
	}
	CHECK(fabs(v0 - t) < 0.002);
	t += 1.0;
	CHECK(NetRepFollow_Step(&f, t, dt) == t);
}

static void Test_Predict(void) {
	NetRepPredHist h;
	NetRepPred p;
	NetRepInput in;
	NetRepPredHist_Init(&h);
	CHECK(!NetRepPredHist_Get(&h, 0) && !NetRepPredHist_Get(&h, 5) && NetRepPredHist_Error(&h, 5, p.pos) < 0.0f);
	for (uint32_t t = 1; t <= 200; t++) {
		memset(&p, 0, sizeof(p));
		p.tick = t;
		p.pos[0] = (float)t * 10.0f;
		in = Test_MakeInput(9999);
		NetRepPredHist_Put(&h, &p, &in);
	}
	CHECK(!NetRepPredHist_Get(&h, 200 - NETREP_PRED));
	CHECK(NetRepPredHist_Get(&h, 201 - NETREP_PRED) && NetRepPredHist_Get(&h, 200)->pos[0] == 2000.0f);
	CHECK(NetRepPredHist_Input(&h, 150) && NetRepPredHist_Input(&h, 150)->tick == 150);
	float sp[3] = { 1503.0f, 4.0f, 0.0f };
	CHECK(fabsf(NetRepPredHist_Error(&h, 150, sp) - 5.0f) < 1e-4f);

	/* the smoothing keeps the drawn position, fades by half per half life, snaps big jumps */
	NetRepSmooth s;
	NetRepSmooth_Init(&s, 0.1f, 500.0f);
	float before[3] = { 100.0f, 0.0f, 0.0f }, after[3] = { 80.0f, 0.0f, 0.0f };
	NetRepSmooth_Correct(&s, before, after);
	CHECK(s.off[0] == 20.0f);
	NetRepSmooth_Step(&s, 0.1f);
	CHECK(fabsf(s.off[0] - 10.0f) < 1e-3f);
	NetRepSmooth_Correct(&s, after, before);
	CHECK(fabsf(s.off[0] + 10.0f) < 1e-3f);
	for (int i = 0; i < 100; i++) {
		NetRepSmooth_Step(&s, 0.05f);
	}
	CHECK(s.off[0] == 0.0f);
	float far[3] = { 100000.0f, 0.0f, 0.0f };
	NetRepSmooth_Correct(&s, far, before);
	CHECK(s.off[0] == 0.0f && s.off[1] == 0.0f && s.off[2] == 0.0f);
	/* a large one fades slower at first: 420 units (1.1 knees above the knee) have 2.1 times the half life */
	float big[3] = { 500.0f, 0.0f, 0.0f };
	NetRepSmooth_Correct(&s, big, after);
	CHECK(s.off[0] == 420.0f);
	NetRepSmooth_Step(&s, 0.1f);
	CHECK(s.off[0] > 298.0f && s.off[0] < 305.0f);
}

/* the interpolation delay follows the jitter of the arrivals, slowly and within its bounds */
static double Test_DelayRun(double jitter, float loss, double secs) {
	NetRepClock c;
	NetRepDelay d;
	double now = 100.0, rtt = 0.05, prev = 0.0;
	NetRepClock_Init(&c);
	NetRepDelay_Init(&d);
	for (uint32_t tick = 3000; now < 100.0 + secs; tick++) {
		now += 1.0 / NETREP_TICK_HZ;
		if (tick % 3 == 2) {
			continue;			/* no snapshot on one tick of three */
		}
		double at = now + rtt * 0.5 + (double)(Rnd() % 1000) / 1000.0 * jitter;
		NetRepDelay_Sample(&d, &c, tick, at, rtt);
		NetRepClock_Sample(&c, tick, at, rtt);
		double dl = NetRepDelay_Step(&d, 1.0 / NETREP_TICK_HZ, loss);
		CHECK(dl >= NETREP_DELAY_MIN - 1e-9 && dl <= NETREP_DELAY_MAX + 1e-9);
		CHECK(!prev || fabs(dl - prev) <= NETREP_DELAY_RATE * 2.0 / NETREP_TICK_HZ + 1e-9);
		prev = dl;
	}
	return prev;
}

static void Test_Delay(void) {
	double calm = Test_DelayRun(0.0, 0.0f, 20.0);
	double some = Test_DelayRun(0.03, 0.0f, 20.0);
	double lossy = Test_DelayRun(0.0, 0.05f, 20.0);
	double wild = Test_DelayRun(0.5, 0.0f, 30.0);
	printf("  delay: calm %.0f ms, jitter 30 ms %.0f ms, 5%% loss %.0f ms, jitter 500 ms %.0f ms\n", calm * 1000.0,
		some * 1000.0, lossy * 1000.0, wild * 1000.0);
	CHECK(calm < 0.075 && calm >= NETREP_DELAY_BASE);
	CHECK(some > calm + 0.01 && some < 0.15);
	CHECK(lossy > calm + 0.04);
	CHECK(wild <= NETREP_DELAY_MAX + 1e-9 && wild > 0.2);
}

/* the positions of a slot at a server time from the snapshots kept (the lag compensation) */
static void Test_PosAt(void) {
	NetRepServer *s = NetRepSrv_Create();
	NetRepState st;
	float pos[3];
	int look = NetRepSrv_Look(s, "C|Dtritus");
	CHECK(!NetRepSrv_PosAt(s, 3, 1.0, pos));
	for (uint32_t tick = 3; tick < 300; tick += 3) {
		memset(&st, 0, sizeof(st));
		st.pos[0] = NetRep_QPos((float)tick * 100.0f);
		st.dt = 10;
		NetRepSrv_Begin(s, tick);
		NetRepSrv_Set(s, 3, NETREP_CHAR, look, &st);
		NetRepSrv_End(s);
	}
	/* the sample of tick t is at t / 30 + 0.01 s */
	CHECK(NetRepSrv_PosAt(s, 3, 290.0 / 30.0 + 0.01, pos) && fabsf(pos[0] - 29000.0f) < 0.5f);
	CHECK(NetRepSrv_PosAt(s, 3, 291.5 / 30.0 + 0.01, pos) && fabsf(pos[0] - 29150.0f) < 0.5f);
	/* past the newest: the newest; before the oldest kept: the oldest */
	CHECK(NetRepSrv_PosAt(s, 3, 1000.0, pos) && fabsf(pos[0] - 29700.0f) < 0.5f);
	CHECK(NetRepSrv_PosAt(s, 3, 0.0, pos) && fabsf(pos[0] - (297.0f - 3.0f * (NETREP_HIST - 1)) * 100.0f) < 0.5f);
	CHECK(!NetRepSrv_PosAt(s, 4, 9.0, pos));
	NetRepSrv_Destroy(s);
}

int main(void) {
	Test_Varints();
	Test_Quant();
	Test_Fields();
	Test_Grid();
	Test_Priority();
	printf("simulation:\n");
	Sim_Run(0, 0, 0, 300, 48 * 1024);
	Sim_Run(10, 5, 3, 600, 48 * 1024);
	Sim_Run(30, 10, 6, 600, 16 * 1024);
	Sim_Run(5, 0, 2, 600, 8 * 1024);
	Test_Garbage();
	printf("players:\n");
	Test_PlayerMessages();
	Test_MissileState();
	Test_Status();
	Test_Shots();
	Test_InBuf(0, 0, 0, 2);
	Test_InBuf(0, 3, 2, 6);
	Test_InBuf(10, 2, 2, 5);
	Test_InBuf(30, 2, 3, 6);
	Test_InBuf(5, 6, 0, 3);
	Test_Clock();
	Test_Follow();
	Test_Delay();
	Test_Predict();
	Test_PosAt();
	printf("netrep_test: %d checks, %d failed\n", g_checks, g_fails);
	return g_fails ? 1 : 0;
}
