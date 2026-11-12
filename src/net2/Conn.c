/*
 * Net2 connection after the handshake: DATA packets in and out, acks, round trip time and loss, the rate budget,
 * time outs and the disconnect.
 *
 * Every packet acks the newest packet received and the 32 before it. A sent packet is remembered in a ring of
 * N2_SENT_RING with the reliable fragments it carried; an ack marks them, a packet that leaves the ack window
 * without an ack counts as lost. A fragment is sent again when it is not acked after the retransmit time (RFC 6298
 * estimate plus the peer's ack delay), whatever happened to the packet. Packets with chunks want an ack within
 * N2_ACK_DELAY; packets with only acks want none, so two idle peers do not ack each other's acks forever.
 */
#include "net2local.h"

int Net2Conn_Init(Net2Host *h, Net2Conn *c) {
	c->sent = Net2_Alloc(h, sizeof(Net2SentPkt) * N2_SENT_RING);
	c->inbox = Net2_Alloc(h, h->cfg.inboxSize);
	c->outbox = Net2_Alloc(h, h->cfg.outboxSize);
	if (!c->sent || !c->inbox || !c->outbox) {
		return 0;
	}
	for (int i = 0; i < h->cfg.numChannels; i++) {
		Net2Channel *ch = &c->ch[i];
		ch->kind = h->cfg.channels[i];
		if (ch->kind == NET2_RELIABLE) {
			ch->rs = Net2_Alloc(h, sizeof(Net2RelSend));
			ch->rr = Net2_Alloc(h, sizeof(Net2RelRecv));
			if (!ch->rs || !ch->rr) {
				return 0;
			}
		}
	}
	c->rate = h->cfg.rate;
	c->tokens = (int64_t)(4 * NET2_MTU);
	c->tokenTime = h->now;
	c->startTime = h->now;
	c->lastRecv = h->now;
	c->lastSend = h->now;
	c->rateTime = h->now;
	return 1;
}

void Net2Conn_Release(Net2Host *h, Net2Conn *c) {
	int index = c->index;

	Net2Rel_FreeAll(h, c);
	Net2_Free(h, c->sent);
	Net2_Free(h, c->inbox);
	Net2_Free(h, c->outbox);
	Net2Crypto_Wipe(c, sizeof(*c));
	c->index = index;
	c->state = N2C_FREE;
}

static uint32_t Net2Conn_Rto(const Net2Conn *c) {
	if (!c->haveRtt) {
		return N2_RTO_INIT;
	}
	float r = c->srtt + 4.0f * c->rttVar + (float)N2_ACK_DELAY;
	if (r < (float)N2_RTO_MIN) {
		return N2_RTO_MIN;
	}
	if (r > (float)N2_RTO_MAX) {
		return N2_RTO_MAX;
	}
	return (uint32_t)r;
}

static void Net2Conn_Rtt(Net2Conn *c, float sample) {
	if (!c->haveRtt) {
		c->srtt = sample;
		c->rttVar = sample * 0.5f;
		c->haveRtt = 1;
		return;
	}
	float d = c->srtt - sample;
	c->rttVar = 0.75f * c->rttVar + 0.25f * (d < 0 ? -d : d);
	c->srtt = 0.875f * c->srtt + 0.125f * sample;
}

static void Net2Conn_Acks(Net2Host *h, Net2Conn *c, const Net2Acks *a) {
	if (a->ack == 0 || a->ack > c->sendSeq) {
		return;
	}
	for (uint32_t i = 0; i <= 32 && i < a->ack; i++) {
		uint32_t seq = a->ack - i;
		if (i > 0 && !((a->bits >> (i - 1)) & 1)) {
			continue;
		}
		Net2SentPkt *e = &c->sent[seq % N2_SENT_RING];
		if (e->seq != seq || e->acked) {
			continue;
		}
		e->acked = 1;
		if (i == 0) {
			int64_t s = (int64_t)(h->now - e->time) - (int64_t)a->delay;
			Net2Conn_Rtt(c, s < 0 ? 0.0f : (float)s);
		}
		for (int j = 0; j < e->numRefs; j++) {
			Net2Rel_Acked(h, c, e->refs[j].channel, e->refs[j].id, e->refs[j].frag);
		}
	}
	if (a->ack <= c->ackTop) {
		return;
	}
	/* the packets that left the ack window [ack - 32, ack] with this ack are acked or lost now */
	uint32_t lo = c->ackTop > 32 ? c->ackTop - 32 : 1;
	uint32_t hi = a->ack > 33 ? a->ack - 33 : 0;
	if (hi >= lo && hi - lo >= N2_SENT_RING) {
		lo = hi - N2_SENT_RING + 1;
	}
	for (uint32_t seq = lo; seq <= hi && seq != 0; seq++) {
		const Net2SentPkt *e = &c->sent[seq % N2_SENT_RING];
		if (e->seq != seq) {
			continue;
		}
		/* the loss of the last 256 packets */
		uint64_t bit = (uint64_t)1 << (c->lossPos & 63);
		uint64_t *word = &c->lossBits[(c->lossPos >> 6) & 3];
		if (c->lossCount == 256 && (*word & bit)) {
			c->lossLost--;
		}
		*word &= ~bit;
		if (!e->acked) {
			*word |= bit;
			c->lossLost++;
			c->packetsLost++;
		}
		c->lossPos = (c->lossPos + 1) & 255;
		if (c->lossCount < 256) {
			c->lossCount++;
		}
	}
	c->ackTop = a->ack;
}

