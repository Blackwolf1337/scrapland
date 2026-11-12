/*
 * Unit tests of Net2: the pure parts (reader and writer, the packet window, chunks, AEAD vectors, cookies, server
 * info, addresses, reassembly) and whole hosts over an in-memory link with loss, duplicates and reordering, on a
 * virtual clock (no sockets, no waiting). Exit code 0 when all pass.
 */
#include <stdio.h>
#include <stdlib.h>

#include "monocypher.h"
#include "net2local.h"
#include "test/testmsg.h"

static int g_fails, g_checks;

#define T_ASSERT(c) do { \
	g_checks++; \
	if (!(c)) { \
		g_fails++; \
		printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
	} \
} while (0)

static uint32_t g_rng = 1;

static uint32_t Rand(void) {
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 17;
	g_rng ^= g_rng << 5;
	return g_rng;
}

static int HexDecode(const char *hex, uint8_t *out, int cap) {
	int n = 0;
	for (; hex[0] && hex[1] && n < cap; hex += 2) {
		unsigned v;
		if (sscanf(hex, "%2x", &v) != 1) {
			return -1;
		}
		out[n++] = (uint8_t)v;
	}
	return n;
}

static void Test_Wire(void) {
	uint8_t buf[8];
	Net2Writer w;
	Net2Reader r;

	Net2W_Init(&w, buf, sizeof(buf));
	Net2W_U16(&w, 0x1234);
	Net2W_U32(&w, 0xa1b2c3d4);
	T_ASSERT(!w.overflow && w.pos == 6);
	T_ASSERT(buf[0] == 0x34 && buf[1] == 0x12 && buf[2] == 0xd4 && buf[5] == 0xa1);
	Net2W_U32(&w, 1);
	T_ASSERT(w.overflow && w.pos == 6);
	Net2W_U8(&w, 1);
	T_ASSERT(w.overflow && w.pos == 6);

	Net2R_Init(&r, buf, 6);
	T_ASSERT(Net2R_U16(&r) == 0x1234);
	T_ASSERT(Net2R_U32(&r) == 0xa1b2c3d4);
	T_ASSERT(Net2R_Left(&r) == 0 && !r.bad);
	T_ASSERT(Net2R_U8(&r) == 0 && r.bad);
	T_ASSERT(Net2R_Bytes(&r, 0) == NULL);
	Net2R_Init(&r, buf, 6);
	T_ASSERT(Net2R_Bytes(&r, -1) == NULL && r.bad);
	Net2R_Init(&r, buf, 6);
	T_ASSERT(Net2R_Bytes(&r, 7) == NULL && r.bad && Net2R_Left(&r) == 0);
	T_ASSERT(Net2_IdDiff(1, 65535) == 2 && Net2_IdDiff(65535, 1) == -2);
}

static void Test_Window(void) {
	Net2Window w;

	memset(&w, 0, sizeof(w));
	T_ASSERT(!Net2Win_Check(&w, 0));
	T_ASSERT(Net2Win_Check(&w, 1));
	for (uint32_t s = 1; s <= 40; s++) {
		if (s != 35) {
			Net2Win_Mark(&w, s);
		}
	}
	T_ASSERT(w.top == 40);
	T_ASSERT(!Net2Win_Check(&w, 40) && !Net2Win_Check(&w, 1) && Net2Win_Check(&w, 35));
	/* bit i of the ack bits is packet top - 1 - i: 35 is bit 4 */
	T_ASSERT(Net2Win_AckBits(&w) == (0xffffffffu & ~(1u << 4)));
	Net2Win_Mark(&w, 35);
	T_ASSERT(Net2Win_AckBits(&w) == 0xffffffffu && !Net2Win_Check(&w, 35));
	/* a jump: everything older than 64 behind the top is refused, the gap is open */
	Net2Win_Mark(&w, 100);
	T_ASSERT(!Net2Win_Check(&w, 36) && !Net2Win_Check(&w, 37));	/* 36: 64 behind; 37: received */
	T_ASSERT(Net2Win_Check(&w, 41));	/* in the gap */
	T_ASSERT(Net2Win_Check(&w, 99) && !Net2Win_Check(&w, 100));
	Net2Win_Mark(&w, 100 + 64);
	T_ASSERT(w.bits == 1 && !Net2Win_Check(&w, 100));
	Net2Win_Mark(&w, 500);
	T_ASSERT(w.top == 500 && w.bits == 1 && Net2Win_AckBits(&w) == 0);
	/* old and duplicate marks do not move the top */
	Net2Win_Mark(&w, 10);
	T_ASSERT(w.top == 500 && w.bits == 1);
	Net2Win_Mark(&w, UINT32_MAX);
	T_ASSERT(w.top == UINT32_MAX && !Net2Win_Check(&w, UINT32_MAX));
}

static void Test_Chunks(void) {
	uint8_t buf[2048], data[NET2_FRAG_SIZE];
	Net2Writer w;
	Net2Reader r;
	Net2Chunk c, out;
	static const uint8_t kinds[3] = { NET2_RELIABLE, NET2_SEQUENCED, NET2_UNRELIABLE };

	for (int i = 0; i < (int)sizeof(data); i++) {
		data[i] = (uint8_t)(i * 7);
	}
	Net2W_Init(&w, buf, sizeof(buf));
	memset(&c, 0, sizeof(c));
	c.type = N2K_UNRELIABLE;
	c.channel = 2;
	c.data = data;
	c.size = 5;
	T_ASSERT(Net2Pkt_WriteChunk(&w, &c) && w.pos == 9);
	c.type = N2K_SEQUENCED;
	c.channel = 1;
	c.id = 0xfffe;
	c.size = 0;
	T_ASSERT(Net2Pkt_WriteChunk(&w, &c));
	c.type = N2K_FRAGMENT;
	c.channel = 0;
	c.id = 77;
	c.index = 2;
	c.count = 3;
	c.size = 100;
	T_ASSERT(Net2Pkt_WriteChunk(&w, &c));
	c.type = N2K_FRAGMENT;
	c.index = 0;
	c.size = NET2_FRAG_SIZE;
	T_ASSERT(Net2Pkt_WriteChunk(&w, &c));
	c.type = N2K_DISCONNECT;
	c.reason = NET2_DR_APP + 3;
	T_ASSERT(Net2Pkt_WriteChunk(&w, &c));
	int total = w.pos;
	T_ASSERT(total == 9 + 6 + 108 + 8 + NET2_FRAG_SIZE + 2);
	T_ASSERT(Net2Pkt_CheckChunks(buf, total, 3, kinds));

	Net2R_Init(&r, buf, total);
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == 1 && out.type == N2K_UNRELIABLE && out.channel == 2 && out.size == 5
		&& memcmp(out.data, data, 5) == 0);
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == 1 && out.type == N2K_SEQUENCED && out.id == 0xfffe && out.size == 0);
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == 1 && out.type == N2K_FRAGMENT && out.id == 77 && out.index == 2
		&& out.count == 3 && out.size == 100 && memcmp(out.data, data, 100) == 0);
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == 1 && out.size == NET2_FRAG_SIZE && out.index == 0);
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == 1 && out.type == N2K_DISCONNECT && out.reason == NET2_DR_APP + 3);
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == 0);

	/* every cut of the chunk list is malformed, except at chunk borders */
	for (int n = 0; n < total; n++) {
		int ok = n == 0 || n == 9 || n == 15 || n == 123 || n == 123 + 8 + NET2_FRAG_SIZE;
		T_ASSERT(Net2Pkt_CheckChunks(buf, n, 3, kinds) == ok);
	}
	/* channel out of range, wrong kind for the channel */
	T_ASSERT(!Net2Pkt_CheckChunks(buf, 9, 2, kinds));
	static const uint8_t allRel[3] = { NET2_RELIABLE, NET2_RELIABLE, NET2_RELIABLE };
	T_ASSERT(!Net2Pkt_CheckChunks(buf, 9, 3, allRel));

	/* bad fragment fields: count 1, count 65, index >= count, short middle fragment, empty last fragment */
	static const struct { int index, count, size; } bad[] = {
		{ 0, 1, 10 }, { 0, 65, NET2_FRAG_SIZE }, { 3, 3, 10 }, { 0, 3, 10 }, { 2, 3, 0 },
	};
	for (int i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++) {
		uint8_t b[32];
		Net2W_Init(&w, b, sizeof(b));
		Net2W_U8(&w, N2K_FRAGMENT);
		Net2W_U8(&w, 0);
		Net2W_U16(&w, 1);
		Net2W_U8(&w, (uint32_t)bad[i].index);
		Net2W_U8(&w, (uint32_t)bad[i].count);
		Net2W_U16(&w, (uint32_t)bad[i].size);
		Net2W_Zeros(&w, bad[i].size > 16 ? 16 : bad[i].size);
		Net2R_Init(&r, b, w.pos);
		T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == -1);
	}
	/* a size field larger than the packet, an unknown type, a reliable chunk over a fragment */
	uint8_t b[8] = { N2K_UNRELIABLE, 0, 0xff, 0xff, 1, 2, 3, 4 };
	Net2R_Init(&r, b, sizeof(b));
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == -1);
	b[0] = 9;
	Net2R_Init(&r, b, sizeof(b));
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == -1);
	uint8_t big[8 + NET2_FRAG_SIZE + 1];
	memset(big, 0, sizeof(big));
	big[0] = N2K_RELIABLE;
	big[4] = (uint8_t)(NET2_FRAG_SIZE + 1);
	big[5] = (uint8_t)((NET2_FRAG_SIZE + 1) >> 8);
	Net2R_Init(&r, big, 6 + NET2_FRAG_SIZE + 1);
	T_ASSERT(Net2Pkt_ReadChunk(&r, &out) == -1);
	/* a chunk that does not fit is not written at all */
	Net2W_Init(&w, buf, 10);
	c.type = N2K_UNRELIABLE;
	c.size = 7;
	T_ASSERT(!Net2Pkt_WriteChunk(&w, &c) && w.pos == 0 && !w.overflow);

	Net2Acks a = { 123456, 0x80000001u, 70000 }, a2;
	Net2W_Init(&w, buf, sizeof(buf));
	Net2Pkt_WriteAcks(&w, &a);
	T_ASSERT(w.pos == N2_ACKS);
	Net2R_Init(&r, buf, w.pos);
	T_ASSERT(Net2Pkt_ReadAcks(&r, &a2) && a2.ack == a.ack && a2.bits == a.bits && a2.delay == 0xffff);
	Net2R_Init(&r, buf, N2_ACKS - 1);
	T_ASSERT(!Net2Pkt_ReadAcks(&r, &a2));
}

