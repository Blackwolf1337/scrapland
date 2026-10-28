/*
 * The All-Seeing Eye server side (the ASE SDK linked into Scrap.exe at rva 0x002070..0x002eb0), written from the
 * public description of its UDP protocol and the packets the original sends, not from its code.
 *
 * A UDP socket on the query port (the game port + 123 unless given, the next free one of up to 64 when it is
 * taken) answers:
 *   's'  "EYE1", then strings of a length byte (its length + 1) and the text: the game name, the game port, the
 *        server status (name, type, map, version, password, players, max players), the rules (key, value pairs),
 *        the byte 1, then the players (a byte of flags for the fields that follow: name 1, team 2, skin 4,
 *        score 8, ping 16, time 32). The game fills them in its callbacks (Net_AseStatus, Net_AseRules,
 *        Net_AsePlayers), which call Ase_ServerStatus, Ase_AddRule and Ase_AddPlayer. An answer of 0x554 bytes or
 *        more goes in packets of 0x54c bytes, "EYE1", "EYE2" ... in front and a 4 byte packet id behind each.
 *   'g'  'G' and the game name.
 *   'p'  (shorter than 20 bytes, with the internet flag) the same packet with 'P' in front: the ping of the master.
 *   'v'  the validation of the master server; it was only answered to the master's address.
 * The masters (master.udpsoft.com, master2.udpsoft.com) are gone. Resolving them would block (gethostbyname),
 * so this version sends no heartbeat ("\hb\<query port>\scrapland" to UDP port 27900) and so never answers 'v'.
 * Differs from the original on purpose: no master lookup and no heartbeat.
 */
#include <string.h>

#include "scrap/net.h"

#define ASE_GAME "scrapland"
#define ASE_QUERY_OFFSET 123
#define ASE_BIND_TRIES 64
#define ASE_BUFFER 0x1800
#define ASE_RECV 0x1000
#define ASE_STRING_MAX 254
#define ASE_SPLIT 0x554		/* answers from this size on are split */
#define ASE_PART 0x54c		/* the data of one part */
#define ASE_PING_MAX 20

#define AF_INET 2
#define SOCK_DGRAM 2
#define INVALID_SOCKET 0xffffffffu
#define FIONBIO 0x8004667e

typedef struct {
	uint16_t family;
	uint16_t port;			/* network order */
	uint32_t addr;			/* network order */
	byte zero[8];
} aseAddr_t;
SD_SIZE(aseAddr_t, 16);

/* fd_set of Winsock 1.1 with one socket, and a zero timeval */
typedef struct {
	uint32_t count;
	uint32_t sock;
} aseFdSet_t;

static struct {
	int init;
	uint32_t sock;
	int poll;			/* ioctlsocket failed: select before each recvfrom */
	int internet;
	int gamePort;
	uint32_t partId;
	byte *at, *end;			/* the answer being written; end is NULL outside the callbacks */
	byte buf[ASE_BUFFER];
	byte recv[ASE_RECV];
} ase;

static void Ase_SetError(const char *text) {
	g_AseError = text;
}

/* "a.b.c.d" in network order, 0 for anything else */
static uint32_t Ase_ParseAddress(const char *s) {
	uint32_t addr = 0;

	for (int i = 0; i < 4; i++) {
		uint32_t v = 0;
		int digits = 0;
		for (; *s >= '0' && *s <= '9' && digits < 4; s++, digits++) {
			v = v * 10 + (uint32_t)(*s - '0');
		}
		if (!digits || v > 255 || (i < 3 && *s++ != '.')) {
			return 0;
		}
		addr |= v << (8 * i);
	}
	return *s ? 0 : addr;
}

/* a string of the answer: its length + 1, then up to ASE_STRING_MAX characters; 0 when it does not fit */
static int Ase_PutString(const char *s) {
	size_t n = s ? strlen(s) : 0;

	if (n > ASE_STRING_MAX) {
		n = ASE_STRING_MAX;
	}
	if (!ase.end || ase.at + 1 + n > ase.end) {
		return 0;
	}
	*ase.at++ = (byte)(n + 1);
	memcpy(ase.at, s ? s : "", n);
	ase.at += n;
	return 1;
}

static int Ase_PutInt(int v) {
	char s[12];
	char *p = s + sizeof(s) - 1;
	uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;

	*p = 0;
	do {
		*--p = (char)('0' + u % 10);
		u /= 10;
	} while (u);
	if (v < 0) {
		*--p = '-';
	}
	return Ase_PutString(p);
}

void Ase_Shutdown(void) {
	if (ase.init) {
		Wsock_closesocket(ase.sock);
	}
	ase.init = 0;
}

/*
 * The socket of the query port: port + 123 (the game port). With an address it binds there, after 32 taken ports
 * on any address from the first port again. internet: the master may ping (the heartbeat itself is not sent).
 */
