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
	cUserNetManager *(__thiscall *Delete)(cUserNetManager *self, byte flags);			/* 0x00 */
	/* the unreliable part and the reliable message of a packet */
	void(__thiscall *OnReceive)(cUserNetManager *self, void *data, int size, void *ctl, int ctlSize);	/* 0x04 */
	void(__thiscall *Send)(cUserNetManager *self, uint32_t time);				/* 0x08 */
	void(__thiscall *SetLocalData)(cUserNetManager *self);					/* 0x0c */
	void(__thiscall *SendHello)(cUserNetManager *self, byte type);				/* 0x10 cServerNetManager */
} cUserNetManagerVtbl;
SD_OFFSET(cUserNetManagerVtbl, SendHello, 0x10);

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

#define g_UserNetManagerVftable ((const cUserNetManagerVtbl *)GAME_VFTABLE(cUserNetManager_vftable, 0x3c6100))
#define g_ServerNetManagerVftable ((const cUserNetManagerVtbl *)GAME_VFTABLE(cServerNetManager_vftable, 0x3c5e20))
#define g_ClientNetManagerVftable ((const cUserNetManagerVtbl *)GAME_VFTABLE(cClientNetManager_vftable, 0x3c5f58))

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
	return Game_DtoUI(X87_Mul(g_pApp->vftable->GetTime(g_pApp), 1000.0));
}

/* exe functions of other modules */
void __thiscall cWithLifeEntity_SetUsrName(cEntity *e, const char *name);	/* rva 0x12c7c0, "" clears */

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

