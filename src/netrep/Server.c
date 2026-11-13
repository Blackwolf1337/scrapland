/*
 * NetRep: the server side (include/scrap/netrep.h). The snapshot states of the last NETREP_HIST snapshots per slot,
 * the clients with their bases, interest, priorities, budget and the messages sent for their acks.
 *
 * Per client and slot: lastSent is the snapshot of the newest create, full or delta record sent, removeSent and
 * removeAck the ones of the newest remove sent and acked. The client may hold the entity while lastSent > removeAck.
 * A remove makes the base invalid at once (the client drops the entity when it takes the remove); an ack of a record
 * older than the newest remove sent sets no base.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/netrep.h"

#define REP_MAX_CLIENTS 1024
#define REP_MSG_RECS (NETREP_MSG_MAX / 3)	/* a record is at least 3 bytes */
#define REP_REC_MAX (32 + NETREP_LOOK_MAX + 3 * 5 * 3 + 2 * 5 * 3)

typedef struct RepHist {
	uint32_t snap;
	NetRepState st;
} RepHist;

typedef struct RepSlot {
	uint32_t liveSnap;				/* the snapshot that set it last */
	uint8_t kind;
	uint16_t look;
	float x, y, z;
} RepSlot;

typedef struct RepEntry {
	uint16_t slot;
	uint8_t op;						/* NETREP_OP_ | NETREP_REC_LOOK */
} RepEntry;

typedef struct RepSent {
	uint16_t seq;
	uint8_t used, acked;
	uint32_t snap;
	uint16_t n;
} RepSent;

struct NetRepClient {
	int index;
	float view[3];
	float radius;
	uint32_t rate;
	int64_t tokens;
	uint16_t seq;
	RepSent sent[NETREP_SENT];
	RepEntry ent[NETREP_SENT][REP_MSG_RECS];
	NetRepState base[NETREP_MAX_ENTS];
	uint32_t baseSnap[NETREP_MAX_ENTS];	/* 0: no base */
	uint32_t lastSent[NETREP_MAX_ENTS];
	uint32_t removeSent[NETREP_MAX_ENTS];
	uint32_t removeAck[NETREP_MAX_ENTS];
	uint32_t mark[NETREP_MAX_ENTS];		/* the snapshot it was in the interest */
	float accum[NETREP_MAX_ENTS];
	uint8_t lookKnown[NETREP_MAX_LOOKS / 8];
	uint16_t lookMsg[NETREP_MAX_LOOKS];	/* the message of this build that carries the string, + 1 */
	NetRepClientStats st;
};

struct NetRepServer {
	uint32_t snap, tick;
	int maxSlot;					/* slots below it were used */
	int cursor;						/* NetRepSrv_FreeSlot goes round */
	int live;
	RepSlot slot[NETREP_MAX_ENTS];
	RepHist hist[NETREP_MAX_ENTS][NETREP_HIST];
	NetRepGrid grid;
	int numLooks;
	char looks[NETREP_MAX_LOOKS][NETREP_LOOK_MAX + 1];
	uint32_t lookHash[NETREP_MAX_LOOKS];
	NetRepClient *clients[REP_MAX_CLIENTS];
	int numClients;
	uint16_t query[NETREP_MAX_ENTS];
	NetRepCand cand[NETREP_MAX_ENTS];
};

NetRepServer *NetRepSrv_Create(void) {
	NetRepServer *s = calloc(1, sizeof(*s));
	if (s) {
		NetRepGrid_Clear(&s->grid, NETREP_CELL);
	}
	return s;
}

void NetRepSrv_Destroy(NetRepServer *s) {
	if (!s) {
		return;
	}
	for (int i = 0; i < s->numClients; i++) {
		free(s->clients[i]);
	}
	free(s);
}

static uint32_t Rep_StrHash(const char *p) {
	uint32_t h = 2166136261u;
	while (*p) {
		h = (h ^ (uint8_t)*p++) * 16777619u;
	}
	return h;
}