static void Test_Aead(void) {
	static const char plainText[] = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for "
		"the future, sunscreen would be it.";
	static const char expect[] = "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca967128"
		"2fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945"
		"585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116" "1ae10b594f09e26a7e902ecbd0600691";
	uint8_t key[32], nonce[12], ad[12], ref[130], out[130], plain[130];
	crypto_aead_ctx ctx;

	/* RFC 8439 2.8.2: the Monocypher calls Net2Crypto_Seal makes are the IETF construction */
	for (int i = 0; i < 32; i++) {
		key[i] = (uint8_t)(0x80 + i);
	}
	HexDecode("070000004041424344454647", nonce, 12);
	HexDecode("50515253c0c1c2c3c4c5c6c7", ad, 12);
	T_ASSERT(HexDecode(expect, ref, sizeof(ref)) == 130);
	crypto_aead_init_ietf(&ctx, key, nonce);
	crypto_aead_write(&ctx, out, out + 114, ad, 12, (const uint8_t *)plainText, 114);
	T_ASSERT(memcmp(out, ref, 130) == 0);

	/* Net2's nonce layout: direction, 3 zeros, seq, 4 zeros (vector made with another ChaCha20-Poly1305) */
	uint8_t hdr[N2_HEADER];
	for (int i = 0; i < 32; i++) {
		key[i] = (uint8_t)i;
	}
	Net2Pkt_WriteHeader(hdr, 0x12345678, 0x01020304);
	T_ASSERT(HexDecode("f33b07e162c830ebb37514f6f6a283bc01ee8664bb86edbfc42f72", ref, sizeof(ref)) == 27);
	Net2Crypto_Seal(out, key, 1, 0x01020304, hdr, (const uint8_t *)"net2 packet", 11);
	T_ASSERT(memcmp(out, ref, 27) == 0);
	T_ASSERT(Net2Crypto_Open(plain, key, 1, 0x01020304, hdr, out, 11) && memcmp(plain, "net2 packet", 11) == 0);
	T_ASSERT(!Net2Crypto_Open(plain, key, 0, 0x01020304, hdr, out, 11));
	T_ASSERT(!Net2Crypto_Open(plain, key, 1, 0x01020305, hdr, out, 11));
	for (int i = 0; i < N2_HEADER; i++) {
		hdr[i] ^= 1;
		T_ASSERT(!Net2Crypto_Open(plain, key, 1, 0x01020304, hdr, out, 11));
		hdr[i] ^= 1;
	}
	for (int i = 0; i < 27; i++) {
		out[i] ^= 0x40;
		T_ASSERT(!Net2Crypto_Open(plain, key, 1, 0x01020304, hdr, out, 11));
		out[i] ^= 0x40;
	}
	T_ASSERT(!Net2Crypto_Open(plain, key, 1, 0x01020304, hdr, out, -1));

	/* both sides derive the same keys; the low order point is refused */
	uint8_t ska[32], skb[32], pka[32], pkb[32], sa[32], sb[32], k1[32], k2[32], k3[32], k4[32];
	for (int i = 0; i < 32; i++) {
		ska[i] = (uint8_t)(i * 3 + 1);
		skb[i] = (uint8_t)(i * 5 + 2);
	}
	crypto_x25519_public_key(pka, ska);
	crypto_x25519_public_key(pkb, skb);
	T_ASSERT(Net2Crypto_Shared(sa, ska, pkb) && Net2Crypto_Shared(sb, skb, pka) && memcmp(sa, sb, 32) == 0);
	Net2Crypto_Keys(k1, k2, sa, pka, pkb);
	Net2Crypto_Keys(k3, k4, sb, pka, pkb);
	T_ASSERT(memcmp(k1, k3, 32) == 0 && memcmp(k2, k4, 32) == 0 && memcmp(k1, k2, 32) != 0);
	uint8_t zero[32];
	memset(zero, 0, sizeof(zero));
	T_ASSERT(!Net2Crypto_Shared(sa, ska, zero));
}

static void Test_Cookie(void) {
	uint8_t secret[32], salt[N2_SALT], pk[32], c1[16], c2[16];
	Net2Addr a, b;

	memset(secret, 7, sizeof(secret));
	memset(salt, 1, sizeof(salt));
	memset(pk, 2, sizeof(pk));
	T_ASSERT(Net2_ParseAddr("10.1.2.3:4000", 0, &a));
	b = a;
	Net2Crypto_Cookie(c1, secret, 5, &a, salt, pk);
	Net2Crypto_Cookie(c2, secret, 5, &b, salt, pk);
	T_ASSERT(memcmp(c1, c2, 16) == 0);
	b.port++;
	Net2Crypto_Cookie(c2, secret, 5, &b, salt, pk);
	T_ASSERT(memcmp(c1, c2, 16) != 0);
	Net2Crypto_Cookie(c2, secret, 6, &a, salt, pk);
	T_ASSERT(memcmp(c1, c2, 16) != 0);
	salt[0] ^= 1;
	Net2Crypto_Cookie(c2, secret, 5, &a, salt, pk);
	T_ASSERT(memcmp(c1, c2, 16) != 0);
	salt[0] ^= 1;
	pk[31] ^= 1;
	Net2Crypto_Cookie(c2, secret, 5, &a, salt, pk);
	T_ASSERT(memcmp(c1, c2, 16) != 0);
}