/* a sockaddr_in of Winsock (16 bytes) */
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
#define g_NetClientServer (GAME_ARRAY(char, g_NetClientServer, 0x462590))	/* [0x100] the server address, "" when not connected */
#define g_NetClientPort GAME_GLOBAL(uint16_t, g_NetClientPort, 0x462690)
#define g_NetClient GAME_GLOBAL(cNetClient *, g_NetClient, 0x462694)
#define g_NetClientData GAME_GLOBAL(void *, g_NetClientData, 0x462698)	/* the receive buffers, 24000 bytes */
#define g_NetClientDataSize GAME_GLOBAL(int, g_NetClientDataSize, 0x46269c)
#define g_NetClientCtl GAME_GLOBAL(void *, g_NetClientCtl, 0x4626a0)
#define g_NetClientCtlSize GAME_GLOBAL(int, g_NetClientCtlSize, 0x4626a4)
/* GameServer */
#define g_NetServerHost (GAME_ARRAY(char, g_NetServerHost, 0x4626a8))	/* [0x100] */
#define g_NetServerIP (GAME_ARRAY(char, g_NetServerIP, 0x4627a8))	/* [0x100] */
#define g_NetBrowserCode GAME_GLOBAL_IN(char *, GVar_BrowserCode, 16, 0x463a88)	/* BrowserCode (cString) */
#define g_NetServerPort GAME_GLOBAL(uint16_t, g_NetServerPort, 0x463a90)	/* 0 without a server */
#define g_NetServerData GAME_GLOBAL(void *, g_NetServerData, 0x463a94)
#define g_NetServerDataSize GAME_GLOBAL(int, g_NetServerDataSize, 0x463a98)
#define g_NetServerCtl GAME_GLOBAL(void *, g_NetServerCtl, 0x463a9c)
#define g_NetServerCtlSize GAME_GLOBAL(int, g_NetServerCtlSize, 0x463aa0)
#define g_NetHeartbeat GAME_GLOBAL(float, g_NetHeartbeat, 0x463aa4)	/* GetTime of the last heartbeat to the master */
#define g_LogNetData GAME_GLOBAL_IN(int, GVar_LogNetData, 16, 0x463ab8)		/* LogNetData */
#define g_NetClientPings (GAME_ARRAY(byte, g_NetClientPings, 0x463ac0))	/* [0x100] the pings the server sends, log2 * 24 */
/* the engine variables (gvars) of Net/: values and their flags byte (bit 0: scripts may not write) */
#define g_NetMaxResourcesSend GAME_GLOBAL_IN(int, GVar_NetMaxResourcesSend, 16, 0x463bd0)
#define g_NetTimeOut GAME_GLOBAL_IN(int, GVar_NetTimeOut, 16, 0x463be8)		/* NetTimeOut, < 0 never */
#define g_NetMaxPlayersInitSend GAME_GLOBAL_IN(int, GVar_NetMaxPlayersInitSend, 16, 0x463c4c)
#define g_ClientPPS GAME_GLOBAL_IN(int, GVar_ClientPPS, 16, 0x463cc4)
#define g_HeartbeatTime GAME_GLOBAL_IN(float, GVar_HeartbeatTime, 16, 0x463cdc)
#define g_ServerMinSendDelay GAME_GLOBAL_IN(int, GVar_ServerMinSendDelay, 16, 0x463cf4)
#define g_ClientConnectionStatus (GAME_ARRAY_IN(cString, GVar_ClientConectionStatus, 16, 0x463d0c))	/* ClientConectionStatus */
#define g_UseServerPassword GAME_GLOBAL_IN(int, GVar_UseServerPassword, 16, 0x463d24)
#define g_MasterServerAddress GAME_GLOBAL_IN(char *, g_GVarMasterServerAddress, 16, 0x463cac)
#define g_MasterServerPortFlags GAME_GLOBAL_IN(byte, g_GVarMasterServerPort, 12, 0x463da4)
#define g_MasterServerPort GAME_GLOBAL_IN(int, g_GVarMasterServerPort, 16, 0x463da8)
#define g_ClientPassword GAME_GLOBAL_IN(char *, GVar_ClientPassword, 16, 0x463dc0)
#define g_ClientRemotePassword GAME_GLOBAL_IN(char *, GVar_ClientRemotePassword, 16, 0x463dfc)
#define g_ServerRemotePassword (GAME_ARRAY_IN(cString, GVar_ServerRemotePassword, 16, 0x463e38))
#define g_ForceServerVehicle GAME_GLOBAL_IN(int, GVar_ForceServerVehicle, 16, 0x463e98)
#define g_ServerPassword (GAME_ARRAY_IN(cString, GVar_ServerPassword, 16, 0x463ed4))
#define g_ServerName GAME_GLOBAL_IN(char *, GVar_ServerName, 16, 0x463eec)
#define g_GVarServerType (GAME_ARRAY(cGVar, g_GVarServerType, 0x467838))
#define g_ServerType GAME_GLOBAL_IN(char *, g_GVarServerType, 16, 0x467848)	/* ServerType (cString) */
#define g_LastMasterCommandFlags GAME_GLOBAL_IN(byte, GVar_LastMasterCommandSent, 12, 0x465568)
#define g_LastMasterCommand (GAME_ARRAY_IN(cString, GVar_LastMasterCommandSent, 16, 0x46556c))	/* LastMasterCommandSent */
#define g_ServerDataRateLimit GAME_GLOBAL_IN(int, GVar_ServerDataRateLimit, 16, 0x465538)
/* the script callbacks (cPy_Function in the gvar) and the flags of their gvars */
#define g_OnClientJoinFlags GAME_GLOBAL_IN(byte, GVar_OnClientJoin, 12, 0x463c24)
#define g_OnClientJoin (GAME_ARRAY_IN(cPy_Function, GVar_OnClientJoin, 16, 0x463c28))
#define g_ClientSetConfig (GAME_ARRAY_IN(cPy_Function, GVar_ClientSetConfig, 16, 0x463c64))
#define g_OnChatStringFlags GAME_GLOBAL_IN(byte, GVar_OnChatString, 12, 0x463c84)
#define g_OnChatString (GAME_ARRAY_IN(cPy_Function, GVar_OnChatString, 16, 0x463c88))
#define g_MasterCommandFunc (GAME_ARRAY_IN(cPy_Function, GVar_MasterCommandFunc, 16, 0x463d3c))
#define g_OnClientModify (GAME_ARRAY_IN(cPy_Function, GVar_OnClientModify, 16, 0x463d60))
#define g_ClientAddResFlags GAME_GLOBAL_IN(byte, GVar_ClientAddRes, 12, 0x463d80)
#define g_ClientAddRes (GAME_ARRAY_IN(cPy_Function, GVar_ClientAddRes, 16, 0x463d84))
#define g_OnUsrStringFlags GAME_GLOBAL_IN(byte, GVar_OnUsrString, 12, 0x463dd4)
#define g_OnUsrString (GAME_ARRAY_IN(cPy_Function, GVar_OnUsrString, 16, 0x463dd8))
#define g_OnNetBrowseFlags GAME_GLOBAL_IN(byte, GVar_OnNetBrowseCallback, 12, 0x463e10)
#define g_OnNetBrowseCallback (GAME_ARRAY_IN(cPy_Function, GVar_OnNetBrowseCallback, 16, 0x463e14))
#define g_OnClientExitFlags GAME_GLOBAL_IN(byte, GVar_OnClientExit, 12, 0x463e4c)
#define g_OnClientExit (GAME_ARRAY_IN(cPy_Function, GVar_OnClientExit, 16, 0x463e50))
#define g_ClientDelResFlags GAME_GLOBAL_IN(byte, GVar_ClientDelRes, 12, 0x463e70)
#define g_ClientDelRes (GAME_ARRAY_IN(cPy_Function, GVar_ClientDelRes, 16, 0x463e74))
#define g_ClientSetPlayerFlags GAME_GLOBAL_IN(byte, GVar_ClientSetPlayer, 12, 0x463eac)
#define g_ClientSetPlayer (GAME_ARRAY_IN(cPy_Function, GVar_ClientSetPlayer, 16, 0x463eb0))
#define g_ClientAfterCreateFlags GAME_GLOBAL_IN(byte, GVar_ClientAfterCreate, 12, 0x463f0c)
#define g_ClientAfterCreate (GAME_ARRAY_IN(cPy_Function, GVar_ClientAfterCreate, 16, 0x463f10))
#define g_ClientCreateFlags GAME_GLOBAL_IN(byte, GVar_ClientCreate, 12, 0x463f30)
#define g_ClientCreate (GAME_ARRAY_IN(cPy_Function, GVar_ClientCreate, 16, 0x463f34))
/* the player config the client sends in its hello (gvar values) */
#define g_PlayerNameStr GAME_GLOBAL_IN(char *, GVar_PlayerName, 16, 0x448148)
#define g_PlayerModel GAME_GLOBAL_IN(char *, GVar_PlayerModel, 16, 0x4481d8)
#define g_PlayerMaxLife GAME_GLOBAL_IN(int, GVar_PlayerMaxLife, 16, 0x447f68)
#define g_PilotModel GAME_GLOBAL_IN(char *, GVar_PilotModel, 16, 0x448010)
#define g_Motor0Model GAME_GLOBAL_IN(char *, GVar_Motor0Model, 16, 0x448070)
#define g_Motor1Model GAME_GLOBAL_IN(char *, GVar_Motor1Model, 16, 0x447fc8)
#define g_Motor2Model GAME_GLOBAL_IN(char *, GVar_Motor2Model, 16, 0x448178)
#define g_Motor3Model GAME_GLOBAL_IN(char *, GVar_Motor3Model, 16, 0x447e30)
#define g_WeaponBayList GAME_GLOBAL_IN(char *, GVar_WeaponBayList, 16, 0x447e18)
#define g_PlayerTeamID GAME_GLOBAL_IN(int, GVar_PlayerTeamID, 16, 0x448190)