static void Net2Conn_Inbox(Net2Conn *c, uint32_t cap, int channel, const uint8_t *data, int size) {
	uint32_t need = 3 + (uint32_t)size;

	if (c->inboxWrite + need > cap) {
		c->dropped++;
		return;
	}
	uint8_t *p = c->inbox + c->inboxWrite;
	p[0] = (uint8_t)channel;
	p[1] = (uint8_t)size;
	p[2] = (uint8_t)(size >> 8);
	if (size) {
		memcpy(p + 3, data, (size_t)size);
	}
	c->inboxWrite += need;
}

void Net2Conn_Close(Net2Host *h, Net2Conn *c, int reason, int notify) {
	if (c->state == N2C_FREE || c->state == N2C_CLOSED || c->state == N2C_CLOSING) {
		return;
	}
	if (c->state == N2C_ACCEPTED) {
		Net2Conn_Release(h, c);		/* the game never saw it */
		return;
	}
	c->reason = reason;
	c->evDisconnect = 1;
	if (notify && c->state == N2C_CONNECTED) {
		c->state = N2C_CLOSING;
		c->closeSends = N2_CLOSE_SENDS;
	} else {
		c->state = N2C_CLOSED;
	}
}

void Net2Conn_Data(Net2Host *h, Net2Conn *c, const Net2Addr *from, const uint8_t *pkt, int size) {
	uint8_t plain[N2_PLAIN_MAX];
	Net2Reader r;
	Net2Acks acks;
	Net2Chunk ck;

	int n = size - N2_HEADER - N2_TAG;
	if (n < N2_ACKS || n > N2_PLAIN_MAX) {
		c->bad++;
		return;
	}
	uint32_t seq = (uint32_t)pkt[5] | (uint32_t)pkt[6] << 8 | (uint32_t)pkt[7] << 16 | (uint32_t)pkt[8] << 24;
	if (!Net2Win_Check(&c->recv, seq)) {
		c->bad++;
		return;
	}
	if (!Net2Crypto_Open(plain, c->recvKey, !h->cfg.server, seq, pkt, pkt + N2_HEADER, n)) {
		c->bad++;
		return;
	}
	int newest = seq > c->recv.top;
	Net2Win_Mark(&c->recv, seq);
	if (newest) {
		c->recvTopTime = h->now;
		if (h->cfg.server && !Net2_AddrEqual(from, &c->addr)) {
			c->addr = *from;		/* the client's NAT mapping changed; only the newest packet moves it */
		}
	}
	c->lastRecv = h->now;
	c->packetsRecv++;
	c->bytesRecv += (uint64_t)size;
	c->secRecv += (uint32_t)size;
	if (c->state == N2C_ACCEPTED) {
		c->state = N2C_CONNECTED;
		c->evConnect = 1;
	}
	if (c->state != N2C_CONNECTED && c->state != N2C_CLOSING) {
		return;
	}
	if (!Net2Pkt_CheckChunks(plain + N2_ACKS, n - N2_ACKS, h->cfg.numChannels, h->cfg.channels)) {
		c->bad++;
		Net2Conn_Close(h, c, NET2_DR_PROTOCOL, 1);
		return;
	}
	Net2R_Init(&r, plain, n);
	Net2Pkt_ReadAcks(&r, &acks);
	Net2Conn_Acks(h, c, &acks);
	if (c->state == N2C_CLOSING || Net2R_Left(&r) == 0) {
		return;
	}
	if (c->recvUnacked++ == 0) {
		c->firstUnacked = h->now;
	}
	while (Net2Pkt_ReadChunk(&r, &ck) > 0) {
		Net2Channel *ch = &c->ch[ck.channel];
		switch (ck.type) {
		case N2K_UNRELIABLE:
			Net2Conn_Inbox(c, h->cfg.inboxSize, ck.channel, ck.data, ck.size);
			break;
		case N2K_SEQUENCED:
			if (!ch->haveRecvSeq || Net2_IdDiff(ck.id, ch->recvSeq) > 0) {
				ch->recvSeq = ck.id;
				ch->haveRecvSeq = 1;
				Net2Conn_Inbox(c, h->cfg.inboxSize, ck.channel, ck.data, ck.size);
			}
			break;
		case N2K_RELIABLE:
		case N2K_FRAGMENT:
			Net2Rel_Recv(h, c, &ck);
			break;
		case N2K_DISCONNECT:
			Net2Conn_Close(h, c, ck.reason, 0);
			return;
		}
	}
}