static void Test_Info(void) {
	uint8_t buf[NET2_MTU];
	Net2ServerInfo in, out;
	uint32_t token, time;

	memset(&in, 0, sizeof(in));
	in.appVersion = 0x10203;
	in.version = NET2_VERSION;
	in.players = 12;
	in.maxPlayers = 64;
	in.flags = 3;
	snprintf(in.name, sizeof(in.name), "Sandbox %s", "DownTown");
	memset(in.map, 'M', NET2_INFO_MAP);
	snprintf(in.mode, sizeof(in.mode), "freeroam");
	int n = Net2Lan_WriteInfo(buf, sizeof(buf), 0xdeadbeef, 1234, &in);
	T_ASSERT(n > 0 && n <= N2_QUERY_SIZE);
	T_ASSERT(Net2Lan_ReadInfo(buf, n, &token, &time, &out));
	T_ASSERT(token == 0xdeadbeef && time == 1234 && memcmp(&in, &out, sizeof(in)) == 0);
	for (int i = 0; i < n; i++) {
		T_ASSERT(!Net2Lan_ReadInfo(buf, i, &token, &time, &out));
	}
	/* trailing bytes, a string length over its field, control characters */
	T_ASSERT(!Net2Lan_ReadInfo(buf, n + 1, &token, &time, &out));
	int nameAt = 1 + 4 + 4 + 4 + 2 + 4 + 3;
	buf[nameAt] = NET2_INFO_NAME + 1;
	T_ASSERT(!Net2Lan_ReadInfo(buf, n, &token, &time, &out));
	buf[nameAt] = (uint8_t)strlen(in.name);
	buf[nameAt + 1] = '\n';
	T_ASSERT(Net2Lan_ReadInfo(buf, n, &token, &time, &out) && out.name[0] == '?');
}

static void Test_Addr(void) {
	Net2Addr a;
	char s[64];

	T_ASSERT(Net2_ParseAddr("127.0.0.1", 29000, &a) && a.family == NET2_AF_IPV4 && a.port == 29000
		&& a.ip[0] == 127 && a.ip[3] == 1);
	Net2_AddrString(&a, s, sizeof(s));
	T_ASSERT(strcmp(s, "127.0.0.1:29000") == 0);
	T_ASSERT(Net2_ParseAddr("10.0.0.2:5", 1, &a) && a.port == 5);
	T_ASSERT(Net2_ParseAddr("::1", 29001, &a) && a.family == NET2_AF_IPV6 && a.ip[15] == 1 && a.port == 29001);
	Net2_AddrString(&a, s, sizeof(s));
	T_ASSERT(strcmp(s, "[::1]:29001") == 0);
	T_ASSERT(Net2_ParseAddr("[::1]:29002", 0, &a) && a.port == 29002);
	T_ASSERT(Net2_ParseAddr("[2001:db8::7]", 3, &a) && a.port == 3 && a.ip[0] == 0x20 && a.ip[15] == 7);
	T_ASSERT(!Net2_ParseAddr("", 1, &a));
	T_ASSERT(!Net2_ParseAddr("localhost", 1, &a));	/* no name lookup */
	T_ASSERT(!Net2_ParseAddr("1.2.3.4:0", 1, &a));
	T_ASSERT(!Net2_ParseAddr("1.2.3.4:65536", 1, &a));
	T_ASSERT(!Net2_ParseAddr("1.2.3.4:12x", 1, &a));
	T_ASSERT(!Net2_ParseAddr("[::1", 1, &a));
	T_ASSERT(!Net2_ParseAddr("[::1]x", 1, &a));
	T_ASSERT(!Net2_ParseAddr("1.2.3.400", 1, &a));
	char longName[200];
	memset(longName, '1', sizeof(longName) - 1);
	longName[sizeof(longName) - 1] = 0;
	T_ASSERT(!Net2_ParseAddr(longName, 1, &a));
	Net2Addr b;
	Net2_ParseAddr("10.0.0.2:5", 0, &a);
	Net2_ParseAddr("10.0.0.2:5", 0, &b);
	T_ASSERT(Net2_AddrEqual(&a, &b));
	b.port = 6;
	T_ASSERT(!Net2_AddrEqual(&a, &b));
	T_ASSERT(Net2_LanAddr(NET2_AF_IPV4, 7, &a) && a.ip[0] == 255 && a.port == 7);
	T_ASSERT(Net2_LanAddr(NET2_AF_IPV6, 7, &a) && a.ip[0] == 0xff && a.ip[1] == 0x02);
	T_ASSERT(strcmp(Net2_ReasonString(NET2_DR_TIMEOUT), "timed out") == 0);
	T_ASSERT(sizeof(Net2Addr) == 24);
}

/*
 * The in-memory link: datagrams wait until their delivery time (delay plus jitter, so they reorder), may be lost or
 * doubled. Each host has a port with its address on the link.
 */
#define LINK_MAX 16384
typedef struct LinkPkt {
	Net2Addr from, to;
	uint64_t at;
	int size;
	uint8_t data[NET2_MTU];
} LinkPkt;

typedef struct Link {
	int count;
	uint64_t now;
	int loss, dup;					/* percent */
	int delay, jitter;				/* ms */
	int cut;
	int overflow;
	int capture;					/* copy the next DATA datagram of a client into captured */
	int dropType;					/* drop every datagram of this type (0: none) */
	LinkPkt captured;
} Link;

static LinkPkt g_linkQ[LINK_MAX];

typedef struct Port {
	Link *link;
	Net2Addr addr;
	Net2Io io;
} Port;

static void Link_Push(Link *l, const Net2Addr *from, const Net2Addr *to, const uint8_t *data, int size,
	uint64_t at) {
	if (l->count == LINK_MAX) {
		l->overflow++;
		return;
	}
	LinkPkt *p = &g_linkQ[l->count++];
	p->from = *from;
	p->to = *to;
	p->at = at;
	p->size = size;
	memcpy(p->data, data, (size_t)size);
}

static int Link_Send(void *ctx, const Net2Addr *to, const uint8_t *data, int size) {
	Port *pt = ctx;
	Link *l = pt->link;

	T_ASSERT(size > 0 && size <= NET2_MTU);
	if (l->capture && data[0] == N2P_DATA && size > 40) {
		l->captured.from = pt->addr;
		l->captured.to = *to;
		l->captured.size = size;
		memcpy(l->captured.data, data, (size_t)size);
		l->capture = 0;
	}
	if (l->cut || (l->dropType && data[0] == l->dropType) || (int)(Rand() % 100) < l->loss) {
		return 1;
	}
	int copies = (int)(Rand() % 100) < l->dup ? 2 : 1;
	for (int i = 0; i < copies; i++) {
		uint64_t at = l->now + (uint64_t)l->delay + (l->jitter ? Rand() % (uint32_t)(l->jitter + 1) : 0);
		Link_Push(l, &pt->addr, to, data, size, at);
	}
	return 1;
}

static int Link_Recv(void *ctx, Net2Addr *from, uint8_t *buf, int cap) {
	Port *pt = ctx;
	Link *l = pt->link;

	for (int i = 0; i < l->count; i++) {
		LinkPkt *p = &g_linkQ[i];
		Net2Addr lan;
		Net2_LanAddr(p->to.family, p->to.port, &lan);
		int bcast = Net2_AddrEqual(&p->to, &lan) && p->to.port == pt->addr.port && p->to.family == pt->addr.family;
		if (p->at <= l->now && (bcast || Net2_AddrEqual(&p->to, &pt->addr))) {
			int n = p->size < cap ? p->size : cap;
			memcpy(buf, p->data, (size_t)n);
			*from = p->from;
			memmove(p, p + 1, sizeof(*p) * (size_t)(l->count - i - 1));
			l->count--;
			return n;
		}
	}
	return 0;
}

/* a host on the link and what the tests check of its events */
typedef struct Peer {
	Net2Host *h;
	Port port;
	int connects, disconnects, reason;
	uint32_t relNext[NET2_MAX_CONNS > 64 ? 64 : NET2_MAX_CONNS];	/* next reliable index expected per conn */
	int relBad;
	uint32_t seqLast[64];
	int seqBad, seqCount;
	uint8_t unrelSeen[64][512];
	int unrelDup, unrelCount, unrelBad;
	int infos;
	Net2Event lastInfo;
} Peer;