/* msgman.cpp */
#define g_NetWaitTime GAME_GLOBAL(uint32_t, g_NetWaitTime, 0x428d28)	/* 5000: no answer that long, NETCLIENT_WAITING */
#define g_NetCloseTime GAME_GLOBAL(uint32_t, g_NetCloseTime, 0x428d2c)	/* 2000: after the goodbye */
#define g_NetSendDelay GAME_GLOBAL(uint32_t, g_NetSendDelay, 0x428d30)	/* 100 */
#define g_NetUserFactory GAME_GLOBAL(netuserfactory_t, g_NetUserFactory, 0x463bf0)
#define g_NetMsg (&GAME_GLOBAL(cFileMem, g_NetMsg, 0x463bf8))	/* the memory file messages are built and read in */
#define g_NetStaticBuffer (&GAME_GLOBAL_IN(cStaticBuffer, cStaticBuffer_PyStaticBuffer, 12, 0x44bbfc))
#define g_PyStaticBuffer (&GAME_GLOBAL_IN(cStaticBuffer, cStaticBuffer_PyStaticBuffer, 0, 0x44bbf0))
#define g_NetOrigin GAME_GLOBAL(vec3_t, g_NetOrigin, 0x463ef4)
#define g_NetMaxDist2 GAME_GLOBAL(float, g_NetMaxDist2, 0x3b5690)	/* the distance (squared) of the near entities */
/* NetData.cpp */
#define g_NetObjCounter GAME_GLOBAL(int, g_NetObjCounter, 0x463f48)
#define g_NetBotCounter GAME_GLOBAL(int, g_NetBotCounter, 0x463f4c)
#define g_NetResources (GAME_ARRAY(char *, g_NetResources, 0x463f50))	/* [NET_RESOURCES] Mem_Alloc names */
#define g_NetResInit GAME_GLOBAL(byte, g_NetResInit, 0x464350)
#define g_NetBrowserPort GAME_GLOBAL(int, g_NetBrowserPort, 0x464354)
#define NET_OFFLINE GAME_STR(str_3c6a10, 0x3c6a10)	/* "Offline" */
/* the server side client table */
#define g_NetClients GAME_GLOBAL(cNetClient **, g_NetClients, 0x465540)	/* [Transport_GetMaxClients()] */
#define g_NetNextSend GAME_GLOBAL(uint32_t, g_NetNextSend, 0x465544)
typedef void (*netmastercmd_t)(const char *cmd);
#define g_MasterCommandCallback GAME_GLOBAL(netmastercmd_t, g_MasterCommandCallback, 0x465548)
/* the counters for the stats (same names as include/scrap/scoreritems.h) */
#define g_NetStatSent GAME_GLOBAL(int, g_NetStatSent, 0x465558)
#define g_NetStatSentSlot GAME_GLOBAL(int, g_NetStatSentSlot, 0x465554)
#define g_NetStatRecv GAME_GLOBAL(int, g_NetStatRecv, 0x465550)
#define g_NetStatRecvSlot GAME_GLOBAL(int, g_NetStatRecvSlot, 0x46554c)
/* transport.cpp */
#define g_NetSocket GAME_GLOBAL(uint32_t, g_NetSocket, 0x465574)
#define g_NetRandom (&GAME_GLOBAL(cNetRandom, g_NetRandom, 0x465578))
#define g_NetLocalAddr (&GAME_GLOBAL(NetAddr, g_NetLocalAddr, 0x466748))
#define g_NetServerAddr (&GAME_GLOBAL(NetAddr, g_NetServerAddr, 0x466758))
#define g_NetMode GAME_GLOBAL(int, g_NetMode, 0x466768)		/* NETMODE_ */
#define g_NetMasterOn GAME_GLOBAL(int, g_NetMasterOn, 0x46676c)
#define g_NetMaxClients GAME_GLOBAL(int, g_NetMaxClients, 0x466770)
#define g_NetNumClients GAME_GLOBAL(int, g_NetNumClients, 0x466774)
#define g_NetClientAddrs GAME_GLOBAL(NetAddr *, g_NetClientAddrs, 0x466778)	/* [g_NetMaxClients], family 0: free */
#define g_NetHost GAME_GLOBAL(void *, g_NetHost, 0x46677c)		/* hostent of the last Transport_Resolve */
#define g_NetPingServers GAME_GLOBAL(NetPingServer *, g_NetPingServers, 0x466780)
#define g_NetBrowseStart GAME_GLOBAL(uint32_t, g_NetBrowseStart, 0x466784)
#define g_NetMasterAddr (&GAME_GLOBAL(NetAddr, g_NetMasterAddr, 0x466788))
#define g_NetServerFullPkt (GAME_ARRAY(const byte, g_NetServerFullPkt, 0x428d48))	/* 5 bytes: 0, 0, 0, 0, 8 */
#define g_NetNeedPasswordPkt (GAME_ARRAY(const byte, g_NetNeedPasswordPkt, 0x428d50))	/* 5 bytes: 0, 0, 0, 0, 9 */

