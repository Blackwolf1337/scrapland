/*
 * The list server's messages, its registry of worlds and the registration of a coordinator, see
 * include/scrap/coordlist.h.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "scrap/coordlist.h"
#include "scrap/netrep.h"

#define LC_CHANNEL 0				/* reliable in the Net2 default layout */
#define LC_RETRY_MS 10000			/* a list server that is away is tried again this often */

enum { LC_OFF, LC_CONNECTING, LC_READY };

/* ---- heartbeats ---- */

static void List_BeatHash(uint8_t hash[64], const uint8_t nonce[COORD_CHALLENGE], const uint8_t listKey[NET2_KEY_SIZE],
	const uint8_t *body, int size) {
	uint8_t buf[sizeof(LIST_SIG_LABEL) + COORD_CHALLENGE + NET2_KEY_SIZE + LIST_MSG];
	int n = (int)sizeof(LIST_SIG_LABEL);
	memcpy(buf, LIST_SIG_LABEL, (size_t)n);
	memcpy(buf + n, nonce, COORD_CHALLENGE);
	n += COORD_CHALLENGE;
	memcpy(buf + n, listKey, NET2_KEY_SIZE);
	n += NET2_KEY_SIZE;
	memcpy(buf + n, body, (size_t)size);
	Net2_Hash(hash, 64, buf, (size_t)(n + size));
}

int List_WriteBeat(uint8_t *out, int cap, const ListBeat *b, const uint8_t seed[NET2_SEED_SIZE],
	const uint8_t nonce[COORD_CHALLENGE], const uint8_t listKey[NET2_KEY_SIZE]) {
	uint8_t hash[64];
	NetRepBuf w;
	if (cap > LIST_MSG) {
		cap = LIST_MSG;
	}
	NetRepBuf_Init(&w, out, cap);
	NetRep_PutU8(&w, LIST_MSG_BEAT);
	NetRep_PutU16(&w, LIST_VERSION);
	CoordMsg_PutBytes(&w, b->w.key, NET2_KEY_SIZE);
	NetRep_PutU32(&w, b->seq);
	NetRep_PutU16(&w, b->w.addr.port);
	NetRep_PutStr(&w, b->address);
	NetRep_PutStr(&w, b->w.name);
	NetRep_PutStr(&w, b->w.map);
	NetRep_PutStr(&w, b->w.mode);
	NetRep_PutU8(&w, b->w.players);
	NetRep_PutU8(&w, b->w.maxPlayers);
	NetRep_PutU8(&w, b->w.flags);
	NetRep_PutU32(&w, b->w.appVersion);
	if (w.bad || w.size + NET2_SIG_SIZE > cap) {
		return 0;
	}
	List_BeatHash(hash, nonce, listKey, out, w.size);
	Net2_Sign(seed, hash, sizeof(hash), out + w.size);
	return w.size + NET2_SIG_SIZE;
}

int List_ReadBeat(const uint8_t *data, int size, const uint8_t nonce[COORD_CHALLENGE],
	const uint8_t listKey[NET2_KEY_SIZE], ListBeat *out) {
	uint8_t hash[64];
	NetRepReader r;
	memset(out, 0, sizeof(*out));
	if (size < 1 + NET2_SIG_SIZE || size > LIST_MSG || data[0] != LIST_MSG_BEAT) {
		return LIST_E_BAD;
	}
	int body = size - NET2_SIG_SIZE;
	NetRepReader_Init(&r, data + 1, body - 1);
	uint32_t version = NetRep_GetU16(&r);
	if (r.bad || version != LIST_VERSION) {
		return r.bad ? LIST_E_BAD : LIST_E_VERSION;
	}
	CoordMsg_GetBytes(&r, out->w.key, NET2_KEY_SIZE);
	out->seq = NetRep_GetU32(&r);
	out->w.addr.port = (uint16_t)NetRep_GetU16(&r);
	NetRep_GetStr(&r, out->address, sizeof(out->address));
	NetRep_GetStr(&r, out->w.name, sizeof(out->w.name));
	NetRep_GetStr(&r, out->w.map, sizeof(out->w.map));
	NetRep_GetStr(&r, out->w.mode, sizeof(out->w.mode));
	out->w.players = (uint8_t)NetRep_GetU8(&r);
	out->w.maxPlayers = (uint8_t)NetRep_GetU8(&r);
	out->w.flags = (uint8_t)NetRep_GetU8(&r);
	out->w.appVersion = NetRep_GetU32(&r);
	if (r.bad || r.pos != r.size || !out->w.addr.port) {
		return LIST_E_BAD;
	}
	List_BeatHash(hash, nonce, listKey, data, body);
	if (!Net2_Verify(out->w.key, hash, sizeof(hash), data + body)) {
		return LIST_E_SIGNATURE;
	}
	return LIST_OK;
}

