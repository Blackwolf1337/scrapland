/*
 * Net/GameServer.cpp of the game: the server (start, close, change of level, kick), its frame (heartbeat to the
 * master, the receive loop, the sends to the clients in turn, the clients that timed out), the hex dumps of the net
 * debug output. The answers to the All-Seeing Eye queries of the original are gone (its master servers are down).
 * See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/net.h"
#include "scrap/python.h"
#include "scrap/vehicleentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/staticinit.h"

#define g_NetBrowserCode (GVar_BrowserCode.value.s)	/* BrowserCode (cString) */
#define g_HeartbeatTime (GVar_HeartbeatTime.value.f)
#define g_ServerMinSendDelay (GVar_ServerMinSendDelay.value.i)
static cGVar GVar_ServerMinSendDelay;
static cGVar GVar_HeartbeatTime;
static cGVar GVar_BrowserCode;

/* the script callbacks of the server gvars back to none (unless scripts may not write them) */
static void Net_ServerResetFuncs(void) {
	if (!(g_OnClientJoinFlags & 1)) {
		cPy_Function_SetFunc(g_OnClientJoin, "");
	}
	if (!(g_OnClientExitFlags & 1)) {
		cPy_Function_SetFunc(g_OnClientExit, "");
	}
	if (!(g_OnUsrStringFlags & 1)) {
		cPy_Function_SetFunc(g_OnUsrString, "");
	}
	if (!(g_OnChatStringFlags & 1)) {
		cPy_Function_SetFunc(g_OnChatString, "");
	}
	if (!(g_OnNetBrowseFlags & 1)) {
		cPy_Function_SetFunc(g_OnNetBrowseCallback, "");
	}
}

/* GameServer */
static char g_NetServerHost[256];
static char g_NetServerIP[256];
static uint16_t g_NetServerPort;
static void *g_NetServerData;
static void *g_NetServerCtl;

byte Net_CloseServer(const char *msg) {
	byte on = g_NetServerPort != 0;

	if (!on) {
		return on;
	}
	Net_Done();
	Con_Print(0, "Closing server...\n", 0);
	NetServer_Goodbye();
	/* the goodbyes go out until every client answers or times out */
	while (NetServer_Flush(Net_TimeMs())) {
	}
	NetServer_Done();
	g_NetServerHost[0] = 0;
	g_NetServerIP[0] = 0;
	g_NetServerPort = 0;
	Mem_Free(g_NetServerData);
	Mem_Free(g_NetServerCtl);
	CWinApp_LoadLevel(g_pApp, msg);
	MsgMan_SetUserFactory(NULL);
	return on;
}

/* g_NetUserFactory of the server */
static cUserNetManager *Net_NewClientManager(cNetClient *client, int slot) {
	cClientNetManager *m = operator_new(sizeof(cClientNetManager));

	if (!m) {
		return NULL;
	}
	return &cClientNetManager_ctor(m, client, slot)->base;
}

const char *Net_GetServerName(void) {
	return g_NetServerHost;
}

const char *Net_GetServerMap(void) {
	return g_NetServerIP;
}

uint16_t Net_GetServerPort(void) {
	return g_NetServerPort;
}

static float g_NetHeartbeat;

/* a server on port b (0: the default) for a clients with the level name; c: the next free port */
byte Net_InitServer(const char *name, int a, int b, int c) {
	if (g_NetServerPort) {
		Con_Printf("Server already running at '%s' (%s:%i)\n", g_NetServerHost, g_NetServerIP, g_NetServerPort);
		return 0;
	}
	if (Net_IsClient()) {
		Con_Printf("Impossible to init server: client mode initialized\n");
		return 0;
	}
	const char *host = Transport_Resolve("localhost", NULL, NULL);
	if (!host) {
		return 0;
	}
	Str_Copy(g_NetServerHost, host, 0x100);
	if (g_ServerMinSendDelay < 1) {
		g_ServerMinSendDelay = 1;
	}
	MsgMan_SetSendDelay((uint32_t)(g_ServerMinSendDelay * 1000));
	Transport_Resolve(g_NetServerHost, g_NetServerIP, NULL);
	Con_Printf("Starting server at  '%s' (%s)...\n", g_NetServerHost, g_NetServerIP);
	g_NetServerPort = (uint16_t)b ? (uint16_t)b : NET_DEFAULT_PORT;
	Net_ServerResetFuncs();
	int ok = NetServer_Init(a, g_NetServerPort, 8);
	if ((byte)c) {
		for (int k = 1000; !ok;) {
			if (k-- <= 0) {
				break;
			}
			g_NetServerPort++;
			ok = NetServer_Init(a, g_NetServerPort, 8);
		}
	}
	if (!ok) {
		Con_Printf("Server Initialization error.\n");
		g_NetServerHost[0] = 0;
		g_NetServerIP[0] = 0;
		g_NetServerPort = 0;
		return 0;
	}
	Con_Printf("port %d binded!\n", g_NetServerPort);
	CWinApp_LoadLevel(g_pApp, name);
	g_NetServerData = Mem_Alloc(24000);
	g_NetServerCtl = Mem_Alloc(24000);
	MsgMan_SetUserFactory((netuserfactory_t)Net_NewClientManager);
	Net_Init();
	if (App_IsDedicatedWin(g_pApp)) {
		Transport_SetMaster(g_MasterServerAddress, (uint16_t)g_MasterServerPort);
	}
	g_NetHeartbeat = 0.0f;
	SaveVars_Delete("");
	return 1;
}

byte Net_IsServer(void) {
	return g_NetServerPort != 0;
}

