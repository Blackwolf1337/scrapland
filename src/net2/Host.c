/*
 * Net2 host: the API of include/scrap/net2.h. Owns the sockets (or the game's Net2Io), the connection table, the
 * identity key and the cookie secret, and hands the events of the connections to Net2_Receive in a fixed order per
 * connection: connect, its messages, disconnect (so a last reliable message before a disconnect is not lost).
 */
#include <stdlib.h>

#include "monocypher.h"
#include "net2local.h"

void *Net2_Alloc(Net2Host *h, size_t size) {
	void *p = h->cfg.alloc ? h->cfg.alloc(h->cfg.allocCtx, size) : malloc(size);
	if (p) {
		memset(p, 0, size);
	}
	return p;
}

void Net2_Free(Net2Host *h, void *p) {
	if (!p) {
		return;
	}
	if (h->cfg.free) {
		h->cfg.free(h->cfg.allocCtx, p);
	} else {
		free(p);
	}
}

static int Net2_SockSend(void *ctx, const Net2Addr *to, const uint8_t *data, int size) {
	Net2Host *h = ctx;
	uintptr_t s = to->family == NET2_AF_IPV6 ? h->sock6 : h->sock4;
	return s != N2_NOSOCK && Net2Sock_Send(s, to, data, size);
}

/* one datagram of either socket */
static int Net2_SockRecv(void *ctx, Net2Addr *from, uint8_t *buf, int cap) {
	Net2Host *h = ctx;
	int n = 0;

	if (h->sock4 != N2_NOSOCK) {
		n = Net2Sock_Recv(h->sock4, from, buf, cap);
	}
	if (n <= 0 && h->sock6 != N2_NOSOCK) {
		n = Net2Sock_Recv(h->sock6, from, buf, cap);
	}
	return n;
}

int Net2_RawSend(Net2Host *h, const Net2Addr *to, const uint8_t *data, int size) {
	if (h->cfg.filter && !h->cfg.filter(h->cfg.filterCtx, 1, to, data, size)) {
		return 1;
	}
	return h->io.send(h->io.ctx, to, data, size);
}

void Net2_DefaultConfig(Net2Config *cfg, int server) {
	memset(cfg, 0, sizeof(*cfg));
	cfg->server = server;
	cfg->port = server ? NET2_DEFAULT_PORT : 0;
	cfg->ipv4 = 1;
	cfg->ipv6 = 1;
	cfg->maxConns = server ? 64 : 1;
	cfg->numChannels = 3;
	cfg->channels[0] = NET2_RELIABLE;
	cfg->channels[1] = NET2_SEQUENCED;
	cfg->channels[2] = NET2_UNRELIABLE;
	cfg->rate = 128 * 1024;
	cfg->timeout = 10000;
	cfg->connectTimeout = 5000;
	cfg->maxPending = 1 << 20;
	cfg->maxRecvBuffer = 1 << 20;
	cfg->inboxSize = 64 * 1024;
	cfg->outboxSize = 64 * 1024;
	/* the system defaults (Linux 208 KB, Windows 64 KB) overflow with a few hundred clients polled at 30 Hz */
	cfg->sockBuffer = server ? 4 << 20 : 0;
}

static int Net2_CheckConfig(const Net2Config *cfg) {
	if (cfg->numChannels < 1 || cfg->numChannels > NET2_MAX_CHANNELS) {
		return 0;
	}
	for (int i = 0; i < cfg->numChannels; i++) {
		if (cfg->channels[i] > NET2_RELIABLE) {
			return 0;
		}
	}
	if (cfg->server && (cfg->maxConns < 1 || cfg->maxConns > NET2_MAX_CONNS)) {
		return 0;
	}
	if (!cfg->io && !cfg->ipv4 && !cfg->ipv6) {
		return 0;
	}
	if ((cfg->alloc == NULL) != (cfg->free == NULL) || cfg->rate == 0 || cfg->maxPending == 0
		|| cfg->maxRecvBuffer == 0) {
		return 0;
	}
	/* one largest message has to fit in each box */
	return cfg->inboxSize >= 3 + NET2_MAX_UNRELIABLE && cfg->outboxSize >= 5 + NET2_MAX_UNRELIABLE;
}

static void Net2_FreeHost(Net2Host *h) {
	if (h->conns) {
		for (int i = 0; i < h->numConns; i++) {
			Net2Conn_Release(h, &h->conns[i]);
		}
		Net2_Free(h, h->conns);
	}
	Net2_Free(h, h->taken);
	Net2Sock_Close(h->sock4);
	Net2Sock_Close(h->sock6);
	if (h->sockStarted) {
		Net2Sock_Cleanup();
	}
	Net2Config cfg = h->cfg;
	Net2Crypto_Wipe(h, sizeof(*h));
	if (cfg.free) {
		cfg.free(cfg.allocCtx, h);
	} else {
		free(h);
	}
}

