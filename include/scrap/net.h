/*
 * Network (original source: Net/). GameClient and GameServer hold the Net_ functions of the SNet script module;
 * msgman.cpp the message manager of one connection (cNetClient: reliable messages with 7 bit sequence numbers,
 * the ping and the time out), ManClient.cpp and ManServer.cpp the user managers on top of it (cServerNetManager on
 * a client for its server, cClientNetManager on the server for each client), NetData.cpp the resource names, the
 * player descriptors and the chat strings, transport.cpp the UDP socket (Winsock through the imports of the exe,
 * every packet ChaCha20 encrypted with a Poly1305 tag) and the server browser. Times are milliseconds
 * (CApp GetTime * 1000). All plain functions are cdecl.
 */
#ifndef SCRAP_NET_H
#define SCRAP_NET_H

#include "scrap/gvar.h"
#include "scrap/scrap.h"

extern cGVarFunction GVar_ClientAddRes;
extern cGVar GVar_ClientConectionStatus;
extern cGVarFunction GVar_ClientCreate;
extern cGVarFunction GVar_ClientDelRes;
extern cGVarFunction GVar_ClientSetPlayer;
extern cGVarFunction GVar_OnChatString;
extern cGVarFunction GVar_OnClientExit;
extern cGVarFunction GVar_OnClientJoin;
extern cGVarFunction GVar_OnNetBrowseCallback;
extern cGVarFunction GVar_OnUsrString;

#define NET_GAME_ID 0xceba
#define NET_GAME_VERSION 0x101
#define NET_DEFAULT_PORT 0x6db6
#define NET_RESOURCES 0x100
#define NET_PLAYER_MAX_ID 0x200		/* "net<n>": n <= 0x200 a player ship, above other objects */

/* the modes of the transport (Transport_GetMode) */
#define NETMODE_NONE 0
#define NETMODE_SERVER 1
#define NETMODE_CLIENT 2
#define NETMODE_BROWSER 3

/* the message kinds, the first byte of a reliable message */
#define NETMSG_LEVEL 0			/* server: level and server type; client: hello */
#define NETMSG_RESOURCES 1
#define NETMSG_GOODBYE 2
#define NETMSG_CHAT 3
#define NETMSG_USR_STRING 4
#define NETMSG_USR_DATA 5		/* server: player descriptors; client: send me everything */
#define NETMSG_DELETE 6			/* server: a player left; client: hello again (modified ship) */
#define NETMSG_PING 7			/* server: reset; browser: the ping of the server list */
#define NETMSG_CONSOLE 8		/* client: remote console command */

/* a reliable message in the queue of a cNetClient (12 bytes, Mem_Alloc) */
typedef struct cNetMsgNode {
	uint16_t size;			/* with the sequence byte */
	byte pad_2[2];
	byte *data;			/* data[0] the sequence number (7 bits), then the message */
	struct cNetMsgNode *next;
} cNetMsgNode;

typedef struct cNetClient cNetClient;
typedef struct cUserNetManager cUserNetManager;

typedef struct cUserNetManagerVtbl {
	void (*Free)(cUserNetManager *self);
	/* the unreliable part and the reliable message of a packet */
	void(*OnReceive)(cUserNetManager *self, void *data, int size, void *ctl, int ctlSize);
	void(*Send)(cUserNetManager *self, uint32_t time);
	void(*SetLocalData)(cUserNetManager *self);
} cUserNetManagerVtbl;

/* calls through the slots of cUserNetManagerVtbl */
static inline void NetMan_Free(void *self) {
	VT(cUserNetManagerVtbl, self)->Free(self);
}

static inline void NetMan_OnReceive(void *self, void *data, int size, void *ctl, int ctlSize) {
	VT(cUserNetManagerVtbl, self)->OnReceive(self, data, size, ctl, ctlSize);
}

static inline void NetMan_Send(void *self, uint32_t time) {
	VT(cUserNetManagerVtbl, self)->Send(self, time);
}

static inline void NetMan_SetLocalData(void *self) {
	VT(cUserNetManagerVtbl, self)->SetLocalData(self);
}

/* the base of the user managers (msgman.cpp), made by g_NetUserFactory for each cNetClient */
struct cUserNetManager {
	const cUserNetManagerVtbl *vftable;
	int slot;			/* the client slot of the server, -1 on a client */
	cNetClient *client;
};

