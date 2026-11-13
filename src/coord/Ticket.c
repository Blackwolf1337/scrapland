/*
 * The tickets of the coordinator and the hash of a join (include/scrap/coord.h).
 */
#include <string.h>

#include "scrap/coord.h"
#include "scrap/netrep.h"

static const uint8_t ticket_magic[4] = { 'S', 'T', 'K', '1' };

typedef struct TkBuf {
	uint8_t *p;
	int size, cap, bad;
} TkBuf;

static void Tk_Bytes(TkBuf *w, const void *b, int n) {
	if (w->bad || n < 0 || w->size + n > w->cap) {
		w->bad = 1;
		return;
	}
	memcpy(w->p + w->size, b, (size_t)n);
	w->size += n;
}

static void Tk_U64(TkBuf *w, uint64_t v) {
	uint8_t b[8];
	for (int i = 0; i < 8; i++) {
		b[i] = (uint8_t)(v >> (8 * i));
	}
	Tk_Bytes(w, b, 8);
}

static void Tk_Str(TkBuf *w, const char *s, int max) {
	size_t n = strlen(s);
	if (n > (size_t)max) {
		w->bad = 1;
		return;
	}
	uint8_t l = (uint8_t)n;
	Tk_Bytes(w, &l, 1);
	Tk_Bytes(w, s, (int)n);
}

typedef struct TkRead {
	const uint8_t *p;
	int size, pos, bad;
} TkRead;

static const uint8_t *Tk_Take(TkRead *r, int n) {
	if (r->bad || n < 0 || r->pos + n > r->size) {
		r->bad = 1;
		return NULL;
	}
	const uint8_t *p = r->p + r->pos;
	r->pos += n;
	return p;
}

static uint64_t Tk_GetU64(TkRead *r) {
	const uint8_t *b = Tk_Take(r, 8);
	uint64_t v = 0;
	for (int i = 0; b && i < 8; i++) {
		v |= (uint64_t)b[i] << (8 * i);
	}
	return v;
}

static void Tk_GetStr(TkRead *r, char *out, int max) {
	const uint8_t *l = Tk_Take(r, 1);
	int n = l ? *l : 0;
	const uint8_t *s = n > max ? (r->bad = 1, NULL) : Tk_Take(r, n);
	if (!s) {
		out[0] = 0;
		return;
	}
	memcpy(out, s, (size_t)n);
	out[n] = 0;
	if (memchr(out, 0, (size_t)n)) {
		r->bad = 1;
	}
}

int Coord_TicketSeal(const CoordTicket *t, const uint8_t seed[NET2_SEED_SIZE], uint8_t *out, int cap) {
	TkBuf w = { out, 0, cap, 0 };
	uint8_t len[2] = { (uint8_t)t->stateLen, (uint8_t)(t->stateLen >> 8) };

	if (t->stateLen < 0 || t->stateLen > COORD_STATE_MAX) {
		return 0;
	}
	Tk_Bytes(&w, ticket_magic, 4);
	Tk_Bytes(&w, t->key, NET2_KEY_SIZE);
	Tk_Str(&w, t->level, COORD_LEVEL);
	Tk_Str(&w, t->entrance, COORD_LEVEL);
	Tk_Str(&w, t->door, COORD_LEVEL);
	Tk_U64(&w, t->issued);
	Tk_U64(&w, t->expires);
	Tk_Bytes(&w, t->nonce, COORD_NONCE);
	Tk_Bytes(&w, len, 2);
	Tk_Bytes(&w, t->state, t->stateLen);
	if (w.bad || w.size + NET2_SIG_SIZE > cap) {
		return 0;
	}
	Net2_Sign(seed, out, (size_t)w.size, out + w.size);
	return w.size + NET2_SIG_SIZE;
}

int Coord_TicketOpen(const uint8_t *data, int size, const uint8_t pub[NET2_KEY_SIZE], uint64_t now, CoordTicket *out) {
	TkRead r = { data, size - NET2_SIG_SIZE, 0, 0 };

	memset(out, 0, sizeof(*out));
	if (size <= NET2_SIG_SIZE + 4 || memcmp(data, ticket_magic, 4)) {
		return COORD_TK_BAD;
	}
	Tk_Take(&r, 4);
	const uint8_t *key = Tk_Take(&r, NET2_KEY_SIZE);
	Tk_GetStr(&r, out->level, COORD_LEVEL);
	Tk_GetStr(&r, out->entrance, COORD_LEVEL);
	Tk_GetStr(&r, out->door, COORD_LEVEL);
	out->issued = Tk_GetU64(&r);
	out->expires = Tk_GetU64(&r);
	const uint8_t *nonce = Tk_Take(&r, COORD_NONCE);
	const uint8_t *len = Tk_Take(&r, 2);
	int n = len ? len[0] | len[1] << 8 : 0;
	const uint8_t *state = n > COORD_STATE_MAX ? (r.bad = 1, NULL) : Tk_Take(&r, n);
	if (r.bad || r.pos != r.size || !key || !nonce || !state) {
		memset(out, 0, sizeof(*out));
		return COORD_TK_BAD;
	}
	/* the signature before any use of the contents */
	if (!Net2_Verify(pub, data, (size_t)r.size, data + r.size)) {
		memset(out, 0, sizeof(*out));
		return COORD_TK_SIGNATURE;
	}
	memcpy(out->key, key, NET2_KEY_SIZE);
	memcpy(out->nonce, nonce, COORD_NONCE);
	memcpy(out->state, state, (size_t)n);
	out->state[n] = 0;
	out->stateLen = n;
	if (now > out->expires) {
		return COORD_TK_EXPIRED;
	}
	if (out->issued > now + COORD_SKEW) {
		return COORD_TK_EARLY;
	}
	return COORD_TK_OK;
}

void Coord_JoinHash(uint8_t hash[64], const uint8_t nonce[COORD_CHALLENGE], const uint8_t serverKey[NET2_KEY_SIZE],
	const char *name) {
	static const char label[] = "scrapland join 1";
	uint8_t buf[sizeof(label) - 1 + COORD_CHALLENGE + NET2_KEY_SIZE + COORD_NAME];
	size_t n = strlen(name);
	n = n > COORD_NAME ? COORD_NAME : n;
	memcpy(buf, label, sizeof(label) - 1);
	memcpy(buf + sizeof(label) - 1, nonce, COORD_CHALLENGE);
	memcpy(buf + sizeof(label) - 1 + COORD_CHALLENGE, serverKey, NET2_KEY_SIZE);
	memcpy(buf + sizeof(label) - 1 + COORD_CHALLENGE + NET2_KEY_SIZE, name, n);
	Net2_Hash(hash, 64, buf, sizeof(label) - 1 + COORD_CHALLENGE + NET2_KEY_SIZE + n);
}

void CoordMsg_PutBytes(NetRepBuf *w, const void *b, int n) {
	const uint8_t *p = b;
	for (int i = 0; i < n; i++) {
		NetRep_PutU8(w, p[i]);
	}
}

void CoordMsg_GetBytes(NetRepReader *r, void *b, int n) {
	uint8_t *p = b;
	for (int i = 0; i < n; i++) {
		p[i] = (uint8_t)NetRep_GetU8(r);
	}
}