static Link g_link;

static void Peer_Make(Peer *p, const char *addr, int server, const Net2Config *base) {
	Net2Config cfg;
	int err;

	memset(p, 0, sizeof(*p));
	p->port.link = &g_link;
	T_ASSERT(Net2_ParseAddr(addr, 0, &p->port.addr));
	p->port.io.ctx = &p->port;
	p->port.io.send = Link_Send;
	p->port.io.recv = Link_Recv;
	if (base) {
		cfg = *base;
	} else {
		Net2_DefaultConfig(&cfg, server);
		cfg.maxConns = server ? 8 : 1;
		cfg.appVersion = 7;
	}
	cfg.server = server;
	cfg.io = &p->port.io;
	p->h = Net2_Create(&cfg, &err);
	T_ASSERT(p->h != NULL && err == NET2_OK);
}

static void Peer_Event(Peer *p, const Net2Event *ev) {
	int conn = ev->conn & 63;

	switch (ev->type) {
	case NET2_EV_CONNECT:
		p->connects++;
		p->relNext[conn] = 0;
		p->seqLast[conn] = 0;
		memset(p->unrelSeen[conn], 0, sizeof(p->unrelSeen[conn]));
		break;
	case NET2_EV_DISCONNECT:
		p->disconnects++;
		p->reason = ev->reason;
		break;
	case NET2_EV_SERVERINFO:
		p->infos++;
		p->lastInfo = *ev;
		break;
	case NET2_EV_MESSAGE:
		if (ev->channel == 0) {
			if (!Msg_Check(ev->data, ev->size, p->relNext[conn])) {
				p->relBad++;
			}
			p->relNext[conn]++;
		} else if (ev->channel == 1) {
			uint32_t s = ev->size == 4 ? Msg_U32(ev->data) : 0;
			if (s <= p->seqLast[conn]) {
				p->seqBad++;
			}
			p->seqLast[conn] = s;
			p->seqCount++;
		} else if (ev->channel == 2) {
			uint32_t id = ev->size == 4 ? Msg_U32(ev->data) : 0xffffffffu;
			if (id >= 512 * 8) {
				p->unrelBad++;
			} else if (p->unrelSeen[conn][id >> 3] & (1 << (id & 7))) {
				p->unrelDup++;
			} else {
				p->unrelSeen[conn][id >> 3] |= (uint8_t)(1 << (id & 7));
				p->unrelCount++;
			}
		}
		break;
	}
}

static void Step(Peer **peers, int n, int ms) {
	Net2Event ev;

	for (int i = 0; i < n; i++) {
		Net2_Poll(peers[i]->h, g_link.now);
		while (Net2_Receive(peers[i]->h, &ev)) {
			Peer_Event(peers[i], &ev);
		}
	}
	for (int i = 0; i < n; i++) {
		Net2_Flush(peers[i]->h, g_link.now);
	}
	g_link.now += (uint64_t)ms;
}

static void Link_Reset(int loss, int dup, int delay, int jitter, uint32_t seed) {
	memset(&g_link, 0, sizeof(g_link));
	g_link.now = 1000;
	g_link.loss = loss;
	g_link.dup = dup;
	g_link.delay = delay;
	g_link.jitter = jitter;
	g_rng = seed;
}

static void Test_Handshake(void) {
	Peer srv, cli;
	Net2Addr sa;

	for (uint32_t seed = 1; seed <= 20; seed++) {
		Link_Reset(30, 15, 20, 60, seed * 2654435761u);
		Peer_Make(&srv, "10.0.0.1:29000", 1, NULL);
		Peer_Make(&cli, "10.0.0.2:29001", 0, NULL);
		Peer *all[2] = { &srv, &cli };
		Net2_ParseAddr("10.0.0.1:29000", 0, &sa);
		T_ASSERT(Net2_Connect(cli.h, &sa, g_link.now) == NET2_OK);
		T_ASSERT(Net2_Connect(cli.h, &sa, g_link.now) == NET2_E_STATE);
		for (int i = 0; i < 1000 && (cli.connects == 0 || srv.connects == 0) && cli.disconnects == 0; i++) {
			Step(all, 2, 5);
		}
		T_ASSERT(cli.connects == 1 && srv.connects == 1 && cli.disconnects == 0);
		int live = 0;
		for (int i = 0; i < srv.h->numConns; i++) {
			live += srv.h->conns[i].state != N2C_FREE;
		}
		T_ASSERT(live == 1);
		uint8_t k1[32], k2[32];
		T_ASSERT(Net2_ServerKey(srv.h, k1) && Net2_ServerKey(cli.h, k2) && memcmp(k1, k2, 32) == 0);
		T_ASSERT(Net2_IsConnected(cli.h, 0));
		Net2_Destroy(srv.h);
		Net2_Destroy(cli.h);
	}
}

/* runs until cond or the step limit */
#define RUN_UNTIL(peers, n, cond, limit) do { \
	for (int run_ = 0; run_ < (limit) && !(cond); run_++) { \
		Step(peers, n, 5); \
	} \
} while (0)

static int Connect(Peer *srv, Peer *cli, const char *srvAddr) {
	Net2Addr sa;
	Peer *all[2] = { srv, cli };
	int before = srv->connects, cliBefore = cli->connects, dis = cli->disconnects;

	Net2_ParseAddr(srvAddr, 0, &sa);
	if (Net2_Connect(cli->h, &sa, g_link.now) != NET2_OK) {
		return 0;
	}
	RUN_UNTIL(all, 2, (cli->connects > cliBefore && srv->connects > before) || cli->disconnects > dis, 2000);
	return cli->connects > cliBefore && srv->connects > before;
}

static void Test_Transfer(void) {
	static uint8_t buf[NET2_MAX_MESSAGE];
	Peer srv, cli;

	for (uint32_t seed = 1; seed <= 6; seed++) {
		Link_Reset(seed < 4 ? 20 : 5, 10, 15, 50, seed * 7919u);
		Peer_Make(&srv, "10.0.0.1:29000", 1, NULL);
		Peer_Make(&cli, "10.0.0.2:29001", 0, NULL);
		Peer *all[2] = { &srv, &cli };
		T_ASSERT(Connect(&srv, &cli, "10.0.0.1:29000"));
		int sc = -1;
		for (int i = 0; i < srv.h->numConns; i++) {
			if (Net2_IsConnected(srv.h, i)) {
				sc = i;
			}
		}
		T_ASSERT(sc >= 0);
		uint32_t sentC = 0, sentS = 0, unrel = 0, seq = 0;
		for (int frame = 0; frame < 1500; frame++) {
			/* reliable both ways: small, fragment sized, largest; unreliable and sequenced every frame */
			if (frame < 600 && frame % 3 == 0) {
				static const int sizes[] = { 8, 100, NET2_FRAG_SIZE, NET2_FRAG_SIZE + 1, 5000, NET2_MAX_MESSAGE, 64 };
				int n = Msg_Reliable(buf, sentC, sizes[sentC % 7]);
				if (Net2_Send(cli.h, 0, 0, buf, n) == NET2_OK) {
					sentC++;
				}
				n = Msg_Reliable(buf, sentS, sizes[(sentS + 3) % 7] / 2);
				if (Net2_Send(srv.h, sc, 0, buf, n) == NET2_OK) {
					sentS++;
				}
			}
			if (unrel < 512 * 8) {
				uint8_t u[4] = { (uint8_t)unrel, (uint8_t)(unrel >> 8), 0, 0 };
				T_ASSERT(Net2_Send(cli.h, 0, 2, u, 4) == NET2_OK);
				unrel++;
			}
			seq++;
			uint8_t s[4] = { (uint8_t)seq, (uint8_t)(seq >> 8), (uint8_t)(seq >> 16), 0 };
			T_ASSERT(Net2_Send(srv.h, sc, 1, s, 4) == NET2_OK);
			Step(all, 2, 16);
		}
		RUN_UNTIL(all, 2, srv.relNext[sc] == sentC && cli.relNext[0] == sentS, 4000);
		T_ASSERT(sentC >= 150 && sentS >= 150);
		T_ASSERT(srv.relNext[sc] == sentC && cli.relNext[0] == sentS);
		T_ASSERT(srv.relBad == 0 && cli.relBad == 0);
		T_ASSERT(cli.seqBad == 0 && cli.seqCount > 300);
		T_ASSERT(srv.unrelDup == 0 && srv.unrelBad == 0 && srv.unrelCount > 1000);
		Net2Stats st;
		T_ASSERT(Net2_GetStats(cli.h, 0, &st));
		T_ASSERT(st.reliablePending == 0);
		/* one way 15..65 ms: the round trip is 30..130 plus up to one ack delay */
		T_ASSERT(st.rtt >= 25 && st.rtt <= 170);
		float loss = (float)g_link.loss / 100.0f;
		T_ASSERT(st.loss > loss * 0.3f && st.loss < loss * 1.8f + 0.03f);
		printf("transfer seed %u: rel %u/%u, rtt %u var %u, loss %.3f (link %d%%), sent %llu lost %llu bad %u\n",
			seed, sentC, sentS, st.rtt, st.rttVar, (double)st.loss, g_link.loss,
			(unsigned long long)st.packetsSent, (unsigned long long)st.packetsLost, st.bad);
		Net2_Destroy(srv.h);
		Net2_Destroy(cli.h);
	}
}

