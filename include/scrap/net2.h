/*
 * Net2: the transport of the new multiplayer (stage M1 of the multiplayer plan), not compatible with the original
 * protocol of Net/. One host is a server (many connections) or a client (one connection, also the server browser).
 * UDP over IPv4 and IPv6, non-blocking: the game drives a host once per frame and nothing in here waits.
 *
 *   Net2_Poll(host, now);                  receive, handshakes, time outs
 *   while (Net2_Receive(host, &ev)) ...    connect, disconnect, message, server info
 *   Net2_Send(host, conn, channel, ...);   queue messages
 *   Net2_Flush(host, now);                 build and send the packets
 *
 * Times are milliseconds of any monotonic clock of the caller (uint64_t). Connection handshake: X25519 with an
 * ephemeral key on both sides, the server signs it with its identity key (EdDSA, BLAKE2b), a stateless cookie
 * against spoofed connects; then ChaCha20-Poly1305 (RFC 8439) per packet with one key per direction, the packet
 * sequence number as the nonce and a replay window of 64 packets. Crypto: third_party/monocypher.
 *
 * The structs of this header are in-memory structs of the caller's ABI (pointer sized members where noted), never
 * a wire format; the wire formats are byte by byte, little endian, in src/net2/net2local.h.
 */
#ifndef SCRAP_NET2_H
#define SCRAP_NET2_H

#include <stddef.h>
#include <stdint.h>

#define NET2_MTU 1200				/* largest UDP payload sent or accepted */
#define NET2_VERSION 1				/* of this protocol, checked in the handshake */
#define NET2_DEFAULT_PORT 28100
#define NET2_MAX_CHANNELS 8
#define NET2_MAX_CONNS 1024			/* per server host */
#define NET2_FRAG_SIZE 1024			/* bytes of a reliable fragment */
#define NET2_MAX_FRAGS 64
#define NET2_MAX_MESSAGE (NET2_FRAG_SIZE * NET2_MAX_FRAGS)	/* largest reliable message */
#define NET2_MAX_UNRELIABLE 1159	/* largest unreliable or sequenced message: one packet */
#define NET2_KEY_SIZE 32			/* public identity key of a server */
#define NET2_LAN_PORTS 8			/* LAN discovery asks the base port and the 7 after it */

/* channel kinds */
#define NET2_UNRELIABLE 0			/* may be lost, duplicated never, any order (input) */
#define NET2_SEQUENCED 1			/* may be lost, older than the last one delivered are dropped (snapshots) */
#define NET2_RELIABLE 2				/* every message once, in order of the channel, fragmented (events, chat) */

/* address families of Net2Addr */
#define NET2_AF_NONE 0
#define NET2_AF_IPV4 4
#define NET2_AF_IPV6 6

/* event types */
#define NET2_EV_NONE 0
#define NET2_EV_CONNECT 1			/* conn is up (client: the server accepted; server: a new client) */
#define NET2_EV_DISCONNECT 2		/* conn is gone, reason says why; the index can be reused after this */
#define NET2_EV_MESSAGE 3			/* data and size of a message on channel */
#define NET2_EV_SERVERINFO 4		/* answer to Net2_Query: addr, info, ping */

/* disconnect reasons (one byte on the wire); the game's own reasons are NET2_DR_APP and above */
#define NET2_DR_NONE 0
#define NET2_DR_LOCAL 1				/* Net2_Disconnect without a reason */
#define NET2_DR_TIMEOUT 2
#define NET2_DR_FULL 3				/* server: no free connection */
#define NET2_DR_VERSION 4			/* protocol or game version differ */
#define NET2_DR_CHANNELS 5			/* the channel layouts differ */
#define NET2_DR_PROTOCOL 6			/* a malformed packet or handshake */
#define NET2_DR_SHUTDOWN 7			/* the host was destroyed */
#define NET2_DR_KEY 8				/* the server key is not the pinned one */
#define NET2_DR_SEQUENCE 9			/* the packet numbers ran out (2^32 packets) */
#define NET2_DR_OVERFLOW 10			/* reliable queue or receive buffer limits */
#define NET2_DR_APP 64

/* errors of Net2_Create, Net2_Connect and Net2_Send */
#define NET2_OK 0
#define NET2_E_ARG -1				/* bad argument (channel, size, conn) */
#define NET2_E_SOCKET -2			/* socket could not be opened or bound */
#define NET2_E_NOMEM -3
#define NET2_E_STATE -4				/* not connected, not a client ... */
#define NET2_E_FULL -5				/* reliable window (256 messages) or queue bytes full */
#define NET2_E_RANDOM -6			/* no random numbers from the system */

