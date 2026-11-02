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
	uint16_t size;			/* 0x0 with the sequence byte */
	byte pad_2[2];
	byte *data;			/* 0x4 data[0] the sequence number (7 bits), then the message */
	struct cNetMsgNode *next;	/* 0x8 */
} cNetMsgNode;
SD_SIZE(cNetMsgNode, 0xc);

typedef struct cNetClient cNetClient;
typedef struct cUserNetManager cUserNetManager;

typedef struct cUserNetManagerVtbl {
	cUserNetManager *(*Delete)(cUserNetManager *self, byte flags);			/* 0x00 */
	/* the unreliable part and the reliable message of a packet */
	void(*OnReceive)(cUserNetManager *self, void *data, int size, void *ctl, int ctlSize);	/* 0x04 */
	void(*Send)(cUserNetManager *self, uint32_t time);				/* 0x08 */
	void(*SetLocalData)(cUserNetManager *self);					/* 0x0c */
} cUserNetManagerVtbl;

typedef struct cServerNetManagerVtbl {
	cUserNetManagerVtbl base;
	void(*SendHello)(cUserNetManager *self, byte type);				/* 0x10 */
} cServerNetManagerVtbl;
SD_OFFSET(cServerNetManagerVtbl, SendHello, 0x10);

/* the base of the user managers (msgman.cpp, 0xc bytes), made by g_NetUserFactory for each cNetClient */
struct cUserNetManager {
	const cUserNetManagerVtbl *vftable;	/* 0x0 */
	int slot;			/* 0x4 the client slot of the server, -1 on a client */
	cNetClient *client;		/* 0x8 */
};
SD_SIZE(cUserNetManager, 0xc);

#define NET_NAME_SIZE 0x80

/* cServerNetManager (ManClient.cpp, 0x8c bytes): on a client, the connection to the server */
typedef struct cNetShip {
	cUserNetManager base;
	char name[NET_NAME_SIZE];	/* 0x0c the entity of the own ship ("net<n>") */
} cNetShip;
SD_OFFSET(cNetShip, name, 0xc);
SD_SIZE(cNetShip, 0x8c);
typedef cNetShip cServerNetManager;

/* cClientNetManager (ManServer.cpp, 0x49c bytes): on the server, one client and its ship */
typedef struct cClientNetManager {
	cUserNetManager base;
	char name[NET_NAME_SIZE];	/* 0x00c the entity "net<slot>" */
	char usrName[NET_NAME_SIZE];	/* 0x08c */
	char model[NET_NAME_SIZE];	/* 0x10c the ship model */
	char pilot[NET_NAME_SIZE];	/* 0x18c */
	char motors[4][NET_NAME_SIZE];	/* 0x20c */
	char weapons[NET_NAME_SIZE];	/* 0x40c the weapon bay list */
	int team;			/* 0x48c */
	uint16_t maxLife;		/* 0x490 */
	byte pad_492[2];
	uint32_t fullSends;		/* 0x494 the first 5 sends carry everything */
	uint16_t sends;			/* 0x498 counts the sends, picks the client of the ping byte */
	byte pad_49a[2];
} cClientNetManager;
SD_OFFSET(cClientNetManager, team, 0x48c);
SD_OFFSET(cClientNetManager, sends, 0x498);
SD_SIZE(cClientNetManager, 0x49c);

extern const cUserNetManagerVtbl cUserNetManager_vftable;
#define g_UserNetManagerVftable (&cUserNetManager_vftable)
extern const cServerNetManagerVtbl cServerNetManager_vftable;
#define g_ServerNetManagerVftable (&cServerNetManager_vftable.base)
extern const cUserNetManagerVtbl cClientNetManager_vftable;
#define g_ClientNetManagerVftable (&cClientNetManager_vftable)

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
	cNetMsgNode *head;		/* 0x00 the reliable messages not acknowledged yet */
	cNetMsgNode *tail;		/* 0x04 */
	byte seqOut;			/* 0x08 the number of the next queued message */
	byte seqIn;			/* 0x09 the last message received, 0x7f at the start */
	byte pad_a[2];
	uint32_t lastRecv;		/* 0x0c time of the last packet */
	int state;			/* 0x10 NETCLIENT_ */
	uint32_t pingStart;		/* 0x14 time the toggle flipped, 0 while waiting */
	int toggle;			/* 0x18 bit 7 of the packets */
	int ping;			/* 0x1c */
	uint32_t lastSend;		/* 0x20 */
	byte received;			/* 0x24 a packet came since the last send */
	byte pad_25[3];
	union {				/* 0x28 the user manager; both derived classes have the entity name at 0xc */
		cNetShip *ship;
		cUserNetManager *user;
	};
};
SD_OFFSET(cNetClient, state, 0x10);
SD_OFFSET(cNetClient, ping, 0x1c);
SD_OFFSET(cNetClient, ship, 0x28);
SD_SIZE(cNetClient, 0x2c);