Net2Host *Net2_Create(const Net2Config *cfg, int *err) {
	uint8_t seed[32];
	Net2Host *h;
	int e = NET2_E_ARG;

	if (!cfg || !Net2_CheckConfig(cfg)) {
		goto fail;
	}
	e = NET2_E_NOMEM;
	h = cfg->alloc ? cfg->alloc(cfg->allocCtx, sizeof(*h)) : malloc(sizeof(*h));
	if (!h) {
		goto fail;
	}
	memset(h, 0, sizeof(*h));
	h->cfg = *cfg;
	h->cfg.identitySeed = NULL;
	h->cfg.serverKey = NULL;
	h->cfg.io = NULL;
	h->sock4 = N2_NOSOCK;
	h->sock6 = N2_NOSOCK;
	if (!cfg->server) {
		h->cfg.maxConns = 1;
	}
	if (cfg->serverKey) {
		memcpy(h->pinKey, cfg->serverKey, NET2_KEY_SIZE);
		h->havePin = 1;
	}
	if (cfg->identitySeed) {
		memcpy(seed, cfg->identitySeed, sizeof(seed));
	} else if (!Net2_Random(seed, sizeof(seed))) {
		e = NET2_E_RANDOM;
		goto failHost;
	}
	crypto_eddsa_key_pair(h->idSk, h->idPk, seed);	/* wipes seed */
	if (!Net2_Random(h->cookieSecret, sizeof(h->cookieSecret)) || !Net2_Random(&h->queryToken, 4)) {
		e = NET2_E_RANDOM;
		goto failHost;
	}
	h->numConns = h->cfg.maxConns;
	h->conns = Net2_Alloc(h, sizeof(Net2Conn) * (size_t)h->numConns);
	if (!h->conns) {
		goto failHost;
	}
	for (int i = 0; i < h->numConns; i++) {
		h->conns[i].index = i;
	}
	if (cfg->io) {
		h->io = *cfg->io;
	} else {
		e = NET2_E_SOCKET;
		if (!Net2Sock_Startup()) {
			goto failHost;
		}
		h->sockStarted = 1;
		uint16_t port = cfg->port;
		if (cfg->ipv4) {
			h->sock4 = Net2Sock_Open(NET2_AF_IPV4, &cfg->bind4, port, cfg->sockBuffer);
			if (h->sock4 != N2_NOSOCK && port == 0 && cfg->server) {
				port = Net2Sock_Port(h->sock4);	/* the same port for IPv6 */
			}
		}
		if (cfg->ipv6) {
			h->sock6 = Net2Sock_Open(NET2_AF_IPV6, &cfg->bind6, port, cfg->sockBuffer);
		}
		if (h->sock4 == N2_NOSOCK && h->sock6 == N2_NOSOCK) {
			goto failHost;
		}
		h->io.ctx = h;
		h->io.send = Net2_SockSend;
		h->io.recv = Net2_SockRecv;
	}
	if (err) {
		*err = NET2_OK;
	}
	return h;

failHost:
	Net2Crypto_Wipe(seed, sizeof(seed));
	Net2_FreeHost(h);
fail:
	if (err) {
		*err = e;
	}
	return NULL;
}

void Net2_Destroy(Net2Host *h) {
	if (!h) {
		return;
	}
	for (int i = 0; i < h->numConns; i++) {
		Net2Conn *c = &h->conns[i];
		Net2Conn_Close(h, c, NET2_DR_SHUTDOWN, 1);
		Net2Conn_Flush(h, c);
	}
	Net2_FreeHost(h);
}

int Net2_Connect(Net2Host *h, const Net2Addr *server, uint64_t now) {
	Net2Conn *c = &h->conns[0];

	if (h->cfg.server || c->state != N2C_FREE) {
		return NET2_E_STATE;
	}
	if (server->family != NET2_AF_IPV4 && server->family != NET2_AF_IPV6) {
		return NET2_E_ARG;
	}
	if (h->sockStarted && (server->family == NET2_AF_IPV6 ? h->sock6 : h->sock4) == N2_NOSOCK) {
		return NET2_E_SOCKET;
	}
	h->now = now;
	if (!Net2Conn_Init(h, c)) {
		Net2Conn_Release(h, c);
		return NET2_E_NOMEM;
	}
	if (!Net2_Random(c->salt, sizeof(c->salt)) || !Net2_Random(c->secret, sizeof(c->secret))) {
		Net2Conn_Release(h, c);
		return NET2_E_RANDOM;
	}
	c->addr = *server;
	Net2Hs_ClientStart(h, c);
	return NET2_OK;
}