static void Test_Disconnect(void) {
	Peer srv, cli;

	/* a reason of the game reaches the server */
	Link_Reset(5, 0, 10, 10, 99);
	Peer_Make(&srv, "10.0.0.1:29000", 1, NULL);
	Peer_Make(&cli, "10.0.0.2:29001", 0, NULL);
	Peer *all[2] = { &srv, &cli };
	T_ASSERT(Connect(&srv, &cli, "10.0.0.1:29000"));
	/* the last reliable message before a disconnect still arrives */
	uint8_t buf[64];
	int n = Msg_Reliable(buf, 0, 40);
	T_ASSERT(Net2_Send(cli.h, 0, 0, buf, n) == NET2_OK);
	RUN_UNTIL(all, 2, srv.relNext[0] == 1, 400);
	Net2_Disconnect(cli.h, 0, NET2_DR_APP + 5);
	T_ASSERT(Net2_Send(cli.h, 0, 0, buf, n) == NET2_E_STATE);
	RUN_UNTIL(all, 2, srv.disconnects && cli.disconnects, 400);
	T_ASSERT(srv.relNext[0] == 1 && srv.relBad == 0);
	T_ASSERT(cli.disconnects == 1 && cli.reason == NET2_DR_APP + 5);
	T_ASSERT(srv.disconnects == 1 && (srv.reason == NET2_DR_APP + 5 || srv.reason == NET2_DR_TIMEOUT));
	T_ASSERT(srv.h->conns[0].state == N2C_FREE && cli.h->conns[0].state == N2C_FREE);
	/* the client can connect again */
	cli.connects = 0;
	T_ASSERT(Connect(&srv, &cli, "10.0.0.1:29000"));

	/* the link breaks: both time out */
	g_link.cut = 1;
	srv.disconnects = cli.disconnects = 0;
	RUN_UNTIL(all, 2, srv.disconnects && cli.disconnects, 4000);
	T_ASSERT(srv.disconnects == 1 && srv.reason == NET2_DR_TIMEOUT);
	T_ASSERT(cli.disconnects == 1 && cli.reason == NET2_DR_TIMEOUT);
	T_ASSERT(g_link.now < 1000 + 30000);

	/* the server shuts down: the client hears it */
	g_link.cut = 0;
	cli.connects = 0;
	T_ASSERT(Connect(&srv, &cli, "10.0.0.1:29000"));
	cli.disconnects = 0;
	Net2_Destroy(srv.h);
	Peer *only[1] = { &cli };
	RUN_UNTIL(only, 1, cli.disconnects, 400);
	T_ASSERT(cli.disconnects == 1 && (cli.reason == NET2_DR_SHUTDOWN || cli.reason == NET2_DR_TIMEOUT));
	Net2_Destroy(cli.h);

	/* nobody answers: connect times out */
	Link_Reset(0, 0, 10, 0, 5);
	Peer_Make(&cli, "10.0.0.2:29001", 0, NULL);
	Net2Addr sa;
	Net2_ParseAddr("10.0.0.9:29000", 0, &sa);
	T_ASSERT(Net2_Connect(cli.h, &sa, g_link.now) == NET2_OK);
	RUN_UNTIL(only, 1, cli.disconnects, 4000);
	T_ASSERT(cli.disconnects == 1 && cli.reason == NET2_DR_TIMEOUT && cli.connects == 0);
	T_ASSERT(g_link.now >= 1000 + 5000 && g_link.now < 1000 + 6000);
	Net2_Destroy(cli.h);
}

static void Test_Refusals(void) {
	Net2Config cfg;
	Peer srv, cli, cli2;

	/* full */
	Link_Reset(0, 0, 5, 0, 11);
	Net2_DefaultConfig(&cfg, 1);
	cfg.maxConns = 1;
	cfg.appVersion = 7;
	Peer_Make(&srv, "10.0.0.1:29000", 1, &cfg);
	Peer_Make(&cli, "10.0.0.2:29001", 0, NULL);
	Peer_Make(&cli2, "10.0.0.3:29002", 0, NULL);
	T_ASSERT(Connect(&srv, &cli, "10.0.0.1:29000"));
	T_ASSERT(!Connect(&srv, &cli2, "10.0.0.1:29000"));
	T_ASSERT(cli2.disconnects == 1 && cli2.reason == NET2_DR_FULL);
	Net2_Destroy(cli2.h);

	/* game version */
	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = 8;
	Peer_Make(&cli2, "10.0.0.3:29002", 0, &cfg);
	T_ASSERT(!Connect(&srv, &cli2, "10.0.0.1:29000"));
	T_ASSERT(cli2.reason == NET2_DR_VERSION);
	Net2_Destroy(cli2.h);

	/* channel layout */
	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = 7;
	cfg.channels[2] = NET2_RELIABLE;
	Peer_Make(&cli2, "10.0.0.3:29002", 0, &cfg);
	Net2_Disconnect(cli.h, 0, 0);
	Peer *two[2] = { &srv, &cli };
	RUN_UNTIL(two, 2, srv.disconnects, 400);
	T_ASSERT(!Connect(&srv, &cli2, "10.0.0.1:29000"));
	T_ASSERT(cli2.reason == NET2_DR_CHANNELS);
	Net2_Destroy(cli2.h);

	/* pinned server key: the right one connects, another one is refused */
	uint8_t key[32];
	Net2_ServerKey(srv.h, key);
	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = 7;
	cfg.serverKey = key;
	Peer_Make(&cli2, "10.0.0.3:29002", 0, &cfg);
	T_ASSERT(Connect(&srv, &cli2, "10.0.0.1:29000"));
	Net2_Destroy(cli2.h);
	key[0] ^= 1;
	Peer_Make(&cli2, "10.0.0.4:29003", 0, &cfg);
	T_ASSERT(!Connect(&srv, &cli2, "10.0.0.1:29000"));
	T_ASSERT(cli2.reason == NET2_DR_KEY);
	Net2_Destroy(cli2.h);

	/* a fixed identity seed gives the same key */
	uint8_t seed[32], k1[32], k2[32];
	memset(seed, 9, sizeof(seed));
	Net2_DefaultConfig(&cfg, 1);
	cfg.identitySeed = seed;
	Peer_Make(&cli2, "10.0.0.5:1", 1, &cfg);
	Net2_ServerKey(cli2.h, k1);
	Net2_Destroy(cli2.h);
	Peer_Make(&cli2, "10.0.0.5:1", 1, &cfg);
	Net2_ServerKey(cli2.h, k2);
	Net2_Destroy(cli2.h);
	T_ASSERT(memcmp(k1, k2, 32) == 0 && seed[0] == 9);

	/* bad configs */
	int err;
	Net2_DefaultConfig(&cfg, 1);
	cfg.numChannels = 0;
	T_ASSERT(Net2_Create(&cfg, &err) == NULL && err == NET2_E_ARG);
	Net2_DefaultConfig(&cfg, 1);
	cfg.maxConns = NET2_MAX_CONNS + 1;
	T_ASSERT(Net2_Create(&cfg, &err) == NULL && err == NET2_E_ARG);
	Net2_DefaultConfig(&cfg, 1);
	cfg.channels[1] = 3;
	T_ASSERT(Net2_Create(&cfg, &err) == NULL && err == NET2_E_ARG);

	Net2_Destroy(srv.h);
	Net2_Destroy(cli.h);
}

