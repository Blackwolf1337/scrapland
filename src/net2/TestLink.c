/*
 * Net2: the test link of a host on real sockets (Net2_SetTestLink, include/scrap/net2.h). The datagrams it sends are
 * dropped by chance or held back by a delay plus a random jitter (which may reorder them) and go out from the next
 * Poll or Flush once due; for measurements of the game under loss and latency on loopback.
 */
#include <string.h>

#include "net2local.h"

#define N2T_SLOTS 1024

typedef struct N2TPacket {
	uint64_t due;
	Net2Addr to;
	int size;
	uint8_t data[NET2_MTU];
} N2TPacket;

struct Net2TestLink {
	Net2Host *h;
	Net2Io inner;
	uint32_t loss;					/* per 65536 */
	uint32_t delay, jitter;			/* ms */
	uint32_t rng;
	uint32_t dropped, held, full;
	int count;
	N2TPacket q[N2T_SLOTS];
};

static uint32_t N2T_Rand(Net2TestLink *t) {
	t->rng ^= t->rng << 13;
	t->rng ^= t->rng >> 17;
	t->rng ^= t->rng << 5;
	return t->rng;
}

/* the held datagrams that are due (all with force) */
void Net2Test_Release(Net2Host *h, int force) {
	Net2TestLink *t = h->test;
	if (!t) {
		return;
	}
	for (int i = 0; i < t->count;) {
		N2TPacket *p = &t->q[i];
		if (!force && p->due > h->now) {
			i++;
			continue;
		}
		t->inner.send(t->inner.ctx, &p->to, p->data, p->size);
		*p = t->q[--t->count];
	}
}

static int N2T_Send(void *ctx, const Net2Addr *to, const uint8_t *data, int size) {
	Net2TestLink *t = ctx;
	if (t->loss && (N2T_Rand(t) & 0xffff) < t->loss) {
		t->dropped++;
		return 1;
	}
	if (!t->delay && !t->jitter) {
		return t->inner.send(t->inner.ctx, to, data, size);
	}
	if (t->count == N2T_SLOTS || size > NET2_MTU) {
		t->full++;
		return 1;
	}
	N2TPacket *p = &t->q[t->count++];
	p->due = t->h->now + t->delay + (t->jitter ? N2T_Rand(t) % (t->jitter + 1) : 0);
	p->to = *to;
	p->size = size;
	memcpy(p->data, data, (size_t)size);
	t->held++;
	return 1;
}

static int N2T_Recv(void *ctx, Net2Addr *from, uint8_t *buf, int cap) {
	Net2TestLink *t = ctx;
	Net2Test_Release(t->h, 0);
	return t->inner.recv(t->inner.ctx, from, buf, cap);
}

int Net2_SetTestLink(Net2Host *h, float lossPercent, uint32_t delayMs, uint32_t jitterMs, uint32_t seed) {
	if (h->test || (lossPercent <= 0.0f && !delayMs && !jitterMs)) {
		return h->test ? NET2_E_STATE : NET2_OK;
	}
	Net2TestLink *t = Net2_Alloc(h, sizeof(*t));
	if (!t) {
		return NET2_E_NOMEM;
	}
	t->h = h;
	t->inner = h->io;
	t->loss = lossPercent >= 100.0f ? 65536u : (uint32_t)(lossPercent * 655.36f);
	t->delay = delayMs;
	t->jitter = jitterMs;
	t->rng = seed ? seed : 0x2545f491u;
	h->test = t;
	h->io.ctx = t;
	h->io.send = N2T_Send;
	h->io.recv = N2T_Recv;
	return NET2_OK;
}

void Net2Test_Free(Net2Host *h) {
	Net2TestLink *t = h->test;
	if (t) {
		Net2Test_Release(h, 1);
		h->io = t->inner;
		h->test = NULL;
		Net2_Free(h, t);
	}
}