static void Net2Conn_SendPlain(Net2Host *h, Net2Conn *c, const uint8_t *plain, int n, const Net2RelRef *refs,
	int numRefs) {
	uint8_t pkt[NET2_MTU];

	if (c->sendSeq == UINT32_MAX) {
		Net2Conn_Close(h, c, NET2_DR_SEQUENCE, 0);
		return;
	}
	uint32_t seq = ++c->sendSeq;
	Net2Pkt_WriteHeader(pkt, c->id, seq);
	Net2Crypto_Seal(pkt + N2_HEADER, c->sendKey, h->cfg.server, seq, pkt, plain, n);
	int size = N2_HEADER + n + N2_TAG;
	Net2_RawSend(h, &c->addr, pkt, size);

	Net2SentPkt *e = &c->sent[seq % N2_SENT_RING];
	e->seq = seq;
	e->time = h->now;
	e->size = (uint16_t)size;
	e->acked = 0;
	e->numRefs = (uint8_t)numRefs;
	if (numRefs) {
		memcpy(e->refs, refs, sizeof(*refs) * (size_t)numRefs);
	}
	c->lastSend = h->now;
	c->recvUnacked = 0;
	c->forceSend = 0;
	c->tokens -= size;
	c->packetsSent++;
	c->bytesSent += (uint64_t)size;
	c->secSent += (uint32_t)size;
}

static void Net2Conn_StartPlain(Net2Host *h, Net2Conn *c, Net2Writer *w, uint8_t *plain) {
	Net2Acks a;

	a.ack = c->recv.top;
	a.bits = Net2Win_AckBits(&c->recv);
	a.delay = c->recv.top ? (uint32_t)(h->now - c->recvTopTime) : 0;
	Net2W_Init(w, plain, N2_PLAIN_MAX);
	Net2Pkt_WriteAcks(w, &a);
}

/* reliable fragments that are due, oldest message first, until the packet or its reference list is full */
static int Net2Conn_AddReliable(Net2Host *h, Net2Conn *c, Net2Writer *w, Net2RelRef *refs, int *numRefs) {
	uint32_t rto = Net2Conn_Rto(c);
	int added = 0;

	for (int i = 0; i < h->cfg.numChannels; i++) {
		Net2RelSend *rs = c->ch[i].rs;
		if (!rs) {
			continue;
		}
		for (uint16_t id = rs->base; id != rs->next; id++) {
			Net2RelMsg *m = rs->slot[id % N2_WINDOW];
			if (!m) {
				continue;
			}
			const uint8_t *data = (const uint8_t *)(m->sent + m->count);
			for (int f = 0; f < m->count; f++) {
				if ((m->acked >> f) & 1) {
					continue;
				}
				/* sent[] holds the send time + 1, so 0 means never sent */
				if (m->sent[f] && h->now + 1 - m->sent[f] < rto) {
					continue;
				}
				if (*numRefs == N2_PKT_REFS) {
					return added;
				}
				Net2Chunk ck;
				memset(&ck, 0, sizeof(ck));
				ck.type = m->count == 1 ? N2K_RELIABLE : N2K_FRAGMENT;
				ck.channel = i;
				ck.id = m->id;
				ck.index = f;
				ck.count = m->count;
				ck.data = data + (size_t)f * NET2_FRAG_SIZE;
				ck.size = f == m->count - 1 ? (int)m->size - f * NET2_FRAG_SIZE : NET2_FRAG_SIZE;
				if (!Net2Pkt_WriteChunk(w, &ck)) {
					return added;
				}
				m->sent[f] = h->now + 1;
				refs[*numRefs].id = m->id;
				refs[*numRefs].channel = (uint8_t)i;
				refs[*numRefs].frag = (uint8_t)f;
				(*numRefs)++;
				added++;
			}
		}
	}
	return added;
}

/* unreliable and sequenced messages of the outbox from *pos on, while they fit */
static int Net2Conn_AddOutbox(Net2Host *h, Net2Conn *c, Net2Writer *w, uint32_t *pos) {
	int added = 0;

	while (*pos < c->outboxUsed) {
		const uint8_t *p = c->outbox + *pos;
		Net2Chunk ck;
		memset(&ck, 0, sizeof(ck));
		ck.channel = p[0];
		ck.id = (uint16_t)(p[1] | p[2] << 8);
		ck.size = p[3] | p[4] << 8;
		ck.data = p + 5;
		ck.type = h->cfg.channels[ck.channel] == NET2_SEQUENCED ? N2K_SEQUENCED : N2K_UNRELIABLE;
		if (!Net2Pkt_WriteChunk(w, &ck)) {
			break;
		}
		*pos += 5 + (uint32_t)ck.size;
		added++;
	}
	return added;
}