typedef cUserNetManager *(*netuserfactory_t)(cNetClient *client, int slot);

/* the fields of a vehicle the player descriptors carry */
#define NetEnt_Weapons(e) ((void *)&Veh(e)->weapons)	/* cWeaponManager */
#define NetEnt_Motors(e) ((void *)&Veh(e)->motors)

/* the time of the net code: CApp GetTime in milliseconds (x87 multiply, __dtoui3) */
static inline uint32_t Net_TimeMs(void) {
	return Float_ToU32(((g_pApp->vftable->GetTime(g_pApp)) * 1000.0));
}

/* exe functions of other modules */
void cWithLifeEntity_SetUsrName(cEntity *e, const char *name);	/* rva 0x12c7c0, "" clears */

/* the 64 bit Mersenne Twister of the packet nonces with a shuffle table of 256 outputs (0x11cc bytes) */
#define NETRAND_NN 312
#pragma pack(push, 4)
typedef struct cNetRandom {
	uint64_t table[0x100];		/* 0x000 */
	int i;				/* 0x800 */
	int j;				/* 0x804 */
	uint64_t mt[NETRAND_NN];	/* 0x808 */
	int mti;			/* 0x11c8 NETRAND_NN + 1 before the first seed */
} cNetRandom;
#pragma pack(pop)
SD_OFFSET(cNetRandom, mt, 0x808);
SD_OFFSET(cNetRandom, mti, 0x11c8);
SD_SIZE(cNetRandom, 0x11cc);

/* a growing byte array of the transport (the data of a packet for the Poly1305 tag) */
typedef struct cNetBuffer {
	uint32_t size;			/* 0x0 */
	uint32_t cap;			/* 0x4 */
	byte *data;			/* 0x8 Mem_Alloc */
} cNetBuffer;
SD_SIZE(cNetBuffer, 0xc);

/* the ChaCha20 state */
typedef struct cChaCha {
	uint32_t state[16];
} cChaCha;
SD_SIZE(cChaCha, 0x40);

/* a sockaddr_in (16 bytes, the same on Winsock and BSD sockets) */
typedef struct NetAddr {
	uint16_t family;		/* 0x0 2: AF_INET */
	uint16_t port;			/* 0x2 network order */
	uint32_t addr;			/* 0x4 */
	byte zero[8];			/* 0x8 */
} NetAddr;
SD_SIZE(NetAddr, 0x10);

/* the servers to ping in browser mode, 0x200 of them */
#define NET_PING_SERVERS 0x200
typedef struct NetPingServer {
	NetAddr addr;			/* 0x00 port 0: free */
	uint32_t time;			/* 0x10 timeGetTime of the ping */
} NetPingServer;
SD_SIZE(NetPingServer, 0x14);

/* the answer to a ping of the browser (0x5c bytes) */
typedef struct NetServerInfo {
	uint16_t id;			/* 0x00 NET_GAME_ID */
	uint16_t version;		/* 0x02 NET_GAME_VERSION */
	uint16_t port;			/* 0x04 */
	int16_t maxPlayers;		/* 0x06 */
	int16_t numPlayers;		/* 0x08 */
	byte flags;			/* 0x0a 1 dedicated, 2 ForceServerVehicle, 4 UseServerPassword */
	char name[0x20];		/* 0x0b ServerName */
	char type[0x10];		/* 0x2b ServerType */
	char map[0x20];			/* 0x3b the level path after "Levels/" */
	byte pad_5b;
} NetServerInfo;
SD_OFFSET(NetServerInfo, map, 0x3b);
SD_SIZE(NetServerInfo, 0x5c);

