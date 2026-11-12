/*
 * Net2 packet layer without state: the window of received packet numbers (replay check and acks), the ack block
 * and the chunks of a DATA packet (formats in net2local.h). Every size read from the wire is checked here.
 */
#include "net2local.h"

/* the sizes of net2.h and net2local.h fit each other */
_Static_assert(NET2_MAX_UNRELIABLE == NET2_MTU - N2_OVERHEAD - 6, "the largest sequenced chunk fills a packet");
_Static_assert(NET2_FRAG_SIZE + 8 <= NET2_MTU - N2_OVERHEAD, "a fragment chunk fits a packet");
_Static_assert(NET2_MAX_FRAGS <= 64, "one bit per fragment in a uint64_t");
_Static_assert((NET2_MAX_CONNS & (NET2_MAX_CONNS - 1)) == 0, "connection index in the low bits of the id");
_Static_assert(N2_ACCEPT_SIZE == 1 + N2_SALT + 4 + 32 + 32 + 64, "ACCEPT layout");
_Static_assert(N2_CHALLENGE_SIZE == 1 + N2_SALT + N2_COOKIE, "CHALLENGE layout");
_Static_assert(1 + 12 + 2 + 4 + 3 + 3 + NET2_INFO_NAME + NET2_INFO_MAP + NET2_INFO_MODE <= N2_QUERY_SIZE,
	"no INFO is larger than its QUERY");
_Static_assert(sizeof(Net2Addr) == 24, "Net2Addr has no padding");

int Net2Win_Check(const Net2Window *w, uint32_t seq) {
	if (seq == 0) {
		return 0;
	}
	if (seq > w->top) {
		return 1;
	}
	uint32_t d = w->top - seq;
	if (d >= 64) {
		return 0;
	}
	return !((w->bits >> d) & 1);
}

void Net2Win_Mark(Net2Window *w, uint32_t seq) {
	if (seq > w->top) {
		uint32_t d = seq - w->top;
		w->bits = d >= 64 ? 0 : w->bits << d;
		w->bits |= 1;
		w->top = seq;
	} else if (w->top - seq < 64) {
		w->bits |= (uint64_t)1 << (w->top - seq);
	}
}

uint32_t Net2Win_AckBits(const Net2Window *w) {
	return (uint32_t)(w->bits >> 1);
}

void Net2Pkt_WriteAcks(Net2Writer *w, const Net2Acks *a) {
	Net2W_U32(w, a->ack);
	Net2W_U32(w, a->bits);
	Net2W_U16(w, a->delay > 0xffff ? 0xffff : a->delay);
}

int Net2Pkt_ReadAcks(Net2Reader *r, Net2Acks *a) {
	a->ack = Net2R_U32(r);
	a->bits = Net2R_U32(r);
	a->delay = Net2R_U16(r);
	return !r->bad;
}

void Net2Pkt_WriteHeader(uint8_t *hdr, uint32_t connId, uint32_t seq) {
	Net2Writer w;
	Net2W_Init(&w, hdr, N2_HEADER);
	Net2W_U8(&w, N2P_DATA);
	Net2W_U32(&w, connId);
	Net2W_U32(&w, seq);
}

int Net2Pkt_ChunkSize(const Net2Chunk *c) {
	switch (c->type) {
	case N2K_UNRELIABLE:
		return 4 + c->size;
	case N2K_SEQUENCED:
	case N2K_RELIABLE:
		return 6 + c->size;
	case N2K_FRAGMENT:
		return 8 + c->size;
	case N2K_DISCONNECT:
		return 2;
	}
	return 0;
}

int Net2Pkt_WriteChunk(Net2Writer *w, const Net2Chunk *c) {
	int n = Net2Pkt_ChunkSize(c);
	if (w->overflow || n <= 0 || n > w->size - w->pos || c->size < 0 || c->size > 0xffff) {
		return 0;
	}
	Net2W_U8(w, (uint32_t)c->type);
	if (c->type == N2K_DISCONNECT) {
		Net2W_U8(w, (uint32_t)c->reason);
		return 1;
	}
	Net2W_U8(w, (uint32_t)c->channel);
	if (c->type != N2K_UNRELIABLE) {
		Net2W_U16(w, c->id);
	}
	if (c->type == N2K_FRAGMENT) {
		Net2W_U8(w, (uint32_t)c->index);
		Net2W_U8(w, (uint32_t)c->count);
	}
	Net2W_U16(w, (uint32_t)c->size);
	Net2W_Bytes(w, c->data, c->size);
	return !w->overflow;
}

int Net2Pkt_ReadChunk(Net2Reader *r, Net2Chunk *c) {
	memset(c, 0, sizeof(*c));
	if (Net2R_Left(r) == 0) {
		return r->bad ? -1 : 0;
	}
	c->type = (int)Net2R_U8(r);
	if (c->type == N2K_DISCONNECT) {
		c->reason = (int)Net2R_U8(r);
		return r->bad ? -1 : 1;
	}
	if (c->type < N2K_UNRELIABLE || c->type > N2K_FRAGMENT) {
		return -1;
	}
	c->channel = (int)Net2R_U8(r);
	if (c->type != N2K_UNRELIABLE) {
		c->id = (uint16_t)Net2R_U16(r);
	}
	if (c->type == N2K_FRAGMENT) {
		c->index = (int)Net2R_U8(r);
		c->count = (int)Net2R_U8(r);
	} else {
		c->count = 1;
	}
	c->size = (int)Net2R_U16(r);
	c->data = Net2R_Bytes(r, c->size);
	if (r->bad) {
		return -1;
	}
	if (c->type == N2K_FRAGMENT) {
		if (c->count < 2 || c->count > NET2_MAX_FRAGS || c->index >= c->count) {
			return -1;
		}
		if (c->index < c->count - 1 ? c->size != NET2_FRAG_SIZE : c->size < 1 || c->size > NET2_FRAG_SIZE) {
			return -1;
		}
	} else if (c->type == N2K_RELIABLE ? c->size > NET2_FRAG_SIZE : c->size > NET2_MAX_UNRELIABLE) {
		return -1;
	}
	return 1;
}

/* the whole plain text after the acks: every chunk well formed and on a channel of its kind */
int Net2Pkt_CheckChunks(const uint8_t *p, int size, int numChannels, const uint8_t *kinds) {
	Net2Reader r;
	Net2Chunk c;
	int n;

	Net2R_Init(&r, p, size);
	while ((n = Net2Pkt_ReadChunk(&r, &c)) > 0) {
		if (c.type == N2K_DISCONNECT) {
			continue;
		}
		if (c.channel >= numChannels) {
			return 0;
		}
		int kind = kinds[c.channel];
		if ((c.type == N2K_UNRELIABLE && kind != NET2_UNRELIABLE)
			|| (c.type == N2K_SEQUENCED && kind != NET2_SEQUENCED)
			|| ((c.type == N2K_RELIABLE || c.type == N2K_FRAGMENT) && kind != NET2_RELIABLE)) {
			return 0;
		}
	}
	return n == 0;
}