/*
 * An address: 24 bytes on every ABI (no padding). ip holds 4 bytes for IPv4 (ip[4..15] zero), 16 for IPv6;
 * port in host order; scope is the IPv6 scope id (interface) of link local addresses.
 */
typedef struct Net2Addr {
	uint8_t family;
	uint8_t pad;
	uint16_t port;
	uint32_t scope;
	uint8_t ip[16];
} Net2Addr;

/*
 * The game's replacement for the socket layer (tests: an in-memory link). send returns 1 when the datagram is gone
 * (or dropped), recv the size of one datagram, 0 when none is waiting. Pointers: ctx and the two functions.
 */
typedef struct Net2Io {
	void *ctx;
	int (*send)(void *ctx, const Net2Addr *to, const uint8_t *data, int size);
	int (*recv)(void *ctx, Net2Addr *from, uint8_t *buf, int cap);
} Net2Io;

/* 0 drops the datagram: a test hook for loss on real sockets (outgoing 1 on send, 0 on receive) */
typedef int (*net2filter_t)(void *ctx, int outgoing, const Net2Addr *addr, const uint8_t *data, int size);

typedef struct Net2Config {
	int server;						/* 1 server, 0 client */
	uint16_t port;					/* server: the port (both families); client: 0 any */
	int ipv4, ipv6;					/* which sockets to open */
	Net2Addr bind4, bind6;			/* family NET2_AF_NONE: any address of the family */
	int maxConns;					/* server: 1..NET2_MAX_CONNS */
	uint32_t appVersion;			/* the game's version, must match in the handshake */
	int numChannels;				/* 1..NET2_MAX_CHANNELS, the same on both sides */
	uint8_t channels[NET2_MAX_CHANNELS];	/* NET2_UNRELIABLE, NET2_SEQUENCED, NET2_RELIABLE */
	uint32_t rate;					/* bytes per second sent per connection */
	uint32_t timeout;				/* ms without a packet until the connection drops */
	uint32_t connectTimeout;		/* ms for a client handshake */
	uint32_t maxPending;			/* reliable bytes queued per connection (send side) */
	uint32_t maxRecvBuffer;			/* bytes of out-of-order reliable messages per connection */
	uint32_t inboxSize;				/* bytes of received unreliable messages per connection until Receive */
	uint32_t outboxSize;			/* bytes of unreliable messages per connection until Flush */
	uint32_t sockBuffer;			/* bytes of socket receive and send buffer to ask for (the system may give
									   less), 0 its default; a server polled once per tick needs room for all
									   datagrams of a tick */
	const uint8_t *identitySeed;	/* server: 32 bytes of its identity key, NULL: a random one for this host */
	const uint8_t *serverKey;		/* client: NET2_KEY_SIZE bytes the server's identity must match, or NULL */
	const Net2Io *io;				/* NULL: UDP sockets */
	net2filter_t filter;			/* NULL: none */
	void *filterCtx;
	void *(*alloc)(void *ctx, size_t size);	/* NULL: malloc and free */
	void (*free)(void *ctx, void *p);
	void *allocCtx;
} Net2Config;

/* a server list entry: the answer to a query (LAN discovery or direct) */
#define NET2_INFO_NAME 32
#define NET2_INFO_MAP 32
#define NET2_INFO_MODE 16
typedef struct Net2ServerInfo {
	uint32_t appVersion;
	uint16_t version;				/* NET2_VERSION of the server */
	uint8_t players, maxPlayers;
	uint8_t flags;					/* the game's (password, dedicated ...) */
	char name[NET2_INFO_NAME + 1];	/* zero terminated */
	char map[NET2_INFO_MAP + 1];
	char mode[NET2_INFO_MODE + 1];
} Net2ServerInfo;

typedef struct Net2Event {
	int type;						/* NET2_EV_* */
	int conn;						/* server: connection index; client: 0 */
	int channel;
	int reason;						/* NET2_EV_DISCONNECT */
	const uint8_t *data;			/* NET2_EV_MESSAGE: valid until the next Net2_Receive */
	int size;
	Net2Addr addr;					/* NET2_EV_SERVERINFO: the server; CONNECT: the peer */
	uint32_t ping;					/* NET2_EV_SERVERINFO: ms */
	Net2ServerInfo info;
} Net2Event;

