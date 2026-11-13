/*
 * Net2 connection handshake (formats in net2local.h):
 *   client HELLO -> server CHALLENGE (stateless: the cookie is a keyed hash of the client address, salt and key)
 *   client CONNECT with the cookie -> server makes the connection, its ephemeral key, the session keys, and
 *   answers ACCEPT signed with its identity key -> client derives the same keys; the first DATA packet of the client
 *   moves the server side from ACCEPTED to CONNECTED.
 * The client sends HELLO or CONNECT again every N2_RESEND ms until it gets an answer or connectTimeout passes (and
 * goes back to HELLO when its cookie gets old); a repeated CONNECT gets the stored ACCEPT again, so a lost ACCEPT
 * costs one resend and no second connection.
 */
#include "monocypher.h"
#include "net2local.h"

static void Net2Hs_WriteRequest(Net2Host *h, Net2Writer *w, int type, const Net2Conn *c) {
	Net2W_U8(w, (uint32_t)type);
	Net2W_U32(w, N2_PROTOCOL);
	Net2W_U16(w, NET2_VERSION);
	Net2W_U32(w, h->cfg.appVersion);
	Net2W_Bytes(w, c->salt, N2_SALT);
	Net2W_Bytes(w, c->clientPk, 32);
}

void Net2Hs_ClientStart(Net2Host *h, Net2Conn *c) {
	c->state = N2C_HELLO;
	c->forceSend = 1;
	crypto_x25519_public_key(c->clientPk, c->secret);
	Net2Hs_ClientTick(h, c);
}

void Net2Hs_ClientTick(Net2Host *h, Net2Conn *c) {
	uint8_t pkt[N2_HANDSHAKE_SIZE];
	Net2Writer w;

	if (c->state != N2C_HELLO && c->state != N2C_CONNECT) {
		return;
	}
	if (h->now - c->startTime > h->cfg.connectTimeout) {
		Net2Crypto_Wipe(c->secret, sizeof(c->secret));
		Net2Conn_Close(h, c, NET2_DR_TIMEOUT, 0);
		return;
	}
	if (c->state == N2C_CONNECT && h->now - c->cookieTime >= N2_COOKIE_PERIOD / 2) {
		c->state = N2C_HELLO;		/* the cookie may be too old by now: get a new one */
		c->forceSend = 1;
	}
	if (!c->forceSend && h->now - c->lastHandshake < N2_RESEND) {
		return;
	}
	Net2W_Init(&w, pkt, sizeof(pkt));
	if (c->state == N2C_HELLO) {
		Net2Hs_WriteRequest(h, &w, N2P_HELLO, c);
	} else {
		Net2Hs_WriteRequest(h, &w, N2P_CONNECT, c);
		Net2W_Bytes(&w, c->cookie, N2_COOKIE);
		Net2W_U8(&w, (uint32_t)h->cfg.numChannels);
		Net2W_Bytes(&w, h->cfg.channels, h->cfg.numChannels);
	}
	Net2W_Zeros(&w, (int)sizeof(pkt) - w.pos);
	Net2_RawSend(h, &c->addr, pkt, w.pos);
	c->lastHandshake = h->now;
	c->forceSend = 0;
}

static void Net2Hs_Deny(Net2Host *h, const Net2Addr *to, const uint8_t salt[N2_SALT], int reason) {
	uint8_t pkt[N2_DENY_SIZE];
	Net2Writer w;

	Net2W_Init(&w, pkt, sizeof(pkt));
	Net2W_U8(&w, N2P_DENY);
	Net2W_Bytes(&w, salt, N2_SALT);
	Net2W_U8(&w, (uint32_t)reason);
	Net2_RawSend(h, to, pkt, w.pos);
}

typedef struct Net2Request {
	uint32_t protocol, appVersion;
	uint16_t version;
	const uint8_t *salt, *pk, *cookie, *kinds;
	int numChannels;
} Net2Request;

static int Net2Hs_ReadRequest(const uint8_t *pkt, int size, Net2Request *q) {
	Net2Reader r;

	memset(q, 0, sizeof(*q));
	if (size != N2_HANDSHAKE_SIZE) {
		return 0;
	}
	Net2R_Init(&r, pkt, size);
	int type = (int)Net2R_U8(&r);
	q->protocol = Net2R_U32(&r);
	q->version = (uint16_t)Net2R_U16(&r);
	q->appVersion = Net2R_U32(&r);
	q->salt = Net2R_Bytes(&r, N2_SALT);
	q->pk = Net2R_Bytes(&r, 32);
	if (type == N2P_CONNECT) {
		q->cookie = Net2R_Bytes(&r, N2_COOKIE);
		q->numChannels = (int)Net2R_U8(&r);
		if (q->numChannels > NET2_MAX_CHANNELS) {
			return 0;
		}
		q->kinds = Net2R_Bytes(&r, q->numChannels);
	}
	return !r.bad && q->protocol == N2_PROTOCOL;
}