static void Net2_Dispatch(Net2Host *h, const Net2Addr *from, const uint8_t *pkt, int size) {
	int type = pkt[0];

	if (type == N2P_DATA) {
		if (size < N2_HEADER + N2_TAG + N2_ACKS) {
			return;
		}
		uint32_t id = (uint32_t)pkt[1] | (uint32_t)pkt[2] << 8 | (uint32_t)pkt[3] << 16 | (uint32_t)pkt[4] << 24;
		uint32_t i = h->cfg.server ? id & (NET2_MAX_CONNS - 1) : 0;
		if (i >= (uint32_t)h->numConns) {
			return;
		}
		Net2Conn *c = &h->conns[i];
		if (c->id != id || (c->state != N2C_ACCEPTED && c->state != N2C_CONNECTED && c->state != N2C_CLOSING)) {
			return;
		}
		if (!h->cfg.server && !Net2_AddrEqual(from, &c->addr)) {
			return;
		}
		Net2Conn_Data(h, c, from, pkt, size);
	} else if (type >= N2P_HELLO && type <= N2P_DENY) {
		Net2Hs_Packet(h, from, pkt, size);
	} else if (type == N2P_QUERY || type == N2P_INFO) {
		Net2Lan_Packet(h, from, pkt, size);
	}
}

void Net2_Poll(Net2Host *h, uint64_t now) {
	uint8_t buf[NET2_MTU + 1];
	Net2Addr from;

	h->now = now;
	for (int k = 0; k < N2_POLL_PACKETS; k++) {
		int n = h->io.recv(h->io.ctx, &from, buf, sizeof(buf));
		if (n <= 0) {
			break;
		}
		if (n > NET2_MTU) {
			continue;
		}
		if (h->cfg.filter && !h->cfg.filter(h->cfg.filterCtx, 0, &from, buf, n)) {
			continue;
		}
		Net2_Dispatch(h, &from, buf, n);
	}
	for (int i = 0; i < h->numConns; i++) {
		Net2Conn *c = &h->conns[i];
		if (c->state == N2C_HELLO || c->state == N2C_CONNECT) {
			Net2Hs_ClientTick(h, c);
		} else if (c->state != N2C_FREE) {
			Net2Conn_Tick(h, c);
		}
	}
}

static int Net2_ConnEvent(Net2Host *h, Net2Conn *c, Net2Event *ev) {
	ev->conn = c->index;
	if (c->evConnect) {
		c->evConnect = 0;
		ev->type = NET2_EV_CONNECT;
		ev->addr = c->addr;
		return 1;
	}
	if (c->inboxRead < c->inboxWrite) {
		const uint8_t *p = c->inbox + c->inboxRead;
		ev->type = NET2_EV_MESSAGE;
		ev->channel = p[0];
		ev->size = p[1] | p[2] << 8;
		ev->data = p + 3;
		c->inboxRead += 3 + (uint32_t)ev->size;
		if (c->inboxRead == c->inboxWrite) {
			c->inboxRead = 0;		/* the data stays put until the next Net2_Poll writes the inbox again */
			c->inboxWrite = 0;
		}
		return 1;
	}
	for (int i = 0; i < h->cfg.numChannels; i++) {
		if (c->ch[i].rr && Net2Rel_Ready(c, i)) {
			ev->type = NET2_EV_MESSAGE;
			ev->channel = i;
			h->taken = Net2Rel_Take(c, i, &ev->size);
			ev->data = h->taken;
			return 1;
		}
	}
	if (c->state == N2C_CLOSED && c->evDisconnect) {
		ev->type = NET2_EV_DISCONNECT;
		ev->reason = c->reason;
		ev->addr = c->addr;
		Net2Conn_Release(h, c);
		return 1;
	}
	return 0;
}

int Net2_Receive(Net2Host *h, Net2Event *ev) {
	Net2_Free(h, h->taken);
	h->taken = NULL;
	memset(ev, 0, sizeof(*ev));
	if (h->evCount) {
		*ev = h->events[h->evHead];
		h->evHead = (h->evHead + 1) % N2_EVENTS;
		h->evCount--;
		return 1;
	}
	for (int k = 0; k < h->numConns; k++) {
		int i = (h->cursor + k) % h->numConns;
		Net2Conn *c = &h->conns[i];
		if (c->state != N2C_FREE && Net2_ConnEvent(h, c, ev)) {
			h->cursor = i;
			return 1;
		}
	}
	return 0;
}

