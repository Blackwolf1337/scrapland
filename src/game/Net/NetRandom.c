/*
 * The random numbers of the packet nonces (Net/, between msgman.cpp and NetData.cpp, no CHECK in it): MT19937-64
 * with a shuffle table of 256 outputs, one instance g_NetRandom seeded from GetTickCount64 through ChaCha20.
 */
#include <string.h>

#include "scrap/net.h"
#include "scrap/win32.h"

#define NETRAND_MM 156
#define NETRAND_UM 0xffffffff80000000ull
#define NETRAND_LM 0x7fffffffull
#define g_NetRandMag01 (GAME_ARRAY(const uint64_t, g_NetRandMag01, 0x428d38))	/* 0, 0xb5026f5aa96619e9 */

/* the next output of the twister */
static uint64_t NetRandom_Gen(cNetRandom *self) {
	uint64_t *mt = self->mt;
	uint64_t x;
	int i;

	if (self->mti >= NETRAND_NN) {
		if (self->mti == NETRAND_NN + 1) {
			cNetRandom_Seed(self, 5489);
		}
		for (i = 0; i < NETRAND_NN - NETRAND_MM; i++) {
			x = (mt[i] & NETRAND_UM) | (mt[i + 1] & NETRAND_LM);
			mt[i] = mt[i + NETRAND_MM] ^ (x >> 1) ^ g_NetRandMag01[x & 1];
		}
		for (; i < NETRAND_NN - 1; i++) {
			x = (mt[i] & NETRAND_UM) | (mt[i + 1] & NETRAND_LM);
			mt[i] = mt[i + (NETRAND_MM - NETRAND_NN)] ^ (x >> 1) ^ g_NetRandMag01[x & 1];
		}
		x = (mt[NETRAND_NN - 1] & NETRAND_UM) | (mt[0] & NETRAND_LM);
		mt[NETRAND_NN - 1] = mt[NETRAND_MM - 1] ^ (x >> 1) ^ g_NetRandMag01[x & 1];
		self->mti = 0;
	}
	x = mt[self->mti++];
	x ^= (x >> 29) & 0x5555555555555555ull;
	x ^= (x << 17) & 0x71d67fffeda60000ull;
	x ^= (x << 37) & 0xfff7eee000000000ull;
	x ^= x >> 43;
	return x;
}

/* y into the table, an older output out of it */
static uint64_t NetRandom_Shuffle(cNetRandom *self, uint64_t y) {
	self->i = (self->i + 1) & 0xff;
	uint32_t a = (uint32_t)self->table[self->i];
	self->j = (int)((a + (uint32_t)self->j) & 0xff);
	byte k = (byte)((byte)self->table[self->j] + (byte)a);
	uint64_t r = self->table[k];
	self->table[k] = y;
	return r;
}

cNetRandom *__fastcall cNetRandom_ctor(cNetRandom *self) {
	self->mti = NETRAND_NN + 1;
	return self;
}

void __thiscall cNetRandom_InitByArray(cNetRandom *self, const uint64_t *key, uint32_t len) {
	uint64_t *mt = self->mt;
	uint32_t i = 1, j = 0, k;

	cNetRandom_Seed(self, 19650218);
	for (k = len < NETRAND_NN ? NETRAND_NN : len; k; k--) {
		mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 62)) * 3935559000370003845ull)) + key[j] + j;
		i++;
		j++;
		if (i >= NETRAND_NN) {
			mt[0] = mt[NETRAND_NN - 1];
			i = 1;
		}
		if (j >= len) {
			j = 0;
		}
	}
	for (k = NETRAND_NN - 1; k; k--) {
		mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 62)) * 2862933555777941757ull)) - i;
		i++;
		if (i >= NETRAND_NN) {
			mt[0] = mt[NETRAND_NN - 1];
			i = 1;
		}
	}
	mt[0] = 1ull << 63;
	cNetRandom_Fill(self);
}

void __thiscall cNetRandom_Seed(cNetRandom *self, uint64_t seed) {
	self->mt[0] = seed;
	for (self->mti = 1; self->mti < NETRAND_NN; self->mti++) {
		int i = self->mti;
		self->mt[i] = 6364136223846793005ull * (self->mt[i - 1] ^ (self->mt[i - 1] >> 62)) + (uint64_t)(int64_t)i;
	}
	cNetRandom_Fill(self);
}

uint64_t __fastcall cNetRandom_Next(cNetRandom *self) {
	return NetRandom_Shuffle(self, NetRandom_Gen(self));
}

void __fastcall cNetRandom_Fill(cNetRandom *self) {
	for (int n = 0; n < 0x100; n++) {
		self->table[n] = NetRandom_Shuffle(self, NetRandom_Gen(self));
	}
	self->i = 0;
	self->j = 0;
}

cNetRandom *__fastcall cNetRandom_ctorSeeded(cNetRandom *self) {
	uint64_t buf[NETRAND_NN];
	byte key[32], nonce[12];
	cChaCha ctx;

	cNetRandom_ctor(self);
	cChaCha_ctor(&ctx);
	cNetRandom_Seed(self, exe_GetTickCount64());
	for (uint32_t n = 0; n < NETRAND_NN; n++) {
		buf[n] = cNetRandom_Next(self);
	}
	memcpy(key, &buf[4], sizeof(key));
	memcpy(nonce, &buf[24], 12);
	cChaCha_Crypt(&ctx, key, 1, nonce, (const byte *)buf, (byte *)buf, sizeof(buf));
	cNetRandom_InitByArray(self, buf, NETRAND_NN);
	return self;
}