void Net2Conn_Flush(Net2Host *h, Net2Conn *c) {
	uint8_t plain[N2_PLAIN_MAX];
	Net2RelRef refs[N2_PKT_REFS];
	Net2Writer w;

	if (c->state == N2C_CLOSING) {
		while (c->closeSends > 0 && c->state == N2C_CLOSING) {
			Net2Chunk ck;
			memset(&ck, 0, sizeof(ck));
			ck.type = N2K_DISCONNECT;
			ck.reason = c->reason;
			Net2Conn_StartPlain(h, c, &w, plain);
			Net2Pkt_WriteChunk(&w, &ck);
			Net2Conn_SendPlain(h, c, plain, w.pos, NULL, 0);
			c->closeSends--;
		}
		c->state = N2C_CLOSED;
		return;
	}
	if (c->state != N2C_CONNECTED) {
		return;
	}
	uint64_t dt = h->now - c->tokenTime;
	int64_t burst = (int64_t)c->rate / 4;
	if (burst < 4 * NET2_MTU) {
		burst = 4 * NET2_MTU;
	}
	c->tokens += (int64_t)((uint64_t)c->rate * dt / 1000);
	if (c->tokens > burst) {
		c->tokens = burst;
	}
	c->tokenTime = h->now;

	uint32_t pos = 0;
	for (int n = 0; n < N2_FLUSH_PACKETS && c->state == N2C_CONNECTED; n++) {
		int wantAck = c->recvUnacked > 0
			&& (h->now - c->firstUnacked >= N2_ACK_DELAY || c->recvUnacked >= N2_ACK_COUNT);
		int keep = h->now - c->lastSend >= N2_KEEPALIVE || c->forceSend;
		int numRefs = 0, added = 0;

		Net2Conn_StartPlain(h, c, &w, plain);
		/* input and snapshots first: they are stale next frame, a reliable fragment can wait for the budget */
		if (c->tokens > 0) {
			added += Net2Conn_AddOutbox(h, c, &w, &pos);
			added += Net2Conn_AddReliable(h, c, &w, refs, &numRefs);
		}
		if (!added && !wantAck && !keep) {
			break;
		}
		Net2Conn_SendPlain(h, c, plain, w.pos, refs, numRefs);
	}
	/* what did not go out this frame is stale */
	while (pos < c->outboxUsed) {
		pos += 5 + (uint32_t)(c->outbox[pos + 3] | c->outbox[pos + 4] << 8);
		c->dropped++;
	}
	c->outboxUsed = 0;
}

/* time out, rates; 0 when the connection was closed */
int Net2Conn_Tick(Net2Host *h, Net2Conn *c) {
	if (h->now - c->rateTime >= 1000) {
		uint64_t dt = h->now - c->rateTime;
		c->sendRate = (uint32_t)((uint64_t)c->secSent * 1000 / dt);
		c->recvRate = (uint32_t)((uint64_t)c->secRecv * 1000 / dt);
		c->secSent = 0;
		c->secRecv = 0;
		c->rateTime = h->now;
	}
	uint32_t limit = c->state == N2C_ACCEPTED ? h->cfg.connectTimeout : h->cfg.timeout;
	if ((c->state == N2C_ACCEPTED || c->state == N2C_CONNECTED) && h->now - c->lastRecv > limit) {
		Net2Conn_Close(h, c, NET2_DR_TIMEOUT, 0);
		return 0;
	}
	return 1;
}

void Net2Conn_Stats(const Net2Conn *c, Net2Stats *st) {
	memset(st, 0, sizeof(*st));
	st->rtt = (uint32_t)(c->srtt + 0.5f);
	st->rttVar = (uint32_t)(c->rttVar + 0.5f);
	st->loss = c->lossCount ? (float)c->lossLost / (float)c->lossCount : 0.0f;
	st->sendRate = c->sendRate;
	st->recvRate = c->recvRate;
	st->packetsSent = c->packetsSent;
	st->packetsRecv = c->packetsRecv;
	st->packetsLost = c->packetsLost;
	st->bytesSent = c->bytesSent;
	st->bytesRecv = c->bytesRecv;
	for (int i = 0; i < NET2_MAX_CHANNELS; i++) {
		if (c->ch[i].rs) {
			st->reliablePending += (uint16_t)(c->ch[i].rs->next - c->ch[i].rs->base);
		}
	}
	st->dropped = c->dropped;
	st->bad = c->bad;
}