/* GameClient */
extern char g_NetClientServer[256];	/* [0x100] the server address, "" when not connected */
extern uint16_t g_NetClientPort;
extern cNetClient *g_NetClient;
extern void *g_NetClientData;	/* the receive buffers, 24000 bytes */
extern int g_NetClientDataSize;
extern void *g_NetClientCtl;
extern int g_NetClientCtlSize;
/* GameServer */
extern char g_NetServerHost[256];	/* [0x100] */
extern char g_NetServerIP[256];	/* [0x100] */
extern cGVar GVar_BrowserCode;
#define g_NetBrowserCode (GVar_BrowserCode.value.s)	/* BrowserCode (cString) */
extern uint16_t g_NetServerPort;	/* 0 without a server */
extern void *g_NetServerData;
extern int g_NetServerDataSize;
extern void *g_NetServerCtl;
extern int g_NetServerCtlSize;
extern float g_NetHeartbeat;	/* GetTime of the last heartbeat to the master */
extern cGVar GVar_LogNetData;
#define g_LogNetData (GVar_LogNetData.value.i)		/* LogNetData */
extern byte g_NetClientPings[256];	/* [0x100] the pings the server sends, log2 * 24 */
/* the engine variables (gvars) of Net/: values and their flags byte (bit 0: scripts may not write) */
extern cGVar GVar_NetMaxResourcesSend;
#define g_NetMaxResourcesSend (GVar_NetMaxResourcesSend.value.i)
extern cGVar GVar_NetTimeOut;
#define g_NetTimeOut (GVar_NetTimeOut.value.i)		/* NetTimeOut, < 0 never */
extern cGVar GVar_NetMaxPlayersInitSend;
#define g_NetMaxPlayersInitSend (GVar_NetMaxPlayersInitSend.value.i)
extern cGVar GVar_ClientPPS;
#define g_ClientPPS (GVar_ClientPPS.value.i)
extern cGVar GVar_HeartbeatTime;
#define g_HeartbeatTime (GVar_HeartbeatTime.value.f)
extern cGVar GVar_ServerMinSendDelay;
#define g_ServerMinSendDelay (GVar_ServerMinSendDelay.value.i)
#define g_ClientConnectionStatus (((cString *)&GAME_GVAR(GVar_ClientConectionStatus)->value))	/* ClientConectionStatus */
extern cGVar GVar_UseServerPassword;
#define g_UseServerPassword (GVar_UseServerPassword.value.i)
#define g_MasterServerAddress (*(char * *)&GAME_GVAR(g_GVarMasterServerAddress)->value)
#define g_MasterServerPortFlags (*(byte *)&GAME_GVAR(g_GVarMasterServerPort)->base.flags)
#define g_MasterServerPort (*(int *)&GAME_GVAR(g_GVarMasterServerPort)->value)
extern cGVar GVar_ClientPassword;
#define g_ClientPassword (GVar_ClientPassword.value.s)
extern cGVar GVar_ClientRemotePassword;
#define g_ClientRemotePassword (GVar_ClientRemotePassword.value.s)
#define g_ServerRemotePassword (((cString *)&GAME_GVAR(GVar_ServerRemotePassword)->value))
extern cGVar GVar_ForceServerVehicle;
#define g_ForceServerVehicle (GVar_ForceServerVehicle.value.i)
#define g_ServerPassword (((cString *)&GAME_GVAR(GVar_ServerPassword)->value))
extern cGVar GVar_ServerName;
#define g_ServerName (GVar_ServerName.value.s)
#define g_GVarServerType (GAME_ARRAY(cGVar, g_GVarServerType))
#define g_ServerType (*(char * *)&GAME_GVAR(g_GVarServerType)->value)	/* ServerType (cString) */
#define g_LastMasterCommandFlags (*(byte *)&GAME_GVAR(GVar_LastMasterCommandSent)->base.flags)
#define g_LastMasterCommand (((cString *)&GAME_GVAR(GVar_LastMasterCommandSent)->value))	/* LastMasterCommandSent */
extern cGVar GVar_ServerDataRateLimit;
#define g_ServerDataRateLimit (GVar_ServerDataRateLimit.value.i)
/* the script callbacks (cPy_Function in the gvar) and the flags of their gvars */
#define g_OnClientJoinFlags (GAME_GVARFUNC(GVar_OnClientJoin)->head.flags)
#define g_OnClientJoin (((cPy_Function *)&GAME_GVARFUNC(GVar_OnClientJoin)->func))
#define g_ClientSetConfig (((cPy_Function *)&GAME_GVARFUNC(GVar_ClientSetConfig)->func))
#define g_OnChatStringFlags (GAME_GVARFUNC(GVar_OnChatString)->head.flags)
#define g_OnChatString (((cPy_Function *)&GAME_GVARFUNC(GVar_OnChatString)->func))
#define g_MasterCommandFunc (((cPy_Function *)&GAME_GVARFUNC(GVar_MasterCommandFunc)->func))
#define g_OnClientModify (((cPy_Function *)&GAME_GVARFUNC(GVar_OnClientModify)->func))
#define g_ClientAddResFlags (GAME_GVARFUNC(GVar_ClientAddRes)->head.flags)
#define g_ClientAddRes (((cPy_Function *)&GAME_GVARFUNC(GVar_ClientAddRes)->func))
#define g_OnUsrStringFlags (GAME_GVARFUNC(GVar_OnUsrString)->head.flags)
#define g_OnUsrString (((cPy_Function *)&GAME_GVARFUNC(GVar_OnUsrString)->func))
#define g_OnNetBrowseFlags (GAME_GVARFUNC(GVar_OnNetBrowseCallback)->head.flags)
#define g_OnNetBrowseCallback (((cPy_Function *)&GAME_GVARFUNC(GVar_OnNetBrowseCallback)->func))
#define g_OnClientExitFlags (GAME_GVARFUNC(GVar_OnClientExit)->head.flags)
#define g_OnClientExit (((cPy_Function *)&GAME_GVARFUNC(GVar_OnClientExit)->func))
#define g_ClientDelResFlags (GAME_GVARFUNC(GVar_ClientDelRes)->head.flags)
#define g_ClientDelRes (((cPy_Function *)&GAME_GVARFUNC(GVar_ClientDelRes)->func))
#define g_ClientSetPlayerFlags (GAME_GVARFUNC(GVar_ClientSetPlayer)->head.flags)
#define g_ClientSetPlayer (((cPy_Function *)&GAME_GVARFUNC(GVar_ClientSetPlayer)->func))
#define g_ClientAfterCreateFlags (GAME_GVARFUNC(GVar_ClientAfterCreate)->head.flags)
#define g_ClientAfterCreate (((cPy_Function *)&GAME_GVARFUNC(GVar_ClientAfterCreate)->func))
#define g_ClientCreateFlags (GAME_GVARFUNC(GVar_ClientCreate)->head.flags)
#define g_ClientCreate (((cPy_Function *)&GAME_GVARFUNC(GVar_ClientCreate)->func))
/* the player config the client sends in its hello (gvar values) */
extern cGVar GVar_PlayerName;
#define g_PlayerNameStr (GVar_PlayerName.value.s)
extern cGVar GVar_PlayerModel;
#define g_PlayerModel (GVar_PlayerModel.value.s)
extern cGVar GVar_PlayerMaxLife;
#define g_PlayerMaxLife (GVar_PlayerMaxLife.value.i)
extern cGVar GVar_PilotModel;
#define g_PilotModel (GVar_PilotModel.value.s)
extern cGVar GVar_Motor0Model;
#define g_Motor0Model (GVar_Motor0Model.value.s)
extern cGVar GVar_Motor1Model;
#define g_Motor1Model (GVar_Motor1Model.value.s)
extern cGVar GVar_Motor2Model;
#define g_Motor2Model (GVar_Motor2Model.value.s)
extern cGVar GVar_Motor3Model;
#define g_Motor3Model (GVar_Motor3Model.value.s)
extern cGVar GVar_WeaponBayList;
#define g_WeaponBayList (GVar_WeaponBayList.value.s)
extern cGVar GVar_PlayerTeamID;
#define g_PlayerTeamID (GVar_PlayerTeamID.value.i)

