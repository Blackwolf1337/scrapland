/*
 * The packet crypto of Net/transport.cpp (the code at rva 0x1898d0..0x18a3f0 and Poly1305 at 0x18fa90, no CHECK in
 * them): cNetBuffer, a growing byte array, ChaCha20 (RFC 7539 block layout, 96 bit nonce) and Poly1305
 * (poly1305-donna, the 32 bit one-shot version). See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/net.h"

static inline uint32_t LoadLE32(const byte *p) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void StoreLE32(byte *p, uint32_t v) {
	p[0] = (byte)v;
	p[1] = (byte)(v >> 8);
	p[2] = (byte)(v >> 16);
	p[3] = (byte)(v >> 24);
}

static inline uint32_t Rotl32(uint32_t v, int n) {
	return v >> (-n & 31) | v << (n & 31);
}

/* room for n more bytes: the capacity grows by whole multiples of itself */
static void cNetBuffer_Reserve(cNetBuffer *self, uint32_t cap);
static void NetBuffer_Grow(cNetBuffer *self, uint32_t n) {
	if (self->size + n > self->cap) {
		cNetBuffer_Reserve(self, ((self->size + n) / self->cap + 1) * self->cap);
	}
}

static void NetBuffer_Copy(cNetBuffer *self, const byte *src, uint32_t n) {
	NetBuffer_Grow(self, n);
	for (uint32_t i = 0; i < n; i++) {
		self->data[self->size] = src[i];
		self->size++;
	}
}

cNetBuffer *cNetBuffer_ctor(cNetBuffer *self, uint32_t cap) {
	self->size = 0;
	self->cap = 0;
	self->data = NULL;
	cNetBuffer_Reserve(self, cap);
	return self;
}

void cNetBuffer_dtor(cNetBuffer *self) {
	if (self->data) {
		memset(self->data, 0, self->cap);
	}
	Mem_Free(self->data);
	self->data = NULL;
	self->size = 0;
	self->cap = 0;
}

byte *cNetBuffer_Data(cNetBuffer *self) {
	return self->data;
}

uint32_t cNetBuffer_Size(cNetBuffer *self) {
	return self->size;
}

int cNetBuffer_Append(cNetBuffer *self, const void *src, uint32_t n) {
	NetBuffer_Copy(self, src, n);
	return 1;
}

int cNetBuffer_WriteU64(cNetBuffer *self, uint64_t v) {
	NetBuffer_Grow(self, 8);
	for (int i = 0; i < 8; i++) {
		self->data[self->size++] = (byte)(v >> (i * 8));
	}
	return 1;
}

int cNetBuffer_AppendPad16(cNetBuffer *self, const void *src, uint32_t n) {
	static const byte zero[16];

	NetBuffer_Copy(self, src, n);
	if (n & 0xf) {
		cNetBuffer_Append(self, zero, 0x10 - (n & 0xf));
	}
	return 1;
}

static void cNetBuffer_Reserve(cNetBuffer *self, uint32_t cap) {
	byte *p = Mem_Alloc(cap);

	if (!p) {
		/* the original spins here when the allocation fails */
		for (;;) {
		}
	}
	memset(p, 0, cap);
	if (self->data && self->size) {
		for (uint32_t i = 0; i < self->size; i++) {
			p[i] = self->data[i];
		}
		Mem_Free(self->data);
	}
	self->data = p;
	self->cap = cap;
}

cChaCha *cChaCha_ctor(cChaCha *self) {
	return self;
}

static void cChaCha_Init(cChaCha *self, const byte *key, uint32_t counter, const byte *nonce) {
	uint32_t *s = self->state;

	s[0] = 0x61707865;
	s[1] = 0x3320646e;
	s[2] = 0x79622d32;
	s[3] = 0x6b206574;
	for (int i = 0; i < 8; i++) {
		s[4 + i] = LoadLE32(key + i * 4);
	}
	s[12] = counter;
	for (int i = 0; i < 3; i++) {
		s[13 + i] = LoadLE32(nonce + i * 4);
	}
}