/* replayed, forged, spoofed and random datagrams change nothing */
static void Test_Attacks(void) {
	Peer srv, cli;
	uint8_t buf[4096];

	Link_Reset(0, 0, 5, 0, 1234);
	Peer_Make(&srv, "10.0.0.1:29000", 1, NULL);
	Peer_Make(&cli, "10.0.0.2:29001", 0, NULL);
	Peer *all[2] = { &srv, &cli };
	T_ASSERT(Connect(&srv, &cli, "10.0.0.1:29000"));

	/* replay: a captured reliable packet sent again later is dropped, its message comes once */
	g_link.capture = 1;
	int n = Msg_Reliable(buf, 0, 100);
	Net2_Send(cli.h, 0, 0, buf, n);
	RUN_UNTIL(all, 2, srv.relNext[0] == 1, 200);
	T_ASSERT(!g_link.capture && srv.relNext[0] == 1);
	Net2Stats before, after;
	Net2_GetStats(srv.h, 0, &before);
	for (int i = 0; i < 3; i++) {
		Link_Push(&g_link, &g_link.captured.from, &g_link.captured.to, g_link.captured.data, g_link.captured.size,
			g_link.now);
		Step(all, 2, 5);
	}
	/* from another address too: no move of the connection */
	Net2Addr evil;
	Net2_ParseAddr("10.6.6.6:666", 0, &evil);
	Link_Push(&g_link, &evil, &g_link.captured.to, g_link.captured.data, g_link.captured.size, g_link.now);
	Step(all, 2, 5);
	Net2_GetStats(srv.h, 0, &after);
	T_ASSERT(after.bad == before.bad + 4 && srv.relBad == 0 && srv.relNext[0] == 1);
	T_ASSERT(Net2_AddrEqual(&srv.h->conns[0].addr, &cli.port.addr));

	/* a forged packet with the right connection id and a new number: the tag fails */
	memcpy(buf, g_link.captured.data, (size_t)g_link.captured.size);
	buf[5] = 0xf0;
	buf[6] = 0xff;
	buf[g_link.captured.size - 1] ^= 1;
	Link_Push(&g_link, &cli.port.addr, &srv.port.addr, buf, g_link.captured.size, g_link.now);
	Step(all, 2, 5);
	Net2_GetStats(srv.h, 0, &before);
	T_ASSERT(before.bad == after.bad + 1 && srv.h->conns[0].recv.top < 0xfff0);

	/* CONNECT with a made up cookie: no connection */
	uint8_t pkt[N2_HANDSHAKE_SIZE];
	Net2Writer w;
	Net2W_Init(&w, pkt, sizeof(pkt));
	Net2W_U8(&w, N2P_CONNECT);
	Net2W_U32(&w, N2_PROTOCOL);
	Net2W_U16(&w, NET2_VERSION);
	Net2W_U32(&w, 7);
	Net2W_Zeros(&w, N2_SALT);
	Net2W_Zeros(&w, 32);
	Net2W_Zeros(&w, N2_COOKIE);
	Net2W_U8(&w, 3);
	Net2W_Bytes(&w, srv.h->cfg.channels, 3);
	Net2W_Zeros(&w, (int)sizeof(pkt) - w.pos);
	Link_Push(&g_link, &evil, &srv.port.addr, pkt, sizeof(pkt), g_link.now);
	/* a short HELLO gets nothing (no amplification); a full one gets a 25 byte challenge */
	pkt[0] = N2P_HELLO;
	Link_Push(&g_link, &evil, &srv.port.addr, pkt, 100, g_link.now);
	Step(all, 2, 5);
	int live = 0;
	for (int i = 0; i < srv.h->numConns; i++) {
		live += srv.h->conns[i].state != N2C_FREE;
	}
	T_ASSERT(live == 1 && srv.connects == 1);
	int toEvil = 0;
	for (int i = 0; i < g_link.count; i++) {
		toEvil += Net2_AddrEqual(&g_linkQ[i].to, &evil);
	}
	T_ASSERT(toEvil == 0);

	/* random garbage and cut or bit flipped copies of real packets at both hosts */
	for (int i = 0; i < 20000; i++) {
		int size = 1 + (int)(Rand() % NET2_MTU);
		for (int j = 0; j < size; j++) {
			buf[j] = (uint8_t)Rand();
		}
		static const uint8_t types[] = { N2P_HELLO, N2P_CHALLENGE, N2P_CONNECT, N2P_ACCEPT, N2P_DENY, N2P_DATA,
			N2P_QUERY, N2P_INFO };
		if (i % 2) {
			buf[0] = types[Rand() % sizeof(types)];
		}
		if (i % 4 == 1 && size > 5) {
			memcpy(buf + 1, &srv.h->conns[0].id, 4);	/* the right connection id */
		}
		if (i % 8 == 3) {
			memcpy(buf, g_link.captured.data, (size_t)g_link.captured.size);
			size = 1 + (int)(Rand() % (uint32_t)g_link.captured.size);
			buf[Rand() % (uint32_t)size] ^= (uint8_t)(1 << (Rand() % 8));
		}
		Link_Push(&g_link, i % 3 ? &cli.port.addr : &evil, i % 5 ? &srv.port.addr : &cli.port.addr, buf, size,
			g_link.now);
		if (i % 64 == 0) {
			Step(all, 2, 5);
		}
	}
	RUN_UNTIL(all, 2, g_link.count == 0, 100);
	/* the connection still works */
	n = Msg_Reliable(buf, 1, 3000);
	T_ASSERT(Net2_Send(cli.h, 0, 0, buf, n) == NET2_OK);
	RUN_UNTIL(all, 2, srv.relNext[0] == 2, 400);
	T_ASSERT(srv.relNext[0] == 2 && srv.relBad == 0 && cli.disconnects == 0 && srv.disconnects == 0);
	T_ASSERT(Net2_IsConnected(cli.h, 0) && srv.connects == 1);

	/* the client's NAT mapping changes: the server follows the newest packet */
	Net2_ParseAddr("10.0.0.2:40000", 0, &cli.port.addr);
	n = Msg_Reliable(buf, 2, 10);
	Net2_Send(cli.h, 0, 0, buf, n);
	n = Msg_Reliable(buf, 0, 10);
	Net2_Send(srv.h, 0, 0, buf, n);
	RUN_UNTIL(all, 2, srv.relNext[0] == 3 && cli.relNext[0] == 1, 400);
	T_ASSERT(srv.relNext[0] == 3 && cli.relNext[0] == 1);
	T_ASSERT(Net2_AddrEqual(&srv.h->conns[0].addr, &cli.port.addr));
	Net2_Destroy(srv.h);
	Net2_Destroy(cli.h);
}