/* msgman.cpp */
extern uint32_t g_NetWaitTime;	/* 5000: no answer that long, NETCLIENT_WAITING */
extern uint32_t g_NetCloseTime;	/* 2000: after the goodbye */
extern uint32_t g_NetSendDelay;	/* 100 */
extern netuserfactory_t g_NetUserFactory;
#define g_NetMsg (&GAME_GLOBAL(cFileMem, g_NetMsg))	/* the memory file messages are built and read in */
#define g_NetStaticBuffer (&GAME_ARRAY(cStaticBuffer, cStaticBuffer_PyStaticBuffer)[1])
#define g_PyStaticBuffer (&GAME_GLOBAL_IN(cStaticBuffer, cStaticBuffer_PyStaticBuffer, 0))
extern vec3_t g_NetOrigin;
#define g_NetMaxDist2 GAME_GLOBAL(float, g_NetMaxDist2)	/* the distance (squared) of the near entities */
/* NetData.cpp */
extern int g_NetObjCounter;
extern int g_NetBotCounter;
extern char * g_NetResources[256];	/* [NET_RESOURCES] Mem_Alloc names */
extern byte g_NetResInit;
extern int g_NetBrowserPort;
#define NET_OFFLINE "Offline"
/* the server side client table */
extern cNetClient ** g_NetClients;	/* [Transport_GetMaxClients()] */
extern uint32_t g_NetNextSend;
typedef void (*netmastercmd_t)(const char *cmd);
extern netmastercmd_t g_MasterCommandCallback;
/* the counters for the stats (same names as include/scrap/scoreritems.h) */
extern int g_NetStatSent;
extern int g_NetStatSentSlot;
extern int g_NetStatRecv;
extern int g_NetStatRecvSlot;
/* transport.cpp */
extern uint32_t g_NetSocket;
#define g_NetRandom (&GAME_GLOBAL(cNetRandom, g_NetRandom))
#define g_NetLocalAddr (&GAME_GLOBAL(NetAddr, g_NetLocalAddr))
#define g_NetServerAddr (&GAME_GLOBAL(NetAddr, g_NetServerAddr))
extern int g_NetMode;		/* NETMODE_ */
extern int g_NetMasterOn;
extern int g_NetMaxClients;
extern int g_NetNumClients;
extern NetAddr *g_NetClientAddrs;	/* [g_NetMaxClients], family 0: free */
extern NetPingServer *g_NetPingServers;
extern uint32_t g_NetBrowseStart;
#define g_NetMasterAddr (&GAME_GLOBAL(NetAddr, g_NetMasterAddr))
#define g_NetServerFullPkt (GAME_ARRAY(const byte, g_NetServerFullPkt))	/* 5 bytes: 0, 0, 0, 0, 8 */
#define g_NetNeedPasswordPkt (GAME_ARRAY(const byte, g_NetNeedPasswordPkt))	/* 5 bytes: 0, 0, 0, 0, 9 */

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

