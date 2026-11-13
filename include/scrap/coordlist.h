/*
 * The list server of the worlds (stage M5c of the multiplayer plan, src/coord/List.c and the program ScrapList of
 * src/coord/listmain.c). Not in the original.
 *
 * Coordinators register with it and clients ask it for the worlds. Both connect to it over Net2 (its port,
 * COORD_LIST_PORT by default); it sends every connection a challenge (a random nonce). A coordinator then sends a
 * heartbeat every LIST_BEAT_SECS (and soon after its number of players changed): its identity key, a sequence number,
 * its port and the server info, signed with its identity key over the challenge and the list server's own key (so a
 * heartbeat counts only on the connection it was made for). A world stays listed LIST_TTL_SECS after its last
 * heartbeat; the list server lists it at the address the connection comes from (a coordinator on the list server's
 * own machine may name another one, its public address) and only once that address answered a server query on the
 * port (Net2_Query: the world is reachable there). A client sends a query and gets the listed worlds in pages; the
 * entries carry the identity keys, so a client pins the key when it joins (Net2Config.serverKey).
 *
 * Wire formats (little endian; strings u8 length + bytes; the reliable channel 0 of the Net2 default layout):
 *   list -> any:          'C' challenge: version u16, nonce 32
 *   coordinator -> list:  'B' beat: version u16, key 32, seq u32, port u16, address str ("" the one it sees), name str,
 *                         map str, mode str, players u8, max u8, flags u8, app version u32, signature 64 (EdDSA of
 *                         BLAKE2b-512 of LIST_SIG_LABEL, the challenge, the list server's key and the bytes of the
 *                         beat before the signature)
 *   list -> coordinator:  'A' answer: result u8 (LIST_*), seconds it stays listed u16
 *   client -> list:       'Q' query: version u16
 *   list -> client:       'L' page: total u16, first u16, count u8, then per world: family u8, ip 16, port u16, key 32,
 *                         name str, map str, mode str, players u8, max u8, flags u8 (pages until first + count is
 *                         total; the list server closes the connection after the last one)
 */
#ifndef SCRAP_COORDLIST_H
#define SCRAP_COORDLIST_H

#include <stdint.h>

#include "scrap/coord.h"
#include "scrap/net2.h"

#define LIST_VERSION 1
#define LIST_BEAT_SECS 30				/* a coordinator's heartbeat */
#define LIST_BEAT_MIN_SECS 5			/* at most this often (a change of the players) */
#define LIST_TTL_SECS 90				/* listed this long after the last heartbeat */
#define LIST_MAX 1024					/* worlds a list server keeps */
#define LIST_PER_ADDR 16				/* worlds of one address */
#define LIST_PAGE 128					/* worlds in one page */
#define LIST_PROBE_MS 3000				/* an unverified world is asked this often */
#define LIST_SIG_LABEL "scrapland list beat 1"
#define LIST_MSG 512					/* bytes of a beat */

#define LIST_MSG_CHALLENGE 'C'
#define LIST_MSG_BEAT 'B'
#define LIST_MSG_ANSWER 'A'
#define LIST_MSG_QUERY 'Q'
#define LIST_MSG_PAGE 'L'

/* results of a beat */
#define LIST_OK 0
#define LIST_E_BAD 1					/* malformed */
#define LIST_E_VERSION 2
#define LIST_E_SIGNATURE 3
#define LIST_E_REPLAY 4					/* not newer than the last one of the key on this challenge */
#define LIST_E_FULL 5					/* LIST_MAX worlds, or LIST_PER_ADDR of the address */
#define LIST_E_ADDRESS 6				/* an address it may not name */

/* a world as a heartbeat gives it and a page lists it */
typedef struct ListWorld {
	uint8_t key[NET2_KEY_SIZE];
	Net2Addr addr;					/* where clients reach the coordinator (port included) */
	char name[NET2_INFO_NAME + 1], map[NET2_INFO_MAP + 1], mode[NET2_INFO_MODE + 1];
	uint8_t players, maxPlayers, flags;
	uint32_t appVersion;
} ListWorld;

