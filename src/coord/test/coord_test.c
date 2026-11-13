/*
 * Unit tests of the pure parts of the coordinator (include/scrap/coord.h): the state text, the tickets (signature,
 * times, malformed ones), the player store (round trip, the fall back to the file before, corrupt files), the list
 * server's heartbeats, registry and pages, and the quoting of arguments for the game's parser.
 *
 *   coord_test [folder]      the store files go to folder (default coord_test_data in the current folder)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/cmdquote.h"
#include "scrap/coord.h"
#include "scrap/coordlist.h"
#include "scrap/placedef.h"

static int checks, failed;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void Test_Kv(void) {
	char text[256], out[128], key[64];
	int len = 0;

	CHECK(CoordKv_KeyOk("money"));
	CHECK(CoordKv_KeyOk("var.Hangar.HangarShip1"));
	CHECK(!CoordKv_KeyOk(""));
	CHECK(!CoordKv_KeyOk("a=b"));
	CHECK(!CoordKv_KeyOk("a b"));
	CHECK(CoordKv_Set(text, &len, sizeof(text), "money", "2000"));
	CHECK(CoordKv_Set(text, &len, sizeof(text), "ship", "SPOLI1<-MPoli1<-<-<-<-1,1<-100<-0,0"));
	CHECK(CoordKv_Set(text, &len, sizeof(text), "note", "two\nlines \\ and a backslash"));
	CHECK(len == (int)strlen("money=2000\nship=SPOLI1<-MPoli1<-<-<-<-1,1<-100<-0,0\nnote=two\\nlines \\\\ and a backslash\n"));
	CHECK(CoordKv_Get(text, len, "money", out, sizeof(out)) && !strcmp(out, "2000"));
	CHECK(CoordKv_Get(text, len, "note", out, sizeof(out)) && !strcmp(out, "two\nlines \\ and a backslash"));
	CHECK(!CoordKv_Get(text, len, "mone", out, sizeof(out)) && !out[0]);
	CHECK(!CoordKv_Get(text, len, "moneyx", out, sizeof(out)));
	/* replace with a longer and a shorter value */
	CHECK(CoordKv_Set(text, &len, sizeof(text), "money", "123456789"));
	CHECK(CoordKv_Get(text, len, "money", out, sizeof(out)) && !strcmp(out, "123456789"));
	CHECK(CoordKv_Get(text, len, "ship", out, sizeof(out)) && !strncmp(out, "SPOLI1", 6));
	CHECK(CoordKv_Set(text, &len, sizeof(text), "money", "5"));
	CHECK(CoordKv_Get(text, len, "money", out, sizeof(out)) && !strcmp(out, "5"));
	CHECK(CoordKv_Valid(text, len));
	/* no room: unchanged */
	char small[16];
	int sl = 0;
	CHECK(CoordKv_Set(small, &sl, sizeof(small), "a", "1234567890"));
	CHECK(!CoordKv_Set(small, &sl, sizeof(small), "b", "x"));
	CHECK(sl == 13);
	CHECK(!CoordKv_Set(small, &sl, sizeof(small), "a", "12345678901234"));
	CHECK(CoordKv_Get(small, sl, "a", out, sizeof(out)) && !strcmp(out, "1234567890"));
	/* a cut value */
	CHECK(CoordKv_Get(text, len, "ship", out, 7) && !strcmp(out, "SPOLI1"));
	/* iteration and merge */
	int pos = 0, n = 0;
	while (CoordKv_Next(text, len, &pos, key, sizeof(key), out, sizeof(out))) {
		n++;
	}
	CHECK(n == 3);
	char other[128];
	int ol = 0;
	CoordKv_Set(other, &ol, sizeof(other), "money", "77");
	CoordKv_Set(other, &ol, sizeof(other), "alarm", "0.5");
	CHECK(CoordKv_Merge(text, &len, sizeof(text), other, ol));
	CHECK(CoordKv_Get(text, len, "money", out, sizeof(out)) && !strcmp(out, "77"));
	CHECK(CoordKv_Get(text, len, "alarm", out, sizeof(out)) && !strcmp(out, "0.5"));
	CHECK(CoordKv_Get(text, len, "note", out, sizeof(out)));
	/* malformed lines: skipped by Next, refused by Valid */
	const char *bad = "money=1\nno equals sign\n=x\nk y=2\nok=3\n";
	CHECK(!CoordKv_Valid(bad, (int)strlen(bad)));
	pos = n = 0;
	while (CoordKv_Next(bad, (int)strlen(bad), &pos, key, sizeof(key), out, sizeof(out))) {
		n++;
	}
	CHECK(n == 2);
	CHECK(CoordKv_Get(bad, (int)strlen(bad), "ok", out, sizeof(out)) && !strcmp(out, "3"));
	CHECK(!CoordKv_Valid("a=1\r\n", 5));
	CHECK(CoordKv_Valid("", 0));
	/* a text without its last newline gets one before a new pair */
	char nn[32] = "a=1";
	int nl = 3;
	CHECK(CoordKv_Set(nn, &nl, sizeof(nn), "b", "2") && nl == 8 && !memcmp(nn, "a=1\nb=2\n", 8));
}