int NetRepSrv_Look(NetRepServer *s, const char *look) {
	char buf[NETREP_LOOK_MAX + 1];
	size_t n = strlen(look);
	if (n > NETREP_LOOK_MAX) {
		n = NETREP_LOOK_MAX;
	}
	memcpy(buf, look, n);
	buf[n] = 0;
	uint32_t h = Rep_StrHash(buf);
	for (int i = 0; i < s->numLooks; i++) {
		if (s->lookHash[i] == h && !strcmp(s->looks[i], buf)) {
			return i;
		}
	}
	if (s->numLooks >= NETREP_MAX_LOOKS) {
		return -1;
	}
	memcpy(s->looks[s->numLooks], buf, n + 1);
	s->lookHash[s->numLooks] = h;
	return s->numLooks++;
}

static int Rep_Exists(const NetRepClient *c, int slot) {
	return c->lastSent[slot] > c->removeAck[slot];
}

int NetRepSrv_FreeSlot(const NetRepServer *s) {
	for (int k = 0; k < NETREP_MAX_ENTS; k++) {
		int slot = (s->cursor + k) % NETREP_MAX_ENTS;
		if (s->slot[slot].liveSnap && s->slot[slot].liveSnap + 1 >= s->snap) {
			continue;
		}
		int held = 0;
		for (int i = 0; i < s->numClients && !held; i++) {
			held = Rep_Exists(s->clients[i], slot);
		}
		if (!held) {
			((NetRepServer *)s)->cursor = (slot + 1) % NETREP_MAX_ENTS;
			return slot;
		}
	}
	return -1;
}

uint32_t NetRepSrv_Begin(NetRepServer *s, uint32_t tick) {
	s->snap++;
	s->tick = tick;
	s->live = 0;
	NetRepGrid_Clear(&s->grid, NETREP_CELL);
	return s->snap;
}

void NetRepSrv_Set(NetRepServer *s, int slot, int kind, int look, const NetRepState *st) {
	if (slot < 0 || slot >= NETREP_MAX_ENTS || look < 0) {
		return;
	}
	RepSlot *e = &s->slot[slot];
	e->liveSnap = s->snap;
	e->kind = (uint8_t)kind;
	e->look = (uint16_t)look;
	e->x = NetRep_UPos(st->pos[0]);
	e->y = NetRep_UPos(st->pos[1]);
	e->z = NetRep_UPos(st->pos[2]);
	RepHist *h = &s->hist[slot][s->snap % NETREP_HIST];
	h->snap = s->snap;
	h->st = *st;
	NetRepGrid_Add(&s->grid, slot, e->x, e->z);
	if (slot >= s->maxSlot) {
		s->maxSlot = slot + 1;
	}
	s->live++;
}

void NetRepSrv_End(NetRepServer *s) {
	(void)s;
}

int NetRepSrv_Live(const NetRepServer *s) {
	return s->live;
}

NetRepClient *NetRepSrv_AddClient(NetRepServer *s, uint32_t rate) {
	if (s->numClients >= REP_MAX_CLIENTS) {
		return NULL;
	}
	NetRepClient *c = calloc(1, sizeof(*c));
	if (!c) {
		return NULL;
	}
	c->rate = rate < NETREP_RATE_MIN ? NETREP_RATE_MIN : rate > NETREP_RATE_MAX ? NETREP_RATE_MAX : rate;
	c->radius = NETREP_RADIUS;
	c->index = s->numClients;
	s->clients[s->numClients++] = c;
	return c;
}

void NetRepSrv_RemoveClient(NetRepServer *s, NetRepClient *c) {
	if (!c) {
		return;
	}
	int i = c->index;
	s->clients[i] = s->clients[--s->numClients];
	s->clients[i]->index = i;
	free(c);
}

void NetRepSrv_SetView(NetRepClient *c, float x, float y, float z, float radius) {
	c->view[0] = x;
	c->view[1] = y;
	c->view[2] = z;
	if (radius > 0.0f) {
		c->radius = radius;
	}
}