static void Net2Hs_Hello(Net2Host *h, const Net2Addr *from, const uint8_t *pkt, int size) {
	uint8_t out[N2_CHALLENGE_SIZE];
	uint8_t cookie[N2_COOKIE];
	Net2Request q;
	Net2Writer w;

	if (!Net2Hs_ReadRequest(pkt, size, &q)) {
		return;
	}
	if (q.version != NET2_VERSION || q.appVersion != h->cfg.appVersion) {
		Net2Hs_Deny(h, from, q.salt, NET2_DR_VERSION);
		return;
	}
	Net2Crypto_Cookie(cookie, h->cookieSecret, (uint32_t)(h->now / N2_COOKIE_PERIOD), from, q.salt, q.pk);
	Net2W_Init(&w, out, sizeof(out));
	Net2W_U8(&w, N2P_CHALLENGE);
	Net2W_Bytes(&w, q.salt, N2_SALT);
	Net2W_Bytes(&w, cookie, N2_COOKIE);
	Net2_RawSend(h, from, out, w.pos);
}

static int Net2Hs_CookieOk(Net2Host *h, const Net2Addr *from, const Net2Request *q) {
	uint32_t period = (uint32_t)(h->now / N2_COOKIE_PERIOD);
	uint8_t cookie[N2_COOKIE];

	for (int i = 0; i < 2 && (i == 0 || period > 0); i++) {
		Net2Crypto_Cookie(cookie, h->cookieSecret, period - (uint32_t)i, from, q->salt, q->pk);
		if (crypto_verify16(cookie, q->cookie) == 0) {
			return 1;
		}
	}
	return 0;
}

static void Net2Hs_Connect(Net2Host *h, const Net2Addr *from, const uint8_t *pkt, int size) {
	uint8_t sk[32], pk[32], shared[32], hash[32], sig[64];
	Net2Request q;
	Net2Conn *c = NULL;
	Net2Writer w;

	if (!Net2Hs_ReadRequest(pkt, size, &q)) {
		return;
	}
	if (q.version != NET2_VERSION || q.appVersion != h->cfg.appVersion) {
		Net2Hs_Deny(h, from, q.salt, NET2_DR_VERSION);
		return;
	}
	if (!Net2Hs_CookieOk(h, from, &q)) {
		return;
	}
	for (int i = 0; i < h->numConns; i++) {
		Net2Conn *e = &h->conns[i];
		if (e->state != N2C_FREE && Net2_AddrEqual(&e->addr, from) && memcmp(e->clientPk, q.pk, 32) == 0) {
			if (e->state == N2C_ACCEPTED) {
				Net2_RawSend(h, from, e->accept, N2_ACCEPT_SIZE);
			}
			return;
		}
	}
	if (q.numChannels != h->cfg.numChannels || memcmp(q.kinds, h->cfg.channels, (size_t)q.numChannels) != 0) {
		Net2Hs_Deny(h, from, q.salt, NET2_DR_CHANNELS);
		return;
	}
	/* a connect storm is spread over the next polls (the clients send CONNECT again) instead of one long frame */
	if (h->acceptsLeft <= 0) {
		return;
	}
	h->acceptsLeft--;
	for (int i = 0; i < h->numConns; i++) {
		if (h->conns[i].state == N2C_FREE) {
			c = &h->conns[i];
			break;
		}
	}
	if (!c) {
		Net2Hs_Deny(h, from, q.salt, NET2_DR_FULL);
		return;
	}
	if (!Net2_Random(sk, sizeof(sk)) || !Net2_Random(&c->id, sizeof(c->id)) || !Net2Conn_Init(h, c)) {
		Net2Conn_Release(h, c);
		return;
	}
	c->id = (c->id & ~(uint32_t)(NET2_MAX_CONNS - 1)) | (uint32_t)c->index;
	c->addr = *from;
	memcpy(c->clientPk, q.pk, 32);
	memcpy(c->salt, q.salt, N2_SALT);
	crypto_x25519_public_key(pk, sk);
	int ok = Net2Crypto_Shared(shared, sk, q.pk);
	Net2Crypto_Wipe(sk, sizeof(sk));
	if (!ok) {
		Net2Crypto_Wipe(shared, sizeof(shared));
		Net2Conn_Release(h, c);
		Net2Hs_Deny(h, from, q.salt, NET2_DR_PROTOCOL);
		return;
	}
	Net2Crypto_Keys(c->recvKey, c->sendKey, shared, q.pk, pk);
	Net2Crypto_Wipe(shared, sizeof(shared));
	Net2Crypto_Transcript(hash, c->salt, c->id, c->clientPk, pk);
	crypto_eddsa_sign(sig, h->idSk, hash, sizeof(hash));

	Net2W_Init(&w, c->accept, N2_ACCEPT_SIZE);
	Net2W_U8(&w, N2P_ACCEPT);
	Net2W_Bytes(&w, c->salt, N2_SALT);
	Net2W_U32(&w, c->id);
	Net2W_Bytes(&w, pk, 32);
	Net2W_Bytes(&w, h->idPk, 32);
	Net2W_Bytes(&w, sig, 64);
	c->state = N2C_ACCEPTED;
	Net2_RawSend(h, from, c->accept, N2_ACCEPT_SIZE);
}