#define NET_NAME_SIZE 0x80

/* cServerNetManager (ManClient.cpp): on a client, the connection to the server */
typedef struct cNetShip {
	cUserNetManager base;
	char name[NET_NAME_SIZE];	/* the entity of the own ship ("net<n>") */
} cNetShip;
typedef cNetShip cServerNetManager;

/* cClientNetManager (ManServer.cpp): on the server, one client and its ship */
typedef struct cClientNetManager {
	cUserNetManager base;
	char name[NET_NAME_SIZE];	/* the entity "net<slot>" */
	char usrName[NET_NAME_SIZE];
	char model[NET_NAME_SIZE];	/* the ship model */
	char pilot[NET_NAME_SIZE];
	char motors[4][NET_NAME_SIZE];
	char weapons[NET_NAME_SIZE];	/* the weapon bay list */
	int team;
	uint16_t maxLife;
	byte pad_492[2];
	uint32_t fullSends;		/* the first 5 sends carry everything */
	uint16_t sends;			/* counts the sends, picks the client of the ping byte */
	byte pad_49a[2];
} cClientNetManager;

/* cNetClient.state */
#define NETCLIENT_CONNECTING 0
#define NETCLIENT_CONNECTED 1
#define NETCLIENT_WAITING 2		/* "Waiting server response" */
#define NETCLIENT_TIMEOUT 3
#define NETCLIENT_DISCONNECTED 4	/* goodbye sent, waits g_NetCloseTime */

/*
 * the message manager of one connection (msgman.cpp, 0x2c bytes). A packet: byte 0 the sequence number of the
 * first queued message (7 bits) with the ping toggle in bit 7, a ushort length of that message, the message, then
 * the unreliable data. The queue head goes out again in every packet until the other side acknowledges it.
 */
struct cNetClient {
	cNetMsgNode *head;		/* the reliable messages not acknowledged yet */
	cNetMsgNode *tail;
	byte seqOut;			/* the number of the next queued message */
	byte seqIn;			/* the last message received, 0x7f at the start */
	byte pad_a[2];
	uint32_t lastRecv;		/* time of the last packet */
	int state;			/* NETCLIENT_ */
	uint32_t pingStart;		/* time the toggle flipped, 0 while waiting */
	int toggle;			/* bit 7 of the packets */
	int ping;
	uint32_t lastSend;
	byte received;			/* a packet came since the last send */
	byte pad_25[3];
	union {				/* the user manager; both derived classes have the entity name at 0xc */
		cNetShip *ship;
		cUserNetManager *user;
	};
};

typedef cUserNetManager *(*netuserfactory_t)(cNetClient *client, int slot);

/* the fields of a vehicle the player descriptors carry */
#define NetEnt_Weapons(e) ((void *)&Veh(e)->weapons)	/* cWeaponManager */
#define NetEnt_Motors(e) ((void *)&Veh(e)->motors)

/* the time of the net code: CApp GetTime in milliseconds (x87 multiply, __dtoui3) */
static inline uint32_t Net_TimeMs(void) {
	return Float_ToU32(((App_GetTime(g_pApp)) * 1000.0));
}

/* exe functions of other modules */
void cWithLifeEntity_SetUsrName(cEntity *e, const char *name);	/* "" clears */

/* the 64 bit Mersenne Twister of the packet nonces with a shuffle table of 256 outputs */
#define NETRAND_NN 312
#pragma pack(push, 4)
typedef struct cNetRandom {
	uint64_t table[0x100];
	int i;
	int j;
	uint64_t mt[NETRAND_NN];
	int mti;			/* NETRAND_NN + 1 before the first seed */
} cNetRandom;
#pragma pack(pop)

/* a growing byte array of the transport (the data of a packet for the Poly1305 tag) */
typedef struct cNetBuffer {
	uint32_t size;
	uint32_t cap;
	byte *data;			/* Mem_Alloc */
} cNetBuffer;

/* the ChaCha20 state */
typedef struct cChaCha {
	uint32_t state[16];
} cChaCha;

/* a sockaddr_in (16 bytes, the same on Winsock and BSD sockets) */
typedef struct NetAddr {
	uint16_t family;		/* 2: AF_INET */
	uint16_t port;			/* network order */
	uint32_t addr;
	byte zero[8];
} NetAddr;