static void Test_Reassembly(void) {
	static uint8_t msg[NET2_MAX_MESSAGE];
	Net2Config cfg;
	Port port;
	int err;

	memset(&port, 0, sizeof(port));
	port.link = &g_link;
	port.io.ctx = &port;
	port.io.send = Link_Send;
	port.io.recv = Link_Recv;
	Net2_DefaultConfig(&cfg, 0);
	cfg.io = &port.io;
	cfg.maxRecvBuffer = 3 * NET2_FRAG_SIZE;
	Net2Host *h = Net2_Create(&cfg, &err);
	T_ASSERT(h != NULL);
	Net2Conn *c = &h->conns[0];
	T_ASSERT(Net2Conn_Init(h, c));
	for (int i = 0; i < (int)sizeof(msg); i++) {
		msg[i] = (uint8_t)(i ^ (i >> 8));
	}

	/* id 0: 5 fragments in shuffled order with duplicates; id 1: one chunk, comes first */
	Net2Chunk ck;
	memset(&ck, 0, sizeof(ck));
	ck.type = N2K_RELIABLE;
	ck.id = 1;
	ck.count = 1;
	ck.data = msg;
	ck.size = 33;
	T_ASSERT(Net2Rel_Recv(h, c, &ck) == 1);
	T_ASSERT(!Net2Rel_Ready(c, 0));
	int order[] = { 3, 0, 4, 4, 1, 0, 2 };
	int size = 4 * NET2_FRAG_SIZE + 77;
	ck.type = N2K_FRAGMENT;
	ck.id = 0;
	ck.count = 5;
	for (int k = 0; k < 7; k++) {
		ck.index = order[k];
		ck.data = msg + order[k] * NET2_FRAG_SIZE;
		ck.size = order[k] == 4 ? 77 : NET2_FRAG_SIZE;
		T_ASSERT(Net2Rel_Recv(h, c, &ck) == 1);	/* the next id in order always gets its slot */
		T_ASSERT(Net2Rel_Ready(c, 0) == (k == 6));
	}
	int got;
	uint8_t *p = Net2Rel_Take(c, 0, &got);
	T_ASSERT(got == size && memcmp(p, msg, (size_t)size) == 0);
	Net2_Free(h, p);
	T_ASSERT(Net2Rel_Ready(c, 0));
	p = Net2Rel_Take(c, 0, &got);
	T_ASSERT(got == 33 && memcmp(p, msg, 33) == 0);
	Net2_Free(h, p);
	T_ASSERT(!Net2Rel_Ready(c, 0) && c->recvBytes == 0);

	/* an old id again is ignored; an id beyond the window too */
	ck.type = N2K_RELIABLE;
	ck.count = 1;
	ck.index = 0;
	ck.id = 1;
	T_ASSERT(Net2Rel_Recv(h, c, &ck) == 1 && !Net2Rel_Ready(c, 0));
	ck.id = (uint16_t)(2 + N2_WINDOW);
	T_ASSERT(Net2Rel_Recv(h, c, &ck) == 1 && c->recvBytes == 0);

	/* the receive limit: ids ahead of the next one get no slot past maxRecvBuffer */
	ck.type = N2K_FRAGMENT;
	ck.count = 4;
	ck.index = 0;
	ck.size = NET2_FRAG_SIZE;
	ck.id = 3;
	T_ASSERT(Net2Rel_Recv(h, c, &ck) == 0 && c->recvBytes == 0);
	ck.id = 2;
	T_ASSERT(Net2Rel_Recv(h, c, &ck) == 1 && c->recvBytes == 4 * NET2_FRAG_SIZE);
	/* a fragment with another count for the same id is ignored */
	ck.count = 3;
	ck.index = 1;
	T_ASSERT(Net2Rel_Recv(h, c, &ck) == 1 && c->ch[0].rr->slot[2].have == 1);

	/* send side: window of 256 ids and the byte limit */
	c->state = N2C_CONNECTED;
	for (int i = 0; i < N2_WINDOW; i++) {
		T_ASSERT(Net2_Send(h, 0, 0, msg, 10) == NET2_OK);
	}
	T_ASSERT(Net2_Send(h, 0, 0, msg, 10) == NET2_E_FULL);
	for (int i = 0; i < 10; i++) {
		Net2Rel_Acked(h, c, 0, (uint16_t)i, 0);
	}
	T_ASSERT(c->ch[0].rs->base == 10 && Net2_Send(h, 0, 0, msg, 10) == NET2_OK);
	Net2Rel_Acked(h, c, 0, 20, 0);	/* out of order: the base stays */
	T_ASSERT(c->ch[0].rs->base == 10);
	Net2Rel_Acked(h, c, 0, 9, 0);	/* acked before */
	Net2Rel_Acked(h, c, 0, 400, 0);	/* never sent */
	Net2Rel_Acked(h, c, 0, 10, 5);	/* no such fragment */
	T_ASSERT(c->ch[0].rs->base == 10);
	T_ASSERT(Net2_Send(h, 0, 0, msg, NET2_MAX_MESSAGE + 1) == NET2_E_ARG);
	T_ASSERT(Net2_Send(h, 0, 1, msg, NET2_MAX_UNRELIABLE + 1) == NET2_E_ARG);
	T_ASSERT(Net2_Send(h, 0, 1, msg, NET2_MAX_UNRELIABLE) == NET2_OK);
	T_ASSERT(Net2_Send(h, 0, 9, msg, 1) == NET2_E_ARG);
	T_ASSERT(Net2_Send(h, 1, 0, msg, 1) == NET2_E_STATE);
	Net2_Destroy(h);
}

/* a DATA packet from the client with the client's real keys: what a hostile peer after the handshake can send */
static void SendAuthentic(Peer *cli, Peer *srv, const uint8_t *plain, int n) {
	uint8_t pkt[NET2_MTU];
	Net2Conn *c = &cli->h->conns[0];
	uint32_t seq = ++c->sendSeq;

	Net2Pkt_WriteHeader(pkt, c->id, seq);
	Net2Crypto_Seal(pkt + N2_HEADER, c->sendKey, 0, seq, pkt, plain, n);
	Link_Push(&g_link, &cli->port.addr, &srv->port.addr, pkt, N2_HEADER + n + N2_TAG, g_link.now);
}

/* random bytes into the decoders; well formed chunk lists with hostile fields into a live connection */
static void Test_Fuzz(void) {
	static const uint8_t kinds[3] = { NET2_RELIABLE, NET2_SEQUENCED, NET2_UNRELIABLE };
	uint8_t buf[NET2_MTU + 64];
	Net2ServerInfo info;
	uint32_t token, time;
	Net2Reader r;
	Net2Chunk ck;
	int valid = 0;

	g_rng = 4242;
	for (int i = 0; i < 200000; i++) {
		int size = (int)(Rand() % (sizeof(buf) + 1));
		for (int j = 0; j < size; j++) {
			buf[j] = (uint8_t)Rand();
		}
		if (size > 0 && i % 2) {
			buf[0] = (uint8_t)(1 + Rand() % 5);
		}
		if (Net2Pkt_CheckChunks(buf, size, 3, kinds)) {
			valid++;
			Net2R_Init(&r, buf, size);
			while (Net2Pkt_ReadChunk(&r, &ck) > 0) {
				T_ASSERT(ck.size >= 0 && ck.data + ck.size <= buf + size);
			}
		}
		Net2Lan_ReadInfo(buf, size, &token, &time, &info);
		T_ASSERT(info.name[NET2_INFO_NAME] == 0 && memchr(info.name, 0, sizeof(info.name)) != NULL);
	}

	Peer srv, cli;
	Link_Reset(0, 0, 5, 0, 31337);
	Net2Config cfg;
	Net2_DefaultConfig(&cfg, 1);
	cfg.maxConns = 2;
	cfg.appVersion = 7;
	cfg.maxRecvBuffer = 200000;
	Peer_Make(&srv, "10.0.0.1:29000", 1, &cfg);
	Peer_Make(&cli, "10.0.0.2:29001", 0, NULL);
	Peer *all[2] = { &srv, &cli };
	T_ASSERT(Connect(&srv, &cli, "10.0.0.1:29000"));
	static uint8_t src[NET2_FRAG_SIZE];
	memset(src, 0x5a, sizeof(src));
	int sent = 0;
	for (int i = 0; i < 20000 && srv.disconnects == 0; i++) {
		Net2Writer w;
		Net2Acks a = { Rand() % 4 ? cli.h->conns[0].recv.top : Rand(), Rand(), Rand() };
		Net2W_Init(&w, buf, N2_PLAIN_MAX);
		Net2Pkt_WriteAcks(&w, &a);
		for (int k = 0; k < 8; k++) {
			Net2Chunk c;
			memset(&c, 0, sizeof(c));
			c.type = 1 + (int)(Rand() % 4);
			c.channel = c.type == N2K_UNRELIABLE ? 2 : c.type == N2K_SEQUENCED ? 1 : 0;
			c.id = (uint16_t)(Rand() % 4 ? Rand() % 300 : Rand());
			c.count = c.type == N2K_FRAGMENT ? 2 + (int)(Rand() % (NET2_MAX_FRAGS - 1)) : 1;
			c.index = (int)(Rand() % (uint32_t)c.count);
			c.size = c.type == N2K_FRAGMENT ? (c.index < c.count - 1 ? NET2_FRAG_SIZE : 1 + (int)(Rand() % 100))
				: (int)(Rand() % 200);
			c.data = src;
			Net2Pkt_WriteChunk(&w, &c);
		}
		SendAuthentic(&cli, &srv, buf, w.pos);
		sent++;
		Step(all, 2, 1);
		T_ASSERT(srv.h->conns[0].recvBytes <= 200000 + NET2_MAX_MESSAGE);
	}
	T_ASSERT(sent == 20000 && srv.disconnects == 0);
	/* an authentic packet with a malformed chunk list: the server drops the peer with a protocol error */
	uint8_t bad[N2_ACKS + 3];
	memset(bad, 0, sizeof(bad));
	bad[N2_ACKS] = 9;
	SendAuthentic(&cli, &srv, bad, sizeof(bad));
	RUN_UNTIL(all, 2, srv.disconnects && cli.disconnects, 400);
	T_ASSERT(srv.disconnects == 1 && srv.reason == NET2_DR_PROTOCOL);
	T_ASSERT(cli.disconnects == 1 && cli.reason == NET2_DR_PROTOCOL);
	printf("fuzz: %d of 200000 random chunk lists well formed, 20000 hostile packets taken\n", valid);
	Net2_Destroy(srv.h);
	Net2_Destroy(cli.h);
}

