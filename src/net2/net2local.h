/*
 * Internals of Net2 (include/scrap/net2.h): wire formats, the byte reader and writer, the structs of a host and
 * its connections. Every number on the wire is little endian, written byte by byte, so no struct is ever sent.
 *
 * Datagram types (byte 0):
 *   HELLO     c>s  type, protocol u32, version u16, appVersion u32, salt[8], client public[32], zeros to 256 bytes
 *   CHALLENGE s>c  type, salt[8], cookie[16]
 *   CONNECT   c>s  type, protocol u32, version u16, appVersion u32, salt[8], client public[32], cookie[16],
 *                  numChannels u8, kinds[numChannels], zeros to 256 bytes
 *   ACCEPT    s>c  type, salt[8], conn id u32, server public[32], server identity[32], signature[64]
 *   DENY      s>c  type, salt[8], reason u8
 *   DATA      both type, conn id u32, seq u32, cipher text, tag[16]; the 9 bytes in front are the additional data
 *   QUERY     any  type, protocol u32, token u32, time u32, zeros to 128 bytes
 *   INFO      s>any type, protocol u32, token u32, time u32, version u16, appVersion u32, players u8, max u8,
 *                  flags u8, then name, map and mode as length u8 + bytes
 * HELLO and CONNECT are padded so that no answer of the server is larger than the request (no amplification), the
 * salt is random per connect attempt and echoed, so an off-path sender cannot fake a CHALLENGE, ACCEPT or DENY.
 *
 * DATA plain text: ack u32 (newest packet received, 0 none), ackBits u32 (bit i: packet ack-1-i received), ack
 * delay u16 (ms since that packet came), then chunks until the end:
 *   UNRELIABLE  type, channel u8, size u16, data
 *   SEQUENCED   type, channel u8, seq u16, size u16, data
 *   RELIABLE    type, channel u8, id u16, size u16, data                   (a message of one fragment)
 *   FRAGMENT    type, channel u8, id u16, index u8, count u8, size u16, data (count 2..64; every fragment but
 *               the last is NET2_FRAG_SIZE bytes)
 *   DISCONNECT  type, reason u8
 * Packet numbers start at 1 in each direction and never wrap (the connection ends before 2^32).
 */
#ifndef NET2LOCAL_H
#define NET2LOCAL_H

#include <stdint.h>
#include <string.h>

#include "scrap/net2.h"

#define N2_PROTOCOL 0x32524353u		/* "SCR2" */

#define N2P_HELLO 0x01
#define N2P_CHALLENGE 0x02
#define N2P_CONNECT 0x03
#define N2P_ACCEPT 0x04
#define N2P_DENY 0x05
#define N2P_DATA 0x10
#define N2P_QUERY 0x20
#define N2P_INFO 0x21

#define N2K_UNRELIABLE 1
#define N2K_SEQUENCED 2
#define N2K_RELIABLE 3
#define N2K_FRAGMENT 4
#define N2K_DISCONNECT 5

#define N2_HEADER 9
#define N2_TAG 16
#define N2_ACKS 10
#define N2_OVERHEAD (N2_HEADER + N2_TAG + N2_ACKS)
#define N2_PLAIN_MAX (NET2_MTU - N2_HEADER - N2_TAG)
#define N2_HANDSHAKE_SIZE 256
#define N2_CHALLENGE_SIZE 25
#define N2_ACCEPT_SIZE 141
#define N2_DENY_SIZE 10
#define N2_QUERY_SIZE 128
#define N2_SALT 8
#define N2_COOKIE 16

#define N2_WINDOW 256				/* reliable message ids in flight per channel */
#define N2_SENT_RING 128			/* packets remembered for acks */
#define N2_PKT_REFS 32				/* reliable fragments per packet */
#define N2_EVENTS 64				/* queued server info events */

#define N2_RESEND 250				/* ms between handshake packets */
#define N2_ACK_DELAY 20				/* ms a received packet waits for a packet that carries its ack */
#define N2_ACK_COUNT 16				/* or this many received packets */
#define N2_KEEPALIVE 250			/* ms without a sent packet */
#define N2_RTO_INIT 200
#define N2_RTO_MIN 40
#define N2_RTO_MAX 2000
#define N2_FLUSH_PACKETS 16			/* packets per connection and flush */
#define N2_CLOSE_SENDS 3			/* packets with the disconnect chunk */
#define N2_COOKIE_PERIOD 10000		/* ms; a cookie is good for this and the period before */
#define N2_POLL_PACKETS 4096		/* datagrams handled per poll */

/* byte writer: overflow is sticky, the caller checks it once */
typedef struct Net2Writer {
	uint8_t *p;
	int size;
	int pos;
	int overflow;
} Net2Writer;