static const NetRepState *Rep_Hist(const NetRepServer *s, int slot, uint32_t snap) {
	const RepHist *h = &s->hist[slot][snap % NETREP_HIST];
	return h->snap == snap ? &h->st : NULL;
}

static void Rep_AckOne(NetRepServer *s, NetRepClient *c, uint16_t seq) {
	RepSent *m = &c->sent[seq % NETREP_SENT];
	if (!m->used || m->acked || m->seq != seq) {
		return;
	}
	m->acked = 1;
	c->st.acked++;
	const RepEntry *e = c->ent[seq % NETREP_SENT];
	uint32_t snap = m->snap;
	for (int i = 0; i < m->n; i++) {
		int slot = e[i].slot;
		int op = e[i].op & 3;
		if (op == NETREP_OP_REMOVE) {
			if (snap > c->removeAck[slot]) {
				c->removeAck[slot] = snap;
			}
			continue;
		}
		if (e[i].op & NETREP_REC_LOOK) {
			int look = s->slot[slot].look;
			c->lookKnown[look >> 3] |= (uint8_t)(1 << (look & 7));
		}
		if (snap <= c->removeSent[slot] || snap <= c->baseSnap[slot]) {
			continue;
		}
		const NetRepState *st = Rep_Hist(s, slot, snap);
		if (st) {
			c->base[slot] = *st;
			c->baseSnap[slot] = snap;
		}
	}
}

void NetRepSrv_Ack(NetRepServer *s, NetRepClient *c, uint16_t newest, uint32_t bits) {
	Rep_AckOne(s, c, newest);
	for (int i = 0; i < 32; i++) {
		if ((bits >> i) & 1) {
			Rep_AckOne(s, c, (uint16_t)(newest - 1 - i));
		}
	}
}

/* the record of a candidate into w */
static void Rep_WriteRecord(NetRepServer *s, NetRepClient *c, NetRepBuf *w, const NetRepCand *k, int withLook) {
	int slot = k->slot;
	const NetRepState *cur = Rep_Hist(s, slot, s->snap);
	static const NetRepState zero;

	NetRep_PutVar(w, (uint32_t)slot);
	switch (k->op) {
	case NETREP_OP_REMOVE:
		NetRep_PutU8(w, NETREP_OP_REMOVE);
		return;
	case NETREP_OP_DELTA: {
		const NetRepState *base = &c->base[slot];
		NetRep_PutU8(w, NETREP_OP_DELTA);
		NetRep_PutVar(w, s->snap - c->baseSnap[slot]);
		uint32_t mask = NetRep_Diff(base, cur);
		NetRep_PutVar(w, mask);
		NetRep_WriteFields(w, base, cur, mask);
		return;
	}
	case NETREP_OP_FULL:
		NetRep_PutU8(w, NETREP_OP_FULL);
		break;
	default: {
		int look = s->slot[slot].look;
		NetRep_PutU8(w, NETREP_OP_CREATE | (withLook ? NETREP_REC_LOOK : 0));
		NetRep_PutU8(w, s->slot[slot].kind);
		NetRep_PutVar(w, (uint32_t)look);
		if (withLook) {
			NetRep_PutStr(w, s->looks[look]);
		}
		break;
	}
	}
	uint32_t mask = NetRep_Diff(&zero, cur);
	NetRep_PutVar(w, mask);
	NetRep_WriteFields(w, &zero, cur, mask);
}

static void Rep_MsgStart(NetRepServer *s, NetRepClient *c, NetRepBuf *w, uint8_t *buf) {
	NetRepBuf_Init(w, buf, NETREP_MSG_MAX);
	NetRep_PutU8(w, NETREP_MSG_SNAP);
	NetRep_PutU16(w, c->seq);
	NetRep_PutU32(w, s->snap);
	NetRep_PutU32(w, s->tick);
	NetRep_PutU16(w, 0);			/* the count, written at the end */
	RepSent *m = &c->sent[c->seq % NETREP_SENT];
	m->seq = c->seq;
	m->used = 1;
	m->acked = 0;
	m->snap = s->snap;
	m->n = 0;
}