static Net2Conn *Net2_GetConn(const Net2Host *h, int conn) {
	if (conn < 0 || conn >= h->numConns || h->conns[conn].state == N2C_FREE) {
		return NULL;
	}
	return &h->conns[conn];
}

int Net2_Send(Net2Host *h, int conn, int channel, const void *data, int size) {
	Net2Conn *c = Net2_GetConn(h, conn);

	if (!c || c->state != N2C_CONNECTED) {
		return NET2_E_STATE;
	}
	if (channel < 0 || channel >= h->cfg.numChannels || size < 0 || (size > 0 && !data)) {
		return NET2_E_ARG;
	}
	Net2Channel *ch = &c->ch[channel];
	if (ch->kind == NET2_RELIABLE) {
		return Net2Rel_Queue(h, c, channel, data, size);
	}
	if (size > NET2_MAX_UNRELIABLE) {
		return NET2_E_ARG;
	}
	if (c->outboxUsed + 5 + (uint32_t)size > h->cfg.outboxSize) {
		c->dropped++;
		return NET2_E_FULL;
	}
	uint16_t seq = ch->kind == NET2_SEQUENCED ? ++ch->sendSeq : 0;
	uint8_t *p = c->outbox + c->outboxUsed;
	p[0] = (uint8_t)channel;
	p[1] = (uint8_t)seq;
	p[2] = (uint8_t)(seq >> 8);
	p[3] = (uint8_t)size;
	p[4] = (uint8_t)(size >> 8);
	if (size) {
		memcpy(p + 5, data, (size_t)size);
	}
	c->outboxUsed += 5 + (uint32_t)size;
	return NET2_OK;
}

void Net2_Flush(Net2Host *h, uint64_t now) {
	h->now = now;
	for (int i = 0; i < h->numConns; i++) {
		Net2Conn_Flush(h, &h->conns[i]);
	}
}

void Net2_Disconnect(Net2Host *h, int conn, int reason) {
	Net2Conn *c = Net2_GetConn(h, conn);

	if (c) {
		Net2Conn_Close(h, c, reason ? reason : NET2_DR_LOCAL, 1);
	}
}

int Net2_IsConnected(const Net2Host *h, int conn) {
	const Net2Conn *c = Net2_GetConn(h, conn);
	return c && c->state == N2C_CONNECTED;
}

int Net2_GetStats(const Net2Host *h, int conn, Net2Stats *st) {
	const Net2Conn *c = Net2_GetConn(h, conn);

	if (!c) {
		memset(st, 0, sizeof(*st));
		return 0;
	}
	Net2Conn_Stats(c, st);
	return 1;
}

void Net2_SetRate(Net2Host *h, int conn, uint32_t rate) {
	Net2Conn *c = Net2_GetConn(h, conn);

	if (c && rate > 0) {
		c->rate = rate;
	}
}

int Net2_PeerAddr(const Net2Host *h, int conn, Net2Addr *addr) {
	const Net2Conn *c = Net2_GetConn(h, conn);

	if (!c) {
		return 0;
	}
	*addr = c->addr;
	return 1;
}

int Net2_ServerKey(const Net2Host *h, uint8_t key[NET2_KEY_SIZE]) {
	if (h->cfg.server) {
		memcpy(key, h->idPk, NET2_KEY_SIZE);
		return 1;
	}
	const Net2Conn *c = &h->conns[0];
	if (c->state != N2C_CONNECTED && c->state != N2C_CLOSING) {
		return 0;
	}
	memcpy(key, c->serverKey, NET2_KEY_SIZE);
	return 1;
}

uint16_t Net2_LocalPort(const Net2Host *h, int family) {
	uintptr_t s = family == NET2_AF_IPV6 ? h->sock6 : h->sock4;
	return s == N2_NOSOCK ? 0 : Net2Sock_Port(s);
}

void Net2_SetServerInfo(Net2Host *h, const Net2ServerInfo *info) {
	h->info = *info;
	h->info.name[NET2_INFO_NAME] = 0;
	h->info.map[NET2_INFO_MAP] = 0;
	h->info.mode[NET2_INFO_MODE] = 0;
}

const char *Net2_ReasonString(int reason) {
	static const char *const names[] = {
		"none", "disconnected", "timed out", "server full", "version mismatch", "channel mismatch",
		"protocol error", "shutdown", "server key mismatch", "sequence exhausted", "overflow",
	};

	if (reason >= 0 && reason < (int)(sizeof(names) / sizeof(names[0]))) {
		return names[reason];
	}
	return reason >= NET2_DR_APP ? "game" : "unknown";
}