typedef struct Net2Stats {
	uint32_t rtt, rttVar;			/* smoothed round trip time and its variation, ms */
	float loss;						/* packet loss estimate 0..1 */
	uint32_t sendRate, recvRate;	/* bytes per second of the last full second */
	uint64_t packetsSent, packetsRecv, packetsLost;
	uint64_t bytesSent, bytesRecv;
	uint64_t resent;				/* reliable fragments sent again (not acked in time) */
	uint32_t reliablePending;		/* reliable messages queued or not acked yet */
	uint32_t dropped;				/* unreliable messages not sent (rate, space) or not kept (inbox full) */
	uint32_t bad;					/* packets that failed the replay window, the tag or the parse */
} Net2Stats;

typedef struct Net2Host Net2Host;

void Net2_DefaultConfig(Net2Config *cfg, int server);
Net2Host *Net2_Create(const Net2Config *cfg, int *err);
void Net2_Destroy(Net2Host *h);	/* sends a disconnect to every peer first */

int Net2_Connect(Net2Host *h, const Net2Addr *server, uint64_t now);
void Net2_Poll(Net2Host *h, uint64_t now);
int Net2_Receive(Net2Host *h, Net2Event *ev);	/* 1 with an event, 0 when there is none */
int Net2_Send(Net2Host *h, int conn, int channel, const void *data, int size);
void Net2_Flush(Net2Host *h, uint64_t now);
void Net2_Disconnect(Net2Host *h, int conn, int reason);

int Net2_IsConnected(const Net2Host *h, int conn);
int Net2_GetStats(const Net2Host *h, int conn, Net2Stats *st);
/* tests: the datagrams the host sends from now on are dropped by chance (percent) or held back delay ms plus up to
 * jitter ms (they may come out of order); once per host */
int Net2_SetTestLink(Net2Host *h, float lossPercent, uint32_t delayMs, uint32_t jitterMs, uint32_t seed);
void Net2_SetRate(Net2Host *h, int conn, uint32_t rate);
int Net2_PeerAddr(const Net2Host *h, int conn, Net2Addr *addr);
int Net2_ServerKey(const Net2Host *h, uint8_t key[NET2_KEY_SIZE]);	/* server: own; client: the server's */
uint16_t Net2_LocalPort(const Net2Host *h, int family);

void Net2_SetServerInfo(Net2Host *h, const Net2ServerInfo *info);
int Net2_Query(Net2Host *h, const Net2Addr *to, uint64_t now);
int Net2_LanQuery(Net2Host *h, uint16_t basePort, uint64_t now);	/* basePort .. + NET2_LAN_PORTS - 1 */
int Net2_LanAddr(int family, uint16_t port, Net2Addr *out);	/* IPv4 broadcast or the IPv6 group */

int Net2_ParseAddr(const char *s, uint16_t defPort, Net2Addr *out);	/* numeric only: no name lookup */
/* the sockets of cfg bound to the numeric address bind (only its family opened); "" or NULL leaves cfg as it is
 * (both families, any address); 0 when it is no numeric address */
int Net2_SetBind(Net2Config *cfg, const char *bind);
void Net2_AddrString(const Net2Addr *a, char *buf, int size);
int Net2_AddrEqual(const Net2Addr *a, const Net2Addr *b);
const char *Net2_ReasonString(int reason);

/*
 * Identity keys for the game's own use (players, the coordinator's tickets): the signature of the server identity of
 * the handshake (EdDSA on Curve25519 with BLAKE2b, Monocypher's crypto_eddsa). A key is its 32 byte seed; the public
 * key and the signatures follow from it. The random numbers of the system and a BLAKE2b hash.
 */
#define NET2_SEED_SIZE 32
#define NET2_SIG_SIZE 64
int Net2_Random(void *buf, size_t size);	/* 1 ok, 0 when the system gives none */
void Net2_KeyPublic(const uint8_t seed[NET2_SEED_SIZE], uint8_t pub[NET2_KEY_SIZE]);
void Net2_Sign(const uint8_t seed[NET2_SEED_SIZE], const void *msg, size_t size, uint8_t sig[NET2_SIG_SIZE]);
int Net2_Verify(const uint8_t pub[NET2_KEY_SIZE], const void *msg, size_t size, const uint8_t sig[NET2_SIG_SIZE]);	/* 1 good */
void Net2_Hash(uint8_t *out, int outSize, const void *msg, size_t size);	/* BLAKE2b, outSize 1..64 */
void Net2_Wipe(void *p, size_t size);

#endif