static inline void Net2W_Init(Net2Writer *w, uint8_t *p, int size) {
	w->p = p;
	w->size = size;
	w->pos = 0;
	w->overflow = 0;
}

static inline void Net2W_Bytes(Net2Writer *w, const void *src, int n) {
	if (w->overflow || n < 0 || n > w->size - w->pos) {
		w->overflow = 1;
		return;
	}
	if (n) {
		memcpy(w->p + w->pos, src, (size_t)n);
	}
	w->pos += n;
}

static inline void Net2W_Zeros(Net2Writer *w, int n) {
	if (w->overflow || n < 0 || n > w->size - w->pos) {
		w->overflow = 1;
		return;
	}
	memset(w->p + w->pos, 0, (size_t)n);
	w->pos += n;
}

static inline void Net2W_U8(Net2Writer *w, uint32_t v) {
	uint8_t b = (uint8_t)v;
	Net2W_Bytes(w, &b, 1);
}

static inline void Net2W_U16(Net2Writer *w, uint32_t v) {
	uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
	Net2W_Bytes(w, b, 2);
}

static inline void Net2W_U32(Net2Writer *w, uint32_t v) {
	uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
	Net2W_Bytes(w, b, 4);
}

/* byte reader: a read past the end sets bad and returns zeros (or NULL) */
typedef struct Net2Reader {
	const uint8_t *p;
	int size;
	int pos;
	int bad;
} Net2Reader;

static inline void Net2R_Init(Net2Reader *r, const uint8_t *p, int size) {
	r->p = p;
	r->size = size;
	r->pos = 0;
	r->bad = 0;
}

static inline int Net2R_Left(const Net2Reader *r) {
	return r->bad ? 0 : r->size - r->pos;
}

static inline const uint8_t *Net2R_Bytes(Net2Reader *r, int n) {
	if (r->bad || n < 0 || n > r->size - r->pos) {
		r->bad = 1;
		return NULL;
	}
	const uint8_t *p = r->p + r->pos;
	r->pos += n;
	return p;
}

static inline uint32_t Net2R_U8(Net2Reader *r) {
	const uint8_t *p = Net2R_Bytes(r, 1);
	return p ? p[0] : 0;
}

static inline uint32_t Net2R_U16(Net2Reader *r) {
	const uint8_t *p = Net2R_Bytes(r, 2);
	return p ? (uint32_t)p[0] | (uint32_t)p[1] << 8 : 0;
}

static inline uint32_t Net2R_U32(Net2Reader *r) {
	const uint8_t *p = Net2R_Bytes(r, 4);
	return p ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24 : 0;
}

/* wrap safe order of 16 bit ids */
static inline int Net2_IdDiff(uint16_t a, uint16_t b) {
	return (int16_t)(uint16_t)(a - b);
}

/*
 * Packet.c: the received packet numbers of a connection, used both as the replay window and for the acks.
 * top is the newest packet (0: none yet), bit i of bits is packet top - i.
 */
typedef struct Net2Window {
	uint32_t top;
	uint64_t bits;
} Net2Window;

int Net2Win_Check(const Net2Window *w, uint32_t seq);	/* 1: new and not older than the window */
void Net2Win_Mark(Net2Window *w, uint32_t seq);
uint32_t Net2Win_AckBits(const Net2Window *w);

typedef struct Net2Chunk {
	int type;						/* N2K_* */
	int channel;
	uint16_t id;					/* reliable id or sequenced number */
	int index, count;				/* fragment */
	int reason;						/* disconnect */
	const uint8_t *data;
	int size;
} Net2Chunk;

typedef struct Net2Acks {
	uint32_t ack, bits;
	uint32_t delay;
} Net2Acks;

void Net2Pkt_WriteAcks(Net2Writer *w, const Net2Acks *a);
int Net2Pkt_ReadAcks(Net2Reader *r, Net2Acks *a);
int Net2Pkt_ChunkSize(const Net2Chunk *c);	/* bytes on the wire */
int Net2Pkt_WriteChunk(Net2Writer *w, const Net2Chunk *c);	/* 0 when it does not fit (nothing written) */
int Net2Pkt_ReadChunk(Net2Reader *r, Net2Chunk *c);	/* 1 a chunk, 0 the end, -1 malformed */
int Net2Pkt_CheckChunks(const uint8_t *p, int size, int numChannels, const uint8_t *kinds);	/* 1 valid */
void Net2Pkt_WriteHeader(uint8_t *hdr, uint32_t connId, uint32_t seq);

/* Crypto.c */
int Net2_Random(void *buf, size_t size);	/* Sock.c: 1 ok */
void Net2Crypto_Seal(uint8_t *out, const uint8_t key[32], int toClient, uint32_t seq, const uint8_t *hdr,
	const uint8_t *plain, int size);	/* out: size bytes cipher text and the tag */