/* Winsock 1.1 of the exe: the jump thunks to wsock32 (src/game/Net/Wsock.c), stdcall */
int __stdcall Wsock_bind(uint32_t s, const void *addr, int len);
int __stdcall Wsock_closesocket(uint32_t s);
int __stdcall Wsock_ioctlsocket(uint32_t s, int32_t cmd, uint32_t *arg);
uint32_t __stdcall Wsock_htonl(uint32_t v);
uint16_t __stdcall Wsock_htons(uint16_t v);
uint16_t __stdcall Wsock_ntohs(uint16_t v);
int __stdcall Wsock_recvfrom(uint32_t s, void *buf, int len, int flags, void *from, int *fromlen);
int __stdcall Wsock_select(int n, void *readfds, void *writefds, void *exceptfds, const void *timeout);
int __stdcall Wsock_sendto(uint32_t s, const void *buf, int len, int flags, const void *to, int tolen);
int __stdcall Wsock_setsockopt(uint32_t s, int level, int name, const void *val, int len);
uint32_t __stdcall Wsock_socket(int af, int type, int protocol);
void *__stdcall Wsock_gethostbyaddr(const void *addr, int len, int type);
void *__stdcall Wsock_gethostbyname(const char *name);
int __stdcall Wsock_WSAStartup(uint16_t version, void *data);
int __stdcall Wsock_WSACleanup(void);