static void Test_Ticket(void) {
	static CoordTicket t, o;
	static uint8_t buf[COORD_TICKET_MAX];
	uint8_t seed[32], pub[32], seed2[32], pub2[32];
	uint64_t now = 1763000000000ull;

	for (int i = 0; i < 32; i++) {
		seed[i] = (uint8_t)(i * 7 + 1);
		seed2[i] = (uint8_t)(i * 5 + 3);
	}
	Net2_KeyPublic(seed, pub);
	Net2_KeyPublic(seed2, pub2);
	memset(&t, 0, sizeof(t));
	for (int i = 0; i < 32; i++) {
		t.key[i] = (uint8_t)(200 - i);
	}
	strcpy(t.level, "TownHall");
	strcpy(t.entrance, "DM_Player_Spawn_DownTown");
	strcpy(t.door, "DoorElevatorDownTown");
	t.issued = now;
	t.expires = now + COORD_TICKET_LIFE;
	memset(t.nonce, 0x5a, sizeof(t.nonce));
	t.stateLen = snprintf(t.state, sizeof(t.state), "money=3100\nship=SBERTO1<-<-<-<-<-1<-80<-0,0,0,0,0,0\n");
	int n = Coord_TicketSeal(&t, seed, buf, sizeof(buf));
	CHECK(n > NET2_SIG_SIZE);
	CHECK(Coord_TicketOpen(buf, n, pub, now + 1000, &o) == COORD_TK_OK);
	CHECK(!memcmp(o.key, t.key, 32) && !strcmp(o.level, "TownHall") && !strcmp(o.entrance, t.entrance) &&
		!strcmp(o.door, t.door) && o.issued == t.issued && o.expires == t.expires && !memcmp(o.nonce, t.nonce, 16) &&
		o.stateLen == t.stateLen && !strcmp(o.state, t.state));
	/* times */
	CHECK(Coord_TicketOpen(buf, n, pub, t.expires, &o) == COORD_TK_OK);
	CHECK(Coord_TicketOpen(buf, n, pub, t.expires + 1, &o) == COORD_TK_EXPIRED);
	CHECK(Coord_TicketOpen(buf, n, pub, now - COORD_SKEW, &o) == COORD_TK_OK);
	CHECK(Coord_TicketOpen(buf, n, pub, now - COORD_SKEW - 1, &o) == COORD_TK_EARLY);
	/* another coordinator's key */
	CHECK(Coord_TicketOpen(buf, n, pub2, now, &o) == COORD_TK_SIGNATURE);
	/* every byte changed: refused (malformed or the signature) */
	int all = 1;
	for (int i = 0; i < n; i++) {
		buf[i] ^= 0x20;
		int r = Coord_TicketOpen(buf, n, pub, now, &o);
		if (r == COORD_TK_OK) {
			all = 0;
			printf("byte %d changed was taken\n", i);
		}
		buf[i] ^= 0x20;
	}
	CHECK(all);
	/* every shorter size and one longer */
	all = 1;
	for (int k = 0; k < n; k++) {
		if (Coord_TicketOpen(buf, k, pub, now, &o) == COORD_TK_OK) {
			all = 0;
		}
	}
	CHECK(all);
	buf[n] = 0;
	CHECK(Coord_TicketOpen(buf, n + 1, pub, now, &o) != COORD_TK_OK);
	CHECK(Coord_TicketOpen(buf, n, pub, now, &o) == COORD_TK_OK);
	/* a refused ticket leaves nothing in out */
	buf[10] ^= 1;
	CHECK(Coord_TicketOpen(buf, n, pub, now, &o) == COORD_TK_SIGNATURE && o.stateLen == 0 && !o.level[0]);
	buf[10] ^= 1;
	/* the largest state fits, one more does not */
	t.stateLen = COORD_STATE_MAX;
	memset(t.state, 'x', COORD_STATE_MAX);
	t.state[0] = 'a';
	t.state[1] = '=';
	n = Coord_TicketSeal(&t, seed, buf, sizeof(buf));
	CHECK(n > 0 && Coord_TicketOpen(buf, n, pub, now, &o) == COORD_TK_OK && o.stateLen == COORD_STATE_MAX);
	t.stateLen = COORD_STATE_MAX + 1;
	CHECK(Coord_TicketSeal(&t, seed, buf, sizeof(buf)) == 0);
	t.stateLen = 10;
	CHECK(Coord_TicketSeal(&t, seed, buf, 40) == 0);
	/* names longer than the field */
	memset(t.level, 'L', COORD_LEVEL);
	t.level[COORD_LEVEL] = 0;
	CHECK(Coord_TicketSeal(&t, seed, buf, sizeof(buf)) > 0);
	/* the join signature */
	uint8_t nonce[COORD_CHALLENGE], hash[64], hash2[64], sig[64];
	memset(nonce, 9, sizeof(nonce));
	Coord_JoinHash(hash, nonce, pub2, "Bob");
	Net2_Sign(seed, hash, sizeof(hash), sig);
	CHECK(Net2_Verify(pub, hash, sizeof(hash), sig));
	Coord_JoinHash(hash2, nonce, pub2, "Bobby");
	CHECK(!Net2_Verify(pub, hash2, sizeof(hash2), sig));
	Coord_JoinHash(hash2, nonce, pub, "Bob");
	CHECK(!Net2_Verify(pub, hash2, sizeof(hash2), sig));
	nonce[0] ^= 1;
	Coord_JoinHash(hash2, nonce, pub2, "Bob");
	CHECK(!Net2_Verify(pub, hash2, sizeof(hash2), sig));
	CHECK(!Net2_Verify(pub2, hash, sizeof(hash), sig));
}

