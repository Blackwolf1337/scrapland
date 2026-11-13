/*
 * Net/transport.cpp of the game: the UDP socket of the server, the client or the browser (Net/Sock.c),
 * the address table of the clients, the ping list of the browser, the master server and the encryption of every
 * packet (ChaCha20 with a random nonce, Poly1305 tag over nonce, data and lengths). See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/net.h"
#include "scrap/platform.h"
#include "scrap/staticinit.h"

#define g_LastMasterCommandFlags (GVar_LastMasterCommandSent.base.flags)
#define g_LastMasterCommand (((cString *)&GVar_LastMasterCommandSent.value))	/* LastMasterCommandSent */
static cGVar GVar_LastMasterCommandSent;
#define NET_AF_INET 2
#define NET_PKT_MAX 0x800

/* the address and the name of the last Transport_Resolve */
static uint32_t net_hostAddr;
static char net_hostName[256];

/* the fixed key of every packet: 2, 4, 6, ... */
static void Transport_Key(byte *key) {
	for (int i = 0; i < 0x20; i++) {
		key[i] = (byte)((i + 1) * 2);
	}
}

static void Transport_SetAddr(NetAddr *a, uint16_t port) {
	memset(a, 0, sizeof(*a));
	a->family = NET_AF_INET;
	a->port = Net_Htons(port);
	a->addr = net_hostAddr;
}

/* transport.cpp */
static uint32_t g_NetSocket;

static void Transport_CloseSocket(void) {
	if (Sock_Close(g_NetSocket)) {
		Con_Print(0x8f008f, "Can't close socket!", 0);
	}
	Sock_Cleanup();
	g_NetSocket = 0;
}

int g_NetStatRecvSlot;
int g_NetStatRecv;
static int g_NetMode;
static NetPingServer *g_NetPingServers;

/* transport.cpp (src/game/Net/Transport.c) */
static int Transport_PingFreeSlot(void);
static int Transport_SendTo(uint32_t s, const void *data, int len, int flags, const NetAddr *to, int tolen);
static int Transport_PingServer(const NetAddr *addr) {
	if (g_NetMode != NETMODE_BROWSER) {
		return 0;
	}
	/* a packet of sequence 0x7f with the message NETMSG_PING */
	byte pkt[5] = { 0x7f, 1, 0, 0, NETMSG_PING };
	Transport_SendTo(g_NetSocket, pkt, 5, 0, addr, sizeof(NetAddr));
	g_NetStatRecvSlot = 0;
	g_NetStatRecv = 5;
	CHECK(g_NetPingServers != NULL);
	int i = Transport_PingFreeSlot();
	g_NetPingServers[i].addr = *addr;
	g_NetPingServers[i].time = Plat_Ticks();
	return 1;
}

static int g_NetMasterOn;

int Transport_CloseBrowser(void) {
	if (g_NetMode != NETMODE_BROWSER) {
		return 0;
	}
	Mem_Free(g_NetPingServers);
	g_NetPingServers = NULL;
	g_NetMode = NETMODE_NONE;
	g_NetMasterOn = 0;
	Transport_CloseSocket();
	return 1;
}

void Transport_CloseClient(void) {
	g_NetMode = NETMODE_NONE;
	g_NetMasterOn = 0;
	Transport_CloseSocket();
}

static NetAddr *g_NetClientAddrs;

void Transport_Close(void) {
	g_NetMode = NETMODE_NONE;
	g_NetMasterOn = 0;
	Mem_Free(g_NetClientAddrs);
	Transport_CloseSocket();
}

static int g_NetNumClients;

int Transport_RemoveClient(int slot) {
	if (!g_NetClientAddrs[slot].addr) {
		return 0;
	}
	g_NetNumClients--;
	memset(&g_NetClientAddrs[slot], 0, sizeof(NetAddr));
	return 1;
}

int Net_GetSendRate(void) {
	return g_NetNumClients;
}