/* the servers to ping in browser mode, 0x200 of them */
#define NET_PING_SERVERS 0x200
typedef struct NetPingServer {
	NetAddr addr;			/* port 0: free */
	uint32_t time;			/* timeGetTime of the ping */
} NetPingServer;

/* the answer to a ping of the browser */
typedef struct NetServerInfo {
	uint16_t id;			/* NET_GAME_ID */
	uint16_t version;		/* NET_GAME_VERSION */
	uint16_t port;
	int16_t maxPlayers;
	int16_t numPlayers;
	byte flags;			/* 1 dedicated, 2 ForceServerVehicle, 4 UseServerPassword */
	char name[0x20];		/* ServerName */
	char type[0x10];		/* ServerType */
	char map[0x20];			/* the level path after "Levels/" */
	byte pad_5b;
} NetServerInfo;

extern cGVar GVar_ClientPPS;
#define g_ClientPPS (GVar_ClientPPS.value.i)
#define g_ClientConnectionStatus (((cString *)&GVar_ClientConectionStatus.value))	/* ClientConectionStatus */
#define g_MasterServerAddress (*(char * *)&g_GVarMasterServerAddress.value)
#define g_MasterServerPortFlags (g_GVarMasterServerPort.base.flags)
#define g_MasterServerPort (g_GVarMasterServerPort.value.i)
extern cGVar g_GVarServerType;
#define g_ServerType (*(char * *)&g_GVarServerType.value)	/* ServerType (cString) */
/* the script callbacks (cPy_Function in the gvar) and the flags of their gvars */
#define g_OnClientJoinFlags (GVar_OnClientJoin.head.flags)
#define g_OnClientJoin (((cPy_Function *)&GVar_OnClientJoin.func))
#define g_OnChatStringFlags (GVar_OnChatString.head.flags)
#define g_OnChatString (((cPy_Function *)&GVar_OnChatString.func))
#define g_ClientAddResFlags (GVar_ClientAddRes.head.flags)
#define g_ClientAddRes (((cPy_Function *)&GVar_ClientAddRes.func))
#define g_OnUsrStringFlags (GVar_OnUsrString.head.flags)
#define g_OnUsrString (((cPy_Function *)&GVar_OnUsrString.func))
#define g_OnNetBrowseFlags (GVar_OnNetBrowseCallback.head.flags)
#define g_OnNetBrowseCallback (((cPy_Function *)&GVar_OnNetBrowseCallback.func))
#define g_OnClientExitFlags (GVar_OnClientExit.head.flags)
#define g_OnClientExit (((cPy_Function *)&GVar_OnClientExit.func))
#define g_ClientDelResFlags (GVar_ClientDelRes.head.flags)
#define g_ClientDelRes (((cPy_Function *)&GVar_ClientDelRes.func))
#define g_ClientSetPlayerFlags (GVar_ClientSetPlayer.head.flags)
#define g_ClientSetPlayer (((cPy_Function *)&GVar_ClientSetPlayer.func))
#define g_ClientCreateFlags (GVar_ClientCreate.head.flags)
#define g_ClientCreate (((cPy_Function *)&GVar_ClientCreate.func))

extern cFileMem g_NetMsg;
extern cStaticBuffer cStaticBuffer_PyStaticBuffer[2];
#define g_NetStaticBuffer (&cStaticBuffer_PyStaticBuffer[1])
#define g_PyStaticBuffer (&cStaticBuffer_PyStaticBuffer[0])
extern vec3_t g_NetOrigin;
#define NET_OFFLINE "Offline"
typedef void (*netmastercmd_t)(const char *cmd);
extern netmastercmd_t g_MasterCommandCallback;
/* the counters for the stats (same names as include/scrap/scoreritems.h) */
extern int g_NetStatSent;
extern int g_NetStatSentSlot;
extern int g_NetStatRecv;
extern int g_NetStatRecvSlot;