static void Net2Hs_Accept(Net2Host *h, Net2Conn *c, const uint8_t *pkt, int size) {
	uint8_t shared[32], hash[32];
	Net2Reader r;

	if (c->state != N2C_CONNECT || size != N2_ACCEPT_SIZE) {
		return;
	}
	Net2R_Init(&r, pkt + 1 + N2_SALT, size - 1 - N2_SALT);
	uint32_t id = Net2R_U32(&r);
	const uint8_t *serverPk = Net2R_Bytes(&r, 32);
	const uint8_t *idPk = Net2R_Bytes(&r, 32);
	const uint8_t *sig = Net2R_Bytes(&r, 64);
	if (r.bad) {
		return;
	}
	Net2Crypto_Transcript(hash, c->salt, id, c->clientPk, serverPk);
	if (crypto_eddsa_check(sig, idPk, hash, sizeof(hash)) != 0) {
		return;						/* not made by the holder of idPk: ignore, the real answer may still come */
	}
	if (h->havePin && memcmp(idPk, h->pinKey, NET2_KEY_SIZE) != 0) {
		Net2Crypto_Wipe(c->secret, sizeof(c->secret));
		Net2Conn_Close(h, c, NET2_DR_KEY, 0);
		return;
	}
	int ok = Net2Crypto_Shared(shared, c->secret, serverPk);
	Net2Crypto_Wipe(c->secret, sizeof(c->secret));
	if (!ok) {
		Net2Crypto_Wipe(shared, sizeof(shared));
		Net2Conn_Close(h, c, NET2_DR_PROTOCOL, 0);
		return;
	}
	Net2Crypto_Keys(c->sendKey, c->recvKey, shared, c->clientPk, serverPk);
	Net2Crypto_Wipe(shared, sizeof(shared));
	c->id = id;
	memcpy(c->serverKey, idPk, NET2_KEY_SIZE);
	c->state = N2C_CONNECTED;
	c->evConnect = 1;
	c->forceSend = 1;				/* the first DATA moves the server to CONNECTED */
	c->lastRecv = h->now;
	c->lastSend = h->now;
	c->tokenTime = h->now;
	c->rateTime = h->now;
}

void Net2Hs_Packet(Net2Host *h, const Net2Addr *from, const uint8_t *pkt, int size) {
	int type = pkt[0];

	if (h->cfg.server) {
		if (type == N2P_HELLO) {
			Net2Hs_Hello(h, from, pkt, size);
		} else if (type == N2P_CONNECT) {
			Net2Hs_Connect(h, from, pkt, size);
		}
		return;
	}
	Net2Conn *c = &h->conns[0];
	if ((c->state != N2C_HELLO && c->state != N2C_CONNECT) || !Net2_AddrEqual(from, &c->addr)
		|| size < 1 + N2_SALT || memcmp(pkt + 1, c->salt, N2_SALT) != 0) {
		return;
	}
	if (type == N2P_CHALLENGE && size == N2_CHALLENGE_SIZE && c->state == N2C_HELLO) {
		memcpy(c->cookie, pkt + 1 + N2_SALT, N2_COOKIE);
		c->cookieTime = h->now;
		c->state = N2C_CONNECT;
		c->forceSend = 1;
		Net2Hs_ClientTick(h, c);
	} else if (type == N2P_ACCEPT) {
		Net2Hs_Accept(h, c, pkt, size);
	} else if (type == N2P_DENY && size == N2_DENY_SIZE) {
		Net2Crypto_Wipe(c->secret, sizeof(c->secret));
		Net2Conn_Close(h, c, pkt[1 + N2_SALT] ? pkt[1 + N2_SALT] : NET2_DR_PROTOCOL, 0);
	}
}