byte Net_Kick(const char *player) {
	if (Net_IsPlayerName(player)) {
		cNetClient *c = NetServer_GetClient(Net_GetPlayerIndex(player));
		if (c) {
			cNetClient_Goodbye(c);
			return 1;
		}
	}
	return 0;
}

byte Net_ServerChangeLevel(const char *level) {
	if (!g_NetServerPort) {
		Con_Print(0, "Server not initialized.", 0);
		return 0;
	}
	Net_ServerResetFuncs();
	CWinApp_LoadLevel(g_pApp, level);
	Net_Init();
	byte msg = NETMSG_PING;
	NetServer_SendAll(&msg, 1);
	return 1;
}

/* the server frame up to the wait of the dedicated server: the heartbeat to the master and the ASE queries */
void Net_ServerFrameHead(void) {
	char s[0x80];

	CHECK(g_NetServerPort != 0);
	if (Transport_GetMaxClients() > Net_GetSendRate() && g_NetBrowserCode && *g_NetBrowserCode) {
		double next = (double)(g_HeartbeatTime + g_NetHeartbeat);
		if (App_GetTime(g_pApp) > next) {
			g_NetHeartbeat = (float)App_GetTime(g_pApp);
			Str_PrintfQuiet(s, sizeof(s), "HB=%d,%d:%d|%s", NET_GAME_ID, NET_GAME_VERSION, g_NetServerPort,
				g_NetBrowserCode);
			Net_SendMasterString(s);
		}
	}
}

static int g_NetServerDataSize;
static int g_NetServerCtlSize;

/* the rest after the wait: receive, send to the clients in turn, drop the ones that timed out */
byte Net_ServerFrameTail(void) {
	int r;

	do {
		g_NetServerDataSize = 24000;
		g_NetServerCtlSize = 24000;
		r = NetServer_Receive(Net_TimeMs(), g_NetServerData, &g_NetServerDataSize, g_NetServerCtl,
			&g_NetServerCtlSize);
	} while (r == -2);
	/* the clients in the order of their last send, each once at most */
	int slot = NetServer_NextToSend(Net_TimeMs());
	for (int n = Net_GetSendRate(); slot >= 0 && n >= 0; n--) {
		cNetClient *c = NetServer_GetClient(slot);
		if (!c) {
			break;
		}
		cUserNetManager *u = c->user;
		if (u) {
			NetMan_Send(u, Net_TimeMs());
		}
		slot = NetServer_NextToSend(Net_TimeMs());
	}
	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		cNetClient *c = NetServer_GetClient(i);
		if (c && c->state == NETCLIENT_TIMEOUT) {
			NetServer_DropClient(i);
		}
	}
	return r != -1;
}

/* differs from the original on purpose (no blocking waits): the dedicated server's wait for a packet (Transport_Wait)
 * is not here but in CApp_Frame, which finishes the frame later without blocking; this is the other servers' frame */
byte Net_ServerFrame(void) {
	Net_ServerFrameHead();
	return Net_ServerFrameTail();
}

/* a hex and text dump of data into the console, 16 bytes a line */
static void Net_HexDump(const void *data, int size) {
	const byte *d = data;
	char line[0x47];
	char *ascii = line + 0x36;
	int pos = 0;

	memset(line, ' ', sizeof(line));
	line[0x46] = 0;
	for (int i = 0; i < size; i++) {
		byte c = d[i];
		int col = i % 16;
		if (!col) {
			if (i > 0) {
				Con_Printf("%s\n", line);
				memset(line, ' ', sizeof(line));
				line[0x46] = 0;
				pos = 0;
			}
			crt_sprintf(line + pos, "%03d ", i);
			line[pos + 4] = ' ';
			pos += 4;
		}
		crt_sprintf(line + pos, " %02x", c);
		pos += 3;
		line[pos] = ' ';
		if ((byte)(c - 'a') <= 25 || (byte)(c - 'A') <= 25 || (byte)(c - '0') <= 9 || c == ' ' || c == '_' ||
			c == '\'' || c == '@') {
			ascii[i & 0xf] = (char)c;
		} else {
			ascii[i & 0xf] = '.';
		}
		if (i == size - 1) {
			for (int k = 15 - col; k > 0; k--) {
				Con_Printf("   ");
				crt_sprintf(line + pos, "   ");
				pos += 3;
				line[pos] = ' ';
			}
			Con_Printf("%s\n", line);
		}
	}
}

void Net_DumpData(const char *title, const void *data, int size) {
	Con_Printf("%s\n", title);
	Net_HexDump(data, size);
	Con_Printf("\n");
}

static cGVar GVar_BrowserCode;
cGVarFunction GVar_OnClientJoin;
static cGVar GVar_HeartbeatTime;
static cGVar GVar_ServerMinSendDelay;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_BrowserCode, &GVar_BrowserCode)
INIT_GVAR_STRING(Init_GVar_BrowserCode, Fini_GVar_BrowserCode, &GVar_BrowserCode, "BrowserCode", 4, "", "")
FINI_CALL(Fini_GVar_OnClientJoin, &GVar_OnClientJoin.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnClientJoin, Fini_GVar_OnClientJoin, &GVar_OnClientJoin, "OnClientJoin", 0, "OnClientJoin(entityname,ModelName,MaxLife,UsrName,PilotName,Motor0,Motor1,Motor2,Motor3,WeaponBay,tgtype) player ship created", "")
INIT_GVAR_INT(Init_GVar_ServerMinSendDelay, &GVar_ServerMinSendDelay, "ServerMinSendDelay", 6, "Mininimal delay between packets (in seconds).", 5)
INIT_GVAR_FLOAT(Init_GVar_HeartbeatTime, &GVar_HeartbeatTime, "HeartbeatTime", 6, "Heartbeat to the master server.\n", 30.0f)