static int Ase_Start(int port, int internet, const char *address, int queryPort) {
	aseAddr_t sa = { AF_INET, 0, 0, { 0 } };
	uint32_t on = 1;
	int tries;

	if (ase.init) {
		Wsock_closesocket(ase.sock);
		ase.init = 0;
	}
	ase.gamePort = port;
	ase.internet = internet;
	if (address) {
		sa.addr = Ase_ParseAddress(address);
	}
	if (!queryPort) {
		queryPort = port + ASE_QUERY_OFFSET;
	}
	ase.sock = Wsock_socket(AF_INET, SOCK_DGRAM, 0);
	if (ase.sock == INVALID_SOCKET) {
		Ase_SetError("socket failed");
		return 0;
	}
	int bound = queryPort;
	for (tries = 0; tries < ASE_BIND_TRIES; tries++, bound++) {
		if (tries == ASE_BIND_TRIES / 2 && sa.addr) {
			sa.addr = 0;
			bound = queryPort;
		}
		sa.port = Wsock_htons((uint16_t)bound);
		if (!Wsock_bind(ase.sock, &sa, sizeof(sa))) {
			break;
		}
	}
	if (tries == ASE_BIND_TRIES) {
		Wsock_closesocket(ase.sock);
		Ase_SetError("bind failed");
		return 0;
	}
	ase.poll = Wsock_ioctlsocket(ase.sock, (int32_t)FIONBIO, &on) != 0;
	ase.init = 1;
	return 1;
}

int Ase_Initialize(int port, int internet, const char *address) {
	return Ase_Start(port, internet, address, 0);
}

/* ASEQuery_wantstatus: the server status, the first strings after the game name and port */
void Ase_ServerStatus(const char *name, const char *type, const char *map, const char *version, int password,
	int numPlayers, int maxPlayers) {
	if (Ase_PutString(name) && Ase_PutString(type) && Ase_PutString(map) && Ase_PutString(version)
		&& Ase_PutInt(password) && Ase_PutInt(numPlayers)) {
		Ase_PutInt(maxPlayers);
	}
}

/* ASEQuery_wantrules: one rule */
void Ase_AddRule(const char *key, const char *value) {
	byte *at = ase.at;

	if (!Ase_PutString(key) || !Ase_PutString(value)) {
		ase.at = at;
	}
}

/* ASEQuery_wantplayers: one player, the fields that are not NULL */
void Ase_AddPlayer(const char *name, const char *team, const char *skin, const char *score, const char *ping,
	const char *time) {
	const char *field[6] = { name, team, skin, score, ping, time };
	byte *at = ase.at, flags = 0;

	if (!ase.end || ase.at >= ase.end) {
		return;
	}
	ase.at++;
	for (int i = 0; i < 6; i++) {
		if (field[i]) {
			if (!Ase_PutString(field[i])) {
				ase.at = at;
				return;
			}
			flags |= (byte)(1 << i);
		}
	}
	if (!flags) {
		ase.at = at;
		return;
	}
	*at = flags;
}

static void Ase_Send(const void *data, int len, const aseAddr_t *to) {
	Wsock_sendto(ase.sock, data, len, 0, to, sizeof(*to));
}

/* the answer to 's': at once when it is small, else in parts with "EYEn" in front and the packet id behind */
static void Ase_SendStatus(const aseAddr_t *to) {
	byte *p = ase.buf;
	int left = (int)(ase.at - ase.buf);

	if (left < ASE_SPLIT) {
		Ase_Send(ase.buf, left, to);
		return;
	}
	ase.partId = ase.partId * 1103515245u + 12345u + to->addr;
	while (left > 4) {
		int n = left < ASE_PART + 4 ? left : ASE_PART + 4;
		byte keep[4];
		/* the buffer has room behind the last part: ASE_BUFFER is beyond any answer the callbacks may write */
		memcpy(keep, p + n, 4);
		memcpy(p + n, &ase.partId, 4);
		Ase_Send(p, n + 4, to);
		memcpy(p + n, keep, 4);
		byte part = p[3];
		p += ASE_PART;
		left -= ASE_PART;
		memcpy(p, "EYE", 3);
		p[3] = (byte)(part + 1);
		if (left == 4) {
			/* only the header is left: it goes with a zero byte */
			p[4] = 0;
			left = 5;
		}
	}
}

static void Ase_Answer(int len, const aseAddr_t *from) {
	switch (ase.recv[0]) {
	case 'p':
		if (len < ASE_PING_MAX && ase.internet) {
			ase.recv[0] = 'P';
			Ase_Send(ase.recv, len, from);
		}
		break;
	case 's':
		memcpy(ase.buf, "EYE1", 4);
		ase.at = ase.buf + 4;
		/* 4 bytes stay free for the packet id behind the last part */
		ase.end = ase.buf + sizeof(ase.buf) - 4;
		Ase_PutString(ASE_GAME);
		Ase_PutInt(ase.gamePort);
		Net_AseStatus();
		Net_AseRules();
		if (ase.at < ase.end) {
			*ase.at++ = 1;
		}
		Net_AsePlayers();
		ase.end = NULL;
		Ase_SendStatus(from);
		break;
	case 'g':
		ase.buf[0] = 'G';
		memcpy(ase.buf + 1, ASE_GAME, sizeof(ASE_GAME) - 1);
		Ase_Send(ase.buf, (int)sizeof(ASE_GAME), from);
		break;
	default:
		/* 'v' comes only from the master, which is gone; anything else has no answer */
		break;
	}
}

/* the queries that came since the last call, answered; 0 when not initialized */
int Ase_Check(void) {
	static const uint32_t zero[2] = { 0, 0 };

	if (!ase.init) {
		Ase_SetError("not initialized");
		return 0;
	}
	for (;;) {
		aseAddr_t from;
		int fromLen = sizeof(from);
		if (ase.poll) {
			aseFdSet_t set = { 1, ase.sock };
			if (Wsock_select((int)ase.sock + 1, &set, NULL, NULL, zero) <= 0) {
				break;
			}
		}
		int len = Wsock_recvfrom(ase.sock, ase.recv, sizeof(ase.recv), 0, &from, &fromLen);
		if (len <= 0) {
			break;
		}
		Ase_Answer(len, &from);
	}
	return 1;
}