extern cGVar GVar_Fraglimit;
#define g_Fraglimit (GVar_Fraglimit.value.i)

/* GameClient (src/game/Net/GameClient.c) */
byte Net_CloseClient(void);
void Net_ClientResetFuncs(void);
byte Net_ClientFrame(void);
void Net_ClientReset(void);
byte Net_CloseClientMsg(const char *msg);
cUserNetManager *Net_NewServerManager(cNetClient *client, int slot);
const char *Net_GetClientName(void);
uint16_t Net_GetClientPort(void);
cNetClient *Net_GetClient(void);
byte Net_InitClient(const char *address, int port);
byte Net_IsClient(void);
/* asks the server for everything (NETMSG_USR_DATA) */
byte Net_ClientReady(void);

/* GameServer.cpp (src/game/Net/GameServer.c) */
void Net_ServerResetFuncs(void);
byte Net_CloseServer(const char *msg);
cUserNetManager *Net_NewClientManager(cNetClient *client, int slot);
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
/* a hex and text dump of data into the console, 16 bytes a line */
void Net_HexDump(const void *data, int size);
void Net_DumpData(const char *title, const void *data, int size);

/* ManClient.cpp (src/game/Net/ManClient.c) */
cServerNetManager *cServerNetManager_ctor(cServerNetManager *self, cNetClient *client, int slot);
void cServerNetManager_dtor(cServerNetManager *self);
cUserNetManager *cServerNetManager_Delete(cUserNetManager *base, byte flags);
void cServerNetManager_OnReceive(cUserNetManager *elem, void *data, int size, void *ctl, int ctlSize);
/* the ping of a client the server reported (2^(b/24)), 0 unknown */
int Net_GetClientPing(uint32_t slot);
void cServerNetManager_SendHello(cUserNetManager *elem, byte type);
void cServerNetManager_Send(cUserNetManager *base, uint32_t time);
void cUserNetManager_Nop(cUserNetManager *self);