static void Test_Hex(void) {
	uint8_t b[4] = { 0x00, 0x9f, 0xa0, 0xff }, c[4];
	char s[9];
	Coord_Hex(b, 4, s);
	CHECK(!strcmp(s, "009fa0ff"));
	CHECK(Coord_Unhex("009FA0ff", c, 4) && !memcmp(b, c, 4));
	CHECK(!Coord_Unhex("009fa0f", c, 4));
	CHECK(!Coord_Unhex("009fa0ffa", c, 4));
	CHECK(!Coord_Unhex("009fa0fg", c, 4));
}

static void Write(const char *path, const char *data, int n) {
	FILE *f = fopen(path, "wb");
	if (f) {
		fwrite(data, 1, (size_t)n, f);
		fclose(f);
	}
}

static int Exists(const char *path) {
	FILE *f = fopen(path, "rb");
	if (f) {
		fclose(f);
	}
	return f != NULL;
}

static void Test_Store(const char *dir) {
	static char text[COORD_STATE_MAX], file[COORD_STATE_MAX + 256];
	char hex[65], dat[COORD_PATH], bak[COORD_PATH], bad[COORD_PATH], out[64];
	uint8_t key[32];
	int len;

	Coord_MkDir(dir);
	for (int i = 0; i < 32; i++) {
		key[i] = (uint8_t)(i + 0x40);
	}
	Coord_Hex(key, 32, hex);
	snprintf(dat, sizeof(dat), "%s/%s.dat", dir, hex);
	snprintf(bak, sizeof(bak), "%s/%s.bak", dir, hex);
	snprintf(bad, sizeof(bad), "%s/%s.corrupt", dir, hex);
	remove(dat);
	remove(bak);
	remove(bad);
	/* a new player */
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_NEW && len == 0);
	/* round trip; a second save keeps the first as .bak */
	const char *one = "name=Bob\nmoney=2500\n";
	CHECK(CoordStore_Save(dir, key, one, (int)strlen(one)));
	CHECK(Exists(dat) && !Exists(bak));
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_OK && len == (int)strlen(one) &&
		!memcmp(text, one, (size_t)len));
	const char *two = "name=Bob\nmoney=4100\nship=SBANKER1<-<-<-<-<-1<-60<-0,0,0,0,0,0\n";
	CHECK(CoordStore_Save(dir, key, two, (int)strlen(two)));
	CHECK(Exists(dat) && Exists(bak));
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_OK);
	CHECK(CoordKv_Get(text, len, "money", out, sizeof(out)) && !strcmp(out, "4100"));
	/* an empty state and one without its last newline */
	CHECK(CoordStore_Save(dir, key, "", 0));
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_OK && len == 0);
	CHECK(CoordStore_Save(dir, key, "a=1", 3));
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_OK && len == 4 && !memcmp(text, "a=1\n", 4));
	/* an invalid state is not written */
	CHECK(!CoordStore_Save(dir, key, "no pair\n", 8));
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_OK && len == 4);
	/* the main file damaged in several ways: the one before it is read */
	CHECK(CoordStore_Save(dir, key, one, (int)strlen(one)));
	CHECK(CoordStore_Save(dir, key, two, (int)strlen(two)));
	int fn = Coord_ReadFile(dat, file, sizeof(file));
	CHECK(fn > 0);
	struct {
		const char *what;
		int at, cut;
		char to;
	} damage[] = {
		{ "a changed digit", 30, 0, '7' }, { "a changed header", 2, 0, 'X' }, { "a changed sum", fn - 3, 0, '0' },
		{ "cut short", 0, fn / 2, 0 }, { "cut before its newline", 0, fn - 1, 0 }, { "empty", 0, 0, 0 },
	};
	for (size_t d = 0; d < sizeof(damage) / sizeof(damage[0]); d++) {
		static char copy[COORD_STATE_MAX + 256];
		memcpy(copy, file, (size_t)fn);
		int n = fn;
		if (damage[d].to) {
			copy[damage[d].at] = copy[damage[d].at] == damage[d].to ? (char)(damage[d].to + 1) : damage[d].to;
		} else {
			n = damage[d].cut;
		}
		Write(dat, copy, n);
		int r = CoordStore_Load(dir, key, text, &len, sizeof(text));
		CHECK(r == COORD_LD_BACKUP);
		if (r != COORD_LD_BACKUP) {
			printf("  damage: %s\n", damage[d].what);
		}
		CHECK(CoordKv_Get(text, len, "money", out, sizeof(out)) && !strcmp(out, "2500"));
	}
	/* garbage and a file too large for a state */
	Write(dat, "\x01\x02garbage", 9);
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_BACKUP);
	{
		static char big[COORD_STATE_MAX + 1024];
		memset(big, 'a', sizeof(big));
		Write(dat, big, sizeof(big));
		CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_BACKUP);
	}
	/* a whole file with a body that is no state (hand edited with a good sum is still checked) */
	{
		const char *body = "scrapplayer 1\nnot a pair\n";
		uint8_t h[16];
		char sum[33], f2[128];
		Net2_Hash(h, 16, body, strlen(body));
		Coord_Hex(h, 16, sum);
		int k = snprintf(f2, sizeof(f2), "%ssum %s\n", body, sum);
		Write(dat, f2, k);
		CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_BACKUP);
	}
	/* both bad: the newest kept aside, a new player */
	Write(dat, "bad", 3);
	Write(bak, "bad too", 7);
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_CORRUPT && len == 0);
	CHECK(Exists(bad) && !Exists(dat));
	/* only the .bak left (a crash between the two renames) */
	remove(bad);
	remove(bak);
	CHECK(CoordStore_Save(dir, key, one, (int)strlen(one)));
	CHECK(CoordStore_Save(dir, key, two, (int)strlen(two)));
	remove(dat);
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_BACKUP);
	CHECK(CoordKv_Get(text, len, "money", out, sizeof(out)) && !strcmp(out, "2500"));
	/* only a bad .bak */
	Write(bak, "bad", 3);
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_CORRUPT && Exists(bad) && !Exists(bak));
	/* a temporary file of a crash before the rename is no state */
	char tmp[COORD_PATH + 8];
	snprintf(tmp, sizeof(tmp), "%s.tmp", dat);
	Write(tmp, "half", 4);
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_NEW);
	CHECK(CoordStore_Save(dir, key, one, (int)strlen(one)) && !Exists(tmp));
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_OK);
	/* the largest state */
	static char maxs[COORD_STATE_MAX];
	memset(maxs, 'v', sizeof(maxs));
	memcpy(maxs, "k=", 2);
	maxs[COORD_STATE_MAX - 1] = '\n';
	CHECK(CoordStore_Save(dir, key, maxs, COORD_STATE_MAX));
	CHECK(CoordStore_Load(dir, key, text, &len, sizeof(text)) == COORD_LD_OK && len == COORD_STATE_MAX);
	CHECK(!CoordStore_Save(dir, key, maxs, COORD_STATE_MAX + 1));
	/* the seed file */
	char seedPath[COORD_PATH];
	uint8_t s1[32], s2[32];
	int made;
	snprintf(seedPath, sizeof(seedPath), "%s/test.key", dir);
	remove(seedPath);
	CHECK(Coord_LoadSeed(seedPath, s1, &made) && made);
	CHECK(Coord_LoadSeed(seedPath, s2, &made) && !made && !memcmp(s1, s2, 32));
	Write(seedPath, "zz", 2);
	CHECK(!Coord_LoadSeed(seedPath, s2, &made));
	remove(seedPath);
	remove(dat);
	remove(bak);
	remove(bad);
}