/* the All-Seeing Eye server (cdecl; src/lib/ase/Ase.c, hooks on the SDK linked into the exe at rva 0x002070) */
#define g_AseInit GAME_GLOBAL_IN(int, GVar_ASEPublic, 16, 0x462584)
#define g_AseError GAME_GLOBAL(const char *, g_AseError, 0x43d030)	/* the text of the last error */
#define g_Fraglimit GAME_GLOBAL_IN(int, GVar_Fraglimit, 16, 0x4676a4)
void Ase_Shutdown(void);
int Ase_Initialize(int port, int internet, const char *address);
void Ase_ServerStatus(const char *name, const char *type, const char *map, const char *version, int password,
	int numPlayers, int maxPlayers);
void Ase_AddRule(const char *key, const char *value);
void Ase_AddPlayer(const char *name, const char *team, const char *skin, const char *score, const char *ping,
	const char *time);
int Ase_Check(void);

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
void Net_AsePlayer(int n);
void Net_AseShutdown(void);
void Net_AseInit(int port);
void Net_AseCheck(void);
void Net_AsePlayers(void);
void Net_AseRules(void);
void Net_AseStatus(void);
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
cServerNetManager *__thiscall cServerNetManager_ctor(cServerNetManager *self, cNetClient *client, int slot);
void __fastcall cServerNetManager_dtor(cServerNetManager *self);
cServerNetManager *__thiscall cServerNetManager_Delete(cServerNetManager *self, byte flags);
void __thiscall cServerNetManager_OnReceive(cServerNetManager *self, void *data, int size, void *ctl, int ctlSize);
/* the ping of a client the server reported (2^(b/24)), 0 unknown */
int Net_GetClientPing(uint32_t slot);
void __thiscall cServerNetManager_SendHello(cServerNetManager *self, byte type);
void __thiscall cServerNetManager_Send(cServerNetManager *self, uint32_t time);
void __thiscall cUserNetManager_Nop(cUserNetManager *self);