/* ---- the registry ---- */

void ListReg_Init(ListReg *r, uint32_t ttlMs, int perAddr) {
	memset(r, 0, sizeof(*r));
	r->ttl = ttlMs ? ttlMs : LIST_TTL_SECS * 1000u;
	r->perAddr = perAddr > 0 && perAddr < LIST_PER_ADDR ? perAddr : LIST_PER_ADDR;
}

static int List_Loopback(const Net2Addr *a) {
	static const uint8_t v6[16] = { [15] = 1 };
	return (a->family == NET2_AF_IPV4 && a->ip[0] == 127) || (a->family == NET2_AF_IPV6 && !memcmp(a->ip, v6, 16));
}

static int List_SameIp(const Net2Addr *a, const Net2Addr *b) {
	return a->family == b->family && !memcmp(a->ip, b->ip, sizeof(a->ip));
}

int ListReg_Beat(ListReg *r, const ListBeat *b, const uint8_t nonce[COORD_CHALLENGE], const Net2Addr *peer,
	uint64_t now, int *isNew) {
	Net2Addr at = *peer;
	ListEntry *e = NULL, *free = NULL;
	int sameAddr = 0;

	*isNew = 0;
	if (b->address[0]) {
		if (!List_Loopback(peer) || !Net2_ParseAddr(b->address, b->w.addr.port, &at)) {
			return LIST_E_ADDRESS;
		}
	}
	at.port = b->w.addr.port;
	at.scope = peer->scope;
	for (int i = 0; i < LIST_MAX; i++) {
		ListEntry *x = &r->e[i];
		if (!x->used) {
			free = free ? free : x;
			continue;
		}
		if (!memcmp(x->w.key, b->w.key, NET2_KEY_SIZE)) {
			e = x;
		} else if (List_SameIp(&x->w.addr, &at)) {
			sameAddr++;
		}
	}
	if (e && !memcmp(e->nonce, nonce, COORD_CHALLENGE) && b->seq <= e->seq) {
		return LIST_E_REPLAY;
	}
	if (!e) {
		if (!free || sameAddr >= r->perAddr) {
			return LIST_E_FULL;
		}
		e = free;
		memset(e, 0, sizeof(*e));
		e->used = 1;
		*isNew = 1;
	}
	/* a world that moved is asked again before it is listed at the new address */
	if (!Net2_AddrEqual(&e->w.addr, &at)) {
		e->verified = 0;
		e->probeAt = now;
	}
	e->w = b->w;
	e->w.addr = at;
	memcpy(e->nonce, nonce, COORD_CHALLENGE);
	e->seq = b->seq;
	e->expires = now + r->ttl;
	return LIST_OK;
}

int ListReg_Expire(ListReg *r, uint64_t now) {
	int n = 0;
	for (int i = 0; i < LIST_MAX; i++) {
		if (r->e[i].used && now >= r->e[i].expires) {
			r->e[i].used = 0;
			n++;
		}
	}
	return n;
}

int ListReg_NextProbe(ListReg *r, uint64_t now, Net2Addr *out) {
	for (int i = 0; i < LIST_MAX; i++) {
		ListEntry *e = &r->e[i];
		if (e->used && !e->verified && now >= e->probeAt) {
			e->probeAt = now + LIST_PROBE_MS;
			*out = e->w.addr;
			return 1;
		}
	}
	return 0;
}