static void Test_Query(void) {
	Peer srv, cli;
	Net2ServerInfo info;
	Net2Addr sa;

	Link_Reset(0, 0, 7, 0, 77);
	Peer_Make(&srv, "10.0.0.1:29000", 1, NULL);
	Peer_Make(&cli, "10.0.0.2:29050", 0, NULL);
	Peer *all[2] = { &srv, &cli };
	memset(&info, 0, sizeof(info));
	snprintf(info.name, sizeof(info.name), "LAN test");
	snprintf(info.map, sizeof(info.map), "DownTown");
	snprintf(info.mode, sizeof(info.mode), "sandbox");
	info.players = 3;
	info.maxPlayers = 64;
	Net2_SetServerInfo(srv.h, &info);
	Net2_ParseAddr("10.0.0.1:29000", 0, &sa);
	T_ASSERT(Net2_Query(cli.h, &sa, g_link.now) == NET2_OK);
	RUN_UNTIL(all, 2, cli.infos, 100);
	T_ASSERT(cli.infos == 1 && strcmp(cli.lastInfo.info.name, "LAN test") == 0
		&& strcmp(cli.lastInfo.info.map, "DownTown") == 0 && cli.lastInfo.info.players == 3
		&& cli.lastInfo.info.maxPlayers == 64 && cli.lastInfo.info.appVersion == 7
		&& Net2_AddrEqual(&cli.lastInfo.addr, &sa));
	T_ASSERT(cli.lastInfo.ping >= 14 && cli.lastInfo.ping <= 20);
	/* LAN: a second server on the next port, an IPv6 one; the query goes to all base ports of both families */
	Peer srv2, srv6;
	Peer_Make(&srv2, "10.0.0.7:29001", 1, NULL);
	Peer_Make(&srv6, "[fd00::1]:29002", 1, NULL);
	Peer *lan[4] = { &srv, &cli, &srv2, &srv6 };
	cli.infos = 0;
	T_ASSERT(Net2_LanQuery(cli.h, 29000, g_link.now) == 2 * NET2_LAN_PORTS);
	RUN_UNTIL(lan, 4, cli.infos == 3, 100);
	T_ASSERT(cli.infos == 3);
	Net2_Destroy(srv2.h);
	Net2_Destroy(srv6.h);
	/* an INFO for another browser's token is not taken */
	uint8_t buf[NET2_MTU];
	int n = Net2Lan_WriteInfo(buf, sizeof(buf), cli.h->queryToken + 1, 0, &info);
	Link_Push(&g_link, &sa, &cli.port.addr, buf, n, g_link.now);
	Step(all, 2, 5);
	T_ASSERT(cli.infos == 3);
	Net2_Destroy(srv.h);
	Net2_Destroy(cli.h);
}

/* every CONNECT is lost for longer than a cookie lives: the client gets a new cookie and connects after all */
static void Test_CookieRefresh(void) {
	Peer srv, cli;
	Net2Config cfg;

	Link_Reset(0, 0, 10, 0, 21);
	Peer_Make(&srv, "10.0.0.1:29000", 1, NULL);
	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = 7;
	cfg.connectTimeout = 60000;
	Peer_Make(&cli, "10.0.0.2:29001", 0, &cfg);
	Peer *all[2] = { &srv, &cli };
	Net2Addr sa;
	Net2_ParseAddr("10.0.0.1:29000", 0, &sa);
	T_ASSERT(Net2_Connect(cli.h, &sa, g_link.now) == NET2_OK);
	g_link.dropType = N2P_CONNECT;
	RUN_UNTIL(all, 2, 0, 25000 / 5);
	g_link.dropType = 0;
	RUN_UNTIL(all, 2, cli.connects && srv.connects, 2000);
	T_ASSERT(cli.connects == 1 && srv.connects == 1 && cli.disconnects == 0);
	Net2_Destroy(srv.h);
	Net2_Destroy(cli.h);
}

/* the rate budget holds the send rate near the configured one */
static void Test_Rate(void) {
	static uint8_t buf[NET2_MAX_MESSAGE];
	Peer srv, cli;
	Net2Config cfg;

	Link_Reset(0, 0, 10, 0, 3);
	Net2_DefaultConfig(&cfg, 1);
	cfg.maxConns = 2;
	cfg.appVersion = 7;
	cfg.rate = 40000;
	Peer_Make(&srv, "10.0.0.1:29000", 1, &cfg);
	Peer_Make(&cli, "10.0.0.2:29001", 0, NULL);
	Peer *all[2] = { &srv, &cli };
	T_ASSERT(Connect(&srv, &cli, "10.0.0.1:29000"));
	uint32_t sent = 0;
	uint64_t start = g_link.now;
	Net2Stats st0, st;
	Net2_GetStats(srv.h, 0, &st0);
	while (g_link.now - start < 5000) {
		if (Net2_Send(srv.h, 0, 0, buf, Msg_Reliable(buf, sent, 20000)) == NET2_OK) {
			sent++;
		}
		Step(all, 2, 10);
	}
	Net2_GetStats(srv.h, 0, &st);
	double rate = (double)(st.bytesSent - st0.bytesSent) * 1000.0 / (double)(g_link.now - start);
	printf("rate: %.0f bytes/s for a budget of %u (last second %u), %u messages queued\n", rate, cfg.rate,
		st.sendRate, sent);
	T_ASSERT(rate > cfg.rate * 0.8 && rate < cfg.rate * 1.1);
	T_ASSERT(cli.relBad == 0 && cli.relNext[0] > 5);
	Net2_Destroy(srv.h);
	Net2_Destroy(cli.h);
}

int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	Test_Wire();
	Test_Window();
	Test_Chunks();
	Test_Aead();
	Test_Cookie();
	Test_Info();
	Test_Addr();
	Test_Reassembly();
	Test_Handshake();
	Test_Transfer();
	Test_Disconnect();
	Test_Refusals();
	Test_Attacks();
	Test_Query();
	Test_CookieRefresh();
	Test_Rate();
	Test_Fuzz();
	printf("net2_test: %d checks, %d failed\n", g_checks, g_fails);
	return g_fails != 0;
}