int Net2Crypto_Open(uint8_t *plain, const uint8_t key[32], int toClient, uint32_t seq, const uint8_t *hdr,
	const uint8_t *cipher, int size);	/* size without the tag; 1 ok */
void Net2Crypto_Keys(uint8_t c2s[32], uint8_t s2c[32], const uint8_t shared[32], const uint8_t clientPk[32],
	const uint8_t serverPk[32]);
int Net2Crypto_Shared(uint8_t shared[32], const uint8_t sk[32], const uint8_t pk[32]);	/* 0: low order point */
void Net2Crypto_Cookie(uint8_t cookie[N2_COOKIE], const uint8_t secret[32], uint32_t period, const Net2Addr *a,
	const uint8_t salt[N2_SALT], const uint8_t clientPk[32]);
void Net2Crypto_Transcript(uint8_t hash[32], const uint8_t salt[N2_SALT], uint32_t connId,
	const uint8_t clientPk[32], const uint8_t serverPk[32]);
void Net2Crypto_Wipe(void *p, size_t size);

/* Sock.c: sockets are uintptr_t (a SOCKET of Winsock is UINT_PTR, a descriptor elsewhere) */
#define N2_NOSOCK UINTPTR_MAX
int Net2Sock_Startup(void);
void Net2Sock_Cleanup(void);
uintptr_t Net2Sock_Open(int family, const Net2Addr *at, uint16_t port);
void Net2Sock_Close(uintptr_t s);
int Net2Sock_Send(uintptr_t s, const Net2Addr *to, const uint8_t *data, int size);
int Net2Sock_Recv(uintptr_t s, Net2Addr *from, uint8_t *buf, int cap);	/* >0 size, 0 none */
uint16_t Net2Sock_Port(uintptr_t s);
void Net2_AddrKey(const Net2Addr *a, uint8_t out[20]);	/* family, port, ip: the cookie input */

/* Reliable.c: the reliable messages of one channel */
typedef struct Net2RelMsg {
	uint16_t id;
	uint8_t count;					/* fragments 1..NET2_MAX_FRAGS */
	uint8_t pad;
	uint32_t size;
	uint64_t acked;					/* bit per fragment */
	uint64_t sent[];				/* count send times (0: never), then size bytes of data */
} Net2RelMsg;

typedef struct Net2RelSend {
	uint16_t base;					/* oldest id not acked */
	uint16_t next;					/* id of the next new message */
	Net2RelMsg *slot[N2_WINDOW];	/* by id % N2_WINDOW */
} Net2RelSend;

typedef struct Net2RelSlot {
	uint8_t *data;					/* NULL: empty */
	uint32_t size;					/* known when the last fragment came */
	uint32_t cap;					/* bytes allocated */
	uint64_t have;					/* fragments received */
	uint16_t id;
	uint8_t count;
	uint8_t pad;
} Net2RelSlot;

typedef struct Net2RelRecv {
	uint16_t expect;				/* next id to deliver */
	Net2RelSlot slot[N2_WINDOW];
} Net2RelRecv;

typedef struct Net2Host Net2Host;
typedef struct Net2Conn Net2Conn;

int Net2Rel_Queue(Net2Host *h, Net2Conn *c, int channel, const uint8_t *data, int size);
void Net2Rel_Acked(Net2Host *h, Net2Conn *c, int channel, uint16_t id, int frag);
int Net2Rel_Recv(Net2Host *h, Net2Conn *c, const Net2Chunk *ch);	/* 0: over the limits */
int Net2Rel_Ready(Net2Conn *c, int channel);	/* 1: the next message is complete */
uint8_t *Net2Rel_Take(Net2Conn *c, int channel, int *size);	/* the caller frees it */
void Net2Rel_FreeAll(Net2Host *h, Net2Conn *c);

/* Conn.c / Host.c */
typedef struct Net2RelRef {
	uint16_t id;
	uint8_t channel;
	uint8_t frag;
} Net2RelRef;

typedef struct Net2SentPkt {
	uint64_t time;
	uint32_t seq;
	uint16_t size;
	uint8_t acked;
	uint8_t numRefs;
	Net2RelRef refs[N2_PKT_REFS];
} Net2SentPkt;

typedef struct Net2Channel {
	uint8_t kind;
	uint16_t sendSeq;				/* sequenced: last number sent */
	uint16_t recvSeq;				/* sequenced: last number delivered */
	int haveRecvSeq;
	Net2RelSend *rs;
	Net2RelRecv *rr;
} Net2Channel;