/* Net/Sock.c: the UDP socket on Winsock or BSD sockets */
int Sock_Startup(void);			/* 1 when the network can be used */
void Sock_Cleanup(void);
uint32_t Sock_OpenUdp(void);		/* non-blocking, broadcast allowed; 0 on failure */
int Sock_Close(uint32_t s);		/* 0 when closed */
int Sock_Bind(uint32_t s, const NetAddr *a);	/* 0 when bound */
int Sock_RecvFrom(uint32_t s, void *buf, int len, NetAddr *from);	/* the bytes, negative for none */
int Sock_SendTo(uint32_t s, const void *buf, int len, const NetAddr *to);
int Sock_Readable(uint32_t s);		/* a packet waits */
/* the IPv4 address (network order) and the official name of a host name or a dotted address; 0 when not found */
int Sock_Resolve(const char *name, uint32_t *addr, char *canon, int size);
/* network order of the x86 targets */
static inline uint16_t Net_Htons(uint16_t v) {
	return (uint16_t)(v << 8 | v >> 8);
}

/* GameClient (src/game/Net/GameClient.c) */
byte Net_CloseClient(void);
byte Net_ClientFrame(void);
void Net_ClientReset(void);
byte Net_CloseClientMsg(const char *msg);
const char *Net_GetClientName(void);
uint16_t Net_GetClientPort(void);
cNetClient *Net_GetClient(void);
byte Net_InitClient(const char *address, int port);
byte Net_IsClient(void);
/* asks the server for everything (NETMSG_USR_DATA) */
byte Net_ClientReady(void);

byte Net_CloseServer(const char *msg);
const char *Net_GetServerName(void);
const char *Net_GetServerMap(void);
uint16_t Net_GetServerPort(void);
byte Net_InitServer(const char *name, int a, int b, int c);
byte Net_IsServer(void);
byte Net_Kick(const char *player);
byte Net_ServerChangeLevel(const char *level);
byte Net_ServerFrame(void);
void Net_ServerFrameHead(void);
byte Net_ServerFrameTail(void);
void Net_DumpData(const char *title, const void *data, int size);

/* ManClient.cpp (src/game/Net/ManClient.c) */
cServerNetManager *cServerNetManager_ctor(cServerNetManager *self, cNetClient *client, int slot);
/* the ping of a client the server reported (2^(b/24)), 0 unknown */
int Net_GetClientPing(uint32_t slot);
void cUserNetManager_Nop(cUserNetManager *self);

/* ManServer.cpp (src/game/Net/ManServer.c) */
cClientNetManager *cClientNetManager_ctor(cClientNetManager *self, cNetClient *client, int slot);
/* the descriptor of player slot to every client (toAll) or to all but that one */
void ManServer_SendUsrData(int slot, byte toAll);

/* msgman.cpp (src/game/Net/MsgMan.c) */
cNetClient *cNetClient_ctor(cNetClient *self, int slot);
cUserNetManager *cUserNetManager_ctor(cUserNetManager *self, cNetClient *client, int slot);
void cNetClient_dtor(cNetClient *self);
void cUserNetManager_dtor(cUserNetManager *self);
/* a reliable message into the queue */
void cNetClient_Push(cNetClient *self, const void *data, uint16_t size);
/* an empty packet when nothing went out for g_NetSendDelay; 0 once timed out */
int cNetClient_Flush(cNetClient *self, uint32_t time, int slot);
void cNetClient_RequestFull(cNetClient *self);
uint32_t cNetClient_TimeToSend(cNetClient *self, uint32_t time);
void cNetClient_Goodbye(cNetClient *self);
int cNetClient_ClientRecv(cNetClient *self, uint32_t time, int *slot, void *data, int *dataSize, void *ctl,
	int *ctlSize);
/* the packet with size bytes of unreliable data; its length, 0 if not sent */
uint32_t cNetClient_Send(cNetClient *self, uint32_t time, int slot, int size, const void *data);
int cNetClient_Receive(cNetClient *self, uint32_t time, const byte *pkt, int len, void *data, int *dataSize,
	void *ctl, int *ctlSize);
void MsgMan_SetUserFactory(netuserfactory_t f);
void MsgMan_SetSendDelay(uint32_t ms);

uint64_t cNetRandom_Next(cNetRandom *self);
/* seeded from GetTickCount64 through ChaCha20 */
cNetRandom *cNetRandom_ctorSeeded(cNetRandom *self);