/* a heartbeat: the world, its sequence number, the address it names (text, "" none) */
typedef struct ListBeat {
	ListWorld w;
	uint32_t seq;
	char address[48];
} ListBeat;

/* the message 'B' of beat (its address field and w.addr.port are written; w.addr's ip is not) signed with seed for
 * the challenge nonce and the list server's key: its size, 0 when it does not fit */
int List_WriteBeat(uint8_t *out, int cap, const ListBeat *b, const uint8_t seed[NET2_SEED_SIZE],
	const uint8_t nonce[COORD_CHALLENGE], const uint8_t listKey[NET2_KEY_SIZE]);
/* a message 'B' read and its signature checked: LIST_OK, LIST_E_BAD, LIST_E_VERSION, LIST_E_SIGNATURE (out->w.addr
 * holds only the port) */
int List_ReadBeat(const uint8_t *data, int size, const uint8_t nonce[COORD_CHALLENGE],
	const uint8_t listKey[NET2_KEY_SIZE], ListBeat *out);

/* ---- the registry of a list server (pure: the times are ms of the caller's clock) ---- */

typedef struct ListEntry {
	ListWorld w;
	uint8_t used, verified;
	uint8_t nonce[COORD_CHALLENGE];	/* the challenge of its last heartbeat */
	uint32_t seq;
	uint64_t expires, probeAt;
} ListEntry;

typedef struct ListReg {
	ListEntry e[LIST_MAX];
	int perAddr;					/* LIST_PER_ADDR, or less */
	uint32_t ttl;					/* ms */
} ListReg;

void ListReg_Init(ListReg *r, uint32_t ttlMs, int perAddr);
/* a checked heartbeat that came on a connection from peer with the challenge nonce: LIST_OK (*isNew 1 for a world
 * not listed before), LIST_E_REPLAY, LIST_E_FULL, LIST_E_ADDRESS. The world's address is peer's ip, or the beat's
 * address when peer is a loopback address; the port the beat's */
int ListReg_Beat(ListReg *r, const ListBeat *b, const uint8_t nonce[COORD_CHALLENGE], const Net2Addr *peer,
	uint64_t now, int *isNew);
/* worlds past their time removed: their count */
int ListReg_Expire(ListReg *r, uint64_t now);
/* the next world to probe (not verified, its time come): 1 with its address, its next time set */
int ListReg_NextProbe(ListReg *r, uint64_t now, Net2Addr *out);
/* the answer of a server query from addr: the world there verified (its players and max from info); 1 when one was */
int ListReg_Answer(ListReg *r, const Net2Addr *addr, const Net2ServerInfo *info);
/* the verified worlds */
int ListReg_Count(const ListReg *r);
/* the page of the verified worlds from index first on (at most LIST_PAGE): its size, 0 when it does not fit; *count
 * the worlds in it */
int ListReg_WritePage(const ListReg *r, int first, uint8_t *out, int cap, int *count);
/* a page read: the worlds into out (at most cap), *total and *first: their count, 0 when malformed */
int List_ReadPage(const uint8_t *data, int size, ListWorld *out, int cap, int *total, int *first);

/* ---- the registration of a coordinator (src/coord/List.c, used by Coord.c) ---- */

typedef struct ListClient {
	char addr[64];
	uint16_t port;
	Net2Host *net;
	int phase;
	uint8_t nonce[COORD_CHALLENGE];
	uint32_t seq;
	uint64_t nextBeat, lastBeat, retryAt;
	int lastPlayers;
	int listed;						/* the last answer was LIST_OK */
} ListClient;

/* addr "" switches it off */
void ListClient_Init(ListClient *c, const char *addr, uint16_t port);
/* one pass: connect (again), the challenge, a beat when due (b: the world now, its key and port; the address it may
 * name), the answers; log for its lines (NULL none) */
void ListClient_Pass(ListClient *c, uint64_t now, const ListBeat *b, const uint8_t seed[NET2_SEED_SIZE],
	void (*log)(const char *fmt, ...));
void ListClient_Close(ListClient *c);

#endif