/* ManServer.cpp (src/game/Net/ManServer.c) */
cClientNetManager *cClientNetManager_ctor(cClientNetManager *self, cNetClient *client, int slot);
void cClientNetManager_dtor(cClientNetManager *self);
cUserNetManager *cClientNetManager_Delete(cUserNetManager *base, byte flags);
void cClientNetManager_OnReceive(cUserNetManager *elem, void *data, int size, void *ctl, int ctlSize);
/* creates the ship "net<slot>", sends the level and the resources */
void cClientNetManager_InitClient(cClientNetManager *self);
/* the descriptor of player slot to every client (toAll) or to all but that one */
void ManServer_SendUsrData(int slot, byte toAll);
void cClientNetManager_SendResources(cClientNetManager *self);
void cClientNetManager_Send(cUserNetManager *elem, uint32_t time);
/* the descriptors of all players to this client (unless local), then this one's to the others */
void cClientNetManager_SendUsers(cClientNetManager *self, byte local);
void cClientNetManager_SetLocalData(cUserNetManager *elem);

/* msgman.cpp (src/game/Net/MsgMan.c) */
cNetClient *cNetClient_ctor(cNetClient *self, int slot);
cUserNetManager *cUserNetManager_ctor(cUserNetManager *self, cNetClient *client, int slot);
void cNetClient_dtor(cNetClient *self);
void cUserNetManager_dtor(cUserNetManager *self);
cUserNetManager *cUserNetManager_Delete(cUserNetManager *self, byte flags);
/* a reliable message into the queue */
void cNetClient_Push(cNetClient *self, const void *data, uint16_t size);
int cNetClient_UpdateState(cNetClient *self, uint32_t time);
/* an empty packet when nothing went out for g_NetSendDelay; 0 once timed out */
int cNetClient_Flush(cNetClient *self, uint32_t time, int slot);
void cNetClient_RequestFull(cNetClient *self);
cNetMsgNode *cNetClient_NewMsg(cNetClient *self, const void *data, uint16_t size);
void cNetClient_GetSeq(cNetClient *self, int *head, int *in);
uint32_t cNetClient_TimeToSend(cNetClient *self, uint32_t time);
void cNetClient_Goodbye(cNetClient *self);
int cNetClient_ClientRecv(cNetClient *self, uint32_t time, int *slot, void *data, int *dataSize, void *ctl,
	int *ctlSize);
/* the packet with size bytes of unreliable data; its length, 0 if not sent */
uint32_t cNetClient_Send(cNetClient *self, uint32_t time, int slot, int size, const void *data);
int cNetClient_Receive(cNetClient *self, uint32_t time, const byte *pkt, int len, void *data, int *dataSize,
	void *ctl, int *ctlSize);
void MsgMan_SetCloseTime(uint32_t ms);
void MsgMan_SetUserFactory(netuserfactory_t f);
void MsgMan_SetSendDelay(uint32_t ms);
void MsgMan_SetWaitTime(uint32_t ms);

/* the random numbers (src/game/Net/NetRandom.c) */
cNetRandom *cNetRandom_ctor(cNetRandom *self);
void cNetRandom_InitByArray(cNetRandom *self, const uint64_t *key, uint32_t len);
void cNetRandom_Seed(cNetRandom *self, uint64_t seed);
uint64_t cNetRandom_Next(cNetRandom *self);
void cNetRandom_Fill(cNetRandom *self);
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
void NetData_ReadUsrDescriptor(cAbstractFile *f, cEntity *e);
void NetData_WriteResources(cAbstractFile *f, uint16_t first, byte count);
byte Net_SendChatString(const char *text, int arg);
byte Net_SendCtrlString(const char *text);
/* a chat (chat) or user string to client slot (-1 all) or to the server */
byte Net_SendString(const char *text, int slot, byte chat);
byte Net_SendUsrString(const char *text, int arg);
void NetData_OnMasterCommand(const char *cmd);
void NetData_WriteUsr(cAbstractFile *f, uint16_t slot);
void NetData_WriteUsrDescriptor(cAbstractFile *f, cEntity *e);
/* the descriptors of the players from e on (NetMaxPlayersInitSend at most); the entity to go on with */
cEntity *NetData_WriteUsrDescriptors(cAbstractFile *f, cEntity *e);
byte Net_GetNetIndex(const char *name, int *n);
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
int NetServer_GetIndexOfSlot(int slot);
int NetServer_NextToSend(uint32_t time);
int NetServer_Init(int maxClients, uint16_t port, int arg);
void NetServer_Goodbye(void);
int NetServer_DropClient(int slot);
int NetServer_Flush(uint32_t time);
byte Net_ModifyUsrData(int arg);
/* the slot of the packet read (-1 none, -2 refused) */
int NetServer_Receive(uint32_t time, void *data, int *dataSize, void *ctl, int *ctlSize);