/* ManServer.cpp (src/game/Net/ManServer.c) */
cClientNetManager *__thiscall cClientNetManager_ctor(cClientNetManager *self, cNetClient *client, int slot);
void __fastcall cClientNetManager_dtor(cClientNetManager *self);
cClientNetManager *__thiscall cClientNetManager_Delete(cClientNetManager *self, byte flags);
void __thiscall cClientNetManager_OnReceive(cClientNetManager *self, void *data, int size, void *ctl, int ctlSize);
/* creates the ship "net<slot>", sends the level and the resources */
void __fastcall cClientNetManager_InitClient(cClientNetManager *self);
/* the descriptor of player slot to every client (toAll) or to all but that one */
void ManServer_SendUsrData(int slot, byte toAll);
void __fastcall cClientNetManager_SendResources(cClientNetManager *self);
void __thiscall cClientNetManager_Send(cClientNetManager *self, uint32_t time);
/* the descriptors of all players to this client (unless local), then this one's to the others */
void __thiscall cClientNetManager_SendUsers(cClientNetManager *self, byte local);
void __fastcall cClientNetManager_SetLocalData(cClientNetManager *self);

/* msgman.cpp (src/game/Net/MsgMan.c) */
cNetClient *__thiscall cNetClient_ctor(cNetClient *self, int slot);
cUserNetManager *__thiscall cUserNetManager_ctor(cUserNetManager *self, cNetClient *client, int slot);
void __fastcall cNetClient_dtor(cNetClient *self);
void __fastcall cUserNetManager_dtor(cUserNetManager *self);
cUserNetManager *__thiscall cUserNetManager_Delete(cUserNetManager *self, byte flags);
/* a reliable message into the queue */
void __thiscall cNetClient_Push(cNetClient *self, const void *data, uint16_t size);
int __thiscall cNetClient_UpdateState(cNetClient *self, uint32_t time);
/* an empty packet when nothing went out for g_NetSendDelay; 0 once timed out */
int __thiscall cNetClient_Flush(cNetClient *self, uint32_t time, int slot);
void __fastcall cNetClient_RequestFull(cNetClient *self);
cNetMsgNode *__thiscall cNetClient_NewMsg(cNetClient *self, const void *data, uint16_t size);
void __thiscall cNetClient_GetSeq(cNetClient *self, int *head, int *in);
uint32_t __thiscall cNetClient_TimeToSend(cNetClient *self, uint32_t time);
void __fastcall cNetClient_Goodbye(cNetClient *self);
int __thiscall cNetClient_ClientRecv(cNetClient *self, uint32_t time, int *slot, void *data, int *dataSize, void *ctl,
	int *ctlSize);
/* the packet with size bytes of unreliable data; its length, 0 if not sent */
uint32_t __thiscall cNetClient_Send(cNetClient *self, uint32_t time, int slot, int size, const void *data);
int __thiscall cNetClient_Receive(cNetClient *self, uint32_t time, const byte *pkt, int len, void *data, int *dataSize,
	void *ctl, int *ctlSize);
void MsgMan_SetCloseTime(uint32_t ms);
void MsgMan_SetUserFactory(netuserfactory_t f);
void MsgMan_SetSendDelay(uint32_t ms);
void MsgMan_SetWaitTime(uint32_t ms);