int ListReg_Answer(ListReg *r, const Net2Addr *addr, const Net2ServerInfo *info) {
	int found = 0;
	for (int i = 0; i < LIST_MAX; i++) {
		ListEntry *e = &r->e[i];
		if (e->used && Net2_AddrEqual(&e->w.addr, addr)) {
			e->verified = 1;
			e->w.players = info->players;
			e->w.maxPlayers = info->maxPlayers;
			found = 1;
		}
	}
	return found;
}

int ListReg_Count(const ListReg *r) {
	int n = 0;
	for (int i = 0; i < LIST_MAX; i++) {
		n += r->e[i].used && r->e[i].verified;
	}
	return n;
}

int ListReg_WritePage(const ListReg *r, int first, uint8_t *out, int cap, int *count) {
	NetRepBuf w;
	int total = ListReg_Count(r), k = 0, n = 0;
	NetRepBuf_Init(&w, out, cap);
	NetRep_PutU8(&w, LIST_MSG_PAGE);
	NetRep_PutU16(&w, (uint32_t)total);
	NetRep_PutU16(&w, (uint32_t)first);
	int countAt = w.size;
	NetRep_PutU8(&w, 0);
	for (int i = 0; i < LIST_MAX && n < LIST_PAGE; i++) {
		const ListEntry *e = &r->e[i];
		if (!e->used || !e->verified || k++ < first) {
			continue;
		}
		NetRep_PutU8(&w, e->w.addr.family);
		CoordMsg_PutBytes(&w, e->w.addr.ip, 16);
		NetRep_PutU16(&w, e->w.addr.port);
		CoordMsg_PutBytes(&w, e->w.key, NET2_KEY_SIZE);
		NetRep_PutStr(&w, e->w.name);
		NetRep_PutStr(&w, e->w.map);
		NetRep_PutStr(&w, e->w.mode);
		NetRep_PutU8(&w, e->w.players);
		NetRep_PutU8(&w, e->w.maxPlayers);
		NetRep_PutU8(&w, e->w.flags);
		n++;
	}
	if (w.bad) {
		return 0;
	}
	out[countAt] = (uint8_t)n;
	*count = n;
	return w.size;
}

int List_ReadPage(const uint8_t *data, int size, ListWorld *out, int cap, int *total, int *first) {
	NetRepReader r;
	if (size < 6 || data[0] != LIST_MSG_PAGE) {
		return 0;
	}
	NetRepReader_Init(&r, data + 1, size - 1);
	*total = (int)NetRep_GetU16(&r);
	*first = (int)NetRep_GetU16(&r);
	int n = (int)NetRep_GetU8(&r), k = 0;
	for (int i = 0; i < n && !r.bad; i++) {
		ListWorld w;
		memset(&w, 0, sizeof(w));
		w.addr.family = (uint8_t)NetRep_GetU8(&r);
		CoordMsg_GetBytes(&r, w.addr.ip, 16);
		w.addr.port = (uint16_t)NetRep_GetU16(&r);
		CoordMsg_GetBytes(&r, w.key, NET2_KEY_SIZE);
		NetRep_GetStr(&r, w.name, sizeof(w.name));
		NetRep_GetStr(&r, w.map, sizeof(w.map));
		NetRep_GetStr(&r, w.mode, sizeof(w.mode));
		w.players = (uint8_t)NetRep_GetU8(&r);
		w.maxPlayers = (uint8_t)NetRep_GetU8(&r);
		w.flags = (uint8_t)NetRep_GetU8(&r);
		if (w.addr.family != NET2_AF_IPV4 && w.addr.family != NET2_AF_IPV6) {
			r.bad = 1;
		}
		if (!r.bad && k < cap) {
			out[k++] = w;
		}
	}
	if (r.bad || r.pos != r.size || *first + n > *total) {
		return 0;
	}
	return k;
}

/* ---- the registration of a coordinator ---- */

void ListClient_Init(ListClient *c, const char *addr, uint16_t port) {
	memset(c, 0, sizeof(*c));
	snprintf(c->addr, sizeof(c->addr), "%s", addr ? addr : "");
	c->port = port ? port : COORD_LIST_PORT;
	c->lastPlayers = -1;
}

