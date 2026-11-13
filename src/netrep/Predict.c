/*
 * NetRep: the players' input and the client's prediction (include/scrap/netrep.h, stage M3b): the server's input
 * buffer, the client's clock, its predicted states and the smoothing of corrections.
 */
#include <math.h>
#include <string.h>

#include "scrap/netrep.h"

#define CLOCK_JUMP 0.25				/* s */
#define CLOCK_GAIN 0.05

/* ---- the server's input buffer ---- */

void NetRepInBuf_Init(NetRepInBuf *b) {
	memset(b, 0, sizeof(*b));
}

int NetRepInBuf_Put(NetRepInBuf *b, const NetRepInput *in, uint32_t now) {
	int i = (int)(in->tick % NETREP_INBUF);
	int known = b->have[i] && b->in[i].tick == in->tick;
	if (in->tick <= b->ran || in->tick <= now) {
		if (known) {
			b->dup++;
		} else {
			b->late++;
		}
		return 0;
	}
	if (in->tick > now + NETREP_INPUT_AHEAD) {
		b->ahead++;
		return 0;
	}
	if (known) {
		b->dup++;
		return 0;
	}
	b->in[i] = *in;
	b->have[i] = 1;
	b->got++;
	b->leadSum += (int32_t)(in->tick - now);
	b->leadN++;
	return 1;
}

int NetRepInBuf_Take(NetRepInBuf *b, uint32_t tick, NetRepInput *out) {
	int i = (int)(tick % NETREP_INBUF);
	int have = b->have[i] == 1 && b->in[i].tick == tick;
	if (have) {
		b->last = b->in[i];
		b->have[i] = 2;
	} else {
		b->last.tick = tick;
		b->missed++;
		b->have[i] = 0;
	}
	b->ran = tick;
	*out = b->last;
	return have;
}

void NetRepInBuf_Restart(NetRepInBuf *b) {
	memset(b->have, 0, sizeof(b->have));
	uint32_t tick = b->last.tick;
	memset(&b->last, 0, sizeof(b->last));
	b->last.tick = tick;
}

/* ---- the client's clock ---- */

void NetRepClock_Init(NetRepClock *c) {
	memset(c, 0, sizeof(*c));
	c->jump = CLOCK_JUMP;
	c->gain = CLOCK_GAIN;
}

void NetRepClock_Sample(NetRepClock *c, uint32_t tick, double now, double rtt) {
	double target = (double)tick / NETREP_TICK_HZ + rtt * 0.5 - now;
	double d = target - c->offset;
	if (!c->have || d > c->jump || d < -c->jump) {
		c->offset = target;
		c->have = 1;
		return;
	}
	/* a sample can only come late (queues, a slow frame): the newest ticks move the clock forward at once, late
	 * ones pull it back slowly */
	c->offset += d > 0.0 ? d * 0.5 : d * c->gain;
}

double NetRepClock_Server(const NetRepClock *c, double now) {
	return now + c->offset;
}

uint32_t NetRepClock_InputTick(const NetRepClock *c, double now, double rtt, double lead) {
	double t = NetRepClock_Server(c, now) + rtt * 0.5 + lead;
	return t <= 0.0 ? 0 : (uint32_t)(t * NETREP_TICK_HZ);
}

/* ---- the interpolation delay ---- */

void NetRepDelay_Init(NetRepDelay *d) {
	memset(d, 0, sizeof(*d));
	d->target = d->delay = NETREP_DELAY_BASE + 0.03;
}

void NetRepDelay_Sample(NetRepDelay *d, const NetRepClock *c, uint32_t tick, double now, double rtt) {
	if (!c->have) {
		return;
	}
	double dev = (double)tick / NETREP_TICK_HZ + rtt * 0.5 - now - c->offset;
	dev = dev < 0.0 ? -dev : dev;
	/* a jump of the clock (a stall) is not jitter */
	if (dev > c->jump) {
		return;
	}
	d->dev = d->have ? d->dev + (dev - d->dev) * 0.0625 : dev;
	d->have = 1;
}

double NetRepDelay_Step(NetRepDelay *d, double dt, float loss) {
	double t = NETREP_DELAY_BASE + NETREP_DELAY_JITTER * d->dev;
	t += loss > NETREP_DELAY_LOSS ? 1.0 / NETREP_SNAP_HZ : 0.0;
	t = t < NETREP_DELAY_MIN ? NETREP_DELAY_MIN : t > NETREP_DELAY_MAX ? NETREP_DELAY_MAX : t;
	d->target = t;
	double step = NETREP_DELAY_RATE * dt;
	d->delay += t > d->delay + step ? step : t < d->delay - step ? -step : t - d->delay;
	return d->delay;
}

/* ---- the predicted states ---- */

void NetRepPredHist_Init(NetRepPredHist *h) {
	memset(h, 0, sizeof(*h));
}

void NetRepPredHist_Put(NetRepPredHist *h, const NetRepPred *p, const NetRepInput *in) {
	int i = (int)(p->tick % NETREP_PRED);
	h->p[i] = *p;
	h->in[i] = *in;
	h->in[i].tick = p->tick;
}

const NetRepPred *NetRepPredHist_Get(const NetRepPredHist *h, uint32_t tick) {
	const NetRepPred *p = &h->p[tick % NETREP_PRED];
	return tick && p->tick == tick ? p : NULL;
}

const NetRepInput *NetRepPredHist_Input(const NetRepPredHist *h, uint32_t tick) {
	return NetRepPredHist_Get(h, tick) ? &h->in[tick % NETREP_PRED] : NULL;
}

float NetRepPredHist_Error(const NetRepPredHist *h, uint32_t tick, const float *serverPos) {
	const NetRepPred *p = NetRepPredHist_Get(h, tick);
	if (!p) {
		return -1.0f;
	}
	float dx = serverPos[0] - p->pos[0], dy = serverPos[1] - p->pos[1], dz = serverPos[2] - p->pos[2];
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* ---- the smoothing of corrections ---- */

void NetRepSmooth_Init(NetRepSmooth *s, float halfLife, float snap) {
	memset(s, 0, sizeof(*s));
	s->halfLife = halfLife;
	s->snap = snap;
}

void NetRepSmooth_Correct(NetRepSmooth *s, const float *before, const float *after) {
	float d[3], len2 = 0.0f;
	for (int k = 0; k < 3; k++) {
		d[k] = s->off[k] + before[k] - after[k];
		len2 += d[k] * d[k];
	}
	if (len2 > s->snap * s->snap) {
		memset(s->off, 0, sizeof(s->off));
		return;
	}
	memcpy(s->off, d, sizeof(d));
}

void NetRepSmooth_Step(NetRepSmooth *s, float dt) {
	/* a large offset fades slower at first (a correction after a collision or a hit glides back) */
	float len = sqrtf(s->off[0] * s->off[0] + s->off[1] * s->off[1] + s->off[2] * s->off[2]);
	float grow = len > NETREP_SMOOTH_KNEE ? (len - NETREP_SMOOTH_KNEE) / NETREP_SMOOTH_KNEE : 0.0f;
	float hl = s->halfLife * (1.0f + (grow > NETREP_SMOOTH_SLOW - 1.0f ? NETREP_SMOOTH_SLOW - 1.0f : grow));
	float k = hl > 0.0f ? exp2f(-dt / hl) : 0.0f;
	for (int i = 0; i < 3; i++) {
		s->off[i] *= k;
		if (fabsf(s->off[i]) < 0.01f) {
			s->off[i] = 0.0f;
		}
	}
}