static void Rep_MsgEnd(NetRepClient *c, NetRepBuf *w) {
	RepSent *m = &c->sent[c->seq % NETREP_SENT];
	w->p[NETREP_MSG_HEADER - 2] = (uint8_t)m->n;
	w->p[NETREP_MSG_HEADER - 1] = (uint8_t)(m->n >> 8);
	c->seq++;
}

int NetRepSrv_Build(NetRepServer *s, NetRepClient *c, uint8_t msgs[][NETREP_MSG_MAX], int *sizes, int max) {
	int64_t per = (int64_t)c->rate / NETREP_SNAP_HZ;
	c->tokens += per;
	if (c->tokens > 2 * per) {
		c->tokens = 2 * per;
	}
	if (c->tokens <= 0 || max <= 0) {
		return 0;
	}
	uint32_t snap = s->snap;
	float keep = c->radius * NETREP_KEEP;
	int nc = 0;

	/* the interest: creates and deltas */
	int nq = NetRepGrid_Query(&s->grid, c->view[0], c->view[2], keep, s->query, NETREP_MAX_ENTS);
	int inInterest = 0;
	for (int i = 0; i < nq; i++) {
		int slot = s->query[i];
		const RepSlot *e = &s->slot[slot];
		if (e->liveSnap != snap) {
			continue;
		}
		float dx = e->x - c->view[0], dy = e->y - c->view[1], dz = e->z - c->view[2];
		float d = sqrtf(dx * dx + dy * dy + dz * dz);
		if (d > (Rep_Exists(c, slot) ? keep : c->radius)) {
			continue;
		}
		c->mark[slot] = snap;
		inInterest++;
		const NetRepState *cur = Rep_Hist(s, slot, snap);
		int known = c->baseSnap[slot] > c->removeSent[slot];
		NetRepCand *k = &s->cand[nc];
		k->slot = (uint16_t)slot;
		if (!known) {
			k->op = NETREP_OP_CREATE;
			k->prio = NetRep_Priority(d, c->radius, 0.0f, NETREP_OP_CREATE);
		} else {
			if (!NetRep_Diff(&c->base[slot], cur)) {
				c->accum[slot] = 0.0f;
				continue;
			}
			float ex = NetRep_UPos(cur->pos[0] - c->base[slot].pos[0]);
			float ey = NetRep_UPos(cur->pos[1] - c->base[slot].pos[1]);
			float ez = NetRep_UPos(cur->pos[2] - c->base[slot].pos[2]);
			k->op = snap - c->baseSnap[slot] >= NETREP_FULL_AGE ? NETREP_OP_FULL : NETREP_OP_DELTA;
			k->prio = NetRep_Priority(d, c->radius, sqrtf(ex * ex + ey * ey + ez * ez), k->op);
		}
		k->prio += c->accum[slot];
		nc++;
	}
	/* the removes: held by the client and out of the interest or gone */
	for (int slot = 0; slot < s->maxSlot; slot++) {
		if (c->mark[slot] == snap || !Rep_Exists(c, slot)) {
			continue;
		}
		NetRepCand *k = &s->cand[nc++];
		k->slot = (uint16_t)slot;
		k->op = NETREP_OP_REMOVE;
		k->prio = NetRep_Priority(0.0f, c->radius, 0.0f, NETREP_OP_REMOVE) + c->accum[slot];
	}
	c->st.interest = (uint32_t)inInterest;
	NetRep_SortCands(s->cand, nc);

	/* the records by priority into messages until the budget or the messages run out */
	uint8_t rec[REP_REC_MAX];
	NetRepBuf w, r;
	int nm = 0, records = 0, open = 0;
	int64_t used = 0;
	int i = 0;
	for (; i < nc; i++) {
		const NetRepCand *k = &s->cand[i];
		int look = s->slot[k->slot].look;
		int needLook = k->op == NETREP_OP_CREATE && !(c->lookKnown[look >> 3] & (1 << (look & 7)));
		/* the string once per message: the client reads the records of a message in order */
		int withLook = needLook && !(open && c->lookMsg[look] == nm + 1);
		NetRepBuf_Init(&r, rec, sizeof(rec));
		Rep_WriteRecord(s, c, &r, k, withLook);
		if (r.bad) {
			continue;
		}
		RepSent *m = open ? &c->sent[c->seq % NETREP_SENT] : NULL;
		if (open && (w.size + r.size > NETREP_MSG_MAX || m->n >= REP_MSG_RECS)) {
			Rep_MsgEnd(c, &w);
			sizes[nm++] = w.size;
			open = 0;
		}
		if (!open) {
			if (needLook && !withLook) {
				withLook = 1;
				NetRepBuf_Init(&r, rec, sizeof(rec));
				Rep_WriteRecord(s, c, &r, k, 1);
			}
			int64_t need = NETREP_MSG_OVERHEAD + NETREP_MSG_HEADER + r.size;
			if (nm >= max || (records > 0 && used + need > c->tokens)) {
				break;
			}
			Rep_MsgStart(s, c, &w, msgs[nm]);
			used += NETREP_MSG_OVERHEAD + NETREP_MSG_HEADER;
			open = 1;
			m = &c->sent[c->seq % NETREP_SENT];
		} else if (records > 0 && used + r.size > c->tokens) {
			break;
		}
		if (withLook) {
			c->lookMsg[look] = (uint16_t)(nm + 1);
		}
		memcpy(w.p + w.size, rec, (size_t)r.size);
		w.size += r.size;
		used += r.size;
		RepEntry *e = &c->ent[c->seq % NETREP_SENT][m->n++];
		e->slot = k->slot;
		e->op = (uint8_t)(k->op | (withLook ? NETREP_REC_LOOK : 0));
		records++;
		c->accum[k->slot] = 0.0f;
		switch (k->op) {
		case NETREP_OP_REMOVE:
			c->removeSent[k->slot] = snap;
			break;
		case NETREP_OP_CREATE:
			c->st.creates++;
			c->lastSent[k->slot] = snap;
			break;
		case NETREP_OP_FULL:
			c->st.fulls++;
			c->lastSent[k->slot] = snap;
			break;
		default:
			c->lastSent[k->slot] = snap;
			break;
		}
		if (k->op == NETREP_OP_REMOVE) {
			c->st.removes++;
		}
	}
	if (open) {
		Rep_MsgEnd(c, &w);
		sizes[nm++] = w.size;
	}
	/* what did not fit waits with its priority */
	for (; i < nc; i++) {
		c->accum[s->cand[i].slot] = s->cand[i].prio;
		c->st.skipped++;
	}
	for (int l = 0; l < s->numLooks; l++) {
		c->lookMsg[l] = 0;
	}
	c->tokens -= used;
	c->st.snaps++;
	c->st.msgs += (uint32_t)nm;
	c->st.records += (uint32_t)records;
	c->st.bytes += (uint64_t)used;
	if ((uint32_t)used > c->st.maxSnapBytes) {
		c->st.maxSnapBytes = (uint32_t)used;
	}
	if ((uint32_t)records > c->st.maxRecords) {
		c->st.maxRecords = (uint32_t)records;
	}
	return nm;
}

const NetRepClientStats *NetRepSrv_Stats(const NetRepClient *c) {
	return &c->st;
}

const NetRepState *NetRepSrv_Base(const NetRepClient *c, int slot) {
	if (slot < 0 || slot >= NETREP_MAX_ENTS || c->baseSnap[slot] <= c->removeSent[slot]) {
		return NULL;
	}
	return &c->base[slot];
}
