/*
 * Net2 reliable ordered channels. Send side: a window of 256 message ids from the oldest one not acked; a message
 * is cut into fragments of NET2_FRAG_SIZE, each fragment is acked by the packet it went in (Conn.c) and sent again
 * after the retransmit time. Receive side: a slot per id of the window, fragments are copied into it, and the
 * messages leave in id order through Net2Rel_Take. Receive memory is capped by maxRecvBuffer, except for the next
 * id in order, which always gets its slot so the channel cannot jam.
 */
#include "net2local.h"

static uint64_t Net2Rel_Full(int count) {
	return count >= 64 ? ~(uint64_t)0 : ((uint64_t)1 << count) - 1;
}

int Net2Rel_Queue(Net2Host *h, Net2Conn *c, int channel, const uint8_t *data, int size) {
	Net2RelSend *rs = c->ch[channel].rs;

	if (size < 0 || size > NET2_MAX_MESSAGE || (size > 0 && !data)) {
		return NET2_E_ARG;
	}
	if ((uint16_t)(rs->next - rs->base) >= N2_WINDOW || c->pendingBytes + (uint32_t)size > h->cfg.maxPending) {
		return NET2_E_FULL;
	}
	int count = size <= NET2_FRAG_SIZE ? 1 : (size + NET2_FRAG_SIZE - 1) / NET2_FRAG_SIZE;
	Net2RelMsg *m = Net2_Alloc(h, sizeof(Net2RelMsg) + sizeof(uint64_t) * (size_t)count + (size_t)size);
	if (!m) {
		return NET2_E_NOMEM;
	}
	m->id = rs->next++;
	m->count = (uint8_t)count;
	m->size = (uint32_t)size;
	if (size) {
		memcpy(m->sent + count, data, (size_t)size);
	}
	rs->slot[m->id % N2_WINDOW] = m;
	c->pendingBytes += (uint32_t)size;
	return NET2_OK;
}

void Net2Rel_Acked(Net2Host *h, Net2Conn *c, int channel, uint16_t id, int frag) {
	Net2RelSend *rs = c->ch[channel].rs;
	Net2RelMsg *m;

	if (!rs || Net2_IdDiff(id, rs->base) < 0 || Net2_IdDiff(id, rs->next) >= 0) {
		return;
	}
	m = rs->slot[id % N2_WINDOW];
	if (!m || m->id != id || frag >= m->count) {
		return;
	}
	m->acked |= (uint64_t)1 << frag;
	while (rs->base != rs->next) {
		m = rs->slot[rs->base % N2_WINDOW];
		if (!m || m->acked != Net2Rel_Full(m->count)) {
			break;
		}
		c->pendingBytes -= m->size;
		rs->slot[rs->base % N2_WINDOW] = NULL;
		Net2_Free(h, m);
		rs->base++;
	}
}

int Net2Rel_Recv(Net2Host *h, Net2Conn *c, const Net2Chunk *ck) {
	Net2RelRecv *rr = c->ch[ck->channel].rr;
	int d = Net2_IdDiff(ck->id, rr->expect);
	Net2RelSlot *s;

	if (d < 0 || d >= N2_WINDOW) {
		return 1;					/* delivered before (its ack was lost), or not a window the sender can have */
	}
	s = &rr->slot[ck->id % N2_WINDOW];
	if (s->data) {
		if (s->id != ck->id || s->count != ck->count || (s->have >> ck->index & 1)) {
			return 1;
		}
	} else {
		uint32_t cap = ck->count == 1 ? (uint32_t)ck->size : (uint32_t)ck->count * NET2_FRAG_SIZE;
		if (cap == 0) {
			cap = 1;
		}
		if (d != 0 && c->recvBytes + cap > h->cfg.maxRecvBuffer) {
			return 0;
		}
		s->data = Net2_Alloc(h, cap);
		if (!s->data) {
			return 0;
		}
		s->cap = cap;
		s->id = ck->id;
		s->count = (uint8_t)ck->count;
		s->have = 0;
		s->size = 0;
		c->recvBytes += cap;
	}
	uint32_t at = (uint32_t)ck->index * NET2_FRAG_SIZE;
	if (at + (uint32_t)ck->size > s->cap) {
		return 1;
	}
	if (ck->size) {
		memcpy(s->data + at, ck->data, (size_t)ck->size);
	}
	s->have |= (uint64_t)1 << ck->index;
	if (ck->index == ck->count - 1) {
		s->size = at + (uint32_t)ck->size;
	}
	return 1;
}

int Net2Rel_Ready(Net2Conn *c, int channel) {
	Net2RelRecv *rr = c->ch[channel].rr;
	const Net2RelSlot *s = &rr->slot[rr->expect % N2_WINDOW];

	return s->data && s->id == rr->expect && s->have == Net2Rel_Full(s->count);
}

uint8_t *Net2Rel_Take(Net2Conn *c, int channel, int *size) {
	Net2RelRecv *rr = c->ch[channel].rr;
	Net2RelSlot *s = &rr->slot[rr->expect % N2_WINDOW];
	uint8_t *p = s->data;

	*size = (int)s->size;
	c->recvBytes -= s->cap;
	memset(s, 0, sizeof(*s));
	rr->expect++;
	return p;
}

void Net2Rel_FreeAll(Net2Host *h, Net2Conn *c) {
	for (int i = 0; i < NET2_MAX_CHANNELS; i++) {
		Net2Channel *ch = &c->ch[i];
		for (int j = 0; j < N2_WINDOW; j++) {
			if (ch->rs) {
				Net2_Free(h, ch->rs->slot[j]);
			}
			if (ch->rr) {
				Net2_Free(h, ch->rr->slot[j].data);
			}
		}
		Net2_Free(h, ch->rs);
		Net2_Free(h, ch->rr);
		ch->rs = NULL;
		ch->rr = NULL;
	}
	c->pendingBytes = 0;
	c->recvBytes = 0;
}