/* connection states */
#define N2C_FREE 0
#define N2C_HELLO 1					/* client: sends HELLO */
#define N2C_CONNECT 2				/* client: has the cookie, sends CONNECT */
#define N2C_ACCEPTED 3				/* server: sent ACCEPT, waits for the first DATA */
#define N2C_CONNECTED 4
#define N2C_CLOSING 5				/* sends the disconnect chunk */
#define N2C_CLOSED 6				/* waits for Net2_Receive to report the disconnect */

struct Net2Conn {
	int state;
	int index;
	uint32_t id;
	Net2Addr addr;
	uint8_t sendKey[32], recvKey[32];
	uint8_t clientPk[32];			/* client public key of the handshake */
	uint8_t secret[32];				/* client: its ephemeral secret until ACCEPT */
	uint8_t salt[N2_SALT];
	uint8_t cookie[N2_COOKIE];		/* client: from the CHALLENGE */
	uint8_t serverKey[NET2_KEY_SIZE];	/* client: the server's identity */
	uint8_t accept[N2_ACCEPT_SIZE];	/* server: sent again for a repeated CONNECT */
	uint64_t startTime, lastRecv, lastSend, lastHandshake;
	uint64_t cookieTime;			/* client: when the CHALLENGE came */

	uint32_t sendSeq;				/* last packet number sent */
	Net2Window recv;
	uint64_t recvTopTime;			/* when recv.top came (ack delay) */
	int recvUnacked;				/* packets received since the last sent packet */
	uint64_t firstUnacked;
	uint32_t ackTop;				/* newest own packet the peer acked */
	Net2SentPkt *sent;				/* N2_SENT_RING */
	int haveRtt;
	float srtt, rttVar;
	uint64_t lossBits[4];			/* lost (1) or acked (0) of the last 256 packets out of the ack window */
	int lossPos, lossCount, lossLost;
	uint32_t rate;
	int64_t tokens;
	uint64_t tokenTime;

	Net2Channel ch[NET2_MAX_CHANNELS];
	uint32_t pendingBytes;			/* reliable send data */
	uint32_t recvBytes;				/* reliable receive buffers */
	uint8_t *inbox;					/* records channel u8, size u16, data */
	uint32_t inboxRead, inboxWrite;
	uint8_t *outbox;				/* records channel u8, seq u16, size u16, data */
	uint32_t outboxUsed;

	int evConnect, evDisconnect;
	int reason;
	int closeSends;
	int forceSend;

	uint64_t packetsSent, packetsRecv, packetsLost, bytesSent, bytesRecv;
	uint32_t dropped, bad;
	uint64_t rateTime;
	uint32_t secSent, secRecv, sendRate, recvRate;
};

struct Net2Host {
	Net2Config cfg;
	uint8_t idSk[64], idPk[32];
	uint8_t cookieSecret[32];
	uint8_t pinKey[NET2_KEY_SIZE];
	int havePin;
	uintptr_t sock4, sock6;
	int sockStarted;				/* UDP sockets (no Net2Io of the game) */
	Net2Io io;
	Net2Conn *conns;
	int numConns;
	int cursor;						/* Net2_Receive goes round the connections */
	uint8_t *taken;					/* the message of the last Net2_Receive, freed by the next */
	Net2ServerInfo info;
	uint32_t queryToken;
	Net2Event events[N2_EVENTS];
	int evHead, evCount;
	uint64_t now;
};

void *Net2_Alloc(Net2Host *h, size_t size);	/* zeroed */
void Net2_Free(Net2Host *h, void *p);
int Net2_RawSend(Net2Host *h, const Net2Addr *to, const uint8_t *data, int size);

/* Conn.c */
int Net2Conn_Init(Net2Host *h, Net2Conn *c);
void Net2Conn_Release(Net2Host *h, Net2Conn *c);
void Net2Conn_Data(Net2Host *h, Net2Conn *c, const Net2Addr *from, const uint8_t *pkt, int size);
void Net2Conn_Flush(Net2Host *h, Net2Conn *c);
void Net2Conn_Close(Net2Host *h, Net2Conn *c, int reason, int notify);
int Net2Conn_Tick(Net2Host *h, Net2Conn *c);
void Net2Conn_Stats(const Net2Conn *c, Net2Stats *st);

/* Handshake.c */
void Net2Hs_ClientStart(Net2Host *h, Net2Conn *c);
void Net2Hs_ClientTick(Net2Host *h, Net2Conn *c);
void Net2Hs_Packet(Net2Host *h, const Net2Addr *from, const uint8_t *pkt, int size);

/* Lan.c */
int Net2Lan_WriteInfo(uint8_t *buf, int cap, uint32_t token, uint32_t time, const Net2ServerInfo *info);
int Net2Lan_ReadInfo(const uint8_t *pkt, int size, uint32_t *token, uint32_t *time, Net2ServerInfo *info);
void Net2Lan_Packet(Net2Host *h, const Net2Addr *from, const uint8_t *pkt, int size);

#endif