/* NetData.cpp (src/game/Net/NetData.c) */
int Net_AddResource(const char *name);
void Net_Done(void);
void Net_GetBotName(char *buf);
/* n of "net<n>", -1 for other names */
int Net_GetPlayerIndex(const char *name);
void Net_GetObjName(char *buf);
/* the name of network resource i, "Offline" outside a network game */
const char *Net_GetResourceName(int i);
int Net_GetPing(const char *player);
void Net_Init(void);
void NetData_ReadResources(cAbstractFile *f);
void NetData_ReadUsrDescriptors(cAbstractFile *f);
void NetData_WriteResources(cAbstractFile *f, uint16_t first, byte count);
byte Net_SendChatString(const char *text, int arg);
byte Net_SendCtrlString(const char *text);
byte Net_SendUsrString(const char *text, int arg);
void NetData_WriteUsr(cAbstractFile *f, uint16_t slot);
/* the descriptors of the players from e on (NetMaxPlayersInitSend at most); the entity to go on with */
cEntity *NetData_WriteUsrDescriptors(cAbstractFile *f, cEntity *e);
byte Net_IsPlayerName(const char *name);
int Net_InitBrowser(int a, const char *address);
int Net_DoneBrowser(void);
void Net_Update(void);
void NetData_OnBrowse(const char *ip, int port, const NetServerInfo *info, int ping);

/* the server's client table (src/game/Net/NetServer.c) */
void NetServer_SendAll(const void *data, uint16_t size);
void NetServer_SendAllBut(const void *data, uint16_t size, int slot);
int NetServer_SendTo(uint32_t time, int slot, int size, const void *data);
void NetServer_Done(void);
void NetServer_GetInfo(NetServerInfo *info);
int NetServer_GetSlotByIndex(int n);
cNetClient *NetServer_GetClient(int slot);
int NetServer_NextToSend(uint32_t time);
int NetServer_Init(int maxClients, uint16_t port, int arg);
void NetServer_Goodbye(void);
int NetServer_DropClient(int slot);
int NetServer_Flush(uint32_t time);
byte Net_ModifyUsrData(int arg);
/* the slot of the packet read (-1 none, -2 refused) */
int NetServer_Receive(uint32_t time, void *data, int *dataSize, void *ctl, int *ctlSize);

int Transport_CloseBrowser(void);
void Transport_CloseClient(void);
void Transport_Close(void);
int Transport_RemoveClient(int slot);
/* the number of connected clients (the name comes from its callers) */
int Net_GetSendRate(void);
int Transport_Recv(byte *buf, int *len);
int Transport_GetMaxClients(void);
int Transport_GetMode(void);
uint16_t Transport_GetLocalPort(void);
int Transport_SetMaster(const char *address, uint16_t port);
int Transport_InitBrowser(const char *address, uint16_t port);
int Transport_InitClient(const char *address, uint16_t port);
int Transport_InitServer(int maxClients, uint16_t port);
int Net_PingInetSvrs(void);
int Transport_SendNeedPassword(int slot);
/* the host name of address (a name or a dotted IP), its IP as text into ip, as number into addr; NULL if unknown */
const char *Transport_Resolve(const char *address, char *ip, uint32_t *addr);
int Transport_PingMaster(void);
int Transport_SendToClient(const void *data, int size, int slot);
void Transport_SendToServer(const void *data, int size);
int Net_SendMasterString(const char *text);
int Transport_Wait(int ms);
int Net_IsMaster(void);

/* the crypto of the packets (src/game/Net/Crypto.c) */
cNetBuffer *cNetBuffer_ctor(cNetBuffer *self, uint32_t cap);
void cNetBuffer_dtor(cNetBuffer *self);
byte *cNetBuffer_Data(cNetBuffer *self);
uint32_t cNetBuffer_Size(cNetBuffer *self);
int cNetBuffer_Append(cNetBuffer *self, const void *src, uint32_t n);
/* the bytes and zeros up to the next multiple of 16 of n */
int cNetBuffer_AppendPad16(cNetBuffer *self, const void *src, uint32_t n);
int cNetBuffer_WriteU64(cNetBuffer *self, uint64_t v);
cChaCha *cChaCha_ctor(cChaCha *self);
void cChaCha_Crypt(cChaCha *self, const byte *key, uint32_t counter, const byte *nonce, const byte *in,
	byte *out, uint32_t len);
/* Poly1305 (poly1305-donna) of size bytes of msg with key (32 bytes) into mac (16 bytes) */
void Poly1305_Auth(byte *mac, const byte *msg, uint32_t size, const byte *key);

#endif