static void Beat(ListBeat *b, const uint8_t seed[NET2_SEED_SIZE], const char *name, uint16_t port, uint32_t seq) {
	memset(b, 0, sizeof(*b));
	Net2_KeyPublic(seed, b->w.key);
	b->w.addr.port = port;
	b->seq = seq;
	snprintf(b->w.name, sizeof(b->w.name), "%s", name);
	snprintf(b->w.map, sizeof(b->w.map), "DownTown");
	snprintf(b->w.mode, sizeof(b->w.mode), "sandbox");
	b->w.players = 3;
	b->w.maxPlayers = 16;
	b->w.flags = COORD_INFO_WORLD;
	b->w.appVersion = 1;
}

static void Test_List(void) {
	uint8_t seed[NET2_SEED_SIZE], seed2[NET2_SEED_SIZE], nonce[COORD_CHALLENGE], nonce2[COORD_CHALLENGE];
	uint8_t lseed[NET2_SEED_SIZE], lkey[NET2_KEY_SIZE], msg[LIST_MSG];
	static ListReg reg;
	static uint8_t page[NET2_MAX_MESSAGE];
	static ListWorld got[LIST_MAX];
	ListBeat b, r;
	Net2Addr peer, loop, a;
	int isNew, total, first, count;

	for (int i = 0; i < NET2_SEED_SIZE; i++) {
		seed[i] = (uint8_t)(i + 1);
		seed2[i] = (uint8_t)(i * 7 + 3);
		lseed[i] = (uint8_t)(200 - i);
	}
	memset(nonce, 0x11, sizeof(nonce));
	memset(nonce2, 0x22, sizeof(nonce2));
	Net2_KeyPublic(lseed, lkey);
	/* heartbeats: the round trip, the signature over the challenge and the list server's key */
	Beat(&b, seed, "Ann's world", 28100, 1);
	snprintf(b.address, sizeof(b.address), "192.0.2.7");
	int n = List_WriteBeat(msg, sizeof(msg), &b, seed, nonce, lkey);
	CHECK(n > 0);
	CHECK(List_ReadBeat(msg, n, nonce, lkey, &r) == LIST_OK);
	CHECK(!memcmp(r.w.key, b.w.key, NET2_KEY_SIZE) && r.seq == 1 && r.w.addr.port == 28100);
	CHECK(!strcmp(r.w.name, "Ann's world") && !strcmp(r.w.map, "DownTown") && !strcmp(r.w.mode, "sandbox"));
	CHECK(!strcmp(r.address, "192.0.2.7") && r.w.players == 3 && r.w.maxPlayers == 16 && r.w.flags == COORD_INFO_WORLD);
	CHECK(List_ReadBeat(msg, n, nonce2, lkey, &r) == LIST_E_SIGNATURE);
	uint8_t other[NET2_KEY_SIZE];
	Net2_KeyPublic(seed2, other);
	CHECK(List_ReadBeat(msg, n, nonce, other, &r) == LIST_E_SIGNATURE);
	msg[10] ^= 1;
	CHECK(List_ReadBeat(msg, n, nonce, lkey, &r) == LIST_E_SIGNATURE);
	msg[10] ^= 1;
	CHECK(List_ReadBeat(msg, n - 1, nonce, lkey, &r) != LIST_OK);
	CHECK(List_ReadBeat(msg, 10, nonce, lkey, &r) == LIST_E_BAD);
	msg[1] = 9;
	CHECK(List_ReadBeat(msg, n, nonce, lkey, &r) == LIST_E_VERSION);
	msg[1] = LIST_VERSION;
	/* signed by another key than the one it names */
	n = List_WriteBeat(msg, sizeof(msg), &b, seed2, nonce, lkey);
	CHECK(List_ReadBeat(msg, n, nonce, lkey, &r) == LIST_E_SIGNATURE);
	/* the registry */
	ListReg_Init(&reg, 90000, 2);
	CHECK(Net2_ParseAddr("198.51.100.4:40000", 0, &peer));
	CHECK(Net2_ParseAddr("127.0.0.1:40001", 0, &loop));
	Beat(&b, seed, "A", 28100, 1);
	CHECK(ListReg_Beat(&reg, &b, nonce, &peer, 1000, &isNew) == LIST_OK && isNew);
	CHECK(ListReg_Count(&reg) == 0);
	CHECK(ListReg_Beat(&reg, &b, nonce, &peer, 1500, &isNew) == LIST_E_REPLAY);
	b.seq = 2;
	CHECK(ListReg_Beat(&reg, &b, nonce, &peer, 2000, &isNew) == LIST_OK && !isNew);
	b.seq = 1;
	CHECK(ListReg_Beat(&reg, &b, nonce2, &peer, 2500, &isNew) == LIST_OK && !isNew);
	/* the address it names: only from loopback */
	snprintf(b.address, sizeof(b.address), "203.0.113.9");
	b.seq = 2;
	CHECK(ListReg_Beat(&reg, &b, nonce2, &peer, 3000, &isNew) == LIST_E_ADDRESS);
	/* probed, then listed at the address the connection came from */
	CHECK(ListReg_NextProbe(&reg, 3000, &a));
	CHECK(a.family == NET2_AF_IPV4 && a.ip[0] == 198 && a.port == 28100);
	CHECK(!ListReg_NextProbe(&reg, 3001, &a));
	CHECK(ListReg_NextProbe(&reg, 3000 + LIST_PROBE_MS, &a));
	Net2ServerInfo info;
	memset(&info, 0, sizeof(info));
	info.players = 5;
	info.maxPlayers = 20;
	CHECK(ListReg_Answer(&reg, &a, &info));
	CHECK(ListReg_Count(&reg) == 1);
	CHECK(!ListReg_NextProbe(&reg, 100000, &a));
	/* a coordinator on the list server's machine names its public address */
	ListBeat c;
	Beat(&c, seed2, "B", 28200, 1);
	snprintf(c.address, sizeof(c.address), "203.0.113.9");
	CHECK(ListReg_Beat(&reg, &c, nonce, &loop, 4000, &isNew) == LIST_OK && isNew);
	CHECK(ListReg_NextProbe(&reg, 4000, &a) && a.ip[0] == 203 && a.ip[3] == 9 && a.port == 28200);
	CHECK(ListReg_Answer(&reg, &a, &info));
	/* a page of the listed worlds */
	n = ListReg_WritePage(&reg, 0, page, sizeof(page), &count);
	CHECK(n > 0 && count == 2);
	int k = List_ReadPage(page, n, got, LIST_MAX, &total, &first);
	CHECK(k == 2 && total == 2 && first == 0);
	CHECK(!strcmp(got[0].name, "A") && got[0].players == 5 && got[0].addr.port == 28100);
	CHECK(!memcmp(got[1].key, c.w.key, NET2_KEY_SIZE) && got[1].addr.ip[0] == 203);
	CHECK(List_ReadPage(page, n - 1, got, LIST_MAX, &total, &first) == 0);
	page[0] = 'X';
	CHECK(List_ReadPage(page, n, got, LIST_MAX, &total, &first) == 0);
	/* the limit of one address */
	uint8_t s3[NET2_SEED_SIZE];
	for (int i = 0; i < 3; i++) {
		memcpy(s3, seed, sizeof(s3));
		s3[0] = (uint8_t)(50 + i);
		Beat(&c, s3, "C", (uint16_t)(29000 + i), 1);
		int res = ListReg_Beat(&reg, &c, nonce, &peer, 5000, &isNew);
		CHECK(i < 1 ? res == LIST_OK : res == LIST_E_FULL);
	}
	/* the times */
	CHECK(ListReg_Expire(&reg, 2500 + 89999) == 0);
	CHECK(ListReg_Expire(&reg, 2500 + 90000) == 1);
	CHECK(ListReg_Expire(&reg, 4000 + 90000) == 1);
	CHECK(ListReg_Expire(&reg, 5000 + 90000) == 1);
	CHECK(ListReg_Count(&reg) == 0);
	/* many worlds in pages */
	ListReg_Init(&reg, 90000, LIST_PER_ADDR);
	for (int i = 0; i < 300; i++) {
		memcpy(s3, seed, sizeof(s3));
		s3[0] = (uint8_t)i;
		s3[1] = (uint8_t)(i >> 8);
		Beat(&c, s3, "Many", 28100, 1);
		char ip[32];
		snprintf(ip, sizeof(ip), "10.0.%d.%d:1", i / 200, i % 200 + 1);
		CHECK(Net2_ParseAddr(ip, 0, &a));
		CHECK(ListReg_Beat(&reg, &c, nonce, &a, 1000, &isNew) == LIST_OK);
		a.port = 28100;
		CHECK(ListReg_Answer(&reg, &a, &info));
	}
	CHECK(ListReg_Count(&reg) == 300);
	int all = 0;
	for (first = 0; first < 300;) {
		n = ListReg_WritePage(&reg, first, page, sizeof(page), &count);
		CHECK(n > 0 && count > 0 && count <= LIST_PAGE);
		k = List_ReadPage(page, n, got + all, LIST_MAX - all, &total, &first);
		CHECK(k == count && total == 300);
		all += k;
		first += count;
		if (!count) {
			break;
		}
	}
	CHECK(all == 300);
}