/* the quarter round in the order of the original (a register keeps each new value) */
static inline void ChaCha_QR(uint32_t *x, int a, int b, int c, int d) {
	uint32_t t;

	x[a] += x[b];
	t = Rotl32(x[d] ^ x[a], 16);
	x[d] = t;
	x[c] += t;
	t = Rotl32(x[b] ^ x[c], 12);
	x[b] = t;
	x[a] += t;
	t = Rotl32(x[d] ^ x[a], 8);
	x[d] = t;
	x[c] += t;
	x[b] = Rotl32(x[b] ^ x[c], 7);
}

/* one block of rounds (20) of the state into out (64 bytes); self is not used */
static void cChaCha_Block(cChaCha *self, const uint32_t *state, byte *out, int rounds) {
	uint32_t x[16];
	(void)self;

	memcpy(x, state, sizeof(x));
	if (rounds > 0) {
		for (uint32_t n = (uint32_t)(rounds - 1) / 2 + 1; n; n--) {
			ChaCha_QR(x, 0, 4, 8, 12);
			ChaCha_QR(x, 1, 5, 9, 13);
			ChaCha_QR(x, 2, 6, 10, 14);
			ChaCha_QR(x, 3, 7, 11, 15);
			ChaCha_QR(x, 0, 5, 10, 15);
			ChaCha_QR(x, 1, 6, 11, 12);
			ChaCha_QR(x, 2, 7, 8, 13);
			ChaCha_QR(x, 3, 4, 9, 14);
		}
	}
	for (int i = 0; i < 16; i++) {
		x[i] += state[i];
	}
	for (int i = 0; i < 16; i++) {
		StoreLE32(out + i * 4, x[i]);
	}
}

void cChaCha_Crypt(cChaCha *self, const byte *key, uint32_t counter, const byte *nonce, const byte *in,
	byte *out, uint32_t len) {
	byte ks[0x40];

	cChaCha_Init(self, key, counter, nonce);
	for (uint32_t i = 0; i < len;) {
		cChaCha_Block(self, self->state, ks, 20);
		self->state[12]++;
		uint32_t end = i + 0x40;
		for (uint32_t k = 0; i < end && i < len; i++, k++) {
			out[i] = in[i] ^ ks[k];
		}
		i = end;
	}
}