/* the random numbers (src/game/Net/NetRandom.c) */
cNetRandom *__fastcall cNetRandom_ctor(cNetRandom *self);
void __thiscall cNetRandom_InitByArray(cNetRandom *self, const uint64_t *key, uint32_t len);
void __thiscall cNetRandom_Seed(cNetRandom *self, uint64_t seed);
uint64_t __fastcall cNetRandom_Next(cNetRandom *self);
void __fastcall cNetRandom_Fill(cNetRandom *self);
/* seeded from GetTickCount64 through ChaCha20 */
cNetRandom *__fastcall cNetRandom_ctorSeeded(cNetRandom *self);

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
int Transport_WSAStartup(void);
int Net_PingInetSvrs(void);
int Transport_MasterPacket(const byte *buf, int len);
int Transport_SendNeedPassword(int slot);
/* the host name of address (a name or a dotted IP), its IP as text into ip, as number into addr; NULL if unknown */
const char *Transport_Resolve(const char *address, char *ip, uint32_t *addr);
int Transport_PingMaster(void);
int Transport_SendToClient(const void *data, int size, int slot);
void Transport_SendToServer(const void *data, int size);
int Net_SendMasterString(const char *text);
byte __thiscall cGVar_SetString(cGVar *self, const char *s);
int Transport_Wait(int ms);
uint16_t Transport_Crc16(const byte *data, int size);
int Net_IsMaster(void);
int Net_PingInetSvrs(void);
/* recvfrom of an encrypted packet: the plain length, -1 if none or the tag is wrong */
int Transport_RecvFrom(uint32_t s, byte *buf, int len, int flags, NetAddr *from, int *fromlen);
int Transport_SendTo(uint32_t s, const void *data, int len, int flags, const NetAddr *to, int tolen);

/* the crypto of the packets (src/game/Net/Crypto.c) */
cNetBuffer *__thiscall cNetBuffer_ctor(cNetBuffer *self, uint32_t cap);
void __fastcall cNetBuffer_dtor(cNetBuffer *self);
byte *__fastcall cNetBuffer_Data(cNetBuffer *self);
uint32_t __fastcall cNetBuffer_Size(cNetBuffer *self);
int __thiscall cNetBuffer_Append(cNetBuffer *self, const void *src, uint32_t n);
int __thiscall cNetBuffer_AppendBuffer(cNetBuffer *self, const cNetBuffer *src);
/* the bytes and zeros up to the next multiple of 16 of n */
int __thiscall cNetBuffer_AppendPad16(cNetBuffer *self, const void *src, uint32_t n);
int __thiscall cNetBuffer_AppendBufferPad16(cNetBuffer *self, const cNetBuffer *src);
int __thiscall cNetBuffer_WriteU16(cNetBuffer *self, uint16_t v);
int __thiscall cNetBuffer_WriteU32(cNetBuffer *self, uint32_t v);
int __thiscall cNetBuffer_WriteU64(cNetBuffer *self, uint64_t v);
int __thiscall cNetBuffer_WriteU8(cNetBuffer *self, byte v);
void __thiscall cNetBuffer_Reserve(cNetBuffer *self, uint32_t cap);
cChaCha *__fastcall cChaCha_ctor(cChaCha *self);
void __thiscall cChaCha_Init(cChaCha *self, const byte *key, uint32_t counter, const byte *nonce);
void __thiscall cChaCha_Crypt(cChaCha *self, const byte *key, uint32_t counter, const byte *nonce, const byte *in,
	byte *out, uint32_t len);
/* one block of rounds (20) of the state into out (64 bytes); self is not used */
void __thiscall cChaCha_Block(cChaCha *self, const uint32_t *state, byte *out, int rounds);
void __thiscall cChaCha_QuarterRound(cChaCha *self, uint32_t *x, int a, int b, int c, int d);
uint32_t __thiscall cChaCha_Rotl(cChaCha *self, uint32_t v, int n);
void __thiscall cChaCha_Store(cChaCha *self, const uint32_t *x, byte *out);
void __thiscall cChaCha_StoreU32(cChaCha *self, uint32_t v, byte *out);
uint32_t __thiscall cChaCha_LoadU32(cChaCha *self, const byte *p);
/* Poly1305 (poly1305-donna) of size bytes of msg with key (32 bytes) into mac (16 bytes) */
void Poly1305_Auth(byte *mac, const byte *msg, uint32_t size, const byte *key);

#endif