static int Transport_PingFreeSlot(void) {
	CHECK(g_NetPingServers != NULL);
	uint32_t oldest = g_NetPingServers[0].time;
	int found = 0;
	for (int i = 0; i < NET_PING_SERVERS; i++) {
		uint32_t t = g_NetPingServers[i].time;
		if (!t) {
			return i;
		}
		if (oldest > t) {
			oldest = t;
			found = i;
		}
	}
	return found;
}

static byte g_NetServerFullPkt[5] = { 0, 0, 0, 0, 8 };

static byte g_NetNeedPasswordPkt[5] = { 0, 0, 0, 0, 9 };

static int Transport_FindClient(const NetAddr *addr);
static int Transport_MasterPacket(const byte *buf, int len);
static int Transport_PingTime(const NetAddr *addr);
static int Transport_RecvFrom(uint32_t s, byte *buf, int len, int flags, NetAddr *from, int *fromlen);
int Transport_Recv(byte *buf, int *len) {
	NetAddr from;
	int fromlen;
	NetServerInfo info;
	char ip[0x20];

	switch (g_NetMode) {
	case NETMODE_SERVER:
		fromlen = sizeof(from);
		memset(&from, 0, sizeof(from));
		*len = Transport_RecvFrom(g_NetSocket, buf, *len, 0, &from, &fromlen);
		if (Transport_MasterPacket(buf, *len)) {
			*len = 0;
			return -1;
		}
		/* a ping of a browser: the server info */
		if (*len > 4 && (buf[0] & 0x7f) == 0x7f && !(buf[3] & 0x7f) && buf[4] == NETMSG_PING &&
			(buf[1] || buf[2])) {
			NetServer_GetInfo(&info);
			Transport_SendTo(g_NetSocket, &info, sizeof(info), 0, &from, sizeof(from));
			*len = 0;
			return -1;
		}
		if (*len <= 0) {
			return -1;
		}
		g_NetStatSentSlot = Transport_FindClient(&from);
		g_NetStatSent = *len;
		if (g_NetStatSentSlot == -1) {
			Transport_SendTo(g_NetSocket, g_NetServerFullPkt, 5, 0, &from, sizeof(from));
		}
		return g_NetStatSentSlot;
	case NETMODE_CLIENT:
		*len = Transport_RecvFrom(g_NetSocket, buf, *len, 0, NULL, NULL);
		if (Transport_MasterPacket(buf, *len)) {
			*len = 0;
			return *len > 0 ? 0 : -1;
		}
		if (*len > 0) {
			g_NetStatSentSlot = 0;
			g_NetStatSent = *len;
			if ((uint32_t)*len >= 5) {
				if (!memcmp(g_NetServerFullPkt, buf, 4) && g_NetServerFullPkt[4] == buf[4]) {
					*len = -1;
					if (cString_CmpStr(g_ClientConnectionStatus, "server_is_full")) {
						cString_Set(g_ClientConnectionStatus, "server_is_full");
					}
				} else if (!memcmp(g_NetNeedPasswordPkt, buf, 4) && g_NetNeedPasswordPkt[4] == buf[4]) {
					*len = -1;
					if (cString_CmpStr(g_ClientConnectionStatus, "need_password")) {
						cString_Set(g_ClientConnectionStatus, "need_password");
					}
				}
			}
		}
		return *len > 0 ? 0 : -1;
	case NETMODE_BROWSER:
		fromlen = sizeof(from);
		memset(&from, 0, sizeof(from));
		*len = Transport_RecvFrom(g_NetSocket, buf, *len, 0, &from, &fromlen);
		if (Transport_MasterPacket(buf, *len)) {
			*len = 0;
		} else if (*len > 0 && *len == (int)sizeof(NetServerInfo)) {
			const byte *a = (const byte *)&from.addr;
			crt_sprintf(ip, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
			int ping = Transport_PingTime(&from);
			NetData_OnBrowse(ip, ((const NetServerInfo *)buf)->port, (const NetServerInfo *)buf, ping);
		}
		*len = -1;
		return -1;
	default:
		return -1;
	}
}

static int g_NetMaxClients;

static int Transport_FindClient(const NetAddr *addr) {
	static const byte zero[sizeof(NetAddr)];
	int found = -1;

	for (int i = 0; i < g_NetMaxClients; i++) {
		NetAddr *c = &g_NetClientAddrs[i];
		if (addr->port == c->port && addr->addr == c->addr && addr->family == c->family) {
			return i;
		}
		if (found == -1 && !memcmp(zero, c, sizeof(NetAddr))) {
			found = i;
		}
	}
	if (found != -1) {
		g_NetNumClients++;
		g_NetClientAddrs[found] = *addr;
	}
	return found;
}

int Transport_GetMaxClients(void) {
	return g_NetMaxClients;
}

int Transport_GetMode(void) {
	return g_NetMode;
}

static uint32_t g_NetBrowseStart;

static int Transport_PingTime(const NetAddr *addr) {
	CHECK(g_NetPingServers != NULL);
	for (int i = 0; i < NET_PING_SERVERS; i++) {
		NetPingServer *p = &g_NetPingServers[i];
		if (!memcmp(p, addr, sizeof(NetAddr))) {
			uint32_t t = p->time;
			int d = (int)(Plat_Ticks() - t);
			p->time = t + d;
			return d;
		}
	}
	return (int)(Plat_Ticks() - g_NetBrowseStart);
}

static NetAddr g_NetLocalAddr;

uint16_t Transport_GetLocalPort(void) {
	return Net_Htons(g_NetLocalAddr.port);
}

static NetAddr g_NetMasterAddr;

static int Transport_Startup(void);
int Transport_SetMaster(const char *address, uint16_t port) {
	if (!*address || !port || !Transport_Startup() || !Transport_Resolve(address, NULL, NULL)) {
		return 0;
	}
	memset((&g_NetMasterAddr), 0, sizeof(NetAddr));
	g_NetMasterAddr.family = NET_AF_INET;
	g_NetMasterAddr.port = Net_Htons(port);
	g_NetMasterOn = 1;
	g_NetMasterAddr.addr = net_hostAddr;
	return 1;
}

static NetAddr g_NetServerAddr;

int Transport_InitBrowser(const char *address, uint16_t port) {
	if (g_NetMode != NETMODE_NONE || !Transport_Startup() || !Transport_Resolve(address, NULL, NULL)) {
		return 0;
	}
	Transport_SetAddr((&g_NetServerAddr), port);
	g_NetMode = NETMODE_BROWSER;
	CHECK(g_NetPingServers == NULL);
	g_NetPingServers = Mem_Alloc(NET_PING_SERVERS * sizeof(NetPingServer));
	memset(g_NetPingServers, 0, NET_PING_SERVERS * sizeof(NetPingServer));
	g_NetBrowseStart = 0;
	Transport_SetMaster(g_MasterServerAddress, (uint16_t)g_MasterServerPort);
	return 1;
}

int Transport_InitClient(const char *address, uint16_t port) {
	if (!Transport_Startup() || !Transport_Resolve(address, NULL, NULL)) {
		return 0;
	}
	Transport_SetAddr((&g_NetServerAddr), port);
	g_NetMode = NETMODE_CLIENT;
	return 1;
}

int Transport_InitServer(int maxClients, uint16_t port) {
	if (!Transport_Startup()) {
		return 0;
	}
	memset((&g_NetLocalAddr), 0, sizeof(NetAddr));
	g_NetLocalAddr.family = NET_AF_INET;
	memset((&g_NetServerAddr), 0, sizeof(NetAddr));
	g_NetLocalAddr.port = Net_Htons(port);
	g_NetLocalAddr.addr = 0;
	if (Sock_Bind(g_NetSocket, (&g_NetLocalAddr))) {
		return 0;
	}
	g_NetMaxClients = maxClients;
	g_NetMode = NETMODE_SERVER;
	g_NetNumClients = 0;
	g_NetClientAddrs = Mem_Alloc(maxClients << 4);
	memset(g_NetClientAddrs, 0, g_NetMaxClients << 4);
	return 1;
}

static int Transport_Startup(void) {
	if (g_NetSocket) {
		return 1;
	}
	if (!Sock_Startup()) {
		return 0;
	}
	g_NetSocket = Sock_OpenUdp();
	return g_NetSocket != 0;
}

int Net_PingInetSvrs(void) {
	if (!g_NetMasterOn || g_NetMode != NETMODE_BROWSER) {
		return 0;
	}
	for (int i = 0; i < NET_PING_SERVERS; i++) {
		NetPingServer *p = &g_NetPingServers[i];
		if (p->addr.port) {
			NetAddr a = p->addr;
			memset(&p->addr, 0, sizeof(NetAddr));
			p->time = 0;
			Transport_PingServer(&a);
		}
	}
	return 1;
}

netmastercmd_t g_MasterCommandCallback;

static byte cGVar_SetString(cGVar *self, const char *s);
static int Transport_MasterPacket(const byte *buf, int len) {
	if (!g_NetMasterOn) {
		return 0;
	}
	if (len <= 6 || g_NetMode != NETMODE_BROWSER || buf[0] || buf[1] || buf[2] || buf[3]) {
		return 0;
	}
	char c = (char)buf[4];
	if (c == '|') {
		/* "<address>:<port>": the master moved */
		char addr[0x100], port[8];
		int p = 5000, state = 0;
		uint32_t j = 0;
		memset(addr, 0, sizeof(addr));
		memset(port, 0, sizeof(port));
		for (int i = 5; i < len && state < 2; i++) {
			char ch = (char)buf[i];
			if (state == 0) {
				if (!ch) {
					if (j >= sizeof(addr)) {
						crt_report_rangecheckfailure();
					}
					addr[j] = 0;
					state = 2;
				} else if (ch == ':') {
					if (j >= sizeof(addr)) {
						crt_report_rangecheckfailure();
					}
					addr[j] = 0;
					state = 1;
					j = 0;
				} else {
					if (j >= sizeof(addr)) {
						crt_report_rangecheckfailure();
					}
					addr[j++] = ch;
				}
			} else {
				if (j >= sizeof(port)) {
					crt_report_rangecheckfailure();
				}
				if (!ch) {
					port[j] = 0;
					state = 2;
				} else {
					port[j++] = ch;
				}
			}
		}
		if (strlen(port)) {
			p = crt_atoi(port);
		}
		cGVar_SetString((&g_GVarMasterServerAddress), addr);
		int mp = g_MasterServerPort;
		if (!(g_MasterServerPortFlags & 1)) {
			mp = p;
		}
		g_MasterServerPort = mp;
		Transport_SetMaster(g_MasterServerAddress, (uint16_t)mp);
		Net_SendMasterString(NULL);
	} else if (c == '}') {
		/* the server list: 4 bytes of address and the port for each */
		if (len < 11) {
			return 1;
		}
		const byte *p = buf + 6;
		do {
			NetAddr a;
			memset(&a, 0, sizeof(a));
			a.family = NET_AF_INET;
			memcpy(&a.addr, p - 1, 4);
			uint16_t port;
			memcpy(&port, p + 3, 2);
			a.port = Net_Htons(port);
			if (!a.addr) {
				break;
			}
			const byte *ab = (const byte *)&a.addr;
			Con_Debugf("IP From Master : %3d.%3d.%3d.%3d:%8d\n", ab[0], ab[1], ab[2], ab[3], Net_Htons(a.port));
			Transport_PingServer(&a);
			p += 6;
		} while ((int)(p - buf) + 5 <= len);
	} else if (c == '~') {
		Con_Debugf("From master:'%s'\n", (const char *)buf + 5);
		if (g_MasterCommandCallback) {
			g_MasterCommandCallback((const char *)buf + 5);
		}
	} else {
		return 0;
	}
	return 1;
}

int Transport_SendNeedPassword(int slot) {
	NetAddr *a = &g_NetClientAddrs[slot];

	if (!a->addr) {
		return 0;
	}
	Transport_SendTo(g_NetSocket, g_NetNeedPasswordPkt, 5, 0, a, sizeof(NetAddr));
	g_NetStatRecvSlot = slot;
	g_NetStatRecv = 5;
	return 1;
}

const char *Transport_Resolve(const char *address, char *ip, uint32_t *addr) {
	Transport_Startup();
	if (!Sock_Resolve(address, &net_hostAddr, net_hostName, sizeof(net_hostName))) {
		return NULL;
	}
	if (ip) {
		const byte *h = (const byte *)&net_hostAddr;
		crt_sprintf(ip, "%d.%d.%d.%d", h[0], h[1], h[2], h[3]);
	}
	if (addr) {
		*addr = net_hostAddr;
	}
	return net_hostName;
}

int Transport_PingMaster(void) {
	g_NetBrowseStart = Plat_Ticks();
	return Transport_PingServer((&g_NetServerAddr));
}

int Transport_SendToClient(const void *data, int size, int slot) {
	NetAddr *a = &g_NetClientAddrs[slot];

	if (!a->addr) {
		return 0;
	}
	Transport_SendTo(g_NetSocket, data, size, 0, a, sizeof(NetAddr));
	g_NetStatRecv = size;
	g_NetStatRecvSlot = slot;
	return 1;
}

void Transport_SendToServer(const void *data, int size) {
	Transport_SendTo(g_NetSocket, data, size, 0, (&g_NetServerAddr), sizeof(NetAddr));
	g_NetStatRecv = size;
	g_NetStatRecvSlot = 0;
}

int Net_SendMasterString(const char *text) {
	if (!g_NetMasterOn) {
		return 0;
	}
	if (text && !(g_LastMasterCommandFlags & 1)) {
		cString_Set(g_LastMasterCommand, text);
	}
	const char *s = *g_LastMasterCommand;
	if (!*s) {
		return 0;
	}
	Transport_SendTo(g_NetSocket, s, (int)strlen(s) + 1, 0, (&g_NetMasterAddr), sizeof(NetAddr));
	if (!text && !(g_LastMasterCommandFlags & 1)) {
		cString_Set(g_LastMasterCommand, "");
	}
	return 1;
}

static byte cGVar_SetString(cGVar *self, const char *s) {
	if (self->base.flags & 1) {
		return 0;
	}
	cString_Set(&self->value.s, s);
	return 1;
}

/*
 * differs from the original on purpose (no blocking waits): the original waited in a select until a packet came or
 * ms passed. This polls with a zero timeout and says whether that wait is over; the wait starts at the first call
 * after the last one that returned 1.
 */
int Transport_Wait(int ms) {
	static uint32_t start;
	static int waiting;

	if (g_NetMode != NETMODE_SERVER) {
		return 1;
	}
	uint32_t now = Plat_Ticks();
	if (!waiting) {
		waiting = 1;
		start = now;
	}
	if (now - start < (uint32_t)ms && !Sock_Readable(g_NetSocket)) {
		return 0;
	}
	waiting = 0;
	return 1;
}

int Net_IsMaster(void) {
	return g_NetMasterOn;
}

/* recvfrom of an encrypted packet: the plain length, -1 if none or the tag is wrong */
static int Transport_RecvFrom(uint32_t s, byte *buf, int len, int flags, NetAddr *from, int *fromlen) {
	byte pkt[NET_PKT_MAX], key[0x20], polyKey[0x20], nonce[12], mac[16];
	cChaCha ctx;
	cNetBuffer nb;

	(void)flags, (void)fromlen;
	int n = Sock_RecvFrom(s, pkt, NET_PKT_MAX, from);
	if (n < 0x30) {
		return -1;
	}
	cChaCha_ctor(&ctx);
	cNetBuffer_ctor(&nb, NET_PKT_MAX);
	memcpy(nonce, pkt, sizeof(nonce));
	Transport_Key(key);
	Transport_Key(polyKey);
	cChaCha_Crypt(&ctx, key, 0, nonce, polyKey, polyKey, sizeof(polyKey));
	Poly1305_Auth(mac, pkt, (uint32_t)(n - 16), polyKey);
	if (memcmp(pkt + n - 16, mac, 16)) {
		Con_Printf("Bad Poly1305\n");
		Net_DumpData("Poly1305 Key", polyKey, sizeof(polyKey));
		Net_DumpData("Poly1305 Calculated MAC", mac, sizeof(mac));
		Net_DumpData("Received Packet", pkt, n);
		cNetBuffer_dtor(&nb);
		return -1;
	}
	/* nonce, 4 zeros, the data, padding, then the two lengths (8 bytes each) and the tag */
	/* the length comes from the sender: it must fit the received ciphertext (n less the padded nonce, the two
	 * lengths and the tag) and the caller's buffer */
	uint32_t size;
	memcpy(&size, pkt + n - 0x18, 4);
	if (len < 0 || size > (uint32_t)(n - 0x30) || size > (uint32_t)len) {
		cNetBuffer_dtor(&nb);
		return -1;
	}
	cChaCha_Crypt(&ctx, key, 1, nonce, pkt + 0x10, buf, size);
	cNetBuffer_dtor(&nb);
	return (int)size;
}

static cNetRandom g_NetRandom;

static int Transport_SendTo(uint32_t s, const void *data, int len, int flags, const NetAddr *to, int tolen) {
	byte ct[NET_PKT_MAX], key[0x20], polyKey[0x20], nonce[12], mac[16];
	cChaCha ctx;
	cNetBuffer nb;

	/* ct holds the plain text: a longer message is dropped (the receiver's packet is NET_PKT_MAX with the padded
	 * nonce, the two lengths and the tag around the data) */
	if (len < 0 || len > NET_PKT_MAX - 0x30) {
		Con_Printf("Transport: message of %d bytes too long, dropped\n", len);
		return -1;
	}
	cChaCha_ctor(&ctx);
	cNetBuffer_ctor(&nb, NET_PKT_MAX);
	memset(nonce, 0, sizeof(nonce));
	memset(mac, 0, sizeof(mac));
	Transport_Key(key);
	Transport_Key(polyKey);
	uint64_t r = cNetRandom_Next((&g_NetRandom));
	memcpy(nonce, &r, 8);
	r = cNetRandom_Next((&g_NetRandom));
	memcpy(nonce + 8, &r, 4);
	cChaCha_Crypt(&ctx, key, 0, nonce, polyKey, polyKey, sizeof(polyKey));
	cNetBuffer_AppendPad16(&nb, nonce, sizeof(nonce));
	cChaCha_Crypt(&ctx, key, 1, nonce, data, ct, (uint32_t)len);
	cNetBuffer_AppendPad16(&nb, ct, (uint32_t)len);
	cNetBuffer_WriteU64(&nb, sizeof(nonce));
	cNetBuffer_WriteU64(&nb, (uint64_t)(int64_t)len);
	Poly1305_Auth(mac, cNetBuffer_Data(&nb), cNetBuffer_Size(&nb), polyKey);
	cNetBuffer_Append(&nb, mac, sizeof(mac));
	(void)flags, (void)tolen;
	int r2 = Sock_SendTo(s, cNetBuffer_Data(&nb), (int)cNetBuffer_Size(&nb), to);
	cNetBuffer_dtor(&nb);
	return r2;
}

cGVar GVar_ClientConectionStatus;
static cGVar GVar_LastMasterCommandSent;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_ClientConectionStatus, &GVar_ClientConectionStatus)
INIT_GVAR_STRING(Init_GVar_ClientConectionStatus, Fini_GVar_ClientConectionStatus, &GVar_ClientConectionStatus, "ClientConectionStatus", 1, "String with conection information\n", "")
INIT_CALL(Init_cNetRandom_NetRandom, &g_NetRandom, cNetRandom_ctorSeeded)
FINI_GVAR_STRING(Fini_GVar_LastMasterCommandSent, &GVar_LastMasterCommandSent)
INIT_GVAR_STRING(Init_GVar_LastMasterCommandSent, Fini_GVar_LastMasterCommandSent, &GVar_LastMasterCommandSent, "LastMasterCommandSent", 0, "", "")