static void Test_Quote(void) {
	char out[64];
	CHECK(CmdQuote_Arg("-name:a-b c", out, sizeof(out)) == 13 && !strcmp(out, "-name:\"a-b c\""));
	CHECK(CmdQuote_Arg("-imap:Levels/FZ", out, sizeof(out)) == 15 && !strcmp(out, "-imap:Levels/FZ"));
	CHECK(CmdQuote_Arg("-join:::1", out, sizeof(out)) > 0 && !strcmp(out, "-join:\"::1\""));
	CHECK(CmdQuote_Arg("-name:'a b'", out, sizeof(out)) > 0 && !strcmp(out, "-name:'a b'"));
	CHECK(CmdQuote_Arg("-dedicated", out, sizeof(out)) > 0 && !strcmp(out, "-dedicated"));
	CHECK(CmdQuote_Arg("two words", out, sizeof(out)) > 0 && !strcmp(out, "\"two words\""));
	CHECK(CmdQuote_Arg("-name:a-b", out, 10) == -1);
	CHECK(CmdQuote_Arg("-name:a-b", out, 12) == 11);
}

static void Test_Places(void) {
	static PlaceTable t;
	char err[128], list[256];
	const char *ok =
		"# a comment\n"
		"place TownHall\n"
		"\tlink DM_Door_ElevatorOrbit_Orbit Orbit\n"
		"place Orbit listed\n"
		"\tlink DM_Door_ElevatorOrbitC_TownHall TownHall,Fake   # two levels\n"
		"\tnpc Bishop_00\n"
		"\tnpc Police_F1 DM_Conversion_Point_14 -300 0 10.5 * fixed\n"
		"\tgate Hall - 1 2 3 90 to TownHall Junkyard:Lift\n"
		"\ttraffic 12 SBetty1,SFunc1\n"
		"\trail 1 2 3\n"
		"\tpolice on\n"
		"\tspawn DM_Player_Spawn 0 0 0 *\n"
		"\trepos Functionary 3\n"
		"\tnouse DM_EditKeyboard\n"
		"\tai 6000\n"
		"place QF listed\n";
	CHECK(PlaceDef_Parse(ok, &t, err, sizeof(err)));
	CHECK(t.n == 3);
	const PlaceLevel *l = PlaceDef_Level(&t, "orbit");
	CHECK(l && l->listed && l->nlinks == 1 && !strcmp(l->link[0].levels, "TownHall,Fake"));
	CHECK(l && l->nnpcs == 2 && l->npc[0].mission && !l->npc[0].fixed && !strcmp(l->npc[0].name, "Bishop_00"));
	CHECK(l && !l->npc[1].mission && l->npc[1].fixed && l->npc[1].pos.dummyYaw && l->npc[1].pos.x == -300.0f &&
		l->npc[1].pos.z == 10.5f && !strcmp(l->npc[1].pos.dummy, "DM_Conversion_Point_14"));
	const PlaceGate *g = PlaceDef_Gate(l, "hall");
	CHECK(g && g->nopt == 2 && !g->pos.dummy[0] && g->pos.yaw == 90.0f && g->pos.y == 2.0f);
	CHECK(g && !strcmp(g->opt[0].level, "TownHall") && !g->opt[0].gate[0] && !strcmp(g->opt[1].level, "Junkyard") &&
		!strcmp(g->opt[1].gate, "Lift"));
	CHECK(l && l->traffic == 12 && !strcmp(l->ships, "SBetty1,SFunc1") && l->rail[2] == 3 && l->police == 1);
	CHECK(l && l->nspawns == 1 && l->nrepos == 1 && l->repos[0].n == 3 && l->nnouse == 1 && l->intraffic[0] == -1);
	CHECK(l && l->aiStep == 6000.0f);
	l = PlaceDef_Level(&t, "TownHall");
	CHECK(l && !l->listed && l->traffic == -1 && l->police == -1);
	CHECK(PlaceDef_Places(&t, list, sizeof(list)) && !strcmp(list, "Orbit,QF"));
	CHECK(!PlaceDef_Places(&t, list, 7) && !strcmp(list, "Orbit"));
	CHECK(!PlaceDef_Parse("\tnpc A_1\n", &t, err, sizeof(err)) && strstr(err, "line 1"));
	CHECK(!PlaceDef_Parse("place A\nplace a\n", &t, err, sizeof(err)) && strstr(err, "line 2"));
	CHECK(!PlaceDef_Parse("place A\n\tnpc Nobody\n", &t, err, sizeof(err)));
	CHECK(!PlaceDef_Parse("place A\n\tgate G - 1 2 3 * to B\n", &t, err, sizeof(err)));
	CHECK(!PlaceDef_Parse("place A\n\tgate G - 1 2 3 0 B\n", &t, err, sizeof(err)));
	CHECK(!PlaceDef_Parse("place A\n\tpolice maybe\n", &t, err, sizeof(err)));
	CHECK(!PlaceDef_Parse("place A\n\ttraffic -1\n", &t, err, sizeof(err)));
	CHECK(!PlaceDef_Parse("place A\n\twhat 1\n", &t, err, sizeof(err)) && strstr(err, "what"));
	CHECK(!PlaceDef_Parse("place A\n\tai 10\n", &t, err, sizeof(err)));
	/* the built-in table: the city comes first in the world's levels, the places after it */
	const char *w = PlaceDef_WorldLevels();
	CHECK(!strncmp(w, COORD_CITY, strlen(COORD_CITY)) && strstr(w, ",Orbit") && strstr(w, ",QF"));
	CHECK(PlaceDef_Level(PlaceDef_Table(), "Orbit") != NULL);
	int levels = 1;
	for (const char *p = w; *p; p++) {
		levels += *p == ',';
	}
	CHECK(levels <= COORD_MAX_LEVELS);
}

int main(int argc, char **argv) {
	Test_Kv();
	Test_Hex();
	Test_Ticket();
	Test_Store(argc > 1 ? argv[1] : "coord_test_data");
	Test_List();
	Test_Quote();
	Test_Places();
	printf("coord_test: %d checks, %d failed\n", checks, failed);
	return failed != 0;
}