/* transport.cpp (src/game/Net/Transport.c) */
int Transport_PingServer(const NetAddr *addr);
int Transport_CloseBrowser(void);
void Transport_CloseClient(void);
void Transport_Close(void);
int Transport_RemoveClient(int slot);
/* the number of connected clients (the name comes from its callers) */
int Net_GetSendRate(void);
int Transport_PingFreeSlot(void);
int Transport_Recv(byte *buf, int *len);
int Transport_FindClient(const NetAddr *addr);
int Transport_GetMaxClients(void);
int Transport_GetMode(void);
int Transport_PingTime(const NetAddr *addr);
uint16_t Transport_GetLocalPort(void);
int Transport_SetMaster(const char *address, uint16_t port);
int Transport_InitBrowser(const char *address, uint16_t port);
int Transport_InitClient(const char *address, uint16_t port);
int Transport_InitServer(int maxClients, uint16_t port);
int Transport_Startup(void);
int Net_PingInetSvrs(void);
int Transport_MasterPacket(const byte *buf, int len);
int Transport_SendNeedPassword(int slot);
/* the host name of address (a name or a dotted IP), its IP as text into ip, as number into addr; NULL if unknown */
const char *Transport_Resolve(const char *address, char *ip, uint32_t *addr);
int Transport_PingMaster(void);
int Transport_SendToClient(const void *data, int size, int slot);
void Transport_SendToServer(const void *data, int size);
int Net_SendMasterString(const char *text);
byte cGVar_SetString(cGVar *self, const char *s);
int Transport_Wait(int ms);
uint16_t Transport_Crc16(const byte *data, int size);
int Net_IsMaster(void);
/* recvfrom of an encrypted packet: the plain length, -1 if none or the tag is wrong */
int Transport_RecvFrom(uint32_t s, byte *buf, int len, int flags, NetAddr *from, int *fromlen);
int Transport_SendTo(uint32_t s, const void *data, int len, int flags, const NetAddr *to, int tolen);

/* the crypto of the packets (src/game/Net/Crypto.c) */
cNetBuffer *cNetBuffer_ctor(cNetBuffer *self, uint32_t cap);
void cNetBuffer_dtor(cNetBuffer *self);
byte *cNetBuffer_Data(cNetBuffer *self);
uint32_t cNetBuffer_Size(cNetBuffer *self);
int cNetBuffer_Append(cNetBuffer *self, const void *src, uint32_t n);
int cNetBuffer_AppendBuffer(cNetBuffer *self, const cNetBuffer *src);
/* the bytes and zeros up to the next multiple of 16 of n */
int cNetBuffer_AppendPad16(cNetBuffer *self, const void *src, uint32_t n);
int cNetBuffer_AppendBufferPad16(cNetBuffer *self, const cNetBuffer *src);
int cNetBuffer_WriteU16(cNetBuffer *self, uint16_t v);
int cNetBuffer_WriteU32(cNetBuffer *self, uint32_t v);
int cNetBuffer_WriteU64(cNetBuffer *self, uint64_t v);
int cNetBuffer_WriteU8(cNetBuffer *self, byte v);
void cNetBuffer_Reserve(cNetBuffer *self, uint32_t cap);
cChaCha *cChaCha_ctor(cChaCha *self);
void cChaCha_Init(cChaCha *self, const byte *key, uint32_t counter, const byte *nonce);
void cChaCha_Crypt(cChaCha *self, const byte *key, uint32_t counter, const byte *nonce, const byte *in,
	byte *out, uint32_t len);
/* one block of rounds (20) of the state into out (64 bytes); self is not used */
void cChaCha_Block(cChaCha *self, const uint32_t *state, byte *out, int rounds);
void cChaCha_QuarterRound(cChaCha *self, uint32_t *x, int a, int b, int c, int d);
uint32_t cChaCha_Rotl(cChaCha *self, uint32_t v, int n);
void cChaCha_Store(cChaCha *self, const uint32_t *x, byte *out);
void cChaCha_StoreU32(cChaCha *self, uint32_t v, byte *out);
uint32_t cChaCha_LoadU32(cChaCha *self, const byte *p);
/* Poly1305 (poly1305-donna) of size bytes of msg with key (32 bytes) into mac (16 bytes) */
void Poly1305_Auth(byte *mac, const byte *msg, uint32_t size, const byte *key);

#endif