static void LC_Log(void (*log)(const char *fmt, ...), const char *fmt, ...) {
	char buf[256];
	va_list ap;
	if (!log) {
		return;
	}
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	log("%s", buf);
}

static void LC_Connect(ListClient *c, uint64_t now, void (*log)(const char *fmt, ...)) {
	Net2Config cfg;
	Net2Addr a;
	int err = 0;
	c->retryAt = now + LC_RETRY_MS;
	if (!Net2_ParseAddr(c->addr, c->port, &a)) {
		LC_Log(log, "list: '%s' is not a numeric address\n", c->addr);
		c->addr[0] = 0;
		return;
	}
	if (!c->net) {
		Net2_DefaultConfig(&cfg, 0);
		cfg.appVersion = LIST_VERSION;
		cfg.ipv4 = a.family == NET2_AF_IPV4;
		cfg.ipv6 = a.family == NET2_AF_IPV6;
		c->net = Net2_Create(&cfg, &err);
		if (!c->net) {
			LC_Log(log, "list: no Net2 client (error %d)\n", err);
			return;
		}
	}
	if (Net2_Connect(c->net, &a, now) == NET2_OK) {
		c->phase = LC_CONNECTING;
	}
}

void ListClient_Pass(ListClient *c, uint64_t now, const ListBeat *b, const uint8_t seed[NET2_SEED_SIZE],
	void (*log)(const char *fmt, ...)) {
	Net2Event ev;
	if (!c->addr[0]) {
		return;
	}
	if (c->phase == LC_OFF && now >= c->retryAt) {
		LC_Connect(c, now, log);
	}
	if (!c->net) {
		return;
	}
	Net2_Poll(c->net, now);
	while (Net2_Receive(c->net, &ev)) {
		if (ev.type == NET2_EV_DISCONNECT) {
			LC_Log(log, "list: %s %s (%s)\n", c->phase == LC_CONNECTING ? "not reached" : "lost", c->addr,
				Net2_ReasonString(ev.reason));
			c->phase = LC_OFF;
			c->listed = 0;
			c->retryAt = now + LC_RETRY_MS;
		} else if (ev.type == NET2_EV_MESSAGE && ev.channel == LC_CHANNEL && ev.size >= 1) {
			NetRepReader r;
			NetRepReader_Init(&r, ev.data + 1, ev.size - 1);
			if (ev.data[0] == LIST_MSG_CHALLENGE) {
				uint32_t version = NetRep_GetU16(&r);
				CoordMsg_GetBytes(&r, c->nonce, COORD_CHALLENGE);
				if (!r.bad && version == LIST_VERSION) {
					c->phase = LC_READY;
					c->nextBeat = now;
					c->seq = 0;
				}
			} else if (ev.data[0] == LIST_MSG_ANSWER) {
				int result = (int)NetRep_GetU8(&r);
				int secs = (int)NetRep_GetU16(&r);
				if (!r.bad && (result == LIST_OK) != c->listed) {
					LC_Log(log, result == LIST_OK ? "list: registered with %s (%d s)\n" :
						"list: %s refused the heartbeat (%d)\n", c->addr, result == LIST_OK ? secs : result);
				}
				c->listed = !r.bad && result == LIST_OK;
			}
		}
	}
	if (c->phase == LC_READY && (now >= c->nextBeat || (b->w.players != c->lastPlayers &&
		now - c->lastBeat >= LIST_BEAT_MIN_SECS * 1000u))) {
		uint8_t msg[LIST_MSG], listKey[NET2_KEY_SIZE];
		ListBeat beat = *b;
		beat.seq = ++c->seq;
		int n = Net2_ServerKey(c->net, listKey) ? List_WriteBeat(msg, sizeof(msg), &beat, seed, c->nonce, listKey) : 0;
		if (n > 0) {
			Net2_Send(c->net, 0, LC_CHANNEL, msg, n);
		}
		c->lastBeat = now;
		c->nextBeat = now + LIST_BEAT_SECS * 1000u;
		c->lastPlayers = b->w.players;
	}
	Net2_Flush(c->net, now);
}

void ListClient_Close(ListClient *c) {
	if (c->net) {
		Net2_Destroy(c->net);
		c->net = NULL;
	}
	c->phase = LC_OFF;
}