void Poly1305_Auth(byte *mac, const byte *m, uint32_t bytes, const byte *key) {
	uint32_t t0, t1, t2, t3;
	uint32_t h0, h1, h2, h3, h4;
	uint32_t r0, r1, r2, r3, r4;
	uint32_t s1, s2, s3, s4;
	uint32_t b, nb, g0, g1, g2, g3, g4;
	uint64_t t[5], f0, f1, f2, f3;
	byte mp[16];
	uint32_t j;

	t0 = LoadLE32(key + 0);
	t1 = LoadLE32(key + 4);
	t2 = LoadLE32(key + 8);
	t3 = LoadLE32(key + 12);
	r0 = t0 & 0x3ffffff;
	t0 >>= 26;
	t0 |= t1 << 6;
	r1 = t0 & 0x3ffff03;
	t1 >>= 20;
	t1 |= t2 << 12;
	r2 = t1 & 0x3ffc0ff;
	t2 >>= 14;
	t2 |= t3 << 18;
	r3 = t2 & 0x3f03fff;
	t3 >>= 8;
	r4 = t3 & 0x00fffff;
	s1 = r1 * 5;
	s2 = r2 * 5;
	s3 = r3 * 5;
	s4 = r4 * 5;
	h0 = h1 = h2 = h3 = h4 = 0;
	if (bytes < 16) {
		goto atmost15;
	}

blocks16:
	t0 = LoadLE32(m);
	t1 = LoadLE32(m + 4);
	t2 = LoadLE32(m + 8);
	t3 = LoadLE32(m + 12);
	m += 16;
	bytes -= 16;
	h0 += t0 & 0x3ffffff;
	h1 += (uint32_t)((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
	h2 += (uint32_t)((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
	h3 += (uint32_t)((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
	h4 += (t3 >> 8) | (1 << 24);

mul:
	t[0] = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
	t[1] = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
	t[2] = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
	t[3] = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
	t[4] = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;
	h0 = (uint32_t)t[0] & 0x3ffffff;
	t[1] += t[0] >> 26;
	h1 = (uint32_t)t[1] & 0x3ffffff;
	b = (uint32_t)(t[1] >> 26);
	t[2] += b;
	h2 = (uint32_t)t[2] & 0x3ffffff;
	b = (uint32_t)(t[2] >> 26);
	t[3] += b;
	h3 = (uint32_t)t[3] & 0x3ffffff;
	b = (uint32_t)(t[3] >> 26);
	t[4] += b;
	h4 = (uint32_t)t[4] & 0x3ffffff;
	b = (uint32_t)(t[4] >> 26);
	h0 += b * 5;
	if (bytes >= 16) {
		goto blocks16;
	}

atmost15:
	if (!bytes) {
		goto finish;
	}
	for (j = 0; j < bytes; j++) {
		mp[j] = m[j];
	}
	mp[j++] = 1;
	for (; j < 16; j++) {
		mp[j] = 0;
	}
	bytes = 0;
	t0 = LoadLE32(mp + 0);
	t1 = LoadLE32(mp + 4);
	t2 = LoadLE32(mp + 8);
	t3 = LoadLE32(mp + 12);
	h0 += t0 & 0x3ffffff;
	h1 += (uint32_t)((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
	h2 += (uint32_t)((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
	h3 += (uint32_t)((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
	h4 += t3 >> 8;
	goto mul;

finish:
	b = h0 >> 26;
	h0 &= 0x3ffffff;
	h1 += b;
	b = h1 >> 26;
	h1 &= 0x3ffffff;
	h2 += b;
	b = h2 >> 26;
	h2 &= 0x3ffffff;
	h3 += b;
	b = h3 >> 26;
	h3 &= 0x3ffffff;
	h4 += b;
	b = h4 >> 26;
	h4 &= 0x3ffffff;
	h0 += b * 5;
	b = h0 >> 26;
	h0 &= 0x3ffffff;
	h1 += b;

	g0 = h0 + 5;
	b = g0 >> 26;
	g0 &= 0x3ffffff;
	g1 = h1 + b;
	b = g1 >> 26;
	g1 &= 0x3ffffff;
	g2 = h2 + b;
	b = g2 >> 26;
	g2 &= 0x3ffffff;
	g3 = h3 + b;
	b = g3 >> 26;
	g3 &= 0x3ffffff;
	g4 = h4 + b - (1 << 26);

	b = (g4 >> 31) - 1;
	nb = ~b;
	h0 = (h0 & nb) | (g0 & b);
	h1 = (h1 & nb) | (g1 & b);
	h2 = (h2 & nb) | (g2 & b);
	h3 = (h3 & nb) | (g3 & b);
	h4 = (h4 & nb) | (g4 & b);

	f0 = ((h0) | (h1 << 26)) + (uint64_t)LoadLE32(key + 16);
	f1 = ((h1 >> 6) | (h2 << 20)) + (uint64_t)LoadLE32(key + 20);
	f2 = ((h2 >> 12) | (h3 << 14)) + (uint64_t)LoadLE32(key + 24);
	f3 = ((h3 >> 18) | (h4 << 8)) + (uint64_t)LoadLE32(key + 28);
	StoreLE32(mac + 0, (uint32_t)f0);
	f1 += f0 >> 32;
	StoreLE32(mac + 4, (uint32_t)f1);
	f2 += f1 >> 32;
	StoreLE32(mac + 8, (uint32_t)f2);
	f3 += f2 >> 32;
	StoreLE32(mac + 12, (uint32_t)f3);
}
