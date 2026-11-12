/*
 * Net2 crypto on Monocypher: the AEAD of a DATA packet, the session keys of the handshake, the server cookie and
 * the transcript the server signs.
 */
#include "monocypher.h"
#include "net2local.h"

/* RFC 8439 nonce: direction byte, three zeros, the packet number u32, four zeros */
static void Net2Crypto_Nonce(uint8_t nonce[12], int toClient, uint32_t seq) {
	memset(nonce, 0, 12);
	nonce[0] = (uint8_t)(toClient != 0);
	nonce[4] = (uint8_t)seq;
	nonce[5] = (uint8_t)(seq >> 8);
	nonce[6] = (uint8_t)(seq >> 16);
	nonce[7] = (uint8_t)(seq >> 24);
}

/* crypto_aead_init_ietf and one crypto_aead_write are ChaCha20-Poly1305 of RFC 8439 */
void Net2Crypto_Seal(uint8_t *out, const uint8_t key[32], int toClient, uint32_t seq, const uint8_t *hdr,
	const uint8_t *plain, int size) {
	uint8_t nonce[12];
	crypto_aead_ctx ctx;

	Net2Crypto_Nonce(nonce, toClient, seq);
	crypto_aead_init_ietf(&ctx, key, nonce);
	crypto_aead_write(&ctx, out, out + size, hdr, N2_HEADER, plain, (size_t)size);
	crypto_wipe(&ctx, sizeof(ctx));
}

int Net2Crypto_Open(uint8_t *plain, const uint8_t key[32], int toClient, uint32_t seq, const uint8_t *hdr,
	const uint8_t *cipher, int size) {
	uint8_t nonce[12];
	crypto_aead_ctx ctx;

	if (size < 0) {
		return 0;
	}
	Net2Crypto_Nonce(nonce, toClient, seq);
	crypto_aead_init_ietf(&ctx, key, nonce);
	int bad = crypto_aead_read(&ctx, plain, cipher + size, hdr, N2_HEADER, cipher, (size_t)size);
	crypto_wipe(&ctx, sizeof(ctx));
	return bad == 0;
}

int Net2Crypto_Shared(uint8_t shared[32], const uint8_t sk[32], const uint8_t pk[32]) {
	static const uint8_t zero[32];

	crypto_x25519(shared, sk, pk);
	return crypto_verify32(shared, zero) != 0;
}

/* BLAKE2b-512 keyed with the raw X25519 secret over a label and both public keys: c2s | s2c */
void Net2Crypto_Keys(uint8_t c2s[32], uint8_t s2c[32], const uint8_t shared[32], const uint8_t clientPk[32],
	const uint8_t serverPk[32]) {
	static const char label[] = "scrap net2 keys";
	crypto_blake2b_ctx ctx;
	uint8_t out[64];

	crypto_blake2b_keyed_init(&ctx, 64, shared, 32);
	crypto_blake2b_update(&ctx, (const uint8_t *)label, sizeof(label) - 1);
	crypto_blake2b_update(&ctx, clientPk, 32);
	crypto_blake2b_update(&ctx, serverPk, 32);
	crypto_blake2b_final(&ctx, out);
	memcpy(c2s, out, 32);
	memcpy(s2c, out + 32, 32);
	crypto_wipe(out, sizeof(out));
}

void Net2Crypto_Cookie(uint8_t cookie[N2_COOKIE], const uint8_t secret[32], uint32_t period, const Net2Addr *a,
	const uint8_t salt[N2_SALT], const uint8_t clientPk[32]) {
	crypto_blake2b_ctx ctx;
	uint8_t pb[4] = { (uint8_t)period, (uint8_t)(period >> 8), (uint8_t)(period >> 16), (uint8_t)(period >> 24) };
	uint8_t ak[20];

	Net2_AddrKey(a, ak);
	crypto_blake2b_keyed_init(&ctx, N2_COOKIE, secret, 32);
	crypto_blake2b_update(&ctx, pb, sizeof(pb));
	crypto_blake2b_update(&ctx, ak, sizeof(ak));
	crypto_blake2b_update(&ctx, salt, N2_SALT);
	crypto_blake2b_update(&ctx, clientPk, 32);
	crypto_blake2b_final(&ctx, cookie);
}

void Net2Crypto_Transcript(uint8_t hash[32], const uint8_t salt[N2_SALT], uint32_t connId,
	const uint8_t clientPk[32], const uint8_t serverPk[32]) {
	static const char label[] = "scrap net2 accept";
	crypto_blake2b_ctx ctx;
	uint8_t id[4] = { (uint8_t)connId, (uint8_t)(connId >> 8), (uint8_t)(connId >> 16), (uint8_t)(connId >> 24) };

	crypto_blake2b_init(&ctx, 32);
	crypto_blake2b_update(&ctx, (const uint8_t *)label, sizeof(label) - 1);
	crypto_blake2b_update(&ctx, salt, N2_SALT);
	crypto_blake2b_update(&ctx, id, sizeof(id));
	crypto_blake2b_update(&ctx, clientPk, 32);
	crypto_blake2b_update(&ctx, serverPk, 32);
	crypto_blake2b_final(&ctx, hash);
}

void Net2Crypto_Wipe(void *p, size_t size) {
	crypto_wipe(p, size);
}
